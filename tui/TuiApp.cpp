#include "TuiApp.h"

#include "TuiSheet.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <utility>

#if !defined(_WIN32)
#  include <unistd.h>
#endif

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#if defined(__APPLE__)
#  include <mach-o/dyld.h>
#endif

#if defined(_WIN32)
#  include <windows.h>
#endif

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
 * メニューバー
 * -------------------------------------------------------------------- */
namespace {

/* '&' 直後 1 文字をホット文字として返す。なければ '\0'。 */
char hot_letter_of(const char *label) {
    for (const char *p = label; *p; ++p) {
        if (*p == '&' && p[1]) return (char)std::tolower((unsigned char)p[1]);
    }
    return '\0';
}

/* '&' を除去した表示用ラベル長 (ASCII 前提、十分)。 */
int label_display_len(const char *label) {
    int n = 0;
    for (const char *p = label; *p; ++p) { if (*p != '&') ++n; }
    return n;
}

/* '&' を除去したラベル文字列 (ftxui::text 用)。
 * ホット文字は別色で描画するため、label_elements() を別途使う。 */
Element label_elements(const char *label, bool disabled) {
    Elements parts;
    bool next_hot = false;
    std::string buf;
    for (const char *p = label; *p; ++p) {
        if (*p == '&' && p[1]) { next_hot = true; continue; }
        if (next_hot) {
            if (!buf.empty()) {
                parts.push_back(text(buf));
                buf.clear();
            }
            Element hot = text(std::string(1, *p));
            parts.push_back(disabled ? hot | dim : hot | color(Color::YellowLight));
            next_hot = false;
        } else {
            buf.push_back(*p);
        }
    }
    if (!buf.empty()) parts.push_back(text(buf));
    return hbox(std::move(parts));
}

const MenuItem kFileMenu[] = {
    { "&Open...",         "Ctrl+O",        MenuCmd::Open,           false, false, false },
    { "&Save",            "Ctrl+S",        MenuCmd::Save,           false, false, false },
    { "S&amples",         "",              MenuCmd::SamplesSubmenu, false, false, true  },
    { "-",                "",              MenuCmd::None,           true,  false, false },
    { "&Clear All",       "Ctrl+Shift+Del",MenuCmd::ClearAll,       false, false, false },
    { "-",                "",              MenuCmd::None,           true,  false, false },
    { "&Preferences...",  "",              MenuCmd::Preferences,    false, true,  false },
    { "A&bout calcyx",    "F1",            MenuCmd::About,          false, false, false },
    { "-",                "",              MenuCmd::None,           true,  false, false },
    { "E&xit",            "Ctrl+Q",        MenuCmd::Exit,           false, false, false },
};

const MenuItem kEditMenu[] = {
    { "&Undo",              "Ctrl+Z",         MenuCmd::Undo,        false, false, false },
    { "&Redo",              "Ctrl+Y",         MenuCmd::Redo,        false, false, false },
    { "-",                  "",               MenuCmd::None,        true,  false, false },
    { "Copy &All",          "Alt+C",          MenuCmd::CopyAll,     false, false, false },
    { "-",                  "",               MenuCmd::None,        true,  false, false },
    { "&Insert Row Below",  "Enter",          MenuCmd::InsertBelow, false, false, false },
    { "Insert Row A&bove",  "Shift+Enter",    MenuCmd::InsertAbove, false, false, false },
    { "&Delete Row",        "Ctrl+Del",       MenuCmd::DeleteRow,   false, false, false },
    { "Move Row &Up",       "Ctrl+Shift+Up",  MenuCmd::MoveRowUp,   false, false, false },
    { "Move Row Do&wn",     "Ctrl+Shift+Down",MenuCmd::MoveRowDown, false, false, false },
    { "-",                  "",               MenuCmd::None,        true,  false, false },
    { "R&ecalculate",       "F5",             MenuCmd::Recalculate, false, false, false },
};

const MenuItem kViewMenu[] = {
    { "&Compact Mode",       "F6",    MenuCmd::ToggleCompact,      false, false, false },
    { "-",                   "",      MenuCmd::None,               true,  false, false },
    { "Decimals &+",         "Alt+.", MenuCmd::DecimalsInc,        false, false, false },
    { "Decimals &-",         "Alt+,", MenuCmd::DecimalsDec,        false, false, false },
    { "-",                   "",      MenuCmd::None,               true,  false, false },
    { "&Auto Completion",    "",      MenuCmd::ToggleAutoComplete, false, false, false },
};

const MenuItem kFormatMenu[] = {
    { "&Auto",           "F8",  MenuCmd::FormatAuto, false, false, false },
    { "&Decimal",        "F9",  MenuCmd::FormatDec,  false, false, false },
    { "&Hex",            "F10", MenuCmd::FormatHex,  false, false, false },
    { "&Binary",         "F11", MenuCmd::FormatBin,  false, false, false },
    { "&SI Prefix",      "F12", MenuCmd::FormatSI,   false, false, false },
};

const MenuItem kHelpMenu[] = {
    { "&About calcyx",   "F1",  MenuCmd::HelpAbout,  false, false, false },
};

struct MenuDef {
    const char     *title;
    MenuId          id;
    const MenuItem *items;
    int             count;
};

const MenuDef kMenus[] = {
    { "&File",   MenuId::File,   kFileMenu,   (int)(sizeof(kFileMenu)   / sizeof(MenuItem)) },
    { "&Edit",   MenuId::Edit,   kEditMenu,   (int)(sizeof(kEditMenu)   / sizeof(MenuItem)) },
    { "&View",   MenuId::View,   kViewMenu,   (int)(sizeof(kViewMenu)   / sizeof(MenuItem)) },
    { "fo&Rmat", MenuId::Format, kFormatMenu, (int)(sizeof(kFormatMenu) / sizeof(MenuItem)) },
    { "&Help",   MenuId::Help,   kHelpMenu,   (int)(sizeof(kHelpMenu)   / sizeof(MenuItem)) },
};
constexpr int kMenuCount = (int)(sizeof(kMenus) / sizeof(MenuDef));

int menu_index(MenuId id) {
    for (int i = 0; i < kMenuCount; ++i) if (kMenus[i].id == id) return i;
    return -1;
}

} /* namespace */

