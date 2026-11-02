// lua_menu.cpp - RC_Common, siehe lua_menu.h
#include "lua_menu.h"

LuaItem* LuaMenu::add(uint8_t id, uint8_t parent, uint8_t type, const char* name) {
    if (n >= cap) return nullptr;
    LuaItem& it = items[n++];
    it = LuaItem{};
    it.id = id; it.parent = parent; it.type = type; it.name = name;
    it.text = ""; it.unit = "";
    return &it;
}

LuaItem* LuaMenu::folder(uint8_t id, uint8_t parent, const char* name) {
    return add(id, parent, LUA_FOLDER, name);
}

LuaItem* LuaMenu::info(uint8_t id, uint8_t parent, const char* name, const char* text) {
    LuaItem* it = add(id, parent, LUA_INFO, name);
    if (it) it->text = text ? text : "";
    return it;
}

LuaItem* LuaMenu::u8(uint8_t id, uint8_t parent, const char* name, uint8_t min, uint8_t max,
                     const char* unit, LuaGetFn get, LuaSetFn set, uint8_t arg) {
    LuaItem* it = add(id, parent, LUA_UINT8, name);
    if (!it) return nullptr;
    it->min = min; it->max = max; it->unit = unit ? unit : "";
    it->get = get; it->set = set; it->arg = arg;
    return it;
}

LuaItem* LuaMenu::select(uint8_t id, uint8_t parent, const char* name, const char* options,
                         LuaGetFn get, LuaSetFn set, uint8_t arg) {
    LuaItem* it = add(id, parent, LUA_SELECT, name);
    if (!it) return nullptr;
    it->text = options ? options : "";
    it->min = 0;
    it->max = optionCount(it->text) - 1;
    it->get = get; it->set = set; it->arg = arg;
    return it;
}

uint8_t LuaMenu::optionCount(const char* options) {
    if (!options || !*options) return 1;
    uint8_t c = 1;
    for (const char* p = options; *p; ++p) if (*p == ';') c++;
    return c;
}

const LuaItem* LuaMenu::find(uint8_t id) const {
    for (uint8_t i = 0; i < n; i++) if (items[i].id == id) return &items[i];
    return nullptr;
}

uint8_t LuaMenu::maxId() const {
    uint8_t m = 0;
    for (uint8_t i = 0; i < n; i++) if (items[i].id > m) m = items[i].id;
    return m;
}

LuaView LuaMenu::viewOf(const LuaItem& it, char* buf) const {
    LuaView v;
    v.type  = it.type;
    v.name  = it.name ? it.name : "";
    v.text  = it.text ? it.text : "";
    v.value = it.get ? it.get(it.arg) : 0;
    v.min   = it.min;
    v.max   = it.max;
    v.unit  = it.unit ? it.unit : "";
    if (it.view) it.view(it.arg, v, buf);
    return v;
}

bool LuaMenu::send(CRSF& crsf, uint8_t id) const {
    const LuaItem* it = find(id);
    if (!it) return false;
    char buf[LUA_VIEW_BUF] = {0};
    LuaView v = viewOf(*it, buf);
    switch (v.type) {
        case LUA_FOLDER: {
            uint8_t children[CRSF_PACKET_SIZE];
            uint8_t c = 0;
            for (uint8_t i = 0; i < n && c < sizeof(children); i++) {
                const LuaItem& ch = items[i];
                if (ch.parent == it->id && ch.id != it->id) children[c++] = ch.id;
            }
            crsf.send_param_response_CRSF_FOLDER(it->id, it->parent, v.name, children, c);
            break;
        }
        case LUA_INFO:
            crsf.send_param_response_CRSF_INFO(it->id, it->parent, v.name, v.text);
            break;
        case LUA_UINT8: {
            uint8_t val = v.value < v.min ? v.min : (v.value > v.max ? v.max : v.value);
            crsf.send_param_response_CRSF_UINT8(it->id, it->parent, v.name, val, v.min, v.max, v.unit);
            break;
        }
        case LUA_SELECT: {
            uint8_t val = v.value > v.max ? v.max : v.value;
            crsf.send_param_response_CRSF_TEXT_SELECTION(it->id, it->parent, v.name, v.text, val, v.min, v.max);
            break;
        }
        default:
            return false;
    }
    return true;
}

bool LuaMenu::write(uint8_t id, uint8_t value) const {
    const LuaItem* it = find(id);
    if (!it || !it->set) return false;
    char buf[LUA_VIEW_BUF] = {0};
    LuaView v = viewOf(*it, buf);
    if (v.type != LUA_UINT8 && v.type != LUA_SELECT) return false;
    if (value < v.min) value = v.min;
    if (value > v.max) value = v.max;
    it->set(it->arg, value);
    return true;
}
