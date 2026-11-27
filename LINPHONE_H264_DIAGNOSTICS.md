# Linphone H.264 investigation — 2026-10-16

## Linphone 5.2.6 codec repair

The Linux 5.2.6 AppImage downloads OpenH264 2.4.1 (SONAME
`libopenh264.so.7`), but its `libmsopenh264.so` plugin requires
`libopenh264.so.6`. This is a confirmed upstream bug:
https://github.com/BelledonneCommunications/linphone-desktop/issues/729

Installed Cisco's OpenH264 **2.2.0**, downloaded directly from:
https://ciscobinary.openh264.org/libopenh264-2.2.0-linux64.6.so.bz2

The corrected file is `~/.local/share/linphone/codecs/libopenh264.so`.
The previous 2.4.1 library is preserved alongside it as
`libopenh264.so.backup-20261016-003743-668428`.

`H264.txt` deliberately retains 5.2.6's expected download URL. The application
uses this marker to recognize the installed codec; changing it to the corrected
URL can make 5.2.6 download the incompatible binary again.

The repair can be repeated with:

```sh
python3 tools/repair_linphone_526_h264.py /path/to/libopenh264-2.2.0-linux64.6.so.bz2
```

Validation:

- The installed library reports version 2.2.0 through `WelsGetCodecVersion`.
- The actual 5.2.6 media plugin creates both an H.264 encoder and decoder with
  that library in a headless factory check.
- After restarting 5.2.6, its saved configuration contains H264 enabled at
  90000 Hz. A live call negotiates H.264 successfully.

## Blank video investigation

Test network: board `10.192.65.235`, computer/Asterisk `10.192.65.165`.
The board is registered as extension 103 and calls extension 101.

The serial trace confirms:

- The board offers H264/90000 with `packetization-mode=1` and `sendonly`.
- Asterisk accepts H264/90000 with `packetization-mode=1` and a nonzero port.
- The board enters its video session and supplies encoded frames containing
  SPS, PPS and IDR NAL units.

The second packet capture, `/tmp/board-sip-video-both.pcap`, confirms:

- 1,932 board video RTP packets arrive at Asterisk, containing 211 complete
  intra frames over 39.2 seconds (about 5.38 fps).
- Asterisk forwards the same video payload to Linphone, translating RTP
  payload type 96 to 97.
- Reconstructed Annex B streams before and after Asterisk are byte identical:
  SHA-256 `cc0eac46440aa62b62367065672db3645098a8599975a0b48e8bfe3f8870b897`.
- FFmpeg decodes all 211 frames from the Linphone-bound stream without errors:
  Constrained Baseline H.264, 640x480. A decoded frame contains camera imagery.
- No IP fragmentation or broken FU-A assembly was observed in this sample.

These results establish camera encoding, delivery to the computer and Asterisk
forwarding for the captured call. They do not by themselves establish decoder
compatibility or correct RTP timing.

## Firmware corrections

### Access unit boundaries

Using the actual installed Cisco 2.2.0 library, `tools/check_openh264_compat.cpp`
reproduced the failure offline from the captured board stream:

