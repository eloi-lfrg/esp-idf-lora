---
trigger: always_on
---

# C++ Coding Standards — ha-lora-gateway

Target: ESP32 / ESP-IDF 6 / FreeRTOS / C++17. No Arduino framework.

---

## 1. Language

All code must be written in English:
- Identifiers (variables, functions, classes, constants, type aliases)
- Comments (`//`, `/* */`)
- Doxygen docstrings
- Log messages (`ESP_LOGI`, `ESP_LOGW`, `ESP_LOGE`)

---

## 2. File conventions

- Headers: `.h` extension (not `.hpp`)
- Sources: `.cpp` extension
- File name matches the class it defines: `LoraRadio.h` / `LoraRadio.cpp`
- Always `#pragma once` — never `#ifndef` include guards

### Include order (within a `.cpp`)

1. Own header (`"LoraRadio.h"`)
2. Standard library (`<cstring>`, `<functional>`)
3. ESP-IDF / FreeRTOS (`"esp_log.h"`, `"freertos/FreeRTOS.h"`)
4. Third-party (`<RadioLib.h>`)

---

## 3. Naming conventions

| Construct | Rule | Example |
|---|---|---|
| Class / struct / enum | PascalCase | `LoraRadio`, `EspHal` |
| Method (public or private) | camelCase | `begin()`, `startReceive()`, `readPacket()` |
| Instance member variable | `m_` + camelCase | `m_rxTask`, `m_cfg`, `m_hal` |
| Static member variable | `s_` + camelCase | `s_rxFlag`, `s_waitingTask` |
| Local variable | camelCase | `timeoutMs`, `wasReceiving` |
| Function parameter | camelCase | `data`, `len`, `timeoutMs` |
| Macro / `#define` constant | UPPER_SNAKE_CASE | `LORA_MAX_PACKET`, `IS_MASTER` |
| `static constexpr` class constant | UPPER_SNAKE_CASE | `static constexpr uint8_t MAX_RETRIES = 3` |
| Callback type alias | PascalCase + `Cb` suffix | `OnRxCb`, `OnTxCb`, `OnErrCb` |
| Nested config / data struct | PascalCase | `Config`, `Packet` |
| FreeRTOS task name (string literal) | snake_case | `"lora_rx"` |
| Logging TAG | `static constexpr const char *TAG` | `"LoraRadio"` |

---

## 4. Classes

### Constructor

```cpp
// Single-argument constructors must be explicit
explicit LoraRadio(const Config &cfg);

// Constructors only initialise data members — no hardware I/O
// Hardware initialisation belongs in begin()
esp_err_t begin();
```

### Member initialisation order

Members are initialised in declaration order. Declare members that others depend on **first**:

```cpp
// m_hal must come before m_radio because m_radio's ctor takes a pointer to m_hal
EspHal   m_hal;
SX1276   m_radio;
```

### Header layout

```cpp
class MyClass {
public:
  // 1. Nested types (Config, Packet, type aliases)
  // 2. Lifecycle (constructor, destructor, begin(), stop())
  // 3. Operational methods
  // 4. Callback setters (setOnRx, setOnTx…)
  // 5. Runtime parameter setters

private:
  static constexpr const char *TAG = "MyClass";

  // ── Group name ───────────────────────────────
  MemberType m_member = defaultValue;

  // ── Another group ────────────────────────────
  static volatile bool s_flag;

  void privateHelper();
};
```

---

## 5. Error handling

- Functions that can fail return `esp_err_t`
- Return `ESP_OK` on success
- Return `ESP_FAIL` for generic / RadioLib errors
- Return standard `ESP_ERR_*` codes (e.g. `ESP_ERR_NO_MEM`, `ESP_ERR_TIMEOUT`) when applicable
- Log with `ESP_LOGE(TAG, "context: %d", state)` before returning failure
- Never silently discard errors

```cpp
esp_err_t LoraRadio::setFreq(float freq) {
  int16_t s = m_radio.setFrequency(freq);
  if (s != RADIOLIB_ERR_NONE) {
    ESP_LOGE(TAG, "setFreq failed: %d", s);
    return ESP_FAIL;
  }
  m_cfg.freq = freq;
  return ESP_OK;
}
```

