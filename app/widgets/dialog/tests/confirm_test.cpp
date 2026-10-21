// UI0-008: xác nhận phá hủy (ConfirmDialog), "Không hỏi lại" (SuppressedPrompts), hộp thoại lỗi (ErrorDialog). Mỗi
// tiêu chí nghiệm thu một nhóm hàm test:
//   1  actionLabel: isActionLabel nhận "Xóa %1 layout", từ chối nhãn thiếu %1, chỉ có %1, chữ chung chung (OK…);
//      builderAsserts (bản có TRION_ASSERT): nhãn chung chung, thiếu %1, số lượng 0 thì assert; countFormat: nút
//      "Xóa 3 layout", "Xóa 1.250 layout", câu hỏi đậm <b>1.250</b>; English 1,250.
//   2  dontAsk: đánh dấu rồi bấm nút chính / Hủy: lần sau trả kết quả đã nhớ ngay, không dựng hộp thoại; model
//      khôi phục từng mục, tất cả; restart: Store mới đọc cùng tệp vẫn nhớ, ConfirmDialog đọc kho của tiến trình.
//   3  errorDetails: chi tiết thu gọn mặc định, mở thì hộp thoại cao thêm; "Sao chép chi tiết" chép đúng gói (mã lỗi,
//      phiên bản, hệ điều hành, 200 dòng log cuối của nguồn trả 250 dòng); chưa có nguồn log: "Chưa có log.";
//      errorReport: chưa có handler thì không có nút; mở, đóng không gọi handler; bấm thì gọi một lần kèm chi tiết.
//   4  focusKeys: không hoàn tác được: focus đầu ở Hủy, Enter = Hủy; hoàn tác được: focus ở nút chính, Enter = làm;
//      Esc = Hủy; done gọi đúng một lần, hộp thoại tự xóa. openTime: mở xác nhận, lỗi mỗi loại 20 lần, UiProbe
//      dialog_open p50/p99 (ngưỡng 150 ms chỉ kiểm ở release, tests/timing.h).
//   5  baseline (DoD-U 1): ảnh xác nhận và lỗi Tối/Sáng × 100/150/200% so baseline theo nền tảng (ghi lại:
//      TRION_UPDATE_BASELINE=<thư mục>); pseudoNoTruncation: vi-XL, findTruncations rỗng (lỗi: cả khi mở chi tiết).
#include "app/widgets/dialog/ConfirmDialog.h"
#include "app/widgets/dialog/DialogFrame.h"
#include "app/widgets/dialog/ErrorDialog.h"
#include "app/widgets/dialog/SuppressedPrompts.h"

#include "app/core/i18n/Translator.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/core/perf/UiProbe.h"
#include "app/core/UiExecutor.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/generated/Tokens.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "core/base/assert.h"
#include "core/base/sched/Scheduler.h"
#include "tests/timing.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QEventLoop>
#include <QJsonObject>
#include <QLabel>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QSysInfo>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

#include <chrono>
#include <functional>
#include <memory>

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;
using namespace trion::theme;
using namespace trion::ui;
namespace i18n = trion::i18n;
namespace sched = trion::sched;
namespace settings = trion::settings;

