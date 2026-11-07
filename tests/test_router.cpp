// tests/test_router.cpp - Host-Tests fuer den 7-Port-Router (Variante Router; Test nutzt 6) (ohne RP2350)
//   sh tests/run_tests.sh
// Prueft die Weiterleitungsregeln, die Warteschlangen, das nicht
// blockierende Senden und die Anbindung der CRSF-Klasse (Lua, Ping).
#include <cstdio>
#include <vector>
#include <string>
#include "src/RC_Common/lua_menu.h"
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

// v0.4.0: Filter nach unten je Port
static void testDownFilter() {
    MultiRouter r; FakeStream up, p1, p2;
    r.begin(&up); r.setPort(0, &p1); r.setPort(1, &p2);
    r.setPortFilter(0, false, true);    // Port 1 ohne RC-Kanaele
    r.setPortFilter(1, true, false);    // Port 2 ohne LinkStats
    auto rc = rcFrame();
    auto ls = frame(0x14, std::vector<uint8_t>(10, 0x22));
    auto ping = frame(0x28, {0x00, 0xEA});
    up.feed(rc); up.feed(ls); up.feed(ping); r.poll(1);
    CHECK(!contains(p1.tx, rc) && contains(p1.tx, ls) && contains(p1.tx, ping));
    CHECK(contains(p2.tx, rc) && !contains(p2.tx, ls) && contains(p2.tx, ping));
    CHECK(r.filtered() == 2);
    CHECK(r.local().available() == (int)(rc.size() + ls.size() + ping.size()));   // CRSF-Klasse sieht alles
}

