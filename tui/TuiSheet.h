#ifndef CALCYX_TUI_SHEET_H
#define CALCYX_TUI_SHEET_H

#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "TuiCompletion.h"
#include "sheet_model.h"

extern "C" {
#include "color_presets.h"
}

namespace calcyx::tui {

/* GUI 色を再現するときに参照するパレット。active=false の場合は
 * 従来のセマンティック (端末色 + 16 色基調) で描画する。
 * 各メンバは calcyx_color_palette_t の一部。 */
struct TuiPalette {
    bool active = false;
    /* シート描画用 */
    calcyx_rgb_t bg{}, sel_bg{}, text{}, accent{};
    calcyx_rgb_t symbol{}, ident{}, special{}, si_pfx{}, error{};
    calcyx_rgb_t paren[4]{};
    /* UI クローム (GUI の calcyx_color_palette_t.ui_* に対応):
     *   ui_menu=メニュー背景、 ui_bg=ダイアログ背景、
     *   ui_text=本文色、 ui_label=ラベル色 */
    calcyx_rgb_t ui_menu{}, ui_bg{}, ui_text{}, ui_label{};
};

/* sheet_model を FTXUI Component としてラップ。 フォーカス行のみ編集、
 * 行移動で flush + undo 登録、 編集中は live preview. */
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

    /* コピー完了・再計算・クリア等の短いフィードバックを TuiApp の flash_message に流す */
    void set_status_callback(std::function<void(std::string)> cb) { status_cb_ = std::move(cb); }

    /* 改行を含むテキストを Ctrl+V したときの "Paste Options" モーダル起動。
     * TuiApp が ↑↓/Enter のラジオを担当し、確定時に paste_multiline_*() を呼ぶ。
     * unset の場合は status_msg にスキップ表示する従来挙動になる。 */
    void set_multiline_paste_callback(std::function<void(std::string)> cb) {
        multiline_paste_cb_ = std::move(cb);
    }
    /* TuiApp のモーダル確定で呼ばれる。テキストを行で分割し、現在行の直下に
     * 各行を新規行として挿入する (sheet_model_commit を 1 回でアトミック)。 */
    void paste_multiline_as_rows  (const std::string &text);
    /* 改行を空白に置換してカーソル位置に挿入する。 */
    void paste_multiline_as_single(const std::string &text);

    /* 右クリックメニュー起動 (TuiApp 側に座標を渡す)。 unset なら無効。 */
    void set_context_menu_callback(std::function<void(int x, int y)> cb) {
        context_menu_cb_ = std::move(cb);
    }

    /* TuiApp のコンテキストメニューから呼ばれる行レベル操作。
     * 既存の private アクションを薄くラップしただけ (キーボード経路と
     * メニュー経路で同じ実装を共有する)。 */
    void copy_focused_row();
    void copy_focused_expr();
    void copy_focused_result();
    void cut_focused_row();
    void paste_at_cursor();
    void insert_row_above();
    void insert_row_below();
    void delete_focused_row();

    /* TuiApp から sheet_model 経由での I/O 完了後に呼ぶ */
    void reload_focused_row();

    /* ステータスバー表示用 */
    int   focused_row() const { return focused_row_; }
    bool  editor_dirty() const;
    const std::string& live_preview() const { return live_preview_; }
    const std::string& file_path()    const { return file_path_; }
    void  set_file_path(std::string p) { file_path_ = std::move(p); }

    /* サンプル等テンプレートとして開いたとき true. Ctrl+S は直書きせず
     * Save-As プロンプトに落ちる (誤って配布版を上書きしない)。
     * 保存後の false 復帰は TuiApp::prompt_submit の責務。 */
    bool  read_only() const { return read_only_; }
    void  set_read_only(bool v) { read_only_ = v; }

    /* コンパクトモード: シート行だけ表示し、status/help を隠す。
     * TuiApp は compact_mode() を見て下段のステータスバーも隠す。 */
    bool  compact_mode() const { return compact_mode_; }
    void  set_compact_mode(bool v) { compact_mode_ = v; }

    /* GUI の g_input_auto_completion 相当。View メニューから toggle。 */
    bool  auto_complete() const { return auto_complete_; }
    void  set_auto_complete(bool v) { auto_complete_ = v; }

    /* tui_color_source = mirror_gui のときのパレット。active=true で描画が
     * GUI と同じ RGB に切り替わる。デフォルトは inactive (= 従来挙動)。 */
    void set_palette(const TuiPalette &p) { palette_ = p; }
    const TuiPalette& palette() const { return palette_; }

