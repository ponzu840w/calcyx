// calcyx メインウィンドウ
// 移植元の MainForm.cs + SheetView.cs の簡略版

#pragma once

#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Browser.H>
#include <FL/Fl_Box.H>
#include <string>
#include <vector>

extern "C" {
#include "eval/eval.h"
#include "eval/builtin.h"
}

// 1行分の履歴エントリ
struct HistoryEntry {
    std::string expr;    // 入力式
    std::string result;  // 評価結果
    bool        is_error;
};

class MainWindow : public Fl_Double_Window {
public:
    MainWindow(int w, int h, const char *title);
    ~MainWindow();

private:
    Fl_Browser *history_browser_;  // 式と結果の履歴リスト
    Fl_Input   *input_;            // 入力行

    eval_ctx_t  ctx_;
    std::vector<HistoryEntry> history_;
    int history_cursor_;           // 履歴ナビゲーション用

    void eval_current();
    void update_browser();
    void nav_history(int delta);

    static void input_cb (Fl_Widget *w, void *data);
    static void browser_cb(Fl_Widget *w, void *data);

    int handle(int event) override;
};
