/**
 * @file main.c
 * @author Adam Karsten (a.karsten@ostfalia.de)
 * @brief Task to send Sensordata via Wifi to Host
 * @version 0.5
 * @date 2023-12-09
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <stdio.h>
#include "nvs_flash.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_system.h"
#include <sys/socket.h>
#include "coap3/coap.h"
#include "lwip/sockets.h"
#include "driver/gptimer.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"

// Custom Headerfiles
#include "configuration.h"
#include "wifi_setting.h"
#include "led_setting.h"
#include "ringbuffer.h"
//#include "http_client.h"

// Global Defines
#define RINGBUFFER_SIZE 1250
#define SENSOR_RATE (1000000/44000) // Frequency 44kHz --> ~23 us

// ESP_LOG Tags
const char *tag_ringbuffer1 = "ringbuffer1";
const char *tag_ringbuffer2 = "ringbuffer2";
const char *tag_socket = "Socket";
const char *tag_coap = "CoAP-Client";
const char *tag_sntp = "SNTP";
const char *tag_debug = "Debug Info";

// Variables for ISR
int lastBufferWritten = 2;
uint32_t testVar = 0;

// Stopwatch
gptimer_handle_t stopwatchtimer = NULL;
gptimer_config_t timer_config_stopwatch = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000, // 1MHz, 1 tick=1us
        };

// UDP Variables
int sock;
struct sockaddr_in server_addr;
socklen_t server_addr_len = sizeof(server_addr);

//CoAP Variables
coap_context_t *coap_context;
coap_session_t *coap_session;
coap_address_t coap_address;
coap_pdu_t *coap_message;

// Global Ringbuffers
ringbuffer_handle_t *ringbuffer1;
ringbuffer_handle_t *ringbuffer2;

// Mutexes for ringbuffer
SemaphoreHandle_t ringbuffer1_mutex;
SemaphoreHandle_t ringbuffer2_mutex;
BaseType_t ISRMutex = pdFALSE;
BaseType_t MutexHolder1 = pdFALSE;
BaseType_t MutexHolder2 = pdFALSE;

// Time Variables
time_t now;
struct tm timeinfo = {0};
time_t measuringStart;

/**
 * @brief Initialize non-volatile Storage to manage Wifi-Storage
 * 
 */
void init_nvs(void);

/**
 * @brief Check for free Buffer and write Sensordata to Buffer until buffer_full == true
 * 
 * @param pvParameters NULL
 */
bool write_task(void);

/**
 * @brief Initialize UDP Broadcast Socket
 * 
 */
int init_udp(void);

/**
 * @brief If Buffer is full, the Buffer will be send to Socket.
 * 
 * @param pvParameters NULL
 */
void send_task_udp(void *pvParameters);

/**
 * @brief Initialize CoAP Client and Connect to Local Server
 * 
 */
void init_coap(void);

/**
 * @brief Copy Ringbuffer to Array and send Array via CoAP to Local Server
 * 
 * @param pvParameters NULL
 */
void send_task_coap(void *pvParameters);

/**
 * @brief Function to Sync Localtime with Server via SNTP
 * 
 * @return -1 if failed / 1 if Timesync is successfull
 */
int obtain_time(void);


