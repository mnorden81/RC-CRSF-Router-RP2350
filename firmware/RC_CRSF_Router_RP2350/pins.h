#ifndef PINS_H
#define PINS_H

/*
 * pins.h  -  CRSF-Router 8 (Waveshare RP2350-Zero)
 *
 * 8 UARTs = 1 Uplink zum Empfaenger + 7 Ports:
 *   - Uplink und Port 2 laufen auf den beiden Hardware-UARTs
 *   - Port 1 und 3-7 laufen als PIO-UART (je 2 der 12 PIO-State-Machines)
 * Die Ports sind entlang der Stiftleiste durchnummeriert (Port 1 = GP2/3).
 * Alle 16 Pins liegen auf GPIO0-15 (Stiftleisten + untere Kante).
 * GPIO16 ist die RGB-LED (WS2812), GPIO26 misst die Spannung.
 * Alle Pins vertragen nur 3,3 V. CRSF-Empfaenger und -Module arbeiten mit
 * 3,3-V-Pegeln, das passt direkt.
 */

// ── Uplink zum Empfaenger: Hardware-UART0 (Serial1), fest 420000 Baud ─
#define PIN_UPLINK_TX  0
#define PIN_UPLINK_RX  1

// ── Port 2: Hardware-UART1 (Serial2) ─────────────────────────────────
#define PIN_P2_TX      4
#define PIN_P2_RX      5

// ── Port 1, 3-7: PIO-UARTs (SerialPIO) ───────────────────────────────
#define PIN_P1_TX      2
#define PIN_P1_RX      3
#define PIN_P3_TX      6
#define PIN_P3_RX      7
#define PIN_P4_TX      8
#define PIN_P4_RX      9
#define PIN_P5_TX      10
#define PIN_P5_RX      11
#define PIN_P6_TX      12
#define PIN_P6_RX      13
#define PIN_P7_TX      14
#define PIN_P7_RX      15

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
