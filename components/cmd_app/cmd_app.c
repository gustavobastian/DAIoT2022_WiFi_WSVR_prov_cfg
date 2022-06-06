/* Commands of the app

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_sleep.h"
#include "esp_spi_flash.h"
#include "driver/rtc_io.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "nvs.h"
#include "esp_wifi.h"

#include "lwip/ip4_addr.h"
#if !IP_NAPT
#error "IP_NAPT must be defined"
#endif
#include "lwip/lwip_napt.h"

#include "app_globals.h"
#include "cmd_app.h"

#ifdef CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS
#define WITH_TASKS_INFO 1
#endif

static const char *TAG = "cmd_app";

void preprocess_string(char* str)
{
    char *p, *q;

    for (p = q = str; *p != 0; p++)
    {
        if (*(p) == '%' && *(p + 1) != 0 && *(p + 2) != 0)
        {
            // quoted hex
            uint8_t a;
            p++;
            if (*p <= '9')
                a = *p - '0';
            else
                a = toupper((unsigned char)*p) - 'A' + 10;
            a <<= 4;
            p++;
            if (*p <= '9')
                a += *p - '0';
            else
                a += toupper((unsigned char)*p) - 'A' + 10;
            *q++ = a;
        }
        else if (*(p) == '+') {
            *q++ = ' ';
        } else {
            *q++ = *p;
        }
    }
    *q = '\0';
}

esp_err_t get_config_param_str(char* name, char** param)
{
    nvs_handle_t nvs;

    esp_err_t err = nvs_open(PARAM_NAMESPACE, NVS_READONLY, &nvs);
    if (err == ESP_OK) {
        size_t len;
        if ( (err = nvs_get_str(nvs, name, NULL, &len)) == ESP_OK) {
            *param = (char *)malloc(len);
            err = nvs_get_str(nvs, name, *param, &len);
            ESP_LOGI(TAG, "%s %s", name, *param);
        } else {
            return err;
        }
        nvs_close(nvs);
    } else {
        return err;
    }
    return ESP_OK;
}

esp_err_t get_config_param_int(char* name, int* param)
{
    nvs_handle_t nvs;

    esp_err_t err = nvs_open(PARAM_NAMESPACE, NVS_READONLY, &nvs);
    if (err == ESP_OK) {
        if ( (err = nvs_get_i32(nvs, name, param)) == ESP_OK) {
            ESP_LOGI(TAG, "%s %d", name, *param);
        } else {
            return err;
        }
        nvs_close(nvs);
    } else {
        return err;
    }
    return ESP_OK;
}

esp_err_t get_config_param_blob(char* name, uint8_t* blob,  size_t blob_len)
{
    nvs_handle_t nvs;

    esp_err_t err = nvs_open(PARAM_NAMESPACE, NVS_READONLY, &nvs);
    if (err == ESP_OK) {
        size_t len;
        if ( (err = nvs_get_blob(nvs, name, NULL, &len)) == ESP_OK) {
            if (len != blob_len) {
                return ESP_ERR_NVS_INVALID_LENGTH;
            }
            err = nvs_get_blob(nvs, name, blob, &len);
            ESP_LOGI(TAG, "%s: %d", name, len);
        } else {
            return err;
        }
        nvs_close(nvs);
    } else {
        return err;
    }
    return ESP_OK;
}


/* 'set_sta' command */
int set_sta(int argc, char **argv)
{
    esp_err_t err;
    nvs_handle_t nvs;

    ESP_LOGI(TAG, "Opening NVS_READWRITE");
    err = nvs_open(PARAM_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        return err;
    }

    ESP_LOGI(TAG, "NVS Writing SSID & PASSWS");
    err = nvs_set_str(nvs, "ssid", argv[1]);
    if (err == ESP_OK) {
        err = nvs_set_str(nvs, "passwd", argv[2]);
        if (err == ESP_OK) {
            err = nvs_commit(nvs);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "STA settings %s/%s stored.", argv[1], argv[2]);
            }
        }
    }

    ESP_LOGI(TAG, "Closing NVS_READWRITE");   
    nvs_close(nvs);
    return err;
}










