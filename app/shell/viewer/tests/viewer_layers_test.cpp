// UIA-118: palette Layer của Viewer (ViewerLayers.h), tệp trạng thái hiển thị .tcview (ViewState.h, ViewStateFile).
// DOC-004, DOC-009, CMD-501, INT-002/INT-020 chưa có: nguồn layer là snapshot do test dựng; "lõi" giả áp macro
// '_-LAYER bằng bộ đọc nhỏ ở đây (applyMacro) rồi gọi setLayers như nguồn thật sẽ làm.
//   1  layerMacro: thoát ký tự đại diện, ngoặc kép khi có khoảng trắng, bỏ tên không hợp lệ, LayerMacro::to.
//   2  toggles: bấm, Space, Enter trên ô Bật/Đóng băng → đúng macro; ô Khóa, Màu → trang nâng cấp Pro; menu ⋯ →
//      ^C^C_LAYON, _LAYTHW, _LAYISO, _LAYUNISO; bảng không tự đổi khi chưa có snapshot mới. Hiện trên vùng vẽ sau regen
//      gia tăng: Hoãn (cần RND, CMD-501).
//   3  filterTiming: 5.000 layer, mỗi phím ≤ 16 ms CPU luồng UI (release); kết quả trùng lọc vét cạn. search: gấp dấu,
//      mẫu * ?, "Chỉ hiện layer đang dùng", sắp theo cột.
//   4  restoreOriginal: 5.000 layer, thay đổi ngẫu nhiên → "Khôi phục như tệp gốc" đưa từng layer về trạng thái lúc mở.
//   5  viewStateRoundTrip: .tcview ghi rồi đọc lại cho cùng view, layout, trạng thái layer (hàm và ViewStateFile).
//      brokenFiles: tệp hỏng, sai phiên bản, > 1 MB → bỏ qua kèm banner; thiếu tệp → không banner.
//   6  drawingUntouched: tệp bản vẽ giữ nguyên nội dung, thời điểm sửa; thư mục chỉ thêm .tcview. viewerProGate: bản
//      Viewer không lưu, không nạp, mở trang nâng cấp Viewer Pro; mục menu có huy hiệu.
//   7  keyboardA11y: checkNames, checkFocus 0 vi phạm, thứ tự Tab. pseudoNoTruncation: vi-XL, rộng --pal-w.
#include "app/shell/viewer/ViewerLayers.h"

#include "app/core/UiExecutor.h"
#include "app/core/a11y/AccessibilityAudit.h"
#include "app/core/edition/EditionGate.h"
#include "app/core/i18n/Translator.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/theme/ThemeManager.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/widgets/controls/Controls.h"
#include "app/widgets/menu/Menu.h"
#include "app/widgets/message/Messages.h"
#include "app/widgets/tooltip/RichTooltip.h"
#include "app/widgets/views/VirtualTableView.h"
#include "core/base/sched/Scheduler.h"
#include "core/base/settings/Settings.h"
#include "core/base/text/Fold.h"
#include "tests/timing.h"

#include <QBoxLayout>
#include <QCryptographicHash>
#include <QHeaderView>
#include <QLineEdit>
#include <QRandomGenerator>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>
#include <memory>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;
using namespace trion::shell;
using trion::test::kTimed;
using trion::test::threadCpu;
namespace edition = trion::edition;
namespace i18n = trion::i18n;
namespace sched = trion::sched;
namespace settings = trion::settings;
namespace theme = trion::theme;
namespace ui = trion::ui;

