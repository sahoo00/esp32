/*
 * client.c
 *
 *  Created on: Apr 12, 2018
 *      Author: Debashis
 */


/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/



/****************************************************************************
*
* This file is for gatt client. It can scan ble device, connect multiple devices,
* The gattc_multi_connect demo can connect three ble slaves at the same time.
* Modify the name of gatt_server demo named ESP_GATTS_DEMO_a,
* ESP_GATTS_DEMO_b and ESP_GATTS_DEMO_c,then run three demos,the gattc_multi_connect demo will connect
* the three gatt_server demos, and then exchange data.
* Of course you can also modify the code to connect more devices, we default to connect
* up to 4 devices, more than 4 you need to modify menuconfig.
*
****************************************************************************/

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "controller.h"

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"

#include "common.h"

#define GATTC_TAG "Shanvi MN Client"
#define SMPH_SERVICE_UUID        0x0000
#define MN_SERVICE_UUID        0x0002
#define REMOTE_NOTIFY_CHAR_UUID    0x0011

/* register three profiles, each profile corresponds to one connection,
   which makes it easy to handle each connection event */
#define PROFILE_NUM 3
#define PROFILE_A_APP_ID 0
#define PROFILE_B_APP_ID 1
#define INVALID_HANDLE   0

/* Declare static functions */
static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
static void gattc_profile_a_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
static void gattc_profile_b_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);

static esp_bt_uuid_t smph_service_uuid = {
    .len = ESP_UUID_LEN_128,
    .uuid = { .uuid128 = {SMPH_SERVICE_UUID128}},
};

static esp_bt_uuid_t mn_service_uuid = {
    .len = ESP_UUID_LEN_128,
    .uuid = {.uuid128 = {MN_SERVICE_UUID128},},
};

static esp_bt_uuid_t char_safety_alert = {
    .len = ESP_UUID_LEN_128,
    .uuid = {.uuid128 = {CHAR_SAFETY_ALERT},},
};

static esp_bt_uuid_t char_alert_status = {
    .len = ESP_UUID_LEN_128,
    .uuid = {.uuid128 = {CHAR_ALERT_STATUS},},
};

static esp_bt_uuid_t notify_descr_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG,},
};

static bool conn_device_a   = false;
static bool conn_device_b   = false;

static bool get_service_a   = false;
static bool get_service_b   = false;

static bool Isconnecting    = false;
static bool stop_scan_done  = false;

static esp_gattc_char_elem_t  *char_elem_result_a   = NULL;
static esp_gattc_descr_elem_t *descr_elem_result_a  = NULL;
static esp_gattc_char_elem_t  *char_elem_result_b   = NULL;
static esp_gattc_descr_elem_t *descr_elem_result_b  = NULL;

static const char remote_device_name[2][20] = {"Shanvi", "Sh_MN"};

static esp_ble_scan_params_t ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x50,
    .scan_window            = 0x30
};

struct gattc_profile_inst {
    esp_gattc_cb_t gattc_cb;
    uint16_t gattc_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_start_handle;
    uint16_t service_end_handle;
    uint16_t tx_handle;
    uint16_t rx_handle;
    uint16_t cccd_handle;
    esp_bd_addr_t remote_bda;
    bool ready_to_send;
};

/* One gatt-based profile one app_id and one gattc_if, this array will store the gattc_if returned by ESP_GATTS_REG_EVT */
static struct gattc_profile_inst gl_profile_tab[PROFILE_NUM] = {
    [PROFILE_A_APP_ID] = {
        .gattc_cb = gattc_profile_a_event_handler,
        .gattc_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
		.ready_to_send = false,
    },
    [PROFILE_B_APP_ID] = {
        .gattc_cb = gattc_profile_b_event_handler,
        .gattc_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
		.ready_to_send = false,
    },

};

void pre_start_client_scan() {
	stop_advertising();
    stop_scan_done = false;
    Isconnecting = false;
    esp_err_t scan_ret = esp_ble_gap_set_scan_params(&ble_scan_params);
    if (scan_ret){
        ESP_LOGE(GATTC_TAG, "set scan params error, error code = %x", scan_ret);
    }
}

void start_client_scan(void) {
    uint32_t duration = 30;
    esp_ble_gap_start_scanning(duration);
}

void stop_client_scan(void) {
    esp_ble_gap_stop_scanning();
	start_advertising();
}

