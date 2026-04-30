/* sheet_model の Undo / Redo / 行操作 / 補完候補を直接テストする。
 * 移植元: ui/test_undo.cpp (SheetView 経由) を C API で再実装。
 * FLTK 非依存のため engine テスト (CTest ラベル "engine") として登録。 */

#include "sheet_model.h"
#include "path_utf8.h"
#include "types/real.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

#ifdef _WIN32
#  include <process.h>  /* _getpid */
#  define get_pid _getpid
#else
#  include <unistd.h>   /* getpid */
#  define get_pid getpid
#endif

static int g_pass = 0;
static int g_fail = 0;

static void check(const char *label, int ok) {
    if (ok) { printf("  PASS: %s\n", label); g_pass++; }
    else    { printf("  FAIL: %s\n", label); g_fail++; }
}

/* 文字列比較ヘルパ */
static int eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}

/* row idx に old_expr → new_expr の変更を commit。
 * SheetView::commit() と同等 (original_expr_ = old_expr のとき)。 */
static void commit_change(sheet_model_t *m, int idx,
                           const char *old_expr, const char *new_expr) {
    sheet_op_t undo_op = { SHEET_OP_CHANGE_EXPR, idx, old_expr };
    sheet_op_t redo_op = { SHEET_OP_CHANGE_EXPR, idx, new_expr };
    sheet_view_state_t vs = { idx, 0 };
    sheet_model_commit(m, &undo_op, 1, &redo_op, 1, vs);
}

/* SheetView::insert_row(after) と同等。新規空行を after+1 に挿入。 */
static int insert_empty_row(sheet_model_t *m, int after) {
    int ins = after + 1;
    int n   = sheet_model_row_count(m);
    if (ins > n) ins = n;
    sheet_op_t undo_op = { SHEET_OP_DELETE, ins, "" };
    sheet_op_t redo_op = { SHEET_OP_INSERT, ins, "" };
    sheet_view_state_t vs = { after, 0 };
    sheet_model_commit(m, &undo_op, 1, &redo_op, 1, vs);
    return ins;
}

/* idx 行を削除 (SheetView::delete_row と同等)。 */
static void delete_row(sheet_model_t *m, int idx) {
    int n = sheet_model_row_count(m);
    if (n <= 1) {
        const char *cur = sheet_model_row_expr(m, 0);
        if (!cur[0]) return;
        commit_change(m, 0, cur, "");
        return;
    }
    const char *expr = sheet_model_row_expr(m, idx);
    char *saved = strdup(expr);
    sheet_op_t undo_op = { SHEET_OP_INSERT, idx, saved };
    sheet_op_t redo_op = { SHEET_OP_DELETE, idx, "" };
    sheet_view_state_t vs = { idx, 0 };
    sheet_model_commit(m, &undo_op, 1, &redo_op, 1, vs);
    free(saved);
}

/* 行 a と b (= a+1) をスワップ。 */
static void swap_rows(sheet_model_t *m, int a, int b) {
    const char *ea_src = sheet_model_row_expr(m, a);
    const char *eb_src = sheet_model_row_expr(m, b);
    char *ea = strdup(ea_src);
    char *eb = strdup(eb_src);
    sheet_op_t undo_ops[2] = {
        { SHEET_OP_CHANGE_EXPR, a, ea },
        { SHEET_OP_CHANGE_EXPR, b, eb },
    };
    sheet_op_t redo_ops[2] = {
        { SHEET_OP_CHANGE_EXPR, a, eb },
        { SHEET_OP_CHANGE_EXPR, b, ea },
    };
    sheet_view_state_t vs = { a, 0 };
    sheet_model_commit(m, undo_ops, 2, redo_ops, 2, vs);
    free(ea);
    free(eb);
}

/* ===== テストケース ===== */

