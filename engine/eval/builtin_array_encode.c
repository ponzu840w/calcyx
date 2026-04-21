/* このファイルは builtin_array.c から分割された。
 * 編集時は builtin_array_internal.h のセクション境界に注意。 */

#include "builtin_array_internal.h"

/* ======================================================
 * エンコーディング (移植元: EncodingFuncs.cs)
 * ====================================================== */

/* utf8Enc(str) → byte array */
val_t *bi_utf8Enc(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    const char *s = (a[0]->type == VAL_STR) ? a[0]->str_v : "";
    int len = (int)strlen(s);
    val_t **items = (val_t **)malloc((size_t)(len + 1) * sizeof(val_t *));
    int cnt = 0;
    for (int i = 0; i < len; i++) {
        items[cnt++] = val_new_i64((uint8_t)s[i], FMT_HEX);
    }
    val_t *arr = val_new_array(items, cnt, FMT_HEX);
    for (int i = 0; i < cnt; i++) val_free(items[i]);
    free(items);
    return arr;
}

/* utf8Dec(bytes[]) → string */
val_t *bi_utf8Dec(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    if (a[0]->type != VAL_ARRAY) return val_new_str("");
    int len = a[0]->arr_len;
    char *buf = (char *)malloc((size_t)(len + 1));
    for (int i = 0; i < len; i++) {
        buf[i] = (char)(uint8_t)(val_as_long(a[0]->arr_items[i]) & 0xFF);
    }
    buf[len] = '\0';
    val_t *v = val_new_str(buf);
    free(buf);
    return v;
}

/* urlEnc(str) → percent-encoded string */
val_t *bi_urlEnc(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    const char *s = (a[0]->type == VAL_STR) ? a[0]->str_v : "";
    int slen = (int)strlen(s);
    char *buf = (char *)malloc((size_t)(slen * 3 + 1));
    int pos = 0;
    for (int i = 0; i < slen; i++) {
        unsigned char c = (unsigned char)s[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c=='-' || c=='_' || c=='.' || c=='~') {
            buf[pos++] = (char)c;
        } else {
            pos += snprintf(buf + pos, 4, "%%%02X", c);
        }
    }
    buf[pos] = '\0';
    val_t *v = val_new_str(buf);
    free(buf);
    return v;
}

/* urlDec(str) → decoded string */
val_t *bi_urlDec(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    const char *s = (a[0]->type == VAL_STR) ? a[0]->str_v : "";
    int slen = (int)strlen(s);
    char *buf = (char *)malloc((size_t)(slen + 1));
    int pos = 0;
    for (int i = 0; i < slen; ) {
        if (s[i] == '%' && i + 2 < slen && isxdigit((unsigned char)s[i+1]) && isxdigit((unsigned char)s[i+2])) {
            char hex[3] = {s[i+1], s[i+2], '\0'};
            buf[pos++] = (char)(int)strtol(hex, NULL, 16);
            i += 3;
        } else if (s[i] == '+') {
            buf[pos++] = ' ';
            i++;
        } else {
            buf[pos++] = s[i++];
        }
    }
    buf[pos] = '\0';
    val_t *v = val_new_str(buf);
    free(buf);
    return v;
}

