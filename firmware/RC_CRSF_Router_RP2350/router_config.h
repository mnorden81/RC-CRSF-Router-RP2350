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
#include <stddef.h>
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

// Telemetrie nach oben je Port: hoechstens x Frames/s je Sensor-Art
static constexpr uint8_t  TELE_RATE_COUNT = 8;
static const char* const  TELE_RATE_OPTIONS = "Alle;50/s;20/s;10/s;5/s;2/s;1/s;Aus";
static const uint16_t     TELE_RATE_MS[TELE_RATE_COUNT] = {0, 20, 50, 100, 200, 500, 1000, MR_TELE_OFF};


struct RouterConfig {
    uint8_t moduleAddr;              // WM-Adresse 0-15 -> CRSF 0xC0+Adresse
    uint8_t portBaud[MR_MAX_DOWN];   // Index in PORT_BAUD, 0 = aus
    uint8_t voltMode;                // VOLT_*
    int8_t  voltCorr;                // Korrektur in 0,1 V, -5..+5
    uint8_t tempSend;                // Temperatur als CRSF-Telemetrie (0x0D)
    uint8_t ledRgb;                  // 0 = GRB (Standard WS2812), 1 = RGB
    // ab v0.4.0 (Konfig-Version 4)
    uint8_t portRc[MR_MAX_DOWN];     // RC-Kanaele nach unten 0/1
    uint8_t portLs[MR_MAX_DOWN];     // LinkStats nach unten 0/1
    uint8_t portTele[MR_MAX_DOWN];   // Index in TELE_RATE_MS
};

extern RouterConfig  cfg;
extern bool          cfgDirty;
extern unsigned long cfgDirtyMs;

void cfgDefaults();
void cfgSanitize();
void cfgLoad();
void cfgSave();
void cfgMarkDirty();
void cfgFromV3(const uint8_t* raw, size_t n);   // Einstellungen von v0.3.0 uebernehmen

#endif
