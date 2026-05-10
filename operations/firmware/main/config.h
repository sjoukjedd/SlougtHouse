#pragma once

#include <stdint.h>

/* ==========================================================================
 * VU-AMS Firmware — Hardware Configuration
 * Target: ESP32-S3-MINI-1-N8R8 (dual-core LX7, 240 MHz, 8 MB Flash, 8 MB PSRAM)
 * Author: Kai Müller
 * Date:   2026-05-08
 * ========================================================================== */

/* --------------------------------------------------------------------------
 * SPI Bus (VSPI / SPI2_HOST)
 * Shared by ADS1256 (ADC) and ICM-20948 (IMU)
 * -------------------------------------------------------------------------- */
#define SPI_HOST_DEV        SPI2_HOST
#define PIN_SPI_MOSI        11
#define PIN_SPI_MISO        13
#define PIN_SPI_CLK         12

/* ADS1256 24-bit ADC */
#define PIN_ADC_CS          10
#define PIN_ADC_DRDY        9       /* Data-ready interrupt input — active low */
#define SPI_CLK_ADC_HZ      1920000 /* 1.92 MHz — ADS1256 max for 24-bit, SPI mode 1 */
#define ADC_CHANNELS        8
#define ADC_SAMPLE_RATE_HZ  1000

/* ICM-20948 9-axis IMU */
#define PIN_IMU_CS          8
#define SPI_CLK_IMU_HZ      7000000 /* 7 MHz — ICM-20948 max */
#define IMU_SAMPLE_RATE_HZ  100
#define IMU_GYRO_RATE_HZ    100     /* Gyroscope ODR [Hz] — GYRO_SMPLRT_DIV=10  */
#define IMU_ACCEL_RATE_HZ   1000    /* Accelerometer ODR [Hz] for SAD — ACCEL_SMPLRT_DIV=0 */

/* --------------------------------------------------------------------------
 * I²C Bus
 * Owned exclusively by TASK_I2C_SENSORS — no mutex needed.
 * -------------------------------------------------------------------------- */
#define I2C_PORT_NUM        I2C_NUM_0
#define PIN_I2C_SDA         5
#define PIN_I2C_SCL         6
#define I2C_CLK_HZ          400000  /* 400 kHz Fast Mode */

/* I²C device addresses */
#define I2C_ADDR_MAX30101   0x57    /* PPG sensor — fixed */
#define I2C_ADDR_TMP117     0x48    /* Temperature — ADD0 to GND */
#define I2C_ADDR_BMP390     0x77    /* Barometer — SDO to VDD */
#define I2C_ADDR_AD5933     0x0D    /* Impedance analyser — fixed */

/* MAX30101 PPG */
#define PIN_MAX30101_INT    3       /* FIFO watermark interrupt — active low */
#define PPG_FIFO_WATERMARK  25      /* Samples before interrupt fires */
#define PPG_SAMPLE_RATE_HZ  100

/* TMP117 temperature */
#define TMP117_SAMPLE_RATE_HZ   4

/* BMP390 barometer */
#define I2C_ADDR_BARO           I2C_ADDR_BMP390  /* Alias used by baro driver */
#define BARO_SAMPLE_RATE_HZ     25               /* 25 Hz — stair-climb detection */

/* --------------------------------------------------------------------------
 * SD Card — SDMMC native 4-bit mode (SDMMC_HOST_SLOT_1)
 * Pins are dedicated to SDMMC; SPI2_HOST is now exclusive to ADS1256 + IMU.
 * -------------------------------------------------------------------------- */
#define PIN_SDMMC_CLK       36      /* SDMMC CLK  */
#define PIN_SDMMC_CMD       35      /* SDMMC CMD  */
#define PIN_SDMMC_D0        37      /* SDMMC D0   */
#define PIN_SDMMC_D1        38      /* SDMMC D1   */
#define PIN_SDMMC_D2        33      /* SDMMC D2   */
#define PIN_SDMMC_D3        34      /* SDMMC D3   */
#define PIN_SDMMC_CD        39      /* Card detect — active low, internal pull-up */

/* --------------------------------------------------------------------------
 * Oscillator monitor — 32 kHz Epson SG-210STF via PCNT
 * -------------------------------------------------------------------------- */
#define PIN_OSC_MONITOR     4       /* PCNT input — 32 kHz square wave */

/* --------------------------------------------------------------------------
 * Battery ADC
 * -------------------------------------------------------------------------- */
#define PIN_BATT_ADC        1       /* GPIO1 = ADC1_CH0 */
#define BATT_VMIN_MV        3000    /* LiPo empty: 3.0 V */
#define BATT_VMAX_MV        4200    /* LiPo full:  4.2 V */
#define BATT_LOW_PCT        15      /* % SOC — post BATTERY_LOW event */
#define BATT_CRITICAL_PCT   5       /* % SOC — trigger graceful shutdown */

