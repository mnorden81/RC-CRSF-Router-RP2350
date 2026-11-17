// status_led.h - RC_Common: Status-LED mit Blinkmustern und Farben
//
// Gleiches Schema in allen Modulen (seit RC_Common 1.4.0).
//
// RGB-LED (WS2812):
//   gruen   langsam 500/500 ms  RC-Signal in Ordnung
//   rot     schnell 100/100 ms  Frames kommen, aber Failsafe bzw. kein RC-Link
//   rot     Aufblitzen 100/900  kein Signal am Eingang (Modul lebt)
//   blau    Doppelblitz         Adresskonflikt (Geraet mit gleicher Adresse)
//   gelb    Doppelblitz         sonstiger Konfigurationsfehler
//   violett statt gruen/rot     WLAN ist an (Muster bleibt)
//   blau    blinkend 250/250    BOOT-Fenster nach dem Start (Projekt)
//
// Einfache LED (GPIO): langsam = OK, schnell = Failsafe, aus = kein Signal,
// Doppelblitz = Konfigurationsfehler.
// Reine Anzeige, aendert nichts am Verhalten der Firmware.
//
// statusLedMode()/statusLedOn()/statusLedLook() sind reine Logik (Host-Tests
// in tests/, auch vom RP2350-Router benutzt), StatusLed treibt die LED.
#ifndef RC_STATUS_LED_H
#define RC_STATUS_LED_H

#include <stdint.h>

// Zustaende (statusLedMode) und zugleich Blinkmuster (statusLedOn)
static constexpr uint8_t SLED_OFF    = 0;   // Zustand: kein Signal / Muster: aus
static constexpr uint8_t SLED_SLOW   = 1;
static constexpr uint8_t SLED_FAST   = 2;
static constexpr uint8_t SLED_DOUBLE = 3;
static constexpr uint8_t SLED_BLIP   = 4;   // nur Muster: 100 ms an, 900 ms aus

// Eingaenge fuer die Musterwahl
struct StatusLedInput {
    bool          configError;     // Doppelblitz hat Vorrang
    bool          crsf;            // Port A laeuft mit CRSF (sonst SBUS)
    bool          busOk;           // gueltige Frames (CRSF) bzw. BUS_OK
    unsigned long lastRcFrameMs;   // CRSF: letzter RC-Kanal-Frame, 0 = keiner
    unsigned long linkStatsMs;     // CRSF: letzte Link-Statistik, 0 = keine
    uint8_t       uplinkLQ;        // CRSF: Link-Qualitaet aus der Link-Statistik
    unsigned long lastSbusFrameMs; // SBUS: letzter Frame, 0 = keiner
    bool          sbusFailsafe;    // SBUS: Failsafe-Bit
};

static constexpr unsigned long LED_RC_LINK_TIMEOUT_MS = 1000;
static constexpr unsigned long LED_LINKSTATS_MAX_AGE  = 2000;
static constexpr unsigned long LED_SBUS_TIMEOUT_MS    = 2000;
static constexpr unsigned long LED_BOOT_BLINK_MS      = 250;   // BOOT-Fenster

uint8_t statusLedMode(const StatusLedInput& in, unsigned long nowMs);
// t = Zeit seit Beginn des Musters
bool    statusLedOn(uint8_t pattern, unsigned long t);

// Farbe und Muster einer RGB-LED fuer einen Zustand (Helligkeit fuer 40/255)
struct StatusLedLook {
    uint8_t pattern;   // SLED_SLOW/FAST/DOUBLE/BLIP
    uint8_t r, g, b;
};
// dupAddress: Doppelblitz blau statt gelb; wifiOn: violett statt gruen/rot
StatusLedLook statusLedLook(uint8_t mode, bool dupAddress, bool wifiOn);
// BOOT-Fenster: blau blinkend, t = Zeit seit Beginn des Fensters
bool    statusLedBootOn(unsigned long t);
// Klartext fuer WebUI/USB, z. B. "gruen langsam: RC-Signal OK".
// rgb = false: einfache LED (ohne Farben, "aus" bei kein Signal)
const char* statusLedText(uint8_t mode, bool dupAddress, bool wifiOn, bool rgb = true);

#if defined(ARDUINO) && defined(ESP32)   // nur ESP32 (RP2350 nutzt den Rest)
class StatusLed {
public:
    // pin < 0 = keine LED. ws2812: RGB-LED (z. B. ESP32-S3-Zero GPIO 21),
    // sonst GPIO (activeHigh). brightness nur fuer WS2812 (0-255, 40 = Vorgabe).
    void begin(int pin, bool ws2812, uint8_t brightness = 40, bool activeHigh = true);
    // Farbfolge der WS2812: false = GRB (Standard), true = RGB (S3-Zero)
    void setRgbOrder(bool rgb);
    // mode aus statusLedMode(). Nur WS2812: dupAddress = Doppelblitz blau,
    // wifiOn = violett.
    void update(uint8_t mode, unsigned long nowMs, bool dupAddress = false, bool wifiOn = false);
    // Direkt schreiben (z. B. BOOT-Fenster), Werte fuer Helligkeit 40.
    // Einfache LED: an, sobald eine Farbe > 0 ist.
    void writeRgb(uint8_t r, uint8_t g, uint8_t b);
    // BOOT-Fenster: blau blinkend, t = Zeit seit Beginn
    void showBootWindow(unsigned long t) { writeRgb(0, 0, statusLedBootOn(t) ? 40 : 0); }
private:
    int           pin_ = -1;
    bool          ws_ = false;
    bool          activeHigh_ = true;
    bool          rgbOrder_ = false;
    uint8_t       bright_ = 40;
    uint8_t       mode_ = 0xFF;
    unsigned long start_ = 0;
    uint32_t      last_ = 0xFFFFFFFF;   // zuletzt geschriebene Farbe
};
#endif

#endif
