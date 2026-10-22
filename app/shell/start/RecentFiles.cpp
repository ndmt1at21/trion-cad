#include "app/shell/start/RecentFiles.h"

#include "app/core/UiExecutor.h"
#include "app/core/i18n/UiFormat.h"
#include "core/base/settings/Settings.h"
#include "core/base/text/Fold.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QTimeZone>

#include <algorithm>
#include <cctype>

using namespace Qt::StringLiterals;

namespace trion::shell {
namespace {

const settings::Setting<int> kLimit{"recent/limit", RecentFiles::kDefaultLimit};
const QString kFilesKey = u"recent/files"_s;

#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
constexpr Qt::CaseSensitivity kPathCase = Qt::CaseInsensitive;
#else
constexpr Qt::CaseSensitivity kPathCase = Qt::CaseSensitive;
#endif

QString normalized(const QString& path)
{
    return path.isEmpty() ? QString() : QDir::cleanPath(QDir::fromNativeSeparators(path));
}

QString pathKey(const QString& path)
{
    return kPathCase == Qt::CaseInsensitive ? path.toCaseFolded() : path;
}

} // namespace

struct RecentFiles::Probe {
    QString path{};
    bool missing = true;
    qint64 size = -1;
    QDateTime modified{};
    QString version{};
};

RecentFiles::RecentFiles(settings::Store& store, QObject* parent) : QAbstractTableModel(parent), store_(store)
{
    load();
}

RecentFiles::~RecentFiles()
{
    cancel_.cancel();
}

void RecentFiles::fold(Entry& e)
{
    e.folded = text::foldForSearch(QString(e.name + u' ' + QDir::toNativeSeparators(QFileInfo(e.path).path())));
}

void RecentFiles::load()
{
    QSet<QString> seen;
    const QJsonArray list = store_.value(kFilesKey).toArray();
    entries_.reserve(std::size_t(list.size()));
    for (const QJsonValue& v : list) {
        const QJsonObject o = v.toObject();
        Entry e{.path = normalized(o.value("path"_L1).toString())};
        if (e.path.isEmpty() || seen.contains(pathKey(e.path)))
            continue; // tệp sửa tay hỏng, trùng: bỏ mục đó
        seen.insert(pathKey(e.path));
        e.name = QFileInfo(e.path).fileName();
        if (const QJsonValue m = o.value("modified"_L1); m.isDouble())
            e.modified = QDateTime::fromMSecsSinceEpoch(qint64(m.toDouble()), QTimeZone::UTC);
        e.size = qint64(o.value("size"_L1).toDouble(-1));
        e.version = o.value("version"_L1).toString();
        e.pinned = o.value("pinned"_L1).toBool();
        e.missing = o.value("missing"_L1).toBool();
        fold(e);
        entries_.push_back(std::move(e));
    }
    trim();
}

void RecentFiles::save()
{
    QJsonArray list;
    for (const Entry& e : entries_) {
        QJsonObject o{{"path"_L1, e.path}};
        if (e.pinned)
            o.insert("pinned"_L1, true);
        if (e.missing)
            o.insert("missing"_L1, true);
        if (e.size >= 0)
            o.insert("size"_L1, double(e.size));
        if (e.modified.isValid())
            o.insert("modified"_L1, double(e.modified.toMSecsSinceEpoch()));
        if (!e.version.isEmpty())
            o.insert("version"_L1, e.version);
        list.append(o);
    }
    store_.setValue(kFilesKey, list);
}

int RecentFiles::limit() const
{
    return std::max(1, store_.get(kLimit));
}

// Bỏ mục cũ nhất chưa ghim cho tới khi vừa limit(). ponytail: toàn mục ghim thì danh sách vượt giới hạn.
void RecentFiles::trim()
{
    for (int i = size() - 1; i >= 0 && size() > limit(); --i)
        if (!entries_[std::size_t(i)].pinned) {
            beginRemoveRows({}, i, i);
            entries_.erase(entries_.begin() + i);
            endRemoveRows();
        }
}

int RecentFiles::indexOf(const QString& path) const
{
    const QString p = normalized(path);
    for (int i = 0; i < size(); ++i)
        if (entries_[std::size_t(i)].path.compare(p, kPathCase) == 0)
            return i;
    return -1;
}

void RecentFiles::add(const QString& path)
{
    const QString p = normalized(path);
    if (p.isEmpty())
        return;
    if (const int i = indexOf(p); i > 0) {
        beginMoveRows({}, i, i, {}, 0);
        Entry e = std::move(entries_[std::size_t(i)]);
        entries_.erase(entries_.begin() + i);
        entries_.insert(entries_.begin(), std::move(e));
        endMoveRows();
    } else if (i < 0) {
        Entry e{.path = p, .name = QFileInfo(p).fileName()};
        fold(e);
        beginInsertRows({}, 0, 0);
        entries_.insert(entries_.begin(), std::move(e));
        endInsertRows();
        trim();
    }
    save();
    refresh();
}

bool RecentFiles::remove(const QString& path)
{
    const int i = indexOf(path);
    if (i < 0)
        return false;
    beginRemoveRows({}, i, i);
    entries_.erase(entries_.begin() + i);
    endRemoveRows();
    save();
    return true;
}

void RecentFiles::restore(int row, const Entry& entry)
{
    if (entry.path.isEmpty() || indexOf(entry.path) >= 0)
        return;
    row = std::clamp(row, 0, size());
    beginInsertRows({}, row, row);
    entries_.insert(entries_.begin() + row, entry);
    endInsertRows();
    save();
}

void RecentFiles::setPinned(const QString& path, bool pinned)
{
    const int i = indexOf(path);
    if (i < 0 || entries_[std::size_t(i)].pinned == pinned)
        return;
    entries_[std::size_t(i)].pinned = pinned;
    emit dataChanged(index(i, 0), index(i, ColumnCount - 1));
    save();
}

QStringList RecentFiles::folders(int max) const
{
    QStringList out;
    QSet<QString> seen;
    for (const Entry& e : entries_) {
        if (out.size() >= max)
            break;
        const QString dir = QFileInfo(e.path).path();
        if (!seen.contains(pathKey(dir))) {
            seen.insert(pathKey(dir));
            out << QDir::toNativeSeparators(dir);
        }
    }
    return out;
}

// ------------------------------------------------------------------------------------------------- kiểm tệp ở nền

QString RecentFiles::versionName(QByteArrayView code, bool dxf)
{
    static constexpr struct {
        const char* code;
        const char* name;
    } kNames[] = {{"AC1006", "R10"},  {"AC1009", "R12"},  {"AC1012", "R13"},  {"AC1014", "R14"},
                  {"AC1015", "2000"}, {"AC1018", "2004"}, {"AC1021", "2007"}, {"AC1024", "2010"},
                  {"AC1027", "2013"}, {"AC1032", "2018"}};
    if (code.size() < 6 || !code.startsWith("AC10") || !std::isdigit(uchar(code[4])) || !std::isdigit(uchar(code[5])))
        return {};
    code = code.first(6);
    for (const auto& k : kNames)
        if (code == QByteArrayView(k.code))
            return (dxf ? u"DXF "_s : u"DWG "_s) + QLatin1StringView(k.name);
    return QString::fromLatin1(code); // bản mới hơn bảng: hiện mã
}

QString RecentFiles::readVersion(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    if (!path.endsWith(u".dxf"_s, Qt::CaseInsensitive))
        return versionName(f.read(6), false);
    // DXF dạng chữ: "  9 / $ACADVER /  1 / AC1027" ở đầu phần HEADER; dạng nhị phân hoặc không thấy: chỉ "DXF".
    const QByteArray head = f.read(4096);
    if (const qsizetype at = head.indexOf("$ACADVER"); at >= 0) {
        const QList<QByteArray> lines = head.mid(at).split('\n');
        if (lines.size() >= 3)
            if (QString name = versionName(lines[2].trimmed(), true); !name.isEmpty())
                return name;
    }
    return u"DXF"_s;
}

void RecentFiles::refresh()
{
    cancel_.cancel();
    cancel_ = sched::CancelSource();
    const std::uint64_t generation = ++generation_;
    QStringList paths;
    paths.reserve(size());
    for (const Entry& e : entries_)
        paths << e.path;
    refreshing_ = true;
    sched::submit(sched::Priority::Background,
                  [rx = ui::Receiver(this), this, paths, generation, cancel = cancel_.token()] {
                      std::vector<Probe> probes;
                      probes.reserve(std::size_t(paths.size()));
                      for (const QString& path : paths) {
                          if (cancel.cancelled())
                              return;
                          Probe p{.path = path};
                          const QFileInfo info(path);
                          if (info.isFile()) {
                              p.missing = false;
                              p.size = info.size();
                              p.modified = info.lastModified(QTimeZone::UTC);
                              p.version = readVersion(path);
                          }
                          probes.push_back(std::move(p));
                      }
                      ui::post(rx, [this, generation, probes = std::move(probes)] { apply(generation, probes); });
                  });
}

void RecentFiles::apply(std::uint64_t generation, const std::vector<Probe>& probes)
{
    if (generation != generation_)
        return; // lần kiểm cũ
    refreshing_ = false;
    int first = size(), last = -1; // dòng đổi: báo một lô (proxy sắp lại một lần, không mỗi dòng một lần)
    QStringList edits;
    for (std::size_t k = 0; k < probes.size(); ++k) {
        const Probe& p = probes[k];
        // Thứ tự thường không đổi trong lúc kiểm: so đúng chỗ trước, lệch mới tìm.
        const int i = k < entries_.size() && entries_[k].path == p.path ? int(k) : indexOf(p.path);
        if (i < 0)
            continue; // đã xóa khỏi danh sách trong lúc kiểm
        Entry& e = entries_[std::size_t(i)];
        const bool edited = !p.missing && e.modified.isValid() && (e.modified != p.modified || e.size != p.size);
        if (p.missing ? e.missing
                      : !e.missing && e.size == p.size && e.modified == p.modified && e.version == p.version)
            continue;
        if (p.missing) {
            e.missing = true; // giữ số liệu biết lần cuối
        } else {
            e.missing = false;
            e.size = p.size;
            e.modified = p.modified;
            e.version = p.version;
        }
        first = std::min(first, i);
        last = std::max(last, i);
        if (edited)
            edits << e.path;
    }
    if (last >= 0) {
        save();
        emit dataChanged(index(first, 0), index(last, ColumnCount - 1));
    }
    for (const QString& path : std::as_const(edits))
        emit fileChanged(path);
    emit refreshed();
}

// ------------------------------------------------------------------------------------------------- model

int RecentFiles::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : size();
}

