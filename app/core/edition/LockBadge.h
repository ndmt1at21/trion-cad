#pragma once
// UI0-011 · Huy hiệu khóa theo phiên bản (07.1 §2; styleguide.html mục "Phiên bản và khóa tính năng"; css/app.css
// .ed.xs, .lkb). Có chữ, không chỉ là màu; trình đọc màn hình đọc "Có trong TrionCAD Pro" (EditionGate::availableIn).
//   Tag     .ed.xs  "Pro" | "Viewer Pro" (RichTooltip::paintBadge): mục menu và tooltip (Menu, RichTooltip tự vẽ theo
//                   TipInfo::need mà EditionGate::bind đặt), nút và hàng palette, chân hộp thoại ("Lưu thành… [Pro]")
//   Ribbon  .lkb    ổ khóa + "Pro" | "VP": nút ribbon (góc trên phải nút lớn, sau nhãn nút nhỏ; UIA-005)
// Widget tự vẽ (ribbon, delegate của hàng palette) gọi size()/paint() khi EditionGate::locked(need). Trong bố cục
// widget dùng LockBadge: hiện khi EditionGate::instance() khóa need, tự ẩn/hiện khi giấy phép đổi. Màu, cỡ đọc token
// mỗi lần đo, vẽ.
//
// Dùng (sau ThemeManager, EditionGate):
//   row->addWidget(new LockBadge(ui::Edition::Pro, LockBadge::Style::Tag, rowWidget));
//   if (gate.locked(need)) LockBadge::paint(p, badgeRect, need, LockBadge::Style::Ribbon); // nút ribbon tự vẽ
//   button->setLocked(true); button->setAccessibleDescription(EditionGate::availableIn(need)); // Controls
//
// ponytail: .lkb của prototype cao 13 px, chữ 9 px, lề 3 px; token gần nhất: chữ --fs-2xs (10 px) đậm như .ed.xs, lề
// và khoảng cách --sp-1, ổ khóa --sp-4 (8 px) nét 1 px theo hình src/lock.svg.

#include "app/widgets/tooltip/RichTooltip.h"

#include <QLabel>

#include <cstdint>

namespace trion::edition {

class LockBadge : public QLabel {
    Q_OBJECT
public:
    enum class Style : std::uint8_t { Tag, Ribbon };

    // Cần ThemeManager. Ẩn ngay nếu EditionGate::instance() không khóa need (hoặc chưa có cổng).
    LockBadge(ui::Edition need, Style style, QWidget* parent = nullptr);

    ui::Edition need() const { return need_; }
    Style style() const { return style_; }
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    static QString label(ui::Edition need, Style style); // chữ trên huy hiệu: "Pro", "Viewer Pro", "VP"
    static QSize size(ui::Edition need, Style style);    // {} nếu need là None
    static void paint(QPainter& p, const QRect& r, ui::Edition need, Style style); // r cỡ size()

protected:
    void paintEvent(QPaintEvent* e) override;
    void changeEvent(QEvent* e) override;

private:
    void sync(); // hiện/ẩn theo EditionGate
    void retranslate();

    ui::Edition need_;
    Style style_;
};

} // namespace trion::edition