namespace {

bool spinUntil(const std::function<bool()>& cond, std::chrono::milliseconds timeout = 10s)
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

double ms(std::chrono::nanoseconds d)
{
    return std::chrono::duration<double, std::milli>(d).count();
}

QString truncations(QWidget* root)
{
    QStringList out;
    for (const i18n::Truncation& t : i18n::findTruncations(root))
        out << t.toString();
    return out.join(u"; "_s);
}

// Bản vẽ mẫu: tên có dấu, khoảng trắng, ký tự đại diện của wcmatch; trạng thái trong tệp ngẫu nhiên (seed cố định),
// hiệu lực = tệp (vừa mở). Layer 0 hiện hành.
QList<ViewerLayer> drawing(int n, quint32 seed = 7)
{
    static const QString kStems[] = {u"KT-TƯỜNG"_s, u"KC DẦM"_s, u"Cửa đi"_s, u"DIM.1"_s, // i18n-ok: tên layer mẫu
                                     u"TC#[A]"_s,  u"ĐIỆN,NƯỚC"_s, u"Hatch@2"_s, u"~Ghi chú"_s};
    QRandomGenerator rng(seed);
    QList<ViewerLayer> out;
    out.reserve(n);
    for (int i = 0; i < n; ++i) {
        ViewerLayer l;
        l.name = i == 0 ? u"0"_s : u"%1-%2"_s.arg(kStems[i % 8]).arg(i, 4, 10, QChar(u'0'));
        l.fileOn = rng.bounded(4) != 0;
        l.fileFrozen = i != 0 && rng.bounded(6) == 0;
        l.on = l.fileOn;
        l.frozen = l.fileFrozen;
        l.locked = rng.bounded(5) == 0;
        l.current = i == 0;
        l.color = QColor::fromRgb(rng.generate() | 0xFF000000u);
        l.objects = rng.bounded(3) == 0 ? 0 : rng.bounded(100000);
        out << l;
    }
    return out;
}

// "Lõi" giả: áp '_-LAYER _ON a,b _OFF … ; vào snapshot (tách token có ngoặc kép, danh sách theo dấu phẩy chưa thoát,
// bỏ dấu thoát `). Tên không có trong bản vẽ: trả false.
bool applyMacro(QList<ViewerLayer>& layers, QString macro)
{
    if (!macro.startsWith(u"'_-LAYER "_s) || !macro.endsWith(u';'))
        return false;
    macro = macro.mid(9, macro.size() - 10);
    QStringList tokens;
    QString cur;
    bool quoted = false;
    for (QChar c : macro) {
        if (c == u'"')
            quoted = !quoted;
        else if (c == u' ' && !quoted)
            tokens << std::exchange(cur, {});
        else
            cur += c;
    }
    tokens << cur;
    if (tokens.size() % 2)
        return false;
    QHash<QString, qsizetype> byName; // tên không phân biệt hoa thường như bảng LAYER
    for (qsizetype i = 0; i < layers.size(); ++i)
        byName.insert(layers[i].name.toCaseFolded(), i);
    for (qsizetype t = 0; t < tokens.size(); t += 2) {
        QStringList names{QString()};
        for (qsizetype i = 0; i < tokens[t + 1].size(); ++i) {
            const QChar c = tokens[t + 1][i];
            if (c == u'`' && i + 1 < tokens[t + 1].size())
                names.last() += tokens[t + 1][++i];
            else if (c == u',')
                names << QString();
            else
                names.last() += c;
        }
        for (const QString& n : names) {
            const auto found = byName.constFind(n.toCaseFolded());
            if (found == byName.cend())
                return false;
            ViewerLayer* it = &layers[*found];
            const QString& op = tokens[t];
            if (op == u"_ON"_s || op == u"_OFF"_s)
                it->on = op == u"_ON"_s;
            else if (op == u"_THAW"_s || op == u"_FREEZE"_s)
                it->frozen = op == u"_FREEZE"_s;
            else
                return false;
        }
    }
    return true;
}

QByteArray sha256(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha256);
}

} // namespace

class ViewerLayersTest : public QObject {
    Q_OBJECT

    QTemporaryDir dir_;
    std::unique_ptr<settings::Store> store_;
    std::unique_ptr<ui::UiExecutor> executor_;
    std::unique_ptr<theme::ThemeManager> tm_;
    std::unique_ptr<theme::IconProvider> icons_;
    std::unique_ptr<i18n::Translator> translator_;
    std::unique_ptr<ui::RichTooltip> tips_;
    std::unique_ptr<edition::EditionGate> gate_;
    std::unique_ptr<QWidget> window_;
    ViewerLayers* layers_ = nullptr;
    QStringList macros_;
    QList<edition::UpgradeRequest> upgrades_;

    // Palette trong cửa sổ rộng --pal-w (như dock phải), macro vào macros_.
    void makePalette(QList<ViewerLayer> snapshot, int width = 272)
    {
        window_ = std::make_unique<QWidget>();
        auto* box = new QVBoxLayout(window_.get());
        box->setContentsMargins(0, 0, 0, 0);
        layers_ = new ViewerLayers(*gate_, window_.get());
        box->addWidget(layers_);
        connect(layers_, &ViewerLayers::macroRequested, this, [this](const QString& m) { macros_ << m; });
        window_->resize(width, 640);
        window_->show();
        QVERIFY(QTest::qWaitForWindowExposed(window_.get()));
        setLayers(std::move(snapshot));
    }

    void setLayers(QList<ViewerLayer> snapshot)
    {
        QSignalSpy done(layers_, &ViewerLayers::filtered);
        layers_->setLayers(std::move(snapshot));
        QVERIFY(spinUntil([&] { return !done.isEmpty(); }));
    }

    void type(const QString& text)
    {
        if (layers_->searchField()->text() == text)
            return;
        QSignalSpy done(layers_, &ViewerLayers::filtered);
        layers_->searchField()->setText(text);
        QVERIFY(spinUntil([&] { return !done.isEmpty(); }));
    }

    QStringList shownNames() const
    {
        QStringList out;
        const QAbstractItemModel* m = layers_->table()->model();
        for (int r = 0; r < m->rowCount(); ++r)
            out << m->index(r, ViewerLayers::Name).data().toString();
        return out;
    }

