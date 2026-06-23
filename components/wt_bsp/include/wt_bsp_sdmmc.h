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
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "sd_pwr_ctrl.h"

#if WT_BSP_SDCARD_ENABLE_IS_ENABLED

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/**
 * @brief SDMMC 对象句柄。
 */
typedef struct wt_bsp_sdmmc_obj_t *wt_bsp_sdmmc_t;

/**
 * @brief SDMMC 硬件配置。
 */
typedef struct {
    /**
     * @brief 挂载点路径（如 "/sdcard"）。
     */
    const char *mount_point;

    /**
     * @brief SDMMC 插槽索引。
     */
    uint8_t slot;

    /**
     * @brief 总线宽度。
     */
    uint8_t width;

    /**
     * @brief 卡检测 GPIO 编号（如果不需要则设为 -1）。
     */
    int cd_gpio;

    /**
     * @brief 写保护 GPIO 编号（如果不需要则设为 -1）。
     */
    int wp_gpio;

    /**
     * @brief 时钟信号 GPIO 编号。
     */
    int clk_gpio;

    /**
     * @brief 命令信号 GPIO 编号。
     */
    int cmd_gpio;

    /**
     * @brief 数据 0 信号 GPIO 编号。
     */
    int d0_gpio;

    /**
     * @brief 数据 1 信号 GPIO 编号。
     */
    int d1_gpio;

    /**
     * @brief 数据 2 信号 GPIO 编号。
     */
    int d2_gpio;

    /**
     * @brief 数据 3 信号 GPIO 编号。
     */
    int d3_gpio;

    /**
     * @brief 是否启用片上 LDO 为 SD 卡供电（仅 ESP32-P4 需要）。
     */
    bool use_on_chip_ldo;

    /**
     * @brief 片上 LDO 通道 ID（如 VO4 则设为 4）。
     */
    int ldo_chan_id;

    /**
     * @brief LDO 输出电压（单位 mV，IDF v5.5.4 暂不支持）。
     */
    int ldo_voltage_mv;
} wt_bsp_sdmmc_info_t;

/**
 * @brief SDMMC 对象。
 */
typedef struct wt_bsp_sdmmc_obj_t {
    /**
     * @brief SDMMC 硬件配置。
     */
    wt_bsp_sdmmc_info_t info;

    /**
     * @brief SDMMC 卡句柄。
     */
    sdmmc_card_t *card;

    /**
     * @brief 是否已挂载文件系统。
     */
    bool is_mounted;

    /**
     * @brief SD 电源控制句柄（用于 ESP32-P4 片上 LDO）。
     */
    sd_pwr_ctrl_handle_t pwr_ctrl_handle;
} wt_bsp_sdmmc_obj_t;

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief 初始化 SDMMC 对象。
 *
 * @param[in,out] sdmmc 待初始化的 SDMMC 对象。
 * @param[in] info SDMMC 硬件配置。
 *
 * @return 成功时返回 ESP_OK。
 */
esp_err_t wt_bsp_sdmmc_init(wt_bsp_sdmmc_t sdmmc, const wt_bsp_sdmmc_info_t *info);

/**
 * @brief 反初始化 SDMMC 对象。
 *
 * @param[in,out] sdmmc 待反初始化的 SDMMC 对象。
 *
 * @return 成功时返回 ESP_OK。
 */
esp_err_t wt_bsp_sdmmc_deinit(wt_bsp_sdmmc_t sdmmc);

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
