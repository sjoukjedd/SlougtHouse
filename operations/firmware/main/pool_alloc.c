/* ==========================================================================
 * VU-AMS Firmware — Static pool allocator
 * Author: Kai Müller
 * Date:   2026-05-12
 * ========================================================================== */

#include "pool_alloc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "pool_alloc";

static portMUX_TYPE s_small_mux = portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE s_large_mux = portMUX_INITIALIZER_UNLOCKED;

/* --------------------------------------------------------------------------
 * Pool geometry
 * -------------------------------------------------------------------------- */

#define SMALL_SLOT_SIZE   64u
#define SMALL_SLOT_COUNT  16u

#define LARGE_SLOT_SIZE   512u
#define LARGE_SLOT_COUNT  8u

/* --------------------------------------------------------------------------
 * Storage — placed in BSS (zero-initialised at boot)
 * -------------------------------------------------------------------------- */

static uint8_t  s_small_buf[SMALL_SLOT_COUNT][SMALL_SLOT_SIZE];
static uint16_t s_small_inuse;   /* bitmask, bit i = slot i occupied */

static uint8_t  s_large_buf[LARGE_SLOT_COUNT][LARGE_SLOT_SIZE];
static uint8_t  s_large_inuse;   /* bitmask, bit i = slot i occupied */

/* --------------------------------------------------------------------------
 * pool_init
 * -------------------------------------------------------------------------- */

void pool_init(void)
{
    s_small_inuse = 0;
    s_large_inuse = 0;
    ESP_LOGI(TAG, "Pool init: SMALL %u×%u B, LARGE %u×%u B",
             SMALL_SLOT_COUNT, SMALL_SLOT_SIZE,
             LARGE_SLOT_COUNT, LARGE_SLOT_SIZE);
}

/* --------------------------------------------------------------------------
 * pool_alloc
 * -------------------------------------------------------------------------- */

void *pool_alloc(size_t size)
{
    if (size == 0) {
        return NULL;
    }

    if (size <= SMALL_SLOT_SIZE) {
        /* Search SMALL_POOL for a free slot */
        portENTER_CRITICAL(&s_small_mux);
        int found = -1;
        for (int i = 0; i < (int)SMALL_SLOT_COUNT; i++) {
            if (!(s_small_inuse & (1u << i))) {
                s_small_inuse |= (1u << i);
                found = i;
                break;
            }
        }
        portEXIT_CRITICAL(&s_small_mux);

        if (found < 0) {
            ESP_LOGW(TAG, "SMALL pool full (size=%u)", (unsigned)size);
            return NULL;
        }
        memset(s_small_buf[found], 0, SMALL_SLOT_SIZE);
        return s_small_buf[found];
    }

    if (size <= LARGE_SLOT_SIZE) {
        /* Search LARGE_POOL for a free slot */
        portENTER_CRITICAL(&s_large_mux);
        int found = -1;
        for (int i = 0; i < (int)LARGE_SLOT_COUNT; i++) {
            if (!(s_large_inuse & (1u << i))) {
                s_large_inuse |= (1u << i);
                found = i;
                break;
            }
        }
        portEXIT_CRITICAL(&s_large_mux);

        if (found < 0) {
            ESP_LOGW(TAG, "LARGE pool full (size=%u)", (unsigned)size);
            return NULL;
        }
        memset(s_large_buf[found], 0, LARGE_SLOT_SIZE);
        return s_large_buf[found];
    }

    /* Oversized — fall back to heap; warn so this gets noticed */
    ESP_LOGW(TAG, "pool_alloc: size=%u exceeds max pool slot, falling back to heap",
             (unsigned)size);
    return malloc(size);
}

/* --------------------------------------------------------------------------
 * pool_free
 * -------------------------------------------------------------------------- */

void pool_free(void *ptr)
{
    if (ptr == NULL) {
        return;
    }

    /* Check SMALL_POOL range */
    uint8_t *p = (uint8_t *)ptr;
    if (p >= &s_small_buf[0][0] &&
        p <  &s_small_buf[SMALL_SLOT_COUNT][0]) {
        /* Calculate slot index */
        size_t offset = (size_t)(p - &s_small_buf[0][0]);
        int slot = (int)(offset / SMALL_SLOT_SIZE);
        portENTER_CRITICAL(&s_small_mux);
        s_small_inuse &= ~(1u << slot);
        portEXIT_CRITICAL(&s_small_mux);
        return;
    }

    /* Check LARGE_POOL range */
    if (p >= &s_large_buf[0][0] &&
        p <  &s_large_buf[LARGE_SLOT_COUNT][0]) {
        size_t offset = (size_t)(p - &s_large_buf[0][0]);
        int slot = (int)(offset / LARGE_SLOT_SIZE);
        portENTER_CRITICAL(&s_large_mux);
        s_large_inuse &= ~(1u << slot);
        portEXIT_CRITICAL(&s_large_mux);
        return;
    }

    /* Pointer not in either pool — must be a heap fallback from pool_alloc */
    free(ptr);
}
