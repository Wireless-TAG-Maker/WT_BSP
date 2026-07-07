/**
 * @file board.c
 * @author YobieZhou
 * @brief
 * @version 0.1
 * @date 2026-06-09
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

/* ==================== [Includes] ========================================== */

#include "board.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#if WT_BSP_SDMMC_ENABLED
#include "driver/spi_master.h"
#endif
#if WT_BSP_DSI_ENABLED || WT_BSP_CSI_ENABLED || WT_BSP_TOUCH_ENABLED
#include "driver/i2c_master.h"
#endif

/* ==================== [Defines] =========================================== */

#define BOARD_VERSION_MAJOR 0
#define BOARD_VERSION_MINOR 1

#define BOARD_I2C_FEATURE_ENABLED (WT_BSP_DSI_ENABLED || WT_BSP_CSI_ENABLED || WT_BSP_TOUCH_ENABLED)

#define BOARD_NAME "WT9932P4C61-TINY"

#define BOARD_BUTTON_GPIO_NUM 35
#define BOARD_BUTTON_ACTIVE_LEVEL WT_BSP_BUTTON_ACTIVE_LOW

#define BOARD_RGB_GPIO_NUM 51
#define BOARD_RGB_MODEL WT_BSP_RGB_MODEL_WS2812
#define BOARD_RGB_FORMAT WT_BSP_RGB_FORMAT_GRB
#define BOARD_RGB_INVERT_OUT false

#define BOARD_SDMMC_MOUNT_POINT "/sdcard"
#define BOARD_SDMMC_SLOT 0
#define BOARD_SDMMC_WIDTH 4
#define BOARD_SDMMC_CD_GPIO -1
#define BOARD_SDMMC_WP_GPIO -1
#define BOARD_SDMMC_CLK_GPIO 43
#define BOARD_SDMMC_CMD_GPIO 44
#define BOARD_SDMMC_D0_GPIO 39
#define BOARD_SDMMC_D1_GPIO 40
#define BOARD_SDMMC_D2_GPIO 41
#define BOARD_SDMMC_D3_GPIO 42
#if defined(CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE) && CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE
#define BOARD_SDMMC_USE_SDSPI 1
#else
#define BOARD_SDMMC_USE_SDSPI 0
#endif
#define BOARD_SDMMC_SPI_HOST SPI2_HOST

#define BOARD_SDMMC_USE_ON_CHIP_LDO 1
#define BOARD_SDMMC_LDO_CHAN_ID 4
#define BOARD_SDMMC_LDO_VOLTAGE_MV 3300

#define BOARD_DSI_BACKLIGHT_GPIO_NUM 26
#define BOARD_DSI_RESET_GPIO_NUM 27
#define BOARD_DSI_WIDTH 480
#define BOARD_DSI_HEIGHT 640
#define BOARD_DSI_PANEL_TYPE WT_BSP_DSI_PANEL_ST7102_480_640
#define BOARD_DSI_LANE_NUM 2
#define BOARD_DSI_COLOR_FORMAT WT_BSP_DSI_COLOR_FORMAT_RGB888
#define BOARD_DSI_DPI_FB_NUM 2
#define BOARD_DSI_LANE_BITRATE_MBPS 1000
#define BOARD_DSI_PHY_LDO_CHANNEL 3
#define BOARD_DSI_PHY_LDO_VOLTAGE_MV 2500
#define BOARD_DSI_LEDC_CHANNEL 1
#define BOARD_DSI_LEDC_TIMER 1

#define BOARD_CSI_SCCB_SCL_PIN 8
#define BOARD_CSI_SCCB_SDA_PIN 7
#define BOARD_CSI_RESET_PIN -1
#define BOARD_CSI_PWDN_PIN -1
#define BOARD_CSI_WIDTH 1024
#define BOARD_CSI_HEIGHT 600
#define BOARD_CSI_FPS 30
#define BOARD_CSI_BUF_COUNT 3

#define BOARD_I2C_SCAN_TIMEOUT_MS 200

/* I2C 设备地址定义 */
#define BOARD_I2C_ADDR_DISPLAY  0x28
#define BOARD_I2C_ADDR_TOUCH    0x55
#define BOARD_I2C_ADDR_CAMERA   0x30

#define BOARD_TOUCH_I2C_PORT 0
#define BOARD_TOUCH_I2C_SDA_PIN 7
#define BOARD_TOUCH_I2C_SCL_PIN 8
#define BOARD_TOUCH_RST_PIN -1
#define BOARD_TOUCH_INT_PIN -1

