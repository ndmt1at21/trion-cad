#include "app/widgets/dialog/SuppressedPrompts.h"

#include <QJsonObject>

using namespace Qt::StringLiterals;

namespace trion::ui {

namespace {

const settings::Setting<QJsonObject> kSuppressed{"ui/suppressedPrompts", {}};

} // namespace

SuppressedPrompts::SuppressedPrompts(settings::Store& store, QObject* parent)
    : QAbstractListModel(parent), store_(store)
{
    reload();
    connect(&store_, &settings::Store::changed, this, [this](const QString& key) {
        if (key == QLatin1StringView(kSuppressed.key))
            reload();
    });
}

std::optional<bool> SuppressedPrompts::remembered(const QString& id, const settings::Store& store)
{
    const QJsonValue accepted = store.get(kSuppressed).value(id)["accepted"_L1];
    return accepted.isBool() ? std::optional(accepted.toBool()) : std::nullopt;
}

void SuppressedPrompts::remember(const QString& id, const QString& title, bool accepted, settings::Store& store)
{
    QJsonObject all = store.get(kSuppressed);
    all.insert(id, QJsonObject{{u"title"_s, title}, {u"accepted"_s, accepted}});
    store.set(kSuppressed, all);
}

int SuppressedPrompts::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(rows_.size());
}

QVariant SuppressedPrompts::data(const QModelIndex& index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid))
        return {};
    const auto& [id, title] = rows_[index.row()];
    return role == Qt::DisplayRole ? QVariant(title) : role == IdRole ? QVariant(id) : QVariant();
}

void SuppressedPrompts::restore(int row)
{
    if (row < 0 || row >= rows_.size())
        return;
    QJsonObject all = store_.get(kSuppressed);
    all.remove(rows_[row].first);
    store_.set(kSuppressed, all); // changed → reload
}

void SuppressedPrompts::restoreAll()
{
    store_.reset(kSuppressed);
}

void SuppressedPrompts::reload()
{
    beginResetModel();
    rows_.clear();
    const QJsonObject all = store_.get(kSuppressed);
    for (auto it = all.begin(); it != all.end(); ++it) // QJsonObject: theo mã
        if (it.value()["accepted"_L1].isBool())        // như remembered(): mục hỏng không hiện
            rows_.append({it.key(), it.value()["title"_L1].toString()});
    endResetModel();
}

} // namespace trion::ui
