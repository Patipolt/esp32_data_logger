#ifndef ESP_ONBOARDLED_H_
#define ESP_ONBOARDLED_H_

#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"

#define LED_ON 1
#define LED_BLINK_SLOW 2
#define LED_BLINK_FAST 3
#define LED_BLINK_AIRPLANE_ONCE 4
#define LED_BLINK_AIRPLANE_TWICE 5

class ONBOARD_LED{
public:
    // Methods
    ONBOARD_LED(gpio_num_t pin);
    ~ONBOARD_LED();
    void setPace(int pace);
    static void run_task(void *pvParameter);
    void run();

    // Attributes
    gpio_num_t m_pin; // GPIO pin for the LED
    int m_delay = 0;
    bool m_blink = false;
    bool m_blink_airplane = false;
    int m_airplane_mode = 1;        // 0: AIRPLANR_ONCE, 1: AIRPLANR_TWICE
};

#endif // ESP_ONBOARDLED_H_