#include "esp_lsm6dso_i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "LSM6DSO_I2C";

LSM6DSO_I2C::LSM6DSO_I2C(
    gpio_num_t sda,
    gpio_num_t scl,
    uint32_t frequency,
    i2c_port_t i2c_port) : m_sda(sda), m_scl(scl), m_frequency(frequency), m_i2c_port(i2c_port) {
    esp_err_t err = I2C_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error initializing I2C bus: %s", esp_err_to_name(err));
        return;
    }
    if (i2c_scan() == ESP_OK) {
        setup();
    }
}

esp_err_t LSM6DSO_I2C::I2C_init() {
    ESP_LOGI(TAG, "Configuring I2C bus: SDA %d, SCL %d", m_sda, m_scl);
    ESP_LOGI(TAG, "I2C clk_speed: %d", (int)m_frequency);
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

    ESP_LOGI(TAG, "I2C bus configured successfully");
    return ESP_OK;
}

// Probes both possible LSM6DSO addresses and checks WHO_AM_I
esp_err_t LSM6DSO_I2C::i2c_scan(void)
{
    ESP_LOGI(TAG, "Looking for LSM6DSO...");
    const uint8_t candidates[] = {LSM6DSO_I2C_ADDRESS_SA0_LOW, LSM6DSO_I2C_ADDRESS_SA0_HIGH};
    for (uint8_t addr : candidates)
    {
        m_address = addr;
        if (readRegister(WHO_AM_I, 1) == ESP_OK)
        {
            if (m_buffer[0] == LSM6DSO_WHO_AM_I_VALUE)
            {
                ESP_LOGI(TAG, "LSM6DSO found at address 0x%02x", addr);
                m_isFound = true;
                return ESP_OK;
            }
            ESP_LOGW(TAG, "Device at 0x%02x has unexpected WHO_AM_I 0x%02x", addr, m_buffer[0]);
        }
    }
    ESP_LOGE(TAG, "LSM6DSO not found");
    m_isFound = false;
    return ESP_FAIL;
}

void LSM6DSO_I2C::setup()
{
    ESP_LOGI(TAG, "Setting up LSM6DSO...");
    // Reset the device
    writeRegister(CTRL3_C, SW_RESET);
    safetyDelay();
    // Block data update and register address auto-increment for multi-byte reads
    writeRegister(CTRL3_C, BDU | IF_INC);
    safetyDelay();
    // Accelerometer: 208 Hz, 2g
    writeRegister(CTRL1_XL, ODR_XL_208HZ);
    safetyDelay();
    setACCConfig(FS_XL_2G);
    safetyDelay();
    // Gyroscope: 208 Hz, 250 dps
    writeRegister(CTRL2_G, ODR_G_208HZ);
    safetyDelay();
    setGYROConfig(FS_G_250DPS);
    safetyDelay();

    ESP_LOGI(TAG, "Setup complete");
}

void LSM6DSO_I2C::safetyDelay()
{
    // Leave time for the device to process the last command
    vTaskDelay(30 / portTICK_PERIOD_MS);
}

int16_t LSM6DSO_I2C::getACCRawX() { return m_ACCrawX; }
int16_t LSM6DSO_I2C::getACCRawY() { return m_ACCrawY; }
int16_t LSM6DSO_I2C::getACCRawZ() { return m_ACCrawZ; }
int16_t LSM6DSO_I2C::getGYRRawX() { return m_GYRrawX; }
int16_t LSM6DSO_I2C::getGYRRawY() { return m_GYRrawY; }
int16_t LSM6DSO_I2C::getGYRRawZ() { return m_GYRrawZ; }
int16_t LSM6DSO_I2C::getTempRaw() { return m_Tempraw; }

float LSM6DSO_I2C::getACCX() { return (float)m_ACCrawX * m_ACCfsr; }
float LSM6DSO_I2C::getACCY() { return (float)m_ACCrawY * m_ACCfsr; }
float LSM6DSO_I2C::getACCZ() { return (float)m_ACCrawZ * m_ACCfsr; }
float LSM6DSO_I2C::getGYRX() { return (float)m_GYRrawX * m_GYRfsr; }
float LSM6DSO_I2C::getGYRY() { return (float)m_GYRrawY * m_GYRfsr; }
float LSM6DSO_I2C::getGYRZ() { return (float)m_GYRrawZ * m_GYRfsr; }

float LSM6DSO_I2C::getTemp()
{
    return ((float)m_Tempraw / 256.0f) + 25.0f; // 256 LSB/°C, 0 LSB at 25°C
}

// Output registers are little-endian (low byte first)
#define LE16(lo) ((int16_t)((uint16_t)m_buffer[(lo) + 1] << 8 | m_buffer[(lo)]))

void LSM6DSO_I2C::readACCX()
{
    readRegister(OUTX_L_A, 2);
    m_ACCrawX = LE16(0);
}

