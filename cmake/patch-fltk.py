#!/usr/bin/env python3
"""FLTK 1.4.4 パッチ (deps.cmake の PATCH_COMMAND から呼ばれる)

バグ修正:
  - menuwindow::autoscroll() がメニューバーウィンドウ (itemheight==0) を
    誤ってリポジションするバグを修正。マルチモニタ環境でメニューがずれる
    原因だった。

パフォーマンス修正 (Windows):
  - メニューバー項目間を移動する度に menuwindow (HWND) と menutitle (HWND)
    を destroy/create していた挙動を修正。rebind() メソッドで既存の
    Fl_Window を流用し、CreateWindowExW/DestroyWindow の往復 (~50ms/回)
    を削減する。
  - 上流 FLTK (branch-1.4 / master) には未修正。

デバッグログ (Debug ビルドのみ):
  - MDBG マクロで fltk-menu-debug.log に Release 時無効のトレースを残す。
"""
import sys, os

path = os.path.join(sys.argv[1], "src", "Fl_Menu.cxx")
with open(path, "r") as f:
    src = f.read()

if "calcyx_reuse_pool" in src:
    print("Already patched, skipping")
    sys.exit(0)

# ---------------------------------------------------------------------------
# 1) Debug logging infrastructure (Release では完全に no-op)
# ---------------------------------------------------------------------------
MARKER1 = '#include "Fl_Screen_Driver.H"'
DEBUG_HEADER = r'''#include "Fl_Screen_Driver.H"

// ---- calcyx: menu-position debug logging (debug builds only) ----
#if defined(_WIN32) && !defined(NDEBUG)
#include <windows.h>
static FILE *_menu_dbg_fp = nullptr;
static FILE *_menu_dbg_open() {
  if (!_menu_dbg_fp) {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    char *bs = strrchr(path, '\\');
    if (bs) *(bs + 1) = '\0';
    strcat(path, "fltk-menu-debug.log");
    _menu_dbg_fp = fopen(path, "a");
    if (_menu_dbg_fp) { fprintf(_menu_dbg_fp, "---- session ----\n"); fflush(_menu_dbg_fp); }
  }
  return _menu_dbg_fp;
}
#define MDBG(...) do { FILE *_f = _menu_dbg_open(); if(_f){fprintf(_f,__VA_ARGS__);fflush(_f);} } while(0)
#else
#define MDBG(...) ((void)0)
#endif
'''
src = src.replace(MARKER1, DEBUG_HEADER, 1)

# ---------------------------------------------------------------------------
# 2) autoscroll() early-return (マルチモニタのメニュー位置ずれ対策)
# ---------------------------------------------------------------------------
src = src.replace(
    '// scroll so item i is visible on screen\n'
    'void menuwindow::autoscroll(int n) {',
    '// scroll so item i is visible on screen\n'
    'void menuwindow::autoscroll(int n) {\n'
    '  if (itemheight == 0) return; // menubar has no scrollable items',
    1)

# ---------------------------------------------------------------------------
# 3) menuwindow / menutitle リサイクル機構 (Windows 向けパフォーマンス)
# ---------------------------------------------------------------------------

# 3a) クラス宣言に rebind() を追加。
#     menuwindow: ctor 本体の再実行、menutitle: 軽い位置+内容更新。
src = src.replace(
    '  bool in_menubar;\n'
    '};\n',
    '  bool in_menubar;\n'
    '  void rebind(int X, int Y, int W, int H, const Fl_Menu_Item* L, bool inbar);\n'
    '};\n',
    1)

src = src.replace(
    '  void autoscroll(int);\n',
    '  void autoscroll(int);\n'
    '  void rebind(const Fl_Menu_Item* m, int X, int Y, int Wp, int Hp,\n'
    '              const Fl_Menu_Item* picked, const Fl_Menu_Item* title,\n'
    '              int menubar = 0, int menubar_title = 0, int right_edge = 0);\n',
    1)

# 3b) menutitle::rebind 実装 (menutitle::menutitle の直後に追加)
MENUTITLE_REBIND = r'''
// ---- calcyx: HWND-preserving title rebind (Windows perf) ----
void menutitle::rebind(int X, int Y, int W, int H, const Fl_Menu_Item* L, bool inbar) {
  menu = L;
  in_menubar = inbar;
  resize(X, Y, W, H);
  redraw();
}
'''
src = src.replace(
    'menutitle::menutitle(int X, int Y, int W, int H, const Fl_Menu_Item* L, bool inbar) :\n'
    '  window_with_items(X, Y, W, H, L) {\n'
    '  in_menubar = inbar;\n'
    '}\n',
    'menutitle::menutitle(int X, int Y, int W, int H, const Fl_Menu_Item* L, bool inbar) :\n'
    '  window_with_items(X, Y, W, H, L) {\n'
    '  in_menubar = inbar;\n'
    '}\n'
    + MENUTITLE_REBIND,
    1)

