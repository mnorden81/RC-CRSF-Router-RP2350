/*
   RC_CRSF_Router_RP2350 - CRSF-Verteiler mit 7 Ports (Waveshare RP2350-Zero)
   Variante "Router" (7 Ports) oder "Test" (6 Ports): Auswahl in pins.h
   PiperPilot

   Grosser Bruder des RC-CRSF-Router-ESP32-S3: gleiche Regeln, gleiche
   Lua-Bedienung, aber 7 (Test: 6) Ports statt 2 und kein WLAN.

   Der Router haengt an einem freien CRSF-Port des Empfaengers (Uplink) und
   verteilt CRSF auf Port 1-7 (Test: 1-6). An jedem Port: Multiswitch, Soundmodul,
   weiterer Router (Kette) oder Sensor.
     - vom Empfaenger nach unten: alles ausser Anfragen an die eigene Adresse
     - von unten nach oben: alles
     - zwischen den Ports: nie (keine Schleifen)
     - nur vollstaendige Frames mit gueltiger CRC, unveraendert
   Ab v0.4.0 je Port im Lua-Menue:
     - Filter: RC-Kanaele und LinkStats nach unten an/aus
     - Telemetrie nach oben: alle, hoechstens 50 ... 1 Frame/s je Sensor-Art, aus
     - Leitung: Vollduplex (TX+RX) oder Halbduplex (nur TX-Pin, nicht Port 2)
     - Geraeteliste: wer an welchem Port haengt (Ping beim Start und auf Abruf)

   ── Ports (pins.h) ────────────────────────────────────────────────────
   Uplink  = Hardware-UART0 (Serial1), TX GP0 / RX GP1, fest 420000 Baud
   Port 1  = PIO-UART, TX GP2 / RX GP3
   Port 2  = Hardware-UART1 (Serial2), TX GP4 / RX GP5
   Router:  Port 3-7 = PIO-UART: GP6/7, GP8/9, GP10/11, GP12/13, GP14/15
   Test:    Port 3-6 = PIO-UART: GP6/7, GP8/14, GP15/27, GP28/29
            (nur seitliche Stiftleisten, die unteren Pads GP9-13 bleiben frei)
   Je Port Baudrate oder "Aus" im Lua-Menue, gilt nach einem Neustart.

   ── Adresse ───────────────────────────────────────────────────────────
   Eigene WM-Moduladresse 0-15 -> CRSF-Adresse 0xC0+Adresse, Vorgabe 15.
   Mehrere Router im Modell (auch S3-Router) brauchen verschiedene Adressen.
   Meldet ein Geraet weiter hinten dieselbe Adresse, blinkt die LED gelb.

   ── Status-LED (WS2812, GP16) ─────────────────────────────────────────
   gruen langsam      RC-Signal in Ordnung
   rot schnell        kein RC-Signal / Failsafe
   gelb Doppelblitz   Adresskonflikt oder ein Port liess sich nicht starten
   Rot und Gruen vertauscht? Lua "LED Farbfolge" auf RGB stellen.

   ── Telemetrie (beides Vorgabe aus) ───────────────────────────────────
   Spannung (Spannungsteiler an GP26) als RxBt oder Zellen-Wert,
   Chiptemperatur des RP2350 (0x0D).

   ── Arduino IDE ───────────────────────────────────────────────────────
   Boardverwalter-URL:
   https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
   Core "Raspberry Pi Pico/RP2040/RP2350" 6.3.2, Board "Waveshare RP2350 Zero".
   Keine weiteren Bibliotheken.
*/

#include <Arduino.h>
#include "src/RC_Common/CRSF_ESP32/crsf_esp32.h"
#include "src/RC_Common/lua_menu.h"
#include "src/RC_Common/status_led.h"
#include "pins.h"
#include "multi_router.h"
#include "router_config.h"
#include "ws2812.h"
#include "serial_pio_hd.h"

