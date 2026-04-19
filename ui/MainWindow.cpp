// 移植元: Calctus/UI/MainForm.cs (簡略版)

#include "MainWindow.h"
#include "PrefsDialog.h"
#include "settings_globals.h"
#include "platform_tray.h"
#include "colors.h"
#include <FL/Fl.H>
#include <FL/Fl_Native_File_Chooser.H>
#include "app_prefs.h"
#include <FL/fl_ask.H>
#include <FL/Fl_SVG_Image.H>
#include <FL/Fl_Help_View.H>
#include <FL/fl_utf8.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <sys/stat.h>
#include <dirent.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <libgen.h>
extern "C" void mac_set_window_level(Fl_Window *win, int topmost);
#endif
#ifdef _WIN32
#include <FL/platform.H>
#include <windows.h>
#elif !defined(__APPLE__)
#include <FL/platform.H>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#endif

// フォーマット定義 (移植元: RepresentaionFuncs.cs)
static const struct { const char *label; const char *func_name; } FMT_DEFS[] = {
    { "Auto",  nullptr  },
    { "Dec",   "dec"    },
    { "Hex",   "hex"    },
    { "Bin",   "bin"    },
    { "Oct",   "oct"    },
    { "SI",    "si"     },
    { "Kibi",  "kibi"   },
    { "Char",  "char"   },
};
static const int FMT_COUNT = 8;

static std::string find_icon_svg();

// UI クロームカラーは g_colors.ui_* から取得
#define C_WIN_BG   g_colors.ui_win_bg
#define C_MENU_BG  g_colors.ui_menu
#define C_MENU_FG  g_colors.ui_text

