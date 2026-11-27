#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

esp_err_t sip_video_tx_init(size_t capacity);
void sip_video_tx_begin(void);
int sip_video_tx_enqueue(void *rtp, const uint8_t *data, uint32_t length, uint64_t timestamp_us);
void sip_video_tx_diagnostics(void);
