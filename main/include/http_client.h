#ifndef __HTTP_CLIENT_H__
#define __HTTP_CLIENT_H__

#include "esp_http_client.h"

// Custom Headerfiles
#include "startup.h"

esp_err_t _http_event_handler(esp_http_client_event_t *evt);
void http_setup(void);
void http_task(ringbuffer_handle_t *buffer);

#endif