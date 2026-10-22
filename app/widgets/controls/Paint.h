#pragma once
// Hàm vẽ dùng chung của điều khiển (UI0-106): thành phần tự vẽ khác (QAT, ribbon, thanh trạng thái…) dùng lại để viền
// focus, khung, mũi tên giống hệt nút của Controls.h. Màu, bán kính đọc token lúc gọi.
#include <QColor>
#include <QPixmap>
#include <QPointF>
#include <QRectF>

class QPainter;
class QWidget;

namespace trion::ui::paint {

// Focus từ bàn phím (:focus-visible); nhấp chuột không vẽ viền focus.
bool focusVisible(const QWidget* w);
// Khung .btn/.dd/.seg: nền rồi viền 1 px, bo --r; màu không hợp lệ thì bỏ phần đó.
void drawBox(QPainter& p, const QRectF& r, const QColor& fill, const QColor& border);
// Viền 2 px nằm trong mép (outline 2 px, offset -1 px), bo --r: focus --c-focus, lỗi --c-danger.
void drawRing(QPainter& p, const QRectF& r, const QColor& c);
// Mũi tên xuống tự vẽ (.ic.xs, 9 px) căn giữa tại c.
void drawChevron(QPainter& p, QPointF c, const QColor& color);
// Biểu tượng một màu tô bằng màu c (currentColor của prototype), giữ alpha.
QPixmap tinted(QPixmap pm, const QColor& c);

} // namespace trion::ui::paint
