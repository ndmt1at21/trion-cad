#pragma once
// UIH-001 · Hộp thoại In (UI-H02; lệnh PLOT, Ctrl+P). Prototype SC.plot, runPlot trong js/screens2.js; css .grp .gh
// .frow .chk. Modal trên DialogFrame (mã "plot"); mọi trường đọc/ghi một PlotSettingsModel.
//
//   cột chính    Thiết lập trang [tên ▾][Thêm… Pro]
//                Máy in          Tên [thiết bị ▾][Thuộc tính… Pro] · Vị trí · ☐ In ra tệp (PDF, PNG: bắt buộc)
//                Khổ giấy [▾], Số bản, chỗ xem trước nhỏ (UIH-002)  |  Vùng in [▾] + view có tên ▾ | Chọn cửa sổ <
//                                                                   |  Tỉ lệ ☐ Vừa khổ giấy [1:N | Tùy chỉnh ▾]
//                                                                   |    [mm] = [đơn vị] (Tùy chỉnh) ☐ Co giãn
//                                                                   |  Offset ☐ Canh giữa, X / Y (mm)
//   cột mở rộng  Kiểu in (Plot style) [▾][Sửa… Pro] ☐ Hiện kiểu in · Viewport có shade · Tùy chọn in (8, "Lưu thay đổi
//                vào layout" Pro) · Hướng giấy [Dọc | Ngang] ☐ In ngược
//   chân         [Xem trước…][Thêm tùy chọn ▸] ··· [Áp dụng cho layout Pro][Hủy][In]
//
// Hành vi:
//   - "Thêm tùy chọn ▸" hiện/ẩn cột mở rộng trên cùng các widget (không dựng lại, giữ giá trị đang sửa), nhớ trong Store
//     (kPlotMore) cho lần mở sau.
//   - Máy in hệ thống quét ở nền (plot::scanPrinters) khi mở: danh sách có ngay thiết bị dựng sẵn và cache của lần quét
//     trước; quét xong thì cập nhật. Thiết bị của thiết lập không có sau khi quét: dòng lỗi dưới ô máy in.
//   - Vùng in Cửa sổ: "Chọn cửa sổ <" ẩn hộp (hideForPick), phát windowPickRequested; INT-003 lấy 2 góc trên vùng vẽ
//     (có bắt điểm) rồi gọi finishWindowPick (hộp hiện lại kèm tọa độ) hoặc cancelWindowPick (Esc: giữ cửa sổ cũ).
//     Chưa có cửa sổ: dòng lỗi dưới nút, nút In vô hiệu.
//   - In (nút xác nhận, Enter): hộp đóng ngay, PlotRunner chạy nền. "Lưu thay đổi vào layout" (Pro) thì phát thêm
//     applyToLayoutRequested.
//   - Viewer, Viewer Pro: in đủ; Thêm…, Thuộc tính…, Sửa… kiểu in, Áp dụng cho layout, Lưu thay đổi vào layout có huy
//     hiệu Pro, bấm thì mở trang nâng cấp (EditionGate::instance()->upgrade()).
//
// ponytail: chỉ chế độ In; chế độ Thiết lập trang (PAGESETUP, nút "Lưu thiết lập") thêm ở UIH-004. Tỉ lệ Vừa khổ giấy
// chưa hiện số tự tính (PLT-101 computePlotTransform). Chưa có "Kiểm tra trước khi in" (P1, PlotReport của PLT-003),
// dấu in chưa có nút thiết lập (P2), chưa chọn tên tệp ra (dùng plot::defaultFile).
//
// Dùng (integration):
//   PlotRunner plots(jobs);   // một cho ứng dụng
//   dialogs.add(u"plot"_s, [&](QWidget* parent) { return new PlotDialog(plotContext(), &plots, parent); });
//   router.add(u"PLOT"_s, [&] { dialogs.open(u"plot"_s); });            // Ctrl+P (keymap "plot")
//   connect(dlg, &PlotDialog::windowPickRequested, …);                  // INT-003: 2 góc → finishWindowPick

#include "app/dialogs/plot/PlotSettings.h"
#include "app/widgets/dialog/DialogFrame.h"
#include "core/base/settings/Settings.h"

#include <QList>
#include <QPointer>

#include <array>

class QCheckBox;
class QLabel;

namespace trion::ui {

class FieldMessage;
class NumericField;
class PlotRunner;
class PlotSettingsModel;

inline const settings::Setting<bool> kPlotMore{"plot/more", false}; // cột mở rộng đang mở

class PlotDialog : public DialogFrame {
    Q_OBJECT
public:
    // context: dữ liệu bản vẽ; runner: chạy in nền khi bấm In (nullptr: chỉ đóng). context.previous trống thì lấy
    // runner->previous(). Cần ThemeManager, IconProvider, Store; EditionGate::instance() nếu có.
    PlotDialog(plot::PlotContext context, PlotRunner* runner, QWidget* parent = nullptr);
    ~PlotDialog() override;

