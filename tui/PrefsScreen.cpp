/* TUI Preferences 画面 phase 1 実装 (read-only スケルトン)。 */

#include <cstring>
#include "PrefsScreen.h"
#include "SemanticColors.h"
#include "TuiApp.h"
#include "i18n.h"

extern "C" {
#include "settings_schema.h"
#include "settings_io.h"
#include "settings_writer.h"
}

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <algorithm>

using namespace ftxui;

namespace calcyx::tui {

/* ---- 編集対象項目テーブル (= 「TUI に効くもの限定」) ---- */
namespace {

enum Tab { TAB_GENERAL = 0, TAB_NUMBER = 1, TAB_INPUT = 2, TAB_COLORS = 3 };

/* schema 値の編集ではなく専用アクション行を表現するための識別子。 */
enum PrefsAction {
    ACT_NONE = 0,
    ACT_EXTERNAL_EDITOR,
    ACT_RESET_ALL,
    ACT_PREV_PAGE,
    ACT_NEXT_PAGE,
};

/* tui_color_source の現在値で表示/非表示を切替えるためのフラグ。 */
enum PrefsVisibility {
    VIS_ALWAYS         = 0,
    VIS_MIRROR_ONLY    = 1,  /* tui_color_source = mirror_gui のときだけ */
    VIS_SEMANTIC_ONLY  = 2,  /* tui_color_source = semantic のときだけ */
};

struct PrefsItem {
    int   tab;
    const char *section; /* GUI Preferences のサブヘッダ相当 (NULL で省略)。
                          * 直前項目と異なるとき Render でヘッダ行を挿入。 */
    const char *key;     /* schema_key (= action 行なら NULL) */
    const char *label;   /* i18n raw key */
    int         visibility; /* VIS_ALWAYS / VIS_MIRROR_ONLY / VIS_SEMANTIC_ONLY */
    PrefsAction action;  /* ACT_NONE で schema 値編集 */
};

constexpr PrefsItem kItems[] = {
    /* General */
    { TAB_GENERAL, "Shared with GUI", "language",                "Language",            VIS_ALWAYS, ACT_NONE },
    { TAB_GENERAL, "Shared with GUI", "max_array_length",        "Max array length",    VIS_ALWAYS, ACT_NONE },
    { TAB_GENERAL, "Shared with GUI", "max_string_length",       "Max string length",   VIS_ALWAYS, ACT_NONE },
    { TAB_GENERAL, "Shared with GUI", "max_call_depth",          "Max call depth",      VIS_ALWAYS, ACT_NONE },
    { TAB_GENERAL, "TUI only",        "tui_clear_after_overlay", "Clear after overlay", VIS_ALWAYS, ACT_NONE },
    { TAB_GENERAL, nullptr,           nullptr,                   "Edit preferences in text editor",
                                                                                        VIS_ALWAYS, ACT_EXTERNAL_EDITOR },
    { TAB_GENERAL, nullptr,           nullptr,                   "Reset all settings to defaults",
                                                                                        VIS_ALWAYS, ACT_RESET_ALL },
    { TAB_GENERAL, nullptr,           nullptr,                   "<- Prev page",        VIS_ALWAYS, ACT_PREV_PAGE },
    { TAB_GENERAL, nullptr,           nullptr,                   "Next page ->",        VIS_ALWAYS, ACT_NEXT_PAGE },

    /* Number Format */
    { TAB_NUMBER,  nullptr,           "decimal_digits",          "Decimal digits",      VIS_ALWAYS, ACT_NONE },
    { TAB_NUMBER,  "Scientific",      "e_notation",              "E notation",          VIS_ALWAYS, ACT_NONE },
    { TAB_NUMBER,  "Scientific",      "e_positive_min",          "E positive min",      VIS_ALWAYS, ACT_NONE },
    { TAB_NUMBER,  "Scientific",      "e_negative_max",          "E negative max",      VIS_ALWAYS, ACT_NONE },
    { TAB_NUMBER,  "Scientific",      "e_alignment",             "E alignment",         VIS_ALWAYS, ACT_NONE },
    { TAB_NUMBER,  nullptr,           nullptr,                   "<- Prev page",        VIS_ALWAYS, ACT_PREV_PAGE },
    { TAB_NUMBER,  nullptr,           nullptr,                   "Next page ->",        VIS_ALWAYS, ACT_NEXT_PAGE },

    /* Input */
    { TAB_INPUT,   nullptr,           "auto_completion",         "Auto completion",     VIS_ALWAYS, ACT_NONE },
    { TAB_INPUT,   nullptr,           "bs_delete_empty_row",     "BS deletes empty row",VIS_ALWAYS, ACT_NONE },
    { TAB_INPUT,   nullptr,           nullptr,                   "<- Prev page",        VIS_ALWAYS, ACT_PREV_PAGE },
    { TAB_INPUT,   nullptr,           nullptr,                   "Next page ->",        VIS_ALWAYS, ACT_NEXT_PAGE },

    /* Colors タブの先頭は描画の振る舞い系設定を「Rendering」 セクションに
     * まとめ、 続く Syntax / Sheet / UI Chrome / Popup を純粋な色項目にする。 */
    { TAB_COLORS,  "Rendering",       "tui_color_source",        "Color source",        VIS_ALWAYS, ACT_NONE },
    { TAB_COLORS,  "Rendering",       "tui_sem_color_literal",   "Color literal in actual color",
                                                                                        VIS_SEMANTIC_ONLY, ACT_NONE },
    { TAB_COLORS,  "Syntax",          "tui_sem_ident",           "Identifiers",         VIS_SEMANTIC_ONLY, ACT_NONE },
    { TAB_COLORS,  "Syntax",          "tui_sem_special",         "Literals",            VIS_SEMANTIC_ONLY, ACT_NONE },
    { TAB_COLORS,  "Syntax",          "tui_sem_si_pfx",          "SI Prefix",           VIS_SEMANTIC_ONLY, ACT_NONE },
    { TAB_COLORS,  "Syntax",          "tui_sem_symbol",          "Symbols",             VIS_SEMANTIC_ONLY, ACT_NONE },
    { TAB_COLORS,  "Syntax",          "tui_sem_paren0",          "Paren 1",             VIS_SEMANTIC_ONLY, ACT_NONE },
    { TAB_COLORS,  "Syntax",          "tui_sem_paren1",          "Paren 2",             VIS_SEMANTIC_ONLY, ACT_NONE },
    { TAB_COLORS,  "Syntax",          "tui_sem_paren2",          "Paren 3",             VIS_SEMANTIC_ONLY, ACT_NONE },
    { TAB_COLORS,  "Syntax",          "tui_sem_paren3",          "Paren 4",             VIS_SEMANTIC_ONLY, ACT_NONE },

    /* === mirror_gui モード: GUI と共通の RGB hex 色 === */
    { TAB_COLORS,  "Preset",          "color_preset",            "Color preset",        VIS_MIRROR_ONLY, ACT_NONE },
    { TAB_COLORS,  "Sheet",           "color_bg",                "Background",          VIS_MIRROR_ONLY, ACT_NONE },
    { TAB_COLORS,  "Sheet",           "color_sel_bg",            "Selection",           VIS_MIRROR_ONLY, ACT_NONE },
    { TAB_COLORS,  "Sheet",           "color_rowline",           "Row Line",            VIS_MIRROR_ONLY, ACT_NONE },
    { TAB_COLORS,  "Sheet",           "color_text",              "Text",                VIS_MIRROR_ONLY, ACT_NONE },
    { TAB_COLORS,  "Sheet",           "color_accent",            "Accent",              VIS_MIRROR_ONLY, ACT_NONE },
    { TAB_COLORS,  "Syntax",          "color_symbol",            "Symbols",             VIS_MIRROR_ONLY, ACT_NONE },
    { TAB_COLORS,  "Syntax",          "color_ident",             "Identifiers",         VIS_MIRROR_ONLY, ACT_NONE },
    { TAB_COLORS,  "Syntax",          "color_special",           "Literals",            VIS_MIRROR_ONLY, ACT_NONE },
    { TAB_COLORS,  "Syntax",          "color_si_pfx",            "SI Prefix",           VIS_MIRROR_ONLY, ACT_NONE },
    { TAB_COLORS,  "Syntax",          "color_paren0",            "Paren 1",             VIS_MIRROR_ONLY, ACT_NONE },
    { TAB_COLORS,  "Syntax",          "color_paren1",            "Paren 2",             VIS_MIRROR_ONLY, ACT_NONE },
    { TAB_COLORS,  "Syntax",          "color_paren2",            "Paren 3",             VIS_MIRROR_ONLY, ACT_NONE },
    { TAB_COLORS,  "Syntax",          "color_paren3",            "Paren 4",             VIS_MIRROR_ONLY, ACT_NONE },
    { TAB_COLORS,  "Syntax",          "color_error",             "Error",               VIS_MIRROR_ONLY, ACT_NONE },
    { TAB_COLORS,  "UI Chrome",       "color_ui_win_bg",         "Win BG",              VIS_MIRROR_ONLY, ACT_NONE },
    { TAB_COLORS,  "UI Chrome",       "color_ui_bg",             "Dlg BG",              VIS_MIRROR_ONLY, ACT_NONE },
    { TAB_COLORS,  "UI Chrome",       "color_ui_input",          "UI Input",            VIS_MIRROR_ONLY, ACT_NONE },
    { TAB_COLORS,  "UI Chrome",       "color_ui_btn",            "UI Button",           VIS_MIRROR_ONLY, ACT_NONE },
    { TAB_COLORS,  "UI Chrome",       "color_ui_menu",           "Menu BG",             VIS_MIRROR_ONLY, ACT_NONE },
    { TAB_COLORS,  "UI Chrome",       "color_ui_text",           "UI Text",             VIS_MIRROR_ONLY, ACT_NONE },
    { TAB_COLORS,  "UI Chrome",       "color_ui_label",          "UI Label",            VIS_MIRROR_ONLY, ACT_NONE },
    { TAB_COLORS,  "UI Chrome",       "color_ui_dim",            "UI Dim",              VIS_MIRROR_ONLY, ACT_NONE },
    { TAB_COLORS,  "Popup",           "color_pop_bg",            "Popup BG",            VIS_MIRROR_ONLY, ACT_NONE },
    { TAB_COLORS,  "Popup",           "color_pop_sel",           "Popup Sel",           VIS_MIRROR_ONLY, ACT_NONE },
    { TAB_COLORS,  "Popup",           "color_pop_text",          "Popup Text",          VIS_MIRROR_ONLY, ACT_NONE },
    { TAB_COLORS,  "Popup",           "color_pop_desc",          "Popup Desc",          VIS_MIRROR_ONLY, ACT_NONE },
    { TAB_COLORS,  "Popup",           "color_pop_desc_bg",       "Popup DescBG",        VIS_MIRROR_ONLY, ACT_NONE },
    { TAB_COLORS,  "Popup",           "color_pop_border",        "Popup Border",        VIS_MIRROR_ONLY, ACT_NONE },

    { TAB_COLORS,  nullptr,           nullptr,                   "<- Prev page",        VIS_ALWAYS, ACT_PREV_PAGE },
    { TAB_COLORS,  nullptr,           nullptr,                   "Next page ->",        VIS_ALWAYS, ACT_NEXT_PAGE },
};
constexpr int kItemsCount = (int)(sizeof(kItems) / sizeof(kItems[0]));

constexpr const char *kTabLabels[] = {
    "General", "Number-Format", "Input", "Colors"
};
constexpr int kTabCount = 4;

/* schema 既定値の文字列化 (defaults_lookup と同等の最小実装)。 */
std::string schema_default_string(const char *key) {
    auto *d = calcyx_settings_find(key);
    if (!d) return "";
    char buf[128];
    switch (d->kind) {
    case CALCYX_SETTING_KIND_BOOL:
        return d->b_def ? "true" : "false";
    case CALCYX_SETTING_KIND_INT:
        snprintf(buf, sizeof(buf), "%d", d->i_def);
        return buf;
    case CALCYX_SETTING_KIND_FONT:
    case CALCYX_SETTING_KIND_HOTKEY:
    case CALCYX_SETTING_KIND_COLOR_PRESET:
    case CALCYX_SETTING_KIND_STRING:
        return d->s_def ? d->s_def : "";
    case CALCYX_SETTING_KIND_COLOR:
        return "#000000";  /* 後で preset 既定色で上書き予定 */
    default:
        return "";
    }
}

/* schema KIND_STRING (列挙) / KIND_COLOR_PRESET の選択肢テーブル。
 * 「←/→ で循環」 を実現するため、 各 key に対して候補リストを持つ。 */
struct ChoiceList {
    const char *const *items;
    int                count;
};
static const char *const kLanguageChoices[]      = { "auto", "en", "ja" };
static const char *const kColorSourceChoices[]   = { "semantic", "mirror_gui" };
static const char *const kClearOverlayChoices[]  = { "auto", "true", "false" };
static const char *const kColorPresetChoices[]   = {
    "otaku-black", "gyakubari-white", "saboten-grey", "saboten-white", "user-defined"
};

ChoiceList choices_for(const char *key) {
    if (!key) return { nullptr, 0 };
    if (strcmp(key, "language") == 0)
        return { kLanguageChoices, 3 };
    if (strcmp(key, "tui_color_source") == 0)
        return { kColorSourceChoices, 2 };
    if (strcmp(key, "tui_clear_after_overlay") == 0)
        return { kClearOverlayChoices, 3 };
    if (strcmp(key, "color_preset") == 0)
        return { kColorPresetChoices, 5 };
    /* semantic syntax color: tui_sem_* は kSemanticColors の名前一覧から選ぶ。 */
    if (strncmp(key, "tui_sem_", 8) == 0) {
        static const char *names[64];
        static int n = 0;
        if (n == 0) {
            for (int i = 0; i < kSemanticColorCount && i < 64; i++)
                names[i] = kSemanticColors[i].name;
            n = kSemanticColorCount;
        }
        return { names, n };
    }
    return { nullptr, 0 };
}

bool parse_bool(const std::string &s) {
    return s == "true" || s == "1" || s == "on" || s == "yes";
}

/* UTF-8 文字列の表示 cell width。 ASCII / Latin は 1 cell、 CJK 漢字・かな・
 * ハングル・全角形式は 2 cell。 ftxui の string_width との完全一致は保証されない
 * が、 「日本語ラベルを pad してから ftxui に渡せば、 ftxui も pad 済み ASCII
 * 空白を 1 cell ずつ加算するため、 端末描画と揃う」 が狙い。 */
int display_width_utf8(const std::string &s) {
    int w = 0;
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = (unsigned char)s[i];
        unsigned int cp = 0;
        int len = 1;
        if (c < 0x80) { cp = c; len = 1; }
        else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; len = 2; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; len = 3; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; len = 4; }
        else { i++; continue; }
        for (int k = 1; k < len; k++) {
            if (i + (size_t)k >= s.size()) break;
            cp = (cp << 6) | ((unsigned char)s[i + k] & 0x3F);
        }
        bool wide =
               (cp >= 0x1100 && cp <= 0x115F)
            || (cp >= 0x2E80 && cp <= 0xA4CF && cp != 0x303F)
            || (cp >= 0xAC00 && cp <= 0xD7A3)
            || (cp >= 0xF900 && cp <= 0xFAFF)
            || (cp >= 0xFE30 && cp <= 0xFE4F)
            || (cp >= 0xFF00 && cp <= 0xFF60)
            || (cp >= 0xFFE0 && cp <= 0xFFE6);
        w += wide ? 2 : 1;
        i += len;
    }
    return w;
}

