/* calcyx — 統合バイナリエントリ
 *
 * 単一バイナリで CLI / TUI 両モードを提供する。以下のルールで分岐する:
 *
 *   1. -h / --help / -V / --version           → 情報表示して終了
 *   2. -r / --repl                             → 旧 CLI REPL (fgets ループ)
 *   3. -e EXPR があれば                        → CLI -e モード
 *   4. stdin が TTY でない (pipe / リダイレクト) →
 *        ファイルが指定されていれば CLI ファイルモード
 *        なければ CLI ストリームモード (stdin を評価)
 *   5. -b / --batch                            → CLI ファイル/ストリームモード (強制)
 *   6. それ以外 (TTY 上で引数なし or 位置引数のみ) → TUI モード
 *        位置引数のファイルがあれば TUI 起動時にロード
 *
 * Usage:
 *   calcyx                    TUI を起動
 *   calcyx file.txt           TUI でファイルを開く
 *   calcyx -e '1+1'           CLI 一発評価
 *   calcyx -o both file.txt   CLI バッチ (出力書式指定)
 *   calcyx --batch file.txt   CLI バッチ (強制)
 *   calcyx --repl             旧 CLI REPL
 *   echo 'hex(255)' | calcyx  CLI ストリーム (パイプ)
 *
 * 終了コード:
 *   0  正常
 *   1  評価エラーあり
 *   2  引数エラー / ファイルオープン失敗
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#  include <io.h>
#  include <windows.h>
#  define isatty _isatty
#  define fileno _fileno
#else
#  include <unistd.h>
#endif

extern "C" {
#include "eval/eval.h"
#include "eval/eval_ctx.h"
#include "types/val.h"
}

#include "TuiApp.h"

#define MAX_LINE 4096

typedef enum { OUT_RESULT, OUT_BOTH } output_mode_t;

/* ----------------------------------------------------------------------
 * CLI 共通ヘルパ
 * -------------------------------------------------------------------- */
static void trim_right(char *s) {
    int len = (int)std::strlen(s);
    while (len > 0 && ((unsigned char)s[len-1] <= ' '))
        s[--len] = '\0';
}

/* 1 行を評価して出力。
 * 空行・コメントのみの行は何もせず false を返す。
 * エラー時は stderr に出力し *exit_code を 1 に設定して true を返す。 */
static bool eval_line(const char *raw, eval_ctx_t *ctx,
                      output_mode_t out, int *exit_code) {
    char line[MAX_LINE];
    std::strncpy(line, raw, sizeof(line) - 1);
    line[sizeof(line) - 1] = '\0';

    eval_strip_comment(line);
    trim_right(line);
    if (line[0] == '\0') return false;

    char errmsg[512] = "";
    val_t *result = eval_str(line, ctx, errmsg, sizeof(errmsg));

    if (!result) {
        const char *msg = (ctx->has_error && ctx->error_msg[0])
                          ? ctx->error_msg : errmsg;
        std::fprintf(stderr, "error: %s\n", msg[0] ? msg : "unknown error");
        ctx->has_error    = false;
        ctx->error_msg[0] = '\0';
        if (exit_code) *exit_code = 1;
        return true;
    }

    char result_str[1024] = "";
    val_to_display_str(result, result_str, sizeof(result_str));
    val_free(result);

    if (out == OUT_BOTH)
        std::printf("%s = %s\n", line, result_str);
    else
        std::printf("%s\n", result_str);

    return true;
}

/* ----------------------------------------------------------------------
 * REPL / ストリーム
 * -------------------------------------------------------------------- */
static void run_repl(eval_ctx_t *ctx) {
    char line[MAX_LINE];
    while (1) {
        std::fputs("> ", stdout);
        std::fflush(stdout);
        if (!std::fgets(line, sizeof(line), stdin)) break;

        char tmp[MAX_LINE];
        std::strncpy(tmp, line, sizeof(tmp) - 1);
        eval_strip_comment(tmp);
        trim_right(tmp);
        if (tmp[0] == '\0') continue;

        char errmsg[512] = "";
        val_t *result = eval_str(tmp, ctx, errmsg, sizeof(errmsg));
        if (!result) {
            const char *msg = (ctx->has_error && ctx->error_msg[0])
                              ? ctx->error_msg : errmsg;
            std::fprintf(stderr, "error: %s\n", msg[0] ? msg : "unknown error");
            ctx->has_error    = false;
            ctx->error_msg[0] = '\0';
        } else {
            char buf[1024];
            val_to_display_str(result, buf, sizeof(buf));
            std::puts(buf);
            val_free(result);
        }
    }
    std::putchar('\n');
}

static int run_stream(FILE *fp, eval_ctx_t *ctx, output_mode_t out) {
    char line[MAX_LINE];
    int exit_code = 0;
    while (std::fgets(line, sizeof(line), fp))
        eval_line(line, ctx, out, &exit_code);
    return exit_code;
}

/* ----------------------------------------------------------------------
 * ヘルプ
 * -------------------------------------------------------------------- */