/* ==================== [Typedefs] ========================================== */

/**
 * @brief I2C 设备检测状态
 */
#if BOARD_I2C_FEATURE_ENABLED
typedef struct {
    bool display_detected;
    bool touch_detected;
    bool camera_detected;
} board_i2c_device_status_t;
#endif

/* ==================== [Static Prototypes] ================================= */

static esp_err_t board_init(void);
static esp_err_t board_deinit(void);
static wt_bsp_board_t board_get_board(void);
static wt_bsp_button_t board_get_button(void);
static wt_bsp_rgb_t board_get_rgb(void);
static wt_bsp_sdmmc_t board_get_sdmmc(void);
static wt_bsp_dsi_t board_get_dsi(void);
static wt_bsp_csi_t board_get_csi(void);
static wt_bsp_touch_t board_get_touch(void);
#if BOARD_I2C_FEATURE_ENABLED
static esp_err_t board_i2c_scan_devices(i2c_master_bus_handle_t bus_handle, board_i2c_device_status_t *status);
#endif

/* ==================== [Static Variables] ================================== */

static const char *TAG = "board";

static bool s_board_is_init = false;
#if BOARD_I2C_FEATURE_ENABLED
static i2c_master_bus_handle_t s_shared_i2c_bus = NULL;
#endif

static wt_bsp_interface_t s_bsp_interface = {
    .init = board_init,
    .deinit = board_deinit,
    .get_board = board_get_board,
    .get_button = board_get_button,
    .get_rgb = board_get_rgb,
    .get_sdmmc = board_get_sdmmc,
    .get_dsi = board_get_dsi,
    .get_csi = board_get_csi,
    .get_touch = board_get_touch,
};

static wt_bsp_board_obj_t s_bsp_board = {0};
#if WT_BSP_BUTTON_ENABLED
static wt_bsp_button_obj_t s_bsp_button = {0};
#endif
#if WT_BSP_RGB_ENABLED
static wt_bsp_rgb_obj_t s_bsp_rgb = {0};
#endif
#if WT_BSP_SDMMC_ENABLED
static wt_bsp_sdmmc_obj_t s_bsp_sdmmc = {0};
#endif
#if WT_BSP_DSI_ENABLED
static wt_bsp_dsi_obj_t s_bsp_dsi = {0};
#endif
#if WT_BSP_CSI_ENABLED
static wt_bsp_csi_obj_t s_bsp_csi = {0};
#endif
#if WT_BSP_TOUCH_ENABLED
static wt_bsp_touch_obj_t s_bsp_touch = {0};
#endif

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

wt_bsp_interface_t *board_get_bsp_interface(void)
{
    return &s_bsp_interface;
}

/* ==================== [Static Functions] ================================== */

/**
 * @brief 扫描 I2C 总线上的指定设备
 * @param bus_handle I2C 总线句柄
 * @param status 输出检测到的设备状态
 * @return ESP_OK 或 ESP_ERR_NOT_FOUND（如果没有检测到任何设备）
 */
