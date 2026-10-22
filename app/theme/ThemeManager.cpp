#include "app/theme/ThemeManager.h"

#include "core/base/assert.h"
#include "core/base/tsan.h"

#include <QAbstractAnimation>
#include <QAbstractItemView>
#include <QApplication>
#include <QFile>
#include <QFocusFrame>
#include <QGroupBox>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPointer>
#include <QProgressBar>
#include <QProxyStyle>
#include <QRegularExpression>
#include <QScreen>
#include <QStyleFactory>
#include <QStyleHints>
#include <QStyleOption>
#include <QTabBar>
#include <QTextEdit>
#include <QToolTip>

#include <atomic>
#include <cmath>
#include <utility>

using namespace Qt::StringLiterals;

namespace trion::theme {

namespace {

std::atomic<ThemeManager*> g_instance{nullptr};

bool sameMode(tok::Mode a, tok::Mode b)
{
    return a.light == b.light && a.comfortable == b.comfortable && a.lite == b.lite &&
           a.reducedMotion == b.reducedMotion;
}

// px → pt theo DPI logic của màn hình: QFont::setPixelSize và QSS "px" chỉ nhận số nguyên (10,5 px thành 11).
double ptFromPx(double px)
{
    const QScreen* s = QGuiApplication::primaryScreen();
    return px * 72.0 / (s ? s->logicalDotsPerInchY() : 96.0);
}

// Lớp ít thể hiện: gắn QSS của app.qss.in (TrionStyle::polish). Lớp nhiều thể hiện do TrionStyle vẽ, để đổi chủ đề
// không phải polish lại. Đã đo, nên TrionStyle vẽ luôn: menu (mỗi QComboBox dựng một QMenu tạm khi đổi palette,
// gắn QSS cho nó tốn ~50 ms/lần đổi với 190 combobox), tab (QSS trên QTabBar làm QTabWidget cha thành
// WA_StyleSheet, cả cây con thôi nhận palette của ứng dụng), tooltip (QTipLabel tự thay QSS riêng của nó).
bool wantsQss(const QWidget* w)
{
    return qobject_cast<const QHeaderView*>(w) || qobject_cast<const QProgressBar*>(w);
}

bool isPopup(const QWidget* w) { return w->windowType() == Qt::Popup || w->windowType() == Qt::ToolTip; }

// Widget đang hiện chữ có '&' (nút, nhãn, tab, tiêu đề nhóm, menu): vẽ lại khi Alt đổi. Không vẽ lại cả cửa sổ (vùng vẽ
// bản vẽ không có chữ tắt).
bool hasMnemonic(const QWidget* w)
{
    if (qobject_cast<const QMenu*>(w))
        return true;
    if (const auto* t = qobject_cast<const QTabBar*>(w)) {
        for (int i = 0; i < t->count(); ++i)
            if (t->tabText(i).contains(u'&'))
                return true;
        return false;
    }
    return w->property(qobject_cast<const QGroupBox*>(w) ? "title" : "text").toString().contains(u'&');
}

// Chữ tắt '&' gạch chân khi đang giữ Alt (Windows, Linux; 07.1 §13.6: macOS không gạch chân). Bộ lọc của ứng dụng:
// phím đi tới widget có focus, không qua cửa sổ. Rời ứng dụng khi còn giữ (Alt+Tab) thì tắt.
class AltWatch final : public QObject {
public:
    bool down = false;

protected:
    bool eventFilter(QObject* o, QEvent* e) override
    {
        TRION_TSAN_ACQUIRE(e); // sự kiện đánh thức của UiExecutor dựng ở luồng nền (core/base/tsan.h)
        const QEvent::Type t = e->type();
        if (t == QEvent::KeyPress || t == QEvent::KeyRelease) {
            if (const auto* k = static_cast<QKeyEvent*>(e); k->key() == Qt::Key_Alt && !k->isAutoRepeat())
                set(t == QEvent::KeyPress);
        } else if (t == QEvent::ApplicationDeactivate) {
            set(false);
        }
        return QObject::eventFilter(o, e);
    }

private:
    void set(bool on)
    {
        if (std::exchange(down, on) == on)
            return;
        for (QWidget* w : QApplication::allWidgets())
            if (w->isVisible() && hasMnemonic(w))
                w->update();
    }
};

} // namespace

// Fusion + token của chế độ hiện tại: chiều cao theo mật độ, thời gian chuyển động, vẽ phẳng theo prototype
// (.btn, .ib, .inp, .chk, .menu, .tabs, .tip, :focus-visible). Màu nền, chữ lấy từ QPalette.
class TrionStyle final : public QProxyStyle {
public:
    TrionStyle() : QProxyStyle(QStyleFactory::create(u"Fusion"_s)) {}

