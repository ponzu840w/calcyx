/* このファイルは builtin_array.c から分割された。
 * 編集時は builtin_array_internal.h のセクション境界に注意。 */

#include "builtin_array_internal.h"

/* ======================================================
 * 色変換 (移植元: ColorFuncs.cs, ColorSpace.cs)
 * ====================================================== */

static int cs_clamp255(double v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (int)(v + 0.5);
}
static int cs_sat_pack(double r, double g, double b) {
    return (cs_clamp255(r) << 16) | (cs_clamp255(g) << 8) | cs_clamp255(b);
}
static void cs_unpack(int64_t rgb, int *r, int *g, int *b) {
    *r = (int)((rgb >> 16) & 0xff);
    *g = (int)((rgb >> 8)  & 0xff);
    *b = (int)(rgb & 0xff);
}

static double cs_rgb_to_hue(double r, double g, double b, double mn, double mx) {
    if (mn == mx) return 0;
    if (mn == b)  return 60 * (g - r) / (mx - mn) + 60;
    if (mn == r)  return 60 * (b - g) / (mx - mn) + 180;
    return          60 * (r - b) / (mx - mn) + 300;
}

/* HSV <-> RGB */
static int cs_hsv2rgb(double h, double s, double v) {
    h = fmod(h, 360.0);
    s = fmax(0, fmin(100, s)) / 100.0;
    v = fmax(0, fmin(100, v)) / 100.0;
    double f = fmod(h / 60.0, 1.0);
    double x = v * 255, y = v * (1 - s) * 255;
    double z = v * (1 - s * f) * 255, w = v * (1 - s * (1 - f)) * 255;
    double r, g, b;
    if      (s == 0)   { r=x; g=x; b=x; }
    else if (h <  60)  { r=x; g=w; b=y; }
    else if (h < 120)  { r=z; g=x; b=y; }
    else if (h < 180)  { r=y; g=x; b=w; }
    else if (h < 240)  { r=y; g=z; b=x; }
    else if (h < 300)  { r=w; g=y; b=x; }
    else               { r=x; g=y; b=z; }
    return cs_sat_pack(r, g, b);
}
static void cs_rgb2hsv(int ri, int gi, int bi, double *h, double *s, double *v) {
    double r=ri, g=gi, b=bi;
    double mn=fmin(r,fmin(g,b)), mx=fmax(r,fmax(g,b));
    *h = cs_rgb_to_hue(r, g, b, mn, mx);
    *s = (mx == 0) ? 0 : 100*(mx-mn)/mx;
    *v = mx * 100.0 / 255.0;
}

/* HSL <-> RGB */
static int cs_hsl2rgb(double h, double s, double l) {
    h = fmod(h, 360.0);
    s = fmax(0, fmin(100, s)) / 100.0;
    l = fmax(0, fmin(100, l)) / 100.0;
    double f = fmod(h / 60.0, 1.0);
    double chroma = s * (1 - fabs(2*l - 1));
    double mx = 255 * (l + chroma / 2.0);
    double mn = 255 * (l - chroma / 2.0);
    double x = mn + (mx - mn) * f, y = mn + (mx - mn) * (1 - f);
    double r, g, b;
    if      (s == 0)   { r=mx; g=mx; b=mx; }
    else if (h <  60)  { r=mx; g=x;  b=mn; }
    else if (h < 120)  { r=y;  g=mx; b=mn; }
    else if (h < 180)  { r=mn; g=mx; b=x;  }
    else if (h < 240)  { r=mn; g=y;  b=mx; }
    else if (h < 300)  { r=x;  g=mn; b=mx; }
    else               { r=mx; g=mn; b=y;  }
    return cs_sat_pack(r, g, b);
}
static void cs_rgb2hsl(int ri, int gi, int bi, double *h, double *s, double *l) {
    double r=ri, g=gi, b=bi;
    double mn=fmin(r,fmin(g,b)), mx=fmax(r,fmax(g,b));
    *h = cs_rgb_to_hue(r, g, b, mn, mx);
    double p = 255.0 - fabs(mx + mn - 255.0);
    *s = (p == 0) ? 0 : 100*(mx-mn)/p;
    *l = 100*(mx+mn)/(255.0*2.0);
}

/* YUV <-> RGB */
static int cs_rgb2yuv(int ri, int gi, int bi) {
    double r=ri, g=gi, b=bi;
    double y =  0.257*r + 0.504*g + 0.098*b + 16;
    double u = -0.148*r - 0.291*g + 0.439*b + 128;
    double v =  0.439*r - 0.368*g - 0.071*b + 128;
    return cs_sat_pack(y, u, v);
}
static int cs_yuv2rgb(int yi, int ui, int vi) {
    double y=yi-16, u=ui-128, v=vi-128;
    double r = 1.164383*y + 1.596027*v;
    double g = 1.164383*y - 0.391762*u - 0.812968*v;
    double b = 1.164383*y + 2.017232*u;
    return cs_sat_pack(r, g, b);
}

