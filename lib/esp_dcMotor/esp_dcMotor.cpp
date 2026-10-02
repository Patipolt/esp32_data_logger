#include "esp_dcMotor.h"

static const char *TAG = "DCMOTOR";

DCMOTOR::DCMOTOR(gpio_num_t pwm_gpio_1, gpio_num_t pwm_gpio_2, ledc_channel_t channel, ledc_timer_t timer, ledc_mode_t mode, uint32_t freq, uint32_t resolution, uint32_t max_duty, bool direction_pin) 
    : m_direction_pin(direction_pin), m_mode(mode), m_channel(channel), m_channel_2((ledc_channel_t)(channel + 1)), m_dir_gpio(pwm_gpio_2) // Use the next channel for the second PWM pin
{
    // Initialize the PWM and direction GPIOs
    ledc_timer_config_t ledc_timer = {};
    ledc_timer.speed_mode = m_mode,
    ledc_timer.duty_resolution = (ledc_timer_bit_t)resolution,
    ledc_timer.timer_num = timer,
    ledc_timer.freq_hz = freq,
    ledc_timer.clk_cfg = LEDC_AUTO_CLK;
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {};
    ledc_channel.gpio_num = pwm_gpio_1;
    ledc_channel.speed_mode = m_mode;
    ledc_channel.channel = m_channel;
    ledc_channel.intr_type = LEDC_INTR_DISABLE;
    ledc_channel.timer_sel = timer;
    ledc_channel.duty = 0;
    ledc_channel.hpoint = 0;
    ledc_channel.flags.output_invert = false; // No output invert
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    if (m_direction_pin) {
        gpio_set_direction(pwm_gpio_2, GPIO_MODE_OUTPUT);
        gpio_set_level(pwm_gpio_2, 0); // Set initial level to 0
    }
    else {
        // set up a channel for the second PWM pin
        ledc_channel_config_t ledc_channel_2 = {};
        ledc_channel_2.gpio_num = pwm_gpio_2;
        ledc_channel_2.speed_mode = m_mode;
        ledc_channel_2.channel = m_channel_2;
        ledc_channel_2.intr_type = LEDC_INTR_DISABLE;
        ledc_channel_2.timer_sel = timer;
        ledc_channel_2.duty = 0;
        ledc_channel_2.hpoint = 0;
        ledc_channel_2.flags.output_invert = false; // No output invert
        ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_2));

    }
    
    if (m_direction_pin) {
        ESP_LOGI(TAG, "PWM initialized on GPIO %d with direction GPIO %d", pwm_gpio_1, pwm_gpio_2);
    } else {
        ESP_LOGI(TAG, "PWM initialized on GPIO %d and %d", pwm_gpio_1, pwm_gpio_2);
    }
}

DCMOTOR::~DCMOTOR() {
    // Cleanup code if needed
    ESP_LOGI(TAG, "PWM destroyed");
}

void DCMOTOR::setDuty(uint32_t duty, char *direction) {
    // copy direct to m_direction for later use
    strncpy(m_direction, direction, sizeof(m_direction) - 1);
    if (m_direction_pin) {
        if (strcmp(m_direction, "FORWARD") == 0) {
            ledc_set_duty(m_mode, m_channel, duty);
        } else if (strcmp(m_direction, "BACKWARD") == 0) {
            ledc_set_duty(m_mode, m_channel, duty);
        } else {
            ESP_LOGE(TAG, "Invalid direction: %s", direction);
            return;
        }
    }
    else {
        if (strcmp(m_direction, "FORWARD") == 0) {
            ledc_set_duty(m_mode, m_channel, duty);
            ledc_set_duty(m_mode, m_channel_2, 0); // Set second channel to 0 duty
        } else if (strcmp(m_direction, "BACKWARD") == 0) {
            ledc_set_duty(m_mode, m_channel, 0); // Set first channel to 0 duty
            ledc_set_duty(m_mode, m_channel_2, duty);
        } else {
            ESP_LOGE(TAG, "Invalid direction: %s", m_direction);
            return;
        }
    }
}

void DCMOTOR::run() {
    if (m_direction_pin) {
        if (strcmp(m_direction, "FORWARD") == 0) {
            ledc_update_duty(m_mode, m_channel);
            gpio_set_level(m_dir_gpio, 1); // Set direction pin to HIGH
        } else if (strcmp(m_direction, "BACKWARD") == 0) {
            ledc_update_duty(m_mode, m_channel);
            gpio_set_level(m_dir_gpio, 0); // Set direction pin to LOW
        } else {
            ESP_LOGE(TAG, "Invalid direction: %s", m_direction);
            return;
        }
    } else {
        ledc_update_duty(m_mode, m_channel);
        ledc_update_duty(m_mode, m_channel_2);
    }
}

void DCMOTOR::stop() {
    if (m_direction_pin) {
        ledc_set_duty(m_mode, m_channel, 0); // Set duty cycle to 0%
    } else {
        ledc_set_duty(m_mode, m_channel, 0); // Set first channel to 0 duty
        ledc_set_duty(m_mode, m_channel_2, 0); // Set second channel to 0 duty
    }
    ledc_update_duty(m_mode, m_channel);
    ledc_update_duty(m_mode, m_channel_2);
}
    