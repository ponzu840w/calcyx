#ifndef CALCYX_TUI_APP_H
#define CALCYX_TUI_APP_H

#include <ftxui/component/component_base.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include <memory>
#include <string>

#include "sheet_model.h"

namespace calcyx::tui {

class TuiSheet;

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
    void flash_message(std::string msg);

    /* About ダイアログ (F1 で開閉)。F1 / Esc / q / Enter で閉じる、
     * ↑↓ でショートカット一覧をスクロール。 */
    bool           about_handle_event(ftxui::Event ev);  /* true で吸収 */
    ftxui::Element about_overlay() const;

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
};

} // namespace calcyx::tui

#endif
