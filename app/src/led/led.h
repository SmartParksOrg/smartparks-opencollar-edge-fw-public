/** @file led.h
 *
 * @brief File containing interface for led and led events
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2023 Irnas. All rights reserved.
 */

#ifndef LED_H
#define LED_H

#include <zephyr/kernel.h>

/* Public struct defines */
enum led_color {
	LED_R = 0,   /* Red */
	LED_G = 1,   /* Green */
	LED_B = 2,   /* Blue */
	LED_ALL = 3, /* All colors - White */
	LED_M = 4,   /* Magenta */
	LED_C = 5,   /* Cyan */
	LED_Y = 6,   /* Yellow */
};

/*!
 * @brief Init LED.
 *
 *
 * @return negative error code, 0 is successful.
 */
int led_init(void);

/**
 * @brief Enable or disable functionality of all led lights.
 * If any led is turned on, it will be turned off beforehand.
 * This will not alter NVS setting.
 *
 * @param[in] stat on/off
 */
void led_change_status(bool stat);

/**
 * @brief Turn on single LED.
 * First turn other colors off.
 * Turn LED on and change our led state and color.
 *
 * @param[in] color
 */
void led_turn_on(enum led_color color);

/**
 * @brief Turn off all leds.
 *
 * @param[in] color
 */
void led_turn_off(enum led_color color);

/**
 * @brief Blink LED of specific color number of times.
 *
 * @param n - number  of times to blink LED
 * @param color - led color
 */
void led_blink(uint8_t n, enum led_color color);

/**
 * @brief Blink LED on set interval.
 *
 * @param[in] change_interval - blink interval in ms
 * @param[in] color - led color
 */
void led_blink_interval(uint32_t change_interval, enum led_color color);

/**
 * @brief Control led based on the current system state.
 *
 */
void led_handler(void);

/**
 * @brief Get the led state object based on color
 *
 * @param[in] color led color
 * @return uint8_t state on (1)/ off (0)
 */
uint8_t led_get_state(enum led_color color);

#endif // LED_H
