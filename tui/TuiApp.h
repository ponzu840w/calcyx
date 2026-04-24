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

    sheet_model_t            *model_ = nullptr;
    std::shared_ptr<TuiSheet> sheet_;
    std::string               status_message_;
    ftxui::ScreenInteractive  screen_;

    /* プロンプト状態 */
    PromptMode   prompt_mode_   = PromptMode::None;
    std::string  prompt_label_;
    std::string  prompt_buf_;
    size_t       prompt_cursor_ = 0;
};

} // namespace calcyx::tui

#endif
