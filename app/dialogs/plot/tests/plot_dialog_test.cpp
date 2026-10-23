// UIH-001 · Hộp thoại In (PlotDialog, PlotSettingsModel, PlotRunner, parsePlotCommand). Mỗi tiêu chí nghiệm thu một
// nhóm hàm test; bản vẽ là PlotContext mẫu (lõi PLT-001 chưa có), máy in hệ thống là nguồn mẫu (setPrinterSource).
//   1  groupsAndFields: đủ nhóm, trường của 07.2; PDF/PNG bắt buộc in ra tệp; khổ theo thiết bị; số bản 1–999; vùng in
//      theo ngữ cảnh (Layout | Giới hạn), View có danh sách, Cửa sổ có nút chọn; offset; tỉ lệ; phần mở rộng.
//   2  initialValues: giá trị ban đầu từ thiết lập trang của layout; Model chưa có thì Màn hình hiện tại + Vừa khổ giấy.
//   3  namedSetupAndExpand: chọn thiết lập có tên chép mọi trường; <Lần in trước>; mở rộng không dựng lại, giữ giá trị,
//      nhớ trong Store.
//   4  windowPick: Cửa sổ ẩn hộp, hiện lại kèm tọa độ; Esc (cancelWindowPick) giữ cửa sổ cũ.
//   5  plotRunsInBackground, plotToPrinter, cancelPlot: hộp đóng ngay, việc nền "Đang in 1/1 · p%", toast xong (Mở
//      thư mục), toast gửi máy in, hủy ≤ 50 ms (kTimed), không tệp dở.
//   6  scriptMatchesDialog: cùng PlotSettings từ hộp thoại và từ chuỗi trả lời -PLOT (Model, layout); lỗi của -PLOT.
//      So PDF 0 pixel ở 300 DPI: Hoãn (cần PLT-003, PLT-105, QA-005).
//   7  editionBadges: Viewer, Viewer Pro: in đủ; 5 phần Pro có huy hiệu, bấm mở trang nâng cấp; Pro: phát yêu cầu.
//   8  timing: mở đến khung đầu ≤ 150 ms, đổi một trường ≤ 16 ms (CPU luồng UI, kiểm khi kTimed).
//   DoD-U: printersInBackground (quét nền, đang tải, máy in vắng), keyboard (Enter, Esc), accessibility (a11y::audit),
//   pseudoNoTruncation (vi-XL, hộp gọn và mở rộng).
#include "app/dialogs/plot/PlotCommand.h"
#include "app/dialogs/plot/PlotDialog.h"
#include "app/dialogs/plot/PlotRunner.h"
#include "app/dialogs/plot/PlotSettingsModel.h"

#include "app/core/a11y/AccessibilityAudit.h"
#include "app/core/edition/EditionGate.h"
#include "app/core/edition/LockBadge.h"
#include "app/core/i18n/Translator.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/core/UiExecutor.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/message/Messages.h"
#include "app/widgets/numeric/NumericField.h"
#include "app/widgets/progress/ProgressPresenter.h"
#include "core/base/sched/Scheduler.h"
#include "tests/timing.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMainWindow>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <atomic>
#include <memory>
#include <thread>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;
using namespace trion;
using namespace trion::ui;
using plot::AreaKind;
using Option = PlotSettingsModel::Option;

namespace {

using Clock = std::chrono::steady_clock;

const QString kA1 = u"ISO_full_bleed_A1_(841.00_x_594.00_MM)"_s;
const QString kCanon = u"Canon iR-ADV C3530 (KT)"_s;
const QString kHp = u"HP DesignJet T650"_s;

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

double ms(std::chrono::nanoseconds d)
{
    return std::chrono::duration<double, std::milli>(d).count();
}

QList<plot::Device> samplePrinters()
{
    plot::Device canon{kCanon, plot::DeviceKind::System, u"\\\\SRV-01\\Canon-KT"_s, u"Canon iR-ADV C3530"_s,
                       {{u"A4"_s, u"A4"_s, 210, 297}, {u"A3"_s, u"A3"_s, 297, 420}}, u"A4"_s};
    plot::Device hp{kHp, plot::DeviceKind::System, u"192.168.1.45:9100"_s, {},
                    {{u"A1"_s, u"A1"_s, 594, 841}, {u"A0"_s, u"A0"_s, 841, 1189}}, u"A1"_s};
    return {canon, hp};
}

plot::PageSetup a1Pdf()
{
    plot::PageSetup p;
    p.name = u"A1-PDF"_s;
    p.device = plot::kPdfDevice.toString();
    p.paper = kA1;
    p.area.kind = AreaKind::Layout;
    p.plotStyle = u"TCVN.ctb"_s;
    return p;
}

plot::PageSetup a3Canon()
{
    plot::PageSetup p;
    p.name = u"A3-Phong-KT"_s;
    p.device = kCanon;
    p.paper = u"A3"_s;
    p.area.kind = AreaKind::Extents;
    p.scale.fit = true;
    p.offset.centered = true;
    p.orientation = plot::Orientation::Portrait;
    p.plotStyle = u"monochrome.ctb"_s;
    p.shade.mode = plot::ShadeMode::Hidden;
    return p;
}

// Chờ khung đầu của hộp: lần vẽ đầu xong thì timer 0 ms hẹn từ lúc đó chạy trước việc hẹn sau nó.
class FirstFrame : public QObject {
public:
    bool done = false;
    std::chrono::nanoseconds cpu{-1};
    bool eventFilter(QObject*, QEvent* e) override
    {
        if (e->type() == QEvent::Paint && !std::exchange(armed_, true))
            QTimer::singleShot(0, this, [this] {
                cpu = trion::test::threadCpu();
                done = true;
            });
        return false;
    }

private:
    bool armed_ = false;
};

template <class T>
T* child(QWidget* d, const char* name)
{
    return d->findChild<T*>(QString::fromLatin1(name));
}

QComboBox* combo(QWidget* d, const char* name)
{
    return child<QComboBox>(d, name);
}

QCheckBox* option(QWidget* d, Option o)
{
    return d->findChild<QCheckBox*>(u"option%1"_s.arg(int(o)));
}

// Chọn mục có dữ liệu value như người dùng (phát currentIndexChanged).
bool pick(QComboBox* box, const QVariant& value)
{
    const int i = box->findData(value);
    if (i < 0)
        return false;
    box->setCurrentIndex(i);
    return true;
}

// Gõ vào ô số rồi rời ô (Tab) để nhận; Enter sẽ đi tiếp tới nút In. Hộp phải active để ô nhận focus.
void type(NumericField* f, const QString& text)
{
    auto* edit = qobject_cast<QLineEdit*>(f->focusProxy());
    QVERIFY(edit);
    if (QApplication::activeWindow() != f->window()) {
        f->window()->activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(f->window()));
    }
    edit->setFocus(Qt::OtherFocusReason);
    edit->selectAll();
    QTest::keyClicks(edit, text);
    QTest::keyClick(edit, Qt::Key_Tab);
}

