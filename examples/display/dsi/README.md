| Supported Targets | WT9932P4-TINY | WT9932P4C61-TINY | WT9932P4X-TINY |
| --- | --- | --- | --- |

# DSI Display Example

This example uses `wt_bsp.h` to initialize the display and touch, then runs LVGL Widgets
(the default) or the official Benchmark. The default retains the original ST7102 board
configuration. The two additional LCD adapter configurations are available only on
**WT9932P4X-TINY** and **WT9932P4C61-TINY**.

WT9932P4-TINY uses P4 v1.x; WT9932P4X-TINY uses P4 v3.x. They share peripheral wiring
but require different firmware. Select the actual board; see the
[P4X revision and build notes](../../../components/wt_bsp/boards/WT9932P4X-TINY/README.md).

## Select the Board, LCD and Demo

Run the existing board-selection flow in this example directory:

```sh
idf.py set-board
idf.py menuconfig
```

1. Select the actual host board with `set-board`.
2. Select the LCD configuration under `WT BSP -> DSI LCD adapter`; the default retains the original onboard panel.
3. Select `Widgets` or `Benchmark` under `DSI example -> LVGL demo`.

P4X uses its separate v3.x preset. The two new adapter choices are unavailable for the
original P4 v1.x configuration. Features own panel initialization, timings, color format
and touch orientation; the example does not copy GPIO definitions or panel commands.

| LCD configuration | Panel | Resolution / format | DSI rate | Requested / actual DPI clock | Backlight PWM |
| --- | --- | --- | --- | --- | --- |
| Board default | ST7102 | 480×640 / RGB888 | 2 × 1000 Mbps | Original panel configuration | Original configuration |
| WLK7002-MIPI-15P | JD9165BA | 1024×600 / RGB565 | 2 × 500 Mbps | 40 / 40 MHz | 2 kHz |
| WTDKP4-LCD-ADAPTER | EK79007 | 1024×600 / RGB565 | 2 × 1000 Mbps | 52 / 48 MHz | 5 kHz |

IDF v6.1 rounds the EK79007 52 MHz request to 48 MHz with its default integer divider;
that startup message is expected. Equal resolution does not make the initialization or
timings interchangeable. The new configurations use one framebuffer, a 64-line partial
draw buffer and CPU copy; the original board configuration retains its existing behavior.
For either new configuration, select 16 bit under `LVGL configuration -> Color settings`
(`CONFIG_LV_COLOR_DEPTH_16=y`) to match RGB565. Keep the original 24 bit example setting
for the default onboard panel.

## Wiring

These are host GPIO numbers, not header positions, for the two external LCD configurations:

| Signal | Host connection |
| --- | --- |
| MIPI DSI | Dedicated host MIPI DSI connector, 2 data lanes |
| Backlight PWM | GPIO26, active high |
| LCD reset | GPIO27, active low |
| GT911 SDA / SCL | GPIO7 / GPIO8, carried by the matching 15-pin DSI cable |
| GT911 reset / INT | No software GPIO configured; touch is polled |

The WTDKP4-LCD-ADAPTER reference also requires `5V -> 5V`, `GND -> GND`,
`PWM -> GPIO26`, `LCD_RST -> GPIO27`, and `INIT_TP -> GND`.
Check each adapter's supply and FPC contact orientation with power disconnected.
GPIO26 connects to the backlight driver's control input, not bare LED terminals.
GT911 probes 0x5D / 0x14, retains its factory configuration and scales the reported range.
WLK7002 defaults to no mirroring; WTDKP4 mirrors both axes. Verify orientation on the actual assembly.

## Build, Flash and Run Benchmark

After selecting the board and configuration:

```sh
idf.py build
idf.py -p PORT flash monitor
```

Replace `PORT` with the actual FUSB serial port. Use this example's complete bootloader,
partition table and application layout. The existing explicit board selection also works
from the repository root:

```sh
WT_BSP_BOARD=WT9932P4X-TINY idf.py -C examples/display/dsi build
WT_BSP_BOARD=WT9932P4C61-TINY idf.py -C examples/display/dsi build
```

Select `Benchmark` in menuconfig, then rebuild and flash. With LVGL 9.5 and newer, the end
callback shows a scrollable result table and exports `BENCHMARK_SCENE` records plus
`BENCHMARK_DONE scenes=16 ...`. Scenes, durations and measurements remain the official implementation.
Widgets remains the default. LVGL objects and touch registration are protected by the BSP
LVGL lock; the display demo can run without touch hardware.

The defaults use LVGL 9's valid 100 KiB ordinary-layer buffer option and Montserrat 16 font
to avoid the corresponding configuration warnings. FPS and LVGL sysmon CPU are library
metrics, not measured panel scanout rate or whole-system CPU utilization.
A USB Serial/JTAG RTS reset can retain download boot state. If the log only shows
`wait usb download`, release BOOT and press RESET, and check WSL USB forwarding.

## BSP APIs Used

- `wt_bsp_init()`, `wt_bsp_get_dsi()`, `wt_bsp_get_touch()`.
- `wt_bsp_dsi_lvgl_start()`, `wt_bsp_touch_lvgl_start()`.
- `wt_bsp_dsi_display_on()`, `wt_bsp_dsi_set_brightness()`.
- `wt_bsp_dsi_lvgl_lock()` / `wt_bsp_dsi_lvgl_unlock()`.

The application includes only `wt_bsp.h` for BSP access and does not access private board
headers or driver resources directly.
