| Supported Targets | WT9932P4-TINY | WT9932P4C61-TINY |
| ----------------- | ------------- | ---------------- |

# CSI 摄像头示例

本示例演示如何使用 Wireless-Tag BSP 提供的 MIPI CSI 接口采集摄像头画面，并通过 PPA 硬件加速实时显示到 MIPI DSI 液晶屏上。

## 🛠️ 快速上手

1.  **选择开发板**：在示例目录中运行 `idf.py set-board` 并选择支持的开发板，命令会生成下一次配置/构建使用的板级默认配置：

```shell
~/WT_BSP/examples/camera/csi$ idf.py set-board
...
Supported boards in this example:
0: WT9932P4-TINY (esp32p4)
1: WT9932P4C61-TINY (esp32p4)

Please select the target board by entering the corresponding number.
Enter board number:
```

根据你的硬件套件型号输入对应数字，然后按下回车（Enter）按键。选择成功后会看到类似输出：

```shell
Enter board number: 0
Generated sdkconfig.board
Generated sdkconfig.board.Kconfig
Updated build/sdkconfig
Selected WT9932P4-TINY (esp32p4)
```

然后执行 `idf.py build` 进行编译：

```shell
~/WT_BSP/examples/camera/csi$ idf.py build
Executing action: all (aliases: build)
Running ninja in directory ~/WT_BSP/examples/camera/csi/build
Executing "ninja all"...
...
```

之后修改了代码，再次执行 `idf.py build` 进行编译。

> 开发过程中可以随时根据需要切换到不同的硬件套件；切换到不同目标芯片时会自动执行 `idf.py fullclean`。

## 硬件要求

目前支持的开发板：

* **WT9932P4-TINY**，配套 MIPI DSI 屏幕和 SC2336 MIPI CSI 摄像头。
* **WT9932P4C61-TINY**，配套 MIPI DSI 屏幕和 SC2336 MIPI CSI 摄像头。

请确保 MIPI DSI 屏幕和 MIPI CSI 摄像头已正确连接。该硬件中 IO0 连接到摄像头 PWDN/LDO/RESET 控制路径，BSP 会在检测摄像头前自动将 IO0 拉高。

## 示例效果

示例正常运行后，液晶屏会显示摄像头实时画面。串口监视器中应看到类似日志：

```text
I (324) csi_example: Initializing Wireless-Tag BSP
I (324) csi_example: Setting up DSI display
I (324) csi_example: Starting CSI camera stream
I (324) wt_bsp_csi: Video Stream Start
I (324) csi_example: System ready. Camera feed should be visible on the screen.
```

## 核心 API

* `wt_bsp_init()`：初始化包括 DSI 和 CSI 在内的所有已启用板载外设。
* `wt_bsp_get_csi()`：获取 CSI 摄像头对象句柄。
* `wt_bsp_csi_start()`：开始摄像头视频流采集并注册帧回调。
* `wt_bsp_dsi_get_panel_handle()`：获取底层屏幕句柄以绘制图像帧。
