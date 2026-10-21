#include "app/widgets/dock/AutoHideStrip.h"

#include "app/theme/ThemeManager.h"
#include "core/base/tsan.h"

#include <QAbstractButton>
#include <QApplication>
#include <QBoxLayout>
#include <QContextMenuEvent>
#include <QCursor>
#include <QDockWidget>
#include <QKeyEvent>
#include <QMainWindow>
#include <QPainter>
#include <QPropertyAnimation>

using namespace Qt::StringLiterals;

namespace trion::ui {

namespace {

theme::ThemeManager& tm()
{
    return theme::ThemeManager::instance();
}
int px(tok::PxId id)
{
    return qRound(tm().px(id));
}

constexpr int kBorder = 1; // nét chia của dải phía vùng vẽ (.dock border 1 px), viền khung trượt (.palhost border)

Qt::ToolBarArea toolBarArea(Qt::DockWidgetArea side)
{
    return side == Qt::LeftDockWidgetArea     ? Qt::LeftToolBarArea
           : side == Qt::BottomDockWidgetArea ? Qt::BottomToolBarArea
                                              : Qt::RightToolBarArea;
}

// Tab của dải (.strip .vtab): chữ --fs-xs đọc từ dưới lên (writing-mode:vertical-rl; rotate(180deg)), đệm 10 px hai
// đầu, ngang đủ 22 px; rê hoặc đang mở: nền --c-hover, chữ --c-text; focus bàn phím: viền --c-focus (:focus-visible).
// checked = đang mở (aria-expanded của prototype).
class Tab final : public QAbstractButton {
public:
    Tab(bool vertical, QWidget* parent) : QAbstractButton(parent), vertical_(vertical)
    {
        setCheckable(true);
        setFocusPolicy(Qt::TabFocus); // bấm chuột không lấy focus của dòng lệnh
    }

    QSize sizeHint() const override
    {
        const int pad = px(tok::sp_4) + px(tok::sp_1); // padding:10px 0
        const int len = QFontMetrics(tm().font(tok::fs_xs)).horizontalAdvance(text()) + 2 * pad;
        return vertical_ ? QSize(px(tok::strip), len) : QSize(len, px(tok::strip));
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const bool hot = underMouse() || isChecked();
        const qreal r = tm().px(tok::r);
        if (hot) {
            p.setPen(Qt::NoPen);
            p.setBrush(tm().color(tok::c_hover));
            p.drawRoundedRect(rect(), r, r);
        }
        if (hasFocus()) {
            p.setPen(QPen(tm().color(tok::c_focus), 2));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), r, r);
        }
        p.setPen(tm().color(hot ? tok::c_text : tok::c_text_2));
        p.setFont(tm().font(tok::fs_xs));
        QRect box = rect();
        if (vertical_) {
            p.translate(0, height());
            p.rotate(-90);
            box = QRect(0, 0, height(), width());
        }
        p.drawText(box, Qt::AlignCenter, text());
    }
    void enterEvent(QEnterEvent* e) override
    {
        update();
        QAbstractButton::enterEvent(e);
    }
    void leaveEvent(QEvent* e) override
    {
        update();
        QAbstractButton::leaveEvent(e);
    }
    void nextCheckState() override {} // bấm không tự đổi checked: AutoHideStrip mở/thu rồi đặt

private:
    bool vertical_;
};

// Khung của một palette trong khung trượt (.dock.flyout .palhost): nền --c-base, viền 1 px --c-border-ctl; đầu palette
// rồi thân, cỡ thân như khi dock.
class Sheet final : public QWidget {
public:
    explicit Sheet(QWidget* parent) : QWidget(parent)
    {
        auto* col = new QVBoxLayout(this);
        col->setContentsMargins(kBorder, kBorder, kBorder, kBorder);
        col->setSpacing(0);
        hide();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.fillRect(rect(), tm().color(tok::c_border_ctl));
        p.fillRect(rect().adjusted(kBorder, kBorder, -kBorder, -kBorder), tm().color(tok::c_base));
    }
};

} // namespace

// Khung trượt: con của cửa sổ, nằm trên vùng vẽ. Lúc trượt, khung hẹp dần về phía dải còn khung của palette giữ cỡ
// cuối (không bố cục lại mỗi khung hình) và bám mép đang trượt ra, nên phần vượt khỏi khung bị cắt.
class AutoHideStrip::Flyout final : public QWidget {
public:
    Flyout(AutoHideStrip* strip, QWidget* parent) : QWidget(parent), strip_(strip) { hide(); }

