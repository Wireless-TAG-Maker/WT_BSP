#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "wt_bsp.h"

static const char *TAG = "sdmmc_example";

#define MOUNT_POINT "/sdcard"

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing BSP");
    ESP_ERROR_CHECK(wt_bsp_init());

    wt_bsp_sdmmc_t sdmmc = wt_bsp_get_sdmmc();
    if (sdmmc == NULL) {
        ESP_LOGE(TAG, "Failed to get SDMMC handle");
        return;
    }

    ESP_LOGI(TAG, "Mounting SD card");
    esp_err_t ret = wt_bsp_sdmmc_mount(sdmmc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card");
        return;
    }

    // Use POSIX and C standard library functions to work with files.

    // First create a file.
    const char *file_hello = MOUNT_POINT"/hello.txt";

    ESP_LOGI(TAG, "Opening file %s", file_hello);
    FILE *f = fopen(file_hello, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }
    fprintf(f, "Hello SDMMC!\n");
    fclose(f);
    ESP_LOGI(TAG, "File written");

    // Check if destination file exists before renaming
    struct stat st;
    if (stat(file_hello, &st) == 0) {
        // Delete it if it exists
        unlink(MOUNT_POINT"/foo.txt");
    }

    // Rename original file
    ESP_LOGI(TAG, "Renaming file %s to %s", file_hello, MOUNT_POINT"/foo.txt");
    if (rename(file_hello, MOUNT_POINT"/foo.txt") != 0) {
        ESP_LOGE(TAG, "Rename failed");
        return;
    }

    // Open renamed file for reading
    ESP_LOGI(TAG, "Reading file %s", MOUNT_POINT"/foo.txt");
    f = fopen(MOUNT_POINT"/foo.txt", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return;
    }
    char line[64];
    fgets(line, sizeof(line), f);
    fclose(f);

    // strip newline
    char *pos = strchr(line, '\n');
    if (pos) {
        *pos = '\0';
    }
    ESP_LOGI(TAG, "Read from file: '%s'", line);

    // All done, unmount partition and disable SDMMC host peripheral
    ESP_LOGI(TAG, "Unmounting SD card");
    wt_bsp_sdmmc_unmount(sdmmc);
    ESP_LOGI(TAG, "Done");
}
