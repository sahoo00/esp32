/*
 * blinky.c
 *
 *  Created on: Apr 10, 2018
 *      Author: Debashis
 */


#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/gpio.h"

#define BLINK_GPIO GPIO_NUM_2

static TaskHandle_t blinkyHandle = NULL;

void blinky(void *pvParameter)
{

    gpio_pad_select_gpio(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
    while(1) {
        /* Blink off (output low) */
        gpio_set_level(BLINK_GPIO, 0);
        vTaskDelay(1000 / portTICK_RATE_MS);
        /* Blink on (output high) */
        gpio_set_level(BLINK_GPIO, 1);
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
}


void blinky_start()
{
	if (blinkyHandle == NULL) {
		xTaskCreate(&blinky, "blinky", 512,NULL,5, &blinkyHandle);
	}
}

void blinky_stop()
{
	if (blinkyHandle != NULL) {
		vTaskDelete(blinkyHandle);
		gpio_set_level(BLINK_GPIO, 0);
		blinkyHandle = NULL;
	}
}
