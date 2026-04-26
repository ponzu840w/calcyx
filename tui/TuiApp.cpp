#include "TuiApp.h"

#include "TuiSheet.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <map>
#include <sys/stat.h>
#include <sys/types.h>
#include <utility>

#include "types/val.h"
#include "settings_schema.h"
#include "color_presets.h"
#include "settings_io.h"

#if defined(_WIN32)
#  include <direct.h>  /* _mkdir */
#endif

#if !defined(_WIN32)
#  include <termios.h>
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
    sheet_->set_multiline_paste_callback(
        [this](std::string raw) { paste_modal_open(std::move(raw)); });
    sheet_->set_context_menu_callback(
        [this](int x, int y) { context_menu_open(x, y); });

    /* GUI と共有の calcyx.conf からエンジン関連設定を取り込む。
     * 失敗しても黙って既定値で続行する。 */
    apply_settings_from_conf();
}

TuiApp::~TuiApp() {
    if (model_) sheet_model_free(model_);
}

/* ----------------------------------------------------------------------
 * ステータスメッセージ
 * -------------------------------------------------------------------- */
void TuiApp::flash_message(std::string msg) {
    status_message_ = std::move(msg);
    /* このフレームでは表示する。次イベントの先頭で消える。 */
    flash_pending_clear_ = false;
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
            /* 明示的に書き込んだ瞬間にユーザーの作業ファイルへ昇格する。
             * 以後の Ctrl+S は同じパスへ直書きでよい。 */
            sheet_->set_read_only(false);
            flash_message("Saved: " + path);
        } else {
            flash_message("Save failed: " + path);
        }
    } else { /* Open */
        if (sheet_model_load_file(model_, path.c_str())) {
            sheet_->set_file_path(path);
            /* Ctrl+O で開いたファイルは編集対象。read_only は解除。 */
            sheet_->set_read_only(false);
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
    /* read_only_ なファイル (サンプル等) は file_path_ への直書きを禁止し、
     * 必ず Save-As プロンプトに落とす (配布版サンプルの誤上書き防止)。
     * 既定値も空にして、ユーザーがゼロから書き先を入力するよう促す。 */
    if (path.empty() || sheet_->read_only()) {
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
 * Preferences (calcyx.conf を外部エディタで開く)
 * -------------------------------------------------------------------- */
namespace {
/* GUI の AppPrefs::config_dir / settings_globals.cpp::ensure_path と同じ
 * パス決定ロジックを TUI 側で再実装。FLTK 依存を持ち込まないため、
 * ヘッダ共通化は今回見送る。 */
std::string preferences_config_dir() {
    char buf[1024];
#if defined(_WIN32)
    /* %APPDATA%\calcyx — UTF-8 化は GUI 側ほど厳密にやらない (TUI なら
     * パスに非 ASCII が混じってもエディタが解釈する想定)。 */
    const char *appdata = std::getenv("APPDATA");
    if (!appdata || !*appdata) appdata = ".";
    std::snprintf(buf, sizeof(buf), "%s\\calcyx", appdata);
    _mkdir(buf);
#elif defined(__APPLE__)
    const char *home = std::getenv("HOME");
    if (!home) home = ".";
    std::snprintf(buf, sizeof(buf), "%s/Library/Application Support/calcyx", home);
    mkdir(buf, 0755);
#else
    const char *xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0]) {
        std::snprintf(buf, sizeof(buf), "%s/calcyx", xdg);
    } else {
        const char *home = std::getenv("HOME");
        if (!home) home = ".";
        std::snprintf(buf, sizeof(buf), "%s/.config/calcyx", home);
    }
    mkdir(buf, 0755);
#endif
    return buf;
}

std::string preferences_conf_path() {
    std::string dir = preferences_config_dir();
#if defined(_WIN32)
    return dir + "\\calcyx.conf";
#else
    return dir + "/calcyx.conf";
#endif
}

/* ファイルが無ければ空ファイルを作る (エディタが新規作成扱いになるが、
 * 既に存在するなら何もしない)。 */
void preferences_touch(const std::string &path) {
    FILE *fp = std::fopen(path.c_str(), "r");
    if (fp) { std::fclose(fp); return; }
    fp = std::fopen(path.c_str(), "a");
    if (fp) std::fclose(fp);
}

/* calcyx.conf の最小パーサ。GUI 側 (settings_globals.cpp::read_conf) の
 * 仕様に合わせる: '#' で始まる行と空行はスキップ、key=value の
 * 前後空白を除去して std::map に積む。FLTK 依存を引き込まないために
 * 自前実装にしているが、フォーマットはバイト互換。 */
std::map<std::string, std::string> conf_read(const std::string &path) {
    std::map<std::string, std::string> kv;
    FILE *fp = std::fopen(path.c_str(), "r");
    if (!fp) return kv;
    char line[512];
    while (std::fgets(line, sizeof(line), fp)) {
        size_t len = std::strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';
        if (line[0] == '#' || line[0] == '\0') continue;
        char *eq = std::strchr(line, '=');
        if (!eq) continue;
        char *ke = eq - 1;
        while (ke >= line && (*ke == ' ' || *ke == '\t')) --ke;
        std::string key(line, ke - line + 1);
        const char *vs = eq + 1;
        while (*vs == ' ' || *vs == '\t') ++vs;
        kv[key] = vs;
    }
    std::fclose(fp);
    return kv;
}

bool conf_get_bool(const std::map<std::string, std::string> &kv,
                   const char *key, bool def) {
    auto it = kv.find(key);
    if (it == kv.end()) return def;
    const std::string &v = it->second;
    return v == "true" || v == "1" || v == "yes";
}

int conf_get_int(const std::map<std::string, std::string> &kv,
                 const char *key, int def) {
    auto it = kv.find(key);
    return (it != kv.end()) ? std::atoi(it->second.c_str()) : def;
}

#if !defined(_WIN32)
/* shell に渡す引数を single-quote で安全に包む。中身の ' は '\'' に置換。
 * 基本的にユーザーの calcyx 設定ディレクトリ配下のパスなので心配は薄いが、
 * ホームに記号が混じった環境のために最低限のサニタイズを入れる。 */
std::string preferences_shell_quote(const std::string &s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else           out += c;
    }
    out += "'";
    return out;
}
#endif
} /* namespace */

