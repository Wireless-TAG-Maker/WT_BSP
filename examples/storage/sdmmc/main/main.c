#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "wt_bsp.h"

static const char *TAG = "sdmmc_example";

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing BSP");
    /* 
     * 在本 BSP 框架中，wt_bsp_init() 会自动初始化并挂载 SDMMC 文件系统。
     * 用户无需手动调用挂载接口，初始化成功后即可直接使用 POSIX 文件操作。
     */
    ESP_ERROR_CHECK(wt_bsp_init());

    // 获取 BSP 内部设置的挂载点
    const char *mount_point = wt_bsp_get_sdmmc_mount_point();
    if (strlen(mount_point) == 0) {
        ESP_LOGE(TAG, "Failed to get SDMMC mount point");
        return;
    }

    ESP_LOGI(TAG, "SD card is automatically mounted at: %s", mount_point);

    // 使用标准 C 库和 POSIX 接口操作文件
    char file_hello[128];
    snprintf(file_hello, sizeof(file_hello), "%s/hello.txt", mount_point);

    ESP_LOGI(TAG, "Creating file: %s", file_hello);
    FILE *f = fopen(file_hello, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }
    fprintf(f, "Hello SDMMC from WT_BSP!\n");
    fclose(f);
    ESP_LOGI(TAG, "File written successfully");

    // 准备重命名后的文件名
    char file_foo[128];
    snprintf(file_foo, sizeof(file_foo), "%s/foo.txt", mount_point);

    // 如果目标文件已存在，先删除它
    struct stat st;
    if (stat(file_foo, &st) == 0) {
        unlink(file_foo);
    }

    // 重命名文件
    ESP_LOGI(TAG, "Renaming %s to %s", file_hello, file_foo);
    if (rename(file_hello, file_foo) != 0) {
        ESP_LOGE(TAG, "Rename failed");
        return;
    }

    // 读取重命名后的文件
    ESP_LOGI(TAG, "Reading file: %s", file_foo);
    f = fopen(file_foo, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return;
    }
    char line[64];
    if (fgets(line, sizeof(line), f) != NULL) {
        // 移除换行符
        char *pos = strchr(line, '\n');
        if (pos) {
            *pos = '\0';
        }
        ESP_LOGI(TAG, "Read from file: '%s'", line);
    }
    fclose(f);

    /* 
     * 注意：本 BSP 在 wt_bsp_deinit() 时会自动卸载 SDMMC。
     * 在典型的嵌入式应用中，app_main 通常不会返回。
     */
    ESP_LOGI(TAG, "Example finished successfully");
}
