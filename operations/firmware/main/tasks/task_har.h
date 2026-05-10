#pragma once

/* ==========================================================================
 * VU-AMS Firmware — Human Activity Recognition task
 * xTaskCreatePinnedToCore: priority 3, core 1, stack 8192
 * Author: Kai Müller
 * Date:   2026-05-10
 *
 * Consumes M-blocks (100 Hz, g_imu_block_queue) and B-blocks
 * (25 Hz, g_baro_block_queue) to classify posture and locomotion in real
 * time.  Features are extracted from 2-second windows with 50% overlap
 * (100-sample hop at 100 Hz).  Classification is rule-based, matching the
 * threshold definitions in har_sad_spec_001.md §2.4.2.
 *
 * Results are published via an X-block (BLOCK_TYPE_X) every 2 seconds to
 * g_x_block_queue, which is consumed by task_block_assembler for both BLE
 * and SD dispatch.  Atomic getters are also provided for any task that
 * needs the current classification without queue overhead.
 * ========================================================================== */

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdbool.h>

/* --------------------------------------------------------------------------
 * Activity class enumeration
 * -------------------------------------------------------------------------- */
typedef enum {
    HAR_UNKNOWN      = 0,
    HAR_LYING        = 1,
    HAR_SITTING      = 2,
    HAR_STANDING     = 3,
    HAR_WALKING      = 4,
    HAR_RUNNING      = 5,
    HAR_CYCLING      = 6,
    HAR_STAIRS_UP    = 7,
    HAR_STAIRS_DOWN  = 8,
} har_class_t;

/* --------------------------------------------------------------------------
 * Queue handle for outgoing X-blocks (x_block_t).
 * Created by task_har_init(); consumed by task_block_assembler.
 * -------------------------------------------------------------------------- */
extern QueueHandle_t g_x_block_queue;

/**
 * @brief  Dedicated M-block input queue for HAR.
 *         task_block_assembler forwards a copy of each m_block_t pointer here
 *         after its own SD/BLE dispatch.  Created by task_har_init().
 *         Depth is shallow (8 blocks = 800 ms) since HAR processes at 1 Hz.
 */
extern QueueHandle_t g_har_m_block_queue;

/**
 * @brief  Dedicated barometer pressure queue for HAR.
 *         task_i2c_sensors enqueues the float pressure value (Pa) alongside
 *         its normal B-block posting.  Created by task_har_init().
 *         Depth 32 covers ~1.3 s of 25 Hz baro data.
 */
extern QueueHandle_t g_har_baro_queue;

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

/**
 * @brief  Initialise internal state and create g_x_block_queue.
 *         Must be called from app_main before the task is spawned.
 */
void task_har_init(void);

/**
 * @brief  FreeRTOS task entry point.  Pass NULL as pvParameters.
 */
void task_har(void *pvParameters);

/**
 * @brief  Return the current HAR activity class.
 *         Thread-safe: reads an atomically updated variable.
 */
har_class_t task_har_get_class(void);

/**
 * @brief  Return the current cadence in steps per minute.
 *         0.0f when the subject is not walking or running.
 */
float task_har_get_cadence_spm(void);

/**
 * @brief  Return the current motion intensity, normalised to [0, 1].
 *         Formula: clamp((mean_rms_mag - 1.0) / 2.0, 0, 1)
 *         0 at rest (~1g), ~0.75 during running (~2.5g).
 */
float task_har_get_motion_intensity(void);
