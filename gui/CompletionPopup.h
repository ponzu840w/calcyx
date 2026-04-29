// 入力補完ポップアップ
// 移植元: Calctus/UI/Sheets/InputCandidateForm.cs (簡略版)
//
// CompletionPopupBase は widget 型に依存しない共通ロジック
// (候補リスト・フィルタ・選択状態・色適用) を担う。
// 派生の CompletionPopup (Fl_Group) はメインウィンドウ内に埋め込まれ、
// CompletionPopupWindow (Fl_Menu_Window) は独立した borderless トップ
// レベルウィンドウとして画面はみ出しを許す。どちらを使うかは設定と
// コンパクトモードの状態から MainWindow が選択する。
//
// SheetView 側は CompletionPopupBase * として扱い、どちらの実装かは
// 関知しない。

#pragma once

#include <FL/Fl_Group.H>
#include <FL/Fl_Menu_Window.H>
#include <FL/Fl_Select_Browser.H>
#include <FL/Fl_Box.H>
#include "completion_filter.hpp"
#include <string>
#include <vector>

/* 共有実装に統合済み。 既存呼び出し側のため alias を残す。 */
using Candidate = calcyx::Candidate;

// 抽象基底: widget 型を問わない共通 API + 内部状態。
// Fl_Group / Fl_Menu_Window の派生が multiple inheritance で混ぜる。
class CompletionPopupBase {
public:
    virtual ~CompletionPopupBase() = default;

    // 全候補を設定（シート評価後に呼ぶ）
    void set_all(std::vector<Candidate> all);

    // メインウィンドウ相対座標 (wx, wy_below) に表示して key で絞り込む
    // wy_below: エディタ下端、editor_top: エディタ上端（上に出す場合に使用）
    virtual void show_at(int wx, int wy_below, int editor_top, const std::string &key) = 0;

    // 表示中に key が変わったとき絞り込みを更新
    virtual void update_key(const std::string &key) = 0;

    virtual void hide_popup() = 0;

    // メインウィンドウ相対の (wx, wy) が popup の可視領域内か。
    // 埋め込み実装は x/y/w/h で判定、独立ウィンドウ実装は常に false
    // (別 OS ウィンドウなのでメインウィンドウ座標には存在しない)。
    virtual bool contains_window_point(int wx, int wy) const = 0;

    bool is_shown() const { return shown_; }

    void select_prev();
    void select_next();

    // 現在選択されている候補（なければ nullptr）
    const Candidate *selected() const;

    void apply_colors();   // g_colors からポップアップ色を再適用

protected:
    // 派生コンストラクタは自ウィジェットを FLTK の current group にしてから
    // init_widgets() を呼ぶ。list_ / desc_ が base 側に作成される。
    void init_widgets(int w);

    void rebuild(const std::string &key);

    static void list_cb(Fl_Widget *, void *);

    Fl_Select_Browser *list_ = nullptr;
    Fl_Box            *desc_ = nullptr;

    std::vector<Candidate> all_;
    std::vector<Candidate> filtered_;
    bool shown_  = false;
    int  cur_h_  = 0;   // rebuild() が計算した適切な高さ
    int  max_h_  = 0;   // show_at() で確定した表示可能な最大高さ (0=無制限)

    static constexpr int POP_W  = 280;
    static constexpr int DESC_H = 40;
    static constexpr int MAX_VIS = 8;
};

// 埋め込み実装: メインウィンドウの子 Fl_Group。親にクリップされる。
class CompletionPopup : public Fl_Group, public CompletionPopupBase {
public:
    CompletionPopup();

    void show_at(int wx, int wy_below, int editor_top, const std::string &key) override;
    void update_key(const std::string &key) override;
    void hide_popup() override;
    bool contains_window_point(int wx, int wy) const override;

    void draw()   override;
    void resize(int x, int y, int w, int h) override;
};

// 独立ウィンドウ実装: Fl_Menu_Window (borderless トップレベル)。
// 親ウィンドウ外にはみ出せるが、ウィンドウ生成/破棄とフォーカス管理が
// 伴う。画面絶対座標で show() する。
class MainWindow;
class CompletionPopupWindow : public Fl_Menu_Window, public CompletionPopupBase {
public:
    explicit CompletionPopupWindow(MainWindow *main);

    void show_at(int wx, int wy_below, int editor_top, const std::string &key) override;
    void update_key(const std::string &key) override;
    void hide_popup() override;
    bool contains_window_point(int wx, int wy) const override;

    void draw() override;

private:
    MainWindow *main_;  // 親ウィンドウ相対座標を画面絶対に変換するため保持
};
