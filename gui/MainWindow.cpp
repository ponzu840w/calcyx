// 移植元: Calctus/UI/MainForm.cs (簡略版)

#include "MainWindow.h"
#include "PrefsDialog.h"
#include "settings_globals.h"
#include "platform_tray.h"
#include "colors.h"
#include <FL/Fl.H>
#include <FL/Fl_Native_File_Chooser.H>
#include "app_prefs.h"
#include <FL/fl_ask.H>
#include <FL/Fl_SVG_Image.H>
#include <FL/Fl_Help_View.H>
#include <FL/fl_utf8.h>
#include <FL/Fl_Box.H>
#include <FL/fl_draw.H>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <sys/stat.h>
#include <dirent.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <libgen.h>
extern "C" void mac_set_window_level(Fl_Window *win, int topmost);
#endif
#ifdef _WIN32
#include <FL/platform.H>
#include <windows.h>
#elif !defined(__APPLE__)
#include <FL/platform.H>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <unistd.h>
#endif

// フォーマット定義 (移植元: RepresentaionFuncs.cs)
static const struct { const char *label; const char *func_name; } FMT_DEFS[] = {
    { "Auto",  nullptr  },
    { "Dec",   "dec"    },
    { "Hex",   "hex"    },
    { "Bin",   "bin"    },
    { "Oct",   "oct"    },
    { "SI",    "si"     },
    { "Kibi",  "kibi"   },
    { "Char",  "char"   },
};
static const int FMT_COUNT = 8;

static std::string find_icon_svg();

// UI クロームカラーは g_colors.ui_* から取得
#define C_WIN_BG   g_colors.ui_win_bg
#define C_MENU_BG  g_colors.ui_menu
#define C_MENU_FG  g_colors.ui_text

// コンパクトモードのアイコン色: labelcolor と直下のシート背景色の
// 50:50 ブレンド。FLTK 1.x は widget 単位の α 合成を持たないので、
// 真の半透明ではなく「薄めた色」で擬似的にシート数字が透けて見える
// ように見せる。アイコンのフィルは極力使わず枠線だけに留めるので、
// 数字のストロークは主にアイコンの隙間から視認できる。
static Fl_Color compact_overlay_color(Fl_Color fg) {
    return fl_color_average(fg, g_colors.bg, 0.5f);
}

// コンパクトモードのドラッグハンドル。3x3 ドット柄を描画し、
// FL_PUSH / FL_DRAG でウィンドウを追従移動させる。
class DragGrip : public Fl_Box {
public:
    DragGrip(int x, int y, int w, int h) : Fl_Box(x, y, w, h) {
        box(FL_NO_BOX);  // 背景を塗らず、アイコンのみ描く (半透明風)
    }
    void draw() override {
        Fl_Box::draw();
        fl_color(compact_overlay_color(labelcolor()));
        int cx = x() + w() / 2, cy = y() + h() / 2;
        int d = 2;   // ドット径
        int g = 4;   // ドット間隔
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                int px = cx + dx * g - d / 2;
                int py = cy + dy * g - d / 2;
                fl_rectf(px, py, d, d);
            }
        }
    }
    int handle(int e) override {
        switch (e) {
            case FL_ENTER:
                fl_cursor(FL_CURSOR_MOVE);
                return 1;
            case FL_LEAVE:
                fl_cursor(FL_CURSOR_DEFAULT);
                return 1;
            case FL_PUSH:
                drag_dx_ = Fl::event_x_root() - window()->x_root();
                drag_dy_ = Fl::event_y_root() - window()->y_root();
                return 1;
            case FL_DRAG:
                window()->position(Fl::event_x_root() - drag_dx_,
                                   Fl::event_y_root() - drag_dy_);
                return 1;
        }
        return Fl_Box::handle(e);
    }
private:
    int drag_dx_ = 0, drag_dy_ = 0;
};

// コンパクトモードの右下コーナーに置くリサイズハンドル。
// 右下向き斜め矢印 (↘) を描画し、ドラッグでウィンドウサイズを変える。
class ResizeGrip : public Fl_Box {
public:
    ResizeGrip(int x, int y, int w, int h) : Fl_Box(x, y, w, h) {
        box(FL_NO_BOX);  // 背景を塗らず、矢印のみ描く (半透明風)
    }
    void draw() override {
        Fl_Box::draw();
        fl_color(compact_overlay_color(labelcolor()));
        int pad = 4;
        int x0 = x() + pad;
        int y0 = y() + pad;
        int x1 = x() + w() - 1 - pad;
        int y1 = y() + h() - 1 - pad;
        fl_line_style(FL_SOLID, 2);
        fl_line(x0, y0, x1, y1);   // 斜めの軸
        int hlen = 6;
        fl_line(x1, y1, x1 - hlen, y1);  // 矢じり: 左へ
        fl_line(x1, y1, x1, y1 - hlen);  // 矢じり: 上へ
        fl_line_style(0);
    }
    int handle(int e) override {
        switch (e) {
            case FL_ENTER:
                fl_cursor(FL_CURSOR_NWSE);
                return 1;
            case FL_LEAVE:
                fl_cursor(FL_CURSOR_DEFAULT);
                return 1;
            case FL_PUSH:
                start_w_  = window()->w();
                start_h_  = window()->h();
                start_mx_ = Fl::event_x_root();
                start_my_ = Fl::event_y_root();
                return 1;
            case FL_DRAG: {
                int nw = start_w_ + (Fl::event_x_root() - start_mx_);
                int nh = start_h_ + (Fl::event_y_root() - start_my_);
                const int MIN_W = 120, MIN_H = 60;
                if (nw < MIN_W) nw = MIN_W;
                if (nh < MIN_H) nh = MIN_H;
                window()->size(nw, nh);
                return 1;
            }
        }
        return Fl_Box::handle(e);
    }
private:
    int start_w_ = 0, start_h_ = 0, start_mx_ = 0, start_my_ = 0;
};

// コンパクトモード開始/解除ボタン用のアイコン描画 (PiP 風)。
// Fl_Button を継承して、ラベル代わりに外枠 + 右下インナー矩形を描く。
class CompactIconButton : public Fl_Button {
public:
    CompactIconButton(int x, int y, int w, int h) : Fl_Button(x, y, w, h) {
        box(FL_NO_BOX);       // 背景を塗らず、アイコンのみ描く (半透明風)
        down_box(FL_NO_BOX);  // 押下時も箱を描かない
        visible_focus(0);
    }
    void draw() override {
        Fl_Button::draw();
        // 通常モードのツールバーに置かれる btn_compact_ はメニュー背景の
        // 上に乗るので、半透明風の薄色ブレンドは適用しない。オーバーレイ
        // (compact_exit_) だけがシートの上に出るので、シート色とのブレンド
        // が意味を持つ。
        bool overlay = box() == FL_NO_BOX;
        fl_color(overlay ? compact_overlay_color(labelcolor()) : labelcolor());
        int cx = x() + w() / 2, cy = y() + h() / 2;
        int ow = 12, oh = 10;              // 外枠
        int ox = cx - ow / 2, oy = cy - oh / 2;
        fl_rect(ox, oy, ow, oh);
        // 右下の内側矩形: オーバーレイは枠線のみ、通常ボタンは塗り。
        int iw = 5, ih = 4;
        int ix = ox + ow - iw - 1;
        int iy = oy + oh - ih - 1;
        if (overlay) fl_rect (ix, iy, iw, ih);
        else         fl_rectf(ix, iy, iw, ih);
    }
};

