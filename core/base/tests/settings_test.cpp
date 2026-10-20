#include "core/base/settings/Settings.h"

#include "core/base/sched/Scheduler.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;

using namespace trion::settings;

namespace {
const Setting<QString> kTheme{"ui/theme", "dark"};
constexpr Setting<int> kCount{"ui/count", 3};
constexpr Setting<bool> kLite{"ui/lite", false};
const Setting<QStringList> kRecent{"ui/recent", {}};
const Setting<QByteArray> kState{"ui/state", {}};
} // namespace

class SettingsTest : public QObject {
    Q_OBJECT
private slots:
    void defaultsThenPersistsAcrossReload()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(u"sub/user.json"_s);
        {
            Store s(path);
            QCOMPARE(s.get(kTheme), u"dark"_s);
            QCOMPARE(s.get(kCount), 3);
            s.set(kTheme, u"light"_s);
            s.set(kCount, 9);
            s.set(kLite, true);
            s.set(kRecent, QStringList{u"a.dwg"_s, u"Bản vẽ.dwg"_s});
            s.set(kState, QByteArray("\x00\x01\xff", 3));
            QCOMPARE(s.get(kTheme), u"light"_s);
        } // hủy: ghi nốt
        Store r(path);
        QCOMPARE(r.get(kTheme), u"light"_s);
        QCOMPARE(r.get(kCount), 9);
        QCOMPARE(r.get(kLite), true);
        QCOMPARE(r.get(kRecent), (QStringList{u"a.dwg"_s, u"Bản vẽ.dwg"_s}));
        QCOMPARE(r.get(kState), QByteArray("\x00\x01\xff", 3));
        r.reset(kTheme);
        QCOMPARE(r.get(kTheme), u"dark"_s);
    }

    void manySetsCoalesceIntoBackgroundWrite()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(u"user.json"_s);
        Store s(path);
        for (int i = 0; i < 10000; ++i)
            s.set(kCount, i);
        QVERIFY(!QFile::exists(path)); // chưa ghi đồng bộ trên luồng UI
        QCoreApplication::processEvents();
        trion::sched::waitIdle();
        QVERIFY(QFile::exists(path));
        QCOMPARE(Store(path).get(kCount), 9999);
    }

    void changedSignalOnlyOnRealChange()
    {
        QTemporaryDir dir;
        Store s(dir.filePath(u"user.json"_s));
        QSignalSpy spy(&s, &Store::changed);
        s.set(kTheme, u"light"_s);
        s.set(kTheme, u"light"_s);
        s.reset(kTheme);
        s.reset(kTheme);
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(0).at(0).toString(), u"ui/theme"_s);
    }

    void corruptFileKeptAsBadAndWrongTypeFallsBack()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(u"user.json"_s);
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{ not json");
        f.close();
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u"tệp hỏng"_s));
        Store s(path);
        QVERIFY(QFile::exists(path + u".bad"_s));
        QCOMPARE(s.get(kTheme), u"dark"_s);

        s.setValue(u"ui/count"_s, u"không phải số"_s);
        QCOMPARE(s.get(kCount), 3);
    }

    void instanceCanBeReplaced()
    {
        QTemporaryDir dir;
        Store s(dir.filePath(u"user.json"_s));
        Store::setInstance(&s);
        QCOMPARE(&Store::instance(), &s);
        Store::setInstance(nullptr);
        QVERIFY(&Store::instance() != &s);
    }
};

QTEST_GUILESS_MAIN(SettingsTest)
#include "settings_test.moc"
