/* 移植元: Calctus/Model/Expressions/ (各 OnEval メソッド),
 *          Calctus/Model/Evaluations/EvalContext.cs */

#include "eval.h"
#include "../parser/parser.h"
#include "../types/real.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

/* ======================================================
 * expr_free で使う body 解放用ラッパー (func_def_t から呼ばれる)
 * ====================================================== */

static void body_free(void *e) { expr_free((expr_t *)e); }
static void *body_dup (const void *e) { return (void *)expr_dup((const expr_t *)e); }

/* ======================================================
 * ユーティリティ
 * ====================================================== */

/* 配列の各要素に単項演算を適用 (UnaryOp.cs の array broadcast) */
static val_t *apply_unary_array(val_t *arr,
        val_t *(*op)(const val_t *)) {
    val_t **res = (val_t **)malloc((size_t)arr->arr_len * sizeof(val_t *));
    if (!res) return NULL;
    for (int i = 0; i < arr->arr_len; i++) {
        res[i] = op(arr->arr_items[i]);
        if (!res[i]) {
            for (int j = 0; j < i; j++) val_free(res[j]);
            free(res);
            return NULL;
        }
    }
    val_t *out = val_new_array(res, arr->arr_len, arr->fmt);
    for (int i = 0; i < arr->arr_len; i++) val_free(res[i]);
    free(res);
    return out;
}

/* 2項演算の scalar 演算 */
static val_t *scalar_binop(op_id_t op, val_t *a, val_t *b, eval_ctx_t *ctx) {
    switch (op) {
        case OP_MUL:       return val_mul (a, b);
        case OP_DIV:
        case OP_IDIV:
        case OP_MOD: {
            real_t rb; val_as_real(&rb, b);
            if (real_is_zero(&rb)) {
                EVAL_ERROR(ctx, 0, "Division by zero.");
                return NULL;
            }
            if (op == OP_DIV)  return val_div (a, b);
            if (op == OP_IDIV) return val_idiv(a, b);
            return val_mod(a, b);
        }
        case OP_ADD:       return val_add (a, b);
        case OP_SUB:       return val_sub (a, b);
        case OP_LSL:       return val_lsl (a, b);
        case OP_LSR:       return val_lsr (a, b);
        case OP_ASL:       return val_asl (a, b);
        case OP_ASR:       return val_asr (a, b);
        case OP_GT:        return val_gt  (a, b);
        case OP_GE:        return val_ge  (a, b);
        case OP_LT:        return val_lt  (a, b);
        case OP_LE:        return val_le  (a, b);
        case OP_EQ:        return val_eq  (a, b);
        case OP_NE:        return val_ne  (a, b);
        case OP_BIT_AND:   return val_bit_and(a, b);
        case OP_BIT_XOR:   return val_bit_xor(a, b);
        case OP_BIT_OR:    return val_bit_or (a, b);
        case OP_LOGIC_AND: return val_logic_and(a, b);
        case OP_LOGIC_OR:  return val_logic_or (a, b);
        case OP_POW: {
            real_t ra, rb, rout;
            real_init(&ra); real_init(&rb); real_init(&rout);
            val_as_real(&ra, a);
            val_as_real(&rb, b);
            real_pow(&rout, &ra, &rb);
            return val_new_real(&rout, a->fmt);
        }
        default:
            return NULL;
    }
}

/* 配列ブロードキャスト共通ループ: a/b の arr_items[i] か scalar を選択して演算 */
static val_t *broadcast_op(op_id_t op, const val_t *a, const val_t *b,
                            int len, val_fmt_t fmt, eval_ctx_t *ctx) {
    val_t **res = (val_t **)malloc((size_t)len * sizeof(val_t *));
    if (!res) return NULL;
    for (int i = 0; i < len; i++) {
        const val_t *ai = (a->type == VAL_ARRAY) ? a->arr_items[i] : a;
        const val_t *bi = (b->type == VAL_ARRAY) ? b->arr_items[i] : b;
        res[i] = scalar_binop(op, (val_t *)ai, (val_t *)bi, ctx);
        if (!res[i] || ctx->has_error) {
            for (int j = 0; j < i; j++) val_free(res[j]);
            free(res);
            return NULL;
        }
    }
    val_t *out = val_new_array(res, len, fmt);
    for (int i = 0; i < len; i++) val_free(res[i]);
    free(res);
    return out;
}

