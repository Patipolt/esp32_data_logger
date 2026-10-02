#include "esp_rgbLed.h"

static const char *TAG = "RGB_LED";

#define RMT_RESOLUTION_HZ   10000000    // 10 MHz -> 0.1 us per tick

RGB_LED::RGB_LED(gpio_num_t pin, uint8_t brightness)
    : m_pin(pin), m_brightness(brightness) {
    if (RMT_init() != ESP_OK) {
        return;
    }
    m_ready = true;
    setPattern(RGB_COLOR_WHITE, RGB_PACE_SOLID);
    run();
}

RGB_LED::~RGB_LED() {}

esp_err_t RGB_LED::RMT_init() {
    rmt_tx_channel_config_t tx_cfg = {};
    tx_cfg.gpio_num = m_pin;
    tx_cfg.clk_src = RMT_CLK_SRC_DEFAULT;
    tx_cfg.resolution_hz = RMT_RESOLUTION_HZ;
    tx_cfg.mem_block_symbols = 48;
    tx_cfg.trans_queue_depth = 1;

    esp_err_t err = rmt_new_tx_channel(&tx_cfg, &m_channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error creating RMT channel: %s", esp_err_to_name(err));
        return err;
    }

    rmt_copy_encoder_config_t enc_cfg = {};
    err = rmt_new_copy_encoder(&enc_cfg, &m_encoder);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error creating RMT encoder: %s", esp_err_to_name(err));
        return err;
    }

    err = rmt_enable(m_channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error enabling RMT channel: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "RGB LED initialized on GPIO %d", m_pin);
    return ESP_OK;
}

RGB_Value RGB_LED::presetToRGB(RGB_Color color) {
    switch (color) {
        case RGB_COLOR_RED:     return {255, 0, 0};
        case RGB_COLOR_GREEN:   return {0, 255, 0};
        case RGB_COLOR_BLUE:    return {0, 0, 255};
        case RGB_COLOR_WHITE:   return {255, 255, 255};
        case RGB_COLOR_CYAN:    return {0, 255, 255};
        case RGB_COLOR_MAGENTA: return {255, 0, 255};
        case RGB_COLOR_YELLOW:  return {255, 255, 0};
        case RGB_COLOR_ORANGE:  return {255, 50, 0};
        default:                return {0, 0, 0};
    }
}

void RGB_LED::update() {
    m_version = m_version + 1;
}

void RGB_LED::setColor(RGB_Color color) {
    RGB_Value c = presetToRGB(color);
    setColor(c.r, c.g, c.b);
}

void RGB_LED::setColor(uint8_t r, uint8_t g, uint8_t b) {
    m_r = r;
    m_g = g;
    m_b = b;
    update();
}

void RGB_LED::setColor(uint32_t rgb) {
    setColor((uint8_t)(rgb >> 16), (uint8_t)(rgb >> 8), (uint8_t)rgb);
}

void RGB_LED::setPace(RGB_Pace pace) {
    m_pace = pace;
    update();
}

void RGB_LED::setPattern(RGB_Color color, RGB_Pace pace) {
    RGB_Value c = presetToRGB(color);
    setPattern(c.r, c.g, c.b, pace);
}

void RGB_LED::setPattern(uint8_t r, uint8_t g, uint8_t b, RGB_Pace pace) {
    m_r = r;
    m_g = g;
    m_b = b;
    m_pace = pace;
    update();
}

void RGB_LED::setBrightness(uint8_t brightness) {
    m_brightness = brightness;
    update();
}

void RGB_LED::off() {
    setPattern(RGB_COLOR_OFF, RGB_PACE_SOLID);
}