/* RGB565 */
static int cs_rgb888to565(int rgb) {
    int r = (int)fmin(31, (((rgb >> 18) & 0x3f) + 1) >> 1);
    int g = (int)fmin(63, (((rgb >> 9)  & 0x7f) + 1) >> 1);
    int b = (int)fmin(31, (((rgb >> 2)  & 0x3f) + 1) >> 1);
    return ((r & 31) << 11) | ((g & 63) << 5) | (b & 31);
}
static int cs_rgb565to888(int rgb) {
    int r = (rgb >> 11) & 0x1f;
    int g = (rgb >> 5)  & 0x3f;
    int b =  rgb        & 0x1f;
    r = (r << 3) | ((r >> 2) & 7);
    g = (g << 2) | ((g >> 4) & 3);
    b = (b << 3) | ((b >> 2) & 7);
    return cs_sat_pack(r, g, b);
}

static val_t *make_real_arr3(double a, double b, double c) {
    val_t *items[3] = {
        val_new_double(a, FMT_REAL),
        val_new_double(b, FMT_REAL),
        val_new_double(c, FMT_REAL)
    };
    val_t *arr = val_new_array(items, 3, FMT_REAL);
    for (int i = 0; i < 3; i++) val_free(items[i]);
    return arr;
}

val_t *bi_rgb_3(val_t **a, int n, void *ctx) {
    (void)ctx;(void)n;
    int r=cs_clamp255(val_as_double(a[0]));
    int g=cs_clamp255(val_as_double(a[1]));
    int b=cs_clamp255(val_as_double(a[2]));
    return val_new_i64((r<<16)|(g<<8)|b, FMT_WEB_COLOR);
}
val_t *bi_rgb_1(val_t **a, int n, void *ctx) {
    (void)ctx;(void)n;
    return val_reformat(a[0], FMT_WEB_COLOR);
}
val_t *bi_hsv2rgb(val_t **a, int n, void *ctx) {
    (void)ctx;(void)n;
    return val_new_i64(cs_hsv2rgb(val_as_double(a[0]),val_as_double(a[1]),val_as_double(a[2])), FMT_WEB_COLOR);
}
val_t *bi_rgb2hsv(val_t **a, int n, void *ctx) {
    (void)ctx;(void)n;
    int r,g,b; cs_unpack(val_as_long(a[0]),&r,&g,&b);
    double h,s,v; cs_rgb2hsv(r,g,b,&h,&s,&v);
    return make_real_arr3(h,s,v);
}
val_t *bi_hsl2rgb(val_t **a, int n, void *ctx) {
    (void)ctx;(void)n;
    return val_new_i64(cs_hsl2rgb(val_as_double(a[0]),val_as_double(a[1]),val_as_double(a[2])), FMT_WEB_COLOR);
}
val_t *bi_rgb2hsl(val_t **a, int n, void *ctx) {
    (void)ctx;(void)n;
    int r,g,b; cs_unpack(val_as_long(a[0]),&r,&g,&b);
    double h,s,l; cs_rgb2hsl(r,g,b,&h,&s,&l);
    return make_real_arr3(h,s,l);
}
val_t *bi_rgb2yuv_3(val_t **a, int n, void *ctx) {
    (void)ctx;(void)n;
    int r=cs_clamp255(val_as_double(a[0]));
    int g=cs_clamp255(val_as_double(a[1]));
    int b=cs_clamp255(val_as_double(a[2]));
    return val_new_i64(cs_rgb2yuv(r,g,b), FMT_HEX);
}
val_t *bi_rgb2yuv_1(val_t **a, int n, void *ctx) {
    (void)ctx;(void)n;
    int r,g,b; cs_unpack(val_as_long(a[0]),&r,&g,&b);
    return val_new_i64(cs_rgb2yuv(r,g,b), FMT_HEX);
}
val_t *bi_yuv2rgb_3(val_t **a, int n, void *ctx) {
    (void)ctx;(void)n;
    int y=cs_clamp255(val_as_double(a[0]));
    int u=cs_clamp255(val_as_double(a[1]));
    int v=cs_clamp255(val_as_double(a[2]));
    return val_new_i64(cs_yuv2rgb(y,u,v), FMT_WEB_COLOR);
}
val_t *bi_yuv2rgb_1(val_t **a, int n, void *ctx) {
    (void)ctx;(void)n;
    int r,g,b; cs_unpack(val_as_long(a[0]),&r,&g,&b);
    return val_new_i64(cs_yuv2rgb(r,g,b), FMT_WEB_COLOR);
}
val_t *bi_rgbTo565(val_t **a, int n, void *ctx) {
    (void)ctx;(void)n;
    return val_new_i64(cs_rgb888to565((int)val_as_long(a[0])), FMT_HEX);
}
val_t *bi_rgbFrom565(val_t **a, int n, void *ctx) {
    (void)ctx;(void)n;
    return val_new_i64(cs_rgb565to888((int)val_as_long(a[0])), FMT_WEB_COLOR);
}
val_t *bi_pack565(val_t **a, int n, void *ctx) {
    (void)ctx;(void)n;
    int r=(int)val_as_long(a[0]),g=(int)val_as_long(a[1]),b=(int)val_as_long(a[2]);
    r=r<0?0:(r>31?31:r); g=g<0?0:(g>63?63:g); b=b<0?0:(b>31?31:b);
    return val_new_i64((r<<11)|(g<<5)|b, FMT_HEX);
}
val_t *bi_unpack565(val_t **a, int n, void *ctx) {
    (void)ctx;(void)n;
    int rgb=(int)val_as_long(a[0]);
    val_t *items[3] = {
        val_new_i64((rgb>>11)&0x1f, FMT_INT),
        val_new_i64((rgb>>5)&0x3f,  FMT_INT),
        val_new_i64(rgb&0x1f,       FMT_INT)
    };
    val_t *arr = val_new_array(items, 3, FMT_INT);
    for (int i=0;i<3;i++) val_free(items[i]);
    return arr;
}