| Decoder input/path | Input pictures | Output pictures | Errors |
| --- | ---: | ---: | ---: |
| Original stream, `DecodeFrame2` (Linphone's path) | 211 | 0 | 0 |
| Original stream, `DecodeFrameNoDelay` | 211 | 211 | 0 |
| AUD before each picture, `DecodeFrame2` | 211 | 210 | 0 |

The last row has the normal one-picture buffering delay of `DecodeFrame2`.
With GOP 1, the hardware repeats its picture identifiers. Explicit access unit
boundaries allow this decoder to finish pictures instead of silently buffering.
`sip_send_video` now prepends `00 00 00 01 09 f0` (an H.264 AUD NAL) to each SIP
camera frame. It reserves those six bytes before copying, so the SDK destination
capacity remains bounded. The camera/RTSP source buffer and GOP remain unchanged.

### RTP timing

The original board RTP timestamps advance 18.467 seconds during 39.2 seconds
of wall time. Linphone's 90 kHz jitter estimator reports a clock ratio around
0.52 and repeatedly resets. The prebuilt RTC timer adds a nominal interval per
callback even when callbacks are delayed.

`main/sip_rtp_clock.c` wraps `esp_rtp_send`, using elapsed microseconds from a
shared audio/video call origin for buffers identified by this application's SIP
callbacks. Unrelated RTP callers pass through. The private function ABI was
verified from the pinned protocol component 0.6.1's DWARF and disassembly; review
it before upgrading that dependency. The timing-only hardware test improved the
clock estimate, but did not resolve blank video, which led to the decoder test
above.

### Build and recovery

The ESP-IDF build and host regression tests pass:

```sh
python3 tests/run_host_tests.py --cc gcc
python3 tests/run_sip_rtp_clock_test.py
```

The original 4 MiB factory application partition was read from the device into
`backups/sip-before-wall-clock/device-factory.bin`; its application bytes match
`backups/sip-before-wall-clock/internship.bin`. Only the application at offset
`0x10000` is flashed; the bootloader, partition table and NVS are preserved.
The original sdkconfig is also saved in that folder and is unchanged.

Live verification: the user confirmed that Linphone now displays the board's
video after the AUD fix. In that call, its video clock ratio settles near 1.0
(0.987–1.007 in the sampled log), instead of the original approximately 0.52.

## Follow-up: choppy board microphone audio

The user reported choppy board → Linphone audio once video worked. Linphone's
call statistics show 1,134 PCMA packets over approximately 35.5 seconds, with
zero lost, malformed, late or queue-overflow RTP packets. At 160 samples per
packet this carries only 22.68 seconds of speech. The source is under-supplying
audio; these statistics do not show network packet loss.

The RTC library runs its audio and video send callbacks on the shared ESP timer
task. The old microphone callback performs a blocking 20 ms I2S read there,
leaving little time for video transmission or timely microphone reads.

`main/sip_audio_capture.c` now captures and encodes PCMA in a dedicated task.
The SDK callback reads a bounded 12-frame queue without waiting for I2S. Samples
retain their capture timestamps and 20 ms sample spacing when sent after a
delay. Queue overflow drops the oldest chunk to bound latency; call generations
prevent previous-call audio from reaching a new call. `SIP_AUDIO` diagnostics
report captured frames, supplied samples, empty reads, queue drops and I2S errors.

The microphone queue regression test covers delayed consumers, capture timing,
overflow, partial reads, call changes, I2S errors and a call ending during a
hardware read:

```sh
python3 tests/run_sip_audio_capture_test.py
```

The firmware build and all host tests pass. The video-working image before this
audio change is preserved in `backups/sip-video-working/internship.bin`.
The user confirmed that this microphone queue improves the speech, but some
gaps remain. In that live test the board captures about 50 chunks per second
without I2S errors, while chunks accumulate and overflow during synchronous
video sends on the timer task. For example, one 2,052 ms interval captures 103
chunks but supplies only 7,200 samples and drops 67 queued chunks.

`main/sip_video_tx.c` moves those video RTP sends to a separate task. The timer
callback copies complete encoded frames into two bounded PSRAM slots and
returns immediately. If both slots are occupied, it drops a whole intra frame
instead of holding up audio. A wrapper around `esp_rtp_stop` drains queued and
in-flight video sends before the SDK frees the transport. Both private RTP
function signatures were checked against the pinned component's debug data.

The additional host regression test checks buffer ownership, bounded drops,
PSRAM cache synchronization, error recovery and draining before RTP teardown:

```sh
python3 tests/run_sip_video_tx_test.py
```

All host tests and the ESP-IDF build pass. The separate video sender image
(SHA-256 `359c5152ac0a4cebab3f84df95ec66ef2181cc660c2fa33fc2d0c7371bcb3c9c`)
was flashed at `0x10000` and verified. The previous microphone-queue image is
preserved in `backups/sip-mic-queue/internship.bin`; sdkconfig is unchanged.
The user confirmed good board → Linphone audio with this image. During the
test, the board supplies about 8,000 samples/s with zero microphone queue drops
and zero I2S errors. Linphone reports 3,165 received audio packets and zero lost,
late, malformed or queue-overflow packets. Reverse audio has not been confirmed.

## Follow-up: video detail and freezing

The user reports both poor picture detail and slow/freezing video after audio
is fixed. Linphone decodes only about 2–6 fps through much of the recorded call.
The SDK's video receive task has priority 15 and holds the same RTP mutex as the
sender while polling for incoming packets with a 200 ms timeout. The new video
worker originally had priority 5, allowing that receiver to starve it.

The next image raises the video worker to priority 16 and wraps `esp_rtp_recv`
to cap that timeout at 10 ms for the identified SIP video handle only. This
application already discards incoming video, so shortening those polls does
not affect a displayed remote picture. Audio and unrelated RTP receive calls
retain their original timeout. The receive function's pointer and uint32_t
arguments were verified in the pinned 0.6.1 archive's DWARF and disassembly.

The original video worker's `errors` counter incorrectly classified successful
positive UDP byte counts as errors; it now accepts nonnegative send results and
still treats cache synchronization failures separately. The earlier `sent=0`
logs therefore do not mean that no video was sent. The host test now exercises
positive send returns, positive ESP cache error codes, video-only receive poll
limits and unchanged receive behavior on other handles.

To improve intra-frame detail, the configured encoder target increases from
1.5 to 3 Mbit/s at 30 encoded fps, and maximum QP decreases from 40 to 36.
The SIP target stays 15 fps at 640×480. The transmitted video bitrate depends
on which complete encoded frames SIP sends; the encoder target is not a
guarantee of network throughput. Independent frames and explicit AUDs remain
necessary for the latest-frame publisher and this Linphone decoder path.

The audio-working image is preserved in
`backups/sip-audio-working/internship.bin`. The quality update passes all host
tests and the ESP-IDF build. Its SHA-256 is
`1750e136a66ae84879031021840836b5452d02a4bbc5dc8bc593748b09050840`.
It was flashed at `0x10000` and its flash hash verified. In the first live call,
the sender reaches about 15 frames/s through much of the sample, with zero
video queue drops or send errors. Some intervals still supply fewer camera
frames when the camera publisher is busy. Linphone's sampled decoder rates are
13.75, 13.95, 15.16 and 9.09 fps; its video clock estimate settles near 1.000.
Both audio and video report zero lost or late RTP packets at hangup. The board
continues supplying approximately 8,000 microphone samples/s with no queue
drops or I2S errors. The user confirmed that the update worked and this version
now works well. The verified firmware, sdkconfig and SHA-256 checksum are saved
in `backups/sip-av-verified/`.

## Final speaker-only endpoint check

The user later commented out microphone transmission in `_send_audio` for
speaker-only testing. Laptop Linphone → board playback produced a scream, but
smartphone Linphone → board playback worked correctly with the same board
setup. No receive-audio firmware change was made for this symptom. The endpoint
comparison implicated the laptop configuration/test setup; the exact setting
or acoustic mechanism was not established. The microphone capture task still
starts even while `_send_audio` returns zero. The saved verified firmware
predates this user's source edit.

For the worked explanations and ordered study labs, see
[PROJECT_TECHNICAL_GUIDE.md](PROJECT_TECHNICAL_GUIDE.md) and
[LEARNING_PATH.md](LEARNING_PATH.md).
