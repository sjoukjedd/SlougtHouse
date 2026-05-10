/* ==========================================================================
 * VU-AMS Firmware — PSRAM Ring Buffer Implementation
 * Author: Kai Müller
 * Date:   2026-05-08
 * ========================================================================== */

#include "psram_buffer.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "psram_buf";

/* --------------------------------------------------------------------------
 * Internal helpers (no locking — caller must hold mutex)
 * -------------------------------------------------------------------------- */

static inline size_t buf_used(const psram_ringbuf_t *buf)
{
    if (buf->head >= buf->tail) {
        return buf->head - buf->tail;
    }
    return buf->capacity - (buf->tail - buf->head);
}

static inline size_t buf_free_space(const psram_ringbuf_t *buf)
{
    /* Keep one byte reserved so head==tail always means empty */
    return buf->capacity - buf_used(buf) - 1;
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

bool psram_buf_init(psram_ringbuf_t *buf)
{
    buf->data = (uint8_t *)heap_caps_malloc(PSRAM_RINGBUF_SIZE, MALLOC_CAP_SPIRAM);
    if (buf->data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate %u bytes from PSRAM", PSRAM_RINGBUF_SIZE);
        return false;
    }

    buf->capacity = PSRAM_RINGBUF_SIZE;
    buf->head     = 0;
    buf->tail     = 0;

    buf->mutex = xSemaphoreCreateMutex();
    if (buf->mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create PSRAM buffer mutex");
        heap_caps_free(buf->data);
        buf->data = NULL;
        return false;
    }

    ESP_LOGI(TAG, "PSRAM ring buffer ready: %u bytes at %p",
             (unsigned)PSRAM_RINGBUF_SIZE, (void *)buf->data);
    return true;
}

size_t psram_buf_write(psram_ringbuf_t *buf, const void *src, size_t len)
{
    if (len == 0) {
        return 0;
    }

    if (xSemaphoreTake(buf->mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        ESP_LOGW(TAG, "write: mutex timeout");
        return 0;
    }

    size_t available = buf_free_space(buf);
    size_t to_write  = (len <= available) ? len : 0; /* All-or-nothing write */

    if (to_write == 0) {
        ESP_LOGW(TAG, "write: buffer full, dropping %u bytes", (unsigned)len);
        xSemaphoreGive(buf->mutex);
        return 0;
    }

    /* Write in up to two segments (wrap-around) */
    size_t space_to_end = buf->capacity - buf->head;
    if (to_write <= space_to_end) {
        memcpy(buf->data + buf->head, src, to_write);
    } else {
        memcpy(buf->data + buf->head, src, space_to_end);
        memcpy(buf->data, (const uint8_t *)src + space_to_end, to_write - space_to_end);
    }
    buf->head = (buf->head + to_write) % buf->capacity;

    xSemaphoreGive(buf->mutex);
    return to_write;
}

size_t psram_buf_read(psram_ringbuf_t *buf, void *dst, size_t max_len)
{
    if (max_len == 0) {
        return 0;
    }

    if (xSemaphoreTake(buf->mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        ESP_LOGW(TAG, "read: mutex timeout");
        return 0;
    }

    size_t used    = buf_used(buf);
    size_t to_read = (max_len <= used) ? max_len : used;

    if (to_read == 0) {
        xSemaphoreGive(buf->mutex);
        return 0;
    }

    /* Read in up to two segments (wrap-around) */
    size_t bytes_to_end = buf->capacity - buf->tail;
    if (to_read <= bytes_to_end) {
        memcpy(dst, buf->data + buf->tail, to_read);
    } else {
        memcpy(dst, buf->data + buf->tail, bytes_to_end);
        memcpy((uint8_t *)dst + bytes_to_end, buf->data, to_read - bytes_to_end);
    }
    buf->tail = (buf->tail + to_read) % buf->capacity;

    xSemaphoreGive(buf->mutex);
    return to_read;
}

size_t psram_buf_available(psram_ringbuf_t *buf)
{
    if (xSemaphoreTake(buf->mutex, pdMS_TO_TICKS(5)) != pdTRUE) {
        return 0;
    }
    size_t used = buf_used(buf);
    xSemaphoreGive(buf->mutex);
    return used;
}

size_t psram_buf_free(psram_ringbuf_t *buf)
{
    if (xSemaphoreTake(buf->mutex, pdMS_TO_TICKS(5)) != pdTRUE) {
        return 0;
    }
    size_t free_bytes = buf_free_space(buf);
    xSemaphoreGive(buf->mutex);
    return free_bytes;
}

void psram_buf_flush(psram_ringbuf_t *buf)
{
    if (xSemaphoreTake(buf->mutex, portMAX_DELAY) == pdTRUE) {
        buf->head = 0;
        buf->tail = 0;
        xSemaphoreGive(buf->mutex);
        ESP_LOGW(TAG, "Ring buffer flushed");
    }
}
