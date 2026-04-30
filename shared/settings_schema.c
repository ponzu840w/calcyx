/* 設定項目スキーマ実装。 エントリ定義順 = conf の canonical 出力順。 */

#include "settings_schema.h"
#include "color_presets.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* ヘルパマクロ。 desc は writer のドキュメントコメント (1 行)。
 * range / bool / default は writer が自動付与するので desc に書かない。 */
#define SEC(hdr) \
    { CALCYX_SETTING_KIND_SECTION, NULL, 0, 0,0,0, 0, NULL, hdr, NULL }
#define BOOLE(k, scope, dv, ds) \
    { CALCYX_SETTING_KIND_BOOL, k, scope, 0,0,0, dv, NULL, NULL, ds }
#define INTC(k, scope, dv, lo, hi, ds) \
    { CALCYX_SETTING_KIND_INT, k, scope, lo, hi, dv, 0, NULL, NULL, ds }
#define FONT(k, scope, sd, ds) \
    { CALCYX_SETTING_KIND_FONT, k, scope, 0,0,0, 0, sd, NULL, ds }
#define HKEY(k, scope, sd, ds) \
    { CALCYX_SETTING_KIND_HOTKEY, k, scope, 0,0,0, 0, sd, NULL, ds }
#define PRESET(k, scope, di, sd, ds) \
    { CALCYX_SETTING_KIND_COLOR_PRESET, k, scope, 0,0,di, 0, sd, NULL, ds }
#define COLOR(k, scope) \
    { CALCYX_SETTING_KIND_COLOR, k, scope, 0,0,0, 0, NULL, NULL, NULL }
#define STR(k, scope, sd, ds) \
    { CALCYX_SETTING_KIND_STRING, k, scope, 0,0,0, 0, sd, NULL, ds }

#define G    CALCYX_SETTING_SCOPE_GUI
#define T    CALCYX_SETTING_SCOPE_TUI
#define GT   (CALCYX_SETTING_SCOPE_GUI | CALCYX_SETTING_SCOPE_TUI)
#define CORE CALCYX_SETTING_SCOPE_CORE

/* desc 文字列の方針: scope / range / values / default はメタ行 (writer が
 * 自動付与) で示されるので desc 側からは省く。 desc は「何のための設定か」
 * だけを短く書く。 冗長な "GUI ..." "(GUI sheet)" 等の scope 注釈も不要。 */

