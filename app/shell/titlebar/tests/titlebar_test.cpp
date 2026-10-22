// UIA-002: thanh tiêu đề, QAT, ô Tìm lệnh, menu tài khoản (TitleBar.h, CommandSearch.h, Qat.h). Cửa sổ mẫu: thanh
// tiêu đề + ô nhập giả làm vùng vẽ (nơi focus quay về); ActionRegistry với sink ghi chuỗi lệnh; EditionGate với trang
// nâng cấp giả. Mỗi tiêu chí nghiệm thu một nhóm hàm test:
//   1  searchQueries: 50 truy vấn mẫu (tiếng Việt có/không dấu, viết tắt, tên tiếng Anh) → lệnh đúng trong top 3 ở
//      ≥ 48 ca; "đường tròn" → CIRCLE đứng đầu.
//   2  searchLatency: mỗi phím (gõ 50 truy vấn từng ký tự, popup đang mở: xếp hạng, đặt lại danh sách, vẽ popup) ≤ 16 ms
//      p99, thời gian CPU của luồng; dựng chỉ mục ≤ 2 ms. Ngưỡng kiểm ở release (kTimed), số đo luôn in.
//   3  lockedCommand: Viewer: dòng của lệnh Pro có huy hiệu (vẽ ra, trợ năng đọc "Có trong TrionCAD Pro"); Enter mở
//      trang nâng cấp, không gửi lệnh; lệnh có trong Viewer chạy.
//   4  dirtyWithinFrame ("*" trong 1 khung), longNameElided (cắt giữa, tooltip đủ đường dẫn), keyboard (Tab qua mọi
//      nút; Enter, Alt+↓, Esc), popupKeys (↑↓, Esc trả focus, bấm chuột chạy).
//   DoD-U: emptyAreaPassesMouse (hợp đồng kéo cửa sổ với MainWindow), editionBadge, accountMenu, helpMenu, qatMenu,
//   idleNoTimer, pseudoNoTruncation (vi-XL, 1366 px, cả popup và menu), baseline (Tối/Sáng; ghi lại:
//   TRION_UPDATE_BASELINE=<thư mục>).
#include "app/shell/titlebar/CommandSearch.h"
#include "app/shell/titlebar/Qat.h"
#include "app/shell/titlebar/TitleBar.h"

#include "app/core/actions/ActionRegistry.h"
#include "app/core/edition/EditionGate.h"
#include "app/core/i18n/Translator.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/core/keymap/Keymap.h"
#include "app/core/UiExecutor.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/controls/Controls.h"
#include "app/widgets/menu/Menu.h"
#include "app/widgets/tooltip/RichTooltip.h"
#include "core/base/sched/Scheduler.h"
#include "core/base/settings/Settings.h"
#include "tests/timing.h"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QScreen>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>
#include <chrono>
#include <memory>

#if defined(Q_OS_LINUX)
#include <ctime>
#endif

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;
using namespace trion;
using namespace trion::shell;
using edition::Edition;
using trion::test::kTimed;
using Clock = std::chrono::steady_clock;

namespace {

#if defined(Q_OS_WIN)
constexpr auto kPlatform = "windows";
#elif defined(Q_OS_MACOS)
constexpr auto kPlatform = "macos";
#else
constexpr auto kPlatform = "linux";
#endif
constexpr int kNoise = 8;          // baseline: lệch ≤ 8/255 mỗi kênh bỏ qua, điểm lệch hơn được tối đa 0,1% ảnh
constexpr double kKeyBudgetMs = 16; // DoD-L: danh sách tự hoàn thành cập nhật sau mỗi phím (máy tầm trung)
constexpr double kBuildBudgetMs = 2; // DoD-U 4: việc trên luồng UI

// Tiêu chí 1: truy vấn mẫu → lệnh đúng. Tiếng Việt có dấu, không dấu, viết tắt (hoa, thường), tên tiếng Anh (đủ,
// một phần).
struct Case {
    const char* query;
    const char* command;
};
constexpr Case kCases[] = {
    {"đường tròn", "CIRCLE"},       {"đoạn thẳng", "LINE"},     {"vẽ đường thẳng", "LINE"},
    {"hình chữ nhật", "RECTANG"},   {"đa giác", "POLYGON"},     {"di chuyển", "MOVE"},
    {"sao chép", "COPY"},           {"xoay", "ROTATE"},         {"đối xứng", "MIRROR"},
    {"bo góc", "FILLET"},           {"vát góc", "CHAMFER"},     {"kéo dài", "EXTEND"},
    {"xóa", "ERASE"},               {"tô vật liệu", "HATCH"},   {"chữ", "MTEXT"},
    {"kích thước", "DIM"},          {"đo khoảng cách", "DIST"}, {"diện tích", "AREA"},
    {"in", "PLOT"},                 {"lớp", "LAYER"},           {"phá khối", "EXPLODE"},
    {"dọn dẹp", "PURGE"},           {"đám mây", "REVCLOUD"},    {"phóng to", "ZOOM"},
    {"bác sĩ font", "FIXFONT"},     {"duong tron", "CIRCLE"},   {"hinh chu nhat", "RECTANG"},
    {"di chuyen", "MOVE"},          {"bo goc", "FILLET"},       {"do dien tich", "AREA"},
    {"chen block", "INSERT"},       {"song song", "OFFSET"},    {"cat", "TRIM"},
    {"mang", "ARRAY"},              {"ghi chu", "MLEADER"},     {"C", "CIRCLE"},
    {"L", "LINE"},                  {"CO", "COPY"},             {"tr", "TRIM"},
    {"REC", "RECTANG"},             {"PL", "PLINE"},            {"MA", "MATCHPROP"},
    {"x", "EXPLODE"},               {"circle", "CIRCLE"},       {"offset", "OFFSET"},
    {"hatch", "HATCH"},             {"layer", "LAYER"},         {"plot", "PLOT"},
    {"dimlinear", "DIMLINEAR"},     {"circ", "CIRCLE"},
};

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

double ms(Clock::duration d)
{
    return std::chrono::duration<double, std::milli>(d).count();
}

// Thời gian CPU của luồng (không phụ thuộc tải của máy dùng chung); nơi chưa có: thời gian thực.
double nowMs()
{
#if defined(Q_OS_LINUX)
    timespec t{};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
    return double(t.tv_sec) * 1e3 + double(t.tv_nsec) / 1e6;
#else
    return ms(Clock::now().time_since_epoch());
#endif
}

double percentile(std::vector<double> v, double p)
{
    std::sort(v.begin(), v.end());
    return v.empty() ? 0 : v[std::min(v.size() - 1, size_t(std::ceil(p * double(v.size()))) - 1)];
}

Clock::duration frameTime()
{
    const QScreen* s = QGuiApplication::primaryScreen();
    return std::chrono::nanoseconds(qint64(1e9 / (s && s->refreshRate() > 1 ? s->refreshRate() : 60)));
}

// Gõ chữ như bộ gõ đã chốt (cả chữ có dấu): popup đang mở thì phím tới popup (QCompleter chuyển về ô) như thật.
void type(QWidget* field, const QString& text)
{
    for (const QChar ch : text) {
        QWidget* to = QApplication::activePopupWidget() ? QApplication::activePopupWidget() : field;
        const auto key = ch.unicode() < 0x80 ? Qt::Key(ch.toUpper().unicode()) : Qt::Key_unknown;
        QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier, QString(ch));
        QCoreApplication::sendEvent(to, &press);
        QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier, QString(ch));
        QCoreApplication::sendEvent(to, &release);
    }
}