Element TuiApp::menu_bar_render() const {
    Elements cells;
    cells.push_back(text(" "));
    for (int i = 0; i < kMenuCount; ++i) {
        Element cell = hbox({
            text(" "),
            label_elements(kMenus[i].title, /*disabled=*/false),
            text(" "),
        });
        if (kMenus[i].id == menu_active_) cell = cell | inverted;
        cells.push_back(std::move(cell));
        cells.push_back(text(" "));
    }
    return hbox(std::move(cells));
}

Element TuiApp::menu_overlay() const {
    int idx = menu_index(menu_active_);
    if (idx < 0) return text("");
    const MenuDef &def = kMenus[idx];

    /* 左端位置 = menu_bar_render 上の該当メニューの開始列。 *
     * 先頭スペース 1 + 各メニューの [space title space] + 区切りスペース 1。 */
    int col = 1;
    for (int i = 0; i < idx; ++i) {
        col += 1 + label_display_len(kMenus[i].title) + 1 + 1;
    }

    /* ショートカット列幅 */
    int sc_max = 0;
    int label_max = 0;
    for (int i = 0; i < def.count; ++i) {
        if (def.items[i].separator) continue;
        int sl = label_display_len(def.items[i].label);
        if (def.items[i].submenu) sl += 2; /* " ▶" 分 */
        if (sl > label_max) label_max = sl;
        int sc = (int)std::strlen(def.items[i].shortcut);
        if (sc > sc_max) sc_max = sc;
    }
    int inner_width = std::max(12, label_max + 4 + sc_max);

    Elements rows;
    for (int i = 0; i < def.count; ++i) {
        const MenuItem &it = def.items[i];
        if (it.separator) {
            rows.push_back(separator());
            continue;
        }
        Element lab = label_elements(it.label, it.disabled);
        if (it.submenu) lab = hbox({ lab, text(" ▶") });
        Element sc = text(it.shortcut ? it.shortcut : "");
        Element row = hbox({
            text(" "),
            lab,
            filler(),
            sc | dim,
            text(" "),
        }) | size(WIDTH, EQUAL, inner_width + 2);
        if (it.disabled) row = row | dim;
        if (i == menu_item_ && !it.disabled) row = row | inverted;
        rows.push_back(std::move(row));
    }

    Element dd = vbox(std::move(rows)) | border;

    /* Samples submenu があれば右側に並べる。 */
    if (submenu_active_ && def.id == MenuId::File) {
        /* samples の数が多いと縦長になるので表示は最大 12 行でスクロール。 */
        const int visible = 12;
        int count = (int)samples_files_.size();
        int start = std::max(0, std::min(submenu_item_ - visible + 1,
                                          std::max(0, count - visible)));
        Elements srows;
        if (count == 0) {
            srows.push_back(text(" (no samples) ") | dim);
        } else {
            for (int i = start; i < count && i < start + visible; ++i) {
                std::string nm = samples_files_[i];
                if (nm.size() > 4 && nm.substr(nm.size() - 4) == ".txt")
                    nm = nm.substr(0, nm.size() - 4);
                Element row = hbox({ text(" "), text(nm), text(" ") }) |
                              size(WIDTH, GREATER_THAN, 18);
                if (i == submenu_item_) row = row | inverted;
                srows.push_back(std::move(row));
            }
        }
        Element sdd = vbox(std::move(srows)) | border;
        dd = hbox({ dd, sdd });
    }

    /* dropdown を col だけ右・1 行だけ下にオフセット。 */
    Element h = hbox({ filler() | size(WIDTH, EQUAL, col), dd, filler() });
    return vbox({ text(""), h, filler() });
}