static bool isUUIDpresent(esp_gatt_srvc_id_t *srvc_id, esp_bt_uuid_t * p_uuid) {
	if (srvc_id->id.uuid.len == ESP_UUID_LEN_16 && p_uuid->len == ESP_UUID_LEN_16
			&& srvc_id->id.uuid.uuid.uuid16 == p_uuid->uuid.uuid16) {
		ESP_LOGI(GATTC_TAG, "UUID16: %x", srvc_id->id.uuid.uuid.uuid16);
		return true;
	}
	if (srvc_id->id.uuid.len == ESP_UUID_LEN_128 && p_uuid->len == ESP_UUID_LEN_128) {
		bool match = true;
		uint8_t * p_data = (uint8_t *) srvc_id->id.uuid.uuid.uuid128;
		uint8_t * p_target_uuid = (uint8_t*) &p_uuid->uuid.uuid128;
		for (uint8_t i = 0; i < 16; i++) {
			if (p_data[i] != p_target_uuid[i]) {
				match = false;
			}
		}
		if (match) {
			ESP_LOGI(GATTC_TAG, "UUID128:");
			esp_log_buffer_hex(GATTC_TAG, p_target_uuid, 16);
			return true;
		}

	}
	return false;
}

static void gattc_profile_a_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{

    esp_ble_gattc_cb_param_t *p_data = (esp_ble_gattc_cb_param_t *)param;

    switch (event) {
    case ESP_GATTC_REG_EVT:
        ESP_LOGI(GATTC_TAG, "REG_EVT");
        pre_start_client_scan();
        break;
    /* one device connect successfully, all profiles callback function will get the ESP_GATTC_CONNECT_EVT,
     so must compare the mac address to check which device is connected, so it is a good choice to use ESP_GATTC_OPEN_EVT. */
    case ESP_GATTC_CONNECT_EVT:
        break;
    case ESP_GATTC_OPEN_EVT:
        if (p_data->open.status != ESP_GATT_OK){
            //open failed, ignore the first device, connect the second device
            ESP_LOGE(GATTC_TAG, "connect device failed, status %d", p_data->open.status);
            conn_device_a = false;
            gl_profile_tab[PROFILE_A_APP_ID].ready_to_send = false;
            //start_client_scan();
            break;
        }
        memcpy(gl_profile_tab[PROFILE_A_APP_ID].remote_bda, p_data->open.remote_bda, 6);
        gl_profile_tab[PROFILE_A_APP_ID].conn_id = p_data->open.conn_id;
        ESP_LOGI(GATTC_TAG, "ESP_GATTC_OPEN_EVT conn_id %d, if %d, status %d, mtu %d", p_data->open.conn_id, gattc_if, p_data->open.status, p_data->open.mtu);
        ESP_LOGI(GATTC_TAG, "REMOTE BDA:");
        esp_log_buffer_hex(GATTC_TAG, p_data->open.remote_bda, sizeof(esp_bd_addr_t));
        esp_err_t mtu_ret = esp_ble_gattc_send_mtu_req (gattc_if, p_data->open.conn_id);
        if (mtu_ret){
            ESP_LOGE(GATTC_TAG, "config MTU error, error code = %x", mtu_ret);
        }
        break;
    case ESP_GATTC_CFG_MTU_EVT:
        if (param->cfg_mtu.status != ESP_GATT_OK){
            ESP_LOGE(GATTC_TAG,"Config mtu failed");
        }
        ESP_LOGI(GATTC_TAG, "Status %d, MTU %d, conn_id %d", param->cfg_mtu.status, param->cfg_mtu.mtu, param->cfg_mtu.conn_id);
        esp_ble_gattc_search_service(gattc_if, param->cfg_mtu.conn_id, &smph_service_uuid);
        break;
    case ESP_GATTC_SEARCH_RES_EVT: {
        esp_gatt_srvc_id_t *srvc_id = (esp_gatt_srvc_id_t *)&p_data->search_res.srvc_id;
        ESP_LOGI(GATTC_TAG, "SEARCH RES: conn_id = %x", p_data->search_res.conn_id);
        if (isUUIDpresent(srvc_id, &smph_service_uuid)) {
            get_service_a = true;
            gl_profile_tab[PROFILE_A_APP_ID].service_start_handle = p_data->search_res.start_handle;
            gl_profile_tab[PROFILE_A_APP_ID].service_end_handle = p_data->search_res.end_handle;
        }
        break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT:
        if (p_data->search_cmpl.status != ESP_GATT_OK){
            ESP_LOGE(GATTC_TAG, "search service failed, error status = %x", p_data->search_cmpl.status);
            break;
        }
        if (get_service_a){
            uint16_t count = 0;
            esp_gatt_status_t status = esp_ble_gattc_get_attr_count( gattc_if,
                                                                     p_data->search_cmpl.conn_id,
                                                                     ESP_GATT_DB_CHARACTERISTIC,
                                                                     gl_profile_tab[PROFILE_A_APP_ID].service_start_handle,
                                                                     gl_profile_tab[PROFILE_A_APP_ID].service_end_handle,
                                                                     INVALID_HANDLE,
                                                                     &count);
            if (status != ESP_GATT_OK){
                ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_attr_count error");
            }
            if (count > 0) {
                char_elem_result_a = (esp_gattc_char_elem_t *)malloc(sizeof(esp_gattc_char_elem_t) * count);
                if (!char_elem_result_a){
                    ESP_LOGE(GATTC_TAG, "gattc no mem");
                }else {
                    status = esp_ble_gattc_get_char_by_uuid( gattc_if,
                                                             p_data->search_cmpl.conn_id,
                                                             gl_profile_tab[PROFILE_A_APP_ID].service_start_handle,
                                                             gl_profile_tab[PROFILE_A_APP_ID].service_end_handle,
							                                 char_safety_alert,
                                                             char_elem_result_a,
                                                             &count);
                    if (status != ESP_GATT_OK){
                        ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_char_by_uuid error");
                    }

                    /*  Every service have only one char in our 'ESP_GATTS_DEMO' demo, so we used first 'char_elem_result' */
                    if (count > 0 && (char_elem_result_a[0].properties & ESP_GATT_CHAR_PROP_BIT_WRITE)){
                        gl_profile_tab[PROFILE_A_APP_ID].rx_handle = char_elem_result_a[0].char_handle;
                        gl_profile_tab[PROFILE_A_APP_ID].ready_to_send = true;
                    }

                    status = esp_ble_gattc_get_char_by_uuid( gattc_if,
                                                             p_data->search_cmpl.conn_id,
                                                             gl_profile_tab[PROFILE_A_APP_ID].service_start_handle,
                                                             gl_profile_tab[PROFILE_A_APP_ID].service_end_handle,
							                                 char_alert_status,
                                                             char_elem_result_a,
                                                             &count);
                    if (status != ESP_GATT_OK){
                        ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_char_by_uuid error");
                    }

                    /*  Every service have only one char in our 'ESP_GATTS_DEMO' demo, so we used first 'char_elem_result' */
                    if (count > 0 && (char_elem_result_a[0].properties & ESP_GATT_CHAR_PROP_BIT_NOTIFY)){
                        gl_profile_tab[PROFILE_A_APP_ID].tx_handle = char_elem_result_a[0].char_handle;
                        esp_ble_gattc_register_for_notify (gattc_if, gl_profile_tab[PROFILE_A_APP_ID].remote_bda, char_elem_result_a[0].char_handle);
                    }

                }
                /* free char_elem_result */
                free(char_elem_result_a);
            }else {
                ESP_LOGE(GATTC_TAG, "no char found");
            }
        }
        break;
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
        if (p_data->reg_for_notify.status != ESP_GATT_OK){
            ESP_LOGE(GATTC_TAG, "reg notify failed, error status =%x", p_data->reg_for_notify.status);
            break;
        }
        uint16_t count = 0;
        uint16_t notify_en = 1;
        esp_gatt_status_t ret_status = esp_ble_gattc_get_attr_count( gattc_if,
                                                                     gl_profile_tab[PROFILE_A_APP_ID].conn_id,
                                                                     ESP_GATT_DB_DESCRIPTOR,
                                                                     gl_profile_tab[PROFILE_A_APP_ID].service_start_handle,
                                                                     gl_profile_tab[PROFILE_A_APP_ID].service_end_handle,
                                                                     gl_profile_tab[PROFILE_A_APP_ID].tx_handle,
                                                                     &count);
        if (ret_status != ESP_GATT_OK){
            ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_attr_count error");
        }
        if (count > 0){
            descr_elem_result_a = (esp_gattc_descr_elem_t *)malloc(sizeof(esp_gattc_descr_elem_t) * count);
            if (!descr_elem_result_a){
                ESP_LOGE(GATTC_TAG, "malloc error, gattc no mem");
            }else{
                ret_status = esp_ble_gattc_get_descr_by_char_handle( gattc_if,
                                                                     gl_profile_tab[PROFILE_A_APP_ID].conn_id,
                                                                     p_data->reg_for_notify.handle,
                                                                     notify_descr_uuid,
                                                                     descr_elem_result_a,
                                                                     &count);
                if (ret_status != ESP_GATT_OK){
                    ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_descr_by_char_handle error");
                }

                /* Every char has only one descriptor in our 'ESP_GATTS_DEMO' demo, so we used first 'descr_elem_result' */
                if (count > 0 && descr_elem_result_a[0].uuid.len == ESP_UUID_LEN_16 && descr_elem_result_a[0].uuid.uuid.uuid16 == ESP_GATT_UUID_CHAR_CLIENT_CONFIG){
                	gl_profile_tab[PROFILE_A_APP_ID].cccd_handle = descr_elem_result_a[0].handle;
                	ret_status = esp_ble_gattc_write_char_descr( gattc_if,
                                                                 gl_profile_tab[PROFILE_A_APP_ID].conn_id,
                                                                 descr_elem_result_a[0].handle,
                                                                 sizeof(notify_en),
                                                                 (uint8_t *)&notify_en,
                                                                 ESP_GATT_WRITE_TYPE_RSP,
                                                                 ESP_GATT_AUTH_REQ_NONE);
                }

                if (ret_status != ESP_GATT_OK){
                    ESP_LOGE(GATTC_TAG, "esp_ble_gattc_write_char_descr error");
                }

                /* free descr_elem_result */
                free(descr_elem_result_a);
            }
        }
        else{
            ESP_LOGE(GATTC_TAG, "decsr not found");
        }
        break;
    }
    case ESP_GATTC_NOTIFY_EVT:
        ESP_LOGI(GATTC_TAG, "ESP_GATTC_NOTIFY_EVT, Receive notify value:");
        esp_log_buffer_hex(GATTC_TAG, p_data->notify.value, p_data->notify.value_len);
        break;
    case ESP_GATTC_WRITE_DESCR_EVT:
        if (p_data->write.status != ESP_GATT_OK){
            ESP_LOGE(GATTC_TAG, "write descr failed, error status = %x", p_data->write.status);
            break;
        }
        ESP_LOGI(GATTC_TAG, "write descr success");
        uint8_t write_char_data[35];
        for (int i = 0; i < sizeof(write_char_data); ++i)
        {
            write_char_data[i] = i % 256;
        }
        esp_ble_gattc_write_char( gattc_if,
                                  gl_profile_tab[PROFILE_A_APP_ID].conn_id,
                                  gl_profile_tab[PROFILE_A_APP_ID].rx_handle,
                                  sizeof(write_char_data),
                                  write_char_data,
                                  ESP_GATT_WRITE_TYPE_RSP,
                                  ESP_GATT_AUTH_REQ_NONE);
        break;
    case ESP_GATTC_WRITE_CHAR_EVT:
        if (p_data->write.status != ESP_GATT_OK){
            ESP_LOGE(GATTC_TAG, "write char failed, error status = %x", p_data->write.status);
        }else{
            ESP_LOGI(GATTC_TAG, "write char success");
        }
        start_client_scan();
        break;
    case ESP_GATTC_SRVC_CHG_EVT: {
        esp_bd_addr_t bda;
        memcpy(bda, p_data->srvc_chg.remote_bda, sizeof(esp_bd_addr_t));
        ESP_LOGI(GATTC_TAG, "ESP_GATTC_SRVC_CHG_EVT, bd_addr:%08x%04x",(bda[0] << 24) + (bda[1] << 16) + (bda[2] << 8) + bda[3],
                 (bda[4] << 8) + bda[5]);
        break;
    }
    case ESP_GATTC_DISCONNECT_EVT:
        //Start scanning again
        //start_client_scan();
        if (memcmp(p_data->disconnect.remote_bda, gl_profile_tab[PROFILE_A_APP_ID].remote_bda, 6) == 0){
            ESP_LOGI(GATTC_TAG, "device a disconnect");
            conn_device_a = false;
            get_service_a = false;
            gl_profile_tab[PROFILE_A_APP_ID].ready_to_send = false;
        }
        break;
    default:
        break;
    }
}

