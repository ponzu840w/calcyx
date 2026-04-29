/* calcyx.conf をコメント・並び順・未知キーを保持して書き出すライタ。
 * 出力順の正本は shared/settings_schema.c の TABLE. */

#ifndef CALCYX_SHARED_SETTINGS_WRITER_H
#define CALCYX_SHARED_SETTINGS_WRITER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* キー名を渡すと現在値を文字列化して buf に書く。
 *   1  = PROVIDED. key=value (is_default=1 なら #key=value)。
 *   -1 = LEAVE. 既存行を非破壊で転写 (例: GUI save 時の TUI 専用キー)。
 * SECTION エントリには呼ばれない。 */
typedef int (*calcyx_setting_value_fn)(const char *key, char *buf, size_t buflen,
                                       int *out_is_default, void *user);

/* path の conf を lookup で更新して書き戻す。
 *   first_time_header: 新規 / 空ファイル時のみ先頭に出す (NULL 可)。
 *   戻り値: 0=成功、 非 0=エラー。 */
int calcyx_settings_write_preserving(const char            *path,
                                     const char            *first_time_header,
                                     calcyx_setting_value_fn lookup,
                                     void                  *user);

/* スキーマ既定値で path に conf を新規生成する (既存は上書きしない)。
 * 戻り値: 1=新規生成、 0=スキップ、 -1=エラー。 */
int calcyx_settings_init_defaults(const char *path,
                                  const char *first_time_header);

#ifdef __cplusplus
}
#endif

#endif
