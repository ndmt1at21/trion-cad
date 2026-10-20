#include "core/base/settings/Settings.h"

#include "core/base/assert.h"
#include "core/base/sched/Scheduler.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>
#include <QThread>
#include <QtDebug>

#include <algorithm>
#include <atomic>

namespace trion::settings {

struct Store::Writer {
    QString path;
    std::mutex mutex;
    quint64 written = 0;

    // Ghi nguyên tử (tệp tạm + đổi tên). Bỏ qua nếu đã có thế hệ mới hơn trên đĩa.
    void write(const QByteArray& bytes, quint64 gen)
    {
        std::lock_guard lock(mutex);
        if (gen <= written)
            return;
        QDir().mkpath(QFileInfo(path).absolutePath());
        QSaveFile f(path);
        if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size() || !f.commit()) {
            qWarning("settings: không ghi được %s", qUtf8Printable(path));
            return;
        }
        written = gen;
    }
};

namespace {
std::atomic<Store*> g_instance{nullptr};
}

Store::Store(QString filePath, QObject* parent)
    : QObject(parent), path_(std::move(filePath)), writer_(std::make_shared<Writer>())
{
    writer_->path = path_;
    QFile f(path_);
    if (!f.exists() || !f.open(QIODevice::ReadOnly))
        return;
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    f.close();
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        const QString bad = path_ + QStringLiteral(".bad");
        QFile::remove(bad);
        QFile::rename(path_, bad);
        qWarning("settings: tệp hỏng, đã chuyển sang %s, dùng mặc định", qUtf8Printable(bad));
        return;
    }
    const QJsonObject o = doc.object();
    for (auto it = o.begin(); it != o.end(); ++it)
        values_.insert(it.key(), it.value());
}

Store::~Store()
{
    if (writeQueued_)
        flush();
    Store* self = this;
    g_instance.compare_exchange_strong(self, nullptr);
}

Store& Store::instance()
{
    if (Store* s = g_instance.load())
        return *s;
    static Store fallback(defaultFilePath());
    return fallback;
}

void Store::setInstance(Store* store)
{
    g_instance.store(store);
}

QString Store::defaultFilePath()
{
    // ponytail: thư mục "dev" cố định; thư mục theo năm phiên bản (%APPDATA%\TrionCAD\<năm>…) do NT-010/NT-111.
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
           QStringLiteral("/trioncad/dev/user.json");
}

QJsonValue Store::value(const QString& key) const
{
    std::lock_guard lock(mutex_);
    return values_.value(key, QJsonValue(QJsonValue::Undefined));
}

void Store::setValue(const QString& key, const QJsonValue& v)
{
    TRION_ASSERT(QThread::currentThread() == thread());
    {
        std::lock_guard lock(mutex_);
        auto it = values_.find(key);
        if (it != values_.end() && *it == v)
            return;
        values_.insert(key, v);
    }
    emit changed(key);
    scheduleWrite();
}

void Store::remove(const QString& key)
{
    TRION_ASSERT(QThread::currentThread() == thread());
    {
        std::lock_guard lock(mutex_);
        if (!values_.remove(key))
            return;
    }
    emit changed(key);
    scheduleWrite();
}

QStringList Store::keys() const
{
    std::lock_guard lock(mutex_);
    QStringList k = values_.keys();
    std::sort(k.begin(), k.end());
    return k;
}

QByteArray Store::serialize() const
{
    QJsonObject o;
    std::lock_guard lock(mutex_);
    for (auto it = values_.begin(); it != values_.end(); ++it)
        o.insert(it.key(), it.value());
    return QJsonDocument(o).toJson(QJsonDocument::Indented);
}

// Gom mọi lần set trong cùng một lượt vòng sự kiện thành một lần ghi, ghi ở luồng nền.
void Store::scheduleWrite()
{
    if (writeQueued_)
        return;
    writeQueued_ = true;
    QMetaObject::invokeMethod(
        this,
        [this] {
            if (!writeQueued_)
                return; // flush() đã ghi
            writeQueued_ = false;
            sched::submit(sched::Priority::Background,
                          [w = writer_, bytes = serialize(), gen = ++generation_] { w->write(bytes, gen); });
        },
        Qt::QueuedConnection);
}

void Store::flush()
{
    writeQueued_ = false;
    writer_->write(serialize(), ++generation_);
}

} // namespace trion::settings
