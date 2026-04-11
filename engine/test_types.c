/* 元実装 (calctus-linux) の埋め込みテストを C に移植したもの
 *
 * ufixed113: Calctus/Model/Types/ufixed113.cs - ufixed113.Test()
 * quad:      Calctus/Model/Types/quad.cs      - quad.Test()
 * real:      元実装にテストなし
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types/real.h"
#include "types/ufixed113.h"
#include "types/quad.h"
#include "types/val.h"

static int g_failures = 0;

static void assert_ufixed113_eq(const char *label,
                                 ufixed113_t got, ufixed113_t expected) {
    if (!ufixed113_eq(got, expected)) {
        fprintf(stderr, "FAIL: %s\n", label);
        g_failures++;
    }
}

static void assert_u32_eq(const char *label, uint32_t got, uint32_t expected) {
    if (got != expected) {
        fprintf(stderr, "FAIL: %s  got=%u expected=%u\n", label, got, expected);
        g_failures++;
    }
}

/* ufixed113.cs: AssertAdd(double a, double b, double q, uint carry) */
static void ufixed113_assert_add(double a, double b, double q, uint32_t expected_carry) {
    char label[128];
    ufixed113_t va = ufixed113_from_double(a);
    ufixed113_t vb = ufixed113_from_double(b);
    ufixed113_t vq = ufixed113_from_double(q);
    uint32_t carry;
    ufixed113_t result = ufixed113_add(va, vb, &carry);

    snprintf(label, sizeof(label), "[ufixed113] %g+%g", a, b);
    assert_ufixed113_eq(label, result, vq);

    snprintf(label, sizeof(label), "[ufixed113] carry(%g+%g)", a, b);
    assert_u32_eq(label, carry, expected_carry);
}

/* ufixed113.cs: AssertSub(double a, double b, double q, uint carry) */
static void ufixed113_assert_sub(double a, double b, double q, uint32_t expected_borrow) {
    char label[128];
    ufixed113_t va = ufixed113_from_double(a);
    ufixed113_t vb = ufixed113_from_double(b);
    ufixed113_t vq = ufixed113_from_double(q);
    uint32_t borrow;
    ufixed113_t result = ufixed113_sub(va, vb, &borrow);

    snprintf(label, sizeof(label), "[ufixed113] %g-%g", a, b);
    assert_ufixed113_eq(label, result, vq);

    snprintf(label, sizeof(label), "[ufixed113] carry(%g-%g)", a, b);
    assert_u32_eq(label, borrow, expected_borrow);
}

/* ufixed113.cs: ufixed113.Test() をそのまま移植 */
static void test_ufixed113(void) {
    ufixed113_assert_add(0.5, 0.5, 1,  0u);
    ufixed113_assert_add(1,   1,   0,  1u);
    ufixed113_assert_sub(1,   1,   0,  0u);
    printf("[ufixed113] done\n");
}

/* --- quad テスト ---
 * 移植元: Calctus/Model/Types/quad.cs - quad.Test() */

static void assert_quad_eq(const char *label, cx_quad_t got, cx_quad_t expected) {
    if (!quad_eq(got, expected)) {
        real_t rg, re;
        quad_to_real(&rg, got);
        quad_to_real(&re, expected);
        char bg[64], be[64];
        real_to_str(&rg, bg, sizeof(bg));
        real_to_str(&re, be, sizeof(be));
        fprintf(stderr, "FAIL: %s  got=%s expected=%s\n", label, bg, be);
        g_failures++;
    }
}

/* quad.cs: AssertAdd(decimal a, decimal b, decimal q) */
static void quad_assert_add(const char *as, const char *bs, const char *qs) {
    char label[128];
    real_t ra, rb, rq;
    real_from_str(&ra, as); real_from_str(&rb, bs); real_from_str(&rq, qs);
    cx_quad_t qa = quad_from_real(&ra);
    cx_quad_t qb = quad_from_real(&rb);
    cx_quad_t qq = quad_from_real(&rq);
    snprintf(label, sizeof(label), "[quad] %s+%s", as, bs);
    assert_quad_eq(label, quad_add(qa, qb), qq);
}

