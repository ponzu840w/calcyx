/* settings_io.c — see settings_io.h */

#include "settings_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <direct.h>
#  define MKDIR(p) _mkdir(p)
#else
#  include <sys/stat.h>
#  include <sys/types.h>
#  define MKDIR(p) mkdir((p), 0755)
#endif

/* ---- default conf path ---- */

int calcyx_default_conf_path(char *buf, size_t buflen) {
    if (!buf || buflen < 16) return 0;
#if defined(_WIN32)
    const char *appdata = getenv("APPDATA");
    if (!appdata || !*appdata) return 0;
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s\\calcyx", appdata);
    MKDIR(dir);
    snprintf(buf, buflen, "%s\\calcyx.conf", dir);
    return 1;
#elif defined(__APPLE__)
    const char *home = getenv("HOME");
    if (!home || !*home) return 0;
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s/Library/Application Support/calcyx", home);
    MKDIR(dir);
    snprintf(buf, buflen, "%s/calcyx.conf", dir);
    return 1;
#else
    char dir[1024];
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg) {
        snprintf(dir, sizeof(dir), "%s/calcyx", xdg);
    } else {
        const char *home = getenv("HOME");
        if (!home || !*home) return 0;
        snprintf(dir, sizeof(dir), "%s/.config/calcyx", home);
    }
    MKDIR(dir);
    snprintf(buf, buflen, "%s/calcyx.conf", dir);
    return 1;
#endif
}

/* ---- conf reader ---- */

int calcyx_conf_each(const char *path, calcyx_conf_kv_fn cb, void *user) {
    FILE *fp;
    char  line[1024];
    int   line_no = 0;
    if (!path || !cb) return -1;
    fp = fopen(path, "rb");
    if (!fp) return -1;
    while (fgets(line, sizeof(line), fp)) {
        char *p, *eq, *ke, *vs, *ve;
        size_t klen, vlen;
        char  key[256], val[768];
        line_no++;
        /* trim trailing \r\n */
        {
            size_t L = strlen(line);
            while (L > 0 && (line[L-1] == '\n' || line[L-1] == '\r')) line[--L] = '\0';
        }
        p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') continue;
        if (*p == '#') {
            /* '#<key> = <value>' 形式 (writer 自動生成 / 手書きの commented
             * 値) は値として読む. '# 自由文', '##...' は通常コメントとして
             * 無視. これにより color_* を commented で温存する仕組みが
             * 起動セッション越しに有効になる. */
            char *q = p + 1;
            if (*q == '\0' || *q == ' ' || *q == '\t' || *q == '#') continue;
            p = q;
        }
        eq = strchr(p, '=');
        if (!eq) continue;
        /* key range */
        ke = eq;
        while (ke > p && (ke[-1] == ' ' || ke[-1] == '\t')) ke--;
        if (ke == p) continue;
        klen = (size_t)(ke - p);
        if (klen >= sizeof(key)) klen = sizeof(key) - 1;
        memcpy(key, p, klen);
        key[klen] = '\0';
        /* value range */
        vs = eq + 1;
        while (*vs == ' ' || *vs == '\t') vs++;
        ve = vs + strlen(vs);
        while (ve > vs && (ve[-1] == ' ' || ve[-1] == '\t')) ve--;
        vlen = (size_t)(ve - vs);
        if (vlen >= sizeof(val)) vlen = sizeof(val) - 1;
        memcpy(val, vs, vlen);
        val[vlen] = '\0';
        cb(key, val, line_no, user);
    }
    fclose(fp);
    return 0;
}

/* ---- value parsers ---- */

int calcyx_conf_parse_bool(const char *s, int *out) {
    if (!s || !out) return 0;
    if (strcmp(s, "true") == 0 || strcmp(s, "1") == 0 || strcmp(s, "yes") == 0) {
        *out = 1; return 1;
    }
    if (strcmp(s, "false") == 0 || strcmp(s, "0") == 0 || strcmp(s, "no") == 0) {
        *out = 0; return 1;
    }
    return 0;
}

int calcyx_conf_parse_int(const char *s, int *out) {
    char *end;
    long v;
    if (!s || !out) return 0;
    if (*s == '\0') return 0;
    v = strtol(s, &end, 10);
    if (end == s) return 0;
    while (*end == ' ' || *end == '\t') end++;
    if (*end != '\0') return 0;
    *out = (int)v;
    return 1;
}

static int hexnyb(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int calcyx_conf_parse_hex_color(const char *s, unsigned char rgb[3]) {
    int i;
    if (!s || !rgb) return 0;
    if (s[0] != '#') return 0;
    if (strlen(s) != 7) return 0;
    for (i = 0; i < 3; i++) {
        int hi = hexnyb(s[1 + i*2]);
        int lo = hexnyb(s[2 + i*2]);
        if (hi < 0 || lo < 0) return 0;
        rgb[i] = (unsigned char)((hi << 4) | lo);
    }
    return 1;
}
