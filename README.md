# ha-lora-gateway

Passerelle LoRa sur ESP32 pour Home Assistant, basée sur ESP-IDF 6 et RadioLib.  
Le projet implémente un nœud ping-pong bidirectionnel (maître/esclave) et fournit une couche radio réutilisable (`LoraRadio`) qui peut être intégrée dans n'importe quelle application ESP-IDF.

---

## Matériel requis

| Composant | Valeur |
|---|---|
| MCU | ESP32 (testé sur ESP32-WROVER) |
| Module radio | SX1276 (breakout ou module RFM95W) |
| Fréquence | 868 MHz (EU) — configurable |
| Interface | SPI (SPI3_HOST / VSPI) |

### Câblage par défaut

| Signal | GPIO ESP32 |
|---|---|
| SCK | 5 |
| MISO | 19 |
| MOSI | 27 |
| CS | 18 |
| DIO0 | 26 |
| RST | 14 |
| DIO1 | 35 |

Tous les pins sont configurables dans `LoraRadio::Config`.

---

## Prérequis logiciels

- [ESP-IDF 6.0](https://docs.espressif.com/projects/esp-idf/en/v6.0/esp32/get-started/index.html) installé et activé (`idf.py` dans le PATH)
- CMake ≥ 3.22
- VSCode + extension [ESP-IDF](https://marketplace.visualstudio.com/items?itemName=espressif.esp-idf-tools) (optionnel)

---

## Structure du projet

```
ha-lora-gateway/
├── components/
│   └── lora/
│       ├── EspHal.h / EspHal.cpp     # HAL ESP-IDF pour RadioLib
│       ├── LoraRadio.h / LoraRadio.cpp  # Driver LoRa haut niveau
│       └── CMakeLists.txt
├── main/
│   ├── main.cpp                      # Application ping-pong maître/esclave
│   └── idf_component.yml
├── CMakeLists.txt
└── dependencies.lock
```

---

## Compilation et flash

```bash
# Configurer la cible
idf.py set-target esp32

# Compiler
idf.py build

# Flasher (remplacer /dev/ttyUSB0 par le bon port)
idf.py -p /dev/ttyUSB0 flash monitor
```

---

## Utilisation — application ping-pong

Le `main.cpp` implémente un ping-pong radio simple contrôlé par le define `IS_MASTER` :

```cpp
#define IS_MASTER true   // true = maître, false = esclave
```

**Maître** : envoie un ping numéroté toutes les 2 secondes, attend une réponse pendant 3 secondes.  
**Esclave** : attend indéfiniment un ping, répond immédiatement avec `"pong"`.

Compiler deux firmwares (un avec `IS_MASTER true`, l'autre `false`) et les flasher sur deux cartes.

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

### Émission

```cpp
// Buffer binaire
radio.transmit(buffer, len);

// Chaîne de caractères
radio.transmit("hello");
```

### Réception bloquante (mode ping-pong)

```cpp
LoraRadio::Packet pkt{};

// Avec timeout
if (radio.receive(pkt, 3000) == ESP_OK) {
    // pkt.data, pkt.len, pkt.rssi, pkt.snr, pkt.freqErr
}

// Sans timeout (portMAX_DELAY)
radio.receive(pkt, portMAX_DELAY);
```

### Réception continue avec callback

```cpp
radio.setOnRx([](const LoraRadio::Packet &pkt) {
    ESP_LOGI("RX", "Reçu %d octets, RSSI=%.1f", pkt.len, pkt.rssi);
});

radio.startReceive();
// ...
radio.stopReceive();
```

### Modifications à la volée

```cpp
radio.setFreq(915.0f);   // Changer la fréquence
radio.setSF(12);          // Changer le spreading factor
radio.setTxPower(20);     // Changer la puissance TX
```

---

## Architecture interne

### `EspHal`

Implémente l'interface `RadioLibHal` avec les API natives ESP-IDF :

| Fonctionnalité | API ESP-IDF |
|---|---|
| GPIO | `gpio_config`, `gpio_set_level`, `gpio_get_level` |
| Interruptions | `gpio_isr_handler_add` (service IRAM) |
| SPI | `spi_bus_initialize`, `spi_device_polling_transmit` (SPI3_HOST, 4 MHz) |
| Temporisation | `esp_timer_get_time`, `vTaskDelay` |

### `LoraRadio`

Deux modèles de réception :

- **Continu** (`startReceive`) : tâche FreeRTOS `lora_rx` qui dort sur `ulTaskNotifyTake` et est réveillée par l'ISR DIO0.
- **Bloquant** (`receive`) : le thread appelant dort sur `ulTaskNotifyTake` — pas de busy-wait, la tâche IDLE n'est jamais affamée.

L'ISR est placée en IRAM (`IRAM_ATTR`) et utilise `vTaskNotifyGiveFromISR` + `portYIELD_FROM_ISR` pour un réveil immédiat à priorité correcte.

---

## Dépendances

| Dépendance | Version | Source |
|---|---|---|
| ESP-IDF | 6.0.0 | Espressif |
| [RadioLib](https://github.com/jgromes/RadioLib) | 7.6.0 | Espressif Component Registry |

---

## Tests

Les tests sont dans `tests/lora/`. Deux niveaux existent : des tests purement Python (sans matériel) et des tests d'intégration Unity qui s'exécutent sur l'ESP32.

### Prérequis

```bash
cd tests/lora
source venv/bin/activate   # venv déjà provisionné
# ou, si absent :
python3 -m venv venv && source venv/bin/activate && pip install -r requirements.txt
```

### Tests sans matériel (défaut)

Ces tests parsent les headers C++ et vérifient les constantes, les valeurs par défaut de `Config`, la structure de `Packet` et la surface d'API — aucun ESP32 requis.

```bash
cd tests/lora
pytest -v -m "not hardware"
```

Lancer un seul test :

```bash
pytest -v -m "not hardware" -k "test_default_frequency"
```

### Tests d'intégration hardware (ESP32 + SX1276)

Ces tests flashent automatiquement le firmware Unity sur l'ESP32, lisent la sortie série et rapportent chaque `TEST_CASE` comme un test pytest distinct.

```bash
cd tests/lora
pytest -v   # détecte le port série automatiquement
```

Le port est lu depuis `.vscode/settings.json` (`idf.port`) ou auto-détecté. Pour le spécifier manuellement, modifier `_serial_port()` dans `test_lora.py` ou définir `idf.port` dans les settings VSCode.

> **Note :** `IDF_PATH` et le chemin Python ESP-IDF sont codés en dur dans `test_lora.py` — adapter si l'installation ESP-IDF diffère de `/Users/eloi/.espressif/`.

### Exécution manuelle des tests Unity

Pour lancer les tests sans pytest (moniteur série direct) :

```bash
idf.py -C tests/lora flash monitor
# Dans le moniteur : taper '!' puis Entrée pour exécuter tous les tests
```

### Ajouter un test

- **Test sans hardware** : ajouter une méthode dans la classe appropriée de `tests/lora/test_lora.py`.
- **Test d'intégration** : ajouter un `TEST_CASE("…", "[radio]")` dans `tests/lora/main/test_lora_radio.cpp` ; il sera automatiquement découvert et exécuté par pytest.

---

## Licence

Ce projet est distribué sous licence MIT.
