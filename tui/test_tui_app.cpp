/* TuiApp プロンプトモードのシナリオテスト。
 * run() Loop はブロッキングなので test_dispatch() で
 * CatchEvent → prompt_handle_event → sheet OnEvent を直接叩く。 */

#include "PrefsScreen.h"
#include "TuiApp.h"
#include "TuiSheet.h"
#include "clipboard.h"

extern "C" {
#include "sheet_model.h"
#include "i18n.h"
}

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

using namespace ftxui;
using namespace calcyx::tui;

static int g_failures = 0;

#define EXPECT(label, cond) do {                                              \
    if (!(cond)) {                                                             \
        fprintf(stderr, "FAIL [%s]: %s\n", label, #cond);                     \
        ++g_failures;                                                          \
    }                                                                          \
} while (0)

static void type_str(TuiApp &app, const std::string &str) {
    for (char c : str) {
        app.test_dispatch(Event::Character(std::string(1, c)));
    }
}

static void dump_render(const char *scenario, TuiApp &app) {
    /* Sheet を直接 Render してダンプ — TuiApp 自体は Renderer() ラッパなので
     * 描画入口は sheet->Render()。 プロンプト行は TuiApp::run 内の Renderer
     * でしか生成されないため、このテストでは省略 (状態で確認済み)。 */
    TuiSheet *sheet = app.test_sheet();
    auto screen = Screen::Create(Dimensions{80, 14});
    Element el  = sheet->Render();
    Render(screen, el);
    fprintf(stderr, "\n===== render: %s =====\n%s\n",
            scenario, screen.ToString().c_str());
}

/* ----------------------------------------------------------------------
 * シナリオ 6a: Ctrl+O でプロンプト → Esc でキャンセル
 * -------------------------------------------------------------------- */
static void test_prompt_open_cancel() {
    TuiApp app;

    EXPECT("open: initially not active", !app.test_prompt_active());

    /* Ctrl+O (0x0f) でプロンプト表示 */
    app.test_dispatch(Event::Special("\x0f"));
    EXPECT("open: active after Ctrl+O", app.test_prompt_active());
    EXPECT("open: label is Open",
           app.test_prompt_label().find("Open") != std::string::npos);

    /* タイピングしてみる: "/tmp/foo" */
    type_str(app, "/tmp/foo");
    EXPECT("open: buf reflects typing",
           app.test_prompt_buf() == "/tmp/foo");

    /* Esc でキャンセル */
    app.test_dispatch(Event::Escape);
    EXPECT("open: inactive after Esc", !app.test_prompt_active());
    EXPECT("open: buf cleared",         app.test_prompt_buf().empty());
    EXPECT("open: status = Cancelled",
           app.test_status_msg() == "Cancelled");

    dump_render("6a. prompt open then cancel", app);
}

/* ----------------------------------------------------------------------
 * シナリオ 6b: 式入力 → Ctrl+S → パス入力 → Enter で保存、再 Ctrl+O で読込
 * -------------------------------------------------------------------- */
