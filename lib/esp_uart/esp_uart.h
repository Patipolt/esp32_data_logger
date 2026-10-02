#ifndef ESP_UART_H_
#define ESP_UART_H_

#include "esp_err.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class UART
{
    public:
        // Methods
        UART(gpio_num_t rx, gpio_num_t tx, int baud_rate = 115200, int buf_size = 1024, uart_port_t uart_port = UART_NUM_1);
        ~UART();
        esp_err_t setup(void);
        esp_err_t send(const char *data, size_t length);
        esp_err_t receive(size_t length, TickType_t wait_time = portMAX_DELAY);
        esp_err_t flush(void);
        esp_err_t clear(void);

        // Attributes
        char *m_buffer;


    private:
        // Methods

        // Attributes
        gpio_num_t m_uart_rx;
        gpio_num_t m_uart_tx;
        int m_baud_rate;
        int m_buf_size;
        uart_port_t m_uart_port;
};




#endif // ESP_UART_H_