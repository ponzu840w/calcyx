#pragma once

#include <string>

/* クロスプラットフォームのクリップボード I/O。
 *
 * 書き込みは外部コマンド (pbcopy / wl-copy / xclip / clip.exe) を優先し、
 * フォールバックで OSC 52 escape sequence を stdout に直接書き込む。
 * 読み込みは外部コマンドのみ (OSC 52 read はサポートが薄く、応答待ちで
 * ブロックする端末があるため使わない)。
 *
 * テスト用にモックバッファ機構を備える: set_mock_for_test(true) を有効化
 * すると以下プロセス内のコピー/ペーストが in-memory バッファに対して
 * 行われ、外部 I/O は発生しない。 */

namespace calcyx::tui::clipboard {

bool write(const std::string &text);
bool read(std::string &out);

/* テスト用 */
void               set_mock_for_test(bool on);
void               set_mock_buffer  (const std::string &t);
const std::string &get_mock_buffer  ();

} // namespace calcyx::tui::clipboard
