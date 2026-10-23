#include "app/shell/canvas/Navigation.h"

#include "app/core/actions/ActionRegistry.h"
#include "app/shell/canvas/CanvasWidget.h"

#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QPointingDevice>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;

namespace trion::shell {

namespace {

const QString kZoomFactor = u"ZOOMFACTOR"_s; // 3–100: tốc độ zoom của lăn
const QString kZoomWheel = u"ZOOMWHEEL"_s;   // 0: lăn tới phóng to; 1: đảo chiều
const QString kMButtonPan = u"MBUTTONPAN"_s; // 1: giữ chuột giữa pan, nhấp đúp zoom toàn bộ; 0: theo CUI
const QString kUndoCtl = u"UNDOCTL"_s;       // bit 16: gộp zoom, pan liên tiếp thành một bước hoàn tác

constexpr int kCombineBit = 16;
constexpr double kNotch = QWheelEvent::DefaultDeltasPerStep; // 120 phần tám độ = một nấc
constexpr double kMinPinch = 0.01; // hệ số chụm nhỏ nhất của một sự kiện (value ≤ -1 là dữ liệu hỏng)

// Kéo hai ngón trên touchpad (không phải chuột lăn): có pixelDelta và có pha cuộn (macOS, Wayland) hoặc thiết bị là
// TouchPad (X11). Ctrl + lăn là chụm của Windows: zoom.
bool twoFingerScroll(const QWheelEvent& e)
{
    if ((e.modifiers() & Qt::ControlModifier) || e.pixelDelta().isNull())
        return false;
    const QPointingDevice* device = e.pointingDevice();
    return e.phase() != Qt::NoScrollPhase || (device && device->type() == QInputDevice::DeviceType::TouchPad);
}

} // namespace

// ------------------------------------------------------------------------------------------------ TempView

TempView::TempView(CanvasWidget& canvas, QObject* parent) : QObject(parent), canvas_(canvas) {}

QPointF TempView::mid() const
{
    return {canvas_.width() / 2.0, canvas_.height() / 2.0};
}

QPointF TempView::toScreen(QPointF world) const
{
    const QPointF m = mid();
    return {(world.x() - center_.x()) * scale_ + m.x(), (center_.y() - world.y()) * scale_ + m.y()};
}

QPointF TempView::toWorld(QPointF screen) const
{
    const QPointF m = mid();
    return {center_.x() + (screen.x() - m.x()) / scale_, center_.y() - (screen.y() - m.y()) / scale_};
}

QTransform TempView::transform() const
{
    const QPointF m = mid();
    return {scale_, 0, 0, -scale_, m.x() - center_.x() * scale_, m.y() + center_.y() * scale_};
}

void TempView::apply(QPointF center, double scale)
{
    // So đúng từng bit: QPointF::operator== so gần đúng (1e-12 tương đối) nên ở tọa độ ≈ 10⁹ bỏ mất pan nhỏ.
    if (center.x() == center_.x() && center.y() == center_.y() && scale == scale_)
        return;
    center_ = center;
    scale_ = scale;
    canvas_.updateScene();
    emit changed();
}

void TempView::setView(QPointF center, double scale)
{
    apply(center, std::clamp(scale, kMinScale, kMaxScale));
}

void TempView::zoomAbout(QPointF pos, double factor)
{
    if (!(factor > 0) || !std::isfinite(factor))
        return;
    // Điểm bản vẽ dưới pos trước khi đổi tỉ lệ; tâm mới tính từ nó để pos vẫn chỉ đúng điểm ấy (prototype zoomAt).
    const QPointF w = toWorld(pos), m = mid();
    const double s = std::clamp(scale_ * factor, kMinScale, kMaxScale);
    apply({w.x() - (pos.x() - m.x()) / s, w.y() + (pos.y() - m.y()) / s}, s);
}

void TempView::panBy(QPointF delta)
{
    apply({center_.x() - delta.x() / scale_, center_.y() + delta.y() / scale_}, scale_);
}

void TempView::zoomExtents()
{
    if (extents_.isNull()) // rộng, cao đều 0: chưa có extents (bản vẽ rỗng)
        return;
    const QRectF e = extents_.normalized();
    const double w = e.width() * kExtentsPad, h = e.height() * kExtentsPad; // một đoạn ngang, dọc: một cạnh 0
    const double s = std::min(w > 0 ? canvas_.width() / w : kMaxScale, h > 0 ? canvas_.height() / h : kMaxScale);
    apply(e.center(), std::clamp(s, kMinScale, kMaxScale));
}

// ------------------------------------------------------------------------------------------------ Navigation

Navigation::Navigation(CanvasWidget& canvas, actions::Sysvars& sysvars, ViewControl& view, QObject* parent)
    : QObject(parent), canvas_(canvas), sysvars_(sysvars), view_(view)
{
    canvas_.installEventFilter(this); // Qt tự gỡ filter khi Navigation bị hủy
}

int Navigation::var(const QString& name) const
{
    return sysvars_.get(name);
}

bool Navigation::eventFilter(QObject* watched, QEvent* e)
{
    if (watched != &canvas_)
        return false;
    switch (e->type()) {
    case QEvent::Wheel:
        wheel(*static_cast<QWheelEvent*>(e));
        return true;
    case QEvent::NativeGesture:
        return gesture(*static_cast<QNativeGestureEvent*>(e));
    case QEvent::MouseButtonPress:
        return press(*static_cast<QMouseEvent*>(e));
    case QEvent::MouseButtonDblClick:
        return doubleClick(*static_cast<QMouseEvent*>(e));
    case QEvent::MouseMove:
        move(*static_cast<QMouseEvent*>(e));
        return false; // vùng vẽ vẫn theo chuột: nhả ra thì chữ thập đúng chỗ
    case QEvent::MouseButtonRelease:
        if (drag_ != Drag::None && static_cast<QMouseEvent*>(e)->button() == dragButton_)
            endDrag();
        return false; // vùng vẽ đổi bàn tay nắm → mở (trạng thái Pan)
    // Mọi phím trên vùng vẽ (kể cả phím tắt), chuột rời vùng vẽ: chuỗi điều hướng xong.
    case QEvent::ShortcutOverride:
    case QEvent::KeyPress:
    case QEvent::Leave:
        flush();
        return false;
    case QEvent::Hide:
        endDrag();
        flush();
        return false;
    default:
        return false;
    }
}

void Navigation::wheel(const QWheelEvent& e)
{
    if (e.phase() == Qt::ScrollBegin)
        scrolling_ = true;
    if (twoFingerScroll(e)) {
        view_.panBy(QPointF(e.pixelDelta()));
        changed();
    } else if (const int delta = e.angleDelta().y()) { // cuộn ngang của chuột: bỏ
        const double base = 1.0 + std::clamp(var(kZoomFactor), 3, 100) / 300.0;
        const double steps = (var(kZoomWheel) == 1 ? -delta : delta) / kNotch;
        view_.zoomAbout(e.position(), std::pow(base, steps));
        changed();
    }
    if (e.phase() == Qt::ScrollEnd)
        scrolling_ = false;
    if (!scrolling_)
        opEnd();
}

bool Navigation::gesture(const QNativeGestureEvent& e)
{
    switch (e.gestureType()) {
    case Qt::BeginNativeGesture:
        native_ = true;
        return true;
    case Qt::EndNativeGesture:
        native_ = false;
        opEnd();
        return true;
    case Qt::ZoomNativeGesture:
        if (e.value() != 0) {
            view_.zoomAbout(e.position(), std::max(1.0 + e.value(), kMinPinch));
            changed();
        }
        break;
    case Qt::PanNativeGesture:
        if (!e.delta().isNull()) {
            view_.panBy(e.delta());
            changed();
        }
        break;
    default:
        return false; // xoay, vuốt, smart zoom: chưa dùng
    }
    if (!native_)
        opEnd();
    return true;
}

bool Navigation::press(const QMouseEvent& e)
{
    const CursorState state = canvas_.cursorState();
    if (e.button() == Qt::MiddleButton && var(kMButtonPan) == 1) {
        endDrag();
        drag_ = Drag::Pan;
        dragButton_ = Qt::MiddleButton;
        last_ = e.position();
        restore_ = state;
        // Bàn tay (khung đang kéo bị bỏ như UIA-109; đa giác của lệnh giữ); vùng vẽ nhận cú nhấn → bàn tay nắm.
        canvas_.setCursorState(CursorState::Pan);
        return false;
    }
    if (e.button() == Qt::LeftButton && drag_ == Drag::None &&
        (state == CursorState::Pan || state == CursorState::Zoom)) {
        drag_ = state == CursorState::Pan ? Drag::Pan : Drag::Zoom;
        dragButton_ = Qt::LeftButton;
        last_ = e.position();
        return false; // Pan: vùng vẽ đổi bàn tay mở → nắm; Zoom: vùng vẽ bỏ qua
    }
    if (drag_ == Drag::None)
        flush(); // nút khác (chọn, menu, điểm của lệnh): bước view ghi trước việc nó gây ra
    return false;
}

bool Navigation::doubleClick(const QMouseEvent& e)
{
    if (e.button() != Qt::MiddleButton || var(kMButtonPan) != 1)
        return false;
    // Qt gửi cú nhấn thứ hai (đã bắt đầu kéo) rồi mới tới DblClick: bỏ lần kéo đó, nhả chuột không pan thêm.
    endDrag();
    view_.zoomExtents();
    changed();
    opEnd();
    return true;
}

void Navigation::move(const QMouseEvent& e)
{
    if (drag_ == Drag::None)
        return;
    if (!(e.buttons() & dragButton_)) { // nhả ở nơi không gửi release (cửa sổ khác giành chuột)
        endDrag();
        return;
    }
    const QPointF p = e.position(), d = p - last_;
    last_ = p;
    if (drag_ == Drag::Pan && !d.isNull()) {
        view_.panBy(d);
        changed();
    } else if (drag_ == Drag::Zoom && d.y() != 0 && canvas_.height() > 0) {
        const QPointF mid(canvas_.width() / 2.0, canvas_.height() / 2.0);
        view_.zoomAbout(mid, std::exp2(-d.y() / mid.y()));
        changed();
    }
}

void Navigation::endDrag()
{
    if (drag_ == Drag::None)
        return;
    drag_ = Drag::None;
    dragButton_ = Qt::NoButton;
    // Lệnh đổi trạng thái con trỏ trong lúc kéo thì giữ trạng thái của lệnh.
    if (const auto state = std::exchange(restore_, std::nullopt); state && canvas_.cursorState() == CursorState::Pan)
        canvas_.setCursorState(*state);
    opEnd();
}

void Navigation::opEnd()
{
    if (!(var(kUndoCtl) & kCombineBit))
        flush();
}

void Navigation::flush()
{
    if (std::exchange(pending_, false))
        emit viewStep();
}

} // namespace trion::shell
