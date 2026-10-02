#include "esp_i2c.h"

static const char *TAG = "I2C";

I2C::I2C(gpio_num_t sda, gpio_num_t scl, uint32_t frequency, i2c_port_t i2c_port) {
    this->m_sda = sda;
    this->m_scl = scl;
    this->m_i2c_port = i2c_port;
    this->m_frequency = frequency;
}

I2C::~I2C() {}

esp_err_t I2C::setup() {

    ESP_LOGI(TAG, "Configuring I2C bus: SDA %d, SCL %d", m_sda, m_scl);
    ESP_LOGI(TAG, "I2C clk_speed: %d", m_frequency);
    ESP_LOGI(TAG, "I2C port: %d", m_i2c_port);

    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = m_sda;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = m_scl;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = m_frequency;
    conf.clk_flags = 0;

    esp_err_t err = i2c_param_config(m_i2c_port, &conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error configuring I2C bus: %s", esp_err_to_name(err));
        return err;
    }

    err = i2c_driver_install(m_i2c_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error installing I2C driver: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "I2C setup complete.");
    ESP_LOGI(TAG, "I2C bus configured successfully");

    return ESP_OK;
}

esp_err_t I2C::i2c_scan(void)
{
    ESP_LOGI(TAG, "Scanning I2C bus...");
    int devices_found = 0;
    for (int addr = 0; addr < 127; addr++)
    {
        uint8_t add = (uint8_t)addr;
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (add << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin(m_i2c_port, cmd, 1000);
        i2c_cmd_link_delete(cmd);
        if (ret == ESP_OK)
        {
            ESP_LOGI(TAG, "Device found at address 0x%02x", addr);
            devices_found++;
        }
        vTaskDelay(5 / portTICK_PERIOD_MS);  // Let watchdog breathe
    }
    if (devices_found == 0)
    {
        ESP_LOGE(TAG, "No devices found");
        isFound = false;
        return ESP_FAIL;
    } else {
        isFound = true;
    }
    return ESP_OK;
}