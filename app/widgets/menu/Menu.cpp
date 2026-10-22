#include "app/widgets/menu/Menu.h"

#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/tooltip/RichTooltip.h"

#include <QAccessible>
#include <QActionEvent>
#include <QApplication>
#include <QCommonStyle>
#include <QKeySequence>
#include <QMouseEvent>
#include <QPainter>
#include <QStyleOptionMenuItem>

#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;
using trion::theme::IconProvider;
using trion::theme::IconState;
using trion::theme::TextRole;
using trion::theme::ThemeManager;
using trion::theme::Weight;

namespace trion::ui {

namespace {

constexpr int kMinWidth = 200; // .menu min-width của prototype (chưa có token)

int px(tok::PxId id)
{
    return qRound(ThemeManager::instance().px(id));
}
QColor col(tok::ColorId id)
{
    return ThemeManager::instance().color(id);
}

// .menu .mi .k: --f-mono --fs-xs; cách tên (hoặc huy hiệu) gap 8 px + padding-left 18 px (--sp-4 + --sp-6 + --sp-1).
QFont keyFont()
{
    return theme::font(TextRole::MonoCaption);
}
int keyGap()
{
    return px(tok::sp_4) + px(tok::sp_6) + px(tok::sp_1);
}

// .menu .mh: --fs-xs đậm, chữ hoa, giãn .04em (vẽ chuỗi đã viết hoa để đo và vẽ cùng một chuỗi).
QFont headerFont()
{
    QFont f = theme::font(TextRole::Caption, Weight::Bold);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 0.04 * QFontInfo(f).pixelSize());
    return f;
}

struct Parts {
    QString label, key;
    Edition need = Edition::None;
};

// Menu::initStyleOption để "tên\tphím" trong text và action trong styleObject.
Parts parts(const QStyleOptionMenuItem& mi)
{
    const qsizetype tab = mi.text.indexOf(u'\t');
    return {mi.text.left(tab), tab < 0 ? QString() : mi.text.mid(tab + 1),
            RichTooltip::info(qobject_cast<const QAction*>(mi.styleObject)).need};
}

// Style riêng của Menu: đo, vẽ mục (.mi) và tiêu đề nhóm (.mh); mọi thứ khác (nền, viền, lề, vạch chia, cuộn, gợi ý)
// chuyển cho style của ứng dụng lúc gọi. Không dùng QProxyStyle: nó giành quyền sở hữu style nền, mà style nền ở đây là
// style của ứng dụng.
class MenuStyle final : public QCommonStyle {
public:
    using QCommonStyle::polish;
    using QCommonStyle::unpolish;
    void polish(QWidget* w) override { app()->polish(w); }
    void unpolish(QWidget* w) override { app()->unpolish(w); }
    int pixelMetric(PixelMetric m, const QStyleOption* o, const QWidget* w) const override
    {
        return app()->pixelMetric(m, o, w);
    }
    int styleHint(StyleHint h, const QStyleOption* o, const QWidget* w, QStyleHintReturn* r) const override
    {
        return app()->styleHint(h, o, w, r);
    }
    void drawPrimitive(PrimitiveElement e, const QStyleOption* o, QPainter* p, const QWidget* w) const override
    {
        app()->drawPrimitive(e, o, p, w);
    }
    void drawComplexControl(ComplexControl c, const QStyleOptionComplex* o, QPainter* p,
                            const QWidget* w) const override
    {
        app()->drawComplexControl(c, o, p, w);
    }
    QRect subElementRect(SubElement e, const QStyleOption* o, const QWidget* w) const override
    {
        return app()->subElementRect(e, o, w);
    }
    QRect subControlRect(ComplexControl c, const QStyleOptionComplex* o, SubControl s, const QWidget* w) const override
    {
        return app()->subControlRect(c, o, s, w);
    }
    SubControl hitTestComplexControl(ComplexControl c, const QStyleOptionComplex* o, const QPoint& pt,
                                     const QWidget* w) const override
    {
        return app()->hitTestComplexControl(c, o, pt, w);
    }
    QPixmap standardPixmap(StandardPixmap s, const QStyleOption* o, const QWidget* w) const override
    {
        return app()->standardPixmap(s, o, w);
    }
    QIcon standardIcon(StandardPixmap s, const QStyleOption* o, const QWidget* w) const override
    {
        return app()->standardIcon(s, o, w);
    }
    QPixmap generatedIconPixmap(QIcon::Mode m, const QPixmap& pm, const QStyleOption* o) const override
    {
        return app()->generatedIconPixmap(m, pm, o);
    }
    int layoutSpacing(QSizePolicy::ControlType a, QSizePolicy::ControlType b, Qt::Orientation orientation,
                      const QStyleOption* o, const QWidget* w) const override
    {
        return app()->layoutSpacing(a, b, orientation, o, w);
    }

