/* ==========================================================================
 * VU-AMS Firmware — Application entry point
 * Target: ESP32-S3-MINI-1-N8R8 (dual-core LX7, 240 MHz, 8 MB Flash, 8 MB PSRAM)
 * Author: Kai Müller
 * Date:   2026-05-08
 *
 * Boot sequence:
 *  1. NVS flash initialisation
 *  2. SPI bus initialisation (shared by ADS1256, ICM-20948)
 *  3. I2C bus initialisation (MAX30101, TMP117, BMP390, AD5933)
 *  4. PSRAM ring buffer allocation
 *  5. Per-task init functions and shared queue creation
 *  6. Spawn all tasks via xTaskCreatePinnedToCore
 * ========================================================================== */

#include "config.h"
#include "psram_buffer.h"
#include "data_blocks.h"
#include "tasks/task_adc.h"
#include "tasks/task_imu.h"
#include "tasks/task_i2c_sensors.h"
#include "tasks/task_block_assembler.h"
#include "tasks/task_sd_writer.h"
#include "tasks/task_ble_stream.h"
#include "tasks/task_battery_monitor.h"
#include "tasks/task_watchdog.h"
#include "tasks/task_usb_guard.h"

#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/spi_master.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

static const char *TAG = "main";

/* --------------------------------------------------------------------------
 * Global PSRAM ring buffer — shared between block_assembler and sd_writer
 * -------------------------------------------------------------------------- */
psram_ringbuf_t g_psram_buf;

/* --------------------------------------------------------------------------
 * USB-C connection state — Type BF safety interlock
 * Written only from the usb_guard callback; read by any task via
 * usb_guard_is_connected().  Declared here for boot-time logging.
 * -------------------------------------------------------------------------- */
static volatile bool g_usb_connected = false;

static void on_usb_state_change(bool connected)
{
    g_usb_connected = connected;
    /* Individual tasks poll usb_guard_is_connected() directly; this flag is
     * available here for any future system-level response (e.g. UI, BLE
     * status characteristic update). */
}

/* --------------------------------------------------------------------------
 * Global event groups
 * -------------------------------------------------------------------------- */
EventGroupHandle_t g_system_events   = NULL;  /* EVT_SD_*, EVT_BLE_*, EVT_BATTERY_* */
EventGroupHandle_t g_acq_events      = NULL;  /* EVT_ADC_FRAME, EVT_IMU_FRAME, ... */

/* --------------------------------------------------------------------------
 * Bus initialisation helpers
 * -------------------------------------------------------------------------- */

static void init_spi_bus(void)
{
    spi_bus_config_t bus_cfg = {
        .mosi_io_num     = PIN_SPI_MOSI,
        .miso_io_num     = PIN_SPI_MISO,
        .sclk_io_num     = PIN_SPI_CLK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = 4096,
        .flags           = SPICOMMON_BUSFLAG_MASTER,
    };
    esp_err_t err = spi_bus_initialize(SPI_HOST_DEV, &bus_cfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "SPI bus initialised (MOSI=%d, MISO=%d, CLK=%d)",
                 PIN_SPI_MOSI, PIN_SPI_MISO, PIN_SPI_CLK);
    }
}

static void init_i2c_bus(void)
{
    i2c_config_t i2c_cfg = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = PIN_I2C_SDA,
        .scl_io_num       = PIN_I2C_SCL,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_CLK_HZ,
    };
    esp_err_t err = i2c_param_config(I2C_PORT_NUM, &i2c_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_param_config failed: %s", esp_err_to_name(err));
        return;
    }
    err = i2c_driver_install(I2C_PORT_NUM, I2C_MODE_MASTER, 0, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_driver_install failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "I2C bus initialised (SDA=%d, SCL=%d, %d Hz)",
                 PIN_I2C_SDA, PIN_I2C_SCL, I2C_CLK_HZ);
    }
}

/* --------------------------------------------------------------------------
 * app_main
 * -------------------------------------------------------------------------- */

