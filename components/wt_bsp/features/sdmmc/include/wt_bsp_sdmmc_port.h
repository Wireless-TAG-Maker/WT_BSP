/**
 * @file wt_bsp_sdmmc_port.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief Wireless-Tag BSP SDMMC port 接口。
 * @version 0.1
 * @date 2026-06-26
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

#ifndef __WT_BSP_SDMMC_PORT_H__
#define __WT_BSP_SDMMC_PORT_H__

/* ==================== [Includes] ========================================== */

#include "wt_bsp_sdmmc.h"

#if WT_BSP_SDMMC_ENABLED

#include <stdbool.h>
#include <stdint.h>
#include "sd_pwr_ctrl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/**
 * @brief SDMMC 硬件配置。
 */
typedef struct {
    const char *mount_point;     /*!< 挂载点路径（如 "/sdcard"）。 */
    uint8_t slot;                /*!< SDMMC 插槽索引。 */
    uint8_t width;               /*!< 总线宽度。 */
    int cd_gpio;                 /*!< 卡检测 GPIO 编号（如果不需要则设为 -1）。 */
    int wp_gpio;                 /*!< 写保护 GPIO 编号（如果不需要则设为 -1）。 */
    int clk_gpio;                /*!< 时钟信号 GPIO 编号。 */
    int cmd_gpio;                /*!< 命令信号 GPIO 编号。 */
    int d0_gpio;                 /*!< 数据 0 信号 GPIO 编号。 */
    int d1_gpio;                 /*!< 数据 1 信号 GPIO 编号。 */
    int d2_gpio;                 /*!< 数据 2 信号 GPIO 编号。 */
    int d3_gpio;                 /*!< 数据 3 信号 GPIO 编号。 */
    bool use_sdspi;              /*!< 是否通过 SPI 外设访问 SD 卡。 */
    int spi_host;                /*!< SDSPI 使用的 SPI host。 */
    bool use_on_chip_ldo;        /*!< 是否启用片上 LDO 为 SD 卡供电（仅 ESP32-P4 需要）。 */
    int ldo_chan_id;             /*!< 片上 LDO 通道 ID（如 VO4 则设为 4）。 */
    int ldo_voltage_mv;          /*!< LDO 输出电压（单位 mV，IDF v5.5.4 暂不支持）。 */
} wt_bsp_sdmmc_info_t;

/**
 * @brief SDMMC 对象。
 */
typedef struct wt_bsp_sdmmc_obj_t {
    wt_bsp_sdmmc_info_t info;              /*!< SDMMC 硬件配置。 */
    sdmmc_card_t *card;                    /*!< SDMMC 卡句柄。 */
    bool is_initialized;                   /*!< 是否已初始化 SDMMC 对象。 */
    bool is_mounted;                       /*!< 是否已挂载文件系统。 */
    bool spi_bus_initialized;              /*!< SDSPI 总线是否已初始化。 */
    sd_pwr_ctrl_handle_t pwr_ctrl_handle;  /*!< SD 电源控制句柄（用于 ESP32-P4 片上 LDO）。 */
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

/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // WT_BSP_SDMMC_ENABLED

#endif // __WT_BSP_SDMMC_PORT_H__
