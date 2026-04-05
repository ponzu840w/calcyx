// SheetView: 複数行の式/結果表示ウィジェット
// 移植元: Calctus/UI/SheetView.cs (簡略版)

#pragma once

#include <FL/Fl_Group.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Scrollbar.H>
#include <string>
#include <vector>

extern "C" {
#include "eval/eval.h"
#include "eval/builtin.h"
#include "types/val.h"
}

class SheetView : public Fl_Group {
public:
    SheetView(int x, int y, int w, int h);
    ~SheetView();

    // フォーカス行のフォーマットを変更
    void apply_fmt(val_fmt_t fmt);
    // リアルタイム評価 (SheetLineInput から呼ばれる)
    void live_eval();
    // テキストファイルを読み込んで全行を置き換え
    bool load_file(const char *path);
    // テキストファイルに保存
    bool save_file(const char *path);

    void draw()   override;
    int  handle(int event) override;
    void resize(int x, int y, int w, int h) override;

private:
    struct Row {
        std::string expr;
        std::string result;
        bool        error = false;
        val_fmt_t   fmt   = FMT_REAL;   // FMT_REAL = Auto (自然フォーマット)
    };

    std::vector<Row> rows_;
    int focused_row_ = 0;
    int scroll_top_  = 0;

    Fl_Input     *editor_;
    Fl_Scrollbar *vscroll_;
    eval_ctx_t    ctx_;

    static const int ROW_H = 24;
    static const int SB_W  = 14;
    static const int PAD   = 3;

    int sheet_w()   const { return w() - SB_W; }
    int expr_w()    const { return sheet_w() * 3 / 5; }
    int vis_rows()  const { return h() / ROW_H + 1; }

    void eval_all();
    void commit();                          // editor → rows_[focused_row_].expr, then eval_all
    void focus_row(int idx);
    void insert_row(int after);
    void delete_row(int idx);
    void sync_scroll();
    void place_editor();
    int  row_at_y(int fy) const;           // fy = y座標 relative to widget top
};
