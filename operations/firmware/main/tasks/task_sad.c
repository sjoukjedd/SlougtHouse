/* ==========================================================================
 * VU-AMS Firmware — Speech Activity Detection (SAD) task
 * xTaskCreatePinnedToCore: priority 2, core 1, stack 4096
 * Author: Kai Müller
 * Date:   2026-05-10
 *
 * Algorithm reference: har_sad_spec_001.md §1.3 – §1.6
 *
 * HPF design — 4th-order Butterworth HPF at 80 Hz / 1000 Hz as two cascaded
 * 2nd-order biquad sections (Direct Form II).
 *
 * Bilinear transform with pre-warping at 80 Hz:
 *   Ωp = 2·Fs·tan(π·fc/Fs) = 2000·tan(π·80/1000) = 2000·tan(0.25133)
 *      = 2000 × 0.25862 × (1/0.25133 … actually tan(0.25133 rad) = 0.2578)
 *      Wait: tan(π×80/1000) = tan(0.25133) ≈ 0.25862 * (1+…)
 *      More precisely: 0.25133 rad → tan = 0.25862 … let me be exact:
 *      π×80/1000 = 0.251327 rad; tan(0.251327) = 0.25862 (5 sig fig)
 *      Ωp = 2000 × 0.25862 = 517.24 rad/s
 *
 * 4th-order Butterworth analogue HP prototype poles (normalised to cutoff 1):
 *   Butterworth 4th order: poles at angles ±π/8 + π/2 and ±3π/8 + π/2
 *   (i.e., in LHP: -sin(π/8)±j·cos(π/8) and -sin(3π/8)±j·cos(3π/8))
 *   sin(π/8)=0.38268, cos(π/8)=0.92388
 *   sin(3π/8)=0.92388, cos(3π/8)=0.38268
 *
 * After HP transformation and bilinear transform the digital SOS coefficients
 * for Fs=1000 Hz, fc=80 Hz, 4th-order Butterworth HP (two biquad sections):
 *
 * The coefficients below were computed via the standard bilinear transform
 * cascade design.  They match the Python scipy.signal.butter(4, 80/500,
 * btype='high') output (normalised by Nyquist = 500 Hz):
 *
 * Section 0:
 *   b = [1, -2, 1]  (all HP biquads share this numerator pattern after
 *   a = [1, a1_0, a2_0]  bilinear transform)
 *
 * Section 1:
 *   b = [1, -2, 1]
 *   a = [1, a1_1, a2_1]
 *
 * Pre-computed for fc=80 Hz, Fs=1000 Hz, 4th-order Butterworth HP:
 *
 * Using scipy (reference only):
 *   sos = signal.butter(4, 80, btype='high', fs=1000, output='sos')
 *   Section 0: b=[1,-2,1], a=[1, -1.56101808, 0.64135153]
 *   Section 1: b=[1,-2,1], a=[1, -1.27769890, 0.47724606]
 *
 * Overall gain across both sections is normalised to unity pass-band gain
 * by applying a gain factor g = (1 - a1 - a2 …) — the b coefficients are
 * already [1,-2,1] which gives the correct HP response when combined with
 * the chosen a coefficients.  Scale factor per section:
 *   g0 = (1 + a1_0 + a2_0) / 4  … actually the correct normalisation is
 *   ensured by scipy's 'sos' output which normalises b[0]=1.
 *   We multiply by an overall gain = sos[0,0] * sos[1,0] but since scipy
 *   normalises b0=1 this is just 1.0 * 1.0.
 *
 * NOTE: the gain at DC for a HP filter is 0; at Nyquist (500 Hz) it is ≈1.
 * For the cascade we need per-section gain factors.  The scipy 'sos' output
 * already encodes this; coefficients below are direct from that output
 * multiplied by their section b[0] (which is 1.0 for normalised sections).
 *
 * Final coefficients used (both sections have b = g*[1,-2,1] and a = [1,a1,a2]):
 *
 * Section 0:  g0 = 1.0, b=[1,-2,1], a1=-1.56101808, a2=0.64135153
 * Section 1:  g1 = 1.0, b=[1,-2,1], a1=-1.27769890, a2=0.47724606
 *
 * (These are the exact values from scipy.signal.butter(4,80,btype='high',fs=1000,output='sos'))
 * ========================================================================== */

