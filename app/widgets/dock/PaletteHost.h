#pragma once
// UI0-113 · Palette dock cơ sở (UI-A01, 07.1 §3): palette nằm trên dock của QMainWindow — trái, phải, dưới, nổi, nhóm
// tab (gộp: thả palette vào giữa palette khác, hoặc "Chuyển sang dock …" trong menu của đầu palette; tách: kéo tab
// ra) — do QMainWindow và QDockWidget của Qt làm (GroupedDragging: kéo đầu palette mang cả nhóm, kéo tab mang một
// palette; nhóm nổi được), không viết lại docking. PaletteHost thêm:
//   - đăng ký palette theo mã với hàm dựng: dock và đầu palette tạo ngay (rẻ), nội dung dựng ở lần đầu palette thật sự
//     hiện (tab nền của nhóm chưa dựng); khung đầu đo bằng UiProbe palette_open (≤ 150 ms, DoD-L);
//   - đầu palette theo prototype (PaletteHeader); tên cho trình đọc màn hình là tiêu đề (windowTitle của dock);
//   - lưu, khôi phục bố cục qua Store (NT-010); palette nổi trên màn hình không còn thì về màn hình chính, nằm trọn
//     trong vùng làm việc (fitToScreens);
//   - F6 / Shift+F6 đưa focus lần lượt qua các palette đang hiện rồi về vùng vẽ; Esc trong palette trả focus về
//     widget trung tâm (vùng vẽ).
//   - trợ năng của nhóm tab (DoD-U 7): dải tab Qt tạo khi gộp (QMainWindowTabBar; tên cho trình đọc màn hình là chữ
//     của tab đang mở) có viền focus --c-focus quanh tab đang mở khi Tab tới. Qt cất palette ở tab nền và dải của
//     nhóm còn dưới hai palette mở ra tọa độ âm mà vẫn isVisible(): palette cất bị vô hiệu, dải cất không nhận focus
//     (không còn điểm dừng Tab không thấy); về chỗ thì trả lại.
//
// Dùng (UIA-001; sau ThemeManager, IconProvider; trước window.show()):
//   auto* host = new PaletteHost(&window);                 // con của window, sống theo window
//   host->registerPalette({u"props"_s, [] { return MainWindow::tr("Thuộc tính"); },
//                          [] { return new PropertiesPalette; }, Qt::RightDockWidgetArea, true});
//   host->restoreLayout();                                  // bố cục đã lưu; không có hoặc hỏng thì giữ mặc định
//   window.show();
//   host->showPalette(u"layers"_s);                         // lệnh LAYER, phím Ctrl+1… (UI0-015)
// Bố cục lưu khi cửa sổ nhận QEvent::Close, hoặc khi gọi saveLayout().
//
// UI0-010 · Tự ẩn, quy tắc theo màn hình, đổi DPI:
//   - Ghim là trạng thái của từng palette (isPinned, setPinned, pinnedChanged; nút ghim trên đầu palette), độc lập
//     với mở/đóng. Bỏ ghim palette đang mở: thu vào dải 22 px ở cạnh của nó (AutoHideStrip, strip(cạnh)); ghim lại:
//     về đúng chỗ dock cũ. showPalette() palette bỏ ghim: vào dải nếu đang đóng, trượt ra không lấy focus. Đóng
//     palette tự ẩn: ra khỏi dải, vẫn bỏ ghim (mở lại thì lại trượt ra). moveTo() ghim palette. Lưu cả palette
//     trong dải ("ui/palettes/strip").
//   - Bố cục mặc định theo lớp màn hình (restoreLayout(screen), DisplayContext::classify): Wide (≥ 1600 px) theo
//     PaletteSpec (visible, pinned); hẹp hơn: mọi palette bỏ ghim, palette mặc định mở nằm ở dải.
//   - Palette nổi (một palette hoặc nhóm tab nổi) được DisplayContext::track sau trackDisplay(); đổi DPI hoặc màn
//     hình (không đang kéo) thì về trọn vùng làm việc; rút màn hình lúc đang chạy thì về màn hình còn lại
//     (fitToScreens). Palette đang trượt ra theo dải (đổi cỡ, đổi chỗ khi cửa sổ đổi màn hình, DPI).
//   Dùng (UIA-001): host->restoreLayout(DisplayContext::classify(window.screen()->geometry().width())) trước
//   window.show(); tạo DisplayContext display(&window) xong thì host->trackDisplay(display).
// ponytail: quy tắc theo màn hình chỉ cho bố cục mặc định (chưa lưu); đổi lớp màn hình lúc chạy (kéo cửa sổ sang
// màn hình hẹp) không tự bỏ ghim — nối DisplayContext::screenClassChanged nếu UIA-001 cần. Bật/tắt dock bằng
// toggleViewAction() của Qt bỏ qua dải: dùng showPalette()/close().
// ponytail: F6 / Shift+F6 đặt thẳng ở đây (QAction của cửa sổ); chuyển vào bảng phím một nguồn khi UI0-015 nối hành
// động, lúc đó gọi focusNextPalette().
#include "app/core/display/DisplayContext.h"
#include "core/base/settings/Settings.h"

