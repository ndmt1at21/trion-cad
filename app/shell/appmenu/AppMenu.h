#pragma once
// UIA-003 · Menu ứng dụng, bật từ nút logo (07.1 UI-A03, §2; prototype SC.appMenu trong js/screens1.js, .amenu trong
// css/app.css):
//
//   ┌──────────────────────────────────────────────────────────────┐
//   │ [🔍 Tìm lệnh hoặc tệp gần đây                              ] │
//   ├──────────────────┬───────────────────────────────────────────┤
//   │ 🗋 Mới           │ LỆNH                         (khi có chữ)  │
//   │ 📂 Mở            │ ◯ Đường tròn                      CIRCLE  │
//   │ 💾 Lưu           │ TỆP GẦN ĐÂY                     [Ngày ▾]  │
//   │ 💾 Lưu thành   › │ 📌 Banve-01.dwg               16/10/2026  │
//   │ 📄 Xuất        › │    D:\DuAn\NhaO-2T                        │
//   │ 🖨 In          › │ …  (cuộn; rê một tệp: ảnh xem trước)       │
//   │ 🔧 Tiện ích    › │                                           │
//   │ ✕ Đóng         › │                                           │
//   ├──────────────────┴───────────────────────────────────────────┤
//   │                          [⚙ Tùy chọn] [⎋ Thoát TrionCAD]     │
//   └──────────────────────────────────────────────────────────────┘
//
// - Mục trái: Mới, Mở, Lưu chạy QAction của ActionRegistry (UI0-015: chuỗi lệnh, cổng phiên bản UI0-011); Lưu thành,
//   Xuất, In, Tiện ích bản vẽ, Đóng mở menu con (ui::Menu) bên phải mục. Mục của menu con là QAction riêng của menu
//   kích hoạt QAction của ActionRegistry (đóng menu ứng dụng trước, hộp thoại của lệnh không mở dưới popup); mục không
//   có lệnh trong registry (Lưu thành theo phiên bản DWG/DXF/DWT, Xuất DXF, Đóng tất cả) phát macroRequested. Mục khóa
//   theo phiên bản có huy hiệu (Menu vẽ theo TipInfo::need) và mở trang nâng cấp thay vì chạy.
//   Hồ sơ Viewer (EditionGate::profile, 07.1 §2): ẩn mục cần Pro (Mới, Lưu, Lưu thành), giữ mục khác.
// - Cột phải: tệp gần đây (RecentFiles qua RecentFilter của riêng menu; ghim lên đầu, sắp theo ngày, tên, kích thước),
//   danh sách ảo hóa. Bấm, Enter → openRequested; bấm ghim cuối dòng (rê) hoặc menu chuột phải (phím Menu,
//   Shift+F10) để ghim/bỏ ghim. Rê một tệp: ảnh xem trước (ThumbnailQueue của menu) bên cạnh menu.
// - Ô tìm: mỗi phím lọc tệp (RecentFilter) và tìm lệnh bằng chỉ mục của ô Tìm lệnh (CommandSearch::search, UIA-002),
//   hiện tối đa kMaxCommands lệnh trên tệp. Enter chạy kết quả đầu (lệnh, không có thì tệp).
// - Bàn phím: mở menu thì focus ở ô tìm; ↓ từ ô tìm vào mục trái (chưa gõ) hoặc kết quả; ↑↓ trong danh sách (đầu danh
//   sách thì lên ô tìm); ←→ giữa hai cột (→ ở mục có menu con thì mở menu con, ← trong menu con thì đóng nó); Enter,
//   Space chạy; Tab qua ô tìm, mục, lệnh, cách sắp, tệp, hai nút. Esc đóng menu; focus về widget có focus trước khi mở
//   (vùng vẽ). Bấm ra ngoài: đóng và nuốt cú bấm (bấm lại nút logo không mở lại).
// Màu, cỡ đọc token lúc vẽ; ThemeManager::changed bố trí lại; LanguageChange dịch lại. Rảnh không timer. Luồng UI.
//
// Dùng (bước integration, sau ThemeManager, IconProvider, RichTooltip, EditionGate (bind), TitleBar; RecentFiles do
// main giữ, dùng chung với StartPage):
//   auto* menu = new trion::shell::AppMenu(actions, recent, *titleBar->search(), &mainWindow);
//   menu->thumbnails().setEmbeddedSource(dwgPreview);   // DWG-026, như StartPage
//   menu->thumbnails().setRenderSource(offscreen);      // RND-021
//   connect(titleBar, &TitleBar::appMenuRequested, menu, &AppMenu::popupAt);
//   connect(menu, &AppMenu::macroRequested, &router, &CommandRouter::submit);
//   connect(menu, &AppMenu::openRequested, app, &Application::requestOpen);
//
// ponytail: macOS dùng thanh menu hệ thống thay menu này (07.1 §13.6, nút logo ẩn) — mốc macOS alpha dựng QMenuBar từ
// cùng các QAction. Chuỗi lệnh Lưu thành theo phiên bản là đề xuất ("^C^C_SAVEAS _2013 ~": tùy chọn định dạng của
// prompt SAVEAS khi FILEDIA=0, "~" mở hộp thoại tệp UI-B02) tới khi CMD-244 chốt docs/specs/commands/SAVEAS.md. Cách
// sắp tệp chỉ giữ trong phiên.