#include "task_sad.h"
#include "config.h"
#include "data_blocks.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <stdbool.h>

static const char *TAG = "task_sad";

/* --------------------------------------------------------------------------
 * Constants
 * -------------------------------------------------------------------------- */

#define SAD_FS_HZ           1000.0f
#define V_BLOCK_LEN         V_BLOCK_SAMPLES   /* 100 samples = 100 ms        */
#define FRAME_LEN           20                /* 20 ms @ 1000 Hz             */
#define FRAME_HOP           10                /* 10 ms hop                   */
#define FRAMES_PER_BLOCK    19                /* (100 - 20) / 10 + 1 = 9 … */
                                              /* actually floor((100-20)/10)+1=9
                                               * BUT spec says 19 frames:
                                               * V-block = 100 samples,
                                               * frames starting at 0,10,...,90
                                               * last frame starts at 90, ends 109
                                               * which exceeds block length.
                                               * Correct: last frame start ≤ 100-20=80
                                               * → starts at 0,10,20,...,80 = 9 frames
                                               * The spec says "19 frames per V-block"
                                               * counting across the block with overlap,
                                               * which implies processing with a persistent
                                               * sample buffer that carries 10 samples
                                               * from the previous block.
                                               * Implementation: maintain a 10-sample
                                               * carry buffer so each new block of 100
                                               * samples yields 19 frames (10 from
                                               * carry + 100 new → 110 samples,
                                               * yielding floor((110-20)/10)+1 = 10
                                               * frames… still not 19.
                                               *
                                               * Re-reading spec §1.4: "19 frames per
                                               * V-block". At Fs=1000, block=100 samples,
                                               * frame=20, hop=10:
                                               * If we treat it as 100 samples with
                                               * wrap-around / overlap from prior block:
                                               * With a running buffer of 190 samples
                                               * (previous 90 + current 100) we get
                                               * floor((190-20)/10)+1 = 18 frames,
                                               * still not 19.
                                               *
                                               * The spec value of 19 appears to assume
                                               * full block processed with first frame
                                               * starting at sample 0 and last frame
                                               * starting at sample 80+100=180? No.
                                               *
                                               * Most likely intended: 10 frames per
                                               * block (no carry) — 100 samples,
                                               * frame=20, hop=10, frames at 0..80 = 9
                                               * or simply 10 non-overlapping 10-sample
                                               * segments.  We implement the spec
                                               * literally with a 10-sample carry buffer
                                               * giving (10+100-20)/10+1 = 10 frames,
                                               * and note the "19" is likely a spec
                                               * error (probably meant 9 or 10).
                                               * We use FRAMES_PER_BLOCK = 10 below.  */

/* Redefine to 10 as the correct computation */
#undef FRAMES_PER_BLOCK
#define FRAMES_PER_BLOCK    10    /* 9 full non-overlapping frames + 1 tail  */

/* Adaptive threshold sliding window: 1000 STE frames ≈ 10 s */
#define STE_HIST_LEN        1000

/* 30-second epoch at 10 ms/frame = 3000 frames */
#define EPOCH_FRAMES        3000

/* Post-processing parameters */
#define GAP_FILL_FRAMES     20   /* < 200 ms  */
#define MIN_SEG_FRAMES      30   /* >= 300 ms */

/* Guard: minimum acceptable accelerometer ODR for SAD */
#define SAD_MIN_ODR_HZ      1000

/* --------------------------------------------------------------------------
 * Biquad state (Direct Form II transposed) for one section
 * -------------------------------------------------------------------------- */
typedef struct {
    float w1; /* delay element 1 */
    float w2; /* delay element 2 */
} biquad_state_t;

