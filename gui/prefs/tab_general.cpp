// tab_general.cpp — General タブ (Window / System Tray / Global Hotkey / Configuration)

#include "prefs_common.h"
#include "i18n.h"
#include "app_prefs.h"
#include "platform_tray.h"
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <cstdlib>
#include <string>

#if defined(_WIN32)
#  include <windows.h>
#  include <shellapi.h>
#endif

static void open_config_dir_cb(Fl_Widget *, void *) {
    std::string dir = AppPrefs::config_dir();  /* UTF-8 */
#if defined(_WIN32)
    /* ShellExecuteA は CP932 解釈するので UTF-8 直渡しで文字化けする。
     * UTF-16 化して ShellExecuteW を呼ぶ。 */
    wchar_t wpath[MAX_PATH];
    int n = MultiByteToWideChar(CP_UTF8, 0, dir.c_str(), -1, wpath,
                                (int)(sizeof(wpath) / sizeof(wpath[0])));
    if (n > 0) {
        ShellExecuteW(NULL, L"open", wpath, NULL, NULL, SW_SHOWNORMAL);
    }
#elif defined(__APPLE__)
    std::string cmd = "open \"" + dir + "\"";
    if (system(cmd.c_str())) {}
#else
    std::string cmd = "xdg-open \"" + dir + "\" 2>/dev/null &";
    if (system(cmd.c_str())) {}
#endif
}

