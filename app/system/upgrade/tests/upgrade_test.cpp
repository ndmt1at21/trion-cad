// UIL-014: trang nâng cấp UI-L04. Mỗi tiêu chí nghiệm thu một nhóm hàm test:
//   1  contentMatchesJson: "Có trong TrionCAD <phiên bản>", tên tính năng, lợi ích, ảnh, giá "Từ … đ/năm" và dòng phụ
//      đúng theo JSON (test tự đọc :/trion/upgrade/upgrade-content.json), cả vi và en. buttons: Mua mở trang mua chọn
//      sẵn gói (#pro, #vp, mã tính năng) rồi đóng; không mở được trình duyệt thì ở lại, báo lỗi kèm địa chỉ; Để sau, Esc
//      đóng không mở gì; Enter = Mua; Kích hoạt, Dùng thử ẩn tới khi có hook, có thì gọi hook (Dùng thử là nút chính),
//      Dùng thử ẩn khi đang dùng thử.
//   2  oncePerSession: qua EditionGate::upgrade().request: lần đầu mở trang, lần sau không mở mà toast "X có trong
//      TrionCAD Y." kèm "Xem"; "Xem" mở lại trang; tính năng khác vẫn mở trang.
//   3  contentFile: bản nhúng đọc được, đủ vi/en, ảnh có trong bộ biểu tượng, phiên bản cần khớp danh mục lệnh; parse báo
//      lỗi JSON hỏng, thiếu mục. genericFallback: tính năng thiếu mục dùng nội dung chung của phiên bản; need None lấy
//      theo mục của tính năng.
//   4  openTime: CPU luồng UI từ lúc gọi trang đến khung đầu có nội dung, 20 lần (lần đầu gồm đọc JSON, raster ảnh);
//      ngưỡng 150 ms chỉ kiểm ở release (tests/timing.h).
//   5  DoD-U: pseudoNoTruncation (vi-XL và English, đủ nút), accessibility (a11y::audit: tên, vai trò, Tab, viền focus),
//      illustration (ảnh không chữ: nền --c-canvas, hình --c-accent, Tối và Sáng).
#include "app/system/upgrade/UpgradeDialog.h"

#include "app/core/a11y/AccessibilityAudit.h"
#include "app/core/actions/ActionRegistry.h"
#include "app/core/i18n/Translator.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/core/i18n/UiFormat.h"
#include "app/core/UiExecutor.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/message/Messages.h"
#include "core/base/sched/Scheduler.h"
#include "tests/timing.h"

#include <QApplication>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrlQuery>
#include <QtTest>

#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;
using namespace trion;
using edition::EditionGate;
using edition::UpgradeRequest;
using edition::UpgradeRouter;
using upgrade::UpgradeContent;
using upgrade::UpgradeDialog;
using upgrade::UpgradePage;

namespace {

using Clock = std::chrono::steady_clock;
using trion::test::kTimed;

constexpr double kOpenBudgetMs = 150; // DoD-L "mở hộp thoại đến khung đầu có nội dung", máy tầm trung

// Tên tính năng đã dịch do bên gọi đưa (EditionGate::bind: tên của QAction).
constexpr char kSaveName[] = "Lưu bản vẽ";                    // i18n-ok: chữ mẫu của test
constexpr char kBatchName[] = "In hàng loạt theo khung tên"; // i18n-ok: chữ mẫu của test
constexpr char kOverkillName[] = "Xóa trùng lặp";           // i18n-ok: chữ mẫu của test
constexpr char kLengthName[] = "Tổng chiều dài";            // i18n-ok: chữ mẫu của test

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

bool waitActive(QWidget* w)
{
    return spinUntil([w] { return QApplication::activeWindow() == w->window(); });
}

double ms(std::chrono::nanoseconds d)
{
    return std::chrono::duration<double, std::milli>(d).count();
}

QJsonObject contentJson()
{
    QFile f(u":/trion/upgrade/upgrade-content.json"_s);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

QString editionKey(ui::Edition e)
{
    return e == ui::Edition::ViewerPro ? u"ViewerPro"_s : u"Pro"_s;
}

QString truncations(QWidget* w)
{
    QStringList out;
    for (const i18n::Truncation& t : i18n::findTruncations(w))
        out << t.toString();
    return out.join(u"; "_s);
}

// Đếm lần vẽ của một widget.
struct PaintSpy final : QObject {
    int paints = 0;
    bool eventFilter(QObject*, QEvent* e) override
    {
        if (e->type() == QEvent::Paint)
            ++paints;
        return false;
    }
};

} // namespace

class UpgradeTest : public QObject {
    Q_OBJECT
    QTemporaryDir dir_;
    std::unique_ptr<settings::Store> store_;
    std::unique_ptr<ui::UiExecutor> executor_;
    std::unique_ptr<theme::ThemeManager> tm_;
    std::unique_ptr<theme::IconProvider> icons_;
    std::unique_ptr<i18n::Translator> translator_;
    std::unique_ptr<QWidget> main_;
    std::unique_ptr<ui::NotificationCenter> notes_;
    std::unique_ptr<ui::ToastHost> toasts_;
    std::unique_ptr<EditionGate> gate_;
    std::unique_ptr<UpgradePage> page_;
    QLineEdit* cmd_ = nullptr;
    QList<QUrl> opened_;            // địa chỉ Mua đã mở
    bool openOk_ = true;            // trình duyệt giả mở được
    QList<UpgradeRequest> activated_, trials_;
    QJsonObject json_;

