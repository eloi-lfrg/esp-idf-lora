#include "LoraRadio.h"
#include "unity.h"

// ── Default RF parameters ─────────────────────────────────────────────────────

TEST_CASE("Config: default frequency is 868 MHz", "[config]") {
  LoraRadio::Config cfg;
  TEST_ASSERT_EQUAL_FLOAT(868.0f, cfg.freq);
}

TEST_CASE("Config: default bandwidth is 125 kHz", "[config]") {
  LoraRadio::Config cfg;
  TEST_ASSERT_EQUAL_FLOAT(125.0f, cfg.bw);
}

TEST_CASE("Config: default spreading factor is 7", "[config]") {
  LoraRadio::Config cfg;
  TEST_ASSERT_EQUAL_UINT8(7, cfg.sf);
}

TEST_CASE("Config: default coding rate denominator is 5 (4/5)", "[config]") {
  LoraRadio::Config cfg;
  TEST_ASSERT_EQUAL_UINT8(5, cfg.cr);
}

TEST_CASE("Config: default TX power is 14 dBm", "[config]") {
  LoraRadio::Config cfg;
  TEST_ASSERT_EQUAL_INT8(14, cfg.txPower);
}

TEST_CASE("Config: default sync word is 0x12 (private network)", "[config]") {
  LoraRadio::Config cfg;
  TEST_ASSERT_EQUAL_HEX8(0x12, cfg.syncWord);
}

TEST_CASE("Config: default preamble length is 8 symbols", "[config]") {
  LoraRadio::Config cfg;
  TEST_ASSERT_EQUAL_UINT16(8, cfg.preamble);
}

TEST_CASE("Config: CRC is enabled by default", "[config]") {
  LoraRadio::Config cfg;
  TEST_ASSERT_TRUE(cfg.crc);
}

TEST_CASE("Config: IQ inversion is disabled by default", "[config]") {
  LoraRadio::Config cfg;
  TEST_ASSERT_FALSE(cfg.invertIQ);
}

// ── Default pin assignments ───────────────────────────────────────────────────

TEST_CASE("Config: default SPI SCK pin is GPIO 5", "[config][pins]") {
  LoraRadio::Config cfg;
  TEST_ASSERT_EQUAL_INT8(5, cfg.sck);
}

TEST_CASE("Config: default SPI MISO pin is GPIO 19", "[config][pins]") {
  LoraRadio::Config cfg;
  TEST_ASSERT_EQUAL_INT8(19, cfg.miso);
}

TEST_CASE("Config: default SPI MOSI pin is GPIO 27", "[config][pins]") {
  LoraRadio::Config cfg;
  TEST_ASSERT_EQUAL_INT8(27, cfg.mosi);
}

TEST_CASE("Config: default CS pin is GPIO 18", "[config][pins]") {
  LoraRadio::Config cfg;
  TEST_ASSERT_EQUAL_INT8(18, cfg.cs);
}

TEST_CASE("Config: default DIO0 pin is GPIO 26", "[config][pins]") {
  LoraRadio::Config cfg;
  TEST_ASSERT_EQUAL_INT8(26, cfg.dio0);
}

TEST_CASE("Config: default RST pin is GPIO 14", "[config][pins]") {
  LoraRadio::Config cfg;
  TEST_ASSERT_EQUAL_INT8(14, cfg.rst);
}

TEST_CASE("Config: default DIO1 pin is GPIO 35", "[config][pins]") {
  LoraRadio::Config cfg;
  TEST_ASSERT_EQUAL_INT8(35, cfg.dio1);
}

// ── Config mutation ───────────────────────────────────────────────────────────

TEST_CASE("Config: frequency can be overridden", "[config]") {
  LoraRadio::Config cfg;
  cfg.freq = 915.0f;
  TEST_ASSERT_EQUAL_FLOAT(915.0f, cfg.freq);
}

TEST_CASE("Config: spreading factor can be overridden", "[config]") {
  LoraRadio::Config cfg;
  cfg.sf = 12;
  TEST_ASSERT_EQUAL_UINT8(12, cfg.sf);
}

TEST_CASE("Config: SF valid range boundary — minimum is 6", "[config]") {
  LoraRadio::Config cfg;
  cfg.sf = 6;
  TEST_ASSERT_EQUAL_UINT8(6, cfg.sf);
}

TEST_CASE("Config: SF valid range boundary — maximum is 12", "[config]") {
  LoraRadio::Config cfg;
  cfg.sf = 12;
  TEST_ASSERT_EQUAL_UINT8(12, cfg.sf);
}

TEST_CASE("Config: public sync word can be set to 0x34 (TTN)", "[config]") {
  LoraRadio::Config cfg;
  cfg.syncWord = 0x34;
  TEST_ASSERT_EQUAL_HEX8(0x34, cfg.syncWord);
}