// Version: bei jeder Aenderung hochzaehlen (auch PARAM-Version im DEVICE_INFO)
#define FW_MAJOR 0
#define FW_MINOR 4
#define FW_PATCH 0
static const char*   FW_VERSION       = "0.4.0";
static const uint8_t FW_PARAM_VERSION = FW_MAJOR * 100 + FW_MINOR * 10 + FW_PATCH;   // 40
#if defined(VARIANTE_ROUTER)
static const char*   DEVICE_NAME      = "CRSF-Router8";
#else
static const char*   DEVICE_NAME      = "CRSF-Router8 Test";
#endif
// v0.1.0: erste Firmware: 1 Uplink + 7 Ports, Lua-Menue, Status-LED,
//         Spannung/Temperatur als Telemetrie, Einstellungen im Flash.
// v0.2.0: nur seitliche Stiftleisten: 1 Uplink + 6 Ports (Port 7 entfaellt),
//         Pins Port 4-6 neu, Lua-IDs ab Moduladresse um 1 nach vorn.
// v0.3.0: zwei Varianten in pins.h: "Router" (7 Ports, alle Pins wie v0.1)
//         und "Test" (6 Ports, nur seitliche Stiftleisten wie v0.2).
// v0.4.0: "Version 2" der Funktionen: Filter je Port (RC-Kanaele,
//         LinkStats), Telemetrie-Bremse je Port, Geraeteliste per Ping,
//         Halbduplex je PIO-Port. Lua-Menue neu geordnet (neue IDs).
//         Einstellungen aus v0.3.0 werden uebernommen.

static constexpr uint32_t UPLINK_BAUD = 420000;
static constexpr size_t   RX_FIFO     = 512;   // Software-Empfangspuffer je UART

// Port 1 und 3-7 (Test: 3-6) als PIO-UART (belegen erst bei begin() State-Machines)
static SerialPIO uartPio1(PIN_P1_TX, PIN_P1_RX, RX_FIFO);
static SerialPIO uartPio3(PIN_P3_TX, PIN_P3_RX, RX_FIFO);
static SerialPIO uartPio4(PIN_P4_TX, PIN_P4_RX, RX_FIFO);
static SerialPIO uartPio5(PIN_P5_TX, PIN_P5_RX, RX_FIFO);
static SerialPIO uartPio6(PIN_P6_TX, PIN_P6_RX, RX_FIFO);
#if PORT_COUNT >= 7
static SerialPIO uartPio7(PIN_P7_TX, PIN_P7_RX, RX_FIFO);
#endif
// Index = Port-1; Port 2 (Index 1) ist der Hardware-UART
static SerialPIO* const PIO_PORTS[PORT_COUNT] = {&uartPio1, nullptr, &uartPio3, &uartPio4, &uartPio5, &uartPio6
#if PORT_COUNT >= 7
    , &uartPio7
#endif
};
static_assert(PORT_COUNT <= MR_MAX_DOWN, "MR_MAX_DOWN zu klein");
// TX-Pin je Port (fuer Halbduplex: ein Draht am TX-Pin)
static const uint8_t PORT_TX_PIN[PORT_COUNT] = {PIN_P1_TX, PIN_P2_TX, PIN_P3_TX, PIN_P4_TX, PIN_P5_TX, PIN_P6_TX
#if PORT_COUNT >= 7
    , PIN_P7_TX
#endif
};

// Zustand je Port: 0 aus, 1 laeuft, 2 Start fehlgeschlagen
static constexpr uint8_t PORT_OFF = 0, PORT_RUN = 1, PORT_FAIL = 2;
static uint8_t portState[PORT_COUNT] = {0};

CRSF crsf;
MultiRouter router;

static bool          busOk       = false;
static unsigned long lastFrameMs = 0;
static unsigned long restartAtMs = 0;
static float         rxVolt      = 0.0f;   // Spannung (V), korrigiert
static float         chipTemp    = 0.0f;   // Chiptemperatur (Grad C)

static constexpr unsigned long CRSF_TIMEOUT_MS = 2000;
#define CRSF_SLOT_MS 5
static inline uint8_t  ownAddr()     { return 0xC0 + (cfg.moduleAddr & 0x0F); }
static inline uint16_t slotDelayMs() { return (uint16_t)(cfg.moduleAddr & 0x0F) * 2 * CRSF_SLOT_MS; }
static bool anyPortFailed() {
    for (uint8_t i = 0; i < PORT_COUNT; i++) if (portState[i] == PORT_FAIL) return true;
    return false;
}
static bool configError() { return router.duplicateAddress() || anyPortFailed(); }

static void requestRestart() { cfgSave(); restartAtMs = millis() + 500; if (!restartAtMs) restartAtMs = 1; }

