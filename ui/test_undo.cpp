// Undo/Redo 動作確認テスト
// FLTK ウィンドウを生成して SheetView の操作を直接呼び出す。

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <cstdio>
#include <cstring>
#include "SheetView.h"

static int g_pass = 0;
static int g_fail = 0;

static void check(const char *label, bool ok) {
    if (ok) {
        printf("  PASS: %s\n", label);
        g_pass++;
    } else {
        printf("  FAIL: %s\n", label);
        g_fail++;
    }
}

// ---- テストケース ----

// 基本: 式を入力→Undo→Redo
static void test_basic_commit_undo_redo(SheetView *sv) {
    printf("[test_basic_commit_undo_redo]\n");

    sv->test_type_and_commit("1+1");
    check("commit: expr == '1+1'", sv->row_expr(0) == "1+1");
    check("after commit: can_undo", sv->can_undo());
    check("after commit: !can_redo", !sv->can_redo());

    sv->undo();
    check("after undo: expr == ''", sv->row_expr(0) == "");
    check("after undo: !can_undo", !sv->can_undo());
    check("after undo: can_redo", sv->can_redo());

    sv->redo();
    check("after redo: expr == '1+1'", sv->row_expr(0) == "1+1");
    check("after redo: can_undo", sv->can_undo());
    check("after redo: !can_redo", !sv->can_redo());
}

// 複数の式変更を順にUndo
static void test_multiple_commits(SheetView *sv) {
    printf("[test_multiple_commits]\n");

    sv->test_type_and_commit("aaa");
    sv->test_type_and_commit("bbb");
    sv->test_type_and_commit("ccc");
    check("expr == 'ccc'", sv->row_expr(0) == "ccc");

    sv->undo();
    check("undo1: expr == 'bbb'", sv->row_expr(0) == "bbb");
    sv->undo();
    check("undo2: expr == 'aaa'", sv->row_expr(0) == "aaa");
    sv->undo();
    check("undo3: expr == ''",    sv->row_expr(0) == "");

    sv->redo();
    check("redo1: expr == 'aaa'", sv->row_expr(0) == "aaa");
    sv->redo();
    check("redo2: expr == 'bbb'", sv->row_expr(0) == "bbb");
    sv->redo();
    check("redo3: expr == 'ccc'", sv->row_expr(0) == "ccc");
}

// Undo 後に新たな操作をすると Redo 履歴が消える
static void test_redo_cleared_after_new_commit(SheetView *sv) {
    printf("[test_redo_cleared_after_new_commit]\n");

    sv->test_type_and_commit("aaa");
    sv->test_type_and_commit("bbb");
    sv->undo();
    check("before fork: expr == 'aaa'", sv->row_expr(0) == "aaa");
    check("before fork: can_redo", sv->can_redo());

    sv->test_type_and_commit("ccc");  // ここで Redo 履歴が消える
    check("after fork: expr == 'ccc'", sv->row_expr(0) == "ccc");
    check("after fork: !can_redo", !sv->can_redo());

    sv->undo();
    check("undo after fork: expr == 'aaa'", sv->row_expr(0) == "aaa");
}

// 行挿入のUndo/Redo
static void test_insert_row_undo(SheetView *sv) {
    printf("[test_insert_row_undo]\n");

    sv->test_type_and_commit("row0");
    check("row_count == 1", sv->row_count() == 1);

    sv->test_insert_row(0);  // row0 の後に新行挿入 → 行数2、focused=1
    check("after insert: row_count == 2", sv->row_count() == 2);
    check("after insert: focused == 1", sv->focused_row() == 1);

    sv->undo();  // 挿入を取り消し
    check("undo insert: row_count == 1", sv->row_count() == 1);
    check("undo insert: row0 expr intact", sv->row_expr(0) == "row0");

    sv->redo();  // 挿入を再適用
    check("redo insert: row_count == 2", sv->row_count() == 2);
}

// 行削除のUndo/Redo
static void test_delete_row_undo(SheetView *sv) {
    printf("[test_delete_row_undo]\n");

    sv->test_type_and_commit("row0");
    sv->test_insert_row(0);
    sv->test_type_and_commit("row1");
    check("setup: 2 rows", sv->row_count() == 2);

    sv->test_delete_row(0);  // row0 を削除
    check("after delete: row_count == 1", sv->row_count() == 1);
    check("after delete: row0 == 'row1'", sv->row_expr(0) == "row1");

    sv->undo();
    check("undo delete: row_count == 2", sv->row_count() == 2);
    check("undo delete: row0 == 'row0'", sv->row_expr(0) == "row0");
    check("undo delete: row1 == 'row1'", sv->row_expr(1) == "row1");

    sv->redo();
    check("redo delete: row_count == 1", sv->row_count() == 1);
    check("redo delete: row0 == 'row1'", sv->row_expr(0) == "row1");
}

