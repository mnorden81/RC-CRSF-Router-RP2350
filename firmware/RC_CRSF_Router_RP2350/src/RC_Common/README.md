# RC_Common (Auszug für den RP2350)

Unveränderter Auszug aus **RC_Common 1.4.0** (CRSF_ESP32 1.1.1). Nur die Teile, die ohne ESP32 laufen:

| Datei | Inhalt |
|---|---|
| `CRSF_ESP32/` | CRSF-Bibliothek (eigene README) |
| `lua_menu.*` | Lua-Parametermenü als Tabelle, Felder per `view()` umbenennbar, sperrbar und ausblendbar |
| `status_led.*` | Blinkmuster der Status-LED |

Die früheren RP2350-Anpassungen (`init_crsf()`/`init_router()` und `StatusLed` nur bei `ESP32`, `<initializer_list>` in `crsf_platform.h`) sind seit 1.3.0 im Original. Die Dateien sind damit identisch mit den Kopien in den ESP32-Projekten (gleiche Prüfsummen wie dort in `MANIFEST.sha256`).
