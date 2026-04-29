/* シートモデル実装。
 * 移植元: ui/SheetView.cpp の rows_ / ctx_ / undo_buf_ / eval_all /
 *         build_candidates / load_file / save_file 等を C99 で再実装。 */

#include "sheet_model.h"
#include "builtin_docs.h"
#include "completion_match.h"
#include "i18n.h"

#include "eval/eval.h"
#include "eval/builtin.h"
#include "types/val.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define SHEET_UNDO_DEPTH 1000

/* ------------------------------------------------------------------ */
/* 内部データ構造                                                       */
/* ------------------------------------------------------------------ */
typedef struct {
    char     *expr;        /* owned, malloc'd. NULL は空行として扱う */
    char     *result;      /* owned */
    val_fmt_t result_fmt;
    bool      error;
    bool      visible;     /* "=" と result を表示するか */
} row_t;

typedef struct {
    sheet_op_type_t type;
    int             index;
    char           *expr;   /* owned, malloc'd (DELETE なら NULL 可) */
} undo_op_t;

typedef struct {
    sheet_view_state_t view_state;
    undo_op_t         *undo_ops;
    int                undo_n;
    undo_op_t         *redo_ops;
    int                redo_n;
} undo_entry_t;

struct sheet_model {
    row_t          *rows;
    int             n_rows;
    int             cap_rows;

    eval_ctx_t      ctx;

    undo_entry_t   *undo_buf;
    int             undo_n;
    int             undo_cap;
    int             undo_idx;

    sheet_eval_limits_t limits;

    sheet_candidate_t *cands;
    int                n_cands;
    int                cap_cands;

    sheet_model_change_cb change_cb;
    void                 *change_ud;
};

/* ------------------------------------------------------------------ */
/* 小物ユーティリティ                                                   */
/* ------------------------------------------------------------------ */
static char *sm_strdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static void sm_free(void *p) { if (p) free(p); }

static void row_init_empty(row_t *r) {
    r->expr       = sm_strdup("");
    r->result     = sm_strdup("");
    r->result_fmt = FMT_REAL;
    r->error      = false;
    r->visible    = false;
}

static void row_clear(row_t *r) {
    sm_free(r->expr);   r->expr   = NULL;
    sm_free(r->result); r->result = NULL;
}

static void row_set_expr(row_t *r, const char *expr) {
    sm_free(r->expr);
    r->expr = sm_strdup(expr ? expr : "");
}

static void rows_reserve(sheet_model_t *m, int need) {
    if (m->cap_rows >= need) return;
    int nc = m->cap_rows ? m->cap_rows * 2 : 8;
    while (nc < need) nc *= 2;
    m->rows = (row_t *)realloc(m->rows, sizeof(row_t) * nc);
    m->cap_rows = nc;
}

static void rows_insert_empty(sheet_model_t *m, int idx, const char *expr) {
    if (idx < 0) idx = 0;
    if (idx > m->n_rows) idx = m->n_rows;
    rows_reserve(m, m->n_rows + 1);
    memmove(&m->rows[idx + 1], &m->rows[idx], sizeof(row_t) * (m->n_rows - idx));
    row_init_empty(&m->rows[idx]);
    row_set_expr(&m->rows[idx], expr);
    m->n_rows++;
}

static void rows_erase(sheet_model_t *m, int idx) {
    if (idx < 0 || idx >= m->n_rows) return;
    row_clear(&m->rows[idx]);
    memmove(&m->rows[idx], &m->rows[idx + 1], sizeof(row_t) * (m->n_rows - idx - 1));
    m->n_rows--;
    if (m->n_rows == 0) {
        rows_insert_empty(m, 0, "");
    }
}

static void rows_clear_all(sheet_model_t *m) {
    for (int i = 0; i < m->n_rows; i++) row_clear(&m->rows[i]);
    m->n_rows = 0;
}

/* ------------------------------------------------------------------ */
/* undo エントリ管理                                                    */
/* ------------------------------------------------------------------ */
static void entry_free_ops(undo_op_t *ops, int n) {
    if (!ops) return;
    for (int i = 0; i < n; i++) sm_free(ops[i].expr);
    sm_free(ops);
}