// ======== Ports starten ================================================
static void startPorts() {
    Serial1.setTX(PIN_UPLINK_TX);
    Serial1.setRX(PIN_UPLINK_RX);
    Serial1.setFIFOSize(RX_FIFO);
    Serial1.begin(UPLINK_BAUD);
    router.begin(&Serial1);

    for (uint8_t i = 0; i < PORT_COUNT; i++) {
        uint32_t baud = PORT_BAUD[cfg.portBaud[i]];
        if (baud == 0) { portState[i] = PORT_OFF; continue; }
        if (i == 1) {
            Serial2.setTX(PIN_P2_TX);
            Serial2.setRX(PIN_P2_RX);
            Serial2.setFIFOSize(RX_FIFO);
            Serial2.begin(baud);
            router.setPort(1, &Serial2);
            portState[1] = PORT_RUN;
        } else {
            // Halbduplex: eigener UART mit TX und RX auf dem TX-Pin
            SerialPIO* p = PIO_PORTS[i];
            if (cfg.portLine[i] == LINE_HALF) p = new SerialPIOHD(PORT_TX_PIN[i], RX_FIFO);
            p->begin(baud);
            if (*p) { router.setPort(i, p); portState[i] = PORT_RUN; }
            else    { portState[i] = PORT_FAIL; Serial.printf("Port %u: kein PIO frei\n", i + 1); }
        }
        router.setPortHalfDuplex(i, cfg.portLine[i] == LINE_HALF);
    }
}

// Filter und Telemetrie-Bremse aus den Einstellungen (wirken sofort)
static void applyPortFilters() {
    for (uint8_t i = 0; i < PORT_COUNT; i++) {
        router.setPortFilter(i, cfg.portRc[i], cfg.portLs[i]);
        router.setTelemetry(i, TELE_RATE_MS[cfg.portTele[i]]);
    }
}

// Geraeteliste: Ping an alle Ports kurz nach dem Start (die Geraete
// brauchen etwas zum Hochfahren) und auf Abruf im Lua-Menue
static unsigned long pingAtMs[2] = {2000, 6000};
static void searchDevices() {
    router.clearDevices();
    router.pingPorts();
    pingAtMs[0] = millis() + 1500; if (!pingAtMs[0]) pingAtMs[0] = 1;   // zweiter Ping fuer langsame Geraete
    pingAtMs[1] = 0;
}

// Text fuer die Geraeteliste eines Ports, hoechstens max Zeichen
static void portDevicesText(uint8_t i, char* buf, size_t max) {
    if (portState[i] == PORT_OFF)  { snprintf(buf, max, "aus"); return; }
    if (portState[i] == PORT_FAIL) { snprintf(buf, max, "Fehler"); return; }
    uint8_t n = router.deviceCount(i);
    if (n == 0) { snprintf(buf, max, router.portAlive(i, millis()) ? "aktiv, ohne Namen" : "kein Geraet"); return; }
    size_t k = 0;
    buf[0] = '\0';
    for (uint8_t d = 0; d < n; d++) {
        const char* nm = router.device(i, d).name;
        size_t need = strlen(nm) + (d ? 2 : 0);
        if (k + need >= max) {
            snprintf(buf + k, max - k, " +%u", (unsigned)(n - d));
            return;
        }
        k += snprintf(buf + k, max - k, "%s%s", d ? ", " : "", nm);
    }
}

// ======== Lua-Menue ===================================================
// IDs werden beim Aufbau fortlaufend vergeben (Reihenfolge = Anzeige):
//  1 Version
//  2 Ordner "Ports (nach Neustart)": Port n Baudrate, dann Port n Leitung
//    (nur PIO-Ports, Port 2 ist immer Vollduplex)
//    Ordner "Filter": je Port ein Ordner mit RC-Kanaele, LinkStats, Telemetrie hoch
//    Ordner "Geraete": Suchen, dann je Port die erkannten Geraete
//    Modul Adresse
//    Ordner "Telemetrie": Spannung senden, Korrektur, Spannung, Temperatur senden, Chiptemperatur
//    Ordner "Status": Frames runter/hoch, Verworfen, Gefiltert, CRC-Fehler unten, Adresskonflikt, Ports aktiv
//    LED Farbfolge, Neustart, Geraetename
// Router (7 Ports): 71 Felder, Test (6 Ports): 64 Felder
static constexpr uint8_t LUA_COUNT = 22 + 7 * PORT_COUNT;
static LuaItem luaItems[LUA_COUNT + 1];
static LuaMenu luaMenu(luaItems, sizeof(luaItems) / sizeof(luaItems[0]));