void TuiApp::do_preferences() {
    std::string path = preferences_conf_path();
    preferences_touch(path);

#if defined(_WIN32)
    /* 関連付けされたエディタ (テキスト) で開く。ShellExecute は非同期だが
     * TUI からの "編集中ロック" は必須ではないので、起動だけして戻る。 */
    ShellExecuteA(nullptr, "open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    flash_message("Opened: " + path);
#else
    /* $VISUAL → $EDITOR → vi の順で探す。FTXUI の端末モードは
     * WithRestoredIO 経由で一時退避し、エディタが終了したら自動復帰。 */
    const char *editor = std::getenv("VISUAL");
    if (!editor || !*editor) editor = std::getenv("EDITOR");
    if (!editor || !*editor) editor = "vi";

    std::string cmd = std::string(editor) + " " + preferences_shell_quote(path);
    /* WithRestoredIO は Closure を 1 個受け取って、それを「端末復帰 → 実行
     * → 再インストール」でラップした Closure を返す。返り値を即時呼び出す。 */
    screen_.WithRestoredIO([cmd] {
        int rc = std::system(cmd.c_str());
        (void)rc;
    })();
    flash_message("Edited: " + path);
#endif
    /* エディタ終了直後に再読み込みして即時反映 (GUI 側との一貫性)。 */
    apply_settings_from_conf();
}

/* GUI と共有する設定キーだけ取り込む。フォント・色・ホットキーなど GUI 専用の
 * キーはスキーマの scope フラグで自動的にフィルタされる (calcyx.conf 自体は
 * 両方が書き込みうる共有ファイル)。
 *
 * スキーマ駆動: shared/settings_schema.c の TABLE を 1 回走査して
 * scope に CALCYX_SETTING_SCOPE_TUI が立っているキーのみ処理する。
 * 各キーへの実際のバインドは下の dispatch 部 (strcmp 連鎖) で書く。
 * スキーマには出るが TUI が扱わない BOTH キーは下の連鎖でカバー漏れ
 * として認識できる (= スキーマ追加時に TUI 側でも反映を判断する強制点)。 */
void TuiApp::apply_settings_from_conf() {
    auto kv = conf_read(preferences_conf_path());
    if (kv.empty()) return;

    /* TUI 側で受け取った値は最後にまとめて反映 (clamp はキーごとに) */
    int  decimal_len    = g_fmt_settings.decimal_len;
    bool e_notation     = g_fmt_settings.e_notation;
    int  e_positive_min = g_fmt_settings.e_positive_min;
    int  e_negative_max = g_fmt_settings.e_negative_max;
    bool e_alignment    = g_fmt_settings.e_alignment;

    sheet_eval_limits_t lim;
    lim.max_array_length  = 256;
    lim.max_string_length = 256;
    lim.max_call_depth    = 64;

    bool auto_completion     = true;
    bool bs_delete_empty_row = true;

    int n = 0;
    const calcyx_setting_desc_t *table = calcyx_settings_table(&n);
    for (int i = 0; i < n; i++) {
        const calcyx_setting_desc_t &d = table[i];
        if (!(d.scope & CALCYX_SETTING_SCOPE_TUI)) continue;
        if (!d.key) continue;

        /* キー → TUI 側変数へ振り分け. スキーマに新しい BOTH キーが
         * 追加された際は ここに分岐を足すか、明示的に「無視」コメントで
         * 残す方針。 */
        if (strcmp(d.key, "decimal_digits") == 0) {
            decimal_len = std::clamp(conf_get_int(kv, d.key, decimal_len),
                                     d.i_lo, d.i_hi);
        } else if (strcmp(d.key, "e_notation") == 0) {
            e_notation = conf_get_bool(kv, d.key, e_notation);
        } else if (strcmp(d.key, "e_positive_min") == 0) {
            e_positive_min = std::clamp(conf_get_int(kv, d.key, e_positive_min),
                                        d.i_lo, d.i_hi);
        } else if (strcmp(d.key, "e_negative_max") == 0) {
            e_negative_max = std::clamp(conf_get_int(kv, d.key, e_negative_max),
                                        d.i_lo, d.i_hi);
        } else if (strcmp(d.key, "e_alignment") == 0) {
            e_alignment = conf_get_bool(kv, d.key, e_alignment);
        } else if (strcmp(d.key, "max_array_length") == 0) {
            lim.max_array_length = std::clamp(
                conf_get_int(kv, d.key, lim.max_array_length), d.i_lo, d.i_hi);
        } else if (strcmp(d.key, "max_string_length") == 0) {
            lim.max_string_length = std::clamp(
                conf_get_int(kv, d.key, lim.max_string_length), d.i_lo, d.i_hi);
        } else if (strcmp(d.key, "max_call_depth") == 0) {
            lim.max_call_depth = std::clamp(
                conf_get_int(kv, d.key, lim.max_call_depth), d.i_lo, d.i_hi);
        } else if (strcmp(d.key, "auto_completion") == 0) {
            auto_completion = conf_get_bool(kv, d.key, auto_completion);
        } else if (strcmp(d.key, "bs_delete_empty_row") == 0) {
            bs_delete_empty_row = conf_get_bool(kv, d.key, bs_delete_empty_row);
        }
        /* color_preset / color_* は tui_color_source=mirror_gui のときだけ
         * 下のパレット構築フェーズで読む。 */
    }

    g_fmt_settings.decimal_len    = decimal_len;
    g_fmt_settings.e_notation     = e_notation;
    g_fmt_settings.e_positive_min = e_positive_min;
    g_fmt_settings.e_negative_max = e_negative_max;
    g_fmt_settings.e_alignment    = e_alignment;
    sheet_model_set_limits(model_, lim);

    if (sheet_) {
        sheet_->set_auto_complete(auto_completion);
        sheet_->set_bs_delete_empty_row(bs_delete_empty_row);

        /* tui_color_source: semantic (default) | mirror_gui
         * mirror_gui のとき color_preset を読み, user-defined なら
         * 個別 color_* キーで上書きしてから TuiPalette を sheet に渡す. */
        TuiPalette tp;
        auto src_it = kv.find("tui_color_source");
        std::string src = (src_it != kv.end()) ? src_it->second : "semantic";
        if (src == "mirror_gui") {
            std::string preset_id = "otaku-black";
            auto pit = kv.find("color_preset");
            if (pit != kv.end()) preset_id = pit->second;
            int pid = calcyx_color_preset_lookup(preset_id.c_str());
            if (pid < 0) pid = CALCYX_COLOR_PRESET_OTAKU_BLACK;
            calcyx_color_palette_t pal;
            calcyx_color_preset_get(pid, &pal);

            /* user-defined なら conf の color_* で個別上書き. */
            if (pid == CALCYX_COLOR_PRESET_USER_DEFINED) {
                struct { const char *key; calcyx_rgb_t *dst; } overrides[] = {
                    { "color_bg",       &pal.bg },
                    { "color_sel_bg",   &pal.sel_bg },
                    { "color_text",     &pal.text },
                    { "color_cursor",   &pal.cursor },
                    { "color_symbol",   &pal.symbol },
                    { "color_ident",    &pal.ident },
                    { "color_special",  &pal.special },
                    { "color_si_pfx",   &pal.si_pfx },
                    { "color_error",    &pal.error },
                    { "color_paren0",   &pal.paren[0] },
                    { "color_paren1",   &pal.paren[1] },
                    { "color_paren2",   &pal.paren[2] },
                    { "color_paren3",   &pal.paren[3] },
                };
                for (const auto &o : overrides) {
                    auto it = kv.find(o.key);
                    if (it == kv.end()) continue;
                    unsigned char rgb[3];
                    if (calcyx_conf_parse_hex_color(it->second.c_str(), rgb)) {
                        o.dst->r = rgb[0]; o.dst->g = rgb[1]; o.dst->b = rgb[2];
                    }
                }
            }

            tp.active  = true;
            tp.bg      = pal.bg;
            tp.sel_bg  = pal.sel_bg;
            tp.text    = pal.text;
            tp.cursor  = pal.cursor;
            tp.symbol  = pal.symbol;
            tp.ident   = pal.ident;
            tp.special = pal.special;
            tp.si_pfx  = pal.si_pfx;
            tp.error   = pal.error;
            for (int i = 0; i < 4; ++i) tp.paren[i] = pal.paren[i];
        }
        sheet_->set_palette(tp);
    }

    /* 桁数・E ノテーションが変わると既存行の表示も変わるので再評価。 */
    sheet_model_eval_all(model_);
    if (sheet_) sheet_->reload_focused_row();
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
    { "F6",               "Toggle compact mode" },
    { "F8-F12",           "Format: Auto / Dec / Hex / Bin / SI" },
    { "Alt+. / Alt+,",    "Decimal places +/-" },
    { "Alt+C",            "Copy all (OSC 52)" },
    { "Ctrl+Shift+Del",   "Clear all rows" },
    { "Ctrl+O / Ctrl+S",  "Open / Save file" },
    { "Ctrl+Q",           "Quit" },
    { "F1",               "This About dialog" },
};
constexpr int kShortcutCount     = (int)(sizeof(kShortcuts) / sizeof(kShortcuts[0]));
constexpr int kAboutVisibleRows  = 10;
constexpr int kAboutMaxScroll    =
    (kShortcutCount > kAboutVisibleRows) ? kShortcutCount - kAboutVisibleRows : 0;

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
     * 最大 kAboutVisibleRows 行だけ表示する。 */
    int scroll = std::clamp(about_scroll_, 0, kAboutMaxScroll);

    Elements rows;
    for (int i = scroll; i < kShortcutCount && i < scroll + kAboutVisibleRows; ++i) {
        rows.push_back(hbox({
            text(kShortcuts[i].key) | bold | size(WIDTH, EQUAL, 22),
            text(" "),
            text(kShortcuts[i].desc),
        }));
    }
    /* スクロール可能なことを示すヒント */
    std::string hint = "↑↓: scroll  (";
    hint += std::to_string(scroll + 1);
    hint += "-";
    hint += std::to_string(std::min(scroll + kAboutVisibleRows, kShortcutCount));
    hint += "/";
    hint += std::to_string(kShortcutCount);
    hint += ")   Esc / Enter / q: close";

    Elements body;
    for (auto &e : header) body.push_back(std::move(e));
    for (auto &e : rows)   body.push_back(std::move(e));
    body.push_back(separator());
    body.push_back(text(hint) | dim);

    /* clear_under: ダイアログの背面セルを完全に塗り潰し、下層のシート内容や
     * カーソルハイライト (inverted) が透けて見えないようにする。 */
    return vbox(std::move(body)) | border | size(WIDTH, LESS_THAN, 70) |
           size(HEIGHT, LESS_THAN, 24) | clear_under | reflect(about_box_) | center;
}