static const calcyx_setting_desc_t TABLE[] = {
    SEC("# ---- Language ----\n"),
    STR("language", CORE, "auto",
        "UI language: 'auto' (follow OS locale) / 'en' / 'ja'."),

    SEC("# ---- Font ----\n"),
    FONT("font",       G, "Courier",
         "Font name (e.g. monospace, Courier, DejaVu Sans Mono)."),
    INTC("font_size",  G, 15, 8, 36,
         "Font size in points."),

    SEC("# ---- General ----\n"),
    BOOLE("remember_position", G, 1,
          "Restore window position and size on next launch."),
    BOOLE("start_topmost",     G, 0,
          "Pin the window on top of other windows at startup."),
    BOOLE("show_rowlines",     G, 1,
          "Draw thin separator lines between rows in the sheet."),
    BOOLE("gui_menubar_in_window", G, 1,
          "(macOS only) Show menu items in the in-window menu bar in addition to the global menu bar. The in-window bar is always kept for the toolbar buttons; this only controls whether menu items are populated. Restart to apply."),
    INTC("max_array_length",   CORE, 256, 1, 1000000,
         "Maximum elements in arrays and strings (memory guard)."),
    INTC("max_string_length",  CORE, 256, 1, 1000000,
         "Maximum characters in a string value (memory guard)."),
    INTC("max_call_depth",     CORE,  64, 1, 1000,
         "Maximum function call recursion depth."),

    SEC("# ---- Input ----\n"),
    BOOLE("auto_completion",            GT, 1,
          "Show identifier/function completion popup while typing."),
    BOOLE("auto_close_brackets",        G, 0,
          "Auto-insert the matching closing bracket when typing the opening one."),
    BOOLE("bs_delete_empty_row",        GT, 1,
          "Backspace at the start of an empty row deletes that row."),
    BOOLE("popup_independent_normal",   G, 0,
          "Show completion popup as a separate OS window in normal layout."),
    BOOLE("popup_independent_compact",  G, 1,
          "Show completion popup as a separate OS window in compact layout."),

    SEC("# ---- Number Format ----\n"),
    INTC("decimal_digits",       CORE,   9,   1, 34,
         "Number of digits after the decimal point."),
    BOOLE("e_notation",          CORE,   1,
          "Use scientific (E) notation for very large or very small numbers."),
    INTC("e_positive_min",       CORE,  15,   1, 30,
         "Switch to E notation when the integer part has this many digits or more."),
    INTC("e_negative_max",       CORE,  -5, -30, -1,
         "Switch to E notation when the magnitude is 10^N or smaller."),
    BOOLE("e_alignment",         CORE,   0,
          "Right-align the mantissa width across results in E notation."),
    /* thousands_separator / hex_separator は GUI のシート描画時の桁区切り
     * 挿入フラグ。 TUI は独自の描画なので参照しない (scope=GUI)。 */
    BOOLE("thousands_separator", G,   1,
          "Insert '_' as a thousands separator in decimal display."),
    BOOLE("hex_separator",       G,   1,
          "Insert '_' every 4 hex digits in hexadecimal display."),

    SEC("# ---- System Tray ----\n"),
    BOOLE("tray_icon",      G, 0,
          "Show the calcyx icon in the system tray."),
    BOOLE("hotkey_enabled", G, 0,
          "Enable a global hotkey to show or hide the window."),
    BOOLE("hotkey_win",     G, 0,
          "Hotkey modifier: Win/Super/Cmd."),
    BOOLE("hotkey_alt",     G, 1,
          "Hotkey modifier: Alt/Option."),
    BOOLE("hotkey_ctrl",    G, 0,
          "Hotkey modifier: Ctrl."),
    BOOLE("hotkey_shift",   G, 0,
          "Hotkey modifier: Shift."),
    HKEY ("hotkey_key",     G, "Space",
          "Hotkey base key in FLTK form (e.g. Space, F1, A)."),

    SEC("# ---- Colors ----\n"),
    /* color_preset / color_* は Phase C で TUI も読むようになるので scope=GT.
     * TUI 側は tui_color_source=mirror_gui のときだけ参照する条件付きアクセス。 */
    PRESET("color_preset", GT, 0, "otaku-black",
           "Color theme preset. color_* keys below take effect only when set to 'user'."),

    /* color_* は preset != user-defined のとき commented で書かれる。 */
    COLOR("color_bg",          GT),
    COLOR("color_sel_bg",      GT),
    COLOR("color_rowline",     GT),
    COLOR("color_text",        GT),
    COLOR("color_accent",      GT),
    COLOR("color_symbol",      GT),
    COLOR("color_ident",       GT),
    COLOR("color_special",     GT),
    COLOR("color_si_pfx",      GT),
    COLOR("color_paren0",      GT),
    COLOR("color_paren1",      GT),
    COLOR("color_paren2",      GT),
    COLOR("color_paren3",      GT),
    COLOR("color_error",       GT),
    COLOR("color_ui_win_bg",   GT),
    COLOR("color_ui_bg",       GT),
    COLOR("color_ui_input",    GT),
    COLOR("color_ui_btn",      GT),
    COLOR("color_ui_menu",     GT),
    COLOR("color_ui_text",     GT),
    COLOR("color_ui_label",    GT),
    COLOR("color_ui_dim",      GT),
    COLOR("color_pop_bg",      GT),
    COLOR("color_pop_sel",     GT),
    COLOR("color_pop_text",    GT),
    COLOR("color_pop_desc",    GT),
    COLOR("color_pop_desc_bg", GT),
    COLOR("color_pop_border",  GT),

    SEC("# ---- TUI ----\n"),
    STR("tui_color_source", T, "semantic",
        "Color rendering mode: 'semantic' (terminal palette + accent) or 'mirror_gui' (use color_*)."),
    STR("tui_clear_after_overlay", T, "auto",
        "Force full screen clear after closing overlays (workaround for macOS Terminal + tmux + CJK ghosting). 'auto' = on for macOS, off elsewhere; 'true' / 'false' force the choice."),

    /* tui_color_source = semantic のとき、 構文ハイライトをハードコード値ではなく
     * 端末 ANSI 17 色 (default + 通常 8 + light 8) から選んで描画する。
     * 値は 'cyan-light' / 'red' などのキー名 (PrefsScreen / TuiSheet 側の
     * kSemanticColors テーブルで FTXUI Color enum に変換)。 */
    BOOLE("tui_sem_color_literal", T, 1,
          "Render `#RRGGBB` literals with their actual color (background = literal, foreground = white/black by luminance). When false, fall back to the literal/string color."),
    STR("tui_sem_ident",   T, "cyan-light",    "Semantic color: identifiers."),
    STR("tui_sem_special", T, "magenta-light", "Semantic color: literals."),
    STR("tui_sem_si_pfx",  T, "yellow-light",  "Semantic color: SI prefix / exponent."),
    STR("tui_sem_symbol",  T, "red-light",     "Semantic color: operators / keywords."),
    STR("tui_sem_paren0",  T, "yellow-light",  "Semantic color: paren depth 1."),
    STR("tui_sem_paren1",  T, "magenta-light", "Semantic color: paren depth 2."),
    STR("tui_sem_paren2",  T, "cyan-light",    "Semantic color: paren depth 3."),
    STR("tui_sem_paren3",  T, "green-light",   "Semantic color: paren depth 4.")
};

