/* Emscripten 向けエンジン glue (eval_ctx_t を保持し JS から ccall で呼ぶ)。 */

#include <stdio.h>
#include <string.h>
#include "eval/eval.h"
#include "eval/eval_ctx.h"
#include "types/val.h"

#ifdef __EMSCRIPTEN__
#  include <emscripten.h>
#  define WASM_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#  define WASM_EXPORT
#endif

static eval_ctx_t g_ctx;
static bool       g_initialized = false;

static char g_result[2048];
static char g_error[512];

/* ---- 初期化 / リセット ---- */

WASM_EXPORT
void wasm_init(void) {
    if (!g_initialized) {
        eval_ctx_init(&g_ctx);
        g_initialized = true;
    }
    g_result[0] = '\0';
    g_error[0]  = '\0';
}

/* 全変数をクリアして新規セッション開始 */
WASM_EXPORT
void wasm_reset(void) {
    if (g_initialized) eval_ctx_free(&g_ctx);
    eval_ctx_init(&g_ctx);
    g_initialized = true;
    g_result[0] = '\0';
    g_error[0]  = '\0';
}

/* ---- 1行評価 ----
 * 戻り値: 1=成功、 0=エラー
 * 結果は wasm_get_result() / wasm_get_error() で取得する。 */
WASM_EXPORT
int wasm_eval_line(const char *expr) {
    if (!g_initialized) wasm_init();
    g_result[0] = '\0';
    g_error[0]  = '\0';

    char errmsg[512] = "";
    val_t *result = eval_str(expr, &g_ctx, errmsg, sizeof(errmsg));
    if (!result) {
        const char *msg = (g_ctx.has_error && g_ctx.error_msg[0])
                          ? g_ctx.error_msg : errmsg;
        snprintf(g_error, sizeof(g_error), "%s", msg[0] ? msg : "unknown error");
        g_ctx.has_error   = false;
        g_ctx.error_msg[0] = '\0';
        return 0;
    }
    val_to_display_str(result, g_result, sizeof(g_result));
    val_free(result);
    return 1;
}

WASM_EXPORT
const char *wasm_get_result(void) { return g_result; }

WASM_EXPORT
const char *wasm_get_error(void) { return g_error; }

/* 式の = と右辺 (結果) を表示すべきかどうか。
 * 代入式 / def / lambda → 0 (非表示)、それ以外 → 1 (表示) */
WASM_EXPORT
int wasm_result_visible(const char *expr) {
    return eval_result_visible(expr) ? 1 : 0;
}
