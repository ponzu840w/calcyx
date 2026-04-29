/* settings_writer の単体テスト (FLTK 非依存、 engine ラベル ctest)。
 * lookup 戻り値: 1=PROVIDED, -1=LEAVE. is_default=1 で commented (#key=val)。
 * 検証: コメント保持 / 未知キー保持 / 順序保持 / commented↔uncommented
 * のラウンドトリップ / -1 で行非破壊 / 初回 header / 末尾追記。 */

#include "settings_writer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_pass = 0;
static int g_fail = 0;

static void check(const char *label, int ok) {
    if (ok) { printf("  PASS: %s\n", label); g_pass++; }
    else    { printf("  FAIL: %s\n", label); g_fail++; }
}

/* ---- helpers ---- */

static char *read_all(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(fp); return NULL; }
    fread(buf, 1, (size_t)sz, fp);
    buf[sz] = '\0';
    fclose(fp);
    return buf;
}

static void write_all(const char *path, const char *body) {
    FILE *fp = fopen(path, "wb");
    fputs(body, fp);
    fclose(fp);
}

static int contains(const char *hay, const char *needle) {
    return strstr(hay, needle) != NULL;
}

/* hay の中で needle1 が needle2 より前に現れるか。 両方が必須。 */
static int order_ok(const char *hay, const char *needle1, const char *needle2) {
    const char *p1 = strstr(hay, needle1);
    const char *p2 = strstr(hay, needle2);
    return p1 && p2 && p1 < p2;
}

/* hay 中に「行頭」で needle が現れるか。 needle は \n を含まないこと。 */
static int contains_at_line_start(const char *hay, const char *needle) {
    const char *p = hay;
    size_t nlen = strlen(needle);
    while ((p = strstr(p, needle)) != NULL) {
        if (p == hay || p[-1] == '\n') return 1;
        p += nlen;
    }
    return 0;
}

/* ---- lookup callbacks ---- */

/* シンプルな lookup: 主要キーに固定値を返す。 その他は -1 (LEAVE)。
 * is_default = 0 なので writer は素の "key = value" (uncommented) を出す。 */
static int lookup_basic(const char *key, char *buf, size_t buflen,
                        int *out_is_default, void *user) {
    (void)user;
    if (out_is_default) *out_is_default = 0;
    if (strcmp(key, "decimal_digits") == 0)      { snprintf(buf, buflen, "12"); return 1; }
    if (strcmp(key, "auto_completion") == 0)     { snprintf(buf, buflen, "true"); return 1; }
    if (strcmp(key, "auto_close_brackets") == 0) { snprintf(buf, buflen, "false"); return 1; }
    if (strcmp(key, "max_array_length") == 0)    { snprintf(buf, buflen, "256"); return 1; }
    if (strcmp(key, "max_string_length") == 0)   { snprintf(buf, buflen, "256"); return 1; }
    if (strcmp(key, "max_call_depth") == 0)      { snprintf(buf, buflen, "64"); return 1; }
    if (strcmp(key, "color_preset") == 0)        { snprintf(buf, buflen, "otaku-black"); return 1; }
    /* 該当しないキーは管轄外 (LEAVE) */
    return -1;
}

/* user-defined preset 想定: color_preset = user-defined, 一部 color_* を提供。 */
static int lookup_user_colors(const char *key, char *buf, size_t buflen,
                              int *out_is_default, void *user) {
    (void)user;
    if (out_is_default) *out_is_default = 0;
    if (strcmp(key, "color_preset") == 0) { snprintf(buf, buflen, "user-defined"); return 1; }
    if (strcmp(key, "color_bg") == 0)     { snprintf(buf, buflen, "#102030"); return 1; }
    if (strcmp(key, "color_text") == 0)   { snprintf(buf, buflen, "#ffffff"); return 1; }
    return -1;
}

/* lookup が is_default=1 を返すケース: 値はデフォルト同等なので commented で書く。 */
static int lookup_default_marker(const char *key, char *buf, size_t buflen,
                                 int *out_is_default, void *user) {
    (void)user;
    if (strcmp(key, "decimal_digits") == 0) {
        snprintf(buf, buflen, "9");
        if (out_is_default) *out_is_default = 1;
        return 1;
    }
    return -1;
}