static void gattc_profile_b_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    esp_ble_gattc_cb_param_t *p_data = (esp_ble_gattc_cb_param_t *)param;

    switch (event) {
    case ESP_GATTC_REG_EVT:
        ESP_LOGI(GATTC_TAG, "REG_EVT");
        break;
    case ESP_GATTC_CONNECT_EVT:
        break;
    case ESP_GATTC_OPEN_EVT:
        if (p_data->open.status != ESP_GATT_OK){
            //open failed, ignore the second device, connect the third device
            ESP_LOGE(GATTC_TAG, "connect device failed, status %d", p_data->open.status);
            conn_device_b = false;
            gl_profile_tab[PROFILE_B_APP_ID].ready_to_send = false;
            //start_client_scan();
            break;
        }
        memcpy(gl_profile_tab[PROFILE_B_APP_ID].remote_bda, p_data->open.remote_bda, 6);
        gl_profile_tab[PROFILE_B_APP_ID].conn_id = p_data->open.conn_id;
        ESP_LOGI(GATTC_TAG, "ESP_GATTC_OPEN_EVT conn_id %d, if %d, status %d, mtu %d", p_data->open.conn_id, gattc_if, p_data->open.status, p_data->open.mtu);
        ESP_LOGI(GATTC_TAG, "REMOTE BDA:");
        esp_log_buffer_hex(GATTC_TAG, p_data->open.remote_bda, sizeof(esp_bd_addr_t));
        esp_err_t mtu_ret = esp_ble_gattc_send_mtu_req (gattc_if, p_data->open.conn_id);
        if (mtu_ret){
            ESP_LOGE(GATTC_TAG, "config MTU error, error code = %x", mtu_ret);
        }
        break;
    case ESP_GATTC_CFG_MTU_EVT:
        if (param->cfg_mtu.status != ESP_GATT_OK){
            ESP_LOGE(GATTC_TAG,"Config mtu failed");
        }
        ESP_LOGI(GATTC_TAG, "Status %d, MTU %d, conn_id %d", param->cfg_mtu.status, param->cfg_mtu.mtu, param->cfg_mtu.conn_id);
        esp_ble_gattc_search_service(gattc_if, param->cfg_mtu.conn_id, &mn_service_uuid);
        break;
    case ESP_GATTC_SEARCH_RES_EVT: {
        esp_gatt_srvc_id_t *srvc_id = (esp_gatt_srvc_id_t *)&p_data->search_res.srvc_id;
        ESP_LOGI(GATTC_TAG, "SEARCH RES: conn_id = %x", p_data->search_res.conn_id);
        if (isUUIDpresent(srvc_id, &mn_service_uuid)) {
            get_service_b = true;
            gl_profile_tab[PROFILE_B_APP_ID].service_start_handle = p_data->search_res.start_handle;
            gl_profile_tab[PROFILE_B_APP_ID].service_end_handle = p_data->search_res.end_handle;
        }
        break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT:
        if (p_data->search_cmpl.status != ESP_GATT_OK){
            ESP_LOGE(GATTC_TAG, "search service failed, error status = %x", p_data->search_cmpl.status);
            break;
        }
        if (get_service_b){
            uint16_t count = 0;
            esp_gatt_status_t status = esp_ble_gattc_get_attr_count( gattc_if,
                                                                     p_data->search_cmpl.conn_id,
                                                                     ESP_GATT_DB_CHARACTERISTIC,
                                                                     gl_profile_tab[PROFILE_B_APP_ID].service_start_handle,
                                                                     gl_profile_tab[PROFILE_B_APP_ID].service_end_handle,
                                                                     INVALID_HANDLE,
                                                                     &count);
            if (status != ESP_GATT_OK){
                ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_attr_count error");
            }

            if (count > 0){
                char_elem_result_b = (esp_gattc_char_elem_t *)malloc(sizeof(esp_gattc_char_elem_t) * count);
                if (!char_elem_result_b){
                    ESP_LOGE(GATTC_TAG, "gattc no mem");
                }else{
                    status = esp_ble_gattc_get_char_by_uuid( gattc_if,
                                                             p_data->search_cmpl.conn_id,
                                                             gl_profile_tab[PROFILE_B_APP_ID].service_start_handle,
                                                             gl_profile_tab[PROFILE_B_APP_ID].service_end_handle,
							                                 char_safety_alert,
                                                             char_elem_result_b,
                                                             &count);
                    if (status != ESP_GATT_OK){
                        ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_char_by_uuid error");
                    }

                    /*  Every service have only one char in our 'ESP_GATTS_DEMO' demo, so we used first 'char_elem_result' */
                    if (count > 0 && (char_elem_result_a[0].properties & ESP_GATT_CHAR_PROP_BIT_WRITE)){
                        gl_profile_tab[PROFILE_B_APP_ID].rx_handle = char_elem_result_b[0].char_handle;
                        gl_profile_tab[PROFILE_B_APP_ID].ready_to_send = true;
                    }

                    status = esp_ble_gattc_get_char_by_uuid( gattc_if,
                                                             p_data->search_cmpl.conn_id,
                                                             gl_profile_tab[PROFILE_B_APP_ID].service_start_handle,
                                                             gl_profile_tab[PROFILE_B_APP_ID].service_end_handle,
							                                 char_alert_status,
                                                             char_elem_result_b,
                                                             &count);
                    if (status != ESP_GATT_OK){
                        ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_char_by_uuid error");
                    }

                    /*  Every service have only one char in our 'ESP_GATTS_DEMO' demo, so we used first 'char_elem_result' */
                    if (count > 0 && (char_elem_result_a[0].properties & ESP_GATT_CHAR_PROP_BIT_NOTIFY)){
                        gl_profile_tab[PROFILE_B_APP_ID].tx_handle = char_elem_result_b[0].char_handle;
                        esp_ble_gattc_register_for_notify (gattc_if, gl_profile_tab[PROFILE_B_APP_ID].remote_bda, char_elem_result_b[0].char_handle);
                    }

                }
                /* free char_elem_result */
                free(char_elem_result_b);
            }else{
                ESP_LOGE(GATTC_TAG, "no char found");
            }
        }
        break;
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {

        if (p_data->reg_for_notify.status != ESP_GATT_OK){
            ESP_LOGE(GATTC_TAG, "reg notify failed, error status =%x", p_data->reg_for_notify.status);
            break;
        }
        uint16_t count = 0;
        uint16_t notify_en = 1;
        esp_gatt_status_t ret_status = esp_ble_gattc_get_attr_count( gattc_if,
                                                                     gl_profile_tab[PROFILE_B_APP_ID].conn_id,
                                                                     ESP_GATT_DB_DESCRIPTOR,
                                                                     gl_profile_tab[PROFILE_B_APP_ID].service_start_handle,
                                                                     gl_profile_tab[PROFILE_B_APP_ID].service_end_handle,
                                                                     gl_profile_tab[PROFILE_B_APP_ID].tx_handle,
                                                                     &count);
        if (ret_status != ESP_GATT_OK){
            ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_attr_count error");
        }
        if (count > 0){
            descr_elem_result_b = (esp_gattc_descr_elem_t *)malloc(sizeof(esp_gattc_descr_elem_t) * count);
            if (!descr_elem_result_b){
                ESP_LOGE(GATTC_TAG, "malloc error, gattc no mem");
            }else{
                ret_status = esp_ble_gattc_get_descr_by_char_handle( gattc_if,
                                                                     gl_profile_tab[PROFILE_B_APP_ID].conn_id,
                                                                     p_data->reg_for_notify.handle,
                                                                     notify_descr_uuid,
                                                                     descr_elem_result_b,
                                                                     &count);
                if (ret_status != ESP_GATT_OK){
                    ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_descr_by_char_handle error");
                }

                /* Every char has only one descriptor in our 'ESP_GATTS_DEMO' demo, so we used first 'descr_elem_result' */
                if (count > 0 && descr_elem_result_b[0].uuid.len == ESP_UUID_LEN_16 && descr_elem_result_b[0].uuid.uuid.uuid16 == ESP_GATT_UUID_CHAR_CLIENT_CONFIG){
		    gl_profile_tab[PROFILE_B_APP_ID].cccd_handle = descr_elem_result_b[0].handle;
                    ret_status = esp_ble_gattc_write_char_descr( gattc_if,
                                                                 gl_profile_tab[PROFILE_B_APP_ID].conn_id,
                                                                 descr_elem_result_b[0].handle,
                                                                 sizeof(notify_en),
                                                                 (uint8_t *)&notify_en,
                                                                 ESP_GATT_WRITE_TYPE_RSP,
                                                                 ESP_GATT_AUTH_REQ_NONE);
                }

                if (ret_status != ESP_GATT_OK){
                    ESP_LOGE(GATTC_TAG, "esp_ble_gattc_write_char_descr error");
                }

                /* free descr_elem_result */
                free(descr_elem_result_b);
            }
        }
        else{
            ESP_LOGE(GATTC_TAG, "decsr not found");
        }
        break;
    }
    case ESP_GATTC_NOTIFY_EVT:
        ESP_LOGI(GATTC_TAG, "ESP_GATTC_NOTIFY_EVT, Receive notify value:");
        esp_log_buffer_hex(GATTC_TAG, p_data->notify.value, p_data->notify.value_len);
        break;
    case ESP_GATTC_WRITE_DESCR_EVT:
        if (p_data->write.status != ESP_GATT_OK){
            ESP_LOGE(GATTC_TAG, "write descr failed, error status = %x", p_data->write.status);
            break;
        }
        ESP_LOGI(GATTC_TAG, "write descr success");
        uint8_t write_char_data[35];
        for (int i = 0; i < sizeof(write_char_data); ++i)
        {
            write_char_data[i] = i % 256;
        }
        esp_ble_gattc_write_char( gattc_if,
                                  gl_profile_tab[PROFILE_B_APP_ID].conn_id,
                                  gl_profile_tab[PROFILE_B_APP_ID].rx_handle,
                                  sizeof(write_char_data),
                                  write_char_data,
                                  ESP_GATT_WRITE_TYPE_RSP,
                                  ESP_GATT_AUTH_REQ_NONE);
        break;
    case ESP_GATTC_WRITE_CHAR_EVT:
        if (p_data->write.status != ESP_GATT_OK){
            ESP_LOGE(GATTC_TAG, "Write char failed, error status = %x", p_data->write.status);
        }else{
            ESP_LOGI(GATTC_TAG, "Write char success");
        }
        start_client_scan();
        break;
    case ESP_GATTC_SRVC_CHG_EVT: {
        esp_bd_addr_t bda;
        memcpy(bda, p_data->srvc_chg.remote_bda, sizeof(esp_bd_addr_t));
        ESP_LOGI(GATTC_TAG, "ESP_GATTC_SRVC_CHG_EVT, bd_addr:%08x%04x",(bda[0] << 24) + (bda[1] << 16) + (bda[2] << 8) + bda[3],
                 (bda[4] << 8) + bda[5]);
        break;
    }
    case ESP_GATTC_DISCONNECT_EVT:
        if (memcmp(p_data->disconnect.remote_bda, gl_profile_tab[PROFILE_B_APP_ID].remote_bda, 6) == 0){
            ESP_LOGI(GATTC_TAG, "device b disconnect");
            conn_device_b = false;
            get_service_b = false;
            gl_profile_tab[PROFILE_B_APP_ID].ready_to_send = false;
        }
        break;
    default:
        break;
    }
}

