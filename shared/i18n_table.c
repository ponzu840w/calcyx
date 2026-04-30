/* English → Japanese 翻訳辞書。 キーは英語そのまま (gettext 風)。
 * i18n.c が起動時に qsort + bsearch するので並び順は不問。 */

#include <stddef.h>

typedef struct {
    const char *en;
    const char *ja;
} calcyx_tr_entry_t;

const calcyx_tr_entry_t CALCYX_TR_TABLE_JA[] = {
    /* === 共通 === */
    { "Restart to apply this change", "再起動後に変更が反映されます" },
    { "Locked by calcyx.conf.override", "calcyx.conf.override で固定されています" },
    { "OK", "OK" },
    { "Cancel", "キャンセル" },
    { "Apply", "適用" },
    { "Reset", "リセット" },

    /* === GUI: メニューバー (FLTK menu_->add の path) ===
     * "&X" の & は次の文字をアクセラレータにする FLTK の慣例。
     * 日本語ラベルでは末尾に "(&X)" を付ける Windows 風の流儀に合わせる。 */
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
    { "Completion",                            "入力補完" },
    { "Brackets",                              "括弧" },
    { "Editing",                               "編集" },

    /* Number Format タブ */
    { "Decimal",                               "小数" },
    { "Numeric Separators",                    "数値区切り" },

    /* Calculation タブ */
    { "Limits",                                "リミット" },
    { "Max array length:",                     "配列の最大長:" },
    { "Max string length:",                    "文字列の最大長:" },
    { "Max call recursion depth:",             "関数呼び出しの最大ネスト数:" },

    /* General タブ詳細 */
    { "Remember window position on exit",      "終了時のウィンドウ位置を記憶" },
    { "Start with Always on Top",              "常に手前に表示で起動" },
    { "Sets the initial state at launch. Toggle anytime from View menu or pin button.",
      "起動時の初期状態を設定。表示メニューやピンボタンでいつでも切替可能。" },
    { "Enable system tray icon",               "システムトレイアイコンを有効化" },
    { "When enabled, closing the window minimizes to tray.",
      "有効時。ウィンドウを閉じるとトレイに最小化されます。" },
    { "Enable global hotkey",                  "グローバルホットキーを有効化" },
    { "Modifiers:",                            "修飾キー:" },
    { "Key:",                                  "キー:" },

    /* Input タブ詳細 */
    { "Auto-completion on typing",             "自動入力補完を有効化" },
    { "Ctrl+Space opens the popup regardless of this setting.",
      "Ctrl+Space はこの設定に関わらずポップアップを開きます。" },
    { "Show popup as a separate window:",      "ポップアップを独立ウィンドウで表示:" },
    { "In normal mode",                        "通常モード時" },
    { "In compact mode",                       "コンパクトモード時" },
    { "Auto-close brackets ( ) [ ] { }",       "閉じ括弧 ) ] } を自動で挿入する" },
    { "Backspace on empty row deletes the row", "空行で BackSpace キーを押すと行を削除する" },

    /* Number Format タブ詳細 */
    { "Max length of decimal places to display:", "小数桁の最大表示桁数:" },
    { "Scientific notation (E)",               "科学的記数法 (E指数表記)" },
    { "Engineering alignment",                  "エンジニアリング桁揃え（指数が3の倍数）" },
    { "Separate decimal numbers every 3 digits", "10 進数を 3 桁ごとに区切る" },
    { "Separate hex/bin/oct numbers every 4 digits",
      "16/2/8 進数を 4 桁ごとに区切る" },

    /* Appearance タブ詳細 (色エントリ名) */
    { "Background",   "背景" },
    { "Selection",    "選択行" },
    { "Row Line",     "行罫線" },
    { "Text",         "テキスト" },
    { "Accent",       "強調" },
    { "Symbols",      "記号" },
    { "Identifiers",  "識別子" },
    { "Literals",     "リテラル" },
    { "SI Prefix",    "SI 接頭辞" },
    { "Paren 1",      "括弧 1" },
    { "Paren 2",      "括弧 2" },
    { "Paren 3",      "括弧 3" },
    { "Paren 4",      "括弧 4" },
    { "Error",        "エラー" },
    { "Win BG",       "ｳｨﾝﾄﾞｳ背景" },
    { "Dlg BG",       "ﾀﾞｲｱﾛｸﾞ背景" },
    { "UI Input",     "入力欄" },
    { "UI Button",    "ボタン" },
    { "Menu BG",      "ﾒﾆｭｰ背景" },
    { "UI Text",      "UI テキスト" },
    { "UI Label",     "UI ラベル" },
    { "UI Dim",       "UI 無効状態" },
    { "Popup BG",     "ﾎﾟｯﾌﾟｱｯﾌﾟ背景" },
    { "Popup Sel",    "ﾎﾟｯﾌﾟｱｯﾌﾟ選択" },
    { "Popup Text",   "ﾎﾟｯﾌﾟｱｯﾌﾟﾃｷｽﾄ" },
    { "Popup Desc",   "ﾎﾟｯﾌﾟｱｯﾌﾟ説明" },
    { "Popup DescBG", "ﾎﾟｯﾌﾟｱｯﾌﾟ説明背景" },
    { "Popup Border", "ﾎﾟｯﾌﾟｱｯﾌﾟ枠" },
    { "Show row separator lines", "罫線を表示" },

    /* === GUI: ツールチップ (右上ボタン) ===
     * macOS 版 (⌘) と他プラットフォーム版 (Ctrl+) の両方を登録する。
     * ショートカット表記部分は untranslated。 */
    { "Undo (\xe2\x8c\x98Z)",          "元に戻す (\xe2\x8c\x98Z)" },
    { "Redo (\xe2\x8c\x98Y)",          "やり直し (\xe2\x8c\x98Y)" },
    { "Compact Mode (\xe2\x8c\x98:)",  "コンパクトモード (\xe2\x8c\x98:)" },
    { "Always on Top (\xe2\x8c\x98T)", "常に手前に表示 (\xe2\x8c\x98T)" },
    { "Undo (Ctrl+Z)",          "元に戻す (Ctrl+Z)" },
    { "Redo (Ctrl+Y)",          "やり直し (Ctrl+Y)" },
    { "Compact Mode (Ctrl+:)",  "コンパクトモード (Ctrl+:)" },
    { "Always on Top (Ctrl+T)", "常に手前に表示 (Ctrl+T)" },

    /* === GUI: 書式 Choice (右上の Format ドロップダウン) === */
    { "Auto", "自動" },
    { "Dec",  "10 進" },
    { "Hex",  "16 進" },
    { "Bin",  "2 進" },
    { "Oct",  "8 進" },
    /* "SI" / "Kibi" は固定 (英語のまま表示) */
    { "Char", "文字" },

    /* === GUI: フォントピッカー (Appearance タブ → Font 選択) === */
    { "Select Font",        "フォントを選択" },
    { "Use system fonts",   "システムフォントを表示" },
    { "Show proportional fonts (not recommended)",
      "プロポーショナルフォントを表示 (非推奨)" },
    { "Color",              "色" },

    /* === 補完ポップアップ: 組み込み定数 / キーワードの説明 (= calctus の Description) === */
    { "circle ratio",                                  "円周率" },
    { "base of natural logarithm",                     "自然対数の底" },
    { "minimum value of 32 bit signed integer",        "32 bit 符号付き整数の最小値" },
    { "maximum value of 32 bit signed integer",        "32 bit 符号付き整数の最大値" },
    { "minimum value of 32 bit unsigned integer",      "32 bit 符号なし整数の最小値" },
    { "maximum value of 32 bit unsigned integer",      "32 bit 符号なし整数の最大値" },
    { "minimum value of 64 bit signed integer",        "64 bit 符号付き整数の最小値" },
    { "maximum value of 64 bit signed integer",        "64 bit 符号付き整数の最大値" },
    { "minimum value of 64 bit unsigned integer",      "64 bit 符号なし整数の最小値" },
    { "maximum value of 64 bit unsigned integer",      "64 bit 符号なし整数の最大値" },
    { "minimum value of Decimal",                      "Decimal の最小値" },
    { "maximum value of Decimal",                      "Decimal の最大値" },
    { "user-defined variable",                         "ユーザー定義変数" },
    { "last answer",                                   "直前の評価結果" },
    { "true value",                                    "真値" },
    { "false value",                                   "偽値" },
    { "user function definition",                      "ユーザー関数の定義" },

    /* === GUI: PasteOptionForm (複数行ペーストダイアログ) ===
     * "Paste Options" / "Cancel" / "OK" は他で定義済みなので再掲しない。 */
    { "Clipboard Text:",       "クリップボード:" },
    { "Text will be pasted:",  "貼り付け後:" },
    { "Column Delimiter:",     "区切り文字:" },
    { "Column Index:",         "列番号:" },
    { "Select Column",         "列を抽出" },
    { "Remove Commas",         "コンマを除去" },
    { "Remove Right-hands",    "右辺を除去" },

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
    { "Delete row",          "行を削除" },

    /* === CLI: --help === */
    { "Usage: %s [options] [file...]\n",
      "使い方: %s [オプション] [ファイル...]\n" },
    { "Options:\n"
      "  -e <expr>          Evaluate expression directly (repeatable, CLI mode)\n"
      "  -o, --output <fmt> Output format (CLI mode):\n"
      "                       result  result only (default)\n"
      "                       both    expr = result\n"
      "  -b, --batch        Force CLI batch evaluation of files / stdin\n"
      "  -r, --repl         Launch the legacy CLI REPL (fgets line loop)\n"
      "  --print-config     Print the resolved settings in canonical form\n"
      "  --check-config     Syntax-check the conf; exit 1 if warnings\n"
      "  --init-config      Create conf with defaults if missing (no overwrite)\n"
      "  --config <path>    conf file for --print-config / --check-config / --init-config\n"
      "                       (defaults to platform-specific calcyx.conf)\n"
      "  -V, --version      Show version\n"
      "  -h, --help         Show this help\n"
      "\n"
      "Mode selection:\n"
      "  CLI mode if -e / -o / -b / -r is given.\n"
      "  CLI mode if stdin is not a tty (pipe / redirect).\n"
      "  TUI mode otherwise (interactive terminal with no flags or only file args).\n"
      "  Positional files are loaded into the TUI at startup.\n"
      "\n"
      "Comments:\n"
      "  Anything after `;` to end-of-line is a comment (except inside string/char literals).\n"
      "\n"
      "Examples:\n",
      "オプション:\n"
      "  -e <式>            式を直接指定して評価 (複数指定可, CLI モード)\n"
      "  -o, --output <形式> 出力形式 (CLI モード):\n"
      "                       result  結果のみ (デフォルト)\n"
      "                       both    式 = 結果\n"
      "  -b, --batch        位置引数ファイル / stdin の CLI バッチ評価を強制\n"
      "  -r, --repl         旧 CLI 対話 REPL (fgets 行ループ) を起動\n"
      "  --print-config     現在解釈される設定を canonical 形式で stdout に出力\n"
      "  --check-config     conf を syntax check し, 警告があれば exit 1\n"
      "  --init-config      conf が無ければ既定値で生成 (上書きしない)\n"
      "  --config <path>    --print-config / --check-config / --init-config の対象 conf を指定\n"
      "                       (省略時はプラットフォーム既定の calcyx.conf)\n"
      "  -V, --version      バージョンを表示\n"
      "  -h, --help         このヘルプを表示\n"
      "\n"
      "モード判定:\n"
      "  -e / -o / -b / -r があれば CLI モード\n"
      "  stdin が tty でなければ (パイプ / リダイレクト) CLI モード\n"
      "  それ以外 (対話端末で引数なし or 位置引数ファイルのみ) は TUI モード\n"
      "  位置引数のファイルは TUI 起動時にロードされる\n"
      "\n"
      "コメント:\n"
      "  ; 以降を行末コメントとして無視 (文字列・文字リテラル内を除く)\n"
      "\n"
      "例:\n" },
    { "Launch TUI",                       "TUI を起動" },
    { "Open a file in TUI",               "TUI でファイルを開く" },
    { "CLI: pass expressions directly",   "CLI: 式を直接指定" },
    { "CLI: batch-evaluate a file",       "CLI: ファイルをバッチ評価" },
    { "CLI: pipe input",                  "CLI: パイプ入力" },
    { "Exit codes: 0=success, 1=eval error, 2=arg/file error\n",
      "終了コード: 0=正常, 1=評価エラー, 2=引数/ファイルエラー\n" },

    /* === builtin_docs (組み込み関数の説明) ===
     * shared/builtin_docs.c の各 desc に対応する ja 翻訳。
     * 関数名 (sin / cos 等) と引数名 (`x` / `array` 等) は英語固定。 */

    /* 指数・対数 */
    { "`y` power of `x`",                                "`x` の `y` 乗" },
    { "Square root of `x`",                              "`x` の平方根" },
    { "Exponential of `x`",                              "`x` の指数 (e^x)" },
    { "Logarithm of `x`",                                "`x` の自然対数" },
    { "Binary logarithm of `x`",                         "`x` の 2 進対数" },
    { "Common logarithm of `x`",                         "`x` の常用対数" },
    { "Ceiling of binary logarithm of `x`",              "`x` の 2 進対数の天井値" },
    { "Ceiling of common logarithm of `x`",              "`x` の常用対数の天井値" },

    /* 三角関数 */
    { "Sine",                "正弦 (sin)" },
    { "Cosine",              "余弦 (cos)" },
    { "Tangent",             "正接 (tan)" },
    { "Arcsine",             "逆正弦 (asin)" },
    { "Arccosine",           "逆余弦 (acos)" },
    { "Arctangent",          "逆正接 (atan)" },
    { "Arctangent of a / b", "a / b の逆正接 (atan2)" },
    { "Hyperbolic sine",     "双曲線正弦 (sinh)" },
    { "Hyperbolic cosine",   "双曲線余弦 (cosh)" },
    { "Hyperbolic tangent",  "双曲線正接 (tanh)" },

    /* 丸め */
    { "Largest integral value less than or equal to `x`",     "`x` 以下の最大整数 (床)" },
    { "Smallest integral value greater than or equal to `x`", "`x` 以上の最小整数 (天井)" },
    { "Integral part of `x`",                                  "`x` の整数部 (切捨て)" },
    { "Nearest integer to `x`",                                "`x` に最も近い整数 (四捨五入)" },

    /* 絶対値・符号 */
    { "Absolute value of `x`",                                "`x` の絶対値" },
    { "Returns 1 for positives, -1 for negatives, 0 otherwise.",
      "正なら 1, 負なら -1, それ以外は 0 を返す" },
    { "Magnitude of vector `x`",                              "ベクトル `x` の大きさ" },

    /* 最大・最小 */
    { "Maximum value of elements of the `array`",             "`array` の要素の最大値" },
    { "Minimum value of elements of the `array`",             "`array` の要素の最小値" },

    /* GCD / LCM */
    { "Greatest common divisor of elements of the `array`.",  "`array` の要素の最大公約数" },
    { "Least common multiple of elements of the `array`.",    "`array` の要素の最小公倍数" },

    /* アサーション */
    { "Asserts that `x` is true (non-zero).",                 "`x` が true (非ゼロ) であることを表明する" },
    { "Returns true if all elements of `array` are non-zero.", "`array` の全要素が非ゼロなら true" },
    { "Returns true if any element of `array` is non-zero.",   "`array` のいずれかの要素が非ゼロなら true" },

    /* 日時 */
    { "Converts `x` to datetime representation.",             "`x` を日時表現に変換する" },
    { "Current epoch time",                                   "現在のエポック時刻" },
    { "Converts from days to epoch time.",                    "日数からエポック時刻に変換" },
    { "Converts from hours to epoch time.",                   "時間からエポック時刻に変換" },
    { "Converts from minutes to epoch time.",                 "分からエポック時刻に変換" },
    { "Converts from seconds to epoch time.",                 "秒からエポック時刻に変換" },
    { "Converts from epoch time to days.",                    "エポック時刻から日数に変換" },
    { "Converts from epoch time to hours.",                   "エポック時刻から時間に変換" },
    { "Converts from epoch time to minutes.",                 "エポック時刻から分に変換" },
    { "Converts from epoch time to seconds.",                 "エポック時刻から秒に変換" },

    /* 表示形式 */
    { "Converts `x` to binary representation.",               "`x` を 2 進数表現に変換する" },
    { "Converts `x` to decimal representation.",              "`x` を 10 進数表現に変換する" },
    { "Converts `x` to hexadecimal representation.",          "`x` を 16 進数表現に変換する" },
    { "Converts `x` to octal representation.",                "`x` を 8 進数表現に変換する" },
    { "Converts `x` to character representation.",            "`x` を文字表現に変換する" },
    { "Converts `x` to binary prefixed representation.",      "`x` を 2 進接頭辞付き表現 (Ki/Mi/Gi 等) に変換" },
    { "Converts `x` to SI prefixed representation.",          "`x` を SI 接頭辞付き表現 (k/M/G 等) に変換" },

    /* 乱数 */
    { "Generates a random value between 0.0 and 1.0.", "0.0 と 1.0 の間の乱数を生成" },
    { "Generates a 32bit random integer.",             "32bit 乱数整数を生成" },
    { "Generates a 64bit random integer.",             "64bit 乱数整数を生成" },

    /* 配列操作 */
    { "Length of `array`.",                                "`array` の要素数" },
    { "Array from `start` to `stop` (exclusive).",         "`start` から `stop` (排他) までの配列" },
    { "Array from `start` to `stop` (inclusive).",         "`start` から `stop` (包含) までの配列" },
    { "Concatenates two arrays.",                          "2 つの配列を連結" },
    { "Reverses the order of elements in `array`.",        "`array` の要素順を逆転" },
    { "Applies `func` to each element of `array`.",        "`array` の各要素に `func` を適用" },
    { "Filters `array` by `func` predicate.",              "`func` 述語で `array` をフィルタ" },
    { "Counts elements of `array` matching `func`.",       "`func` に合致する `array` の要素数" },
    { "Sorts `array`, optionally by `func` comparator.",   "`array` をソート (任意で `func` 比較子)" },
    { "Aggregates `array` by applying `func` cumulatively.",
      "`func` を累積適用して `array` を集約" },
    { "Extends `array` by applying `func` `count` times.", "`func` を `count` 回適用して `array` を拡張" },
    { "First index of `val` in `array`, or -1.",           "`array` の最初の `val` のインデックス (無ければ -1)" },
    { "Last index of `val` in `array`, or -1.",            "`array` の最後の `val` のインデックス (無ければ -1)" },
    { "Returns true if `array` contains `val`.",           "`array` に `val` が含まれるなら true" },
    { "Elements of `array0` not in `array1`.",             "`array1` に含まれない `array0` の要素 (差集合)" },
    { "Elements common to both arrays.",                   "両方の配列に共通する要素 (積集合)" },
    { "Union of two arrays (no duplicates).",              "2 つの配列の和集合 (重複なし)" },
    { "Removes duplicate elements from `array`.",          "`array` から重複要素を除去" },

    /* 統計 */
    { "Sum of elements of the `array`.",                          "`array` の要素の合計" },
    { "Arithmetic mean of elements of the `array`.",              "`array` の要素の算術平均" },
    { "Geometric mean of elements of the `array`.",               "`array` の要素の幾何平均" },
    { "Harmonic mean of elements of the `array`.",                "`array` の要素の調和平均" },
    { "Inverse of the sum of the inverses. Composite resistance of parallel resistors.",
      "逆数和の逆数 (並列抵抗の合成抵抗)" },

    /* 素数 */
    { "Returns whether `x` is prime or not.", "`x` が素数かどうかを返す" },
    { "`x`-th prime number.",                 "`x` 番目の素数" },
    { "Returns prime factors of `x`.",        "`x` の素因数を返す" },

    /* 方程式 */
    { "Finds a root of `func` using Newton's method.", "ニュートン法で `func` の根を求める" },

    /* 文字列 */
    { "Converts byte-array to string.",                  "バイト配列を文字列に変換" },
    { "Converts string to byte-array.",                  "文字列をバイト配列に変換" },
    { "Removes whitespace from both ends of string.",    "文字列の両端の空白を除去" },
    { "Removes leading whitespace from string.",         "文字列先頭の空白を除去" },
    { "Removes trailing whitespace from string.",        "文字列末尾の空白を除去" },
    { "Replaces occurrences of `old` with `new` in string.",
      "文字列中の `old` を `new` で置換" },
    { "Converts string to lower case.",                  "文字列を小文字に変換" },
    { "Converts string to upper case.",                  "文字列を大文字に変換" },
    { "Returns true if string starts with `prefix`.",    "文字列が `prefix` で始まれば true" },
    { "Returns true if string ends with `suffix`.",      "文字列が `suffix` で終われば true" },
    { "Splits string by delimiter into an array.",       "文字列を区切り文字で分割して配列に" },
    { "Joins array elements into a string with separator.", "配列要素を区切り文字で連結して文字列に" },

    /* グレイコード */
    { "Converts the value from binary to gray-code.", "2 進数からグレイコードに変換" },
    { "Converts the value from gray-code to binary.", "グレイコードから 2 進数に変換" },

    /* ビット・バイト操作 */
    { "Number of bits of `x` that have the value 1.",        "`x` のうち値が 1 のビット数 (popcount)" },
    { "Packs array elements into an integer.",               "配列要素を整数にパック" },
    { "Separates the value of `x` into elements of `b` bit width.",
      "`x` の値を `b` ビット幅の要素に分解" },
    { "Reverses the lower `b` bits of `x`.",                 "`x` の下位 `b` ビットを逆転" },
    { "Reverses the lower `b` bytes of `x`.",                "`x` の下位 `b` バイトを逆転" },
    { "Rotates the lower `b` bits of `x` to the left.",      "`x` の下位 `b` ビットを左回転" },
    { "Rotates the lower `b` bits of `x` to the right.",     "`x` の下位 `b` ビットを右回転" },
    { "Swaps even and odd bytes of `x`.",                    "`x` の偶数バイトと奇数バイトを入れ替え" },
    { "Reverses the order of each 4 bytes of `x`.",          "`x` の 4 バイトごとに順序を逆転" },
    { "Reverses the byte-order of `x`.",                     "`x` のバイト順を逆転" },
    { "Swaps the nibble of each byte of `x`.",               "`x` の各バイトのニブルを入れ替え" },

    /* 色変換 */
    { "Generates 24 bit color value from R, G, B.",      "R, G, B から 24bit カラー値を生成" },
    { "Converts from H, S, V to 24 bit RGB color value.", "H, S, V から 24bit RGB カラー値に変換" },
    { "Converts the 24 bit RGB color value to HSV.",      "24bit RGB カラー値を HSV に変換" },
    { "Converts from H, S, L to 24 bit color RGB value.", "H, S, L から 24bit RGB カラー値に変換" },
    { "Converts the 24 bit RGB color value to HSL.",      "24bit RGB カラー値を HSL に変換" },
    { "Converts 24bit RGB color to 24 bit YUV.",         "24bit RGB カラーを 24bit YUV に変換" },
    { "Converts the 24 bit YUV color to 24 bit RGB.",    "24bit YUV カラーを 24bit RGB に変換" },
    { "Downconverts RGB888 color to RGB565.",            "RGB888 カラーを RGB565 にダウンコンバート" },
    { "Upconverts RGB565 color to RGB888.",              "RGB565 カラーを RGB888 にアップコンバート" },
    { "Packs the 3 values to an RGB565 color.",          "3 つの値を RGB565 カラーにパック" },
    { "Unpacks the RGB565 color to 3 values.",           "RGB565 カラーを 3 つの値にアンパック" },

    /* ECC / パリティ */
    { "Reduction XOR of `x` (same as even parity).",                   "`x` のリダクション XOR (偶パリティと同じ)" },
    { "Odd parity of `x`.",                                            "`x` の奇パリティ" },
    { "Width of ECC for `b`-bit data.",                                "`b` ビットデータに対する ECC の幅" },
    { "Generates ECC code (`b`: data width, `x`: data).",              "ECC コードを生成 (`b`: データ幅, `x`: データ)" },
    { "Decodes and corrects ECC (`b`: data width, `ecc`: ECC, `x`: data).",
      "ECC をデコードして訂正 (`b`: データ幅, `ecc`: ECC, `x`: データ)" },

    /* エンコーディング */
    { "Encode `str` to UTF8 byte sequence.", "`str` を UTF8 バイト列にエンコード" },
    { "Decode UTF8 byte sequence.",          "UTF8 バイト列をデコード" },
    { "Escape URL string.",                  "URL 文字列をエスケープ" },
    { "Unescape URL string.",                "URL 文字列をアンエスケープ" },
    { "Encode string to Base64.",            "文字列を Base64 にエンコード" },
    { "Decode Base64 to string.",            "Base64 を文字列にデコード" },
    { "Encode byte-array to Base64.",        "バイト配列を Base64 にエンコード" },
    { "Decode Base64 to byte-array.",        "Base64 をバイト配列にデコード" },

    /* E 系列 */
    { "Largest E-series value less than or equal to `x`.",     "`x` 以下の最大 E 系列値" },
    { "Smallest E-series value greater than or equal to `x`.", "`x` 以上の最小 E 系列値" },
    { "Nearest E-series value to `x`.",                        "`x` に最も近い E 系列値" },
    { "E-series ratio for `x`.",                               "`x` の E 系列比" },

    /* キャスト */
    { "Rational fraction approximation of `x`.", "`x` の有理分数近似" },
    { "Converts `x` to a real number.",          "`x` を実数に変換" },

    /* === TUI: Preferences 画面 ===
     * 色項目ラベル (Background, Selection, Row Line, ...) と
     * Number Format / Input カテゴリ名は GUI Preferences と共通の翻訳エントリを
     * 再利用する。 ここでは TUI 固有のものだけ追加。 */
    /* タブ名 (前後スペースなし; render 側で " " を付加してから訳す) */
    { "General",                "一般" },
    { "Number-Format",          "数値書式" },
    { "Input",                  "入力" },
    /* General タブ */
    { "Max array length",       "配列の最大長" },
    { "Max string length",      "文字列の最大長" },
    { "Max call depth",         "関数呼び出しの最大ネスト数" },
    { "Color source",           "色設定の参照元" },
    { "Clear after overlay",    "画面切替時に画面消去" },
    { "Edit preferences in text editor", "設定をテキストエディタで編集" },
    { "<- Prev page",           "<- 前のページ" },
    { "Next page ->",           "次のページ ->" },
    /* Number Format タブ */
    { "Decimal digits",         "小数桁数" },
    { "E notation",             "E 指数表記" },
    { "E positive min",         "E 表記に切替する正の指数下限" },
    { "E negative max",         "E 表記に切替する負の指数上限" },
    { "E alignment",            "エンジニアリング桁揃え" },
    /* Input タブ */
    { "Auto completion",        "自動入力補完" },
    { "BS deletes empty row",   "空行で BackSpace で行削除" },
    /* Colors タブ */
    { "Color preset",           "カラープリセット" },
    /* セクションヘッダ (各タブ内のサブジャンル名) */
    { "Shared with GUI",        "GUI と共通" },
    { "TUI only",               "TUI 専用" },
    { "Scientific",             "科学的記数法" },
    { "Preset",                 "プリセット" },
    { "Sheet",                  "シート" },
    { "Syntax",                 "構文ハイライト" },
    { "UI Chrome",              "UI 部品" },
    { "Popup",                  "ポップアップ" },
    /* ステータスヒント (前後スペース込みでキー一致させる) */
    { " Tab/Shift+Tab tab  \xe2\x86\x91\xe2\x86\x93 item  "
      "\xe2\x86\x90\xe2\x86\x92 change  Enter edit/run  "
      "Ctrl+E ext-editor  Esc close ",
      " Tab/Shift+Tab タブ切替  \xe2\x86\x91\xe2\x86\x93 項目移動  "
      "\xe2\x86\x90\xe2\x86\x92 値変更  Enter 編集/実行  "
      "Ctrl+E 外部エディタ  Esc 閉じる " },
    { " \xe2\x86\x90\xe2\x86\x92 \xc2\xb1" "1  0-9 type  Bksp  Enter ok  Esc cancel ",
      " \xe2\x86\x90\xe2\x86\x92 \xc2\xb1" "1  0-9 入力  Bksp  Enter 確定  Esc 取消 " },
    { " type chars  Bksp  Enter ok  Esc cancel ",
      " 入力  Bksp  Enter 確定  Esc 取消 " },
    /* タブ末尾の next-page ヒント */
    { "Tab: next page  /  Shift+Tab: prev page",
      "Tab: 次のページ  /  Shift+Tab: 前のページ" },
    /* タイトル / バリデーションエラー */
    { " Preferences ",          " 設定 " },
    { "Invalid color (expected #RRGGBB)", "色指定が不正です (#RRGGBB 形式)" }
};

const int CALCYX_TR_TABLE_JA_N =
    (int)(sizeof(CALCYX_TR_TABLE_JA) / sizeof(CALCYX_TR_TABLE_JA[0]));
