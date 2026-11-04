# CRSF_ESP32

Gemeinsame CRSF-Bibliothek für das ESP32-Soundmodul und den ESP32-Multiswitch (V2, V3, TXRX). Basis ist [CRSF_ESP32 von Ziege-One](https://github.com/Ziege-One/CRSF_ESP32) (GPL-3.0).

## Wo sie liegt

Seit 1.1.0 ist die Bibliothek Teil von **RC_Common** (gemeinsame Basis aller ESP32-Module, siehe `../README.md`). Jedes Projekt trägt eine identische Kopie des Ordners `RC_Common/`, darin `CRSF_ESP32/`. Eingebunden wird sie mit:

```cpp
#include "src/RC_Common/CRSF_ESP32/crsf_esp32.h"
```

**Regel:** Eine Änderung immer in allen Kopien nachziehen und `CRSF_ESP32_VERSION` in `crsf_esp32.h` hochzählen. Gleiche Versionsnummer heißt gleicher Inhalt. Die Host-Tests liegen in `RC_Common/tests/` (`sh RC_Common/tests/run_tests.sh`).

## Dateien

| Datei | Inhalt |
|---|---|
| `crsf_esp32.h/.cpp` | Klasse `CRSF`: Empfang, Parameter-Menü (Lua), Telemetrie, WM-Multiswitch-Kommandos |
| `crsf_esp32_protocol.h` | Frame-Typen, Adressen, Datentypen, Strukturen `LinkStatistics` und `DeviceInfo` |
| `crsf_platform.h` | `<Arduino.h>` auf dem ESP32, kleiner Ersatz auf dem Host für die Tests |

## Versionen

| Version | Änderung |
|---|---|
| 1.0.0 | Zusammengeführt aus Soundmodul v7.18 und Multiswitch v2.03/v3.12. Fixes laut CRSF-Spezifikation: Parser behält seinen Zustand über `read_packets()`-Aufrufe (halb empfangene Frames gingen verloren und zählten als CRC-Fehler); Link-Statistik 0x14 mit `uplinkRSSI2`, alle Felder korrekt zugeordnet; Command-CRC-Tabelle (0xBA) korrigiert; `send_command_MWPROP` mit Command-CRC; FOLDER-Eintrag ohne überzähliges Byte; ungenutztes `send_command()` entfernt. Neu: `getLinkStatistics()`, `getDeviceInfo()`, `attach_stream()`. |
| 1.1.0 | Router für die Modulkette (aus RC-Multiswitch v4.04): `init_router()` hängt bis zu zwei weitere UARTs (Port B/C) an. RC- und alle übrigen Frames von Port A gehen nach unten weiter, Extended-Frames an die eigene Adresse nicht; Antworten von unten gehen immer nach oben, nie von B nach C. Jeder Port hat eine Warteschlange (8 Frames), bei Überlauf fällt der älteste RC-Frame weg. Doppelte Moduladresse wird aus DEVICE_INFO von unten erkannt (`isDuplicateAddress()`). Neu: `getLastRcFrameMs()` für Failsafe und LED, `route_packets()`, Zähler `getFwdDown/Up/Dropped()`, FOLDER aus Array (`send_param_response_CRSF_FOLDER(id, parent, name, children, count)`). `init_crsf()` stellt die UART-Puffer vor `begin()` ein. |
