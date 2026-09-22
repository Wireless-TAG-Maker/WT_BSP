/**
 * @file factory_console.c
 * @author Wireless-Tag
 * @brief Read-only factory diagnostics over the composite CDC interface.
 * @version 0.1
 * @date 2026-09-20
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 */

/* ==================== [Includes] ========================================== */

#include "factory_console.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "freertos/task.h"

/* ==================== [Defines] =========================================== */
#define CONSOLE_WRITE_TIMEOUT_MS 2000

/* ==================== [Typedefs] ========================================== */
/* ==================== [Static Prototypes] ================================= */

static void console_task(void *argument);
static bool console_write(const void *data, size_t length);
static bool console_text(const char *text);
static void console_command(const char *command);
static bool recorded_filename(const char *name);

/* ==================== [Static Variables] ================================== */

static wt_bsp_usb_device_cdc_t s_cdc;
static QueueHandle_t s_status;
static char s_directory[FACTORY_PATH_SIZE];

/* ==================== [Macros] ============================================ */
/* ==================== [Global Functions] ================================== */

esp_err_t factory_console_start(wt_bsp_usb_device_cdc_t cdc, const char *directory,
                                QueueHandle_t status)
{
    if (cdc == NULL || directory == NULL || status == NULL || strlen(directory) >= sizeof(s_directory)) {
        return ESP_ERR_INVALID_ARG;
    }
    s_cdc = cdc;
    s_status = status;
    strcpy(s_directory, directory);
    return xTaskCreate(console_task, "factory_cdc", 6144, NULL, 4, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

/* ==================== [Static Functions] ================================== */

static bool console_write(const void *data, size_t length)
{
    const uint8_t *bytes = data;
    size_t offset = 0;
    TickType_t started = xTaskGetTickCount();
    while (offset < length && wt_bsp_usb_device_cdc_is_connected(s_cdc)) {
        size_t written = 0;
        esp_err_t ret = wt_bsp_usb_device_cdc_write(s_cdc, bytes + offset, length - offset, &written, 0);
        offset += written;
        if (ret != ESP_OK && ret != ESP_ERR_NOT_FINISHED) {
            return false;
        }
        if (xTaskGetTickCount() - started >= pdMS_TO_TICKS(CONSOLE_WRITE_TIMEOUT_MS)) {
            return false;
        }
        if (offset < length) {
            vTaskDelay(1);
        }
    }
    return offset == length;
}

static bool console_text(const char *text)
{
    return console_write(text, strlen(text));
}

static bool recorded_filename(const char *name)
{
    if (strlen(name) != 14 || strncmp(name, "rec_", 4) != 0 || strcmp(name + 10, ".avi") != 0) {
        return false;
    }
    for (size_t i = 4; i < 10; i++) {
        if (!isdigit((unsigned char)name[i])) {
            return false;
        }
    }
    return true;
}

static void console_command(const char *command)
{
    factory_status_t status = {0};
    if (xQueuePeek(s_status, &status, 0) != pdTRUE) {
        console_text("ERR not ready\n");
        return;
    }
    char response[384];
    if (strcmp(command, "ping") == 0) {
        console_text("pong\n");
    } else if (strcmp(command, "status") == 0) {
        snprintf(response, sizeof(response),
                 "{\"mode\":\"%s\",\"recording_enabled\":%s,\"paused\":%s,\"camera_ok\":%s,\"sd_ok\":%s,"
                 "\"recorded_frames\":%" PRIu64 ",\"segments\":%" PRIu32 ",\"file\":\"%s\","
                 "\"free_internal\":%" PRIu32 ",\"free_psram\":%" PRIu32 "}\n",
                 status.usb ? "usb" : (!status.recording_enabled ? "monitor" :
                                      (status.paused ? "paused" : "recording")),
                 status.recording_enabled ? "true" : "false",
                 status.paused ? "true" : "false", status.camera_ok ? "true" : "false",
                 status.sd_ok ? "true" : "false", status.recorded, status.segments,
                 status.file, status.free_internal, status.free_psram);
        console_text(response);
    } else if (!status.usb || !status.sd_ok) {
        console_text("ERR storage unavailable or recording active\n");
    } else if (strcmp(command, "list") == 0) {
        DIR *directory = opendir(s_directory);
        if (directory == NULL) {
            /* With recording disabled, a fresh card has no recordings directory. */
            console_text(errno == ENOENT ? "END\n" : "ERR directory\n");
            return;
        }
        struct dirent *entry;
        while ((entry = readdir(directory)) != NULL) {
            if (!recorded_filename(entry->d_name)) {
                continue;
            }
            char path[FACTORY_PATH_SIZE + 32];
            snprintf(path, sizeof(path), "%s/%.14s", s_directory, entry->d_name);
            struct stat info;
            if (stat(path, &info) == 0) {
                snprintf(response, sizeof(response), "FILE %s %ld\n", entry->d_name, (long)info.st_size);
                if (!console_text(response)) {
                    break;
                }
            }
        }
        closedir(directory);
        console_text("END\n");
    } else if (strncmp(command, "get ", 4) == 0 && recorded_filename(command + 4)) {
        char path[FACTORY_PATH_SIZE + 32];
        snprintf(path, sizeof(path), "%s/%s", s_directory, command + 4);
        struct stat info;
        if (stat(path, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size <= 0) {
            console_text("ERR file\n");
            return;
        }
        FILE *file = fopen(path, "rb");
        if (file == NULL) {
            console_text("ERR open\n");
            return;
        }
        snprintf(response, sizeof(response), "DATA %ld\n", (long)info.st_size);
        bool complete = console_text(response);
        uint8_t buffer[1024];
        size_t remaining = info.st_size;
        while (complete && remaining > 0) {
            size_t count = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
            complete = fread(buffer, 1, count, file) == count && console_write(buffer, count);
            remaining -= count;
        }
        fclose(file);
        if (complete) {
            console_text("\nEND\n");
        }
    } else {
        console_text("ERR commands: status, ping, list, get rec_000001.avi\n");
    }
}

static void console_task(void *argument)
{
    (void)argument;
    char command[80];
    size_t used = 0;
    bool overflow = false;
    for (;;) {
        if (!wt_bsp_usb_device_cdc_is_connected(s_cdc)) {
            used = 0;
            overflow = false;
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        uint8_t bytes[64];
        size_t count = 0;
        if (wt_bsp_usb_device_cdc_read(s_cdc, bytes, sizeof(bytes), &count) == ESP_OK) {
            for (size_t i = 0; i < count; i++) {
                if (bytes[i] == '\n') {
                    command[used] = '\0';
                    if (overflow) {
                        console_text("ERR command too long\n");
                    } else if (used > 0) {
                        console_command(command);
                    }
                    used = 0;
                    overflow = false;
                } else if (bytes[i] != '\r' && !overflow) {
                    if (used + 1 < sizeof(command)) {
                        command[used++] = (char)bytes[i];
                    } else {
                        overflow = true;
                    }
                }
            }
        }
        /* A 5 ms delay rounds to zero with the default 100 Hz tick. Always
         * block for a tick so an open, idle CDC port cannot starve IDLE0. */
        vTaskDelay(1);
    }
}
