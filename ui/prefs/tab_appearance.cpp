// tab_appearance.cpp — Appearance タブ (Font + Colors)
//
// PrefsDialog.cpp から抜き出したタブ構築処理。フォント選択 Picker・
// カラースウォッチ・プリセット選択とプレビュー SheetView を配置する。

#include "prefs_common.h"
#include "SheetView.h"
#include <FL/Fl_Hold_Browser.H>
#include <FL/Fl_Color_Chooser.H>
#include <FL/fl_draw.H>
#include <set>
#include <algorithm>
#include <cstring>

// ---- 組み込みフォント ----
static const struct { const char *label; Fl_Font id; bool mono; } BUILTIN_FONTS[] = {
    { "Courier",      FL_COURIER,      true  },
    { "Courier Bold", FL_COURIER_BOLD, true  },
    { "Helvetica",    FL_HELVETICA,    false },
    { "Times",        FL_TIMES,        false },
    { "Screen",       FL_SCREEN,       true  },
    { "Screen Bold",  FL_SCREEN_BOLD,  true  },
};
static const int BUILTIN_COUNT = sizeof(BUILTIN_FONTS) / sizeof(BUILTIN_FONTS[0]);

static std::vector<SysFont> s_all_fonts;
static bool s_fonts_loaded = false;

static bool detect_monospace(Fl_Font id) {
    fl_font(id, 14);
    return fl_width("iiii", 4) == fl_width("MMMM", 4);
}

static void load_system_fonts() {
    if (s_fonts_loaded) return;
    s_fonts_loaded = true;
    Fl_Font n = Fl::set_fonts(nullptr);
    std::set<std::string> seen;
    for (Fl_Font i = 0; i < n; i++) {
        int attr = 0;
        const char *raw = Fl::get_font_name(i, &attr);
        if (!raw || !raw[0]) continue;
        if (raw[0] == ' ' || raw[0] == '.') continue;
        std::string name(raw);
        if (!seen.insert(name).second) continue;
        bool mono = detect_monospace(i);
        s_all_fonts.push_back({name, i, mono});
    }
    std::sort(s_all_fonts.begin(), s_all_fonts.end(),
              [](const SysFont &a, const SysFont &b) { return a.name < b.name; });
}

std::string font_id_to_display_name(Fl_Font id) {
    for (int i = 0; i < BUILTIN_COUNT; i++)
        if (BUILTIN_FONTS[i].id == id) return BUILTIN_FONTS[i].label;
    int attr = 0;
    const char *n = Fl::get_font_name(id, &attr);
    if (n && n[0]) return n;
    return "Courier";
}

void update_font_btn(FontTab *ft) {
    ft->font_btn->copy_label(ft->selected_name.c_str());
    ft->font_btn->labelfont(ft->selected_id);
    ft->font_btn->redraw();
}

void update_preview(FontTab *ft) {
    if (!ft->preview) return;
    ft->preview->labelfont(ft->selected_id);
    ft->preview->labelsize((int)ft->size_spin->value());
    ft->preview->redraw();
}

