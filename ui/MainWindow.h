// calcyx メインウィンドウ
// 移植元: Calctus/UI/MainForm.cs (簡略版)

#pragma once

#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Menu_Bar.H>
#include "SheetView.h"

class MainWindow : public Fl_Double_Window {
public:
    MainWindow(int w, int h, const char *title);
    void open_file(const char *path);

    // フォーカス行変更時に Fl_Choice の表示を更新
    void update_fmt_choice();

private:
    Fl_Menu_Bar *menu_;
    Fl_Choice   *fmt_choice_;
    SheetView   *sheet_;

    static const int MENU_H   = 24;
    static const int CHOICE_W = 110;
    static const int PAD      = 4;

    static void menu_cb  (Fl_Widget *w, void *data);
    static void choice_cb(Fl_Widget *w, void *data);
    static void row_change_cb(void *data);  // SheetView からのコールバック
};
