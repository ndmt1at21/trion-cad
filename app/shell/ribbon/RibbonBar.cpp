#include "app/shell/ribbon/RibbonBar.h"

#include "app/core/actions/ActionRegistry.h"
#include "app/core/edition/EditionGate.h"
#include "app/shell/ribbon/controls/RibbonControls.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/generated/Tokens.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/controls/Controls.h"
#include "app/widgets/controls/Paint.h"
#include "app/widgets/menu/Menu.h"
#include "app/widgets/tooltip/RichTooltip.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QBoxLayout>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QTabBar>
#include <QWidgetAction>

#include <algorithm>
#include <functional>
#include <numeric>
#include <utility>
#include <vector>

using namespace Qt::StringLiterals;

namespace trion::ribbon {

using theme::IconProvider;
using theme::IconState;
using theme::TextRole;
using ui::RichTooltip;
using State = RibbonPanel::State;

namespace {

theme::ThemeManager& tm()
{
    return theme::ThemeManager::instance();
}
QColor col(tok::ColorId id)
{
    return tm().color(id);
}
int px(tok::PxId id)
{
    return qRound(tm().px(id));
}

// Số của prototype không có token (như controls/): nét 1 px; mũi tên .ic.xs 9 px; khe 3 px của .rb-l button, .rb-coll.
constexpr int kBorder = 1;
constexpr int kChevron = 9;
int gap()
{
    return px(tok::sp_1) + kBorder;
}

// Cao thân theo mức thu gọn: .rb-body (3 hàng nút, 8 px, nhãn panel); .min-panels (nhãn panel, 8 px); .min-tabs ẩn.
int bodyHeight(RibbonBar::Collapse c)
{
    switch (c) {
    case RibbonBar::Collapse::Full:
        return 3 * px(tok::h_btn) + px(tok::sp_4) + px(tok::h_lbl);
    case RibbonBar::Collapse::PanelTitles:
        return px(tok::h_lbl) + px(tok::sp_4);
    case RibbonBar::Collapse::TabNames:
        break;
    }
    return 0;
}

// Biểu tượng mọi tab theo thứ tự tab (tab đầu raster trước): điều khiển, mục split và panel mở rộng (không có thì của
// lệnh), nút panel đã thu; thêm của khung (ghim, thu gọn), ô số (chọn điểm), khóa và dấu chọn của menu.
QStringList iconNames(const RibbonModel& model)
{
    QStringList out;
    const auto add = [&](const QString& icon, const QString& cmd) {
        out << (icon.isEmpty() ? RibbonControls::commandIcon(cmd) : icon);
    };
    const std::function<void(const std::vector<Item>&)> walk = [&](const std::vector<Item>& items) {
        for (const Item& it : items) {
            add(it.control.icon, it.control.cmd);
            for (const MenuItem& m : it.control.menu)
                add(m.icon, m.cmd);
            walk(it.items);
        }
    };
    for (const Tab& t : model.tabs())
        for (const Panel& p : t.panels) {
            out << (p.icon.isEmpty() ? u"more"_s : p.icon);
            walk(p.items);
            for (const MenuItem& m : p.more)
                add(m.icon, m.cmd);
        }
    return out << u"pin"_s << u"up"_s << u"down"_s << u"idpoint"_s << u"lock"_s << u"check"_s;
}

bool inside(const QWidget* root, const QWidget* w)
{
    return root && w && (w == root || root->isAncestorOf(w));
}

// Điều khiển đầu tiên nhận focus bằng Tab trong root.
QWidget* firstFocusable(QWidget* root)
{
    for (QWidget* w : root->findChildren<QWidget*>())
        if ((w->focusPolicy() & Qt::TabFocus) && w->isEnabled() && w->isVisibleTo(root))
            return w;
    return nullptr;
}

// Bề rộng thân panel từ cây widget của buildPanel, cộng như QBoxLayout (hàng: tổng và khe, cột: lớn nhất). Không phụ
// thuộc layout đã chạy chưa nên đúng ngay sau khi chữ, cỡ đổi và đo được dạng chưa áp (gọn). Khối hàng/cột của
// buildPanel không có objectName; điều khiển có (RibbonControls::create đặt id).
int measureBox(const QBoxLayout* box, bool compact)
{
    const bool row = box->direction() == QBoxLayout::LeftToRight || box->direction() == QBoxLayout::RightToLeft;
    int sum = 0, widest = 0, n = 0;
    for (int i = 0; i < box->count(); ++i) {
        const QWidget* w = box->itemAt(i)->widget();
        if (!w || w->isHidden())
            continue;
        const auto* inner = w->objectName().isEmpty() ? qobject_cast<const QBoxLayout*>(w->layout()) : nullptr;
        int cw = 0;
        if (const auto* b = qobject_cast<const RibbonButton*>(w))
            cw = b->sizeFor(compact).width();
        else if (inner)
            cw = measureBox(inner, compact);
        else // như QWidgetItem::sizeHint
            cw = w->sizeHint().expandedTo(w->minimumSizeHint()).boundedTo(w->maximumSize()).expandedTo(w->minimumSize())
                     .width();
        sum += cw;
        widest = std::max(widest, cw);
        ++n;
    }
    const QMargins m = box->contentsMargins();
    return (row ? sum + box->spacing() * std::max(0, n - 1) : widest) + m.left() + m.right();
}

// Dải tab (.rb-tabs .rb-tab): cao --h-tab, lề ngang --sp-5; không có nét dưới như .tabs.
class RibbonTabs final : public ui::TabBar {
public:
    using TabBar::TabBar;

protected:
    QSize tabSizeHint(int index) const override
    {
        return {fontMetrics().horizontalAdvance(tabText(index)) + 2 * px(tok::sp_5), px(tok::h_tab)};
    }
    QSize minimumTabSizeHint(int index) const override { return tabSizeHint(index); }
    void paintEvent(QPaintEvent* e) override { QTabBar::paintEvent(e); } // bỏ nét dưới của ui::TabBar
};

// Nét chia giữa nhóm mục của panel mở rộng (.ms: --c-border, lề --sp-1).
class Rule final : public QWidget {
public:
    using QWidget::QWidget;
    QSize sizeHint() const override { return {0, 2 * px(tok::sp_1) + kBorder}; }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter(this).fillRect(QRect(0, height() / 2, width(), kBorder), col(tok::c_border));
    }
};

} // namespace

