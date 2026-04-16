"""
Tests du composant lora.

- Tests [config] et [packet] : purement Python, pas de hardware requis.
  Ils parsent les fichiers d'en-tête C++ et vérifient les constantes et
  valeurs par défaut directement dans le code source.

- Tests [hardware] : nécessitent un ESP32 + SX1276 câblé.
  Lancer manuellement via :
      idf.py -C . flash monitor
      # Dans le moniteur : taper '!' pour lancer tous les tests Unity

Lancer uniquement les tests sans hardware :
    pytest -v -m "not hardware"

Lancer tous les tests (dont hardware) :
    pytest -v
"""

import json
import os
import re
import subprocess
import time
import pytest
import serial
import serial.tools.list_ports
from pathlib import Path

# ── Helpers ───────────────────────────────────────────────────────────────────

COMPONENT = Path(__file__).parents[2] / "components" / "lora"


def header(name: str) -> str:
    return (COMPONENT / name).read_text(encoding="utf-8")


def find_define(content: str, name: str) -> str:
    m = re.search(rf"#define\s+{name}\s+(\S+)", content)
    assert m, f"#define {name} not found in header"
    return m.group(1)


def find_field_default(content: str, field: str) -> str:
    """Extract the default value of a struct field using a word-boundary match."""
    m = re.search(rf"\b{field}\b\s*=\s*([^;/\n]+)", content)
    assert m, f"field '{field}' with default not found in header"
    return m.group(1).strip()


# ── Config — RF parameters ────────────────────────────────────────────────────

class TestConfigRF:
    def setup_method(self):
        self.h = header("LoraRadio.h")

    def test_default_frequency_is_868_mhz(self):
        assert "868.0f" in find_field_default(self.h, "freq")

    def test_default_bandwidth_is_125_khz(self):
        assert "125.0f" in find_field_default(self.h, "bw")

    def test_default_sf_is_7(self):
        assert "7" in find_field_default(self.h, "sf")

    def test_default_cr_is_5(self):
        assert "5" in find_field_default(self.h, "cr")

    def test_default_tx_power_is_14_dbm(self):
        assert "14" in find_field_default(self.h, "txPower")

    def test_default_sync_word_is_0x12_private(self):
        assert "0x12" in find_field_default(self.h, "syncWord")

    def test_default_preamble_is_8_symbols(self):
        assert "8" in find_field_default(self.h, "preamble")

    def test_crc_enabled_by_default(self):
        assert "true" in find_field_default(self.h, "crc")

    def test_invert_iq_disabled_by_default(self):
        assert "false" in find_field_default(self.h, "invertIQ")


# ── Config — pin assignments ──────────────────────────────────────────────────

class TestConfigPins:
    def setup_method(self):
        self.h = header("LoraRadio.h")

    def test_default_sck_is_gpio_5(self):
        assert "5" in find_field_default(self.h, "sck")

    def test_default_miso_is_gpio_19(self):
        assert "19" in find_field_default(self.h, "miso")

    def test_default_mosi_is_gpio_27(self):
        assert "27" in find_field_default(self.h, "mosi")

    def test_default_cs_is_gpio_18(self):
        assert "18" in find_field_default(self.h, "cs")

    def test_default_dio0_is_gpio_26(self):
        assert "26" in find_field_default(self.h, "dio0")

    def test_default_rst_is_gpio_14(self):
        assert "14" in find_field_default(self.h, "rst")

    def test_default_dio1_is_gpio_35(self):
        assert "35" in find_field_default(self.h, "dio1")


# ── Packet struct ─────────────────────────────────────────────────────────────

class TestPacket:
    def setup_method(self):
        self.h = header("LoraRadio.h")

    def test_lora_max_packet_is_255(self):
        val = find_define(self.h, "LORA_MAX_PACKET")
        assert int(val) == 255

    def test_data_field_uses_lora_max_packet(self):
        assert "data[LORA_MAX_PACKET]" in self.h

    def test_packet_has_rssi_field(self):
        assert "float    rssi" in self.h or "float rssi" in self.h

    def test_packet_has_snr_field(self):
        assert "float    snr" in self.h or "float snr" in self.h

    def test_packet_has_freq_err_field(self):
        assert "freqErr" in self.h

    def test_packet_has_len_field(self):
        assert "size_t" in self.h and "len" in self.h


