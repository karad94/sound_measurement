/**
 * @file wifi_setting.h
 * @author ESP32 Example Code https://github.com/espressif/esp-idf/tree/8fc8f3f47997aadba21facabc66004c1d22de181/examples/wifi/getting_started/station
 * @brief Init Wifi in Station Mode and Connect to Accesspoint given from configuration.h
 * @version 1.0
 * @date 2023-11-27
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef __WIFI_SETTING_H__
#define __WIFI_SETTING_H__

/**
 * @brief Eventhandler for WiFi Connection. Will be called by event_handler_register
 * 
 * @param arg pointer to event_handler_arg
 * @param event_base Event base of Type esp_event_base_t
 * @param event_id Event ID
 * @param event_data Pointer to Data that will be read out of Event
 */
void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

/**
 * @brief Initialiaze WiFi Station Mode of ESP32 and Connect to WiFi Access Point
 * 
 */
void wifi_init_sta(void);

#endif