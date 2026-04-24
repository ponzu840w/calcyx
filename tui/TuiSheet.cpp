#include "TuiSheet.h"

#include "keymap.h"

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

#include <algorithm>
#include <cstring>

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
    rows.reserve(n);

    /* 2 列レイアウト: 左 = expr、右 = result。
     * カーソル行が画面外に出ないよう yframe + focus でスクロール */
    for (int i = 0; i < n; ++i) {
        Element row = render_row(i, i == focused_row_, 0);
        if (i == focused_row_) row = row | focus;
        rows.push_back(row);
    }

    val_fmt_t cur_fmt = sheet_model_row_fmt(model_, focused_row_);
    std::string status =
        "[" + std::to_string(focused_row_ + 1) + "/" + std::to_string(n) + "]  "
        "fmt: " + fmt_label(cur_fmt);
    if (!file_path_.empty()) status += "  file: " + file_path_;
    if (editor_dirty())      status += "  *";

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
    Action a = map(ev);

    switch (a) {
        case Action::None:    return false;
        case Action::Quit:
            if (quit_cb_) quit_cb_();
            return true;

        case Action::CursorLeft:       action_cursor_left();        return true;
        case Action::CursorRight:      action_cursor_right();       return true;
        case Action::CursorHome:       action_cursor_home();        return true;
        case Action::CursorEnd:        action_cursor_end();         return true;
        case Action::CursorWordLeft:   action_cursor_word_left();   return true;
        case Action::CursorWordRight:  action_cursor_word_right();  return true;

        case Action::RowUp:            action_row_up();             return true;
        case Action::RowDown:          action_row_down();           return true;
        case Action::RowPageUp:        action_page(-1);             return true;
        case Action::RowPageDown:      action_page(+1);             return true;

        case Action::InsertChar:
            action_insert_char(ev.character());
            return true;
        case Action::Backspace:        action_backspace();          return true;
        case Action::DeleteChar:       action_delete_char();        return true;
        case Action::DeleteWord:       action_delete_word();        return true;
        case Action::KillLineRight:    action_kill_line_right();    return true;

        case Action::CommitAndInsertBelow: action_commit_and_insert_below(); return true;
        case Action::InsertAbove:      action_insert_above();       return true;
        case Action::DeleteRow:        action_delete_row();         return true;
        case Action::MoveRowUp:        action_move_row(-1);         return true;
        case Action::MoveRowDown:      action_move_row(+1);         return true;

        case Action::Undo:             action_undo();               return true;
        case Action::Redo:             action_redo();               return true;

        case Action::FormatAuto:       action_format(FMT_REAL,      "");    return true;
        case Action::FormatDec:        action_format(FMT_REAL,      "dec"); return true;
        case Action::FormatHex:        action_format(FMT_HEX,       "hex"); return true;
        case Action::FormatBin:        action_format(FMT_BIN,       "bin"); return true;
        case Action::FormatSI:         action_format(FMT_SI_PREFIX, "si");  return true;

        case Action::FileOpen:
            if (file_open_cb_) file_open_cb_();
            return true;
        case Action::FileSave:
            if (file_save_cb_) file_save_cb_();
            return true;
        case Action::FileSaveAs:
            if (file_save_cb_) file_save_cb_();
            return true;

        case Action::CompletionTrigger:
            /* v1: 補完は次コミットで実装 */
            return true;
    }
    return false;
}

} // namespace calcyx::tui
