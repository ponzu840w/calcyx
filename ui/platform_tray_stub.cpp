// platform_tray_stub.cpp — トレイ/ホットキーのスタブ実装
// Web (Emscripten) やトレイ非対応プラットフォーム用。すべて no-op。

#include "platform_tray.h"
#include <FL/Fl.H>

// ---- キー名テーブル (共通) ----

struct KeyEntry { const char *name; int fl_key; };

static const KeyEntry KEY_TABLE[] = {
    {"Space",  ' '},
    {"A", 'a'}, {"B", 'b'}, {"C", 'c'}, {"D", 'd'}, {"E", 'e'},
    {"F", 'f'}, {"G", 'g'}, {"H", 'h'}, {"I", 'i'}, {"J", 'j'},
    {"K", 'k'}, {"L", 'l'}, {"M", 'm'}, {"N", 'n'}, {"O", 'o'},
    {"P", 'p'}, {"Q", 'q'}, {"R", 'r'}, {"S", 's'}, {"T", 't'},
    {"U", 'u'}, {"V", 'v'}, {"W", 'w'}, {"X", 'x'}, {"Y", 'y'},
    {"Z", 'z'},
    {"0", '0'}, {"1", '1'}, {"2", '2'}, {"3", '3'}, {"4", '4'},
    {"5", '5'}, {"6", '6'}, {"7", '7'}, {"8", '8'}, {"9", '9'},
    {"F1",  FL_F+1},  {"F2",  FL_F+2},  {"F3",  FL_F+3},
    {"F4",  FL_F+4},  {"F5",  FL_F+5},  {"F6",  FL_F+6},
    {"F7",  FL_F+7},  {"F8",  FL_F+8},  {"F9",  FL_F+9},
    {"F10", FL_F+10}, {"F11", FL_F+11}, {"F12", FL_F+12},
    {"Tab", FL_Tab},
    {"Escape", FL_Escape},
};

static const int KEY_TABLE_SIZE = sizeof(KEY_TABLE) / sizeof(KEY_TABLE[0]);

// キー名リスト (plat_key_names 用)
static const char *s_key_name_list[sizeof(KEY_TABLE)/sizeof(KEY_TABLE[0]) + 1] = {};
static bool s_key_names_built = false;

static void build_key_names() {
    if (s_key_names_built) return;
    for (int i = 0; i < KEY_TABLE_SIZE; i++)
        s_key_name_list[i] = KEY_TABLE[i].name;
    s_key_name_list[KEY_TABLE_SIZE] = nullptr;
    s_key_names_built = true;
}

// ---- キー名変換 (共通実装) ----

int plat_keyname_to_flkey(const char *name) {
    if (!name) return 0;
    for (int i = 0; i < KEY_TABLE_SIZE; i++) {
        if (strcmp(name, KEY_TABLE[i].name) == 0)
            return KEY_TABLE[i].fl_key;
    }
    return 0;
}

const char *plat_flkey_to_keyname(int fl_key) {
    for (int i = 0; i < KEY_TABLE_SIZE; i++) {
        if (KEY_TABLE[i].fl_key == fl_key)
            return KEY_TABLE[i].name;
    }
    return "Space";
}

const char *const *plat_key_names() {
    build_key_names();
    return s_key_name_list;
}

int plat_key_names_count() {
    return KEY_TABLE_SIZE;
}

// ---- スタブ実装 ----

bool plat_tray_create(void *, const TrayCallbacks &) { return false; }
void plat_tray_destroy() {}
bool plat_tray_is_active() { return false; }

bool plat_hotkey_register(int, int) { return false; }
void plat_hotkey_unregister() {}
void plat_hotkey_poll() {}

void plat_window_toggle(void *, bool) {}
void plat_window_raise(void *) {}