// CODE
void init_nvs(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

bool write_task(void)
{   
    ISRMutex = pdFALSE;
    testVar = testVar + 1;
    if(testVar == 50000)
    {
        testVar = 0;
    }
    if(lastBufferWritten == 2)
    {
        if(MutexHolder1 == pdFALSE)
        {   
            if (xSemaphoreTakeFromISR(ringbuffer1_mutex, &ISRMutex) == pdTRUE)
            {
                MutexHolder1 = pdTRUE;
                if(!is_full(ringbuffer1))
                {
                    write_to_buffer(ringbuffer1, testVar);
                }
                else
                {
                    xSemaphoreGiveFromISR(ringbuffer1_mutex, &ISRMutex);
                    MutexHolder1 = pdFALSE;
                }
            }
        }
        else
        {
            write_to_buffer(ringbuffer1, testVar);
            if(is_full(ringbuffer1))
            {
                xSemaphoreGiveFromISR(ringbuffer1_mutex, &ISRMutex);
                MutexHolder1 = pdFALSE;
                lastBufferWritten = 1;
            }
        }
    }
    else if(lastBufferWritten == 1)
    {
        if(MutexHolder2 == pdFALSE)
        {
            if(xSemaphoreTakeFromISR(ringbuffer2_mutex, &ISRMutex) == pdTRUE)
            {
                MutexHolder2 = pdTRUE;
                if(!is_full(ringbuffer2))
                {
                    write_to_buffer(ringbuffer2, testVar);
                }
                else
                {
                    xSemaphoreGiveFromISR(ringbuffer2_mutex, &ISRMutex);
                    MutexHolder2 = pdFALSE;
                }
            }
        }
        else
        {
            write_to_buffer(ringbuffer2, testVar);

            if(is_full(ringbuffer2))
            {
                xSemaphoreGiveFromISR(ringbuffer2_mutex, &ISRMutex);
                MutexHolder2 = pdFALSE;
                lastBufferWritten = 2;
            }
        }
    }

    return ISRMutex == pdFALSE;
}

int init_udp(void)
{
    int init_sock;
    struct sockaddr_in init_addr;
    int err;
    int reg_message = 0;
    int data_port = 0;

    init_addr.sin_addr.s_addr = inet_addr(LOCALHOST_ADDRESS);
    init_addr.sin_family = AF_INET;
    init_addr.sin_port = htons(LOCALHOST_PORT);

    init_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        ESP_LOGI(tag_socket, "Unable to create socket: errno %d", errno);
        return -1;
    }
    ESP_LOGI(tag_socket, "Socket created, connecting to %s, %d", LOCALHOST_ADDRESS, LOCALHOST_PORT);
    
    err = sendto(init_sock, &reg_message, sizeof(reg_message), 0, (struct sockaddr *)&init_addr, sizeof(init_addr));
    if(err < 0)
    {
        ESP_LOGE(tag_socket, "Failed to send Register!");
    }
    else{
        ESP_LOGI(tag_socket, "Register send");
    }

    err = recvfrom(init_sock, &data_port, sizeof(data_port), 0, (struct sockaddr *)&server_addr, &server_addr_len);
    if(err < 0)
    {
        ESP_LOGE(tag_socket, "Failed to receive Register!");
    }
    else{
        ESP_LOGI(tag_socket, "Received IP: %s and port: %d", inet_ntoa(server_addr.sin_addr), ntohs(data_port));
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = data_port;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        ESP_LOGI(tag_socket, "Unable to create socket: errno %d", errno);
        return -1;
    }
    ESP_LOGI(tag_socket, "Socket created, connecting to %s, %d", inet_ntoa(server_addr.sin_addr), ntohs(data_port));

    //close(init_sock);

    return 1;
}

