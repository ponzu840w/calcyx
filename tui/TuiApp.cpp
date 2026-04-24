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
 * About ダイアログ
 * -------------------------------------------------------------------- */
#ifndef CALCYX_VERSION_FULL
#define CALCYX_VERSION_FULL "dev"
#endif
#ifndef CALCYX_EDITION
#define CALCYX_EDITION      "TUI Edition"
#endif

namespace {

/* ショートカット一覧。左列がキー、右列が説明。
 * GUI のメニュー (ui/MainWindow.cpp の menu_->add) を参考に TUI で実際に
 * バインドしているキーを列挙する。スクロール対応。 */
struct Shortcut { const char *key; const char *desc; };

const Shortcut kShortcuts[] = {
    { "Enter",            "Commit and insert row below" },
    { "Shift+Enter",      "Insert row above" },
    { "Ctrl+Del / Ctrl+BS","Delete current row" },
    { "Shift+Del",        "Delete row, move focus up" },
    { "BS on empty row",  "Delete row, move focus up" },
    { "Ctrl+Shift+Up/Down","Move current row" },
    { "Ctrl+Z / Ctrl+Y",  "Undo / Redo" },
    { "Tab / Ctrl+Space", "Trigger completion" },
    { "(while typing)",   "Auto-complete popup" },
    { "F5",               "Recalculate all" },
    { "F6 / Ctrl+:",      "Toggle compact mode" },
    { "F8-F12",           "Format: Auto / Dec / Hex / Bin / SI" },
    { "Alt+. / Alt+,",    "Decimal places +/-" },
    { "Alt+C",            "Copy all (OSC 52)" },
    { "Ctrl+Shift+Del",   "Clear all rows" },
    { "Ctrl+O / Ctrl+S",  "Open / Save file" },
    { "Ctrl+Q",           "Quit" },
    { "F1",               "This About dialog" },
};
constexpr int kShortcutCount = (int)(sizeof(kShortcuts) / sizeof(kShortcuts[0]));

} /* namespace */

Element TuiApp::about_overlay() const {
    using namespace ftxui;

    Elements header;
    header.push_back(text("calcyx " CALCYX_VERSION_FULL) | bold | center);
    header.push_back(text(CALCYX_EDITION) | dim | center);
    header.push_back(text(""));
    header.push_back(text("A programmable calculator based on Calctus") | center);
    header.push_back(text("https://github.com/ponzu840w/calcyx") |
                      color(Color::CyanLight) | center);
    header.push_back(text(""));
    header.push_back(text("Keyboard shortcuts") | bold);
    header.push_back(separator());

    /* ショートカット一覧をスクロール可能にするため、about_scroll_ の位置から
     * 最大 visible_rows 行だけ表示する。 */
    const int visible_rows = 10;
    int max_scroll = std::max(0, kShortcutCount - visible_rows);
    int scroll = std::clamp(about_scroll_, 0, max_scroll);

    Elements rows;
    for (int i = scroll; i < kShortcutCount && i < scroll + visible_rows; ++i) {
        rows.push_back(hbox({
            text(kShortcuts[i].key) | color(Color::YellowLight) |
                size(WIDTH, EQUAL, 22),
            text(" "),
            text(kShortcuts[i].desc),
        }));
    }
    /* スクロール可能なことを示すヒント */
    std::string hint = "↑↓: scroll  (";
    hint += std::to_string(scroll + 1);
    hint += "-";
    hint += std::to_string(std::min(scroll + visible_rows, kShortcutCount));
    hint += "/";
    hint += std::to_string(kShortcutCount);
    hint += ")   Esc / Enter / q: close";

    Elements body;
    for (auto &e : header) body.push_back(std::move(e));
    for (auto &e : rows)   body.push_back(std::move(e));
    body.push_back(separator());
    body.push_back(text(hint) | dim);

    return vbox(std::move(body)) | border | size(WIDTH, LESS_THAN, 70) |
           size(HEIGHT, LESS_THAN, 24) | center;
}

bool TuiApp::about_handle_event(Event ev) {
    if (!about_visible_) return false;
    if (ev == Event::Escape || ev == Event::Return || ev == Event::F1 ||
        ev == Event::Character("q") || ev == Event::Character("Q")) {
        about_visible_ = false;
        return true;
    }
    if (ev == Event::ArrowUp)   { if (about_scroll_ > 0) --about_scroll_; return true; }
    if (ev == Event::ArrowDown) { ++about_scroll_;                       return true; }
    if (ev == Event::PageUp)   { about_scroll_ = std::max(0, about_scroll_ - 5); return true; }
    if (ev == Event::PageDown) { about_scroll_ += 5;                     return true; }
    /* その他のキーは吸収のみ (シートに流さない) */
    return true;
}

/* ----------------------------------------------------------------------
 * テスト用: CatchEvent → sheet OnEvent の経路をテストから再現する
 * -------------------------------------------------------------------- */
void TuiApp::test_dispatch(Event ev) {
    if (about_handle_event(ev)) return;
    if (ev == Event::F1) { about_visible_ = true; about_scroll_ = 0; return; }
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

        Element base;
        /* プロンプト入力中は compact でも表示する (操作中は必須)。
         * プロンプトなしの status_message_ は compact 時は省略。 */
        if (prompt_mode_ != PromptMode::None) {
            const std::string &b = prompt_buf_;
            size_t p = std::min(prompt_cursor_, b.size());
            std::string a = b.substr(0, p);
            std::string m = (p < b.size()) ? std::string(1, b[p]) : std::string(" ");
            std::string c = (p < b.size()) ? b.substr(p + 1) : "";
            Element prompt_el = hbox({
                text(prompt_label_) | color(Color::Yellow),
                text(a),
                text(m) | inverted,
                text(c),
            });
            base = vbox({ body | flex, prompt_el });
        } else if (sheet_->compact_mode()) {
            base = vbox({ body | flex });
        } else {
            Element prompt_el =
                text(status_message_.empty() ? " " : status_message_) | dim;
            base = vbox({ body | flex, prompt_el });
        }

        if (about_visible_) {
            return dbox({ base, about_overlay() });
        }
        return base;
    });

    auto root = CatchEvent(renderer, [this](Event ev) {
        /* About が開いている間は全イベントを吸収する。 */
        if (about_handle_event(ev)) return true;
        /* 非表示のときに F1 で開く。 */
        if (ev == Event::F1) {
            about_visible_ = true;
            about_scroll_  = 0;
            return true;
        }
        return prompt_handle_event(ev);
    });

    screen_.Loop(root);
    return 0;
}

} // namespace calcyx::tui
