#include "keymap.h"

namespace calcyx::tui {

/* FTXUI v5: Ctrl+X = Special({X}) (0x01..0x1a)。 修飾矢印は Xterm:
 *   Shift+Arrow=ESC[1;2<dir>, Alt=1;3, Ctrl=1;5, Ctrl+Shift=1;6 */
Action map(const ftxui::Event &ev) {
    using E = ftxui::Event;

    /* 終了: Ctrl+Q のみ。
     * Ctrl+C は Copy に充てるため Quit から外した (ISIG を切ってあるので
     * SIGINT には化けず、生の 0x03 が stdin に届く)。 */
    if (ev == E::Special("\x11")) return Action::Quit;

    /* クリップボード (現在行 / 編集中バッファ)。
     * Ctrl+X (0x18) と Ctrl+V (0x16) は FTXUI の C0 DROP パッチで
     * 通るようになっている (cmake/deps.cmake の PATCH_COMMAND)。 */
    if (ev == E::Special("\x03")) return Action::Copy;   /* Ctrl+C */
    if (ev == E::Special("\x18")) return Action::Cut;    /* Ctrl+X */
    if (ev == E::Special("\x16")) return Action::Paste;  /* Ctrl+V */

    /* Enter / Shift+Enter */
    if (ev == E::Return) return Action::CommitAndInsertBelow;
    if (ev == E::Special("\x1b[13;2u")    /* Kitty CSI-u Shift+Enter */
     || ev == E::Special("\x1b\r")        /* Alt+Enter */
     || ev == E::Special("\x1b\x0a"))
        return Action::InsertAbove;

    /* 履歴: Ctrl+Z/Y. 通すための仕込み:
     *   - FTXUI の C0 DROP パッチ (cmake/deps.cmake)
     *   - termios ISIG/IEXTEN off (TuiApp::run; Ctrl+Z が SIGTSTP に化けない) */
    if (ev == E::Special("\x1a")) return Action::Undo;  /* Ctrl+Z */
    if (ev == E::Special("\x19")) return Action::Redo;  /* Ctrl+Y */

    /* 行移動 (Ctrl+Shift+Up/Down; Alt+Up/Down もフォールバック) */
    if (ev == E::Special("\x1b[1;6A")) return Action::MoveRowUp;
    if (ev == E::Special("\x1b[1;6B")) return Action::MoveRowDown;
    if (ev == E::Special("\x1b[1;3A")) return Action::MoveRowUp;     /* Alt+Up */
    if (ev == E::Special("\x1b[1;3B")) return Action::MoveRowDown;   /* Alt+Down */

    /* 矢印・ホーム・エンド */
    if (ev == E::ArrowUp)    return Action::RowUp;
    if (ev == E::ArrowDown)  return Action::RowDown;
    if (ev == E::ArrowLeft)  return Action::CursorLeft;
    if (ev == E::ArrowRight) return Action::CursorRight;
    if (ev == E::Home || ev == E::Special("\x01")) return Action::CursorHome; /* Ctrl+A */
    if (ev == E::End  || ev == E::Special("\x05")) return Action::CursorEnd;  /* Ctrl+E */
    if (ev == E::PageUp)   return Action::RowPageUp;
    if (ev == E::PageDown) return Action::RowPageDown;

    /* 単語移動 (Ctrl+Arrow) */
    if (ev == E::ArrowLeftCtrl)  return Action::CursorWordLeft;
    if (ev == E::ArrowRightCtrl) return Action::CursorWordRight;

    /* 編集 */
    if (ev == E::Backspace) return Action::Backspace;
    if (ev == E::Delete)    return Action::DeleteChar;
    if (ev == E::Special("\x17")) return Action::DeleteWord;    /* Ctrl+W */
    if (ev == E::Special("\x0b")) return Action::KillLineRight; /* Ctrl+K */

    /* 行削除: Ctrl+Del=CSI 3;5~, Ctrl+BS=0x08 (xterm 既定)。 GUI と同じ。
     * 旧 Ctrl+D は廃止 (端末 EOF 慣習との衝突回避)。 */
    if (ev == E::Special("\x1b[3;5~")) return Action::DeleteRow;
    if (ev == E::Special("\x08"))      return Action::DeleteRow;
    /* delete_row_up: Shift+Del = CSI 3;2~。Shift+BS は端末では BS と区別不能 */
    if (ev == E::Special("\x1b[3;2~")) return Action::DeleteRowUp;

    /* 全体操作 */
    if (ev == E::Special("\x1b[3;6~"))  return Action::ClearAll;   /* Ctrl+Shift+Del */
    if (ev == E::Special("\x1b[67;6u")) return Action::CopyAll;    /* Ctrl+Shift+C (CSI-u) */
    if (ev == E::Special("\x1b" "c"))   return Action::CopyAll;    /* Alt+C (フォールバック) */
    if (ev == E::F5)                    return Action::Recalculate;

    /* 小数桁。 GUI の Ctrl+Shift+. / , は端末では拾えないため Alt+./, を使う。
     * 新しめの端末 (kitty/foot) で CSI-u が有効な場合の \x1b[46;6u / \x1b[44;6u
     * もサポートしておく。 */
    if (ev == E::Special("\x1b."))       return Action::DecimalsInc;
    if (ev == E::Special("\x1b,"))       return Action::DecimalsDec;
    if (ev == E::Special("\x1b[46;6u"))  return Action::DecimalsInc;
    if (ev == E::Special("\x1b[44;6u"))  return Action::DecimalsDec;

    /* コンパクトモード切替。 Ctrl+: が第一候補、 F6 と Alt+z は CSI-u 非対応
     * 端末向けフォールバック。 Ctrl+: は kitty/foot CSI-u: \x1b[58;5u 等。 */
    if (ev == E::Special("\x1b[58;5u")) return Action::ToggleCompact;
    if (ev == E::Special("\x1b[59;6u")) return Action::ToggleCompact;
    if (ev == E::F6)                    return Action::ToggleCompact;
    if (ev == E::Special("\x1b" "z"))   return Action::ToggleCompact;

    /* format (F8-F12) */
    if (ev == E::F8)  return Action::FormatAuto;
    if (ev == E::F9)  return Action::FormatDec;
    if (ev == E::F10) return Action::FormatHex;
    if (ev == E::F11) return Action::FormatBin;
    if (ev == E::F12) return Action::FormatSI;

    /* file */
    if (ev == E::Special("\x0f")) return Action::FileOpen; /* Ctrl+O */
    if (ev == E::Special("\x13")) return Action::FileSave; /* Ctrl+S */

    /* 補完 */
    if (ev == E::Tab)             return Action::CompletionTrigger;
    if (ev == E::Special("\x00")) return Action::CompletionTrigger; /* Ctrl+Space */

    /* 文字入力 */
    if (ev.is_character()) return Action::InsertChar;

    return Action::None;
}

} // namespace calcyx::tui