    UpgradeDialog* openPage(const QString& feature, ui::Edition need, const char* name)
    {
        gate_->upgrade().open({feature, need, QString::fromUtf8(name)});
        UpgradeDialog* d = page_->dialog();
        return d && waitActive(d) ? d : nullptr;
    }

    template <class T = QLabel>
    static T* child(QWidget* d, const QString& name)
    {
        return d->findChild<T*>(name);
    }

    static QString text(QWidget* d, const QString& name) { return child(d, name)->text(); }

    static bool closed(const QPointer<UpgradeDialog>& d)
    {
        return spinUntil([&] { return !d; }); // WA_DeleteOnClose
    }

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
        main_ = std::make_unique<QWidget>();
        main_->resize(1100, 760);
        main_->move(40, 40);
        cmd_ = new QLineEdit(main_.get()); // như dòng lệnh: focus trước khi mở trang, phải về lại khi đóng
        cmd_->setGeometry(20, 700, 600, 26);
        notes_ = std::make_unique<ui::NotificationCenter>();
        toasts_ = std::make_unique<ui::ToastHost>(main_.get(), *notes_);
        toasts_->setTimeout(1h);
        main_->show();
        main_->activateWindow();
        QVERIFY(waitActive(main_.get()));
        json_ = contentJson();
        QVERIFY(!json_.isEmpty());
    }

    void init()
    {
        tm_->setTheme(theme::Theme::Dark);
        gate_ = std::make_unique<EditionGate>(); // Viewer; mỗi test một phiên mới của UpgradeRouter
        page_ = std::make_unique<UpgradePage>(*gate_, main_.get(), toasts_.get());
        page_->setUrlOpener([this](const QUrl& u) {
            opened_ << u;
            return openOk_;
        });
        opened_.clear();
        activated_.clear();
        trials_.clear();
        openOk_ = true;
        main_->activateWindow();
        QVERIFY(waitActive(main_.get()));
    }

    void cleanup()
    {
        page_.reset(); // đóng trang đang mở
        gate_.reset();
        for (ui::Toast* t : toasts_->toasts())
            t->closeButton()->click();
        QCoreApplication::processEvents();
        if (translator_->language() != u"vi"_s) {
            QVERIFY(translator_->setLanguage(u"vi"_s));
            QCoreApplication::processEvents();
        }
    }

    void cleanupTestCase()
    {
        toasts_.reset();
        notes_.reset();
        main_.reset();
        translator_.reset();
        icons_.reset();
        tm_.reset();
        settings::Store::setInstance(nullptr);
        store_.reset();
        sched::waitIdle();
        QCoreApplication::processEvents();
        executor_.reset();
    }

