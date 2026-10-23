// UIC-008 · LayerTableModel. Tiêu chí nghiệm thu (số đo thời gian 10.000 layer, 10⁶ entity:
// bench/layer_model_bench.cpp):
//   cột, vai trò     columnsAndRoles (mọi cột theo 07.2 UI-C03: chữ, chú thích bằng lời, giá trị thô, ghi đè VP),
//                    statusFromCounts (hiện hành / đang dùng / không dùng / đang đếm), currentLayer
//   3                wildcardMatches (KT-*, ?: đúng tập theo hàm tham chiếu QRegularExpression::fromWildcard),
//                    foldedContains ("tuong" khớp "TƯỜNG"), rootFilters (Tất cả, Đang dùng, từng xref)
//   4                sortEveryColumn (mọi cột × 2 chiều khớp thứ tự tham chiếu; lặp lại như nhau),
//                    countsDeterministic (mọi số phần đếm cho cùng kết quả), sortSameForAnyParts
//   2 (đúng đắn)     incrementalEvents (lô thêm/xóa/đổi = dựng lại từ đầu; không reset; giữ chọn; layer không đổi giữ
//                    entry), newestCountWins
//   quản lý cột      columnLayout (mặc định, kéo đổi thứ tự, rộng, ẩn, nhớ theo hồ sơ; cột VP theo không gian)
//   DoD-U            pseudoNoTruncation (vi-XL: đầu cột, ô không cắt chữ), summaryText, retranslateSignals
#include "app/palettes/layers/LayerTableModel.h"

#include "app/core/i18n/Translator.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/core/i18n/UiFormat.h"
#include "app/core/UiExecutor.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/views/VirtualTableView.h"
#include "core/base/sched/Scheduler.h"
#include "core/base/settings/Settings.h"
#include "core/base/text/Fold.h"

#include <QApplication>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include <algorithm>
#include <chrono>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <random>
#include <set>

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;
using namespace trion::layers;
namespace i18n = trion::i18n;
namespace sched = trion::sched;
namespace settings = trion::settings;
namespace theme = trion::theme;
using M = LayerTableModel;

