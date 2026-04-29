/* カラープリセットの RGB 表 (FLTK 非依存、 GUI/TUI 共有の正本)。 */

#ifndef CALCYX_SHARED_COLOR_PRESETS_H
#define CALCYX_SHARED_COLOR_PRESETS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { unsigned char r, g, b; } calcyx_rgb_t;

/* CalcyxColors と 1:1 対応する RGB パレット。 ui/colors.h と同じフィールド順。 */
typedef struct {
    /* 背景 / フレーム */
    calcyx_rgb_t bg;
    calcyx_rgb_t sel_bg;
    calcyx_rgb_t rowline;

    /* テキスト / アクセント */
    calcyx_rgb_t text;
    calcyx_rgb_t accent;

    /* シンタックスハイライト */
    calcyx_rgb_t symbol;
    calcyx_rgb_t ident;
    calcyx_rgb_t special;
    calcyx_rgb_t si_pfx;
    calcyx_rgb_t paren[4];
    calcyx_rgb_t error;

    /* UI クローム */
    calcyx_rgb_t ui_win_bg;
    calcyx_rgb_t ui_bg;
    calcyx_rgb_t ui_input;
    calcyx_rgb_t ui_btn;
    calcyx_rgb_t ui_menu;
    calcyx_rgb_t ui_text;
    calcyx_rgb_t ui_label;

    /* 補完ポップアップ */
    calcyx_rgb_t pop_bg;
    calcyx_rgb_t pop_sel;
    calcyx_rgb_t pop_text;
    calcyx_rgb_t pop_desc;
    calcyx_rgb_t pop_desc_bg;
    calcyx_rgb_t pop_border;
} calcyx_color_palette_t;

/* enum 値は ui/colors.h の ColorPreset と一致させる。 */
enum {
    CALCYX_COLOR_PRESET_OTAKU_BLACK     = 0,
    CALCYX_COLOR_PRESET_GYAKUBARI_WHITE = 1,
    CALCYX_COLOR_PRESET_SABOTEN_GREY    = 2,
    CALCYX_COLOR_PRESET_SABOTEN_WHITE   = 3,
    CALCYX_COLOR_PRESET_USER_DEFINED    = 4,
    CALCYX_COLOR_PRESET_COUNT           = 5
};

/* preset id 文字列 → enum. 未知なら -1. */
int calcyx_color_preset_lookup(const char *id);

/* enum → id 文字列。 範囲外なら NULL. */
const char *calcyx_color_preset_id(int preset);

/* preset の RGB パレットを out に書く。 preset が user-defined / 範囲外なら
 * otaku-black を書く (UI 側で個別キーを上書きする想定)。 */
void calcyx_color_preset_get(int preset, calcyx_color_palette_t *out);

#ifdef __cplusplus
}
#endif

#endif
