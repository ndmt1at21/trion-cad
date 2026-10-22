#pragma once
// Huy hiệu phiên bản .ed (07.1 §2; prototype js/shell.js edBadge; css/app.css .ed, .ed[data-ed], .ed.warn): "Pro",
// "Viewer", "Dùng thử: còn 12 ngày"…; lâu chưa xác minh giấy phép (EditionGate::offlineDays ≥ 0, trừ Viewer) thêm
// " · còn 3 ngày offline" và màu cảnh báo .ed.warn. Có chữ, không chỉ là màu. Theo EditionGate: đổi ngay khi phiên
// bản, ngày dùng thử, ngày offline đổi; dịch lại khi đổi ngôn ngữ. Chữ --fs-xs đậm, lề ngang --sp-3, cao chữ +
// 2 × --sp-1 (17 px của .ed), bo --r. Màu, cỡ đọc token mỗi lần đo, vẽ.
// Widget không tương tác: thanh tiêu đề, trang Trạng thái giấy phép (UI-L03). Nơi tự vẽ (nút huy hiệu của thanh trạng
// thái, bấm mở trang giấy phép) gọi label()/sizeFor()/paint().
//
// Dùng (sau ThemeManager, EditionGate):
//   row->addWidget(new edition::EditionBadge(gate));
//   EditionBadge::paint(p, QRectF(origin, EditionBadge::sizeFor(gate)), gate); // nút tự vẽ; tên tiếp cận: label(gate)

#include <QLabel>

namespace trion::edition {

class EditionGate;

class EditionBadge : public QLabel {
    Q_OBJECT
public:
    // Cần ThemeManager; gate sống lâu hơn huy hiệu.
    explicit EditionBadge(EditionGate& gate, QWidget* parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    static QString label(const EditionGate& gate); // "Dùng thử: còn 12 ngày", "Pro · còn 3 ngày offline" (đã dịch)
    static QSize sizeFor(const EditionGate& gate);
    static void paint(QPainter& p, const QRectF& r, const EditionGate& gate); // r cỡ sizeFor()

protected:
    void paintEvent(QPaintEvent* e) override;
    void changeEvent(QEvent* e) override;

private:
    void sync();

    EditionGate& gate_;
};

} // namespace trion::edition
