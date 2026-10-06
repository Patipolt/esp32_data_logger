#include <cstdio>
#include <cstring>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "config.h"
#include "esp_lsm6dso_i2c.h"
#include "esp_sdCard.h"
#include "esp_ticker.h"
#include "esp_wifiManager.h"
#include "esp_webServer.h"
#include "esp_rgbLed.h"

static const char *TAG = "MAIN";

static LSM6DSO_I2C *imu = nullptr;
static SD_Card *sdCard = nullptr;
static WIFI_MANAGER wifi;
static WEB_SERVER web;
static RGB_LED *rgb_led = nullptr;
static Ticker ticker;

const char *log_path = MOUNT_POINT "/imu_log.csv";
char line[MAX_ENTRY_SIZE];
int count = 0;

struct ImuSample {
    volatile float accX, accY, accZ;
    volatile float gyrX, gyrY, gyrZ;
};
static ImuSample latest = {};

// Runs on the HTTP server task
static int imuJson(char *buf, size_t len, void *)
{
    return snprintf(buf, len,
                    "{\"accX\":%.3f,\"accY\":%.3f,\"accZ\":%.3f,\"gyrX\":%.2f,\"gyrY\":%.2f,\"gyrZ\":%.2f}",
                    latest.accX, latest.accY, latest.accZ, latest.gyrX, latest.gyrY, latest.gyrZ);
}

bool setup()
{
    imu = new LSM6DSO_I2C(LSM6DSO_SDA_PIN, LSM6DSO_SCL_PIN, LSM6DSO_I2C_FREQ, LSM6DSO_I2C_PORT);
    sdCard = new SD_Card(MISO_PIN, MOSI_PIN, SCLK_PIN, CS_PIN,
                         SDCARD_SPI_HOST, MOUNT_POINT, BUFFER_SIZE,
                         SDCARD_USE_PSRAM);
    rgb_led = new RGB_LED(RGB_LED_PIN, 25);

    rgb_led->setPattern(RGB_COLOR_WHITE, RGB_PACE_SOLID);

    if (wifi.startAP(WIFI_AP_SSID, WIFI_AP_PASS) == ESP_OK) {
        web.start(imuJson);
        ESP_LOGI(TAG, "Open http://%s in a browser", wifi.getIP());
    } else {
        ESP_LOGE(TAG, "WiFi AP failed, logging to SD only");
        rgb_led->setPattern(RGB_COLOR_BLUE, RGB_PACE_BLINK_SLOW);
    }

    if (!imu->isFound()) {
        ESP_LOGE(TAG, "IMU not found, halting");
        rgb_led->setPattern(RGB_COLOR_MAGENTA, RGB_PACE_BLINK_FAST);
        return false;
    }
    if (sdCard->init() != ESP_OK) {
        ESP_LOGE(TAG, "SD card init failed, halting");
        rgb_led->setPattern(RGB_COLOR_RED, RGB_PACE_BLINK_FAST);
        return false;
    }

    rgb_led->setPattern(RGB_COLOR_GREEN, RGB_PACE_SOLID);

    return true;
}


extern "C" void app_main()
{    
    if (!setup()) {
        ESP_LOGE(TAG, "Failed to setup");
        return;
    }
    int64_t lastTime = esp_timer_get_time();
    ticker.start(TICKER_HZ);
    while(1)
    {
        ticker.wait();
        count++;
        imu->readAll();
        float accX = imu->getACCX(), accY = imu->getACCY(), accZ = imu->getACCZ();
        float gyrX = imu->getGYRX(), gyrY = imu->getGYRY(), gyrZ = imu->getGYRZ();
        latest.accX = accX; 
        latest.accY = accY; 
        latest.accZ = accZ;
        latest.gyrX = gyrX; 
        latest.gyrY = gyrY; 
        latest.gyrZ = gyrZ;

        int64_t now = esp_timer_get_time();
        if (now - lastTime >= 1000000) {
            lastTime = now;
            ESP_LOGI(TAG, "Actual frequency: %d Hz", count);
            count = 0;
        }

        // ESP_LOGI(TAG, "ACC: X=%.3f, Y=%.3f, Z=%.3f | GYR: X=%.2f, Y=%.2f, Z=%.2f, count=%d", accX, accY, accZ, gyrX, gyrY, gyrZ, count);
        snprintf(line, sizeof(line), "%.3f,%.3f,%.3f,%.2f,%.2f,%.2f,%d\n", accX, accY, accZ, gyrX, gyrY, gyrZ, count);
        esp_err_t write_result = sdCard->write(log_path, line);
        if (write_result != ESP_OK) {
            ESP_LOGE(TAG, "SD log write failed: %s", esp_err_to_name(write_result));
        }
    }
}
