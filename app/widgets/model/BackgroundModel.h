#pragma once
// UI0-014 · Model nền cho danh sách lớn (DoD-U 4, DoD-L): dựng dòng từ snapshot ở luồng nền, diff theo khóa,
// áp vào model theo lô ≤ 4 ms mỗi khung, giữ chọn và vị trí cuộn; yêu cầu mới hủy yêu cầu cũ.
//
// Dùng, trên luồng UI:
//   struct LayerRow {
//       std::uint64_t key;                                // khóa ổn định (handle), không trùng trong một lần dựng
//       QString name;
//       bool on = true;
//       bool operator==(const LayerRow&) const = default; // biết dòng nào đổi
//   };
//   class LayerModel : public trion::ui::BackgroundModel<LayerRow> {
//       int columnCount(const QModelIndex& p = {}) const override { return p.isValid() ? 0 : 2; }
//       QVariant data(const QModelIndex& i, int role) const override { ... row(i.row()).name ... }
//   };
//   model.keepScrollPosition(view);
//   model.refresh(snapshot, version, [filter](const Snapshot& s, const trion::sched::CancelToken& cancel) {
//       std::vector<LayerRow> rows;   // luồng nền: chỉ đọc snapshot, không chạm model hay widget
//       ...                           // xét cancel.cancelled() khoảng mỗi 1 ms việc, thấy thì trả ngay
//       return rows;
//   });
//
// Hợp đồng
//  - Snapshot bất biến (shared_ptr<const S>; DOC-009 cắm vào đây). Hàm dựng chạy ở worker (sched::submit,
//    Normal), trả dòng theo thứ tự hiển thị. Kết quả về luồng UI qua trion::ui::post (UI0-101). Dòng cũng được
//    hủy ở worker (Background), nên Row không chứa thứ chỉ dùng được trên luồng UI (QPixmap, QObject).
//  - version định danh cả đầu vào: snapshot và tham số dựng (bộ lọc, cách sắp). Trùng version của lần gọi
//    trước: bỏ qua, không việc nền, không tín hiệu. Version khác mà dòng y hệt: diff rỗng, 0 cập nhật model.
//  - Mới thắng: refresh() hủy việc nền đang chạy (hàm dựng thấy cancel); kết quả của yêu cầu cũ về sau bị bỏ,
//    không bao giờ đè kết quả mới hơn.
//  - Diff theo khóa, thứ tự áp: xóa (từng đoạn liên tiếp, từ cuối lên), hoán vị dòng còn lại (một
//    layoutChanged, persistent index đi theo), chèn (từng đoạn liên tiếp), rồi dataChanged theo đoạn ≤ 4.096 dòng
//    (từ dòng đổi đầu đến dòng đổi cuối của đoạn). Không reset, trừ khi hàm dựng trả khóa trùng (lỗi lập trình:
//    assert ở debug, bản release reset để vẫn hiện đúng). Model rỗng (lần dựng đầu): chèn thẳng một đoạn, không diff;
//    chỉ mục khóa (sắp, O(n log n)) dựng ở nền sau khi đã gửi kết quả, nên khung đầu không chờ nó.
//  - Áp theo lô: mỗi lô áp thao tác đến gần hết hạn mức (4 ms, chừa 1/8 cho sai số dự đoán; hoán vị và Settle của
//    danh sách ≥ 16.384 dòng luôn đứng đầu lô) rồi nhường vòng sự kiện; lô kế tiếp bắt đầu sớm nhất
//    một khung (16 ms) sau lô trước, nên mỗi khung có tối đa 4 ms việc áp dù view vẽ theo timer hay theo sự
//    kiện. Selection, current index giữ nhờ persistent index. View đăng ký keepScrollPosition(): dòng nhìn thấy
//    đầu tiên còn tồn tại giữ nguyên chỗ trong suốt lần cập nhật.
//  - Model phẳng một cấp; số cột do lớp con quyết định (columnCount, mặc định 1); flags có
//    ItemNeverHasChildren để QTreeView không hỏi con từng dòng khi bố trí (lớp con đổi flags thì giữ cờ này).
//
// ponytail: model phẳng; cây (Properties theo nhóm, cây block) thêm parent/child khi task cần. Hoán vị là một
// thao tác O(n) trên luồng UI (~1–1,5 ms CPU mỗi 100.000 dòng, đo không view); đoạn dataChanged phủ cả dòng không
// đổi xen giữa (view chỉ vẽ lại vùng nhìn; proxy phía trên sẽ xét lại cả đoạn). Danh sách triệu dòng: chia hoán vị
// theo lô.

#include "app/core/UiExecutor.h"
#include "core/base/sched/Scheduler.h"