/* 2項演算 (配列ブロードキャスト付き, 移植元: BinaryOp.scalarOperation + 配列分岐) */
static val_t *apply_binop(op_id_t op, val_t *a, val_t *b, eval_ctx_t *ctx) {
    bool a_arr = (a->type == VAL_ARRAY);
    bool b_arr = (b->type == VAL_ARRAY);
    if (a_arr && !b_arr) return broadcast_op(op, a, b, a->arr_len, a->fmt, ctx);
    if (!a_arr && b_arr) return broadcast_op(op, a, b, b->arr_len, b->fmt, ctx);
    if (a_arr && b_arr) {
        if (a->arr_len != b->arr_len) {
            EVAL_ERROR(ctx, 0, "Array size mismatch.");
            return NULL;
        }
        return broadcast_op(op, a, b, a->arr_len, a->fmt, ctx);
    }
    return scalar_binop(op, a, b, ctx);
}

/* ======================================================
 * 範囲配列 (BinaryOp: OP_EXCL_RANGE / OP_INCL_RANGE)
 * 移植元: RMath.Range + BinaryOp.OnEval
 * ====================================================== */

static val_t *make_range(val_t *a, val_t *b, bool inclusive) {
    int64_t from = val_as_long(a);
    int64_t to   = val_as_long(b);
    int64_t step = (from <= to) ? 1 : -1;
    int64_t end  = inclusive ? to + step : to;
    int64_t count = (step > 0) ? (end - from) : (from - end);
    if (count < 0) count = 0;
    if (count > 1000000) count = 1000000;  /* 上限 */

    val_t **items = (val_t **)malloc((size_t)count * sizeof(val_t *));
    if (!items) return NULL;
    int64_t v = from;
    for (int64_t i = 0; i < count; i++, v += step)
        items[i] = val_new_i64(v, a->fmt);
    val_t *out = val_new_array(items, (int)count, a->fmt);
    for (int i = 0; i < (int)count; i++) val_free(items[i]);
    free(items);
    return out;
}

/* ======================================================
 * 関数呼び出し実行
 * ====================================================== */

/* func_def_t を使って引数を束縛し、本体を評価する */
static val_t *call_func(func_def_t *fd, val_t **args, int n_args,
                         eval_ctx_t *ctx) {
    /* vec_arg_idx ブロードキャスト: 指定インデックスの引数が配列なら element-wise 呼び出し */
    if (fd->vec_arg_idx >= 0 && fd->vec_arg_idx < n_args
            && args[fd->vec_arg_idx]->type == VAL_ARRAY) {
        val_t *arr = args[fd->vec_arg_idx];
        int len = arr->arr_len;
        val_t **res = (val_t **)malloc((size_t)len * sizeof(val_t *));
        if (!res) return NULL;
        for (int i = 0; i < len; i++) {
            val_t **tmp_args = (val_t **)malloc((size_t)n_args * sizeof(val_t *));
            if (!tmp_args) {
                for (int j = 0; j < i; j++) val_free(res[j]);
                free(res);
                return NULL;
            }
            for (int j = 0; j < n_args; j++) tmp_args[j] = args[j];
            tmp_args[fd->vec_arg_idx] = arr->arr_items[i];
            res[i] = call_func(fd, tmp_args, n_args, ctx);
            free(tmp_args);
            if (!res[i] || ctx->has_error) {
                for (int j = 0; j < i; j++) val_free(res[j]);
                free(res);
                return NULL;
            }
        }
        val_t *out = val_new_array(res, len, arr->fmt);
        for (int i = 0; i < len; i++) val_free(res[i]);
        free(res);
        return out;
    }
    /* 組み込み */
    if (fd->builtin) {
        return fd->builtin(args, n_args, ctx);
    }
    /* ユーザ定義 */
    if (!fd->body) { EVAL_ERROR(ctx, 0, "Function has no body."); return NULL; }
    if (ctx->depth >= EVAL_DEPTH_MAX) {
        EVAL_ERROR(ctx, 0, "Depth of recursion exceeds limit.");
        return NULL;
    }
    eval_ctx_t child;
    eval_ctx_init_child(&child, ctx);
    int bind_n = (fd->n_params == -1) ? n_args : fd->n_params;
    for (int i = 0; i < bind_n && i < n_args; i++) {
        const char *pname = (fd->param_names && fd->param_names[i])
                            ? fd->param_names[i] : "";
        if (pname[0] && child.n_vars < EVAL_VAR_MAX) {
            /* 親コンテキストを検索せず子に直接作成 (同名の外側変数を上書きしない) */
            eval_var_t *nv = &child.vars[child.n_vars++];
            memset(nv, 0, sizeof(*nv));
            strncpy(nv->name, pname, TOK_TEXT_MAX - 1);
            nv->value    = val_dup(args[i]);
            nv->readonly = false;
        }
    }
    val_t *result = expr_eval((expr_t *)fd->body, &child);
    /* 子コンテキストのエラーを親に伝播 */
    if (child.has_error && !ctx->has_error) {
        ctx->has_error  = child.has_error;
        ctx->error_pos  = child.error_pos;
        memcpy(ctx->error_msg, child.error_msg, sizeof(ctx->error_msg));
    }
    eval_ctx_free(&child);
    return result;
}

