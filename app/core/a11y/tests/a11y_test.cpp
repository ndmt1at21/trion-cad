// UI0-013 · Khả năng tiếp cận (AccessibilityAudit.h, Announce.h). Tiêu chí nghiệm thu:
//   1  galleryAudit, dialogsAudit (cổng CI): bảng mẫu và mọi hộp thoại của app/widgets/dialog (DialogFrame mẫu, đang
//      tải, ConfirmDialog, ErrorDialog thu gọn và mở chi tiết): 100% phần tử nhận focus có tên và vai trò, Tab đi hết
//      vòng, 0 bẫy focus. auditCatches: audit bắt từng loại vi phạm (thêm vi phạm thì test đỏ).
//   2  galleryAudit, dialogsAudit chạy ở Tối và Sáng: Tab tới mỗi điểm dừng thì quanh nó có viền --c-focus (so ảnh
//      trước và sau khi focus). Tương phản --c-focus với các nền ≥ 3:1 ở hai chủ đề: kiểm lúc build
//      (app/theme/contrast-pairs.txt, tokengen làm build lỗi), không lặp ở đây.
//   3  reducedMotion: bật giảm chuyển động lúc chạy → mọi --t* = 0 ms; animation đang chạy (khung trượt của dải tự ẩn
//      UI0-010, vòng quay "Đang tải…", QPropertyAnimation, QVariantAnimation vô hạn, nhóm) dừng ở cuối ngay trong lời
//      gọi; mở lại thì không trượt. Toast (UI0-009), tiến trình (UI0-112) không có animation.
//   4  treeBaseline: cây QAccessible của bảng mẫu khớp tests/baseline/gallery-<vi|en>.json. Ghi lại:
//      TRION_UPDATE_BASELINE=<thư mục> chạy test, chép gallery-*.json vào tests/baseline.
//   announce: a11y::announce phát QAccessibleAnnouncementEvent đúng nguồn, mức; toast; tiến trình đọc lúc bắt đầu
//      (một lần, không mỗi phần trăm), xong/lỗi qua toast của ToastHost::followJobs.
#include "app/core/a11y/AccessibilityAudit.h"
#include "app/core/a11y/Announce.h"

#include "app/core/i18n/Translator.h"
#include "app/core/keymap/Keymap.h"
#include "app/core/UiExecutor.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/dialog/ConfirmDialog.h"
#include "app/widgets/dialog/DialogFrame.h"
#include "app/widgets/dialog/ErrorDialog.h"
#include "app/widgets/dock/AutoHideStrip.h"
#include "app/widgets/gallery/Gallery.h"
#include "app/widgets/message/Messages.h"
#include "app/widgets/model/BackgroundModel.h"
#include "app/widgets/progress/InlineProgress.h"
#include "app/widgets/progress/ProgressPresenter.h"
#include "app/widgets/tooltip/RichTooltip.h"
#include "core/base/sched/Scheduler.h"
#include "core/base/settings/Settings.h"

#include <QAbstractItemView>
#include <QAccessible>
#include <QApplication>
#include <QBoxLayout>
#include <QCheckBox>
#include <QDockWidget>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QRadioButton>
#include <QSequentialAnimationGroup>
#include <QStyle>
#include <QTemporaryDir>
#include <QToolButton>
#include <QtTest>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;
using namespace trion::theme;
using namespace trion::ui;
namespace a11y = trion::a11y;
namespace i18n = trion::i18n;
namespace jobs = trion::jobs;
namespace sched = trion::sched;
namespace settings = trion::settings;

