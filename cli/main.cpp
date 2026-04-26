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
#include "i18n.h"
#include "settings_io.h"
#include "settings_schema.h"
#include "settings_writer.h"
#include "types/val.h"
}

#include <map>

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
 * --print-config / --check-config
 * -------------------------------------------------------------------- */

namespace {

const char *KNOWN_COLOR_PRESETS[] = {
    "otaku-black", "gyakubari-white", "saboten-grey", "saboten-white",
    "user-defined", nullptr
};

bool is_known_preset(const char *id) {
    for (int i = 0; KNOWN_COLOR_PRESETS[i]; ++i) {
        if (std::strcmp(KNOWN_COLOR_PRESETS[i], id) == 0) return true;
    }
    return false;
}

/* conf を std::map<key, value> で読む (settings_io 経由). */
struct ConfMap {
    std::map<std::string, std::string> kv;
    static void cb(const char *key, const char *value, int /*line_no*/, void *user) {
        auto *self = static_cast<ConfMap *>(user);
        self->kv[key] = value;
    }
};

/* effective preset: conf にあればそれ, なければ schema の s_def. */
std::string effective_preset(const std::map<std::string, std::string> &kv) {
    auto it = kv.find("color_preset");
    if (it != kv.end()) return it->second;
    const calcyx_setting_desc_t *d = calcyx_settings_find("color_preset");
    return d && d->s_def ? d->s_def : "otaku-black";
}

int run_print_config(const char *path) {
    ConfMap cm;
    calcyx_conf_each(path, ConfMap::cb, &cm);   /* 失敗しても空 conf として続行 */
    bool user_colors = (effective_preset(cm.kv) == "user-defined");

    int n = 0;
    const calcyx_setting_desc_t *table = calcyx_settings_table(&n);
    bool first_section = true;
    for (int i = 0; i < n; ++i) {
        const calcyx_setting_desc_t &d = table[i];
        if (d.kind == CALCYX_SETTING_KIND_SECTION) {
            if (!first_section) std::putchar('\n');
            first_section = false;
            if (d.section) std::fputs(d.section, stdout);
            continue;
        }
        if (!d.key) continue;
        auto it = cm.kv.find(d.key);
        const char *raw = (it != cm.kv.end()) ? it->second.c_str() : nullptr;
        switch (d.kind) {
        case CALCYX_SETTING_KIND_BOOL: {
            int v = d.b_def;
            if (raw) (void)calcyx_conf_parse_bool(raw, &v);
            std::printf("%s = %s\n", d.key, v ? "true" : "false");
            break;
        }
        case CALCYX_SETTING_KIND_INT: {
            int v = d.i_def;
            if (raw) (void)calcyx_conf_parse_int(raw, &v);
            if (v < d.i_lo) v = d.i_lo;
            if (v > d.i_hi) v = d.i_hi;
            std::printf("%s = %d\n", d.key, v);
            break;
        }
        case CALCYX_SETTING_KIND_FONT:
        case CALCYX_SETTING_KIND_HOTKEY:
        case CALCYX_SETTING_KIND_COLOR_PRESET:
        case CALCYX_SETTING_KIND_STRING: {
            const char *v = raw ? raw : (d.s_def ? d.s_def : "");
            std::printf("%s = %s\n", d.key, v);
            break;
        }
        case CALCYX_SETTING_KIND_COLOR: {
            /* preset != user-defined のときは color_* を出力しない (settings_save の挙動と一致). */
            if (!user_colors) break;
            const char *v = raw ? raw : "#000000";
            std::printf("%s = %s\n", d.key, v);
            break;
        }
        default:
            break;
        }
    }
    return 0;
}

struct CheckCtx {
    int   warnings;
    /* 同じキーが複数回現れた場合の検出用. キー名 → 最初の行番号. */
    std::map<std::string, int> seen;
};

void check_cb(const char *key, const char *value, int line_no, void *user) {
    auto *cx = static_cast<CheckCtx *>(user);
    {
        auto it = cx->seen.find(key);
        if (it != cx->seen.end()) {
            std::fprintf(stderr,
                "warning: line %d: duplicate key '%s' (first seen on line %d)\n",
                line_no, key, it->second);
            cx->warnings++;
        } else {
            cx->seen[key] = line_no;
        }
    }
    const calcyx_setting_desc_t *d = calcyx_settings_find(key);
    if (!d) {
        std::fprintf(stderr, "warning: line %d: unknown key '%s'\n", line_no, key);
        cx->warnings++;
        return;
    }
    switch (d->kind) {
    case CALCYX_SETTING_KIND_BOOL: {
        int b;
        if (!calcyx_conf_parse_bool(value, &b)) {
            std::fprintf(stderr,
                "warning: line %d: '%s' = '%s': not a bool (expected true/false/1/0/yes/no)\n",
                line_no, key, value);
            cx->warnings++;
        }
        break;
    }
    case CALCYX_SETTING_KIND_INT: {
        int v;
        if (!calcyx_conf_parse_int(value, &v)) {
            std::fprintf(stderr,
                "warning: line %d: '%s' = '%s': not an integer\n",
                line_no, key, value);
            cx->warnings++;
        } else if (v < d->i_lo || v > d->i_hi) {
            std::fprintf(stderr,
                "warning: line %d: '%s' = %d: out of range [%d, %d]\n",
                line_no, key, v, d->i_lo, d->i_hi);
            cx->warnings++;
        }
        break;
    }
    case CALCYX_SETTING_KIND_COLOR: {
        unsigned char rgb[3];
        if (!calcyx_conf_parse_hex_color(value, rgb)) {
            std::fprintf(stderr,
                "warning: line %d: '%s' = '%s': expected '#RRGGBB'\n",
                line_no, key, value);
            cx->warnings++;
        }
        break;
    }
    case CALCYX_SETTING_KIND_COLOR_PRESET: {
        if (!is_known_preset(value)) {
            std::fprintf(stderr,
                "warning: line %d: '%s' = '%s': unknown preset\n",
                line_no, key, value);
            cx->warnings++;
        }
        break;
    }
    case CALCYX_SETTING_KIND_FONT:
    case CALCYX_SETTING_KIND_HOTKEY:
    case CALCYX_SETTING_KIND_STRING:
        if (!*value) {
            std::fprintf(stderr,
                "warning: line %d: '%s' is empty\n", line_no, key);
            cx->warnings++;
        } else if (std::strcmp(key, "tui_color_source") == 0) {
            if (std::strcmp(value, "semantic") != 0 &&
                std::strcmp(value, "mirror_gui") != 0) {
                std::fprintf(stderr,
                    "warning: line %d: '%s' = '%s': expected 'semantic' or 'mirror_gui'\n",
                    line_no, key, value);
                cx->warnings++;
            }
        }
        break;
    default:
        break;
    }
}

int run_init_config(const char *path) {
    int rc = calcyx_settings_init_defaults(path,
        "# calcyx user settings — edit freely.\n");
    if (rc == 1) {
        std::printf("created: %s\n", path);
        return 0;
    }
    if (rc == 0) {
        std::printf("already exists: %s\n", path);
        return 0;
    }
    std::fprintf(stderr, "error: cannot write '%s'\n", path);
    return 2;
}

int run_check_config(const char *path) {
    CheckCtx cx;
    cx.warnings = 0;
    int rc = calcyx_conf_each(path, check_cb, &cx);
    if (rc != 0) {
        std::fprintf(stderr, "error: cannot open '%s'\n", path);
        return 2;
    }
    if (cx.warnings > 0) {
        std::fprintf(stderr, "%d warning(s) in %s\n", cx.warnings, path);
        return 1;
    }
    std::printf("ok: %s\n", path);
    return 0;
}

}  // namespace

