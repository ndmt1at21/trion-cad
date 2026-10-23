#pragma once
// UIA-008 · Điều hướng vùng vẽ (07.1 §9.2; prototype js/canvas.js View.prototype.zoomAt, panBy, zoomExtents, pushHist;
// js/shell.js bindCanvas): chuột, touchpad trên CanvasWidget → lời gọi điều hướng qua chỗ nối INT-016 (ViewControl).
//
// Navigation là event filter trên vùng vẽ; phần chọn, phím, con trỏ của CanvasWidget giữ nguyên.
//  - Lăn: zoom quanh con trỏ. Một nấc (120 = 15°) nhân 1 + ZOOMFACTOR/300 (ZOOMFACTOR 3–100; mặc định 60 → 1,2 như
//    prototype), phần nấc lẻ của chuột lăn mịn tính theo lũy thừa; ZOOMWHEEL 1 đảo chiều (lăn tới thu nhỏ).
//  - Chuột giữa (MBUTTONPAN 1): giữ kéo pan, con trỏ bàn tay nắm (trạng thái Pan của vùng vẽ), nhả thì trả trạng thái
//    cũ (lệnh đang chạy tiếp tục); nhấp đúp zoom toàn bộ. MBUTTONPAN 0: nút giữa để nguyên cho vùng vẽ.
//  - Trạng thái Pan (PAN thời gian thực): kéo trái pan. Trạng thái Zoom (ZOOM thời gian thực): kéo trái theo chiều dọc
//    zoom quanh giữa vùng vẽ, kéo lên phóng to, nửa chiều cao vùng vẽ là ×2 (AutoCAD: từ giữa lên mép trên 100%).
//  - Touchpad (P1): chụm (QNativeGestureEvent Zoom, nhân 1 + value) zoom quanh điểm giữa hai ngón; Pan native theo
//    delta; lăn có pixelDelta mà có pha cuộn (macOS, Wayland) hoặc từ thiết bị TouchPad (X11) là kéo hai ngón → pan
//    theo pixelDelta. Ctrl + lăn luôn zoom (Windows đổi chụm của touchpad thành Ctrl + lăn); chuột lăn của macOS có
//    pixelDelta nhưng không có pha → zoom.
//  - Bước view (INT-172; 07.1 §9.2 "gộp thành một bước hoàn tác, tùy chọn"): UNDOCTL bit 16 (Combine, mặc định bật)
//    → mọi zoom, pan liên tiếp là một viewStep, phát khi chuỗi dừng: nhấn nút chuột khác (chọn, lệnh), phím trên vùng
//    vẽ, chuột rời vùng vẽ, vùng vẽ ẩn, flush(). Bit tắt → mỗi thao tác một viewStep: một lần lăn, một lần kéo (nhấn
//    đến nhả), một cử chỉ touchpad (Begin đến End). Thao tác không đổi view (nhấp giữa không kéo) không tạo bước.
// ponytail: công thức tốc độ 1 + ZOOMFACTOR/300 là tạm, khớp prototype ở mặc định (INT-170 chốt theo spec); touchpad
// của Windows gửi lăn như chuột (không pixelDelta) nên kéo hai ngón là zoom, như AutoCAD; cuộn ngang của chuột, xoay và
// smart zoom của touchpad bỏ qua; Ctrl/Shift + chuột giữa (joystick pan, 3DORBIT) chưa có. viewStep là tín hiệu, chưa
// ghi undo: INT-172 (DOC-008) thay.
//
// TempView: phép biến đổi view tạm cho tới INT-016/INT-170: tâm view (tọa độ bản vẽ) ở giữa vùng vẽ, tỉ lệ pixel logic
// trên đơn vị bản vẽ, trục Y bản vẽ hướng lên; zoom quanh điểm giữ điểm bản vẽ dưới con trỏ đứng yên (tính lại tâm từ
// điểm đó sau khi đổi tỉ lệ, sai số làm tròn không cộng dồn). Đổi view thì vẽ lại cảnh (CanvasWidget::updateScene).
// ponytail: chưa có twist, hướng 3D, lịch sử view (ZOOM Previous), extents gia tăng (setExtents là chỗ nối), giới hạn
// tỉ lệ theo độ chính xác, chuyển cảnh mượt (VTENABLE), viewport: INT-170 ViewEngine, INT-171 thay.
//
// Lắp vào cửa sổ chính (integration), luồng UI, sau CanvasWidget và CanvasWidget::initSysvars (đặt mặc định ZOOMFACTOR,
// MBUTTONPAN, UNDOCTL):
//   auto* view = new TempView(*canvas, canvas);                  // ponytail: INT-016 (ViewEngine) thay
//   auto* nav = new Navigation(*canvas, sysvars, *view, canvas);
//   canvas->setScenePainter([view](QPainter& p, const QRegion&, const CanvasWidget::Highlight& h) {
//       p.setTransform(view->transform()); …                      // cảnh vẽ theo view (RND-001 thay)
//   });
//   // nav->flush() trước mỗi lệnh tới router (lệnh từ ribbon, dòng lệnh: bước view ghi trước lệnh; LoadGate.h)
//   connect(nav, &Navigation::viewStep, …);                       // ponytail: INT-172 ghi bước view

