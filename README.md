# GUITION JC8012P4A1C_I_W_Y — Voice & Video Call over SIP/RTSP

![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.5-blue)
![ESP-ADF](https://img.shields.io/badge/ESP--ADF-v2.x-orange)
![Board](https://img.shields.io/badge/Board-GUITION%20JC8012P4A1C__I__W__Y-green)
![Transport](https://img.shields.io/badge/Transport-SIP%20%7C%20RTSP-purple)
![Video](https://img.shields.io/badge/Video-MJPEG%20%7C%20H.264-red)

A complete voice-call and video-call implementation for the **GUITION `JC8012P4A1C_I_W_Y`** board. The board uses an **ESP32-P4** as the main application SoC and an **ESP32-C6** as the radio co-processor. The application runs on **ESP-ADF v2.x**, which is based on **ESP-IDF v5.5**.

The project provides:

- **SIP-based voice calls**
- **SIP-based video calls** using **MJPEG** or **H.264**
- **RTSP streaming** using **MJPEG**
- Full-duplex audio through the ESP-ADF audio pipeline
- Live video preview and call control UI
- Wi-Fi connectivity through the ESP32-C6 radio using ESP-Hosted

---

## Table of Contents

- [Overview](#overview)
- [Hardware](#hardware)
- [Software Stack](#software-stack)
- [Architecture](#architecture)
- [Protocol Support](#protocol-support)
- [Requirements](#requirements)
- [Setup](#setup)
- [Configuration](#configuration)
- [Build and Flash](#build-and-flash)
- [Usage](#usage)
- [RTSP Streaming](#rtsp-streaming)
- [Project Structure](#project-structure)
- [Troubleshooting](#troubleshooting)
- [Notes and Limitations](#notes-and-limitations)

---

## Overview

This project turns the **GUITION `JC8012P4A1C_I_W_Y`** board into a SIP voice/video endpoint and an RTSP MJPEG streaming device.

The **ESP32-P4** handles:

- SIP signalling
- SDP negotiation
- RTP/RTCP media transport
- Audio and video pipelines
- RTSP server
- Camera capture
- LCD display and UI
- Board peripherals

The **ESP32-C6** handles:

- Wi-Fi connectivity
- Radio communication
- Network offload through ESP-Hosted

The application is built with **ESP-ADF v2.x** on top of **ESP-IDF v5.5**. SIP calls support both **MJPEG** and **H.264** video, while RTSP streaming uses **MJPEG**.

---


## Hardware

**Target board:** `GUITION JC8012P4A1C_I_W_Y`

| Component | Role |
|-----------|------|
| ESP32-P4 | Main application SoC: SIP, RTSP, media pipelines, UI, display, camera, audio |
| ESP32-C6 | Radio co-processor: Wi-Fi and network connectivity |
| ESP-Hosted link | High-throughput link between ESP32-P4 and ESP32-C6 |
| On-board LCD | Video preview and call-control UI |
| On-board audio | Microphone input and speaker output |
| Camera interface | Video source for SIP video calls and RTSP streaming |
| SDIO | Hosted transport between P4 and C6 |

> Flash the matching ESP32-C6 radio firmware before running the ESP32-P4 application. The C6 firmware must be compatible with the ESP-Hosted version used by the ESP-IDF v5.5 build.

---

## Software Stack

```text
┌───────────────────────────────────────────────────────────┐
│                     Application Layer                     │
│   SIP Client  │  RTSP Server/Client  │  Call UI           │
├───────────────────────────────────────────────────────────┤
│                     ESP-ADF v2.x                          │
│   Audio Pipeline │ Video Pipeline │ Codec Handling        │
├───────────────────────────────────────────────────────────┤
│                     ESP-IDF v5.5                          │
│   lwIP │ RTP/RTSP │ mbedTLS │ SDIO │ FreeRTOS             │
├───────────────────────────────────────────────────────────┤
│              ESP-Hosted (ESP32-P4 ⇄ ESP32-C6)             │
├───────────────────────────────────────────────────────────┤
│                ESP32-C6 Radio Firmware                    │
│                    Wi-Fi / Network                        │
└───────────────────────────────────────────────────────────┘
```

- **ESP-ADF v2.x** — audio/video pipelines, codec handling, board abstraction
- **ESP-IDF v5.5** — networking, lwIP, RTP/RTSP support, SDIO, FreeRTOS
- **ESP-Hosted** — network transport between ESP32-C6 and ESP32-P4
- **ESP32-C6 firmware** — Wi-Fi radio firmware

---

## Architecture

```text
                 ┌────────────────────────────────┐
     SIP INVITE  │                                │
  ──────────────▶│  SIP Signalling Task           │
                 │  REGISTER / INVITE / BYE       │
                 └────────────┬───────────────────┘
                              │ SDP negotiation
                              ▼
                 ┌────────────────────────────────┐
   RTP Audio     │   ESP-ADF Audio Pipeline       │
  ◀─────────────▶│   Mic → Codec → RTP            │
                 │   RTP → Codec → Speaker        │
                 └────────────────────────────────┘

                 ┌────────────────────────────────┐
   RTP Video     │   Video Pipeline               │
  ◀─────────────▶│   Camera → MJPEG/H.264 → RTP   │
                 │   RTP → Decode → LCD           │
                 └────────────┬───────────────────┘
                              │
                 ┌────────────▼───────────────────┐
   RTSP MJPEG    │   RTSP Server                  │
  ◀─────────────▶│   rtsp://<ip>:8554/stream      │
                 └────────────────────────────────┘
```

### SIP Call Flow

```text
Endpoint A                    SIP Server / Peer                    Endpoint B
    │                                │                                │
    │──── REGISTER ─────────────────▶│                                │
    │◀─── 200 OK ────────────────────│                                │
    │                                │                                │
    │──── INVITE (SDP offer) ───────▶│──── INVITE (SDP offer) ───────▶│
    │                                │◀─── 180 Ringing ───────────────│
    │◀─── 180 Ringing ───────────────│                                │
    │                                │◀─── 200 OK (SDP answer) ───────│
    │◀─── 200 OK (SDP answer) ───────│                                │
    │──── ACK ──────────────────────▶│──── ACK ──────────────────────▶│
    │                                │                                │
    │◀════════════ RTP Audio / Video ════════════════════════════════▶│
    │                                │                                │
    │──── BYE ──────────────────────▶│──── BYE ──────────────────────▶│
    │◀─── 200 OK ────────────────────│◀─── 200 OK ────────────────────│
```

### RTSP Streaming Flow

```text
RTSP Client                                      Board
    │                                              │
    │──── OPTIONS ────────────────────────────────▶│
    │◀─── 200 OK ──────────────────────────────────│
    │                                              │
    │──── DESCRIBE rtsp://<ip>:8554/stream ───────▶│
    │◀─── 200 OK (SDP: MJPEG/RTP) ─────────────────│
    │                                              │
    │──── SETUP ──────────────────────────────────▶│
    │◀─── 200 OK (RTP session) ────────────────────│
    │                                              │
    │──── PLAY ───────────────────────────────────▶│
    │◀─── 200 OK ──────────────────────────────────│
    │                                              │
    │◀════════════ RTP MJPEG Frames ═══════════════│
    │                                              │
    │──── TEARDOWN ───────────────────────────────▶│
    │◀─── 200 OK ──────────────────────────────────│
```

---

## Protocol Support

### SIP

Supported SIP methods:

| Method | Purpose |
|--------|---------|
| `REGISTER` | Register the endpoint with the SIP registrar |
| `INVITE` | Start a voice or video call |
| `ACK` | Confirm final response |
| `BYE` | Terminate an active call |
| `CANCEL` | Cancel a pending INVITE |

Supported media:

| Media | Codec | SIP | RTSP |
|-------|-------|-----|------|
| Audio | ESP-ADF supported audio codec | Yes | No |
| Video | MJPEG | Yes | Yes |
| Video | H.264 | Yes | No |

SDP offers and answers advertise the negotiated audio and video payloads. For video, the selected codec is controlled by configuration:

- `MJPEG` — `image/jpeg`
- `H.264` — `H264/90000`

### RTSP

RTSP server details:

| Item | Value |
|------|-------|
| Default URL | `rtsp://<device-ip>:8554/stream` |
| Default port | `8554` |
| Default path | `/stream` |
| Video codec | MJPEG |
| RTP payload | `image/jpeg` |
| Transport | RTP/AVP over UDP or TCP interleaved |
| Supported methods | OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN |

Tested clients:

- VLC
- ffplay
- GStreamer

---

## Requirements

### Software

- **ESP-IDF v5.5**
- **ESP-ADF v2.x**
- Python 3.10+
- CMake
- Ninja
- ESP32-P4 toolchain bundled with ESP-IDF v5.5
- Compatible ESP32-C6 radio firmware for ESP-Hosted

### Hardware

- GUITION `JC8012P4A1C_I_W_Y` board
- USB cables for ESP32-P4 and ESP32-C6 flashing
- Wi-Fi access point
- SIP server or SIP peer
- RTSP client such as VLC, ffplay, or GStreamer for testing

---

## Setup

### 1. Install ESP-IDF v5.5

```bash
git clone -b v5.5 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh
. ./export.sh
```

### 2. Install ESP-ADF v2.x

```bash
git clone -b v2.x --recursive https://github.com/espressif/esp-adf.git
export ADF_PATH=/path/to/esp-adf
```

### 3. Clone this project

```bash
git clone <repository-url>
cd guition-p4-sip-rtsp
```

### 4. Export environment

```bash
export IDF_PATH=/path/to/esp-idf
export ADF_PATH=/path/to/esp-adf
. $IDF_PATH/export.sh
```

### 5. Set target

```bash
idf.py set-target esp32p4
```

### 6. Configure the project

```bash
idf.py menuconfig
```

Configure:

- Wi-Fi SSID and password
- SIP server URI
- SIP username and password
- SIP video codec: MJPEG or H.264
- RTSP port and stream path
- Camera resolution and frame rate
- Display and board options

---

## Configuration

Configuration is available through `idf.py menuconfig` and can also be overridden in `main/app_config.h`.

| Option | Default | Description |
|--------|---------|-------------|
| `CONFIG_WIFI_SSID` | — | Wi-Fi SSID |
| `CONFIG_WIFI_PASSWORD` | — | Wi-Fi password |
| `CONFIG_SIP_SERVER_URI` | `sip:192.168.1.10` | SIP registrar or proxy URI |
| `CONFIG_SIP_USERNAME` | `esp32p4` | SIP username |
| `CONFIG_SIP_PASSWORD` | `secret` | SIP password |
| `CONFIG_SIP_VIDEO_CODEC` | `MJPEG` | SIP video codec: `MJPEG` or `H264` |
| `CONFIG_RTSP_PORT` | `8554` | RTSP server port |
| `CONFIG_RTSP_STREAM_PATH` | `/stream` | RTSP stream path |
| `CONFIG_CAMERA_FPS` | `15` | Camera frame rate |
| `CONFIG_CAMERA_RESOLUTION` | `VGA` | Camera resolution |

Example `main/app_config.h`:

```c
#pragma once

#define APP_WIFI_SSID              "your-ssid"
#define APP_WIFI_PASSWORD          "your-password"

#define APP_SIP_SERVER_URI         "sip:192.168.1.10"
#define APP_SIP_USERNAME           "esp32p4"
#define APP_SIP_PASSWORD           "secret"

#define APP_SIP_VIDEO_CODEC_MJPEG  1
#define APP_SIP_VIDEO_CODEC_H264   2
#define APP_SIP_VIDEO_CODEC        APP_SIP_VIDEO_CODEC_MJPEG

#define APP_RTSP_PORT              8554
#define APP_RTSP_STREAM_PATH       "/stream"

#define APP_CAMERA_FPS             15
#define APP_CAMERA_RESOLUTION      "VGA"
```

---

## Build and Flash

### 1. Build the application

```bash
idf.py build
```

### 2. Flash ESP32-C6 radio firmware

Flash the compatible ESP32-C6 radio firmware using the ESP-Hosted or board-specific flashing procedure. The exact command depends on the C6 flashing setup on your board.

> The ESP32-C6 firmware must match the ESP-Hosted version used by the ESP32-P4 application.

### 3. Flash ESP32-P4 application

```bash
idf.py -p <P4_PORT> flash
```

### 4. Monitor logs

```bash
idf.py -p <P4_PORT> monitor
```

Examples for serial ports:

```bash
# Linux
idf.py -p /dev/ttyACM0 flash monitor

# Windows
idf.py -p COM5 flash monitor
```

### 5. Clean rebuild

```bash
idf.py fullclean
idf.py build
```

---

## Usage

### Boot Sequence

On boot, the board performs the following steps:

1. Initializes the ESP32-C6 hosted link.
2. Connects to the configured Wi-Fi network.
3. Registers with the SIP server.
4. Starts the RTSP server.
5. Initializes the camera, audio, and LCD.
6. Displays the idle screen with local video preview.

### Voice Call

From the board UI:

```text
Dial → sip:<peer>@<server> → Voice
```

Or call the board from another SIP endpoint using its registered SIP AOR.

### Video Call

From the board UI:

```text
Dial → sip:<peer>@<server> → Video
```

The video codec is selected by `CONFIG_SIP_VIDEO_CODEC`:

- MJPEG
- H.264

The SIP call negotiates the video payload through SDP.

### Incoming Call

When an INVITE is received, the UI displays:

```text
Incoming call from <caller-id>

[ Answer ]    [ Reject ]
```

### Hang Up

Press the hang-up control in the UI or send a BYE from the remote peer.

---

## RTSP Streaming

Start the board and note its IP address from the serial log or UI.

Default RTSP URL:

```text
rtsp://<device-ip>:8554/stream
```

### VLC

```bash
vlc rtsp://<device-ip>:8554/stream
```

### ffplay

```bash
ffplay -rtsp_transport tcp rtsp://<device-ip>:8554/stream
```

### GStreamer

```bash
gst-launch-1.0 rtspsrc location=rtsp://<device-ip>:8554/stream ! \
    rtpjpegdepay ! jpegdec ! autovideosink
```

### RTSP Notes

- RTSP streaming uses MJPEG only.
- SIP video calls support both MJPEG and H.264.
- If UDP streaming is unstable, use TCP interleaved transport.
- Lower camera FPS or resolution if the stream stutters.

---

## Project Structure

```text
guition-p4-sip-rtsp/
├── CMakeLists.txt
├── sdkconfig.defaults
├── partitions.csv
├── main/
│   ├── CMakeLists.txt
│   ├── app_main.c
│   ├── app_config.h
│   ├── wifi/
│   │   └── wifi_manager.c
│   ├── sip/
│   │   ├── sip_client.c
│   │   ├── sdp.c
│   │   └── rtp_transport.c
│   ├── rtsp/
│   │   ├── rtsp_server.c
│   │   └── rtsp_mjpeg.c
│   ├── media/
│   │   ├── audio_pipeline.c
│   │   ├── video_pipeline.c
│   │   └── h264_encoder.c
│   ├── ui/
│   │   ├── ui_main.c
│   │   └── lcd_init.c
│   └── board/
│       └── board.c
├── components/
│   └── esp32_c6_hosted/
└── README.md
```

### Main Modules

| Module | Description |
|--------|-------------|
| `wifi_manager.c` | Wi-Fi station connection and reconnection |
| `sip_client.c` | SIP registration and call state machine |
| `sdp.c` | SDP offer/answer construction and parsing |
| `rtp_transport.c` | RTP/RTCP transport over UDP |
| `rtsp_server.c` | RTSP server state machine |
| `rtsp_mjpeg.c` | MJPEG RTP packetization for RTSP |
| `audio_pipeline.c` | ESP-ADF audio capture and playback |
| `video_pipeline.c` | Camera capture and video encoding |
| `h264_encoder.c` | H.264 encoding path |
| `ui_main.c` | Call UI and status display |
| `lcd_init.c` | LCD initialization and display driver |
| `board.c` | Board pin mapping and power configuration |

---

## Troubleshooting

| Symptom | Possible Cause | Fix |
|---------|----------------|-----|
| `ESP-Hosted: SDIO init failed` | C6 firmware mismatch or SDIO issue | Re-flash the compatible ESP32-C6 radio firmware; check ESP-Hosted version |
| Wi-Fi does not connect | Wrong SSID or password | Verify `CONFIG_WIFI_SSID` and `CONFIG_WIFI_PASSWORD` |
| SIP `REGISTER` returns `401` | Wrong credentials or realm mismatch | Verify SIP username, password, and server URI |
| SIP registration times out | Network or server unreachable | Check IP configuration and SIP server address |
| No video in SIP call | Codec mismatch | Ensure both peers support the selected codec; try MJPEG |
| H.264 video does not decode on peer | Peer lacks compatible H.264 profile | Use baseline profile or switch to MJPEG |
| RTSP stream stutters | High FPS/resolution or UDP loss | Lower FPS/resolution; use TCP transport |
| RTSP client cannot connect | Wrong IP, port, or path | Verify `rtsp://<device-ip>:8554/stream` |
| LCD stays black | Display init or board config issue | Check LCD initialization and board configuration |
| Audio is distorted | Codec or clock configuration issue | Check audio pipeline and codec settings |
| `ADF_PATH` not found | ESP-ADF environment not exported | Export `ADF_PATH` before building |
| Build fails after switching targets | Stale build artifacts | Run `idf.py fullclean` and rebuild |

### Enable Verbose Logs

```bash
idf.py menuconfig
```

Go to:

```text
Component config → Log output → Default log verbosity → Debug
```

Then rebuild and monitor:

```bash
idf.py build flash monitor
```

---

## Notes and Limitations

- RTSP streaming supports **MJPEG only**.
- SIP video calls support **MJPEG** and **H.264**.
- H.264 compatibility depends on the peer’s supported profile and decoder.
- ESP32-C6 radio firmware must match the ESP-Hosted version used by the ESP32-P4 application.
- Network performance depends on Wi-Fi signal quality, camera resolution, FPS, and selected codec.
- For stable RTSP playback over lossy networks, prefer TCP interleaved transport.
