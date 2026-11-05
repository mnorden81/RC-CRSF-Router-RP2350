// tests/test_router.cpp - Host-Tests fuer den 7-Port-Router (Variante Router; Test nutzt 6) (ohne RP2350)
//   sh tests/run_tests.sh
// Prueft die Weiterleitungsregeln, die Warteschlangen, das nicht
// blockierende Senden und die Anbindung der CRSF-Klasse (Lua, Ping).
#include <cstdio>
#include <vector>
#include "multi_router.h"
#include "router_config.h"

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FEHLER %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)


class FakeStream : public Stream {
public:
    std::vector<uint8_t> rx, tx;
    size_t pos = 0;
    int room = 4096;           // Platz im Sende-FIFO
    int available() override { return (int)(rx.size() - pos); }
    int read() override { return pos < rx.size() ? rx[pos++] : -1; }
    int availableForWrite() override { return room; }
    size_t write(const uint8_t* b, size_t n) override {
        if ((int)n > room) n = room;
        tx.insert(tx.end(), b, b + n);
        if (room < 4096) room -= (int)n;   // kleiner FIFO laeuft voll
        return n;
    }
    void feed(const std::vector<uint8_t>& f) { rx.insert(rx.end(), f.begin(), f.end()); }
};

static uint8_t crcRef(const uint8_t* p, size_t n) {
    uint8_t c = 0;
    for (size_t i = 0; i < n; i++) { c ^= p[i]; for (int b = 0; b < 8; b++) c = (c & 0x80) ? (uint8_t)((c << 1) ^ 0xD5) : (uint8_t)(c << 1); }
    return c;
}
static std::vector<uint8_t> frame(uint8_t type, const std::vector<uint8_t>& pl, uint8_t sync = 0xC8) {
    std::vector<uint8_t> f{sync, (uint8_t)(pl.size() + 2), type};
    f.insert(f.end(), pl.begin(), pl.end());
    f.push_back(crcRef(&f[2], f.size() - 2));
    return f;
}
static std::vector<uint8_t> rcFrame() { return frame(0x16, std::vector<uint8_t>(22, 0x11)); }
static bool contains(const std::vector<uint8_t>& hay, const std::vector<uint8_t>& f) {
    if (hay.size() < f.size()) return false;
    for (size_t i = 0; i + f.size() <= hay.size(); i++)
        if (std::equal(f.begin(), f.end(), hay.begin() + i)) return true;
    return false;
}

static void testDownBroadcast() {
    MultiRouter r; FakeStream up, p[7];
    r.begin(&up); for (int i = 0; i < 7; i++) r.setPort(i, &p[i]);
    r.setOwnAddress(0xCF);
    auto rc = rcFrame();
    up.feed(rc); r.poll(1);
    for (int i = 0; i < 7; i++) CHECK(p[i].tx == rc);
    CHECK(r.fwdDown() == 1);
    CHECK(r.local().available() == (int)rc.size());   // CRSF-Klasse sieht ihn auch
}

static void testOwnAddressStaysLocal() {
    MultiRouter r; FakeStream up, p1, p2;
    r.begin(&up); r.setPort(0, &p1); r.setPort(3, &p2); r.setOwnAddress(0xCF);
    auto own   = frame(0x2C, {0xCF, 0xEA, 0x01, 0x00});   // PARAM_READ an uns
    auto other = frame(0x2C, {0xCE, 0xEA, 0x01, 0x00});   // an Router 14 weiter hinten
    auto bc    = frame(0x28, {0x00, 0xEA});                // Ping Broadcast
    up.feed(own); up.feed(other); up.feed(bc); r.poll(1);
    CHECK(!contains(p1.tx, own));
    CHECK(contains(p1.tx, other) && contains(p2.tx, other));
    CHECK(contains(p1.tx, bc) && contains(p2.tx, bc));
    CHECK(r.local().available() == (int)(own.size() + other.size() + bc.size()));
}

static void testUpAndNoCrossTalk() {
    MultiRouter r; FakeStream up, p[7];
    r.begin(&up); for (int i = 0; i < 7; i++) r.setPort(i, &p[i]);
    auto tele = frame(0x08, {0, 50, 0, 0, 0, 0, 0, 0});
    p[4].feed(tele); r.poll(1);
    CHECK(up.tx == tele);
    for (int i = 0; i < 7; i++) CHECK(p[i].tx.empty());   // nie zu anderen Ports
    CHECK(r.fwdUp() == 1);
    CHECK(r.portAlive(4, 100) && !r.portAlive(3, 100));
    CHECK(!r.portAlive(4, 5000));
}

static void testCrcAndGarbage() {
    MultiRouter r; FakeStream up, p1;
    r.begin(&up); r.setPort(0, &p1);
    auto bad = rcFrame(); bad.back() ^= 0xFF;
    p1.feed({0x55, 0x12, 0x99}); p1.feed(bad); p1.feed(rcFrame());
    r.poll(1);
    CHECK(r.crcErrors() == 1);
    CHECK(up.tx == rcFrame());
}