/* quad.cs: AssertSub(decimal a, decimal b, decimal q) */
static void quad_assert_sub(const char *as, const char *bs, const char *qs) {
    char label[128];
    real_t ra, rb, rq;
    real_from_str(&ra, as); real_from_str(&rb, bs); real_from_str(&rq, qs);
    cx_quad_t qa = quad_from_real(&ra);
    cx_quad_t qb = quad_from_real(&rb);
    cx_quad_t qq = quad_from_real(&rq);
    snprintf(label, sizeof(label), "[quad] %s-%s", as, bs);
    assert_quad_eq(label, quad_sub(qa, qb), qq);
}

/* quad.cs: AssertMul(decimal a, decimal b, decimal q) */
static void quad_assert_mul(const char *as, const char *bs, const char *qs) {
    char label[128];
    real_t ra, rb, rq;
    real_from_str(&ra, as); real_from_str(&rb, bs); real_from_str(&rq, qs);
    cx_quad_t qa = quad_from_real(&ra);
    cx_quad_t qb = quad_from_real(&rb);
    cx_quad_t qq = quad_from_real(&rq);
    snprintf(label, sizeof(label), "[quad] %s*%s", as, bs);
    assert_quad_eq(label, quad_mul(qa, qb), qq);
}

/* quad.cs: AssertTrunc(decimal a, decimal q) */
static void quad_assert_trunc(const char *as, const char *qs) {
    char label[128];
    real_t ra, rq;
    real_from_str(&ra, as); real_from_str(&rq, qs);
    cx_quad_t qa = quad_from_real(&ra);
    cx_quad_t qq = quad_from_real(&rq);
    snprintf(label, sizeof(label), "[quad] Truncate(%s)", as);
    assert_quad_eq(label, quad_truncate(qa), qq);
}

/* quad.cs: Assert.Equal("[quad] ", (decimal)(quad)X, X) */
static void quad_assert_roundtrip(const char *as) {
    char label[128];
    real_t ra, back;
    real_from_str(&ra, as);
    cx_quad_t qa = quad_from_real(&ra);
    quad_to_real(&back, qa);
    char buf_got[64], buf_exp[64];
    real_to_str(&back, buf_got, sizeof(buf_got));
    real_to_str(&ra,   buf_exp, sizeof(buf_exp));
    snprintf(label, sizeof(label), "[quad] roundtrip(%s)", as);
    if (!real_eq(&back, &ra)) {
        fprintf(stderr, "FAIL: %s  got=%s expected=%s\n", label, buf_got, buf_exp);
        g_failures++;
    }
}

/* quad.cs: quad.Test() をそのまま移植
 * 0x10000000 = 268435456 */
static void test_quad(void) {
    quad_assert_add("5",   "3",   "8");
    quad_assert_add("3",   "5",   "8");
    quad_assert_add("3",   "-5",  "-2");
    quad_assert_add("-3",  "5",   "2");
    quad_assert_add("0.25","0.75","1");
    quad_assert_add("4294967295", "1",          "4294967296");
    quad_assert_add("4294967296", "1",          "4294967297");
    quad_assert_add("4294967295", "4294967295", "8589934590");
    /* 1 + 1/0x10000000 = 1 + 1/268435456 */
    quad_assert_add("1", "0.0000000037252902984619140625", "1.0000000037252902984619140625");

    quad_assert_sub("5",   "3",    "2");
    quad_assert_sub("3",   "5",    "-2");
    quad_assert_sub("3",   "-5",   "8");
    quad_assert_sub("-3",  "5",    "-8");
    quad_assert_sub("0.25","0.75", "-0.5");
    quad_assert_sub("4294967295", "1",          "4294967294");
    quad_assert_sub("4294967296", "1",          "4294967295");
    quad_assert_sub("4294967295", "4294967295", "0");
    quad_assert_sub("1", "0.0000000037252902984619140625", "0.9999999962747097015380859375");

    quad_assert_mul("5",    "3",    "15");
    quad_assert_mul("3",    "5",    "15");
    quad_assert_mul("3",    "-5",   "-15");
    quad_assert_mul("-3",   "5",    "-15");
    quad_assert_mul("0.25", "0.75", "0.1875");
    /* 0x10000000 * (1/0x10000000) = 1 */
    quad_assert_mul("268435456", "0.0000000037252902984619140625", "1");
    quad_assert_mul("1.75", "1.75", "3.0625");

    quad_assert_trunc("0",     "0");
    quad_assert_trunc("0.5",   "0");
    /* 1 - 1/0x10000000 */
    quad_assert_trunc("0.9999999962747097015380859375", "0");
    quad_assert_trunc("1",     "1");
    quad_assert_trunc("1.5",   "1");
    /* 2 - 1/0x10000000 */
    quad_assert_trunc("1.9999999962747097015380859375", "1");
    quad_assert_trunc("2",     "2");
    quad_assert_trunc("-0.5",  "0");
    /* -1 + 1/0x10000000 */
    quad_assert_trunc("-0.9999999962747097015380859375", "0");
    quad_assert_trunc("-1",    "-1");
    quad_assert_trunc("-1.5",  "-1");
    /* -2 + 1/0x10000000 */
    quad_assert_trunc("-1.9999999962747097015380859375", "-1");
    quad_assert_trunc("-2",    "-2");

    quad_assert_roundtrip("12345");
    quad_assert_roundtrip("0.03125");
    quad_assert_roundtrip("12345.03125");
    /* 注: 0.1 は 2進数で非終端なので roundtrip テスト対象外（元実装でもコメントアウト） */

    printf("[quad] done\n");
}

