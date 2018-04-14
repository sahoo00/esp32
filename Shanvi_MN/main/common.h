/*
 * common.h
 *
 *  Created on: Apr 10, 2018
 *      Author: Debashis
 */

#ifndef MAIN_COMMON_H_
#define MAIN_COMMON_H_

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"


#define MN_DEVICE_ID        0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
#define MN_CKEY        		"73b1ed59ebfdd3e3"


#define SMPH_SERVICE_UUID128 \
	0xc6, 0x1d, 0x93, 0x6e, 0x09, 0x03, 0x63, 0x8b,	0xe8, 0x49, 0xa5, 0xcf, 0x00, 0x00, 0xba, 0xde,
	/* LSB <--------------------------------------------------------------------------------> MSB */
	/* first uuid, 16bit, [12],[13] is the value                                                  */
#define MN_SERVICE_UUID128 \
	0xc6, 0x1d, 0x93, 0x6e, 0x09, 0x03, 0x63, 0x8b,	0xe8, 0x49, 0xa5, 0xcf, 0x02, 0x00, 0xba, 0xde,

#define CHAR_SAFETY_ALERT \
	0xc6, 0x1d, 0x93, 0x6e, 0x09, 0x03, 0x63, 0x8b,	0xe8, 0x49, 0xa5, 0xcf, 0x10, 0x00, 0xba, 0xde,
#define CHAR_ALERT_STATUS \
	0xc6, 0x1d, 0x93, 0x6e, 0x09, 0x03, 0x63, 0x8b,	0xe8, 0x49, 0xa5, 0xcf, 0x11, 0x00, 0xba, 0xde,
#define CHAR_SAFE_MN_SSID \
	0xc6, 0x1d, 0x93, 0x6e, 0x09, 0x03, 0x63, 0x8b,	0xe8, 0x49, 0xa5, 0xcf, 0x12, 0x00, 0xba, 0xde,
#define CHAR_SAFE_MN_PASS \
	0xc6, 0x1d, 0x93, 0x6e, 0x09, 0x03, 0x63, 0x8b,	0xe8, 0x49, 0xa5, 0xcf, 0x13, 0x00, 0xba, 0xde,
#define CHAR_SAFE_MN_CMD \
	0xc6, 0x1d, 0x93, 0x6e, 0x09, 0x03, 0x63, 0x8b,	0xe8, 0x49, 0xa5, 0xcf, 0x14, 0x00, 0xba, 0xde,

void print_string(char *data, int len);
void app_ble_main();
void app_gatts_main();
void app_wifi_main();
void app_client_main();
void app_simple_client_main();

void set_ssid(const char * ssid, int len);
void set_pass(const char * pass, int len);
void connect_wifi();
bool is_wifi_connected();

void blinky_start();
void blinky_stop();

void notify_connection(uint8_t *data, int len);
void stop_advertising();
void start_advertising();

bool sendTrigger(long did, uint32_t ckey, const char* rinfo);
bool cancelTrigger(long did, uint32_t ckey, const char* rinfo);
bool sendLocation(long did, float lat, float lon, float alt, const char* rinfo);

void processData(uint8_t *data, int len);
void router_start();
void router_stop();

void pre_start_client_scan();
void start_client_scan(void);
void stop_client_scan(void);
bool isClientConnected();
bool isClientReady();
void add_hop(void * data, uint8_t h);
bool check_hop(void * data, uint8_t h);
void pre_start_simple_client_scan();
void stop_simple_client_scan(void);
void send_packet_simple(uint8_t * data, int len);
bool isSimpleClientConnected();
bool isSimpleClientReady();
void prepare_to_send_simple(uint8_t * data, int len);

#endif /* MAIN_COMMON_H_ */
