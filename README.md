# ZMK Non-LiPo Battery Management Module

This module provides battery management functionality for non-LiPo batteries (such as alkaline or NiMH) in ZMK keyboards. It includes voltage monitoring, battery percentage calculation, and power management features.

## Features

- Battery voltage monitoring through ADC
- Battery percentage calculation based on configurable min/max voltage range
- Low battery automatic shutdown to protect the battery
- Optional GPIO control for power to voltage divider
- Advertising timeout sleep to conserve battery when left unpaired

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
        io-channels = <&adc 2>;  // Replace with your ADC channel number
        
        // Optional: Power control GPIO for the voltage divider
        power-gpios = <&gpio0 3 GPIO_ACTIVE_HIGH>;  // Replace with your GPIO
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
CONFIG_ZMK_NON_LIPO_ADV_SLEEP_TIMEOUT=60000  # 60 seconds
```

## Configuration Options

| Option | Description | Default |
|--------|-------------|---------|
| `CONFIG_ZMK_NON_LIPO_BATTERY_MANAGEMENT` | Enable the feature | n/a |
| `CONFIG_ZMK_NON_LIPO_MIN_MV` | Minimum voltage in millivolts (corresponds to 0% charge) | 1100 |
| `CONFIG_ZMK_NON_LIPO_MAX_MV` | Maximum voltage in millivolts (corresponds to 100% charge) | 1300 |
| `CONFIG_ZMK_NON_LIPO_LOW_MV` | Shutdown threshold voltage in millivolts | 1050 |
| `CONFIG_ZMK_NON_LIPO_ADV_SLEEP_TIMEOUT` | Time in milliseconds after which the device will sleep if left in advertising mode | 60000 |

## Advertising Sleep Timeout

This module includes a feature to automatically put the keyboard into deep sleep if it's been advertising (waiting for connection) for too long. This helps conserve battery when the keyboard is powered on but not connected.

When the keyboard disconnects from a device, it starts advertising and a timer begins. If no connection is established within `CONFIG_ZMK_NON_LIPO_ADV_SLEEP_TIMEOUT` milliseconds (default: 60 seconds), the keyboard will enter deep sleep mode.

To wake the keyboard from deep sleep, press the reset button or any key configured to wake the keyboard.

## How It Works

The module:
1. Measures battery voltage using the specified ADC channel
2. Converts voltage to percentage based on min/max settings
3. Reports battery status to ZMK battery monitoring system
4. Monitors for low voltage condition and triggers system shutdown if detected
5. Automatically enters deep sleep mode after advertising timeout

For alkaline batteries, typical voltages are:
- 1.5V per cell when new (100%)
- 1.0V per cell when depleted (0%)
- System shutdown around 1.0-1.1V per cell to prevent battery damage

## Example Configuration

Here is an example configuration for a keyboard using two AA batteries (1.5V nominal):

```
CONFIG_ZMK_NON_LIPO_MIN_MV=2000
CONFIG_ZMK_NON_LIPO_MAX_MV=3000
CONFIG_ZMK_NON_LIPO_LOW_MV=1900
CONFIG_ZMK_NON_LIPO_ADV_SLEEP_TIMEOUT=300000  # 5 minutes
```

---

# ZMK 非LiPoバッテリー管理モジュール

このモジュールは、ZMKを搭載したキーボードで非LiPoバッテリー（アルカリ電池やニッケル水素電池など）のモニタリングと管理機能を提供します。

## 機能

- ADCを使用したバッテリー電圧モニタリング
- 設定可能な最小/最大電圧に基づく直線的な電圧-パーセント変換
- バッテリーの保護のための低電圧自動シャットダウン
- 電圧分圧器への電源供給のためのオプションのGPIO制御
- ペアリングされていない場合のバッテリー節約のためのアドバタイジングタイムアウトスリープ

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
        
        // オプション: 電圧分圧器の電源制御GPIO
        power-gpios = <&gpio0 3 GPIO_ACTIVE_HIGH>;  // 使用するGPIOに置き換えてください
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
CONFIG_ZMK_NON_LIPO_ADV_SLEEP_TIMEOUT=60000  # 60秒
```

## 設定オプション

| オプション | 説明 | デフォルト値 |
|-----------|------|------------|
| `CONFIG_ZMK_NON_LIPO_BATTERY_MANAGEMENT` | 機能を有効にする | n/a |
| `CONFIG_ZMK_NON_LIPO_MIN_MV` | 最小電圧（ミリボルト単位、0%充電に対応） | 1100 |
| `CONFIG_ZMK_NON_LIPO_MAX_MV` | 最大電圧（ミリボルト単位、100%充電に対応） | 1300 |
| `CONFIG_ZMK_NON_LIPO_LOW_MV` | シャットダウンのしきい値電圧（ミリボルト単位） | 1050 |
| `CONFIG_ZMK_NON_LIPO_ADV_SLEEP_TIMEOUT` | アドバタイジングモードで放置された場合にデバイスがスリープするまでの時間（ミリ秒単位） | 60000 |

## アドバタイジングタイムアウトスリープ

このモジュールには、キーボードがアドバタイジング（接続待機）状態で長時間放置された場合に自動的にディープスリープに移行する機能が含まれています。これにより、キーボードが電源オン状態で接続されていない場合のバッテリー消費を節約できます。

キーボードがデバイスから切断されると、アドバタイジングを開始し、タイマーが作動します。`CONFIG_ZMK_NON_LIPO_ADV_SLEEP_TIMEOUT`ミリ秒（デフォルト: 60秒）以内に接続が確立されない場合、キーボードはディープスリープモードに移行します。

ディープスリープからキーボードを起動するには、リセットボタンを押すか、キーボードを起動するように設定された任意のキーを押してください。

## 動作の仕組み

このモジュールは：
1. 指定されたADCチャンネルを使用してバッテリー電圧を測定します
2. 最小/最大設定に基づいて電圧をパーセンテージに変換します
3. バッテリーの状態をZMKバッテリーモニタリングシステムに報告します
4. 低電圧状態を監視し、検出された場合にシステムシャットダウンをトリガーします
5. アドバタイジングタイムアウト後に自動的にディープスリープモードに移行します

アルカリ電池の典型的な電圧は：
- 新品時：1セルあたり1.5V（100%）
- 消耗時：1セルあたり1.0V（0%）
- システムシャットダウン：バッテリーの損傷を防ぐため、1セルあたり約1.0〜1.1V

## 設定例

以下は、2本のAA電池（公称電圧1.5V）を使用するキーボードの設定例です：

```
CONFIG_ZMK_NON_LIPO_MIN_MV=2000
CONFIG_ZMK_NON_LIPO_MAX_MV=3000
CONFIG_ZMK_NON_LIPO_LOW_MV=1900
CONFIG_ZMK_NON_LIPO_ADV_SLEEP_TIMEOUT=300000  # 5分
```