/* 基本: 式を commit → Undo → Redo */
static void test_basic_commit_undo_redo(void) {
    printf("[test_basic_commit_undo_redo]\n");
    sheet_model_t *m = sheet_model_new();

    commit_change(m, 0, "", "1+1");
    check("commit: expr == '1+1'", eq(sheet_model_row_expr(m, 0), "1+1"));
    check("after commit: can_undo", sheet_model_can_undo(m));
    check("after commit: !can_redo", !sheet_model_can_redo(m));

    sheet_model_undo(m, NULL);
    check("after undo: expr == ''", eq(sheet_model_row_expr(m, 0), ""));
    check("after undo: !can_undo", !sheet_model_can_undo(m));
    check("after undo: can_redo", sheet_model_can_redo(m));

    sheet_model_redo(m, NULL);
    check("after redo: expr == '1+1'", eq(sheet_model_row_expr(m, 0), "1+1"));
    check("after redo: can_undo", sheet_model_can_undo(m));
    check("after redo: !can_redo", !sheet_model_can_redo(m));

    sheet_model_free(m);
}

/* 複数 commit を順に Undo / Redo */
static void test_multiple_commits(void) {
    printf("[test_multiple_commits]\n");
    sheet_model_t *m = sheet_model_new();

    commit_change(m, 0, "",    "aaa");
    commit_change(m, 0, "aaa", "bbb");
    commit_change(m, 0, "bbb", "ccc");
    check("expr == 'ccc'", eq(sheet_model_row_expr(m, 0), "ccc"));

    sheet_model_undo(m, NULL);
    check("undo1: expr == 'bbb'", eq(sheet_model_row_expr(m, 0), "bbb"));
    sheet_model_undo(m, NULL);
    check("undo2: expr == 'aaa'", eq(sheet_model_row_expr(m, 0), "aaa"));
    sheet_model_undo(m, NULL);
    check("undo3: expr == ''",    eq(sheet_model_row_expr(m, 0), ""));

    sheet_model_redo(m, NULL);
    check("redo1: expr == 'aaa'", eq(sheet_model_row_expr(m, 0), "aaa"));
    sheet_model_redo(m, NULL);
    check("redo2: expr == 'bbb'", eq(sheet_model_row_expr(m, 0), "bbb"));
    sheet_model_redo(m, NULL);
    check("redo3: expr == 'ccc'", eq(sheet_model_row_expr(m, 0), "ccc"));

    sheet_model_free(m);
}

/* Undo 後の新しい commit で redo 履歴がクリアされる */
static void test_redo_cleared_after_new_commit(void) {
    printf("[test_redo_cleared_after_new_commit]\n");
    sheet_model_t *m = sheet_model_new();

    commit_change(m, 0, "",    "aaa");
    commit_change(m, 0, "aaa", "bbb");
    sheet_model_undo(m, NULL);
    check("before fork: expr == 'aaa'", eq(sheet_model_row_expr(m, 0), "aaa"));
    check("before fork: can_redo", sheet_model_can_redo(m));

    commit_change(m, 0, "aaa", "ccc");  /* redo 履歴が消える */
    check("after fork: expr == 'ccc'", eq(sheet_model_row_expr(m, 0), "ccc"));
    check("after fork: !can_redo", !sheet_model_can_redo(m));

    sheet_model_undo(m, NULL);
    check("undo after fork: expr == 'aaa'", eq(sheet_model_row_expr(m, 0), "aaa"));

    sheet_model_free(m);
}

/* 行挿入の Undo / Redo */
static void test_insert_row_undo(void) {
    printf("[test_insert_row_undo]\n");
    sheet_model_t *m = sheet_model_new();

    commit_change(m, 0, "", "row0");
    check("row_count == 1", sheet_model_row_count(m) == 1);

    insert_empty_row(m, 0);
    check("after insert: row_count == 2", sheet_model_row_count(m) == 2);

    sheet_model_undo(m, NULL);
    check("undo insert: row_count == 1", sheet_model_row_count(m) == 1);
    check("undo insert: row0 expr intact", eq(sheet_model_row_expr(m, 0), "row0"));

    sheet_model_redo(m, NULL);
    check("redo insert: row_count == 2", sheet_model_row_count(m) == 2);

    sheet_model_free(m);
}

