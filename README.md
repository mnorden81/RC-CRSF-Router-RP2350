# RC-CRSF-Router-RP2350 („CRSF-Router 8“)

CRSF-Verteiler für ELRS-Empfänger mit **6 Ports** auf einem **Waveshare RP2350-Zero**. Der große Bruder des [RC-CRSF-Router-ESP32-S3](https://github.com/mnorden81/RC-CRSF-Router-ESP32-S3): gleiche Weiterleitungsregeln, gleiche Bedienung über das Lua-Menü, aber 6 statt 2 Ports und **kein WLAN**.

> Stand: Firmware **v0.2.0** (nur seitliche Stiftleisten), kompiliert und mit Host-Tests geprüft, **noch nicht auf Hardware getestet**.

## Was er macht

Der Router hängt an einem freien CRSF-Port des Empfängers und verteilt CRSF auf bis zu 6 Geräte: Multiswitche (auch die Ziege-One-Module), Soundmodule, Sensoren oder weitere Router.

- Vom Empfänger nach unten geht alles, außer Anfragen an die eigene Adresse.
- Von unten nach oben geht alles (Telemetrie, Lua-Antworten, DEVICE_INFO).
- Zwischen den Ports geht nichts, so entstehen keine Schleifen.
- Weitergegeben werden nur vollständige Frames mit gültiger CRC, unverändert.
- Jeder Port hat eine eigene Warteschlange und sendet nicht blockierend. Ein langsamer Port bremst die anderen nicht aus. Wird es eng, fällt zuerst ein alter RC-Frame weg, Lua-Antworten bleiben erhalten.

## Status-LED (RGB-LED auf dem Board)

| LED | Bedeutung |
|---|---|
| grün langsam | RC-Signal in Ordnung |
| rot schnell | kein RC-Signal / Failsafe |
| gelb Doppelblitz | Adresskonflikt oder ein Port ließ sich nicht starten |

Sind Rot und Grün vertauscht, im Lua-Menü „LED Farbfolge“ auf **RGB** stellen.

## Lua-Menü (ELRS Lua / TBS Agent)

| ID | Eintrag | Werte |
|---|---|---|
| 1 | Version | z. B. „0.2.0 RP2350“ |
| 2 | Ordner **Ports (nach Neustart)** | |
| 3–8 | Port 1 … Port 6 | Aus, 57600, 115200, 400000, **420000**, 921600 |
| 9 | Modul Adresse | 0–15, Vorgabe **15** |
| 10 | Ordner **Telemetrie** | |
| 11 | Spannung senden | **Aus**, als RxBt, als Zellen |
| 12 | Spannung Korrektur | −0,5 … +0,5 V |
| 13 | Spannung | Anzeige |
| 14 | Temperatur senden | **Aus**, Ein |
| 15 | Chiptemperatur | Anzeige |
| 16 | Ordner **Status** | |
| 17–20 | Frames runter / hoch / verworfen / CRC-Fehler unten | Zähler |
| 21 | Adresskonflikt | ja/nein |
| 22 | Ports aktiv | je Port: Nummer = Gerät antwortet, `.` still, `-` aus, `!` Fehler |
| 23 | LED Farbfolge | **GRB**, RGB |
| 24 | Neustart | Nein, Jetzt |
| 25 | Gerätename | „CRSF-Router8“ |

Im Sender erscheint der Router als **CRSF-Router8@15** (Name@Adresse).

## Mehrere Router

Jeder Router hat eine eigene WM-Adresse (0–15 → CRSF 0xC0 + Adresse). Mehrere Router im Modell (auch gemischt mit dem S3-Router) bekommen verschiedene Adressen, z. B. 15, 14, 13. Meldet ein Gerät weiter hinten dieselbe Adresse, blinkt die LED gelb. Router lassen sich verketten; mehr als 3 Router hintereinander sind nicht sinnvoll (je ca. 0,6 ms Verzögerung).

## Anschlüsse (Kurzfassung)

| Port | TX | RX | Art |
|---|---|---|---|
| Uplink (Empfänger) | GP0 | GP1 | Hardware-UART, fest 420000 Baud |
| Port 1 | GP2 | GP3 | PIO-UART |
| Port 2 | GP4 | GP5 | Hardware-UART |
| Port 3 | GP6 | GP7 | PIO-UART |
| Port 4 | GP8 (rechts unten) | GP14 (links unten) | PIO-UART |
| Port 5 | GP15 | GP27 | PIO-UART |
| Port 6 | GP28 | GP29 | PIO-UART |
| Spannung | GP26 | | Teiler 33k/10k, optional |

Belegt werden nur die beiden seitlichen Stiftleisten (rechts GP0–GP8, links 5V, GND, 3V3, GP29, GP28, GP27, GP26, GP15, GP14). Die Lötpads an der unteren Kante (GP9–GP13) bleiben frei. Dort liegen nur 15 freie GPIOs, deshalb 6 statt 7 Ports.

TX des Routers an RX des Geräts und umgekehrt, GND gemeinsam. Details und Schaltplan: [hardware/README.md](hardware/README.md).

## Flashen

**Ohne Software:** BOOT-Taste gedrückt halten und das USB-C-Kabel einstecken. Am PC erscheint das Laufwerk **RP2350**. Die `.uf2`-Datei (aus GitHub Actions oder der Arduino IDE) darauf ziehen, der Router startet von selbst neu.

**Arduino IDE:**
1. Datei → Einstellungen → Zusätzliche Boardverwalter-URLs:
   `https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json`
2. Boardverwaltung: „Raspberry Pi Pico/RP2040/RP2350“ Version **6.3.2** installieren.
3. Board **Waveshare RP2350 Zero** wählen, `firmware/RC_CRSF_Router_RP2350/RC_CRSF_Router_RP2350.ino` öffnen.
4. Beim allerersten Mal mit gedrückter BOOT-Taste einstecken, Port „UF2 Board“ wählen, Hochladen. Danach geht es ohne Taste über den COM-Port.

Keine weiteren Bibliotheken nötig. Der serielle Monitor (115200) zeigt alle 10 s eine Statuszeile.

## Aufbau des Repos

| Ordner | Inhalt |
|---|---|
| `firmware/RC_CRSF_Router_RP2350/` | Arduino-Sketch (siehe [firmware/README.md](firmware/README.md)) |
| `tests/` | Host-Tests des Routers (`sh tests/run_tests.sh`) |
| `hardware/` | Anschlussplan |
| `docs/` | Projektbeschreibung (DOCX) |

## Herkunft

Die CRSF-Bibliothek stammt aus RC_Common (CRSF_ESP32 1.1.0), Basis ist [CRSF_ESP32 von Ziege-One](https://github.com/Ziege-One/CRSF_ESP32) (GPL-3.0).