// ------------------------------------------------------------------------------------------------- PanelButton

// Nút của khung panel: "tên ▾" cao --h-lbl (.rb-l button: chữ --fs-xs --c-text-3, rê --c-text) hoặc nút lớn của panel
// đã thu (.rb-coll: biểu tượng --ic-lg, "tên ▾" --fs-s, cao hết panel).
class PanelButton final : public QAbstractButton {
public:
    explicit PanelButton(QWidget* parent) : QAbstractButton(parent)
    {
        setAttribute(Qt::WA_Hover);
        setFocusPolicy(Qt::TabFocus); // bấm chuột không lấy focus của vùng vẽ
        setLarge(false);
    }

    bool isLarge() const { return large_; }
    void setLarge(bool large)
    {
        large_ = large;
        setSizePolicy(QSizePolicy::Fixed, large ? QSizePolicy::Expanding : QSizePolicy::Fixed);
        updateGeometry();
        update();
    }
    void setIconName(const QString& name) { icon_ = name; }
    void setOpen(bool open) // menu, panel mở rộng của nút đang mở: nền --c-press
    {
        open_ = open;
        update();
    }
    bool isOpen() const { return open_; }

    QSize sizeFor(bool large) const
    {
        const QFontMetrics fm(theme::font(large ? TextRole::Label : TextRole::Caption));
        const int text = fm.horizontalAdvance(this->text()) + gap() + kChevron;
        if (large)
            return {std::max(px(tok::lg_w), std::max(px(tok::ic_lg), text) + 2 * px(tok::sp_3)),
                    px(tok::sp_3) + px(tok::ic_lg) + px(tok::sp_2) + fm.height()};
        return {text + 2 * px(tok::sp_2), px(tok::h_lbl)};
    }
    QSize sizeHint() const override { return sizeFor(large_); }
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void keyPressEvent(QKeyEvent* e) override
    {
        const int k = e->key();
        if (e->modifiers() == Qt::NoModifier && (k == Qt::Key_Return || k == Qt::Key_Enter || k == Qt::Key_Down))
            return click(); // như Space; ↓ mở danh sách như split
        QAbstractButton::keyPressEvent(e);
    }
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const bool hover = isEnabled() && underMouse();
        if (isDown() || open_ || hover) {
            const qreal r = tm().px(tok::r);
            p.setPen(Qt::NoPen);
            p.setBrush(col(isDown() || open_ ? tok::c_press : tok::c_hover));
            p.drawRoundedRect(QRectF(rect()), r, r);
        }
        if (ui::paint::focusVisible(this))
            ui::paint::drawRing(p, rect(), col(tok::c_focus));
        const QColor ink = col(!isEnabled() ? tok::c_text_dis : large_ || hover || open_ ? tok::c_text : tok::c_text_3);
        const QFont f = theme::font(large_ ? TextRole::Label : TextRole::Caption);
        const QFontMetrics fm(f);
        int y = 0, h = height();
        if (large_) {
            const int ic = px(tok::ic_lg), top = px(tok::sp_3);
            const QPixmap pm =
                IconProvider::instance().pixmap(icon_, ic, p.device()->devicePixelRatio(), IconState::Normal);
            if (!pm.isNull())
                p.drawPixmap((width() - ic) / 2, top, ui::paint::tinted(pm, ink));
            y = top + ic + px(tok::sp_2);
            h = fm.height();
        }
        const int tw = fm.horizontalAdvance(text()), x = (width() - tw - gap() - kChevron) / 2;
        p.setFont(f);
        p.setPen(ink);
        p.drawText(QRect(x, y, tw, h), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, text());
        ui::paint::drawChevron(p, QPointF(x + tw + gap() + kChevron / 2.0, y + h / 2.0), col(tok::c_text_3));
    }

private:
    QString icon_;
    bool large_ = false;
    bool open_ = false;
};

// ---------------------------------------------------------------------------------------------------- Floating

// Vùng nổi trên cửa sổ (thân ribbon ở mức chỉ tên tab, panel mở rộng): con của window() đặt dưới anchor, không phải
// cửa sổ popup nên cú bấm ra ngoài vẫn tới vùng vẽ. Không ghim: bấm ra ngoài (trừ trong keep, và khi menu popup đang
// mở), focus ra ngoài, Esc thì đóng; bộ lọc của ứng dụng chỉ cài khi đang hiện mà không ghim. Ghim: chỉ Esc.
class Floating final : public QWidget {
public:
    Floating(RibbonBar* bar, QWidget* keep) : bar_(bar), keep_(keep)
    {
        hide();
        auto* box = new QVBoxLayout(this);
        box->setContentsMargins(kBorder, 0, kBorder, kBorder);
        box->setSpacing(0);
    }
    ~Floating() override { setFiltering(false); }

    std::function<void(bool byKey)> dismissed; // đóng vì bấm, focus ra ngoài (false) hoặc Esc (true)
    RibbonBar* bar() const { return bar_; }

    void setPinned(bool pinned)
    {
        pinned_ = pinned;
        setFiltering(isVisible() && !pinned);
    }

    // Hiện dưới anchor, mép trái theo anchor (dời vào trong cửa sổ), rộng ít nhất minWidth.
    void open(QWidget* anchor, int minWidth)
    {
        QWidget* win = anchor->window();
        if (win == anchor || inside(this, anchor))
            return;
        if (parentWidget() != win)
            setParent(win);
        const QSize s(std::max(minWidth, sizeHint().width()), sizeHint().height());
        QPoint at = win->mapFromGlobal(anchor->mapToGlobal(QPoint(0, anchor->height())));
        at.setX(std::clamp(at.x(), 0, std::max(0, win->width() - s.width())));
        setGeometry(QRect(at, s));
        show();
        raise();
        setFiltering(!pinned_);
    }

