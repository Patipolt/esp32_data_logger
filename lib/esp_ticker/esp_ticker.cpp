#include "esp_ticker.h"

static const char *TAG = "ESP_TICKER";

EventGroupHandle_t event_group;
esp_timer_handle_t periodic_timer;


ESP_TICKER::ESP_TICKER() {
    ticker_running = false;
}

ESP_TICKER::~ESP_TICKER() {
    if (this->ticker_running) {
        this->stop();
    }
    if (event_group != nullptr) {
        vEventGroupDelete(event_group);
        event_group = nullptr;
    }
    if (periodic_timer != nullptr) {
        esp_timer_delete(periodic_timer);
        periodic_timer = nullptr;
    }
}

void ESP_TICKER::periodic_timer_callback(void* arg) {
    xEventGroupSetBits(event_group, TASK_FLAG);
}

void ESP_TICKER::start(float Hz)
{
    // Ensure event_group and periodic_timer are initialized only once
    if (event_group == nullptr) {
        /* Initialize the event group */
        event_group = xEventGroupCreate();
        if (event_group == NULL) {
            ESP_LOGE(TAG, "Failed to create event group");
            return;
        }
    }

    /* Configure the high-resolution timer */
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &ESP_TICKER::periodic_timer_callback,
        .arg = this,  // Pass the ESP_TICKER instance
        .dispatch_method = ESP_TIMER_TASK,
        .name = "periodic_timer",
        .skip_unhandled_events = false,
    };

    // Create the periodic timer
    esp_err_t ret = esp_timer_create(&periodic_timer_args, &periodic_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create periodic timer, error: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Periodic timer created successfully");
    }

    // Start the periodic timer with microseconds interval
    ret = esp_timer_start_periodic(periodic_timer, (1000 / Hz)*1000);  // microseconds
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start periodic timer");
        return;
    }
    this->ticker_running = true;
    ESP_LOGI(TAG, "Ticker started with frequency: %.2f Hz", Hz);
}

void ESP_TICKER::stop(void)
{
    if (!this->ticker_running) {
        ESP_LOGW(TAG, "Ticker is already stopped");
        return;
    }

    esp_err_t ret = esp_timer_stop(periodic_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop periodic timer, error: %s", esp_err_to_name(ret));
        return;
    }

    this->ticker_running = false;
    ESP_LOGI(TAG, "Ticker stopped");
}

EventBits_t ESP_TICKER::wait_for_ticks(void)
{
    return xEventGroupWaitBits(event_group, TASK_FLAG, pdTRUE, pdTRUE, portMAX_DELAY);
}