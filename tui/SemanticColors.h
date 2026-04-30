/* tui_color_source = semantic 用の色名 ↔ FTXUI Color enum マッピング。
 * PrefsScreen (= 編集 UI) と TuiSheet (= 描画) で共有する。 */

#ifndef CALCYX_TUI_SEMANTIC_COLORS_H
#define CALCYX_TUI_SEMANTIC_COLORS_H

#include <ftxui/screen/color.hpp>
#include <string>

namespace calcyx::tui {

struct SemanticColorChoice {
    const char  *name;     /* "cyan-light" 等の小文字ハイフン区切り */
    ftxui::Color color;
};

/* default + ANSI 16 の合計 17 色。 entries 順は PrefsScreen の Choice 循環順。 */
extern const SemanticColorChoice kSemanticColors[];
extern const int                 kSemanticColorCount;

/* 名前から FTXUI Color を引く。 未知ならば fallback を返す。 */
ftxui::Color parse_semantic_color(const std::string &name,
                                  ftxui::Color       fallback);

} // namespace calcyx::tui

#endif