    QSize sizeFromContents(ContentsType t, const QStyleOption* o, const QSize& s, const QWidget* w) const override
    {
        const auto* mi = qstyleoption_cast<const QStyleOptionMenuItem*>(o);
        return t == CT_MenuItem && ours(mi) ? itemSize(*mi) : app()->sizeFromContents(t, o, s, w);
    }

    void drawControl(ControlElement e, const QStyleOption* o, QPainter* p, const QWidget* w) const override
    {
        const auto* mi = qstyleoption_cast<const QStyleOptionMenuItem*>(o);
        if (e == CE_MenuItem && ours(mi))
            drawItem(*mi, p, w);
        else
            app()->drawControl(e, o, p, w);
    }

private:
    static QStyle* app() { return QApplication::style(); }

    // Mục thường, mục mở menu con, tiêu đề nhóm (vạch chia có chữ của addSection); vạch chia trơn, vùng cuộn để
    // style của ứng dụng.
    static bool ours(const QStyleOptionMenuItem* mi)
    {
        if (!mi)
            return false;
        switch (mi->menuItemType) {
        case QStyleOptionMenuItem::Normal:
        case QStyleOptionMenuItem::DefaultItem:
        case QStyleOptionMenuItem::SubMenu:
            return true;
        case QStyleOptionMenuItem::Separator:
            return !mi->text.isEmpty();
        default:
            return false;
        }
    }

    // .mi: cao --h-ctl; lề 0 8 0 6 (--sp-4, --sp-3); cột biểu tượng --ic; các phần cách gap 8 px (--sp-4);
    // mũi tên menu con 6 px (--sp-3). .mh: lề 6 8 3 (3 px ngoài thang token: --sp-1).
    static QSize itemSize(const QStyleOptionMenuItem& mi)
    {
        if (mi.menuItemType == QStyleOptionMenuItem::Separator) {
            const QFontMetrics fm(headerFont());
            return {fm.horizontalAdvance(mi.text.toUpper()) + 2 * px(tok::sp_4),
                    px(tok::sp_3) + fm.height() + px(tok::sp_1)};
        }
        const Parts p = parts(mi);
        int w = px(tok::sp_3) + px(tok::ic) + px(tok::sp_4) +
                mi.fontMetrics.size(Qt::TextShowMnemonic, p.label).width() + px(tok::sp_4);
        if (p.need != Edition::None)
            w += px(tok::sp_4) + RichTooltip::badgeSize(p.need).width();
        if (!p.key.isEmpty())
            w += keyGap() + QFontMetrics(keyFont()).horizontalAdvance(p.key);
        if (mi.menuItemType == QStyleOptionMenuItem::SubMenu)
            w += px(tok::sp_4) + px(tok::sp_3);
        return {w, std::max(px(tok::h_ctl), mi.fontMetrics.height())};
    }

