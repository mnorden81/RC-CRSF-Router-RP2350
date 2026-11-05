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
static constexpr uint8_t  CFG_VERSION = 2;   // v0.2.0: 6 statt 7 Ports
static constexpr size_t   CFG_EEPROM  = 256;

struct CfgBlock {
    uint32_t     magic;
    uint8_t      version;
    uint8_t      size;
    RouterConfig c;
    uint8_t      sum;
};

void cfgDefaults() {
    memset(&cfg, 0, sizeof(cfg));
    cfg.moduleAddr = 15;   // 0xCF, weitere Router 14, 13 ...
    for (uint8_t i = 0; i < MR_MAX_DOWN; i++) cfg.portBaud[i] = PORT_BAUD_DEFAULT;
    cfg.voltMode = VOLT_OFF;
    cfg.voltCorr = 0;
    cfg.tempSend = 0;
    cfg.ledRgb   = 0;
}

void cfgSanitize() {
    if (cfg.moduleAddr > 15) cfg.moduleAddr = 15;
    for (uint8_t i = 0; i < MR_MAX_DOWN; i++)
        if (cfg.portBaud[i] >= PORT_BAUD_COUNT) cfg.portBaud[i] = PORT_BAUD_DEFAULT;
    if (cfg.voltMode > VOLT_CELLS) cfg.voltMode = VOLT_OFF;
    if (cfg.voltCorr < -5 || cfg.voltCorr > 5) cfg.voltCorr = 0;
    cfg.tempSend = cfg.tempSend ? 1 : 0;
    cfg.ledRgb   = cfg.ledRgb ? 1 : 0;
}

#ifdef ARDUINO
static uint8_t checksum(const RouterConfig& c) {
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