static void test_prompt_save_and_load() {
    /* 一時ファイルパスを作る */
    char tmpl[] = "/tmp/calcyx-tui-test-XXXXXX";
    int fd = mkstemp(tmpl);
    if (fd >= 0) close(fd);
    std::string path = tmpl;

    /* 1. 初期状態 (1 空行) に "12*34" を打ち、Enter で確定 */
    {
        TuiApp app;
        type_str(app, "12*34");
        app.test_dispatch(Event::Return);
        EXPECT("save: baseline row count",
               sheet_model_row_count(app.test_model()) == 2);

        /* Ctrl+S (0x13) でプロンプト起動。file_path 未設定なので必ずプロンプト */
        app.test_dispatch(Event::Special("\x13"));
        EXPECT("save: prompt active",        app.test_prompt_active());
        EXPECT("save: prompt label is Save",
               app.test_prompt_label().find("Save") != std::string::npos);

        /* パスをタイプして Enter */
        type_str(app, path);
        EXPECT("save: path echoed in buf",  app.test_prompt_buf() == path);
        app.test_dispatch(Event::Return);

        EXPECT("save: prompt closed after Enter", !app.test_prompt_active());
        EXPECT("save: status contains 'Saved'",
               app.test_status_msg().find("Saved") != std::string::npos);
        EXPECT("save: sheet.file_path updated",
               app.test_sheet()->file_path() == path);

        dump_render("6b-save. after save", app);
    }

    /* 2. 新しい TuiApp で同じファイルを Ctrl+O → パス → Enter で読む */
    {
        TuiApp app;
        app.test_dispatch(Event::Special("\x0f"));  /* Ctrl+O */
        EXPECT("load: prompt active", app.test_prompt_active());

        type_str(app, path);
        app.test_dispatch(Event::Return);

        EXPECT("load: prompt closed",  !app.test_prompt_active());
        EXPECT("load: status contains 'Loaded'",
               app.test_status_msg().find("Loaded") != std::string::npos);

        /* 読み込んだ内容: row 0 は "12*34"、結果は "408" */
        const char *expr0 = sheet_model_row_expr(app.test_model(), 0);
        const char *res0  = sheet_model_row_result(app.test_model(), 0);
        EXPECT("load: row 0 expr",   expr0 && strcmp(expr0, "12*34") == 0);
        EXPECT("load: row 0 result", res0  && strcmp(res0,  "408")   == 0);

        dump_render("6b-load. after load", app);
    }

    std::remove(path.c_str());
}

/* ----------------------------------------------------------------------
 * シナリオ 6c: F1 で About を開き、↑↓ でスクロール、Esc / q / Enter で閉じる
 * -------------------------------------------------------------------- */
static void test_about_dialog() {
    TuiApp app;

    EXPECT("about: initially hidden", !app.test_about_visible());

    /* F1 で表示 */
    app.test_dispatch(Event::F1);
    EXPECT("about: visible after F1", app.test_about_visible());
    EXPECT("about: scroll resets to 0", app.test_about_scroll() == 0);

    /* ↓ で 1 行スクロール */
    app.test_dispatch(Event::ArrowDown);
    EXPECT("about: scroll advanced",  app.test_about_scroll() == 1);

    /* ↑ で戻る */
    app.test_dispatch(Event::ArrowUp);
    EXPECT("about: scroll back to 0", app.test_about_scroll() == 0);

    /* PageDown でさらに進む */
    app.test_dispatch(Event::PageDown);
    EXPECT("about: PageDown advances 5 lines", app.test_about_scroll() == 5);

    /* About 表示中はシートに入力が伝播しないことを確認 */
    int before = sheet_model_row_count(app.test_model());
    app.test_dispatch(Event::Character("x"));
    EXPECT("about: sheet did not receive input while visible",
           sheet_model_row_count(app.test_model()) == before);

    /* Esc で閉じる */
    app.test_dispatch(Event::Escape);
    EXPECT("about: hidden after Esc", !app.test_about_visible());

    /* 再度 F1 で開いて Enter で閉じる */
    app.test_dispatch(Event::F1);
    EXPECT("about: visible again", app.test_about_visible());
    app.test_dispatch(Event::Return);
    EXPECT("about: hidden after Enter", !app.test_about_visible());

    /* 再度 F1 で開いて q で閉じる */
    app.test_dispatch(Event::F1);
    app.test_dispatch(Event::Character("q"));
    EXPECT("about: hidden after q", !app.test_about_visible());

    dump_render("6c. about dialog closed", app);
}

/* シナリオ 6d: メニューバー基本動作。
 *   Alt+F で File 展開、 → で次、 Esc で閉じる。
 *   Alt+E → 'u' (Undo) で sheet に undo が届く。
 *   Alt+R → Enter (先頭の Auto) で FormatAuto. */
