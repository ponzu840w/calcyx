// calcyx メインウィンドウ
// 移植元: Calctus/UI/MainForm.cs (簡略版)

#pragma once

#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Button.H>
#include "SheetView.h"
#include "CompletionPopup.h"
#include <string>
#include <vector>
#include <unordered_map>

class DragGrip;     // コンパクトモード用 (MainWindow.cpp で定義)
class ResizeGrip;   // コンパクトモード用 (MainWindow.cpp で定義)

class MainWindow : public Fl_Double_Window {
public:
    MainWindow(int w, int h, const char *title);
    void open_file(const char *path);

    // フォーカス行変更時に Fl_Choice とツールバーボタンを更新
    void update_fmt_choice();
    void update_toolbar();
    void apply_ui_colors();
    void apply_tray_settings();  // PrefsDialog 変更後にトレイ再構築
    bool should_keep_running();  // トレイ常駐中は true、トレイ消失時は復帰
    void toggle_always_on_top(); // 起動時適用 + メニュー/ボタンから呼ばれる
    void toggle_compact_mode();  // コンパクトモードの切替

    int  handle(int event) override;
    void resize(int x, int y, int w, int h) override;
    void hide() override;
    void flush() override;

private:
    void save_prefs();
    Fl_Menu_Bar     *menu_;
    Fl_Button       *btn_undo_;   // ← ツールバー Undo
    Fl_Button       *btn_redo_;   // → ツールバー Redo
    Fl_Button       *btn_compact_; // ▣ コンパクトモード開始トグル
    Fl_Button       *btn_topmost_; // 📌 Always on Top トグル
    Fl_Choice       *fmt_choice_;
    SheetView       *sheet_;
    CompletionPopupBase *popup_;
    // popup_ を現在の設定・モードに合わせて再生成
    // (型が変わるときだけ生成・破棄; 変わらなければ何もしない)
    void recreate_popup_if_needed();
    DragGrip        *drag_grip_    = nullptr;  // コンパクトモード: ドラッグハンドル
    Fl_Button       *compact_exit_ = nullptr;  // コンパクトモード: 解除ボタン (PiP アイコン)
    ResizeGrip      *resize_grip_  = nullptr;  // コンパクトモード: リサイズハンドル
    /* コマンド ID → メニュー項目インデックス。 build_menu_index() が 1 度埋める。
     * find_index() がショートカット付きラベルで動かないため自前で走査。 */
    std::unordered_map<std::string, int> menu_indices_;
    int menu_idx(const char *cmd) const {
        auto it = menu_indices_.find(cmd);
        return (it != menu_indices_.end()) ? it->second : -1;
    }
    void build_menu_index();

    static const int MENU_H    = 24;
    static const int CHOICE_W  = 76;
    static const int BTN_W     = 22;   // ← → ボタン幅
    static const int PIN_W     = 22;   // 📌 ピンボタン幅
    static const int COMPACT_W = 22;   // ▣ コンパクトモード開始ボタン幅
    static const int PAD       = 4;
    static const int GRIP_SZ   = 18;   // コンパクトモード: ドラッグ/解除/リサイズハンドル一辺

    // メニューバー幅 = ウィンドウ幅 − 右側ウィジェット群
    static int calc_menu_w(int win_w) {
        return win_w - BTN_W * 2 - COMPACT_W - PIN_W - PAD - CHOICE_W;
    }

    static void menu_cb  (Fl_Widget *w, void *data);
    static void choice_cb(Fl_Widget *w, void *data);
    static void row_change_cb(void *data);  // SheetView からのコールバック
    void populate_samples_menu();
    static bool open_sample_file(MainWindow *win, const char *filename);
    void sync_view_menu_toggles();  // View メニューのチェック状態を g_ 変数に合わせる
    void apply_font_and_refresh();  // Zoom 系で apply_font + live_eval + redraw を一括
    void apply_size_range();        // compact_mode_ に応じて size_range を設定
    void setup_tray();
    void teardown_tray();
    void toggle_visibility();
    void show_and_activate();
    static void close_cb(Fl_Widget *w, void *data);
    static void hotkey_poll_cb(void *data);
    bool tray_initialized_ = false;  // setup_tray 初回実行済みフラグ

    bool topmost_ = false;
    bool tray_active_ = false;
    std::vector<std::string> sample_files_;  // メニュー文字列の安定した記憶域
    std::vector<std::string> scheme_cmds_;   // "scheme_N" コマンドの安定した記憶域

    // コンパクトモード状態
    bool compact_mode_   = false;
    bool saved_topmost_  = false;  // compact 突入前の topmost_ を保持
    int  saved_x_ = 0, saved_y_ = 0, saved_w_ = 0, saved_h_ = 0;

    // コンパクトモードのジオメトリは通常モードと別枠で永続化する。
    // 初回は未設定 (compact_geometry_valid_ = false) で、一度 compact に
    // 切替えると以降はその位置・サイズを覚える。
    bool compact_geometry_valid_ = false;
    int  compact_x_ = 0, compact_y_ = 0, compact_w_ = 0, compact_h_ = 0;
    void load_compact_geometry();
};
