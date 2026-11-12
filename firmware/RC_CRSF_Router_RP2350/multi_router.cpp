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
// Lua/Ping/DEVICE_INFO (0x28-0x2D): Vorrang nach oben, zuletzt verworfen
static bool isPrio(uint8_t type) { return type >= 0x28 && type <= 0x2D; }
// Telemetrie (Broadcast-Frames < 0x28 ausser RC-Kanaelen)
static bool isTele(uint8_t type) {
    return type < 0x28 && type != CRSF_FRAMETYPE_RC_CHANNELS_PACKED && type != 0x17;
}
// Quellen-ID bei RPM, Temperatur und Zellen (0x0C-0x0E), sonst 0
static uint8_t teleSub(const uint8_t* f, uint8_t n) {
    return (f[2] >= 0x0C && f[2] <= 0x0E && n > 4) ? f[3] : 0;
}

// Voll: zuerst den aeltesten RC-Kanal-Frame verwerfen (Aktualitaet vor
// Vollstaendigkeit, wie in CRSF_ESP32), dann den aeltesten Nicht-Lua-Frame,
// erst zuletzt Lua. Ein neuer RC-Frame verdraengt keinen anderen Frame.
// Nach oben (upward) zusaetzlich wie im ELRS-Empfaenger:
//   - Telemetrie gleicher Art (und Quellen-ID) vom selben Port ersetzt
//     den noch wartenden alten Frame an dessen Platz
//   - Lua-Frames reihen sich vor der Telemetrie ein (untereinander in
//     Reihenfolge)
// Der Kopf wird nie angefasst, wenn er schon teilweise gesendet ist.
template <uint8_t N>
bool MultiRouter::enqueue(MrQueue<N>& q, const uint8_t* f, uint8_t n, MrPortStats& st,
                          uint8_t src, bool upward) {
    if (n > MR_FRAME) return false;
    const uint8_t type  = f[2];
    const uint8_t first = q.txOff ? 1 : 0;
    if (upward && isTele(type)) {
        const uint8_t sub = teleSub(f, n);
        for (uint8_t i = first; i < q.count; i++) {
            uint8_t k = (q.head + i) % N;
            if (q.qSrc[k] == src && q.q[k][2] == type && teleSub(q.q[k], q.qLen[k]) == sub) {
                memcpy(q.q[k], f, n);
                q.qLen[k] = n;
                st.replaced++;
                return true;
            }
        }
    }
    if (q.count >= N) {
        int victim = -1;
        for (uint8_t i = first; i < q.count && victim < 0; i++)
            if (q.q[(q.head + i) % N][2] == CRSF_FRAMETYPE_RC_CHANNELS_PACKED) victim = i;
        st.dropped++;
        if (victim < 0 && type == CRSF_FRAMETYPE_RC_CHANNELS_PACKED) return false;
        for (uint8_t i = first; i < q.count && victim < 0; i++)
            if (!isPrio(q.q[(q.head + i) % N][2])) victim = i;
        if (victim < 0) victim = first;
        for (uint8_t i = victim; i + 1 < q.count; i++) {
            uint8_t a = (q.head + i) % N;
            uint8_t b = (q.head + i + 1) % N;
            memcpy(q.q[a], q.q[b], q.qLen[b]);
            q.qLen[a] = q.qLen[b];
            q.qSrc[a] = q.qSrc[b];
        }
        q.count--;
    }
    // Einfuegestelle: hinten, Lua nach oben hinter den letzten Lua-Frame
    uint8_t pos = q.count;
    if (upward && isPrio(type)) {
        pos = first;
        while (pos < q.count && isPrio(q.q[(q.head + pos) % N][2])) pos++;
        for (uint8_t i = q.count; i > pos; i--) {
            uint8_t a = (q.head + i) % N;
            uint8_t b = (q.head + i - 1) % N;
            memcpy(q.q[a], q.q[b], q.qLen[b]);
            q.qLen[a] = q.qLen[b];
            q.qSrc[a] = q.qSrc[b];
        }
    }
    uint8_t k = (q.head + pos) % N;
    memcpy(q.q[k], f, n);
    q.qLen[k] = n;
    q.qSrc[k] = src;
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
MultiRouter::MultiRouter() {
    local_.r = this;
    memset(addrPort, 0xFF, sizeof(addrPort));
}

void MultiRouter::begin(Stream* up) { uplink = up; }

void MultiRouter::setPort(uint8_t i, Stream* s) {
    if (i < MR_MAX_DOWN) down[i] = s;
}

// ── Filter je Port ─────────────────────────────────────────────────────
void MultiRouter::setPortFilter(uint8_t i, bool rc, bool linkStats) {
    if (i >= MR_MAX_DOWN) return;
    pcfg[i].rc = rc;
    pcfg[i].linkStats = linkStats;
}

void MultiRouter::setTelemetry(uint8_t i, uint16_t ms) {
    if (i >= MR_MAX_DOWN) return;
    pcfg[i].teleMs = ms;
    memset(tele[i], 0, sizeof(tele[i]));
}

// Nach unten: RC-Kanaele und LinkStats je Port abschaltbar, alles andere
// (Ping, Lua, Befehle, Broadcasts) geht immer durch
bool MultiRouter::passDown(uint8_t i, uint8_t type) {
    if (type == CRSF_FRAMETYPE_RC_CHANNELS_PACKED || type == 0x17) return pcfg[i].rc;
    if (type == CRSF_FRAMETYPE_LINK_STATISTICS || type == 0x1C || type == 0x1D) return pcfg[i].linkStats;
    return true;
}

// Nach oben: Telemetrie (Frames < 0x28) je Port aus oder gebremst. Die
// Bremse zaehlt je Sensor-Art; bei RPM, Temperatur und Zellen (0x0C-0x0E)
// zaehlt die Quellen-ID mit, damit sich mehrere Sensoren nicht verdraengen.
bool MultiRouter::passUp(uint8_t i, const uint8_t* f, uint8_t n) {
    uint8_t type = f[2];
    if (type >= 0x28) return true;
    uint16_t ms = pcfg[i].teleMs;
    if (ms == 0) return true;
    if (ms == MR_TELE_OFF) return false;
    uint8_t sub = (type >= 0x0C && type <= 0x0E && n > 4) ? f[3] : 0;
    MrTeleSlot* slot = nullptr;
    for (uint8_t k = 0; k < MR_TELE_SLOTS && !slot; k++) {
        MrTeleSlot& t = tele[i][k];
        if (t.used && t.type == type && t.sub == sub) slot = &t;
    }
    if (slot) {
        if (now_ - slot->last < ms) return false;
    } else {
        // neue Sensor-Art: freien Platz nehmen, sonst den am laengsten
        // nicht benutzten
        slot = &tele[i][0];
        for (uint8_t k = 0; k < MR_TELE_SLOTS; k++) {
            MrTeleSlot& t = tele[i][k];
            if (!t.used) { slot = &t; break; }
            if ((long)(t.last - slot->last) < 0) slot = &t;
        }
        slot->used = 1; slot->type = type; slot->sub = sub;
    }
    slot->last = now_;
    return true;
}

// ── Geraeteliste ───────────────────────────────────────────────────────
// DEVICE_INFO: [sync][len][0x29][Ziel][Absender][Name\0][Seriennr. ...]
void MultiRouter::learnDevice(uint8_t i, const uint8_t* f, uint8_t n) {
    if (n < 7) return;
    uint8_t addr = f[4];
    char name[MR_DEV_NAME];
    uint8_t k = 0;
    for (uint8_t p = 5; p + 1 < n && f[p] && k + 1 < MR_DEV_NAME; p++) {
        uint8_t c = f[p];
        name[k++] = (c >= 0x20 && c < 0x7F) ? (char)c : '?';
    }
    name[k] = '\0';
    for (uint8_t d = 0; d < devCount[i]; d++) {
        if (dev[i][d].addr == addr) {
            if (strcmp(dev[i][d].name, name) != 0) { memcpy(dev[i][d].name, name, k + 1); devChanges++; }
            return;
        }
    }
    if (devCount[i] >= MR_DEV_PER_PORT) return;
    dev[i][devCount[i]].addr = addr;
    memcpy(dev[i][devCount[i]].name, name, k + 1);
    devCount[i]++;
    devChanges++;
}

void MultiRouter::clearDevices() {
    memset(devCount, 0, sizeof(devCount));
    memset(addrPort, 0xFF, sizeof(addrPort));
    devChanges++;
}

// ── Adress-Lernen (wie CRSFConnector im ELRS-Empfaenger) ───────────────
// Absender, die nie hinter einem Port sitzen: Broadcast, 0xC8 (gilt hier
// als Broadcast), Sender, Sendemodul, Empfaenger, ELRS-Lua und wir selbst.
void MultiRouter::learnAddress(uint8_t i, uint8_t addr) {
    if (addr == 0x00 || addr == CRSF_SYNC_byte || addr == ownAddr ||
        addr == CRSF_ADDRESS_RADIO_TRANSMITTER || addr == CRSF_ADDRESS_CRSF_TRANSMITTER ||
        addr == CRSF_ADDRESS_CRSF_RECEIVER || addr == CRSF_ADDRESS_ELRS_LUA) return;
    addrPort[addr] = i;   // zuletzt gesehen gewinnt (Geraet umgesteckt)
}

int MultiRouter::portOf(uint8_t addr) const {
    uint8_t i = addrPort[addr];
    return (i < MR_MAX_DOWN && down[i]) ? i : -1;
}

// Ping (0x28) an Broadcast, Absender = eigene Adresse. Die Geraete
// antworten mit DEVICE_INFO an uns; das landet nur in der Geraeteliste.
void MultiRouter::pingPorts() {
    uint8_t f[6] = {CRSF_SYNC_byte, 4, CRSF_FRAMETYPE_DEVICE_PING, 0x00, ownAddr, 0};
    f[5] = crc8(&f[2], 3);
    for (uint8_t i = 0; i < MR_MAX_DOWN; i++)
        if (down[i]) enqueue(downQ[i], f, sizeof(f), downStats[i]);
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
    // Ziel bekannt: nur an den Port, hinter dem es sitzt
    if (type >= 0x28 && n > 4) {
        int t = portOf(f[3]);
        if (t >= 0) {
            enqueue(downQ[t], f, n, downStats[t]);
            fwdDown_++;
            targeted_++;
            return;
        }
    }
    bool any = false;
    for (uint8_t i = 0; i < MR_MAX_DOWN; i++) {
        if (!down[i]) continue;
        if (!passDown(i, type)) { downStats[i].filtered++; continue; }
        enqueue(downQ[i], f, n, downStats[i]);
        any = true;
    }
    if (any) fwdDown_++;
}

void MultiRouter::fromDown(uint8_t i, const uint8_t* f, uint8_t n) {
    downStats[i].framesIn++;
    downStats[i].lastRxMs = now_ ? now_ : 1;
    if (f[2] >= 0x28 && n > 5) learnAddress(i, f[4]);
    else if (f[2] == 0x0B && n >= 6) learnAddress(i, f[4]);   // Heartbeat: int16 MSB, Adresse im 2. Byte
    if (f[2] == CRSF_FRAMETYPE_DEVICE_INFO && n > 5) {
        // Doppelte Adresse: ein Geraet weiter hinten meldet unsere Adresse
        if (f[4] == ownAddr) dupAddr = true;
        learnDevice(i, f, n);
        // Antwort auf unseren eigenen Ping: bleibt hier
        if (f[3] == ownAddr) return;
    }
    if (!uplink) return;
    if (!passUp(i, f, n)) { downStats[i].filtered++; return; }
    enqueue(upQ, f, n, upStats, i, true);
    fwdUp_++;
}

// Bytes eines Ports lesen. Bleibt ein Frame halb liegen (Stoerung, Geraet
// abgesteckt), wird er nach MR_PARSER_TIMEOUT_MS verworfen, damit der
// naechste Frame sauber erkannt wird.
void MultiRouter::readPort(Stream* s, MrParser& p, unsigned long& lastByte, bool up, uint8_t i) {
    if (p.idx && s->available() <= 0 && now_ - lastByte > MR_PARSER_TIMEOUT_MS) p.idx = 0;
    bool crcErr;
    // Begrenzt, damit ein Dauerstrom die anderen Ports nicht aushungert
    for (int k = 0; k < 256 && s->available() > 0; k++) {
        lastByte = now_;
        if (p.feed((uint8_t)s->read(), crcErr)) {
            if (up) fromUplink(p.buf, p.frameLen());
            else    fromDown(i, p.buf, p.frameLen());
        } else if (crcErr) {
            if (up) upStats.crcErrors++;
            else    downStats[i].crcErrors++;
        }
    }
}

void MultiRouter::poll(unsigned long now) {
    now_ = now;
    if (uplink) readPort(uplink, upParser, upByteMs, true, 0);
    for (uint8_t i = 0; i < MR_MAX_DOWN; i++)
        if (down[i]) readPort(down[i], downParser[i], downByteMs[i], false, i);
    if (uplink) pump(upQ, uplink, upStats);
    for (uint8_t i = 0; i < MR_MAX_DOWN; i++)
        if (down[i]) pump(downQ[i], down[i], downStats[i]);
}

uint32_t MultiRouter::dropped() const {
    uint32_t n = upStats.dropped + localDropped;
    for (uint8_t i = 0; i < MR_MAX_DOWN; i++) n += downStats[i].dropped;
    return n;
}

uint32_t MultiRouter::filtered() const {
    uint32_t n = 0;
    for (uint8_t i = 0; i < MR_MAX_DOWN; i++) n += downStats[i].filtered;
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
            r->enqueue(r->upQ, txParser.buf, txParser.frameLen(), r->upStats, MR_SRC_SELF, true);
    }
    return n;
}
