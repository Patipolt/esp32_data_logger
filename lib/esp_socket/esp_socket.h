#ifndef ESP_SOCKET_H_
#define ESP_SOCKET_H_

#include <cstddef>
#include <cstdint>
#include "esp_err.h"
#include "esp_log.h"

#define SOCKET_ERROR_GENERIC    -1
#define SOCKET_ERROR_TIMEOUT    -2

// Timeouts are in ms; 0 blocks forever. WiFi must be up (see esp_wifiManager) before use.

class UDP_Socket{
public:
    UDP_Socket() {}
    ~UDP_Socket() { close(); }

    esp_err_t begin(uint16_t localPort = 0);        // 0 = any port (send only)
    void enableBroadcast();
    int sendTo(const char *ip, uint16_t port, const void *data, size_t len);
    // Returns bytes received, SOCKET_ERROR_TIMEOUT or SOCKET_ERROR_GENERIC. srcIp needs 16 bytes.
    int receive(void *buf, size_t len, uint32_t timeoutMs, char *srcIp = nullptr, uint16_t *srcPort = nullptr);
    void close();

private:
    int m_sock = -1;
};

class TCP_Client{
public:
    TCP_Client() {}
    ~TCP_Client() { close(); }
    TCP_Client(const TCP_Client &) = delete;
    TCP_Client &operator=(const TCP_Client &) = delete;

    esp_err_t connect(const char *ip, uint16_t port, uint32_t timeoutMs = 5000);
    int send(const void *data, size_t len);         // sends everything or fails
    // Returns bytes received, 0 if the peer closed, SOCKET_ERROR_TIMEOUT or SOCKET_ERROR_GENERIC.
    int receive(void *buf, size_t len, uint32_t timeoutMs);
    void close();
    bool isConnected() { return m_sock >= 0; }

private:
    friend class TCP_Server;
    void attach(int sock);
    int m_sock = -1;
};

class TCP_Server{
public:
    TCP_Server() {}
    ~TCP_Server() { close(); }

    esp_err_t begin(uint16_t port, int backlog = 1);
    // Waits for a client; ESP_ERR_TIMEOUT if none arrived.
    esp_err_t accept(TCP_Client &client, uint32_t timeoutMs);
    void close();

private:
    int m_sock = -1;
};

#endif // ESP_SOCKET_H_
