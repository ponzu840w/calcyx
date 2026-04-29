// ユーザー設定の永続化 (GUI 側)。
// スキーマは shared/settings_schema.c. ここはキー→g_* ポインタ対応表と
// FLTK の Fl_Color / フォント ID 変換のみ担当する。

#include "settings_globals.h"
#include "AppSettings.h"
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
#include "path_utf8.h"
}

// ---- 後方互換のための g_<name> 参照 (実体は g_settings.<name>) ----
std::string &g_language                   = g_settings.language;
int         &g_font_id                    = g_settings.font_id;
int         &g_font_size                  = g_settings.font_size;
bool        &g_input_auto_completion      = g_settings.input_auto_completion;
bool        &g_input_auto_close_brackets  = g_settings.input_auto_close_brackets;
bool        &g_input_bs_delete_empty_row  = g_settings.input_bs_delete_empty_row;
bool        &g_popup_independent_normal   = g_settings.popup_independent_normal;
bool        &g_popup_independent_compact  = g_settings.popup_independent_compact;
bool        &g_sep_thousands              = g_settings.sep_thousands;
bool        &g_sep_hex                    = g_settings.sep_hex;
int         &g_limit_max_array_length     = g_settings.limit_max_array_length;
int         &g_limit_max_string_length    = g_settings.limit_max_string_length;
int         &g_limit_max_call_depth       = g_settings.limit_max_call_depth;
bool        &g_show_rowlines              = g_settings.show_rowlines;
bool        &g_remember_position          = g_settings.remember_position;
bool        &g_start_topmost              = g_settings.start_topmost;
bool        &g_tray_icon                  = g_settings.tray_icon;
bool        &g_hotkey_enabled             = g_settings.hotkey_enabled;
bool        &g_hotkey_win                 = g_settings.hotkey_win;
bool        &g_hotkey_alt                 = g_settings.hotkey_alt;
bool        &g_hotkey_ctrl                = g_settings.hotkey_ctrl;
bool        &g_hotkey_shift               = g_settings.hotkey_shift;
int         &g_hotkey_keycode             = g_settings.hotkey_keycode;

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

// ---- スキーマキー → g_* ポインタの対応表 (~53 件、 線形検索で OK) ----

namespace {

struct GuiTarget {
    const char *key;
    void       *target;   // bool*, int*, または Fl_Color*
};

const GuiTarget GUI_TARGETS[] = {
    // Language
    {"language",   &g_language},
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
    {"color_text",      &g_colors.text},
    {"color_accent",    &g_colors.accent},
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
        {"color_text",     &CalcyxColors::text},
        {"color_accent",   &CalcyxColors::accent},
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
    FILE *fp = calcyx_fopen(s_conf_path.c_str(), "r");
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
    case CALCYX_SETTING_KIND_STRING:
        *(std::string *)target = d.s_def ? d.s_def : "";
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
    /* 初回起動時に conf が無ければ canonical な既定値テンプレートを書き出す。
     * ユーザーが手編集の足がかりにできる。 既存ファイルには触らない。 */
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
        case CALCYX_SETTING_KIND_STRING:
            *(std::string *)target =
                map_get(kv, d.key, d.s_def ? d.s_def : "");
            break;
        default:
            break;  // COLOR / SECTION は後段で処理
        }
    }

    /* color_* は preset によらず常に g_user_colors に読み込む。
     * '#key=value' (commented) 形式も settings_io が値として返すので、
     * preset != USER のセッション越しの色設定が round-trip する。 */
    {
        CalcyxColors def;
        colors_init_defaults(&def);
        for (int i = 0; i < n; i++) {
            const calcyx_setting_desc_t &d = table[i];
            if (d.kind != CALCYX_SETTING_KIND_COLOR) continue;
            if (!(d.scope & CALCYX_SETTING_SCOPE_GUI)) continue;
            void *target = gui_target(d.key);
            if (!target) continue;
            size_t offset = (char *)target - (char *)&g_colors;
            Fl_Color *user_target =
                (Fl_Color *)((char *)&g_user_colors + offset);
            Fl_Color fallback = color_default(d.key, def);
            *user_target = hex_to_color(map_get(kv, d.key, ""), fallback);
        }
    }
    /* g_colors は preset に応じて: USER_DEFINED なら g_user_colors のコピー、
     * そうでなければ preset 由来の色値で上書き。 */
    colors_apply_preset(g_color_preset);

    update_crash_config_snapshot();
}