namespace {

// Vòng sự kiện thật đến khi cond() hoặc quá hạn (QEventLoop + nhịp 1 ms, như các test có sẵn).
bool spinUntil(const std::function<bool()>& cond, std::chrono::milliseconds timeout = 20s)
{
    if (cond())
        return true;
    QEventLoop loop;
    QTimer tick;
    QObject::connect(&tick, &QTimer::timeout, &loop, [&] {
        if (cond())
            loop.quit();
    });
    QTimer::singleShot(timeout, &loop, &QEventLoop::quit);
    tick.start(1);
    loop.exec();
    return cond();
}

bool settle(const LayerTableModel& m)
{
    return spinUntil([&] { return !m.busy() && !m.counting(); });
}

// Dữ liệu mẫu tất định (mt19937 như nhau ở mọi nền tảng): tên tiếng Việt có dấu, ~1/13 layer phụ thuộc xref (một
// xref viết hai kiểu hoa thường), thuộc tính, ghi đè viewport ngẫu nhiên.
const QStringList kDisciplines = {u"KT"_s, u"KC"_s, u"ĐIỆN"_s, u"NƯỚC"_s, u"PCCC"_s, u"HVAC"_s};
const QStringList kElements = {u"TƯỜNG"_s,      u"CỘT"_s,  u"DẦM"_s,     u"SÀN"_s,       u"CỬA"_s,
                               u"KÍCH THƯỚC"_s, u"TRỤC"_s, u"Ghi chú"_s, u"tường bao"_s, u"Mái"_s};
const QStringList kXrefs = {u"MB-TANG1"_s, u"KC-MONG"_s, u"Mb-Tang1"_s};
const QStringList kLinetypes = {u"Continuous"_s, u"DASHED"_s, u"Center"_s, u"HIDDEN2"_s, u"dashed"_s};

LayerKey keyOf(int i)
{
    return 0x100 + LayerKey(i) * 7; // handle không liền nhau, không theo thứ tự tên
}

LayerInfo makeLayer(int i, std::mt19937& rng)
{
    LayerInfo l;
    l.key = keyOf(i);
    l.name = u"%1-%2-%3"_s.arg(kDisciplines[int(rng() % 6)], kElements[int(rng() % 10)]).arg(i);
    if (i % 13 == 5)
        l.name = kXrefs[int(rng() % 3)] + u'|' + l.name;
    l.on = rng() % 4 != 0;
    l.frozen = rng() % 5 == 0;
    l.locked = rng() % 6 == 0;
    l.plot = rng() % 7 != 0;
    l.newVpFrozen = rng() % 9 == 0;
    l.color = rng() % 5 == 0 ? LayerColor{0, qRgb(int(rng() % 256), int(rng() % 256), int(rng() % 256))}
                             : LayerColor{int(1 + rng() % 255), 0};
    l.linetype = kLinetypes[int(rng() % 5)];
    l.lineweight = rng() % 4 == 0 ? -3 : int(rng() % 212);
    l.transparency = int(rng() % 91);
    l.plotStyle = u"Color_%1"_s.arg(l.color.aci);
    if (rng() % 3 != 0)
        l.description = u"Mô tả %1"_s.arg(rng() % 100);
    l.vpFrozen = rng() % 8 == 0;
    if (rng() % 4 == 0)
        l.vpColor = LayerColor{int(1 + rng() % 255), 0};
    if (rng() % 5 == 0)
        l.vpLinetype = kLinetypes[int(rng() % 5)];
    if (rng() % 5 == 0)
        l.vpLineweight = int(rng() % 212);
    if (rng() % 6 == 0)
        l.vpTransparency = int(rng() % 91);
    if (rng() % 7 == 0)
        l.vpPlotStyle = u"Color_%1"_s.arg(rng() % 255 + 1);
    return l;
}

std::vector<LayerInfo> makeLayers(int n, std::uint32_t seed = 1)
{
    std::mt19937 rng(seed);
    std::vector<LayerInfo> out;
    out.reserve(std::size_t(n));
    for (int i = 0; i < n; ++i)
        out.push_back(makeLayer(i, rng));
    return out;
}

// Entity rải trên 3/4 số layer đầu (1/4 còn lại không dùng), kèm vài entity trỏ tới khóa không có trong bảng.
std::shared_ptr<const EntityLayers> makeEntities(const std::vector<LayerInfo>& layers, int n, std::uint32_t seed = 2)
{
    std::mt19937 rng(seed);
    auto e = std::make_shared<EntityLayers>();
    e->layerOf.reserve(std::size_t(n));
    const std::size_t used = std::max<std::size_t>(1, layers.size() * 3 / 4);
    for (int i = 0; i < n; ++i)
        e->layerOf.push_back(i % 1000 == 999 ? LayerKey(3) : layers[rng() % used].key);
    return e;
}

std::map<LayerKey, qint64> referenceCounts(const EntityLayers& e)
{
    std::map<LayerKey, qint64> out;
    for (LayerKey k : e.layerOf)
        ++out[k];
    return out;
}

std::shared_ptr<const ObjectCounts> countNow(std::shared_ptr<const EntityLayers> e, int parts)
{
    auto promise = std::make_shared<std::promise<std::shared_ptr<const ObjectCounts>>>();
    auto future = promise->get_future();
    countObjects(std::move(e), parts, {},
                 [promise](std::shared_ptr<const ObjectCounts> c) { promise->set_value(std::move(c)); });
    return future.wait_for(30s) == std::future_status::ready ? future.get() : nullptr;
}

std::vector<LayerKey> keys(const LayerTableModel& m)
{
    std::vector<LayerKey> out;
    for (int r = 0; r < m.rowCount(); ++r)
        out.push_back(m.row(r).key);
    return out;
}

std::set<LayerKey> keySet(const LayerTableModel& m)
{
    const auto k = keys(m);
    return {k.begin(), k.end()};
}

// Thứ tự tham chiếu (viết lại độc lập theo đặc tả trong LayerTableModel.h): sắp ổn định theo cột trên danh sách đã sắp
// theo (tên gấp dấu, tên, khóa) tăng dần.
struct Ref {
    const LayerInfo* l;
    qint64 objects;
    bool current;
    QString folded;
};

int three(auto a, auto b)
{
    return a < b ? -1 : b < a ? 1 : 0;
}

int refCompare(const Ref& a, const Ref& b, int column)
{
    const LayerInfo &x = *a.l, &y = *b.l;
    const auto rank = [](const Ref& r) { return r.current ? 0 : r.objects > 0 ? 1 : r.objects == 0 ? 2 : 3; };
    const auto color = [](const LayerColor& c) { return std::pair(c.aci == 0 ? 256 : c.aci, c.rgb); };
    const auto text = [](const QString& s, const QString& t) { return QString::compare(s, t, Qt::CaseInsensitive); };
    switch (column) {
    case M::Status:
        return three(rank(a), rank(b));
    case M::Name:
        if (const int c = a.folded.compare(b.folded))
            return c;
        return x.name.compare(y.name);
    case M::On:
        return three(int(!x.on), int(!y.on));
    case M::Freeze:
        return three(int(x.frozen), int(y.frozen));
    case M::Lock:
        return three(int(x.locked), int(y.locked));
    case M::Plot:
        return three(int(!x.plot), int(!y.plot));
    case M::Color:
        return three(color(x.color), color(y.color));
    case M::Linetype:
        return text(x.linetype, y.linetype);
    case M::Lineweight:
        return three(x.lineweight, y.lineweight);
    case M::Transparency:
        return three(x.transparency, y.transparency);
    case M::PlotStyle:
        return text(x.plotStyle, y.plotStyle);
    case M::NewVpFreeze:
        return three(int(x.newVpFrozen), int(y.newVpFrozen));
    case M::Description:
        return text(x.description, y.description);
    case M::Objects:
        return three(a.objects, b.objects);
    case M::VpFreeze:
        return three(int(x.vpFrozen), int(y.vpFrozen));
    case M::VpColor:
        return three(color(x.vpColor.value_or(x.color)), color(y.vpColor.value_or(y.color)));
    case M::VpLinetype:
        return text(x.vpLinetype.value_or(x.linetype), y.vpLinetype.value_or(y.linetype));
    case M::VpLineweight:
        return three(x.vpLineweight.value_or(x.lineweight), y.vpLineweight.value_or(y.lineweight));
    case M::VpTransparency:
        return three(x.vpTransparency.value_or(x.transparency), y.vpTransparency.value_or(y.transparency));
    case M::VpPlotStyle:
        return text(x.vpPlotStyle.value_or(x.plotStyle), y.vpPlotStyle.value_or(y.plotStyle));
    default:
        return 0;
    }
}

std::vector<QString> foldAll(const std::vector<LayerInfo>& layers)
{
    std::vector<QString> out;
    for (const LayerInfo& l : layers)
        out.push_back(trion::text::foldForSearch(l.name));
    return out;
}

std::vector<LayerKey> referenceOrder(const std::vector<LayerInfo>& layers, const std::vector<QString>& folded,
                                     const ObjectCounts* counts, LayerKey current, int column, Qt::SortOrder order)
{
    std::vector<Ref> refs;
    for (std::size_t i = 0; i < layers.size(); ++i)
        refs.push_back({&layers[i], counts ? counts->of(layers[i].key) : -1, layers[i].key == current, folded[i]});
    std::sort(refs.begin(), refs.end(), [](const Ref& a, const Ref& b) {
        if (const int c = a.folded.compare(b.folded))
            return c < 0;
        if (const int c = a.l->name.compare(b.l->name))
            return c < 0;
        return a.l->key < b.l->key;
    });
    std::stable_sort(refs.begin(), refs.end(), [&](const Ref& a, const Ref& b) {
        const int c = refCompare(a, b, column);
        return order == Qt::AscendingOrder ? c < 0 : c > 0;
    });
    std::vector<LayerKey> out;
    for (const Ref& r : refs)
        out.push_back(r.l->key);
    return out;
}

// Áp lô theo đúng đặc tả của LayerChange lên danh sách tham chiếu (thứ tự bảng).
void applyReference(std::vector<LayerInfo>& ref, const std::vector<LayerChange>& batch)
{
    for (const LayerChange& c : batch) {
        const auto it = std::find_if(ref.begin(), ref.end(), [&](const LayerInfo& l) { return l.key == c.layer.key; });
        if (c.kind == LayerChange::Removed) {
            if (it != ref.end())
                ref.erase(it);
        } else if (it != ref.end()) {
            *it = c.layer;
        } else {
            ref.push_back(c.layer);
        }
    }
}

// Lô ngẫu nhiên `n` sự kiện trên `ref` (thêm, đổi: bật/tắt, đổi tên, màu; xóa), gồm cả ca biên: đổi giống hệt, thêm
// trùng khóa, xóa khóa không có.
std::vector<LayerChange> randomBatch(const std::vector<LayerInfo>& ref, int n, std::mt19937& rng, LayerKey& nextKey)
{
    std::vector<LayerInfo> scratch = ref;
    std::vector<LayerChange> batch;
    for (int k = 0; k < n; ++k) {
        const int kind = int(rng() % 3);
        LayerChange c;
        if (kind == 0 || scratch.empty()) {
            c.kind = LayerChange::Added;
            c.layer = makeLayer(int(rng() % 1000), rng);
            c.layer.key = nextKey++;
            c.layer.name = u"MỚI-%1"_s.arg(c.layer.key);
        } else if (kind == 1) {
            c.kind = LayerChange::Changed;
            c.layer = scratch[rng() % scratch.size()];
            switch (rng() % 4) {
            case 0:
                c.layer.on = !c.layer.on;
                break;
            case 1:
                c.layer.name = u"ĐỔI-%1"_s.arg(c.layer.key);
                break;
            case 2:
                c.layer.color = LayerColor{int(1 + rng() % 255), 0};
                break;
            default: // giống hệt: không tạo entry mới
                break;
            }
        } else {
            c.kind = LayerChange::Removed;
            c.layer.key = rng() % 50 == 0 ? LayerKey(1) : scratch[rng() % scratch.size()].key;
        }
        applyReference(scratch, {c});
        batch.push_back(std::move(c));
    }
    return batch;
}

} // namespace