#include <QHash>
#include <QList>
#include <QMap>
#include <QObject>
#include <QPointer>
#include <QRect>
#include <QSet>
#include <QStringList>

#include <functional>

class QDockWidget;
class QMainWindow;
class QWidget;

namespace trion::ui {

class AutoHideStrip;
class PaletteHeader;

struct PaletteSpec {
    QString id; // mã ổn định, ASCII ("props"): objectName của dock, khóa trong bố cục đã lưu
    std::function<QString()> title; // tiêu đề đã dịch (tr() của nơi đăng ký); gọi lại khi đổi ngôn ngữ
    std::function<QWidget*()> create; // dựng nội dung ở lần hiện đầu; dock sở hữu widget trả về
    Qt::DockWidgetArea area = Qt::RightDockWidgetArea; // chỗ mặc định: trái, phải, dưới; trùng chỗ thì gộp tab
    bool visible = false;                              // hiện trong bố cục mặc định
    bool pinned = true; // ghim trong bố cục mặc định ở màn hình rộng; false: palette mở nằm ở dải tự ẩn
};

class PaletteHost : public QObject {
    Q_OBJECT
public:
    // Đặt dockOptions, vị trí tab của window; store phải sống lâu hơn host.
    explicit PaletteHost(QMainWindow* window, settings::Store& store = settings::Store::instance());

    // Tạo dock (chưa dựng nội dung) với đầu palette, đặt theo spec.area: chỗ đó đã có palette thì gộp tab với nó.
    // Đăng ký sau restoreLayout() thì palette về chỗ đã lưu nếu có. Mã trùng hoặc thiếu hàm: TRION_CHECK.
    QDockWidget* registerPalette(PaletteSpec spec);
    QDockWidget* dock(const QString& id) const;     // nullptr nếu chưa đăng ký
    PaletteHeader* header(const QString& id) const; // nullptr nếu chưa đăng ký
    QWidget* content(const QString& id) const;      // nullptr nếu chưa dựng
    QStringList ids() const { return order_; }      // theo thứ tự đăng ký

    // Hiện palette, đưa lên đầu nhóm tab, dựng nội dung nếu chưa. Không lấy focus (vùng vẽ vẫn nhận phím gõ).
    // Palette bỏ ghim: trượt ra từ dải (vào dải trước nếu đang đóng).
    void showPalette(const QString& id);
    // Ghim rồi chuyển palette sang dock trái/phải/dưới (gộp tab với palette đang ở đó) hoặc nổi
    // (Qt::NoDockWidgetArea), và hiện.
    void moveTo(const QString& id, Qt::DockWidgetArea area);

