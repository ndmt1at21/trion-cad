#include "app/palettes/layers/LayerTableModel.h"

#include "app/core/i18n/UiFormat.h"
#include "app/core/UiExecutor.h"
#include "app/theme/generated/Tokens.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/views/VirtualTableView.h"
#include "core/base/text/Fold.h"

#include <QFontMetrics>
#include <QHeaderView>

#include <algorithm>
#include <map>
#include <thread>

using namespace Qt::StringLiterals;

namespace trion::layers {
namespace {

using M = LayerTableModel;

// Phần đếm nhỏ nhất: nhỏ hơn thì chia thêm tốn hơn lợi (đánh thức worker, gộp bảng băm).
constexpr std::size_t kMinPart = 16'384;
// Đếm: xét hủy mỗi chừng này entity (~0,5 ms), DoD-H 5. Dựng dòng: mỗi chừng này layer.
constexpr std::size_t kCountStride = 65'536;
constexpr std::size_t kBuildStride = 1'024;
// Gấp dấu tên trước khi dựng lần đầu: mỗi việc nền chừng này layer.
constexpr std::size_t kFoldPart = 1'024;

int defaultParts()
{
    return int(std::max(1u, std::thread::hardware_concurrency()));
}

int px(tok::PxId id)
{
    return qRound(theme::ThemeManager::instance().px(id));
}

// Bảng layer bất biến dùng chung với luồng nền: hủy ở worker (10.000 con trỏ chung, thường luồng UI thả cuối).
std::shared_ptr<const std::vector<std::shared_ptr<const LayerEntry>>>
share(std::unique_ptr<std::vector<std::shared_ptr<const LayerEntry>>> layers)
{
    using Layers = std::vector<std::shared_ptr<const LayerEntry>>;
    return std::shared_ptr<const Layers>(
        layers.release(), [](const Layers* l) { sched::submit(sched::Priority::Background, [l] { delete l; }); });
}

bool isToggle(int column)
{
    switch (column) {
    case M::On:
    case M::Freeze:
    case M::Lock:
    case M::Plot:
    case M::NewVpFreeze:
    case M::VpFreeze:
        return true;
    default:
        return false;
    }
}

bool isNumber(int column)
{
    return column == M::Lineweight || column == M::Transparency || column == M::Objects || column == M::VpLineweight ||
           column == M::VpTransparency;
}

M::LayerStatus statusOf(const LayerRow& r)
{
    return r.current        ? M::LayerStatus::Current
           : r.objects > 0  ? M::LayerStatus::Used
           : r.objects == 0 ? M::LayerStatus::Unused
                            : M::LayerStatus::Counting;
}

// Giá trị hiệu lực ở viewport hiện hành: ghi đè nếu có, không thì của layer.
template <class T>
const T& atViewport(const std::optional<T>& override, const T& layer)
{
    return override ? *override : layer;
}

QString colorName(const LayerColor& c)
{
    switch (c.aci) {
    case 0: // True Color
        return u"%1,%2,%3"_s.arg(qRed(c.rgb)).arg(qGreen(c.rgb)).arg(qBlue(c.rgb));
    case 1:
        return LayerTableModel::tr("Đỏ");
    case 2:
        return LayerTableModel::tr("Vàng");
    case 3:
        return LayerTableModel::tr("Xanh lá");
    case 4:
        return LayerTableModel::tr("Xanh lơ");
    case 5:
        return LayerTableModel::tr("Xanh dương");
    case 6:
        return LayerTableModel::tr("Tím");
    case 7:
        return LayerTableModel::tr("Trắng");
    default:
        return QString::number(c.aci);
    }
}

QString lineweightText(int lw)
{
    // Số của bản vẽ: luôn dấu chấm thập phân (QString::number không theo locale), 2 chữ số như prototype.
    return lw < 0 ? LayerTableModel::tr("Mặc định")
                  : LayerTableModel::tr("%1 mm").arg(QString::number(lw / 100.0, 'f', 2));
}

QString headerLabel(int column)
{
    switch (column) {
    case M::Status:
        return LayerTableModel::tr("TT");
    case M::Name:
        return LayerTableModel::tr("Tên");
    case M::On:
        return LayerTableModel::tr("Bật");
    case M::Freeze:
        return LayerTableModel::tr("Băng");
    case M::Lock:
        return LayerTableModel::tr("Khóa");
    case M::Plot:
        return LayerTableModel::tr("In");
    case M::Color:
        return LayerTableModel::tr("Màu");
    case M::Linetype:
        return LayerTableModel::tr("Kiểu đường");
    case M::Lineweight:
        return LayerTableModel::tr("Độ dày nét");
    case M::Transparency:
        return LayerTableModel::tr("Trong suốt");
    case M::PlotStyle:
        return LayerTableModel::tr("Kiểu in");
    case M::NewVpFreeze:
        return LayerTableModel::tr("VP mới");
    case M::Description:
        return LayerTableModel::tr("Mô tả");
    case M::Objects:
        return LayerTableModel::tr("Số đối tượng");
    case M::VpFreeze:
        return LayerTableModel::tr("Băng VP");
    case M::VpColor:
        return LayerTableModel::tr("Màu VP");
    case M::VpLinetype:
        return LayerTableModel::tr("Kiểu đường VP");
    case M::VpLineweight:
        return LayerTableModel::tr("Độ dày nét VP");
    case M::VpTransparency:
        return LayerTableModel::tr("Trong suốt VP");
    case M::VpPlotStyle:
        return LayerTableModel::tr("Kiểu in VP");
    default:
        return {};
    }
}

// Tên đầy đủ của cột (chú thích đầu cột, trình đọc màn hình).
QString headerTip(int column)
{
    switch (column) {
    case M::Status:
        return LayerTableModel::tr("Trạng thái");
    case M::On:
        return LayerTableModel::tr("Bật/Tắt");
    case M::Freeze:
        return LayerTableModel::tr("Đóng băng");
    case M::Plot:
        return LayerTableModel::tr("In / Không in");
    case M::Linetype:
        return LayerTableModel::tr("Kiểu đường (Linetype)");
    case M::Transparency:
        return LayerTableModel::tr("Độ trong suốt");
    case M::PlotStyle:
        return LayerTableModel::tr("Kiểu in (Plot style)");
    case M::NewVpFreeze:
        return LayerTableModel::tr("Đóng băng ở viewport mới");
    case M::Objects:
        return LayerTableModel::tr("Số đối tượng trên layer");
    case M::VpFreeze:
        return LayerTableModel::tr("Đóng băng ở viewport hiện hành");
    case M::VpColor:
        return LayerTableModel::tr("Màu ở viewport hiện hành");
    case M::VpLinetype:
        return LayerTableModel::tr("Kiểu đường ở viewport hiện hành");
    case M::VpLineweight:
        return LayerTableModel::tr("Độ dày nét ở viewport hiện hành");
    case M::VpTransparency:
        return LayerTableModel::tr("Độ trong suốt ở viewport hiện hành");
    case M::VpPlotStyle:
        return LayerTableModel::tr("Kiểu in ở viewport hiện hành");
    default:
        return headerLabel(column);
    }
}

bool overridden(const LayerInfo& l, int column)
{
    switch (column) {
    case M::VpColor:
        return l.vpColor.has_value();
    case M::VpLinetype:
        return l.vpLinetype.has_value();
    case M::VpLineweight:
        return l.vpLineweight.has_value();
    case M::VpTransparency:
        return l.vpTransparency.has_value();
    case M::VpPlotStyle:
        return l.vpPlotStyle.has_value();
    default:
        return false;
    }
}

// ------------------------------------------------------------------------------------------------- Sắp

template <class T>
int cmp(const T& a, const T& b)
{
    return a < b ? -1 : b < a ? 1 : 0;
}

int cmpText(const QString& a, const QString& b) // không phân biệt hoa thường, không theo locale: tất định
{
    return QString::compare(a, b, Qt::CaseInsensitive);
}

int cmpColor(const LayerColor& a, const LayerColor& b) // ACI 1–255 rồi True Color theo RGB
{
    return cmp(std::pair(a.aci == 0 ? 256 : a.aci, a.rgb), std::pair(b.aci == 0 ? 256 : b.aci, b.rgb));
}

int cmpName(const LayerRow& a, const LayerRow& b)
{
    if (const int c = a.entry->folded().compare(b.entry->folded()); c != 0)
        return c;
    return a.entry->info.name.compare(b.entry->info.name);
}

// So theo cột, chiều tăng; 0: bằng nhau theo cột. Bật trước tắt, chưa đóng băng/khóa trước, in trước không in.
int compareColumn(const LayerRow& a, const LayerRow& b, int column)
{
    const LayerInfo &x = a.entry->info, &y = b.entry->info;
    switch (column) {
    case M::Status:
        return cmp(int(statusOf(a)), int(statusOf(b)));
    case M::Name:
        return cmpName(a, b);
    case M::On:
        return cmp(!x.on, !y.on);
    case M::Freeze:
        return cmp(x.frozen, y.frozen);
    case M::Lock:
        return cmp(x.locked, y.locked);
    case M::Plot:
        return cmp(!x.plot, !y.plot);
    case M::Color:
        return cmpColor(x.color, y.color);
    case M::Linetype:
        return cmpText(x.linetype, y.linetype);
    case M::Lineweight:
        return cmp(x.lineweight, y.lineweight);
    case M::Transparency:
        return cmp(x.transparency, y.transparency);
    case M::PlotStyle:
        return cmpText(x.plotStyle, y.plotStyle);
    case M::NewVpFreeze:
        return cmp(x.newVpFrozen, y.newVpFrozen);
    case M::Description:
        return cmpText(x.description, y.description);
    case M::Objects:
        return cmp(a.objects, b.objects);
    case M::VpFreeze:
        return cmp(x.vpFrozen, y.vpFrozen);
    case M::VpColor:
        return cmpColor(atViewport(x.vpColor, x.color), atViewport(y.vpColor, y.color));
    case M::VpLinetype:
        return cmpText(atViewport(x.vpLinetype, x.linetype), atViewport(y.vpLinetype, y.linetype));
    case M::VpLineweight:
        return cmp(atViewport(x.vpLineweight, x.lineweight), atViewport(y.vpLineweight, y.lineweight));
    case M::VpTransparency:
        return cmp(atViewport(x.vpTransparency, x.transparency), atViewport(y.vpTransparency, y.transparency));
    case M::VpPlotStyle:
        return cmpText(atViewport(x.vpPlotStyle, x.plotStyle), atViewport(y.vpPlotStyle, y.plotStyle));
    default:
        return 0;
    }
}

// Thứ tự toàn phần: cột theo chiều đã chọn, rồi tên (gấp dấu, nguyên văn), khóa luôn tăng dần.
bool before(const LayerRow& a, const LayerRow& b, int column, Qt::SortOrder order)
{
    if (const int c = compareColumn(a, b, column); c != 0)
        return order == Qt::AscendingOrder ? c < 0 : c > 0;
    if (column != M::Name)
        if (const int c = cmpName(a, b); c != 0)
            return c < 0;
    return a.key < b.key;
}

} // namespace

// ------------------------------------------------------------------------------------------------- Đếm, khớp

qint64 ObjectCounts::of(LayerKey key) const
{
    const auto it = std::lower_bound(byKey.begin(), byKey.end(), key,
                                     [](const std::pair<LayerKey, qint64>& e, LayerKey k) { return e.first < k; });
    return it != byKey.end() && it->first == key ? it->second : 0;
}

void countObjects(std::shared_ptr<const EntityLayers> entities, int parts, sched::CancelToken cancel,
                  std::function<void(std::shared_ptr<const ObjectCounts>)> done)
{
    using Counts = std::unordered_map<LayerKey, qint64>;
    struct Job {
        std::shared_ptr<const EntityLayers> entities;
        sched::CancelToken cancel;
        std::function<void(std::shared_ptr<const ObjectCounts>)> done;
        std::vector<Counts> partial; // mỗi phần ghi ô của mình; phần xong cuối đọc tất cả
        std::atomic<int> left{0};
    };
    const std::size_t n = entities ? entities->layerOf.size() : 0;
    const int count =
        int(std::clamp<std::size_t>(std::size_t(std::max(parts, 1)), 1, std::max<std::size_t>(1, n / kMinPart)));
    auto job = std::make_shared<Job>();
    job->entities = std::move(entities);
    job->cancel = std::move(cancel);
    job->done = std::move(done);
    job->partial.resize(std::size_t(count));
    job->left.store(count, std::memory_order_relaxed);
    for (int p = 0; p < count; ++p) {
        sched::submit(sched::Priority::Normal, [job, p, count, n] {
            const std::size_t first = n * std::size_t(p) / std::size_t(count);
            const std::size_t last = n * std::size_t(p + 1) / std::size_t(count);
            Counts& counts = job->partial[std::size_t(p)];
            const LayerKey* layerOf = n ? job->entities->layerOf.data() : nullptr;
            LayerKey runKey = 0;
            qint64* run =
                nullptr; // entity liền nhau thường cùng layer: khỏi tra bảng băm (con trỏ giá trị bền qua rehash)
            for (std::size_t i = first; i < last && !job->cancel.cancelled();) {
                for (const std::size_t end = std::min(last, i + kCountStride); i < end; ++i) {
                    if (!run || layerOf[i] != runKey) {
                        runKey = layerOf[i];
                        run = &counts[runKey];
                    }
                    ++*run;
                }
            }
            if (job->left.fetch_sub(1, std::memory_order_acq_rel) != 1 || job->cancel.cancelled())
                return;
            Counts all = std::move(job->partial[0]);
            for (std::size_t q = 1; q < job->partial.size(); ++q)
                for (const auto& [key, c] : job->partial[q])
                    all[key] += c;
            auto out = std::make_shared<ObjectCounts>();
            out->byKey.assign(all.begin(), all.end());
            std::sort(out->byKey.begin(), out->byKey.end());
            job->done(std::move(out));
        });
    }
}

bool matchWildcard(QStringView text, QStringView pattern)
{
    // Tham lam, quay lui về dấu * gần nhất: O(|text|·|pattern|) xấu nhất, thường tuyến tính.
    qsizetype t = 0, p = 0, star = -1, resume = 0;
    while (t < text.size()) {
        if (p < pattern.size() && (pattern[p] == u'?' || (pattern[p] != u'*' && pattern[p] == text[t]))) {
            ++t;
            ++p;
        } else if (p < pattern.size() && pattern[p] == u'*') {
            star = p++;
            resume = t;
        } else if (star >= 0) {
            p = star + 1;
            t = ++resume;
        } else {
            return false;
        }
    }
    while (p < pattern.size() && pattern[p] == u'*')
        ++p;
    return p == pattern.size();
}

LayerEntry::LayerEntry(LayerInfo layer)
    : info(std::move(layer)), xrefLength_(std::max<qsizetype>(0, info.name.indexOf(u'|')))
{
}

const QString& LayerEntry::folded() const
{
    if (!foldedReady_.load(std::memory_order_acquire))
        std::call_once(foldOnce_, [this] {
            folded_ = text::foldForSearch(info.name);
            foldedReady_.store(true, std::memory_order_release);
        });
    return folded_;
}

// ------------------------------------------------------------------------------------------------- Model

// Đầu vào một lần dựng: bất biến, luồng nền đọc.
struct LayerTableModel::Input {
    std::shared_ptr<const Layers> layers;
    std::shared_ptr<const ObjectCounts> counts;
    LayerKey current = 0;
    QString needle; // chữ tìm đã gấp dấu
    bool wildcard = false;
    Root root = Root::All;
    QString xref;
    int column = Name;
    Qt::SortOrder order = Qt::AscendingOrder;
};

LayerTableModel::LayerTableModel(QObject* parent)
    : BackgroundModel(parent), layers_(share(std::make_unique<Layers>())), parts_(defaultParts())
{
}

LayerTableModel::~LayerTableModel()
{
    countCancel_.cancel();
}

void LayerTableModel::setSnapshot(LayerSnapshot snapshot)
{
    auto layers = std::make_unique<Layers>();
    layers->reserve(snapshot.layers.size());
    for (LayerInfo& l : snapshot.layers)
        layers->push_back(std::make_shared<const LayerEntry>(std::move(l)));
    current_ = snapshot.current;
    position_.clear(); // dựng lại ở lần apply sau
    dead_ = 0;
    setLayers(std::move(layers));
    // Gấp dấu tên song song (DoD-H 1) trước lần dựng: việc dựng đến entry nào đang gấp thì chờ entry ấy (call_once).
    for (std::size_t first = kFoldPart; first < layers_->size(); first += kFoldPart)
        sched::submit(sched::Priority::Normal, [layers = layers_, first] {
            for (std::size_t i = first, last = std::min(layers->size(), first + kFoldPart); i < last; ++i)
                (*layers)[i]->folded();
        });
}

void LayerTableModel::apply(const std::vector<LayerChange>& batch)
{
    if (batch.empty())
        return;
    if (position_.empty()) {
        position_.reserve(layers_->size() + batch.size());
        for (std::size_t i = 0; i < layers_->size(); ++i)
            if (const auto& e = (*layers_)[i])
                position_.emplace(e->info.key, int(i));
    }
    auto next = std::make_unique<Layers>(*layers_); // bảng cũ có thể đang được việc nền đọc: chép con trỏ
    for (const LayerChange& c : batch) {
        const auto it = position_.find(c.layer.key);
        if (c.kind == LayerChange::Removed) {
            if (it != position_.end()) {
                (*next)[std::size_t(it->second)].reset();
                position_.erase(it);
                ++dead_;
            }
        } else if (it != position_.end()) {
            std::shared_ptr<const LayerEntry>& slot = (*next)[std::size_t(it->second)];
            if (!(slot->info == c.layer))
                slot = std::make_shared<const LayerEntry>(c.layer);
        } else {
            position_.emplace(c.layer.key, int(next->size()));
            next->push_back(std::make_shared<const LayerEntry>(c.layer));
        }
    }
    // Layer đã xóa để lại chỗ trống (dựng dòng bỏ qua); dồn một lượt, giữ thứ tự bảng, khi chỗ trống quá 1/4 bảng: lô
    // nhỏ không phải đánh số lại cả bảng.
    if (std::size_t(dead_) * 4 > next->size()) {
        std::size_t to = 0;
        for (std::size_t from = 0; from < next->size(); ++from) {
            if (!(*next)[from])
                continue;
            if (to != from) {
                (*next)[to] = std::move((*next)[from]);
                position_[(*next)[to]->info.key] = int(to);
            }
            ++to;
        }
        next->resize(to);
        dead_ = 0;
    }
    setLayers(std::move(next));
}

void LayerTableModel::setLayers(std::unique_ptr<Layers> layers)
{
    layers_ = share(std::move(layers));
    rebuild();
}

void LayerTableModel::setCurrent(LayerKey key)
{
    if (key == current_)
        return;
    current_ = key;
    rebuild();
}

void LayerTableModel::setEntities(std::shared_ptr<const EntityLayers> entities)
{
    countCancel_.cancel();
    countCancel_ = sched::CancelSource();
    const std::uint64_t generation = ++countGeneration_;
    if (!entities) {
        counting_ = false;
        counts_.reset();
        rebuild();
        emit countsChanged();
        return;
    }
    counting_ = true;
    countObjects(std::move(entities), parts_, countCancel_.token(),
                 [rx = ui::Receiver(this), this, generation](std::shared_ptr<const ObjectCounts> counts) {
                     ui::post(rx, [this, generation, counts = std::move(counts)]() mutable {
                         if (generation != countGeneration_)
                             return; // đã có lần đếm mới hơn
                         counting_ = false;
                         counts_ = std::move(counts);
                         rebuild();
                         emit countsChanged();
                     });
                 });
}

void LayerTableModel::setFilterText(const QString& text)
{
    if (text == text_)
        return;
    text_ = text;
    rebuild();
}

void LayerTableModel::setRoot(Root root, const QString& xref)
{
    const QString x = root == Root::Xref ? xref : QString();
    if (root == root_ && x == xref_)
        return;
    root_ = root;
    xref_ = x;
    rebuild();
}

void LayerTableModel::sort(int column, Qt::SortOrder order)
{
    if (column < 0 || column >= ColumnCount)
        column = -1;
    if (column == column_ && order == order_)
        return;
    column_ = column;
    order_ = order;
    rebuild();
}

void LayerTableModel::rebuild()
{
    auto in = std::make_shared<Input>();
    in->layers = layers_;
    in->counts = counts_;
    in->current = current_;
    const QString query = text_.trimmed();
    in->needle = text::foldForSearch(query);
    in->wildcard = query.contains(u'*') || query.contains(u'?');
    in->root = root_;
    in->xref = xref_;
    in->column = column_;
    in->order = order_;
    BackgroundModel::refresh(std::shared_ptr<const Input>(std::move(in)), ++version_, &LayerTableModel::build);
}

std::vector<LayerRow> LayerTableModel::build(const Input& in, const sched::CancelToken& cancel)
{
    const Layers& layers = *in.layers;
    std::vector<LayerRow> rows;
    rows.reserve(layers.size());
    for (std::size_t i = 0; i < layers.size(); ++i) {
        if (i % kBuildStride == 0 && cancel.cancelled())
            return rows; // kết quả đã hủy: model bỏ
        const std::shared_ptr<const LayerEntry>& e = layers[i];
        if (!e)
            continue; // layer đã xóa (chỗ trống chờ dồn)
        const qint64 objects = in.counts ? in.counts->of(e->info.key) : -1;
        if (in.root == Root::Used && objects <= 0)
            continue;
        if (in.root == Root::Xref && (e->xref().isEmpty() || e->xref().compare(in.xref, Qt::CaseInsensitive) != 0))
            continue;
        if (!in.needle.isEmpty()) {
            const QString& name = e->folded();
            if (in.wildcard ? !matchWildcard(name, in.needle) : !name.contains(in.needle))
                continue;
        }
        rows.push_back({e->info.key, e, objects, e->info.key == in.current});
    }
    if (in.column >= 0 && !cancel.cancelled())
        std::sort(rows.begin(), rows.end(),
                  [&](const LayerRow& a, const LayerRow& b) { return before(a, b, in.column, in.order); });
    return rows;
}

LayerTableModel::LayerStatus LayerTableModel::status(int row) const
{
    return statusOf(this->row(row));
}

int LayerTableModel::rowOf(LayerKey key) const
{
    for (int r = 0, n = rowCount(); r < n; ++r)
        if (row(r).key == key)
            return r;
    return -1;
}

QList<LayerTableModel::Xref> LayerTableModel::xrefs() const
{
    std::map<QString, Xref> byName; // khóa: tên gấp hoa thường (tên xref không phân biệt hoa thường)
    for (const auto& e : *layers_) {
        const QStringView x = e ? e->xref() : QStringView();
        if (x.isEmpty())
            continue;
        Xref& slot = byName[x.toString().toCaseFolded()];
        if (slot.name.isEmpty())
            slot.name = x.toString();
        ++slot.layers;
    }
    QList<Xref> out;
    out.reserve(qsizetype(byName.size()));
    for (auto& entry : byName)
        out.push_back(std::move(entry.second));
    return out;
}

QString LayerTableModel::summary(int selected) const
{
    const QString shown = i18n::formatCount(rowCount()), total = i18n::formatCount(layerCount());
    return selected > 0 ? tr("Hiện %1/%2 layer · %3 đang chọn").arg(shown, total, i18n::formatCount(selected))
                        : tr("Hiện %1/%2 layer").arg(shown, total);
}

void LayerTableModel::retranslate()
{
    emit headerDataChanged(Qt::Horizontal, 0, ColumnCount - 1);
    if (rowCount() > 0)
        emit dataChanged(index(0, 0), index(rowCount() - 1, ColumnCount - 1));
}

QString LayerTableModel::cellText(const LayerRow& r, int column) const
{
    const LayerInfo& l = r.entry->info;
    switch (column) {
    case Name:
        return l.name;
    case Color:
        return colorName(l.color);
    case Linetype:
        return l.linetype;
    case Lineweight:
        return lineweightText(l.lineweight);
    case Transparency:
        return QString::number(l.transparency);
    case PlotStyle:
        return l.plotStyle;
    case Description:
        return l.description;
    case Objects:
        return r.objects < 0 ? u"…"_s : i18n::formatCount(r.objects);
    case VpColor:
        return colorName(atViewport(l.vpColor, l.color));
    case VpLinetype:
        return atViewport(l.vpLinetype, l.linetype);
    case VpLineweight:
        return lineweightText(atViewport(l.vpLineweight, l.lineweight));
    case VpTransparency:
        return QString::number(atViewport(l.vpTransparency, l.transparency));
    case VpPlotStyle:
        return atViewport(l.vpPlotStyle, l.plotStyle);
    default: // Trạng thái, cột bật/tắt: ký hiệu do delegate vẽ, chữ ở chú thích
        return {};
    }
}

// Trạng thái bằng chữ (không chỉ ký hiệu, màu): chú thích và tên cho trình đọc màn hình.
QString LayerTableModel::cellTip(const LayerRow& r, int column) const
{
    const LayerInfo& l = r.entry->info;
    switch (column) {
    case Status:
        switch (statusOf(r)) {
        case LayerStatus::Current:
            return tr("Layer hiện hành");
        case LayerStatus::Used:
            return tr("Đang dùng");
        case LayerStatus::Unused:
            return tr("Không có đối tượng");
        case LayerStatus::Counting:
            return tr("Đang đếm đối tượng…");
        }
        return {};
    case On:
        return l.on ? tr("Đang bật") : tr("Đang tắt");
    case Freeze:
        return l.frozen ? tr("Đang đóng băng") : tr("Không đóng băng");
    case Lock:
        return l.locked ? tr("Đang khóa") : tr("Không khóa");
    case Plot:
        return l.plot ? tr("In") : tr("Không in");
    case NewVpFreeze:
        return l.newVpFrozen ? tr("Đóng băng ở viewport mới") : tr("Không đóng băng ở viewport mới");
    case VpFreeze:
        return l.vpFrozen ? tr("Đóng băng ở viewport hiện hành") : tr("Không đóng băng ở viewport hiện hành");
    case Objects:
        return r.objects < 0 ? tr("Đang đếm đối tượng…") : cellText(r, column);
    default:
        return overridden(l, column) ? tr("%1 · ghi đè của viewport").arg(cellText(r, column)) : cellText(r, column);
    }
}

QVariant LayerTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= rowCount() || index.column() >= ColumnCount)
        return {};
    const LayerRow& r = row(index.row());
    const LayerInfo& l = r.entry->info;
    const int column = index.column();
    switch (role) {
    case Qt::DisplayRole: {
        QString text = cellText(r, column);
        return text.isEmpty() ? QVariant() : QVariant(std::move(text));
    }
    case Qt::ToolTipRole:
    case Qt::AccessibleTextRole:
        return cellTip(r, column);
    case Qt::TextAlignmentRole:
        return column == Status || isToggle(column) ? QVariant::fromValue(Qt::Alignment(Qt::AlignCenter))
               : isNumber(column)                   ? QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter)
                                                    : QVariant();
    case KeyRole:
        return qulonglong(r.key);
    case OverrideRole:
        return column >= VpFreeze ? QVariant(overridden(l, column)) : QVariant();
    case ValueRole:
        switch (column) {
        case Status:
            return int(statusOf(r));
        case On:
            return l.on;
        case Freeze:
            return l.frozen;
        case Lock:
            return l.locked;
        case Plot:
            return l.plot;
        case NewVpFreeze:
            return l.newVpFrozen;
        case VpFreeze:
            return l.vpFrozen;
        case Color:
            return QColor(l.color.rgb);
        case VpColor:
            return QColor(atViewport(l.vpColor, l.color).rgb);
        case Lineweight:
            return l.lineweight;
        case VpLineweight:
            return atViewport(l.vpLineweight, l.lineweight);
        case Transparency:
            return l.transparency;
        case VpTransparency:
            return atViewport(l.vpTransparency, l.transparency);
        case Objects:
            return qlonglong(r.objects);
        default:
            return cellText(r, column);
        }
    default:
        return {};
    }
}

