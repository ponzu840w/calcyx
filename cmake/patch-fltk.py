#!/usr/bin/env python3
"""FLTK 1.4.4 パッチ (deps.cmake の PATCH_COMMAND から呼ばれる)

バグ修正:
  - menuwindow::autoscroll() がメニューバーウィンドウ (itemheight==0) を
    誤ってリポジションするバグを修正。マルチモニタ環境でメニューがずれる
    原因だった。
  - menuwindow の背景色が `Fl::scheme()` 有効時に常に FL_GRAY
    (= FL_BACKGROUND_COLOR) で塗られ、Fl_Menu_Bar::color() が無視される
    バグを修正。calcyx は scheme="gtk+" + ダーク配色を使うため、
    ドロップダウンがシート背景と同色になり「透けて見える」ように
    なっていた。 button->color() を常に優先する。
  - macOS でメニューが意図的に setAlphaValue:0.97 で 3% 透過されている
    のを完全 opaque に変更。 native menu 風の演出だが calcyx のダーク
    配色ではシート内容が透けて見えてしまうため。
  - FLTK の modal_window_level() が NSStatusWindowLevel (25) を返す
    のを NSModalPanelWindowLevel (8) に変更。 macOS 26 (Tahoe) では
    NSStatusWindowLevel に上げたウィンドウに Liquid Glass 系の合成
    エフェクトが自動付加され、 外部 (1x) ディスプレイで文字が滲む。
    通常の modal dialog なら NSModalPanelWindowLevel が本来適切。
  - Windows の OEM キー (VK_OEM_1 / VK_OEM_PLUS など) の FLTK keysym 変換
    が US 配列固定のテーブル (vktab) を使っており、JIS キーボードだと物理
    キーと FLTK keysym がずれる (例: JIS `;` キーが FLTK keysym `=` で
    届く → メニュー "Ctrl+=" のショートカットが物理的には Ctrl+; で発火)。
    MapVirtualKeyW(vk, MAPVK_VK_TO_CHAR) で現在のキーボード配列の文字を
    取得し override する。

パフォーマンス修正 (Windows):
  - メニューバー項目間を移動する度に menuwindow (HWND) と menutitle (HWND)
    を destroy/create していた挙動を修正。rebind() メソッドで既存の
    Fl_Window を流用し、CreateWindowExW/DestroyWindow の往復 (~50ms/回)
    を削減する。
  - さらに menuwindow オフスクリーン退避プールを導入し、深さが減ってから
    増える遷移 (例: Samples を閉じて Color Scheme を開く) でも HWND を
    使い回す。プールは pulldown() 1 セッション内でのみ有効 (pulldown
    終了時に drain)。pulldown 外で pool に残った menuwindow が Windows
    メッセージを受けると pp (pulldown の file-static 状態) が無効なため
    クラッシュするので、敢えて drain する。
  - 上流 FLTK (branch-1.4 / master) には未修正。

デバッグログ (Debug ビルドのみ):
  - MDBG マクロで fltk-menu-debug.log に Release 時無効のトレースを残す。
"""
import sys, os

# ===========================================================================
# 1) Fl_Menu.cxx — メニュー関連バグ/パフォーマンス修正
# ===========================================================================
path = os.path.join(sys.argv[1], "src", "Fl_Menu.cxx")
with open(path, "r") as f:
    src = f.read()

if "s_calcyx_mw_pool" in src:
    print("Already patched Fl_Menu.cxx, skipping")
    menu_already_patched = True
else:
    menu_already_patched = False

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
# 2b) menuwindow 背景色: scheme 有効時も button->color() を尊重する
#     元: color(button && !Fl::scheme() ? button->color() : FL_GRAY);
#     新: color(button ? button->color() : FL_GRAY);
#     ctor 側 (1 箇所) と、後続で挿入する rebind() の中 (1 箇所) の両方が
#     同じ判定をするので置換は count 制限なしで全置換する。
# ---------------------------------------------------------------------------
src = src.replace(
    'color(button && !Fl::scheme() ? button->color() : FL_GRAY);',
    'color(button ? button->color() : FL_GRAY);')

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

