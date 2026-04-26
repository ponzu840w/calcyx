/* i18n_table.c — English → Japanese 翻訳辞書.
 *
 * Phase ごとに各フロントエンド (GUI/TUI/CLI) や builtin_docs の翻訳を
 * 追記する. キーは英語そのまま (gettext 風). 同じ英語で文脈が違う場合は
 * 別途 prefix キーを用意する (まだ未発生).
 *
 * 起動時に i18n.c が qsort して bsearch するので, 並び順は気にしなくてよい. */

#include <stddef.h>

typedef struct {
    const char *en;
    const char *ja;
} calcyx_tr_entry_t;

const calcyx_tr_entry_t CALCYX_TR_TABLE_JA[] = {
    /* L1: インフラ単独テスト用に最小エントリを置く. 各 phase で追記. */
    { "Restart to apply this change", "再起動後に変更が反映されます" }
};

const int CALCYX_TR_TABLE_JA_N =
    (int)(sizeof(CALCYX_TR_TABLE_JA) / sizeof(CALCYX_TR_TABLE_JA[0]));
