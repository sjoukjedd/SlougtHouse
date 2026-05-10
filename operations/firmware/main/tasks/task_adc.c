/* ==========================================================================
 * VU-AMS Firmware — ADC acquisition task (ADS1256)
 * xTaskCreatePinnedToCore(task_adc, "task_adc", 4096, NULL, 8, NULL, 0)
 * Priority: 8  |  Core: 0  |  Stack: 4096 bytes
 * Author: Kai Müller
 * Date:   2026-05-08
 * ========================================================================== */

#include "task_adc.h"
#include "config.h"
#include "data_blocks.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "task_adc";

/* --------------------------------------------------------------------------
 * ADS1256 command bytes
 * -------------------------------------------------------------------------- */
#define ADS1256_CMD_WAKEUP      0x00
#define ADS1256_CMD_RDATA       0x01
#define ADS1256_CMD_RDATAC      0x03
#define ADS1256_CMD_SDATAC      0x0F
#define ADS1256_CMD_RREG(reg)   (0x10 | ((reg) & 0x0F))
#define ADS1256_CMD_WREG(reg)   (0x50 | ((reg) & 0x0F))
#define ADS1256_CMD_SYNC        0xFC
#define ADS1256_CMD_RESET       0xFE

/* --------------------------------------------------------------------------
 * ADS1256 register addresses
 * -------------------------------------------------------------------------- */
#define ADS1256_REG_STATUS      0x00
#define ADS1256_REG_MUX         0x01
#define ADS1256_REG_ADCON       0x02
#define ADS1256_REG_DRATE       0x03

/* STATUS register bits */
#define ADS1256_STATUS_BUFEN    (1 << 1)   /* Input buffer enable         */
#define ADS1256_STATUS_ACAL     (1 << 2)   /* Auto-calibration enable     */
#define ADS1256_STATUS_ORDER    (0 << 3)   /* MSB-first data output       */

/* ADCON register: CLK[5:4]=0 (CLK off), SDCS[3:2]=0, PGA[2:0]=0 (gain=1) */
#define ADS1256_ADCON_GAIN1     0x00

/* DRATE: 0xA0 = 1000 SPS */
#define ADS1256_DRATE_1000SPS   0xA0

/* MUX: single-ended channel N vs AINCOM (0x8) */
#define ADS1256_MUX_CH(ch)      (((ch) << 4) | 0x08)

/* --------------------------------------------------------------------------
 * Channel mapping (single-ended vs AINCOM)
 * -------------------------------------------------------------------------- */
#define ADC_CH_ECG_RA           0   /* ECG channel 1 (RA lead)     → ecg1[]  */
#define ADC_CH_ECG_LL           1   /* ECG channel 2 (LL lead)     → ecg2[]  */
#define ADC_CH_ECG_V1           2   /* ECG V1 lead  (not stored in a_block_t) */
#define ADC_CH_ICG_SENSE        3   /* ICG sense electrode                    */
#define ADC_CH_ICG_REF          4   /* ICG reference electrode                */
#define ADC_CH_PPG              5   /* PPG (aux, primary via MAX30101)        */
/* Channels 6 and 7 unused */

/* --------------------------------------------------------------------------
 * Queues: block pointers allocated from heap, freed by consumer
 * -------------------------------------------------------------------------- */
QueueHandle_t g_adc_block_queue = NULL;
QueueHandle_t g_z_block_queue   = NULL;  /* Created by app_main, used here */

/* --------------------------------------------------------------------------
 * Module-private state
 * -------------------------------------------------------------------------- */
static spi_device_handle_t  s_adc_spi      = NULL;
static SemaphoreHandle_t    s_drdy_sem     = NULL;

/* A-block accumulation buffer — 250 ECG samples then posted */
static a_block_t            s_current_block;
static uint16_t             s_sample_count = 0;

/* Z-block accumulation buffer — 250 ICG differential samples then posted */
static z_block_t            s_z_block;
static uint16_t             s_z_sample_count = 0;
static uint16_t             s_z_seq          = 0;

/* --------------------------------------------------------------------------
 * DRDY GPIO interrupt service routine
 * -------------------------------------------------------------------------- */