// Phím tới popup đang mở (QCompleter, Menu) nếu có, không thì tới widget đang focus.
void key(Qt::Key k, Qt::KeyboardModifiers m = Qt::NoModifier)
{
    QWidget* to = QApplication::activePopupWidget() ? QApplication::activePopupWidget() : QApplication::focusWidget();
    QVERIFY(to);
    QTest::keyClick(to, k, m);
}

// Cửa sổ mẫu: ghi nhấn/nhấp đúp chuột lên tới nó (MainWindow kéo cửa sổ, phóng to).
class Host final : public QWidget {
public:
    int presses = 0, doubleClicks = 0;

protected:
    void mousePressEvent(QMouseEvent*) override { ++presses; }
    void mouseDoubleClickEvent(QMouseEvent*) override { ++doubleClicks; }
};

// Ghi lần vẽ đầu của một widget.
class Watcher final : public QObject {
public:
    explicit Watcher(QWidget* w) { w->installEventFilter(this); }
    Clock::time_point firstPaintAt{};
    int paints = 0;

protected:
    bool eventFilter(QObject*, QEvent* e) override
    {
        if (e->type() == QEvent::Paint && !paints++)
            firstPaintAt = Clock::now();
        return false;
    }
};

QAction* actionWithText(QMenu* m, const QString& text)
{
    for (QAction* a : m->actions())
        if (a->text() == text)
            return a;
    return nullptr;
}

QString truncations(QWidget* root)
{
    QStringList out;
    for (const i18n::Truncation& t : i18n::findTruncations(root))
        out << t.toString();
    return out.join(u"; "_s);
}

// Số điểm ảnh có đúng màu c (huy hiệu khóa trong dòng kết quả).
int countColor(const QImage& img, const QColor& c)
{
    int n = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            n += img.pixelColor(x, y).rgb() == c.rgb();
    return n;
}

} // namespace

class TitleBarTest : public QObject {
    Q_OBJECT
    QTemporaryDir dir_;
    std::unique_ptr<settings::Store> store_;
    std::unique_ptr<ui::UiExecutor> executor_;
    std::unique_ptr<theme::ThemeManager> tm_;
    std::unique_ptr<theme::IconProvider> icons_;
    std::unique_ptr<i18n::Translator> translator_;
    std::unique_ptr<keymap::Keymap> keymap_;
    std::unique_ptr<ui::RichTooltip> tips_;
    std::unique_ptr<actions::Sysvars> sysvars_;
    std::unique_ptr<actions::ActionRegistry> reg_;
    std::unique_ptr<edition::EditionGate> gate_;
    std::unique_ptr<Host> host_;
    TitleBar* bar_ = nullptr;
    QLineEdit* canvas_ = nullptr; // vùng vẽ giả
    QStringList sent_;            // chuỗi lệnh tới INT-002
    QStringList upgrades_;        // tính năng đã yêu cầu nâng cấp (trang hoặc chú thích)

    CommandSearch* search() const { return bar_->search(); }
    QAbstractItemView* popup() const { return search()->popup(); }
    template <class T>
    T* child(const char* name) const
    {
        return bar_->findChild<T*>(QLatin1StringView(name));
    }

