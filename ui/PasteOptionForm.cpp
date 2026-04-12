// 移植元: Calctus/UI/PasteOptionForm.cs

#include "PasteOptionForm.h"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cstdlib>

// ---- カラー ----
static const Fl_Color PF_BG       = fl_rgb_color( 38,  38,  43);
static const Fl_Color PF_INPUT_BG = fl_rgb_color( 50,  52,  60);
static const Fl_Color PF_BTN_BG   = fl_rgb_color( 55,  60,  75);
static const Fl_Color PF_TEXT     = fl_rgb_color(215, 215, 225);

// ---- テキストユーティリティ ----

// 改行で分割 (\r\n / \n / \r すべて対応)
static std::vector<std::string> split_lines(const std::string &text) {
    std::vector<std::string> lines;
    std::string line;
    for (size_t i = 0; i < text.size(); i++) {
        char c = text[i];
        if (c == '\r') {
            lines.push_back(line); line.clear();
            if (i + 1 < text.size() && text[i+1] == '\n') ++i;
        } else if (c == '\n') {
            lines.push_back(line); line.clear();
        } else {
            line += c;
        }
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}

// 移植元: PasteOptionForm.splitLine()
static std::vector<std::string> split_by(const std::string &s, char delim) {
    std::vector<std::string> result;
    if (delim == ' ') {
        // スペース: 連続スペースをまとめて分割 (空トークン除去)
        std::string tok;
        for (char c : s) {
            if (c == ' ' || c == '\t') {
                if (!tok.empty()) { result.push_back(tok); tok.clear(); }
            } else {
                tok += c;
            }
        }
        if (!tok.empty()) result.push_back(tok);
    } else {
        std::string tok;
        for (char c : s) {
            if (c == delim) { result.push_back(tok); tok.clear(); }
            else             tok += c;
        }
        result.push_back(tok);
    }
    return result;
}

static std::string trim(const std::string &s) {
    size_t b = s.find_first_not_of(" \t");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t");
    return s.substr(b, e - b + 1);
}

// ---- コンストラクタ ----

PasteOptionForm::PasteOptionForm(const std::string &clipboard_text)
    : Fl_Window(640, 460, "Paste Options")
{
    set_modal();
    color(PF_BG);

    // 改行コードを \n に正規化、末尾トリム
    source_text_.reserve(clipboard_text.size());
    for (size_t i = 0; i < clipboard_text.size(); i++) {
        char c = clipboard_text[i];
        if (c == '\r') {
            source_text_ += '\n';
            if (i + 1 < clipboard_text.size() && clipboard_text[i+1] == '\n') ++i;
        } else {
            source_text_ += c;
        }
    }
    while (!source_text_.empty() && source_text_.back() == '\n')
        source_text_.pop_back();

    src_buf_     = new Fl_Text_Buffer();
    preview_buf_ = new Fl_Text_Buffer();
    src_buf_->text(source_text_.c_str());
    preview_buf_->text(source_text_.c_str());

    // ---- レイアウト定数 ----
    const int MX    = 8;      // 外マージン
    const int GAP   = 8;      // 左右テキスト間隔
    const int LBL_H = 16;
    const int TXT_H = 248;
    const int C_H   = 24;     // コントロール高さ
    const int BTN_H = 26;
    const int W     = 640, H = 460;
    const int half_w = (W - MX * 2 - GAP) / 2;

    int y = MX;

    // ---- テキストエリアのラベル ----
    auto make_label = [&](int x, int lw, const char *txt) {
        auto *b = new Fl_Box(x, y, lw, LBL_H, txt);
        b->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        b->labelcolor(PF_TEXT);
        b->color(PF_BG);
        b->box(FL_NO_BOX);
    };
    make_label(MX,               half_w, "Clipboard Text:");
    make_label(MX + half_w + GAP, half_w, "Text will be pasted:");
    y += LBL_H + 2;

    // ---- テキストエリア ----
    auto setup_text = [&](Fl_Text_Display *td) {
        td->color(PF_INPUT_BG);
        td->textcolor(PF_TEXT);
        td->textfont(FL_COURIER);
        td->textsize(12);
        td->box(FL_FLAT_BOX);
        td->scrollbar_width(10);
    };

    src_display_ = new Fl_Text_Editor(MX, y, half_w, TXT_H);
    src_display_->buffer(src_buf_);
    setup_text(src_display_);

    preview_editor_ = new Fl_Text_Editor(MX + half_w + GAP, y, half_w, TXT_H);
    preview_editor_->buffer(preview_buf_);
    setup_text(preview_editor_);

    y += TXT_H + MX;

    // ---- コントロール行1: Delimiter / Column select ----
    auto make_small_label = [&](int x, int lw, const char *txt) {
        auto *b = new Fl_Box(x, y, lw, C_H, txt);
        b->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        b->labelcolor(PF_TEXT);
        b->color(PF_BG);
        b->box(FL_NO_BOX);
    };
    auto make_input = [&](int x, int iw) -> Fl_Input* {
        auto *in = new Fl_Input(x, y, iw, C_H);
        in->color(PF_INPUT_BG);
        in->textcolor(PF_TEXT);
        in->textfont(FL_COURIER);
        in->textsize(12);
        in->box(FL_FLAT_BOX);
        return in;
    };
    auto make_btn = [&](int x, int bw, const char *lbl) -> Fl_Button* {
        auto *b = new Fl_Button(x, y, bw, C_H, lbl);
        b->color(PF_BTN_BG);
        b->labelcolor(PF_TEXT);
        b->box(FL_FLAT_BOX);
        return b;
    };

    // テキストエリアの右端 x 座標
    const int right_x = MX + half_w + GAP;

    // ---- 行1 (y):
    //   左: "Column Delimiter:" [input]
    //   右: [Remove Commas] [Remove Right Hands]
    //
    // ---- 行2 (y + C_H + gap):
    //   左: "Column Index:" [input] /N [Select Column]
    //
    // ---- 行3 (右下): [OK] [Cancel]

    // 左: Column Delimiter
    make_small_label(MX, 110, "Column Delimiter:");
    delimiter_input_ = make_input(MX + 114, 42);
    delimiter_input_->callback(delimiter_cb, this);
    delimiter_input_->when(FL_WHEN_CHANGED);

    // 右: Remove ボタン2つ (同じ y)
    remove_comma_btn_ = make_btn(right_x, 140, "Remove Commas");
    remove_comma_btn_->callback(remove_comma_cb, this);

    remove_right_btn_ = make_btn(right_x + 148, 160, "Remove Right-hands");
    remove_right_btn_->callback(remove_right_cb, this);

    y += C_H + MX / 2;

    // 左: Column Index / Select Column
    make_small_label(MX, 110, "Column Index:");
    col_number_input_ = make_input(MX + 114, 42);
    col_number_input_->value("1");

    num_cols_label_ = new Fl_Box(MX + 114 + 48, y, 40, C_H, "");
    num_cols_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    num_cols_label_->labelcolor(PF_TEXT);
    num_cols_label_->color(PF_BG);
    num_cols_label_->box(FL_NO_BOX);

    select_col_btn_ = make_btn(MX + 114 + 48 + 44, 120, "Select Column");
    select_col_btn_->callback(select_col_cb, this);

    // ---- OK / Cancel (右下) ----
    cancel_btn_ = new Fl_Button(W - MX - 90, H - MX - BTN_H, 90, BTN_H, "Cancel");
    cancel_btn_->color(PF_BTN_BG);
    cancel_btn_->labelcolor(PF_TEXT);
    cancel_btn_->box(FL_FLAT_BOX);
    cancel_btn_->callback(cancel_cb, this);

    ok_btn_ = new Fl_Return_Button(W - MX - 90 - MX - 90, H - MX - BTN_H, 90, BTN_H, "OK");
    ok_btn_->color(PF_BTN_BG);
    ok_btn_->labelcolor(PF_TEXT);
    ok_btn_->box(FL_FLAT_BOX);
    ok_btn_->callback(ok_cb, this);

    end();

    // デリミタ自動検出 (移植元: PasteOptionForm_Load)
    if (source_text_.find('\t') != std::string::npos)
        delimiter_input_->value("\\t");
    else if (source_text_.find('|') != std::string::npos)
        delimiter_input_->value("|");
    else if (source_text_.find(',') != std::string::npos)
        delimiter_input_->value(",");
    else
        delimiter_input_->value(" ");

    update_num_cols();
}

PasteOptionForm::~PasteOptionForm() {
    // バッファを切り離してから解放
    src_display_->buffer(nullptr);
    preview_editor_->buffer(nullptr);
    delete src_buf_;
    delete preview_buf_;
}

// ---- 公開メソッド ----

bool PasteOptionForm::run() {
    show();
    while (shown()) Fl::wait();
    return ok_pressed_;
}

std::vector<std::string> PasteOptionForm::result_lines() const {
    std::string s(preview_buf_->text());
    // 末尾改行トリム
    while (!s.empty() && s.back() == '\n') s.pop_back();
    if (s.empty()) return {};
    return split_lines(s);
}

// ---- 内部処理 ----

char PasteOptionForm::get_delimiter() const {
    const char *v = delimiter_input_->value();
    if (strcmp(v, "\\t") == 0) return '\t';
    if (v[0] == '\0') return ' ';
    return v[0];
}

// 移植元: PasteOptionForm.ColumnDelimiterText_TextChanged()
void PasteOptionForm::update_num_cols() {
    char d = get_delimiter();
    auto lines = split_lines(source_text_);
    int max_cols = 0;
    for (const auto &line : lines) {
        if (line.empty()) continue;
        int n = (int)split_by(line, d).size();
        if (n > max_cols) max_cols = n;
    }
    snprintf(num_cols_text_, sizeof(num_cols_text_), "/ %d", max_cols);
    num_cols_label_->label(num_cols_text_);
    num_cols_label_->redraw();
}

// 移植元: PasteOptionForm.SelectColumnButton_Click()
void PasteOptionForm::select_column() {
    int col_n = 0;
    if (sscanf(col_number_input_->value(), "%d", &col_n) != 1 || col_n < 0) return;

    if (col_n == 0) {
        // 0 指定: 全カラム (ソースに戻す)
        preview_buf_->text(source_text_.c_str());
        return;
    }

    int col_idx = col_n - 1;  // 0-based
    char d = get_delimiter();
    auto lines = split_lines(source_text_);
    std::string result;

    for (const auto &line : lines) {
        if (line.empty()) continue;
        auto cols = split_by(line, d);
        if (col_idx < (int)cols.size()) {
            if (!result.empty()) result += '\n';
            result += trim(cols[col_idx]);
        }
    }
    preview_buf_->text(result.c_str());
}

// 移植元: PasteOptionForm.RemoveCommaButton_Click()
void PasteOptionForm::remove_commas() {
    std::string s(preview_buf_->text());
    s.erase(std::remove(s.begin(), s.end(), ','), s.end());
    preview_buf_->text(s.c_str());
}

// 移植元: PasteOptionForm.RemoveRightHandsButton_Click()
void PasteOptionForm::remove_right_hands() {
    std::string s(preview_buf_->text());
    auto lines = split_lines(s);
    std::string result;

    for (auto &line : lines) {
        line = trim(line);
        if (line.empty()) continue;
        // 最後の '=' 以降を除去
        size_t eq = line.rfind('=');
        if (eq != std::string::npos) {
            line = trim(line.substr(0, eq));
        }
        if (!result.empty()) result += '\n';
        result += line;
    }
    preview_buf_->text(result.c_str());
}

// ---- コールバック ----

void PasteOptionForm::ok_cb(Fl_Widget*, void *data) {
    auto *self = static_cast<PasteOptionForm*>(data);
    self->ok_pressed_ = true;
    self->hide();
}

void PasteOptionForm::cancel_cb(Fl_Widget*, void *data) {
    static_cast<PasteOptionForm*>(data)->hide();
}

void PasteOptionForm::select_col_cb(Fl_Widget*, void *data) {
    static_cast<PasteOptionForm*>(data)->select_column();
}

void PasteOptionForm::remove_comma_cb(Fl_Widget*, void *data) {
    static_cast<PasteOptionForm*>(data)->remove_commas();
}

void PasteOptionForm::remove_right_cb(Fl_Widget*, void *data) {
    static_cast<PasteOptionForm*>(data)->remove_right_hands();
}

void PasteOptionForm::delimiter_cb(Fl_Widget*, void *data) {
    static_cast<PasteOptionForm*>(data)->update_num_cols();
}