namespace {

// Vòng sự kiện thật đến khi cond() đúng hoặc quá hạn (như gallery_test).
bool spinUntil(const std::function<bool()>& cond, std::chrono::milliseconds timeout = 20s)
{
    if (cond())
        return true;
    QEventLoop loop;
    QTimer tick, deadline;
    QObject::connect(&tick, &QTimer::timeout, &loop, [&] {
        if (cond())
            loop.quit();
    });
    deadline.setSingleShot(true);
    QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
    deadline.start(timeout);
    tick.start(1);
    loop.exec();
    return cond();
}

// QTest::qWaitForWindowActive đúng ngay khi cửa sổ cha (transient parent) đang active: chờ chính widget active.
bool waitActive(QWidget* w)
{
    w->activateWindow();
    return spinUntil([w] { return w->isActiveWindow(); });
}

// Bộ bắt sự kiện tiếp cận.
struct Said {
    QObject* source;
    QString message;
    QAccessible::AnnouncementPoliteness politeness;
};
std::vector<Said> g_said;

void record(QAccessibleEvent* e)
{
    if (e->type() == QAccessible::Announcement) {
        const auto* a = static_cast<QAccessibleAnnouncementEvent*>(e);
        g_said.push_back({e->object(), a->message(), a->politeness()});
    }
}

qsizetype saidFrom(QObject* source)
{
    return std::count_if(g_said.begin(), g_said.end(), [source](const Said& s) { return s.source == source; });
}

const Said* saidText(const QString& text)
{
    for (const Said& s : g_said)
        if (s.message == text)
            return &s;
    return nullptr;
}

// Việc nền do test điều khiển: báo progress khi đổi, chạy đến khi release; fail thì trả lỗi.
struct Gate {
    std::atomic<bool> release{false};
    std::atomic<bool> fail{false};
    std::atomic<double> progress{0};
};

jobs::JobId startGated(jobs::BackgroundJobs& r, const QString& name, const std::shared_ptr<Gate>& g)
{
    return r.start(name, u"test"_s, 0, [g](jobs::Progress& p, sched::CancelToken t) -> trion::Expected<void> {
        double last = -1;
        while (!g->release.load() && !t.cancelled()) {
            if (const double v = g->progress.load(); v != last) {
                p.set(v);
                last = v;
            }
            std::this_thread::sleep_for(1ms);
        }
        if (g->fail.load())
            return trion::Error{"test", "disk-full", "No space left on device"};
        return {};
    });
}

// Mọi animation trong cây QObject của các cửa sổ cấp cao.
QList<QAbstractAnimation*> animations()
{
    QList<QAbstractAnimation*> all;
    for (QWidget* top : QApplication::topLevelWidgets())
        all += top->findChildren<QAbstractAnimation*>();
    return all;
}

// Hộp thoại mẫu: ô một dòng, ô nhiều dòng (nhãn buddy), hộp kiểm; nút Hủy và nút xác nhận.
DialogFrame* sampleFrame(QWidget* parent)
{
    auto* f = new DialogFrame(u"a11y-sample"_s, parent);
    f->setModal(true);
    f->setCommand(u"SAMPLE"_s);
    f->setWindowTitle(u"Tạo block"_s); // i18n-ok: chữ mẫu của test
    auto* body = new QWidget;
    auto* v = new QVBoxLayout(body);
    v->setContentsMargins(0, 0, 0, 0);
    auto* nameLabel = new QLabel(u"Tên &block"_s); // i18n-ok: chữ mẫu của test
    auto* name = new QLineEdit;
    nameLabel->setBuddy(name);
    auto* noteLabel = new QLabel(u"&Mô tả"_s); // i18n-ok: chữ mẫu của test
    auto* note = new QPlainTextEdit;
    note->setFixedHeight(60);
    noteLabel->setBuddy(note);
    auto* keep = new QCheckBox(u"Giữ đối tượng"_s); // i18n-ok: chữ mẫu của test
    for (QWidget* w : {static_cast<QWidget*>(nameLabel), static_cast<QWidget*>(name), static_cast<QWidget*>(noteLabel),
                       static_cast<QWidget*>(note), static_cast<QWidget*>(keep)})
        v->addWidget(w);
    f->setBody(body);
    f->addCancel();
    f->setConfirm(u"Tạo block"_s); // i18n-ok: chữ mẫu của test
    return f;
}

} // namespace

class A11yTest : public QObject {
    Q_OBJECT
    QTemporaryDir dir_;
    std::unique_ptr<settings::Store> store_;
    std::unique_ptr<UiExecutor> executor_;
    std::unique_ptr<i18n::Translator> translator_;
    std::unique_ptr<ThemeManager> tm_;
    std::unique_ptr<IconProvider> icons_;
    std::unique_ptr<trion::keymap::Keymap> keys_;
    std::unique_ptr<RichTooltip> tips_;
    std::unique_ptr<GalleryWindow> gallery_;
    std::unique_ptr<QWidget> main_; // cha của hộp thoại