void TuiApp::menu_open(MenuId id) {
    menu_active_    = id;
    menu_item_      = 0;
    submenu_active_ = false;
    submenu_item_   = 0;
    /* 最初の有効項目へ */
    menu_move_item(0);
}

void TuiApp::menu_close() {
    menu_active_    = MenuId::None;
    menu_item_      = 0;
    submenu_active_ = false;
    submenu_item_   = 0;
}

void TuiApp::menu_move_item(int dir) {
    int idx = menu_index(menu_active_);
    if (idx < 0) return;
    const MenuDef &def = kMenus[idx];

    if (submenu_active_) {
        int count = (int)samples_files_.size();
        if (count == 0) return;
        submenu_item_ = ((submenu_item_ + dir) % count + count) % count;
        return;
    }

    int i = menu_item_;
    if (dir == 0) {
        /* 現在位置が無効なら次有効へ */
        for (int k = 0; k < def.count; ++k) {
            int j = (i + k) % def.count;
            if (!def.items[j].separator && !def.items[j].disabled) {
                menu_item_ = j;
                return;
            }
        }
        menu_item_ = 0;
        return;
    }
    for (int k = 0; k < def.count; ++k) {
        i = ((i + dir) % def.count + def.count) % def.count;
        if (!def.items[i].separator && !def.items[i].disabled) {
            menu_item_ = i;
            return;
        }
    }
}

void TuiApp::menu_activate_current() {
    int idx = menu_index(menu_active_);
    if (idx < 0) return;
    const MenuDef &def = kMenus[idx];

    if (submenu_active_) {
        if (samples_files_.empty()) return;
        if (submenu_item_ < 0 ||
            submenu_item_ >= (int)samples_files_.size()) return;
        menu_invoke_cmd(MenuCmd::OpenSample);
        return;
    }

    if (menu_item_ < 0 || menu_item_ >= def.count) return;
    const MenuItem &it = def.items[menu_item_];
    if (it.separator || it.disabled) return;

    if (it.submenu && it.cmd == MenuCmd::SamplesSubmenu) {
        samples_populate();
        submenu_active_ = true;
        submenu_item_   = 0;
        return;
    }
    menu_invoke_cmd(it.cmd);
}

/* キーを Event に変換して sheet に流す小ヘルパ。 */
static inline bool sheet_dispatch_key(TuiSheet *s, Event ev) {
    return s->OnEvent(ev);
}

