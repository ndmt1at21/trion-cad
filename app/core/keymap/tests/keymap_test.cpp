// Test UI0-114: bảng phím tắt một nguồn (app/core/keymap/Keymap.h).
// golden.json sinh từ prototype bằng keymap_gen.mjs: "kdef" là các dòng KDEF nguyên văn, "keyLabelMac" là
// [đầu vào, kết quả TC.keyLabel trên macOS].
#include "app/core/keymap/Keymap.h"

#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace trion::keymap;
using trion::settings::Store;

namespace {

QJsonObject golden()
{
    QFile f(u":/tests/golden.json"_s);
    return f.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(f.readAll()).object() : QJsonObject();
}

QKeyCombination kc(Qt::KeyboardModifiers m, Qt::Key k)
{
    return QKeyCombination(m, k);
}

constexpr auto C = Qt::ControlModifier, S = Qt::ShiftModifier, A = Qt::AltModifier, M = Qt::MetaModifier;

} // namespace

class KeymapTest : public QObject {
    Q_OBJECT
    QTemporaryDir dir_;
    QString path(const char* name) const { return dir_.filePath(QString::fromLatin1(name)); }

private slots:
    // Tiêu chí 1: 100% dòng KDEF có trong keymap.json, so từng trường qua API Keymap.
    void everyKdefRowLoaded()
    {
        const QJsonArray kdef = golden()["kdef"_L1].toArray();
        QVERIFY(kdef.size() >= 40);
        Store s(path("kdef.json"));
        Keymap km(s, Platform::Windows);
        QCOMPARE(km.platform(), Platform::Windows);
        QCOMPARE(km.actions().size(), kdef.size());
        for (qsizetype i = 0; i < kdef.size(); ++i) {
            const QJsonArray r = kdef[i].toArray();
            const Action& a = km.actions()[i];
            QVERIFY2(r.size() == 6 || r.size() == 7, qPrintable(a.id));
            QCOMPARE(a.id, r[0].toString());
            QCOMPARE(a.group, r[1].toString());
            QCOMPARE(a.name, r[2].toString());
            QCOMPARE(a.command, r[3].toString());
            QCOMPARE(a.defaultKeys.join(u" · "_s), r[4].toString());
            QCOMPARE(a.keys, a.defaultKeys);
            QCOMPARE(a.scope == Scope::Viewport ? u"Vùng vẽ"_s : u"Toàn cục"_s, r[5].toString());
            QCOMPARE(a.macAlt, r.size() > 6 ? r[6].toString() : QString());
            QCOMPARE(km.action(a.id), &a);
        }
        QVERIFY(!km.action(u"khong-co"_s));
    }

    // Tiêu chí 1, vế sau: mọi lệnh trong keymap có trong registry INT-001.
    void commandsExistInRegistry()
    {
        // ponytail: hoãn vì registry lệnh INT-001 chưa có. Khi có: mọi Action::command khác "—" phải có
        // trong registry (ca này chạy trong ctest -L keymap của CI).
        QSKIP("hoãn: registry lệnh INT-001 chưa có");
    }

    // Phím mặc định không trùng nhau trên Windows, Linux, macOS và tra ra đúng hành động của nó.
    void defaultsRouteToTheirAction()
    {
#if defined(Q_OS_LINUX)
        QCOMPARE(currentPlatform(), Platform::Linux);
#endif
        Store s(path("defaults.json"));
        for (const Platform p : {Platform::Windows, Platform::Linux, Platform::Mac}) {
            Keymap km(s, p);
            for (const Action& a : km.actions())
                for (const QString& k : a.keys) {
                    const QKeyCombination c = keyCombination(k, p);
                    QVERIFY2(c.key() != Qt::Key_unknown, qPrintable(k));
                    QVERIFY2(km.actionFor(c) == &a, qPrintable(a.id + u' ' + k));
                    QVERIFY(a.bindings.contains(c));
                }
        }
    }

