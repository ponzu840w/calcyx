#include "TuiSheet.h"

#include "keymap.h"
#include "clipboard.h"

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/string.hpp>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
#include "parser/lexer.h"
#include "parser/token.h"
#include "types/val.h"
}

#include <cctype>

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

/* 1 文字ごとのハイライト色を組み立てる。範囲外 (TOK_EMPTY 等) は
 * Color::Default を残す → 端末デフォルト前景でそのまま描画される。
 * 移植元: ui/SheetView.cpp:220 draw_expr_highlighted。 */
std::vector<ftxui::Color> build_char_colors(const std::string &expr) {
    using ftxui::Color;
    std::vector<Color> out(expr.size(), Color::Default);
    if (expr.empty()) return out;

    tok_queue_t q;
    tok_queue_init(&q);
    lexer_tokenize(expr.c_str(), &q);

    /* 括弧ネスト色は GUI の g_colors.paren 4 色を FTXUI 色に置換。
     * ネストごとに色を変えて対応関係を目で追いやすくする。 */
    static const Color paren_colors[4] = {
        Color::YellowLight, Color::MagentaLight,
        Color::CyanLight,   Color::GreenLight,
    };
    int paren_depth = 0;

    auto paint = [&](int p, int end, Color c) {
        int n = (int)expr.size();
        if (p < 0) p = 0;
        if (end > n) end = n;
        for (int i = p; i < end; ++i) out[i] = c;
    };

    for (;;) {
        const token_t *peek = tok_queue_peek(&q);
        if (!peek || peek->type == TOK_EOS || peek->type == TOK_EMPTY) break;
        token_t tok = tok_queue_pop(&q);

        int p  = tok.pos;
        int tl = (int)std::strlen(tok.text);
        int end = p + tl;
        if (p < 0 || p >= (int)expr.size()) { tok_free(&tok); continue; }
        if (end > (int)expr.size()) end = (int)expr.size();

        switch (tok.type) {
            case TOK_WORD:
                paint(p, end, Color::CyanLight);
                break;
            case TOK_BOOL_LIT:
                paint(p, end, Color::MagentaLight);
                break;
            case TOK_NUM_LIT:
                if (tok.val) {
                    val_fmt_t vfmt = tok.val->fmt;
                    if (vfmt == FMT_SI_PREFIX) {
                        if (end - 1 >= p) paint(end - 1, end, Color::YellowLight);
                    } else if (vfmt == FMT_BIN_PREFIX) {
                        if (end - 2 >= p) paint(end - 2, end, Color::YellowLight);
                    } else if (vfmt == FMT_CHAR || vfmt == FMT_STRING ||
                               vfmt == FMT_DATETIME || vfmt == FMT_WEB_COLOR) {
                        paint(p, end, Color::MagentaLight);
                    } else {
                        /* 数値リテラル本体は既定色のまま。指数 (e/E 以降) だけ
                         * SI プレフィクスと同じ黄色で強調する。 */
                        for (int i = p + 1; i < end; ++i) {
                            if ((expr[i] == 'e' || expr[i] == 'E') &&
                                std::isdigit((unsigned char)expr[i-1])) {
                                paint(i, end, Color::YellowLight);
                                break;
                            }
                        }
                    }
                }
                break;
            case TOK_OP:
            case TOK_KEYWORD:
                paint(p, end, Color::RedLight);
                break;
            case TOK_SYMBOL:
                if (tl == 1 && (tok.text[0] == '(' || tok.text[0] == ')')) {
                    int d = paren_depth;
                    if (tok.text[0] == ')' && d > 0) d--;
                    paint(p, end, paren_colors[d % 4]);
                    if (tok.text[0] == '(') paren_depth++;
                    else if (paren_depth > 0) paren_depth--;
                } else {
                    paint(p, end, Color::RedLight);
                }
                break;
            default:
                break;
        }
        tok_free(&tok);
    }
    tok_queue_free(&q);
    return out;
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

/* GUI の SheetView::completion_update (ui/SheetView.cpp:1396) と同じ挙動。
 * auto_complete_ が on の場合、毎キー入力後に呼んで
 *   - カーソル左が識別子の続き (id_follow) でなければ閉じる
 *   - 識別子の先頭が id_start でなければ閉じる
 *   - 未表示なら候補をリロードして開く
 *   - 表示中ならキーを更新
 * という形で自動的に追従させる。 */
void TuiSheet::completion_auto_update() {
    if (cursor_pos_ == 0 || !lexer_is_id_follow(editor_buf_[cursor_pos_ - 1])) {
        completion_.hide();
        return;
    }
    size_t start = cursor_pos_;
    while (start > 0 && lexer_is_id_follow(editor_buf_[start - 1])) --start;
    if (!lexer_is_id_start(editor_buf_[start])) {
        completion_.hide();
        return;
    }
    std::string key = editor_buf_.substr(start, cursor_pos_ - start);
    if (!completion_.visible()) {
        completion_.reload(model_);
        completion_.open(key);
    } else {
        completion_.update_key(key);
    }
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
    /* 未コミット編集があれば先に commit してから undo を呼ぶ。
     * これで「行に typing → Ctrl+Z で取り消し → Ctrl+Y で復元」が成立する:
     *   1) commit_if_changed() が [original→typed] を undo スタックに積む
     *   2) sheet_model_undo() がそれを pop して typed を redo スタックに残す
     *   3) restore_view_state() で editor バッファが original に戻る */
    commit_if_changed();
    sheet_view_state_t vs{};
    if (sheet_model_undo(model_, &vs)) {
        restore_view_state(vs);
    }
}
void TuiSheet::action_redo() {
    /* 未コミット編集を先に commit (commit 内で redo スタックが truncate されるので、
     * 直後の sheet_model_redo は no-op になる)。これで「typing 中に Ctrl+Y を
     * 押したら typing が黙って消える」事故を防ぐ。 */
    commit_if_changed();
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

/* 全コピー: clipboard::write() に委譲。pbcopy / wl-copy / xclip / clip.exe
 * のフォールバック → 最後に OSC 52 を試す。
 * 書式: "<expr> = <result>\n" を全行結合。移植元 ui/SheetView.cpp:1591。 */
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
    bool ok = clipboard::write(text);
    if (status_cb_) {
        if (ok)
            status_cb_("Copied " + std::to_string(n) + " row(s) to clipboard");
        else
            status_cb_("Clipboard write failed");
    }
}

/* 単一行コピー: 編集中なら commit してから現在行を `expr = result` 形式で
 * クリップボードへ書き込む。同時に last_copied_text_ / last_copied_expr_ を
 * 更新し、直後の Ctrl+V で「式のみ」を貼り付けられるようにする。 */
void TuiSheet::action_copy() {
    commit_if_changed();
    int n = sheet_model_row_count(model_);
    if (focused_row_ < 0 || focused_row_ >= n) return;

    const char *expr_p = sheet_model_row_expr  (model_, focused_row_);
    const char *res_p  = sheet_model_row_result(model_, focused_row_);
    bool        vis    = sheet_model_row_visible(model_, focused_row_);

    std::string expr = expr_p ? expr_p : "";
    std::string text = expr;
    if (res_p && res_p[0] && vis) {
        text += " = ";
        text += res_p;
    }

    if (!clipboard::write(text)) {
        if (status_cb_) status_cb_("Clipboard write failed");
        return;
    }
    last_copied_text_ = text;
    last_copied_expr_ = expr;
    if (status_cb_) status_cb_("Copied row to clipboard");
}

/* Cut: コピーしてから現在行を削除。 */
void TuiSheet::action_cut() {
    int n = sheet_model_row_count(model_);
    if (focused_row_ < 0 || focused_row_ >= n) return;

    /* action_copy() の status_cb_ は cut の status で上書きするので、
     * コピー失敗時のみフィードバックを返す。 */
    commit_if_changed();
    const char *expr_p = sheet_model_row_expr  (model_, focused_row_);
    const char *res_p  = sheet_model_row_result(model_, focused_row_);
    bool        vis    = sheet_model_row_visible(model_, focused_row_);
    std::string expr = expr_p ? expr_p : "";
    std::string text = expr;
    if (res_p && res_p[0] && vis) { text += " = "; text += res_p; }

    if (!clipboard::write(text)) {
        if (status_cb_) status_cb_("Clipboard write failed; row not deleted");
        return;
    }
    last_copied_text_ = text;
    last_copied_expr_ = expr;

    action_delete_row();
    if (status_cb_) status_cb_("Cut row to clipboard");
}

/* Paste: クリップボードから読み込み、内容が直前の Ctrl+C/X と一致すれば
 * 式部分のみを、そうでなければそのままを現在のカーソル位置に挿入する。
 * 改行を含む場合はマルチライン挿入処理 (将来モーダル) — 現状は status のみ。 */
void TuiSheet::action_paste() {
    std::string text;
    if (!clipboard::read(text)) {
        if (status_cb_) status_cb_("Clipboard read failed");
        return;
    }
    /* コマンド出力末尾の単一改行はおまけなので削る。複数改行は保持。 */
    if (!text.empty() && text.back() == '\n') text.pop_back();
    if (!text.empty() && text.back() == '\r') text.pop_back();

    bool from_self = !last_copied_text_.empty() && text == last_copied_text_;
    std::string to_insert = from_self ? last_copied_expr_ : text;

    if (to_insert.find('\n') != std::string::npos ||
        to_insert.find('\r') != std::string::npos) {
        /* マルチラインは TuiApp 側のモーダルへ委譲。コールバック未設定の
         * テスト等ではフィードバックのみ。 */
        if (multiline_paste_cb_) {
            multiline_paste_cb_(to_insert);
        } else if (status_cb_) {
            status_cb_("Multi-line paste skipped (modal unavailable)");
        }
        return;
    }

    editor_buf_.insert(cursor_pos_, to_insert);
    cursor_pos_ += to_insert.size();
    live_evaluate();
    if (status_cb_)
        status_cb_(from_self ? "Pasted expression" : "Pasted from clipboard");
}

namespace {
/* \r\n / \n / \r どれでも 1 行として扱って分割。末尾空行はドロップする
 * (コピー由来の trailing newline は 1 つ削ってあるが念のため)。 */
std::vector<std::string> split_lines(const std::string &text) {
    std::vector<std::string> lines;
    std::string line;
    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (c == '\r') {
            lines.push_back(line); line.clear();
            if (i + 1 < text.size() && text[i + 1] == '\n') ++i;
        } else if (c == '\n') {
            lines.push_back(line); line.clear();
        } else {
            line += c;
        }
    }
    if (!line.empty()) lines.push_back(line);
    while (!lines.empty() && lines.back().empty()) lines.pop_back();
    return lines;
}
} // namespace

