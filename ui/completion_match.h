// CompletionPopup で使われる case-insensitive なマッチ関数。
// CompletionPopup.cpp から取り出してテスト可能にする。

#pragma once
#include <string>

// hay が needle を部分文字列として含むか (大小区別なし)
// needle が空なら true。
bool completion_icontains(const std::string &hay, const std::string &needle);

// s が prefix p で始まるか (大小区別なし)
// p が空なら true。p.size() > s.size() なら false。
bool completion_istartswith(const std::string &s, const std::string &p);