namespace {

using trion::test::kTimed;

#if defined(Q_OS_WIN)
constexpr auto kPlatform = "windows";
#elif defined(Q_OS_MACOS)
constexpr auto kPlatform = "macos";
#else
constexpr auto kPlatform = "linux";
#endif
constexpr int kNoise = 8; // như dialog_test: lệch ≤ 8/255 mỗi kênh bỏ qua, điểm lệch hơn tối đa 0,1%
constexpr double kOpenBudgetMs = 150; // DoD-L "mở hộp thoại đến khung đầu có nội dung", máy tầm trung

// Chữ mẫu, dịch qua ngữ cảnh "Sample" (vi-XL kéo dài cả chúng).
constexpr char kTitle[] = "Xóa layout";     // i18n-ok: chữ mẫu
constexpr char kAction[] = "Xóa %1 layout"; // i18n-ok: chữ mẫu
constexpr char kQuestion[] = "Xóa %1 layout đã chọn? Đối tượng trên các layout này cũng bị xóa."; // i18n-ok: chữ mẫu
constexpr char kDetail[] = "Hoàn tác được bằng Ctrl+Z.";                                   // i18n-ok: chữ mẫu
constexpr char kErrTitle[] = "Không lưu được bản vẽ";                                      // i18n-ok: chữ mẫu
constexpr char kErrText[] = "Ổ đĩa đầy. Giải phóng ít nhất 120 MB trên ổ D: rồi lưu lại."; // i18n-ok: chữ mẫu

QString sampleTr(const char* source, int n = -1)
{
    return QCoreApplication::translate("Sample", source, nullptr, n);
}

ConfirmDialog sample(qint64 n)
{
    return ConfirmDialog(sampleTr(kTitle), sampleTr(kAction, int(n)), n)
        .question(sampleTr(kQuestion, int(n)))
        .detail(sampleTr(kDetail));
}

const trion::Error kDiskFull{"io", "disk-full", "write: No space left on device"};

ErrorDialog* errorDialog(QWidget* parent)
{
    return new ErrorDialog(sampleTr(kErrTitle), sampleTr(kErrText), kDiskFull, parent);
}

QStringList fakeLog(int lines)
{
    QStringList log;
    for (int i = 1; i <= lines; ++i)
        log << u"12:00:%1 [dwg] info dòng %2"_s.arg(i % 60, 2, 10, QChar(u'0')).arg(i);
    return log;
}

// Chờ điều kiện trong vòng sự kiện thật (QEventLoop + QTimer, như ui_probe_test).
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

int probeCount(const char* op)
{
    const QJsonObject j = probe::toJson();
    return j["ops"_L1][QLatin1StringView(op)]["count"_L1].toInt();
}

QString truncations(QWidget* w)
{
    QStringList out;
    for (const i18n::Truncation& t : i18n::findTruncations(w))
        out << t.toString();
    return out.join(u"; "_s);
}

} // namespace

class ConfirmTest : public QObject {
    Q_OBJECT
    QTemporaryDir dir_;
    std::unique_ptr<settings::Store> store_;
    std::unique_ptr<UiExecutor> executor_;
    std::unique_ptr<ThemeManager> tm_;
    std::unique_ptr<IconProvider> icons_;
    std::unique_ptr<i18n::Translator> translator_;
    std::unique_ptr<QWidget> main_;

    // Mở n lần hộp thoại do open() dựng (đã hiện), đóng bằng Hủy; trả {p50, p99} của dialog_open.
    std::pair<double, double> measureOpen(const char* what, const std::function<QDialog*()>& open)
    {
        probe::enable();
        probe::reset();
        constexpr int kOpens = 20;
        for (int i = 0; i < kOpens; ++i) {
            QPointer<QDialog> f = open();
            if (!spinUntil([&] { return probeCount("dialog_open") > i; }))
                return {-1, -1};
            f->reject();
            if (!spinUntil([&] { return !f; }))
                return {-1, -1};
        }
        const QJsonValue op = probe::toJson()["ops"_L1]["dialog_open"_L1];
        probe::disable();
        const double p50 = op["p50_ms"_L1].toDouble(), p99 = op["p99_ms"_L1].toDouble();
        qInfo("dialog_open %s (%d lần): p50 %.2f ms, p99 %.2f ms, max %.2f ms (ngưỡng %.0f ms)", what,
              op["count"_L1].toInt(), p50, p99, op["max_ms"_L1].toDouble(), kOpenBudgetMs);
        return {p50, p99};
    }

private slots:
    void initTestCase()
    {
        QVERIFY(dir_.isValid());
        store_ = std::make_unique<settings::Store>(dir_.filePath(u"user.json"_s));
        settings::Store::setInstance(store_.get());
        QVERIFY(loadFonts());
        executor_ = std::make_unique<UiExecutor>();
        tm_ = std::make_unique<ThemeManager>(*store_);
        icons_ = std::make_unique<IconProvider>();
        translator_ = std::make_unique<i18n::Translator>();
        main_ = std::make_unique<QWidget>();
        main_->resize(600, 400);
        main_->move(100, 80);
        main_->show();
        main_->activateWindow();
        QVERIFY(waitActive(main_.get()));
    }

    void init()
    {
        tm_->setTheme(Theme::Dark);
        tm_->setLite(false);
        SuppressedPrompts(*store_).restoreAll();
        main_->activateWindow();
        QVERIFY(waitActive(main_.get()));
    }

