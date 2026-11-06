# BSP 分层改造验证记录

验证日期：2026-11-04。所有构建在 `/tmp` 副本执行，未向开发板烧录。
先完成最终源码的全部测试，再补充 Markdown 文档；提交前单独核对所有非文档文件
的内容和权限与测试快照一致。构建产物、依赖锁文件及模拟器测试夹具不进入 BSP。

## 最终结果

| 验证项目 | 环境与范围 | 结果 |
| --- | --- | --- |
| 完整示例矩阵 | ESP-IDF v6.1；由 `.github/scripts/build_examples.py --matrix` 动态生成，每组独立副本，实际执行 set-board/set-target 和 build | 40/40 |
| 最低 SDK 代表构建 | ESP-IDF v6.0；C3 blink、P4 CSI、P4 DSI、P4 UVC | 4/4 |
| 配置回归单测 | ESP-IDF v6.0、v6.1；每个环境 11 项，均无跳过 | 22/22 |
| 实际选板/配置回归 | 8 个选板入口、原生 choice、P4/P4C61/P4X 预设、保留配置、交互及环境变量跨目标 fullclean | 15/15 |
| 实际托管依赖切换 | C3 普通 reconfigure：RGB y→n→y，检查 JSON、led_strip 依赖锁及选板保留 | 2/2 |
| 非默认工厂分支 | P4 UVC+CSI、P4C61 UVC+CSI、S31 OV3660；核实实际配置和编译源文件 | 3/3 |
| 拆分提交中间版本 | 选板版本 C3/P4、CSI 版本 P4、UVC 版本 S31/UVC、S31 工厂和 P4/UVC | 6/6 |
| 官方模拟器 | Espressif esp-emu v0.51.0；以下五组构建和运行 | 5/5 |

完整矩阵覆盖 CSI、UVC、DSI、blink、button、C61 独立工程、SDMMC、CDC、
ESP-Hosted 和工厂工程。40 是本次快照的结果，后续验证仍以动态矩阵为准。
本地执行矩阵不等同于验证 GitHub 事件触发或云端 SDK 安装。

配置单测使用真实 Kconfig JSON 序列化和 Component Manager 条件解析器。
每个 SDK 的清单测试显式覆盖 102 个 SDK/目标/功能子组合，并检查实际版本条件，
捕获隐藏 bool 缺失、错误引号、布尔条件遗漏及最低 SDK 版本选择问题。
本次使用 esp-idf-kconfig 3.14.0 和 IDF Component Manager 3.1.2。

ESP-IDF 6.0 的兼容选择为 `esp_lvgl_port 2.8.*` 和相关 P4 显示/视频依赖的
`esp_h264 1.3.0`；6.1 下的既有依赖解析结果保持不变。S31 只在 v6.1 下验证，
该 SDK 中使用 `--preview`。

## 官方模拟器覆盖

