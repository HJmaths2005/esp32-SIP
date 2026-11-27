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
- [configuration](#configuration)
- [limitations](#limitations)

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

> you may need to flash the matching ESP32-C6 radio firmware before running the ESP32-P4 application. The C6 firmware must be compatible with the ESP-Hosted version used by the ESP-IDF v5.5 build. if you want to do this in this specific board you will need a FTDI USB to UART converter and use it with the pin header beside the C6 chip. Firmware needed on C6 is also included in the project as slave folder so you have access to that.If your C6 is preflashed you can use ESP OTA capabilities for updating C6 firmware.

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
cd ./internship
```

### 4. Export environment

```bash
export IDF_PATH=/path/to/esp-idf
export ADF_PATH=/path/to/esp-adf
. $IDF_PATH/export.sh
```

### 5.building project:

run 
```bash
idf.py build
```
remember to avoid dependency hell do not run any command like
```bash
idf.py menuconfig
```
or 

```bash
idf.py set-target
```
or every other command that resets the sdkconfig. every thing you need is being included in the sdkconfig file.

### 6. updating LCD commands 
this module uses jd9365 lcd controller that many vendors manufacture it under the same name so you will need to use correct commands for this lcd. correct command set is included inside the project under the file name lcd_command.txt. you need to paste those commands in the C driver for the jd9365 that will be added to the project after fetching needed components in the path managed_components/espressif__esp_lcd_jd9365. the file name is esp_lcd_jd9365.c . you should copy and paste these commands inside this file under the tatic const jd9365_lcd_init_cmd_t vendor_specific_init_default[].(this array contains commands for the lcd you will need to replace them with commands inside lcd_command.txt file)


## Configuration:

only configuration you might need is the SIP video call video encoding that you can do it in the menuconfig.

---

## limitations

- RTSP streaming supports **MJPEG only**.
- SIP video calls support **MJPEG** and **H.264**.
- H.264 compatibility depends on the peer’s supported profile and decoder.
- ESP32-C6 radio firmware must match the ESP-Hosted version used by the ESP32-P4 application.
- Network performance depends on Wi-Fi signal quality, camera resolution, FPS, and selected codec.
- For stable RTSP playback over lossy networks, prefer TCP interleaved transport.


---
