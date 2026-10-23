#pragma once
// UIC-008 · Model danh sách layer ảo hóa cho 10.000 layer (07.2 UI-C03; prototype js/palettes.js layerRows, layerTable,
// lstate). Palette Quản lý layer (UIC-009) hiện model này trong ui::VirtualTableView.
//
// Dùng, trên luồng UI (sau UiExecutor, ThemeManager):
//   auto* model = new LayerTableModel(palette);
//   model->setSnapshot(layersOfActiveDocument());          // chỗ nối nguồn (DOC-004/DOC-009 thay)
//   model->setEntities(entityLayersOfActiveDocument());    // đếm đối tượng mỗi layer, song song ở nền
//   view->setModel(model);
//   setupColumns(*view);                                   // cột mặc định, kéo đổi thứ tự, lưu theo hồ sơ
//   model->keepScrollPosition(view);
//   connect(search, &QLineEdit::textChanged, model, &LayerTableModel::setFilterText);
//   // lô sự kiện DOC-007: model->apply(changes); đổi CLAYER: model->setCurrent(key)
//   // chân palette: model->summary(view->selectionModel()->selectedRows().size())
//
// Hành vi
//  - Bảng layer là snapshot bất biến: mỗi layer một LayerEntry dùng chung giữa các snapshot; lô sự kiện chỉ thay entry
//    của layer đổi (apply), không dựng lại cả bảng. Lọc, sắp dựng dòng ở nền (ui::BackgroundModel: diff theo khóa, áp
//    theo lô ≤ 4 ms mỗi khung, giữ chọn và vị trí cuộn); mọi đổi đầu vào (bảng, số đếm, chữ tìm, bộ lọc gốc, cột sắp)
//    là một lần dựng lại, lần mới hủy lần cũ.
//  - Tìm: gấp dấu, không phân biệt hoa thường ("tuong" khớp "TƯỜNG"); chữ có * hoặc ? thì khớp cả tên theo mẫu
//    ("KT-*": mọi layer tên bắt đầu bằng "KT-"; ? đúng một ký tự), không thì tên chứa chữ tìm. Bộ lọc gốc: Tất cả,
//    Đang dùng (có đối tượng), từng xref (layer phụ thuộc "XREF|tên").
//  - Sắp theo cột bất kỳ (sort(), cột -1: thứ tự bảng LAYER của tệp); bằng nhau theo cột thì theo tên gấp dấu, tên,
//    khóa: thứ tự tất định, như nhau giữa các lần chạy, mọi số luồng.
//  - Số đối tượng mỗi layer đếm song song ở nền (countObjects, chia theo số luồng của máy); đang đếm: cột Số đối
//    tượng hiện "…", trạng thái Counting.
//  - Chỉ đọc: model không sửa bản vẽ (sửa trong lưới là UIC-010, qua lệnh INT-002).
//
// ponytail: chỗ nối tạm tối thiểu thay lõi chưa có — LayerSnapshot/LayerChange thay bảng LAYER DOC-004, snapshot
// DOC-009 và lô sự kiện DOC-007; EntityLayers thay duyệt entity của snapshot DOC-009 (đếm lại cả bản vẽ mỗi lần
// setEntities; DOC-007 cập nhật số đếm gia tăng theo sự kiện entity); xref suy từ tên "XREF|tên" (DOC-013 thay).
// Chưa có: dòng bộ lọc trong danh sách và bộ lọc người dùng (UIC-015), cột VP chỉ sửa được ở layout (UIC-012, UIC-013),
// Plot style chỉ sửa được với bản vẽ dùng STB (UIC-010).

#include "app/widgets/model/BackgroundModel.h"

#include "core/base/sched/Scheduler.h"

#include <QColor>
#include <QList>
#include <QString>
#include <QStringView>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace trion::ui {
class VirtualTableView;
}

