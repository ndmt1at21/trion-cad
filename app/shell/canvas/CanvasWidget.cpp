#include "app/shell/canvas/CanvasWidget.h"

#include "app/core/actions/ActionRegistry.h"
#include "app/core/ime/ImeBypass.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/controls/Paint.h"
#include "app/widgets/message/Messages.h"

#include <QAccessible>
#include <QAccessibleWidget>
#include <QApplication>
#include <QInputMethod>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QStyleHints>
#include <QtMath>

#include <algorithm>
#include <utility>

using namespace Qt::StringLiterals;

namespace trion::shell {

namespace {

// Biến vùng vẽ đọc: cùng tên, cùng miền giá trị như AutoCAD.
const QString kCursorSize = u"CURSORSIZE"_s;             // 1–100: % cạnh dài vùng vẽ
const QString kPickbox = u"PICKBOX"_s;                   // 0–50: nửa cạnh ô chọn, pixel logic
const QString kAperture = u"APERTURE"_s;                 // 1–50: nửa cạnh ô bắt điểm
const QString kSelectionPreview = u"SELECTIONPREVIEW"_s; // bit 1: khi lệnh hỏi chọn; bit 2: khi không có lệnh
const QString kPickDrag = u"PICKDRAG"_s;                 // 0 hai cú nhấp, 1 nhấn-kéo, 2 cả hai
const QString kPickAuto = u"PICKAUTO"_s;                 // bit 4: nhấn-kéo lasso (bit 1, 2 thuộc INT-155)
const QString kCursorBadge = u"CURSORBADGE"_s;           // 1 tắt, 2 bật
const QString kRolloverTips = u"ROLLOVERTIPS"_s;         // 0 tắt, 1 bật

constexpr int kLassoAuto = 4;    // bit PICKAUTO cho lasso
constexpr int kLassoStepPx = 2;  // lấy mẫu đường lasso như nét kéo của prototype (cad.js stroke: > 2 px)
constexpr int kBusyTurnMs = 800; // vòng bận quay như .spin (0,8 s)
constexpr int kRingInset = 3;    // viền focus 2 px trong mép (paint::drawRing) và khử răng cưa

// Vai trò Canvas cho trình đọc màn hình (QWidget thường chỉ là Client).
QAccessibleInterface* accessibleCanvas(const QString&, QObject* object)
{
    auto* w = qobject_cast<CanvasWidget*>(object);
    return w ? new QAccessibleWidget(w, QAccessible::Canvas) : nullptr;
}

QPoint at(const QMouseEvent* e)
{
    return {qFloor(e->position().x()), qFloor(e->position().y())};
}

// Phím gõ chuyển cho đích: phím soạn và chữ mà ô nhập của đích tự nhận (như QInputControl::isAcceptableInput của
// QLineEdit: chữ in được trừ Ctrl, Ctrl+Shift; AltGr = Ctrl+Alt vẫn là chữ).
bool routed(const QKeyEvent& k)
{
    const Qt::KeyboardModifiers mods = k.modifiers();
    switch (k.key()) {
    case Qt::Key_Backspace:
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Escape:
    case Qt::Key_Up:
    case Qt::Key_Down:
        return !(mods & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
    default:
        break;
    }
    const QString text = k.text();
    if (text.isEmpty())
        return false;
    const QChar c = text.front();
    if (c.category() == QChar::Other_Format) // ZWNJ, ZWJ… (Windows gõ bằng Ctrl+Shift)
        return true;
    if (mods == Qt::ControlModifier || mods == (Qt::ShiftModifier | Qt::ControlModifier))
        return false;
    return c.isPrint() || c.category() == QChar::Other_PrivateUse || (c.isHighSurrogate() && text.size() > 1);
}

QPolygonF rectArea(QPoint a, QPoint b)
{
    return QPolygonF{QPointF(a), QPointF(b.x(), a.y()), QPointF(b), QPointF(a.x(), b.y())};
}

std::span<const quint64> one(const std::optional<quint64>& id)
{
    return id ? std::span<const quint64>(&*id, 1) : std::span<const quint64>();
}

} // namespace

CanvasWidget::CanvasWidget(actions::Sysvars& sysvars, QWidget* parent)
    : QWidget(parent), sysvars_(sysvars), tipDelay_(this), busy_(this)
{
    static const bool installed = (QAccessible::installFactory(accessibleCanvas), true);
    Q_UNUSED(installed);
    setObjectName(u"canvas"_s);
    setAttribute(Qt::WA_OpaquePaintEvent); // nền, cảnh, con trỏ phủ hết vùng bẩn
    setAttribute(Qt::WA_InputMethodEnabled);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAccessibleName(tr("Vùng vẽ"));

    tipDelay_.setSingleShot(true);
    connect(&tipDelay_, &QTimer::timeout, this, &CanvasWidget::showTip);
    busy_.setStartValue(0.0);
    busy_.setEndValue(360.0);
    busy_.setDuration(kBusyTurnMs);
    busy_.setLoopCount(-1);
    connect(&busy_, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        Overlay next = ov_;
        next.busyTurn = v.toDouble();
        setOverlay(next);
    });
    connect(&sysvars_, &actions::Sysvars::changed, this, [this](const QString& name) { sysvarChanged(name); });
    connect(&theme::ThemeManager::instance(), &theme::ThemeManager::changed, this, [this] {
        applyCursor(); // con trỏ zoom tô theo màu
        syncBusy();    // giảm chuyển động, Giao diện nhẹ
        update();
    });
    ov_ = sized(ov_);
    applyCursor();
}

CanvasWidget::~CanvasWidget()
{
    if (hooked_)
        ui::ime::setHookTarget(nullptr);
}

void CanvasWidget::initSysvars(actions::Sysvars& sysvars)
{
    const std::pair<const QString&, int> defaults[] = {
        {kCursorSize, 5}, {kPickbox, 3},    {kAperture, 10},   {kSelectionPreview, 3},
        {kPickDrag, 2},   {kPickAuto, 5},   {kCursorBadge, 2}, {kRolloverTips, 1},
    };
    for (const auto& [name, value] : defaults)
        if (sysvars.get(name) == 0)
            sysvars.set(name, value);
}

int CanvasWidget::var(const QString& name) const
{
    return sysvars_.get(name);
}

bool CanvasWidget::selectable() const
{
    return ov_.cursor == CursorState::Idle || ov_.cursor == CursorState::PickObject || ov_.cursor == CursorState::Busy;
}

bool CanvasWidget::previewOn() const
{
    return var(kSelectionPreview) & (ov_.cursor == CursorState::PickObject ? 1 : 2);
}

Overlay CanvasWidget::sized(Overlay o) const
{
    const int percent = std::clamp(var(kCursorSize), 1, 100);
    o.crossHalf = (percent * std::max(width(), height()) + 99) / 100; // 100: nửa chữ thập phủ hết vùng vẽ từ mọi điểm
    o.pickHalf = std::clamp(var(kPickbox), 0, 50);
    o.apertureHalf = aperture_ ? std::clamp(var(kAperture), 1, 50) : 0;
    o.badge = var(kCursorBadge) == 1 ? QString() : badge_;
    return o;
}

// ------------------------------------------------------------------------------------------------ trạng thái, vẽ

void CanvasWidget::setOverlay(const Overlay& next)
{
    QRegion dirty;
    if (!ov_.sameCursor(next))
        dirty = cursorRegion(ov_) | cursorRegion(next);
    if (!ov_.sameShape(next))
        dirty |= shapeRegion(ov_) | shapeRegion(next);
    ov_ = next;
    if (!dirty.isEmpty())
        update(dirty);
}

void CanvasWidget::paintEvent(QPaintEvent* e)
{
    QPainter p(this);
    const QColor background = canvasColor(tok::c_canvas);
    for (const QRect& r : e->region())
        p.fillRect(r, background);
    if (scene_) {
        p.save();
        scene_(p, e->region(), Highlight{hover_, preview_});
        p.restore();
    }
    paintOverlay(p, ov_);
    if (ui::paint::focusVisible(this)) {
        p.setRenderHint(QPainter::Antialiasing);
        ui::paint::drawRing(p, rect(), theme::ThemeManager::instance().color(tok::c_focus));
    }
}

void CanvasWidget::applyCursor()
{
    switch (ov_.cursor) {
    case CursorState::Pan:
        setCursor(panning_ ? Qt::ClosedHandCursor : Qt::OpenHandCursor); // prototype: cursor grab / grabbing
        break;
    case CursorState::Zoom: { // Qt không có kính lúp: biểu tượng "zoom" của bộ, điểm nóng ở tâm kính (7, 7 trên lưới 16)
        // ponytail: kính lúp có dấu +; dạng +/− riêng khi INT-016 có zoom thời gian thực (UIA-008).
        const int size = qRound(theme::ThemeManager::instance().px(tok::ic_lg));
        const QPixmap icon = theme::IconProvider::instance().pixmap(u"zoom"_s, size, devicePixelRatio(),
                                                                   theme::IconState::Normal);
        setCursor(QCursor(ui::paint::tinted(icon, canvasColor(tok::c_cross)), size * 7 / 16, size * 7 / 16));
        break;
    }
    default:
        setCursor(Qt::BlankCursor); // vùng vẽ tự vẽ chữ thập, ô chọn
        break;
    }
}

void CanvasWidget::syncBusy()
{
    const bool run = ov_.cursor == CursorState::Busy && ov_.mouse && isVisible() &&
                     theme::ThemeManager::instance().ms(tok::t) > 0;
    if (!run)
        busy_.stop();
    else if (busy_.state() != QAbstractAnimation::Running)
        busy_.start();
}

void CanvasWidget::setCursorState(CursorState state)
{
    if (state == ov_.cursor)
        return;
    panning_ = false;
    Overlay next = ov_;
    next.cursor = state;
    setOverlay(next);
    applyCursor();
    if (!selectable() && gesture_ != Gesture::None && gesture_ != Gesture::Polygon)
        endGesture();
    refreshHover();
    syncBusy();
}

void CanvasWidget::setApertureVisible(bool on)
{
    aperture_ = on;
    setOverlay(sized(ov_));
}

void CanvasWidget::setCursorBadge(const QString& icon)
{
    badge_ = icon;
    setOverlay(sized(ov_));
}

void CanvasWidget::sysvarChanged(const QString& name)
{
    if (name == kCursorSize || name == kPickbox || name == kAperture || name == kCursorBadge) {
        setOverlay(sized(ov_));
    } else if (name == kSelectionPreview) {
        refreshHover();
        refreshPreview();
    } else if (name == kRolloverTips && var(kRolloverTips) == 0) {
        tipDelay_.stop();
        if (tip_)
            tip_->hide();
    }
}

// ------------------------------------------------------------------------------------------------ chuột

void CanvasWidget::mousePressEvent(QMouseEvent* e)
{
    const QPoint p = at(e);
    if (ov_.mouse != p) { // nhấn ở chỗ chưa có sự kiện di chuột (bút, cảm ứng, sự kiện tổng hợp): con trỏ tới đó trước
        Overlay next = ov_;
        next.mouse = p;
        last_ = p;
        setOverlay(next);
    }
    if (e->button() != Qt::LeftButton) {
        if (selecting() && gesture_ != Gesture::Polygon) // chuột phải, giữa hủy khung đang kéo (prototype contextmenu)
            cancelSelection();
        else
            e->ignore(); // điều hướng (UIA-008), menu ngữ cảnh
        return;
    }
    if (gesture_ == Gesture::Polygon) {
        points_.append(QPointF(p));
        refreshShape();
        return;
    }
    if (gesture_ == Gesture::ClickBox) {
        swallowRelease_ = true;
        finishBox(p, e->modifiers());
        return;
    }
    switch (ov_.cursor) {
    case CursorState::PickPoint: // lệnh chờ điểm: lấy điểm lúc nhấn như prototype (cad.down)
        emit picked(QPointF(p), e->modifiers());
        return;
    case CursorState::Pan: // kéo pan do UIA-008 (Navigation) làm; ở đây chỉ đổi bàn tay mở → nắm
        panning_ = true;
        applyCursor();
        return;
    case CursorState::Zoom:
        e->ignore();
        return;
    default:
        break;
    }
    gesture_ = Gesture::Press;
    anchor_ = p;
    last_ = p;
    pressTime_ = e->timestamp();
}

void CanvasWidget::mouseMoveEvent(QMouseEvent* e)
{
    const QPoint p = at(e);
    if (ov_.mouse == p)
        return;
    last_ = p;
    bool shape = gesture_ == Gesture::Box || gesture_ == Gesture::ClickBox || gesture_ == Gesture::Polygon;
    if (gesture_ == Gesture::Press && (p - anchor_).manhattanLength() >= QApplication::startDragDistance()) {
        // Nhấn giữ rồi mới kéo là lasso; kéo ngay là khung (PICKDRAG 0: khung chờ cú nhấp sau, không xong khi nhả).
        const auto hold = quint64(QGuiApplication::styleHints()->mousePressAndHoldInterval());
        if ((var(kPickAuto) & kLassoAuto) && e->timestamp() >= pressTime_ + hold) {
            gesture_ = Gesture::Lasso;
            points_ = QPolygonF{QPointF(anchor_), QPointF(p)};
            polygonMode_ = p.x() >= anchor_.x() ? SelectMode::WPolygon : SelectMode::CPolygon; // sang phải: Window
        } else {
            gesture_ = var(kPickDrag) == 0 ? Gesture::ClickBox : Gesture::Box;
        }
        setHover(std::nullopt);
        shape = true;
    } else if (gesture_ == Gesture::Lasso && (p - points_.constLast().toPoint()).manhattanLength() >= kLassoStepPx) {
        points_.append(QPointF(p));
        shape = true;
    }
    const bool entered = !ov_.mouse;
    Overlay next = ov_;
    next.mouse = p;
    if (shape) {
        next.shape = area(next.mouse);
        next.mode = mode();
    }
    setOverlay(next);
    if (shape)
        refreshPreview();
    else
        refreshHover();
    if (entered)
        syncBusy();
}

void CanvasWidget::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) {
        e->ignore();
        return;
    }
    if (panning_) {
        panning_ = false;
        applyCursor();
        return;
    }
    if (std::exchange(swallowRelease_, false))
        return;
    switch (gesture_) {
    case Gesture::Press:
        click(e->modifiers());
        break;
    case Gesture::Box:
        finishBox(at(e), e->modifiers());
        break;
    case Gesture::Lasso:
        finishLasso(e->modifiers());
        break;
    default: // ClickBox: chờ cú nhấp sau; Polygon: đỉnh thêm lúc nhấn
        break;
    }
}