    bool galleryReady()
    {
        return spinUntil([this] {
            if (!gallery_->isActiveWindow() || !gallery_->isStaged())
                return false;
            for (const char* name : {"live/table", "live/tree"}) {
                auto* view = gallery_->findChild<QAbstractItemView*>(QLatin1StringView(name));
                auto* m = view ? qobject_cast<BackgroundModelBase*>(view->model()) : nullptr;
                if (!m || m->busy() || m->rowCount() == 0)
                    return false;
            }
            return true;
        });
    }

    bool setLanguage(const QString& lang)
    {
        if (translator_->language() == lang)
            return true;
        QWidget* before = gallery_->page();
        return translator_->setLanguage(lang) && spinUntil([&] { return gallery_->page() != before; }) &&
               galleryReady();
    }

private slots:
    void initTestCase()
    {
        QVERIFY(dir_.isValid());
        store_ = std::make_unique<settings::Store>(dir_.filePath(u"user.json"_s));
        settings::Store::setInstance(store_.get());
        executor_ = std::make_unique<UiExecutor>();
        translator_ = std::make_unique<i18n::Translator>();
        QVERIFY(loadFonts());
        tm_ = std::make_unique<ThemeManager>(*store_);
        icons_ = std::make_unique<IconProvider>();
        keys_ = std::make_unique<trion::keymap::Keymap>(*store_);
        tips_ = std::make_unique<RichTooltip>();
        tips_->setKeymap(keys_.get());
        QCursor::setPos(-1000, -1000); // con trỏ ngoài cửa sổ: không gì "đang rê"
        gallery_ = std::make_unique<GalleryWindow>();
        gallery_->resize(1366, 768);
        gallery_->show();
        QVERIFY(waitActive(gallery_.get()));
        QVERIFY(galleryReady());
        main_ = std::make_unique<QWidget>();
        main_->resize(600, 400);
        main_->move(100, 80);
    }

    void init()
    {
        tm_->setTheme(Theme::Dark);
        tm_->setDensity(Density::Compact);
        tm_->setLite(false);
        tm_->setReducedMotion(false);
        // Bảng mẫu chụp lại ở nền khi chủ đề đổi (mở popup tạm, lấy cửa sổ active): chờ chụp xong rồi mới hiện cửa sổ khác,
        // không thì lần kích hoạt cửa sổ đó có thể mất (Windows).
        QVERIFY(waitActive(gallery_.get()) && galleryReady());
        QVERIFY(setLanguage(u"vi"_s));
    }

    void cleanupTestCase()
    {
        main_.reset();
        gallery_.reset();
        tips_.reset();
        keys_.reset();
        icons_.reset();
        tm_.reset();
        translator_.reset();
        settings::Store::setInstance(nullptr);
        store_.reset();
        sched::waitIdle();
        QCoreApplication::processEvents();
        executor_.reset();
    }

    // Tiêu chí 1, 2: bảng mẫu.
    void galleryAudit_data()
    {
        QTest::addColumn<Theme>("theme");
        QTest::newRow("dark") << Theme::Dark;
        QTest::newRow("light") << Theme::Light;
    }
    void galleryAudit()
    {
        QFETCH(Theme, theme);
        tm_->setTheme(theme);
        QVERIFY(waitActive(gallery_.get()));
        QVERIFY(galleryReady());
        QStringList order;
        const QStringList issues = a11y::checkNames(gallery_.get()) + a11y::checkFocus(gallery_.get(), &order);
        QVERIFY2(issues.isEmpty(), qPrintable(issues.join(u'\n')));
        QCOMPARE(order.size(), 21); // như gallery_test: thanh chọn 4, Dùng thử 15, bảng, cây
        qInfo("bảng mẫu: %lld điểm dừng Tab", qint64(order.size()));
    }

