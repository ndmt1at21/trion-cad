#pragma once
// UIC-017 · Bộ chọn màu dùng chung (07.2 UI-C06; lệnh COLOR; prototype index.html#UI-C06, js/screens5.js
// SC.colorPicker, css .aci .acs .col-foot .cmpbox .tcw). Hộp thoại modal trên DialogFrame (UI0-110), mã "color":
//   tab Màu chỉ số   bảng ACI 24 × 10 (10–249: cột là sắc độ, hàng là sắc thái), hàng 1–9, hàng xám 250–255 (RGB của
//                    RGN-002); ByLayer, ByBlock; rê hoặc đi bằng phím: "Chỉ số màu N (tên) · RGB r, g, b"; nhấp chọn,
//                    nhấp đúp chọn và đóng; mũi tên đi và chọn trong bảng, Enter chọn và đóng
//   tab True Color   vùng sắc độ × bão hòa + thanh độ sáng; mô hình HSL hoặc RGB với ô số UI-C09 (chặn 0–360/100/255),
//                    mã hex. Giữ cả HSL (số thực) lẫn RGB: đổi mô hình không tính lại, nên qua lại không trôi màu
//   tab Color Book   "Có sau v1.0" (P2); màu Color Book đang dùng vẫn hiện tên ở ô Màu và đúng RGB ở ô so sánh
//   chân thân        8 màu gần đây (hồ sơ, kRecentColors), nút lấy màu từ bản vẽ, ô "Màu" (gõ số, tên vi/en, ByLayer,
//                    ByBlock, #RRGGBB; sai: viền lỗi và câu dưới hàng, Enter sai không đóng hộp), ô so sánh cũ | mới
// "Dùng màu này" (Enter) phát colorAccepted(màu) rồi đóng; Hủy, Esc: không phát. Màu ACI và true color vào đầu danh
// sách gần đây khi dùng.
// Chế độ TrueColorOnly (màu giao diện vùng vẽ, UID-020): chỉ tab True Color, không ByLayer/ByBlock, không lấy từ bản
// vẽ; số, tên ACI gõ ở ô Màu hay chọn ở hàng gần đây đổi thành RGB tương ứng; kết quả luôn là true color.
//
// Dùng (sau QApplication, loadFonts, ThemeManager, IconProvider, RichTooltip như main):
//   // lệnh COLOR (ponytail: INT-002 thay chỗ nối — lệnh thật đặt CECOLOR qua lõi):
//   ColorPickerDialog::ask(&mainWindow, currentColor, [](const Color& c) { setCurrentColor(c); });
//   // UID-020, màu nền vùng vẽ:
//   ColorPickerDialog::ask(this, Color::rgb(bg), apply, ColorPickerDialog::Mode::TrueColorOnly);
//   // lấy màu từ bản vẽ (P1, INT-009): nút ⌖ chỉ bật khi có nơi nối pickRequested
//   connect(dialog, &ColorPickerDialog::pickRequested, canvas, [=] {
//       cmdline->setPrompt({.command = u"COLOR"_s, .text = ColorPickerDialog::pickPrompt()});
//       pickObject([=](const Color* c) { c ? dialog->finishPick(*c) : dialog->cancelPick(); }); // Esc: nullptr
//   });

#include "app/dialogs/pickers/Color.h"
#include "app/widgets/dialog/DialogFrame.h"
#include "core/base/settings/Settings.h"

#include <QStringList>

#include <array>
#include <cstdint>
#include <functional>

class QLabel;
class QStackedWidget;

namespace trion::ui {

class ColorCompare;
class EmptyState;
class FieldMessage;
class HueSatArea;
class NumericField;
class SwatchGrid;

// 8 màu gần đây của hồ sơ (NT-010): ACI "140", true color "#3CC8C0"; mặc định như prototype.
inline const settings::Setting<QStringList> kRecentColors{
    "color/recent",
    {QStringLiteral("1"), QStringLiteral("3"), QStringLiteral("8"), QStringLiteral("140"), QStringLiteral("30")}};

class ColorPickerDialog : public DialogFrame {
    Q_OBJECT
public:
    enum class Mode : std::uint8_t { Full, TrueColorOnly };
    enum class Tab : std::uint8_t { Index, TrueColor, Book };
    static constexpr int kRecentMax = 8;

    // current: màu đang dùng (ô "cũ", lựa chọn ban đầu; tab đầu theo loại màu). Màu gần đây đọc từ
    // settings::Store::instance().
    explicit ColorPickerDialog(const Color& current, Mode mode = Mode::Full, QWidget* parent = nullptr);

    // Móc mở hộp (lệnh COLOR, palette, layer, UID-020): dựng, open() (modal, không vòng sự kiện lồng), hộp tự hủy khi
    // đóng. done(màu) chỉ gọi khi người dùng dùng màu (nút, Enter, nhấp đúp); Hủy, Esc thì không.
    // ponytail: lệnh COLOR nối tạm qua CommandRouter gọi ask() rồi tự đặt màu hiện hành; INT-002 thay (lệnh COLOR thật:
    // tùy chọn dòng lệnh, đặt CECOLOR qua lõi, undo).
    static ColorPickerDialog* ask(QWidget* parent, const Color& current, std::function<void(const Color&)> done,
                                  Mode mode = Mode::Full);

