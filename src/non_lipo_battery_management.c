/*
 * Copyright (c) 2025 sekigon-gonnoc
 *
 * SPDX-License-Identifier: MIT
 */
/*
 * non_lipo_battery_management.c
 *
 * Hybrid driver:
 *  - Based on original zmk_non_lipo_battery driver structure (sensor API + DT)
 *  - Adds lightweight public API functions for other modules to query SOC/voltage
 *  - Preserves low-voltage auto-shutdown and optional advertising-timeout sleep behavior
 *
 * Compatible binding: zmk,non-lipo-battery
 */

#define DT_DRV_COMPAT zmk_non_lipo_battery

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/pm/pm.h>
#include <zephyr/sys/poweroff.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/kernel.h>

#include <zmk/pm.h>
#include <zmk/usb.h>

LOG_MODULE_REGISTER(zmk_battery_non_lipo, CONFIG_ZMK_LOG_LEVEL);

/* ---------------------------
 * Optional BLE advertising sleep timeout feature
 * --------------------------- */
#if IS_ENABLED(CONFIG_ZMK_NON_LIPO_ADV_SLEEP_TIMEOUT)
static bool is_advertising = true;  /* assume advertising initially */
static bool has_connection = false;
static int64_t advertising_start_time = 0;
static struct k_work_delayable adv_timeout_work;

static void adv_timeout_handler(struct k_work *work)
{
    if (is_advertising && !has_connection && !zmk_usb_is_powered()) {
        int64_t now = k_uptime_get();
        int64_t elapsed = now - advertising_start_time;

        if (elapsed >= CONFIG_ZMK_NON_LIPO_ADV_SLEEP_TIMEOUT) {
            LOG_WRN("Advertising timeout reached (%lldms). Entering suspend/poweroff.", elapsed);
            k_sleep(K_MSEC(100)); /* allow logs */
            zmk_pm_suspend_devices();
            sys_poweroff();
            return;
        } else {
            int64_t remaining = CONFIG_ZMK_NON_LIPO_ADV_SLEEP_TIMEOUT - elapsed;
            k_work_schedule(&adv_timeout_work, K_MSEC(MIN(remaining, 10000)));
        }
    }
}

static void connected_cb(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        LOG_ERR("BT connection failed: %u", err);
        return;
    }
    has_connection = true;
    is_advertising = false;
    k_work_cancel_delayable(&adv_timeout_work);
}

static void disconnected_cb(struct bt_conn *conn, uint8_t reason)
{
    ARG_UNUSED(reason);
    has_connection = false;
    is_advertising = true;
    advertising_start_time = k_uptime_get();
    k_work_schedule(&adv_timeout_work, K_MSEC(10000));
}

static struct bt_conn_cb conn_callbacks = {
    .connected = connected_cb,
    .disconnected = disconnected_cb,
};
#endif /* CONFIG_ZMK_NON_LIPO_ADV_SLEEP_TIMEOUT */

/* ---------------------------
 * Driver config/data structures
 * --------------------------- */
struct io_channel_config {
    uint8_t channel;
};

struct non_lipo_config {
    struct io_channel_config io_channel;
#if DT_INST_NODE_HAS_PROP(0, power_gpios)
    struct gpio_dt_spec power;
#endif
};

struct non_lipo_data {
    const struct device *adc;
    struct adc_channel_cfg acc;
    struct adc_sequence as;
    int16_t adc_raw;
    uint16_t millivolts;
    uint8_t state_of_charge;
    struct k_mutex lock; /* protect read/update of state */
};

/* ---------------------------
 * Helper: convert mV -> percent (linear)
 * --------------------------- */
static uint8_t non_lipo_mv_to_pct(int16_t mv)
{
    if (mv >= CONFIG_ZMK_NON_LIPO_MAX_MV) {
        return 100;
    } else if (mv <= CONFIG_ZMK_NON_LIPO_MIN_MV) {
        return 0;
    }

    return (uint8_t)((100LL * (mv - CONFIG_ZMK_NON_LIPO_MIN_MV)) /
                     (CONFIG_ZMK_NON_LIPO_MAX_MV - CONFIG_ZMK_NON_LIPO_MIN_MV));
}

/* ---------------------------
 * Shutdown check (invoked after measurement)
 * --------------------------- */
static void check_voltage_and_shutdown(uint16_t millivolts)
{
    if (millivolts <= CONFIG_ZMK_NON_LIPO_LOW_MV) {
        if (!zmk_usb_is_powered()) {
            LOG_WRN("Battery voltage %dmV <= critical %dmV: powering off", millivolts, CONFIG_ZMK_NON_LIPO_LOW_MV);
            k_sleep(K_MSEC(100)); /* allow logs to flush */
            zmk_pm_suspend_devices();
            sys_poweroff();
        } else {
            LOG_WRN("Battery %dmV <= critical %dmV but USB power detected: staying on", millivolts, CONFIG_ZMK_NON_LIPO_LOW_MV);
        }
    }
}

