// multi_router.h - CRSF-Router mit vielen Ports (RP2350)
//
// Gleiche Regeln wie der Router in RC_Common (CRSF_ESP32 1.1.0), nur mit
// bis zu 7 Ports nach unten statt 2:
//   Uplink -> Ports : alles ausser Extended-Frames an die eigene Adresse
//                     (0xC8 gilt als Broadcast und geht immer weiter)
//   Ports -> Uplink : alles
//   Port <-> Port   : nie (keine Schleifen)
// Ab v0.4.0 je Port einstellbar (Vorgabe: alles an, wie bisher):
//   - Filter nach unten: RC-Kanaele (0x16/0x17), LinkStats (0x14/0x1C/0x1D)
//   - Telemetrie nach oben (Frames < 0x28): alle, hoechstens x/s je
//     Sensor-Art (Bremse) oder aus. Extended-Frames (DEVICE_INFO, Lua,
//     Befehle) gehen immer hoch.
//   - Geraeteliste: DEVICE_INFO-Frames von unten werden je Port gemerkt.
//     pingPorts() fragt alle Ports aktiv ab; Antworten an die eigene
//     Adresse bleiben im Router und gehen nicht zum Empfaenger.
// Ab v0.4.1 (Ideen aus dem Router im ELRS-Empfaenger 4.x):
//   - Adress-Lernen: jede Absender-Adresse aus Extended-Frames (ab 0x28)
//     und Heartbeat (0x0B) wird dem Port zugeordnet, von dem sie kam.
//     Extended-Frames vom Empfaenger an eine bekannte Adresse gehen nur
//     noch an diesen Port. Unbekannt, Broadcast (0x00) und 0xC8 -> alle.
//   - Telemetrie nach oben: ein neuer Wert gleicher Art vom selben Port
//     ersetzt den noch wartenden alten (Ueberschreiben statt Verwerfen).
//   - Lua/Ping-Frames (0x28-0x2D) nach oben haben Vorrang vor Telemetrie.
//   - Volle Warteschlange: zuerst RC-Frame, dann Telemetrie, Lua zuletzt.
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

#define MR_MAX_DOWN      7     // Ports nach unten (Platz; Variante Test nutzt 6)
#define MR_FRAME         64    // groesster CRSF-Frame (wie CRSF_PACKET_SIZE)
#define MR_QUEUE_DOWN    8     // Frames je Port nach unten
#define MR_QUEUE_UP      16    // Frames zum Empfaenger (alle Ports + eigene)
#define MR_LOCAL_BUF     512   // Bytes fuer die CRSF-Klasse (Empfang)
#define MR_TELE_SLOTS    8     // Sensor-Arten je Port fuer die Telemetrie-Bremse
#define MR_DEV_PER_PORT  4     // gemerkte Geraete je Port
#define MR_DEV_NAME      24    // Zeichen je Geraetename (mit \0)
#define MR_PARSER_TIMEOUT_MS 3 // halber Frame ohne neue Bytes -> verwerfen

// Telemetrie nach oben: 0 = alle, MR_TELE_OFF = aus, sonst Mindestabstand
// in ms je Sensor-Art
#define MR_TELE_OFF      0xFFFF
#define MR_SRC_SELF      0xFF  // Herkunft "Router selbst / Empfaenger"

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
    uint8_t qSrc[N];     // Herkunft: Port-Index, MR_SRC_SELF = eigene Frames
    uint8_t head  = 0;
    uint8_t count = 0;
    uint8_t txOff = 0;   // vom Kopf schon gesendete Bytes
};

struct MrPortStats {
    uint32_t framesIn  = 0;   // gueltige Frames empfangen
    uint32_t framesOut = 0;   // Frames komplett gesendet
    uint32_t dropped   = 0;   // wegen voller Warteschlange verworfen
    uint32_t crcErrors = 0;
    uint32_t filtered  = 0;   // durch Filter/Telemetrie-Bremse nicht weitergegeben
    uint32_t replaced  = 0;   // wartender Telemetrie-Frame durch neueren ersetzt
    unsigned long lastRxMs = 0;   // letzter gueltiger Frame, 0 = noch keiner
};

// Ein Geraet an einem Port (aus seinem DEVICE_INFO)
struct MrDevice {
    uint8_t addr;               // CRSF-Adresse (Absender im DEVICE_INFO)
    char    name[MR_DEV_NAME];
};

// Einstellungen je Port
struct MrPortCfg {
    bool     rc        = true;   // RC-Kanaele nach unten
    bool     linkStats = true;   // LinkStats nach unten
    uint16_t teleMs    = 0;      // Telemetrie nach oben, siehe MR_TELE_OFF
};