    tok::Mode mode;
    QString qss;

    QColor c(tok::ColorId id) const { return toQColor(tok::color(id, mode)); }
    int px(tok::PxId id) const { return qRound(tok::px(id, mode)); }

    int pixelMetric(PixelMetric m, const QStyleOption* o, const QWidget* w) const override
    {
        switch (m) {
        case PM_ToolTipLabelFrameWidth: // .tip: QTipLabel lề = 1 + giá trị này
            return px(tok::sp_3) - 1;
        case PM_MenuPanelWidth: // .menu: viền 1 px, lề --sp-2
            return 1;
        case PM_MenuHMargin:
        case PM_MenuVMargin:
            return px(tok::sp_2);
        case PM_FocusFrameHMargin: // viền focus của ô nhiều dòng (QFocusFrame): 1 px ngoài mép như .inp:focus
        case PM_FocusFrameVMargin:
            return qobject_cast<const QFocusFrame*>(w) ? 1 : QProxyStyle::pixelMetric(m, o, w);
        default:
            return QProxyStyle::pixelMetric(m, o, w);
        }
    }

    int styleHint(StyleHint h, const QStyleOption* o, const QWidget* w, QStyleHintReturn* r) const override
    {
        if (h == SH_Widget_Animation_Duration)
            return tok::ms(tok::t, mode);
        if (h == SH_UnderlineShortcut) // chữ tắt '&': chỉ khi giữ Alt; macOS không bao giờ
            return alt_.down;
        if (h == SH_FocusFrame_AboveWidget) // vùng nhìn của ô nhiều dòng phủ kín ô: viền vẽ đè lên
            return true;
        if (h == SH_FocusFrame_Mask) { // vòng 2 px
            if (auto* mask = qstyleoption_cast<QStyleHintReturnMask*>(r); mask && o)
                mask->region = QRegion(o->rect) - QRegion(o->rect.adjusted(2, 2, -2, -2));
            return true;
        }
        return QProxyStyle::styleHint(h, o, w, r);
    }

    QSize sizeFromContents(ContentsType t, const QStyleOption* o, const QSize& s, const QWidget* w) const override
    {
        QSize r = QProxyStyle::sizeFromContents(t, o, s, w);
        switch (t) {
        case CT_LineEdit: // ô không viền (trong spinbox, combobox sửa được) giữ cỡ tự nhiên
            if (const auto* f = qstyleoption_cast<const QStyleOptionFrame*>(o); f && f->lineWidth <= 0)
                break;
            [[fallthrough]];
        case CT_PushButton:
        case CT_ComboBox:
        case CT_SpinBox: // --h-ctl gồm viền 1 px hai phía; chữ to hơn thì theo chữ
            r.setHeight(std::max(px(tok::h_ctl), s.height() + 2));
            break;
        case CT_ToolButton: // .ib: cao --h-ctl, rộng ít nhất --h-ctl với biểu tượng 16 px
            if (s.height() <= px(tok::ic))
                r = QSize(std::max(r.width(), px(tok::h_ctl)), px(tok::h_ctl));
            break;
        case CT_ItemViewItem: // dòng palette, bảng: --h-row
            r.setHeight(std::max(r.height(), px(tok::h_row)));
            break;
        case CT_TabBarTab: // .tabs .tab: --h-ftab + gạch chân 2 px (30 px ở mật độ gọn)
            r.setHeight(px(tok::h_ftab) + 2);
            break;
        case CT_MenuItem: // .menu .mi: cao --h-ctl; .ms: nét 1 px, lề --sp-2 trên dưới
            if (const auto* mi = qstyleoption_cast<const QStyleOptionMenuItem*>(o))
                r.setHeight(mi->menuItemType == QStyleOptionMenuItem::Separator ? 1 + 2 * px(tok::sp_2)
                                                                                : std::max(px(tok::h_ctl), s.height()));
            break;
        default:
            break;
        }
        return r;
    }

