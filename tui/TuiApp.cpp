#include "TuiApp.h"

#include "TuiSheet.h"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

using namespace ftxui;

namespace calcyx::tui {

TuiApp::TuiApp()
    : screen_(ScreenInteractive::Fullscreen()) {
    model_ = sheet_model_new();
    /* 空の 1 行を入れておく (sheet_model は新規時に空 rows を返す) */
    if (sheet_model_row_count(model_) == 0) {
        const char *empty[] = { "" };
        sheet_model_set_rows(model_, empty, 1);
    }
    sheet_ = MakeTuiSheet(model_);

    sheet_->set_quit_callback([this]() { screen_.Exit(); });
    sheet_->set_file_save_callback([this]() { do_file_save(); });
    sheet_->set_file_open_callback([this]() { do_file_open(); });
}

TuiApp::~TuiApp() {
    if (model_) sheet_model_free(model_);
}

void TuiApp::flash_message(std::string msg) {
    status_message_ = std::move(msg);
}

void TuiApp::do_file_save() {
    const std::string &path = sheet_->file_path();
    if (path.empty()) {
        flash_message("Save-As not yet implemented (open a file first with -f)");
        return;
    }
    if (sheet_model_save_file(model_, path.c_str())) {
        flash_message("Saved: " + path);
    } else {
        flash_message("Save failed: " + path);
    }
}

void TuiApp::do_file_open() {
    /* v1: mini-prompt 未実装。-f <path> 起動時のみ I/O 可能 */
    flash_message("File open dialog not yet implemented; start with -f <path>");
}

int TuiApp::run(const std::string &initial_file) {
    if (!initial_file.empty()) {
        if (sheet_model_load_file(model_, initial_file.c_str())) {
            sheet_->set_file_path(initial_file);
            sheet_->reload_focused_row();
            flash_message("Loaded: " + initial_file);
        } else {
            /* 新規ファイルとして扱う */
            sheet_->set_file_path(initial_file);
            flash_message("New file: " + initial_file);
        }
    }

    /* TuiSheet を Renderer でラップしてステータスバーを合成 */
    auto renderer = Renderer(sheet_, [this] {
        auto body = sheet_->Render();
        Element status = text(status_message_.empty() ? " " : status_message_)
                          | dim;
        return vbox({ body | flex, status });
    });

    screen_.Loop(renderer);
    return 0;
}

} // namespace calcyx::tui
