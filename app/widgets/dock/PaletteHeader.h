#pragma once
// UI0-113 · Đầu palette (prototype .pal-tabs, js/shell.js dock()): tiêu đề vẽ như tab đang mở (nền --c-base, gạch
// 2 px --c-accent phía trên), rồi các nút .ib 22 px: ghim (bật = đang ghim), menu (…), đóng. Nền --c-chrome, nét chia
// 1 px --c-border ở đáy. Là titleBarWidget của QDockWidget: chỗ trống của đầu bỏ qua sự kiện chuột nên QDockWidget
// nhận kéo (dock/nổi/gộp tab) và nhấp đúp (nổi ⇄ dock) như thanh tiêu đề chuẩn.
//
// Tên cho trình đọc màn hình theo aria-label của prototype: "Ghim palette" / "Tự ẩn palette" (theo trạng thái),
// "Tùy chọn palette", "Đóng palette"; tiêu đề lấy từ windowTitle của dock (đổi theo ngôn ngữ ở PaletteHost).
//
// ponytail: nhóm tab dùng dải tab của QMainWindow (phía trên đầu palette) nên nhóm cao thêm một dải; prototype gộp tab
// vào chính đầu palette. Gộp lại nếu ảnh chụp QA-011 đòi (UI0-010 không cần cho tiêu chí nên giữ).
#include <QWidget>

class QDockWidget;
class QLabel;

namespace trion::ui {

class Button;
class ToggleButton;

class PaletteHeader : public QWidget {
    Q_OBJECT
public:
    explicit PaletteHeader(QDockWidget* dock);

    QLabel* titleLabel() const { return title_; }
    ToggleButton* pinButton() const { return pin_; } // checked = đang ghim; PaletteHost nghe toggled
    Button* menuButton() const { return menu_; }     // PaletteHost mở menu khi clicked
    Button* closeButton() const { return close_; }   // đóng dock

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* e) override;
    void changeEvent(QEvent* e) override; // LanguageChange: tên, tooltip; StyleChange: cỡ theo token

private:
    void retranslate();
    void applyMetrics();

    QLabel* title_;
    ToggleButton* pin_;
    Button* menu_;
    Button* close_;
};

} // namespace trion::ui