void send_task_udp(void *pvParameters)
{
    int err = 0;
    int lastBufferRead = 2;
    uint32_t buffer_array[RINGBUFFER_SIZE * 2] = {0};
    int counter = 0;
    uint64_t time_elapsed;

    while(1)
    {
        if (lastBufferRead == 2)
        {
            xSemaphoreTake(ringbuffer1_mutex, (TickType_t) portMAX_DELAY);
            //gptimer_set_raw_count(stopwatchtimer, 0);
            if (is_full(ringbuffer1))
            {
                while (is_full(ringbuffer1))
                {
                    buffer_array[counter] = htonl(read_from_buffer(ringbuffer1));
                    counter++;
                }
                //gptimer_get_raw_count(stopwatchtimer, &time_elapsed);
                xSemaphoreGive(ringbuffer1_mutex);
                //ESP_LOGI(tag_debug, "Time Buffer 1 blocked Send_task: %llu us", time_elapsed);
            }
            else
            {
                xSemaphoreGive(ringbuffer1_mutex);
            }
            lastBufferRead = 1;
        }
        else if (lastBufferRead == 1)
        {
            xSemaphoreTake(ringbuffer2_mutex, (TickType_t) portMAX_DELAY);
            //gptimer_set_raw_count(stopwatchtimer, 0);
            if (is_full(ringbuffer2))
            {
                while (is_full(ringbuffer2))
                {
                    buffer_array[counter] = htonl(read_from_buffer(ringbuffer2));
                    counter++;
                    //ESP_LOGI(tag_ringbuffer2, "ReadIndex: %d, IsFull: %d", ringbuffer2->readIndex, ringbuffer2->full);
                }
                //gptimer_get_raw_count(stopwatchtimer, &time_elapsed);  
                xSemaphoreGive(ringbuffer2_mutex);
                //ESP_LOGI(tag_debug, "Time Buffer 2 blocked Send_task: %llu us", time_elapsed);       
            }
            else
            {
                xSemaphoreGive(ringbuffer2_mutex);
            }
            lastBufferRead = 2;
            counter = 0;

            gptimer_get_raw_count(stopwatchtimer, &time_elapsed);
            err = sendto(sock, &buffer_array, sizeof(buffer_array), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
            gptimer_set_raw_count(stopwatchtimer, 0);    
            if (err < 0)
            {
                ESP_LOGE(tag_socket, "Send failed! err: %d", errno);
                perror("Fehlertext");
            }
            else
            {
                ESP_LOGI(tag_debug, "Time to send UDP-Package: %llu us", time_elapsed);
                ESP_LOGI(tag_socket, "Successfully send");
            }     
        }        
                              
    }
}

void init_coap(void)
{
    coap_startup();

    coap_context = coap_new_context(NULL);

    coap_address_init(&coap_address);
    coap_address.addr.sin.sin_family = AF_INET;
    coap_address.addr.sin.sin_addr.s_addr = inet_addr(COAP_SERVERADDRESS);
    coap_address.addr.sin.sin_port = htons(COAP_PORT);

    coap_session = coap_new_client_session(coap_context, NULL, &coap_address, COAP_PROTO_UDP);
    if(coap_session == NULL)
    {
        ESP_LOGE(tag_coap, "Failed to create CoAP Session!");
    }
    else
    {
        ESP_LOGI(tag_coap, "CoAP Session created!");
    }

}

void send_task_coap(void *pvParameters)
{
    int lastBufferRead = 1;
    uint32_t buffer_array[RINGBUFFER_SIZE] = {0};
    uint8_t payload[sizeof(buffer_array)];
    uint8_t content_type_buffer[2];
    size_t content_type_length = coap_encode_var_safe(content_type_buffer, sizeof(content_type_buffer), COAP_MEDIATYPE_APPLICATION_OCTET_STREAM);

    init_coap();

    while(1)
    {
        int counter = 0;
        coap_message = coap_new_pdu(COAP_MESSAGE_NON, COAP_REQUEST_CODE_POST, coap_session);
        coap_add_option(coap_message, COAP_OPTION_CONTENT_TYPE, content_type_length, content_type_buffer);
        
        if (lastBufferRead == 2)
        {
            xSemaphoreTake(ringbuffer1_mutex, (TickType_t) portMAX_DELAY);
            if (is_full(ringbuffer1))
            {
                while (is_full(ringbuffer1))
                {
                    buffer_array[counter] = read_from_buffer(ringbuffer1);
                    counter++;
                    //ESP_LOGI(tag_ringbuffer1, "ReadIndex: %d, IsFull: %d", ringbuffer1->readIndex, ringbuffer1->full);
                }
                xSemaphoreGive(ringbuffer1_mutex);    
            }
            else
            {
                xSemaphoreGive(ringbuffer1_mutex);
            }
            lastBufferRead = 1;
        }
        else if (lastBufferRead == 1)
        {
            xSemaphoreTake(ringbuffer2_mutex, (TickType_t) portMAX_DELAY);
            if (is_full(ringbuffer2))
            {
                while (is_full(ringbuffer2))
                {
                    buffer_array[counter] = read_from_buffer(ringbuffer2);
                    counter++;
                    //ESP_LOGI(tag_ringbuffer2, "ReadIndex: %d, IsFull: %d", ringbuffer2->readIndex, ringbuffer2->full);
                }
                xSemaphoreGive(ringbuffer2_mutex);           
            }
            else
            {
                xSemaphoreGive(ringbuffer2_mutex);
            }
            lastBufferRead = 2;    
        }

        memcpy(payload, buffer_array, sizeof(buffer_array));

        coap_add_data(coap_message, sizeof(payload), payload);

        if(coap_send(coap_session, coap_message) == COAP_INVALID_MID)
        {
            ESP_LOGE(tag_coap, "Send Failed!");
        }
        else
        {
            ESP_LOGI(tag_coap, "Send Successfull!");
        }


    }
}

int obtain_time(void)
{
    int retry = 0;
    int retry_max = 10;

    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, inet_ntoa(server_addr.sin_addr));
    esp_sntp_init();
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);

    while (timeinfo.tm_year < (2016 - 1900) && retry++ < retry_max)
    {
        ESP_LOGI(tag_sntp, "Wait for SNTP Synchronisation...");
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    if(retry < retry_max)
    {
        ESP_LOGI(tag_sntp, "SNTP Sync successfull");
        ESP_LOGI(tag_sntp, "Current Servertime is %s", asctime(&timeinfo));
        return 1;
    }
    else
    {
        ESP_LOGE(tag_sntp, "Failed to get Servertime");
        return -1;
    }
}

