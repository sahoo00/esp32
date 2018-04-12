/*
 * common.h
 *
 *  Created on: Apr 10, 2018
 *      Author: Debashis
 */

#ifndef MAIN_COMMON_H_
#define MAIN_COMMON_H_


#define MN_DEVICE_ID        0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
#define MN_CKEY        		"73b1ed59ebfdd3e3"


void print_string(char *data, int len);
void app_gatts_main();
void app_wifi_main();

void set_ssid(const char * ssid, int len);
void set_pass(const char * pass, int len);
void connect_wifi();
bool is_wifi_connected();

void blinky_start();
void blinky_stop();

void notify_connection(uint8_t *data, int len);


void processData(uint8_t *data, int len);

#endif /* MAIN_COMMON_H_ */
