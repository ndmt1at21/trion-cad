#pragma once
// UIA-109 · Widget vùng vẽ UI-A07 (07.1 §9.1 con trỏ, §9.3 chọn, §9.4 tô sáng; prototype js/shell.js bindCanvas,
// bindKeys; js/cad.js cad.move, cad.down, cad.up, cad.boxDone).
//
// Phần nhận sự kiện của vùng vẽ; phần vẽ ở CanvasOverlay.h. Chỉ vẽ khi có thay đổi và chỉ vùng bẩn: chữ thập cũ + mới,
// khung chọn, đối tượng đổi tô sáng (theo chỗ nối setBounds). Rảnh: không vẽ, không timer.
//  - Con trỏ: 6 trạng thái (setCursorState), cỡ theo sysvar CURSORSIZE (% cạnh dài vùng vẽ: 100 là hết vùng vẽ),
//    PICKBOX, APERTURE (khi setApertureVisible), huy hiệu (setCursorBadge, CURSORBADGE 1 tắt). Con trỏ hệ thống ẩn khi
//    vùng vẽ tự vẽ con trỏ; chuột rời vùng vẽ thì không còn chữ thập.
//  - Phím (focus ở vùng vẽ): ký tự in được, Backspace, Enter, Space, Esc, ↑↓ và chuỗi của bộ gõ (QInputMethodEvent)
//    chuyển nguyên cho đích setKeySink (ô dòng lệnh, ô DYN của UIA-111); Qt::ImEnabled, vị trí ô soạn chữ hỏi đích.
//    Phím tắt (QAction của ActionRegistry: phạm vi Toàn cục trên cửa sổ, Vùng vẽ qua addViewport) chạy trước, không bị
//    nuốt: vùng vẽ chỉ giành ShortcutOverride cho Esc khi đang kéo khung và Space khi đang kéo lasso.
//  - Chọn, chuột trái ở trạng thái Chờ, Bận, Chọn đối tượng: nhấp → picked; kéo trái → phải Window, phải → trái
//    Crossing → boxSelected (PICKDRAG 0: hai cú nhấp, 1: nhấn-kéo, 2: cả hai; nhấp chỗ trống mở khung hai cú nhấp như
//    AutoCAD); nhấn giữ rồi kéo (quá QStyleHints::mousePressAndHoldInterval, PICKAUTO bit 4) là lasso, Space luân
//    phiên Window / Crossing / Fence; Esc, chuột phải hủy khung đang kéo. Chọn điểm: nhấn → picked. Lệnh hỏi F / WP /
//    CP: beginPolygon, mỗi cú nhấp thêm một đỉnh, finishPolygon (Enter của lệnh) → boxSelected.
//  - Tô sáng: rê ở trạng thái Chờ, Bận (SELECTIONPREVIEW bit 2) hoặc Chọn đối tượng (bit 1) → chỗ nối hit test →
//    hovered; khung đang kéo → chỗ nối truy vấn vùng → danh sách xem trước; cả hai đưa cho chỗ nối vẽ cảnh. Tooltip rê
//    (ROLLOVERTIPS) qua CursorTip sau SH_ToolTip_WakeUpDelay.
//
// Tọa độ của tín hiệu và chỗ nối: pixel logic của widget (lõi đổi sang tọa độ thế giới theo viewport).
// ponytail: chưa có RND-001, INT-008/009/010/016: cảnh, hit test, truy vấn vùng, vùng màn hình của đối tượng là chỗ nối
// std::function; kết quả chọn là tín hiệu, không tự chọn. INT-008 (chọn thật), INT-009 (hit test), INT-153 (vùng),
// RND-013 (cờ tô sáng) thay chỗ nối.
//
// Lắp vào cửa sổ chính (integration), luồng UI, sau ThemeManager, IconProvider:
//   CanvasWidget::initSysvars(sysvars);               // một lần, sau khi tạo Sysvars
//   auto* canvas = new CanvasWidget(sysvars);         // thay ShellAssembly::canvas trong ngăn xếp vùng Canvas
//   actions.addViewport(canvas);                      // phím phạm vi Vùng vẽ (Delete → ERASE, Ctrl+C…)
//   canvas->setKeySink(shell.commandLine->input());   // chữ gõ trên vùng vẽ vào dòng lệnh (UIA-111 đổi sang ô DYN)
//   connect(canvas, &CanvasWidget::picked, …);         // ponytail: INT-008 chọn thật; boxSelected, hovered tương tự

#include "app/shell/canvas/CanvasOverlay.h"

