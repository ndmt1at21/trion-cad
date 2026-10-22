#pragma once
// UIA-112 · Nút chế độ hỗ trợ vẽ trên thanh trạng thái (UI-A08; 07.1 §10 dòng Lưới … Hiển thị độ trong suốt;
// prototype js/shell.js renderStatus, toggle, osnapMenu; css/app.css .sbt).
//
// Dải nút theo thứ tự prototype: GRID F7, SNAP F9, DYN F12, ORTHO F8, POLAR F10, OTRACK F11, OSNAP F3, hiển thị
// lineweight, hiển thị độ trong suốt. Mỗi nút là QToolButton::setDefaultAction(QAction gắn sysvar của ActionRegistry):
// bấm, Space, Enter, phím F gửi 'TÊN giá-trị (trong suốt: F8 khi đang LINE không ngắt lệnh) tới sink của registry;
// trạng thái chỉ theo Sysvars::changed, đồng bộ (sysvar đổi từ LISP, .scr hiện lên nút ngay). Bật: nền --c-on-bg,
// biểu tượng --c-on và gạch dưới --c-on (phân biệt không cần màu); trình đọc màn hình: nút bật/tắt, trạng thái chọn.
// Tooltip 2 mức (RichTooltip): tên, tên sysvar · phím (Keymap), mô tả; F1 mở trợ giúp của sysvar.
//
// Chuột phải, phím Menu, Shift+F10: menu của nút (07.1 §10). OSNAP: chế độ có ô chọn theo bit OSMODE của AutoCAD (menu
// mở tiếp để bật/tắt nhiều chế độ), Chọn tất cả, Bỏ tất cả. POLAR: góc tăng 90 … 5 và góc người dùng (POLARANG), Góc
// khác… (dòng lệnh hỏi giá trị). SNAP: snap lưới / polar (SNAPTYPE). DYN: nhập con trỏ, nhập kích thước (bit DYNMODE),
// prompt động (DYNPROMPT). "Thiết lập…" mở hộp thoại ('_+DSETTINGS <tab> như AutoCAD, '_LWEIGHT); lệnh đó khóa theo
// phiên bản thì kích hoạt QAction của lệnh để cổng UI0-011 mở trang nâng cấp. Giá trị không phải toggle (mặt nạ
// OSMODE, góc thực POLARANG…) và "Thiết lập…" phát macroRequested.
//
// Dùng (luồng UI, sau ActionRegistry, EditionGate::bind, RichTooltip, IconProvider):
//   auto* modes = new trion::shell::ModeToggles(actions, sysvars);   // UIA-009 đặt vào thanh trạng thái
//   connect(modes, &ModeToggles::macroRequested, router, …);         // chỗ nối INT-002 như ActionRegistry::setSink
//   connect(&gate, &EditionGate::profileChanged, modes, &ModeToggles::setProfile);
//   modes->setProfile(gate.profile());
//
// ponytail: Isometric (P1) chờ ActionRegistry có hành động "iso" gắn SNAPSTYL (UIA-001/INT-005): có thì nút và menu
// mặt trái/trên/phải (SNAPISOPAIR) tự hiện; F5 đổi mặt đã chạy qua hành động "isop" của registry.

#include "app/core/edition/EditionGate.h"

#include <QToolButton>
#include <QWidget>

#include <utility>
#include <vector>

class QAction;
class QMenu;

namespace trion::actions {
class ActionRegistry;
class Sysvars;
} // namespace trion::actions

namespace trion::shell {

// Nút biểu tượng .sbt của thanh trạng thái, dùng chung cho dải nút chế độ và StatusBar (UIA-009): rộng --h-btn + --sp-2,
// cao --h-btn. Nền và màu biểu tượng theo TrionStyle (.ib: rê --c-hover, nhấn --c-press, bật --c-on-bg và --c-on); bật
// thêm gạch dưới cao --sp-1 màu --c-on để phân biệt không cần màu (07.3 Phụ lục C); dot: chấm --c-danger góc trên phải
// (.sbt.has-dot, có thông báo chưa đọc). Enter như Space; menu (con của nút): chuột phải, phím Menu, Shift+F10 mở lên
// trên (prototype).
class StatusButton : public QToolButton {
public:
    explicit StatusButton(QWidget* parent = nullptr);
    QMenu* menu = nullptr; // menu chuột phải
    bool dot = false;

    // Mở m lên trên anchor: mép trái thẳng anchor, hoặc mép phải (alignRight, .sb cuối thanh như prototype).
    static void popupAbove(QMenu* m, const QWidget* anchor, bool alignRight = false);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;
};

class ModeToggles : public QWidget {
    Q_OBJECT
public:
    // actions, sysvars sống lâu hơn dải nút.
    ModeToggles(actions::ActionRegistry& actions, actions::Sysvars& sysvars, QWidget* parent = nullptr);

    // Viewer: chỉ OSNAP ("Bắt điểm (khi đo)"), lineweight, độ trong suốt (07.1 §10, UI0-011). Mặc định Full.
    void setProfile(edition::UiProfile profile);

    // Nút theo id hành động của ActionRegistry ("ortho", "LWDISPLAY"); nullptr nếu không có. Menu là con của nút.
    QToolButton* button(const QString& id) const;

    struct Spec;   // bảng nút, ModeToggles.cpp
    struct Choice; // mục menu gắn sysvar, ModeToggles.cpp

signals:
    // Chuỗi lệnh không đi qua QAction của registry: 'OSMODE 37, 'POLARANG 22.5, '_+DSETTINGS 2 (chỗ nối INT-002).
    void macroRequested(const QString& macro);

protected:
    void changeEvent(QEvent* e) override;                   // LanguageChange
    bool eventFilter(QObject* watched, QEvent* e) override; // menu OSNAP mở tiếp sau khi bật/tắt một chế độ

private:
    void addChoice(QMenu* menu, const Choice& c);
    void addAngles(QMenu* menu);
    void setSysvar(const QString& name, int value); // 'TÊN giá-trị
    void openSettings(int tab);
    void sync(); // dấu chọn của menu theo Sysvars
    void retranslate();

    actions::ActionRegistry& actions_;
    actions::Sysvars& sysvars_;
    bool viewer_ = false;
    std::vector<std::pair<const Spec*, QToolButton*>> buttons_;
    std::vector<std::pair<QAction*, const Choice*>> choices_;
    std::vector<QAction*> angles_; // góc polar, data() là góc ("22.5"); cuối là góc người dùng
    std::vector<std::pair<QAction*, const char*>> labels_; // chữ gốc của mục menu
};

} // namespace trion::shell
