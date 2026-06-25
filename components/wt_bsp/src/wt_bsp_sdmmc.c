/**
 * @file wt_bsp_sdmmc.c
 * @author cangyu (sky.kirto@qq.com)
 * @brief
 * @version 0.1
 * @date 2026-05-24
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

/* ==================== [Includes] ========================================== */

#include "wt_bsp_sdmmc.h"

#if WT_BSP_SDCARD_ENABLE_IS_ENABLED

#include <string.h>
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/* ==================== [Static Prototypes] ================================= */

/* ==================== [Static Variables] ================================== */

static const char *TAG = "wt_bsp_sdmmc";

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

esp_err_t wt_bsp_sdmmc_init(wt_bsp_sdmmc_t sdmmc, const wt_bsp_sdmmc_info_t *info)
{
    if (sdmmc == NULL || info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(sdmmc, 0, sizeof(wt_bsp_sdmmc_obj_t));
    sdmmc->info = *info;
    sdmmc->is_mounted = false;
    sdmmc->card = NULL;
    sdmmc->pwr_ctrl_handle = NULL;

#ifdef CONFIG_IDF_TARGET_ESP32P4
    // Initialize on-chip LDO for SD card power if requested
    if (sdmmc->info.use_on_chip_ldo) {
        ESP_LOGI(TAG, "Initializing on-chip LDO channel %d for SD card",
                 sdmmc->info.ldo_chan_id);
        sd_pwr_ctrl_ldo_config_t ldo_config = {
            .ldo_chan_id = sdmmc->info.ldo_chan_id,
        };
        esp_err_t ret = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &sdmmc->pwr_ctrl_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize on-chip LDO: %s", esp_err_to_name(ret));
            return ret;
        }
        ESP_LOGI(TAG, "On-chip LDO initialized successfully");
    }
#endif

    return ESP_OK;
}

esp_err_t wt_bsp_sdmmc_deinit(wt_bsp_sdmmc_t sdmmc)
{
    if (sdmmc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (sdmmc->is_mounted) {
        wt_bsp_sdmmc_unmount(sdmmc);
    }

#ifdef CONFIG_IDF_TARGET_ESP32P4
    // Release on-chip LDO power control handle
    if (sdmmc->pwr_ctrl_handle != NULL) {
        ESP_LOGI(TAG, "Releasing on-chip LDO power control");
        sd_pwr_ctrl_del_on_chip_ldo(sdmmc->pwr_ctrl_handle);
        sdmmc->pwr_ctrl_handle = NULL;
    }
#endif

    return ESP_OK;
}

esp_err_t wt_bsp_sdmmc_mount(wt_bsp_sdmmc_t sdmmc)
{
    if (sdmmc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (sdmmc->is_mounted) {
        ESP_LOGW(TAG, "SD card already mounted");
        return ESP_OK;
    }

    esp_err_t ret;

    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and
    // formatted in case when mounting fails.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    ESP_LOGI(TAG, "Using SDMMC peripheral");

    // By default, SDMMC host property has its 'slot' set to 1.
    // For ESP32-P4, we need to check if we can use other slots.
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = sdmmc->info.slot;

#ifdef CONFIG_IDF_TARGET_ESP32P4
    // Set the power control handle for SDMMC (required for ESP32-P4)
    host.pwr_ctrl_handle = sdmmc->pwr_ctrl_handle;
#endif

    // This initializes the slot without card detect or write protect signals.
    // Modify slot_config.cd and slot_config.wp if your board has these signals.
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = sdmmc->info.width;
    slot_config.cd = sdmmc->info.cd_gpio;
    slot_config.wp = sdmmc->info.wp_gpio;
    slot_config.clk = sdmmc->info.clk_gpio;
    slot_config.cmd = sdmmc->info.cmd_gpio;
    slot_config.d0 = sdmmc->info.d0_gpio;
    slot_config.d1 = sdmmc->info.d1_gpio;
    slot_config.d2 = sdmmc->info.d2_gpio;
    slot_config.d3 = sdmmc->info.d3_gpio;

    // For ESP32-P4, if we want to use specific pins, we might need to set them.
    // However, usually SDMMC pins are fixed on some SoC versions or handled by GPIO matrix.
    // In P4, SDMMC pins are typically dedicated.

    ESP_LOGI(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdmmc_mount(sdmmc->info.mount_point, &host, &slot_config, &mount_config, &sdmmc->card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                     "If you want the card to be formatted, set format_if_mount_failed = true.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return ret;
    }

    ESP_LOGI(TAG, "Filesystem mounted");
    sdmmc->is_mounted = true;

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, sdmmc->card);

    return ESP_OK;
}

esp_err_t wt_bsp_sdmmc_unmount(wt_bsp_sdmmc_t sdmmc)
{
    if (sdmmc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!sdmmc->is_mounted) {
        ESP_LOGW(TAG, "SD card not mounted");
        return ESP_OK;
    }

    esp_err_t ret = esp_vfs_fat_sdcard_unmount(sdmmc->info.mount_point, sdmmc->card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unmount filesystem (%s)", esp_err_to_name(ret));
        return ret;
    }

    sdmmc->is_mounted = false;
    sdmmc->card = NULL;
    ESP_LOGI(TAG, "Card unmounted");

    return ESP_OK;
}

sdmmc_card_t *wt_bsp_sdmmc_get_card(wt_bsp_sdmmc_t sdmmc)
{
    if (sdmmc == NULL) {
        return NULL;
    }
    return sdmmc->card;
}

const char *wt_bsp_sdmmc_get_mount_point(wt_bsp_sdmmc_t sdmmc)
{
    if (sdmmc == NULL) {
        return "";
    }
    return sdmmc->info.mount_point;
}

#endif // WT_BSP_SDCARD_ENABLE_IS_ENABLED
