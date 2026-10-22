#pragma once
// UIA-002 · Quick Access Toolbar bản V (07.1 UI-A04; prototype renderTitle và qatMenu trong js/shell.js; .qat, .qsplit
// trong css/app.css): Mở, In (nút tách: In, In nhiều layout, In theo khung tên, Xem trước), nút ▾ mở menu tùy biến
// bật/tắt các mục (Mở, In, Xem trước bản in, Bác sĩ Font, Khớp thuộc tính, In hàng loạt).
//
// Nút dùng QAction của ActionRegistry: bấm = trigger (cổng phiên bản UI0-011 rồi chuỗi lệnh tới INT-002); tooltip,
// phím, khóa theo QAction. Lệnh bị khóa: nút có ổ khóa, mục trong menu tùy biến có huy hiệu "Pro"/"Viewer Pro", bật
// mục đó thì mở trang nâng cấp thay vì thêm nút.
//
// ponytail: QAT bản V; UIA-101 (EA) thêm Mới, Lưu, Lưu thành, Hoàn tác/Làm lại (danh sách bước), "Hiện bên dưới
// ribbon", "Hiện thanh menu", "Thêm vào QAT" từ ribbon: thêm dòng vào kItems trong Qat.cpp. Bật/tắt mục chỉ giữ trong
// phiên; UIA-101 lưu qua NT-010.

#include <QPointF>
#include <QWidget>

#include <vector>

class QAction;
class QColor;
class QPainter;

namespace trion::actions {
class ActionRegistry;
}
namespace trion::edition {
class EditionGate;
}
namespace trion::ui {
class Button;
class Menu;
} // namespace trion::ui

namespace trion::shell {

// Mũi tên xuống .ic.xs (9 px, nét --sw × 1,5) căn giữa tại c: nút ▾ của QAT, nút menu ứng dụng.
void drawChevron(QPainter& p, QPointF c, const QColor& color);

class Qat : public QWidget {
    Q_OBJECT
public:
    // actions, gate sống lâu hơn QAT. Cần ThemeManager, IconProvider, RichTooltip.
    Qat(actions::ActionRegistry& actions, edition::EditionGate& gate, QWidget* parent = nullptr);

    ui::Button* button(const QString& command) const; // nút của lệnh (kể cả đang ẩn); nullptr nếu không có
    ui::Menu* menu() const { return menu_; }          // menu tùy biến của nút ▾

protected:
    void changeEvent(QEvent* e) override;

private:
    struct Item {
        QAction* action;     // QAction của ActionRegistry
        ui::Button* button;  // nút trên QAT
        QAction* toggle;     // mục bật/tắt trong menu tùy biến
        const char* label;   // nhãn của mục (nguồn tiếng Việt)
    };
    void sync(); // nhãn, tooltip, khóa theo QAction (đổi ngôn ngữ, phiên bản)

    std::vector<Item> items_;
    ui::Menu* menu_;
    QAction* heading_;
    ui::Menu* printMenu_ = nullptr;
    ui::Button* more_;
};

} // namespace trion::shell
