// settings_globals.cpp — ユーザー設定の定義と永続化
//
// calcyx.conf はコメント付きの key = value 形式。
// ユーザーがテキストエディタで直接編集できる。
// 設定ダイアログは GUI ヘルパーとしてこのファイルを読み書きする。
//
// init/load/save は SETTINGS_TABLE を駆動する統一ループで実装される。
// conf フォーマットはバイト互換を維持する (セクションヘッダや色エントリ
// の条件出力もテーブル側で表現)。

#include "settings_globals.h"
#include "app_prefs.h"
#include "colors.h"
#include "platform_tray.h"
#include "crash_handler.h"
#include <FL/Fl.H>
#include <cstdio>
#include <cstring>
#include <string>
#include <map>
#include <algorithm>

extern "C" {
#include "types/val.h"
}

// ---- グローバル変数 (初期値は settings_globals.h の DEFAULT_* 定数) ----
int  g_font_id   = DEFAULT_FONT_ID;
int  g_font_size  = DEFAULT_FONT_SIZE;
bool g_input_auto_completion    = DEFAULT_AUTO_COMPLETION;
bool g_input_auto_close_brackets = DEFAULT_AUTO_CLOSE_BRACKETS;
bool g_sep_thousands = DEFAULT_SEP_THOUSANDS;
bool g_sep_hex       = DEFAULT_SEP_HEX;
int  g_limit_max_array_length  = DEFAULT_MAX_ARRAY_LENGTH;
int  g_limit_max_string_length = DEFAULT_MAX_STRING_LENGTH;
int  g_limit_max_call_depth    = DEFAULT_MAX_CALL_DEPTH;
bool g_show_rowlines = DEFAULT_SHOW_ROWLINES;
bool g_remember_position = DEFAULT_REMEMBER_POSITION;
bool g_tray_icon       = DEFAULT_TRAY_ICON;
bool g_hotkey_enabled  = DEFAULT_HOTKEY_ENABLED;
bool g_hotkey_win      = DEFAULT_HOTKEY_WIN;
bool g_hotkey_alt      = DEFAULT_HOTKEY_ALT;
bool g_hotkey_ctrl     = DEFAULT_HOTKEY_CTRL;
bool g_hotkey_shift    = DEFAULT_HOTKEY_SHIFT;
int  g_hotkey_keycode  = DEFAULT_HOTKEY_KEYCODE;

static std::string s_conf_path;

// ---- フォント名 <-> ID 変換 (システムフォント対応) ----
static bool s_sys_fonts_loaded = false;

static void ensure_sys_fonts() {
    if (s_sys_fonts_loaded) return;
    s_sys_fonts_loaded = true;
    Fl::set_fonts(nullptr);
}

static std::string font_id_to_name(Fl_Font id) {
    ensure_sys_fonts();
    int attr = 0;
    const char *name = Fl::get_font_name(id, &attr);
    if (name && name[0]) return name;
    return "Courier";
}

static Fl_Font font_name_to_id(const std::string &name) {
    ensure_sys_fonts();
    Fl_Font n = Fl::set_fonts(nullptr);
    for (Fl_Font i = 0; i < n; i++) {
        int attr = 0;
        const char *fn = Fl::get_font_name(i, &attr);
        if (fn && name == fn) return i;
    }
    return FL_COURIER;
}

// ---- Fl_Color <-> hex string 変換 ----
static std::string color_to_hex(Fl_Color c) {
    uchar r, g, b;
    Fl::get_color(c, r, g, b);
    char buf[8];
    snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
    return buf;
}

static Fl_Color hex_to_color(const std::string &s, Fl_Color def) {
    if (s.size() != 7 || s[0] != '#') return def;
    unsigned r, g, b;
    if (sscanf(s.c_str(), "#%02X%02X%02X", &r, &g, &b) != 3) return def;
    return fl_rgb_color((uchar)r, (uchar)g, (uchar)b);
}

// ---- パス ----
static void ensure_path() {
    if (!s_conf_path.empty()) return;
    std::string dir = AppPrefs::config_dir();
#if defined(_WIN32)
    s_conf_path = dir + "\\calcyx.conf";
#else
    s_conf_path = dir + "/calcyx.conf";
#endif
}

const char *settings_path() {
    ensure_path();
    return s_conf_path.c_str();
}

