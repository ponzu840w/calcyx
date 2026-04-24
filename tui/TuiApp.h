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
 *   - ファイルパスミニプロンプト (後続で実装)
 * を束ねる。ScreenInteractive の Loop() を提供する。 */
class TuiApp {
public:
    TuiApp();
    ~TuiApp();

    int run(const std::string &initial_file);

private:
    void do_file_save();
    void do_file_open();
    void flash_message(std::string msg);

    sheet_model_t            *model_ = nullptr;
    std::shared_ptr<TuiSheet> sheet_;
    std::string               status_message_;
    ftxui::ScreenInteractive  screen_;
};

} // namespace calcyx::tui

#endif