    void cleanup()
    {
        ErrorDialog::setLogSource({}); // handler của test bắt biến cục bộ
        ErrorDialog::setReportHandler({});
        probe::disable();
        qDeleteAll(main_->findChildren<QDialog*>(Qt::FindDirectChildrenOnly));
        QCoreApplication::processEvents();
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

    // Tiêu chí 1.
    void actionLabel()
    {
        for (const char* good : {"Xóa %1 layout", "Delete %1 layouts", "&Dọn %1 mục", "Sửa %1 mục"}) // i18n-ok: mẫu
            QVERIFY2(ConfirmDialog::isActionLabel(QString::fromUtf8(good)), good);
        for (const char* bad : {"Xóa layout", "OK", "%1", " & %1 ", "OK %1", "&Ok %1", "Đồng ý %1", "Xác  nhận %1",
                                "Có (%1)", "Yes %1", "Continue %1"}) // i18n-ok: nhãn sai mà kiểm tra phải bắt
            QVERIFY2(!ConfirmDialog::isActionLabel(QString::fromUtf8(bad)), bad);
    }

    void builderAsserts()
    {
#ifdef NDEBUG
        QSKIP("TRION_ASSERT tắt ở bản release; quy tắc nhãn kiểm ở actionLabel");
#else
        static int assertFails = 0;
        const trion::AssertHandler old = trion::setAssertHandler([](const char*, const char*, int) { ++assertFails; });
        (void)ConfirmDialog(sampleTr(kTitle), u"OK"_s, 3);              // i18n-ok: nút chung chung
        (void)ConfirmDialog(sampleTr(kTitle), u"Xóa layout"_s, 3);      // i18n-ok: nhãn không có số lượng
        (void)ConfirmDialog(sampleTr(kTitle), sampleTr(kAction, 0), 0); // số lượng 0
        const int bad = assertFails;
        (void)ConfirmDialog(sampleTr(kTitle), sampleTr(kAction, 3), 3); // hợp lệ
        trion::setAssertHandler(old);
        QCOMPARE(bad, 3);
        QCOMPARE(assertFails, 3);
#endif
    }

    void countFormat()
    {
        QPointer<DialogFrame> f = sample(3).ask(main_.get(), [](bool) {});
        QCOMPARE(f->confirmButton()->text(), u"Xóa 3 layout"_s); // i18n-ok: chữ mong đợi
        QVERIFY(f->isModal());
        f->reject();
        QVERIFY(spinUntil([&] { return !f; }));

        f = sample(1250).ask(main_.get(), [](bool) {});
        QCOMPARE(f->confirmButton()->text(), u"Xóa 1.250 layout"_s); // i18n-ok: chữ mong đợi
        const QString question = f->body()->findChildren<QLabel*>().value(0)->text();
        QVERIFY2(question.contains(u"<b>1.250</b>"_s), qPrintable(question));
        f->reject();
        QVERIFY(spinUntil([&] { return !f; }));

        QVERIFY(translator_->setLanguage(u"en"_s));
        f = sample(1250).ask(main_.get(), [](bool) {});
        const QString english = f->confirmButton()->text();
        f->reject();
        QVERIFY(translator_->setLanguage(u"vi"_s));
        QVERIFY2(english.contains(u"1,250"_s), qPrintable(english));
        QVERIFY(spinUntil([&] { return !f; }));
    }

    // Tiêu chí 2.
    void dontAsk()
    {
        int calls = 0;
        bool last = false;
        const auto done = [&](bool yes) {
            ++calls;
            last = yes;
        };
        QPointer<DialogFrame> f = sample(2).ask(main_.get(), done);
        QVERIFY(!f->findChild<QCheckBox*>()); // không dontAsk: không có ô
        f->reject();
        QVERIFY(spinUntil([&] { return !f; }));
        QCOMPARE(calls, 1);

        f = sample(2).dontAsk(u"test.yes"_s).ask(main_.get(), done); // đánh dấu, nút chính
        auto* again = f->findChild<QCheckBox*>(u"dontAsk"_s);
        QVERIFY(again && !again->isChecked());
        again->setChecked(true);
        f->confirmButton()->click();
        QVERIFY(spinUntil([&] { return !f; }));
        QCOMPARE(calls, 2);
        QVERIFY(last);
        QCOMPARE(sample(5).dontAsk(u"test.yes"_s).ask(main_.get(), done), nullptr); // lần sau: không hiện
        QCOMPARE(calls, 3);
        QVERIFY(last);
        QVERIFY(main_->findChildren<QDialog*>().isEmpty());

        f = sample(2).dontAsk(u"test.no"_s).ask(main_.get(), done); // đánh dấu, Hủy
        f->findChild<QCheckBox*>(u"dontAsk"_s)->setChecked(true);
        f->findChild<Button*>(u"cancel"_s)->click();
        QVERIFY(spinUntil([&] { return !f; }));
        QCOMPARE(calls, 4);
        QVERIFY(!last);
        QCOMPARE(sample(2).dontAsk(u"test.no"_s).ask(main_.get(), done), nullptr);
        QCOMPARE(calls, 5);
        QVERIFY(!last);

        f = sample(2).dontAsk(u"test.unchecked"_s).ask(main_.get(), done); // không đánh dấu: không nhớ
        f->confirmButton()->click();
        QVERIFY(spinUntil([&] { return !f; }));
        QVERIFY(!SuppressedPrompts::remembered(u"test.unchecked"_s));

        SuppressedPrompts model(*store_);
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.data(model.index(0, 0), SuppressedPrompts::IdRole).toString(), u"test.no"_s); // theo mã
        QCOMPARE(model.data(model.index(1, 0)).toString(), sampleTr(kTitle));
        QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
        model.restore(0); // khôi phục từng mục
        QCOMPARE(reset.count(), 1);
        QCOMPARE(model.rowCount(), 1);
        QVERIFY(!SuppressedPrompts::remembered(u"test.no"_s));
        QVERIFY(SuppressedPrompts::remembered(u"test.yes"_s) == true);
        f = sample(2).dontAsk(u"test.no"_s).ask(main_.get(), done); // hỏi lại
        QVERIFY(f);
        f->reject();
        QVERIFY(spinUntil([&] { return !f; }));
        model.restore(7); // ngoài khoảng: bỏ qua
        QCOMPARE(model.rowCount(), 1);
        model.restoreAll();
        QCOMPARE(model.rowCount(), 0);
        QVERIFY(!SuppressedPrompts::remembered(u"test.yes"_s));

        // Model đang mở thấy mục mới (trang Tùy chọn mở trong lúc ConfirmDialog nhớ).
        SuppressedPrompts::remember(u"live"_s, sampleTr(kTitle), true);
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.data(model.index(0, 0), Qt::ToolTipRole), QVariant());

