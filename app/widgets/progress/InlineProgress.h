#pragma once
// UI0-112 · Tiến trình gọn. Hiện khi việc đã chạy ≥ 2 s (ProgressPresenter), tự ẩn khi việc dừng. Hai kiểu:
//   Full     [thanh % giãn] "Đang mở · 63%" [Hủy]: chân hộp thoại (.tx-prog), danh sách Tác vụ nền
//   Compact  [vòng quay] "Đang mở · 63%" [thanh 80×4] [×]: thanh trạng thái (.sb-task: lề, khoảng cách --sp-3, chữ
//            --fs-xs --c-text-2; thanh cao --sp-2; nút × .ib.sm, tên tiếp cận "Hủy")
//
//   auto* progress = new InlineProgress(presenter, jobId, footer); // hộp thoại: đúng một việc
//   auto* task = new InlineProgress(presenter, 0, bar, InlineProgress::Style::Compact); // tác vụ chính, "· 2 khác"
//
// - Bấm Hủy: nút thành "Đang hủy…" (Compact: nhãn "… · đang hủy…", tên tiếp cận của nút × "Đang hủy…") và tắt ngay
//   trong lượt sự kiện của cú bấm (khung kế đã vẽ), rồi NT-013 hủy việc; việc dừng thì widget ẩn. Hủy từ nơi khác
//   (đóng document) cũng hiện "Đang hủy…".
// - Trình đọc màn hình: thanh là QProgressBar (vai trò ProgressBar, tên = tên việc) nhưng giá trị của nó chỉ đổi
//   theo bước 10% (một QAccessibleValueChangeEvent mỗi bước); tên tiếp cận của nhãn cũng theo bước 10%, nhãn hiện
//   số chính xác. Nút Hủy có mô tả tiếp cận là tên việc.
// - Hộp thoại muốn giữ chỗ khi ẩn: QSizePolicy::setRetainSizeWhenHidden(true).

#include "app/widgets/progress/ProgressPresenter.h"

#include <QWidget>

#include <cstdint>

class QLabel;

namespace trion::ui {

class Button;
class Spinner;
class StepBar;

class InlineProgress final : public QWidget {
    Q_OBJECT
public:
    enum class Style : std::uint8_t { Full, Compact };

    // job 0: theo tác vụ chính của presenter. Chỉ luồng UI; luôn có widget cha (tự hiện/ẩn như widget con);
    // presenter sống lâu hơn widget.
    explicit InlineProgress(ProgressPresenter& presenter, jobs::JobId job = 0, QWidget* parent = nullptr,
                            Style style = Style::Full);

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
    Style style_;
    Spinner* spin_ = nullptr; // Compact
    StepBar* bar_;
    QLabel* text_;
    Button* cancel_;
};

} // namespace trion::ui
