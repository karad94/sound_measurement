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

// Custom Headerfiles
#include "configuration.h"
#include "wifi_setting.h"
#include "led_setting.h"
#include "ringbuffer.h"
//#include "http_client.h"

// Global Defines
#define RINGBUFFER_SIZE 250
#define SENSOR_RATE (1000000/44000) // Frequency 44kHz --> ~23 us

// ESP_LOG Tags
const char *tag_ringbuffer1 = "ringbuffer1";
const char *tag_ringbuffer2 = "ringbuffer2";
const char *tag_socket = "Socket";
const char *tag_coap = "CoAP-Client";

// Variables for ISR
int lastBufferWritten = 2;
uint32_t testVar = 0;

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
BaseType_t ISRMutex1 = pdFALSE;
BaseType_t ISRMutex2 = pdFALSE;
BaseType_t MutexHolder1 = pdFALSE;
BaseType_t MutexHolder2 = pdFALSE;


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
void write_task(void *pvParameters);

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

void write_task(void *pvParameters)
{   
    if(lastBufferWritten == 2)
    {
        if(MutexHolder1 == pdFALSE)
        {   
            //printf("ping ISR vor\n");
            if (xSemaphoreTakeFromISR(ringbuffer1_mutex, &ISRMutex1) == pdTRUE)
            {
                MutexHolder1 = pdTRUE;
                if(!is_full(ringbuffer1))
                {
                    write_to_buffer(ringbuffer1, testVar);
                    //ESP_LOGI(tag_ringbuffer1, "First WriteIndex: %d, IsFull: %d, Value: %lu", ringbuffer1->writeIndex, ringbuffer1->full, testVar);
                    testVar = testVar + 1;
                }
                else
                {
                    xSemaphoreGiveFromISR(ringbuffer1_mutex, &ISRMutex1);
                    ISRMutex1 = pdFALSE;
                    MutexHolder1 = pdFALSE;
                }
            }
            else
            {
                testVar = testVar + 1;
            }
        }
        else
        {
            write_to_buffer(ringbuffer1, testVar);
            //ESP_LOGI(tag_ringbuffer1, "WriteIndex: %d, IsFull: %d, Value: %lu", ringbuffer1->writeIndex, ringbuffer1->full, testVar);
            testVar = testVar + 1;

            if(is_full(ringbuffer1))
            {
                xSemaphoreGiveFromISR(ringbuffer1_mutex, &ISRMutex1);
                ISRMutex1 = pdFALSE;
                MutexHolder1 = pdFALSE;
                lastBufferWritten = 1;
                testVar = 100;
            }
        }
    }
    else if(lastBufferWritten == 1)
    {
        if(MutexHolder2 == pdFALSE)
        {
            if(xSemaphoreTakeFromISR(ringbuffer2_mutex, &ISRMutex2) == pdTRUE)
            {
                MutexHolder2 = pdTRUE;
                if(!is_full(ringbuffer2))
                {
                    write_to_buffer(ringbuffer2, testVar);
                    //ESP_LOGI(tag_ringbuffer2, "WriteIndex: %d, IsFull: %d", ringbuffer2->writeIndex, ringbuffer2->full);
                    testVar = testVar + 1;
                }
                else
                {
                    xSemaphoreGiveFromISR(ringbuffer2_mutex, &ISRMutex2);
                    ISRMutex2 = pdFALSE;
                    MutexHolder2 = pdFALSE;
                }
            }
            else
            {
                testVar = testVar + 1;
            }
        }
        else
        {
            write_to_buffer(ringbuffer2, testVar);
            //ESP_LOGI(tag_ringbuffer2, "WriteIndex: %d, IsFull: %d", ringbuffer2->writeIndex, ringbuffer2->full);
            testVar = testVar + 1;

            if(is_full(ringbuffer2))
            {
                xSemaphoreGiveFromISR(ringbuffer2_mutex, &ISRMutex2);
                ISRMutex2 = pdFALSE;
                MutexHolder2 = pdFALSE;
                lastBufferWritten = 2;
                testVar = 0;
            }
        }
    }
}

int init_udp(void)
{
    int init_sock;
    int broadcast_enable = 1;
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

    setsockopt(init_sock, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable));

    
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
        ESP_LOGI(tag_socket, "Received IP: %s and port: %d", inet_ntoa(server_addr.sin_addr), data_port);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(data_port);

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        ESP_LOGI(tag_socket, "Unable to create socket: errno %d", errno);
        return -1;
    }
    ESP_LOGI(tag_socket, "Socket created, connecting to %s, %d", inet_ntoa(server_addr.sin_addr), data_port);

    close(init_sock);

    return 1;
}

void send_task_udp(void *pvParameters)
{
    int err = 0;
    int lastBufferRead = 1;
    uint32_t buffer_array[RINGBUFFER_SIZE] = {0};

    while(1)
    {
        int counter = 0;
        if (lastBufferRead == 2)
        {
            xSemaphoreTake(ringbuffer1_mutex, (TickType_t) portMAX_DELAY);
            if (is_full(ringbuffer1))
            {
                while (is_full(ringbuffer1))
                {
                    buffer_array[counter] = htonl(read_from_buffer(ringbuffer1));
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
                    buffer_array[counter] = htonl(read_from_buffer(ringbuffer2));
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

        err = sendto(sock, &buffer_array, sizeof(buffer_array), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
        if (err < 0)
        {
            ESP_LOGE(tag_socket, "Send failed! err: %d", errno);
            perror("Fehlertext");
        }
        else
        {
            ESP_LOGI(tag_socket, "Successfully send");
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

void app_main(void)
{
    //Init of Timer
    esp_timer_handle_t sensor_timer;
    esp_timer_create_args_t sensor_timer_args = {
        .callback = &write_task,
        .arg = NULL,
        .name = "sensor_timer",
    };

    // Initialisation of the LED, NVS, WIFI
    config_led();
    init_nvs();
    wifi_init_sta();
    ringbuffer1_mutex = xSemaphoreCreateMutex();
    ringbuffer2_mutex = xSemaphoreCreateMutex();
    //esp_log_level_set(tag_socket, ESP_LOG_ERROR);

    if(init_udp() < 0)
    {
        ESP_LOGE(tag_socket, "Register at Server failed");
        return;
    }
    else
    {
        ESP_LOGI(tag_socket, "Register at Server successfull");
    }

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

    esp_timer_create(&sensor_timer_args, &sensor_timer);
    esp_timer_start_periodic(sensor_timer, SENSOR_RATE);

    xTaskCreatePinnedToCore(&send_task_udp, "send_task_udp", 4096, NULL, 5, NULL, 1);
   
}