/* 行削除の Undo / Redo */
static void test_delete_row_undo(void) {
    printf("[test_delete_row_undo]\n");
    sheet_model_t *m = sheet_model_new();

    commit_change(m, 0, "", "row0");
    insert_empty_row(m, 0);
    commit_change(m, 1, "", "row1");
    check("setup: 2 rows", sheet_model_row_count(m) == 2);

    delete_row(m, 0);
    check("after delete: row_count == 1", sheet_model_row_count(m) == 1);
    check("after delete: row0 == 'row1'", eq(sheet_model_row_expr(m, 0), "row1"));

    sheet_model_undo(m, NULL);
    check("undo delete: row_count == 2", sheet_model_row_count(m) == 2);
    check("undo delete: row0 == 'row0'", eq(sheet_model_row_expr(m, 0), "row0"));
    check("undo delete: row1 == 'row1'", eq(sheet_model_row_expr(m, 1), "row1"));

    sheet_model_redo(m, NULL);
    check("redo delete: row_count == 1", sheet_model_row_count(m) == 1);
    check("redo delete: row0 == 'row1'", eq(sheet_model_row_expr(m, 0), "row1"));

    sheet_model_free(m);
}

/* 1 行だけのとき削除 → 空行化 */
static void test_delete_last_row(void) {
    printf("[test_delete_last_row]\n");
    sheet_model_t *m = sheet_model_new();

    commit_change(m, 0, "", "only");
    check("setup: 1 row", sheet_model_row_count(m) == 1);

    delete_row(m, 0);
    check("after delete: row_count still 1", sheet_model_row_count(m) == 1);
    check("after delete: expr == ''", eq(sheet_model_row_expr(m, 0), ""));

    sheet_model_undo(m, NULL);
    check("undo: expr == 'only'", eq(sheet_model_row_expr(m, 0), "only"));

    sheet_model_free(m);
}

/* 行スワップ (move_row_up / move_row_down 相当) */
static void test_move_row(void) {
    printf("[test_move_row]\n");
    sheet_model_t *m = sheet_model_new();

    commit_change(m, 0, "", "AAA");
    insert_empty_row(m, 0);
    commit_change(m, 1, "", "BBB");
    insert_empty_row(m, 1);
    commit_change(m, 2, "", "CCC");
    check("setup: [AAA,BBB,CCC]",
          eq(sheet_model_row_expr(m, 0), "AAA") &&
          eq(sheet_model_row_expr(m, 1), "BBB") &&
          eq(sheet_model_row_expr(m, 2), "CCC"));

    swap_rows(m, 1, 2);
    check("swap(1,2): [AAA,CCC,BBB]",
          eq(sheet_model_row_expr(m, 0), "AAA") &&
          eq(sheet_model_row_expr(m, 1), "CCC") &&
          eq(sheet_model_row_expr(m, 2), "BBB"));

    swap_rows(m, 0, 1);
    check("swap(0,1): [CCC,AAA,BBB]",
          eq(sheet_model_row_expr(m, 0), "CCC") &&
          eq(sheet_model_row_expr(m, 1), "AAA") &&
          eq(sheet_model_row_expr(m, 2), "BBB"));

    sheet_model_undo(m, NULL);
    check("undo x1: [AAA,CCC,BBB]",
          eq(sheet_model_row_expr(m, 0), "AAA") &&
          eq(sheet_model_row_expr(m, 1), "CCC") &&
          eq(sheet_model_row_expr(m, 2), "BBB"));
    sheet_model_undo(m, NULL);
    check("undo x2: [AAA,BBB,CCC]",
          eq(sheet_model_row_expr(m, 0), "AAA") &&
          eq(sheet_model_row_expr(m, 1), "BBB") &&
          eq(sheet_model_row_expr(m, 2), "CCC"));

    sheet_model_free(m);
}

/* clear_all 相当: 先頭行を空にして他を削除する複合 op */
static void test_clear_all(void) {
    printf("[test_clear_all]\n");
    sheet_model_t *m = sheet_model_new();

    commit_change(m, 0, "", "aaa");
    insert_empty_row(m, 0);
    commit_change(m, 1, "", "bbb");
    insert_empty_row(m, 1);
    commit_change(m, 2, "", "ccc");
    check("setup: 3 rows", sheet_model_row_count(m) == 3);

    /* redo: row0="" にして row1/row2 を削除 */
    /* undo: row0=aaa に戻し row1/row2 を復元 */
    sheet_op_t undo_ops[3] = {
        { SHEET_OP_CHANGE_EXPR, 0, "aaa" },
        { SHEET_OP_INSERT,      1, "bbb" },
        { SHEET_OP_INSERT,      2, "ccc" },
    };
    sheet_op_t redo_ops[3] = {
        { SHEET_OP_CHANGE_EXPR, 0, "" },
        { SHEET_OP_DELETE,      2, "" },
        { SHEET_OP_DELETE,      1, "" },
    };
    sheet_view_state_t vs = { 0, 0 };
    sheet_model_commit(m, undo_ops, 3, redo_ops, 3, vs);

    check("after clear: 1 row",   sheet_model_row_count(m) == 1);
    check("after clear: empty",   eq(sheet_model_row_expr(m, 0), ""));

    sheet_model_undo(m, NULL);
    check("undo: 3 rows",         sheet_model_row_count(m) == 3);
    check("undo: row0 == aaa",    eq(sheet_model_row_expr(m, 0), "aaa"));
    check("undo: row1 == bbb",    eq(sheet_model_row_expr(m, 1), "bbb"));
    check("undo: row2 == ccc",    eq(sheet_model_row_expr(m, 2), "ccc"));

    sheet_model_redo(m, NULL);
    check("redo: 1 row",  sheet_model_row_count(m) == 1);
    check("redo: empty",  eq(sheet_model_row_expr(m, 0), ""));

    sheet_model_free(m);
}

