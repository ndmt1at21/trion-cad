#pragma once
// UIJ-001 · Banner Bác sĩ Font trên vùng vẽ (UI-J01; 07.3 §1, 07.1 §12 "Banner trên vùng vẽ"; prototype js/palettes.js
// FD.onOpen, js/shell.js renderBanners; css/app.css .banner):
//
//   [fontdoc] Bác sĩ Font: Bản vẽ dùng 3 font không có trên máy và 250 đối tượng chữ mã TCVN3, 46 chữ VNI. TrionCAD đã
//             tự hiển thị đúng.    [Xem chi tiết] [🔒 Chuyển sang Unicode][Viewer Pro] [Không nhắc lại với tệp này] [×]
//
// - Widget riêng, không qua ui::CanvasBanner (UI0-009): CanvasBanner (1) giữ chữ một dòng và rút gọn "…", ở 1366 px với
//   vi-XL câu này bị cắt mà UIJ-001 cần xuống dòng; (2) MessageAction chưa có huy hiệu phiên bản cho nút. Cùng dáng
//   .banner: nền --c-banner, biểu tượng fontdoc màu --c-accent, viền đáy --c-border, chữ --fs-s, nút .btn.sm
//   (Chuyển sang Unicode là .btn.primary, Không nhắc lại là .btn.ghost); lề, khoảng cách như CanvasBanner. Con của vùng
//   vẽ, phủ mép trên, rộng bằng vùng vẽ; stackBelow(CanvasBanner) đặt ngay dưới banner chung nên hai lớp không chồng
//   nhau.
// - "Bác sĩ Font:" đậm đứng đầu (<b> của prototype), câu theo sau, xuống dòng thì thụt thẳng sau tiêu đề. Hai QLabel
//   chữ thường: rich text (QTextDocument) tốn ~1 ms mỗi lần đo cao theo bề rộng, quá ngân sách 2 ms mỗi lần mở bản vẽ.
// - Không cắt chữ: câu xuống dòng; nút cùng hàng khi câu còn chỗ cho ≤ 2 dòng, hẹp hơn thì nút xuống hàng dưới (và
//   xuống tiếp khi vẫn thiếu chỗ). Cao theo nội dung, tối thiểu kMinHeight như .banner.
// - Không chặn thao tác: hiện lên không lấy focus, không kích hoạt cửa sổ; chỉ phủ dải trên của vùng vẽ. Nút nhận focus
//   bằng Tab, không bằng cú nhấp (vùng vẽ, dòng lệnh giữ phím gõ). Esc khi focus trong banner = Đóng. Ẩn khi đang giữ
//   focus: focus về chỗ trước khi Tab vào banner.
// - Chuyển sang Unicode: EditionGate khóa Viewer Pro (bản Viewer) thì nút có ổ khóa, huy hiệu "Viewer Pro" (LockBadge),
//   mô tả tiếp cận "Có trong TrionCAD Viewer Pro"; bấm thì gate.upgrade().request (trang nâng cấp UI-L04), không phát
//   convertRequested. Đổi phiên bản lúc chạy: theo EditionGate::editionChanged.
// - Trình đọc màn hình: vai trò StatusBar (role="status"), tên là cả câu; hiện hoặc đổi câu thì thông báo Polite.
// Màu, cỡ đọc token mỗi lần vẽ, đo. Không timer. Chỉ luồng UI.
//
// FontDoctorUi (FontDoctorUi.h) quyết hiện/ẩn theo bản vẽ hiện hành và xử lý các tín hiệu; banner không tự ẩn.

#include <QPointer>
#include <QString>
#include <QWidget>

class QLabel;

namespace trion::edition {
class EditionGate;
class LockBadge;
} // namespace trion::edition

namespace trion::ui {
class Button;
class Glyph;
} // namespace trion::ui

namespace trion::fontdoctor {

class FontDoctorBanner final : public QWidget {
    Q_OBJECT
public:
    static constexpr int kMinHeight = 38; // .banner min-height

    // Cần ThemeManager, IconProvider; gate sống lâu hơn banner. Ẩn tới lần showScan đầu.
    FontDoctorBanner(QWidget* canvas, edition::EditionGate& gate);

    // Hiện, hoặc đổi câu theo số liệu quét (có vấn đề: missingFonts > 0 hoặc tcvn3 + vni > 0). canMute: bản vẽ có đường
    // dẫn (nút "Không nhắc lại với tệp này"). Đang hiện cùng số liệu: không làm gì (không thông báo lại).
    void showScan(int missingFonts, qint64 tcvn3, qint64 vni, bool canMute);
    QString message() const { return message_; } // câu đang hiện, không kèm "Bác sĩ Font:" (test, QA)

    // Câu của banner theo mẫu 07.3 §1 UI-J01, số theo i18n::formatCount (1.250).
    static QString compose(int missingFonts, qint64 tcvn3, qint64 vni);

    // Đặt banner ngay dưới other (ui::CanvasBanner, cùng cha) khi other đang hiện; nullptr: sát mép trên vùng vẽ.
    void stackBelow(QWidget* other);

    ui::Button* detailsButton() const { return details_; }
    ui::Button* convertButton() const { return convert_; }
    ui::Button* muteButton() const { return mute_; }
    ui::Button* closeButton() const { return close_; }
    edition::LockBadge* convertBadge() const { return badge_; }

    void setVisible(bool visible) override;

signals:
    void detailsRequested();
    void convertRequested(); // chỉ khi không khóa; khóa thì banner tự mở trang nâng cấp
    void muteRequested();
    void closeRequested(); // × hoặc Esc

protected:
    bool event(QEvent* e) override; // LayoutRequest (nút đổi cỡ), ShortcutOverride của Esc
    bool eventFilter(QObject* watched, QEvent* e) override; // vùng vẽ, banner bên trên đổi cỡ, hiện/ẩn
    void changeEvent(QEvent* e) override;                   // LanguageChange
    void keyPressEvent(QKeyEvent* e) override;              // Esc
    void paintEvent(QPaintEvent* e) override;

private:
    int arrange(int width); // đặt chỗ các con theo bề rộng, trả chiều cao cần
    void place();           // theo vùng vẽ và banner bên trên
    void retranslate();
    void syncLock();
    void applyTheme();

    edition::EditionGate& gate_;
    ui::Glyph* glyph_;
    QLabel* title_; // "Bác sĩ Font:" đậm
    QLabel* text_;  // câu, xuống dòng
    ui::Button* details_;
    ui::Button* convert_;
    edition::LockBadge* badge_;
    ui::Button* mute_;
    ui::Button* close_;
    QPointer<QWidget> above_; // stackBelow
    QPointer<QWidget> back_;  // focus trước khi Tab vào banner
    int missing_ = -1;        // số liệu đang hiện; -1: chưa có
    qint64 tcvn3_ = 0;
    qint64 vni_ = 0;
    QString message_;
};

} // namespace trion::fontdoctor
