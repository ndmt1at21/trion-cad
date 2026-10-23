#pragma once
// UIA-016 · Hướng dẫn nhanh 5 bước trên cửa sổ chính (UI-A15 bước 8; prototype SC.tour trong js/screens1.js, "coach
// mark trên giao diện thật"). Mỗi bước tô sáng một vùng theo thứ tự của prototype:
//   1 vùng vẽ · 2 ribbon · 3 palette Thuộc tính (docked; đang thu ở dải: tab của nó) · 4 thanh trạng thái (chỉ báo Bác
//   sĩ Font nếu đang hiện) · 5 dòng lệnh
// Vùng không hiện thì bỏ qua bước đó, số bước vẫn tính trên 5 (như prototype).
//
// Hình: lớp phủ con của cửa sổ chính phủ --c-scrim ngoài vùng tô sáng, viền 2 px --c-accent quanh vùng (cách mép 3 px,
// bo --r-xl); lớp phủ không nhận chuột (pointer-events:none của prototype): vùng vẽ, ribbon… vẫn dùng được. Thẻ .mcard
// rộng 300 px cạnh vùng: "Bước i / n", câu hướng dẫn, [Kết thúc] … [Tiếp | Xong]; dưới vùng, không đủ chỗ thì trên;
// vùng cao hơn nửa cửa sổ (vùng vẽ) thì thẻ nằm trong vùng, cách góc trên trái.
// Phím (focus ở nút của thẻ; mỗi bước focus về nút Tiếp): Enter/Space bấm nút; Tab, Shift+Tab giữa hai nút theo thứ tự
// Tab của cửa sổ (không bẫy focus); → ↓ bước sau, ← ↑ bước trước; Esc kết thúc. Trình đọc màn hình: mỗi bước thông báo
// "Hướng dẫn nhanh, bước i / n: <tên vùng>. <câu>" (tên vùng theo MainWindow::regionName), mô tả của nút Tiếp cùng câu
// đó. Không timer, không animation. Kết thúc: gỡ lớp phủ và thẻ, focus đang ở thẻ thì về vùng vẽ, phát finished rồi tự
// hủy (deleteLater).
//
// Dùng: (new QuickTour(window))->start();   // Onboarding gọi sau bước cuối
#include <QObject>
#include <QPointer>
#include <QString>

class QEvent;
class QLabel;
class QWidget;

namespace trion::ui {
class Button;
}

namespace trion::shell {

class MainWindow;

class QuickTour : public QObject {
    Q_OBJECT
public:
    static constexpr int kStops = 5;

    explicit QuickTour(MainWindow& window); // cha: window
    ~QuickTour() override;                  // gỡ lớp phủ, thẻ

    void start();    // bước đầu có vùng đang hiện; không có vùng nào: kết thúc ngay
    void next();     // bước sau; ở bước cuối: kết thúc (completed)
    void previous(); // bước trước có vùng đang hiện; ở bước đầu: giữ nguyên
    void finish(bool completed = false); // Kết thúc, Esc; gọi lại sau khi đã kết thúc: không làm gì

    bool isRunning() const { return current_ >= 0; }
    int current() const { return current_; } // 0…kStops-1, -1 khi không chạy
    QWidget* target() const { return target_; }
    QString targetName() const; // tên vùng đang tô sáng, đã dịch
    QWidget* overlay() const { return overlay_; }
    QWidget* card() const { return card_; }

signals:
    void stepChanged(int index);
    void finished(bool completed); // completed: đi hết bước cuối (Xong), không phải Kết thúc/Esc

protected:
    bool eventFilter(QObject* watched, QEvent* e) override; // cửa sổ, vùng đổi cỡ; phím của thẻ

private:
    QWidget* resolve(int stop) const; // widget của bước, nullptr nếu không hiện
    void show(int stop);
    void place();
    void retranslate();

    MainWindow& window_;
    QWidget* overlay_;
    QWidget* card_;
    QLabel* title_;
    QLabel* text_;
    ui::Button* end_;
    ui::Button* next_;
    QPointer<QWidget> target_;
    int current_ = -1;
};

} // namespace trion::shell
