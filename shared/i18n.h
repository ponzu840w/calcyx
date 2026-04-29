/* 軽量多言語化レイヤ (gettext 風、 英語 id)。
 * 翻訳は shared/i18n_table.c の static map. 未登録 / en モードは identity.
 *   _("File not found")  // 翻訳された文字列を返す */

#ifndef CALCYX_SHARED_I18N_H
#define CALCYX_SHARED_I18N_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CALCYX_LANG_EN = 0,
    CALCYX_LANG_JA = 1
} calcyx_lang_t;

/* 起動時に 1 回呼ぶ。 lang_id は "auto" / "en" / "ja". "auto" は OS の
 * ロケールを検出して en/ja のどちらかを選ぶ。 不明な値は en にフォールバック。 */
void calcyx_i18n_init(const char *lang_id);

/* 現在固定中の言語を返す。 */
calcyx_lang_t calcyx_i18n_current(void);

/* calcyx_i18n_init が呼ばれていれば 1, それ以外は 0. テスト等で先に
 * i18n_init を呼んだあとに再初期化を抑止したいときに使う。 */
int calcyx_i18n_is_initialized(void);

/* lang_id が "auto" / "en" / "ja" のいずれかなら 1, それ以外は 0. */
int calcyx_i18n_lang_id_valid(const char *lang_id);

/* 翻訳関数。 テーブル未登録 / en モード / NULL 入力では引数そのまま返す。
 * 戻り値は内部 static or 入力ポインタなので呼び出し側で free しない。 */
const char *calcyx_tr(const char *en);

/* gettext 風ショートマクロ。 */
#define _(s) calcyx_tr(s)

/* OS のロケール設定から "en" / "ja" を返す。 環境変数 / Win32 API を見る。
 * 戻り値は静的バッファ。 */
const char *calcyx_locale_detect(void);

/* 純粋関数。 ロケール文字列 (例: "ja_JP.UTF-8") を "en"/"ja" に正規化。
 * NULL や空文字、 非対応言語は "en" を返す。 テスト用に export. */
const char *calcyx_locale_normalize(const char *raw);

#ifdef __cplusplus
}
#endif

#endif /* CALCYX_SHARED_I18N_H */
