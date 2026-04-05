// calcyx メインウィンドウ
// 移植元: Calctus/UI/MainForm.cs (簡略版)

#pragma once

#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Menu_Bar.H>
#include "SheetView.h"

class MainWindow : public Fl_Double_Window {
public:
    MainWindow(int w, int h, const char *title);
    // コマンドライン等からファイルを開く
    void open_file(const char *path);

private:
    Fl_Menu_Bar *menu_;
    SheetView   *sheet_;
    Fl_Button   *fmt_btns_[8];   // Auto / Dec / Hex / Bin / Oct / SI / Kibi / Char

    static const int MENU_H    = 24;
    static const int FMT_BAR_H = 28;
    static const int PAD       = 4;

    static void menu_cb(Fl_Widget *w, void *data);
    static void fmt_cb (Fl_Widget *w, void *data);
};