    // Tiêu chí 1, 2: mọi hộp thoại của app/widgets/dialog.
    void dialogsAudit_data()
    {
        QTest::addColumn<Theme>("theme");
        QTest::addColumn<QString>("kind");
        for (const auto& [theme, t] : {std::pair{Theme::Dark, "dark"}, std::pair{Theme::Light, "light"}})
            for (const char* kind : {"frame", "loading", "confirm", "confirm-full", "error", "error-details"})
                QTest::addRow("%s-%s", t, kind) << theme << QString::fromLatin1(kind);
    }
    void dialogsAudit()
    {
        QFETCH(Theme, theme);
        QFETCH(QString, kind);
        tm_->setTheme(theme);
        QVERIFY(galleryReady()); // xem init()
        main_->show();
        QVERIFY(waitActive(main_.get()));
        ErrorDialog::setReportHandler([](const QString&, QWidget*) {}); // có nút "Gửi báo lỗi"
        QPointer<DialogFrame> d;
        if (kind == u"frame"_s || kind == u"loading"_s) {
            d = sampleFrame(main_.get());
            if (kind == u"loading"_s)
                d->setLoading();
            d->open();
        } else if (kind.startsWith(u"confirm"_s)) {
            ConfirmDialog c(u"Xóa layout"_s, u"Xóa %1 layout"_s, 3); // i18n-ok: chữ mẫu của test
            c.question(u"Xóa %1 layout đã chọn?"_s).detail(u"Hoàn tác được bằng Ctrl+Z."_s); // i18n-ok: chữ mẫu
            if (kind == u"confirm-full"_s)
                c.danger().irreversible().dontAsk(u"a11y.test"_s);
            d = c.ask(main_.get(), [](bool) {});
        } else {
            d = new ErrorDialog(u"Không lưu được bản vẽ"_s, u"Ổ đĩa đầy."_s, // i18n-ok: chữ mẫu của test
                                trion::Error{"io", "disk-full", "No space left on device"}, main_.get());
            d->open();
            if (kind == u"error-details"_s) {
                auto* toggle = d->findChild<QPushButton*>(u"detailsToggle"_s);
                QVERIFY(toggle);
                toggle->click();
                QVERIFY(d->findChild<QPlainTextEdit*>(u"details"_s)->isVisible());
            }
        }
        QVERIFY(d);
        QVERIFY(waitActive(d));
        QStringList order;
        const QStringList issues = a11y::checkNames(d) + a11y::checkFocus(d, &order);
        const QString title = d->windowTitle();
        d->reject();
        delete d.data(); // ConfirmDialog, ErrorDialog tự xóa khi đóng (deleteLater): QPointer đã rỗng thì bỏ qua
        ErrorDialog::setReportHandler({});
        main_->hide();
        QVERIFY2(issues.isEmpty(), qPrintable(title + u":\n"_s + issues.join(u'\n')));
        QVERIFY(order.size() >= 3); // Trợ giúp hoặc Đóng, nút chân
        qInfo("%s: %lld điểm dừng Tab", qPrintable(kind), qint64(order.size()));
    }

    // Tiêu chí 1: audit bắt được vi phạm, nên thêm vi phạm vào bảng mẫu hay hộp thoại thì test trên đỏ.
    void auditCatches()
    {
        QWidget w;
        auto* v = new QVBoxLayout(&w);
        auto* named = new QPushButton(u"Lưu"_s, &w); // i18n-ok: chữ mẫu của test
        auto* plain = new QWidget(&w);               // widget tự làm: không tên, vai trò Client, không viền focus
        plain->setObjectName(u"plain"_s);
        plain->setFocusPolicy(Qt::StrongFocus);
        plain->setMinimumSize(40, 20);
        auto* icon = new QPushButton(&w); // nút chỉ biểu tượng quên setAccessibleName
        icon->setObjectName(u"icon"_s);
        auto* trap = new QPlainTextEdit(&w); // giữ Tab để chèn ký tự tab
        trap->setObjectName(u"trap"_s);
        trap->setAccessibleName(u"Ghi chú"_s); // i18n-ok: chữ mẫu của test
        for (QWidget* c : {static_cast<QWidget*>(named), plain, static_cast<QWidget*>(icon), static_cast<QWidget*>(trap)})
            v->addWidget(c);
        w.show();
        QVERIFY(waitActive(&w));
        const QString all = (a11y::checkNames(&w) + a11y::checkFocus(&w)).join(u'\n');
        for (const QString& want : {u"QWidget 'plain': không có tên"_s, u"QWidget 'plain': không có vai trò"_s,
                                    u"QWidget 'plain': không thấy viền"_s, u"QPushButton 'icon': không có tên"_s,
                                    u"QPlainTextEdit 'trap': bẫy focus"_s})
            QVERIFY2(all.contains(want), qPrintable(want + u"\n--- có:\n"_s + all));
        QVERIFY2(!all.contains(u"QPushButton '': "_s), qPrintable(all)); // nút có chữ: đạt
    }