    // Tiêu chí 3: tệp nội dung.
    void contentFile()
    {
        const Expected<UpgradeContent>& c = UpgradeContent::builtin();
        QVERIFY2(c, c ? "" : c.error().message.c_str());
        const QStringList icons = theme::IconProvider::names();
        const QJsonObject editions = json_["editions"_L1].toObject();
        for (const QString& key : {u"Pro"_s, u"ViewerPro"_s}) {
            const QJsonObject e = editions[key].toObject();
            QVERIFY2(icons.contains(e["image"_L1].toString()), qPrintable(key));
            QVERIFY(e["price"_L1]["year"_L1].toInteger() > 0);
            QVERIFY(!e["benefit"_L1]["vi"_L1].toString().isEmpty());
            QVERIFY(!e["benefit"_L1]["en"_L1].toString().isEmpty());
        }
        QHash<QString, QString> catalog; // lệnh → bản cần trong danh mục (ActionRegistry)
        for (const actions::CommandInfo& cmd : actions::commands())
            catalog.insert(QString::fromLatin1(cmd.name), QString::fromLatin1(cmd.need));
        const QJsonObject features = json_["features"_L1].toObject();
        QStringList keys = features.keys();
        std::sort(keys.begin(), keys.end());
        QCOMPARE(c->features(), keys);
        QVERIFY(keys.size() >= 10);
        for (const QString& f : keys) {
            const QJsonObject x = features[f].toObject();
            const QString need = x["need"_L1].toString();
            QVERIFY2(icons.contains(x["image"_L1].toString()), qPrintable(f));
            QVERIFY2(!x["benefit"_L1]["vi"_L1].toString().isEmpty() && !x["benefit"_L1]["en"_L1].toString().isEmpty(),
                     qPrintable(f));
            if (catalog.contains(f)) // mã là lệnh: phiên bản cần khớp danh mục (khóa thật do EditionGate theo danh mục)
                QCOMPARE(need, catalog[f] == "VP"_L1 ? u"ViewerPro"_s : u"Pro"_s);
        }

        // Lỗi đọc: JSON hỏng, thiếu phiên bản, thiếu bản English, need lạ.
        QVERIFY(UpgradeContent::parse("{").error().code == "content");
        QJsonObject broken = json_;
        QJsonObject e = broken["editions"_L1].toObject();
        e.remove(u"ViewerPro"_s);
        broken[u"editions"_s] = e;
        QVERIFY(!UpgradeContent::parse(QJsonDocument(broken).toJson()));
        broken = json_;
        QJsonObject fs = broken["features"_L1].toObject();
        QJsonObject save = fs[u"QSAVE"_s].toObject();
        save[u"benefit"_s] = QJsonObject{{u"vi"_s, u"x"_s}};
        fs[u"QSAVE"_s] = save;
        broken[u"features"_s] = fs;
        QVERIFY(!UpgradeContent::parse(QJsonDocument(broken).toJson()));
        save[u"benefit"_s] = QJsonObject{{u"vi"_s, u"x"_s}, {u"en"_s, u"x"_s}};
        save[u"need"_s] = u"Max"_s;
        fs[u"QSAVE"_s] = save;
        broken[u"features"_s] = fs;
        QVERIFY(!UpgradeContent::parse(QJsonDocument(broken).toJson()));
    }

    // Tiêu chí 1, 3: nội dung theo JSON, vi và en.
    void contentMatchesJson_data()
    {
        QTest::addColumn<QString>("feature");
        QTest::addColumn<int>("needValue");
        QTest::addColumn<QString>("name");
        QTest::newRow("pro") << u"QSAVE"_s << int(ui::Edition::Pro) << QString::fromUtf8(kSaveName);
        QTest::newRow("viewer-pro") << u"TCBATCHPLOT"_s << int(ui::Edition::ViewerPro)
                                    << QString::fromUtf8(kBatchName);
    }