namespace trion::layers {

using LayerKey = std::uint64_t; // handle của bản ghi LAYER (DOC-001): ổn định, không trùng trong một bản vẽ

// Màu layer: ACI 1–255 hoặc True Color (aci = 0). rgb: màu để vẽ ô mẫu (ACI đã quy ra RGB theo bảng màu, UIC-017).
struct LayerColor {
    int aci = 7;
    QRgb rgb = 0;
    bool operator==(const LayerColor&) const = default;
};

// Một bản ghi LAYER như palette thấy (chỗ nối tạm; DOC-004 thay).
struct LayerInfo {
    LayerKey key = 0;
    QString name; // "0", "KT-TƯỜNG", layer phụ thuộc xref: "XREF|tên"
    bool on = true;
    bool frozen = false;
    bool locked = false;
    bool plot = true;
    bool newVpFrozen = false; // Đóng băng ở VP mới
    LayerColor color;
    QString linetype;
    int lineweight = -3;  // 1/100 mm; -3: Mặc định
    int transparency = 0; // 0–90 (%)
    QString plotStyle;    // bản vẽ CTB: tên suy từ màu ("Color_7"), không sửa được
    QString description;
    // Viewport hiện hành khi đang ở layout (DOC-011): Đóng băng VP và các ghi đè (không có: theo layer).
    bool vpFrozen = false;
    std::optional<LayerColor> vpColor;
    std::optional<QString> vpLinetype;
    std::optional<int> vpLineweight;
    std::optional<int> vpTransparency;
    std::optional<QString> vpPlotStyle;
    bool operator==(const LayerInfo&) const = default;
};

// Bảng LAYER của bản vẽ đang mở.
struct LayerSnapshot {
    std::vector<LayerInfo> layers; // thứ tự bản ghi trong tệp (DOC-004 giữ nguyên)
    LayerKey current = 0;          // CLAYER
};

// Một sự kiện của lô DOC-007. Removed chỉ cần layer.key. Added trùng khóa đã có coi như Changed, Changed khóa chưa có
// coi như Added (layer mới thêm vào cuối bảng).
struct LayerChange {
    enum Kind : std::uint8_t { Added, Changed, Removed };
    Kind kind = Changed;
    LayerInfo layer;
};

// Nguồn entity → layer (chỗ nối tạm; DOC-009 thay): layer của từng entity trong bản vẽ.
struct EntityLayers {
    std::vector<LayerKey> layerOf;
};

// Số đối tượng theo layer, sắp theo khóa.
struct ObjectCounts {
    std::vector<std::pair<LayerKey, qint64>> byKey;
    qint64 of(LayerKey key) const; // layer không có entity: 0
};

// Mọi luồng: đếm entity theo layer, chia `parts` phần chạy song song qua sched::submit (DoD-H 1); phần xong cuối gộp
// và gọi done(kết quả) trên worker của nó. Kết quả như nhau với mọi parts (tổng số nguyên, sắp theo khóa). cancel: các
// phần dừng sớm, done không được gọi.
// ponytail: mỗi phần một std::unordered_map, phần xong cuối gộp tuần tự (10⁶ entity: ~15–25 ms một luồng, bench);
// đếm trên cột layer của snapshot bằng parallel_for (DOC-009, NT-005) thay khi có.
void countObjects(std::shared_ptr<const EntityLayers> entities, int parts, sched::CancelToken cancel,
                  std::function<void(std::shared_ptr<const ObjectCounts>)> done);

// Khớp cả chuỗi theo mẫu: * là chuỗi bất kỳ (kể cả rỗng), ? đúng một ký tự, ký tự khác so đúng nguyên văn.
bool matchWildcard(QStringView text, QStringView pattern);

// Một layer của bảng: dùng chung giữa các snapshot, bất biến. Tên gấp dấu tính lười, một lần, ở luồng đọc đầu tiên.
class LayerEntry {
public:
    explicit LayerEntry(LayerInfo info);

    const LayerInfo info;
    QStringView xref() const { return QStringView(info.name).left(xrefLength_); } // "" nếu không phụ thuộc xref
    const QString& folded() const; // text::foldForSearch(tên); mọi luồng

private:
    qsizetype xrefLength_ = 0;
    mutable std::atomic<bool> foldedReady_{false};
    mutable std::once_flag foldOnce_;
    mutable QString folded_;
};

// Một dòng đang hiện (dựng ở nền).
struct LayerRow {
    LayerKey key = 0;
    std::shared_ptr<const LayerEntry> entry;
    qint64 objects = -1; // -1: chưa đếm xong
    bool current = false;
    bool operator==(const LayerRow&) const = default; // entry so theo con trỏ: layer đổi có entry mới
};

class LayerTableModel final : public trion::ui::BackgroundModel<trion::layers::LayerRow> {
    Q_OBJECT
public:
    // Cột theo 07.2 UI-C03; Vp*: ghi đè của viewport hiện hành khi ở layout.
    enum Column {
        Status,
        Name,
        On,
        Freeze,
        Lock,
        Plot,
        Color,
        Linetype,
        Lineweight,
        Transparency,
        PlotStyle,
        NewVpFreeze,
        Description,
        Objects,
        VpFreeze,
        VpColor,
        VpLinetype,
        VpLineweight,
        VpTransparency,
        VpPlotStyle,
        ColumnCount
    };
    enum class LayerStatus { Current, Used, Unused, Counting };
    enum class Root { All, Used, Xref };
    enum Role {
        KeyRole = Qt::UserRole + 1, // LayerKey (qulonglong)
        ValueRole,   // giá trị thô: bool (cột bật/tắt), int (Status, Độ dày nét, Trong suốt), QColor
                     // (ô mẫu màu do delegate vẽ), qlonglong (Số đối tượng, -1 đang đếm), QString
        OverrideRole // cột Vp*: viewport có ghi đè giá trị này (palette tô nền khác, UIC-013)
    };

