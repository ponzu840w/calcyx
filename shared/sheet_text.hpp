// sheet_text — シート行のテキスト整形と行分割の GUI/TUI 共通ヘルパー.
//
// 元々 GUI (SheetView::copy_all_to_clipboard / PasteOptionForm::split_lines)
// と TUI (action_copy / action_copy_all / split_lines in anonymous ns) で
// 重複していた処理をここに集約する.
//
// クリップボード I/O 自体 (Fl::copy / OSC52) は各 UI 個別のままで、
// テキスト整形と行分割だけが共有対象.

#pragma once

#include <string>
#include <vector>

extern "C" {
#include "sheet_model.h"
}

namespace calcyx {

/* 1 行を `expr [= result]` 形式で整形する.
 * with_visibility が true なら row_visible() が false の行は result を省略
 * (TUI の挙動). false ならエラー以外は常に "expr = result" (元の GUI 挙動). */
inline std::string format_row_for_copy(sheet_model_t *m, int row,
                                        bool with_visibility = true) {
    const char *expr   = sheet_model_row_expr(m, row);
    const char *result = sheet_model_row_result(m, row);
    bool visible       = with_visibility ? sheet_model_row_visible(m, row) : true;

    std::string text = expr ? expr : "";
    if (result && result[0] && visible) {
        text += " = ";
        text += result;
    }
    return text;
}

/* 全行を改行区切りでまとめて整形 (末尾改行あり). */
inline std::string format_all_rows_for_copy(sheet_model_t *m,
                                             bool with_visibility = true) {
    std::string out;
    int n = sheet_model_row_count(m);
    for (int i = 0; i < n; i++) {
        out += format_row_for_copy(m, i, with_visibility);
        out += "\n";
    }
    return out;
}

/* \r\n / \n / \r どれでも 1 行とみなして分割.
 * drop_trailing_empty: 末尾の空行を捨てる (コピー由来の trailing newline 用). */
inline std::vector<std::string> split_lines(const std::string &text,
                                             bool drop_trailing_empty = false) {
    std::vector<std::string> lines;
    std::string line;
    for (size_t i = 0; i < text.size(); i++) {
        char c = text[i];
        if (c == '\r') {
            lines.push_back(line); line.clear();
            if (i + 1 < text.size() && text[i + 1] == '\n') ++i;
        } else if (c == '\n') {
            lines.push_back(line); line.clear();
        } else {
            line += c;
        }
    }
    if (!line.empty()) lines.push_back(line);
    if (drop_trailing_empty) {
        while (!lines.empty() && lines.back().empty()) lines.pop_back();
    }
    return lines;
}

}  // namespace calcyx