使用 [Espressif 官方 esp-emulator](https://github.com/espressif/esp-emulator)，
每组要求构建成功、模拟器退出码为 0、收到完整成功标记且无检测到的固件异常。

| 测试组 | 实际验证内容 |
| --- | --- |
| 原生 WT9932C3 | BSP 初始化，board/RGB/button getter，不可用 camera/UVC 返回 NULL，反初始化 |
| P4 虚拟 camera-only 板 | 仅新增板目录和 mock sensor 的 `.c/.h`；未知驱动/非法属性拒绝，真实 GC2145/OV3660 factory 延迟初始化，自注册发现，统一 prepare/capture/status 分发，清理超时保留所有权后重试成功 |
| 原生 S31 GC2145 | 板级统一 info 路由到真实 GC2145 factory，延迟初始化、状态查询、反初始化 |
| 原生 S31 OV3660 | 板级统一 info 路由到真实 OV3660 factory，延迟初始化、状态查询、反初始化 |
| 原生 P4 UVC-only | CSI/camera 关闭，真实 UVC feature 和依赖参与编译链接；BSP 准备独立对象并释放，不启动 USB |

虚拟板只声明 board/camera 能力，没有为通过依赖解析而添加无关能力。
测试副本仅额外增加六个板级夹具文件和两个 mock sensor 文件，生产文件逐项保持一致。
链接证据检查了 GC2145、OV3660、mock 的自注册项与自动生成的保留锚点，
不依赖将整个 BSP 静态库强制链接。

模拟器测试固件在初始化、接口断言和清理后打印唯一成功标记；构建后合并 flash 镜像，
通过 `--firmware`、`--elf`、`--timeout 45s`、`--exit-on <成功标记>` 执行。
测试夹具仅存在于本地验证副本，仓库示例仍是实际应用，不会打印这些专用成功标记。
模拟器二进制 SHA256：

```text
162761d52bcee36fe8be5fd039202fed5873ca550a83a12229038311a4bbc18c
```

## 复验入口

加载对应 SDK 后，在仓库根目录运行配置回归：

```sh
python -B -m unittest discover -s components/wt_bsp/tools/tests -p 'test_*.py' -v
python -B .github/scripts/build_examples.py --matrix
```

若测试被跳过，先检查是否实际加载了 ESP-IDF Python/Kconfig 环境；跳过不计为通过。
完整矩阵采用 [AGENTS.md](../AGENTS.md) 中的 `/tmp` 快照流程。
修改构建代码或配置后，应建立新快照并重新验证完整矩阵。

单个应用继续使用原来的两步构建流程，例如：

```sh
WT_BSP_BOARD=WT9932P4-TINY idf.py -C examples/camera/usb_device_uvc set-board
idf.py -C examples/camera/usb_device_uvc build
```

中间版本验证将对应功能 tree 同步到独立工作副本，保留构建缓存后重新配置、编译和链接；
构建前后逐文件核对源代码，避免将最终工作区误当作中间 commit 的测试结果。

## 已知限制与实板验收

- Mock 帧只证明 interface/ops 和回调分发；GC2145/OV3660 模拟器用例止于延迟初始化，
  没有探测实体传感器，也没有验证真实采集、JPEG 编码质量、时序或吞吐。
- UVC-only 用例只准备和释放对象。USB 枚举、视频传输、主机兼容性和复合 CDC/UVC
  仍需连接实板；工厂和 UVC 示例的编译通过不能代替这些检查。
- CSI 本次修改覆盖任务停止等待和回调缓冲的释放顺序。既有全局 `esp_video`/SCCB
  清理链仍有缺口：关闭采集文件描述符没有成对执行全局 video 反初始化。
  完整硬件 stop/reinit 及 BSP init→deinit→init 循环尚未验收；mock 清理重试结果只证明软件所有权契约。
- esp-emu v0.51.0 不支持 C2/C61，S31 属于早期支持；这些边界不能用其他芯片的通过结果代替。
- GC2145 的单用户 DVP 缓冲及 PSRAM 预算调整需实板确认持续采集和内存余量。
  实板还应覆盖流的开停重连、停止失败恢复、录像/推流仲裁、SD 文件完整性及各类外设。
- README 中 2026-09/10 的实板记录和截图属于调整前固件，不能作为本次分层迁移的实板验收。

## 本次证据索引

最终测试快照 SHA256：

```text
982acdee56410feff9fa7cc25145390c48db2553689a35d5cde9d37e45760cf5
```

以下路径是本次本地临时证据，清理 `/tmp` 后不再保留；上面的结果、覆盖范围和复验方法
作为仓库内的持久记录。补文档后另存非文档文件一致性审计，不覆盖原测试快照核对结果。

| 证据 | 本地位置 |
| --- | --- |
| 完整矩阵及每组日志 | `/tmp/wt-bsp-final-ci-04/summary.json`、同目录 `case-*.log` |
| 最低 SDK、配置和选板回归 | `/tmp/wt-bsp-idf60-ci04-m4theoy8/final-validation.json` |
| 非默认分支、模拟器、夹具和 ELF/镜像哈希 | `/tmp/wt-bsp-emulator-final-04-2xu58wsz/validation-report.md`、`summary.json` |
| 中间功能版本构建 | `/tmp/wt-bsp-commit-validation-w36i30yy/summary.json` |
| 文档完成后的构建输入一致性 | `/tmp/wt-bsp-final-ci-04/post-documentation-verification.json` |
