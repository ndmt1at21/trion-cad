#include "app/main/StartupTrace.h"

#include "app/core/perf/UiProbe.h"
#include "core/base/assert.h"
#include "core/base/sched/Scheduler.h"

#include <QCoreApplication>
#include <QEvent>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QWidget>
#include <QWindow>

#include <cmath>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(Q_OS_LINUX)
#include <time.h>
#include <unistd.h>
#endif

using namespace Qt::StringLiterals;

namespace trion::app {

namespace {

using Clock = StartupTrace::Clock;

// Tuổi của tiến trình theo hệ điều hành; không đọc được thì nullopt.
std::optional<Clock::duration> processAge()
{
#if defined(Q_OS_WIN)
    FILETIME created, exited, kernel, user, now;
    if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user))
        return std::nullopt;
    GetSystemTimePreciseAsFileTime(&now);
    const auto ticks = [](FILETIME t) { return qint64((quint64(t.dwHighDateTime) << 32) | t.dwLowDateTime); }; // 100 ns
    return std::chrono::duration_cast<Clock::duration>(std::chrono::nanoseconds((ticks(now) - ticks(created)) * 100));
#elif defined(Q_OS_LINUX)
    // Trường 22 của /proc/self/stat: lúc tạo tiến trình, tính bằng tick (1/CLK_TCK s) từ lúc khởi động máy, theo
    // CLOCK_BOOTTIME. Trường 2 (tên lệnh, trong ngoặc) có thể chứa dấu cách nên đếm từ dấu ')' cuối.
    QFile f(u"/proc/self/stat"_s);
    if (!f.open(QIODevice::ReadOnly))
        return std::nullopt;
    const QByteArray stat = f.readAll();
    const QList<QByteArray> fields = stat.mid(stat.lastIndexOf(')') + 2).split(' '); // từ trường 3
    bool ok = false;
    const double start = fields.size() > 19 ? fields[19].toDouble(&ok) / double(sysconf(_SC_CLK_TCK)) : 0;
    timespec now{};
    if (!ok || clock_gettime(CLOCK_BOOTTIME, &now) != 0)
        return std::nullopt;
    const std::chrono::duration<double> age(double(now.tv_sec) + double(now.tv_nsec) * 1e-9 - start);
    return std::chrono::duration_cast<Clock::duration>(age);
#else
    // ponytail: macOS (sysctl KERN_PROC_PID → kp_proc.p_starttime) làm ở mốc macOS; tới lúc đó mốc 0 là lúc vào main.
    return std::nullopt;
#endif
}

double ms(Clock::duration d)
{
    return std::round(std::chrono::duration<double, std::milli>(d).count() * 10) / 10;
}

// Qua tệp tạm rồi đổi tên (QSaveFile, có fsync): người đọc không bao giờ thấy tệp dở.
bool save(const QString& path, const QByteArray& json)
{
    QSaveFile f(path);
    if (f.open(QIODevice::WriteOnly) && f.write(json) >= 0 && f.commit())
        return true;
    qWarning("trion startup-trace: không ghi được %s", qUtf8Printable(path));
    return false;
}

} // namespace

StartupTrace::StartupTrace()
{
    const Clock::time_point now = Clock::now();
    const std::optional<Clock::duration> age = processAge();
    // Âm hoặc quá một phút: đồng hồ hệ thống vừa đổi, hoặc không dựng ở đầu main() (test).
    fromProcess_ = age && *age >= Clock::duration::zero() && *age < std::chrono::minutes(1);
    origin_ = fromProcess_ ? now - *age : now;
    marks_.append({u"main"_s, now});
}

StartupTrace::~StartupTrace()
{
    mark(u"exit"_s);
    write();
}

void StartupTrace::setOutput(const QString& jsonPath)
{
    path_ = jsonPath;
}

void StartupTrace::mark(const QString& name)
{
    marks_.append({name, Clock::now()});
}

void StartupTrace::watchFirstFrame(QWidget* window)
{
    QWindow* handle = window->windowHandle();
    TRION_CHECK(handle && "watchFirstFrame: gọi sau show()");
    handle->installEventFilter(this);
}

bool StartupTrace::eventFilter(QObject* watched, QEvent* event)
{
    // Khung đầu: lần giao Expose đầu tiên khi cửa sổ đã hiện; QWidgetWindow vẽ và flush ngay trong lần giao đó. Tự giao
    // ở đây (như UiProbe) để biết đúng lúc giao xong.
    if (event->type() != QEvent::Expose || !static_cast<QWindow*>(watched)->isExposed())
        return false;
    watched->removeEventFilter(this);
    QCoreApplication::sendEvent(watched, event);
    mark(u"first_frame"_s);
    ui::probe::record(ui::probe::Op::WarmStart, marks_.last().second - origin_);
    if (!path_.isEmpty()) // fsync ở nền, không trên luồng UI (DoD-U 4); lần ghi cuối (hàm hủy) chạy sau khi việc nền
                          // dừng
        sched::submit(sched::Priority::Background,
                      [path = path_, json = QJsonDocument(toJson()).toJson()] { save(path, json); });
    emit firstFrame();
    return true;
}

std::optional<double> StartupTrace::at(const QString& name) const
{
    for (const auto& [n, t] : marks_)
        if (n == name)
            return ms(t - origin_);
    return std::nullopt;
}

QJsonObject StartupTrace::toJson() const
{
    QJsonArray marks;
    Clock::time_point prev = origin_;
    for (const auto& [name, t] : marks_) {
        marks.append(QJsonObject{{u"name"_s, name}, {u"at_ms"_s, ms(t - origin_)}, {u"ms"_s, ms(t - prev)}});
        prev = t;
    }
    return {{u"format"_s, u"trion-startup-trace/1"_s},
            {u"origin"_s, fromProcess_ ? u"process"_s : u"main"_s},
            {u"marks"_s, marks}};
}

bool StartupTrace::write() const
{
    return path_.isEmpty() || save(path_, QJsonDocument(toJson()).toJson());
}

} // namespace trion::app
