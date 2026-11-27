# A project-based route to deep understanding

Companion to [the technical guide](PROJECT_TECHNICAL_GUIDE.md).
Project root: `/home/haami/codes/C_codes/idf_projects/test_22_07`.

## How to use this course

Work through the stages in order. Each stage has a prerequisite, concrete
material, a lab and an exit criterion. Reading names of APIs is insufficient:
you should be able to predict bytes, rates, ordering and failure behavior.

Keep one notebook with five columns: hypothesis, expected measurement,
experiment, observed measurement, conclusion. Maintain a known-working firmware
and a separate place for experiments. The current working tree deliberately
has microphone TX disabled; distinguish that from the saved accepted duplex
firmware when studying or running tests.

For every chapter, use this sequence:

1. Explain the physical or software problem.
2. Derive its mathematical model and units.
3. Draw the data representation and lifetime.
4. Implement the smallest useful version on the host.
5. Map it to the board's actual source.
6. Inject a failure and collect evidence.
7. Explain what your test cannot prove.

If comfortable with C and basic electronics, reserve several months of regular
study for the complete sequence. If pointers, algebra or binary arithmetic are
new, extend the foundation stages. Progress by the exit criteria, not an
arbitrary weekly deadline. A complete treatment of H.264, DSP, operating systems
and networking is much larger than one guide; this path makes that study
ordered and experimentally connected to your project.

## Stage 0 — Units and mathematical prerequisites

**Prerequisite:** arithmetic.

Learn binary/hexadecimal, powers of two, bit masks, signed two's-complement,
dimensional analysis, integer overflow, ratios and modular arithmetic. Then
study logarithms and decibels, sine waves, basic probability and descriptive
statistics. Add vectors/matrices, complex numbers, convolution and Fourier
analysis before the deeper image/DSP/AEC chapters. Basic algebra is enough for
the first bandwidth and queue calculations; advanced calculus need not block
the initial labs.

**Lab:** derive all sizes/rates in technical-guide sections 8 and 9 by hand.
Write units beside every intermediate value. Compute mean, maximum and 95th
percentile for a synthetic list of packet delays.

**Exit:** you can distinguish samples/s, bytes/frame, bits/s, ticks/s and fps
without substituting one for another.

## Stage 1 — C memory and interfaces

**Prerequisite:** stage 0 and elementary C syntax.

Study pointers versus arrays, object lifetime, stack versus static storage,
heap allocation, ownership, const, signedness, integer promotions, alignment,
struct padding, function pointers, error conventions and undefined behavior.
Then study atomics and why `volatile` is not a synchronization mechanism.
For the formatting wrappers, learn variadic functions, `va_list`, `va_copy`,
`snprintf` length/truncation behavior and allocator pairing.

**Lab:** enqueue a pointer to a producer's reusable array, overwrite it and
observe corrupted consumption. Replace it with an owned copy. Use host address
and undefined-behavior sanitizers to investigate an intentionally unsafe toy
version, not the live board firmware.

**Exit:** draw the lifetime of every buffer crossing `_send_audio`,
`sip_send_video` and `_receive_audio`; identify which caller owns each one.

## Stage 2 — Compilation, linking and reproducibility

**Prerequisite:** stage 1.

Study preprocessing, translation units, object files, static archives,
relocations, ELF symbols/sections, link maps and linker wrapping. Learn the
difference between a header API and a binary ABI. Study CMake components,
Kconfig defaults versus selected sdkconfig, dependency version ranges versus
resolved locks, cross-compilation and architecture-specific libraries.

**Lab:** build a host toy library containing a sender function; wrap it with
`--wrap`, count calls, and pass unrelated cases to `__real_...`. Inspect symbols
with `nm`/`readelf`. Compare a Linux x86-64 codec library with a RISC-V firmware
object: explain why they cannot be interchanged.

**Exit:** explain why matching symbol names and a successful link do not prove
matching parameter layout, timestamp units or return semantics.

## Stage 3 — MCU execution and hardware buses

**Prerequisite:** stages 1–2.