    void dismiss(bool byKey)
    {
        hide();
        if (dismissed)
            dismissed(byKey);
    }

protected:
    bool eventFilter(QObject* o, QEvent* e) override
    {
        if (e->type() == QEvent::MouseButtonPress && o->isWidgetType() && !QApplication::activePopupWidget()) {
            const auto* w = static_cast<QWidget*>(o);
            if (!inside(this, w) && !inside(keep_, w))
                dismiss(false);
        }
        return false;
    }
    void hideEvent(QHideEvent*) override { setFiltering(false); }
    void keyPressEvent(QKeyEvent* e) override
    {
        if (e->key() == Qt::Key_Escape && e->modifiers() == Qt::NoModifier)
            return dismiss(true);
        QWidget::keyPressEvent(e);
    }
    void contextMenuEvent(QContextMenuEvent* e) override
    {
        if (bar_)
            bar_->contextMenu(childAt(e->pos()))->popup(e->globalPos());
    }
    void paintEvent(QPaintEvent*) override // nền --c-base, nét --c-border trái, phải, dưới
    {
        QPainter p(this);
        p.fillRect(rect(), col(tok::c_base));
        const QColor line = col(tok::c_border);
        p.fillRect(QRect(0, 0, kBorder, height()), line);
        p.fillRect(QRect(width() - kBorder, 0, kBorder, height()), line);
        p.fillRect(QRect(0, height() - kBorder, width(), kBorder), line);
    }

private:
    void setFiltering(bool on)
    {
        if (on == filtering_)
            return;
        filtering_ = on;
        if (!on) {
            qApp->removeEventFilter(this);
            disconnect(focus_);
            return;
        }
        qApp->installEventFilter(this);
        focus_ = connect(qApp, &QApplication::focusChanged, this, [this](QWidget*, QWidget* now) {
            if (now && !inside(this, now) && !inside(keep_, now) && !QApplication::activePopupWidget())
                dismiss(false);
        });
    }

    QPointer<RibbonBar> bar_;
    QPointer<QWidget> keep_;
    QMetaObject::Connection focus_;
    bool pinned_ = false;
    bool filtering_ = false;
};

namespace {

// Ribbon chứa widget w (cả khi w nằm trong vùng nổi).
RibbonBar* barOf(QWidget* w)
{
    for (; w; w = w->parentWidget()) {
        if (auto* b = qobject_cast<RibbonBar*>(w))
            return b;
        if (auto* f = dynamic_cast<Floating*>(w))
            return f->bar();
    }
    return nullptr;
}

} // namespace

// ------------------------------------------------------------------------------------------------- RibbonPanel

RibbonPanel::RibbonPanel(const Panel& panel, const QString& key, RibbonControls& controls, settings::Store& store,
                         QWidget* parent)
    : QWidget(parent), panel_(panel), key_(key), controls_(controls), store_(store),
      body_(controls.buildPanel(panel, this)), drop_(new PanelButton(this))
{
    setObjectName(key);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    pinned_ = store.get(kPinnedPanels).contains(key);
    auto* box = new QVBoxLayout(this);
    box->setContentsMargins(px(tok::sp_2), px(tok::sp_2), px(tok::sp_2) + kBorder, 0); // .rb-p padding 4px 4px 0
    box->setSpacing(0);
    box->addWidget(body_, 1, Qt::AlignLeft | Qt::AlignTop);
    if (panel.more.empty()) {
        label_ = new QLabel(this);
        label_->setFont(theme::font(TextRole::Caption));
        label_->setAlignment(Qt::AlignCenter);
        box->addWidget(label_, 0, Qt::AlignHCenter);
    } else {
        more_ = new PanelButton(this);
        box->addWidget(more_, 0, Qt::AlignHCenter);
        // Bấm bằng bàn phím (nút đang có focus; chuột không lấy focus): focus vào vùng mở rộng.
        connect(more_, &QAbstractButton::clicked, this, [this] { setExpanded(!isExpanded(), more_->hasFocus()); });
    }
    drop_->setIconName(panel.icon.isEmpty() ? u"more"_s : panel.icon);
    drop_->hide();
    box->addWidget(drop_, 1, Qt::AlignHCenter);
    connect(drop_, &QAbstractButton::clicked, this, [this] {
        drop_->setOpen(true);
        menu()->popup(drop_->mapToGlobal(QPoint(0, drop_->height())));
    });
    retranslate();
    applyStyle();
}

RibbonPanel::~RibbonPanel()
{
    delete expansion_.data();
}

QAbstractButton* RibbonPanel::moreButton() const
{
    return more_;
}

QAbstractButton* RibbonPanel::dropButton() const
{
    return drop_;
}

void RibbonPanel::setState(State s)
{
    if (s == state_)
        return;
    state_ = s;
    const bool open = s == State::Full || s == State::Compact;
    for (RibbonButton* b : body_->findChildren<RibbonButton*>())
        b->setCompact(s == State::Compact);
    body_->setVisible(open);
    (label_ ? static_cast<QWidget*>(label_) : more_)->setVisible(open);
    drop_->setLarge(s == State::Collapsed);
    drop_->setVisible(!open);
    updateGeometry();
}

int RibbonPanel::widthFor(State s) const
{
    int& w = widths_[size_t(s)];
    if (w < 0) {
        const int frame = 2 * px(tok::sp_2) + kBorder;
        if (s == State::Full || s == State::Compact) {
            const int name = label_ ? label_->sizeHint().width() : more_->sizeFor(false).width();
            w = std::max(measureBox(static_cast<const QBoxLayout*>(body_->layout()), s == State::Compact), name) + frame;
        } else {
            w = drop_->sizeFor(s == State::Collapsed).width() + frame;
        }
    }
    return w;
}

void RibbonPanel::invalidate()
{
    widths_.fill(-1);
    updateGeometry();
}

QSize RibbonPanel::sizeHint() const
{
    return {widthFor(state_), QWidget::sizeHint().height()};
}

QMenu* RibbonPanel::menu()
{
    if (!menu_) {
        menu_ = new ui::Menu(this);
        connect(menu_, &QMenu::aboutToShow, this, &RibbonPanel::fillMenu);
        connect(menu_, &QMenu::aboutToHide, drop_, [this] { drop_->setOpen(false); });
    }
    return menu_;
}

