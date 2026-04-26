// settings_globals.cpp — ユーザー設定の定義と永続化 (GUI 側)
//
// calcyx.conf はコメント付きの key = value 形式。ユーザーがテキストエディタで
// 直接編集できる。設定ダイアログは GUI ヘルパーとしてこのファイルを読み書きする。
//
// スキーマ (key 名・並び順・kind・デフォルト・範囲) は shared/settings_schema.c
// に集約されている。本ファイルは GUI 側の「キー → グローバル変数ポインタ」
// マッピングと、FLTK の Fl_Color / フォント ID 変換だけを担当する。

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
#include "settings_schema.h"
#include "settings_writer.h"
}

// ---- グローバル変数 (初期値は settings_globals.h の DEFAULT_* 定数) ----
int  g_font_id   = DEFAULT_FONT_ID;
int  g_font_size  = DEFAULT_FONT_SIZE;
bool g_input_auto_completion    = DEFAULT_AUTO_COMPLETION;
bool g_input_auto_close_brackets = DEFAULT_AUTO_CLOSE_BRACKETS;
bool g_input_bs_delete_empty_row = DEFAULT_BS_DELETE_EMPTY_ROW;
bool g_popup_independent_normal  = DEFAULT_POPUP_INDEPENDENT_NORMAL;
bool g_popup_independent_compact = DEFAULT_POPUP_INDEPENDENT_COMPACT;
bool g_sep_thousands = DEFAULT_SEP_THOUSANDS;
bool g_sep_hex       = DEFAULT_SEP_HEX;
int  g_limit_max_array_length  = DEFAULT_MAX_ARRAY_LENGTH;
int  g_limit_max_string_length = DEFAULT_MAX_STRING_LENGTH;
int  g_limit_max_call_depth    = DEFAULT_MAX_CALL_DEPTH;
bool g_show_rowlines = DEFAULT_SHOW_ROWLINES;
bool g_remember_position = DEFAULT_REMEMBER_POSITION;
bool g_start_topmost     = DEFAULT_START_TOPMOST;
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

void settings_set_path_for_test(const char *path) {
    s_conf_path = path ? path : "";
}

// ---- スキーマキー → GUI グローバル変数 ポインタの対応表 ----
//
// shared/settings_schema.c の TABLE は pure data (target ポインタを持たない) ので、
// GUI 側で「キー名 → 自分の変数ポインタ」を別途持つ。エントリ数は ~53 で
// 線形検索で十分速い。

