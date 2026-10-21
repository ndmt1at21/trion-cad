#pragma once
// NT-013 · Đăng ký tác vụ nền: tiến trình, hủy, kết quả, thông báo. Tập con đủ cho UI0-112; dùng chung cho thanh
// trạng thái (UIA-009), toast (UI0-009), hộp thoại, và việc dài của PLT-008, VN-006, DOC-019.
//
// Dùng (trên luồng UI; start/cancel gọi được từ mọi luồng):
//   jobs::BackgroundJobs registry;   // một cái cho cả ứng dụng (UI0-001 giữ, instance()); test tự tạo
//   jobs::JobId id = registry.start(tr("Đang mở"), u"open"_s, docKey,
//       [path](jobs::Progress& p, sched::CancelToken cancel) -> trion::Expected<void> {
//           jobs::Progress read = p.child(0.7), regen = p.child(0.3);   // phân cấp có trọng số
//           for (int i = 0; i < n; ++i) {
//               if (cancel.cancelled())
//                   return {};                                           // thành Cancelled
//               ...
//               read.set(double(i + 1) / n);
//           }
//           if (bad)
//               return trion::Error{"dwg", "corrupt", "…"};              // thành Failed
//           return {};                                                   // thành Done
//       });
//   connect(&registry, &jobs::BackgroundJobs::updated, this, [](const std::vector<jobs::JobInfo>& changed) { … });
//   registry.cancelOwner(docKey);  // đóng document: hủy mọi việc của nó
//
// Hợp đồng
//  - Đối tượng tạo và hủy trên luồng UI, cần QCoreApplication và poster của trion::sched::postToUi (UI0-101
//    UiExecutor). Hủy đối tượng thì yêu cầu hủy mọi việc; việc đang chạy vẫn giữ trạng thái chung của registry
//    đến khi fn trả về. Gỡ poster (hủy UiExecutor) chỉ sau khi việc nền đã dừng (sched::waitIdle()).
//  - fn chạy trên pool NT-005 (sched::submit, ưu tiên Background), không chạm widget. Việc tôn trọng CancelToken
//    nên kiểm nó ít nhất mỗi 10 ms (DoD-H 5: hủy ≤ 50 ms). Đã yêu cầu hủy thì fn trả gì cũng thành Cancelled;
//    fn trả Error thành Failed; fn ném ngoại lệ thành Failed với mã "jobs/exception".
//  - Việc tính là đang chạy từ lúc start() (kể cả lúc còn chờ trong hàng đợi của pool). Hủy việc còn chờ: thành
//    Cancelled ngay trong cancel(), fn không chạy.
//  - Báo về luồng UI: tín hiệu updated() theo lô, chỉ gồm việc có thay đổi (tiến trình, trạng thái, cờ, yêu cầu
//    hủy), theo thứ tự id. Lô đầu sau lúc rảnh đi ngay; lô sau bắt đầu ≥ Timing::batchInterval sau khi lô trước
//    áp xong (100 ms: ≤ 10 lô/s, UI chậm thì nhận ít lô hơn). Ngoại lệ: việc đã được yêu cầu hủy kết thúc thì báo ngay
//    (người dùng đang chờ). Việc kết thúc có mặt trong đúng một lô rồi rời registry. Rảnh: không timer, không sự kiện,
//    không polling.
//  - Cờ busyCursor (≥ 0,5 s) và showProgress (≥ 2 s) tính theo thời gian từ start(); khi cờ của một việc đổi mà
//    việc không báo gì, registry tự hẹn một lô (một timer một lần, chỉ khi có việc chưa qua mốc), vẫn theo nhịp
//    ≤ 10 lô/s nên có thể trễ ≤ batchInterval. UI cần đúng khung hình thì tự hẹn giờ từ JobInfo::startedAt và
//    timing() (UI0-112 ProgressPresenter làm vậy).
//
// ponytail: tập con của NT-013. Chưa có: việc không xác định % (thanh chạy vòng), nhãn bước con ("tệp 3/10"),
// tạm dừng/tiếp tục (VN-006), kết quả kiểu dữ liệu (việc tự đưa kết quả về qua trion::ui::post), lưu nhật ký
// việc đã xong cho trung tâm thông báo (UI0-009 giữ). Thêm khi task dùng cần.

#include "core/base/error.h"
#include "core/base/sched/Scheduler.h"

#include <QObject>
#include <QString>
#include <QTimer>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace trion::jobs {

using Clock = std::chrono::steady_clock;
using JobId = std::uint64_t; // tăng dần từ 1, không dùng lại; 0 không phải việc nào

enum class State : std::uint8_t { Running, Done, Failed, Cancelled };

