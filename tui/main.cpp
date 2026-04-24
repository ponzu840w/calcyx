#include <cstdio>
#include <cstring>
#include <string>

#include "TuiApp.h"

static void print_usage(const char *argv0) {
    std::fprintf(stderr,
        "calcyx-tui — Programmers' calculator (TUI)\n"
        "Usage:\n"
        "  %s [--file PATH | -f PATH]\n"
        "  %s --version\n"
        "  %s --help\n",
        argv0, argv0, argv0);
}

int main(int argc, char **argv) {
    std::string file_path;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") {
            print_usage(argv[0]);
            return 0;
        }
        if (a == "--version") {
#ifdef CALCYX_VERSION_FULL
            std::printf("calcyx-tui %s\n", CALCYX_VERSION_FULL);
#else
            std::printf("calcyx-tui (unknown version)\n");
#endif
            return 0;
        }
        if (a == "--file" || a == "-f") {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "error: %s requires an argument\n", a.c_str());
                return 2;
            }
            file_path = argv[++i];
            continue;
        }
        /* 先頭の - がなければ位置引数としてファイル名扱い */
        if (!a.empty() && a[0] != '-') {
            file_path = a;
            continue;
        }
        std::fprintf(stderr, "error: unknown option: %s\n", a.c_str());
        print_usage(argv[0]);
        return 2;
    }

    calcyx::tui::TuiApp app;
    return app.run(file_path);
}
