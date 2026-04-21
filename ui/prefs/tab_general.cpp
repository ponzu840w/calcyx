// tab_general.cpp — General タブ (Window / Calculation Limits / Configuration)

#include "prefs_common.h"
#include "app_prefs.h"
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <cstdlib>
#include <string>

#if defined(_WIN32)
#  include <windows.h>
#  include <shellapi.h>
#endif

static void open_config_dir_cb(Fl_Widget *, void *) {
    std::string dir = AppPrefs::config_dir();
#if defined(_WIN32)
    ShellExecuteA(NULL, "open", dir.c_str(), NULL, NULL, SW_SHOWNORMAL);
#elif defined(__APPLE__)
    std::string cmd = "open \"" + dir + "\"";
    if (system(cmd.c_str())) {}
#else
    std::string cmd = "xdg-open \"" + dir + "\" 2>/dev/null &";
    if (system(cmd.c_str())) {}
#endif
}

void build_general_tab(DlgState &st, int tab_h) {
    Fl_Group *g = new Fl_Group(5, 30, DW - 10, tab_h - 25, " General ");
    g->color(DLG_BG);
    g->selection_color(DLG_BG);
    g->labelcolor(DLG_TEXT);
    g->labelsize(12);

    int lx = 20, ly = 55;

    // --- Window ---
    Fl_Box *sec_win = new Fl_Box(lx, ly, 200, 20, "Window");
    sec_win->labelcolor(DLG_LABEL);
    sec_win->labelsize(12);
    sec_win->labelfont(FL_BOLD);
    sec_win->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    ly += 25;

    st.remember_pos_chk = new Fl_Check_Button(lx + 10, ly, 300, 25, "Remember window position on exit");
    style_check(st.remember_pos_chk);
    st.remember_pos_chk->value(g_remember_position ? 1 : 0);
    ly += 40;

    // --- Calculation Limits ---
    Fl_Box *sec_lim = new Fl_Box(lx, ly, 200, 20, "Calculation Limits");
    sec_lim->labelcolor(DLG_LABEL);
    sec_lim->labelsize(12);
    sec_lim->labelfont(FL_BOLD);
    sec_lim->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    ly += 25;

    int slx = lx + 10, slw = 190, ssw = 100;

    Fl_Box *lb_arr = new Fl_Box(slx, ly, slw, 25, "Max array length:");
    style_label(lb_arr);
    lb_arr->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    st.limit_array_spin = new Fl_Spinner(slx + slw, ly, ssw, 25);
    style_spinner(st.limit_array_spin);
    st.limit_array_spin->range(1, 1000000);
    st.limit_array_spin->step(1);
    st.limit_array_spin->value(g_limit_max_array_length);
    ly += 30;

    Fl_Box *lb_str = new Fl_Box(slx, ly, slw, 25, "Max string length:");
    style_label(lb_str);
    lb_str->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    st.limit_string_spin = new Fl_Spinner(slx + slw, ly, ssw, 25);
    style_spinner(st.limit_string_spin);
    st.limit_string_spin->range(1, 1000000);
    st.limit_string_spin->step(1);
    st.limit_string_spin->value(g_limit_max_string_length);
    ly += 30;

    Fl_Box *lb_dep = new Fl_Box(slx, ly, slw, 25, "Max call recursion depth:");
    style_label(lb_dep);
    lb_dep->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    st.limit_depth_spin = new Fl_Spinner(slx + slw, ly, ssw, 25);
    style_spinner(st.limit_depth_spin);
    st.limit_depth_spin->range(1, 1000);
    st.limit_depth_spin->step(1);
    st.limit_depth_spin->value(g_limit_max_call_depth);
    ly += 40;

    // --- Configuration ---
    Fl_Box *sec_cfg = new Fl_Box(lx, ly, 200, 20, "Configuration");
    sec_cfg->labelcolor(DLG_LABEL);
    sec_cfg->labelsize(12);
    sec_cfg->labelfont(FL_BOLD);
    sec_cfg->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    ly += 25;

    std::string cfg_dir = AppPrefs::config_dir();
    Fl_Box *path_box = new Fl_Box(slx, ly, DW - 60, 20);
    path_box->copy_label(cfg_dir.c_str());
    path_box->labelcolor(DLG_TEXT);
    path_box->labelsize(11);
    path_box->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    ly += 25;

    Fl_Button *open_dir_btn = new Fl_Button(slx, ly, 120, 28, "Open folder");
    open_dir_btn->color(DLG_BTN);
    open_dir_btn->labelcolor(DLG_TEXT);
    open_dir_btn->labelsize(12);
    open_dir_btn->callback(open_config_dir_cb, nullptr);

    g->end();
}