/* GUI の SheetView::multiline_paste 相当。現在行の直下に各行を 1 行ずつ
 * 挿入し、まとめて 1 つの undo ステップとして commit する。 */
void TuiSheet::paste_multiline_as_rows(const std::string &text) {
    auto lines = split_lines(text);
    if (lines.empty()) return;

    commit_if_changed();
    int insert_at = focused_row_ + 1;

    std::vector<sheet_op_t> redo_ops;
    redo_ops.reserve(lines.size());
    for (size_t i = 0; i < lines.size(); ++i) {
        sheet_op_t op{};
        op.type  = SHEET_OP_INSERT;
        op.index = (int)(insert_at + i);
        op.expr  = lines[i].c_str();
        redo_ops.push_back(op);
    }
    std::vector<sheet_op_t> undo_ops;
    undo_ops.reserve(lines.size());
    for (size_t i = 0; i < lines.size(); ++i) {
        sheet_op_t op{};
        op.type  = SHEET_OP_DELETE;
        op.index = (int)(insert_at + (lines.size() - 1 - i));
        op.expr  = nullptr;
        undo_ops.push_back(op);
    }

    sheet_model_commit(model_,
                       undo_ops.data(), (int)undo_ops.size(),
                       redo_ops.data(), (int)redo_ops.size(),
                       capture_view_state());

    /* 挿入された最後の行にフォーカスを移す。 */
    focused_row_ = insert_at + (int)lines.size() - 1;
    load_editor_from_row();

    if (status_cb_)
        status_cb_("Pasted " + std::to_string(lines.size()) + " row(s)");
}

