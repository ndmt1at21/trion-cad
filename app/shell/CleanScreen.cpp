#include "app/shell/CleanScreen.h"

#include "app/core/actions/ActionRegistry.h"
#include "app/shell/CommandRouter.h"
#include "app/shell/Workspace.h"
#include "app/widgets/dock/AutoHideStrip.h"

#include <QApplication>
#include <QDockWidget>
#include <QPointer>
#include <QToolBar>

using namespace Qt::StringLiterals;

namespace trion::shell {

namespace {
const QString kState = u"CLEANSCREENSTATE"_s;
}

CleanScreen::CleanScreen(MainWindow& window, actions::Sysvars& sysvars, CommandRouter& router)
    : QObject(&window), window_(window), sysvars_(sysvars)
{
    router.add(u"CLEANSCREENON"_s, [self = QPointer(this)] {
        if (self)
            self->setOn(true);
    });
    router.add(u"CLEANSCREENOFF"_s, [self = QPointer(this)] {
        if (self)
            self->setOn(false);
    });
    connect(&window, &MainWindow::aboutToSave, this, [this] { setOn(false); });
    connect(&window.workspace(), &Workspace::changed, this, [this] { // workspace mới đã tự hiện vùng, palette
        if (on_)
            finish(false);
    });
    sysvars_.set(kState, 0);
}

void CleanScreen::setOn(bool on)
{
    if (on == on_)
        return;
    if (on) {
        QWidget* focus = QApplication::focusWidget();
        hidden_.clear();
        for (const auto r : {MainWindow::Region::TitleBar, MainWindow::Region::Ribbon, MainWindow::Region::FileTabs,
                             MainWindow::Region::LayoutTabs})
            if (window_.isRegionVisible(r)) {
                hidden_ << r;
                window_.setRegionVisible(r, false);
            }
        // Palette ở dock, nổi, trong nhóm tab nổi (palette trong dải đã ẩn sẵn); tab đang chọn: dock đang lộ ra.
        QMainWindow* docks = window_.dockHost();
        QList<QWidget*> hide;
        for (QDockWidget* d : docks->findChildren<QDockWidget*>())
            if (!d->isHidden()) {
                hide << d;
                if (!d->isFloating() && !d->visibleRegion().isEmpty())
                    raised_ << d;
            }
        for (QToolBar* t : docks->findChildren<QToolBar*>(Qt::FindDirectChildrenOnly)) // dải tự ẩn, toolbar
            if (!t->isHidden()) {
                if (auto* s = qobject_cast<ui::AutoHideStrip*>(t))
                    s->collapse();
                hide << t;
            }
        for (QObject* o : docks->children()) // nhóm tab nổi (QDockWidgetGroupWindow)
            if (auto* w = qobject_cast<QWidget*>(o); w && w->isWindow() && !w->isHidden() && !hide.contains(w))
                hide << w;
        for (QWidget* w : std::as_const(hide)) {
            shown_ << w;
            w->hide();
        }
        if (focus && !focus->isVisible()) // focus ở phần vừa ẩn: về vùng vẽ
            window_.region(MainWindow::Region::Canvas)->setFocus();
    } else {
        for (const MainWindow::Region r : std::as_const(hidden_))
            window_.setRegionVisible(r, true);
        for (const QPointer<QWidget>& w : std::as_const(shown_))
            if (w)
                w->show();
        for (const QPointer<QDockWidget>& d : std::as_const(raised_))
            if (d)
                d->raise(); // tab đang chọn của nhóm như trước khi bật
    }
    finish(on);
}

void CleanScreen::finish(bool on)
{
    on_ = on;
    if (!on) {
        hidden_.clear();
        shown_.clear();
        raised_.clear();
    }
    sysvars_.set(kState, on ? 1 : 0);
    emit toggled(on);
}

} // namespace trion::shell
