/* see i18n.h. 起動時 1 回 qsort, 以降は bsearch.
 * en モード / 未登録キーは入力ポインタを return. */

#include "i18n.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *en;
    const char *ja;
} calcyx_tr_entry_t;

/* shared/i18n_table.c で定義。 翻訳エントリ群。 */
extern const calcyx_tr_entry_t CALCYX_TR_TABLE_JA[];
extern const int               CALCYX_TR_TABLE_JA_N;

/* 状態。 グローバルだが i18n_init を 1 回呼んだあとは const とみなして OK. */
static calcyx_lang_t  s_lang        = CALCYX_LANG_EN;
static int            s_initialized = 0;
static calcyx_tr_entry_t *s_sorted_ja = NULL;  /* sort 済みコピー (lazy) */

static int cmp_entry_en(const void *a, const void *b) {
    const calcyx_tr_entry_t *ea = (const calcyx_tr_entry_t *)a;
    const calcyx_tr_entry_t *eb = (const calcyx_tr_entry_t *)b;
    return strcmp(ea->en, eb->en);
}

static void ensure_sorted(void) {
    if (s_sorted_ja || CALCYX_TR_TABLE_JA_N == 0) return;
    s_sorted_ja = (calcyx_tr_entry_t *)malloc(
        sizeof(calcyx_tr_entry_t) * (size_t)CALCYX_TR_TABLE_JA_N);
    if (!s_sorted_ja) return;
    memcpy(s_sorted_ja, CALCYX_TR_TABLE_JA,
           sizeof(calcyx_tr_entry_t) * (size_t)CALCYX_TR_TABLE_JA_N);
    qsort(s_sorted_ja, (size_t)CALCYX_TR_TABLE_JA_N,
          sizeof(calcyx_tr_entry_t), cmp_entry_en);
}

int calcyx_i18n_lang_id_valid(const char *lang_id) {
    if (!lang_id) return 0;
    return strcmp(lang_id, "auto") == 0
        || strcmp(lang_id, "en")   == 0
        || strcmp(lang_id, "ja")   == 0;
}

void calcyx_i18n_init(const char *lang_id) {
    const char *resolved = "en";
    if (lang_id && strcmp(lang_id, "ja") == 0) {
        resolved = "ja";
    } else if (lang_id && strcmp(lang_id, "en") == 0) {
        resolved = "en";
    } else {
        /* "auto" / NULL / 不明 → OS ロケールから検出 */
        resolved = calcyx_locale_detect();
    }
    s_lang = (strcmp(resolved, "ja") == 0) ? CALCYX_LANG_JA : CALCYX_LANG_EN;
    if (s_lang == CALCYX_LANG_JA) ensure_sorted();
    s_initialized = 1;
}

calcyx_lang_t calcyx_i18n_current(void) {
    return s_lang;
}

int calcyx_i18n_is_initialized(void) {
    return s_initialized;
}

const char *calcyx_tr(const char *en) {
    calcyx_tr_entry_t key;
    calcyx_tr_entry_t *hit;
    if (!en) return en;
    /* init 前 / en モード / テーブル空 → identity */
    if (!s_initialized || s_lang != CALCYX_LANG_JA
            || !s_sorted_ja || CALCYX_TR_TABLE_JA_N == 0) {
        return en;
    }
    key.en = en;
    key.ja = NULL;
    hit = (calcyx_tr_entry_t *)bsearch(&key, s_sorted_ja,
                                       (size_t)CALCYX_TR_TABLE_JA_N,
                                       sizeof(calcyx_tr_entry_t),
                                       cmp_entry_en);
    if (!hit || !hit->ja) return en;
    return hit->ja;
}

const char *calcyx_locale_normalize(const char *raw) {
    /* 先頭 2 文字を見て "ja" prefix なら ja, それ以外は en. */
    if (!raw || !raw[0] || !raw[1]) return "en";
    if ((raw[0] == 'j' || raw[0] == 'J') &&
        (raw[1] == 'a' || raw[1] == 'A')) {
        return "ja";
    }
    return "en";
}
