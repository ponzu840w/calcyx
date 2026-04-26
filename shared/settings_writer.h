/* settings_writer — calcyx.conf を「コメント・並び順・未知キーを保持」して
 * 書き出すライタ.
 *
 * 既存ファイルを 1 行ずつ読み, 既知キーは現在値で上書き, 未知キー・コメント・
 * 空行はそのまま, スキーマに存在するが既存ファイルに未出現のキーは末尾の
 * セクションヘッダ付きで追記する. これにより GUI の Preferences ダイアログ
 * から save しても, ユーザーが手で書いたコメントが消えなくなる.
 *
 * shared/settings_schema.c の TABLE が出力順 (canonical 形式) の決定版. */

#ifndef CALCYX_SHARED_SETTINGS_WRITER_H
#define CALCYX_SHARED_SETTINGS_WRITER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* スキーマのキー名を渡すと現在値を文字列化して buf に書く.
 *  - 戻り値 1: buf に値を書いた (key = buf 形式で出力する)
 *  - 戻り値 0: そのキーは出力すべきでない (色プリセット非 user 時の color_* 等).
 *              既存行があれば削除し, 末尾追記もしない.
 *
 * out_is_default が NULL でない場合, 値がスキーマのデフォルトと一致するなら
 * *out_is_default = 1, そうでなければ 0 を書き込む. writer はデフォルト値を
 * '#key = value' 形式 (コメントアウト) で出力する. SECTION エントリに対しては
 * 呼ばれない. */
typedef int (*calcyx_setting_value_fn)(const char *key, char *buf, size_t buflen,
                                       int *out_is_default, void *user);

/* path の conf を読み, lookup の値で更新して書き戻す.
 *
 * - first_time_header: 既存ファイルが存在しない (or 空) 場合だけ先頭に出力する
 *                      文字列. NULL なら何も出さない. 末尾改行の有無は呼び元責任.
 * - 戻り値: 0=成功, 非 0=エラー (errno が立つことが多い). */
int calcyx_settings_write_preserving(const char            *path,
                                     const char            *first_time_header,
                                     calcyx_setting_value_fn lookup,
                                     void                  *user);

/* スキーマの既定値で path に conf を新規生成する. 既存ファイルがあれば
 * 何もしない (上書きしない).
 *
 * - first_time_header: 先頭に書くコメント行. NULL なら省略.
 * - 戻り値:  1 = 新規生成した
 *            0 = 既に存在したのでスキップ
 *           -1 = 書き込みエラー (errno が立つ).
 *
 * 内部的に calcyx_settings_write_preserving の defaults lookup 版を呼ぶだけ. */
int calcyx_settings_init_defaults(const char *path,
                                  const char *first_time_header);

#ifdef __cplusplus
}
#endif

#endif
