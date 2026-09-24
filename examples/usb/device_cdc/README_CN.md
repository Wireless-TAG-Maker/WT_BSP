# USB Device CDC 回显

WT9932P4-TINY 对应 ESP32-P4 v1.x，WT9932P4X-TINY 对应 v3.x；两者外设接线相同，固件不能混用。请在 `idf.py set-board` 中选择实际板型，详见[版本与构建说明](../../../components/wt_bsp/boards/WT9932P4X-TINY/README.md)。

本示例通过 BSP 提供 USB CDC ACM 串口回显，支持 ESP32-P4 和 ESP32-S31 板卡。

| 开发板 | ESP-IDF | 烧录 / 日志接口 | CDC 回显接口 |
| --- | --- | --- | --- |
| WT9932P4-TINY | v6.0.0 及以上 | FUSB | HUSB（High-Speed USB OTG） |
| WT9932P4X-TINY | v6.0.0 及以上 | FUSB | HUSB（High-Speed USB OTG） |
| WT9932P4C61-TINY | v6.0.0 及以上 | FUSB | HUSB（High-Speed USB OTG） |
| WT9932S31-TINY | v6.1 及以上 | J2 | J1（High-Speed USB OTG） |

WT9932P4C61-TINY 的本示例运行在 ESP32-P4 上。

从仓库根目录构建：

```sh
WT_BSP_BOARD=WT9932P4-TINY idf.py -C examples/usb/device_cdc -B build-cdc-p4 build
WT_BSP_BOARD=WT9932P4X-TINY idf.py -C examples/usb/device_cdc -B build-cdc-p4x build
WT_BSP_BOARD=WT9932P4C61-TINY idf.py -C examples/usb/device_cdc -B build-cdc-p4c61 build
WT_BSP_BOARD=WT9932S31-TINY idf.py --preview -C examples/usb/device_cdc build
```

选择与实物匹配的一条命令；ESP-IDF v6.1 中 S31 命令需要 `--preview`。
也可以在示例目录运行 `idf.py set-board` 选择板卡，S31 使用 `idf.py --preview set-board`。

通过表中的烧录接口写入固件后，将 CDC 回显接口连接主机，打开新出现的
`Wireless-Tag CDC` 串口并启用 DTR。发送的数据会原样回显；烧录 / 日志串口
不承担该回显功能。USB CDC 的波特率设置不会改变 USB 传输速率。

应用仅通过 `wt_bsp.h` 初始化 BSP、获取 CDC 句柄和读写数据；短写时只重试尚未
排队的后缀，USB 回调不执行阻塞操作。主机断开或取消 DTR 后停止回显当前数据块。

本示例使用独立 CDC 配置，关闭 UVC。P4 的独立 CDC 与 UVC 配置互斥；S31 可在
[工厂示例](../../wt_factory/wt9932s31-tiny/README.md)中通过复合设备同时提供 UVC 和 CDC。
BSP 持有 TinyUSB 驱动，应用不要再次安装或释放它。
S31 接线细节见[板卡说明](../../../components/wt_bsp/boards/WT9932S31-TINY/README.md)。
