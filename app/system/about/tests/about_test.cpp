// UIL-011: hộp Giới thiệu (lệnh ABOUT, UI-L13). Mỗi tiêu chí nghiệm thu một nhóm hàm test:
//   1  header: phiên bản, bản build, kênh (BuildInfo), "TrionCAD" và huy hiệu phiên bản theo EditionGate (đổi ngay khi
//      giấy phép đổi). systemCopy: thông tin hệ thống đọc nền (backend, GPU qua hook chạy ngoài luồng UI; chưa về thì
//      "Đang đọc…" và Sao chép vô hiệu), Sao chép chép đủ trường: phiên bản, bản build, kênh, phiên bản giấy phép, hệ
//      điều hành, CPU, RAM, backend, GPU, độ phân giải, tỉ lệ, ngôn ngữ, hồ sơ.
//   2  licenses: bảng từ licenses.json (gen_licenses.py lúc build; thiếu văn bản giấy phép thì lỗi: test_gen_licenses.py)
//      đủ thư viện 05 §2; Qt "LGPL-3.0, liên kết động"; văn bản giấy phép (nút, Enter, nhấp đúp) có LGPL-3.0, GPL-3.0 và
//      cách thay thư viện Qt; Enter trên bảng không đóng hộp Giới thiệu. licensesParse: JSON hỏng, thiếu văn bản → lỗi.
//   3  links: Điều khoản sử dụng, Chính sách riêng tư mở trang web (không mở được: ở lại, dòng lỗi có địa chỉ); ghi chú
//      "AutoCAD® là nhãn hiệu của Autodesk, Inc."; Liên hệ hỗ trợ: có hook UI-L17 thì đóng hộp rồi gọi, không có thì trang
//      hỗ trợ trên web.
//   4  openTime: CPU luồng UI từ lúc mở đến khung đầu có nội dung, 20 lần; ngưỡng 150 ms chỉ kiểm ở release
//      (tests/timing.h); báo thêm thời gian tới khi dữ liệu nền về.
//   5  DoD-U: pseudoNoTruncation (vi-XL, English; cả hộp văn bản giấy phép), accessibility (a11y::audit: tên, vai trò,
//      Tab, viền focus; Esc đóng, focus về chỗ cũ).
#include "app/system/about/AboutDialog.h"

#include "app/core/a11y/AccessibilityAudit.h"
#include "app/core/edition/EditionBadge.h"
#include "app/core/edition/EditionGate.h"
#include "app/core/i18n/Translator.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/core/i18n/UiFormat.h"
#include "app/core/UiExecutor.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/controls/Controls.h"
#include "app/widgets/message/Messages.h"
#include "app/widgets/views/VirtualTableView.h"
#include "core/base/sched/Scheduler.h"
#include "tests/timing.h"

#include <QApplication>
#include <QClipboard>
#include <QEventLoop>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QScreen>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QtTest>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <memory>

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;
using namespace trion;
using about::AboutDialog;
using about::GraphicsInfo;
using about::Licenses;
using edition::EditionGate;

namespace {

using Clock = std::chrono::steady_clock;
using trion::test::kTimed;

constexpr double kOpenBudgetMs = 150; // DoD-L "mở hộp thoại đến khung đầu có nội dung", máy tầm trung
constexpr char kProfile[] = "Hồ sơ Kết cấu"; // i18n-ok: chữ mẫu của test
constexpr char kBackend[] = "Vulkan";
constexpr char kAdapter[] = "Test GPU 9000";

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

QString truncations(QWidget* w)
{
    QStringList out;
    for (const i18n::Truncation& t : i18n::findTruncations(w))
        out << t.toString();
    return out.join(u"; "_s);
}

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

class AboutTest : public QObject {
    Q_OBJECT
    QTemporaryDir dir_;
    std::unique_ptr<settings::Store> store_;
    std::unique_ptr<ui::UiExecutor> executor_;
    std::unique_ptr<theme::ThemeManager> tm_;
    std::unique_ptr<theme::IconProvider> icons_;
    std::unique_ptr<i18n::Translator> translator_;
    std::unique_ptr<QWidget> main_;
    QLineEdit* cmd_ = nullptr;
    std::unique_ptr<EditionGate> gate_;
    QList<QUrl> opened_;
    bool openOk_ = true;
    std::atomic<bool> probeOffUi_{false}; // hook GPU chạy ngoài luồng UI