void CanvasWidget::leaveEvent(QEvent* e)
{
    Overlay next = ov_;
    next.mouse.reset();
    next.shape = area(next.mouse); // đa giác của lệnh: bỏ đỉnh theo con trỏ
    setOverlay(next);
    setHover(std::nullopt);
    syncBusy();
    QWidget::leaveEvent(e);
}

// ------------------------------------------------------------------------------------------------ chọn

SelectMode CanvasWidget::mode() const
{
    if (gesture_ == Gesture::Box || gesture_ == Gesture::ClickBox)
        return last_.x() < anchor_.x() ? SelectMode::Crossing : SelectMode::Window; // phải → trái: Crossing
    return polygonMode_;
}

QPolygonF CanvasWidget::area(const std::optional<QPoint>& mouse) const
{
    switch (gesture_) {
    case Gesture::Box:
    case Gesture::ClickBox:
        return rectArea(anchor_, last_);
    case Gesture::Lasso:
        return points_;
    case Gesture::Polygon: {
        QPolygonF a = points_;
        if (!a.isEmpty() && mouse)
            a.append(QPointF(*mouse));
        return a;
    }
    default:
        return {};
    }
}

void CanvasWidget::refreshShape()
{
    Overlay next = ov_;
    next.shape = area(ov_.mouse);
    next.mode = mode();
    setOverlay(next);
    refreshPreview();
}

