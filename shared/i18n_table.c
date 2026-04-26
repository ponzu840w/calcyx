/* i18n_table.c — English → Japanese 翻訳辞書.
 *
 * Phase ごとに各フロントエンド (GUI/TUI/CLI) や builtin_docs の翻訳を
 * 追記する. キーは英語そのまま (gettext 風). 同じ英語で文脈が違う場合は
 * 別途 prefix キーを用意する (まだ未発生).
 *
 * 起動時に i18n.c が qsort して bsearch するので, 並び順は気にしなくてよい. */

#include <stddef.h>

typedef struct {
    const char *en;
    const char *ja;
} calcyx_tr_entry_t;

const calcyx_tr_entry_t CALCYX_TR_TABLE_JA[] = {
    /* === 共通 === */
    { "Restart to apply this change", "再起動後に変更が反映されます" },
    { "OK", "OK" },
    { "Cancel", "キャンセル" },
    { "Apply", "適用" },
    { "Reset", "リセット" },

    /* === GUI: メニューバー (FLTK menu_->add の path) ===
     * "&X" の & は次の文字をアクセラレータにする FLTK の慣例.
     * 日本語ラベルでは末尾に "(&X)" を付ける Windows 風の流儀に合わせる. */
    { "&File/All &Clear",         "ファイル(&F)/全消去(&C)" },
    { "&File/&Open...",           "ファイル(&F)/開く(&O)..." },
    { "&File/&Save As...",        "ファイル(&F)/名前を付けて保存(&S)..." },
    { "&File/&Samples",           "ファイル(&F)/サンプル(&S)" },
    { "&File/&Preferences...",    "ファイル(&F)/設定(&P)..." },
    { "&File/&About calcyx",      "ファイル(&F)/calcyx について(&A)" },
    { "&File/E&xit",              "ファイル(&F)/終了(&X)" },

    { "&Edit/&Undo",              "編集(&E)/元に戻す(&U)" },
    { "&Edit/&Redo",              "編集(&E)/やり直し(&R)" },
    { "&Edit/Copy &All",          "編集(&E)/全行をコピー(&A)" },
    { "&Edit/&Insert Row Below",  "編集(&E)/下に行を挿入(&I)" },
    { "&Edit/Insert Row A&bove",  "編集(&E)/上に行を挿入(&B)" },
    { "&Edit/&Delete Row",        "編集(&E)/行を削除(&D)" },
    { "&Edit/Move Row &Up",       "編集(&E)/行を上に移動(&U)" },
    { "&Edit/Move Row Do&wn",     "編集(&E)/行を下に移動(&W)" },
    { "&Edit/&Recalculate",       "編集(&E)/強制再計算(&R)" },

    { "&View/Always on &Top",            "表示(&V)/常に手前に表示(&T)" },
    { "&View/&Compact Mode",             "表示(&V)/コンパクトモード(&C)" },
    { "&View/Sys&tem Tray",              "表示(&V)/システムトレイ(&T)" },
    { "&View/Color &Scheme",             "表示(&V)/カラースキーム(&S)" },
    { "&View/Show &Row Lines",           "表示(&V)/罫線を表示(&R)" },
    { "&View/Zoom &In",                  "表示(&V)/拡大(&I)" },
    { "&View/Zoom &Out",                 "表示(&V)/縮小(&O)" },
    { "&View/Reset &Zoom",               "表示(&V)/拡大率をリセット(&Z)" },
    { "&View/Scientific Notation (&E)",  "表示(&V)/科学的記数法(&E)" },
    { "&View/Show Thousands &Separator", "表示(&V)/3 桁区切りを表示(&S)" },
    { "&View/Show &Hex Separator",       "表示(&V)/16 進区切りを表示(&H)" },
    { "&View/Decimals &+",               "表示(&V)/小数桁数 &+" },
    { "&View/Decimals &\xe2\x88\x92",    "表示(&V)/小数桁数 &\xe2\x88\x92" },
    { "&View/&Auto Completion",          "表示(&V)/自動補完(&A)" },

    /* === GUI: ファイルダイアログ === */
    { "Open",                  "開く" },
    { "Save As",               "名前を付けて保存" },
    { "Text files",            "テキストファイル" },
    { "All files",             "すべてのファイル" },
    { "File not found:",       "ファイルが見つかりません:" },
    { "Cannot open file:",     "ファイルを開けません:" },
    { "Cannot save file:",     "ファイルを保存できません:" },

    /* === GUI: About === */
    { "About calcyx",          "calcyx について" },
    { "An engineering calculator based on Calctus.",
      "Calctus ベースのエンジニアリング電卓" },

    /* === GUI: Preferences ダイアログ === */
    { "Preferences",                "設定" },
    { "Reset all settings to defaults", "すべての設定をデフォルトに戻す" },
    { "Reset all settings to defaults?", "すべての設定をデフォルトに戻しますか?" },

    /* タブ名 (FLTK タブの内側マージン用に前後スペース) */
    { " General ",         " 一般 " },
    { " Appearance ",      " 外観 " },
    { " Input ",           " 入力 " },
    { " Number Format ",   " 数値書式 " },
    { " Calculation ",     " 計算 " },

    /* General タブ */
    { "Language",                              "言語" },
    { "Language:",                             "言語:" },
    { "Restart calcyx after changing language.",
      "言語を変更したら calcyx を再起動してください。" },
    { "Window",                                "ウィンドウ" },
    { "System Tray",                           "システムトレイ" },
    { "Global Hotkey",                         "グローバルホットキー" },
    { "Configuration",                         "設定ファイル" },
    { "Open folder",                           "フォルダを開く" },

    /* Appearance タブ */
    { "Font",                                  "フォント" },
    { "Font:",                                 "フォント:" },
    { "Size:",                                 "サイズ:" },
    { "Colors",                                "色" },
    { "Preset:",                               "プリセット:" },
    { "Copy to user-defined",                  "ユーザー定義にコピー" },
    { "Copy current preset colors to user-defined and switch to it for editing",
      "現在のプリセット色をユーザー定義にコピーして編集可能にします" },

    /* Input タブ */
    { "Completion",                            "補完" },
    { "Brackets",                              "括弧" },
    { "Editing",                               "編集" },

    /* Number Format タブ */
    { "Decimal",                               "小数" },
    { "Numeric Separators",                    "数値区切り" },

    /* Calculation タブ */
    { "Limits",                                "リミット" },

    /* === TUI: メニューバータイトル === */
    { "&File",   "ファイル(&F)" },
    { "&Edit",   "編集(&E)" },
    { "&View",   "表示(&V)" },
    { "fo&Rmat", "書式(&R)" },

    /* === TUI: File メニュー項目 === */
    { "&Open...",         "開く(&O)..." },
    { "&Save",            "保存(&S)" },
    { "S&amples",         "サンプル(&A)" },
    { "&Clear All",       "全消去(&C)" },
    { "&Preferences...",  "設定(&P)..." },
    { "A&bout calcyx",    "calcyx について(&B)" },
    { "E&xit",            "終了(&X)" },

    /* === TUI: Edit メニュー項目 === */
    { "&Undo",              "元に戻す(&U)" },
    { "&Redo",              "やり直し(&R)" },
    { "Copy &All",          "全行をコピー(&A)" },
    { "&Insert Row Below",  "下に行を挿入(&I)" },
    { "Insert Row A&bove",  "上に行を挿入(&B)" },
    { "&Delete Row",        "行を削除(&D)" },
    { "Move Row &Up",       "行を上に移動(&U)" },
    { "Move Row Do&wn",     "行を下に移動(&W)" },
    { "R&ecalculate",       "強制再計算(&E)" },

    /* === TUI: View メニュー項目 === */
    { "&Compact Mode",       "コンパクトモード(&C)" },
    { "Decimals &+",         "小数桁数 &+" },
    { "Decimals &-",         "小数桁数 &-" },
    { "&Auto Completion",    "自動補完(&A)" },

    /* === TUI: Format メニュー項目 === */
    { "&Auto",               "自動(&A)" },
    { "&Decimal",            "10 進数(&D)" },
    { "&Hex",                "16 進数(&H)" },
    { "&Binary",             "2 進数(&B)" },
    { "&SI Prefix",          "SI 接頭辞(&S)" },

    /* === TUI: prompt label === */
    { "Open file: ", "開くファイル: " },
    { "Save as:   ", "保存先:       " },

    /* === TUI: flash messages === */
    { "Cancelled",                  "キャンセルしました" },
    { "Path is empty",              "パスが空です" },
    { "Saved: ",                    "保存しました: " },
    { "Save failed: ",              "保存に失敗しました: " },
    { "Loaded: ",                   "読み込みました: " },
    { "Load failed: ",              "読み込みに失敗しました: " },
    { "Opened: ",                   "開きました: " },
    { "Edited: ",                   "編集しました: " },
    { "Loaded sample: ",            "サンプルを読み込みました: " },
    { "New file: ",                 "新規ファイル: " },
    { "samples directory not found", "サンプルディレクトリが見つかりません" },
    { "Paste cancelled",            "貼り付けをキャンセルしました" },

    /* === TUI: ヘルプ行 === */
    { " F1 help  Alt+F menu  ^Q quit  ^Z/^Y undo/redo  F8-F12 fmt ",
      " F1 ヘルプ  Alt+F メニュー  ^Q 終了  ^Z/^Y 元に戻す/やり直し  F8-F12 書式 " },

    /* === TUI: Paste Options ダイアログ === */
    { "Paste Options",                       "貼り付けオプション" },
    { "Clipboard contains ",                 "クリップボードに " },
    { " line(s):",                           " 行あります:" },
    { "Insert each line as a new row",       "各行を新しい行として挿入" },
    { "Join into single line at cursor",     "カーソル位置に 1 行として結合" },
    { "↑↓ select   Enter confirm   Esc cancel",
      "↑↓ 選択   Enter 確定   Esc キャンセル" },

    /* === TUI: About ダイアログ === */
    { "An engineering calculator based on Calctus.",
      "Calctus ベースのエンジニアリング電卓" },
    { "Shortcuts",                           "ショートカット" },
    { "License",                             "ライセンス" },
    { "Tab: switch   ↑↓: scroll  (",         "Tab: 切替   ↑↓: スクロール  (" },
    { ")   Esc / Enter / q: close",          ")   Esc / Enter / q: 閉じる" },

    /* === TUI: ショートカット説明 === */
    { "Commit and insert row below",         "確定して下に行を挿入" },
    { "Insert row above",                    "上に行を挿入" },
    { "Delete current row",                  "現在の行を削除" },
    { "Delete row, move focus up",           "行を削除して上に移動" },
    { "Move current row",                    "現在の行を移動" },
    { "Undo / Redo",                         "元に戻す / やり直し" },
    { "Trigger completion",                  "補完を起動" },
    { "Auto-complete popup",                 "自動補完ポップアップ" },
    { "Recalculate all",                     "全行を再計算" },
    { "Toggle compact mode",                 "コンパクトモード切替" },
    { "Format: Auto / Dec / Hex / Bin / SI", "書式: 自動 / 10 進 / 16 進 / 2 進 / SI" },
    { "Decimal places +/-",                  "小数桁数 +/-" },
    { "Copy all (OSC 52)",                   "全行をコピー (OSC 52)" },
    { "Clear all rows",                      "全行を削除" },
    { "Open / Save file",                    "ファイルを開く / 保存" },
    { "Quit",                                "終了" },
    { "This About dialog",                   "この About ダイアログ" },

    /* === TUI: コンテキストメニュー === */
    { "Copy row",            "行をコピー" },
    { "Copy expression",     "式をコピー" },
    { "Copy result",         "結果をコピー" },
    { "Cut",                 "切り取り" },
    { "Paste",               "貼り付け" },
    { "Insert row above",    "上に行を挿入" },
    { "Insert row below",    "下に行を挿入" },
    { "Delete row",          "行を削除" }
};

const int CALCYX_TR_TABLE_JA_N =
    (int)(sizeof(CALCYX_TR_TABLE_JA) / sizeof(CALCYX_TR_TABLE_JA[0]));