namespace {

struct GuiTarget {
    const char *key;
    void       *target;   // bool*, int*, または Fl_Color*
};

const GuiTarget GUI_TARGETS[] = {
    // Font
    {"font",       &g_font_id},
    {"font_size",  &g_font_size},
    // General
    {"remember_position", &g_remember_position},
    {"start_topmost",     &g_start_topmost},
    {"show_rowlines",     &g_show_rowlines},
    {"max_array_length",  &g_limit_max_array_length},
    {"max_string_length", &g_limit_max_string_length},
    {"max_call_depth",    &g_limit_max_call_depth},
    // Input
    {"auto_completion",            &g_input_auto_completion},
    {"auto_close_brackets",        &g_input_auto_close_brackets},
    {"bs_delete_empty_row",        &g_input_bs_delete_empty_row},
    {"popup_independent_normal",   &g_popup_independent_normal},
    {"popup_independent_compact",  &g_popup_independent_compact},
    // Number Format
    {"decimal_digits",       &g_fmt_settings.decimal_len},
    {"e_notation",           &g_fmt_settings.e_notation},
    {"e_positive_min",       &g_fmt_settings.e_positive_min},
    {"e_negative_max",       &g_fmt_settings.e_negative_max},
    {"e_alignment",          &g_fmt_settings.e_alignment},
    {"thousands_separator",  &g_sep_thousands},
    {"hex_separator",        &g_sep_hex},
    // System Tray
    {"tray_icon",      &g_tray_icon},
    {"hotkey_enabled", &g_hotkey_enabled},
    {"hotkey_win",     &g_hotkey_win},
    {"hotkey_alt",     &g_hotkey_alt},
    {"hotkey_ctrl",    &g_hotkey_ctrl},
    {"hotkey_shift",   &g_hotkey_shift},
    {"hotkey_key",     &g_hotkey_keycode},
    // Colors
    {"color_preset",    &g_color_preset},
    {"color_bg",        &g_colors.bg},
    {"color_sel_bg",    &g_colors.sel_bg},
    {"color_rowline",   &g_colors.rowline},
    {"color_sep",       &g_colors.sep},
    {"color_text",      &g_colors.text},
    {"color_cursor",    &g_colors.cursor},
    {"color_symbol",    &g_colors.symbol},
    {"color_ident",     &g_colors.ident},
    {"color_special",   &g_colors.special},
    {"color_si_pfx",    &g_colors.si_pfx},
    {"color_paren0",    &g_colors.paren[0]},
    {"color_paren1",    &g_colors.paren[1]},
    {"color_paren2",    &g_colors.paren[2]},
    {"color_paren3",    &g_colors.paren[3]},
    {"color_error",     &g_colors.error},
    {"color_ui_win_bg", &g_colors.ui_win_bg},
    {"color_ui_bg",     &g_colors.ui_bg},
    {"color_ui_input",  &g_colors.ui_input},
    {"color_ui_btn",    &g_colors.ui_btn},
    {"color_ui_menu",   &g_colors.ui_menu},
    {"color_ui_text",   &g_colors.ui_text},
    {"color_ui_label",  &g_colors.ui_label},
    {"color_ui_dim",    &g_colors.ui_dim},
    {"color_pop_bg",    &g_colors.pop_bg},
    {"color_pop_sel",   &g_colors.pop_sel},
    {"color_pop_text",  &g_colors.pop_text},
    {"color_pop_desc",  &g_colors.pop_desc},
    {"color_pop_desc_bg", &g_colors.pop_desc_bg},
    {"color_pop_border",  &g_colors.pop_border},
};

void *gui_target(const char *key) {
    for (const GuiTarget &t : GUI_TARGETS) {
        if (strcmp(t.key, key) == 0) return t.target;
    }
    return nullptr;
}

// K_COLOR エントリに対応する CalcyxColors のデフォルトメンバ値を返す。
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
static void apply_default_one(const calcyx_setting_desc_t &d, void *target) {
    if (!target) return;
    switch (d.kind) {
    case CALCYX_SETTING_KIND_BOOL:
        *(bool *)target = (d.b_def != 0);
        break;
    case CALCYX_SETTING_KIND_INT:
        *(int  *)target = d.i_def;
        break;
    case CALCYX_SETTING_KIND_FONT:
        *(int  *)target = DEFAULT_FONT_ID;
        break;
    case CALCYX_SETTING_KIND_HOTKEY:
        *(int *)target = DEFAULT_HOTKEY_KEYCODE;
        break;
    case CALCYX_SETTING_KIND_COLOR_PRESET:
        *(int *)target = d.i_def;
        break;
    default:
        break;
    }
}

void settings_init_defaults() {
    int n = 0;
    const calcyx_setting_desc_t *table = calcyx_settings_table(&n);
    for (int i = 0; i < n; i++) {
        const calcyx_setting_desc_t &d = table[i];
        if (!(d.scope & CALCYX_SETTING_SCOPE_GUI)) continue;
        if (!d.key) continue;
        apply_default_one(d, gui_target(d.key));
    }
    update_crash_config_snapshot();
}

// ---- load ----
void settings_load() {
    /* 初回起動時に conf が無ければ canonical な既定値テンプレートを書き出す.
     * ユーザーが手編集の足がかりにできる. 既存ファイルには触らない. */
    ensure_path();
    calcyx_settings_init_defaults(s_conf_path.c_str(),
        "# calcyx user settings — edit freely.\n");

    auto kv = read_conf();
    if (kv.empty()) return;

    int n = 0;
    const calcyx_setting_desc_t *table = calcyx_settings_table(&n);

    for (int i = 0; i < n; i++) {
        const calcyx_setting_desc_t &d = table[i];
        if (!(d.scope & CALCYX_SETTING_SCOPE_GUI)) continue;
        if (!d.key) continue;
        void *target = gui_target(d.key);
        if (!target) continue;

        switch (d.kind) {
        case CALCYX_SETTING_KIND_BOOL:
            *(bool *)target = map_get_bool(kv, d.key, d.b_def != 0);
            break;
        case CALCYX_SETTING_KIND_INT: {
            int v = map_get_int(kv, d.key, d.i_def);
            *(int *)target = std::clamp(v, d.i_lo, d.i_hi);
            break;
        }
        case CALCYX_SETTING_KIND_FONT:
            *(int *)target = font_name_to_id(map_get(kv, d.key, d.s_def));
            break;
        case CALCYX_SETTING_KIND_HOTKEY: {
            std::string kn = map_get(kv, d.key, d.s_def);
            int k = plat_keyname_to_flkey(kn.c_str());
            *(int *)target = k ? k : DEFAULT_HOTKEY_KEYCODE;
            break;
        }
        case CALCYX_SETTING_KIND_COLOR_PRESET: {
            std::string ps = map_get(kv, d.key,
                COLOR_PRESET_INFO[d.i_def].id);
            int idx = d.i_def;
            for (int j = 0; j < COLOR_PRESET_COUNT; j++) {
                if (ps == COLOR_PRESET_INFO[j].id) { idx = j; break; }
            }
            *(int *)target = idx;
            break;
        }
        default:
            break;  // COLOR / SECTION は後段で処理
        }
    }

    // 色: プリセット確定後に user-defined なら各 color_* を読む
    if (g_color_preset == COLOR_PRESET_USER_DEFINED) {
        CalcyxColors def;
        colors_init_defaults(&def);
        for (int i = 0; i < n; i++) {
            const calcyx_setting_desc_t &d = table[i];
            if (d.kind != CALCYX_SETTING_KIND_COLOR) continue;
            if (!(d.scope & CALCYX_SETTING_SCOPE_GUI)) continue;
            void *target = gui_target(d.key);
            if (!target) continue;
            Fl_Color fallback = color_default(d.key, def);
            *(Fl_Color *)target = hex_to_color(map_get(kv, d.key, ""), fallback);
        }
    } else {
        colors_init_preset(&g_colors, g_color_preset);
    }

    update_crash_config_snapshot();
}

// ---- save ----
//
// shared/settings_writer.{h,c} の write_preserving に lookup を渡して
// コメント・並び順・未知キーを保ったまま書き戻す。GUI 側の責務はキー名から
// 現在値を文字列化することだけ。

namespace {

// settings_writer に渡すコールバック: キー → 文字列値.
// 戻り値: 1=書いた, 0=このキーは出力しない (color_* で preset != user 時).
int gui_value_lookup(const char *key, char *buf, size_t buflen, void *user) {
    (void)user;
    const calcyx_setting_desc_t *d = calcyx_settings_find(key);
    if (!d) return 0;
    if (!(d->scope & CALCYX_SETTING_SCOPE_GUI)) return 0;
    void *target = gui_target(key);
    if (!target) return 0;

    switch (d->kind) {
    case CALCYX_SETTING_KIND_BOOL:
        snprintf(buf, buflen, "%s", *(bool *)target ? "true" : "false");
        return 1;
    case CALCYX_SETTING_KIND_INT:
        snprintf(buf, buflen, "%d", *(int *)target);
        return 1;
    case CALCYX_SETTING_KIND_FONT: {
        std::string name = font_id_to_name(*(int *)target);
        snprintf(buf, buflen, "%s", name.c_str());
        return 1;
    }
    case CALCYX_SETTING_KIND_HOTKEY:
        snprintf(buf, buflen, "%s", plat_flkey_to_keyname(*(int *)target));
        return 1;
    case CALCYX_SETTING_KIND_COLOR_PRESET:
        snprintf(buf, buflen, "%s", COLOR_PRESET_INFO[*(int *)target].id);
        return 1;
    case CALCYX_SETTING_KIND_COLOR: {
        if (g_color_preset != COLOR_PRESET_USER_DEFINED) return 0;
        std::string hex = color_to_hex(*(Fl_Color *)target);
        snprintf(buf, buflen, "%s", hex.c_str());
        return 1;
    }
    default:
        return 0;
    }
}

} // namespace

void settings_save() {
    ensure_path();
    /* 既存ファイルが空 / 不在のときだけヘッダを置く. 既存ファイルにある
     * ユーザー編集のコメントは write_preserving が温存する. */
    const char *first_time_header = "# calcyx user settings — edit freely.\n";
    calcyx_settings_write_preserving(s_conf_path.c_str(),
                                     first_time_header,
                                     gui_value_lookup, nullptr);
    update_crash_config_snapshot();
}