# 3c) menuwindow::rebind 実装 (~menuwindow の直前に追加)。
#     ctor 本体 (361-501 付近) のレイアウト計算を手動で複製している。
#     FLTK を更新する際は ctor と同期する必要がある。
MENUWINDOW_REBIND = r'''
// ---- calcyx: HWND-preserving submenu rebind (Windows perf) ----
// NOTE: menuwindow::menuwindow (本ファイルの上方) のレイアウト処理を複製して
// いる。FLTK 本体を更新する際は両者を同期する必要がある。
void menuwindow::rebind(const Fl_Menu_Item* m, int X, int Y, int Wp, int Hp,
                        const Fl_Menu_Item* picked, const Fl_Menu_Item* t,
                        int menubar, int menubar_title_arg, int right_edge) {
  menu = m;
  menubartitle = menubar_title_arg;
  origin = NULL;
  offset_y = 0;

  int scr_x, scr_y, scr_w, scr_h;
  int tx = X, ty = Y;
  int n = (Wp > 0 ? Fl::screen_num(X, Y) : -1);
  Fl_Window_Driver::driver(this)->menu_window_area(scr_x, scr_y, scr_w, scr_h, n);
  if (!right_edge || right_edge > scr_x+scr_w) right_edge = scr_x+scr_w;

  if (m) m = m->first();
  drawn_selected = -1;
  if (button) {
    Fl_Boxtype b = button->menu_box();
    if (b==FL_NO_BOX) b = button->box();
    if (b==FL_NO_BOX) b = FL_FLAT_BOX;
    box(b);
  } else {
    box(FL_UP_BOX);
  }
  color(button && !Fl::scheme() ? button->color() : FL_GRAY);
  selected = -1;
  {
    int j = 0;
    if (m) for (const Fl_Menu_Item* m1=m; ; m1 = m1->next(), j++) {
      if (picked) {
        if (m1 == picked) {selected = j; picked = 0;}
        else if (m1 > picked) {selected = j-1; picked = 0; Wp = Hp = 0;}
      }
      if (!m1->text) break;
    }
    numitems = j;
  }

  if (menubar) {
    // menubar 自身の rebind はサポート対象外 (pulldown loop では発生しない)。
    itemheight = 0;
    if (title) { title->hide(); delete title; title = 0; }
    redraw();
    return;
  }

  itemheight = 1;
  int hotKeysw = 0;
  int hotModsw = 0;
  int Wtitle = 0;
  int Htitle = 0;
  if (t) Wtitle = t->measure(&Htitle, button) + 12;
  int W = 0;
  if (m) for (; m->text; m = m->next()) {
    int hh;
    int w1 = m->measure(&hh, button);
    if (hh+Fl::menu_linespacing()>itemheight) itemheight = hh+Fl::menu_linespacing();
    if (m->flags&(FL_SUBMENU|FL_SUBMENU_POINTER)) w1 += FL_NORMAL_SIZE;
    if (w1 > W) W = w1;
    if (m->shortcut_) {
      const char *k, *s = fl_shortcut_label(m->shortcut_, &k);
      if (fl_utf_nb_char((const unsigned char*)k, (int) strlen(k))<=4) {
        w1 = int(fl_width(s, (int) (k-s)));
        if (w1 > hotModsw) hotModsw = w1;
        w1 = int(fl_width(k))+4;
        if (w1 > hotKeysw) hotKeysw = w1;
      } else {
        w1 = int(fl_width(s))+4;
        if (w1 > (hotModsw+hotKeysw)) hotModsw = w1-hotKeysw;
      }
    }
  }
  shortcutWidth = hotKeysw;
  if (selected >= 0 && !Wp) X -= W/2;
  int BW = Fl::box_dx(box());
  W += hotKeysw+hotModsw+2*BW+7;
  if (Wp > W) W = Wp;
  if (Wtitle > W) W = Wtitle;

  if (X < scr_x) X = scr_x;
  if (X > scr_x+scr_w-W) X = scr_x+scr_w-W;
  int newH = (numitems ? itemheight*numitems-4 : 0)+2*BW+3;
  if (selected >= 0) {
    Y = Y+(Hp-itemheight)/2-selected*itemheight-BW;
  } else {
    Y = Y+Hp;
    if (Y+newH>scr_y+scr_h && Y-newH>=scr_y) {
      if (Hp>1) {
        Y = Y-Hp-newH;
      } else if (t) {
        Y = Y-itemheight-newH-Fl::box_dh(box());
      } else {
        Y = Y-newH+itemheight+Fl::box_dy(box());
      }
      if (t) {
        if (menubar_title_arg) {
          Y = Y + Fl::menu_linespacing() - Fl::box_dw(button->box());
        } else {
          Y += 2*Htitle+2*BW+3;
        }
      }
    }
  }
  MDBG("[rebind] X=%d Y=%d W=%d newH=%d items=%d ih=%d t=%p\n",
       X, Y, W, newH, numitems, itemheight, (void*)t);
  if (m) {
    resize(X, Y, W, newH);
  } else {
    resize(X, Y-2, 1, 1);
  }

  // Title handling: reuse existing menutitle if possible (saves 1 HWND per transition).
  if (t) {
    int tX, tY, tW, tH;
    bool t_inbar;
    if (menubar_title_arg) {
      int dy = Fl::box_dy(button->box())+1;
      int ht = button->h()-dy*2;
      tX = tx; tY = ty-ht-dy; tW = Wtitle; tH = ht;
      t_inbar = true;
    } else {
      int dy = 2;
      int ht = Htitle+2*BW+3;
      tX = X; tY = Y-ht-dy; tW = Wtitle; tH = ht;
      t_inbar = false;
    }
    if (title) {
      title->rebind(tX, tY, tW, tH, t, t_inbar);
    } else {
      title = new menutitle(tX, tY, tW, tH, t, t_inbar);
    }
  } else {
    if (title) { title->hide(); delete title; title = 0; }
  }
  redraw();
}

'''
src = src.replace(
    'menuwindow::~menuwindow() {',
    MENUWINDOW_REBIND + 'menuwindow::~menuwindow() {',
    1)