class LayerModelTest : public QObject {
    Q_OBJECT
    QTemporaryDir dir_;
    std::unique_ptr<settings::Store> store_;
    std::unique_ptr<trion::ui::UiExecutor> executor_;
    std::unique_ptr<theme::ThemeManager> tm_;
    std::unique_ptr<i18n::Translator> translator_;

private slots:
    void initTestCase()
    {
        QVERIFY(dir_.isValid());
        store_ = std::make_unique<settings::Store>(dir_.filePath(u"user.json"_s));
        settings::Store::setInstance(store_.get());
        QVERIFY(theme::loadFonts());
        executor_ = std::make_unique<trion::ui::UiExecutor>();
        tm_ = std::make_unique<theme::ThemeManager>(*store_);
        translator_ = std::make_unique<i18n::Translator>();
    }

    void cleanupTestCase()
    {
        translator_.reset();
        tm_.reset();
        settings::Store::setInstance(nullptr);
        store_.reset();
        sched::waitIdle();
        QCoreApplication::processEvents();
        executor_.reset();
    }

    // Mọi cột của 07.2 UI-C03: chữ hiện, chú thích bằng lời, căn lề, giá trị thô, ô màu, ghi đè viewport.
    void columnsAndRoles()
    {
        LayerSnapshot s;
        LayerInfo a;
        a.key = 10;
        a.name = u"KT-TƯỜNG"_s;
        a.color = {1, qRgb(255, 0, 0)};
        a.linetype = u"DASHED"_s;
        a.lineweight = 25;
        a.transparency = 40;
        a.plotStyle = u"Color_1"_s;
        a.description = u"Tường bao"_s;
        a.vpColor = LayerColor{0, qRgb(10, 20, 30)};
        a.vpLineweight = 50;
        LayerInfo b;
        b.key = 20;
        b.name = u"0"_s;
        b.on = false;
        b.frozen = true;
        b.locked = true;
        b.plot = false;
        b.newVpFrozen = true;
        b.vpFrozen = true;
        b.color = {7, qRgb(255, 255, 255)};
        b.linetype = u"Continuous"_s;
        LayerInfo c;
        c.key = 30;
        c.name = u"XR|KC-CỘT"_s;
        c.color = {0, qRgb(1, 2, 3)};
        s.layers = {a, b, c};
        s.current = 20;
        M model;
        model.setSnapshot(s);
        QVERIFY(settle(model));
        QCOMPARE(model.columnCount(), int(M::ColumnCount));
        QCOMPARE(model.layerCount(), 3);
        QCOMPARE(model.rowCount(), 3);
        for (int col = 0; col < M::ColumnCount; ++col) {
            QVERIFY2(!model.headerData(col, Qt::Horizontal).toString().isEmpty(), qPrintable(QString::number(col)));
            QVERIFY(!model.headerData(col, Qt::Horizontal, Qt::ToolTipRole).toString().isEmpty());
        }
        QVERIFY(!model.headerData(0, Qt::Vertical).isValid());

        // Chưa đếm: trạng thái Counting, ô "…", chú thích bằng lời.
        const int ra = model.rowOf(10), rb = model.rowOf(20), rc = model.rowOf(30);
        QVERIFY(ra >= 0 && rb >= 0 && rc >= 0);
        QCOMPARE(model.rowOf(99), -1);
        QCOMPARE(model.status(ra), M::LayerStatus::Counting);
        QCOMPARE(model.status(rb), M::LayerStatus::Current); // hiện hành dù chưa đếm
        QCOMPARE(model.index(ra, M::Objects).data().toString(), u"…"_s);
        QCOMPARE(model.index(ra, M::Objects).data(M::ValueRole).toLongLong(), -1);
        QCOMPARE(model.index(ra, M::Status).data(Qt::ToolTipRole).toString(),
                 LayerTableModel::tr("Đang đếm đối tượng…"));

        auto e = std::make_shared<EntityLayers>();
        e->layerOf.assign(1234, 10);
        model.setEntities(e);
        QVERIFY(model.counting());
        QVERIFY(settle(model));
        const auto at = [&](int row, int col, int role = Qt::DisplayRole) { return model.index(row, col).data(role); };
        QCOMPARE(model.status(ra), M::LayerStatus::Used);
        QCOMPARE(model.status(rb), M::LayerStatus::Current);
        QCOMPARE(model.status(rc), M::LayerStatus::Unused);
        QCOMPARE(at(ra, M::Status, Qt::ToolTipRole).toString(), LayerTableModel::tr("Đang dùng"));
        QCOMPARE(at(rb, M::Status, Qt::ToolTipRole).toString(), LayerTableModel::tr("Layer hiện hành"));
        QCOMPARE(at(rc, M::Status, Qt::ToolTipRole).toString(), LayerTableModel::tr("Không có đối tượng"));
        QCOMPARE(at(ra, M::Status, M::ValueRole).toInt(), int(M::LayerStatus::Used));
        QVERIFY(!at(ra, M::Status).isValid()); // ký hiệu do delegate vẽ, không có chữ

        QCOMPARE(at(ra, M::Name).toString(), u"KT-TƯỜNG"_s);
        QCOMPARE(at(ra, M::Objects).toString(), i18n::formatCount(1234));
        QCOMPARE(at(ra, M::Objects, M::ValueRole).toLongLong(), 1234);
        QCOMPARE(at(rb, M::Objects).toString(), i18n::formatCount(0));
        QCOMPARE(at(ra, M::Color).toString(), LayerTableModel::tr("Đỏ"));
        QCOMPARE(at(rb, M::Color).toString(), LayerTableModel::tr("Trắng"));
        QCOMPARE(at(rc, M::Color).toString(), u"1,2,3"_s);
        QCOMPARE(at(ra, M::Color, M::ValueRole).value<QColor>(), QColor(qRgb(255, 0, 0)));
        QVERIFY(!at(ra, M::Color, Qt::DecorationRole).isValid()); // ô mẫu màu do delegate của palette vẽ
        QCOMPARE(at(ra, M::Linetype).toString(), u"DASHED"_s);
        QCOMPARE(at(ra, M::Lineweight).toString(), LayerTableModel::tr("%1 mm").arg(u"0.25"_s));
        QCOMPARE(at(rb, M::Lineweight).toString(), LayerTableModel::tr("Mặc định"));
        QCOMPARE(at(ra, M::Lineweight, M::ValueRole).toInt(), 25);
        QCOMPARE(at(ra, M::Transparency).toString(), u"40"_s);
        QCOMPARE(at(ra, M::PlotStyle).toString(), u"Color_1"_s);
        QCOMPARE(at(ra, M::Description).toString(), u"Tường bao"_s);
        QCOMPARE(at(ra, M::Name, M::KeyRole).toULongLong(), 10ULL);

        // Cột bật/tắt: không chữ, giá trị thô, chú thích bằng lời (không chỉ biểu tượng).
        for (int col :
             {int(M::On), int(M::Freeze), int(M::Lock), int(M::Plot), int(M::NewVpFreeze), int(M::VpFreeze)}) {
            QVERIFY(!at(ra, col).isValid());
            QCOMPARE(at(ra, col, Qt::TextAlignmentRole).value<Qt::Alignment>(), Qt::Alignment(Qt::AlignCenter));
            QCOMPARE(at(ra, col, Qt::ToolTipRole), at(ra, col, Qt::AccessibleTextRole));
            QVERIFY(at(ra, col, Qt::ToolTipRole).toString() != at(rb, col, Qt::ToolTipRole).toString());
        }
        QCOMPARE(at(ra, M::On, M::ValueRole).toBool(), true);
        QCOMPARE(at(rb, M::On, M::ValueRole).toBool(), false);
        QCOMPARE(at(rb, M::On, Qt::ToolTipRole).toString(), LayerTableModel::tr("Đang tắt"));
        QCOMPARE(at(rb, M::Freeze, Qt::ToolTipRole).toString(), LayerTableModel::tr("Đang đóng băng"));
        QCOMPARE(at(rb, M::Lock, Qt::ToolTipRole).toString(), LayerTableModel::tr("Đang khóa"));
        QCOMPARE(at(rb, M::Plot, Qt::ToolTipRole).toString(), LayerTableModel::tr("Không in"));
        QCOMPARE(at(rb, M::NewVpFreeze, M::ValueRole).toBool(), true);
        QCOMPARE(at(rb, M::VpFreeze, M::ValueRole).toBool(), true);
        QCOMPARE(at(ra, M::Objects, Qt::TextAlignmentRole).value<Qt::Alignment>(), Qt::AlignRight | Qt::AlignVCenter);
        QVERIFY(!at(ra, M::Name, Qt::TextAlignmentRole).isValid());

        // Cột VP: giá trị hiệu lực (ghi đè nếu có), cờ ghi đè, chú thích nêu ghi đè.
        QCOMPARE(at(ra, M::VpColor).toString(), u"10,20,30"_s);
        QCOMPARE(at(ra, M::VpColor, M::OverrideRole).toBool(), true);
        QCOMPARE(at(ra, M::VpColor, M::ValueRole).value<QColor>(), QColor(qRgb(10, 20, 30)));
        QCOMPARE(at(ra, M::VpColor, Qt::ToolTipRole).toString(),
                 LayerTableModel::tr("%1 · ghi đè của viewport").arg(u"10,20,30"_s));
        QCOMPARE(at(ra, M::VpLineweight).toString(), LayerTableModel::tr("%1 mm").arg(u"0.50"_s));
        QCOMPARE(at(ra, M::VpLinetype).toString(), u"DASHED"_s); // không ghi đè: theo layer
        QCOMPARE(at(ra, M::VpLinetype, M::OverrideRole).toBool(), false);
        QCOMPARE(at(rb, M::VpColor).toString(), LayerTableModel::tr("Trắng"));
        QVERIFY(!at(ra, M::Color, M::OverrideRole).isValid());

        QCOMPARE(model.layer(rc).name, u"XR|KC-CỘT"_s);
        QCOMPARE(model.row(rc).entry->xref().toString(), u"XR"_s);
        QVERIFY(model.row(ra).entry->xref().isEmpty());
        QVERIFY(model.xrefs() == (QList<M::Xref>{{u"XR"_s, 1}}));
    }