    static void drawItem(const QStyleOptionMenuItem& mi, QPainter* p, const QWidget* w)
    {
        const QRect r = mi.rect;
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        if (mi.menuItemType == QStyleOptionMenuItem::Separator) { // .mh: --c-text-3
            p->setFont(headerFont());
            p->setPen(col(tok::c_text_3));
            p->drawText(r.adjusted(px(tok::sp_4), px(tok::sp_3), -px(tok::sp_4), 0),
                        Qt::AlignLeft | Qt::AlignTop | Qt::TextSingleLine, mi.text.toUpper());
            p->restore();
            return;
        }
        const bool enabled = mi.state & State_Enabled;
        if (enabled && (mi.state & State_Selected)) { // .mi:hover: nền --c-accent-soft bo --r
            const qreal radius = ThemeManager::instance().px(tok::r);
            p->setPen(Qt::NoPen);
            p->setBrush(col(tok::c_accent_soft));
            p->drawRoundedRect(QRectF(r), radius, radius);
        }
        const Parts pt = parts(mi);
        // Cột --ic: dấu chọn (.chkm: biểu tượng check 12 px --c-accent) hoặc biểu tượng của mục (.ic --c-text-2,
        // như prototype không đổi khi mục vô hiệu).
        const int ic = px(tok::ic), y0 = r.top() + (r.height() - ic) / 2;
        const QRect icon(r.left() + px(tok::sp_3), y0, ic, ic);
        if (mi.checkType != QStyleOptionMenuItem::NotCheckable) {
            if (mi.checked) {
                const int s = px(tok::sp_5); // .ic.s12
                QPixmap pm =
                    IconProvider::instance().pixmap(u"check"_s, s, p->device()->devicePixelRatio(), IconState::Normal);
                QPainter tint(&pm);
                tint.setCompositionMode(QPainter::CompositionMode_SourceIn);
                tint.fillRect(pm.rect(), col(tok::c_accent));
                tint.end();
                p->drawPixmap(QRect(icon.center() - QPoint(s / 2, s / 2), QSize(s, s)), pm);
            }
        } else if (!mi.icon.isNull()) {
            mi.icon.paint(p, icon, Qt::AlignCenter, QIcon::Normal);
        }
        const int x = icon.right() + 1 + px(tok::sp_4);
        int right = r.right() + 1 - px(tok::sp_4);
        if (mi.menuItemType == QStyleOptionMenuItem::SubMenu) { // .mi.sub::after: góc 6 px nét 1,5 px --c-text-2
            const qreal side = ThemeManager::instance().px(tok::sp_3), d = side / std::sqrt(2.0);
            const QPointF c(right - side / 2, QRectF(r).center().y());
            QPen pen(col(tok::c_text_2), 1.5);
            pen.setJoinStyle(Qt::MiterJoin);
            p->setPen(pen);
            p->setBrush(Qt::NoBrush);
            p->drawPolyline(QList<QPointF>{c + QPointF(-d / 2, -d), c + QPointF(d / 2, 0), c + QPointF(-d / 2, d)});
            right -= px(tok::sp_3) + px(tok::sp_4);
        }
        // .k: --c-text-3, cả khi mục vô hiệu (như prototype). Mục đang trỏ dùng --c-text-2: --c-text-3 trên
        // --c-accent-soft của chủ đề Tối chỉ 3,5:1 (DoD-U 3 cần 4,5:1), --c-text-2 đạt 4,9:1.
        if (!pt.key.isEmpty()) {
            const QFont f = keyFont();
            p->setFont(f);
            p->setPen(col(enabled && (mi.state & State_Selected) ? tok::c_text_2 : tok::c_text_3));
            p->drawText(QRect(x, r.top(), right - x, r.height()),
                        Qt::AlignRight | Qt::AlignVCenter | Qt::TextSingleLine, pt.key);
            right -= QFontMetrics(f).horizontalAdvance(pt.key) + keyGap();
        }
        if (pt.need != Edition::None) { // .ed.xs sát phải, trước cột phím
            const QSize b = RichTooltip::badgeSize(pt.need);
            const QRect badge(QPoint(right - b.width(), r.top() + (r.height() - b.height()) / 2), b);
            RichTooltip::paintBadge(*p, badge, pt.need);
            right = badge.left() - px(tok::sp_4);
        }
        int flags = Qt::AlignLeft | Qt::AlignVCenter | Qt::TextShowMnemonic | Qt::TextSingleLine;
        if (!app()->styleHint(SH_UnderlineShortcut, &mi, w))
            flags |= Qt::TextHideMnemonic;
        p->setFont(mi.font);
        p->setPen(col(enabled ? tok::c_text : tok::c_text_dis));
        p->drawText(QRect(x, r.top(), right - x, r.height()), flags, pt.label);
        p->restore();
    }
};

Menu* owner(const QAction* a)
{
    for (QObject* o : a->associatedObjects())
        if (auto* m = qobject_cast<Menu*>(o))
            return m;
    return nullptr;
}

// Trợ năng của mục trong Menu, thay QAccessibleMenuItem của Qt (header riêng tư, không đọc được mô tả): tên (bỏ &),
// phím theo nền tảng (Accelerator; không có thì mnemonic như Qt), trạng thái khóa và mô tả lệnh (Description).
class ItemAccessible final : public QAccessibleInterface, public QAccessibleActionInterface {
public:
    explicit ItemAccessible(QAction* a) : action_(a) {}

