#include "app/widgets/dock/PaletteHost.h"

#include "app/core/perf/UiProbe.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/controls/Controls.h"
#include "app/widgets/dock/PaletteHeader.h"
#include "core/base/assert.h"

#include <QAction>
#include <QApplication>
#include <QBoxLayout>
#include <QDockWidget>
#include <QKeyEvent>
#include <QMainWindow>
#include <QMenu>
#include <QScreen>

#include <algorithm>

namespace trion::ui {

namespace {

const settings::Setting<QByteArray> kState{"ui/palettes/state", {}};        // QMainWindow::saveState
const settings::Setting<QStringList> kUnpinned{"ui/palettes/unpinned", {}}; // mã các palette bỏ ghim
constexpr int kLayoutVersion = 1; // tăng khi đổi bố cục không tương thích: bản đã lưu bị bỏ, về mặc định

// Thật sự hiện cho người dùng: tab nền của nhóm vẫn isVisible() nhưng Qt dời nó ra tọa độ âm (QWidgetAnimator).
bool onScreen(const QWidget* w)
{
    return w->isVisible() && !w->visibleRegion().isEmpty();
}

// Widget đầu tiên nhận focus bằng Tab trong cây của root (theo chuỗi focus), nullptr nếu không có.
QWidget* firstFocusable(QWidget* root)
{
    QWidget* w = root;
    do {
        if ((w->focusPolicy() & Qt::TabFocus) && w->isEnabled() && w->isVisible() &&
            (w == root || root->isAncestorOf(w)))
            return w;
        w = w->nextInFocusChain();
    } while (w && w != root);
    return nullptr;
}

// Thân palette: nội dung dựng trễ vào đây. QMainWindow lấy bề rộng đầu của dock từ sizeHint của thân (không phải
// của QDockWidget), nên thân rộng ít nhất --pal-w khi nội dung không đòi rộng hơn.
class Body final : public QWidget {
public:
    Body() { (new QVBoxLayout(this))->setContentsMargins({}); }
    QSize sizeHint() const override
    {
        QSize s = QWidget::sizeHint();
        s.setWidth(std::max(s.width(), qRound(theme::ThemeManager::instance().px(tok::pal_w))));
        return s;
    }
};

} // namespace

class PaletteHost::Dock final : public QDockWidget {
public:
    Dock(PaletteHost* host, PaletteSpec s, QMainWindow* window) : QDockWidget(window), spec(std::move(s)), host_(host)
    {
        setObjectName(spec.id);
        setWindowTitle(spec.title());
        setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);
        setFeatures(DockWidgetClosable | DockWidgetMovable | DockWidgetFloatable);
        header = new PaletteHeader(this);
        setTitleBarWidget(header);
        setWidget(new Body);
    }

    PaletteSpec spec;
    PaletteHeader* header;
    QWidget* content = nullptr; // nullptr: chưa dựng

protected:
    void changeEvent(QEvent* e) override
    {
        if (e->type() == QEvent::LanguageChange)
            setWindowTitle(spec.title()); // đầu palette, tab của nhóm, tên cho trình đọc màn hình theo windowTitle
        QDockWidget::changeEvent(e);
    }

    // Esc không widget nào trong palette dùng (ô tìm còn chữ thì xóa chữ trước, popup tự đóng) → về vùng vẽ.
    void keyPressEvent(QKeyEvent* e) override
    {
        if (e->key() == Qt::Key_Escape && e->modifiers() == Qt::NoModifier)
            host_->focusCanvas();
        else
            QDockWidget::keyPressEvent(e);
    }

private:
    PaletteHost* host_;
};

PaletteHost::PaletteHost(QMainWindow* window, settings::Store& store) : QObject(window), window_(window), store_(store)
{
    // GroupedDragging: kéo đầu palette mang cả nhóm tab, kéo tab tách một palette, nhóm nổi được. Animation của Qt
    // dài SH_Widget_Animation_Duration của TrionStyle (--t, về 0 khi giảm chuyển động hoặc Giao diện nhẹ).
    window->setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowTabbedDocks | QMainWindow::GroupedDragging);
    window->setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::North); // tab nhóm ở trên như prototype
    window->installEventFilter(this);

    for (bool backward : {false, true}) {
        auto* a = new QAction(this);
        a->setShortcut(backward ? QKeySequence(Qt::SHIFT | Qt::Key_F6) : QKeySequence(Qt::Key_F6));
        connect(a, &QAction::triggered, this, [this, backward] { focusNextPalette(backward); });
        window->addAction(a);
    }
}

