/* utf8.h — UTF-8 エンコード / デコードの最小ユーティリティ.
 *
 * engine/types/val.c (FMT_CHAR の出力) と engine/parser/lexer.c
 * (文字リテラル '\u{N}'/'X' のデコード) で使う. */

#ifndef CALCYX_ENGINE_UTF8_H
#define CALCYX_ENGINE_UTF8_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* コードポイント cp を out に UTF-8 で書き込む.
 * 戻り値: 書き込んだバイト数 (1-4). 無効な cp なら 0.
 * out は最大 4 byte 書き込まれる (NUL 終端なし). */
int calcyx_utf8_encode(int32_t cp, char out[4]);

/* s から UTF-8 1 文字をデコード.
 * *out_cp にコードポイントを書き込む. NUL 終端文字列を前提.
 * 戻り値: 消費したバイト数. 無効バイト列の場合は 1 (バイトをそのまま返す). */
int calcyx_utf8_decode(const char *s, int32_t *out_cp);

#ifdef __cplusplus
}
#endif

#endif
