#ifndef __WIFI_SETTING_H__
#define __WIFI_SETTING_H__

#include "startup.h"

void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
void wifi_init_sta(void);

#endif