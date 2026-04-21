/* 移植元: Calctus/Model/Functions/BuiltIns/ArrayFuncs.cs,
 *          Sum_AverageFuncs.cs, PrimeNumberFuncs.cs, SolveFuncs.cs,
 *          StringFuncs.cs, Absolute_SignFuncs.cs (mag)
 *          Calctus/Model/Mathematics/NewtonsMethod.cs, RMath.cs
 *
 * 関数本体は builtin_array_{ops,stats,string,bits,color,encode}.c に
 * 分割されている。本ファイルは共通ヘルパー (bia_*) と EXTRA_TABLE +
 * dispatcher (builtin_find_extra / builtin_register_extra /
 * builtin_enum_extra) のみ保持する。 */

#include "builtin_array_internal.h"

/* ======================================================
 * 文字列コピーヘルパー (strncpy + 確実な NUL 終端)
 * ====================================================== */

void bia_str_copy(char *dst, const char *src, size_t size) {
    strncpy(dst, src, size - 1);
    dst[size - 1] = '\0';
}

/* ======================================================
 * ヘルパー: 関数値を引数付きで呼び出す
 * ====================================================== */

/* パラメータを子コンテキストに直接作成するヘルパー (親変数を上書きしない) */
void bia_bind_param(eval_ctx_t *child, const char *pname, val_t *val) {
    if (!pname || !pname[0] || child->n_vars >= EVAL_VAR_MAX) { val_free(val); return; }
    eval_var_t *nv = &child->vars[child->n_vars++];
    memset(nv, 0, sizeof(*nv));
    bia_str_copy(nv->name, pname, TOK_TEXT_MAX);
    nv->value    = val;
    nv->readonly = false;
}

val_t *bia_call_fd_1(func_def_t *fd, val_t *arg, eval_ctx_t *ctx) {
    val_t *args[1] = { arg };
    if (fd->builtin) return fd->builtin(args, 1, ctx);
    if (!fd->body || !ctx) return NULL;
    if (ctx->depth >= ctx->settings.max_call_depth) return NULL;
    eval_ctx_t child;
    eval_ctx_init_child(&child, ctx);
    if (fd->n_params >= 1 && fd->param_names && fd->param_names[0])
        bia_bind_param(&child, fd->param_names[0], val_dup(arg));
    val_t *r = expr_eval((expr_t *)fd->body, &child);
    if (child.has_error && !ctx->has_error) {
        ctx->has_error = child.has_error;
        ctx->error_pos = child.error_pos;
        memcpy(ctx->error_msg, child.error_msg, sizeof(ctx->error_msg));
    }
    eval_ctx_free(&child);
    return r;
}

val_t *bia_call_fd_2(func_def_t *fd, val_t *a0, val_t *a1, eval_ctx_t *ctx) {
    val_t *args[2] = { a0, a1 };
    if (fd->builtin) return fd->builtin(args, 2, ctx);
    if (!fd->body || !ctx) return NULL;
    if (ctx->depth >= ctx->settings.max_call_depth) return NULL;
    eval_ctx_t child;
    eval_ctx_init_child(&child, ctx);
    if (fd->n_params >= 1 && fd->param_names && fd->param_names[0])
        bia_bind_param(&child, fd->param_names[0], val_dup(a0));
    if (fd->n_params >= 2 && fd->param_names && fd->param_names[1])
        bia_bind_param(&child, fd->param_names[1], val_dup(a1));
    val_t *r = expr_eval((expr_t *)fd->body, &child);
    if (child.has_error && !ctx->has_error) {
        ctx->has_error = child.has_error;
        ctx->error_pos = child.error_pos;
        memcpy(ctx->error_msg, child.error_msg, sizeof(ctx->error_msg));
    }
    eval_ctx_free(&child);
    return r;
}

/* a[n] から func_def_t を取り出す */
func_def_t *bia_get_fd(val_t *v) {
    if (v && v->type == VAL_FUNC) return v->func_v;
    return NULL;
}

/* ======================================================
 * 追加の builtin_find / builtin_register_all 対応テーブル
 * builtin.c の BUILTIN_TABLE を拡張するため別のテーブルを用意し、
 * builtin_find_extra / builtin_register_extra として公開する
 * ====================================================== */