void LSM6DSO_I2C::readACCY()
{
    readRegister(OUTY_L_A, 2);
    m_ACCrawY = LE16(0);
}

void LSM6DSO_I2C::readACCZ()
{
    readRegister(OUTZ_L_A, 2);
    m_ACCrawZ = LE16(0);
}

void LSM6DSO_I2C::readACCAll()
{
    readRegister(OUTX_L_A, 6);
    m_ACCrawX = LE16(0);
    m_ACCrawY = LE16(2);
    m_ACCrawZ = LE16(4);
}

void LSM6DSO_I2C::readGYRX()
{
    readRegister(OUTX_L_G, 2);
    m_GYRrawX = LE16(0);
}

void LSM6DSO_I2C::readGYRY()
{
    readRegister(OUTY_L_G, 2);
    m_GYRrawY = LE16(0);
}

void LSM6DSO_I2C::readGYRZ()
{
    readRegister(OUTZ_L_G, 2);
    m_GYRrawZ = LE16(0);
}

void LSM6DSO_I2C::readGYRAll()
{
    readRegister(OUTX_L_G, 6);
    m_GYRrawX = LE16(0);
    m_GYRrawY = LE16(2);
    m_GYRrawZ = LE16(4);
}

void LSM6DSO_I2C::readTemp()
{
    readRegister(OUT_TEMP_L, 2);
    m_Tempraw = LE16(0);
}

// Temperature, gyro and accel are contiguous from 0x20 to 0x2D
void LSM6DSO_I2C::readAll()
{
    readRegister(OUT_TEMP_L, 14);
    m_Tempraw = LE16(0);
    m_GYRrawX = LE16(2);
    m_GYRrawY = LE16(4);
    m_GYRrawZ = LE16(6);
    m_ACCrawX = LE16(8);
    m_ACCrawY = LE16(10);
    m_ACCrawZ = LE16(12);
}

void LSM6DSO_I2C::setGYROConfig(uint8_t config)
{
    readRegister(CTRL2_G, 1);
    writeRegister(CTRL2_G, (m_buffer[0] & ~FS_G_MASK) | config);
    safetyDelay();

    // Confirm the change
    readRegister(CTRL2_G, 1);
    switch (m_buffer[0] & FS_G_MASK) {
        case FS_G_250DPS:
            ESP_LOGI(TAG, "GYRO config set to 250DPS");
            m_GYRfsr = 0.00875f;
            break;
        case FS_G_500DPS:
            ESP_LOGI(TAG, "GYRO config set to 500DPS");
            m_GYRfsr = 0.0175f;
            break;
        case FS_G_1000DPS:
            ESP_LOGI(TAG, "GYRO config set to 1000DPS");
            m_GYRfsr = 0.035f;
            break;
        case FS_G_2000DPS:
            ESP_LOGI(TAG, "GYRO config set to 2000DPS");
            m_GYRfsr = 0.070f;
            break;
        default:
            ESP_LOGE(TAG, "Invalid GYRO config value: %d", m_buffer[0]);
    }
}

void LSM6DSO_I2C::setACCConfig(uint8_t config)
{
    readRegister(CTRL1_XL, 1);
    writeRegister(CTRL1_XL, (m_buffer[0] & ~FS_XL_MASK) | config);
    safetyDelay();

    // Confirm the change
    readRegister(CTRL1_XL, 1);
    switch (m_buffer[0] & FS_XL_MASK) {
        case FS_XL_2G:
            ESP_LOGI(TAG, "ACC config set to 2G");
            m_ACCfsr = 0.000061f;
            break;
        case FS_XL_4G:
            ESP_LOGI(TAG, "ACC config set to 4G");
            m_ACCfsr = 0.000122f;
            break;
        case FS_XL_8G:
            ESP_LOGI(TAG, "ACC config set to 8G");
            m_ACCfsr = 0.000244f;
            break;
        case FS_XL_16G:
            ESP_LOGI(TAG, "ACC config set to 16G");
            m_ACCfsr = 0.000488f;
            break;
        default:
            ESP_LOGE(TAG, "Invalid ACC config value: %d", m_buffer[0]);
    }
}

esp_err_t LSM6DSO_I2C::readRegister(uint8_t addr, uint8_t nBytes) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (m_address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, addr, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (m_address << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, m_buffer, nBytes, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(m_i2c_port, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

void LSM6DSO_I2C::writeRegister(uint8_t addr, uint8_t msg) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (m_address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, addr, true);
    i2c_master_write_byte(cmd, msg, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(m_i2c_port, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
}

void LSM6DSO_I2C::writeRegister(uint8_t addr, uint8_t* msg, size_t nBytes) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (m_address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, addr, true);
    i2c_master_write(cmd, msg, nBytes, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(m_i2c_port, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
}