    void clickCell(int row, int column)
    {
        ui::VirtualTableView* t = layers_->table();
        const QModelIndex i = t->model()->index(row, column);
        t->scrollTo(i);
        QTest::mouseClick(t->viewport(), Qt::LeftButton, {}, t->visualRect(i).center());
    }

    QAction* menuAction(const char* id) const { return layers_->menu()->findChild<QAction*>(QLatin1StringView(id)); }

    QString drawingPath(const char* name) const { return dir_.filePath(QLatin1StringView(name)); }

private slots:
    void initTestCase()
    {
        QVERIFY(dir_.isValid());
        store_ = std::make_unique<settings::Store>(dir_.filePath(u"user.json"_s));
        settings::Store::setInstance(store_.get());
        QVERIFY(theme::loadFonts());
        executor_ = std::make_unique<ui::UiExecutor>();
        tm_ = std::make_unique<theme::ThemeManager>(*store_);
        icons_ = std::make_unique<theme::IconProvider>();
        translator_ = std::make_unique<i18n::Translator>();
        tips_ = std::make_unique<ui::RichTooltip>();
        gate_ = std::make_unique<edition::EditionGate>();
        gate_->upgrade().setHandler([this](const edition::UpgradeRequest& r) { upgrades_ << r; });
        gate_->upgrade().setUrlOpener([](const QUrl&) { return true; });
        connect(&gate_->upgrade(), &edition::UpgradeRouter::noticeRequested, this,
                [this](const edition::UpgradeRequest& r) { upgrades_ << r; });
    }

    void init()
    {
        gate_->setEdition(edition::Edition::Viewer);
        macros_.clear();
        upgrades_.clear();
    }

    void cleanup()
    {
        sched::waitIdle();
        layers_ = nullptr;
        window_.reset();
        if (translator_->language() != u"vi"_s)
            QVERIFY(translator_->setLanguage(u"vi"_s));
    }

    void cleanupTestCase()
    {
        sched::waitIdle();
        window_.reset();
        gate_.reset();
        tips_.reset();
        translator_.reset();
        icons_.reset();
        tm_.reset();
        settings::Store::setInstance(nullptr);
        store_.reset();
        QCoreApplication::processEvents();
        executor_.reset();
    }

    void layerMacro()
    {
        QCOMPARE(LayerMacro{.off = {u"KT-TUONG"_s}}.text(), u"'_-LAYER _OFF KT-TUONG;"_s);
        // Ký tự đại diện của wcmatch thoát bằng `: "KT.TUONG" không bật nhầm "KT-TUONG".
        QCOMPARE((LayerMacro{.on = {u"KT.TUONG"_s, u"A#1"_s}}.text()), u"'_-LAYER _ON KT`.TUONG,A`#1;"_s);
        QCOMPARE(LayerMacro{.freeze = {u"ĐIỆN,NƯỚC"_s}}.text(), u"'_-LAYER _FREEZE ĐIỆN`,NƯỚC;"_s); // i18n-ok: tên
        QCOMPARE((LayerMacro{.thaw = {u"KC DẦM"_s, u"0"_s}}.text()), u"'_-LAYER _THAW \"KC DẦM,0\";"_s); // i18n-ok
        QCOMPARE((LayerMacro{.on = {u"A"_s}, .off = {u"B"_s}, .thaw = {u"C"_s}, .freeze = {u"D"_s}}.text()),
                 u"'_-LAYER _ON A _OFF B _THAW C _FREEZE D;"_s);
        // Tên không hợp lệ (không biểu diễn được trong lệnh, có thể chèn lệnh) bị bỏ; không còn tên nào: chuỗi rỗng.
        QCOMPARE((LayerMacro{.off = {u"A;;_ERASE;_ALL;"_s, u"B\"C"_s, {}, u"x\ny"_s}}.text()), QString());
        QCOMPARE((LayerMacro{.off = {u"A;"_s, u"B"_s}}.text()), u"'_-LAYER _OFF B;"_s);
        QVERIFY(!isLayerName(QString(256, u'a')) && isLayerName(QString(255, u'a')));
        QVERIFY(!isLayerName(QString(QChar(0xD800))));
        const QList<LayerFlags> target{{u"A"_s, false, true}, {u"B"_s, true, false}};
        QCOMPARE(LayerMacro::to(target).text(), u"'_-LAYER _ON B _OFF A _THAW B _FREEZE A;"_s);
        QCOMPARE(viewStatePath(u"/du-an/Ban ve.v2.dwg"_s), u"/du-an/Ban ve.v2.tcview"_s);
    }