// --- 設定スキーマ ---
//
// SETTINGS_TABLE は save 順に並んだエントリリスト。section_header が
// 非 NULL のエントリは直前に空行 + そのヘッダブロックを出力する。
// key が NULL のエントリはセクションコメント区切り専用 (値なし)。

namespace {

enum Kind {
    K_BOOL,
    K_INT,
    K_FONT,        // target=int*, s_def=default font name
    K_HOTKEY_KEY,  // target=int*, s_def=default key name ("Space")
    K_COLOR_PRESET,
    K_COLOR,       // target=Fl_Color* (user-defined 時のみ save)
    K_SECTION,     // 出力専用: 空行 + section header コメント
};

struct Desc {
    Kind         kind;
    const char  *key;       // conf key; K_SECTION では NULL
    void        *target;    // K_SECTION では NULL
    int          i_def;     // K_INT, K_FONT, K_HOTKEY_KEY, K_COLOR_PRESET
    int          i_lo;      // K_INT の下限 (K_FONT 等では 0)
    int          i_hi;      // K_INT の上限
    bool         b_def;     // K_BOOL
    const char  *s_def;     // K_FONT, K_HOTKEY_KEY
    const char  *section;   // K_SECTION の複数行ヘッダ ("# ---- XXX ----\n"+追加コメント)
    // K_COLOR: target は Fl_Color*, 「def ポインタ」は CalcyxColors 内の対応メンバへのオフセット相当を
    // 実装側で def_color(key) 関数で個別計算する。
};

// ヘルパー: エントリ作成マクロ
#define SEC(hdr) { K_SECTION, nullptr, nullptr, 0,0,0, false, nullptr, hdr }
#define BOOLE(k, t, d) { K_BOOL, k, &t, 0,0,0, d, nullptr, nullptr }
#define INTC(k, t, d, lo, hi) { K_INT, k, &t, d, lo, hi, false, nullptr, nullptr }
#define FONT(k, t, d)  { K_FONT, k, &t, 0,0,0, false, d, nullptr }
#define HKEY(k, t, d)  { K_HOTKEY_KEY, k, &t, 0,0,0, false, d, nullptr }
#define PRESET(k, t, d) { K_COLOR_PRESET, k, &t, d, 0,0, false, nullptr, nullptr }
#define COLOR(k, t)    { K_COLOR, k, &t, 0,0,0, false, nullptr, nullptr }

const Desc SETTINGS_TABLE[] = {
    SEC("# ---- Font ----\n"
        "# Any font name installed on the system (e.g. monospace, DejaVu Sans Mono)\n"),
    FONT("font", g_font_id, "Courier"),
    INTC("font_size", g_font_size, DEFAULT_FONT_SIZE, 8, 36),

    SEC("# ---- General ----\n"),
    BOOLE("remember_position", g_remember_position, DEFAULT_REMEMBER_POSITION),
    BOOLE("show_rowlines", g_show_rowlines, DEFAULT_SHOW_ROWLINES),
    INTC("max_array_length",  g_limit_max_array_length,  DEFAULT_MAX_ARRAY_LENGTH,  1, 1000000),
    INTC("max_string_length", g_limit_max_string_length, DEFAULT_MAX_STRING_LENGTH, 1, 1000000),
    INTC("max_call_depth",    g_limit_max_call_depth,    DEFAULT_MAX_CALL_DEPTH,    1, 1000),

    SEC("# ---- Input ----\n"),
    BOOLE("auto_completion",     g_input_auto_completion,     DEFAULT_AUTO_COMPLETION),
    BOOLE("auto_close_brackets", g_input_auto_close_brackets, DEFAULT_AUTO_CLOSE_BRACKETS),

    SEC("# ---- Number Format ----\n"),
    INTC("decimal_digits",  g_fmt_settings.decimal_len,    DEFAULT_FMT_DECIMAL_LEN,    1, 34),
    BOOLE("e_notation",     g_fmt_settings.e_notation,     DEFAULT_FMT_E_NOTATION),
    INTC("e_positive_min",  g_fmt_settings.e_positive_min, DEFAULT_FMT_E_POSITIVE_MIN,   1, 30),
    INTC("e_negative_max",  g_fmt_settings.e_negative_max, DEFAULT_FMT_E_NEGATIVE_MAX, -30, -1),
    BOOLE("e_alignment",    g_fmt_settings.e_alignment,    DEFAULT_FMT_E_ALIGNMENT),
    BOOLE("thousands_separator", g_sep_thousands, DEFAULT_SEP_THOUSANDS),
    BOOLE("hex_separator",       g_sep_hex,       DEFAULT_SEP_HEX),

    SEC("# ---- System Tray ----\n"),
    BOOLE("tray_icon",      g_tray_icon,      DEFAULT_TRAY_ICON),
    BOOLE("hotkey_enabled", g_hotkey_enabled, DEFAULT_HOTKEY_ENABLED),
    BOOLE("hotkey_win",     g_hotkey_win,     DEFAULT_HOTKEY_WIN),
    BOOLE("hotkey_alt",     g_hotkey_alt,     DEFAULT_HOTKEY_ALT),
    BOOLE("hotkey_ctrl",    g_hotkey_ctrl,    DEFAULT_HOTKEY_CTRL),
    BOOLE("hotkey_shift",   g_hotkey_shift,   DEFAULT_HOTKEY_SHIFT),
    HKEY ("hotkey_key",     g_hotkey_keycode, "Space"),

    SEC("# ---- Colors ----\n"
        "# Preset: otaku-black, gyakubari-white, saboten-grey, saboten-white, user\n"),
    PRESET("color_preset", g_color_preset, DEFAULT_COLOR_PRESET),

    // ---- user-defined 時のみ出力される色エントリ ----
    COLOR("color_bg",       g_colors.bg),
    COLOR("color_sel_bg",   g_colors.sel_bg),
    COLOR("color_rowline",  g_colors.rowline),
    COLOR("color_sep",      g_colors.sep),
    COLOR("color_text",     g_colors.text),
    COLOR("color_cursor",   g_colors.cursor),
    COLOR("color_symbol",   g_colors.symbol),
    COLOR("color_ident",    g_colors.ident),
    COLOR("color_special",  g_colors.special),
    COLOR("color_si_pfx",   g_colors.si_pfx),
    COLOR("color_paren0",   g_colors.paren[0]),
    COLOR("color_paren1",   g_colors.paren[1]),
    COLOR("color_paren2",   g_colors.paren[2]),
    COLOR("color_paren3",   g_colors.paren[3]),
    COLOR("color_error",    g_colors.error),
    COLOR("color_ui_win_bg", g_colors.ui_win_bg),
    COLOR("color_ui_bg",    g_colors.ui_bg),
    COLOR("color_ui_input", g_colors.ui_input),
    COLOR("color_ui_btn",   g_colors.ui_btn),
    COLOR("color_ui_menu",  g_colors.ui_menu),
    COLOR("color_ui_text",  g_colors.ui_text),
    COLOR("color_ui_label", g_colors.ui_label),
    COLOR("color_ui_dim",   g_colors.ui_dim),
    COLOR("color_pop_bg",   g_colors.pop_bg),
    COLOR("color_pop_sel",  g_colors.pop_sel),
    COLOR("color_pop_text", g_colors.pop_text),
    COLOR("color_pop_desc", g_colors.pop_desc),
    COLOR("color_pop_desc_bg", g_colors.pop_desc_bg),
    COLOR("color_pop_border",  g_colors.pop_border),
};

#undef SEC
#undef BOOLE
#undef INTC
#undef FONT
#undef HKEY
#undef PRESET
#undef COLOR

constexpr size_t SETTINGS_TABLE_N = sizeof(SETTINGS_TABLE)/sizeof(SETTINGS_TABLE[0]);

// K_COLOR エントリに対応する CalcyxColors のデフォルトメンバ値を返す。
// SETTINGS_TABLE の定義順と一致するよう手動で対応付ける。
Fl_Color color_default(const char *key, const CalcyxColors &def) {
    struct { const char *k; Fl_Color CalcyxColors::*m; } simple[] = {
        {"color_bg",       &CalcyxColors::bg},
        {"color_sel_bg",   &CalcyxColors::sel_bg},
        {"color_rowline",  &CalcyxColors::rowline},
        {"color_sep",      &CalcyxColors::sep},
        {"color_text",     &CalcyxColors::text},
        {"color_cursor",   &CalcyxColors::cursor},
        {"color_symbol",   &CalcyxColors::symbol},
        {"color_ident",    &CalcyxColors::ident},
        {"color_special",  &CalcyxColors::special},
        {"color_si_pfx",   &CalcyxColors::si_pfx},
        {"color_error",    &CalcyxColors::error},
        {"color_ui_win_bg",&CalcyxColors::ui_win_bg},
        {"color_ui_bg",    &CalcyxColors::ui_bg},
        {"color_ui_input", &CalcyxColors::ui_input},
        {"color_ui_btn",   &CalcyxColors::ui_btn},
        {"color_ui_menu",  &CalcyxColors::ui_menu},
        {"color_ui_text",  &CalcyxColors::ui_text},
        {"color_ui_label", &CalcyxColors::ui_label},
        {"color_ui_dim",   &CalcyxColors::ui_dim},
        {"color_pop_bg",   &CalcyxColors::pop_bg},
        {"color_pop_sel",  &CalcyxColors::pop_sel},
        {"color_pop_text", &CalcyxColors::pop_text},
        {"color_pop_desc", &CalcyxColors::pop_desc},
        {"color_pop_desc_bg", &CalcyxColors::pop_desc_bg},
        {"color_pop_border",  &CalcyxColors::pop_border},
    };
    for (auto &e : simple) {
        if (strcmp(e.k, key) == 0) return def.*(e.m);
    }
    if (strcmp(key, "color_paren0") == 0) return def.paren[0];
    if (strcmp(key, "color_paren1") == 0) return def.paren[1];
    if (strcmp(key, "color_paren2") == 0) return def.paren[2];
    if (strcmp(key, "color_paren3") == 0) return def.paren[3];
    return FL_BLACK;
}

} // namespace