typedef struct {
    const char   *name;
    int           n_params;   /* -1 = variadic */
    val_t       *(*fn)(val_t **args, int n, void *ctx);
    int           vec_arg_idx; /* >=0: この引数インデックスで配列ブロードキャスト; -1: なし */
} bi_entry_t;

static const bi_entry_t EXTRA_TABLE[] = {
    /* 配列 */
    { "mag",          -1, bi_mag,           -1 },
    { "len",           1, bi_len,           -1 },
    { "range",         2, bi_range2,        -1 },
    { "range",         3, bi_range3,        -1 },
    { "rangeInclusive",2, bi_rangeIncl2,    -1 },
    { "rangeInclusive",3, bi_rangeIncl3,    -1 },
    { "concat",        2, bi_concat,        -1 },
    { "reverseArray",  1, bi_reverseArray,  -1 },
    { "map",           2, bi_map,           -1 },
    { "filter",        2, bi_filter,        -1 },
    { "count",         2, bi_count_fn,      -1 },
    { "sort",          1, bi_sort1,         -1 },
    { "sort",          2, bi_sort2,         -1 },
    { "aggregate",     2, bi_aggregate,     -1 },
    { "extend",        3, bi_extend,        -1 },
    { "indexOf",       2, bi_indexOf_arr,   -1 },
    { "lastIndexOf",   2, bi_lastIndexOf_arr, -1 },
    { "contains",      2, bi_contains_arr,  -1 },
    { "except",        2, bi_except,        -1 },
    { "intersect",     2, bi_intersect,     -1 },
    { "union",         2, bi_union_arr,     -1 },
    { "unique",        1, bi_unique1,       -1 },
    { "unique",        2, bi_unique2,       -1 },
    { "all",           2, bi_all2,          -1 },
    { "any",           2, bi_any2,          -1 },
    /* 統計 */
    { "sum",          -1, bi_sum,           -1 },
    { "ave",          -1, bi_ave,           -1 },
    { "geoMean",      -1, bi_geoMean,       -1 },
    { "harMean",      -1, bi_harMean,       -1 },
    { "invSum",       -1, bi_invSum,        -1 },
    /* 素数: isPrime/prime は第0引数でブロードキャスト */
    { "isPrime",       1, bi_isPrime,        0 },
    { "prime",         1, bi_prime,          0 },
    { "primeFact",     1, bi_primeFact,     -1 },
    /* solve */
    { "solve",         1, bi_solve1,        -1 },
    { "solve",         2, bi_solve2,        -1 },
    { "solve",         3, bi_solve3,        -1 },
    /* 文字列 */
    { "str",           1, bi_str,           -1 },
    { "array",         1, bi_array_str,     -1 },
    { "trim",          1, bi_trim,           0 },
    { "trimStart",     1, bi_trimStart,      0 },
    { "trimEnd",       1, bi_trimEnd,        0 },
    { "replace",       3, bi_replace,       -1 },
    { "toLower",       1, bi_toLower,        0 },
    { "toUpper",       1, bi_toUpper,        0 },
    { "startsWith",    2, bi_startsWith,    -1 },
    { "endsWith",      2, bi_endsWith,      -1 },
    { "split",         2, bi_split,         -1 },
    { "join",          2, bi_join,          -1 },
    /* GrayCode */
    { "toGray",        1, bi_toGray,         0 },
    { "fromGray",      1, bi_fromGray,       0 },
    /* BitByteOps */
    { "count1",        1, bi_count1,         0 },
    { "pack",         -1, bi_pack,          -1 },
    { "reverseBits",   2, bi_reverseBits,   -1 },
    { "reverseBytes",  2, bi_reverseBytes,  -1 },
    { "rotateL",       2, bi_rotateL,       -1 },
    { "rotateL",       3, bi_rotateL,       -1 },
    { "rotateR",       2, bi_rotateR,       -1 },
    { "rotateR",       3, bi_rotateR,       -1 },
    { "swap2",         1, bi_swap2,          0 },
    { "swap4",         1, bi_swap4,          0 },
    { "swap8",         1, bi_swap8,          0 },
    { "swapNib",       1, bi_swapNib,        0 },
    { "unpack",        3, bi_unpack,        -1 },
    { "unpack",        2, bi_unpack,        -1 },
    /* Color */
    { "rgb",           3, bi_rgb_3,          -1 },
    { "rgb",           1, bi_rgb_1,          -1 },
    { "hsv2rgb",       3, bi_hsv2rgb,        -1 },
    { "rgb2hsv",       1, bi_rgb2hsv,        -1 },
    { "hsl2rgb",       3, bi_hsl2rgb,        -1 },
    { "rgb2hsl",       1, bi_rgb2hsl,        -1 },
    { "rgb2yuv",       3, bi_rgb2yuv_3,      -1 },
    { "rgb2yuv",       1, bi_rgb2yuv_1,      -1 },
    { "yuv2rgb",       3, bi_yuv2rgb_3,      -1 },
    { "yuv2rgb",       1, bi_yuv2rgb_1,      -1 },
    { "rgbTo565",      1, bi_rgbTo565,       -1 },
    { "rgbFrom565",    1, bi_rgbFrom565,     -1 },
    { "pack565",       3, bi_pack565,        -1 },
    { "unpack565",     1, bi_unpack565,      -1 },
    /* Parity / ECC */
    { "xorReduce",     1, bi_xorReduce,      -1 },
    { "oddParity",     1, bi_oddParity,      -1 },
    { "eccWidth",      1, bi_eccWidth,       -1 },
    { "eccEnc",        2, bi_eccEnc,         -1 },
    { "eccDec",        3, bi_eccDec,         -1 },
    /* Encoding */
    { "utf8Enc",       1, bi_utf8Enc,        -1 },
    { "utf8Dec",       1, bi_utf8Dec,        -1 },
    { "urlEnc",        1, bi_urlEnc,         -1 },
    { "urlDec",        1, bi_urlDec,         -1 },
    { "base64Enc",     1, bi_base64Enc,      -1 },
    { "base64Dec",     1, bi_base64Dec,      -1 },
    { "base64EncBytes",1, bi_base64EncBytes, -1 },
    { "base64DecBytes",1, bi_base64DecBytes, -1 },
    /* E系列 */
    { "esFloor",       2, bi_esFloor,        1 },
    { "esCeil",        2, bi_esCeil,         1 },
    { "esRound",       2, bi_esRound,        1 },
    { "esRatio",       2, bi_esRatio,        1 },
    /* Cast */
    { "rat",           1, bi_rat1,          -1 },
    { "rat",           2, bi_rat2,          -1 },
    { "real",          1, bi_real_fn,       -1 },
    { NULL, 0, NULL, -1 }
};

