// ws2812.cpp - CRSF-Router 8, siehe ws2812.h
#include "ws2812.h"
#ifdef ARDUINO
#include <Arduino.h>
#include <hardware/gpio.h>
#include <hardware/sync.h>
#include <hardware/clocks.h>
#include <pico/platform.h>

static uint8_t wsPin = 0xFF;

void ws2812Begin(uint8_t pin) {
    wsPin = pin;
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
    gpio_put(pin, 0);
    delayMicroseconds(80);
}

// Zeiten laut Datenblatt: Bit 0 = 0,4 us high, Bit 1 = 0,8 us high,
// Periode 1,25 us (je +-150 ns). Etwas kuerzer angesetzt, weil Schleife und
// gpio_put() ein paar Takte dazugeben.
void ws2812Write(uint8_t r, uint8_t g, uint8_t b, bool rgbOrder) {
    if (wsPin == 0xFF) return;
    uint32_t hz  = clock_get_hz(clk_sys);
    uint32_t t0h = hz / 2857143;   // 0,35 us
    uint32_t t1h = hz / 1333333;   // 0,75 us
    uint32_t tb  = hz / 869565;    // 1,15 us (+ Schleife ~1,25 us)
    uint32_t v = rgbOrder ? ((uint32_t)r << 16 | (uint32_t)g << 8 | b)
                          : ((uint32_t)g << 16 | (uint32_t)r << 8 | b);
    uint32_t irq = save_and_disable_interrupts();
    for (int i = 23; i >= 0; i--) {
        uint32_t hi = (v >> i) & 1 ? t1h : t0h;
        gpio_put(wsPin, 1);
        busy_wait_at_least_cycles(hi);
        gpio_put(wsPin, 0);
        busy_wait_at_least_cycles(tb - hi);
    }
    restore_interrupts(irq);
    // > 50 us low = Uebernahme; der naechste Aufruf kommt fruehestens Millisekunden spaeter
}
#else
void ws2812Begin(uint8_t) {}
void ws2812Write(uint8_t, uint8_t, uint8_t, bool) {}
#endif