        // Tệp cài đặt sửa tay sai kiểu: coi như chưa nhớ, không hiện trong danh sách.
        store_->setValue(u"ui/suppressedPrompts"_s, QJsonObject{{u"bad"_s, QJsonObject{{u"accepted"_s, u"yes"_s}}}});
        QVERIFY(!SuppressedPrompts::remembered(u"bad"_s));
        QCOMPARE(model.rowCount(), 0);
        f = sample(1).dontAsk(u"bad"_s).ask(main_.get(), done); // hỏi lại
        QVERIFY(f);
        f->reject();
        QVERIFY(spinUntil([&] { return !f; }));
    }

    void restart()
    {
        const QString path = dir_.filePath(u"restart.json"_s);
        {
            settings::Store a(path);
            SuppressedPrompts::remember(u"a.one"_s, sampleTr(kTitle), true, a);
            SuppressedPrompts::remember(u"a.two"_s, sampleTr(kErrTitle), false, a);
        } // ~Store ghi nốt, đồng bộ (như thoát ứng dụng)
        {
            settings::Store b(path); // khởi động lại
            SuppressedPrompts model(b);
            QCOMPARE(model.rowCount(), 2);
            QCOMPARE(model.data(model.index(1, 0)).toString(), sampleTr(kErrTitle));
            QVERIFY(SuppressedPrompts::remembered(u"a.one"_s, b) == true);
            QVERIFY(SuppressedPrompts::remembered(u"a.two"_s, b) == false);
            settings::Store::setInstance(&b); // ConfirmDialog dùng kho của tiến trình
            int got = 0;
            DialogFrame* f = sample(1).dontAsk(u"a.two"_s).ask(main_.get(), [&](bool yes) { got = yes ? 1 : 2; });
            settings::Store::setInstance(store_.get());
            QCOMPARE(f, nullptr);
            QCOMPARE(got, 2);
            model.restore(0);
            QCOMPARE(model.rowCount(), 1);
        }
        settings::Store c(path);
        QCOMPARE(SuppressedPrompts(c).rowCount(), 1);
        QVERIFY(!SuppressedPrompts::remembered(u"a.one"_s, c));
        SuppressedPrompts(c).restoreAll();
        QCOMPARE(SuppressedPrompts(c).rowCount(), 0);
    }

    // Tiêu chí 3.
    void errorDetails()
    {
        int asked = 0;
        ErrorDialog::setLogSource([&](int n) {
            asked = n;
            return fakeLog(250); // nguồn trả thừa
        });
        QPointer<ErrorDialog> f = errorDialog(main_.get());
        QCOMPARE(asked, ErrorDialog::kLogLines);
        f->open();
        QVERIFY(waitActive(f));
        QVERIFY(f->isModal());
        QCOMPARE(QApplication::focusWidget(), f->confirmButton()); // "Đóng"

        const QStringList lines = f->details().split(u'\n');
        QCOMPARE(lines.value(0), sampleTr(kErrTitle));
        QVERIFY(lines.contains(ErrorDialog::tr("Mã lỗi: %1").arg(u"io.disk-full"_s)));
        QVERIFY(lines.contains(ErrorDialog::tr("Thông điệp: %1").arg(QString::fromStdString(kDiskFull.message))));
        QVERIFY(f->details().contains(QRegularExpression(u"TrionCAD \\d+\\.\\d+\\.\\d+"_s)));
        QVERIFY(f->details().contains(QSysInfo::prettyProductName()));
        QVERIFY(f->details().contains(QSysInfo::currentCpuArchitecture()));
        QVERIFY(lines.contains(ErrorDialog::tr("%n dòng log cuối:", nullptr, 200)));
        QCOMPARE(lines.mid(lines.size() - 200), fakeLog(250).mid(50)); // đúng 200 dòng cuối, cũ trước
        QVERIFY(!lines.contains(fakeLog(50).last()));

        // Mã lỗi hiện ở thân; chi tiết thu gọn mặc định, mở thì cao thêm, đóng lại như cũ.
        QVERIFY(f->body()->findChildren<QLabel*>().value(1)->text().contains(u"io.disk-full"_s));
        auto* box = f->findChild<QPlainTextEdit*>(u"details"_s);
        auto* toggle = f->findChild<Button*>(u"detailsToggle"_s);
        QVERIFY(box && toggle);
        QVERIFY(box->isHidden());
        QCOMPARE(box->toPlainText(), f->details());
        const int collapsed = f->height();
        toggle->click();
        QVERIFY(box->isVisible());
        QVERIFY2(f->height() > collapsed, qPrintable(u"%1 → %2"_s.arg(collapsed).arg(f->height())));
        toggle->click();
        QVERIFY(box->isHidden());
        QCOMPARE(f->height(), collapsed);

        QGuiApplication::clipboard()->clear();
        auto* copy = f->findChild<Button*>(u"copy"_s);
        const QString copyLabel = copy->text();
        copy->click();
        QCOMPARE(QGuiApplication::clipboard()->text(), f->details());
        QVERIFY(copy->text() != copyLabel); // "Đã sao chép"
        QTest::keyClick(f, Qt::Key_Escape);
        QVERIFY(spinUntil([&] { return !f; }));

        ErrorDialog::setLogSource({}); // NT-009 chưa nối
        f = errorDialog(main_.get());
        QCOMPARE(f->details().split(u'\n').last(), ErrorDialog::tr("Chưa có log."));
        delete f;
    }

    void errorReport()
    {
        QPointer<ErrorDialog> f = errorDialog(main_.get()); // PLF-010 chưa nối: không có nút
        QVERIFY(!f->findChild<Button*>(u"report"_s));
        delete f;

        int calls = 0;
        QString sent;
        QWidget* parent = nullptr;
        ErrorDialog::setReportHandler([&](const QString& details, QWidget* p) {
            ++calls;
            sent = details;
            parent = p;
        });
        f = errorDialog(main_.get()); // mở, xem chi tiết, chép, đóng: không gọi luồng gửi
        f->open();
        QVERIFY(waitActive(f));
        f->findChild<Button*>(u"detailsToggle"_s)->click();
        f->findChild<Button*>(u"copy"_s)->click();
        f->confirmButton()->click();
        QVERIFY(spinUntil([&] { return !f; }));
        QCOMPARE(calls, 0);

        f = errorDialog(main_.get());
        f->open();
        QVERIFY(waitActive(f));
        auto* report = f->findChild<Button*>(u"report"_s);
        QVERIFY(report && report->isVisible());
        report->click();
        QCOMPARE(calls, 1);
        QCOMPARE(sent, f->details());
        QCOMPARE(parent, f.data());
        QVERIFY(f->isVisible()); // luồng UI-L08 mở trên hộp thoại lỗi; đóng do người dùng
        f->reject();
        QVERIFY(spinUntil([&] { return !f; }));
        QCOMPARE(calls, 1);
    }

    // Tiêu chí 4.
    void focusKeys()
    {
        int calls = 0;
        bool last = true;
        const auto done = [&](bool yes) {
            ++calls;
            last = yes;
        };
        QPointer<DialogFrame> f = sample(4).irreversible().danger().ask(main_.get(), done);
        QVERIFY(waitActive(f));
        QCOMPARE(f->confirmButton()->kind(), Button::Kind::Danger);
        auto* cancel = f->findChild<Button*>(u"cancel"_s);
        QCOMPARE(QApplication::focusWidget(), cancel);
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Return); // Enter không làm nhầm
        QVERIFY(spinUntil([&] { return !f; }));
        QCOMPARE(calls, 1);
        QVERIFY(!last);

        f = sample(4).ask(main_.get(), done);
        QVERIFY(waitActive(f));
        QCOMPARE(f->confirmButton()->kind(), Button::Kind::Primary);
        QCOMPARE(QApplication::focusWidget(), f->confirmButton());
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Return);
        QVERIFY(spinUntil([&] { return !f; }));
        QCOMPARE(calls, 2);
        QVERIFY(last);

        for (bool irreversible : {false, true}) {
            f = sample(4).irreversible(irreversible).dontAsk(u"esc"_s).ask(main_.get(), done);
            QVERIFY(waitActive(f));
            QTest::keyClick(QApplication::focusWidget(), Qt::Key_Escape); // Esc = Hủy
            QVERIFY(spinUntil([&] { return !f; }));
            QVERIFY(!last);
        }
        QCOMPARE(calls, 4);
        QVERIFY(!SuppressedPrompts::remembered(u"esc"_s)); // không đánh dấu: không nhớ
    }

    void openTime()
    {
        ErrorDialog::setLogSource([](int n) { return fakeLog(n); });
        ErrorDialog::setReportHandler([](const QString&, QWidget*) {});
        const auto [c50, c99] = measureOpen("xác nhận", [this] {
            return sample(1250).irreversible().danger().dontAsk(u"time"_s).ask(main_.get(), [](bool) {});
        });
        const auto [e50, e99] = measureOpen("lỗi", [this] {
            ErrorDialog* f = errorDialog(main_.get());
            f->open();
            return f;
        });
        QVERIFY(c50 >= 0 && e50 >= 0);
        QVERIFY2(!kTimed || c99 <= kOpenBudgetMs, qPrintable(QString::number(c99)));
        QVERIFY2(!kTimed || e99 <= kOpenBudgetMs, qPrintable(QString::number(e99)));
    }

    // Tiêu chí 5 (DoD-U 1): ảnh theo loại × chủ đề × DPR.
    void baseline_data()
    {
        QTest::addColumn<QString>("kind");
        QTest::addColumn<bool>("light");
        QTest::addColumn<qreal>("dpr");
        for (const char* kind : {"confirm", "error"})
            for (bool light : {false, true})
                for (qreal dpr : {1.0, 1.5, 2.0})
                    QTest::addRow("%s-%s-%d", kind, light ? "light" : "dark", qRound(dpr * 100))
                        << QString::fromLatin1(kind) << light << dpr;
    }

    void baseline()
    {
        QFETCH(QString, kind);
        QFETCH(bool, light);
        QFETCH(qreal, dpr);
        tm_->setTheme(light ? Theme::Light : Theme::Dark);
        QCoreApplication::processEvents(); // Qt 6.11 gửi ApplicationPaletteChange qua hàng đợi
        ErrorDialog::setReportHandler([](const QString&, QWidget*) {});
        QPointer<DialogFrame> f;
        if (kind == "confirm"_L1) {
            f = sample(1250).danger().dontAsk(u"shot"_s).ask(main_.get(), [](bool) {});
        } else {
            f = errorDialog(main_.get());
            f->open();
        }
        QVERIFY(QTest::qWaitForWindowExposed(f));
        QImage shot(f->size() * dpr, QImage::Format_ARGB32_Premultiplied);
        shot.setDevicePixelRatio(dpr);
        shot.fill(tm_->color(tok::c_canvas)); // nền vùng vẽ dưới hộp thoại
        f->render(&shot);
        f->reject();
        QVERIFY(spinUntil([&] { return !f; }));
        const QString name = u"%1-%2-%3-%4.png"_s.arg(kind, light ? u"light"_s : u"dark"_s)
                                 .arg(qRound(dpr * 100))
                                 .arg(QLatin1StringView(kPlatform));
        const QString out = qEnvironmentVariable("TRION_UPDATE_BASELINE");
        if (!out.isEmpty()) {
            QVERIFY(shot.save(out + u'/' + name, "PNG", 0)); // nén cao nhất
            QSKIP("đã ghi baseline");
        }
        const QImage want = QImage(u":/baseline/"_s + name).convertToFormat(QImage::Format_ARGB32_Premultiplied);
        QVERIFY2(!want.isNull(), qPrintable(u"thiếu baseline "_s + name));
        QCOMPARE(shot.size(), want.size());
        qsizetype off = 0;
        int maxDiff = 0;
        for (int y = 0; y < shot.height(); ++y) {
            const auto* a = reinterpret_cast<const QRgb*>(shot.constScanLine(y));
            const auto* b = reinterpret_cast<const QRgb*>(want.constScanLine(y));
            for (int x = 0; x < shot.width(); ++x) {
                const int d = std::max({std::abs(qRed(a[x]) - qRed(b[x])), std::abs(qGreen(a[x]) - qGreen(b[x])),
                                        std::abs(qBlue(a[x]) - qBlue(b[x])), std::abs(qAlpha(a[x]) - qAlpha(b[x]))});
                maxDiff = std::max(maxDiff, d);
                off += d > kNoise;
            }
        }
        qInfo("%s: %lld điểm ảnh lệch > %d/255, lệch lớn nhất %d/255", qPrintable(name), qint64(off), kNoise, maxDiff);
        if (off > shot.width() * shot.height() / 1000)
            shot.save(QDir::temp().filePath(u"got-"_s + name)); // để so bằng mắt
        QVERIFY(off <= shot.width() * shot.height() / 1000);
    }

    void pseudoNoTruncation()
    {
        ErrorDialog::setLogSource([](int n) { return fakeLog(n); });
        ErrorDialog::setReportHandler([](const QString&, QWidget*) {});
        QVERIFY(translator_->setLanguage(i18n::kPseudoLanguage.toString()));
        QCoreApplication::processEvents();
        QPointer<DialogFrame> confirm = sample(1250).danger().dontAsk(u"xl"_s).ask(main_.get(), [](bool) {});
        QVERIFY(QTest::qWaitForWindowExposed(confirm));
        const QString confirmCut = truncations(confirm);
        const QString label = confirm->confirmButton()->text();
        confirm->reject();
        QVERIFY(spinUntil([&] { return !confirm; }));
        QPointer<ErrorDialog> error = errorDialog(main_.get());
        error->open();
        QVERIFY(QTest::qWaitForWindowExposed(error));
        const QString errorCut = truncations(error);
        error->findChild<Button*>(u"detailsToggle"_s)->click();
        QCoreApplication::processEvents();
        const QString expandedCut = truncations(error);
        error->reject();
        QVERIFY(translator_->setLanguage(u"vi"_s));
        QVERIFY(spinUntil([&] { return !error; }));
        QVERIFY2(label.size() > QStringLiteral("Xóa 1.250 layout").size(), qPrintable(label)); // vi-XL có hiệu lực
        QCOMPARE(confirmCut, QString());
        QCOMPARE(errorCut, QString());
        QCOMPARE(expandedCut, QString());
    }
};

QTEST_MAIN(ConfirmTest)
#include "confirm_test.moc"
