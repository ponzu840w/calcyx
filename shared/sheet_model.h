/* シート全体のモデル層。
 * 行データ (expr / result / fmt / error / visible) と評価コンテキスト、
 * undo/redo スタック、補完候補、ファイル I/O を一体で管理する。
 *
 * UI 非依存 (FLTK / FTXUI / JS いずれにも依存しない C99)。
 * GUI の SheetView と TUI の TuiSheet が同じ model を共有して動作する。
 * 移植元: ui/SheetView.cpp (rows_ / ctx_ / undo_buf_ 回りを C へ抽出). */

#ifndef CALCYX_SHARED_SHEET_MODEL_H
#define CALCYX_SHARED_SHEET_MODEL_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* engine ヘッダ群は素の C なので extern "C" ブロック内で取り込む。
 * 外側に出すと、eval_ctx.h → parser/expr.h → parser/token.h 経由で
 * tok_free 等が C++ リンケージで先取りされてしまい、C++ フロントエンド
 * から呼び出したときに undefined reference になる。 */
#include "types/val.h"
#include "eval/eval_ctx.h"

typedef struct sheet_model sheet_model_t;

/* ------------------------------------------------------------------ */
/* ビュー状態スナップショット                                           */
/* ------------------------------------------------------------------ */
/* model は view を知らないが、undo/redo 時に焦点行・カーソル位置を
 * 復元するため、不透明な view_state をエントリに保存する。 */
typedef struct {
    int focused_row;
    int cursor_pos;
} sheet_view_state_t;

/* ------------------------------------------------------------------ */
/* undo 用オペレーション                                                 */
/* ------------------------------------------------------------------ */
typedef enum {
    SHEET_OP_INSERT,      /* idx 位置に expr で新規行を挿入 */
    SHEET_OP_DELETE,      /* idx 位置の行を削除 (expr は無視) */
    SHEET_OP_CHANGE_EXPR  /* idx 位置の行を expr に変更 */
} sheet_op_type_t;

typedef struct {
    sheet_op_type_t type;
    int             index;
    const char     *expr;  /* DELETE のとき NULL 可。model が複製して保持する */
} sheet_op_t;

/* ------------------------------------------------------------------ */
/* 補完候補                                                             */
/* ------------------------------------------------------------------ */
/* 文字列は model が所有する (strdup)。再ビルドまたは free までは有効。 */
typedef struct {
    const char *id;           /* 挿入テキスト */
    const char *label;        /* 表示ラベル ("sin(x)" など) */
    const char *description;  /* ドキュメント (NULL 可) */
    bool        is_function;
} sheet_candidate_t;

/* ------------------------------------------------------------------ */
/* 評価リミット (settings_globals の g_limit_* に相当)                  */
/* ------------------------------------------------------------------ */
typedef struct {
    int max_array_length;
    int max_string_length;
    int max_call_depth;
} sheet_eval_limits_t;

/* ------------------------------------------------------------------ */
/* 変更通知                                                             */
/* ------------------------------------------------------------------ */
typedef void (*sheet_model_change_cb)(void *userdata);

/* ================================================================== */
/* ライフサイクル                                                       */
/* ================================================================== */
sheet_model_t *sheet_model_new (void);
void           sheet_model_free(sheet_model_t *m);

/* eval_all の直前に ctx へ反映される値。Prefs 変更時に都度呼び出す。 */
void sheet_model_set_limits(sheet_model_t *m, sheet_eval_limits_t limits);

void sheet_model_set_change_cb(sheet_model_t *m,
                                sheet_model_change_cb cb, void *userdata);

/* ================================================================== */
/* 行データ読み取り                                                     */
/* ================================================================== */
int         sheet_model_row_count  (const sheet_model_t *m);
const char *sheet_model_row_expr   (const sheet_model_t *m, int idx);
const char *sheet_model_row_result (const sheet_model_t *m, int idx);
val_fmt_t   sheet_model_row_fmt    (const sheet_model_t *m, int idx);
bool        sheet_model_row_error  (const sheet_model_t *m, int idx);
/* result と "=" を表示するか (代入/def は false)。 */
bool        sheet_model_row_visible(const sheet_model_t *m, int idx);

