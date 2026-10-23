#include "app/dialogs/plot/PlotSettings.h"

#include <QCoreApplication>
#include <QDir>

#ifdef TRION_PLOT_PRINTSUPPORT
#include <QPageSize>
#include <QPrinterInfo>
#endif

#include <algorithm>
#include <cmath>
#include <mutex>

using namespace Qt::StringLiterals;

namespace trion::plot {

namespace {

constexpr double kSameSize = 0.5; // mm: khổ cùng cỡ khi đổi thiết bị (tra khổ theo kích thước, PLT-001)

QList<Paper> isoPapers()
{
    // Cạnh dài trước như tên khổ của AutoCAD; khổ dài theo ISO 5457 (A3×3).
    return {
        {u"ISO_full_bleed_A0_(1189.00_x_841.00_MM)"_s, u"ISO A0"_s, 1189, 841},
        {u"ISO_full_bleed_A1_(841.00_x_594.00_MM)"_s, u"ISO A1"_s, 841, 594},
        {u"ISO_full_bleed_A2_(594.00_x_420.00_MM)"_s, u"ISO A2"_s, 594, 420},
        {u"ISO_full_bleed_A3_(420.00_x_297.00_MM)"_s, u"ISO A3"_s, 420, 297},
        {u"ISO_full_bleed_A4_(297.00_x_210.00_MM)"_s, u"ISO A4"_s, 297, 210},
        {u"ISO_full_bleed_A3x3_(891.00_x_420.00_MM)"_s,
         QString::fromUtf8(QT_TRANSLATE_NOOP("trion::ui::PlotDialog", "A3 kéo dài")), 891, 420, true},
    };
}

const QString& defaultPaper()
{
    static const QString a3 = u"ISO_full_bleed_A3_(420.00_x_297.00_MM)"_s;
    return a3;
}

const Device* findDevice(const QList<Device>& devices, const QString& name)
{
    const auto it = std::find_if(devices.begin(), devices.end(), [&](const Device& d) { return d.name == name; });
    return it == devices.end() ? nullptr : &*it;
}

const Paper* anyPaper(const QList<Device>& devices, const QString& name)
{
    for (const Device& d : devices)
        if (const Paper* p = findPaper(d, name))
            return p;
    return nullptr;
}

bool sameSize(const Paper& a, const Paper& b)
{
    const auto near = [](double x, double y) { return std::abs(x - y) <= kSameSize; };
    return (near(a.width, b.width) && near(a.height, b.height)) || (near(a.width, b.height) && near(a.height, b.width));
}

// Nguồn và cache máy in hệ thống: một khóa cho cả hai; lần quét giữ khóa riêng để các lần quét chạy lần lượt.
std::mutex& stateLock()
{
    static std::mutex m;
    return m;
}
std::mutex& scanLock()
{
    static std::mutex m;
    return m;
}
PrinterSource& source()
{
    static PrinterSource s;
    return s;
}
QList<Device>& cache()
{
    static QList<Device> c;
    return c;
}

QList<Device> qtPrinters()
{
    QList<Device> out;
#ifdef TRION_PLOT_PRINTSUPPORT
    for (const QPrinterInfo& info : QPrinterInfo::availablePrinters()) {
        Device d;
        d.name = info.printerName();
        d.location = info.location();
        d.model = info.makeAndModel();
        for (const QPageSize& size : info.supportedPageSizes()) {
            const QSizeF mm = size.size(QPageSize::Millimeter);
            d.papers.append({size.key(), size.name(), mm.width(), mm.height()});
        }
        d.defaultPaper = info.defaultPageSize().key();
        out.append(d);
    }
#endif
    return out;
}

} // namespace

const QList<Device>& builtinDevices()
{
    static const QList<Device> list = [] {
        Device pdf{kPdfDevice.toString(), DeviceKind::Pdf, {}, {}, isoPapers(), defaultPaper()};
        Device png{kPngDevice.toString(), DeviceKind::Png, {}, {}, isoPapers(), defaultPaper()};
        return QList<Device>{pdf, png};
    }();
    return list;
}

const Paper* findPaper(const Device& device, const QString& name)
{
    const auto it = std::find_if(device.papers.begin(), device.papers.end(), [&](const Paper& p) { return p.name == name; });
    return it == device.papers.end() ? nullptr : &*it;
}

std::span<const int> standardScales()
{
    static constexpr int kScales[] = {1, 2, 5, 10, 20, 25, 50, 75, 100, 125, 200, 250, 500, 1000};
    return kScales;
}

QString defaultFile(const PlotContext& context, const PageSetup& setup)
{
    const QString ext = setup.device == kPdfDevice ? u"pdf"_s : setup.device == kPngDevice ? u"png"_s : u"plt"_s;
    const QString sheet = context.isLayout() ? context.layout : u"Model"_s;
    return QDir(context.outputDir).filePath(u"%1-%2.%3"_s.arg(context.document, sheet, ext));
}

void normalize(PlotSettings& s, const PlotContext& context, const QList<Device>& devices)
{
    PageSetup& p = s.setup;
    if (const Device* device = findDevice(devices, p.device)) {
        if (!findPaper(*device, p.paper)) {
            const Paper* old = anyPaper(devices, p.paper);
            const auto same = std::find_if(device->papers.begin(), device->papers.end(),
                                           [&](const Paper& q) { return old && sameSize(*old, q); });
            if (same != device->papers.end())
                p.paper = same->name;
            else if (findPaper(*device, device->defaultPaper))
                p.paper = device->defaultPaper;
            else if (!device->papers.isEmpty())
                p.paper = device->papers.first().name;
        }
        if (device->isFile())
            s.toFile = true;
    }
    s.copies = s.toFile ? 1 : std::clamp(s.copies, 1, kMaxCopies);

    const bool layout = context.isLayout();
    PlotArea& a = p.area;
    if ((a.kind == AreaKind::Layout && !layout) || (a.kind == AreaKind::Limits && layout)
        || (a.kind == AreaKind::View && context.views.isEmpty()))
        a.kind = layout ? AreaKind::Layout : AreaKind::Display;
    if (a.kind == AreaKind::View && !context.views.contains(a.view))
        a.view = context.views.first();
    a.window = a.window.normalized();
    if (a.kind == AreaKind::Layout)
        p.offset.centered = false; // AutoCAD: vùng in Layout không canh giữa

    if (!(p.scale.mm > 0) || !std::isfinite(p.scale.mm))
        p.scale.mm = 1;
    if (!(p.scale.units > 0) || !std::isfinite(p.scale.units))
        p.scale.units = 1;
    if (p.flags.plotStyles)
        p.flags.lineweights = true; // in theo kiểu in thì kiểu in quyết định độ dày nét
    if (!layout) {
        p.flags.paperSpaceLast = false;
        p.flags.hidePaperSpace = false;
    }
    p.shade.dpi = std::clamp(p.shade.dpi, kMinDpi, kMaxDpi);
    s.file = s.toFile ? defaultFile(context, p) : QString();
}

PlotSettings initialSettings(const PlotContext& context)
{
    PlotSettings s;
    if (context.layoutSetup) {
        s.setup = *context.layoutSetup;
    } else {
        s.setup.device = kPdfDevice.toString();
        s.setup.paper = defaultPaper();
        s.setup.area.kind = context.isLayout() ? AreaKind::Layout : AreaKind::Display;
        s.setup.scale.fit = !context.isLayout();
    }
    normalize(s, context, builtinDevices() + cachedPrinters());
    return s;
}

void setPrinterSource(PrinterSource s)
{
    const std::lock_guard g(stateLock());
    source() = std::move(s);
}

QList<Device> scanPrinters()
{
    const std::lock_guard scan(scanLock());
    PrinterSource src;
    {
        const std::lock_guard g(stateLock());
        src = source();
    }
    QList<Device> list = src ? src() : qtPrinters();
    const std::lock_guard g(stateLock());
    cache() = list;
    return list;
}

QList<Device> cachedPrinters()
{
    const std::lock_guard g(stateLock());
    return cache();
}

} // namespace trion::plot
