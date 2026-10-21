#include "app/widgets/model/BackgroundModel.h"

#include "core/base/assert.h"

#include <QAbstractItemView>
#include <QCoreApplication>
#include <QEvent>
#include <QScrollBar>
#include <QTimer>

#include <algorithm>
#include <ctime>

namespace trion::ui {
namespace {

using Clock = std::chrono::steady_clock;

const auto kApplyEvent = static_cast<QEvent::Type>(QEvent::registerEventType());
constexpr auto kFrame = std::chrono::milliseconds(16);
constexpr int kChangeChunk = 4096; // dòng tối đa của một dataChanged
constexpr int kAnchorRows = 8; // dòng nhìn thấy ghi lại mỗi view; cả 8 bị xóa thì để Qt giữ giá trị thanh cuộn
constexpr int kHeavyRows = 16'384; // từ cỡ này hoán vị, Settle (O(n)) tốn ≳ 0,2 ms: chỉ chạy đầu lô

// Thời gian CPU của luồng gọi. Linux: CLOCK_THREAD_CPUTIME_ID; nơi khác chưa có (GetThreadTimes của Windows nhảy theo
// nhịp 15,6 ms): -1.
std::chrono::nanoseconds threadCpu()
{
#if defined(__linux__)
    timespec t{};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
    return std::chrono::seconds(t.tv_sec) + std::chrono::nanoseconds(t.tv_nsec);
#else
    return std::chrono::nanoseconds(-1);
#endif
}

} // namespace

BackgroundModelBase::BackgroundModelBase(QObject* parent)
    : QAbstractItemModel(parent), target_(std::make_shared<const Table>())
{
}

BackgroundModelBase::~BackgroundModelBase()
{
    cancel_.cancel(); // việc nền dừng sớm; kết quả của nó bị executor bỏ vì rx_ đã hủy
}

void BackgroundModelBase::request(std::uint64_t version, Same same,
                                  std::function<std::shared_ptr<const Table>(const sched::CancelToken&)> make)
{
    TRION_ASSERT_UI_THREAD();
    if (wanted_ == version)
        return; // cùng đầu vào: 0 việc nền, 0 cập nhật
    wanted_ = version;
    cancel_.cancel();
    cancel_ = sched::CancelSource();
    ++generation_;
    if (have_ == version)
        return; // quay về version đang có: chỉ cần bỏ việc dở
    ++stats_.builds;
    // Việc nền không chạm `this`; chỉ callback đã post (chạy trên luồng UI, rx_ còn sống) mới dùng.
    sched::submit(sched::Priority::Normal, [self = this, rx = rx_, cancel = cancel_.token(), base = target_,
                                            make = std::move(make), same, generation = generation_, version] {
        if (cancel.cancelled())
            return;
        std::shared_ptr<const Table> table = make(cancel);
        if (cancel.cancelled())
            return;
        auto ops = plan(*base, table, same, cancel);
        if (cancel.cancelled())
            return;
        post(rx, [self, generation, version, table, ops = std::move(ops)]() mutable {
            self->accept(generation, version, std::move(table), std::move(ops));
        });
        if (!cancel.cancelled())
            table->index(); // lần dựng đầu chưa cần: dựng sau khi gửi, sẵn cho lần diff kế
    });
}

const BackgroundModelBase::Table::Index& BackgroundModelBase::Table::index() const
{
    std::call_once(indexed_, [this] {
        index_.byKey.reserve(std::size_t(size));
        for (int i = 0; i < size; ++i)
            index_.byKey.emplace_back(key(at(i)), i);
        std::sort(index_.byKey.begin(), index_.byKey.end());
        index_.unique = std::adjacent_find(index_.byKey.begin(), index_.byKey.end(), [](const auto& a, const auto& b) {
                            return a.first == b.first;
                        }) == index_.byKey.end();
        TRION_ASSERT(index_.unique); // hàm dựng trả khóa trùng
    });
    return index_;
}

// Nền: thao tác biến base thành next. Xóa từ cuối lên (chỉ số phía trước không đổi); hoán vị dòng còn lại theo
// thứ tự mới; chèn từ đầu xuống (chỗ chèn chính là vị trí cuối cùng); Settle trỏ mọi dòng vào next. Base rỗng: chèn
// thẳng, không cần chỉ mục khóa.
std::deque<BackgroundModelBase::Op> BackgroundModelBase::plan(const Table& base, std::shared_ptr<const Table> next,
                                                              Same same, const sched::CancelToken& cancel)
{
    std::deque<Op> ops;
    const auto add = [&ops](Op::Kind kind, int first, int count, const Table* table = nullptr,
                            std::vector<int> perm = {}, std::shared_ptr<const Table> owner = {}) {
        ops.push_back({kind, first, count, table, std::move(perm), std::move(owner)});
    };
    const int m = base.size, n = next->size;
    if (m == 0) {
        if (n > 0) {
            add(Op::Insert, 0, n, next.get());
            add(Op::Settle, 0, 0, nullptr, {}, std::move(next));
        }
        return ops;
    }
    const Table::Index &from = base.index(), &to = next->index();
    if (!from.unique || !to.unique) {
        add(Op::Reset, 0, 0, nullptr, {}, std::move(next));
        return ops;
    }
    std::vector<int> toNew(std::size_t(m), -1), fromOld(std::size_t(n), -1);
    for (std::size_t a = 0, b = 0; a < from.byKey.size() && b < to.byKey.size();) {
        const auto [ka, ia] = from.byKey[a];
        const auto [kb, ib] = to.byKey[b];
        if (ka != kb) {
            ka < kb ? ++a : ++b;
            continue;
        }
        toNew[std::size_t(ia)] = ib;
        fromOld[std::size_t(ib)] = ia;
        ++a, ++b;
    }

    for (int i = m - 1; i >= 0; --i) {
        if (toNew[std::size_t(i)] >= 0)
            continue;
        int first = i;
        while (first > 0 && toNew[std::size_t(first - 1)] < 0)
            --first;
        add(Op::Remove, first, i - first + 1);
        i = first;
    }

    std::vector<int> perm; // vị trí cuối của dòng còn lại, theo thứ tự cũ
    perm.reserve(std::size_t(n));
    for (int p : toNew)
        if (p >= 0)
            perm.push_back(p);
    if (!std::is_sorted(perm.begin(), perm.end())) {
        std::vector<int> rank(std::size_t(n), -1); // vị trí cuối → thứ hạng trong dãy còn lại
        for (int j = 0, r = 0; j < n; ++j)
            if (fromOld[std::size_t(j)] >= 0)
                rank[std::size_t(j)] = r++;
        for (int& p : perm)
            p = rank[std::size_t(p)];
        const int count = int(perm.size());
        add(Op::Permute, 0, count, nullptr, std::move(perm));
    }

    for (int j = 0; j < n; ++j) {
        if (fromOld[std::size_t(j)] >= 0)
            continue;
        int last = j;
        while (last + 1 < n && fromOld[std::size_t(last + 1)] < 0)
            ++last;
        add(Op::Insert, j, last - j + 1, next.get());
        j = last;
    }

    // Dòng giữ khóa mà khác nội dung: dataChanged theo đoạn ≤ kChangeChunk dòng, từ dòng đổi đầu đến dòng đổi cuối
    // của đoạn. Mỗi thao tác có trần thời gian (QTreeView duyệt từng dòng của đoạn) mà số thao tác vẫn ít.
    std::vector<std::pair<int, int>> changed; // (đầu, số dòng)
    for (int j = 0, first = -1, last = -1; j <= n; ++j) {
        if ((j & 4095) == 0 && cancel.cancelled())
            return {};
        const int o = j < n ? fromOld[std::size_t(j)] : -1;
        const bool differs = o >= 0 && !same(base.at(o), next->at(j));
        if (first >= 0 && (j == n || (differs && j - first >= kChangeChunk))) {
            changed.emplace_back(first, last - first + 1);
            first = -1;
        }
        if (differs) {
            first = first < 0 ? j : first;
            last = j;
        }
    }
    if (ops.empty() && changed.empty())
        return {}; // y hệt: 0 cập nhật
    add(Op::Settle, 0, 0, nullptr, {}, std::move(next));
    for (const auto& [first, count] : changed)
        add(Op::Change, first, count);
    return ops;
}

void BackgroundModelBase::accept(std::uint64_t generation, std::uint64_t version, std::shared_ptr<const Table> table,
                                 std::deque<Op> ops)
{
    if (generation != generation_) {
        ++stats_.stale; // có yêu cầu mới hơn: kết quả này không bao giờ được hiện
        return;
    }
    have_ = version;
    if (ops.empty()) { // nội dung y hệt bảng mới nhất: giữ target_ (khóa và dòng như nhau)
        if (ops_.empty())
            emit settled(version);
        return;
    }
    target_ = std::move(table);
    std::move(ops.begin(), ops.end(), std::back_inserter(ops_));
    schedule();
}

void BackgroundModelBase::schedule()
{
    if (scheduled_ || ops_.empty())
        return;
    scheduled_ = true;
    // Lô trước bắt đầu chưa đủ một khung: hẹn phần còn lại (timer một lần, chỉ khi còn việc). Không thì đăng sự
    // kiện ưu tiên thấp như UpdateRequest của widget, xếp sau khung đang chờ. Đăng từ luồng UI cho chính mình
    // (quy tắc post/postLatest là cho kết quả từ luồng nền).
    const auto wait = std::chrono::ceil<std::chrono::milliseconds>(stats_.lastBatch + kFrame - Clock::now());
    if (budget_ > std::chrono::nanoseconds::zero() && wait > std::chrono::milliseconds::zero())
        QTimer::singleShot(wait, Qt::PreciseTimer, this, [this] {
            scheduled_ = false;
            if (Clock::now() < stats_.lastBatch + kFrame)
                schedule(); // timer Windows đến sớm tới ~0,5 ms so với steady_clock: hẹn phần còn lại
            else
                applyBatch();
        });
    else
        QCoreApplication::postEvent(this, new QEvent(kApplyEvent), Qt::LowEventPriority);
}

void BackgroundModelBase::customEvent(QEvent* event)
{
    if (event->type() != kApplyEvent)
        return QAbstractItemModel::customEvent(event);
    scheduled_ = false;
    applyBatch();
}

void BackgroundModelBase::applyBatch()
{
    const auto start = Clock::now();
    const auto cpu = threadCpu();
    if (stats_.batches > 0)
        stats_.shortestGap = std::min<std::chrono::nanoseconds>(stats_.shortestGap, start - stats_.lastBatch);
    stats_.lastBatch = start;
    saveAnchors();
    Clock::duration longest{};
    for (std::uint64_t done = 0; !ops_.empty(); ++done) {
        const auto t = Clock::now();
        // Dừng khi thao tác kế tiếp, nếu lâu như thao tác lâu nhất lô này, sẽ vượt hạn mức (chừa 1/8 cho sai số dự
        // đoán: nới khoảng trống, cache), hoặc khi nó là thao tác O(n) của danh sách lớn (không đoán được từ các thao
        // tác nhỏ trước nó, nên mở lô riêng).
        const auto kind = ops_.front().kind;
        const bool heavy = (kind == Op::Permute || kind == Op::Settle || kind == Op::Reset) && rowCount() >= kHeavyRows;
        if (done > 0 && (heavy || (t - start) + longest > budget_ - budget_ / 8))
            break;
        Op op = std::move(ops_.front());
        ops_.pop_front();
        apply(op);
        ++stats_.ops;
        longest = std::max(longest, Clock::now() - t);
    }
    const auto applied = Clock::now();
    if (cpu.count() >= 0)
        stats_.longestApplyCpu = std::max(stats_.longestApplyCpu, threadCpu() - cpu);
    restoreAnchors();
    const auto end = Clock::now();
    ++stats_.batches;
    stats_.applyTotal += applied - start;
    stats_.longestApply = std::max<std::chrono::nanoseconds>(stats_.longestApply, applied - start);
    stats_.longestBatch = std::max<std::chrono::nanoseconds>(stats_.longestBatch, end - start);
    if (!ops_.empty())
        schedule();
    else if (have_)
        emit settled(*have_);
}

void BackgroundModelBase::apply(Op& op)
{
    switch (op.kind) {
    case Op::Remove:
        beginRemoveRows({}, op.first, op.first + op.count - 1);
        moveGap(std::size_t(op.first + op.count));
        gap_ = std::size_t(op.first); // dòng bị xóa nhập vào khoảng trống
        endRemoveRows();
        break;
    case Op::Permute: {
        emit layoutAboutToBeChanged({}, VerticalSortHint);
        const QModelIndexList from = persistentIndexList(); // sau layoutAboutToBeChanged: gồm cả index view vừa ghi
        QModelIndexList to;
        to.reserve(from.size());
        for (const QModelIndex& i : from)
            to.push_back(createIndex(op.perm[std::size_t(i.row())], i.column()));
        std::vector<const void*> moved(static_cast<std::size_t>(rowCount()));
        for (int r = 0; r < rowCount(); ++r)
            moved[std::size_t(op.perm[std::size_t(r)])] = rowAt(r);
        rows_.swap(moved);
        gap_ = gapEnd_ = rows_.size();
        changePersistentIndexList(from, to);
        emit layoutChanged({}, VerticalSortHint);
        break;
    }
    case Op::Insert: {
        beginInsertRows({}, op.first, op.first + op.count - 1);
        const auto count = std::size_t(op.count);
        if (gapEnd_ - gap_ < count) { // nới khoảng trống: thỉnh thoảng, O(n)
            const auto extra = std::max(count, rows_.size() / 4 + 64);
            rows_.insert(rows_.begin() + std::ptrdiff_t(gapEnd_), extra, nullptr);
            gapEnd_ += extra;
        }
        moveGap(std::size_t(op.first));
        for (std::size_t k = 0; k < count; ++k)
            rows_[gap_ + k] = op.table->at(op.first + int(k));
        gap_ += count;
        endInsertRows();
        break;
    }
    case Op::Settle:
    case Op::Reset: {
        // Thứ tự đã khớp bảng mới: trỏ mọi dòng vào nó (dòng đổi nhận nội dung mới, bảng cũ được giải phóng).
        const bool reset = op.kind == Op::Reset;
        if (reset)
            beginResetModel();
        TRION_ASSERT(reset || rowCount() == op.owner->size);
        rows_.resize(std::size_t(op.owner->size));
        gap_ = gapEnd_ = rows_.size();
        for (int j = 0; j < op.owner->size; ++j)
            rows_[std::size_t(j)] = op.owner->at(j);
        shown_ = std::move(op.owner);
        if (reset) {
            endResetModel();
            ++stats_.resets;
        }
        break;
    }
    case Op::Change:
        emit dataChanged(index(op.first, 0), index(op.first + op.count - 1, columnCount() - 1));
        break;
    }
}

// Dời khoảng trống tới vị trí dòng pos: chỉ chép các dòng nằm giữa chỗ cũ và chỗ mới.
void BackgroundModelBase::moveGap(std::size_t pos)
{
    const auto at = [this](std::size_t i) { return rows_.begin() + std::ptrdiff_t(i); };
    const std::size_t len = gapEnd_ - gap_;
    if (pos < gap_)
        std::move_backward(at(pos), at(gap_), at(gapEnd_));
    else
        std::move(at(gapEnd_), at(gapEnd_ + (pos - gap_)), at(gap_));
    gap_ = pos;
    gapEnd_ = pos + len;
}

void BackgroundModelBase::keepScrollPosition(QAbstractItemView* view)
{
    TRION_ASSERT_UI_THREAD();
    if (std::find(views_.begin(), views_.end(), view) == views_.end())
        views_.emplace_back(view);
}

// Neo của mỗi view giữ suốt một lần cập nhật (mọi lô), nhờ vậy vị trí cuối không phụ thuộc cách chia lô. Ghi lại
// khi chưa có, khi mọi dòng neo đã bị xóa, hoặc khi thanh cuộn khác lúc lô trước kết thúc (người dùng tự cuộn).
void BackgroundModelBase::saveAnchors()
{
    std::erase_if(views_, [](const auto& v) { return v.isNull(); });
    std::erase_if(anchors_, [](const Anchor& a) { return !a.view || !a.alive(); });
    for (const auto& v : views_) {
        if (v->model() != this || !v->isVisible() || rowCount() == 0)
            continue;
        QScrollBar* bar = v->verticalScrollBar();
        auto a = std::find_if(anchors_.begin(), anchors_.end(), [&](const Anchor& x) { return x.view == v; });
        if (a == anchors_.end() || bar->value() != a->value) {
            if (a == anchors_.end())
                a = anchors_.insert(anchors_.end(), Anchor{v, 0, 0, {}});
            a->seen.clear();
            // QListView::indexAt tính độ lệch cuộn trước khi chạy bố trí đang chờ (sau chèn/xóa) nên trả index
            // rỗng; visualRect chạy bố trí đang chờ trước (khung tới đằng nào cũng làm).
            v->visualRect(index(0, 0));
            const int height = v->viewport()->height();
            for (QModelIndex i = v->indexAt(QPoint(0, 0)); i.isValid() && a->seen.size() < kAnchorRows;
                 i = index(i.row() + 1, 0)) {
                const int top = v->visualRect(index(i.row(), 0)).top();
                if (top >= height)
                    break;
                a->seen.push_back({index(i.row(), 0), top});
            }
            if (a->seen.empty()) {
                anchors_.erase(a);
                continue;
            }
        }
        a->value = bar->value();
        a->row = a->alive()->index.row();
    }
}

// Dòng neo (dòng nhìn thấy đầu tiên còn tồn tại) về lại đỉnh cũ. Cả số dòng của nó lẫn thanh cuộn không đổi thì
// không làm gì. View tự cuộn trong lô (current index bị xóa → Qt chuyển current và scrollTo) cũng bị đưa về: cập
// nhật nền không được làm nhảy vùng nhìn.
void BackgroundModelBase::restoreAnchors()
{
    for (Anchor& a : anchors_) {
        const Seen* alive = a.view ? a.alive() : nullptr;
        if (!alive)
            continue;
        QScrollBar* bar = a.view->verticalScrollBar();
        if (alive->index.row() != a.row || bar->value() != a.value) {
            // visualRect bố trí lại view ngay (khung tới đằng nào cũng làm), cập nhật dải thanh cuộn.
            const QRect r = a.view->visualRect(alive->index);
            const int delta = r.top() - alive->top;
            const bool pixels = a.view->verticalScrollMode() == QAbstractItemView::ScrollPerPixel;
            // ponytail: ScrollPerItem tính theo chiều cao dòng neo (đúng khi dòng cao đều, như mọi danh sách lớn).
            bar->setValue(bar->value() + (pixels ? delta : delta / std::max(1, r.height())));
        }
        a.value = bar->value();
    }
    if (ops_.empty())
        anchors_.clear(); // xong lần cập nhật: lần sau ghi neo mới
}

int BackgroundModelBase::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(rows_.size() - (gapEnd_ - gap_));
}

int BackgroundModelBase::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : 1;
}

QModelIndex BackgroundModelBase::index(int row, int column, const QModelIndex& parent) const
{
    if (parent.isValid() || row < 0 || row >= rowCount() || column < 0 || column >= columnCount())
        return {};
    return createIndex(row, column);
}

QModelIndex BackgroundModelBase::parent(const QModelIndex&) const
{
    return {};
}

bool BackgroundModelBase::hasChildren(const QModelIndex& parent) const
{
    return !parent.isValid() && rowCount() > 0;
}

Qt::ItemFlags BackgroundModelBase::flags(const QModelIndex& index) const
{
    return index.isValid() ? Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemNeverHasChildren : Qt::NoItemFlags;
}

} // namespace trion::ui
