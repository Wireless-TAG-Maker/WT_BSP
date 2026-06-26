/**
 * @file wt_bsp_sdmmc.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief Wireless-Tag BSP 组件的 SDMMC 接口。
 * @version 0.1
 * @date 2026-05-24
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

#ifndef __WT_BSP_SDMMC_H__
#define __WT_BSP_SDMMC_H__

/* ==================== [Includes] ========================================== */

#include "wt_bsp_config_internal.h"
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"

/**
 * @brief SDMMC 对象句柄。
 */
typedef struct wt_bsp_sdmmc_obj_t *wt_bsp_sdmmc_t;

#if WT_BSP_SDCARD_ENABLE_IS_ENABLED

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief 挂载 SDMMC 文件系统。
 *
 * @param[in,out] sdmmc SDMMC 对象句柄。
 *
 * @return 成功时返回 ESP_OK。
 */
esp_err_t wt_bsp_sdmmc_mount(wt_bsp_sdmmc_t sdmmc);

/**
 * @brief 卸载 SDMMC 文件系统。
 *
 * @param[in,out] sdmmc SDMMC 对象句柄。
 *
 * @return 成功时返回 ESP_OK。
 */
esp_err_t wt_bsp_sdmmc_unmount(wt_bsp_sdmmc_t sdmmc);

/**
 * @brief 获取卡信息。
 *
 * @param[in] sdmmc SDMMC 对象句柄。
 *
 * @return SDMMC 卡句柄；未挂载时可能返回 NULL。
 */
sdmmc_card_t *wt_bsp_sdmmc_get_card(wt_bsp_sdmmc_t sdmmc);

/**
 * @brief 获取挂载点路径。
 *
 * @param[in] sdmmc SDMMC 对象句柄。
 *
 * @return 挂载点路径字符串；当 @p sdmmc 为 NULL 时返回空字符串。
 */
const char *wt_bsp_sdmmc_get_mount_point(wt_bsp_sdmmc_t sdmmc);

/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // WT_BSP_SDCARD_ENABLE_IS_ENABLED

#endif // __WT_BSP_SDMMC_H__
