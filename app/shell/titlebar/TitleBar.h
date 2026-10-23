#pragma once
// UIA-002 · Thanh tiêu đề (07.1 §3 dòng Thanh tiêu đề, UI-A01, UI-A04; prototype renderTitle, edBadge, userMenu,
// helpMenu trong js/shell.js; .tb .tb-app .tb-doc .ed trong css/app.css):
//   [≡ TrionCAD ▾] [QAT] … Banve-01.dwg* [Chỉ đọc] [Đã phục hồi] — TrionCAD Pro [Dùng thử: còn 12 ngày] … [Tìm lệnh] [👤] [?]
// Nút menu ứng dụng phát appMenuRequested (UIA-003 dựng menu; macOS dùng thanh menu hệ thống, nút ẩn, 07.1 §13.6).
// Tên bản vẽ cắt giữa khi thiếu chỗ, tooltip có đủ đường dẫn. Huy hiệu phiên bản edition::EditionBadge (dùng thử còn
// N ngày, cảnh báo giấy phép offline: EditionGate::setTrialDays/setOfflineDays). Menu tài khoản (tên, email, phiên
// bản, Trạng thái giấy phép, Kích hoạt, Quản lý tài khoản, Đăng xuất) và menu trợ giúp như prototype. Viewer, Viewer
// Pro: "[Chỉ xem]" (07.1 §2, EditionGate::profile).
//
// Khung cửa sổ là của MainWindow (UIA-001): thanh không vẽ nút cửa sổ; MainWindow chừa chỗ cho nút cửa sổ bằng
// setContentsMargins (bố cục ở trong contentsRect). Vùng trống và chữ tên bản vẽ không nhận mousePress,
// mouseDoubleClick: sự kiện lên MainWindow để kéo cửa sổ, nhấp đúp phóng to.
//
// Dùng (bước integration, luồng UI, sau ActionRegistry, EditionGate, RichTooltip):
//   auto* bar = new TitleBar(actions, gate);
//   mainWindow.setRegion(Region::TitleBar, bar);
//   connect(bar, &TitleBar::appMenuRequested, appMenu, &AppMenu::popupAt);           // UIA-003
//   connect(bar, &TitleBar::screenRequested, router, &Screens::open);                // "license"…
//   bar->search()->setLocator([&](const QString& c) { return ribbon.location(c); }); // UIA-102
//   bar->setDocument({.name = doc.name(), .path = doc.path(), .dirty = doc.modified()}); // DOC-018
//   QAT (bar->qat()): nối theo mục "Dùng" của Qat.h (chuỗi lệnh, "Thêm vào QAT", nguồn hoàn tác, dưới ribbon).

#include <QString>
#include <QUrl>
#include <QWidget>

#include <functional>

namespace trion::actions {
class ActionRegistry;
}
namespace trion::edition {
class EditionBadge;
class EditionGate;
} // namespace trion::edition
namespace trion::ui {
class Button;
class Menu;
} // namespace trion::ui

namespace trion::shell {

class CommandSearch;
class Qat;

// Bản vẽ đang mở. ponytail: chỗ nối tạm thay DOC-018 (tài liệu, trạng thái lưu); bước integration gọi setDocument
// khi đổi tab, sửa lần đầu, lưu.
struct DocInfo {
    QString name{};        // "Banve-01.dwg"; rỗng: không có bản vẽ (tab Start)
    QString path{};        // đường dẫn đủ (tooltip)
    bool dirty = false;    // chưa lưu: "*"
    bool readOnly = false; // "[Chỉ đọc]"
    bool recovered = false; // "[Đã phục hồi]"
};

// Tài khoản. ponytail: chỗ nối tạm thay PLF (đăng nhập, giấy phép).
struct Account {
    QString name{};  // "Nguyễn Minh An"
    QString email{}; // "an.nguyen@example.com"
    bool signedIn = false;
};

class TitleBar : public QWidget {
    Q_OBJECT
public:
    // actions, gate sống lâu hơn thanh. Cần ThemeManager, IconProvider, RichTooltip.
    TitleBar(actions::ActionRegistry& actions, edition::EditionGate& gate, QWidget* parent = nullptr);

    void setDocument(const DocInfo& doc);
    void setAccount(const Account& account);
    // Mở trang web (Quản lý tài khoản). Mặc định QDesktopServices::openUrl; test thay hàm giả.
    void setUrlOpener(std::function<bool(const QUrl&)> opener);

    CommandSearch* search() const { return search_; }
    Qat* qat() const { return qat_; }
    QString shownName() const; // tên bản vẽ đang hiện (cắt giữa khi thiếu chỗ); cho test, QA

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void appMenuRequested(QPoint globalPos); // góc dưới trái nút logo
    void signOutRequested();                 // xác nhận đăng xuất là việc của nơi nhận (PLF, UIL)
    // Màn hình chưa có lệnh trong danh mục: "license" (Trạng thái giấy phép, UI-L03), "activate" (kích hoạt, đăng
    // nhập), "migrate", "keys" (bài trợ giúp), "support", "feedback", "update".
    void screenRequested(const QString& id);

protected:
    void paintEvent(QPaintEvent* e) override;
    void changeEvent(QEvent* e) override;

private:
    class AppButton;
    class DocTitle;

    void sync(); // chữ, menu theo bản vẽ, tài khoản, phiên bản, ngày dùng thử, ngôn ngữ
    void popupBelow(ui::Menu* menu, QWidget* button);

    actions::ActionRegistry& actions_;
    edition::EditionGate& gate_;
    DocInfo doc_;
    Account account_;
    std::function<bool(const QUrl&)> openUrl_;

    AppButton* app_;
    Qat* qat_;
    DocTitle* title_;
    edition::EditionBadge* badge_;
    CommandSearch* search_;
    ui::Button* user_;
    ui::Button* help_;
    ui::Menu* userMenu_;
    ui::Menu* helpMenu_;
};

} // namespace trion::shell
