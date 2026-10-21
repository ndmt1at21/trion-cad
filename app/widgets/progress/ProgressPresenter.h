#pragma once
// UI0-112 · Tiến trình gắn NT-013: con trỏ bận, thanh %, nút hủy (07.1 §12, DoD-L "thao tác dài").
//
//   ProgressPresenter presenter(*jobs::BackgroundJobs::instance());   // một cái cho cửa sổ chính (UI0-001)
//   statusList->setModel(presenter.model());                         // UIA-009: danh sách Tác vụ nền
//   auto* item = new InlineProgress(presenter);                      // mục trên thanh trạng thái: tác vụ chính
//   auto* bar = new InlineProgress(presenter, jobId, dialog);        // trong hộp thoại: một việc (InlineProgress.h)
//   connect(&presenter, &ProgressPresenter::finished, toasts, …);    // UI0-009: xong thì toast
//
// Quy tắc theo thời gian từ JobInfo::startedAt, hẹn đúng mốc bằng một timer một lần (không đợi lô 100 ms của
// NT-013): < 0,5 s không hiện gì; từ 0,5 s con trỏ bận (Qt::BusyCursor, chuột và phím vẫn tới vùng vẽ, không hộp
// thoại chặn); từ 2 s việc vào model (thanh %, nút hủy). Lúc qua mốc hỏi lại NT-013 (info()) nên việc vừa xong mà
// lô báo xong chưa tới không làm con trỏ nháy. Mọi thay đổi hiện ra (model, changed()) gộp tối đa một lần mỗi
// khung hình theo tần số màn hình; rảnh: không timer, không cập nhật.
//
// Luồng: chỉ luồng UI. Ngưỡng 0,5 s/2 s lấy từ BackgroundJobs::timing() (test rút ngắn). Tạo trước khi có việc
// (presenter chỉ biết việc qua lô của NT-013).
// ponytail: busy cursor đặt bằng QGuiApplication::setOverrideCursor cho cả ứng dụng; vùng vẽ (RND) tự vẽ con trỏ
// riêng thì nối busyChanged() khi có canvas thật.

#include "core/base/jobs/BackgroundJobs.h"

#include <QAbstractListModel>
#include <QObject>
#include <QTimer>

#include <chrono>
#include <vector>

namespace trion::ui {

class ProgressPresenter;

// Các việc đã chạy ≥ 2 s, cũ nhất trước: hàng 0 là tác vụ chính. Thanh trạng thái (UIA-009) và danh sách Tác vụ
// nền dùng. Chỉ đọc; hủy qua ProgressPresenter::cancel(). Thay đổi tới theo hàng (thêm, bớt, dataChanged), tối đa
// một lần mỗi khung.
class ProgressItemModel final : public QAbstractListModel {
    Q_OBJECT
public:
    enum Role {
        JobIdRole = Qt::UserRole + 1, // quint64
        NameRole,                     // QString: tên việc ("Đang mở")
        KindRole,                     // QString: mã loại ("open")
        PercentRole,                  // int 0..100
        CancellingRole,               // bool: đã bấm hủy, chờ việc dừng ("Đang hủy…")
    };
    // Qt::DisplayRole: "Đang mở · 63%".

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

private:
    friend class ProgressPresenter;
    explicit ProgressItemModel(ProgressPresenter* presenter);
    // Presenter gọi khi áp: rows tăng dần theo id; dataChanged cho mọi hàng nếu dữ liệu của hàng nào đã đổi.
    void sync(const std::vector<jobs::JobId>& rows, bool changed);

    ProgressPresenter* presenter_;
    std::vector<jobs::JobId> rows_;
};

class ProgressPresenter final : public QObject {
    Q_OBJECT
public:
    explicit ProgressPresenter(jobs::BackgroundJobs& jobs, QObject* parent = nullptr);
    ~ProgressPresenter() override; // trả con trỏ chuột nếu đang bận

    jobs::BackgroundJobs& jobs() const { return jobs_; }
    ProgressItemModel* model() { return &model_; }

    bool busy() const { return busy_; } // con trỏ bận đang bật

    // Việc đang theo dõi (đang chạy, chưa có lô báo kết thúc); nullptr nếu không có. Con trỏ hết hạn ở lần áp sau.
    const jobs::JobInfo* job(jobs::JobId id) const;
    bool shown(jobs::JobId id) const;      // đã qua mốc 2 s: có thanh % và nút hủy
    bool cancelling(jobs::JobId id) const; // đã bấm hủy (ở đây hoặc nơi khác), việc chưa dừng
    jobs::JobId primary() const; // tác vụ chính: việc hiện tiến trình cũ nhất; 0 nếu không có
    int shownCount() const;      // số việc đang hiện tiến trình

    // Bấm hủy: đánh dấu "Đang hủy…" rồi yêu cầu NT-013 hủy. Việc không theo dõi: bỏ qua.
    void cancel(jobs::JobId id);

    // "Đang mở · 63%": nhãn chung của model và InlineProgress. percent 0..100.
    static QString label(const QString& name, int percent);
    static int percent(double progress); // 0..1 → 0..100, làm tròn xuống (63% chỉ khi đã đủ 63%)

signals:
    void busyChanged(bool busy);
    void changed();                                  // sau mỗi lần áp, tối đa một lần mỗi khung
    void finished(const trion::jobs::JobInfo& info); // việc kết thúc (Done, Failed, Cancelled); ngay khi lô tới

private:
    struct Entry {
        jobs::JobInfo info;
        bool busy = false;       // đã qua mốc con trỏ bận
        bool shown = false;      // đã qua mốc hiện tiến trình
        bool ended = false;      // NT-013 báo đã dừng, chờ lô báo kết thúc: không hiện nữa
        bool cancelling = false; // bấm hủy ở đây
    };
    const Entry* find(jobs::JobId id) const;
    Entry* find(jobs::JobId id);
    void onBatch(const std::vector<jobs::JobInfo>& batch);
    void request(); // có thay đổi: áp ngay hoặc ở khung kế
    void apply();
    void setBusy(bool busy);

    jobs::BackgroundJobs& jobs_;
    ProgressItemModel model_{this};
    std::vector<Entry> entries_; // tăng dần theo id
    bool busy_ = false;
    bool dataDirty_ = false; // dữ liệu của hàng đang hiện đã đổi từ lần áp trước
    QTimer mark_{this};      // một lần: mốc 0,5 s/2 s kế tiếp
    QTimer frame_{this};     // một lần: lần áp kế tiếp khi khung này đã áp
    jobs::Clock::time_point lastApply_{};
    std::chrono::nanoseconds frameTime_{16'666'667};
};

} // namespace trion::ui
