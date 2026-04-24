/* tui/test_tui_app.cpp — TuiApp プロンプトモードのシナリオテスト
 *
 * run() は ScreenInteractive::Loop() をブロッキング呼び出しするため、
 * test_dispatch() を使って CatchEvent → prompt_handle_event → sheet OnEvent
 * の経路を直接叩く。Ctrl+O / Ctrl+S でプロンプトが開き、Esc で閉じることを
 * 検証する。 */

#include "TuiApp.h"
#include "TuiSheet.h"

extern "C" {
#include "sheet_model.h"
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
     * 描画入口は sheet->Render(). プロンプト行は TuiApp::run 内の Renderer
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

int main() {
    test_prompt_open_cancel();
    test_prompt_save_and_load();
    test_about_dialog();

    if (g_failures > 0) {
        fprintf(stderr, "\n%d failure(s)\n", g_failures);
        return 1;
    }
    fprintf(stderr, "\nall TuiApp scenarios passed\n");
    return 0;
}