    AboutDialog::Sources sources(std::function<GraphicsInfo()> graphics = {})
    {
        AboutDialog::Sources s;
        s.graphics = graphics ? std::move(graphics) : [this] {
            probeOffUi_ = QThread::currentThread() != QCoreApplication::instance()->thread();
            return GraphicsInfo{QString::fromLatin1(kBackend), QString::fromLatin1(kAdapter)};
        };
        s.profile = QString::fromUtf8(kProfile);
        s.openUrl = [this](const QUrl& u) {
            opened_ << u;
            return openOk_;
        };
        return s;
    }

    // Mở như DialogHost (không chế độ, tự hủy khi đóng) và chờ dữ liệu nền.
    AboutDialog* open(AboutDialog::Sources s, bool waitLoaded = true)
    {
        auto* d = new AboutDialog(*gate_, std::move(s), main_.get());
        d->setAttribute(Qt::WA_DeleteOnClose);
        d->show();
        if (!waitActive(d) || (waitLoaded && !spinUntil([d] { return d->isLoaded(); })))
            return nullptr;
        return d;
    }

    template <class T = QLabel>
    static T* child(QWidget* d, const QString& name)
    {
        return d->findChild<T*>(name);
    }

    static bool closed(const QPointer<QWidget>& d)
    {
        return spinUntil([&] { return !d || !d->isVisible(); });
    }

    // Hộp văn bản giấy phép đang mở (con của d).
    static ui::DialogFrame* licenseDialog(QWidget* d)
    {
        for (ui::DialogFrame* f : d->findChildren<ui::DialogFrame*>())
            if (f->id() == u"license-text"_s && f->isVisible())
                return f;
        return nullptr;
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
        cmd_ = new QLineEdit(main_.get()); // như dòng lệnh: focus về đây khi hộp đóng
        cmd_->setGeometry(20, 700, 600, 26);
        main_->show();
        main_->activateWindow();
        QVERIFY(waitActive(main_.get()));
    }

    void init()
    {
        gate_ = std::make_unique<EditionGate>();
        opened_.clear();
        openOk_ = true;
        main_->activateWindow();
        QVERIFY(waitActive(main_.get()));
        cmd_->setFocus();
    }

    void cleanup()
    {
        for (AboutDialog* d : main_->findChildren<AboutDialog*>(Qt::FindDirectChildrenOnly))
            delete d;
        gate_.reset();
        if (translator_->language() != u"vi"_s) {
            QVERIFY(translator_->setLanguage(u"vi"_s));
            QCoreApplication::processEvents();
        }
    }

