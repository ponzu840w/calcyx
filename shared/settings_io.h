/* calcyx.conf の C-only 読み出しと既定パス解決 (GUI/TUI/CLI 共有)。
 *  default_conf_path: 既定パス取得 + ディレクトリ作成。
 *  conf_each       : key=value 行ごとに callback (コメント/空行スキップ)。 */

#ifndef CALCYX_SHARED_SETTINGS_IO_H
#define CALCYX_SHARED_SETTINGS_IO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 既定の conf パスを buf に書く。 戻り値: 1=成功、 0=失敗 (HOME/APPDATA 取得不可)。 */
int calcyx_default_conf_path(char *buf, size_t buflen);

/* path の conf を 1 行ずつ読み key/value で callback. line_no は 1-origin
 * (空行/コメントもカウント)。 重複キーは出現順に全て callback (validation 用)。 */
typedef void (*calcyx_conf_kv_fn)(const char *key, const char *value,
                                  int line_no, void *user);

int calcyx_conf_each(const char *path, calcyx_conf_kv_fn cb, void *user);

/* 文字列を bool として解釈。 1=書いた、 0=parse 失敗。 */
int calcyx_conf_parse_bool(const char *s, int *out);

/* 文字列を int として解釈。 末尾空白許容、 余計な文字があれば 0. */
int calcyx_conf_parse_int(const char *s, int *out);

/* "#RRGGBB" を [r,g,b] に。 1=書いた、 0=parse 失敗。 */
int calcyx_conf_parse_hex_color(const char *s, unsigned char rgb[3]);

#ifdef __cplusplus
}
#endif

#endif