bool TuiApp::about_handle_event(Event ev) {
    if (!about_visible_) return false;
    if (ev == Event::Escape || ev == Event::Return || ev == Event::F1 ||
        ev == Event::Character("q") || ev == Event::Character("Q")) {
        about_visible_ = false;
        return true;
    }
    if (ev == Event::ArrowUp)   { about_scroll_ = std::max(0, about_scroll_ - 1); return true; }
    if (ev == Event::ArrowDown) { about_scroll_ = std::min(kAboutMaxScroll, about_scroll_ + 1); return true; }
    if (ev == Event::PageUp)   { about_scroll_ = std::max(0, about_scroll_ - 5); return true; }
    if (ev == Event::PageDown) { about_scroll_ = std::min(kAboutMaxScroll, about_scroll_ + 5); return true; }
    /* その他のキーは吸収のみ (シートに流さない) */
    return true;
}

/* ----------------------------------------------------------------------
 * Paste Options モーダル (改行入りクリップボード貼り付け時)
 *
 * GUI の PasteOptionForm を簡素化したラジオ 3 択。テキストプレビューと
 * 選択肢を表示し、↑↓ で選択、Enter で確定、Esc でキャンセル。
 * -------------------------------------------------------------------- */

namespace {
constexpr int kPasteChoiceMultiRows  = 0;
constexpr int kPasteChoiceSingleLine = 1;
constexpr int kPasteChoiceCancel     = 2;
constexpr int kPasteChoiceCount      = 3;
constexpr int kPastePreviewMaxLines  = 8;

int count_lines(const std::string &s) {
    int n = 1;
    for (char c : s) if (c == '\n') ++n;
    return n;
}
} // namespace

