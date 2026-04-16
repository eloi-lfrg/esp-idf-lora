#pragma once

#include "RadioLib.h"
#include "driver/spi_master.h"

/**
 * @brief ESP-IDF hardware abstraction layer for RadioLib.
 *
 * Implements the RadioLibHal interface using native ESP-IDF APIs so that
 * RadioLib can operate without the Arduino framework.  A single instance
 * manages one SPI bus (SPI3_HOST) and the GPIO interrupt service required
 * by RadioLib for DIOx signalling.
 *
 * Interrupt modes exposed to RadioLib:
 *   - GpioInterruptRising  → GPIO_INTR_POSEDGE
 *   - GpioInterruptFalling → GPIO_INTR_NEGEDGE
 */
class EspHal : public RadioLibHal {
public:
  /**
   * @brief Construct the HAL and install the GPIO ISR service.
   *
   * The ISR service is installed once with ESP_INTR_FLAG_IRAM so that
   * interrupt handlers placed in IRAM can fire even during flash operations.
   * Duplicate installation (ESP_ERR_INVALID_STATE) is silently ignored.
   *
   * @param sck  GPIO number for SPI clock.
   * @param miso GPIO number for SPI MISO.
   * @param mosi GPIO number for SPI MOSI.
   */
  explicit EspHal(int8_t sck, int8_t miso, int8_t mosi);

  // ── Lifecycle ──────────────────────────────────────────────────────────────

  /**
   * @brief Initialize the HAL — opens the SPI bus.
   *
   * Called automatically by RadioLib inside the radio module's begin().
   */
  void init() override;

  /**
   * @brief Tear down the HAL — releases the SPI bus.
   *
   * Called automatically by RadioLib when the module is destroyed or put to
   * sleep.
   */
  void term() override;

  // ── GPIO ──────────────────────────────────────────────────────────────────

  /**
   * @brief Configure a GPIO pin direction.
   *
   * No-op if @p pin is RADIOLIB_NC.
   *
   * @param pin  GPIO number.
   * @param mode GPIO mode (GPIO_MODE_INPUT / GPIO_MODE_OUTPUT).
   */
  void pinMode(uint32_t pin, uint32_t mode) override;

  /**
   * @brief Set a GPIO output level.
   *
   * No-op if @p pin is RADIOLIB_NC.
   *
   * @param pin   GPIO number.
   * @param value Non-zero for logic high, zero for logic low.
   */
  void digitalWrite(uint32_t pin, uint32_t value) override;

  /**
   * @brief Read a GPIO input level.
   *
   * @param pin GPIO number.
   * @return 1 if high, 0 if low. Returns 0 for RADIOLIB_NC.
   */
  uint32_t digitalRead(uint32_t pin) override;

  /**
   * @brief Attach an interrupt handler to a GPIO pin.
   *
   * No-op if @p pin is RADIOLIB_NC.  The lower 3 bits of @p mode are
   * interpreted as gpio_int_type_t (POSEDGE, NEGEDGE, …).
   *
   * @param pin  GPIO number.
   * @param cb   ISR callback (must be in IRAM if flash cache may be disabled).
   * @param mode Trigger mode (use GpioInterruptRising / GpioInterruptFalling).
   */
  void attachInterrupt(uint32_t pin, void (*cb)(void), uint32_t mode) override;

  /**
   * @brief Remove the interrupt handler from a GPIO pin.
   *
   * No-op if @p pin is RADIOLIB_NC.
   *
   * @param pin GPIO number.
   */
  void detachInterrupt(uint32_t pin) override;

  // ── Timing ────────────────────────────────────────────────────────────────

  /**
   * @brief Block the calling task for at least @p ms milliseconds.
   *
   * Delegates to vTaskDelay(), yielding the CPU to other tasks.
   *
   * @param ms Duration in milliseconds.
   */
  void delay(RadioLibTime_t ms) override;

  /**
   * @brief Busy-wait for at least @p us microseconds.
   *
   * Uses esp_timer_get_time() spin loop because vTaskDelay() has only 1 ms
   * resolution.  Avoid calling for long durations as this blocks the CPU.
   *
   * @param us Duration in microseconds.
   */
  void delayMicroseconds(RadioLibTime_t us) override;

  /**
   * @brief Return elapsed time in milliseconds since boot.
   *
   * Backed by the 64-bit esp_timer (1 µs resolution).
   *
   * @return Milliseconds since first boot.
   */
  RadioLibTime_t millis() override;

  /**
   * @brief Return elapsed time in microseconds since boot.
   *
   * @return Microseconds since first boot.
   */
  RadioLibTime_t micros() override;

  /**
   * @brief Measure the duration of a pulse on a GPIO pin.
   *
   * Waits for the pin to reach @p state, then measures how long it stays
   * there.  Returns 0 if the pin does not reach @p state before @p timeout
   * microseconds elapse.
   *
   * @param pin     GPIO number.
   * @param state   Logic level to measure (0 or 1).
   * @param timeout Maximum wait time in microseconds.
   * @return Pulse duration in microseconds, or 0 on timeout / RADIOLIB_NC.
   */
  long pulseIn(uint32_t pin, uint32_t state, RadioLibTime_t timeout) override;

  // ── SPI ───────────────────────────────────────────────────────────────────

  /**
   * @brief Initialize SPI3_HOST and add the radio device at 4 MHz, mode 0.
   *
   * CS is managed externally by RadioLib (spics_io_num = -1).
   * DMA channel is selected automatically.
   */
  void spiBegin() override;

  /**
   * @brief Begin an SPI transaction — no-op on ESP-IDF.
   *
   * Transaction framing is handled per-transfer by spi_device_polling_transmit.
   */
  void spiBeginTransaction() override;

  /**
   * @brief Full-duplex SPI transfer of @p len bytes.
   *
   * For single-byte transfers the inline tx/rx buffers are used to avoid heap
   * allocation.  Multi-byte transfers use DMA-capable buffers pointed to by
   * @p out and @p in.
   *
   * @param out Source buffer (bytes to send); may be nullptr to send 0x00.
   * @param len Number of bytes to transfer.
   * @param in  Destination buffer for received bytes; may be nullptr to discard.
   */
  void spiTransfer(uint8_t *out, size_t len, uint8_t *in) override;

  /**
   * @brief End an SPI transaction — no-op on ESP-IDF.
   */
  void spiEndTransaction() override;

  /**
   * @brief Remove the SPI device and free SPI3_HOST.
   */
  void spiEnd() override;

private:
  /**
   * @brief Transfer a single byte over SPI using inline tx/rx buffers.
   *
   * @param b Byte to send.
   * @return Byte received during the transfer.
   */
  uint8_t spiTransferByte(uint8_t b);

  static constexpr const char *TAG = "EspHal";

  int8_t m_spiSCK;
  int8_t m_spiMISO;
  int8_t m_spiMOSI;
  spi_device_handle_t m_spi = nullptr;
};
