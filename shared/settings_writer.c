/* settings_writer.c — コメント保持型 conf ライタ. */

#include "settings_writer.h"
#include "settings_schema.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <io.h>
#  define unlink _unlink
#else
#  include <unistd.h>
#endif

/* ---- dynamic byte buffer ---- */

typedef struct {
    char  *data;
    size_t len;
    size_t cap;
} buf_t;

static int buf_reserve(buf_t *b, size_t need) {
    size_t newcap;
    char *p;
    if (b->cap >= b->len + need) return 0;
    newcap = b->cap ? b->cap : 256;
    while (newcap < b->len + need) newcap *= 2;
    p = (char *)realloc(b->data, newcap);
    if (!p) return -1;
    b->data = p;
    b->cap = newcap;
    return 0;
}

static int buf_append(buf_t *b, const char *s, size_t n) {
    if (n == 0) return 0;
    if (buf_reserve(b, n) != 0) return -1;
    memcpy(b->data + b->len, s, n);
    b->len += n;
    return 0;
}

static int buf_append_str(buf_t *b, const char *s) {
    return buf_append(b, s, strlen(s));
}

static int buf_append_kv(buf_t *b, const char *key, const char *value) {
    if (buf_append_str(b, key) != 0) return -1;
    if (buf_append_str(b, " = ") != 0) return -1;
    if (buf_append_str(b, value) != 0) return -1;
    return buf_append_str(b, "\n");
}

/* ---- 行解析: key = value の "key" 部分を抽出. なければ NULL. ---- */

/* line は \n 抜きの 1 行 (NUL 終端). 戻り値: key を新規 malloc して返す.
 * key が抽出できなかった場合は NULL. NUL は line を破壊しない. */
static char *line_extract_key(const char *line) {
    const char *eq, *p, *ks, *ke;
    size_t klen;
    char *out;
    if (!line) return NULL;
    /* 先頭スペースを飛ばす. ただし '#' や空行はキーではない. */
    p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '\0' || *p == '#') return NULL;
    eq = strchr(p, '=');
    if (!eq) return NULL;
    /* key の範囲を確定 */
    ks = p;
    ke = eq;
    while (ke > ks && (ke[-1] == ' ' || ke[-1] == '\t')) ke--;
    if (ke == ks) return NULL;
    klen = (size_t)(ke - ks);
    out = (char *)malloc(klen + 1);
    if (!out) return NULL;
    memcpy(out, ks, klen);
    out[klen] = '\0';
    return out;
}

/* ---- ファイル読み込み (全文を malloc) ---- */

static char *read_file_all(const char *path, size_t *out_len) {
    FILE *fp;
    long sz;
    char *buf;
    size_t n;
    *out_len = 0;
    fp = fopen(path, "rb");
    if (!fp) return NULL;
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return NULL; }
    sz = ftell(fp);
    if (sz < 0) { fclose(fp); return NULL; }
    if (fseek(fp, 0, SEEK_SET) != 0) { fclose(fp); return NULL; }
    buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(fp); return NULL; }
    n = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);
    buf[n] = '\0';
    *out_len = n;
    return buf;
}

/* ---- メイン ---- */

