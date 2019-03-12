/*
 * This file is part of the MicroPython ESP32 project, https://github.com/junhuanchen/micropython
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 junhuanchen (https://github.com/junhuanchen)
 *
 * Based on the ESP IDF example code which is Public Domain / CC0
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_smartconfig.h"

#include <stdbool.h>

#include "lib/oofatfs/ff.h"
#include "extmod/vfs_fat.h"

STATIC FATFS *lookup_path(const TCHAR **path) 
{
    mp_vfs_mount_t *fs = mp_vfs_lookup_path(*path, path);
    // printf("lookup_path %s fs %.*s\n", *path, fs->len, fs->str);
    if (fs == MP_VFS_NONE || fs == MP_VFS_ROOT) 
    {
        return NULL;
    }
    return &((fs_user_mount_t*)MP_OBJ_TO_PTR(fs->obj))->fatfs;
}

#define NETWORK_LED 18

#define SMART_CONFIG_FILE "./wifi_cfg.py"

static FATFS *vfs = NULL;

static bool wifi_config_file_init()
{
    if(NULL == vfs)
    {  
	    const char * path = "/flashbdev";
	    vfs = lookup_path(&path);
    }
	return (NULL != (vfs));
}

static wifi_config_t wifi_sta_config;

static EventGroupHandle_t wifi_event_group;

static const int ESPTOUCH_DONE_BIT = BIT0;
static const int SMARTCONFIG_DONE_BIT = BIT1;
static const char *TAG = "smartconfig";

static bool wifi_config_file_write(wifi_config_t *config)
{
    bool result = false;
    if(NULL != vfs)
    {
        FIL fp = { 0 };
        FRESULT res = f_open(vfs, &fp, SMART_CONFIG_FILE, FA_WRITE | FA_CREATE_ALWAYS);
        // printf("f_write vfs %p f_open res %d\n", vfs, res);
        if (FR_OK == res) 
        {
            // printf("f_write p %p\n", f_write);
            uint8_t mac[6];
            uint16_t bit_name = 0;
            if(ESP_OK == esp_efuse_mac_get_default(mac))
            {
                bit_name = ((uint16_t)mac[4] << 8) | mac[5];
            }
            else
            {
                ESP_LOGD(TAG, "esp_efuse_mac_get_default error! need to reset chipid.");
            }
            
            char buf[128];
            sprintf(buf, "WIFI_SSID = '%s'\nWIFI_PSWD = '%s'\nHOST_NAME = 'bit%04hx'\n", config->sta.ssid, config->sta.password, bit_name);
            
            UINT n = 0, wrtie_len = strlen(buf);
            res = f_write(&fp, (void *)buf, wrtie_len, &n);
            // printf("f_write res %d\n", res);
            if (wrtie_len == n )
            {
                // printf("f_write n %d buf %s\n", n, buf);
                result = true;
            }
            // printf("f_write f_close res %d\n", res);
            f_close(&fp);
        }
    }
    return result;
}

static void sc_callback(smartconfig_status_t status, void *pdata)
{
    static bool state_tip = true;
    gpio_set_level(NETWORK_LED, state_tip), state_tip = !state_tip;

    switch (status)
    {
    case SC_STATUS_WAIT:
        ESP_LOGD(TAG, "SC_STATUS_WAIT");
        break;
    case SC_STATUS_FIND_CHANNEL:
        ESP_LOGD(TAG, "SC_STATUS_FINDING_CHANNEL");
        break;
    case SC_STATUS_GETTING_SSID_PSWD:
        ESP_LOGD(TAG, "SC_STATUS_GETTING_SSID_PSWD");
        break;
    case SC_STATUS_LINK:
        ESP_LOGD(TAG, "SC_STATUS_LINK");
        wifi_config_t *wifi_config = pdata;
        ESP_LOGD(TAG, "SSID:%s", wifi_config->sta.ssid);
        ESP_LOGD(TAG, "PASSWORD:%s", wifi_config->sta.password);
        ESP_ERROR_CHECK(esp_wifi_disconnect());
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA , wifi_config));
        ESP_ERROR_CHECK(esp_wifi_connect());
        wifi_sta_config = *wifi_config;
        break;
    case SC_STATUS_LINK_OVER:
        ESP_LOGD(TAG, "SC_STATUS_LINK_OVER");
        if (pdata != NULL)
        {
            uint8_t phone_ip[4] = {0};
            memcpy(phone_ip, (uint8_t *)pdata, 4);
            ESP_LOGI(TAG, "Phone ip: %d.%d.%d.%d\n", phone_ip[0], phone_ip[1], phone_ip[2], phone_ip[3]);
        }
        xEventGroupSetBits(wifi_event_group, ESPTOUCH_DONE_BIT);
        break;
    default:
        break;
    }
}

void smartconfig_task(void *parm)
{
    EventBits_t uxBits;
    ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH));
    ESP_ERROR_CHECK(esp_smartconfig_start(sc_callback));
    
    uxBits = xEventGroupWaitBits(wifi_event_group, ESPTOUCH_DONE_BIT, true, false, 60000 / portTICK_PERIOD_MS);

    if (uxBits & ESPTOUCH_DONE_BIT)
    {
        ESP_LOGI(TAG, "Success");
        
        esp_smartconfig_stop();
        xEventGroupClearBits(wifi_event_group, ESPTOUCH_DONE_BIT);
        xEventGroupSetBits(wifi_event_group, SMARTCONFIG_DONE_BIT);
        vTaskDelete(NULL);
    }
    else
    {
        ESP_LOGI(TAG, "Fail will restart");
        vTaskDelay(100 / portTICK_PERIOD_MS);
        esp_restart();
    }
}

bool config_smartconfig(void)
{
    bool result = false;
    if(wifi_started) return result;
    
    wifi_event_group = xEventGroupCreate();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    result = wifi_config_file_init();
    
    ESP_LOGD(TAG, "wifi_config_file_init result:%d\n", result);
    
    ESP_ERROR_CHECK(esp_wifi_start());

    result = wifi_started = wifi_sta_connect_requested = true;
    
    gpio_set_direction(NETWORK_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(NETWORK_LED, 1);
    
    xTaskCreate(smartconfig_task, "smartconfig_task", 2048, NULL, 3, NULL);
    
    ESP_LOGD(TAG, "wait SMARTCONFIG_DONE_BIT");
    EventBits_t uxBits = xEventGroupWaitBits(wifi_event_group, SMARTCONFIG_DONE_BIT, true, false, 60000 / portTICK_PERIOD_MS);

    if (uxBits & SMARTCONFIG_DONE_BIT)
    {
        ESP_LOGD(TAG, "SMARTCONFIG_DONE_BIT");
        if (false == wifi_config_file_write(&wifi_sta_config))
        {
            ESP_LOGD(TAG, "wifi_config_file_write");
            remove(SMART_CONFIG_FILE);
            result = false;
        }
        // esp_restart();
    }
    
    vEventGroupDelete(wifi_event_group);

    gpio_reset_pin(NETWORK_LED);
    
    return result;
}

STATIC mp_obj_t esp_smartconfig() {
    return mp_obj_new_bool(config_smartconfig());
}

STATIC MP_DEFINE_CONST_FUN_OBJ_0(esp_smartconfig_obj, esp_smartconfig);