static void luaShowVersion(uint8_t, LuaView& v, char* buf) { snprintf(buf, LUA_VIEW_BUF, "%s %s", FW_VERSION, VARIANTE_NAME); v.text = buf; }
static void luaShowName(uint8_t, LuaView& v, char*)        { v.text = DEVICE_NAME; }

static uint8_t luaGetPort(uint8_t a)            { return cfg.portBaud[a]; }
static void    luaSetPort(uint8_t a, uint8_t v) { cfg.portBaud[a] = v; cfgMarkDirty(); }
static uint8_t luaGetLine(uint8_t a)            { return cfg.portLine[a]; }
static void    luaSetLine(uint8_t a, uint8_t v) { cfg.portLine[a] = v; cfgMarkDirty(); }
static uint8_t luaGetRc(uint8_t a)              { return cfg.portRc[a]; }
static void    luaSetRc(uint8_t a, uint8_t v)   { cfg.portRc[a] = v; applyPortFilters(); cfgMarkDirty(); }
static uint8_t luaGetLs(uint8_t a)              { return cfg.portLs[a]; }
static void    luaSetLs(uint8_t a, uint8_t v)   { cfg.portLs[a] = v; applyPortFilters(); cfgMarkDirty(); }
static uint8_t luaGetTele(uint8_t a)            { return cfg.portTele[a]; }
static void    luaSetTele(uint8_t a, uint8_t v) { cfg.portTele[a] = v; applyPortFilters(); cfgMarkDirty(); }
static uint8_t luaGetAddr(uint8_t)              { return cfg.moduleAddr & 0x0F; }
static void    luaSetAddr(uint8_t, uint8_t v)   { cfg.moduleAddr = v; crsf.setDeviceAddress(ownAddr()); router.setOwnAddress(ownAddr()); cfgMarkDirty(); }

static uint8_t luaGetVMode(uint8_t)             { return cfg.voltMode; }
static void    luaSetVMode(uint8_t, uint8_t v)  { cfg.voltMode = v; cfgMarkDirty(); }
static uint8_t luaGetVCorr(uint8_t)             { return (uint8_t)(cfg.voltCorr + 5); }
static void    luaSetVCorr(uint8_t, uint8_t v)  { cfg.voltCorr = (int8_t)v - 5; cfgMarkDirty(); }
static uint8_t luaGetTSend(uint8_t)             { return cfg.tempSend; }
static void    luaSetTSend(uint8_t, uint8_t v)  { cfg.tempSend = v; cfgMarkDirty(); }
static uint8_t luaGetLed(uint8_t)               { return cfg.ledRgb; }
static void    luaSetLed(uint8_t, uint8_t v)    { cfg.ledRgb = v; cfgMarkDirty(); }
static void luaShowVolt(uint8_t, LuaView& v, char* buf) { snprintf(buf, LUA_VIEW_BUF, "%.2f V", rxVolt); v.text = buf; }
static void luaShowTemp(uint8_t, LuaView& v, char* buf) { snprintf(buf, LUA_VIEW_BUF, "%.1f C", chipTemp); v.text = buf; }
static void luaShowCount(uint8_t a, LuaView& v, char* buf) {
    uint32_t n = a == 0 ? router.fwdDown() : a == 1 ? router.fwdUp() : a == 2 ? router.dropped() :
                 a == 3 ? router.filtered() : router.crcErrors();
    snprintf(buf, LUA_VIEW_BUF, "%lu", (unsigned long)n); v.text = buf;
}
static void luaShowDup(uint8_t, LuaView& v, char*) { v.text = router.duplicateAddress() ? "ja" : "nein"; }
// Je Port ein Zeichen: Nummer = Geraet antwortet, . = still, - = aus, ! = Fehler
static void luaShowPorts(uint8_t, LuaView& v, char* buf) {
    unsigned long now = millis();
    uint8_t k = 0;
    for (uint8_t i = 0; i < PORT_COUNT && (size_t)k + 2 < LUA_VIEW_BUF; i++) {
        char c = portState[i] == PORT_OFF ? '-' : portState[i] == PORT_FAIL ? '!' :
                 router.portAlive(i, now) ? (char)('1' + i) : '.';
        buf[k++] = c;
        buf[k++] = ' ';
    }
    buf[k ? k - 1 : 0] = '\0';
    v.text = buf;
}
// Geraete an Port a. Name + Text muessen in einen Parameter-Frame passen
// (64 Bytes), deshalb hoechstens 40 Zeichen.
static void luaShowDevices(uint8_t a, LuaView& v, char* buf) { portDevicesText(a, buf, 41); v.text = buf; }
static uint8_t luaGetSearch(uint8_t)             { return 0; }
static void    luaSetSearch(uint8_t, uint8_t v)  { if (v) searchDevices(); }
static uint8_t luaGetRestart(uint8_t)            { return restartAtMs ? 1 : 0; }
static void    luaSetRestart(uint8_t, uint8_t v) { if (v) requestRestart(); }

