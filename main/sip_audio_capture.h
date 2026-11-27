#pragma once

#include <stdint.h>
#include "esp_err.h"

esp_err_t sip_audio_capture_init(void);
void sip_audio_capture_start(void);
void sip_audio_capture_stop(void);
/* Nonblocking, bounded read of queued PCMA samples; timestamp is in us. */
int sip_audio_capture_read(uint8_t *data, int capacity, int64_t *timestamp_us);
void sip_audio_capture_diagnostics(void);