void app_main(void)
{

    // Initialisation of the LED, NVS, WIFI
    config_led();
    init_nvs();
    wifi_init_sta();

    ringbuffer1 = init_buffer(RINGBUFFER_SIZE);
    if (ringbuffer1)
    {
        ESP_LOGI(tag_ringbuffer1, "Ringbuffer 1 created succesfully!");
    }

    ringbuffer2 = init_buffer(RINGBUFFER_SIZE);
    if (ringbuffer2)
    {
        ESP_LOGI(tag_ringbuffer2, "Ringbuffer 2 created succesfully!");
    }

    ringbuffer1_mutex = xSemaphoreCreateMutex();
    ringbuffer2_mutex = xSemaphoreCreateMutex();
    esp_log_level_set(tag_socket, ESP_LOG_ERROR);

    gptimer_handle_t writetimer = NULL;
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000, // 1MHz, 1 tick=1us
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &writetimer));
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config_stopwatch, &stopwatchtimer));

    gptimer_event_callbacks_t cbs = {
        .on_alarm = write_task,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(writetimer, &cbs, NULL));

    gptimer_alarm_config_t alarm_config = {
    .reload_count = 0,
    .alarm_count = SENSOR_RATE,
    .flags.auto_reload_on_alarm = true,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(writetimer, &alarm_config));

    if(init_udp() < 0)
    {
        ESP_LOGE(tag_socket, "Register at Server failed");
        return;
    }
    else
    {
        ESP_LOGI(tag_socket, "Register at Server successfull");
        
        if(obtain_time() > 0)
        {

            gptimer_enable(stopwatchtimer);
            gptimer_start(stopwatchtimer);
            gptimer_enable(writetimer);
            ESP_ERROR_CHECK(gptimer_start(writetimer)); 

            xTaskCreate(&send_task_udp, "send_task_udp", 15000, NULL, 0, NULL);
        }
    }
   
}
