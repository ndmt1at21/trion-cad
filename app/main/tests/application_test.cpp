// UI0-001 trong tiến trình test: StartupTrace (mốc, khung đầu, JSON), Application (thứ tự khởi động, thoát hủy việc nền
// hợp tác, đưa cửa sổ lên trước, rảnh không sự kiện, dòng lệnh), SingleInstance (khóa theo tệp cài đặt, giao thức, khóa
// và socket của phiên đã chết). Exe trion thật (lần chạy sau, trace, CPU rảnh): trion_exe_test.cpp.

#include "app/core/display/DisplayContext.h"
#include "app/core/i18n/Translator.h"
#include "app/core/perf/UiProbe.h"
#include "app/main/Application.h"
#include "app/main/SingleInstance.h"
#include "app/main/StartupTrace.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "core/base/sched/Scheduler.h"
#include "core/base/settings/Settings.h"
#include "tests/timing.h"

#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMainWindow>
#include <QRegularExpression>
#include <QScreen>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QWidget>

#include <atomic>
#include <cstring>
#include <functional>
#include <memory>
#include <thread>

#ifdef Q_OS_UNIX
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;
using trion::app::Application;
using trion::app::Options;
using trion::app::SingleInstance;
using trion::app::StartupTrace;
using Clock = std::chrono::steady_clock;

namespace {

// Chạy vòng sự kiện đến khi cond đúng hoặc hết hạn (không processEvents(WaitForMoreEvents): treo trên Windows).
bool spinUntil(const std::function<bool()>& cond, std::chrono::milliseconds timeout = 10s)
{
    if (cond())
        return true;
    QEventLoop loop;
    QTimer tick;
    QObject::connect(&tick, &QTimer::timeout, &loop, [&] {
        if (cond())
            loop.quit();
    });
    QTimer::singleShot(timeout, &loop, &QEventLoop::quit);
    tick.start(1);
    loop.exec();
    return cond();
}

void spinFor(std::chrono::milliseconds duration)
{
    QEventLoop loop;
    QTimer::singleShot(duration, &loop, &QEventLoop::quit);
    loop.exec();
}

double ms(Clock::duration d)
{
    return std::chrono::duration<double, std::milli>(d).count();
}

QStringList markNames(const StartupTrace& trace)
{
    QStringList names;
    for (const QJsonValue& m : trace.toJson()[u"marks"_s].toArray())
        names << m[u"name"_s].toString();
    return names;
}

QByteArray message(const QStringList& paths)
{
    return QJsonDocument(QJsonArray::fromStringList(paths)).toJson(QJsonDocument::Compact) + '\n';
}

std::unique_ptr<QLocalSocket> connectTo(const QString& serverName)
{
    auto socket = std::make_unique<QLocalSocket>();
    socket->connectToServer(serverName);
    if (!socket->waitForConnected(5000))
        return nullptr;
    return socket;
}

// Ghi lại mọi sự kiện giao trong tiến trình, trừ sự kiện tới `ignore` (timer đo của test).
class EventLog : public QObject {
public:
    explicit EventLog(const QObject* ignore) : ignore_(ignore) {}
    QStringList seen;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched != ignore_) {
            QString s;
            QDebug(&s).nospace() << event->type() << " -> " << watched->metaObject()->className();
            seen << s;
        }
        return false;
    }

private:
    const QObject* ignore_;
};

class PaintedWidget : public QWidget {
public:
    bool painted = false;

protected:
    void paintEvent(QPaintEvent*) override { painted = true; }
};

} // namespace

class ApplicationTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        dir_ = std::make_unique<QTemporaryDir>();
        QVERIFY(dir_->isValid());
    }

    void cleanupTestCase()
    {
        trion::sched::waitIdle();
        dir_.reset();
    }

    // Mốc theo thứ tự đặt, JSON đúng định dạng, ghi qua tệp tạm rồi đổi tên.
    void traceMarksAndJson()
    {
        StartupTrace trace;
        trace.mark(u"qt"_s);
        trace.mark(u"settings"_s);
        QCOMPARE(markNames(trace), (QStringList{u"main"_s, u"qt"_s, u"settings"_s}));
        QVERIFY(trace.at(u"qt"_s).has_value());
        QVERIFY(!trace.at(u"window"_s).has_value());

        const QJsonObject j = trace.toJson();
        QCOMPARE(j[u"format"_s].toString(), u"trion-startup-trace/1"_s);
#if defined(Q_OS_LINUX) || defined(Q_OS_WIN)
        QCOMPARE(j[u"origin"_s].toString(), u"process"_s); // mốc 0 là lúc tạo tiến trình test
#endif
        double prev = 0;
        for (const QJsonValue& m : j[u"marks"_s].toArray()) {
            const double at = m[u"at_ms"_s].toDouble();
            QVERIFY(at >= prev);
            QVERIFY(qAbs(m[u"ms"_s].toDouble() - (at - prev)) <= 0.11); // làm tròn 0,1 ms
            prev = at;
        }

        QVERIFY(trace.write()); // chưa đặt tệp: không làm gì
        const QString path = dir_->filePath(u"trace.json"_s);
        trace.setOutput(path);
        QVERIFY(trace.write());
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(f.readAll()).object(), trace.toJson());
        trace.setOutput(dir_->filePath(u"no/such/dir/trace.json"_s));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u"không ghi được"_s));
        QVERIFY(!trace.write());
        trace.setOutput({});
    }

    // first_frame đặt ngay sau khi khung đầu đã vẽ, một lần; WarmStart vào UiProbe.
    void traceFirstFrameAfterPaint()
    {
        trion::ui::probe::enable();
        trion::ui::probe::reset();
        StartupTrace trace;
        PaintedWidget w;
        w.resize(320, 200);
        w.show();
        trace.watchFirstFrame(&w);
        int frames = 0;
        bool paintedBefore = false;
        connect(&trace, &StartupTrace::firstFrame, this, [&] {
            paintedBefore = w.painted;
            ++frames;
        });
        QVERIFY(spinUntil([&] { return frames == 1; }));
        QVERIFY(paintedBefore);
        QVERIFY(*trace.at(u"first_frame"_s) >= *trace.at(u"main"_s));
        w.update();
        spinFor(100ms);
        QCOMPARE(frames, 1);
        QCOMPARE(markNames(trace).count(u"first_frame"_s), 1);
        const QJsonObject ops = trion::ui::probe::toJson()[u"ops"_s].toObject();
        QCOMPARE(ops[u"warm_start"_s][u"count"_s].toInt(), 1);
        trion::ui::probe::disable();
    }

    // Thứ tự khởi động của main (UI0-001 đầu ra), mỗi phần sống suốt đời Application; hủy: token bị hủy, mốc thoát.
    void startsInOrderAndStops()
    {
        StartupTrace trace;
        const Options o = options(u"start"_s);
        trion::sched::CancelToken token;
        {
            Application app(o, trace);
            QCOMPARE(Application::instance(), &app);
            QCOMPARE(trion::settings::Store::instance().filePath(), o.settingsPath);
            QVERIFY(trion::i18n::Translator::instance());
            (void)trion::theme::ThemeManager::instance(); // chưa có thì TRION_CHECK dừng test
            (void)trion::theme::IconProvider::instance();
            QCOMPARE(trion::ui::DisplayContext::instance().screenClass(),
                     trion::ui::DisplayContext::classify(app.mainWindow()->screen()->geometry().width()));
            QVERIFY(app.mainWindow()->isVisible());
            token = Application::cancelToken();
            QVERIFY(!token.cancelled());
            QVERIFY(spinUntil([&] { return trace.at(u"first_frame"_s).has_value(); }));
            QCOMPARE(markNames(trace),
                     (QStringList{u"main"_s, u"settings"_s, u"i18n"_s, u"fonts"_s, u"theme"_s, u"icons"_s, u"keymap"_s,
                                  u"ready"_s, u"window"_s, u"first_frame"_s}));
        }
        QCOMPARE(Application::instance(), nullptr);
        QVERIFY(token.cancelled());
        QVERIFY(!Application::cancelToken().cancelled()); // không có Application: token không hủy
        QCOMPARE(markNames(trace).last(), u"tasks_stopped"_s);
    }

    // Tiêu chí 4: thoát khi còn việc nền chạy mãi đến khi bị hủy: hủy hợp tác, chờ dừng hết rồi mới hủy executor.
    void quitCancelsBackgroundWork()
    {
        StartupTrace trace;
        auto app = std::make_unique<Application>(options(u"quit"_s), trace);
        QVERIFY(spinUntil([&] { return trace.at(u"first_frame"_s).has_value(); }));
        constexpr int kTasks = 16; // nhiều hơn số worker: có việc còn chờ trong hàng lúc thoát
        std::atomic<int> started = 0, stopped = 0;
        const trion::sched::CancelToken token = Application::cancelToken();
        for (int i = 0; i < kTasks; ++i)
            trion::sched::submit(trion::sched::Priority::Background, [token, &started, &stopped] {
                started.fetch_add(1);
                while (!token.cancelled())
                    std::this_thread::sleep_for(1ms);
                stopped.fetch_add(1);
            });
        QVERIFY(spinUntil([&] { return started.load() > 0; }));
        const Clock::time_point t0 = Clock::now();
        app.reset();
        const double took = ms(Clock::now() - t0);
        QCOMPARE(stopped.load(), kTasks);
        qInfo("thoát khi còn %d việc nền: %.1f ms", kTasks, took);
        QVERIFY2(!trion::test::kTimed || took <= 500, qPrintable(u"%1 ms"_s.arg(took)));
    }

    // Lần chạy sau (hoặc dòng lệnh): cửa sổ chính bỏ thu nhỏ, lên trước, openRequested mang đúng danh sách.
    void requestOpenRaisesWindow()
    {
        StartupTrace trace;
        Application app(options(u"raise"_s), trace);
        QSignalSpy spy(&app, &Application::openRequested);
        QMainWindow* w = app.mainWindow();
        w->showMinimized();
        QVERIFY(w->isMinimized());
        const QStringList paths{dir_->filePath(u"a.dwg"_s), dir_->filePath(u"b.dxf"_s)};
        app.requestOpen(paths);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy[0][0].toStringList(), paths);
        QVERIFY(!w->isMinimized());
        QVERIFY(w->isVisible());
        QVERIFY(spinUntil([&] { return w->isActiveWindow(); }));
        app.requestOpen({}); // không đường dẫn: chỉ đưa cửa sổ lên trước
        QCOMPARE(spy.count(), 1);
    }

    // Tiêu chí 5 (phần trong tiến trình): rảnh sau khởi động không có sự kiện nào (timer định kỳ, polling, việc gửi
    // về).
    void idleHasNoEvents()
    {
        StartupTrace trace;
        Application app(options(u"idle"_s), trace);
        QVERIFY(spinUntil([&] { return trace.at(u"first_frame"_s).has_value(); }));
        trion::sched::waitIdle(); // raster trước bộ biểu tượng xong ở nền
        spinFor(200ms);           // kết quả của nó đã về luồng UI
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete); // timer của spinFor tự hủy
        QTimer end;
        end.setSingleShot(true);
        EventLog log(&end);
        qApp->installEventFilter(&log);
        QEventLoop loop;
        connect(&end, &QTimer::timeout, &loop, &QEventLoop::quit);
        end.start(1s);
        loop.exec();
        qApp->removeEventFilter(&log);
        QVERIFY2(log.seen.isEmpty(), qPrintable(log.seen.join(u", "_s)));
    }

    // Đường dẫn tuyệt đối theo thư mục hiện tại; --ui-probe=<tệp> (của UiProbe) không làm parser báo lỗi.
    void parseOptionsMakesPathsAbsolute()
    {
        const QString old = QDir::currentPath();
        QVERIFY(QDir::setCurrent(dir_->path()));
        const QString cwd = QDir::currentPath();
        const Options o = Application::parseOptions({u"trion"_s, u"a.dwg"_s, u"sub/../b.dxf"_s, u"--settings"_s,
                                                     u"s.json"_s, u"--startup-trace"_s, u"t.json"_s,
                                                     u"--ui-probe=p.json"_s, u"--quit-after-first-frame"_s});
        QVERIFY(QDir::setCurrent(old));
        QCOMPARE(o.paths, (QStringList{cwd + u"/a.dwg"_s, cwd + u"/b.dxf"_s}));
        QCOMPARE(o.settingsPath, cwd + u"/s.json"_s);
        QCOMPARE(o.tracePath, u"t.json"_s);
        QVERIFY(o.quitAfterFirstFrame);

        const Options d = Application::parseOptions({u"trion"_s, u"--ui-probe"_s});
        QVERIFY(d.paths.isEmpty());
        QCOMPARE(d.settingsPath, trion::settings::Store::defaultFilePath());
        QVERIFY(d.tracePath.isEmpty());
        QVERIFY(!d.quitAfterFirstFrame);
    }

    // Khóa theo tệp cài đặt (cùng tệp viết khác nhau: cùng khóa).
    void instanceKeyPerSettingsFile()
    {
        const QString a = dir_->filePath(u"a/user.json"_s);
        const QString name = SingleInstance(a).serverName();
        QVERIFY(name.startsWith(u"trion-"_s));
        QCOMPARE(SingleInstance(dir_->filePath(u"a/../a/./user.json"_s)).serverName(), name);
        QVERIFY(SingleInstance(dir_->filePath(u"b/user.json"_s)).serverName() != name);
    }

    // Một phiên chính cho mỗi tệp cài đặt (kể cả hai lần trong cùng tiến trình); phiên chính thoát thì lần sau giành
    // được.
    void onePrimaryPerSettingsFile()
    {
        const QString settings = dir_->filePath(u"one.json"_s);
        auto first = std::make_unique<SingleInstance>(settings);
        QVERIFY(first->listen());
        QVERIFY(first->listen()); // gọi lại: vẫn là phiên chính
        SingleInstance second(settings);
        QVERIFY(!second.listen());
        QVERIFY(!second.listen());
        SingleInstance other(dir_->filePath(u"other.json"_s));
        QVERIFY(other.listen()); // tệp cài đặt khác: phiên khác
        first.reset();           // phiên chính thoát: ngừng nghe, thả khóa
        QVERIFY(second.listen());
    }

    // Phiên chính ghép đủ dòng (gửi làm hai phần), phát received đúng danh sách, đáp "\n".
    void primaryReceivesAndAcks()
    {
        SingleInstance primary(dir_->filePath(u"recv.json"_s));
        QVERIFY(primary.listen());
        QSignalSpy spy(&primary, &SingleInstance::received);
        const QStringList paths{u"/x/a.dwg"_s, u"/x/b c.dxf"_s, u"/x/bản vẽ \"1\"\n.dwg"_s};
        const QByteArray msg = message(paths);
        const auto client = connectTo(primary.serverName());
        QVERIFY(client);
        client->write(msg.left(7));
        client->flush();
        spinFor(50ms);
        QCOMPARE(spy.count(), 0);
        client->write(msg.mid(7));
        QVERIFY(spinUntil([&] { return spy.count() == 1; }));
        QCOMPARE(spy[0][0].toStringList(), paths);
        QVERIFY(spinUntil([&] { return client->bytesAvailable() > 0; }));
        QCOMPARE(client->readAll(), "\n");
    }

    // Thông điệp sai hoặc quá dài: đóng kết nối, không phát; sau đó vẫn nhận thông điệp đúng.
    void primaryDropsMalformedMessages()
    {
        SingleInstance primary(dir_->filePath(u"bad.json"_s));
        QVERIFY(primary.listen());
        QSignalSpy spy(&primary, &SingleInstance::received);
        const QList<QByteArray> bad{"not json\n", "{\"a\": 1}\n", "[\"/x/a.dwg\", 2]\n",
                                    QByteArray(SingleInstance::kMaxMessage + 1, 'x')};
        for (const QByteArray& msg : bad) {
            const auto client = connectTo(primary.serverName());
            QVERIFY(client);
            client->write(msg);
            QVERIFY2(spinUntil([&] { return client->state() == QLocalSocket::UnconnectedState; }), msg.left(20));
        }
        QCOMPARE(spy.count(), 0);
        const auto client = connectTo(primary.serverName());
        QVERIFY(client);
        client->write(message({u"/x/ok.dwg"_s}));
        QVERIFY(spinUntil([&] { return spy.count() == 1; }));
    }

    // Lần chạy sau: phiên chính không đáp → false đúng hạn; không còn phiên chính → giành khóa, listen() thành phiên
    // chính.
    void forwardFailsWithoutAck()
    {
        SingleInstance secondary(dir_->filePath(u"mute.json"_s));
        QLocalServer mute; // nghe đúng tên, không bao giờ đáp
        QVERIFY(mute.listen(secondary.serverName()));
        Clock::time_point t0 = Clock::now();
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u"không xác nhận"_s));
        QVERIFY(!secondary.forward({u"/x/a.dwg"_s}, 300ms));
        const double waited = ms(Clock::now() - t0);
        QVERIFY2(waited >= 290 && waited < 5000, qPrintable(u"%1 ms"_s.arg(waited)));
        mute.close();
        t0 = Clock::now();
        QVERIFY(!secondary.forward({u"/x/a.dwg"_s}, 5s));
        QVERIFY(Clock::now() - t0 < 4s); // không chờ hết hạn: khóa trống thì giành ngay
        QVERIFY(secondary.listen());
        QSignalSpy spy(&secondary, &SingleInstance::received);
        const auto client = connectTo(secondary.serverName());
        QVERIFY(client);
        client->write(message({u"/x/b.dwg"_s}));
        QVERIFY(spinUntil([&] { return spy.count() == 1; }));
    }

    // Phiên chết để lại tệp socket (Unix): phiên chính mới dọn rồi nghe được.
    void staleSocketIsCleaned()
    {
#ifdef Q_OS_UNIX
        SingleInstance primary(dir_->filePath(u"stale.json"_s));
        const QByteArray path = QFile::encodeName(QDir::tempPath() + u'/' + primary.serverName());
        const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        QVERIFY(fd >= 0);
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        QVERIFY(size_t(path.size()) < sizeof addr.sun_path);
        std::memcpy(addr.sun_path, path.constData(), size_t(path.size()));
        QCOMPARE(::bind(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof addr), 0);
        ::close(fd); // tệp socket còn, không ai nghe
        QVERIFY(QFileInfo::exists(QFile::decodeName(path)));
        QVERIFY(primary.listen());
        QSignalSpy spy(&primary, &SingleInstance::received);
        const auto client = connectTo(primary.serverName());
        QVERIFY(client);
        client->write(message({u"/x/a.dwg"_s}));
        QVERIFY(spinUntil([&] { return spy.count() == 1; }));
#else
        QSKIP("Windows: named pipe mất cùng tiến trình, không để lại gì");
#endif
    }

private:
    Options options(const QString& name) const
    {
        Options o;
        o.settingsPath = dir_->filePath(name + u".json"_s);
        return o;
    }

    std::unique_ptr<QTemporaryDir> dir_;
};

QTEST_MAIN(ApplicationTest)
#include "application_test.moc"