/* label 文字列を 半角空白で kLabelWidth まで pad する。 trunc は行わない
 * (= 翻訳が長すぎて溢れたら value 列にめり込むが、 そのときは kLabelWidth 拡大で
 * 対応)。 */
std::string pad_to_width(const std::string &s, int target_cells) {
    int dw = display_width_utf8(s);
    if (dw >= target_cells) return s;
    return s + std::string(target_cells - dw, ' ');
}

/* override 含む現在の conf 読み込み。 PrefsScreen::open() から呼ぶ。 */
void load_conf_kv(std::map<std::string, std::string> *out_kv,
                  std::set<std::string>              *out_locked,
                  const std::string                  &conf_path,
                  const std::string                  &override_path) {
    out_kv->clear();
    out_locked->clear();
    auto cb = +[](const char *k, const char *v, int line, void *user) {
        (void)line;
        auto *kv = static_cast<std::map<std::string, std::string> *>(user);
        if (k && v) (*kv)[k] = v;
    };
    calcyx_conf_each(conf_path.c_str(), cb, out_kv);
    /* override は kv に上書きしつつ locked に記録 */
    std::map<std::string, std::string> ov;
    calcyx_conf_each(override_path.c_str(), cb, &ov);
    for (auto &p : ov) {
        (*out_kv)[p.first] = p.second;
        out_locked->insert(p.first);
    }
}

} // anonymous namespace

