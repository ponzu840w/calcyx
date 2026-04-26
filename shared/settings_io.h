/* settings_io — calcyx.conf の C-only 読み出しと既定パス解決. GUI / TUI / CLI
 * で共有するための薄いユーティリティ層. 書き出しは settings_writer に任せ,
 * ここでは「読む側」だけを提供する.
 *
 * - calcyx_default_conf_path() : XDG_CONFIG_HOME / APPDATA / ~/Library を踏まえ
 *                                プラットフォーム既定の conf パスを buf に書く.
 *                                conf ディレクトリが無ければ作成する.
 * - calcyx_conf_each()         : key = value 形式の行ごとに callback を呼ぶ.
 *                                コメント/空行/解析失敗行はスキップする.
 *                                callback 戻り値は無視. */

#ifndef CALCYX_SHARED_SETTINGS_IO_H
#define CALCYX_SHARED_SETTINGS_IO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 既定の conf パスを buf に書く. 戻り値: 1=成功, 0=失敗 (HOME/APPDATA 取得不可). */
int calcyx_default_conf_path(char *buf, size_t buflen);

/* path の conf を 1 行ずつ読み, key/value 文字列で callback を呼ぶ.
 *  - line_no は 1-origin の元ファイル行番号 (空行/コメントもカウント).
 *  - 空行・先頭 '#' 行・'=' を含まない行は callback されない.
 *  - 戻り値: 0=成功, -1=fopen 失敗.
 *  - 同じキーが複数回出現する場合は出現順にすべて callback する (validation 用). */
typedef void (*calcyx_conf_kv_fn)(const char *key, const char *value,
                                  int line_no, void *user);

int calcyx_conf_each(const char *path, calcyx_conf_kv_fn cb, void *user);

/* 文字列を bool として解釈. 1=書いた, 0=parse 失敗. */
int calcyx_conf_parse_bool(const char *s, int *out);

/* 文字列を int として解釈. 末尾空白許容, 余計な文字があれば 0. */
int calcyx_conf_parse_int(const char *s, int *out);

/* "#RRGGBB" を [r,g,b] に. 1=書いた, 0=parse 失敗. */
int calcyx_conf_parse_hex_color(const char *s, unsigned char rgb[3]);

#ifdef __cplusplus
}
#endif

#endif