    // Cài lên qApp khi đang mở. Esc gửi cho widget ngoài khung trong cửa sổ chính (dòng lệnh, vùng vẽ; không có
    // popup) là của khung trượt: ShortcutOverride nhận để không phím tắt Esc nào chạy, KeyPress thì thu lại; widget
    // đó không nhận Esc này. Focus ở trong khung thì Esc tới widget đó trước (ô tìm còn chữ thì xóa chữ), không ai
    // dùng thì lên tới keyPressEvent của khung.
    bool eventFilter(QObject* watched, QEvent* e) override
    {
        TRION_TSAN_ACQUIRE(e); // mọi sự kiện, cả sự kiện đánh thức của UiExecutor dựng ở luồng nền (core/base/tsan.h)
        if (e->type() != QEvent::KeyPress && e->type() != QEvent::ShortcutOverride)
            return false;
        const auto* k = static_cast<QKeyEvent*>(e);
        const auto* w = qobject_cast<QWidget*>(watched);
        if (k->key() != Qt::Key_Escape || k->modifiers() != Qt::NoModifier || !w || w->window() != parentWidget() ||
            w == this || isAncestorOf(w) || QApplication::activePopupWidget())
            return false;
        e->accept();
        if (e->type() == QEvent::KeyPress)
            strip_->escape();
        return true;
    }

protected:
    void keyPressEvent(QKeyEvent* e) override
    {
        if (e->key() == Qt::Key_Escape && e->modifiers() == Qt::NoModifier)
            strip_->escape(); // như Esc trong palette dock: về vùng vẽ
        else
            QWidget::keyPressEvent(e);
    }
    void resizeEvent(QResizeEvent*) override
    {
        const qsizetype i = strip_->find(strip_->current());
        if (i < 0)
            return;
        QWidget* sheet = strip_->entries_[i].sheet; // dải trái: bám mép phải; phải, dưới: bám mép trái, trên
        sheet->move(strip_->side_ == Qt::LeftDockWidgetArea ? width() - sheet->width() : 0, 0);
    }
    void leaveEvent(QEvent*) override { strip_->maybeCollapse(); }

private:
    AutoHideStrip* strip_;
};

AutoHideStrip::AutoHideStrip(Qt::DockWidgetArea side, QMainWindow* window)
    : QToolBar(window), window_(window), side_(side), flyout_(new Flyout(this, window)),
      slide_(new QPropertyAnimation(flyout_, "geometry", flyout_))
{
    // Tên cho QMainWindow::saveState; không di chuyển, không nổi, không vào menu toolbar/dock của cửa sổ.
    setObjectName(side == Qt::LeftDockWidgetArea     ? u"strip-left"_s
                  : side == Qt::BottomDockWidgetArea ? u"strip-bottom"_s
                                                     : u"strip-right"_s);
    setMovable(false);
    setFloatable(false);
    setAllowedAreas(toolBarArea(side));
    toggleViewAction()->setVisible(false);
    setWindowTitle(tr("Palette tự ẩn")); // aria-label của prototype; tên cho trình đọc màn hình
    setAccessibleName(windowTitle());
    hover_.setSingleShot(true);
    hover_.setInterval(kHoverMs);
    hover_.setTimerType(Qt::PreciseTimer); // 300 ms đúng như hẹn (timer thô có thể sớm 5%); chỉ chạy khi đang rê
    connect(&hover_, &QTimer::timeout, this, [this] { open(pending_); });
    slide_->setEasingCurve(QEasingCurve::OutCubic);
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget* old, QWidget* now) {
        if (current() && old && flyout_->isAncestorOf(old) && !(now && flyout_->isAncestorOf(now)))
            maybeCollapse();
    });
    window->addToolBar(toolBarArea(side), this);
    hide();
}

AutoHideStrip::~AutoHideStrip()
{
    delete flyout_;
}

QList<QDockWidget*> AutoHideStrip::palettes() const
{
    QList<QDockWidget*> out;
    for (const Entry& e : entries_)
        out << e.dock;
    return out;
}

qsizetype AutoHideStrip::find(const QDockWidget* dock) const
{
    for (qsizetype i = 0; i < entries_.size(); ++i)
        if (entries_[i].dock == dock)
            return i;
    return -1;
}

