#pragma once
// UIH-001 · In chạy nền (07.1 §12; tiêu chí 5): một việc của NT-013 (BackgroundJobs) mỗi lần In, tên "Đang in 1/1",
// loại "plot", chủ là document (đóng bản vẽ hủy việc in). Thanh trạng thái (UIA-009 qua ProgressPresenter) tự hiện
// "Đang in 1/1 · p%" và nút hủy từ mốc 2 s. Xong: toast "Đã in ra <tệp>" kèm Mở thư mục (thiết bị tệp) hoặc
// "Đã gửi n tờ tới <máy in>"; hủy: toast "Đã hủy in."; lỗi: toast lỗi của ToastHost::followJobs. Toast qua
// ToastHost::instance() (không có thì bỏ). Giữ thiết lập của lần In gần nhất cho <Lần in trước>.
//
// Dùng (integration, một cho ứng dụng, sau BackgroundJobs và ToastHost):
//   PlotRunner plots(jobs);                                // jobs: BackgroundJobs của ứng dụng
//   new PlotDialog(context, &plots, mainWindow);           // bấm In: plots.start(…)
//
// ponytail: pipeline mặc định là công việc giả (fakePipeline: tiến trình đều trong kFakeDuration, hủy kiểm mỗi 10 ms,
// không ghi tệp, không gửi máy in); PLT-003 (PlotStream) + PLT-105 (PDF, ghi tệp tạm rồi đổi tên: hủy không để tệp dở)
// + PLT-005 (QPrinter) thay qua setPipeline. Toast xong còn trùng câu chung "… — xong" của followJobs khi việc ≥ 2 s
// (UIA-015 event_map thay câu chung theo loại việc).

#include "app/dialogs/plot/PlotSettings.h"
#include "core/base/jobs/BackgroundJobs.h"

#include <QObject>

#include <chrono>
#include <functional>
#include <map>
#include <optional>

namespace trion::ui {

class PlotRunner : public QObject {
    Q_OBJECT
public:
    // Chạy trên pool nền: in một tờ theo s, báo progress 0..1, kiểm cancel ít nhất mỗi 10 ms; lỗi trả trion::Error.
    using Pipeline = std::function<Expected<void>(const plot::PlotSettings& s, jobs::Progress& progress,
                                                  sched::CancelToken cancel)>;
    static constexpr std::chrono::milliseconds kFakeDuration{3000};

    explicit PlotRunner(jobs::BackgroundJobs& jobs, QObject* parent = nullptr);

    static Pipeline fakePipeline(std::chrono::milliseconds duration = kFakeDuration);
    void setPipeline(Pipeline pipeline);
    // "Mở thư mục" của toast; mặc định QDesktopServices::openUrl. Test thay.
    void setFolderOpener(std::function<void(const QString& dir)> opener);

    // Bắt đầu in nền; trả id việc của NT-013. Ghi s.setup làm <Lần in trước>.
    jobs::JobId start(const plot::PlotSettings& s, std::uint64_t owner = 0);
    const std::optional<plot::PageSetup>& previous() const { return previous_; }

signals:
    void finished(trion::jobs::JobId id, trion::jobs::State state);

private:
    void onBatch(const std::vector<jobs::JobInfo>& batch);

    jobs::BackgroundJobs& jobs_;
    Pipeline pipeline_;
    std::function<void(const QString&)> openFolder_;
    std::optional<plot::PageSetup> previous_;
    std::map<jobs::JobId, plot::PlotSettings> running_;
};

} // namespace trion::ui
