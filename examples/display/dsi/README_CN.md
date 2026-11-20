| Supported Targets | WT9932P4-TINY | WT9932P4C61-TINY | WT9932P4X-TINY |
| --- | --- | --- | --- |

# DSI 显示示例

本示例通过 `wt_bsp.h` 初始化显示与触摸，运行 LVGL Widgets（默认）或官方 Benchmark。
默认保留原板载 ST7102 配置；三种外接屏幕组合只对 **WT9932P4X-TINY** 和
**WT9932P4C61-TINY** 开放。

WT9932P4-TINY 使用 P4 v1.x，WT9932P4X-TINY 使用 P4 v3.x，共用外设实现但固件不能混用。
选板时使用实际板名，详见 [P4X 版本与构建说明](../../../components/wt_bsp/boards/WT9932P4X-TINY/README.md)。

## 选择板卡、屏幕与 Demo

在本示例目录使用现有选板流程：

```sh
idf.py set-board
idf.py menuconfig
```

1. 在 `set-board` 中选择实际主板。
2. 在 `WT BSP -> DSI LCD adapter` 选择屏幕组合；默认项保持原板载屏幕。
3. 在 `DSI example -> LVGL demo` 选择 `Widgets` 或 `Benchmark`。

P4X 使用其独立 v3.x 预设；旧 P4 v1.x 不提供外接适配板选项。
驱动、时序、颜色格式与触摸方向由 feature 配置处理，示例不复制板级 GPIO 或面板初始化。

| 屏幕组合 | 面板配置 | 分辨率 / 格式 | DSI 速率 | DPI 请求 / 实际时钟 | 背光 PWM |
| --- | --- | --- | --- | --- | --- |
| 默认板载配置 | ST7102 | 480×640 / RGB888 | 2 × 1000 Mbps | 沿用原面板配置 | 沿用原配置 |
| WLK7002-MIPI-15P | JD9165BA | 1024×600 / RGB565 | 2 × 500 Mbps | 40 / 40 MHz | 2 kHz |
| WTDKP4-LCD-ADAPTER | EK79007 | 1024×600 / RGB565 | 2 × 1000 Mbps | 52 / 48 MHz | 5 kHz |
| EQT350XJI718P | ILI9488 | 320×480 / RGB565 | 1 × 500 Mbps | 12 MHz 请求 | 2 kHz |

EK79007 的 52 MHz 请求在 IDF v6.1 默认时钟源下整数分频为 48 MHz，日志中的取整提示是预期行为。
两种组合使用不同初始化和时序，不能只按 1024×600 分辨率互换驱动。
两种 1024×600 组合使用单 framebuffer、64 行 partial draw buffer 和 CPU copy；原板载配置保持原行为。
选择 RGB565 组合时，还需在 `LVGL configuration -> Color settings` 选择 16 bit（`CONFIG_LV_COLOR_DEPTH_16=y`），
与 RGB565 一致；默认板载屏幕保留本示例原有的 24 bit 配置。

## 接线

以下为两种 1024×600 外接组合的主板 GPIO 编号，不是排针位置编号：

| 功能 | 主板连接 |
| --- | --- |
| MIPI DSI | 主板专用 MIPI DSI 接口，2 data lanes |
| 背光 PWM | GPIO26，高有效 |
| LCD reset | GPIO27，低有效 |
| GT911 SDA / SCL | GPIO7 / GPIO8，配套 15 pin DSI 排线携带这两路信号 |
| GT911 reset / INT | 未配置软件 GPIO，使用轮询读取 |

WTDKP4-LCD-ADAPTER 按其案例还需外接 `5V -> 5V`、`GND -> GND`、
`PWM -> GPIO26`、`LCD_RST -> GPIO27`、`INIT_TP -> GND`。
底板供电与排线接触面按各自硬件核对，断电接线；GPIO26 只接背光驱动控制输入。
GT911 自动探测 0x5D / 0x14，保留工厂配置并按读取的范围缩放坐标。
WLK7002 默认不镜像，WTDKP4 默认同时镜像 X/Y；最终方向以实物点按、拖动和滚动验收。

### EQT350XJI718P 转接板

断电接线，使用与 30 pin 模组配套的转接板。DSI 只接 D0 和 CLK，D1 空置。
GPIO27 接低有效 LCD reset，GPIO26 接背光驱动 PWM 输入。CHSC6540 的 SDA/SCL
分别接 GPIO7/GPIO8，使用 7 位地址 `0x2e`，以 100 kHz 轮询。冷启动探测未应答时，BSP 会在 LCD 复位后重试。
LCD 使用 DCS 初始化，触摸缺失时仍可显示。请根据转接板核对供电、背光驱动和排线方向。

水平和垂直的 sync/back/front 均为 10/20/20。两个 RGB565 帧缓冲共占 600 KiB。
与其他双缓冲 P4 屏相同，LVGL 在后台缓冲中绘制整帧，提交后等 DPI 刷新回调再复用上一页。MADCTL 设为
`0x98`，行寻址和垂直刷新均自下而上。实机横向滑动时，观察竖直边缘在屏幕中部是否错开，
并检查四角、红蓝色块和触摸拖动。原始触点 (0, 0) 应映射到 (319, 479)。

## 编译、烧录与 Benchmark

选择好主板和配置后执行：

```sh
idf.py build
idf.py -p PORT flash monitor
```

将 `PORT` 换成实际 FUSB 串口，烧录本示例生成的完整 bootloader、分区表和应用。
也可从仓库根目录用已有显式选板方式构建：

```sh
WT_BSP_BOARD=WT9932P4X-TINY idf.py -C examples/display/dsi build
WT_BSP_BOARD=WT9932P4C61-TINY idf.py -C examples/display/dsi build
```

首次使用 `Benchmark` 时，在 menuconfig 选中该 Demo 后重新构建和烧录。
LVGL 9.5 及更新版本会在结束回调显示可滚动结果表，串口输出 `BENCHMARK_SCENE` 和
`BENCHMARK_DONE scenes=16 ...`；场景、时长和统计方法使用官方实现。
Widgets 保持默认行为。所有 LVGL 对象及触摸注册在 BSP 的 LVGL 锁内完成，缺少触摸时仍运行显示 Demo。

默认配置使用 LVGL 9 有效的 100 KiB 普通图层缓冲和 Montserrat 16 字体，避免对应配置提示。
FPS 和 LVGL sysmon CPU 为库内指标，不等于物理扫描率及全系统 CPU 占用。
USB Serial/JTAG 普通 RTS 复位可能保留下载启动状态；若只见 `wait usb download`，
释放 BOOT 并按主板 RESET 后观察启动，WSL 下同时确认 USB 已转发。

## 使用的 BSP API

- `wt_bsp_init()`、`wt_bsp_get_dsi()`、`wt_bsp_get_touch()`。
- `wt_bsp_dsi_lvgl_start()`、`wt_bsp_touch_lvgl_start()`。
- `wt_bsp_dsi_display_on()`、`wt_bsp_dsi_set_brightness()`。
- `wt_bsp_dsi_lvgl_lock()` / `wt_bsp_dsi_lvgl_unlock()`。

应用只包含 `wt_bsp.h` 使用 BSP，不直接访问 board 私有头或驱动资源。
