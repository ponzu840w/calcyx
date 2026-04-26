#ifndef CALCYX_TUI_APP_H
#define CALCYX_TUI_APP_H

#include <ftxui/component/component_base.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/screen/box.hpp>

#include <memory>
#include <string>
#include <vector>

#include "sheet_model.h"

namespace calcyx::tui {

class TuiSheet;

/* メニュー識別子。None はメニュー未展開。 */
enum class MenuId {
    None, File, Edit, View, Format,
};

/* 各メニュー項目が表すコマンド。invoke_menu_cmd() で switch する。 */
enum class MenuCmd {
    None,
    /* File */
    Open, Save, SamplesSubmenu, ClearAll, Preferences, About, Exit,
    /* Edit */
    Undo, Redo, CopyAll, InsertBelow, InsertAbove,
    DeleteRow, MoveRowUp, MoveRowDown, Recalculate,
    /* View */
    ToggleCompact, DecimalsInc, DecimalsDec, ToggleAutoComplete,
    /* Format */
    FormatAuto, FormatDec, FormatHex, FormatBin, FormatSI,
    /* Samples: Enter で開いているサンプルファイル (sample_item_ で選択) */
    OpenSample,
};

/* メニュー項目定義。label の '&' の直後 1 文字がホット文字。
 * separator=true ならほかのフィールドは無視して区切り線。 */
struct MenuItem {
    const char *label;     /* "&Open..." など。"-" なら separator */
    const char *shortcut;  /* 右寄せ表示、空文字なら表示しない */
    MenuCmd     cmd;
    bool        separator;
    bool        disabled;
    bool        submenu;   /* 右に ▶、Enter / → で submenu へ */
};

/* ルートアプリケーション。sheet_model を所有し、
 *   - TuiSheet (メインビュー)
 *   - ファイルパスミニプロンプト (Ctrl+O / Ctrl+S で起動)
 * を束ねる。ScreenInteractive の Loop() を提供する。 */
class TuiApp {
public:
    TuiApp();
    ~TuiApp();

    int run(const std::string &initial_file);

    /* --- テスト用フック (本番コードからは使わない) ---
     * run() は ScreenInteractive::Loop() をブロッキング実行するため、
     * CatchEvent 相当のディスパッチをテストから呼べる形で公開する。 */
    void test_dispatch(ftxui::Event ev);
    bool test_prompt_active() const { return prompt_mode_ != PromptMode::None; }
    const std::string& test_prompt_buf()   const { return prompt_buf_; }
    const std::string& test_prompt_label() const { return prompt_label_; }
    const std::string& test_status_msg()   const { return status_message_; }
    sheet_model_t *test_model() const { return model_; }
    TuiSheet      *test_sheet() const { return sheet_.get(); }
    bool test_about_visible() const { return about_visible_; }
    int  test_about_scroll()  const { return about_scroll_; }
    MenuId test_menu_active() const { return menu_active_; }
    int    test_menu_item()   const { return menu_item_; }
    bool   test_submenu_active() const { return submenu_active_; }
    bool   test_paste_modal_visible() const { return paste_modal_visible_; }
    int    test_paste_modal_choice()  const { return paste_modal_choice_; }
    bool   test_context_menu_visible() const { return context_menu_visible_; }
    int    test_context_menu_item()    const { return context_menu_item_; }
    void   test_open_context_menu(int x, int y) { context_menu_open(x, y); }

private:
    enum class PromptMode {
        None,
        Open,
        Save,
    };

    void prompt_begin(PromptMode mode, const std::string &initial);
    bool prompt_handle_event(ftxui::Event ev);  /* true で吸収 */
    void prompt_submit();
    void prompt_cancel();

    void do_file_save();
    void do_file_open();
    /* GUI と同じ calcyx.conf を $VISUAL/$EDITOR (Unix) や関連付けエディタ
     * (Windows) で開く。FTXUI の端末ハンドリングを WithRestoredIO で一時退避。
     * TUI 自身は現状 calcyx.conf を読み込まないため、編集内容は GUI からのみ
     * 有効。flash で「次回 GUI 起動から有効」とフィードバックする。 */
    void do_preferences();
    /* calcyx.conf からエンジン共通の設定を読み込んで反映する。
     * GUI と共有する小数桁・E ノテーション・評価リミット・auto_completion
     * を対象とする (フォント・色・ホットキーは GUI 専用のため無視)。
     * 起動時とプリファレンス編集後に呼ぶ。 */
    void apply_settings_from_conf();
    void flash_message(std::string msg);

