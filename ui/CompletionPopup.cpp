// 移植元: Calctus/UI/Sheets/InputCandidateForm.cs (簡略版)

#include "CompletionPopup.h"
#include "completion_match.h"
#include "colors.h"
#include "MainWindow.h"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <algorithm>

#ifdef __APPLE__
extern "C" void mac_configure_popup(Fl_Window *popup, Fl_Window *main_win);
#endif

// ==================================================================
// CompletionPopupBase — widget に依存しない共通ロジック
// ==================================================================

void CompletionPopupBase::init_widgets(int w) {
    list_ = new Fl_Select_Browser(0, 0, w, 0);
    list_->textfont(FL_COURIER);
    list_->textsize(13);
    list_->box(FL_FLAT_BOX);
    list_->callback(list_cb, this);

    desc_ = new Fl_Box(0, 0, w, DESC_H);
    desc_->box(FL_FLAT_BOX);
    desc_->labelfont(FL_HELVETICA);
    desc_->labelsize(11);
    desc_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);

    apply_colors();
}

void CompletionPopupBase::apply_colors() {
    if (!list_ || !desc_) return;
    list_->color(g_colors.pop_bg);
    list_->selection_color(g_colors.pop_sel);
    list_->textcolor(g_colors.pop_text);
    desc_->color(g_colors.pop_desc_bg);
    desc_->labelcolor(g_colors.pop_desc);
}

void CompletionPopupBase::set_all(std::vector<Candidate> all) {
    all_ = std::move(all);
}

void CompletionPopupBase::select_prev() {
    int cur = list_->value();
    if (cur > 1) list_->value(cur - 1);
    else         list_->value((int)filtered_.size());
    const Candidate *c = selected();
    desc_->copy_label(c ? c->description.c_str() : "");
    if (list_->parent()) list_->parent()->redraw();
}

void CompletionPopupBase::select_next() {
    int cur = list_->value();
    int n   = (int)filtered_.size();
    if (cur < n) list_->value(cur + 1);
    else         list_->value(1);
    const Candidate *c = selected();
    desc_->copy_label(c ? c->description.c_str() : "");
    if (list_->parent()) list_->parent()->redraw();
}

const Candidate *CompletionPopupBase::selected() const {
    int idx = list_->value();
    if (idx < 1 || idx > (int)filtered_.size()) return nullptr;
    return &filtered_[idx - 1];
}

