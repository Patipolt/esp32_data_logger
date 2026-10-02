#include "esp_socket.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/select.h>
#include "lwip/sockets.h"

static const char *TAG = "ESP_SOCKET";

static void setTimeouts(int sock, int optname, uint32_t timeoutMs) {
    struct timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, optname, &tv, sizeof(tv));
}

static int translateError() {
    return (errno == EAGAIN || errno == EWOULDBLOCK) ? SOCKET_ERROR_TIMEOUT : SOCKET_ERROR_GENERIC;
}

// ---------------- UDP ----------------

esp_err_t UDP_Socket::begin(uint16_t localPort) {
    close();
    m_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (m_sock < 0) {
        ESP_LOGE(TAG, "UDP socket creation failed: errno %d", errno);
        return ESP_FAIL;
    }
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(localPort);
    if (bind(m_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "UDP bind failed: errno %d", errno);
        close();
        return ESP_FAIL;
    }
    return ESP_OK;
}

void UDP_Socket::enableBroadcast() {
    int enable = 1;
    setsockopt(m_sock, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(enable));
}

int UDP_Socket::sendTo(const char *ip, uint16_t port, const void *data, size_t len) {
    if (m_sock < 0) return SOCKET_ERROR_GENERIC;
    struct sockaddr_in dest = {};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &dest.sin_addr) != 1) return SOCKET_ERROR_GENERIC;
    int n = sendto(m_sock, data, len, 0, (struct sockaddr *)&dest, sizeof(dest));
    return n < 0 ? SOCKET_ERROR_GENERIC : n;
}

int UDP_Socket::receive(void *buf, size_t len, uint32_t timeoutMs, char *srcIp, uint16_t *srcPort) {
    if (m_sock < 0) return SOCKET_ERROR_GENERIC;
    setTimeouts(m_sock, SO_RCVTIMEO, timeoutMs);
    struct sockaddr_in src = {};
    socklen_t srcLen = sizeof(src);
    int n = recvfrom(m_sock, buf, len, 0, (struct sockaddr *)&src, &srcLen);
    if (n < 0) return translateError();
    if (srcIp) inet_ntop(AF_INET, &src.sin_addr, srcIp, 16);
    if (srcPort) *srcPort = ntohs(src.sin_port);
    return n;
}

void UDP_Socket::close() {
    if (m_sock >= 0) {
        ::close(m_sock);
        m_sock = -1;
    }
}

// ---------------- TCP client ----------------

esp_err_t TCP_Client::connect(const char *ip, uint16_t port, uint32_t timeoutMs) {
    close();
    struct sockaddr_in dest = {};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &dest.sin_addr) != 1) return ESP_ERR_INVALID_ARG;

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "TCP socket creation failed: errno %d", errno);
        return ESP_FAIL;
    }

    // Non-blocking connect so the timeout is honoured
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    int rc = ::connect(sock, (struct sockaddr *)&dest, sizeof(dest));
    if (rc < 0 && errno != EINPROGRESS) {
        ::close(sock);
        return ESP_FAIL;
    }
    if (rc < 0) {
        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(sock, &wfds);
        struct timeval tv = {(time_t)(timeoutMs / 1000), (suseconds_t)((timeoutMs % 1000) * 1000)};
        rc = select(sock + 1, nullptr, &wfds, nullptr, &tv);
        if (rc <= 0) {
            ::close(sock);
            return rc == 0 ? ESP_ERR_TIMEOUT : ESP_FAIL;
        }
        int soErr = 0;
        socklen_t soLen = sizeof(soErr);
        getsockopt(sock, SOL_SOCKET, SO_ERROR, &soErr, &soLen);
        if (soErr != 0) {
            ::close(sock);
            return ESP_FAIL;
        }
    }
    fcntl(sock, F_SETFL, flags);

    attach(sock);
    return ESP_OK;
}

void TCP_Client::attach(int sock) {
    close();
    m_sock = sock;
    int nodelay = 1;
    setsockopt(m_sock, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
}

int TCP_Client::send(const void *data, size_t len) {
    if (m_sock < 0) return SOCKET_ERROR_GENERIC;
    const uint8_t *p = static_cast<const uint8_t *>(data);
    size_t sent = 0;
    while (sent < len) {
        int n = ::send(m_sock, p + sent, len - sent, 0);
        if (n < 0) return SOCKET_ERROR_GENERIC;
        sent += n;
    }
    return (int)sent;
}

int TCP_Client::receive(void *buf, size_t len, uint32_t timeoutMs) {
    if (m_sock < 0) return SOCKET_ERROR_GENERIC;
    setTimeouts(m_sock, SO_RCVTIMEO, timeoutMs);
    int n = recv(m_sock, buf, len, 0);
    if (n < 0) return translateError();
    return n;
}

void TCP_Client::close() {
    if (m_sock >= 0) {
        ::close(m_sock);
        m_sock = -1;
    }
}

// ---------------- TCP server ----------------

esp_err_t TCP_Server::begin(uint16_t port, int backlog) {
    close();
    m_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (m_sock < 0) {
        ESP_LOGE(TAG, "TCP server socket creation failed: errno %d", errno);
        return ESP_FAIL;
    }
    int reuse = 1;
    setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(m_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0 || listen(m_sock, backlog) < 0) {
        ESP_LOGE(TAG, "TCP bind/listen failed: errno %d", errno);
        close();
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "TCP server listening on port %d", port);
    return ESP_OK;
}

esp_err_t TCP_Server::accept(TCP_Client &client, uint32_t timeoutMs) {
    if (m_sock < 0) return ESP_ERR_INVALID_STATE;

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(m_sock, &rfds);
    struct timeval tv = {(time_t)(timeoutMs / 1000), (suseconds_t)((timeoutMs % 1000) * 1000)};
    int rc = select(m_sock + 1, &rfds, nullptr, nullptr, timeoutMs ? &tv : nullptr);
    if (rc == 0) return ESP_ERR_TIMEOUT;
    if (rc < 0) return ESP_FAIL;

    int sock = ::accept(m_sock, nullptr, nullptr);
    if (sock < 0) return ESP_FAIL;
    client.attach(sock);
    return ESP_OK;
}

void TCP_Server::close() {
    if (m_sock >= 0) {
        ::close(m_sock);
        m_sock = -1;
    }
}
