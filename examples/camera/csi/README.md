| Supported Targets | WT9932P4-TINY |
| ----------------- | ------------- |

# CSI 摄像头示例

本示例演示了如何使用 Wireless-Tag BSP 提供的 MIPI CSI 接口来驱动摄像头，并将采集到的图像实时显示在 MIPI DSI 液晶屏上。

## 如何使用示例

### 硬件要求

目前支持的开发板：

*   **WT9932P4-TINY** (配套 1024x600 MIPI DSI 屏幕和 SC2336 MIPI CSI 摄像头)

#### 硬件连接说明

1.  确保 MIPI DSI 屏幕已正确连接到开发板。
2.  确保 MIPI CSI 摄像头已正确连接到开发板。
3.  确保开发板供电充足（建议使用 5V 适配器）。

### 配置工程

在编译之前，您需要设置目标芯片为 `esp32p4`：

```bash
idf.py set-target esp32p4
```

本示例默认配置使用 SC2336 摄像头，分辨率为 1024x600。如需更改，请在 `idf.py menuconfig` 中调整。

### 编译与烧录

编译工程并烧录到开发板：

```bash
idf.py build flash monitor
```

## 预期输出

当示例正常运行时，开发板上的液晶屏应显示摄像头捕获的实时画面。串口监视器中应看到类似以下的日志：

```text
I (324) wt_bsp_csi: CSI initialized successfully
I (324) dsi_example: Initializing BSP
I (324) wt_bsp_dsi: DSI display initialized: 1024x600, panel=1, lanes=2
I (324) csi_example: Setting up DSI display
I (324) csi_example: Starting CSI camera stream
I (324) wt_bsp_csi: Video Stream Start
I (324) csi_example: System ready. Camera feed should be visible on the screen.
```

## 核心 API

*   `wt_bsp_init()`: 初始化包括 DSI 和 CSI 在内的所有板载外设。
*   `wt_bsp_get_csi()`: 获取 CSI 摄像头对象句柄。
*   `wt_bsp_csi_start()`: 开始摄像头视频流采集并注册回调函数。
*   `wt_bsp_dsi_get_panel_handle()`: 获取底层屏幕句柄以进行图像绘制。
