// ws2812.cpp - CRSF-Router 8, siehe ws2812.h
#include "ws2812.h"
#ifdef ARDUINO
#include <Arduino.h>
#include <hardware/gpio.h>
#include <hardware/sync.h>
#include <hardware/clocks.h>
#include <pico/platform.h>
#include <hardware/structs/sio.h>
#include <hardware/timer.h>

static uint8_t wsPin = 0xFF;
static void wsCycInit();   // v0.4.6, siehe unten

void ws2812Begin(uint8_t pin) {
    wsPin = pin;
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
    gpio_put(pin, 0);
    wsCycInit();
    delayMicroseconds(80);
}

// Zeiten laut Datenblatt: Bit 0 = 0,4 us high, Bit 1 = 0,8 us high,
// Periode 1,25 us (je +-150 ns).
//
// v0.4.6: Zeitmessung mit dem Zyklenzaehler des Cortex-M33 (DWT CYCCNT)
// gegen feste Zeitpunkte. v0.4.5 hatte die Schleife zwar im RAM, rief aber
// busy_wait_at_least_cycles() auf - das kann je nach Optimierung eine
// normale Funktion im Flash sein, Cache-Fehlgriffe verlaengerten dann
// weiter die High-Pulse ("aus" erschien gelb, Farben wechselten). Jetzt
// keine Aufrufe mehr: Pin direkt ueber SIO, warten bis CYCCNT den
// Zeitpunkt erreicht. Verspaetungen verkuerzen hoechstens die Low-Zeit.
#define WS_DEMCR    (*(volatile uint32_t*)0xE000EDFCu)
#define WS_DWT_CTRL (*(volatile uint32_t*)0xE0001000u)
#define WS_CYCCNT   (*(volatile uint32_t*)0xE0001004u)

static bool wsCycOk = false;
// v0.4.7: Ausgabe-Variante zum Testen (Lua "LED Variante", nicht gespeichert)
//   0 Standard: Reset 80 us, 0-Bit 0,35 us, 1-Bit 0,70 us
//   1 langer Reset 300 us (WS2812B-V5 braucht > 280 us)
//   2 kurzes 0-Bit 0,25 us, 1-Bit 0,65 us
//   3 wie 2 + langer Reset + erstes Bit nur halb so lang
static uint8_t wsVariant = 0;
void ws2812SetVariant(uint8_t v) { wsVariant = v < 4 ? v : 0; }

static void wsCycInit() {
    WS_DEMCR |= (1u << 24);        // TRCENA
    WS_CYCCNT = 0;
    WS_DWT_CTRL |= 1u;             // CYCCNTENA
    uint32_t a = WS_CYCCNT;
    for (volatile int i = 0; i < 100; i++) {}
    wsCycOk = (WS_CYCCNT != a);
}

static void __not_in_flash_func(wsSendBits)(uint32_t v, uint32_t mask,
                                            uint32_t t0h, uint32_t t1h, uint32_t tb,
                                            bool firstShort) {
    uint32_t t = WS_CYCCNT;
    for (int i = 23; i >= 0; i--) {
        uint32_t hi = (v >> i) & 1 ? t1h : t0h;
        if (firstShort && i == 23) hi /= 2;
        sio_hw->gpio_set = mask;
        while ((uint32_t)(WS_CYCCNT - t) < hi) {}
        sio_hw->gpio_clr = mask;
        t += tb;
        while ((uint32_t)(WS_CYCCNT - t) >= 0x80000000u) {}   // bis t erreicht
    }
}

// Ersatz, falls der Zyklenzaehler nicht laeuft (wie v0.4.5)
static void __not_in_flash_func(wsSendBitsLoop)(uint32_t v, uint32_t mask,
                                                uint32_t t0h, uint32_t t1h, uint32_t tb) {
    for (int i = 23; i >= 0; i--) {
        uint32_t hi = (v >> i) & 1 ? t1h : t0h;
        sio_hw->gpio_set = mask;
        for (volatile uint32_t k = hi / 4; k; k--) {}
        sio_hw->gpio_clr = mask;
        for (volatile uint32_t k = (tb - hi) / 4; k; k--) {}
    }
}

void ws2812Write(uint8_t r, uint8_t g, uint8_t b, bool rgbOrder) {
    if (wsPin == 0xFF) return;
    uint32_t hz  = clock_get_hz(clk_sys);
    bool     shortBits = (wsVariant == 2 || wsVariant == 3);
    uint32_t t0h = shortBits ? hz / 4000000 : hz / 2857143;   // 0,25 / 0,35 us
    uint32_t t1h = shortBits ? hz / 1538462 : hz / 1428571;   // 0,65 / 0,70 us
    uint32_t tb  = hz / 800000;    // 1,25 us
    uint32_t v = rgbOrder ? ((uint32_t)r << 16 | (uint32_t)g << 8 | b)
                          : ((uint32_t)g << 16 | (uint32_t)r << 8 | b);
    uint32_t mask = 1u << wsPin;
    sio_hw->gpio_clr = mask;
    busy_wait_us_32((wsVariant == 1 || wsVariant == 3) ? 300 : 80);   // Reset (low) vor jedem Bild
    uint32_t irq = save_and_disable_interrupts();
    if (wsCycOk) wsSendBits(v, mask, t0h, t1h, tb, wsVariant == 3);
    else         wsSendBitsLoop(v, mask, t0h, t1h, tb);
    restore_interrupts(irq);
}

bool ws2812CycleCounter() { return wsCycOk; }
#else
void ws2812Begin(uint8_t) {}
void ws2812Write(uint8_t, uint8_t, uint8_t, bool) {}
bool ws2812CycleCounter() { return false; }
void ws2812SetVariant(uint8_t) {}
#endif