void TuiApp::paste_modal_open(const std::string &raw_text) {
    paste_modal_visible_ = true;
    paste_modal_choice_  = kPasteChoiceMultiRows;
    paste_modal_text_    = raw_text;
}

bool TuiApp::paste_modal_handle_event(Event ev) {
    if (!paste_modal_visible_) return false;

    if (ev == Event::Escape) {
        paste_modal_visible_ = false;
        paste_modal_text_.clear();
        flash_message("Paste cancelled");
        return true;
    }
    if (ev == Event::ArrowUp) {
        paste_modal_choice_ = (paste_modal_choice_ - 1 + kPasteChoiceCount) %
                               kPasteChoiceCount;
        return true;
    }
    if (ev == Event::ArrowDown) {
        paste_modal_choice_ = (paste_modal_choice_ + 1) % kPasteChoiceCount;
        return true;
    }
    if (ev == Event::Return) {
        paste_modal_confirm();
        return true;
    }
    /* ホットキー: m / s / c で直接確定 (GUI の Calctus 風) */
    if (ev == Event::Character("m") || ev == Event::Character("M")) {
        paste_modal_choice_ = kPasteChoiceMultiRows;  paste_modal_confirm(); return true;
    }
    if (ev == Event::Character("s") || ev == Event::Character("S")) {
        paste_modal_choice_ = kPasteChoiceSingleLine; paste_modal_confirm(); return true;
    }
    if (ev == Event::Character("c") || ev == Event::Character("C")) {
        paste_modal_choice_ = kPasteChoiceCancel;     paste_modal_confirm(); return true;
    }
    /* 他キーは吸収のみ */
    return true;
}

void TuiApp::paste_modal_confirm() {
    int        choice = paste_modal_choice_;
    std::string text  = std::move(paste_modal_text_);
    paste_modal_visible_ = false;
    paste_modal_text_.clear();

    if (!sheet_) return;
    switch (choice) {
        case kPasteChoiceMultiRows:  sheet_->paste_multiline_as_rows(text);   break;
        case kPasteChoiceSingleLine: sheet_->paste_multiline_as_single(text); break;
        default: flash_message("Paste cancelled"); break;
    }
}