QDockWidget* PaletteHost::registerPalette(PaletteSpec spec)
{
    const bool valid = !spec.id.isEmpty() && !docks_.contains(spec.id) && spec.title && spec.create;
    TRION_CHECK(valid); // mã rỗng, trùng, hoặc thiếu hàm tiêu đề / hàm dựng
    if (!valid)
        return nullptr; // handler của test trả về: bỏ qua
    const QString id = spec.id;
    const Qt::DockWidgetArea area = spec.area;
    const bool visible = spec.visible;
    auto* d = new Dock(this, std::move(spec), window_);
    // Đăng ký sau restoreLayout: về chỗ đã lưu nếu có (trước khi đặt mặc định: addDockWidget xóa chỗ giữ của Qt).
    if (!restored_ || !window_->restoreDockWidget(d)) {
        // Chỗ mặc định đã có palette thì gộp tab với palette đầu tiên ở đó (prototype: mỗi bên một nhóm tab).
        const auto first = std::find_if(order_.cbegin(), order_.cend(), [&](const QString& o) {
            return !docks_[o]->isFloating() && window_->dockWidgetArea(docks_[o]) == area;
        });
        if (first != order_.cend())
            window_->tabifyDockWidget(docks_[*first], d);
        else
            window_->addDockWidget(area, d);
        d->setVisible(visible);
    }
    docks_.insert(id, d);
    order_ << id;

    d->header->pinButton()->setChecked(isPinned(id));
    connect(d, &QDockWidget::visibilityChanged, this, [this, d](bool on) {
        if (on && onScreen(d))
            ensureContent(d);
    });
    connect(d->header->pinButton(), &QAbstractButton::toggled, this, [this, id](bool on) { setPinned(id, on); });
    connect(d->header->menuButton(), &QAbstractButton::clicked, this, [this, d] { showMenu(d); });
    return d;
}

QDockWidget* PaletteHost::dock(const QString& id) const
{
    return find(id);
}

PaletteHeader* PaletteHost::header(const QString& id) const
{
    const Dock* d = find(id);
    return d ? d->header : nullptr;
}

QWidget* PaletteHost::content(const QString& id) const
{
    const Dock* d = find(id);
    return d ? d->content : nullptr;
}

void PaletteHost::ensureContent(Dock* d)
{
    if (d->content)
        return;
    TRION_UI_PROBE_SCOPE("PaletteHost::create");
    if (d->isVisible())
        probe::untilFrame(probe::Op::PaletteOpen, d); // từ input còn chờ khung (nhấp, phím), không thì từ bây giờ
    d->content = d->spec.create();
    d->widget()->layout()->addWidget(d->content);
    if (d->isVisible())
        d->content->show(); // QLayout hiện con mới qua hàng đợi; hiện ngay để khung đầu đã có nội dung
}

void PaletteHost::showPalette(const QString& id)
{
    Dock* d = find(id);
    if (!d)
        return;
    d->show();
    d->raise(); // lên đầu nhóm tab; palette nổi lên trên
    ensureContent(d);
}

void PaletteHost::moveTo(const QString& id, Qt::DockWidgetArea area)
{
    Dock* d = find(id);
    if (!d)
        return;
    if (area == Qt::NoDockWidgetArea) {
        d->setFloating(true);
    } else {
        window_->removeDockWidget(d);
        // Gộp tab với palette đang hiện ở chỗ đó (như "Chuyển sang dock …" của prototype), không có thì dock riêng.
        const auto there = std::find_if(order_.cbegin(), order_.cend(), [&](const QString& o) {
            Dock* x = docks_[o];
            return x != d && !x->isHidden() && !x->isFloating() && window_->dockWidgetArea(x) == area;
        });
        if (there != order_.cend())
            window_->tabifyDockWidget(docks_[*there], d);
        else
            window_->addDockWidget(area, d);
        d->setFloating(false); // sau khi vào layout: Qt chỉ dock lại palette đã có chỗ
    }
    showPalette(id);
}

// Menu "Tùy chọn palette" như prototype (shell.js data-palmenu): Ghim; chuyển sang chỗ khác; Nổi; Mật độ thoáng.
void PaletteHost::showMenu(Dock* d)
{
    auto* menu = new QMenu(d->header);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    const QString id = d->spec.id;
    QAction* pin = menu->addAction(tr("Ghim"), this, [this, id](bool on) { setPinned(id, on); });
    pin->setCheckable(true);
    pin->setChecked(isPinned(id));
    const Qt::DockWidgetArea at = d->isFloating() ? Qt::NoDockWidgetArea : window_->dockWidgetArea(d);
    const auto move = [&](const QString& text, Qt::DockWidgetArea area) {
        if (area != at)
            menu->addAction(text, this, [this, id, area] { moveTo(id, area); });
    };
    move(tr("Chuyển sang dock trái"), Qt::LeftDockWidgetArea);
    move(tr("Chuyển sang dock phải"), Qt::RightDockWidgetArea);
    move(tr("Chuyển sang dock dưới"), Qt::BottomDockWidgetArea);
    move(tr("Nổi"), Qt::NoDockWidgetArea);
    menu->addSeparator();
    auto& theme = theme::ThemeManager::instance();
    QAction* roomy = menu->addAction(tr("Mật độ thoáng"), &theme, [&theme](bool on) {
        theme.setDensity(on ? theme::Density::Comfortable : theme::Density::Compact);
    });
    roomy->setCheckable(true);
    roomy->setChecked(theme.density() == theme::Density::Comfortable);
    const QWidget* b = d->header->menuButton();
    menu->popup(b->mapToGlobal(QPoint(b->width() - menu->sizeHint().width(), b->height()))); // mép phải thẳng nút
}

