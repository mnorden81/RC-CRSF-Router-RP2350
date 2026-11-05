#ifndef PINS_H
#define PINS_H

/*
 * pins.h  -  CRSF-Router 8 (Waveshare RP2350-Zero)
 *
 * 7 UARTs = 1 Uplink zum Empfaenger + 6 Ports:
 *   - Uplink und Port 2 laufen auf den beiden Hardware-UARTs
 *   - Port 1 und 3-6 laufen als PIO-UART (je 2 der 12 PIO-State-Machines)
 * v0.2.0: nur die beiden seitlichen Stiftleisten werden benutzt, die
 * Loetpads an der unteren Kante (GP9-13) bleiben frei.
 *   rechte Leiste: GP0 GP1 GP2 GP3 GP4 GP5 GP6 GP7 GP8
 *   linke Leiste:  5V GND 3V3 GP29 GP28 GP27 GP26 GP15 GP14
 * Das sind 15 freie GPIOs: 14 fuer 7 UARTs, GP26 misst die Spannung.
 * GPIO16 ist die RGB-LED (WS2812, auf dem Board).
 * Alle Pins vertragen nur 3,3 V. CRSF-Empfaenger und -Module arbeiten mit
 * 3,3-V-Pegeln, das passt direkt.
 */

// ── Uplink zum Empfaenger: Hardware-UART0 (Serial1), fest 420000 Baud ─
#define PIN_UPLINK_TX  0
#define PIN_UPLINK_RX  1

// ── Port 2: Hardware-UART1 (Serial2) ─────────────────────────────────
#define PIN_P2_TX      4
#define PIN_P2_RX      5

// ── Port 1, 3-6: PIO-UARTs (SerialPIO, beliebige Pins) ───────────────
#define PIN_P1_TX      2
#define PIN_P1_RX      3
#define PIN_P3_TX      6
#define PIN_P3_RX      7
// Port 4: TX rechts unten (GP8), RX links unten (GP14)
#define PIN_P4_TX      8
#define PIN_P4_RX      14
// Port 5: GP15 und GP27 (dazwischen GP26 = Spannung)
#define PIN_P5_TX      15
#define PIN_P5_RX      27
#define PIN_P6_TX      28
#define PIN_P6_RX      29

// ── Status-LED: RGB-LED (WS2812) des RP2350-Zero ─────────────────────
// Alle PIO-Einheiten sind von den UARTs belegt, die LED wird deshalb per
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
