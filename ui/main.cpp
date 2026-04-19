// calcyx — メインエントリポイント

#include <FL/Fl.H>
#include <FL/fl_ask.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Button.H>
#include <FL/Fl.H>
#include "MainWindow.h"
#include "app_prefs.h"
#include "colors.h"
#include "settings_globals.h"
#include "crash_handler.h"
#include <string>
#include <cstring>
#include <cstdio>

#if defined(_WIN32)
#  include <windows.h>
#  define IDI_ICON1 101
#endif

static void show_crash_dialog(const std::string &content) {

    Fl_Double_Window dlg(520, 380, "calcyx - Crash Report");
    dlg.set_modal();

    Fl_Box label(10, 10, 500, 40,
        "calcyx terminated unexpectedly during the last session.\n"
        "The following crash report was generated:");
    label.align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
    label.labelsize(13);

    Fl_Text_Buffer buf;
    buf.text(content.c_str());

    Fl_Text_Display disp(10, 55, 500, 270);
    disp.buffer(&buf);
    disp.textfont(FL_COURIER);
    disp.textsize(11);

    Fl_Button copy_btn(10, 340, 100, 30, "Copy");
    Fl_Button close_btn(410, 340, 100, 30, "Close");

    copy_btn.callback([](Fl_Widget *, void *data) {
        auto *b = static_cast<Fl_Text_Buffer *>(data);
        const char *txt = b->text();
        if (txt) Fl::copy(txt, (int)strlen(txt), 1);
        free((void *)txt);
    }, &buf);

    close_btn.callback([](Fl_Widget *w, void *) {
        w->window()->hide();
    });

    dlg.end();
    dlg.show();
    while (dlg.shown()) Fl::wait();
}

int main(int argc, char **argv) {
    // --show-crash <path> : クラッシュ直後に別プロセスとして起動された場合
    if (argc >= 3 && strcmp(argv[1], "--show-crash") == 0) {
        FILE *fp = fopen(argv[2], "r");
        if (fp) {
            std::string content;
            char buf[1024];
            while (size_t n = fread(buf, 1, sizeof(buf), fp))
                content.append(buf, n);
            fclose(fp);
            remove(argv[2]);
            Fl::scheme("gtk+");
            show_crash_dialog(content);
        }
        return 0;
    }

    crash_handler_install();

    colors_init_defaults(&g_colors);
    settings_init_defaults();

    Fl::scheme("gtk+");
    Fl::visual(FL_DOUBLE | FL_INDEX);

    // 前回クラッシュしていたらレポートダイアログを表示 (次回起動時のフォールバック)
    std::string crash_log = crash_handler_check();
    if (!crash_log.empty()) {
        show_crash_dialog(crash_log);
    }

    // ウィンドウジオメトリを前回終了時の設定から復元
    int wx = 0, wy = 0, ww = 520, wh = 600;
    settings_load();
    colors_apply_fl_scheme();

    if (g_remember_position) {
        AppPrefs prefs;
        int valid = prefs.get_int("geometry_valid", 0);
        if (valid) {
            wx = prefs.get_int("x", 0);
            wy = prefs.get_int("y", 0);
            ww = prefs.get_int("w", 520);
            wh = prefs.get_int("h", 600);
            if (ww < 300) ww = 520;
            if (wh < 200) wh = 600;
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
    }  // g_remember_position

    MainWindow win(ww, wh, "calcyx");
    if (wx || wy) win.position(wx, wy);

#if defined(_WIN32)
    HICON hicon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
    win.icon(hicon);
#elif !defined(__APPLE__)
    win.xclass("calcyx");
#endif

    win.show(argc, argv);

    return Fl::run();
}
