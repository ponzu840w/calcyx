// アプリ設定の読み書き

#include "app_prefs.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

#if defined(_WIN32)
#  include <windows.h>
#  include <direct.h>
#  define MKDIR(p) _mkdir(p)
#else
#  include <errno.h>
#  define MKDIR(p) mkdir((p), 0755)
#endif

// ---- 設定ディレクトリのパスを返す ----
std::string AppPrefs::config_path() {
    char buf[1024];

#if defined(_WIN32)
    // %APPDATA%\calcyx\config.ini
    const char *appdata = getenv("APPDATA");
    if (!appdata) appdata = ".";
    snprintf(buf, sizeof(buf), "%s\\calcyx", appdata);
    MKDIR(buf);
    snprintf(buf, sizeof(buf), "%s\\calcyx\\config.ini", appdata);

#elif defined(__APPLE__)
    // ~/Library/Application Support/calcyx/config.ini
    const char *home = getenv("HOME");
    if (!home) home = ".";
    snprintf(buf, sizeof(buf), "%s/Library/Application Support/calcyx", home);
    MKDIR(buf);
    snprintf(buf, sizeof(buf), "%s/Library/Application Support/calcyx/config.ini", home);

#else
    // $XDG_CONFIG_HOME/calcyx/config.ini  or  ~/.config/calcyx/config.ini
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0]) {
        snprintf(buf, sizeof(buf), "%s/calcyx", xdg);
    } else {
        const char *home = getenv("HOME");
        if (!home) home = ".";
        snprintf(buf, sizeof(buf), "%s/.config/calcyx", home);
    }
    MKDIR(buf);
    strncat(buf, "/config.ini", sizeof(buf) - strlen(buf) - 1);
#endif

    return buf;
}

// ---- コンストラクタ: ファイルを読み込む ----
AppPrefs::AppPrefs() {
    path_ = config_path();
    FILE *fp = fopen(path_.c_str(), "r");
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
    FILE *fp = fopen(path_.c_str(), "w");
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