void CanvasWidget::click(Qt::KeyboardModifiers modifiers)
{
    // Nhấp chỗ trống mở khung hai cú nhấp (prototype: cad.click không trúng → clickPick; AutoCAD PICKDRAG 0, 2).
    const bool empty = !hitTest_ || !hitTest_(QPointF(anchor_), ov_.pickHalf);
    if (empty && var(kPickDrag) != 1) {
        gesture_ = Gesture::ClickBox;
        setHover(std::nullopt);
        refreshShape();
        return;
    }
    gesture_ = Gesture::None;
    emit picked(QPointF(anchor_), modifiers);
}

void CanvasWidget::finishBox(QPoint at, Qt::KeyboardModifiers modifiers)
{
    last_ = at;
    const QPolygonF a = area(ov_.mouse);
    const SelectMode m = mode();
    const bool tiny = (at - anchor_).manhattanLength() < QApplication::startDragDistance();
    endGesture();
    if (!tiny) // hai cú nhấp trùng chỗ: không chọn gì (prototype boxDone: < 3 px)
        emit boxSelected(a, m, modifiers);
}

void CanvasWidget::finishLasso(Qt::KeyboardModifiers modifiers)
{
    const QPolygonF a = points_;
    const SelectMode m = polygonMode_;
    endGesture();
    if (a.size() >= (m == SelectMode::Fence ? 2 : 3))
        emit boxSelected(a, m, modifiers);
}

