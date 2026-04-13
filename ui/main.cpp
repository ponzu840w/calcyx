// calcyx — メインエントリポイント

#include <FL/Fl.H>
#include "MainWindow.h"
#include "app_prefs.h"

#if defined(_WIN32)
#  include <windows.h>
#  define IDI_ICON1 101
#endif

int main(int argc, char **argv) {
    Fl::scheme("gtk+");
    Fl::visual(FL_DOUBLE | FL_INDEX);

    // ウィンドウジオメトリを前回終了時の設定から復元
    int wx = 0, wy = 0, ww = 520, wh = 600;
    {
        AppPrefs prefs;
        int valid = prefs.get_int("geometry_valid", 0);
        if (valid) {
            wx = prefs.get_int("x", 0);
            wy = prefs.get_int("y", 0);
            ww = prefs.get_int("w", 520);
            wh = prefs.get_int("h", 600);
            // 最小サイズ検証
            if (ww < 300) ww = 520;
            if (wh < 200) wh = 600;
            // いずれかの画面上にウィンドウが収まるか確認
            bool on_screen = false;
            for (int s = 0; s < Fl::screen_count(); s++) {
                int sx, sy, sw, sh;
                Fl::screen_xywh(sx, sy, sw, sh, s);
                if (wx + ww > sx && wx < sx + sw &&
                    wy + wh > sy && wy < sy + sh) {
                    on_screen = true; break;
                }
            }
            if (!on_screen) { wx = 0; wy = 0; }
        }
    }

    MainWindow win(ww, wh, "calcyx");
    if (wx || wy) win.position(wx, wy);

#if defined(_WIN32)
    // タスクバー・ウィンドウ枠のアイコンを exe リソースから設定
    HICON hicon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
    win.icon(hicon);
#elif !defined(__APPLE__)
    // Linux: WM_CLASS を設定してデスクトップ環境がアイコンを解決できるようにする
    win.xclass("calcyx");
#endif

    win.show(argc, argv);

    return Fl::run();
}