static void testDuplicateAddress() {
    MultiRouter r; FakeStream up, p1;
    r.begin(&up); r.setPort(0, &p1); r.setOwnAddress(0xCE);
    p1.feed(frame(0x29, {0xEA, 0xCD, 'X', 0})); r.poll(1);
    CHECK(!r.duplicateAddress());
    p1.feed(frame(0x29, {0xEA, 0xCE, 'X', 0})); r.poll(2);
    CHECK(r.duplicateAddress());
}

// Langsamer Port: nur wenige Bytes Platz. Frames gehen trotzdem ganz und
// in Reihenfolge raus, die anderen Ports bekommen sofort alles.
static void testNonBlockingPump() {
    MultiRouter r; FakeStream up, slow, fast;
    r.begin(&up); r.setPort(0, &slow); r.setPort(1, &fast);
    slow.room = 3;
    auto a = frame(0x2D, {0xC5, 0xEA, 1, 2}), b = frame(0x2D, {0xC5, 0xEA, 3, 4});
    up.feed(a); up.feed(b); r.poll(1);
    CHECK(fast.tx.size() == a.size() + b.size());
    CHECK(slow.tx.size() == 3);
    for (int k = 0; k < 20; k++) { slow.room = 3; r.poll(2 + k); }
    std::vector<uint8_t> ab(a); ab.insert(ab.end(), b.begin(), b.end());
    CHECK(slow.tx == ab);
    CHECK(r.portStats(0).framesOut == 2);
}

// Voll: alter RC-Frame faellt weg, Lua-Antworten bleiben erhalten
static void testQueueOverflow() {
    MultiRouter r; FakeStream up, p1;
    r.begin(&up); r.setPort(0, &p1);
    p1.room = 0;
    auto lua = frame(0x2B, {0xEA, 0xC1, 5, 0, 0});
    up.feed(lua);
    for (int i = 0; i < 12; i++) up.feed(rcFrame());
    r.poll(1);
    CHECK(r.portStats(0).dropped > 0);
    p1.room = 4096; r.poll(2);   // 4096 = unbegrenzt
    CHECK(contains(p1.tx, lua));
    CHECK(p1.tx.size() == (size_t)lua.size() + (MR_QUEUE_DOWN - 1) * rcFrame().size());
}

// CRSF-Klasse an local(): Ping vom Sender -> DEVICE_INFO geht zum Empfaenger
static void testLocalCrsf() {
    MultiRouter r; FakeStream up, p1;
    r.begin(&up); r.setPort(0, &p1); r.setOwnAddress(0xCF);
    CRSF crsf; crsf.attach_stream(&r.local()); crsf.setDeviceAddress(0xCF);
    up.feed(frame(0x28, {0x00, 0xEA})); r.poll(1);
    crsf.read_packets(0);
    CHECK(crsf.getDeviceInfoReplyPending());
    crsf.setDeviceInfoReplyPending(false);
    crsf.send_device_info("CRSF-Router8@15", 26, 30);
    r.poll(2);
    CHECK(up.tx.size() > 6 && up.tx[2] == 0x29 && up.tx[4] == 0xCF);
    CHECK(p1.tx.size() == 6);   // nur der Ping ging nach unten, nicht unsere Antwort
    // Antwort eines Moduls unten und eigene Telemetrie mischen sich nicht
    up.tx.clear();
    p1.feed(frame(0x08, {0, 60, 0, 0, 0, 0, 0, 0}));
    crsf.send_tele_Temp(0, {250});
    r.poll(3);
    // Strom zerlegen: nur ganze, gueltige Frames, beide vorhanden
    std::vector<uint8_t> types;
    size_t i = 0; bool ok = true;
    while (i + 2 <= up.tx.size()) {
        size_t n = up.tx[i + 1] + 2;
        if (i + n > up.tx.size() || crcRef(&up.tx[i + 2], n - 3) != up.tx[i + n - 1]) { ok = false; break; }
        types.push_back(up.tx[i + 2]); i += n;
    }
    CHECK(ok && i == up.tx.size());
    CHECK(types.size() == 2 && types[0] != types[1] && (types[0] == 0x08 || types[0] == 0x0D) && (types[1] == 0x08 || types[1] == 0x0D));
}

static void testConfig() {
    cfgDefaults();
    CHECK(cfg.moduleAddr == 15 && cfg.portBaud[6] == PORT_BAUD_DEFAULT && PORT_BAUD[PORT_BAUD_DEFAULT] == 420000);
    cfg.moduleAddr = 99; cfg.portBaud[2] = 77; cfg.voltCorr = 9; cfg.voltMode = 5;
    cfgSanitize();
    CHECK(cfg.moduleAddr == 15 && cfg.portBaud[2] == PORT_BAUD_DEFAULT && cfg.voltCorr == 0 && cfg.voltMode == VOLT_OFF);
}

int main() {
    testDownBroadcast(); testOwnAddressStaysLocal(); testUpAndNoCrossTalk(); testCrcAndGarbage();
    testDuplicateAddress(); testNonBlockingPump(); testQueueOverflow(); testLocalCrsf(); testConfig();
    printf("%d Pruefungen, %d Fehler\n", checks, fails);
    return fails ? 1 : 0;
}
