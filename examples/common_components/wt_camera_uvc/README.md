# Camera UVC application component

This component belongs to the repository applications, outside `wt_bsp`.
It connects two independently enabled BSP capabilities: CSI/JPEG or the camera
capture API, and USB UVC. It owns stream callbacks, bridge buffers, frame copying,
CSI JPEG conversion, and application USB descriptor profiles.

The application initializes the BSP first, then explicitly calls
`camera_uvc_start()`. It must call `camera_uvc_stop()` successfully before
`wt_bsp_deinit()` or releasing camera resources. BSP initialization alone does
not start USB video or connect a camera to USB.

CSI is selected when enabled; otherwise the board-owned JPEG camera API is used.
The camera feature owns DVP capture, sensor profiles and JPEG conversion for
that API. Boards supply hardware configuration and hold the camera object;
this application component consumes public frames and manages their USB delivery.
Enable the required source explicitly in the project's configuration. The P4
CSI profile enables the hardware JPEG video device and sets USB video to
1024x600 at 30 FPS, matching the TINY boards' CSI capture mode. This also applies
when enabling UVC in either P4 factory application. The S31 profiles align USB
video with the selected sensor. These profiles require both the selected source
and UVC to be enabled; enabling UVC alone does not enable a camera.

CSI frames are copied into a cache-aligned PSRAM buffer before being submitted
to the JPEG encoder as USERPTR input. The allocator uses the current target's
cache requirements, including P4 configurations with 128-byte cache lines.

The S31 factory application selects local recording versus host streaming.
The BSP camera API does not inspect USB state or decide which consumer wins.

## Add the bridge to an application

First integrate `wt_bsp` and configure the target and board as described in the
[BSP guide](../../../components/wt_bsp/README.md). Then append this application
component directory to the existing `EXTRA_COMPONENT_DIRS` before including
ESP-IDF's `project.cmake` and calling `project()`. Replace the repository path
in this top-level CMake excerpt:

```cmake
set(WT_BSP_REPOSITORY "/path/to/WT_BSP")
list(APPEND EXTRA_COMPONENT_DIRS
    "${WT_BSP_REPOSITORY}/examples/common_components")

# Keep the project's BSP integration and configuration setup above this include.
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(my_camera_uvc)
```

This excerpt only adds `wt_camera_uvc`. The BSP guide covers the project layout
needed for the `set-board` then `build` flow: the tool discovers the BSP from the
project's ancestor directories. A project using only an external BSP path can
select its target and board through native ESP-IDF configuration; the external
path alone does not provide that repository layout.
In `main/CMakeLists.txt`, declare the components used by the application source:

```cmake
idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES wt_bsp wt_camera_uvc)
```

The application includes its bridge header and uses only the public BSP entry
header for board resources:

```c
#include "wt_bsp.h"
#include "camera_uvc.h"
```

Enable `CONFIG_WT_BSP_ENABLE_USB_DEVICE_UVC` and the intended frame source:
`CONFIG_WT_BSP_ENABLE_CSI` for P4 CSI, or `CONFIG_WT_BSP_ENABLE_CAMERA` for a
board-owned JPEG camera such as S31. If both sources are enabled, this bridge
chooses CSI; disable CSI to choose the JPEG camera API. Enabling UVC alone does
not enable a source. Select the actual sensor in the board's settings; USB
descriptor profiles belong to this application component's configuration.

Check `wt_bsp_init()` before calling `camera_uvc_start()`, and handle the start
result. While the bridge owns the source, do not start a competing CSI preview
or camera consumer. On shutdown, stop application consumers, call
`camera_uvc_stop()` and check that it succeeds, then call `wt_bsp_deinit()`.
A stop error retains bridge resources for retry; keep the BSP and source alive
until that retry succeeds. This ordering does not establish that the CSI
backend globally destroys and recreates every video or SCCB resource.

This component's `idf_component.yml` selects `esp_video` as a private dependency
only for the P4 CSI-to-UVC branch. Keep that manifest with the component when
reusing it; applications need not include `esp_video` headers or expose its types.
The manifest handles the managed dependency condition because Kconfig is not
available during ESP-IDF's initial component requirements discovery. Basic IDF
requirements remain declared in CMake independently of the enabled source.
The JPEG driver is an explicit requirement so the encoder remains available in
applications using ESP-IDF's minimal build mode.