// Mọi trường của PlotSettings thành chữ: so hộp thoại và -PLOT, in ra khi khác.
QString describe(const plot::PlotSettings& s)
{
    const plot::PageSetup& p = s.setup;
    return u"name=%1 device=%2 paper=%3 units=%4 area=%5/%6/%7,%8,%9,%10 "_s.arg(p.name, p.device, p.paper)
               .arg(int(p.units)).arg(int(p.area.kind)).arg(p.area.view)
               .arg(p.area.window.x()).arg(p.area.window.y()).arg(p.area.window.width()).arg(p.area.window.height())
         + u"scale=%1/%2/%3/%4 offset=%5/%6/%7 orient=%8 upside=%9 "_s.arg(p.scale.fit).arg(p.scale.mm)
               .arg(p.scale.units).arg(p.scale.scaleLineweights).arg(p.offset.centered).arg(p.offset.x)
               .arg(p.offset.y).arg(int(p.orientation)).arg(p.upsideDown)
         + u"style=%1 flags=%2%3%4%5%6%7 shade=%8/%9/%10 "_s.arg(p.plotStyle).arg(p.flags.plotStyles)
               .arg(p.flags.lineweights).arg(p.flags.transparency).arg(p.flags.paperSpaceLast)
               .arg(p.flags.hidePaperSpace).arg(p.flags.showPlotStyles).arg(int(p.shade.mode))
               .arg(int(p.shade.quality)).arg(p.shade.dpi)
         + u"copies=%1 toFile=%2 file=%3 bg=%4 stamp=%5 save=%6"_s.arg(s.copies).arg(s.toFile).arg(s.file)
               .arg(s.background).arg(s.stamp).arg(s.saveToLayout);
}

QString truncations(QWidget* root)
{
    QStringList out;
    for (const i18n::Truncation& t : i18n::findTruncations(root))
        out << t.toString();
    return out.join(u"; "_s);
}

} // namespace

QString g_cwd;

class PlotDialogTest : public QObject {
    Q_OBJECT
public:
    PlotDialogTest() { QDir::setCurrent(g_cwd); }

    // Màn hình offscreen 1366 × 768 (DoD-U: màn hình nhỏ nhất): hộp gọn và mở rộng phải vừa, kể cả vi-XL. Đường dẫn
    // tương đối: tùy chọn của plugin ngăn cách bằng ':', đường dẫn Windows "C:/…" có ':'.
    static void initMain()
    {
        static QTemporaryDir config;
        g_cwd = QDir::currentPath();
        const QJsonObject screen{{u"name"_s, u"s1366"_s}, {u"x"_s, 0}, {u"y"_s, 0}, {u"width"_s, 1366},
                                 {u"height"_s, 768}, {u"logicalDpi"_s, 96}, {u"logicalBaseDpi"_s, 96}, {u"dpr"_s, 1}};
        QFile f(config.filePath(u"screens.json"_s));
        const QByteArray json = QJsonDocument(QJsonObject{{u"screens"_s, QJsonArray{screen}}}).toJson();
        if (!config.isValid() || !f.open(QIODevice::WriteOnly) || f.write(json) != json.size()
            || !QDir::setCurrent(config.path()))
            qFatal("screens.json");
        qputenv("QT_QPA_PLATFORM", "offscreen:configfile=screens.json");
    }

private:
    QTemporaryDir dir_;
    std::unique_ptr<settings::Store> store_;
    std::unique_ptr<UiExecutor> executor_;
    std::unique_ptr<theme::ThemeManager> tm_;
    std::unique_ptr<theme::IconProvider> icons_;
    std::unique_ptr<RichTooltip> tips_;
    std::unique_ptr<i18n::Translator> translator_;
    std::unique_ptr<edition::EditionGate> gate_;
    std::unique_ptr<QMainWindow> window_;
    std::unique_ptr<NotificationCenter> notes_;
    std::unique_ptr<ToastHost> toasts_;
    std::unique_ptr<jobs::BackgroundJobs> jobs_;
    std::unique_ptr<ProgressPresenter> presenter_;
    std::unique_ptr<PlotRunner> runner_;
    QStringList upgrades_;
    QStringList opened_;
    std::atomic<bool> release_{true}; // nguồn máy in chậm của printersInBackground

    plot::PlotContext layoutContext() const
    {
        plot::PlotContext c;
        c.document = u"NhaO-2T"_s;
        c.layout = u"A1-KT"_s;
        c.layoutSetup = a1Pdf();
        c.namedSetups = {a1Pdf(), a3Canon()};
        c.views = {u"Mat bang tang 1"_s, u"Mat cat 1-1"_s};
        c.plotStyles = {u"TCVN.ctb"_s, u"monochrome.ctb"_s, u"grayscale.ctb"_s};
        c.outputDir = dir_.path();
        c.owner = 7;
        c.units.luprec = 2;
        return c;
    }

    plot::PlotContext modelContext() const
    {
        plot::PlotContext c = layoutContext();
        c.layout.clear();
        c.layoutSetup.reset();
        return c;
    }

    QPointer<PlotDialog> open(plot::PlotContext context)
    {
        auto* d = new PlotDialog(std::move(context), runner_.get(), window_.get());
        d->setAttribute(Qt::WA_DeleteOnClose);
        d->open();
        if (!QTest::qWaitForWindowExposed(d))
            return {};
        return d;
    }