void CanvasWidget::endGesture()
{
    gesture_ = Gesture::None;
    points_.clear();
    Overlay next = ov_;
    next.shape.clear();
    setOverlay(next);
    if (!preview_.empty()) {
        update(highlightRegion(preview_));
        preview_.clear();
    }
    refreshHover();
}

void CanvasWidget::beginPolygon(SelectMode mode)
{
    endGesture();
    gesture_ = Gesture::Polygon;
    polygonMode_ = mode;
    setHover(std::nullopt);
    refreshShape();
}

bool CanvasWidget::finishPolygon()
{
    if (gesture_ != Gesture::Polygon)
        return false;
    const QPolygonF a = points_;
    const SelectMode m = polygonMode_;
    endGesture();
    if (a.size() < (m == SelectMode::Fence ? 2 : 3))
        return false;
    emit boxSelected(a, m, QGuiApplication::keyboardModifiers());
    return true;
}

void CanvasWidget::cancelSelection()
{
    swallowRelease_ = false;
    if (gesture_ != Gesture::None)
        endGesture();
}

// ------------------------------------------------------------------------------------------------ tô sáng

QRegion CanvasWidget::highlightRegion(std::span<const quint64> ids) const
{
    if (ids.empty())
        return {};
    if (!bounds_)
        return rect();
    QRectF all;
    for (const quint64 id : ids)
        all |= bounds_(id);
    return QRegion(all.toAlignedRect().adjusted(-1, -1, 1, 1)) & rect();
}

