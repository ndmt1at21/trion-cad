#pragma once
// UIA-007 · Thiết lập dòng lệnh (lệnh INPUTSEARCHOPTIONS; mục "Tùy chọn dòng lệnh…" của menu chuột phải dòng lệnh;
// 07.1 §8 "Thiết lập dòng lệnh"; prototype SC.options tab "Dòng lệnh" trong js/screens2.js: nhãn cột 240 px, .gh, .chk).
// Hộp thoại modal trên DialogFrame (UI0-110):
//   Dòng lệnh            Số dòng lịch sử (ô số UI-C09, 0–10), Cỡ chữ, Độ trong suốt khi nổi (0–90 %)
//   Tự hoàn thành        Độ trễ (0, 200, 500 ms), Tìm biến hệ thống, Tìm lệnh theo tiếng Việt
//   Bộ gõ tiếng Việt     Khi đang chờ tên lệnh: bỏ qua bộ gõ (kImeBypass)
//   Tự sửa lỗi gõ        bảng Gõ sai | Sửa thành | Số lần chọn | thùng rác (bấm, hoặc phím Delete trên dòng chọn)
// "Lưu" ghi mọi giá trị vào settings::Store (NT-010) và xóa các mục AutoCorrect đã bỏ; dòng lệnh, tự hoàn thành, ô nhập
// nghe Store::changed nên áp ngay, không khởi động lại. "Hủy", Esc: không đổi gì (xóa AutoCorrect cũng chỉ áp khi Lưu,
// nên Hủy là hoàn tác).
// ponytail: "Tìm giữa chuỗi" của prototype chưa có thiết lập (UIA-108 luôn tìm giữa chuỗi); số dòng ngoài 0–10 kẹp
// lại khi nhận (NumericField chưa có chặn trên).
//
// Dùng (bước integration, với DialogHost):
//   dialogs.add(u"inputsearchoptions"_s, [](QWidget* parent) { return new InputSearchOptions(parent); });
//   router.add(u"INPUTSEARCHOPTIONS"_s, [&] { dialogs.open(u"inputsearchoptions"_s); });

#include "app/widgets/dialog/DialogFrame.h"

#include <QList>
#include <QStringList>

class QCheckBox;
class QLabel;
class QStandardItemModel;

namespace trion::ui {

class EmptyState;
class NumericField;
class VirtualTableView;

class InputSearchOptions : public DialogFrame {
    Q_OBJECT
public:
    // Đọc giá trị hiện tại từ settings::Store::instance(). Cần ThemeManager, IconProvider.
    explicit InputSearchOptions(QWidget* parent = nullptr);

    void accept() override; // Lưu: ghi Store, xóa AutoCorrect đã bỏ, đóng

protected:
    void changeEvent(QEvent* e) override;
    bool eventFilter(QObject* watched, QEvent* e) override; // Delete trên bảng AutoCorrect

private:
    QLabel* label(const char* source); // nhãn cột trái (.frow .lab)
    QLabel* heading(const char* source); // .gh
    void removeRows(QList<int> rows);  // bỏ mục AutoCorrect (chờ Lưu)
    void restyle();
    void retranslate();

    // = nullptr: DialogFrame cho widget hiện (polish → eventFilter, changeEvent) ngay trong hàm dựng, trước khi gán
    NumericField* lines_ = nullptr;
    DropDown* fontSize_ = nullptr;
    Slider* transparency_ = nullptr;
    QLabel* percent_ = nullptr;
    QLabel* linesHint_ = nullptr;
    DropDown* delay_ = nullptr;
    QCheckBox* sysvars_ = nullptr;
    QCheckBox* vietnamese_ = nullptr;
    QCheckBox* ime_ = nullptr;
    QStandardItemModel* model_ = nullptr;
    VirtualTableView* corrections_ = nullptr;
    EmptyState* none_ = nullptr;
    struct Text {
        QLabel* label;
        const char* source;
    };
    QList<Text> labels_, headings_;
    QStringList removed_; // lỗi gõ đã bỏ, chờ Lưu
};

} // namespace trion::ui
