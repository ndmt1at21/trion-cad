#pragma once
// UIJ-001 · Chỉ báo Bác sĩ Font trên thanh trạng thái (07.1 §10 "Chỉ báo Bác sĩ Font"; prototype js/shell.js
// renderStatus #sb-font; css/app.css .sb-chip.warn, .sb-task):
//   có vấn đề   [⚠ Font: 3 thay thế]    .sb-chip.warn: nền --c-warn-soft, chữ và biểu tượng warn --c-warn, chữ --fs-s;
//                                       chỉ có chữ mã cũ: "Font: 250 chữ mã cũ". Rê: "Bác sĩ Font", "3 font đã thay
//                                       thế · 250 đối tượng chữ mã cũ. Bấm để mở Bác sĩ Font." (RichTooltip, FIXFONT)
//   đang quét   [Đang quét font ▬▬▭▭]   như .sb-task: chữ --fs-xs --c-text-2, thanh 80×4 (--c-active, phần xong
//                                       --c-accent); chưa biết tiến trình: chỉ chữ
//   không vấn đề, chưa quét: ẩn (không chip)
// Cao --h-btn; rê: viền trong --c-border-ctl (chip), --c-hover (đang quét); focus bàn phím: viền --c-focus. Bấm, Enter,
// Space: clicked (FontDoctorUi mở palette UI-J02). Nhận focus bằng Tab, không bằng cú nhấp. Tự hiện/ẩn theo nội dung;
// cỡ cố định theo chữ (sizeHint = minimumSizeHint) nên không bị nén, không cắt chữ. Trình đọc màn hình: tên là chữ
// trên chip, mô tả là số liệu hoặc phần trăm đã quét.
//
// Gắn vào StatusBar (UIA-009) ở chỗ mục "fontDoctor" (sau tác vụ chính, trước chỉ báo đồ họa, như #sb-font): thanh hiện
// nhận widget ngoài chưa được; đề nghị trong báo cáo UIJ-001 (thay TextButton font_ và setFontIssues bằng chip này,
// "muốn hiện" = hasContent()). Cần ThemeManager, IconProvider, RichTooltip. Rảnh không timer. Chỉ luồng UI.

#include <QToolButton>

#include <cstdint>

namespace trion::fontdoctor {

class FontStatusChip final : public QToolButton {
    Q_OBJECT
public:
    explicit FontStatusChip(QWidget* parent = nullptr);

    void setIssues(int missingFonts, qint64 tcvn3, qint64 vni); // không vấn đề (0, 0, 0): như clear()
    void setScanning(double progress);                          // 0…1; < 0: chưa biết tiến trình
    void clear();

    bool hasContent() const { return state_ != State::Empty; }
    bool isScanning() const { return state_ == State::Scanning; }
    double progress() const { return progress_; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override; // Enter như Space
    void changeEvent(QEvent* e) override;      // LanguageChange

private:
    enum class State : std::uint8_t { Empty, Scanning, Issues };
    void retranslate();
    int pad() const;

    State state_ = State::Empty;
    int missing_ = 0;
    qint64 tcvn3_ = 0;
    qint64 vni_ = 0;
    double progress_ = -1;
};

} // namespace trion::fontdoctor