/* 改行を空白に変換して現在のカーソル位置に挿入。 */
void TuiSheet::paste_multiline_as_single(const std::string &text) {
    std::string flat;
    flat.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (c == '\r' || c == '\n') {
            if (!flat.empty() && flat.back() != ' ') flat.push_back(' ');
            if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n') ++i;
        } else {
            flat.push_back(c);
        }
    }
    /* 末尾の空白は削る */
    while (!flat.empty() && flat.back() == ' ') flat.pop_back();

    editor_buf_.insert(cursor_pos_, flat);
    cursor_pos_ += flat.size();
    live_evaluate();

    if (status_cb_) status_cb_("Pasted as single line");
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
Element TuiSheet::render_highlighted(const std::string &expr,
                                      size_t cursor_byte_pos,
                                      bool   dim_style) const {
    auto colors = build_char_colors(expr);
    int  n      = (int)expr.size();

    Elements els;
    els.reserve(n + 2);

    std::string buf;
    Color buf_color = Color::Default;
    auto flush = [&] {
        if (buf.empty()) return;
        Element e = text(buf);
        if (!(buf_color == Color::Default)) e = e | color(buf_color);
        if (dim_style) e = e | dim;
        els.push_back(e);
        buf.clear();
    };

    for (int i = 0; i < n; ++i) {
        if ((size_t)i == cursor_byte_pos) {
            flush();
            Element e = text(std::string(1, expr[i]));
            if (!(colors[i] == Color::Default)) e = e | color(colors[i]);
            e = e | inverted;
            els.push_back(e);
            continue;
        }
        if (buf.empty()) {
            buf_color = colors[i];
            buf.push_back(expr[i]);
        } else if (colors[i] == buf_color) {
            buf.push_back(expr[i]);
        } else {
            flush();
            buf_color = colors[i];
            buf.push_back(expr[i]);
        }
    }
    flush();
    /* カーソルが末尾 (= size) の場合はトレイルスペースを反転して見せる */
    if (cursor_byte_pos == (size_t)n) {
        els.push_back(text(" ") | inverted);
    }
    return hbox(std::move(els));
}