/* ======================================================
 * ラムダ/def 共通: func_def_t を式ノードから生成
 * ====================================================== */

static func_def_t *make_func_def(const expr_t *e, const char *name) {
    func_def_t *fd = (func_def_t *)calloc(1, sizeof(func_def_t));
    if (!fd) return NULL;
    strncpy(fd->name, name, sizeof(fd->name) - 1);
    if (e->arg_defs) {
        fd->n_params    = e->arg_defs->n;
        fd->vec_arg_idx = e->arg_defs->vec_arg_idx;
        fd->variadic    = e->arg_defs->variadic;
        if (fd->n_params > 0) {
            fd->param_names = (char **)calloc((size_t)fd->n_params, sizeof(char *));
            if (!fd->param_names) { free(fd); return NULL; }
            for (int i = 0; i < fd->n_params; i++)
                fd->param_names[i] = e->arg_defs->names[i]
                                     ? strdup(e->arg_defs->names[i])
                                     : NULL;
        }
    } else {
        fd->n_params    = 0;
        fd->vec_arg_idx = -1;
    }
    fd->body      = (void *)expr_dup(e->body);
    fd->free_body = body_free;
    fd->dup_body  = body_dup;
    return fd;
}

/* ======================================================
 * 代入ヘルパー
 * ====================================================== */

