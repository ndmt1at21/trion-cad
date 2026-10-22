// UI0-001 với exe trion thật (QProcess, offscreen, --settings tệp tạm): chạy một phiên giữa các tiến trình (tiêu chí
// 1), mốc khởi động (2), thoát khi còn việc nền (4), rảnh (5), phiên chết đột ngột để lại khóa và socket, --help. Exe
// con build cùng sanitizer với test và thừa hưởng môi trường (TSAN_OPTIONS, ASAN_OPTIONS): lỗi TSan, ASan, rò rỉ (LSan)
// làm mã thoát khác 0.

#include "app/main/SingleInstance.h"
#include "tests/timing.h"

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(Q_OS_LINUX)
#include <unistd.h>
#endif

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;
using trion::app::SingleInstance;
using Clock = std::chrono::steady_clock;

namespace {

// Chạy vòng sự kiện đến khi cond đúng hoặc hết hạn (không processEvents(WaitForMoreEvents): treo trên Windows).
bool spinUntil(const std::function<bool()>& cond, std::chrono::milliseconds timeout = 30s)
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

QString trionExe()
{
#ifdef Q_OS_WIN
    return QCoreApplication::applicationDirPath() + u"/trion.exe"_s;
#else
    return QCoreApplication::applicationDirPath() + u"/trion"_s;
#endif
}

// Thư mục chuẩn hóa (bỏ liên kết, tên ngắn 8.3) + tên tệp; tệp không cần tồn tại.
QString canonical(const QString& path)
{
    const QFileInfo fi(path);
    return QDir(fi.absolutePath()).canonicalPath() + u'/' + fi.fileName();
}

QJsonObject readTrace(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(f.readAll()).object() : QJsonObject();
}

std::optional<double> markAt(const QJsonObject& trace, const QString& name)
{
    for (const QJsonValue& m : trace[u"marks"_s].toArray())
        if (m[u"name"_s].toString() == name)
            return m[u"at_ms"_s].toDouble();
    return std::nullopt;
}

// Đường dẫn trong các dòng log "trion.app: openRequested: a, b" (Application::requestOpen) của stderr.
QStringList requestedPaths(const QByteArray& stderrText)
{
    QStringList paths;
    const QString tag = u"openRequested: "_s;
    for (const QString& line : QString::fromUtf8(stderrText).split(u'\n'))
        if (const qsizetype at = line.indexOf(tag); at >= 0)
            for (const QString& p : line.mid(at + tag.size()).trimmed().split(u", "_s))
                paths << canonical(p);
    return paths;
}

// Thời gian CPU (người dùng + hệ thống) của tiến trình, giây; < 0 nếu chưa hỗ trợ nền tảng này.
double cpuSeconds(qint64 pid)
{
#if defined(Q_OS_LINUX)
    QFile f(u"/proc/%1/stat"_s.arg(pid)); // trường 14 utime, 15 stime, tính bằng tick
    if (!f.open(QIODevice::ReadOnly))
        return -1;
    const QByteArray stat = f.readAll();
    const QList<QByteArray> fields = stat.mid(stat.lastIndexOf(')') + 2).split(' '); // từ trường 3
    if (fields.size() < 13)
        return -1;
    return (fields[11].toDouble() + fields[12].toDouble()) / double(sysconf(_SC_CLK_TCK));
#elif defined(Q_OS_WIN)
    const HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, DWORD(pid));
    if (!h)
        return -1;
    FILETIME created, exited, kernel, user;
    const bool ok = GetProcessTimes(h, &created, &exited, &kernel, &user);
    CloseHandle(h);
    const auto seconds = [](FILETIME t) { return double((quint64(t.dwHighDateTime) << 32) | t.dwLowDateTime) * 1e-7; };
    return ok ? seconds(kernel) + seconds(user) : -1;
#else
    Q_UNUSED(pid);
    return -1;
#endif
}

// Số lần luồng chính của tiến trình tự nhường CPU (thức dậy vì timer, sự kiện rồi ngủ lại); chỉ Linux, nơi khác -1.
qint64 mainThreadSwitches(qint64 pid)
{
#if defined(Q_OS_LINUX)
    QFile f(u"/proc/%1/task/%1/status"_s.arg(pid));
    if (!f.open(QIODevice::ReadOnly))
        return -1;
    for (const QByteArray& line : f.readAll().split('\n'))
        if (line.startsWith("voluntary_ctxt_switches:"))
            return line.mid(line.indexOf(':') + 1).trimmed().toLongLong();
    return -1;
#else
    Q_UNUSED(pid);
    return -1;
#endif
}

} // namespace

class TrionExeTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY2(QFileInfo::exists(trionExe()), qPrintable(trionExe()));
        dir_ = std::make_unique<QTemporaryDir>();
        QVERIFY(dir_->isValid());
    }

    void cleanupTestCase() { dir_.reset(); }

    void helpListsOptions()
    {
        const QString settings = path(u"help.json"_s);
        auto help = start({u"--settings"_s, settings, u"--help"_s});
        QVERIFY(exitedWith(*help, 0));
        const QString out = QString::fromUtf8(help->readAllStandardOutput());
        for (const QString& option :
             {u"--settings"_s, u"--startup-trace"_s, u"--ui-probe"_s, u"--quit-after-first-frame"_s, u"--help"_s})
            QVERIFY2(out.contains(option), qPrintable(out));
        auto bad = start({u"--settings"_s, settings, u"--no-such-option"_s});
        QVERIFY(exitedWith(*bad, 1));
        QVERIFY(bad->readAllStandardError().contains("no-such-option"));
    }

    // Tiêu chí 2 và 4: trace đủ mọi pha theo thứ tự; lần tốt nhất của vài lần chạy: sẵn sàng dựng cửa sổ chính
    // ≤ 400 ms, thoát (aboutToQuit → hết main, gồm dừng việc nền đang raster biểu tượng) ≤ 500 ms.
    void startupTraceHasEveryPhase()
    {
        const QStringList expected{u"main"_s,   u"qt"_s,          u"instance"_s, u"settings"_s,      u"i18n"_s,
                                   u"fonts"_s,  u"theme"_s,       u"icons"_s,    u"keymap"_s,        u"ready"_s,
                                   u"window"_s, u"first_frame"_s, u"quit"_s,     u"tasks_stopped"_s, u"exit"_s};
        const QString settings = path(u"warm.json"_s);
        constexpr int kRuns = 5;
        QJsonObject best;
        QStringList readies;
        for (int i = 0; i < kRuns; ++i) {
            const QString trace = path(u"warm-%1.trace.json"_s.arg(i));
            auto p = start({u"--settings"_s, settings, u"--startup-trace"_s, trace, u"--quit-after-first-frame"_s});
            QVERIFY(exitedWith(*p, 0));
            const QJsonObject t = readTrace(trace);
            QCOMPARE(t[u"format"_s].toString(), u"trion-startup-trace/1"_s);
#if defined(Q_OS_LINUX) || defined(Q_OS_WIN)
            QCOMPARE(t[u"origin"_s].toString(), u"process"_s);
#endif
            QStringList names;
            double prev = 0;
            for (const QJsonValue& m : t[u"marks"_s].toArray()) {
                names << m[u"name"_s].toString();
                QVERIFY(m[u"at_ms"_s].toDouble() >= prev);
                prev = m[u"at_ms"_s].toDouble();
            }
            QCOMPARE(names, expected);
            readies << QString::number(*markAt(t, u"ready"_s), 'f', 1);
            if (best.isEmpty() || *markAt(t, u"ready"_s) < *markAt(best, u"ready"_s))
                best = t;
        }
        QStringList phases;
        for (const QJsonValue& m : best[u"marks"_s].toArray())
            phases << u"%1 %2"_s.arg(m[u"name"_s].toString()).arg(m[u"ms"_s].toDouble(), 0, 'f', 1);
        const double ready = *markAt(best, u"ready"_s);
        const double exit = *markAt(best, u"exit"_s) - *markAt(best, u"quit"_s);
        qInfo("khởi động ấm, lần tốt nhất / %d (ms; sẵn sàng các lần: %s): sẵn sàng dựng cửa sổ chính %.1f, khung đầu "
              "%.1f, thoát %.1f; từng pha: %s",
              kRuns, qPrintable(readies.join(u" "_s)), ready, *markAt(best, u"first_frame"_s), exit,
              qPrintable(phases.join(u", "_s)));
        QVERIFY2(!trion::test::kTimed || ready <= 400, qPrintable(u"%1 ms"_s.arg(ready)));
        QVERIFY2(!trion::test::kTimed || exit <= 500, qPrintable(u"%1 ms"_s.arg(exit)));
        // Khởi động ấm đến khung đầu của cửa sổ chính thật (UIA-001) ≤ 1,5 s (DoD-L, 05 §1.1).
        const double firstFrame = *markAt(best, u"first_frame"_s);
        QVERIFY2(!trion::test::kTimed || firstFrame <= 1500, qPrintable(u"%1 ms"_s.arg(firstFrame)));
    }

    // Tiêu chí 1 (lần chạy đầu): `trion a.dwg sub/../b.dxf` yêu cầu mở hai đường dẫn tuyệt đối.
    void firstLaunchRequestsOpen()
    {
        auto p = start(
            {u"a.dwg"_s, u"sub/../b.dxf"_s, u"--settings"_s, path(u"first.json"_s), u"--quit-after-first-frame"_s});
        QVERIFY(exitedWith(*p, 0));
        QCOMPARE(requestedPaths(p->readAllStandardError()),
                 (QStringList{canonical(path(u"a.dwg"_s)), canonical(path(u"b.dxf"_s))}));
    }

    // Tiêu chí 1 (lần chạy sau): test là phiên chính; `trion c.dwg` chuyển đường dẫn tuyệt đối rồi thoát mã 0.
    // Phiên chính nhận ≤ 300 ms tính từ lúc khởi chạy (lần tốt nhất của 3).
    void secondLaunchForwardsToRunningSession()
    {
        const QString settings = path(u"forward.json"_s);
        SingleInstance primary(settings);
        QVERIFY(primary.listen());
        QSignalSpy spy(&primary, &SingleInstance::received);
        double best = 1e9;
        for (int i = 0; i < 3; ++i) {
            const QString file = u"c%1.dwg"_s.arg(i);
            const Clock::time_point t0 = Clock::now();
            auto p = start({file, u"--settings"_s, settings});
            QVERIFY(spinUntil([&] { return spy.count() == i + 1; }));
            best = std::min(best, ms(Clock::now() - t0));
            QVERIFY(exitedWith(*p, 0));
            const QStringList got = spy[i][0].toStringList();
            QCOMPARE(got.size(), 1);
            QCOMPARE(canonical(got[0]), canonical(path(file)));
        }
        qInfo("lần chạy sau → phiên chính nhận đường dẫn: %.1f ms (lần tốt nhất / 3)", best);
        QVERIFY2(!trion::test::kTimed || best <= 300, qPrintable(u"%1 ms"_s.arg(best)));
    }

    // Tiêu chí 1 giữa hai exe: phiên trion đang chạy nhận đường dẫn của lần chạy sau; lần chạy không đường dẫn chỉ đưa
    // cửa sổ lên trước; lần chạy sau thoát mã 0 sau khi phiên chính xác nhận. Mốc "forwarded" của lần chạy sau (tạo
    // tiến trình → nhận xác nhận, gồm cả nạp thư viện của nó) là cận trên của lúc phiên chính nhận; lần tốt nhất của 3.
    void secondLaunchForwardsToRunningTrion()
    {
        const QString settings = path(u"session.json"_s), trace = path(u"session.trace.json"_s);
        auto session = start({u"--settings"_s, settings, u"--startup-trace"_s, trace});
        QVERIFY(spinUntil([&] { return markAt(readTrace(trace), u"first_frame"_s).has_value(); }));
        QByteArray log;
        double best = 1e9;
        QStringList marks;
        for (int i = 0; i < 3; ++i) {
            const QString file = u"d%1.dwg"_s.arg(i), secondTrace = path(u"second-%1.trace.json"_s.arg(i));
            auto second = start({file, u"--settings"_s, settings, u"--startup-trace"_s, secondTrace});
            QVERIFY(exitedWith(*second, 0));
            const QJsonObject t = readTrace(secondTrace);
            const std::optional<double> forwarded = markAt(t, u"forwarded"_s);
            QVERIFY(forwarded.has_value());
            if (*forwarded < best) {
                best = *forwarded;
                marks.clear();
                for (const QJsonValue& m : t[u"marks"_s].toArray())
                    marks << u"%1 %2"_s.arg(m[u"name"_s].toString()).arg(m[u"at_ms"_s].toDouble(), 0, 'f', 1);
            }
            QVERIFY(spinUntil([&] {
                log += session->readAllStandardError();
                return requestedPaths(log).contains(canonical(path(file)));
            }));
        }
        auto third = start({u"--settings"_s, settings});
        QVERIFY(exitedWith(*third, 0));
        spinFor(100ms);
        log += session->readAllStandardError();
        QCOMPARE(requestedPaths(log).size(), 3);       // lần chạy không đường dẫn không yêu cầu mở gì
        QCOMPARE(session->state(), QProcess::Running); // phiên không chết (sanitizer, lỗi)
        stop(*session);
        qInfo("trion → trion: phiên chính xác nhận lúc %.1f ms tính từ lúc tạo tiến trình lần chạy sau (lần tốt nhất / "
              "3; "
              "mốc: %s)",
              best, qPrintable(marks.join(u", "_s)));
        QVERIFY2(!trion::test::kTimed || best <= 300, qPrintable(u"%1 ms"_s.arg(best)));
    }

    // Phiên chết đột ngột (Unix: còn tệp socket): lần chạy sau thành phiên chính, không chuyển cho phiên đã chết.
    void crashedSessionIsReplaced()
    {
        const QString settings = path(u"crash.json"_s), trace = path(u"crash.trace.json"_s);
        auto session = start({u"--settings"_s, settings, u"--startup-trace"_s, trace});
        QVERIFY(spinUntil([&] { return markAt(readTrace(trace), u"first_frame"_s).has_value(); }));
        stop(*session);
#ifdef Q_OS_UNIX
        QVERIFY(QFileInfo::exists(QDir::tempPath() + u'/' + SingleInstance(settings).serverName()));
#endif
        const QString next = path(u"crash-next.trace.json"_s);
        auto p = start({u"--settings"_s, settings, u"--startup-trace"_s, next, u"--quit-after-first-frame"_s});
        QVERIFY(exitedWith(*p, 0));
        QVERIFY(markAt(readTrace(next), u"first_frame"_s).has_value());
    }

    // Tiêu chí 5: rảnh sau khởi động, CPU trung bình ≤ 0,5%; Linux: luồng UI không thức lần nào (0 timer định kỳ).
    // Khoảng đo 30 s ở bản release (bộ test vẫn < 60 s), 5 s ở bản khác; không tính 2 s đầu (raster trước biểu tượng).
    void idleUsesNoCpu()
    {
        const QString trace = path(u"idle.trace.json"_s);
        auto session = start({u"--settings"_s, path(u"idle.json"_s), u"--startup-trace"_s, trace});
        QVERIFY(spinUntil([&] { return markAt(readTrace(trace), u"first_frame"_s).has_value(); }));
        spinFor(2s);
        const qint64 pid = session->processId();
        const double cpu0 = cpuSeconds(pid);
        const qint64 switches0 = mainThreadSwitches(pid);
        const Clock::time_point t0 = Clock::now();
        spinFor(trion::test::kTimed ? 30s : 5s);
        const double cpu1 = cpuSeconds(pid);
        const qint64 switches1 = mainThreadSwitches(pid);
        const double seconds = std::chrono::duration<double>(Clock::now() - t0).count();
        QCOMPARE(session->state(), QProcess::Running);
        stop(*session);
        if (cpu0 < 0)
            QSKIP("chưa đọc được thời gian CPU của tiến trình trên nền tảng này");
        const double percent = (cpu1 - cpu0) / seconds * 100;
        qInfo("rảnh %.1f s sau khởi động: CPU %.2f%% (%.0f ms), luồng UI tự nhường CPU %s lần", seconds, percent,
              (cpu1 - cpu0) * 1000,
              switches0 < 0 ? "(chỉ đo trên Linux)" : qPrintable(QString::number(switches1 - switches0)));
        // Sanitizer có luồng nền riêng (TSan) và thay malloc: chỉ báo số.
        QVERIFY2(trion::test::kSanitized || percent <= 0.5, qPrintable(u"%1%"_s.arg(percent)));
        if (switches0 >= 0)
            QVERIFY2(trion::test::kSanitized || switches1 == switches0, qPrintable(u"%1"_s.arg(switches1 - switches0)));
    }