// Menu của panel đã thu: điều khiển theo thứ tự của thân. Nút, toggle, phần chính của split theo nguồn: mục bấm như
// bấm nút (cùng cổng phiên bản, lựa chọn gần nhất); split: menu con là danh sách của chính nút; điều khiển khác: chính
// điều khiển trong menu (QWidgetAction). Panel mở rộng ở cuối.
void RibbonPanel::fillMenu()
{
    menu_->clear();
    const QList<RibbonButton*> buttons = body_->findChildren<RibbonButton*>();
    qsizetype next = 0;
    const auto iconOf = [](const QString& icon, const QString& cmd) {
        const QString name = icon.isEmpty() ? RibbonControls::commandIcon(cmd) : icon;
        return name.isEmpty() ? QIcon() : IconProvider::icon(name);
    };
    std::function<void(const std::vector<Item>&)> add = [&](const std::vector<Item>& items) {
        for (const Item& it : items) {
            if (it.kind != Item::Kind::Control) {
                add(it.items);
                continue;
            }
            const Control& c = it.control;
            if (c.type != ControlType::Button && c.type != ControlType::Split && c.type != ControlType::Toggle) {
                auto* a = new QWidgetAction(menu_);
                a->setDefaultWidget(controls_.create(c));
                menu_->addAction(a);
                continue;
            }
            RibbonButton* b = buttons.value(next++);
            if (!b)
                continue;
            const MenuItem main = b->mainItem();
            const QIcon icon = iconOf(main.cmd == c.cmd ? c.icon : main.icon, main.cmd);
            if (c.type != ControlType::Split || !c.source.isEmpty()) {
                QAction* a = menu_->addAction(icon, b->text());
                a->setEnabled(b->isEnabled());
                a->setCheckable(b->isCheckable());
                a->setChecked(b->isChecked());
                RichTooltip::attach(a, RichTooltip::info(b));
                connect(a, &QAction::triggered, b, [a, b] {
                    a->setChecked(b->isChecked()); // toggle: trạng thái chỉ theo sysvar
                    b->click();
                });
            }
            if (c.type == ControlType::Split) {
                QMenu* sub = b->menu();
                sub->setTitle(b->text());
                sub->setIcon(icon);
                menu_->addMenu(sub)->setEnabled(b->isEnabled());
            }
        }
    };
    add(panel_.items);
    if (panel_.more.empty())
        return;
    menu_->addSection(tr("%1 mở rộng").arg(RibbonModel::line(panel_.name)));
    for (const MenuItem& m : panel_.more) {
        if (m.separator) {
            menu_->addSeparator();
            continue;
        }
        const QString name = RibbonModel::line(m.label);
        QAction* a = menu_->addAction(iconOf(m.icon, m.cmd), name);
        const ui::Edition need = controls_.registry().need(m.cmd);
        const QAction* reg = controls_.registry().action(m.cmd);
        RichTooltip::attach(a, {.command = m.cmd,
                                .keyId = reg ? RichTooltip::info(reg).keyId : QString(),
                                .description = RibbonControls::describe(m.cmd, name),
                                .need = RibbonControls::locked(need) ? need : ui::Edition::None});
        connect(a, &QAction::triggered, this, [this, m, name] { controls_.run(m.cmd, m.args, name); });
    }
}

bool RibbonPanel::isExpanded() const
{
    return expansion_ && expansion_->isVisible();
}

QWidget* RibbonPanel::expansion() const
{
    return expansion_;
}

// Panel mở rộng: các mục more là nút nhỏ (cùng tooltip, khóa, cổng phiên bản của RibbonControls) xếp một cột; nút ghim
// cuối vùng.
QWidget* RibbonPanel::buildExpansion()
{
    auto* w = new QWidget;
    auto* column = new QVBoxLayout(w);
    column->setContentsMargins(px(tok::sp_2), px(tok::sp_2), px(tok::sp_2), px(tok::sp_2));
    column->setSpacing(kBorder); // .rb-col: khe 1 px
    for (const MenuItem& m : panel_.more) {
        if (m.separator) {
            column->addWidget(new Rule(w));
            continue;
        }
        const Control c{.type = ControlType::Button,
                        .size = ControlSize::Small,
                        .id = m.key(),
                        .label = m.label,
                        .icon = m.icon,
                        .cmd = m.cmd,
                        .args = m.args};
        column->addWidget(controls_.create(c, w), 0, Qt::AlignLeft);
    }
    auto* pin = new ui::ToggleButton({}, ui::Button::Kind::Icon, w);
    pin->setObjectName(u"pin"_s);
    pin->setControlSize(ui::Button::Size::Small);
    pin->setIconName(u"pin"_s);
    pin->setFocusPolicy(Qt::TabFocus);
    pin->setChecked(pinned_);
    connect(pin, &QAbstractButton::toggled, this, &RibbonPanel::setPinned);
    column->addWidget(pin, 0, Qt::AlignRight);
    return w;
}

void RibbonPanel::setExpanded(bool open, bool focus)
{
    if (!more_)
        return;
    if (!open) {
        if (expansion_)
            expansion_->hide();
        more_->setOpen(false);
        setPinned(false); // đóng bằng tay thì bỏ ghim
        return;
    }
    if (!expansion_) {
        expansion_ = new Floating(barOf(this), more_);
        expansion_->layout()->addWidget(buildExpansion());
        expansion_->setPinned(pinned_);
        expansion_->dismissed = [this](bool byKey) {
            more_->setOpen(false);
            setPinned(false);
            if (byKey)
                more_->setFocus(Qt::TabFocusReason);
        };
        retranslate();
    }
    expansion_->open(this, width());
    more_->setOpen(expansion_->isVisible());
    if (focus)
        if (QWidget* w = firstFocusable(expansion_))
            w->setFocus(Qt::TabFocusReason);
}

