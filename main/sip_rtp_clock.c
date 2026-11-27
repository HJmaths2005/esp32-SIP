#include "sip_rtp_clock.h"
#include "sip_video_tx.h"

#include <stdint.h>
#include <stddef.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

static portMUX_TYPE s_clock_lock = portMUX_INITIALIZER_UNLOCKED;
static const void *s_audio_buffer;
static const void *s_video_buffer;
static int64_t s_epoch_us;
static bool s_clock_started;
static bool s_audio_has_timestamp;
static int64_t s_audio_timestamp_us;

void sip_rtp_clock_begin(void)
{
    int64_t now = esp_timer_get_time();
    portENTER_CRITICAL(&s_clock_lock);
    s_epoch_us = now;
    s_audio_buffer = s_video_buffer = NULL;
    s_audio_has_timestamp = false;
    s_clock_started = true;
    portEXIT_CRITICAL(&s_clock_lock);
}

void sip_rtp_clock_reset(void)
{
    portENTER_CRITICAL(&s_clock_lock);
    s_clock_started = false;
    s_audio_buffer = s_video_buffer = NULL;
    s_audio_has_timestamp = false;
    portEXIT_CRITICAL(&s_clock_lock);
}

void sip_rtp_clock_set_buffer(bool video, const void *buffer)
{
    portENTER_CRITICAL(&s_clock_lock);
    if (video) s_video_buffer = buffer;
    else { s_audio_buffer = buffer; s_audio_has_timestamp = false; }
    portEXIT_CRITICAL(&s_clock_lock);
}

void sip_rtp_clock_set_audio_buffer(const void *buffer, int64_t capture_timestamp_us)
{
    portENTER_CRITICAL(&s_clock_lock);
    s_audio_buffer = buffer;
    s_audio_timestamp_us = capture_timestamp_us;
    s_audio_has_timestamp = true;
    portEXIT_CRITICAL(&s_clock_lock);
}

/* esp_media_protocols 0.6.1's esp_rtp_send ABI and microsecond timestamp units
 * were checked against its ESP32-P4 archive's DWARF/disassembly. Its RTC timer
 * adds one nominal frame interval per callback, so a delayed callback makes
 * the RTP clock run slow. In a live trace 39.2 s became 18.467 s at 90 kHz.
 * Use one elapsed-time origin for duplex audio and camera video. Match only
 * our SIP callback buffers; RTSP and unrelated RTP callers pass through.
 * Recheck this private ABI before upgrading the protocol component.
 */
int __real_esp_rtp_send(void *rtp, uint8_t *payload, uint32_t length,
                        uint64_t timestamp_us);

int __wrap_esp_rtp_send(void *rtp, uint8_t *payload, uint32_t length,
                        uint64_t timestamp_us)
{
    int64_t now = esp_timer_get_time();
    bool video = false;
    portENTER_CRITICAL(&s_clock_lock);
    if (s_clock_started && payload && length && now >= s_epoch_us &&
        (payload == s_audio_buffer || payload == s_video_buffer)) {
        /* Queued microphone samples retain capture time when sent in a burst. */
        if (payload == s_audio_buffer && s_audio_has_timestamp) now = s_audio_timestamp_us;
        if (now < s_epoch_us) now = s_epoch_us;
        timestamp_us = (uint64_t)(now - s_epoch_us);
        video = payload == s_video_buffer;
    }
    portEXIT_CRITICAL(&s_clock_lock);
    if (video) return sip_video_tx_enqueue(rtp, payload, length, timestamp_us);
    return __real_esp_rtp_send(rtp, payload, length, timestamp_us);
}
