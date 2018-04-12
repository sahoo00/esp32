/*
 * wifi.c
 *
 *  Created on: Apr 10, 2018
 *      Author: Debashis
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "esp_request.h"
#include "common.h"

/*
	References:
	[1] https://github.com/espressif/esp-idf/tree/master/examples/wifi/simple_wifi
	[2] https://github.com/tuanpmt/esp-request-app
	[3] https://github.com/tuanpmt/esp-request
	[4] https://github.com/espressif/esp-idf/tree/master/examples/bluetooth/gatt_server

*/

/* The examples use simple WiFi configuration that you can set via
   'make menuconfig'.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int WIFI_CONNECTED_BIT = BIT0;

static const char *TAG = "Shanvi MN Wifi";
static char wifi_ssid[32];
static char wifi_pass[64];
#define STORAGE_NAMESPACE "wifi_c"

esp_err_t store_key_value(const char * key, const char * value, int len)
{
    nvs_handle my_handle;
    esp_err_t err;

    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;

    err = nvs_set_blob(my_handle, key, value, len);
    if (err != ESP_OK) return err;

    // Commit
    err = nvs_commit(my_handle);
    if (err != ESP_OK) return err;

    // Close
    nvs_close(my_handle);
    return ESP_OK;
}

esp_err_t get_key_value(const char * key, char * value, int len)
{
    nvs_handle my_handle;
    esp_err_t err = ESP_OK;

    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;


    // Read the size of memory space required for blob
    size_t required_size = 0;  // value will default to 0, if not set yet in NVS
    err = nvs_get_blob(my_handle, key, NULL, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;
    ESP_LOGI(TAG, "size:%d", required_size);
    if (required_size > 0 && required_size < len) {
        err = nvs_get_blob(my_handle, key, value, &required_size);
        if (err != ESP_OK) return err;
    }
    else {
    	err = ESP_FAIL;
    }

    // Close
    nvs_close(my_handle);
    return ESP_OK;
}


void set_ssid(const char * ssid, int len) {
	if (len < 32) {
	    ESP_LOGI(TAG, "SSID:%s", ssid);
		strncpy(wifi_ssid, ssid, len);
		store_key_value("ssid", ssid, len);
	}
}

void set_pass(const char * pass, int len) {
	if (len < 64) {
	    ESP_LOGI(TAG, "password:%s", pass);
		strncpy(wifi_pass, pass, len);
		store_key_value("pass", pass, len);
	}
}

void connect_wifi() {
	esp_wifi_disconnect();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_ESP_WIFI_SSID,
            .password = CONFIG_ESP_WIFI_PASSWORD
        },
    };

    memcpy(wifi_config.sta.ssid, wifi_ssid, sizeof(wifi_config.sta.ssid));
    memcpy(wifi_config.sta.password, wifi_pass, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");
    ESP_LOGI(TAG, "connect to ap SSID:%s password:%s",
    		wifi_ssid, wifi_pass);

}


static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "got ip:%s",
                 ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        blinky_start();
        {uint8_t data[2] = {0x00, 0x01};
        notify_connection(data, 2);}

        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        {uint8_t data[2] = {0x00, 0x02};
        notify_connection(data, 2);}
        blinky_stop();
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

void wifi_init_sta()
{
    wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL) );

    if (get_key_value("ssid", wifi_ssid, 32) != ESP_OK) {
    	set_ssid(CONFIG_ESP_WIFI_SSID, sizeof(CONFIG_ESP_WIFI_SSID));
    }
    if (get_key_value("pass", wifi_pass, 64) != ESP_OK) {
    	set_pass(CONFIG_ESP_WIFI_PASSWORD, sizeof(CONFIG_ESP_WIFI_PASSWORD));
    }
    connect_wifi();
}

static void request_task(void *pvParameters)
{
    request_t *req;
    int status;

    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);
    ESP_LOGI(TAG, "Connected to AP, freemem=%d",esp_get_free_heap_size());
    // vTaskDelay(1000/portTICK_RATE_MS);

    req = req_new("http://shanvishield.com/safety/safety.php?go=getPrices");
    status = req_perform(req);
    print_string(req->buffer->data, req->buffer->bytes_write);
    req_clean(req);
    ESP_LOGI(TAG, "Finish request, status=%d, freemem=%d", status, esp_get_free_heap_size());
    vTaskDelete(NULL);
}

void app_wifi_main()
{
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();
    xTaskCreate(&request_task, "request_task", 8192, NULL, 5, NULL);
}

bool is_wifi_connected() {
	if( xEventGroupGetBits( wifi_event_group ) != 0 ) {
		return true;
	}
	return false;
}


