// アプリ状態 (state.ini) の読み書き。 プラットフォーム標準ディレクトリ:
//   Win: %APPDATA%\calcyx\, mac: ~/Library/Application Support/calcyx/,
//   Linux: $XDG_CONFIG_HOME/calcyx/ (or ~/.config/calcyx/)

#pragma once
#include <string>
#include <map>

class AppPrefs {
public:
    AppPrefs();   // ファイルを読み込む
    ~AppPrefs();  // ファイルに書き出す

    int         get_int(const std::string &key, int         def) const;
    std::string get_str(const std::string &key, const std::string &def) const;

    void set_int(const std::string &key, int         val);
    void set_str(const std::string &key, const std::string &val);

    // 設定ディレクトリのパスを返す (ディレクトリがなければ作成する)
    static std::string config_dir();

private:
    std::string path_;
    std::map<std::string, std::string> data_;
    bool dirty_ = false;

    static std::string config_path();
};