    static bool printersLoaded(PlotDialog* d) { return d->model()->printersLoaded(); }

private slots:
    void initTestCase()
    {
        QVERIFY(dir_.isValid());
        store_ = std::make_unique<settings::Store>(dir_.filePath(u"user.json"_s));
        settings::Store::setInstance(store_.get());
        QVERIFY(theme::loadFonts());
        executor_ = std::make_unique<UiExecutor>();
        tm_ = std::make_unique<theme::ThemeManager>(*store_);
        icons_ = std::make_unique<theme::IconProvider>();
        tips_ = std::make_unique<RichTooltip>();
        translator_ = std::make_unique<i18n::Translator>();
        gate_ = std::make_unique<edition::EditionGate>();
        // Lần đầu mỗi tính năng: trang nâng cấp; lần sau trong phiên: chú thích có liên kết. Cả hai là "mở nâng cấp".
        gate_->upgrade().setHandler([this](const edition::UpgradeRequest& r) { upgrades_ << r.feature; });
        connect(&gate_->upgrade(), &edition::UpgradeRouter::noticeRequested, this,
                [this](const edition::UpgradeRequest& r) { upgrades_ << r.feature; });
        window_ = std::make_unique<QMainWindow>();
        window_->resize(1366, 768);
        window_->show();
        QVERIFY(QTest::qWaitForWindowExposed(window_.get()));
        notes_ = std::make_unique<NotificationCenter>();
        plot::setPrinterSource([] { return samplePrinters(); });
    }

    void init()
    {
        gate_->setEdition(edition::Edition::Pro);
        store_->set(kPlotMore, false);
        upgrades_.clear();
        opened_.clear();
        toasts_ = std::make_unique<ToastHost>(window_.get(), *notes_);
        jobs_ = std::make_unique<jobs::BackgroundJobs>(jobs::Timing{10ms, 20ms, 20ms});
        presenter_ = std::make_unique<ProgressPresenter>(*jobs_);
        runner_ = std::make_unique<PlotRunner>(*jobs_);
        runner_->setPipeline(PlotRunner::fakePipeline(300ms));
        runner_->setFolderOpener([this](const QString& dir) { opened_ << dir; });
    }

    void cleanup()
    {
        release_ = true;
        plot::setPrinterSource([] { return samplePrinters(); });
        qDeleteAll(window_->findChildren<PlotDialog*>());
        jobs_->cancelOwner(7);
        sched::waitIdle();
        QVERIFY(spinUntil([&] { return jobs_->size() == 0; }));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents();
        runner_.reset();
        presenter_.reset();
        jobs_.reset();
        toasts_.reset();
        if (translator_->language() != u"vi"_s)
            QVERIFY(translator_->setLanguage(u"vi"_s));
    }

    void cleanupTestCase()
    {
        plot::setPrinterSource({});
        window_.reset();
        notes_.reset();
        gate_.reset();
        translator_.reset();
        tips_.reset();
        icons_.reset();
        tm_.reset();
        settings::Store::setInstance(nullptr);
        store_.reset();
        sched::waitIdle();
        QCoreApplication::processEvents();
        executor_.reset();
    }

    // Tiêu chí 2: giá trị ban đầu.
    void initialValues()
    {
        QPointer<PlotDialog> d = open(layoutContext());
        QVERIFY(d);
        const plot::PageSetup& p = d->model()->settings().setup;
        QCOMPARE(p.device, plot::kPdfDevice.toString());
        QCOMPARE(p.paper, kA1);
        QCOMPARE(p.plotStyle, u"TCVN.ctb"_s);
        QCOMPARE(p.area.kind, AreaKind::Layout);
        QVERIFY(!p.scale.fit);
        QCOMPARE(combo(d, "setup")->currentData().toString(), u"A1-PDF"_s);
        QCOMPARE(combo(d, "device")->currentData().toString(), plot::kPdfDevice.toString());
        QCOMPARE(combo(d, "paper")->currentData().toString(), kA1);
        QCOMPARE(combo(d, "area")->currentData().toInt(), int(AreaKind::Layout));
        QCOMPARE(combo(d, "scale")->currentText(), u"1:1"_s);
        QCOMPARE(combo(d, "plotStyle")->currentData().toString(), u"TCVN.ctb"_s);
        QVERIFY(!child<QCheckBox>(d, "fit")->isChecked());
        d->close();

        d = open(modelContext());
        QVERIFY(d);
        QCOMPARE(d->model()->settings().setup.area.kind, AreaKind::Display);
        QVERIFY(d->model()->settings().setup.scale.fit);
        QCOMPARE(combo(d, "setup")->currentIndex(), 0); // <Không>
        QCOMPARE(combo(d, "area")->currentData().toInt(), int(AreaKind::Display));
        QVERIFY(child<QCheckBox>(d, "fit")->isChecked());
        QVERIFY(!combo(d, "scale")->isEnabled());
    }