int RecentFiles::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant RecentFiles::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= size())
        return {};
    const Entry& e = entries_[std::size_t(index.row())];
    const auto date = [&] { return e.modified.isValid() ? i18n::formatDate(e.modified.toLocalTime().date()) : QString(); };
    const auto bytes = [&] { return e.size >= 0 ? i18n::formatBytes(e.size) : QString(); };
    const auto folder = [&] { return QDir::toNativeSeparators(QFileInfo(e.path).path()); };
    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case Name:
            return e.name;
        case Modified:
            return date();
        case Size:
            return bytes();
        case Version:
            return e.version;
        default:
            return {};
        }
    case Qt::ToolTipRole:
        return QDir::toNativeSeparators(e.path);
    case Qt::AccessibleTextRole: {
        if (index.column() == Pin)
            return e.pinned ? tr("Bỏ ghim") : tr("Ghim");
        if (index.column() != Name)
            return data(index, Qt::DisplayRole);
        QStringList parts{e.name};
        if (e.pinned)
            parts << tr("đã ghim");
        if (e.missing)
            parts << tr("Không tìm thấy");
        else
            parts << data(index, MetaRole).toString();
        parts << folder();
        parts.removeAll(QString());
        return parts.join(u", "_s);
    }
    case PathRole:
        return e.path;
    case FolderRole:
        return folder();
    case PinnedRole:
        return e.pinned;
    case MissingRole:
        return e.missing;
    case MetaRole: {
        QStringList parts{date(), bytes(), e.version};
        parts.removeAll(QString());
        return parts.join(u" · "_s);
    }
    default:
        return {};
    }
}