    // Tiêu chí 2: widget Qt chuẩn (hộp thoại dùng) có viền --c-focus khi Tab tới, qua TrionStyle (ThemeManager).
    void standardWidgets_data()
    {
        QTest::addColumn<Theme>("theme");
        QTest::newRow("dark") << Theme::Dark;
        QTest::newRow("light") << Theme::Light;
    }
    void standardWidgets()
    {
        QFETCH(Theme, theme);
        tm_->setTheme(theme);
        QVERIFY(galleryReady()); // xem init()
        QWidget w;
        auto* v = new QVBoxLayout(&w);
        auto* push = new QPushButton(u"Lưu"_s); // i18n-ok: chữ mẫu của test
        auto* tool = new QToolButton;
        tool->setText(u"Chọn"_s); // i18n-ok: chữ mẫu của test
        tool->setFocusPolicy(Qt::StrongFocus);
        auto* check = new QCheckBox(u"Giữ đối tượng"_s);  // i18n-ok: chữ mẫu của test
        auto* radio = new QRadioButton(u"Theo layer"_s);  // i18n-ok: chữ mẫu của test
        auto* lineLabel = new QLabel(u"Tên"_s);            // i18n-ok: chữ mẫu của test
        auto* line = new QLineEdit;
        lineLabel->setBuddy(line);
        auto* textLabel = new QLabel(u"Ghi chú"_s); // i18n-ok: chữ mẫu của test
        auto* text = new QPlainTextEdit;
        text->setTabChangesFocus(true);
        text->setFixedHeight(60);
        textLabel->setBuddy(text);
        for (QWidget* c : std::initializer_list<QWidget*>{push, tool, check, radio, lineLabel, line, textLabel, text})
            v->addWidget(c);
        w.show();
        QVERIFY(waitActive(&w));
        QStringList order;
        const QStringList issues = a11y::checkNames(&w) + a11y::checkFocus(&w, &order);
        QVERIFY2(issues.isEmpty(), qPrintable(issues.join(u'\n')));
        QCOMPARE(order.size(), 6);
    }