    void contentMatchesJson()
    {
        QFETCH(QString, feature);
        QFETCH(int, needValue);
        QFETCH(QString, name);
        const auto need = ui::Edition(needValue);
        gate_->upgrade().open({feature, need, name});
        QPointer<UpgradeDialog> d = page_->dialog();
        QVERIFY(d && waitActive(d));
        const QJsonObject f = json_["features"_L1][feature].toObject();
        const QJsonObject e = json_["editions"_L1][editionKey(need)].toObject();
        const QJsonObject price = e["price"_L1].toObject();
        const auto check = [&](const QString& lang) {
            QCOMPARE(text(d, u"tag"_s), EditionGate::availableIn(need)); // "Có trong TrionCAD Pro"
            QCOMPARE(text(d, u"feature"_s), name);
            QCOMPARE(text(d, u"benefit"_s), f["benefit"_L1][lang].toString());
            QCOMPARE(child<QWidget>(d, u"illustration"_s)->property("image").toString(), f["image"_L1].toString());
            QVERIFY(text(d, u"price"_s).contains(i18n::formatCount(price["year"_L1].toInteger())));
            const QString note = text(d, u"priceNote"_s);
            for (const char* k : {"month", "perpetual"}) {
                const qint64 v = price[QLatin1StringView(k)].toInteger();
                QCOMPARE(v > 0 && note.contains(i18n::formatCount(v)), v > 0);
            }
            QVERIFY(!d->content().generic);
        };
        check(u"vi"_s);
        QVERIFY(text(d, u"price"_s).startsWith(u"Từ "_s)); // i18n-ok: chuỗi nguồn
        const QString vi = text(d, u"benefit"_s);
        QVERIFY(translator_->setLanguage(u"en"_s));
        QVERIFY(spinUntil([&] { return text(d, u"benefit"_s) != vi; })); // LanguageChange đăng hàng đợi
        check(u"en"_s);
        QVERIFY(translator_->setLanguage(u"vi"_s));
        QVERIFY(spinUntil([&] { return text(d, u"benefit"_s) == vi; }));
        qInfo("%s: %s | %s | %s", qPrintable(feature), qPrintable(text(d, u"tag"_s)), qPrintable(text(d, u"price"_s)),
              qPrintable(text(d, u"priceNote"_s)));
    }

    // Tiêu chí 3: tính năng thiếu mục dùng nội dung chung của phiên bản; need None theo mục của tính năng.
    void genericFallback()
    {
        const QJsonObject editions = json_["editions"_L1].toObject();
        struct Case {
            QString feature;
            ui::Edition need;
            const char* name;
        };
        for (const Case& c : {Case{u"OVERKILL"_s, ui::Edition::Pro, kOverkillName},
                              Case{u"TCLENGTH"_s, ui::Edition::ViewerPro, kLengthName}}) {
            QVERIFY(!json_["features"_L1].toObject().contains(c.feature));
            UpgradeDialog* d = openPage(c.feature, c.need, c.name);
            QVERIFY(d);
            const QJsonObject e = editions[editionKey(c.need)].toObject();
            QCOMPARE(text(d, u"tag"_s), EditionGate::availableIn(c.need));
            QCOMPARE(text(d, u"benefit"_s), e["benefit"_L1]["vi"_L1].toString());
            QCOMPARE(child<QWidget>(d, u"illustration"_s)->property("image").toString(), e["image"_L1].toString());
            QVERIFY(text(d, u"price"_s).contains(i18n::formatCount(e["price"_L1]["year"_L1].toInteger())));
            QVERIFY(d->content().generic);
            QPointer<UpgradeDialog> p = d;
            d->reject();
            QVERIFY(closed(p));
        }
        // need None (bên gọi không biết bản cần): theo mục của tính năng, không có mục thì Pro.
        UpgradeDialog* d = openPage(u"COUNT"_s, ui::Edition::None, kBatchName);
        QVERIFY(d);
        QCOMPARE(d->request().need, ui::Edition::ViewerPro);
        QCOMPARE(text(d, u"tag"_s), EditionGate::availableIn(ui::Edition::ViewerPro));
        page_->showUpgrade({u"NOPE"_s, ui::Edition::None, QString()}); // đang mở: thay nội dung tại chỗ
        QCOMPARE(page_->dialog(), d);
        QCOMPARE(d->request().need, ui::Edition::Pro);
        QCOMPARE(text(d, u"feature"_s), u"NOPE"_s); // không có tên: mã tính năng
    }

