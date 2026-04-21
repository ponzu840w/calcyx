/* このファイルは builtin_array.c から分割された。
 * 編集時は builtin_array_internal.h のセクション境界に注意。 */

#include "builtin_array_internal.h"

/* --- 文字列関数 (移植元: StringFuncs.cs) --- */

val_t *bi_str(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    char buf[1024];
    if (a[0]->type == VAL_ARRAY) {
        /* 配列→文字列: 各要素を char コードとして連結 */
        size_t pos = 0;
        for (int i = 0; i < a[0]->arr_len && pos < sizeof(buf) - 4; i++) {
            int64_t c = val_as_long(a[0]->arr_items[i]);
            /* UTF-8 エンコード (最大4バイト) */
            if (c < 0x80) {
                buf[pos++] = (char)c;
            } else if (c < 0x800) {
                buf[pos++] = (char)(0xC0 | (c >> 6));
                buf[pos++] = (char)(0x80 | (c & 0x3F));
            } else if (c < 0x10000) {
                buf[pos++] = (char)(0xE0 | (c >> 12));
                buf[pos++] = (char)(0x80 | ((c >> 6) & 0x3F));
                buf[pos++] = (char)(0x80 | (c & 0x3F));
            } else {
                buf[pos++] = (char)(0xF0 | (c >> 18));
                buf[pos++] = (char)(0x80 | ((c >> 12) & 0x3F));
                buf[pos++] = (char)(0x80 | ((c >> 6) & 0x3F));
                buf[pos++] = (char)(0x80 | (c & 0x3F));
            }
        }
        buf[pos] = '\0';
        return val_new_str(buf);
    }
    val_to_str(a[0], buf, sizeof(buf));
    return val_new_str(buf);
}

/* array(str): 文字列 → char コードの配列 */
val_t *bi_array_str(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    if (a[0]->type != VAL_STR) return val_dup(a[0]);
    const unsigned char *s = (const unsigned char *)a[0]->str_v;
    /* UTF-8 デコード */
    val_t *tmp[4096];
    int cnt = 0;
    for (size_t i = 0; s[i] && cnt < 4096; ) {
        int64_t cp;
        if (s[i] < 0x80) {
            cp = s[i++];
        } else if ((s[i] & 0xE0) == 0xC0) {
            cp = (s[i] & 0x1F); i++;
            if (s[i]) { cp = (cp << 6) | (s[i] & 0x3F); i++; }
        } else if ((s[i] & 0xF0) == 0xE0) {
            cp = (s[i] & 0x0F); i++;
            for (int k = 0; k < 2 && s[i]; k++) { cp = (cp << 6) | (s[i] & 0x3F); i++; }
        } else {
            cp = (s[i] & 0x07); i++;
            for (int k = 0; k < 3 && s[i]; k++) { cp = (cp << 6) | (s[i] & 0x3F); i++; }
        }
        tmp[cnt++] = val_new_i64(cp, FMT_CHAR);
    }
    val_t *out = val_new_array(tmp, cnt, FMT_CHAR);
    for (int i = 0; i < cnt; i++) val_free(tmp[i]);
    return out;
}

val_t *bi_trim(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    if (a[0]->type != VAL_STR) return val_dup(a[0]);
    const char *s = a[0]->str_v;
    while (*s == ' ' || *s == '\t') s++;
    char buf[1024]; bia_str_copy(buf, s, sizeof(buf));
    int len = (int)strlen(buf);
    while (len > 0 && (buf[len-1] == ' ' || buf[len-1] == '\t')) buf[--len] = '\0';
    return val_new_str(buf);
}
val_t *bi_trimStart(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    if (a[0]->type != VAL_STR) return val_dup(a[0]);
    const char *s = a[0]->str_v;
    while (*s == ' ' || *s == '\t') s++;
    return val_new_str(s);
}
val_t *bi_trimEnd(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    if (a[0]->type != VAL_STR) return val_dup(a[0]);
    char buf[1024]; bia_str_copy(buf, a[0]->str_v, sizeof(buf));
    int len = (int)strlen(buf);
    while (len > 0 && (buf[len-1] == ' ' || buf[len-1] == '\t')) buf[--len] = '\0';
    return val_new_str(buf);
}

val_t *bi_toLower(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    if (a[0]->type != VAL_STR) return val_dup(a[0]);
    char buf[1024]; bia_str_copy(buf, a[0]->str_v, sizeof(buf));
    for (int i = 0; buf[i]; i++) buf[i] = (char)tolower((unsigned char)buf[i]);
    return val_new_str(buf);
}
val_t *bi_toUpper(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    if (a[0]->type != VAL_STR) return val_dup(a[0]);
    char buf[1024]; bia_str_copy(buf, a[0]->str_v, sizeof(buf));
    for (int i = 0; buf[i]; i++) buf[i] = (char)toupper((unsigned char)buf[i]);
    return val_new_str(buf);
}