static void open_font_picker(FontTab *ft, Fl_Window *parent) {
    const int PW = 500, PH = 480;
    Fl_Double_Window picker(PW, PH, "Select Font");
    picker.set_modal();
    picker.color(DLG_BG);

    Fl_Hold_Browser browser(10, 10, PW - 20, PH - 100);
    browser.color(DLG_INPUT);
    browser.textcolor(DLG_TEXT);
    browser.textsize(13);
    browser.selection_color(g_colors.cursor);

    Fl_Check_Button sys_chk(10, PH - 82, 180, 22, "Use system fonts");
    sys_chk.labelcolor(DLG_LABEL);
    sys_chk.labelsize(12);

    Fl_Check_Button prop_chk(200, PH - 82, PW - 210, 22,
                             "Show proportional fonts (not recommended)");
    prop_chk.labelcolor(DLG_LABEL);
    prop_chk.labelsize(12);

    bool cur_builtin = false;
    for (int i = 0; i < BUILTIN_COUNT; i++)
        if (BUILTIN_FONTS[i].id == ft->selected_id) { cur_builtin = true; break; }
    sys_chk.value(cur_builtin ? 0 : 1);

    struct PickerState {
        Fl_Hold_Browser *browser;
        Fl_Check_Button *sys_chk;
        Fl_Check_Button *prop_chk;
        std::vector<Fl_Font> ids;
        Fl_Font cur_id;
    };
    PickerState ps = { &browser, &sys_chk, &prop_chk, {}, ft->selected_id };

    auto rebuild = [](PickerState *ps) {
        ps->browser->clear();
        ps->ids.clear();
        bool use_sys  = ps->sys_chk->value() != 0;
        bool show_all = ps->prop_chk->value() != 0;
        int select = 0;

        if (!use_sys) {
            for (int i = 0; i < BUILTIN_COUNT; i++) {
                if (!show_all && !BUILTIN_FONTS[i].mono) continue;
                if (BUILTIN_FONTS[i].id == ps->cur_id)
                    select = (int)ps->ids.size() + 1;
                ps->ids.push_back(BUILTIN_FONTS[i].id);
                ps->browser->add(BUILTIN_FONTS[i].label);
            }
        } else {
            load_system_fonts();
            for (int i = 0; i < (int)s_all_fonts.size(); i++) {
                if (!show_all && !s_all_fonts[i].monospace) continue;
                if (s_all_fonts[i].id == ps->cur_id)
                    select = (int)ps->ids.size() + 1;
                ps->ids.push_back(s_all_fonts[i].id);
                ps->browser->add(s_all_fonts[i].name.c_str());
            }
        }
        if (select >= 1 && select <= (int)ps->ids.size()) {
            ps->browser->value(select);
            ps->browser->middleline(select);
        }
    };
    rebuild(&ps);

    sys_chk.callback([](Fl_Widget *, void *d) {
        auto *ps = static_cast<PickerState *>(d);
        if (ps->sys_chk->value()) load_system_fonts();
        auto rebuild = [](PickerState *ps) {
            ps->browser->clear(); ps->ids.clear();
            bool use_sys = ps->sys_chk->value() != 0;
            bool show_all = ps->prop_chk->value() != 0;
            int select = 0;
            if (!use_sys) {
                for (int i = 0; i < BUILTIN_COUNT; i++) {
                    if (!show_all && !BUILTIN_FONTS[i].mono) continue;
                    if (BUILTIN_FONTS[i].id == ps->cur_id) select = (int)ps->ids.size() + 1;
                    ps->ids.push_back(BUILTIN_FONTS[i].id);
                    ps->browser->add(BUILTIN_FONTS[i].label);
                }
            } else {
                for (int i = 0; i < (int)s_all_fonts.size(); i++) {
                    if (!show_all && !s_all_fonts[i].monospace) continue;
                    if (s_all_fonts[i].id == ps->cur_id) select = (int)ps->ids.size() + 1;
                    ps->ids.push_back(s_all_fonts[i].id);
                    ps->browser->add(s_all_fonts[i].name.c_str());
                }
            }
            if (select >= 1 && select <= (int)ps->ids.size()) {
                ps->browser->value(select); ps->browser->middleline(select);
            }
        };
        rebuild(ps);
    }, &ps);
    prop_chk.callback(sys_chk.callback(), &ps);

    Fl_Button ok_btn(PW - 170, PH - 40, 75, 30, "OK");
    ok_btn.color(DLG_BTN); ok_btn.labelcolor(DLG_TEXT); ok_btn.labelsize(12);
    Fl_Button cancel_btn(PW - 85, PH - 40, 75, 30, "Cancel");
    cancel_btn.color(DLG_BTN); cancel_btn.labelcolor(DLG_TEXT); cancel_btn.labelsize(12);

    bool accepted = false;
    ok_btn.callback([](Fl_Widget *w, void *d) {
        *static_cast<bool *>(d) = true;
        w->window()->hide();
    }, &accepted);
    cancel_btn.callback([](Fl_Widget *w, void *) { w->window()->hide(); }, nullptr);

    browser.callback([](Fl_Widget *w, void *d) {
        if (Fl::event_clicks()) {
            *static_cast<bool *>(d) = true;
            w->window()->hide();
        }
    }, &accepted);

    picker.end();
    picker.position(parent->x() + (parent->w() - PW) / 2,
                    parent->y() + (parent->h() - PH) / 2);
    picker.show();
    while (picker.shown()) Fl::wait();

    if (accepted) {
        int line = browser.value();
        if (line >= 1 && line <= (int)ps.ids.size()) {
            ft->selected_id = ps.ids[line - 1];
            ft->selected_name = font_id_to_display_name(ft->selected_id);
            update_font_btn(ft);
            update_preview(ft);
        }
    }
}

static void font_btn_cb(Fl_Widget *w, void *data) {
    auto *st = static_cast<DlgState *>(data);
    open_font_picker(&st->font, w->window());
    update_preview(&st->font);
    refresh_previews(st);
}