QVariant RecentFiles::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal)
        return {};
    if (role == Qt::DisplayRole) {
        switch (section) {
        case Name:
            return tr("Tên");
        case Modified:
            return tr("Ngày sửa");
        case Size:
            return tr("Kích thước");
        case Version:
            return tr("Phiên bản");
        default:
            return QString();
        }
    }
    if ((role == Qt::ToolTipRole || role == Qt::AccessibleTextRole) && section == Pin)
        return tr("Ghim");
    return {};
}

Qt::ItemFlags RecentFiles::flags(const QModelIndex& index) const
{
    return index.isValid() ? Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemNeverHasChildren : Qt::NoItemFlags;
}

// ------------------------------------------------------------------------------------------------- RecentFilter

RecentFilter::RecentFilter(RecentFiles& files, QObject* parent) : QSortFilterProxyModel(parent), files_(files)
{
    setSourceModel(&files);
    sort(0);
}

void RecentFilter::setQuery(const QString& text)
{
    if (text == query_)
        return;
    beginFilterChange();
    query_ = text;
    folded_ = text::foldForSearch(text.trimmed());
    endFilterChange(Direction::Rows);
}

void RecentFilter::setSortKey(Sort sort)
{
    if (sort == sort_)
        return;
    sort_ = sort;
    invalidate();
}

const RecentFiles::Entry& RecentFilter::entry(int row) const
{
    return files_.entry(mapToSource(index(row, 0)).row());
}

bool RecentFilter::filterAcceptsRow(int row, const QModelIndex&) const
{
    return folded_.isEmpty() || files_.entry(row).folded.contains(folded_);
}

bool RecentFilter::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const RecentFiles::Entry& a = files_.entry(left.row());
    const RecentFiles::Entry& b = files_.entry(right.row());
    if (a.pinned != b.pinned)
        return a.pinned;
    switch (sort_) {
    case Sort::Date: // mới trước; chưa biết ngày xếp sau
        if (a.modified != b.modified)
            return a.modified > b.modified;
        break;
    case Sort::Name: // khóa gấp bắt đầu bằng tên: A→Z không phân biệt dấu, rồi theo thư mục
        if (const int c = a.folded.compare(b.folded); c != 0)
            return c < 0;
        break;
    case Sort::Size:
        if (a.size != b.size)
            return a.size > b.size;
        break;
    }
    return left.row() < right.row(); // thứ tự mở
}

} // namespace trion::shell
