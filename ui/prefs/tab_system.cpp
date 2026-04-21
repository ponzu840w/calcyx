// tab_system.cpp — System タブ (System Tray + Global Hotkey)

#include "prefs_common.h"
#include "platform_tray.h"
#include <FL/Fl_Box.H>

void build_system_tab(DlgState &st, int tab_h) {
    Fl_Group *g = new Fl_Group(5, 30, DW - 10, tab_h - 25, " System ");
    g->color(DLG_BG);
    g->selection_color(DLG_BG);
    g->labelcolor(DLG_TEXT);
    g->labelsize(12);

    int lx = 20, ly = 55;

    // --- System Tray ---
    Fl_Box *sec_tray = new Fl_Box(lx, ly, 200, 20, "System Tray");
    sec_tray->labelcolor(DLG_LABEL);
    sec_tray->labelsize(12);
    sec_tray->labelfont(FL_BOLD);
    sec_tray->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    ly += 25;

    st.tray_chk = new Fl_Check_Button(lx + 10, ly, 300, 25, "Enable system tray icon");
    style_check(st.tray_chk);
    st.tray_chk->value(g_tray_icon ? 1 : 0);
    ly += 25;

    Fl_Box *tray_note = new Fl_Box(lx + 30, ly, 400, 20, "When enabled, closing the window minimizes to tray.");
    tray_note->labelcolor(DLG_LABEL);
    tray_note->labelsize(11);
    tray_note->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    ly += 35;

    // --- Global Hotkey ---
    Fl_Box *sec_hk = new Fl_Box(lx, ly, 200, 20, "Global Hotkey");
    sec_hk->labelcolor(DLG_LABEL);
    sec_hk->labelsize(12);
    sec_hk->labelfont(FL_BOLD);
    sec_hk->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    ly += 25;

    st.hotkey_chk = new Fl_Check_Button(lx + 10, ly, 300, 25, "Enable global hotkey");
    style_check(st.hotkey_chk);
    st.hotkey_chk->value(g_hotkey_enabled ? 1 : 0);
    ly += 30;

    Fl_Box *mod_label = new Fl_Box(lx + 30, ly, 80, 25, "Modifiers:");
    style_label(mod_label);
    mod_label->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    int mx = lx + 110;

#ifdef __APPLE__
    st.hotkey_win_chk = new Fl_Check_Button(mx, ly, 60, 25, "Cmd");
    style_check(st.hotkey_win_chk);
    st.hotkey_win_chk->value(g_hotkey_win ? 1 : 0);
    mx += 60;
    st.hotkey_alt_chk = new Fl_Check_Button(mx, ly, 65, 25, "Option");
    style_check(st.hotkey_alt_chk);
    st.hotkey_alt_chk->value(g_hotkey_alt ? 1 : 0);
    mx += 65;
    st.hotkey_ctrl_chk = new Fl_Check_Button(mx, ly, 60, 25, "Ctrl");
    style_check(st.hotkey_ctrl_chk);
    st.hotkey_ctrl_chk->value(g_hotkey_ctrl ? 1 : 0);
    mx += 60;
    st.hotkey_shift_chk = new Fl_Check_Button(mx, ly, 60, 25, "Shift");
#else
    st.hotkey_win_chk = new Fl_Check_Button(mx, ly, 55, 25, "Win");
    style_check(st.hotkey_win_chk);
    st.hotkey_win_chk->value(g_hotkey_win ? 1 : 0);
    mx += 55;
    st.hotkey_alt_chk = new Fl_Check_Button(mx, ly, 50, 25, "Alt");
    style_check(st.hotkey_alt_chk);
    st.hotkey_alt_chk->value(g_hotkey_alt ? 1 : 0);
    mx += 50;
    st.hotkey_ctrl_chk = new Fl_Check_Button(mx, ly, 55, 25, "Ctrl");
    style_check(st.hotkey_ctrl_chk);
    st.hotkey_ctrl_chk->value(g_hotkey_ctrl ? 1 : 0);
    mx += 55;
    st.hotkey_shift_chk = new Fl_Check_Button(mx, ly, 60, 25, "Shift");
#endif
    style_check(st.hotkey_shift_chk);
    st.hotkey_shift_chk->value(g_hotkey_shift ? 1 : 0);
    ly += 30;

    Fl_Box *key_label = new Fl_Box(lx + 30, ly, 80, 25, "Key:");
    style_label(key_label);
    key_label->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    st.hotkey_key_choice = new Fl_Choice(lx + 110, ly, 100, 25);
    st.hotkey_key_choice->color(DLG_INPUT);
    st.hotkey_key_choice->textcolor(DLG_TEXT);
    st.hotkey_key_choice->labelcolor(DLG_LABEL);
    st.hotkey_key_choice->textsize(12);
    int key_count = plat_key_names_count();
    const char *const *key_names = plat_key_names();
    int sel_idx = 0;
    for (int i = 0; i < key_count; i++) {
        st.hotkey_key_choice->add(key_names[i]);
        if (plat_keyname_to_flkey(key_names[i]) == g_hotkey_keycode)
            sel_idx = i;
    }
    st.hotkey_key_choice->value(sel_idx);

    g->end();
}