    void drawControl(ControlElement ce, const QStyleOption* o, QPainter* p, const QWidget* w) const override
    {
        if (ce == CE_FocusFrame) { // QFocusFrame của ô nhiều dòng; hình vòng do mặt nạ SH_FocusFrame_Mask
            p->fillRect(o->rect, c(tok::c_focus));
            return;
        }
        const auto* tab = qstyleoption_cast<const QStyleOptionTab*>(o);
        if (ce == CE_TabBarTabShape && tab) { // phẳng; tab đang mở gạch chân 2 px --c-accent, lùi --sp-4 hai bên
            if (o->state & State_Selected)
                p->fillRect(QRect(o->rect.left() + px(tok::sp_4), o->rect.bottom() - 1,
                                  o->rect.width() - 2 * px(tok::sp_4), 2),
                            c(tok::c_accent));
            return;
        }
        if (const auto* mi = qstyleoption_cast<const QStyleOptionMenuItem*>(o); ce == CE_MenuItem && mi) {
            if (mi->menuItemType == QStyleOptionMenuItem::Separator) { // .ms: nét --c-border, lùi --sp-1
                const int y = o->rect.center().y();
                p->fillRect(QRect(o->rect.left() + px(tok::sp_1), y, o->rect.width() - 2 * px(tok::sp_1), 1),
                            c(tok::c_border));
                return;
            }
            QStyleOptionMenuItem item = *mi; // .mi:hover: nền --c-accent-soft bo --r, không viền
            if ((o->state & State_Selected) && (o->state & State_Enabled)) {
                p->save();
                p->setRenderHint(QPainter::Antialiasing);
                p->setPen(Qt::NoPen);
                p->setBrush(c(tok::c_accent_soft));
                p->drawRoundedRect(QRectF(o->rect), tok::px(tok::r, mode), tok::px(tok::r, mode));
                p->restore();
                item.state &= ~State_Selected;
            }
            QProxyStyle::drawControl(ce, &item, p, w);
            return;
        }
        if (const auto* tb = qstyleoption_cast<const QStyleOptionToolButton*>(o);
            ce == CE_ToolButtonLabel && tb && (o->state & State_Enabled)) { // .ib: chữ --c-text-2; rê --c-text; bật --c-on
            QStyleOptionToolButton b = *tb;
            b.palette.setColor(QPalette::ButtonText, o->state & State_On          ? c(tok::c_on)
                                                     : o->state & State_MouseOver ? c(tok::c_text)
                                                                                  : c(tok::c_text_2));
            QProxyStyle::drawControl(ce, &b, p, w);
            return;
        }
        if (ce == CE_TabBarTabLabel && tab) { // chữ --c-text-2; rê, đang mở: --c-text; đang mở đậm --fw-m
            QStyleOptionTab t = *tab;
            if (o->state & State_Enabled)
                t.palette.setColor(QPalette::WindowText,
                                   o->state & (State_Selected | State_MouseOver) ? c(tok::c_text) : c(tok::c_text_2));
            t.state &= ~State_HasFocus; // QCommonStyle vẽ khung focus của Fusion trong tab: thay bằng viền sau đây
            p->save();
            if (o->state & State_Selected) {
                QFont f = p->font();
                f.setWeight(QFont::Weight(qRound(tok::num(tok::fw_m))));
                p->setFont(f);
            }
            QProxyStyle::drawControl(ce, &t, p, w);
            p->restore();
            if (o->state & State_HasFocus) // :focus-visible: viền 2 px quanh cả tab (PE_FrameFocusRect)
                drawPrimitive(PE_FrameFocusRect, o, p, w);
            return;
        }
        QProxyStyle::drawControl(ce, o, p, w);
    }

