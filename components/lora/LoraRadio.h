// LoraRadio.h
#pragma once

#include "EspHal.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <RadioLib.h>
#include <cstring>
#include <functional>

/** Maximum LoRa payload length (SX1276 hardware limit). */
#define LORA_MAX_PACKET 255

/**
 * @brief High-level LoRa radio driver for the SX1276, built on RadioLib.
 *
 * Provides two reception models:
 *  - **Continuous / interrupt-driven** (startReceive / stopReceive): a
 *    FreeRTOS task waits for DIO0 interrupts and delivers packets via an
 *    OnRxCb callback or an internal queue.
 *  - **Blocking** (receive): the calling task sleeps on a task notification
 *    until a packet arrives or the timeout expires.  No busy-wait — the IDLE
 *    task is never starved.
 *
 * Both modes are mutually exclusive; startReceive() must not be active while
 * calling receive().
 *
 * Thread-safety: transmit() and receive() must be called from a single task.
 * Callbacks registered with setOnRx/setOnTx/setOnErr are invoked from the
 * internal "lora_rx" FreeRTOS task.
 */
class LoraRadio {
public:
  // ── Configuration ─────────────────────────────────────────────────────────

  /**
   * @brief Full hardware and RF configuration for the radio.
   *
   * All fields have sensible defaults for a typical EU868 deployment with an
   * SX1276 wired to the VSPI bus of an ESP32.
   */
  struct Config {
    // SPI bus pins
    int8_t sck  = 5;   ///< SPI clock GPIO.
    int8_t miso = 19;  ///< SPI MISO GPIO.
    int8_t mosi = 27;  ///< SPI MOSI GPIO.
    // Radio control pins
    int8_t cs   = 18;  ///< Chip-select GPIO (active low).
    int8_t dio0 = 26;  ///< DIO0 GPIO — TX done / RX done interrupt.
    int8_t rst  = 14;  ///< Reset GPIO (active low).
    int8_t dio1 = 35;  ///< DIO1 GPIO — RX timeout / CAD done.
    // RF parameters
    float    freq     = 868.0f; ///< Centre frequency in MHz.
    float    bw       = 125.0f; ///< Bandwidth in kHz (7.8 … 500).
    uint8_t  sf       = 7;      ///< Spreading factor (6–12).
    uint8_t  cr       = 5;      ///< Coding rate denominator: 5 → 4/5, … 8 → 4/8.
    int8_t   txPower  = 14;     ///< TX power in dBm (2–17, or 20 with PA_BOOST).
    uint8_t  syncWord = 0x12;   ///< Sync word: 0x12 = private, 0x34 = public (TTN).
    uint16_t preamble = 8;      ///< Preamble length in symbols.
    bool     crc      = true;   ///< Enable CRC on payloads.
    bool     invertIQ = false;  ///< Invert IQ (set true on the gateway side for LoRaWAN).
  };

  // ── Received packet ───────────────────────────────────────────────────────

  /**
   * @brief Metadata and payload of a received LoRa packet.
   */
  struct Packet {
    uint8_t  data[LORA_MAX_PACKET]; ///< Raw payload bytes.
    size_t   len;                   ///< Number of valid bytes in @p data.
    float    rssi;                  ///< Received signal strength in dBm.
    float    snr;                   ///< Signal-to-noise ratio in dB.
    float    freq;                  ///< Actual reception frequency (informational).
    int32_t  freqErr;               ///< Frequency error in Hz.
  };

  // ── Callback types ────────────────────────────────────────────────────────

  /** Called from the RX task each time a valid packet is received. */
  using OnRxCb  = std::function<void(const Packet &)>;
  /** Called from the task context after a successful transmission. */
  using OnTxCb  = std::function<void()>;
  /** Called when RadioLib reports a non-zero error code. */
  using OnErrCb = std::function<void(int16_t code)>;

  // ── Lifecycle ─────────────────────────────────────────────────────────────

  /**
   * @brief Construct the driver with the given configuration.
   *
   * Does not touch the hardware; call begin() to initialise the radio.
   *
   * @param cfg Hardware pin assignment and RF parameters.
   */
  explicit LoraRadio(const Config &cfg);

  /**
   * @brief Destructor — calls stop() to shut down the radio and RX task.
   */
  ~LoraRadio();

  /**
   * @brief Initialise the SPI bus, configure the SX1276 and create the RX queue.
   *
   * Must be called once before any TX/RX operation.
   *
   * @return ESP_OK on success, ESP_FAIL or ESP_ERR_NO_MEM on error.
   */
  esp_err_t begin();

  /**
   * @brief Stop the RX task, put the radio to sleep and free resources.
   *
   * Safe to call multiple times; idempotent.
   */
  void stop();

  // ── Transmit ──────────────────────────────────────────────────────────────

  /**
   * @brief Transmit @p len bytes and block until the transmission completes.
   *
   * If continuous RX is running it is paused for the duration of the
   * transmission and automatically restarted afterwards.
   *
   * @param data Pointer to the payload buffer.
   * @param len  Number of bytes to send (max LORA_MAX_PACKET).
   * @return ESP_OK on success, ESP_FAIL on radio error.
   */
  esp_err_t transmit(const uint8_t *data, size_t len);

  /**
   * @brief Transmit a null-terminated string.
   *
   * Convenience overload — delegates to transmit(const uint8_t *, size_t).
   *
   * @param str Null-terminated ASCII string.
   * @return ESP_OK on success, ESP_FAIL on radio error.
   */
  esp_err_t transmit(const char *str);

  // ── Continuous receive ────────────────────────────────────────────────────

