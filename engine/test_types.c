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

static void assert_quad_eq(const char *label, quad_t got, quad_t expected) {
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
    quad_t qa = quad_from_real(&ra);
    quad_t qb = quad_from_real(&rb);
    quad_t qq = quad_from_real(&rq);
    snprintf(label, sizeof(label), "[quad] %s+%s", as, bs);
    assert_quad_eq(label, quad_add(qa, qb), qq);
}

/* quad.cs: AssertSub(decimal a, decimal b, decimal q) */
static void quad_assert_sub(const char *as, const char *bs, const char *qs) {
    char label[128];
    real_t ra, rb, rq;
    real_from_str(&ra, as); real_from_str(&rb, bs); real_from_str(&rq, qs);
    quad_t qa = quad_from_real(&ra);
    quad_t qb = quad_from_real(&rb);
    quad_t qq = quad_from_real(&rq);
    snprintf(label, sizeof(label), "[quad] %s-%s", as, bs);
    assert_quad_eq(label, quad_sub(qa, qb), qq);
}

/* quad.cs: AssertMul(decimal a, decimal b, decimal q) */
static void quad_assert_mul(const char *as, const char *bs, const char *qs) {
    char label[128];
    real_t ra, rb, rq;
    real_from_str(&ra, as); real_from_str(&rb, bs); real_from_str(&rq, qs);
    quad_t qa = quad_from_real(&ra);
    quad_t qb = quad_from_real(&rb);
    quad_t qq = quad_from_real(&rq);
    snprintf(label, sizeof(label), "[quad] %s*%s", as, bs);
    assert_quad_eq(label, quad_mul(qa, qb), qq);
}

/* quad.cs: AssertTrunc(decimal a, decimal q) */
static void quad_assert_trunc(const char *as, const char *qs) {
    char label[128];
    real_t ra, rq;
    real_from_str(&ra, as); real_from_str(&rq, qs);
    quad_t qa = quad_from_real(&ra);
    quad_t qq = quad_from_real(&rq);
    snprintf(label, sizeof(label), "[quad] Truncate(%s)", as);
    assert_quad_eq(label, quad_truncate(qa), qq);
}

/* quad.cs: Assert.Equal("[quad] ", (decimal)(quad)X, X) */
static void quad_assert_roundtrip(const char *as) {
    char label[128];
    real_t ra, back;
    real_from_str(&ra, as);
    quad_t qa = quad_from_real(&ra);
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

int main(void) {
    real_ctx_init();

    test_ufixed113();
    test_quad();

    if (g_failures == 0) {
        printf("All tests passed.\n");
    } else {
        printf("%d test(s) FAILED.\n", g_failures);
    }
    return g_failures ? 1 : 0;
}
