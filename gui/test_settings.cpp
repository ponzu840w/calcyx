// settings スキーマの往復テスト (init_defaults / save / load の整合)。
// settings_set_path_for_test() で一時ファイルに誘導。

#include "settings_globals.h"
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

static int g_failures = 0;

#define EXPECT(label, cond) \
    do { if (!(cond)) { fprintf(stderr, "FAIL: %s\n", label); g_failures++; } } while (0)

static std::string make_tmp_path() {
    char tmpl[] = "/tmp/calcyx_test_settings_XXXXXX";
    int fd = mkstemp(tmpl);
    if (fd < 0) { perror("mkstemp"); exit(2); }
    close(fd);
    std::string path = tmpl;
    unlink(path.c_str());   // mkstemp は作成するが、初期状態は「未存在」にしておく
    return path;
}

static void write_file(const std::string &path, const std::string &content) {
    FILE *fp = fopen(path.c_str(), "w");
    if (!fp) { perror("fopen"); exit(2); }
    fputs(content.c_str(), fp);
    fclose(fp);
}

static void test_defaults() {
    auto path = make_tmp_path();
    settings_set_path_for_test(path.c_str());
    settings_init_defaults();
    EXPECT("defaults: font_size",        g_font_size == DEFAULT_FONT_SIZE);
    EXPECT("defaults: auto_completion",  g_input_auto_completion == DEFAULT_AUTO_COMPLETION);
    EXPECT("defaults: sep_thousands",    g_sep_thousands == DEFAULT_SEP_THOUSANDS);
    EXPECT("defaults: max_array",        g_limit_max_array_length == DEFAULT_MAX_ARRAY_LENGTH);
    EXPECT("defaults: max_string",       g_limit_max_string_length == DEFAULT_MAX_STRING_LENGTH);
    EXPECT("defaults: max_call_depth",   g_limit_max_call_depth == DEFAULT_MAX_CALL_DEPTH);
    EXPECT("defaults: show_rowlines",    g_show_rowlines == DEFAULT_SHOW_ROWLINES);
    EXPECT("defaults: remember_position",g_remember_position == DEFAULT_REMEMBER_POSITION);
    EXPECT("defaults: hotkey_alt",       g_hotkey_alt == DEFAULT_HOTKEY_ALT);
    unlink(path.c_str());
}

static void test_int_roundtrip() {
    auto path = make_tmp_path();
    settings_set_path_for_test(path.c_str());
    settings_init_defaults();
    g_font_size              = 20;
    g_limit_max_array_length = 512;
    g_limit_max_call_depth   = 32;
    settings_save();
    g_font_size              = 0;
    g_limit_max_array_length = 0;
    g_limit_max_call_depth   = 0;
    settings_load();
    EXPECT("int roundtrip: font_size",      g_font_size == 20);
    EXPECT("int roundtrip: max_array",      g_limit_max_array_length == 512);
    EXPECT("int roundtrip: max_call_depth", g_limit_max_call_depth == 32);
    unlink(path.c_str());
}

static void test_bool_roundtrip() {
    auto path = make_tmp_path();
    settings_set_path_for_test(path.c_str());
    settings_init_defaults();
    g_input_auto_completion = false;
    g_sep_thousands         = false;
    g_show_rowlines         = false;
    settings_save();
    g_input_auto_completion = true;
    g_sep_thousands         = true;
    g_show_rowlines         = true;
    settings_load();
    EXPECT("bool roundtrip: auto_completion==false", g_input_auto_completion == false);
    EXPECT("bool roundtrip: sep_thousands==false",   g_sep_thousands == false);
    EXPECT("bool roundtrip: show_rowlines==false",   g_show_rowlines == false);
    unlink(path.c_str());
}

static void test_unknown_key_ignored() {
    auto path = make_tmp_path();
    settings_set_path_for_test(path.c_str());
    settings_init_defaults();
    write_file(path,
        "unknown_key = bogus\n"
        "font_size = 18\n"
        "another_unknown = 999\n");
    settings_load();
    EXPECT("unknown key: known key still loaded", g_font_size == 18);
    unlink(path.c_str());
}

static void test_comment_and_blank_lines() {
    auto path = make_tmp_path();
    settings_set_path_for_test(path.c_str());
    settings_init_defaults();
    write_file(path,
        "# this is a comment\n"
        "\n"
        "# another comment\n"
        "font_size = 22\n"
        "\n"
        "max_array_length = 999\n");
    settings_load();
    EXPECT("comment/blank: font_size loaded",   g_font_size == 22);
    EXPECT("comment/blank: max_array loaded",   g_limit_max_array_length == 999);
    unlink(path.c_str());
}

int main(int argc, char **argv) {
    // settings_load 内の K_FONT エントリが Fl::set_fonts を呼ぶため、
    // FLTK の初期化 (test_undo と同じパターン) を行う。
    Fl_Window win(1, 1, "test_settings");
    win.end();
    win.show(argc, argv);
    Fl::wait(0);

    test_defaults();
    test_int_roundtrip();
    test_bool_roundtrip();
    test_unknown_key_ignored();
    test_comment_and_blank_lines();

    if (g_failures == 0) {
        printf("All settings tests passed.\n");
        return 0;
    }
    fprintf(stderr, "%d test(s) failed.\n", g_failures);
    return 1;
}