    // Tiêu chí 1: đủ nhóm và trường, quy tắc liên đới.
    void groupsAndFields()
    {
        plot::PlotContext ctx = layoutContext();
        ctx.previous = a3Canon();
        QPointer<PlotDialog> d = open(ctx);
        QVERIFY(d);
        QCOMPARE(combo(d, "setup")->count(), 4); // <Không>, <Lần in trước>, 2 thiết lập có tên
        QVERIFY(spinUntil([&] { return printersLoaded(d); }));
        QComboBox* device = combo(d, "device");
        QCOMPARE(device->count(), 4); // PDF, PNG của TrionCAD + 2 máy in hệ thống
        QCOMPARE(device->itemData(0).toString(), plot::kPdfDevice.toString());
        QCOMPARE(device->itemData(1).toString(), plot::kPngDevice.toString());
        QCOMPARE(device->itemData(2).toString(), kCanon);

        // PDF, PNG: in ra tệp bắt buộc, số bản khóa; máy in hệ thống: tự chọn, có vị trí; khổ theo thiết bị.
        auto* toFile = child<QCheckBox>(d, "toFile");
        auto* copies = child<NumericField>(d, "copies");
        QVERIFY(toFile->isChecked() && !toFile->isEnabled());
        QVERIFY(!copies->isEnabled());
        QVERIFY(!child<QLabel>(d, "where")->text().isEmpty());
        QVERIFY(pick(device, kCanon));
        QVERIFY(!toFile->isChecked() && toFile->isEnabled());
        QVERIFY(copies->isEnabled());
        QCOMPARE(child<QLabel>(d, "where")->text(), u"\\\\SRV-01\\Canon-KT"_s);
        QComboBox* paper = combo(d, "paper");
        QCOMPARE(paper->count(), 2);
        QCOMPARE(paper->currentData().toString(), u"A4"_s); // A1 không có: khổ mặc định của máy in
        QVERIFY(pick(device, kHp));
        QCOMPARE(paper->currentData().toString(), u"A1"_s);
        QVERIFY(pick(device, plot::kPngDevice.toString()));
        QCOMPARE(paper->currentData().toString(), kA1); // A1 594 × 841 của HP: cùng cỡ ISO A1
        QCOMPARE(paper->count(), 6);
        QVERIFY(paper->currentText().contains(u"841 × 594 mm"_s));
        QVERIFY(toFile->isChecked() && !toFile->isEnabled());

        // Số bản 1–999.
        QVERIFY(pick(device, kCanon));
        type(copies, u"3"_s);
        QCOMPARE(d->model()->settings().copies, 3);
        type(copies, u"0"_s);
        QVERIFY(copies->hasError());
        QCOMPARE(d->model()->settings().copies, 3);
        type(copies, u"1500"_s);
        QVERIFY(copies->hasError());
        QCOMPARE(d->model()->settings().copies, 3);
        type(copies, u"999"_s);
        QCOMPARE(d->model()->settings().copies, plot::kMaxCopies);

        // Vùng in theo ngữ cảnh: layout có Layout, không có Giới hạn.
        QComboBox* area = combo(d, "area");
        QList<int> kinds;
        for (int i = 0; i < area->count(); ++i)
            kinds << area->itemData(i).toInt();
        QCOMPARE(kinds, (QList<int>{int(AreaKind::Layout), int(AreaKind::Extents), int(AreaKind::Display),
                                    int(AreaKind::View), int(AreaKind::Window)}));
        auto* center = child<QCheckBox>(d, "center");
        QVERIFY(!center->isEnabled()); // Layout: không canh giữa
        QVERIFY(pick(area, int(AreaKind::View)));
        QComboBox* view = combo(d, "view");
        QVERIFY(view->isVisible());
        QCOMPARE(view->count(), 2);
        QVERIFY(pick(view, u"Mat cat 1-1"_s));
        QCOMPARE(d->model()->settings().setup.area.view, u"Mat cat 1-1"_s);
        QVERIFY(pick(area, int(AreaKind::Window)));
        QVERIFY(!view->isVisible());
        QVERIFY(child<QWidget>(d, "pickWindow")->isVisible());
        QVERIFY(child<FieldMessage>(d, "windowMsg")->isError());
        QVERIFY(!d->confirmButton()->isEnabled()); // chưa có cửa sổ: chưa in được
        QVERIFY(pick(area, int(AreaKind::Extents)));
        QVERIFY(d->confirmButton()->isEnabled());

        // Offset: canh giữa khóa X/Y.
        QVERIFY(center->isEnabled());
        center->click();
        QVERIFY(d->model()->settings().setup.offset.centered);
        QVERIFY(!child<NumericField>(d, "offsetX")->isEnabled());
        center->click();
        type(child<NumericField>(d, "offsetX"), u"12.5"_s);
        type(child<NumericField>(d, "offsetY"), u"-3"_s);
        QCOMPARE(d->model()->settings().setup.offset.x, 12.5);
        QCOMPARE(d->model()->settings().setup.offset.y, -3.0);

        // Tỉ lệ: vừa khổ giấy, chuẩn, tùy chỉnh; co giãn độ dày nét.
        auto* fit = child<QCheckBox>(d, "fit");
        QComboBox* scale = combo(d, "scale");
        fit->click();
        QVERIFY(d->model()->settings().setup.scale.fit);
        QVERIFY(!scale->isEnabled() && !child<QWidget>(d, "customScale")->isVisible());
        fit->click();
        QVERIFY(pick(scale, 8)); // 1:100
        QCOMPARE(d->model()->settings().setup.scale.units, 100.0);
        QVERIFY(child<QLabel>(d, "scaleInfo")->text().contains(u"100"_s));
        QVERIFY(!child<QWidget>(d, "customScale")->isVisible());
        QVERIFY(pick(scale, -1)); // Tùy chỉnh: mm = đơn vị thay dòng thông tin
        QVERIFY(child<QWidget>(d, "customScale")->isVisible() && !child<QLabel>(d, "scaleInfo")->isVisible());
        type(child<NumericField>(d, "scaleMm"), u"2"_s);
        QCOMPARE(d->model()->settings().setup.scale.mm, 2.0);
        QCOMPARE(scale->currentData().toInt(), -1); // Tùy chỉnh
        child<QCheckBox>(d, "scaleLineweights")->click();
        QVERIFY(d->model()->settings().setup.scale.scaleLineweights);

        // Phần mở rộng: kiểu in, viewport có shade, 8 tùy chọn in, hướng giấy.
        d->setExpanded(true);
        QComboBox* style = combo(d, "plotStyle");
        QCOMPARE(style->count(), 4); // Không + 3 tệp
        QVERIFY(pick(style, QString()));
        QVERIFY(d->model()->settings().setup.plotStyle.isEmpty());
        QCOMPARE(combo(d, "shadeMode")->count(), 4);
        auto* dpi = child<NumericField>(d, "dpi");
        QVERIFY(!dpi->isEnabled());
        QVERIFY(pick(combo(d, "shadeQuality"), int(plot::ShadeQuality::Custom)));
        QVERIFY(dpi->isEnabled());
        for (int i = 0; i < PlotSettingsModel::kOptionCount; ++i)
            QVERIFY(option(d, Option(i)));
        QVERIFY(!option(d, Option::Lineweights)->isEnabled()); // in theo kiểu in: độ dày nét bật và khóa
        option(d, Option::PlotStyles)->click();
        QVERIFY(option(d, Option::Lineweights)->isEnabled());
        option(d, Option::Lineweights)->click();
        QVERIFY(!d->model()->settings().setup.flags.lineweights);
        QVERIFY(option(d, Option::PaperSpaceLast)->isEnabled()); // layout
        auto* orientation = child<SegmentedGroup>(d, "orientation");
        QCOMPARE(orientation->count(), 2);
        orientation->segment(0)->click();
        QCOMPARE(d->model()->settings().setup.orientation, plot::Orientation::Portrait);
        child<QCheckBox>(d, "upsideDown")->click();
        QVERIFY(d->model()->settings().setup.upsideDown);
        QVERIFY(d->previewSlot() && d->previewSlot()->isVisible()); // chỗ của UIH-002
        d->close();

        // Model: có Giới hạn, không có Layout; tùy chọn Paper space khóa.
        d = open(modelContext());
        QVERIFY(d);
        kinds.clear();
        for (int i = 0; i < combo(d, "area")->count(); ++i)
            kinds << combo(d, "area")->itemData(i).toInt();
        QCOMPARE(kinds, (QList<int>{int(AreaKind::Extents), int(AreaKind::Display), int(AreaKind::Limits),
                                    int(AreaKind::View), int(AreaKind::Window)}));
        QVERIFY(!option(d, Option::PaperSpaceLast)->isEnabled());
        QVERIFY(!option(d, Option::HidePaperSpace)->isEnabled());
    }

