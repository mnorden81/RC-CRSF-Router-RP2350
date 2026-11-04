// status_led.cpp - RC_Common, siehe status_led.h
#include "status_led.h"
#ifdef ARDUINO
#include <Arduino.h>
#endif

uint8_t statusLedMode(const StatusLedInput& in, unsigned long now) {
    if (in.configError) return SLED_DOUBLE;
    if (in.crsf) {
        if (!in.busOk) return SLED_OFF;
        bool noRc   = in.lastRcFrameMs == 0 || now - in.lastRcFrameMs > LED_RC_LINK_TIMEOUT_MS;
        bool lqNull = in.linkStatsMs != 0 && now - in.linkStatsMs <= LED_LINKSTATS_MAX_AGE &&
                      in.uplinkLQ == 0;
        return (noRc || lqNull) ? SLED_FAST : SLED_SLOW;
    }
    // SBUS: BUS_OK ist im Failsafe false, deshalb eigene Frame-Zeit
    if (in.lastSbusFrameMs == 0 || now - in.lastSbusFrameMs > LED_SBUS_TIMEOUT_MS) return SLED_OFF;
    return in.sbusFailsafe ? SLED_FAST : SLED_SLOW;
}

bool statusLedOn(uint8_t mode, unsigned long t) {
    switch (mode) {
        case SLED_SLOW:   return (t % 1000) < 500;              // 500/500 ms
        case SLED_FAST:   return (t % 200) < 100;               // 100/100 ms
        case SLED_DOUBLE: { unsigned long p = t % 1000;         // 2x 100 ms, Pause
                           return p < 100 || (p >= 250 && p < 350); }
        default:         return false;
    }
}

#if defined(ARDUINO) && defined(ESP32)   // RP2350-Kopie: nur ESP32
void StatusLed::begin(int pin, bool ws2812, uint8_t brightness, bool activeHigh) {
    pin_ = pin; ws_ = ws2812; bright_ = brightness; activeHigh_ = activeHigh;
    if (pin_ < 0) return;
    if (!ws_) pinMode(pin_, OUTPUT);
    write(false);
}

void StatusLed::write(bool on) {
    if (pin_ < 0) return;
    if (ws_) neopixelWrite(pin_, 0, on ? bright_ : 0, 0);   // gruen
    else     digitalWrite(pin_, (on == activeHigh_) ? HIGH : LOW);
}

void StatusLed::update(uint8_t mode, unsigned long now) {
    if (pin_ < 0) return;
    if (mode != mode_) { mode_ = mode; start_ = now; }
    bool on = statusLedOn(mode_, now - start_);
    if (on != on_) { on_ = on; write(on); }
}
#endif
