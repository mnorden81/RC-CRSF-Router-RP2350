// multi_router.cpp - CRSF-Router mit vielen Ports, siehe multi_router.h
#include "multi_router.h"
#include <string.h>

// ── Parser ─────────────────────────────────────────────────────────────
static bool isSync(uint8_t b) {
    return b == CRSF_SYNC_byte || b == CRSF_ADDRESS_RADIO_TRANSMITTER ||
           b == CRSF_ADDRESS_CRSF_TRANSMITTER || b == CRSF_ADDRESS_ELRS_LUA ||
           b == CRSF_ADDRESS_CRSF_RECEIVER || b == CRSF_ADDRESS_BROADCAST;
}

bool MrParser::feed(uint8_t b, bool& crcError) {
    crcError = false;
    if (idx == 0) {
        if (isSync(b)) buf[idx++] = b;
        return false;
    }
    if (idx == 1) {
        if (b < 2 || b > MR_FRAME - 2) { idx = 0; return false; }
        len = b;
        buf[idx++] = b;
        return false;
    }
    buf[idx++] = b;
    if (idx < len + 2) return false;
    idx = 0;
    if (b == crc8(&buf[2], len - 1)) return true;
    crcError = true;
    return false;
}

// ── Warteschlangen ─────────────────────────────────────────────────────
// Voll: aeltesten RC-Kanal-Frame verwerfen (Aktualitaet vor
// Vollstaendigkeit, wie in CRSF_ESP32). Ohne RC-Frame in der Schlange wird
// ein neuer RC-Frame verworfen, andere Frames verdraengen den aeltesten.
// Der Kopf wird nie angefasst, wenn er schon teilweise gesendet ist.
template <uint8_t N>
bool MultiRouter::enqueue(MrQueue<N>& q, const uint8_t* f, uint8_t n, MrPortStats& st) {
    if (n > MR_FRAME) return false;
    if (q.count >= N) {
        uint8_t first = q.txOff ? 1 : 0;
        int victim = -1;
        for (uint8_t i = first; i < q.count; i++) {
            uint8_t k = (q.head + i) % N;
            if (q.q[k][2] == CRSF_FRAMETYPE_RC_CHANNELS_PACKED) { victim = i; break; }
        }
        st.dropped++;
        if (victim < 0) {
            if (f[2] == CRSF_FRAMETYPE_RC_CHANNELS_PACKED) return false;
            victim = first;
        }
        for (uint8_t i = victim; i + 1 < q.count; i++) {
            uint8_t a = (q.head + i) % N;
            uint8_t b = (q.head + i + 1) % N;
            memcpy(q.q[a], q.q[b], q.qLen[b]);
            q.qLen[a] = q.qLen[b];
        }
        q.count--;
    }
    uint8_t k = (q.head + q.count) % N;
    memcpy(q.q[k], f, n);
    q.qLen[k] = n;
    q.count++;
    return true;
}

// Nicht blockierend: nur so viele Bytes, wie der UART gerade nimmt
template <uint8_t N>
void MultiRouter::pump(MrQueue<N>& q, Stream* s, MrPortStats& st) {
    while (q.count > 0) {
        int room = s->availableForWrite();
        if (room <= 0) return;
        uint8_t* f = q.q[q.head];
        uint8_t  left = q.qLen[q.head] - q.txOff;
        uint8_t  n = (uint8_t)((int)left < room ? left : room);
        size_t   w = s->write(f + q.txOff, n);
        if (w == 0) return;
        q.txOff += (uint8_t)w;
        if (q.txOff >= q.qLen[q.head]) {
            q.txOff = 0;
            q.head = (q.head + 1) % N;
            q.count--;
            st.framesOut++;
        }
    }
}

// ── Router ─────────────────────────────────────────────────────────────
void MultiRouter::begin(Stream* up) { uplink = up; }

void MultiRouter::setPort(uint8_t i, Stream* s) {
    if (i < MR_MAX_DOWN) down[i] = s;
}

