#include "esp_chip_info.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <inttypes.h>
#include <stdio.h>

#include "LoraRadio.h"

#define IS_MASTER true // false sur le nœud esclave

extern "C" void app_main() {
  LoraRadio::Config cfg;
  cfg.freq = 868.0f;
  cfg.sf = 9; // SF plus élevé = plus de portée
  cfg.bw = 125.0f;
  cfg.txPower = 17;

  LoraRadio lora(cfg);
  if (lora.begin() != ESP_OK)
    return;

  LoraRadio::Packet pkt{};
  unsigned long seq = 0;

  while (true) {
#if IS_MASTER
    // Master : TX puis attente réponse
    char msg[16];
    snprintf(msg, sizeof(msg), "ping %lu", seq++);
    lora.transmit(reinterpret_cast<const uint8_t *>(msg), strlen(msg));

    if (lora.receive(pkt, 3000) == ESP_OK) {
      ESP_LOGI("PING", "Pong reçu: RSSI=%.1f SNR=%.1f", pkt.rssi, pkt.snr);
    } else {
      ESP_LOGW("PING", "Pas de pong");
    }
    vTaskDelay(pdMS_TO_TICKS(2000));

#else
    // Slave : attente ping puis réponse immédiate
    if (lora.receive(pkt, portMAX_DELAY) == ESP_OK) {
      ESP_LOGI("PONG", "Ping reçu: RSSI=%.1f", pkt.rssi);
      vTaskDelay(pdMS_TO_TICKS(100)); // délai turnaround
      lora.transmit(reinterpret_cast<const uint8_t *>("pong"), 4);
    }
#endif
  }
}
