/* TUI Preferences 画面 (full-screen 切替方式)。
 *
 * TuiApp::Render() / OnEvent() が prefs_visible_ のとき本クラスに丸投げ。
 * dbox 重ね描きしないので box / clear_under は使わない。 */

#ifndef CALCYX_TUI_PREFS_SCREEN_H
#define CALCYX_TUI_PREFS_SCREEN_H

#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>
#include <map>
#include <set>
#include <string>
#include <vector>

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

    /* --- テスト用アクセサ (本番コードからは使わない) --- */
    int  test_tab()             const { return tab_; }
    int  test_item()            const { return item_; }
    bool test_editing()         const { return editing_; }
    bool test_confirming_reset() const { return confirming_reset_; }
    int  test_visible_count()   const { return (int)visible_items_.size(); }
    bool test_locked(const char *key) const {
        return key && locked_.count(key) > 0;
    }
    std::string test_value(const char *key) const {
        if (!key) return "";
        auto it = values_.find(key);
        return (it != values_.end()) ? it->second : std::string();
    }

private:
    TuiApp *app_;
    bool    visible_ = false;
    int     tab_     = 0;   /* 0..3 */
    int     item_    = 0;   /* 現タブ内 visible item index */

    /* phase 1 では編集モード未実装。 phase 2 で activate。 */
    bool        editing_ = false;
    std::string edit_buf_;
    size_t      edit_cur_ = 0;

    /* Reset all settings の Y/N 確認モード (= 破壊的なので 1 段挟む)。 */
    bool        confirming_reset_ = false;

    std::map<std::string, std::string> values_;   /* open 時に conf から load */
    std::set<std::string>              locked_;   /* override 由来 */

    /* 表示中の各項目の絶対 index → kItems インデックスの map (filtering 後)。
     * Render と OnEvent で共有する。 */
    void refresh_visible_items() const;
    mutable std::vector<int> visible_items_;

    /* per-kind 1 行レンダリング。 値のテキスト部分と色サンプル部分は
     * 別 Element として返す。 これは選択行に inverted を当てるとき色
     * サンプルまで反転して潰れるのを避けるため (Render 側で色サンプルを
     * 反転外に置く)。 */
    ftxui::Element render_value(int item_idx) const;
    ftxui::Element render_color_sample(int item_idx) const;

    /* 現項目に値を確定 + conf 書き戻し + apply_settings_from_conf 再呼出し。 */
    void commit_current(const std::string &new_val);

    /* 全 schema key を default 値で書き戻し、 values_ を再読込する。
     * Y/N 確認後に activate_current から呼ぶ。 */
    void do_reset_all();

    /* Enter/Space で発火する選択項目の動作。 マウスクリックからも呼ぶ。 */
    void activate_current();
    /* ←/→ で値を ±1 ステップ進める動作 (BOOL toggle / Choice 循環 / INT ±1)。 */
    void shift_current(int dir);
    /* タブ切替 (= reset item_, refresh visible, overlay_closed)。 */
    void set_tab(int new_tab);

    /* マウスイベント処理。 OnEvent から ev.is_mouse() のとき呼ぶ。 */
    bool handle_mouse(const ftxui::Mouse &m);

    /* マウス hit-test 用 Box (= Render で reflect で更新)。 */
    mutable std::vector<ftxui::Box> tab_boxes_;
    mutable std::vector<ftxui::Box> row_boxes_;     /* visible_items_ と同 index */
    mutable ftxui::Box              close_box_;     /* タイトルバー右の [X] */
};

} // namespace calcyx::tui

#endif
