# RC_Common (Auszug für den RP2350)

Auszug aus **RC_Common 1.2.0** (CRSF_ESP32 1.1.0). Nur die Teile, die ohne ESP32 laufen:

| Datei | Inhalt |
|---|---|
| `CRSF_ESP32/` | CRSF-Bibliothek (eigene README) |
| `lua_menu.*` | Lua-Parametermenü als Tabelle |
| `status_led.*` | Blinkmuster der Status-LED |

Drei Anpassungen, im Code mit „RP2350-Kopie“ markiert:

1. `CRSF_ESP32/crsf_esp32.h/.cpp`: `init_crsf()`/`init_router()` nur bei `ESP32`.
2. `status_led.h/.cpp`: Klasse `StatusLed` nur bei `ESP32`.
3. `CRSF_ESP32/crsf_platform.h`: `#include <initializer_list>`.

Für die ESP32-Projekte ändert sich dadurch nichts. Bei RC_Common 1.3.0 in alle Kopien übernehmen, dann ist dieser Auszug wieder identisch mit dem Original.
