// 移植元: Calctus/UI/MainForm.cs (簡略版)

#include "MainWindow.h"
#include <FL/Fl.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/fl_ask.H>
#include <cstring>
#include <cstdio>

// フォーマットボタンの定義 (移植元: RepresentaionFuncs.cs)
// func_name: nullptr = Auto (ラッパー除去)
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

static const Fl_Color C_WIN_BG  = fl_rgb_color( 30,  30,  30);
static const Fl_Color C_BTN_BG  = fl_rgb_color( 55,  55,  65);
static const Fl_Color C_BTN_FG  = fl_rgb_color(210, 210, 220);
static const Fl_Color C_MENU_BG = fl_rgb_color( 40,  40,  45);
static const Fl_Color C_MENU_FG = fl_rgb_color(210, 210, 220);

MainWindow::MainWindow(int w, int h, const char *title)
    : Fl_Double_Window(w, h, title)
{
    color(C_WIN_BG);

    // メニューバー
    menu_ = new Fl_Menu_Bar(0, 0, w, MENU_H);
    menu_->color(C_MENU_BG);
    menu_->textcolor(C_MENU_FG);
    menu_->box(FL_FLAT_BOX);

    menu_->add("&File/&Open...\t", FL_COMMAND + 'o', menu_cb, (void*)"open");
    menu_->add("&File/&Save As...\t", FL_COMMAND + 's', menu_cb, (void*)"save");
    menu_->add("&File/&Examples",  0,                menu_cb, (void*)"examples");

    // シートビュー
    int sheet_y = MENU_H + PAD;
    int sheet_h = h - MENU_H - FMT_BAR_H - PAD * 3;
    sheet_ = new SheetView(PAD, sheet_y, w - PAD * 2, sheet_h);

    // フォーマットボタンバー（下部）
    int bar_y = h - FMT_BAR_H - PAD;
    int btn_w = (w - PAD * 2) / 8;
    for (int i = 0; i < 8; i++) {
        int bx = PAD + i * btn_w;
        int bw = (i == 7) ? (w - PAD - bx) : btn_w;
        fmt_btns_[i] = new Fl_Button(bx, bar_y, bw, FMT_BAR_H, FMT_DEFS[i].label);
        fmt_btns_[i]->color(C_BTN_BG);
        fmt_btns_[i]->labelcolor(C_BTN_FG);
        fmt_btns_[i]->box(FL_FLAT_BOX);
        fmt_btns_[i]->labelfont(FL_HELVETICA);
        fmt_btns_[i]->labelsize(12);
        // callback(cb, data) で data = インデックス。user_data() は callback() と同じ格納先を
        // 使うため、callback() の後に user_data() を呼ぶと上書きされる点に注意。
        fmt_btns_[i]->callback(fmt_cb, reinterpret_cast<void *>(static_cast<intptr_t>(i)));
    }

    end();
    resizable(sheet_);
}

void MainWindow::open_file(const char *path) {
    if (!sheet_->load_file(path)) {
        fl_alert("Cannot open file:\n%s", path);
    }
}

void MainWindow::menu_cb(Fl_Widget *w, void *data) {
    const char *cmd = static_cast<const char *>(data);
    // メニューが属する MainWindow を取得
    MainWindow *win = nullptr;
    for (Fl_Window *fw = Fl::first_window(); fw; fw = Fl::next_window(fw)) {
        if ((win = dynamic_cast<MainWindow *>(fw))) break;
    }
    if (!win) return;

    if (strcmp(cmd, "open") == 0) {
        Fl_Native_File_Chooser fc;
        fc.title("Open");
        fc.type(Fl_Native_File_Chooser::BROWSE_FILE);
        fc.filter("Text files\t*.txt\nAll files\t*");
        if (fc.show() == 0)
            win->open_file(fc.filename());

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

    } else if (strcmp(cmd, "examples") == 0) {
        // 移植元リポジトリの Examples.txt を開く
        // 実行ファイルの親ディレクトリから相対パスを試みる
        static const char *candidates[] = {
            "tmp/calctus-linux/Samples/Examples.txt",
            "../tmp/calctus-linux/Samples/Examples.txt",
            "../../tmp/calctus-linux/Samples/Examples.txt",
        };
        for (auto *p : candidates) {
            if (win->sheet_->load_file(p)) return;
        }
        fl_alert("Examples.txt not found.\nUse File > Open to select it manually.");
    }
}

void MainWindow::fmt_cb(Fl_Widget *w, void *data) {
    (void)w;
    intptr_t idx = reinterpret_cast<intptr_t>(data);
    if (idx < 0 || idx >= 8) return;
    // callback の data はインデックス。MainWindow は first_window から取得する。
    MainWindow *win = nullptr;
    for (Fl_Window *fw = Fl::first_window(); fw; fw = Fl::next_window(fw))
        if ((win = dynamic_cast<MainWindow *>(fw))) break;
    if (!win) return;
    win->sheet_->apply_fmt(FMT_DEFS[idx].func_name);
}
