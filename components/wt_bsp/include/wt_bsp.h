/**
 * @file wt_bsp.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief Wireless-Tag BSP 组件的公共入口。
 * @version 0.1
 * @date 2026-05-11
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

#ifndef __WT_BSP_H__
#define __WT_BSP_H__

/* ==================== [Includes] ========================================== */

#include "wt_bsp_config_internal.h"
#include "wt_bsp_board.h"
#include "wt_bsp_button.h"
#include "wt_bsp_rgb.h"
#include "wt_bsp_sdmmc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/**
 * @brief BSP 对象句柄。
 */
typedef struct wt_bsp_obj_t *wt_bsp_t;

/**
 * @brief 板级 BSP 操作表。
 *
 * 每个板级实现都会提供一个接口实例。@ref wt_bsp_init 成功后，
 * 通用的 `wt_bsp_*` 接口会通过该表转发到具体板级实现。
 */
typedef struct{
    /**
     * @brief 初始化当前选中板卡的资源。
     *
     * @return 成功时返回 ESP_OK，失败时返回 ESP-IDF 错误码。
     */
    esp_err_t (*init)(void);

    /**
     * @brief 反初始化当前选中板卡的资源。
     *
     * @return 成功时返回 ESP_OK，失败时返回 ESP-IDF 错误码。
     */
    esp_err_t (*deinit)(void);

    /**
     * @brief 获取板卡信息句柄。
     *
     * @return 板卡句柄；不可用时返回 NULL。
     */
    wt_bsp_board_t (*get_board)(void);

#if WT_BSP_BUTTON_ENABLE_IS_ENABLED
    /**
     * @brief 获取默认按键句柄。
     *
     * @return 按键句柄；不可用时返回 NULL。
     */
    wt_bsp_button_t (*get_button)(void);
#endif

#if WT_BSP_RGB_ENABLE_IS_ENABLED
    /**
     * @brief 获取默认 RGB LED 句柄。
     *
     * @return RGB LED 句柄；不可用时返回 NULL。
     */
    wt_bsp_rgb_t (*get_rgb)(void);
#endif

#if WT_BSP_SDCARD_ENABLE_IS_ENABLED
    /**
     * @brief 获取默认 SD 卡句柄。
     *
     * @return SD 卡句柄；不可用时返回 NULL。
     */
    wt_bsp_sdmmc_t (*get_sdmmc)(void);
#endif
} wt_bsp_interface_t;

/**
 * @brief 通用 BSP 对象。
 */
typedef struct wt_bsp_obj_t{
    /**
     * @brief 当前激活的板级接口。
     */
    wt_bsp_interface_t *interface;
} wt_bsp_obj_t;

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief 初始化当前选中板卡的 BSP。
 *
 * 该函数会获取板级接口，并初始化 BSP 组件对外暴露的板卡资源。
 *
 * @return 成功时返回 ESP_OK，失败时返回 ESP-IDF 错误码。
 */
esp_err_t wt_bsp_init(void);

/**
 * @brief 反初始化当前选中板卡的 BSP。
 *
 * @return 成功时返回 ESP_OK，失败时返回 ESP-IDF 错误码。
 */
esp_err_t wt_bsp_deinit(void);

/**
 * @brief 获取已初始化的板卡信息句柄。
 *
 * @return 板卡句柄；BSP 未初始化时返回 NULL。
 */
wt_bsp_board_t wt_bsp_get_board(void);

/**
 * @brief 获取已初始化的默认按键句柄。
 *
 * @return 按键句柄；BSP 未初始化或按键支持关闭时返回 NULL。
 */
wt_bsp_button_t wt_bsp_get_button(void);

/**
 * @brief 获取已初始化的默认 RGB LED 句柄。
 *
 * @return RGB LED 句柄；BSP 未初始化 or RGB 支持关闭时返回 NULL。
 */
wt_bsp_rgb_t wt_bsp_get_rgb(void);

/**
 * @brief 获取已初始化的默认 SD 卡句柄。
 *
 * @return SD 卡句柄；BSP 未初始化或 SD 卡支持关闭时返回 NULL。
 */
wt_bsp_sdmmc_t wt_bsp_get_sdmmc(void);

/**
 * @brief 获取已初始化的默认 SD 卡挂载点。
 *
 * @return 挂载点字符串；BSP 未初始化或 SD 卡支持关闭时返回空字符串。
 */
const char *wt_bsp_get_sdmmc_mount_point(void);

/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __WT_BSP_H__