// ---- conf ファイル読み込み ----
static std::map<std::string, std::string> read_conf() {
    ensure_path();
    std::map<std::string, std::string> kv;
    FILE *fp = fopen(s_conf_path.c_str(), "r");
    if (!fp) return kv;
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';
        if (line[0] == '#' || line[0] == '\0') continue;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        // key の末尾スペースを除去
        char *ke = eq - 1;
        while (ke >= line && (*ke == ' ' || *ke == '\t')) ke--;
        std::string key(line, ke - line + 1);
        // value の先頭スペースを除去
        const char *vs = eq + 1;
        while (*vs == ' ' || *vs == '\t') vs++;
        kv[key] = vs;
    }
    fclose(fp);
    return kv;
}

static std::string map_get(const std::map<std::string, std::string> &kv,
                           const char *key, const char *def) {
    auto it = kv.find(key);
    return (it != kv.end()) ? it->second : def;
}

static int map_get_int(const std::map<std::string, std::string> &kv,
                       const char *key, int def) {
    auto it = kv.find(key);
    return (it != kv.end()) ? atoi(it->second.c_str()) : def;
}

static bool map_get_bool(const std::map<std::string, std::string> &kv,
                         const char *key, bool def) {
    auto it = kv.find(key);
    if (it == kv.end()) return def;
    const std::string &v = it->second;
    return v == "true" || v == "1" || v == "yes";
}

