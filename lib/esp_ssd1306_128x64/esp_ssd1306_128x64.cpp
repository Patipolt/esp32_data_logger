#include "esp_ssd1306_128x64.h"

static const char *TAG = "SSD1306_128X64";

I2C *SSD1306_128X64::m_i2c = nullptr; // Initialize static member

SSD1306_128X64::SSD1306_128X64(
    gpio_num_t sda, 
    gpio_num_t scl, 
    uint32_t frequency, 
    i2c_port_t i2c_port  = I2C_NUM_0, 
    uint8_t address = 0x3C) : m_address(address)
{
    m_i2c = new I2C(sda, scl, frequency, i2c_port);
    m_i2c->setup();
    m_i2c->i2c_scan(); // Scan for I2C devices
    init();
}
SSD1306_128X64::~SSD1306_128X64() {
    if (m_i2c != nullptr)
    {
        delete m_i2c; // Clean up I2C object
        m_i2c = nullptr;
    }
    ESP_LOGI(TAG, "SSD1306 OLED display destroyed.");
    // No need to delete m_u8g2, as it is managed by u8g2 library   
}

esp_err_t SSD1306_128X64::init(void)
{
    ESP_LOGI(TAG, "Initializing SSD1306 OLED display at address 0x%02X", m_address);
    u8g2_esp32_hal_t u8g2_esp32_hal = U8G2_ESP32_HAL_DEFAULT;
    u8g2_esp32_hal.sda = m_i2c->get_sda();
    u8g2_esp32_hal.scl = m_i2c->get_scl();
    u8g2_esp32_hal.i2c_port_num = m_i2c->get_i2c_port();
    u8g2_esp32_hal_init(u8g2_esp32_hal);

    u8g2_Setup_ssd1306_i2c_128x64_noname_f(
        &m_u8g2,
        U8G2_R0,
        u8g2_esp32_i2c_byte_cb,
        u8g2_esp32_gpio_and_delay_cb);

    u8x8_SetI2CAddress(&m_u8g2.u8x8, m_address << 1); // Set I2C address
    u8g2_InitDisplay(&m_u8g2);
    u8g2_SetPowerSave(&m_u8g2, 0);
    ESP_LOGI(TAG, "SSD1306 OLED display initialized successfully.");

    return ESP_OK;
} // init

void SSD1306_128X64::clear_display(void)
{
    u8g2_ClearBuffer(&m_u8g2);
    u8g2_SendBuffer(&m_u8g2);
} // clear_display

void SSD1306_128X64::display_message(const char *message)
{
    /* If the length of the message is not exceeding the 128 pixels
    (width of the current font x length of the message),
    then display the message in the middle of the screen. If it exceeds 128 pixels,
    divide the message into at maximun 4 lines and display it also in the middle of the screen.*/
    int current_font_width = 6 + 1;   // for spacing
    int current_font_height = 10 + 2; // Capital A + 2 for spacing

    // number of characters that can fit in 1 line
    // round down to the nearest integer
    int num_of_char_avai = (128 / current_font_width) - 1; // 1 for spacing

    // number of lines that can fit in 1 screen
    // round down to the nearest integer
    int num_of_lines_avai = (64 / current_font_height);
    
    // char cut_message[num_of_lines_avai][num_of_char_avai + 1];
    char cut_message[num_of_lines_avai][num_of_char_avai + 1] = {0}; // +1 for null terminator
    int num_of_char = strlen(message);
    int num_of_lines = ceil((float)num_of_char / num_of_char_avai);

    if (num_of_lines > num_of_lines_avai)
    {
        // Showing a message that is too long
        // const char *long_message = "Message too long";
        // strncpy(cut_message[0], long_message, sizeof(cut_message[0]) - 1);
        // cut_message[0][sizeof(cut_message[0]) - 1] = '\0';
        // num_of_lines = 1;

        // Printing it anyway, but the message will be cut off
        num_of_lines = num_of_lines_avai;
        for (int i = 0; i < num_of_lines_avai; i++)
        {
            strncpy(cut_message[i], message + i * (num_of_char_avai), num_of_char_avai);
            cut_message[i][num_of_char_avai] = '\0';
        }
    }
    else {
        if (num_of_char > num_of_char_avai)
        {
            for (int i = 0; i < num_of_lines; i++)
            {
                strncpy(cut_message[i], message + i * (num_of_char_avai), num_of_char_avai);
                cut_message[i][num_of_char_avai] = '\0';
            }
        }
        else
        {
            strncpy(cut_message[0], message, num_of_char);
            cut_message[0][num_of_char] = '\0';
        }
    }


    // Do not put any logic during the display of the message
    // it does not work properly
    // Clear the buffer and draw text
    u8g2_ClearBuffer(&m_u8g2);
    u8g2_SetFont(&m_u8g2, u8g2_font_7x14B_tr);
    if (num_of_lines > 1)
    {
        for (int i = 0; i < num_of_lines; i++)
        {
            u8g2_DrawStr(&m_u8g2, 64 - (num_of_char_avai * current_font_width / 2), 32 + current_font_height - (num_of_lines * current_font_height / 2) + (i * current_font_height), cut_message[i]);
        }
    }
    else
    {
        int line_len = strlen(cut_message[0]);
        u8g2_DrawStr(&m_u8g2, 64 - (line_len * current_font_width / 2), 32 + current_font_height / 2, cut_message[0]);
    }
    // Send buffer content to the display
    u8g2_SendBuffer(&m_u8g2);
} // display_message

void SSD1306_128X64::display_image(int x, int y, uint16_t width, uint16_t height, const uint8_t *image)
{
    u8g2_DrawXBM(&m_u8g2, x, y, width, height, image);
    u8g2_SendBuffer(&m_u8g2);
} // display_image