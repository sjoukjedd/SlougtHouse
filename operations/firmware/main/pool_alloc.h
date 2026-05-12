#pragma once

/* ==========================================================================
 * VU-AMS Firmware — Static pool allocator
 * Author: Kai Müller
 * Date:   2026-05-12
 *
 * Two-tier static pool replacing heap malloc() in the block assembler.
 *
 * SMALL_POOL: 16 slots × 64 bytes  — b_block, z_block, v_block (and small
 *             dispatch copies of s_block, t_block, p_block, y_block, i_block)
 * LARGE_POOL:  8 slots × 512 bytes — a_block dispatch copies, m_block copies
 *
 * Thread-safe via FreeRTOS portENTER_CRITICAL / portEXIT_CRITICAL.
 * pool_alloc() returns NULL if the appropriate pool is full; callers must
 * handle NULL exactly as they would a failed malloc().
 * ========================================================================== */

#include <stddef.h>

/**
 * @brief  Initialise both pools.  Must be called once before any pool_alloc()
 *         call, typically from task_block_assembler_init().
 */
void  pool_init(void);

/**
 * @brief  Allocate a slot from the appropriate pool.
 *
 * Routing logic:
 *   size <= 64  → SMALL_POOL (16 slots × 64 B)
 *   size <= 512 → LARGE_POOL  (8 slots × 512 B)
 *   size >  512 → falls back to heap malloc() with an ESP_LOGW warning
 *
 * @param  size  Requested allocation size in bytes.
 * @return Pointer to a zero-initialised slot, or NULL if the pool is full.
 */
void *pool_alloc(size_t size);

/**
 * @brief  Return a slot to its pool.  Passing NULL is safe (no-op).
 *         Passing a pointer that did not originate from pool_alloc() is
 *         undefined behaviour.
 */
void  pool_free(void *ptr);
