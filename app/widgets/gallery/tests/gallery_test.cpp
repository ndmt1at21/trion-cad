// UI0-006 · Bảng mẫu thành phần (Gallery.h). Tiêu chí nghiệm thu:
//   1  contents: 13 hàng điều khiển × 8 trạng thái là ảnh, trừ đúng các ô không áp dụng (docs/ui/thanh-phan.md);
//      menu, tooltip mức 1 và 2, bảng và cây (trạng thái, vô hiệu, rỗng) có ảnh; mục Dùng thử đủ bản thật.
//      switches: thanh chọn đổi chủ đề, mật độ, Giao diện nhẹ, ngôn ngữ ngay trong trang.
//   2  baseline: ảnh cả trang Tối/Sáng × gọn/thoáng × 100/150/200% so baseline theo nền tảng (như controls_test:
//      lệch ≤ 8/255 mỗi kênh bỏ qua, điểm lệch hơn ≤ 0,1% ảnh). Ghi lại: TRION_UPDATE_BASELINE=<thư mục> chạy test,
//      chép gallery-*-<nền tảng>.png vào tests/baseline.
//   3  tabOrder: Tab đi qua mọi điểm dừng của trang theo thứ tự đọc (hàng trên xuống, trong hàng trái sang), mỗi điểm
//      có viền --c-focus quanh widget (đếm điểm ảnh màu focus so với lúc chưa focus), hết vòng về điểm đầu (0 bẫy
//      focus); Shift+Tab đi ngược đúng thứ tự đó.
//   4  idle: mở xong rồi rảnh: 0 lần vẽ, 0 sự kiện tới đối tượng của bảng mẫu, 0 timer của trang, CPU ~0%. Đo 20 s
//      ở release, 2 s ở bản khác (cả bộ < 60 s); TRION_IDLE_SECONDS=60 đo đủ 60 s bằng tay.
//   DoD-U 3 pseudoNoTruncation: ngôn ngữ giả vi-XL, hai mật độ: findTruncations của cửa sổ và của các điều khiển đã
//      chụp đều rỗng.
#include "app/widgets/gallery/Gallery.h"

#include "app/core/i18n/Translator.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/core/keymap/Keymap.h"
#include "app/core/perf/UiProbe.h"
#include "app/core/UiExecutor.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/generated/Tokens.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/model/BackgroundModel.h"
#include "app/widgets/tooltip/RichTooltip.h"
#include "core/base/sched/Scheduler.h"
#include "core/base/settings/Settings.h"
#include "tests/timing.h"

#include <QAbstractButton>
#include <QAbstractEventDispatcher>
#include <QAbstractItemView>
#include <QApplication>
#include <QButtonGroup>
#include <QElapsedTimer>
#include <QPainter>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <functional>
#include <memory>

#ifdef Q_OS_WIN
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <ctime>
#endif

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;
using namespace trion::theme;
using namespace trion::ui;
namespace i18n = trion::i18n;
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
constexpr int kNoise = 8;

constexpr const char* kRowIds[] = {"button",    "primary",  "danger",   "ghost",  "icon", "toggle", "split",
                                   "segmented", "dropdown", "combobox", "search", "tabs", "slider"};
constexpr const char* kStateIds[] = {"default", "hover", "pressed", "focus", "disabled", "on", "locked", "error"};

// Vòng sự kiện thật (có aboutToBlock như ứng dụng, không qSleep như qWait) đến khi cond() đúng hoặc quá hạn. Timer
// trên stack: không để lại timer đơn (QTimer::singleShot) nổ muộn trong lần đo rảnh sau.
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

void spinFor(std::chrono::milliseconds d)
{
    spinUntil([] { return false; }, d);
}

double cpuSeconds() // thời gian CPU của cả tiến trình (mọi luồng)
{
#ifdef Q_OS_WIN
    FILETIME created, exited, kernel, user;
    GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user);
    const auto s = [](FILETIME f) { return double(quint64(f.dwHighDateTime) << 32 | f.dwLowDateTime) * 1e-7; };
    return s(kernel) + s(user);
