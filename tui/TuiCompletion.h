#ifndef CALCYX_TUI_COMPLETION_H
#define CALCYX_TUI_COMPLETION_H

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>

#include <string>
#include <vector>

#include "sheet_model.h"
#include "completion_filter.hpp"

namespace calcyx::tui {

/* 補完ドロップダウン。sheet_model_build_candidates で取得した全候補を
 * 内部に保持し、キー (現在のカーソル位置の識別子) でフィルタ・ランキングする。
 *
 * フィルタ・ランキングロジックは shared/completion_filter.hpp に GUI と
 * 共通化されている。 */
class TuiCompletion {
public:
    using Item = calcyx::Candidate;

    /* sheet_model から候補を再構築。open() 前に毎回呼ぶ想定。 */
    void reload(sheet_model_t *model);

    void open (const std::string &key);
    void hide ();
    bool visible() const { return visible_; }

    void update_key  (const std::string &key);
    void move_selection(int dir);  /* -1 / +1; filter 内を循環 */

    /* 現在選択中の item。なければ nullptr。 */
    const Item *selected() const;

    /* key prefix 長。補完確定時、[pos - key_len, pos] を replace する目印。 */
    const std::string &key() const { return key_; }

    /* 最大 max_rows 行でドロップダウンをレンダー。 */
    ftxui::Element render(int max_rows = 8) const;

    int filtered_count() const { return (int)filtered_.size(); }

    /* マウス対応: render() 中に各項目の Box が item_boxes_ に格納される。 *
     * (x,y) を含む項目の絶対インデックスを返す。なければ -1。 */
    int  item_at(int x, int y) const;
    void set_selected(int idx);  /* 範囲外は無視 */

private:
    void rebuild();

    std::vector<Item> all_;
    std::vector<Item> filtered_;
    std::string       key_;
    int               selected_ = 0;
    bool              visible_  = false;

    /* render() で更新 (mutable) */
    mutable std::vector<ftxui::Box> item_boxes_;
};

} // namespace calcyx::tui

#endif
