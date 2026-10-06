#include "esp_sdCard.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include <cstdlib>
#include <cstring>
#include <unistd.h>

bool verbose = false;

static const char *TAG = "SD_CARD";

SD_Card::SD_Card(gpio_num_t miso, gpio_num_t mosi, gpio_num_t sclk, gpio_num_t cs,
                  spi_host_device_t spi_host, const char *mount_point,
                  size_t buffer_size, bool use_psram)
    : m_miso(miso), m_mosi(mosi), m_sclk(sclk), m_cs(cs),
      m_spi_host(spi_host), m_mount_point(mount_point), m_buffers{nullptr, nullptr},
      m_buffer_size(buffer_size), m_use_psram(use_psram), m_buffer_used(0),
      m_active_buffer(-1),
      m_open_path(nullptr), m_write_queue(nullptr), m_free_queue(nullptr),
      m_flush_done(nullptr), m_writer_task(nullptr), m_last_error(ESP_OK) {}

SD_Card::~SD_Card() {
    if (m_writer_task != nullptr) {
        flush();
        WriteJob stop_job = {-1, 0, m_flush_done, true};
        xQueueSend(m_write_queue, &stop_job, portMAX_DELAY);
        xSemaphoreTake(m_flush_done, portMAX_DELAY);
        m_writer_task = nullptr;
    }
    if (m_write_queue != nullptr) {
        vQueueDelete(m_write_queue);
    }
    if (m_free_queue != nullptr) {
        vQueueDelete(m_free_queue);
    }
    if (m_flush_done != nullptr) {
        vSemaphoreDelete(m_flush_done);
    }
    free(m_open_path);
    for (size_t i = 0; i < BUFFER_COUNT; ++i) {
        heap_caps_free(m_buffers[i]);
    }
}

