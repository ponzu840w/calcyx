#include "completion_match.h"
#include <ctype.h>
#include <string.h>

static int ci_eq(char a, char b) {
    return tolower((unsigned char)a) == tolower((unsigned char)b);
}

bool completion_icontains(const char *hay, const char *needle) {
    if (!needle || !*needle) return true;
    if (!hay) return false;
    size_t nlen = strlen(needle);
    size_t hlen = strlen(hay);
    if (nlen > hlen) return false;
    for (size_t i = 0; i + nlen <= hlen; i++) {
        size_t j = 0;
        for (; j < nlen; j++) {
            if (!ci_eq(hay[i + j], needle[j])) break;
        }
        if (j == nlen) return true;
    }
    return false;
}

bool completion_istartswith(const char *s, const char *p) {
    if (!p || !*p) return true;
    if (!s) return false;
    for (size_t i = 0; p[i]; i++) {
        if (s[i] == '\0') return false;
        if (!ci_eq(s[i], p[i])) return false;
    }
    return true;
}
