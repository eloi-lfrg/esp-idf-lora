# C++ Standards — ha-lora-gateway

Apply these rules to every C++ file in this repository. They are derived from
the existing code (`EspHal`, `LoraRadio`) and must be followed for all new code.

## Language

- All code in English: identifiers, comments, log messages.
- No French in source files.

## Naming

| Construct | Convention |
|---|---|
| Class / struct | PascalCase (`LoraRadio`) |
| Method | camelCase (`startReceive`) |
| Instance member | `m_` + camelCase (`m_rxTask`) |
| Static member | `s_` + camelCase (`s_rxFlag`) |
| Local / parameter | camelCase (`timeoutMs`) |
| Macro / `#define` | UPPER_SNAKE_CASE (`LORA_MAX_PACKET`) |
| Callback type alias | PascalCase + `Cb` (`OnRxCb`) |
| FreeRTOS task name | snake_case string (`"lora_rx"`) |
| Logging TAG | `static constexpr const char *TAG = "ClassName"` |

## Headers

- `#pragma once` always — never `#ifndef` guards.
- File name matches class name: `LoraRadio.h`.

## Constructors

- `explicit` on every single-argument constructor.
- Constructors only initialise data members; hardware init belongs in `begin()`.
- Declare members in the order they must be initialised (dependencies first).

## Error handling

- Return `esp_err_t` for any function that can fail.
- `ESP_OK` / `ESP_FAIL` / standard `ESP_ERR_*` only.
- Log with `ESP_LOGE(TAG, …)` before returning failure.
- Never swallow errors silently.

## Logging

- `ESP_LOGI/W/E/D` with class `TAG` — no `printf`, no `std::cout`.
- All messages in English.

## ISR

- `IRAM_ATTR` on every ISR function.
- Only IRAM-safe calls inside ISRs.
- Wake tasks with `vTaskNotifyGiveFromISR()` + `portYIELD_FROM_ISR()`.

## FreeRTOS

- Prefer `ulTaskNotifyTake` / `xTaskNotifyGive` over semaphores for simple signalling.
- Task entry point: `static void xyzEntry(void *arg)` → delegates to `void xyz()`.
- Null-initialise all `TaskHandle_t` members; clear to `nullptr` after deletion.

## Callbacks

- `std::function<>` type aliases ending in `Cb`.
- Default-init to `nullptr`; always null-check before calling.

## Comments

- Doxygen `/** @brief … @param … @return … */` for all public API.
- Private `//` comment only when the *why* is non-obvious.
- No commented-out code.

## Class header layout

```
public:
  nested types → lifecycle → operations → callback setters → parameter setters
private:
  static constexpr TAG
  // ── Group ──
  members
  private methods
```

## Section separators in `.cpp`

```cpp
// ─── methodName ──────────────────────────────────────────────────────────────
```