/* ---- tests ---- */

static const char *TMPDIR = "/tmp";

static void mkpath(char *out, size_t n, const char *name) {
    snprintf(out, n, "%s/%s", TMPDIR, name);
}

/* T1: 手書きコメント・未知キー・並び順が保持され、 既存値だけ in-place 更新される */
static void test_preserve_comments(void) {
    char path[256];
    mkpath(path, sizeof(path), "calcyx_test_writer_T1.conf");
    const char *body =
        "# my custom comment\n"
        "decimal_digits = 6\n"
        "wibble_unknown = 42\n"
        "\n"
        "# section sep\n"
        "auto_completion = false\n";
    write_all(path, body);

    int rc = calcyx_settings_write_preserving(path, NULL, lookup_basic, NULL);
    check("T1 rc", rc == 0);

    char *out = read_all(path);
    check("T1 user comment preserved", contains(out, "# my custom comment\n"));
    check("T1 user comment2 preserved", contains(out, "# section sep\n"));
    check("T1 unknown key preserved", contains(out, "wibble_unknown = 42\n"));
    check("T1 decimal_digits updated in place",
          contains(out, "decimal_digits = 12\n") && !contains(out, "decimal_digits = 6"));
    check("T1 auto_completion updated", contains(out, "auto_completion = true\n"));
    check("T1 order: decimal_digits before wibble",
          order_ok(out, "decimal_digits", "wibble_unknown"));
    check("T1 order: wibble before auto_completion",
          order_ok(out, "wibble_unknown", "auto_completion"));
    free(out);
    remove(path);
}

/* T2: 初回生成 (空ファイル) で first_time_header が先頭に出る */
static void test_first_time_header(void) {
    char path[256];
    mkpath(path, sizeof(path), "calcyx_test_writer_T2.conf");
    remove(path);

    const char *header = "# auto generated header\n";
    int rc = calcyx_settings_write_preserving(path, header, lookup_basic, NULL);
    check("T2 rc", rc == 0);

    char *out = read_all(path);
    check("T2 header at top", out && strncmp(out, header, strlen(header)) == 0);
    check("T2 missing keys appended", contains(out, "decimal_digits = 12\n"));
    check("T2 section header for missing keys", contains(out, "# ---- "));
    free(out);
    remove(path);
}

/* T3: lookup が is_default=1 を返したとき、 既存の uncommented 行は
 *     '#key = value' (commented) に変換される。 (= ユーザーが値をデフォルトに
 *      戻した、 あるいは preset 切替で color_* が default 同等になったケース) */
static void test_default_marker_comments_line(void) {
    char path[256];
    mkpath(path, sizeof(path), "calcyx_test_writer_T3.conf");
    const char *body = "decimal_digits = 6\n";
    write_all(path, body);

    int rc = calcyx_settings_write_preserving(path, NULL,
                                              lookup_default_marker, NULL);
    check("T3 rc", rc == 0);

    char *out = read_all(path);
    check("T3 default value commented",
          contains(out, "#decimal_digits = 9\n"));
    check("T3 old uncommented form gone",
          !contains_at_line_start(out, "decimal_digits = 6"));
    free(out);
    remove(path);
}

/* T3b: 既に commented な行 (#color_bg = ...) で lookup が is_default=0 を返す場合、
 * uncomment されて key = value 形式になる (再度 user-defined に切替えるシナリオ)。 */
static void test_commented_uncomments_when_value_returned(void) {
    char path[256];
    mkpath(path, sizeof(path), "calcyx_test_writer_T3b.conf");
    const char *body =
        "#color_bg = #ababab\n"
        "decimal_digits = 6\n";
    write_all(path, body);

    int rc = calcyx_settings_write_preserving(path, NULL,
                                              lookup_user_colors, NULL);
    check("T3b rc", rc == 0);

    char *out = read_all(path);
    check("T3b color_bg uncommented with new value",
          contains_at_line_start(out, "color_bg = #102030\n"));
    check("T3b old commented form gone",
          !contains(out, "#color_bg = #ababab"));
    free(out);
    remove(path);
}

