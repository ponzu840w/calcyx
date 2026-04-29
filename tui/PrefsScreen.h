/* TUI Preferences 画面 (full-screen 切替方式)。
 *
 * TuiApp::Render() / OnEvent() が prefs_visible_ のとき本クラスに丸投げ。
 * dbox 重ね描きしないので box / clear_under は使わない。 */

#ifndef CALCYX_TUI_PREFS_SCREEN_H
#define CALCYX_TUI_PREFS_SCREEN_H

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <map>
#include <set>
#include <string>

#include "TuiSheet.h"  // TuiPalette

namespace calcyx::tui {

class TuiApp;

class PrefsScreen {
public:
    explicit PrefsScreen(TuiApp *app) : app_(app) {}

    void open();
    void close();
    bool visible() const { return visible_; }

    /* キーイベントを処理。 true なら吸収。 */
    bool OnEvent(ftxui::Event ev);

    /* 全画面を埋める Element を返す。 TuiApp::run() の Renderer から呼ぶ。 */
    ftxui::Element Render() const;

private:
    TuiApp *app_;
    bool    visible_ = false;
    int     tab_     = 0;   /* 0..3 */
    int     item_    = 0;   /* 現タブ内 visible item index */

    /* phase 1 では編集モード未実装。 phase 2 で activate。 */
    bool        editing_ = false;
    std::string edit_buf_;
    size_t      edit_cur_ = 0;

    std::map<std::string, std::string> values_;   /* open 時に conf から load */
    std::set<std::string>              locked_;   /* override 由来 */

    /* 表示中の各項目の絶対 index → kItems インデックスの map (filtering 後)。
     * Render と OnEvent で共有する。 */
    void refresh_visible_items() const;
    mutable std::vector<int> visible_items_;

    /* per-kind 1 行レンダリング。 */
    ftxui::Element render_value(int item_idx) const;
};

} // namespace calcyx::tui

#endif