/* ---- PrefsScreen ---- */

void PrefsScreen::open() {
    visible_ = true;
    tab_ = 0; item_ = 0; editing_ = false;
    edit_buf_.clear(); edit_cur_ = 0;

    std::string conf_path     = app_->preferences_conf_path_str();
    std::string override_path = conf_path + ".override";
    load_conf_kv(&values_, &locked_, conf_path, override_path);

    /* schema で定義されているが conf に未出現の key には既定値を流し込む。
     * action 行 (key=NULL) はスキップ — values_.find(nullptr) で
     * std::string(nullptr) → strlen(nullptr) → SEGV になる。 */
    for (int i = 0; i < kItemsCount; i++) {
        const char *k = kItems[i].key;
        if (!k) continue;
        if (values_.find(k) == values_.end())
            values_[k] = schema_default_string(k);
    }
    refresh_visible_items();
    /* シート画面 → prefs 画面の全面切替で macOS+tmux+CJK のゴースト残りを
     * 防ぐため明示的に画面クリア。 */
    app_->overlay_closed_public();
}

void PrefsScreen::close() {
    visible_ = false;
    editing_ = false;
    /* シート画面 ← prefs 画面の全面切替で macOS+tmux+CJK のゴースト残りを
     * 防ぐため画面クリア。 tui_clear_after_overlay の設定に従う (= macOS で
     * default true、 他 false)。 */
    app_->overlay_closed_public();
}