// v0.4.0: Telemetrie nach oben: aus, Bremse je Sensor-Art, Extended immer
static void testTelemetryBrake() {
    MultiRouter r; FakeStream up, p1, p2;
    r.begin(&up); r.setPort(0, &p1); r.setPort(1, &p2);
    r.setTelemetry(0, 100);            // hoechstens 10/s je Sensor-Art
    r.setTelemetry(1, MR_TELE_OFF);
    auto bat  = frame(0x08, {0, 50, 0, 0, 0, 0, 0, 0});
    auto t1   = frame(0x0D, {1, 0, 200});   // Temperatur Quelle 1
    auto t2   = frame(0x0D, {2, 0, 210});   // Temperatur Quelle 2
    auto info = frame(0x29, {0xEA, 0xC3, 'M', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    p1.feed(bat); p1.feed(bat); p1.feed(t1); p1.feed(t2); r.poll(1000);
    CHECK(up.tx.size() == bat.size() + t1.size() + t2.size());   // zweiter Batterie-Frame gebremst
    up.tx.clear();
    p1.feed(bat); r.poll(1050);  CHECK(up.tx.empty());            // zu frueh
    p1.feed(bat); r.poll(1100);  CHECK(up.tx == bat);             // 100 ms spaeter wieder
    up.tx.clear();
    p2.feed(bat); p2.feed(info); r.poll(2000);
    CHECK(up.tx == info);                                         // Telemetrie aus, DEVICE_INFO geht
    CHECK(r.portStats(1).filtered == 1 && r.portStats(0).filtered == 2);
    r.setTelemetry(0, 0); up.tx.clear();
    p1.feed(bat); p1.feed(bat); r.poll(2001);
    CHECK(up.tx.size() == 2 * bat.size());                        // alle
}

static std::vector<uint8_t> devInfo(uint8_t dest, uint8_t src, const char* name) {
    std::vector<uint8_t> pl{dest, src};
    for (const char* c = name; *c; c++) pl.push_back((uint8_t)*c);
    pl.push_back(0);
    for (int i = 0; i < 14; i++) pl.push_back(0);
    return frame(0x29, pl);
}

// v0.4.0: Geraeteliste, eigener Ping, Antworten an uns bleiben im Router
static void testDeviceList() {
    MultiRouter r; FakeStream up, p[3];
    r.begin(&up); for (int i = 0; i < 3; i++) r.setPort(i, &p[i]);
    r.setOwnAddress(0xCF);
    r.pingPorts(); r.poll(1);
    auto ping = frame(0x28, {0x00, 0xCF});
    for (int i = 0; i < 3; i++) CHECK(p[i].tx == ping);
    CHECK(up.tx.empty());
    uint32_t ch = r.devicesChanged();
    p[0].feed(devInfo(0xCF, 0xC3, "Multiswitch@3"));   // Antwort auf unseren Ping
    p[2].feed(devInfo(0xEA, 0xCE, "CRSF-Router8@14")); // Antwort auf Ping vom Sender
    p[2].feed(devInfo(0xCF, 0xC2, "Sound@2"));         // hinter dem Router 14
    p[2].feed(devInfo(0xCF, 0xC2, "Sound@2"));         // doppelt
    r.poll(2);
    CHECK(r.deviceCount(0) == 1 && r.device(0, 0).addr == 0xC3 && std::string(r.device(0, 0).name) == "Multiswitch@3");
    CHECK(r.deviceCount(1) == 0);
    CHECK(r.deviceCount(2) == 2 && std::string(r.device(2, 1).name) == "Sound@2");
    CHECK(r.devicesChanged() != ch);
    CHECK(up.tx == devInfo(0xEA, 0xCE, "CRSF-Router8@14"));   // nur die an den Sender geht hoch
    CHECK(!r.duplicateAddress());
    r.clearDevices();
    CHECK(r.deviceCount(0) == 0 && r.deviceCount(2) == 0);
}

// v0.4.0: Halbduplex: nicht senden, solange ein Frame hereinkommt;
// halbe Frames verfallen nach MR_PARSER_TIMEOUT_MS
static void testHalfDuplex() {
    MultiRouter r; FakeStream up, p1;
    r.begin(&up); r.setPort(0, &p1); r.setPortHalfDuplex(0, true);
    auto tele = frame(0x08, {0, 50, 0, 0, 0, 0, 0, 0});
    p1.feed(std::vector<uint8_t>(tele.begin(), tele.begin() + 4));   // halber Frame
    up.feed(rcFrame()); r.poll(10);
    CHECK(p1.tx.empty());                                            // wartet
    p1.feed(std::vector<uint8_t>(tele.begin() + 4, tele.end())); r.poll(11);
    CHECK(up.tx == tele && p1.tx == rcFrame());                      // Frame fertig, jetzt senden
    // Stoerung: halber Frame, dann Ruhe -> nach Timeout wieder senden
    p1.tx.clear();
    p1.feed({0xC8, 0x0A, 0x08}); up.feed(rcFrame()); r.poll(20);
    CHECK(p1.tx.empty());
    r.poll(20 + MR_PARSER_TIMEOUT_MS + 1);
    CHECK(p1.tx == rcFrame());
}

static void testConfigV4() {
    cfgDefaults();
    CHECK(cfg.portRc[0] == 1 && cfg.portLs[6] == 1 && cfg.portTele[3] == 0 && cfg.portLine[2] == LINE_FULL);
    cfg.portLine[PORT_HW_UART] = LINE_HALF; cfg.portLine[3] = LINE_HALF; cfg.portTele[0] = 99; cfg.portRc[1] = 7;
    cfgSanitize();
    CHECK(cfg.portLine[PORT_HW_UART] == LINE_FULL && cfg.portLine[3] == LINE_HALF);   // Port 2 nur Vollduplex
    CHECK(cfg.portTele[0] == 0 && cfg.portRc[1] == 1);
    CHECK(TELE_RATE_MS[TELE_RATE_COUNT - 1] == MR_TELE_OFF && LuaMenu::optionCount(TELE_RATE_OPTIONS) == TELE_RATE_COUNT);
    // Einstellungen von v0.3.0 uebernehmen
    uint8_t old[12] = {13, 1, 2, 3, 0, 4, 5, 2, 2, (uint8_t)-3, 1, 1};
    cfgFromV3(old, sizeof(old));
    CHECK(cfg.moduleAddr == 13 && cfg.portBaud[0] == 1 && cfg.portBaud[3] == 0 && cfg.portBaud[6] == 2);
    CHECK(cfg.voltMode == 2 && cfg.voltCorr == -3 && cfg.tempSend == 1 && cfg.ledRgb == 1);
    CHECK(cfg.portRc[4] == 1 && cfg.portLine[4] == LINE_FULL);
}

int main() {
    testDownBroadcast(); testOwnAddressStaysLocal(); testUpAndNoCrossTalk(); testCrcAndGarbage();
    testDuplicateAddress(); testNonBlockingPump(); testQueueOverflow(); testLocalCrsf(); testConfig();
    testDownFilter(); testTelemetryBrake(); testDeviceList(); testHalfDuplex(); testConfigV4();
    printf("%d Pruefungen, %d Fehler\n", checks, fails);
    return fails ? 1 : 0;
}