# 3c) menuwindow::rebind 実装 + オフスクリーン退避プール
#     (~menuwindow の直前に追加)。ctor 本体 (361-501 付近) のレイアウト
#     計算を手動で複製している。FLTK を更新する際は ctor と同期する必要が
#     ある。
#
#     プールは pulldown() 1 セッション内でのみ有効 (pulldown 終了時に
#     drain)。持続させるとメニュー閉じ後に pool 内 menuwindow が
#     Windows メッセージを受信して handle() が無効な pp を参照しクラッシュ。
MENUWINDOW_REBIND_AND_POOL = r'''
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
  color(button ? button->color() : FL_GRAY);
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

// ---- calcyx: offscreen menuwindow pool (Windows perf) ----
// 閉じた submenu を即 delete する代わりに offscreen へ park してプールに
// 退避する。別深さで新たに submenu を開くときにプールから取り出し rebind
// することで HWND 使い回しを実現する。menubar (itemheight==0) は除外。
// プールは pulldown() 1 セッション内でのみ有効で、pulldown() 終了時に
// drain する。持続させると menuwindow::handle() が無効な pp を参照して
// クラッシュするため。
#if defined(_WIN32)
#define CALCYX_MW_POOL_CAP 4
static menuwindow *s_calcyx_mw_pool[CALCYX_MW_POOL_CAP];
static int s_calcyx_mw_pool_n = 0;

static bool calcyx_mw_pool_push(menuwindow *w) {
  if (!w || w->itemheight == 0) return false;
  if (s_calcyx_mw_pool_n >= CALCYX_MW_POOL_CAP) return false;
  if (w->title) w->title->resize(-30000, -30000, 1, 1);
  w->resize(-30000, -30000, 1, 1);
  s_calcyx_mw_pool[s_calcyx_mw_pool_n++] = w;
  MDBG("[pool#push] n=%d w=%p\n", s_calcyx_mw_pool_n, (void*)w);
  return true;
}

static menuwindow *calcyx_mw_pool_pop() {
  if (s_calcyx_mw_pool_n == 0) return NULL;
  menuwindow *w = s_calcyx_mw_pool[--s_calcyx_mw_pool_n];
  MDBG("[pool#pop ] n=%d w=%p\n", s_calcyx_mw_pool_n, (void*)w);
  return w;
}

static void calcyx_mw_pool_drain() {
  if (s_calcyx_mw_pool_n) MDBG("[pool#drain] n=%d\n", s_calcyx_mw_pool_n);
  while (s_calcyx_mw_pool_n > 0) delete s_calcyx_mw_pool[--s_calcyx_mw_pool_n];
}
#endif

'''
src = src.replace(
    'menuwindow::~menuwindow() {',
    MENUWINDOW_REBIND_AND_POOL + 'menuwindow::~menuwindow() {',
    1)

# 3d) pulldown 側の delete サイトをプール対応に差し替え。
#
#     A) line ~1290「the menu is already up」— 余分な深さを trim
#     B) line ~1292「delete all old + create new」— 主たる再利用サイト
#     C) line ~1320「!m->submenu()」— 非サブメニュー項目ホバー
#     E) pulldown 終了時 — 残りを drain

# del#A: trim
src = src.replace(
    '      } else if (pp.nummenus > pp.menu_number+1 &&\n'
    '                 pp.p[pp.menu_number+1]->menu == menutable) {\n'
    '        // the menu is already up:\n'
    '        while (pp.nummenus > pp.menu_number+2) delete pp.p[--pp.nummenus];\n'
    '        pp.p[pp.nummenus-1]->set_selected(-1);',

    '      } else if (pp.nummenus > pp.menu_number+1 &&\n'
    '                 pp.p[pp.menu_number+1]->menu == menutable) {\n'
    '        // the menu is already up:\n'
    '        while (pp.nummenus > pp.menu_number+2) {\n'
    '          menuwindow *_old = pp.p[--pp.nummenus];\n'
    '#if defined(_WIN32)\n'
    '          if (calcyx_mw_pool_push(_old)) continue;\n'
    '#endif\n'
    '          delete _old;\n'
    '        }\n'
    '        pp.p[pp.nummenus-1]->set_selected(-1);',
    1)

