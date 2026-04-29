// CompletionPopup のマッチ関数 unit テスト (istartswith / icontains, 大小区別なし)。

#include "completion_match.h"
#include <cstdio>
#include <string>

static int g_failures = 0;

static void check_bool(const char *label, bool got, bool expected) {
    if (got != expected) {
        fprintf(stderr, "FAIL: %s  got=%s expected=%s\n",
                label, got ? "true" : "false", expected ? "true" : "false");
        g_failures++;
    }
}

#define EXPECT_TRUE(label, expr)  check_bool(label, (expr), true)
#define EXPECT_FALSE(label, expr) check_bool(label, (expr), false)

static void test_icontains() {
    EXPECT_TRUE ("icontains: empty needle",         completion_icontains("Hello", ""));
    EXPECT_TRUE ("icontains: empty hay + empty",    completion_icontains("", ""));
    EXPECT_FALSE("icontains: empty hay + nonempty", completion_icontains("", "a"));
    EXPECT_TRUE ("icontains: mid match",            completion_icontains("Hello", "ell"));
    EXPECT_TRUE ("icontains: case insensitive",     completion_icontains("Hello", "ELL"));
    EXPECT_TRUE ("icontains: head match",           completion_icontains("Hello", "He"));
    EXPECT_TRUE ("icontains: tail match",           completion_icontains("Hello", "lo"));
    EXPECT_FALSE("icontains: no match",             completion_icontains("Hello", "xyz"));
    EXPECT_FALSE("icontains: needle longer",        completion_icontains("abc", "abcd"));
    EXPECT_TRUE ("icontains: full match",           completion_icontains("abc", "abc"));
    // builtin 関数名の典型パターン
    EXPECT_TRUE ("icontains: sin in asin",          completion_icontains("asin", "sin"));
    EXPECT_TRUE ("icontains: Sin in asin (CI)",     completion_icontains("asin", "Sin"));
}

static void test_istartswith() {
    EXPECT_TRUE ("istartswith: empty prefix",         completion_istartswith("Hello", ""));
    EXPECT_TRUE ("istartswith: empty both",           completion_istartswith("", ""));
    EXPECT_FALSE("istartswith: empty s + nonempty p", completion_istartswith("", "a"));
    EXPECT_TRUE ("istartswith: prefix",               completion_istartswith("Hello", "He"));
    EXPECT_TRUE ("istartswith: case insensitive",     completion_istartswith("Hello", "HE"));
    EXPECT_TRUE ("istartswith: full match",           completion_istartswith("abc", "abc"));
    EXPECT_FALSE("istartswith: mid match",            completion_istartswith("Hello", "ell"));
    EXPECT_FALSE("istartswith: suffix only",          completion_istartswith("Hello", "lo"));
    EXPECT_FALSE("istartswith: prefix longer than s", completion_istartswith("He", "Hello"));
    // 典型例: sin は asin の prefix ではない
    EXPECT_FALSE("istartswith: sin is not prefix of asin",
                 completion_istartswith("asin", "sin"));
    EXPECT_TRUE ("istartswith: asi is prefix of asin",
                 completion_istartswith("asin", "asi"));
}

int main() {
    test_icontains();
    test_istartswith();
    if (g_failures == 0) {
        printf("All completion_match tests passed.\n");
        return 0;
    }
    fprintf(stderr, "%d test(s) failed.\n", g_failures);
    return 1;
}