void CanvasWidget::refreshHover()
{
    std::optional<quint64> id;
    if (ov_.mouse && (gesture_ == Gesture::None || gesture_ == Gesture::Press) && selectable() && previewOn() &&
        hitTest_)
        id = hitTest_(QPointF(*ov_.mouse), ov_.pickHalf);
    setHover(id);
}

void CanvasWidget::setHover(std::optional<quint64> id)
{
    if (id == hover_)
        return;
    update(highlightRegion(one(hover_)) | highlightRegion(one(id)));
    hover_ = id;
    tipDelay_.stop();
    if (tip_)
        tip_->hide();
    if (hover_ && hoverTip_ && var(kRolloverTips) != 0)
        tipDelay_.start(style()->styleHint(QStyle::SH_ToolTip_WakeUpDelay, nullptr, this));
    emit hovered(id);
}

void CanvasWidget::refreshPreview()
{
    std::vector<quint64> ids;
    const QPolygonF a = area(ov_.mouse);
    if (selecting() && a.size() >= 2 && previewOn() && areaQuery_)
        ids = areaQuery_(a, mode());
    if (ids == preview_)
        return;
    update(highlightRegion(preview_) | highlightRegion(ids));
    preview_ = std::move(ids);
}

void CanvasWidget::showTip()
{
    if (!hover_ || !hoverTip_ || !ov_.mouse || var(kRolloverTips) == 0)
        return;
    const QString text = hoverTip_(*hover_);
    if (text.isEmpty())
        return;
    if (!tip_)
        tip_ = new ui::CursorTip(this);
    tip_->showAt(*ov_.mouse, text, ui::Severity::Info);
}