void PrefsScreen::refresh_visible_items() const {
    visible_items_.clear();
    /* tui_color_source の現在値で MIRROR_ONLY / SEMANTIC_ONLY 行を filter。 */
    auto src_it = values_.find("tui_color_source");
    bool is_semantic = (src_it == values_.end() || src_it->second == "semantic");
    for (int i = 0; i < kItemsCount; i++) {
        if (kItems[i].tab != tab_) continue;
        int v = kItems[i].visibility;
        if (v == VIS_SEMANTIC_ONLY && !is_semantic) continue;
        if (v == VIS_MIRROR_ONLY   &&  is_semantic) continue;
        visible_items_.push_back(i);
    }
}

/* 現在選択中の項目の schema 情報。 */
const calcyx_setting_desc_t *PrefsScreen_current_desc(
        const std::vector<int> &visible, int item) {
    if (item < 0 || item >= (int)visible.size()) return nullptr;
    return calcyx_settings_find(kItems[visible[item]].key);
}

/* 1 項目の値を conf に書き戻す + apply_settings_from_conf 再呼び出し。 */
void PrefsScreen::commit_current(const std::string &new_val) {
    if (visible_items_.empty()) return;
    int idx = visible_items_[item_];
    const PrefsItem &it = kItems[idx];
    values_[it.key] = new_val;

    /* lookup: values_ に該当 key だけ書き戻し、 他は LEAVE。 */
    struct Ctx {
        const std::map<std::string, std::string> *values;
        const char *target_key;
    } ctx { &values_, it.key };
    auto cb = +[](const char *key, char *buf, size_t buflen,
                  int *out_is_default, void *user) -> int {
        Ctx *c = (Ctx *)user;
        if (strcmp(key, c->target_key) != 0) return -1;  /* LEAVE */
        auto vit = c->values->find(key);
        if (vit == c->values->end()) return -1;
        snprintf(buf, buflen, "%s", vit->second.c_str());
        if (out_is_default) *out_is_default = 0;
        return 1;
    };
    std::string path = app_->preferences_conf_path_str();
    calcyx_settings_write_preserving(path.c_str(), nullptr, cb, &ctx);
    /* メモリ反映: 全 schema 項目を再読込 (= 桁数 / パレット / 制限すべて反映)。 */
    app_->apply_settings_public();

    /* tui_color_source が変わると Colors タブの可視項目セットが大きく
     * 入れ替わるため、 端末ゴミ対策の画面消去をここでも発火する。 */
    if (strcmp(it.key, "tui_color_source") == 0) {
        refresh_visible_items();
        if (item_ >= (int)visible_items_.size())
            item_ = std::max(0, (int)visible_items_.size() - 1);
        app_->overlay_closed_public();
    }
}

/* GUI の reset_to_defaults と同等。 conf 全 key を schema default に戻し、
 * COLOR は preset 依存なので touch しない (= writer が preset に応じて出力)。 */
