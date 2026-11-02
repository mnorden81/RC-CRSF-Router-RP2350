# Hardware CRSF-Router 8

Basis ist ein **Waveshare RP2350-Zero** (ca. 23,5 × 18 mm, USB-C, RGB-LED an GP16, BOOT- und RESET-Taste). Es gibt noch keine eigene Platine, für den ersten Test reicht eine Lochrasterplatine mit Steckern.

## Pinbelegung

```
                    ___(_____)___
              5V  1 |   *USB C*   | 23 GP0  Uplink TX  → RX Empfänger
             GND  2 |             | 22 GP1  Uplink RX  ← TX Empfänger
            3V3   3 |             | 21 GP2  Port 1 TX
   (frei)  GP29   4 |             | 20 GP3  Port 1 RX
   (frei)  GP28   5 |             | 19 GP4  Port 2 TX
   (frei)  GP27   6 |             | 18 GP5  Port 2 RX
 Spannung  GP26   7 |             | 17 GP6  Port 3 TX
 Port 7 RX GP15   8 |             | 16 GP7  Port 3 RX
 Port 7 TX GP14   9 |__|_|_|_|_|__| 15 GP8  Port 4 TX
                     10 11 12 13 14
```

Untere Kante (Pin 10–14, von links): GP13 Port 6 RX, GP12 Port 6 TX, GP11 Port 5 RX, GP10 Port 5 TX, GP9 Port 4 RX.

## Verdrahtung je Port

| Router | Gerät |
|---|---|
| TX | RX (CRSF-Eingang) |
| RX | TX (Telemetrie-Ausgang) |
| GND | GND |
| 5 V | 5 V (nur wenn das Gerät darüber versorgt wird) |

Alle Pins sind **3,3 V**. ELRS-Empfänger und die ESP32-Module arbeiten auch mit 3,3 V, das passt direkt. Geräte mit 5-V-Signalen brauchen einen Pegelwandler.

**Ziege-One-Multiswitche ohne eigenen Weiterleitungs-Port** hängen einfach jeweils an einem eigenen Port. Genau dafür sind die 7 Ports gedacht.

**S.Port** ist in v0.1.0 noch nicht vorgesehen (alle Ports sprechen CRSF).

## Versorgung

Der RP2350-Zero wird über den 5-V-Pin vom Empfänger/BEC versorgt (eigener Regler auf dem Board, ca. 30 mA). Die 5 V der Ports sind einfach durchverbunden. Bei mehreren Servos/Modulen auf eine ausreichend starke Versorgung achten.

## Spannungsmessung (optional)

```
+V (BEC/Empfänger) ── 33 kΩ ──┬── GP26
                              ├── 100 nF ── GND   (optional, beruhigt den Messwert)
                              └── 10 kΩ ── GND
```

Teiler 4,3 : 1, messbar bis ca. 14 V. Ohne Teiler im Lua-Menü „Spannung senden“ auf **Aus** lassen.