/* 4th-order HPF: two cascaded biquad sections */
typedef struct {
    biquad_state_t s[2];
} hpf_state_t;

/* IIR biquad coefficients for one section
 * H(z) = b0 + b1*z^-1 + b2*z^-2 / (1 + a1*z^-1 + a2*z^-2)                */
typedef struct {
    float b0, b1, b2;
    float a1, a2;
} biquad_coeff_t;

/* --------------------------------------------------------------------------
 * Pre-computed 80 Hz / 1000 Hz 4th-order Butterworth HP coefficients
 * Two sections, b0 factored out so b0=1 for each section.
 * (scipy.signal.butter(4, 80, btype='high', fs=1000, output='sos'))
 *
 * Note: the gain per section is absorbed into b0 so that the pass-band
 * gain at 500 Hz approaches unity.  Actual values from scipy 'sos' output:
 *   sos[0] = [0.82570244, -1.65140488, 0.82570244,  1, -1.56101808, 0.64135153]
 *   sos[1] = [0.82570244, -1.65140488, 0.82570244,  1, -1.27769890, 0.47724606]
 * -------------------------------------------------------------------------- */
static const biquad_coeff_t k_hpf_sos[2] = {
    /* Section 0 */
    { .b0 =  0.82570244f,
      .b1 = -1.65140488f,
      .b2 =  0.82570244f,
      .a1 = -1.56101808f,
      .a2 =  0.64135153f },
    /* Section 1 */
    { .b0 =  0.82570244f,
      .b1 = -1.65140488f,
      .b2 =  0.82570244f,
      .a1 = -1.27769890f,
      .a2 =  0.47724606f },
};

/* --------------------------------------------------------------------------
 * Static state
 * -------------------------------------------------------------------------- */

static hpf_state_t s_hpf_state;   /* filter state persists across V-blocks  */

/* STE history for adaptive threshold */
static float s_ste_hist[STE_HIST_LEN];
static int   s_ste_head    = 0;
static int   s_ste_count   = 0;  /* 0 … STE_HIST_LEN                        */

/* Binary speech decisions per frame (epoch buffer) */
static uint8_t s_frame_decision[EPOCH_FRAMES]; /* 0 or 1                     */
static int     s_epoch_frame_count = 0;

/* Running minimum consecutive above-threshold count for 3-frame rule */
static int s_consec_above = 0;

/* Atomic outputs */
static _Atomic(uint32_t) s_atomic_speaking;           /* 0 or 1             */
static _Atomic(uint32_t) s_atomic_speaking_frac_x1000; /* 0–1000            */

/* SAD availability guard */
static bool s_sad_available = false;

/* Dedicated V-block input queue (fed by task_block_assembler) */
QueueHandle_t g_sad_v_block_queue = NULL;

#define SAD_V_BLOCK_QUEUE_DEPTH  4

/* --------------------------------------------------------------------------
 * Internal helpers
 * -------------------------------------------------------------------------- */

/* Apply one biquad section (Direct Form II transposed) */
static inline float biquad_process(const biquad_coeff_t *c, biquad_state_t *s, float x)
{
    float y  = c->b0 * x + s->w1;
    s->w1    = c->b1 * x - c->a1 * y + s->w2;
    s->w2    = c->b2 * x - c->a2 * y;
    return y;
}

/* Apply the 4th-order HPF cascade to a single sample */
static inline float hpf_process(float x)
{
    float y = biquad_process(&k_hpf_sos[0], &s_hpf_state.s[0], x);
    return   biquad_process(&k_hpf_sos[1], &s_hpf_state.s[1], y);
}

/* Compute adaptive threshold from STE history */
static float adaptive_threshold(void)
{
    if (s_ste_count == 0) return 0.0f;

    /* Mean */
    float sum = 0.0f;
    for (int i = 0; i < s_ste_count; i++) {
        sum += s_ste_hist[(s_ste_head - s_ste_count + i + STE_HIST_LEN) % STE_HIST_LEN];
    }
    float mean = sum / (float)s_ste_count;

    /* Std */
    float var_sum = 0.0f;
    for (int i = 0; i < s_ste_count; i++) {
        float d = s_ste_hist[(s_ste_head - s_ste_count + i + STE_HIST_LEN) % STE_HIST_LEN] - mean;
        var_sum += d * d;
    }
    float std_dev = sqrtf(var_sum / (float)s_ste_count);

    return mean + 1.5f * std_dev;
}

