/* tui/test_tui_sheet.cpp — TuiSheet end-to-end シナリオテスト
 *
 * TUI は入力も出力もテキストなので、ScreenInteractive ループを回さずに
 *   1. sheet_model を作る
 *   2. TuiSheet にイベントを直接投げる
 *   3. sheet_model / TuiSheet の状態をアサート
 * というフローで end-to-end 検証できる。描画の最終形は Screen に Render して
 * stderr にダンプ (比較はしない)、CI ログで目視確認できるようにする。
 *
 * カバーするシナリオ (tui スモークテスト):
 *   1. 式入力 + Enter
 *   2. Tab 補完
 *   3. Undo / Redo (Ctrl+Z / Ctrl+Y)
 *   4. Ctrl+Shift+Down で行移動
 *   5. F10 で Hex フォーマット切替
 */

#include "TuiSheet.h"

extern "C" {
#include "sheet_model.h"
}

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

using namespace ftxui;
using namespace calcyx::tui;

static int g_failures = 0;

#define EXPECT(label, cond) do {                                              \
    if (!(cond)) {                                                             \
        fprintf(stderr, "FAIL [%s]: %s\n", label, #cond);                     \
        ++g_failures;                                                          \
    }                                                                          \
} while (0)

/* ----------------------------------------------------------------------
 * ヘルパ
 * -------------------------------------------------------------------- */
static std::shared_ptr<TuiSheet> make_sheet(sheet_model_t **out_model) {
    sheet_model_t *m = sheet_model_new();
    const char *empty[] = { "" };
    sheet_model_set_rows(m, empty, 1);
    *out_model = m;
    return MakeTuiSheet(m);
}

static void type_str(TuiSheet &s, const std::string &str) {
    for (char c : str) {
        s.OnEvent(Event::Character(std::string(1, c)));
    }
}

/* 現在の TuiSheet を 80x14 スクリーンに Render して stderr にダンプする。
 * 比較はしない — CI ログで目視する用。 */
static void dump_render(const char *scenario, TuiSheet &sheet) {
    auto screen = Screen::Create(Dimensions{80, 14});
    Element el  = sheet.Render();
    Render(screen, el);
    fprintf(stderr, "\n===== render: %s =====\n%s\n",
            scenario, screen.ToString().c_str());
}

/* ----------------------------------------------------------------------
 * シナリオ 1: 式入力 + Enter
 * -------------------------------------------------------------------- */
static void test_input_and_enter() {
    sheet_model_t *model = nullptr;
    auto sheet = make_sheet(&model);

    type_str(*sheet, "1+2");
    EXPECT("input: editor buf",  sheet->test_editor_buf() == "1+2");
    EXPECT("input: cursor pos",  sheet->test_cursor_pos() == 3);
    EXPECT("input: dirty flag",  sheet->editor_dirty());
    EXPECT("input: live preview", sheet->live_preview() == "3");

    sheet->OnEvent(Event::Return);
    EXPECT("enter: row count",   sheet_model_row_count(model) == 2);
    EXPECT("enter: focused row", sheet->focused_row() == 1);

    const char *expr0 = sheet_model_row_expr(model, 0);
    const char *res0  = sheet_model_row_result(model, 0);
    EXPECT("enter: row0 expr",   expr0 && strcmp(expr0, "1+2") == 0);
    EXPECT("enter: row0 result", res0  && strcmp(res0,  "3")   == 0);

    /* 続けて 2 行目に 4*5 を入れる */
    type_str(*sheet, "4*5");
    sheet->OnEvent(Event::Return);
    EXPECT("enter2: row count",  sheet_model_row_count(model) == 3);
    const char *res1 = sheet_model_row_result(model, 1);
    EXPECT("enter2: row1 result", res1 && strcmp(res1, "20") == 0);

    dump_render("1. input and enter", *sheet);
    sheet.reset();
    sheet_model_free(model);
}

/* ----------------------------------------------------------------------
 * シナリオ 2: Tab 補完
 * -------------------------------------------------------------------- */
static void test_completion() {
    sheet_model_t *model = nullptr;
    auto sheet = make_sheet(&model);

    /* "si" と入力して Tab → sin/si/sign 等の候補が並ぶはず */
    type_str(*sheet, "si");
    EXPECT("compl: not visible yet", !sheet->test_completion_visible());

    sheet->OnEvent(Event::Tab);
    EXPECT("compl: visible after Tab", sheet->test_completion_visible());
    EXPECT("compl: has candidates",    sheet->test_completion_count() > 0);

    /* 最初の候補を確定 (Enter) → editor_buf_ が補完済みに */
    sheet->OnEvent(Event::Return);
    EXPECT("compl: hidden after confirm", !sheet->test_completion_visible());
    /* 補完後は "(" 付きの関数呼び出しになっていることを期待 (先頭候補は関数のはず) */
    const std::string &buf = sheet->test_editor_buf();
    EXPECT("compl: buf contains '('", buf.find('(') != std::string::npos);
    EXPECT("compl: buf begins with 's'", !buf.empty() && (buf[0] == 's' || buf[0] == 'S'));

    dump_render("2. completion si+Tab+Enter", *sheet);
    sheet.reset();
    sheet_model_free(model);
}

/* ----------------------------------------------------------------------
 * シナリオ 3: Undo / Redo
 * -------------------------------------------------------------------- */