void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    uint8_t *adv_name = NULL;
    uint8_t adv_name_len = 0;
    switch (event) {
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
         ESP_LOGI(GATTC_TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
                  param->update_conn_params.status,
                  param->update_conn_params.min_int,
                  param->update_conn_params.max_int,
                  param->update_conn_params.conn_int,
                  param->update_conn_params.latency,
                  param->update_conn_params.timeout);
        break;
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
    	start_client_scan();
        break;
    }
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        //scan start complete event to indicate scan start successfully or failed
        if (param->scan_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
            ESP_LOGI(GATTC_TAG, "Scan start success");
        }else{
            ESP_LOGE(GATTC_TAG, "Scan start failed");
        }
        break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
        esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
        switch (scan_result->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
            esp_log_buffer_hex(GATTC_TAG, scan_result->scan_rst.bda, 6);
            ESP_LOGI(GATTC_TAG, "Searched Adv Data Len %d, Scan Response Len %d", scan_result->scan_rst.adv_data_len, scan_result->scan_rst.scan_rsp_len);
            adv_name = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv,
                                                ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);
            ESP_LOGI(GATTC_TAG, "Searched Device Name Len %d", adv_name_len);
            esp_log_buffer_char(GATTC_TAG, adv_name, adv_name_len);
            ESP_LOGI(GATTC_TAG, "\n");
            if (Isconnecting){
                break;
            }
            if (conn_device_a && conn_device_b && !stop_scan_done){
                stop_scan_done = true;
                stop_client_scan();
                ESP_LOGI(GATTC_TAG, "all devices are connected");
                break;
            }
            if (adv_name != NULL) {

                if (strlen(remote_device_name[0]) == adv_name_len && strncmp((char *)adv_name, remote_device_name[0], adv_name_len) == 0) {
                    if (conn_device_a == false) {
                        conn_device_a = true;
                        gl_profile_tab[PROFILE_A_APP_ID].ready_to_send = false;
                        ESP_LOGI(GATTC_TAG, "Searched device %s", remote_device_name[0]);
                        esp_ble_gap_stop_scanning();
                        esp_ble_gattc_open(gl_profile_tab[PROFILE_A_APP_ID].gattc_if, scan_result->scan_rst.bda, scan_result->scan_rst.ble_addr_type, true);
                        Isconnecting = true;
                    }
                    break;
                }
                else if (strlen(remote_device_name[1]) == adv_name_len && strncmp((char *)adv_name, remote_device_name[1], adv_name_len) == 0) {
                    if (conn_device_b == false) {
                        conn_device_b = true;
                        gl_profile_tab[PROFILE_B_APP_ID].ready_to_send = false;
                        ESP_LOGI(GATTC_TAG, "Searched device %s", remote_device_name[1]);
                        esp_ble_gap_stop_scanning();
                        esp_ble_gattc_open(gl_profile_tab[PROFILE_B_APP_ID].gattc_if, scan_result->scan_rst.bda, scan_result->scan_rst.ble_addr_type, true);
                        Isconnecting = true;

                    }
                }

            }
            break;
        case ESP_GAP_SEARCH_INQ_CMPL_EVT:
            break;
        default:
            break;
        }
        break;
    }

    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS){
            ESP_LOGE(GATTC_TAG, "Scan stop failed");
            break;
        }
        ESP_LOGI(GATTC_TAG, "Stop scan successfully");

        break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS){
            ESP_LOGE(GATTC_TAG, "Adv stop failed");
            break;
        }
        ESP_LOGI(GATTC_TAG, "Stop adv successfully");
        break;

    default:
        break;
    }
}

