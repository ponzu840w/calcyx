// settings_globals.cpp — ユーザー設定の定義と永続化
//
// calcyx.conf はコメント付きの key = value 形式。
// ユーザーがテキストエディタで直接編集できる。
// 設定ダイアログは GUI ヘルパーとしてこのファイルを読み書きする。

#include "settings_globals.h"
#include "app_prefs.h"
#include "colors.h"
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

// ---- グローバル変数 ----
int  g_font_id   = FL_COURIER;
int  g_font_size  = 13;
bool g_input_auto_completion    = true;
bool g_input_auto_close_brackets = false;
bool g_sep_thousands = true;
bool g_sep_hex       = true;
int  g_limit_max_array_length  = 256;
int  g_limit_max_string_length = 256;
int  g_limit_max_call_depth    = 64;
bool g_show_rowlines = true;
bool g_remember_position = true;

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

static void update_crash_config_snapshot();

// ---- デフォルト値 ----
void settings_init_defaults() {
    g_font_id   = FL_COURIER;
    g_font_size  = 13;
    g_input_auto_completion    = true;
    g_input_auto_close_brackets = false;
    g_sep_thousands = true;
    g_sep_hex       = true;
    g_limit_max_array_length  = 256;
    g_limit_max_string_length = 256;
    g_limit_max_call_depth    = 64;
    g_show_rowlines = true;
    g_remember_position = true;

    g_fmt_settings.decimal_len     = 9;
    g_fmt_settings.e_notation      = true;
    g_fmt_settings.e_positive_min  = 15;
    g_fmt_settings.e_negative_max  = -5;
    g_fmt_settings.e_alignment     = false;

    update_crash_config_snapshot();
}

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

static std::string get(const std::map<std::string, std::string> &kv,
                       const char *key, const char *def) {
    auto it = kv.find(key);
    return (it != kv.end()) ? it->second : def;
}

static int get_int(const std::map<std::string, std::string> &kv,
                   const char *key, int def) {
    auto it = kv.find(key);
    return (it != kv.end()) ? atoi(it->second.c_str()) : def;
}

