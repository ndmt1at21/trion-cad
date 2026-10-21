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
// Cho UI0-010 (tự ẩn thành dải, quy tắc theo màn hình, đổi DPI): dock(id) là QDockWidget thường (tín hiệu
// topLevelChanged, dockLocationChanged, visibilityChanged); ghim là trạng thái của host (isPinned, setPinned,
// pinnedChanged), nút ghim trên đầu palette đã nối, lưu cùng bố cục.
// ponytail: bỏ ghim chỉ đổi trạng thái và phát pinnedChanged; thu thành dải 22 px là UI0-010.
// ponytail: F6 / Shift+F6 đặt thẳng ở đây (QAction của cửa sổ); chuyển vào bảng phím một nguồn khi UI0-015 nối hành
// động, lúc đó gọi focusNextPalette().
#include "core/base/settings/Settings.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QRect>
#include <QSet>
#include <QStringList>

#include <functional>

class QDockWidget;
class QMainWindow;
class QWidget;

namespace trion::ui {

class PaletteHeader;

struct PaletteSpec {
    QString id; // mã ổn định, ASCII ("props"): objectName của dock, khóa trong bố cục đã lưu
    std::function<QString()> title; // tiêu đề đã dịch (tr() của nơi đăng ký); gọi lại khi đổi ngôn ngữ
    std::function<QWidget*()> create; // dựng nội dung ở lần hiện đầu; dock sở hữu widget trả về
    Qt::DockWidgetArea area = Qt::RightDockWidgetArea; // chỗ mặc định: trái, phải, dưới; trùng chỗ thì gộp tab
    bool visible = false;                              // hiện trong bố cục mặc định
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
    void showPalette(const QString& id);
    // Chuyển palette sang dock trái/phải/dưới (gộp tab với palette đang ở đó) hoặc nổi (Qt::NoDockWidgetArea), và hiện.
    void moveTo(const QString& id, Qt::DockWidgetArea area);

    bool isPinned(const QString& id) const { return !unpinned_.contains(id); }
    void setPinned(const QString& id, bool pinned);

    void saveLayout();
    // Khôi phục bố cục đã lưu, kéo palette nổi về vùng làm việc. false: chưa lưu hoặc không đọc được (giữ mặc định).
    bool restoreLayout();

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
    bool eventFilter(QObject* watched, QEvent* e) override; // Close của window → saveLayout

private:
    class Dock;
    Dock* find(const QString& id) const { return docks_.value(id); }
    void ensureContent(Dock* d);
    void focusInto(Dock* d);
    void showMenu(Dock* d);

    QMainWindow* window_;
    settings::Store& store_;
    QHash<QString, Dock*> docks_;
    QStringList order_;
    QSet<QString> unpinned_;
    bool restored_ = false;
};

} // namespace trion::ui
