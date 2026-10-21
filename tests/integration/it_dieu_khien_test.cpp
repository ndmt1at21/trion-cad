// Integration test nhóm 3: INT-105 (parser giá trị), UI0-014 (model nền), UI0-012 (High-DPI), UI0-106 (điều khiển),
// UI0-109 (parser ô số) cùng executor, chủ đề, bản địa hóa, biểu tượng, đo độ trễ của nhóm 1, 2 trên một QApplication
// có hai màn hình giả 100% và 150% (plugin offscreen, như test của UI0-012).

#include "app/core/display/DisplayContext.h"
#include "app/core/i18n/Translator.h"
#include "app/core/perf/UiProbe.h"
#include "app/core/UiExecutor.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/controls/Controls.h"
#include "app/widgets/model/BackgroundModel.h"
#include "app/widgets/numeric/ValueParser.h"
#include "core/base/sched/Scheduler.h"
#include "core/base/settings/Settings.h"
#include "core/base/text/Fold.h"
#include "core/regen/numfmt/NumFormat.h"
#include "tests/timing.h"

#include <QApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListView>
#include <QScreen>
#include <QScrollBar>
#include <QSignalSpy>
#include <QStyledItemDelegate>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <bit>
#include <memory>
#include <numeric>
#include <thread>
#include <vector>

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;
using trion::settings::Store;
using trion::theme::Density;
using trion::theme::IconProvider;
using trion::theme::Theme;
using trion::theme::ThemeManager;
using trion::ui::DisplayContext;
namespace i18n = trion::i18n;
namespace probe = trion::ui::probe;
namespace regen = trion::regen;
namespace ui = trion::ui;

namespace {

// Màn hình giả: tỉ lệ = logicalDpi / 96. Qt giữ gốc toạ độ thiết bị làm gốc logic.
struct Screen {
    const char* name;
    int x, dpi;
};
constexpr Screen kScreens[] = {{"p100", 0, 96}, {"p150", 2000, 144}};
QString g_cwd; // thư mục chạy test; initMain đổi sang thư mục chứa cấu hình màn hình tới khi tạo lớp test

QScreen* screenNamed(const char* name)
{
    for (QScreen* s : QGuiApplication::screens())
        if (s->name() == QLatin1StringView(name))
            return s;
    return nullptr;
}

// Như kéo cửa sổ: tâm sang màn hình kia thì Qt đổi màn hình và DPR, kích thước logic giữ nguyên.
bool moveTo(QWidget& w, const char* name)
{
    QScreen* s = screenNamed(name);
    w.move(s->geometry().topLeft() + QPoint(40, 40));
    return QTest::qWaitFor([&] { return w.screen() == s; });
}

// Việc nền xong và kết quả gửi về (ui::post) đã chạy: số đo của IconProvider không đổi giữa chừng một bước kiểm.
bool settle()
{
    trion::sched::waitIdle();
    bool done = false;
    ui::post([&done] { done = true; });
    return QTest::qWaitFor([&] { return done; }, 10000);
}

// n biểu tượng ngoài bộ Viewer: không được raster trước, phải raster khi vẽ ở DPR mới.
QStringList nonViewer(int n)
{
    const QStringList viewer = IconProvider::names(true);
    QStringList out;
    for (const QString& name : IconProvider::names())
        if (!viewer.contains(name) && out.size() < n)
            out << name;
    return out;
}

// Danh sách layer của model nền. Bản chụp (thay DOC-009): khóa theo thứ tự hiển thị; renamed: khóa chia hết cho 7
// mang tên mới. Tên lặp theo chu kỳ 5 của khóa; 2/5 số tên có "Lớp"/"LỚP".
struct Layers {
    std::vector<std::uint64_t> keys;
    bool renamed = false;
};

struct LayerRow {
    std::uint64_t key;
    QString name;
    bool operator==(const LayerRow&) const = default;
};

class LayerModel : public ui::BackgroundModel<LayerRow> {
public:
    QVariant data(const QModelIndex& i, int role) const override
    {
        return i.isValid() && role == Qt::DisplayRole ? QVariant(row(i.row()).name) : QVariant();
    }
};

// Dòng cao theo --h-row như danh sách của palette: đổi mật độ thì dòng đổi cao (22 ↔ 26 px).
class RowDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem& o, const QModelIndex& i) const override
    {
        return {QStyledItemDelegate::sizeHint(o, i).width(), qRound(ThemeManager::instance().px(tok::h_row))};
    }
};