#else
    timespec t{};
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t);
    return double(t.tv_sec) + double(t.tv_nsec) * 1e-9;
#endif
}

bool similar(QColor a, QColor b, int tolerance = 24)
{
    return std::abs(a.red() - b.red()) <= tolerance && std::abs(a.green() - b.green()) <= tolerance &&
           std::abs(a.blue() - b.blue()) <= tolerance;
}

// Mọi sự kiện tới mọi đối tượng, trừ timer kết thúc lần đo. bad: sự kiện vẽ, hoặc sự kiện tới đối tượng của bảng mẫu
// (cửa sổ và con, model, RichTooltip). Còn lại là của Qt (QPixmapCache dọn cache 30 s sau lần vẽ cuối rồi tự dừng):
// chỉ báo.
struct EventLog : QObject {
    EventLog(const QObject* ignore, QList<QObject*> ours) : ignore_(ignore), ours_(std::move(ours)) {}
    bool eventFilter(QObject* o, QEvent* e) override
    {
        if (o == ignore_)
            return false;
        const QString what = u"%1:%2"_s.arg(QLatin1StringView(o->metaObject()->className())).arg(int(e->type()));
        seen << what;
        const QEvent::Type t = e->type();
        if (t == QEvent::Paint || t == QEvent::UpdateRequest || t == QEvent::UpdateLater || ours_.contains(o))
            bad << what;
        return false;
    }
    QStringList seen, bad;

private:
    const QObject* ignore_;
    QList<QObject*> ours_;
};

// Điểm dừng Tab của trang theo thứ tự đọc: hàng (khoảng dọc chồng nhau) từ trên xuống, trong hàng từ trái sang.
// Nhóm chọn một (QButtonGroup độc quyền) chỉ dừng ở đoạn đang bật.
QList<QWidget*> readingOrder(QWidget* page)
{
    QList<QWidget*> stops;
    for (QWidget* w : page->findChildren<QWidget*>()) {
        if (!w->isVisible() || !w->isEnabled() || !(w->focusPolicy() & Qt::TabFocus))
            continue;
        if (auto* b = qobject_cast<QAbstractButton*>(w); b && b->group() && b->group()->exclusive() && !b->isChecked())
            continue;
        stops << w;
    }
    const auto box = [page](QWidget* w) { return QRect(w->mapTo(page, QPoint()), w->size()); };
    std::sort(stops.begin(), stops.end(), [&](QWidget* a, QWidget* b) { return box(a).top() < box(b).top(); });
    QList<QWidget*> out;
    for (qsizetype i = 0; i < stops.size();) {
        qsizetype j = i + 1;
        int bottom = box(stops[i]).bottom();
        while (j < stops.size() && box(stops[j]).top() <= bottom)
            bottom = std::max(bottom, box(stops[j++]).bottom());
        QList<QWidget*> row = stops.mid(i, j - i);
        std::sort(row.begin(), row.end(), [&](QWidget* a, QWidget* b) { return box(a).left() < box(b).left(); });
        out += row;
        i = j;
    }
    return out;
}

QString describe(const QWidget* w)
{
    return u"%1 '%2'"_s.arg(QLatin1StringView(w->metaObject()->className()), w->objectName());
}

} // namespace

class GalleryTest : public QObject {
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

