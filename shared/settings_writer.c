/* コメント保持型 conf ライタ。 ファイル I/O は path_utf8 経由。 */

#include "settings_writer.h"
#include "settings_schema.h"
#include "path_utf8.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* pos に n バイトを挿入。 成功 0, 失敗 -1. */
static int buf_insert(buf_t *b, size_t pos, const char *data, size_t n) {
    if (n == 0) return 0;
    if (pos > b->len) return -1;
    if (buf_reserve(b, n) != 0) return -1;
    memmove(b->data + pos + n, b->data + pos, b->len - pos);
    memcpy(b->data + pos, data, n);
    b->len += n;
    return 0;
}

/* hay の中から needle を探す。 見つかれば hay 内のオフセット、 無ければ
 * (size_t)-1. needle_len = 0 のときは 0 を返す。 */
static size_t find_substring(const char *hay, size_t hay_len,
                             const char *needle, size_t needle_len) {
    size_t i;
    if (needle_len == 0) return 0;
    if (hay_len < needle_len) return (size_t)-1;
    for (i = 0; i + needle_len <= hay_len; i++) {
        if (memcmp(hay + i, needle, needle_len) == 0) return i;
    }
    return (size_t)-1;
}

static int buf_append_kv(buf_t *b, const char *key, const char *value) {
    if (buf_append_str(b, key) != 0) return -1;
    if (buf_append_str(b, " = ") != 0) return -1;
    if (buf_append_str(b, value) != 0) return -1;
    return buf_append_str(b, "\n");
}

/* 行から key を抽出。 'key=value' → key + commented=0,
 * '#key=value' → key + commented=1, それ以外 → NULL.
 * 戻り値 key は呼び出し側で free. out_commented は NULL 可。 */
static char *line_extract_key(const char *line, int *out_commented) {
    const char *eq, *p, *ks, *ke;
    size_t klen;
    char *out;
    if (out_commented) *out_commented = 0;
    if (!line) return NULL;
    p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '\0') return NULL;
    if (*p == '#') {
        /* writer が出力する '#<key> = <value>' 形式を識別。
         * '# 自由文', '## ', '#\0' 等は通常コメントとして扱い NULL を返す。 */
        const char *q = p + 1;
        if (*q == '\0' || *q == ' ' || *q == '\t' || *q == '#') return NULL;
        if (out_commented) *out_commented = 1;
        p = q;
    }
    eq = strchr(p, '=');
    if (!eq) return NULL;
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

/* "key = value\n" または "#key = value\n" を出力。 is_commented は
 * デフォルト値のときコメントアウトするかどうか。 */
static int buf_append_kv_form(buf_t *b, const char *key, const char *value,
                              int is_commented) {
    if (is_commented) {
        if (buf_append_str(b, "#") != 0) return -1;
    }
    return buf_append_kv(b, key, value);
}

/* width 桁を目安に word-wrap して各行を "# <text>\n" として buf に追記。
 * text 中の '\n' は段落区切りとして尊重する。 width は "# " を除いた可視文字数。
 * 単語境界 (空白) で折り返し、 単語単独で width を超える場合は強制改行。 */
static int buf_append_wrapped_comment(buf_t *b, const char *text, size_t width) {
    const char *p;
    if (!text || !*text) return 0;
    p = text;
    while (*p) {
        const char *line_start = p;
        const char *last_space = NULL;
        const char *line_end;
        size_t len = 0;
        while (*p && *p != '\n' && len < width) {
            if (*p == ' ') last_space = p;
            p++;
            len++;
        }
        if (*p == '\0' || *p == '\n') {
            line_end = p;
            if (*p == '\n') p++;
        } else if (last_space && last_space > line_start) {
            line_end = last_space;
            p = last_space + 1;
        } else {
            /* 単語が width 超え or 行頭が空白なし → 強制改行。 */
            line_end = p;
        }
        if (buf_append_str(b, "# ") != 0) return -1;
        if (buf_append(b, line_start,
                       (size_t)(line_end - line_start)) != 0) return -1;
        if (buf_append_str(b, "\n") != 0) return -1;
    }
    return 0;
}