void TuiApp::menu_invoke_cmd(MenuCmd cmd) {
    /* メニュー系は実行前に閉じる (prompt を開くコマンドも対応するため)。 */
    MenuId was = menu_active_;
    bool   in_sub = submenu_active_;
    int    sub_idx = submenu_item_;
    menu_close();
    (void)was; (void)in_sub;

    switch (cmd) {
        case MenuCmd::None:        break;
        case MenuCmd::Open:        do_file_open();                                        break;
        case MenuCmd::Save:        do_file_save();                                        break;
        case MenuCmd::ClearAll:    sheet_dispatch_key(sheet_.get(), Event::Special("\x1b[3;6~")); break;
        case MenuCmd::Preferences: flash_message("Preferences: not available in TUI");    break;
        case MenuCmd::About:
        case MenuCmd::HelpAbout:
            about_visible_ = true; about_scroll_ = 0;                                     break;
        case MenuCmd::Exit:        screen_.Exit();                                         break;

        case MenuCmd::Undo:        sheet_dispatch_key(sheet_.get(), Event::Special("\x1a")); break;
        case MenuCmd::Redo:        sheet_dispatch_key(sheet_.get(), Event::Special("\x19")); break;
        case MenuCmd::CopyAll:     sheet_dispatch_key(sheet_.get(), Event::Special("\x1b""c")); break;
        case MenuCmd::InsertBelow: sheet_dispatch_key(sheet_.get(), Event::Return);          break;
        case MenuCmd::InsertAbove: sheet_dispatch_key(sheet_.get(), Event::Special("\x1b\r")); break;
        case MenuCmd::DeleteRow:   sheet_dispatch_key(sheet_.get(), Event::Special("\x1b[3;5~")); break;
        case MenuCmd::MoveRowUp:   sheet_dispatch_key(sheet_.get(), Event::Special("\x1b[1;6A")); break;
        case MenuCmd::MoveRowDown: sheet_dispatch_key(sheet_.get(), Event::Special("\x1b[1;6B")); break;
        case MenuCmd::Recalculate: sheet_dispatch_key(sheet_.get(), Event::F5);              break;

        case MenuCmd::ToggleCompact:
            sheet_->set_compact_mode(!sheet_->compact_mode());
            flash_message(sheet_->compact_mode() ? "Compact mode on" : "Compact mode off");
            break;
        case MenuCmd::DecimalsInc: sheet_dispatch_key(sheet_.get(), Event::Special("\x1b" ".")); break;
        case MenuCmd::DecimalsDec: sheet_dispatch_key(sheet_.get(), Event::Special("\x1b" ",")); break;
        case MenuCmd::ToggleAutoComplete:
            sheet_->set_auto_complete(!sheet_->auto_complete());
            flash_message(sheet_->auto_complete() ? "Auto completion on"
                                                  : "Auto completion off");
            break;

        case MenuCmd::FormatAuto: sheet_dispatch_key(sheet_.get(), Event::F8);  break;
        case MenuCmd::FormatDec:  sheet_dispatch_key(sheet_.get(), Event::F9);  break;
        case MenuCmd::FormatHex:  sheet_dispatch_key(sheet_.get(), Event::F10); break;
        case MenuCmd::FormatBin:  sheet_dispatch_key(sheet_.get(), Event::F11); break;
        case MenuCmd::FormatSI:   sheet_dispatch_key(sheet_.get(), Event::F12); break;

        case MenuCmd::SamplesSubmenu:
            /* 直接コマンドは未使用 (submenu 展開経由) */
            break;

        case MenuCmd::OpenSample: {
            if (sub_idx < 0 || sub_idx >= (int)samples_files_.size()) break;
            std::string dir = samples_dir();
            if (dir.empty()) { flash_message("samples directory not found"); break; }
            std::string path = dir + "/" + samples_files_[sub_idx];
            if (sheet_model_load_file(model_, path.c_str())) {
                sheet_->set_file_path(path);
                sheet_->reload_focused_row();
                flash_message("Loaded: " + path);
            } else {
                flash_message("Load failed: " + path);
            }
            break;
        }
    }
}

std::string TuiApp::samples_dir() const {
    /* calcyx 実行ファイルの場所から samples/ を探す。GUI の find_samples_dir
     * と同じ優先順位。 */
    char buf[1024] = {0};
    std::string exe_dir;
#if defined(__APPLE__)
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) == 0) {
        char *sep = std::strrchr(buf, '/');
        if (sep) { *sep = '\0'; exe_dir = buf; }
    }
#elif defined(_WIN32)
    wchar_t wbuf[1024];
    DWORD n = GetModuleFileNameW(nullptr, wbuf, 1024);
    if (n > 0 && n < 1024) {
        int len = WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, buf,
                                      sizeof(buf), nullptr, nullptr);
        if (len > 0) {
            char *sep = std::strrchr(buf, '\\');
            if (!sep) sep = std::strrchr(buf, '/');
            if (sep) { *sep = '\0'; exe_dir = buf; }
        }
    }
#else
    ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        char *sep = std::strrchr(buf, '/');
        if (sep) { *sep = '\0'; exe_dir = buf; }
    }
#endif
    if (exe_dir.empty()) return "";

    const char *suffixes[] = {
#if defined(__APPLE__)
        "/../Resources/samples",
#endif
        "/samples",
        "/../samples",
        "/../share/calcyx/samples",
    };
    struct stat st;
    for (const char *suf : suffixes) {
        std::string cand = exe_dir + suf;
        if (::stat(cand.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) return cand;
    }
    return "";
}