    QRect subElementRect(SubElement e, const QStyleOption* o, const QWidget* w) const override
    {
        if (e == SE_PushButtonFocusRect) // viền tiêu điểm trùng viền nút (outline-offset: -1px)
            return o->rect;
        return QProxyStyle::subElementRect(e, o, w);
    }

    void drawPrimitive(PrimitiveElement pe, const QStyleOption* o, QPainter* p, const QWidget* w) const override
    {
        const bool enabled = o->state & State_Enabled;
        const bool hover = enabled && (o->state & State_MouseOver);
        const qreal r = tok::px(tok::r, mode);
        const QRectF box = QRectF(o->rect).adjusted(0.5, 0.5, -0.5, -0.5); // nét 1 px nằm trọn trong ô
        const QColor ctl = hover ? c(tok::c_text_2) : enabled ? c(tok::c_border_ctl) : c(tok::c_border);
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        switch (pe) {
        case PE_PanelButtonCommand: // .btn; Fusion báo focus qua viền của panel (CE_PushButton không vẽ khung focus)
            p->setPen(ctl);
            p->setBrush(o->state & (State_Sunken | State_On) ? c(tok::c_active) : c(tok::c_raised));
            p->drawRoundedRect(box, r, r);
            if ((o->state & State_HasFocus) && (o->state & State_KeyboardFocusChange)) { // :focus-visible
                p->setPen(QPen(c(tok::c_focus), 2));
                p->setBrush(Qt::NoBrush);
                p->drawRoundedRect(QRectF(o->rect).adjusted(1, 1, -1, -1), r, r);
            }
            break;
        case PE_PanelButtonTool: // .ib: phẳng; rê, nhấn, bật có nền
            if ((o->state & (State_On | State_Sunken)) || hover) {
                p->setPen(Qt::NoPen);
                p->setBrush(o->state & State_On       ? c(tok::c_on_bg)
                            : o->state & State_Sunken ? c(tok::c_press)
                                                      : c(tok::c_hover));
                p->drawRoundedRect(QRectF(o->rect), r, r);
            }
            break;
        case PE_PanelLineEdit: // .inp; ô không viền (trong spinbox, combobox sửa được) để Fusion vẽ
        case PE_FrameLineEdit: {
            const auto* f = qstyleoption_cast<const QStyleOptionFrame*>(o);
            if (pe == PE_PanelLineEdit && f && f->lineWidth <= 0) {
                p->restore();
                QProxyStyle::drawPrimitive(pe, o, p, w);
                return;
            }
            p->setPen(o->state & State_HasFocus ? c(tok::c_focus) : enabled ? c(tok::c_border_ctl) : c(tok::c_border));
            p->setBrush(pe == PE_PanelLineEdit ? o->palette.base() : Qt::NoBrush);
            p->drawRoundedRect(box, r, r);
            break;
        }
        case PE_FrameFocusRect: // :focus-visible: chỉ khi tiêu điểm đến từ bàn phím
            if (o->state & State_KeyboardFocusChange) {
                p->setPen(QPen(c(tok::c_focus), 2));
                p->setBrush(Qt::NoBrush);
                p->drawRoundedRect(QRectF(o->rect).adjusted(1, 1, -1, -1), r, r);
            }
            break;
        case PE_IndicatorCheckBox: // .chk input (accent-color)
        case PE_IndicatorRadioButton: {
            const bool radio = pe == PE_IndicatorRadioButton;
            const bool on = o->state & (State_On | State_NoChange);
            const QColor accent = enabled ? c(tok::c_accent) : c(tok::c_text_dis);
            p->setPen(on ? accent : ctl);
            p->setBrush(on && !radio ? accent : c(tok::c_raised));
            radio ? p->drawEllipse(box) : p->drawRoundedRect(box, r, r);
            if (on) {
                const QRectF in = box.marginsRemoved(QMarginsF(box.width(), box.height(), box.width(), box.height()) / 4);
                if (radio) {
                    p->setPen(Qt::NoPen);
                    p->setBrush(accent);
                    p->drawEllipse(in);
                } else {
                    p->setPen(QPen(c(tok::c_accent_t), tok::num(tok::sw) * 1.5));
                    if (o->state & State_NoChange)
                        p->drawLine(QPointF(in.left(), in.center().y()), QPointF(in.right(), in.center().y()));
                    else
                        p->drawPolyline(QList<QPointF>{{in.left(), in.center().y()},
                                                       {in.left() + in.width() * 0.4, in.bottom()},
                                                       {in.right(), in.top()}});
                }
            }
            break;
        }
        case PE_PanelMenu: // .menu: nền --c-raised, viền --c-border-ctl
            p->fillRect(o->rect, c(tok::c_raised));
            break;
        case PE_FrameMenu:
            p->setPen(c(tok::c_border_ctl));
            p->setBrush(Qt::NoBrush);
            p->drawRect(box);
            break;
        case PE_PanelTipLabel: // .tip
            p->setPen(c(tok::c_dyn_b));
            p->setBrush(o->palette.toolTipBase());
            p->drawRoundedRect(box, r, r);
            break;
        case PE_FrameTabWidget: // .tabs: một đường chia dưới dải tab
            p->setPen(c(tok::c_border));
            p->drawLine(o->rect.topLeft(), o->rect.topRight());
            break;
        default:
            p->restore();
            QProxyStyle::drawPrimitive(pe, o, p, w);
            return;
        }
        p->restore();
    }