    // CLAYER đổi: hai dòng đổi trạng thái; sắp theo Trạng thái đưa layer hiện hành lên đầu.
    void currentLayer()
    {
        const auto layers = makeLayers(300);
        M model;
        model.setSnapshot({layers, layers[5].key});
        model.setEntities(makeEntities(layers, 5000));
        model.sort(M::Status, Qt::AscendingOrder);
        QVERIFY(settle(model));
        QCOMPARE(model.row(0).key, layers[5].key);
        model.setCurrent(layers[250].key); // layer không có entity: vẫn là hiện hành
        QVERIFY(settle(model));
        QCOMPARE(model.row(0).key, layers[250].key);
        QCOMPARE(model.status(0), M::LayerStatus::Current);
        const int old = model.rowOf(layers[5].key);
        QVERIFY(model.status(old) != M::LayerStatus::Current);
        QCOMPARE(model.stats().resets, std::uint64_t(0));
    }

    // Tiêu chí 3: KT-* và ? khớp đúng tập layer theo hàm tham chiếu (regex từ wildcard trên tên gấp dấu).
    void wildcardMatches()
    {
        QVERIFY(matchWildcard(u"kt-tuong", u"kt-*"));
        QVERIFY(!matchWildcard(u"xkt-tuong", u"kt-*"));
        QVERIFY(matchWildcard(u"abc", u"a?c"));
        QVERIFY(!matchWildcard(u"ac", u"a?c"));
        QVERIFY(matchWildcard(u"", u"*"));
        QVERIFY(matchWildcard(u"", u""));
        QVERIFY(!matchWildcard(u"a", u""));
        QVERIFY(matchWildcard(u"axbyc", u"a*b*c"));
        QVERIFY(matchWildcard(u"abcbc", u"a*bc"));
        QVERIFY(matchWildcard(u"ab", u"a**b"));
        QVERIFY(matchWildcard(u"mississippi", u"m*iss*ppi"));
        QVERIFY(!matchWildcard(u"mississippi", u"m*iss*ppx"));
        QVERIFY(!matchWildcard(u"a*b", u"a\\*b"));
        QVERIFY(matchWildcard(u"a[1]", u"a[?]")); // [ ] là chữ thường, không phải tập ký tự

        const auto layers = makeLayers(10'000);
        const auto folded = foldAll(layers);
        M model;
        model.setSnapshot({layers, 0});
        const auto check = [&](const QString& query) {
            model.setFilterText(query);
            if (!settle(model))
                return false;
            const QRegularExpression re =
                QRegularExpression::fromWildcard(trion::text::foldForSearch(query.trimmed()), Qt::CaseSensitive,
                                                 QRegularExpression::NonPathWildcardConversion);
            std::set<LayerKey> want;
            for (std::size_t i = 0; i < layers.size(); ++i)
                if (re.match(folded[i]).hasMatch())
                    want.insert(layers[i].key);
            const auto got = keySet(model);
            qInfo("%s: %zu layer", qUtf8Printable(query), got.size());
            return got == want && !want.empty();
        };
        QVERIFY(check(u"KT-*"_s));
        QVERIFY(check(u"kt-*"_s));
        QVERIFY(check(u"  KT-* "_s)); // khoảng trắng hai đầu bỏ
        QVERIFY(check(u"*TƯỜNG*"_s));
        QVERIFY(check(u"KC-C?T-*"_s));
        QVERIFY(check(u"MB-TANG1|*"_s));
        QVERIFY(check(u"*-1?"_s));
        QVERIFY(check(u"đIỆN-*-9*"_s));
        model.setFilterText(u"KT-"_s); // không có * ?: chứa chuỗi (cả "MB-TANG1|KT-…")
        QVERIFY(settle(model));
        std::set<LayerKey> contains;
        for (std::size_t i = 0; i < layers.size(); ++i)
            if (folded[i].contains(u"kt-"_s))
                contains.insert(layers[i].key);
        QCOMPARE(keySet(model), contains);
        model.setFilterText(u"  "_s);
        QVERIFY(settle(model));
        QCOMPARE(model.rowCount(), 10'000);
        QCOMPARE(model.stats().resets, std::uint64_t(0));
    }

    // Tiêu chí 3: "tuong" khớp "TƯỜNG", "tường bao"; không phân biệt hoa thường, đ = d.
    void foldedContains()
    {
        const auto layers = makeLayers(2'000);
        M model;
        model.setSnapshot({layers, 0});
        for (const QString& q : {u"tuong"_s, u"TUONG"_s, u"Tường"_s, u"dien"_s, u"ghi chu"_s, u"kich thuoc-1"_s}) {
            model.setFilterText(q);
            QVERIFY(settle(model));
            const QString needle = trion::text::foldForSearch(q);
            std::set<LayerKey> want;
            for (const LayerInfo& l : layers)
                if (trion::text::foldForSearch(l.name).contains(needle))
                    want.insert(l.key);
            QVERIFY2(!want.empty(), qPrintable(q));
            QCOMPARE(keySet(model), want);
        }
        model.setFilterText(u"tuong"_s);
        QVERIFY(settle(model));
        bool upper = false, lower = false;
        for (int r = 0; r < model.rowCount(); ++r) {
            upper |= model.layer(r).name.contains(u"TƯỜNG"_s);
            lower |= model.layer(r).name.contains(u"tường bao"_s);
        }
        QVERIFY(upper && lower);
        QCOMPARE(model.filterText(), u"tuong"_s);
    }

    // Bộ lọc gốc: Tất cả, Đang dùng (có đối tượng), từng xref (không phân biệt hoa thường); kết hợp với ô tìm.
    void rootFilters()
    {
        const auto layers = makeLayers(3'000);
        const auto entities = makeEntities(layers, 50'000);
        const auto counts = referenceCounts(*entities);
        M model;
        model.setSnapshot({layers, 0});
        model.setRoot(M::Root::Used);
        QVERIFY(settle(model));
        QCOMPARE(model.rowCount(), 0); // chưa đếm: chưa biết layer nào đang dùng
        model.setEntities(entities);
        QVERIFY(settle(model));
        std::set<LayerKey> used, mb, all;
        for (const LayerInfo& l : layers) {
            all.insert(l.key);
            if (counts.count(l.key))
                used.insert(l.key);
            if (l.name.startsWith(u"MB-TANG1|"_s, Qt::CaseInsensitive))
                mb.insert(l.key);
        }
        QVERIFY(!used.empty() && used.size() < all.size());
        QCOMPARE(keySet(model), used);
        QCOMPARE(model.root(), M::Root::Used);

        model.setRoot(M::Root::Xref, u"mb-tang1"_s);
        QVERIFY(settle(model));
        QVERIFY(!mb.empty());
        QCOMPARE(keySet(model), mb);
        model.setFilterText(u"*CỘT*"_s);
        QVERIFY(settle(model));
        std::set<LayerKey> mbCot;
        for (LayerKey k : mb)
            if (layers[std::size_t((k - 0x100) / 7)].name.contains(u"CỘT"_s))
                mbCot.insert(k);
        QCOMPARE(keySet(model), mbCot);
        model.setFilterText({});
        model.setRoot(M::Root::Xref, {});
        QVERIFY(settle(model));
        QCOMPARE(model.rowCount(), 0);
        model.setRoot(M::Root::All);
        QVERIFY(settle(model));
        QCOMPARE(keySet(model), all);

        // Cây bộ lọc: mỗi xref một nút có số layer; "Mb-Tang1" cùng nút với "MB-TANG1".
        std::map<QString, int> want;
        for (const LayerInfo& l : layers)
            if (const qsizetype bar = l.name.indexOf(u'|'); bar > 0)
                ++want[l.name.left(bar).toCaseFolded()];
        const auto xrefs = model.xrefs();
        QCOMPARE(xrefs.size(), qsizetype(want.size()));
        QCOMPARE(want.size(), std::size_t(2));
        auto it = want.begin();
        for (const M::Xref& x : xrefs) {
            QCOMPARE(x.name.toCaseFolded(), it->first);
            QCOMPARE(x.layers, it->second);
            ++it;
        }
    }

    // Tiêu chí 4: mọi cột × hai chiều khớp thứ tự tham chiếu; sắp lại lần nữa cho đúng dãy cũ; cột -1: thứ tự bảng.
    void sortEveryColumn()
    {
        const auto layers = makeLayers(3'000, 7);
        const auto entities = makeEntities(layers, 40'000, 8);
        M model;
        model.setSnapshot({layers, layers[42].key});
        model.setEntities(entities);
        QVERIFY(settle(model));
        const auto counts = countNow(entities, 1);
        QVERIFY(counts);
        const auto folded = foldAll(layers);
        std::map<std::pair<int, int>, std::vector<LayerKey>> first;
        for (int round = 0; round < 2; ++round) { // vòng sau: sắp lại cho đúng dãy của vòng đầu
            for (int col = 0; col < M::ColumnCount; ++col) {
                for (Qt::SortOrder order : {Qt::AscendingOrder, Qt::DescendingOrder}) {
                    model.sort(col, order);
                    QVERIFY(settle(model));
                    QCOMPARE(model.sortColumn(), col);
                    QCOMPARE(model.sortOrder(), order);
                    const auto got = keys(model);
                    const std::pair<int, int> key(col, int(order));
                    if (round == 0) {
                        QVERIFY2(got == referenceOrder(layers, folded, counts.get(), layers[42].key, col, order),
                                 qPrintable(u"cột %1, chiều %2"_s.arg(col).arg(int(order))));
                        first[key] = got;
                    } else {
                        QVERIFY(first[key] == got);
                    }
                }
            }
        }
        model.sort(-1);
        QVERIFY(settle(model));
        QCOMPARE(model.sortColumn(), -1);
        std::vector<LayerKey> table;
        for (const LayerInfo& l : layers)
            table.push_back(l.key);
        QVERIFY(keys(model) == table);
        QCOMPARE(model.stats().resets, std::uint64_t(0));
    }

    // Tiêu chí 4 (đếm): mọi số phần cho cùng kết quả, đúng với đếm tuần tự; entity trỏ tới khóa lạ vẫn được đếm theo
    // khóa.
    void countsDeterministic()
    {
        const auto layers = makeLayers(5'000);
        const auto entities = makeEntities(layers, 300'000, 11);
        const auto want = referenceCounts(*entities);
        std::vector<std::pair<LayerKey, qint64>> ref(want.begin(), want.end());
        for (int parts : {1, 2, 3, 4, 7, 8, 13, 16, 64}) {
            const auto got = countNow(entities, parts);
            QVERIFY(got);
            QVERIFY2(got->byKey == ref, qPrintable(QString::number(parts)));
        }
        const auto c = countNow(entities, 8);
        QCOMPARE(c->of(LayerKey(3)), qint64(300)); // khóa không có trong bảng layer
        QCOMPARE(c->of(LayerKey(4)), qint64(0));
        const auto empty = countNow(std::make_shared<EntityLayers>(), 8);
        QVERIFY(empty && empty->byKey.empty());
        const auto none = countNow(nullptr, 3);
        QVERIFY(none && none->byKey.empty());

        // Đã hủy: không phần nào gọi done.
        sched::CancelSource cancel;
        cancel.cancel();
        auto called = std::make_shared<std::atomic<bool>>(false);
        countObjects(entities, 8, cancel.token(),
                     [called](std::shared_ptr<const ObjectCounts>) { called->store(true); });
        sched::waitIdle();
        QVERIFY(!called->load());
    }

    // Tiêu chí 4: thứ tự theo Số đối tượng, Trạng thái như nhau với mọi số phần đếm.
    void sortSameForAnyParts()
    {
        const auto layers = makeLayers(4'000, 3);
        const auto entities = makeEntities(layers, 200'000, 4);
        std::vector<LayerKey> reference[2];
        for (int parts : {1, 2, 5, 8, 16}) {
            int k = 0;
            for (int col : {int(M::Objects), int(M::Status)}) {
                M model;
                model.setCountParts(parts);
                model.setSnapshot({layers, layers[7].key});
                model.setEntities(entities);
                model.sort(col, Qt::DescendingOrder);
                QVERIFY(settle(model));
                if (reference[k].empty())
                    reference[k] = keys(model);
                else
                    QVERIFY2(keys(model) == reference[k], qPrintable(u"%1 phần, cột %2"_s.arg(parts).arg(col)));
                ++k;
            }
        }
    }

    // Tiêu chí 2 (đúng đắn của áp gia tăng): sau mỗi lô ngẫu nhiên, model như dựng lại từ đầu với bảng tham chiếu;
    // không reset; dòng đang chọn còn tồn tại vẫn được chọn; layer không đổi giữ entry (không dataChanged thừa).
    void incrementalEvents()
    {
        auto ref = makeLayers(2'000, 21);
        const auto entities = makeEntities(ref, 20'000, 22);
        const LayerKey current = ref[3].key;
        M model;
        model.setSnapshot({ref, current});
        model.setEntities(entities);
        QVERIFY(settle(model));
        QItemSelectionModel selection(&model);
        std::mt19937 rng(23);
        LayerKey nextKey = 1'000'000;
        for (int batchNo = 0; batchNo < 6; ++batchNo) {
            for (int r = 0; r < model.rowCount(); r += 37)
                selection.select(model.index(r, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
            std::set<LayerKey> selected;
            for (const QModelIndex& i : selection.selectedRows())
                selected.insert(model.row(i.row()).key);
            std::vector<LayerChange> batch;
            if (batchNo == 3) { // xóa nửa bảng: quá 1/4 thì dồn chỗ trống, đánh số lại; đổi vài layer cuối bảng
                for (std::size_t i = 0; i < ref.size(); i += 2)
                    batch.push_back({LayerChange::Removed, ref[i]});
                for (std::size_t i = ref.size() - 1; i + 40 > ref.size(); i -= 3) {
                    LayerInfo l = ref[i];
                    l.on = !l.on;
                    batch.push_back({LayerChange::Changed, l});
                }
            } else {
                batch = randomBatch(ref, batchNo == 5 ? 1'000 : 200, rng, nextKey);
            }
            std::map<LayerKey, const LayerEntry*> before;
            for (int r = 0; r < model.rowCount(); ++r)
                before[model.row(r).key] = model.row(r).entry.get();
            applyReference(ref, batch);
            model.apply(batch);
            QVERIFY(settle(model));

            M fresh;
            fresh.setSnapshot({ref, current});
            fresh.setEntities(entities);
            QVERIFY(settle(fresh));
            QCOMPARE(model.layerCount(), int(ref.size()));
            QCOMPARE(model.rowCount(), fresh.rowCount());
            for (int r = 0; r < model.rowCount(); ++r) {
                QCOMPARE(model.row(r).key, fresh.row(r).key);
                QVERIFY(model.layer(r) == fresh.layer(r));
                QCOMPARE(model.row(r).objects, fresh.row(r).objects);
            }
            QCOMPARE(model.stats().resets, std::uint64_t(0));

            std::set<LayerKey> alive, changed;
            for (const LayerInfo& l : ref)
                alive.insert(l.key);
            for (const LayerChange& c : batch)
                changed.insert(c.layer.key);
            std::set<LayerKey> want;
            for (LayerKey k : selected)
                if (alive.count(k))
                    want.insert(k);
            std::set<LayerKey> now;
            for (const QModelIndex& i : selection.selectedRows())
                now.insert(model.row(i.row()).key);
            QCOMPARE(now, want);
            for (int r = 0; r < model.rowCount(); ++r)
                if (const LayerKey k = model.row(r).key; !changed.count(k))
                    QCOMPARE(model.row(r).entry.get(), before[k]); // không nằm trong lô: cùng entry
        }
        // Đổi giống hệt: giữ entry; thêm trùng khóa: coi như đổi; xóa khóa không có: bỏ qua.
        const LayerInfo same = model.layer(0);
        const LayerEntry* entry = model.row(0).entry.get();
        LayerInfo renamed = same;
        renamed.name = u"TÊN MỚI"_s;
        LayerInfo missing;
        missing.key = 424242;
        model.apply({{LayerChange::Changed, same}, {LayerChange::Removed, missing}});
        QVERIFY(settle(model));
        QCOMPARE(model.row(model.rowOf(same.key)).entry.get(), entry);
        const int count = model.layerCount();
        model.apply({{LayerChange::Added, renamed}});
        QVERIFY(settle(model));
        QCOMPARE(model.layerCount(), count);
        QCOMPARE(model.layer(model.rowOf(same.key)).name, u"TÊN MỚI"_s);
    }

    // Đếm mới thắng: kết quả của lần đếm cũ về sau bị bỏ; bỏ nguồn thì về "đang đếm".
    void newestCountWins()
    {
        const auto layers = makeLayers(500);
        M model;
        model.setSnapshot({layers, 0});
        auto big = makeEntities(layers, 400'000, 31);
        auto small = std::make_shared<EntityLayers>();
        small->layerOf.assign(5, layers[0].key);
        QSignalSpy spy(&model, &M::countsChanged);
        model.setEntities(big);
        model.setEntities(small);
        QVERIFY(settle(model));
        sched::waitIdle();
        QCoreApplication::processEvents();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(model.row(model.rowOf(layers[0].key)).objects, qint64(5));
        QCOMPARE(model.row(model.rowOf(layers[1].key)).objects, qint64(0));
        model.setEntities(nullptr);
        QVERIFY(settle(model));
        QCOMPARE(spy.count(), 2);
        QCOMPARE(model.status(0), M::LayerStatus::Counting);
    }

    // Quản lý cột: mặc định (cột VP ẩn, rộng không hẹp hơn chữ đầu cột, kéo được, Trạng thái cố định đầu, sắp theo
    // Tên); người dùng kéo đổi thứ tự, đổi rộng, ẩn → view mới nạp lại từ hồ sơ (Store "views/layers"); cột VP theo
    // không gian không đổi rộng cột khác.
    void columnLayout()
    {
        store_->remove(u"views/layers"_s);
        M model;
        const auto layers = makeLayers(50);
        model.setSnapshot({layers, 0});
        QVERIFY(settle(model));
        {
            trion::ui::VirtualTableView view;
            view.setModel(&model);
            setupColumns(view);
            QHeaderView* h = view.header();
            for (int c = 0; c < M::ColumnCount; ++c) {
                QCOMPARE(h->isSectionHidden(c), c >= M::VpFreeze);
                if (!h->isSectionHidden(c))
                    QVERIFY2(h->sectionSize(c) >= h->sectionSizeHint(c), qPrintable(QString::number(c)));
            }
            QVERIFY(h->sectionsMovable());
            QVERIFY(!h->isFirstSectionMovable());
            QCOMPARE(h->sortIndicatorSection(), int(M::Name));
            QCOMPARE(view.stateKey(), u"layers"_s);
            QCOMPARE(view.searchColumn(), int(M::Name));
            QVERIFY(view.isSortingEnabled());

            h->moveSection(h->visualIndex(M::Color), 2); // như kéo đầu cột
            h->resizeSection(M::Name, 300);
            view.setColumnHidden(M::Description, true);
            h->setSortIndicator(M::Objects, Qt::DescendingOrder);
            QCOMPARE(model.sortColumn(), int(M::Objects));
            showViewportColumns(view, true);
            for (int c = M::VpFreeze; c < M::ColumnCount; ++c)
                QVERIFY(!h->isSectionHidden(c));
        }
        QVERIFY(store_->value(u"views/layers"_s).isString());
        trion::ui::VirtualTableView again;
        again.setModel(&model);
        setupColumns(again);
        QHeaderView* h = again.header();
        QCOMPARE(h->visualIndex(M::Color), 2);
        QCOMPARE(h->sectionSize(M::Name), 300);
        QVERIFY(h->isSectionHidden(M::Description));
        QCOMPARE(h->sortIndicatorSection(), int(M::Objects));
        QCOMPARE(h->sortIndicatorOrder(), Qt::DescendingOrder);
        for (int c = M::VpFreeze; c < M::ColumnCount; ++c)
            QVERIFY(!h->isSectionHidden(c));
        const int color = h->sectionSize(M::Color);
        showViewportColumns(again, false); // về Model: cột VP ẩn ngay, cột khác giữ rộng
        for (int c = M::VpFreeze; c < M::ColumnCount; ++c)
            QVERIFY(h->isSectionHidden(c));
        QCOMPARE(h->sectionSize(M::Name), 300);
        QCOMPARE(h->sectionSize(M::Color), color);
        QVERIFY(settle(model));
        store_->remove(u"views/layers"_s);
    }

    // DoD-U: ngôn ngữ giả vi-XL (chuỗi dài hơn 40%), bảng đủ cột: đầu cột và ô đang nhìn không cắt chữ.
    void pseudoNoTruncation()
    {
        QVERIFY(translator_->setLanguage(i18n::kPseudoLanguage.toString()));
        QCoreApplication::processEvents();
        store_->remove(u"views/layers"_s);
        std::vector<LayerInfo> layers;
        for (int i = 0; i < 8; ++i) {
            LayerInfo l;
            l.key = LayerKey(i + 1);
            l.name = u"KT-TƯỜNG-%1"_s.arg(i);
            l.color = {i == 0 ? 0 : i, qRgb(200, 100, 50)};
            l.linetype = u"Continuous"_s;
            l.lineweight = i % 2 ? -3 : 211;
            l.transparency = 90;
            l.plotStyle = u"Color_%1"_s.arg(i);
            l.description = u"Mô tả %1"_s.arg(i);
            l.vpColor = LayerColor{5, qRgb(0, 0, 255)};
            l.vpLineweight = -3;
            layers.push_back(l);
        }
        auto e = std::make_shared<EntityLayers>();
        e->layerOf.assign(1'234'567, 1);
        M model;
        model.setSnapshot({layers, 2});
        model.setEntities(e);
        QVERIFY(settle(model));
        trion::ui::VirtualTableView view;
        view.setModel(&model);
        setupColumns(view);
        showViewportColumns(view, true);
        int width = 0;
        for (int c = 0; c < M::ColumnCount; ++c)
            width += view.header()->sectionSize(c);
        view.resize(width + 40, 400);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        const auto cut = i18n::findTruncations(&view);
        for (const auto& t : cut)
            qWarning("%s", qUtf8Printable(t.toString()));
        QVERIFY(cut.isEmpty());
        QVERIFY(translator_->setLanguage(u"vi"_s));
        QCoreApplication::processEvents();
        store_->remove(u"views/layers"_s);
    }

    // Chân palette "Hiện 48/312 layer · 3 đang chọn".
    void summaryText()
    {
        const auto layers = makeLayers(1'500);
        M model;
        model.setSnapshot({layers, 0});
        model.setFilterText(u"KT-*"_s);
        QVERIFY(settle(model));
        const QString shown = i18n::formatCount(model.rowCount()), total = i18n::formatCount(1'500);
        QVERIFY(model.rowCount() > 0 && model.rowCount() < 1'500);
        QCOMPARE(model.summary(3), LayerTableModel::tr("Hiện %1/%2 layer · %3 đang chọn").arg(shown, total, u"3"_s));
        QCOMPARE(model.summary(0), LayerTableModel::tr("Hiện %1/%2 layer").arg(shown, total));
        QVERIFY(model.summary(0).contains(u"1.500"_s));
    }

    // Đổi ngôn ngữ: palette gọi retranslate → đầu cột và ô báo đổi (view vẽ lại chữ đã dịch).
    void retranslateSignals()
    {
        M model;
        model.setSnapshot({makeLayers(20), 0});
        QVERIFY(settle(model));
        QSignalSpy header(&model, &QAbstractItemModel::headerDataChanged);
        QSignalSpy cells(&model, &QAbstractItemModel::dataChanged);
        model.retranslate();
        QCOMPARE(header.count(), 1);
        QCOMPARE(header.first().at(1).toInt(), 0);
        QCOMPARE(header.first().at(2).toInt(), int(M::ColumnCount) - 1);
        QCOMPARE(cells.count(), 1);
        QCOMPARE(cells.first().at(1).value<QModelIndex>().row(), model.rowCount() - 1);
    }
};

QTEST_MAIN(LayerModelTest)
#include "layer_model_test.moc"