    // Tiêu chí 3: thiết lập có tên, <Lần in trước>, mở rộng.
    void namedSetupAndExpand()
    {
        plot::PlotContext ctx = modelContext();
        plot::PageSetup previous = a1Pdf();
        previous.name.clear();
        previous.area.kind = AreaKind::Window;
        previous.area.window = QRectF(0, 0, 420, 297);
        previous.offset = {false, 5, 7};
        ctx.previous = previous;
        QPointer<PlotDialog> d = open(ctx);
        QVERIFY(d);
        QVERIFY(spinUntil([&] { return printersLoaded(d); }));
        QComboBox* setup = combo(d, "setup");
        QVERIFY(pick(setup, u"A3-Phong-KT"_s));
        plot::PlotSettings want;
        want.setup = a3Canon();
        plot::normalize(want, ctx, d->model()->devices());
        QVERIFY(d->model()->settings().setup == want.setup);
        QCOMPARE(combo(d, "device")->currentData().toString(), kCanon);
        QCOMPARE(combo(d, "paper")->currentData().toString(), u"A3"_s);
        QVERIFY(child<QCheckBox>(d, "center")->isChecked());
        QVERIFY(pick(setup, PlotSettingsModel::kPreviousSetup));
        QCOMPARE(d->model()->settings().setup.area.kind, AreaKind::Window); // Model: vùng Layout của layout không hợp
        QCOMPARE(d->model()->settings().setup.area.window, QRectF(0, 0, 420, 297));
        QCOMPARE(d->model()->settings().setup.offset.x, 5.0);
        const plot::PlotSettings before = d->model()->settings();
        QVERIFY(pick(setup, PlotSettingsModel::kNoSetup)); // <Không>: giữ giá trị
        QVERIFY(d->model()->settings() == before);

        // Mở rộng: cùng widget, giữ giá trị, nhớ trong Store; mép trái đứng yên.
        type(child<NumericField>(d, "offsetY"), u"9"_s);
        QWidget* more = child<QWidget>(d, "more");
        QWidget* copiesField = child<NumericField>(d, "copies");
        const QRect closed = d->geometry();
        QVERIFY(!more->isVisible());
        child<QAbstractButton>(d, "moreOptions")->click();
        QVERIFY(d->isExpanded() && more->isVisible());
        QVERIFY(store_->get(kPlotMore));
        QCOMPARE(child<QWidget>(d, "more"), more);
        QCOMPARE(child<NumericField>(d, "copies"), copiesField);
        QCOMPARE(child<NumericField>(d, "offsetY")->value(), 9.0);
        QCOMPARE(d->model()->settings().setup.offset.y, 9.0);
        QVERIFY(d->geometry().width() > closed.width());
        QCOMPARE(d->geometry().left(), closed.left());
        d->close();
        d = open(modelContext());
        QVERIFY(d && d->isExpanded()); // nhớ theo người dùng
        child<QAbstractButton>(d, "moreOptions")->click();
        QVERIFY(!d->isExpanded());
        QVERIFY(!store_->get(kPlotMore));
    }

    // Tiêu chí 4: vùng in Cửa sổ.
    void windowPick()
    {
        QPointer<PlotDialog> d = open(modelContext());
        QVERIFY(d);
        QVERIFY(pick(combo(d, "area"), int(AreaKind::Window)));
        QSignalSpy asked(d.data(), &PlotDialog::windowPickRequested);
        child<QAbstractButton>(d, "pickWindow")->click();
        QCOMPARE(asked.count(), 1);
        QVERIFY(d->isHiddenForPick() && !d->isVisible());
        d->finishWindowPick({100, 200}, {10, 20}); // INT-003: 2 góc, thứ tự bất kỳ
        QVERIFY(!d->isHiddenForPick() && d->isVisible());
        QCOMPARE(d->model()->settings().setup.area.window, QRectF(10, 20, 90, 180));
        const QString where = child<FieldMessage>(d, "windowMsg")->text();
        QVERIFY2(where.contains(u"(10.00, 20.00)"_s) && where.contains(u"(100.00, 200.00)"_s), qPrintable(where));
        QVERIFY(d->confirmButton()->isEnabled());
        child<QAbstractButton>(d, "pickWindow")->click();
        QVERIFY(d->isHiddenForPick());
        d->cancelWindowPick(); // Esc trên vùng vẽ
        QVERIFY(d->isVisible());
        QCOMPARE(d->model()->settings().setup.area.window, QRectF(10, 20, 90, 180));
        d->finishWindowPick({0, 0}, {1, 1}); // không đang chọn: bỏ qua
        QCOMPARE(d->model()->settings().setup.area.window, QRectF(10, 20, 90, 180));
    }

