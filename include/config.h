#ifndef CONFIG_H_
#define CONFIG_H_

// Configurations for LSM6DSO_I2C class
#define LSM6DSO_SDA_PIN       GPIO_NUM_2
#define LSM6DSO_SCL_PIN       GPIO_NUM_1
#define LSM6DSO_I2C_FREQ      400000 // 400kHz
#define LSM6DSO_I2C_PORT      I2C_NUM_0

//Configuration for SD_Card class (Molex 50628DC connector, SPI mode)
#define MISO_PIN        GPIO_NUM_37     // DAT0
#define MOSI_PIN        GPIO_NUM_35     // CMD
#define SCLK_PIN        GPIO_NUM_36     // CLK
#define CS_PIN          GPIO_NUM_38     // DAT3
#define MOUNT_POINT     "/sdcard"
#define BUFFER_SIZE     (32 * 1024)       // Batch SD writes to reduce per-sample overhead
#define SDCARD_USE_PSRAM false             // false uses internal RAM for both buffers
#define MAX_ENTRY_SIZE  64
#define SDCARD_SPI_HOST SPI2_HOST

// Configurations for WiFi hotspot, web page and UDP streaming
#define WIFI_AP_SSID    "IMU_Logger"
#define WIFI_AP_PASS    NULL          // 8-63 characters, empty for an open network
#define UDP_TARGET_IP   "192.168.4.255"     // broadcast on the hotspot subnet
#define UDP_TARGET_PORT 5005

// Configurations for RGB_LED class
#define RGB_LED_PIN     GPIO_NUM_48

// Configurations for Ticker class
#define TICKER_HZ       100                // 100Hz for logging IMU data

#endif // CONFIG_H_