void PrefsScreen::do_reset_all() {
    auto cb = +[](const char *key, char *buf, size_t buflen,
                  int *out_is_default, void *user) -> int {
        (void)user;
        const calcyx_setting_desc_t *d = calcyx_settings_find(key);
        if (!d) return -1;
        switch (d->kind) {
        case CALCYX_SETTING_KIND_BOOL:
            snprintf(buf, buflen, "%s", d->b_def ? "true" : "false");
            break;
        case CALCYX_SETTING_KIND_INT:
            snprintf(buf, buflen, "%d", d->i_def);
            break;
        case CALCYX_SETTING_KIND_FONT:
        case CALCYX_SETTING_KIND_HOTKEY:
        case CALCYX_SETTING_KIND_COLOR_PRESET:
        case CALCYX_SETTING_KIND_STRING:
            if (!d->s_def) return -1;
            snprintf(buf, buflen, "%s", d->s_def);
            break;
        case CALCYX_SETTING_KIND_COLOR:
            return -1;  /* preset 依存。 writer に任せる */
        default:
            return -1;
        }
        if (out_is_default) *out_is_default = 1;
        return 1;
    };
    std::string path = app_->preferences_conf_path_str();
    calcyx_settings_write_preserving(path.c_str(), nullptr, cb, nullptr);

    /* 書き戻し後、 values_ / locked_ を再読込して画面表示を更新。 */
    std::string override_path = path + ".override";
    load_conf_kv(&values_, &locked_, path, override_path);
    for (int i = 0; i < kItemsCount; i++) {
        const char *k = kItems[i].key;
        if (!k) continue;
        if (values_.find(k) == values_.end())
            values_[k] = schema_default_string(k);
    }
    app_->apply_settings_public();
    refresh_visible_items();
    if (item_ >= (int)visible_items_.size())
        item_ = std::max(0, (int)visible_items_.size() - 1);
    app_->overlay_closed_public();
}

bool PrefsScreen::OnEvent(Event ev) {
    if (!visible_) return false;

    /* --- Reset 確認モード: Y/Enter で実行、 N/Esc でキャンセル。 --- */
    if (confirming_reset_) {
        if (ev.is_mouse()) return true;  /* マウスは無視 */
        if (ev == Event::Character('y') || ev == Event::Character('Y')
                                        || ev == Event::Return) {
            do_reset_all();
            confirming_reset_ = false;
            app_->flash_message_public(
                std::string(_("Settings reset to defaults")));
            return true;
        }
        if (ev == Event::Character('n') || ev == Event::Character('N')
                                        || ev == Event::Escape) {
            confirming_reset_ = false;
            return true;
        }
        return true;
    }

    /* --- 編集モード中 --- */
    if (editing_) {
        const calcyx_setting_desc_t *d =
            PrefsScreen_current_desc(visible_items_, item_);
        if (!d) { editing_ = false; return true; }

        if (ev == Event::Escape) { editing_ = false; edit_buf_.clear(); return true; }

        /* INT spinner: ←→ で増減、 数字直入力、 Enter で commit + clamp。 */
        if (d->kind == CALCYX_SETTING_KIND_INT) {
            auto bump = [&](int delta) {
                int v = std::atoi(edit_buf_.c_str()) + delta;
                v = std::clamp(v, d->i_lo, d->i_hi);
                edit_buf_ = std::to_string(v);
            };
            if (ev == Event::ArrowLeft)  { bump(-1); return true; }
            if (ev == Event::ArrowRight) { bump(+1); return true; }
            if (ev == Event::Backspace) {
                if (!edit_buf_.empty()) edit_buf_.pop_back();
                return true;
            }
            if (ev.is_character()) {
                const std::string &c = ev.character();
                if (c.size() == 1 && ((c[0] >= '0' && c[0] <= '9') ||
                                       (c[0] == '-' && edit_buf_.empty()))) {
                    edit_buf_ += c;
                }
                return true;
            }
            if (ev == Event::Return) {
                int v = std::atoi(edit_buf_.c_str());
                v = std::clamp(v, d->i_lo, d->i_hi);
                commit_current(std::to_string(v));
                editing_ = false;
                return true;
            }
            return true;
        }

        /* COLOR (`#RRGGBB`) / 自由 STRING: テキスト編集。 */
        if (d->kind == CALCYX_SETTING_KIND_COLOR ||
            d->kind == CALCYX_SETTING_KIND_FONT  ||
            d->kind == CALCYX_SETTING_KIND_HOTKEY||
            d->kind == CALCYX_SETTING_KIND_STRING) {
            if (ev == Event::Backspace) {
                if (!edit_buf_.empty()) edit_buf_.pop_back();
                return true;
            }
            if (ev.is_character()) {
                edit_buf_ += ev.character();
                return true;
            }
            if (ev == Event::Return) {
                if (d->kind == CALCYX_SETTING_KIND_COLOR) {
                    unsigned char rgb[3];
                    if (!calcyx_conf_parse_hex_color(edit_buf_.c_str(), rgb)) {
                        app_->flash_message_public("Invalid color (expected #RRGGBB)");
                        return true;
                    }
                }
                commit_current(edit_buf_);
                editing_ = false;
                return true;
            }
            return true;
        }
        editing_ = false;
        return true;
    }

    /* --- 通常モード (BIOS 風) --- */
    /* マウスは別 dispatch。 */
    if (ev.is_mouse()) {
        return handle_mouse(ev.mouse());
    }
    if (ev == Event::Escape || ev == Event::Special("\x11") /* Ctrl+Q */) {
        close();
        return true;
    }
    /* Ctrl+E で外部エディタ起動 (= do_preferences)。 */
    if (ev == Event::Special("\x05")) {
        close();
        app_->do_preferences_public();
        return true;
    }
    /* タブ切替は Tab / Shift+Tab。 ↑↓ は項目移動専用。 */
    if (ev == Event::Tab) { set_tab((tab_ + 1) % kTabCount); return true; }
    if (ev == Event::TabReverse) { set_tab((tab_ + kTabCount - 1) % kTabCount); return true; }
    if (ev == Event::ArrowUp) {
        if (visible_items_.empty()) return true;
        item_ = (item_ + (int)visible_items_.size() - 1) % (int)visible_items_.size();
        return true;
    }
    if (ev == Event::ArrowDown) {
        if (visible_items_.empty()) return true;
        item_ = (item_ + 1) % (int)visible_items_.size();
        return true;
    }

    /* ←/→ は値の循環 (Bool / Choice / COLOR_PRESET) または ±1 (INT)。
     * 編集モードに入らずその場で commit。 BIOS 風。 */
    if (ev == Event::ArrowLeft)  { shift_current(-1); return true; }
    if (ev == Event::ArrowRight) { shift_current(+1); return true; }

    /* Enter / Space: 列挙値 (Bool / Choice / COLOR_PRESET) は循環、 INT /
     * COLOR / 自由 STRING は edit_buf_ に値を移して編集モード突入。 アクション
     * 行は実行。 */
    if (ev == Event::Return || ev == Event::Character(' ')) {
        activate_current();
        return true;
    }
    return true;
}

