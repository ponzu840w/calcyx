// settings_globals.cpp — ユーザー設定の定義と永続化
//
// calcyx.conf はコメント付きの key = value 形式。
// ユーザーがテキストエディタで直接編集できる。
// 設定ダイアログは GUI ヘルパーとしてこのファイルを読み書きする。

#include "settings_globals.h"
#include "app_prefs.h"
#include "colors.h"
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
bool g_sep_thousands = false;
bool g_sep_hex       = false;

static std::string s_conf_path;

// ---- フォント名 <-> ID 変換 ----
struct FontEntry { const char *name; int id; };
static const FontEntry FONT_TABLE[] = {
    { "Courier",      FL_COURIER },
    { "Courier Bold", FL_COURIER_BOLD },
    { "Helvetica",    FL_HELVETICA },
    { "Times",        FL_TIMES },
    { "Screen",       FL_SCREEN },
    { "Screen Bold",  FL_SCREEN_BOLD },
};
static const int FONT_TABLE_SIZE = sizeof(FONT_TABLE) / sizeof(FONT_TABLE[0]);

static const char *font_id_to_name(int id) {
    for (int i = 0; i < FONT_TABLE_SIZE; i++)
        if (FONT_TABLE[i].id == id) return FONT_TABLE[i].name;
    return "Courier";
}

static int font_name_to_id(const std::string &name) {
    for (int i = 0; i < FONT_TABLE_SIZE; i++)
        if (name == FONT_TABLE[i].name) return FONT_TABLE[i].id;
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

// ---- デフォルト値 ----
void settings_init_defaults() {
    g_font_id   = FL_COURIER;
    g_font_size  = 13;
    g_input_auto_completion    = true;
    g_input_auto_close_brackets = false;
    g_sep_thousands = false;
    g_sep_hex       = false;

    g_fmt_settings.decimal_len     = 15;
    g_fmt_settings.e_notation      = true;
    g_fmt_settings.e_positive_min  = 7;
    g_fmt_settings.e_negative_max  = -4;
    g_fmt_settings.e_alignment     = true;
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

// ---- load ----
void settings_load() {
    auto kv = read_conf();
    if (kv.empty()) return;

    g_font_id   = font_name_to_id(get(kv, "font", "Courier"));
    g_font_size  = std::clamp(get_int(kv, "font_size", 13), 8, 36);
    g_input_auto_completion     = get_bool(kv, "auto_completion", true);
    g_input_auto_close_brackets = get_bool(kv, "auto_close_brackets", false);
    g_sep_thousands = get_bool(kv, "thousands_separator", false);
    g_sep_hex       = get_bool(kv, "hex_separator", false);

    g_fmt_settings.decimal_len    = std::clamp(get_int(kv, "decimal_digits", 15), 1, 34);
    g_fmt_settings.e_notation     = get_bool(kv, "e_notation", true);
    g_fmt_settings.e_positive_min = std::clamp(get_int(kv, "e_positive_min", 7), 1, 30);
    g_fmt_settings.e_negative_max = std::clamp(get_int(kv, "e_negative_max", -4), -30, -1);
    g_fmt_settings.e_alignment    = get_bool(kv, "e_alignment", true);

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
        "# Available: Courier, Courier Bold, Helvetica, Times, Screen, Screen Bold\n"
        "font = %s\n"
        "font_size = %d\n"
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
        "# ---- Colors (#RRGGBB) ----\n"
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
        font_id_to_name(g_font_id), g_font_size,
        g_input_auto_completion ? "true" : "false",
        g_input_auto_close_brackets ? "true" : "false",
        g_fmt_settings.decimal_len,
        g_fmt_settings.e_notation ? "true" : "false",
        g_fmt_settings.e_positive_min,
        g_fmt_settings.e_negative_max,
        g_fmt_settings.e_alignment ? "true" : "false",
        g_sep_thousands ? "true" : "false",
        g_sep_hex ? "true" : "false",
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

    fclose(fp);
}