    // Tiêu chí 1: nút.
    void buttons()
    {
        // Để sau, Esc: đóng, không mở gì.
        QPointer<UpgradeDialog> d = openPage(u"QSAVE"_s, ui::Edition::Pro, kSaveName);
        QVERIFY(d);
        QVERIFY(!child<ui::Button>(d, u"activate"_s)->isVisible()); // mốc VP, chưa có hook
        QVERIFY(!child<ui::Button>(d, u"trial"_s));               // mốc EA, chưa có hook
        QCOMPARE(d->confirmButton(), child<ui::Button>(d, u"buy"_s)); // Mua là nút chính
        QCOMPARE(d->confirmButton()->iconName(), u"ext"_s);
        QCOMPARE(d->cardGeometry().width(), 560); // TC.dialog w:560
        child<ui::Button>(d, u"later"_s)->click();
        QVERIFY(closed(d));
        d = openPage(u"QSAVE"_s, ui::Edition::Pro, kSaveName);
        QVERIFY(d);
        QTest::keyClick(d, Qt::Key_Escape);
        QVERIFY(closed(d));
        QVERIFY(opened_.isEmpty());

        // Mua: trang mua chọn sẵn gói, kèm mã tính năng; rồi đóng. Enter = Mua.
        d = openPage(u"QSAVE"_s, ui::Edition::Pro, kSaveName);
        QVERIFY(d);
        child<ui::Button>(d, u"buy"_s)->click();
        QVERIFY(closed(d));
        QCOMPARE(opened_.size(), 1);
        QCOMPARE(opened_[0].fragment(), u"pro"_s);
        QCOMPARE(QUrlQuery(opened_[0]).queryItemValue(u"feature"_s), u"QSAVE"_s);
        d = openPage(u"COMPARE"_s, ui::Edition::ViewerPro, kBatchName);
        QVERIFY(d);
        QTest::keyClick(d, Qt::Key_Return);
        QVERIFY(closed(d));
        QCOMPARE(opened_.size(), 2);
        QCOMPARE(opened_[1].fragment(), u"vp"_s);

        // Không mở được trình duyệt: ở lại, dòng lỗi có địa chỉ (lỗi có biểu tượng, được đọc: FieldMessage).
        openOk_ = false;
        d = openPage(u"COMPARE"_s, ui::Edition::ViewerPro, kBatchName);
        QVERIFY(d);
        const int h = d->height();
        child<ui::Button>(d, u"buy"_s)->click();
        auto* error = child<ui::FieldMessage>(d, u"error"_s);
        QVERIFY(d && d->isVisible());
        QVERIFY(error->isVisible() && error->isError());
        QVERIFY(error->text().contains(opened_.last().toString()));
        QVERIFY(d->height() > h); // cao thêm dòng lỗi, không cắt
        QCOMPARE(truncations(d), QString());
        openOk_ = true;
        child<ui::Button>(d, u"buy"_s)->click();
        QVERIFY(closed(d));

        // Chỗ gắn theo mốc: có hook thì hiện, bấm thì đóng rồi gọi hook với yêu cầu; Dùng thử là nút chính.
        page_->setActivateHook([this](const UpgradeRequest& r) { activated_ << r; });
        page_->setTrialHook([this](const UpgradeRequest& r) { trials_ << r; });
        d = openPage(u"QSAVE"_s, ui::Edition::Pro, kSaveName);
        QVERIFY(d);
        QVERIFY(child<ui::Button>(d, u"activate"_s)->isVisible());
        QCOMPARE(d->confirmButton(), child<ui::Button>(d, u"trial"_s));
        QCOMPARE(child<ui::Button>(d, u"buy"_s)->kind(), ui::Button::Kind::Normal);
        QTest::keyClick(d, Qt::Key_Return);
        QVERIFY(closed(d));
        QCOMPARE(trials_.size(), 1);
        QCOMPARE(trials_[0].feature, u"QSAVE"_s);
        d = openPage(u"QSAVE"_s, ui::Edition::Pro, kSaveName);
        QVERIFY(d);
        child<ui::Button>(d, u"activate"_s)->click();
        QVERIFY(closed(d));
        QCOMPARE(activated_.size(), 1);
        QCOMPARE(activated_[0].need, ui::Edition::Pro);
        d = openPage(u"QSAVE"_s, ui::Edition::Pro, kSaveName);
        QVERIFY(d);
        child<ui::Button>(d, u"buy"_s)->click(); // Mua vẫn là nút thường bên cạnh
        QVERIFY(closed(d));
        QCOMPARE(opened_.size(), 5); // 2 lần mua, 1 lần lỗi + 1 lần mở lại, lần này
        // Đang dùng thử: không mời dùng thử nữa.
        gate_->setEdition(edition::Edition::Trial);
        d = openPage(u"QSAVE"_s, ui::Edition::Pro, kSaveName);
        QVERIFY(d);
        QVERIFY(!child<ui::Button>(d, u"trial"_s));
        QCOMPARE(d->confirmButton(), child<ui::Button>(d, u"buy"_s));
    }

