// 移植元: Calctus/UI/Sheets/InputCandidateForm.cs (簡略版)

#include "CompletionPopup.h"
#include "colors.h"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <algorithm>
#include <cctype>

// ---- 大小無視の文字列マッチ ----
static bool icontains(const std::string &hay, const std::string &needle) {
    if (needle.empty()) return true;
    auto it = std::search(hay.begin(), hay.end(),
                          needle.begin(), needle.end(),
                          [](char a, char b){ return std::tolower((unsigned char)a)
                                                  == std::tolower((unsigned char)b); });
    return it != hay.end();
}

static bool istartswith(const std::string &s, const std::string &p) {
    if (p.size() > s.size()) return false;
    for (size_t i = 0; i < p.size(); i++)
        if (std::tolower((unsigned char)s[i]) != std::tolower((unsigned char)p[i]))
            return false;
    return true;
}
CompletionPopup::CompletionPopup()
    : Fl_Group(0, 0, POP_W, 0)
{
    list_ = new Fl_Select_Browser(0, 0, POP_W, 0);
    list_->textfont(FL_COURIER);
    list_->textsize(13);
    list_->box(FL_FLAT_BOX);
    list_->callback(list_cb, this);

    desc_ = new Fl_Box(0, 0, POP_W, DESC_H);
    desc_->box(FL_FLAT_BOX);
    desc_->labelfont(FL_HELVETICA);
    desc_->labelsize(11);
    desc_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);

    apply_colors();

    end();
    hide();  // 最初は非表示
}

void CompletionPopup::apply_colors() {
    list_->color(g_colors.pop_bg);
    list_->selection_color(g_colors.pop_sel);
    list_->textcolor(g_colors.pop_text);
    desc_->color(g_colors.pop_desc_bg);
    desc_->labelcolor(g_colors.pop_desc);
}

// ---- draw: 背景・枠線を描いてから子ウィジェットを描画 ----
void CompletionPopup::draw() {
    fl_draw_box(FL_FLAT_BOX, x(), y(), w(), h(), g_colors.pop_bg);
    draw_children();
    fl_color(g_colors.pop_border);
    fl_rect(x(), y(), w(), h());
}

// ---- resize: グループ移動に合わせて子を再配置 ----
void CompletionPopup::resize(int nx, int ny, int nw, int nh) {
    Fl_Group::resize(nx, ny, nw, nh);
    int lh = nh - DESC_H;
    if (lh < 0) lh = 0;
    list_->resize(nx, ny,      nw, lh);
    desc_->resize(nx, ny + lh, nw, DESC_H);
}

// ---- 公開 API ----

void CompletionPopup::set_all(std::vector<Candidate> all) {
    all_ = std::move(all);
}

void CompletionPopup::show_at(int wx, int wy_below, int editor_top,
                               const std::string &key) {
    rebuild(key);
    if (filtered_.empty()) { hide_popup(); return; }

    int pw = parent() ? parent()->w() : Fl::w();
    int ph = parent() ? parent()->h() : Fl::h();

    // X: ウィンドウ右端に収まるよう調整
    if (wx + POP_W > pw) wx = pw - POP_W;
    if (wx < 0) wx = 0;

    // Y: 下に収まらなければエディタの上に表示
    // どちらにも収まらない場合は空間の広い方に高さを縮小して収める
    int wy = wy_below;
    int h  = cur_h_;
    if (wy + h > ph) {
        // 上に表示できるか試みる
        int space_above = editor_top;
        int space_below = ph - wy_below;
        if (space_above >= h) {
            wy = editor_top - h;
        } else if (space_above >= space_below) {
            // 上の方が広い → 上に縮小して表示
            h  = space_above;
            wy = 0;
        } else {
            // 下の方が広い → 下に縮小して表示
            h  = space_below;
            wy = wy_below;
        }
        if (h < DESC_H + (list_->textsize() + 4)) {
            hide_popup();  // 1行すら入らないなら非表示
            return;
        }
    }

    shown_ = true;
    max_h_ = h;   // この表示位置での上限高さを記憶
    resize(wx, wy, POP_W, h);
    show();
    if (parent()) parent()->redraw();
}

void CompletionPopup::update_key(const std::string &key) {
    if (!shown_) return;
    rebuild(key);
    if (filtered_.empty()) { hide_popup(); return; }
    // max_h_ を超えない範囲でリサイズ
    int h = (max_h_ > 0) ? std::min(cur_h_, max_h_) : cur_h_;
    resize(x(), y(), POP_W, h);
    if (parent()) parent()->redraw();
}

void CompletionPopup::hide_popup() {
    shown_ = false;
    hide();
    if (parent()) parent()->redraw();
}

void CompletionPopup::select_prev() {
    int cur = list_->value();
    if (cur > 1) list_->value(cur - 1);
    else         list_->value((int)filtered_.size());
    const Candidate *c = selected();
    desc_->copy_label(c ? c->description.c_str() : "");
    redraw();
}

void CompletionPopup::select_next() {
    int cur = list_->value();
    int n   = (int)filtered_.size();
    if (cur < n) list_->value(cur + 1);
    else         list_->value(1);
    const Candidate *c = selected();
    desc_->copy_label(c ? c->label.c_str() : "");
    redraw();
}

const Candidate *CompletionPopup::selected() const {
    int idx = list_->value();
    if (idx < 1 || idx > (int)filtered_.size()) return nullptr;
    return &filtered_[idx - 1];
}

// ---- 内部: リスト再構築 ----
void CompletionPopup::rebuild(const std::string &key) {
    std::vector<Candidate> b1, b2;
    for (auto &c : all_) {
        if (key.empty() || istartswith(c.id, key)) b1.push_back(c);
        else if (icontains(c.id, key))             b2.push_back(c);
    }
    filtered_.clear();
    for (auto &c : b1) filtered_.push_back(c);
    for (auto &c : b2) filtered_.push_back(c);

    list_->clear();
    for (auto &c : filtered_)
        list_->add(c.label.c_str());

    if (!filtered_.empty()) {
        list_->value(1);
        desc_->copy_label(filtered_[0].description.c_str());
    } else {
        desc_->copy_label("");
    }

    int n      = std::min((int)filtered_.size(), MAX_VIS);
    int item_h = list_->textsize() + 4;
    int lh     = std::max(n * item_h, item_h);
    cur_h_     = lh + DESC_H;
}

void CompletionPopup::list_cb(Fl_Widget *, void *data) {
    auto *self = static_cast<CompletionPopup *>(data);
    const Candidate *c = self->selected();
    self->desc_->copy_label(c ? c->description.c_str() : "");
    self->redraw();
}