/* eval_all が式を評価して result に文字列を入れる */
static void test_eval_all(void) {
    printf("[test_eval_all]\n");
    sheet_model_t *m = sheet_model_new();

    const char *rows[] = { "1+2", "3*4" };
    sheet_model_set_rows(m, rows, 2);
    sheet_model_eval_all(m);

    check("eval: row0 result == '3'",  eq(sheet_model_row_result(m, 0), "3"));
    check("eval: row1 result == '12'", eq(sheet_model_row_result(m, 1), "12"));
    check("eval: row0 not error", !sheet_model_row_error(m, 0));
    check("eval: row0 visible",    sheet_model_row_visible(m, 0));

    /* エラー行 */
    const char *bad[] = { "1/0" };
    sheet_model_set_rows(m, bad, 1);
    sheet_model_eval_all(m);
    check("eval: div0 error", sheet_model_row_error(m, 0));

    sheet_model_free(m);
}

/* 補完候補に builtin 関数とキーワードが含まれる */
static void test_build_candidates(void) {
    printf("[test_build_candidates]\n");
    sheet_model_t *m = sheet_model_new();

    const sheet_candidate_t *arr = NULL;
    int n = sheet_model_build_candidates(m, &arr);
    check("candidates > 0", n > 0 && arr != NULL);

    int has_sin = 0, has_ans = 0, has_true = 0;
    for (int i = 0; i < n; i++) {
        if (eq(arr[i].id, "sin"))  has_sin  = 1;
        if (eq(arr[i].id, "ans"))  has_ans  = 1;
        if (eq(arr[i].id, "true")) has_true = 1;
    }
    check("has sin",  has_sin);
    check("has ans",  has_ans);
    check("has true", has_true);

    /* アルファベット順 */
    int sorted = 1;
    for (int i = 1; i < n; i++)
        if (strcmp(arr[i-1].id, arr[i].id) > 0) { sorted = 0; break; }
    check("sorted alphabetically", sorted);

    sheet_model_free(m);
}

/* strip_formatter / current_fmt_name */
static void test_fmt_helpers(void) {
    printf("[test_fmt_helpers]\n");
    sheet_model_t *m = sheet_model_new();

    commit_change(m, 0, "", "hex(0xff)");
    check("current_fmt: hex", eq(sheet_model_current_fmt_name(m, 0), "hex"));

    char *stripped = sheet_model_strip_formatter("hex(1+1)");
    check("strip hex", eq(stripped, "1+1"));
    free(stripped);

    stripped = sheet_model_strip_formatter("si( 42 )");
    check("strip si w/ spaces", eq(stripped, "42"));
    free(stripped);

    stripped = sheet_model_strip_formatter("1+2");
    check("strip non-wrapped", eq(stripped, "1+2"));
    free(stripped);

    sheet_model_free(m);
}

/* 日本語ディレクトリ + 日本語ファイル名で sheet_model_save_file /
 * sheet_model_load_file が round-trip するか検証する。
 *
 * Windows での "ポン酢" 等の日本語ユーザ名 / パスで calcyx_fopen が
 * 正しく _wfopen に橋渡しできているかを保証する回帰テスト。 Linux でも
 * UTF-8 path はそのまま動くので両プラットフォーム共通で実行される。
 *
 * 一時ディレクトリは:
 *   POSIX: $TMPDIR か /tmp
 *   Win  : %TEMP% か カレント
 * の下に "calcyx_jp_日本語_<pid>" を作成。 ファイル名は "テスト.txt".
 * テスト後はファイルだけ削除 (ディレクトリは OS の TMP 掃除に任せる)。 */