static void buildLuaMenu() {
    static const char* AUS_EIN = "Aus;Ein";
    static const char* PORT_NAME[7] = {"Port 1", "Port 2", "Port 3", "Port 4", "Port 5", "Port 6", "Port 7"};
    static const char* LINE_NAME[7] = {"Port 1 Leitung", "Port 2 Leitung", "Port 3 Leitung", "Port 4 Leitung",
                                       "Port 5 Leitung", "Port 6 Leitung", "Port 7 Leitung"};
    LuaMenu& m = luaMenu;
    uint8_t id = 1;
    m.clear();
    m.folder(0, 0, "");
    m.info(id++, 0, "Version", "")->view = luaShowVersion;

    const uint8_t fPorts = id++;
    m.folder(fPorts, 0, "Ports (nach Neustart)");
    for (uint8_t i = 0; i < PORT_COUNT; i++)
        m.select(id++, fPorts, PORT_NAME[i], PORT_BAUD_OPTIONS, luaGetPort, luaSetPort, i);
    for (uint8_t i = 0; i < PORT_COUNT; i++)
        if (i != PORT_HW_UART) m.select(id++, fPorts, LINE_NAME[i], "Voll (TX+RX);Halb (nur TX)", luaGetLine, luaSetLine, i);

    const uint8_t fFilter = id++;
    m.folder(fFilter, 0, "Filter");
    for (uint8_t i = 0; i < PORT_COUNT; i++) {
        const uint8_t f = id++;
        m.folder(f, fFilter, PORT_NAME[i]);
        m.select(id++, f, "RC-Kanaele runter", AUS_EIN, luaGetRc, luaSetRc, i);
        m.select(id++, f, "LinkStats runter", AUS_EIN, luaGetLs, luaSetLs, i);
        m.select(id++, f, "Telemetrie hoch", TELE_RATE_OPTIONS, luaGetTele, luaSetTele, i);
    }

    const uint8_t fDev = id++;
    m.folder(fDev, 0, "Geraete");
    m.select(id++, fDev, "Geraete suchen", "Nein;Jetzt", luaGetSearch, luaSetSearch);
    for (uint8_t i = 0; i < PORT_COUNT; i++) {
        LuaItem* it = m.info(id++, fDev, PORT_NAME[i], "");
        it->arg = i; it->view = luaShowDevices;
    }

    m.u8(id++, 0, "Modul Adresse", 0, 15, "", luaGetAddr, luaSetAddr);

    const uint8_t fTele = id++;
    m.folder(fTele, 0, "Telemetrie");
    m.select(id++, fTele, "Spannung senden", "Aus;als RxBt;als Zellen", luaGetVMode, luaSetVMode);
    m.select(id++, fTele, "Spannung Korrektur", "-0.5V;-0.4V;-0.3V;-0.2V;-0.1V;0.0V;+0.1V;+0.2V;+0.3V;+0.4V;+0.5V", luaGetVCorr, luaSetVCorr);
    m.info(id++, fTele, "Spannung", "")->view = luaShowVolt;
    m.select(id++, fTele, "Temperatur senden", AUS_EIN, luaGetTSend, luaSetTSend);
    m.info(id++, fTele, "Chiptemperatur", "")->view = luaShowTemp;

    const uint8_t fStat = id++;
    m.folder(fStat, 0, "Status");
    static const char* CNT[5] = {"Frames runter", "Frames hoch", "Verworfen", "Gefiltert", "CRC-Fehler unten"};
    for (uint8_t a = 0; a < 5; a++) { LuaItem* it = m.info(id++, fStat, CNT[a], ""); it->arg = a; it->view = luaShowCount; }
    m.info(id++, fStat, "Adresskonflikt", "")->view = luaShowDup;
    m.info(id++, fStat, "Ports aktiv", "")->view = luaShowPorts;

    m.select(id++, 0, "LED Farbfolge", "GRB;RGB", luaGetLed, luaSetLed);
    m.select(id++, 0, "Neustart", "Nein;Jetzt", luaGetRestart, luaSetRestart);
    m.info(id++, 0, "Geraetename", "")->view = luaShowName;
    if (id - 1 != LUA_COUNT) Serial.printf("Lua: %u Felder statt %u!\n", (unsigned)(id - 1), (unsigned)LUA_COUNT);
}

