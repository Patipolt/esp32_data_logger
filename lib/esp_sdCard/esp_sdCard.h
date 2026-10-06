/* This class deals with the SD card adapter for Arduino (HW-125). */

#ifndef ESP_SDCARD_H
#define ESP_SDCARD_H

#include <atomic>
#include <cstddef>

#include "esp_err.h"             // for ESP_ERROR_CHECK
#include "esp_vfs_fat.h"
#include "driver/gpio.h"         // for GPIO functions
#include "driver/spi_master.h"   // for SPI functions
#include "sdmmc_cmd.h"           // for SDMMC functions
#include "driver/sdspi_host.h"   // for SPI bus configuration
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

class SD_Card
{
public:
    SD_Card(gpio_num_t miso, gpio_num_t mosi, gpio_num_t sclk, gpio_num_t cs,
            spi_host_device_t spi_host, const char *mount_point,
            size_t buffer_size = 16 * 1024, bool use_psram = false);
    ~SD_Card();

    /* Methods */
    esp_err_t init(void);
    esp_err_t write(const char *path, const char *data);
    esp_err_t flush(void);

private:
    static constexpr size_t BUFFER_COUNT = 2;

    struct WriteJob {
        int buffer_index;
        size_t length;
        SemaphoreHandle_t completion;
        bool stop;
    };

    gpio_num_t m_miso;
    gpio_num_t m_mosi;
    gpio_num_t m_sclk;
    gpio_num_t m_cs;
    spi_host_device_t m_spi_host;
    const char *m_mount_point;
    char *m_buffers[BUFFER_COUNT];
    size_t m_buffer_size;
    bool m_use_psram;
    size_t m_buffer_used;
    int m_active_buffer;
    char *m_open_path;
    QueueHandle_t m_write_queue;
    QueueHandle_t m_free_queue;
    SemaphoreHandle_t m_flush_done;
    TaskHandle_t m_writer_task;
    std::atomic<int> m_last_error;

    esp_err_t queueActiveBuffer(void);
    esp_err_t writeBatch(int buffer_index, size_t length);
    static void writerTaskEntry(void *arg);
    void writerTask(void);
};

#endif // ESP_SDCARD_H