static void test_load_save_utf8_path(void) {
    printf("[test_load_save_utf8_path]\n");

    /* 一時ディレクトリの親 */
    char base[512] = "";
#ifdef _WIN32
    if (!calcyx_getenv_utf8("TEMP", base, sizeof(base)) &&
        !calcyx_getenv_utf8("TMP",  base, sizeof(base)))
        snprintf(base, sizeof(base), ".");
    const char *sep = "\\";
#else
    if (!calcyx_getenv_utf8("TMPDIR", base, sizeof(base)))
        snprintf(base, sizeof(base), "/tmp");
    const char *sep = "/";
#endif

    /* 日本語ディレクトリ "calcyx_jp_日本語_<pid>" */
    char dir[1024];
    snprintf(dir, sizeof(dir),
             "%s%scalcyx_jp_\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e_%d",
             base, sep, (int)get_pid());
    int rc = calcyx_mkdir(dir);
    /* mkdir は既存だと失敗するが round-trip 自体は通せばよいので無視. */
    (void)rc;

    /* "テスト.txt" */
    char path[1024];
    snprintf(path, sizeof(path),
             "%s%s\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88.txt", dir, sep);

    /* 1 行コミットしてから save → load round-trip. */
    sheet_model_t *m = sheet_model_new();
    commit_change(m, 0, "", "12*34");

    int saved = sheet_model_save_file(m, path);
    check("utf8 path: save_file ok", saved);

    sheet_model_t *m2 = sheet_model_new();
    int loaded = sheet_model_load_file(m2, path);
    check("utf8 path: load_file ok", loaded);
    check("utf8 path: round-trip row 0",
          eq(sheet_model_row_expr(m2, 0), "12*34"));

    sheet_model_free(m);
    sheet_model_free(m2);
    calcyx_remove(path);
}

/* LC_NUMERIC が "," 系ロケールに切替わった状態でも real_to_double が
 * 正しく "." 解釈で値を返すことを確認する。 real.c の局所ガードが効いて
 * いるかの回帰テスト。 de_DE.UTF-8 がシステムに install されていない
 * 環境では skip する (setlocale が NULL を返す)。 */
static void test_real_to_double_locale_safe(void) {
    printf("[test_real_to_double_locale_safe]\n");
    real_ctx_init();

    /* 現在の locale を保存. */
    char *prev = setlocale(LC_NUMERIC, NULL);
    char *prev_copy = prev ? strdup(prev) : NULL;

    /* "," 系ロケールに切替を試行。 失敗しても本テスト自体の意味はあるので
     * (= "C" で常に 1.5 になることだけは確認) 続ける。 */
    const char *comma_locales[] = {
        "de_DE.UTF-8", "fr_FR.UTF-8", "de_DE", "fr_FR", NULL
    };
    int locale_set = 0;
    for (int i = 0; comma_locales[i]; ++i) {
        if (setlocale(LC_NUMERIC, comma_locales[i])) {
            printf("  using locale: %s\n", comma_locales[i]);
            locale_set = 1;
            break;
        }
    }
    if (!locale_set) {
        printf("  SKIP: comma-decimal locale not installed, "
               "verifying default behavior only\n");
    }

    real_t r;
    real_init(&r);
    real_from_str(&r, "1.5");
    double v = real_to_double(&r);
    check("real_to_double('1.5') ~= 1.5 under non-C locale",
          v > 1.4 && v < 1.6);

    /* 復元. */
    if (prev_copy) {
        setlocale(LC_NUMERIC, prev_copy);
        free(prev_copy);
    }
}

int main(void) {
    test_basic_commit_undo_redo();
    test_multiple_commits();
    test_redo_cleared_after_new_commit();
    test_insert_row_undo();
    test_delete_row_undo();
    test_delete_last_row();
    test_move_row();
    test_clear_all();
    test_eval_all();
    test_build_candidates();
    test_fmt_helpers();
    test_load_save_utf8_path();
    test_real_to_double_locale_safe();

    printf("\n=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
