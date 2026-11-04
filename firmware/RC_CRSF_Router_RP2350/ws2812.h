#ifndef WS2812_H
#define WS2812_H

/*
 * ws2812.h  -  CRSF-Router 8: eine WS2812-LED ohne PIO
 *
 * Alle 12 PIO-State-Machines des RP2350 sind von den 6 PIO-UARTs belegt.
 * Die LED wird deshalb per Software mit festen Taktzyklen angesteuert;
 * dabei sind die Interrupts ca. 30 us gesperrt. Die UARTs verlieren nichts
 * (Hardware-FIFO 32 Byte, PIO-FIFO 8 Byte = ca. 190 us bei 420000 Baud).
 * Nur bei Farbwechseln aufrufen, nicht in jedem Loop.
 */

#include <stdint.h>

void ws2812Begin(uint8_t pin);
// rgbOrder: false = GRB (Standard), true = RGB (manche Boards)
void ws2812Write(uint8_t r, uint8_t g, uint8_t b, bool rgbOrder = false);

#endif