void RibbonPanel::setPinned(bool pinned)
{
    if (pinned == pinned_)
        return;
    pinned_ = pinned;
    QStringList list = store_.get(kPinnedPanels);
    list.removeAll(key_);
    if (pinned)
        list << key_;
    store_.set(kPinnedPanels, list);
    if (expansion_) {
        expansion_->setPinned(pinned);
        if (auto* pin = expansion_->findChild<QAbstractButton*>(u"pin"_s))
            pin->setChecked(pinned);
        retranslate();
    }
    syncExpansion();
}

void RibbonPanel::syncExpansion()
{
    if (!more_)
        return;
    if (more_->isVisible() && (pinned_ || isExpanded()))
        setExpanded(true);
    else if (isExpanded()) {
        expansion_->hide(); // nút ẩn (đổi tab, panel thu): ghim thì hiện lại cùng nút
        more_->setOpen(false);
    }
}

void RibbonPanel::retranslate()
{
    const QString name = RibbonModel::line(panel_.name);
    setAccessibleName(name); // nhóm điều khiển cho trình đọc màn hình
    if (label_)
        label_->setText(name);
    for (PanelButton* b : {more_, drop_}) {
        if (!b)
            continue;
        b->setText(name);
        b->updateGeometry();
    }
    if (more_) {
        const QString expand = tr("Mở rộng panel %1").arg(name);
        more_->setAccessibleName(expand);
        RichTooltip::attach(more_, {.name = expand});
    }
    drop_->setAccessibleName(tr("Panel %1").arg(name));
    RichTooltip::attach(drop_, {.name = name, .description = tr("Các lệnh của panel đã thu gọn.")});
    if (expansion_)
        expansion_->setAccessibleName(tr("%1 mở rộng").arg(name));
    if (auto* pin = expansion_ ? expansion_->findChild<QAbstractButton*>(u"pin"_s) : nullptr) {
        const QString text = pinned_ ? tr("Bỏ ghim panel mở rộng") : tr("Ghim panel mở rộng");
        pin->setAccessibleName(text);
        RichTooltip::attach(pin, {.name = text});
    }
    widths_.fill(-1);
}

void RibbonPanel::applyStyle()
{
    if (!label_)
        return;
    QPalette p = label_->palette(); // .rb-l: --c-text-3, cao --h-lbl
    p.setColor(QPalette::WindowText, col(tok::c_text_3));
    label_->setPalette(p);
    label_->setFixedHeight(px(tok::h_lbl));
}

void RibbonPanel::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    else if (e->type() == QEvent::PaletteChange || e->type() == QEvent::StyleChange)
        applyStyle();
    QWidget::changeEvent(e);
}

void RibbonPanel::paintEvent(QPaintEvent*)
{
    QPainter(this).fillRect(QRect(width() - kBorder, 0, kBorder, height()), col(tok::c_border)); // .rb-p nét phải
}

// --------------------------------------------------------------------------------------------------- RibbonBar

RibbonBar::RibbonBar(const RibbonModel& model, RibbonControls& controls, settings::Store& store, QWidget* parent)
    : QWidget(parent), model_(model), controls_(controls), store_(store), tabs_(new RibbonTabs(this)),
      collapseButton_(new ui::Button({}, ui::Button::Kind::Icon, this)),
      pages_(new RibbonPages(int(model.tabs().size()), [this](int i) { return buildPage(i); }, this)),
      collapse_(Collapse(std::clamp(store.get(kCollapse), 0, 2)))
{
    // Lần đầu mở tab không raster SVG trên luồng UI (0,1–0,2 ms mỗi biểu tượng): raster trước cả 8 tab ở nền.
    IconProvider::instance().prewarm(devicePixelRatio(), iconNames(model));
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    tabs_->setObjectName(u"tabs"_s);
    tabs_->setFocusPolicy(Qt::TabFocus);
    tabs_->setFont(theme::font(TextRole::Ui));
    collapseButton_->setObjectName(u"collapse"_s);
    collapseButton_->setControlSize(ui::Button::Size::Small);
    collapseButton_->setFocusPolicy(Qt::TabFocus);
    const QStringList hidden = store.get(kHiddenTabs);
    for (const Tab& t : model.tabs())
        tabs_->setTabVisible(tabs_->addTab(RibbonModel::text(t.name)), !hidden.contains(t.id));

    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, kBorder); // .rb: nét dưới
    column->setSpacing(0);
    auto* top = new QHBoxLayout;
    top->setContentsMargins(px(tok::sp_3), 0, px(tok::sp_3), 0); // .rb-tabs padding 0 6px
    top->addWidget(tabs_);
    top->addStretch();
    top->addWidget(collapseButton_, 0, Qt::AlignVCenter);
    column->addLayout(top);
    column->addWidget(pages_);

    connect(collapseButton_, &QAbstractButton::clicked, this,
            [this] { setCollapse(Collapse((int(collapse_) + 1) % 3)); });
    connect(tabs_, &QTabBar::currentChanged, this, &RibbonBar::showTab);
    connect(tabs_, &QTabBar::tabBarClicked, this, [this](int i) { // mức chỉ tên tab: bấm tab mở/đóng thân nổi
        if (i >= 0 && collapse_ == Collapse::TabNames)
            setFloating(!(isFloating() && i == tabs_->currentIndex()));
    });
    // Lệnh vừa chạy (bấm điều khiển, phím tắt): đóng thân nổi, panel mở rộng chưa ghim.
    for (QAction* a : controls.registry().actions())
        connect(a, &QAction::triggered, this, &RibbonBar::closeTransient);
    connect(&controls, &RibbonControls::macroRequested, this, &RibbonBar::closeTransient);
    connect(&controls, &RibbonControls::picked, this, &RibbonBar::closeTransient);
    if (auto* g = edition::EditionGate::instance()) // huy hiệu khóa đổi bề rộng nút
        connect(g, &edition::EditionGate::editionChanged, this, [this] { scheduleFit(true); });

    retranslate();
    applyCollapse();
    for (int i = 0; i < tabs_->count(); ++i) // tab đầu chưa ẩn; QTabBar chọn tab 0 cả khi tab đó ẩn
        if (tabs_->isTabVisible(i)) {
            setCurrentTab(i);
            break;
        }
    if (!pages_->currentWidget())
        showTab(tabs_->currentIndex());
}

