#pragma once
// UIC-017 · Phần tự vẽ của bộ chọn màu UI-C06 (prototype js/screens5.js SC.colorPicker; css .aci, .acs, .cmpbox,
// canvas #col-hs):
//   SwatchGrid    lưới ô màu: bảng ACI 24 × 10, hàng 1–9, hàng xám 250–255, hàng màu gần đây
//   HueSatArea    vùng sắc độ (ngang) × bão hòa (dọc) của tab True Color, nền theo độ sáng
//   ColorCompare  ô so sánh màu cũ | mới
// Màu, cỡ đọc token mỗi lần vẽ, đo. ByLayer, ByBlock vẽ bằng hình (sọc chéo, nửa chéo), không chỉ bằng màu.
// Mỗi lưới, vùng là một điểm dừng Tab; có focus từ bàn phím thì viền 2 px --c-focus quanh cả widget (lề kRing trong
// widget dành cho viền) và quanh ô con trỏ.

#include "app/dialogs/pickers/Color.h"

#include <QList>
#include <QWidget>

class QPainter;

namespace trion::ui {

// Ô màu c trong r: màu đặc; ByLayer sọc chéo 4 px --c-raised/--c-border; ByBlock nửa trên trái --c-text, nửa dưới
// phải --c-base (prototype sw()).
void paintSwatch(QPainter& p, const QRectF& r, const Color& c);

class SwatchGrid : public QWidget {
    Q_OBJECT
public:
    static constexpr int kRing = 3; // lề quanh lưới: viền focus 2 px + 1 px

    explicit SwatchGrid(QWidget* parent = nullptr);

    // Ô theo thứ tự đọc (hết hàng sang hàng), `columns` cột. Con trỏ về ô 0.
    void setColors(const QList<Color>& colors, int columns);
    // Cỡ ô (px logic) và khe giữa các ô. stretch: ô giãn theo bề ngang widget (bảng ACI: .aci 24 cột 1fr), cỡ là tối
    // thiểu.
    void setCellSize(QSize size, int gap, bool stretch = false);

    int count() const { return int(colors_.size()); }
    Color colorAt(int i) const { return colors_.value(i); }
    QRect cellRect(int i) const;  // tọa độ widget
    int cellAt(QPoint pos) const; // -1: ngoài ô

    // Ô con trỏ bàn phím (mũi tên, Home, End).
    int current() const { return current_; }
    void setCurrent(int i);
    // Ô có màu bằng `c` vẽ viền chọn 2 px --c-accent (.acs.on); không ô nào bằng thì không viền.
    Color selected() const { return selected_; }
    void setSelected(const Color& c);
    int hoverIndex() const { return hover_; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void hoverChanged(int index);   // chuột vào ô (-1: rời lưới)
    void currentChanged(int index); // con trỏ bàn phím đổi ô
    void picked(const Color& c);    // nhấp, mũi tên, Home/End, Space: chọn
    void activated(const Color& c); // nhấp đúp, Enter: chọn và xác nhận

protected:
    void paintEvent(QPaintEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void leaveEvent(QEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void focusInEvent(QFocusEvent* e) override;
    void focusOutEvent(QFocusEvent* e) override;

private:
    int rows() const;
    void move(int to); // con trỏ tới ô `to` (nếu có) và chọn
    void setHover(int i);

    QList<Color> colors_;
    int columns_ = 1;
    QSize cell_{16, 16};
    int gap_ = 1;
    bool stretch_ = false;
    int current_ = 0;
    int hover_ = -1;
    Color selected_;
};

class HueSatArea : public QWidget {
    Q_OBJECT
public:
    static constexpr int kRing = SwatchGrid::kRing;

    explicit HueSatArea(QWidget* parent = nullptr);

    // Dấu chọn theo h (trái → phải 0–360°) và s (trên 100 %, dưới 0 %); nền vẽ ở độ sáng l. Không phát tín hiệu.
    Hsl hsl() const { return hsl_; }
    void setHsl(const Hsl& c);
    QRect plane() const;      // vùng màu (trừ lề viền focus)
    QPoint markerPos() const; // tâm dấu chọn

    QSize sizeHint() const override; // 300 × 180 (prototype canvas) + lề viền

signals:
    // Người dùng nhấp, kéo, bấm mũi tên (←→ sắc độ 1°, ↑↓ bão hòa 1 %; Shift × 10). h nguyên 0–360, s bội của 0,01.
    void hueSatChanged(double hue, double saturation);

protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void focusInEvent(QFocusEvent* e) override;
    void focusOutEvent(QFocusEvent* e) override;

private:
    void pickAt(QPoint pos);
    void offer(double h, double s);

    Hsl hsl_;
};

class ColorCompare : public QWidget {
    Q_OBJECT
public:
    explicit ColorCompare(QWidget* parent = nullptr);
    void setColors(const Color& before, const Color& after);
    QRect beforeRect() const; // ô màu cũ (trái)
    QRect afterRect() const;  // ô màu mới (phải)
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent* e) override;

private:
    Color before_, after_;
};

} // namespace trion::ui