void AutoHideStrip::addPalette(QDockWidget* dock)
{
    if (!dock || contains(dock))
        return;
    auto* tab = new Tab(side_ != Qt::BottomDockWidgetArea, this);
    const Entry e{dock, tab, new Sheet(flyout_), dock->titleBarWidget(), dock->widget()};
    dock->hide();                     // QMainWindow trả chỗ cho vùng vẽ, giữ chỗ cũ của dock
    dock->setTitleBarWidget(nullptr); // QDockWidget ẩn widget cũ, bỏ khỏi bố cục của nó, không hủy
    dock->setWidget(nullptr);
    auto* col = static_cast<QVBoxLayout*>(e.sheet->layout());
    for (QWidget* w : {e.header, e.body}) {
        if (!w)
            continue;
        w->setParent(e.sheet);
        col->addWidget(w, w == e.body ? 1 : 0);
        w->show(); // bỏ trạng thái ẩn QDockWidget đặt; hiện thật khi khung hiện
    }
    e.sheet->resize(e.sheet->sizeHint()); // cỡ như khi dock (vẽ khi ẩn bằng render() cũng đúng cỡ)
    tab->setText(dock->windowTitle());
    e.sheet->setAccessibleName(dock->windowTitle());
    connect(dock, &QWidget::windowTitleChanged, tab, &QAbstractButton::setText);
    connect(dock, &QWidget::windowTitleChanged, e.sheet, &QWidget::setAccessibleName);
    connect(tab, &QAbstractButton::clicked, this, [this, dock, tab] {
        if (current() == dock)
            collapse();
        else
            open(dock, tab->hasFocus()); // Space khi tab có focus (bàn phím): focus vào palette
    });
    tab->installEventFilter(this);
    tab->show();
    entries_ << e;
    show();
    relayout();
}

void AutoHideStrip::removePalette(QDockWidget* dock)
{
    const qsizetype i = find(dock);
    if (i < 0)
        return;
    if (current() == dock)
        collapse();
    const Entry e = entries_.takeAt(i);
    if (pending_ == dock)
        hover_.stop();
    if (e.header)
        dock->setTitleBarWidget(e.header); // QLayout gỡ khỏi khung, đổi cha về dock, hiện
    if (e.body)
        dock->setWidget(e.body);
    delete e.tab;
    delete e.sheet;
    setVisible(!entries_.isEmpty());
    relayout();
}

QDockWidget* AutoHideStrip::current() const
{
    for (const Entry& e : entries_)
        if (e.tab->isChecked())
            return e.dock;
    return nullptr;
}

QAbstractButton* AutoHideStrip::tab(const QDockWidget* dock) const
{
    const qsizetype i = find(dock);
    return i < 0 ? nullptr : entries_[i].tab;
}

void AutoHideStrip::open(QDockWidget* dock, bool focus)
{
    hover_.stop();
    const qsizetype i = find(dock);
    if (i < 0)
        return;
    window_->layout()->activate(); // dải vừa hiện (palette đầu tiên): QMainWindow xếp chỗ ngay để đo đúng
    QDockWidget* was = current();
    if (was != dock) {
        if (was) { // đang mở palette khác: đổi ngay, không trượt lại
            const Entry& o = entries_[find(was)];
            o.tab->setChecked(false);
            o.sheet->hide();
        }
        const Entry& e = entries_[i];
        e.tab->setChecked(true);
        e.sheet->show();
        flyout_->raise();
        // Chỗ bắt đầu trượt: khung rỗng sát mép dải (hiện ở đây thì không phủ con trỏ, không sinh Enter/Leave).
        const QRect g = geometry();
        const QRect from = side_ == Qt::LeftDockWidgetArea    ? QRect(g.right(), g.top(), 0, g.height())
                           : side_ == Qt::RightDockWidgetArea ? QRect(g.left() + kBorder, g.top(), 0, g.height())
                                                              : QRect(g.left(), g.top() + kBorder, g.width(), 0);
        if (!was) {
            flyout_->setGeometry(from);
            flyout_->show();
            qApp->installEventFilter(flyout_);
        }
        emit aboutToOpen(dock); // nội dung dựng khi khung đã hiện: khung đầu có nội dung (UiProbe palette_open)
        const QRect to = flyoutRect();
        e.sheet->resize(to.size());
        slide_->stop();
        const int ms = tm().ms(tok::t); // 0 khi Giao diện nhẹ hoặc giảm chuyển động
        if (was || ms <= 0) {
            flyout_->setGeometry(to);
        } else {
            slide_->setDuration(ms);
            slide_->setStartValue(from);
            slide_->setEndValue(to);
            slide_->start();
        }
    }
    if (focus)
        emit focusRequested(dock);
}

void AutoHideStrip::collapse()
{
    hover_.stop();
    const qsizetype i = find(current());
    if (i < 0)
        return;
    slide_->stop();
    qApp->removeEventFilter(flyout_);
    entries_[i].tab->setChecked(false);
    entries_[i].sheet->hide();
    flyout_->hide();
}