/* ================================================================== */
/* 行データ書き込み                                                     */
/* ================================================================== */
/* 非 undo 系: 編集途中の live_eval で使う。undo スタックを触らない。 */
void sheet_model_set_row_expr_raw(sheet_model_t *m, int idx, const char *expr);

/* 行全体を exprs[] (n 要素) で置換。undo スタックをクリアする。
 * preview / load_file / テスト初期化用。 */
void sheet_model_set_rows(sheet_model_t *m, const char *const *exprs, int n);

/* 評価: 全行を ctx で再評価し、result / fmt / error / visible を更新。 */
void sheet_model_eval_all(sheet_model_t *m);

/* ================================================================== */
/* undo 用コミット API                                                  */
/* ================================================================== */
/* 複合 undo エントリを push し、redo_ops を実際に適用して eval_all を呼ぶ。
 * - undo_ops: undo したとき適用する操作列 (先頭から順に)
 * - redo_ops: redo したとき適用する操作列 (先頭から順に)
 * - view_state: undo 時に復元する焦点行・カーソル
 * 呼び出し側で entry を完全に組み立てる責務がある (move/clear などで
 * 非対称な op 列になる)。 */
void sheet_model_commit(sheet_model_t *m,
                         const sheet_op_t *undo_ops, int undo_n,
                         const sheet_op_t *redo_ops, int redo_n,
                         sheet_view_state_t view_state);

/* ================================================================== */
/* Undo / Redo                                                         */
/* ================================================================== */
bool sheet_model_can_undo(const sheet_model_t *m);
bool sheet_model_can_redo(const sheet_model_t *m);

/* 復元すべき view_state を out_vs に格納 (NULL 可)。
 * 何もなければ false。 */
bool sheet_model_undo(sheet_model_t *m, sheet_view_state_t *out_vs);
bool sheet_model_redo(sheet_model_t *m, sheet_view_state_t *out_vs);

void sheet_model_clear_undo(sheet_model_t *m);

/* ================================================================== */
/* 補完候補                                                             */
/* ================================================================== */
/* 内部ストレージを再構築し先頭ポインタを *out_arr に返す。要素数を返り値で
 * 返す。アルファベット順にソート済み。 */
int sheet_model_build_candidates(sheet_model_t *m,
                                  const sheet_candidate_t **out_arr);

/* ================================================================== */
/* ファイル I/O                                                         */
/* ================================================================== */
/* 各行を 1 expr として読み込み、undo を初期化して eval_all する。 */
bool sheet_model_load_file(sheet_model_t *m, const char *path);
/* 各行を改行区切りで書き出す (result は保存しない)。 */
bool sheet_model_save_file(const sheet_model_t *m, const char *path);

/* ================================================================== */
/* ユーティリティ                                                       */
/* ================================================================== */
/* idx 行に fmt ラッパー (hex/bin/oct/dec/si/kibi/char) があれば関数名を
 * 返す。なければ NULL。Auto 判定用。 */
const char *sheet_model_current_fmt_name(const sheet_model_t *m, int idx);

/* expr から fmt ラッパーを剥がす (例: "hex(1+1)" → "1+1")。
 * 返値は malloc された新しい文字列 (呼び出し側が free)。 */
char *sheet_model_strip_formatter(const char *expr);

/* シート全体を "expr = result\n" 形式でスナップショット。crash_handler 用。
 * 返値は malloc された文字列 (呼び出し側が free)。 */
char *sheet_model_build_snapshot(const sheet_model_t *m);

#ifdef __cplusplus
}
#endif

#endif /* CALCYX_SHARED_SHEET_MODEL_H */
