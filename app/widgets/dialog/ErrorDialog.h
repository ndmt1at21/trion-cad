#pragma once
// UI0-008 · Hộp thoại lỗi chặn thao tác (07.1 §12): lời giải thích (vì sao, cách khắc phục; câu ngắn, không đổ lỗi),
// mã lỗi, chi tiết kỹ thuật thu gọn mặc định, "Sao chép chi tiết", "Gửi báo lỗi"; nút chính "Đóng". Modal, tự xóa khi
// đóng.
//
//   (new ErrorDialog(tr("Không lưu được bản vẽ"), tr("Ổ đĩa đầy. Giải phóng dung lượng rồi lưu lại."), err, window))
//       ->open();
//
// Chi tiết (details(): hiện khi mở "Chi tiết kỹ thuật", chép vào clipboard, chuyển cho luồng báo lỗi): tiêu đề, mã lỗi
// (Error::module.code), thông điệp của Error, phiên bản, hệ điều hành, kLogLines dòng log cuối lúc dựng hộp thoại.
//
// Chỗ nối tạm, main đặt một lần trên luồng UI:
//   ErrorDialog::setLogSource([](int n) { return …; });            // n dòng log cuối, cũ trước
//   ErrorDialog::setReportHandler([](const QString& details, QWidget* parent) { /* mở UI-L08 */ });
// ponytail: NT-009 (log ring buffer) chưa có: chưa đặt nguồn log thì chi tiết ghi "Chưa có log". PLF-010 (TCREPORT,
// UI-L08) chưa có: chưa đặt handler thì không hiện "Gửi báo lỗi". Nút này chỉ gọi handler kèm chi tiết; xem nội dung
// và đồng ý gửi là việc của UI-L08, hộp thoại này không gửi gì.
#include "app/widgets/dialog/DialogFrame.h"
#include "core/base/error.h"

#include <QString>
#include <QStringList>

#include <functional>

namespace trion::ui {

class ErrorDialog : public DialogFrame {
    Q_OBJECT
public:
    static constexpr int kLogLines = 200; // [đề xuất] UI0-008 tiêu chí 3

    using LogSource = std::function<QStringList(int maxLines)>;                         // ≤ maxLines dòng cuối
    using ReportHandler = std::function<void(const QString& details, QWidget* parent)>; // parent: hộp thoại lỗi
    static void setLogSource(LogSource source);                                         // rỗng: gỡ
    static void setReportHandler(ReportHandler handler);                                // rỗng: gỡ

    // title: việc không làm được ("Không lưu được bản vẽ"); explanation: vì sao và cách khắc phục; error: mã và thông
    // điệp kỹ thuật cho phần chi tiết.
    ErrorDialog(const QString& title, const QString& explanation, const Error& error, QWidget* parent = nullptr);

    QString details() const { return details_; }

private:
    QString details_;
};

} // namespace trion::ui