    /* GUI の g_input_bs_delete_empty_row 相当。空行で BS を押すと
     * 行を削除して上に詰める挙動。誤削除を嫌うユーザーは calcyx.conf
     * で off にできる (Shift+BS / Ctrl+BS は影響を受けない)。 */
    bool  bs_delete_empty_row() const { return bs_delete_empty_row_; }
    void  set_bs_delete_empty_row(bool v) { bs_delete_empty_row_ = v; }

    /* --- テスト用アクセサ (本番コードからは使わない) --- */
    const std::string& test_editor_buf() const { return editor_buf_; }
    size_t             test_cursor_pos() const { return cursor_pos_; }
    bool               test_completion_visible() const { return completion_.visible(); }
    int                test_completion_count() const { return completion_.filtered_count(); }

private:
    /* --- 編集バッファ操作 --- */
    void load_editor_from_row();
    void commit_if_changed();
    void live_evaluate();

    /* --- 補完 --- */
    bool completion_visible() const { return completion_.visible(); }
    std::string current_word_at_cursor(size_t *out_start) const;
    void completion_trigger();
    void completion_update_key();
    void completion_auto_update();  /* GUI互換: 自動オープン or 更新 or 閉じる */
    void completion_confirm();

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
    void action_delete_row_up();
    void action_move_row(int dir);
    void action_undo();
    void action_redo();
    void action_recalculate();
    void action_clear_all();
    void action_copy_all();
    void action_copy();             /* 現在行を `expr = result` 形式でコピー */
    void action_cut();              /* コピーして行削除 */
    void action_paste();             /* クリップボードをカーソル位置に挿入 */
    void action_decimals_inc();
    void action_decimals_dec();
    void action_format(const char *fmt_func);

    /* --- undo helpers --- */
    sheet_view_state_t capture_view_state() const;
    void               restore_view_state(const sheet_view_state_t &vs);

    /* --- マウス --- */
    bool   handle_mouse(const ftxui::Mouse &m);
    /* UTF-8 文字列の指定セル列までの byte offset。日本語は cell 幅 2。 */
    static size_t byte_pos_for_cell(const std::string &s, int target_cell);
    /* UTF-8 文字列の表示セル幅。日本語は 2、ASCII は 1。 */
    static int    display_cells(const std::string &s);

    /* --- 描画 --- */
    ftxui::Element render_row(int idx, bool is_focused, int eq_col) const;
    /* シンタックスハイライト: GUI の draw_expr_highlighted と同じカテゴリ分け。
     * cursor_byte_pos が範囲内なら反転表示、末尾 (= size) なら末尾に反転スペース。
     * SIZE_MAX ならカーソルなし。dim_style=true でスパン全体に dim 属性を重ねる。 */
    ftxui::Element render_highlighted(const std::string &expr,
                                       size_t cursor_byte_pos,
                                       bool   dim_style) const;

    sheet_model_t *model_;
    int            focused_row_ = 0;
    std::string    editor_buf_;
    std::string    original_expr_;
    size_t         cursor_pos_ = 0;
    std::string    live_preview_;  /* 編集中の仮評価結果 */
    std::string    file_path_;     /* 保存先 (未保存なら空) */

    std::function<void()>               quit_cb_;
    std::function<void()>               file_open_cb_;
    std::function<void()>               file_save_cb_;
    std::function<void(std::string)>    status_cb_;
    std::function<void(std::string)>    multiline_paste_cb_;
    std::function<void(int, int)>       context_menu_cb_;

    TuiCompletion completion_;
    bool          auto_complete_       = true;
    bool          compact_mode_        = false;
    bool          read_only_           = false;
    bool          bs_delete_empty_row_ = true;
    TuiPalette    palette_;  /* active=false なら従来のセマンティック描画 */

    /* 直近 Ctrl+C のクリップボード内容 + 行の式部分。
     * Ctrl+V 時に完全一致なら同一プロセス由来とみなし式のみを挿入する
     * (他アプリ経由のラウンドトリップではフル expr=result を貼る)。 */
    std::string   last_copied_text_;
    std::string   last_copied_expr_;

    /* マウス: 各行と編集領域の Box (Render() で再構築)。 */
    mutable std::vector<ftxui::Box> row_boxes_;
    mutable ftxui::Box              editor_box_;
};

std::shared_ptr<TuiSheet> MakeTuiSheet(sheet_model_t *model);

} // namespace calcyx::tui

#endif