    void polish(QWidget* w) override
    {
        QProxyStyle::polish(w);
        if (!qss.isEmpty() && w->styleSheet().isEmpty() && wantsQss(w))
            w->setStyleSheet(qss); // gọi lại polish() qua QStyleSheetStyle; lúc đó styleSheet() đã khác rỗng
        if (auto* v = qobject_cast<QAbstractItemView*>(w); v && !w->inherits("QComboBoxListView"))
            v->viewport()->setBackgroundRole(QPalette::Window); // bảng, cây nằm trên --c-base; ô nhập trên --c-raised
        if (isPopup(w) && w->windowFlags().testFlag(Qt::NoDropShadowWindowHint) != mode.lite)
            w->setWindowFlag(Qt::NoDropShadowWindowHint, mode.lite); // Giao diện nhẹ: popup không bóng hệ thống
        if (qobject_cast<QPlainTextEdit*>(w) || qobject_cast<QTextEdit*>(w))
            w->installEventFilter(this); // viền focus: eventFilter
    }
    void unpolish(QWidget* w) override
    {
        w->removeEventFilter(this);
        QProxyStyle::unpolish(w);
    }
    void polish(QApplication* app) override
    {
        QProxyStyle::polish(app);
#ifndef Q_OS_MACOS
        app->installEventFilter(&alt_);
#endif
    }
    void unpolish(QApplication* app) override
    {
        app->removeEventFilter(&alt_);
        QProxyStyle::unpolish(app);
    }
    using QProxyStyle::polish;
    using QProxyStyle::unpolish;

