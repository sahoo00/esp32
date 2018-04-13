/* Simple WiFi Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "common.h"

/*
	References:
	[1] https://github.com/espressif/esp-idf/tree/master/examples/wifi/simple_wifi
	[2] https://github.com/tuanpmt/esp-request-app
	[3] https://github.com/tuanpmt/esp-request
	[4] https://github.com/espressif/esp-idf/tree/master/examples/bluetooth/gatt_server

*/

static const char *TAG = "Shanvi MN";

void app_main()
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    

    ESP_LOGI(TAG, "Start Wifi");
    app_wifi_main();

    ESP_LOGI(TAG, "Start Shanvi MN");
    app_gatts_main();

    ESP_LOGI(TAG, "Start Shanvi MN Client");
    app_client_main();

    ESP_LOGI(TAG, "Start Shanvi MN Router");
    router_start();

}