RibbonBar::~RibbonBar()
{
    if (filtered_)
        filtered_->removeEventFilter(this);
    if (altDown_)
        qApp->removeEventFilter(this);
    for (RibbonPanel* p : pages_->findChildren<RibbonPanel*>())
        delete p->expansion(); // vùng nổi là con của cửa sổ
    if (pages_->parentWidget() != this)
        delete pages_; // đang ở trong thân nổi
    delete float_.data();
}

QTabBar* RibbonBar::tabBar() const
{
    return tabs_;
}

QAbstractButton* RibbonBar::collapseButton() const
{
    return collapseButton_;
}

QList<RibbonPanel*> RibbonBar::panels() const
{
    QWidget* page = pages_->currentWidget();
    return page ? page->findChildren<RibbonPanel*>(Qt::FindDirectChildrenOnly) : QList<RibbonPanel*>();
}

RibbonPanel* RibbonBar::panel(const QString& key) const
{
    return pages_->findChild<RibbonPanel*>(key);
}

int RibbonBar::currentTab() const
{
    return tabs_->currentIndex();
}

void RibbonBar::setCurrentTab(int index)
{
    if (!tabs_->isTabVisible(index))
        return;
    if (index == tabs_->currentIndex())
        showTab(index); // dựng nếu chưa (tab 0 của QTabBar có sẵn từ addTab)
    else
        tabs_->setCurrentIndex(index);
}

QWidget* RibbonBar::buildPage(int index)
{
    const Tab& t = model_.tabs()[size_t(index)];
    auto* page = new QWidget;
    page->setObjectName(t.id);
    auto* row = new QHBoxLayout(page);
    row->setContentsMargins(px(tok::sp_1), 0, px(tok::sp_1), 0); // .rb-body padding 0 2px
    row->setSpacing(0);
    const QStringList hidden = store_.get(kHiddenPanels);
    for (const Panel& p : t.panels) {
        const QString key = t.id + u'/' + p.id;
        auto* panel = new RibbonPanel(p, key, controls_, store_, page);
        panel->setVisible(!hidden.contains(key));
        row->addWidget(panel);
    }
    row->addStretch();
    return page;
}

void RibbonBar::showTab(int index)
{
    if (index < 0)
        return;
    const bool built = pages_->isBuilt(index);
    pages_->setCurrentTab(index);
    if (!built) { // trang dựng sau nằm cuối chuỗi focus của cửa sổ: đưa về ngay sau nút thu gọn, theo thứ tự trên màn hình
        QWidget* prev = collapseButton_;
        for (QWidget* w : pages_->currentWidget()->findChildren<QWidget*>())
            if ((w->focusPolicy() & Qt::TabFocus) && w->window() == prev->window()) { // không gồm popup của QComboBox
                QWidget::setTabOrder(prev, w);
                prev = w;
            }
    }
    fit();
    syncExpansions();
}

// fitRibbon của prototype trên bề rộng tính trước (RibbonPanel::widthFor), rồi áp trạng thái một lần.
void RibbonBar::fit()
{
    fitPending_ = false;
    QWidget* page = pages_->currentWidget();
    if (!page)
        return;
    if (std::exchange(remeasure_, false))
        for (RibbonPanel* p : pages_->findChildren<RibbonPanel*>())
            p->invalidate();
    QList<RibbonPanel*> list;
    for (RibbonPanel* p : page->findChildren<RibbonPanel*>(Qt::FindDirectChildrenOnly))
        if (!p->isHidden())
            list << p;
    const qsizetype n = list.size();
    std::vector<State> st(static_cast<size_t>(n), collapse_ == Collapse::PanelTitles ? State::Title : State::Full);
    if (collapse_ != Collapse::PanelTitles) {
        const int avail = width() - 2 * px(tok::sp_1);
        int total = 0;
        for (const RibbonPanel* p : list)
            total += p->widthFor(State::Full);
        const auto set = [&](qsizetype i, State s) {
            total += list[i]->widthFor(s) - list[i]->widthFor(st[size_t(i)]);
            st[size_t(i)] = s;
        };
        // Panel shrink > 1 thu trước: shrink lớn trước, cùng mức thì bên phải trước.
        std::vector<qsizetype> order(static_cast<size_t>(n));
        std::iota(order.begin(), order.end(), 0);
        std::ranges::sort(order, [&](qsizetype a, qsizetype b) {
            const int sa = list[a]->panel().shrink, sb = list[b]->panel().shrink;
            return sa != sb ? sa > sb : a > b;
        });
        for (qsizetype i : order)
            if (total > avail && list[i]->panel().shrink > 1)
                set(i, State::Collapsed);
        for (qsizetype i = n - 1; i >= 0; --i) // nút nhỏ bỏ chữ, từ phải sang
            if (total > avail && st[size_t(i)] != State::Collapsed)
                set(i, State::Compact);
        for (qsizetype i = n - 1; i >= 0; --i) // thu nốt, từ phải sang
            if (total > avail)
                set(i, State::Collapsed);
    }
    for (qsizetype i = 0; i < n; ++i)
        list[i]->setState(st[size_t(i)]);
    if (QLayout* l = page->layout()) // vị trí panel ngay (panel mở rộng đặt theo nó)
        l->activate();
}

void RibbonBar::scheduleFit(bool remeasure)
{
    remeasure_ = remeasure_ || remeasure;
    if (std::exchange(fitPending_, true))
        return;
    // Sau khi mọi widget xử lý xong sự kiện đang giao (LanguageChange tới con sau cha): một lần, không timer.
    QMetaObject::invokeMethod(
        this,
        [this] {
            if (fitPending_) {
                fit();
                syncExpansions();
            }
        },
        Qt::QueuedConnection);
}

void RibbonBar::setCollapse(Collapse c)
{
    if (c == collapse_)
        return;
    collapse_ = c;
    store_.set(kCollapse, int(c));
    applyCollapse();
}

