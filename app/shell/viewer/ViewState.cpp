#include "app/shell/viewer/ViewState.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <cmath>

using namespace Qt::StringLiterals;

namespace trion::shell {

namespace {

constexpr qsizetype kMaxName = 255; // tên layer, tên layout (AutoCAD)

Error fail(const char* code, const char* message)
{
    return Error{"ui", code, message};
}

// Thoát ký tự đại diện của wcmatch để -LAYER hiểu tên đúng nguyên văn ("KT.TUONG" không khớp "KT-TUONG").
QString escaped(const QString& name)
{
    QString out;
    out.reserve(name.size() + 4);
    for (QChar c : name) {
        if (QStringView(u"#@.~[],").contains(c)) // * ? ` không có trong tên hợp lệ
            out += u'`';
        out += c;
    }
    return out;
}

} // namespace

bool isLayerName(QStringView name)
{
    if (name.isEmpty() || name.size() > kMaxName || !name.isValidUtf16())
        return false;
    for (QChar c : name)
        if (c.unicode() < 0x20 || c.unicode() == 0x7F || QStringView(u"<>/\\\":;?*|=`").contains(c))
            return false;
    return true;
}

QString viewStatePath(const QString& drawing)
{
    const QFileInfo f(drawing);
    return f.dir().filePath(f.completeBaseName() + u".tcview"_s);
}

QByteArray toJson(const ViewState& s)
{
    QJsonArray layers;
    for (const LayerFlags& l : s.layers)
        layers.append(QJsonObject{{u"name"_s, l.name}, {u"on"_s, l.on}, {u"frozen"_s, l.frozen}});
    const QJsonObject view{{u"center"_s, QJsonArray{s.centerX, s.centerY}}, {u"height"_s, s.height}};
    return QJsonDocument(QJsonObject{{u"tcview"_s, ViewState::kVersion},
                                     {u"layout"_s, s.layout},
                                     {u"view"_s, view},
                                     {u"layers"_s, layers}})
        .toJson(QJsonDocument::Compact);
}

Expected<ViewState> fromJson(QByteArrayView json)
{
    if (json.size() > ViewState::kMaxBytes)
        return fail("too-large", "tcview larger than 1 MB");
    const auto corrupt = [] { return fail("corrupt", "tcview is not a valid view state"); };
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toByteArray(), &error); // bản sao kết thúc NUL cho Qt
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return corrupt();
    const QJsonObject o = doc.object();
    const qint64 version = o.value(u"tcview"_s).toInteger(-1);
    if (version > ViewState::kVersion)
        return fail("version", "tcview written by a newer TrionCAD");
    if (version != ViewState::kVersion)
        return corrupt();

    ViewState s;
    s.layout = o.value(u"layout"_s).toString();
    const QJsonObject view = o.value(u"view"_s).toObject();
    const QJsonArray center = view.value(u"center"_s).toArray();
    const QJsonValue height = view.value(u"height"_s);
    const QJsonValue layers = o.value(u"layers"_s);
    if (s.layout.isEmpty() || s.layout.size() > kMaxName || !s.layout.isValidUtf16() || center.size() != 2 ||
        !center[0].isDouble() || !center[1].isDouble() || !height.isDouble() || !layers.isArray())
        return corrupt();
    s.centerX = center[0].toDouble();
    s.centerY = center[1].toDouble();
    s.height = height.toDouble();
    if (!std::isfinite(s.centerX) || !std::isfinite(s.centerY) || !std::isfinite(s.height) || !(s.height > 0))
        return corrupt();
    const QJsonArray list = layers.toArray();
    s.layers.reserve(list.size());
    for (const QJsonValue& v : list) {
        const QJsonObject l = v.toObject();
        const QJsonValue name = l.value(u"name"_s), on = l.value(u"on"_s), frozen = l.value(u"frozen"_s);
        if (!name.isString() || !on.isBool() || !frozen.isBool() || !isLayerName(name.toString()))
            return corrupt();
        s.layers.append({name.toString(), on.toBool(), frozen.toBool()});
    }
    return s;
}

Expected<ViewState> readViewState(const QString& drawing)
{
    QFile f(viewStatePath(drawing));
    if (!f.exists())
        return fail("missing", "no tcview next to the drawing");
    if (!f.open(QIODevice::ReadOnly))
        return fail("io", "cannot read tcview");
    // Đọc tối đa kMaxBytes + 1: tệp lớn lên giữa chừng cũng không đọc hết vào bộ nhớ.
    const QByteArray data = f.read(ViewState::kMaxBytes + 1);
    if (f.error() != QFileDevice::NoError)
        return fail("io", "cannot read tcview");
    return fromJson(data);
}

Expected<void> writeViewState(const QString& drawing, const ViewState& state)
{
    if (QFileInfo(drawing).suffix().compare(u"tcview"_s, Qt::CaseInsensitive) == 0) // viewStatePath là chính nó
        return fail("path", "the drawing itself is a .tcview file");
    const QString path = viewStatePath(drawing);
    const QByteArray data = toJson(state);
    if (data.size() > ViewState::kMaxBytes)
        return fail("too-large", "tcview larger than 1 MB");
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size() || !f.commit())
        return fail("io", "cannot write tcview");
    return {};
}

LayerMacro LayerMacro::to(const QList<LayerFlags>& target)
{
    LayerMacro m;
    for (const LayerFlags& l : target) {
        (l.on ? m.on : m.off) << l.name;
        (l.frozen ? m.freeze : m.thaw) << l.name;
    }
    return m;
}

QString LayerMacro::text() const
{
    QString out;
    const auto option = [&out](const char* keyword, const QStringList& names) {
        QStringList valid;
        for (const QString& n : names)
            if (isLayerName(n))
                valid << escaped(n);
        if (valid.isEmpty())
            return;
        QString list = valid.join(u',');
        if (list.contains(u' '))
            list = u'"' + list + u'"';
        out += u' ';
        out += QLatin1StringView(keyword);
        out += u' ';
        out += list;
    };
    option("_ON", on);
    option("_OFF", off);
    option("_THAW", thaw);
    option("_FREEZE", freeze);
    return out.isEmpty() ? out : u"'_-LAYER"_s + out + u';';
}

} // namespace trion::shell