Study CPU registers, stack frames, interrupt entry, memory-mapped peripherals,
clock trees, internal SRAM, external PSRAM and DMA. Identify what GPIO, I2C,
I2S, MIPI CSI, MIPI DSI and SDIO carry in this project. Separate control
register traffic from high-rate media transfer. Trace `app_main` initialization
and the board's hosted WiFi coprocessor connection.

**Lab:** annotate every initialization call with what it allocates and what
must already exist. Make a peripheral/pin/clock diagram from the source.

**Exit:** explain why a working I2C codec response does not prove correct I2S
audio, and why available PSRAM does not guarantee adequate DMA-capable memory.

## Stage 4 — FreeRTOS and real-time scheduling

**Prerequisite:** stages 1 and 3.

Study ready/running/blocked states, preemption, task priorities, core affinity,
ISRs, timer-task callbacks, queues, notifications, mutexes, semaphores, critical
sections, deadlocks, starvation and priority inheritance. Distinguish CPU time
from time a shared executor is occupied by a blocking operation.

**Lab:** construct two timer callbacks: one records deadlines, the other
blocks. Measure the first callback's delay. Move the blocking operation into
a worker and repeat. Reproduce a higher-priority receiver reacquiring a shared
mutex while a sender waits; then bound the lock hold and change scheduling.

**Exit:** draw the exact wait chain behind the project's microphone gaps and
later video starvation. Explain why increasing queue size alone cannot solve
either permanent under-service condition.

Read the [IDF FreeRTOS guide](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32p4/api-reference/system/freertos_idf.html)
and [ESP timer guide](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32p4/api-reference/system/esp_timer.html).

## Stage 5 — DMA buffers and cache visibility

**Prerequisite:** stages 3–4.

Study cache lines, dirty/stale data, writeback, invalidation, alignment,
capability-based allocation, DMA completion and ownership handoffs. Work through
the camera pool state machine before studying cache calls. Mutual exclusion
and cache visibility address different failure classes.

**Lab:** model the three-buffer state machine on the host. Test a driver
requesting the next capture before completing the old one. Prove that a
PROCESSING buffer is never reclaimed by capture. On hardware, instrument
completion boundaries without intentionally creating uncontrolled corruption.

**Exit:** explain every C2M/M2C call in `camera_manip.c` and `sip_video_tx.c`,
including allocation coverage after rounding lengths to 64-byte boundaries.
Use the [memory synchronization guide](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32p4/api-reference/system/mm_sync.html).

## Stage 6 — Digital audio, PCM and G.711

**Prerequisite:** stages 0, 1 and 3.

Study sampling/aliasing and anti-alias filtering, quantization, signed linear
PCM, clipping, RMS/peak amplitude, dBFS versus analog gain, channels and
interleaving. Then study A-law and μ-law companding, their eight-bit codes and
their RTP identities. Learn why nonlinear companding and resampling are
different operations.

**Lab:** generate 440 Hz and 1 kHz PCM sine waves at 8 kHz, save a WAV file,
and inspect time samples and a spectrum. Implement A-law encode/decode against
known test vectors; compare with an independent implementation. A-law `D5`
decodes to +8 and `55` to −8 in the usual linear representation. All-zero
encoded bytes are not digital silence.

