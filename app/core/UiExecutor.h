#pragma once
// UI0-101 · Executor luồng UI: kết quả việc nền về luồng UI theo lô; kiểm sở hữu luồng (DoD-U 4, DoD-H 6).
//
// Hợp đồng luồng
//  - Luồng UI là luồng của QCoreApplication; nó sở hữu document và mọi widget.
//  - Cả tiến trình có một UiExecutor, tạo và hủy trên luồng UI khi đã có QCoreApplication (UI0-001 giữ nó
//    suốt đời ứng dụng, test tự tạo). Khi tạo, nó cài poster cho trion::sched::postToUi(); khi hủy thì gỡ
//    poster và bỏ mọi kết quả còn chờ. Chỉ hủy sau khi việc nền đã dừng (hủy + sched::waitIdle()): gửi khi
//    không có executor là lỗi lập trình (assert ở debug), kết quả bị bỏ.
//  - post()/postLatest() gọi được từ mọi luồng, không chặn (một mutex giữ rất ngắn). Chỉ lần gửi khi chưa có
//    sự kiện đánh thức nào chờ mới đăng một sự kiện; mọi lần gửi sau đó, đến khi luồng UI dọn xong hàng, đi
//    chung lô ấy (cả lô một sự kiện, không phải mỗi kết quả một sự kiện).
//  - Luồng UI xử lý theo lượt: chạy callback theo thứ tự gửi đến khi hết hàng hoặc hết hạn mức 3 ms (cả lượt
//    ≤ 4 ms), rồi trả vòng sự kiện; còn việc thì hẹn lượt sau bằng một sự kiện. Rảnh: không sự kiện, không
//    timer, không polling.
//  - Callback chạy trên luồng UI, phải ngắn (≲ 0,1 ms) và không mở vòng sự kiện lồng (exec()); việc dài hơn
//    2 ms chạy nền. Callback ném ngoại lệ thì ghi stderr, lượt chạy tiếp.
//  - Receiver: tạo trên luồng UI từ một QObject sống ở đó (QPointer chỉ an toàn khi chắc đối tượng còn sống
//    lúc tạo; sai luồng thì assert ở debug). Sau đó chép, chuyển, hủy ở luồng nào cũng được; chỉ executor đọc
//    nó, trên luồng UI, ngay trước khi chạy callback: receiver đã hủy thì callback bị bỏ. Nhờ vậy callback
//    được giữ con trỏ thô tới receiver (kể cả `this`); luồng nền thì không được chạm vào receiver.
//
// Ví dụ, trên luồng UI:
//   trion::ui::Receiver rx(this);
//   trion::sched::submit(trion::sched::Priority::Normal, [rx, this, path] {
//       auto rows = scan(path);                                                 // nền, không chạm `this`
//       trion::ui::post(rx, [this, rows = std::move(rows)] { apply(rows); });  // luồng UI, `this` còn sống
//   });

#include "core/base/assert.h"
#include "core/base/error.h"

#include <QObject>
#include <QPointer>

#include <chrono>
#include <cstdint>
#include <functional>

namespace trion::ui {

// Đang ở luồng UI? Chưa có QCoreApplication thì không.
bool isUiThread();

// Kiểm sở hữu luồng cho API ghi: sai luồng → assert ở debug (TRION_ASSERT) và trả lỗi "ui/not-ui-thread";
// bản release chỉ trả lỗi. Dùng: `if (auto ok = trion::ui::requireUiThread(); !ok) return ok.error();`
Expected<void> requireUiThread();

// Tay cầm tới receiver của kết quả, đồng thời là khóa gộp của postLatest(): bản chép chung khóa, hai
// Receiver tạo riêng (kể cả cho cùng đối tượng) là hai khóa. Khóa không bao giờ dùng lại, nên đối tượng mới
// trùng địa chỉ đối tượng đã hủy không nhận nhầm kết quả cũ.
class Receiver {
public:
    explicit Receiver(const QObject* object); // chỉ trên luồng UI, object sống trên luồng UI

private:
    friend void post(const Receiver&, std::function<void()>);
    friend void postLatest(const Receiver&, std::function<void()>);
    QPointer<const QObject> object_;
    std::uint64_t key_;
};

// Mọi luồng: chạy fn trên luồng UI ở lượt tới. trion::sched::postToUi() đi qua đây.
void post(std::function<void()> fn);

// Mọi luồng: như trên, nhưng bỏ fn nếu receiver đã hủy trước khi tới lượt.
void post(const Receiver& receiver, std::function<void()> fn);

// Mọi luồng: gộp theo khóa receiver, mới nhất thắng. Nếu khóa đã có kết quả đang chờ thì thay fn và giữ chỗ
// cũ trong hàng; receiver nhận một lần với giá trị cuối. Dùng cho tiến trình, xem trước, trạng thái.
void postLatest(const Receiver& receiver, std::function<void()> fn);

class UiExecutor final : public QObject {
public:
    UiExecutor();
    ~UiExecutor() override;

    // Số đo cho test và UI0-105, tính từ lúc tạo executor. Chỉ đọc trên luồng UI.
    struct Stats {
        std::uint64_t wakeups = 0;            // sự kiện đánh thức đã đăng (lô mới và lượt hẹn sau)
        std::uint64_t ran = 0;                // callback đã chạy
        std::uint64_t dropped = 0;            // callback bỏ vì receiver đã hủy
        std::chrono::nanoseconds busy{0};     // tổng thời gian các lượt trên luồng UI
        std::chrono::nanoseconds longest{0};  // lượt dài nhất
    };
    Stats stats() const;

private:
    void customEvent(QEvent*) override; // lượt xử lý
};

} // namespace trion::ui

// Bất biến "đang ở luồng UI" cho mã nội bộ; bản release bỏ. API ghi dùng requireUiThread().
#define TRION_ASSERT_UI_THREAD() TRION_ASSERT(::trion::ui::isUiThread())
