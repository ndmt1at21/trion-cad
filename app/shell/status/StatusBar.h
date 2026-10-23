#pragma once
// UIA-009 · Thanh trạng thái UI-A08 (07.1 §10, §12 Tiến trình; design/README.md Quy ước "Tác vụ nền"; prototype
// js/shell.js renderStatus, updateCoord, A.setTask; js/screens2.js SC.tasks; css/app.css .sb .sb-coord .sb-txt .sb-sep
// .sb-chip .sb-task .sbt .ed):
//   [x, y, z][MODEL] | nút chế độ UIA-112 | [tỉ lệ]    …    [tác vụ chính][⚠ Font: 3 thay thế][đồ họa][tác vụ][chuông][Pro][⛶][»][≡]
//
// - Tọa độ theo COORDS (Sysvars): 1 tuyệt đối "x, y, z"; 2 tương đối "khoảng cách<góc, z" khi lệnh đang hỏi điểm có
//   điểm gốc, không có thì tuyệt đối (như AutoCAD); 0 "Tọa độ tắt". Số chỉ qua RGN-013 (formatLinear, formatAngle; dấu
//   chấm). Bấm hoặc Ctrl+I: QAction "coord" của ActionRegistry ('COORDS n tới sink); chuột phải, phím Menu, Shift+F10:
//   menu Kiểu hiển thị. setCursorPoint chỉ lưu điểm và hẹn vẽ: tối đa một lần mỗi khung theo tần số màn hình chứa thanh,
//   chuột 1.000 Hz không thêm lần vẽ. Ô chỉ nới rộng (không nhảy theo từng điểm), đặt lại khi đổi đơn vị.
// - MODEL/PAPER: trong layout bấm chuyển Paper ↔ Model trong viewport (^C^C_MSPACE / ^C^C_PSPACE); tab Model: vô hiệu.
// - Nút chế độ: ModeToggles (UIA-112), macroRequested của nó đi ra macroRequested của thanh.
// - Tác vụ nền (NT-013 qua ProgressPresenter, UI0-112): tác vụ chính là InlineProgress kiểu gọn (vòng quay, nhãn
//   "· N tác vụ khác", thanh 80×4, nút ×); nút Tác vụ nền mở danh sách mọi việc đang hiện tiến trình, mỗi dòng có nút
//   hủy, rỗng thì có câu giải thích.
// - Bác sĩ Font: chỉ hiện khi có font thay thế hoặc đối tượng TCVN3; rê: "3 font đã thay thế · 250 đối tượng TCVN3";
//   bấm: FIXFONT (UI-J02). Đồ họa: rê: backend · GPU, FPS đọc lúc rê; bấm: GRAPHICSCONFIG (UI-L10). Clean Screen:
//   CLEANSCREENON (Ctrl+0, UIA-001). Lệnh đi qua QAction của ActionRegistry (cổng phiên bản, sink), không tự chạy.
// - Chuông: chấm khi NotificationCenter còn thông báo chưa đọc (số chưa đọc ở mô tả tiếp cận); bấm: screenRequested
//   ("notifications", UI-L14). Huy hiệu phiên bản edition::EditionBadge theo EditionGate (phiên bản, dùng thử còn N
//   ngày, cảnh báo offline: EditionGate::setTrialDays/setOfflineDays); bấm: screenRequested("license", UI-L03).
// - Viewer (UIA-017, UI-M01; prototype renderStatus khi viewer): nút chế độ còn Bắt điểm (khi đo), lineweight; sau dải
//   nút là tỉ lệ chỉ xem: tỉ lệ chú thích (Model, biểu tượng anno) hoặc tỉ lệ viewport đang kích hoạt (Layout,
//   vplock; không có viewport kích hoạt: "Paper"), không mũi tên; bấm: toast "Viewer chỉ xem tỉ lệ…" (ToastHost).
//   ponytail: hồ sơ Full chưa có dropdown tỉ lệ chú thích/viewport (EA, 07.1 §10): mục ẩn.
// - Tùy biến (≡): chọn mục hiện, lưu Store "ui/statusbar/hidden"; menu mở tiếp để đổi nhiều mục liền.
// - Thiếu chỗ (1366 px, vi-XL, mật độ thoáng): mục ưu tiên thấp vào menu tràn (») trước; không mục nào bị nén dưới cỡ
//   của nó nên không cắt chữ. Viewer: nút chế độ theo ModeToggles; Tùy biến, menu tràn chỉ có mục của hồ sơ.
// Rảnh: không timer, không vẽ lại. Chỉ luồng UI.
//
// Dùng (integration nhóm 6, sau ActionRegistry, EditionGate::bind, RichTooltip, ProgressPresenter, NotificationCenter):
//   auto* bar = new shell::StatusBar(actions, sysvars, gate, presenter, notes);  // hồ sơ, phiên bản theo gate
//   window.setRegion(MainWindow::Region::StatusBar, bar);
//   connect(bar, &StatusBar::macroRequested, &router, &CommandRouter::submit);    // chỗ nối INT-002
//   connect(bar, &StatusBar::screenRequested, …);  // "notifications" (UIL-001), "license" (UIL-018)
//   vùng vẽ: bar->setCursorPoint(x, y); tab layout: bar->setSpace(…); VN-004: bar->setFontIssues(…)