struct MrTeleSlot {
    uint8_t  type, sub, used;
    unsigned long last;
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
    MultiRouter();

    // uplink = UART zum Empfaenger, down[i] = Port i+1 (nullptr = aus)
    void begin(Stream* uplink);
    void setPort(uint8_t i, Stream* s);
    void setOwnAddress(uint8_t a) { ownAddr = a; }

    // Einstellungen je Port (wirken sofort)
    void setPortFilter(uint8_t i, bool rc, bool linkStats);
    void setTelemetry(uint8_t i, uint16_t ms);        // 0 = alle, MR_TELE_OFF = aus
    const MrPortCfg& portCfg(uint8_t i) const { return pcfg[i]; }

    // Geraeteliste
    void    pingPorts();                              // Ping an alle aktiven Ports
    void    clearDevices();
    uint8_t deviceCount(uint8_t i) const { return i < MR_MAX_DOWN ? devCount[i] : 0; }
    const MrDevice& device(uint8_t i, uint8_t k) const { return dev[i][k]; }
    uint32_t devicesChanged() const { return devChanges; }   // zaehlt bei jeder Aenderung hoch
    // Gelernte Adressen: Port-Index, hinter dem die Adresse sitzt, sonst -1
    int      portOf(uint8_t addr) const;

    // Jeden Loop aufrufen: lesen, weiterleiten, senden. now = millis()
    void poll(unsigned long now);

    MrLocalStream& local() { return local_; }

    // Zaehler
    uint32_t fwdDown()   const { return fwdDown_; }    // Frames vom Empfaenger nach unten
    uint32_t fwdUp()     const { return fwdUp_; }      // Frames von unten zum Empfaenger
    uint32_t dropped()   const;                        // alle Ports + Uplink
    uint32_t crcErrors() const;                        // alle Ports nach unten
    uint32_t filtered()  const;                        // alle Ports, durch Filter/Bremse
    uint32_t targeted()  const { return targeted_; }   // gezielt an nur einen Port
    uint32_t replaced()  const { return upStats.replaced; }   // Telemetrie ueberschrieben
    bool     duplicateAddress() const { return dupAddr; }
    const MrPortStats& uplinkStats() const { return upStats; }
    const MrPortStats& portStats(uint8_t i) const { return downStats[i]; }
    bool     portEnabled(uint8_t i) const { return i < MR_MAX_DOWN && down[i] != nullptr; }
    // Port i hat in den letzten ms Millisekunden etwas gesendet
    bool     portAlive(uint8_t i, unsigned long now, unsigned long ms = 1500) const;

private:
    friend class MrLocalStream;
    template <uint8_t N> bool enqueue(MrQueue<N>& q, const uint8_t* f, uint8_t n, MrPortStats& st,
                                      uint8_t src = MR_SRC_SELF, bool upward = false);
    template <uint8_t N> void pump(MrQueue<N>& q, Stream* s, MrPortStats& st);
    void fromUplink(const uint8_t* f, uint8_t n);
    void fromDown(uint8_t i, const uint8_t* f, uint8_t n);
    bool passDown(uint8_t i, uint8_t type);
    bool passUp(uint8_t i, const uint8_t* f, uint8_t n);
    void learnDevice(uint8_t i, const uint8_t* f, uint8_t n);
    void learnAddress(uint8_t i, uint8_t addr);
    void readPort(Stream* s, MrParser& p, unsigned long& lastByte, bool up, uint8_t i);

    Stream*   uplink = nullptr;
    Stream*   down[MR_MAX_DOWN] = {nullptr};
    MrParser  upParser;
    MrParser  downParser[MR_MAX_DOWN];
    MrQueue<MR_QUEUE_UP>   upQ;
    MrQueue<MR_QUEUE_DOWN> downQ[MR_MAX_DOWN];
    MrPortStats upStats;
    MrPortStats downStats[MR_MAX_DOWN];
    MrPortCfg   pcfg[MR_MAX_DOWN];
    MrTeleSlot  tele[MR_MAX_DOWN][MR_TELE_SLOTS] = {};
    MrDevice    dev[MR_MAX_DOWN][MR_DEV_PER_PORT] = {};
    uint8_t     devCount[MR_MAX_DOWN] = {0};
    uint32_t    devChanges = 0;
    uint8_t     addrPort[256];   // gelernte Adresse -> Port-Index, 0xFF = unbekannt
    unsigned long upByteMs = 0, downByteMs[MR_MAX_DOWN] = {0};
    MrLocalStream local_;
    uint8_t  ownAddr = 0xCF;
    uint32_t fwdDown_ = 0, fwdUp_ = 0, localDropped = 0, targeted_ = 0;
    bool     dupAddr = false;
    unsigned long now_ = 0;
};

#endif