static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    //ESP_LOGI(GATTC_TAG, "EVT %d, gattc if %d, app_id %d", event, gattc_if, param->reg.app_id);

    /* If event is register event, store the gattc_if for each profile */
    if (event == ESP_GATTC_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            gl_profile_tab[param->reg.app_id].gattc_if = gattc_if;
        } else {
            ESP_LOGI(GATTC_TAG, "Reg app failed, app_id %04x, status %d",
                    param->reg.app_id,
                    param->reg.status);
            return;
        }
    }

    /* If the gattc_if equal to profile A, call profile A cb handler,
     * so here call each profile's callback */
    do {
        int idx;
        for (idx = 0; idx < PROFILE_NUM; idx++) {
            if (gattc_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
                    gattc_if == gl_profile_tab[idx].gattc_if) {
                if (gl_profile_tab[idx].gattc_cb) {
                    gl_profile_tab[idx].gattc_cb(event, gattc_if, param);
                }
            }
        }
    } while (0);
}

void app_client_main()
{
    esp_err_t ret;

    //register the  callback function to the gap module
    ret = esp_ble_gap_register_callback(esp_gap_cb);
    if (ret){
        ESP_LOGE(GATTC_TAG, "%s gap register failed, error code = %x\n", __func__, ret);
        return;
    }

    //register the callback function to the gattc module
    ret = esp_ble_gattc_register_callback(esp_gattc_cb);
    if(ret){
        ESP_LOGE(GATTC_TAG, "gattc register error, error code = %x", ret);
        return;
    }

    ret = esp_ble_gattc_app_register(PROFILE_A_APP_ID);
    if (ret){
        ESP_LOGE(GATTC_TAG, "gattc app register error, error code = %x", ret);
        return;
    }

    ret = esp_ble_gattc_app_register(PROFILE_B_APP_ID);
    if (ret){
        ESP_LOGE(GATTC_TAG, "gattc app register error, error code = %x", ret);
        return;
    }

}

