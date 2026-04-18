// 移植元: Calctus/UI/MainForm.cs (簡略版)

#include "MainWindow.h"
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
#endif
#ifdef _WIN32
#include <FL/platform.H>
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

static const Fl_Color C_WIN_BG  = fl_rgb_color( 30,  30,  30);
static const Fl_Color C_MENU_BG = fl_rgb_color( 40,  40,  45);
static const Fl_Color C_MENU_FG = fl_rgb_color(210, 210, 220);

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
    populate_samples_menu();
    menu_->add("&File/E&xit",         0,                menu_cb, (void*)"exit");

    // 全メニュー追加後にインデックスを取得
    // find_index(path) はショートカット付きラベルで失敗するので手動検索
    mi_undo_ = mi_redo_ = -1;
    for (int i = 0; i < menu_->size(); i++) {
        const Fl_Menu_Item &it = menu_->menu()[i];
        if (!it.label()) continue;
        if (it.callback() == menu_cb) {
            const char *d = (const char *)it.user_data();
            if (d && strcmp(d, "undo") == 0) mi_undo_ = i;
            if (d && strcmp(d, "redo") == 0) mi_redo_ = i;
        }
    }

    // ---- ← → ツールバーボタン (右寄せ: ? の左隣) ----
    auto make_btn = [&](int bx, const char *label, const char *cmd) {
        auto *b = new Fl_Button(bx, 0, BTN_W, MENU_H, label);
        b->box(FL_FLAT_BOX);
        b->color(C_MENU_BG);
        b->labelcolor(C_MENU_FG);
        b->labelsize(14);
        b->callback(menu_cb, (void*)cmd);
        b->visible_focus(0);
        return b;
    };
    btn_undo_  = make_btn(w - CHOICE_W - PAD - ABOUT_W - PAD - BTN_W * 2, "@<-", "undo");
    btn_redo_  = make_btn(w - CHOICE_W - PAD - ABOUT_W - PAD - BTN_W,     "@->", "redo");

    // ---- ? About ボタン (右寄せ、フォーマット選択の左) ----
    btn_about_ = new Fl_Button(w - CHOICE_W - PAD - ABOUT_W, 0, ABOUT_W, MENU_H, "?");
    btn_about_->box(FL_FLAT_BOX);
    btn_about_->color(C_MENU_BG);
    btn_about_->labelcolor(C_MENU_FG);
    btn_about_->labelsize(13);
    btn_about_->callback(menu_cb, (void*)"about");
    btn_about_->visible_focus(0);

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
}

void MainWindow::resize(int nx, int ny, int nw, int nh) {
    Fl_Double_Window::resize(nx, ny, nw, nh);
    int mw = calc_menu_w(nw);
    menu_->resize(0, 0, mw, MENU_H);
    btn_undo_->resize(nw - CHOICE_W - PAD - ABOUT_W - PAD - BTN_W * 2, 0, BTN_W, MENU_H);
    btn_redo_->resize(nw - CHOICE_W - PAD - ABOUT_W - PAD - BTN_W,     0, BTN_W, MENU_H);
    btn_about_->resize(nw - CHOICE_W - PAD - ABOUT_W, 0, ABOUT_W, MENU_H);
    fmt_choice_->resize(nw - CHOICE_W, 0, CHOICE_W, MENU_H);
}

void MainWindow::update_toolbar() {
    bool u = sheet_->can_undo() || sheet_->has_uncommitted_edit();
    bool r = sheet_->can_redo();

    // ← → ボタン: deactivate() は白っぽくなるので labelcolor だけ変更
    // (undo/redo は内部でガードしているので、無効時にクリックしても安全)
    static const Fl_Color C_DIM = fl_rgb_color(60, 60, 65);
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
    dlg.color(fl_rgb_color(30, 30, 30));

    // アイコン
    Fl_Box icon_box(DW / 2 - 32, 10, 64, 64);
    icon_box.box(FL_NO_BOX);
    if (about_icon) icon_box.image(about_icon);

    // HTML コンテンツ
    Fl_Help_View hv(10, 80, DW - 20, DH - 120);
    hv.color(fl_rgb_color(30, 30, 30));
    hv.textcolor(fl_rgb_color(210, 210, 220));
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
        win->save_prefs();
        exit(0);
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

void MainWindow::save_prefs() {
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

void MainWindow::choice_cb(Fl_Widget *w, void *data) {
    auto *win = static_cast<MainWindow *>(data);
    int idx = static_cast<Fl_Choice *>(w)->value();
    if (idx < 0 || idx >= FMT_COUNT) return;
    win->sheet_->apply_fmt(FMT_DEFS[idx].func_name);
    // apply_fmt → commit → row_change_cb で choice も更新される
}