    // Tiêu chí 5: in nền, tiến trình, toast "Đã in ra" kèm Mở thư mục.
    void plotRunsInBackground()
    {
        QPointer<PlotDialog> d = open(layoutContext());
        QVERIFY(d);
        std::vector<jobs::State> ended;
        connect(runner_.get(), &PlotRunner::finished, this, [&](jobs::JobId, jobs::State s) { ended.push_back(s); });
        d->confirmButton()->click();
        QVERIFY(!d || !d->isVisible()); // đóng ngay
        QCOMPARE(jobs_->size(), std::size_t(1));
        QVERIFY(spinUntil([&] { return presenter_->model()->rowCount() == 1; }));
        const QString label = presenter_->model()->index(0).data().toString();
        QVERIFY2(QRegularExpression(u"^Đang in 1/1 · \\d+%$"_s).match(label).hasMatch(), qPrintable(label)); // i18n-ok: chữ kiểm
        QVERIFY(spinUntil([&] { return !ended.empty(); }));
        QCOMPARE(ended.front(), jobs::State::Done);
        QVERIFY(runner_->previous() && runner_->previous()->paper == kA1); // <Lần in trước>
        QVERIFY(!toasts_->toasts().isEmpty());
        Toast* t = toasts_->toasts().last();
        QCOMPARE(t->text(), u"Đã in ra NhaO-2T-A1-KT.pdf"_s); // i18n-ok: chữ kiểm
        QVERIFY(t->actionButton());
        QCOMPARE(t->actionButton()->text(), u"Mở thư mục"_s); // i18n-ok: chữ kiểm
        t->actionButton()->click();
        QCOMPARE(opened_, QStringList{QDir(dir_.path()).absolutePath()});
    }

    void plotToPrinter()
    {
        QPointer<PlotDialog> d = open(modelContext());
        QVERIFY(d);
        QVERIFY(spinUntil([&] { return printersLoaded(d); }));
        QVERIFY(pick(combo(d, "device"), kCanon));
        type(child<NumericField>(d, "copies"), u"2"_s);
        bool done = false;
        connect(runner_.get(), &PlotRunner::finished, this, [&] { done = true; });
        d->confirmButton()->click();
        QVERIFY(spinUntil([&] { return done; }));
        QVERIFY(!toasts_->toasts().isEmpty());
        QCOMPARE(toasts_->toasts().last()->text(), u"Đã gửi 2 tờ tới %1"_s.arg(kCanon)); // i18n-ok: chữ kiểm
        QVERIFY(!toasts_->toasts().last()->actionButton());
    }

    // Tiêu chí 5: hủy dừng ≤ 50 ms, không để tệp.
    void cancelPlot()
    {
        std::atomic<Clock::rep> stopped{0};
        runner_->setPipeline([&stopped](const plot::PlotSettings& s, jobs::Progress& p, sched::CancelToken c) {
            auto r = PlotRunner::fakePipeline(20s)(s, p, c);
            stopped = Clock::now().time_since_epoch().count();
            return r;
        });
        jobs::JobId id = 0;
        connect(jobs_.get(), &jobs::BackgroundJobs::updated, this, [&](const std::vector<jobs::JobInfo>& batch) {
            for (const jobs::JobInfo& j : batch)
                if (j.progress > 0)
                    id = j.id;
        });
        std::vector<jobs::State> ended;
        connect(runner_.get(), &PlotRunner::finished, this, [&](jobs::JobId, jobs::State s) { ended.push_back(s); });
        QPointer<PlotDialog> d = open(layoutContext());
        QVERIFY(d);
        d->confirmButton()->click();
        QVERIFY(spinUntil([&] { return id != 0; }));
        const Clock::time_point asked = Clock::now();
        QVERIFY(jobs_->cancel(id));
        QVERIFY(spinUntil([&] { return stopped != 0; }));
        const double latency = ms(Clock::time_point(Clock::duration(stopped.load())) - asked);
        qInfo("cancel → pipeline dừng: %.1f ms", latency);
        QVERIFY2(!test::kTimed || latency <= 50, qPrintable(QString::number(latency)));
        QVERIFY(spinUntil([&] { return !ended.empty(); }));
        QCOMPARE(ended.front(), jobs::State::Cancelled);
        QCOMPARE(toasts_->toasts().last()->text(), u"Đã hủy in."_s); // i18n-ok: chữ kiểm
        QVERIFY(QDir(dir_.path()).entryList({u"*.pdf"_s, u"*.png"_s, u"*.plt"_s}, QDir::Files).isEmpty());
    }

    // Tiêu chí 6 (phần kiểm được): cùng PlotSettings từ hộp thoại và từ -PLOT.
    void scriptMatchesDialog()
    {
        QPointer<PlotDialog> d = open(modelContext());
        QVERIFY(d);
        QVERIFY(spinUntil([&] { return printersLoaded(d); }));
        d->setExpanded(true);
        QVERIFY(pick(combo(d, "paper"), kA1));
        child<SegmentedGroup>(d, "orientation")->segment(0)->click();
        child<QCheckBox>(d, "upsideDown")->click();
        QVERIFY(pick(combo(d, "area"), int(AreaKind::Extents)));
        child<QCheckBox>(d, "fit")->click();
        QVERIFY(pick(combo(d, "scale"), 8)); // 1:100
        child<QCheckBox>(d, "center")->click();
        QVERIFY(pick(combo(d, "plotStyle"), u"monochrome.ctb"_s));
        QVERIFY(pick(combo(d, "shadeMode"), int(plot::ShadeMode::Hidden)));
        const QStringList model = {u"Y"_s, u"Model"_s, u"TrionCAD PDF"_s, u"ISO A1"_s, u"M"_s, u"P"_s, u"Y"_s,
                                   u"E"_s, u"1:100"_s, u"C"_s, u"Y"_s, u"monochrome.ctb"_s, u"Y"_s, u"H"_s, {},
                                   u"N"_s, u"Y"_s};
        auto parsed = plot::parsePlotCommand(model, d->model()->context(), d->model()->devices());
        QVERIFY2(parsed, parsed ? "" : parsed.error().message.c_str());
        QCOMPARE(describe(*parsed), describe(d->model()->settings()));
        QVERIFY(*parsed == d->model()->settings());
        d->close();

        d = open(layoutContext());
        QVERIFY(d);
        QVERIFY(spinUntil([&] { return printersLoaded(d); }));
        d->setExpanded(true);
        QVERIFY(pick(combo(d, "device"), plot::kPngDevice.toString()));
        QVERIFY(pick(combo(d, "scale"), -1));
        type(child<NumericField>(d, "scaleUnits"), u"50"_s);
        type(child<NumericField>(d, "offsetX"), u"5"_s);
        type(child<NumericField>(d, "offsetY"), u"10"_s);
        QVERIFY(pick(combo(d, "area"), int(AreaKind::Window)));
        child<QAbstractButton>(d, "pickWindow")->click();
        d->finishWindowPick({420, 297}, {0, 0});
        option(d, Option::PlotStyles)->click();
        option(d, Option::Lineweights)->click();
        QVERIFY(pick(combo(d, "plotStyle"), QString()));
        child<QCheckBox>(d, "scaleLineweights")->click();
        option(d, Option::HidePaperSpace)->click();
        const QStringList layout = {u"y"_s, {}, u"trioncad png"_s, {}, u"M"_s, u"L"_s, u"N"_s, u"W"_s, u"0,0"_s,
                                    u"420,297"_s, u"1=50"_s, u"5,10"_s, u"N"_s, u"."_s, u"N"_s, u"Y"_s, u"N"_s, u"Y"_s,
                                    {}, u"N"_s, u"Y"_s};
        parsed = plot::parsePlotCommand(layout, d->model()->context(), d->model()->devices());
        QVERIFY2(parsed, parsed ? "" : parsed.error().message.c_str());
        QCOMPARE(describe(*parsed), describe(d->model()->settings()));
        QVERIFY(*parsed == d->model()->settings());

        // Lỗi: thiết bị lạ, câu đầu No, thiếu trả lời.
        QStringList bad = layout;
        bad[2] = u"Plotter X"_s;
        QVERIFY(!plot::parsePlotCommand(bad, d->model()->context(), d->model()->devices()));
        bad = layout;
        bad[0] = u"N"_s;
        QVERIFY(!plot::parsePlotCommand(bad, d->model()->context(), d->model()->devices()));
        QVERIFY(!plot::parsePlotCommand(layout.mid(0, 10), d->model()->context(), d->model()->devices()));
    }