static void IRAM_ATTR drdy_isr_handler(void *arg)
{
    BaseType_t higher_priority_woken = pdFALSE;
    xSemaphoreGiveFromISR(s_drdy_sem, &higher_priority_woken);
    if (higher_priority_woken) {
        portYIELD_FROM_ISR();
    }
}

/* --------------------------------------------------------------------------
 * Low-level SPI helpers — CS is managed manually
 * -------------------------------------------------------------------------- */

static inline void ads1256_cs_low(void)
{
    gpio_set_level(PIN_ADC_CS, 0);
}

static inline void ads1256_cs_high(void)
{
    gpio_set_level(PIN_ADC_CS, 1);
}

/**
 * @brief  Transmit @p tx_len bytes from @p tx_buf (no simultaneous receive).
 *         CS must be held low by caller around the full transaction.
 */
static void ads1256_tx(const uint8_t *tx_buf, size_t tx_len)
{
    spi_transaction_t t = {
        .length    = tx_len * 8,
        .tx_buffer = tx_buf,
        .rx_buffer = NULL,
        .flags     = 0,
    };
    spi_device_polling_transmit(s_adc_spi, &t);
}

/**
 * @brief  Receive @p rx_len bytes into @p rx_buf (MOSI held low).
 *         CS must be held low by caller around the full transaction.
 */
static void ads1256_rx(uint8_t *rx_buf, size_t rx_len)
{
    spi_transaction_t t = {
        .length    = rx_len * 8,
        .rxlength  = rx_len * 8,
        .tx_buffer = NULL,
        .rx_buffer = rx_buf,
        .flags     = 0,
    };
    spi_device_polling_transmit(s_adc_spi, &t);
}

/* --------------------------------------------------------------------------
 * ADS1256 register write helper
 * Writes @p count consecutive registers starting at @p reg.
 * -------------------------------------------------------------------------- */
static void ads1256_wreg(uint8_t reg, const uint8_t *data, uint8_t count)
{
    uint8_t cmd[2] = {
        ADS1256_CMD_WREG(reg),
        (uint8_t)(count - 1),
    };
    ads1256_cs_low();
    ads1256_tx(cmd, 2);
    ads1256_tx(data, count);
    ads1256_cs_high();
}

/* --------------------------------------------------------------------------
 * ADS1256 initialisation sequence
 *
 * Sequence per ADS1256 datasheet §9.5.6:
 *  1. Assert RESET command and wait for DRDY to go low (device ready).
 *  2. Issue SDATAC to stop any continuous conversion mode.
 *  3. Write STATUS, MUX, ADCON, DRATE in one 4-register WREG burst.
 *  4. Issue SYNC + WAKEUP to re-start conversions with new settings.
 * -------------------------------------------------------------------------- */
static void ads1256_init_device(void)
{
    /* --- RESET ------------------------------------------------------------ */
    uint8_t cmd_reset = ADS1256_CMD_RESET;
    ads1256_cs_low();
    ads1256_tx(&cmd_reset, 1);
    ads1256_cs_high();

    /* ADS1256 datasheet: t_s18 — wait ≥ 0.6 ms after RESET before DRDY low */
    vTaskDelay(pdMS_TO_TICKS(2));

    /* Poll DRDY — it goes low when reset is complete and first conversion done */
    uint32_t drdy_wait = 100;   /* max ~100 ms; 0.65 ms expected               */
    while (gpio_get_level(PIN_ADC_DRDY) != 0 && drdy_wait--) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    if (drdy_wait == 0) {
        ESP_LOGE(TAG, "ADS1256 DRDY did not assert after RESET");
    }

    /* --- SDATAC: stop continuous conversion mode (default after reset) ---- */
    uint8_t cmd_sdatac = ADS1256_CMD_SDATAC;
    ads1256_cs_low();
    ads1256_tx(&cmd_sdatac, 1);
    ads1256_cs_high();

    /* t_6 — 50 * t_clkin delay after SDATAC; at 7.68 MHz CLKIN ≈ 7 µs.
     * A 1 ms delay is generous and safe.                                    */
    vTaskDelay(pdMS_TO_TICKS(1));

    /* --- Write STATUS, MUX, ADCON, DRATE in one burst --------------------- */
    uint8_t reg_values[4] = {
        /* STATUS: ORDER=0 (MSB first), ACAL=1, BUFEN=1 */
        ADS1256_STATUS_ACAL | ADS1256_STATUS_BUFEN,
        /* MUX: start with ch0 (AIN0) positive, AINCOM negative */
        ADS1256_MUX_CH(ADC_CH_ECG_RA),
        /* ADCON: CLK off, no SDCS, PGA = 1 (gain = 1) */
        ADS1256_ADCON_GAIN1,
        /* DRATE: 1000 SPS */
        ADS1256_DRATE_1000SPS,
    };
    ads1256_wreg(ADS1256_REG_STATUS, reg_values, 4);

    /* --- SYNC + WAKEUP: apply new settings -------------------------------- */
    uint8_t sync_wake[2] = { ADS1256_CMD_SYNC, ADS1256_CMD_WAKEUP };
    ads1256_cs_low();
    ads1256_tx(&sync_wake[0], 1);
    /* t_11: 24 × t_clkin (≈ 3 µs) minimum between SYNC and WAKEUP.
     * esp_rom_delay_us would be ideal here, but a brief NOP loop or
     * a second SPI transaction after CS de-assert is cleaner.             */
    ads1256_cs_high();
    vTaskDelay(pdMS_TO_TICKS(1));
    ads1256_cs_low();
    ads1256_tx(&sync_wake[1], 1);
    ads1256_cs_high();

    /* Wait for DRDY to confirm first conversion with new settings is done  */
    drdy_wait = 100;
    while (gpio_get_level(PIN_ADC_DRDY) != 0 && drdy_wait--) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    if (drdy_wait == 0) {
        ESP_LOGE(TAG, "ADS1256 DRDY did not assert after config");
    }
}

