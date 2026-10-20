#include "core/base/error.h"

#include <QTest>

namespace {
int g_fired = 0;
void countingHandler(const char*, const char*, int) { ++g_fired; }
} // namespace

class ErrorTest : public QObject {
    Q_OBJECT
private slots:
    void expectedHoldsValueOrError()
    {
        trion::Expected<int> ok = 42;
        QVERIFY(ok);
        QCOMPARE(*ok, 42);
        QCOMPARE(ok.value_or(7), 42);

        trion::Expected<int> bad = trion::Error{"ui", "comma", "Dùng dấu chấm"};
        QVERIFY(!bad);
        QCOMPARE(bad.error().code, std::string("comma"));
        QCOMPARE(bad.value_or(7), 7);

        trion::Expected<void> v;
        QVERIFY(v);
        trion::Expected<void> e = trion::Error{"ui", "x", ""};
        QVERIFY(!e);
        QCOMPARE(e.error().module, std::string("ui"));
    }

    void checkCallsHandlerAndAssertFollowsNdebug()
    {
        auto old = trion::setAssertHandler(&countingHandler);
        g_fired = 0;
        TRION_CHECK(1 + 1 == 3);
        QCOMPARE(g_fired, 1);
        TRION_ASSERT(false);
#ifdef NDEBUG
        QCOMPARE(g_fired, 1);
#else
        QCOMPARE(g_fired, 2);
#endif
        trion::setAssertHandler(old);
    }
};

QTEST_GUILESS_MAIN(ErrorTest)
#include "error_test.moc"