// ======== Status-LED (Farben) =========================================
// Muster aus RC_Common (statusLedMode/statusLedOn), Farbe hier im Projekt.
static void updateLed() {
    unsigned long now = millis();
    const LinkStatistics& ls = crsf.getLinkStatistics();
    StatusLedInput in = {};
    in.configError   = configError();
    in.crsf          = true;
    in.busOk         = busOk;
    in.lastRcFrameMs = crsf.getLastRcFrameMs();
    in.linkStatsMs   = ls.lastUpdate;
    in.uplinkLQ      = ls.uplinkLQ;
    uint8_t mode = statusLedMode(in, now);
    if (mode == SLED_OFF) mode = SLED_FAST;   // kein Signal: rot schnell statt dunkel
    static uint8_t ledMode = 0xFF;
    static unsigned long start = 0;
    if (mode != ledMode) { ledMode = mode; start = now; }
    uint8_t r = 0, g = 0, b = 0;
    if (mode == SLED_DOUBLE)    { r = 40; g = 28; }   // gelb
    else if (mode == SLED_FAST) { r = 40; }           // rot
    else                        { g = 40; }           // gruen
    bool on = statusLedOn(mode, now - start);
    uint32_t rgb = on ? ((uint32_t)r << 16 | (uint32_t)g << 8 | b) : 0;
    static uint32_t lastRgb = 0xFFFFFFFF;
    static uint8_t  lastOrder = 0xFF;
    if (rgb != lastRgb || cfg.ledRgb != lastOrder) {
        ws2812Write(r * on, g * on, b * on, cfg.ledRgb);
        lastRgb = rgb; lastOrder = cfg.ledRgb;
    }
}

// ======== Telemetrie ==================================================
static void readSensors() {
    static unsigned long last = 0;
    unsigned long now = millis();
    if (now - last < 200) return;
    last = now;
    float v = analogRead(PIN_VSENSE) * 3.3f / 4095.0f * (VSENSE_R_TOP + VSENSE_R_BOTTOM) / VSENSE_R_BOTTOM;
    v += cfg.voltCorr * 0.1f;
    if (v < 0) v = 0;
    rxVolt   = rxVolt == 0.0f ? v : rxVolt * 0.8f + v * 0.2f;   // leicht glaetten
    chipTemp = analogReadTemp();
}

static void sendOwnTelemetry() {
    if (!busOk) return;
    static unsigned long lastV = 0, lastT = 0;
    unsigned long now = millis();
    if (cfg.voltMode != VOLT_OFF && now - lastV >= 500) {
        lastV = now;
        if (cfg.voltMode == VOLT_RXBT) crsf.send_tele_Battery_Sensor((uint16_t)(rxVolt * 10.0f + 0.5f), 0, 0, 0);
        else crsf.send_tele_Voltages(8, {(int32_t)(rxVolt * 1000.0f + 0.5f)});   // Quelle 8 (FLVSS nutzt 1/2)
    }
    if (cfg.tempSend && now - lastT >= 2000) {
        lastT = now;
        crsf.send_tele_Temp(0, {(int32_t)(chipTemp * 10.0f)});   // 0,1 Grad C
    }
}