    // Tiêu chí 3.
    void reducedMotion()
    {
        QVERIFY(tm_->ms(tok::t) > 0);
        QMainWindow window;
        window.resize(900, 600);
        auto* dock = new QDockWidget(&window);
        dock->setObjectName(u"palette"_s);
        dock->setWidget(new QWidget);
        window.addDockWidget(Qt::RightDockWidgetArea, dock);
        auto* strip = new AutoHideStrip(Qt::RightDockWidgetArea, &window);
        auto* box = new QWidget(&window);
        window.setCentralWidget(box);
        NotificationCenter center;
        ToastHost toasts(&window, center);
        jobs::BackgroundJobs registry;
        ProgressPresenter presenter(registry);
        auto* progress = new InlineProgress(presenter, 0, box);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        strip->addPalette(dock);
        strip->open(dock); // trượt ra trong --t

        auto* move = new QPropertyAnimation(box, "minimumHeight", box);
        move->setDuration(10'000);
        move->setStartValue(0);
        move->setEndValue(100);
        move->start();
        auto* spin = new QVariantAnimation(box); // vòng lặp vô hạn
        spin->setStartValue(0.0);
        spin->setEndValue(360.0);
        spin->setDuration(800);
        spin->setLoopCount(-1);
        spin->start();
        auto* group = new QSequentialAnimationGroup(box);
        for (int i = 0; i < 2; ++i) {
            auto* step = new QVariantAnimation(group);
            step->setStartValue(0);
            step->setEndValue(10);
            step->setDuration(5'000);
            group->addAnimation(step);
        }
        group->start();
        DialogFrame* loading = sampleFrame(&window);
        loading->setLoading();
        loading->show(); // vòng quay "Đang tải…" chạy
        Toast* toast = toasts.post(Severity::Info, u"Đã lưu"_s); // i18n-ok: chữ mẫu của test

        QStringList running;
        for (QAbstractAnimation* a : animations())
            if (a->state() == QAbstractAnimation::Running && !a->group())
                running << QLatin1StringView(a->metaObject()->className());
        QVERIFY2(running.size() >= 5, qPrintable(running.join(u", "_s))); // trượt, 3 mẫu, vòng quay
        QVERIFY(toast->findChildren<QAbstractAnimation*>().isEmpty());    // toast, tiến trình không animation
        QVERIFY(progress->findChildren<QAbstractAnimation*>().isEmpty());

        tm_->setReducedMotion(true); // DisplayContext gọi khi hệ điều hành đổi
        for (tok::MsId t : {tok::t, tok::t_fast})
            QCOMPARE(tm_->ms(t), 0);
        QCOMPARE(QApplication::style()->styleHint(QStyle::SH_Widget_Animation_Duration), 0);
        QVERIFY(!QApplication::isEffectEnabled(Qt::UI_AnimateMenu));
        QStringList still;
        for (QAbstractAnimation* a : animations())
            if (a->state() != QAbstractAnimation::Stopped)
                still << QLatin1StringView(a->metaObject()->className());
        QVERIFY2(still.isEmpty(), qPrintable(still.join(u", "_s))); // ngay trong lời gọi, trước khung kế
        QCOMPARE(box->minimumHeight(), 100);                          // hữu hạn: tới giá trị cuối
        QCOMPARE(strip->flyout()->geometry(), strip->flyoutRect());

        strip->collapse();
        strip->open(dock); // mở lại: hiện ngay, không trượt
        QCOMPARE(strip->flyout()->geometry(), strip->flyoutRect());
        for (QAbstractAnimation* a : animations())
            QCOMPARE(a->state(), QAbstractAnimation::Stopped);

        tm_->setReducedMotion(false);
        QVERIFY(tm_->ms(tok::t) > 0);
        delete loading;
    }

    // Tiêu chí 4.
    void treeBaseline_data()
    {
        QTest::addColumn<QString>("lang");
        QTest::newRow("vi") << u"vi"_s;
        QTest::newRow("en") << u"en"_s;
    }
    void treeBaseline()
    {
        QFETCH(QString, lang);
        QVERIFY(setLanguage(lang));
        const QByteArray json = a11y::treeJson(gallery_.get());
        QVERIFY(json.contains("\"role\": \"Window\""));
        const QString name = u"gallery-%1.json"_s.arg(lang);
        if (const QString out = qEnvironmentVariable("TRION_UPDATE_BASELINE"); !out.isEmpty()) {
            QFile f(out + u'/' + name);
            QVERIFY(f.open(QIODevice::WriteOnly) && f.write(json) == json.size());
            QSKIP("đã ghi baseline");
        }
        QFile f(u":/baseline/"_s + name);
        QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(name + u": chưa có baseline"_s));
        const QByteArray want = f.readAll();
        if (json != want) { // ghi bản thực tế cạnh test để so
            QFile got(name);
            QVERIFY(got.open(QIODevice::WriteOnly));
            got.write(json);
            const QList<QByteArray> a = json.split('\n'), b = want.split('\n');
            qsizetype i = 0;
            while (i < a.size() && i < b.size() && a[i] == b[i])
                ++i;
            QFAIL(qPrintable(u"%1 khác baseline từ dòng %2: '%3' / baseline '%4' (bản thực tế: %5)"_s.arg(
                name, QString::number(i + 1), QString::fromUtf8(a.value(i)).trimmed(),
                QString::fromUtf8(b.value(i)).trimmed(), QFileInfo(got).absoluteFilePath())));
        }
    }

    void announce()
    {
        g_said.clear();
        QAccessible::installUpdateHandler(record);
        QAccessible::setActive(true);
        QWidget window;
        window.resize(800, 500);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        a11y::announce(&window, u"một"_s);                                 // i18n-ok: chữ mẫu của test
        a11y::announce(&window, u"hai"_s, a11y::Politeness::Assertive);    // i18n-ok: chữ mẫu của test
        a11y::announce(nullptr, u"ba"_s);                                  // i18n-ok: chữ mẫu của test
        a11y::announce(&window, QString());                                // rỗng: không phát
        QCOMPARE(int(g_said.size()), 3);
        QCOMPARE(g_said[0].source, &window);
        QCOMPARE(g_said[0].politeness, QAccessible::AnnouncementPoliteness::Polite);
        QCOMPARE(g_said[1].politeness, QAccessible::AnnouncementPoliteness::Assertive);
        QCOMPARE(g_said[2].source, QCoreApplication::instance());

        // Toast (UI0-009): lỗi Assertive, khác Polite; nguồn là toast.
        NotificationCenter center;
        ToastHost toasts(&window, center);
        Toast* ok = toasts.post(Severity::Ok, u"Đã lưu"_s);                // i18n-ok: chữ mẫu của test
        Toast* err = toasts.post(Severity::Error, u"Không lưu được"_s);    // i18n-ok: chữ mẫu của test
        QCOMPARE(saidFrom(ok), 1);
        QCOMPARE(saidText(u"Đã lưu"_s)->politeness, QAccessible::AnnouncementPoliteness::Polite);
        QCOMPARE(saidFrom(err), 1);
        QCOMPARE(saidText(u"Không lưu được"_s)->politeness, QAccessible::AnnouncementPoliteness::Assertive);

        // Tiến trình (UI0-112): việc qua mốc hiện tiến trình → đọc tên việc một lần; phần trăm đổi không đọc; xong và
        // lỗi đọc qua toast của followJobs.
        jobs::BackgroundJobs registry(jobs::Timing{.busyCursor = 10ms, .showProgress = 30ms, .batchInterval = 5ms});
        ProgressPresenter presenter(registry, &window);
        toasts.followJobs(presenter);
        auto done = std::make_shared<Gate>(), failed = std::make_shared<Gate>();
        g_said.clear();
        const jobs::JobId a = startGated(registry, u"Đang mở"_s, done);   // i18n-ok: chữ mẫu của test
        const jobs::JobId b = startGated(registry, u"Đang lưu"_s, failed); // i18n-ok: chữ mẫu của test
        QVERIFY(spinUntil([&] { return presenter.shown(a) && presenter.shown(b); }));
        for (double p : {0.1, 0.35, 0.6, 0.9}) { // nhiều lô phần trăm
            done->progress = p;
            const int at = ProgressPresenter::percent(p);
            QVERIFY(spinUntil([&] { return presenter.job(a) && ProgressPresenter::percent(presenter.job(a)->progress) == at; }));
        }
        QCOMPARE(int(g_said.size()), 2); // chỉ hai lần bắt đầu
        QCOMPARE(g_said[0].message, u"Đang mở"_s);
        QCOMPARE(g_said[0].source, &window);
        QCOMPARE(g_said[1].message, u"Đang lưu"_s);
        failed->fail = true;
        failed->release = true;
        done->release = true;
        QVERIFY(spinUntil([&] { return registry.size() == 0 && g_said.size() >= 4; }));
        QCOMPARE(int(g_said.size()), 4); // xong: một toast Polite; lỗi: một toast Assertive nêu lý do
        const Said& end1 = g_said[2].politeness == QAccessible::AnnouncementPoliteness::Polite ? g_said[2] : g_said[3];
        const Said& end2 = &end1 == &g_said[2] ? g_said[3] : g_said[2];
        QVERIFY2(qobject_cast<Toast*>(end1.source) && end1.message.contains(u"Đang mở"_s), qPrintable(end1.message));
        QCOMPARE(end2.politeness, QAccessible::AnnouncementPoliteness::Assertive);
        QVERIFY2(end2.message.contains(u"No space left on device"_s), qPrintable(end2.message));

        QAccessible::installUpdateHandler(nullptr);
        QAccessible::setActive(false);
        sched::waitIdle();
    }
};

QTEST_MAIN(A11yTest)
#include "a11y_test.moc"
