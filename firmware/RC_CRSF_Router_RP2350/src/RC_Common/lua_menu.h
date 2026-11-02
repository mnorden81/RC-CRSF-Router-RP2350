// lua_menu.h - RC_Common: CRSF-Parametermenue (Lua/TBS Agent) als Tabelle
//
// Vorbild ist das Parametermenue im G030-Multiswitch (WMuCpp): jeder Eintrag
// steht einmal in einer Tabelle mit Feld-ID, Elternordner, Typ, Name,
// Optionen, Grenzen und Lese-/Schreibfunktion. Senden und Schreiben laufen
// generisch ueber diese Tabelle, die Kinderlisten der Ordner ergeben sich
// aus den Elternangaben (Reihenfolge = Reihenfolge des Einfuegens).
//
// Feld-IDs bleiben die bisherigen Nummern der Projekte, damit gespeicherte
// Sender-Profile und Lua-Skripte weiterlaufen.
//
// Laeuft auch in den Host-Tests (tests/), braucht nur CRSF_ESP32.
#ifndef RC_LUA_MENU_H
#define RC_LUA_MENU_H

#include "CRSF_ESP32/crsf_esp32.h"

// Typen (Konstanten statt enum, siehe Hinweis in uart_roles.h)
static constexpr uint8_t LUA_FOLDER = 0;
static constexpr uint8_t LUA_INFO   = 1;
static constexpr uint8_t LUA_UINT8  = 2;
static constexpr uint8_t LUA_SELECT = 3;

// Was beim Senden tatsaechlich rausgeht. view() darf jedes Feld aendern,
// z. B. Name oder Grenzen je nach Zustand, oder aus einer Zahl eine Info
// machen ("Deaktiviert").
struct LuaView {
    uint8_t     type;
    const char* name;
    const char* text;    // LUA_SELECT: Optionen "a;b;c", LUA_INFO: Text
    uint8_t     value;
    uint8_t     min;
    uint8_t     max;
    const char* unit;
};

typedef uint8_t (*LuaGetFn)(uint8_t arg);
typedef void    (*LuaSetFn)(uint8_t arg, uint8_t value);
// buf (LUA_VIEW_BUF Bytes) darf fuer einen zusammengesetzten Namen/Text benutzt werden
static constexpr size_t LUA_VIEW_BUF = 48;
typedef void    (*LuaViewFn)(uint8_t arg, LuaView& v, char* buf);

struct LuaItem {
    uint8_t     id;
    uint8_t     parent;
    uint8_t     type;
    uint8_t     arg;       // wird an get/set/view durchgereicht (z. B. Ausgang-Nr.)
    const char* name;
    const char* text;      // Optionen bzw. Info-Text
    uint8_t     min;
    uint8_t     max;
    const char* unit;
    LuaGetFn    get;
    LuaSetFn    set;       // nullptr = nur lesen
    LuaViewFn   view;      // nullptr = Felder wie eingetragen
};

class LuaMenu {
public:
    LuaMenu(LuaItem* storage, uint8_t capacity) : items(storage), cap(capacity) {}

    void clear() { n = 0; }

    // Eintraege anlegen. Rueckgabe zum Nachtragen von arg/view, nullptr wenn voll.
    LuaItem* folder(uint8_t id, uint8_t parent, const char* name);
    LuaItem* info(uint8_t id, uint8_t parent, const char* name, const char* text);
    LuaItem* u8(uint8_t id, uint8_t parent, const char* name, uint8_t min, uint8_t max,
                const char* unit, LuaGetFn get, LuaSetFn set, uint8_t arg = 0);
    // max = Anzahl Optionen - 1
    LuaItem* select(uint8_t id, uint8_t parent, const char* name, const char* options,
                    LuaGetFn get, LuaSetFn set, uint8_t arg = 0);

    const LuaItem* find(uint8_t id) const;
    // Eintrag senden; false = unbekannte ID (nichts gesendet)
    bool send(CRSF& crsf, uint8_t id) const;
    // Wert schreiben (auf min/max der aktuellen Ansicht begrenzt);
    // false = unbekannt, Ordner, Info oder nur lesbar
    bool write(uint8_t id, uint8_t value) const;

    // Hoechste Feld-ID = "Parameter Count" im DEVICE_INFO-Frame
    uint8_t maxId() const;
    uint8_t size() const { return n; }

    // Aktuelle Ansicht eines Eintrags (fuer send/write und Tests)
    LuaView viewOf(const LuaItem& it, char* buf) const;

    static uint8_t optionCount(const char* options);

private:
    LuaItem* add(uint8_t id, uint8_t parent, uint8_t type, const char* name);
    LuaItem* items;
    uint8_t  cap;
    uint8_t  n = 0;
};

#endif