MainWindow::MainWindow(int w, int h, const char *title)
    : Fl_Double_Window(w, h, title)
{
    color(C_WIN_BG);

    // ---- メニューバー ----
    int mw = calc_menu_w(w);
    menu_ = new Fl_Menu_Bar(0, 0, mw, MENU_H);
    menu_->color(C_MENU_BG);
    menu_->textcolor(C_MENU_FG);
    menu_->box(FL_FLAT_BOX);
    menu_->add("&File/All &Clear",    FL_COMMAND | FL_SHIFT | FL_Delete, menu_cb, (void*)"clear_all", FL_MENU_DIVIDER);
    menu_->add("&File/&Open...",     FL_COMMAND + 'o', menu_cb, (void*)"open");
    menu_->add("&File/&Save As...",  FL_COMMAND + 's', menu_cb, (void*)"save", FL_MENU_DIVIDER);
    menu_->add("&Edit/&Undo",       FL_COMMAND + 'z', menu_cb, (void*)"undo");
    menu_->add("&Edit/&Redo",       FL_COMMAND + 'y', menu_cb, (void*)"redo", FL_MENU_DIVIDER);
    menu_->add("&Edit/Copy &All",   FL_COMMAND | FL_SHIFT + 'c', menu_cb, (void*)"copy_all", FL_MENU_DIVIDER);
    menu_->add("&Edit/&Insert Row Below", FL_Enter,            menu_cb, (void*)"insert_below");
    menu_->add("&Edit/Insert Row A&bove", FL_SHIFT | FL_Enter, menu_cb, (void*)"insert_above");
    menu_->add("&Edit/&Delete Row",       FL_COMMAND | FL_Delete,          menu_cb, (void*)"delete_row");
    menu_->add("&Edit/Move Row &Up",      FL_COMMAND | FL_SHIFT | FL_Up,   menu_cb, (void*)"move_up");
    menu_->add("&Edit/Move Row Do&wn",    FL_COMMAND | FL_SHIFT | FL_Down, menu_cb, (void*)"move_down", FL_MENU_DIVIDER);
    menu_->add("&Edit/&Recalculate", FL_F + 5, menu_cb, (void*)"recalc");
    menu_->add("&View/Always on &Top", FL_COMMAND + 't', menu_cb, (void*)"topmost", FL_MENU_TOGGLE);
    populate_samples_menu();
    menu_->add("&File/&Preferences...", FL_COMMAND + ',', menu_cb, (void*)"prefs", FL_MENU_DIVIDER);
    menu_->add("&File/E&xit",         0,                menu_cb, (void*)"exit");

    // 全メニュー追加後にインデックスを取得
    // find_index(path) はショートカット付きラベルで失敗するので手動検索
    mi_undo_ = mi_redo_ = mi_topmost_ = -1;
    for (int i = 0; i < menu_->size(); i++) {
        const Fl_Menu_Item &it = menu_->menu()[i];
        if (!it.label()) continue;
        if (it.callback() == menu_cb) {
            const char *d = (const char *)it.user_data();
            if (d && strcmp(d, "undo") == 0) mi_undo_ = i;
            if (d && strcmp(d, "redo") == 0) mi_redo_ = i;
            if (d && strcmp(d, "topmost") == 0) mi_topmost_ = i;
        }
    }

    // ---- ← → 📌 ? ツールバーボタン (右寄せ) ----
    auto make_btn = [&](int bx, int bw, const char *label, const char *cmd) {
        auto *b = new Fl_Button(bx, 0, bw, MENU_H, label);
        b->box(FL_FLAT_BOX);
        b->color(C_MENU_BG);
        b->labelcolor(C_MENU_FG);
        b->labelsize(14);
        b->callback(menu_cb, (void*)cmd);
        b->visible_focus(0);
        return b;
    };
    // 右端から: [Format▼] PAD [?] PAD [📌] [← →]
    int rx = w - CHOICE_W;                              // Format▼
    rx -= PAD + ABOUT_W;                                // ?
    btn_about_ = make_btn(rx, ABOUT_W, "?", "about");
    btn_about_->labelsize(13);
    rx -= PIN_W;                                        // 📌
    btn_topmost_ = make_btn(rx, PIN_W, "@menu", "topmost");
    btn_topmost_->labelsize(10);
    rx -= BTN_W;                                        // →
    btn_redo_ = make_btn(rx, BTN_W, "@->", "redo");
    rx -= BTN_W;                                        // ←
    btn_undo_ = make_btn(rx, BTN_W, "@<-", "undo");

    // ---- フォーマット選択ドロップダウン (右端) ----
    fmt_choice_ = new Fl_Choice(w - CHOICE_W, 0, CHOICE_W, MENU_H);
    fmt_choice_->color(C_MENU_BG);
    fmt_choice_->labelcolor(C_MENU_FG);
    fmt_choice_->textcolor(C_MENU_FG);
    fmt_choice_->box(FL_FLAT_BOX);
    for (int i = 0; i < FMT_COUNT; i++) {
        const char *l = FMT_DEFS[i].label;
        int sc = 0;
        if (strcmp(l, "Auto") == 0) sc = FL_F + 8;
        else if (strcmp(l, "Dec") == 0) sc = FL_F + 9;
        else if (strcmp(l, "Hex") == 0) sc = FL_F + 10;
        else if (strcmp(l, "Bin") == 0) sc = FL_F + 11;
        else if (strcmp(l, "SI")  == 0) sc = FL_F + 12;
        fmt_choice_->add(l, sc, nullptr);
    }
    fmt_choice_->value(0);  // Auto
    fmt_choice_->callback(choice_cb, this);
    fmt_choice_->visible_focus(0);  // Tab キー順から除外

    // シートビュー (メニューバー直下、ウィンドウ全体)
    int sheet_y = MENU_H + PAD;
    int sheet_h = h - MENU_H - PAD * 2;
    sheet_ = new SheetView(PAD, sheet_y, w - PAD * 2, sheet_h);

    // フォーカス行変更時に Fl_Choice を更新
    sheet_->set_row_change_cb(row_change_cb, this);

    // 補完ポップアップ: 最後の子として追加することで全ての上に描画される
    // Fl_Group ベースなので OS ウィンドウを作らず、フォーカス問題がない
    popup_ = new CompletionPopup();

    end();
    resizable(sheet_);

    // SheetView にポインタを渡す (end() 後なので自動追加されない)
    sheet_->popup_ = popup_;

    // 初期状態のグレーアウトを反映
    update_toolbar();

    // WM close を横取りしてトレイ最小化に対応
    callback(close_cb, this);

    // setup_tray() はウィンドウが shown() になってから実行する
    // (fl_xid が有効になるタイミング = handle(FL_SHOW) 初回)
}

