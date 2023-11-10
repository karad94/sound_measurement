#include "led_strip.h"
#include "esp_log.h"


const char *TAG_LED = "LED";

led_strip_handle_t led_light;

void config_led(void)
{
    ESP_LOGI(TAG_LED, "LED-setup started");

    led_strip_config_t strip_config = {
        .strip_gpio_num = GPIO_NUM_48,
        .max_leds = 1,
    };

    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, //10 MHz
        .flags.with_dma = false,
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_light));

    led_strip_clear(led_light);

    ESP_LOGI(TAG_LED, "LED-setup finished");
}

void set_led(uint32_t r, uint32_t g, uint32_t b)
{
    led_strip_set_pixel(led_light, 0, r, g, b);
    led_strip_refresh(led_light);
    ESP_LOGI(TAG_LED, "LED refreshed");
}