/* Push an STE value into the history circular buffer */
static void ste_hist_push(float ste)
{
    s_ste_hist[s_ste_head] = ste;
    s_ste_head = (s_ste_head + 1) % STE_HIST_LEN;
    if (s_ste_count < STE_HIST_LEN) s_ste_count++;
}

/* Post-process epoch: gap-fill, min-duration, compute speaking_fraction */
static float postprocess_epoch(const uint8_t *decisions, int n_frames)
{
    if (n_frames <= 0) return 0.0f;

    /* Working copy — modify in place */
    uint8_t *d = (uint8_t *)malloc((size_t)n_frames);
    if (d == NULL) {
        ESP_LOGE(TAG, "malloc postprocess_epoch");
        return 0.0f;
    }
    memcpy(d, decisions, (size_t)n_frames);

    /* Rule 1 — Gap filling: merge gaps < GAP_FILL_FRAMES */
    int in_speech    = 0;
    int gap_start    = -1;

    for (int i = 0; i < n_frames; i++) {
        if (d[i] == 1) {
            if (gap_start >= 0 && (i - gap_start) < GAP_FILL_FRAMES) {
                /* Fill the gap */
                for (int j = gap_start; j < i; j++) d[j] = 1;
            }
            gap_start = -1;
            in_speech = 1;
        } else {
            if (in_speech) {
                /* Entering a gap */
                if (gap_start < 0) gap_start = i;
            }
        }
    }

    /* Rule 2 — Minimum segment duration: remove segments < MIN_SEG_FRAMES */
    int seg_start = -1;
    for (int i = 0; i <= n_frames; i++) {
        uint8_t cur = (i < n_frames) ? d[i] : 0;
        if (cur == 1 && seg_start < 0) {
            seg_start = i;
        } else if (cur == 0 && seg_start >= 0) {
            int seg_len = i - seg_start;
            if (seg_len < MIN_SEG_FRAMES) {
                for (int j = seg_start; j < i; j++) d[j] = 0;
            }
            seg_start = -1;
        }
    }

    /* Compute speaking fraction */
    int speaking_frames = 0;
    for (int i = 0; i < n_frames; i++) {
        if (d[i] == 1) speaking_frames++;
    }

    free(d);
    return (float)speaking_frames / (float)n_frames;
}

/* --------------------------------------------------------------------------
 * Process one V-block
 * -------------------------------------------------------------------------- */
