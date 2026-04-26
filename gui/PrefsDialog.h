// PrefsDialog — 設定ダイアログ (File > Preferences)

#pragma once

class SheetView;

typedef void (*PrefsApplyUiCb)(void *data);

namespace PrefsDialog {
    void run(SheetView *sheet, PrefsApplyUiCb ui_cb = nullptr, void *ui_data = nullptr);
}