static void print_help(const char *prog) {
    std::fprintf(stderr,
        "calcyx " CALCYX_VERSION_FULL " (" CALCYX_EDITION ")\n"
        "\n"
        "Usage: %s [options] [file...]\n"
        "\n"
        "Options:\n"
        "  -e <expr>          式を直接指定して評価（複数指定可、CLI モード）\n"
        "  -o, --output <fmt> 出力形式 (CLI モード):\n"
        "                       result  結果のみ（デフォルト）\n"
        "                       both    式 = 結果\n"
        "  -b, --batch        位置引数ファイル or stdin を CLI バッチ評価する (強制)\n"
        "  -r, --repl         旧 CLI 対話 REPL (fgets 行ループ) を起動する\n"
        "  -V, --version      バージョンを表示\n"
        "  -h, --help         このヘルプを表示\n"
        "\n"
        "モード判定:\n"
        "  -e / -o / -b / -r があれば CLI モード。\n"
        "  stdin が tty でなければ (パイプ / リダイレクト) CLI モード。\n"
        "  それ以外 (対話端末で引数なし or 位置引数ファイルのみ) は TUI モード。\n"
        "  位置引数のファイルは TUI 起動時にロードされる。\n"
        "\n"
        "コメント:\n"
        "  ; 以降を行末コメントとして無視します（文字列・文字リテラル内を除く）\n"
        "\n"
        "Examples:\n"
        "  %s                    TUI を起動\n"
        "  %s file.txt           TUI でファイルを開く\n"
        "  %s -e '1+1' -e 'x=42' CLI: 式を直接指定\n"
        "  %s --batch file.txt   CLI: ファイルをバッチ評価\n"
        "  echo 'hex(255)' | %s  CLI: パイプ\n"
        "\n"
        "Exit codes: 0=正常, 1=評価エラー, 2=引数/ファイルエラー\n",
        prog, prog, prog, prog, prog, prog);
}

/* ----------------------------------------------------------------------
 * main
 * -------------------------------------------------------------------- */
int main(int argc, char *argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    output_mode_t            out = OUT_RESULT;
    std::vector<const char*> inline_exprs;
    std::vector<const char*> files;
    bool force_batch = false;
    bool force_repl  = false;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (std::strcmp(a, "-h") == 0 || std::strcmp(a, "--help") == 0) {
            print_help(argv[0]);
            return 0;
        }
        if (std::strcmp(a, "-V") == 0 || std::strcmp(a, "--version") == 0) {
            std::printf("calcyx " CALCYX_VERSION_FULL " (" CALCYX_EDITION ")\n");
            return 0;
        }
        if (std::strcmp(a, "-e") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "error: -e requires an argument\n");
                return 2;
            }
            inline_exprs.push_back(argv[++i]);
            continue;
        }
        if (std::strcmp(a, "-o") == 0 || std::strcmp(a, "--output") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "error: %s requires an argument\n", a);
                return 2;
            }
            const char *fmt = argv[++i];
            if      (std::strcmp(fmt, "both")   == 0) out = OUT_BOTH;
            else if (std::strcmp(fmt, "result") == 0) out = OUT_RESULT;
            else {
                std::fprintf(stderr, "error: unknown output format '%s'\n", fmt);
                return 2;
            }
            continue;
        }
        if (std::strcmp(a, "-b") == 0 || std::strcmp(a, "--batch") == 0) {
            force_batch = true;
            continue;
        }
        if (std::strcmp(a, "-r") == 0 || std::strcmp(a, "--repl") == 0) {
            force_repl = true;
            continue;
        }
        if (a[0] == '-') {
            std::fprintf(stderr, "error: unknown option '%s'\n", a);
            return 2;
        }
        files.push_back(a);
    }

    /* 明示的 CLI モードと TUI モードの判定。
     * !isatty(stdin) の場合は tty でないのでそもそも TUI 起動できない
     * (FTXUI の ScreenInteractive が非対話端末で動かない) → CLI に落とす。 */
    bool stdin_is_tty = (bool)isatty(fileno(stdin));
    bool is_cli_mode  = force_repl || force_batch || !inline_exprs.empty()
                        || (out == OUT_BOTH) || !stdin_is_tty;

    if (!is_cli_mode) {
        /* TUI モード */
        std::string file_path = files.empty() ? std::string() : files[0];
        if (files.size() > 1) {
            std::fprintf(stderr,
                "warning: TUI モードは最初の 1 ファイルのみ読み込みます\n");
        }
        calcyx::tui::TuiApp app;
        return app.run(file_path);
    }

    /* CLI モード */
    eval_ctx_t ctx;
    eval_ctx_init(&ctx);
    int exit_code = 0;

    if (force_repl) {
        run_repl(&ctx);
    } else if (!inline_exprs.empty()) {
        for (const char *e : inline_exprs)
            eval_line(e, &ctx, out, &exit_code);
    } else if (!files.empty()) {
        for (const char *f : files) {
            FILE *fp = std::fopen(f, "r");
            if (!fp) {
                std::fprintf(stderr, "error: cannot open '%s'\n", f);
                exit_code = 2;
                continue;
            }
            int r = run_stream(fp, &ctx, out);
            std::fclose(fp);
            if (r) exit_code = r;
        }
    } else {
        /* pipe / リダイレクト from stdin */
        exit_code = run_stream(stdin, &ctx, out);
    }

    eval_ctx_free(&ctx);
    return exit_code;
}