// Alle 10 s eine Statuszeile auf der USB-Konsole (Serieller Monitor, 115200)
static void printStatus() {
    static unsigned long last = 0;
    unsigned long now = millis();
    if (now - last < 10000) return;
    last = now;
    char ports[LUA_VIEW_BUF];
    LuaView v = {};
    luaShowPorts(0, v, ports);
    Serial.printf("RC %s, LQ %u, runter %lu, hoch %lu, verworfen %lu, gefiltert %lu, CRC %lu, Ports [%s], %.2f V, %.1f C\n",
                  busOk ? "ok" : "fehlt", crsf.getLinkStatistics().uplinkLQ,
                  (unsigned long)router.fwdDown(), (unsigned long)router.fwdUp(),
                  (unsigned long)router.dropped(), (unsigned long)router.filtered(),
                  (unsigned long)router.crcErrors(), ports, rxVolt, chipTemp);
}

// Geraeteliste auf der USB-Konsole, sobald sich etwas aendert
static void printDevices() {
    static uint32_t seen = 0;
    if (router.devicesChanged() == seen) return;
    seen = router.devicesChanged();
    for (uint8_t i = 0; i < PORT_COUNT; i++) {
        if (router.deviceCount(i) == 0) continue;
        Serial.printf("Port %u:", i + 1);
        for (uint8_t d = 0; d < router.deviceCount(i); d++)
            Serial.printf(" %s (0x%02X)", router.device(i, d).name, router.device(i, d).addr);
        Serial.printf("\n");
    }
}

// ======== Setup / Loop =================================================
void setup() {
    Serial.begin(115200);   // USB-Konsole, wartet nicht auf den PC
    ws2812Begin(PIN_STATUS_LED);
    analogReadResolution(12);
    cfgLoad();

    startPorts();
    applyPortFilters();
    crsf.attach_stream(&router.local());
    crsf.setDeviceAddress(ownAddr());
    router.setOwnAddress(ownAddr());

    buildLuaMenu();
    Serial.printf("%s %s (RP2350, Variante %s, %d Ports), Adresse 0x%02X, Lua %u Felder\n",
                  DEVICE_NAME, FW_VERSION, VARIANTE_NAME, PORT_COUNT, ownAddr(), (unsigned)luaMenu.size());
}

void loop() {
    unsigned long now = millis();
    router.poll(now);

    crsf.read_packets(0);
    static uint32_t lastValid = 0;
    uint32_t vf = crsf.getValidFrames();
    if (vf != lastValid) { lastValid = vf; busOk = true; lastFrameMs = now; }
    if (now - lastFrameMs > CRSF_TIMEOUT_MS) busOk = false;

    readSensors();
    sendOwnTelemetry();
    updateLed();
    printStatus();
    printDevices();
    for (uint8_t k = 0; k < 2; k++)
        if (pingAtMs[k] && (long)(now - pingAtMs[k]) >= 0) { pingAtMs[k] = 0; router.pingPorts(); }

    if (crsf.getDeviceInfoReplyPending() && now - crsf.getPingTime() >= slotDelayMs()) {
        crsf.setDeviceInfoReplyPending(false);
        char dn[32];
        snprintf(dn, sizeof(dn), "%s@%d", DEVICE_NAME, cfg.moduleAddr);
        crsf.send_device_info(dn, luaMenu.maxId(), FW_PARAM_VERSION);
    }
    if (crsf.getDeviceReadReplyPending()) {
        uint8_t id = crsf.getParamReadIndex();
        crsf.setDeviceReadReplyPending(false);
        luaMenu.send(crsf, id);
    }
    if (crsf.getDeviceWriteReplyPending()) {
        uint8_t id = crsf.getParamWriteIndex(), v = crsf.getParamWriteValue();
        crsf.setDeviceWriteReplyPending(false);
        luaMenu.write(id, v);
        luaMenu.send(crsf, id);
    }
    // Schaltbefehle (WM-Protokoll) sind fuer die Module weiter hinten, der
    // Router selbst schaltet nichts.
    if (crsf.getDeviceCommandReplyPending()) crsf.setDeviceCommandReplyPending(false);

    if (cfgDirty && now - cfgDirtyMs >= 500) cfgSave();
    if (restartAtMs && (long)(now - restartAtMs) >= 0) rp2040.reboot();
}
