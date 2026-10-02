#ifndef ESP_WIFIMANAGER_H_
#define ESP_WIFIMANAGER_H_

#include <cstdint>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#define WIFI_STA_DEFAULT_TIMEOUT_MS     15000
#define WIFI_AP_DEFAULT_CHANNEL         1
#define WIFI_AP_DEFAULT_MAX_CONN        4

// Brings WiFi up either as a hotspot (AP) or as a client of an existing network (STA).
class WIFI_MANAGER{
public:
    WIFI_MANAGER();
    ~WIFI_MANAGER();

    // Empty password -> open network, otherwise WPA2 (8-63 characters). AP IP is 192.168.4.1.
    esp_err_t startAP(const char *ssid, const char *password,
                      uint8_t channel = WIFI_AP_DEFAULT_CHANNEL,
                      uint8_t maxConnections = WIFI_AP_DEFAULT_MAX_CONN);

    // Blocks until an IP is obtained or the timeout expires (ESP_ERR_TIMEOUT).
    // After a timeout the driver keeps retrying in the background.
    esp_err_t startSTA(const char *ssid, const char *password,
                       uint32_t timeoutMs = WIFI_STA_DEFAULT_TIMEOUT_MS);

    void stop();

    bool isConnected() { return m_connected; }          // STA: has IP, AP: running
    const char *getIP() { return m_ip; }
    int getClientCount() { return m_clientCount; }      // AP only

private:
    esp_err_t init();
    static void eventHandler(void *arg, esp_event_base_t base, int32_t id, void *data);

    bool m_initialized = false;
    volatile bool m_staActive = false;
    volatile bool m_connected = false;
    volatile int m_clientCount = 0;
    char m_ip[16] = "0.0.0.0";
    esp_netif_t *m_staNetif = nullptr;
    esp_netif_t *m_apNetif = nullptr;
    EventGroupHandle_t m_events;
};

#endif // ESP_WIFIMANAGER_H_
