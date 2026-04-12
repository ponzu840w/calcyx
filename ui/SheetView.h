// SheetView: 複数行の式/結果表示ウィジェット
// 移植元: Calctus/UI/SheetView.cs (簡略版)

#pragma once

#include <FL/Fl_Group.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Scrollbar.H>
#include <string>
#include <vector>

#include "CompletionPopup.h"

extern "C" {
#include "eval/eval.h"
#include "eval/builtin.h"
#include "types/val.h"
#include "parser/lexer.h"
}

class SheetView : public Fl_Group {
public:
    SheetView(int x, int y, int w, int h);
    ~SheetView();

    // フォーカス行のフォーマットを変更 (移植元: SheetViewItem.ReplaceFormatterFunction)
    // func_name: "hex"/"bin"/"oct"/"dec"/"si"/"kibi"/"char"、nullptr で Auto (ラッパー除去)
    void apply_fmt(const char *func_name);
    // リアルタイム評価 (SheetLineInput から呼ばれる)
    void live_eval();
    // テキストファイルを読み込んで全行を置き換え
    bool load_file(const char *path);
    // テキストファイルに保存
    bool save_file(const char *path);

    // フォーカス行変更時に呼ばれるコールバックを設定
    void set_row_change_cb(void (*cb)(void *), void *data);
    // 現在フォーカス行のフォーマッタ関数名 (nullptr = Auto)
    const char *current_fmt_name() const;

    // 入力補完 (SheetLineInput から呼ばれる)
    void completion_update();
    void completion_confirm();
    void completion_hide();

    // Tab キー処理 (SheetLineInput から呼ばれる)
    void focus_result();       // editor_ → result_display_ へフォーカス移動
    void tab_from_result();    // result_display_ → 次行 editor_ へフォーカス移動

    // 行操作
    void delete_row_up();      // 現在行削除・上の行へ移動
    void move_row_up();        // 現在行を上にスワップ (Ctrl+Shift+↑)
    void move_row_down();      // 現在行を下にスワップ (Ctrl+Shift+↓)
    void clear_all();          // 全行削除 (Ctrl+Shift+Del / File>All Clear)

    // クリップボード
    void copy_all_to_clipboard() const;  // 全行を "式 = 結果" 形式でコピー
    CompletionPopup *popup_ = nullptr;

    // Undo / Redo
    void undo();
    void redo();
    bool can_undo() const { return undo_idx_ > 0; }
    bool can_redo() const { return undo_idx_ < (int)undo_buf_.size(); }

    // テスト用ヘルパー
    int         row_count() const { return (int)rows_.size(); }
    std::string row_expr(int i) const { return (i >= 0 && i < (int)rows_.size()) ? rows_[i].expr : ""; }
    int         focused_row() const { return focused_row_; }
    void        test_type_and_commit(const char *expr);  // editor に入力してコミット
    void        test_insert_row(int after) { insert_row(after); }
    void        test_delete_row(int idx)   { delete_row(idx); }

    void draw()   override;
    int  handle(int event) override;
    void resize(int x, int y, int w, int h) override;

private:
    struct Row {
        std::string expr;
        std::string result;
        bool        error       = false;
        bool        wrapped     = false;     // 式幅が eq_pos_ を超える場合に2行レイアウト
        val_fmt_t   result_fmt  = FMT_REAL;  // 結果値の実際のフォーマット (ハイライト用)
    };

    // ---- Undo / Redo ----
    enum class UndoOpType { Insert, Delete, ChangeExpr };
    struct UndoOp {
        UndoOpType  type;
        int         index;
        std::string expr;
    };
    struct UndoViewState {
        int focused_row;
        int cursor_pos;
    };
    struct UndoEntry {
        UndoViewState       view_state;
        std::vector<UndoOp> undo_ops;
        std::vector<UndoOp> redo_ops;
    };
    static const int UNDO_DEPTH = 1000;
    std::vector<UndoEntry> undo_buf_;
    int                    undo_idx_ = 0;
    std::string            original_expr_;  // focus_row() 時点の式 (Undo 比較用)

    UndoViewState capture_view_state() const;
    void          apply_ops(const std::vector<UndoOp> &ops);
    void          commit_undo_entry(UndoEntry entry);

    std::vector<Row> rows_;
    int focused_row_ = 0;
    int scroll_top_  = 0;

    Fl_Input        *editor_;
    Fl_Input        *result_display_;  // フォーカス行の結果表示 (read-only)
    Fl_Scrollbar    *vscroll_;
    eval_ctx_t       ctx_;

    void (*row_change_cb_)(void *) = nullptr;
    void  *row_change_data_        = nullptr;

    static const int ROW_H = 24;
    static const int SB_W  = 14;
    static const int PAD   = 3;

    // "=" カラム位置・幅 (update_layout() が動的に設定)
    int eq_pos_ = 0;   // sheet 左端からの "=" カラム開始 x オフセット
    int eq_w_   = 22;  // "=" カラム幅 (フォントから算出)

    int sheet_w()    const { return w() - SB_W; }
    int expr_w()     const { return eq_pos_; }
    int eq_col_x()   const { return x() + eq_pos_; }
    int result_x()   const { return x() + eq_pos_ + eq_w_; }
    int result_w()   const { return sheet_w() - eq_pos_ - eq_w_; }

    // 行 i の高さ: 折り返しなら ROW_H*2、そうでなければ ROW_H
    int row_h(int i) const { return (i >= 0 && i < (int)rows_.size() && rows_[i].wrapped) ? ROW_H * 2 : ROW_H; }

    void eval_all();
    void update_layout();                  // 全行を走査して eq_pos_ / eq_w_ を算出
    void commit();
    void focus_row(int idx);
    void insert_row(int after);
    void delete_row(int idx, bool move_up = false);
    void sync_scroll();
    void place_editor();
    void update_result_display();
    int  row_at_y(int fy) const;

    // ---- 入力補完 ----
    std::vector<Candidate> build_candidates() const;
};
