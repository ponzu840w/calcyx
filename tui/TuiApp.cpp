#include "TuiApp.h"

#include "TuiSheet.h"

#include <utility>

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

using namespace ftxui;

namespace calcyx::tui {

TuiApp::TuiApp()
    : screen_(ScreenInteractive::Fullscreen()) {
    model_ = sheet_model_new();
    if (sheet_model_row_count(model_) == 0) {
        const char *empty[] = { "" };
        sheet_model_set_rows(model_, empty, 1);
    }
    sheet_ = MakeTuiSheet(model_);

    sheet_->set_quit_callback([this]() { screen_.Exit(); });
    sheet_->set_file_save_callback([this]() { do_file_save(); });
    sheet_->set_file_open_callback([this]() { do_file_open(); });
    sheet_->set_status_callback([this](std::string m) { flash_message(std::move(m)); });
}

TuiApp::~TuiApp() {
    if (model_) sheet_model_free(model_);
}

/* ----------------------------------------------------------------------
 * ステータスメッセージ
 * -------------------------------------------------------------------- */
void TuiApp::flash_message(std::string msg) {
    status_message_ = std::move(msg);
}

/* ----------------------------------------------------------------------
 * プロンプト
 * -------------------------------------------------------------------- */
void TuiApp::prompt_begin(PromptMode mode, const std::string &initial) {
    prompt_mode_   = mode;
    prompt_buf_    = initial;
    prompt_cursor_ = prompt_buf_.size();
    switch (mode) {
        case PromptMode::Open: prompt_label_ = "Open file: "; break;
        case PromptMode::Save: prompt_label_ = "Save as:   "; break;
        default:               prompt_label_.clear();         break;
    }
}

void TuiApp::prompt_cancel() {
    prompt_mode_ = PromptMode::None;
    prompt_buf_.clear();
    prompt_cursor_ = 0;
    prompt_label_.clear();
    flash_message("Cancelled");
}

void TuiApp::prompt_submit() {
    if (prompt_mode_ == PromptMode::None) return;

    std::string path = prompt_buf_;
    PromptMode mode = prompt_mode_;
    prompt_mode_ = PromptMode::None;
    prompt_buf_.clear();
    prompt_cursor_ = 0;
    prompt_label_.clear();

    if (path.empty()) {
        flash_message("Path is empty");
        return;
    }

    if (mode == PromptMode::Save) {
        if (sheet_model_save_file(model_, path.c_str())) {
            sheet_->set_file_path(path);
            flash_message("Saved: " + path);
        } else {
            flash_message("Save failed: " + path);
        }
    } else { /* Open */
        if (sheet_model_load_file(model_, path.c_str())) {
            sheet_->set_file_path(path);
            sheet_->reload_focused_row();
            flash_message("Loaded: " + path);
        } else {
            flash_message("Load failed: " + path);
        }
    }
}

bool TuiApp::prompt_handle_event(Event ev) {
    if (prompt_mode_ == PromptMode::None) return false;

    if (ev == Event::Escape) { prompt_cancel(); return true; }
    if (ev == Event::Return) { prompt_submit(); return true; }

    if (ev == Event::Backspace) {
        if (prompt_cursor_ > 0) {
            prompt_buf_.erase(prompt_cursor_ - 1, 1);
            --prompt_cursor_;
        }
        return true;
    }
    if (ev == Event::Delete) {
        if (prompt_cursor_ < prompt_buf_.size())
            prompt_buf_.erase(prompt_cursor_, 1);
        return true;
    }
    if (ev == Event::ArrowLeft) {
        if (prompt_cursor_ > 0) --prompt_cursor_;
        return true;
    }
    if (ev == Event::ArrowRight) {
        if (prompt_cursor_ < prompt_buf_.size()) ++prompt_cursor_;
        return true;
    }
    if (ev == Event::Home || ev == Event::Special("\x01")) {
        prompt_cursor_ = 0;
        return true;
    }
    if (ev == Event::End || ev == Event::Special("\x05")) {
        prompt_cursor_ = prompt_buf_.size();
        return true;
    }
    if (ev == Event::Special("\x15")) {  /* Ctrl+U: clear */
        prompt_buf_.clear();
        prompt_cursor_ = 0;
        return true;
    }
    if (ev.is_character()) {
        prompt_buf_.insert(prompt_cursor_, ev.character());
        prompt_cursor_ += ev.character().size();
        return true;
    }
    /* 他のキー (Ctrl+Q など) は吸収しない */
    return false;
}

/* ----------------------------------------------------------------------
 * File I/O エントリ
 * -------------------------------------------------------------------- */
void TuiApp::do_file_save() {
    const std::string &path = sheet_->file_path();
    if (path.empty()) {
        prompt_begin(PromptMode::Save, "");
        return;
    }
    if (sheet_model_save_file(model_, path.c_str())) {
        flash_message("Saved: " + path);
    } else {
        flash_message("Save failed: " + path);
    }
}

void TuiApp::do_file_open() {
    prompt_begin(PromptMode::Open, sheet_->file_path());
}

/* ----------------------------------------------------------------------
 * テスト用: CatchEvent → sheet OnEvent の経路をテストから再現する
 * -------------------------------------------------------------------- */
void TuiApp::test_dispatch(Event ev) {
    if (prompt_handle_event(ev)) return;
    sheet_->OnEvent(ev);
}

/* ----------------------------------------------------------------------
 * メインループ
 * -------------------------------------------------------------------- */
int TuiApp::run(const std::string &initial_file) {
    if (!initial_file.empty()) {
        if (sheet_model_load_file(model_, initial_file.c_str())) {
            sheet_->set_file_path(initial_file);
            sheet_->reload_focused_row();
            flash_message("Loaded: " + initial_file);
        } else {
            sheet_->set_file_path(initial_file);
            flash_message("New file: " + initial_file);
        }
    }

    /* プロンプト入力中はシートへの入力を横取りする。 */
    auto renderer = Renderer(sheet_, [this] {
        Element body = sheet_->Render();

        Element prompt_el;
        if (prompt_mode_ != PromptMode::None) {
            /* カーソル表示 (反転) */
            const std::string &b = prompt_buf_;
            size_t p = std::min(prompt_cursor_, b.size());
            std::string a = b.substr(0, p);
            std::string m = (p < b.size()) ? std::string(1, b[p]) : std::string(" ");
            std::string c = (p < b.size()) ? b.substr(p + 1) : "";
            prompt_el = hbox({
                text(prompt_label_) | color(Color::Yellow),
                text(a),
                text(m) | inverted,
                text(c),
            });
        } else {
            prompt_el = text(status_message_.empty() ? " " : status_message_) | dim;
        }

        return vbox({ body | flex, prompt_el });
    });

    auto root = CatchEvent(renderer, [this](Event ev) {
        return prompt_handle_event(ev);
    });

    screen_.Loop(root);
    return 0;
}

} // namespace calcyx::tui
