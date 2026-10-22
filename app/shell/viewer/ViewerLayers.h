#pragma once
// UIA-118 · Palette Layer của Viewer (07.3 UI-M04; prototype #UI-M04: js/palettes.js P.layers, TC.layers, layerRows;
// css/app.css .pal-note .pal-tools .tbl .tgl .tag .pal-foot) và lưu, nạp trạng thái hiển thị .tcview (UI-M03, VP).
//
//   (i) Bật/tắt và đóng băng là tạm thời. Thay đổi không lưu vào bản vẽ.      .pal-note
//   [Tìm layer, ví dụ KT-*                    ] [⋯]                           .pal-tools
//   bảng ảo hóa: trạng thái · Tên [tạm] · Bật · Đóng băng · Khóa · Màu · Số    .tbl (cột gọn của prototype)
//   Hiện 48/312 layer  [Khôi phục như tệp gốc]                               .pal-foot
//
// Hành vi
//  - Không tự đổi layer: mỗi thao tác phát macroRequested (nơi lắp nối vào CommandRouter, INT-002). Bấm ô Bật hoặc Đóng
//    băng (bàn phím: Space hoặc Enter trên ô đó) → '_-LAYER _OFF <tên>; … (LayerMacro). Menu ⋯: Bật tất cả, Rã đông
//    tất cả, Cô lập layer của đối tượng…, Bỏ cô lập → ^C^C_LAYON, _LAYTHW, _LAYISO, _LAYUNISO (IMPL.* của prototype;
//    LAYISO chọn đối tượng trên vùng vẽ). "Khôi phục như tệp gốc" (nút ở chân, mục menu) → một '_-LAYER đưa mọi layer
//    đã đổi về trạng thái trong tệp. Bảng chỉ đổi khi nguồn gọi setLayers lại (không giả kết quả của lệnh).
//  - Ô Khóa, Màu: Viewer không sửa (07.1 §2), bấm mở trang nâng cấp Pro (EditionGate::upgrade).
//  - Tìm: gấp dấu, không phân biệt hoa thường ("kt tuong" khớp "KT TƯỜNG"); có * hoặc ? thì khớp cả tên theo mẫu
//    ("KT-*"). Lọc và sắp (bấm đầu cột) chạy ở nền (BackgroundModel), áp theo lô. Menu ⋯: "Chỉ hiện layer đang dùng".
//  - Thẻ "tạm" ở tên, nút Khôi phục chỉ khi có layer khác tệp; ô đổi tạm có chú thích "đã đổi tạm thời". Không có layer
//    (chưa mở bản vẽ): trạng thái rỗng. "Lưu trạng thái hiển thị" (menu ⋯, huy hiệu Viewer Pro khi bản Viewer) phát
//    saveViewRequested.
//
// Lắp (integration, UIA-017; luồng UI, sau ThemeManager, IconProvider, RichTooltip, UiExecutor, EditionGate):
//   host.registerPalette({u"layers"_s, [] { return ViewerLayers::tr("Layer"); },
//                         [&] { auto* p = new ViewerLayers(gate);
//                               QObject::connect(p, &ViewerLayers::macroRequested, &router, &CommandRouter::submit);
//                               p->setLayers(snapshotOfActiveDocument()); …; return p; }});
//   ViewStateFile states(gate, *banner);                          // một cho cả phiên
//   states.load(path);                                            // khi mở bản vẽ ở hồ sơ Viewer
//   connect(&states, &ViewStateFile::loaded, …(path, s) { đổi layout, view (RND);
//                                                        router.submit(LayerMacro::to(s.layers).text()); });
//   connect(palette, &ViewerLayers::saveViewRequested, … { states.save(path, {layout, view, …, p->overrides()}); });
//   connect(&states, &ViewStateFile::saved, … (path, error) { toast Ok "Đã lưu…" hoặc lỗi });
#include "app/shell/viewer/ViewState.h"

#include <QColor>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QWidget>

#include <cstdint>
#include <memory>

class QLabel;
class QLineEdit;
class QModelIndex;

namespace trion::edition {
class EditionGate;
}
namespace trion::ui {
class Button;
class CanvasBanner;
class EmptyState;
class Glyph;
class Menu;
class VirtualTableView;
} // namespace trion::ui

namespace trion::shell {

// Một layer trong snapshot của bản vẽ đang xem.
struct ViewerLayer {
    QString name;
    bool on = true, frozen = false;         // hiệu lực: tệp + thay đổi tạm của phiên
    bool fileOn = true, fileFrozen = false; // trong tệp: trạng thái lúc mở (Viewer không ghi tệp)
    bool locked = false;                    // chỉ hiện
    bool current = false;                   // layer hiện hành của tệp (CLAYER)
    QColor color;                           // màu layer đã quy ra RGB (ACI, True Color)
    qint64 objects = 0;                     // số đối tượng
    bool changed() const { return on != fileOn || frozen != fileFrozen; }
    bool operator==(const ViewerLayer&) const = default;
};

class ViewerLayers : public QWidget {
    Q_OBJECT
public:
    enum Column { Status, Name, On, Freeze, Lock, Color, Count, ColumnCount };

