/* This esp_ticker class is ported from esp_timer to achieve high-resolution ticker in ESP32. 
The class uses FreeRTOS event group to synchronize the periodic timer callback function with the main task. 
The class also uses FreeRTOS task to run the time-critical code. The idea is similar to using Ticker in MBed OS. 
Patipol Thanuphol July 2024*/

#ifndef ESP_TICKER_H
#define ESP_TICKER_H

#include "esp_log.h"                // for ESP_LOGI
#include "esp_err.h"                // for ESP_ERROR_CHECK
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"      // for vTaskDelay
#include "freertos/task.h"          // for xTaskCreate
#include "freertos/event_groups.h"


class ESP_TICKER
{
public:
    ESP_TICKER();
    virtual ~ESP_TICKER();

    /* Methods */
    static void periodic_timer_callback(void* arg);
    void start(float Hz);
    void stop(void);
    EventBits_t wait_for_ticks(void);

    /* Attributes */
    static const int TASK_FLAG = BIT0;
    bool ticker_running;
};


#endif // ESP_TICKER_H