#if BOARD_I2C_FEATURE_ENABLED
static esp_err_t board_i2c_scan_devices(i2c_master_bus_handle_t bus_handle, board_i2c_device_status_t *status)
{
    if (status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(status, 0, sizeof(board_i2c_device_status_t));
    bool any_device_found = false;

    ESP_LOGI(TAG, "Scanning I2C bus for devices...");

    // 检测屏幕 (0x28)
    esp_err_t ret = i2c_master_probe(bus_handle, BOARD_I2C_ADDR_DISPLAY, BOARD_I2C_SCAN_TIMEOUT_MS);
    if (ret == ESP_OK) {
        status->display_detected = true;
        any_device_found = true;
        ESP_LOGI(TAG, "Found display device at address: 0x%02x", BOARD_I2C_ADDR_DISPLAY);
    } else {
        ESP_LOGW(TAG, "Display device not found at address: 0x%02x", BOARD_I2C_ADDR_DISPLAY);
    }

    // 检测触摸屏 (0x55)
    ret = i2c_master_probe(bus_handle, BOARD_I2C_ADDR_TOUCH, BOARD_I2C_SCAN_TIMEOUT_MS);
    if (ret == ESP_OK) {
        status->touch_detected = true;
        any_device_found = true;
        ESP_LOGI(TAG, "Found touch device at address: 0x%02x", BOARD_I2C_ADDR_TOUCH);
    } else {
        ESP_LOGW(TAG, "Touch device not found at address: 0x%02x", BOARD_I2C_ADDR_TOUCH);
    }

    // 检测摄像头 (0x30)
    ret = i2c_master_probe(bus_handle, BOARD_I2C_ADDR_CAMERA, BOARD_I2C_SCAN_TIMEOUT_MS);
    if (ret == ESP_OK) {
        status->camera_detected = true;
        any_device_found = true;
        ESP_LOGI(TAG, "Found camera device at address: 0x%02x", BOARD_I2C_ADDR_CAMERA);
    } else {
        ESP_LOGW(TAG, "Camera device not found at address: 0x%02x", BOARD_I2C_ADDR_CAMERA);
    }

    ESP_LOGI(TAG, "I2C scan complete. Display: %s, Touch: %s, Camera: %s",
             status->display_detected ? "YES" : "NO",
             status->touch_detected ? "YES" : "NO",
             status->camera_detected ? "YES" : "NO");

    return any_device_found ? ESP_OK : ESP_ERR_NOT_FOUND;
}
#endif

static esp_err_t board_init(void)
{
    esp_err_t ret = ESP_OK;

    if (s_board_is_init) {
        ESP_LOGW(TAG, "Board is already initialized");
        return ESP_OK;
    }

    // Initialize board
    wt_bsp_board_info_t board_info = {
        .name = BOARD_NAME,
        .version_major = BOARD_VERSION_MAJOR,
        .version_minor = BOARD_VERSION_MINOR,
    };

    ret = wt_bsp_board_init(&s_bsp_board, &board_info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize board: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize button
#if WT_BSP_BUTTON_ENABLED
    ret = wt_bsp_button_init(&s_bsp_button, &(wt_bsp_button_info_t) {
        .gpio_num = BOARD_BUTTON_GPIO_NUM,
        .active_level = BOARD_BUTTON_ACTIVE_LEVEL,
    });
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize button: %s", esp_err_to_name(ret));
        return ret;
    }
#endif

    // Initialize RGB
#if WT_BSP_RGB_ENABLED
    ret = wt_bsp_rgb_init(&s_bsp_rgb, &(wt_bsp_rgb_info_t) {
        .gpio_num = BOARD_RGB_GPIO_NUM,
        .model = BOARD_RGB_MODEL,
        .format = BOARD_RGB_FORMAT,
        .invert_out = BOARD_RGB_INVERT_OUT,
    });
    if (ret != ESP_OK) {
#if WT_BSP_BUTTON_ENABLED
        wt_bsp_button_deinit(&s_bsp_button);
#endif
        ESP_LOGE(TAG, "Failed to initialize RGB: %s", esp_err_to_name(ret));
        return ret;
    }
#endif

#if WT_BSP_SDMMC_ENABLED
    // Initialize SDMMC
    ret = wt_bsp_sdmmc_init(&s_bsp_sdmmc, &(wt_bsp_sdmmc_info_t) {
        .mount_point = BOARD_SDMMC_MOUNT_POINT,
        .slot = BOARD_SDMMC_SLOT,
        .width = BOARD_SDMMC_WIDTH,
        .cd_gpio = BOARD_SDMMC_CD_GPIO,
        .wp_gpio = BOARD_SDMMC_WP_GPIO,
        .clk_gpio = BOARD_SDMMC_CLK_GPIO,
        .cmd_gpio = BOARD_SDMMC_CMD_GPIO,
        .d0_gpio = BOARD_SDMMC_D0_GPIO,
        .d1_gpio = BOARD_SDMMC_D1_GPIO,
        .d2_gpio = BOARD_SDMMC_D2_GPIO,
        .d3_gpio = BOARD_SDMMC_D3_GPIO,
        .use_sdspi = BOARD_SDMMC_USE_SDSPI,
        .spi_host = BOARD_SDMMC_SPI_HOST,
        .use_on_chip_ldo = 0,  // WT9932P4C61-TINY does not use VO4
        .ldo_chan_id = -1,
        .ldo_voltage_mv = BOARD_SDMMC_LDO_VOLTAGE_MV,
    });
    if (ret != ESP_OK) {
#if WT_BSP_RGB_ENABLED
        wt_bsp_rgb_deinit(&s_bsp_rgb);
#endif
#if WT_BSP_BUTTON_ENABLED
        wt_bsp_button_deinit(&s_bsp_button);
#endif
        ESP_LOGE(TAG, "Failed to initialize SDMMC: %s", esp_err_to_name(ret));
        return ret;
    }

#endif
#if BOARD_I2C_FEATURE_ENABLED
    // 硬件限制：IO0 连接到了摄像头的 PWDN/LDO/RESET 引脚。
    // 正常使用摄像头前，必须将 IO0 拉高以启用摄像头供电及解除复位。
    // 在 I2C 扫描前需要先开启摄像头电源，确保摄像头能被检测到。
    gpio_config_t csi_pwr_conf = {
        .pin_bit_mask = (1ULL << 0),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&csi_pwr_conf);
    gpio_set_level(0, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Create shared I2C bus for Touch and CSI
    i2c_master_bus_config_t i2c_config = {
        .i2c_port = BOARD_TOUCH_I2C_PORT,
        .sda_io_num = BOARD_TOUCH_I2C_SDA_PIN,
        .scl_io_num = BOARD_TOUCH_I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .flags.enable_internal_pullup = true,
    };
    ret = i2c_new_master_bus(&i2c_config, &s_shared_i2c_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize shared I2C bus: %s", esp_err_to_name(ret));
        s_shared_i2c_bus = NULL;
#if WT_BSP_SDMMC_ENABLED
        wt_bsp_sdmmc_deinit(&s_bsp_sdmmc);
#endif
#if WT_BSP_RGB_ENABLED
        wt_bsp_rgb_deinit(&s_bsp_rgb);
#endif
#if WT_BSP_BUTTON_ENABLED
        wt_bsp_button_deinit(&s_bsp_button);
#endif
        return ESP_ERR_NOT_FOUND;
    }

    // Scan I2C bus for expected devices
    board_i2c_device_status_t device_status = {0};
    ret = board_i2c_scan_devices(s_shared_i2c_bus, &device_status);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C device detection failed: no devices found");
        i2c_del_master_bus(s_shared_i2c_bus);
        s_shared_i2c_bus = NULL;
        // Keep RGB, Button, and SDMMC for status indication
        s_board_is_init = true;  // Mark as init to allow deinit later
        return ESP_ERR_NOT_FOUND;
    }

#if WT_BSP_DSI_ENABLED
    // Initialize DSI only if display device was detected
    if (device_status.display_detected) {
        ret = wt_bsp_dsi_init(&s_bsp_dsi, &(wt_bsp_dsi_info_t) {
            .backlight_gpio_num = BOARD_DSI_BACKLIGHT_GPIO_NUM,
            .reset_gpio_num = BOARD_DSI_RESET_GPIO_NUM,
            .width = BOARD_DSI_WIDTH,
            .height = BOARD_DSI_HEIGHT,
            .dsi_lane_num = BOARD_DSI_LANE_NUM,
            .panel_type = BOARD_DSI_PANEL_TYPE,
            .color_format = BOARD_DSI_COLOR_FORMAT,
            .dpi_frame_buffer_num = BOARD_DSI_DPI_FB_NUM,
            .lane_bit_rate_mbps = BOARD_DSI_LANE_BITRATE_MBPS,
            .phy_ldo_channel = BOARD_DSI_PHY_LDO_CHANNEL,
            .phy_ldo_voltage_mv = BOARD_DSI_PHY_LDO_VOLTAGE_MV,
            .ledc_channel = BOARD_DSI_LEDC_CHANNEL,
            .ledc_timer = BOARD_DSI_LEDC_TIMER,
        });
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "DSI initialization failed: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "DSI initialized successfully");
        }
    } else {
        ESP_LOGW(TAG, "Display device not detected, skipping DSI initialization");
    }

#endif
#if WT_BSP_CSI_ENABLED
    // Initialize CSI only if camera device was detected
    if (device_status.camera_detected) {
        ret = wt_bsp_csi_init(&s_bsp_csi, &(wt_bsp_csi_info_t) {
            .i2c_bus_handle = s_shared_i2c_bus,
            .sccb_scl_pin = BOARD_CSI_SCCB_SCL_PIN,
            .sccb_sda_pin = BOARD_CSI_SCCB_SDA_PIN,
            .reset_pin = BOARD_CSI_RESET_PIN,
            .pwdn_pin = BOARD_CSI_PWDN_PIN,
            .width = BOARD_CSI_WIDTH,
            .height = BOARD_CSI_HEIGHT,
            .fps = BOARD_CSI_FPS,
            .buffer_count = BOARD_CSI_BUF_COUNT,
        });
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "CSI initialization failed: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "CSI initialized successfully");
        }
    } else {
        ESP_LOGW(TAG, "Camera device not detected, skipping CSI initialization");
    }