static const int TABLE_N = (int)(sizeof(TABLE) / sizeof(TABLE[0]));

const calcyx_setting_desc_t *calcyx_settings_table(int *out_count) {
    if (out_count) *out_count = TABLE_N;
    return TABLE;
}

const calcyx_setting_desc_t *calcyx_settings_find(const char *key) {
    int i;
    if (!key) return NULL;
    for (i = 0; i < TABLE_N; i++) {
        if (TABLE[i].key && strcmp(TABLE[i].key, key) == 0) {
            return &TABLE[i];
        }
    }
    return NULL;
}

/* COLOR キー名と calcyx_color_palette_t のフィールドオフセットの対応表。
 * シンプルに strcmp で線形検索する (件数 28)。 */
static const struct {
    const char *key;
    size_t      offset;
} COLOR_KEY_OFFSETS[] = {
    { "color_bg",          offsetof(calcyx_color_palette_t, bg) },
    { "color_sel_bg",      offsetof(calcyx_color_palette_t, sel_bg) },
    { "color_rowline",     offsetof(calcyx_color_palette_t, rowline) },
    { "color_text",        offsetof(calcyx_color_palette_t, text) },
    { "color_accent",      offsetof(calcyx_color_palette_t, accent) },
    { "color_symbol",      offsetof(calcyx_color_palette_t, symbol) },
    { "color_ident",       offsetof(calcyx_color_palette_t, ident) },
    { "color_special",     offsetof(calcyx_color_palette_t, special) },
    { "color_si_pfx",      offsetof(calcyx_color_palette_t, si_pfx) },
    { "color_paren0",      offsetof(calcyx_color_palette_t, paren[0]) },
    { "color_paren1",      offsetof(calcyx_color_palette_t, paren[1]) },
    { "color_paren2",      offsetof(calcyx_color_palette_t, paren[2]) },
    { "color_paren3",      offsetof(calcyx_color_palette_t, paren[3]) },
    { "color_error",       offsetof(calcyx_color_palette_t, error) },
    { "color_ui_win_bg",   offsetof(calcyx_color_palette_t, ui_win_bg) },
    { "color_ui_bg",       offsetof(calcyx_color_palette_t, ui_bg) },
    { "color_ui_input",    offsetof(calcyx_color_palette_t, ui_input) },
    { "color_ui_btn",      offsetof(calcyx_color_palette_t, ui_btn) },
    { "color_ui_menu",     offsetof(calcyx_color_palette_t, ui_menu) },
    { "color_ui_text",     offsetof(calcyx_color_palette_t, ui_text) },
    { "color_ui_label",    offsetof(calcyx_color_palette_t, ui_label) },
    /* color_ui_dim はプリセット由来 (ui_text と ui_menu の中間色) なので
     * テーブル外。COLOR_PRESET_INFO 経由でデフォルトを返さない。 */
    { "color_pop_bg",      offsetof(calcyx_color_palette_t, pop_bg) },
    { "color_pop_sel",     offsetof(calcyx_color_palette_t, pop_sel) },
    { "color_pop_text",    offsetof(calcyx_color_palette_t, pop_text) },
    { "color_pop_desc",    offsetof(calcyx_color_palette_t, pop_desc) },
    { "color_pop_desc_bg", offsetof(calcyx_color_palette_t, pop_desc_bg) },
    { "color_pop_border",  offsetof(calcyx_color_palette_t, pop_border) }
};

const char *calcyx_settings_color_default(const char *key,
                                          const char *preset_id) {
    static char buf[8];
    int preset;
    calcyx_color_palette_t pal;
    size_t i;
    if (!key || !preset_id) return NULL;
    preset = calcyx_color_preset_lookup(preset_id);
    if (preset < 0) return NULL;
    calcyx_color_preset_get(preset, &pal);
    for (i = 0; i < sizeof(COLOR_KEY_OFFSETS)/sizeof(COLOR_KEY_OFFSETS[0]); i++) {
        if (strcmp(COLOR_KEY_OFFSETS[i].key, key) == 0) {
            const calcyx_rgb_t *rgb =
                (const calcyx_rgb_t *)((const char *)&pal + COLOR_KEY_OFFSETS[i].offset);
            snprintf(buf, sizeof(buf), "#%02X%02X%02X", rgb->r, rgb->g, rgb->b);
            return buf;
        }
    }
    return NULL;
}
