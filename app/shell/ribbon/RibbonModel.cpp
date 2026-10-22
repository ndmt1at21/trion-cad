#include "app/shell/ribbon/RibbonModel.h"

#include "app/core/actions/ActionRegistry.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include <algorithm>
#include <initializer_list>

using namespace Qt::StringLiterals;

namespace trion::ribbon {

namespace {

const QString kDir = u":/trion/ribbon/"_s;
constexpr const char* kTabs[] = {"home", "insert", "annotate", "view", "manage", "output", "vntools", "express"};

// Đọc một tệp: lỗi ghi "tệp: vị trí: lỗi" (cho lint, nhà phát triển; không hiện lên giao diện).
struct Reader {
    QStringList& errors;
    QString file;

    void error(const QString& where, const QString& what) const { errors << u"%1: %2: %3"_s.arg(file, where, what); }

    bool known(const QJsonObject& o, std::initializer_list<QStringView> keys, const QString& where) const
    {
        bool ok = true;
        for (auto it = o.begin(); it != o.end(); ++it)
            if (std::find(keys.begin(), keys.end(), it.key()) == keys.end()) {
                error(where, u"khóa lạ \"%1\""_s.arg(it.key()));
                ok = false;
            }
        return ok;
    }

    QString text(const QJsonObject& o, QStringView key, const QString& where, bool required = false) const
    {
        const QJsonValue v = o.value(key);
        if (v.isString() && !v.toString().isEmpty())
            return v.toString();
        if (!v.isUndefined() || required)
            error(where, u"\"%1\" phải là chuỗi không rỗng"_s.arg(key));
        return {};
    }

    MenuItem menuItem(const QJsonValue& v, const QString& where) const
    {
        const QJsonObject o = v.toObject();
        if (o.value(u"separator").toBool())
            return {.separator = true};
        known(o, {u"label", u"cmd", u"args", u"icon"}, where);
        return {text(o, u"label", where, true), text(o, u"cmd", where, true), text(o, u"args", where),
                text(o, u"icon", where)};
    }

    std::vector<MenuItem> menu(const QJsonValue& v, const QString& where) const
    {
        std::vector<MenuItem> out;
        const QJsonArray a = v.toArray();
        for (qsizetype i = 0; i < a.size(); ++i)
            out.push_back(menuItem(a[i], u"%1[%2]"_s.arg(where).arg(i)));
        return out;
    }

    Control control(const QJsonObject& o, const QString& where) const
    {
        static const QHash<QString, ControlType> types = {
            {u"button"_s, ControlType::Button},   {u"split"_s, ControlType::Split},
            {u"toggle"_s, ControlType::Toggle},   {u"dropdown"_s, ControlType::DropDown},
            {u"gallery"_s, ControlType::Gallery}, {u"number"_s, ControlType::Number},
            {u"slider"_s, ControlType::Slider},
        };
        static const QHash<QString, ControlSize> sizes = {
            {u"large"_s, ControlSize::Large}, {u"small"_s, ControlSize::Small}, {u"icon"_s, ControlSize::Icon}};
        known(o,
              {u"type", u"size", u"id", u"label", u"icon", u"cmd", u"args", u"need", u"source", u"sysvar", u"above",
               u"min", u"max", u"width", u"menuOnly", u"menu"},
              where);
        Control c;
        const QString type = o.value(u"type").toString();
        if (!types.contains(type))
            error(where, u"\"type\" lạ \"%1\""_s.arg(type));
        c.type = types.value(type);
        if (const QJsonValue s = o.value(u"size"); !s.isUndefined()) {
            if (!sizes.contains(s.toString()))
                error(where, u"\"size\" lạ \"%1\""_s.arg(s.toString()));
            c.size = sizes.value(s.toString(), ControlSize::Small);
        }
        c.label = text(o, u"label", where, true);
        c.icon = text(o, u"icon", where);
        c.cmd = text(o, u"cmd", where);
        c.args = text(o, u"args", where);
        c.need = text(o, u"need", where);
        if (!c.need.isEmpty() && c.need != u"VP" && c.need != u"PRO")
            error(where, u"\"need\" phải là VP hoặc PRO"_s);
        c.source = text(o, u"source", where);
        c.sysvar = text(o, u"sysvar", where);
        if (const QJsonValue v = o.value(u"above"); v.isDouble())
            c.above = v.toDouble();
        c.min = o.value(u"min").toDouble(c.min);
        c.max = o.value(u"max").toDouble(c.max);
        c.width = o.value(u"width").toInt();
        c.menuOnly = o.value(u"menuOnly").toBool();
        c.menu = menu(o.value(u"menu"), where + u".menu"_s);

        const bool command =
            c.type == ControlType::Button || c.type == ControlType::Split || c.type == ControlType::Toggle;
        const bool data = c.type == ControlType::DropDown || c.type == ControlType::Gallery;
        const bool value = c.type == ControlType::Number || c.type == ControlType::Slider;
        if (command && c.cmd.isEmpty())
            error(where, u"thiếu \"cmd\""_s);
        if (data && c.source.isEmpty())
            error(where, u"thiếu \"source\""_s);
        if (value && c.sysvar.isEmpty())
            error(where, u"thiếu \"sysvar\""_s);
        if (c.type == ControlType::Split && c.menu.empty() && c.source.isEmpty())
            error(where, u"split cần \"menu\" hoặc \"source\""_s);
        if (c.type != ControlType::Split && (!c.menu.empty() || c.menuOnly))
            error(where, u"\"menu\", \"menuOnly\" chỉ dùng cho split"_s);
        if (c.type == ControlType::Slider && !(c.min < c.max))
            error(where, u"slider cần min < max"_s);
        c.id = o.value(u"id").toString(c.cmd.isEmpty() ? (c.source.isEmpty() ? c.sysvar : c.source) : c.cmd);
        return c;
    }