#endif
#if WT_BSP_TOUCH_ENABLED
    // Initialize touch only if touch device was detected
    if (device_status.touch_detected) {
        ret = wt_bsp_touch_init(&s_bsp_touch, &(wt_bsp_touch_info_t) {
            .i2c_bus_handle = s_shared_i2c_bus,
            .i2c_port = BOARD_TOUCH_I2C_PORT,
            .scl_pin = BOARD_TOUCH_I2C_SCL_PIN,
            .sda_pin = BOARD_TOUCH_I2C_SDA_PIN,
            .rst_pin = BOARD_TOUCH_RST_PIN,
            .int_pin = BOARD_TOUCH_INT_PIN,
            .width = BOARD_DSI_WIDTH,
            .height = BOARD_DSI_HEIGHT,
        });
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Touch initialization failed: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "Touch initialized successfully");
        }
    } else {
        ESP_LOGW(TAG, "Touch device not detected, skipping touch initialization");
    }

#endif
#endif
    s_board_is_init = true;

    return ESP_OK;
}

static esp_err_t board_deinit(void)
{
    esp_err_t ret = ESP_OK;
    if (!s_board_is_init) {
        ESP_LOGW(TAG, "Board is not initialized");
        return ESP_OK;
    }

#if WT_BSP_TOUCH_ENABLED
    // Deinitialize touch
    ret = wt_bsp_touch_deinit(&s_bsp_touch);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deinitialize touch: %s", esp_err_to_name(ret));
    }
