/* engine/test_parser.c — レキサー/パーサーの境界ケース回帰テスト
 *
 * 移植元 (Calctus) には対応するテストがないため独自追加。
 * 2026-04-21 の lexer.c 空白スキップバグ (0x80 重複 / UTF-8 継続バイト
 * 単独マッチ) のような境界ケースを今後検知するためのテスト集。
 *
 * 既存 test_types.c と同様、グローバル g_failures に件数を集計し
 * main で return g_failures ? 1 : 0 する方式。
 */

#include "parser/lexer.h"
#include "parser/parser.h"
#include "parser/expr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_failures = 0;

#define FAIL(label) do { \
    fprintf(stderr, "FAIL: %s\n", (label)); \
    g_failures++; \
} while (0)

#define EXPECT(label, cond) do { \
    if (!(cond)) FAIL(label); \
} while (0)

/* ---- ヘルパー ---- */

/* 最初の非 EOS トークンを取得 (所有権は q が保持) */
static const token_t *first_token(tok_queue_t *q) {
    for (int i = 0; i < q->count; i++) {
        int idx = (q->head + i) % TOK_QUEUE_MAX;
        if (q->tokens[idx].type != TOK_EOS) return &q->tokens[idx];
    }
    return NULL;
}

/* EOS 以外のトークン数 */
static int non_eos_count(const tok_queue_t *q) {
    int n = 0;
    for (int i = 0; i < q->count; i++) {
        int idx = (q->head + i) % TOK_QUEUE_MAX;
        if (q->tokens[idx].type != TOK_EOS) n++;
    }
    return n;
}

/* ---- Lexer: 空白スキップ回帰テスト (2026-04-21 バグ関連) ----
 *
 * 修正前: lexer_eos は c == 0x80 || c == 0xE3 という条件で単独バイトを
 * 空白扱いしていたため、U+3000 の継続バイト 0x80 だけで空白スキップが
 * 発火してしまっていた。現行: E3 80 80 の 3 バイト列のみを U+3000 として
 * スキップする。以下は同種のリグレッションを防ぐための入力。
 */

static void test_ws_u3000_alone(void) {
    /* U+3000 だけの入力: トークン化結果は EOS のみ */
    tok_queue_t q;
    lexer_tokenize("\xE3\x80\x80", &q);
    EXPECT("[lexer] U+3000 alone -> only EOS", non_eos_count(&q) == 0);
    tok_queue_free(&q);
}

static void test_ws_u3000_prefix(void) {
    /* U+3000 を前置した "1+1" が従来通り評価できる */
    char errmsg[128] = {0};
    expr_t *e = parse("\xE3\x80\x80""1+1", errmsg, sizeof(errmsg));
    EXPECT("[parser] U+3000 prefix + expr parses", e != NULL);
    if (e) expr_free(e);
}

static void test_ws_bare_0x80_not_ws(void) {
    /* 単独の 0x80 は空白扱いしない (UTF-8 継続バイトは単独で現れない)。
     * 修正前はここで 0x80 が空白スキップされ "1" だけが TOK_NUM_LIT に
     * なってしまっていた。現行は 0x80 が TOK_EMPTY で残り、"1" が後続
     * トークンになる。 */
    tok_queue_t q;
    lexer_tokenize("\x80""1", &q);
    const token_t *t = first_token(&q);
    EXPECT("[lexer] bare 0x80 -> first tok is TOK_EMPTY (not skipped)",
           t && t->type == TOK_EMPTY);
    EXPECT("[lexer] bare 0x80 -> has 2 non-EOS tokens", non_eos_count(&q) == 2);
    tok_queue_free(&q);
}

static void test_ws_0xE3_partial(void) {
    /* 0xE3 単独 (U+3000 の第1バイトだけ) は空白扱いしない */
    tok_queue_t q;
    lexer_tokenize("\xE3""abc", &q);
    const token_t *t = first_token(&q);
    EXPECT("[lexer] 0xE3 alone -> first tok is TOK_EMPTY (not skipped)",
           t && t->type == TOK_EMPTY);
    tok_queue_free(&q);

    /* 0xE3 0x80 だけ (U+3000 の第3バイト欠損) も空白扱いしない */
    tok_queue_t q2;
    lexer_tokenize("\xE3\x80""abc", &q2);
    const token_t *t2 = first_token(&q2);
    EXPECT("[lexer] 0xE3 0x80 (no third byte) -> first tok is TOK_EMPTY",
           t2 && t2->type == TOK_EMPTY);
    tok_queue_free(&q2);
}

/* ---- Lexer: 数値フォーマット境界 ---- */

static void test_number_bare_0x(void) {
    /* "0x" だけ: hex 桁なし → try_hex 失敗、try_int で "0" のみ消費、
     * "x" は識別子になる */
    tok_queue_t q;
    lexer_tokenize("0x", &q);
    EXPECT("[lexer] bare 0x -> 2 tokens", non_eos_count(&q) == 2);
    const token_t *t = first_token(&q);
    EXPECT("[lexer] bare 0x -> first is NUM_LIT",
           t && t->type == TOK_NUM_LIT);
    EXPECT("[lexer] bare 0x -> first text is \"0\"",
           t && strcmp(t->text, "0") == 0);
    tok_queue_free(&q);
}

