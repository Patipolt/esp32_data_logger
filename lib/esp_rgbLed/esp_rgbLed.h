#ifndef ESP_RGBLED_H_
#define ESP_RGBLED_H_

#include <cstdint>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/rmt_tx.h"

// Preset colors
enum RGB_Color {
    RGB_COLOR_OFF = 0,
    RGB_COLOR_RED,
    RGB_COLOR_GREEN,
    RGB_COLOR_BLUE,
    RGB_COLOR_WHITE,
    RGB_COLOR_CYAN,
    RGB_COLOR_MAGENTA,
    RGB_COLOR_YELLOW,
    RGB_COLOR_ORANGE,
};

// Blinking patterns
enum RGB_Pace {
    RGB_PACE_SOLID = 0,
    RGB_PACE_BLINK_SLOW,
    RGB_PACE_BLINK_FAST,
    RGB_PACE_AIRPLANE_ONCE,
    RGB_PACE_AIRPLANE_TWICE,
    RGB_PACE_FADE,              // continuous color cycle, ignores the color
};

#define RGB_SLOW_PACE_MS        500
#define RGB_FAST_PACE_MS        100
#define RGB_AIRPLANE_GAP_MS     2000
#define RGB_FADE_STEP_MS        5

struct RGB_Value {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

// Drives a single WS2812/NeoPixel-type LED (e.g. GPIO48 on ESP32-S3 dev kits) through the RMT peripheral.
class RGB_LED{
public:
    // Methods
    RGB_LED(gpio_num_t pin, uint8_t brightness = 50);
    ~RGB_LED();

    void setColor(RGB_Color color);
    void setColor(uint8_t r, uint8_t g, uint8_t b);
    void setColor(uint32_t rgb);                        // 0xRRGGBB
    void setPace(RGB_Pace pace);
    void setPattern(RGB_Color color, RGB_Pace pace);
    void setPattern(uint8_t r, uint8_t g, uint8_t b, RGB_Pace pace);
    void setBrightness(uint8_t brightness);             // 0-255
    void off();

    static RGB_Value presetToRGB(RGB_Color color);

private:
    esp_err_t RMT_init();
    void show(RGB_Value color);
    bool wait(uint32_t version, uint32_t ms);           // false if the pattern was changed meanwhile
    void update();
    static void run_task(void *pvParameter);
    void run();

    gpio_num_t m_pin;
    rmt_channel_handle_t m_channel = nullptr;
    rmt_encoder_handle_t m_encoder = nullptr;
    rmt_symbol_word_t m_symbols[25];                    // 24 data bits + reset

    volatile uint8_t m_brightness;
    volatile RGB_Pace m_pace = RGB_PACE_SOLID;
    volatile uint8_t m_r = 0, m_g = 0, m_b = 0;
    volatile uint32_t m_version = 0;                    // bumped on every change
    bool m_ready = false;
};

#endif // ESP_RGBLED_H_