    explicit ViewerLayers(edition::EditionGate& gate, QWidget* parent = nullptr);
    ~ViewerLayers() override;

    // ponytail: chỗ nối nguồn layer, tạm thay DOC-004 (bảng LAYER), CMD-501 (LayerSessionOverlay, effectiveLayerState,
    // đếm đối tượng ở nền) và DOC-009 (snapshot). Gọi khi mở, đổi bản vẽ và sau mỗi lệnh layer. Tên không trùng (không
    // phân biệt hoa thường). Danh sách giữ nguyên thứ tự giữa các lần gọi của cùng bản vẽ (khóa dòng là chỉ số: bảng
    // layer của Viewer không thêm, bớt); có handle của DOC-009 thì khóa theo handle.
    void setLayers(QList<ViewerLayer> layers);
    const QList<ViewerLayer>& layers() const { return *snapshot_; }

    QList<LayerFlags> overrides() const; // layer khác tệp (hiệu lực), để lưu .tcview
    bool hasChanges() const;
    void restoreOriginal();             // "Khôi phục như tệp gốc"

    // Widget con (test, QA-011).
    QLineEdit* searchField() const;
    ui::VirtualTableView* table() const { return table_; }
    ui::Menu* menu() const { return menu_; } // menu ⋯; mục có objectName: on, thaw, iso, uniso, used, save, restore
    ui::Button* restoreButton() const { return restore_; }
    int shownCount() const; // số dòng đang hiện sau lọc

signals:
    void macroRequested(const QString& macro);
    void saveViewRequested();
    void filtered(); // bảng đã hiện xong kết quả lọc, sắp gần nhất (test, QA-011)

protected:
    void changeEvent(QEvent* e) override; // LanguageChange
    void showEvent(QShowEvent* e) override; // đo lại cột theo font đầu cột đã polish
    bool eventFilter(QObject* watched, QEvent* e) override; // Space trên ô của bảng

private:
    class Model;
    class Delegate;
    void refresh(); // lọc, sắp lại ở nền
    void activate(const QModelIndex& index);
    void toggle(int row, Column column);
    void updateFooter();
    void retranslate();
    void applyTheme();

    edition::EditionGate& gate_;
    std::shared_ptr<const QList<ViewerLayer>> snapshot_; // bất biến: luồng lọc nền đọc cùng lúc
    std::uint64_t version_ = 0;                          // đầu vào lọc, sắp (BackgroundModel::refresh)
    Model* model_;
    QWidget* noteBand_ = nullptr;
    ui::Glyph* noteIcon_ = nullptr;
    QLabel* note_ = nullptr;
    QWidget* toolsBand_ = nullptr;
    QLineEdit* search_ = nullptr;
    ui::Button* more_ = nullptr;
    ui::Menu* menu_ = nullptr;
    ui::VirtualTableView* table_ = nullptr;
    ui::EmptyState* empty_ = nullptr;
    QWidget* footBand_ = nullptr;
    QLabel* count_ = nullptr;
    ui::Button* restore_ = nullptr;
    bool usedOnly_ = false;
    int sortColumn_ = Name;
    Qt::SortOrder sortOrder_ = Qt::AscendingOrder;
};

// Lưu, nạp .tcview (UI-M03 "Lưu trạng thái hiển thị", Viewer Pro). Đọc, ghi ở nền, kết quả về luồng UI; tệp hỏng hoặc
// > 1 MB: banner kBanner trên vùng vẽ, bỏ qua tệp (bản vẽ hiện như tệp gốc). Chỉ chạm tệp .tcview, không mở tệp bản vẽ.
// ponytail: một banner cho cả vùng vẽ (banner theo bản vẽ là của UIA-015); không hủy việc nền khi đóng bản vẽ (tệp
// ≤ 1 MB, kết quả của bản vẽ đã đóng bên nhận tự bỏ theo đường dẫn).
class ViewStateFile : public QObject {
    Q_OBJECT
public:
    static constexpr QStringView kBanner = u"tcview";

    ViewStateFile(edition::EditionGate& gate, ui::CanvasBanner& banner, QObject* parent = nullptr);

    // Bản Viewer (chưa có tính năng): không làm gì. Chưa có tệp: không làm gì; hỏng, quá cỡ: banner.
    void load(const QString& drawing);
    // Bản Viewer: mở trang nâng cấp Viewer Pro, trả false. Còn lại ghi ở nền, xong phát saved, trả true.
    bool save(const QString& drawing, const ViewState& state);

signals:
    void loaded(const QString& drawing, const trion::shell::ViewState& state);
    void saved(const QString& drawing, const QString& error); // error rỗng: đã ghi; có: câu đã dịch (toast lỗi)

private:
    edition::EditionGate& gate_;
    QPointer<ui::CanvasBanner> banner_;
};

} // namespace trion::shell
