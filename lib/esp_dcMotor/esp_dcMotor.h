#ifndef ESP_DCMOTOR_H_
#define ESP_DCMOTOR_H_

#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "string.h"

class DCMOTOR{
public:
    // Methods
    DCMOTOR(gpio_num_t pwm_gpio_1, gpio_num_t pwm_gpio_2, ledc_channel_t channel, ledc_timer_t timer, ledc_mode_t mode, uint32_t freq, uint32_t resolution, uint32_t max_duty, bool direction_pin = false);
    ~DCMOTOR();
    void setDuty(uint32_t duty, char *direction); // Set duty cycle for PWM
    void run(); // Start the PWM
    void stop(); // Stop the PWM

    // Attributes
    bool m_direction_pin = false; // Flag to indicate if direction pin is used
    ledc_mode_t m_mode; // PWM mode
    ledc_channel_t m_channel; // PWM channel
    ledc_channel_t m_channel_2; // Second PWM channel (if used)
    gpio_num_t m_dir_gpio; // PWM GPIO number

    char m_direction[10] = "FORWARD"; // Direction string
};

#endif // ESP_DCMOTOR_H_