    // Mỗi thao tác phát đúng lệnh; bảng chỉ đổi khi nguồn gửi snapshot mới (không giả kết quả của lệnh).
    void toggles()
    {
        QList<ViewerLayer> s = drawing(9);
        s[8].on = s[8].fileOn = true;
        s[8].frozen = s[8].fileFrozen = false;
        makePalette(s);
        const int row = int(shownNames().indexOf(s[8].name));
        QVERIFY(row >= 0);
        const QString name = s[8].name; // KT-TƯỜNG-0008: không có ký tự cần thoát
        clickCell(row, ViewerLayers::On);
        clickCell(row, ViewerLayers::Freeze);
        QCOMPARE(macros_, (QStringList{u"'_-LAYER _OFF %1;"_s.arg(name), u"'_-LAYER _FREEZE %1;"_s.arg(name)}));
        QCOMPARE(layers_->layers(), s);

        // Bàn phím: ô hiện hành ở cột Bật, Space; cột Đóng băng, Enter. Cột Tên: Space không bật/tắt gì.
        ui::VirtualTableView* t = layers_->table();
        t->setFocus();
        macros_.clear();
        t->setCurrentIndex(t->model()->index(row, ViewerLayers::On));
        QTest::keyClick(t, Qt::Key_Space);
        t->setCurrentIndex(t->model()->index(row, ViewerLayers::Freeze));
        QTest::keyClick(t, Qt::Key_Return);
        t->setCurrentIndex(t->model()->index(row, ViewerLayers::Name));
        QTest::keyClick(t, Qt::Key_Space);
        QCOMPARE(macros_, (QStringList{u"'_-LAYER _OFF %1;"_s.arg(name), u"'_-LAYER _FREEZE %1;"_s.arg(name)}));

        // Nguồn áp lệnh rồi gửi snapshot mới: ô đổi, thẻ "tạm", nút Khôi phục hiện; bấm lại → bật, rã đông.
        QVERIFY(!layers_->restoreButton()->isVisible());
        QVERIFY(applyMacro(s, macros_[0]) && applyMacro(s, macros_[1]));
        setLayers(s);
        QVERIFY(layers_->restoreButton()->isVisible());
        QVERIFY(t->model()->index(row, ViewerLayers::On).data(Qt::ToolTipRole).toString().contains(
            u"đã đổi tạm thời"_s)); // i18n-ok: so chữ tiếng Việt nguồn
        macros_.clear();
        clickCell(row, ViewerLayers::On);
        clickCell(row, ViewerLayers::Freeze);
        QCOMPARE(macros_, (QStringList{u"'_-LAYER _ON %1;"_s.arg(name), u"'_-LAYER _THAW %1;"_s.arg(name)}));

        // Ô Khóa, Màu: Viewer không sửa, mở trang nâng cấp Pro; không phát lệnh.
        macros_.clear();
        clickCell(row, ViewerLayers::Lock);
        clickCell(row, ViewerLayers::Color);
        QVERIFY(macros_.isEmpty());
        QCOMPARE(upgrades_.size(), 2);
        QCOMPARE(upgrades_[0].need, ui::Edition::Pro);
        QCOMPARE(upgrades_[1].feature, u"layer-props"_s);

        // Menu ⋯: lệnh layer INT-020 cho phép ở Viewer, như IMPL.* của prototype.
        for (const char* id : {"on", "thaw", "iso", "uniso"})
            menuAction(id)->trigger();
        QCOMPARE(macros_, (QStringList{u"^C^C_LAYON"_s, u"^C^C_LAYTHW"_s, u"^C^C_LAYISO"_s, u"^C^C_LAYUNISO"_s}));
        // Hiện trên vùng vẽ ở khung kế tiếp sau regen gia tăng: Hoãn (cần RND, CMD-501).
    }

