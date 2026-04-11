// calcyx — メインエントリポイント

#include <FL/Fl.H>
#include "MainWindow.h"

#if defined(_WIN32)
#  include <windows.h>
#  define IDI_ICON1 101
#endif

int main(int argc, char **argv) {
    Fl::scheme("gtk+");
    Fl::visual(FL_DOUBLE | FL_INDEX);

    MainWindow win(520, 600, "calcyx");

#if defined(_WIN32)
    // タスクバー・ウィンドウ枠のアイコンを exe リソースから設定
    HICON hicon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
    win.icon(hicon);
#endif

    win.show(argc, argv);

    return Fl::run();
}