    void cleanupTestCase()
    {
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

    // Tiêu chí 1: phiên bản, bản build, kênh; giấy phép (phiên bản, huy hiệu).
    void header()
    {
        QPointer<AboutDialog> d = open(sources());
        QVERIFY(d);
        QCOMPARE(d->command(), u"ABOUT"_s); // F1, nút Trợ giúp → ABOUT
        QCOMPARE(child(d, u"product"_s)->text(), u"TrionCAD"_s);
        const about::BuildInfo b = about::BuildInfo::current();
        QVERIFY(!b.version.isEmpty() && !b.build.isEmpty());
        const QString version = child(d, u"version"_s)->text();
        for (const QString& part : {b.version, b.build, b.channelName()})
            QVERIFY2(version.contains(part), qPrintable(version));
        auto* badge = child<edition::EditionBadge>(d, u"edition"_s);
        QVERIFY(badge && badge->isVisible());
        QCOMPARE(badge->text(), edition::EditionBadge::label(*gate_)); // "Viewer"
        gate_->setEdition(edition::Edition::Pro);                      // kích hoạt khi đang mở: đổi ngay
        QCOMPARE(badge->text(), edition::EditionBadge::label(*gate_));
        QVERIFY(d->systemText().contains(edition::EditionBadge::label(*gate_)));
        qInfo("%s | %s", qPrintable(badge->text()), qPrintable(version));
    }

    // Tiêu chí 1: thông tin hệ thống đọc nền; Sao chép đủ trường.
    void systemCopy()
    {
        std::promise<void> release;
        const std::shared_future<void> ready = release.get_future().share();
        QPointer<AboutDialog> d = open(sources([this, ready] {
                                           probeOffUi_ = QThread::currentThread() !=
                                                         QCoreApplication::instance()->thread();
                                           ready.wait_for(10s); // dò GPU chậm (RND-001)
                                           return GraphicsInfo{QString::fromLatin1(kBackend),
                                                               QString::fromLatin1(kAdapter)};
                                       }),
                                       false);
        QVERIFY(d);
        // GPU chưa về: phần đó "Đang đọc…", Sao chép vô hiệu; phần khác đã về, giao diện vẫn chạy.
        QVERIFY(spinUntil([&] { return !d->licenses().components.isEmpty(); }));
        QVERIFY(!d->isLoaded());
        auto* copy = child<ui::Button>(d, u"copy"_s);
        QVERIFY(!copy->isEnabled());
        const QString waiting = child(d, u"system"_s)->text();
        release.set_value();
        QVERIFY(spinUntil([&] { return d->isLoaded(); }));
        QVERIFY(probeOffUi_);
        QVERIFY(copy->isEnabled());
        QVERIFY(child(d, u"system"_s)->text() != waiting);

        QGuiApplication::clipboard()->clear();
        copy->click();
        const QString text = QGuiApplication::clipboard()->text();
        QCOMPARE(text, d->systemText());
        const about::BuildInfo b = about::BuildInfo::current();
        const about::SystemInfo sys = about::SystemInfo::read();
        const qreal dpr = d->screen()->devicePixelRatio();
        const QSize px = d->screen()->geometry().size() * dpr;
        const QStringList fields{
            u"TrionCAD"_s,
            b.version,
            b.build,
            b.channelName(),
            edition::EditionBadge::label(*gate_), // giấy phép: phiên bản
            sys.os,                               // hệ điều hành
            sys.cpu,                              // CPU
            i18n::formatBytes(sys.ram),           // RAM
            QString::fromLatin1(kBackend),        // backend
            QString::fromLatin1(kAdapter),        // GPU
            u"%1 × %2"_s.arg(px.width()).arg(px.height()), // độ phân giải
            u"%1%"_s.arg(qRound(dpr * 100)),               // tỉ lệ
            u"Tiếng Việt"_s,                               // i18n-ok: ngôn ngữ
            QString::fromUtf8(kProfile),                   // hồ sơ
        };
        for (const QString& f : fields)
            QVERIFY2(!f.isEmpty() && text.contains(f), qPrintable(f + u" ∉ "_s + text));
        QVERIFY(sys.ram > 0 && sys.threads > 0);
        QCOMPARE(copy->iconName(), u"check"_s); // "Đã sao chép"
        qInfo("%s", qPrintable(text));
    }

    // Không có hook RND-001: "chưa rõ", vẫn sao chép được.
    void noGraphicsProbe()
    {
        AboutDialog::Sources s = sources();
        s.graphics = {};
        s.profile.clear();
        QPointer<AboutDialog> d = open(std::move(s));
        QVERIFY(d);
        QVERIFY(child<ui::Button>(d, u"copy"_s)->isEnabled());
        QVERIFY(d->systemText().contains(u"<<"_s)); // hồ sơ mặc định "<<Mặc định>>"
        QVERIFY(!d->systemText().contains(QString::fromLatin1(kAdapter)));
    }

    // Tiêu chí 2: danh sách thư viện và giấy phép.
    void licenses()
    {
        QPointer<AboutDialog> d = open(sources());
        QVERIFY(d);
        const Licenses& list = d->licenses();
        QStringList ids;
        for (const about::Component& c : list.components)
            ids << c.id;
        for (const QString& need : {u"qt"_s, u"freetype"_s, u"harfbuzz"_s, u"zlib"_s, u"libdeflate"_s, u"pdfium"_s,
                                    u"libsodium"_s, u"crashpad"_s, u"plex"_s})
            QVERIFY2(ids.contains(need), qPrintable(need)); // 05 §2 và font nhúng
        auto* table = child<ui::VirtualTableView>(d, u"licenses"_s);
        QCOMPARE(table->model()->rowCount(), list.components.size());
        const int qt = int(ids.indexOf(u"qt"_s));
        const QString qtLicense = table->model()->index(qt, 1).data().toString();
        QVERIFY2(qtLicense.contains(u"LGPL-3.0"_s) && qtLicense.size() > u"LGPL-3.0"_s.size(), qPrintable(qtLicense));
        QCOMPARE(table->model()->index(qt, 0).data().toString(), u"Qt 6"_s);
        for (int r = 0; r < table->model()->rowCount(); ++r)
            QVERIFY(!table->model()->index(r, 1).data().toString().isEmpty());

        // Văn bản Qt: cách thay thư viện, LGPL-3.0, GPL-3.0, mã nguồn.
        const QString text = AboutDialog::licenseText(list.components[qt], list);
        for (const QString& s : {u"LGPL-3.0"_s, u"GNU LESSER GENERAL PUBLIC LICENSE"_s, u"GNU GENERAL PUBLIC LICENSE"_s,
                                 QString::fromLatin1(qVersion()), u"download.qt.io"_s,
#if defined(Q_OS_WIN)
                                 u"Qt6*.dll"_s
#elif defined(Q_OS_MACOS)
                                 u"Qt*.framework"_s
#else
                                 u"libQt6*.so.6"_s
#endif
             })
            QVERIFY2(text.contains(s), qPrintable(s));
        const QString ftl = AboutDialog::licenseText(list.components[int(ids.indexOf(u"freetype"_s))], list);
        QVERIFY(ftl.contains(u"The FreeType Project LICENSE"_s) && !ftl.contains(u"GNU"_s));

        // Nút "Văn bản giấy phép…" (dòng đang chọn: dòng đầu, Qt) mở hộp văn bản; Esc đóng, hộp Giới thiệu ở lại.
        table->setCurrentIndex(table->model()->index(qt, 0));
        child<ui::Button>(d, u"viewLicense"_s)->click();
        QPointer<ui::DialogFrame> f = licenseDialog(d);
        QVERIFY(f && waitActive(f));
        QVERIFY(f->windowTitle().contains(u"Qt 6"_s));
        QCOMPARE(child<QPlainTextEdit>(f, u"licenseText"_s)->toPlainText(), text);
        QTest::keyClick(f, Qt::Key_Escape);
        QVERIFY(closed(f.data()));
        QVERIFY(d && d->isVisible());

        // Enter trên bảng: văn bản của dòng hiện hành, không tới nút Đóng của hộp Giới thiệu.
        QVERIFY(waitActive(d));
        table->setFocus();
        table->setCurrentIndex(table->model()->index(int(ids.indexOf(u"plex"_s)), 0));
        QTest::keyClick(table, Qt::Key_Return);
        f = licenseDialog(d);
        QVERIFY(f && waitActive(f));
        QVERIFY(child<QPlainTextEdit>(f, u"licenseText"_s)->toPlainText().contains(u"SIL OPEN FONT LICENSE"_s));
        QVERIFY(d->isVisible());
        f->reject();
        QVERIFY(closed(f.data()));
        // Nhấp đúp (tín hiệu của QAbstractItemView).
        QVERIFY(waitActive(d));
        emit table->doubleClicked(table->model()->index(0, 0));
        f = licenseDialog(d);
        QVERIFY(f && waitActive(f));
        f->reject();
    }

    void licensesParse()
    {
        QVERIFY(Licenses::load());
        QVERIFY(!Licenses::parse("{"));
        QVERIFY(!Licenses::parse(R"({"format": "x", "components": []})"));
        const QByteArray missing =
            R"({"format": "trion-licenses/1", "texts": {},
                "components": [{"id": "qt", "name": "Qt 6", "license": "LGPL-3.0", "texts": ["LGPL-3.0-only"]}]})";
        const Expected<Licenses> l = Licenses::parse(missing);
        QVERIFY(!l);
        QVERIFY(l.error().message.find("LGPL-3.0-only") != std::string::npos);
    }