    // Model nền của bảng/cây thật đã áp xong.
    bool liveSettled() const
    {
        for (const char* name : {"live/table", "live/tree"}) {
            auto* view = gallery_->findChild<QAbstractItemView*>(QLatin1StringView(name));
            auto* m = view ? qobject_cast<BackgroundModelBase*>(view->model()) : nullptr;
            if (!m || m->busy() || m->rowCount() == 0)
                return false;
        }
        return true;
    }
    // Cửa sổ active, ảnh đã chụp theo chế độ hiện tại, model nền xong.
    bool ready()
    {
        return spinUntil([this] { return gallery_->isActiveWindow() && gallery_->isStaged() && liveSettled(); });
    }
    QWidget* shot(const QString& name) const { return gallery_->findChild<QWidget*>(name); }

private slots:
    void initTestCase()
    {
        QVERIFY(dir_.isValid());
        store_ = std::make_unique<settings::Store>(dir_.filePath(u"user.json"_s));
        settings::Store::setInstance(store_.get());
        executor_ = std::make_unique<UiExecutor>();
        translator_ = std::make_unique<i18n::Translator>();
        QVERIFY(loadFonts()); // chữ Plex nhúng: ảnh không phụ thuộc font của máy
        tm_ = std::make_unique<ThemeManager>(*store_);
        icons_ = std::make_unique<IconProvider>();
        keys_ = std::make_unique<trion::keymap::Keymap>(*store_);
        tips_ = std::make_unique<RichTooltip>();
        tips_->setKeymap(keys_.get());
        QCursor::setPos(-1000, -1000); // con trỏ ngoài cửa sổ: không ô nào "đang rê"
        gallery_ = std::make_unique<GalleryWindow>();
        gallery_->resize(1366, 768); // DoD-U 1: màn hình 1366×768
        gallery_->show();
        QVERIFY(QTest::qWaitForWindowActive(gallery_.get()));
        QVERIFY(ready());
    }

    // Mỗi test bắt đầu ở Tối, gọn, tiếng Việt, không focus bàn phím.
    void init()
    {
        tm_->setTheme(Theme::Dark);
        tm_->setDensity(Density::Compact);
        tm_->setLite(false);
        gallery_->setAttribute(Qt::WA_KeyboardFocusChange, false);
        if (QWidget* f = QApplication::focusWidget())
            f->clearFocus();
        gallery_->activateWindow();
        QVERIFY(ready());
    }

    void cleanupTestCase()
    {
        gallery_.reset();
        tips_.reset();
        keys_.reset();
        icons_.reset();
        tm_.reset();
        translator_.reset();
        settings::Store::setInstance(nullptr);
        store_.reset();
        trion::sched::waitIdle();
        QCoreApplication::processEvents();
        executor_.reset();
    }

    // Tiêu chí 1.
    void contents()
    {
        // Ô "—" (accessibleDescription khác rỗng): nút chính, nguy hiểm, ô chọn, ô tìm, tab, thanh trượt không có
        // trạng thái bật (bảng ở docs/ui/thanh-phan.md).
        const QStringList none = {u"primary/on"_s, u"danger/on"_s, u"dropdown/on"_s, u"combobox/on"_s,
                                  u"search/on"_s,  u"tabs/on"_s,   u"slider/on"_s};
        int images = 0;
        for (const char* row : kRowIds)
            for (const char* state : kStateIds) {
                const QString name = u"%1/%2"_s.arg(QLatin1StringView(row), QLatin1StringView(state));
                QWidget* s = shot(name);
                QVERIFY2(s, qPrintable(name));
                QCOMPARE(s->accessibleDescription().isEmpty(), !none.contains(name));
                if (s->accessibleDescription().isEmpty()) {
                    QVERIFY2(s->width() >= 16 && s->height() >= 16, qPrintable(name));
                    ++images;
                }
            }
        QCOMPARE(images, 13 * 8 - 7);
        for (const QString& name : {u"menu"_s, u"tip/1"_s, u"tip/2"_s, u"table/0"_s, u"table/1"_s, u"table/2"_s,
                                    u"tree/0"_s, u"tree/1"_s, u"tree/2"_s}) {
            QWidget* s = shot(name);
            QVERIFY2(s && s->accessibleDescription().isEmpty() && s->height() >= 32, qPrintable(name));
        }
        QVERIFY(shot(u"tip/2"_s)->height() > shot(u"tip/1"_s)->height()); // mức 2 có thêm mô tả, dòng F1
        for (const char* row : kRowIds)
            QVERIFY2(shot(u"live/"_s + QLatin1StringView(row)), row);
        for (const QString& name : {u"live/small"_s, u"live/large"_s, u"live/table"_s, u"live/tree"_s})
            QVERIFY2(shot(name), qPrintable(name));
        // Thời gian chụp lại cả trang (đổi chủ đề, mật độ): chỉ báo; DoD-L đổi trạng thái giao diện ≤ 100 ms.
        QElapsedTimer t;
        t.start();
        gallery_->stage();
        qInfo("stage(): %lld ms", qint64(t.elapsed()));
        QVERIFY(gallery_->isStaged());
    }