MainWindow::MainWindow(int w, int h, const char *title)
    : Fl_Double_Window(w, h, title)
{
    color(C_WIN_BG);

    // ---- メニューバー ----
    int mw = calc_menu_w(w);
    menu_ = new Fl_Menu_Bar(0, 0, mw, MENU_H);
    menu_->color(C_MENU_BG);
    menu_->textcolor(C_MENU_FG);
    menu_->box(FL_FLAT_BOX);
    menu_->add("&File/All &Clear",    FL_COMMAND | FL_SHIFT | FL_Delete, menu_cb, (void*)"clear_all", FL_MENU_DIVIDER);
    menu_->add("&File/&Open...",     FL_COMMAND + 'o', menu_cb, (void*)"open");
    menu_->add("&File/&Save As...",  FL_COMMAND + 's', menu_cb, (void*)"save", FL_MENU_DIVIDER);
    menu_->add("&Edit/&Undo",       FL_COMMAND + 'z', menu_cb, (void*)"undo");
    menu_->add("&Edit/&Redo",       FL_COMMAND + 'y', menu_cb, (void*)"redo", FL_MENU_DIVIDER);
    menu_->add("&Edit/Copy &All",   (FL_COMMAND | FL_SHIFT) + 'c', menu_cb, (void*)"copy_all", FL_MENU_DIVIDER);
    menu_->add("&Edit/&Insert Row Below", FL_Enter,            menu_cb, (void*)"insert_below");
    menu_->add("&Edit/Insert Row A&bove", FL_SHIFT | FL_Enter, menu_cb, (void*)"insert_above");
    menu_->add("&Edit/&Delete Row",       FL_COMMAND | FL_Delete,          menu_cb, (void*)"delete_row");
    menu_->add("&Edit/Move Row &Up",      FL_COMMAND | FL_SHIFT | FL_Up,   menu_cb, (void*)"move_up");
    menu_->add("&Edit/Move Row Do&wn",    FL_COMMAND | FL_SHIFT | FL_Down, menu_cb, (void*)"move_down", FL_MENU_DIVIDER);
    menu_->add("&Edit/&Recalculate", FL_F + 5, menu_cb, (void*)"recalc");
    menu_->add("&View/Always on &Top", FL_COMMAND + 't', menu_cb, (void*)"topmost", FL_MENU_TOGGLE);
    // Ctrl+: (JIS では Zoom Out の Ctrl+- と同じキー列。
    // OEM キー → FLTK keysym 化の配列ずれは
    // cmake/patch-fltk.py の Fl_win32.cxx パッチで根本対応済み)。
    menu_->add("&View/&Compact Mode", FL_COMMAND | ':', menu_cb, (void*)"toggle_compact", FL_MENU_TOGGLE);
    menu_->add("&View/Sys&tem Tray",              0, menu_cb, (void*)"toggle_tray",
               FL_MENU_TOGGLE | FL_MENU_DIVIDER);
    // Color Scheme サブメニュー (FL_MENU_RADIO)
    // USER_DEFINED は Prefs で編集する扱い。メニューには名前付きプリセットのみ出す。
    scheme_cmds_.reserve(COLOR_PRESET_COUNT);
    for (int i = 0; i < COLOR_PRESET_COUNT; i++) {
        char buf[24]; snprintf(buf, sizeof(buf), "scheme_%d", i);
        scheme_cmds_.push_back(buf);
        if (i == COLOR_PRESET_USER_DEFINED) continue;
        char path[128];
        snprintf(path, sizeof(path), "&View/Color &Scheme/%s", COLOR_PRESET_INFO[i].label);
        menu_->add(path, 0, menu_cb, (void*)scheme_cmds_[i].c_str(), FL_MENU_RADIO);
    }
    menu_->add("&View/Show &Row Lines",           0, menu_cb, (void*)"toggle_rowlines",
               FL_MENU_TOGGLE | FL_MENU_DIVIDER);
    menu_->add("&View/Zoom &In",       FL_COMMAND | FL_SHIFT | '-', menu_cb, (void*)"zoom_in");
    menu_->add("&View/Zoom &Out",      FL_COMMAND | '-',             menu_cb, (void*)"zoom_out");
    menu_->add("&View/Reset &Zoom",    FL_COMMAND + '0', menu_cb, (void*)"zoom_reset", FL_MENU_DIVIDER);
    menu_->add("&View/Scientific Notation (&E)",  0, menu_cb, (void*)"toggle_e_notation", FL_MENU_TOGGLE);
    menu_->add("&View/Show Thousands &Separator", 0, menu_cb, (void*)"toggle_thousands", FL_MENU_TOGGLE);
    menu_->add("&View/Show &Hex Separator",       0, menu_cb, (void*)"toggle_hexsep", FL_MENU_TOGGLE);
    menu_->add("&View/Decimals &+",    (FL_COMMAND | FL_SHIFT) + '.', menu_cb, (void*)"dec_inc");
    menu_->add("&View/Decimals &\xe2\x88\x92", (FL_COMMAND | FL_SHIFT) + ',', menu_cb, (void*)"dec_dec", FL_MENU_DIVIDER);
    menu_->add("&View/&Auto Completion",          0, menu_cb, (void*)"toggle_auto_complete", FL_MENU_TOGGLE);
    populate_samples_menu();
    menu_->add("&File/&Preferences...", FL_COMMAND + ',', menu_cb, (void*)"prefs");
    menu_->add("&File/&About calcyx",   FL_F + 1,         menu_cb, (void*)"about", FL_MENU_DIVIDER);
    menu_->add("&File/E&xit",         0,                menu_cb, (void*)"exit");

    // 全メニュー追加後にインデックスを取得
    // find_index(path) はショートカット付きラベルで失敗するので手動検索
    mi_undo_ = mi_redo_ = mi_topmost_ = -1;
    for (int i = 0; i < menu_->size(); i++) {
        const Fl_Menu_Item &it = menu_->menu()[i];
        if (!it.label()) continue;
        if (it.callback() == menu_cb) {
            const char *d = (const char *)it.user_data();
            if (d && strcmp(d, "undo") == 0) mi_undo_ = i;
            if (d && strcmp(d, "redo") == 0) mi_redo_ = i;
            if (d && strcmp(d, "topmost") == 0) mi_topmost_ = i;
            if (d && strcmp(d, "toggle_compact") == 0) mi_compact_ = i;
            if (d && strcmp(d, "toggle_rowlines") == 0)  mi_rowlines_  = i;
            if (d && strcmp(d, "toggle_thousands") == 0) mi_thousands_ = i;
            if (d && strcmp(d, "toggle_hexsep") == 0)    mi_hexsep_    = i;
            if (d && strcmp(d, "toggle_e_notation") == 0) mi_e_notation_ = i;
            if (d && strcmp(d, "toggle_auto_complete") == 0) mi_auto_complete_ = i;
            if (d && strcmp(d, "toggle_tray") == 0)      mi_tray_      = i;
            if (d && strncmp(d, "scheme_", 7) == 0) {
                int idx = atoi(d + 7);
                if (idx >= 0 && idx < COLOR_PRESET_COUNT) mi_scheme_[idx] = i;
            }
        }
    }

    // ---- ← → 📌 ? ツールバーボタン (右寄せ) ----
    auto make_btn = [&](int bx, int bw, const char *label, const char *cmd) {
        auto *b = new Fl_Button(bx, 0, bw, MENU_H, label);
        b->box(FL_FLAT_BOX);
        b->color(C_MENU_BG);
        b->labelcolor(C_MENU_FG);
        b->labelsize(14);
        b->callback(menu_cb, (void*)cmd);
        b->visible_focus(0);
        return b;
    };
    // ツールチップ: macOS では Cmd (⌘)、それ以外は Ctrl で表記。
#ifdef __APPLE__
    const char *TT_UNDO    = "Undo (\xe2\x8c\x98Z)";
    const char *TT_REDO    = "Redo (\xe2\x8c\x98Y)";
    const char *TT_COMPACT = "Compact Mode (\xe2\x8c\x98:)";
    const char *TT_TOPMOST = "Always on Top (\xe2\x8c\x98T)";
#else
    const char *TT_UNDO    = "Undo (Ctrl+Z)";
    const char *TT_REDO    = "Redo (Ctrl+Y)";
    const char *TT_COMPACT = "Compact Mode (Ctrl+:)";
    const char *TT_TOPMOST = "Always on Top (Ctrl+T)";
#endif

    // 右端から: [Format▼] PAD [📌] [▣] [→] [←]
    int rx = w - CHOICE_W;                              // Format▼
    rx -= PAD + PIN_W;                                  // 📌
    btn_topmost_ = make_btn(rx, PIN_W, "@menu", "topmost");
    btn_topmost_->labelsize(10);
    btn_topmost_->tooltip(TT_TOPMOST);
    rx -= COMPACT_W;                                    // ▣ (コンパクトモード開始)
    btn_compact_ = new CompactIconButton(rx, 0, COMPACT_W, MENU_H);
    // ツールバー内のボタンは半透明ではなく不透明にする (周囲のメニューと色を合わせる)
    btn_compact_->box(FL_FLAT_BOX);
    btn_compact_->down_box(FL_FLAT_BOX);
    btn_compact_->color(C_MENU_BG);
    btn_compact_->labelcolor(C_MENU_FG);
    btn_compact_->callback(menu_cb, (void*)"toggle_compact");
    btn_compact_->tooltip(TT_COMPACT);
    rx -= BTN_W;                                        // →
    btn_redo_ = make_btn(rx, BTN_W, "@->", "redo");
    btn_redo_->tooltip(TT_REDO);
    rx -= BTN_W;                                        // ←
    btn_undo_ = make_btn(rx, BTN_W, "@<-", "undo");
    btn_undo_->tooltip(TT_UNDO);

    // ---- フォーマット選択ドロップダウン (右端) ----
    fmt_choice_ = new Fl_Choice(w - CHOICE_W, 0, CHOICE_W, MENU_H);
    fmt_choice_->color(C_MENU_BG);
    fmt_choice_->labelcolor(C_MENU_FG);
    fmt_choice_->textcolor(C_MENU_FG);
    fmt_choice_->box(FL_FLAT_BOX);
    for (int i = 0; i < FMT_COUNT; i++) {
        const char *l = FMT_DEFS[i].label;
        int sc = 0;
        if (strcmp(l, "Auto") == 0) sc = FL_F + 8;
        else if (strcmp(l, "Dec") == 0) sc = FL_F + 9;
        else if (strcmp(l, "Hex") == 0) sc = FL_F + 10;
        else if (strcmp(l, "Bin") == 0) sc = FL_F + 11;
        else if (strcmp(l, "SI")  == 0) sc = FL_F + 12;
        fmt_choice_->add(l, sc, nullptr);
    }
    fmt_choice_->value(0);  // Auto
    fmt_choice_->callback(choice_cb, this);
    fmt_choice_->visible_focus(0);  // Tab キー順から除外

    // シートビュー (メニューバー直下、ウィンドウ全体)
    int sheet_y = MENU_H + PAD;
    int sheet_h = h - MENU_H - PAD * 2;
    sheet_ = new SheetView(PAD, sheet_y, w - PAD * 2, sheet_h);

    // フォーカス行変更時に Fl_Choice を更新
    sheet_->set_row_change_cb(row_change_cb, this);

    // コンパクトモード用のオーバーレイ (通常は hide)。右上にドラッグ
    // グリップ、その下に解除ボタン、右下にリサイズグリップを配置する。
    // sheet_ より後に追加することで sheet 上に描画される (popup_ より前
    // なので popup が最前面)。
    drag_grip_ = new DragGrip(w - GRIP_SZ, 0, GRIP_SZ, GRIP_SZ);
    drag_grip_->color(C_MENU_BG);
    drag_grip_->labelcolor(C_MENU_FG);
    drag_grip_->hide();

    compact_exit_ = new CompactIconButton(w - GRIP_SZ, GRIP_SZ, GRIP_SZ, GRIP_SZ);
    compact_exit_->color(C_MENU_BG);
    compact_exit_->labelcolor(C_MENU_FG);
    compact_exit_->callback(menu_cb, (void*)"toggle_compact");
    compact_exit_->hide();

    resize_grip_ = new ResizeGrip(w - GRIP_SZ, h - GRIP_SZ, GRIP_SZ, GRIP_SZ);
    resize_grip_->color(C_MENU_BG);
    resize_grip_->labelcolor(C_MENU_FG);
    resize_grip_->hide();

    // 補完ポップアップ: 設定に応じて埋め込み (Fl_Group の子) または独立
    // (Fl_Menu_Window) で生成する。sheet_->popup_ も同時に設定される。
    popup_ = nullptr;
    recreate_popup_if_needed();

    end();
    resizable(sheet_);
    apply_size_range();

    // コンパクトモード用のジオメトリを state.ini から読み込む (通常モードの
    // ジオメトリは main.cpp で読み込み済み)。
    load_compact_geometry();

    // 初期状態のグレーアウトを反映
    update_toolbar();
    // View メニューの toggle 項目を現在の設定値で同期
    sync_view_menu_toggles();

    // WM close を横取りしてトレイ最小化に対応
    callback(close_cb, this);

    // setup_tray() はウィンドウが shown() になってから実行する
    // (fl_xid が有効になるタイミング = handle(FL_SHOW) 初回)
}