#include <QPointer>
#include <QPolygonF>
#include <QRegion>
#include <QTimer>
#include <QVariantAnimation>
#include <QWidget>

#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace trion::actions {
class Sysvars;
}
namespace trion::ui {
class CursorTip;
}

namespace trion::shell {

class CanvasWidget : public QWidget {
    Q_OBJECT
public:
    // Tô sáng đưa cho chỗ nối vẽ cảnh: đối tượng dưới con trỏ, đối tượng trong khung đang kéo (07.1 §9.4).
    struct Highlight {
        std::optional<quint64> hover;
        std::span<const quint64> preview;
    };
    // Vẽ cảnh trong vùng bẩn, sau nền --c-canvas, trước khung và con trỏ. ponytail: RND-001 thay.
    using ScenePainter = std::function<void(QPainter& p, const QRegion& dirty, const Highlight& highlight)>;
    // Đối tượng tại điểm (ô chọn nửa cạnh pickboxPx). ponytail: INT-009 HitTester::pick thay.
    using HitTest = std::function<std::optional<quint64>(QPointF pos, double pickboxPx)>;
    // Đối tượng mà vùng đang kéo sẽ chọn, cho xem trước. ponytail: INT-153 AreaQuery thay.
    using AreaQuery = std::function<std::vector<quint64>(const QPolygonF& area, SelectMode mode)>;
    // Vùng màn hình mà tô sáng của đối tượng chiếm (kể cả quầng): vẽ lại đúng chỗ khi tô sáng đổi. Không đặt thì tô
    // sáng đổi là vẽ lại cả widget. ponytail: RND-013 (cờ tô sáng trên GPU) thay.
    using Bounds = std::function<QRectF(quint64 id)>;
    // Chữ tooltip khi rê lên đối tượng (loại, layer, màu, linetype…); rỗng: không hiện. ponytail: INT-009/DOC thay.
    using HoverTip = std::function<QString(quint64 id)>;

    // sysvars sống lâu hơn widget. Cần ThemeManager, IconProvider.
    explicit CanvasWidget(actions::Sysvars& sysvars, QWidget* parent = nullptr);
    ~CanvasWidget() override;

    // ponytail: Sysvars tạm (INT-005) chưa có mặc định (biến chưa đặt đọc là 0): đặt mặc định AutoCAD cho các biến
    // vùng vẽ đọc mà đang là 0 (CURSORSIZE 5, PICKBOX 3, APERTURE 10, SELECTIONPREVIEW 3, PICKDRAG 2, PICKAUTO 5,
    // CURSORBADGE 2, ROLLOVERTIPS 1). INT-107 (bảng sysvar có mặc định) thay.
    static void initSysvars(actions::Sysvars& sysvars);

    // Con trỏ (07.1 §9.1). Đổi trạng thái hủy khung đang kéo nếu trạng thái mới không chọn được.
    void setCursorState(CursorState state);
    CursorState cursorState() const { return ov_.cursor; }
    void setApertureVisible(bool on); // INT-010 đang bắt điểm: ô APERTURE quanh chữ thập
    void setCursorBadge(const QString& icon); // biểu tượng của lệnh đang chạy (P1); rỗng: không có

    // Đích của phím gõ trên vùng vẽ; nullptr: không chuyển (phím lên widget cha).
    void setKeySink(QWidget* sink);
    QWidget* keySink() const { return sink_; }

    // Lệnh hỏi Fence, WPolygon, CPolygon: mỗi cú nhấp chuột trái thêm một đỉnh, đỉnh cuối theo con trỏ.
    void beginPolygon(SelectMode mode);
    // Enter của lệnh: đủ đỉnh (Fence ≥ 2, WPolygon/CPolygon ≥ 3) thì phát boxSelected; trả false nếu thiếu (vẫn kết thúc).
    bool finishPolygon();
    void cancelSelection(); // Esc của lệnh: bỏ khung, lasso, đa giác đang dựng
    bool selecting() const { return gesture_ != Gesture::None && gesture_ != Gesture::Press; }

    // Chỗ nối (luồng UI). Đặt lại: vẽ lại, tô sáng tính lại tại con trỏ.
    void setScenePainter(ScenePainter painter);
    void setHitTest(HitTest hitTest);
    void setAreaQuery(AreaQuery query);
    void setBounds(Bounds bounds);
    void setHoverTip(HoverTip tip);
    // Cảnh đổi (lõi báo): vẽ lại vùng (rỗng: cả widget), tô sáng tính lại tại con trỏ.
    void updateScene(const QRegion& region = {});

