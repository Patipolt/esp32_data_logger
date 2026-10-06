#include "esp_ticker.h"

Ticker::Ticker()
    : event_group_(nullptr), timer_(nullptr), bit_(BIT0), running_(false) {}

Ticker::~Ticker() {
    stop();
    if (timer_) {
        esp_timer_delete(timer_);
        timer_ = nullptr;
    }
    if (event_group_) {
        vEventGroupDelete(event_group_);
        event_group_ = nullptr;
    }
}

void Ticker::start(float hz) {
    if (hz <= 0.0f) {
        ESP_LOGE(TAG, "Invalid Hz: %f", (double)hz);
        return;
    }
    const double period_us_d = 1000000.0 / (double)hz;
    startPeriodUs((uint64_t)period_us_d);
}

void Ticker::startPeriodUs(uint64_t us) {
    if (running_) {
        ESP_LOGW(TAG, "Ticker already running, stopping first");
        stop();
    }

    if (!event_group_) {
        event_group_ = xEventGroupCreate();
        if (!event_group_) {
            ESP_LOGE(TAG, "Failed to create event group");
            return;
        }
    }

    esp_timer_create_args_t args = {
        .callback = &Ticker::timerCallback,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "esp_ticker",
        .skip_unhandled_events = true,
    };

    if (esp_timer_create(&args, &timer_) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create esp_timer");
        return;
    }

    if (esp_timer_start_periodic(timer_, us) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start esp_timer");
        return;
    }

    running_ = true;
    ESP_LOGI(TAG, "Ticker started, period = %llu us", (unsigned long long)us);
}

void Ticker::stop() {
    if (running_ && timer_) {
        esp_timer_stop(timer_);
        running_ = false;
        ESP_LOGI(TAG, "Ticker stopped");
    }
}

EventBits_t Ticker::wait() {
    if (!event_group_) {
        ESP_LOGE(TAG, "wait() called but event group not created");
        return 0;
    }
    return xEventGroupWaitBits(event_group_, bit_, pdTRUE, pdTRUE, portMAX_DELAY);
}

void Ticker::timerCallback(void* arg) {
    Ticker* self = static_cast<Ticker*>(arg);
    if (self && self->event_group_) {
        xEventGroupSetBits(self->event_group_, self->bit_);
    }
}