void TuiApp::samples_populate() {
    if (samples_scanned_) return;
    samples_scanned_ = true;
    samples_files_.clear();

    std::string dir = samples_dir();
    if (dir.empty()) return;
    DIR *dp = ::opendir(dir.c_str());
    if (!dp) return;
    struct dirent *ent;
    while ((ent = ::readdir(dp)) != nullptr) {
        std::string name = ent->d_name;
        if (name.size() > 4 && name.substr(name.size() - 4) == ".txt")
            samples_files_.push_back(name);
    }
    ::closedir(dp);
    std::sort(samples_files_.begin(), samples_files_.end());
}

bool TuiApp::menu_handle_event(Event ev) {
    /* ----- メニュー未展開時: Alt+X で展開 ----- */
    if (menu_active_ == MenuId::None) {
        if (ev == Event::Special("\x1b" "f") || ev == Event::Special("\x1b" "F")) { menu_open(MenuId::File);   return true; }
        if (ev == Event::Special("\x1b" "e") || ev == Event::Special("\x1b" "E")) { menu_open(MenuId::Edit);   return true; }
        if (ev == Event::Special("\x1b" "v") || ev == Event::Special("\x1b" "V")) { menu_open(MenuId::View);   return true; }
        if (ev == Event::Special("\x1b" "r") || ev == Event::Special("\x1b" "R")) { menu_open(MenuId::Format); return true; }
        if (ev == Event::Special("\x1b" "h") || ev == Event::Special("\x1b" "H")) { menu_open(MenuId::Help);   return true; }
        return false;
    }

    /* ----- メニュー展開中 ----- */
    if (ev == Event::Escape) {
        if (submenu_active_) { submenu_active_ = false; submenu_item_ = 0; return true; }
        menu_close(); return true;
    }
    if (ev == Event::ArrowUp)   { menu_move_item(-1); return true; }
    if (ev == Event::ArrowDown) { menu_move_item(+1); return true; }
    if (ev == Event::ArrowRight) {
        /* submenu に入るべき状況なら入る */
        if (!submenu_active_) {
            int idx = menu_index(menu_active_);
            if (idx >= 0 && menu_item_ >= 0 && menu_item_ < kMenus[idx].count) {
                const MenuItem &it = kMenus[idx].items[menu_item_];
                if (it.submenu && !it.disabled) {
                    samples_populate();
                    submenu_active_ = true;
                    submenu_item_   = 0;
                    return true;
                }
            }
            /* そうでなければ隣メニューへ */
            int cur = menu_index(menu_active_);
            if (cur >= 0) menu_open(kMenus[(cur + 1) % kMenuCount].id);
            return true;
        }
        return true;  /* submenu 展開中の右は無視 */
    }
    if (ev == Event::ArrowLeft) {
        if (submenu_active_) { submenu_active_ = false; submenu_item_ = 0; return true; }
        int cur = menu_index(menu_active_);
        if (cur >= 0) menu_open(kMenus[(cur - 1 + kMenuCount) % kMenuCount].id);
        return true;
    }
    if (ev == Event::Return) { menu_activate_current(); return true; }

    /* ホット文字での項目選択 */
    if (ev.is_character() && !submenu_active_) {
        const std::string &s = ev.character();
        if (s.size() == 1) {
            char c = (char)std::tolower((unsigned char)s[0]);
            int idx = menu_index(menu_active_);
            if (idx >= 0) {
                const MenuDef &def = kMenus[idx];
                for (int i = 0; i < def.count; ++i) {
                    if (def.items[i].separator || def.items[i].disabled) continue;
                    if (hot_letter_of(def.items[i].label) == c) {
                        menu_item_ = i;
                        menu_activate_current();
                        return true;
                    }
                }
            }
        }
    }

    /* その他は吸収 */
    return true;
}

/* ----------------------------------------------------------------------
 * テスト用: CatchEvent → sheet OnEvent の経路をテストから再現する
 * -------------------------------------------------------------------- */
void TuiApp::test_dispatch(Event ev) {
    if (about_handle_event(ev)) return;
    if (ev == Event::F1) { about_visible_ = true; about_scroll_ = 0; return; }
    if (menu_handle_event(ev)) return;
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

        /* compact のときはメニューバーも隠す (F6 / Ctrl+: で復帰)。 */
        if (!sheet_->compact_mode()) {
            base = vbox({ menu_bar_render(), base | flex });
        }

        Elements layers;
        layers.push_back(base);
        if (menu_active_ != MenuId::None) layers.push_back(menu_overlay());
        if (about_visible_)               layers.push_back(about_overlay());
        if (layers.size() == 1) return base;
        return dbox(std::move(layers));
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
        if (menu_handle_event(ev)) return true;
        return prompt_handle_event(ev);
    });

    screen_.Loop(root);
    return 0;
}

} // namespace calcyx::tui
