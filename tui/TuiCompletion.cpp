#include "TuiCompletion.h"

#include "completion_match.h"

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

#include <algorithm>

using namespace ftxui;

namespace calcyx::tui {

void TuiCompletion::reload(sheet_model_t *model) {
    const sheet_candidate_t *arr = nullptr;
    int n = sheet_model_build_candidates(model, &arr);
    all_.clear();
    all_.reserve(n);
    for (int i = 0; i < n; ++i) {
        Item it;
        it.id          = arr[i].id          ? arr[i].id          : "";
        it.label       = arr[i].label       ? arr[i].label       : it.id;
        it.description = arr[i].description ? arr[i].description : "";
        it.is_function = arr[i].is_function;
        all_.push_back(std::move(it));
    }
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
    std::vector<Item> prefix, substr;
    const char *key_c = key_.c_str();
    for (const auto &c : all_) {
        if (key_.empty() || completion_istartswith(c.id.c_str(), key_c)) {
            prefix.push_back(c);
        } else if (completion_icontains(c.id.c_str(), key_c)) {
            substr.push_back(c);
        }
    }
    filtered_.clear();
    filtered_.reserve(prefix.size() + substr.size());
    for (auto &c : prefix) filtered_.push_back(std::move(c));
    for (auto &c : substr) filtered_.push_back(std::move(c));
}

Element TuiCompletion::render(int max_rows) const {
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

} // namespace calcyx::tui
