#include "sip_video_tx.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <string.h>
#include "esp_cache.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define TX_SLOTS 2
#define CACHE_ALIGNED(n) (((n) + 63) & ~(size_t)63)

typedef struct {
    uint8_t *data;
    void *rtp;
    uint32_t length;
    uint64_t timestamp_us;
} video_packet_t;

static video_packet_t s_slots[TX_SLOTS];
static size_t s_capacity;
static QueueHandle_t s_free, s_ready;
static SemaphoreHandle_t s_done;
static TaskHandle_t s_task;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static bool s_enabled;
static void *s_video_rtp;
static unsigned s_pending;
static atomic_uint s_queued, s_sent, s_dropped, s_errors;

/* ABI verified from the pinned 0.6.1 archive (same as sip_rtp_clock.c). */
int __real_esp_rtp_send(void *rtp, uint8_t *data, uint32_t length, uint64_t timestamp_us);
int __real_esp_rtp_stop(void *rtp);
int __real_esp_rtp_recv(void *rtp, uint8_t *data, uint32_t capacity, uint32_t timeout_ms);

static void release_slot(unsigned index)
{
    portENTER_CRITICAL(&s_lock);
    --s_pending;
    portEXIT_CRITICAL(&s_lock);
    xQueueSend(s_free, &index, 0);
    xSemaphoreGive(s_done);
}

static void send_slot(unsigned index)
{
    video_packet_t *packet = &s_slots[index];
    esp_err_t cache_ret = esp_cache_msync(packet->data, CACHE_ALIGNED(packet->length), ESP_CACHE_MSYNC_FLAG_DIR_M2C);
    int ret = cache_ret == ESP_OK
        ? __real_esp_rtp_send(packet->rtp, packet->data, packet->length, packet->timestamp_us)
        : -1;
    /* esp_rtp_send returns the last UDP write's byte count on success. */
    if (ret >= 0) atomic_fetch_add(&s_sent, 1);
    else atomic_fetch_add(&s_errors, 1);
    release_slot(index);
}

static void video_tx_task(void *arg)
{
    unsigned index;
    for (;;) {
        if (xQueueReceive(s_ready, &index, portMAX_DELAY) == pdTRUE) send_slot(index);
    }
}

esp_err_t sip_video_tx_init(size_t capacity)
{
    if (!capacity) return ESP_ERR_INVALID_ARG;
    if (s_task) return capacity <= s_capacity ? ESP_OK : ESP_ERR_INVALID_SIZE;
    s_capacity = capacity;
    s_free = xQueueCreate(TX_SLOTS, sizeof(unsigned));
    s_ready = xQueueCreate(TX_SLOTS, sizeof(unsigned));
    s_done = xSemaphoreCreateBinary();
    if (!s_free || !s_ready || !s_done) goto fail;
    for (unsigned i = 0; i < TX_SLOTS; ++i) {
        s_slots[i].data = heap_caps_aligned_alloc(64, CACHE_ALIGNED(capacity), MALLOC_CAP_SPIRAM);
        if (!s_slots[i].data) goto fail;
        xQueueSend(s_free, &i, 0);
    }
    /* The SDK receiver is priority 15 and shares the RTP handle's mutex.
     * Let a waiting sender run as soon as that receiver releases it. */
    if (xTaskCreate(video_tx_task, "sip_video_tx", 4096, NULL, 16, &s_task) != pdPASS) goto fail;
    return ESP_OK;
fail:
    for (unsigned i = 0; i < TX_SLOTS; ++i) {
        heap_caps_free(s_slots[i].data);
        s_slots[i].data = NULL;
    }
    if (s_free) vQueueDelete(s_free);
    if (s_ready) vQueueDelete(s_ready);
    if (s_done) vSemaphoreDelete(s_done);
    s_free = s_ready = NULL;
    s_done = NULL;
    s_task = NULL;
    return ESP_ERR_NO_MEM;
}