QString layerName(std::uint64_t key, bool renamed)
{
    static constexpr QStringView kWords[] = {u"Lớp tường", u"KT-CỬA", u"ĐƯỜNG trục", u"LỚP hoàn thiện", u"Kích thước"};
    return u"%1 %2%3"_s.arg(kWords[key % 5])
        .arg(key, 6, 10, QChar(u'0'))
        .arg(renamed && key % 7 == 0 ? u" sửa"_s : QString());
}

// Hàm dựng, chạy ở worker.
std::vector<LayerRow> buildRows(const Layers& s, const trion::sched::CancelToken& cancel)
{
    std::vector<LayerRow> rows;
    rows.reserve(s.keys.size());
    for (const std::uint64_t k : s.keys) {
        if (rows.size() % 1024 == 0 && cancel.cancelled())
            return {};
        rows.push_back({k, layerName(k, s.renamed)});
    }
    return rows;
}

// refresh rồi chạy vòng sự kiện tới khi model hiện đủ version.
bool refreshAndSettle(LayerModel& m, std::shared_ptr<const Layers> s, quint64 version)
{
    bool done = false;
    const auto c = QObject::connect(&m, &LayerModel::settled, [&](quint64 v) { done = done || v == version; });
    m.refresh(std::move(s), version, &buildRows);
    const bool ok = QTest::qWaitFor([&] { return done; }, 30000);
    QObject::disconnect(c);
    return ok;
}

// Số layer của bản chụp có tên khớp chuỗi lọc đã gấp dấu.
int matching(const Layers& s, const QString& needle)
{
    return int(std::count_if(s.keys.begin(), s.keys.end(), [&](std::uint64_t k) {
        return trion::text::foldForSearch(layerName(k, s.renamed)).contains(needle);
    }));
}

// Đọc (UI0-109 qua INT-105) rồi hiển thị lại (RGN-013) một bộ chuỗi gõ ở mọi LUNITS, AUNITS: "chuỗi → mã lỗi, giá
// trị từng bit, chuỗi hiển thị". Chuỗi hiển thị gõ lại phải ra chính nó (ô giữ chuỗi đã hiển thị; Enter không đổi gì).
QStringList readBack()
{
    using ui::ValueKind;
    struct Typed {
        ValueKind kind;
        const char* text;
    };
    static constexpr Typed kTyped[] = {
        {ValueKind::Distance, "12.5"},  {ValueKind::Distance, "=1200/2+50"}, {ValueKind::Distance, "3'6\""},
        {ValueKind::Distance, "250cm"}, {ValueKind::Distance, "1/4"},        {ValueKind::Distance, "12,5"},
        {ValueKind::Angle, "45"},       {ValueKind::Angle, "45d30'"},        {ValueKind::Angle, "50g"},
        {ValueKind::Angle, "0.785r"},   {ValueKind::Angle, "N45dE"},         {ValueKind::Angle, "12,5"},
        {ValueKind::Scale, "1:50"},     {ValueKind::Integer, "12.5"},        {ValueKind::Integer, "-32768"},
        {ValueKind::Real, "=1e3/7"},    {ValueKind::Real, "abc"},
    };
    QStringList out;
    char shown[regen::kMaxNumText], again[regen::kMaxNumText];
    for (int u = 0; u < 5; ++u)
        for (const Typed& t : kTyped) {
            ui::ValueSpec s;
            s.kind = t.kind;
            s.units.lunits = regen::LinearUnits(u + 1);
            s.units.aunits = regen::AngularUnits(u);
            s.units.auprec = 4;
            s.insunits = 4; // mm
            const ui::ValueResult r = ui::parseValue(t.text, s);
            const std::string_view text(shown, r ? ui::formatValue(r.value, s, shown) : 0);
            const ui::ValueResult r2 = ui::parseValue(text, s);
            const std::string_view text2(again, r2 ? ui::formatValue(r2.value, s, again) : 0);
            const auto str = [](std::string_view v) { return QString::fromUtf8(v.data(), qsizetype(v.size())); };
            out << QStringList{QString::number(u), QString::number(int(t.kind)), str(t.text),
                               str(ui::errorCode(r.error)), QString::number(std::bit_cast<quint64>(r.value), 16),
                               str(text), r && text2 != text ? u"gõ lại: "_s + str(text2) : QString()}
                       .join(u' ');
        }
    return out;
}

} // namespace

