// アプリ設定の読み書き
// レジストリを使わず、プラットフォーム標準ディレクトリの config.ini に保存する
//   Windows : %APPDATA%\calcyx\config.ini
//   macOS   : ~/Library/Application Support/calcyx/config.ini
//   Linux   : $XDG_CONFIG_HOME/calcyx/config.ini  (なければ ~/.config/calcyx/config.ini)

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

private:
    std::string path_;
    std::map<std::string, std::string> data_;
    bool dirty_ = false;

    static std::string config_path();
};