/* ----------------------------------------------------------------------
 * ヘルプ
 * -------------------------------------------------------------------- */
static void print_help(const char *prog) {
    std::fprintf(stderr,
        "calcyx " CALCYX_VERSION_FULL " (" CALCYX_EDITION ")\n"
        "\n");
    std::fprintf(stderr, _("Usage: %s [options] [file...]\n"), prog);
    std::fprintf(stderr, "\n%s",
        _("Options:\n"
          "  -e <expr>          Evaluate expression directly (repeatable, CLI mode)\n"
          "  -o, --output <fmt> Output format (CLI mode):\n"
          "                       result  result only (default)\n"
          "                       both    expr = result\n"
          "  -b, --batch        Force CLI batch evaluation of files / stdin\n"
          "  -r, --repl         Launch the legacy CLI REPL (fgets line loop)\n"
          "  --print-config     Print the resolved settings in canonical form\n"
          "  --check-config     Syntax-check the conf; exit 1 if warnings\n"
          "  --init-config      Create conf with defaults if missing (no overwrite)\n"
          "  --config <path>    conf file for --print-config / --check-config / --init-config\n"
          "                       (defaults to platform-specific calcyx.conf)\n"
          "  -V, --version      Show version\n"
          "  -h, --help         Show this help\n"
          "\n"
          "Mode selection:\n"
          "  CLI mode if -e / -o / -b / -r is given.\n"
          "  CLI mode if stdin is not a tty (pipe / redirect).\n"
          "  TUI mode otherwise (interactive terminal with no flags or only file args).\n"
          "  Positional files are loaded into the TUI at startup.\n"
          "\n"
          "Comments:\n"
          "  Anything after `;` to end-of-line is a comment (except inside string/char literals).\n"
          "\n"
          "Examples:\n"));
    std::fprintf(stderr,
        "  %s                    %s\n"
        "  %s file.txt           %s\n"
        "  %s -e '1+1' -e 'x=42' %s\n"
        "  %s --batch file.txt   %s\n"
        "  echo 'hex(255)' | %s  %s\n",
        prog, _("Launch TUI"),
        prog, _("Open a file in TUI"),
        prog, _("CLI: pass expressions directly"),
        prog, _("CLI: batch-evaluate a file"),
        prog, _("CLI: pipe input"));
    std::fprintf(stderr, "\n%s",
        _("Exit codes: 0=success, 1=eval error, 2=arg/file error\n"));
}