void sip_video_tx_begin(void)
{
    portENTER_CRITICAL(&s_lock);
    /* esp_rtp_stop drains the old call before its handle is freed. */
    if (!s_pending) {
        s_video_rtp = NULL;
        s_enabled = true;
    }
    portEXIT_CRITICAL(&s_lock);
}

int sip_video_tx_enqueue(void *rtp, const uint8_t *data, uint32_t length, uint64_t timestamp_us)
{
    if (!rtp || !data || !length || length > s_capacity || !s_task) return ESP_ERR_INVALID_ARG;
    unsigned index;
    if (xQueueReceive(s_free, &index, 0) != pdTRUE) {
        atomic_fetch_add(&s_dropped, 1);
        return ESP_OK; /* Whole intra frame dropped; never delay the audio timer. */
    }
    portENTER_CRITICAL(&s_lock);
    bool accepted = s_enabled && (!s_video_rtp || s_video_rtp == rtp);
    if (accepted) { s_video_rtp = rtp; ++s_pending; }
    portEXIT_CRITICAL(&s_lock);
    if (!accepted) {
        xQueueSend(s_free, &index, 0);
        return ESP_ERR_INVALID_STATE;
    }
    video_packet_t *packet = &s_slots[index];
    packet->rtp = rtp;
    packet->length = length;
    packet->timestamp_us = timestamp_us;
    memcpy(packet->data, data, length);
    esp_err_t ret = esp_cache_msync(packet->data, CACHE_ALIGNED(length), ESP_CACHE_MSYNC_FLAG_DIR_C2M);
    if (ret != ESP_OK || xQueueSend(s_ready, &index, 0) != pdTRUE) {
        atomic_fetch_add(&s_errors, 1);
        release_slot(index);
        return ret != ESP_OK ? ret : ESP_FAIL;
    }
    atomic_fetch_add(&s_queued, 1);
    return ESP_OK;
}

/* Stop accepting this handle and join all copied/in-flight sends before the
 * SDK destroys its RTP mutex, socket and handle. Other RTP handles pass through.
 * No task ever dereferences the SDK's reusable source buffer after enqueue.
 */
int __wrap_esp_rtp_stop(void *rtp)
{
    portENTER_CRITICAL(&s_lock);
    bool video = rtp && rtp == s_video_rtp;
    if (video) s_enabled = false;
    portEXIT_CRITICAL(&s_lock);
    if (video) {
        for (;;) {
            portENTER_CRITICAL(&s_lock);
            bool pending = s_pending != 0;
            portEXIT_CRITICAL(&s_lock);
            if (!pending) break;
            xSemaphoreTake(s_done, portMAX_DELAY);
        }
    }
    return __real_esp_rtp_stop(rtp);
}

/* The SDK video receiver holds the same mutex as esp_rtp_send while waiting
 * up to 200 ms for a packet. This application discards remote video; a short
 * receive poll keeps that unused path from starving the local camera sender.
 * Keep audio and unrelated RTP receivers unchanged. ABI verified in 0.6.1.
 */
int __wrap_esp_rtp_recv(void *rtp, uint8_t *data, uint32_t capacity, uint32_t timeout_ms)
{
    portENTER_CRITICAL(&s_lock);
    bool video = s_enabled && rtp && rtp == s_video_rtp;
    portEXIT_CRITICAL(&s_lock);
    if (video && timeout_ms > 10) timeout_ms = 10;
    return __real_esp_rtp_recv(rtp, data, capacity, timeout_ms);
}

void sip_video_tx_diagnostics(void)
{
    static int64_t last_report;
    int64_t now = esp_timer_get_time();
    if (!s_task || now - last_report < 1000000) return;
    last_report = now;
    ESP_LOGI("SIP_VIDEO_TX", "queued=%u sent=%u busy_drop=%u errors=%u",
             atomic_exchange(&s_queued, 0), atomic_exchange(&s_sent, 0),
             atomic_exchange(&s_dropped, 0), atomic_exchange(&s_errors, 0));
}
