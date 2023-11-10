#ifndef __STARTUP_H__
#define __STARTUP_H__

//Global Headerfiles

#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <stdlib.h>
#include <ctype.h>
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "ringbuffer.h"


/*
    Change Settings to your local WIFI AP & Webserver
*/
#define ESP_WIFI_SSID "XXXX"
#define ESP_WIFI_PASS "XXXX"

#define HOSTNAME_WEBSERVER "XXXX"



#endif