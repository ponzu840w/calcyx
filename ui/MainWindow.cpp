// 移植元: Calctus/UI/MainForm.cs (簡略版)

#include "MainWindow.h"
#include <FL/Fl.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/fl_ask.H>
#include <cstdio>
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
    menu_->add("&Edit/&Undo\t",       FL_COMMAND + 'z', menu_cb, (void*)"undo");
    menu_->add("&Edit/&Redo\t",       FL_COMMAND + 'y', menu_cb, (void*)"redo");
    menu_->add("&File/&Examples/Examples",            0, menu_cb, (void*)"Examples.txt");
    menu_->add("&File/&Examples/Test_Abs_Sign",       0, menu_cb, (void*)"Test_Abs_Sign.txt");
    menu_->add("&File/&Examples/Test_Array",          0, menu_cb, (void*)"Test_Array.txt");
    menu_->add("&File/&Examples/Test_Assign",         0, menu_cb, (void*)"Test_Assign.txt");
    menu_->add("&File/&Examples/Test_BitByteOps",     0, menu_cb, (void*)"Test_BitByteOps.txt");
    menu_->add("&File/&Examples/Test_Cast",           0, menu_cb, (void*)"Test_Cast.txt");
    menu_->add("&File/&Examples/Test_Color",          0, menu_cb, (void*)"Test_Color.txt");
    menu_->add("&File/&Examples/Test_DateTime",       0, menu_cb, (void*)"Test_DateTime.txt");
    menu_->add("&File/&Examples/Test_ECC",            0, menu_cb, (void*)"Test_ECC.txt");
    menu_->add("&File/&Examples/Test_Encoding",       0, menu_cb, (void*)"Test_Encoding.txt");
    menu_->add("&File/&Examples/Test_Exponential",    0, menu_cb, (void*)"Test_Exponential.txt");
    menu_->add("&File/&Examples/Test_Functions",      0, menu_cb, (void*)"Test_Functions.txt");
    menu_->add("&File/&Examples/Test_GCD_LCM",        0, menu_cb, (void*)"Test_GCD_LCM.txt");
    menu_->add("&File/&Examples/Test_GrayCode",       0, menu_cb, (void*)"Test_GrayCode.txt");
    menu_->add("&File/&Examples/Test_MinMax",         0, menu_cb, (void*)"Test_MinMax.txt");
    menu_->add("&File/&Examples/Test_PrimeNumber",    0, menu_cb, (void*)"Test_PrimeNumber.txt");
    menu_->add("&File/&Examples/Test_Representation", 0, menu_cb, (void*)"Test_Representation.txt");
    menu_->add("&File/&Examples/Test_Rounding",       0, menu_cb, (void*)"Test_Rounding.txt");
    menu_->add("&File/&Examples/Test_Solve",          0, menu_cb, (void*)"Test_Solve.txt");
    menu_->add("&File/&Examples/Test_String",         0, menu_cb, (void*)"Test_String.txt");
    menu_->add("&File/&Examples/Test_Sum_Average",    0, menu_cb, (void*)"Test_Sum_Average.txt");
    menu_->add("&File/&Examples/Test_Trigonometric",  0, menu_cb, (void*)"Test_Trigonometric.txt");
    menu_->add("&File/&Examples/Test_eSeries",        0, menu_cb, (void*)"Test_eSeries.txt");

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

    } else if (strcmp(cmd, "undo") == 0) {
        win->sheet_->undo();
    } else if (strcmp(cmd, "redo") == 0) {
        win->sheet_->redo();
    } else {
        open_sample_file(win, cmd);
    }
}

bool MainWindow::open_sample_file(MainWindow *win, const char *filename) {
    static const char *bases[] = {
        "samples/",
        "../samples/",
        "../../samples/",
    };
    char path[512];
    for (auto *base : bases) {
        snprintf(path, sizeof(path), "%s%s", base, filename);
        if (win->sheet_->load_file(path)) return true;
    }
    fl_alert("File not found:\n%s", filename);
    return false;
}

void MainWindow::choice_cb(Fl_Widget *w, void *data) {
    auto *win = static_cast<MainWindow *>(data);
    int idx = static_cast<Fl_Choice *>(w)->value();
    if (idx < 0 || idx >= FMT_COUNT) return;
    win->sheet_->apply_fmt(FMT_DEFS[idx].func_name);
    // apply_fmt → commit → row_change_cb で choice も更新される
}