void app_main(void)
{
    ESP_LOGI(TAG, "VU-AMS firmware starting");

    /* --- Step 1: NVS flash ------------------------------------------------- */
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES ||
        nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition truncated — erasing and reinitialising");
        nvs_flash_erase();
        nvs_flash_init();
    }
    ESP_LOGI(TAG, "NVS flash ready");

    /* --- Step 2: Event groups ---------------------------------------------- */
    g_system_events = xEventGroupCreate();
    g_acq_events    = xEventGroupCreate();
    if (!g_system_events || !g_acq_events) {
        ESP_LOGE(TAG, "Event group creation failed — halting");
        return;
    }

    /* --- Step 3: Bus initialisation ---------------------------------------- */
    init_spi_bus();
    init_i2c_bus();

    /* --- Step 3a: USB-C guard init (Type BF safety interlock) -------------
     * Must run before any recording task is initialised or spawned so that
     * usb_guard_is_connected() returns a valid state from the start.
     * ----------------------------------------------------------------------- */
    task_usb_guard_init();
    usb_guard_register_callback(on_usb_state_change);

    /* --- Step 4: PSRAM ring buffer ----------------------------------------- */
    if (!psram_buf_init(&g_psram_buf)) {
        ESP_LOGE(TAG, "PSRAM ring buffer init failed — halting");
        return;
    }

    /* --- Step 5: Per-task device and queue initialisation -------------------
     * Order matters: output queues must exist before assembler is inited.
     * SD and BLE queues are created first because assembler posts into them.
     * ----------------------------------------------------------------------- */

    /* Z-block queue: created here because task_adc.c uses it but does not own
     * its creation — follows the same pattern as other cross-task queues.     */
    g_z_block_queue = xQueueCreate(4, sizeof(z_block_t *));
    if (g_z_block_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create z_block_queue — halting");
        return;
    }

    task_sd_writer_init();          /* creates g_sd_write_queue, mounts SD  */
    task_ble_stream_init();         /* creates g_ble_tx_queue, inits NimBLE */
    task_adc_init();                /* registers ADS1256 on SPI bus         */
    task_imu_init();                /* registers ICM-20948 on SPI bus       */
    task_i2c_sensors_init();        /* configures MAX30101, TMP117, BMP390, AD5933 */
    task_block_assembler_init();    /* no-op for now, assembler uses above queues  */
    task_battery_monitor_init();    /* configures ADC1 channel for VBatt          */
    task_watchdog_init();           /* configures TWDT                            */

    /* --- Step 6: Spawn tasks -----------------------------------------------
     *
     *  Task                 | Prio | Core | Stack
     *  ---------------------|------|------|-------
     *  task_adc             |  8   |  0   |  4096
     *  task_imu             |  7   |  0   |  4096
     *  task_i2c_sensors     |  6   |  0   |  4096
     *  task_block_assembler |  6   |  1   |  8192
     *  task_sd_writer       |  5   |  1   |  8192
     *  task_ble_stream      |  4   |  1   |  8192
     *  task_watchdog        |  3   |  1   |  2048
     *  task_battery_monitor |  2   |  1   |  2048
     *  task_usb_guard       |  2   |  0   |  2048
     * ----------------------------------------------------------------------- */

    /* Boot-time USB-C check — warn operator before recording tasks start.    */
    if (usb_guard_is_connected()) {
        ESP_LOGW(TAG,
            "USB-C connected at boot — Type BF interlock is ACTIVE. "
            "Recording will be blocked until USB-C is removed.");
    }

    BaseType_t ret;

    ret = xTaskCreatePinnedToCore(task_adc, "task_adc", 4096,
                                  NULL, 8, NULL, 0);
    if (ret != pdPASS) { ESP_LOGE(TAG, "Failed to create task_adc"); }

    ret = xTaskCreatePinnedToCore(task_imu, "task_imu", 4096,
                                  NULL, 7, NULL, 0);
    if (ret != pdPASS) { ESP_LOGE(TAG, "Failed to create task_imu"); }

    ret = xTaskCreatePinnedToCore(task_i2c_sensors, "task_i2c", 4096,
                                  NULL, 6, NULL, 0);
    if (ret != pdPASS) { ESP_LOGE(TAG, "Failed to create task_i2c"); }

    ret = xTaskCreatePinnedToCore(task_block_assembler, "task_asm", 8192,
                                  NULL, 6, NULL, 1);
    if (ret != pdPASS) { ESP_LOGE(TAG, "Failed to create task_asm"); }

    ret = xTaskCreatePinnedToCore(task_sd_writer, "task_sd", 8192,
                                  NULL, 5, NULL, 1);
    if (ret != pdPASS) { ESP_LOGE(TAG, "Failed to create task_sd"); }

    ret = xTaskCreatePinnedToCore(task_ble_stream, "task_ble", 8192,
                                  NULL, 4, NULL, 1);
    if (ret != pdPASS) { ESP_LOGE(TAG, "Failed to create task_ble"); }

    ret = xTaskCreatePinnedToCore(task_watchdog, "task_wdt", 2048,
                                  NULL, 3, NULL, 1);
    if (ret != pdPASS) { ESP_LOGE(TAG, "Failed to create task_wdt"); }

    ret = xTaskCreatePinnedToCore(task_battery_monitor, "task_bat", 2048,
                                  NULL, 2, NULL, 1);
    if (ret != pdPASS) { ESP_LOGE(TAG, "Failed to create task_bat"); }

    ret = xTaskCreatePinnedToCore(task_usb_guard, "task_usbg", 2048,
                                  NULL, 2, NULL, 0);
    if (ret != pdPASS) { ESP_LOGE(TAG, "Failed to create task_usbg"); }

    ESP_LOGI(TAG, "All tasks spawned — app_main returning");
    /* app_main returns here; the FreeRTOS scheduler takes over. */
}
