/* 移植元: Calctus/Model/Parsers/Parser.cs */

#ifndef PARSER_H
#define PARSER_H

#include "expr.h"
#include "lexer.h"

/* --- パーサー --- */

typedef struct {
    tok_queue_t *q;
    token_t      buff;       /* 1 トークン先読みバッファ (TOK_EMPTY = 未使用) */
    token_t      last_tok;   /* 直前に消費したトークン (val は所有しない) */
    bool         has_error;
    char         error_msg[256];
    int          error_pos;
} parser_t;

void    parser_init (parser_t *p, tok_queue_t *q);
expr_t *parser_pop  (parser_t *p, bool root);

/* 入力文字列全体を解析して AST を返す。
 * 失敗時は NULL を返し errmsg (NULL 可) にメッセージを書く */
expr_t *parse(const char *src, char *errmsg, int errmsg_len);

#endif /* PARSER_H */