static void entry_free(undo_entry_t *e) {
    entry_free_ops(e->undo_ops, e->undo_n);
    entry_free_ops(e->redo_ops, e->redo_n);
    e->undo_ops = e->redo_ops = NULL;
    e->undo_n = e->redo_n = 0;
}

static undo_op_t *ops_copy(const sheet_op_t *src, int n) {
    if (n <= 0) return NULL;
    undo_op_t *dst = (undo_op_t *)calloc(n, sizeof(undo_op_t));
    for (int i = 0; i < n; i++) {
        dst[i].type  = src[i].type;
        dst[i].index = src[i].index;
        dst[i].expr  = sm_strdup(src[i].expr);
    }
    return dst;
}

static void undo_truncate_redo(sheet_model_t *m) {
    for (int i = m->undo_idx; i < m->undo_n; i++) entry_free(&m->undo_buf[i]);
    m->undo_n = m->undo_idx;
}

static void undo_reserve(sheet_model_t *m, int need) {
    if (m->undo_cap >= need) return;
    int nc = m->undo_cap ? m->undo_cap * 2 : 64;
    while (nc < need) nc *= 2;
    m->undo_buf = (undo_entry_t *)realloc(m->undo_buf, sizeof(undo_entry_t) * nc);
    m->undo_cap = nc;
}

