#ifndef PINS_H
#define PINS_H

/*
 * pins.h  -  CRSF-Router 8 (Waveshare RP2350-Zero)
 *
 * Zwei Varianten, gleicher Sketch. Genau EINE Zeile unten aktiv lassen:
 *
 *   VARIANTE_ROUTER  "Router": Eingang + 7 Ports, alle Pins (GP0-GP15).
 *                    Braucht auch die Loetpads an der unteren Kante
 *                    (GP9-GP13): dort eine 5er-Stiftleiste einloeten.
 *                    Platine: hardware/Router/
 *   VARIANTE_TEST    "Test": Eingang + 6 Ports, nur die beiden seitlichen
 *                    Stiftleisten, Pads GP9-GP13 bleiben frei. Gut fuer
 *                    Steckbrett und Probeaufbau. Platine: hardware/Test/
 *
 * Belegung RP2350-Zero (Oberseite, USB oben):
 *   rechte Leiste:  GP0 GP1 GP2 GP3 GP4 GP5 GP6 GP7 GP8
 *   untere Kante:   GP9 GP10 GP11 GP12 GP13 (nur Loetpads)
 *   linke Leiste:   5V GND 3V3 GP29 GP28 GP27 GP26 GP15 GP14
 * Mehr als 8 UARTs gehen nicht: 2 Hardware-UARTs + 6 PIO-UARTs
 * (je 2 der 12 PIO-State-Machines).
 * GPIO16 ist die RGB-LED (WS2812, auf dem Board), GPIO26 misst die Spannung.
 * Alle Pins vertragen nur 3,3 V. CRSF-Empfaenger und -Module arbeiten mit
 * 3,3-V-Pegeln, das passt direkt.
 */

#if !defined(VARIANTE_ROUTER) && !defined(VARIANTE_TEST)
#define VARIANTE_ROUTER      // Vollversion: Eingang + 7 Ports
//#define VARIANTE_TEST      // Testversion: Eingang + 6 Ports, nur seitliche Leisten
#endif
#if defined(VARIANTE_ROUTER) && defined(VARIANTE_TEST)
#error "Nur eine Variante waehlen: VARIANTE_ROUTER oder VARIANTE_TEST"
#endif

// ── Uplink zum Empfaenger: Hardware-UART0 (Serial1), fest 420000 Baud ─
#define PIN_UPLINK_TX  0
#define PIN_UPLINK_RX  1

// ── Port 2: Hardware-UART1 (Serial2) ─────────────────────────────────
#define PIN_P2_TX      4
#define PIN_P2_RX      5

// ── Port 1 und 3-7: PIO-UARTs (SerialPIO, beliebige Pins) ───────────
#define PIN_P1_TX      2
#define PIN_P1_RX      3
#define PIN_P3_TX      6
#define PIN_P3_RX      7

#if defined(VARIANTE_ROUTER)
#define VARIANTE_NAME  "Router"
#define PORT_COUNT     7
#define PIN_P4_TX      8
#define PIN_P4_RX      9
#define PIN_P5_TX      10
#define PIN_P5_RX      11
#define PIN_P6_TX      12
#define PIN_P6_RX      13
#define PIN_P7_TX      14
#define PIN_P7_RX      15
// GP27, GP28, GP29: frei (Reserve)
#else
#define VARIANTE_NAME  "Test"
#define PORT_COUNT     6
// Port 4: TX rechts unten (GP8), RX links unten (GP14)
#define PIN_P4_TX      8
#define PIN_P4_RX      14
// Port 5: GP15 und GP27 (dazwischen GP26 = Spannung)
#define PIN_P5_TX      15
#define PIN_P5_RX      27
#define PIN_P6_TX      28
#define PIN_P6_RX      29
#endif

// ── Status-LED: RGB-LED (WS2812) des RP2350-Zero ─────────────────────
// Die PIO-Einheiten gehoeren den UARTs, die LED wird deshalb per
// Software angesteuert (ca. 30 us je Farbwechsel).
#define PIN_STATUS_LED 16

// ── Spannung: Spannungsteiler an ADC0 ────────────────────────────────
//   +V (BEC/Empfaenger) ── 33 kOhm ──┬── GPIO26
//                                    └── 10 kOhm ── GND
// Teiler 4,3 : 1 -> bis ca. 14 V messbar. Ohne Teiler "Spannung senden"
// auf Aus lassen (offener Pin liefert Zufallswerte).
#define PIN_VSENSE       26
#define VSENSE_R_TOP     33.0f
#define VSENSE_R_BOTTOM  10.0f

#endif // PINS_H