// 1行だけのとき削除するとクリアになる (行数は1のまま)
static void test_delete_last_row(SheetView *sv) {
    printf("[test_delete_last_row]\n");

    sv->test_type_and_commit("only");
    check("setup: 1 row", sv->row_count() == 1);

    sv->test_delete_row(0);
    check("after delete: row_count still 1", sv->row_count() == 1);
    check("after delete: expr == ''", sv->row_expr(0) == "");

    sv->undo();
    check("undo: expr == 'only'", sv->row_expr(0) == "only");
}

// 未コミット編集中に Undo した場合の挙動確認
static void test_undo_with_uncommitted_edit(SheetView *sv) {
    printf("[test_undo_with_uncommitted_edit]\n");

    sv->test_type_and_commit("row0_edited");
    sv->test_insert_row(0);   // 行1を作成、フォーカスが行1へ
    check("setup: 2 rows", sv->row_count() == 2);
    check("setup: focused on row1", sv->focused_row() == 1);

    // 行1を「未コミット」のまま編集: editor に入力するが commit() しない
    // test_type_and_commit の代わりに editor 値だけ変える
    // (SheetView 外から editor_ に直接アクセスできないので、
    //  test_type_and_commit を "途中まで" 模倣: original_expr_ = "" のまま editor に書く)
    // → ここでは live_eval 相当として test_type_and_commit("row1_partial") 後に
    //   undo で "row1_partial" が消えることを確認する。
    sv->test_type_and_commit("row1_partial");  // コミット済み
    check("row1 committed", sv->row_expr(1) == "row1_partial");

    // 第1回 Undo: 行1のコミットを取り消す
    sv->undo();
    check("undo1: row1 expr == ''", sv->row_expr(1) == "");
    check("undo1: row0 still edited", sv->row_expr(0) == "row0_edited");

    // 第2回 Undo: insert_row を取り消す → 行数1に戻る
    sv->undo();
    check("undo2: row_count == 1", sv->row_count() == 1);
    check("undo2: row0 still edited", sv->row_expr(0) == "row0_edited");

    // 第3回 Undo: 行0のコミットを取り消す
    sv->undo();
    check("undo3: row0 expr == ''", sv->row_expr(0) == "");
}

// ---- SheetView をリセットするユーティリティ ----
// load_file でリセットできないので、Undo を全部戻してから初期化
static void reset_sv(SheetView *sv) {
    // undo を全て巻き戻してから空の状態にする
    while (sv->can_undo()) sv->undo();
    // 空でなければクリア
    if (!sv->row_expr(0).empty()) {
        sv->test_type_and_commit("");
        // この commit も Undo スタックに入るが、テスト間では問題なし
    }
    // undo スタックを手動クリアするため、一度 load_file を使う
    // (load_file は undo_buf_.clear() を呼ぶ)
    // ここでは便宜上、テスト間でそのまま使う (各テストは独立した操作を追加するだけ)
}

int main(int argc, char **argv) {
    // FLTK 初期化: 表示なしで widget を生成するには show() が必要
    Fl_Window *win = new Fl_Window(800, 600, "test_undo");
    SheetView *sv  = new SheetView(0, 0, 800, 600);
    win->end();
    win->show(argc, argv);   // font/draw context の初期化に必要
    Fl::wait(0);             // 初期イベントを処理

    // 各テストの前に load_file 相当のリセットが必要なため、
    // テストごとに独立した SheetView を作るのが理想だが、
    // ここでは load_file (undo スタックをクリア) + 式リセットで代替する
    auto reset = [&]() {
        // undo_buf_ をクリアするために empty string を書き込み load_file 効果を再現
        // 実際には SheetView が持つ load_file を使う
        // ファイルがないので直接 UndoStack クリアはできないため、
        // 新しい SheetView を作り直す方式を採用
        win->remove(sv);
        delete sv;
        sv = new SheetView(0, 0, 800, 600);
        win->add(sv);
        win->redraw();
        Fl::wait(0);
    };

    test_basic_commit_undo_redo(sv);   reset();
    test_multiple_commits(sv);         reset();
    test_redo_cleared_after_new_commit(sv); reset();
    test_insert_row_undo(sv);          reset();
    test_delete_row_undo(sv);          reset();
    test_delete_last_row(sv);          reset();
    test_undo_with_uncommitted_edit(sv);

    printf("\n=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
