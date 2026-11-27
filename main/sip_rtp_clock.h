#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Call before media callbacks start for a new SIP call. */
void sip_rtp_clock_begin(void);
void sip_rtp_clock_reset(void);
/* Identify only the SDK buffers supplied to our own SIP callbacks. */
void sip_rtp_clock_set_buffer(bool video, const void *buffer);
void sip_rtp_clock_set_audio_buffer(const void *buffer, int64_t capture_timestamp_us);