static void test_menu_bar() {
    TuiApp app;

    /* 初期は未展開 */
    EXPECT("menu: inactive at start",
           app.test_menu_active() == MenuId::None);

    /* Alt+F で File が開く */
    app.test_dispatch(Event::Special("\x1b" "f"));
    EXPECT("menu: File opened by Alt+F",
           app.test_menu_active() == MenuId::File);

    /* → で隣メニュー (Edit) へ */
    app.test_dispatch(Event::ArrowRight);
    EXPECT("menu: ArrowRight moves to Edit",
           app.test_menu_active() == MenuId::Edit);

    /* ← で File に戻る */
    app.test_dispatch(Event::ArrowLeft);
    EXPECT("menu: ArrowLeft moves back to File",
           app.test_menu_active() == MenuId::File);

    /* ↓ で項目移動 */
    int before_item = app.test_menu_item();
    app.test_dispatch(Event::ArrowDown);
    EXPECT("menu: ArrowDown moves item",
           app.test_menu_item() != before_item);

    /* Esc で閉じる */
    app.test_dispatch(Event::Escape);
    EXPECT("menu: closed by Esc",
           app.test_menu_active() == MenuId::None);

    /* --- Edit/Undo 呼び出し経路 --- */
    /* "1+2" Enter で行を作る */
    type_str(app, "1+2");
    app.test_dispatch(Event::Return);
    int rows_after_insert = sheet_model_row_count(app.test_model());
    EXPECT("menu: baseline 2 rows",
           rows_after_insert == 2);

    /* Alt+E → 'u' で Undo (Undo のホット文字は "&Undo" の 'u') */
    app.test_dispatch(Event::Special("\x1b" "e"));
    EXPECT("menu: Edit opened", app.test_menu_active() == MenuId::Edit);
    app.test_dispatch(Event::Character("u"));
    /* ホット文字で activate → menu 閉じる */
    EXPECT("menu: closed after hot-letter activate",
           app.test_menu_active() == MenuId::None);
    /* Undo で行数が戻っているはず */
    EXPECT("menu: undo decreased row count",
           sheet_model_row_count(app.test_model()) < rows_after_insert);

    /* --- Format/Auto 呼び出し (Alt+R → Enter) --- */
    app.test_dispatch(Event::Special("\x1b" "r"));
    EXPECT("menu: Format opened by Alt+R",
           app.test_menu_active() == MenuId::Format);
    app.test_dispatch(Event::Return);
    EXPECT("menu: closed after Enter",
           app.test_menu_active() == MenuId::None);

    /* --- File → About 経路 (Alt+F → 'b') --- */
    EXPECT("menu: about hidden", !app.test_about_visible());
    app.test_dispatch(Event::Special("\x1b" "f"));
    EXPECT("menu: File opened", app.test_menu_active() == MenuId::File);
    app.test_dispatch(Event::Character("b"));  /* "A&bout calcyx" の hot letter */
    EXPECT("menu: About opened via File/About", app.test_about_visible());
    app.test_dispatch(Event::Escape);  /* About を閉じる */

    dump_render("6d. menu bar", app);
}

/* シナリオ 6e: マルチライン Ctrl+V で Paste Options モーダル。
 *   m=各行を別行、 c=キャンセル、 s=空白区切りで 1 行結合。
 * モーダル表示中は Sheet にイベントが届かないことも確認。 */
static void test_paste_modal_multiline() {
    clipboard::set_mock_for_test(true);

    /* --- 6e.1: Multi rows --- */
    {
        TuiApp app;
        clipboard::set_mock_buffer("a\nb\nc");
        int rows_before = sheet_model_row_count(app.test_model());
        EXPECT("6e.1: modal hidden", !app.test_paste_modal_visible());

        app.test_dispatch(Event::Special("\x16"));  /* Ctrl+V */
        EXPECT("6e.1: modal visible", app.test_paste_modal_visible());

        /* モーダル中は Sheet にキーが落ちないことを確認 (適当な文字 'x') */
        std::string before_buf = app.test_sheet()->test_editor_buf();
        app.test_dispatch(Event::Character("x"));
        EXPECT("6e.1: editor unchanged while modal open",
               app.test_sheet()->test_editor_buf() == before_buf);

        /* 'm' で各行を別行として挿入 */
        app.test_dispatch(Event::Character("m"));
        EXPECT("6e.1: modal closed after confirm",
               !app.test_paste_modal_visible());
        EXPECT("6e.1: rows increased",
               sheet_model_row_count(app.test_model()) == rows_before + 3);
    }

    /* --- 6e.2: Cancel --- */
    {
        TuiApp app;
        clipboard::set_mock_buffer("x\ny");
        int rows_before = sheet_model_row_count(app.test_model());

        app.test_dispatch(Event::Special("\x16"));
        EXPECT("6e.2: modal visible", app.test_paste_modal_visible());

        app.test_dispatch(Event::Escape);
        EXPECT("6e.2: cancelled", !app.test_paste_modal_visible());
        EXPECT("6e.2: rows unchanged",
               sheet_model_row_count(app.test_model()) == rows_before);
    }

    /* --- 6e.3: Single line (改行を空白に変換) --- */
    {
        TuiApp app;
        clipboard::set_mock_buffer("1+\n2");
        app.test_dispatch(Event::Special("\x16"));
        EXPECT("6e.3: modal visible", app.test_paste_modal_visible());
        app.test_dispatch(Event::Character("s"));
        EXPECT("6e.3: modal closed", !app.test_paste_modal_visible());
        EXPECT("6e.3: editor has joined text",
               app.test_sheet()->test_editor_buf() == "1+ 2");
    }

    clipboard::set_mock_for_test(false);
}