    // Tiêu chí 3: điều khoản, chính sách riêng tư, nhãn hiệu, liên hệ hỗ trợ.
    void links()
    {
        QPointer<AboutDialog> d = open(sources());
        QVERIFY(d);
        const QString tm = child(d, u"trademark"_s)->text();
        QVERIFY2(tm.contains(u"AutoCAD®"_s) && tm.contains(u"Autodesk, Inc."_s), qPrintable(tm));
        child<ui::Button>(d, u"terms"_s)->click();
        child<ui::Button>(d, u"privacy"_s)->click();
        QCOMPARE(opened_.size(), 2);
        QVERIFY(opened_[0].isValid() && opened_[1].isValid() && opened_[0] != opened_[1]);
        QVERIFY(!child<ui::FieldMessage>(d, u"error"_s)->isVisible());

        // Không mở được trình duyệt: ở lại, dòng lỗi có địa chỉ.
        openOk_ = false;
        child<ui::Button>(d, u"terms"_s)->click();
        auto* error = child<ui::FieldMessage>(d, u"error"_s);
        QVERIFY(error->isVisible() && error->isError());
        QVERIFY(error->text().contains(opened_.last().toString()));
        QCOMPARE(truncations(d), QString());
        openOk_ = true;

        // Liên hệ hỗ trợ chưa có UI-L17: trang hỗ trợ trên web, hộp ở lại; mở được thì dòng lỗi tắt.
        child<ui::Button>(d, u"support"_s)->click();
        QCOMPARE(opened_.size(), 4);
        QVERIFY(!error->isVisible());
        QVERIFY(d->isVisible());
        d->reject();
        QVERIFY(closed(d.data()));

        // Có hook UI-L17: đóng hộp Giới thiệu rồi gọi hook với cửa sổ cha.
        QWidget* supportParent = nullptr;
        int supports = 0;
        AboutDialog::Sources s = sources();
        s.support = [&](QWidget* parent) {
            ++supports;
            supportParent = parent;
        };
        d = open(std::move(s));
        QVERIFY(d);
        child<ui::Button>(d, u"support"_s)->click();
        QCOMPARE(supports, 1);
        QCOMPARE(supportParent, main_.get());
        QVERIFY(closed(d.data()));
        QCOMPARE(opened_.size(), 4);
    }

