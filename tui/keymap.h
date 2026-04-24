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
    DeleteRow,              /* Ctrl+D */
    MoveRowUp,              /* Ctrl+Shift+Up */
    MoveRowDown,            /* Ctrl+Shift+Down */

    /* history */
    Undo,
    Redo,

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