void CanvasWidget::setScenePainter(ScenePainter painter)
{
    scene_ = std::move(painter);
    update();
}

void CanvasWidget::setHitTest(HitTest hitTest)
{
    hitTest_ = std::move(hitTest);
    refreshHover();
}

void CanvasWidget::setAreaQuery(AreaQuery query)
{
    areaQuery_ = std::move(query);
    refreshPreview();
}

void CanvasWidget::setBounds(Bounds bounds)
{
    bounds_ = std::move(bounds);
}

void CanvasWidget::setHoverTip(HoverTip tip)
{
    hoverTip_ = std::move(tip);
}

void CanvasWidget::updateScene(const QRegion& region)
{
    update(region.isEmpty() ? QRegion(rect()) : region);
    refreshHover();
    refreshPreview();
}

// ------------------------------------------------------------------------------------------------ phím, bộ gõ

bool CanvasWidget::owns(const QKeyEvent& k) const
{
    if (k.modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))
        return false;
    if (k.key() == Qt::Key_Escape) // hủy khung đang kéo (lần Esc sau mới tới dòng lệnh: hủy lệnh, bỏ vùng chọn)
        return gesture_ == Gesture::Box || gesture_ == Gesture::ClickBox || gesture_ == Gesture::Lasso;
    return k.key() == Qt::Key_Space && gesture_ == Gesture::Lasso; // luân phiên Window / Crossing / Fence
}

bool CanvasWidget::event(QEvent* e)
{
    if (e->type() == QEvent::ShortcutOverride && owns(*static_cast<QKeyEvent*>(e))) {
        e->accept();
        return true;
    }
    return QWidget::event(e);
}

void CanvasWidget::keyPressEvent(QKeyEvent* e)
{
    if (owns(*e)) {
        if (e->key() == Qt::Key_Escape) {
            cancelSelection();
        } else {
            polygonMode_ = polygonMode_ == SelectMode::WPolygon   ? SelectMode::CPolygon
                           : polygonMode_ == SelectMode::CPolygon ? SelectMode::Fence
                                                                  : SelectMode::WPolygon;
            refreshShape();
        }
        return;
    }
    if (sink_ && routed(*e)) {
        // Cùng sự kiện tới ô của đích (qua event filter của nó, như dòng lệnh: Enter, Space, Esc, ↑↓) như khi ô có focus.
        QCoreApplication::sendEvent(sink_, e);
        e->accept(); // đích không nhận thì Qt đã đưa lên cha của đích; không đưa lần hai lên cha của vùng vẽ
        syncInputMethod();
        return;
    }
    QWidget::keyPressEvent(e);
}

void CanvasWidget::inputMethodEvent(QInputMethodEvent* e)
{
    if (!sink_) {
        e->ignore();
        return;
    }
    QCoreApplication::sendEvent(sink_, e);
    syncInputMethod();
}