/* ops を rows_ に適用 (undo/redo 共通)。 */
static void apply_ops(sheet_model_t *m, const undo_op_t *ops, int n) {
    for (int i = 0; i < n; i++) {
        const undo_op_t *op = &ops[i];
        switch (op->type) {
            case SHEET_OP_INSERT:
                rows_insert_empty(m, op->index, op->expr ? op->expr : "");
                break;
            case SHEET_OP_DELETE:
                rows_erase(m, op->index);
                break;
            case SHEET_OP_CHANGE_EXPR:
                if (op->index >= 0 && op->index < m->n_rows)
                    row_set_expr(&m->rows[op->index], op->expr ? op->expr : "");
                break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* ライフサイクル                                                       */
/* ------------------------------------------------------------------ */
sheet_model_t *sheet_model_new(void) {
    sheet_model_t *m = (sheet_model_t *)calloc(1, sizeof(sheet_model_t));
    m->limits.max_array_length  = 256;
    m->limits.max_string_length = 256;
    m->limits.max_call_depth    = 64;

    eval_ctx_init(&m->ctx);
    builtin_register_all(&m->ctx);

    rows_insert_empty(m, 0, "");
    return m;
}

static void cands_free(sheet_model_t *m) {
    for (int i = 0; i < m->n_cands; i++) {
        sm_free((char *)m->cands[i].id);
        sm_free((char *)m->cands[i].label);
        sm_free((char *)m->cands[i].description);
    }
    m->n_cands = 0;
}

void sheet_model_free(sheet_model_t *m) {
    if (!m) return;
    rows_clear_all(m);
    sm_free(m->rows);

    for (int i = 0; i < m->undo_n; i++) entry_free(&m->undo_buf[i]);
    sm_free(m->undo_buf);

    cands_free(m);
    sm_free(m->cands);

    eval_ctx_free(&m->ctx);
    free(m);
}

void sheet_model_set_limits(sheet_model_t *m, sheet_eval_limits_t limits) {
    m->limits = limits;
}

void sheet_model_set_change_cb(sheet_model_t *m,
                                sheet_model_change_cb cb, void *ud) {
    m->change_cb = cb;
    m->change_ud = ud;
}

static void notify_change(sheet_model_t *m) {
    if (m->change_cb) m->change_cb(m->change_ud);
}

/* ------------------------------------------------------------------ */
/* 行データアクセス                                                     */
/* ------------------------------------------------------------------ */
int sheet_model_row_count(const sheet_model_t *m) { return m->n_rows; }

const char *sheet_model_row_expr(const sheet_model_t *m, int idx) {
    if (idx < 0 || idx >= m->n_rows) return "";
    return m->rows[idx].expr ? m->rows[idx].expr : "";
}

const char *sheet_model_row_result(const sheet_model_t *m, int idx) {
    if (idx < 0 || idx >= m->n_rows) return "";
    return m->rows[idx].result ? m->rows[idx].result : "";
}

val_fmt_t sheet_model_row_fmt(const sheet_model_t *m, int idx) {
    if (idx < 0 || idx >= m->n_rows) return FMT_REAL;
    return m->rows[idx].result_fmt;
}

bool sheet_model_row_error(const sheet_model_t *m, int idx) {
    if (idx < 0 || idx >= m->n_rows) return false;
    return m->rows[idx].error;
}

bool sheet_model_row_visible(const sheet_model_t *m, int idx) {
    if (idx < 0 || idx >= m->n_rows) return false;
    return m->rows[idx].visible;
}

/* ------------------------------------------------------------------ */
/* 行データ書き込み                                                     */
/* ------------------------------------------------------------------ */
void sheet_model_set_row_expr_raw(sheet_model_t *m, int idx, const char *expr) {
    if (idx < 0 || idx >= m->n_rows) return;
    row_set_expr(&m->rows[idx], expr);
}

void sheet_model_set_rows(sheet_model_t *m, const char *const *exprs, int n) {
    rows_clear_all(m);
    if (n <= 0) {
        rows_insert_empty(m, 0, "");
    } else {
        for (int i = 0; i < n; i++)
            rows_insert_empty(m, m->n_rows, exprs[i] ? exprs[i] : "");
    }
    sheet_model_clear_undo(m);
}

/* ------------------------------------------------------------------ */
/* eval_all                                                            */
/* ------------------------------------------------------------------ */
void sheet_model_eval_all(sheet_model_t *m) {
    eval_ctx_free(&m->ctx);
    eval_ctx_init(&m->ctx);
    m->ctx.settings.max_array_length  = m->limits.max_array_length;
    m->ctx.settings.max_string_length = m->limits.max_string_length;
    m->ctx.settings.max_call_depth    = m->limits.max_call_depth;

    for (int i = 0; i < m->n_rows; i++) {
        row_t *r = &m->rows[i];
        const char *expr = r->expr ? r->expr : "";
        if (expr[0] == '\0') {
            sm_free(r->result);
            r->result     = sm_strdup("");
            r->error      = false;
            r->visible    = false;
            r->result_fmt = FMT_REAL;
            continue;
        }
        /* 代入/def/lambda の場合は = と右辺を非表示 */
        r->visible = eval_result_visible(expr);

        m->ctx.has_error    = false;
        m->ctx.error_msg[0] = '\0';
        char errmsg[256] = "";
        val_t *v = eval_str(expr, &m->ctx, errmsg, sizeof(errmsg));
        if (v) {
            char buf[512];
            val_to_display_str(v, buf, sizeof(buf));
            sm_free(r->result);
            r->result     = sm_strdup(buf);
            r->result_fmt = v->fmt;
            r->error      = false;
            if (v->type == VAL_NULL || v->type == VAL_FUNC) r->visible = false;
            val_free(v);
        } else if (errmsg[0] == '\0') {
            /* コメントのみの行: 結果なし・非表示 */
            sm_free(r->result);
            r->result     = sm_strdup("");
            r->error      = false;
            r->visible    = false;
            r->result_fmt = FMT_REAL;
        } else {
            sm_free(r->result);
            r->result     = sm_strdup(errmsg[0] ? errmsg : "error");
            r->error      = true;
            r->result_fmt = FMT_REAL;
            m->ctx.has_error    = false;
            m->ctx.error_msg[0] = '\0';
        }
    }
}

/* ------------------------------------------------------------------ */
/* undo コミット                                                        */
/* ------------------------------------------------------------------ */
void sheet_model_commit(sheet_model_t *m,
                         const sheet_op_t *undo_ops, int undo_n,
                         const sheet_op_t *redo_ops, int redo_n,
                         sheet_view_state_t view_state) {
    undo_truncate_redo(m);

    /* 先に両方を strdup コピーしておく。undo_ops の expr は呼び出し側が
     * model の rows 内部バッファを参照している可能性があり、apply_ops で
     * rows を書き換えた後では dangling になる恐れがある。 */
    undo_op_t *undo_copy = ops_copy(undo_ops, undo_n);
    undo_op_t *redo_copy = ops_copy(redo_ops, redo_n);

    /* redo_ops を実際に rows_ へ適用 */
    apply_ops(m, redo_copy, redo_n);

    undo_reserve(m, m->undo_n + 1);
    undo_entry_t *e = &m->undo_buf[m->undo_n++];
    e->view_state = view_state;
    e->undo_ops   = undo_copy;
    e->undo_n     = undo_n;
    e->redo_ops   = redo_copy;
    e->redo_n     = redo_n;

    m->undo_idx = m->undo_n;

    /* 深さ制限 */
    if (m->undo_n > SHEET_UNDO_DEPTH) {
        entry_free(&m->undo_buf[0]);
        memmove(&m->undo_buf[0], &m->undo_buf[1],
                sizeof(undo_entry_t) * (m->undo_n - 1));
        m->undo_n--;
        m->undo_idx--;
    }

    sheet_model_eval_all(m);
    notify_change(m);
}

/* ------------------------------------------------------------------ */
/* Undo / Redo                                                         */
/* ------------------------------------------------------------------ */
bool sheet_model_can_undo(const sheet_model_t *m) { return m->undo_idx > 0; }
bool sheet_model_can_redo(const sheet_model_t *m) { return m->undo_idx < m->undo_n; }

bool sheet_model_undo(sheet_model_t *m, sheet_view_state_t *out_vs) {
    if (!sheet_model_can_undo(m)) return false;
    m->undo_idx--;
    const undo_entry_t *e = &m->undo_buf[m->undo_idx];
    apply_ops(m, e->undo_ops, e->undo_n);
    if (out_vs) *out_vs = e->view_state;
    sheet_model_eval_all(m);
    notify_change(m);
    return true;
}

bool sheet_model_redo(sheet_model_t *m, sheet_view_state_t *out_vs) {
    if (!sheet_model_can_redo(m)) return false;
    const undo_entry_t *e = &m->undo_buf[m->undo_idx];
    apply_ops(m, e->redo_ops, e->redo_n);
    m->undo_idx++;
    /* 次 entry の view_state (またはなければ現 entry) に基づいてフォーカス復元 */
    if (out_vs) {
        if (m->undo_idx < m->undo_n)
            *out_vs = m->undo_buf[m->undo_idx].view_state;
        else
            *out_vs = e->view_state;
    }
    sheet_model_eval_all(m);
    notify_change(m);
    return true;
}

void sheet_model_clear_undo(sheet_model_t *m) {
    for (int i = 0; i < m->undo_n; i++) entry_free(&m->undo_buf[i]);
    m->undo_n   = 0;
    m->undo_idx = 0;
}

/* ------------------------------------------------------------------ */
/* 補完候補                                                             */
/* ------------------------------------------------------------------ */
static void cands_reserve(sheet_model_t *m, int need) {
    if (m->cap_cands >= need) return;
    int nc = m->cap_cands ? m->cap_cands * 2 : 64;
    while (nc < need) nc *= 2;
    m->cands = (sheet_candidate_t *)realloc(m->cands,
                                             sizeof(sheet_candidate_t) * nc);
    m->cap_cands = nc;
}

static void cands_push(sheet_model_t *m,
                        const char *id, const char *label,
                        const char *description, bool is_function) {
    /* 同名重複をスキップ */
    for (int i = 0; i < m->n_cands; i++)
        if (strcmp(m->cands[i].id, id) == 0) return;

    cands_reserve(m, m->n_cands + 1);
    sheet_candidate_t *c = &m->cands[m->n_cands++];
    c->id          = sm_strdup(id);
    c->label       = sm_strdup(label ? label : id);
    c->description = sm_strdup(description ? description : "");
    c->is_function = is_function;
}

static void make_label(char *buf, size_t bufsz, const char *name, int n_params) {
    if      (n_params == 0) snprintf(buf, bufsz, "%s()", name);
    else if (n_params == 1) snprintf(buf, bufsz, "%s(x)", name);
    else if (n_params == 2) snprintf(buf, bufsz, "%s(x, y)", name);
    else if (n_params == 3) snprintf(buf, bufsz, "%s(x, y, z)", name);
    else                    snprintf(buf, bufsz, "%s(...)", name);
}

static void on_builtin_enum(const char *name, int n_params, void *ud) {
    sheet_model_t *m = (sheet_model_t *)ud;
    char label[128];
    make_label(label, sizeof(label), name, n_params);
    const char *doc = builtin_doc(name);
    cands_push(m, name, label, doc, true);
}

static int cand_cmp(const void *a, const void *b) {
    const sheet_candidate_t *ca = (const sheet_candidate_t *)a;
    const sheet_candidate_t *cb = (const sheet_candidate_t *)b;
    return strcmp(ca->id, cb->id);
}

/* 組み込み定数の説明 (移植元: Calctus EvalContext() AddConstantReal/Hex)。
 * sheet_model_build_candidates でこのテーブルから description を引く。 */
static const struct { const char *name; const char *desc; } CONST_DOCS[] = {
    { "PI",          "circle ratio" },
    { "E",           "base of natural logarithm" },
    { "INT_MIN",     "minimum value of 32 bit signed integer" },
    { "INT_MAX",     "maximum value of 32 bit signed integer" },
    { "UINT_MIN",    "minimum value of 32 bit unsigned integer" },
    { "UINT_MAX",    "maximum value of 32 bit unsigned integer" },
    { "LONG_MIN",    "minimum value of 64 bit signed integer" },
    { "LONG_MAX",    "maximum value of 64 bit signed integer" },
    { "ULONG_MIN",   "minimum value of 64 bit unsigned integer" },
    { "ULONG_MAX",   "maximum value of 64 bit unsigned integer" },
    { "DECIMAL_MIN", "minimum value of Decimal" },
    { "DECIMAL_MAX", "maximum value of Decimal" },
};

static const char *var_doc(const char *name) {
    for (size_t i = 0; i < sizeof(CONST_DOCS) / sizeof(CONST_DOCS[0]); i++)
        if (strcmp(CONST_DOCS[i].name, name) == 0)
            return calcyx_tr(CONST_DOCS[i].desc);
    return calcyx_tr("user-defined variable");
}

int sheet_model_build_candidates(sheet_model_t *m,
                                  const sheet_candidate_t **out_arr) {
    cands_free(m);

    builtin_enum_main (on_builtin_enum, m);
    builtin_enum_extra(on_builtin_enum, m);

    for (int i = 0; i < m->ctx.n_vars; i++) {
        const eval_var_t *v = &m->ctx.vars[i];
        if (!v->value) continue;
        /* 組み込み関数は builtin_enum で追加済 */
        if (v->value->type == VAL_FUNC && v->value->func_v && v->value->func_v->builtin)
            continue;
        char label[128];
        bool is_func = (v->value->type == VAL_FUNC);
        if (is_func && v->value->func_v) {
            make_label(label, sizeof(label), v->name, v->value->func_v->n_params);
            /* ユーザ定義関数: 移植元では Description プロパティを表示するが、
             * calcyx は def 行のコメントから抽出していないので空のまま。 */
            cands_push(m, v->name, label, NULL, true);
        } else {
            cands_push(m, v->name, v->name, var_doc(v->name), false);
        }
    }

    static const struct { const char *kw; const char *doc; } KEYWORDS[] = {
        { "ans",   "last answer" },
        { "true",  "true value" },
        { "false", "false value" },
        { "def",   "user function definition" },
    };
    for (size_t i = 0; i < sizeof(KEYWORDS) / sizeof(KEYWORDS[0]); i++)
        cands_push(m, KEYWORDS[i].kw, KEYWORDS[i].kw,
                   calcyx_tr(KEYWORDS[i].doc), false);

    qsort(m->cands, m->n_cands, sizeof(sheet_candidate_t), cand_cmp);

    if (out_arr) *out_arr = m->cands;
    return m->n_cands;
}

/* ------------------------------------------------------------------ */
/* ファイル I/O                                                         */
/* ------------------------------------------------------------------ */
bool sheet_model_load_file(sheet_model_t *m, const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return false;

    rows_clear_all(m);
    char line[4096];
    while (fgets(line, sizeof(line), fp)) {
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';
        rows_insert_empty(m, m->n_rows, line);
    }
    fclose(fp);
    if (m->n_rows == 0) rows_insert_empty(m, 0, "");

    sheet_model_clear_undo(m);
    sheet_model_eval_all(m);
    notify_change(m);
    return true;
}

bool sheet_model_save_file(const sheet_model_t *m, const char *path) {
    FILE *fp = fopen(path, "w");
    if (!fp) return false;
    for (int i = 0; i < m->n_rows; i++)
        fprintf(fp, "%s\n", m->rows[i].expr ? m->rows[i].expr : "");
    fclose(fp);
    return true;
}

/* ------------------------------------------------------------------ */
/* ユーティリティ                                                       */
/* ------------------------------------------------------------------ */
static const char *const FMTFUNCS[] = {
    "dec", "hex", "bin", "oct", "si", "kibi", "char", NULL
};

static size_t skip_spaces(const char *s, size_t p, size_t end) {
    while (p < end && s[p] == ' ') p++;
    return p;
}

const char *sheet_model_current_fmt_name(const sheet_model_t *m, int idx) {
    if (idx < 0 || idx >= m->n_rows) return NULL;
    const char *expr = m->rows[idx].expr;
    if (!expr) return NULL;
    size_t len = strlen(expr);
    size_t start = skip_spaces(expr, 0, len);
    if (start >= len) return NULL;
    for (int i = 0; FMTFUNCS[i]; i++) {
        size_t fnlen = strlen(FMTFUNCS[i]);
        if (start + fnlen > len) continue;
        if (strncmp(expr + start, FMTFUNCS[i], fnlen) != 0) continue;
        size_t p = skip_spaces(expr, start + fnlen, len);
        if (p < len && expr[p] == '(') return FMTFUNCS[i];
    }
    return NULL;
}

char *sheet_model_strip_formatter(const char *expr) {
    if (!expr) return sm_strdup("");
    size_t len = strlen(expr);
    size_t start = skip_spaces(expr, 0, len);
    if (start >= len) return sm_strdup(expr);
    for (int i = 0; FMTFUNCS[i]; i++) {
        size_t fnlen = strlen(FMTFUNCS[i]);
        if (start + fnlen > len) continue;
        if (strncmp(expr + start, FMTFUNCS[i], fnlen) != 0) continue;
        size_t p = skip_spaces(expr, start + fnlen, len);
        if (p >= len || expr[p] != '(') continue;
        p++;
        p = skip_spaces(expr, p, len);
        /* 末尾の ')' を探す */
        size_t last = len;
        while (last > 0 && expr[last-1] == ' ') last--;
        if (last == 0 || expr[last-1] != ')') continue;
        size_t body_end = last - 1;
        /* body_end から後ろのスペースを除去 */
        while (body_end > p && expr[body_end-1] == ' ') body_end--;
        size_t body_len = body_end - p;
        char *out = (char *)malloc(body_len + 1);
        memcpy(out, expr + p, body_len);
        out[body_len] = '\0';
        return out;
    }
    return sm_strdup(expr);
}

char *sheet_model_build_snapshot(const sheet_model_t *m) {
    /* 必要サイズを計測 */
    size_t cap = 1;
    for (int i = 0; i < m->n_rows; i++) {
        const char *e = m->rows[i].expr;
        const char *r = m->rows[i].result;
        if (!e || !e[0]) continue;
        cap += strlen(e);
        if (r && r[0]) {
            cap += 3;  /* " = " */
            cap += strlen(r);
        }
        cap += 1;  /* '\n' */
    }
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    size_t pos = 0;
    for (int i = 0; i < m->n_rows; i++) {
        const char *e = m->rows[i].expr;
        const char *r = m->rows[i].result;
        if (!e || !e[0]) continue;
        size_t el = strlen(e);
        memcpy(buf + pos, e, el); pos += el;
        if (r && r[0]) {
            memcpy(buf + pos, " = ", 3); pos += 3;
            size_t rl = strlen(r);
            memcpy(buf + pos, r, rl); pos += rl;
        }
        buf[pos++] = '\n';
    }
    buf[pos] = '\0';
    return buf;
}
