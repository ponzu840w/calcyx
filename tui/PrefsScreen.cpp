/* TUI Preferences 画面 phase 1 実装 (read-only スケルトン)。 */

#include "PrefsScreen.h"
#include "TuiApp.h"
#include "i18n.h"

extern "C" {
#include "settings_schema.h"
#include "settings_io.h"
}

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <algorithm>

using namespace ftxui;

namespace calcyx::tui {

/* ---- 編集対象項目テーブル (= 「TUI に効くもの限定」) ---- */
namespace {

enum Tab { TAB_GENERAL = 0, TAB_NUMBER = 1, TAB_INPUT = 2, TAB_COLORS = 3 };

struct PrefsItem {
    int   tab;
    const char *key;    /* schema_key */
    const char *label;  /* i18n raw key (fallback: schema_key) */
};

constexpr PrefsItem kItems[] = {
    /* General */
    { TAB_GENERAL, "language",                 "Language" },
    { TAB_GENERAL, "max_array_length",         "Max array length" },
    { TAB_GENERAL, "max_string_length",        "Max string length" },
    { TAB_GENERAL, "max_call_depth",           "Max call depth" },
    { TAB_GENERAL, "tui_color_source",         "Color source" },
    { TAB_GENERAL, "tui_clear_after_overlay",  "Clear after overlay" },

    /* Number Format */
    { TAB_NUMBER,  "decimal_digits",           "Decimal digits" },
    { TAB_NUMBER,  "e_notation",               "E notation" },
    { TAB_NUMBER,  "e_positive_min",           "E positive min" },
    { TAB_NUMBER,  "e_negative_max",           "E negative max" },
    { TAB_NUMBER,  "e_alignment",              "E alignment" },

    /* Input */
    { TAB_INPUT,   "auto_completion",          "Auto completion" },
    { TAB_INPUT,   "bs_delete_empty_row",      "BS deletes empty row" },

    /* Colors */
    { TAB_COLORS,  "color_preset",             "Color preset" },
    { TAB_COLORS,  "color_bg",                 "Background" },
    { TAB_COLORS,  "color_sel_bg",             "Selection" },
    { TAB_COLORS,  "color_rowline",            "Row line" },
    { TAB_COLORS,  "color_text",               "Text" },
    { TAB_COLORS,  "color_accent",             "Accent" },
    { TAB_COLORS,  "color_symbol",             "Symbols" },
    { TAB_COLORS,  "color_ident",              "Identifiers" },
    { TAB_COLORS,  "color_special",            "Literals" },
    { TAB_COLORS,  "color_si_pfx",             "SI prefix" },
    { TAB_COLORS,  "color_paren0",             "Paren 1" },
    { TAB_COLORS,  "color_paren1",             "Paren 2" },
    { TAB_COLORS,  "color_paren2",             "Paren 3" },
    { TAB_COLORS,  "color_paren3",             "Paren 4" },
    { TAB_COLORS,  "color_error",              "Error" },
    { TAB_COLORS,  "color_ui_win_bg",          "Window BG" },
    { TAB_COLORS,  "color_ui_bg",              "Dialog BG" },
    { TAB_COLORS,  "color_ui_input",           "UI input" },
    { TAB_COLORS,  "color_ui_btn",             "UI button" },
    { TAB_COLORS,  "color_ui_menu",            "Menu BG" },
    { TAB_COLORS,  "color_ui_text",            "UI text" },
    { TAB_COLORS,  "color_ui_label",           "UI label" },
    { TAB_COLORS,  "color_ui_dim",             "UI dim" },
    { TAB_COLORS,  "color_pop_bg",             "Popup BG" },
    { TAB_COLORS,  "color_pop_sel",            "Popup sel" },
    { TAB_COLORS,  "color_pop_text",           "Popup text" },
    { TAB_COLORS,  "color_pop_desc",           "Popup desc" },
    { TAB_COLORS,  "color_pop_desc_bg",        "Popup desc BG" },
    { TAB_COLORS,  "color_pop_border",         "Popup border" },
};
constexpr int kItemsCount = (int)(sizeof(kItems) / sizeof(kItems[0]));

constexpr const char *kTabLabels[] = {
    "General", "Number Format", "Input", "Colors"
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

    /* schema で定義されているが conf に未出現の key には既定値を流し込む。 */
    for (int i = 0; i < kItemsCount; i++) {
        const char *k = kItems[i].key;
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
    for (int i = 0; i < kItemsCount; i++) {
        if (kItems[i].tab == tab_) visible_items_.push_back(i);
    }
}

bool PrefsScreen::OnEvent(Event ev) {
    if (!visible_) return false;

    if (editing_) {
        /* phase 2 で実装。 とりあえず Esc だけ受ける。 */
        if (ev == Event::Escape) { editing_ = false; return true; }
        return true;  /* 他キーは吸収 */
    }

    if (ev == Event::Escape || ev == Event::Special("\x11") /* Ctrl+Q */) {
        close();
        return true;
    }
    if (ev == Event::ArrowLeft)  { tab_ = (tab_ + kTabCount - 1) % kTabCount;
                                    item_ = 0; refresh_visible_items();
                                    app_->overlay_closed_public(); return true; }
    if (ev == Event::ArrowRight) { tab_ = (tab_ + 1) % kTabCount;
                                    item_ = 0; refresh_visible_items();
                                    app_->overlay_closed_public(); return true; }
    if (ev == Event::ArrowUp || ev == Event::TabReverse) {
        if (visible_items_.empty()) return true;
        item_ = (item_ + (int)visible_items_.size() - 1) % (int)visible_items_.size();
        return true;
    }
    if (ev == Event::ArrowDown || ev == Event::Tab) {
        if (visible_items_.empty()) return true;
        item_ = (item_ + 1) % (int)visible_items_.size();
        return true;
    }
    /* phase 2: Enter/Space で編集モード突入。 */
    return true;  /* prefs 中は他のキーを sheet に流さない */
}

Element PrefsScreen::render_value(int item_idx) const {
    const PrefsItem &it = kItems[item_idx];
    auto vit = values_.find(it.key);
    std::string val = (vit != values_.end()) ? vit->second : "";
    /* phase 1 では生文字列をそのまま表示。 phase 2 で kind 別装飾 (Bool→[*]/[ ]
     * 等) を入れる。 */
    return text(val);
}

Element PrefsScreen::Render() const {
    refresh_visible_items();

    /* タブヘッダ */
    Elements tabs;
    for (int t = 0; t < kTabCount; t++) {
        std::string lbl = std::string(" ") + _(kTabLabels[t]) + " ";
        Element e = text(lbl);
        if (t == tab_) e = e | inverted | bold;
        tabs.push_back(e);
    }
    Element tab_row = hbox(std::move(tabs));

    /* 項目リスト */
    Elements rows;
    for (size_t row_idx = 0; row_idx < visible_items_.size(); row_idx++) {
        int idx = visible_items_[row_idx];
        const PrefsItem &it = kItems[idx];
        bool is_locked = locked_.count(it.key) > 0;
        Element label = text(std::string("  ") + _(it.label));
        Element value = render_value(idx);
        Element row = hbox({
            label | size(WIDTH, EQUAL, 30),
            value | flex,
            text("  "),
        });
        if (is_locked)            row = row | dim;
        if ((int)row_idx == item_) row = row | inverted;
        rows.push_back(row);
    }

    /* status hint */
    Element hint = text(_(" ←→ tab  ↑↓ item  Enter edit  "
                          "Ctrl+E external  Esc close ")) | dim;

    Element body = vbox({
        hbox({ text(_(" Preferences ")) | bold, filler() }),
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