void CompletionPopupBase::rebuild(const std::string &key) {
    std::vector<Candidate> b1, b2;
    for (auto &c : all_) {
        if (key.empty() || completion_istartswith(c.id.c_str(), key.c_str())) b1.push_back(c);
        else if (completion_icontains(c.id.c_str(), key.c_str()))             b2.push_back(c);
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

void CompletionPopupBase::list_cb(Fl_Widget *, void *data) {
    auto *self = static_cast<CompletionPopupBase *>(data);
    const Candidate *c = self->selected();
    self->desc_->copy_label(c ? c->description.c_str() : "");
    if (self->list_->parent()) self->list_->parent()->redraw();
}

// ==================================================================
// CompletionPopup — メインウィンドウ子 Fl_Group (埋め込み)
// ==================================================================

CompletionPopup::CompletionPopup()
    : Fl_Group(0, 0, POP_W, 0)
{
    init_widgets(POP_W);
    end();
    hide();
}

void CompletionPopup::draw() {
    fl_draw_box(FL_FLAT_BOX, x(), y(), w(), h(), g_colors.pop_bg);
    draw_children();
    fl_color(g_colors.pop_border);
    fl_rect(x(), y(), w(), h());
}

void CompletionPopup::resize(int nx, int ny, int nw, int nh) {
    Fl_Group::resize(nx, ny, nw, nh);
    int lh = nh - DESC_H;
    if (lh < 0) lh = 0;
    list_->resize(nx, ny,      nw, lh);
    desc_->resize(nx, ny + lh, nw, DESC_H);
}

void CompletionPopup::show_at(int wx, int wy_below, int editor_top,
                               const std::string &key) {
    rebuild(key);
    if (filtered_.empty()) { hide_popup(); return; }

    int pw = parent() ? parent()->w() : Fl::w();
    int ph = parent() ? parent()->h() : Fl::h();

    // X: 親の右端に収まるよう調整
    if (wx + POP_W > pw) wx = pw - POP_W;
    if (wx < 0) wx = 0;

    // Y: 下に収まらなければエディタの上に表示
    int wy = wy_below;
    int h  = cur_h_;
    if (wy + h > ph) {
        int space_above = editor_top;
        int space_below = ph - wy_below;
        if (space_above >= h) {
            wy = editor_top - h;
        } else if (space_above >= space_below) {
            h  = space_above;
            wy = 0;
        } else {
            h  = space_below;
            wy = wy_below;
        }
        if (h < DESC_H + (list_->textsize() + 4)) {
            hide_popup();
            return;
        }
    }

    shown_ = true;
    max_h_ = h;
    resize(wx, wy, POP_W, h);
    show();
    if (parent()) parent()->redraw();
}

void CompletionPopup::update_key(const std::string &key) {
    if (!shown_) return;
    rebuild(key);
    if (filtered_.empty()) { hide_popup(); return; }
    int h = (max_h_ > 0) ? std::min(cur_h_, max_h_) : cur_h_;
    resize(x(), y(), POP_W, h);
    if (parent()) parent()->redraw();
}

void CompletionPopup::hide_popup() {
    shown_ = false;
    hide();
    if (parent()) parent()->redraw();
}

bool CompletionPopup::contains_window_point(int wx, int wy) const {
    return wx >= x() && wx < x() + w() && wy >= y() && wy < y() + h();
}

// ==================================================================
// CompletionPopupWindow — 独立 borderless トップレベルウィンドウ
// ==================================================================

CompletionPopupWindow::CompletionPopupWindow(MainWindow *main)
    : Fl_Menu_Window(POP_W, DESC_H)
    , main_(main)
{
    // set_override() は NOBORDER + OVERRIDE フラグを立てる。X11 では
    // override_redirect ウィンドウとして生成され WM が管理しないので、
    // show() しても親ウィンドウから入力フォーカスが奪われない。
    // Windows / macOS でも borderless + 非アクティブ化挙動になるため
    // クロスプラットフォームで安全。Fl_Menu_Window は既定では override
    // を立てないので明示的に呼ぶ必要がある (tooltip/menu の _NET_WM_TYPE
    // だけでは WM に焦点移動を止めさせられない)。
    set_override();
    init_widgets(POP_W);
    end();
    hide();
}

void CompletionPopupWindow::draw() {
    // 背景を自前で塗ってから子を描画し、最後に枠線を重ねる。
    fl_draw_box(FL_FLAT_BOX, 0, 0, w(), h(), g_colors.pop_bg);
    draw_child(*list_);
    draw_child(*desc_);
    fl_color(g_colors.pop_border);
    fl_rect(0, 0, w(), h());
}

void CompletionPopupWindow::show_at(int wx, int wy_below, int editor_top,
                                     const std::string &key) {
    rebuild(key);
    if (filtered_.empty()) { hide_popup(); return; }

    // ウィンドウ相対 → 画面絶対座標
    int sx       = main_->x_root() + wx;
    int sy_below = main_->y_root() + wy_below;
    int sy_top   = main_->y_root() + editor_top;

    // 主ウィンドウがあるスクリーンの作業領域を取得。
    // Fl::w()/Fl::h() はプライマリモニタ固定なので、主ウィンドウが
    // サブモニタにある場合にポップアップがプライマリに吸い寄せられる
    // (画面半分より右に出られないように見える) 挙動になる。
    int scr_x, scr_y, scr_w, scr_h;
    Fl::screen_work_area(scr_x, scr_y, scr_w, scr_h, main_->screen_num());

    // X: 作業領域内に収める
    if (sx + POP_W > scr_x + scr_w) sx = scr_x + scr_w - POP_W;
    if (sx < scr_x) sx = scr_x;

    // Y: 原則として下に表示。独立ウィンドウはメインウィンドウ外にも
    // 出せるので、下の空間は画面下端まで使える = ほとんどの場合は
    // 下に表示できる。下の余白が最小サイズに満たないときだけ上に
    // 回す。
    int sy          = sy_below;
    int h           = cur_h_;
    int scr_bottom  = scr_y + scr_h;
    int space_below = scr_bottom - sy_below;
    int min_h       = DESC_H + (list_->textsize() + 4);

    if (h > space_below) {
        if (space_below >= min_h) {
            // 下に縮小表示
            h = space_below;
        } else {
            // 下が狭すぎるので上へ退避
            int space_above = sy_top - scr_y;
            if (space_above >= h) {
                sy = sy_top - h;
            } else if (space_above >= min_h) {
                sy = scr_y;
                h  = space_above;
            } else {
                hide_popup();  // どちらにも入らない
                return;
            }
        }
    }

    shown_ = true;
    max_h_ = h;

    int lh = h - DESC_H;
    if (lh < 0) lh = 0;
    list_->resize(0, 0,  POP_W, lh);
    desc_->resize(0, lh, POP_W, DESC_H);

    resize(sx, sy, POP_W, h);
    show();
#ifdef __APPLE__
    // macOS: ポップアップをメインウィンドウより手前に置き、フォーカスを戻す
    mac_configure_popup(this, main_);
#endif
}

void CompletionPopupWindow::update_key(const std::string &key) {
    if (!shown_) return;
    rebuild(key);
    if (filtered_.empty()) { hide_popup(); return; }
    int h = (max_h_ > 0) ? std::min(cur_h_, max_h_) : cur_h_;

    int lh = h - DESC_H;
    if (lh < 0) lh = 0;
    list_->resize(0, 0,  POP_W, lh);
    desc_->resize(0, lh, POP_W, DESC_H);

    size(w(), h);
    redraw();
}

void CompletionPopupWindow::hide_popup() {
    shown_ = false;
    hide();
}

bool CompletionPopupWindow::contains_window_point(int /*wx*/, int /*wy*/) const {
    // 別 OS ウィンドウなのでメインウィンドウ座標に popup は存在しない。
    return false;
}
