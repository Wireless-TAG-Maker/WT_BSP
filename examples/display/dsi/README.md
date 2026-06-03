| Supported Targets | WT9932P4-TINY |
| ----------------- | ------------- |

# DSI 显示示例

本示例演示了如何使用 Wireless-Tag BSP 提供的 MIPI DSI 接口来驱动液晶屏，并集成 LVGL8 图形库显示简单的 UI 界面。

## 如何使用示例

### 硬件要求

目前支持的开发板：

*   **WT9932P4-TINY** (配套 1024x600 或 800x1280 MIPI DSI 屏幕)

#### 硬件连接说明

1.  确保 MIPI DSI 屏幕已正确连接到开发板的 DSI 接口。
2.  确保开发板供电充足。

### 配置工程

在编译之前，您需要设置目标芯片为 `esp32p4`：

```bash
idf.py set-target esp32p4
```

### 编译与烧录

编译工程并烧录到开发板：

```bash
idf.py build flash monitor
```

## 预期输出

当示例正常运行时，开发板上的液晶屏应点亮并显示 "Hello Wireless-Tag DSI!" 文字。串口监视器中应看到类似以下的日志：

```text
I (314) main_task: Started on CPU0
I (324) main_task: Calling app_main()
I (324) dsi_example: Initializing BSP
I (324) wt_bsp_dsi: DSI display initialized: 1024x600, panel=1, lanes=2
I (324) dsi_example: Starting LVGL
I (324) dsi_example: Turning on display and setting brightness
I (324) dsi_example: DSI example ready
```

## 核心 API

*   `wt_bsp_init()`: 初始化包括 DSI 在内的所有板载外设。
*   `wt_bsp_get_dsi()`: 获取 DSI 显示对象句柄。
*   `wt_bsp_dsi_lvgl_start()`: 启动 LVGL 任务并注册显示驱动。
*   `wt_bsp_dsi_lvgl_lock()` / `wt_bsp_dsi_lvgl_unlock()`: LVGL 线程安全保护锁。
