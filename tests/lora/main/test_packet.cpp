#include "LoraRadio.h"
#include "unity.h"
#include <string.h>

// ── LORA_MAX_PACKET constant ──────────────────────────────────────────────────

TEST_CASE("Packet: LORA_MAX_PACKET equals 255 (SX1276 hardware limit)", "[packet]") {
  TEST_ASSERT_EQUAL(255, LORA_MAX_PACKET);
}

// ── Packet struct layout ──────────────────────────────────────────────────────

TEST_CASE("Packet: data buffer size matches LORA_MAX_PACKET", "[packet]") {
  LoraRadio::Packet pkt{};
  TEST_ASSERT_EQUAL(LORA_MAX_PACKET, sizeof(pkt.data));
}

TEST_CASE("Packet: zero-initialised len is 0", "[packet]") {
  LoraRadio::Packet pkt{};
  TEST_ASSERT_EQUAL(0u, pkt.len);
}

TEST_CASE("Packet: zero-initialised RSSI is 0.0", "[packet]") {
  LoraRadio::Packet pkt{};
  TEST_ASSERT_EQUAL_FLOAT(0.0f, pkt.rssi);
}

TEST_CASE("Packet: zero-initialised SNR is 0.0", "[packet]") {
  LoraRadio::Packet pkt{};
  TEST_ASSERT_EQUAL_FLOAT(0.0f, pkt.snr);
}

TEST_CASE("Packet: zero-initialised freqErr is 0", "[packet]") {
  LoraRadio::Packet pkt{};
  TEST_ASSERT_EQUAL_INT32(0, pkt.freqErr);
}

TEST_CASE("Packet: zero-initialised data buffer is all zeroes", "[packet]") {
  LoraRadio::Packet pkt{};
  uint8_t expected[LORA_MAX_PACKET] = {};
  TEST_ASSERT_EQUAL_MEMORY(expected, pkt.data, LORA_MAX_PACKET);
}

// ── Packet field writes ───────────────────────────────────────────────────────

TEST_CASE("Packet: data buffer accepts a full 255-byte payload", "[packet]") {
  LoraRadio::Packet pkt{};
  for (int i = 0; i < LORA_MAX_PACKET; i++) {
    pkt.data[i] = static_cast<uint8_t>(i & 0xFF);
  }
  pkt.len = LORA_MAX_PACKET;

  TEST_ASSERT_EQUAL(LORA_MAX_PACKET, pkt.len);
  TEST_ASSERT_EQUAL_UINT8(0x00, pkt.data[0]);
  TEST_ASSERT_EQUAL_UINT8(0xFE, pkt.data[254]);
}

TEST_CASE("Packet: RSSI field accepts negative dBm values", "[packet]") {
  LoraRadio::Packet pkt{};
  pkt.rssi = -112.5f;
  TEST_ASSERT_EQUAL_FLOAT(-112.5f, pkt.rssi);
}

TEST_CASE("Packet: SNR field accepts negative dB values", "[packet]") {
  LoraRadio::Packet pkt{};
  pkt.snr = -20.0f;
  TEST_ASSERT_EQUAL_FLOAT(-20.0f, pkt.snr);
}

TEST_CASE("Packet: freqErr field accepts signed Hz offsets", "[packet]") {
  LoraRadio::Packet pkt{};
  pkt.freqErr = -3500;
  TEST_ASSERT_EQUAL_INT32(-3500, pkt.freqErr);
}