    /* About ダイアログ (F1 で開閉)。F1 / Esc / q / Enter で閉じる、
     * ↑↓ でショートカット一覧をスクロール。 */
    bool           about_handle_event(ftxui::Event ev);  /* true で吸収 */
    ftxui::Element about_overlay() const;

    /* マルチライン貼り付けモーダル (TuiSheet からコールバックで開かれる)。
     * ラジオ 3 択: 各行を別行 / 1 行に結合して挿入 / キャンセル。
     * ↑↓ で選択、Enter で確定、Esc でキャンセル。 */
    void           paste_modal_open(const std::string &raw_text);
    bool           paste_modal_handle_event(ftxui::Event ev);  /* true で吸収 */
    ftxui::Element paste_modal_overlay() const;
    void           paste_modal_confirm();

    /* 行右クリックコンテキストメニュー (TuiSheet の Mouse::Right から起動)。
     * 行レベル操作 (コピー/カット/ペースト/挿入/削除) をその場で発火。
     * ↑↓ で項目移動、Enter / 左クリックで実行、Esc / 外側クリックで閉じる。 */
    void           context_menu_open(int x, int y);
    void           context_menu_close();
    bool           context_menu_handle_event(ftxui::Event ev);  /* true で吸収 */
    ftxui::Element context_menu_overlay() const;
    void           context_menu_move(int dir);    /* +1 / -1 */
    void           context_menu_activate();        /* 現項目を実行 */

    /* メニューバー (Alt+F/E/V/R/H で展開)。
     * 展開中: ↑↓ で項目移動、←→ で隣メニューへ、Enter で実行、Esc で閉じる。 */
    bool           menu_handle_event(ftxui::Event ev);   /* true で吸収 */
    ftxui::Element menu_bar_render() const;              /* 最上段 */
    ftxui::Element menu_overlay() const;                 /* 展開中のドロップダウン */
    void           menu_open(MenuId id);
    void           menu_close();
    void           menu_move_item(int dir);              /* +1 / -1、区切り・無効をスキップ */
    void           menu_activate_current();              /* 現項目を実行 */
    void           menu_invoke_cmd(MenuCmd cmd);
    void           samples_populate();                   /* lazy init */
    std::string    samples_dir() const;

    sheet_model_t            *model_ = nullptr;
    std::shared_ptr<TuiSheet> sheet_;
    std::string               status_message_;
    ftxui::ScreenInteractive  screen_;

    /* プロンプト状態 */
    PromptMode   prompt_mode_   = PromptMode::None;
    std::string  prompt_label_;
    std::string  prompt_buf_;
    size_t       prompt_cursor_ = 0;

    /* About 状態 */
    bool about_visible_ = false;
    int  about_scroll_  = 0;

    /* マルチライン貼り付けモーダル状態。
     * paste_modal_choice_: 0=各行を別行 / 1=1 行に結合 / 2=キャンセル。 */
    bool        paste_modal_visible_ = false;
    int         paste_modal_choice_  = 0;
    std::string paste_modal_text_;
    mutable ftxui::Box paste_modal_box_;

    /* コンテキストメニュー状態。
     * context_menu_item_: 現在の選択項目 (0..items.size-1)、separator はスキップ。
     * 表示位置: 右クリック座標 (anchor) を起点に、画面右下にはみ出す場合だけ
     * 上 / 左方向に倒す。 */
    bool        context_menu_visible_ = false;
    int         context_menu_item_    = 0;
    int         context_menu_anchor_x_ = 0;
    int         context_menu_anchor_y_ = 0;
    mutable ftxui::Box              context_menu_box_;
    mutable std::vector<ftxui::Box> context_menu_item_boxes_;

    /* メニュー状態 */
    MenuId menu_active_     = MenuId::None;
    int    menu_item_       = 0;
    bool   submenu_active_  = false;  /* File/Samples が開いているとき true */
    int    submenu_item_    = 0;

    /* Samples の一覧 (lazy populate)。空なら未スキャン。 */
    mutable std::vector<std::string> samples_files_;
    mutable bool                     samples_scanned_ = false;

    /* マウス対応: 描画ごとに reflect で更新。 */
    bool handle_mouse(const ftxui::Mouse &m);  /* true で吸収 */
    mutable std::vector<ftxui::Box> menu_title_boxes_;
    mutable std::vector<ftxui::Box> menu_item_boxes_;
    mutable std::vector<ftxui::Box> submenu_item_boxes_;
    mutable ftxui::Box              about_box_;
    mutable ftxui::Box              prompt_box_;
};

} // namespace calcyx::tui

#endif