class IntegrationDieuKhien : public QObject {
    Q_OBJECT
public:
    // Sau QApplication (plugin đã đọc cấu hình màn hình), trước qExec: về lại thư mục chạy test.
    IntegrationDieuKhien() { QDir::setCurrent(g_cwd); }

    // Trước QApplication (QTEST_MAIN): bật lại co giãn DPI (môi trường test chung tắt), hệ số lẻ không làm tròn
    // (UI0-012), hai màn hình giả.
    static void initMain()
    {
        for (const char* v : {"QT_ENABLE_HIGHDPI_SCALING", "QT_SCALE_FACTOR", "QT_SCREEN_SCALE_FACTORS",
                              "QT_SCALE_FACTOR_ROUNDING_POLICY"})
            qunsetenv(v);
        DisplayContext::initMain();
        QJsonArray screens;
        for (const Screen& s : kScreens)
            screens.append(QJsonObject{{u"name"_s, QLatin1StringView(s.name)},
                                       {u"x"_s, s.x},
                                       {u"y"_s, 0},
                                       {u"width"_s, 1920},
                                       {u"height"_s, 1080},
                                       {u"logicalDpi"_s, s.dpi},
                                       {u"logicalBaseDpi"_s, 96},
                                       {u"dpr"_s, 1}});
        // Đường dẫn tương đối: tùy chọn của plugin ngăn cách bằng ':', đường dẫn Windows "C:/…" có ':'.
        static QTemporaryDir config;
        g_cwd = QDir::currentPath();
        QFile f(config.filePath(u"screens.json"_s));
        const QByteArray json = QJsonDocument(QJsonObject{{u"screens"_s, screens}}).toJson();
        if (!config.isValid() || !f.open(QIODevice::WriteOnly) || f.write(json) != json.size() ||
            !QDir::setCurrent(config.path()))
            qFatal("không ghi được cấu hình màn hình");
        qputenv("QT_QPA_PLATFORM", "offscreen:configfile=screens.json");
    }

private slots:
    void initTestCase()
    {
        QCOMPARE(QGuiApplication::screens().size(), qsizetype(std::size(kScreens)));
        QCOMPARE(screenNamed("p150")->devicePixelRatio(), 1.5);
        QVERIFY(dir_.isValid());
        // Linux: "giảm chuyển động" của hệ điều hành (DisplayContext) đọc $XDG_CONFIG_HOME, không lấy của máy chạy test.
        QVERIFY(QDir(dir_.path()).mkpath(u"config"_s));
        qputenv("XDG_CONFIG_HOME", QFile::encodeName(dir_.filePath(u"config"_s)));
        store_ = std::make_unique<Store>(dir_.filePath(u"user.json"_s));
        Store::setInstance(store_.get());
        executor_ = std::make_unique<ui::UiExecutor>();
        translator_ = std::make_unique<i18n::Translator>();
        QVERIFY(trion::theme::loadFonts());
        theme_ = std::make_unique<ThemeManager>(*store_);
        icons_ = std::make_unique<IconProvider>();
        QCOMPARE(theme_->theme(), Theme::Dark);
        QCOMPARE(theme_->density(), Density::Compact);
        QCOMPARE(translator_->language(), u"vi"_s);
    }

    void cleanupTestCase()
    {
        probe::disable();
        trion::sched::waitIdle();
        icons_.reset();
        theme_.reset();
        translator_.reset();
        executor_.reset();
        Store::setInstance(nullptr);
        store_.reset();
    }