#include "core/base/settings/Settings.h"
#include "core/regen/numfmt/UnitContext.h"

#include <QPointF>
#include <QWidget>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

class QAction;

namespace trion::actions {
class ActionRegistry;
class Sysvars;
} // namespace trion::actions

namespace trion::edition {
class EditionGate;
} // namespace trion::edition

namespace trion::ui {
class InlineProgress;
class Menu;
class NotificationCenter;
class ProgressPresenter;
} // namespace trion::ui

namespace trion::shell {

class ModeToggles;
class StatusButton;

class StatusBar : public QWidget {
    Q_OBJECT
public:
    // Mọi tham chiếu sống lâu hơn thanh. Cần ThemeManager, IconProvider, RichTooltip.
    StatusBar(actions::ActionRegistry& actions, actions::Sysvars& sysvars, edition::EditionGate& gate,
              ui::ProgressPresenter& progress, ui::NotificationCenter& notes,
              settings::Store& store = settings::Store::instance(), QWidget* parent = nullptr);
    ~StatusBar() override;

    // ponytail: chỗ nối tạm. UIA-008 (vùng vẽ) gọi mỗi lần con trỏ di chuyển, tọa độ trong UCS hiện hành (UIA-113).
    void setCursorPoint(double x, double y);
    // ponytail: INT-012 (jig) đặt điểm gốc khi lệnh hỏi điểm có điểm gốc (COORDS 2), nullopt khi hết hỏi.
    void setBasePoint(std::optional<QPointF> base);
    // ponytail: DOC-010 điền từ biến header của bản vẽ đang mở (LUNITS, LUPREC, AUNITS, AUPREC, ANGBASE, ANGDIR…).
    void setUnits(const regen::UnitContext& units);

    enum class Space : std::uint8_t { Model, Paper, ModelInViewport };
    void setSpace(Space space); // ponytail: UIA-011 (tab layout, viewport kích hoạt) gọi; mặc định Model
    // ponytail: chỗ nối tạm. DOC-010 (CANNOSCALE của bản vẽ) đặt tỉ lệ chú thích, mặc định "1:1"; UIA-011 đặt tỉ lệ
    // viewport đang kích hoạt ("1:50"), rỗng khi không có. Chữ của bản vẽ, không dịch.
    void setAnnotationScale(const QString& scale);
    void setViewportScale(const QString& scale);

    struct GraphicsInfo {
        QString backend; // "D3D11", "Vulkan"…
        QString gpu;     // "Intel UHD 620"
        double fps = 0;  // khung hình hiện tại; ≤ 0: chưa có
    };
    // ponytail: RND-001 đặt; gọi mỗi lần rê chỉ báo đồ họa. Chưa đặt: tooltip chỉ có tên và "bấm để mở".
    void setGraphicsSource(std::function<GraphicsInfo()> source);
    // ponytail: VN-004 (vn::scanEncodings) đặt sau khi quét bản vẽ đang mở; 0, 0: ẩn chỉ báo.
    void setFontIssues(int substitutedFonts, int legacyObjects);