esp_err_t SD_Card::init(void){
    esp_err_t ret;
    sdmmc_card_t* card;
    ESP_LOGI(TAG, "Initializing SD card");

    // Use settings defined above to initialize SPI bus
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.max_freq_khz = 15000;

    /* Initialize the SPI bus */
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = m_mosi,
        .miso_io_num = m_miso,
        .sclk_io_num = m_sclk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    ret = spi_bus_initialize(m_spi_host, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bus: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize the SD card
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = m_cs;
    slot_config.host_id = m_spi_host;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
        /* Explanation of Allocation Unit Size
        Allocation Unit Size (Cluster Size): This is the smallest amount of disk space that can be allocated 
        to a file. Even if a file is only 1 byte, it will still occupy an entire cluster. 
        Choosing a larger cluster size can reduce the overhead of the filesystem but may result 
        in wasted space if you have many small files. Conversely, a smaller cluster size can be 
        more space-efficient for small files but may result in higher overhead.
        Value in the Code: 16 * 1024 equals 16,384 bytes or 16 KB. 
        This is a common cluster size for FAT filesystems on larger storage devices, 
        balancing between efficiency and space utilization.*/
    };
    ESP_LOGI(TAG, "Mounting SD card");

    ret = esp_vfs_fat_sdspi_mount(m_mount_point, &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount filesystem: %s", esp_err_to_name(ret));
        return ret;
    }

    // Print card info if initialization was successful
    sdmmc_card_print_info(stdout, card);

    // Allocate buffers for asynchronous writing
    if (m_buffer_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint32_t memory_caps = MALLOC_CAP_8BIT |
        (m_use_psram ? MALLOC_CAP_SPIRAM : MALLOC_CAP_INTERNAL);
    const size_t available_memory = heap_caps_get_free_size(memory_caps);
    const size_t required_memory = BUFFER_COUNT * m_buffer_size;
    if (available_memory < required_memory) {
        ESP_LOGE(TAG, "Not enough %s for SD buffers: need %u, available %u bytes",
                 m_use_psram ? "PSRAM" : "internal RAM",
                 static_cast<unsigned>(required_memory),
                 static_cast<unsigned>(available_memory));
        return ESP_ERR_NO_MEM;
    }

    for (size_t i = 0; i < BUFFER_COUNT; ++i) {
        m_buffers[i] = static_cast<char *>(heap_caps_malloc(m_buffer_size, memory_caps));
        if (m_buffers[i] == nullptr) {
            ESP_LOGE(TAG, "Failed to allocate SD write buffer %u in %s",
                     static_cast<unsigned>(i),
                     m_use_psram ? "PSRAM" : "internal RAM");
            return ESP_ERR_NO_MEM;
        }
    }

    m_write_queue = xQueueCreate(BUFFER_COUNT + 1, sizeof(WriteJob));
    m_free_queue = xQueueCreate(BUFFER_COUNT, sizeof(int));
    m_flush_done = xSemaphoreCreateBinary();
    if (m_write_queue == nullptr || m_free_queue == nullptr || m_flush_done == nullptr) {
        ESP_LOGE(TAG, "Failed to create SD writer synchronization objects");
        return ESP_ERR_NO_MEM;
    }

    for (int i = 0; i < static_cast<int>(BUFFER_COUNT); ++i) {
        xQueueSend(m_free_queue, &i, 0);
    }
    xQueueReceive(m_free_queue, &m_active_buffer, 0);

    BaseType_t task_created = xTaskCreatePinnedToCore(
        writerTaskEntry, "sd_writer", 4096, this, 3, &m_writer_task, 1);
    if (task_created != pdPASS) {
        m_writer_task = nullptr;
        ESP_LOGE(TAG, "Failed to create SD writer task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Asynchronous SD writer started with two %u-byte buffers in %s",
             static_cast<unsigned>(m_buffer_size),
             m_use_psram ? "PSRAM" : "internal RAM");
    return ESP_OK;
}

esp_err_t SD_Card::writeBatch(int buffer_index, size_t length) {
    if (m_open_path == nullptr || buffer_index < 0 || length == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    FILE *file = fopen(m_open_path, "a");
    if (file == nullptr) {
        if (verbose) {
            ESP_LOGE(TAG, "Failed to open %s for batch write", m_open_path);
        }
        
        return ESP_FAIL;
    }

    const size_t written = fwrite(m_buffers[buffer_index], 1, length, file);
    if (written != length) {
        fclose(file);
        if (verbose) {
            ESP_LOGE(TAG, "SD batch write failed (%u/%u bytes)",
                     static_cast<unsigned>(written), static_cast<unsigned>(length));
        }
        return ESP_FAIL;
    }

    bool commit_failed = false;
    if (fflush(file) != 0) {
        commit_failed = true;
    } else if (fsync(fileno(file)) != 0) {
        commit_failed = true;
    }
    if (fclose(file) != 0) {
        commit_failed = true;
    }
    if (commit_failed) {
        if (verbose) {
            ESP_LOGE(TAG, "Failed to commit SD file %s", m_open_path);
        }
        return ESP_FAIL;
    }

    if (verbose) {
        ESP_LOGI(TAG, "Committed %u bytes to %s",
                 static_cast<unsigned>(length), m_open_path);
    }

    return ESP_OK;
}

void SD_Card::writerTaskEntry(void *arg) {
    static_cast<SD_Card *>(arg)->writerTask();
}

void SD_Card::writerTask(void) {
    WriteJob job;
    while (xQueueReceive(m_write_queue, &job, portMAX_DELAY) == pdTRUE) {
        if (job.stop) {
            if (job.completion != nullptr) {
                xSemaphoreGive(job.completion);
            }
            vTaskDelete(nullptr);
            return;
        }

        if (job.buffer_index >= 0) {
            esp_err_t result = writeBatch(job.buffer_index, job.length);
            if (result != ESP_OK) {
                m_last_error.store(result);
            }
            xQueueSend(m_free_queue, &job.buffer_index, portMAX_DELAY);
        }

        if (job.completion != nullptr) {
            xSemaphoreGive(job.completion);
        }
    }
}

esp_err_t SD_Card::queueActiveBuffer(void) {
    if (m_active_buffer < 0 || m_buffer_used == 0) {
        return ESP_OK;
    }

    WriteJob job = {m_active_buffer, m_buffer_used, nullptr, false};
    if (xQueueSend(m_write_queue, &job, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }

    m_active_buffer = -1;
    m_buffer_used = 0;
    if (xQueueReceive(m_free_queue, &m_active_buffer, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t SD_Card::flush(void) {
    if (m_writer_task == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = queueActiveBuffer();
    if (ret != ESP_OK) {
        return ret;
    }

    // A barrier job waits until all earlier queued writes have completed.
    WriteJob barrier = {-1, 0, m_flush_done, false};
    if (xQueueSend(m_write_queue, &barrier, portMAX_DELAY) != pdTRUE ||
        xSemaphoreTake(m_flush_done, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }

    return static_cast<esp_err_t>(m_last_error.exchange(ESP_OK));
}

esp_err_t SD_Card::write(const char *path, const char *data) {
    if (path == nullptr || data == nullptr || m_writer_task == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    if (m_open_path == nullptr) {
        m_open_path = strdup(path);
        if (m_open_path == nullptr) {
            return ESP_ERR_NO_MEM;
        }
    } else if (strcmp(path, m_open_path) != 0) {
        esp_err_t ret = flush();
        if (ret != ESP_OK) {
            return ret;
        }
        char *new_path = strdup(path);
        if (new_path == nullptr) {
            return ESP_ERR_NO_MEM;
        }
        free(m_open_path);
        m_open_path = new_path;
    }

    size_t data_remaining = strlen(data);
    while (data_remaining > 0) {
        const size_t free_space = m_buffer_size - m_buffer_used;
        const size_t copy_size = data_remaining < free_space ? data_remaining : free_space;
        memcpy(m_buffers[m_active_buffer] + m_buffer_used, data, copy_size);
        m_buffer_used += copy_size;
        data += copy_size;
        data_remaining -= copy_size;

        if (m_buffer_used == m_buffer_size) {
            esp_err_t ret = queueActiveBuffer();
            if (ret != ESP_OK) {
                return ret;
            }
        }
    }

    return static_cast<esp_err_t>(m_last_error.exchange(ESP_OK));
}