/* --------------------------------------------------------------------------
 * Read one channel using the MUX → SYNC → WAKEUP → (wait DRDY) → RDATA
 * sequence.
 *
 * Call flow within task_adc:
 *  For each conversion cycle (triggered by DRDY falling):
 *   1. Set MUX to next_channel, send SYNC+WAKEUP to start that conversion.
 *   2. While that conversion runs, RDATA the *previous* channel's result
 *      (the ADS1256 has already settled).
 *
 * This pipelined approach is described in ADS1256 datasheet §9.5.8.
 * The very first call bootstraps the pipeline: set ch0, wait one DRDY,
 * then enter the pipeline loop.
 * -------------------------------------------------------------------------- */

/**
 * @brief  Switch MUX to @p channel and trigger a new conversion.
 *         Must be called immediately after the previous RDATA read, while CS
 *         is still high and before the next DRDY.
 */
static void ads1256_mux_and_sync(uint8_t channel)
{
    uint8_t mux_val = ADS1256_MUX_CH(channel);
    ads1256_wreg(ADS1256_REG_MUX, &mux_val, 1);

    uint8_t sync_wake[2] = { ADS1256_CMD_SYNC, ADS1256_CMD_WAKEUP };
    ads1256_cs_low();
    ads1256_tx(&sync_wake[0], 1);
    ads1256_cs_high();
    /* t_11: ~3 µs between SYNC and WAKEUP — negligible at task level */
    ads1256_cs_low();
    ads1256_tx(&sync_wake[1], 1);
    ads1256_cs_high();
}

/**
 * @brief  Read 3-byte conversion result from ADS1256 and sign-extend to int32_t.
 *         Must be called after DRDY has asserted low.
 */
