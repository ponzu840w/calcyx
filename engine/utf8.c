/* utf8.c — see utf8.h */

#include "utf8.h"

int calcyx_utf8_encode(int32_t cp, char out[4]) {
    if (cp < 0)            return 0;
    if (cp < 0x80)       { out[0] = (char)cp;                                                  return 1; }
    if (cp < 0x800)      { out[0] = (char)(0xC0 | (cp >> 6));
                           out[1] = (char)(0x80 | (cp & 0x3F));                                return 2; }
    if (cp < 0x10000)    { out[0] = (char)(0xE0 | (cp >> 12));
                           out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
                           out[2] = (char)(0x80 | (cp & 0x3F));                                return 3; }
    if (cp < 0x110000)   { out[0] = (char)(0xF0 | (cp >> 18));
                           out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
                           out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
                           out[3] = (char)(0x80 | (cp & 0x3F));                                return 4; }
    return 0;
}

int calcyx_utf8_decode(const char *s, int32_t *out_cp) {
    unsigned char c0 = (unsigned char)s[0];
    if (c0 < 0x80) {
        *out_cp = c0;
        return 1;
    }
    int32_t cp;
    int need;
    if      ((c0 & 0xE0) == 0xC0) { cp = c0 & 0x1F; need = 1; }
    else if ((c0 & 0xF0) == 0xE0) { cp = c0 & 0x0F; need = 2; }
    else if ((c0 & 0xF8) == 0xF0) { cp = c0 & 0x07; need = 3; }
    else {
        /* 不正な先頭バイト。 1 byte 進めて raw 値を返す。 */
        *out_cp = c0;
        return 1;
    }
    int consumed = 1;
    for (int i = 0; i < need; i++) {
        unsigned char ci = (unsigned char)s[consumed];
        if ((ci & 0xC0) != 0x80) break;  /* 不正な継続バイト or NUL */
        cp = (cp << 6) | (ci & 0x3F);
        consumed++;
    }
    *out_cp = cp;
    return consumed;
}
