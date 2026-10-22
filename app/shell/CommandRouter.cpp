#include "app/shell/CommandRouter.h"

#include "app/core/actions/ActionRegistry.h"

#include <QLoggingCategory>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace trion::shell {

namespace {

Q_STATIC_LOGGING_CATEGORY(lcCmd, "trion.cmd", QtWarningMsg)

bool isCatalogCommand(const QString& name)
{
    return std::ranges::any_of(actions::commands(),
                               [&](const actions::CommandInfo& c) { return name == QLatin1StringView(c.name); });
}

} // namespace

CommandRouter::CommandRouter(actions::Sysvars& sysvars, QObject* parent) : QObject(parent), sysvars_(sysvars) {}

void CommandRouter::add(const QString& name, std::function<void()> fn)
{
    commands_.insert(name.toUpper(), std::move(fn));
}

void CommandRouter::submit(const QString& macro)
{
    QStringView m = macro;
    while (m.startsWith(u"^C", Qt::CaseInsensitive)) // hủy lệnh đang chạy: chưa có lệnh nào chạy
        m = m.sliced(2);
    bool transparent = false;
    while (!m.isEmpty() && (m.front() == u'\'' || m.front() == u'_' || m.front() == u'.')) {
        transparent |= m.front() == u'\'';
        m = m.sliced(1);
    }
    QStringList words = m.toString().replace(u';', u' ').split(u' ', Qt::SkipEmptyParts);
    if (words.isEmpty())
        return; // chỉ có ^C^C: không còn gì để chạy
    const QString name = words.takeFirst().toUpper();
    if (const auto it = commands_.constFind(name); it != commands_.cend()) {
        (*it)();
        return;
    }
    bool isInt = false;
    const int value = words.size() == 1 ? words.front().toInt(&isInt) : 0;
    if (transparent && isInt && !isCatalogCommand(name)) {
        sysvars_.set(name, value);
        return;
    }
    qCInfo(lcCmd).noquote() << "unhandled:" << macro; // nhãn ASCII: stderr Windows theo mã trang ANSI
    emit unhandled(macro);
}

} // namespace trion::shell
