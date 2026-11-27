#include "sip_audio_capture.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>
#include "audio_manip.h"
#include "esp_g711.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define MIC_SAMPLES 160
#define MIC_PERIOD_US 20000
#define MIC_QUEUE_FRAMES 12

typedef struct {
    int64_t timestamp_us;
    unsigned generation;
    uint8_t data[MIC_SAMPLES];
} mic_frame_t;

static QueueHandle_t s_queue;
static TaskHandle_t s_task;
static atomic_bool s_active;
static atomic_uint s_generation;
static atomic_uint s_captured, s_samples_sent, s_empty, s_dropped, s_errors;
/* Only the SDK's audio send callback accesses the partial-frame cursor. */
static mic_frame_t s_current;
static unsigned s_offset = MIC_SAMPLES;

typedef struct {
    unsigned generation;
    bool valid;
    int64_t next_timestamp;
} mic_clock_t;

static void mic_capture_frame(mic_clock_t *clock)
{
    unsigned generation = atomic_load(&s_generation);
    if (generation != clock->generation) {
        clock->generation = generation;
        clock->valid = false;
    }
    int16_t stereo[MIC_SAMPLES * 2];
    int ret = audio_read_mic(stereo, sizeof(stereo));
    if (!atomic_load(&s_active) || generation != atomic_load(&s_generation)) return;
    if (ret != ESP_CODEC_DEV_OK) {
        atomic_fetch_add(&s_errors, 1);
        clock->valid = false;
        vTaskDelay(pdMS_TO_TICKS(10));
        return;
    }
    if (!clock->valid) {
        clock->next_timestamp = esp_timer_get_time();
        clock->valid = true;
    }
    mic_frame_t frame = {.timestamp_us = clock->next_timestamp, .generation = generation};
    clock->next_timestamp += MIC_PERIOD_US;
    for (int i = 0; i < MIC_SAMPLES; ++i) frame.data[i] = esp_g711a_encode(stereo[i * 2]);
    atomic_fetch_add(&s_captured, 1);
    if (xQueueSend(s_queue, &frame, 0) != pdTRUE) {
        /* Bound latency during congestion. Never block the capture clock. */
        mic_frame_t discarded;
        if (xQueueReceive(s_queue, &discarded, 0) == pdTRUE) atomic_fetch_add(&s_dropped, 1);
        if (xQueueSend(s_queue, &frame, 0) != pdTRUE) atomic_fetch_add(&s_dropped, 1);
    }
}

static void mic_capture_task(void *arg)
{
    mic_clock_t clock = {0};
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        while (atomic_load(&s_active)) mic_capture_frame(&clock);
    }
}

esp_err_t sip_audio_capture_init(void)
{
    if (s_task) return ESP_OK;
    s_queue = xQueueCreate(MIC_QUEUE_FRAMES, sizeof(mic_frame_t));
    if (!s_queue) return ESP_ERR_NO_MEM;
    if (xTaskCreate(mic_capture_task, "sip_mic", 4096, NULL, 6, &s_task) != pdPASS) {
        vQueueDelete(s_queue);
        s_queue = NULL;
        s_task = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void sip_audio_capture_start(void)
{
    if (!s_task) return;
    atomic_fetch_add(&s_generation, 1);
    atomic_store(&s_active, true);
    xTaskNotifyGive(s_task);
}

void sip_audio_capture_stop(void)
{
    atomic_store(&s_active, false);
    atomic_fetch_add(&s_generation, 1);
}

int sip_audio_capture_read(uint8_t *data, int capacity, int64_t *timestamp_us)
{
    if (!data || capacity <= 0 || !timestamp_us || !s_queue || !atomic_load(&s_active)) return 0;
    unsigned generation = atomic_load(&s_generation);
    if (s_current.generation != generation) s_offset = MIC_SAMPLES;
    /* Old-call chunks are discarded, never played in a subsequent call. */
    for (int attempt = 0; s_offset == MIC_SAMPLES && attempt < MIC_QUEUE_FRAMES; ++attempt) {
        if (xQueueReceive(s_queue, &s_current, 0) != pdTRUE) {
            atomic_fetch_add(&s_empty, 1);
            return 0;
        }
        if (s_current.generation == generation) s_offset = 0;
    }
    if (s_offset == MIC_SAMPLES || generation != atomic_load(&s_generation)) return 0;
    int n = MIC_SAMPLES - s_offset;
    if (n > capacity) n = capacity;
    *timestamp_us = s_current.timestamp_us + s_offset * 125; /* 8 kHz */
    memcpy(data, s_current.data + s_offset, n);
    s_offset += n;
    atomic_fetch_add(&s_samples_sent, n);
    return n;
}

void sip_audio_capture_diagnostics(void)
{
    static int64_t last_report;
    int64_t now = esp_timer_get_time();
    if (!atomic_load(&s_active)) { last_report = now; return; }
    if (now - last_report < 1000000) return;
    int64_t interval_ms = (now - last_report) / 1000;
    last_report = now;
    ESP_LOGI("SIP_AUDIO", "Mic: captured=%u sent_samples=%u empty=%u queue_drop=%u read_errors=%u queued=%u interval_ms=%"PRId64,
             atomic_exchange(&s_captured, 0), atomic_exchange(&s_samples_sent, 0),
             atomic_exchange(&s_empty, 0), atomic_exchange(&s_dropped, 0), atomic_exchange(&s_errors, 0),
             (unsigned)uxQueueMessagesWaiting(s_queue), interval_ms);
}
