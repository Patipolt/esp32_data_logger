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
#include "esp_socket.h"
#include "esp_webServer.h"
#include "esp_rgbLed.h"
#include "esp_uart.h"

static const char *TAG = "MAIN";

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

extern "C" void app_main()
{
    static LSM6DSO_I2C imu(LSM6DSO_SDA_PIN, LSM6DSO_SCL_PIN, LSM6DSO_I2C_FREQ, LSM6DSO_I2C_PORT);
    static SD_Card sdCard(MISO_PIN, MOSI_PIN, SCLK_PIN, CS_PIN, SDCARD_SPI_HOST, MOUNT_POINT);
    static WIFI_MANAGER wifi;
    static WEB_SERVER web;
    static RGB_LED rgb_led(RGB_LED_PIN, 25);
    static UDP_Socket udp;
    static UART uart(UART_RX_PIN, UART_TX_PIN, 115200, 1024, UART_NUM);

    rgb_led.setPattern(RGB_COLOR_WHITE, RGB_PACE_SOLID);

    if (uart.setup() != ESP_OK) {
        ESP_LOGE(TAG, "UART setup failed, halting");
        rgb_led.setPattern(RGB_COLOR_CYAN, RGB_PACE_BLINK_FAST);
        return;
    }

    if (!imu.isFound()) {
        ESP_LOGE(TAG, "IMU not found, halting");
        rgb_led.setPattern(RGB_COLOR_MAGENTA, RGB_PACE_BLINK_FAST);
        return;
    }
    if (sdCard.init() != ESP_OK) {
        ESP_LOGE(TAG, "SD card init failed, halting");
        rgb_led.setPattern(RGB_COLOR_RED, RGB_PACE_BLINK_FAST);
        return;
    }

    if (wifi.startAP(WIFI_AP_SSID, WIFI_AP_PASS) == ESP_OK) {
        web.start(imuJson);
        udp.begin();
        udp.enableBroadcast();
        ESP_LOGI(TAG, "Open http://%s in a browser", wifi.getIP());
    } else {
        ESP_LOGE(TAG, "WiFi AP failed, logging to SD only");
        rgb_led.setPattern(RGB_COLOR_BLUE, RGB_PACE_BLINK_SLOW);
    }

    rgb_led.setPattern(RGB_COLOR_GREEN, RGB_PACE_SOLID);

    const char *log_path = MOUNT_POINT "/imu_log.csv";
    char line[MAX_ENTRY_SIZE];
    int count = 0;

    while(1)
    {
        count++;
        imu.readAll();
        float accX = imu.getACCX(), accY = imu.getACCY(), accZ = imu.getACCZ();
        float gyrX = imu.getGYRX(), gyrY = imu.getGYRY(), gyrZ = imu.getGYRZ();
        latest.accX = accX; latest.accY = accY; latest.accZ = accZ;
        latest.gyrX = gyrX; latest.gyrY = gyrY; latest.gyrZ = gyrZ;

        uart.send(line, snprintf(line, sizeof(line), "%.3f,%.3f,%.3f,%.2f,%.2f,%.2f,%d\n", accX, accY, accZ, gyrX, gyrY, gyrZ, count));
        ESP_LOGI(TAG, "ACC: X=%.3f, Y=%.3f, Z=%.3f | GYR: X=%.2f, Y=%.2f, Z=%.2f, count=%d", accX, accY, accZ, gyrX, gyrY, gyrZ, count);
        int n = snprintf(line, sizeof(line), "%.3f,%.3f,%.3f,%.2f,%.2f,%.2f,%d\n", accX, accY, accZ, gyrX, gyrY, gyrZ, count);
        sdCard.write(log_path, line);
        if (wifi.getClientCount() > 0) {
            udp.sendTo(UDP_TARGET_IP, UDP_TARGET_PORT, line, n);
        }
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}