    // Ô nhiều dòng (QPlainTextEdit, QTextEdit): vùng nhìn phủ kín ô nên viền focus (.inp:focus, 2 px --c-focus: 1 px
    // ngoài, 1 px trong mép) vẽ trên một QFocusFrame đặt trên ô, như QMacStyle; một cái cho cả ứng dụng, theo ô có focus.
    // ponytail: góc vuông (mặt nạ chữ nhật), ô bo --r; bo theo khi QA-011 so ảnh đòi.
    bool eventFilter(QObject* o, QEvent* e) override
    {
        if (e->type() == QEvent::FocusIn) {
            auto* w = static_cast<QWidget*>(o);
            if (!ring_)
                ring_ = new QFocusFrame(w); // setWidget chuyển nó sang cha của ô
            ring_->setWidget(w);
        } else if (e->type() == QEvent::FocusOut && ring_ && ring_->widget() == o) {
            ring_->setWidget(nullptr);
        }
        return QProxyStyle::eventFilter(o, e);
    }

private:
    QPointer<QFocusFrame> ring_;
    AltWatch alt_;
};

ThemeManager::ThemeManager(settings::Store& store, QObject* parent)
    : QObject(parent), store_(store), semantic_(store)
{
    if (qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
        const QStyleHints* h = QGuiApplication::styleHints();
        systemLight_ = h->colorScheme() == Qt::ColorScheme::Light;
        // Dùng giá trị của tín hiệu: hệ điều hành đổi là QStyleHints đã cập nhật trước khi phát.
        connect(h, &QStyleHints::colorSchemeChanged, this, [this](Qt::ColorScheme s) {
            systemLight_ = s == Qt::ColorScheme::Light;
            apply(wantedMode());
        });
    }
    connect(&store_, &settings::Store::changed, this, [this](const QString& key) {
        if (key == QLatin1StringView(kSemanticOverrides.key)) {
            semantic_.reload();
            emit changed();
        } else if (key == QLatin1StringView(kTheme.key) || key == QLatin1StringView(kDensity.key) ||
                   key == QLatin1StringView(kLite.key)) {
            apply(wantedMode());
        }
    });
    g_instance.store(this);
    apply(wantedMode());
}

ThemeManager::~ThemeManager()
{
    ThemeManager* self = this;
    g_instance.compare_exchange_strong(self, nullptr);
}

ThemeManager& ThemeManager::instance()
{
    ThemeManager* t = g_instance.load();
    TRION_CHECK(t && "chưa tạo ThemeManager");
    return *t;
}

Theme ThemeManager::theme() const
{
    const QString t = store_.get(kTheme);
    return t == u"light"_s ? Theme::Light : t == u"system"_s ? Theme::System : Theme::Dark; // giá trị lạ: Tối
}

void ThemeManager::setTheme(Theme t)
{
    store_.set(kTheme, t == Theme::Light ? u"light"_s : t == Theme::System ? u"system"_s : u"dark"_s);
}

Density ThemeManager::density() const
{
    return store_.get(kDensity) == u"comfortable"_s ? Density::Comfortable : Density::Compact;
}

void ThemeManager::setDensity(Density d)
{
    store_.set(kDensity, d == Density::Comfortable ? u"comfortable"_s : u"compact"_s);
}

void ThemeManager::setReducedMotion(bool on)
{
    reducedMotion_ = on;
    apply(wantedMode());
}

tok::Mode ThemeManager::wantedMode() const
{
    const Theme t = theme();
    return {.light = t == Theme::Light || (t == Theme::System && systemLight_),
            .comfortable = density() == Density::Comfortable,
            .lite = lite(),
            .reducedMotion = reducedMotion_};
}

QFont ThemeManager::font(tok::PxId size) const
{
    QFont f = QApplication::font();
    f.setPointSizeF(ptFromPx(px(size)));
    return f;
}

QPalette ThemeManager::makePalette(tok::Mode m)
{
    const auto c = [m](tok::ColorId id) { return toQColor(tok::color(id, m)); };
    QPalette p;
    p.setColor(QPalette::Window, c(tok::c_base));
    p.setColor(QPalette::WindowText, c(tok::c_text));
    p.setColor(QPalette::Base, c(tok::c_raised));
    p.setColor(QPalette::AlternateBase, c(tok::c_sunken));
    p.setColor(QPalette::Text, c(tok::c_text));
    p.setColor(QPalette::PlaceholderText, c(tok::c_text_3));
    p.setColor(QPalette::Button, c(tok::c_raised));
    p.setColor(QPalette::ButtonText, c(tok::c_text));
    p.setColor(QPalette::BrightText, c(tok::c_accent_t));
    p.setColor(QPalette::Light, c(tok::c_raised));
    p.setColor(QPalette::Midlight, c(tok::c_active));
    p.setColor(QPalette::Mid, c(tok::c_border_ctl));
    p.setColor(QPalette::Dark, c(tok::c_border));
    p.setColor(QPalette::Shadow, c(tok::c_chrome));
    p.setColor(QPalette::Highlight, c(tok::c_accent_soft)); // hàng chọn, mục menu đang trỏ
    p.setColor(QPalette::HighlightedText, c(tok::c_text));
    p.setColor(QPalette::Link, c(tok::c_accent));
    p.setColor(QPalette::LinkVisited, c(tok::c_accent));
    p.setColor(QPalette::ToolTipBase, c(tok::c_dyn_bg));
    p.setColor(QPalette::ToolTipText, c(tok::c_dyn_t));
    p.setColor(QPalette::Accent, c(tok::c_accent));
    for (const QPalette::ColorRole r :
         {QPalette::WindowText, QPalette::Text, QPalette::ButtonText, QPalette::PlaceholderText, QPalette::HighlightedText})
        p.setColor(QPalette::Disabled, r, c(tok::c_text_dis));
    return p;
}

QString ThemeManager::makeStyleSheet(tok::Mode m)
{
    static const QString tpl = [] { // bỏ chú thích /* … */
        QFile f(u":/trion/theme/app.qss.in"_s);
        TRION_CHECK(f.open(QIODevice::ReadOnly) && "thiếu tài nguyên app.qss.in");
        static const QRegularExpression comment(uR"(/\*.*?\*/\s*)"_s, QRegularExpression::DotMatchesEverythingOption);
        return QString::fromUtf8(f.readAll()).remove(comment);
    }();
    static const QRegularExpression var(uR"(var\((--[a-z0-9-]+)\))"_s);
    QString out;
    qsizetype last = 0;
    for (auto it = var.globalMatch(tpl); it.hasNext();) {
        const QRegularExpressionMatch mt = it.next();
        out += QStringView(tpl).sliced(last, mt.capturedStart() - last);
        last = mt.capturedEnd();
        const QByteArray name = mt.captured(1).toLatin1();
        const tok::Token* t = tok::find({name.constData(), std::size_t(name.size())});
        if (!t) {
            qWarning("theme: app.qss.in dùng token không có trong tokens.css: %s", name.constData());
            continue;
        }
        switch (t->kind) {
        case tok::Kind::Color: {
            const tok::Rgba c = tok::color(tok::ColorId{t->i}, m);
            out += c.a == 255 ? toQColor(c).name() : u"rgba(%1, %2, %3, %4)"_s.arg(c.r).arg(c.g).arg(c.b).arg(c.a);
            break;
        }
        case tok::Kind::Px: { // QSS đọc "px" thành số nguyên: cỡ lẻ ghi bằng pt
            const double v = tok::px(tok::PxId{t->i}, m);
            out += v == std::floor(v) ? u"%1px"_s.arg(v) : u"%1pt"_s.arg(ptFromPx(v));
            break;
        }
        case tok::Kind::Ms:
            out += QString::number(tok::ms(tok::MsId{t->i}, m));
            break;
        case tok::Kind::Num:
            out += QString::number(tok::num(tok::NumId{t->i}));
            break;
        case tok::Kind::Str: {
            const std::string_view s = tok::str(tok::StrId{t->i});
            out += QString::fromUtf8(s.data(), qsizetype(s.size()));
            break;
        }
        case tok::Kind::Shadow: // QSS không có bóng
            qWarning("theme: app.qss.in không dùng được token bóng %s", name.constData());
            break;
        }
    }
    out += QStringView(tpl).sliced(last);
    return out;
}

void ThemeManager::apply(tok::Mode m)
{
    if (applied_ && sameMode(m, mode_))
        return;
    const bool first = !applied_;
    mode_ = m;
    applied_ = true;
    qss_ = makeStyleSheet(m);

    if (qobject_cast<QApplication*>(QCoreApplication::instance())) {
        // Trạng thái đang áp cho ứng dụng nằm ở TrionStyle (có thể do ThemeManager trước để lại).
        auto* style = dynamic_cast<TrionStyle*>(QApplication::style());
        const bool fresh = !style;
        if (fresh) {
            style = new TrionStyle; // QApplication sở hữu
            style->mode = m;
            style->qss = qss_;
            QApplication::setStyle(style);
            const std::string_view f = tok::str(tok::f_ui);
            QStringList families = QString::fromUtf8(f.data(), qsizetype(f.size())).split(u',');
            for (QString& name : families)
                name = name.trimmed().remove(u'"');
            QFont font(families);
            font.setPointSizeF(ptFromPx(tok::px(tok::fs, m))); // body: --fs/--f-ui
            QApplication::setFont(font);
            font.setPointSizeF(ptFromPx(tok::px(tok::fs_s, m))); // .tip: --fs-s
            QToolTip::setFont(font);
        }
        const tok::Mode was = std::exchange(style->mode, m);
        const QString oldQss = std::exchange(style->qss, qss_);
        const QList<QWidget*> all = QApplication::allWidgets();
        if (first || was.light != m.light) {
            const QPalette pal = makePalette(m);
            QApplication::setPalette(pal); // PaletteChange tới mọi widget → update()
            QToolTip::setPalette(pal);
        }
        if (qss_ != oldQss) { // chỉ các widget lớp ít thể hiện mang QSS (TrionStyle::polish)
            for (QWidget* w : all)
                if (w->styleSheet() == oldQss)
                    w->setStyleSheet(qss_);
        }
        // ponytail: StyleChange tới mọi widget, kể cả ẩn (~10–30 ms với 3.500 widget, phần lớn ở QComboBox); chậm thì
        // hoãn widget ẩn tới lúc hiện.
        if (was.comfortable != m.comfortable) { // sau QSS: sizeHint đã cache tính lại theo QSS mới
            for (QWidget* w : all) { // kích thước theo mật độ: StyleChange → updateGeometry(), xóa sizeHint đã cache
                QEvent e(QEvent::StyleChange);
                QCoreApplication::sendEvent(w, &e);
            }
        }
        QApplication::setEffectEnabled(Qt::UI_General, tok::ms(tok::t, m) > 0); // menu, combobox, tooltip trượt/mờ
        // Giảm chuyển động, Giao diện nhẹ vừa bật (--t về 0): animation đang chạy tới cuối ngay (vô hạn thì dừng), không
        // chờ hết thời gian cũ; animation mới đọc ms() nên chạy 0 ms. Chỉ thấy animation có cha trong cây QObject của
        // cửa sổ cấp cao (QPropertyAnimation(…, cha), thành viên tạo với cha); con của nhóm chạy theo nhóm.
        if (tok::ms(tok::t, was) > 0 && tok::ms(tok::t, m) == 0)
            for (QWidget* top : QApplication::topLevelWidgets())
                for (QAbstractAnimation* a : top->findChildren<QAbstractAnimation*>())
                    if (!a->group() && a->state() != QAbstractAnimation::Stopped) {
                        if (a->totalDuration() < 0)
                            a->stop();
                        else
                            a->setCurrentTime(a->direction() == QAbstractAnimation::Forward ? a->totalDuration() : 0);
                    }
        if (was.lite != m.lite) { // popup đang mở giữ nguyên tới lần đổi sau: đổi cờ cửa sổ sẽ ẩn nó
            for (QWidget* w : all)
                if (isPopup(w) && !w->isVisible() && w->windowFlags().testFlag(Qt::NoDropShadowWindowHint) != m.lite)
                    w->setWindowFlag(Qt::NoDropShadowWindowHint, m.lite);
        }
    }
    if (!first)
        emit changed();
}

} // namespace trion::theme