/* ----------------------------------------------------------------------
 * main
 * -------------------------------------------------------------------- */
int main(int argc, char *argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    /* 早期 i18n_init: conf を読む前に OS ロケールで仮決定しておくことで,
     * --help / --version の出力も翻訳される. conf の language キーで明示
     * 指定があれば argv 解析後に再 init する. */
    calcyx_i18n_init("auto");

    output_mode_t            out = OUT_RESULT;
    std::vector<const char*> inline_exprs;
    std::vector<const char*> files;
    bool force_batch = false;
    bool force_repl  = false;
    bool do_print_config = false;
    bool do_check_config = false;
    bool do_init_config  = false;
    const char *config_override = nullptr;

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
        if (std::strcmp(a, "--print-config") == 0) {
            do_print_config = true;
            continue;
        }
        if (std::strcmp(a, "--check-config") == 0) {
            do_check_config = true;
            continue;
        }
        if (std::strcmp(a, "--init-config") == 0) {
            do_init_config = true;
            continue;
        }
        if (std::strcmp(a, "--config") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "error: --config requires an argument\n");
                return 2;
            }
            config_override = argv[++i];
            continue;
        }
        if (a[0] == '-') {
            std::fprintf(stderr, "error: unknown option '%s'\n", a);
            return 2;
        }
        files.push_back(a);
    }

    /* 言語を conf から確定して i18n を初期化. config_override 反映後の
     * パスから language キーを読む. 取れなければ "auto" → OS 検出. */
    {
        char path_buf[1024];
        const char *path = config_override
            ? config_override
            : (calcyx_default_conf_path(path_buf, sizeof(path_buf)) ? path_buf : nullptr);
        std::string lang = "auto";
        if (path) {
            ConfMap cm;
            calcyx_conf_each(path, ConfMap::cb, &cm);
            auto it = cm.kv.find("language");
            if (it != cm.kv.end()) lang = it->second;
        }
        calcyx_i18n_init(lang.c_str());
    }

    /* --print-config / --check-config / --init-config は他フラグと独立。
     * conf 関連だけ実行して exit. */
    if (do_print_config || do_check_config || do_init_config) {
        char path_buf[1024];
        const char *path;
        if (config_override) {
            path = config_override;
        } else {
            if (!calcyx_default_conf_path(path_buf, sizeof(path_buf))) {
                std::fprintf(stderr, "error: cannot determine default conf path\n");
                return 2;
            }
            path = path_buf;
        }
        if (do_init_config)  return run_init_config(path);
        if (do_print_config) return run_print_config(path);
        return run_check_config(path);
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
