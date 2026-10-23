#pragma once
// UI0-007 · Ô nhập số chuẩn UI-C09 (07.2 UI-C09; prototype TC.numField, numCommit, TC.pickValue trong js/ui.js;
// css/app.css .numf, .fmsg, .nfw). Dùng cho mọi trường số của palette và hộp thoại. Đọc và hiển thị số chỉ qua
// UI0-109 (parseValue, formatValue → INT-105, RGN-013), không có bộ đọc số thứ hai.
//
// Hình (.nfw): hộp .numf cao --h-ctl = [chữ Mono --fs-s][⌖ 22 px][↑↓ 16 px], dưới hộp dòng lỗi .fmsg (biểu tượng
// cảnh báo + câu --c-danger, xuống dòng khi dài, ẩn khi không lỗi). Focus: viền 2 px --c-focus; lỗi: viền 2 px
// --c-danger (lỗi thắng focus như SearchField); vô hiệu: mờ .55; khóa theo phiên bản: chỉ đọc, ổ khóa thay ⌖ và nút
// bước (không chỉ bằng màu). Màu, cỡ đọc token mỗi lần vẽ, đo.
//
// Hành vi:
//   hiển thị     formatValue theo spec(): 1250.00, 4'-1 1/2". setSpec (UNITS đổi) hiển thị lại ngay trong lượt gọi,
//                khung kế tiếp đã đúng; giá trị không đổi, không phát tín hiệu.
//   nhận         Enter, hoặc rời ô (trừ khi focus sang popup). Chữ chưa sửa: không đọc lại, không phát. Sai: viền lỗi,
//                câu dưới ô (errorText của UI0-109, "… phải lớn hơn …" của setLowerBound hoặc "… không được lớn hơn …"
//                của setUpperBound), trình đọc màn hình đọc câu đó; không phát; Enter sai không tới hộp thoại. Đúng:
//                định dạng lại, phát valueCommitted đúng 1 lần; Enter đi tiếp tới nút xác nhận của hộp thoại như ô
//                thường.
//   ↑ ↓          ± step() (Shift ×10), cả nút bước: từ chữ đang gõ nếu đọc được, không thì từ value(); nhận và phát
//                ngay như prototype.
//   Esc          đang sửa hoặc đang lỗi: trả chữ của giá trị trước khi sửa, bỏ lỗi. Không thì Esc tới hộp thoại.
//   *Khác nhau*  setMixed(): ô trống, hiện *Khác nhau* nghiêng --c-text-3. Gõ giá trị rồi nhận: phát 1 lần (bên gọi áp
//                cho mọi đối tượng). Rời ô hay Esc khi chưa gõ: vẫn *Khác nhau*; ↑ ↓ khi chưa gõ: không làm gì.
//   ⌖            ô Distance, Angle (setPickEnabled(false) để bỏ): ẩn DialogFrame chứa ô (hideForPick), phát
//                pickRequested; finishPick(v) hiện lại hộp thoại, nhận v như gõ (kiểm, phát 1 lần), focus về ô;
//                cancelPick() hiện lại, giá trị không đổi.
//   trợ năng     tên đọc "nhãn (đơn vị)", giá trị là chữ trong ô, mô tả là câu lỗi (hoặc *Khác nhau*); câu lỗi mới
//                gửi QAccessibleAnnouncementEvent (như aria-live="polite" của .fmsg). Focus vào ô chữ (focusProxy).
//
// Dùng (sau QApplication, loadFonts, ThemeManager, IconProvider như main):
//   auto* radius = new NumericField(ValueKind::Distance);
//   radius->setLabel(tr("Bán kính"));                         // tên đọc; câu lỗi "Bán kính phải lớn hơn 0."
//   radius->setSpec({ValueKind::Distance, units, insunits});  // UnitContext, INSUNITS của bản vẽ; UNITS đổi: gọi lại
//   radius->setLowerBound(0);
//   radius->setValue(250);                                    // nhiều đối tượng khác giá trị: setMixed()
//   connect(radius, &NumericField::valueCommitted, this, &RadiusForm::apply);
//
// ponytail: ⌖ nối tạm qua tín hiệu pickRequested + finishPick/cancelPick; INT-003 (prompt lấy khoảng cách 2 điểm hoặc
// góc trên vùng vẽ, Esc hủy) nối vào đây, giữ ô bằng QPointer. Chưa ai nối pickRequested thì ⌖ không làm gì.
// ponytail: chặn dưới ngặt (setLowerBound, như data-min của prototype) và chặn trên gồm cả cận (setUpperBound); kiểm
// khác (theo ô khác) thêm khi màn hình cần. Gõ ',' chỉ gợi ý trong câu lỗi lúc nhận, chưa có toast nhắc ngay khi gõ như
// prototype (cần UIA toast).

