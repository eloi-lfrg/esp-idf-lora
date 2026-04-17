# ha-lora-gateway

LoRa gateway on ESP32 for Home Assistant, based on ESP-IDF 6 and RadioLib.  
The project implements a bidirectional ping-pong node (master/slave) and provides a reusable radio layer (`LoraRadio`) that can be integrated into any ESP-IDF application.

---

## Required Hardware

| Component | Value |
|---|---|
| MCU | ESP32 (tested on ESP32-WROVER) |
| Radio module | SX1276 (breakout or RFM95W module) |
| Frequency | 868 MHz (EU) — configurable |
| Interface | SPI (SPI3_HOST / VSPI) |

### Default Wiring

| Signal | ESP32 GPIO |
|---|---|
| SCK | 5 |
| MISO | 19 |
| MOSI | 27 |
| CS | 18 |
| DIO0 | 26 |
| RST | 14 |
| DIO1 | 35 |

All pins are configurable in `LoraRadio::Config`.

---

## Software Prerequisites

- [ESP-IDF 6.0](https://docs.espressif.com/projects/esp-idf/en/v6.0/esp32/get-started/index.html) installed and activated (`idf.py` in PATH)
- CMake ≥ 3.22
- VSCode + [ESP-IDF extension](https://marketplace.visualstudio.com/items?itemName=espressif.esp-idf-tools) (optional)

---

## Project Structure

```
ha-lora-gateway/
├── components/
│   └── lora/
│       ├── EspHal.h / EspHal.cpp     # ESP-IDF HAL for RadioLib
│       ├── LoraRadio.h / LoraRadio.cpp  # High-level LoRa driver
│       └── CMakeLists.txt
├── main/
│   ├── main.cpp                      # Master/slave ping-pong application
│   └── idf_component.yml
├── CMakeLists.txt
└── dependencies.lock
```

---

## Build and Flash

```bash
# Set the target
idf.py set-target esp32

# Build
idf.py build

# Flash (replace /dev/ttyUSB0 with the correct port)
idf.py -p /dev/ttyUSB0 flash monitor
```

---

## Usage — Ping-Pong Application

`main.cpp` implements a simple radio ping-pong controlled by the `IS_MASTER` define:

```cpp
#define IS_MASTER true   // true = master, false = slave
```

**Master**: sends a numbered ping every 2 seconds, waits for a response for 3 seconds.  
**Slave**: waits indefinitely for a ping, responds immediately with `"pong"`.

Build two firmwares (one with `IS_MASTER true`, the other `false`) and flash them onto two boards.

---

## API — `LoraRadio`

### Configuration

```cpp
LoraRadio::Config cfg;
cfg.freq    = 868.0f;  // MHz
cfg.sf      = 9;       // Spreading factor (6-12)
cfg.bw      = 125.0f;  // Bandwidth kHz
cfg.txPower = 17;      // dBm

LoraRadio radio(cfg);
radio.begin();
```

### Transmit

```cpp
// Binary buffer
radio.transmit(buffer, len);

// String
radio.transmit("hello");
```

### Blocking Receive (ping-pong mode)

```cpp
LoraRadio::Packet pkt{};

// With timeout
if (radio.receive(pkt, 3000) == ESP_OK) {
    // pkt.data, pkt.len, pkt.rssi, pkt.snr, pkt.freqErr
}

// No timeout (portMAX_DELAY)
radio.receive(pkt, portMAX_DELAY);
```

### Continuous Receive with Callback

```cpp
radio.setOnRx([](const LoraRadio::Packet &pkt) {
    ESP_LOGI("RX", "Received %d bytes, RSSI=%.1f", pkt.len, pkt.rssi);
});

radio.startReceive();
// ...
radio.stopReceive();
```

### Runtime Parameter Changes

```cpp
radio.setFreq(915.0f);   // Change frequency
radio.setSF(12);          // Change spreading factor
radio.setTxPower(20);     // Change TX power
```

---

## Internal Architecture

### `EspHal`

Implements the `RadioLibHal` interface using native ESP-IDF APIs:

| Feature | ESP-IDF API |
|---|---|
| GPIO | `gpio_config`, `gpio_set_level`, `gpio_get_level` |
| Interrupts | `gpio_isr_handler_add` (IRAM service) |
| SPI | `spi_bus_initialize`, `spi_device_polling_transmit` (SPI3_HOST, 4 MHz) |
| Timing | `esp_timer_get_time`, `vTaskDelay` |

### `LoraRadio`

Two receive models:

- **Continuous** (`startReceive`): FreeRTOS `lora_rx` task sleeping on `ulTaskNotifyTake`, woken by the DIO0 ISR.
- **Blocking** (`receive`): the calling thread sleeps on `ulTaskNotifyTake` — no busy-wait, the IDLE task is never starved.

The ISR is placed in IRAM (`IRAM_ATTR`) and uses `vTaskNotifyGiveFromISR` + `portYIELD_FROM_ISR` for immediate wake-up at the correct priority.

---

## Dependencies

| Dependency | Version | Source |
|---|---|---|
| ESP-IDF | 6.0.0 | Espressif |
| [RadioLib](https://github.com/jgromes/RadioLib) | 7.6.0 | Espressif Component Registry |

---

## Tests

Tests are located in `tests/lora/`. Two levels exist: pure Python tests (no hardware) and Unity integration tests that run on the ESP32.

### Prerequisites

```bash
cd tests/lora
source venv/bin/activate   # venv already provisioned
# or, if missing:
python3 -m venv venv && source venv/bin/activate && pip install -r requirements.txt
```

### Software-only Tests (default)

These tests parse C++ headers and verify constants, `Config` default values, `Packet` structure, and API surface — no ESP32 required.

```bash
cd tests/lora
pytest -v -m "not hardware"
```

Run a single test:

```bash
pytest -v -m "not hardware" -k "test_default_frequency"
```

### Hardware Integration Tests (ESP32 + SX1276)

These tests automatically flash the Unity firmware onto the ESP32, read the serial output, and report each `TEST_CASE` as a separate pytest test.

```bash
cd tests/lora
pytest -v   # auto-detects the serial port
```

The port is read from `.vscode/settings.json` (`idf.port`) or auto-detected. To specify it manually, modify `_serial_port()` in `test_lora.py` or set `idf.port` in VSCode settings.

> **Note:** `IDF_PATH` and the ESP-IDF Python path are hardcoded in `test_lora.py` — update them if your ESP-IDF installation differs from `/Users/eloi/.espressif/`.

### Running Unity Tests Manually

To run the tests without pytest (direct serial monitor):

```bash
idf.py -C tests/lora flash monitor
# In the monitor: type '!' then Enter to run all tests
```

### Adding a Test

- **Software test**: add a method to the appropriate class in `tests/lora/test_lora.py`.
- **Integration test**: add a `TEST_CASE("…", "[radio]")` in `tests/lora/main/test_lora_radio.cpp`; it will be automatically discovered and executed by pytest.

---

## License

This project is distributed under the MIT license.
