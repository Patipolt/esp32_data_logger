#include "esp_mpu6050_i2c.h"

static const char *TAG = "MPU6050_I2C";

MPU6050_I2C::MPU6050_I2C(
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

esp_err_t MPU6050_I2C::I2C_init() {
    // Configure the I2C bus
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

    // // Set I²C timeout after driver installation (timeout is set in APB clock cycles)
    // // Use I2C_TIMEOUT_MS value to calculate appropriate APB clock cycle timeout
    // uint32_t timeout_cycles = 1000 * 80;  // 80 MHz APB clock
    // i2c_set_timeout(m_i2c_port, timeout_cycles);

    ESP_LOGI(TAG, "I2C bus configured successfully");
    
    return ESP_OK;
}

esp_err_t MPU6050_I2C::i2c_scan(void)
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
            m_scanned_addr = addr;
            devices_found++;
        }
        vTaskDelay(5 / portTICK_PERIOD_MS);  // Let watchdog breathe
    }
    if (devices_found == 0)
    {
        ESP_LOGE(TAG, "No devices found");
        m_isFound = false;
        return ESP_FAIL;
    } else {
        m_isFound = true;
    }
    return ESP_OK;
}

void MPU6050_I2C::setup()
{
    ESP_LOGI(TAG, "Setting up MPU6050...");
    // Reset the device
    writeRegister(PWR_MGMT_1, 0x80); // Reset device
    safetyDelay();
    // Wake up the device
    writeRegister(PWR_MGMT_1, 0x01); // Set clock source to PLL with X axis gyroscope reference
    safetyDelay();
    // Set the sample rate to 250 Hz
    writeRegister(SMPLRT_DIV, 0x03);
    safetyDelay();
    // Set the gyroscope configuration
    setGYROConfig(FS_SEL_250DPS); // Set full scale range to 250 degrees/sec
    safetyDelay();
    // Set the accelerometer configuration
    setACCConfig(AFS_SEL_2G); // Set full scale range to 2g
    safetyDelay();
    // Set the digital low pass filter configuration
    writeRegister(CONFIG, DLPF_CFG_184HZ_188HZ_NA);
    safetyDelay();

    ESP_LOGI(TAG, "Setup complete");
    return;
}

void MPU6050_I2C::safetyDelay()
{
    // Delay for 30 ms to leave time for the device to process the last command
    vTaskDelay(30 / portTICK_PERIOD_MS);
    return;
}

uint16_t MPU6050_I2C::getACCRawX()
{
    return m_ACCrawX;
}

uint16_t MPU6050_I2C::getACCRawY()
{
    return m_ACCrawY;
}

uint16_t MPU6050_I2C::getACCRawZ()
{
    return m_ACCrawZ;
}

uint16_t MPU6050_I2C::getGYRRawX()
{
    return m_GYRrawX;
}

uint16_t MPU6050_I2C::getGYRRawY()
{
    return m_GYRrawY;
}

uint16_t MPU6050_I2C::getGYRRawZ()
{
    return m_GYRrawZ;
}

uint16_t MPU6050_I2C::getTempRaw()
{
    return m_Tempraw;
}

float MPU6050_I2C::getACCX()
{
    return (float)((int16_t)m_ACCrawX)*m_ACCfsr;
}

float MPU6050_I2C::getACCY()
{
    return (float)((int16_t)m_ACCrawY)*m_ACCfsr;
}

float MPU6050_I2C::getACCZ()
{
    return (float)((int16_t)m_ACCrawZ)*m_ACCfsr;
}

float MPU6050_I2C::getGYRX()
{
    return (float)((int16_t)m_GYRrawX)*m_GYRfsr;
}

float MPU6050_I2C::getGYRY()
{
    return (float)((int16_t)m_GYRrawY)*m_GYRfsr;
}

float MPU6050_I2C::getGYRZ()
{
    return (float)((int16_t)m_GYRrawZ)*m_GYRfsr;
}

float MPU6050_I2C::getTemp()
{
    return ((float)((int16_t)m_Tempraw)/340) + 36.53; // Temperature in Celsius, on page 31
}

void MPU6050_I2C::readACCX()
{
    readRegister(ACCEL_XOUT_H, 2);
    m_ACCrawX = (uint16_t)(m_buffer[0] << 8 | m_buffer[1]);
    return;
}

void MPU6050_I2C::readACCY()
{
    readRegister(ACCEL_YOUT_H, 2);
    m_ACCrawY = (uint16_t)(m_buffer[0] << 8 | m_buffer[1]);
    return;
}

void MPU6050_I2C::readACCZ()
{
    readRegister(ACCEL_ZOUT_H, 2);
    m_ACCrawZ = (uint16_t)(m_buffer[0] << 8 | m_buffer[1]);
    return;
}

void MPU6050_I2C::readACCAll()
{
    readRegister(ACCEL_XOUT_H, 6);
    m_ACCrawX = (uint16_t)(m_buffer[0] << 8 | m_buffer[1]);
    m_ACCrawY = (uint16_t)(m_buffer[2] << 8 | m_buffer[3]);
    m_ACCrawZ = (uint16_t)(m_buffer[4] << 8 | m_buffer[5]);
    return;
}

void MPU6050_I2C::readGYRX()
{
    readRegister(GYRO_XOUT_H, 2);
    m_GYRrawX = (uint16_t)(m_buffer[0] << 8 | m_buffer[1]);
    return;
}

