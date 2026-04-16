#include "LoraRadio.h"

volatile bool LoraRadio::s_rxFlag = false;
TaskHandle_t LoraRadio::s_waitingTask = nullptr;

// ─────────────────────────────────────────────────────────────────────────────
IRAM_ATTR void LoraRadio::isrHandler() {
  s_rxFlag = true;
  if (s_waitingTask) {
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(s_waitingTask, &higherPriorityTaskWoken);
    portYIELD_FROM_ISR(higherPriorityTaskWoken);
  }
}

// ─── Constructor / Destructor
// ─────────────────────────────────────────────────
LoraRadio::LoraRadio(const Config &cfg)
    : m_hal(cfg.sck, cfg.miso, cfg.mosi),
      m_radio(new Module(&m_hal, cfg.cs, cfg.dio0, cfg.rst, cfg.dio1)),
      m_cfg(cfg) {}

LoraRadio::~LoraRadio() { stop(); }

// ─── begin ───────────────────────────────────────────────────────────────────
esp_err_t LoraRadio::begin() {
  int16_t state = m_radio.begin(m_cfg.freq, m_cfg.bw, m_cfg.sf, m_cfg.cr,
                                m_cfg.syncWord, m_cfg.txPower, m_cfg.preamble);
  if (state != RADIOLIB_ERR_NONE) {
    ESP_LOGE(TAG, "begin() failed: %d", state);
    return ESP_FAIL;
  }

  if (applyConfig() != ESP_OK)
    return ESP_FAIL;

  // Attach ISR on DIO0 (TX done / RX packet received)
  m_radio.setDio0Action(isrHandler, m_hal.GpioInterruptRising);

  m_rxQueue = xQueueCreate(8, sizeof(Packet));
  if (!m_rxQueue) {
    ESP_LOGE(TAG, "Failed to create RX queue");
    return ESP_ERR_NO_MEM;
  }

  ESP_LOGI(TAG, "Init OK (%.1fMHz SF%d BW%.0fkHz CR4/%d +%ddBm)", m_cfg.freq,
           m_cfg.sf, m_cfg.bw, m_cfg.cr, m_cfg.txPower);
  return ESP_OK;
}

// ─── applyConfig ─────────────────────────────────────────────────────────────
esp_err_t LoraRadio::applyConfig() {
  int16_t s;

  s = m_radio.setCRC(static_cast<uint8_t>(m_cfg.crc ? 2 : 0));
  if (s != RADIOLIB_ERR_NONE) {
    ESP_LOGE(TAG, "setCRC failed: %d", s);
    return ESP_FAIL;
  }

  s = m_radio.invertIQ(m_cfg.invertIQ);
  if (s != RADIOLIB_ERR_NONE) {
    ESP_LOGE(TAG, "invertIQ failed: %d", s);
    return ESP_FAIL;
  }

  return ESP_OK;
}

// ─── stop ────────────────────────────────────────────────────────────────────
void LoraRadio::stop() {
  stopReceive();
  m_radio.sleep();
  if (m_rxQueue) {
    vQueueDelete(m_rxQueue);
    m_rxQueue = nullptr;
  }
}

// ─── transmit ────────────────────────────────────────────────────────────────
esp_err_t LoraRadio::transmit(const uint8_t *data, size_t len) {
  // Pause continuous RX if active
  bool wasReceiving = m_rxRunning;
  if (wasReceiving)
    stopReceive();

  int16_t state = m_radio.transmit(const_cast<uint8_t *>(data), len);

  if (wasReceiving)
    startReceive();

  if (state != RADIOLIB_ERR_NONE) {
    ESP_LOGE(TAG, "transmit failed: %d", state);
    if (m_onErr)
      m_onErr(state);
    return ESP_FAIL;
  }

  ESP_LOGD(TAG, "TX OK (%d bytes, ToA=%lums)", len,
           m_radio.getTimeOnAir(len) / 1000);
  if (m_onTx)
    m_onTx();
  return ESP_OK;
}

esp_err_t LoraRadio::transmit(const char *str) {
  return transmit(reinterpret_cast<const uint8_t *>(str), strlen(str));
}

// ─── startReceive ────────────────────────────────────────────────────────────
esp_err_t LoraRadio::startReceive() {
  if (m_rxRunning)
    return ESP_OK;

  int16_t state = m_radio.startReceive();
  if (state != RADIOLIB_ERR_NONE) {
    ESP_LOGE(TAG, "startReceive failed: %d", state);
    return ESP_FAIL;
  }

  m_rxRunning = true;
  s_rxFlag = false;

  xTaskCreate(rxTaskEntry, "lora_rx", 4096, this, 5, &m_rxTask);

  ESP_LOGI(TAG, "Continuous RX started");
  return ESP_OK;
}