static val_t *eval_assign(const expr_t *lhs, const expr_t *rhs,
                           eval_ctx_t *ctx) {
    val_t *rval = expr_eval(rhs, ctx);
    if (!rval || ctx->has_error) return rval;

    /* 単純変数代入: a = expr */
    if (lhs->type == EXPR_ID) {
        eval_ctx_set_var(ctx, lhs->name, val_dup(rval));
        return rval;
    }

    /* 配列アンパック: [a, b, c] = [1, 2, 3] */
    if (lhs->type == EXPR_ARRAY) {
        if (rval->type != VAL_ARRAY) {
            EVAL_ERROR(ctx, lhs->tok.pos, "Array is required.");
            val_free(rval); return NULL;
        }
        if (lhs->n_args != rval->arr_len) {
            EVAL_ERROR(ctx, lhs->tok.pos, "Array size mismatch.");
            val_free(rval); return NULL;
        }
        for (int i = 0; i < lhs->n_args; i++) {
            if (lhs->args[i]->type != EXPR_ID) {
                EVAL_ERROR(ctx, lhs->args[i]->tok.pos, "Identifier expected.");
                val_free(rval); return NULL;
            }
            eval_ctx_set_var(ctx, lhs->args[i]->name,
                             val_dup(rval->arr_items[i]));
        }
        return rval;
    }

    /* 部分参照代入: a[from:to] = val (移植元: BinaryOp.OnEval Assign PartRef) */
    if (lhs->type == EXPR_PART_REF && lhs->child_a->type == EXPR_ID) {
        eval_var_t *var = eval_ctx_ref_var(ctx, lhs->child_a->name, false);
        if (!var) {
            EVAL_ERROR(ctx, lhs->child_a->tok.pos, "Variable not found: '%.200s'",
                       lhs->child_a->name);
            val_free(rval); return NULL;
        }
        val_t *from_v = expr_eval(lhs->child_b, ctx);
        if (!from_v || ctx->has_error) { val_free(rval); return NULL; }
        int64_t from = val_as_long(from_v);
        val_free(from_v);

        int64_t to = from;
        if (lhs->child_c) {
            val_t *to_v = expr_eval(lhs->child_c, ctx);
            if (!to_v || ctx->has_error) { val_free(rval); return NULL; }
            to = val_as_long(to_v);
            val_free(to_v);
        }

        val_t *target = var->value;

        if (target->type == VAL_STR) {
            /* 文字列スライス置換: s[from:to] = str
             * 両端インクルーシブ（calcyx 独自仕様。配列・ビットフィールドと統一） */
            const char *s = target->str_v;
            int len = (int)strlen(s);
            if (from < 0) from += len;
            if (to   < 0) to   += len;
            int lo = (from < to) ? (int)from : (int)to;
            int hi = (from < to) ? (int)to   : (int)from;
            if (lo < 0 || hi >= len) {
                EVAL_ERROR(ctx, lhs->tok.pos, "Index out of range.");
                val_free(rval); return NULL;
            }
            const char *rep = (rval->type == VAL_STR) ? rval->str_v : "";
            int rep_len = (int)strlen(rep);
            /* prefix: s[0..lo), suffix: s[hi+1..end) */
            int pre = lo;
            int suf_start = hi + 1;
            int suf_len = len - suf_start;
            if (suf_len < 0) suf_len = 0;
            char *nbuf = (char *)malloc((size_t)(pre + rep_len + suf_len + 1));
            if (!nbuf) { val_free(rval); return NULL; }
            memcpy(nbuf, s, (size_t)pre);
            memcpy(nbuf + pre, rep, (size_t)rep_len);
            memcpy(nbuf + pre + rep_len, s + suf_start, (size_t)suf_len);
            nbuf[pre + rep_len + suf_len] = '\0';
            val_t *new_str = val_new_str(nbuf);
            free(nbuf);
            var->value = new_str;
            val_free(target);
        } else if (target->type == VAL_ARRAY) {
            /* 配列要素の書き換え */
            int len = target->arr_len;
            if (from < 0) from += len;
            if (to   < 0) to   += len;
            if (from < 0 || from >= len || to < 0 || to >= len) {
                EVAL_ERROR(ctx, lhs->tok.pos, "Index out of range.");
                val_free(rval); return NULL;
            }
            val_t *new_arr = val_dup(target);
            if (from == to) {
                val_free(new_arr->arr_items[from]);
                new_arr->arr_items[from] = val_dup(rval);
            } else {
                /* 範囲置換 */
                if (rval->type != VAL_ARRAY) {
                    EVAL_ERROR(ctx, lhs->tok.pos, "Array required for range assign.");
                    val_free(new_arr); val_free(rval); return NULL;
                }
                int w = (int)(to - from);
                if (w < 0) w = -w;
                w++;
                if (w != rval->arr_len) {
                    EVAL_ERROR(ctx, lhs->tok.pos, "Array size mismatch for range assign.");
                    val_free(new_arr); val_free(rval); return NULL;
                }
                int lo = (from < to) ? (int)from : (int)to;
                for (int i = 0; i < w; i++) {
                    val_free(new_arr->arr_items[lo + i]);
                    new_arr->arr_items[lo + i] = val_dup(rval->arr_items[i]);
                }
            }
            var->value = new_arr;
            val_free(target);
        } else {
            /* ビットフィールド書き換え (from >= to) */
            if (from < to) {
                EVAL_ERROR(ctx, lhs->tok.pos,
                           "Bit field range: MSB must be >= LSB.");
                val_free(rval); return NULL;
            }
            if (from < 0 || from > 63 || to < 0 || to > 63) {
                EVAL_ERROR(ctx, lhs->tok.pos,
                           "Bit field index out of range (must be 0-63).");
                val_free(rval); return NULL;
            }
            int64_t w = from - to + 1;
            int64_t mask = (w < 64) ? ((1LL << w) - 1LL)
                                    : (int64_t)(-1LL); /* all bits */
            mask <<= to;
            int64_t buf = val_as_long(target);
            buf &= ~mask;
            buf |= (val_as_long(rval) << to) & mask;
            val_t *new_val = val_new_i64(buf, target->fmt);
            var->value = new_val;
            val_free(target);
        }
        return rval;
    }

    EVAL_ERROR(ctx, lhs->tok.pos, "Invalid assignment target.");
    val_free(rval);
    return NULL;
}

