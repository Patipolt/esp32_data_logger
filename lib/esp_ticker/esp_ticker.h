/* This esp_ticker class is ported from esp_timer to achieve high-resolution ticker in ESP32. 
The class uses FreeRTOS event group to synchronize the periodic timer callback function with the main task. 
The class also uses FreeRTOS task to run the time-critical code. The idea is similar to using Ticker in MBed OS. 
Patipol Thanuphol July 2024*/

#pragma once

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

// A very simple, single-instance-safe ticker class.
// No copy/move semantics — just create, start, wait, stop, destroy.
class Ticker {
public:
    Ticker();
    ~Ticker();

    void start(float hz);                 // Start with frequency in Hz
    void startPeriodUs(uint64_t us);      // Start with period in microseconds
    void stop();                          // Stop the timer

    EventBits_t wait();                   // Block until next tick
    bool isRunning() const { return running_; }

private:
    static void timerCallback(void* arg); // timer ISR callback

private:
    EventGroupHandle_t event_group_;
    esp_timer_handle_t timer_;
    EventBits_t bit_;
    bool running_;
    static constexpr const char* TAG = "Ticker";
};