    Mode mode() const { return mode_; }
    Color current() const { return current_; } // màu cũ
    Color color() const { return pick_; }      // màu đang chọn
    void setColor(const Color& c);             // chọn (TrueColorOnly: đổi thành RGB; ByLayer/ByBlock bỏ qua)
    Tab tab() const;
    void setTab(Tab t);

    // Lấy màu từ bản vẽ (P1). Nút ⌖: ẩn hộp (hideForPick) rồi phát pickRequested; bên nối bấm đối tượng rồi gọi
    // finishPick(màu của đối tượng, ByLayer giữ nguyên ByLayer) hoặc cancelPick() (Esc: hiện lại, không đổi).
    // ponytail: chỗ nối tạm bằng tín hiệu, chưa ai nối thì nút ⌖ vô hiệu; INT-009 thay (hit test trên vùng vẽ, Esc).
    bool isPicking() const { return picking_; }
    void finishPick(const Color& c);
    void cancelPick();
    static QString pickPrompt(); // câu cho dòng lệnh khi đang lấy

    void accept() override; // "Dùng màu này": chữ đang gõ sai thì không đóng; ghi màu gần đây, phát colorAccepted

signals:
    void colorAccepted(const Color& c);
    void pickRequested();

protected:
    void changeEvent(QEvent* e) override;                   // LanguageChange
    void connectNotify(const QMetaMethod& signal) override; // nút ⌖ bật khi có nơi nối pickRequested (lượt kế)
    void disconnectNotify(const QMetaMethod& signal) override;

private:
    class TextField;
    enum class From : std::uint8_t { Other, TrueColor };

    QWidget* buildIndexPane();
    QWidget* buildTrueColorPane();
    QWidget* buildFoot();
    void setPick(const Color& c, From from = From::Other);
    void takeRgb(regen::Rgb c); // RGB từ ngoài tab True Color (ACI, hex, ô RGB): HSL theo, giữ sắc độ khi xám
    void trueColorEdited();     // người dùng sửa ở tab True Color: chọn true color rgb_
    void onField(int i, double v);
    void setModel(int model); // 0 HSL, 1 RGB: đổi nhãn, chặn, giá trị ô số (không tính lại màu)
    void sync();              // mọi phần hiện theo pick_, hsl_, rgb_
    void describe(const Color& c, QWidget* announceFrom = nullptr); // dòng rê; announceFrom: đi bằng phím, đọc to
    QString colorError() const;
    void fitHeight(); // cao thêm cho câu lỗi xuống dòng
    void startPick();
    void updatePickButton();
    void loadRecent();
    void remember(const Color& c);
    void restyle();
    void retranslate();

    Color current_;
    Color pick_;
    Mode mode_;
    Hsl hsl_{176, 0.56, 0.51}; // tab True Color (prototype HSL [176, 56, 51])
    regen::Rgb rgb_{};
    int model_ = 0;
    bool picking_ = false;
    bool syncing_ = false;
    Color described_; // ô vừa rê/đi tới (dòng rê); ByLayer: chưa có
    bool hasDescribed_ = false;

    // = nullptr: DialogFrame cho widget hiện (polish → changeEvent) ngay trong hàm dựng, trước khi gán
    TabBar* tabs_ = nullptr;
    QStackedWidget* panes_ = nullptr;
    QLabel* aciCaption_ = nullptr;
    SwatchGrid* aciGrid_ = nullptr;
    SwatchGrid* standard_ = nullptr;
    SwatchGrid* grays_ = nullptr;
    Button* byLayer_ = nullptr;
    Button* byBlock_ = nullptr;
    QLabel* hover_ = nullptr;
    HueSatArea* area_ = nullptr;
    Slider* light_ = nullptr;
    QLabel* modelLabel_ = nullptr;
    SegmentedGroup* models_ = nullptr;
    std::array<QLabel*, 3> fieldLabels_{};
    std::array<NumericField*, 3> fields_{};
    QLabel* hexLabel_ = nullptr;
    TextField* hex_ = nullptr;
    FieldMessage* hexMsg_ = nullptr;
    EmptyState* book_ = nullptr;
    QLabel* recentLabel_ = nullptr;
    SwatchGrid* recent_ = nullptr;
    QLabel* recentEmpty_ = nullptr;
    Button* pickButton_ = nullptr;
    QLabel* colorLabel_ = nullptr;
    TextField* colorEdit_ = nullptr;
    FieldMessage* colorMsg_ = nullptr;
    ColorCompare* compare_ = nullptr;
    QLabel* hint_ = nullptr;
};

} // namespace trion::ui