void MainWindow::resize(int nx, int ny, int nw, int nh) {
    Fl_Double_Window::resize(nx, ny, nw, nh);
    if (compact_mode_) {
        // コンパクトモード: sheet が全面。overlay は sheet のスクロールバー
        // 左隣に置く (スクロールバーと重ならないように)。リサイズグリップ
        // は右下コーナー、同じく SB 左側。
        sheet_->resize(0, 0, nw, nh);
        int ox = nw - sheet_->sb_w() - GRIP_SZ;
        drag_grip_->resize(ox, 0, GRIP_SZ, GRIP_SZ);
        compact_exit_->resize(ox, GRIP_SZ, GRIP_SZ, GRIP_SZ);
        resize_grip_->resize(ox, nh - GRIP_SZ, GRIP_SZ, GRIP_SZ);
        return;
    }
    // 通常モード: sheet 位置をコンパクト遷移前の normal 配置に戻す
    // (compact 中に sheet を (0,0,nw,nh) に広げているので明示的に再計算)
    sheet_->resize(PAD, MENU_H + PAD, nw - PAD * 2, nh - MENU_H - PAD * 2);
    int mw = calc_menu_w(nw);
    menu_->resize(0, 0, mw, MENU_H);
    int rx = nw - CHOICE_W;
    fmt_choice_->resize(rx, 0, CHOICE_W, MENU_H);
    rx -= PAD + PIN_W;
    btn_topmost_->resize(rx, 0, PIN_W, MENU_H);
    rx -= COMPACT_W;
    btn_compact_->resize(rx, 0, COMPACT_W, MENU_H);
    rx -= BTN_W;
    btn_redo_->resize(rx, 0, BTN_W, MENU_H);
    rx -= BTN_W;
    btn_undo_->resize(rx, 0, BTN_W, MENU_H);
}