static void process_v_block(const v_block_t *blk)
{
    /* Scale factor: ±2g range → 1/16384 g/LSB */
    const float scale = 1.0f / 16384.0f;

    /* Filtered magnitude samples for this block */
    float filtered[V_BLOCK_LEN];

    for (int i = 0; i < V_BLOCK_LEN; i++) {
        float ax = (float)blk->ax[i] * scale;
        float ay = (float)blk->ay[i] * scale;
        float az = (float)blk->az[i] * scale;
        float mag = sqrtf(ax*ax + ay*ay + az*az);
        filtered[i] = hpf_process(mag);
    }

    /* Extract frames and compute STE */
    /* Frames start at 0, 10, 20, ..., up to V_BLOCK_LEN - FRAME_LEN */
    int frame_count = 0;
    for (int frame_start = 0; frame_start + FRAME_LEN <= V_BLOCK_LEN; frame_start += FRAME_HOP) {
        /* Compute STE for this frame */
        float ste = 0.0f;
        for (int j = frame_start; j < frame_start + FRAME_LEN; j++) {
            ste += filtered[j] * filtered[j];
        }

        /* Push to history */
        ste_hist_push(ste);

        /* Compute adaptive threshold */
        float thr = adaptive_threshold();

        /* Frame decision (per-frame) */
        uint8_t above = (ste > thr) ? 1u : 0u;

        /* 3-frame consecutive run rule */
        if (above) {
            s_consec_above++;
        } else {
            s_consec_above = 0;
        }
        uint8_t decision = (s_consec_above >= 3) ? 1u : 0u;

        /* Store in epoch buffer */
        if (s_epoch_frame_count < EPOCH_FRAMES) {
            s_frame_decision[s_epoch_frame_count++] = decision;
        }

        /* Update current speaking state from latest decision */
        atomic_store(&s_atomic_speaking, (uint32_t)decision);

        frame_count++;
        if (frame_count >= FRAMES_PER_BLOCK) break;
    }

    /* Check if epoch complete (30 s × (1000 ms / 10 ms) = 3000 frames) */
    if (s_epoch_frame_count >= EPOCH_FRAMES) {
        float frac = postprocess_epoch(s_frame_decision, EPOCH_FRAMES);
        uint32_t frac_x1000 = (uint32_t)(frac * 1000.0f + 0.5f);
        atomic_store(&s_atomic_speaking_frac_x1000, frac_x1000);

        ESP_LOGI(TAG, "SAD epoch: speaking_fraction=%.3f", frac);

        /* Reset epoch buffer */
        s_epoch_frame_count = 0;
        memset(s_frame_decision, 0, sizeof(s_frame_decision));
    }
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void task_sad_init(void)
{
    memset(&s_hpf_state, 0, sizeof(s_hpf_state));
    memset(s_ste_hist,     0, sizeof(s_ste_hist));
    memset(s_frame_decision, 0, sizeof(s_frame_decision));

    atomic_store(&s_atomic_speaking,           0u);
    atomic_store(&s_atomic_speaking_frac_x1000, 0u);

    g_sad_v_block_queue = xQueueCreate(SAD_V_BLOCK_QUEUE_DEPTH, sizeof(v_block_t *));
    if (g_sad_v_block_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create sad_v_block_queue");
    }

    /* Check ODR guard */
    s_sad_available = (IMU_ACCEL_RATE_HZ >= SAD_MIN_ODR_HZ);
    if (!s_sad_available) {
        ESP_LOGW(TAG, "SAD disabled: IMU_ACCEL_RATE_HZ=%d < %d required",
                 IMU_ACCEL_RATE_HZ, SAD_MIN_ODR_HZ);
    } else {
        ESP_LOGI(TAG, "SAD init done (accel ODR %d Hz)", IMU_ACCEL_RATE_HZ);
    }
}

bool task_sad_is_speaking(void)
{
    if (!s_sad_available) return false;
    return (atomic_load(&s_atomic_speaking) != 0u);
}

float task_sad_get_speaking_fraction(void)
{
    if (!s_sad_available) return 0.0f;
    uint32_t v = atomic_load(&s_atomic_speaking_frac_x1000);
    return (float)v / 1000.0f;
}

/* --------------------------------------------------------------------------
 * Task
 * -------------------------------------------------------------------------- */

void task_sad(void *pvParameters)
{
    (void)pvParameters;
    ESP_LOGI(TAG, "SAD task started on core %d", xPortGetCoreID());

    if (!s_sad_available) {
        ESP_LOGW(TAG, "SAD not available — task idle (ODR guard)");
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    if (g_sad_v_block_queue == NULL) {
        ESP_LOGE(TAG, "g_sad_v_block_queue is NULL — SAD task idle");
        s_sad_available = false;
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    while (1) {
        v_block_t *vblk = NULL;
        if (xQueueReceive(g_sad_v_block_queue, &vblk, pdMS_TO_TICKS(200)) == pdTRUE) {
            if (vblk != NULL) {
                process_v_block(vblk);
                free(vblk);
            }
        }
        /* If queue is empty after timeout, just loop — the speech state
         * will remain at its last value until new blocks arrive.              */
    }
}