/* ←/→ で値を変える共通動作。 dir=+1/-1。 マウスからも呼ぶ予定。 */
void PrefsScreen::shift_current(int dir) {
    if (visible_items_.empty()) return;
    const PrefsItem &it = kItems[visible_items_[item_]];
    if (!it.key) return;  /* action 行 */
    if (locked_.count(it.key)) {
        app_->flash_message_public(
            std::string(_("Locked by calcyx.conf.override")));
        return;
    }
    const calcyx_setting_desc_t *d = calcyx_settings_find(it.key);
    if (!d) return;
    if (d->kind == CALCYX_SETTING_KIND_BOOL) {
        bool cur = parse_bool(values_[it.key]);
        commit_current(cur ? "false" : "true");
        return;
    }
    ChoiceList ch = (d->kind == CALCYX_SETTING_KIND_STRING ||
                     d->kind == CALCYX_SETTING_KIND_COLOR_PRESET)
                    ? choices_for(it.key) : ChoiceList{ nullptr, 0 };
    if (ch.items) {
        int cur = 0;
        for (int i = 0; i < ch.count; i++)
            if (values_[it.key] == ch.items[i]) { cur = i; break; }
        cur = (cur + ch.count + dir) % ch.count;
        commit_current(ch.items[cur]);
        return;
    }
    if (d->kind == CALCYX_SETTING_KIND_INT) {
        int v = std::atoi(values_[it.key].c_str()) + dir;
        v = std::clamp(v, d->i_lo, d->i_hi);
        commit_current(std::to_string(v));
        return;
    }
}

/* Enter/Space または行ダブルクリックで発火する共通動作。 */
void PrefsScreen::activate_current() {
    if (visible_items_.empty()) return;
    const PrefsItem &it = kItems[visible_items_[item_]];

    /* 専用アクション行 (= schema 値ではない)。 */
    if (it.action != ACT_NONE) {
        switch (it.action) {
        case ACT_EXTERNAL_EDITOR:
            close();
            app_->do_preferences_public();
            break;
        case ACT_RESET_ALL:
            confirming_reset_ = true;
            break;
        case ACT_PREV_PAGE:
            set_tab((tab_ + kTabCount - 1) % kTabCount);
            break;
        case ACT_NEXT_PAGE:
            set_tab((tab_ + 1) % kTabCount);
            break;
        default:
            break;
        }
        return;
    }

    const calcyx_setting_desc_t *d = calcyx_settings_find(it.key);
    if (!d) return;
    if (locked_.count(it.key)) {
        app_->flash_message_public(
            std::string(_("Locked by calcyx.conf.override")));
        return;
    }
    if (d->kind == CALCYX_SETTING_KIND_BOOL) {
        bool cur = parse_bool(values_[it.key]);
        commit_current(cur ? "false" : "true");
        return;
    }
    ChoiceList ch = (d->kind == CALCYX_SETTING_KIND_STRING ||
                     d->kind == CALCYX_SETTING_KIND_COLOR_PRESET)
                    ? choices_for(it.key) : ChoiceList{ nullptr, 0 };
    if (ch.items) {
        int cur = 0;
        for (int i = 0; i < ch.count; i++)
            if (values_[it.key] == ch.items[i]) { cur = i; break; }
        cur = (cur + 1) % ch.count;
        commit_current(ch.items[cur]);
        return;
    }
    /* INT / COLOR / 自由 STRING は編集モードでテキスト入力。 */
    edit_buf_ = values_[it.key];
    edit_cur_ = edit_buf_.size();
    editing_  = true;
}

void PrefsScreen::set_tab(int new_tab) {
    tab_  = new_tab;
    item_ = 0;
    refresh_visible_items();
    app_->overlay_closed_public();
}

/* マウス処理: タブヘッダクリックで切替、 行クリックで選択 (再クリックで
 * activate)、 ホイールで項目移動。 編集中はホイールと Esc 相当のみ反応。 */
