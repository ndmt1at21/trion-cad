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
// "ui/window/customLayout" (bố cục palette người dùng đã đổi), "ui/window/commandLineFloat" (dòng lệnh nổi: chỗ, rộng
// theo tọa độ vùng vẽ; rỗng: ở cột); dock, ghim, dải: "ui/palettes/…" của PaletteHost.
// Lưu khi đóng cửa sổ (closeEvent) hoặc save(); trước khi lưu phát aboutToSave (Clean Screen tắt, lưu bố cục thật).
//
// Dòng lệnh nổi (07.1 §8; UIA-007 CommandLine::setFloating, floatRequested, dockRequested): setCommandLineFloating
// đưa widget của vùng CommandLine ra khỏi cột thành con của vùng vẽ, hoặc về lại ô của nó; bên lắp nối
// commandLineFloatChanged tới CommandLine::setFloating. Vùng vẽ đổi cỡ: dòng lệnh nổi được kẹp lại trong vùng vẽ; đổi
// widget vùng vẽ (setRegion): dòng lệnh chuyển sang vùng vẽ mới, giữ chỗ và rộng.
// Hàng dưới ribbon (UIA-101 "Hiện bên dưới ribbon"): addBelowRibbon(QAT); hàng theo thanh tiêu đề (Clean Screen ẩn
// cả hai; RIBBONCLOSE chỉ ẩn ribbon, QAT vẫn dùng được như RIBBONCLOSE.md mục 4).
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

#include <QHash>
#include <QMainWindow>
#include <QPoint>
#include <QRect>

#include <array>
#include <functional>
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
    // palette đã lưu (chưa có, hỏng, hoặc chưa từng đổi: bố cục mặc định của workspace theo lớp màn hình), dòng lệnh
    // nổi (chỗ, rộng đã lưu, kẹp theo vùng vẽ mỗi lần nó đổi cỡ). Gọi trước show().
    void restore();
    void save(); // aboutToSave, rồi ghi hình học, bố cục palette, dòng lệnh nổi; chưa restore() thì không ghi

    // Dòng lệnh nổi trên vùng vẽ. true: widget của vùng CommandLine rời cột, thành con của region(Canvas), phát
    // commandLineFloatChanged(true, at) rồi hiện (trừ khi vùng bị ẩn tường minh: COMMANDLINEHIDE); đang nổi thì chỉ phát
    // lại (dời tới at, kẹp trong vùng vẽ). false: phát commandLineFloatChanged(false, …) rồi widget về ô của nó trong
    // cột (giữa tab layout và thanh trạng thái). at: góc trên trái, tọa độ màn hình. Ẩn/hiện, Clean Screen như khi ở cột.
    // Chỗ đã chọn (cả khi người dùng kéo) được giữ: vùng vẽ nhỏ lại thì kẹp, lớn lại thì về đúng chỗ.
    void setCommandLineFloating(bool floating, QPoint at = {});
    bool isCommandLineFloating() const { return floating_; }

    // Hàng dưới ribbon: w vào hàng (cha thành hàng, xếp từ trái). Hàng hiện khi có widget và thanh tiêu đề không bị ẩn;
    // widget rời hàng (layout khác lấy, setParent) thì hàng rỗng tự ẩn.
    void addBelowRibbon(QWidget* w);
    QWidget* belowRibbon() const { return belowRibbon_; } // hàng (test, thứ tự Tab)

    // Nội dung thật cho palette giữ chỗ (props, layers, fontdoc, tcvnlib, blocks): create dựng ở lần palette hiện đầu,
    // nhận ô giữ chỗ (EmptyState "có ở bản sau") để dùng tiếp khi tính năng chưa có ở hồ sơ đang dùng; không dùng thì
    // hủy nó. Gọi trước lần hiện đầu (trước restore()). ponytail: palette thật đăng ký thay ô giữ chỗ khi UIC-002,
    // UIC-009, UIJ-002, UIJ-011, UIG-015 có (bỏ API này).
    void setPaletteContent(const QString& id, std::function<QWidget*(QWidget* placeholder)> create);

signals:
    void aboutToSave();
    // Bên lắp nối tới CommandLine::setFloating(floating, at), đồng bộ: true khi widget đã là con của vùng vẽ (cũng khi
    // dời, kẹp lại); false trước khi widget về cột (gom phần nó đặt vào vùng vẽ).
    void commandLineFloatChanged(bool floating, QPoint at);

protected:
    bool eventFilter(QObject* watched, QEvent* e) override; // vùng vẽ đổi cỡ; dòng lệnh dời; widget rời hàng dưới ribbon
    void closeEvent(QCloseEvent* e) override;
    // Vùng trống của thanh tiêu đề (sự kiện widget con không nhận): kéo → di chuyển cửa sổ, nhấp đúp → phóng to.
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;

private:
    bool onTitleBar(QPoint p) const; // p: tọa độ của MainWindow
    void registerPalettes();
    void floatCommandLine(const QRect& want); // nổi (rời cột nếu chưa) ở chỗ, rộng want của vùng vẽ (rộng 0: mặc định)
    void placeFloating();                     // dòng lệnh nổi về floatAt_, kẹp trong vùng vẽ hiện tại
    void syncBelowRibbon();

    settings::Store& store_;
    QVBoxLayout* column_ = nullptr;
    QMainWindow* docks_ = nullptr;
    std::array<QWidget*, kRegionCount> regions_{};
    QWidget* belowRibbon_ = nullptr;
    ui::PaletteHost* palettes_ = nullptr;
    Workspace* workspace_ = nullptr;
    ui::ScreenClass screenClass_ = ui::ScreenClass::Wide;
    bool restored_ = false;
    bool floating_ = false;
    bool placing_ = false; // đang đặt dòng lệnh nổi (bỏ qua Move, Resize do chính mình gây ra)
    QRect floatAt_;        // dòng lệnh nổi: chỗ (tọa độ vùng vẽ) người dùng chọn hoặc đã lưu, rộng; chỉ topLeft, width
    QPoint placed_;        // chỗ vừa đặt (đã kẹp): Move tới chỗ khác là người dùng kéo
    QHash<QString, std::function<QWidget*(QWidget*)>> contents_; // nội dung thật của palette giữ chỗ
    std::optional<QPoint> pressAt_; // nhấn trái trên thanh tiêu đề, chờ kéo đủ xa
};

} // namespace trion::shell