void MultiRouter::fromUplink(const uint8_t* f, uint8_t n) {
    upStats.framesIn++;
    upStats.lastRxMs = now_ ? now_ : 1;
    // Jeder Frame geht an die CRSF-Klasse (RC-Kanaele, Ping, Lua ...)
    if (!local_.push(f, n)) localDropped++;
    // Extended-Frame an die eigene Adresse bleibt hier. Ausnahme 0xC8
    // (Adresse 8 = Flight-Controller/Sync), die gilt als Broadcast.
    uint8_t type = f[2];
    if (type >= 0x28 && n > 4 && f[3] == ownAddr && ownAddr != CRSF_SYNC_byte) return;
    bool any = false;
    for (uint8_t i = 0; i < MR_MAX_DOWN; i++) {
        if (!down[i]) continue;
        enqueue(downQ[i], f, n, downStats[i]);
        any = true;
    }
    if (any) fwdDown_++;
}

void MultiRouter::fromDown(uint8_t i, const uint8_t* f, uint8_t n) {
    downStats[i].framesIn++;
    downStats[i].lastRxMs = now_ ? now_ : 1;
    // Doppelte Adresse: ein Geraet weiter hinten meldet unsere Adresse
    if (f[2] == CRSF_FRAMETYPE_DEVICE_INFO && n > 5 && f[4] == ownAddr) dupAddr = true;
    if (!uplink) return;
    enqueue(upQ, f, n, upStats);
    fwdUp_++;
}

void MultiRouter::poll(unsigned long now) {
    now_ = now;
    bool crcErr;
    if (uplink) {
        // Begrenzt, damit ein Dauerstrom die anderen Ports nicht aushungert
        for (int k = 0; k < 256 && uplink->available() > 0; k++) {
            if (upParser.feed((uint8_t)uplink->read(), crcErr))
                fromUplink(upParser.buf, upParser.frameLen());
            else if (crcErr) upStats.crcErrors++;
        }
    }
    for (uint8_t i = 0; i < MR_MAX_DOWN; i++) {
        if (!down[i]) continue;
        for (int k = 0; k < 256 && down[i]->available() > 0; k++) {
            if (downParser[i].feed((uint8_t)down[i]->read(), crcErr))
                fromDown(i, downParser[i].buf, downParser[i].frameLen());
            else if (crcErr) downStats[i].crcErrors++;
        }
    }
    if (uplink) pump(upQ, uplink, upStats);
    for (uint8_t i = 0; i < MR_MAX_DOWN; i++)
        if (down[i]) pump(downQ[i], down[i], downStats[i]);
}

uint32_t MultiRouter::dropped() const {
    uint32_t n = upStats.dropped + localDropped;
    for (uint8_t i = 0; i < MR_MAX_DOWN; i++) n += downStats[i].dropped;
    return n;
}

uint32_t MultiRouter::crcErrors() const {
    uint32_t n = 0;
    for (uint8_t i = 0; i < MR_MAX_DOWN; i++) n += downStats[i].crcErrors;
    return n;
}

bool MultiRouter::portAlive(uint8_t i, unsigned long now, unsigned long ms) const {
    if (i >= MR_MAX_DOWN || !down[i] || downStats[i].lastRxMs == 0) return false;
    return now - downStats[i].lastRxMs <= ms;
}

// ── Datenstrom fuer die CRSF-Klasse ────────────────────────────────────
bool MrLocalStream::push(const uint8_t* f, uint8_t n) {
    if (rxCount + n > MR_LOCAL_BUF) return false;   // nur ganze Frames
    for (uint8_t i = 0; i < n; i++) {
        rx[(rxHead + rxCount) % MR_LOCAL_BUF] = f[i];
        rxCount++;
    }
    return true;
}

int MrLocalStream::available() { return rxCount; }

int MrLocalStream::read() {
    if (rxCount == 0) return -1;
    uint8_t b = rx[rxHead];
    rxHead = (rxHead + 1) % MR_LOCAL_BUF;
    rxCount--;
    return b;
}

#ifdef ARDUINO
int MrLocalStream::peek() { return rxCount ? rx[rxHead] : -1; }
#endif

// Eigene Frames (DEVICE_INFO, Lua, Telemetrie) kommen von send_packets()
// am Stueck; der Parser stellt trotzdem sicher, dass nur ganze, gueltige
// Frames in die Warteschlange zum Empfaenger gehen.
size_t MrLocalStream::write(const uint8_t* b, size_t n) {
    bool crcErr;
    for (size_t i = 0; i < n; i++) {
        if (txParser.feed(b[i], crcErr) && r && r->uplink)
            r->enqueue(r->upQ, txParser.buf, txParser.frameLen(), r->upStats);
    }
    return n;
}