    void focus(QWidget* w)
    {
        host_->activateWindow();
        w->setFocus(Qt::TabFocusReason);
        QVERIFY(spinUntil([&] { return QApplication::focusWidget() == w; }));
    }

    void resizeHost(int width)
    {
        host_->resize(width, 600);
        QVERIFY(spinUntil([&] { return bar_->width() == width; }));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    }

    void closePopups()
    {
        for (int i = 0; i < 5 && QApplication::activePopupWidget(); ++i)
            QApplication::activePopupWidget()->close();
    }

private slots:
    void initTestCase()
    {
        QVERIFY(dir_.isValid());
        store_ = std::make_unique<settings::Store>(dir_.filePath(u"user.json"_s));
        settings::Store::setInstance(store_.get());
        executor_ = std::make_unique<ui::UiExecutor>();
        QVERIFY(theme::loadFonts()); // chữ Plex nhúng: ảnh không phụ thuộc font của máy
        tm_ = std::make_unique<theme::ThemeManager>(*store_);
        icons_ = std::make_unique<theme::IconProvider>();
        translator_ = std::make_unique<i18n::Translator>();
        keymap_ = std::make_unique<keymap::Keymap>(*store_, keymap::Platform::Windows);
        tips_ = std::make_unique<ui::RichTooltip>();
        tips_->setKeymap(keymap_.get());
        sysvars_ = std::make_unique<actions::Sysvars>();
        reg_ = std::make_unique<actions::ActionRegistry>(*keymap_, *sysvars_);
        reg_->setSink([this](const QString& m) { sent_ << m; });
        gate_ = std::make_unique<edition::EditionGate>();
        gate_->bind(*reg_);
        gate_->upgrade().setHandler([this](const edition::UpgradeRequest& r) { upgrades_ << r.feature; });
        connect(&gate_->upgrade(), &edition::UpgradeRouter::noticeRequested, this,
                [this](const edition::UpgradeRequest& r) { upgrades_ << r.feature; });

        host_ = std::make_unique<Host>();
        auto* col = new QVBoxLayout(host_.get());
        col->setContentsMargins({});
        col->setSpacing(0);
        bar_ = new TitleBar(*reg_, *gate_);
        canvas_ = new QLineEdit;
        col->addWidget(bar_);
        col->addWidget(canvas_);
        col->addStretch(1);
        host_->resize(1366, 600);
        host_->show();
        QVERIFY(QTest::qWaitForWindowExposed(host_.get()));
    }

    void init()
    {
        tm_->setTheme(theme::Theme::Dark);
        tm_->setDensity(theme::Density::Compact);
        gate_->setEdition(Edition::Pro);
        bar_->setDocument({.name = u"Banve-01.dwg"_s, .path = u"D:/Du an/Banve-01.dwg"_s});
        bar_->setAccount({});
        bar_->setTrialDays(0);
        bar_->setOfflineDays(-1);
        bar_->setContentsMargins({});
        resizeHost(1366);
        sent_.clear();
        upgrades_.clear();
        focus(canvas_);
    }

    void cleanup()
    {
        closePopups();
        search()->clear();
        bar_->setUrlOpener({});
        if (translator_->language() != u"vi"_s)
            QVERIFY(translator_->setLanguage(u"vi"_s));
        QCoreApplication::processEvents();
    }

