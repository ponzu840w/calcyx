// calcyx — メインエントリポイント

#include <FL/Fl.H>
#include <FL/fl_ask.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_PNG_Image.H>
#include "MainWindow.h"
#include "app_prefs.h"
#include "colors.h"
#include "settings_globals.h"
#include "crash_handler.h"
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <FL/Fl_Int_Input.H>

// ---- FcPatternAddString フック (exp/font-debug ブランチ専用) ----
// libfontconfig に FcPatternAddString を直接渡して、FLTK が
// "family" キーに何の文字列を流しているかをトレースする。
// --font-trace / --font-diag / --font-probe 時のみ標準エラーに出力。
static bool s_fc_trace_enabled = false;
extern "C" int FcPatternAddString(void *p, const char *object,
                                  const unsigned char *s) {
    typedef int (*real_t)(void *, const char *, const unsigned char *);
    static real_t real = nullptr;
    if (!real) real = (real_t)dlsym(RTLD_NEXT, "FcPatternAddString");
    if (s_fc_trace_enabled && object && s &&
        (strcmp(object, "family") == 0 || strcmp(object, "FC_FAMILY") == 0)) {
        fprintf(stderr, "[FcPatternAddString] %s = \"%s\"\n", object, s);
    }
    return real ? real(p, object, s) : 0;
}

// ---- フォント診断コード (exp/font-debug ブランチ専用) ----
// `--font-diag` 起動で fl_fonts スロット名・g_font_id・ロード済みフォント
// ファイルなどを標準出力にダンプする。Xft を触らずに fontconfig の
// 様子も CLI からざっくり見られるようにしている。
static void dump_loaded_fonts_from_proc() {
    FILE *fp = fopen("/proc/self/maps", "r");
    if (!fp) { printf("  (maps not available)\n"); return; }
    char line[4096];
    std::string last;
    while (fgets(line, sizeof(line), fp)) {
        const char *p = strstr(line, "/");
        if (!p) continue;
        const char *ext[] = {".pfb", ".pfa", ".ttf", ".otf", ".ttc", ".afm", nullptr};
        bool hit = false;
        for (int i = 0; ext[i]; i++) if (strstr(p, ext[i])) { hit = true; break; }
        if (!hit) continue;
        std::string path = p;
        size_t nl = path.find('\n');
        if (nl != std::string::npos) path.erase(nl);
        if (path == last) continue;
        printf("  %s", path.c_str());
        printf("\n");
        last = path;
    }
    fclose(fp);
}