/* T4: user-defined preset では color_* 行が末尾追記される */
static void test_user_preset_emits_colors(void) {
    char path[256];
    mkpath(path, sizeof(path), "calcyx_test_writer_T4.conf");
    remove(path);

    int rc = calcyx_settings_write_preserving(path, NULL, lookup_user_colors, NULL);
    check("T4 rc", rc == 0);

    char *out = read_all(path);
    check("T4 color_preset emitted", contains(out, "color_preset = user-defined\n"));
    check("T4 color_bg emitted", contains(out, "color_bg = #102030\n"));
    check("T4 color_text emitted", contains(out, "color_text = #ffffff\n"));
    /* lookup が 0 を返したキー (color_link 等) は出力されない */
    check("T4 unset color_* not emitted", !contains(out, "color_link"));
    free(out);
    remove(path);
}

/* T4b: lookup が -1 (LEAVE) を返したキーは元の行をそのまま転写し、
 * 上書きも commenting も追記もされない。 GUI 保存で TUI 専用キー
 * (tui_color_source 等) が破壊されないことを担保する重要なシナリオ。 */
static int lookup_leave_decimal(const char *key, char *buf, size_t buflen,
                                 int *out_is_default, void *user) {
    (void)buf; (void)buflen; (void)user;
    if (out_is_default) *out_is_default = 0;
    /* decimal_digits は管轄外として LEAVE. それ以外も全部 LEAVE. */
    if (strcmp(key, "decimal_digits") == 0) return -1;
    return -1;
}

static void test_leave_preserves_line_verbatim(void) {
    char path[256];
    mkpath(path, sizeof(path), "calcyx_test_writer_T4b.conf");
    /* 行頭の余分な空白も含めて完全に転写されるかは保証しないが、 値と
     * commented/uncommented 状態は保持されるべき。 */
    const char *body =
        "decimal_digits = 6\n"
        "#auto_completion = false\n";
    write_all(path, body);

    int rc = calcyx_settings_write_preserving(path, NULL,
                                              lookup_leave_decimal, NULL);
    check("T4b rc", rc == 0);

    char *out = read_all(path);
    check("T4b uncommented value preserved verbatim",
          contains(out, "decimal_digits = 6\n"));
    check("T4b not modified to default",
          !contains(out, "decimal_digits = 9"));
    check("T4b commented form preserved verbatim",
          contains(out, "#auto_completion = false\n"));
    free(out);
    remove(path);
}

/* T5: 既存ファイルの先頭に first_time_header が無ければ挿入される
 * (ユーザがヘッダ行だけ削除したケースの復活)。 */
static void test_header_injected_when_missing(void) {
    char path[256];
    mkpath(path, sizeof(path), "calcyx_test_writer_T5.conf");
    write_all(path, "decimal_digits = 6\n");

    int rc = calcyx_settings_write_preserving(path, "# brand new\n",
                                              lookup_basic, NULL);
    check("T5 rc", rc == 0);

    char *out = read_all(path);
    check("T5 header injected at top", strncmp(out, "# brand new\n", 12) == 0);
    /* 既存内容は転写される (lookup_basic で 6 → 12 に書き換わる)。 */
    check("T5 existing content kept",  contains(out, "decimal_digits = 12\n"));
    free(out);
    remove(path);
}

/* T5b: 既存ファイル先頭が first_time_header と一致するなら重複挿入しない。 */
static void test_header_not_duplicated(void) {
    char path[256];
    mkpath(path, sizeof(path), "calcyx_test_writer_T5b.conf");
    write_all(path, "# brand new\ndecimal_digits = 6\n");

    int rc = calcyx_settings_write_preserving(path, "# brand new\n",
                                              lookup_basic, NULL);
    check("T5b rc", rc == 0);

    char *out = read_all(path);
    /* 「# brand new\n」 が 1 回しか出ない */
    const char *p1 = strstr(out, "# brand new\n");
    int duplicated = p1 && strstr(p1 + 1, "# brand new\n");
    check("T5b header not duplicated", !duplicated);
    free(out);
    remove(path);
}

int main(void) {
    test_preserve_comments();
    test_first_time_header();
    test_default_marker_comments_line();
    test_commented_uncomments_when_value_returned();
    test_user_preset_emits_colors();
    test_leave_preserves_line_verbatim();
    test_header_injected_when_missing();
    test_header_not_duplicated();

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