static int32_t ads1256_rdata(void)
{
    uint8_t cmd   = ADS1256_CMD_RDATA;
    uint8_t raw[3] = { 0, 0, 0 };

    ads1256_cs_low();
    ads1256_tx(&cmd, 1);
    /* t_6: datasheet requires ≥ 50 × t_clkin (≈ 6.5 µs) between RDATA command
     * and first DOUT clock. The SPI driver's setup overhead at 1.92 MHz covers
     * this; if CLKIN > 1.92 MHz were used a short delay would be needed here. */
    ads1256_rx(raw, 3);
    ads1256_cs_high();

    /* Sign-extend 24-bit two's complement to 32-bit */
    int32_t value = ((int32_t)raw[0] << 16) |
                    ((int32_t)raw[1] <<  8) |
                    ((int32_t)raw[2]);
    if (value & 0x00800000) {
        value |= (int32_t)0xFF000000;   /* sign extend */
    }
    return value;
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void task_adc_init(void)
{
    /* Queue holds pointers to heap-allocated a_block_t; consumer frees them */
    g_adc_block_queue = xQueueCreate(4, sizeof(a_block_t *));
    if (g_adc_block_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create adc_block_queue");
        return;
    }

    /* Binary semaphore: ISR gives, task takes — one token per DRDY pulse     */
    s_drdy_sem = xSemaphoreCreateBinary();
    if (s_drdy_sem == NULL) {
        ESP_LOGE(TAG, "Failed to create drdy semaphore");
        return;
    }

    /* --- Configure CS GPIO (output, initially high) ----------------------- */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_ADC_CS),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(PIN_ADC_CS, 1);

    /* --- Configure DRDY GPIO (input, pull-up, falling-edge interrupt) ----- */
    io_conf.pin_bit_mask = (1ULL << PIN_ADC_DRDY);
    io_conf.mode         = GPIO_MODE_INPUT;
    io_conf.pull_up_en   = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type    = GPIO_INTR_NEGEDGE;
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_ADC_DRDY, drdy_isr_handler, NULL);

    /* --- Register ADS1256 SPI device on shared SPI2_HOST bus -------------- */
    spi_device_interface_config_t dev_cfg = {
        .command_bits     = 0,
        .address_bits     = 0,
        .dummy_bits       = 0,
        .mode             = 1,              /* CPOL=0, CPHA=1 — ADS1256 requirement */
        .clock_speed_hz   = SPI_CLK_ADC_HZ,
        .spics_io_num     = -1,             /* CS managed manually                  */
        .queue_size       = 4,
        .flags            = SPI_DEVICE_NO_DUMMY,
    };
    esp_err_t err = spi_bus_add_device(SPI_HOST_DEV, &dev_cfg, &s_adc_spi);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_add_device failed: %s", esp_err_to_name(err));
        return;
    }

    /* --- Initialise A-block accumulation header ---------------------------- */
    memset(&s_current_block, 0, sizeof(s_current_block));
    s_current_block.header.type        = BLOCK_TYPE_A;
    s_current_block.header.version     = BLOCK_VERSION;
    s_current_block.header.payload_len = (uint16_t)(sizeof(s_current_block) -
                                                     sizeof(vuams_block_header_t));

    /* --- Initialise Z-block accumulation buffer ---------------------------- */
    memset(&s_z_block, 0, sizeof(s_z_block));
    s_z_block.block_type = BLOCK_TYPE_Z;
}

/* --------------------------------------------------------------------------
 * Task
 *
 * The ADS1256 runs in pipelined mode (datasheet §9.5.8):
 *
 *   DRDY[n] falls → read result[n-1] from RDATA
 *                 → immediately write MUX[n+1] + SYNC + WAKEUP
 *                 → wait for DRDY[n+1]
 *
 * We cycle through 8 channels per ADS1256 conversion cycle.  At 1000 SPS
 * with 8 channels the effective per-channel rate is 125 SPS — but because
 * we only store ECG channels (ch0=ecg1, ch1=ecg2) we accumulate 250 samples
 * at the original 1000 SPS clock: each complete pass through all 8 channels
 * contributes one sample to ecg1 and one to ecg2.
 * -------------------------------------------------------------------------- */