Element TuiApp::paste_modal_overlay() const {
    using namespace ftxui;

    int total = count_lines(paste_modal_text_);

    /* プレビュー: 先頭 8 行まで。それ以上なら "... and N more" 行を出す。 */
    Elements preview;
    {
        std::string line;
        int shown = 0;
        size_t i = 0;
        while (i < paste_modal_text_.size() && shown < kPastePreviewMaxLines) {
            char c = paste_modal_text_[i++];
            if (c == '\r' || c == '\n') {
                if (c == '\r' && i < paste_modal_text_.size() &&
                    paste_modal_text_[i] == '\n') ++i;
                preview.push_back(text(line) | dim);
                line.clear();
                ++shown;
            } else {
                line += c;
            }
        }
        if (shown < kPastePreviewMaxLines && !line.empty()) {
            preview.push_back(text(line) | dim);
            ++shown;
        }
        if (shown < total) {
            preview.push_back(
                text("... and " + std::to_string(total - shown) + " more line(s)")
                | dim);
        }
    }

    auto choice_row = [this](const char *label, const char *hint, int idx) {
        bool selected = (paste_modal_choice_ == idx);
        Element marker = selected ? text("[*] ") | bold
                                   : text("[ ] ");
        Element body   = hbox({
            std::move(marker),
            text(label),
            filler(),
            text(hint) | dim,
        });
        if (selected) body = body | inverted;
        return body;
    };

    Elements body;
    body.push_back(text("Paste Options") | bold | center);
    body.push_back(separator());
    body.push_back(text("Clipboard contains " + std::to_string(total) +
                        " line(s):") | dim);
    for (auto &p : preview) body.push_back(std::move(p));
    body.push_back(separator());
    body.push_back(choice_row("Insert each line as a new row", "(M)",
                              kPasteChoiceMultiRows));
    body.push_back(choice_row("Join into single line at cursor", "(S)",
                              kPasteChoiceSingleLine));
    body.push_back(choice_row("Cancel", "(C / Esc)",
                              kPasteChoiceCancel));
    body.push_back(separator());
    body.push_back(text("↑↓ select   Enter confirm   Esc cancel") | dim | center);

    return vbox(std::move(body)) | border | size(WIDTH, LESS_THAN, 70) |
           size(HEIGHT, LESS_THAN, 22) | clear_under |
           reflect(paste_modal_box_) | center;
}

/* ----------------------------------------------------------------------
 * コンテキストメニュー (右クリック)
 *
 * ↑↓: 項目移動 (separator スキップ)
 * Enter / 左クリック項目: 実行
 * Esc / 外側クリック: 閉じる
 * 各項目はキーボードショートカットも併記 (実際の処理はメインで動く)。
 * -------------------------------------------------------------------- */
namespace {

enum class ContextCmd {
    None,
    Separator,
    CopyRow,        /* 行を `expr = result` でコピー */
    CopyExpr,       /* 式のみコピー */
    CopyResult,     /* 結果のみコピー */
    Cut,
    Paste,
    InsertAbove,
    InsertBelow,
    DeleteRow,
};

struct ContextItem {
    const char *label;
    const char *shortcut;  /* 表示用、空文字なら表示しない */
    ContextCmd  cmd;
};

/* メニュー項目順は GUI の右クリックと揃える。 */
constexpr ContextItem kContextMenu[] = {
    { "Copy row",            "Ctrl+C",       ContextCmd::CopyRow     },
    { "Copy expression",     "",             ContextCmd::CopyExpr    },
    { "Copy result",         "",             ContextCmd::CopyResult  },
    { "Cut",                 "Ctrl+X",       ContextCmd::Cut         },
    { "Paste",               "Ctrl+V",       ContextCmd::Paste       },
    { "-",                   "",             ContextCmd::Separator   },
    { "Insert row above",    "Ctrl+Shift+N", ContextCmd::InsertAbove },
    { "Insert row below",    "Enter",        ContextCmd::InsertBelow },
    { "Delete row",          "Ctrl+D",       ContextCmd::DeleteRow   },
};
constexpr int kContextMenuCount = (int)(sizeof(kContextMenu) / sizeof(kContextMenu[0]));

/* 最も長い label の表示幅 + 余白 + shortcut の合計 (ASCII 前提)。 */
int context_menu_width() {
    int w = 0;
    for (const auto &it : kContextMenu) {
        if (it.cmd == ContextCmd::Separator) continue;
        int line = (int)std::strlen(it.label);
        if (it.shortcut[0]) line += 2 + (int)std::strlen(it.shortcut);
        if (line > w) w = line;
    }
    return w + 2;  /* 左右パディング */
}

} // namespace

void TuiApp::context_menu_open(int x, int y) {
    context_menu_visible_ = true;
    context_menu_anchor_x_ = x;
    context_menu_anchor_y_ = y;
    /* 最初の非 separator を選ぶ。 */
    context_menu_item_ = 0;
    while (context_menu_item_ < kContextMenuCount &&
           kContextMenu[context_menu_item_].cmd == ContextCmd::Separator) {
        ++context_menu_item_;
    }
}

void TuiApp::context_menu_close() {
    context_menu_visible_ = false;
}

void TuiApp::context_menu_move(int dir) {
    if (kContextMenuCount == 0) return;
    int i = context_menu_item_;
    for (int step = 0; step < kContextMenuCount; ++step) {
        i = (i + dir + kContextMenuCount) % kContextMenuCount;
        if (kContextMenu[i].cmd != ContextCmd::Separator) {
            context_menu_item_ = i;
            return;
        }
    }
}

void TuiApp::context_menu_activate() {
    if (!sheet_) { context_menu_close(); return; }
    if (context_menu_item_ < 0 || context_menu_item_ >= kContextMenuCount) {
        context_menu_close();
        return;
    }
    ContextCmd cmd = kContextMenu[context_menu_item_].cmd;
    context_menu_close();

    switch (cmd) {
        case ContextCmd::CopyRow:     sheet_->copy_focused_row();      break;
        case ContextCmd::CopyExpr:    sheet_->copy_focused_expr();     break;
        case ContextCmd::CopyResult:  sheet_->copy_focused_result();   break;
        case ContextCmd::Cut:         sheet_->cut_focused_row();       break;
        case ContextCmd::Paste:       sheet_->paste_at_cursor();       break;
        case ContextCmd::InsertAbove: sheet_->insert_row_above();      break;
        case ContextCmd::InsertBelow: sheet_->insert_row_below();      break;
        case ContextCmd::DeleteRow:   sheet_->delete_focused_row();    break;
        case ContextCmd::Separator:
        case ContextCmd::None:        break;
    }
}