    bool isValid() const override { return action_ && owner(action_); }
    QObject* object() const override { return action_; }
    QWindow* window() const override
    {
        const Menu* m = owner(action_);
        return m ? m->windowHandle() : nullptr;
    }
    QAccessibleInterface* parent() const override { return QAccessible::queryAccessibleInterface(owner(action_)); }
    QAccessibleInterface* child(int index) const override
    {
        return index == 0 && action_->menu() ? QAccessible::queryAccessibleInterface(action_->menu()) : nullptr;
    }
    int childCount() const override { return action_->menu() ? 1 : 0; }
    int indexOfChild(const QAccessibleInterface* c) const override
    {
        return c && action_->menu() && c->object() == action_->menu() ? 0 : -1;
    }
    QAccessibleInterface* childAt(int, int) const override { return nullptr; }
    QAccessible::Role role() const override
    {
        return action_->isSeparator() ? QAccessible::Separator : QAccessible::MenuItem;
    }
    QRect rect() const override
    {
        const Menu* m = owner(action_);
        if (!m)
            return {};
        const QRect r = m->actionGeometry(action_);
        return {m->mapToGlobal(r.topLeft()), r.size()};
    }
    QAccessible::State state() const override
    {
        QAccessible::State s;
        const Menu* m = owner(action_);
        s.invisible = !m || !m->isVisible() || !action_->isVisible();
        s.focused = m && m->activeAction() == action_;
        s.focusable = action_->isEnabled() && !action_->isSeparator();
        s.disabled = !s.focusable;
        s.checkable = action_->isCheckable();
        s.checked = action_->isChecked();
        s.hasPopup = action_->menu() != nullptr;
        return s;
    }
    QString text(QAccessible::Text t) const override
    {
        switch (t) {
        case QAccessible::Name:
            return RichTooltip::name(action_);
        case QAccessible::Accelerator:
            if (QString k = Menu::keyText(action_); !k.isEmpty())
                return k;
            return QKeySequence::mnemonic(action_->text()).toString(QKeySequence::NativeText);
        case QAccessible::Description: {
            const TipInfo i = RichTooltip::info(action_);
            QStringList d;
            if (i.need != Edition::None) // như huy hiệu khóa (EditionGate::availableIn)
                d << Menu::tr("Có trong TrionCAD %1").arg(RichTooltip::editionName(i.need));
            if (!i.description.isEmpty())
                d << i.description;
            return d.join(u". "_s);
        }
        default:
            return {};
        }
    }
    void setText(QAccessible::Text, const QString&) override {}
    void* interface_cast(QAccessible::InterfaceType t) override
    {
        return t == QAccessible::ActionInterface ? static_cast<QAccessibleActionInterface*>(this) : nullptr;
    }

