// 入力補完ポップアップ
// 移植元: Calctus/UI/Sheets/InputCandidateForm.cs (簡略版)
// OS ウィンドウではなくメインウィンドウの最後の子 Fl_Group として実装
// → フォーカス問題・二重ウィンドウ問題を根本解消

#pragma once

#include <FL/Fl_Group.H>
#include <FL/Fl_Select_Browser.H>
#include <FL/Fl_Box.H>
#include <string>
#include <vector>

struct Candidate {
    std::string id;          // 挿入されるテキスト
    std::string label;       // 表示ラベル（引数情報付き）
    std::string description; // 説明文（builtin_docs.cpp より）
    bool        is_function; // 関数なら true
};

class CompletionPopup : public Fl_Group {
public:
    CompletionPopup();

    // 全候補を設定（シート評価後に呼ぶ）
    void set_all(std::vector<Candidate> all);

    // メインウィンドウ相対座標 (wx, wy_below) に表示して key で絞り込む
    // wy_below: エディタ下端、editor_top: エディタ上端（上に出す場合に使用）
    void show_at(int wx, int wy_below, int editor_top, const std::string &key);

    // 表示中に key が変わったとき絞り込みを更新
    void update_key(const std::string &key);

    void hide_popup();

    bool is_shown() const { return shown_; }

    void select_prev();
    void select_next();

    // 現在選択されている候補（なければ nullptr）
    const Candidate *selected() const;

    void draw()   override;
    void resize(int x, int y, int w, int h) override;

private:
    Fl_Select_Browser *list_;
    Fl_Box            *desc_;

    std::vector<Candidate> all_;
    std::vector<Candidate> filtered_;
    bool shown_  = false;
    int  cur_h_  = 0;   // rebuild() が計算した適切な高さ
    int  max_h_  = 0;   // show_at() で確定した表示可能な最大高さ (0=無制限)

    static constexpr int POP_W  = 280;
    static constexpr int DESC_H = 40;
    static constexpr int MAX_VIS = 8;

    void rebuild(const std::string &key);

    static void list_cb(Fl_Widget *, void *);
};
