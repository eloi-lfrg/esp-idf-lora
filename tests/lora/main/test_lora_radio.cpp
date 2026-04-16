/**
 * @file test_lora_radio.cpp
 * @brief Integration tests for LoraRadio — require a wired SX1276 module.
 *
 * These tests exercise the full hardware stack (SPI, GPIO, RadioLib).
 * Run them on an ESP32 with the SX1276 connected on the default pins
 * defined in LoraRadio::Config.
 *
 * Tag: [radio]
 */

#include "LoraRadio.h"
#include "unity.h"

// ── Helpers ───────────────────────────────────────────────────────────────────

/** Build a Config with the default pins and EU868 RF parameters. */
static LoraRadio::Config make_config() {
  LoraRadio::Config cfg;
  cfg.freq    = 868.0f;
  cfg.sf      = 7;
  cfg.bw      = 125.0f;
  cfg.txPower = 14;
  return cfg;
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

TEST_CASE("LoraRadio: begin() returns ESP_OK with correct wiring", "[radio]") {
  LoraRadio radio(make_config());
  TEST_ASSERT_EQUAL(ESP_OK, radio.begin());
  radio.stop();
}

TEST_CASE("LoraRadio: stop() is idempotent — safe to call multiple times", "[radio]") {
  LoraRadio radio(make_config());
  TEST_ASSERT_EQUAL(ESP_OK, radio.begin());
  radio.stop();
  radio.stop(); // second call must not crash
}

TEST_CASE("LoraRadio: begin() can be called after stop()", "[radio]") {
  LoraRadio radio(make_config());
  TEST_ASSERT_EQUAL(ESP_OK, radio.begin());
  radio.stop();
  // Re-initialise should succeed (SPI bus is freed by stop/spiEnd).
  TEST_ASSERT_EQUAL(ESP_OK, radio.begin());
  radio.stop();
}

// ── Transmit ─────────────────────────────────────────────────────────────────

TEST_CASE("LoraRadio: transmit byte buffer succeeds", "[radio]") {
  LoraRadio radio(make_config());
  TEST_ASSERT_EQUAL(ESP_OK, radio.begin());

  const uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};
  TEST_ASSERT_EQUAL(ESP_OK, radio.transmit(payload, sizeof(payload)));

  radio.stop();
}

TEST_CASE("LoraRadio: transmit C-string succeeds", "[radio]") {
  LoraRadio radio(make_config());
  TEST_ASSERT_EQUAL(ESP_OK, radio.begin());

  TEST_ASSERT_EQUAL(ESP_OK, radio.transmit("hello"));

  radio.stop();
}

// ── Receive — timeout path (no transmitter present) ──────────────────────────

TEST_CASE("LoraRadio: receive() returns ESP_ERR_TIMEOUT when no packet arrives", "[radio]") {
  LoraRadio radio(make_config());
  TEST_ASSERT_EQUAL(ESP_OK, radio.begin());

  LoraRadio::Packet pkt{};
  esp_err_t result = radio.receive(pkt, 500); // 500 ms — short enough for CI

  TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, result);
  radio.stop();
}

TEST_CASE("LoraRadio: receive() does not modify packet on timeout", "[radio]") {
  LoraRadio radio(make_config());
  TEST_ASSERT_EQUAL(ESP_OK, radio.begin());

  LoraRadio::Packet pkt{};
  pkt.len = 0xFF; // sentinel
  radio.receive(pkt, 200);

  // len must not have been updated to a valid value by a ghost packet
  TEST_ASSERT_EQUAL_UINT8(0xFF, pkt.len);

  radio.stop();
}

// ── Continuous receive ────────────────────────────────────────────────────────

TEST_CASE("LoraRadio: startReceive/stopReceive cycle succeeds", "[radio]") {
  LoraRadio radio(make_config());
  TEST_ASSERT_EQUAL(ESP_OK, radio.begin());

  TEST_ASSERT_EQUAL(ESP_OK, radio.startReceive());
  vTaskDelay(pdMS_TO_TICKS(200)); // let the RX task spin briefly
  TEST_ASSERT_EQUAL(ESP_OK, radio.stopReceive());

  radio.stop();
}

TEST_CASE("LoraRadio: startReceive() is idempotent when already running", "[radio]") {
  LoraRadio radio(make_config());
  TEST_ASSERT_EQUAL(ESP_OK, radio.begin());

  TEST_ASSERT_EQUAL(ESP_OK, radio.startReceive());
  TEST_ASSERT_EQUAL(ESP_OK, radio.startReceive()); // must return OK, not spawn a second task
  TEST_ASSERT_EQUAL(ESP_OK, radio.stopReceive());

  radio.stop();
}

TEST_CASE("LoraRadio: stopReceive() is idempotent when not running", "[radio]") {
  LoraRadio radio(make_config());
  TEST_ASSERT_EQUAL(ESP_OK, radio.begin());

  // Never started — must not crash or block
  TEST_ASSERT_EQUAL(ESP_OK, radio.stopReceive());

  radio.stop();
}

// ── Runtime parameter updates ─────────────────────────────────────────────────

TEST_CASE("LoraRadio: setFreq() accepts a valid EU868 frequency", "[radio]") {
  LoraRadio radio(make_config());
  TEST_ASSERT_EQUAL(ESP_OK, radio.begin());

  TEST_ASSERT_EQUAL(ESP_OK, radio.setFreq(869.525f));

  radio.stop();
}

TEST_CASE("LoraRadio: setSF() accepts SF9", "[radio]") {
  LoraRadio radio(make_config());
  TEST_ASSERT_EQUAL(ESP_OK, radio.begin());

  TEST_ASSERT_EQUAL(ESP_OK, radio.setSF(9));

  radio.stop();
}

TEST_CASE("LoraRadio: setTxPower() accepts 17 dBm", "[radio]") {
  LoraRadio radio(make_config());
  TEST_ASSERT_EQUAL(ESP_OK, radio.begin());

  TEST_ASSERT_EQUAL(ESP_OK, radio.setTxPower(17));

  radio.stop();
}

// ── RSSI / SNR accessors (valid only after reception) ────────────────────────

TEST_CASE("LoraRadio: getRSSI() returns a float without crashing", "[radio]") {
  LoraRadio radio(make_config());
  TEST_ASSERT_EQUAL(ESP_OK, radio.begin());

  // No packet has been received — value is implementation-defined but must
  // not crash or return NaN.
  float rssi = radio.getRSSI();
  TEST_ASSERT_FALSE(isnanf(rssi));

  radio.stop();
}
