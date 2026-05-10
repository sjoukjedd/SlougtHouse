#pragma once

/* ==========================================================================
 * VU-AMS Firmware — Speech Activity Detection (SAD) task
 * xTaskCreatePinnedToCore: priority 2, core 1, stack 4096
 * Author: Kai Müller
 * Date:   2026-05-10
 *
 * Detects speech activity from body-conducted vocal vibrations using the
 * high-ODR (1 kHz) accelerometer V-blocks.
 *
 * Per V-block (100 samples, 100 ms):
 *   1. Compute accel magnitude per sample.
 *   2. Apply 4th-order 80 Hz Butterworth HPF (two cascaded biquad sections,
 *      Direct Form II, state preserved across blocks).
 *   3. Frame into 20-sample (20 ms) windows, 10-sample hop (19 frames/block).
 *   4. Compute Short-Time Energy (STE) per frame.
 *   5. Adaptive threshold: mean + 1.5 × std of last 1000 STE frames (~10 s).
 *   6. Speech decision: frame above threshold AND 3+ consecutive frames.
 *   7. Post-processing every 30-second epoch: gap-fill < 200 ms, remove
 *      segments < 300 ms, compute speaking_fraction.
 *
 * Guard: if g_v_block_queue is NULL or IMU accelerometer ODR < 1000 Hz,
 * s_sad_available is set false and all outputs are zero / false.
 * ========================================================================== */

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdbool.h>

/**
 * @brief  Dedicated V-block input queue for SAD.
 *         task_block_assembler forwards a copy of each v_block_t pointer here
 *         after its own SD dispatch.  Created by task_sad_init().
 *         Depth 4 covers 400 ms of 1 kHz accel data.
 */
extern QueueHandle_t g_sad_v_block_queue;

/**
 * @brief  Initialise filter state, threshold buffer, and internal counters.
 *         Must be called from app_main before the task is spawned.
 */
void task_sad_init(void);

/**
 * @brief  FreeRTOS task entry point.  Pass NULL as pvParameters.
 */
void task_sad(void *pvParameters);

/**
 * @brief  Return whether the subject is currently speaking.
 *         Thread-safe: reads an atomically updated flag.
 * @return true while a speech segment is detected, false otherwise.
 *         Always false when sad_available is false.
 */
bool task_sad_is_speaking(void);

/**
 * @brief  Return the speaking fraction in the last completed 30-second epoch.
 *         Range 0.0–1.0.  Returns 0.0 when sad_available is false.
 */
float task_sad_get_speaking_fraction(void);
