#pragma once
// UI0-001 · Mốc các pha khởi động ấm (DoD-L: đến khung đầu ≤ 1,5 s, nghiệm thu ở UI0-115; pha nền ≤ 400 ms).
//
// Mốc 0 là lúc hệ điều hành tạo tiến trình (Linux: /proc/self/stat, độ phân giải 1 tick = 10 ms; Windows:
// GetProcessTimes), nên thời gian nạp thư viện Qt trước main() cũng được tính; không đọc được thì mốc 0 là lúc dựng
// StartupTrace ("origin": "main"). Mỗi mốc là lúc một pha kết thúc; main.cpp và Application đặt:
//   main (vào main) → qt (QApplication) → instance (chạy một phiên) → settings (Store, UiExecutor) → i18n → fonts →
//   theme → icons → keymap → ready (sẵn sàng dựng cửa sổ chính: tiêu chí 2 của UI0-001) → window (dựng, hiện cửa sổ
//   chính) → first_frame (khung đầu đã vẽ và flush) → quit (aboutToQuit) → tasks_stopped (việc nền đã dừng) → exit.
// Lần chạy sau (chuyển đường dẫn cho phiên đang chạy) có: main → qt → forwarded → exit.
// UI0-115 thêm mốc của mình bằng mark() (trang Start, ribbon, dòng lệnh nhận phím…).
//
// --startup-trace <tệp.json>: ghi ở nền khi có khung đầu (đọc được khi ứng dụng còn chạy) và ghi lại khi hủy (đủ mốc
// thoát); ghi qua tệp tạm rồi đổi tên nên người đọc không bao giờ thấy tệp dở:
//   {"format": "trion-startup-trace/1", "origin": "process"|"main",
//    "marks": [{"name": "main", "at_ms": 21.4, "ms": 21.4}, {"name": "qt", "at_ms": 48.0, "ms": 26.6}, ...]}
// at_ms tính từ mốc 0, ms từ mốc trước (độ dài pha), làm tròn 0,1 ms. Khung đầu còn ghi vào UiProbe:
// Op::WarmStart = mốc 0 → first_frame.

#include <QList>
#include <QObject>
#include <QString>

#include <chrono>
#include <optional>
#include <utility>

class QJsonObject;
class QWidget;

namespace trion::app {

class StartupTrace : public QObject {
    Q_OBJECT
public:
    using Clock = std::chrono::steady_clock;

    // Dựng đầu main(), trước QApplication: đặt mốc 0 và mốc "main".
    StartupTrace();
    ~StartupTrace() override; // đặt mốc "exit", ghi tệp lần cuối

    void setOutput(const QString& jsonPath); // rỗng: không ghi

    // Luồng UI: pha `name` kết thúc bây giờ.
    void mark(const QString& name);

    // Luồng UI, ngay sau window->show() và trước khi về vòng sự kiện: khi khung đầu của cửa sổ đã vẽ và flush thì đặt
    // mốc "first_frame", ghi Op::WarmStart vào UiProbe, gửi việc ghi tệp ra nền (sched) rồi phát firstFrame(). Chỉ khung
    // đầu.
    void watchFirstFrame(QWidget* window);

    // Lúc đặt mốc `name` lần đầu, ms từ mốc 0; chưa có thì nullopt.
    std::optional<double> at(const QString& name) const;
    QJsonObject toJson() const;
    bool write() const; // ghi ra tệp của setOutput(); chưa đặt tệp thì không làm gì. Lỗi: cảnh báo, trả false

signals:
    void firstFrame();

private:
    bool eventFilter(QObject* watched, QEvent* event) override;

    Clock::time_point origin_;
    bool fromProcess_ = false; // mốc 0 là lúc tạo tiến trình
    QList<std::pair<QString, Clock::time_point>> marks_;
    QString path_;
};

} // namespace trion::app