// Ảnh chụp một việc tại lúc chụp (lô của updated() hoặc info()).
struct JobInfo {
    JobId id = 0;
    QString name;            // nhãn đã dịch cho người dùng ("Đang mở")
    QString kind;            // loại, mã ổn định cho máy đọc: "open", "plot", "autosave", "fontfix"…
    std::uint64_t owner = 0; // khóa chủ (document); 0: không thuộc document nào
    State state = State::Running;
    double progress = 0; // 0..1, không lùi; Done luôn 1
    bool cancelRequested = false;
    bool busyCursor = false;   // elapsed ≥ Timing::busyCursor
    bool showProgress = false; // elapsed ≥ Timing::showProgress
    Clock::time_point startedAt;
    Clock::duration elapsed{}; // từ start() đến lúc chụp; việc đã kết thúc: đến lúc kết thúc
    Error error;               // chỉ khi Failed
};

struct Timing {
    std::chrono::milliseconds busyCursor{500};    // DoD-L "thao tác dài": > 0,5 s con trỏ bận
    std::chrono::milliseconds showProgress{2000}; // > 2 s thanh tiến trình và nút hủy
    std::chrono::milliseconds batchInterval{100}; // khoảng cách tối thiểu giữa hai lô (≤ 10 lô/s)
};

namespace detail {
struct Job;
struct Registry;
} // namespace detail

// Tiến trình của một việc, hoặc một phạm vi con của nó. Mỗi đối tượng dùng trên một luồng tại một lúc; các
// phạm vi con trao cho luồng khác chạy song song được (cộng dồn nguyên tử, không khóa). Chỉ dùng trong lúc fn
// đang chạy.
class Progress {
public:
    // Phần đã xong của phạm vi này, 0..1: kẹp vào [0, 1], không lùi (giá trị nhỏ hơn lần trước bị bỏ, NaN coi
    // như 0). Không khóa; lô gửi UI gộp mọi lần gọi.
    void set(double fraction);

    // Phạm vi con chiếm `weight` (0..1, kẹp) của phạm vi này, nối tiếp ngay sau phần đã báo hoặc đã giao:
    //   auto read = p.child(0.7), regen = p.child(0.3);  // read 0..1 → 0..70%, regen 0..1 → 70..100%
    // Phần giao cho con coi như đã sắp chỗ: set() của cha chỉ cộng phần nằm sau nó.
    Progress child(double weight);

private:
    friend struct detail::Registry;
    Progress(detail::Job* job, std::int64_t lo, std::int64_t hi) : job_(job), lo_(lo), hi_(hi), pos_(lo) {}
    void advance(std::int64_t to);

    detail::Job* job_; // không sở hữu: việc sống đến khi fn trả về
    std::int64_t lo_, hi_, pos_;
};

class BackgroundJobs final : public QObject {
    Q_OBJECT
public:
    using Fn = std::function<Expected<void>(Progress&, sched::CancelToken)>;

    explicit BackgroundJobs(Timing timing = {}, QObject* parent = nullptr);
    ~BackgroundJobs() override;

    // Registry của ứng dụng: cái được tạo khi chưa có registry nào, đến lúc nó bị hủy; nullptr nếu không có.
    // Mọi luồng. Mã lõi (DOC-019, PLT-008, VN-006) bắt đầu việc qua đây.
    static BackgroundJobs* instance();

    const Timing& timing() const { return timing_; }

    // Mọi luồng. name: nhãn đã dịch; kind: mã loại; owner: khóa chủ (0: không có). Trả id của việc.
    JobId start(QString name, QString kind, std::uint64_t owner, Fn fn);

    // Mọi luồng. Yêu cầu hủy; false nếu không có việc đang chạy với id đó (đã xong, đã hủy, không tồn tại).
    bool cancel(JobId id);

    // Mọi luồng. Hủy mọi việc đang chạy của owner (đóng document); trả số việc vừa yêu cầu hủy.
    int cancelOwner(std::uint64_t owner);

    // Mọi luồng. Trạng thái hiện tại (không chờ lô); nullopt nếu việc không còn trong registry.
    std::optional<JobInfo> info(JobId id) const;

    // Mọi luồng. Số việc trong registry: đang chạy, hoặc đã kết thúc mà chưa báo trong lô nào.
    std::size_t size() const;

    // Số đo cho test và UI0-105. Chỉ đọc trên luồng UI.
    struct Stats {
        std::uint64_t batches = 0;          // số lô đã phát (updated())
        std::chrono::nanoseconds longest{}; // lần áp lô lâu nhất trên luồng UI: chụp + phát tín hiệu
    };
    Stats stats() const { return stats_; }

signals:
    // Luồng UI. Việc có thay đổi kể từ lô trước, theo thứ tự id.
    void updated(const std::vector<trion::jobs::JobInfo>& changed);

private:
    friend struct detail::Registry;
    void onWake();
    void schedule(Clock::time_point when);
    void flush();

    Timing timing_;
    std::shared_ptr<detail::Registry> s_;
    QTimer timer_{this};           // một lần: lô kế tiếp hoặc mốc cờ kế tiếp
    Clock::time_point deadline_{}; // lúc timer_ hẹn
    Clock::time_point lastFlush_{};
    Stats stats_;
};

} // namespace trion::jobs