# ── API surface ───────────────────────────────────────────────────────────────

class TestApiSurface:
    def setup_method(self):
        self.h = header("LoraRadio.h")

    def test_begin_declared(self):
        assert "esp_err_t begin()" in self.h

    def test_stop_declared(self):
        assert "void stop()" in self.h

    def test_transmit_binary_declared(self):
        assert "transmit(const uint8_t" in self.h

    def test_transmit_string_declared(self):
        assert "transmit(const char" in self.h

    def test_start_receive_declared(self):
        assert "startReceive()" in self.h

    def test_stop_receive_declared(self):
        assert "stopReceive()" in self.h

    def test_receive_blocking_declared(self):
        assert "receive(Packet" in self.h

    def test_set_freq_declared(self):
        assert "setFreq(" in self.h

    def test_set_sf_declared(self):
        assert "setSF(" in self.h

    def test_set_tx_power_declared(self):
        assert "setTxPower(" in self.h

    def test_get_rssi_declared(self):
        assert "getRSSI()" in self.h

    def test_get_snr_declared(self):
        assert "getSNR()" in self.h

    def test_callbacks_declared(self):
        assert "setOnRx(" in self.h
        assert "setOnTx(" in self.h
        assert "setOnErr(" in self.h


# ── EspHal API surface ────────────────────────────────────────────────────────

class TestEspHal:
    def setup_method(self):
        self.h = header("EspHal.h")

    def test_esphal_extends_radiolibhal(self):
        assert "public RadioLibHal" in self.h

    def test_spi_begin_declared(self):
        assert "spiBegin()" in self.h

    def test_spi_transfer_declared(self):
        assert "spiTransfer(" in self.h

    def test_spi_end_declared(self):
        assert "spiEnd()" in self.h

    def test_gpio_methods_declared(self):
        assert "pinMode(" in self.h
        assert "digitalWrite(" in self.h
        assert "digitalRead(" in self.h

    def test_interrupt_methods_declared(self):
        assert "attachInterrupt(" in self.h
        assert "detachInterrupt(" in self.h

    def test_timing_methods_declared(self):
        assert "millis()" in self.h
        assert "micros()" in self.h
        assert "delay(" in self.h


# ── Hardware runner ───────────────────────────────────────────────────────────

TEST_APP_DIR = Path(__file__).parent
VSCODE_SETTINGS = Path(__file__).parents[2] / ".vscode" / "settings.json"
BAUD_RATE = 115200


def _serial_port() -> str:
    """Return the configured serial port from VSCode settings or auto-detect."""
    if VSCODE_SETTINGS.exists():
        settings = json.loads(VSCODE_SETTINGS.read_text())
        port = settings.get("idf.port", "detect")
        if port not in ("detect", ""):
            return port
    ports = list(serial.tools.list_ports.comports())
    assert ports, "No serial port detected — connect the ESP32"
    return ports[0].device


IDF_PATH = Path(os.environ.get("IDF_PATH", "/Users/eloi/.espressif/v6.0/esp-idf"))
_IDF_PYTHON = Path("/Users/eloi/.espressif/tools/python/v6.0/venv/bin/python")


def _idf_run(args: list[str], timeout: int = 300) -> subprocess.CompletedProcess:  # type: ignore[type-arg]
    """Run idf.py with the correct ESP-IDF Python, bypassing any active venv."""
    env = os.environ.copy()
    env.pop("VIRTUAL_ENV", None)
    env.pop("PYTHONHOME", None)
    env["IDF_PATH"] = str(IDF_PATH)
    return subprocess.run(
        [str(_IDF_PYTHON), str(IDF_PATH / "tools" / "idf.py")] + args,
        capture_output=True,
        text=True,
        timeout=timeout,
        env=env,
    )


