#pragma once
// UI0-110 · Khung hộp thoại (07.2 "Quy ước áp dụng"; 07.1 §1 mục 2, §13.3; prototype TC.dialog, css .dlg*).
//
// Bố cục (.dlg): thẻ bo --r-xl, viền --c-border-ctl, bóng --sh-dlg (Giao diện nhẹ: viền 1 px, token tự đổi).
//   đầu (.dlg-h)  tiêu đề = windowTitle(), tên lệnh tiếng Anh (Mono, --c-text-3), nút Trợ giúp (F1), nút Đóng (Esc);
//                 kéo đầu để di chuyển
//   thân (.dlg-b) setBody(); đang tải: setLoading() hoặc loadBody() (dựng nội dung trễ, việc nền)
//   chân (.dlg-f) [footer()->insertWidget(0, …)] [co giãn] [nút thêm, addCancel()] [setConfirm()]
// Cửa sổ không viền của hệ điều hành, nền trong suốt để vẽ bóng: mép ngoài thẻ là lề bóng (contentsMargins).
//
// Phím (07.1 §13.3): Enter = nút xác nhận (QDialog: nút mặc định; ô nhiều dòng giữ Enter để xuống dòng, nút đang có
// focus tự kích hoạt); Esc hủy (reject); F1 phát helpRequested kể cả khi phím tắt F1 toàn cục đã gán; Tab vòng trong
// hộp thoại. Hiện: focus vào widget nhận Tab đầu tiên của thân không phải nút (ô nhập), không có thì nút xác nhận;
// đóng (done) thì focus về widget có focus lúc mở.
//
// Đo (DoD-L, UI0-105): UiProbe tự đo dialog_open của mọi QDialog, từ phím/nhấp mở nó (không có thì lúc hiện) đến hết
// khung đầu. Dữ liệu chậm (loadBody): khung đầu là đầu, chân và "Đang tải…", không chờ dữ liệu; không phải báo thêm.
//
// Modal / modeless: theo QDialog. Modal (07.2: áp dụng khi bấm nút xác nhận) gọi setModal(true) rồi open(); mặc định
// modeless, show(): không chặn cửa sổ chính (07.1 §1 mục 2). DialogHost mở theo mã, nhớ vị trí, cỡ.
//
// Dùng (sau QApplication, loadFonts, ThemeManager, UiExecutor, IconProvider như main):
//   class PlotDialog : public trion::ui::DialogFrame {
//   public:
//       explicit PlotDialog(QWidget* parent) : DialogFrame(u"plot"_s, parent)
//       {
//           setModal(true);
//           setCommand(u"PLOT"_s);
//           setBody(form_ = new PlotForm);   // hoặc loadBody(đọc nền, dựng widget) khi dữ liệu chậm
//           addCancel();
//           retranslate();
//       }
//       void accept() override { apply(); DialogFrame::accept(); } // nút xác nhận, Enter
//   protected:
//       void changeEvent(QEvent* e) override
//       {
//           if (e->type() == QEvent::LanguageChange)
//               retranslate();
//           DialogFrame::changeEvent(e);
//       }
//   private:
//       void retranslate()
//       {
//           setWindowTitle(tr("In"));
//           setConfirm(tr("In"));           // tên hành động, không "OK" (DialogHost::confirmLabelIssues)
//       }
//   };
#include "app/core/UiExecutor.h"
#include "app/widgets/controls/Controls.h"
#include "core/base/sched/Scheduler.h"

#include <QDialog>
#include <QPointer>

#include <memory>
#include <type_traits>
#include <utility>

class QHBoxLayout;
class QLabel;
class QVBoxLayout;

namespace trion::ui {

class DialogFrame : public QDialog {
    Q_OBJECT
public:
    // id: mã hộp thoại, ASCII không '/' ("plot"): khóa lưu vị trí, cỡ, một bản mỗi mã (DialogHost), chủ đề trợ giúp
    // khi không có tên lệnh. parent: cửa sổ mở nó (cửa sổ chính, hoặc hộp thoại khi xếp chồng).
    explicit DialogFrame(const QString& id, QWidget* parent = nullptr);
    ~DialogFrame() override;

    QString id() const { return id_; }

    // Tên lệnh tiếng Anh hiện cạnh tiêu đề ("PLOT"); có thì có nút Trợ giúp. Không dịch.
    QString command() const { return command_; }
    void setCommand(const QString& command);

    // Thân. flush: không lề (.dlg-b.flush) cho bảng, danh sách sát mép. Frame sở hữu; gọi lại thì thay, bản cũ bị xóa
    // (kể cả "Đang tải…": đừng giữ con trỏ vào thân cũ); nút xác nhận bật lại nếu đang tải. Ô nhiều dòng có sẵn trong
    // thân được setTabChangesFocus(true) để Tab vòng trong hộp thoại.
    void setBody(QWidget* body, bool flush = false);
    QWidget* body() const { return body_; }
    // Thân thành "Đang tải…" có vòng quay (.empty .spin; đứng yên khi Giao diện nhẹ, giảm chuyển động); nút xác nhận
    // vô hiệu đến khi setBody.
    void setLoading();
    bool isLoading() const;
    // Dựng nội dung trễ (dữ liệu chậm): setLoading() ngay, khung đầu hiện không chờ dữ liệu; load(sched::CancelToken)
    // chạy nền (NT-005, ưu tiên Interactive), không chạm widget; build(kết quả) chạy trên luồng UI, trả QWidget* cho
    // setBody. Đóng hộp thoại (done), hủy frame hoặc gọi loadBody lần nữa thì hủy token, kết quả cũ bị bỏ.
    template <class Load, class Build>
    void loadBody(Load load, Build build, bool flush = false);