void MainWindow::apply_ui_colors() {
    colors_apply_fl_scheme();
    color(C_WIN_BG);
    menu_->color(C_MENU_BG);
    menu_->textcolor(C_MENU_FG);
    btn_undo_->color(C_MENU_BG);
    btn_redo_->color(C_MENU_BG);
    btn_compact_->color(C_MENU_BG);
    btn_compact_->labelcolor(C_MENU_FG);
    btn_topmost_->color(C_MENU_BG);
    btn_topmost_->labelcolor(topmost_ ? C_MENU_FG : fl_inactive(C_MENU_FG));
    fmt_choice_->color(C_MENU_BG);
    fmt_choice_->textcolor(C_MENU_FG);
    if (drag_grip_)    { drag_grip_->color(C_MENU_BG);    drag_grip_->labelcolor(C_MENU_FG); }
    if (compact_exit_) { compact_exit_->color(C_MENU_BG); compact_exit_->labelcolor(C_MENU_FG); }
    if (resize_grip_)  { resize_grip_->color(C_MENU_BG);  resize_grip_->labelcolor(C_MENU_FG); }
    update_toolbar();
    redraw();
}

void MainWindow::update_toolbar() {
    bool u = sheet_->can_undo() || sheet_->has_uncommitted_edit();
    bool r = sheet_->can_redo();

    // ← → ボタン: deactivate() は白っぽくなるので labelcolor だけ変更
    // (undo/redo は内部でガードしているので、無効時にクリックしても安全)
    Fl_Color C_DIM = g_colors.ui_dim;
    btn_undo_->labelcolor(u ? C_MENU_FG : C_DIM);
    btn_redo_->labelcolor(r ? C_MENU_FG : C_DIM);
    btn_undo_->redraw();
    btn_redo_->redraw();

    // Edit メニュー項目: deactivate() すると FLTK が fl_inactive() で更に
    // 背景色にブレンドしてしまい C_DIM が潰れるので、labelcolor のみ変更。
    // sheet_->undo()/redo() 側で can_undo()/can_redo() を見た no-op ガードがあるため安全。
    Fl_Menu_Item *items = (Fl_Menu_Item *)menu_->menu();
    auto set_menu = [&](int idx, bool active) {
        if (idx < 0) return;
        items[idx].labelcolor(active ? C_MENU_FG : C_DIM);
    };
    set_menu(mi_undo_, u);
    set_menu(mi_redo_, r);
}

int MainWindow::handle(int event) {
#ifdef __APPLE__
    // macOS: Cmd+Shift+, / Cmd+Shift+. を FLTK メニューより先に捕捉する。
    // FLTK は Cmd+Shift+, を Cmd+, (Preferences) とマッチしてしまうため、
    // handle() で先にキーを判定して直接ディスパッチする。
    if (event == FL_SHORTCUT || event == FL_KEYBOARD) {
        int key = Fl::event_key();
        int state = Fl::event_state();
        if ((state & (FL_COMMAND | FL_SHIFT)) == (FL_COMMAND | FL_SHIFT)) {
            if (key == ',' || key == '<') {
                menu_cb(this, (void *)"dec_dec");
                return 1;
            }
            if (key == '.' || key == '>') {
                menu_cb(this, (void *)"dec_inc");
                return 1;
            }
        }
    }
#endif

    // ウィンドウ表示後に初回のみトレイ/ホットキーを初期化
    if (event == FL_SHOW && !tray_initialized_) {
        tray_initialized_ = true;
        // 次の event loop iteration で実行 (fl_xid が確実に有効になるタイミング)
        Fl::add_timeout(0.0, [](void *d) {
            static_cast<MainWindow *>(d)->setup_tray();
        }, this);
    }

    // ポップアップ表示中にポップアップ外をクリックしたら閉じる。
    // 独立ウィンドウ実装では contains_window_point() は常に false を返す
    // (別 OS ウィンドウなのでメインウィンドウへのクリックは必ず外側)。
    if (event == FL_PUSH && popup_->is_shown()) {
        int ex = Fl::event_x(), ey = Fl::event_y();
        if (!popup_->contains_window_point(ex, ey))
            sheet_->completion_hide();
    }
#if defined(_WIN32) && !defined(NDEBUG)
    if (event == FL_PUSH && shown()) {
        HWND hwnd = fl_win32_xid(this);
        if (hwnd) {
            POINT pt = {0, 0};
            ClientToScreen(hwnd, &pt);
            float s = Fl::screen_scale(screen_num());
            static FILE *dbgf = nullptr;
            if (!dbgf) {
                char p[MAX_PATH];
                GetModuleFileNameA(NULL, p, MAX_PATH);
                std::string lp(p);
                auto slash = lp.rfind('\\');
                if (slash != std::string::npos) lp = lp.substr(0, slash);
                lp += "\\menu-debug.log";
                dbgf = fopen(lp.c_str(), "a");
            }
            if (dbgf) {
                fprintf(dbgf, "[menu-dbg] FL(%d,%d,%d,%d) C2S(%ld,%ld) scr=%d s=%g mouse=(%d,%d)\n",
                        x(), y(), w(), h(),
                        pt.x, pt.y, screen_num(), s,
                        Fl::event_x_root(), Fl::event_y_root());
                fflush(dbgf);
            }
        }
    }
#endif
    return Fl_Double_Window::handle(event);
}

void MainWindow::open_file(const char *path) {
    if (!sheet_->load_file(path))
        fl_alert("Cannot open file:\n%s", path);
}

void MainWindow::update_fmt_choice() {
    const char *fn = sheet_->current_fmt_name();
    for (int i = 0; i < FMT_COUNT; i++) {
        if (fn == nullptr && FMT_DEFS[i].func_name == nullptr) { fmt_choice_->value(i); return; }
        if (fn && FMT_DEFS[i].func_name && strcmp(fn, FMT_DEFS[i].func_name) == 0) { fmt_choice_->value(i); return; }
    }
    fmt_choice_->value(0);  // 不明な場合は Auto
}

void MainWindow::row_change_cb(void *data) {
    auto *win = static_cast<MainWindow *>(data);
    win->update_fmt_choice();
    win->update_toolbar();
}

// ---- About ダイアログ (Fl_Help_View でリンクをクリック可能にする) ----

static const char *about_link_cb(Fl_Widget *, const char *uri) {
    if (uri && *uri) fl_open_uri(uri);
    return nullptr;  // nullptr を返すと Help_View 内遷移をしない
}