Element TuiSheet::render_row(int idx, bool is_focused, int eq_col) const {
    const char *expr_c  = sheet_model_row_expr(model_, idx);
    const char *result_c = sheet_model_row_result(model_, idx);
    std::string expr    = expr_c   ? expr_c   : "";
    std::string result  = result_c ? result_c : "";

    /* 左カラム: "> editor" or "  expr" を eq_col セル幅に揃える。
     * フォーカス行でカーソルが末尾にある場合、render_highlighted が末尾に
     * 反転スペース 1 セルを追加するので、その分も幅に含める。 */
    const std::string &left_src = is_focused ? editor_buf_ : expr;
    int left_w = display_cells(left_src);  /* "> " の 2 セルは別途 */
    if (is_focused && cursor_pos_ == editor_buf_.size()) left_w += 1;
    int pad    = (eq_col > left_w) ? (eq_col - left_w) : 0;

    Element expr_el;
    if (is_focused) {
        expr_el = render_highlighted(editor_buf_, cursor_pos_, /*dim_style=*/false) |
                  reflect(editor_box_);
    } else {
        expr_el = render_highlighted(expr, SIZE_MAX, /*dim_style=*/true);
    }
    Element left = hbox({
        text(is_focused ? "> " : "  "),
        expr_el,
        text(std::string(pad, ' ')),
    });

    /* 中央 "=": 結果がある行のみ表示。 */
    bool has_result = false;
    std::string res_text;
    if (is_focused && editor_dirty()) {
        if (!live_preview_.empty()) { has_result = true; res_text = live_preview_; }
    } else if (sheet_model_row_visible(model_, idx)) {
        has_result = true; res_text = result;
    }

    Element eq = text(has_result ? " = " : "   ");

    /* 結果のシンタックスハイライト:
     *   - error 行: 赤一色 (per-token 色は無視)
     *   - dirty (preview): 黄色一色 (未コミットを示す)
     *   - それ以外: render_highlighted で式と同じトークンベース色付け */
    Element right;
    if (sheet_model_row_error(model_, idx)) {
        right = text(res_text) | color(Color::Red);
    } else if (is_focused && editor_dirty()) {
        right = text(res_text) | color(Color::Yellow);
    } else if (has_result) {
        right = render_highlighted(res_text, SIZE_MAX, /*dim_style=*/false);
    } else {
        right = text("");
    }

    Element row = hbox({
        left,
        eq,
        right | flex,
    });
    if (is_focused) row = row | bgcolor(Color::Blue);
    return row;
}