    std::vector<Item> items(const QJsonValue& v, const QString& where) const
    {
        std::vector<Item> out;
        const QJsonArray a = v.toArray();
        if (!v.isArray() || a.isEmpty())
            error(where, u"cần mảng không rỗng"_s);
        for (qsizetype i = 0; i < a.size(); ++i) {
            const QJsonObject o = a[i].toObject();
            const QString at = u"%1[%2]"_s.arg(where).arg(i);
            if (o.contains(u"row") || o.contains(u"column")) {
                const bool row = o.contains(u"row");
                known(o, {row ? u"row" : u"column"}, at);
                out.push_back({.kind = row ? Item::Kind::Row : Item::Kind::Column,
                               .items = items(o.value(row ? u"row" : u"column"), at)});
            } else {
                out.push_back({.control = control(o, at)});
            }
        }
        return out;
    }

    Tab tab(const QJsonObject& o) const
    {
        known(o, {u"id", u"name", u"panels"}, u"tab"_s);
        Tab t{text(o, u"id", u"tab"_s, true), text(o, u"name", u"tab"_s, true), {}};
        const QJsonArray panels = o.value(u"panels").toArray();
        for (qsizetype i = 0; i < panels.size(); ++i) {
            const QJsonObject p = panels[i].toObject();
            const QString at = u"panels[%1]"_s.arg(i);
            known(p, {u"id", u"name", u"icon", u"shrink", u"items", u"more"}, at);
            t.panels.push_back({text(p, u"id", at, true), text(p, u"name", at, true), text(p, u"icon", at),
                                p.value(u"shrink").toInt(1), items(p.value(u"items"), at + u".items"_s),
                                menu(p.value(u"more"), at + u".more"_s)});
        }
        if (t.panels.empty())
            error(u"tab"_s, u"không có panel"_s);
        return t;
    }
};

bool has(const std::vector<MenuItem>& menu, const QString& cmd)
{
    return std::ranges::any_of(menu, [&](const MenuItem& m) { return m.cmd == cmd; });
}

bool has(const std::vector<Item>& items, const QString& cmd)
{
    return std::ranges::any_of(items, [&](const Item& it) {
        return it.control.cmd == cmd || has(it.control.menu, cmd) || has(it.items, cmd);
    });
}

} // namespace

RibbonModel::RibbonModel(const QStringList& files)
{
    for (const QString& f : files) {
        QFile file(f);
        Reader r{errors_, QFileInfo(f).fileName()};
        if (!file.open(QIODevice::ReadOnly)) {
            r.error(u"tệp"_s, file.errorString());
            continue;
        }
        QJsonParseError e;
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &e);
        if (!doc.isObject()) {
            r.error(u"vị trí %1"_s.arg(e.offset), e.errorString());
            continue;
        }
        tabs_.push_back(r.tab(doc.object()));
    }
}

QStringList RibbonModel::defaultFiles()
{
    QStringList out;
    for (const char* t : kTabs)
        out << kDir + QLatin1StringView(t) + u".json"_s;
    return out;
}

QStringList RibbonModel::resourceFiles()
{
    QStringList out;
    for (const QString& f : QDir(kDir).entryList({u"*.json"_s}, QDir::Files, QDir::Name))
        out << kDir + f;
    return out;
}

QString RibbonModel::locate(const QString& cmd) const
{
    if (!cmd.isEmpty())
        for (const Tab& t : tabs_)
            for (const Panel& p : t.panels)
                if (has(p.items, cmd) || has(p.more, cmd))
                    return u"%1 › %2"_s.arg(line(t.name), line(p.name));
    return {};
}

const char* RibbonModel::context(const QString& label)
{
    static const QSet<QString> actionLabels = [] {
        QSet<QString> s;
        for (const actions::CommandInfo& c : actions::commands())
            s.insert(QString::fromUtf8(c.label));
        return s;
    }();
    return actionLabels.contains(label) ? "actions" : "ribbon";
}

QString RibbonModel::text(const QString& label)
{
    return label.isEmpty() ? label : QCoreApplication::translate(context(label), label.toUtf8().constData());
}

} // namespace trion::ribbon
