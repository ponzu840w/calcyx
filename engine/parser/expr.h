/* 移植元: Calctus/Model/Expressions/ (各 Expr クラス), Calctus/Model/OpDef.cs */

#ifndef EXPR_H
#define EXPR_H

#include "token.h"
#include <stdbool.h>

/* --- 演算子定義 (移植元: OpDef.cs) --- */

typedef enum {
    /* 単項演算子 */
    OP_UNARY_PLUS = 0, /* +   priority=90 */
    OP_ARITH_INV,      /* -   priority=90 */
    OP_LOGIC_NOT,      /* !   priority=90 */
    OP_BIT_NOT,        /* ~   priority=90 */
    /* 二項演算子 */
    OP_FRAC,           /* $   priority=70 */
    OP_POW,            /* ^   priority=62 */
    OP_MUL,            /* *   priority=61 */
    OP_DIV,            /* /   priority=61 */
    OP_IDIV,           /* //  priority=61 */
    OP_MOD,            /* %   priority=61 */
    OP_ADD,            /* +   priority=60 */
    OP_SUB,            /* -   priority=60 */
    OP_LSL,            /* <<  priority=50 */
    OP_LSR,            /* >>  priority=50 */
    OP_ASL,            /* <<< priority=50 */
    OP_ASR,            /* >>> priority=50 */
    OP_GT,             /* >   priority=41 */
    OP_GE,             /* >=  priority=41 */
    OP_LT,             /* <   priority=41 */
    OP_LE,             /* <=  priority=41 */
    OP_EQ,             /* ==  priority=40 */
    OP_NE,             /* !=  priority=40 */
    OP_BIT_AND,        /* &   priority=34 */
    OP_BIT_XOR,        /* +|  priority=33 */
    OP_BIT_OR,         /* |   priority=32 */
    OP_LOGIC_AND,      /* &&  priority=31 */
    OP_LOGIC_OR,       /* ||  priority=30 */
    OP_EXCL_RANGE,     /* ..  priority=20 */
    OP_INCL_RANGE,     /* ..= priority=20 */
    OP_ARROW,          /* =>  priority=10 */
    OP_ASSIGN,         /* =   priority=0, right-assoc */
    OP_LEADER,         /* ... priority=-1 */
    OP_COUNT
} op_id_t;

typedef enum { OPTYPE_NONE=0, OPTYPE_UNARY=1, OPTYPE_BINARY=2 } op_type_t;
typedef enum { ASSOC_LEFT=0, ASSOC_RIGHT=1 } op_assoc_t;

typedef struct {
    op_id_t    id;
    op_type_t  type;
    op_assoc_t assoc;
    int        priority;
    const char *symbol;
} op_def_t;

extern const op_def_t OP_TABLE[OP_COUNT];
const op_def_t *op_find(op_type_t type, const char *symbol);

/* --- 引数定義リスト (移植元: ArgDefList.cs) --- */

typedef struct {
    char  **names;       /* malloc'd 文字列配列 */
    int     n;
    int     vec_arg_idx; /* -1: なし */
    bool    variadic;    /* [] ... 可変長モード */
} arg_def_list_t;

void arg_def_list_free(arg_def_list_t *a);

/* --- AST ノード型 (移植元: Calctus/Model/Expressions/ 各クラス) --- */

typedef enum {
    EXPR_NUM_LIT,  /* 数値リテラル (val) */
    EXPR_BOOL_LIT, /* 真偽値リテラル (val) */
    EXPR_ID,       /* 識別子参照 (name) */
    EXPR_CALL,     /* 関数呼び出し (name, args, n_args) */
    EXPR_UNARY,    /* 単項演算 (op, child_a) */
    EXPR_BINARY,   /* 二項演算 (op, child_a, child_b) */
    EXPR_COND,     /* 条件演算 ?: (child_a=cond, child_b=t, child_c=f) */
    EXPR_ARRAY,    /* 配列リテラル / 多重括弧 (args, n_args) */
    EXPR_LAMBDA,   /* ラムダ式 (arg_defs, body) */
    EXPR_DEF,      /* def 宣言 (name, arg_defs, body) */
    EXPR_PART_REF, /* 部分参照 a[i] か a[i:j] (child_a=target, child_b=from,
                      child_c=to; to が NULL なら単一インデックス) */
    EXPR_ASTER,    /* ベクタ化引数 *id (name) */
} expr_type_t;

typedef struct expr_s expr_t;

struct expr_s {
    expr_type_t    type;
    token_t        tok;              /* エラー報告用トークン */

    val_t         *val;              /* NUM_LIT, BOOL_LIT */
    char           name[TOK_TEXT_MAX]; /* ID, CALL, ASTER, DEF */
    op_id_t        op;               /* UNARY, BINARY */

    expr_t        *child_a;          /* 第1子 */
    expr_t        *child_b;          /* 第2子 */
    expr_t        *child_c;          /* 第3子 */

    expr_t       **args;             /* CALL/ARRAY: 子式配列 */
    int            n_args;

    arg_def_list_t *arg_defs;        /* LAMBDA, DEF; その他 NULL */
    expr_t         *body;            /* LAMBDA, DEF */
};

void    expr_free(expr_t *e);
expr_t *expr_dup (const expr_t *e);  /* ディープコピー */

/* 移植元: Calctus/Model/Expressions/ - CausesValueChange()
 * 式が「値の変化」を起こすかどうか: false なら = と右辺を非表示にする。
 * - 代入式: RHS の causes_value_change に依存
 * - def / lambda: false
 * - 数値/真偽値リテラル単体: false
 * - それ以外 (演算・関数呼び出し・変数参照 etc.): true */
bool expr_causes_value_change(const expr_t *e);

#endif /* EXPR_H */
