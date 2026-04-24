/* 補完候補のランキングに使う case-insensitive な文字列マッチ関数。
 * UI 非依存のため GUI (FLTK) / TUI (FTXUI) / CLI から共有して使う。
 * 移植元: ui/CompletionPopup.cpp 内の補完フィルタ。 */

#ifndef CALCYX_SHARED_COMPLETION_MATCH_H
#define CALCYX_SHARED_COMPLETION_MATCH_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* hay が needle を部分文字列として含むか (大小区別なし)。
 * needle が NULL または空文字列のとき true。 */
bool completion_icontains(const char *hay, const char *needle);

/* s が prefix p で始まるか (大小区別なし)。
 * p が NULL または空文字列のとき true。 */
bool completion_istartswith(const char *s, const char *p);

#ifdef __cplusplus
}
#endif

#endif
