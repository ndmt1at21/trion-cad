#include "app/dialogs/plot/PlotRunner.h"

#include "app/widgets/message/Messages.h"

#include <QDesktopServices>
#include <QFileInfo>
#include <QUrl>

#include <thread>

using namespace Qt::StringLiterals;

namespace trion::ui {

PlotRunner::PlotRunner(jobs::BackgroundJobs& jobs, QObject* parent)
    : QObject(parent), jobs_(jobs), pipeline_(fakePipeline()),
      openFolder_([](const QString& dir) { QDesktopServices::openUrl(QUrl::fromLocalFile(dir)); })
{
    connect(&jobs_, &jobs::BackgroundJobs::updated, this, &PlotRunner::onBatch);
}

PlotRunner::Pipeline PlotRunner::fakePipeline(std::chrono::milliseconds duration)
{
    return [duration](const plot::PlotSettings&, jobs::Progress& progress, sched::CancelToken cancel) -> Expected<void> {
        using Clock = std::chrono::steady_clock;
        constexpr std::chrono::milliseconds kStep{10}; // DoD-H 5: hủy ≤ 50 ms
        const Clock::time_point start = Clock::now();
        for (;;) {
            if (cancel.cancelled())
                return {};
            const auto done = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start);
            if (done >= duration)
                break;
            progress.set(double(done.count()) / double(duration.count()));
            std::this_thread::sleep_for(std::min(kStep, duration - done));
        }
        progress.set(1);
        return {};
    };
}

void PlotRunner::setPipeline(Pipeline pipeline)
{
    pipeline_ = pipeline ? std::move(pipeline) : fakePipeline();
}

void PlotRunner::setFolderOpener(std::function<void(const QString&)> opener)
{
    openFolder_ = std::move(opener);
}

jobs::JobId PlotRunner::start(const plot::PlotSettings& s, std::uint64_t owner)
{
    previous_ = s.setup;
    const jobs::JobId id = jobs_.start(tr("Đang in %1/%2").arg(1).arg(1), u"plot"_s, owner,
                                       [pipeline = pipeline_, s](jobs::Progress& p, sched::CancelToken cancel) {
                                           return pipeline(s, p, cancel);
                                       });
    running_[id] = s; // lô báo kết thúc tới sau, trên luồng UI
    return id;
}

void PlotRunner::onBatch(const std::vector<jobs::JobInfo>& batch)
{
    for (const jobs::JobInfo& job : batch) {
        const auto it = running_.find(job.id);
        if (job.state == jobs::State::Running || it == running_.end())
            continue;
        const plot::PlotSettings s = std::move(it->second);
        running_.erase(it);
        if (ToastHost* toasts = ToastHost::instance()) {
            if (job.state == jobs::State::Done && s.toFile) {
                const QFileInfo file(s.file);
                toasts->post(Severity::Ok, tr("Đã in ra %1").arg(file.fileName()),
                             {tr("Mở thư mục"), [open = openFolder_, dir = file.absolutePath()] {
                                  if (open)
                                      open(dir);
                              }});
            } else if (job.state == jobs::State::Done) {
                toasts->post(Severity::Ok, tr("Đã gửi %n tờ tới %1", nullptr, s.copies).arg(s.setup.device));
            } else if (job.state == jobs::State::Cancelled) {
                toasts->post(Severity::Warning, tr("Đã hủy in."));
            }
        }
        emit finished(job.id, job.state);
    }
}

} // namespace trion::ui
