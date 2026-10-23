#pragma once
// UIA-008 · Hiển thị dần khi mở tệp lớn (UI-B13; prototype js/screens1.js F.openLarge): việc nạp ở nền, vùng vẽ vẫn
// xem, pan, zoom; lệnh sửa gõ trong lúc nạp chờ đến khi nạp xong.
//  - start(): việc nạp đăng ký NT-013 (BackgroundJobs, loại "open", nhãn "Đang nạp"): thanh trạng thái hiện tiến trình
//    và nút hủy qua ProgressPresenter (UI0-112, từ 2 s), hủy phản hồi ≤ 50 ms khi việc kiểm CancelToken ≥ mỗi 10 ms.
//    Mỗi lô việc báo đã công bố (published, gọi từ luồng nền) → vùng vẽ vẽ lại cảnh (updateScene, gộp theo lượt UI).
//    Không chặn luồng UI: điều hướng (Navigation) chạy như thường.
//  - submit(macro): cổng lệnh đặt trước router (INT-002). Đang nạp mà lệnh ghi bản vẽ: giữ lại (một lệnh; lệnh sửa sau
//    thay lệnh trước), báo chờ bằng CursorTip tại con trỏ (đọc cho trình đọc màn hình) và notice() cho dòng lệnh; nạp
//    xong thì gửi sink. Esc trong cửa sổ của vùng vẽ (vùng vẽ hay ô dòng lệnh có focus) hoặc cancelHeld() bỏ lệnh chờ;
//    Esc đó chỉ bỏ lệnh chờ (lần Esc sau mới tới dòng lệnh). Nạp bị hủy, lỗi: bỏ lệnh chờ kèm notice. Lệnh xem, lệnh đo
//    (ZOOM, PAN, DIST…), sysvar trong suốt ('ORTHOMODE 1), lệnh ngoài danh mục (lệnh giao diện): đi thẳng.
// ponytail: chỗ nối DOC-016 (LoadFn: core/doc/load/progressive thay) và INT-001 (setWrites: cờ "ghi bản vẽ" của lệnh
// thay); mặc định coi lệnh của danh mục mà Viewer không có là lệnh ghi (Viewer không sửa được bản vẽ), trừ lệnh mở tài
// liệu, giao diện khác (NEW…). Một việc nạp một lúc (start khi đang nạp: hủy việc cũ, bỏ lệnh chờ); đa tài liệu (lệnh
// chờ theo tài liệu, đổi tab) do integration với document (DOC-011) làm. Thanh trạng thái chưa có số đối tượng ("· 1,2
// triệu đối tượng": NT-013 chưa có nhãn bước con). Cổng ghi thật là DOC-103 (Loading); cổng này cho trải nghiệm chờ.
//
// Lắp (integration), luồng UI, sau CanvasWidget, BackgroundJobs, CommandRouter:
//   auto* gate = new LoadGate(jobs, *canvas, canvas);
//   gate->setSink([&router, nav](const QString& m) { nav->flush(); router.submit(m); }); // bước view trước lệnh
//   // mọi nguồn lệnh (ActionRegistry::setSink, ribbon, QAT, dòng lệnh) gửi gate->submit thay cho router.submit
//   connect(gate, &LoadGate::notice, cmdline, [cmdline](const QString& t) { cmdline->appendHistory(t); });
//   // mở tệp lớn (DOC-016 cung cấp hàm nạp):
//   gate->start(docKey, [](jobs::Progress& p, sched::CancelToken c, const LoadGate::Published& published) { … });

#include "core/base/error.h"
#include "core/base/jobs/BackgroundJobs.h"

#include <QObject>
#include <QPointer>
#include <QString>

#include <cstdint>
#include <functional>

namespace trion::ui {
class CursorTip;
}

namespace trion::shell {

class CanvasWidget;

class LoadGate final : public QObject {
    Q_OBJECT
public:
    // Báo một lô bản vẽ đã công bố (snapshot DOC-009 thấy lô ấy). Gọi từ mọi luồng, không chặn.
    using Published = std::function<void()>;
    // Việc nạp (DOC-016), chạy trên pool NT-005: báo tiến trình, kiểm cancel ≥ mỗi 10 ms, gọi published sau mỗi lô.
    using LoadFn =
        std::function<Expected<void>(jobs::Progress& progress, sched::CancelToken cancel, const Published& published)>;
    // Lệnh có ghi bản vẽ (phải chờ nạp xong)? command: tên viết hoa, đã bỏ ^C và tiền tố ' _ . ("LINE").
    using Writes = std::function<bool(const QString& command)>;

    // jobs, canvas sống lâu hơn cổng (cha nên là canvas). Luồng UI.
    LoadGate(jobs::BackgroundJobs& jobs, CanvasWidget& canvas, QObject* parent = nullptr);
    ~LoadGate() override; // hủy việc nạp đang chạy

    // Bắt đầu nạp; owner: khóa tài liệu (BackgroundJobs::cancelOwner khi đóng). Trả id của việc.
    jobs::JobId start(std::uint64_t owner, LoadFn fn);
    bool cancel(); // yêu cầu hủy việc đang nạp; false nếu không có
    bool loading() const { return job_ != 0; }
    jobs::JobId job() const { return job_; }

    void setSink(std::function<void(const QString& macro)> sink) { sink_ = std::move(sink); } // router (INT-002)
    void setWrites(Writes writes) { writes_ = std::move(writes); } // không đặt: writesByDefault
    static bool writesByDefault(const QString& command);

    // Một chuỗi lệnh (cú pháp macro của ActionRegistry: ^C^C_LINE, '_ZOOM, 'ORTHOMODE 1): gửi sink ngay hoặc giữ chờ.
    void submit(const QString& macro);
    QString held() const { return held_; } // lệnh đang chờ; rỗng: không có
    void cancelHeld();                     // bỏ lệnh chờ (Esc)

signals:
    void loadingChanged(bool loading);
    void finished(trion::jobs::State state); // việc nạp kết thúc: Done, Failed, Cancelled
    void heldChanged(const QString& macro);  // lệnh chờ đổi; rỗng: hết chờ (đã gửi hoặc bị bỏ)
    void notice(const QString& text);        // thông báo cho dòng lệnh (CommandLine::appendHistory)
    void published();                        // lô mới đã đưa vùng vẽ vẽ lại

protected:
    bool eventFilter(QObject* watched, QEvent* e) override; // Esc khi có lệnh chờ (lọc cả ứng dụng, chỉ lúc chờ)

private:
    void finish(jobs::State state);
    void release(); // hết chờ: gỡ lọc phím, ẩn CursorTip

    jobs::BackgroundJobs& jobs_;
    CanvasWidget& canvas_;
    jobs::JobId job_ = 0;
    std::function<void(const QString&)> sink_;
    Writes writes_;
    QString held_;
    QPointer<ui::CursorTip> tip_; // con của vùng vẽ, tạo khi cần
};

} // namespace trion::shell
