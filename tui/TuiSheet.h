#ifndef CALCYX_TUI_SHEET_H
#define CALCYX_TUI_SHEET_H

#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <functional>
#include <memory>
#include <string>

#include "sheet_model.h"

namespace calcyx::tui {

/* sheet_model をラップして、FTXUI の Component として振る舞う。
 * フォーカス行のみを編集対象とし、矢印上下で別の行へ移動すると
 * 現在の編集を flush → undo 登録する。編集中 (原文と一致しない) は
 * live preview を行って結果を右側に表示する。 */
class TuiSheet : public ftxui::ComponentBase {
public:
    explicit TuiSheet(sheet_model_t *model);
    ~TuiSheet() override;

    ftxui::Element Render() override;
    bool           OnEvent(ftxui::Event ev) override;
    bool           Focusable() const override { return true; }

    /* Ctrl+Q 等。TuiApp が Screen::Exit() を渡す */
    void set_quit_callback(std::function<void()> cb) { quit_cb_ = std::move(cb); }

    /* Ctrl+O / Ctrl+S 時のパス入力プロンプトを TuiApp が担当する */
    void set_file_open_callback (std::function<void()> cb) { file_open_cb_ = std::move(cb); }
    void set_file_save_callback (std::function<void()> cb) { file_save_cb_ = std::move(cb); }

    /* TuiApp から sheet_model 経由での I/O 完了後に呼ぶ */
    void reload_focused_row();

    /* ステータスバー表示用 */
    int   focused_row() const { return focused_row_; }
    bool  editor_dirty() const;
    const std::string& live_preview() const { return live_preview_; }
    const std::string& file_path()    const { return file_path_; }
    void  set_file_path(std::string p) { file_path_ = std::move(p); }

private:
    /* --- 編集バッファ操作 --- */
    void load_editor_from_row();
    void commit_if_changed();
    void live_evaluate();

    /* --- キーアクション実装 --- */
    void action_cursor_left();
    void action_cursor_right();
    void action_cursor_home();
    void action_cursor_end();
    void action_cursor_word_left();
    void action_cursor_word_right();
    void action_row_up();
    void action_row_down();
    void action_page(int dir);  /* -1 / +1 */
    void action_insert_char(const std::string &s);
    void action_backspace();
    void action_delete_char();
    void action_delete_word();
    void action_kill_line_right();
    void action_commit_and_insert_below();
    void action_insert_above();
    void action_delete_row();
    void action_move_row(int dir);
    void action_undo();
    void action_redo();
    void action_format(val_fmt_t fmt, const char *fmt_func);

    /* --- undo helpers --- */
    sheet_view_state_t capture_view_state() const;
    void               restore_view_state(const sheet_view_state_t &vs);

    /* --- 描画 --- */
    ftxui::Element render_row(int idx, bool is_focused, int result_col) const;

    sheet_model_t *model_;
    int            focused_row_ = 0;
    std::string    editor_buf_;
    std::string    original_expr_;
    size_t         cursor_pos_ = 0;
    std::string    live_preview_;  /* 編集中の仮評価結果 */
    std::string    file_path_;     /* 保存先 (未保存なら空) */

    std::function<void()> quit_cb_;
    std::function<void()> file_open_cb_;
    std::function<void()> file_save_cb_;
};

std::shared_ptr<TuiSheet> MakeTuiSheet(sheet_model_t *model);

} // namespace calcyx::tui

#endif