    // 5.000 layer: lọc mỗi phím ≤ 16 ms CPU của luồng UI (bản release; lọc, sắp chạy ở nền), kết quả trùng lọc vét cạn.
    void filterTiming()
    {
        const QList<ViewerLayer> s = drawing(5000);
        makePalette(s, 360);
        QCOMPARE(layers_->shownCount(), 5000);
        const auto expected = [&](const QString& q) {
            const QString needle = trion::text::foldForSearch(q.trimmed());
            int n = 0;
            for (const ViewerLayer& l : s)
                n += trion::text::foldForSearch(l.name).contains(needle);
            return n;
        };
        // Mỗi chuỗi gõ 3 lượt, mỗi phím lấy lượt nhanh nhất (như "best" của tests/timing.h): máy dùng chung (WSL2,
        // nhiều agent) làm thời gian CPU của luồng vọt lên ngẫu nhiên cả ở phím không đổi kết quả.
        QMap<QString, std::chrono::nanoseconds> best;
        std::chrono::nanoseconds worstWall{0};
        for (int round = 0; round < 3; ++round) {
            for (const QString& q : {u"kt-tuong-00"_s, u"cua di 1"_s}) {
                type({});
                QString query;
                for (QChar c : q) {
                    query += c;
                    // Chờ bằng vòng sự kiện thoát theo tín hiệu (không timer 1 ms như spinUntil): CPU đo được chỉ là
                    // việc của palette (phím, lọc, áp lô, vẽ), không lẫn nhịp chờ.
                    QEventLoop loop;
                    QSignalSpy done(layers_, &ViewerLayers::filtered);
                    connect(layers_, &ViewerLayers::filtered, &loop, &QEventLoop::quit);
                    QTimer::singleShot(10s, &loop, &QEventLoop::quit);
                    const auto cpu0 = threadCpu();
                    const auto wall0 = std::chrono::steady_clock::now();
                    QTest::keyClick(layers_->searchField(), c.toLatin1());
                    loop.exec();
                    QVERIFY(!done.isEmpty());
                    const auto cpu = threadCpu() - cpu0;
                    worstWall = std::max(worstWall, std::chrono::steady_clock::now() - wall0);
                    if (!best.contains(query) || cpu < best[query])
                        best[query] = cpu;
                    QCOMPARE(layers_->searchField()->text(), query);
                    QCOMPARE(layers_->shownCount(), expected(query));
                }
            }
        }
        const std::chrono::nanoseconds worstCpu = *std::max_element(best.cbegin(), best.cend());
        qInfo("lọc 5.000 layer: CPU luồng UI mỗi phím (lượt nhanh nhất trong 3) tối đa %.2f ms; đến khi hiện xong tối "
              "đa %.2f ms (cả lượt chậm)",
              ms(worstCpu), ms(worstWall));
        if (threadCpu().count() >= 0)
            QVERIFY2(!kTimed || worstCpu <= 16ms, qPrintable(u"%1 ms"_s.arg(ms(worstCpu))));
    }

    void search()
    {
        QList<ViewerLayer> s = drawing(9);
        s[2].objects = 0;
        makePalette(s);
        type(u"tuong"_s); // "KT-TƯỜNG-0008"
        QCOMPARE(shownNames(), QStringList{s[8].name});
        type(u"KC DAM"_s);
        QCOMPARE(shownNames(), QStringList{s[1].name});
        type(u"cua"_s);
        QCOMPARE(shownNames(), QStringList{s[2].name});
        type(u"kt-*8"_s); // mẫu: cả tên, gấp dấu
        QCOMPARE(shownNames(), QStringList{s[8].name});
        type(u"kt-*1"_s);
        QCOMPARE(layers_->shownCount(), 0);
        type(u"?I*3"_s); // DIM.1-0003 (ĐIỆN,NƯỚC-0005 cũng có "i" thứ hai nhưng không tận cùng 3)
        QCOMPARE(shownNames(), QStringList{s[3].name});
        type(u"khong co"_s);
        QCOMPARE(layers_->shownCount(), 0);
        QVERIFY(layers_->findChild<QLabel*>(u"layerCount"_s)->text().contains(u"0/9"_s));
        type({});
        QSignalSpy done(layers_, &ViewerLayers::filtered);
        menuAction("used")->trigger();
        QVERIFY(spinUntil([&] { return !done.isEmpty(); }));
        QVERIFY(!shownNames().contains(s[2].name));
        QCOMPARE(layers_->shownCount(), int(std::count_if(s.begin(), s.end(), [](auto& l) { return l.objects > 0; })));
        menuAction("used")->trigger();
        // Sắp theo số đối tượng giảm dần (bấm đầu cột hai lần).
        done.clear();
        layers_->table()->model()->sort(ViewerLayers::Count, Qt::DescendingOrder);
        QVERIFY(spinUntil([&] { return !done.isEmpty(); }));
        const QAbstractItemModel* m = layers_->table()->model();
        for (int r = 1; r < m->rowCount(); ++r)
            QVERIFY(m->index(r - 1, ViewerLayers::Count).data().toString().remove(u'.').toLongLong() >=
                    m->index(r, ViewerLayers::Count).data().toString().remove(u'.').toLongLong());
    }

    // "Khôi phục như tệp gốc" trả đúng trạng thái lúc mở, so từng layer.
    void restoreOriginal()
    {
        const QList<ViewerLayer> opened = drawing(5000, 11);
        makePalette(opened);
        layers_->restoreOriginal(); // chưa đổi gì: không lệnh
        QVERIFY(macros_.isEmpty());
        QVERIFY(!layers_->restoreButton()->isVisible());
        emit layers_->menu()->aboutToShow();
        QVERIFY(!menuAction("restore")->isEnabled());

        // Phiên (lõi giả áp lệnh): bật/tắt, đóng băng ngẫu nhiên 1.500 lần, rồi cô lập giả (tắt 1/3 số layer) bằng một
        // lệnh -LAYER của LayerMacro.
        QList<ViewerLayer> s = opened;
        QRandomGenerator rng(3);
        for (int k = 0; k < 1500; ++k) {
            ViewerLayer& l = s[rng.bounded(int(s.size()))];
            if (rng.bounded(2))
                l.on = !l.on;
            else
                l.frozen = !l.frozen;
        }
        LayerMacro iso;
        for (qsizetype i = 0; i < s.size(); i += 3)
            iso.off << s[i].name;
        QVERIFY(applyMacro(s, iso.text()));
        setLayers(s);
        QVERIFY(layers_->hasChanges());
        QVERIFY(layers_->restoreButton()->isVisible());
        emit layers_->menu()->aboutToShow();
        QVERIFY(menuAction("restore")->isEnabled());

        QTest::mouseClick(layers_->restoreButton(), Qt::LeftButton);
        QCOMPARE(macros_.size(), 1);
        QVERIFY(applyMacro(s, macros_[0]));
        for (qsizetype i = 0; i < s.size(); ++i) {
            QCOMPARE(s[i].on, opened[i].on);
            QCOMPARE(s[i].frozen, opened[i].frozen);
        }
        setLayers(s);
        QVERIFY(!layers_->hasChanges());
        QVERIFY(!layers_->restoreButton()->isVisible());
        QCOMPARE(layers_->overrides(), QList<LayerFlags>());
    }