def _build_and_flash(port: str) -> None:
    """Build the Unity test firmware and flash it to the device."""
    for action in ("build", "flash"):
        result = _idf_run(["-C", str(TEST_APP_DIR), "-p", port, action])
        assert result.returncode == 0, (
            f"idf.py {action} failed:\n{result.stdout}\n{result.stderr}"
        )


def _run_unity(port: str, timeout: int = 90) -> str:
    """
    Open the serial port, wait for the Unity menu prompt, then send '!\\n' to
    run all tests and collect output until the summary line appears.

    unity_gets() flushes the RX buffer before reading, so the trigger must be
    sent AFTER the menu prompt is printed (i.e., after the flush window).
    """
    with serial.Serial(port, BAUD_RATE, timeout=1) as ser:
        # Wait up to 15 s for the Unity menu prompt
        menu_deadline = time.time() + 15
        while time.time() < menu_deadline:
            raw = ser.readline()
            line = raw.decode("utf-8", errors="replace").rstrip()
            if "Enter test for running" in line:
                break

        # Give unity_gets() time to finish flushing the RX buffer
        time.sleep(0.3)
        ser.write(b"!\n")  # '!' = run all tests; '\n' terminates the line

        lines: list[str] = []
        deadline = time.time() + timeout
        while time.time() < deadline:
            raw = ser.readline()
            line = raw.decode("utf-8", errors="replace").rstrip()
            if line:
                lines.append(line)
            # Unity prints "N Tests N Failures N Ignored" at the end
            if re.search(r"\d+ Tests \d+ Failures \d+ Ignored", line):
                time.sleep(0.3)  # drain remaining bytes
                while ser.in_waiting:
                    extra = ser.readline().decode("utf-8", errors="replace").rstrip()
                    if extra:
                        lines.append(extra)
                break

    return "\n".join(lines)


def _parse_unity(output: str) -> list[tuple[str, bool, str]]:
    """
    Parse Unity output into a list of (test_name, passed, message).

    Unity line formats:
        file.cpp:LINE:test_name:PASS
        file.cpp:LINE:test_name:FAIL:reason
    """
    results = []
    for line in output.splitlines():
        m = re.match(r"[^:]+\.cpp:\d+:(.+?):(PASS|FAIL)(:.+)?$", line)
        if m:
            name = m.group(1).strip()
            passed = m.group(2) == "PASS"
            reason = (m.group(3) or "").lstrip(":")
            results.append((name, passed, reason))
    return results


def _collect_test_names() -> list[str]:
    """Extract TEST_CASE names from the C++ source files at collection time."""
    names = []
    for src in TEST_APP_DIR.glob("main/test_*.cpp"):
        for m in re.finditer(r'TEST_CASE\s*\(\s*"([^"]+)"', src.read_text()):
            names.append(m.group(1))
    return names or ["hardware_suite"]


# ── Hardware integration (require physical SX1276) ────────────────────────────

@pytest.fixture(scope="module")
def unity_results() -> dict[str, tuple[bool, str]]:
    """
    Flash the Unity test firmware once per module and return all results as
    {test_name: (passed, failure_message)}.
    """
    port = _serial_port()
    _build_and_flash(port)
    output = _run_unity(port)

    parsed = _parse_unity(output)
    assert parsed, f"No Unity results found. Raw output:\n{output}"

    return {name: (passed, msg) for name, passed, msg in parsed}


@pytest.mark.hardware
@pytest.mark.parametrize("test_name", _collect_test_names())
def test_hardware(test_name: str, unity_results: dict) -> None:
    """Run a single Unity TEST_CASE on the ESP32 and report its result."""
    if test_name not in unity_results:
        pytest.skip(f"'{test_name}' not found in Unity output (device may not have run it)")
    passed, reason = unity_results[test_name]
    assert passed, f"FAIL: {reason}"