# del#B: main reuse site
src = src.replace(
    '      } else {\n'
    '        // delete all the old menus and create new one:\n'
    '        while (pp.nummenus > pp.menu_number+1) delete pp.p[--pp.nummenus];\n'
    '        pp.p[pp.nummenus++]= new menuwindow(menutable, nX, nY,\n'
    '                                          title?1:0, 0, 0, title, 0, menubar,\n'
    '                                            (title ? 0 : cw.x()) );',

    '      } else {\n'
    '        // calcyx: delete/park old menus, then reuse a pooled menuwindow\n'
    '        // if available (Windows perf).\n'
    '        while (pp.nummenus > pp.menu_number+1) {\n'
    '          menuwindow *_old = pp.p[--pp.nummenus];\n'
    '#if defined(_WIN32)\n'
    '          if (calcyx_mw_pool_push(_old)) continue;\n'
    '#endif\n'
    '          delete _old;\n'
    '        }\n'
    '        menuwindow *_reuse = NULL;\n'
    '#if defined(_WIN32)\n'
    '        _reuse = calcyx_mw_pool_pop();\n'
    '#endif\n'
    '        if (_reuse) {\n'
    '          MDBG("[reuse] rebind submenu (avoid HWND churn)\\n");\n'
    '          _reuse->rebind(menutable, nX, nY,\n'
    '                         title?1:0, 0, 0, title, 0, menubar,\n'
    '                         (title ? 0 : cw.x()));\n'
    '          pp.p[pp.nummenus++] = _reuse;\n'
    '        } else {\n'
    '          MDBG("[new#submenu] create new menuwindow\\n");\n'
    '          pp.p[pp.nummenus++]= new menuwindow(menutable, nX, nY,\n'
    '                                            title?1:0, 0, 0, title, 0, menubar,\n'
    '                                              (title ? 0 : cw.x()) );\n'
    '        }',
    1)

# del#C: !m->submenu() branch
src = src.replace(
    '    } else { // !m->submenu():\n'
    '      while (pp.nummenus > pp.menu_number+1) delete pp.p[--pp.nummenus];',

    '    } else { // !m->submenu():\n'
    '      while (pp.nummenus > pp.menu_number+1) {\n'
    '        menuwindow *_old = pp.p[--pp.nummenus];\n'
    '#if defined(_WIN32)\n'
    '        if (calcyx_mw_pool_push(_old)) continue;\n'
    '#endif\n'
    '        delete _old;\n'
    '      }',
    1)

# del#E: pulldown 終了時にプールを drain。持続させると menuwindow::handle()
# が file-static pp の無効状態を参照してクラッシュするため必須。
src = src.replace(
    '  while (pp.nummenus>1) delete pp.p[--pp.nummenus];\n'
    '  mw.hide();',

    '  while (pp.nummenus>1) delete pp.p[--pp.nummenus];\n'
    '#if defined(_WIN32)\n'
    '  calcyx_mw_pool_drain();\n'
    '#endif\n'
    '  mw.hide();',
    1)

with open(path, "w") as f:
    f.write(src)

print("Patched successfully:", path)

# ===========================================================================
# 2) Fl_win32.cxx — OEM キーの配列対応 (JIS キーボード対策)
# ===========================================================================
# FLTK の ms2fltk() は US 配列固定の vktab で VK→keysym を変換するため、
# JIS キーボードだと物理キーと keysym がずれる (例: 物理 `;` キーは
# VK_OEM_PLUS=0xbb で届くが vktab は '=' に変換 → メニュー "Ctrl+=" の
# ショートカットが物理 Ctrl+; で発火する)。
# ms2fltk() の戻り値を MapVirtualKeyW(vk, MAPVK_VK_TO_CHAR) で現在の
# キーボード配列の文字に上書きする。OEM 領域だけを対象にし、英数字や
# Function キーには触らない。
path = os.path.join(sys.argv[1], "src", "Fl_win32.cxx")
with open(path, "r") as f:
    src = f.read()

if "calcyx: layout-aware OEM" in src:
    print("Already patched Fl_win32.cxx, skipping")
