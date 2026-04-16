# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Contexte

Passerelle LoRa sur ESP32 pour Home Assistant. Firmware ESP-IDF 6 en C++ utilisant RadioLib pour piloter un SX1276 (RFM95W). Aucun Arduino framework — uniquement ESP-IDF natif + FreeRTOS.

## Commandes principales

```bash
# Configurer la cible (une seule fois)
idf.py set-target esp32

# Compiler
idf.py build

# Flasher et monitorer (adapter le port)
idf.py -p /dev/tty.usbserial-XXXX flash monitor

# Monitorer uniquement
idf.py -p /dev/tty.usbserial-XXXX monitor

# Nettoyer
idf.py fullclean

# Menuconfig (configuration Kconfig)
idf.py menuconfig
```

L'environnement ESP-IDF doit être activé avant toute commande : `. $IDF_PATH/export.sh`

## Architecture

```
components/lora/   → composant ESP-IDF réutilisable (bibliothèque radio)
  EspHal           → implémente RadioLibHal avec les API ESP-IDF natives
  LoraRadio        → driver haut niveau SX1276 (TX, RX bloquant, RX continu)
main/
  main.cpp         → application ping-pong maître/esclave (démo)
```

### Deux couches distinctes

**`EspHal`** — pont entre RadioLib et ESP-IDF. Gère SPI3_HOST (4 MHz, polling), GPIO, interruptions IRAM, et temporisation via `esp_timer`. Ne pas toucher sauf pour des problèmes de bas niveau (SPI, ISR).

**`LoraRadio`** — driver applicatif au-dessus de RadioLib/SX1276. Deux modes de réception mutuellement exclusifs :
- `receive(pkt, timeoutMs)` — RX bloquant : le thread appelant dort sur `ulTaskNotifyTake`, réveillé par l'ISR DIO0 via `vTaskNotifyGiveFromISR`. Pas de busy-wait.
- `startReceive()` / `stopReceive()` — RX continu : tâche FreeRTOS `lora_rx` dédiée, avec callback `OnRxCb` ou queue interne (profondeur 8).

### Points d'attention

- `m_hal` doit être déclaré **avant** `m_radio` dans `LoraRadio` (ordre d'initialisation des membres).
- L'ISR `isrHandler()` est en IRAM (`IRAM_ATTR`) — ne pas y appeler de fonctions hors IRAM.
- `s_waitingTask` est un pointeur statique partagé entre ISR et tâche appelante — pas de mutex (accès atomique suffisant sur ESP32).
- `transmit()` suspend automatiquement le RX continu si actif, puis le redémarre.
- `IS_MASTER` dans `main.cpp` est un define à modifier manuellement pour compiler le firmware esclave.

## Normes de code

@.claude/rules/cpp-standards.md

## Dépendances

- **ESP-IDF 6.0** — requis (API `esp_driver_spi`, `esp_driver_gpio` introduites en v5+)
- **RadioLib 7.6.0** — via Espressif Component Registry (`jgromes/radiolib: ^7.6.0` dans `main/idf_component.yml`)

## Câblage par défaut (ESP32 VSPI)

| Signal | GPIO | Champ `Config` |
|--------|------|----------------|
| SCK    | 5    | `cfg.sck`      |
| MISO   | 19   | `cfg.miso`     |
| MOSI   | 27   | `cfg.mosi`     |
| CS     | 18   | `cfg.cs`       |
| DIO0   | 26   | `cfg.dio0`     |
| RST    | 14   | `cfg.rst`      |
| DIO1   | 35   | `cfg.dio1`     |