QRect AutoHideStrip::flyoutRect() const
{
    const qsizetype i = find(current());
    if (i < 0)
        return {};
    // Phủ dọc theo cả dải (vùng dock của cạnh đó), rộng theo palette (ít nhất --pal-w như khi dock), không quá cửa sổ.
    // Viền khung đè lên nét chia của dải như prototype (.palhost nằm trên viền của .dock).
    const QRect g = geometry();
    const QSize want = entries_[i].sheet->sizeHint();
    if (side_ == Qt::LeftDockWidgetArea)
        return {g.right(), g.top(), std::min(want.width(), window_->width() - g.right()), g.height()};
    if (side_ == Qt::RightDockWidgetArea) {
        const int w = std::min(want.width(), g.left() + kBorder);
        return {g.left() + kBorder - w, g.top(), w, g.height()};
    }
    const int h = std::min(want.height(), g.top() + kBorder);
    return {g.left(), g.top() + kBorder - h, g.width(), h};
}

void AutoHideStrip::place()
{
    const qsizetype i = find(current());
    if (i < 0)
        return;
    slide_->stop();
    const QRect to = flyoutRect();
    entries_[i].sheet->resize(to.size());
    flyout_->setGeometry(to);
}

bool AutoHideStrip::hovered() const
{
    const QPoint p = window_->mapFromGlobal(QCursor::pos());
    return geometry().contains(p) || flyoutRect().contains(p);
}

void AutoHideStrip::maybeCollapse()
{
    if (!current() || QApplication::activePopupWidget() || hovered())
        return;
    const QWidget* f = QApplication::focusWidget();
    if (f && flyout_->isAncestorOf(f)) // đang làm việc trong palette: thu khi focus rời hoặc Esc
        return;
    collapse();
}

void AutoHideStrip::escape()
{
    const QWidget* f = QApplication::focusWidget();
    if (f && flyout_->isAncestorOf(f) && window_->centralWidget())
        window_->centralWidget()->setFocus(Qt::ShortcutFocusReason); // như Esc trong palette dock: về vùng vẽ
    collapse();
}

void AutoHideStrip::relayout()
{
    // .strip{padding-top:6px; gap:4px}; tab sát nét chia phía vùng vẽ.
    const bool vertical = side_ != Qt::BottomDockWidgetArea;
    const int inner = side_ == Qt::LeftDockWidgetArea ? 0 : kBorder;
    int at = px(tok::sp_3);
    for (const Entry& e : std::as_const(entries_)) {
        const QSize s = e.tab->sizeHint();
        e.tab->setGeometry(vertical ? QRect(QPoint(inner, at), s) : QRect(QPoint(at, inner), s));
        at += (vertical ? s.height() : s.width()) + px(tok::sp_2);
    }
    updateGeometry();
}

QSize AutoHideStrip::sizeHint() const
{
    const bool vertical = side_ != Qt::BottomDockWidgetArea;
    int len = px(tok::sp_3);
    for (const Entry& e : entries_)
        len += (vertical ? e.tab->sizeHint().height() : e.tab->sizeHint().width()) + px(tok::sp_2);
    const int across = px(tok::strip) + kBorder;
    return vertical ? QSize(across, len) : QSize(len, across);
}

bool AutoHideStrip::event(QEvent* e)
{
    if (e->type() == QEvent::LayoutRequest)
        relayout();
    return QToolBar::event(e);
}

bool AutoHideStrip::eventFilter(QObject* watched, QEvent* e)
{
    // Chỉ cài lên các tab: rê 300 ms thì mở (timer chỉ chạy khi đang rê), rời tab thì hủy.
    if (e->type() == QEvent::Enter) {
        for (const Entry& x : std::as_const(entries_))
            if (x.tab == watched && x.dock != current()) {
                pending_ = x.dock;
                hover_.start();
            }
    } else if (e->type() == QEvent::Leave) {
        hover_.stop();
        maybeCollapse();
    }
    return QToolBar::eventFilter(watched, e);
}

void AutoHideStrip::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), tm().color(tok::c_chrome));
    const QRect line = side_ == Qt::LeftDockWidgetArea    ? QRect(width() - kBorder, 0, kBorder, height())
                       : side_ == Qt::RightDockWidgetArea ? QRect(0, 0, kBorder, height())
                                                          : QRect(0, 0, width(), kBorder);
    p.fillRect(line, tm().color(tok::c_border));
}

void AutoHideStrip::resizeEvent(QResizeEvent* e)
{
    QToolBar::resizeEvent(e);
    place();
}

void AutoHideStrip::moveEvent(QMoveEvent* e)
{
    QToolBar::moveEvent(e);
    place();
}

void AutoHideStrip::leaveEvent(QEvent* e)
{
    QToolBar::leaveEvent(e);
    maybeCollapse();
}

void AutoHideStrip::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::LanguageChange) {
        setWindowTitle(tr("Palette tự ẩn"));
        setAccessibleName(windowTitle());
    }
    QToolBar::changeEvent(e);
}

void AutoHideStrip::contextMenuEvent(QContextMenuEvent* e)
{
    e->accept();
}

} // namespace trion::ui
