#include "app/shell/Workspace.h"

#include "app/shell/MainWindow.h"
#include "app/widgets/dock/AutoHideStrip.h"
#include "app/widgets/dock/PaletteHost.h"

#include <QDockWidget>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaEnum>
#include <QSet>

using namespace Qt::StringLiterals;

namespace trion::shell {

namespace {

const settings::Setting<QString> kChosen{"ui/window/workspace", QStringLiteral("2d")};
const QString kViewer = u"viewer"_s;

struct Palette {
    QString id;
    Qt::DockWidgetArea area;
    bool pinned;
};
struct Description {
    QList<MainWindow::Region> regions;
    QList<Palette> palettes;
};

// Mô tả nhúng qrc (dữ liệu của ta; test kiểm từng mô tả đọc đủ). Đọc mỗi lần áp: vài micro giây.
Description describe(const QString& id)
{
    Description d;
    QFile f(u":/trion/shell/workspaces/%1.json"_s.arg(id));
    if (!f.open(QIODevice::ReadOnly))
        return d;
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    const QMetaEnum regions = QMetaEnum::fromType<MainWindow::Region>();
    for (const QJsonValue& v : o[u"regions"_s].toArray()) {
        bool ok = false;
        const int r = regions.keyToValue(v.toString().toLatin1().constData(), &ok);
        if (ok)
            d.regions << MainWindow::Region(r);
    }
    for (const QJsonValue& v : o[u"palettes"_s].toArray()) {
        const QJsonObject p = v.toObject();
        const QString side = p[u"area"_s].toString();
        d.palettes.push_back({p[u"id"_s].toString(),
                              side == u"left"     ? Qt::LeftDockWidgetArea
                              : side == u"bottom" ? Qt::BottomDockWidgetArea
                                                  : Qt::RightDockWidgetArea,
                              p[u"pinned"_s].toBool(true)});
    }
    return d;
}

} // namespace

Workspace::Workspace(MainWindow& window, settings::Store& store) : QObject(&window), window_(window), store_(store) {}

QStringList Workspace::ids()
{
    return {u"2d"_s, kViewer};
}

QString Workspace::name(const QString& id)
{
    if (id == u"2d")
        return tr("Soạn thảo 2D");
    if (id == kViewer)
        return tr("Viewer");
    return {};
}

QString Workspace::chosen() const
{
    const QString id = store_.get(kChosen);
    return ids().contains(id) ? id : kChosen.def;
}

QString Workspace::current() const
{
    return viewer_ ? kViewer : chosen();
}

void Workspace::apply(const QString& id)
{
    if (!ids().contains(id))
        return;
    const QString before = current();
    store_.set(kChosen, id);
    applyLayout();
    if (current() != before)
        emit changed(current());
}

void Workspace::setProfile(edition::UiProfile profile)
{
    const QString before = current();
    viewer_ = profile == edition::UiProfile::Viewer;
    if (restored_ && current() != before) {
        applyLayout();
        emit changed(current());
    }
}

void Workspace::restore(bool keepSaved)
{
    restored_ = true;
    if (!keepSaved)
        applyLayout();
}

void Workspace::applyLayout()
{
    const Description d = describe(current());
    for (int i = 0; i < MainWindow::kRegionCount; ++i) {
        const auto r = MainWindow::Region(i);
        window_.setRegionVisible(r, r == MainWindow::Region::Canvas || d.regions.contains(r));
    }
    // Đóng hết rồi mở theo thứ tự của mô tả: nhóm tab mỗi cạnh theo đúng thứ tự đó.
    ui::PaletteHost& host = window_.palettes();
    const bool wide = window_.screenClass() == ui::ScreenClass::Wide;
    for (const QString& id : host.ids()) {
        host.dock(id)->close();
        host.setPinned(id, wide); // palette đóng: chỉ đổi trạng thái (mở sau thì theo quy tắc màn hình)
    }
    for (const Palette& p : d.palettes)
        host.moveTo(p.id, p.area); // ghim, dock ở cạnh, gộp tab, hiện
    QSet<Qt::DockWidgetArea> raised;
    for (const Palette& p : d.palettes)
        if (!raised.contains(p.area)) { // palette đầu mỗi cạnh là tab đang mở
            raised.insert(p.area);
            host.showPalette(p.id);
        }
    for (const Palette& p : d.palettes)
        if (!wide || !p.pinned)
            host.setPinned(p.id, false); // vào dải tự ẩn ở cạnh của nó
    defaultSignature_ = signature();
}

bool Workspace::isDefaultLayout() const
{
    return !defaultSignature_.isEmpty() && signature() == defaultSignature_;
}

QString Workspace::signature() const
{
    const ui::PaletteHost& host = window_.palettes();
    QStringList s;
    for (const QString& id : host.ids()) {
        QDockWidget* d = host.dock(id);
        const Qt::DockWidgetArea area = window_.dockHost()->dockWidgetArea(d);
        const ui::AutoHideStrip* strip = host.strip(area);
        s << u"%1 %2 %3 %4 %5 %6"_s.arg(id)
                 .arg(int(area))
                 .arg(d->isHidden())
                 .arg(host.isPinned(id))
                 .arg(d->isFloating())
                 .arg(strip && strip->contains(d));
    }
    return s.join(u';');
}

} // namespace trion::shell
