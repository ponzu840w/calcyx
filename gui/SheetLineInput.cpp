#include "SheetLineInput.h"
#include "SheetView.h"
#include "CompletionPopup.h"
#include "sheet_highlight.h"
#include "colors.h"
#include "settings_globals.h"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <algorithm>
#include <cstring>
#include <vector>

SheetLineInput::SheetLineInput(int x, int y, int w, int h, bool editor_mode)
    : Fl_Input(x, y, w, h), editor_mode_(editor_mode), override_color_(0)
{
    if (!editor_mode_) readonly(1);
}

void SheetLineInput::draw() {
    fl_font(textfont(), textsize());

    const char *txt = value();
    int len = (int)strlen(txt);
    int p1 = std::min(insert_position(), mark());
    int p2 = std::max(insert_position(), mark());
    int sx = xscroll();
    int tx = x() + SHEET_ROW_PAD - sx;

    // セパレータシフト計算
    std::vector<double> shifts;
    if (editor_mode_)
        calc_expr_separator_shifts(txt, len, shifts);
    else
        calc_separator_shifts(txt, len, result_fmt_, shifts);

    // 背景
    fl_draw_box(FL_FLAT_BOX, x(), y(), w(), h(), g_colors.sel_bg);

    // 選択ハイライト背景 (フォーカスがある時のみ)
    if (p1 != p2 && Fl::focus() == this) {
        int sx1 = tx + (int)char_pos_to_x(txt, p1, shifts);
        int sx2 = tx + (int)char_pos_to_x(txt, p2, shifts);
        fl_push_clip(x(), y(), w(), h());
        fl_color(FL_SELECTION_COLOR);
        fl_rectf(sx1, y() + 1, sx2 - sx1, h() - 2);
        fl_pop_clip();
    }

    // テキスト描画
    if (override_color_ != (Fl_Color)0) {
        fl_push_clip(x(), y(), w(), h());
        fl_font(g_font_id, g_font_size);
        fl_color(override_color_);
        int baseline = y() + (h() + fl_height() - fl_descent() * 2) / 2;
        fl_draw(txt, tx, baseline);
        fl_pop_clip();
    } else {
        draw_expr_highlighted(txt, tx, x(), y(), w(), h(),
                              editor_mode_ ? (val_fmt_t)-1 : result_fmt_);
    }

    // カーソル (非選択時のみ)
    if (Fl::focus() == this && p1 == p2) {
        int cx = tx + (int)char_pos_to_x(txt, insert_position(), shifts);
        if (cx >= x() && cx < x() + w()) {
            fl_color(cursor_color());
            fl_line(cx, y() + 2, cx, y() + h() - 2);
        }
    }
}