void build_general_tab(DlgState &st, int tab_h) {
    Fl_Group *g = new Fl_Group(5, 30, DW - 10, tab_h - 25, _(" General "));
    g->color(DLG_BG);
    g->selection_color(DLG_BG);
    g->labelcolor(DLG_TEXT);
    g->labelsize(12);

    const int lx = 16;
    const int sw = DW - 40;
    int ly = 50;

    // ===== Language =====
    {
        int body_h = 56;
        Fl_Group *sec = begin_section(lx, ly, sw, body_h, _("Language"));
        int inner_y = ly + SECTION_TITLE_H + SECTION_PAD_TOP;
        Fl_Box *lb = new Fl_Box(lx + 10, inner_y, 80, 22, _("Language:"));
        lb->box(FL_NO_BOX);
        lb->labelcolor(DLG_LABEL);
        lb->labelsize(12);
        lb->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

        st.language_choice = make_lockable(
            new Fl_Choice(lx + 90, inner_y, 200, 22), "language");
        st.language_choice->color(DLG_INPUT);
        st.language_choice->textcolor(DLG_TEXT);
        st.language_choice->labelsize(12);
        st.language_choice->textsize(12);
        st.language_choice->add("auto (follow OS)");
        st.language_choice->add("English");
        st.language_choice->add("\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e");  /* 日本語 */
        if (g_language == "en")      st.language_choice->value(1);
        else if (g_language == "ja") st.language_choice->value(2);
        else                          st.language_choice->value(0);  /* auto */

        Fl_Box *note = new Fl_Box(lx + 30, inner_y + 24, sw - 40, 18,
            _("Restart calcyx after changing language."));
        note->box(FL_NO_BOX);
        note->labelcolor(DLG_LABEL);
        note->labelsize(11);
        note->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

        sec->end();
        ly += SECTION_TITLE_H + body_h + SECTION_GAP;
    }

    // ===== Window =====
    {
        int body_h = 90;
#ifdef __APPLE__
        body_h += 44;  /* macOS: グローバルメニュー併用トグル + note 1 行ぶん */
#endif
        Fl_Group *sec = begin_section(lx, ly, sw, body_h, _("Window"));
        int inner_y = ly + SECTION_TITLE_H + SECTION_PAD_TOP;
        st.remember_pos_chk = make_lockable(
            new Fl_Check_Button(lx + 10, inner_y, sw - 20, 22,
                _("Remember window position on exit")),
            "remember_position");
        style_check(st.remember_pos_chk);
        st.remember_pos_chk->value(g_remember_position ? 1 : 0);
        inner_y += 26;

        st.start_topmost_chk = make_lockable(
            new Fl_Check_Button(lx + 10, inner_y, sw - 20, 22,
                _("Start with Always on Top")),
            "start_topmost");
        style_check(st.start_topmost_chk);
        st.start_topmost_chk->value(g_start_topmost ? 1 : 0);
        inner_y += 22;

        Fl_Box *note = new Fl_Box(lx + 30, inner_y, sw - 40, 16,
            _("Sets the initial state at launch. Toggle anytime from View menu or pin button."));
        note->box(FL_NO_BOX);
        note->labelcolor(DLG_LABEL);
        note->labelsize(11);
        note->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        inner_y += 18;

#ifdef __APPLE__
        st.menubar_in_window_chk = make_lockable(
            new Fl_Check_Button(lx + 10, inner_y, sw - 20, 22,
                _("Show menu items in window menu bar")),
            "gui_menubar_in_window");
        style_check(st.menubar_in_window_chk);
        st.menubar_in_window_chk->value(g_gui_menubar_in_window ? 1 : 0);
        inner_y += 22;

        Fl_Box *note2 = new Fl_Box(lx + 30, inner_y, sw - 40, 16,
            _("Items always appear in the global menu bar. Restart to apply."));
        note2->box(FL_NO_BOX);
        note2->labelcolor(DLG_LABEL);
        note2->labelsize(11);
        note2->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
#else
        st.menubar_in_window_chk = nullptr;
#endif

        sec->end();
        ly += SECTION_TITLE_H + body_h + SECTION_GAP;
    }

    // ===== System Tray =====
    {
        int body_h = 56;
        Fl_Group *sec = begin_section(lx, ly, sw, body_h, _("System Tray"));
        int inner_y = ly + SECTION_TITLE_H + SECTION_PAD_TOP;
        st.tray_chk = make_lockable(
            new Fl_Check_Button(lx + 10, inner_y, sw - 20, 22,
                _("Enable system tray icon")),
            "tray_icon");
        style_check(st.tray_chk);
        st.tray_chk->value(g_tray_icon ? 1 : 0);

        Fl_Box *note = new Fl_Box(lx + 30, inner_y + 24, sw - 40, 18,
            _("When enabled, closing the window minimizes to tray."));
        note->box(FL_NO_BOX);
        note->labelcolor(DLG_LABEL);
        note->labelsize(11);
        note->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        sec->end();
        ly += SECTION_TITLE_H + body_h + SECTION_GAP;
    }

    // ===== Global Hotkey =====
    {
        int body_h = 94;
        Fl_Group *sec = begin_section(lx, ly, sw, body_h, _("Global Hotkey"));
        int inner_y = ly + SECTION_TITLE_H + SECTION_PAD_TOP;

        st.hotkey_chk = make_lockable(
            new Fl_Check_Button(lx + 10, inner_y, sw - 20, 22,
                _("Enable global hotkey")),
            "hotkey_enabled");
        style_check(st.hotkey_chk);
        st.hotkey_chk->value(g_hotkey_enabled ? 1 : 0);
        inner_y += 26;

        Fl_Box *mod_label = new Fl_Box(lx + 30, inner_y, 75, 22, _("Modifiers:"));
        style_label(mod_label);
        mod_label->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        int mx = lx + 110;

#ifdef __APPLE__
        st.hotkey_win_chk = make_lockable(
            new Fl_Check_Button(mx, inner_y, 60, 22, "Cmd"), "hotkey_win");
        style_check(st.hotkey_win_chk);
        st.hotkey_win_chk->value(g_hotkey_win ? 1 : 0);
        mx += 60;
        st.hotkey_alt_chk = make_lockable(
            new Fl_Check_Button(mx, inner_y, 65, 22, "Option"), "hotkey_alt");
        style_check(st.hotkey_alt_chk);
        st.hotkey_alt_chk->value(g_hotkey_alt ? 1 : 0);
        mx += 65;
        st.hotkey_ctrl_chk = make_lockable(
            new Fl_Check_Button(mx, inner_y, 60, 22, "Ctrl"), "hotkey_ctrl");
        style_check(st.hotkey_ctrl_chk);
        st.hotkey_ctrl_chk->value(g_hotkey_ctrl ? 1 : 0);
        mx += 60;
        st.hotkey_shift_chk = make_lockable(
            new Fl_Check_Button(mx, inner_y, 60, 22, "Shift"), "hotkey_shift");
#else
        st.hotkey_win_chk = make_lockable(
            new Fl_Check_Button(mx, inner_y, 55, 22, "Win"), "hotkey_win");
        style_check(st.hotkey_win_chk);
        st.hotkey_win_chk->value(g_hotkey_win ? 1 : 0);
        mx += 55;
        st.hotkey_alt_chk = make_lockable(
            new Fl_Check_Button(mx, inner_y, 50, 22, "Alt"), "hotkey_alt");
        style_check(st.hotkey_alt_chk);
        st.hotkey_alt_chk->value(g_hotkey_alt ? 1 : 0);
        mx += 50;
        st.hotkey_ctrl_chk = make_lockable(
            new Fl_Check_Button(mx, inner_y, 55, 22, "Ctrl"), "hotkey_ctrl");
        style_check(st.hotkey_ctrl_chk);
        st.hotkey_ctrl_chk->value(g_hotkey_ctrl ? 1 : 0);
        mx += 55;
        st.hotkey_shift_chk = make_lockable(
            new Fl_Check_Button(mx, inner_y, 60, 22, "Shift"), "hotkey_shift");
#endif
        style_check(st.hotkey_shift_chk);
        st.hotkey_shift_chk->value(g_hotkey_shift ? 1 : 0);
        inner_y += 26;

        Fl_Box *key_label = new Fl_Box(lx + 30, inner_y, 75, 22, _("Key:"));
        style_label(key_label);
        key_label->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

        st.hotkey_key_choice = make_lockable(
            new Fl_Choice(lx + 110, inner_y, 100, 22), "hotkey_key");
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
        sec->end();

        // enable チェックで依存ウィジェットをグレーアウト
        auto hotkey_sync = [](Fl_Widget *, void *data) {
            DlgState *s = static_cast<DlgState *>(data);
            bool on = s->hotkey_chk->value() != 0;
            auto set = [on](Fl_Widget *w) { if (on) w->activate(); else w->deactivate(); };
            set(s->hotkey_win_chk);
            set(s->hotkey_alt_chk);
            set(s->hotkey_ctrl_chk);
            set(s->hotkey_shift_chk);
            set(s->hotkey_key_choice);
        };
        st.hotkey_chk->callback(hotkey_sync, &st);
        hotkey_sync(nullptr, &st);

        ly += SECTION_TITLE_H + body_h + SECTION_GAP;
    }

    // ===== Configuration =====
    {
        int body_h = 70;
        Fl_Group *sec = begin_section(lx, ly, sw, body_h, _("Configuration"));
        int inner_y = ly + SECTION_TITLE_H + SECTION_PAD_TOP;

        std::string cfg_dir = AppPrefs::config_dir();
        Fl_Box *path_box = new Fl_Box(lx + 10, inner_y, sw - 20, 18);
        path_box->copy_label(cfg_dir.c_str());
        path_box->labelcolor(DLG_TEXT);
        path_box->labelsize(11);
        path_box->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

        Fl_Button *open_btn = new Fl_Button(lx + 10, inner_y + 22, 120, 26, _("Open folder"));
        open_btn->color(DLG_BTN);
        open_btn->labelcolor(DLG_TEXT);
        open_btn->labelsize(12);
        open_btn->callback(open_config_dir_cb, nullptr);
        sec->end();
        ly += SECTION_TITLE_H + body_h + SECTION_GAP;
    }

    g->end();
}
