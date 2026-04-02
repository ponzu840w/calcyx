/* 移植元: Calctus/Model/Parsers/Token.cs, Types.cs */

#ifndef TOKEN_H
#define TOKEN_H

#include "../types/val.h"
#include <stdbool.h>

#define TOK_TEXT_MAX 256

typedef enum {
    TOK_NUM_LIT,     /* 数値リテラル (hex/oct/bin/int/real/char/si/bin-pfx/color) */
    TOK_BOOL_LIT,    /* true / false */
    TOK_OP,          /* 演算子記号 */
    TOK_SYMBOL,      /* 一般記号: ( ) [ ] , : ? */
    TOK_KEYWORD,     /* def */
    TOK_WORD,        /* 識別子 */
    TOK_EOS,         /* 入力終端 */
    TOK_EMPTY,       /* 空/無効 */
} tok_type_t;

typedef struct {
    tok_type_t  type;
    int         pos;                /* ソース文字列内の開始位置 */
    char        text[TOK_TEXT_MAX]; /* トークン文字列 (NUL 終端) */
    val_t      *val;                /* TOK_NUM_LIT: 解析済みの値 (所有) */
} token_t;

/* token_t を解放 (val が所有されている場合は val_free も呼ぶ) */
void tok_free(token_t *t);

#endif /* TOKEN_H */