void RibbonBar::applyCollapse()
{
    auto* column = static_cast<QVBoxLayout*>(layout());
    const bool docked = collapse_ != Collapse::TabNames;
    if (docked) {
        if (float_)
            float_->hide();
        if (column->indexOf(pages_) < 0) {
            pages_->setParent(this);
            column->addWidget(pages_);
        }
        pages_->show();
    } else if (column->indexOf(pages_) >= 0) { // chỉ hiện khi nổi
        column->removeWidget(pages_);
        pages_->hide();
    }
    pages_->setFixedHeight(bodyHeight(docked ? collapse_ : Collapse::Full));
    setFixedHeight(px(tok::h_tab) + bodyHeight(collapse_) + kBorder);
    collapseButton_->setIconName(collapse_ == Collapse::TabNames ? u"down"_s : u"up"_s);
    fit();
    syncExpansions();
}

bool RibbonBar::isTabHidden(const QString& tab) const
{
    return store_.get(kHiddenTabs).contains(tab);
}

void RibbonBar::setTabHidden(const QString& tab, bool hidden)
{
    const auto& tabs = model_.tabs();
    const auto it = std::ranges::find(tabs, tab, &Tab::id);
    if (it == tabs.end() || hidden == isTabHidden(tab))
        return;
    const int index = int(it - tabs.begin());
    int shown = 0;
    for (int i = 0; i < tabs_->count(); ++i)
        shown += tabs_->isTabVisible(i);
    if (hidden && shown <= 1)
        return;
    QStringList list = store_.get(kHiddenTabs);
    list.removeAll(tab);
    if (hidden)
        list << tab;
    store_.set(kHiddenTabs, list);
    tabs_->setTabVisible(index, !hidden); // tab đang mở bị ẩn: QTabBar chọn tab hiện kế bên
}

QStringList RibbonBar::hiddenPanels() const
{
    return store_.get(kHiddenPanels);
}

bool RibbonBar::isPanelHidden(const QString& key) const
{
    return hiddenPanels().contains(key);
}

void RibbonBar::setPanelHidden(const QString& key, bool hidden)
{
    QStringList list = hiddenPanels();
    if (list.contains(key) == hidden)
        return;
    list.removeAll(key);
    if (hidden)
        list << key;
    store_.set(kHiddenPanels, list);
    if (RibbonPanel* p = panel(key)) {
        p->setVisible(!hidden);
        if (hidden)
            p->syncExpansion();
    }
    fit();
    syncExpansions();
}

bool RibbonBar::isFloating() const
{
    return float_ && float_->isVisible();
}

QWidget* RibbonBar::floatingBody() const
{
    return float_;
}

void RibbonBar::setFloating(bool open)
{
    open = open && collapse_ == Collapse::TabNames && window() != this;
    if (open == isFloating())
        return;
    if (!open) {
        float_->hide();
        syncExpansions();
        return;
    }
    if (!float_) {
        float_ = new Floating(this, tabs_);
        float_->setAccessibleName(accessibleName());
        float_->dismissed = [this](bool byKey) {
            syncExpansions();
            if (byKey)
                tabs_->setFocus(Qt::TabFocusReason);
        };
    }
    if (pages_->parentWidget() != float_) {
        pages_->setParent(float_);
        float_->layout()->addWidget(pages_);
    }
    pages_->show();
    float_->open(this, width());
    fit();
    syncExpansions();
}

void RibbonBar::syncExpansions()
{
    for (RibbonPanel* p : pages_->findChildren<RibbonPanel*>())
        p->syncExpansion();
}

void RibbonBar::closeTransient()
{
    setFloating(false);
    for (RibbonPanel* p : pages_->findChildren<RibbonPanel*>())
        if (p->isExpanded() && !p->isPinned())
            p->setExpanded(false);
}

bool RibbonBar::hasFocusInside() const
{
    const QWidget* f = QApplication::focusWidget();
    return inside(this, f) || inside(float_, f);
}

void RibbonBar::enter()
{
    if (!hasFocusInside())
        returnFocus_ = QApplication::focusWidget();
    tabs_->setFocus(Qt::ShortcutFocusReason);
}

void RibbonBar::enterBody()
{
    if (collapse_ == Collapse::TabNames)
        setFloating(true);
    if (QWidget* page = pages_->currentWidget())
        if (QWidget* w = firstFocusable(page))
            w->setFocus(Qt::TabFocusReason);
}

void RibbonBar::leave()
{
    setFloating(false);
    QWidget* back = std::exchange(returnFocus_, nullptr);
    if (back && back->isVisible())
        back->setFocus(Qt::ShortcutFocusReason);
    else if (QWidget* f = QApplication::focusWidget(); inside(this, f))
        f->clearFocus();
}

QMenu* RibbonBar::contextMenu(QWidget* at)
{
    auto* m = new ui::Menu(this);
    m->setAttribute(Qt::WA_DeleteOnClose);
    // Lệnh của điều khiển dưới con trỏ (thông tin tooltip của nó).
    QString cmd;
    for (QObject* o = at; o && cmd.isEmpty() && o != this; o = o->parent())
        cmd = RichTooltip::info(o).command;
    if (!cmd.isEmpty() && controls_.registry().action(cmd)) {
        connect(m->addAction(tr("Thêm vào thanh truy cập nhanh")), &QAction::triggered, this,
                [this, cmd] { emit addToQatRequested(cmd); });
        m->addSeparator();
    }
    auto* tabsMenu = new ui::Menu(tr("Hiện tab"), m);
    m->addMenu(tabsMenu);
    int shown = 0;
    for (int i = 0; i < tabs_->count(); ++i)
        shown += tabs_->isTabVisible(i);
    for (const Tab& t : model_.tabs()) {
        QAction* a = tabsMenu->addAction(RibbonModel::text(t.name));
        a->setCheckable(true);
        a->setChecked(!isTabHidden(t.id));
        a->setEnabled(!a->isChecked() || shown > 1); // không ẩn tab cuối cùng
        connect(a, &QAction::toggled, this, [this, id = t.id](bool on) { setTabHidden(id, !on); });
    }
    if (const int cur = tabs_->currentIndex(); cur >= 0) {
        auto* panelsMenu = new ui::Menu(tr("Hiện panel"), m);
        m->addMenu(panelsMenu);
        const Tab& t = model_.tabs()[size_t(cur)];
        for (const Panel& p : t.panels) {
            QAction* a = panelsMenu->addAction(RibbonModel::line(p.name));
            const QString key = t.id + u'/' + p.id;
            a->setCheckable(true);
            a->setChecked(!isPanelHidden(key));
            connect(a, &QAction::toggled, this, [this, key](bool on) { setPanelHidden(key, !on); });
        }
    }
    auto* collapseMenu = new ui::Menu(tr("Thu gọn"), m);
    m->addMenu(collapseMenu);
    auto* group = new QActionGroup(collapseMenu);
    const QString names[] = {tr("Đầy đủ"), tr("Chỉ tên panel"), tr("Chỉ tên tab")};
    for (int i = 0; i < 3; ++i) {
        QAction* a = collapseMenu->addAction(names[i]);
        a->setCheckable(true);
        a->setChecked(i == int(collapse_));
        group->addAction(a);
        connect(a, &QAction::triggered, this, [this, i] { setCollapse(Collapse(i)); });
    }
    return m;
}