void MainWindow::resize(int nx, int ny, int nw, int nh) {
    Fl_Double_Window::resize(nx, ny, nw, nh);
    int mw = calc_menu_w(nw);
    menu_->resize(0, 0, mw, MENU_H);
    int rx = nw - CHOICE_W;
    fmt_choice_->resize(rx, 0, CHOICE_W, MENU_H);
    rx -= PAD + ABOUT_W;
    btn_about_->resize(rx, 0, ABOUT_W, MENU_H);
    rx -= PIN_W;
    btn_topmost_->resize(rx, 0, PIN_W, MENU_H);
    rx -= BTN_W;
    btn_redo_->resize(rx, 0, BTN_W, MENU_H);
    rx -= BTN_W;
    btn_undo_->resize(rx, 0, BTN_W, MENU_H);
}

void MainWindow::apply_ui_colors() {
    colors_apply_fl_scheme();
    color(C_WIN_BG);
    menu_->color(C_MENU_BG);
    menu_->textcolor(C_MENU_FG);
    btn_undo_->color(C_MENU_BG);
    btn_redo_->color(C_MENU_BG);
    btn_topmost_->color(C_MENU_BG);
    btn_topmost_->labelcolor(topmost_ ? C_MENU_FG : fl_inactive(C_MENU_FG));
    btn_about_->color(C_MENU_BG);
    btn_about_->labelcolor(C_MENU_FG);
    fmt_choice_->color(C_MENU_BG);
    fmt_choice_->textcolor(C_MENU_FG);
    update_toolbar();
    redraw();
}

void MainWindow::update_toolbar() {
    bool u = sheet_->can_undo() || sheet_->has_uncommitted_edit();
    bool r = sheet_->can_redo();

    // ← → ボタン: deactivate() は白っぽくなるので labelcolor だけ変更
    // (undo/redo は内部でガードしているので、無効時にクリックしても安全)
    Fl_Color C_DIM = g_colors.ui_dim;
    btn_undo_->labelcolor(u ? C_MENU_FG : C_DIM);
    btn_redo_->labelcolor(r ? C_MENU_FG : C_DIM);
    btn_undo_->redraw();
    btn_redo_->redraw();

    // Edit メニュー項目のグレーアウト (色も変更して視認性を確保)
    Fl_Menu_Item *items = (Fl_Menu_Item *)menu_->menu();
    auto set_menu = [&](int idx, bool active) {
        if (idx < 0) return;
        if (active) { items[idx].activate();   items[idx].labelcolor(C_MENU_FG); }
        else        { items[idx].deactivate(); items[idx].labelcolor(C_DIM); }
    };
    set_menu(mi_undo_, u);
    set_menu(mi_redo_, r);
}

int MainWindow::handle(int event) {
    // ウィンドウ表示後に初回のみトレイ/ホットキーを初期化
    if (event == FL_SHOW && !tray_initialized_) {
        tray_initialized_ = true;
        // 次の event loop iteration で実行 (fl_xid が確実に有効になるタイミング)
        Fl::add_timeout(0.0, [](void *d) {
            static_cast<MainWindow *>(d)->setup_tray();
        }, this);
    }

    // ポップアップ表示中にポップアップ外をクリックしたら閉じる
    if (event == FL_PUSH && popup_->is_shown()) {
        int ex = Fl::event_x(), ey = Fl::event_y();
        if (ex < popup_->x() || ex >= popup_->x() + popup_->w() ||
            ey < popup_->y() || ey >= popup_->y() + popup_->h()) {
            sheet_->completion_hide();
        }
    }
#if defined(_WIN32) && !defined(NDEBUG)
    if (event == FL_PUSH && shown()) {
        HWND hwnd = fl_win32_xid(this);
        if (hwnd) {
            POINT pt = {0, 0};
            ClientToScreen(hwnd, &pt);
            float s = Fl::screen_scale(screen_num());
            static FILE *dbgf = nullptr;
            if (!dbgf) {
                char p[MAX_PATH];
                GetModuleFileNameA(NULL, p, MAX_PATH);
                std::string lp(p);
                auto slash = lp.rfind('\\');
                if (slash != std::string::npos) lp = lp.substr(0, slash);
                lp += "\\menu-debug.log";
                dbgf = fopen(lp.c_str(), "a");
            }
            if (dbgf) {
                fprintf(dbgf, "[menu-dbg] FL(%d,%d,%d,%d) C2S(%ld,%ld) scr=%d s=%g mouse=(%d,%d)\n",
                        x(), y(), w(), h(),
                        pt.x, pt.y, screen_num(), s,
                        Fl::event_x_root(), Fl::event_y_root());
                fflush(dbgf);
            }
        }
    }
#endif
    return Fl_Double_Window::handle(event);
}

