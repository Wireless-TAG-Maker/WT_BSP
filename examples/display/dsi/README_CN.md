| Supported Targets | WT9932P4-TINY | WT9932P4C61-TINY |
| ----------------- | ------------- | ---------------- |

# DSI 显示示例

本示例演示如何使用 Wireless-Tag BSP 提供的 MIPI DSI 接口驱动液晶屏，并启动 LVGL 进行 UI 渲染。

## 🛠️ 快速上手

1.  **选择开发板**：在示例目录中运行 `idf.py set-board` 并选择支持的开发板，命令会生成下一次配置/构建使用的板级默认配置：

```shell
~/WT_BSP/examples/display/dsi$ idf.py set-board
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
~/WT_BSP/examples/display/dsi$ idf.py build
Executing action: all (aliases: build)
Running ninja in directory ~/WT_BSP/examples/display/dsi/build
Executing "ninja all"...
...
```

之后修改了代码，再次执行 `idf.py build` 进行编译。

> 开发过程中可以随时根据需要切换到不同的硬件套件；切换到不同目标芯片时会自动执行 `idf.py fullclean`。

## 硬件要求

目前支持的开发板：

* **WT9932P4-TINY**，配套 MIPI DSI 屏幕。
* **WT9932P4C61-TINY**，配套 MIPI DSI 屏幕。

请确保 MIPI DSI 屏幕已正确连接，并确保开发板供电充足。

## 示例效果

示例正常运行后，液晶屏会点亮并显示 LVGL widgets demo。串口监视器中应看到类似日志：

```text
I (324) dsi_example: Initializing BSP
I (324) dsi_example: Starting LVGL
I (324) dsi_example: Turning on display and setting brightness
I (324) dsi_example: DSI example ready
```

## 核心 API

* `wt_bsp_init()`：初始化包括 DSI 在内的所有已启用板载外设。
* `wt_bsp_get_dsi()`：获取 DSI 显示对象句柄。
* `wt_bsp_dsi_lvgl_start()`：启动 LVGL port 并注册显示驱动。
* `wt_bsp_dsi_lvgl_lock()` / `wt_bsp_dsi_lvgl_unlock()`：保护 LVGL API 调用。