bool PrefsScreen::handle_mouse(const ftxui::Mouse &m) {
    auto inside = [&](const Box &b) {
        return m.x >= b.x_min && m.x <= b.x_max
            && m.y >= b.y_min && m.y <= b.y_max;
    };

    /* ホイールスクロール (= 項目移動)。 編集中も項目移動はせず無視。 */
    if (!editing_ && (m.button == Mouse::WheelUp || m.button == Mouse::WheelDown)) {
        if (visible_items_.empty()) return true;
        if (m.button == Mouse::WheelUp)
            item_ = (item_ + (int)visible_items_.size() - 1) % (int)visible_items_.size();
        else
            item_ = (item_ + 1) % (int)visible_items_.size();
        return true;
    }

    /* 左クリック (Pressed → Released で確定) のみ反応。 */
    if (m.button != Mouse::Left) return true;  /* 他ボタンは吸収のみ */
    if (m.motion != Mouse::Released) return true;

    /* 編集中: 外側クリックで cancel、 中ではノーオペ。 */
    if (editing_) {
        editing_ = false;
        edit_buf_.clear();
        return true;
    }

    /* タイトルバーの [X] クリック → close。 */
    if (inside(close_box_)) {
        close();
        return true;
    }

    /* タブヘッダクリック → タブ切替。 */
    for (int t = 0; t < (int)tab_boxes_.size() && t < kTabCount; t++) {
        if (inside(tab_boxes_[t])) {
            if (t != tab_) set_tab(t);
            return true;
        }
    }

    /* 行クリック → 選択 (= 既選択なら activate)。 */
    for (int r = 0; r < (int)row_boxes_.size() && r < (int)visible_items_.size(); r++) {
        if (inside(row_boxes_[r])) {
            if (r == item_) {
                activate_current();
            } else {
                item_ = r;
            }
            return true;
        }
    }
    return true;
}

Element PrefsScreen::render_value(int item_idx) const {
    const PrefsItem &it = kItems[item_idx];
    if (!it.key) return text("");  /* action 行は値表示なし */

    const calcyx_setting_desc_t *d = calcyx_settings_find(it.key);
    auto vit = values_.find(it.key);
    std::string val = (vit != values_.end()) ? vit->second : "";

    bool is_current = (!visible_items_.empty()
                       && item_idx == visible_items_[item_]);

    /* 行の左右に必ず 2 文字を確保することで、 全行の中央 (= val 表示位置) と
     * 右側の色サンプル位置を揃える。 prefix/suffix は以下のいずれか:
     *  - 通常 (非選択 or 矢印不可 kind): "  " / "  "
     *  - 通常 (選択 + 矢印可): "← " / " →"
     *  - 編集中 (必ず選択):     "[ " / " ]"
     * カーソルは val 末尾に空白の inverted ブロックを 1 文字。 これだけは
     * 編集中のみ +1 文字伸びるが、 サンプル位置への影響を抑えるため val は
     * (右伸縮) 領域内に留める。 */

    bool show_arrows = is_current
                       && d
                       && !locked_.count(it.key)
                       && (d->kind == CALCYX_SETTING_KIND_BOOL
                           || d->kind == CALCYX_SETTING_KIND_INT
                           || d->kind == CALCYX_SETTING_KIND_COLOR_PRESET
                           || (d->kind == CALCYX_SETTING_KIND_STRING
                               && choices_for(it.key).items));

    /* 端末によって ←(U+2190)/→(U+2192) は ambiguous で 1 or 2 cell に変動し、
     * 行間で値の x 座標がブレる。 ASCII '<' '>' で 1 cell 確定にして揃える。 */
    std::string prefix = "  ";
    std::string suffix = "  ";
    if (editing_ && is_current) {
        prefix = "[ ";
        suffix = " ]";
    } else if (show_arrows) {
        prefix = "< ";
        suffix = " >";
    }

    /* 中央 val: 編集中なら edit_buf_ + 反転カーソル、 それ以外は値そのまま。
     * BOOL は [x]/[ ] 表記。 Choice 値 (列挙) は表示時に翻訳経由。 */
    Element center;
    if (editing_ && is_current) {
        center = hbox({ text(edit_buf_), text(" ") | inverted });
    } else if (d && d->kind == CALCYX_SETTING_KIND_BOOL) {
        center = text(parse_bool(val) ? "[x]" : "[ ]");
    } else {
        /* COLOR_PRESET の値 (otaku-black 等) は固有名詞なので翻訳しない.
         * STRING の enum 系 (semantic / mirror_gui 等) のみ choice 翻訳する. */
        bool is_choice = d && d->kind == CALCYX_SETTING_KIND_STRING
                            && choices_for(it.key).items;
        if (is_choice && !val.empty()) {
            center = text(_(val.c_str()));
        } else {
            center = text(val);
        }
    }

    return hbox({ text(prefix), center, text(suffix) });
}

/* 色サンプル (= "    " を bgcolor)。 mirror_gui の COLOR は RGB hex から、
 * semantic の tui_sem_* は ANSI 色名から Color enum を引く。 選択行の
 * inverted から外して表示するため値テキストとは別 Element で返す。 */
Element PrefsScreen::render_color_sample(int item_idx) const {
    const PrefsItem &it = kItems[item_idx];
    if (!it.key) return text("");
    const calcyx_setting_desc_t *d = calcyx_settings_find(it.key);
    if (!d) return text("");

    bool is_current = (!visible_items_.empty()
                       && item_idx == visible_items_[item_]);
    std::string val;
    if (editing_ && is_current) {
        val = edit_buf_;
    } else {
        auto vit = values_.find(it.key);
        val = (vit != values_.end()) ? vit->second : "";
    }

    if (d->kind == CALCYX_SETTING_KIND_COLOR) {
        unsigned char rgb[3];
        if (!calcyx_conf_parse_hex_color(val.c_str(), rgb)) return text("");
        return text("    ") | bgcolor(Color::RGB(rgb[0], rgb[1], rgb[2]));
    }

    /* tui_sem_* (= STRING + Choice) は ANSI 色のサンプル。 */
    if (d->kind == CALCYX_SETTING_KIND_STRING
        && strncmp(it.key, "tui_sem_", 8) == 0) {
        Color c = parse_semantic_color(val, Color::Default);
        return text("    ") | bgcolor(c);
    }
    return text("");
}