Element TuiSheet::Render() {
    int n = sheet_model_row_count(model_);
    Elements rows;
    rows.reserve(n + 2);

    /* マウス対応: 行ごとの Box をリセット。reflect で埋まる。 */
    row_boxes_.assign(n, Box{});
    editor_box_ = Box{};

    /* eq_col_: "=" カラムの位置を全行で揃える。
     * 各行の expr (フォーカス行は editor_buf_) のセル幅の最大値。
     * フォーカス行のカーソルが末尾にあるときは反転スペース 1 セル分を加味。
     * 上限は 60 セル (画面幅 80 を想定して 3/4 程度)。極端な式は溢れさせる。 */
    int eq_col = 0;
    for (int i = 0; i < n; ++i) {
        std::string s;
        if (i == focused_row_) {
            s = editor_buf_;
        } else {
            const char *e = sheet_model_row_expr(model_, i);
            s = e ? e : "";
        }
        int w = display_cells(s);
        if (i == focused_row_ && cursor_pos_ == editor_buf_.size()) w += 1;
        if (w > eq_col) eq_col = w;
    }
    if (eq_col > 60) eq_col = 60;

    /* 2 列レイアウト: 左 = expr (eq_col 幅に padding)、右 = result。
     * カーソル行が画面外に出ないよう yframe + focus でスクロール。
     * 補完ポップアップは焦点行の直下に挟み込む。 */
    for (int i = 0; i < n; ++i) {
        Element row = render_row(i, i == focused_row_, eq_col);
        if (i == focused_row_) row = row | focus;
        row = row | reflect(row_boxes_[i]);
        rows.push_back(row);
        if (i == focused_row_ && completion_visible()) {
            Element popup = hbox({
                text("  "),
                completion_.render(8) | size(WIDTH, LESS_THAN, 40),
            });
            rows.push_back(popup);
        }
    }

    /* compact_mode_ ではシート行のみ表示。status/help/separator は省略。 */
    if (compact_mode_) {
        return vbox({ vbox(std::move(rows)) | yframe | flex });
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

    std::string help = " F1 help  Alt+F menu  ^Q quit  ^Z/^Y undo/redo  "
                       "Tab compl.  F5 recalc  F6 compact  F8-F12 fmt ";

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
 * マウス
 * -------------------------------------------------------------------- */
size_t TuiSheet::byte_pos_for_cell(const std::string &s, int target_cell) {
    if (target_cell <= 0) return 0;
    auto glyphs = Utf8ToGlyphs(s);
    size_t byte = 0;
    int    cell = 0;
    for (const auto &g : glyphs) {
        if (g.empty()) continue;  /* 全角文字の継続セル */
        int w = string_width(g);
        if (cell + w > target_cell) break;
        byte += g.size();
        cell += w;
    }
    return byte;
}

int TuiSheet::display_cells(const std::string &s) {
    auto glyphs = Utf8ToGlyphs(s);
    int n = 0;
    for (const auto &g : glyphs) {
        if (g.empty()) continue;
        n += string_width(g);
    }
    return n;
}

bool TuiSheet::handle_mouse(const Mouse &m) {
    /* ホイール: 行単位スクロール。常に処理。 */
    if (m.button == Mouse::WheelUp) {
        action_row_up();
        completion_.hide();
        return true;
    }
    if (m.button == Mouse::WheelDown) {
        action_row_down();
        completion_.hide();
        return true;
    }

    /* 右クリック (Pressed のみ): 行 hit-test してフォーカス行を移動し、
     * TuiApp 側のコンテキストメニューを開く。 unset なら無効。 */
    if (m.button == Mouse::Right) {
        if (m.motion != Mouse::Pressed) return true;
        if (!context_menu_cb_) return false;
        for (int i = 0; i < (int)row_boxes_.size(); ++i) {
            if (!row_boxes_[i].Contain(m.x, m.y)) continue;
            if (i != focused_row_) {
                commit_if_changed();
                focused_row_ = i;
                load_editor_from_row();
            }
            completion_.hide();
            context_menu_cb_(m.x, m.y);
            return true;
        }
        return false;
    }

    /* 左クリック (Pressed のみ反応、Released は吸収するだけ) */
    if (m.button != Mouse::Left) return false;
    if (m.motion != Mouse::Pressed) return true;

    /* 補完ポップアップ内のクリック: 該当項目を選択して確定 */
    if (completion_visible()) {
        int idx = completion_.item_at(m.x, m.y);
        if (idx >= 0) {
            completion_.set_selected(idx);
            completion_confirm();
            return true;
        }
        /* 補完外をクリックしたら閉じる (確定はしない) */
        completion_.hide();
        /* fall through: 行 hit-test も試す */
    }

    /* 行 hit-test */
    for (int i = 0; i < (int)row_boxes_.size(); ++i) {
        if (!row_boxes_[i].Contain(m.x, m.y)) continue;
        if (i != focused_row_) {
            commit_if_changed();
            focused_row_ = i;
            load_editor_from_row();
            completion_.hide();
            return true;
        }
        /* 既にフォーカス行 — 編集領域内ならカーソル位置設定 */
        if (editor_box_.Contain(m.x, m.y)) {
            int target_cell = m.x - editor_box_.x_min;
            cursor_pos_ = byte_pos_for_cell(editor_buf_, target_cell);
        } else if (m.x < editor_box_.x_min) {
            cursor_pos_ = 0;
        }
        return true;
    }

    return false;
}

/* ----------------------------------------------------------------------
 * OnEvent
 * -------------------------------------------------------------------- */
bool TuiSheet::OnEvent(Event ev) {
    if (ev.is_mouse()) return handle_mouse(ev.mouse());

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
        case Action::Copy:             action_copy();
                                       completion_.hide();          break;
        case Action::Cut:              action_cut();
                                       completion_.hide();          break;
        case Action::Paste:            action_paste();
                                       needs_key_update = true;
                                       completion_.hide();          break;
        case Action::DecimalsInc:      action_decimals_inc();       break;
        case Action::DecimalsDec:      action_decimals_dec();       break;
        case Action::ToggleCompact:
            compact_mode_ = !compact_mode_;
            if (status_cb_)
                status_cb_(compact_mode_ ? "Compact mode on" : "Compact mode off");
            break;

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

    if (needs_key_update) {
        /* GUI 互換: auto_complete が有効なら毎キー入力で自動オープン/更新。
         * 無効なら popup が既に開いているときだけキーを追従。 */
        if (auto_complete_) completion_auto_update();
        else                completion_update_key();
    }
    return true;
}

/* ----------------------------------------------------------------------
 * コンテキストメニュー用のパブリック API。
 * 既存の private アクションを薄くラップしただけで、キーボード経路と
 * メニュー経路で同じ実装を共有する。
 * -------------------------------------------------------------------- */
void TuiSheet::copy_focused_row()    { action_copy(); }
void TuiSheet::cut_focused_row()     { action_cut(); }
void TuiSheet::paste_at_cursor()     { action_paste(); }
void TuiSheet::insert_row_above()    { action_insert_above(); }
void TuiSheet::insert_row_below()    { action_commit_and_insert_below(); }
void TuiSheet::delete_focused_row()  { action_delete_row(); }

void TuiSheet::copy_focused_expr() {
    commit_if_changed();
    int idx = focused_row_;
    int n   = sheet_model_row_count(model_);
    if (idx < 0 || idx >= n) return;
    const char *expr = sheet_model_row_expr(model_, idx);
    std::string text = expr ? expr : "";
    clipboard::write(text);
    last_copied_text_ = text;
    last_copied_expr_ = text;
    if (status_cb_) status_cb_("copied expression");
}

void TuiSheet::copy_focused_result() {
    commit_if_changed();
    int idx = focused_row_;
    int n   = sheet_model_row_count(model_);
    if (idx < 0 || idx >= n) return;
    const char *res = sheet_model_row_result(model_, idx);
    std::string text = res ? res : "";
    clipboard::write(text);
    /* 結果コピーは式と一致しないので、Ctrl+V のラウンドトリップ短縮は無効化。 */
    last_copied_text_.clear();
    last_copied_expr_.clear();
    if (status_cb_) status_cb_("copied result");
}

} // namespace calcyx::tui
