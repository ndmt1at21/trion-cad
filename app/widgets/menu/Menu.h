#pragma once
// UI0-107 · Menu thả xuống và ngữ cảnh theo prototype (TC.menu trong js/ui.js; .menu .mi .mh .ms .k .ed.xs trong
// css/app.css; styleguide.html mục Menu, tooltip). Là QMenu nên giữ hành vi chuẩn: mũi tên, Enter, Esc đóng từng cấp,
// mnemonic "&" theo chữ của ngôn ngữ đang dùng (chủ menu đặt lại chữ khi LanguageChange), menu con, cuộn khi dài hơn
// màn hình, tách thanh menu hệ thống macOS (NSMenu tự vẽ, tự hiện phím của QAction).
//
// Vẽ: nền, viền, lề, vạch chia do style của ứng dụng (TrionStyle). Mục (.mi) và tiêu đề nhóm (.mh, addSection) do
// Menu đo và vẽ, đọc token mỗi lần: cột biểu tượng hoặc dấu chọn --ic, tên, huy hiệu phiên bản khi khóa, phím (Mono
// --fs-xs --c-text-3, nhãn theo nền tảng từ Keymap của RichTooltip, đổi ngay khi Keymap đổi), mũi tên menu con.
// Phím, khóa, lệnh, mô tả của mục gắn vào QAction bằng RichTooltip::attach; rê mục có thông tin thì hiện tooltip 2 mức,
// F1 khi rê phát RichTooltip::helpRequested. Trình đọc màn hình: tên mục, phím (Accelerator), "Có trong TrionCAD …"
// (mục khóa, như huy hiệu) và mô tả (Description).
//
// Dùng (sau ThemeManager, IconProvider, RichTooltip như main):
//   auto* m = new Menu(parent);
//   m->addSection(tr("Có vùng chọn"));                // tiêu đề nhóm; giữa menu: addSeparator() trước nó
//   QAction* props = m->addAction(IconProvider::icon(u"props"_s), tr("&Thuộc tính"));
//   RichTooltip::attach(props, {.command = u"PROPERTIES"_s, .keyId = u"props"_s});
//   RichTooltip::attach(m->addAction(tr("Chọn tương tự")), {.command = u"SELECTSIMILAR"_s, .need = Edition::Pro});
//   m->addMenu(new Menu(tr("Lệnh &gần đây"), m));      // menu con cũng là Menu
//   splitButton->setDropMenu(m);                       // hoặc m->popup(pos)
// Khóa (TipInfo::need) hiện huy hiệu và báo trợ năng; chặn lệnh, mở trang nâng cấp là cổng UI0-011 trong ActionRegistry
// (QAction vẫn phát triggered). Phím hiện từ Keymap, không từ QAction::shortcut (Menu đặt shortcutVisibleInContextMenu
// = false cho action thêm vào); menu của QMenuBar mà QAction có shortcut thì Qt vẫn chừa cột phím của nó (rộng hơn
// prototype).

#include <QMenu>
#include <QPointer>

namespace trion::ui {

class Menu : public QMenu {
    Q_OBJECT
public:
    explicit Menu(QWidget* parent = nullptr);
    explicit Menu(const QString& title, QWidget* parent = nullptr);

    // Nhãn phím của mục theo Keymap của RichTooltip::instance() ("Ctrl+S", "⌘S"); rỗng nếu không có.
    static QString keyText(const QAction* action);

protected:
    void initStyleOption(QStyleOptionMenuItem* option, const QAction* action) const override;
    void actionEvent(QActionEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void leaveEvent(QEvent* e) override;
    void hideEvent(QHideEvent* e) override;

private:
    void relayout();              // đo lại mọi mục (phím đổi)
    QPointer<QAction> tipAction_; // mục đang rê có tooltip
};

} // namespace trion::ui