Element PrefsScreen::Render() const {
    refresh_visible_items();
    tab_boxes_.assign(kTabCount, Box{});
    row_boxes_.assign(visible_items_.size(), Box{});

    /* タブヘッダ */
    Elements tabs;
    for (int t = 0; t < kTabCount; t++) {
        std::string lbl = std::string(" ") + _(kTabLabels[t]) + " ";
        Element e = text(lbl);
        if (t == tab_) e = e | inverted | bold;
        tabs.push_back(e | reflect(tab_boxes_[t]));
    }
    Element tab_row = hbox(std::move(tabs));

    /* 項目リスト。 行の構成は「label + value | inverted (選択行)」 + 「色
     * サンプル」 の二段組。 inverted は色サンプルを潰すため外に置く。
     * value 列は固定幅にし、 サンプル位置を全行で揃える。 */
    constexpr int kLabelWidth = 32;
    constexpr int kValueWidth = 24;

    /* 行グループ: schema / 個別 action (= EXTERNAL_EDITOR) / ページ移動 nav。
     * 切り替わりと、 schema 内の section 変化で空行を挟む。 schema の
     * section 切替時のみセクションヘッダ行を挿入。 */
    auto group_of = [](const PrefsItem &it) -> int {
        if (it.action == ACT_NONE) return 0;
        if (it.action == ACT_PREV_PAGE || it.action == ACT_NEXT_PAGE) return 2;
        return 1;
    };
    auto section_eq = [](const char *a, const char *b) {
        if (a == b) return true;
        if (!a || !b) return false;
        return strcmp(a, b) == 0;
    };

    Elements rows;
    const PrefsItem *prev = nullptr;
    for (size_t row_idx = 0; row_idx < visible_items_.size(); row_idx++) {
        int idx = visible_items_[row_idx];
        const PrefsItem &it = kItems[idx];
        bool is_locked  = it.key && locked_.count(it.key) > 0;
        bool is_selected = ((int)row_idx == item_);

        bool insert_blank  = false;
        bool insert_header = false;
        if (!prev) {
            insert_header = (it.section != nullptr);
        } else {
            int gp = group_of(*prev), gc = group_of(it);
            if (gp != gc) insert_blank = true;
            if (gc == 0 && !section_eq(prev->section, it.section)) {
                insert_blank = true;
                insert_header = (it.section != nullptr);
            }
        }
        if (insert_blank)  rows.push_back(text(" "));
        if (insert_header) rows.push_back(hbox({
            text("-- "),
            text(_(it.section)),
            text(" --"),
        }) | bold | dim);
        prev = &it;

        /* ftxui の cell width 計算と端末 (例: macOS Terminal) の表示幅が
         * 日本語混じりラベルで一致しないことがあり、 size(WIDTH, EQUAL) の
         * trunc/pad だけでは右側の value の x 座標が行ごとにブレる。
         * 自前で UTF-8 cell width を計算し、 ASCII 空白で pad して text() に
         * 渡すことで、 ftxui と端末の両方が「同じ cell 数」 と認識する。 */
        std::string label_str = pad_to_width(std::string("  ") + _(it.label),
                                             kLabelWidth);
        Element label = text(label_str);
        Element value = render_value(idx);
        Element body = hbox({
            label,
            value | size(WIDTH, EQUAL, kValueWidth),
        });
        if (is_locked)   body = body | dim;
        if (is_selected) body = body | inverted;
        /* 色サンプルは inverted の外に置く (反転で潰れるのを避けるため)。 */
        Element sample = render_color_sample(idx);
        Element row = hbox({ body, text("  "), sample });
        /* 選択行に focus を当てると yframe が画面内に収まるようスクロールする。 */
        if (is_selected) row = row | focus;
        row = row | reflect(row_boxes_[row_idx]);
        rows.push_back(row);
    }

    /* status hint: モードに応じて切替 */
    std::string hint_text;
    if (confirming_reset_) {
        hint_text = _(" Reset all settings to defaults? "
                      "(Y to confirm / N or Esc to cancel) ");
    } else if (editing_) {
        const calcyx_setting_desc_t *d =
            PrefsScreen_current_desc(visible_items_, item_);
        if (d && d->kind == CALCYX_SETTING_KIND_INT)
            hint_text = _(" ←→ ±1  0-9 type  Bksp  Enter ok  Esc cancel ");
        else
            hint_text = _(" type chars  Bksp  Enter ok  Esc cancel ");
    } else {
        hint_text = _(" Tab/Shift+Tab tab  ↑↓ item  ←→ change  "
                      "Enter edit/run  Ctrl+E ext-editor  Esc close ");
    }
    Element hint = text(hint_text) | dim;

    /* タイトルバー: 左寄せのアプリ名 + 右寄せの閉じるボタン [X] (= マウス
     * クリックで close)。 reflect で Box を取って handle_mouse で hit-test。 */
    Element close_btn = text(" [X] ") | bold | reflect(close_box_);
    Element title_bar = hbox({
        text(_(" calcyx Preferences ")) | bold,
        filler(),
        close_btn,
    });

    Element body = vbox({
        title_bar,
        separator(),
        tab_row,
        separator(),
        vbox(std::move(rows)) | yframe | flex,
        separator(),
        hint,
    });

    /* palette が active ならクロームカラーを当てる。 mirror_gui のとき効く。 */
    const TuiPalette &p = app_->test_sheet()->palette();
    if (p.active) {
        body = body
             | color   (Color::RGB(p.text.r, p.text.g, p.text.b))
             | bgcolor (Color::RGB(p.bg.r,   p.bg.g,   p.bg.b));
    }
    return body;
}

} // namespace calcyx::tui
