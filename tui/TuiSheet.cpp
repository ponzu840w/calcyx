#include "TuiSheet.h"

#include "keymap.h"

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
#include "parser/lexer.h"
#include "types/val.h"
}

using namespace ftxui;

namespace calcyx::tui {

namespace {

bool is_word_char(unsigned char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') || c == '_';
}

size_t word_left(const std::string &s, size_t pos) {
    if (pos == 0) return 0;
    size_t p = pos;
    while (p > 0 && !is_word_char((unsigned char)s[p-1])) --p;
    while (p > 0 &&  is_word_char((unsigned char)s[p-1])) --p;
    return p;
}

size_t word_right(const std::string &s, size_t pos) {
    size_t p = pos;
    while (p < s.size() && !is_word_char((unsigned char)s[p])) ++p;
    while (p < s.size() &&  is_word_char((unsigned char)s[p])) ++p;
    return p;
}

const char *fmt_label(val_fmt_t fmt) {
    switch (fmt) {
        case FMT_REAL:       return "Dec";
        case FMT_INT:        return "Int";
        case FMT_HEX:        return "Hex";
        case FMT_BIN:        return "Bin";
        case FMT_OCT:        return "Oct";
        case FMT_SI_PREFIX:  return "SI";
        case FMT_BIN_PREFIX: return "Kibi";
        case FMT_CHAR:       return "Char";
        case FMT_STRING:     return "Str";
        case FMT_DATETIME:   return "Date";
        case FMT_WEB_COLOR:  return "Color";
        default:             return "?";
    }
}

} // namespace

/* ----------------------------------------------------------------------
 * ctor / dtor
 * -------------------------------------------------------------------- */
TuiSheet::TuiSheet(sheet_model_t *model) : model_(model) {
    load_editor_from_row();
}

TuiSheet::~TuiSheet() = default;

std::shared_ptr<TuiSheet> MakeTuiSheet(sheet_model_t *m) {
    return std::make_shared<TuiSheet>(m);
}

/* ----------------------------------------------------------------------
 * 編集バッファ操作
 * -------------------------------------------------------------------- */
void TuiSheet::load_editor_from_row() {
    const char *e = sheet_model_row_expr(model_, focused_row_);
    editor_buf_     = e ? e : "";
    original_expr_  = editor_buf_;
    cursor_pos_     = editor_buf_.size();
    live_preview_.clear();
}

void TuiSheet::reload_focused_row() {
    focused_row_ = std::min(focused_row_,
                             std::max(0, sheet_model_row_count(model_) - 1));
    load_editor_from_row();
}

/* ----------------------------------------------------------------------
 * 補完
 * -------------------------------------------------------------------- */
std::string TuiSheet::current_word_at_cursor(size_t *out_start) const {
    /* cursor の 1 文字左を起点に、id_follow の範囲を後ろへ拡大。
     * 先頭が id_start でなければ補完対象外 (空文字列を返す)。 */
    if (cursor_pos_ == 0) {
        if (out_start) *out_start = 0;
        return "";
    }
    size_t start = cursor_pos_;
    while (start > 0 && lexer_is_id_follow(editor_buf_[start - 1])) --start;
    if (start >= cursor_pos_) {
        if (out_start) *out_start = cursor_pos_;
        return "";
    }
    if (!lexer_is_id_start(editor_buf_[start])) {
        if (out_start) *out_start = cursor_pos_;
        return "";
    }
    if (out_start) *out_start = start;
    return editor_buf_.substr(start, cursor_pos_ - start);
}

void TuiSheet::completion_trigger() {
    size_t start = 0;
    std::string key = current_word_at_cursor(&start);
    completion_.reload(model_);
    completion_.open(key);
}

void TuiSheet::completion_update_key() {
    if (!completion_visible()) return;
    size_t start = 0;
    std::string key = current_word_at_cursor(&start);
    completion_.update_key(key);
}

void TuiSheet::completion_confirm() {
    const TuiCompletion::Item *c = completion_.selected();
    if (!c) { completion_.hide(); return; }

    size_t start = 0;
    (void)current_word_at_cursor(&start);

    std::string rep = c->id;
    if (c->is_function) rep += "(";

    editor_buf_.replace(start, cursor_pos_ - start, rep);
    cursor_pos_ = start + rep.size();
    completion_.hide();
    live_evaluate();
}

bool TuiSheet::editor_dirty() const {
    return editor_buf_ != original_expr_;
}

void TuiSheet::commit_if_changed() {
    if (!editor_dirty()) return;

    std::string old_expr = original_expr_;
    std::string new_expr = editor_buf_;

    sheet_op_t undo_op{ SHEET_OP_CHANGE_EXPR, focused_row_, old_expr.c_str() };
    sheet_op_t redo_op{ SHEET_OP_CHANGE_EXPR, focused_row_, new_expr.c_str() };
    sheet_view_state_t vs = capture_view_state();
    sheet_model_commit(model_, &undo_op, 1, &redo_op, 1, vs);

    original_expr_ = new_expr;
    live_preview_.clear();
}

void TuiSheet::live_evaluate() {
    if (!editor_dirty()) {
        live_preview_.clear();
        return;
    }
    /* sheet_model には set_row_expr_raw しかないので、
     * 一時的に focused 行を編集バッファに差し替えて eval_all → 戻す */
    std::string backup = original_expr_;
    sheet_model_set_row_expr_raw(model_, focused_row_, editor_buf_.c_str());
    sheet_model_eval_all(model_);
    const char *res = sheet_model_row_result(model_, focused_row_);
    live_preview_ = res ? res : "";
    sheet_model_set_row_expr_raw(model_, focused_row_, backup.c_str());
    sheet_model_eval_all(model_);
}

/* ----------------------------------------------------------------------
 * view state
 * -------------------------------------------------------------------- */
sheet_view_state_t TuiSheet::capture_view_state() const {
    return { focused_row_, (int)cursor_pos_ };
}

void TuiSheet::restore_view_state(const sheet_view_state_t &vs) {
    focused_row_ = std::clamp(vs.focused_row, 0,
                               std::max(0, sheet_model_row_count(model_) - 1));
    load_editor_from_row();
    cursor_pos_ = std::min((size_t)std::max(0, vs.cursor_pos), editor_buf_.size());
}

/* ----------------------------------------------------------------------
 * カーソル操作
 * -------------------------------------------------------------------- */
void TuiSheet::action_cursor_left() {
    if (cursor_pos_ > 0) --cursor_pos_;
}
void TuiSheet::action_cursor_right() {
    if (cursor_pos_ < editor_buf_.size()) ++cursor_pos_;
}
void TuiSheet::action_cursor_home() { cursor_pos_ = 0; }
void TuiSheet::action_cursor_end()  { cursor_pos_ = editor_buf_.size(); }
void TuiSheet::action_cursor_word_left()  { cursor_pos_ = word_left(editor_buf_, cursor_pos_); }
void TuiSheet::action_cursor_word_right() { cursor_pos_ = word_right(editor_buf_, cursor_pos_); }

void TuiSheet::action_row_up() {
    if (focused_row_ <= 0) return;
    commit_if_changed();
    --focused_row_;
    load_editor_from_row();
}
void TuiSheet::action_row_down() {
    commit_if_changed();
    int n = sheet_model_row_count(model_);
    if (focused_row_ + 1 < n) {
        ++focused_row_;
        load_editor_from_row();
    }
}
void TuiSheet::action_page(int dir) {
    commit_if_changed();
    int n    = sheet_model_row_count(model_);
    int step = 10;
    focused_row_ = std::clamp(focused_row_ + dir * step, 0, std::max(0, n-1));
    load_editor_from_row();
}

/* ----------------------------------------------------------------------
 * 編集
 * -------------------------------------------------------------------- */
void TuiSheet::action_insert_char(const std::string &s) {
    editor_buf_.insert(cursor_pos_, s);
    cursor_pos_ += s.size();
    live_evaluate();
}
void TuiSheet::action_backspace() {
    if (cursor_pos_ == 0) return;
    editor_buf_.erase(cursor_pos_ - 1, 1);
    --cursor_pos_;
    live_evaluate();
}
void TuiSheet::action_delete_char() {
    if (cursor_pos_ >= editor_buf_.size()) return;
    editor_buf_.erase(cursor_pos_, 1);
    live_evaluate();
}
void TuiSheet::action_delete_word() {
    size_t left = word_left(editor_buf_, cursor_pos_);
    editor_buf_.erase(left, cursor_pos_ - left);
    cursor_pos_ = left;
    live_evaluate();
}
void TuiSheet::action_kill_line_right() {
    editor_buf_.erase(cursor_pos_);
    live_evaluate();
}

/* ----------------------------------------------------------------------
 * 行操作
 * -------------------------------------------------------------------- */
void TuiSheet::action_commit_and_insert_below() {
    commit_if_changed();
    int n = sheet_model_row_count(model_);
    int insert_at = focused_row_ + 1;
    sheet_op_t redo_op{ SHEET_OP_INSERT, insert_at, "" };
    sheet_op_t undo_op{ SHEET_OP_DELETE, insert_at, nullptr };
    sheet_view_state_t vs = capture_view_state();
    sheet_model_commit(model_, &undo_op, 1, &redo_op, 1, vs);
    focused_row_ = std::min(insert_at, sheet_model_row_count(model_) - 1);
    load_editor_from_row();
    (void)n;
}
void TuiSheet::action_insert_above() {
    commit_if_changed();
    int insert_at = focused_row_;
    sheet_op_t redo_op{ SHEET_OP_INSERT, insert_at, "" };
    sheet_op_t undo_op{ SHEET_OP_DELETE, insert_at, nullptr };
    sheet_view_state_t vs = capture_view_state();
    sheet_model_commit(model_, &undo_op, 1, &redo_op, 1, vs);
    /* 元の focused_row_ の位置に新行が入り、元行は +1 される。
     * 新規行 = 旧 focused_row_ をそのまま編集したい。 */
    load_editor_from_row();
}
void TuiSheet::action_delete_row() {
    int n = sheet_model_row_count(model_);
    if (n <= 1) return;  /* 最低 1 行は残す */

    const char *cur = sheet_model_row_expr(model_, focused_row_);
    std::string expr_copy = cur ? cur : "";

    sheet_op_t redo_op{ SHEET_OP_DELETE, focused_row_, nullptr };
    sheet_op_t undo_op{ SHEET_OP_INSERT, focused_row_, expr_copy.c_str() };
    sheet_view_state_t vs = capture_view_state();
    sheet_model_commit(model_, &undo_op, 1, &redo_op, 1, vs);

    int new_n = sheet_model_row_count(model_);
    focused_row_ = std::min(focused_row_, std::max(0, new_n - 1));
    load_editor_from_row();
}
/* 上方向に消す: 行を削除後、元の行の「上」にフォーカスを移す。
 * Shift+Del / Shift+BS および「空行で BS」のいずれの入口でも同じ挙動。
 * 移植元 SheetView::delete_row_up (ui/SheetView.cpp:1460) と対応。 */
void TuiSheet::action_delete_row_up() {
    int n = sheet_model_row_count(model_);
    if (n <= 1) {
        /* 最後の 1 行は削除せず、内容だけクリア */
        if (!editor_buf_.empty() || !original_expr_.empty()) {
            editor_buf_.clear();
            original_expr_.clear();
            cursor_pos_ = 0;
            sheet_op_t undo_op{ SHEET_OP_CHANGE_EXPR, 0,
                                sheet_model_row_expr(model_, 0) };
            sheet_op_t redo_op{ SHEET_OP_CHANGE_EXPR, 0, "" };
            sheet_view_state_t vs = capture_view_state();
            sheet_model_commit(model_, &undo_op, 1, &redo_op, 1, vs);
            load_editor_from_row();
        }
        return;
    }
    const char *cur = sheet_model_row_expr(model_, focused_row_);
    std::string expr_copy = cur ? cur : "";
    int target = focused_row_;

    sheet_op_t redo_op{ SHEET_OP_DELETE, target, nullptr };
    sheet_op_t undo_op{ SHEET_OP_INSERT, target, expr_copy.c_str() };
    sheet_view_state_t vs = capture_view_state();
    sheet_model_commit(model_, &undo_op, 1, &redo_op, 1, vs);

    focused_row_ = std::max(0, target - 1);
    load_editor_from_row();
    cursor_pos_ = editor_buf_.size();
}
void TuiSheet::action_move_row(int dir) {
    if (editor_dirty()) commit_if_changed();
    int n = sheet_model_row_count(model_);
    int dst = focused_row_ + dir;
    if (dst < 0 || dst >= n) return;

    /* 入れ替えは "delete src → insert at dst" の 2 op 複合 */
    const char *src_expr_c = sheet_model_row_expr(model_, focused_row_);
    std::string src_expr   = src_expr_c ? src_expr_c : "";

    sheet_op_t redo_ops[2] = {
        { SHEET_OP_DELETE, focused_row_, nullptr },
        { SHEET_OP_INSERT, dst,          src_expr.c_str() },
    };
    sheet_op_t undo_ops[2] = {
        { SHEET_OP_DELETE, dst,           nullptr },
        { SHEET_OP_INSERT, focused_row_,  src_expr.c_str() },
    };
    sheet_view_state_t vs = capture_view_state();
    sheet_model_commit(model_, undo_ops, 2, redo_ops, 2, vs);

    focused_row_ = dst;
    load_editor_from_row();
}

/* ----------------------------------------------------------------------
 * undo / redo
 * -------------------------------------------------------------------- */
void TuiSheet::action_undo() {
    /* undo 前に編集中内容を廃棄 (GUI と同じ挙動) */
    sheet_view_state_t vs{};
    if (sheet_model_undo(model_, &vs)) {
        restore_view_state(vs);
    }
}
void TuiSheet::action_redo() {
    sheet_view_state_t vs{};
    if (sheet_model_redo(model_, &vs)) {
        restore_view_state(vs);
    }
}

/* ----------------------------------------------------------------------
 * 全体操作 (全消去 / 全コピー / 再計算 / 小数桁±)
 * -------------------------------------------------------------------- */
void TuiSheet::action_recalculate() {
    commit_if_changed();
    sheet_model_eval_all(model_);
    load_editor_from_row();
    if (status_cb_) status_cb_("Recalculated");
}

/* 移植元: ui/SheetView.cpp:1523 clear_all()
 * 全行を 1 つの空行に戻す。undo は「最終行を残して他を削除 → 残行を空文字に置換」
 * の複合 op として積む。 */
void TuiSheet::action_clear_all() {
    int n = sheet_model_row_count(model_);
    const char *first = sheet_model_row_expr(model_, 0);
    if (n == 1 && (!first || !first[0])) return;  /* 既に空 */

    std::vector<std::string> exprs;
    exprs.reserve(n);
    for (int i = 0; i < n; ++i) {
        const char *e = sheet_model_row_expr(model_, i);
        exprs.emplace_back(e ? e : "");
    }

    std::vector<sheet_op_t> undo_ops;
    undo_ops.push_back({ SHEET_OP_CHANGE_EXPR, 0, exprs[0].c_str() });
    for (int i = 1; i < n; ++i)
        undo_ops.push_back({ SHEET_OP_INSERT, i, exprs[i].c_str() });

    std::vector<sheet_op_t> redo_ops;
    redo_ops.push_back({ SHEET_OP_CHANGE_EXPR, 0, "" });
    for (int i = n - 1; i >= 1; --i)
        redo_ops.push_back({ SHEET_OP_DELETE, i, "" });

    sheet_view_state_t vs = capture_view_state();
    sheet_model_commit(model_,
                        undo_ops.data(), (int)undo_ops.size(),
                        redo_ops.data(), (int)redo_ops.size(),
                        vs);
    focused_row_ = 0;
    load_editor_from_row();
    if (status_cb_) status_cb_("Cleared");
}

/* 全コピー: OSC 52 経由でターミナルのクリップボードへ送る。
 * Windows Terminal / Kitty / WezTerm / Alacritty / iTerm2 / tmux(enable setting on)
 * が対応。非対応端末では無視される (フィードバックは "Copied (OSC 52)" のまま)。
 * 書式: "<expr> = <result>\n" を全行結合。移植元 ui/SheetView.cpp:1591。 */
namespace {
std::string osc52_base64(const std::string &in) {
    static const char *chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((in.size() + 2) / 3) * 4);
    int val = 0, valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) | c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(chars[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}
} // namespace

void TuiSheet::action_copy_all() {
    std::string text;
    int n = sheet_model_row_count(model_);
    for (int i = 0; i < n; ++i) {
        const char *expr   = sheet_model_row_expr(model_, i);
        const char *result = sheet_model_row_result(model_, i);
        text += expr ? expr : "";
        if (result && result[0] && sheet_model_row_visible(model_, i)) {
            text += " = ";
            text += result;
        }
        text += "\n";
    }
    std::string b64 = osc52_base64(text);
    /* OSC 52 は stdout/stderr どちらでもターミナルに届く。stderr を使って
     * FTXUI の描画バッファ (stdout) と干渉させない。 */
    std::fprintf(stderr, "\x1b]52;c;%s\x07", b64.c_str());
    std::fflush(stderr);
    if (status_cb_)
        status_cb_("Copied " + std::to_string(n) + " row(s) to clipboard");
}

/* 小数桁の増減は engine 側のグローバル g_fmt_settings を直接触る (GUI と同じ)。
 * 範囲は GUI に合わせて 1..34。再評価してステータス表示。 */
void TuiSheet::action_decimals_inc() {
    if (g_fmt_settings.decimal_len >= 34) return;
    g_fmt_settings.decimal_len++;
    sheet_model_eval_all(model_);
    if (status_cb_)
        status_cb_("Decimals: " + std::to_string(g_fmt_settings.decimal_len));
}
void TuiSheet::action_decimals_dec() {
    if (g_fmt_settings.decimal_len <= 1) return;
    g_fmt_settings.decimal_len--;
    sheet_model_eval_all(model_);
    if (status_cb_)
        status_cb_("Decimals: " + std::to_string(g_fmt_settings.decimal_len));
}

/* ----------------------------------------------------------------------
 * format 切替
 * -------------------------------------------------------------------- */
void TuiSheet::action_format(val_fmt_t /*fmt*/, const char *fmt_func) {
    /* 既存ラッパーを剥がし、非 Auto なら新しいラッパーで包む */
    const char *cur_c = sheet_model_row_expr(model_, focused_row_);
    std::string cur   = cur_c ? cur_c : "";
    char *stripped_c  = sheet_model_strip_formatter(cur.c_str());
    std::string stripped = stripped_c ? stripped_c : cur;
    if (stripped_c) free(stripped_c);

    std::string new_expr;
    if (!fmt_func || !*fmt_func) {
        new_expr = stripped;  /* Auto = 剥がすだけ */
    } else {
        new_expr = std::string(fmt_func) + "(" + stripped + ")";
    }
    if (new_expr == cur) return;

    sheet_op_t undo_op{ SHEET_OP_CHANGE_EXPR, focused_row_, cur.c_str() };
    sheet_op_t redo_op{ SHEET_OP_CHANGE_EXPR, focused_row_, new_expr.c_str() };
    sheet_view_state_t vs = capture_view_state();
    sheet_model_commit(model_, &undo_op, 1, &redo_op, 1, vs);
    load_editor_from_row();
}

/* ----------------------------------------------------------------------
 * 描画
 * -------------------------------------------------------------------- */
Element TuiSheet::render_row(int idx, bool is_focused, int result_col) const {
    const char *expr_c  = sheet_model_row_expr(model_, idx);
    const char *result_c = sheet_model_row_result(model_, idx);
    std::string expr    = expr_c   ? expr_c   : "";
    std::string result  = result_c ? result_c : "";

    Element left;
    if (is_focused) {
        /* カーソル表示: 編集バッファを使い、cursor_pos_ を反転させる */
        const std::string &buf = editor_buf_;
        size_t p = std::min(cursor_pos_, buf.size());
        std::string a = buf.substr(0, p);
        std::string b;
        std::string c = (p < buf.size()) ? buf.substr(p + 1) : "";
        if (p < buf.size()) b = std::string(1, buf[p]);
        else                b = " ";  /* 末尾カーソル用のスペース */

        left = hbox({
            text("> "),
            text(a),
            text(b) | inverted,
            text(c),
        });
    } else {
        left = hbox({
            text("  "),
            text(expr) | dim,
        });
    }

    /* 結果表示 */
    std::string res_text;
    if (is_focused && editor_dirty()) {
        res_text = live_preview_.empty() ? "" : ("= " + live_preview_);
    } else if (sheet_model_row_visible(model_, idx)) {
        res_text = "= " + result;
    } else {
        res_text = "";
    }

    Element right = text(res_text);
    if (sheet_model_row_error(model_, idx)) {
        right = right | color(Color::Red);
    } else if (is_focused && editor_dirty()) {
        right = right | color(Color::Yellow);
    } else {
        right = right | color(Color::GreenLight);
    }

    Element row = hbox({
        left | flex,
        text(" "),
        right,
    });
    if (is_focused) row = row | bgcolor(Color::Blue);
    return row;
}

Element TuiSheet::Render() {
    int n = sheet_model_row_count(model_);
    Elements rows;
    rows.reserve(n + 2);

    /* 2 列レイアウト: 左 = expr、右 = result。
     * カーソル行が画面外に出ないよう yframe + focus でスクロール。
     * 補完ポップアップは焦点行の直下に挟み込む。 */
    for (int i = 0; i < n; ++i) {
        Element row = render_row(i, i == focused_row_, 0);
        if (i == focused_row_) row = row | focus;
        rows.push_back(row);
        if (i == focused_row_ && completion_visible()) {
            Element popup = hbox({
                text("  "),
                completion_.render(8) | size(WIDTH, LESS_THAN, 40),
            });
            rows.push_back(popup);
        }
    }

    val_fmt_t cur_fmt = sheet_model_row_fmt(model_, focused_row_);
    std::string status =
        "[" + std::to_string(focused_row_ + 1) + "/" + std::to_string(n) + "]  "
        "fmt: " + fmt_label(cur_fmt);
    if (!file_path_.empty()) status += "  file: " + file_path_;
    if (editor_dirty())      status += "  *";
    if (completion_visible()) {
        status += "  (" + std::to_string(completion_.filtered_count()) +
                   " candidates)";
    }

    std::string help = " ^Q quit  ^Z undo  ^Y redo  ^D del  ^S save  ^O open "
                       " Tab complete  F8-F12 fmt ";

    return vbox({
        vbox(std::move(rows)) | yframe | flex,
        separator(),
        hbox({
            text(status) | flex,
        }),
        text(help) | dim,
    });
}

/* ----------------------------------------------------------------------
 * OnEvent
 * -------------------------------------------------------------------- */
bool TuiSheet::OnEvent(Event ev) {
    /* 補完ポップアップが開いている間は、矢印・Enter・Esc を横取りする。
     * 文字入力・Backspace などはそのまま通して、更新後に key を追従させる。 */
    if (completion_visible()) {
        if (ev == Event::Escape)    { completion_.hide();          return true; }
        if (ev == Event::ArrowUp)   { completion_.move_selection(-1); return true; }
        if (ev == Event::ArrowDown) { completion_.move_selection(+1); return true; }
        if (ev == Event::Return)    { completion_confirm();         return true; }
        if (ev == Event::Tab)       { completion_confirm();         return true; }
    }

    Action a = map(ev);

    /* トリガ (Tab or Ctrl+Space) の場合、先にチェック */
    if (a == Action::CompletionTrigger) {
        completion_trigger();
        return true;
    }

    bool needs_key_update = false;

    switch (a) {
        case Action::None:    return false;
        case Action::Quit:
            if (quit_cb_) quit_cb_();
            return true;

        case Action::CursorLeft:       action_cursor_left();
                                       needs_key_update = true;     break;
        case Action::CursorRight:      action_cursor_right();
                                       needs_key_update = true;     break;
        case Action::CursorHome:       action_cursor_home();
                                       completion_.hide();          break;
        case Action::CursorEnd:        action_cursor_end();
                                       completion_.hide();          break;
        case Action::CursorWordLeft:   action_cursor_word_left();
                                       needs_key_update = true;     break;
        case Action::CursorWordRight:  action_cursor_word_right();
                                       needs_key_update = true;     break;

        case Action::RowUp:            action_row_up();
                                       completion_.hide();          break;
        case Action::RowDown:          action_row_down();
                                       completion_.hide();          break;
        case Action::RowPageUp:        action_page(-1);
                                       completion_.hide();          break;
        case Action::RowPageDown:      action_page(+1);
                                       completion_.hide();          break;

        case Action::InsertChar:
            action_insert_char(ev.character());
            needs_key_update = true;
            break;
        case Action::Backspace:
            /* GUI 互換: 編集中の行が空で BS → delete_row_up。
             * 行を跨いだ編集継続ができるので "改行キー" の逆操作になる。 */
            if (editor_buf_.empty()) {
                action_delete_row_up();
                completion_.hide();
            } else {
                action_backspace();
                needs_key_update = true;
            }
            break;
        case Action::DeleteChar:       action_delete_char();
                                       needs_key_update = true;     break;
        case Action::DeleteWord:       action_delete_word();
                                       needs_key_update = true;     break;
        case Action::KillLineRight:    action_kill_line_right();
                                       needs_key_update = true;     break;

        case Action::CommitAndInsertBelow:
            action_commit_and_insert_below();
            completion_.hide();
            break;
        case Action::InsertAbove:      action_insert_above();
                                       completion_.hide();          break;
        case Action::DeleteRow:        action_delete_row();
                                       completion_.hide();          break;
        case Action::DeleteRowUp:      action_delete_row_up();
                                       completion_.hide();          break;
        case Action::MoveRowUp:        action_move_row(-1);
                                       completion_.hide();          break;
        case Action::MoveRowDown:      action_move_row(+1);
                                       completion_.hide();          break;

        case Action::Undo:             action_undo();
                                       completion_.hide();          break;
        case Action::Redo:             action_redo();
                                       completion_.hide();          break;
        case Action::Recalculate:      action_recalculate();
                                       completion_.hide();          break;
        case Action::ClearAll:         action_clear_all();
                                       completion_.hide();          break;
        case Action::CopyAll:          action_copy_all();
                                       completion_.hide();          break;
        case Action::DecimalsInc:      action_decimals_inc();       break;
        case Action::DecimalsDec:      action_decimals_dec();       break;

        case Action::FormatAuto:       action_format(FMT_REAL,      "");    break;
        case Action::FormatDec:        action_format(FMT_REAL,      "dec"); break;
        case Action::FormatHex:        action_format(FMT_HEX,       "hex"); break;
        case Action::FormatBin:        action_format(FMT_BIN,       "bin"); break;
        case Action::FormatSI:         action_format(FMT_SI_PREFIX, "si");  break;

        case Action::FileOpen:
            if (file_open_cb_) file_open_cb_();
            break;
        case Action::FileSave:
        case Action::FileSaveAs:
            if (file_save_cb_) file_save_cb_();
            break;

        case Action::CompletionTrigger:
            /* already handled above */
            break;
    }

    if (needs_key_update) completion_update_key();
    return true;
}

} // namespace calcyx::tui
