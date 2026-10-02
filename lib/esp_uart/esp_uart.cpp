#include "esp_uart.h"

static const char *TAG = "UART";

UART::UART(gpio_num_t rx, gpio_num_t tx, int baud_rate, int buf_size, uart_port_t uart_port) {
    this->m_uart_rx = rx;
    this->m_uart_tx = tx;
    this->m_baud_rate = baud_rate;
    this->m_buf_size = buf_size;
    this->m_uart_port = uart_port;
    this->m_buffer = new char[buf_size]; // Allocate buffer
    if (this->m_buffer == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate buffer for UART");
        return;
    }
}

UART::~UART() {
    delete[] m_buffer;
}

esp_err_t UART::setup() {
    
    ESP_LOGI(TAG, "UART configuring: TX %d, RX %d, Baud %d", m_uart_tx, m_uart_rx, m_baud_rate);
    ESP_LOGI(TAG, "UART buffer size: %d", m_buf_size);
    ESP_LOGI(TAG, "UART port: %d", m_uart_port);
    vTaskDelay(50 / portTICK_PERIOD_MS); 

    uart_config_t uart_config = {};
    uart_config.baud_rate = m_baud_rate;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity    = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;

    esp_err_t err = uart_param_config(m_uart_port, &uart_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error configuring UART: %s", esp_err_to_name(err));
        return err;
    }

    err = uart_set_pin(m_uart_port, m_uart_tx, m_uart_rx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error setting UART pins: %s", esp_err_to_name(err));
        return err;
    }

    err = uart_driver_install(m_uart_port, m_buf_size * 2, m_buf_size * 2, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error installing UART driver: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "UART setup complete.");
    ESP_LOGI(TAG, "UART configured successfully");

    return ESP_OK;
}

esp_err_t UART::send(const char *data, size_t length) {
    return uart_write_bytes(m_uart_port, data, length);
}

esp_err_t UART::receive(size_t length, TickType_t wait_time) {
    return uart_read_bytes(m_uart_port, m_buffer, length, wait_time);
}

esp_err_t UART::flush(void) {
    return uart_flush(m_uart_port);
}

esp_err_t UART::clear(void) {
    return uart_flush_input(m_uart_port);
}



