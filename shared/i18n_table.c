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
    { "&Edit/Copy &All",          "編集(&E)/すべてコピー(&A)" },
    { "&Edit/&Insert Row Below",  "編集(&E)/下に行を挿入(&I)" },
    { "&Edit/Insert Row A&bove",  "編集(&E)/上に行を挿入(&B)" },
    { "&Edit/&Delete Row",        "編集(&E)/行を削除(&D)" },
    { "&Edit/Move Row &Up",       "編集(&E)/行を上に移動(&U)" },
    { "&Edit/Move Row Do&wn",     "編集(&E)/行を下に移動(&W)" },
    { "&Edit/&Recalculate",       "編集(&E)/再計算(&R)" },

    { "&View/Always on &Top",            "表示(&V)/常に手前に表示(&T)" },
    { "&View/&Compact Mode",             "表示(&V)/コンパクトモード(&C)" },
    { "&View/Sys&tem Tray",              "表示(&V)/システムトレイ(&T)" },
    { "&View/Color &Scheme",             "表示(&V)/カラースキーム(&S)" },
    { "&View/Show &Row Lines",           "表示(&V)/行の境界線を表示(&R)" },
    { "&View/Zoom &In",                  "表示(&V)/拡大(&I)" },
    { "&View/Zoom &Out",                 "表示(&V)/縮小(&O)" },
    { "&View/Reset &Zoom",               "表示(&V)/拡大率をリセット(&Z)" },
    { "&View/Scientific Notation (&E)",  "表示(&V)/科学的記数法(&E)" },
    { "&View/Show Thousands &Separator", "表示(&V)/3 桁区切りを表示(&S)" },
    { "&View/Show &Hex Separator",       "表示(&V)/16 進区切りを表示(&H)" },
    { "&View/Decimals &+",               "表示(&V)/小数桁 &+" },
    { "&View/Decimals &\xe2\x88\x92",    "表示(&V)/小数桁 &\xe2\x88\x92" },
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
    { "A programmable calculator based on Calctus.",
      "Calctus をベースにしたプログラム可能な電卓" }
};

const int CALCYX_TR_TABLE_JA_N =
    (int)(sizeof(CALCYX_TR_TABLE_JA) / sizeof(CALCYX_TR_TABLE_JA[0]));