static void font_diag_dump() {
    printf("=========================================================\n");
    printf("calcyx font diagnostic dump (exp/font-debug)\n");
    printf("=========================================================\n\n");

    printf("[1] Built-in FLTK font slots (0..15) BEFORE our set_font override:\n");
    const char *builtin_label[16] = {
        "FL_HELVETICA", "FL_HELVETICA_BOLD", "FL_HELVETICA_ITALIC", "FL_HELVETICA_BOLD_ITALIC",
        "FL_COURIER", "FL_COURIER_BOLD", "FL_COURIER_ITALIC", "FL_COURIER_BOLD_ITALIC",
        "FL_TIMES", "FL_TIMES_BOLD", "FL_TIMES_ITALIC", "FL_TIMES_BOLD_ITALIC",
        "FL_SYMBOL", "FL_SCREEN", "FL_SCREEN_BOLD", "FL_ZAPF_DINGBATS",
    };
    for (int i = 0; i < 16; i++) {
        const char *n = Fl::get_font((Fl_Font)i);
        int attr = 0;
        const char *pretty = Fl::get_font_name((Fl_Font)i, &attr);
        printf("  slot %2d %-24s raw='%s'  pretty='%s' attr=%d\n",
               i, builtin_label[i], n ? n : "(null)", pretty ? pretty : "(null)", attr);
    }

    printf("\n[2] Applying Fl::set_font(FL_COURIER, ' monospace') ...\n");
    Fl::set_font(FL_COURIER,      " monospace");
    Fl::set_font(FL_COURIER_BOLD, "Bmonospace");
    printf("  FL_COURIER raw='%s' pretty='%s'\n",
           Fl::get_font(FL_COURIER), Fl::get_font_name(FL_COURIER, nullptr));

    printf("\n[3] Calling settings_init_defaults + settings_load ...\n");
    settings_init_defaults();
    settings_load();
    printf("  g_font_id = %d\n", g_font_id);
    printf("  fl_fonts[g_font_id] raw='%s' pretty='%s'\n",
           Fl::get_font((Fl_Font)g_font_id),
           Fl::get_font_name((Fl_Font)g_font_id, nullptr));

    printf("\n[4] Enumerating system fonts via Fl::set_fonts(nullptr) ...\n");
    Fl_Font n = Fl::set_fonts(nullptr);
    printf("  total slots: %d\n", (int)n);
    printf("  slots matching 'courier' (case-insensitive):\n");
    for (Fl_Font i = 0; i < n; i++) {
        int attr = 0;
        const char *pretty = Fl::get_font_name(i, &attr);
        if (!pretty) continue;
        std::string s = pretty;
        for (auto &c : s) c = (char)tolower((unsigned char)c);
        if (s.find("courier") != std::string::npos || s.find("mono") != std::string::npos)
            printf("    slot %3d pretty='%s' raw='%s' attr=%d\n",
                   (int)i, pretty, Fl::get_font((Fl_Font)i), attr);
    }

    printf("\n[5] Loaded font files from /proc/self/maps BEFORE rendering:\n");
    dump_loaded_fonts_from_proc();

    printf("\n[6] Forcing a draw of FL_COURIER / g_font_id (FcPatternAddString trace on stderr) ...\n");
    fflush(stdout);
    s_fc_trace_enabled = true;
    Fl_Double_Window *w = new Fl_Double_Window(10, 10, "probe");
    w->begin();
    Fl_Int_Input *dummy = new Fl_Int_Input(0, 0, 10, 10);
    dummy->textfont((Fl_Font)g_font_id);
    dummy->textsize(13);
    w->end();
    w->show();
    Fl::check();
    fl_font((Fl_Font)g_font_id, 13);
    (void)fl_width("M");  // force metric query
    fl_font(FL_COURIER, 13);
    (void)fl_width("M");
    Fl::check();
    s_fc_trace_enabled = false;
    fflush(stderr);

    printf("\n[7] Loaded font files AFTER rendering:\n");
    dump_loaded_fonts_from_proc();

    printf("\n[8] Environment:\n");
    const char *envs[] = {"LANG", "LC_ALL", "DISPLAY", "XDG_SESSION_TYPE",
                          "XDG_CURRENT_DESKTOP", "GDK_BACKEND", "FC_DEBUG", nullptr};
    for (int i = 0; envs[i]; i++)
        printf("  %s = %s\n", envs[i], getenv(envs[i]) ? getenv(envs[i]) : "(unset)");

    printf("\n[9] fc-match 'monospace' / 'Courier' / ' monospace' (via popen):\n");
    const char *queries[] = {"monospace", "Courier", " monospace", nullptr};
    for (int i = 0; queries[i]; i++) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "fc-match '%s' 2>&1", queries[i]);
        FILE *fp = popen(cmd, "r");
        if (!fp) continue;
        char buf[512];
        if (fgets(buf, sizeof(buf), fp))
            printf("  '%s' -> %s", queries[i], buf);
        pclose(fp);
    }

    w->hide();
    delete w;
    printf("\n=== END OF DUMP ===\n");
}
// ---- 診断コードここまで ----

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
    // --font-diag : フォント診断モード (exp/font-debug ブランチ専用)
    if (argc >= 2 && strcmp(argv[1], "--font-diag") == 0) {
        Fl::scheme("gtk+");
        Fl::visual(FL_DOUBLE | FL_INDEX);
        font_diag_dump();
        return 0;
    }

    // --font-trace : 本番と同じ起動パスをたどり FcPatternAddString を
    // 全部標準エラーに流しながら起動する。2秒後に自動終了。
    bool font_trace = (argc >= 2 && strcmp(argv[1], "--font-trace") == 0);
    if (font_trace) s_fc_trace_enabled = true;

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

    // Linux では Courier がビットマップ時代の Adobe Type 1 に解決されるので
    // fontconfig 経由の monospace に差し替える (CJK はフォールバックで補完)。
#if !defined(_WIN32) && !defined(__APPLE__)
    Fl::set_font(FL_COURIER,      " monospace");
    Fl::set_font(FL_COURIER_BOLD, "Bmonospace");
#endif

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
    {
        Fl_PNG_Image icon_img(CALCYX_ICON_PNG);
        if (icon_img.fail() == 0)
            win.icon(&icon_img);
    }
#endif

    win.show(argc, argv);

    // 起動時のデフォルト Always on Top を適用
    // (ユーザーはメニューまたはピンボタンで常時切替可能)
    if (g_start_topmost) {
        // fl_xid が有効になってから適用するため最初の wait を挟む
        Fl::check();
        win.toggle_always_on_top();
    }

    // ベンチマーク用: 環境変数 CALCYX_BENCH_EXIT_MS が設定されていれば、
    // イベントループ到達後に指定ms後で自動終了する。scripts/bench.sh 用。
    if (const char *ms = std::getenv("CALCYX_BENCH_EXIT_MS")) {
        double sec = atof(ms) / 1000.0;
        if (sec > 0.0)
            Fl::add_timeout(sec, [](void *){ std::exit(0); });
    }

    // --font-trace: メインウィンドウ描画後にロード済みフォントをダンプして終了
    if (font_trace) {
        Fl::check();
        Fl::add_timeout(1.5, [](void *){
            fprintf(stderr, "\n=== Loaded font files after main window shown ===\n");
            dump_loaded_fonts_from_proc();
            fprintf(stderr, "g_font_id=%d raw='%s' pretty='%s'\n",
                    g_font_id, Fl::get_font((Fl_Font)g_font_id),
                    Fl::get_font_name((Fl_Font)g_font_id, nullptr));
            std::exit(0);
        });
    }

    // Fl::run() はウィンドウが全て非表示になると終了するが、
    // トレイ常駐中はウィンドウ非表示でもイベントループを継続する必要がある。
    for (;;) {
        if (Fl::first_window()) {
            // ウィンドウあり: 通常の FLTK イベント待ち
            if (Fl::wait() < 0) break;
        } else {
            // ウィンドウなし: トレイ常駐中ならスリープしながら待機
            if (!win.should_keep_running()) break;
            Fl::wait(0.2);  // 200ms 間隔でポーリング (CPU 暴走防止)
        }
    }
    return 0;
}