# 3d) pulldown 側の fast-path:
#     「delete all the old menus and create new one」分岐で、末端 1 枚の
#     submenu を rebind して再利用する。条件は「メニュー階層が 1 段だけ
#     差し替わる + 既存 slot が実サブメニュー (itemheight>0)」のみで、
#     title の有無は rebind が内部で吸収する。
#     非 Windows では従来挙動 (delete+new) にフォールバック。
src = src.replace(
    '      } else {\n'
    '        // delete all the old menus and create new one:\n'
    '        while (pp.nummenus > pp.menu_number+1) delete pp.p[--pp.nummenus];\n'
    '        pp.p[pp.nummenus++]= new menuwindow(menutable, nX, nY,\n'
    '                                          title?1:0, 0, 0, title, 0, menubar,\n'
    '                                            (title ? 0 : cw.x()) );',

    '      } else {\n'
    '        // delete all the old menus and create new one:\n'
    '        // calcyx: reuse the top submenu where possible (Windows perf).\n'
    '        menuwindow *calcyx_reuse_pool = NULL;\n'
    '#if defined(_WIN32)\n'
    '        if (pp.nummenus == pp.menu_number + 2 &&\n'
    '            pp.p[pp.nummenus-1]->itemheight > 0) {\n'
    '          calcyx_reuse_pool = pp.p[--pp.nummenus];\n'
    '        }\n'
    '#endif\n'
    '        while (pp.nummenus > pp.menu_number+1) delete pp.p[--pp.nummenus];\n'
    '        if (calcyx_reuse_pool) {\n'
    '          MDBG("[reuse] rebind submenu (avoid HWND churn)\\n");\n'
    '          calcyx_reuse_pool->rebind(menutable, nX, nY,\n'
    '                                    title?1:0, 0, 0, title, 0, menubar,\n'
    '                                    (title ? 0 : cw.x()));\n'
    '          pp.p[pp.nummenus++] = calcyx_reuse_pool;\n'
    '        } else {\n'
    '          MDBG("[new#submenu] create new menuwindow\\n");\n'
    '          pp.p[pp.nummenus++]= new menuwindow(menutable, nX, nY,\n'
    '                                            title?1:0, 0, 0, title, 0, menubar,\n'
    '                                              (title ? 0 : cw.x()) );\n'
    '        }',
    1)

with open(path, "w") as f:
    f.write(src)

print("Patched successfully:", path)