    // Tiêu chí 2: mỗi tính năng tự mở tối đa một lần mỗi phiên; lần sau toast có "Xem".
    void oncePerSession()
    {
        const UpgradeRequest compare{u"COMPARE"_s, ui::Edition::ViewerPro, QString::fromUtf8(kBatchName)};
        QCOMPARE(gate_->upgrade().request(compare), UpgradeRouter::Outcome::Page);
        QPointer<UpgradeDialog> d = page_->dialog();
        QVERIFY(d && waitActive(d));
        d->reject();
        QVERIFY(closed(d));
        const qsizetype toasts = toasts_->toasts().size();
        QCOMPARE(gate_->upgrade().request(compare), UpgradeRouter::Outcome::Notice);
        QVERIFY(!page_->dialog());
        QCOMPARE(toasts_->toasts().size(), toasts + 1);
        ui::Toast* t = toasts_->toasts().last();
        QCOMPARE(t->text(), UpgradeRouter::noticeText(compare)); // "In hàng loạt… có trong TrionCAD Viewer Pro."
        QCOMPARE(t->severity(), ui::Severity::Info);
        QVERIFY(t->actionButton());
        QVERIFY(!t->actionButton()->text().isEmpty()); // "Xem"
        t->actionButton()->click();
        d = page_->dialog();
        QVERIFY(d && waitActive(d));
        QCOMPARE(d->request().feature, u"COMPARE"_s);
        d->reject();
        QVERIFY(closed(d));
        // Tính năng khác: trang mở.
        QCOMPARE(gate_->upgrade().request({u"QSAVE"_s, ui::Edition::Pro, QString::fromUtf8(kSaveName)}),
                 UpgradeRouter::Outcome::Page);
        QVERIFY(page_->dialog());
        // Gọi lại ngay khi trang vừa đóng (chờ hủy): trang mới hiện, không dùng bản đang đóng.
        QPointer<UpgradeDialog> old = page_->dialog();
        QVERIFY(waitActive(old));
        old->reject();
        QVERIFY(gate_->upgrade().open(compare));
        QVERIFY(page_->dialog() && page_->dialog() != old.data());
        QVERIFY(waitActive(page_->dialog()));
        QCOMPARE(page_->dialog()->request().feature, u"COMPARE"_s);
    }

    // Tiêu chí 4: mở ≤ 150 ms (CPU luồng UI, đến khung đầu có nội dung).
    void openTime()
    {
        constexpr int kOpens = 20;
        QList<double> cpu, wall;
        for (int i = 0; i < kOpens; ++i) {
            PaintSpy spy;
            const auto c0 = trion::test::threadCpu();
            const auto w0 = Clock::now();
            page_->showUpgrade({i % 2 ? u"QSAVE"_s : u"TCBATCHPLOT"_s, i % 2 ? ui::Edition::Pro : ui::Edition::ViewerPro,
                                QString::fromUtf8(i % 2 ? kSaveName : kBatchName)});
            QPointer<UpgradeDialog> d = page_->dialog();
            QVERIFY(d);
            child(d, u"benefit"_s)->installEventFilter(&spy);
            QVERIFY(spinUntil([&] { return spy.paints > 0; }));
            cpu << ms(trion::test::threadCpu() - c0);
            wall << ms(Clock::now() - w0);
            d->reject();
            QVERIFY(closed(d));
        }
        const double worst = *std::max_element(cpu.begin(), cpu.end());
        std::sort(cpu.begin(), cpu.end());
        std::sort(wall.begin(), wall.end());
        qInfo("mở trang nâng cấp đến khung đầu (%d lần): CPU luồng UI p50 %.2f ms, max %.2f ms; đồng hồ p50 %.2f ms, "
              "max %.2f ms (ngưỡng %.0f ms)",
              kOpens, cpu[kOpens / 2], worst, wall[kOpens / 2], wall.last(), kOpenBudgetMs);
        QVERIFY2(!kTimed || trion::test::threadCpu().count() < 0 || worst <= kOpenBudgetMs,
                 qPrintable(QString::number(worst)));
    }

