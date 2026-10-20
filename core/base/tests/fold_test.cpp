#include "core/base/text/Fold.h"

#include <QTest>

using namespace Qt::StringLiterals;

using trion::text::containsFolded;
using trion::text::foldForSearch;
using trion::text::startsWithFolded;

class FoldTest : public QObject {
    Q_OBJECT
private slots:
    void foldsDiacriticsAndCase_data()
    {
        QTest::addColumn<QString>("in");
        QTest::addColumn<QString>("out");
        QTest::newRow("lớp") << u"Lớp"_s << u"lop"_s;
        QTest::newRow("ĐƯỜNG") << u"ĐƯỜNG"_s << u"duong"_s;
        QTest::newRow("đường") << u"đường"_s << u"duong"_s;
        QTest::newRow("nfd") << QString(u"đường"_s).normalized(QString::NormalizationForm_D) << u"duong"_s;
        QTest::newRow("chồng tầng") << u"Ấ Ẫ Ự ỹ"_s << u"a a u y"_s;
        QTest::newRow("ascii") << u"LINE_2"_s << u"line_2"_s;
        QTest::newRow("rỗng") << QString() << QString();
    }
    void foldsDiacriticsAndCase()
    {
        QFETCH(QString, in);
        QFETCH(QString, out);
        QCOMPARE(foldForSearch(in), out);
    }

    void matchesFoldedNeedle()
    {
        const QString needle = foldForSearch(u"lop"_s);
        QVERIFY(containsFolded(u"Quản lý Lớp vẽ"_s, needle));
        QVERIFY(startsWithFolded(u"Lớp 0"_s, needle));
        QVERIFY(!startsWithFolded(u"Quản lý Lớp"_s, needle));
        QVERIFY(containsFolded(u"bất kỳ"_s, QString()));
    }
};

QTEST_GUILESS_MAIN(FoldTest)
#include "fold_test.moc"