/* ======================================================
 * メイン評価関数
 * ====================================================== */

val_t *expr_eval(const expr_t *e, eval_ctx_t *ctx) {
    if (!e || ctx->has_error) return NULL;

    switch (e->type) {

    /* --- リテラル --- */
    case EXPR_NUM_LIT:
    case EXPR_BOOL_LIT:
        return val_dup(e->val);

    /* --- 識別子 (移植元: IdExpr.OnEval) --- */
    case EXPR_ID: {
        eval_var_t *var = eval_ctx_ref_var(ctx, e->name, false);
        if (var) return val_dup(var->value);
        EVAL_ERROR(ctx, e->tok.pos, "Variable '%.200s' not found.", e->name);
        return NULL;
    }

    /* --- 単項演算 (移植元: UnaryOp.OnEval) --- */
    case EXPR_UNARY: {
        val_t *a = expr_eval(e->child_a, ctx);
        if (!a || ctx->has_error) return NULL;
        val_t *result = NULL;
        if (a->type == VAL_ARRAY) {
            switch (e->op) {
                case OP_ARITH_INV: result = apply_unary_array(a, val_neg);      break;
                case OP_BIT_NOT:   result = apply_unary_array(a, val_bit_not);  break;
                case OP_LOGIC_NOT: result = apply_unary_array(a, val_logic_not);break;
                case OP_UNARY_PLUS: result = val_dup(a); break;
                default: break;
            }
        } else {
            switch (e->op) {
                case OP_UNARY_PLUS: result = val_dup(a);         break;
                case OP_ARITH_INV:  result = val_neg(a);         break;
                case OP_BIT_NOT:    result = val_bit_not(a);     break;
                case OP_LOGIC_NOT:  result = val_logic_not(a);   break;
                default: break;
            }
        }
        val_free(a);
        if (!result) EVAL_ERROR(ctx, e->tok.pos, "Unknown unary operator.");
        return result;
    }

    /* --- 二項演算 (移植元: BinaryOp.OnEval) --- */
    case EXPR_BINARY: {
        if (e->op == OP_ASSIGN) {
            return eval_assign(e->child_a, e->child_b, ctx);
        }
        if (e->op == OP_FRAC) {
            /* a $ b → 分数 (移植元: BinaryOp FracVal.Normalize) */
            val_t *a = expr_eval(e->child_a, ctx);
            if (!a || ctx->has_error) return NULL;
            val_t *b = expr_eval(e->child_b, ctx);
            if (!b || ctx->has_error) { val_free(a); return NULL; }
            frac_t f;
            real_t ra, rb;
            val_as_real(&ra, a); val_as_real(&rb, b);
            frac_from_n_d(&f, &ra, &rb);
            val_free(a); val_free(b);
            return val_new_frac(&f);
        }
        if (e->op == OP_EXCL_RANGE || e->op == OP_INCL_RANGE) {
            val_t *a = expr_eval(e->child_a, ctx);
            if (!a || ctx->has_error) return NULL;
            val_t *b = expr_eval(e->child_b, ctx);
            if (!b || ctx->has_error) { val_free(a); return NULL; }
            val_t *r = make_range(a, b, e->op == OP_INCL_RANGE);
            val_free(a); val_free(b);
            return r;
        }
        {
            val_t *a = expr_eval(e->child_a, ctx);
            if (!a || ctx->has_error) return NULL;
            val_t *b = expr_eval(e->child_b, ctx);
            if (!b || ctx->has_error) { val_free(a); return NULL; }
            val_t *r = apply_binop(e->op, a, b, ctx);
            val_free(a); val_free(b);
            if (!r && !ctx->has_error)
                EVAL_ERROR(ctx, e->tok.pos, "Binary operation failed.");
            return r;
        }
    }

    /* --- 条件演算子 ?: (移植元: CondOp.OnEval) --- */
    case EXPR_COND: {
        val_t *cond = expr_eval(e->child_a, ctx);
        if (!cond || ctx->has_error) return NULL;
        bool cv = val_as_bool(cond);
        val_free(cond);
        return expr_eval(cv ? e->child_b : e->child_c, ctx);
    }

    /* --- 配列リテラル (移植元: ArrayExpr.OnEval) --- */
    case EXPR_ARRAY: {
        val_t **items = (val_t **)malloc((size_t)e->n_args * sizeof(val_t *));
        if (!items && e->n_args > 0) return NULL;
        for (int i = 0; i < e->n_args; i++) {
            items[i] = expr_eval(e->args[i], ctx);
            if (!items[i] || ctx->has_error) {
                for (int j = 0; j < i; j++) val_free(items[j]);
                free(items);
                return NULL;
            }
        }
        val_t *out = val_new_array(items, e->n_args, FMT_REAL);
        for (int i = 0; i < e->n_args; i++) val_free(items[i]);
        free(items);
        return out;
    }

    /* --- 関数呼び出し (移植元: CallExpr.OnEval) --- */
    case EXPR_CALL: {
        /* 引数を評価 */
        int n = e->n_args;
        val_t **args = malloc(n * sizeof(val_t *));
        if (!args) { EVAL_ERROR(ctx, e->tok.pos, "Out of memory."); return NULL; }
        for (int i = 0; i < n; i++) {
            args[i] = expr_eval(e->args[i], ctx);
            if (!args[i] || ctx->has_error) {
                for (int j = 0; j < i; j++) val_free(args[j]);
                free(args);
                return NULL;
            }
        }
        /* 変数テーブルで関数値を探す (n_params が一致する場合のみ) */
        eval_var_t *var = eval_ctx_ref_var(ctx, e->name, false);
        if (var && var->value && var->value->type == VAL_FUNC) {
            func_def_t *fd = var->value->func_v;
            if (fd->n_params == -1 || fd->n_params == n) {
                val_t *result = call_func(fd, args, n, ctx);
                for (int i = 0; i < n; i++) val_free(args[i]);
                free(args);
                return result;
            }
        }
        /* 組み込み関数を直接検索 */
        func_def_t *fd = builtin_find(e->name, n);
        if (!fd) fd = builtin_find_extra(e->name, n);
        if (fd) {
            val_t *result = call_func(fd, args, n, ctx);
            func_def_free(fd); free(fd);
            for (int i = 0; i < n; i++) val_free(args[i]);
            free(args);
            return result;
        }
        for (int i = 0; i < n; i++) val_free(args[i]);
        free(args);
        EVAL_ERROR(ctx, e->tok.pos, "Function '%.200s' not found.", e->name);
        return NULL;
    }

    /* --- ラムダ式 (移植元: LambdaExpr.OnEval) --- */
    case EXPR_LAMBDA: {
        func_def_t *fd = make_func_def(e, "<lambda>");
        if (!fd) return NULL;
        return val_new_func(fd);
    }

    /* --- def 宣言 (移植元: DefExpr.OnEval) --- */
    case EXPR_DEF: {
        func_def_t *fd = make_func_def(e, e->name);
        if (!fd) return NULL;
        /* 変数テーブルに FuncVal として登録 */
        eval_ctx_set_var(ctx, e->name, val_new_func(fd));
        return val_new_null();
    }

    /* --- 部分参照 a[i] / a[i:j] (移植元: PartRef.OnEval) --- */
    case EXPR_PART_REF: {
        val_t *from_v = expr_eval(e->child_b, ctx);
        if (!from_v || ctx->has_error) return NULL;
        int64_t from = val_as_long(from_v);
        val_free(from_v);

        int64_t to = from;
        if (e->child_c) {
            val_t *to_v = expr_eval(e->child_c, ctx);
            if (!to_v || ctx->has_error) return NULL;
            to = val_as_long(to_v);
            val_free(to_v);
        }

        val_t *obj = expr_eval(e->child_a, ctx);
        if (!obj || ctx->has_error) return NULL;

        val_t *result = NULL;
        if (obj->type == VAL_ARRAY) {
            int len = obj->arr_len;
            if (from < 0) from += len;
            if (to   < 0) to   += len;
            if (from < 0 || from >= len || to < 0 || to >= len) {
                EVAL_ERROR(ctx, e->tok.pos, "Index out of range.");
                val_free(obj); return NULL;
            }
            if (from == to) {
                result = val_dup(obj->arr_items[(int)from]);
            } else {
                int lo = (from < to) ? (int)from : (int)to;
                int hi = (from < to) ? (int)to   : (int)from;
                int w = hi - lo + 1;
                val_t **items = (val_t **)malloc((size_t)w * sizeof(val_t *));
                if (!items) { val_free(obj); return NULL; }
                for (int i = 0; i < w; i++)
                    items[i] = val_dup(obj->arr_items[lo + i]);
                result = val_new_array(items, w, obj->fmt);
                for (int i = 0; i < w; i++) val_free(items[i]);
                free(items);
            }
        } else if (obj->type == VAL_STR) {
            const char *s = obj->str_v;
            int len = (int)strlen(s);
            if (from < 0) from += len;
            if (to   < 0) to   += len;
            if (!e->child_c) {
                /* 単一インデックス s[i]: UTF-8 コードポイントを数値で返す */
                if (from < 0 || from >= len) {
                    EVAL_ERROR(ctx, e->tok.pos, "Index out of range.");
                    val_free(obj); return NULL;
                }
                unsigned char c = (unsigned char)s[from];
                int64_t code;
                if (c < 0x80) {
                    code = c;
                } else if (c < 0xE0 && from + 1 < len) {
                    code = ((c & 0x1F) << 6) | ((unsigned char)s[from+1] & 0x3F);
                } else if (c < 0xF0 && from + 2 < len) {
                    code = ((c & 0x0F) << 12) | (((unsigned char)s[from+1] & 0x3F) << 6)
                           | ((unsigned char)s[from+2] & 0x3F);
                } else if (from + 3 < len) {
                    code = ((c & 0x07) << 18) | (((unsigned char)s[from+1] & 0x3F) << 12)
                           | (((unsigned char)s[from+2] & 0x3F) << 6)
                           | ((unsigned char)s[from+3] & 0x3F);
                } else {
                    code = c;
                }
                result = val_new_i64(code, FMT_CHAR);
            } else {
                /* スライス s[from:to]: 両端インクルーシブ
                 * Calctus は文字列だけ Substring(from,length) 式（末尾エクスクルーシブ）
                 * だが、calcyx では配列・ビットフィールドと統一し両端インクルーシブとする */
                int lo = (from < to) ? (int)from : (int)to;
                int hi = (from < to) ? (int)to   : (int)from;
                if (lo < 0 || hi >= len) {
                    EVAL_ERROR(ctx, e->tok.pos, "Index out of range.");
                    val_free(obj); return NULL;
                }
                int w = hi - lo + 1;
                char *buf = (char *)malloc((size_t)w + 1);
                if (buf) { memcpy(buf, s + lo, (size_t)w); buf[w] = '\0'; }
                result = buf ? val_new_str(buf) : NULL;
                free(buf);
            }
        } else {
            /* ビットフィールド抽出 (from >= to, MSB:LSB) */
            if (from < to) {
                EVAL_ERROR(ctx, e->tok.pos,
                           "Bit field: MSB index must be >= LSB index.");
                val_free(obj); return NULL;
            }
            if (from < 0 || from > 63 || to < 0 || to > 63) {
                EVAL_ERROR(ctx, e->tok.pos,
                           "Bit field index out of range (must be 0-63).");
                val_free(obj); return NULL;
            }
            int64_t bits = val_as_long(obj);
            bits >>= to;
            int64_t w = from - to + 1;
            if (w < 64) bits &= (1LL << w) - 1LL;
            result = val_new_i64(bits, obj->fmt);
        }
        val_free(obj);
        return result;
    }

    /* --- EXPR_ASTER: 評価時エラー --- */
    case EXPR_ASTER:
        EVAL_ERROR(ctx, e->tok.pos, "Unexpected asterisk.");
        return NULL;
    }

    return NULL;
}