void MainWindow::open_file(const char *path) {
    if (!sheet_->load_file(path))
        fl_alert("Cannot open file:\n%s", path);
}

void MainWindow::update_fmt_choice() {
    const char *fn = sheet_->current_fmt_name();
    for (int i = 0; i < FMT_COUNT; i++) {
        if (fn == nullptr && FMT_DEFS[i].func_name == nullptr) { fmt_choice_->value(i); return; }
        if (fn && FMT_DEFS[i].func_name && strcmp(fn, FMT_DEFS[i].func_name) == 0) { fmt_choice_->value(i); return; }
    }
    fmt_choice_->value(0);  // 不明な場合は Auto
}

void MainWindow::row_change_cb(void *data) {
    auto *win = static_cast<MainWindow *>(data);
    win->update_fmt_choice();
    win->update_toolbar();
}

// ---- About ダイアログ (Fl_Help_View でリンクをクリック可能にする) ----

static const char *about_link_cb(Fl_Widget *, const char *uri) {
    if (uri && *uri) fl_open_uri(uri);
    return nullptr;  // nullptr を返すと Help_View 内遷移をしない
}

static void show_about(MainWindow *win) {
    (void)win;
    // アイコン (static で1回だけロード)
    static Fl_SVG_Image *about_icon = nullptr;
    if (!about_icon) {
        std::string svg_path = find_icon_svg();
        if (!svg_path.empty()) {
            about_icon = new Fl_SVG_Image(svg_path.c_str());
            if (about_icon->fail()) { delete about_icon; about_icon = nullptr; }
            else about_icon->resize(64, 64);
        }
    }

    const int DW = 420, DH = 380;
    Fl_Double_Window dlg(DW, DH, "About calcyx");
    dlg.set_modal();
    dlg.color(g_colors.ui_bg);

    // アイコン
    Fl_Box icon_box(DW / 2 - 32, 10, 64, 64);
    icon_box.box(FL_NO_BOX);
    if (about_icon) icon_box.image(about_icon);

    // HTML コンテンツ
    Fl_Help_View hv(10, 80, DW - 20, DH - 120);
    hv.color(g_colors.ui_bg);
    hv.textcolor(g_colors.ui_text);
    hv.textfont(FL_HELVETICA);
    hv.textsize(12);
    hv.link(about_link_cb);
    hv.scrollbar_size(0);  // スクロールバーを隠す

    std::string html =
        "<center>"
        "<b>calcyx " CALCYX_VERSION_FULL "</b><br>"
        CALCYX_EDITION
        "<p>A programmable calculator based on Calctus.</p>"
        "<a href='https://github.com/ponzu840w/calcyx'>https://github.com/ponzu840w/calcyx</a>"
        "</center>"
        "<font size=2>"
        "<p><b>calcyx</b> \xe2\x80\x94 Copyright \xc2\xa9 2026 ponzu840w \xe2\x80\x94 MIT License</p>"
        "<p><b>Calctus</b> \xe2\x80\x94 Copyright \xc2\xa9 2022 shapoco \xe2\x80\x94 MIT License<br>"
        "<a href='https://github.com/shapoco/calctus'>https://github.com/shapoco/calctus</a></p>"
        "<p><b>FLTK</b> \xe2\x80\x94 Copyright \xc2\xa9 1998-2024 Bill Spitzak and others \xe2\x80\x94 LGPL<br>"
        "<a href='https://www.fltk.org'>https://www.fltk.org</a></p>"
        "<p><b>mpdecimal</b> \xe2\x80\x94 Copyright \xc2\xa9 2008-2024 Stefan Krah \xe2\x80\x94 BSD 2-Clause<br>"
        "<a href='https://www.bytereef.org/mpdecimal'>https://www.bytereef.org/mpdecimal</a></p>"
        "</font>";
    hv.value(html.c_str());

    // OK ボタン
    Fl_Button ok_btn(DW / 2 - 40, DH - 35, 80, 25, "OK");
    ok_btn.callback([](Fl_Widget *w, void *) { w->window()->hide(); });

    dlg.end();
    dlg.show();
    while (dlg.shown()) Fl::wait();
}

