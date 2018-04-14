/*
 * router.c
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

#include "common.h"

static const char *TAG = "Shanvi MN Router";
#define MAX_HOPS_DATA 5

enum {
	PKT_TRGN,
	PKT_TRGC,
	PKT_LOC,
	PKT_ACK,
	PKT_CMD
};

typedef struct sh_trg_packet {
	uint8_t type;
	uint8_t device_id[16];
	uint32_t ckey;
	uint8_t q_count;
	uint8_t h_count;
	uint8_t hops[MAX_HOPS_DATA];
} sh_trg_packet_t;

typedef struct sh_gps_packet {
	uint8_t type;
	uint8_t device_id[16];
	float lat;
	float lon;
	float alt;
	uint8_t q_count;
	uint8_t h_count;
	uint8_t hops[MAX_HOPS_DATA];
} sh_gps_packet_t;

typedef struct sh_ack_packet {
	uint8_t type;
	uint8_t device_id[16];
	uint8_t q_count;
	uint8_t h_count;
	uint8_t hops[MAX_HOPS_DATA];
} sh_ack_packet_t;

typedef struct sh_info_packet {
	uint8_t type;
	long long device_id;
	long long device_id_2;
} sh_info_packet_t;


typedef struct sh_packet {
	union {
		sh_trg_packet_t trgn;
		sh_trg_packet_t trgc;
		sh_gps_packet_t location;
		sh_ack_packet_t ack;
		sh_info_packet_t info;
	};
} sh_packet_t;

static sh_packet_t sh_queue[10];
static uint8_t sh_num = 0;
static uint8_t sh_start = 0;
static uint8_t sh_total = 10;

static portMUX_TYPE sh_mutex = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t routerHandle = NULL;

bool sendData(sh_packet_t * p_pkt);

uint8_t get_pkt_len(sh_packet_t * p_pkt) {
	switch (p_pkt->trgn.type) {
	case PKT_TRGN: return sizeof(sh_trg_packet_t);
	case PKT_TRGC: return sizeof(sh_trg_packet_t);
	case PKT_ACK: return sizeof(sh_ack_packet_t);
	case PKT_LOC: return sizeof(sh_gps_packet_t);
	default:
		break;
	}
	return 0;
}

uint8_t get_q_count(sh_packet_t * p_pkt) {
	switch (p_pkt->trgn.type) {
	case PKT_TRGN: return p_pkt->trgn.q_count;
	case PKT_TRGC: return p_pkt->trgc.q_count;
	case PKT_ACK: return p_pkt->ack.q_count;
	case PKT_LOC: return p_pkt->location.q_count;
	default:
		break;
	}
	return 0;
}

void set_q_count(sh_packet_t * p_pkt, uint8_t c) {
	switch (p_pkt->trgn.type) {
	case PKT_TRGN: p_pkt->trgn.q_count = c; break;
	case PKT_TRGC: p_pkt->trgc.q_count = c; break;
	case PKT_ACK: p_pkt->ack.q_count = c; break;
	case PKT_LOC: p_pkt->location.q_count = c; break;
	default:
		break;
	}
}

uint8_t get_h_count(sh_packet_t * p_pkt) {
	switch (p_pkt->trgn.type) {
	case PKT_TRGN: return p_pkt->trgn.h_count;
	case PKT_TRGC: return p_pkt->trgc.h_count;
	case PKT_ACK: return p_pkt->ack.h_count;
	case PKT_LOC: return p_pkt->location.h_count;
	default:
		break;
	}
	return 0;
}

uint8_t * get_h_ptr(sh_packet_t * p_pkt) {
	switch (p_pkt->trgn.type) {
	case PKT_TRGN: return &p_pkt->trgn.h_count;
	case PKT_TRGC: return &p_pkt->trgc.h_count;
	case PKT_ACK: return &p_pkt->ack.h_count;
	case PKT_LOC: return &p_pkt->location.h_count;
	default:
		break;
	}
	ESP_LOGI(TAG, "Pkt type: %d", p_pkt->trgn.type);
	return NULL;
}

bool isPresentInQueue(sh_packet_t * p_pkt) {
	bool found = false;
	for (uint8_t i = 0; i < sh_num; i++) {
		uint8_t pos = (sh_start + i) % sh_total;
		if (memcmp(&sh_queue[pos], p_pkt, 17) == 0) {
			found = true;
		}
	}
	return found;
}

void addQueue(sh_packet_t * p_pkt) {
	ESP_LOGI(TAG, "Add begin start:%d, num:%d, Conected:%d", sh_start, sh_num, is_wifi_connected());
	if (p_pkt->trgn.type > PKT_ACK) {
		return;
	}
	uint8_t hc = get_h_count(p_pkt);
	uint8_t qc = get_q_count(p_pkt);
	ESP_LOGI(TAG, "Add begin hc:%d, qc:%d", hc, qc);
	//esp_log_buffer_hex(TAG, p_pkt, sizeof(sh_trg_packet_t));
	if (hc > 10) {
		return;
	}
	if (qc > 3) {
		return;
	}
	set_q_count(p_pkt, qc + 1);
	taskENTER_CRITICAL(&sh_mutex);
	bool found = isPresentInQueue(p_pkt);
	if (!found) {
		uint8_t pos = (sh_start + sh_num) % sh_total;
		sh_queue[pos] = p_pkt[0];
		if (sh_num < sh_total) {
			sh_num ++;
		}
		else {
			sh_start = (sh_start + 1) % sh_total;
		}

	}
	taskEXIT_CRITICAL(&sh_mutex);
	//esp_log_buffer_hex(TAG, p_pkt, sizeof(sh_trg_packet_t));
	//esp_log_buffer_hex(TAG, sh_queue, sizeof(sh_trg_packet_t));
	ESP_LOGI(TAG, "Add end start:%d, num:%d, Conected:%d", sh_start, sh_num, is_wifi_connected());
}

esp_err_t removeQueue(sh_packet_t * p_pkt) {
	ESP_LOGI(TAG, "Remove begin start:%d, num:%d, Conected:%d", sh_start, sh_num, is_wifi_connected());
	esp_err_t status = ESP_OK;
	if (sh_num <= 0) {
		status = ESP_FAIL;
		return status;
	}
	taskENTER_CRITICAL(&sh_mutex);
	if (sh_num > 0) {
		p_pkt[0] = sh_queue[sh_start];
		sh_num --;
		sh_start = (sh_start + 1) % sh_total;
	}
	else {
		status = ESP_FAIL;
	}
	taskEXIT_CRITICAL(&sh_mutex);
	esp_log_buffer_hex(TAG, p_pkt, sizeof(sh_trg_packet_t));
	//esp_log_buffer_hex(TAG, sh_queue, sizeof(sh_trg_packet_t));
	ESP_LOGI(TAG, "Remove end start:%d, num:%d, Conected:%d", sh_start, sh_num, is_wifi_connected());
	return status;
}

void printDataSizes() {
	  printf("sizeof(short): %d\n", (int) sizeof(short));

	  printf("sizeof(int): %d\n", (int) sizeof(int));

	  printf("sizeof(long): %d\n", (int) sizeof(long));

	  printf("sizeof(long long): %d\n", (int) sizeof(long long));

	  printf("sizeof(size_t): %d\n", (int) sizeof(size_t));

	  printf("sizeof(void *): %d\n", (int) sizeof(void *));
}

void router(void *pvParameter)
{
    while(1) {
    	//printDataSizes();
    	ESP_LOGI(TAG, "freemem=%d",esp_get_free_heap_size());
    	sh_packet_t pkt;
    	if (removeQueue(&pkt) == ESP_OK) {
    		if (!sendData(&pkt)) {
    			addQueue(&pkt);
    		}
    	}
    	ESP_LOGI(TAG, "freemem=%d",esp_get_free_heap_size());
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
}


void router_start()
{
	ESP_LOGI(TAG, "Router start:%d, num:%d, Conected:%d", sh_start, sh_num, is_wifi_connected());
	if (routerHandle == NULL) {
		xTaskCreate(&router, "router", 8192,NULL,5, &routerHandle);
	}
}

void router_stop()
{
	ESP_LOGI(TAG, "Router stop:%d, num:%d, Conected:%d", sh_start, sh_num, is_wifi_connected());
	if (routerHandle != NULL) {
		vTaskDelete(routerHandle);
		routerHandle = NULL;
	}
}

void processData(uint8_t *data, int len) {
	sh_packet_t pkt;
	bzero(&pkt, sizeof(pkt));
	memcpy(&pkt, data, len);
	addQueue(&pkt);
	if (pkt.trgn.type == PKT_CMD && len == 2) {
		if (data[1] == 0x00) {
			router_stop();
		}
		if (data[1] == 0x01) {
			router_start();
		}
		if (data[1] == 0x02) {
			start_client_scan();
		}
		if (data[1] == 0x03) {
			stop_client_scan();
		}
		if (data[1] == 0x04) {
			sh_packet_t pkt;
			if (removeQueue(&pkt) == ESP_OK) {
				if (!sendData(&pkt)) {
					addQueue(&pkt);
				}
			}
			//stop_client_scan();
		}
	}
}

void add_hop(void * data, uint8_t h) {
	ESP_LOGI(TAG, "Add hop:%d", h);
	sh_packet_t * p_pkt = (sh_packet_t *) data;
	uint8_t * hptr = get_h_ptr(p_pkt);
	hptr[1 + hptr[0]] = h;
	hptr[0] = (hptr[0]+1) % MAX_HOPS_DATA;
}

bool check_hop(void * data, uint8_t h) {
	sh_packet_t * p_pkt = (sh_packet_t *) data;
	esp_log_buffer_hex(TAG, p_pkt, sizeof(sh_trg_packet_t));
	uint8_t * hptr = get_h_ptr(p_pkt);
	ESP_LOGI(TAG, "h count:0x%x", (int)hptr);
	bool found = false;
	for (uint8_t i = 0; i < hptr[0]; i++) {
		if (hptr[1 + i] == h) {
			found = true;
		}
	}
	return found;
}

void get_hex_buffer(char * str, const void * p, int len) {
	for (uint32_t i = 0; i < len; i++) {
		sprintf(&str[2 * i], "%02x", ((uint8_t*) p)[i]);
	}
}

bool sendData(sh_packet_t * p_pkt) {
	bool status = false;
	if (is_wifi_connected()) {
		char buffer[64];
		get_hex_buffer(buffer, get_h_ptr(p_pkt), MAX_HOPS_DATA + 1);
		switch (p_pkt->trgn.type) {
		case PKT_TRGN:
			status = sendTrigger(p_pkt->info.device_id, p_pkt->trgn.ckey, buffer);
			break;
		case PKT_TRGC:
			status = cancelTrigger(p_pkt->info.device_id, p_pkt->trgc.ckey, buffer);
			break;
		case PKT_ACK: break;;
		case PKT_LOC:
			status = sendLocation(p_pkt->info.device_id, p_pkt->location.lat,
					p_pkt->location.lon, p_pkt->location.alt, buffer);
			break;
		default:
			break;
		}
	}
	if (!status) {
		if (!isSimpleClientConnected()) {
			prepare_to_send_simple((uint8_t*)p_pkt, get_pkt_len(p_pkt));
		}
		if (isSimpleClientReady()) {
			send_packet_simple((uint8_t*)p_pkt, get_pkt_len(p_pkt));
		}
	}
	return false;
}

