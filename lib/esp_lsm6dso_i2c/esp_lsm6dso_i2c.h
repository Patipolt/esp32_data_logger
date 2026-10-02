#ifndef ESP_LSM6DSO_I2C_H_
#define ESP_LSM6DSO_I2C_H_

#include "driver/i2c.h"                 // for I2C class
#include "esp_err.h"                    // for ESP_ERROR_CHECK
#include "esp_log.h"                    // for ESP_LOGI

#define LSM6DSO_I2C_ADDRESS_SA0_LOW     0x6AU   // SA0 pin tied to GND
#define LSM6DSO_I2C_ADDRESS_SA0_HIGH    0x6BU   // SA0 pin tied to VDD
#define LSM6DSO_WHO_AM_I_VALUE          0x6CU

#define FUNC_CFG_ACCESS         0x01U   /* R/W */
#define PIN_CTRL                0x02U   /* R/W */
#define FIFO_CTRL1              0x07U   /* R/W */
#define FIFO_CTRL2              0x08U   /* R/W */
#define FIFO_CTRL3              0x09U   /* R/W */
#define FIFO_CTRL4              0x0AU   /* R/W */
#define COUNTER_BDR_REG1        0x0BU   /* R/W */
#define COUNTER_BDR_REG2        0x0CU   /* R/W */
#define INT1_CTRL               0x0DU   /* R/W */
#define INT2_CTRL               0x0EU   /* R/W */
#define WHO_AM_I                0x0FU   /* R */
#define CTRL1_XL                0x10U   /* R/W */
#define CTRL2_G                 0x11U   /* R/W */
#define CTRL3_C                 0x12U   /* R/W */
#define CTRL4_C                 0x13U   /* R/W */
#define CTRL5_C                 0x14U   /* R/W */
#define CTRL6_C                 0x15U   /* R/W */
#define CTRL7_G                 0x16U   /* R/W */
#define CTRL8_XL                0x17U   /* R/W */
#define CTRL9_XL                0x18U   /* R/W */
#define CTRL10_C                0x19U   /* R/W */
#define STATUS_REG              0x1EU   /* R */

#define OUT_TEMP_L              0x20U   /* R */
#define OUT_TEMP_H              0x21U   /* R */
#define OUTX_L_G                0x22U   /* R */
#define OUTX_H_G                0x23U   /* R */
#define OUTY_L_G                0x24U   /* R */
#define OUTY_H_G                0x25U   /* R */
#define OUTZ_L_G                0x26U   /* R */
#define OUTZ_H_G                0x27U   /* R */
#define OUTX_L_A                0x28U   /* R */
#define OUTX_H_A                0x29U   /* R */
#define OUTY_L_A                0x2AU   /* R */
#define OUTY_H_A                0x2BU   /* R */
#define OUTZ_L_A                0x2CU   /* R */
#define OUTZ_H_A                0x2DU   /* R */


// CONFIGURATION BITS
// CTRL3_C
#define SW_RESET                (0x01U<<0)
#define IF_INC                  (0x01U<<2)  // auto-increment register address on multi-byte access
#define BDU                     (0x01U<<6)  // block data update

// CTRL1_XL
#define ODR_XL_POWER_DOWN       (0x00U<<4)
#define ODR_XL_12_5HZ           (0x01U<<4)
#define ODR_XL_26HZ             (0x02U<<4)
#define ODR_XL_52HZ             (0x03U<<4)
#define ODR_XL_104HZ            (0x04U<<4)
#define ODR_XL_208HZ            (0x05U<<4)
#define ODR_XL_416HZ            (0x06U<<4)
#define ODR_XL_833HZ            (0x07U<<4)
#define ODR_XL_1660HZ           (0x08U<<4)
#define ODR_XL_MASK             (0x0FU<<4)

#define FS_XL_2G                (0x00U<<2)
#define FS_XL_4G                (0x02U<<2)
#define FS_XL_8G                (0x03U<<2)
#define FS_XL_16G               (0x01U<<2)
#define FS_XL_MASK              (0x03U<<2)

// CTRL2_G
#define ODR_G_POWER_DOWN        (0x00U<<4)
#define ODR_G_12_5HZ            (0x01U<<4)
#define ODR_G_26HZ              (0x02U<<4)
#define ODR_G_52HZ              (0x03U<<4)
#define ODR_G_104HZ             (0x04U<<4)
#define ODR_G_208HZ             (0x05U<<4)
#define ODR_G_416HZ             (0x06U<<4)
#define ODR_G_833HZ             (0x07U<<4)
#define ODR_G_1660HZ            (0x08U<<4)
#define ODR_G_MASK              (0x0FU<<4)

#define FS_G_250DPS             (0x00U<<2)
#define FS_G_500DPS             (0x01U<<2)
#define FS_G_1000DPS            (0x02U<<2)
#define FS_G_2000DPS            (0x03U<<2)
#define FS_G_MASK               (0x03U<<2)


#define I2C_MASTER_TX_BUF_DISABLE 0      // I2C master doesn't need buffer
#define I2C_MASTER_RX_BUF_DISABLE 0      // I2C master doesn't need buffer

class LSM6DSO_I2C{
    public:
        LSM6DSO_I2C(
            gpio_num_t sda,
            gpio_num_t scl,
            uint32_t frequency,
            i2c_port_t i2c_port);
        virtual ~LSM6DSO_I2C(){};

        int16_t getACCRawX(), getACCRawY(), getACCRawZ();
        int16_t getGYRRawX(), getGYRRawY(), getGYRRawZ();
        int16_t getTempRaw();
        float getACCX(), getACCY(), getACCZ();                   // in g
        float getGYRX(), getGYRY(), getGYRZ();                   // in dps
        float getTemp();                                         // in degrees Celsius

        void readACCX();
        void readACCY();
        void readACCZ();
        void readACCAll();

        void readGYRX();
        void readGYRY();
        void readGYRZ();
        void readGYRAll();

        void readTemp();
        void readAll();

        void setGYROConfig(uint8_t config);
        void setACCConfig(uint8_t config);

        bool isFound() { return m_isFound; }

    private:
        esp_err_t I2C_init();
        esp_err_t i2c_scan(void);
        void setup();
        void safetyDelay();

        void writeRegister(uint8_t addr, uint8_t msg);
        void writeRegister(uint8_t addr, uint8_t* msg, size_t nBytes);
        esp_err_t readRegister(uint8_t addr, uint8_t nBytes);

        gpio_num_t m_sda;
        gpio_num_t m_scl;
        uint32_t m_frequency;
        i2c_port_t m_i2c_port;
        bool m_isFound = false;
        uint8_t m_address = LSM6DSO_I2C_ADDRESS_SA0_LOW;

        uint8_t m_buffer[14] = {0};
        int16_t m_ACCrawX = 0, m_ACCrawY = 0, m_ACCrawZ = 0, m_Tempraw = 0;
        int16_t m_GYRrawX = 0, m_GYRrawY = 0, m_GYRrawZ = 0;
        float m_ACCfsr = 0.0, m_GYRfsr = 0.0;   // sensitivity: g/LSB and dps/LSB
};

#endif // ESP_LSM6DSO_I2C_H_