    // ComboBox (UI0-106) trên model nền (UI0-014). Unit test của ComboBox dùng model chuẩn đổi đồng bộ, của model nền
    // dùng view chuẩn. Ở đây: mục đang chọn đi theo khóa qua xóa, hoán vị, chèn, đổi tên áp theo lô (chữ không lúc
    // nào đổi, không reset); model đổi khi popup đang mở và đang lọc thì danh sách lọc vẫn đúng.
    void comboOverBackgroundModel()
    {
        probe::enable();
        probe::reset();
        LayerModel model;
        QWidget win;
        auto* combo = new ui::ComboBox(&win);
        combo->setModel(&model);
        auto v1 = std::make_shared<Layers>();
        v1->keys.resize(10'000);
        std::iota(v1->keys.begin(), v1->keys.end(), std::uint64_t(0));
        QVERIFY(refreshAndSettle(model, v1, 1));
        QCOMPARE(combo->count(), 10'000);
        combo->setCurrentIndex(7775);
        const QString chosen = layerName(7775, false);
        QCOMPARE(combo->currentText(), chosen);
        combo->resize(240, combo->sizeHint().height());
        win.resize(400, 300);
        win.show();
        QVERIFY(QTest::qWaitForWindowActive(&win));

        // v2: xóa khóa 0…999, đảo thứ tự phần còn lại (hoán vị), chèn 2.000 layer đầu danh sách, đổi tên khóa chia
        // hết cho 7 (7775 không đổi tên).
        QSignalSpy textSpy(combo, &QComboBox::currentTextChanged);
        auto v2 = std::make_shared<Layers>();
        for (std::uint64_t k = 10'000; k < 12'000; ++k)
            v2->keys.push_back(k);
        for (std::uint64_t k = 9'999; k >= 1'000; --k)
            v2->keys.push_back(k);
        v2->renamed = true;
        QVERIFY(refreshAndSettle(model, v2, 2));
        QCOMPARE(combo->count(), 11'000);
        QCOMPARE(combo->currentIndex(), 2'000 + 9'999 - 7'775);
        QCOMPARE(combo->currentText(), chosen);
        QCOMPARE(textSpy.count(), 0);
        QCOMPARE(combo->itemText(2'000 + 9'999 - 7'000), layerName(7'000, true));
        QCOMPARE(model.stats().resets, 0u);

        combo->showPopup();
        QWidget* popup = QApplication::activePopupWidget();
        QVERIFY(popup && popup->parentWidget() == combo);
        auto* edit = popup->findChild<ui::SearchField*>();
        auto* list = popup->findChild<QListView*>();
        QVERIFY(edit && list);
        QCOMPARE(list->currentIndex().row(), combo->currentIndex()); // mở ở mục đang chọn
        const QString needle = trion::text::foldForSearch(u"lop");
        const auto mismatches = [&] { // dòng của popup không khớp chuỗi lọc
            int n = 0;
            for (int i = 0; i < list->model()->rowCount(); ++i)
                n += !trion::text::foldForSearch(list->model()->index(i, 0).data().toString()).contains(needle);
            return n;
        };
        QTest::keyClicks(edit, u"lop"_s);
        QCOMPARE(list->model()->rowCount(), matching(*v2, needle));
        QCOMPARE(mismatches(), 0);

        // v3 khi popup đang mở với chuỗi lọc: 1.001 layer mới đầu danh sách (lệch không chia hết cho chu kỳ tên).
        auto v3 = std::make_shared<Layers>(*v2);
        std::vector<std::uint64_t> fresh(1'001);
        std::iota(fresh.begin(), fresh.end(), std::uint64_t(20'000));
        v3->keys.insert(v3->keys.begin(), fresh.begin(), fresh.end());
        QVERIFY(refreshAndSettle(model, v3, 3));
        QVERIFY(popup->isVisible());
        QCOMPARE(edit->text(), u"lop"_s);
        QCOMPARE(combo->currentText(), chosen);
        // Model nguồn đổi lúc popup mở: popup lọc lại theo chuỗi đang gõ, không giữ số dòng nguồn cũ tới phím kế tiếp
        // (lỗi UI0-106 test này từng bắt: dòng trỏ sang mục khác chữ đang thấy).
        QTRY_COMPARE(list->model()->rowCount(), matching(*v3, needle));
        QCOMPARE(mismatches(), 0);

        // Phím kế tiếp lọc theo nội dung mới; Enter chọn mục khớp đầu tiên.
        QTest::keyClick(edit, Qt::Key_Backspace);
        QTest::keyClick(edit, 'p');
        QCOMPARE(list->model()->rowCount(), matching(*v3, needle));
        QCOMPARE(mismatches(), 0);
        QTest::keyClick(edit, Qt::Key_Return);
        QVERIFY(!popup->isVisible());
        QCOMPARE(combo->currentText(), layerName(20'000, v3->renamed));
        qInfo("ComboBox trên model nền: %llu lô áp, lô dài nhất %.1f ms; UiProbe: %zu lần khựng",
              (unsigned long long)model.stats().batches,
              std::chrono::duration<double, std::milli>(model.stats().longestBatch).count(), probe::stalls().size());
        for (const probe::Stall& s : probe::stalls())
            qInfo("  khựng %.0f ms: %s", std::chrono::duration<double, std::milli>(s.duration).count(), s.where.c_str());
        probe::disable();
    }

    void listKeepsPlaceAcrossDprAndDensity_data()
    {
        QTest::addColumn<bool>("pixels");
        QTest::addRow("item") << false;
        QTest::addRow("pixel") << true;
    }

    // Model nền (UI0-014) giữ dòng nhìn thấy đầu tiên tại chỗ suốt một lần cập nhật nhiều lô, khi giữa chừng mật độ
    // đổi gọn → thoáng (UI0-002: chiều cao dòng đổi) và cửa sổ sang màn hình 150% (UI0-012: DPR đổi). Unit test giữ vị
    // trí cuộn ở mật độ, DPR cố định; test của UI0-012 không có model đang áp.
    void listKeepsPlaceAcrossDprAndDensity()
    {
        QFETCH(bool, pixels);
        LayerModel model;
        QWidget win;
        auto* view = new QListView;
        (new QVBoxLayout(&win))->addWidget(view);
        view->setUniformItemSizes(true);
        view->setItemDelegate(new RowDelegate(view));
        view->setVerticalScrollMode(pixels ? QAbstractItemView::ScrollPerPixel : QAbstractItemView::ScrollPerItem);
        view->setModel(&model);
        model.keepScrollPosition(view);
        auto v1 = std::make_shared<Layers>();
        v1->keys.resize(20'000);
        std::iota(v1->keys.begin(), v1->keys.end(), std::uint64_t(0));
        QVERIFY(refreshAndSettle(model, v1, 1));
        win.resize(320, 400);
        win.move(screenNamed("p100")->geometry().topLeft() + QPoint(40, 40));
        win.show();
        QVERIFY(QTest::qWaitForWindowExposed(&win));
        QCOMPARE(win.screen(), screenNamed("p100"));
        DisplayContext display(&win);
        QVERIFY(settle());

        view->scrollTo(model.index(12'000, 0), QAbstractItemView::PositionAtTop);
        if (pixels)
            view->verticalScrollBar()->setValue(view->verticalScrollBar()->value() + 5); // đỉnh lệch giữa dòng
        QCoreApplication::processEvents();
        const QModelIndex first = view->indexAt(QPoint(0, 0));
        QCOMPARE(first.row(), 12'000);
        const int top = view->visualRect(first).top();
        const int height = view->visualRect(first).height();

        // v2: một layer mới sau mỗi 3 dòng trong 9.000 dòng đầu (trên vùng nhìn): 3.000 thao tác chèn, nhiều lô.
        auto v2 = std::make_shared<Layers>();
        for (std::uint64_t k = 0; k < 20'000; ++k) {
            v2->keys.push_back(k);
            if (k < 9'000 && k % 3 == 2)
                v2->keys.push_back(100'000 + k);
        }
        bool inserted = false, settled = false, busyAtDpr = false;
        QSignalSpy dprSpy(&display, &DisplayContext::devicePixelRatioChanged);
        QObject ctx;
        connect(&model, &QAbstractItemModel::rowsInserted, &ctx, [&] { inserted = true; });
        connect(&model, &LayerModel::settled, &ctx, [&](quint64 v) { settled = settled || v == 2; });
        connect(&display, &DisplayContext::devicePixelRatioChanged, &ctx, [&] { busyAtDpr = model.busy(); });
        model.refresh(std::shared_ptr<const Layers>(v2), 2, &buildRows);
        QVERIFY(QTest::qWaitFor([&] { return inserted; }, 30000));
        QVERIFY(model.busy()); // lô đầu đã áp, còn lô sau
        const auto batches = model.stats().batches;
        theme_->setDensity(Density::Comfortable);
        win.move(screenNamed("p150")->geometry().topLeft() + QPoint(40, 40));
        QVERIFY(QTest::qWaitFor([&] { return settled && dprSpy.count() > 0; }, 60000));
        QCOMPARE(dprSpy.count(), 1);
        QCOMPARE(dprSpy[0][1].toReal(), 1.5);
        QVERIFY(busyAtDpr);                       // DPR đổi giữa lần cập nhật
        QVERIFY(model.stats().batches > batches); // còn lô áp sau khi đổi mật độ
        QCOMPARE(model.stats().resets, 0u);
        QCOMPARE(model.rowCount(), int(v2->keys.size()));

        const int row = 12'000 + 3'000;
        QCOMPARE(model.row(row).key, std::uint64_t(12'000));
        QVERIFY(view->visualRect(model.index(row, 0)).height() != height); // mật độ thoáng: dòng cao hơn
        QCOMPARE(view->visualRect(model.index(row, 0)).top(), top);
        QCOMPARE(view->indexAt(QPoint(0, 0)).row(), row);
        qInfo("%s: %llu lô áp, %llu thao tác", pixels ? "theo pixel" : "theo dòng",
              (unsigned long long)model.stats().batches, (unsigned long long)model.stats().ops);
        theme_->setDensity(Density::Compact);
    }

    // Nút của UI0-106 tự vẽ biểu tượng ở DPR của thiết bị vẽ: cửa sổ sang màn hình 150% (UI0-012) thì khung đầu raster
    // đúng một lần mỗi biểu tượng (UI0-004) ở cỡ mới; DPR đã gặp: 0 raster; đổi chủ đề (UI0-002) chỉ tô lại; đổi mật
    // độ chỉ raster lại khi --ic đổi. Test của UI0-012 dùng QToolButton; test của UI0-106 vẽ ở DPR cố định.
    void buttonIconsFollowDpr()
    {
        const QStringList names = nonViewer(12);
        QCOMPARE(names.size(), 12);
        const auto n = quint64(names.size());
        QWidget win;
        auto* row = new QHBoxLayout(&win);
        for (const QString& name : names) {
            auto* b = new ui::Button({}, ui::Button::Kind::Icon);
            b->setIconName(name);
            b->setAccessibleName(name); // i18n-ok: tên biểu tượng làm dữ liệu thử
            row->addWidget(b);
        }
        win.move(screenNamed("p100")->geometry().topLeft() + QPoint(40, 40));
        win.show();
        QVERIFY(QTest::qWaitForWindowExposed(&win));
        DisplayContext display(&win); // raster trước bộ Viewer ở DPR mọi màn hình
        QVERIFY(settle());
        win.grab();

        QSignalSpy dprSpy(&display, &DisplayContext::devicePixelRatioChanged);
        const quint64 r0 = icons_->stats().rasters;
        QVERIFY(moveTo(win, "p150"));
        QTRY_COMPARE(dprSpy.count(), 1);
        QCOMPARE(dprSpy[0][1].toReal(), 1.5);
        const QImage img = win.grab().toImage();
        QCOMPARE(img.devicePixelRatio(), 1.5);
        QCOMPARE(img.size(), win.size() * 1.5);
        QCOMPARE(icons_->stats().rasters - r0, n);

        QVERIFY(moveTo(win, "p100"));
        win.grab();
        QVERIFY(moveTo(win, "p150"));
        win.grab();
        QCOMPARE(icons_->stats().rasters - r0, n);

        const quint64 t0 = icons_->stats().tints;
        theme_->setTheme(Theme::Light);
        win.grab();
        QCOMPARE(icons_->stats().rasters - r0, n);
        QVERIFY(icons_->stats().tints >= t0 + n);

        const int compact = qRound(theme_->px(tok::ic));
        theme_->setDensity(Density::Comfortable);
        const int comfortable = qRound(theme_->px(tok::ic));
        win.grab();
        QCOMPARE(icons_->stats().rasters - r0, compact == comfortable ? n : 2 * n);
        theme_->setDensity(Density::Compact);
        theme_->setTheme(Theme::Dark);
    }

    // Giá trị bản vẽ qua UI0-109 (đọc bằng INT-105, hiển thị bằng RGN-013) không đổi theo ngôn ngữ giao diện (UI0-005),
    // từng bit; chuỗi hiển thị gõ lại ra chính nó. Câu lỗi dịch lúc gọi (đổi theo ngôn ngữ, cả vi-XL) mà chuỗi gợi ý
    // giữ dấu chấm. Unit test của UI0-109 chạy ở một ngôn ngữ.
    void drawingValuesIgnoreLanguage()
    {
        const QStringList vi = readBack();
        QCOMPARE(vi.filter(u"gõ lại"_s), QStringList());
        ui::ValueSpec mm;
        mm.kind = ui::ValueKind::Distance;
        mm.insunits = 4;
        const QString viText = ui::errorText(ui::ValueError::Comma, "12,5", mm);
        QVERIFY(viText.contains(u"12.5"_s));
        for (const QString& lang : {u"en"_s, i18n::kPseudoLanguage.toString(), u"vi"_s}) {
            QVERIFY(translator_->setLanguage(lang));
            QCOMPARE(readBack(), vi);
            const QString t = ui::errorText(ui::ValueError::Comma, "12,5", mm);
            QVERIFY2(t.contains(u"12.5"_s) && !t.contains(u"12,5"_s), qPrintable(t));
            if (lang == i18n::kPseudoLanguage)
                QVERIFY2(t.size() >= viText.size() + 2, qPrintable(t));
            else if (lang == u"vi"_s)
                QCOMPARE(t, viText);
        }
    }

    // Sau mọi việc trên (model nền, popup, DisplayContext, đổi DPR, ngôn ngữ), đang bật đo: rảnh thì executor lẫn
    // watchdog không bị đánh thức. Rảnh thật là vòng sự kiện chặn trong dispatcher; bộ đếm đọc từ luồng phụ giữa khoảng
    // rảnh, xa sự kiện timer đóng vòng.
    void idleAfterwards()
    {
        probe::enable();
        QVERIFY(settle());
        QTest::qWait(100);
        const auto wakeups = executor_->stats().wakeups;
        probe::Counters c0, c1;
        std::thread sampler([&] {
            std::this_thread::sleep_for(200ms);
            c0 = probe::counters();
            std::this_thread::sleep_for(1000ms);
            c1 = probe::counters();
        });
        QEventLoop loop;
        QTimer::singleShot(1400ms, &loop, &QEventLoop::quit);
        loop.exec();
        sampler.join();
        QCOMPARE(executor_->stats().wakeups, wakeups);
        QCOMPARE(c1.events, c0.events);
        QCOMPARE(c1.watchdogWakeups, c0.watchdogWakeups);
        probe::disable();
    }

private:
    QTemporaryDir dir_;
    std::unique_ptr<Store> store_;
    std::unique_ptr<ui::UiExecutor> executor_;
    std::unique_ptr<i18n::Translator> translator_;
    std::unique_ptr<ThemeManager> theme_;
    std::unique_ptr<IconProvider> icons_;
};

QTEST_MAIN(IntegrationDieuKhien)
#include "it_dieu_khien_test.moc"
