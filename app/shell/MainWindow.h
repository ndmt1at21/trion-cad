#pragma once
// UIA-001 · Cửa sổ chính UI-A01 (07.1 §3; prototype js/shell.js A.mount, css #app): bảy vùng xếp dọc như lưới #app
//   Thanh tiêu đề · Ribbon · Tab tệp · [palette trái | Vùng vẽ | palette phải] · Tab layout · Dòng lệnh · Thanh trạng
//   thái
// Palette dock (PaletteHost, UI0-113/UI0-010) chỉ nằm cạnh vùng vẽ, giữa tab tệp và tab layout như .ws của prototype:
// vùng dock là một QMainWindow con (dockHost) đặt giữa cột, nên QMainWindow ngoài không cần ô statusBar/menuWidget.
//
// Vùng chưa có widget thật là ô giữ chỗ cao đúng token (mật độ gọn/thoáng), nền và nét theo prototype, có tên cho
// trình đọc màn hình (regionName). Bước integration gọi setRegion(vùng, widget):
//   window.setRegion(MainWindow::Region::TitleBar, new TitleBar(...));   // nhận sở hữu, hủy ô giữ chỗ
//   window.setRegion(MainWindow::Region::Canvas, viewport);              // rồi actions.addViewport(viewport)
// Dòng lệnh: MainWindow đặt setProperty("compact", lớp màn hình hẹp hơn Wide) trên widget của vùng (hợp đồng UIA-107).
// Thanh tiêu đề: widget không nhận (accept) mousePress/mouseDoubleClick ở vùng trống; sự kiện lên tới đây: kéo →
// QWindow::startSystemMove, nhấp đúp → phóng to/khôi phục. Lề trái/phải chừa nút cửa sổ: contentsMargins của widget.
//
// Bố cục mặc định theo lớp màn hình (07.1 §3.2) do Workspace áp: Wide → palette Thuộc tính dock mở; hẹp hơn → palette
// tự ẩn ở dải, dòng lệnh compact. Đổi lớp lúc chạy (setScreenClass, nối DisplayContext::screenClassChanged): dòng lệnh
// luôn theo; bố cục palette áp lại chỉ khi vẫn là bố cục mặc định của workspace (Workspace::isDefaultLayout: cùng
// palette mở, cạnh, ghim, nổi, dải; không so cỡ, tab đang chọn). Người dùng đã đổi thì giữ nguyên.
//
// Lưu, khôi phục qua Store (NT-010): "ui/window/geometry" (saveGeometry: hình học thường, phóng to, màn hình; màn hình
// đã lưu không còn thì Qt đặt về màn hình chính, trong vùng làm việc), "ui/window/workspace" (Workspace),
// "ui/window/customLayout" (bố cục palette người dùng đã đổi); dock, ghim, dải: "ui/palettes/…" của PaletteHost.
// Lưu khi đóng cửa sổ (closeEvent) hoặc save(); trước khi lưu phát aboutToSave (Clean Screen tắt, lưu bố cục thật).
//
// Dùng (luồng UI, sau ThemeManager, IconProvider, Translator):
//   shell::MainWindow window(store);
//   window.restore();                                       // hình học, palette, workspace; trước show()
//   ui::DisplayContext display(&window);
//   window.setScreenClass(display.screenClass());
//   connect(&display, &ui::DisplayContext::screenClassChanged, &window, &shell::MainWindow::setScreenClass);
//   window.palettes().trackDisplay(display);
//   window.show();                                          // phóng to nếu lần trước phóng to
//
// ponytail: khung cửa sổ của hệ điều hành giữ nguyên (thanh tiêu đề của ta là hàng đầu vùng client, dưới khung hệ
// điều hành). Qt::ExpandedClientAreaHint (Qt 6.9+) chưa bật: chưa đo được trên Windows thật xem Qt 6.11 có đưa vùng
// client lên hàng tiêu đề không (test nativeTitleBar, chạy tay: TRION_NATIVE_PROBE=1 shell_test.exe -platform windows
// nativeTitleBar). Có hỗ trợ thì bật cờ trong restore() và đặt lề TitleBar theo QWindow::safeAreaMargins (UIA-002).
// Không tự viết WM_NCHITTEST.
#include "app/core/display/DisplayContext.h"
#include "core/base/settings/Settings.h"