QVariant CanvasWidget::inputMethodQuery(Qt::InputMethodQuery query) const
{
    if (!sink_)
        return query == Qt::ImEnabled ? QVariant(false) : QWidget::inputMethodQuery(query);
    QVariant v = sink_->inputMethodQuery(query);
    if (query == Qt::ImEnabled) // như QWidget::event: widget không trả ImEnabled thì theo thuộc tính
        return sink_->isEnabled() && (v.isValid() ? v.toBool() : sink_->testAttribute(Qt::WA_InputMethodEnabled));
    // Ô soạn chữ của bộ gõ đặt theo con trỏ chữ của đích: tọa độ của đích → tọa độ vùng vẽ (focus object).
    const int type = v.typeId();
    if (type == QMetaType::QRect || type == QMetaType::QRectF) {
        const QRectF r = v.toRectF().translated(mapFromGlobal(sink_->mapToGlobal(QPointF(0, 0))));
        return type == QMetaType::QRect ? QVariant(r.toAlignedRect()) : QVariant(r);
    }
    return v;
}

// Bộ gõ hỏi Qt::ImEnabled của focus object (vùng vẽ) khi focus đổi; đích đổi trạng thái (CommandInput bỏ qua bộ gõ khi
// chờ tên lệnh) sau phím vừa chuyển thì báo lại. ponytail: đích đổi trạng thái ngoài luồng phím (lệnh mới từ ribbon)
// được cập nhật ở phím kế tiếp; CommandInput::updateBypass gọi QInputMethod::update cả khi không có focus thì hết trễ.
void CanvasWidget::syncInputMethod()
{
    const bool focused = hasFocus();
    const bool on = focused && inputMethodQuery(Qt::ImEnabled).toBool();
    if (focused && on != imEnabled_) {
        if (!on)
            QGuiApplication::inputMethod()->commit();
        QGuiApplication::inputMethod()->update(Qt::ImEnabled);
    }
    imEnabled_ = on;
    // Bộ gõ kiểu hook của Windows (Unikey…): đích bỏ qua bộ gõ thì vùng vẽ (đang nhận phím) lọc như CommandInput.
    const bool hook = focused && sink_ && !on;
    if (hook != hooked_) {
        hooked_ = hook;
        ui::ime::setHookTarget(hook ? this : nullptr);
    }
}

void CanvasWidget::setKeySink(QWidget* sink)
{
    sink_ = sink;
    syncInputMethod();
}

// Không gọi QWidget::focusInEvent/focusOutEvent: chúng vẽ lại cả widget; chỉ vẽ lại dải viền focus.
void CanvasWidget::focusInEvent(QFocusEvent*)
{
    syncInputMethod();
    update(QRegion(rect()) - QRegion(rect().adjusted(kRingInset, kRingInset, -kRingInset, -kRingInset)));
}

void CanvasWidget::focusOutEvent(QFocusEvent*)
{
    syncInputMethod();
    update(QRegion(rect()) - QRegion(rect().adjusted(kRingInset, kRingInset, -kRingInset, -kRingInset)));
}

// ------------------------------------------------------------------------------------------------ vòng đời

void CanvasWidget::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    setOverlay(sized(ov_)); // CURSORSIZE theo cạnh dài
}

void CanvasWidget::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);
    syncBusy();
}

void CanvasWidget::hideEvent(QHideEvent* e)
{
    tipDelay_.stop();
    if (tip_)
        tip_->hide();
    if (gesture_ != Gesture::Polygon) // khung, lasso đang kéo không sống qua lúc ẩn (đa giác thuộc lệnh: lệnh hủy)
        cancelSelection();
    ov_.mouse.reset(); // hiện lại: chữ thập từ lần di chuột đầu
    setHover(std::nullopt);
    syncBusy();
    QWidget::hideEvent(e);
}

void CanvasWidget::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::LanguageChange)
        setAccessibleName(tr("Vùng vẽ"));
    QWidget::changeEvent(e);
}

} // namespace trion::shell
