#pragma once

#include <string>

/* クロスプラットフォームのクリップボード I/O.
 * write: 外部コマンド (pbcopy/wl-copy/xclip/clip.exe) → OSC 52 fallback.
 * read : 外部コマンドのみ (OSC 52 read は端末ブロックリスクで非採用)。
 * テスト用 set_mock_for_test() で in-memory バッファに切替可能。 */

namespace calcyx::tui::clipboard {

bool write(const std::string &text);
bool read(std::string &out);

/* テスト用 */
void               set_mock_for_test(bool on);
void               set_mock_buffer  (const std::string &t);
const std::string &get_mock_buffer  ();

} // namespace calcyx::tui::clipboard