/* --- QMath テスト ---
 * 移植元: Calctus/Model/Mathematics/QMath.cs - QMath.Test() */

static void test_qmath_log2(void) {
    real_t r;
    cx_quad_t got, expected;

#define ASSERT_LOG2(input_str, expected_str) \
    do { \
        real_from_str(&r, input_str); \
        got = quad_log2(quad_from_real(&r)); \
        real_from_str(&r, expected_str); \
        expected = quad_from_real(&r); \
        assert_quad_eq("[QMath.Log2] " input_str, got, expected); \
    } while (0)

    ASSERT_LOG2("1",     "0");
    ASSERT_LOG2("2",     "1");
    ASSERT_LOG2("4",     "2");
    ASSERT_LOG2("8",     "3");
    ASSERT_LOG2("0.5",   "-1");
    ASSERT_LOG2("0.25",  "-2");
    ASSERT_LOG2("0.125", "-3");

#undef ASSERT_LOG2

    printf("[QMath.Log2] done\n");
}

/* --- NumberFormatter テスト ---
 * 移植元: Calctus/Model/Formats/NumberFormatter.cs - NumberFormatter.Test() */

static void nf_assert(const fmt_settings_t *fs, const char *input, const char *expected) {
    real_t r;
    real_from_str(&r, input);
    char buf[256];
    real_to_str_with_settings(&r, fs, buf, sizeof(buf));
    if (strcmp(buf, expected) != 0) {
        fprintf(stderr, "FAIL: [NumberFormatter] input=%s expected=\"%s\" got=\"%s\"\n",
                input, expected, buf);
        g_failures++;
    }
}