void MainWindow::menu_cb(Fl_Widget *w, void *data) {
    (void)w;
    const char *cmd = static_cast<const char *>(data);
    if (!cmd) return;
    MainWindow *win = nullptr;
    for (Fl_Window *fw = Fl::first_window(); fw; fw = Fl::next_window(fw))
        if ((win = dynamic_cast<MainWindow *>(fw))) break;
    if (!win) return;

    if (strcmp(cmd, "open") == 0) {
        Fl_Native_File_Chooser fc;
        fc.title("Open");
        fc.type(Fl_Native_File_Chooser::BROWSE_FILE);
        fc.filter("Text files\t*.txt\nAll files\t*");
        if (fc.show() == 0) win->open_file(fc.filename());

    } else if (strcmp(cmd, "save") == 0) {
        Fl_Native_File_Chooser fc;
        fc.title("Save As");
        fc.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
        fc.filter("Text files\t*.txt\nAll files\t*");
        fc.options(Fl_Native_File_Chooser::SAVEAS_CONFIRM);
        if (fc.show() == 0) {
            if (!win->sheet_->save_file(fc.filename()))
                fl_alert("Cannot save file:\n%s", fc.filename());
        }

    } else if (strcmp(cmd, "copy_all") == 0) {
        win->sheet_->copy_all_to_clipboard();
    } else if (strcmp(cmd, "recalc") == 0) {
        win->sheet_->live_eval();
        win->sheet_->redraw();
    } else if (strcmp(cmd, "insert_below") == 0) {
        win->sheet_->insert_row_below();
    } else if (strcmp(cmd, "insert_above") == 0) {
        win->sheet_->insert_row_above();
    } else if (strcmp(cmd, "delete_row") == 0) {
        win->sheet_->delete_current_row();
    } else if (strcmp(cmd, "move_up") == 0) {
        win->sheet_->move_row_up();
    } else if (strcmp(cmd, "move_down") == 0) {
        win->sheet_->move_row_down();
    } else if (strcmp(cmd, "clear_all") == 0) {
        win->sheet_->clear_all();
    } else if (strcmp(cmd, "undo") == 0) {
        win->sheet_->undo();
    } else if (strcmp(cmd, "redo") == 0) {
        win->sheet_->redo();
    } else if (strcmp(cmd, "exit") == 0) {
        win->teardown_tray();
        win->save_prefs();
        exit(0);
    } else if (strcmp(cmd, "prefs") == 0) {
        PrefsDialog::run(win->sheet_, [](void *d) {
            auto *mw = static_cast<MainWindow *>(d);
            mw->apply_ui_colors();
            mw->apply_tray_settings();
        }, win);
    } else if (strcmp(cmd, "topmost") == 0) {
        win->toggle_always_on_top();
    } else if (strcmp(cmd, "about") == 0) {
        show_about(win);
    } else {
        open_sample_file(win, cmd);
    }
}

// icon.svg のパスを返す。見つからなければ空文字列。
static std::string find_icon_svg() {
    struct stat st;
#ifdef __APPLE__
    {
        char buf[1024];
        uint32_t size = sizeof(buf);
        if (_NSGetExecutablePath(buf, &size) == 0) {
            char *dir = dirname(buf);
            char candidate[1024];
            snprintf(candidate, sizeof(candidate), "%s/../Resources/icon.svg", dir);
            if (stat(candidate, &st) == 0 && S_ISREG(st.st_mode)) return candidate;
        }
    }
#endif
    static const char *bases[] = {
        "ui/icon.svg", "../ui/icon.svg", "../../ui/icon.svg",
        "icon.svg", "../icon.svg",
        "/usr/share/icons/hicolor/scalable/apps/calcyx.svg",
    };
    for (auto *base : bases) {
        if (stat(base, &st) == 0 && S_ISREG(st.st_mode)) return base;
    }
    return "";
}