#include <QAbstractItemModel>
#include <QPersistentModelIndex>
#include <QPointer>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

class QAbstractItemView;

namespace trion::ui {

// Phần không phụ thuộc kiểu dòng. Dùng BackgroundModel<Row>.
class BackgroundModelBase : public QAbstractItemModel {
    Q_OBJECT
public:
    ~BackgroundModelBase() override;

    // Luồng UI: giữ vị trí cuộn của view (đang dùng model này) khi áp diff. View hủy thì tự bỏ.
    void keepScrollPosition(QAbstractItemView* view);
    // Hạn mức thời gian áp mỗi lô, mặc định 4 ms; hai lô liền nhau cách ≥ 16 ms (một khung). 0 (test): mỗi lô
    // đúng một thao tác, không giãn theo khung.
    void setBatchBudget(std::chrono::nanoseconds budget) { budget_ = budget; }
    // Còn việc: đang dựng ở nền hoặc còn thao tác chưa áp (trạng thái "đang tải" của palette).
    bool busy() const { return wanted_ != have_ || !ops_.empty(); }

    // Số đo cho test, benchmark và QA-011; chỉ đọc trên luồng UI.
    struct Stats {
        std::uint64_t builds = 0;                 // việc dựng đã gửi
        std::uint64_t stale = 0;                  // kết quả cũ bị bỏ
        std::uint64_t batches = 0;                // lượt áp
        std::uint64_t ops = 0;                    // thao tác đã áp (mỗi thao tác một nhóm tín hiệu)
        std::uint64_t resets = 0;                 // reset model (chỉ khi khóa trùng)
        std::chrono::nanoseconds applyTotal{0};   // tổng thời gian áp thao tác
        std::chrono::nanoseconds longestApply{0}; // lô áp thao tác lâu nhất
        // Như trên, tính theo thời gian CPU của luồng UI: không tăng khi máy cho việc khác chạy. -1: nền tảng chưa đo.
        std::chrono::nanoseconds longestApplyCpu{-1};
        std::chrono::nanoseconds longestBatch{0}; // lô lâu nhất kể cả giữ vị trí cuộn (view bố trí lại)
        std::chrono::nanoseconds shortestGap = std::chrono::nanoseconds::max(); // giữa hai lô liền nhau (đầu-đầu)
        std::chrono::steady_clock::time_point lastBatch{};                     // lúc lô gần nhất bắt đầu
    };
    const Stats& stats() const { return stats_; }

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    bool hasChildren(const QModelIndex& parent = {}) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

signals:
    // Model đã hiện đủ kết quả của version: mọi lô đã áp, hoặc kết quả trùng nội dung đang hiện.
    void settled(quint64 version);

protected:
    explicit BackgroundModelBase(QObject* parent = nullptr);

    using Key = std::uint64_t;
    // Dòng của một lần dựng, đã xóa kiểu; bất biến sau khi dựng xong (luồng nền và luồng UI cùng đọc).
    struct Table {
        std::shared_ptr<const void> owner; // giữ std::vector<Row> sống
        const void* data = nullptr;        // dòng 0
        std::size_t stride = 0;            // sizeof(Row)
        int size = 0;
        Key (*key)(const void*) = nullptr; // khóa của một dòng
        const void* at(int i) const { return static_cast<const char*>(data) + std::size_t(i) * stride; }

        struct Index {
            std::vector<std::pair<Key, int>> byKey; // (khóa, dòng), sắp theo khóa
            bool unique = true;                     // không có khóa trùng
        };
        // Chỉ mục khóa cho diff, chỉ luồng nền dùng: dựng lười một lần, việc nền gọi cùng lúc thì chờ lần dựng ấy.
        const Index& index() const;

    private:
        mutable std::once_flag indexed_;
        mutable Index index_;
    };
    using Same = bool (*)(const void*, const void*);

    // Luồng UI. make chạy ở nền, trả bảng; same so nội dung hai dòng cùng khóa.
    void request(std::uint64_t version, Same same,
                 std::function<std::shared_ptr<const Table>(const sched::CancelToken&)> make);
    const void* rowAt(int row) const
    {
        const auto i = std::size_t(row);
        return rows_[i < gap_ ? i : i + (gapEnd_ - gap_)];
    }

private:
    struct Op {
        enum Kind : std::uint8_t { Remove, Permute, Insert, Settle, Change, Reset } kind;
        int first = 0;                       // đoạn [first, first + count) trong trạng thái lúc áp
        int count = 0;
        const Table* table = nullptr;        // Insert: dòng j sau chèn là dòng j của bảng
        std::vector<int> perm;               // Permute: perm[dòng hiện tại] = dòng sau hoán vị
        std::shared_ptr<const Table> owner;  // Settle/Reset: mọi dòng trỏ vào bảng này sau thao tác
                                             // (Change sau Settle chỉ báo dataChanged)
    };
    struct Seen {                  // một dòng nhìn thấy lúc ghi neo
        QPersistentModelIndex index;
        int top;                   // đỉnh trong viewport (px)
    };
    struct Anchor {                // giữ vị trí cuộn của một view suốt một lần cập nhật
        QPointer<QAbstractItemView> view;
        int value;                 // thanh cuộn dọc khi lô gần nhất bắt đầu/kết thúc
        int row;                   // dòng của neo khi lô bắt đầu
        std::vector<Seen> seen;    // trên xuống, tối đa vài dòng
        const Seen* alive() const
        {
            for (const Seen& s : seen)
                if (s.index.isValid())
                    return &s;
            return nullptr;
        }
    };

