#include <stdio.h>
#include "esp_http_client.h"
#include "esp_log.h"


//Include for Change to HTTPS
//#include "esp_tls.h"

//Custom Headerfiles
#include "configuration.h"
#include "ringbuffer.h"

// Defines
static const char *TAG = "HTTP_CLIENT";

//Global Variables
uint sendCounter = 0;


esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGI(TAG, "HTTP_EVENT_REDIRECT");
            break;
    }
    return ESP_OK;
}

void http_setup(void)
{
    esp_err_t err;

    esp_http_client_config_t config = {
        .url = LOCALHOST_ADDRESS,
        .path = "/post",
        .method = HTTP_METHOD_POST,
        .event_handler = _http_event_handler,
        .keep_alive_enable = true,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    char post_data_table[50];
    sprintf(post_data_table, "ID:; sensorwert:; index:");
    //esp_http_client_set_header(client, "Content-Type", "application/csv");
    esp_http_client_set_post_field(client, post_data_table, strlen(post_data_table));

    err = esp_http_client_perform(client);
    if (err == ESP_OK) 
    {
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %"PRId64,
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    }

}

void http_task(ringbuffer_handle_t *buffer)
{
    esp_err_t err;

    esp_http_client_config_t config = {
        .url = LOCALHOST_ADDRESS,
        .path = "/post",
        .method = HTTP_METHOD_POST,
        .event_handler = _http_event_handler,
        .keep_alive_enable = true,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    char post_struct[20];
    uint32_t readData;
    uint sensorId;
    readData = read_from_buffer(buffer);
    sensorId = 1;

    sprintf(post_struct, "%03u; %lu; %u", sensorId, readData, sendCounter);
    sendCounter = sendCounter + 1;
    esp_http_client_set_post_field(client, post_struct, strlen(post_struct));

    err = esp_http_client_perform(client);
    if (err == ESP_OK) 
    {
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %"PRId64,
               esp_http_client_get_status_code(client),
               esp_http_client_get_content_length(client));
    } 
    else 
    {
    ESP_LOGI(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    if(!is_full(buffer))
    {
        sendCounter = 0;
    }


    esp_http_client_cleanup(client);
}