    // Tiêu chí 2: phím trùng hoặc dành riêng bị từ chối kèm tên chủ cũ; không đổi gì, không phát tín hiệu.
    void assignRejectsWithOwner()
    {
        Store s(path("reject.json"));
        Keymap km(s, Platform::Windows);
        QSignalSpy spy(&km, &Keymap::changed);

        Rejection r = km.assign(u"osnap"_s, u"Ctrl+Space"_s);
        QCOMPARE(r.reason, Rejection::Reserved);
        QCOMPARE(r.owner, u"bộ gõ (đổi kiểu gõ tiếng Việt)"_s);
        QCOMPARE(km.assign(u"osnap"_s, u"Alt+F4"_s).owner, u"Windows"_s);
        QCOMPARE(km.assign(u"osnap"_s, u"Ctrl+H"_s).owner, u"macOS (ẩn ứng dụng)"_s);
        QCOMPARE(km.assign(u"osnap"_s, u"Alt+Ctrl+Delete"_s).owner, u"Windows"_s); // thứ tự phụ tùy ý

        r = km.assign(u"osnap"_s, u"Ctrl+S"_s);
        QCOMPARE(r.reason, Rejection::Taken);
        QCOMPARE(r.owner, u"Lưu"_s);
        QCOMPARE(r.ownerId, u"save"_s);
        // Chỉ trùng sau khi đổi sang phím macOS: Ctrl+Shift+Z và Ctrl+Y (Làm lại) đều là ⇧⌘Z.
        r = km.assign(u"help"_s, u"Ctrl+Shift+Z"_s);
        QCOMPARE(r.reason, Rejection::Taken);
        QCOMPARE(r.owner, u"Làm lại"_s);

        QCOMPARE(km.assign(u"osnap"_s, u"K"_s).reason, Rejection::BareKey);
        QCOMPARE(km.assign(u"osnap"_s, u"Shift+Delete"_s).reason, Rejection::BareKey);
        QCOMPARE(km.assign(u"osnap"_s, u"Ctrl+Foo"_s).reason, Rejection::Invalid);
        QCOMPARE(km.assign(QString(), u"Ctrl+K"_s).reason, Rejection::Invalid);
        QVERIFY(!km.assign(u"save"_s, u"Ctrl+S"_s)); // phím của chính nó: nhận, không đổi gì

        QCOMPARE(spy.count(), 0);
        QVERIFY(s.keys().isEmpty());
        QCOMPARE(km.action(u"osnap"_s)->keys, QStringList{u"F3"_s});
    }

    // Tiêu chí 2: ghi đè sống qua khởi động lại (Store mới cùng tệp); "Đặt lại" trả mặc định.
    void overridesSurviveRestartAndReset()
    {
        const QString file = path("restart.json");
        {
            Store s(file);
            Keymap km(s, Platform::Windows);
            QVERIFY(!km.assign(u"osnap"_s, u"Shift+Ctrl+F"_s)); // lưu dạng chuẩn Ctrl+Shift+F
            // "Gán lại cho lệnh này": gỡ khỏi chủ cũ rồi gán.
            const Rejection r = km.assign(u"ortho"_s, u"Ctrl+S"_s);
            QCOMPARE(r.ownerId, u"save"_s);
            km.unassign(r.ownerId, u"Ctrl+S"_s);
            QVERIFY(!km.assign(u"ortho"_s, u"Ctrl+S"_s));
            QVERIFY(!km.assign(u"LINE"_s, u"Ctrl+Alt+L"_s)); // lệnh ngoài KDEF
        } // Store ghi nốt khi hủy
        Store s(file);
        Keymap km(s, Platform::Windows);
        const Action* osnap = km.action(u"osnap"_s);
        QCOMPARE(osnap->keys, (QStringList{u"F3"_s, u"Ctrl+Shift+F"_s}));
        QCOMPARE(km.actionFor(kc(C | S, Qt::Key_F)), osnap);
        QVERIFY(km.action(u"save"_s)->keys.isEmpty());
        QCOMPARE(km.actionFor(kc(C, Qt::Key_S))->id, u"ortho"_s);
        const Action* line = km.action(u"LINE"_s);
        QVERIFY(line);
        QCOMPARE(line->group, u"Lệnh khác"_s);
        QCOMPARE(line->command, u"LINE"_s);
        QCOMPARE(km.actionFor(kc(C | A, Qt::Key_L)), line);

        QSignalSpy spy(&km, &Keymap::changed);
        km.reset(u"osnap"_s);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(km.action(u"osnap"_s)->keys, QStringList{u"F3"_s});
        QVERIFY(!km.actionFor(kc(C | S, Qt::Key_F)));
        km.resetAll();
        QCOMPARE(spy.count(), 2);
        QCOMPARE(km.actions().size(), golden()["kdef"_L1].toArray().size()); // lệnh ngoài KDEF hết phím: bỏ
        QCOMPARE(km.actionFor(kc(C, Qt::Key_S))->id, u"save"_s);
        QVERIFY(!s.keys().contains(u"keymap/overrides"_s));
    }