    void switches()
    {
        const auto click = [this](const QString& name) {
            auto* b = gallery_->findChild<QAbstractButton*>(name);
            QVERIFY2(b, qPrintable(name));
            QTest::mouseClick(b, Qt::LeftButton);
        };
        click(u"theme/2"_s);
        QCOMPARE(tm_->theme(), Theme::Light);
        click(u"theme/1"_s);
        QCOMPARE(tm_->theme(), Theme::Dark);
        click(u"density/1"_s);
        QCOMPARE(tm_->density(), Density::Comfortable);
        click(u"lite"_s);
        QVERIFY(tm_->lite());
        click(u"lite"_s);
        QVERIFY(!tm_->lite());
        QVERIFY(ready());
        QVERIFY(!gallery_->findChild<QAbstractButton*>(u"theme/2"_s)->isChecked());
        tm_->setTheme(Theme::Light); // đổi từ nơi khác: thanh chọn theo
        QVERIFY(gallery_->findChild<QAbstractButton*>(u"theme/2"_s)->isChecked());

        QWidget* before = gallery_->page();
        click(u"language/1"_s);
        QCOMPARE(translator_->language(), u"en"_s);
        QVERIFY(spinUntil([&] { return gallery_->page() != before; })); // dựng lại trang với chữ mới
        QVERIFY(ready());
        QVERIFY(gallery_->findChild<QAbstractButton*>(u"language/1"_s)->isChecked());
        QVERIFY(gallery_->findChild<QAbstractButton*>(u"language/1"_s)->hasFocus()); // focus giữ qua lần dựng lại
        before = gallery_->page();
        QVERIFY(translator_->setLanguage(u"vi"_s));
        // LanguageChange là sự kiện đăng hàng đợi: chờ trang dựng lại bằng tiếng Việt, không thì ảnh baseline kế tiếp
        // chụp chữ English (chỉ lộ ra khi .ts đã có bản English).
        QVERIFY(spinUntil([&] { return gallery_->page() != before; }));
        QVERIFY(ready());
    }

    // Tiêu chí 2 (DoD-U 1).
    void baseline_data()
    {
        QTest::addColumn<bool>("light");
        QTest::addColumn<bool>("comfortable");
        QTest::addColumn<qreal>("dpr");
        for (bool light : {false, true})
            for (bool comfortable : {false, true})
                for (qreal dpr : {1.0, 1.5, 2.0})
                    QTest::addRow("%s-%s-%d", light ? "light" : "dark", comfortable ? "comfortable" : "compact",
                                  qRound(dpr * 100))
                        << light << comfortable << dpr;
    }