**Exit:** calculate a 20 ms chunk's PCM and PCMA sizes, and explain distortion
from wrong law, wrong sample rate, clipping and wrong channel packing.
Read [G.711](https://www.itu.int/rec/T-REC-G.711).

## Stage 7 — I2S codec playback and capture

**Prerequisite:** stages 3, 5 and 6.

Study codec ADC/DAC paths, BCLK, LRCLK/WS, MCLK, I2S bit alignment, slot width,
sample width, stereo masks and amplifier control. Read `audio_manip.c` and the
actual ES8311 driver, including its sample-rate coefficients and volume curve.
Learn the driver's return values before writing success checks.

**Lab:** play a locally generated low-amplitude tone with the network path
absent. Capture a known sound separately. Measure clocks with suitable
instruments if available. Compare the expected 8 kHz WS, 256 kHz BCLK and
2.048 MHz MCLK with the selected two-slot/16-bit/256× configuration.

**Exit:** isolate codec playback, mic capture and network media as three
independently testable paths. Read [I2S documentation](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32p4/api-reference/peripherals/i2s.html).

## Stage 8 — IP networking and sockets

**Prerequisite:** stages 0–2.

Study MAC versus IP addresses, ARP, subnet masks, routing, loopback, UDP ports,
socket binding, blocking/nonblocking calls, select/poll, byte order, MTU and IP
fragmentation. Then study TCP retransmission and head-of-line blocking to
understand RTSP interleaving tradeoffs. Add NAT and symmetric RTP after the
same-LAN system is understood.

**Lab:** write UDP sender/receiver programs on the host. Send to the computer's
own LAN address and capture it. Add delay and packet loss in a controlled toy
environment. Compute payload versus IP-layer bitrate.

**Exit:** explain why our first packet-capture filter missed a media leg and
why a SIP port number does not identify the negotiated RTP ports.

## Stage 9 — SIP, SDP and Asterisk

**Prerequisite:** stage 8.

Study user agents, registrars, proxies and B2BUAs; REGISTER, digest challenge,
INVITE, provisional responses, 200/ACK, CANCEL, BYE, transactions and dialogs.
For SDP learn connection/media lines, dynamic payload mappings, fmtp, clock
rates, directions and offer/answer rules. Study Asterisk as two call/media
legs in the tested topology, not as a transparent wire by assumption.

**Lab:** annotate one complete successful call transcript. Identify which
message creates the dialog, which messages negotiate media and which event
actually starts each media path. Compare both sides of Asterisk's SDP.

**Exit:** diagnose “registered but cannot call,” “answered but no RTP,” and
“RTP present but incompatible payload” as separate states. Read
[SIP](https://www.rfc-editor.org/rfc/rfc3261),
[SDP](https://www.rfc-editor.org/rfc/rfc8866) and
[offer/answer](https://www.rfc-editor.org/rfc/rfc3264).

## Stage 10 — RTP, RTCP and media clocks

**Prerequisite:** stages 6, 8 and 9.

Study packet sequence, SSRC, media timestamps, marker semantics, payload
mapping, jitter, loss/late-discard distinctions and wraparound. Then study
RTCP sender/receiver reports, RTP-to-NTP mappings and synchronization. Separate
source sample time, monotonic host time, wire send time and playout time.

**Lab:** parse RTP headers into explicit integer fields, handling CSRCs,
extensions and padding. Plot packet arrival versus RTP media time. Reproduce
the project's slow nominal callback clock in a simulation, then preserve
sample-indexed time when delayed audio is sent in bursts.

**Exit:** derive expected 160-tick audio and nominal 6,000-tick video spacing,
and explain how zero reported packet loss can coexist with missing speech.
Read [RTP/RTCP](https://www.rfc-editor.org/rfc/rfc3550) and
[the audio/video profile](https://www.rfc-editor.org/rfc/rfc3551).

## Stage 11 — Packet captures as experiments

**Prerequisite:** stages 8–10.

Study capture interfaces, BPF capture filters versus display filters, link-layer
headers, snap length, capture drops, duplicate observations, stream separation
and the limitations of timestamps collected at one host. Learn to distinguish
actual packet loss from an incomplete capture.

**Lab:** reconstruct a PCMA stream and an H.264 stream from a saved capture.
Validate packet bounds before reading fields. Compare encoded media across
the relay while allowing payload identifiers, SSRCs and sequence spaces to
differ. Keep original capture files immutable during analysis.

**Exit:** support a claim about exactly where media stopped with evidence from
the corresponding boundary. Use [Wireshark telephony tools](https://www.wireshark.org/docs/wsug_html_chunked/ChTelephony.html).

## Stage 12 — Digital images, sensor and ISP

**Prerequisite:** stages 0, 3 and 5; basic matrices for color transforms.

Study exposure, analog/digital gain, Bayer sampling, demosaicing, white balance,
color correction, gamma, luma/chroma and 4:2:0 subsampling. Learn stride,
cropping and hardware-specific packed formats. Distinguish sensor dimensions,
ISP output dimensions, encoder input dimensions and display scaling.

**Lab:** create an image with known colored blocks, convert between RGB and
YUV formats on the host, and deliberately swap chroma planes. Compute VGA and
720p buffer/memory-bandwidth budgets. Trace the actual hardware YUV layout
used by the encoder rather than substituting a similarly sized format.

**Exit:** explain color corruption, softness, compression block artifacts and
low frame rate using different candidate mechanisms.

## Stage 13 — H.264 coding and bitstream syntax

**Prerequisite:** stages 0, 1 and 12; transforms/linear algebra for coding depth.

Study spatial/inter prediction, residuals, transforms, quantization, entropy
coding, reference-picture buffers, I/P/B slice types, IDR behavior, GOP policy,
profiles and levels. Then study NAL/RBSP/Annex B, SPS/PPS, Exp-Golomb codes,
emulation prevention, slice headers, picture identity and access-unit detection.

**Lab:** write a NAL scanner that accepts both three- and four-byte start
codes. Print types and lengths without treating each NAL as a picture. Parse
selected SPS/PPS fields, and check dimensions with an independent decoder.
Trace the repeated picture identifiers in the captured GOP-1 stream.

**Exit:** explain why arbitrary drops are easier with independent pictures,
why a P picture can fail after a reference drop, and why an AUD can change
decoder output without changing reconstructed picture pixels.
Read the [H.264 specification](https://www.itu.int/rec/T-REC-H.264).

## Stage 14 — H.264 over RTP and decoder APIs

**Prerequisite:** stages 10, 11 and 13.

Study single-NAL, STAP-A and FU-A payloads, packetization-mode negotiation,
fragment reconstruction, access-unit timestamps and marker placement. Study
decoder input/output contracts, buffered versus immediate output, parameter
set delivery and flushing. Separate decoding return status from picture output.

**Lab:** depacketize FU-A, reject a sequence gap, reconstruct Annex B, and
decode the result offline. Run the same stream through the exact OpenH264
API paths in `tools/check_openh264_compat.cpp`, with and without AUD insertion.
That diagnostic assumes SPS-prefixed pictures from this intra-only capture;
it is not a general arbitrary-stream access-unit parser.

**Exit:** reproduce the 211/0 versus 211/210 result and explain every byte of
the six-byte delimiter. Read [H.264 RTP payloads](https://www.rfc-editor.org/rfc/rfc6184).

## Stage 15 — Streaming pipeline design

**Prerequisite:** stages 4–5, 10 and 14.

Study producer/consumer rate matching, bounded queues, backpressure, freshness,
drop policies, sample-indexed timing, partial consumption and generation IDs.
Add ownership changes caused by asynchronous work and safe shutdown protocols.
Read the three `sip_*` timing/capture/transmit modules together.

**Lab:** inject 100 ms consumer delays, then permanent 35-chunk/s consumption
against a 50-chunk/s producer. Plot queue occupancy, drops and sample age.
Reuse the SDK input immediately after enqueueing video. End a call while a
send is in progress and prove the transport is destroyed only afterward.

**Exit:** derive queue conservation, explain each drop policy and preserve
timestamps and object lifetimes under delayed execution and hangup.

## Stage 16 — Binary diagnosis and interoperability

**Prerequisite:** stages 2, 4 and 14–15.

Study ELF dynamic dependencies/SONAMEs, architecture, loader search paths,
static archive extraction, DWARF, RISC-V calling conventions and disassembly.
Use this material to understand both the Linux codec mismatch and the firmware's
private SDK wrappers. Treat a function's units and return values as part of
its contract, alongside its C type signature.

**Lab:** inspect the plugin dependency, identify ABI 6 versus ABI 7, and make
the actual media plugin construct a decoder. On the firmware archive, identify
the shared RTP mutex and receiver timeout. Check wrapper references in the
linked map. Explain the positive-byte-count diagnostic mistake.

**Exit:** substantiate an ABI claim with types, binary behavior and a functional
test, and list what must be revalidated after an SDK upgrade.

## Stage 17 — UI, asynchronous application state and recovery

**Prerequisite:** stages 1, 4 and 9.

Study LVGL thread ownership, logical versus physical coordinates, RGB stride,
rotation, partial rendering and asynchronous DMA completion. Study WiFi/SIP
state machines and desired-versus-observed state, reconnects and cancellation.
Then study partition tables, application offsets, bootloader/NVS boundaries,
versioned source/configuration and artifact hashes.

**Lab:** trace a late disconnect event following replacement credentials.
Rotate a small numbered pixel grid and verify touch/display correspondence.
Draw buffer ownership until flush completion. Compare the saved verified
firmware/configuration with the intentionally modified current source.

**Exit:** explain why “request succeeded” differs from “operation completed,”
and reproduce a known image without assuming every source/config is identical.

## Stage 18 — Engineering validation and the next advanced topics

**Prerequisite:** all preceding stages.

Study deterministic fault injection, host boundary mocks, hardware acceptance,
latency distributions, clock drift, resource-leak measurements and endpoint
substitution. Extend to AEC with adaptive filters, RTP/RTCP synchronization,
network adaptation, secure signaling/media and NAT traversal as separate
advanced chapters. None is implied by a successful short LAN call.

**Lab:** prepare an acceptance matrix spanning endpoints, media direction,
audio-only/video calls, mute states, hangup/retry, WiFi reconnect and long-call
duration. Record measured frame rates and audio duration, not only subjective
quality. Repeat the smartphone/laptop substitution without changing firmware.

**Exit:** reproduce each original failure on a controlled baseline, demonstrate
the responsible fix and explain the boundaries of your evidence.

## Worked checkpoint problems

1. **160 PCMA bytes every 20 ms:** 8,000 samples/s and 64 kbit/s media payload;
   80 kbit/s with minimal IPv4/UDP/RTP headers, excluding RTCP/link overhead.
2. **12 audio chunks, producer 50/s, consumer 35/s:** approximately 0.8 s to
   fill from empty, and up to 240 ms whole-chunk queued duration at capacity.
3. **211 pictures over 39.2 s:** approximately 5.38 delivered pictures/s.
4. **18.467 s RTP progression over 39.2 s wall time:** ratio approximately
   0.471. This direct calculation is distinct from Linphone's observed adaptive
   estimate near 0.52 over its own window.
5. **A 7,500-byte NAL with a 1,200-byte RTP payload limit:** a FU-A body can
   carry at most 1,198 bytes after its two-byte FU headers. Fragment the 7,499
   bytes after the original NAL header into seven packets. Sequence numbers
   advance per packet; the picture timestamp stays common to its fragments.
6. **A video slot queued by pointer, then overwritten:** correct queue ordering
   cannot recover the old picture. Transfer exclusive ownership or copy before
   returning to the reusable producer.
7. **No decoder error and no picture output:** do not infer success or network
   loss. Check decoder output status, buffering, boundaries and the exact API.
8. **Smartphone speaker playback succeeds while laptop screams:** the endpoint
   comparison narrows the issue; it does not identify a particular laptop
   setting or prove acoustic feedback without additional tests.

## A concrete first ten sessions

1. Record the accepted configuration and draw the media/control paths.
2. Work every unit calculation in guide section 8.
3. Trace buffer lifetime in one audio callback and one video callback.
4. Reproduce pointer reuse with a tiny host queue experiment.
5. Build and inspect a wrapped toy library.
6. Annotate the initialization/deinitialization dependency graph.
7. Simulate the three-buffer camera ownership schedules.
8. Generate and inspect a PCM tone; derive its expected I2S clocks.
9. Send/capture a UDP packet locally and parse its headers.
10. Explain the first two historical failures using measurements and exact
    contracts, without using “SIP problem” or “codec problem” as the full answer.

Continue through the staged labs after those foundations. Your final project
exercise is a clean reconstruction of the accepted design from your own
explanations, followed by the acceptance matrix, rather than copying functions
whose lifetimes, units and scheduling assumptions you cannot yet justify.
