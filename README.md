# ZMK Non-LiPo Battery Management Module

This module provides battery monitoring and management capabilities for non-LiPo batteries (such as alkaline or NiMH) in ZMK-powered keyboards.

## Features

- ADC-based battery voltage monitoring
- Linear voltage-to-percentage conversion based on configurable min/max voltage
- Battery state reporting to ZMK and BLE battery service
- Low voltage protection (shuts down device when voltage falls below threshold)
- Optional GPIO control for power to voltage measurement circuit

## Usage

### 1. Add to your ZMK config west manifest

```yaml
manifest:
  remotes:
    - name: sekigon-gonnoc
      url-base: https://github.com/sekigon-gonnoc
  projects:
    - name: zmk-feature-non-lipo-battery-management
      remote: sekigon-gonnoc
```

### 2. Configure in your devicetree (.overlay file)

```dts
/ {
    chosen {
        zmk,battery = &non_lipo_battery;
    };

    non_lipo_battery: non_lipo_battery {
        compatible = "zmk,non-lipo-battery";
        io-channels = <&adc 2>;  // ADC channel to use
        power-gpios = <&gpio0 5 GPIO_ACTIVE_HIGH>; // Optional
        status = "okay";
    };
};
```

### 3. Enable in your Kconfig configuration

```
CONFIG_ZMK_NON_LIPO_BATTERY_MANAGEMENT=y

# optional
CONFIG_ZMK_NON_LIPO_MIN_MV=1100
CONFIG_ZMK_NON_LIPO_MAX_MV=1300
CONFIG_ZMK_NON_LIPO_LOW_MV=1050
```

## Configuration Options

| Option | Description | Default |
|--------|-------------|---------|
| `CONFIG_ZMK_NON_LIPO_BATTERY_MANAGEMENT` | Enable the feature | n/a |
| `CONFIG_ZMK_NON_LIPO_MIN_MV` | Minimum voltage in millivolts (corresponds to 0% charge) | 1100 |
| `CONFIG_ZMK_NON_LIPO_MAX_MV` | Maximum voltage in millivolts (corresponds to 100% charge) | 1300 |
| `CONFIG_ZMK_NON_LIPO_LOW_MV` | Shutdown threshold voltage in millivolts | 1050 |

## How It Works

The module:
1. Measures battery voltage using the specified ADC channel
2. Converts voltage to percentage based on min/max settings
3. Reports battery status to ZMK battery monitoring system
4. Monitors for low voltage condition and triggers system shutdown if detected

For alkaline batteries, typical voltages are:
- 1.5V per cell when new (100%)
- 1.0V per cell when depleted (0%)
- System shutdown around 1.0-1.1V per cell to prevent battery damage

---

# ZMK 非LiPoバッテリー管理モジュール

このモジュールは、ZMKを搭載したキーボードで非LiPoバッテリー（アルカリ電池やニッケル水素電池など）のモニタリングと管理機能を提供します。

## 機能

- ADCを使用したバッテリー電圧モニタリング
- 設定可能な最小/最大電圧に基づく直線的な電圧-パーセント変換
- ZMKとBLEバッテリーサービスへのバッテリー状態の報告
- 低電圧保護（電圧が閾値を下回るとデバイスをシャットダウン）
- 電圧測定回路への電源供給のためのオプションのGPIO制御

## 使用方法

### 1. ZMK configのwest manifestに追加

```yaml
manifest:
  remotes:
    - name: sekigon-gonnoc
      url-base: https://github.com/sekigon-gonnoc
  projects:
    - name: zmk-feature-non-lipo-battery-management
      remote: sekigon-gonnoc
```

### 2. デバイスツリーで設定 (.overlayファイル)

```dts
/ {
    chosen {
        zmk,battery = &non_lipo_battery;
    };

    non_lipo_battery: non_lipo_battery {
        compatible = "zmk,non-lipo-battery";
        io-channels = <&adc 2>;  // 使用するADCチャンネル
        power-gpios = <&gpio0 5 GPIO_ACTIVE_HIGH>; // オプション
        status = "okay";
    };
};
```

### 3. Kconfigで有効化

```
CONFIG_ZMK_NON_LIPO_BATTERY_MANAGEMENT=y

# オプション設定
CONFIG_ZMK_NON_LIPO_MIN_MV=1100
CONFIG_ZMK_NON_LIPO_MAX_MV=1300
CONFIG_ZMK_NON_LIPO_LOW_MV=1050
```

## 設定オプション

| オプション | 説明 | デフォルト値 |
|-----------|------|------------|
| `CONFIG_ZMK_NON_LIPO_BATTERY_MANAGEMENT` | 機能を有効にする | n/a |
| `CONFIG_ZMK_NON_LIPO_MIN_MV` | 最小電圧（ミリボルト単位、0%充電に対応） | 1100 |
| `CONFIG_ZMK_NON_LIPO_MAX_MV` | 最大電圧（ミリボルト単位、100%充電に対応） | 1300 |
| `CONFIG_ZMK_NON_LIPO_LOW_MV` | シャットダウンのしきい値電圧（ミリボルト単位） | 1050 |

## 動作の仕組み

このモジュールは：
1. 指定されたADCチャンネルを使用してバッテリー電圧を測定します
2. 最小/最大設定に基づいて電圧をパーセンテージに変換します
3. バッテリーの状態をZMKバッテリーモニタリングシステムに報告します
4. 低電圧状態を監視し、検出された場合にシステムシャットダウンをトリガーします

アルカリ電池の典型的な電圧は：
- 新品時：1セルあたり1.5V（100%）
- 消耗時：1セルあたり1.0V（0%）
- システムシャットダウン：バッテリーの損傷を防ぐため、1セルあたり約1.0〜1.1V