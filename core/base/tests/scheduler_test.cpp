#include "core/base/sched/Scheduler.h"

#include <QCoreApplication>
#include <QTest>
#include <QThread>

#include <atomic>
#include <thread>

using namespace trion::sched;

class SchedulerTest : public QObject {
    Q_OBJECT
private slots:
    void runsSubmittedWorkOffTheCallingThread()
    {
        std::atomic<int> runs{0};
        std::atomic<bool> offThread{true};
        const auto self = std::this_thread::get_id();
        for (int i = 0; i < 1000; ++i)
            submit(Priority::Normal, [&] {
                if (std::this_thread::get_id() == self)
                    offThread = false;
                ++runs;
            });
        waitIdle();
        QCOMPARE(runs.load(), 1000);
        QVERIFY(offThread.load());
    }

    void cancelTokenSeesCancel()
    {
        CancelSource src;
        CancelToken tok = src.token();
        QVERIFY(!tok.cancelled());
        std::atomic<int> seen{0};
        submit(Priority::Background, [tok, &seen] {
            while (!tok.cancelled())
                std::this_thread::yield();
            seen = 1;
        });
        src.cancel();
        waitIdle();
        QCOMPARE(seen.load(), 1);
        QVERIFY(CancelToken().cancelled() == false);
    }

    void postToUiUsesInstalledPoster()
    {
        QThread* ui = QThread::currentThread();
        setUiPoster([](std::function<void()> fn) {
            QMetaObject::invokeMethod(QCoreApplication::instance(), std::move(fn), Qt::QueuedConnection);
        });
        std::atomic<bool> onUi{false};
        submit(Priority::Normal, [&] { postToUi([&] { onUi = QThread::currentThread() == ui; }); });
        waitIdle();
        QTRY_VERIFY(onUi.load());
        setUiPoster({});
    }
};

QTEST_GUILESS_MAIN(SchedulerTest)
#include "scheduler_test.moc"