    // Bộ gõ hỏi vùng vẽ (focus object): trả lời của đích (ImEnabled, vị trí con trỏ chữ đổi sang tọa độ vùng vẽ).
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;

    std::optional<quint64> hoveredId() const { return hover_; }
    const std::vector<quint64>& previewIds() const { return preview_; }
    const Overlay& overlay() const { return ov_; } // trạng thái phần vẽ (RND-020, RND-012 đọc; test)

signals:
    // Nhấp chọn (Chờ, Chọn đối tượng) hoặc điểm (Chọn điểm), tọa độ widget; Shift + nhấp để bớt (INT-008, PICKADD).
    void picked(QPointF pos, Qt::KeyboardModifiers modifiers);
    // Vùng chọn xong: khung (4 đỉnh, đỉnh đầu là chỗ bắt đầu kéo), lasso, đa giác của lệnh.
    void boxSelected(const QPolygonF& area, trion::shell::SelectMode mode, Qt::KeyboardModifiers modifiers);
    void hovered(std::optional<quint64> id); // đối tượng dưới con trỏ đổi; nullopt: không còn

protected:
    bool event(QEvent* e) override;
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void leaveEvent(QEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void inputMethodEvent(QInputMethodEvent* e) override;
    void focusInEvent(QFocusEvent* e) override;
    void focusOutEvent(QFocusEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void changeEvent(QEvent* e) override;

private:
    enum class Gesture : std::uint8_t {
        None,
        Press,    // nút trái đang giữ, chưa quá ngưỡng kéo
        Box,      // nhấn-kéo khung, xong khi nhả
        ClickBox, // khung hai cú nhấp, xong ở cú nhấp sau
        Lasso,
        Polygon, // đa giác lệnh hỏi
    };

    int var(const QString& name) const;
    bool selectable() const; // trạng thái con trỏ cho chọn và tô sáng
    bool previewOn() const;  // SELECTIONPREVIEW cho trạng thái hiện tại
    bool owns(const QKeyEvent& k) const;
    void setOverlay(const Overlay& next);
    Overlay sized(Overlay o) const; // cỡ chữ thập, ô chọn, aperture, huy hiệu theo sysvar
    void endGesture();
    void finishBox(QPoint at, Qt::KeyboardModifiers modifiers);
    void finishLasso(Qt::KeyboardModifiers modifiers);
    void click(Qt::KeyboardModifiers modifiers);
    // Vùng đang dựng: khung, lasso, đa giác của lệnh (thêm đỉnh theo con trỏ khi con trỏ trong vùng vẽ).
    QPolygonF area(const std::optional<QPoint>& mouse) const;
    SelectMode mode() const;
    void refreshShape();
    void refreshPreview();
    void refreshHover();
    void setHover(std::optional<quint64> id);
    QRegion highlightRegion(std::span<const quint64> ids) const;
    void showTip();
    void applyCursor();
    void syncBusy();
    void syncInputMethod();
    void sysvarChanged(const QString& name);

    actions::Sysvars& sysvars_;
    Overlay ov_;
    QPointer<QWidget> sink_;
    ScenePainter scene_;
    HitTest hitTest_;
    AreaQuery areaQuery_;
    Bounds bounds_;
    HoverTip hoverTip_;

    Gesture gesture_ = Gesture::None;
    QPoint anchor_;         // chỗ nhấn, đỉnh đầu của khung
    QPoint last_;           // điểm cuối của khung: vị trí chuột gần nhất (giữ khi chuột ra ngoài vùng vẽ)
    quint64 pressTime_ = 0; // dấu thời gian sự kiện nhấn (ms)
    QPolygonF points_;      // lasso, đa giác
    SelectMode polygonMode_ = SelectMode::WPolygon;
    bool swallowRelease_ = false; // cú nhấn đã xong khung hai cú nhấp: bỏ lần nhả của nó
    bool panning_ = false;
    bool aperture_ = false;
    QString badge_;

    std::optional<quint64> hover_;
    std::vector<quint64> preview_;
    QTimer tipDelay_;
    ui::CursorTip* tip_ = nullptr; // tạo khi cần
    QVariantAnimation busy_;       // vòng bận quay khi đang hiện; giảm chuyển động thì đứng yên
    bool imEnabled_ = false;       // Qt::ImEnabled đã báo cho bộ gõ
    bool hooked_ = false;          // là ô của ime::setHookTarget (bộ gõ kiểu hook của Windows)
};

} // namespace trion::shell