static void show_about(MainWindow *win) {
    (void)win;
    // アイコン (static で1回だけロード)
    static Fl_SVG_Image *about_icon = nullptr;
    if (!about_icon) {
#ifdef _WIN32
        // Windows は icon.svg を .exe の RCDATA から読む (calcyx.rc で埋め込み済み)
        HRSRC hRes = FindResourceA(nullptr, "ICON_SVG", RT_RCDATA);
        if (hRes) {
            DWORD sz = SizeofResource(nullptr, hRes);
            HGLOBAL hData = LoadResource(nullptr, hRes);
            const unsigned char *data = hData ? (const unsigned char *)LockResource(hData) : nullptr;
            if (data && sz > 0) {
                about_icon = new Fl_SVG_Image(nullptr, data, sz);
                if (about_icon->fail()) { delete about_icon; about_icon = nullptr; }
                else about_icon->resize(64, 64);
            }
        }
#else
        std::string svg_path = find_icon_svg();
        if (!svg_path.empty()) {
            about_icon = new Fl_SVG_Image(svg_path.c_str());
            if (about_icon->fail()) { delete about_icon; about_icon = nullptr; }
            else about_icon->resize(64, 64);
        }
#endif
    }

    const int DW = 420, DH = 380;
    Fl_Double_Window dlg(DW, DH, "About calcyx");
    dlg.set_modal();
    dlg.color(g_colors.ui_bg);

    // アイコン
    Fl_Box icon_box(DW / 2 - 32, 10, 64, 64);
    icon_box.box(FL_NO_BOX);
    if (about_icon) icon_box.image(about_icon);

    // HTML コンテンツ
    Fl_Help_View hv(10, 80, DW - 20, DH - 120);
    hv.color(g_colors.ui_bg);
    hv.textcolor(g_colors.ui_text);
    hv.textfont(FL_HELVETICA);
    hv.textsize(12);
    hv.link(about_link_cb);
    hv.scrollbar_size(0);  // スクロールバーを隠す

    std::string html =
        "<center>"
        "<b>calcyx " CALCYX_VERSION_FULL "</b><br>"
        CALCYX_EDITION
        "<p>A programmable calculator based on Calctus.</p>"
        "<a href='https://github.com/ponzu840w/calcyx'>https://github.com/ponzu840w/calcyx</a>"
        "</center>"
        "<font size=2>"
        "<p><b>calcyx</b> \xe2\x80\x94 Copyright \xc2\xa9 2026 ponzu840w \xe2\x80\x94 MIT License</p>"
        "<p><b>Calctus</b> \xe2\x80\x94 Copyright \xc2\xa9 2022 shapoco \xe2\x80\x94 MIT License<br>"
        "<a href='https://github.com/shapoco/calctus'>https://github.com/shapoco/calctus</a></p>"
        "<p><b>FLTK</b> \xe2\x80\x94 Copyright \xc2\xa9 1998-2024 Bill Spitzak and others \xe2\x80\x94 LGPL<br>"
        "<a href='https://www.fltk.org'>https://www.fltk.org</a></p>"
        "<p><b>mpdecimal</b> \xe2\x80\x94 Copyright \xc2\xa9 2008-2024 Stefan Krah \xe2\x80\x94 BSD 2-Clause<br>"
        "<a href='https://www.bytereef.org/mpdecimal'>https://www.bytereef.org/mpdecimal</a></p>"
        "</font>";
    hv.value(html.c_str());

    // OK ボタン
    Fl_Button ok_btn(DW / 2 - 40, DH - 35, 80, 25, "OK");
    ok_btn.callback([](Fl_Widget *w, void *) { w->window()->hide(); });

    dlg.end();
    dlg.show();
    while (dlg.shown()) Fl::wait();
}

void MainWindow::menu_cb(Fl_Widget *w, void *data) {
    (void)w;
    const char *cmd = static_cast<const char *>(data);
    if (!cmd) return;
    MainWindow *win = nullptr;
    for (Fl_Window *fw = Fl::first_window(); fw; fw = Fl::next_window(fw))
        if ((win = dynamic_cast<MainWindow *>(fw))) break;
    if (!win) return;

    if (strcmp(cmd, "open") == 0) {
        Fl_Native_File_Chooser fc;
        fc.title("Open");
        fc.type(Fl_Native_File_Chooser::BROWSE_FILE);
        fc.filter("Text files\t*.txt\nAll files\t*");
        if (fc.show() == 0) win->open_file(fc.filename());

    } else if (strcmp(cmd, "save") == 0) {
        Fl_Native_File_Chooser fc;
        fc.title("Save As");
        fc.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
        fc.filter("Text files\t*.txt\nAll files\t*");
        fc.options(Fl_Native_File_Chooser::SAVEAS_CONFIRM);
        if (fc.show() == 0) {
            if (!win->sheet_->save_file(fc.filename()))
                fl_alert("Cannot save file:\n%s", fc.filename());
        }

    } else if (strcmp(cmd, "copy_all") == 0) {
        win->sheet_->copy_all_to_clipboard();
    } else if (strcmp(cmd, "recalc") == 0) {
        win->sheet_->live_eval();
        win->sheet_->redraw();
    } else if (strcmp(cmd, "insert_below") == 0) {
        win->sheet_->insert_row_below();
    } else if (strcmp(cmd, "insert_above") == 0) {
        win->sheet_->insert_row_above();
    } else if (strcmp(cmd, "delete_row") == 0) {
        win->sheet_->delete_current_row();
    } else if (strcmp(cmd, "move_up") == 0) {
        win->sheet_->move_row_up();
    } else if (strcmp(cmd, "move_down") == 0) {
        win->sheet_->move_row_down();
    } else if (strcmp(cmd, "clear_all") == 0) {
        win->sheet_->clear_all();
    } else if (strcmp(cmd, "undo") == 0) {
        win->sheet_->undo();
    } else if (strcmp(cmd, "redo") == 0) {
        win->sheet_->redo();
    } else if (strcmp(cmd, "exit") == 0) {
        win->teardown_tray();
        win->save_prefs();
        exit(0);
    } else if (strcmp(cmd, "prefs") == 0) {
        PrefsDialog::run(win->sheet_, [](void *d) {
            auto *mw = static_cast<MainWindow *>(d);
            mw->apply_ui_colors();
            mw->apply_tray_settings();
            mw->sync_view_menu_toggles();
            // 補完ポップアップ独立化設定が変わっていたら実装を差し替える
            mw->recreate_popup_if_needed();
        }, win);
    } else if (strcmp(cmd, "topmost") == 0) {
        win->toggle_always_on_top();
    } else if (strcmp(cmd, "toggle_compact") == 0) {
        win->toggle_compact_mode();
    } else if (strcmp(cmd, "toggle_rowlines") == 0) {
        g_show_rowlines = !g_show_rowlines;
        win->sheet_->redraw();
        win->sync_view_menu_toggles();
    } else if (strcmp(cmd, "toggle_thousands") == 0) {
        g_sep_thousands = !g_sep_thousands;
        win->sheet_->live_eval();
        win->sheet_->redraw();
        win->sync_view_menu_toggles();
    } else if (strcmp(cmd, "toggle_hexsep") == 0) {
        g_sep_hex = !g_sep_hex;
        win->sheet_->live_eval();
        win->sheet_->redraw();
        win->sync_view_menu_toggles();
    } else if (strcmp(cmd, "toggle_e_notation") == 0) {
        g_fmt_settings.e_notation = !g_fmt_settings.e_notation;
        win->sheet_->live_eval();
        win->sheet_->redraw();
        win->sync_view_menu_toggles();
    } else if (strcmp(cmd, "toggle_auto_complete") == 0) {
        g_input_auto_completion = !g_input_auto_completion;
        win->sync_view_menu_toggles();
    } else if (strcmp(cmd, "dec_inc") == 0) {
        if (g_fmt_settings.decimal_len < 34) {
            g_fmt_settings.decimal_len++;
            win->sheet_->live_eval();
            win->sheet_->redraw();
        }
    } else if (strcmp(cmd, "dec_dec") == 0) {
        if (g_fmt_settings.decimal_len > 1) {
            g_fmt_settings.decimal_len--;
            win->sheet_->live_eval();
            win->sheet_->redraw();
        }
    } else if (strncmp(cmd, "scheme_", 7) == 0) {
        int idx = atoi(cmd + 7);
        if (idx >= 0 && idx < COLOR_PRESET_COUNT) {
            colors_apply_preset(idx);
            win->apply_ui_colors();
            win->sync_view_menu_toggles();
        }
    } else if (strcmp(cmd, "toggle_tray") == 0) {
        g_tray_icon = !g_tray_icon;
        win->apply_tray_settings();
        win->sync_view_menu_toggles();
    } else if (strcmp(cmd, "zoom_in") == 0) {
        if (g_font_size < 36) { g_font_size++; win->apply_font_and_refresh(); }
    } else if (strcmp(cmd, "zoom_out") == 0) {
        if (g_font_size > 8) { g_font_size--; win->apply_font_and_refresh(); }
    } else if (strcmp(cmd, "zoom_reset") == 0) {
        g_font_size = DEFAULT_FONT_SIZE;
        win->apply_font_and_refresh();
    } else if (strcmp(cmd, "about") == 0) {
        show_about(win);
    } else {
        open_sample_file(win, cmd);
    }
}

