#include "app/dialogs/plot/PlotCommand.h"

#include <algorithm>
#include <initializer_list>

using namespace Qt::StringLiterals;

namespace trion::plot {

namespace {

class Script {
public:
    explicit Script(const QStringList& answers) : answers_(answers) {}

    // Câu trả lời kế tiếp (đã bỏ khoảng trắng hai đầu); hết câu: nullopt.
    std::optional<QString> next()
    {
        if (i_ >= answers_.size())
            return std::nullopt;
        return answers_.at(i_++).trimmed();
    }

    // Từ khóa: chỉ số trong list; rỗng: def; không khớp: -1. Khớp đúng trước, rồi tiền tố (viết tắt).
    static int keyword(const QString& a, std::initializer_list<QStringView> list, int def)
    {
        if (a.isEmpty())
            return def;
        int i = 0;
        for (const QStringView k : list) {
            if (k.compare(a, Qt::CaseInsensitive) == 0)
                return i;
            ++i;
        }
        i = 0;
        for (const QStringView k : list) {
            if (k.startsWith(a, Qt::CaseInsensitive))
                return i;
            ++i;
        }
        return -1;
    }

private:
    const QStringList& answers_;
    qsizetype i_ = 0;
};

Error fail(const char* prompt, const QString& answer = {})
{
    std::string m = "-PLOT: ";
    m += prompt;
    if (!answer.isEmpty())
        m += ": \"" + answer.toStdString() + "\"";
    return Error{"plot", "script", m};
}

std::optional<QPointF> point(const QString& a)
{
    const QStringList xy = a.split(u',');
    bool okX = false, okY = false;
    const double x = xy.size() == 2 ? xy[0].trimmed().toDouble(&okX) : 0;
    const double y = xy.size() == 2 ? xy[1].trimmed().toDouble(&okY) : 0;
    if (!okX || !okY)
        return std::nullopt;
    return QPointF(x, y);
}

} // namespace

Expected<PlotSettings> parsePlotCommand(const QStringList& answers, const PlotContext& context,
                                        const QList<Device>& devices)
{
    PlotSettings s = initialSettings(context);
    PageSetup& p = s.setup;
    Script in(answers);
    std::optional<QString> a;
    const auto yesNo = [&](const char* prompt, bool& value) -> std::optional<Error> {
        if (!(a = in.next()))
            return fail(prompt);
        const int k = Script::keyword(*a, {u"Yes", u"No"}, value ? 0 : 1);
        if (k < 0)
            return fail(prompt, *a);
        value = k == 0;
        return std::nullopt;
    };
#define TRION_TRY(expr)                                                                                                \
    if (auto e = (expr))                                                                                               \
        return *e;

    bool detailed = false;
    TRION_TRY(yesNo("Detailed plot configuration?", detailed));
    if (!detailed)
        return fail("Detailed plot configuration? (only Yes)");

    if (!(a = in.next()))
        return fail("layout name");
    const QString sheet = context.isLayout() ? context.layout : u"Model"_s;
    if (!a->isEmpty() && a->compare(sheet, Qt::CaseInsensitive) != 0)
        return fail("layout name", *a);

    if (!(a = in.next()))
        return fail("output device name");
    if (!a->isEmpty()) {
        const auto d = std::find_if(devices.begin(), devices.end(),
                                    [&](const Device& x) { return x.name.compare(*a, Qt::CaseInsensitive) == 0; });
        if (d == devices.end())
            return fail("output device name", *a);
        p.device = d->name;
        normalize(s, context, devices); // khổ mặc định của thiết bị mới
    }
    const auto device = std::find_if(devices.begin(), devices.end(), [&](const Device& x) { return x.name == p.device; });

    if (!(a = in.next()))
        return fail("paper size");
    if (!a->isEmpty()) {
        const Paper* paper = nullptr;
        if (device != devices.end())
            for (const Paper& q : device->papers)
                if (q.name.compare(*a, Qt::CaseInsensitive) == 0 || q.title.compare(*a, Qt::CaseInsensitive) == 0)
                    paper = &q;
        if (!paper)
            return fail("paper size", *a);
        p.paper = paper->name;
    }

    if (!(a = in.next()))
        return fail("paper units");
    if (const int k = Script::keyword(*a, {u"Inches", u"Millimeters"}, p.units == PaperUnits::Inches ? 0 : 1); k >= 0)
        p.units = k == 0 ? PaperUnits::Inches : PaperUnits::Millimeters;
    else
        return fail("paper units", *a);

    if (!(a = in.next()))
        return fail("drawing orientation");
    if (const int k = Script::keyword(*a, {u"Portrait", u"Landscape"}, p.orientation == Orientation::Portrait ? 0 : 1);
        k >= 0)
        p.orientation = k == 0 ? Orientation::Portrait : Orientation::Landscape;
    else
        return fail("drawing orientation", *a);
    TRION_TRY(yesNo("Plot upside down?", p.upsideDown));

    if (!(a = in.next()))
        return fail("plot area");
    const AreaKind third = context.isLayout() ? AreaKind::Layout : AreaKind::Limits;
    const AreaKind kinds[] = {AreaKind::Display, AreaKind::Extents, third, AreaKind::View, AreaKind::Window};
    const int current = int(std::find(std::begin(kinds), std::end(kinds), p.area.kind) - std::begin(kinds));
    const int area = context.isLayout() ? Script::keyword(*a, {u"Display", u"Extents", u"Layout", u"View", u"Window"}, current)
                                        : Script::keyword(*a, {u"Display", u"Extents", u"Limits", u"View", u"Window"}, current);
    if (area < 0 || area > 4)
        return fail("plot area", *a);
    p.area.kind = kinds[area];
    if (p.area.kind == AreaKind::View) {
        if (!(a = in.next()) || !context.views.contains(*a))
            return fail("view name", a.value_or(QString()));
        p.area.view = *a;
    } else if (p.area.kind == AreaKind::Window) {
        const auto lo = (a = in.next()) ? point(*a) : std::nullopt;
        const auto hi = lo && (a = in.next()) ? point(*a) : std::nullopt;
        if (!lo || !hi)
            return fail("window corner", a.value_or(QString()));
        p.area.window = QRectF(*lo, *hi).normalized();
    }

    if (!(a = in.next()))
        return fail("plot scale");
    if (!a->isEmpty()) {
        if (Script::keyword(*a, {u"Fit"}, -1) == 0) {
            p.scale.fit = true;
        } else {
            const qsizetype sep = std::max(a->indexOf(u':'), a->indexOf(u'='));
            bool okMm = false, okUnits = false;
            const double mm = sep > 0 ? a->left(sep).trimmed().toDouble(&okMm) : 0;
            const double units = sep > 0 ? a->mid(sep + 1).trimmed().toDouble(&okUnits) : 0;
            if (!okMm || !okUnits || !(mm > 0) || !(units > 0))
                return fail("plot scale", *a);
            p.scale = {false, mm, units, p.scale.scaleLineweights};
        }
    }

    if (!(a = in.next()))
        return fail("plot offset");
    if (!a->isEmpty()) {
        if (!context.isLayout() && Script::keyword(*a, {u"Center"}, -1) == 0) {
            p.offset.centered = true;
        } else if (const auto o = point(*a)) {
            p.offset = {false, o->x(), o->y()};
        } else {
            return fail("plot offset", *a);
        }
    }

    TRION_TRY(yesNo("Plot with plot styles?", p.flags.plotStyles));
    if (!(a = in.next()))
        return fail("plot style table name");
    if (*a == u".")
        p.plotStyle.clear();
    else if (!a->isEmpty())
        p.plotStyle = *a;
    TRION_TRY(yesNo("Plot with lineweights?", p.flags.lineweights));

    if (context.isLayout()) {
        TRION_TRY(yesNo("Scale lineweights with plot scale?", p.scale.scaleLineweights));
        bool first = !p.flags.paperSpaceLast;
        TRION_TRY(yesNo("Plot paper space first?", first));
        p.flags.paperSpaceLast = !first;
        TRION_TRY(yesNo("Hide paperspace objects?", p.flags.hidePaperSpace));
    } else {
        if (!(a = in.next()))
            return fail("shade plot setting");
        const int k = Script::keyword(*a, {u"As displayed", u"Wireframe", u"Hidden", u"Rendered"}, int(p.shade.mode));
        if (k < 0)
            return fail("shade plot setting", *a);
        p.shade.mode = ShadeMode(k);
    }

    normalize(s, context, devices);
    if (!s.toFile) {
        TRION_TRY(yesNo("Write the plot to a file", s.toFile));
    }
    QString file;
    if (s.toFile) {
        if (!(a = in.next()))
            return fail("file name");
        file = *a;
    }
    TRION_TRY(yesNo("Save changes to page setup", s.saveToLayout));
    bool proceed = true;
    TRION_TRY(yesNo("Proceed with plot", proceed));
#undef TRION_TRY

    normalize(s, context, devices);
    if (!file.isEmpty())
        s.file = file;
    return s;
}

} // namespace trion::plot