    // Tiêu chí 7: Viewer, Viewer Pro in đủ; phần Pro có huy hiệu và mở trang nâng cấp.
    void editionBadges_data()
    {
        QTest::addColumn<int>("edition");
        QTest::newRow("viewer") << int(edition::Edition::Viewer);
        QTest::newRow("viewer-pro") << int(edition::Edition::ViewerPro);
    }
    void editionBadges()
    {
        QFETCH(int, edition);
        gate_->setEdition(edition::Edition(edition));
        store_->set(kPlotMore, true);
        QPointer<PlotDialog> d = open(layoutContext());
        QVERIFY(d);
        const auto badges = [&] {
            int n = 0;
            for (const auto* b : d->findChildren<edition::LockBadge*>())
                n += b->isVisibleTo(d) ? 1 : 0;
            return n;
        };
        QCOMPARE(badges(), 5);
        QCOMPARE(RichTooltip::info(child<QWidget>(d, "deviceProps")).command, u"PLOTTERMANAGER"_s); // tooltip, F1
        QCOMPARE(RichTooltip::info(child<QWidget>(d, "deviceProps")).need, Edition::Pro);
        QCOMPARE(RichTooltip::info(child<QWidget>(d, "editStyle")).command, u"STYLESMANAGER"_s);
        QCOMPARE(RichTooltip::info(child<QWidget>(d, "preview")).command, u"PREVIEW"_s);
        for (const char* name : {"device", "paper", "area", "scale", "plotStyle"})
            QVERIFY2(combo(d, name)->isEnabled(), name);
        QSignalSpy add(d.data(), &PlotDialog::addSetupRequested);
        QSignalSpy apply(d.data(), &PlotDialog::applyToLayoutRequested);
        for (const char* name : {"addSetup", "deviceProps", "editStyle", "apply"}) {
            QWidget* b = child<QWidget>(d, name);
            QCOMPARE(b->accessibleDescription(), edition::EditionGate::availableIn(Edition::Pro));
            child<QAbstractButton>(d, name)->click();
        }
        option(d, Option::SaveToLayout)->click();
        QVERIFY(!option(d, Option::SaveToLayout)->isChecked());
        QCOMPARE(upgrades_, (QStringList{u"pagesetup-add"_s, u"PLOTTERMANAGER"_s, u"STYLESMANAGER"_s,
                                         u"pagesetup-apply"_s, u"pagesetup-save"_s}));
        QCOMPARE(add.count(), 0);
        QCOMPARE(apply.count(), 0);
        d->confirmButton()->click(); // in vẫn được
        QCOMPARE(jobs_->size(), std::size_t(1));

        // Pro: huy hiệu ẩn, nút phát yêu cầu cho integration.
        gate_->setEdition(edition::Edition::Pro);
        d = open(layoutContext());
        QVERIFY(d);
        QCOMPARE(badges(), 0);
        QCOMPARE(RichTooltip::info(child<QWidget>(d, "deviceProps")).need, Edition::None);
        QSignalSpy add2(d.data(), &PlotDialog::addSetupRequested);
        QSignalSpy apply2(d.data(), &PlotDialog::applyToLayoutRequested);
        QSignalSpy style(d.data(), &PlotDialog::editPlotStyleRequested);
        QSignalSpy props(d.data(), &PlotDialog::printerPropertiesRequested);
        for (const char* name : {"addSetup", "deviceProps", "editStyle", "apply"})
            child<QAbstractButton>(d, name)->click();
        QCOMPARE(add2.count(), 1);
        QCOMPARE(style.count(), 1);
        QCOMPARE(props.count(), 1);
        QCOMPARE(apply2.count(), 1);
        option(d, Option::SaveToLayout)->click();
        QVERIFY(option(d, Option::SaveToLayout)->isChecked());
        d->confirmButton()->click();
        QCOMPARE(apply2.count(), 2); // Lưu thay đổi vào layout khi In
        QCOMPARE(upgrades_.size(), 5);
    }