QVariant LayerTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || section < 0 || section >= ColumnCount)
        return {};
    switch (role) {
    case Qt::DisplayRole:
        return headerLabel(section);
    case Qt::ToolTipRole:
    case Qt::AccessibleTextRole:
        return headerTip(section);
    case Qt::TextAlignmentRole: // .tbl th.ctr, th.num
        return section == Status || isToggle(section) ? QVariant::fromValue(Qt::Alignment(Qt::AlignCenter))
               : isNumber(section)                    ? QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter)
                                                      : QVariant();
    default:
        return {};
    }
}

// ------------------------------------------------------------------------------------------------- Cột

void setupColumns(ui::VirtualTableView& view)
{
    QHeaderView* h = view.header();
    view.setStateKey({}); // mặc định dưới đây không ghi đè bố cục đã lưu
    h->setSortIndicator(M::Name, Qt::AscendingOrder);
    view.setSortingEnabled(true); // mũi tên sắp chừa chỗ ở mọi đầu cột: đo rộng sau khi bật
    view.setSearchColumn(M::Name);
    h->ensurePolished();
    // Rộng mặc định theo colgroup của prototype (.tbl đầy đủ, chữ --fs-s): cột ký hiệu = biểu tượng + lề ô; cột chữ
    // theo số ký tự trung bình; cột có chữ dịch (tên màu, "Mặc định") đủ cho chữ dài nhất của ngôn ngữ đang dùng, không
    // hẹp hơn chữ đầu cột (vi-XL không cắt chữ). Ô chữ cần chữ + 2 lề --sp-4 (.tbl td); chừa thêm một --sp-4.
    const QFontMetrics cell(view.font());
    const int ch = cell.averageCharWidth();
    const int pad = 3 * px(tok::sp_4);
    const auto widest = [&](std::initializer_list<QString> texts) {
        int w = 0;
        for (const QString& t : texts)
            w = std::max(w, cell.horizontalAdvance(t));
        return w;
    };
    const int colors = widest({colorName({1, 0}), colorName({2, 0}), colorName({3, 0}), colorName({4, 0}),
                               colorName({5, 0}), colorName({6, 0}), colorName({7, 0}), colorName({0, 0xffffffffu})}) +
                       px(tok::ic) + pad + px(tok::sp_4); // ô mẫu màu và khoảng cách
    const int weights = widest({lineweightText(-3), lineweightText(211)}) + pad;
    const auto want = [&](int column) {
        switch (column) {
        case M::Name:
            return 24 * ch;
        case M::Linetype:
        case M::VpLinetype:
            return 14 * ch;
        case M::Color:
        case M::VpColor:
            return std::max(12 * ch, colors);
        case M::Lineweight:
        case M::VpLineweight:
            return std::max(12 * ch, weights);
        case M::Objects:
            return std::max(11 * ch, widest({i18n::formatCount(9'999'999)}) + pad);
        case M::Description:
            return 20 * ch;
        case M::Transparency:
        case M::VpTransparency:
        case M::PlotStyle:
        case M::VpPlotStyle:
            return 10 * ch;
        default: // Trạng thái, cột bật/tắt
            return px(tok::ic) + 2 * px(tok::sp_4);
        }
    };
    for (int c = 0; c < M::ColumnCount; ++c) {
        h->resizeSection(c, std::max(want(c), h->sectionSizeHint(c)));
        h->setSectionHidden(c, c >= M::VpFreeze);
    }
    h->setSectionsMovable(true);
    h->setFirstSectionMovable(false); // Trạng thái luôn đứng đầu (prototype: cột TT)
    view.setStateKey(u"layers"_s);
}

void showViewportColumns(ui::VirtualTableView& view, bool layout)
{
    for (int c = M::VpFreeze; c < M::ColumnCount; ++c)
        view.setColumnHidden(c, !layout);
}

} // namespace trion::layers
