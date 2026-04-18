#!/usr/bin/env python3
"""FLTK 1.4.4 パッチ (deps.cmake の PATCH_COMMAND から呼ばれる)

バグ修正:
  - menuwindow::autoscroll() がメニューバーウィンドウ (itemheight==0) を
    誤ってリポジションするバグを修正。マルチモニタ環境でメニューがずれる
    原因だった。

デバッグログ (Debug ビルドのみ):
  - メニュー座標の計算過程を fltk-menu-debug.log に記録。
"""
import sys, os

path = os.path.join(sys.argv[1], "src", "Fl_Menu.cxx")
with open(path, "r") as f:
    src = f.read()

if "menubar has no scrollable items" in src:
    print("Already patched, skipping")
    sys.exit(0)

# 1) Add debug logging infrastructure (debug builds only)
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

# ======== BUG FIX (always active) ========
# autoscroll() must not reposition the menubar window (itemheight==0).
# The menubar window is never show()'d, so screen_num() returns a wrong
# default (0 instead of the actual screen), leading to bogus work-area
# bounds and a spurious reposition that shifts cw.y() and displaces
# subsequent submenus.  Reproduces on multi-monitor setups when the
# application window is on a non-primary screen.
src = src.replace(
    '// scroll so item i is visible on screen\n'
    'void menuwindow::autoscroll(int n) {',
    '// scroll so item i is visible on screen\n'
    'void menuwindow::autoscroll(int n) {\n'
    '  if (itemheight == 0) return; // menubar has no scrollable items',
    1)

# ======== DEBUG LOGGING (debug builds only, via MDBG) ========

# Log in menuwindow constructor
src = src.replace(
    '  offset_y = 0;\n  int n = (Wp > 0',
    '  offset_y = 0;\n'
    '  MDBG("[mw-ctor] X=%d Y=%d Wp=%d Hp=%d menubar=%d mbt=%d re=%d\\n", X, Y, Wp, Hp, menubar, menubar_title, right_edge);\n'
    '  int n = (Wp > 0',
    1)

# Log screen info
src = src.replace(
    'if (!right_edge || right_edge > scr_x+scr_w) right_edge = scr_x+scr_w;',
    'if (!right_edge || right_edge > scr_x+scr_w) right_edge = scr_x+scr_w;\n'
    '  MDBG("[mw-ctor] screen n=%d scr(%d,%d,%d,%d) re=%d\\n", n, scr_x, scr_y, scr_w, scr_h, right_edge);',
    1)

# Log final position
src = src.replace(
    '  if (m) y(Y); else {y(Y-2); w(1); h(1);}',
    '  MDBG("[mw-ctor] final x=%d y=%d w=%d h=%d\\n", x(), Y, w(), h());\n'
    '  if (m) y(Y); else {y(Y-2); w(1); h(1);}',
    1)

# Log pulldown translation + mouse
src = src.replace(
    '  } else {\n    X += Fl::event_x_root()-Fl::event_x();\n    Y += Fl::event_y_root()-Fl::event_y();\n    menuwindow::parent_ = Fl::first_window();\n  }',
    '  } else {\n    X += Fl::event_x_root()-Fl::event_x();\n    Y += Fl::event_y_root()-Fl::event_y();\n    menuwindow::parent_ = Fl::first_window();\n  }\n'
    '  MDBG("[pulldown] X=%d Y=%d W=%d H=%d menubar=%d mouse=(%d,%d)\\n", X, Y, W, H, menubar, Fl::event_x_root(), Fl::event_y_root());',
    1)

# Log submenu creation
src = src.replace(
    '      } else {\n        // delete all the old menus and create new one:\n        while (pp.nummenus > pp.menu_number+1) delete pp.p[--pp.nummenus];\n        pp.p[pp.nummenus++]= new menuwindow',
    '      } else {\n        // delete all the old menus and create new one:\n        while (pp.nummenus > pp.menu_number+1) delete pp.p[--pp.nummenus];\n'
    '        MDBG("[pulldown] submenu: cw(%d,%d,%d,%d) nX=%d nY=%d mouse=(%d,%d)\\n", cw.x(), cw.y(), cw.w(), cw.h(), nX, nY, Fl::event_x_root(), Fl::event_y_root());\n'
    '        pp.p[pp.nummenus++]= new menuwindow',
    1)

# Log autoscroll reposition (after the early-return guard)
src = src.replace(
    '  Fl_Window_Driver::driver(this)->reposition_menu_window(x(), y()+Y);',
    '  MDBG("[mw-autoscroll] reposition x=%d y=%d->%d itemheight=%d\\n", x(), y(), y()+Y, itemheight);\n'
    '  Fl_Window_Driver::driver(this)->reposition_menu_window(x(), y()+Y);',
    1)

with open(path, "w") as f:
    f.write(src)

print("Patched successfully:", path)