bool isClientConnected() {
	return conn_device_a || conn_device_b;
}

bool isClientReady() {
	return gl_profile_tab[PROFILE_A_APP_ID].ready_to_send || gl_profile_tab[PROFILE_B_APP_ID].ready_to_send;
}

void send_packet(uint8_t * data, int len) {
	if (gl_profile_tab[PROFILE_A_APP_ID].ready_to_send &&
			!check_hop(data, gl_profile_tab[PROFILE_A_APP_ID].remote_bda[0])) {
		add_hop(data, gl_profile_tab[PROFILE_A_APP_ID].remote_bda[0]);
        esp_ble_gattc_write_char( gl_profile_tab[PROFILE_A_APP_ID].gattc_if,
                                  gl_profile_tab[PROFILE_A_APP_ID].conn_id,
                                  gl_profile_tab[PROFILE_A_APP_ID].rx_handle,
                                  len,
                                  data,
                                  ESP_GATT_WRITE_TYPE_RSP,
                                  ESP_GATT_AUTH_REQ_NONE);
	}
	if (gl_profile_tab[PROFILE_B_APP_ID].ready_to_send &&
			!check_hop(data, gl_profile_tab[PROFILE_B_APP_ID].remote_bda[0])) {
		add_hop(data, gl_profile_tab[PROFILE_B_APP_ID].remote_bda[0]);
        esp_ble_gattc_write_char( gl_profile_tab[PROFILE_B_APP_ID].gattc_if,
                                  gl_profile_tab[PROFILE_B_APP_ID].conn_id,
                                  gl_profile_tab[PROFILE_B_APP_ID].rx_handle,
                                  len,
                                  data,
                                  ESP_GATT_WRITE_TYPE_RSP,
                                  ESP_GATT_AUTH_REQ_NONE);
	}
}