---

## 6. Logging

- Use `ESP_LOGI` / `ESP_LOGW` / `ESP_LOGE` / `ESP_LOGD` exclusively
- Pass the class `TAG` as first argument
- All messages in English
- No `printf`, `puts`, `std::cout`, or `Serial.print`

```cpp
static constexpr const char *TAG = "LoraRadio";

ESP_LOGI(TAG, "Init OK (%.1fMHz SF%d BW%.0fkHz)", freq, sf, bw);
ESP_LOGE(TAG, "begin() failed: %d", state);
ESP_LOGW(TAG, "receive() timed out (%lums)", timeoutMs);
ESP_LOGD(TAG, "RX %d bytes | RSSI=%.1fdBm", pkt.len, pkt.rssi);
```

---

## 7. Comments and documentation

### Public API — Doxygen

```cpp
/**
 * @brief Block until a packet is received or the timeout elapses.
 *
 * @param[out] pkt       Filled with the received packet on success.
 * @param[in]  timeoutMs Maximum wait in ms, or portMAX_DELAY.
 * @return ESP_OK, ESP_ERR_TIMEOUT, or ESP_FAIL.
 */
esp_err_t receive(Packet &pkt, uint32_t timeoutMs = 5000);
```

### Private implementation

Single-line `//` comments only when the *why* is non-obvious:

```cpp
// Attach ISR before arming RX so no packet is missed at startup
m_radio.setDio0Action(isrHandler, m_hal.GpioInterruptRising);
```

### What not to comment

- Do not explain *what* well-named code does
- No commented-out code
- No `TODO` in committed code

---

## 8. ISR rules

- Every ISR must be annotated `IRAM_ATTR`
- Only call IRAM-safe functions from an ISR (no heap alloc, no flash reads)
- Use `vTaskNotifyGiveFromISR()` + `portYIELD_FROM_ISR()` for task wake-up

```cpp
static void IRAM_ATTR isrHandler();

IRAM_ATTR void LoraRadio::isrHandler() {
  s_rxFlag = true;
  if (s_waitingTask) {
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(s_waitingTask, &higherPriorityTaskWoken);
    portYIELD_FROM_ISR(higherPriorityTaskWoken);
  }
}
```

---

## 9. FreeRTOS patterns

### Task notification (preferred over semaphores for simple signalling)

```cpp
// Blocking side (calling task)
ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(timeoutMs));

// Waking side (from another task — not ISR)
xTaskNotifyGive(m_rxTask);
```

### Task entry point idiom

```cpp
// Static trampoline → member function
static void rxTaskEntry(void *arg);     // declared in class
void rxTask();                          // actual loop

void LoraRadio::rxTaskEntry(void *arg) {
  static_cast<LoraRadio *>(arg)->rxTask();
  vTaskDelete(nullptr);
}
```

### Task handle lifecycle

```cpp
TaskHandle_t m_rxTask = nullptr;        // default-initialised to nullptr

// After deletion
m_rxTask = nullptr;                     // always clear after vTaskDelete
```

---

## 10. Callbacks

```cpp
// Definition
using OnRxCb  = std::function<void(const Packet &)>;
using OnErrCb = std::function<void(int16_t code)>;

// Member
OnRxCb m_onRx = nullptr;

// Usage — always null-check
if (m_onRx)
  m_onRx(pkt);
```

---

## 11. ESP-IDF component structure

- Each reusable component lives in `components/<name>/`
- `CMakeLists.txt` uses `idf_component_register()` with explicit `REQUIRES` / `PRIV_REQUIRES`
- External (registry) dependencies declared in `main/idf_component.yml`
- `idf_build_set_property(MINIMAL_BUILD ON)` in root `CMakeLists.txt` to strip unused components

---

## 12. Section separators in `.cpp`

Use Unicode box-drawing separators to delimit logical sections:

```cpp
// ─── begin ───────────────────────────────────────────────────────────────────
esp_err_t LoraRadio::begin() { … }

// ─── transmit ────────────────────────────────────────────────────────────────
esp_err_t LoraRadio::transmit(…) { … }
```