bool TuiApp::context_menu_handle_event(Event ev) {
    if (!context_menu_visible_) return false;
    if (ev == Event::Escape) {
        context_menu_close();
        return true;
    }
    if (ev == Event::ArrowUp)   { context_menu_move(-1); return true; }
    if (ev == Event::ArrowDown) { context_menu_move(+1); return true; }
    if (ev == Event::Return)    { context_menu_activate();   return true; }
    /* 他キーは吸収 (誤入力でシートに届かないように)。 */
    return true;
}

Element TuiApp::context_menu_overlay() const {
    using namespace ftxui;

    Elements rows;
    context_menu_item_boxes_.assign(kContextMenuCount, Box{});
    for (int i = 0; i < kContextMenuCount; ++i) {
        const ContextItem &it = kContextMenu[i];
        if (it.cmd == ContextCmd::Separator) {
            rows.push_back(separator());
            continue;
        }
        bool selected = (i == context_menu_item_);
        Element row = hbox({
            text(" "),
            text(it.label),
            filler(),
            text(it.shortcut[0] ? std::string("  ") + it.shortcut
                                : std::string(" ")) | dim,
            text(" "),
        }) | reflect(context_menu_item_boxes_[i]);
        if (selected) row = row | inverted;
        rows.push_back(std::move(row));
    }

    int w = context_menu_width();
    /* アンカー位置からの相対配置: filler() でパディングしてから
     * 左上に押し込む。画面端は FTXUI 側で自動クリップされる。 */
    int anchor_x = std::max(0, context_menu_anchor_x_);
    int anchor_y = std::max(0, context_menu_anchor_y_);

    Element menu = vbox(std::move(rows)) | border |
                   size(WIDTH, EQUAL, w + 2) | clear_under |
                   reflect(context_menu_box_);

    /* x: anchor_x 列ぶん左に空白 + メニュー、y: anchor_y 行ぶん上に空白 + メニュー */
    Element anchored = hbox({
        text(std::string(anchor_x, ' ')),
        menu,
        filler(),
    });
    return vbox({
        text(std::string(anchor_y, ' ')),
        anchored,
        filler(),
    });
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
            /* ホットキー文字: bold + underlined は端末の "_F_ile" 表示と同じ
             * 古典スタイル。色に依存しないので白背景・モノクロでも読める。 */
            parts.push_back(disabled ? hot | dim : hot | bold | underlined);
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
    { "&Preferences...",  "",              MenuCmd::Preferences,    false, false, false },
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
    { "&Compact Mode",       "Ctrl+:",MenuCmd::ToggleCompact,      false, false, false },
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
};
constexpr int kMenuCount = (int)(sizeof(kMenus) / sizeof(MenuDef));

int menu_index(MenuId id) {
    for (int i = 0; i < kMenuCount; ++i) if (kMenus[i].id == id) return i;
    return -1;
}

} /* namespace */