/* スキーマエントリのドキュメントコメントを出力。 形式:
 *   # <desc を 78 桁で word-wrap>
 *   # [SCOPE] default=<val> [range=lo..hi]
 * desc が NULL なら何も出さない (COLOR 系の節約)。 */
static int buf_append_doc(buf_t *b, const calcyx_setting_desc_t *d) {
    char meta[256];
    char scope_buf[32];
    size_t pos = 0;
    int    n;
    const char *scope_str;

    if (!d->desc) return 0;

    /* 1. desc を word-wrap で書き出し。 */
    if (buf_append_wrapped_comment(b, d->desc, 78) != 0) return -1;

    /* scope 文字列: CORE (全立ち)、 単一 [GUI/TUI/CLI], 複合 [GUI/TUI] etc. */
    if (d->scope == CALCYX_SETTING_SCOPE_CORE) {
        scope_str = "CORE";
    } else {
        size_t sp = 0;
        scope_buf[0] = '\0';
        if (d->scope & CALCYX_SETTING_SCOPE_GUI) {
            n = snprintf(scope_buf + sp, sizeof(scope_buf) - sp, "GUI");
            if (n > 0) sp += (size_t)n;
        }
        if (d->scope & CALCYX_SETTING_SCOPE_TUI) {
            n = snprintf(scope_buf + sp, sizeof(scope_buf) - sp,
                         "%sTUI", sp > 0 ? "/" : "");
            if (n > 0) sp += (size_t)n;
        }
        if (d->scope & CALCYX_SETTING_SCOPE_CLI) {
            n = snprintf(scope_buf + sp, sizeof(scope_buf) - sp,
                         "%sCLI", sp > 0 ? "/" : "");
            if (n > 0) sp += (size_t)n;
        }
        scope_str = sp > 0 ? scope_buf : "?";
    }
    n = snprintf(meta + pos, sizeof(meta) - pos, "[%s]", scope_str);
    if (n > 0) pos += (size_t)n;

    switch (d->kind) {
    case CALCYX_SETTING_KIND_BOOL:
        n = snprintf(meta + pos, sizeof(meta) - pos,
                     " default=%s", d->b_def ? "true" : "false");
        if (n > 0) pos += (size_t)n;
        break;
    case CALCYX_SETTING_KIND_INT:
        n = snprintf(meta + pos, sizeof(meta) - pos,
                     " default=%d range=%d..%d",
                     d->i_def, d->i_lo, d->i_hi);
        if (n > 0) pos += (size_t)n;
        break;
    case CALCYX_SETTING_KIND_FONT:
    case CALCYX_SETTING_KIND_HOTKEY:
    case CALCYX_SETTING_KIND_COLOR_PRESET:
    case CALCYX_SETTING_KIND_STRING:
        if (d->s_def && d->s_def[0]) {
            n = snprintf(meta + pos, sizeof(meta) - pos,
                         " default=%s", d->s_def);
            if (n > 0) pos += (size_t)n;
        }
        break;
    default:
        break;
    }
    if (pos >= sizeof(meta)) pos = sizeof(meta) - 1;

    if (pos > 0) {
        if (buf_append_str(b, "# ") != 0) return -1;
        if (buf_append(b, meta, pos) != 0) return -1;
        if (buf_append_str(b, "\n") != 0) return -1;
    }
    return 0;
}

/* ---- ファイル読み込み (全文を malloc) ---- */

