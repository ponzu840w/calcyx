// calcyx メインウィンドウ実装

#include "MainWindow.h"
#include <FL/Fl.H>
#include <FL/fl_ask.H>
#include <cstring>
#include <cstdio>

// --- レイアウト定数 ---
static const int PAD        = 6;
static const int INPUT_H    = 28;
static const int BROWSER_H_MIN = 200;

// --- カラー ---
static const Fl_Color COL_BG      = fl_rgb_color(30, 30, 30);
static const Fl_Color COL_INPUT_BG = fl_rgb_color(45, 45, 45);
static const Fl_Color COL_INPUT_FG = fl_rgb_color(220, 220, 220);
static const Fl_Color COL_EXPR_FG  = fl_rgb_color(180, 200, 255);
static const Fl_Color COL_RESULT_FG = fl_rgb_color(100, 220, 100);
static const Fl_Color COL_ERROR_FG  = fl_rgb_color(255, 100, 100);
static const Fl_Color COL_BROWSER_BG = fl_rgb_color(22, 22, 22);
static const Fl_Color COL_BROWSER_SEL = fl_rgb_color(50, 60, 80);

MainWindow::MainWindow(int w, int h, const char *title)
    : Fl_Double_Window(w, h, title)
    , history_cursor_(-1)
{
    color(COL_BG);

    // ブラウザ（履歴表示）
    int brow_h = h - INPUT_H - PAD * 3;
    history_browser_ = new Fl_Browser(PAD, PAD, w - PAD * 2, brow_h);
    history_browser_->type(FL_HOLD_BROWSER);
    history_browser_->color(COL_BROWSER_BG);
    history_browser_->selection_color(COL_BROWSER_SEL);
    history_browser_->textfont(FL_COURIER);
    history_browser_->textsize(14);
    history_browser_->callback(browser_cb, this);

    // 入力欄
    input_ = new Fl_Input(PAD, h - INPUT_H - PAD, w - PAD * 2, INPUT_H);
    input_->color(COL_INPUT_BG);
    input_->textcolor(COL_INPUT_FG);
    input_->textfont(FL_COURIER);
    input_->textsize(14);
    input_->cursor_color(COL_INPUT_FG);
    input_->box(FL_FLAT_BOX);
    input_->when(FL_WHEN_ENTER_KEY);
    input_->callback(input_cb, this);

    end();
    resizable(history_browser_);

    // エンジン初期化
    eval_ctx_init(&ctx_);
    builtin_register_all(&ctx_);

    input_->take_focus();
}

MainWindow::~MainWindow() {
    eval_ctx_free(&ctx_);
}

// --- 評価実行 ---
void MainWindow::eval_current() {
    const char *src = input_->value();
    if (!src || src[0] == '\0') return;

    // エラー状態をリセット
    ctx_.has_error = false;
    ctx_.error_msg[0] = '\0';

    char errmsg[256] = "";
    val_t *result = eval_str(src, &ctx_, errmsg, sizeof(errmsg));

    HistoryEntry entry;
    entry.expr = src;

    if (result) {
        char buf[512];
        val_to_str(result, buf, sizeof(buf));
        entry.result   = buf;
        entry.is_error = false;
        val_free(result);
        // エラーをリセット（eval_str 成功後は残りをクリア）
        ctx_.has_error  = false;
        ctx_.error_msg[0] = '\0';
    } else {
        entry.result   = errmsg[0] ? errmsg : "Error";
        entry.is_error = true;
        // コンテキストのエラー状態もクリアして次の評価に備える
        ctx_.has_error  = false;
        ctx_.error_msg[0] = '\0';
    }

    history_.push_back(entry);
    history_cursor_ = -1;

    update_browser();
    input_->value("");
}

// --- ブラウザ更新 ---
void MainWindow::update_browser() {
    history_browser_->clear();
    for (const auto &e : history_) {
        // 式行: インデント付き青系
        char expr_line[640];
        snprintf(expr_line, sizeof(expr_line),
                 "@C%d@F%d@S%d  %s",
                 (int)COL_EXPR_FG, (int)FL_COURIER, 14,
                 e.expr.c_str());
        history_browser_->add(expr_line);

        // 結果行: 緑（エラーは赤）、右寄せ風インデント
        char res_line[640];
        Fl_Color rc = e.is_error ? COL_ERROR_FG : COL_RESULT_FG;
        snprintf(res_line, sizeof(res_line),
                 "@C%d@F%d@S%d    = %s",
                 (int)rc, (int)FL_COURIER, 14,
                 e.result.c_str());
        history_browser_->add(res_line);
    }
    // 最下行にスクロール
    history_browser_->bottomline(history_browser_->size());
}

// --- 履歴ナビゲーション (↑↓キー) ---
void MainWindow::nav_history(int delta) {
    if (history_.empty()) return;
    int n = (int)history_.size();
    if (history_cursor_ == -1 && delta == -1) {
        history_cursor_ = n - 1;
    } else {
        history_cursor_ += delta;
        if (history_cursor_ < 0)  history_cursor_ = 0;
        if (history_cursor_ >= n) { history_cursor_ = -1; input_->value(""); return; }
    }
    input_->value(history_[history_cursor_].expr.c_str());
    input_->insert_position(input_->size());
}

// --- コールバック ---
void MainWindow::input_cb(Fl_Widget *, void *data) {
    static_cast<MainWindow *>(data)->eval_current();
}

void MainWindow::browser_cb(Fl_Widget *, void *data) {
    // ブラウザの行をクリックすると式を入力欄に貼り付け
    auto *self = static_cast<MainWindow *>(data);
    int line = self->history_browser_->value();
    if (line <= 0) return;
    // 奇数行 = 式行、偶数行 = 結果行
    int idx = (line - 1) / 2;
    if (idx >= 0 && idx < (int)self->history_.size()) {
        self->input_->value(self->history_[idx].expr.c_str());
        self->input_->insert_position(self->input_->size());
        self->input_->take_focus();
    }
}

// --- キーハンドリング ---
int MainWindow::handle(int event) {
    if (event == FL_KEYDOWN) {
        int key = Fl::event_key();
        if (Fl::focus() == input_) {
            if (key == FL_Up) {
                nav_history(-1);
                return 1;
            }
            if (key == FL_Down) {
                nav_history(+1);
                return 1;
            }
        }
    }
    return Fl_Double_Window::handle(event);
}