    // .tcview ghi rồi đọc lại cho cùng view, layout và trạng thái layer.
    void viewStateRoundTrip()
    {
        gate_->setEdition(edition::Edition::ViewerPro);
        QList<ViewerLayer> opened = drawing(300, 5), s = opened;
        for (qsizetype i = 1; i < s.size(); i += 7)
            s[i].on = !s[i].on;
        for (qsizetype i = 2; i < s.size(); i += 11)
            s[i].frozen = !s[i].frozen;
        makePalette(s);
        ViewState state{.layout = u"A1-Mặt bằng tầng 2"_s, // i18n-ok: tên layout
                        .centerX = 12345.678901234567,
                        .centerY = -0.1,
                        .height = 4.2e3,
                        .layers = layers_->overrides()};
        QVERIFY(!state.layers.isEmpty());
        QCOMPARE(fromJson(toJson(state)).value(), state);

        const QString dwg = drawingPath("tron-ve.dwg");
        QVERIFY(writeViewState(dwg, state));
        QCOMPARE(readViewState(dwg).value(), state);

        // Qua ViewStateFile (nền) như nơi lắp: lưu, nạp, áp layer bằng một lệnh lên bản vẽ vừa mở lại.
        ui::CanvasBanner banner(window_.get());
        ViewStateFile file(*gate_, banner);
        QSignalSpy saved(&file, &ViewStateFile::saved), loaded(&file, &ViewStateFile::loaded);
        connect(layers_, &ViewerLayers::saveViewRequested, &file, [&] {
            QVERIFY(file.save(dwg, state));
        });
        menuAction("save")->trigger();
        QVERIFY(spinUntil([&] { return !saved.isEmpty(); }));
        QCOMPARE(saved[0][1].toString(), QString());
        file.load(dwg);
        QVERIFY(spinUntil([&] { return !loaded.isEmpty(); }));
        QCOMPARE(loaded[0][0].toString(), dwg);
        const ViewState back = loaded[0][1].value<ViewState>();
        QCOMPARE(back, state);
        QVERIFY(applyMacro(opened, LayerMacro::to(back.layers).text()));
        for (qsizetype i = 0; i < s.size(); ++i) {
            QCOMPARE(opened[i].on, s[i].on);
            QCOMPARE(opened[i].frozen, s[i].frozen);
        }
        QVERIFY(banner.ids().isEmpty());
    }