    // Tiêu chí 8: mở ≤ 150 ms, đổi một trường ≤ 16 ms (CPU luồng UI; kiểm ở release).
    void timing()
    {
        double opening = 0;
        for (int round = 0; round < 3; ++round) { // vòng tốt nhất: vòng đầu nạp font, biểu tượng
            const auto t0 = test::threadCpu();
            auto* d = new PlotDialog(layoutContext(), runner_.get(), window_.get());
            FirstFrame frame;
            d->installEventFilter(&frame);
            d->open();
            QVERIFY(spinUntil([&] { return frame.done; }));
            const double t = t0.count() < 0 ? 0 : ms(frame.cpu - t0);
            opening = round == 0 ? t : std::min(opening, t);
            d->removeEventFilter(&frame);
            delete d;
        }
        QPointer<PlotDialog> d = open(layoutContext());
        QVERIFY(d);
        d->setExpanded(true);
        QVERIFY(spinUntil([&] { return printersLoaded(d); }));
        QCoreApplication::processEvents();
        double change = 0;
        const auto measure = [&](const std::function<void()>& f) {
            const auto t0 = test::threadCpu();
            f();
            QCoreApplication::processEvents(); // vẽ khung kế tiếp
            if (t0.count() >= 0)
                change = std::max(change, ms(test::threadCpu() - t0));
        };
        measure([&] { pick(combo(d, "device"), kCanon); });
        measure([&] { pick(combo(d, "paper"), u"A3"_s); });
        measure([&] { pick(combo(d, "area"), int(AreaKind::Extents)); });
        measure([&] { child<QCheckBox>(d, "fit")->click(); });
        measure([&] { pick(combo(d, "plotStyle"), u"grayscale.ctb"_s); });
        measure([&] { option(d, Option::Transparency)->click(); });
        measure([&] { pick(combo(d, "setup"), u"A1-PDF"_s); });
        qInfo("mở: %.1f ms CPU, đổi trường tối đa: %.1f ms CPU", opening, change);
        QVERIFY2(!test::kTimed || opening <= 150, qPrintable(QString::number(opening)));
        QVERIFY2(!test::kTimed || change <= 16, qPrintable(QString::number(change)));
    }

    // Máy in hệ thống quét nền: mở không chờ; đang tải; máy in vắng thì báo lỗi.
    void printersInBackground()
    {
        plot::setPrinterSource([] { return QList<plot::Device>{}; });
        plot::scanPrinters(); // cache rỗng: như lần mở đầu của phiên
        release_ = false;
        plot::setPrinterSource([this] {
            while (!release_)
                std::this_thread::sleep_for(2ms);
            return samplePrinters();
        });
        plot::PlotContext ctx = layoutContext();
        ctx.layoutSetup->device = u"Old Plotter"_s;
        QPointer<PlotDialog> d = open(ctx); // quét đang chặn ở nền: hộp vẫn mở
        QVERIFY(d);
        QComboBox* device = combo(d, "device");
        QCOMPARE(device->count(), 3); // 2 thiết bị dựng sẵn + máy in đang đặt
        auto* msg = child<FieldMessage>(d, "deviceMsg");
        QVERIFY(msg->isVisible() && !msg->isError()); // "Đang tìm máy in…"
        release_ = true;
        QVERIFY(spinUntil([&] { return printersLoaded(d); }));
        QCOMPARE(device->count(), 5);
        QVERIFY(msg->isVisible() && msg->isError());
        QVERIFY(msg->text().contains(u"Old Plotter"_s));
        QVERIFY(pick(device, kHp));
        QVERIFY(!msg->isVisible());

        // Nguồn mặc định (QPrinterInfo, hoặc rỗng khi build không có PrintSupport) chạy được ở nền.
        plot::setPrinterSource({});
        std::atomic<bool> scanned{false};
        sched::submit(sched::Priority::Background, [&scanned] {
            (void)plot::scanPrinters();
            scanned = true;
        });
        QVERIFY(spinUntil([&] { return scanned.load(); }, 30s));
    }

    // DoD-U: Enter in, Esc đóng; Cửa sổ chưa chọn thì Enter không in.
    void keyboard()
    {
        QPointer<PlotDialog> d = open(layoutContext());
        QVERIFY(d);
        d->activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(d));
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Escape);
        QVERIFY(spinUntil([&] { return !d || !d->isVisible(); }));
        QCOMPARE(jobs_->size(), std::size_t(0));

        d = open(layoutContext());
        QVERIFY(d);
        d->activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(d));
        QVERIFY(pick(combo(d, "area"), int(AreaKind::Window)));
        combo(d, "setup")->setFocus(Qt::TabFocusReason);
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Return);
        QVERIFY(d && d->isVisible()); // chưa có cửa sổ: không in
        QVERIFY(pick(combo(d, "area"), int(AreaKind::Layout)));
        combo(d, "setup")->setFocus(Qt::TabFocusReason);
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Return);
        QVERIFY(spinUntil([&] { return !d || !d->isVisible(); }));
        QCOMPARE(jobs_->size(), std::size_t(1));
    }

    // DoD-U: tên, vai trò, thứ tự Tab, viền focus (hộp gọn và mở rộng).
    void accessibility()
    {
        for (const bool more : {false, true}) {
            store_->set(kPlotMore, more);
            QPointer<PlotDialog> d = open(layoutContext());
            QVERIFY(d);
            QVERIFY(spinUntil([&] { return printersLoaded(d); }));
            d->activateWindow();
            QVERIFY(QTest::qWaitForWindowActive(d));
            const QStringList issues = a11y::audit(d);
            QVERIFY2(issues.isEmpty(), qPrintable(issues.join(u'\n')));
            d->close();
        }
    }

    // DoD-U: vi-XL không cắt chữ (hộp gọn, mở rộng, dòng lỗi máy in vắng và cửa sổ chưa chọn).
    void pseudoNoTruncation()
    {
        QVERIFY(translator_->setLanguage(i18n::kPseudoLanguage.toString()));
        QVERIFY(spinUntil([] { return QCoreApplication::translate("trion::ui::PlotDialog", "In").size() > 2; })); // i18n-ok: khóa dịch
        plot::PlotContext ctx = layoutContext();
        ctx.layoutSetup->device = u"Old Plotter"_s;
        QPointer<PlotDialog> d = open(ctx);
        QVERIFY(d);
        QVERIFY(spinUntil([&] { return printersLoaded(d); }));
        const QRect screen = d->screen()->availableGeometry();
        qInfo("vi-XL gọn: %d × %d", d->width(), d->height());
        QVERIFY(d->height() <= screen.height() && d->width() <= screen.width());
        QCOMPARE(truncations(d), QString());
        QVERIFY(pick(combo(d, "area"), int(AreaKind::Window)));
        d->setExpanded(true);
        QCoreApplication::processEvents();
        qInfo("vi-XL mở rộng: %d × %d", d->width(), d->height());
        QVERIFY(d->height() <= screen.height() && d->width() <= screen.width());
        QCOMPARE(truncations(d), QString());
    }
};

QTEST_MAIN(PlotDialogTest)
#include "plot_dialog_test.moc"