QSize RibbonBar::sizeHint() const
{
    return {tabs_->sizeHint().width(), px(tok::h_tab) + bodyHeight(collapse_) + kBorder};
}

bool RibbonBar::eventFilter(QObject* watched, QEvent* e)
{
    // Alt nhấn rồi thả, không kèm phím, nút chuột nào: vào dải tab (hoặc trả focus nếu đang ở ribbon), như thanh menu
    // của Windows. Phím chưa ô nào nhận đi lên tới cửa sổ: bộ lọc trên cửa sổ thấy Alt nhấn; bộ lọc của ứng dụng chỉ
    // cài từ lúc đó đến khi thả.
    const QEvent::Type t = e->type();
    const bool key = t == QEvent::KeyPress || t == QEvent::KeyRelease || t == QEvent::ShortcutOverride;
    if (!altDown_) {
        if (watched == filtered_.data() && t == QEvent::ShortcutOverride) {
            const auto* k = static_cast<QKeyEvent*>(e);
            if (k->key() == Qt::Key_Alt && !(k->modifiers() & ~(Qt::AltModifier | Qt::KeypadModifier)) &&
                !k->isAutoRepeat() && isVisible()) {
                altDown_ = true;
                qApp->installEventFilter(this);
            }
        }
        return false;
    }
    if (key && static_cast<QKeyEvent*>(e)->key() == Qt::Key_Alt) {
        if (t != QEvent::KeyRelease)
            return false;
        altDown_ = false;
        qApp->removeEventFilter(this);
        if (hasFocusInside())
            leave();
        else
            enter();
        return false;
    }
    if (key || t == QEvent::MouseButtonPress || t == QEvent::Wheel || t == QEvent::ApplicationDeactivate) {
        altDown_ = false;
        qApp->removeEventFilter(this);
    }
    return false;
}

void RibbonBar::showEvent(QShowEvent* e)
{
    if (filtered_ != window()) {
        if (filtered_)
            filtered_->removeEventFilter(this);
        filtered_ = window();
        filtered_->installEventFilter(this);
    }
    QWidget::showEvent(e);
    scheduleFit(false); // panel ghim mở lại khi đã có chỗ trên màn hình
}

void RibbonBar::hideEvent(QHideEvent* e)
{
    // RIBBONCLOSE, Clean Screen: vùng nổi là con của cửa sổ nên ẩn theo tay; panel ghim hiện lại khi ribbon hiện.
    if (float_)
        float_->hide();
    QWidget::hideEvent(e);
    scheduleFit(false); // con còn cờ hiện tới khi Qt ẩn xong: đồng bộ panel mở rộng ở lượt sau
}

void RibbonBar::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    fit();
    if (isFloating())
        float_->open(this, width());
    syncExpansions();
}

void RibbonBar::keyPressEvent(QKeyEvent* e)
{
    // Phím các điều khiển không dùng đi lên tới đây.
    const QWidget* f = QApplication::focusWidget();
    const bool onTabs = f == tabs_;
    if (e->modifiers() == Qt::NoModifier) {
        switch (e->key()) {
        case Qt::Key_Down:
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (onTabs)
                return enterBody();
            break;
        case Qt::Key_Escape:
            if (onTabs || f == collapseButton_)
                return leave();
            if (inside(pages_, f))
                return tabs_->setFocus(Qt::TabFocusReason);
            break;
        default:
            break;
        }
    }
    QWidget::keyPressEvent(e);
}

void RibbonBar::contextMenuEvent(QContextMenuEvent* e)
{
    contextMenu(childAt(e->pos()))->popup(e->globalPos());
}

void RibbonBar::changeEvent(QEvent* e)
{
    switch (e->type()) {
    case QEvent::LanguageChange:
        retranslate();
        scheduleFit(true);
        break;
    case QEvent::StyleChange: // mật độ: --h-btn, --h-lbl
    case QEvent::FontChange:
        applyCollapse();
        scheduleFit(true);
        break;
    default:
        break;
    }
    QWidget::changeEvent(e);
}

void RibbonBar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), col(tok::c_base));                                     // .rb
    p.fillRect(QRect(0, 0, width(), px(tok::h_tab)), col(tok::c_chrome));     // .rb-tabs
    p.fillRect(QRect(0, height() - kBorder, width(), kBorder), col(tok::c_border)); // nét dưới
}

void RibbonBar::retranslate()
{
    for (int i = 0; i < tabs_->count(); ++i)
        tabs_->setTabText(i, RibbonModel::text(model_.tabs()[size_t(i)].name));
    setAccessibleName(tr("Ribbon"));
    tabs_->setAccessibleName(tr("Tab ribbon"));
    const QString name = tr("Thu gọn ribbon");
    collapseButton_->setAccessibleName(name);
    RichTooltip::attach(collapseButton_, {.name = name, .description = tr("Đầy đủ → chỉ tên panel → chỉ tên tab.")});
}

} // namespace trion::ribbon