void PaletteHost::setPinned(const QString& id, bool pinned)
{
    Dock* d = find(id);
    if (!d || isPinned(id) == pinned)
        return;
    if (pinned)
        unpinned_.remove(id);
    else
        unpinned_.insert(id);
    d->header->pinButton()->setChecked(pinned); // toggled gọi lại setPinned: trạng thái đã đổi nên dừng
    emit pinnedChanged(id, pinned);
}

void PaletteHost::saveLayout()
{
    store_.set(kState, window_->saveState(kLayoutVersion));
    QStringList unpinned = unpinned_.values();
    unpinned.sort();
    store_.set(kUnpinned, unpinned);
}

bool PaletteHost::restoreLayout()
{
    restored_ = true;
    const QStringList unpinned = store_.get(kUnpinned);
    for (const QString& id : std::as_const(order_))
        setPinned(id, !unpinned.contains(id));
    for (const QString& id : unpinned)
        unpinned_.insert(id); // cả palette đăng ký sau
    const QByteArray state = store_.get(kState);
    if (state.isEmpty() || !window_->restoreState(state, kLayoutVersion))
        return false;

    // Palette nổi (một palette, hoặc nhóm tab nổi QDockWidgetGroupWindow) về vùng làm việc của màn hình còn có.
    const QList<QScreen*> screens = QGuiApplication::screens();
    QList<QRect> areas;
    for (const QScreen* s : screens)
        areas << s->availableGeometry();
    const qsizetype primary = std::max<qsizetype>(0, screens.indexOf(QGuiApplication::primaryScreen()));
    for (QObject* o : window_->children()) {
        auto* w = qobject_cast<QWidget*>(o);
        if (w && w->isWindow() && (qobject_cast<QDockWidget*>(w) || w->inherits("QDockWidgetGroupWindow")))
            w->setGeometry(fitToScreens(w->geometry(), areas, primary)); // không viền hệ thống: đầu palette tự vẽ
    }
    return true;
}

void PaletteHost::focusNextPalette(bool backward)
{
    QList<Dock*> shown;
    for (const QString& id : std::as_const(order_))
        if (Dock* d = docks_[id]; onScreen(d))
            shown << d;
    const QWidget* f = QApplication::focusWidget();
    qsizetype at = 0; // vòng: 0 là vùng vẽ (hoặc chỗ khác ngoài palette), i + 1 là shown[i]
    for (qsizetype i = 0; i < shown.size(); ++i)
        if (f && (f == shown[i] || shown[i]->isAncestorOf(f)))
            at = i + 1;
    const qsizetype n = shown.size() + 1;
    const qsizetype next = (at + (backward ? n - 1 : 1)) % n;
    if (next == 0)
        focusCanvas();
    else
        focusInto(shown[next - 1]);
}

void PaletteHost::focusInto(Dock* d)
{
    ensureContent(d);
    QWidget* target = d->focusWidget(); // lần trước focus ở đâu trong palette thì về đó
    if (!target || target == d || !target->isVisible() || !target->isEnabled())
        target = firstFocusable(d->content);
    if (!target)
        target = firstFocusable(d->header);
    d->window()->activateWindow(); // palette nổi là cửa sổ riêng
    target->setFocus(Qt::ShortcutFocusReason);
}

void PaletteHost::focusCanvas()
{
    QWidget* canvas = window_->centralWidget();
    if (!canvas)
        return;
    window_->activateWindow();
    canvas->setFocus(Qt::ShortcutFocusReason);
}

bool PaletteHost::eventFilter(QObject* watched, QEvent* e)
{
    if (watched == window_ && e->type() == QEvent::Close)
        saveLayout();
    return QObject::eventFilter(watched, e);
}

QRect PaletteHost::fitToScreens(const QRect& r, const QList<QRect>& workAreas, qsizetype primary)
{
    if (workAreas.isEmpty())
        return r;
    qsizetype at = -1;
    qint64 best = 0;
    for (qsizetype i = 0; i < workAreas.size(); ++i) {
        if (workAreas[i].contains(r.center())) {
            at = i;
            break;
        }
        const QRect common = workAreas[i] & r;
        if (const qint64 a = qint64(common.width()) * common.height(); a > best) {
            best = a;
            at = i;
        }
    }
    const QRect area = workAreas[at >= 0 ? at : std::clamp<qsizetype>(primary, 0, workAreas.size() - 1)];
    QRect out(r.topLeft(), r.size().boundedTo(area.size()));
    out.moveLeft(std::clamp(out.left(), area.left(), area.right() - out.width() + 1));
    out.moveTop(std::clamp(out.top(), area.top(), area.bottom() - out.height() + 1));
    return out;
}

} // namespace trion::ui