    static std::deque<Op> plan(const Table& base, std::shared_ptr<const Table> next, Same same,
                               const sched::CancelToken& cancel);
    void accept(std::uint64_t generation, std::uint64_t version, std::shared_ptr<const Table> table,
                std::deque<Op> ops);
    void schedule();
    void customEvent(QEvent* event) override;
    void applyBatch();
    void apply(Op& op);
    void moveGap(std::size_t pos);
    void saveAnchors();
    void restoreAnchors();

    Receiver rx_{this};
    sched::CancelSource cancel_;
    std::uint64_t generation_ = 0;          // tăng mỗi yêu cầu; kết quả khác số này là cũ
    std::optional<std::uint64_t> wanted_;   // version yêu cầu gần nhất
    std::optional<std::uint64_t> have_;     // version của target_
    std::shared_ptr<const Table> target_;   // kết quả mới nhất đã nhận: gốc diff của yêu cầu sau
    std::shared_ptr<const Table> shown_;    // bảng mà rows_ trỏ vào sau lần Settle gần nhất
    // Dòng đang hiện, mảng có khoảng trống [gap_, gapEnd_) (gap buffer): xóa của một plan đi từ cuối lên, chèn đi
    // từ đầu xuống, nên mỗi thao tác chỉ dời các dòng giữa hai chỗ liền nhau, không dời cả đuôi như vector.
    std::vector<const void*> rows_;
    std::size_t gap_ = 0, gapEnd_ = 0;
    std::deque<Op> ops_;                    // thao tác chờ áp
    bool scheduled_ = false;                // đã hẹn lô kế tiếp
    std::chrono::nanoseconds budget_ = std::chrono::milliseconds(4);
    std::vector<QPointer<QAbstractItemView>> views_;
    std::vector<Anchor> anchors_;
    Stats stats_;
};

// Row: có `key` (số nguyên, không trùng trong một lần dựng) và operator==. Lớp con viết data().
template <class Row>
class BackgroundModel : public BackgroundModelBase {
public:
    explicit BackgroundModel(QObject* parent = nullptr) : BackgroundModelBase(parent) {}

    // Luồng UI: dòng đang hiện. Không giữ tham chiếu qua lần trả vòng sự kiện (lô sau có thể giải phóng nó).
    const Row& row(int row) const { return *static_cast<const Row*>(rowAt(row)); }

    // Luồng UI: dựng lại ở nền. build(const S&, const sched::CancelToken&) -> std::vector<Row>.
    template <class S, class Build>
    void refresh(std::shared_ptr<const S> snapshot, std::uint64_t version, Build build)
    {
        request(version, &same,
                [snapshot = std::move(snapshot), build = std::move(build)](const sched::CancelToken& cancel) {
                    std::shared_ptr<const std::vector<Row>> rows(new std::vector<Row>(build(*snapshot, cancel)),
                                                                 &dispose);
                    auto t = std::make_shared<Table>();
                    t->data = rows->data();
                    t->stride = sizeof(Row);
                    t->size = int(rows->size());
                    t->key = &keyOf;
                    t->owner = std::move(rows);
                    return std::shared_ptr<const Table>(std::move(t));
                });
    }

private:
    static bool same(const void* a, const void* b)
    {
        return *static_cast<const Row*>(a) == *static_cast<const Row*>(b);
    }
    static Key keyOf(const void* row) { return Key(static_cast<const Row*>(row)->key); }
    // Hủy dòng ở worker: 100.000 dòng có chuỗi mất 5–9 ms để giải phóng, mà người thả bảng cuối cùng thường là luồng
    // UI (Settle thay bảng đang hiện, kết quả cũ, kết quả trùng nội dung).
    static void dispose(const std::vector<Row>* rows)
    {
        sched::submit(sched::Priority::Background, [rows] { delete rows; });
    }
};

} // namespace trion::ui
