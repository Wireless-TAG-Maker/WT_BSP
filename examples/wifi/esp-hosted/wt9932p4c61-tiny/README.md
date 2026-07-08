| Supported Targets | WT9932P4C61-TINY |
| ----------------- | ---------------- |

# ESP32-P4 ESP-Hosted Master

This example runs ESP32-P4 as the ESP-Hosted master and uses the onboard ESP32-C61 Wi-Fi capability through SDIO.

## Getting Started

1.  **Select the development board**: Run `idf.py set-board` in the example directory and select a supported development board. The command generates the board-level default configuration used by the next configure/build step:

```shell
~/WT_BSP/examples/wifi/esp-hosted/wt9932p4c61-tiny$ idf.py set-board
...
Supported boards in this example:
0: WT9932P4C61-TINY (esp32p4)

Please select the target board by entering the corresponding number.
Enter board number:
```

Enter the number that matches your hardware kit model, then press Enter. A successful selection prints output similar to this:

```shell
Enter board number: 0
Generated sdkconfig.board
Generated sdkconfig.board.Kconfig
Updated build/sdkconfig
Selected WT9932P4C61-TINY (esp32p4)
```

Then run `idf.py build` to compile:

```shell
~/WT_BSP/examples/wifi/esp-hosted/wt9932p4c61-tiny$ idf.py build
Executing action: all (aliases: build)
Running ninja in directory ~/WT_BSP/examples/wifi/esp-hosted/wt9932p4c61-tiny/build
Executing "ninja all"...
...
```

After modifying the code, run `idf.py build` again to compile.

> During development, you can switch to another hardware kit whenever needed. When switching to a different target chip, `idf.py fullclean` is automatically executed.

## ESP32-C61 Slave Firmware

Before running the ESP32-P4 ESP-Hosted master, make sure the ESP32-C61 slave firmware is flashed.

1. Connect FUSB and flash ESP32-P4 as the C61 bridge:

```bash
cd ./examples/wifi/esp-hosted/wt9932p4c61-tiny
idf.py p4_flash
```

2. Unplug FUSB, connect HUSB, then build and flash the ESP32-C61 slave firmware:

```bash
idf.py -C managed_components/espressif__esp_hosted/slave/ -B build_slave set-target esp32c61
idf.py -C managed_components/espressif__esp_hosted/slave/ -B build_slave build
idf.py -C managed_components/espressif__esp_hosted/slave/ -B build_slave flash -p <HUSB_CDC_PORT>
```

3. Connect FUSB again and flash the ESP32-P4 master firmware:

```bash
idf.py flash monitor
```

## BSP Integration

The example uses the `WT9932P4C61-TINY` board configuration:

* BSP initialization: `wt_bsp_init()`
* RGB LED status indication: `wt_bsp_get_rgb()`
* Provisioning button: `wt_bsp_get_button()`

## RGB LED Status

| Color | State |
|-------|-------|
| Blue | Connecting or reconnecting Wi-Fi |
| Green | Wi-Fi connected |
| Red | Wi-Fi connection failed |
| Purple | Wi-Fi provisioning mode is active |
| Off | Initial state |

## Button Provisioning

Long-press the onboard button for about one second to enter Wi-Fi provisioning mode. Connect a phone or computer to the configuration AP and open `http://192.168.4.1` to configure Wi-Fi.

## Example Output

### ESP32-C61 Slave Output

```text
I (xxx) fg_mcu_slave: ESP-Hosted-MCU Slave FW version :: 0.0.6
I (xxx) fg_mcu_slave: Transport used :: SDIO only
I (xxx) fg_mcu_slave: Initial set up done
```

### ESP32-P4 Master Output

```text
I (xxx) wifi_remote: Initializing BSP...
I (xxx) wifi_remote: wifi_init_sta finished.
I (xxx) wifi_remote: got ip:192.168.1.100
I (xxx) wifi_remote: connected to ap SSID:Your_WiFi password:Your_Password
```

## References

* [ESP-Hosted documentation](https://github.com/espressif/esp-hosted-mcu)
