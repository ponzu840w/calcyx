/* OS ロケール検出。 POSIX: LC_ALL → LC_MESSAGES → LANG.
 * Windows: GetUserDefaultLocaleName, UTF-16 → UTF-8 → 先頭 2 文字。 */

#include "i18n.h"
#include "path_utf8.h"

#include <string.h>

#ifdef _WIN32
#  include <windows.h>
#endif

const char *calcyx_locale_detect(void) {
#ifdef _WIN32
    static char buf[16];
    wchar_t wname[LOCALE_NAME_MAX_LENGTH];
    int n = GetUserDefaultLocaleName(wname,
                                     (int)(sizeof(wname) / sizeof(wname[0])));
    if (n <= 0) return "en";
    if (WideCharToMultiByte(CP_UTF8, 0, wname, -1, buf, (int)sizeof(buf),
                            NULL, NULL) <= 0) return "en";
    return calcyx_locale_normalize(buf);
#else
    static char buf[64];
    if (calcyx_getenv_utf8("LC_ALL", buf, sizeof(buf)))      return calcyx_locale_normalize(buf);
    if (calcyx_getenv_utf8("LC_MESSAGES", buf, sizeof(buf))) return calcyx_locale_normalize(buf);
    if (calcyx_getenv_utf8("LANG", buf, sizeof(buf)))        return calcyx_locale_normalize(buf);
    return "en";
#endif
}
