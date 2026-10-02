#include "esp_onboardLed.h"

static const char *TAG = "ONBOARD_LED";

ONBOARD_LED::ONBOARD_LED(gpio_num_t pin) : m_pin(pin) {
    // Setup onboard LED pins D4, D5
    gpio_set_direction(m_pin, GPIO_MODE_OUTPUT);
    gpio_set_level(m_pin, 0);
    setPace(LED_BLINK_AIRPLANE_TWICE);
    run();
}

ONBOARD_LED::~ONBOARD_LED() {}

void ONBOARD_LED::setPace(int pace) {
    switch (pace) {
        case LED_ON:
            m_blink = false;
            m_blink_airplane = false;
            m_delay = 0;
            break;
        case LED_BLINK_SLOW:
            m_blink = true;
            m_blink_airplane = false;
            m_delay = 500;
            break;
        case LED_BLINK_FAST:
            m_blink = true;
            m_blink_airplane = false;
            m_delay = 100;
            break;
        case LED_BLINK_AIRPLANE_ONCE:
            m_blink = false;
            m_blink_airplane = true;
            m_airplane_mode = 0; // AIRPLANR_ONCE
            m_delay = 2000;
            break;
        case LED_BLINK_AIRPLANE_TWICE:
            m_blink = false;
            m_blink_airplane = true;
            m_airplane_mode = 1; // AIRPLANR_TWICE
            m_delay = 2000;
            break;
        default:
            m_blink = false;
            m_blink_airplane = true;
            m_airplane_mode = 1; // AIRPLANR_TWICE
            m_delay = 2000;
    }
}

void ONBOARD_LED::run_task(void *pvParameter) {
    ONBOARD_LED *led = static_cast<ONBOARD_LED*>(pvParameter);
    while (true) {
        if (led->m_blink == false && led->m_blink_airplane == false) {
            gpio_set_level(led->m_pin, 1); // Turn on the LED
            vTaskDelay(500 / portTICK_PERIOD_MS);  
        }
        else if (led->m_blink == true && led->m_blink_airplane == false) {
            gpio_set_level(led->m_pin, 1); // Turn on the LED
            vTaskDelay(led->m_delay / portTICK_PERIOD_MS);
            gpio_set_level(led->m_pin, 0); // Turn off the LED
            vTaskDelay(led->m_delay / portTICK_PERIOD_MS);
        }
        else if (led->m_blink_airplane == true) {
            if(led->m_airplane_mode == 0) {
                gpio_set_level(led->m_pin, 1); // Turn on the LED
                vTaskDelay(50 / portTICK_PERIOD_MS);
                gpio_set_level(led->m_pin, 0); // Turn off the LED
                vTaskDelay(led->m_delay / portTICK_PERIOD_MS);
            } else if (led->m_airplane_mode == 1) {
                gpio_set_level(led->m_pin, 1); // Turn on the LED
                vTaskDelay(10 / portTICK_PERIOD_MS);
                gpio_set_level(led->m_pin, 0); // Turn off the LED
                vTaskDelay(100 / portTICK_PERIOD_MS);
                gpio_set_level(led->m_pin, 1); // Turn on the LED
                vTaskDelay(50 / portTICK_PERIOD_MS);
                gpio_set_level(led->m_pin, 0); // Turn off the LED
                vTaskDelay(led->m_delay / portTICK_PERIOD_MS);
            }
        }
    }
}

void ONBOARD_LED::run() {
    xTaskCreate(run_task, "Onboard_LED_task", 1024, this, 5, NULL);
    ESP_LOGI(TAG, "Onboard LED task created");
}