    // Tiêu chí 4: mở ≤ 150 ms (CPU luồng UI, đến khung đầu có nội dung); dữ liệu nền về sau đó.
    void openTime()
    {
        constexpr int kOpens = 20;
        QList<double> cpu, build, shown, wall, ready;
        for (int i = 0; i < kOpens; ++i) {
            PaintSpy spy;
            const auto c0 = trion::test::threadCpu();
            const auto w0 = Clock::now();
            QPointer<AboutDialog> d = new AboutDialog(*gate_, sources(), main_.get());
            build << ms(trion::test::threadCpu() - c0);
            child(d, u"system"_s)->installEventFilter(&spy);
            d->show();
            shown << ms(trion::test::threadCpu() - c0);
            QVERIFY(spinUntil([&] { return spy.paints > 0; }));
            cpu << ms(trion::test::threadCpu() - c0);
            wall << ms(Clock::now() - w0);
            QVERIFY(spinUntil([&] { return d->isLoaded(); }));
            ready << ms(Clock::now() - w0);
            delete d.data();
        }
        const double worst = *std::max_element(cpu.begin(), cpu.end());
        for (QList<double>* l : {&cpu, &build, &shown, &wall, &ready})
            std::sort(l->begin(), l->end());
        qInfo("mở Giới thiệu đến khung đầu (%d lần): CPU luồng UI p50 %.2f ms, max %.2f ms (dựng p50 %.2f ms, tới hết "
              "show() %.2f ms); đồng hồ p50 %.2f ms, max %.2f ms; dữ liệu nền về p50 %.2f ms, max %.2f ms (ngưỡng "
              "%.0f ms)",
              kOpens, cpu[kOpens / 2], worst, build[kOpens / 2], shown[kOpens / 2], wall[kOpens / 2], wall.last(), ready[kOpens / 2],
              ready.last(), kOpenBudgetMs);
        QVERIFY2(!kTimed || trion::test::threadCpu().count() < 0 || worst <= kOpenBudgetMs,
                 qPrintable(QString::number(worst)));
    }

