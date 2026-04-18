#pragma once

// クラッシュ時にレポートファイルを書き出すハンドラを登録する。
// 起動直後、他の初期化より前に呼ぶこと。
void crash_handler_install();

// 前回のクラッシュログがあれば内容を返す (なければ空文字列)。
// 呼び出し後、ログファイルは削除される。
#include <string>
std::string crash_handler_check();

// シート内容のスナップショットを保存する。
// eval 後など適切なタイミングで呼ぶこと。
void crash_handler_save_sheet(const char *content);

// 設定のスナップショットを保存する。
// 設定変更時・起動時に呼ぶこと。
void crash_handler_save_config(const char *content);
