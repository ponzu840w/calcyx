/* 移植元: Calctus/Model/Parsers/Lexer.cs */

#ifndef LEXER_H
#define LEXER_H

#include "token.h"

/* ======================================================
 * レキサー
 * ====================================================== */

typedef struct {
    const char *src;  /* 入力文字列 (所有しない) */
    int         pos;  /* 現在の読み取り位置 */
    int         len;  /* 入力文字列の長さ */
    bool        eos_read;  /* EOS トークンを一度読んだか */
} lexer_t;

void lexer_init (lexer_t *lx, const char *src);
bool lexer_eos  (lexer_t *lx);     /* 空白スキップ後に終端か */

/* 次のトークンを1つ読み出す (t に書き込む) */
void lexer_pop  (lexer_t *lx, token_t *t);

/* ======================================================
 * トークンキュー (Parser が使う)
 * ====================================================== */

#define TOK_QUEUE_MAX 512

typedef struct {
    token_t  tokens[TOK_QUEUE_MAX];
    int      head;  /* 次に読む位置 */
    int      count; /* 有効なトークン数 */
} tok_queue_t;

void tok_queue_init      (tok_queue_t *q);
void tok_queue_free      (tok_queue_t *q);
void tok_queue_push_back (tok_queue_t *q, const token_t *t);
/* 先頭を返す (EOSの場合は繰り返し同じEOSを返す) */
const token_t *tok_queue_peek (const tok_queue_t *q);
/* 先頭を消費して返す */
token_t        tok_queue_pop  (tok_queue_t *q);

/* 入力文字列全体をトークン化してキューに積む */
void lexer_tokenize(const char *src, tok_queue_t *q);

/* ======================================================
 * ユーティリティ
 * ====================================================== */

bool lexer_is_id_start (char c);
bool lexer_is_id_follow(char c);

#endif /* LEXER_H */
