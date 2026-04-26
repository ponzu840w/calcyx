/* path_utf8 — UTF-8 で受け取ったパスを正しく扱う薄い IO ラッパー.
 *
 * Windows の標準 C ランタイム (fopen 等) はパスを ANSI = システムコード
 * ページ (日本語環境では CP932) として解釈する. 一方 calcyx は内部的に
 * パスを UTF-8 で扱う (FLTK 1.4 / SHGetFolderPathW + UTF-8 変換に統一).
 * そのまま fopen 等に渡すと CP932 として誤解釈され, ユーザー名に非 ASCII
 * が含まれるパス (例: C:\Users\ポン酢\AppData) で文字化けして読み書き
 * できなくなる.
 *
 * このヘッダの calcyx_fopen 等は:
 *   - Windows: UTF-8 を MultiByteToWideChar で UTF-16 に変換してから
 *              _wfopen / _wrename / _wremove / _wmkdir 等を呼ぶ.
 *   - その他 : 標準 fopen / rename / remove / mkdir をそのまま呼ぶ
 *              (ファイルシステムが UTF-8 想定なので変換不要). */

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

/* getenv を UTF-8 で取得. 戻り値: 1=成功, 0=未設定 / 変換失敗.
 * Windows では _wgetenv で UTF-16 取得 → UTF-8 変換, それ以外では
 * getenv の結果を buf にコピーする (UTF-8 想定の前提). */
int calcyx_getenv_utf8(const char *name, char *buf, size_t buflen);

#ifdef __cplusplus
}
#endif

#endif /* CALCYX_PATH_UTF8_H */
