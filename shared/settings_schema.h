/* settings_schema — calcyx.conf の設定項目スキーマ。
 *
 * GUI / TUI / CLI が同じテーブルを参照することで、key 名・並び順・
 * デフォルト値・範囲制約をひと所で管理する。FLTK 依存なし (C90)。
 *
 * 各クライアントは「キー → 自分の変数」のマッピングを別途持ち、
 * テーブル自体は pure data として共有する。 */

#ifndef CALCYX_SHARED_SETTINGS_SCHEMA_H
#define CALCYX_SHARED_SETTINGS_SCHEMA_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CALCYX_SETTING_KIND_SECTION,        /* セクションヘッダ (key/target なし) */
    CALCYX_SETTING_KIND_BOOL,
    CALCYX_SETTING_KIND_INT,            /* lo/hi で clamp */
    CALCYX_SETTING_KIND_FONT,           /* GUI のフォント名. TUI は無視 */
    CALCYX_SETTING_KIND_HOTKEY,         /* GUI のホットキー keyname. TUI は無視 */
    CALCYX_SETTING_KIND_COLOR_PRESET,   /* "otaku-black" 等 */
    CALCYX_SETTING_KIND_COLOR,          /* "#RRGGBB" */
    CALCYX_SETTING_KIND_STRING          /* enum/列挙の生文字列 (将来用) */
} calcyx_setting_kind_t;

/* 各キーが GUI / TUI のどちらに作用するかの bitmask. */
enum {
    CALCYX_SETTING_SCOPE_GUI  = 1 << 0,
    CALCYX_SETTING_SCOPE_TUI  = 1 << 1,
    CALCYX_SETTING_SCOPE_BOTH = (1 << 0) | (1 << 1)
};

typedef struct {
    int          kind;        /* calcyx_setting_kind_t */
    const char  *key;         /* SECTION では NULL */
    int          scope;       /* CALCYX_SETTING_SCOPE_* bitmask */

    /* 値の制約 / デフォルト. kind ごとに有効なフィールドが異なる. */
    int          i_lo;        /* INT 下限 */
    int          i_hi;        /* INT 上限 */
    int          i_def;       /* INT / COLOR_PRESET (= プリセット index) */
    int          b_def;       /* BOOL デフォルト (0/1) */
    const char  *s_def;       /* FONT/HOTKEY/STRING/COLOR_PRESET (id) */

    const char  *section;     /* SECTION ヘッダ本文 (改行とコメント込み) */
} calcyx_setting_desc_t;

/* スキーマの全エントリ (定義順 = conf 出力順). count != NULL なら *count に件数. */
const calcyx_setting_desc_t *calcyx_settings_table(int *out_count);

/* キー名検索. 見つからなければ NULL. */
const calcyx_setting_desc_t *calcyx_settings_find(const char *key);

/* COLOR エントリのプリセット依存デフォルトを #RRGGBB で返す.
 * preset_id は "otaku-black" / "gyakubari-white" / "saboten-grey" /
 * "saboten-white" / "user". 未知 preset / 未知 key は NULL. */
const char *calcyx_settings_color_default(const char *key,
                                          const char *preset_id);

#ifdef __cplusplus
}
#endif

#endif