    // Tệp hỏng, sai phiên bản, > 1 MB: bỏ qua kèm banner; thiếu tệp: im lặng.
    void brokenFiles()
    {
        gate_->setEdition(edition::Edition::ViewerPro);
        const auto code = [](const QByteArray& json) {
            const trion::Expected<ViewState> s = fromJson(json);
            return s ? QByteArray("ok") : QByteArray::fromStdString(s.error().code);
        };
        const QByteArray ok = R"({"tcview":1,"layout":"Model","view":{"center":[0,0],"height":1},"layers":[]})";
        QCOMPARE(code(ok), "ok");
        QCOMPARE(code(QByteArray(ok).replace(R"("tcview":1)", R"("tcview":2)")), "version");
        QCOMPARE(code(QByteArray(ok).replace(R"("tcview":1)", R"("tcview":"1")")), "corrupt");
        QCOMPARE(code(QByteArray(ok).replace(R"("height":1)", R"("height":0)")), "corrupt");
        QCOMPARE(code(QByteArray(ok).replace(R"("height":1)", R"("height":1e999)")), "corrupt");
        QCOMPARE(code(QByteArray(ok).replace(R"("layout":"Model")", R"("layout":"")")), "corrupt");
        QCOMPARE(code(QByteArray(ok).replace(R"("layers":[])",
                                             R"("layers":[{"name":"a;b","on":true,"frozen":false}])")),
                 "corrupt");
        QCOMPARE(code(QByteArray(ok).replace(R"("layers":[])", R"("layers":[{"name":"A","on":1,"frozen":false}])")),
                 "corrupt");
        QCOMPARE(code(ok.left(ok.size() - 1)), "corrupt");
        QCOMPARE(code(QByteArray(ViewState::kMaxBytes + 1, ' ')), "too-large");
        // Khóa lạ bỏ qua (bản sau thêm trường).
        QCOMPARE(code(QByteArray(ok).replace(R"("layers":[])", R"("layers":[],"markup":{"x":1})")), "ok");

        makePalette(drawing(3));
        ui::CanvasBanner banner(window_.get());
        ViewStateFile file(*gate_, banner);
        QSignalSpy loaded(&file, &ViewStateFile::loaded);
        const auto load = [&](const char* name, const QByteArray& content) {
            const QString dwg = drawingPath(name);
            QFile f(viewStatePath(dwg));
            if (!content.isNull()) {
                QVERIFY(f.open(QIODevice::WriteOnly));
                f.write(content);
                f.close();
            }
            banner.remove(ViewStateFile::kBanner.toString());
            file.load(dwg);
            sched::waitIdle();               // đọc ở nền xong, kết quả đã gửi về luồng UI
            QCoreApplication::processEvents(); // lượt của UiExecutor
        };
        load("thieu.dwg", {});
        QVERIFY(!banner.contains(ViewStateFile::kBanner.toString()));
        QVERIFY(loaded.isEmpty());
        load("hong.dwg", "{\"tcview\":1,\"layout\":");
        QVERIFY(banner.contains(ViewStateFile::kBanner.toString()));
        QVERIFY(loaded.isEmpty());
        QByteArray big = ok;
        big.insert(1, QByteArray(ViewState::kMaxBytes, ' ')); // JSON hợp lệ nhưng > 1 MB
        load("lon.dwg", big);
        QVERIFY(banner.contains(ViewStateFile::kBanner.toString()));
        QVERIFY(loaded.isEmpty());
        QStringList texts;
        for (const QLabel* l : banner.row(ViewStateFile::kBanner.toString())->findChildren<QLabel*>())
            texts << l->text();
        QVERIFY2(texts.join(u'|').contains(u"lon.tcview"_s), qPrintable(texts.join(u'|')));
        load("tot.dwg", ok);
        QCOMPARE(loaded.size(), 1);

        // Lưu: trạng thái > 1 MB không ghi (đọc lại sẽ bị bỏ qua), báo lỗi.
        ViewState huge{.layout = u"Model"_s};
        for (int i = 0; i < 20000; ++i)
            huge.layers.append({u"LAYER-RAT-DAI-%1"_s.arg(i, 6, 10, QChar(u'0')), false, true});
        QCOMPARE(QByteArray::fromStdString(writeViewState(drawingPath("qua-co.dwg"), huge).error().code), "too-large");
        QVERIFY(!QFile::exists(viewStatePath(drawingPath("qua-co.dwg"))));
    }

    // Tệp bản vẽ không bị ghi: nội dung, thời điểm sửa giữ nguyên; trong thư mục chỉ thêm .tcview.
    void drawingUntouched()
    {
        gate_->setEdition(edition::Edition::ViewerPro);
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        const QString dwg = folder.filePath(u"ban-ve.dwg"_s);
        {
            QFile f(dwg);
            QVERIFY(f.open(QIODevice::WriteOnly));
            QByteArray bytes(64 * 1024, Qt::Uninitialized);
            QRandomGenerator(9).fillRange(reinterpret_cast<quint32*>(bytes.data()), bytes.size() / 4);
            f.write(bytes);
        }
        const QByteArray hash = sha256(dwg);
        const QDateTime modified = QFileInfo(dwg).lastModified();

        QList<ViewerLayer> s = drawing(50);
        makePalette(s);
        ui::CanvasBanner banner(window_.get());
        ViewStateFile file(*gate_, banner);
        QSignalSpy saved(&file, &ViewStateFile::saved), loaded(&file, &ViewStateFile::loaded);
        clickCell(1, ViewerLayers::On);
        QVERIFY(applyMacro(s, macros_.last()));
        setLayers(s);
        QVERIFY(file.save(dwg, {.layout = u"Model"_s, .height = 10, .layers = layers_->overrides()}));
        QVERIFY(spinUntil([&] { return !saved.isEmpty(); }));
        file.load(dwg);
        QVERIFY(spinUntil([&] { return !loaded.isEmpty(); }));
        layers_->restoreOriginal();
        QVERIFY(applyMacro(s, macros_.last()));
        setLayers(s);

        QCOMPARE(sha256(dwg), hash);
        QCOMPARE(QFileInfo(dwg).lastModified(), modified);
        QCOMPARE(QDir(folder.path()).entryList(QDir::Files, QDir::Name),
                 (QStringList{u"ban-ve.dwg"_s, u"ban-ve.tcview"_s}));
        // Bản vẽ tên .tcview: không bao giờ ghi đè chính nó.
        const QString odd = folder.filePath(u"la.tcview"_s);
        QVERIFY(QFile::copy(dwg, odd));
        QCOMPARE(QByteArray::fromStdString(writeViewState(odd, {.layout = u"Model"_s}).error().code), "path");
        QCOMPARE(sha256(odd), hash);
    }