    bool isPinned(const QString& id) const { return !unpinned_.contains(id); }
    // Bỏ ghim palette đang mở: thu vào dải ở cạnh của nó (nổi thì về dock mặc định trước); ghim palette đang ở dải:
    // về dock cũ và mở. Palette đang đóng chỉ đổi trạng thái.
    void setPinned(const QString& id, bool pinned);
    // Dải tự ẩn ở cạnh side (trái, phải, dưới); nullptr nếu chưa palette nào tự ẩn ở đó.
    AutoHideStrip* strip(Qt::DockWidgetArea side) const { return strips_.value(side); }

    void saveLayout();
    // Khôi phục bố cục đã lưu (dock, ghim, dải), kéo palette nổi về vùng làm việc. false: chưa lưu hoặc không đọc
    // được: bố cục mặc định theo screen (hẹp hơn Wide: mọi palette bỏ ghim, palette đang mở vào dải). screen cũng
    // là mặc định cho palette đăng ký sau mà bản lưu không có.
    bool restoreLayout(ScreenClass screen = ScreenClass::Wide);

    // Theo dõi DPI, màn hình của palette nổi (đang có và về sau) qua display; display sống lâu hơn host hoặc hủy
    // trước (QPointer).
    void trackDisplay(DisplayContext& display);

    // Focus vào palette đang hiện kế tiếp (trước đó) theo thứ tự đăng ký; sau palette cuối là vùng vẽ.
    void focusNextPalette(bool backward = false);
    void focusCanvas(); // widget trung tâm của window (hoặc focusProxy của nó)

    // Hàm thuần: chỗ của cửa sổ nổi r khi màn hình hiện có vùng làm việc workAreas (availableGeometry, tọa độ ảo).
    // Màn hình chứa tâm r (không có thì màn hình giao nhiều nhất với r; không giao màn hình nào thì
    // workAreas[primary]); r thu nhỏ nếu lớn hơn vùng đó rồi dời vào trọn trong vùng. workAreas rỗng: giữ r.
    static QRect fitToScreens(const QRect& r, const QList<QRect>& workAreas, qsizetype primary = 0);

signals:
    void pinnedChanged(const QString& id, bool pinned);

protected:
    // Close của window → saveLayout; ChildPolished: dải tab của nhóm, nhóm tab nổi (watch). Dải tab: Move (cất),
    // Paint (viền focus).
    bool eventFilter(QObject* watched, QEvent* e) override;

private:
    class Dock;
    Dock* find(const QString& id) const { return docks_.value(id); }
    void ensureContent(Dock* d);
    void focusInto(Dock* d);
    void showMenu(Dock* d);
    void dockAt(Dock* d, Qt::DockWidgetArea area);
    void applyPin(Dock* d, bool pinned); // trạng thái, nút ghim, tín hiệu; không chuyển palette
    void toStrip(Dock* d);
    AutoHideStrip* stripOf(const Dock* d) const;
    QStringList stripped() const; // mã palette trong các dải, theo cạnh rồi theo tab
    QList<QWidget*> floatingWindows() const;
    // Con Qt tạo cho nhóm tab: dải tab (QMainWindowTabBar: lọc sự kiện) hoặc nhóm tab nổi (QDockWidgetGroupWindow:
    // lọc ChildPolished vì Qt chuyển dải tab vào đó, DisplayContext::track).
    void watch(QWidget* w);

    QMainWindow* window_;
    settings::Store& store_;
    QHash<QString, Dock*> docks_;
    QStringList order_;
    QSet<QString> unpinned_;
    QMap<Qt::DockWidgetArea, AutoHideStrip*> strips_;
    QStringList stripIds_; // dải đã lưu: cho palette đăng ký sau restoreLayout
    QPointer<DisplayContext> display_;
    bool restored_ = false;
    bool narrow_ = false; // bố cục mặc định của màn hình hẹp hơn Wide
};

} // namespace trion::ui
