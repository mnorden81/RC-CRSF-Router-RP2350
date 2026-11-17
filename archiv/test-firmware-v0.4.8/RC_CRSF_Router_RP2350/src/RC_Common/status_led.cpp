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

bool statusLedOn(uint8_t pattern, unsigned long t) {
    switch (pattern) {
        case SLED_SLOW:   return (t % 1000) < 500;              // 500/500 ms
        case SLED_FAST:   return (t % 200) < 100;               // 100/100 ms
        case SLED_DOUBLE: { unsigned long p = t % 1000;         // 2x 100 ms, Pause
                           return p < 100 || (p >= 250 && p < 350); }
        case SLED_BLIP:   return (t % 1000) < 100;              // 100/900 ms
        default:         return false;
    }
}

StatusLedLook statusLedLook(uint8_t mode, bool dupAddress, bool wifiOn) {
    StatusLedLook l = {SLED_SLOW, 0, 40, 0};                    // gruen langsam
    switch (mode) {
        case SLED_DOUBLE:
            l.pattern = SLED_DOUBLE;
            if (dupAddress) { l.r = 0;  l.g = 0;  l.b = 60; }    // blau
            else            { l.r = 40; l.g = 28; l.b = 0;  }    // gelb
            return l;                                            // Fehler: ohne WLAN-Farbe
        case SLED_FAST: l.pattern = SLED_FAST; l.r = 40; l.g = 0; break;   // rot schnell
        case SLED_OFF:  l.pattern = SLED_BLIP; l.r = 40; l.g = 0; break;   // rot Aufblitzen
        default: break;
    }
    if (wifiOn) { l.r = 24; l.g = 0; l.b = 40; }                 // violett
    return l;
}

bool statusLedBootOn(unsigned long t) { return (t / LED_BOOT_BLINK_MS) % 2 == 0; }

const char* statusLedText(uint8_t mode, bool dupAddress, bool wifiOn, bool rgb) {
    if (!rgb) {
        switch (mode) {
            case SLED_SLOW:   return "langsam: RC-Signal OK";
            case SLED_FAST:   return "schnell: Failsafe / kein RC-Link";
            case SLED_DOUBLE: return dupAddress ? "Doppelblitz: Adresskonflikt" : "Doppelblitz: Konfigurationsfehler";
            default:          return "aus: kein Signal";
        }
    }
    switch (mode) {
        case SLED_DOUBLE: return dupAddress ? "blau Doppelblitz: Adresskonflikt"
                                            : "gelb Doppelblitz: Konfigurationsfehler";
        case SLED_SLOW:   return wifiOn ? "violett langsam: RC-Signal OK, WLAN an"
                                        : "gruen langsam: RC-Signal OK";
        case SLED_FAST:   return wifiOn ? "violett schnell: Failsafe / kein RC-Link, WLAN an"
                                        : "rot schnell: Failsafe / kein RC-Link";
        default:          return wifiOn ? "violett Aufblitzen: kein Signal, WLAN an"
                                        : "rot Aufblitzen: kein Signal";
    }
}

#if defined(ARDUINO) && defined(ESP32)   // nur ESP32 (RP2350 nutzt den Rest)
// Farbwerte gelten fuer Helligkeit 40, auf die eingestellte umrechnen
static uint8_t scale(uint8_t v, uint8_t bright) {
    uint16_t x = (uint16_t)v * bright / 40;
    return x > 255 ? 255 : (uint8_t)x;
}

void StatusLed::begin(int pin, bool ws2812, uint8_t brightness, bool activeHigh) {
    pin_ = pin; ws_ = ws2812; bright_ = brightness; activeHigh_ = activeHigh;
    if (pin_ < 0) return;
    if (!ws_) pinMode(pin_, OUTPUT);
    writeRgb(0, 0, 0);
}

void StatusLed::setRgbOrder(bool rgb) {
    if (rgb == rgbOrder_) return;
    rgbOrder_ = rgb;
    last_ = 0xFFFFFFFF;   // beim naechsten update() neu schreiben
}

void StatusLed::writeRgb(uint8_t r, uint8_t g, uint8_t b) {
    if (pin_ < 0) return;
    uint32_t c = (uint32_t)r << 16 | (uint32_t)g << 8 | b;
    if (c == last_) return;
    last_ = c;
    if (!ws_) { digitalWrite(pin_, ((c != 0) == activeHigh_) ? HIGH : LOW); return; }
    r = scale(r, bright_); g = scale(g, bright_); b = scale(b, bright_);
    // neopixelWrite(pin, r, g, b) sendet GRB; eine RGB-LED bekommt Rot/Gruen getauscht
    if (rgbOrder_) neopixelWrite(pin_, g, r, b);
    else           neopixelWrite(pin_, r, g, b);
}

void StatusLed::update(uint8_t mode, unsigned long now, bool dupAddress, bool wifiOn) {
    if (pin_ < 0) return;
    uint8_t pattern = mode;
    uint8_t r = 0, g = 40, b = 0;
    if (ws_) { StatusLedLook l = statusLedLook(mode, dupAddress, wifiOn); pattern = l.pattern; r = l.r; g = l.g; b = l.b; }
    if (pattern != mode_) { mode_ = pattern; start_ = now; }
    if (statusLedOn(pattern, now - start_)) writeRgb(r, g, b);
    else                                     writeRgb(0, 0, 0);
}
#endif
