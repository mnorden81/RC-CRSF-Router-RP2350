#ifndef ROUTER_CONFIG_H
#define ROUTER_CONFIG_H

/*
 * router_config.h  -  CRSF-Router 8: Einstellungen im Flash (EEPROM-Emulation)
 *
 * Der RP2350 hat kein NVS wie der ESP32. arduino-pico legt dafuer einen
 * 4-KB-Bereich am Ende des Flashs an (EEPROM-Bibliothek). Ein Block mit
 * Kennung, Version und Pruefsumme; passt etwas nicht, gelten die Vorgaben.
 */

#include <stdint.h>
#include "multi_router.h"

// Spannung senden: 0 = Aus, 1 = als RxBt (Batterie-Frame 0x08),
// 2 = als Zellen-Wert (Frame 0x0E, Quelle 8)
static constexpr uint8_t VOLT_OFF   = 0;
static constexpr uint8_t VOLT_RXBT  = 1;
static constexpr uint8_t VOLT_CELLS = 2;

// Baudraten je Port, Index 0 = Port aus
static constexpr uint8_t  PORT_BAUD_COUNT = 6;
static const char* const  PORT_BAUD_OPTIONS = "Aus;57600;115200;400000;420000;921600";
static const uint32_t     PORT_BAUD[PORT_BAUD_COUNT] = {0, 57600, 115200, 400000, 420000, 921600};
static constexpr uint8_t  PORT_BAUD_DEFAULT = 4;   // 420000 (ELRS/Ziege-One)

struct RouterConfig {
    uint8_t moduleAddr;              // WM-Adresse 0-15 -> CRSF 0xC0+Adresse
    uint8_t portBaud[MR_MAX_DOWN];   // Index in PORT_BAUD, 0 = aus
    uint8_t voltMode;                // VOLT_*
    int8_t  voltCorr;                // Korrektur in 0,1 V, -5..+5
    uint8_t tempSend;                // Temperatur als CRSF-Telemetrie (0x0D)
    uint8_t ledRgb;                  // 0 = GRB (Standard WS2812), 1 = RGB
};

extern RouterConfig  cfg;
extern bool          cfgDirty;
extern unsigned long cfgDirtyMs;

void cfgDefaults();
void cfgSanitize();
void cfgLoad();
void cfgSave();
void cfgMarkDirty();

#endif
