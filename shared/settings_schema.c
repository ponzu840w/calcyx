/* settings_schema.c — calcyx.conf の設定項目スキーマ実装。
 *
 * 各エントリの定義順がそのまま conf の出力順 (canonical 形式) になる。
 * GUI / TUI / CLI が同じテーブルを参照する。 */

#include "settings_schema.h"
#include "color_presets.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* ---- ヘルパマクロ ---- */
#define SEC(hdr) \
    { CALCYX_SETTING_KIND_SECTION, NULL, 0, 0,0,0, 0, NULL, hdr }
#define BOOLE(k, scope, dv) \
    { CALCYX_SETTING_KIND_BOOL, k, scope, 0,0,0, dv, NULL, NULL }
#define INTC(k, scope, dv, lo, hi) \
    { CALCYX_SETTING_KIND_INT, k, scope, lo, hi, dv, 0, NULL, NULL }
#define FONT(k, scope, sd) \
    { CALCYX_SETTING_KIND_FONT, k, scope, 0,0,0, 0, sd, NULL }
#define HKEY(k, scope, sd) \
    { CALCYX_SETTING_KIND_HOTKEY, k, scope, 0,0,0, 0, sd, NULL }
#define PRESET(k, scope, di, sd) \
    { CALCYX_SETTING_KIND_COLOR_PRESET, k, scope, 0,0,di, 0, sd, NULL }
#define COLOR(k, scope) \
    { CALCYX_SETTING_KIND_COLOR, k, scope, 0,0,0, 0, NULL, NULL }
#define STR(k, scope, sd) \
    { CALCYX_SETTING_KIND_STRING, k, scope, 0,0,0, 0, sd, NULL }

#define G  CALCYX_SETTING_SCOPE_GUI
#define T  CALCYX_SETTING_SCOPE_TUI
#define B  CALCYX_SETTING_SCOPE_BOTH

static const calcyx_setting_desc_t TABLE[] = {
    SEC("# ---- Font ----\n"
        "# Any font name installed on the system (e.g. monospace, DejaVu Sans Mono)\n"),
    FONT("font",       G, "Courier"),
    INTC("font_size",  G, 15, 8, 36),

    SEC("# ---- General ----\n"),
    BOOLE("remember_position", G, 1),
    BOOLE("start_topmost",     G, 0),
    BOOLE("show_rowlines",     G, 1),
    INTC("max_array_length",   B, 256, 1, 1000000),
    INTC("max_string_length",  B, 256, 1, 1000000),
    INTC("max_call_depth",     B,  64, 1, 1000),

    SEC("# ---- Input ----\n"),
    BOOLE("auto_completion",            B, 1),
    BOOLE("auto_close_brackets",        G, 0),
    BOOLE("bs_delete_empty_row",        B, 1),
    BOOLE("popup_independent_normal",   G, 0),
    BOOLE("popup_independent_compact",  G, 1),

    SEC("# ---- Number Format ----\n"),
    INTC("decimal_digits",       B,   9,   1, 34),
    BOOLE("e_notation",          B,   1),
    INTC("e_positive_min",       B,  15,   1, 30),
    INTC("e_negative_max",       B,  -5, -30, -1),
    BOOLE("e_alignment",         B,   0),
    /* thousands_separator / hex_separator は GUI のシート描画時の桁区切り
     * 挿入フラグ. TUI は独自の描画なので参照しない (scope=GUI). */
    BOOLE("thousands_separator", G,   1),
    BOOLE("hex_separator",       G,   1),

    SEC("# ---- System Tray ----\n"),
    BOOLE("tray_icon",      G, 0),
    BOOLE("hotkey_enabled", G, 0),
    BOOLE("hotkey_win",     G, 0),
    BOOLE("hotkey_alt",     G, 1),
    BOOLE("hotkey_ctrl",    G, 0),
    BOOLE("hotkey_shift",   G, 0),
    HKEY ("hotkey_key",     G, "Space"),

    SEC("# ---- Colors ----\n"
        "# Preset: otaku-black, gyakubari-white, saboten-grey, saboten-white, user\n"),
    /* color_preset / color_* は Phase C で TUI も読むようになるので scope=B にしてある.
     * ただし TUI 側は tui_color_source=mirror_gui のときだけ参照する条件付きアクセス. */
    PRESET("color_preset", B, 0, "otaku-black"),

    /* user-defined 時のみ出力される色エントリ */
    COLOR("color_bg",          B),
    COLOR("color_sel_bg",      B),
    COLOR("color_rowline",     B),
    COLOR("color_sep",         B),
    COLOR("color_text",        B),
    COLOR("color_cursor",      B),
    COLOR("color_symbol",      B),
    COLOR("color_ident",       B),
    COLOR("color_special",     B),
    COLOR("color_si_pfx",      B),
    COLOR("color_paren0",      B),
    COLOR("color_paren1",      B),
    COLOR("color_paren2",      B),
    COLOR("color_paren3",      B),
    COLOR("color_error",       B),
    COLOR("color_ui_win_bg",   B),
    COLOR("color_ui_bg",       B),
    COLOR("color_ui_input",    B),
    COLOR("color_ui_btn",      B),
    COLOR("color_ui_menu",     B),
    COLOR("color_ui_text",     B),
    COLOR("color_ui_label",    B),
    COLOR("color_ui_dim",      B),
    COLOR("color_pop_bg",      B),
    COLOR("color_pop_sel",     B),
    COLOR("color_pop_text",    B),
    COLOR("color_pop_desc",    B),
    COLOR("color_pop_desc_bg", B),
    COLOR("color_pop_border",  B),

    SEC("# ---- TUI ----\n"
        "# tui_color_source: 'semantic' (default) — 端末色基調 + 意味付け 2 色\n"
        "#                   'mirror_gui'         — calcyx.conf の color_* / color_preset を再現\n"),
    STR("tui_color_source", T, "semantic")
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

/* COLOR キー名と calcyx_color_palette_t のフィールドオフセットの対応表.
 * シンプルに strcmp で線形検索する (件数 28). */
static const struct {
    const char *key;
    size_t      offset;
} COLOR_KEY_OFFSETS[] = {
    { "color_bg",          offsetof(calcyx_color_palette_t, bg) },
    { "color_sel_bg",      offsetof(calcyx_color_palette_t, sel_bg) },
    { "color_rowline",     offsetof(calcyx_color_palette_t, rowline) },
    { "color_sep",         offsetof(calcyx_color_palette_t, sep) },
    { "color_text",        offsetof(calcyx_color_palette_t, text) },
    { "color_cursor",      offsetof(calcyx_color_palette_t, cursor) },
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
