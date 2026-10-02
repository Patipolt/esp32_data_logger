#ifndef ESP_WEBSERVER_H_
#define ESP_WEBSERVER_H_

#include <cstddef>
#include <cstdint>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_http_server.h"

// Serves a page at "/" that polls "/data" and shows every key of the returned flat JSON object.
// The data source is a callback, so this class knows nothing about the sensor.
class WEB_SERVER{
public:
    // Writes a flat JSON object such as {"accX":0.01} into buf and returns its length (<= 0 on failure).
    // It runs on the HTTP server task, so protect shared data accordingly.
    typedef int (*DataCallback)(char *buf, size_t len, void *ctx);

    WEB_SERVER() {}
    ~WEB_SERVER() { stop(); }

    esp_err_t start(DataCallback callback, void *ctx = nullptr, uint16_t port = 80);
    void stop();

private:
    static esp_err_t handleRoot(httpd_req_t *req);
    static esp_err_t handleData(httpd_req_t *req);

    httpd_handle_t m_server = nullptr;
    DataCallback m_callback = nullptr;
    void *m_ctx = nullptr;
};

#endif // ESP_WEBSERVER_H_