    // DoD-U: vi-XL và English không cắt chữ, cả hộp văn bản giấy phép; hộp vừa màn hình nhỏ nhất (1366×768).
    void pseudoNoTruncation()
    {
        QPointer<AboutDialog> d = open(sources());
        QVERIFY(d);
        QCOMPARE(truncations(d), QString());
        qInfo("vi: thẻ %d×%d", d->cardGeometry().width(), d->cardGeometry().height());
        QCOMPARE(d->cardGeometry().width(), 600); // TC.dialog w:600
        child<ui::Button>(d, u"viewLicense"_s)->click();
        QPointer<ui::DialogFrame> f = licenseDialog(d);
        QVERIFY(f && waitActive(f));
        for (const QString& lang : {i18n::kPseudoLanguage.toString(), u"en"_s}) {
            const QString title = d->windowTitle();
            QVERIFY(translator_->setLanguage(lang));
            QVERIFY(spinUntil([&] { return d->windowTitle() != title; }));
            QVERIFY(spinUntil([&] { return truncations(d).isEmpty() && truncations(f).isEmpty(); }, 2s));
            QCOMPARE(truncations(d), QString());
            QCOMPARE(truncations(f), QString());
            const QSize card = d->cardGeometry().size();
            QVERIFY2(card.height() <= 768 - 40 && card.width() <= 1366, qPrintable(u"%1×%2"_s.arg(card.width()).arg(card.height())));
            qInfo("%s: thẻ %d×%d", qPrintable(lang), card.width(), card.height());
        }
    }

    // DoD-U 7: tên, vai trò, Tab không bẫy, viền focus; Esc đóng, focus về chỗ cũ.
    void accessibility()
    {
        QPointer<AboutDialog> d = open(sources());
        QVERIFY(d);
        QStringList order;
        const QStringList issues = a11y::checkNames(d) + a11y::checkFocus(d, &order);
        QVERIFY2(issues.isEmpty(), qPrintable(issues.join(u'\n')));
        QVERIFY(order.size() >= 8); // Trợ giúp, Đóng (đầu), Sao chép, bảng, Văn bản giấy phép, 3 liên kết, Đóng
        QVERIFY(!child<ui::VirtualTableView>(d, u"licenses"_s)->accessibleName().isEmpty());
        // Hộp văn bản giấy phép: ô chữ chỉ đọc có tên, Tab rời ô, Esc đóng về hộp Giới thiệu.
        child<ui::Button>(d, u"viewLicense"_s)->click();
        QPointer<ui::DialogFrame> f = licenseDialog(d);
        QVERIFY(f && waitActive(f));
        const QStringList licenseIssues = a11y::checkNames(f) + a11y::checkFocus(f);
        QVERIFY2(licenseIssues.isEmpty(), qPrintable(licenseIssues.join(u'\n')));
        QTest::keyClick(f, Qt::Key_Escape);
        QVERIFY(closed(f.data()));
        QVERIFY(waitActive(d));
        QTest::keyClick(d, Qt::Key_Escape);
        QVERIFY(closed(d.data()));
        QVERIFY(spinUntil([&] { return QApplication::focusWidget() == cmd_; }));
        qInfo("Tab: %s", qPrintable(order.join(u" → "_s)));
    }
};

QTEST_MAIN(AboutTest)
#include "about_test.moc"
