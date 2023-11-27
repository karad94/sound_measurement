/**
 * @file led_setting.h
 * @author Adam Karsten (a.karsten@ostfalia.de)
 * @brief Control RGB-LED on ESP32-S3 Devkit
 * @version 0.1
 * @date 2023-11-27
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef __LED_SETTING_H__
#define __LED_SETTING_H__

/**
 * @brief Init of RGB and Turn of, if LED was on in Last-Mode
 * 
 */
void config_led(void);

/**
 * @brief Set the colour of LED
 * 
 * @param r Red Colour (0 - 255)
 * @param g Green Colour (0 - 255)
 * @param b Blue Colour (0 - 255)
 */
void set_led(uint32_t r, uint32_t g, uint32_t b);

#endif