    // Tệp cài đặt sửa tay: sai kiểu thì bỏ qua; trùng phím thì hành động đứng trước giữ (tất định).
    void handEditedOverrides()
    {
        Store s(path("hand.json"));
        s.setValue(u"keymap/overrides"_s,
                   QJsonObject{{u"osnap"_s, u"Ctrl+K"_s},
                               {u"grid"_s, QJsonArray{u"Ctrl+Shift+G"_s, 5}},
                               {u"help"_s, QJsonArray{u"Ctrl+S"_s}}});
        Keymap km(s, Platform::Windows);
        QCOMPARE(km.action(u"osnap"_s)->keys, QStringList{u"F3"_s});
        QCOMPARE(km.action(u"grid"_s)->keys, QStringList{u"Ctrl+Shift+G"_s});
        QCOMPARE(km.actionFor(kc(C, Qt::Key_S))->id, u"help"_s);
        QVERIFY(km.action(u"save"_s)->bindings.isEmpty());
    }

    // Tiêu chí 3: keyLabel macOS khớp 100% ca vàng sinh từ TC.keyLabel của prototype;
    // Windows/Linux giữ nguyên.
    void keyLabelMatchesPrototype()
    {
        const QJsonArray cases = golden()["keyLabelMac"_L1].toArray();
        QVERIFY(cases.size() >= 400);
        int bad = 0;
        for (const QJsonValue& v : cases) {
            const QString in = v[0].toString(), want = v[1].toString(), got = keyLabel(in, Platform::Mac);
            if (got != want && ++bad <= 5)
                qWarning("keyLabel(%s) = %s, prototype %s", qUtf8Printable(in), qUtf8Printable(got),
                         qUtf8Printable(want));
            QCOMPARE(keyLabel(in, Platform::Windows), in);
            QCOMPARE(keyLabel(in, Platform::Linux), in);
        }
        qInfo("keyLabel macOS: %lld/%lld ca vàng khớp", qlonglong(cases.size() - bad),
              qlonglong(cases.size()));
        QCOMPARE(bad, 0);
        // Ca của 07.1 §13.6 viết tay, phòng ca vàng sinh sai.
        QCOMPARE(keyLabel(u"Ctrl+S"_s, Platform::Mac), u"⌘S"_s);
        QCOMPARE(keyLabel(u"Ctrl+L"_s, Platform::Mac), u"⌃L"_s);
        QCOMPARE(keyLabel(u"Ctrl+Y"_s, Platform::Mac), u"⇧⌘Z"_s);
        QCOMPARE(keyLabel(u"Delete"_s, Platform::Mac), u"⌫"_s);
        QCOMPARE(keyLabel(u"F5 · Ctrl+E"_s, Platform::Mac), u"F5 · ⌃E"_s);
    }

    // Phím Qt thật đúng như nhãn: macOS ⌘ = Qt::ControlModifier, ⌃ = Qt::MetaModifier.
    void keyCombinationPerPlatform()
    {
        QCOMPARE(keyCombination(u"Ctrl+Shift+S"_s, Platform::Windows), kc(C | S, Qt::Key_S));
        QCOMPARE(keyCombination(u"Ctrl+PgDn"_s, Platform::Linux), kc(C, Qt::Key_PageDown));
        QCOMPARE(keyCombination(u"Delete"_s, Platform::Windows), kc(Qt::NoModifier, Qt::Key_Delete));
        QCOMPARE(keyCombination(u"Ctrl+S"_s, Platform::Mac), kc(C, Qt::Key_S));
        QCOMPARE(keyCombination(u"Ctrl+L"_s, Platform::Mac), kc(M, Qt::Key_L));
        QCOMPARE(keyCombination(u"Ctrl+Y"_s, Platform::Mac), kc(S | C, Qt::Key_Z));
        QCOMPARE(keyCombination(u"Ctrl+PgDn"_s, Platform::Mac), kc(A | C, Qt::Key_Right));
        QCOMPARE(keyCombination(u"Delete"_s, Platform::Mac), kc(Qt::NoModifier, Qt::Key_Backspace));
        QCOMPARE(keyCombination(u"Ctrl+Shift+Tab"_s, Platform::Mac), kc(M | S, Qt::Key_Tab));
        QCOMPARE(keyCombination(u"⌃⌘H"_s, Platform::Mac), kc(M | C, Qt::Key_H));
        for (const char* bad : {"", "Ctrl+Foo", "Meta+X", "F5 · Ctrl+E"})
            QCOMPARE(keyCombination(QString::fromUtf8(bad), Platform::Windows).key(), Qt::Key_unknown);
    }