// ─── stopReceive ─────────────────────────────────────────────────────────────
esp_err_t LoraRadio::stopReceive() {
  if (!m_rxRunning)
    return ESP_OK;

  m_rxRunning = false;

  // Wake the task so it can exit cleanly
  if (m_rxTask) {
    xTaskNotifyGive(m_rxTask);
    vTaskDelay(pdMS_TO_TICKS(50));
    m_rxTask = nullptr;
  }

  m_radio.standby();
  ESP_LOGI(TAG, "RX stopped");
  return ESP_OK;
}

// ─── receive (blocking) ──────────────────────────────────────────────────────
esp_err_t LoraRadio::receive(Packet &pkt, uint32_t timeoutMs) {
  // Clear stale flag and register this task before arming the radio,
  // so the ISR can wake us even if a packet arrives immediately.
  s_rxFlag = false;
  s_waitingTask = xTaskGetCurrentTaskHandle();

  int16_t state = m_radio.startReceive();
  if (state != RADIOLIB_ERR_NONE) {
    s_waitingTask = nullptr;
    return ESP_FAIL;
  }

  // Block until ISR fires or timeout expires — no busy-wait.
  TickType_t ticks =
      (timeoutMs == portMAX_DELAY) ? portMAX_DELAY : pdMS_TO_TICKS(timeoutMs);
  uint32_t notified = ulTaskNotifyTake(pdTRUE, ticks);

  s_waitingTask = nullptr;
  m_radio.standby();

  if (notified) {
    s_rxFlag = false;
    if (readPacket(pkt))
      return ESP_OK;
    return ESP_FAIL;
  }

  ESP_LOGW(TAG, "receive() timed out (%lums)", timeoutMs);
  return ESP_ERR_TIMEOUT;
}

// ─── RX task ─────────────────────────────────────────────────────────────────
void LoraRadio::rxTaskEntry(void *arg) {
  static_cast<LoraRadio *>(arg)->rxTask();
  vTaskDelete(nullptr);
}

void LoraRadio::rxTask() {
  ESP_LOGD(TAG, "rxTask started");

  while (m_rxRunning) {
    // Efficient wait: ISR notification or 100ms timeout
    ulTaskNotifyTake(pdFALSE, pdMS_TO_TICKS(100));

    if (!m_rxRunning)
      break;

    if (s_rxFlag) {
      s_rxFlag = false;

      Packet pkt{};
      if (readPacket(pkt)) {
        if (m_onRx) {
          m_onRx(pkt);
        } else {
          // No callback: enqueue for blocking receive()
          xQueueSend(m_rxQueue, &pkt, 0);
        }
      }

      // Re-arm RX mode (radio exits RX after each packet)
      m_radio.startReceive();
    }
  }

  ESP_LOGD(TAG, "rxTask stopped");
}

// ─── readPacket ──────────────────────────────────────────────────────────────
bool LoraRadio::readPacket(Packet &pkt) {
  pkt.len = m_radio.getPacketLength();
  if (pkt.len == 0 || pkt.len > LORA_MAX_PACKET) {
    ESP_LOGW(TAG, "Invalid packet length: %d", pkt.len);
    return false;
  }

  int16_t state = m_radio.readData(pkt.data, pkt.len);
  if (state != RADIOLIB_ERR_NONE) {
    ESP_LOGW(TAG, "readData failed: %d", state);
    if (m_onErr)
      m_onErr(state);
    return false;
  }

  pkt.rssi = m_radio.getRSSI();
  pkt.snr = m_radio.getSNR();
  pkt.freqErr = m_radio.getFrequencyError();

  ESP_LOGD(TAG, "RX %d bytes | RSSI=%.1fdBm SNR=%.1fdB FreqErr=%ldHz", pkt.len,
           pkt.rssi, pkt.snr, pkt.freqErr);
  return true;
}

// ─── Dynamic setters ─────────────────────────────────────────────────────────
esp_err_t LoraRadio::setFreq(float freq) {
  int16_t s = m_radio.setFrequency(freq);
  if (s != RADIOLIB_ERR_NONE) {
    ESP_LOGE(TAG, "setFreq failed: %d", s);
    return ESP_FAIL;
  }
  m_cfg.freq = freq;
  return ESP_OK;
}

esp_err_t LoraRadio::setSF(uint8_t sf) {
  int16_t s = m_radio.setSpreadingFactor(sf);
  if (s != RADIOLIB_ERR_NONE) {
    ESP_LOGE(TAG, "setSF failed: %d", s);
    return ESP_FAIL;
  }
  m_cfg.sf = sf;
  return ESP_OK;
}

esp_err_t LoraRadio::setTxPower(int8_t dbm) {
  int16_t s = m_radio.setOutputPower(dbm);
  if (s != RADIOLIB_ERR_NONE) {
    ESP_LOGE(TAG, "setTxPower failed: %d", s);
    return ESP_FAIL;
  }
  m_cfg.txPower = dbm;
  return ESP_OK;
}
