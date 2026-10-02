#ifndef ESP_I2C_H_
#define ESP_I2C_H_

#include "esp_err.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define I2C_MASTER_TX_BUF_DISABLE 0      // I2C master doesn't need buffer
#define I2C_MASTER_RX_BUF_DISABLE 0      // I2C master doesn't need buffer

class I2C 
{
    public:
        // Methods
        I2C(gpio_num_t sda, gpio_num_t scl, uint32_t frequency, i2c_port_t i2c_port = I2C_NUM_0);
        ~I2C();
        esp_err_t setup(void);
        esp_err_t i2c_scan(void);
        gpio_num_t get_sda(void) { return m_sda; }
        gpio_num_t get_scl(void) { return m_scl; }
        i2c_port_t get_i2c_port(void) { return m_i2c_port; }
        uint32_t get_frequency(void) { return m_frequency; }

        // Attributes
        bool isFound;

    private:
        // Methods

        // Attributes
        gpio_num_t m_sda;
        gpio_num_t m_scl;
        i2c_port_t m_i2c_port;
        uint32_t m_frequency;
};

#endif // ESP_I2C_H_