#ifndef ESP_SSD1306_128X64_H_
#define ESP_SSD1306_128X64_H_

#include "esp_err.h"
#include "esp_log.h"
#include "I2C.h"
#include "freertos/FreeRTOS.h"
#include "u8g2_esp32_hal.h"
#include "u8g2.h"  // Include U8G2 header
#include <cstring>
#include <cmath>
#include <string>

class SSD1306_128X64
{
    public:
        // Constructor and Destructor
        SSD1306_128X64(
            gpio_num_t sda, 
            gpio_num_t scl, 
            uint32_t frequency, 
            i2c_port_t i2c_port, 
            uint8_t address);
        ~SSD1306_128X64();

        // Methods
        esp_err_t init(void);
        void clear_display(void);
        void display_message(const char *message);
        void display_image(int x, int y, uint16_t width, uint16_t height, const uint8_t *image);

    private:
        // Attributes
        static I2C *m_i2c;
        uint8_t m_address;
        u8g2_t m_u8g2;  // U8G2 object for the display
};


#endif // ESP_SSD1306_128X64_H_