/* ---------------------------
 * Sensor API: sample_fetch
 * --------------------------- */
static int non_lipo_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
    struct non_lipo_data *drv_data = dev->data;
    struct adc_sequence *as = &drv_data->as;

    if (chan != SENSOR_CHAN_GAUGE_VOLTAGE && chan != SENSOR_CHAN_GAUGE_STATE_OF_CHARGE &&
        chan != SENSOR_CHAN_ALL) {
        return -ENOTSUP;
    }

    int rc = 0;

#if DT_INST_NODE_HAS_PROP(0, power_gpios)
    const struct non_lipo_config *drv_cfg = dev->config;
    rc = gpio_pin_set_dt(&drv_cfg->power, 1);
    if (rc != 0) {
        LOG_DBG("Failed to enable ADC power GPIO: %d", rc);
        return rc;
    }
    k_sleep(K_MSEC(10)); /* allow divider to stabilize */
#endif

    rc = adc_read(drv_data->adc, as);
    /* We set calibrate via sequence; keep semantics */
    as->calibrate = false;

    if (rc == 0) {
        int32_t val = drv_data->adc_raw;
        adc_raw_to_millivolts(adc_ref_internal(drv_data->adc), drv_data->acc.gain, as->resolution, &val);
        uint16_t millivolts = (uint16_t)val;

        k_mutex_lock(&drv_data->lock, K_FOREVER);
        drv_data->millivolts = millivolts;
        drv_data->state_of_charge = non_lipo_mv_to_pct(millivolts);
        k_mutex_unlock(&drv_data->lock);

        LOG_DBG("ADC raw=%d -> %d mV, SOC=%u%%", drv_data->adc_raw, millivolts, drv_data->state_of_charge);

        /* check low-voltage -> may shutdown (if configured) */
        check_voltage_and_shutdown(millivolts);
    } else {
        LOG_DBG("adc_read failed: %d", rc);
    }

#if DT_INST_NODE_HAS_PROP(0, power_gpios)
    int rc2 = gpio_pin_set_dt(&drv_cfg->power, 0);
    if (rc2 != 0) {
        LOG_DBG("Failed to disable ADC power GPIO: %d", rc2);
        return rc2;
    }
#endif

    return rc;
}

/* ---------------------------
 * Sensor API: channel_get
 * --------------------------- */
static int non_lipo_channel_get(const struct device *dev, enum sensor_channel chan, struct sensor_value *val)
{
    struct non_lipo_data *drv_data = dev->data;

    k_mutex_lock(&drv_data->lock, K_FOREVER);
    uint16_t mv = drv_data->millivolts;
    uint8_t soc = drv_data->state_of_charge;
    k_mutex_unlock(&drv_data->lock);

    switch (chan) {
    case SENSOR_CHAN_GAUGE_VOLTAGE:
        val->val1 = mv / 1000;
        val->val2 = (mv % 1000) * 1000;
        break;

    case SENSOR_CHAN_GAUGE_STATE_OF_CHARGE:
        val->val1 = soc;
        val->val2 = 0;
        break;

    default:
        return -ENOTSUP;
    }

    return 0;
}

static const struct sensor_driver_api non_lipo_api = {
    .sample_fetch = non_lipo_sample_fetch,
    .channel_get = non_lipo_channel_get,
};

/* ---------------------------
 * Public helper APIs for other modules
 * --------------------------- */

/* Return SOC for instance index; negative error codes on failure */
int non_lipo_battery_get_soc_by_index(int inst_idx)
{
    if (!DT_HAS_NODE(DT_DRV_INST(inst_idx))) {
        return -ENODEV;
    }

    const struct device *dev = DEVICE_DT_INST_GET(inst_idx);
    if (!device_is_ready(dev)) {
        return -ENODEV;
    }

    struct non_lipo_data *drv_data = dev->data;
    k_mutex_lock(&drv_data->lock, K_FOREVER);
    int soc = drv_data->state_of_charge;
    k_mutex_unlock(&drv_data->lock);
    return soc;
}

int non_lipo_battery_get_voltage_mv_by_index(int inst_idx)
{
    if (!DT_HAS_NODE(DT_DRV_INST(inst_idx))) {
        return -ENODEV;
    }

    const struct device *dev = DEVICE_DT_INST_GET(inst_idx);
    if (!device_is_ready(dev)) {
        return -ENODEV;
    }

    struct non_lipo_data *drv_data = dev->data;
    k_mutex_lock(&drv_data->lock, K_FOREVER);
    int mv = drv_data->millivolts;
    k_mutex_unlock(&drv_data->lock);
    return mv;
}

