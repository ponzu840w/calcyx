// 移植元: Calctus/UI/MainForm.cs (簡略版)

#include "MainWindow.h"
#include <FL/Fl.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/fl_ask.H>
#include <cstring>

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

static const Fl_Color C_WIN_BG  = fl_rgb_color( 30,  30,  30);
static const Fl_Color C_MENU_BG = fl_rgb_color( 40,  40,  45);
static const Fl_Color C_MENU_FG = fl_rgb_color(210, 210, 220);

MainWindow::MainWindow(int w, int h, const char *title)
    : Fl_Double_Window(w, h, title)
{
    color(C_WIN_BG);

    // メニューバー (右側に Fl_Choice の分だけ幅を残す)
    menu_ = new Fl_Menu_Bar(0, 0, w - CHOICE_W - PAD, MENU_H);
    menu_->color(C_MENU_BG);
    menu_->textcolor(C_MENU_FG);
    menu_->box(FL_FLAT_BOX);
    menu_->add("&File/&Open...\t",    FL_COMMAND + 'o', menu_cb, (void*)"open");
    menu_->add("&File/&Save As...\t", FL_COMMAND + 's', menu_cb, (void*)"save");
    menu_->add("&File/&Examples",     0,                menu_cb, (void*)"examples");

    // フォーマット選択ドロップダウン (メニューバー右端)
    fmt_choice_ = new Fl_Choice(w - CHOICE_W, 0, CHOICE_W, MENU_H);
    fmt_choice_->color(C_MENU_BG);
    fmt_choice_->labelcolor(C_MENU_FG);
    fmt_choice_->textcolor(C_MENU_FG);
    fmt_choice_->box(FL_FLAT_BOX);
    for (int i = 0; i < FMT_COUNT; i++)
        fmt_choice_->add(FMT_DEFS[i].label);
    fmt_choice_->value(0);  // Auto
    fmt_choice_->callback(choice_cb, this);

    // シートビュー (メニューバー直下、ウィンドウ全体)
    int sheet_y = MENU_H + PAD;
    int sheet_h = h - MENU_H - PAD * 2;
    sheet_ = new SheetView(PAD, sheet_y, w - PAD * 2, sheet_h);

    // フォーカス行変更時に Fl_Choice を更新
    sheet_->set_row_change_cb(row_change_cb, this);

    end();
    resizable(sheet_);
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
    static_cast<MainWindow *>(data)->update_fmt_choice();
}

void MainWindow::menu_cb(Fl_Widget *w, void *data) {
    (void)w;
    const char *cmd = static_cast<const char *>(data);
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

    } else if (strcmp(cmd, "examples") == 0) {
        static const char *candidates[] = {
            "tmp/calctus-linux/Samples/Examples.txt",
            "../tmp/calctus-linux/Samples/Examples.txt",
            "../../tmp/calctus-linux/Samples/Examples.txt",
        };
        for (auto *p : candidates)
            if (win->sheet_->load_file(p)) return;
        fl_alert("Examples.txt not found.\nUse File > Open to select it manually.");
    }
}

void MainWindow::choice_cb(Fl_Widget *w, void *data) {
    auto *win = static_cast<MainWindow *>(data);
    int idx = static_cast<Fl_Choice *>(w)->value();
    if (idx < 0 || idx >= FMT_COUNT) return;
    win->sheet_->apply_fmt(FMT_DEFS[idx].func_name);
    // apply_fmt → commit → row_change_cb で choice も更新される
}
