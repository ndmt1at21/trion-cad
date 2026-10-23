#include "app/dialogs/plot/PlotSettingsModel.h"

#include <algorithm>
#include <cmath>

namespace trion::ui {

using plot::AreaKind;

PlotSettingsModel::PlotSettingsModel(plot::PlotContext context, QObject* parent)
    : QObject(parent), ctx_(std::move(context)), devices_(plot::builtinDevices() + plot::cachedPrinters()),
      s_(plot::initialSettings(ctx_))
{
    if (ctx_.layoutSetup && !ctx_.layoutSetup->name.isEmpty()
        && std::any_of(ctx_.namedSetups.begin(), ctx_.namedSetups.end(),
                       [&](const plot::PageSetup& p) { return p.name == ctx_.layoutSetup->name; }))
        setup_ = ctx_.layoutSetup->name;
    plot::normalize(s_, ctx_, devices_);
}

template <class Edit>
void PlotSettingsModel::edit(Edit apply)
{
    const plot::PlotSettings old = s_;
    apply(s_);
    plot::normalize(s_, ctx_, devices_);
    if (s_ != old)
        emit changed();
}

const plot::Device* PlotSettingsModel::device() const
{
    const auto it = std::find_if(devices_.begin(), devices_.end(),
                                 [&](const plot::Device& d) { return d.name == s_.setup.device; });
    return it == devices_.end() ? nullptr : &*it;
}

QList<plot::Paper> PlotSettingsModel::papers() const
{
    if (const plot::Device* d = device())
        return d->papers;
    for (const plot::Device& d : devices_) // máy in vắng: khổ đang đặt, lấy cỡ từ thiết bị khác nếu biết
        if (const plot::Paper* p = plot::findPaper(d, s_.setup.paper))
            return {*p};
    return {plot::Paper{s_.setup.paper, s_.setup.paper}};
}

QList<AreaKind> PlotSettingsModel::areas() const
{
    QList<AreaKind> out;
    if (ctx_.isLayout())
        out << AreaKind::Layout;
    out << AreaKind::Extents << AreaKind::Display;
    if (!ctx_.isLayout())
        out << AreaKind::Limits;
    if (!ctx_.views.isEmpty())
        out << AreaKind::View;
    out << AreaKind::Window;
    return out;
}

void PlotSettingsModel::setSystemPrinters(const QList<plot::Device>& printers)
{
    QList<plot::Device> list = plot::builtinDevices();
    for (const plot::Device& p : printers)
        if (p.kind == plot::DeviceKind::System)
            list << p;
    const bool first = !printersLoaded_;
    printersLoaded_ = true;
    const plot::PlotSettings old = s_;
    const bool same = list.size() == devices_.size()
                      && std::equal(list.begin(), list.end(), devices_.begin(), [](const auto& a, const auto& b) {
                             return a.name == b.name && a.papers.size() == b.papers.size();
                         });
    devices_ = std::move(list);
    plot::normalize(s_, ctx_, devices_);
    if (first || !same || s_ != old)
        emit changed();
}

void PlotSettingsModel::selectSetup(const QString& choice)
{
    std::optional<plot::PageSetup> from;
    if (choice == kPreviousSetup) {
        from = ctx_.previous;
    } else if (!choice.isEmpty()) {
        for (const plot::PageSetup& p : ctx_.namedSetups)
            if (p.name == choice)
                from = p;
    }
    if (!choice.isEmpty() && !from)
        return; // không có thiết lập đó
    const bool picked = setup_ != choice;
    setup_ = choice;
    const plot::PlotSettings old = s_;
    if (from)
        s_.setup = *from; // chép mọi trường của thiết lập
    plot::normalize(s_, ctx_, devices_);
    if (picked || s_ != old)
        emit changed();
}

void PlotSettingsModel::setDevice(const QString& name)
{
    edit([&](plot::PlotSettings& s) {
        if (s.setup.device == name)
            return;
        const bool wasFile = s.toFile && toFileForced();
        s.setup.device = name;
        const auto it = std::find_if(devices_.begin(), devices_.end(), [&](const plot::Device& d) { return d.name == name; });
        if (wasFile && it != devices_.end() && !it->isFile())
            s.toFile = false; // từ thiết bị tệp sang máy in: in ra máy in như AutoCAD
    });
}

void PlotSettingsModel::setPaper(const QString& name)
{
    const plot::Device* d = device();
    if (d && !plot::findPaper(*d, name))
        return;
    edit([&](plot::PlotSettings& s) { s.setup.paper = name; });
}

void PlotSettingsModel::setCopies(int copies)
{
    edit([&](plot::PlotSettings& s) { s.copies = std::clamp(copies, 1, plot::kMaxCopies); });
}

void PlotSettingsModel::setToFile(bool on)
{
    if (toFileForced())
        return;
    edit([&](plot::PlotSettings& s) { s.toFile = on; });
}

void PlotSettingsModel::setArea(AreaKind kind)
{
    if (!areas().contains(kind))
        return;
    edit([&](plot::PlotSettings& s) { s.setup.area.kind = kind; });
}

void PlotSettingsModel::setView(const QString& name)
{
    if (!ctx_.views.contains(name))
        return;
    edit([&](plot::PlotSettings& s) { s.setup.area.view = name; });
}

void PlotSettingsModel::setWindow(const QRectF& window)
{
    edit([&](plot::PlotSettings& s) { s.setup.area.window = window.normalized(); });
}

void PlotSettingsModel::setCentered(bool on)
{
    edit([&](plot::PlotSettings& s) { s.setup.offset.centered = on; });
}

void PlotSettingsModel::setOffset(double x, double y)
{
    if (!std::isfinite(x) || !std::isfinite(y))
        return;
    edit([&](plot::PlotSettings& s) {
        s.setup.offset.x = x;
        s.setup.offset.y = y;
    });
}

void PlotSettingsModel::setFit(bool on)
{
    edit([&](plot::PlotSettings& s) { s.setup.scale.fit = on; });
}

void PlotSettingsModel::setStandardScale(int index)
{
    const auto scales = plot::standardScales();
    if (index < 0 || index >= int(scales.size()))
        return;
    edit([&](plot::PlotSettings& s) {
        s.setup.scale.fit = false;
        s.setup.scale.mm = 1;
        s.setup.scale.units = scales[std::size_t(index)];
    });
}

void PlotSettingsModel::setCustomScale(double mm, double units)
{
    if (!(mm > 0) || !(units > 0) || !std::isfinite(mm) || !std::isfinite(units))
        return;
    edit([&](plot::PlotSettings& s) {
        s.setup.scale.fit = false;
        s.setup.scale.mm = mm;
        s.setup.scale.units = units;
    });
}

void PlotSettingsModel::setScaleLineweights(bool on)
{
    edit([&](plot::PlotSettings& s) { s.setup.scale.scaleLineweights = on; });
}

void PlotSettingsModel::setPlotStyle(const QString& name)
{
    edit([&](plot::PlotSettings& s) { s.setup.plotStyle = name; });
}

void PlotSettingsModel::setShowPlotStyles(bool on)
{
    edit([&](plot::PlotSettings& s) { s.setup.flags.showPlotStyles = on; });
}

void PlotSettingsModel::setShadeMode(plot::ShadeMode mode)
{
    edit([&](plot::PlotSettings& s) { s.setup.shade.mode = mode; });
}

void PlotSettingsModel::setShadeQuality(plot::ShadeQuality quality)
{
    edit([&](plot::PlotSettings& s) { s.setup.shade.quality = quality; });
}

void PlotSettingsModel::setShadeDpi(int dpi)
{
    edit([&](plot::PlotSettings& s) { s.setup.shade.dpi = dpi; });
}

void PlotSettingsModel::setOption(Option option, bool on)
{
    if (!optionEnabled(option))
        return;
    edit([&](plot::PlotSettings& s) {
        plot::PlotFlags& f = s.setup.flags;
        switch (option) {
        case Option::Background: s.background = on; break;
        case Option::Lineweights: f.lineweights = on; break;
        case Option::Transparency: f.transparency = on; break;
        case Option::PlotStyles: f.plotStyles = on; break;
        case Option::PaperSpaceLast: f.paperSpaceLast = on; break;
        case Option::HidePaperSpace: f.hidePaperSpace = on; break;
        case Option::Stamp: s.stamp = on; break;
        case Option::SaveToLayout: s.saveToLayout = on; break;
        }
    });
}

void PlotSettingsModel::setOrientation(plot::Orientation orientation)
{
    edit([&](plot::PlotSettings& s) { s.setup.orientation = orientation; });
}

void PlotSettingsModel::setUpsideDown(bool on)
{
    edit([&](plot::PlotSettings& s) { s.setup.upsideDown = on; });
}

bool PlotSettingsModel::option(Option option) const
{
    const plot::PlotFlags& f = s_.setup.flags;
    switch (option) {
    case Option::Background: return s_.background;
    case Option::Lineweights: return f.lineweights;
    case Option::Transparency: return f.transparency;
    case Option::PlotStyles: return f.plotStyles;
    case Option::PaperSpaceLast: return f.paperSpaceLast;
    case Option::HidePaperSpace: return f.hidePaperSpace;
    case Option::Stamp: return s_.stamp;
    case Option::SaveToLayout: return s_.saveToLayout;
    }
    return false;
}

bool PlotSettingsModel::optionEnabled(Option option) const
{
    switch (option) {
    case Option::Lineweights: return !s_.setup.flags.plotStyles;
    case Option::PaperSpaceLast:
    case Option::HidePaperSpace: return ctx_.isLayout();
    default: return true;
    }
}

bool PlotSettingsModel::toFileForced() const
{
    const plot::Device* d = device();
    return d && d->isFile();
}

int PlotSettingsModel::scaleIndex() const
{
    const plot::PlotScale& sc = s_.setup.scale;
    if (sc.mm != 1)
        return -1;
    const auto scales = plot::standardScales();
    const auto it = std::find(scales.begin(), scales.end(), sc.units);
    return it == scales.end() ? -1 : int(it - scales.begin());
}

bool PlotSettingsModel::canPlot() const
{
    const plot::PlotArea& a = s_.setup.area;
    return a.kind != AreaKind::Window || !a.window.isEmpty();
}

} // namespace trion::ui