static char *read_file_all(const char *path, size_t *out_len) {
    FILE *fp;
    long sz;
    char *buf;
    size_t n;
    *out_len = 0;
    fp = calcyx_fopen(path, "rb");
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

            {
                int was_commented = 0;
                key = line_extract_key(line_buf, &was_commented);
                if (key) {
                    for (i = 0; i < n_table; i++) {
                        if (table[i].key && strcmp(table[i].key, key) == 0) {
                            char val[256];
                            int  is_default = 0;
                            int  ret = lookup(key, val, sizeof(val),
                                              &is_default, user);
                            seen[i] = 1;
                            if (ret > 0) {
                                /* PROVIDED: 値で書き換え。 is_default なら
                                 * '#key = value' 形式 (commented) で書く。 */
                                int comment_now = is_default;
                                buf_append_kv_form(&out, key, val, comment_now);
                                matched = 1;
                            }
                            /* LEAVE: matched=0 のままで元行を転写。 seen[i]=1
                             * を立てると Pass 2 が重複追記しない。 */
                            break;
                        }
                    }
                    free(key);
                }
            }
            if (!matched) {
                /* 元の行をそのまま転写。 line_end は eol-1 (CR除去後) なので、 content の
                 * pos..eol を memcpy + 末尾 \n を付ける。 */
                buf_append(&out, content + pos, eol - pos);
                /* 元の改行を維持 (\n が見つかれば付与、 末尾 EOF なら付けない) */
                if (eol < content_len) {
                    buf_append(&out, "\n", 1);
                }
            }
            pos = (eol < content_len) ? eol + 1 : eol;
        }
    } else if (first_time_header) {
        /* ファイル新規作成時のみ短いヘッダを置く。 */
        buf_append_str(&out, first_time_header);
    }

    /* Pass 2: 未出現の既知キーをセクション内の本来の位置に挿入する。
     * 既存セクションヘッダ位置を Pass 1 後に scan で求めて、 各セクション末尾
     * (= 次セクションヘッダ直前) に追記。 */
    {
        size_t *section_pos = (size_t *)malloc((size_t)n_table * sizeof(size_t));
        if (!section_pos) {
            rc = -1;
        } else {
            int k;
            size_t scan_from = 0;
            /* 1. out 内のセクションヘッダ位置を schema 順に sequential search.
             *    見つからなかった (= Pass 1 で出てこなかった) ものは (size_t)-1. */
            for (k = 0; k < n_table; k++) section_pos[k] = (size_t)-1;
            for (k = 0; k < n_table; k++) {
                if (table[k].kind == CALCYX_SETTING_KIND_SECTION
                        && table[k].section) {
                    size_t hdr_len = strlen(table[k].section);
                    size_t off = find_substring(out.data + scan_from,
                                                out.len - scan_from,
                                                table[k].section, hdr_len);
                    if (off != (size_t)-1) {
                        section_pos[k] = scan_from + off;
                        scan_from = section_pos[k] + hdr_len;
                    }
                }
            }

            /* 2. セクションごとにミッシングキーの挿入 buf を作り、 適切な位置へ。 */
            k = 0;
            while (k < n_table && rc == 0) {
                int next_section, j, has_missing, has_seen;
                buf_t  add;
                size_t insert_pos;

                if (table[k].kind != CALCYX_SETTING_KIND_SECTION) { k++; continue; }

                next_section = n_table;
                for (j = k + 1; j < n_table; j++) {
                    if (table[j].kind == CALCYX_SETTING_KIND_SECTION) {
                        next_section = j; break;
                    }
                }

                has_missing = 0; has_seen = 0;
                for (j = k + 1; j < next_section; j++) {
                    if (!table[j].key) continue;
                    if (seen[j]) {
                        has_seen = 1;
                    } else {
                        char tmp_val[256];
                        int  tmp_def = 0;
                        int  ret = lookup(table[j].key, tmp_val, sizeof(tmp_val),
                                          &tmp_def, user);
                        if (ret > 0) has_missing = 1;
                    }
                }
                if (!has_missing) { k = next_section; continue; }

                /* 挿入位置 = 次セクションヘッダの直前 (= 現セクションの末尾)。
                 * 次セクションが out に無いか、 自身が最後のセクションなら out.len. */
                insert_pos = out.len;
                {
                    int m;
                    for (m = next_section; m < n_table; m++) {
                        if (table[m].kind == CALCYX_SETTING_KIND_SECTION
                                && section_pos[m] != (size_t)-1) {
                            insert_pos = section_pos[m];
                            break;
                        }
                    }
                }

                /* 挿入 buf を構築。 */
                add.data = NULL; add.len = 0; add.cap = 0;

                /* セクションヘッダが out に無いなら、 ここで前置。 */
                if (!has_seen && section_pos[k] == (size_t)-1
                        && table[k].section) {
                    /* 直前のバイトが \n でないなら改行付与。 視覚セパレータの空行を
                     * 1 行追加してからヘッダ。 */
                    if (insert_pos > 0 && out.data[insert_pos - 1] != '\n') {
                        buf_append(&add, "\n", 1);
                    }
                    /* 末尾追記時のみ視覚的セパレータの空行を入れる。 中間挿入では
                     * 既に直前に空行がある前提なので付けない。 */
                    if (insert_pos == out.len) {
                        buf_append(&add, "\n", 1);
                    }
                    buf_append_str(&add, table[k].section);
                }

                for (j = k + 1; j < next_section; j++) {
                    if (table[j].key && !seen[j]) {
                        char val[256];
                        int  is_default = 0;
                        int  ret = lookup(table[j].key, val, sizeof(val),
                                          &is_default, user);
                        if (ret > 0) {
                            buf_append_doc(&add, &table[j]);
                            buf_append_kv_form(&add, table[j].key, val,
                                               is_default);
                        }
                    }
                }

                /* 中間挿入の場合、 直後にくる次セクションヘッダとの間に
                 * 視覚的セパレータの空行を 1 行入れる。 */
                if (add.len > 0 && insert_pos < out.len) {
                    buf_append(&add, "\n", 1);
                }

                if (add.len > 0) {
                    if (buf_insert(&out, insert_pos, add.data, add.len) != 0) {
                        rc = -1;
                    } else {
                        /* 挿入後、 insert_pos 以降にあった section_pos を補正。 */
                        int m;
                        for (m = 0; m < n_table; m++) {
                            if (section_pos[m] != (size_t)-1
                                    && section_pos[m] >= insert_pos) {
                                section_pos[m] += add.len;
                            }
                        }
                    }
                }
                free(add.data);
                k = next_section;
            }
            free(section_pos);
        }
    }

    free(content);

    /* --- atomic 書き出し: path.tmp に書いて rename --- */
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    fp = calcyx_fopen(tmp_path, "wb");
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
        /* Windows の rename は dest が存在すると失敗するので、 先に消す。 */
        calcyx_remove(path);