/* ======================================================
 * eval_str: 文字列の解析・評価
 * ====================================================== */

/* ; 以降の行末コメントを除去。文字列リテラル ('...' / "...") 内の ; は除く。
 * エスケープシーケンス (\' / \") も正しく読み飛ばす。 */
void eval_strip_comment(char *buf) {
    int in_str  = 0;  /* " の中 */
    int in_char = 0;  /* ' の中 */
    for (int i = 0; buf[i]; i++) {
        char c = buf[i];
        if (in_str) {
            if (c == '\\')     { i++; continue; }   /* エスケープ読み飛ばし */
            if (c == '"')      { in_str = 0; }
        } else if (in_char) {
            if (c == '\\')     { i++; continue; }
            if (c == '\'')     { in_char = 0; }
        } else {
            if (c == '"')      { in_str  = 1; }
            else if (c == '\''){ in_char = 1; }
            else if (c == ';') { buf[i] = '\0'; break; }
        }
    }
}

val_t *eval_str(const char *src, eval_ctx_t *ctx,
                char *errmsg, int errmsg_len) {
    /* ; コメントを除去したコピーで評価 */
    char stripped[4096];
    strncpy(stripped, src, sizeof(stripped) - 1);
    stripped[sizeof(stripped) - 1] = '\0';
    eval_strip_comment(stripped);
    src = stripped;

    char parse_err[256] = "";
    expr_t *ast = parse(src, parse_err, sizeof(parse_err));
    if (!ast) {
        if (errmsg) snprintf(errmsg, (size_t)errmsg_len, "%s", parse_err);
        return NULL;
    }
    val_t *result = expr_eval(ast, ctx);
    expr_free(ast);
    if (ctx->has_error) {
        if (errmsg)
            snprintf(errmsg, (size_t)errmsg_len,
                     "pos %d: %s", ctx->error_pos, ctx->error_msg);
        val_free(result);
        return NULL;
    }
    /* ans: 直前の評価結果を保持する特殊変数 */
    if (result) {
        val_t *ans_copy = val_dup(result);
        eval_ctx_set_var(ctx, "ans", ans_copy);
    }
    return result;
}

/* ======================================================
 * eval_result_visible: = と右辺を表示するか
 * 移植元: Calctus SheetViewItem ansVisible 判定
 * ====================================================== */

bool eval_result_visible(const char *src) {
    if (!src || !*src) return false;

    char stripped[2048];
    snprintf(stripped, sizeof(stripped), "%s", src);
    eval_strip_comment(stripped);

    char err[256] = "";
    expr_t *e = parse(stripped, err, sizeof(err));
    if (!e) return true;   /* 構文エラー → エラーメッセージを表示するので true */

    bool visible = expr_causes_value_change(e);
    expr_free(e);
    return visible;
}