int SheetLineInput::handle(int event) {
    if (event == FL_KEYBOARD) {
        int key   = Fl::event_key();
        bool ctrl  = Fl::event_state(FL_CTRL)  != 0;
        bool meta  = Fl::event_state(FL_COMMAND) != 0;
        bool shift = Fl::event_state(FL_SHIFT)  != 0;

        if (editor_mode_) {
            auto *sv = static_cast<SheetView *>(parent());
            // ポップアップが表示中: Up/Down/Tab/Escape を補完ナビに使う
            if (sv->popup_->is_shown()) {
                if (key == FL_Up)   { sv->popup_->select_prev(); return 1; }
                if (key == FL_Down) { sv->popup_->select_next(); return 1; }
                if (key == FL_Tab || key == FL_Enter || key == FL_KP_Enter)
                    { sv->completion_confirm(); return 1; }
                if (key == FL_Escape) { sv->completion_hide();   return 1; }
            }
            if (ctrl && key == ' ') {
                sv->completion_update();
                return 1;
            }
            // Tab: 左辺 → 右辺へフォーカス移動
            if (key == FL_Tab && !shift) {
                sv->completion_hide();
                sv->focus_result();
                return 1;
            }
            // Shift+Tab: 前行の右辺 (なければ左辺) へ (Tab の逆方向)
            if (key == FL_Tab && shift) {
                sv->completion_hide();
                sv->shift_tab_from_editor();
                return 1;
            }
        } else {
            // 右辺での Tab: 次行の左辺へ
            if (key == FL_Tab && !shift) {
                static_cast<SheetView *>(parent())->tab_from_result();
                return 1;
            }
            // 右辺での Shift+Tab: 同じ行の左辺へ (Tab の逆方向)
            if (key == FL_Tab && shift) {
                static_cast<SheetView *>(parent())->shift_tab_from_result();
                return 1;
            }
        }

        // Up/Down/Enter は左右どちらの欄からも SheetView::handle() に委譲
        if (key == FL_Up || key == FL_Down ||
            key == FL_Enter || key == FL_KP_Enter) {
            if (editor_mode_) static_cast<SheetView *>(parent())->completion_hide();
            return 0;
        }
        // Escape: ウィンドウが閉じないよう消費 (ポップアップなし時)
        if (key == FL_Escape) return 1;

        if (editor_mode_) {
            // Ctrl/Cmd+Delete/BackSpace: 行削除 → SheetView に委譲
            if ((ctrl || meta) && (key == FL_Delete || key == FL_BackSpace)) return 0;
            // Ctrl/Cmd+Shift+Up/Down: 行スワップ → SheetView に委譲
            if ((ctrl || meta) && shift && (key == FL_Up || key == FL_Down)) return 0;
            // Shift+Del/BS (修飾なし): 行削除・上移動 → SheetView に委譲
            if (shift && !ctrl && !meta && (key == FL_Delete || key == FL_BackSpace)) return 0;
            // 空行での BackSpace: 行削除・上移動 → SheetView に委譲
            // (Prefs で無効化されている場合は editor 側で吸収させる)
            if (g_input_bs_delete_empty_row &&
                !shift && !ctrl && !meta && key == FL_BackSpace && size() == 0)
                return 0;
        }
        // Cmd+Z / Cmd+Y / Cmd+Shift+Z は SheetView::handle() に委譲 (Undo/Redo)
        if (meta && (key == 'z' || key == 'y')) {
            return 0;
        }
    }
    // 括弧自動閉じ
    if (event == FL_KEYBOARD && editor_mode_ && g_input_auto_close_brackets) {
        const char *t = Fl::event_text();
        if (t && t[0] && !t[1]) {
            char close = 0;
            if (t[0] == '(') close = ')';
            else if (t[0] == '[') close = ']';
            else if (t[0] == '{') close = '}';
            if (close) {
                int pos = insert_position();
                char pair[3] = { t[0], close, '\0' };
                replace(pos, mark(), pair, 2);
                insert_position(pos + 1);
                if (parent()) static_cast<SheetView *>(parent())->live_eval();
                return 1;
            }
        }
    }

    // ペースト時: 改行なし→通常挿入、改行あり→複数行ペーストダイアログ
    if (event == FL_PASTE && editor_mode_) {
        const char *txt = Fl::event_text();
        int tlen = Fl::event_length();
        bool has_nl = false;
        for (int i = 0; i < tlen; i++) {
            if (txt[i] == '\n' || txt[i] == '\r') { has_nl = true; break; }
        }
        if (has_nl) {
            static_cast<SheetView *>(parent())->multiline_paste(
                std::string(txt, tlen));
        } else {
            replace(insert_position(), mark(), txt, tlen);
            if (parent()) static_cast<SheetView *>(parent())->live_eval();
        }
        return 1;
    }

    // マウスクリック/ドラッグ: セパレータシフトを考慮して文字位置を計算
    if (event == FL_PUSH || event == FL_DRAG) {
        const char *txt = value();
        int len = (int)strlen(txt);
        std::vector<double> shifts;
        if (editor_mode_)
            calc_expr_separator_shifts(txt, len, shifts);
        else
            calc_separator_shifts(txt, len, result_fmt_, shifts);
        if (!shifts.empty()) {
            fl_font(textfont(), textsize());
            int sx = xscroll();
            double click_x = Fl::event_x() - x() - SHEET_ROW_PAD + sx;
            int pos = x_to_char_pos(txt, len, click_x, shifts);
            if (event == FL_PUSH) {
                if (Fl::event_state(FL_SHIFT))
                    insert_position(pos, mark());
                else
                    insert_position(pos, pos);
                Fl::focus(this);
            } else {
                insert_position(pos, mark());
            }
            if (parent()) {
                parent()->redraw();
                if (editor_mode_)
                    static_cast<SheetView *>(parent())->completion_hide();
            }
            return 1;
        }
    }

    int ret = Fl_Input::handle(event);
    if (ret && parent()) {
        if (editor_mode_ && event == FL_KEYBOARD) {
            auto *sv = static_cast<SheetView *>(parent());
            sv->live_eval();
            if (g_input_auto_completion)
                sv->completion_update();
        }
        if (event == FL_PUSH || event == FL_DRAG || event == FL_RELEASE) {
            parent()->redraw();
            if (editor_mode_)
                static_cast<SheetView *>(parent())->completion_hide();
        }
    }
    return ret;
}