    // Viewer Pro mới có lưu, nạp trạng thái hiển thị; bản Viewer: trang nâng cấp Viewer Pro, huy hiệu trên mục menu.
    void viewerProGate()
    {
        makePalette(drawing(5));
        ui::CanvasBanner banner(window_.get());
        ViewStateFile file(*gate_, banner);
        QSignalSpy saved(&file, &ViewStateFile::saved), loaded(&file, &ViewStateFile::loaded);
        const QString dwg = drawingPath("cong.dwg");
        QVERIFY(writeViewState(dwg, {.layout = u"Model"_s})); // tệp có sẵn (bản VP trước đó lưu)
        QFile::remove(viewStatePath(drawingPath("cong-moi.dwg")));

        const auto badge = [this] {
            emit layers_->menu()->aboutToShow();
            return ui::RichTooltip::info(menuAction("save")).need;
        };
        QCOMPARE(badge(), ui::Edition::ViewerPro);
        QVERIFY(!file.save(drawingPath("cong-moi.dwg"), {.layout = u"Model"_s}));
        QCOMPARE(upgrades_.size(), 1);
        QCOMPARE(upgrades_[0].need, ui::Edition::ViewerPro);
        file.load(dwg);
        sched::waitIdle();
        QCoreApplication::processEvents();
        QVERIFY(saved.isEmpty() && loaded.isEmpty());
        QVERIFY(!QFile::exists(viewStatePath(drawingPath("cong-moi.dwg"))));

        gate_->setEdition(edition::Edition::ViewerPro);
        QCOMPARE(badge(), ui::Edition::None);
        QVERIFY(file.save(drawingPath("cong-moi.dwg"), {.layout = u"Model"_s}));
        file.load(dwg);
        QVERIFY(spinUntil([&] { return !saved.isEmpty() && !loaded.isEmpty(); }));
        QVERIFY(QFile::exists(viewStatePath(drawingPath("cong-moi.dwg"))));
    }

    void keyboardA11y()
    {
        QList<ViewerLayer> s = drawing(40);
        s[1].on = !s[1].on; // có thay đổi: nút Khôi phục hiện, vào vòng Tab
        makePalette(s);
        window_->activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(window_.get()));
        QCOMPARE(trion::a11y::checkNames(window_.get()), QStringList());
        QStringList order;
        QCOMPARE(trion::a11y::checkFocus(window_.get(), &order), QStringList());
        QCOMPARE(order.size(), 4);
        QVERIFY2(order[0].contains(u"layerSearch"_s) && order[1].contains(u"layerMore"_s) &&
                     order[2].contains(u"layerTable"_s) && order[3].contains(u"layerRestore"_s),
                 qPrintable(order.join(u" → "_s)));
        // Menu ⋯ mở bằng bàn phím, Esc đóng.
        layers_->findChild<ui::Button*>(u"layerMore"_s)->setFocus(Qt::TabFocusReason);
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Space);
        QVERIFY(spinUntil([&] { return layers_->menu()->isVisible(); }));
        QTest::keyClick(layers_->menu(), Qt::Key_Escape);
        QVERIFY(spinUntil([&] { return !layers_->menu()->isVisible(); }));
    }

    void pseudoNoTruncation()
    {
        QVERIFY(translator_->setLanguage(i18n::kPseudoLanguage.toString()));
        QVERIFY(spinUntil([] { return ViewerLayers::tr("Bỏ cô lập").size() > 9; }));
        // Tên layer là dữ liệu: hẹp thì rút gọn "…" là đúng. Đo chữ của giao diện (gồm thẻ "tạm" cạnh tên) với tên ngắn
        // ở bề rộng palette người dùng kéo ra một chút (--pal-w 272 px chỉ còn ~74 px cho cột Tên).
        QList<ViewerLayer> s = drawing(12);
        for (qsizetype i = 0; i < s.size(); ++i)
            s[i].name = u"L%1"_s.arg(i);
        s[1].on = !s[1].on;
        makePalette({}, 320);
        QVERIFY(layers_->findChild<ui::EmptyState*>()->isVisible());
        QCOMPARE(truncations(window_.get()), QString());
        setLayers(s);
        QVERIFY(spinUntil([&] { return layers_->restoreButton()->isVisible(); }));
        QCOMPARE(truncations(window_.get()), QString()); // gồm menu ⋯ (QMenu con, cả khi đóng)
    }
};

QTEST_MAIN(ViewerLayersTest)
#include "viewer_layers_test.moc"
