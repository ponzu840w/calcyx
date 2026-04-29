// completion_filter — 補完候補フィルタの GUI/TUI 共通実装.
//
// shared/sheet_model.h の sheet_candidate_t (C) を C++ の Candidate に
// 載せ替えて、prefix-then-substring のランキングルールでフィルタする.
// 元々 gui/CompletionPopup.cpp と tui/TuiCompletion.cpp に重複していた.

#pragma once

#include <string>
#include <vector>

extern "C" {
#include "completion_match.h"
#include "sheet_model.h"
}

namespace calcyx {

struct Candidate {
    std::string id;           /* 挿入されるテキスト */
    std::string label;        /* 表示ラベル (引数情報付き) */
    std::string description;  /* 説明文 (なければ空) */
    bool        is_function = false;
};

/* sheet_model から候補を全件取得して Candidate ベクタを返す.
 * sheet_model_build_candidates は内部 strdup 済みの const char * を返すので
 * std::string にコピーして所有権を切り離す. */
inline std::vector<Candidate> build_candidates(sheet_model_t *m) {
    const sheet_candidate_t *arr = nullptr;
    int n = sheet_model_build_candidates(m, &arr);
    std::vector<Candidate> out;
    out.reserve(n);
    for (int i = 0; i < n; i++) {
        out.push_back({
            arr[i].id          ? arr[i].id          : "",
            arr[i].label       ? arr[i].label       : "",
            arr[i].description ? arr[i].description : "",
            arr[i].is_function,
        });
    }
    return out;
}

/* prefix-then-substring ランキング:
 *   1. key 空 or completion_istartswith でマッチ -> 上位グループ
 *   2. completion_icontains でマッチ            -> 下位グループ
 *   両グループとも入力順 (アルファベット順を仮定) を維持. */
inline std::vector<Candidate> filter_completion(
    const std::vector<Candidate> &all, const std::string &key)
{
    std::vector<Candidate> prefix, substr;
    const char *key_c = key.c_str();
    for (const auto &c : all) {
        if (key.empty() || completion_istartswith(c.id.c_str(), key_c)) {
            prefix.push_back(c);
        } else if (completion_icontains(c.id.c_str(), key_c)) {
            substr.push_back(c);
        }
    }
    std::vector<Candidate> out;
    out.reserve(prefix.size() + substr.size());
    for (auto &c : prefix) out.push_back(std::move(c));
    for (auto &c : substr) out.push_back(std::move(c));
    return out;
}

}  // namespace calcyx