// WS2812 expects GRB order, MSB first
void RGB_LED::show(RGB_Value color) {
    if (!m_ready) {
        return;
    }
    const uint8_t br = m_brightness;
    const uint32_t grb = ((uint32_t)(color.g * br / 255) << 16) |
                         ((uint32_t)(color.r * br / 255) << 8)  |
                         ((uint32_t)(color.b * br / 255));

    for (int i = 0; i < 24; i++) {
        bool bit = grb & (1UL << (23 - i));
        m_symbols[i].level0 = 1;
        m_symbols[i].duration0 = bit ? 9 : 3;   // T1H 0.9 us / T0H 0.3 us
        m_symbols[i].level1 = 0;
        m_symbols[i].duration1 = bit ? 3 : 9;   // T1L 0.3 us / T0L 0.9 us
    }
    // Reset: line low for 60 us
    m_symbols[24].level0 = 0;
    m_symbols[24].duration0 = 300;
    m_symbols[24].level1 = 0;
    m_symbols[24].duration1 = 300;

    rmt_transmit_config_t tx_cfg = {};
    tx_cfg.loop_count = 0;
    if (rmt_transmit(m_channel, m_encoder, m_symbols, sizeof(m_symbols), &tx_cfg) == ESP_OK) {
        rmt_tx_wait_all_done(m_channel, portMAX_DELAY);
    }
}

bool RGB_LED::wait(uint32_t version, uint32_t ms) {
    while (ms > 0) {
        uint32_t chunk = ms > 10 ? 10 : ms;
        vTaskDelay(pdMS_TO_TICKS(chunk) > 0 ? pdMS_TO_TICKS(chunk) : 1);
        ms -= chunk;
        if (m_version != version) {
            return false;
        }
    }
    return true;
}

void RGB_LED::run_task(void *pvParameter) {
    RGB_LED *led = static_cast<RGB_LED*>(pvParameter);
    const RGB_Value black = {0, 0, 0};

    while (true) {
        const uint32_t ver = led->m_version;
        const RGB_Value c = {led->m_r, led->m_g, led->m_b};

        switch (led->m_pace) {
            case RGB_PACE_BLINK_SLOW:
            case RGB_PACE_BLINK_FAST: {
                uint32_t delay = (led->m_pace == RGB_PACE_BLINK_SLOW) ? RGB_SLOW_PACE_MS : RGB_FAST_PACE_MS;
                led->show(c);
                if (!led->wait(ver, delay)) break;
                led->show(black);
                led->wait(ver, delay);
                break;
            }
            case RGB_PACE_AIRPLANE_ONCE:
                led->show(c);
                if (!led->wait(ver, 50)) break;
                led->show(black);
                led->wait(ver, RGB_AIRPLANE_GAP_MS);
                break;
            case RGB_PACE_AIRPLANE_TWICE:
                led->show(c);
                if (!led->wait(ver, 10)) break;
                led->show(black);
                if (!led->wait(ver, 100)) break;
                led->show(c);
                if (!led->wait(ver, 50)) break;
                led->show(black);
                led->wait(ver, RGB_AIRPLANE_GAP_MS);
                break;
            case RGB_PACE_FADE:
                // Blue -> Red -> Green -> Blue
                for (int phase = 0; phase < 3; phase++) {
                    bool changed = false;
                    for (int i = 0; i < 256; i++) {
                        RGB_Value f;
                        if (phase == 0)      f = {(uint8_t)i, 0, (uint8_t)(255 - i)};
                        else if (phase == 1) f = {(uint8_t)(255 - i), (uint8_t)i, 0};
                        else                 f = {0, (uint8_t)(255 - i), (uint8_t)i};
                        led->show(f);
                        if (!led->wait(ver, RGB_FADE_STEP_MS)) {
                            changed = true;
                            break;
                        }
                    }
                    if (changed) break;
                }
                break;
            case RGB_PACE_SOLID:
            default:
                led->show(c);
                led->wait(ver, 500);
        }
    }
}

void RGB_LED::run() {
    xTaskCreate(run_task, "RGB_LED_task", 3072, this, 5, NULL);
    ESP_LOGI(TAG, "RGB LED task created");
}