static bool get_bool(const std::map<std::string, std::string> &kv,
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

// ---- load ----
void settings_load() {
    auto kv = read_conf();
    if (kv.empty()) return;

    g_font_id   = font_name_to_id(get(kv, "font", "Courier"));
    g_font_size  = std::clamp(get_int(kv, "font_size", 13), 8, 36);
    g_input_auto_completion     = get_bool(kv, "auto_completion", true);
    g_input_auto_close_brackets = get_bool(kv, "auto_close_brackets", false);
    g_sep_thousands = get_bool(kv, "thousands_separator", true);
    g_sep_hex       = get_bool(kv, "hex_separator", true);

    g_limit_max_array_length  = std::clamp(get_int(kv, "max_array_length", 256), 1, 1000000);
    g_limit_max_string_length = std::clamp(get_int(kv, "max_string_length", 256), 1, 1000000);
    g_limit_max_call_depth    = std::clamp(get_int(kv, "max_call_depth", 64), 1, 1000);
    g_show_rowlines           = get_bool(kv, "show_rowlines", true);
    g_remember_position       = get_bool(kv, "remember_position", true);

    g_fmt_settings.decimal_len    = std::clamp(get_int(kv, "decimal_digits", 9), 1, 34);
    g_fmt_settings.e_notation     = get_bool(kv, "e_notation", true);
    g_fmt_settings.e_positive_min = std::clamp(get_int(kv, "e_positive_min", 15), 1, 30);
    g_fmt_settings.e_negative_max = std::clamp(get_int(kv, "e_negative_max", -5), -30, -1);
    g_fmt_settings.e_alignment    = get_bool(kv, "e_alignment", false);

    std::string preset_str = get(kv, "color_preset", "otaku-black");
    g_color_preset = COLOR_PRESET_OTAKU_BLACK;
    for (int i = 0; i < COLOR_PRESET_COUNT; i++) {
        if (preset_str == COLOR_PRESET_INFO[i].id) {
            g_color_preset = i;
            break;
        }
    }

    if (g_color_preset == COLOR_PRESET_USER_DEFINED) {
        CalcyxColors def;
        colors_init_defaults(&def);
        auto lc = [&](const char *key, Fl_Color d) {
            return hex_to_color(get(kv, key, ""), d);
        };
        g_colors.bg       = lc("color_bg",       def.bg);
        g_colors.sel_bg   = lc("color_sel_bg",   def.sel_bg);
        g_colors.rowline  = lc("color_rowline",  def.rowline);
        g_colors.sep      = lc("color_sep",      def.sep);
        g_colors.text     = lc("color_text",     def.text);
        g_colors.cursor   = lc("color_cursor",   def.cursor);
        g_colors.symbol   = lc("color_symbol",   def.symbol);
        g_colors.ident    = lc("color_ident",    def.ident);
        g_colors.special  = lc("color_special",  def.special);
        g_colors.si_pfx   = lc("color_si_pfx",   def.si_pfx);
        g_colors.paren[0] = lc("color_paren0",   def.paren[0]);
        g_colors.paren[1] = lc("color_paren1",   def.paren[1]);
        g_colors.paren[2] = lc("color_paren2",   def.paren[2]);
        g_colors.paren[3] = lc("color_paren3",   def.paren[3]);
        g_colors.error    = lc("color_error",    def.error);
        g_colors.ui_win_bg = lc("color_ui_win_bg", def.ui_win_bg);
        g_colors.ui_bg    = lc("color_ui_bg",    def.ui_bg);
        g_colors.ui_input = lc("color_ui_input", def.ui_input);
        g_colors.ui_btn   = lc("color_ui_btn",   def.ui_btn);
        g_colors.ui_menu  = lc("color_ui_menu",  def.ui_menu);
        g_colors.ui_text  = lc("color_ui_text",  def.ui_text);
        g_colors.ui_label = lc("color_ui_label", def.ui_label);
        g_colors.ui_dim   = lc("color_ui_dim",   def.ui_dim);
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

    fprintf(fp,
        "# calcyx user settings\n"
        "# Changes take effect on next launch, or immediately via Preferences dialog.\n"
        "# This file is regenerated when saving from the Preferences dialog.\n"
        "# Custom comments will not be preserved.\n"
        "\n"
        "# ---- Font ----\n"
        "# Any font name installed on the system (e.g. monospace, DejaVu Sans Mono)\n"
        "font = %s\n"
        "font_size = %d\n"
        "\n"
        "# ---- General ----\n"
        "remember_position = %s\n"
        "show_rowlines = %s\n"
        "max_array_length = %d\n"
        "max_string_length = %d\n"
        "max_call_depth = %d\n"
        "\n"
        "# ---- Input ----\n"
        "auto_completion = %s\n"
        "auto_close_brackets = %s\n"
        "\n"
        "# ---- Number Format ----\n"
        "decimal_digits = %d\n"
        "e_notation = %s\n"
        "e_positive_min = %d\n"
        "e_negative_max = %d\n"
        "e_alignment = %s\n"
        "thousands_separator = %s\n"
        "hex_separator = %s\n"
        "\n"
        "# ---- Colors ----\n"
        "# Preset: otaku-black, gyakubari-white, saboten-black, saboten-white, user\n"
        "color_preset = %s\n",
        font_id_to_name(g_font_id).c_str(), g_font_size,
        g_remember_position ? "true" : "false",
        g_show_rowlines ? "true" : "false",
        g_limit_max_array_length,
        g_limit_max_string_length,
        g_limit_max_call_depth,
        g_input_auto_completion ? "true" : "false",
        g_input_auto_close_brackets ? "true" : "false",
        g_fmt_settings.decimal_len,
        g_fmt_settings.e_notation ? "true" : "false",
        g_fmt_settings.e_positive_min,
        g_fmt_settings.e_negative_max,
        g_fmt_settings.e_alignment ? "true" : "false",
        g_sep_thousands ? "true" : "false",
        g_sep_hex ? "true" : "false",
        COLOR_PRESET_INFO[g_color_preset].id);

    if (g_color_preset == COLOR_PRESET_USER_DEFINED) {
        fprintf(fp,
            "color_bg = %s\n"
            "color_sel_bg = %s\n"
            "color_rowline = %s\n"
            "color_sep = %s\n"
            "color_text = %s\n"
            "color_cursor = %s\n"
            "color_symbol = %s\n"
            "color_ident = %s\n"
            "color_special = %s\n"
            "color_si_pfx = %s\n"
            "color_paren0 = %s\n"
            "color_paren1 = %s\n"
            "color_paren2 = %s\n"
            "color_paren3 = %s\n"
            "color_error = %s\n",
            color_to_hex(g_colors.bg).c_str(),
            color_to_hex(g_colors.sel_bg).c_str(),
            color_to_hex(g_colors.rowline).c_str(),
            color_to_hex(g_colors.sep).c_str(),
            color_to_hex(g_colors.text).c_str(),
            color_to_hex(g_colors.cursor).c_str(),
            color_to_hex(g_colors.symbol).c_str(),
            color_to_hex(g_colors.ident).c_str(),
            color_to_hex(g_colors.special).c_str(),
            color_to_hex(g_colors.si_pfx).c_str(),
            color_to_hex(g_colors.paren[0]).c_str(),
            color_to_hex(g_colors.paren[1]).c_str(),
            color_to_hex(g_colors.paren[2]).c_str(),
            color_to_hex(g_colors.paren[3]).c_str(),
            color_to_hex(g_colors.error).c_str());
        fprintf(fp,
            "color_ui_win_bg = %s\n"
            "color_ui_bg = %s\n"
            "color_ui_input = %s\n"
            "color_ui_btn = %s\n"
            "color_ui_menu = %s\n"
            "color_ui_text = %s\n"
            "color_ui_label = %s\n"
            "color_ui_dim = %s\n",
            color_to_hex(g_colors.ui_win_bg).c_str(),
            color_to_hex(g_colors.ui_bg).c_str(),
            color_to_hex(g_colors.ui_input).c_str(),
            color_to_hex(g_colors.ui_btn).c_str(),
            color_to_hex(g_colors.ui_menu).c_str(),
            color_to_hex(g_colors.ui_text).c_str(),
            color_to_hex(g_colors.ui_label).c_str(),
            color_to_hex(g_colors.ui_dim).c_str());
    }

    fclose(fp);

    update_crash_config_snapshot();
}