// icon.svg のパスを返す。見つからなければ空文字列。
static std::string find_icon_svg() {
    struct stat st;
#ifdef __APPLE__
    {
        char buf[1024];
        uint32_t size = sizeof(buf);
        if (_NSGetExecutablePath(buf, &size) == 0) {
            char *dir = dirname(buf);
            char candidate[1024];
            snprintf(candidate, sizeof(candidate), "%s/../Resources/icon.svg", dir);
            if (stat(candidate, &st) == 0 && S_ISREG(st.st_mode)) return candidate;
        }
    }
#endif
    static const char *bases[] = {
        "gui/icon.svg", "../gui/icon.svg", "../../gui/icon.svg",
        "icon.svg", "../icon.svg",
        "/usr/share/icons/hicolor/scalable/apps/calcyx.svg",
    };
    for (auto *base : bases) {
        if (stat(base, &st) == 0 && S_ISREG(st.st_mode)) return base;
    }
    return "";
}

// exe の絶対ディレクトリを返す。取得できなければ空文字列。
static std::string find_exe_dir() {
    char buf[1024];
#ifdef __APPLE__
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) != 0) return "";
    return dirname(buf);
#elif defined(_WIN32)
    wchar_t wbuf[1024];
    DWORD n = GetModuleFileNameW(nullptr, wbuf, 1024);
    if (n == 0 || n >= 1024) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, buf, sizeof(buf), nullptr, nullptr);
    if (len <= 0) return "";
    char *sep = strrchr(buf, '\\');
    if (!sep) sep = strrchr(buf, '/');
    if (sep) *sep = '\0';
    return buf;
#else
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return "";
    buf[n] = '\0';
    char *sep = strrchr(buf, '/');
    if (sep) *sep = '\0';
    return buf;
#endif
}

