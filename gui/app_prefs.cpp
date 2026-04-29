// state.ini の読み書き。 パスは UTF-8 (path_utf8 経由)。

#include "app_prefs.h"
#include "path_utf8.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(_WIN32)
#  include <windows.h>
#  include <shlobj.h>
#endif

// ---- 設定ディレクトリを返す (末尾にセパレータなし) ----
std::string AppPrefs::config_dir() {
    char buf[1024];
#if defined(_WIN32)
    // SHGetFolderPathW で UTF-16 取得 → UTF-8 変換 (日本語ユーザー名対応)
    wchar_t wpath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, wpath))) {
        int len = WideCharToMultiByte(CP_UTF8, 0, wpath, -1, buf, sizeof(buf) - 8, NULL, NULL);
        if (len > 0) {
            buf[len - 1] = '\0';
            strcat(buf, "\\calcyx");
        } else {
            snprintf(buf, sizeof(buf), ".\\calcyx");
        }
    } else {
        char appdata[1024];
        if (calcyx_getenv_utf8("APPDATA", appdata, sizeof(appdata))) {
            snprintf(buf, sizeof(buf), "%s\\calcyx", appdata);
        } else {
            snprintf(buf, sizeof(buf), ".\\calcyx");
        }
    }
#elif defined(__APPLE__)
    char home[1024];
    if (!calcyx_getenv_utf8("HOME", home, sizeof(home))) snprintf(home, sizeof(home), ".");
    snprintf(buf, sizeof(buf), "%s/Library/Application Support/calcyx", home);
#else
    char xdg[1024];
    if (calcyx_getenv_utf8("XDG_CONFIG_HOME", xdg, sizeof(xdg))) {
        snprintf(buf, sizeof(buf), "%s/calcyx", xdg);
    } else {
        char home[1024];
        if (!calcyx_getenv_utf8("HOME", home, sizeof(home))) snprintf(home, sizeof(home), ".");
        snprintf(buf, sizeof(buf), "%s/.config/calcyx", home);
    }
#endif
    calcyx_mkdir(buf);
    return buf;
}

// ---- state.ini のパスを返す ----
std::string AppPrefs::config_path() {
    std::string dir = config_dir();
#if defined(_WIN32)
    return dir + "\\state.ini";
#else
    return dir + "/state.ini";
#endif
}

// ---- コンストラクタ: ファイルを読み込む ----
AppPrefs::AppPrefs() {
    path_ = config_path();
    FILE *fp = calcyx_fopen(path_.c_str(), "r");
    if (!fp) return;
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        // 末尾の改行を除去
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';
        // # コメント・空行をスキップ
        if (line[0] == '#' || line[0] == '\0') continue;
        // key=value を分割
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        data_[line] = eq + 1;
    }
    fclose(fp);
}

// ---- デストラクタ: 変更があればファイルに書き出す ----
AppPrefs::~AppPrefs() {
    if (!dirty_) return;
    FILE *fp = calcyx_fopen(path_.c_str(), "w");
    if (!fp) return;
    for (auto &kv : data_)
        fprintf(fp, "%s=%s\n", kv.first.c_str(), kv.second.c_str());
    fclose(fp);
}

// ---- getter / setter ----
int AppPrefs::get_int(const std::string &key, int def) const {
    auto it = data_.find(key);
    if (it == data_.end()) return def;
    return atoi(it->second.c_str());
}

std::string AppPrefs::get_str(const std::string &key, const std::string &def) const {
    auto it = data_.find(key);
    return (it == data_.end()) ? def : it->second;
}

void AppPrefs::set_int(const std::string &key, int val) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", val);
    data_[key] = buf;
    dirty_ = true;
}

void AppPrefs::set_str(const std::string &key, const std::string &val) {
    data_[key] = val;
    dirty_ = true;
}
