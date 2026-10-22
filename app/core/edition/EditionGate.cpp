#include "app/core/edition/EditionGate.h"

#include "app/core/actions/ActionRegistry.h"

#include <QAction>
#include <QDesktopServices>
#include <QUrlQuery>

#include <utility>

using namespace Qt::StringLiterals;

namespace trion::edition {

namespace {

EditionGate* g_instance = nullptr;

constexpr unsigned bit(Edition e)
{
    return 1u << unsigned(e);
}

// ponytail: bảng 07.1 §2 viết tay (tập phiên bản có mỗi khả năng); PLF-002 sinh bảng này. Dùng thử = Pro; Giáo dục =
// Pro (kèm huy hiệu ở thanh tiêu đề, UIA-002); floating, cổng quản trị, cài hàng loạt chỉ Doanh nghiệp.
constexpr unsigned editions(Capability c)
{
    constexpr unsigned all = (1u << kEditionCount) - 1;
    constexpr unsigned fromViewerPro = all & ~bit(Edition::Viewer);
    constexpr unsigned fromPro =
        bit(Edition::Pro) | bit(Edition::Trial) | bit(Edition::Education) | bit(Edition::Enterprise);
    switch (c) {
    case Capability::View:
    case Capability::Measure:
    case Capability::LayerToggle:
    case Capability::Print:
    case Capability::FontView:
        return all;
    case Capability::FontConvert:
    case Capability::Markup:
    case Capability::BatchTools:
        return fromViewerPro;
    case Capability::Edit:
    case Capability::Scripting:
        return fromPro;
    case Capability::Fleet:
        return bit(Edition::Enterprise);
    }
    return 0;
}

// Cờ phiên bản của phần tử UI → dòng đại diện của bảng. Các dòng Viewer Pro (Bác sĩ Font chuyển đổi, markup, công cụ
// hàng loạt) cùng cột, các dòng Pro (sửa, LISP) cùng cột (edition_test kiểm), nên khóa không phụ thuộc dòng chọn.
Capability capability(ui::Edition need)
{
    switch (need) {
    case ui::Edition::ViewerPro:
        return Capability::BatchTools;
    case ui::Edition::Pro:
        return Capability::Edit;
    case ui::Edition::None:
        break;
    }
    return Capability::View;
}

} // namespace

// ---- UpgradeRouter

UpgradeRouter::UpgradeRouter(QObject* parent) : QObject(parent)
{
    setUrlOpener({});
}

void UpgradeRouter::setHandler(Handler handler)
{
    handler_ = std::move(handler);
}

void UpgradeRouter::setUrlOpener(UrlOpener opener)
{
    opener_ = opener ? std::move(opener) : UrlOpener([](const QUrl& u) { return QDesktopServices::openUrl(u); });
}

UpgradeRouter::Outcome UpgradeRouter::request(const UpgradeRequest& r)
{
    if (!shown_.contains(r.feature) && open(r))
        return Outcome::Page;
    emit noticeRequested(r);
    return Outcome::Notice;
}

bool UpgradeRouter::open(const UpgradeRequest& r)
{
    if (handler_)
        handler_(r);
    else if (!opener_(url(r))) {
        qWarning("UpgradeRouter: không mở được %s", qUtf8Printable(url(r).toString()));
        return false;
    }
    shown_.insert(r.feature);
    return true;
}

QUrl UpgradeRouter::url(const UpgradeRequest& r)
{
    QUrl u(u"https://trioncad.vn/thanh-toan"_s);
    u.setQuery(QUrlQuery{{u"feature"_s, r.feature}});
    u.setFragment(r.need == ui::Edition::ViewerPro ? u"vp"_s : u"pro"_s);
    return u;
}

QString UpgradeRouter::noticeText(const UpgradeRequest& r)
{
    return tr("%1 có trong TrionCAD %2.").arg(r.name, ui::RichTooltip::editionName(r.need));
}

// ---- EditionGate

EditionGate::EditionGate(QObject* parent) : QObject(parent)
{
    g_instance = this;
}

EditionGate::~EditionGate()
{
    if (registry_)
        registry_->setGate({});
    if (g_instance == this)
        g_instance = nullptr;
}

EditionGate* EditionGate::instance()
{
    return g_instance;
}

void EditionGate::setEdition(Edition e)
{
    if (e == edition_)
        return;
    const UiProfile before = profile();
    edition_ = e;
    applyLocks();
    emit editionChanged(e);
    if (profile() != before)
        emit profileChanged(profile());
}

void EditionGate::setTrialDays(int days)
{
    if (days == trialDays_)
        return;
    trialDays_ = days;
    emit statusChanged();
}

void EditionGate::setOfflineDays(int days)
{
    if (days == offlineDays_)
        return;
    offlineDays_ = days;
    emit statusChanged();
}

bool EditionGate::has(Edition e, Capability c)
{
    return (editions(c) & bit(e)) != 0;
}

Edition EditionGate::minimumEdition(Capability c)
{
    for (Edition e : {Edition::Viewer, Edition::ViewerPro, Edition::Pro})
        if (has(e, c))
            return e;
    return Edition::Enterprise;
}

UiProfile EditionGate::profile(Edition e)
{
    return has(e, Capability::Edit) ? UiProfile::Full : UiProfile::Viewer;
}

bool EditionGate::locked(ui::Edition need) const
{
    return need != ui::Edition::None && !has(capability(need));
}

void EditionGate::bind(actions::ActionRegistry& registry)
{
    if (registry_ && registry_ != &registry)
        registry_->setGate({});
    registry_ = &registry;
    registry.setGate([this](const QString& id) { return allow(id); });
    applyLocks();
}

bool EditionGate::allow(const QString& id)
{
    const ui::Edition need = registry_->need(id);
    if (!locked(need))
        return true;
    upgrade_.request({id, need, ui::RichTooltip::name(registry_->action(id))});
    return false;
}

void EditionGate::applyLocks()
{
    if (!registry_)
        return;
    for (QAction* a : registry_->actions()) {
        const ui::Edition need = registry_->need(a->objectName());
        const ui::Edition shown = locked(need) ? need : ui::Edition::None;
        if (ui::TipInfo info = ui::RichTooltip::info(a); info.need != shown) {
            info.need = shown;
            ui::RichTooltip::attach(a, info); // Menu đo, vẽ lại mục; tooltip đang hiện vẽ lại
        }
    }
}

QString EditionGate::availableIn(ui::Edition need)
{
    return need == ui::Edition::None ? QString() : tr("Có trong TrionCAD %1").arg(ui::RichTooltip::editionName(need));
}

EditionGate::CommandNotice EditionGate::commandNotice(const QString& command, ui::Edition need)
{
    return {tr("Lệnh %1 có trong TrionCAD %2.").arg(command, ui::RichTooltip::editionName(need)),
            tr("Dùng thử 30 ngày"), tr("Tìm hiểu")};
}

} // namespace trion::edition
