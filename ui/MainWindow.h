// calcyx メインウィンドウ
// 移植元: Calctus/UI/MainForm.cs (簡略版)

#pragma once

#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Button.H>
#include "SheetView.h"
#include "CompletionPopup.h"
#include <string>
#include <vector>

class MainWindow : public Fl_Double_Window {
public:
    MainWindow(int w, int h, const char *title);
    void open_file(const char *path);

    // フォーカス行変更時に Fl_Choice とツールバーボタンを更新
    void update_fmt_choice();
    void update_toolbar();

    int  handle(int event) override;
    void resize(int x, int y, int w, int h) override;
    void hide() override;

private:
    void save_prefs();
    Fl_Menu_Bar     *menu_;
    Fl_Button       *btn_undo_;   // ← ツールバー Undo
    Fl_Button       *btn_redo_;   // → ツールバー Redo
    Fl_Button       *btn_about_;  // ? About ボタン (右寄せ)
    Fl_Choice       *fmt_choice_;
    SheetView       *sheet_;
    CompletionPopup *popup_;

    static const int MENU_H   = 24;
    static const int CHOICE_W = 110;
    static const int BTN_W    = 22;   // ← → ボタン幅
    static const int ABOUT_W  = 22;   // ? ボタン幅
    static const int PAD      = 4;

    // メニューバー幅 = ウィンドウ幅 − 右側ウィジェット群
    static int calc_menu_w(int win_w) {
        return win_w - BTN_W * 2 - PAD - ABOUT_W - PAD - CHOICE_W;
    }

    static void menu_cb  (Fl_Widget *w, void *data);
    static void choice_cb(Fl_Widget *w, void *data);
    static void row_change_cb(void *data);  // SheetView からのコールバック
    void populate_samples_menu();
    static bool open_sample_file(MainWindow *win, const char *filename);

    std::vector<std::string> sample_files_;  // メニュー文字列の安定した記憶域
};