// samples/ ディレクトリへの絶対/相対パスを返す。見つからなければ空文字列。
static std::string find_samples_dir() {
    struct stat st;
#ifdef __APPLE__
    {
        char buf[1024];
        uint32_t size = sizeof(buf);
        if (_NSGetExecutablePath(buf, &size) == 0) {
            char *dir = dirname(buf);
            char candidate[1024];
            snprintf(candidate, sizeof(candidate), "%s/../Resources/samples", dir);
            if (stat(candidate, &st) == 0 && S_ISDIR(st.st_mode)) return candidate;
        }
    }
#endif
    static const char *bases[] = { "samples", "../samples", "../../samples" };
    for (auto *base : bases) {
        if (stat(base, &st) == 0 && S_ISDIR(st.st_mode)) return base;
    }
    return "";
}

void MainWindow::populate_samples_menu() {
    std::string dir = find_samples_dir();
    if (dir.empty()) return;

    DIR *dp = opendir(dir.c_str());
    if (!dp) return;

    std::vector<std::string> files;
    struct dirent *ent;
    while ((ent = readdir(dp)) != nullptr) {
        std::string name = ent->d_name;
        if (name.size() > 4 && name.substr(name.size() - 4) == ".txt")
            files.push_back(name);
    }
    closedir(dp);

    std::sort(files.begin(), files.end());
    sample_files_ = std::move(files);

    for (const auto &f : sample_files_) {
        std::string label = "&File/&Samples/" + f.substr(0, f.size() - 4);
        menu_->add(label.c_str(), 0, menu_cb, (void *)f.c_str());
    }

    // Samples サブメニューの後に区切り線を入れる
    Fl_Menu_Item *it = (Fl_Menu_Item *)menu_->find_item("&File/&Samples");
    if (it) it->flags |= FL_MENU_DIVIDER;
}

bool MainWindow::open_sample_file(MainWindow *win, const char *filename) {
    std::string dir = find_samples_dir();
    if (!dir.empty()) {
        std::string path = dir + "/" + filename;
        if (win->sheet_->load_file(path.c_str())) return true;
    }
    fl_alert("File not found:\n%s", filename);
    return false;
}

