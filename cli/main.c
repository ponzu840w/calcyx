/* calcyx CLI
 *
 * Usage: calcyx [options] [file...]
 *
 * Options:
 *   -e <expr>           式を直接指定して評価（複数指定可）
 *   -o, --output <fmt>  出力形式: result (デフォルト) | both
 *   -h, --help          ヘルプを表示
 *
 * コメント: 行中の ; 以降を無視（文字列・文字リテラル内を除く）
 *
 * 終了コード:
 *   0  正常
 *   1  評価エラーあり
 *   2  引数エラー / ファイルオープン失敗
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifdef _WIN32
#  include <io.h>
#  include <windows.h>
#  define isatty _isatty
#  define fileno _fileno
#else
#  include <unistd.h>
#endif

#include "eval/eval.h"
#include "eval/eval_ctx.h"
#include "types/val.h"

#define MAX_LINE 4096
#define MAX_ARGS 256

typedef enum { OUT_RESULT, OUT_BOTH } output_mode_t;

/* 行末の空白・改行を削除 */
static void trim_right(char *s) {
    int len = (int)strlen(s);
    while (len > 0 && ((unsigned char)s[len-1] <= ' '))
        s[--len] = '\0';
}

/* 1 行を評価して出力。
 * 空行・コメントのみの行は何もせず false を返す。
 * エラー時は stderr に出力し *exit_code を 1 に設定して true を返す。 */
static bool eval_line(const char *raw, eval_ctx_t *ctx,
                      output_mode_t out, int *exit_code) {
    char line[MAX_LINE];
    strncpy(line, raw, sizeof(line) - 1);
    line[sizeof(line) - 1] = '\0';

    eval_strip_comment(line);
    trim_right(line);
    if (line[0] == '\0') return false;

    char errmsg[512] = "";
    val_t *result = eval_str(line, ctx, errmsg, sizeof(errmsg));

    if (!result) {
        const char *msg = (ctx->has_error && ctx->error_msg[0])
                          ? ctx->error_msg : errmsg;
        fprintf(stderr, "error: %s\n", msg[0] ? msg : "unknown error");
        ctx->has_error   = false;
        ctx->error_msg[0] = '\0';
        if (exit_code) *exit_code = 1;
        return true;
    }

    char result_str[1024] = "";
    val_to_display_str(result, result_str, sizeof(result_str));
    val_free(result);

    if (out == OUT_BOTH)
        printf("%s = %s\n", line, result_str);
    else
        printf("%s\n", result_str);

    return true;
}

/* ---- REPL ---- */
static void run_repl(eval_ctx_t *ctx) {
    char line[MAX_LINE];
    while (1) {
        fputs("> ", stdout);
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;

        char tmp[MAX_LINE];
        strncpy(tmp, line, sizeof(tmp) - 1);
        eval_strip_comment(tmp);
        trim_right(tmp);
        if (tmp[0] == '\0') continue;

        char errmsg[512] = "";
        val_t *result = eval_str(tmp, ctx, errmsg, sizeof(errmsg));
        if (!result) {
            const char *msg = (ctx->has_error && ctx->error_msg[0])
                              ? ctx->error_msg : errmsg;
            fprintf(stderr, "error: %s\n", msg[0] ? msg : "unknown error");
            ctx->has_error   = false;
            ctx->error_msg[0] = '\0';
        } else {
            char buf[1024];
            val_to_display_str(result, buf, sizeof(buf));
            puts(buf);
            val_free(result);
        }
    }
    putchar('\n');
}

/* ---- ストリーム評価 ---- */
static int run_stream(FILE *fp, eval_ctx_t *ctx, output_mode_t out) {
    char line[MAX_LINE];
    int exit_code = 0;
    while (fgets(line, sizeof(line), fp))
        eval_line(line, ctx, out, &exit_code);
    return exit_code;
}

/* ---- ヘルプ ---- */
static void print_help(const char *prog) {
    fprintf(stderr,
        "calcyx " CALCYX_VERSION_FULL " (" CALCYX_EDITION ")\n"
        "\n"
        "Usage: %s [options] [file...]\n"
        "\n"
        "Options:\n"
        "  -e <expr>          式を直接指定して評価（複数指定可）\n"
        "  -o, --output <fmt> 出力形式:\n"
        "                       result  結果のみ（デフォルト）\n"
        "                       both    式 = 結果\n"
        "  -V, --version      バージョンを表示\n"
        "  -h, --help         このヘルプを表示\n"
        "\n"
        "コメント:\n"
        "  ; 以降を行末コメントとして無視します（文字列・文字リテラル内を除く）\n"
        "\n"
        "Examples:\n"
        "  %s                    対話モード（REPL）\n"
        "  %s file.txt           ファイル評価\n"
        "  %s -e '1+1' -e 'x=42' 式を直接指定\n"
        "  echo 'hex(255)' | %s  パイプ\n"
        "\n"
        "Exit codes: 0=正常, 1=評価エラー, 2=引数/ファイルエラー\n",
        prog, prog, prog, prog, prog);
}

/* ---- main ---- */
int main(int argc, char *argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    output_mode_t out  = OUT_RESULT;
    const char *inline_exprs[MAX_ARGS];
    int         n_inline = 0;
    const char *files[MAX_ARGS];
    int         n_files  = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("calcyx " CALCYX_VERSION_FULL " (" CALCYX_EDITION ")\n");
            return 0;
        } else if (strcmp(argv[i], "-e") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "error: -e requires an argument\n");
                return 2;
            }
            inline_exprs[n_inline++] = argv[++i];
        } else if (strcmp(argv[i], "-o") == 0 ||
                   strcmp(argv[i], "--output") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "error: %s requires an argument\n", argv[i]);
                return 2;
            }
            const char *fmt = argv[++i];
            if      (strcmp(fmt, "both")   == 0) out = OUT_BOTH;
            else if (strcmp(fmt, "result") == 0) out = OUT_RESULT;
            else {
                fprintf(stderr, "error: unknown output format '%s'\n", fmt);
                return 2;
            }
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "error: unknown option '%s'\n", argv[i]);
            return 2;
        } else {
            files[n_files++] = argv[i];
        }
    }

    eval_ctx_t ctx;
    eval_ctx_init(&ctx);
    int exit_code = 0;

    if (n_inline > 0) {
        /* -e モード */
        for (int i = 0; i < n_inline; i++)
            eval_line(inline_exprs[i], &ctx, out, &exit_code);

    } else if (n_files > 0) {
        /* ファイルモード */
        for (int i = 0; i < n_files; i++) {
            FILE *fp = fopen(files[i], "r");
            if (!fp) {
                fprintf(stderr, "error: cannot open '%s'\n", files[i]);
                exit_code = 2;
                continue;
            }
            int r = run_stream(fp, &ctx, out);
            fclose(fp);
            if (r) exit_code = r;
        }
    } else if (isatty(fileno(stdin))) {
        /* 対話モード */
        run_repl(&ctx);
    } else {
        /* パイプ / stdin リダイレクト */
        exit_code = run_stream(stdin, &ctx, out);
    }

    eval_ctx_free(&ctx);
    return exit_code;
}