#endif

#if WT_BSP_CSI_ENABLED
    // Deinitialize CSI
    ret = wt_bsp_csi_deinit(&s_bsp_csi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deinitialize CSI: %s", esp_err_to_name(ret));
    }
#endif

#if WT_BSP_DSI_ENABLED
    // Deinitialize DSI
    ret = wt_bsp_dsi_deinit(&s_bsp_dsi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deinitialize DSI: %s", esp_err_to_name(ret));
    }
#endif

#if BOARD_I2C_FEATURE_ENABLED
    // Delete I2C bus
    if (s_shared_i2c_bus != NULL) {
        ret = i2c_del_master_bus(s_shared_i2c_bus);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to delete I2C bus: %s", esp_err_to_name(ret));
        }
        s_shared_i2c_bus = NULL;
    }
#endif

#if WT_BSP_SDMMC_ENABLED
    // Deinitialize SDMMC
    ret = wt_bsp_sdmmc_deinit(&s_bsp_sdmmc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deinitialize SDMMC: %s", esp_err_to_name(ret));
    }
#endif

    // Deinitialize RGB
#if WT_BSP_RGB_ENABLED
    ret = wt_bsp_rgb_deinit(&s_bsp_rgb);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deinitialize RGB: %s", esp_err_to_name(ret));
    }
#endif

    // Deinitialize button

#if WT_BSP_BUTTON_ENABLED
    ret = wt_bsp_button_deinit(&s_bsp_button);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deinitialize button: %s", esp_err_to_name(ret));
    }
#endif

    s_board_is_init = false;

    return ESP_OK;
}

static wt_bsp_board_t board_get_board(void)
{
    return &s_bsp_board;
}

static wt_bsp_button_t board_get_button(void)
{
#if WT_BSP_BUTTON_ENABLED
    return &s_bsp_button;
#else
    return NULL;
#endif
}

static wt_bsp_rgb_t board_get_rgb(void)
{
#if WT_BSP_RGB_ENABLED
    return &s_bsp_rgb;
#else
    return NULL;
#endif
}

static wt_bsp_sdmmc_t board_get_sdmmc(void)
{
#if WT_BSP_SDMMC_ENABLED
    return s_bsp_sdmmc.is_initialized ? &s_bsp_sdmmc : NULL;
#else
    return NULL;
#endif
}

static wt_bsp_dsi_t board_get_dsi(void)
{
#if WT_BSP_DSI_ENABLED
    return s_bsp_dsi.is_initialized ? &s_bsp_dsi : NULL;
#else
    return NULL;
#endif
}

static wt_bsp_csi_t board_get_csi(void)
{
#if WT_BSP_CSI_ENABLED
    return s_bsp_csi.is_initialized ? &s_bsp_csi : NULL;
#else
    return NULL;
#endif
}

static wt_bsp_touch_t board_get_touch(void)
{
#if WT_BSP_TOUCH_ENABLED
    return s_bsp_touch.is_initialized ? &s_bsp_touch : NULL;
#else
    return NULL;
#endif
}