void MPU6050_I2C::readGYRY()
{
    readRegister(GYRO_YOUT_H, 2);
    m_GYRrawY = (uint16_t)(m_buffer[0] << 8 | m_buffer[1]);
    return;
}

void MPU6050_I2C::readGYRZ()
{
    readRegister(GYRO_ZOUT_H, 2);
    m_GYRrawZ = (uint16_t)(m_buffer[0] << 8 | m_buffer[1]);
    return;
}   

void MPU6050_I2C::readGYRAll()
{
    readRegister(GYRO_XOUT_H, 6);
    m_GYRrawX = (uint16_t)(m_buffer[0] << 8 | m_buffer[1]);
    m_GYRrawY = (uint16_t)(m_buffer[2] << 8 | m_buffer[3]);
    m_GYRrawZ = (uint16_t)(m_buffer[4] << 8 | m_buffer[5]);
    return;
}

void MPU6050_I2C::readTemp()
{
    readRegister(TEMP_OUT_H, 2);
    m_Tempraw = (uint16_t)(m_buffer[0] << 8 | m_buffer[1]);
    return;
}

void MPU6050_I2C::readAll()
{
    readRegister(ACCEL_XOUT_H, 14);
    m_ACCrawX = (uint16_t)(m_buffer[0] << 8 | m_buffer[1]);
    m_ACCrawY = (uint16_t)(m_buffer[2] << 8 | m_buffer[3]);
    m_ACCrawZ = (uint16_t)(m_buffer[4] << 8 | m_buffer[5]);
    m_Tempraw = (uint16_t)(m_buffer[6] << 8 | m_buffer[7]);
    m_GYRrawX = (uint16_t)(m_buffer[8] << 8 | m_buffer[9]);
    m_GYRrawY = (uint16_t)(m_buffer[10] << 8 | m_buffer[11]);
    m_GYRrawZ = (uint16_t)(m_buffer[12] << 8 | m_buffer[13]);
    return;
}

void MPU6050_I2C::setGYROConfig(uint8_t config)
{
    readRegister(GYRO_CONFIG, 1);
    char msg = (m_buffer[0] & ~FS_SEL_MASK) | config;
    writeRegister(GYRO_CONFIG, msg);
    safetyDelay();

    // Confirm the change
    readRegister(GYRO_CONFIG, 1);
    if (m_buffer[0] == FS_SEL_250DPS) {
        ESP_LOGI(TAG, "GYRO config set to 250DPS");
        m_GYRfsr = 250.0 / 32768.0;
    } else if (m_buffer[0] == FS_SEL_500DPS) {
        ESP_LOGI(TAG, "GYRO config set to 500DPS");
        m_GYRfsr = 500.0 / 32768.0;
    } else if (m_buffer[0] == FS_SEL_1000DPS) {
        ESP_LOGI(TAG, "GYRO config set to 1000DPS");
        m_GYRfsr = 1000.0 / 32768.0;
    } else if (m_buffer[0] == FS_SEL_2000DPS) {
        ESP_LOGI(TAG, "GYRO config set to 2000DPS");
        m_GYRfsr = 2000.0 / 32768.0;
    } else {
        ESP_LOGE(TAG, "Invalid GYRO config value: %d", m_buffer[0]);
    }
}

void MPU6050_I2C::setACCConfig(uint8_t config)
{
    readRegister(ACCEL_CONFIG, 1);
    char msg = (m_buffer[0] & ~AFS_SEL_MASK) | config;
    writeRegister(ACCEL_CONFIG, msg);
    safetyDelay();

    // Confirm the change
    readRegister(ACCEL_CONFIG, 1);
    if (m_buffer[0] == AFS_SEL_2G) {
        ESP_LOGI(TAG, "ACC config set to 2G");
        m_ACCfsr = 2.0 / 32768.0;
    } else if (m_buffer[0] == AFS_SEL_4G) {
        ESP_LOGI(TAG, "ACC config set to 4G");
        m_ACCfsr = 4.0 / 32768.0;
    } else if (m_buffer[0] == AFS_SEL_8G) {
        ESP_LOGI(TAG, "ACC config set to 8G");
        m_ACCfsr = 8.0 / 32768.0;
    } else if (m_buffer[0] == AFS_SEL_16G) {
        ESP_LOGI(TAG, "ACC config set to 16G");
        m_ACCfsr = 16.0 / 32768.0;
    } else {
        ESP_LOGE(TAG, "Invalid ACC config value: %d", m_buffer[0]);
    }
}

void MPU6050_I2C::readRegister(uint8_t addr, uint8_t nBytes) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPU6050_I2C_ADDRESS<<1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, addr, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPU6050_I2C_ADDRESS<<1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, (uint8_t *)m_buffer, nBytes, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(m_i2c_port, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
}

void MPU6050_I2C::writeRegister(uint8_t addr, uint8_t msg) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPU6050_I2C_ADDRESS<<1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, addr, true);
    i2c_master_write_byte(cmd, msg, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(m_i2c_port, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
}

void MPU6050_I2C::writeRegister(uint8_t addr, uint8_t* msg, size_t nBytes) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPU6050_I2C_ADDRESS<<1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, addr, true);
    i2c_master_write(cmd, msg, nBytes, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(m_i2c_port, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
}