    void baseline()
    {
        QFETCH(bool, light);
        QFETCH(bool, comfortable);
        QFETCH(qreal, dpr);
        tm_->setTheme(light ? Theme::Light : Theme::Dark);
        tm_->setDensity(comfortable ? Density::Comfortable : Density::Compact);
        QVERIFY(ready());
        gallery_->stage(dpr);
        QVERIFY(gallery_->isStaged());
        QWidget* page = gallery_->page();
        QImage got(page->size() * dpr, QImage::Format_ARGB32_Premultiplied);
        got.setDevicePixelRatio(dpr);
        got.fill(tm_->color(tok::c_base));
        {
            QPainter p(&got);
            page->render(&p, {}, {}, QWidget::DrawWindowBackground | QWidget::DrawChildren);
        }
        const QString name = u"gallery-%1-%2-%3-%4.png"_s.arg(light ? u"light"_s : u"dark"_s)
                                 .arg(comfortable ? u"comfortable"_s : u"compact"_s)
                                 .arg(qRound(dpr * 100))
                                 .arg(QLatin1StringView(kPlatform));
        if (const QString out = qEnvironmentVariable("TRION_UPDATE_BASELINE"); !out.isEmpty()) {
            QVERIFY(got.save(out + u'/' + name, "PNG", 0)); // nén cao nhất
            QSKIP("đã ghi baseline");
        }
        const QImage want = QImage(u":/baseline/"_s + name).convertToFormat(QImage::Format_ARGB32_Premultiplied);
        QVERIFY2(!want.isNull(), qPrintable(u"thiếu baseline "_s + name));
        QCOMPARE(got.size(), want.size());
        qsizetype off = 0;
        int maxDiff = 0;
        for (int y = 0; y < got.height(); ++y) {
            const auto* a = reinterpret_cast<const QRgb*>(got.constScanLine(y));
            const auto* b = reinterpret_cast<const QRgb*>(want.constScanLine(y));
            if (std::memcmp(a, b, std::size_t(got.width()) * sizeof(QRgb)) == 0)
                continue; // dòng trùng hẳn: nhanh ở bản sanitizer
            for (int x = 0; x < got.width(); ++x) {
                const int d = std::max({std::abs(qRed(a[x]) - qRed(b[x])), std::abs(qGreen(a[x]) - qGreen(b[x])),
                                        std::abs(qBlue(a[x]) - qBlue(b[x])), std::abs(qAlpha(a[x]) - qAlpha(b[x]))});
                maxDiff = std::max(maxDiff, d);
                off += d > kNoise;
            }
        }
        qInfo("%s: %lld điểm ảnh lệch > %d/255, lệch lớn nhất %d/255", qPrintable(name), qint64(off), kNoise, maxDiff);
        if (off > got.width() * got.height() / 1000)
            got.save(QDir::temp().filePath(u"got-"_s + name)); // để so bằng mắt
        QVERIFY(off <= got.width() * got.height() / 1000);
    }

    // Tiêu chí 3.
    void tabOrder()
    {
        QWidget* page = gallery_->page();
        const QList<QWidget*> stops = readingOrder(page);
        QStringList names;
        for (QWidget* w : stops)
            names << describe(w);
        QVERIFY2(stops.size() == 21, qPrintable(names.join(u", "_s))); // thanh chọn 4, Dùng thử 15, bảng, cây
        const QColor focus = tm_->color(tok::c_focus);
        const auto ring = [&](QWidget* w) { // số điểm ảnh màu focus trong vùng quanh widget
            const QRect r = QRect(w->mapTo(page, QPoint()), w->size()).adjusted(-3, -3, 3, 3);
            const QImage img = page->grab(r).toImage();
            int n = 0;
            for (int y = 0; y < img.height(); ++y)
                for (int x = 0; x < img.width(); ++x)
                    n += similar(img.pixelColor(x, y), focus);
            return n;
        };
        QList<int> unfocused;
        for (QWidget* w : stops)
            unfocused << ring(w);

        stops.first()->setFocus(Qt::TabFocusReason);
        gallery_->setAttribute(Qt::WA_KeyboardFocusChange);
        QStringList order, faint;
        for (qsizetype i = 0; i <= stops.size(); ++i) {
            QWidget* f = QApplication::focusWidget();
            QVERIFY(f);
            order << describe(f);
            const qsizetype at = stops.indexOf(f);
            const int need = 2 * std::min(f->width(), f->height()); // viền 2 px: ≥ 4·(w+h) điểm
            if (at < 0 || ring(f) - unfocused[at] < need)
                faint << u"%1: %2 điểm"_s.arg(describe(f)).arg(at < 0 ? -1 : ring(f) - unfocused[at]);
            if (i < stops.size())
                QTest::keyClick(f, Qt::Key_Tab);
        }
        QStringList want;
        for (QWidget* w : stops)
            want << describe(w);
        want << describe(stops.first()); // hết vòng về điểm đầu: không bẫy
        QVERIFY2(order == want, qPrintable(u"Tab: "_s + order.join(u", "_s) + u"\nđọc: "_s + want.join(u", "_s)));
        QVERIFY2(faint.isEmpty(), qPrintable(faint.join(u"; "_s)));

        QStringList back;
        for (qsizetype i = 0; i <= stops.size(); ++i) {
            back << describe(QApplication::focusWidget());
            if (i < stops.size())
                QTest::keyClick(QApplication::focusWidget(), Qt::Key_Tab, Qt::ShiftModifier);
        }
        std::reverse(want.begin(), want.end());
        QVERIFY2(back == want, qPrintable(u"Shift+Tab: "_s + back.join(u", "_s)));
        qInfo("Tab: %lld điểm dừng", qint64(stops.size()));
    }