    // DoD-U: vi-XL và English không cắt chữ, kể cả chân đủ bốn nút.
    void pseudoNoTruncation()
    {
        page_->setActivateHook([](const UpgradeRequest&) {});
        page_->setTrialHook([](const UpgradeRequest&) {});
        QPointer<UpgradeDialog> d = openPage(u"TCBATCHPLOT"_s, ui::Edition::ViewerPro, kBatchName);
        QVERIFY(d);
        QCOMPARE(truncations(d), QString());
        qInfo("vi, đủ 4 nút: thẻ %d×%d", d->cardGeometry().width(), d->cardGeometry().height());
        for (const QString& lang : {i18n::kPseudoLanguage.toString(), u"en"_s}) {
            const QString title = d->windowTitle();
            const QString benefit = text(d, u"benefit"_s);
            QVERIFY(translator_->setLanguage(lang));
            QVERIFY(spinUntil([&] { return d->windowTitle() != title || text(d, u"benefit"_s) != benefit; }));
            QVERIFY(spinUntil([&] { return truncations(d).isEmpty(); }, 2s));
            QCOMPARE(truncations(d), QString());
            const QRect avail = d->screen()->availableGeometry();
            QVERIFY2(d->width() <= avail.width() && d->height() <= avail.height(),
                     qPrintable(u"%1×%2"_s.arg(d->width()).arg(d->height())));
            qInfo("%s: thẻ %d×%d", qPrintable(lang), d->cardGeometry().width(), d->cardGeometry().height());
        }
    }

    // DoD-U 7: tên, vai trò, Tab không bẫy, viền focus; Esc đóng, focus về chỗ cũ.
    void accessibility()
    {
        page_->setActivateHook([](const UpgradeRequest&) {});
        page_->setTrialHook([](const UpgradeRequest&) {});
        cmd_->setFocus();
        QPointer<UpgradeDialog> d = openPage(u"QSAVE"_s, ui::Edition::Pro, kSaveName);
        QVERIFY(d);
        QStringList order;
        const QStringList issues = a11y::checkNames(d) + a11y::checkFocus(d, &order);
        QVERIFY2(issues.isEmpty(), qPrintable(issues.join(u'\n')));
        QVERIFY(order.size() >= 5); // Đóng, Để sau, Kích hoạt, Mua, Dùng thử
        QCOMPARE(QApplication::focusWidget(), static_cast<QWidget*>(d->confirmButton())); // focus đầu: nút chính
        QVERIFY(!d->confirmButton()->accessibleName().isEmpty() || !d->confirmButton()->text().isEmpty());
        QVERIFY(!child<ui::Button>(d, u"buy"_s)->accessibleDescription().isEmpty()); // "Mở trang mua trên web"
        QTest::keyClick(d, Qt::Key_Escape);
        QVERIFY(closed(d));
        QVERIFY(spinUntil([&] { return QApplication::focusWidget() == cmd_; })); // focus về chỗ cũ
        qInfo("Tab: %s", qPrintable(order.join(u" → "_s)));
    }

    // Ảnh minh họa không chữ: nền --c-canvas, hình --c-accent, ổ khóa trên ô --c-base; Tối và Sáng.
    void illustration()
    {
        for (const theme::Theme t : {theme::Theme::Dark, theme::Theme::Light}) {
            tm_->setTheme(t);
            UpgradeDialog* d = openPage(u"QSAVE"_s, ui::Edition::Pro, kSaveName);
            QVERIFY(d);
            QWidget* w = child<QWidget>(d, u"illustration"_s);
            QCOMPARE(w->size(), QSize(150, 150));
            const QImage img = w->grab().toImage().convertToFormat(QImage::Format_ARGB32);
            const QColor canvas = tm_->color(tok::c_canvas), accent = tm_->color(tok::c_accent),
                         base = tm_->color(tok::c_base);
            const auto near = [](QColor a, QColor b) {
                return std::abs(a.red() - b.red()) <= 8 && std::abs(a.green() - b.green()) <= 8 &&
                       std::abs(a.blue() - b.blue()) <= 8;
            };
            int nAccent = 0, nBase = 0;
            for (int y = 0; y < img.height(); ++y)
                for (int x = 0; x < img.width(); ++x) {
                    const QColor c = img.pixelColor(x, y);
                    nAccent += near(c, accent);
                    nBase += near(c, base);
                }
            const QPoint mid(img.width() / 2, img.height() / 10); // mép trên giữa: nền
            QVERIFY(near(img.pixelColor(mid), canvas));
            QVERIFY2(nAccent > 30, qPrintable(QString::number(nAccent))); // hình và ổ khóa (nét mảnh: điểm đủ màu ít)
            QVERIFY2(nBase > 300, qPrintable(QString::number(nBase)));     // ô ổ khóa
            QPointer<UpgradeDialog> p = d;
            d->reject();
            QVERIFY(closed(p));
        }
    }
};

QTEST_MAIN(UpgradeTest)
#include "upgrade_test.moc"