    PlotSettingsModel* model() const { return model_; }

    bool isExpanded() const;
    void setExpanded(bool on);

    // Chỗ của xem trước nhỏ (UIH-002 đặt PlotMiniPreview vào layout của nó).
    QWidget* previewSlot() const { return preview_; }

    // Vùng in Cửa sổ (INT-003): hai góc theo tọa độ bản vẽ, thứ tự bất kỳ. Không đang chọn thì bỏ qua.
    void finishWindowPick(QPointF first, QPointF second);
    void cancelWindowPick();

    void accept() override; // In: đóng ngay, runner chạy nền

signals:
    void windowPickRequested();
    void previewRequested(const trion::plot::PlotSettings& settings);  // Xem trước… (UI-H03, PREVIEW)
    void addSetupRequested(const trion::plot::PageSetup& setup);       // Thêm… (Pro): lưu thành thiết lập có tên
    void applyToLayoutRequested(const trion::plot::PageSetup& setup);  // Áp dụng cho layout (Pro), PLT-103 ghi
    void editPlotStyleRequested(const QString& name);                  // Sửa… kiểu in (Pro, UI-D05)
    void printerPropertiesRequested(const QString& device);            // Thuộc tính… máy in (Pro, UI-H10)

protected:
    void changeEvent(QEvent* e) override;

private:
    struct Text {
        QLabel* label;
        const char* source;
    };
    QLabel* label(const char* source);   // nhãn hàng (.frow .lab)
    QLabel* heading(const char* source); // đầu nhóm (.gh)
    QWidget* group(const char* title, QLayout* content);
    void build();
    void connectFields();
    void sync();          // mọi widget theo model
    void syncEdition();   // mô tả trợ năng của phần khóa theo phiên bản
    void restyle();
    void retranslate();
    void startWindowPick();
    QSize fitted() const; // sizeHint trong vùng dùng được của màn hình
    bool proLocked() const;
    bool upgradeIfLocked(const QString& feature, const QString& name);
    QString paperLabel(const plot::Paper& paper) const;
    QString deviceLabel(const plot::Device& device) const;

    PlotSettingsModel* model_;
    QPointer<PlotRunner> runner_;
    QList<Text> labels_;
    QList<Text> headings_;
    bool syncing_ = false;
    bool customScale_ = false; // đã chọn "Tùy chỉnh" ở ô tỉ lệ dù giá trị trùng tỉ lệ chuẩn

    // = nullptr: DialogFrame cho widget hiện ngay trong hàm dựng (polish → changeEvent) trước khi gán
    DropDown* setup_ = nullptr;
    Button* addSetup_ = nullptr;
    ComboBox* device_ = nullptr;
    Button* deviceProps_ = nullptr;
    QLabel* where_ = nullptr;
    QCheckBox* toFile_ = nullptr;
    FieldMessage* deviceMsg_ = nullptr;
    ComboBox* paper_ = nullptr;
    QWidget* preview_ = nullptr;
    DropDown* area_ = nullptr;
    ComboBox* view_ = nullptr;
    Button* pickWindow_ = nullptr;
    FieldMessage* windowMsg_ = nullptr;
    QCheckBox* fit_ = nullptr;
    DropDown* scale_ = nullptr;
    NumericField* scaleMm_ = nullptr;
    NumericField* scaleUnits_ = nullptr;
    QWidget* customRow_ = nullptr; // [mm] = [đơn vị]: chỉ khi Tùy chỉnh
    QLabel* scaleEq_ = nullptr;
    QLabel* scaleUnitName_ = nullptr;
    QLabel* scaleInfo_ = nullptr;
    QCheckBox* scaleLw_ = nullptr;
    QCheckBox* center_ = nullptr;
    NumericField* offsetX_ = nullptr;
    NumericField* offsetY_ = nullptr;
    QLabel* offsetUnit_ = nullptr;
    NumericField* copies_ = nullptr;
    QWidget* more_ = nullptr;
    DropDown* plotStyle_ = nullptr;
    Button* editStyle_ = nullptr;
    QCheckBox* showStyles_ = nullptr;
    DropDown* shadeMode_ = nullptr;
    DropDown* shadeQuality_ = nullptr;
    NumericField* dpi_ = nullptr;
    QLabel* dpiUnit_ = nullptr;
    std::array<QCheckBox*, 8> options_{};
    SegmentedGroup* orientation_ = nullptr;
    QCheckBox* upsideDown_ = nullptr;
    Button* previewButton_ = nullptr;
    Button* moreButton_ = nullptr;
    Button* apply_ = nullptr;
};

} // namespace trion::ui
