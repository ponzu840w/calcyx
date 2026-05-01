/* path_utf8.c — see path_utf8.h */

#include "path_utf8.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32

#include <windows.h>
#include <wchar.h>
#include <direct.h>
#include <io.h>

/* UTF-8 -> UTF-16 (動的確保、 呼び出し側で free)。 */
static wchar_t *u8_to_w(const char *s) {
    int n;
    wchar_t *w;
    if (!s) return NULL;
    n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    if (n <= 0) return NULL;
    w = (wchar_t *)malloc(sizeof(wchar_t) * (size_t)n);
    if (!w) return NULL;
    if (MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n) <= 0) {
        free(w);
        return NULL;
    }
    return w;
}

/* UTF-16 -> UTF-8. 成功:書き込んだバイト数 (NUL 含む)、 失敗:0. */
static int w_to_u8(const wchar_t *w, char *buf, size_t buflen) {
    int n;
    if (!w || !buf || buflen == 0) return 0;
    n = WideCharToMultiByte(CP_UTF8, 0, w, -1, buf, (int)buflen, NULL, NULL);
    return n > 0 ? n : 0;
}

FILE *calcyx_fopen(const char *path_utf8, const char *mode) {
    wchar_t *wp = u8_to_w(path_utf8);
    wchar_t *wm = u8_to_w(mode);
    FILE *fp = NULL;
    if (wp && wm) fp = _wfopen(wp, wm);
    free(wp);
    free(wm);
    return fp;
}

int calcyx_rename(const char *src_utf8, const char *dst_utf8) {
    wchar_t *ws = u8_to_w(src_utf8);
    wchar_t *wd = u8_to_w(dst_utf8);
    int rc = -1;
    if (ws && wd) rc = _wrename(ws, wd);
    free(ws);
    free(wd);
    return rc;
}

int calcyx_remove(const char *path_utf8) {
    wchar_t *wp = u8_to_w(path_utf8);
    int rc = -1;
    if (wp) rc = _wremove(wp);
    free(wp);
    return rc;
}

int calcyx_mkdir(const char *path_utf8) {
    wchar_t *wp = u8_to_w(path_utf8);
    int rc = -1;
    if (wp) rc = _wmkdir(wp);
    free(wp);
    return rc;
}

int calcyx_getenv_utf8(const char *name, char *buf, size_t buflen) {
    wchar_t *wname;
    const wchar_t *wval;
    int n;
    if (!name || !buf || buflen == 0) return 0;
    wname = u8_to_w(name);
    if (!wname) return 0;
    wval = _wgetenv(wname);
    free(wname);
    if (!wval || !*wval) return 0;
    n = w_to_u8(wval, buf, buflen);
    return n > 0;
}

int calcyx_path_is_dir(const char *path_utf8) {
    wchar_t *wp;
    DWORD attr;
    if (!path_utf8) return 0;
    wp = u8_to_w(path_utf8);
    if (!wp) return 0;
    attr = GetFileAttributesW(wp);
    free(wp);
    if (attr == INVALID_FILE_ATTRIBUTES) return 0;
    return (attr & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
}

int calcyx_dir_iter(const char *path_utf8, calcyx_dir_iter_fn cb, void *user) {
    wchar_t *wp_dir;
    size_t  dir_len, pat_len;
    wchar_t *wpat;
    WIN32_FIND_DATAW fd;
    HANDLE h;
    char namebuf[1024];
    if (!path_utf8 || !cb) return -1;
    wp_dir = u8_to_w(path_utf8);
    if (!wp_dir) return -1;
    dir_len = wcslen(wp_dir);
    pat_len = dir_len + 3;  /* "\\*" + NUL */
    wpat = (wchar_t *)malloc(pat_len * sizeof(wchar_t));
    if (!wpat) { free(wp_dir); return -1; }
    wcscpy(wpat, wp_dir);
    /* セパレータが末尾に無ければ \ を足してから *  */
    if (dir_len > 0 && wp_dir[dir_len - 1] != L'\\' && wp_dir[dir_len - 1] != L'/')
        wcscat(wpat, L"\\");
    wcscat(wpat, L"*");
    free(wp_dir);
    h = FindFirstFileW(wpat, &fd);
    free(wpat);
    if (h == INVALID_HANDLE_VALUE) return -1;
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
            continue;
        if (w_to_u8(fd.cFileName, namebuf, sizeof(namebuf)) > 0)
            cb(namebuf, user);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return 0;
}

#else  /* !_WIN32 */

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>

FILE *calcyx_fopen(const char *path_utf8, const char *mode) {
    return fopen(path_utf8, mode);
}

int calcyx_rename(const char *src_utf8, const char *dst_utf8) {
    return rename(src_utf8, dst_utf8);
}

int calcyx_remove(const char *path_utf8) {
    return remove(path_utf8);
}

int calcyx_mkdir(const char *path_utf8) {
    return mkdir(path_utf8, 0755);
}

int calcyx_getenv_utf8(const char *name, char *buf, size_t buflen) {
    const char *v;
    if (!name || !buf || buflen == 0) return 0;
    v = getenv(name);
    if (!v || !*v) return 0;
    strncpy(buf, v, buflen - 1);
    buf[buflen - 1] = '\0';
    return 1;
}

int calcyx_path_is_dir(const char *path_utf8) {
    struct stat st;
    if (!path_utf8) return 0;
    if (stat(path_utf8, &st) != 0) return 0;
    return S_ISDIR(st.st_mode) ? 1 : 0;
}

int calcyx_dir_iter(const char *path_utf8, calcyx_dir_iter_fn cb, void *user) {
    DIR *dp;
    struct dirent *ent;
    if (!path_utf8 || !cb) return -1;
    dp = opendir(path_utf8);
    if (!dp) return -1;
    while ((ent = readdir(dp)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;
        cb(ent->d_name, user);
    }
    closedir(dp);
    return 0;
}

#endif