static void test_number_bare_0b(void) {
    /* "0b" だけ: 同様に "0" + "b" に分解 */
    tok_queue_t q;
    lexer_tokenize("0b", &q);
    EXPECT("[lexer] bare 0b -> 2 tokens", non_eos_count(&q) == 2);
    const token_t *t = first_token(&q);
    EXPECT("[lexer] bare 0b -> first is NUM_LIT \"0\"",
           t && t->type == TOK_NUM_LIT && strcmp(t->text, "0") == 0);
    tok_queue_free(&q);
}

static void test_number_hex_ok(void) {
    tok_queue_t q;
    lexer_tokenize("0xFF", &q);
    EXPECT("[lexer] 0xFF -> 1 num lit", non_eos_count(&q) == 1);
    const token_t *t = first_token(&q);
    EXPECT("[lexer] 0xFF -> text matches",
           t && strcmp(t->text, "0xFF") == 0);
    tok_queue_free(&q);
}

static void test_number_underscore(void) {
    /* "0x1_2" = 0x12 */
    tok_queue_t q;
    lexer_tokenize("0x1_2", &q);
    EXPECT("[lexer] 0x1_2 -> 1 num lit", non_eos_count(&q) == 1);
    const token_t *t = first_token(&q);
    EXPECT("[lexer] 0x1_2 -> text matches",
           t && strcmp(t->text, "0x1_2") == 0);
    tok_queue_free(&q);
}

static void test_number_dot_frac(void) {
    /* ".5" は整数部省略の実数 */
    tok_queue_t q;
    lexer_tokenize(".5", &q);
    EXPECT("[lexer] .5 -> 1 num lit", non_eos_count(&q) == 1);
    const token_t *t = first_token(&q);
    EXPECT("[lexer] .5 -> TOK_NUM_LIT",
           t && t->type == TOK_NUM_LIT);
    tok_queue_free(&q);
}

static void test_number_trailing_e(void) {
    /* "1.5e" の "e" は指数部として消費されない (digits なし) */
    tok_queue_t q;
    lexer_tokenize("1.5e", &q);
    EXPECT("[lexer] 1.5e -> 2 tokens (num + word e)",
           non_eos_count(&q) == 2);
    const token_t *t = first_token(&q);
    EXPECT("[lexer] 1.5e -> first text is \"1.5\"",
           t && strcmp(t->text, "1.5") == 0);
    tok_queue_free(&q);
}

/* ---- Lexer: 未閉鎖リテラル ---- */

static void test_unclosed_string(void) {
    /* 未閉鎖 "": try_string 失敗 → '"' が TOK_EMPTY として残る */
    tok_queue_t q;
    lexer_tokenize("\"abc", &q);
    const token_t *t = first_token(&q);
    EXPECT("[lexer] unclosed string -> first tok is TOK_EMPTY (\")",
           t && t->type == TOK_EMPTY && t->text[0] == '"');
    tok_queue_free(&q);
}

static void test_unclosed_char(void) {
    /* 未閉鎖 '': try_char 失敗 → '\'' が TOK_EMPTY として残る */
    tok_queue_t q;
    lexer_tokenize("'a", &q);
    const token_t *t = first_token(&q);
    EXPECT("[lexer] unclosed char -> first tok is TOK_EMPTY (')",
           t && t->type == TOK_EMPTY && t->text[0] == '\'');
    tok_queue_free(&q);
}

/* ---- Parser: エラーケース ----
 *
 * parse() はエラー時に NULL を返し errmsg を埋める契約。
 * 各ケースで NULL が返ることを検証する (詳細な errmsg 文字列は
 * 実装都合で変わる可能性があるため内容は確認しない)。
 */

static void expect_parse_error(const char *label, const char *src) {
    char errmsg[128] = {0};
    expr_t *e = parse(src, errmsg, sizeof(errmsg));
    if (e != NULL) {
        fprintf(stderr, "FAIL: %s  expected parse error but got AST\n", label);
        g_failures++;
        expr_free(e);
    }
}

static void test_parse_errors(void) {
    expect_parse_error("[parser] unclosed paren",      "(1+2");
    expect_parse_error("[parser] dangling plus",       "1+");
    expect_parse_error("[parser] lone operator",       "+");
    expect_parse_error("[parser] incomplete ternary",  "1?2");
    expect_parse_error("[parser] unclosed bracket",    "[1,2");
}

static void test_parse_ok(void) {
    /* 正常系も 1 件確認: エラー側だけ動く壊れ方を防ぐ */
    char errmsg[128] = {0};
    expr_t *e = parse("1+2*3", errmsg, sizeof(errmsg));
    EXPECT("[parser] 1+2*3 parses OK", e != NULL);
    if (e) expr_free(e);
}

/* ---- main ---- */

int main(void) {
    /* Lexer: 空白スキップ回帰 */
    test_ws_u3000_alone();
    test_ws_u3000_prefix();
    test_ws_bare_0x80_not_ws();
    test_ws_0xE3_partial();

    /* Lexer: 数値フォーマット境界 */
    test_number_bare_0x();
    test_number_bare_0b();
    test_number_hex_ok();
    test_number_underscore();
    test_number_dot_frac();
    test_number_trailing_e();

    /* Lexer: 未閉鎖リテラル */
    test_unclosed_string();
    test_unclosed_char();

    /* Parser: エラー検出 / 正常系 */
    test_parse_errors();
    test_parse_ok();

    if (g_failures == 0) {
        printf("All parser/lexer tests passed.\n");
        return 0;
    }
    fprintf(stderr, "%d test(s) failed.\n", g_failures);
    return 1;
}