  /**
   * @brief Start continuous interrupt-driven reception.
   *
   * Arms the SX1276 in continuous RX mode and spawns a FreeRTOS task
   * ("lora_rx") that processes incoming packets.  Each valid packet is
   * delivered via the OnRxCb callback if registered, or pushed to an internal
   * queue of depth 8 otherwise.
   *
   * No-op if already running.
   *
   * @return ESP_OK on success, ESP_FAIL on radio error.
   */
  esp_err_t startReceive();

  /**
   * @brief Stop continuous reception and delete the RX task.
   *
   * Sends a task notification to wake the RX task so it exits cleanly, then
   * puts the radio in standby.
   *
   * No-op if not running.
   *
   * @return ESP_OK always.
   */
  esp_err_t stopReceive();

  /**
   * @brief Block the calling task until a packet is received or the timeout elapses.
   *
   * Arms the radio in single-receive mode, then sleeps on a FreeRTOS task
   * notification.  The DIO0 ISR wakes the caller via vTaskNotifyGiveFromISR()
   * — no busy-wait, IDLE tasks are never starved.
   *
   * Pass portMAX_DELAY to wait indefinitely.
   *
   * @note Do not call while startReceive() is active.
   *
   * @param[out] pkt       Filled with the received packet on success.
   * @param[in]  timeoutMs Maximum wait time in milliseconds, or portMAX_DELAY.
   * @return ESP_OK on success, ESP_ERR_TIMEOUT on timeout, ESP_FAIL on radio error.
   */
  esp_err_t receive(Packet &pkt, uint32_t timeoutMs = 5000);

  // ── Callbacks ─────────────────────────────────────────────────────────────

  /**
   * @brief Register a callback invoked on each successfully received packet.
   * @param cb Callback (invoked from the "lora_rx" task context).
   */
  void setOnRx(OnRxCb cb)  { m_onRx  = cb; }

  /**
   * @brief Register a callback invoked after each successful transmission.
   * @param cb Callback (invoked from the caller's task context).
   */
  void setOnTx(OnTxCb cb)  { m_onTx  = cb; }

  /**
   * @brief Register a callback invoked on RadioLib errors.
   * @param cb Callback receiving the RadioLib error code.
   */
  void setOnErr(OnErrCb cb) { m_onErr = cb; }

  // ── Runtime parameter updates ─────────────────────────────────────────────

  /**
   * @brief Change the centre frequency at runtime.
   * @param freq New frequency in MHz.
   * @return ESP_OK on success, ESP_FAIL on radio error.
   */
  esp_err_t setFreq(float freq);

  /**
   * @brief Change the spreading factor at runtime.
   * @param sf New spreading factor (6–12).
   * @return ESP_OK on success, ESP_FAIL on radio error.
   */
  esp_err_t setSF(uint8_t sf);

  /**
   * @brief Change the TX output power at runtime.
   * @param dbm New power in dBm (2–17, or 20 with PA_BOOST).
   * @return ESP_OK on success, ESP_FAIL on radio error.
   */
  esp_err_t setTxPower(int8_t dbm);

  /** @brief Return the RSSI of the last received packet in dBm. */
  float   getRSSI()    { return m_radio.getRSSI(); }

  /** @brief Return the SNR of the last received packet in dB. */
  float   getSNR()     { return m_radio.getSNR(); }

  /** @brief Return the frequency error of the last received packet in Hz. */
  int32_t getFreqErr() { return m_radio.getFrequencyError(); }

private:
  static constexpr const char *TAG = "LoraRadio";

  // m_hal must be declared before m_radio (member initialisation order).
  EspHal   m_hal;
  SX1276   m_radio;
  Config   m_cfg;
  OnRxCb   m_onRx  = nullptr;
  OnTxCb   m_onTx  = nullptr;
  OnErrCb  m_onErr = nullptr;

  // ── Continuous RX task ────────────────────────────────────────────────────
  TaskHandle_t  m_rxTask    = nullptr;
  QueueHandle_t m_rxQueue   = nullptr; ///< Packet queue used when no OnRxCb is set.
  volatile bool m_rxRunning = false;

  /** FreeRTOS task entry point — delegates to rxTask(). */
  static void rxTaskEntry(void *arg);

  /** Main loop of the continuous RX task. */
  void rxTask();

  // ── ISR / task notification ───────────────────────────────────────────────

  /** Set to true by the ISR when DIO0 fires; cleared by the consumer. */
  static volatile bool s_rxFlag;

  /** Handle of the task currently blocked in receive(), or nullptr. */
  static TaskHandle_t s_waitingTask;

  /**
   * @brief DIO0 ISR — sets s_rxFlag and unblocks the waiting task.
   *
   * Placed in IRAM so it can execute even when the flash cache is disabled.
   * Uses vTaskNotifyGiveFromISR() + portYIELD_FROM_ISR() for immediate
   * wake-up of the blocked task.
   */
  static void IRAM_ATTR isrHandler();

  /**
   * @brief Apply supplementary config options not covered by SX1276::begin().
   *
   * Currently configures CRC mode and IQ inversion.
   *
   * @return ESP_OK on success, ESP_FAIL on the first radio error encountered.
   */
  esp_err_t applyConfig();

  /**
   * @brief Read a packet from the SX1276 into @p pkt.
   *
   * Validates the packet length, reads the payload via RadioLib and populates
   * RSSI, SNR and frequency error fields.
   *
   * @param[out] pkt Destination packet structure.
   * @return true if the packet was read successfully, false on error.
   */
  bool readPacket(Packet &pkt);
};
