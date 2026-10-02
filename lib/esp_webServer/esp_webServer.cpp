#include "esp_webServer.h"
#include <cstring>

static const char *TAG = "WEB_SERVER";

static const char INDEX_HTML[] = R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 Data Logger</title>
<style>
body{font-family:sans-serif;margin:1em}
table{border-collapse:collapse}
td{padding:4px 14px;border-bottom:1px solid #ccc}
td+td{text-align:right;font-family:monospace}
</style></head><body>
<h2>Live data</h2>
<table id="t"></table>
<p id="s"></p>
<script>
async function tick(){
  try{
    const r=await fetch('/data',{cache:'no-store'});
    const d=await r.json();
    const t=document.getElementById('t');
    t.textContent='';
    for(const k in d){
      const row=t.insertRow();
      row.insertCell().textContent=k;
      row.insertCell().textContent=d[k];
    }
    document.getElementById('s').textContent='';
  }catch(e){
    document.getElementById('s').textContent='disconnected';
  }
  setTimeout(tick,100);
}
tick();
</script></body></html>)HTML";

esp_err_t WEB_SERVER::start(DataCallback callback, void *ctx, uint16_t port) {
    stop();
    m_callback = callback;
    m_ctx = ctx;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = port;

    esp_err_t err = httpd_start(&m_server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(err));
        m_server = nullptr;
        return err;
    }

    httpd_uri_t root = {};
    root.uri = "/";
    root.method = HTTP_GET;
    root.handler = handleRoot;
    root.user_ctx = this;
    httpd_register_uri_handler(m_server, &root);

    httpd_uri_t data = {};
    data.uri = "/data";
    data.method = HTTP_GET;
    data.handler = handleData;
    data.user_ctx = this;
    httpd_register_uri_handler(m_server, &data);

    ESP_LOGI(TAG, "HTTP server started on port %d", port);
    return ESP_OK;
}

void WEB_SERVER::stop() {
    if (m_server) {
        httpd_stop(m_server);
        m_server = nullptr;
    }
}

esp_err_t WEB_SERVER::handleRoot(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

esp_err_t WEB_SERVER::handleData(httpd_req_t *req) {
    WEB_SERVER *self = static_cast<WEB_SERVER *>(req->user_ctx);
    char buf[256];
    int n = self->m_callback ? self->m_callback(buf, sizeof(buf), self->m_ctx) : 0;
    if (n <= 0 || n >= (int)sizeof(buf)) {
        strcpy(buf, "{}");
        n = 2;
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, buf, n);
}