#include "app/shell/start/RecentFiles.h"
#include "app/shell/start/ThumbnailQueue.h"

#include <QFrame>
#include <QStringList>

#include <array>
#include <cstdint>

class QAbstractItemView;
class QLabel;
class QStandardItemModel;

namespace trion::actions {
class ActionRegistry;
}
namespace trion::ui {
class Button;
class DropDown;
class Menu;
class SearchField;
} // namespace trion::ui

namespace trion::shell {

class CommandSearch;

class AppMenu final : public QFrame {
    Q_OBJECT
public:
    static constexpr int kItemsWidth = 240;   // .amenu cột trái
    static constexpr int kRecentWidth = 360;  // .amenu cột phải
    static constexpr int kItemHeight = 30;    // .amenu .ai
    static constexpr int kMaxCommands = 4;    // prototype: matchCmds(q).slice(0, 4)
    static constexpr int kVisibleFiles = 8;   // [đề xuất] dòng tệp thấy được, còn lại cuộn
    static constexpr int kPreviewWidth = 240; // ảnh xem trước 16:10 (như thẻ tab Start)

    // Mục trái theo thứ tự của prototype; ở hồ sơ Viewer, New, Save, SaveAs ẩn.
    enum Item : std::uint8_t { New, Open, Save, SaveAs, Export, Print, Utilities, Close, ItemCount };

    // actions, recent, commands sống lâu hơn menu. parent: cửa sổ chính (menu là popup của nó).
    AppMenu(actions::ActionRegistry& actions, RecentFiles& recent, const CommandSearch& commands,
            QWidget* parent = nullptr);
    ~AppMenu() override;

    ThumbnailQueue& thumbnails() { return thumbs_; }
    RecentFilter& filter() { return filter_; }

    // Cho test, QA.
    ui::SearchField* search() const { return search_; }
    QAbstractItemView* items() const;                                                      // mục trái
    QAbstractItemView* commands() const;                                                   // lệnh tìm được
    QAbstractItemView* files() const;                                                      // tệp gần đây
    ui::Menu* submenu(Item item) const { return item < ItemCount ? sub_[item] : nullptr; } // nullptr: mục không có
    QWidget* preview() const; // ảnh xem trước đang rê (cửa sổ nổi)

public slots:
    // Hiện với góc trên trái ở globalPos (TitleBar::appMenuRequested: góc dưới trái nút logo), trong màn hình. Đang mở
    // thì đóng.
    void popupAt(const QPoint& globalPos);

signals:
    void macroRequested(const QString& macro);    // mục không có QAction trong ActionRegistry
    void openRequested(const QStringList& paths); // tệp gần đây

protected:
    bool event(QEvent* e) override; // Esc không thành phím tắt của cửa sổ
    void paintEvent(QPaintEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void changeEvent(QEvent* e) override;
    bool eventFilter(QObject* watched, QEvent* e) override;

private:
    class List;
    class Delegate;
    class Preview;

    void build();
    void retranslate();
    void relayout();      // cỡ theo token hiện tại
    void rebuildItems();  // mục trái theo hồ sơ phiên bản và ngôn ngữ
    void fill(Item item); // dựng lại menu con lúc mở: chữ, phím, huy hiệu khóa đúng lúc đó
    void refreshResults();
    bool searchKey(QKeyEvent* k);
    bool listKey(List* view, QKeyEvent* k);
    void focusRow(List* view, int row);
    void focusRight();
    void activate(List* view, const QModelIndex& index, bool keyboard);
    void openSubmenu(int row, bool keyboard);
    void run(QAction* action);
    void openFile(const QString& path);
    void runFirst();
    void showContextMenu(const QPoint& viewportPos);
    void hover(List* view, const QModelIndex& index);
    void unhover();

    actions::ActionRegistry& actions_;
    RecentFiles& recent_;
    const CommandSearch& commands_;
    RecentFilter filter_;
    ThumbnailQueue thumbs_;
    ui::SearchField* search_ = nullptr;
    QWidget* body_ = nullptr;
    QWidget* left_ = nullptr;
    QWidget* right_ = nullptr;
    QWidget* footer_ = nullptr;
    QStandardItemModel* itemModel_ = nullptr;
    QStandardItemModel* cmdModel_ = nullptr;
    List* items_ = nullptr;
    QLabel* cmdTitle_ = nullptr;
    List* cmds_ = nullptr;
    QLabel* fileTitle_ = nullptr;
    ui::DropDown* sort_ = nullptr;
    List* files_ = nullptr;
    ui::Button* options_ = nullptr;
    ui::Button* quit_ = nullptr;
    std::array<ui::Menu*, ItemCount> sub_{};
    ui::Menu* context_ = nullptr; // chuột phải trên tệp
    Preview* preview_ = nullptr;
};

} // namespace trion::shell