Element TuiApp::menu_bar_render() const {
    /* マウス対応: 各タイトル Box を Render() ごとにリセット。 */
    menu_title_boxes_.assign(kMenuCount, Box{});

    Elements cells;
    cells.push_back(text(" "));
    for (int i = 0; i < kMenuCount; ++i) {
        Element cell = hbox({
            text(" "),
            label_elements(kMenus[i].title, /*disabled=*/false),
            text(" "),
        });
        if (kMenus[i].id == menu_active_) cell = cell | inverted;
        cell = cell | reflect(menu_title_boxes_[i]);
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

    /* マウス対応: メニュー項目 Box をリセット (区切り行は (0,0,0,0) のまま)。 */
    menu_item_boxes_.assign(def.count, Box{});

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
        row = row | reflect(menu_item_boxes_[i]);
        rows.push_back(std::move(row));
    }

    Element dd = vbox(std::move(rows)) | border;

    /* clear_under: dropdown 自身の領域を不透明に塗り、下層シートの inverted
     * カーソルや色付き文字が透けないようにする。 */
    dd = dd | clear_under;

    /* Samples submenu があれば右側に並べる。 */
    if (submenu_active_ && def.id == MenuId::File) {
        /* samples の数が多いと縦長になるので表示は最大 12 行でスクロール。 */
        const int visible = 12;
        int count = (int)samples_files_.size();
        /* マウス対応: 全 sample 数ぶん boxes を確保し、未描画行は空のまま。 */
        submenu_item_boxes_.assign(count, Box{});
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
                row = row | reflect(submenu_item_boxes_[i]);
                srows.push_back(std::move(row));
            }
        }
        Element sdd = vbox(std::move(srows)) | border | clear_under;
        dd = hbox({ dd, sdd });
    } else {
        submenu_item_boxes_.clear();
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
        case MenuCmd::Preferences: do_preferences();                                       break;
        case MenuCmd::About:
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
                /* サンプルは読み取り専用。Ctrl+S は Save-As に落として
                 * 配布版サンプルの上書きを防ぐ。 */
                sheet_->set_read_only(true);
                sheet_->reload_focused_row();
                flash_message("Loaded sample: " + samples_files_[sub_idx]);
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
 * マウスディスパッチ
 * -------------------------------------------------------------------- */
bool TuiApp::handle_mouse(const Mouse &m) {
    /* コンテキストメニュー中: 項目クリックで実行、外側クリックで閉じる。
     * ホイール等は吸収。 */
    if (context_menu_visible_) {
        if (m.button == Mouse::Left && m.motion == Mouse::Pressed) {
            for (int i = 0; i < (int)context_menu_item_boxes_.size(); ++i) {
                if (i >= kContextMenuCount) break;
                if (kContextMenu[i].cmd == ContextCmd::Separator) continue;
                if (!context_menu_item_boxes_[i].Contain(m.x, m.y)) continue;
                context_menu_item_ = i;
                context_menu_activate();
                return true;
            }
            /* 外側 → 閉じる。 */
            context_menu_close();
            return true;
        }
        return true;  /* メニュー中は他のマウスも吸収 */
    }

    /* About: ホイールでスクロール、外側クリックで閉じる。 */
    if (about_visible_) {
        if (m.button == Mouse::WheelUp)   { about_scroll_ = std::max(0, about_scroll_ - 1); return true; }
        if (m.button == Mouse::WheelDown) { about_scroll_ = std::min(kAboutMaxScroll, about_scroll_ + 1); return true; }
        if (m.button == Mouse::Left && m.motion == Mouse::Pressed) {
            if (!about_box_.Contain(m.x, m.y)) about_visible_ = false;
            return true;
        }
        return true;  /* About 中は他のマウスも吸収 */
    }

    /* プロンプト: 外側クリックでキャンセル。それ以外は吸収だけ。 */
    if (prompt_mode_ != PromptMode::None) {
        if (m.button == Mouse::Left && m.motion == Mouse::Pressed) {
            if (!prompt_box_.Contain(m.x, m.y)) { prompt_cancel(); return true; }
        }
        return true;
    }

    /* メニュー展開中。 */
    if (menu_active_ != MenuId::None) {
        if (m.button == Mouse::Left && m.motion == Mouse::Pressed) {
            /* タイトルクリック: 同じなら閉じる、別なら切替。 */
            for (int i = 0; i < (int)menu_title_boxes_.size(); ++i) {
                if (!menu_title_boxes_[i].Contain(m.x, m.y)) continue;
                if (kMenus[i].id == menu_active_) menu_close();
                else                              menu_open(kMenus[i].id);
                return true;
            }
            /* submenu 項目 (展開中のみ)。 */
            if (submenu_active_) {
                for (int i = 0; i < (int)submenu_item_boxes_.size(); ++i) {
                    if (!submenu_item_boxes_[i].Contain(m.x, m.y)) continue;
                    submenu_item_ = i;
                    menu_activate_current();
                    return true;
                }
            }
            /* メニュー項目。 */
            int idx = menu_index(menu_active_);
            if (idx >= 0) {
                const MenuDef &def = kMenus[idx];
                for (int i = 0; i < (int)menu_item_boxes_.size(); ++i) {
                    if (!menu_item_boxes_[i].Contain(m.x, m.y)) continue;
                    if (i >= def.count) break;
                    const MenuItem &it = def.items[i];
                    if (it.separator || it.disabled) return true;
                    menu_item_ = i;
                    menu_activate_current();
                    return true;
                }
            }
            /* 外側 → 閉じる。 */
            menu_close();
            return true;
        }
        /* メニュー展開中はホイール等も吸収。 */
        return true;
    }

    /* メニュー未展開時にメニューバータイトルをクリック → 開く。 */
    if (m.button == Mouse::Left && m.motion == Mouse::Pressed) {
        for (int i = 0; i < (int)menu_title_boxes_.size(); ++i) {
            if (!menu_title_boxes_[i].Contain(m.x, m.y)) continue;
            menu_open(kMenus[i].id);
            return true;
        }
    }
    /* それ以外は sheet に流す (sheet が WheelUp/Down・行クリック等を処理)。 */
    return false;
}

/* ----------------------------------------------------------------------
 * テスト用: CatchEvent → sheet OnEvent の経路をテストから再現する
 * -------------------------------------------------------------------- */
void TuiApp::test_dispatch(Event ev) {
    /* 本番 Loop 側の CatchEvent と同じ flash クリア手順を再現する
     * (テストから flash の遅延消去を観測したい場合に整合する)。 */
    if (flash_pending_clear_ &&
        (!ev.is_mouse() || ev.mouse().motion == Mouse::Pressed)) {
        status_message_.clear();
        flash_pending_clear_ = false;
    }

    if (ev.is_mouse()) {
        if (!handle_mouse(ev.mouse())) sheet_->OnEvent(ev);
    } else if (context_menu_handle_event(ev)) {
    } else if (paste_modal_handle_event(ev)) {
    } else if (about_handle_event(ev)) {
    } else if (ev == Event::F1) {
        about_visible_ = true; about_scroll_ = 0;
    } else if (menu_handle_event(ev)) {
    } else if (!prompt_handle_event(ev)) {
        sheet_->OnEvent(ev);
    }

    if (!status_message_.empty()) flash_pending_clear_ = true;
}

/* ----------------------------------------------------------------------
 * メインループ
 * -------------------------------------------------------------------- */

#if !defined(_WIN32)
/* Ctrl+Z (0x1A) / Ctrl+C (0x03) / Ctrl+S (0x13) / Ctrl+Q (0x11) を
 * シグナルやフロー制御としてではなく生バイトとして受け取るため、
 * ISIG / IEXTEN / IXON を落とす (microsoft/edit と同方針)。
 *
 * IXON を残すと kernel の tty 層が Ctrl+S を XOFF として吸収して出力を
 * 凍結してしまい、アプリが復帰できなくなる (Ctrl+Q も XON として吸収
 * されるので Quit が効かない)。
 *
 * FTXUI の Install/Uninstall が我々の改変済み termios を保存・復元する
 * ので、最終的に元の (我々が起動前に保存した) termios に戻す必要がある。
 * RAII で保証する。 */
class TermiosIsigGuard {
    int            fd_;
    bool           ok_ = false;
    struct termios saved_{};
public:
    explicit TermiosIsigGuard(int fd) : fd_(fd) {
        if (!::isatty(fd_)) return;
        if (::tcgetattr(fd_, &saved_) != 0) return;
        ok_ = true;
        struct termios t = saved_;
        t.c_lflag &= ~(ISIG | IEXTEN);
        t.c_iflag &= ~IXON;
        ::tcsetattr(fd_, TCSANOW, &t);
    }
    ~TermiosIsigGuard() {
        if (ok_) ::tcsetattr(fd_, TCSANOW, &saved_);
    }
    TermiosIsigGuard(const TermiosIsigGuard &)            = delete;
    TermiosIsigGuard &operator=(const TermiosIsigGuard &) = delete;
};
#endif

int TuiApp::run(const std::string &initial_file) {
#if !defined(_WIN32)
    TermiosIsigGuard _termios_guard(STDIN_FILENO);
#endif

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
        /* 最下行は単一スロット。優先度: prompt > flash > help。
         *   - prompt: Ctrl+O/S 入力中
         *   - flash:  status_message_ あり (次イベントで消える)
         *   - help:   ホットキーヒント (compact 時は何も出さない)
         * compact 時は flash と help を隠し、prompt のみ表示する。 */
        Element bottom_slot;
        bool bottom_visible = true;
        if (prompt_mode_ != PromptMode::None) {
            const std::string &b = prompt_buf_;
            size_t p = std::min(prompt_cursor_, b.size());
            std::string a = b.substr(0, p);
            std::string m = (p < b.size()) ? std::string(1, b[p]) : std::string(" ");
            std::string c = (p < b.size()) ? b.substr(p + 1) : "";
            /* "Open:" / "Save:" 等のラベル。白背景端末でも読めるよう
             * 蛍光ペン (黒文字 + 黄色背景) スタイルで明示する。 */
            bottom_slot = hbox({
                text(prompt_label_) | color(Color::Black) | bgcolor(Color::YellowLight),
                text(" "),
                text(a),
                text(m) | inverted,
                text(c),
            }) | reflect(prompt_box_);
        } else if (!status_message_.empty()) {
            if (sheet_->compact_mode()) {
                bottom_visible = false;
            } else {
                bottom_slot = text(" " + status_message_ + " ") | dim;
            }
        } else if (sheet_->compact_mode()) {
            bottom_visible = false;
        } else {
            bottom_slot = text(" F1 help  Alt+F menu  ^Q quit  "
                               "^Z/^Y undo/redo  F8-F12 fmt ") | dim;
        }
        base = bottom_visible
                 ? vbox({ body | flex, bottom_slot })
                 : vbox({ body | flex });

        /* compact のときはメニューバーも隠す (F6 / Ctrl+: で復帰)。 */
        if (!sheet_->compact_mode()) {
            base = vbox({ menu_bar_render(), base | flex });
        }

        Elements layers;
        layers.push_back(base);
        if (menu_active_ != MenuId::None) layers.push_back(menu_overlay());
        if (about_visible_)               layers.push_back(about_overlay());
        if (paste_modal_visible_)         layers.push_back(paste_modal_overlay());
        if (context_menu_visible_)        layers.push_back(context_menu_overlay());
        if (layers.size() == 1) return base;
        return dbox(std::move(layers));
    });

    auto root = CatchEvent(renderer, [this](Event ev) {
        /* flash の遅延クリア: 前回の flash が「次イベントで消す」マーク
         * 付きなら、今回のイベント先頭で消す。マウス移動などの passive
         * イベントで消えないよう、キー / クリックに限定する。 */
        if (flash_pending_clear_ &&
            (!ev.is_mouse() || ev.mouse().motion == Mouse::Pressed)) {
            status_message_.clear();
            flash_pending_clear_ = false;
        }

        bool handled;
        if (ev.is_mouse()) {
            /* TuiApp 自身が消費しないマウスは sheet (CatchEvent の child)
             * に流す。handle_mouse() が false ならここで false を返せば
             * Renderer 経由で sheet->OnEvent() に届く。 */
            handled = handle_mouse(ev.mouse());
        } else if (context_menu_handle_event(ev)) {
            /* コンテキストメニュー中は全キーをメニューに渡す。 */
            handled = true;
        } else if (paste_modal_handle_event(ev)) {
            /* Paste モーダル中は全キーをモーダルに渡す。 */
            handled = true;
        } else if (about_handle_event(ev)) {
            /* About が開いている間は全イベントを吸収する。 */
            handled = true;
        } else if (ev == Event::F1) {
            /* 非表示のときに F1 で開く。 */
            about_visible_ = true;
            about_scroll_  = 0;
            handled = true;
        } else if (menu_handle_event(ev)) {
            handled = true;
        } else {
            handled = prompt_handle_event(ev);
        }

        /* このイベントで flash が立ったなら、次イベントで消すマーク。 */
        if (!status_message_.empty()) flash_pending_clear_ = true;
        return handled;
    });

    screen_.Loop(root);
    return 0;
}

} // namespace calcyx::tui
