#pragma once

/* ==========================================================================
 * VU-AMS Firmware — PSRAM Ring Buffer
 * Target: ESP32-S3-MINI-1-N8R8
 * Author: Kai Müller
 * Date:   2026-05-08
 *
 * Lock-protected ring buffer allocated in external SPIRAM (PSRAM).
 * Intended for block-level buffering between acquisition tasks and
 * the SD writer / BLE streamer.
 * ========================================================================== */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "config.h"

/* --------------------------------------------------------------------------
 * Ring buffer descriptor
 * The data array itself is allocated from PSRAM during psram_buf_init().
 * -------------------------------------------------------------------------- */
typedef struct {
    uint8_t         *data;          /* Pointer into PSRAM allocation          */
    size_t           capacity;      /* Total usable bytes (PSRAM_RINGBUF_SIZE)*/
    volatile size_t  head;          /* Write index (producer advances)        */
    volatile size_t  tail;          /* Read  index (consumer advances)        */
    SemaphoreHandle_t mutex;        /* Binary mutex — must hold to touch head/tail */
} psram_ringbuf_t;

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

/**
 * @brief  Allocate and initialise the PSRAM ring buffer.
 *         Must be called from app_main before any task is started.
 * @param  buf   Pointer to a psram_ringbuf_t descriptor (caller-allocated, e.g. static).
 * @return true  on success, false if PSRAM allocation or mutex creation failed.
 */
bool psram_buf_init(psram_ringbuf_t *buf);

/**
 * @brief  Write @p len bytes from @p src into the ring buffer.
 *         Blocks until the mutex is obtained.
 * @return Number of bytes actually written (0 if insufficient space).
 */
size_t psram_buf_write(psram_ringbuf_t *buf, const void *src, size_t len);

/**
 * @brief  Read up to @p max_len bytes from the ring buffer into @p dst.
 *         Blocks until the mutex is obtained.
 * @return Number of bytes actually read (0 if buffer empty).
 */
size_t psram_buf_read(psram_ringbuf_t *buf, void *dst, size_t max_len);

/**
 * @brief  Return the number of bytes currently available for reading.
 *         Acquires the mutex briefly.
 */
size_t psram_buf_available(psram_ringbuf_t *buf);

/**
 * @brief  Return the number of free bytes currently available for writing.
 */
size_t psram_buf_free(psram_ringbuf_t *buf);

/**
 * @brief  Discard all data in the buffer (reset head and tail to 0).
 *         Use only in error/recovery paths.
 */
void psram_buf_flush(psram_ringbuf_t *buf);