#include "app/shell/canvas/CanvasOverlay.h"

#include <QObject>
#include <QPointF>
#include <QRectF>
#include <QTransform>

#include <cstdint>
#include <optional>

class QMouseEvent;
class QNativeGestureEvent;
class QWheelEvent;

namespace trion::actions {
class Sysvars;
}

namespace trion::shell {

class CanvasWidget;

// Chỗ nối INT-016: điều hướng view của viewport hiện hành; tọa độ là pixel logic của vùng vẽ. ponytail: TempView là bản
// tạm; INT-016 thay bằng bộ chuyển gọi INT-170 (ViewEngine::zoomAbout, panBy, zoom Extents; sự kiện đổi view có nguồn).
class ViewControl {
public:
    virtual ~ViewControl() = default;
    virtual void zoomAbout(QPointF pos, double factor) = 0; // factor > 1 phóng to; điểm bản vẽ dưới pos đứng yên
    virtual void panBy(QPointF delta) = 0; // cảnh dời theo delta (kéo sang phải: cảnh sang phải)
    virtual void zoomExtents() = 0;        // zoom toàn bộ đối tượng
};

class TempView final : public QObject, public ViewControl {
    Q_OBJECT
public:
    static constexpr double kMinScale = 1e-9; // pixel trên đơn vị bản vẽ: chặn tràn số, không phải giới hạn của lệnh
    static constexpr double kMaxScale = 1e9;
    static constexpr double kExtentsPad = 1.06; // khoảng hở quanh extents (prototype zoomExtents)

    // canvas sống lâu hơn view (cha nên là canvas): đổi view thì vẽ lại cảnh của nó.
    explicit TempView(CanvasWidget& canvas, QObject* parent = nullptr);

    QPointF center() const { return center_; }
    double scale() const { return scale_; }
    void setView(QPointF center, double scale); // tỉ lệ kẹp vào [kMinScale, kMaxScale]
    // Hộp bao bản vẽ cho zoomExtents (tọa độ bản vẽ); rộng và cao đều 0 (QRectF(), bản vẽ một điểm): chưa có.
    // ponytail: INT-151/INT-170 (extents gia tăng) thay.
    void setExtents(const QRectF& extents) { extents_ = extents; }
    QRectF extents() const { return extents_; }

    QPointF toScreen(QPointF world) const; // bản vẽ → pixel logic của vùng vẽ
    QPointF toWorld(QPointF screen) const;
    QTransform transform() const; // như toScreen, cho QPainter của chỗ nối vẽ cảnh

    void zoomAbout(QPointF pos, double factor) override;
    void panBy(QPointF delta) override;
    void zoomExtents() override; // chưa có extents: không đổi

signals:
    void changed(); // view đổi (sau khi đã yêu cầu vẽ lại)

private:
    QPointF mid() const; // giữa vùng vẽ, pixel logic
    void apply(QPointF center, double scale);

    CanvasWidget& canvas_;
    QPointF center_;
    double scale_ = 1;
    QRectF extents_;
};

class Navigation final : public QObject {
    Q_OBJECT
public:
    // canvas, sysvars, view sống lâu hơn Navigation (cha nên là canvas). Cài event filter lên canvas.
    Navigation(CanvasWidget& canvas, actions::Sysvars& sysvars, ViewControl& view, QObject* parent = nullptr);

    // Kết thúc chuỗi zoom, pan đang gộp: phát viewStep nếu view đã đổi. Router gọi trước mỗi lệnh.
    void flush();
    bool dragging() const { return drag_ != Drag::None; } // đang kéo pan, zoom

signals:
    void viewStep(); // một bước view cho hoàn tác (INT-172)

protected:
    bool eventFilter(QObject* watched, QEvent* e) override;

private:
    enum class Drag : std::uint8_t { None, Pan, Zoom };

    int var(const QString& name) const;
    void wheel(const QWheelEvent& e);
    bool gesture(const QNativeGestureEvent& e);
    bool press(const QMouseEvent& e);
    bool doubleClick(const QMouseEvent& e);
    void move(const QMouseEvent& e);
    void endDrag();
    void changed() { pending_ = true; }
    void opEnd(); // một thao tác xong: không gộp thì phát bước ngay

    CanvasWidget& canvas_;
    actions::Sysvars& sysvars_;
    ViewControl& view_;
    Drag drag_ = Drag::None;
    Qt::MouseButton dragButton_ = Qt::NoButton;
    QPointF last_;                       // vị trí chuột lần trước khi kéo
    std::optional<CursorState> restore_; // trạng thái con trỏ trả lại khi nhả chuột giữa
    bool scrolling_ = false;             // touchpad giữa ScrollBegin và ScrollEnd
    bool native_ = false;                // cử chỉ native giữa Begin và End
    bool pending_ = false;               // view đã đổi, chưa phát viewStep
};

} // namespace trion::shell