private:
    QString path(const QString& name) const { return dir_->filePath(name); }

    // trion với môi trường của test (offscreen), log "trion.app" mức info ra stderr (cả bản GUI trên Windows); thư mục
    // hiện tại là thư mục tạm của test.
    std::unique_ptr<QProcess> start(const QStringList& args) const
    {
        auto p = std::make_unique<QProcess>();
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(u"QT_QPA_PLATFORM"_s, u"offscreen"_s);
        env.insert(u"QT_LOGGING_RULES"_s, u"trion.app.info=true"_s);
        env.insert(u"QT_FORCE_STDERR_LOGGING"_s, u"1"_s);
        p->setProcessEnvironment(env);
        p->setWorkingDirectory(dir_->path());
        p->start(trionExe(), args);
        return p;
    }

    // Chờ tiến trình thoát với mã `code`; không đúng (mã khác, chết bất thường, quá hạn) thì in stderr để biết vì sao.
    static bool exitedWith(QProcess& p, int code)
    {
        const bool done = spinUntil([&] { return p.state() == QProcess::NotRunning; });
        if (done && p.exitStatus() == QProcess::NormalExit && p.exitCode() == code)
            return true;
        qWarning("trion: %s, mã thoát %d; stderr:\n%s", done ? "đã thoát" : "quá hạn", p.exitCode(),
                 p.readAllStandardError().constData());
        return false;
    }

    static void stop(QProcess& p)
    {
        p.kill();
        spinUntil([&] { return p.state() == QProcess::NotRunning; });
    }

    std::unique_ptr<QTemporaryDir> dir_;
};

QTEST_MAIN(TrionExeTest)
#include "trion_exe_test.moc"