static func_def_t *make_extra(const bi_entry_t *e) {
    func_def_t *fd = (func_def_t *)calloc(1, sizeof(func_def_t));
    if (!fd) return NULL;
    bia_str_copy(fd->name, e->name, sizeof(fd->name));
    fd->n_params    = e->n_params;
    fd->vec_arg_idx = e->vec_arg_idx;
    fd->variadic    = (e->n_params == -1);
    fd->builtin     = e->fn;
    return fd;
}

void builtin_enum_extra(builtin_enum_cb cb, void *userdata) {
    for (int i = 0; EXTRA_TABLE[i].name; i++) {
        cb(EXTRA_TABLE[i].name, EXTRA_TABLE[i].n_params, userdata);
    }
}

func_def_t *builtin_find_extra(const char *name, int n_args) {
    for (int i = 0; EXTRA_TABLE[i].name; i++) {
        const bi_entry_t *e = &EXTRA_TABLE[i];
        if (strcmp(e->name, name) != 0) continue;
        if (e->n_params != -1 && e->n_params != n_args) continue;
        return make_extra(e);
    }
    return NULL;
}

void builtin_register_extra(eval_ctx_t *ctx) {
    for (int i = 0; EXTRA_TABLE[i].name; i++) {
        const bi_entry_t *e = &EXTRA_TABLE[i];
        /* 同名が既に登録済みなら上書きしない (range は 2/3 引数両方登録) */
        eval_var_t *v = eval_ctx_ref_var(ctx, e->name, true);
        if (!v) continue;
        /* 既に非 NULL なら skip しない: overwrite して最新版にする */
        func_def_t *fd = make_extra(e);
        if (!fd) continue;
        val_free(v->value);
        v->value    = val_new_func(fd);
        v->readonly = false;
    }
}
