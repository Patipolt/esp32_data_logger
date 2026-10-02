#include "esp_wifiManager.h"
#include <cstring>
#include "nvs_flash.h"

static const char *TAG = "WIFI_MANAGER";

#define BIT_GOT_IP  BIT0

WIFI_MANAGER::WIFI_MANAGER() {
    m_events = xEventGroupCreate();
}

WIFI_MANAGER::~WIFI_MANAGER() {}

esp_err_t WIFI_MANAGER::init() {
    if (m_initialized) {
        return ESP_OK;
    }

    // NVS is required by the WiFi driver
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_netif_init();
    if (err != ESP_OK) return err;

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

    m_staNetif = esp_netif_create_default_wifi_sta();
    m_apNetif = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(err));
        return err;
    }

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &eventHandler, this, nullptr);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &eventHandler, this, nullptr);

    m_initialized = true;
    return ESP_OK;
}

esp_err_t WIFI_MANAGER::startAP(const char *ssid, const char *password, uint8_t channel, uint8_t maxConnections) {
    size_t passLen = password ? strlen(password) : 0;
    if (!ssid || strlen(ssid) == 0 || strlen(ssid) > 32) {
        ESP_LOGE(TAG, "Invalid SSID");
        return ESP_ERR_INVALID_ARG;
    }
    if (passLen != 0 && (passLen < 8 || passLen > 63)) {
        ESP_LOGE(TAG, "AP password must be 8-63 characters");
        return ESP_ERR_INVALID_ARG;
    }

    stop();
    esp_err_t err = init();
    if (err != ESP_OK) return err;

    wifi_config_t conf = {};
    strncpy((char *)conf.ap.ssid, ssid, sizeof(conf.ap.ssid));
    conf.ap.ssid_len = strlen(ssid);
    conf.ap.channel = channel;
    conf.ap.max_connection = maxConnections;
    if (passLen == 0) {
        conf.ap.authmode = WIFI_AUTH_OPEN;
    } else {
        strncpy((char *)conf.ap.password, password, sizeof(conf.ap.password));
        conf.ap.authmode = WIFI_AUTH_WPA2_PSK;
    }

    err = esp_wifi_set_mode(WIFI_MODE_AP);
    if (err == ESP_OK) err = esp_wifi_set_config(WIFI_IF_AP, &conf);
    if (err == ESP_OK) err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start AP: %s", esp_err_to_name(err));
        return err;
    }
    esp_wifi_set_ps(WIFI_PS_NONE);  // lower latency for streaming

    esp_netif_ip_info_t ip;
    if (esp_netif_get_ip_info(m_apNetif, &ip) == ESP_OK) {
        snprintf(m_ip, sizeof(m_ip), IPSTR, IP2STR(&ip.ip));
    }
    m_connected = true;
    ESP_LOGI(TAG, "AP started: SSID '%s', channel %d, IP %s", ssid, channel, m_ip);
    return ESP_OK;
}

esp_err_t WIFI_MANAGER::startSTA(const char *ssid, const char *password, uint32_t timeoutMs) {
    if (!ssid || strlen(ssid) == 0 || strlen(ssid) > 32) {
        ESP_LOGE(TAG, "Invalid SSID");
        return ESP_ERR_INVALID_ARG;
    }
    size_t passLen = password ? strlen(password) : 0;
    if (passLen > 63) {
        ESP_LOGE(TAG, "Password too long");
        return ESP_ERR_INVALID_ARG;
    }

    stop();
    esp_err_t err = init();
    if (err != ESP_OK) return err;

    wifi_config_t conf = {};
    strncpy((char *)conf.sta.ssid, ssid, sizeof(conf.sta.ssid));
    if (passLen > 0) {
        strncpy((char *)conf.sta.password, password, sizeof(conf.sta.password));
        conf.sta.threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    } else {
        conf.sta.threshold.authmode = WIFI_AUTH_OPEN;
    }
    conf.sta.pmf_cfg.capable = true;
    conf.sta.pmf_cfg.required = false;

    m_staActive = true;
    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err == ESP_OK) err = esp_wifi_set_config(WIFI_IF_STA, &conf);
    if (err == ESP_OK) err = esp_wifi_start();   // STA_START event triggers esp_wifi_connect()
    if (err != ESP_OK) {
        m_staActive = false;
        ESP_LOGE(TAG, "Failed to start STA: %s", esp_err_to_name(err));
        return err;
    }
    esp_wifi_set_ps(WIFI_PS_NONE);

    ESP_LOGI(TAG, "Connecting to '%s'...", ssid);
    EventBits_t bits = xEventGroupWaitBits(m_events, BIT_GOT_IP, pdFALSE, pdFALSE, pdMS_TO_TICKS(timeoutMs));
    if (bits & BIT_GOT_IP) {
        return ESP_OK;
    }
    ESP_LOGW(TAG, "Not connected after %u ms, still retrying in background", (unsigned)timeoutMs);
    return ESP_ERR_TIMEOUT;
}

void WIFI_MANAGER::stop() {
    m_staActive = false;
    m_connected = false;
    m_clientCount = 0;
    strcpy(m_ip, "0.0.0.0");
    xEventGroupClearBits(m_events, BIT_GOT_IP);
    if (m_initialized) {
        esp_wifi_stop();
    }
}

void WIFI_MANAGER::eventHandler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    WIFI_MANAGER *self = static_cast<WIFI_MANAGER *>(arg);

    if (base == WIFI_EVENT) {
        switch (id) {
            case WIFI_EVENT_STA_START:
                if (self->m_staActive) esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                self->m_connected = false;
                xEventGroupClearBits(self->m_events, BIT_GOT_IP);
                if (self->m_staActive) {
                    ESP_LOGW(TAG, "Disconnected, retrying...");
                    esp_wifi_connect();
                }
                break;
            case WIFI_EVENT_AP_STACONNECTED: {
                wifi_event_ap_staconnected_t *e = (wifi_event_ap_staconnected_t *)data;
                self->m_clientCount = self->m_clientCount + 1;
                ESP_LOGI(TAG, "Client " MACSTR " joined", MAC2STR(e->mac));
                break;
            }
            case WIFI_EVENT_AP_STADISCONNECTED: {
                wifi_event_ap_stadisconnected_t *e = (wifi_event_ap_stadisconnected_t *)data;
                if (self->m_clientCount > 0) self->m_clientCount = self->m_clientCount - 1;
                ESP_LOGI(TAG, "Client " MACSTR " left", MAC2STR(e->mac));
                break;
            }
            default:
                break;
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        snprintf(self->m_ip, sizeof(self->m_ip), IPSTR, IP2STR(&e->ip_info.ip));
        self->m_connected = true;
        ESP_LOGI(TAG, "Got IP: %s", self->m_ip);
        xEventGroupSetBits(self->m_events, BIT_GOT_IP);
    }
}
