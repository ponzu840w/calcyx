// 移植元: Calctus/UI/MainForm.cs (簡略版)

#include "MainWindow.h"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <cstring>

// フォーマットボタンの定義
static const struct { const char *label; val_fmt_t fmt; } FMT_DEFS[] = {
    { "Auto",  FMT_REAL       },
    { "Dec",   FMT_INT        },
    { "Hex",   FMT_HEX        },
    { "Bin",   FMT_BIN        },
    { "Oct",   FMT_OCT        },
    { "SI",    FMT_SI_PREFIX  },
    { "Kibi",  FMT_BIN_PREFIX },
    { "Char",  FMT_CHAR       },
};

static const Fl_Color C_WIN_BG   = fl_rgb_color( 30,  30,  30);
static const Fl_Color C_BTN_BG   = fl_rgb_color( 55,  55,  65);
static const Fl_Color C_BTN_FG   = fl_rgb_color(210, 210, 220);

MainWindow::MainWindow(int w, int h, const char *title)
    : Fl_Double_Window(w, h, title)
{
    color(C_WIN_BG);

    // シートビュー（フォーマットバーの上）
    int sheet_h = h - FMT_BAR_H - PAD * 2;
    sheet_ = new SheetView(PAD, PAD, w - PAD * 2, sheet_h);

    // フォーマットボタンバー（下部）
    int bar_y  = h - FMT_BAR_H - PAD;
    int btn_w  = (w - PAD * 2) / 8;
    for (int i = 0; i < 8; i++) {
        int bx = PAD + i * btn_w;
        // 最後のボタンは残り幅を吸収
        int bw = (i == 7) ? (w - PAD - bx) : btn_w;
        fmt_btns_[i] = new Fl_Button(bx, bar_y, bw, FMT_BAR_H, FMT_DEFS[i].label);
        fmt_btns_[i]->color(C_BTN_BG);
        fmt_btns_[i]->labelcolor(C_BTN_FG);
        fmt_btns_[i]->box(FL_FLAT_BOX);
        fmt_btns_[i]->labelfont(FL_HELVETICA);
        fmt_btns_[i]->labelsize(12);
        fmt_btns_[i]->user_data(reinterpret_cast<void *>(static_cast<intptr_t>(i)));
        fmt_btns_[i]->callback(fmt_cb, this);
    }

    end();
    resizable(sheet_);
}

void MainWindow::fmt_cb(Fl_Widget *w, void *data) {
    auto *win = static_cast<MainWindow *>(data);
    intptr_t idx = reinterpret_cast<intptr_t>(w->user_data());
    win->sheet_->apply_fmt(FMT_DEFS[idx].fmt);
}
