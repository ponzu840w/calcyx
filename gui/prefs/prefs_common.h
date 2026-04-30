// PrefsDialog タブ間で共有する DlgState とスタイル/プレビュー/swatch 補助。
// 各タブは tab_*.cpp の build_<tab>_tab() として独立。

#pragma once

#include "settings_globals.h"
#include "colors.h"
#include "AppSettings.h"
#include <FL/Fl.H>
#include <FL/Fl_Tabs.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Spinner.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Double_Window.H>
#include <string>
#include <vector>

class SheetView;

// ---- ダイアログ全体のサイズ / スタイル定数 ----
inline constexpr int DW = 580;
inline constexpr int DH = 630;

#define DLG_BG     g_colors.ui_bg
#define DLG_INPUT  g_colors.ui_input
#define DLG_BTN    g_colors.ui_btn
#define DLG_TEXT   g_colors.ui_text
#define DLG_LABEL  g_colors.ui_label

// ---- Font tab 用構造体 ----
struct SysFont {
    std::string name;
    Fl_Font     id;
    bool        monospace;
};

struct DlgState;

struct FontTab {
    Fl_Button       *font_btn;
    Fl_Spinner      *size_spin;
    Fl_Box          *preview;
    Fl_Font          selected_id;
    std::string      selected_name;
};

// ---- Colors tab 用構造体 ----
struct ColorEntry {
    const char *label;
    Fl_Color   *target;
    const char *schema_key;  /* calcyx.conf.override の lock 判定用 */
};

struct SwatchData {
    Fl_Color *target;
    DlgState *dlg;
};

static constexpr int MAX_SWATCHES = 30;

struct ColorsTab {
    Fl_Button *swatches[MAX_SWATCHES];
    ColorEntry entries[MAX_SWATCHES];
    SwatchData swatch_data[MAX_SWATCHES];
    int count;
};

// ---- PrefsDialog::run() が保持する全タブの共有状態 ----
struct DlgState {
    FontTab     font;
    ColorsTab   colors;
    Fl_Spinner *fmt_decimal_spin;
    Fl_Check_Button *fmt_exp_chk;
    Fl_Spinner *fmt_exp_pos_spin;
    Fl_Spinner *fmt_exp_neg_spin;
    Fl_Check_Button *fmt_align_chk;
    Fl_Check_Button *sep_thousands_chk;
    Fl_Check_Button *sep_hex_chk;
    Fl_Check_Button *auto_complete_chk;
    Fl_Check_Button *auto_brackets_chk;
    Fl_Check_Button *bs_delete_empty_chk;
    Fl_Check_Button *popup_indep_normal_chk;
    Fl_Check_Button *popup_indep_compact_chk;
    // General tab
    Fl_Check_Button *show_rowlines_chk;
    Fl_Check_Button *remember_pos_chk;
    Fl_Check_Button *start_topmost_chk;
    Fl_Check_Button *menubar_in_window_chk;  // macOS のみ作成・表示
    // System tab
    Fl_Check_Button *tray_chk;
    Fl_Check_Button *hotkey_chk;
    Fl_Check_Button *hotkey_win_chk;
    Fl_Check_Button *hotkey_alt_chk;
    Fl_Check_Button *hotkey_ctrl_chk;
    Fl_Check_Button *hotkey_shift_chk;
    Fl_Choice       *hotkey_key_choice;
    Fl_Choice       *language_choice;  /* General タブ: auto / en / ja */
    Fl_Spinner *limit_array_spin;
    Fl_Spinner *limit_string_spin;
    Fl_Spinner *limit_depth_spin;
    Fl_Choice  *preset_choice;
    SheetView  *sheet;
    SheetView  *font_preview_sv;
    SheetView  *color_preview_sv;
    SheetView  *fmt_preview_sv;
    /* Cancel ボタンで全設定を戻すための単一スナップショット */
    AppSettings::Snapshot saved;
    void (*ui_cb)(void *);
    void       *ui_data;
    Fl_Window  *dlg_win;
    Fl_Tabs    *tabs;
};

// ---- スタイル補助 ----
void style_label(Fl_Widget *w);
void style_check(Fl_Check_Button *chk);
void style_spinner(Fl_Spinner *sp);

// 太字タイトルと枠付き Fl_Group を作る。呼び出し側で end() すること。
// x, y は全体の左上。title が y..y+SECTION_TITLE_H、frame が y+SECTION_TITLE_H..y+SECTION_TITLE_H+body_h。
Fl_Group *begin_section(int x, int y, int w, int body_h, const char *title);
inline constexpr int SECTION_TITLE_H  = 18;
inline constexpr int SECTION_GAP      = 10;
inline constexpr int SECTION_PAD_TOP  = 8;  // 枠上端から子要素までの余白

// ---- 状態同期 / プレビュー ----
void write_dlg_to_globals(DlgState *st);
void refresh_previews(DlgState *st);
void refresh_dlg_colors(DlgState *st);
void update_swatch_labels(DlgState *st);
void update_swatch_state(DlgState *st);

// ---- OK / Apply / Reset アクション ----
void apply_settings(DlgState *st);
void reset_to_defaults(DlgState *st);

// ---- 各タブの構築 (親 Fl_Tabs の子として Fl_Group を追加) ----
void build_general_tab(DlgState &st, int tab_h);
void build_appearance_tab(DlgState &st, int tab_h);
void build_input_tab(DlgState &st, int tab_h);
void build_number_format_tab(DlgState &st, int tab_h);
void build_calculation_tab(DlgState &st, int tab_h);

// ---- Appearance tab 内部で使う (フォントボタンが main dialog 経由で呼ぶ) ----
std::string font_id_to_display_name(Fl_Font id);
void update_font_btn(FontTab *ft);
void update_preview(FontTab *ft);

// ---- calcyx.conf.override ロックを widget に当てるヘルパ ----
// schema_key を user_data に埋め込み、 起動時 locked なら deactivate + tooltip。
// build_*_tab で widget 作成時に make_lockable(new ..., "key") として包む。
// schema 項目を新規追加するときも widget 作成行に key を渡すだけで lock 対応完了。
#include "i18n.h"
template<class W>
W *make_lockable(W *w, const char *schema_key) {
    if (!w || !schema_key) return w;
    w->user_data((void *)schema_key);
    if (settings_locked_keys().count(schema_key)) {
        w->deactivate();
        w->tooltip(_("Locked by calcyx.conf.override"));
    }
    return w;
}
