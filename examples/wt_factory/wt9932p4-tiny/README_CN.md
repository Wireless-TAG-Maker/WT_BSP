| Supported Targets | WT9932P4-TINY |
| ----------------- | ------------- |

# 综合工厂测试示例 (Factory Firmware)

本工厂固件集成了 Wireless-Tag BSP 提供的 MIPI DSI 显示、MIPI CSI 摄像头、触摸、SD 卡和 RGB LED 控制等外设能力。

## 🛠️ 快速上手

1.  **选择开发板**：在示例目录中运行 `idf.py set-board` 并选择支持的开发板，命令会生成下一次配置/构建使用的板级默认配置：

```shell
~/WT_BSP/examples/wt_factory/wt9932p4-tiny$ idf.py set-board
...
Supported boards in this example:
0: WT9932P4-TINY (esp32p4)

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
~/WT_BSP/examples/wt_factory/wt9932p4-tiny$ idf.py build
Executing action: all (aliases: build)
Running ninja in directory ~/WT_BSP/examples/wt_factory/wt9932p4-tiny/build
Executing "ninja all"...
...
```

之后修改了代码，再次执行 `idf.py build` 进行编译。

> 开发过程中可以随时根据需要切换到不同的硬件套件；切换到不同目标芯片时会自动执行 `idf.py fullclean`。

## 硬件状态指示 (RGB LED)

系统启动时会检测外设连接状态，并通过板载 RGB LED 显示不同颜色来指示硬件状态：

| LED 颜色 | 硬件状态 | 说明 |
|----------|----------|------|
| 熄灭 | 全部正常 | 屏幕、摄像头、SD 卡均已连接并初始化 |
| 蓝色 | 摄像头未连接 | |
| 黄色 | SD 卡未连接 | |
| 粉色 | 屏幕未连接 | |
| 红色 | 全部未连接 | 屏幕、摄像头、SD 卡均未连接 |

## 核心功能

* **摄像头预览**：显示实时摄像头画面，点击画面可切换全屏预览和普通预览模式。
* **LED 控制**：调节板载 RGB LED 颜色。
* **SD 卡状态**：测试 SD 卡挂载并显示容量信息。
* **触摸控制**：使用 DSI 屏配套触摸功能。

## 工程结构

* `main/main.c`：主入口，负责 BSP 初始化、硬件状态检测、LVGL 启动及外设逻辑。
* `main/lvgl_ui.c`：UI 界面实现。
* `managed_components/`：ESP-IDF managed component 依赖。