    explicit LayerTableModel(QObject* parent = nullptr);
    ~LayerTableModel() override;

    // --- Nguồn (luồng UI). ponytail: chỗ nối tạm, DOC-004/DOC-009/DOC-013 thay.
    // Thay cả bảng (mở, đổi bản vẽ). ponytail: tạo entry trên luồng UI (10.000 layer ~2–4 ms CPU,
    // bench); DOC-009 có thì bọc bản ghi của snapshot, không chép.
    void setSnapshot(LayerSnapshot snapshot);
    void apply(const std::vector<LayerChange>& batch); // lô sự kiện thêm/xóa/đổi layer, áp gia tăng
    void setCurrent(LayerKey key);                     // CLAYER đổi
    // Đếm lại số đối tượng ở nền (nullptr: bỏ số đếm). ponytail: DOC-007 thay bằng cập nhật gia tăng.
    void setEntities(std::shared_ptr<const EntityLayers> entities);
    void setCountParts(int parts) { parts_ = parts; } // số phần đếm song song (mặc định: số luồng của máy)

    // --- Lọc, sắp (luồng UI; dựng lại ở nền).
    void setFilterText(const QString& text);
    void setRoot(Root root, const QString& xref = {});
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    // --- Đọc (luồng UI).
    const LayerInfo& layer(int row) const { return this->row(row).entry->info; }
    LayerStatus status(int row) const;
    int layerCount() const { return int(layers_->size()) - dead_; } // tổng số layer (chân "Hiện 48/312 layer")
    bool counting() const { return counting_; }
    int rowOf(LayerKey key) const; // dòng đang hiện của layer, -1 nếu bị lọc hoặc không có
    struct Xref {
        QString name;
        int layers = 0;
        bool operator==(const Xref&) const = default;
    };
    QList<Xref> xrefs() const; // mỗi xref một nút của cây bộ lọc, theo tên
    QString filterText() const { return text_; }
    Root root() const { return root_; }
    int sortColumn() const { return column_; }
    Qt::SortOrder sortOrder() const { return order_; }
    // Chân palette: "Hiện 48/312 layer · 3 đang chọn" (selected = 0: bỏ phần chọn).
    QString summary(int selected) const;
    void retranslate(); // LanguageChange của palette: đầu cột và chữ trong ô dịch lại

    int columnCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : ColumnCount; }
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

signals:
    void countsChanged(); // số đối tượng mới về (cả khi bỏ số đếm)

private:
    using Layers = std::vector<std::shared_ptr<const LayerEntry>>;
    struct Input;
    static std::vector<LayerRow> build(const Input& in, const sched::CancelToken& cancel);
    void rebuild(); // dựng lại dòng ở nền với đầu vào hiện tại
    void setLayers(std::unique_ptr<Layers> layers);
    QString cellText(const LayerRow& row, int column) const;
    QString cellTip(const LayerRow& row, int column) const;

    std::shared_ptr<const Layers> layers_;       // bảng hiện hành (luồng nền đọc cùng lúc)
    std::unordered_map<LayerKey, int> position_; // khóa → chỉ số trong *layers_; dựng lười ở lần apply đầu
    int dead_ = 0;                               // chỗ trống (layer đã xóa) trong *layers_
    std::shared_ptr<const ObjectCounts> counts_;
    LayerKey current_ = 0;
    QString text_;
    Root root_ = Root::All;
    QString xref_;
    int column_ = Name; // prototype lstate.sort = "name"
    Qt::SortOrder order_ = Qt::AscendingOrder;
    std::uint64_t version_ = 0;
    sched::CancelSource countCancel_;
    std::uint64_t countGeneration_ = 0;
    bool counting_ = false;
    int parts_;
};

// Bố cục cột của bảng layer trên view đã setModel(LayerTableModel): rộng mặc định (prototype .tbl, không hẹp hơn chữ
// đầu cột), cột Vp* ẩn, kéo đổi thứ tự (cột Trạng thái đứng đầu), sắp theo Tên, gõ để nhảy theo Tên. Rộng, ẩn, thứ tự,
// cột sắp người dùng đổi lưu theo hồ sơ (settings::Store "views/layers", VirtualTableView::setStateKey) và nạp lại ở
// lần mở sau.
void setupColumns(ui::VirtualTableView& view);
// Cột Vp* hiện khi đang ở layout, ẩn ở Model; cột khác giữ nguyên rộng. ponytail: cột Vp* người dùng ẩn trong layout
// hiện lại ở lần đổi không gian sau (UIC-012 nhớ riêng).
void showViewportColumns(ui::VirtualTableView& view, bool layout);

} // namespace trion::layers
