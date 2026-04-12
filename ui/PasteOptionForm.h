// 移植元: Calctus/UI/PasteOptionForm.cs
#pragma once

#include <string>
#include <vector>
#include <FL/Fl_Window.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Text_Editor.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Return_Button.H>
#include <FL/Fl_Box.H>

class PasteOptionForm : public Fl_Window {
public:
    // clipboard_text: クリップボードから得た生テキスト
    explicit PasteOptionForm(const std::string &clipboard_text);
    ~PasteOptionForm() override;

    // モーダル表示。true=OK、false=Cancel
    bool run();

    // OK 時にペーストする行の配列 (run() が true を返した後に呼ぶ)
    std::vector<std::string> result_lines() const;

private:
    std::string source_text_;   // 正規化済みソーステキスト (\n 区切り)
    bool ok_pressed_ = false;
    char num_cols_text_[32] = {};

    Fl_Text_Buffer   *src_buf_;
    Fl_Text_Buffer   *preview_buf_;
    Fl_Text_Display  *src_display_;
    Fl_Text_Editor   *preview_editor_;
    Fl_Input         *delimiter_input_;
    Fl_Input         *col_number_input_;
    Fl_Box           *num_cols_label_;
    Fl_Button        *select_col_btn_;
    Fl_Button        *remove_comma_btn_;
    Fl_Button        *remove_right_btn_;
    Fl_Return_Button *ok_btn_;
    Fl_Button        *cancel_btn_;

    char get_delimiter() const;
    void update_num_cols();
    void select_column();
    void remove_commas();
    void remove_right_hands();

    static void ok_cb           (Fl_Widget*, void*);
    static void cancel_cb       (Fl_Widget*, void*);
    static void select_col_cb   (Fl_Widget*, void*);
    static void remove_comma_cb (Fl_Widget*, void*);
    static void remove_right_cb (Fl_Widget*, void*);
    static void delimiter_cb    (Fl_Widget*, void*);
};
