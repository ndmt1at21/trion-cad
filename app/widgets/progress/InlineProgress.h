#pragma once
// UI0-112 · Tiến trình gọn: [thanh %] "Đang mở · 63%" [Hủy] (prototype .tx-prog trong chân hộp thoại, .sb-task trên
// thanh trạng thái). Hiện khi việc đã chạy ≥ 2 s (ProgressPresenter), tự ẩn khi việc dừng.
//
//   auto* progress = new InlineProgress(presenter, jobId, footer); // hộp thoại: đúng một việc
//   auto* task = new InlineProgress(presenter, 0, statusBar);      // thanh trạng thái: tác vụ chính, "· 2 tác vụ khác"
//
// - Bấm Hủy: nút thành "Đang hủy…" và tắt ngay trong lượt sự kiện của cú bấm (khung kế đã vẽ), rồi NT-013 hủy việc;
//   việc dừng thì widget ẩn. Hủy từ nơi khác (đóng document) cũng hiện "Đang hủy…".
// - Trình đọc màn hình: thanh là QProgressBar (vai trò ProgressBar, tên = tên việc) nhưng giá trị của nó chỉ đổi
//   theo bước 10% (một QAccessibleValueChangeEvent mỗi bước); tên tiếp cận của nhãn cũng theo bước 10%, nhãn hiện
//   số chính xác. Nút Hủy có mô tả tiếp cận là tên việc.
// - Hộp thoại muốn giữ chỗ khi ẩn: QSizePolicy::setRetainSizeWhenHidden(true).
//
// ponytail: một kiểu cho cả hai nơi (nút "Hủy" chữ, thanh giãn theo chỗ). Kiểu gọn của thanh trạng thái (thanh 80×4,
// nút x biểu tượng, vòng quay) để UIA-009 thêm khi dựng thanh trạng thái thật.

#include "app/widgets/progress/ProgressPresenter.h"

#include <QWidget>

class QLabel;

namespace trion::ui {

class Button;
class StepBar;

class InlineProgress final : public QWidget {
    Q_OBJECT
public:
    // job 0: theo tác vụ chính của presenter. Chỉ luồng UI; luôn có widget cha (tự hiện/ẩn như widget con);
    // presenter sống lâu hơn widget.
    explicit InlineProgress(ProgressPresenter& presenter, jobs::JobId job = 0, QWidget* parent = nullptr);

    jobs::JobId job() const { return job_; }
    void setJob(jobs::JobId job);

    jobs::JobId current() const { return current_; } // việc đang hiện; 0 khi ẩn

protected:
    void changeEvent(QEvent* e) override;

private:
    void refresh();
    void applyTheme();

    ProgressPresenter& presenter_;
    jobs::JobId job_;
    jobs::JobId current_ = 0;
    StepBar* bar_;
    QLabel* text_;
    Button* cancel_;
};

} // namespace trion::ui