/* Base64 テーブル */
static const char B64_ENC[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static val_t *base64_enc_bytes_impl(const uint8_t *data, int dlen) {
    int outlen = ((dlen + 2) / 3) * 4;
    char *buf = (char *)malloc((size_t)(outlen + 1));
    int pos = 0;
    for (int i = 0; i < dlen; i += 3) {
        int b0 = data[i];
        int b1 = (i+1 < dlen) ? data[i+1] : 0;
        int b2 = (i+2 < dlen) ? data[i+2] : 0;
        buf[pos++] = B64_ENC[(b0 >> 2) & 0x3F];
        buf[pos++] = B64_ENC[((b0 & 3) << 4) | ((b1 >> 4) & 0xF)];
        buf[pos++] = (i+1 < dlen) ? B64_ENC[((b1 & 0xF) << 2) | ((b2 >> 6) & 3)] : '=';
        buf[pos++] = (i+2 < dlen) ? B64_ENC[b2 & 0x3F] : '=';
    }
    buf[pos] = '\0';
    val_t *v = val_new_str(buf);
    free(buf);
    return v;
}

static int b64_decode_char(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

/* base64Enc(str) → string */
val_t *bi_base64Enc(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    const char *s = (a[0]->type == VAL_STR) ? a[0]->str_v : "";
    return base64_enc_bytes_impl((const uint8_t *)s, (int)strlen(s));
}

/* base64EncBytes(bytes[]) → string */
val_t *bi_base64EncBytes(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    if (a[0]->type != VAL_ARRAY) return val_new_str("");
    int len = a[0]->arr_len;
    uint8_t *data = (uint8_t *)malloc((size_t)(len + 1));
    for (int i = 0; i < len; i++) data[i] = (uint8_t)(val_as_long(a[0]->arr_items[i]) & 0xFF);
    val_t *v = base64_enc_bytes_impl(data, len);
    free(data);
    return v;
}

/* base64Dec(str) → string */
val_t *bi_base64Dec(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    const char *s = (a[0]->type == VAL_STR) ? a[0]->str_v : "";
    int slen = (int)strlen(s);
    int outlen = (slen / 4) * 3 + 4;
    uint8_t *buf = (uint8_t *)malloc((size_t)outlen);
    int pos = 0;
    for (int i = 0; i + 3 < slen; i += 4) {
        int c0 = b64_decode_char(s[i]);
        int c1 = b64_decode_char(s[i+1]);
        int c2 = (s[i+2] != '=') ? b64_decode_char(s[i+2]) : 0;
        int c3 = (s[i+3] != '=') ? b64_decode_char(s[i+3]) : 0;
        if (c0 < 0 || c1 < 0) break;
        buf[pos++] = (uint8_t)((c0 << 2) | (c1 >> 4));
        if (s[i+2] != '=') buf[pos++] = (uint8_t)(((c1 & 0xF) << 4) | (c2 >> 2));
        if (s[i+3] != '=') buf[pos++] = (uint8_t)(((c2 & 3) << 6) | c3);
    }
    buf[pos] = '\0';
    val_t *v = val_new_str((char *)buf);
    free(buf);
    return v;
}

/* base64DecBytes(str) → bytes[] */
val_t *bi_base64DecBytes(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    const char *s = (a[0]->type == VAL_STR) ? a[0]->str_v : "";
    int slen = (int)strlen(s);
    int outlen = (slen / 4) * 3 + 4;
    uint8_t *buf = (uint8_t *)malloc((size_t)outlen);
    int pos = 0;
    for (int i = 0; i + 3 < slen; i += 4) {
        int c0 = b64_decode_char(s[i]);
        int c1 = b64_decode_char(s[i+1]);
        int c2 = (s[i+2] != '=') ? b64_decode_char(s[i+2]) : 0;
        int c3 = (s[i+3] != '=') ? b64_decode_char(s[i+3]) : 0;
        if (c0 < 0 || c1 < 0) break;
        buf[pos++] = (uint8_t)((c0 << 2) | (c1 >> 4));
        if (s[i+2] != '=') buf[pos++] = (uint8_t)(((c1 & 0xF) << 4) | (c2 >> 2));
        if (s[i+3] != '=') buf[pos++] = (uint8_t)(((c2 & 3) << 6) | c3);
    }
    val_t **items = (val_t **)malloc((size_t)pos * sizeof(val_t *));
    for (int i = 0; i < pos; i++) items[i] = val_new_i64(buf[i], FMT_HEX);
    val_t *arr = val_new_array(items, pos, FMT_HEX);
    for (int i = 0; i < pos; i++) val_free(items[i]);
    free(items); free(buf);
    return arr;
}

/* ======================================================
 * E系列 (移植元: ESeriesFuncs.cs, ESeries.cs, PreferredNumbers.cs)
 * ====================================================== */

/* E系列の double 値を mpdecimal に変換する (%.3g で丸めて精度誤差を除去) */
static val_t *val_from_es(double d, val_fmt_t fmt) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.3g", d);
    real_t r; real_from_str(&r, buf);
    return val_new_real(&r, fmt);
}

static const double E3[]  = {1.0, 2.2, 4.7};
static const double E6[]  = {1.0, 1.5, 2.2, 3.3, 4.7, 6.8};
static const double E12[] = {1.0, 1.2, 1.5, 1.8, 2.2, 2.7, 3.3, 3.9, 4.7, 5.6, 6.8, 8.2};
static const double E24[] = {1.0, 1.1, 1.2, 1.3, 1.5, 1.6, 1.8, 2.0, 2.2, 2.4, 2.7, 3.0,
                              3.3, 3.6, 3.9, 4.3, 4.7, 5.1, 5.6, 6.2, 6.8, 7.5, 8.2, 9.1};
/* 移植元: Calctus/Model/Standards/Eseries.cs - ESeries.E48/E96/E192 */
static const double E48[] = {
    1.00, 1.05, 1.10, 1.15, 1.21, 1.27, 1.33, 1.40, 1.47, 1.54, 1.62, 1.69,
    1.78, 1.87, 1.96, 2.05, 2.15, 2.26, 2.37, 2.49, 2.61, 2.74, 2.87, 3.01,
    3.16, 3.32, 3.48, 3.65, 3.83, 4.02, 4.22, 4.42, 4.64, 4.87, 5.11, 5.36,
    5.62, 5.90, 6.19, 6.49, 6.81, 7.15, 7.50, 7.87, 8.25, 8.66, 9.09, 9.53};
static const double E96[] = {
    1.00, 1.02, 1.05, 1.07, 1.10, 1.13, 1.15, 1.18, 1.21, 1.24, 1.27, 1.30,
    1.33, 1.37, 1.40, 1.43, 1.47, 1.50, 1.54, 1.58, 1.62, 1.65, 1.69, 1.74,
    1.78, 1.82, 1.87, 1.91, 1.96, 2.00, 2.05, 2.10, 2.15, 2.21, 2.26, 2.32,
    2.37, 2.43, 2.49, 2.55, 2.61, 2.67, 2.74, 2.80, 2.87, 2.94, 3.01, 3.09,
    3.16, 3.24, 3.32, 3.40, 3.48, 3.57, 3.65, 3.74, 3.83, 3.92, 4.02, 4.12,
    4.22, 4.32, 4.42, 4.53, 4.64, 4.75, 4.87, 4.99, 5.11, 5.23, 5.36, 5.49,
    5.62, 5.76, 5.90, 6.04, 6.19, 6.34, 6.49, 6.65, 6.81, 6.98, 7.15, 7.32,
    7.50, 7.68, 7.87, 8.06, 8.25, 8.45, 8.66, 8.87, 9.09, 9.31, 9.53, 9.76};
static const double E192[] = {
    1.00, 1.01, 1.02, 1.04, 1.05, 1.06, 1.07, 1.09, 1.10, 1.11, 1.13, 1.14,
    1.15, 1.17, 1.18, 1.20, 1.21, 1.23, 1.24, 1.26, 1.27, 1.29, 1.30, 1.32,
    1.33, 1.35, 1.37, 1.38, 1.40, 1.42, 1.43, 1.45, 1.47, 1.49, 1.50, 1.52,
    1.54, 1.56, 1.58, 1.60, 1.62, 1.64, 1.65, 1.67, 1.69, 1.72, 1.74, 1.76,
    1.78, 1.80, 1.82, 1.84, 1.87, 1.89, 1.91, 1.93, 1.96, 1.98, 2.00, 2.03,
    2.05, 2.08, 2.10, 2.13, 2.15, 2.18, 2.21, 2.23, 2.26, 2.29, 2.32, 2.34,
    2.37, 2.40, 2.43, 2.46, 2.49, 2.52, 2.55, 2.58, 2.61, 2.64, 2.67, 2.71,
    2.74, 2.77, 2.80, 2.84, 2.87, 2.91, 2.94, 2.98, 3.01, 3.05, 3.09, 3.12,
    3.16, 3.20, 3.24, 3.28, 3.32, 3.36, 3.40, 3.44, 3.48, 3.52, 3.57, 3.61,
    3.65, 3.70, 3.74, 3.79, 3.83, 3.88, 3.92, 3.97, 4.02, 4.07, 4.12, 4.17,
    4.22, 4.27, 4.32, 4.37, 4.42, 4.48, 4.53, 4.59, 4.64, 4.70, 4.75, 4.81,
    4.87, 4.93, 4.99, 5.05, 5.11, 5.17, 5.23, 5.30, 5.36, 5.42, 5.49, 5.56,
    5.62, 5.69, 5.76, 5.83, 5.90, 5.97, 6.04, 6.12, 6.19, 6.26, 6.34, 6.42,
    6.49, 6.57, 6.65, 6.73, 6.81, 6.90, 6.98, 7.06, 7.15, 7.23, 7.32, 7.41,
    7.50, 7.59, 7.68, 7.77, 7.87, 7.96, 8.06, 8.16, 8.25, 8.35, 8.45, 8.56,
    8.66, 8.76, 8.87, 8.98, 9.09, 9.20, 9.31, 9.42, 9.53, 9.65, 9.76, 9.88};

static int es_get_series(int n, const double **out) {
    switch (n) {
        case   3: *out = E3;   return   3;
        case   6: *out = E6;   return   6;
        case  12: *out = E12;  return  12;
        case  24: *out = E24;  return  24;
        case  48: *out = E48;  return  48;
        case  96: *out = E96;  return  96;
        case 192: *out = E192; return 192;
        default:  *out = E24;  return  24;
    }
}

/* series[0..len-1] から key 以下の最大要素のインデックスを返す */
static int es_bsearch(const double *series, int len, double key) {
    int i0 = 0, i1 = len - 1;
    while (i0 < i1) {
        int im = (i0 + i1) / 2 + 1;
        if (key < series[im]) i1 = im - 1;
        else                  i0 = im;
    }
    return i0;
}

/* Shift10: value * 10^exp */
static double es_shift10(double value, int exp) {
    double p = 1.0;
    if (exp >= 0) { for (int i=0; i<exp; i++) p *= 10.0; return value * p; }
    else          { for (int i=0; i<-exp; i++) p *= 10.0; return value / p; }
}

/* FindNearests: series から value に最も近い(floor, ceil)を返す */
static void es_find_nearests(const double *series, int len,
                              double value, double *fl, double *ce) {
    double pexp = floor(log10(value));
    int exp = (int)pexp;
    double key = es_shift10(value, -exp);
    int i = es_bsearch(series, len, key);
    *fl = es_shift10(series[i], exp);
    if (fabs(*fl - value) < 1e-10 * fabs(value)) {
        *ce = *fl;
    } else if (i < len - 1) {
        *ce = es_shift10(series[i + 1], exp);
    } else {
        *ce = es_shift10(series[0], exp + 1);
    }
}

/* FindSplitPair: 分圧比 value に最も近いペアを返す */
static void es_find_split_pair(const double *series, int len,
                                double value, double *out_lo, double *out_hi) {
    if (value >= 1.0) { *out_lo = 1.0; *out_hi = 0.0; return; }
    if (value <= 0.0) { *out_lo = 0.0; *out_hi = 1.0; return; }
    double min_diff = 1e300;
    *out_lo = 0.0; *out_hi = 1.0;
    for (int i = 0; i < len; i++) {
        double lo = series[i];
        double hi_target = lo / value - lo;
        double hi_fl, hi_ce;
        es_find_nearests(series, len, hi_target, &hi_fl, &hi_ce);
        double d_fl = fabs(value - lo / (hi_fl + lo));
        double d_ce = fabs(value - lo / (hi_ce + lo));
        if (d_fl < min_diff) { min_diff = d_fl; *out_lo = lo; *out_hi = hi_fl; }
        if (d_ce < min_diff) { min_diff = d_ce; *out_lo = lo; *out_hi = hi_ce; }
    }
    /* 両方が 1 以上になるように桁合わせ */
    double mn = (*out_lo < *out_hi) ? *out_lo : *out_hi;
    if (mn > 0.0) {
        int exp = (int)floor(log10(mn));
        if (exp < 0) {
            *out_lo = es_shift10(*out_lo, -exp);
            *out_hi = es_shift10(*out_hi, -exp);
        }
    }
}

val_t *bi_esFloor(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    const double *series; int len = es_get_series((int)val_as_long(a[0]), &series);
    double v = val_as_double(a[1]);
    double fl, ce; es_find_nearests(series, len, v, &fl, &ce);
    return val_from_es(fl, a[1]->fmt);
}
val_t *bi_esCeil(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    const double *series; int len = es_get_series((int)val_as_long(a[0]), &series);
    double v = val_as_double(a[1]);
    double fl, ce; es_find_nearests(series, len, v, &fl, &ce);
    return val_from_es(ce, a[1]->fmt);
}
val_t *bi_esRound(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    const double *series; int len = es_get_series((int)val_as_long(a[0]), &series);
    double v = val_as_double(a[1]);
    double fl, ce; es_find_nearests(series, len, v, &fl, &ce);
    double result = (fabs(v - fl) <= fabs(ce - v)) ? fl : ce;
    return val_from_es(result, a[1]->fmt);
}
val_t *bi_esRatio(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    const double *series; int len = es_get_series((int)val_as_long(a[0]), &series);
    double v = val_as_double(a[1]);
    double lo, hi; es_find_split_pair(series, len, v, &lo, &hi);
    val_t *items[2] = {
        val_from_es(lo, a[1]->fmt),
        val_from_es(hi, a[1]->fmt)
    };
    val_t *arr = val_new_array(items, 2, a[1]->fmt);
    val_free(items[0]); val_free(items[1]);
    return arr;
}

/* ======================================================
 * Cast functions (移植元: CastFuncs.cs)
 * ====================================================== */

/* rat(x) / rat(x, max_deno): 実数 → 分数 */
val_t *bi_rat1(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    if (a[0]->type == VAL_FRAC) return val_dup(a[0]);
    frac_t f;
    val_as_frac(&f, a[0]);
    return val_new_frac(&f);
}

val_t *bi_rat2(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    int64_t max_deno = val_as_long(a[1]);
    real_t r;
    val_as_real(&r, a[0]);
    /* 連分数で近似 (移植元: RMath.FindFrac) */
    double x = real_to_double(&r);
    int sign = (x < 0) ? -1 : 1;
    x = fabs(x);
    /* xis: 連分数係数リスト */
    double xis[256]; int xis_len = 0;
    int64_t p1 = 1, q1 = 1; /* best so far */
    while (xis_len < 200) {
        double xi = floor(x);
        if (xis_len < 256) xis[xis_len++] = xi;
        /* 後ろから計算して収束分数を求める */
        double n = xi, d = 1.0;
        for (int i = xis_len - 2; i >= 0; i--) {
            double tmp = n;
            n = n * xis[i] + d;
            d = tmp;
            /* 約分 */
            double g = (double)((int64_t)fabs(n) | (int64_t)fabs(d));
            /* シンプルな整数 GCD */
            int64_t a2 = (int64_t)fabs(n), b2 = (int64_t)fabs(d);
            while (b2) { int64_t t = b2; b2 = a2 % b2; a2 = t; } (void)g;
            if (a2 > 1) { n /= a2; d /= a2; }
        }
        if ((int64_t)n > max_deno || (int64_t)d > max_deno) break;
        p1 = (int64_t)n; q1 = (int64_t)d;
        double rem = x - xi;
        if (fabs(rem) < 1e-20) break;
        x = 1.0 / rem;
    }
    p1 *= sign;
    frac_t f;
    real_t rp, rq;
    real_from_i64(&rp, p1);
    real_from_i64(&rq, q1 ? q1 : 1);
    frac_from_n_d(&f, &rp, &rq);
    return val_new_frac(&f);
}

/* real(x): 分数 → 実数 */
val_t *bi_real_fn(val_t **a, int n, void *ctx) {
    (void)ctx; (void)n;
    if (a[0]->type == VAL_REAL) return val_dup(a[0]);
    real_t r;
    val_as_real(&r, a[0]);
    return val_new_real(&r, FMT_REAL);
}