#include "app/widgets/numeric/ValueParser.h"

#include <QString>
#include <QWidget>

#include <optional>

class QAbstractButton;
class QLabel;

namespace trion::ui {

class NumericField : public QWidget {
    Q_OBJECT
public:
    explicit NumericField(ValueKind kind = ValueKind::Distance, QWidget* parent = nullptr);
    ~NumericField() override;

    // Loại ô và đơn vị của bản vẽ (UNITS, INSUNITS). Đổi: hiển thị lại value() ngay, bỏ lỗi, không phát tín hiệu.
    const ValueSpec& spec() const { return spec_; }
    void setSpec(const ValueSpec& spec);

    // Nhãn của trường (như nhãn hàng của form): tên đọc của ô, chủ ngữ của câu "… phải lớn hơn …".
    QString label() const { return label_; }
    void setLabel(const QString& label);

    // Giá trị đã nhận: Distance theo đơn vị bản vẽ, Angle radian, còn lại là số. setValue: bỏ *Khác nhau* và lỗi,
    // không phát tín hiệu (giá trị từ bản vẽ, không phải người dùng sửa).
    double value() const { return value_; }
    void setValue(double value);
    // *Khác nhau*: nhiều đối tượng khác giá trị. value() không có nghĩa đến khi có giá trị mới.
    bool isMixed() const { return mixed_; }
    void setMixed();

    // Bước của ↑ ↓ theo đơn vị của value(); 0 (mặc định): 1, ô góc 1°.
    double step() const { return step_; }
    void setStep(double step);
    // Giá trị phải lớn hơn bound (prototype data-min); nullopt: không chặn.
    std::optional<double> lowerBound() const { return lowerBound_; }
    void setLowerBound(std::optional<double> bound);
    // Giá trị không được lớn hơn bound (gồm cả cận: kênh màu 0–255 thì 255, số dòng 0–10 thì 10); nullopt: không chặn.
    // Ngoài cận: lỗi như chặn dưới (gõ, ↑ ↓, ⌖), câu "… không được lớn hơn …". Ô số nguyên 0–255: setLowerBound(-1).
    std::optional<double> upperBound() const { return upperBound_; }
    void setUpperBound(std::optional<double> bound);

    bool hasError() const { return !error_.isEmpty(); }
    QString errorMessage() const { return error_; } // câu đang hiện dưới ô

    // ⌖ chỉ có ở ô Distance, Angle; false: bỏ (prototype pick:false).
    bool isPickEnabled() const { return pickEnabled_; }
    void setPickEnabled(bool on);
    // Khóa theo phiên bản: chỉ đọc, ổ khóa thay ⌖ và nút bước.
    bool isLocked() const { return locked_; }
    void setLocked(bool locked);

    // Chỗ nối INT-003 (ponytail ở đầu tệp). Đang lấy: từ khi phát pickRequested đến finishPick/cancelPick.
    bool isPicking() const { return picking_; }
    void finishPick(double value); // giá trị theo đơn vị của value(); không đang lấy thì bỏ qua
    void cancelPick();             // Esc trên vùng vẽ: hiện lại hộp thoại, giá trị không đổi

signals:
    // Người dùng nhận một giá trị hợp lệ (gõ, ↑ ↓, ⌖); setValue, setSpec không phát.
    void valueCommitted(double value);
    // Bấm ⌖: INT-003 lấy khoảng cách 2 điểm (Distance) hoặc góc của đoạn (Angle) rồi gọi finishPick hoặc cancelPick.
    void pickRequested();

protected:
    void changeEvent(QEvent* e) override; // LanguageChange

private:
    class Box;
    class Edit;

    QString format(double value) const;
    QString check(const QString& text, double& value) const; // câu lỗi, rỗng khi đúng (value nhận giá trị)
    QString unitName() const;
    double stepSize() const;
    int boxWidth(int digits) const;
    bool isEdited() const;
    bool commit();
    bool revert();
    void stepBy(int direction, bool fast);
    void offer(double value);
    void accept(double value);
    void display();
    void showError(const QString& why);
    void clearError();
    void startPick();
    bool endPick();
    void updateParts();
    void placeParts();
    void retranslate();
    void updateAccessible();

    ValueSpec spec_;
    QString label_;
    double value_ = 0;
    double step_ = 0;
    std::optional<double> lowerBound_;
    std::optional<double> upperBound_;
    QString shown_; // chữ của value_ (rỗng khi *Khác nhau*): chữ trong ô khác shown_ là đang sửa
    QString error_;
    bool mixed_ = false;
    bool pickEnabled_ = true;
    bool locked_ = false;
    bool picking_ = false;

    Box* box_;
    Edit* edit_;
    QAbstractButton* pick_;
    QAbstractButton* up_;
    QAbstractButton* down_;
    QLabel* msg_;
};

} // namespace trion::ui