/* シナリオ 6f: 行右クリックのコンテキストメニュー。
 *   ↑↓ で項目移動 (separator スキップ)、 Enter で実行、 Esc で取消。
 * 開いている間は Sheet にキーが届かない。 */
static void test_context_menu() {
    /* --- 6f.1: open / move / cancel --- */
    {
        TuiApp app;
        EXPECT("6f.1: hidden", !app.test_context_menu_visible());

        app.test_open_context_menu(10, 5);
        EXPECT("6f.1: visible after open", app.test_context_menu_visible());

        int initial = app.test_context_menu_item();
        app.test_dispatch(Event::ArrowDown);
        EXPECT("6f.1: item moved",
               app.test_context_menu_item() != initial);

        /* メニュー中は Sheet にキーが届かない */
        std::string buf_before = app.test_sheet()->test_editor_buf();
        app.test_dispatch(Event::Character("z"));
        EXPECT("6f.1: editor unchanged while menu open",
               app.test_sheet()->test_editor_buf() == buf_before);

        app.test_dispatch(Event::Escape);
        EXPECT("6f.1: closed after Esc",
               !app.test_context_menu_visible());
    }

    /* --- 6f.2: Enter on default item (Copy row) --- */
    {
        TuiApp app;
        clipboard::set_mock_for_test(true);
        clipboard::set_mock_buffer("");
        sheet_model_t *m = app.test_model();
        sheet_model_set_row_expr_raw(m, 0, "1+2");
        sheet_model_eval_all(m);
        app.test_sheet()->reload_focused_row();

        app.test_open_context_menu(0, 0);
        /* Default 項目は最初の非 separator = "Copy row" */
        app.test_dispatch(Event::Return);
        EXPECT("6f.2: closed after Enter",
               !app.test_context_menu_visible());
        EXPECT("6f.2: clipboard contains row text",
               !clipboard::get_mock_buffer().empty());
        clipboard::set_mock_for_test(false);
    }

    /* 6f.3: Insert row below 実行。 kContextMenu 順:
     * 0 CopyRow, 1 CopyExpr, 2 CopyResult, 3 Cut, 4 Paste,
     * 5 Sep, 6 InsertAbove, 7 InsertBelow, 8 DeleteRow.
     * 0→7 は ArrowDown 6 回 (separator 自動スキップ)。 */
    {
        TuiApp app;
        sheet_model_t *m = app.test_model();
        int rows_before = sheet_model_row_count(m);

        app.test_open_context_menu(0, 0);
        for (int i = 0; i < 6; ++i) app.test_dispatch(Event::ArrowDown);
        app.test_dispatch(Event::Return);
        EXPECT("6f.3: closed after Enter",
               !app.test_context_menu_visible());
        EXPECT("6f.3: row inserted below",
               sheet_model_row_count(m) == rows_before + 1);
    }

    /* --- 6f.4: 描画ダンプ (画面状態の目視確認用) --- */
    {
        TuiApp app;
        app.test_open_context_menu(20, 5);
        dump_render("6f.4: context menu open", app);
    }
}

