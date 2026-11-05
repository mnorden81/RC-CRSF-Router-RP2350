# Firmware CRSF-Router 8 (RP2350)

Arduino-Sketch für den Waveshare RP2350-Zero, Core **arduino-pico 6.3.2** (Earle Philhower), keine weiteren Bibliotheken.

## Dateien

| Datei | Inhalt |
|---|---|
| `RC_CRSF_Router_RP2350.ino` | Start der Ports, Lua-Menü, LED, Telemetrie, Hauptschleife |
| `multi_router.*` | Router mit Uplink + bis zu 7 Ports: Frame-Parser, Warteschlangen, nicht blockierendes Senden, Anbindung der CRSF-Klasse |
| `router_config.*` | Einstellungen im Flash (EEPROM-Emulation, mit Prüfsumme) |
| `ws2812.*` | RGB-LED ohne PIO (alle PIO-Einheiten sind von den UARTs belegt) |
| `pins.h` | Pinbelegung und Auswahl der Version (`VARIANTE_ROUTER` oder `VARIANTE_TEST`) |
| `src/RC_Common/` | Auszug aus RC_Common 1.2.0 (siehe unten) |

## Wie die Teile zusammenspielen

```
Empfänger ──UART── MultiRouter ──┬── Port 2 (Hardware-UART)
                       │         ├── Port 1, 3…7 (PIO-UART; Test: 3…6)
                       │
                  local() Stream
                       │
                   CRSF-Klasse (RC_Common): Ping, DEVICE_INFO, Lua, Telemetrie
```

Die CRSF-Klasse hängt nicht direkt am UART. Sie liest jeden Frame vom Empfänger aus `router.local()`, und was sie sendet, reiht der Router als ganzen Frame in die Warteschlange zum Empfänger ein. So mischen sich eigene Antworten nie mit Antworten der Geräte an den Ports.

## RC_Common-Auszug

Der RP2350 kann die ESP32-Teile von RC_Common (WLAN, WebUI, NVS, OTA) nicht nutzen. Übernommen sind nur:

- `CRSF_ESP32/` (CRSF-Bibliothek 1.1.0)
- `lua_menu.*` (Lua-Menü als Tabelle)
- `status_led.*` (Blinkmuster)

Gegenüber RC_Common 1.2.0 gibt es drei kleine Anpassungen, alle mit „RP2350-Kopie“ markiert:

1. `crsf_esp32.h/.cpp`: `init_crsf()` und `init_router()` nur auf dem ESP32 (`#if defined(ARDUINO) && defined(ESP32)`).
2. `status_led.h/.cpp`: Klasse `StatusLed` (nutzt `neopixelWrite`) nur auf dem ESP32.
3. `crsf_platform.h`: `#include <initializer_list>`, arduino-pico bindet es nicht selbst ein.

Für die ESP32-Projekte ändert sich dadurch nichts. Vorschlag: die drei Punkte bei der nächsten RC_Common-Version (1.3.0) in alle Kopien übernehmen, dann ist auch diese Kopie wieder identisch.

## Grenzen von v0.3.0

- Alle Ports nur CRSF. S.Port und GPS an den Ports sind noch nicht eingebaut.
- Kein WLAN, keine WebUI, Bedienung nur über Lua. Updates per USB.
- Speichern einer Einstellung blockiert den Chip einige Millisekunden (Flash). Dabei können einzelne Frames verloren gehen; gespeichert wird nur nach Änderungen im Lua-Menü.
- Nicht auf Hardware getestet: PIO-Belegung Router (12 von 12 State-Machines), Timing der Software-LED, Durchsatz bei 7 aktiven Ports. Die Test-Version (10 von 12) wurde am 05.11.2026 vom Sender erkannt.