    // Chữ ô tọa độ: "x, y, z", "khoảng cách<góc, z" hoặc "Tọa độ tắt" (đã dịch); coords là giá trị COORDS.
    static QString coordText(QPointF point, std::optional<QPointF> base, int coords, const regen::UnitContext& units);

    QStringList overflowed() const; // id mục đang ở menu tràn, theo thứ tự trên thanh (test, QA)
    QString shownCoord() const;     // chữ ô tọa độ đang hiện (test, QA)

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    struct Item; // một mục của thanh, StatusBar.cpp

signals:
    // Chuỗi lệnh không đi qua QAction của registry: nút chế độ (ModeToggles), 'COORDS n, ^C^C_MSPACE (chỗ nối INT-002).
    void macroRequested(const QString& macro);
    // Màn hình chưa có lệnh: "notifications" (UI-L14), "license" (UI-L03), như TitleBar::screenRequested.
    void screenRequested(const QString& id);

protected:
    bool event(QEvent* e) override; // LayoutRequest: mục đổi cỡ, hiện/ẩn
    void resizeEvent(QResizeEvent* e) override;
    void paintEvent(QPaintEvent* e) override;
    void changeEvent(QEvent* e) override;
    bool eventFilter(QObject* watched, QEvent* e) override; // rê chỉ báo đồ họa: đọc thông tin mới

private:
    class CoordButton;
    class TextButton;
    class TaskList;

    void relayout();                      // chọn mục hiện theo chỗ, đặt vị trí
    int required(bool all = false) const; // bề rộng cần cho các mục đang hiện (all: mọi mục muốn hiện)
    bool wanted(const Item& it) const;
    void applyProfile();
    void openTasks(QWidget* anchor);
    void toggleSpace();
    void syncScale();
    void setCoords(int value); // 'COORDS n
    void syncCoordMenu();
    void syncEdition();
    void syncGraphicsTip();
    void retranslate();

    actions::ActionRegistry& actions_;
    actions::Sysvars& sysvars_;
    edition::EditionGate& gate_;
    ui::ProgressPresenter& progress_;
    ui::NotificationCenter& notes_;
    settings::Store& store_;

    std::vector<Item> items_; // thứ tự trên thanh
    QStringList hidden_;      // khóa Tùy biến đang ẩn
    Space space_ = Space::Model;
    int fontSubstituted_ = 0, fontLegacy_ = 0;
    std::function<GraphicsInfo()> graphics_;

    std::array<int, 2> seps_{-1, -1}; // x của hai vạch .sb-sep quanh dải nút chế độ; -1: không vẽ

    CoordButton* coord_;
    TextButton* spaceButton_;
    ModeToggles* modes_;
    TextButton* scale_; // tỉ lệ chỉ xem (Viewer)
    QString annoScale_ = QStringLiteral("1:1"), vpScale_;
    QWidget* taskBox_;           // chứa task_: InlineProgress tự hiện/ẩn theo việc, thanh hiện/ẩn hộp
    ui::InlineProgress* task_;   // tác vụ chính
    TextButton* font_;
    StatusButton* gpu_;
    StatusButton* tasks_;
    StatusButton* bell_;
    TextButton* edition_;
    StatusButton* clean_;
    StatusButton* overflow_;
    StatusButton* custom_;
    ui::Menu* coordMenu_;
    ui::Menu* overflowMenu_;
    ui::Menu* customMenu_;
    QAction* spaceAction_;   // MODEL/PAPER ở menu tràn
    QAction* tasksAction_;   // Tác vụ nền ở menu tràn
    QAction* notesAction_;   // Thông báo ở menu tràn
    QAction* licenseAction_; // Trạng thái giấy phép ở menu tràn
    TaskList* taskList_ = nullptr;
};

} // namespace trion::shell
