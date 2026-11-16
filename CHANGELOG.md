# Versionsgeschichte RC-CRSF-Router-RP2350 („CRSF-Router 8“)

Die Version wird bei jeder Änderung erhöht, auch die CRSF „Parameter Version Number“ im DEVICE_INFO. Neueste Version oben.

## v0.4.8
- Normale Firmware: LED-Test und LED-Variante wieder entfernt (Lua wie v0.4.6, 65/59 Felder), Statuszeile wieder kurz, Startgrund nur in der Startzeile.
- LED: Standard-Schema, RGB, Timing per Zyklenzähler.
- Befund: die LED des Test-Boards ist vermutlich defekt (Rot immer an, Blau tot), ein neues Board folgt.

## v0.4.7
- Zur Fehlersuche: Lua „LED-Test“ (feste Farbe) und „LED Variante“ (4 Ausgabe-Varianten, nicht gespeichert).

## v0.4.6
- WS2812 mit Zyklenzähler (DWT CYCCNT) statt Warteschleife, keine Flash-Aufrufe während der Bits.
- Statuszeile zeigt Laufzeit, Grund des letzten Starts und LED-Zustand (Fehlersuche: Router startete alle 1–2 min neu). Seitdem läuft er stabil.

## v0.4.5
- Status-LED-Fix: WS2812-Bitschleife im RAM statt im Flash. LED-Farbfolge Vorgabe RGB (RP2350-Zero), alte Einstellungen werden einmalig auf RGB umgestellt.

## v0.4.4 (RC_Common 1.4.0)
- Status-LED einheitlich mit allen Modulen: kein Signal = rot Aufblitzen (vorher rot schnell), rot schnell = Frames, aber Failsafe / kein RC-Link.

## v0.4.3 (RC_Common 1.3.0)
- Lange Lua-Auswahlfelder (z. B. „Spannung Korrektur“) kommen Chunk für Chunk, vorher fehlten sie im ELRS-Lua.
- „Spannung Korrektur“ ausgeblendet, solange „Spannung senden“ auf Aus steht.

## v0.4.2
- Adresskonflikt deutlicher: LED blau statt gelb (sah auf dem RP2350-Zero grün aus), „Adresskonflikt“ beim Port in der Geräteliste, „Geräte suchen“ setzt den Konflikt zurück.

## v0.4.1
- Ideen aus dem ELRS-Empfänger 4.x: Adress-Lernen mit gezielter Zustellung nach unten, Telemetrie nach oben ersetzt wartende alte Werte, Lua-Frames nach oben mit Vorrang. Lua-Menü und Einstellungen unverändert.
- Auf dem Steckbrett getestet (14.11.2026).

## v0.4.0 („Version 2“)
- Filter je Port (RC-Kanäle, LinkStats), Telemetrie-Bremse je Port, Geräteliste per Ping.
- Lua-Menü neu geordnet (neue IDs), Einstellungen aus v0.3.0 werden übernommen.
- Halbduplex je Port wieder entfernt.

## v0.3.0
- Zwei Varianten in `pins.h`: „Router“ (7 Ports, alle Pins wie v0.1) und „Test“ (6 Ports, nur seitliche Stiftleisten).

## v0.2.0
- Nur seitliche Stiftleisten: Uplink + 6 Ports, Pins Port 4–6 neu, Lua-IDs ab Moduladresse um 1 nach vorn. Erster Hardwaretest am 05.11.2026 (Sender erkennt den Router).

## v0.1.0
- Erste Firmware: Uplink + 7 Ports, Lua-Menü, Status-LED, Spannung und Temperatur als Telemetrie, Einstellungen im Flash.