else:
    OEM_OVERRIDE = (
        "        // save the keysym until we figure out the characters:\n"
        "        Fl::e_keysym = Fl::e_original_keysym = ms2fltk(wParam, lParam & (1 << 24));\n"
        "        // ---- calcyx: layout-aware OEM key remap (JIS keyboard support) ----\n"
        "        // FLTK の vktab は US 配列固定で OEM_xxx を keysym 化するため、JIS など\n"
        "        // 異配列だと物理キーとメニュー表示がずれる。MapVirtualKeyW で現在の配列の\n"
        "        // 文字に変換して上書き。Shift/AltGr 修飾を含めない base 文字で取得する\n"
        "        // (FLTK shortcut は base キャラクタ + 修飾 state でマッチするため)。\n"
        "        if ((wParam >= 0xba && wParam <= 0xc0) || (wParam >= 0xdb && wParam <= 0xdf) || wParam == 0xe2) {\n"
        "          UINT _ch = MapVirtualKeyW((UINT)wParam, MAPVK_VK_TO_CHAR);\n"
        "          if (_ch && _ch < 0x80 && _ch >= 0x20) {\n"
        "            int _c = (int)_ch;\n"
        "            if (_c >= 'A' && _c <= 'Z') _c += 32; // FLTK keysym は小文字\n"
        "            Fl::e_keysym = Fl::e_original_keysym = _c;\n"
        "          }\n"
        "        }\n"
    )
    needle = (
        "        // save the keysym until we figure out the characters:\n"
        "        Fl::e_keysym = Fl::e_original_keysym = ms2fltk(wParam, lParam & (1 << 24));\n"
    )
    if needle not in src:
        sys.stderr.write("patch-fltk.py: Fl_win32.cxx target line not found\n")
        sys.exit(1)
    src = src.replace(needle, OEM_OVERRIDE, 1)

    with open(path, "w") as f:
        f.write(src)

    print("Patched successfully:", path)

# ===========================================================================
# 3) Fl_cocoa.mm — macOS メニューの 3% 透過を無効化
# ===========================================================================
# FLTK は macOS で menu_window() を 0.97 alpha にして native menu 風の
# 透過感を出しているが、 calcyx のダーク配色 (otaku-black 等) ではシート
# 行のテキストや色がはっきり透けて見えてしまう。 完全 opaque に倒す。
path = os.path.join(sys.argv[1], "src", "Fl_cocoa.mm")
with open(path, "r") as f:
    src = f.read()

if "calcyx: opaque menu windows" in src:
    print("Already patched Fl_cocoa.mm (opaque), skipping")
else:
    needle = (
        "  if(w->menu_window()) { // make menu windows slightly transparent\n"
        "    [cw setAlphaValue:0.97];\n"
        "  }\n"
    )
    replacement = (
        "  // calcyx: opaque menu windows (FLTK 既定の 0.97 透過は\n"
        "  // ダーク配色でシート内容が透けて見えるため無効化)\n"
        "  if (false && w->menu_window()) {\n"
        "    [cw setAlphaValue:0.97];\n"
        "  }\n"
    )
    if needle not in src:
        sys.stderr.write("patch-fltk.py: Fl_cocoa.mm target line not found\n")
        sys.exit(1)
    src = src.replace(needle, replacement, 1)

# 2) FLView に viewDidChangeBackingProperties を追加。
#    macOS は backing scale 変更時にこのメソッドを呼ぶが、 FLTK は
#    windowDidMove 経由でしか検知しておらず、 マルチディスプレイ跨ぎで
#    ダイアログがぼやけたまま残留するバグの原因。
# 2c) modal_window_level の NSStatusWindowLevel を NSModalPanelWindowLevel に
#     差し替える。 NSStatusWindowLevel は status bar アプリ用の特殊レベル
#     (= 25) で、 macOS 26 (Tahoe) ではこのレベルのウィンドウに Liquid
#     Glass 系の合成エフェクトが自動付加され、 外部 1x ディスプレイで
#     文字が滲む。 通常の modal dialog は NSModalPanelWindowLevel (= 8)
#     が本来適切。
if "calcyx: modal level fix" in src:
    print("Already patched Fl_cocoa.mm (modal level), skipping")
else:
    LEVEL_NEEDLE = (
        "  level = max_normal_window_level();\n"
        "  if (level < NSStatusWindowLevel)\n"
        "    return NSStatusWindowLevel;\n"
    )
    LEVEL_REPLACE = (
        "  // ---- calcyx: modal level fix ----\n"
        "  // NSStatusWindowLevel は macOS 26 で Liquid Glass 効果を誘発し、\n"
        "  // 外部 1x ディスプレイで modal dialog の文字が滲む。\n"
        "  // 通常 modal は NSModalPanelWindowLevel が適切。\n"
        "  level = max_normal_window_level();\n"
        "  if (level < NSModalPanelWindowLevel)\n"
        "    return NSModalPanelWindowLevel;\n"
    )
    if LEVEL_NEEDLE not in src:
        sys.stderr.write("patch-fltk.py: Fl_cocoa.mm modal_window_level not found\n")
        sys.exit(1)
    src = src.replace(LEVEL_NEEDLE, LEVEL_REPLACE, 1)

with open(path, "w") as f:
    f.write(src)

print("Patched successfully:", path)
