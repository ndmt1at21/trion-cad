#pragma once
// Kho cài đặt có kiểu — tập con của NT-010 cho các task UI.
// ponytail: chỉ phạm vi người dùng, một tệp JSON, đọc qua mutex. Phạm vi máy/profile, snapshot đọc
// không khóa, di trú phiên bản, xuất/nhập .trionprofile, thư mục theo năm do NT-010 thêm sau, giữ API.
//
// Dùng:
//   inline const trion::settings::Setting<QString> kTheme{"ui/theme", "dark"};
//   auto& s = trion::settings::Store::instance();
//   s.set(kTheme, "light");            // luồng UI; ghi đĩa gom lại, chạy nền (DoD-U 4)
//   QString t = s.get(kTheme);         // mọi luồng
//   connect(&s, &Store::changed, ...)  // báo theo khóa

#include <QByteArray>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>
#include <mutex>

namespace trion::settings {

enum class Scope { User };

template <class T>
struct Setting {
    const char* key; // "ui/theme": nhóm/tên, ASCII
    T def;
    Scope scope = Scope::User;
};

// Chuyển giá trị ⇄ JSON. Sai kiểu trong tệp thì trả mặc định (NT-010 tiêu chí 2).
template <class T>
struct Codec;
template <>
struct Codec<bool> {
    static QJsonValue encode(bool v) { return v; }
    static bool decode(const QJsonValue& j, bool d) { return j.isBool() ? j.toBool() : d; }
};
template <>
struct Codec<int> {
    static QJsonValue encode(int v) { return v; }
    static int decode(const QJsonValue& j, int d) { return j.isDouble() ? j.toInt(d) : d; }
};
template <>
struct Codec<double> {
    static QJsonValue encode(double v) { return v; }
    static double decode(const QJsonValue& j, double d) { return j.isDouble() ? j.toDouble() : d; }
};
template <>
struct Codec<QString> {
    static QJsonValue encode(const QString& v) { return v; }
    static QString decode(const QJsonValue& j, const QString& d) { return j.isString() ? j.toString() : d; }
};
template <>
struct Codec<QStringList> {
    static QJsonValue encode(const QStringList& v) { return QJsonArray::fromStringList(v); }
    static QStringList decode(const QJsonValue& j, const QStringList& d)
    {
        if (!j.isArray())
            return d;
        QStringList out;
        for (const QJsonValue& e : j.toArray()) {
            if (!e.isString())
                return d;
            out << e.toString();
        }
        return out;
    }
};
template <>
struct Codec<QByteArray> { // ví dụ QMainWindow::saveState()
    static QJsonValue encode(const QByteArray& v) { return QString::fromLatin1(v.toBase64()); }
    static QByteArray decode(const QJsonValue& j, const QByteArray& d)
    {
        return j.isString() ? QByteArray::fromBase64(j.toString().toLatin1()) : d;
    }
};
template <>
struct Codec<QJsonObject> {
    static QJsonValue encode(const QJsonObject& v) { return v; }
    static QJsonObject decode(const QJsonValue& j, const QJsonObject& d) { return j.isObject() ? j.toObject() : d; }
};

class Store : public QObject {
    Q_OBJECT
public:
    // Nạp ngay từ filePath. Tệp hỏng: giữ bản "<tệp>.bad", dùng mặc định, cảnh báo.
    explicit Store(QString filePath, QObject* parent = nullptr);
    ~Store() override; // ghi nốt phần chưa ghi, đồng bộ

    // Kho của tiến trình: kho đặt qua setInstance(), nếu không thì kho ở defaultFilePath().
    static Store& instance();
    static void setInstance(Store* store); // không sở hữu; nullptr trả về kho mặc định
    static QString defaultFilePath();

    template <class T>
    T get(const Setting<T>& s) const { return Codec<T>::decode(value(QString::fromLatin1(s.key)), s.def); }
    template <class T>
    void set(const Setting<T>& s, const T& v) { setValue(QString::fromLatin1(s.key), Codec<T>::encode(v)); }
    template <class T>
    void reset(const Setting<T>& s) { remove(QString::fromLatin1(s.key)); }

    QJsonValue value(const QString& key) const; // Undefined nếu chưa đặt
    void setValue(const QString& key, const QJsonValue& v); // luồng của Store (luồng UI)
    void remove(const QString& key);
    QStringList keys() const;

    QString filePath() const { return path_; }
    void flush(); // ghi ngay trên luồng gọi (thoát ứng dụng, test)

signals:
    void changed(const QString& key);

private:
    struct Writer; // tệp đích + thế hệ đã ghi; giữ sống bằng shared_ptr cho việc nền
    void scheduleWrite();
    QByteArray serialize() const;

    QString path_;
    mutable std::mutex mutex_;
    QHash<QString, QJsonValue> values_;
    std::shared_ptr<Writer> writer_;
    quint64 generation_ = 0;
    bool writeQueued_ = false;
};

} // namespace trion::settings
