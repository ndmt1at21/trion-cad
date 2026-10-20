#include "app/core/keymap/Keymap.h"

#include "core/base/assert.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QRegularExpression>

#include <algorithm>
#include <initializer_list>
#include <utility>

using namespace Qt::StringLiterals;

namespace trion::keymap {
namespace {

// id → ["Ctrl+K", …]; chỉ hành động có phím khác mặc định.
const settings::Setting<QJsonObject> kOverrides{"keymap/overrides", {}};

QStringList strings(const QJsonValue& v)
{
    QStringList out;
    for (const QJsonValue& e : v.toArray())
        if (e.isString())
            out << e.toString();
    return out;
}

QHash<QString, QString> table(const QJsonValue& v)
{
    QHash<QString, QString> out;
    const QJsonObject o = v.toObject();
    for (auto it = o.begin(); it != o.end(); ++it)
        out.insert(it.key(), it.value().toString());
    return out;
}

// Các bảng của prototype trong keymap.json, đọc một lần cho cả tiến trình, sau đó chỉ đọc.
struct Data {
    QList<Action> defaults;
    QHash<QString, QString> reserved;  // RESERVED: tổ hợp → bên giữ
    QHash<QString, QString> macSystem; // MACSYS: tổ hợp → nhãn macOS
    QRegularExpression macCommandKeys; // CMDKEYS: phím hệ thống dùng ⌘
};

const Data& data()
{
    static const Data d = [] {
        QFile f(u":/keymap/keymap.json"_s);
        TRION_CHECK(f.open(QIODevice::ReadOnly)); // resource nhúng lúc build: thiếu là lỗi build
        const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
        Data d{{}, table(o["reserved"_L1]), table(o["macSystem"_L1]),
               QRegularExpression(o["macCommandKeys"_L1].toString())};
        for (const QJsonValue& v : o["actions"_L1].toArray()) {
            const QJsonObject j = v.toObject();
            Action a;
            a.id = j["id"_L1].toString();
            a.group = j["group"_L1].toString();
            a.name = j["name"_L1].toString();
            a.command = j["command"_L1].toString();
            a.keys = a.defaultKeys = strings(j["keys"_L1]);
            a.scope = j["scope"_L1].toString() == "viewport"_L1 ? Scope::Viewport : Scope::Global;
            a.macAlt = j["macAlt"_L1].toString();
            d.defaults << a;
        }
        TRION_CHECK(!d.defaults.isEmpty() && d.macCommandKeys.isValid());
        return d;
    }();
    return d;
}

// Tên phím → Qt::Key. Qt gọi PgDn là "PgDown", Delete là "Del"; ký hiệu là của nhãn macOS (MACSYS).
Qt::Key keyFromName(const QString& name)
{
    static const QHash<QString, Qt::Key> extra{
        {u"Delete"_s, Qt::Key_Delete}, {u"PgDn"_s, Qt::Key_PageDown}, {u"⌫"_s, Qt::Key_Backspace},
        {u"→"_s, Qt::Key_Right},       {u"←"_s, Qt::Key_Left},        {u"↑"_s, Qt::Key_Up},
        {u"↓"_s, Qt::Key_Down}};
    if (const auto it = extra.constFind(name); it != extra.cend())
        return *it;
    const QKeySequence s = QKeySequence::fromString(name, QKeySequence::PortableText);
    return s.count() == 1 && s[0].keyboardModifiers() == Qt::NoModifier ? s[0].key() : Qt::Key_unknown;
}

// "Ctrl+Alt+Shift+K" → phím Qt theo nghĩa Windows/Linux.
// ponytail: tách theo '+', nên phím '+' ("Ctrl++") không đọc được, giống TC.keyLabel; thêm khi cần phím zoom.
QKeyCombination fromPortable(const QString& combo)
{
    QStringList parts = combo.split(u'+');
    const Qt::Key key = keyFromName(parts.takeLast());
    Qt::KeyboardModifiers m;
    for (const QString& p : parts) {
        if (p == "Ctrl"_L1)
            m |= Qt::ControlModifier;
        else if (p == "Alt"_L1)
            m |= Qt::AltModifier;
        else if (p == "Shift"_L1)
            m |= Qt::ShiftModifier;
        else
            return {};
    }
    return key == Qt::Key_unknown ? QKeyCombination() : QKeyCombination(m, key);
}

// Nhãn macOS ("⇧⌘Z", "⌃F", "⌫") → phím Qt trên macOS: ⌘ là Qt::ControlModifier, ⌃ là Qt::MetaModifier.
QKeyCombination fromMacLabel(const QString& label)
{
    Qt::KeyboardModifiers m;
    qsizetype i = 0;
    for (; i < label.size(); ++i) {
        const char16_t c = label[i].unicode();
        if (c == u'⌃')
            m |= Qt::MetaModifier;
        else if (c == u'⌥')
            m |= Qt::AltModifier;
        else if (c == u'⇧')
            m |= Qt::ShiftModifier;
        else if (c == u'⌘')
            m |= Qt::ControlModifier;
        else
            break;
    }
    const Qt::Key key = keyFromName(label.mid(i));
    return key == Qt::Key_unknown ? QKeyCombination() : QKeyCombination(m, key);
}

} // namespace

Platform currentPlatform()
{
#if defined(Q_OS_MACOS)
    return Platform::Mac;
#elif defined(Q_OS_WIN)
    return Platform::Windows;
#else
    return Platform::Linux;
#endif
}

QString keyLabel(const QString& keys, Platform p)
{
    if (keys.isEmpty() || p != Platform::Mac)
        return keys;
    const Data& d = data();
    QStringList out;
    for (QString k : keys.split(u" · "_s)) {
        k = k.trimmed();
        if (const auto it = d.macSystem.constFind(k); it != d.macSystem.cend()) {
            out << *it;
            continue;
        }
        QStringList mods = k.split(u'+');
        const QString key = mods.takeLast();
        const bool cmd = d.macCommandKeys.match(k).hasMatch();
        out << (mods.contains("Ctrl"_L1) && !cmd ? u"⌃"_s : QString()) +
                   (mods.contains("Alt"_L1) ? u"⌥"_s : QString()) + (mods.contains("Shift"_L1) ? u"⇧"_s : QString()) +
                   (cmd ? u"⌘"_s : QString()) + key;
    }
    return out.join(u" · "_s);
}

QKeyCombination keyCombination(const QString& combo, Platform p)
{
    const QKeyCombination pc = fromPortable(combo);
    if (p != Platform::Mac)
        return pc;
    // macOS nhận cả nhãn macOS (phím thay thế "⌃F" của KDEF).
    return pc.key() == Qt::Key_unknown ? fromMacLabel(combo) : fromMacLabel(keyLabel(combo, p));
}

Keymap::Keymap(settings::Store& store, Platform platform, QObject* parent)
    : QObject(parent), store_(store), platform_(platform)
{
    connect(&store_, &settings::Store::changed, this, [this](const QString& key) {
        if (key == QLatin1StringView(kOverrides.key))
            reload();
    });
    reload();
}

const Action* Keymap::action(const QString& id) const
{
    for (const Action& a : actions_)
        if (a.id == id)
            return &a;
    return nullptr;
}

const Action* Keymap::actionFor(QKeyCombination key) const
{
    Qt::KeyboardModifiers m = key.keyboardModifiers();
    m.setFlag(Qt::KeypadModifier, false);
    const QKeyCombination k(m, key.key() == Qt::Key_Backtab ? Qt::Key_Tab : key.key());
    const auto it = lookup_.constFind(k.toCombined());
    return it == lookup_.cend() ? nullptr : &actions_.at(*it);
}

Rejection Keymap::assign(const QString& id, const QString& input)
{
    const QKeyCombination pc = keyCombination(input, Platform::Windows);
    if (id.isEmpty() || pc.key() == Qt::Key_unknown)
        return {Rejection::Invalid, {}, {}};
    // Về dạng chuẩn Ctrl+Alt+Shift+<phím>: nhãn và phím macOS (CMDKEYS) phụ thuộc thứ tự này.
    const Qt::KeyboardModifiers m = pc.keyboardModifiers();
    const QString combo = (m & Qt::ControlModifier ? u"Ctrl+"_s : QString()) +
                          (m & Qt::AltModifier ? u"Alt+"_s : QString()) +
                          (m & Qt::ShiftModifier ? u"Shift+"_s : QString()) + input.section(u'+', -1);
    if (const auto it = data().reserved.constFind(combo); it != data().reserved.cend())
        return {Rejection::Reserved, *it, {}};
    // Phím đơn (chữ, số, Delete…) dành cho gõ lệnh trên vùng vẽ; gõ tắt kiểu "L" đặt ở Viết tắt lệnh.
    if (!(m & (Qt::ControlModifier | Qt::AltModifier)) && !(pc.key() >= Qt::Key_F1 && pc.key() <= Qt::Key_F35))
        return {Rejection::BareKey, {}, {}};
    // Trùng trên Windows/Linux hoặc trên macOS đều từ chối, để bảng phím mang sang máy khác vẫn đúng.
    // ponytail: quét mọi phím × 2 nền tảng mỗi lần gán (đo ở test lookupUnder10us), chỉ khi người dùng gán.
    for (const Platform p : {Platform::Windows, Platform::Mac}) {
        const QKeyCombination want = keyCombination(combo, p);
        for (const Action& a : std::as_const(actions_))
            if (a.id != id)
                for (const QString& k : a.keys)
                    if (keyCombination(k, p) == want)
                        return {Rejection::Taken, a.name, a.id};
    }
    const Action* a = action(id);
    QStringList keys = a ? a->keys : QStringList();
    if (!keys.contains(combo)) {
        keys << combo;
        write(id, keys);
    }
    return {};
}

void Keymap::unassign(const QString& id, const QString& combo)
{
    const Action* a = action(id);
    if (!a || !a->keys.contains(combo))
        return;
    QStringList keys = a->keys;
    keys.removeAll(combo);
    write(id, keys);
}

void Keymap::reset(const QString& id)
{
    const Action* a = action(id);
    write(id, a ? a->defaultKeys : QStringList());
}

void Keymap::resetAll()
{
    store_.reset(kOverrides);
}

// Ghi phím của id vào Store; bằng mặc định thì xóa ghi đè. Store báo changed → reload().
void Keymap::write(const QString& id, const QStringList& keys)
{
    const Action* a = action(id);
    QJsonObject o = store_.get(kOverrides);
    if (keys == (a ? a->defaultKeys : QStringList()))
        o.remove(id);
    else
        o.insert(id, QJsonArray::fromStringList(keys));
    if (o.isEmpty())
        store_.reset(kOverrides);
    else
        store_.set(kOverrides, o);
}

void Keymap::reload()
{
    actions_ = data().defaults;
    const QJsonObject o = store_.get(kOverrides);
    for (auto it = o.begin(); it != o.end(); ++it) { // khóa QJsonObject đã sắp xếp: lệnh khác theo id
        if (!it.value().isArray())
            continue; // tệp sửa tay sai kiểu: giữ mặc định
        auto a = std::find_if(actions_.begin(), actions_.end(),
                              [&](const Action& x) { return x.id == it.key(); });
        if (a == actions_.end()) {
            // ponytail: lệnh ngoài KDEF lấy tên lệnh làm tên; tên thật và kiểm tồn tại lấy từ registry INT-001.
            Action x;
            x.id = x.name = x.command = it.key();
            x.group = QString::fromUtf8(QT_TRANSLATE_NOOP("keymap", "Lệnh khác"));
            a = actions_.insert(actions_.end(), x);
        }
        a->keys = strings(it.value());
    }

    // Phím chính giữ trước, rồi phím thay thế macOS lấy phím còn trống. Trùng (chỉ có khi tệp sửa tay)
    // thì hành động đứng trước giữ: tất định.
    lookup_.clear();
    const auto bind = [this](qsizetype i, QKeyCombination kc) {
        if (kc.key() == Qt::Key_unknown)
            return;
        const int c = kc.toCombined();
        if (!lookup_.contains(c))
            lookup_.insert(c, i);
        if (lookup_.value(c) == i && !actions_[i].bindings.contains(kc))
            actions_[i].bindings << kc;
    };
    for (qsizetype i = 0; i < actions_.size(); ++i)
        for (const QString& k : std::as_const(actions_[i].keys))
            bind(i, keyCombination(k, platform_));
    if (platform_ == Platform::Mac)
        for (qsizetype i = 0; i < actions_.size(); ++i)
            if (!actions_[i].macAlt.isEmpty())
                bind(i, fromMacLabel(actions_[i].macAlt));
    emit changed();
}

} // namespace trion::keymap