/* ======================================================
 * パリティ / ECC (移植元: Parity_EccFuncs.cs, LMath.cs)
 * ====================================================== */

static int lm_xor_reduce(int64_t val) {
    val ^= val >> 32; val ^= val >> 16; val ^= val >> 8;
    val ^= val >> 4;  val ^= val >> 2;  val ^= val >> 1;
    return (int)(val & 1);
}
static int lm_odd_parity(int64_t val) { return lm_xor_reduce(val) ^ 1; }

static int lm_ecc_width(int dw) {
    int ew = 0;
    while ((1 << ew) < dw + 1) ew++;
    if (ew + dw >= (1 << ew)) ew++;
    return ew + 1;
}

static const int64_t ECC_XOR_MASK[] = {
    (int64_t)0xab55555556aaad5bLL,
    (int64_t)0xcd9999999b33366dLL,
    (int64_t)0xf1e1e1e1e3c3c78eLL,
    (int64_t)0x01fe01fe03fc07f0LL,
    (int64_t)0x01fffe0003fff800LL,
    (int64_t)0x01fffffffc000000LL,
    (int64_t)0xfe00000000000000LL,
};
static const int ECC_CORR[] = {
     0,-1,-2, 1,-3, 2, 3, 4,-4, 5, 6, 7, 8, 9,10,11,
    -5,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,
    -6,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,
    42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,
    -7,58,59,60,61,62,63,64,
};

static int lm_ecc_encode(int dw, int64_t data) {
    int ew = lm_ecc_width(dw);
    int ecc = 0;
    for (int i = 0; i < ew - 1; i++) {
        ecc |= lm_xor_reduce(data & ECC_XOR_MASK[i]) << i;
    }
    ecc |= (lm_odd_parity(ecc) ^ lm_odd_parity(data)) << (ew - 1);
    return ecc;
}

static int lm_ecc_decode(int dw, int ecc, int64_t data) {
    int parity = lm_odd_parity(ecc) ^ lm_odd_parity(data);
    int ew = lm_ecc_width(dw);
    int syndrome = ecc ^ lm_ecc_encode(dw, data);
    syndrome &= (1 << (ew - 1)) - 1;
    int err_pos = ECC_CORR[syndrome];
    if (parity == 0) {
        return (err_pos == 0) ? 0 : -1;
    } else {
        if (err_pos == 0) return dw + ew;
        if (err_pos < 0)  return dw - err_pos;
        return err_pos;
    }
}

val_t *bi_xorReduce(val_t **a, int n, void *ctx) {
    (void)ctx;(void)n;
    return val_new_i64(lm_xor_reduce(val_as_long(a[0])), FMT_INT);
}
val_t *bi_oddParity(val_t **a, int n, void *ctx) {
    (void)ctx;(void)n;
    return val_new_i64(lm_odd_parity(val_as_long(a[0])), FMT_INT);
}
val_t *bi_eccWidth(val_t **a, int n, void *ctx) {
    (void)ctx;(void)n;
    return val_new_i64(lm_ecc_width((int)val_as_long(a[0])), FMT_INT);
}
val_t *bi_eccEnc(val_t **a, int n, void *ctx) {
    (void)ctx;(void)n;
    return val_new_i64(lm_ecc_encode((int)val_as_long(a[0]), val_as_long(a[1])), FMT_HEX);
}
val_t *bi_eccDec(val_t **a, int n, void *ctx) {
    (void)ctx;(void)n;
    return val_new_i64(lm_ecc_decode((int)val_as_long(a[0]), (int)val_as_long(a[1]), val_as_long(a[2])), FMT_INT);
}