void MainWindow::toggle_always_on_top() {
    topmost_ = !topmost_;
#if defined(_WIN32)
    HWND hwnd = fl_xid(this);
    SetWindowPos(hwnd, topmost_ ? HWND_TOPMOST : HWND_NOTOPMOST,
                 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
#elif defined(__APPLE__)
    mac_set_window_level(this, topmost_ ? 1 : 0);
#else
    Display *dpy = fl_display;
    Window xwin = fl_xid(this);
    Atom wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
    Atom wm_above = XInternAtom(dpy, "_NET_WM_STATE_ABOVE", False);
    XEvent ev = {};
    ev.xclient.type = ClientMessage;
    ev.xclient.window = xwin;
    ev.xclient.message_type = wm_state;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = topmost_ ? 1 : 0;  // _NET_WM_STATE_ADD / REMOVE
    ev.xclient.data.l[1] = (long)wm_above;
    ev.xclient.data.l[2] = 0;
    XSendEvent(dpy, DefaultRootWindow(dpy), False,
               SubstructureNotifyMask | SubstructureRedirectMask, &ev);
    XFlush(dpy);
#endif
    // ボタンの見た目を更新 (ON: 通常色, OFF: 薄色)
    btn_topmost_->labelcolor(topmost_ ? C_MENU_FG : fl_inactive(C_MENU_FG));
    btn_topmost_->redraw();
}

void MainWindow::save_prefs() {
    if (!g_remember_position) return;
    AppPrefs prefs;
    prefs.set_int("geometry_valid", 1);
    prefs.set_int("x", x());
    prefs.set_int("y", y());
    prefs.set_int("w", w());
    prefs.set_int("h", h());
}

void MainWindow::hide() {
    save_prefs();
    Fl_Double_Window::hide();
}

// ---- WM close 横取り ----
void MainWindow::close_cb(Fl_Widget *w, void *data) {
    auto *win = static_cast<MainWindow *>(data);
    if (win->tray_active_) {
        // トレイに隠す (ウィンドウを閉じない)
        plat_window_toggle(win, true);
        return;
    }
    win->hide();
}

// ---- トレイ/ホットキー管理 ----
void MainWindow::setup_tray() {
    if (g_tray_icon) {
        TrayCallbacks cb;
        cb.on_open   = [this]() {
            // 常に表示+前面に (Calctus 準拠)。トグルはホットキーの役割。
            Fl::add_timeout(0.0, [](void *d) {
                static_cast<MainWindow *>(d)->show_and_activate();
            }, this);
        };
        cb.on_exit   = [this]() {
            // Windows: TrackPopupMenu → RemoveWindowSubclass → exit() の再入を避ける
            Fl::add_timeout(0.0, [](void *d) {
                auto *win = static_cast<MainWindow *>(d);
                win->teardown_tray();
                win->save_prefs();
                exit(0);
            }, this);
            Fl::awake();  // ウィンドウ非表示でもタイムアウトを確実に発火させる
        };
        cb.on_hotkey = [this]() {
            Fl::add_timeout(0.0, [](void *d) {
                static_cast<MainWindow *>(d)->toggle_visibility();
            }, this);
        };
        tray_active_ = plat_tray_create(this, cb);
    }

    if (g_hotkey_enabled) {
        int mods = 0;
        if (g_hotkey_alt)   mods |= PMOD_ALT;
        if (g_hotkey_ctrl)  mods |= PMOD_CTRL;
        if (g_hotkey_shift) mods |= PMOD_SHIFT;
        if (g_hotkey_win)   mods |= PMOD_WIN;
        plat_hotkey_register(mods, g_hotkey_keycode);
    }

    // Linux: ホットキーポーリング開始
    if (g_hotkey_enabled)
        Fl::add_timeout(0.05, hotkey_poll_cb, this);
}

void MainWindow::teardown_tray() {
    Fl::remove_timeout(hotkey_poll_cb, this);
    plat_hotkey_unregister();
    if (tray_active_) {
        plat_tray_destroy();
        tray_active_ = false;
    }
}

void MainWindow::apply_tray_settings() {
    bool was_tray = tray_active_;
    teardown_tray();
    setup_tray();

    // トレイが無効化された場合、ウィンドウが非表示ならば表示する
    // (非表示 + トレイなし = ゾンビ状態の防止)
    if (was_tray && !tray_active_ && !visible()) {
        show();
    }
}

void MainWindow::toggle_visibility() {
    plat_window_toggle(this, tray_active_);
}

void MainWindow::show_and_activate() {
    // 非表示なら表示、表示済みなら前面に持ってくる (常にオープン方向)
    if (!visible()) {
        plat_window_toggle(this, tray_active_);  // 非表示→表示
    } else {
        // 既に表示中: 前面に持ってくるだけ (plat_window_toggle は
        // フォアグラウンド時に隠してしまうので、show だけ呼ぶ)
        show();
    }
}

void MainWindow::hotkey_poll_cb(void *data) {
    plat_hotkey_poll();
    Fl::repeat_timeout(0.05, hotkey_poll_cb, data);
}

bool MainWindow::should_keep_running() {
    if (!tray_active_) return false;

    // トレイマネージャーが死んだ場合 (Explorer 再起動等) のゾンビ防止:
    // プラットフォーム側でトレイが消えていたらウィンドウを復帰して終了
    if (!plat_tray_is_active()) {
        tray_active_ = false;
        show();
        return false;
    }
    return true;
}

void MainWindow::choice_cb(Fl_Widget *w, void *data) {
    auto *win = static_cast<MainWindow *>(data);
    int idx = static_cast<Fl_Choice *>(w)->value();
    if (idx < 0 || idx >= FMT_COUNT) return;
    win->sheet_->apply_fmt(FMT_DEFS[idx].func_name);
    // apply_fmt → commit → row_change_cb で choice も更新される
}