static void update_crash_config_snapshot() {
    char buf[800];
    snprintf(buf, sizeof(buf),
        "font_size=%d, sep_thousands=%d, sep_hex=%d\n"
        "decimal_digits=%d, e_notation=%d, e_pos_min=%d, e_neg_max=%d, e_align=%d\n"
        "auto_completion=%d, auto_close_brackets=%d\n"
        "max_array=%d, max_string=%d, max_depth=%d",
        g_font_size, g_sep_thousands, g_sep_hex,
        g_fmt_settings.decimal_len, g_fmt_settings.e_notation,
        g_fmt_settings.e_positive_min, g_fmt_settings.e_negative_max,
        g_fmt_settings.e_alignment,
        g_input_auto_completion, g_input_auto_close_brackets,
        g_limit_max_array_length, g_limit_max_string_length,
        g_limit_max_call_depth);
    crash_handler_save_config(buf);
}

// ---- スカラー init (K_COLOR / K_SECTION 以外) ----
static void apply_default_one(const Desc &d) {
    switch (d.kind) {
    case K_BOOL:   *(bool *)d.target = d.b_def; break;
    case K_INT:    *(int  *)d.target = d.i_def; break;
    case K_FONT:   *(int  *)d.target = DEFAULT_FONT_ID; break;
    case K_HOTKEY_KEY: *(int *)d.target = DEFAULT_HOTKEY_KEYCODE; break;
    case K_COLOR_PRESET: *(int *)d.target = d.i_def; break;
    case K_COLOR:
    case K_SECTION:
        break;
    }
}