/* ----------------------------------------------------------------------
 * シナリオ 7: Preferences 画面
 * --- 一時 conf dir を共通で用意し、 各テスト先頭で test_set_conf_path する。
 * 端末への \x1B[2J 流出を避けるため tui_clear_after_overlay=false で
 * 初期化済みにする。 -------------------------------------------------- */

static std::string g_prefs_test_dir;

static std::string make_prefs_temp_dir() {
    char tmpl[] = "/tmp/calcyx_test_prefs_XXXXXX";
    char *p = mkdtemp(tmpl);
    return p ? std::string(p) : std::string();
}

static void write_text_file(const std::string &path, const std::string &content) {
    FILE *f = fopen(path.c_str(), "w");
    if (!f) return;
    fputs(content.c_str(), f);
    fclose(f);
}

/* 一時 conf を更地から作る。 既存を消して overlay clear を抑止する設定だけ
 * 入れた最小ファイルを置き、 TuiApp 起動時に schema sync で必要な行が補われる。 */
static void prefs_reset_temp_conf() {
    if (g_prefs_test_dir.empty()) g_prefs_test_dir = make_prefs_temp_dir();
    std::string conf = g_prefs_test_dir + "/calcyx.conf";
    std::string ovr  = conf + ".override";
    /* override は使わないテストでは empty。 */
    write_text_file(conf, "tui_clear_after_overlay = false\n");
    write_text_file(ovr,  "");
    TuiApp::test_set_conf_path(conf);
}

static void test_prefs_open_close() {
    prefs_reset_temp_conf();
    TuiApp app;
    EXPECT("7a.0 hidden", !app.test_prefs_visible());
    app.test_open_prefs();
    EXPECT("7a.1 visible", app.test_prefs_visible());
    EXPECT("7a.2 prefs ptr", app.test_prefs() != nullptr);
    EXPECT("7a.3 default tab", app.test_prefs()->test_tab() == 0);
    app.test_dispatch(Event::Escape);
    EXPECT("7a.4 closed", !app.test_prefs_visible());
}

static void test_prefs_tab_cycle() {
    prefs_reset_temp_conf();
    TuiApp app;
    app.test_open_prefs();
    auto *prefs = app.test_prefs();
    EXPECT("7b.0", prefs->test_tab() == 0);
    app.test_dispatch(Event::Tab);
    EXPECT("7b.1 → Number", prefs->test_tab() == 1);
    app.test_dispatch(Event::Tab);
    EXPECT("7b.2 → Input", prefs->test_tab() == 2);
    app.test_dispatch(Event::Tab);
    EXPECT("7b.3 → Colors", prefs->test_tab() == 3);
    app.test_dispatch(Event::Tab);
    EXPECT("7b.4 wrap → General", prefs->test_tab() == 0);
    app.test_dispatch(Event::TabReverse);
    EXPECT("7b.5 ← Colors", prefs->test_tab() == 3);
    app.test_dispatch(Event::Escape);
}

static void test_prefs_bool_toggle() {
    prefs_reset_temp_conf();
    TuiApp app;
    app.test_open_prefs();
    auto *prefs = app.test_prefs();
    /* Input タブの先頭 = auto_completion (BOOL, default true)。 */
    app.test_dispatch(Event::Tab);
    app.test_dispatch(Event::Tab);
    EXPECT("7c.0 tab=Input", prefs->test_tab() == 2);
    EXPECT("7c.1 default", prefs->test_value("auto_completion") == "true");
    app.test_dispatch(Event::Character(" "));
    EXPECT("7c.2 toggled", prefs->test_value("auto_completion") == "false");
    EXPECT("7c.3 sheet sync", !app.test_sheet()->auto_complete());
    app.test_dispatch(Event::Escape);
}