static void test_undo_redo() {
    sheet_model_t *model = nullptr;
    auto sheet = make_sheet(&model);

    type_str(*sheet, "7+8");
    sheet->OnEvent(Event::Return);
    EXPECT("undo: baseline row count", sheet_model_row_count(model) == 2);
    const char *expr0_before = sheet_model_row_expr(model, 0);
    EXPECT("undo: baseline expr0",
           expr0_before && strcmp(expr0_before, "7+8") == 0);

    /* Ctrl+Z で行挿入を undo (編集内容 + Enter の複合を 1 ステップずつ戻す) */
    sheet->OnEvent(Event::Special("\x1a"));  /* Ctrl+Z: 行挿入を戻す */
    sheet->OnEvent(Event::Special("\x1a"));  /* Ctrl+Z: 式コミットを戻す */
    EXPECT("undo: row count back to 1", sheet_model_row_count(model) == 1);
    const char *expr0_after_undo = sheet_model_row_expr(model, 0);
    EXPECT("undo: expr cleared",
           expr0_after_undo && strcmp(expr0_after_undo, "") == 0);

    /* Ctrl+Y で redo 2 回 */
    sheet->OnEvent(Event::Special("\x19"));
    sheet->OnEvent(Event::Special("\x19"));
    EXPECT("redo: row count",   sheet_model_row_count(model) == 2);
    const char *expr0_after_redo = sheet_model_row_expr(model, 0);
    EXPECT("redo: expr restored",
           expr0_after_redo && strcmp(expr0_after_redo, "7+8") == 0);

    dump_render("3. undo/redo", *sheet);
    sheet.reset();
    sheet_model_free(model);
}

/* ----------------------------------------------------------------------
 * シナリオ 4: Ctrl+Shift+Down で行移動
 * -------------------------------------------------------------------- */
static void test_move_row() {
    sheet_model_t *model = nullptr;
    auto sheet = make_sheet(&model);

    /* 3 行作る: "A", "B", "C" */
    type_str(*sheet, "1");
    sheet->OnEvent(Event::Return);
    type_str(*sheet, "2");
    sheet->OnEvent(Event::Return);
    type_str(*sheet, "3");
    sheet->OnEvent(Event::Return);
    EXPECT("move: setup row count", sheet_model_row_count(model) == 4);

    /* focused_row_ は最後 (3 行目の 4 行目 = 空行)。row 0 にフォーカスして
     *   Ctrl+Shift+Down で row 0 と row 1 を入れ替える */
    sheet->OnEvent(Event::ArrowUp);
    sheet->OnEvent(Event::ArrowUp);
    sheet->OnEvent(Event::ArrowUp);
    sheet->OnEvent(Event::ArrowUp);
    EXPECT("move: back to row 0", sheet->focused_row() == 0);

    sheet->OnEvent(Event::Special("\x1b[1;6B"));  /* Ctrl+Shift+Down */
    EXPECT("move: focused moved to row 1", sheet->focused_row() == 1);
    const char *row0 = sheet_model_row_expr(model, 0);
    const char *row1 = sheet_model_row_expr(model, 1);
    EXPECT("move: row 0 now '2'", row0 && strcmp(row0, "2") == 0);
    EXPECT("move: row 1 now '1'", row1 && strcmp(row1, "1") == 0);

    dump_render("4. move row down", *sheet);
    sheet.reset();
    sheet_model_free(model);
}

/* ----------------------------------------------------------------------
 * シナリオ 5: F10 で Hex フォーマット切替
 * -------------------------------------------------------------------- */
static void test_format_hex() {
    sheet_model_t *model = nullptr;
    auto sheet = make_sheet(&model);

    type_str(*sheet, "255");
    sheet->OnEvent(Event::Return);

    /* row 0 にフォーカス戻す */
    sheet->OnEvent(Event::ArrowUp);
    EXPECT("fmt: focused row 0", sheet->focused_row() == 0);

    /* F10 = Hex */
    sheet->OnEvent(Event::F10);

    const char *expr0 = sheet_model_row_expr(model, 0);
    EXPECT("fmt: expr wrapped with hex()",
           expr0 && strstr(expr0, "hex(") != nullptr
                 && strstr(expr0, "255")  != nullptr);

    const char *res0 = sheet_model_row_result(model, 0);
    /* hex(255) は "0xff" になる */
    EXPECT("fmt: result starts with 0x",
           res0 && res0[0] == '0' && res0[1] == 'x');

    /* F8 = Auto で剥がす */
    sheet->OnEvent(Event::F8);
    const char *expr_auto = sheet_model_row_expr(model, 0);
    EXPECT("fmt auto: hex() stripped",
           expr_auto && strstr(expr_auto, "hex(") == nullptr);

    dump_render("5. format hex then auto", *sheet);
    sheet.reset();
    sheet_model_free(model);
}

/* ----------------------------------------------------------------------
 * main
 * -------------------------------------------------------------------- */
int main() {
    test_input_and_enter();
    test_completion();
    test_undo_redo();
    test_move_row();
    test_format_hex();

    if (g_failures > 0) {
        fprintf(stderr, "\n%d failure(s)\n", g_failures);
        return 1;
    }
    fprintf(stderr, "\nall TuiSheet scenarios passed\n");
    return 0;
}
