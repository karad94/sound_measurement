/**
 * @file http_client.h
 * @author Adam Karsten (a.karsten@ostfalia.de)
 * @brief Setup HTTP Client on ESP32 and send data to Host
 * @version 0.1
 * @date 2023-11-27
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef __HTTP_CLIENT_H__
#define __HTTP_CLIENT_H__

/**
 * @brief Event Handler for HTTP Client. Will Print in Log which Event ist currently running
 * 
 * @param evt Pointer to Eventgroup
 * @return ESP_OK
 */
esp_err_t _http_event_handler(esp_http_client_event_t *evt);

/**
 * @brief Setup HTTP Client on ESP an post first Line of csv
 * 
 */
void http_setup(void);

/**
 * @brief Transform Ringbuffer to string and send to Host
 * 
 * @param buffer Ringbuffer which should be send to Host
 */
void http_task(ringbuffer_handle_t *buffer);

#endif