static void test_prefs_color_source_visibility() {
    prefs_reset_temp_conf();
    TuiApp app;
    app.test_open_prefs();
    auto *prefs = app.test_prefs();
    /* Tab to Colors */
    app.test_dispatch(Event::Tab);
    app.test_dispatch(Event::Tab);
    app.test_dispatch(Event::Tab);
    EXPECT("7d.0 tab=Colors", prefs->test_tab() == 3);
    EXPECT("7d.1 default semantic",
           prefs->test_value("tui_color_source") == "semantic");
    int semantic_count = prefs->test_visible_count();
    /* tui_color_source は最上段 (item 0)。 → で mirror_gui へ循環。 */
    app.test_dispatch(Event::ArrowRight);
    EXPECT("7d.2 switched",
           prefs->test_value("tui_color_source") == "mirror_gui");
    int mirror_count = prefs->test_visible_count();
    /* mirror_gui は color_* 28 項目 + preset で semantic 8 項目より多い。 */
    EXPECT("7d.3 mirror has more items", mirror_count > semantic_count);
    app.test_dispatch(Event::Escape);
}

static void test_prefs_reset_all() {
    prefs_reset_temp_conf();
    TuiApp app;
    app.test_open_prefs();
    auto *prefs = app.test_prefs();
    /* Number-Format タブの decimal_digits を ← で 9 → 8 に変える。 */
    app.test_dispatch(Event::Tab);
    EXPECT("7e.0 tab=Number", prefs->test_tab() == 1);
    EXPECT("7e.1 default", prefs->test_value("decimal_digits") == "9");
    app.test_dispatch(Event::ArrowLeft);
    EXPECT("7e.2 decremented", prefs->test_value("decimal_digits") == "8");

    /* General タブに戻って Reset 行 (= item 6) へ ↓ 6 回。 */
    app.test_dispatch(Event::TabReverse);
    EXPECT("7e.3 tab=General", prefs->test_tab() == 0);
    for (int i = 0; i < 6; i++) app.test_dispatch(Event::ArrowDown);
    EXPECT("7e.4 at reset row", prefs->test_item() == 6);

    /* Enter で confirm モード突入 → Y で実行。 */
    app.test_dispatch(Event::Return);
    EXPECT("7e.5 confirming", prefs->test_confirming_reset());
    app.test_dispatch(Event::Character("y"));
    EXPECT("7e.6 left confirm", !prefs->test_confirming_reset());
    EXPECT("7e.7 reset to default",
           prefs->test_value("decimal_digits") == "9");
    app.test_dispatch(Event::Escape);
}

static void test_prefs_override_locked() {
    prefs_reset_temp_conf();
    /* override に max_array_length=999 を強制書き込み。 */
    std::string conf = g_prefs_test_dir + "/calcyx.conf";
    write_text_file(conf + ".override", "max_array_length = 999\n");
    TuiApp app;
    app.test_open_prefs();
    auto *prefs = app.test_prefs();

    EXPECT("7f.0 locked",  prefs->test_locked("max_array_length"));
    EXPECT("7f.1 override value",
           prefs->test_value("max_array_length") == "999");

    /* ↓ 1 回で max_array_length 行 (item 1)、 Enter で edit 試行。 */
    app.test_dispatch(Event::ArrowDown);
    EXPECT("7f.2 item=1", prefs->test_item() == 1);
    app.test_dispatch(Event::Return);
    EXPECT("7f.3 still 999",
           prefs->test_value("max_array_length") == "999");
    EXPECT("7f.4 not editing", !prefs->test_editing());
    /* flash に "Locked" を含むメッセージが出ているはず (i18n_init=en)。 */
    EXPECT("7f.5 flash",
           app.test_status_msg().find("Locked") != std::string::npos);
    app.test_dispatch(Event::Escape);
}

int main() {
    /* テストは英語前提のアサートなので OS ロケールに関係なく en で固定。 */
    calcyx_i18n_init("en");

    test_prompt_open_cancel();
    test_prompt_save_and_load();
    test_about_dialog();
    test_menu_bar();
    test_paste_modal_multiline();
    test_context_menu();

    test_prefs_open_close();
    test_prefs_tab_cycle();
    test_prefs_bool_toggle();
    test_prefs_color_source_visibility();
    test_prefs_reset_all();
    test_prefs_override_locked();
    /* テスト終わりに conf path フックを戻す (= 後続テストで OS 既定に戻る)。 */
    TuiApp::test_set_conf_path("");

    if (g_failures > 0) {
        fprintf(stderr, "\n%d failure(s)\n", g_failures);
        return 1;
    }
    fprintf(stderr, "\nall TuiApp scenarios passed\n");
    return 0;
}