val_t *bi_replace(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    if (a[0]->type != VAL_STR) return val_dup(a[0]);
    const char *s   = a[0]->str_v;
    char from[512] = "", to[512] = "";
    if (a[1]->type == VAL_STR) bia_str_copy(from, a[1]->str_v, sizeof(from));
    else val_to_str(a[1], from, sizeof(from));
    if (a[2]->type == VAL_STR) bia_str_copy(to, a[2]->str_v, sizeof(to));
    else val_to_str(a[2], to, sizeof(to));
    size_t flen = strlen(from), tlen = strlen(to);
    char buf[4096]; size_t pos = 0;
    while (*s && pos + tlen < sizeof(buf) - 1) {
        if (flen > 0 && strncmp(s, from, flen) == 0) {
            memcpy(buf + pos, to, tlen); pos += tlen; s += flen;
        } else {
            buf[pos++] = *s++;
        }
    }
    buf[pos] = '\0';
    return val_new_str(buf);
}

val_t *bi_startsWith(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    if (a[0]->type != VAL_STR) return val_new_bool(false);
    char prefix[512] = "";
    if (a[1]->type == VAL_STR) bia_str_copy(prefix, a[1]->str_v, sizeof(prefix));
    size_t plen = strlen(prefix);
    return val_new_bool(strncmp(a[0]->str_v, prefix, plen) == 0);
}
val_t *bi_endsWith(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    if (a[0]->type != VAL_STR) return val_new_bool(false);
    char suf[512] = "";
    if (a[1]->type == VAL_STR) bia_str_copy(suf, a[1]->str_v, sizeof(suf));
    size_t slen = strlen(a[0]->str_v), suflen = strlen(suf);
    if (suflen > slen) return val_new_bool(false);
    return val_new_bool(strcmp(a[0]->str_v + slen - suflen, suf) == 0);
}

val_t *bi_split(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    /* split(sep, str) */
    char sep[512] = "", src[4096] = "";
    if (a[0]->type == VAL_STR) bia_str_copy(sep, a[0]->str_v, sizeof(sep));
    if (a[1]->type == VAL_STR) bia_str_copy(src, a[1]->str_v, sizeof(src));
    size_t seplen = strlen(sep);
    val_t *tmp[1024];
    int cnt = 0;
    if (seplen == 0) {
        /* 空セパレータ: 1文字ずつ */
        for (size_t i = 0; src[i] && cnt < 1024; i++) {
            char ch[2] = { src[i], '\0' };
            tmp[cnt++] = val_new_str(ch);
        }
    } else {
        char *p = src, *found;
        while ((found = strstr(p, sep)) != NULL && cnt < 1023) {
            size_t len = (size_t)(found - p);
            char buf[1024]; memcpy(buf, p, len); buf[len] = '\0';
            tmp[cnt++] = val_new_str(buf);
            p = found + seplen;
        }
        tmp[cnt++] = val_new_str(p);
    }
    val_t *out = val_new_array(tmp, cnt, FMT_STRING);
    for (int i = 0; i < cnt; i++) val_free(tmp[i]);
    return out;
}

val_t *bi_join(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    /* join(sep, array) */
    char sep[512] = "";
    if (a[0]->type == VAL_STR) bia_str_copy(sep, a[0]->str_v, sizeof(sep));
    if (a[1]->type != VAL_ARRAY) return val_new_str("");
    char buf[4096]; size_t pos = 0;
    size_t seplen = strlen(sep);
    for (int i = 0; i < a[1]->arr_len; i++) {
        char elem[1024] = "";
        if (a[1]->arr_items[i]->type == VAL_STR)
            bia_str_copy(elem, a[1]->arr_items[i]->str_v, sizeof(elem));
        else
            val_to_str(a[1]->arr_items[i], elem, sizeof(elem));
        size_t elen = strlen(elem);
        if (i > 0 && pos + seplen < sizeof(buf) - 1) {
            memcpy(buf + pos, sep, seplen); pos += seplen;
        }
        if (pos + elen < sizeof(buf) - 1) { memcpy(buf + pos, elem, elen); pos += elen; }
    }
    buf[pos] = '\0';
    return val_new_str(buf);
}

/* --- GrayCode (移植元: GrayCodeFuncs.cs) --- */

val_t *bi_toGray(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    uint64_t x = (uint64_t)val_as_long(a[0]);
    return val_new_i64((int64_t)(x ^ (x >> 1)), a[0]->fmt);
}

val_t *bi_fromGray(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    uint64_t x = (uint64_t)val_as_long(a[0]);
    uint64_t mask = x >> 1;
    while (mask) { x ^= mask; mask >>= 1; }
    return val_new_i64((int64_t)x, a[0]->fmt);
}

