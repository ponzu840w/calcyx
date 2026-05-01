// tab_general.cpp — General タブ (Language / Configuration)
//
// Window / System Tray / Global Hotkey は tab_window.cpp に分離。

#include "prefs_common.h"
#include "i18n.h"
#include "app_prefs.h"
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <cstdlib>
#include <string>

#if defined(_WIN32)
#  include <windows.h>
#  include <shellapi.h>
#endif

#if !defined(_WIN32)
/* shell に渡す引数を single-quote で安全に包む。 中身の ' は '\\'' に置換。
 * AppData 配下の path に通常は記号は混じらないが, ホームディレクトリに
 * "$" や "'" が含まれる稀環境でも壊れないようにする (シェルインジェクション
 * 対策の規格化)。 */
static std::string shell_quote(const std::string &s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else           out += c;
    }
    out += "'";
    return out;
}
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
    std::string cmd = "open " + shell_quote(dir);
    if (system(cmd.c_str())) {}
#else
    std::string cmd = "xdg-open " + shell_quote(dir) + " 2>/dev/null &";
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
            _("A restart is required after changing the language."));
        note->box(FL_NO_BOX);
        note->labelcolor(DLG_LABEL);
        note->labelsize(11);
        note->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

        sec->end();
        ly += SECTION_TITLE_H + body_h + SECTION_GAP;
    }

    // ===== Configuration =====
    {
        int body_h = 160;
        Fl_Group *sec = begin_section(lx, ly, sw, body_h, _("Configuration"));
        int inner_y = ly + SECTION_TITLE_H + SECTION_PAD_TOP;

        std::string cfg_dir = AppPrefs::config_dir();
        Fl_Box *path_box = new Fl_Box(lx + 10, inner_y, sw - 20, 18);
        path_box->copy_label(cfg_dir.c_str());
        path_box->labelcolor(DLG_TEXT);
        path_box->labelsize(11);
        path_box->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        inner_y += 22;

        Fl_Button *open_btn = new Fl_Button(lx + 10, inner_y, 120, 26, _("Open folder"));
        open_btn->color(DLG_BTN);
        open_btn->labelcolor(DLG_TEXT);
        open_btn->labelsize(12);
        open_btn->callback(open_config_dir_cb, nullptr);
        inner_y += 32;

        /* ファイル説明 (calcyx.conf / calcyx.conf.override)。
         * ファイル名は固定リテラル (翻訳しない)、 説明文は i18n 対象。 */
        auto add_filename = [&](int y, const char *name) {
            Fl_Box *b = new Fl_Box(lx + 10, y, sw - 20, 16, name);
            b->box(FL_NO_BOX);
            b->labelcolor(DLG_TEXT);
            b->labelfont(FL_HELVETICA_BOLD);
            b->labelsize(11);
            b->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        };
        auto add_desc = [&](int y, int h, const char *text) {
            Fl_Box *b = new Fl_Box(lx + 30, y, sw - 40, h, text);
            b->box(FL_NO_BOX);
            b->labelcolor(DLG_LABEL);
            b->labelsize(11);
            b->align(FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
        };

        add_filename(inner_y, "calcyx.conf");
        inner_y += 16;
        add_desc(inner_y, 16,
            _("Records changes made in this Preferences dialogue. May also be edited directly in a text editor."));
        inner_y += 20;

        add_filename(inner_y, "calcyx.conf.override");
        inner_y += 16;
        add_desc(inner_y, 32,
            _("Settings here take precedence over calcyx.conf. Not editable from this dialogue; intended for users who prefer textual configuration."));
        inner_y += 32;

        sec->end();
        ly += SECTION_TITLE_H + body_h + SECTION_GAP;
    }

    g->end();
}
