/*
 * common.h
 *
 *  Created on: Apr 10, 2018
 *      Author: Debashis
 */

#ifndef MAIN_COMMON_H_
#define MAIN_COMMON_H_


void print_string(char *data, int len);
void app_gatts_main();
void app_wifi_main();

void set_ssid(const char * ssid, int len);
void set_pass(const char * pass, int len);
void connect_wifi();

void blinky_start();
void blinky_stop();

void notify_connection(uint8_t *data, int len);

#endif /* MAIN_COMMON_H_ */