static void size_change_cb(Fl_Widget *, void *data) {
    auto *st = static_cast<DlgState *>(data);
    update_preview(&st->font);
    refresh_previews(st);
}

static void swatch_cb(Fl_Widget *, void *data) {
    auto *sd = static_cast<SwatchData *>(data);
    if (sd->dlg->preset_choice->value() != COLOR_PRESET_USER_DEFINED)
        return;
    uchar r, g, b;
    Fl::get_color(*sd->target, r, g, b);
    if (fl_color_chooser("Color", r, g, b)) {
        *sd->target = fl_rgb_color(r, g, b);
        /* g_colors と g_user_colors を同期 — グローバルバックアップを更新. */
        g_user_colors = g_colors;
        update_swatch_labels(sd->dlg);
        refresh_previews(sd->dlg);
    }
}

static void preset_change_cb(Fl_Widget *, void *data) {
    auto *st = static_cast<DlgState *>(data);
    colors_apply_preset(st->preset_choice->value());
    update_swatch_state(st);
    refresh_dlg_colors(st);
    refresh_previews(st);
}

/* 現在のプリセット色を g_user_colors にコピーし, user-defined に切替えて
 * 編集可能状態に入る. ユーザーが「近いプリセットをベースに微調整したい」
 * というシナリオを 1 クリックで行えるようにする. */
static void copy_to_user_cb(Fl_Widget *, void *data) {
    auto *st = static_cast<DlgState *>(data);
    g_user_colors = g_colors;
    st->preset_choice->value(COLOR_PRESET_USER_DEFINED);
    colors_apply_preset(COLOR_PRESET_USER_DEFINED);
    update_swatch_state(st);
    refresh_dlg_colors(st);
    refresh_previews(st);
}