#endif
        if (calcyx_rename(tmp_path, path) != 0) {
            /* fallback: tmp をそのまま正本にできなかった -> 直接コピー */
            FILE *src = calcyx_fopen(tmp_path, "rb");
            FILE *dst = calcyx_fopen(path, "wb");
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
            calcyx_remove(tmp_path);
        }
    } else {
        calcyx_remove(tmp_path);
    }

    free(seen);
    free(out.data);
    return rc;
}

/* ---- defaults lookup ---- */

static int defaults_lookup(const char *key, char *buf, size_t buflen,
                           int *out_is_default, void *user) {
    const calcyx_setting_desc_t *d;
    (void)user;
    if (out_is_default) *out_is_default = 1;
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
    case CALCYX_SETTING_KIND_COLOR: {
        /* 全キーが conf に書かれる前提のため、 COLOR も初期 default として
         * preset (otaku-black) 由来の色値を commented で書く。 */
        const char *def = calcyx_settings_color_default(key, "otaku-black");
        snprintf(buf, buflen, "%s", def ? def : "#000000");
        if (out_is_default) *out_is_default = 1;
        return 1;
    }
    default:
        return -1;
    }
}

int calcyx_settings_init_defaults(const char *path, const char *first_time_header) {
    FILE *fp;
    int   rc;
    if (!path) return -1;
    fp = calcyx_fopen(path, "rb");
    if (fp) { fclose(fp); return 0; }  /* 既存。 */
    rc = calcyx_settings_write_preserving(path, first_time_header,
                                          defaults_lookup, NULL);
    return (rc == 0) ? 1 : -1;
}
