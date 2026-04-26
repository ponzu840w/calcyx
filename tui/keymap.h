#ifndef CALCYX_TUI_KEYMAP_H
#define CALCYX_TUI_KEYMAP_H

#include <ftxui/component/event.hpp>

namespace calcyx::tui {

/* アクション列挙。TuiSheet / TuiApp が key → action の変換を一度だけ行い、
 * 処理側は action で switch する (macOS / Windows / Linux 共通のキーバインド)。 */
enum class Action {
    None,

    /* カーソル移動 */
    CursorLeft,
    CursorRight,
    CursorHome,
    CursorEnd,
    CursorWordLeft,   /* Ctrl+Left / Alt+B */
    CursorWordRight,  /* Ctrl+Right / Alt+F */

    /* 行間移動 / 選択 */
    RowUp,
    RowDown,
    RowPageUp,
    RowPageDown,
    RowFirst,         /* Ctrl+Home */
    RowLast,          /* Ctrl+End */

    /* 編集 */
    InsertChar,       /* ev.is_character() のとき。呼び出し側で文字取得 */
    Backspace,
    DeleteChar,
    DeleteWord,       /* Ctrl+W */
    KillLineRight,    /* Ctrl+K */

    /* 行操作 */
    CommitAndInsertBelow,   /* Enter */
    InsertAbove,            /* Shift+Enter, Alt+Enter */
    DeleteRow,              /* Ctrl+Del, Ctrl+BS */
    DeleteRowUp,            /* Shift+Del, Shift+BS, BS on empty row */
    MoveRowUp,              /* Ctrl+Shift+Up */
    MoveRowDown,            /* Ctrl+Shift+Down */

    /* history */
    Undo,
    Redo,
    Recalculate,            /* F5 */

    /* 全体操作 */
    ClearAll,               /* Ctrl+Shift+Del */
    CopyAll,                /* Ctrl+Shift+C (CSI-u) / Alt+C (フォールバック) */

    /* クリップボード (現在行 / 編集中バッファ) */
    Copy,                   /* Ctrl+C: 現在行を `expr = result` 形式でコピー */
    Cut,                    /* Ctrl+X: コピー + 行削除 */
    Paste,                  /* Ctrl+V: クリップボードを現在のカーソル位置に挿入 */

    /* 表示オプション */
    DecimalsInc,            /* Alt+. (GUI は Ctrl+Shift+.) */
    DecimalsDec,            /* Alt+, (GUI は Ctrl+Shift+,) */
    ToggleCompact,          /* Ctrl+: (GUI と同) / F6 / Alt+Z */

    /* format 切替 (F8-F12) */
    FormatAuto,
    FormatDec,
    FormatHex,
    FormatBin,
    FormatSI,

    /* file */
    FileOpen,
    FileSave,
    FileSaveAs,

    /* 補完 */
    CompletionTrigger,      /* Tab / Ctrl+Space */

    /* 終了 */
    Quit,                   /* Ctrl+Q */
};

Action map(const ftxui::Event &ev);

} // namespace calcyx::tui

#endif
