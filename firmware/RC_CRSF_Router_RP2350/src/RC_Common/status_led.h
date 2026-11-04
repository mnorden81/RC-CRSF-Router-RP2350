// status_led.h - RC_Common: Status-LED mit Blinkmustern
//
// Muster (aus RC-Multiswitch v4.00, Firmware-Spezifikation Kapitel 6):
//   aus                = kein Signal
//   langsam 500/500 ms = RC-Signal in Ordnung
//   schnell 100/100 ms = Frames kommen, aber Failsafe bzw. kein RC-Link
//   Doppelblitz        = Konfigurationsfehler (z. B. doppelte Moduladresse)
// Reine Anzeige, aendert nichts am Verhalten der Firmware.
//
// statusLedMode()/statusLedOn() sind reine Logik (Host-Tests in tests/),
// StatusLed treibt die LED (WS2812 oder normaler GPIO).
#ifndef RC_STATUS_LED_H
#define RC_STATUS_LED_H

#include <stdint.h>

static constexpr uint8_t SLED_OFF    = 0;
static constexpr uint8_t SLED_SLOW   = 1;
static constexpr uint8_t SLED_FAST   = 2;
static constexpr uint8_t SLED_DOUBLE = 3;

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

uint8_t statusLedMode(const StatusLedInput& in, unsigned long nowMs);
// t = Zeit seit Beginn des Musters
bool    statusLedOn(uint8_t mode, unsigned long t);

#if defined(ARDUINO) && defined(ESP32)   // RP2350-Kopie: nur ESP32
class StatusLed {
public:
    // pin < 0 = keine LED. ws2812: RGB-LED (z. B. ESP32-S3-Zero GPIO 21),
    // sonst GPIO (activeHigh). brightness nur fuer WS2812 (0-255).
    void begin(int pin, bool ws2812, uint8_t brightness = 40, bool activeHigh = true);
    void update(uint8_t mode, unsigned long nowMs);
private:
    void write(bool on);
    int           pin_ = -1;
    bool          ws_ = false;
    bool          activeHigh_ = true;
    uint8_t       bright_ = 40;
    uint8_t       mode_ = 0xFF;
    unsigned long start_ = 0;
    bool          on_ = false;
};
#endif

#endif