void task_adc(void *pvParameters)
{
    (void)pvParameters;

    /* Perform chip initialisation here, after FreeRTOS scheduler is running,
     * so that vTaskDelay() is available inside ads1256_init_device().        */
    ads1256_init_device();

    /* Bootstrap the pipeline: prime MUX with channel 0 and wait for the
     * first DRDY pulse so the pipeline starts at a known channel.           */
    ads1256_mux_and_sync(ADC_CH_ECG_RA);

    /* Discard the semaphore token that may have been queued during init     */
    xSemaphoreTake(s_drdy_sem, pdMS_TO_TICKS(10));

    /* Channel pipeline: current_ch is the channel whose conversion is NOW
     * running on the ADS1256.  On each DRDY we read the result for
     * current_ch, then advance to next_ch.                                   */
    uint8_t current_ch = ADC_CH_ECG_RA;

    /* Raw sample staging — indexed by channel number                         */
    int32_t ch_sample[ADC_CHANNELS];
    memset(ch_sample, 0, sizeof(ch_sample));

    /* Channel sequence to cycle through                                      */
    static const uint8_t ch_seq[ADC_CHANNELS] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    uint8_t seq_idx = 0;   /* index into ch_seq for current_ch               */

    while (1) {
        /* Wait for DRDY interrupt — timeout 50 ms (5× expected 10 ms cycle) */
        if (xSemaphoreTake(s_drdy_sem, pdMS_TO_TICKS(50)) != pdTRUE) {
            ESP_LOGE(TAG, "DRDY timeout — ADS1256 may be unresponsive");
            continue;
        }

        /* Advance sequence index for the *next* conversion we are about to
         * trigger.  next_ch is the channel we will set on the MUX now.      */
        uint8_t next_seq_idx = (seq_idx + 1) % ADC_CHANNELS;
        uint8_t next_ch      = ch_seq[next_seq_idx];

        /* Set MUX to next_ch and fire SYNC+WAKEUP.  This must happen as
         * soon as possible after DRDY, before the ADS1256 self-triggers
         * the next internal conversion cycle.                                */
        ads1256_mux_and_sync(next_ch);

        /* Now read the result for current_ch (conversion finished when DRDY
         * fired — result is stable until we read it).                        */
        ch_sample[current_ch] = ads1256_rdata();

        /* Advance pipeline state */
        seq_idx    = next_seq_idx;
        current_ch = next_ch;

        /* When we have just completed one full sweep (seq_idx wraps to 0),
         * pack ECG channels into the A-block and ICG differential into the
         * Z-block accumulation buffers.                                       */
        if (seq_idx == 0) {
            uint64_t now_us = esp_timer_get_time();

            /* ---- A-block (ECG) ------------------------------------------- */
            if (s_sample_count == 0) {
                s_current_block.header.timestamp_us = now_us;
            }

            s_current_block.ecg1[s_sample_count] = ch_sample[ADC_CH_ECG_RA];
            s_current_block.ecg2[s_sample_count] = ch_sample[ADC_CH_ECG_LL];
            s_sample_count++;

            if (s_sample_count >= 250) {
                a_block_t *block = (a_block_t *)malloc(sizeof(a_block_t));
                if (block != NULL) {
                    memcpy(block, &s_current_block, sizeof(a_block_t));
                    if (xQueueSend(g_adc_block_queue, &block,
                                   pdMS_TO_TICKS(5)) != pdTRUE) {
                        ESP_LOGE(TAG, "adc_block_queue full — block dropped");
                        free(block);
                    }
                } else {
                    ESP_LOGE(TAG, "malloc failed for a_block_t");
                }
                s_sample_count = 0;
                memset(&s_current_block, 0, sizeof(s_current_block));
                s_current_block.header.type        = BLOCK_TYPE_A;
                s_current_block.header.version     = BLOCK_VERSION;
                s_current_block.header.payload_len = (uint16_t)(
                    sizeof(s_current_block) - sizeof(vuams_block_header_t));
            }

            /* ---- Z-block (raw ICG differential: ch3 - ch4) --------------- */
            if (s_z_sample_count == 0) {
                s_z_block.timestamp_us = (uint32_t)(now_us & 0xFFFFFFFFU);
            }

            s_z_block.z_raw[s_z_sample_count] =
                ch_sample[ADC_CH_ICG_SENSE] - ch_sample[ADC_CH_ICG_REF];
            s_z_sample_count++;

            if (s_z_sample_count >= Z_BLOCK_SAMPLES) {
                s_z_block.seq = s_z_seq++;
                s_z_block.crc = 0;  /* CRC calculation reserved for future use */

                z_block_t *zblk = (z_block_t *)malloc(sizeof(z_block_t));
                if (zblk != NULL) {
                    memcpy(zblk, &s_z_block, sizeof(z_block_t));
                    if (xQueueSend(g_z_block_queue, &zblk,
                                   pdMS_TO_TICKS(5)) != pdTRUE) {
                        ESP_LOGE(TAG, "z_block_queue full — block dropped");
                        free(zblk);
                    }
                } else {
                    ESP_LOGE(TAG, "malloc failed for z_block_t");
                }
                s_z_sample_count = 0;
                memset(&s_z_block, 0, sizeof(s_z_block));
                s_z_block.block_type = BLOCK_TYPE_Z;
            }
        }
    }
}