    // Tiêu chí 4 (DoD-H 6).
    void idle()
    {
        trion::sched::waitIdle();
        spinFor(300ms); // việc trễ của lần hiện, chụp (kích hoạt lại cửa sổ…) xong
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete); // timer của spinFor tự hủy
        QList<QObject*> objects{gallery_.get()};
        objects += gallery_->findChildren<QObject*>();
        for (QObject* o : objects)
            QVERIFY2(QAbstractEventDispatcher::instance()->timersForObject(o).isEmpty(), o->metaObject()->className());

        const int seconds = qEnvironmentVariableIsSet("TRION_IDLE_SECONDS")
                                ? qEnvironmentVariableIntValue("TRION_IDLE_SECONDS")
                                : (kTimed ? 20 : 2);
        for (const char* name : {"live/table", "live/tree"})
            objects << gallery_->findChild<QAbstractItemView*>(QLatin1StringView(name))->model();
        objects << tips_.get();
        QTimer end;
        end.setSingleShot(true);
        EventLog log(&end, objects);
        probe::enable();
        const probe::Counters before = probe::counters();
        qApp->installEventFilter(&log);
        const double cpu0 = cpuSeconds();
        QElapsedTimer wall;
        wall.start();
        QEventLoop loop;
        connect(&end, &QTimer::timeout, &loop, &QEventLoop::quit);
        end.start(std::chrono::seconds(seconds));
        loop.exec();
        const double cpu = cpuSeconds() - cpu0, elapsed = double(wall.elapsed()) / 1000;
        qApp->removeEventFilter(&log);
        const probe::Counters after = probe::counters();
        probe::disable();
        qInfo("rảnh %.1f s: %lld sự kiện, CPU %.3f s (%.2f%%), UiProbe: %llu sự kiện, %llu lần watchdog thức", elapsed,
              qint64(log.seen.size()), cpu, 100 * cpu / elapsed,
              static_cast<unsigned long long>(after.events - before.events),
              static_cast<unsigned long long>(after.watchdogWakeups - before.watchdogWakeups));
        if (!log.seen.isEmpty())
            qInfo("sự kiện: %s", qPrintable(log.seen.mid(0, 20).join(u", "_s)));
        QVERIFY2(log.bad.isEmpty(), qPrintable(log.bad.mid(0, 20).join(u", "_s)));
        // ~0%: ngưỡng 1% chỉ ở release không sanitizer (luồng nền của ASan/TSan tự thức theo chu kỳ).
        QVERIFY2(!kTimed || cpu <= 0.01 * elapsed, qPrintable(u"CPU %1 s"_s.arg(cpu)));
    }

    // DoD-U 3: vi-XL, hai mật độ.
    void pseudoNoTruncation()
    {
        QWidget* before = gallery_->page();
        QVERIFY(translator_->setLanguage(i18n::kPseudoLanguage.toString()));
        QVERIFY(spinUntil([&] { return gallery_->page() != before; }));
        for (Density d : {Density::Compact, Density::Comfortable}) {
            tm_->setDensity(d);
            QVERIFY(ready());
            QStringList found = gallery_->stageTruncations();
            for (const i18n::Truncation& t : i18n::findTruncations(gallery_.get()))
                found << t.toString();
            QVERIFY2(found.isEmpty(), qPrintable(found.join(u"; "_s)));
        }
        const QString title = gallery_->windowTitle();
        QVERIFY(translator_->setLanguage(u"vi"_s));
        QVERIFY(spinUntil([&] { return gallery_->windowTitle() != title; }));
        QVERIFY(ready());
    }
};

QTEST_MAIN(GalleryTest)
#include "gallery_test.moc"
