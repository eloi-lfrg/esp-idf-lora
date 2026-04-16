# AGENTS.md

Instructions for AI agents working in this repository.

## Project

ESP32 LoRa gateway firmware for Home Assistant. Written in **C++17** on top of **ESP-IDF 6** (no Arduino). Uses **RadioLib 7.6** to drive an SX1276 (RFM95W) over SPI. FreeRTOS is the RTOS.

## Build

```bash
# One-time environment activation (ESP-IDF must be installed)
. $IDF_PATH/export.sh

idf.py set-target esp32   # run once per checkout
idf.py build
idf.py -p /dev/tty.usbserial-XXXX flash monitor
idf.py fullclean           # full rebuild
```

There is no test suite that runs on the host; testing requires flashing to hardware.

## Repository layout

```
components/lora/     reusable ESP-IDF component (EspHal, LoraRadio)
main/                application entry point (ping-pong demo)
```

## Non-negotiable rules

1. **All code in English** — identifiers, comments, log messages, docstrings, no exceptions.
2. **Instance members** prefixed `m_`, **static members** prefixed `s_`.
3. **`esp_err_t`** return type for any function that can fail; return `ESP_OK` / `ESP_FAIL` / standard `ESP_ERR_*`.
4. **No `printf`** — use `ESP_LOGI/W/E` with the class `TAG`.
5. **ISR functions** must be `IRAM_ATTR` and only call IRAM-safe code.
6. **`#pragma once`** in every header, never `#ifndef` guards.
7. **`explicit`** on every single-argument constructor.
8. Constructors only initialise data; hardware init lives in `begin()`.
9. Public API documented with Doxygen `/** @brief … @param … @return … */`.
10. No commented-out code, no debug `printf`, no `TODO` left in committed code.

## Coding conventions (summary)

| Construct | Convention | Example |
|---|---|---|
| Class | PascalCase | `LoraRadio` |
| Method | camelCase | `startReceive()` |
| Instance member | `m_` + camelCase | `m_rxTask` |
| Static member | `s_` + camelCase | `s_rxFlag` |
| Local / param | camelCase | `timeoutMs` |
| Macro / preprocessor constant | UPPER_SNAKE_CASE | `LORA_MAX_PACKET` |
| Callback type alias | PascalCase + `Cb` | `OnRxCb` |
| FreeRTOS task name (string) | snake_case | `"lora_rx"` |
| Logging tag | `static constexpr const char *TAG` | `"LoraRadio"` |
| File | matches class name | `LoraRadio.cpp` |

Full standards: see `.agents/rules/cpp-standards.md`.
