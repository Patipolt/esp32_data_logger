/* This class deals with the SD card adapter for Arduino (HW-125). */

#ifndef ESP_SDCARD_H
#define ESP_SDCARD_H

#include "esp_err.h"             // for ESP_ERROR_CHECK
#include "esp_vfs_fat.h"
#include "driver/gpio.h"         // for GPIO functions
#include "driver/spi_master.h"   // for SPI functions
#include "sdmmc_cmd.h"           // for SDMMC functions
#include "driver/sdspi_host.h"   // for SPI bus configuration

class SD_Card
{
public:
    SD_Card(gpio_num_t miso, gpio_num_t mosi, gpio_num_t sclk, gpio_num_t cs,
            spi_host_device_t spi_host, const char *mount_point);
    ~SD_Card();

    /* Methods */
    esp_err_t init(void);
    esp_err_t write(const char *path, const char *data);

private:
    gpio_num_t m_miso;
    gpio_num_t m_mosi;
    gpio_num_t m_sclk;
    gpio_num_t m_cs;
    spi_host_device_t m_spi_host;
    const char *m_mount_point;
};

#endif // ESP_SDCARD_H