// ---- save: write_preserving に lookup を渡す。 キー → 値文字列化のみ責務。 ----

namespace {

// settings_writer 用コールバック (キー → 文字列値)。
//   1=PROVIDED, 0=DROP (color_* で preset != user-defined; commented 保持)、
//   -1=LEAVE (GUI 管轄外; TUI 専用キーが GUI save で破壊されないように)。
int gui_value_lookup(const char *key, char *buf, size_t buflen,
                     int *out_is_default, void *user) {
    (void)user;
    if (out_is_default) *out_is_default = 0;
    const calcyx_setting_desc_t *d = calcyx_settings_find(key);
    if (!d) return -1;
    if (!(d->scope & CALCYX_SETTING_SCOPE_GUI)) return -1;
    void *target = gui_target(key);
    if (!target) return -1;

    switch (d->kind) {
    case CALCYX_SETTING_KIND_BOOL: {
        bool v = *(bool *)target;
        snprintf(buf, buflen, "%s", v ? "true" : "false");
        if (out_is_default) *out_is_default = ((v ? 1 : 0) == d->b_def);
        return 1;
    }
    case CALCYX_SETTING_KIND_INT: {
        int v = *(int *)target;
        snprintf(buf, buflen, "%d", v);
        if (out_is_default) *out_is_default = (v == d->i_def);
        return 1;
    }
    case CALCYX_SETTING_KIND_FONT: {
        std::string name = font_id_to_name(*(int *)target);
        snprintf(buf, buflen, "%s", name.c_str());
        if (out_is_default)
            *out_is_default = (d->s_def && name == d->s_def);
        return 1;
    }
    case CALCYX_SETTING_KIND_HOTKEY: {
        const char *kn = plat_flkey_to_keyname(*(int *)target);
        snprintf(buf, buflen, "%s", kn);
        if (out_is_default)
            *out_is_default = (d->s_def && strcmp(kn, d->s_def) == 0);
        return 1;
    }
    case CALCYX_SETTING_KIND_COLOR_PRESET: {
        const char *id = COLOR_PRESET_INFO[*(int *)target].id;
        snprintf(buf, buflen, "%s", id);
        if (out_is_default)
            *out_is_default = (d->s_def && strcmp(id, d->s_def) == 0);
        return 1;
    }
    case CALCYX_SETTING_KIND_STRING: {
        const std::string &s = *(std::string *)target;
        snprintf(buf, buflen, "%s", s.c_str());
        if (out_is_default)
            *out_is_default = (d->s_def && s == d->s_def);
        return 1;
    }
    case CALCYX_SETTING_KIND_COLOR: {
        /* COLOR キーは g_user_colors (preset 切替で温存される) の値を返す。
         * is_default = (preset == USER_DEFINED ? 0 : 1).
         * 全 COLOR キーが conf に常駐し preset 切替で commented⇔uncommented
         * の round-trip が成立する。 */
        size_t offset = (char *)target - (char *)&g_colors;
        Fl_Color *user_target = (Fl_Color *)((char *)&g_user_colors + offset);
        std::string hex = color_to_hex(*user_target);
        snprintf(buf, buflen, "%s", hex.c_str());
        if (out_is_default) {
            *out_is_default = (g_color_preset != COLOR_PRESET_USER_DEFINED);
        }
        return 1;
    }
    default:
        return -1;
    }
}

} // namespace

void settings_save() {
    ensure_path();
    /* 既存ファイルが空 / 不在のときだけヘッダを置く。 既存ファイルにある
     * ユーザー編集のコメントは write_preserving が温存する。 */
    const char *first_time_header = "# calcyx user settings — edit freely.\n";
    calcyx_settings_write_preserving(s_conf_path.c_str(),
                                     first_time_header,
                                     gui_value_lookup, nullptr);
    update_crash_config_snapshot();
}
