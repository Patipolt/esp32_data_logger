#include "esp_sdCard.h"
#include "esp_log.h"

static const char *TAG = "SD_CARD";

SD_Card::SD_Card(gpio_num_t miso, gpio_num_t mosi, gpio_num_t sclk, gpio_num_t cs,
                  spi_host_device_t spi_host, const char *mount_point)
    : m_miso(miso), m_mosi(mosi), m_sclk(sclk), m_cs(cs),
      m_spi_host(spi_host), m_mount_point(mount_point) {}

SD_Card::~SD_Card() {}

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

    return ESP_OK;
}

esp_err_t SD_Card::write(const char *path, const char *data){
    FILE *f = fopen(path, "a");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return ESP_FAIL;
    }
    fputs(data, f); // fputs (not fprintf) since data is untrusted, arbitrary content
    fclose(f);

    return ESP_OK;
}