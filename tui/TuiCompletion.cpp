#include "TuiCompletion.h"

#include "completion_match.h"

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

#include <algorithm>

using namespace ftxui;

namespace calcyx::tui {

void TuiCompletion::reload(sheet_model_t *model) {
    all_ = calcyx::build_candidates(model);
    /* TUI 表示は label が空のとき id を表示する都合があるが、
     * build_candidates は label が NULL のとき "" を入れる.
     * 後段 render() 側で空 label を id にフォールバックする. */
}

void TuiCompletion::open(const std::string &key) {
    key_     = key;
    selected_ = 0;
    visible_ = true;
    rebuild();
    if (filtered_.empty()) visible_ = false;
}

void TuiCompletion::hide() {
    visible_ = false;
    filtered_.clear();
}

void TuiCompletion::update_key(const std::string &key) {
    if (!visible_) return;
    key_ = key;
    rebuild();
    if (filtered_.empty()) { visible_ = false; return; }
    selected_ = std::min(selected_, (int)filtered_.size() - 1);
}

void TuiCompletion::move_selection(int dir) {
    if (filtered_.empty()) return;
    int n = (int)filtered_.size();
    selected_ = ((selected_ + dir) % n + n) % n;
}

const TuiCompletion::Item *TuiCompletion::selected() const {
    if (filtered_.empty()) return nullptr;
    if (selected_ < 0 || selected_ >= (int)filtered_.size()) return nullptr;
    return &filtered_[selected_];
}

void TuiCompletion::rebuild() {
    filtered_ = calcyx::filter_completion(all_, key_);
}

Element TuiCompletion::render(int max_rows) const {
    /* 描画ごとに item_boxes_ をフルサイズに作り直す。
     * 描画されない項目は Contain() が常に false の (0,0,0,0) のまま。 */
    item_boxes_.assign(filtered_.size(), Box{});

    if (!visible_ || filtered_.empty()) return text("");

    Elements rows;
    int n   = (int)filtered_.size();
    int sel = std::clamp(selected_, 0, n - 1);

    /* 選択中アイテムが窓内に収まるよう window 範囲を決める */
    int start = std::max(0, std::min(sel - max_rows / 2, n - max_rows));
    start     = std::max(0, start);
    int end   = std::min(n, start + max_rows);

    for (int i = start; i < end; ++i) {
        const auto &it = filtered_[i];
        std::string label = it.label.empty() ? it.id : it.label;
        Element row = text(label);
        if (i == sel) row = row | inverted;
        row = row | reflect(item_boxes_[i]);
        rows.push_back(row);
    }

    /* 下部に説明文 */
    const Item *cur = selected();
    std::string desc = cur ? cur->description : "";
    Element desc_el = text(desc) | dim;

    return vbox({
        vbox(std::move(rows)) | frame,
        desc_el,
    }) | border | color(Color::CyanLight);
}

int TuiCompletion::item_at(int x, int y) const {
    for (int i = 0; i < (int)item_boxes_.size(); ++i) {
        if (item_boxes_[i].Contain(x, y)) return i;
    }
    return -1;
}

void TuiCompletion::set_selected(int idx) {
    if (idx < 0 || idx >= (int)filtered_.size()) return;
    selected_ = idx;
}

} // namespace calcyx::tui