    void cleanupTestCase()
    {
        host_.reset();
        gate_.reset();
        reg_.reset();
        sysvars_.reset();
        tips_.reset();
        keymap_.reset();
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
    void searchQueries()
    {
        QCOMPARE(int(std::size(kCases)), 50);
        int hits = 0;
        QStringList misses;
        for (const Case& c : kCases) {
            const QStringList top = search()->search(QString::fromUtf8(c.query)).mid(0, 3);
            if (top.contains(QLatin1StringView(c.command)))
                ++hits;
            else
                misses << u"%1 → %2 (có: %3)"_s.arg(QString::fromUtf8(c.query), QLatin1StringView(c.command),
                                                    top.join(u' '));
        }
        qInfo("Tìm lệnh: %d/50 truy vấn có lệnh đúng trong top 3%s%s", hits, misses.isEmpty() ? "" : "; trượt: ",
              qPrintable(misses.join(u"; "_s)));
        QVERIFY2(hits >= 48, qPrintable(misses.join(u"; "_s)));
        QCOMPARE(search()->search(u"đường tròn"_s).value(0), u"CIRCLE"_s);
        QCOMPARE(search()->search(u"ĐƯỜNG TRÒN"_s).value(0), u"CIRCLE"_s);
        QVERIFY(search()->search(u"zzzz"_s).isEmpty());
        QVERIFY(search()->search(u"  "_s).isEmpty());
        QCOMPARE(search()->search(u"a"_s).size(), CommandSearch::kMaxResults);
    }

    // Tiêu chí 2.
    void searchLatency()
    {
        // Dựng chỉ mục: CommandSearch mới tìm lần đầu (dựng + một lần xếp hạng); vòng tốt nhất của 10.
        double build = 1e9;
        for (int i = 0; i < 10; ++i) {
            CommandSearch fresh(*reg_);
            const double t0 = nowMs();
            QCOMPARE(fresh.search(u"c"_s).value(0), u"CIRCLE"_s);
            build = std::min(build, nowMs() - t0);
        }

        // Mỗi phím: tới ô (hoặc popup đang mở), xếp hạng, đặt lại danh sách, đặt popup, vẽ popup.
        std::vector<double> keys;
        for (const Case& c : kCases) {
            focus(search());
            for (const QChar ch : QString::fromUtf8(c.query)) {
                const double t0 = nowMs();
                type(search(), QString(ch));
                search()->popup()->viewport()->repaint();
                keys.push_back(nowMs() - t0);
            }
            QVERIFY(popup()->isVisible());
            key(Qt::Key_Escape);
        }
        const double p50 = percentile(keys, 0.5), p99 = percentile(keys, 0.99);
        qInfo("Tìm lệnh: dựng chỉ mục %.3f ms (ngưỡng %.0f ms); mỗi phím (%zu phím, CPU luồng) p50 %.3f / p99 %.3f / "
              "max %.3f ms (ngưỡng p99 %.0f ms)",
              build, kBuildBudgetMs, keys.size(), p50, p99, *std::max_element(keys.begin(), keys.end()),
              kKeyBudgetMs);
        QVERIFY2(!kTimed || build <= kBuildBudgetMs, qPrintable(QString::number(build)));
        QVERIFY2(!kTimed || p99 <= kKeyBudgetMs, qPrintable(QString::number(p99)));
    }

    // Tiêu chí 3.
    void lockedCommand()
    {
        const auto row = [&] {
            const QAbstractItemView* v = search()->popup();
            return v->viewport()->grab(v->visualRect(v->model()->index(0, 0))).toImage();
        };
        const QColor badge = tm_->color(tok::c_ed_pro); // .ed.xs "Pro"

        focus(search());
        type(search(), u"line"_s); // Pro: không khóa
        QCOMPARE(popup()->model()->index(0, 0).data(Qt::UserRole).toString(), u"LINE"_s);
        QVERIFY(!popup()->model()->index(0, 0).data(Qt::AccessibleTextRole).toString().contains(u"TrionCAD"_s));
        const int unlockedBadge = countColor(row(), badge);
        key(Qt::Key_Escape);

        gate_->setEdition(Edition::Viewer);
        focus(search());
        type(search(), u"line"_s);
        const QModelIndex first = popup()->model()->index(0, 0);
        QCOMPARE(first.data(Qt::UserRole).toString(), u"LINE"_s);
        QVERIFY(first.data(Qt::AccessibleTextRole)
                    .toString()
                    .contains(edition::EditionGate::availableIn(ui::Edition::Pro)));
        const int lockedBadge = countColor(row(), badge);
        qInfo("Huy hiệu khóa trong dòng LINE: %d điểm ảnh màu --c-ed-pro (Pro: %d)", lockedBadge, unlockedBadge);
        QVERIFY(lockedBadge >= 40 && unlockedBadge == 0);

        key(Qt::Key_Return); // khóa: trang nâng cấp, không chạy
        QCOMPARE(upgrades_, QStringList{u"LINE"_s});
        QCOMPARE(sent_, QStringList());
        QVERIFY(!popup()->isVisible());
        QCOMPARE(QApplication::focusWidget(), canvas_);

        focus(search());
        type(search(), u"zoom"_s); // có trong Viewer: chạy
        key(Qt::Key_Return);
        QCOMPARE(sent_, QStringList{u"'_ZOOM"_s});
        QCOMPARE(upgrades_.size(), 1);
    }

    // Tiêu chí 4: "*" trong 1 khung sau thay đổi đầu tiên (vòng tốt nhất của 3, kiểm ở release).
    void dirtyWithinFrame()
    {
        auto* title = child<QLabel>("docTitle");
        QVERIFY(title);
        double best = 1e9;
        for (int round = 0; round < 3; ++round) {
            bar_->setDocument({.name = u"Banve-01.dwg"_s, .path = u"D:/Banve-01.dwg"_s});
            QVERIFY(spinUntil([&] { return !title->text().contains(u'*'); }));
            QCoreApplication::processEvents();
            Watcher w(title);
            const auto t0 = Clock::now();
            bar_->setDocument({.name = u"Banve-01.dwg"_s, .path = u"D:/Banve-01.dwg"_s, .dirty = true});
            QVERIFY(title->text().startsWith(u"Banve-01.dwg*"_s));
            QVERIFY(spinUntil([&] { return w.paints > 0; }));
            best = std::min(best, ms(w.firstPaintAt - t0));
        }
        qInfo("Dấu * vẽ sau %.2f ms (một khung %.1f ms)", best, ms(frameTime()));
        QVERIFY2(!kTimed || best <= ms(frameTime()), qPrintable(QString::number(best)));
        QVERIFY(bar_->shownName() == u"Banve-01.dwg"_s);
    }

    void longNameElided()
    {
        const QString name =
            u"Du-an-nha-o-gia-dinh-2-tang-An-Phu-Quan-2-mat-bang-tang-1-phuong-an-3-chinh-sua-lan-5-ban-in-A1.dwg"_s;
        const QString path = u"D:/Du an/2026/An Phu/"_s + name;
        resizeHost(1024);
        bar_->setDocument({.name = name, .path = path, .dirty = true, .recovered = true});
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        const QString shown = bar_->shownName();
        QVERIFY2(shown.contains(u'…') && shown.startsWith(name.left(3)) && shown.endsWith(u".dwg"_s),
                 qPrintable(shown));
        auto* title = child<QLabel>("docTitle");
        QVERIFY(title->text().contains(name + u'*')); // trình đọc màn hình: đủ tên
        QCOMPARE(ui::RichTooltip::info(title).name, path);
        QVERIFY(title->geometry().right() < search()->geometry().left()); // không đè ô Tìm lệnh

        QCOMPARE(search()->width(), 160); // thiếu chỗ: ô Tìm lệnh co tới tối thiểu trước khi rút gọn tên
        resizeHost(3000); // đủ chỗ: hiện đủ tên, ô rộng 300 px

        QCOMPARE(bar_->shownName(), name);
        QCOMPARE(search()->width(), 300);
    }

    void keyboard()
    {
        QSignalSpy app(bar_, &TitleBar::appMenuRequested);
        auto* appButton = child<QPushButton>("appMenuButton");
        auto* open = bar_->qat()->button(u"OPEN"_s);
        auto* plot = qobject_cast<ui::SplitButton*>(bar_->qat()->button(u"PLOT"_s));
        auto* more = bar_->qat()->findChild<ui::Button*>(u"qatMore"_s);
        auto* user = child<ui::Button>("accountButton");
        auto* help = child<ui::Button>("helpButton");
        QVERIFY(appButton && open && plot && more && user && help);

        // Tab từ vùng vẽ qua mọi nút theo thứ tự trên thanh rồi về vùng vẽ.
        const QList<QWidget*> want{appButton, open, plot, more, search(), user, help, canvas_};
        QList<QWidget*> got;
        for (int i = 0; i < want.size(); ++i) {
            key(Qt::Key_Tab);
            got << QApplication::focusWidget();
        }
        QCOMPARE(got, want);

        focus(appButton);
        key(Qt::Key_Return);
        QCOMPARE(app.size(), 1);
        QCOMPARE(app[0][0].toPoint(), appButton->mapToGlobal(QPoint(0, appButton->height())));

        focus(open);
        key(Qt::Key_Return);
        focus(plot);
        key(Qt::Key_Enter);
        QCOMPARE(sent_, (QStringList{u"^C^C_OPEN"_s, u"^C^C_PLOT"_s}));

        // Nút mở menu: Enter (Alt+↓ với nút tách) mở, Esc đóng, focus về nút.
        const auto menuOf = [&](QWidget* button, Qt::Key k, Qt::KeyboardModifiers m, QMenu* want) {
            focus(button);
            key(k, m);
            QVERIFY(spinUntil([&] { return QApplication::activePopupWidget() == want; }));
            key(Qt::Key_Escape);
            QVERIFY(spinUntil([&] { return !want->isVisible(); }));
            QCOMPARE(QApplication::focusWidget(), button);
        };
        menuOf(plot, Qt::Key_Down, Qt::AltModifier, plot->dropMenu());
        menuOf(more, Qt::Key_Return, Qt::NoModifier, bar_->qat()->menu());
        menuOf(user, Qt::Key_Return, Qt::NoModifier, child<QMenu>("accountMenu"));
        menuOf(help, Qt::Key_Space, Qt::NoModifier, child<QMenu>("helpMenu"));

        // Ô Tìm lệnh: Enter chạy kết quả đầu, Esc trả focus về chỗ cũ.
        focus(user);
        focus(search());
        type(search(), u"zoom"_s);
        key(Qt::Key_Escape);
        QCOMPARE(QApplication::focusWidget(), user);
        QVERIFY(search()->text().isEmpty());
        focus(search());
        type(search(), u"duong tron"_s);
        closePopups(); // bấm ra ngoài đóng popup; ô còn chữ: Enter vẫn chạy kết quả đầu
        focus(search());
        key(Qt::Key_Return);
        QCOMPARE(sent_.last(), u"^C^C_CIRCLE"_s);
    }

    void popupKeys()
    {
        focus(search());
        type(search(), u"cat"_s);
        QAbstractItemView* v = search()->popup();
        QVERIFY(v->isVisible());
        QCOMPARE(v->currentIndex().row(), 0);
        QCOMPARE(v->currentIndex().data(Qt::UserRole).toString(), u"TRIM"_s);
        key(Qt::Key_Down);
        key(Qt::Key_Down);
        QCOMPARE(v->currentIndex().row(), 2);
        key(Qt::Key_Up);
        QCOMPARE(v->currentIndex().row(), 1);
        key(Qt::Key_Up);
        key(Qt::Key_Up); // không vòng xuống cuối
        QCOMPARE(v->currentIndex().row(), 0);
        key(Qt::Key_Down);
        const QString second = v->currentIndex().data(Qt::UserRole).toString();
        key(Qt::Key_Return);
        QCOMPARE(sent_, QStringList{u"^C^C_"_s + second});
        QCOMPARE(QApplication::focusWidget(), canvas_);

        // Esc: đóng, xóa chữ, focus về vùng vẽ.
        focus(search());
        type(search(), u"cat"_s);
        key(Qt::Key_Escape);
        QVERIFY(!v->isVisible());
        QVERIFY(search()->text().isEmpty());
        QCOMPARE(QApplication::focusWidget(), canvas_);

        // Bấm chuột vào dòng thứ 3 chạy lệnh đó; không tìm thấy: một dòng gợi ý, Enter không chạy gì.
        focus(search());
        type(search(), u"cat"_s);
        const QModelIndex third = v->model()->index(2, 0);
        const QString name = third.data(Qt::UserRole).toString();
        QTest::mouseClick(v->viewport(), Qt::LeftButton, {}, v->visualRect(third).center());
        QCOMPARE(sent_.last(), u"^C^C_"_s + name);
        focus(search());
        type(search(), u"qqqq"_s);
        QVERIFY(v->isVisible());
        QCOMPARE(v->model()->rowCount(), 1);
        QVERIFY(v->model()->index(0, 0).data().toString().contains(u"qqqq"_s));
        key(Qt::Key_Return);
        QCOMPARE(sent_.size(), 2);
    }

    // Hợp đồng với MainWindow (UIA-001): nhấn, nhấp đúp ở vùng trống và chữ tên bản vẽ lên tới cửa sổ.
    void emptyAreaPassesMouse()
    {
        auto* title = child<QLabel>("docTitle");
        const QPoint gap(bar_->qat()->geometry().right() + 4, bar_->height() / 2); // giữa QAT và tên
        QVERIFY(!bar_->childAt(gap));
        for (const QPoint at : {gap, title->geometry().center()}) {
            const int presses = host_->presses, doubles = host_->doubleClicks;
            QTest::mousePress(bar_, Qt::LeftButton, {}, at);
            QTest::mouseRelease(bar_, Qt::LeftButton, {}, at);
            QTest::mouseDClick(bar_, Qt::LeftButton, {}, at);
            QVERIFY(host_->presses > presses);
            QCOMPARE(host_->doubleClicks, doubles + 1);
        }
        const int presses = host_->presses; // nút thì nhận
        QTest::mouseClick(child<QWidget>("accountButton"), Qt::LeftButton);
        QCOMPARE(host_->presses, presses);
    }

    void editionBadge()
    {
        auto* title = child<QLabel>("docTitle");
        auto* badge = child<QLabel>("editionBadge");
        const auto text = [](const char* s) { return QString::fromUtf8(s); };

        QCOMPARE(title->text(), text("Banve-01.dwg — TrionCAD Pro"));
        QCOMPARE(badge->text(), text("Pro"));
        bar_->setDocument({.name = u"A.dwg"_s, .readOnly = true, .recovered = true});
        QCOMPARE(title->text(), text("A.dwg [Chỉ đọc] [Đã phục hồi] — TrionCAD Pro"));
        QCOMPARE(ui::RichTooltip::info(title).name, u"A.dwg"_s); // chưa có đường dẫn: tên

        gate_->setEdition(Edition::Viewer); // Viewer: chỉ xem
        QCOMPARE(title->text(), text("A.dwg [Chỉ xem] [Đã phục hồi] — TrionCAD Viewer"));
        QCOMPARE(badge->text(), text("Viewer"));
        bar_->setOfflineDays(5); // Viewer không cần xác minh giấy phép
        QCOMPARE(badge->text(), text("Viewer"));

        gate_->setEdition(Edition::Trial);
        bar_->setTrialDays(12);
        bar_->setOfflineDays(-1);
        QCOMPARE(badge->text(), text("Dùng thử: còn 12 ngày"));
        bar_->setOfflineDays(5);
        QCOMPARE(badge->text(), text("Dùng thử: còn 12 ngày · còn 5 ngày offline"));
        gate_->setEdition(Edition::Education);
        QCOMPARE(badge->text(), text("Giáo dục · còn 5 ngày offline"));
        QVERIFY(title->text().endsWith(text("TrionCAD Giáo dục")));

        bar_->setDocument({}); // tab Start: chỉ tên phiên bản
        QCOMPARE(title->text(), text("TrionCAD Giáo dục"));
        QCOMPARE(bar_->shownName(), QString());
    }

    void accountMenu()
    {
        QStringList urls, screens;
        bar_->setUrlOpener([&](const QUrl& u) {
            urls << u.toString();
            return true;
        });
        QSignalSpy out(bar_, &TitleBar::signOutRequested);
        connect(bar_, &TitleBar::screenRequested, this, [&](const QString& id) { screens << id; });
        auto* menu = child<QMenu>("accountMenu");
        const auto text = [](const char* s) { return QString::fromUtf8(s); };

        gate_->setEdition(Edition::Trial);
        bar_->setTrialDays(12);
        bar_->setAccount({.name = text("Nguyễn Minh An"), .email = u"an.nguyen@example.com"_s, .signedIn = true});
        QStringList items;
        for (QAction* a : menu->actions())
            items << a->text();
        QCOMPARE(items, (QStringList{text("Nguyễn Minh An · an.nguyen@example.com"), text("TrionCAD Pro · còn 12 ngày"),
                                     text("Trạng thái giấy phép"), text("Kích hoạt, nhập key…"),
                                     text("Quản lý tài khoản"), QString(), text("Đăng xuất")}));
        actionWithText(menu, text("TrionCAD Pro · còn 12 ngày"))->trigger();
        actionWithText(menu, text("Trạng thái giấy phép"))->trigger();
        actionWithText(menu, text("Kích hoạt, nhập key…"))->trigger();
        actionWithText(menu, text("Quản lý tài khoản"))->trigger();
        actionWithText(menu, text("Đăng xuất"))->trigger();
        QCOMPARE(screens, (QStringList{u"license"_s, u"license"_s, u"activate"_s}));
        QCOMPARE(urls, QStringList{u"https://trioncad.vn/tai-khoan"_s});
        QCOMPARE(out.size(), 1);

        bar_->setAccount({}); // chưa đăng nhập: không tên, không quản lý, không đăng xuất
        gate_->setEdition(Edition::Viewer);
        items.clear();
        for (QAction* a : menu->actions())
            items << a->text();
        QCOMPARE(items, (QStringList{text("TrionCAD Viewer"), text("Trạng thái giấy phép"), text("Đăng nhập, kích hoạt…")}));
        disconnect(bar_, &TitleBar::screenRequested, this, nullptr);
    }

    void helpMenu()
    {
        QStringList screens;
        connect(bar_, &TitleBar::screenRequested, this, [&](const QString& id) { screens << id; });
        auto* menu = child<ui::Menu>("helpMenu");
        const QList<QAction*> all = menu->actions();
        QCOMPARE(all.size(), 10);
        QCOMPARE(all[0], reg_->action(u"HELP"_s)); // QAction của ActionRegistry: F1, chạy lệnh HELP
        QCOMPARE(ui::Menu::keyText(all[0]), u"F1"_s);
        QCOMPARE(all[5], reg_->action(u"TCREPORT"_s));
        QCOMPARE(all[9], reg_->action(u"ABOUT"_s));
        QVERIFY(all[3]->isSeparator() && all[7]->isSeparator());
        for (int i : {1, 2, 4, 6, 8})
            all[i]->trigger();
        QCOMPARE(screens, (QStringList{u"migrate"_s, u"keys"_s, u"support"_s, u"feedback"_s, u"update"_s}));
        all[0]->trigger();
        QCOMPARE(sent_, QStringList{u"'_HELP"_s});
        disconnect(bar_, &TitleBar::screenRequested, this, nullptr);
    }

    void qatMenu()
    {
        Qat* qat = bar_->qat();
        ui::Menu* menu = qat->menu();
        const auto toggle = [&](const char* label) { return actionWithText(menu, QString::fromUtf8(label)); };
        QVERIFY(qat->button(u"OPEN"_s)->isVisible() && qat->button(u"PLOT"_s)->isVisible());
        QVERIFY(!qat->button(u"PREVIEW"_s)->isVisible());
        QVERIFY(toggle("Mở")->isChecked() && toggle("In")->isChecked() && !toggle("Xem trước bản in")->isChecked());

        // Nút tách In: In, In nhiều layout, In theo khung tên, Xem trước (QAction của ActionRegistry).
        auto* plot = qobject_cast<ui::SplitButton*>(qat->button(u"PLOT"_s));
        QCOMPARE(plot->dropMenu()->actions(),
                 (QList<QAction*>{reg_->action(u"PLOT"_s), reg_->action(u"PUBLISH"_s), reg_->action(u"TCBATCHPLOT"_s),
                                  reg_->action(u"PREVIEW"_s)}));

        toggle("Xem trước bản in")->trigger(); // bật: nút hiện
        QVERIFY(qat->button(u"PREVIEW"_s)->isVisible());
        toggle("Mở")->trigger(); // tắt: nút ẩn
        QVERIFY(!qat->button(u"OPEN"_s)->isVisible());
        toggle("Mở")->trigger();
        toggle("Xem trước bản in")->trigger();

        // Viewer: Khớp thuộc tính (Pro), In hàng loạt (Viewer Pro) có huy hiệu; bật thì mở trang nâng cấp.
        gate_->setEdition(Edition::Viewer);
        QCOMPARE(ui::RichTooltip::info(toggle("Khớp thuộc tính")).need, ui::Edition::Pro);
        QCOMPARE(ui::RichTooltip::info(toggle("In hàng loạt")).need, ui::Edition::ViewerPro);
        QCOMPARE(ui::RichTooltip::info(toggle("Bác sĩ Font")).need, ui::Edition::None);
        toggle("Khớp thuộc tính")->trigger();
        QCOMPARE(upgrades_, QStringList{u"MATCHPROP"_s});
        QVERIFY(!toggle("Khớp thuộc tính")->isChecked());
        QVERIFY(!qat->button(u"MATCHPROP"_s)->isVisible());
        QVERIFY(sent_.isEmpty());

        // Pro: mở khóa, thêm được; nút có tooltip của lệnh, tên cho trình đọc màn hình.
        gate_->setEdition(Edition::Pro);
        QCOMPARE(ui::RichTooltip::info(toggle("Khớp thuộc tính")).need, ui::Edition::None);
        toggle("Khớp thuộc tính")->trigger();
        ui::Button* match = qat->button(u"MATCHPROP"_s);
        QVERIFY(match->isVisible() && !match->isLocked());
        QCOMPARE(ui::RichTooltip::info(match).command, u"MATCHPROP"_s);
        QCOMPARE(match->accessibleName(), reg_->action(u"MATCHPROP"_s)->text());
        gate_->setEdition(Edition::Viewer); // hạ cấp: nút còn, có ổ khóa; bấm mở trang nâng cấp
        QVERIFY(match->isLocked());
        match->click();
        QVERIFY(sent_.isEmpty());
        toggle("Khớp thuộc tính")->trigger(); // tắt vẫn được khi khóa
        QVERIFY(!match->isVisible());
    }

    // DoD-U: rảnh không timer chạy.
    void idleNoTimer()
    {
        QCoreApplication::processEvents();
        for (const QTimer* t : bar_->findChildren<QTimer*>())
            QVERIFY2(!t->isActive(), qPrintable(t->objectName()));
        for (const QTimer* t : search()->popup()->findChildren<QTimer*>())
            QVERIFY(!t->isActive());
    }

    // DoD-U: chữ dài hơn 40% (vi-XL) không bị cắt ở thanh rộng 1366 px (trừ 3 nút cửa sổ của Windows), cả gọn và
    // thoáng; popup kết quả và các menu cũng vậy.
    void pseudoNoTruncation()
    {
        const QString before = child<QLabel>("editionBadge")->text();
        QVERIFY(translator_->setLanguage(i18n::kPseudoLanguage.toString()));
        QVERIFY(spinUntil([&] { return child<QLabel>("editionBadge")->text() != before; }));
        gate_->setEdition(Edition::Trial);
        bar_->setTrialDays(12);
        bar_->setOfflineDays(5);
        bar_->setAccount({.name = u"Nguyễn Minh An"_s, .email = u"an.nguyen@example.com"_s, .signedIn = true});
        bar_->setDocument({.name = u"Banve-01.dwg"_s, .path = u"D:/Banve-01.dwg"_s, .dirty = true, .recovered = true});
        bar_->setContentsMargins(0, 0, 3 * 46, 0); // ponytail: nút cửa sổ của prototype (.win button 44 px + lề)
        for (const theme::Density d : {theme::Density::Comfortable, theme::Density::Compact}) {
            tm_->setDensity(d);
            spinUntil([&] { return truncations(bar_).isEmpty(); }, 5s);
            QCOMPARE(truncations(bar_), QString());
            QCOMPARE(bar_->shownName(), u"Banve-01.dwg"_s);
            focus(search());
            type(search(), u"đường tròn"_s);
            QVERIFY(popup()->isVisible());
            QCOMPARE(truncations(popup()), QString());
            key(Qt::Key_Escape);
        }
    }

    // DoD-U 1: ảnh thanh tiêu đề (bản vẽ chưa lưu, đã phục hồi, dùng thử) và popup kết quả, Tối và Sáng, 100%.
    void baseline_data()
    {
        QTest::addColumn<bool>("light");
        QTest::addRow("dark") << false;
        QTest::addRow("light") << true;
    }

    void baseline()
    {
        QFETCH(bool, light);
        tm_->setTheme(light ? theme::Theme::Light : theme::Theme::Dark);
        QCoreApplication::processEvents(); // Qt 6.11 gửi ApplicationPaletteChange qua hàng đợi
        gate_->setEdition(Edition::Trial);
        bar_->setTrialDays(12);
        bar_->setDocument({.name = u"Banve-01.dwg"_s, .path = u"D:/Banve-01.dwg"_s, .dirty = true, .recovered = true});
        search()->setLocator([](const QString& c) { return c == u"CIRCLE"_s ? u"Trang chủ › Vẽ"_s : QString(); });
        focus(search());
        type(search(), u"duong tron"_s);
        QVERIFY(popup()->isVisible());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);

        const int pad = 10, w = bar_->width() + 2 * pad;
        QImage sheet(QSize(w, bar_->height() + popup()->height() + 3 * pad), QImage::Format_ARGB32_Premultiplied);
        sheet.fill(tm_->color(tok::c_base));
        QPainter p(&sheet);
        bar_->render(&p, QPoint(pad, pad));
        popup()->render(&p, QPoint(w - pad - popup()->width(), bar_->height() + 2 * pad),
                        {}, QWidget::DrawWindowBackground | QWidget::DrawChildren);
        p.end();
        search()->setLocator({});
        key(Qt::Key_Escape);

        const QString name = u"titlebar-%1-100-%2.png"_s.arg(light ? u"light"_s : u"dark"_s, QLatin1StringView(kPlatform));
        const QString out = qEnvironmentVariable("TRION_UPDATE_BASELINE");
        if (!out.isEmpty()) {
            QVERIFY2(sheet.save(out + u'/' + name, "PNG", 0), qPrintable(out));
            QSKIP("đã ghi baseline");
        }
        const QImage want = QImage(u":/baseline/"_s + name).convertToFormat(QImage::Format_ARGB32_Premultiplied);
        QVERIFY2(!want.isNull(), qPrintable(u"thiếu baseline "_s + name));
        QCOMPARE(sheet.size(), want.size());
        qsizetype off = 0;
        int maxDiff = 0;
        for (int y = 0; y < sheet.height(); ++y) {
            const auto* a = reinterpret_cast<const QRgb*>(sheet.constScanLine(y));
            const auto* b = reinterpret_cast<const QRgb*>(want.constScanLine(y));
            for (int x = 0; x < sheet.width(); ++x) {
                const int d = std::max({std::abs(qRed(a[x]) - qRed(b[x])), std::abs(qGreen(a[x]) - qGreen(b[x])),
                                        std::abs(qBlue(a[x]) - qBlue(b[x])), std::abs(qAlpha(a[x]) - qAlpha(b[x]))});
                maxDiff = std::max(maxDiff, d);
                off += d > kNoise;
            }
        }
        qInfo("%s: %lld điểm ảnh lệch > %d/255, lệch lớn nhất %d/255", qPrintable(name), qint64(off), kNoise, maxDiff);
        if (off > sheet.width() * sheet.height() / 1000)
            sheet.save(QDir::temp().filePath(u"got-"_s + name)); // để so bằng mắt
        QVERIFY(off <= sheet.width() * sheet.height() / 1000);
    }
};

QTEST_MAIN(TitleBarTest)
#include "titlebar_test.moc"
