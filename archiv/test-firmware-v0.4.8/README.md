# Archiv: Test-Firmware v0.4.8 (6 Ports, Steckbrett)

Ab v0.4.9 gibt es nur noch die Router-Firmware mit Eingang + 7 Ports. Die frühere Variante „Test“ liegt hier unverändert im Stand v0.4.8 (in `pins.h` ist `VARIANTE_TEST` schon eingestellt). Sie wird nicht mehr gepflegt und nicht von GitHub Actions gebaut.

| | |
|---|---|
| Ports | Eingang + 6 Ports, nur die beiden seitlichen Stiftleisten (Pads GP9–GP13 bleiben frei) |
| Im Sender | `CRSF-Router8 Test@15`, 59 Lua-Felder |
| Pins | Uplink GP0/1, Port 1 GP2/3, Port 2 GP4/5, Port 3 GP6/7, Port 4 GP8/14, Port 5 GP15/27, Port 6 GP28/29 (jeweils TX/RX) |
| Platine | keine, nur für das Steckbrett |

Bauen: `RC_CRSF_Router_RP2350/RC_CRSF_Router_RP2350.ino` in der Arduino IDE öffnen, Core arduino-pico 6.3.2, Board **Waveshare RP2350 Zero**, hochladen wie im Haupt-README beschrieben.