    // Tiêu chí 4: mỗi toggle F3–F12 có phím thay thế trên macOS theo KDEF, và phím đó chạy đúng hành động.
    void macAltForFKeyToggles()
    {
        Store s(path("macalt.json"));
        Keymap mac(s, Platform::Mac), pc(s, Platform::Windows);
        static const QRegularExpression fKey(u"^F([3-9]|1[0-2])$"_s);
        int toggles = 0;
        for (const Action& a : mac.actions()) {
            if (!a.defaultKeys.filter(fKey).isEmpty()) {
                ++toggles;
                QVERIFY2(!a.macAlt.isEmpty(), qPrintable(a.id));
            }
            if (a.macAlt.isEmpty())
                continue;
            const QKeyCombination alt = keyCombination(a.macAlt, Platform::Mac);
            QVERIFY2(alt.key() != Qt::Key_unknown, qPrintable(a.macAlt));
            QVERIFY2(mac.actionFor(alt) == &a, qPrintable(a.id));
            QVERIFY(a.bindings.contains(alt));
        }
        // F3 OSNAP, F5 ISOPLANE, F7 GRID, F8 ORTHO, F9 SNAP, F10 POLAR, F11 OTRACK, F12 DYN (07.1 §11)
        QCOMPARE(toggles, 8);
        QCOMPARE(mac.actionFor(kc(M, Qt::Key_F))->id, u"osnap"_s);
        QVERIFY(!pc.actionFor(kc(C, Qt::Key_F))); // Windows/Linux không có phím thay thế
    }

    // Tiêu chí 5: đổi keymap phát changed() tới mọi nơi nghe, kể cả Keymap khác dùng chung Store.
    void changedReachesEveryListener()
    {
        Store s(path("signal.json"));
        Keymap editor(s, Platform::Windows), menus(s, Platform::Windows);
        QSignalSpy spy(&menus, &Keymap::changed);
        QVERIFY(!editor.assign(u"dyn"_s, u"Ctrl+Shift+D"_s));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(menus.actionFor(kc(C | S, Qt::Key_D))->id, u"dyn"_s);
        editor.unassign(u"dyn"_s, u"Ctrl+Shift+D"_s);
        QCOMPARE(spy.count(), 2);
        editor.unassign(u"dyn"_s, u"Ctrl+Shift+D"_s); // không còn phím đó: không phát
        editor.reset(u"dyn"_s);                         // đang là mặc định: không phát
        QCOMPARE(spy.count(), 2);
    }

    // Tiêu chí 5: tra phím → hành động ≤ 10 µs; nhận nguyên phím từ QKeyEvent.
    void lookupUnder10us()
    {
        Store s(path("perf.json"));
        Keymap km(s, Platform::Windows);
        QCOMPARE(km.actionFor(kc(C | S, Qt::Key_Backtab))->id, u"ntab"_s);
        QCOMPARE(km.actionFor(kc(C | Qt::KeypadModifier, Qt::Key_1))->id, u"props"_s);
        QList<QKeyCombination> keys;
        for (const Action& a : km.actions())
            keys << a.bindings;
        keys << kc(C, Qt::Key_K) << kc(Qt::NoModifier, Qt::Key_L); // chưa gán

        constexpr int kN = 200000;
        qsizetype hits = 0;
        QElapsedTimer t;
        t.start();
        for (int i = 0; i < kN; ++i)
            hits += km.actionFor(keys[i % keys.size()]) != nullptr;
        const double ns = double(t.nsecsElapsed()) / kN;
        qInfo("tra phím: %.1f ns/lần (%d lần, %lld phím)", ns, kN, qlonglong(keys.size()));
        QVERIFY(hits > 0 && hits < kN); // dùng kết quả để vòng lặp không bị bỏ
        QVERIFY2(ns <= 10000.0, qPrintable(QString::number(ns)));

        // Gán phím (thao tác hiếm, trên luồng UI): kiểm trùng 2 nền tảng + ghi Store + dựng lại bảng. Chỉ báo số.
        t.restart();
        QVERIFY(!km.assign(u"osnap"_s, u"Ctrl+Shift+F"_s));
        qInfo("gán phím: %.0f µs", double(t.nsecsElapsed()) / 1000);
    }
};

QTEST_MAIN(KeymapTest)
#include "keymap_test.moc"