static void test_number_formatter(void) {
    /* Group 1: ENotationEnabled=false */
    {
        fmt_settings_t fs = { 28, false, 4, -3, false };
        nf_assert(&fs, "0",    "0");
        nf_assert(&fs, "1",    "1");
        nf_assert(&fs, "12345",              "12345");
        nf_assert(&fs, "1234500000000000",   "1234500000000000");
        nf_assert(&fs, "-1",   "-1");
        nf_assert(&fs, "-10",  "-10");
        nf_assert(&fs, "-12345",             "-12345");
        nf_assert(&fs, "-1234500000000000",  "-1234500000000000");
        nf_assert(&fs, "0.1",  "0.1");
        nf_assert(&fs, "0.01", "0.01");
        nf_assert(&fs, "0.001","0.001");
        nf_assert(&fs, "0.0000000000000012345", "0.0000000000000012345");
        nf_assert(&fs, "-0.1",  "-0.1");
        nf_assert(&fs, "-0.01", "-0.01");
        nf_assert(&fs, "-0.001","-0.001");
        nf_assert(&fs, "-0.0000000000000012345", "-0.0000000000000012345");
    }
    /* Group 2: ENotationEnabled=true, ENotationAlignment=false */
    {
        fmt_settings_t fs = { 28, true, 4, -3, false };
        nf_assert(&fs, "0",    "0");
        nf_assert(&fs, "1",    "1");
        nf_assert(&fs, "1000", "1000");
        nf_assert(&fs, "9999", "9999");
        nf_assert(&fs, "10000",            "1e4");
        nf_assert(&fs, "1234500000000000", "1.2345e15");
        nf_assert(&fs, "-1",   "-1");
        nf_assert(&fs, "-10",  "-10");
        nf_assert(&fs, "-1000","-1000");
        nf_assert(&fs, "-9999","-9999");
        nf_assert(&fs, "-10000",           "-1e4");
        nf_assert(&fs, "-1234500000000000","-1.2345e15");
        nf_assert(&fs, "0.1",  "0.1");
        nf_assert(&fs, "0.01", "0.01");
        nf_assert(&fs, "0.00999",  "9.99e-3");
        nf_assert(&fs, "0.001",    "1e-3");
        nf_assert(&fs, "0.0000000000000012345", "1.2345e-15");
        nf_assert(&fs, "-0.1",  "-0.1");
        nf_assert(&fs, "-0.01", "-0.01");
        nf_assert(&fs, "-0.00999", "-9.99e-3");
        nf_assert(&fs, "-0.001",   "-1e-3");
        nf_assert(&fs, "-0.0000000000000012345", "-1.2345e-15");
    }
    /* Group 3: ENotationEnabled=true, ENotationAlignment=true */
    {
        fmt_settings_t fs = { 28, true, 4, -3, true };
        nf_assert(&fs, "0",    "0");
        nf_assert(&fs, "1",    "1");
        nf_assert(&fs, "1000", "1000");
        nf_assert(&fs, "9999", "9999");
        nf_assert(&fs, "10000",   "10e3");
        nf_assert(&fs, "12345",   "12.345e3");
        nf_assert(&fs, "123456",  "123.456e3");
        nf_assert(&fs, "1234567", "1.234567e6");
        nf_assert(&fs, "1234500000000000", "1.2345e15");
        nf_assert(&fs, "-1",    "-1");
        nf_assert(&fs, "-1000", "-1000");
        nf_assert(&fs, "-9999", "-9999");
        nf_assert(&fs, "-10000",   "-10e3");
        nf_assert(&fs, "-12345",   "-12.345e3");
        nf_assert(&fs, "-123456",  "-123.456e3");
        nf_assert(&fs, "-1234567", "-1.234567e6");
        nf_assert(&fs, "-1234500000000000", "-1.2345e15");
        nf_assert(&fs, "0.1",   "0.1");
        nf_assert(&fs, "0.01",  "0.01");
        nf_assert(&fs, "0.00999",    "9.99e-3");
        nf_assert(&fs, "0.001",      "1e-3");
        nf_assert(&fs, "0.0012345",  "1.2345e-3");
        nf_assert(&fs, "0.00012345", "123.45e-6");
        nf_assert(&fs, "0.000012345","12.345e-6");
        nf_assert(&fs, "0.0000000000000012345", "1.2345e-15");
        nf_assert(&fs, "-0.1",   "-0.1");
        nf_assert(&fs, "-0.01",  "-0.01");
        nf_assert(&fs, "-0.00999",    "-9.99e-3");
        nf_assert(&fs, "-0.001",      "-1e-3");
        nf_assert(&fs, "-0.0012345",  "-1.2345e-3");
        nf_assert(&fs, "-0.00012345", "-123.45e-6");
        nf_assert(&fs, "-0.000012345","-12.345e-6");
        nf_assert(&fs, "-0.0000000000000012345", "-1.2345e-15");
    }
    /* Group 4: DecimalLengthToDisplay=5, ENotationEnabled=false (丸め確認) */
    {
        fmt_settings_t fs = { 5, false, 4, -3, false };
        nf_assert(&fs, "10000",   "10000");
        nf_assert(&fs, "100000",  "100000");
        nf_assert(&fs, "1000000", "1000000");
        nf_assert(&fs, "0.0001",  "0.0001");
        nf_assert(&fs, "0.00001", "0.00001");
        nf_assert(&fs, "0.000009",    "0.00001");
        nf_assert(&fs, "0.000005",    "0.00001");
        nf_assert(&fs, "0.000004999", "0");
    }

    printf("[NumberFormatter] done\n");
}

int main(void) {
    real_ctx_init();

    test_ufixed113();
    test_quad();
    test_qmath_log2();
    test_number_formatter();

    if (g_failures == 0) {
        printf("All tests passed.\n");
    } else {
        printf("%d test(s) FAILED.\n", g_failures);
    }
    return g_failures ? 1 : 0;
}