#include <QMainWindow>
#include <QPoint>

#include <array>
#include <optional>

class QVBoxLayout;

namespace trion::ui {
class PaletteHost;
}

namespace trion::shell {

class Workspace;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    // Thứ tự = thứ tự trên cửa sổ (trên xuống).
    enum class Region : quint8 { TitleBar, Ribbon, FileTabs, Canvas, LayoutTabs, CommandLine, StatusBar };
    Q_ENUM(Region)
    static constexpr int kRegionCount = 7;

    // store phải sống lâu hơn cửa sổ. Dựng ô giữ chỗ, PaletteHost với các palette giữ chỗ, Workspace; chưa khôi phục.
    explicit MainWindow(settings::Store& store = settings::Store::instance(), QWidget* parent = nullptr);
    ~MainWindow() override;

    // Đặt widget thật cho vùng: nhận sở hữu, hủy widget cũ (ô giữ chỗ), giữ trạng thái ẩn/hiện của vùng. Vùng vẽ:
    // widget trung tâm của dockHost (focus, Esc của palette về đây); người gọi tự ActionRegistry::addViewport.
    void setRegion(Region r, QWidget* widget);
    QWidget* region(Region r) const { return regions_[size_t(r)]; }
    void setRegionVisible(Region r, bool visible);
    bool isRegionVisible(Region r) const; // không bị ẩn tường minh (đúng cả trước show())
    static QString regionName(Region r);  // tên cho trình đọc màn hình, đã dịch

    ui::PaletteHost& palettes() const { return *palettes_; }
    QMainWindow* dockHost() const
    {
        return docks_;
    } // vùng dock: palette, dải tự ẩn, toolbar; widget trung tâm = vùng vẽ
    Workspace& workspace() const { return *workspace_; }

    ui::ScreenClass screenClass() const { return screenClass_; }
    // Lớp màn hình chứa cửa sổ (DisplayContext::screenClassChanged; test truyền thẳng). Dòng lệnh compact khi hẹp hơn
    // Wide; bố cục palette áp lại theo quy tắc nếu vẫn là mặc định.
    void setScreenClass(ui::ScreenClass c);

    // Khôi phục hình học (lần đầu: phóng to trên màn hình chính), lớp màn hình theo màn hình chứa cửa sổ, bố cục
    // palette đã lưu (chưa có, hỏng, hoặc chưa từng đổi: bố cục mặc định của workspace theo lớp màn hình). Gọi trước
    // show().
    void restore();
    void save(); // aboutToSave, rồi ghi hình học, bố cục palette; chưa restore() thì không ghi

signals:
    void aboutToSave();

protected:
    void closeEvent(QCloseEvent* e) override;
    // Vùng trống của thanh tiêu đề (sự kiện widget con không nhận): kéo → di chuyển cửa sổ, nhấp đúp → phóng to.
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;

private:
    bool onTitleBar(QPoint p) const; // p: tọa độ của MainWindow
    void registerPalettes();

    settings::Store& store_;
    QVBoxLayout* column_ = nullptr;
    QMainWindow* docks_ = nullptr;
    std::array<QWidget*, kRegionCount> regions_{};
    ui::PaletteHost* palettes_ = nullptr;
    Workspace* workspace_ = nullptr;
    ui::ScreenClass screenClass_ = ui::ScreenClass::Wide;
    bool restored_ = false;
    std::optional<QPoint> pressAt_; // nhấn trái trên thanh tiêu đề, chờ kéo đủ xa
};

} // namespace trion::shell