void settings_init_defaults() {
    for (const Desc &d : SETTINGS_TABLE) apply_default_one(d);
    update_crash_config_snapshot();
}

// ---- load ----
void settings_load() {
    auto kv = read_conf();
    if (kv.empty()) return;

    for (const Desc &d : SETTINGS_TABLE) {
        switch (d.kind) {
        case K_BOOL:
            *(bool *)d.target = map_get_bool(kv, d.key, d.b_def);
            break;
        case K_INT: {
            int v = map_get_int(kv, d.key, d.i_def);
            *(int *)d.target = std::clamp(v, d.i_lo, d.i_hi);
            break;
        }
        case K_FONT:
            *(int *)d.target = font_name_to_id(map_get(kv, d.key, d.s_def));
            break;
        case K_HOTKEY_KEY: {
            std::string kn = map_get(kv, d.key, d.s_def);
            int k = plat_keyname_to_flkey(kn.c_str());
            *(int *)d.target = k ? k : DEFAULT_HOTKEY_KEYCODE;
            break;
        }
        case K_COLOR_PRESET: {
            std::string ps = map_get(kv, d.key, COLOR_PRESET_INFO[d.i_def].id);
            int idx = d.i_def;
            for (int i = 0; i < COLOR_PRESET_COUNT; i++) {
                if (ps == COLOR_PRESET_INFO[i].id) { idx = i; break; }
            }
            *(int *)d.target = idx;
            break;
        }
        case K_COLOR:
        case K_SECTION:
            break;
        }
    }

    // 色: プリセット確定後に user-defined なら各 color_* を読む
    if (g_color_preset == COLOR_PRESET_USER_DEFINED) {
        CalcyxColors def;
        colors_init_defaults(&def);
        for (const Desc &d : SETTINGS_TABLE) {
            if (d.kind != K_COLOR) continue;
            Fl_Color fallback = color_default(d.key, def);
            *(Fl_Color *)d.target = hex_to_color(map_get(kv, d.key, ""), fallback);
        }
    } else {
        colors_init_preset(&g_colors, g_color_preset);
    }

    update_crash_config_snapshot();
}

// ---- save ----
void settings_save() {
    ensure_path();
    FILE *fp = fopen(s_conf_path.c_str(), "w");
    if (!fp) return;

    fputs("# calcyx user settings\n"
          "# Changes take effect on next launch, or immediately via Preferences dialog.\n"
          "# This file is regenerated when saving from the Preferences dialog.\n"
          "# Custom comments will not be preserved.\n", fp);

    bool user_colors = (g_color_preset == COLOR_PRESET_USER_DEFINED);
    for (const Desc &d : SETTINGS_TABLE) {
        switch (d.kind) {
        case K_SECTION:
            fputc('\n', fp);
            fputs(d.section, fp);
            break;
        case K_BOOL:
            fprintf(fp, "%s = %s\n", d.key, *(bool *)d.target ? "true" : "false");
            break;
        case K_INT:
            fprintf(fp, "%s = %d\n", d.key, *(int *)d.target);
            break;
        case K_FONT:
            fprintf(fp, "%s = %s\n", d.key, font_id_to_name(*(int *)d.target).c_str());
            break;
        case K_HOTKEY_KEY:
            fprintf(fp, "%s = %s\n", d.key, plat_flkey_to_keyname(*(int *)d.target));
            break;
        case K_COLOR_PRESET:
            fprintf(fp, "%s = %s\n", d.key, COLOR_PRESET_INFO[*(int *)d.target].id);
            break;
        case K_COLOR:
            if (user_colors) {
                fprintf(fp, "%s = %s\n", d.key,
                        color_to_hex(*(Fl_Color *)d.target).c_str());
            }
            break;
        }
    }

    fclose(fp);
    update_crash_config_snapshot();
}