/* --------------------------------------------------------------------------
 * USB-C detect — IEC 60601-1 Type BF safety interlock
 *
 * GPIO20 is connected to USB_VBUS via a 100 kΩ / GND resistor divider.
 * Pin reads HIGH when USB-C is connected, LOW when on battery only.
 *
 * SAFETY REQUIREMENT: Recording MUST NOT proceed while this pin is HIGH.
 * Leakage current through the USB charging path can reach the patient
 * electrodes (ECG/ICG), violating the Type BF patient-isolation limit
 * (IEC 60601-1 clause 8.7.3).  The USB guard task enforces this interlock.
 * -------------------------------------------------------------------------- */
#define PIN_USB_DETECT      20      /* High = USB-C VBUS present             */

/* --------------------------------------------------------------------------
 * GATT service and characteristic UUIDs
 * Identical strings must be used by Chen's iOS stack.
 * -------------------------------------------------------------------------- */
#define VUAMS_SERVICE_UUID      "A5D5B001-5A5A-4B4B-8888-1A2B3C4D5E6F"
#define VUAMS_A_BLOCK_UUID      "A5D5B002-5A5A-4B4B-8888-1A2B3C4D5E6F"  /* ECG/ICG */
#define VUAMS_I_BLOCK_UUID      "A5D5B003-5A5A-4B4B-8888-1A2B3C4D5E6F"  /* ICG derived */
#define VUAMS_M_BLOCK_UUID      "A5D5B004-5A5A-4B4B-8888-1A2B3C4D5E6F"  /* IMU */
#define VUAMS_P_BLOCK_UUID      "A5D5B005-5A5A-4B4B-8888-1A2B3C4D5E6F"  /* PPG */
#define VUAMS_S_BLOCK_UUID      "A5D5B006-5A5A-4B4B-8888-1A2B3C4D5E6F"  /* SCL/EDA */
#define VUAMS_T_BLOCK_UUID      "A5D5B007-5A5A-4B4B-8888-1A2B3C4D5E6F"  /* Temperature */
#define VUAMS_STATUS_UUID       "A5D5B009-5A5A-4B4B-8888-1A2B3C4D5E6F"  /* Status */
#define VUAMS_CONTROL_UUID      "A5D5B00A-5A5A-4B4B-8888-1A2B3C4D5E6F"  /* Control */
#define VUAMS_X_BLOCK_UUID      "A5D5B00D-5A5A-4B4B-8888-1A2B3C4D5E6F"  /* HAR/SAD context (X-block) */

/* --------------------------------------------------------------------------
 * FreeRTOS inter-task communication — queue depths
 * -------------------------------------------------------------------------- */
#define SD_BLOCK_QUEUE_DEPTH    32
#define BLE_BLOCK_QUEUE_DEPTH   16

/* --------------------------------------------------------------------------
 * PSRAM ring buffer
 * -------------------------------------------------------------------------- */
#define PSRAM_RINGBUF_SIZE      (7 * 1024 * 1024)   /* 7 MB — leaves 1 MB for heap */
#define PSRAM_SAFE_MARGIN       (256 * 1024)         /* 256 KB — drop margin on overflow */

/* --------------------------------------------------------------------------
 * Acquisition event group bits
 * -------------------------------------------------------------------------- */
#define EVT_ADC_FRAME       (1 << 0)
#define EVT_IMU_FRAME       (1 << 1)
#define EVT_PPG_FRAME       (1 << 2)
#define EVT_TEMP_FRAME      (1 << 3)
#define EVT_BARO_FRAME      (1 << 4)

/* --------------------------------------------------------------------------
 * System event group bits
 * -------------------------------------------------------------------------- */
#define EVT_SD_READY        (1 << 0)
#define EVT_SD_ERROR        (1 << 1)
#define EVT_SD_FULL         (1 << 2)
#define EVT_SD_WRITING      (1 << 3)
#define EVT_BLE_CONNECTED   (1 << 4)
#define EVT_BLE_STREAMING   (1 << 5)
#define EVT_BLE_DROP        (1 << 6)
#define EVT_BATTERY_LOW     (1 << 7)
#define EVT_BATTERY_CRIT    (1 << 8)
#define EVT_SYSTEM_FAULT    (1 << 9)

/* --------------------------------------------------------------------------
 * System command IDs (posted to system_cmd_queue)
 * -------------------------------------------------------------------------- */
typedef enum {
    CMD_RECORDING_START = 0x01,
    CMD_RECORDING_STOP  = 0x02,
    CMD_SHUTDOWN        = 0x03,
} system_cmd_t;
