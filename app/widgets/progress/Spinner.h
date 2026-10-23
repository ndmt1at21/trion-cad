#pragma once
// Vòng quay đang chạy .spin (css/app.css: 14×14, viền 2 px --c-active, cung trên --c-accent, quay 0,8 s): tác vụ trên
// thanh trạng thái (InlineProgress kiểu gọn), thân "Đang tải…" của hộp thoại (DialogFrame::setLoading). Chỉ quay khi
// đang hiện; Giao diện nhẹ, giảm chuyển động (--t về 0) thì đứng yên.

#include <QVariantAnimation>
#include <QWidget>

namespace trion::ui {

class Spinner final : public QWidget {
    Q_OBJECT
public:
    explicit Spinner(QWidget* parent = nullptr); // cần ThemeManager

    bool spinning() const { return anim_.state() == QAbstractAnimation::Running; }

protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void paintEvent(QPaintEvent* e) override;

private:
    QVariantAnimation anim_{this}; // có cha: ThemeManager tìm thấy để dừng khi bật giảm chuyển động
};

} // namespace trion::ui
