// calcyx — メインエントリポイント

#include <FL/Fl.H>
#include "MainWindow.h"

int main(int argc, char **argv) {
    Fl::scheme("gtk+");
    Fl::visual(FL_DOUBLE | FL_INDEX);

    MainWindow win(520, 600, "calcyx");
    win.show(argc, argv);

    return Fl::run();
}