    // Chân, tạo khi gọi lần đầu: bố cục ngang, mục 0 là khoảng co giãn; nút thêm vào cuối, phần bên trái chèn trước nó.
    // Nút xác nhận đứng cuối: thêm nút khác trước setConfirm (thường gọi trong retranslate() cuối hàm dựng).
    QHBoxLayout* footer();
    // "Hủy" (ghost) → reject(). Chữ do frame dịch lại.
    Button* addCancel();
    // Nút xác nhận (.btn.primary, cuối chân): chữ là tên hành động ("Tạo block", "In"), không "OK"; là nút mặc định
    // của Enter; bấm → accept() (modal áp dụng rồi đóng; modeless ghi đè accept() nếu không đóng). Gọi lại thì đổi chữ,
    // kiểu. Chữ do bên gọi dịch lại (retranslate).
    Button* setConfirm(const QString& action, Button::Kind kind = Button::Kind::Primary);
    Button* confirmButton() const { return confirm_; }

    // Kéo mép thẻ đổi cỡ (dải --sp-3 ngoài mép). Mặc định không: hộp thoại của prototype cố định cỡ.
    bool isResizable() const { return resizable_; }
    void setResizable(bool on);

    // Ẩn tạm cho thao tác lấy từ bản vẽ (07.2 UI-C09 nút ⌖): ẩn mà không đóng (không finished, không lưu vị trí);
    // modal thì cửa sổ chính nhận lại chuột, phím. Hiện lại đúng chỗ, lại modal, focus về widget đã có focus.
    // ponytail: chỉ ẩn/hiện; thao tác lấy điểm (Esc hủy, trả giá trị) do UI0-007 và chuỗi lệnh INT-002 nối.
    void hideForPick(bool hidden);
    bool isHiddenForPick() const { return pickHidden_; }

    // Thẻ hộp thoại (phần nhìn thấy, không gồm lề bóng) theo tọa độ màn hình: DialogHost lưu và khôi phục cái này.
    QRect cardGeometry() const;
    void setCardGeometry(const QRect& card);

signals:
    // F1 hoặc nút Trợ giúp. topic: tên lệnh (setCommand), không có thì mã hộp thoại.
    // ponytail: chưa nối hệ trợ giúp; UIL-009 nối (qua DialogHost::helpRequested).
    void helpRequested(const QString& topic);

public slots:
    void done(int result) override; // hủy việc tải, rồi đóng như QDialog, focus về chỗ cũ

protected:
    bool event(QEvent* e) override;            // ShortcutOverride: F1 thắng phím tắt toàn cục
    void keyPressEvent(QKeyEvent* e) override; // F1; Enter, Esc của QDialog
    void showEvent(QShowEvent* e) override;    // nhớ focus cũ, đặt focus đầu
    void paintEvent(QPaintEvent* e) override;  // bóng, thẻ, nền đầu, nét chia
    void changeEvent(QEvent* e) override;      // WindowTitleChange, LanguageChange
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    bool eventFilter(QObject* watched, QEvent* e) override; // kéo đầu để di chuyển

private:
    void retranslate();
    void applyMargins(); // lề bóng theo token (Giao diện nhẹ đổi), giữ thẻ đứng yên
    QRect cardRect() const;
    Qt::Edges edgesAt(QPoint pos) const;
    void beginDrag(Qt::Edges edges, QPoint global);
    void requestHelp();

    QString id_;
    QString command_;
    QWidget* header_;
    QLabel* title_;
    QLabel* commandLabel_;
    Button* help_;
    Button* close_;
    QVBoxLayout* bodyBox_;
    QWidget* body_ = nullptr;
    QWidget* loading_ = nullptr; // thân "Đang tải…" khi đang hiện
    QWidget* footer_ = nullptr;
    Button* cancel_ = nullptr;
    Button* confirm_ = nullptr;
    QMargins shadow_;             // lề bóng quanh thẻ
    QPointer<QWidget> prevFocus_; // focus lúc mở, trả lại khi đóng
    QPointer<QWidget> pickFocus_; // focus lúc ẩn tạm
    sched::CancelSource load_;
    struct Drag {
        bool on = false;
        Qt::Edges edges; // rỗng: di chuyển
        QPoint start;
        QRect geometry;
    } drag_;
    bool resizable_ = false;
    bool pickHidden_ = false;
    bool reshowing_ = false;
};

template <class Load, class Build>
void DialogFrame::loadBody(Load load, Build build, bool flush)
{
    using T = std::decay_t<std::invoke_result_t<Load&, sched::CancelToken>>;
    setLoading();
    load_.cancel();
    load_ = sched::CancelSource();
    sched::submit(sched::Priority::Interactive, [rx = Receiver(this), token = load_.token(), load = std::move(load),
                                                 build = std::move(build), this, flush]() mutable {
        auto data = std::make_shared<T>(load(token)); // nền: không chạm `this`
        if (token.cancelled())
            return;
        post(rx, [this, token, data, build, flush]() mutable {
            if (!token.cancelled())
                setBody(build(std::move(*data)), flush);
        });
    });
}

} // namespace trion::ui