    QStringList actionNames() const override
    {
        if (!action_->isEnabled() || action_->isSeparator())
            return {};
        return {action_->menu() ? showMenuAction() : pressAction()};
    }
    void doAction(const QString& name) override
    {
        if (!actionNames().contains(name))
            return;
        if (name == pressAction())
            action_->trigger();
        else if (Menu* m = owner(action_))
            m->setActiveAction(action_);
    }
    QStringList keyBindingsForAction(const QString&) const override { return {}; }

private:
    QPointer<QAction> action_;
};

QAccessibleInterface* accessibleItem(const QString& key, QObject* object)
{
    // Cùng một QAction có thể nằm ở QMenuBar, thanh công cụ: chỉ nhận action thuộc một Menu.
    auto* a = key == "QAction"_L1 ? static_cast<QAction*>(object) : nullptr;
    return a && owner(a) ? new ItemAccessible(a) : nullptr;
}

} // namespace

Menu::Menu(QWidget* parent) : QMenu(parent)
{
    static QPointer<MenuStyle> style; // một style cho mọi Menu, sống cùng QApplication
    if (!style) {
        style = new MenuStyle;
        style->setParent(QCoreApplication::instance());
    }
    static const bool factory = (QAccessible::installFactory(&accessibleItem), true);
    Q_UNUSED(factory);
    setStyle(style);
    setMinimumWidth(kMinWidth);
    if (RichTooltip* rt = RichTooltip::instance())
        connect(rt, &RichTooltip::keysChanged, this, &Menu::relayout);
}

Menu::Menu(const QString& title, QWidget* parent) : Menu(parent)
{
    setTitle(title);
}

QString Menu::keyText(const QAction* action)
{
    const RichTooltip* rt = RichTooltip::instance();
    return rt && action ? rt->keyText(RichTooltip::info(action).keyId) : QString();
}

void Menu::initStyleOption(QStyleOptionMenuItem* option, const QAction* action) const
{
    QMenu::initStyleOption(option, action);
    option->styleObject = const_cast<QAction*>(action); // MenuStyle đọc huy hiệu của mục
    if (action->isSeparator())
        return;
    const QString text = action->text();
    option->text = text.left(text.indexOf(u'\t')); // bỏ phím Qt tự nối từ QAction::shortcut
    if (const QString key = keyText(action); !key.isEmpty())
        option->text += u'\t' + key;
}

void Menu::actionEvent(QActionEvent* e)
{
    if (e->type() == QEvent::ActionAdded)
        e->action()->setShortcutVisibleInContextMenu(false); // phím do Menu vẽ từ Keymap
    QMenu::actionEvent(e);
}

void Menu::mouseMoveEvent(QMouseEvent* e)
{
    QMenu::mouseMoveEvent(e);
    RichTooltip* rt = RichTooltip::instance();
    QAction* a = actionAt(e->position().toPoint());
    if (!rt || a == tipAction_)
        return;
    const bool had = tipAction_ && rt->target() == tipAction_;
    tipAction_ = a;
    if (a && !a->menu() && RichTooltip::hasInfo(a)) {
        const QRect r = actionGeometry(a);
        rt->hover(a, QRect(mapToGlobal(r.topLeft()), r.size()), e->globalPosition().toPoint());
    } else if (had) {
        rt->leave();
    }
}

void Menu::leaveEvent(QEvent* e)
{
    RichTooltip* rt = RichTooltip::instance();
    if (rt && tipAction_ && rt->target() == tipAction_)
        rt->leave();
    tipAction_ = nullptr;
    QMenu::leaveEvent(e);
}

void Menu::hideEvent(QHideEvent* e)
{
    RichTooltip* rt = RichTooltip::instance();
    if (rt && tipAction_ && rt->target() == tipAction_)
        rt->leave();
    tipAction_ = nullptr;
    QMenu::hideEvent(e);
}

void Menu::relayout()
{
    QEvent e(QEvent::StyleChange); // QMenu đo lại mọi mục, đổi cỡ nếu đang mở
    QCoreApplication::sendEvent(this, &e);
}

} // namespace trion::ui