int calcyx_settings_write_preserving(const char            *path,
                                     const char            *first_time_header,
                                     calcyx_setting_value_fn lookup,
                                     void                  *user) {
    char  *content;
    size_t content_len;
    int    n_table = 0, i;
    const calcyx_setting_desc_t *table;
    char  *seen = NULL;            /* per-entry: 0/1 (key only) */
    buf_t  out;
    size_t pos;
    char   tmp_path[1024];
    FILE  *fp;
    int    rc = 0;

    if (!path || !lookup) return -1;

    table = calcyx_settings_table(&n_table);
    if (!table || n_table <= 0) return -1;
    seen = (char *)calloc((size_t)n_table, 1);
    if (!seen) return -1;

    out.data = NULL; out.len = 0; out.cap = 0;

    content = read_file_all(path, &content_len);

    /* --- Pass 1: 既存行を走査して buf へ転写 --- */
    if (content && content_len > 0) {
        pos = 0;
        while (pos < content_len) {
            size_t eol = pos;
            size_t line_end;
            int    has_cr = 0;
            char   line_buf[1024];
            size_t line_len;
            char  *key;
            int    matched = 0;

            while (eol < content_len && content[eol] != '\n') eol++;
            line_end = eol;
            /* CR の除去 */
            if (line_end > pos && content[line_end - 1] == '\r') {
                line_end--;
                has_cr = 1;
            }
            (void)has_cr;
            line_len = line_end - pos;
            if (line_len >= sizeof(line_buf)) line_len = sizeof(line_buf) - 1;
            memcpy(line_buf, content + pos, line_len);
            line_buf[line_len] = '\0';

            key = line_extract_key(line_buf);
            if (key) {
                for (i = 0; i < n_table; i++) {
                    if (table[i].key && strcmp(table[i].key, key) == 0) {
                        char val[256];
                        int  ret = lookup(key, val, sizeof(val), user);
                        seen[i] = 1;
                        if (ret > 0) {
                            buf_append_kv(&out, key, val);
                        }
                        /* ret == 0: 行を削除 (color_* で preset != user 時). */
                        matched = 1;
                        break;
                    }
                }
                free(key);
            }
            if (!matched) {
                /* 元の行をそのまま転写. line_end は eol-1 (CR除去後) なので, content の
                 * pos..eol を memcpy + 末尾 \n を付ける. */
                buf_append(&out, content + pos, eol - pos);
                /* 元の改行を維持 (\n が見つかれば付与, 末尾 EOF なら付けない) */
                if (eol < content_len) {
                    buf_append(&out, "\n", 1);
                }
            }
            pos = (eol < content_len) ? eol + 1 : eol;
        }
    } else if (first_time_header) {
        /* ファイル新規作成時のみ短いヘッダを置く. */
        buf_append_str(&out, first_time_header);
    }

    /* --- Pass 2: 未出現の既知キーをセクション単位で末尾追記 --- */
    {
        i = 0;
        while (i < n_table) {
            if (table[i].kind == CALCYX_SETTING_KIND_SECTION) {
                int j, has_missing = 0;
                /* セクション範囲 [i+1, next-section) を見て missing がいるか確認 */
                int next = i + 1;
                while (next < n_table &&
                       table[next].kind != CALCYX_SETTING_KIND_SECTION) {
                    next++;
                }
                for (j = i + 1; j < next; j++) {
                    if (table[j].key && !seen[j]) {
                        char tmp_val[256];
                        int  ret = lookup(table[j].key, tmp_val, sizeof(tmp_val), user);
                        if (ret > 0) { has_missing = 1; break; }
                    }
                }
                if (has_missing) {
                    /* セクションヘッダ: 出力末尾が改行で終わっていなければ
                     * 改行を入れてから "\n" + section */
                    if (out.len > 0 && out.data[out.len - 1] != '\n') {
                        buf_append(&out, "\n", 1);
                    }
                    buf_append(&out, "\n", 1);
                    if (table[i].section) {
                        buf_append_str(&out, table[i].section);
                    }
                    /* セクション内のミッシングキーを順に出力 */
                    for (j = i + 1; j < next; j++) {
                        if (table[j].key && !seen[j]) {
                            char val[256];
                            int  ret = lookup(table[j].key, val, sizeof(val), user);
                            if (ret > 0) {
                                buf_append_kv(&out, table[j].key, val);
                            }
                        }
                    }
                }
                i = next;
            } else {
                i++;
            }
        }
    }

    free(content);

    /* --- atomic 書き出し: path.tmp に書いて rename --- */
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    fp = fopen(tmp_path, "wb");
    if (!fp) {
        rc = -1;
    } else {
        if (out.len > 0) {
            if (fwrite(out.data, 1, out.len, fp) != out.len) rc = -1;
        }
        if (fclose(fp) != 0) rc = -1;
    }
    if (rc == 0) {
#ifdef _WIN32
        /* Windows の rename は dest が存在すると失敗するので, 先に消す. */
        unlink(path);
#endif
        if (rename(tmp_path, path) != 0) {
            /* fallback: tmp をそのまま正本にできなかった -> 直接コピー */
            FILE *src = fopen(tmp_path, "rb");
            FILE *dst = fopen(path, "wb");
            if (src && dst) {
                char chunk[4096];
                size_t k;
                while ((k = fread(chunk, 1, sizeof(chunk), src)) > 0) {
                    if (fwrite(chunk, 1, k, dst) != k) { rc = -1; break; }
                }
            } else {
                rc = -1;
            }
            if (src) fclose(src);
            if (dst) fclose(dst);
            unlink(tmp_path);
        }
    } else {
        unlink(tmp_path);
    }

    free(seen);
    free(out.data);
    return rc;
}

/* ---- defaults lookup ---- */

static int defaults_lookup(const char *key, char *buf, size_t buflen, void *user) {
    const calcyx_setting_desc_t *d;
    (void)user;
    d = calcyx_settings_find(key);
    if (!d) return 0;
    switch (d->kind) {
    case CALCYX_SETTING_KIND_BOOL:
        snprintf(buf, buflen, "%s", d->b_def ? "true" : "false");
        return 1;
    case CALCYX_SETTING_KIND_INT:
        snprintf(buf, buflen, "%d", d->i_def);
        return 1;
    case CALCYX_SETTING_KIND_FONT:
    case CALCYX_SETTING_KIND_HOTKEY:
    case CALCYX_SETTING_KIND_COLOR_PRESET:
    case CALCYX_SETTING_KIND_STRING:
        snprintf(buf, buflen, "%s", d->s_def ? d->s_def : "");
        return 1;
    case CALCYX_SETTING_KIND_COLOR:
        /* defaults は color_preset = 非 user なので color_* は出力しない. */
        return 0;
    default:
        return 0;
    }
}

int calcyx_settings_init_defaults(const char *path, const char *first_time_header) {
    FILE *fp;
    int   rc;
    if (!path) return -1;
    fp = fopen(path, "rb");
    if (fp) { fclose(fp); return 0; }  /* 既存. */
    rc = calcyx_settings_write_preserving(path, first_time_header,
                                          defaults_lookup, NULL);
    return (rc == 0) ? 1 : -1;
}
