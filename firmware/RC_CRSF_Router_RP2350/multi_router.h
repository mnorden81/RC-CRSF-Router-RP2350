// multi_router.h - CRSF-Router mit vielen Ports (RP2350)
//
// Gleiche Regeln wie der Router in RC_Common (CRSF_ESP32 1.1.0), nur mit
// bis zu 6 Ports nach unten statt 2:
//   Uplink -> Ports : alles ausser Extended-Frames an die eigene Adresse
//                     (0xC8 gilt als Broadcast und geht immer weiter)
//   Ports -> Uplink : alles
//   Port <-> Port   : nie (keine Schleifen)
// Nur vollstaendige Frames mit gueltiger CRC, unveraendert (Store and
// Forward). Jeder Port hat eine eigene Sendewarteschlange; gesendet wird
// nicht blockierend Byte fuer Byte, solange der UART-FIFO Platz hat. So
// bremst ein langsamer oder voller Port die anderen nie aus.
//
// Die CRSF-Klasse aus RC_Common (Lua-Menue, DEVICE_INFO, Telemetrie) haengt
// nicht direkt am UART, sondern an local(): sie bekommt jeden Frame vom
// Empfaenger zu lesen, und was sie sendet, reiht der Router in die
// Warteschlange zum Empfaenger ein (nie mitten in einen anderen Frame).
//
// Laeuft auch in den Host-Tests (tests/), braucht nur crsf_platform.h.
#ifndef MULTI_ROUTER_H
#define MULTI_ROUTER_H

#include "src/RC_Common/CRSF_ESP32/crsf_esp32.h"

#define MR_MAX_DOWN      6     // Ports nach unten
#define MR_FRAME         64    // groesster CRSF-Frame (wie CRSF_PACKET_SIZE)
#define MR_QUEUE_DOWN    8     // Frames je Port nach unten
#define MR_QUEUE_UP      16    // Frames zum Empfaenger (alle Ports + eigene)
#define MR_LOCAL_BUF     512   // Bytes fuer die CRSF-Klasse (Empfang)

// Frame-Parser (gleicher Ablauf wie read_packets() in CRSF_ESP32)
struct MrParser {
    uint8_t buf[MR_FRAME];
    uint8_t idx = 0;
    uint8_t len = 0;
    // true = gueltiger Frame in buf, Laenge frameLen()
    // crcError wird bei falscher CRC gesetzt
    bool    feed(uint8_t b, bool& crcError);
    uint8_t frameLen() const { return (uint8_t)(len + 2); }
};

// Sendewarteschlange ganzer Frames, Kopf wird Byte fuer Byte gesendet
template <uint8_t N>
struct MrQueue {
    uint8_t q[N][MR_FRAME];
    uint8_t qLen[N];
    uint8_t head  = 0;
    uint8_t count = 0;
    uint8_t txOff = 0;   // vom Kopf schon gesendete Bytes
};

struct MrPortStats {
    uint32_t framesIn  = 0;   // gueltige Frames empfangen
    uint32_t framesOut = 0;   // Frames komplett gesendet
    uint32_t dropped   = 0;   // wegen voller Warteschlange verworfen
    uint32_t crcErrors = 0;
    unsigned long lastRxMs = 0;   // letzter gueltiger Frame, 0 = noch keiner
};

class MultiRouter;

// Datenstrom fuer die CRSF-Klasse (crsf.attach_stream(&router.local()))
class MrLocalStream : public Stream {
public:
    int    available() override;
    int    read() override;
    int    availableForWrite() override { return MR_FRAME; }
    size_t write(const uint8_t* b, size_t n) override;
#ifdef ARDUINO
    int    peek() override;
    size_t write(uint8_t c) override { return write(&c, 1); }
    void   flush() override {}
    using Print::write;
#endif
private:
    friend class MultiRouter;
    MultiRouter* r = nullptr;
    uint8_t  rx[MR_LOCAL_BUF];
    uint16_t rxHead = 0, rxCount = 0;
    MrParser txParser;   // eigene Frames erst komplett, dann einreihen
    bool     push(const uint8_t* f, uint8_t n);
};

class MultiRouter {
public:
    MultiRouter() { local_.r = this; }

    // uplink = UART zum Empfaenger, down[i] = Port i+1 (nullptr = aus)
    void begin(Stream* uplink);
    void setPort(uint8_t i, Stream* s);
    void setOwnAddress(uint8_t a) { ownAddr = a; }

    // Jeden Loop aufrufen: lesen, weiterleiten, senden. now = millis()
    void poll(unsigned long now);

    MrLocalStream& local() { return local_; }

    // Zaehler
    uint32_t fwdDown()   const { return fwdDown_; }    // Frames vom Empfaenger nach unten
    uint32_t fwdUp()     const { return fwdUp_; }      // Frames von unten zum Empfaenger
    uint32_t dropped()   const;                        // alle Ports + Uplink
    uint32_t crcErrors() const;                        // alle Ports nach unten
    bool     duplicateAddress() const { return dupAddr; }
    const MrPortStats& uplinkStats() const { return upStats; }
    const MrPortStats& portStats(uint8_t i) const { return downStats[i]; }
    bool     portEnabled(uint8_t i) const { return i < MR_MAX_DOWN && down[i] != nullptr; }
    // Port i hat in den letzten ms Millisekunden etwas gesendet
    bool     portAlive(uint8_t i, unsigned long now, unsigned long ms = 1500) const;

private:
    friend class MrLocalStream;
    template <uint8_t N> bool enqueue(MrQueue<N>& q, const uint8_t* f, uint8_t n, MrPortStats& st);
    template <uint8_t N> void pump(MrQueue<N>& q, Stream* s, MrPortStats& st);
    void fromUplink(const uint8_t* f, uint8_t n);
    void fromDown(uint8_t i, const uint8_t* f, uint8_t n);

    Stream*   uplink = nullptr;
    Stream*   down[MR_MAX_DOWN] = {nullptr};
    MrParser  upParser;
    MrParser  downParser[MR_MAX_DOWN];
    MrQueue<MR_QUEUE_UP>   upQ;
    MrQueue<MR_QUEUE_DOWN> downQ[MR_MAX_DOWN];
    MrPortStats upStats;
    MrPortStats downStats[MR_MAX_DOWN];
    MrLocalStream local_;
    uint8_t  ownAddr = 0xCF;
    uint32_t fwdDown_ = 0, fwdUp_ = 0, localDropped = 0;
    bool     dupAddr = false;
    unsigned long now_ = 0;
};

#endif
