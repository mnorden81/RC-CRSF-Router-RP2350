// router_config.cpp - CRSF-Router 8, siehe router_config.h
#include "router_config.h"
#include <string.h>
#ifdef ARDUINO
#include <Arduino.h>
#include <EEPROM.h>
#endif

RouterConfig  cfg;
bool          cfgDirty   = false;
unsigned long cfgDirtyMs = 0;

static constexpr uint32_t CFG_MAGIC   = 0x38525352;   // "RSR8"
static constexpr uint8_t  CFG_VERSION = 4;   // v0.4.0: Filter, Telemetrie-Bremse, Halbduplex je Port
// v0.3.0 (Version 3): wird beim ersten Start uebernommen, nichts geht verloren
static constexpr size_t   CFG_EEPROM  = 256;

template <typename C>
struct CfgBlockT {
    uint32_t magic;
    uint8_t  version;
    uint8_t  size;
    C        c;
    uint8_t  sum;
};
typedef CfgBlockT<RouterConfig> CfgBlock;

// Aufbau der Einstellungen bis v0.3.0
struct RouterConfigV3 {
    uint8_t moduleAddr;
    uint8_t portBaud[MR_MAX_DOWN];
    uint8_t voltMode;
    int8_t  voltCorr;
    uint8_t tempSend;
    uint8_t ledRgb;
};

void cfgDefaults() {
    memset(&cfg, 0, sizeof(cfg));
    cfg.moduleAddr = 15;   // 0xCF, weitere Router 14, 13 ...
    for (uint8_t i = 0; i < MR_MAX_DOWN; i++) cfg.portBaud[i] = PORT_BAUD_DEFAULT;
    cfg.voltMode = VOLT_OFF;
    cfg.voltCorr = 0;
    cfg.tempSend = 0;
    cfg.ledRgb   = 0;
    for (uint8_t i = 0; i < MR_MAX_DOWN; i++) {
        cfg.portLine[i] = LINE_FULL;
        cfg.portRc[i]   = 1;
        cfg.portLs[i]   = 1;
        cfg.portTele[i] = 0;   // alle
    }
}

void cfgSanitize() {
    if (cfg.moduleAddr > 15) cfg.moduleAddr = 15;
    for (uint8_t i = 0; i < MR_MAX_DOWN; i++)
        if (cfg.portBaud[i] >= PORT_BAUD_COUNT) cfg.portBaud[i] = PORT_BAUD_DEFAULT;
    if (cfg.voltMode > VOLT_CELLS) cfg.voltMode = VOLT_OFF;
    if (cfg.voltCorr < -5 || cfg.voltCorr > 5) cfg.voltCorr = 0;
    cfg.tempSend = cfg.tempSend ? 1 : 0;
    cfg.ledRgb   = cfg.ledRgb ? 1 : 0;
    for (uint8_t i = 0; i < MR_MAX_DOWN; i++) {
        cfg.portLine[i] = (cfg.portLine[i] == LINE_HALF && i != PORT_HW_UART) ? LINE_HALF : LINE_FULL;
        cfg.portRc[i]   = cfg.portRc[i] ? 1 : 0;
        cfg.portLs[i]   = cfg.portLs[i] ? 1 : 0;
        if (cfg.portTele[i] >= TELE_RATE_COUNT) cfg.portTele[i] = 0;
    }
}

// Einstellungen aus v0.3.0 uebernehmen, neue Felder auf Vorgabe
void cfgFromV3(const uint8_t* raw, size_t n) {
    cfgDefaults();
    if (n < sizeof(RouterConfigV3)) return;
    RouterConfigV3 o;
    memcpy(&o, raw, sizeof(o));
    cfg.moduleAddr = o.moduleAddr;
    memcpy(cfg.portBaud, o.portBaud, sizeof(cfg.portBaud));
    cfg.voltMode = o.voltMode;
    cfg.voltCorr = o.voltCorr;
    cfg.tempSend = o.tempSend;
    cfg.ledRgb   = o.ledRgb;
    cfgSanitize();
}

#ifdef ARDUINO
template <typename C>
static uint8_t checksum(const C& c) {
    const uint8_t* p = (const uint8_t*)&c;
    uint8_t s = 0x5A;
    for (size_t i = 0; i < sizeof(c); i++) s = (uint8_t)((s << 1 | s >> 7) ^ p[i]);
    return s;
}

void cfgLoad() {
    EEPROM.begin(CFG_EEPROM);
    CfgBlock b;
    EEPROM.get(0, b);
    if (b.magic == CFG_MAGIC && b.version == CFG_VERSION && b.size == sizeof(RouterConfig) &&
        b.sum == checksum(b.c)) {
        cfg = b.c;
        cfgSanitize();
        return;
    }
    CfgBlockT<RouterConfigV3> o;
    EEPROM.get(0, o);
    if (o.magic == CFG_MAGIC && o.version == 3 && o.size == sizeof(RouterConfigV3) &&
        o.sum == checksum(o.c)) {
        cfgFromV3((const uint8_t*)&o.c, sizeof(o.c));
        cfgSave();
    } else {
        cfgDefaults();
        cfgSave();
    }
}

// Schreibt nur, wenn sich etwas geaendert hat (EEPROM.commit() prueft das
// selbst nicht und loescht sonst bei jedem Aufruf einen Flash-Sektor).
// Waehrend des Schreibens (einige ms) gehen Frames verloren - deshalb nur
// nach Aenderungen im Lua-Menue, nie zyklisch.
void cfgSave() {
    cfgSanitize();
    CfgBlock b;
    memset(&b, 0, sizeof(b));
    b.magic = CFG_MAGIC; b.version = CFG_VERSION; b.size = sizeof(RouterConfig);
    b.c = cfg; b.sum = checksum(cfg);
    CfgBlock old;
    EEPROM.get(0, old);
    cfgDirty = false;
    if (memcmp(&old, &b, sizeof(b)) == 0) return;
    EEPROM.put(0, b);
    EEPROM.commit();
}

void cfgMarkDirty() { cfgDirty = true; cfgDirtyMs = millis(); if (!cfgDirtyMs) cfgDirtyMs = 1; }
#endif
