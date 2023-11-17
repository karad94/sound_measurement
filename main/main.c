/*
    Program to get Sensordata and send them via WiFi to localhost
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

// Header is used for commuicating with Webserver in TCP or UDP
#include <sys/socket.h>

// Custom Headerfiles
#include "configuration.h"
#include "wifi_setting.h"
#include "led_setting.h"
#include "ringbuffer.h"
//#include "http_client.h"

#define RINGBUFFER_SIZE 50
#define SENSOR_RATE (1000000/44000) // Frequency 44kHz --> ~23 us

const char *tag_ringbuffer1 = "ringbuffer1";
const char *tag_ringbuffer2 = "ringbuffer2";
const char *tag_tcp = "TCP";

// Global Variables
ringbuffer_handle_t *ringbuffer1;
ringbuffer_handle_t *ringbuffer2;
int lastBufferWritten = 2;
uint32_t testVar = 0;
int sock;

// Mutexes for ringbuffer
SemaphoreHandle_t ringbuffer1_mutex;
SemaphoreHandle_t ringbuffer2_mutex;

void write_task(void *pvParameters)
{   
    // Writing to first ringbuffer
    // First Check which Buffer was written last / INIT-Value lastBufferValue = 2
    if(lastBufferWritten == 2)
    {
        // Check if Function is Mutex-Holder of Ringbuffer
        if(!xSemaphoreGetMutexHolder(ringbuffer1_mutex))
        {
            // If not Mutex-Holder, Mutex will be taken an then it will be checked, if the Buffer is full
            // If Buffer is full the Mutex will be released, else a Sensorvalue will be written to Buffer
            xSemaphoreTake(ringbuffer1_mutex, (TickType_t) portMAX_DELAY);
            if(!is_full(ringbuffer1))
            {
                write_to_buffer(ringbuffer1, testVar);
                //ESP_LOGI(tag_ringbuffer1, "First WriteIndex: %d, IsFull: %d, Value: %lu", ringbuffer1->writeIndex, ringbuffer1->full, testVar);
                testVar = testVar + 1;
            }
            else
            {
                xSemaphoreGive(ringbuffer1_mutex);
            }
        }
        // When the Mutex is hold by Task, a Sensorvalue will be written to Buffer
        // When Buffer is full, Mutex will be given and lastBufferWritten set to 1
        else
        {
            write_to_buffer(ringbuffer1, testVar);
            //ESP_LOGI(tag_ringbuffer1, "WriteIndex: %d, IsFull: %d, Value: %lu", ringbuffer1->writeIndex, ringbuffer1->full, testVar);
            testVar = testVar + 1;

            if(is_full(ringbuffer1))
            {
                xSemaphoreGive(ringbuffer1_mutex);
                lastBufferWritten = 1;
                testVar = 0;
            }
        }
    }

    // Writing to second Ringbuffer
    // Function same es for Ringbuffer 1
    if(lastBufferWritten == 1)
    {
        if(!xSemaphoreGetMutexHolder(ringbuffer2_mutex))
        {
            xSemaphoreTake(ringbuffer2_mutex, (TickType_t) portMAX_DELAY);
            if(!is_full(ringbuffer2))
            {
                write_to_buffer(ringbuffer2, testVar);
                //ESP_LOGI(tag_ringbuffer2, "WriteIndex: %d, IsFull: %d", ringbuffer2->writeIndex, ringbuffer2->full);
                testVar = testVar + 1;
            }
            else
            {
                xSemaphoreGive(ringbuffer2_mutex);
            }
        }
        else
        {
            write_to_buffer(ringbuffer2, testVar);
            //ESP_LOGI(tag_ringbuffer2, "WriteIndex: %d, IsFull: %d", ringbuffer2->writeIndex, ringbuffer2->full);
            testVar = testVar + 1;

            if(is_full(ringbuffer2))
            {
                xSemaphoreGive(ringbuffer2_mutex);
                lastBufferWritten = 2;
                testVar = 0;
            }
        }
    }

}

// Initialize Non-Volantile-Storage
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

// Function to Create TCP WebSocket Connection to Host
void connect_tcp(void)
{
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(LOCALHOST_ADDRESS);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(LOCALHOST_PORT);


    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        ESP_LOGI(tag_tcp, "Unable to create socket: errno %d", errno);
        return;
    }
    ESP_LOGI(tag_tcp, "Socket created, connecting to %s, %d", LOCALHOST_ADDRESS, 80);

    int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err == ESP_OK)
    {
        ESP_LOGI(tag_tcp, "Successfully connected");
    }

    //close(sock);
}

void tcp_send(void *pvParameters)
{
    int err;
    int lastBufferRead = 2;
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
        
        if (lastBufferRead == 1)
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

        for(int i = 0; i < RINGBUFFER_SIZE; i++)
        {
            buffer_array[i] = htonl(buffer_array[i]); 
        }                              
        err = send(sock, &buffer_array, sizeof(buffer_array), 0);
        if (err < 0)
        {
            ESP_LOGE(tag_tcp, "Send failed! errno: %d", errno);
        }
        else
        {
            ESP_LOGI(tag_tcp, "Successfully send");
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

    // Initialisation of the LED, NVS, WIFI and the HTTP-Socket
    config_led();
    init_nvs();
    //Test for NETIF
    esp_netif_init();
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

    connect_tcp();

    ringbuffer1_mutex = xSemaphoreCreateMutex();
    ringbuffer2_mutex = xSemaphoreCreateMutex();

    esp_timer_create(&sensor_timer_args, &sensor_timer);
    esp_timer_start_periodic(sensor_timer, SENSOR_RATE);

    xTaskCreatePinnedToCore(&tcp_send, "tcp_send", 4096, NULL, 5, NULL, 1);
    
}