void build_appearance_tab(DlgState &st, int tab_h) {
    Fl_Group *g = new Fl_Group(5, 30, DW - 10, tab_h - 25, " Appearance ");
    g->color(DLG_BG);
    g->selection_color(DLG_BG);
    g->labelcolor(DLG_TEXT);
    g->labelsize(12);

    int lx = 20, ly = 50;

    // --- Font ---
    Fl_Box *sec_font = new Fl_Box(lx, ly, 200, 20, "Font");
    sec_font->labelcolor(DLG_LABEL);
    sec_font->labelsize(12);
    sec_font->labelfont(FL_BOLD);
    sec_font->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    ly += 22;

    int lw = 50;
    st.font.selected_id = g_font_id;
    st.font.selected_name = font_id_to_display_name(g_font_id);

    Fl_Box *lb1 = new Fl_Box(lx + 10, ly, lw, 25, "Font:");
    style_label(lb1);
    lb1->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    st.font.font_btn = new Fl_Button(lx + 10 + lw, ly, DW - lx - lw - 50, 25);
    st.font.font_btn->box(FL_DOWN_BOX);
    st.font.font_btn->color(DLG_INPUT);
    st.font.font_btn->labelcolor(DLG_TEXT);
    st.font.font_btn->labelsize(13);
    st.font.font_btn->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    st.font.font_btn->callback(font_btn_cb, &st);
    update_font_btn(&st.font);

    ly += 28;
    Fl_Box *lb2 = new Fl_Box(lx + 10, ly, lw, 25, "Size:");
    style_label(lb2);
    lb2->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    st.font.size_spin = new Fl_Spinner(lx + 10 + lw, ly, 80, 25);
    style_spinner(st.font.size_spin);
    st.font.size_spin->range(8, 36);
    st.font.size_spin->step(1);
    st.font.size_spin->value(g_font_size);
    st.font.size_spin->callback(size_change_cb, &st);
    ly += 30;

    // --- Colors ---
    Fl_Box *sec_col = new Fl_Box(lx, ly, 200, 20, "Colors");
    sec_col->labelcolor(DLG_LABEL);
    sec_col->labelsize(12);
    sec_col->labelfont(FL_BOLD);
    sec_col->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    ly += 22;

    Fl_Box *lb_preset = new Fl_Box(lx + 10, ly, 60, 25, "Preset:");
    style_label(lb_preset);
    lb_preset->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    st.preset_choice = new Fl_Choice(lx + 70, ly, 200, 25);
    st.preset_choice->color(DLG_INPUT);
    st.preset_choice->textcolor(DLG_TEXT);
    st.preset_choice->labelsize(12);
    st.preset_choice->textsize(12);
    st.preset_choice->selection_color(g_colors.cursor);
    for (int i = 0; i < COLOR_PRESET_COUNT; i++)
        st.preset_choice->add(COLOR_PRESET_INFO[i].label);
    st.preset_choice->value(g_color_preset);
    st.preset_choice->callback(preset_change_cb, &st);

    Fl_Button *copy_btn = new Fl_Button(lx + 275, ly, 150, 25, "Copy to user-defined");
    copy_btn->color(DLG_BTN);
    copy_btn->labelcolor(DLG_TEXT);
    copy_btn->labelsize(12);
    copy_btn->tooltip("Copy current preset colors to user-defined and switch to it for editing");
    copy_btn->callback(copy_to_user_cb, &st);
    ly += 28;

    struct { const char *label; Fl_Color *color; } entries[] = {
        { "Background",   &g_colors.bg },
        { "Selection",    &g_colors.sel_bg },
        { "Row Line",     &g_colors.rowline },
        { "Separator",    &g_colors.sep },
        { "Text",         &g_colors.text },
        { "Cursor",       &g_colors.cursor },
        { "Symbols",      &g_colors.symbol },
        { "Identifiers",  &g_colors.ident },
        { "Literals",     &g_colors.special },
        { "SI Prefix",    &g_colors.si_pfx },
        { "Paren 1",      &g_colors.paren[0] },
        { "Paren 2",      &g_colors.paren[1] },
        { "Paren 3",      &g_colors.paren[2] },
        { "Paren 4",      &g_colors.paren[3] },
        { "Error",        &g_colors.error },
        { "Win BG",       &g_colors.ui_win_bg },
        { "Dlg BG",       &g_colors.ui_bg },
        { "UI Input",     &g_colors.ui_input },
        { "UI Button",    &g_colors.ui_btn },
        { "Menu BG",      &g_colors.ui_menu },
        { "UI Text",      &g_colors.ui_text },
        { "UI Label",     &g_colors.ui_label },
        { "UI Dim",       &g_colors.ui_dim },
        { "Popup BG",     &g_colors.pop_bg },
        { "Popup Sel",    &g_colors.pop_sel },
        { "Popup Text",   &g_colors.pop_text },
        { "Popup Desc",   &g_colors.pop_desc },
        { "Popup DescBG", &g_colors.pop_desc_bg },
        { "Popup Border", &g_colors.pop_border },
    };

    const int n_entries = 29;
    st.colors.count = n_entries;
    const int cols = 3, rows = 10;
    int col_w = (DW - 40) / cols;
    int sy = ly;
    for (int i = 0; i < n_entries; i++) {
        int c = i / rows, r = i % rows;
        int cx = lx + 10 + c * col_w;
        int cy = sy + r * 24;
        Fl_Box *lb = new Fl_Box(cx, cy, 70, 20, entries[i].label);
        style_label(lb);
        lb->labelsize(11);
        lb->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

        auto *btn = new Fl_Button(cx + 70, cy, 80, 20);
        btn->box(FL_DOWN_BOX);
        btn->color(*entries[i].color);
        btn->labelsize(10);
        btn->align(FL_ALIGN_INSIDE);
        st.colors.swatch_data[i] = { entries[i].color, &st };
        btn->callback(swatch_cb, &st.colors.swatch_data[i]);
        st.colors.swatches[i] = btn;
        st.colors.entries[i] = { entries[i].label, entries[i].color };
    }

    ly = sy + rows * 24 + 4;

    st.show_rowlines_chk = new Fl_Check_Button(lx + 10, ly, 300, 22, "Show row separator lines");
    style_check(st.show_rowlines_chk);
    st.show_rowlines_chk->value(g_show_rowlines ? 1 : 0);
    st.show_rowlines_chk->callback([](Fl_Widget *, void *data) {
        auto *st = static_cast<DlgState *>(data);
        g_show_rowlines = st->show_rowlines_chk->value() != 0;
        refresh_previews(st);
    }, &st);
    ly += 26;

    // --- Preview ---
    // 上: 直前のウィジェットとの間に小さな余白 / 下: タブ枠との間に同じ余白
    const int PREVIEW_MARGIN = 6;
    ly += PREVIEW_MARGIN;
    int preview_h = tab_h - 25 - (ly - 30) - PREVIEW_MARGIN;
    st.font_preview_sv = nullptr;
    st.color_preview_sv = new SheetView(lx, ly, DW - 50, preview_h, true);
    st.font.preview = nullptr;

    g->end();
}