/* Backwards-compatible wrappers (instance 0) */
int non_lipo_battery_get_soc(void)
{
    return non_lipo_battery_get_soc_by_index(0);
}

int non_lipo_battery_get_voltage_mv(void)
{
    return non_lipo_battery_get_voltage_mv_by_index(0);
}

/* ---------------------------
 * Initialization
 * --------------------------- */
static int non_lipo_init(const struct device *dev)
{
    struct non_lipo_data *drv_data = dev->data;
    const struct non_lipo_config *drv_cfg = dev->config;

    if (!device_is_ready(drv_data->adc)) {
        LOG_ERR("ADC device not ready");
        return -ENODEV;
    }

    int rc = 0;

#if DT_INST_NODE_HAS_PROP(0, power_gpios)
    if (!device_is_ready(drv_cfg->power.port)) {
        LOG_ERR("Power GPIO port not ready");
        return -ENODEV;
    }
    rc = gpio_pin_configure_dt(&drv_cfg->power, GPIO_OUTPUT_INACTIVE);
    if (rc != 0) {
        LOG_ERR("Failed to configure power pin %u: %d", drv_cfg->power.pin, rc);
        return rc;
    }
#endif

    k_mutex_init(&drv_data->lock);

    drv_data->as = (struct adc_sequence){
        .channels = BIT(0),
        .buffer = &drv_data->adc_raw,
        .buffer_size = sizeof(drv_data->adc_raw),
        .oversampling = 4,
        .calibrate = true,
    };

#ifdef CONFIG_ADC_NRFX_SAADC
    drv_data->acc = (struct adc_channel_cfg){
        .gain = ADC_GAIN_1_6,
        .reference = ADC_REF_INTERNAL,
        .acquisition_time = ADC_ACQ_TIME(ADC_ACQ_TIME_MICROSECONDS, 40),
        .input_positive = SAADC_CH_PSELP_PSELP_AnalogInput0 + drv_cfg->io_channel.channel,
    };
    drv_data->as.resolution = 12;
#else
#error "Unsupported ADC backend for non-lipo battery driver"
#endif

    rc = adc_channel_setup(drv_data->adc, &drv_data->acc);
    LOG_DBG("ADC AIN%u setup returned %d", drv_cfg->io_channel.channel, rc);

#if IS_ENABLED(CONFIG_ZMK_NON_LIPO_ADV_SLEEP_TIMEOUT)
    k_work_init_delayable(&adv_timeout_work, adv_timeout_handler);
    bt_conn_cb_register(&conn_callbacks);
    advertising_start_time = k_uptime_get();
    k_work_schedule(&adv_timeout_work, K_MSEC(10000));
    LOG_INF("Non-LiPo advertising sleep timeout initialized (%d ms)", CONFIG_ZMK_NON_LIPO_ADV_SLEEP_TIMEOUT);
#endif

    /* Try an initial sample (best-effort) */
    (void)non_lipo_sample_fetch(dev, SENSOR_CHAN_ALL);

    return rc;
}

/* ---------------------------
 * Device instance / config generation for all DT_INST(..., status = "okay")
 * --------------------------- */

#define NON_LIPO_INIT(inst)                                                              \
    static struct non_lipo_data non_lipo_data_##inst = {                                 \
        .adc = DEVICE_DT_GET(DT_IO_CHANNELS_CTLR(DT_DRV_INST(inst))),                    \
        .adc_raw = 0,                                                                    \
        .millivolts = 0,                                                                 \
        .state_of_charge = 100,                                                          \
    };                                                                                   \
    static const struct non_lipo_config non_lipo_cfg_##inst = {                          \
        .io_channel = {                                                                  \
            DT_IO_CHANNELS_INPUT(DT_DRV_INST(inst)),                                     \
        },                                                                               \
        IF_ENABLED(DT_INST_NODE_HAS_PROP(inst, power_gpios),                             \
            (.power = GPIO_DT_SPEC_INST_GET(inst, power_gpios),))                        \
    };                                                                                   \
    DEVICE_DT_INST_DEFINE(inst,                                                           \
                          &non_lipo_init,                                                 \
                          NULL,                                                           \
                          &non_lipo_data_##inst,                                          \
                          &non_lipo_cfg_##inst,                                           \
                          POST_KERNEL,                                                    \
                          CONFIG_SENSOR_INIT_PRIORITY,                                    \
                          &non_lipo_api);

DT_INST_FOREACH_STATUS_OKAY(NON_LIPO_INIT);

/* End of file */
