/* UTF-8 パスを正しく扱う薄い IO ラッパー。
 * Windows: UTF-8 → UTF-16 変換して _wfopen 等を呼ぶ (CP932 誤解釈で
 *          C:\Users\<非ASCII>\... が文字化けするのを回避)。
 * その他 : 標準 fopen 等をそのまま呼ぶ。 */

#ifndef CALCYX_PATH_UTF8_H
#define CALCYX_PATH_UTF8_H

#include <stdio.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

FILE *calcyx_fopen   (const char *path_utf8, const char *mode);
int   calcyx_rename  (const char *src_utf8, const char *dst_utf8);
int   calcyx_remove  (const char *path_utf8);
int   calcyx_mkdir   (const char *path_utf8);

/* getenv を UTF-8 で取得。 戻り値: 1=成功、 0=未設定 / 変換失敗。
 * Windows では _wgetenv で UTF-16 取得 → UTF-8 変換、 それ以外では
 * getenv の結果を buf にコピーする (UTF-8 想定の前提)。 */
int calcyx_getenv_utf8(const char *name, char *buf, size_t buflen);

#ifdef __cplusplus
}
#endif

#endif /* CALCYX_PATH_UTF8_H */