// samples/ ディレクトリへの絶対パスを返す。見つからなければ空文字列。
static std::string find_samples_dir() {
    std::string dir = find_exe_dir();
    if (dir.empty()) return "";
    struct stat st;
    const char *suffixes[] = {
#ifdef __APPLE__
        "/../Resources/samples",        // .app バンドル埋め込み
#endif
        "/samples",                     // ビルドツリー / exe と同階層
        "/../samples",                  // Windows zip: bin/*.exe + samples/
        "/../share/calcyx/samples",     // Unix インストール (/usr/bin → /usr/share/calcyx/samples)
    };
    for (const char *suf : suffixes) {
        std::string candidate = dir + suf;
        if (stat(candidate.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) return candidate;
    }
    return "";
}

void MainWindow::populate_samples_menu() {
    std::string dir = find_samples_dir();
    if (dir.empty()) return;

    DIR *dp = opendir(dir.c_str());
    if (!dp) return;

    std::vector<std::string> files;
    struct dirent *ent;
    while ((ent = readdir(dp)) != nullptr) {
        std::string name = ent->d_name;
        if (name.size() > 4 && name.substr(name.size() - 4) == ".txt")
            files.push_back(name);
    }
    closedir(dp);

    std::sort(files.begin(), files.end());
    sample_files_ = std::move(files);

    for (const auto &f : sample_files_) {
        std::string label = "&File/&Samples/" + f.substr(0, f.size() - 4);
        menu_->add(label.c_str(), 0, menu_cb, (void *)f.c_str());
    }

    // Samples サブメニューの後に区切り線を入れる
    Fl_Menu_Item *it = (Fl_Menu_Item *)menu_->find_item("&File/&Samples");
    if (it) it->flags |= FL_MENU_DIVIDER;
}

bool MainWindow::open_sample_file(MainWindow *win, const char *filename) {
    std::string dir = find_samples_dir();
    if (!dir.empty()) {
        std::string path = dir + "/" + filename;
        if (win->sheet_->load_file(path.c_str())) return true;
    }
    fl_alert("File not found:\n%s", filename);
    return false;
}

void MainWindow::toggle_always_on_top() {
    topmost_ = !topmost_;
#if defined(_WIN32)
    HWND hwnd = fl_xid(this);
    SetWindowPos(hwnd, topmost_ ? HWND_TOPMOST : HWND_NOTOPMOST,
                 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
#elif defined(__APPLE__)
    mac_set_window_level(this, topmost_ ? 1 : 0);
#else
    Display *dpy = fl_display;
    Window xwin = fl_xid(this);
    Atom wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
    Atom wm_above = XInternAtom(dpy, "_NET_WM_STATE_ABOVE", False);
    XEvent ev = {};
    ev.xclient.type = ClientMessage;
    ev.xclient.window = xwin;
    ev.xclient.message_type = wm_state;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = topmost_ ? 1 : 0;  // _NET_WM_STATE_ADD / REMOVE
    ev.xclient.data.l[1] = (long)wm_above;
    ev.xclient.data.l[2] = 0;
    XSendEvent(dpy, DefaultRootWindow(dpy), False,
               SubstructureNotifyMask | SubstructureRedirectMask, &ev);
    XFlush(dpy);
#endif
    // ボタンの見た目を更新 (ON: 通常色, OFF: 薄色)
    btn_topmost_->labelcolor(topmost_ ? C_MENU_FG : fl_inactive(C_MENU_FG));
    btn_topmost_->redraw();
    // メニュー側のチェックも同期 (ピンボタンから呼ばれた場合)
    sync_view_menu_toggles();
}

// コンパクトモード中の再描画伝播: sheet に damage があれば overlay に
// も damage を立ててから通常の flush に委譲する。これにより sheet の
// 上に overlay が確実に再描画される。
// Windows の FLTK ドライバは WM_PAINT を WndProc から同期処理するため
// Fl::add_check では間に合わない経路があり、flush() override が必須。
void MainWindow::flush() {
    if (compact_mode_ && sheet_ && sheet_->damage()) {
        if (drag_grip_)    drag_grip_->damage(FL_DAMAGE_ALL);
        if (compact_exit_) compact_exit_->damage(FL_DAMAGE_ALL);
        if (resize_grip_)  resize_grip_->damage(FL_DAMAGE_ALL);
    }
    Fl_Double_Window::flush();
}

// コンパクトモードの切替。UI クロームを隠してシート領域を全面化する。
// ボーダー (OS タイトルバー) の切替には hide()/show() サイクルが必要な
// プラットフォームがあるため、全環境で共通処理する。
// コンパクト中は PiP 的に必ず always-on-top。解除時は元のトグル状態に戻す。
// 補完ポップアップを現在のモード × 設定に合わせた実装で (再) 生成する。
// 望む型が現行 popup_ と一致していれば何もしない。
// 初回は popup_ が nullptr なので必ず生成する。
void MainWindow::recreate_popup_if_needed() {
    bool want_independent = compact_mode_
        ? g_popup_independent_compact
        : g_popup_independent_normal;
    bool is_independent = dynamic_cast<CompletionPopupWindow *>(popup_) != nullptr;
    if (popup_ && is_independent == want_independent) return;

    CompletionPopupBase *old = popup_;
    if (old && old->is_shown()) old->hide_popup();
    popup_ = nullptr;

    // 独立ウィンドウは current group に属してはいけない。埋め込みは
    // this を current group にして生成する。どちらも生成後に元に戻す。
    Fl_Group *prev = Fl_Group::current();
    if (want_independent) {
        Fl_Group::current(nullptr);
        popup_ = new CompletionPopupWindow(this);
    } else {
        Fl_Group::current(this);
        popup_ = new CompletionPopup();
    }
    Fl_Group::current(prev);

    if (sheet_) sheet_->popup_ = popup_;

    delete old;
}

void MainWindow::toggle_compact_mode() {
    compact_mode_ = !compact_mode_;

    // モード別の設定に合わせて補完ポップアップの実装 (埋め込み/独立) を切替
    recreate_popup_if_needed();

    int target_x, target_y, target_w, target_h;

    if (compact_mode_) {
        // 復帰用に現在 (通常モード) のジオメトリと topmost 状態を記憶
        saved_x_ = x(); saved_y_ = y();
        saved_w_ = w(); saved_h_ = h();
        saved_topmost_ = topmost_;

        // compact ジオメトリ: 永続化済みならそれに、なければ現状維持
        if (compact_geometry_valid_) {
            target_x = compact_x_; target_y = compact_y_;
            target_w = compact_w_; target_h = compact_h_;
        } else {
            target_x = x(); target_y = y();
            target_w = w(); target_h = h();
        }

        // メニューバーは hide ではなくオフスクリーン (上に押し出し) する。
        // hide すると Fl_Menu_Bar が FL_SHORTCUT を受信しなくなり、コンパクト
        // モード中に Ctrl+Z 等のメニューショートカットがすべて無効化される。
        // visible のまま画面外に置けば描画は走らないが shortcut マッチは生きる。
        menu_->resize(0, -MENU_H, w(), MENU_H);
        btn_undo_->hide();
        btn_redo_->hide();
        btn_compact_->hide();
        btn_topmost_->hide();
        fmt_choice_->hide();
        drag_grip_->show();
        compact_exit_->show();
        resize_grip_->show();
        sheet_->set_sb_w(8);  // 細いスクロールバー
    } else {
        // compact 終了: 現在の compact 状態を次回用に保存
        compact_x_ = x(); compact_y_ = y();
        compact_w_ = w(); compact_h_ = h();
        compact_geometry_valid_ = true;

        target_x = saved_x_; target_y = saved_y_;
        target_w = saved_w_; target_h = saved_h_;

        menu_->show();
        btn_undo_->show();
        btn_redo_->show();
        btn_compact_->show();
        btn_topmost_->show();
        fmt_choice_->show();
        drag_grip_->hide();
        compact_exit_->hide();
        resize_grip_->hide();
        sheet_->set_sb_w(SheetView::SB_W_DEFAULT);
    }

    // ボーダー切替: Windows は hide→border→show サイクルが必須 (show 後の
    // border() が反映されない)。X11/WSLg では同じシーケンスでウィンドウが
    // 消失したり再 map で位置がずれる現象があるため、hide/show を回避し
    // border() を runtime で書き換えるだけにする。
    // hide() は save_prefs を呼ぶので直接 Fl_Double_Window::hide() を叩く。
    // 新モードの最小サイズ制約を resize 前に適用しておかないと、compact→normal
    // で小さめの target に対して size_range の旧制約 (compact 用の小さい値) のまま
    // resize された後に normal 用の大きな制約が適用される順序になり、初回表示が
    // ガクッと拡大する。逆も同様。
    apply_size_range();
#if defined(_WIN32)
    Fl_Double_Window::hide();
    border(compact_mode_ ? 0 : 1);
    resize(target_x, target_y, target_w, target_h);
    show();
#else
    border(compact_mode_ ? 0 : 1);
    resize(target_x, target_y, target_w, target_h);
#endif

#ifdef _WIN32
    // Windows のボーダーレスウィンドウは既定で Alt+Tab 一覧から外れる。
    // WS_EX_APPWINDOW を付けるとタスクバー/Alt+Tab に強制的に出る。
    // 通常モードでは不要なので外す (元の挙動に戻す)。
    if (HWND hwnd = fl_xid(this)) {
        LONG_PTR ex = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
        if (compact_mode_) ex |= WS_EX_APPWINDOW;
        else               ex &= ~WS_EX_APPWINDOW;
        SetWindowLongPtr(hwnd, GWL_EXSTYLE, ex);
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_NOACTIVATE | SWP_FRAMECHANGED);
        // hide→show で失われたフォーカスを戻す (自プロセス内なので
        // フォアグラウンドロックの制約は受けない)。
        SetForegroundWindow(hwnd);
        SetFocus(hwnd);
    }
#endif

    // hide/show でプラットフォーム側の topmost が外れる + compact 中は
    // 強制的に PiP 的 topmost にしたいので、目標状態になるよう toggle する。
    bool want_topmost = compact_mode_ ? true : saved_topmost_;
    topmost_ = !want_topmost;         // toggle 呼出で反転して目標値になる
    toggle_always_on_top();

    sync_view_menu_toggles();
    redraw();
}

// View メニューのトグル項目 (FL_MENU_TOGGLE) のチェック状態を
// 現在の g_ 変数に合わせる。起動時・Prefs 適用後・プログラム的な
// トグル時に呼ぶ。メニュー経由のクリックでは FLTK が自動で切替える
// ので呼ばなくてよいが、呼んでも冪等。
void MainWindow::sync_view_menu_toggles() {
    Fl_Menu_Item *items = (Fl_Menu_Item *)menu_->menu();
    auto set_check = [&](int idx, bool on) {
        if (idx < 0) return;
        if (on) items[idx].set(); else items[idx].clear();
    };
    set_check(mi_rowlines_,      g_show_rowlines);
    set_check(mi_thousands_,     g_sep_thousands);
    set_check(mi_hexsep_,        g_sep_hex);
    set_check(mi_e_notation_,    g_fmt_settings.e_notation);
    set_check(mi_auto_complete_, g_input_auto_completion);
    set_check(mi_tray_,          g_tray_icon);
    set_check(mi_topmost_,       topmost_);
    set_check(mi_compact_,       compact_mode_);
    for (int i = 0; i < COLOR_PRESET_COUNT; i++)
        set_check(mi_scheme_[i], i == g_color_preset);
    menu_->redraw();
}

// Zoom 用: フォント再適用 + レイアウト更新
void MainWindow::apply_font_and_refresh() {
    sheet_->apply_font();
    sheet_->redraw();
}

// 通常モード: メニューバーのトップレベルラベル実測幅 + 右側ウィジェット群。
// コンパクトモード: オーバーレイグリップが収まる最小値。
void MainWindow::apply_size_range() {
    int min_w;
    if (compact_mode_) {
        min_w = GRIP_SZ * 2 + 40;
    } else {
        fl_font(menu_->textfont(), menu_->textsize());
        int menu_min_w = 0;
        int depth = 0;
        for (int i = 0; i < menu_->size(); i++) {
            const Fl_Menu_Item &it = menu_->menu()[i];
            if (!it.label()) { if (depth > 0) depth--; continue; }
            if (depth == 0) menu_min_w += (int)fl_width(it.label()) + 16;
            if (it.flags & FL_SUBMENU) depth++;
        }
        min_w = menu_min_w + BTN_W * 2 + COMPACT_W + PIN_W + PAD + CHOICE_W;
    }
    int min_h = MENU_H + PAD * 2 + 40;
    size_range(min_w, min_h);
}

void MainWindow::save_prefs() {
    if (!g_remember_position) return;
    AppPrefs prefs;
    prefs.set_int("geometry_valid", 1);
    if (compact_mode_) {
        // compact 中に終了: 通常モードの x/y/w/h は入れ替え前の saved_*、
        // compact_* は現在の値を書く (通常モードの方を現在値で潰さない)。
        prefs.set_int("x", saved_x_);
        prefs.set_int("y", saved_y_);
        prefs.set_int("w", saved_w_);
        prefs.set_int("h", saved_h_);
        prefs.set_int("compact_geometry_valid", 1);
        prefs.set_int("compact_x", x());
        prefs.set_int("compact_y", y());
        prefs.set_int("compact_w", w());
        prefs.set_int("compact_h", h());
    } else {
        prefs.set_int("x", x());
        prefs.set_int("y", y());
        prefs.set_int("w", w());
        prefs.set_int("h", h());
        if (compact_geometry_valid_) {
            prefs.set_int("compact_geometry_valid", 1);
            prefs.set_int("compact_x", compact_x_);
            prefs.set_int("compact_y", compact_y_);
            prefs.set_int("compact_w", compact_w_);
            prefs.set_int("compact_h", compact_h_);
        }
    }
}

void MainWindow::load_compact_geometry() {
    AppPrefs prefs;
    if (!prefs.get_int("compact_geometry_valid", 0)) return;
    int cw = prefs.get_int("compact_w", 0);
    int ch = prefs.get_int("compact_h", 0);
    if (cw < 120 || ch < 60) return;  // 破損値ガード
    compact_geometry_valid_ = true;
    compact_x_ = prefs.get_int("compact_x", 0);
    compact_y_ = prefs.get_int("compact_y", 0);
    compact_w_ = cw;
    compact_h_ = ch;
}

void MainWindow::hide() {
    save_prefs();
    Fl_Double_Window::hide();
}

// ---- WM close 横取り ----
void MainWindow::close_cb(Fl_Widget *w, void *data) {
    auto *win = static_cast<MainWindow *>(data);
    if (win->tray_active_) {
        // トレイに隠す (ウィンドウを閉じない)
        plat_window_toggle(win, true);
        return;
    }
    win->hide();
}

// ---- トレイ/ホットキー管理 ----
void MainWindow::setup_tray() {
    if (g_tray_icon) {
        TrayCallbacks cb;
        cb.on_open   = [this]() {
            // 常に表示+前面に (Calctus 準拠)。トグルはホットキーの役割。
            Fl::add_timeout(0.0, [](void *d) {
                static_cast<MainWindow *>(d)->show_and_activate();
            }, this);
        };
        cb.on_exit   = [this]() {
            // Windows: TrackPopupMenu → RemoveWindowSubclass → exit() の再入を避ける
            Fl::add_timeout(0.0, [](void *d) {
                auto *win = static_cast<MainWindow *>(d);
                win->teardown_tray();
                win->save_prefs();
                exit(0);
            }, this);
            Fl::awake();  // ウィンドウ非表示でもタイムアウトを確実に発火させる
        };
        cb.on_hotkey = [this]() {
            Fl::add_timeout(0.0, [](void *d) {
                static_cast<MainWindow *>(d)->toggle_visibility();
            }, this);
            Fl::awake();  // ウィンドウ非表示でもタイムアウトを確実に発火させる
        };
        tray_active_ = plat_tray_create(this, cb);
    }

    if (g_hotkey_enabled) {
        int mods = 0;
        if (g_hotkey_alt)   mods |= PMOD_ALT;
        if (g_hotkey_ctrl)  mods |= PMOD_CTRL;
        if (g_hotkey_shift) mods |= PMOD_SHIFT;
        if (g_hotkey_win)   mods |= PMOD_WIN;
        plat_hotkey_register(mods, g_hotkey_keycode);
    }

    // Linux: ホットキーポーリング開始
    if (g_hotkey_enabled)
        Fl::add_timeout(0.05, hotkey_poll_cb, this);
}

void MainWindow::teardown_tray() {
    Fl::remove_timeout(hotkey_poll_cb, this);
    plat_hotkey_unregister();
    if (tray_active_) {
        plat_tray_destroy();
        tray_active_ = false;
    }
}

void MainWindow::apply_tray_settings() {
    bool was_tray = tray_active_;
    teardown_tray();
    setup_tray();

    // トレイが無効化された場合、ウィンドウが非表示ならば表示する
    // (非表示 + トレイなし = ゾンビ状態の防止)
    if (was_tray && !tray_active_ && !visible()) {
        show();
    }
}

void MainWindow::toggle_visibility() {
    plat_window_toggle(this, tray_active_);
}

void MainWindow::show_and_activate() {
    // 常にウィンドウを表示+前面+フォーカス (Calctus 準拠)
    plat_window_raise(this);
}

void MainWindow::hotkey_poll_cb(void *data) {
    plat_hotkey_poll();
    Fl::repeat_timeout(0.05, hotkey_poll_cb, data);
}

bool MainWindow::should_keep_running() {
    if (!tray_active_) return false;

    // トレイマネージャーが死んだ場合 (Explorer 再起動等) のゾンビ防止:
    // プラットフォーム側でトレイが消えていたらウィンドウを復帰して終了
    if (!plat_tray_is_active()) {
        tray_active_ = false;
        show();
        return false;
    }
    return true;
}

void MainWindow::choice_cb(Fl_Widget *w, void *data) {
    auto *win = static_cast<MainWindow *>(data);
    int idx = static_cast<Fl_Choice *>(w)->value();
    if (idx < 0 || idx >= FMT_COUNT) return;
    win->sheet_->apply_fmt(FMT_DEFS[idx].func_name);
    // apply_fmt → commit → row_change_cb で choice も更新される
}
