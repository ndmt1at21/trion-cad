// UIA-010: dải tab tệp (FileTabBar.h). Phiên DOC-018, render RND-021, bộ định tuyến INT-002 chưa có: tài liệu mẫu qua
// addDocument, nguồn ảnh giả (đếm lời gọi, chậm theo ý, thấy CancelToken), tín hiệu commandRequested đối chiếu bảng.
//   1  thumbnailCached: ảnh có trong cache hiện cùng lượt mở popup (không gọi nguồn), ≤ 150 ms sau khi rê (release);
//      bấm ảnh → activateRequested(id, layout); updateDocument bỏ ảnh cũ. thumbnailPending: nguồn chậm 300 ms — ô giữ
//      chỗ ngay, ảnh hiện khi xong, không lượt sự kiện nào ≥ 17 ms (UiProbe, release). thumbnailCancel: rời chuột thì
//      CancelToken của mọi việc đang chạy bật, việc dừng sớm. BENCH-STD 1 triệu entity: Hoãn (cần RND-021).
//   2  menuMacros: mọi mục menu (tab bản vẽ, tab Start) phát đúng macro theo bảng; mục bị vô hiệu ở Start; Lưu tất cả
//      khi không có gì chưa lưu: toast; sao chép đường dẫn, mở thư mục. closeGestures: chuột giữa, ×, "+".
//      viewerProfile: hồ sơ Viewer — "+" mở bản vẽ, ổ khóa, mục Pro mở trang nâng cấp thay vì phát lệnh.
//   3  fiftyTabs: ▾ liệt kê đủ 50 bản vẽ (và Start); tab đang mở luôn hiện; chuyển tab ≤ 100 ms tới khung kế (release).
//   4  dropFiles: thả 5 tệp DWG/DXF → openRequested đúng thứ tự; .pdf/.txt bị từ chối kèm toast lỗi nêu tên.
//      readDrop: quy tắc lọc và câu từ chối dùng chung với trang Start.
//   Còn lại: sessionHooks (chỗ nối DOC-018), dragReorder, keyboard (Tab đi qua, ←→, Shift+F10/phím Menu, next/previous,
//   tên trợ năng), idle (rảnh không timer), pseudoNoTruncation (vi-XL, gọn và thoáng), baseline (Tối/Sáng 100% so
//   baseline theo nền tảng; ghi lại: TRION_UPDATE_BASELINE=<thư mục>).
#include "app/shell/filetabs/FileTabBar.h"

#include "app/core/UiExecutor.h"
#include "app/core/edition/EditionGate.h"
#include "app/core/i18n/Translator.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/core/keymap/Keymap.h"
#include "app/core/perf/UiProbe.h"
#include "app/theme/ThemeManager.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/widgets/message/Messages.h"
#include "app/widgets/tooltip/RichTooltip.h"
#include "core/base/sched/Scheduler.h"
#include "tests/timing.h"

#include <QAbstractButton>
#include <QAccessible>
#include <QApplication>
#include <QBoxLayout>
#include <QClipboard>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>
#include <atomic>
#include <memory>
#include <thread>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;
using namespace trion::theme;
using namespace trion::ui;
using trion::shell::DocTab;
using trion::shell::FileTabBar;
using trion::test::kTimed;
namespace edition = trion::edition;
namespace i18n = trion::i18n;
namespace keymap = trion::keymap;
namespace probe = trion::ui::probe;
namespace sched = trion::sched;
namespace settings = trion::settings;
using Clock = std::chrono::steady_clock;
using Emitted = QList<QPair<quint64, QString>>;

namespace {

#if defined(Q_OS_WIN)
constexpr auto kPlatform = "windows";
#elif defined(Q_OS_MACOS)
constexpr auto kPlatform = "macos";
#else
constexpr auto kPlatform = "linux";
#endif
constexpr int kNoise = 8; // baseline: lệch ≤ 8/255 mỗi kênh bỏ qua, điểm lệch hơn được tối đa 0,1% ảnh
constexpr int kSheetW = 600;

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

void spinFor(std::chrono::milliseconds d)
{
    QEventLoop loop;
    QTimer::singleShot(d, &loop, &QEventLoop::quit);
    loop.exec();
}

double ms(Clock::duration d)
{
    return std::chrono::duration<double, std::milli>(d).count();
}

int px(tok::PxId id)
{
    return qRound(ThemeManager::instance().px(id));
}

// Nguồn ảnh giả (RND-021 chưa có): đếm lời gọi, chậm delayMs nhưng thấy CancelToken mỗi 2 ms; ảnh màu theo layout.
// Layout "loi" trả ảnh rỗng (lỗi: ô giữ chỗ ở lại).
struct FakeSource {
    std::atomic<int> calls{0};
    std::atomic<int> cancels{0};
    std::atomic<int> delayMs{0};

    FileTabBar::ThumbnailSource function()
    {
        return [this](quint64, const QString& layout, QSize size, sched::CancelToken cancel) {
            ++calls;
            const auto until = Clock::now() + std::chrono::milliseconds(delayMs.load());
            while (Clock::now() < until) {
                if (cancel.cancelled()) {
                    ++cancels;
                    return QImage();
                }
                std::this_thread::sleep_for(2ms);
            }
            if (layout == u"loi")
                return QImage();
            QImage image(size, QImage::Format_ARGB32_Premultiplied);
            const bool model = layout == FileTabBar::kModel;
            image.fill(model ? QColor(15, 17, 19) : QColor(42, 46, 51));
            QPainter p(&image);
            p.setPen(QPen(model ? QColor(60, 200, 192) : QColor(255, 255, 255), 2));
            p.drawRect(image.rect().adjusted(size.width() / 6, size.height() / 5, -size.width() / 6, -size.height() / 5));
            return image;
        };
    }
};

DocTab doc(quint64 id, const QString& name, bool dirty = false, bool readOnly = false, const QStringList& layouts = {},
           const QString& path = {})
{
    return {.id = id, .name = name, .path = path, .dirty = dirty, .readOnly = readOnly, .layouts = layouts};
}

QString truncations(QWidget* root)
{
    QStringList out;
    for (const i18n::Truncation& t : i18n::findTruncations(root))
        out << t.toString();
    return out.join(u"; "_s);
}

// Đếm lần vẽ của widget (chuyển tab tới khung kế).
class PaintCounter final : public QObject {
public:
    explicit PaintCounter(QWidget* w) { w->installEventFilter(this); }
    int paints = 0;

protected:
    bool eventFilter(QObject*, QEvent* e) override
    {
        paints += e->type() == QEvent::Paint;
        return false;
    }
};

} // namespace

class FileTabsTest : public QObject {
    Q_OBJECT
    QTemporaryDir dir_;
    std::unique_ptr<settings::Store> store_;
    std::unique_ptr<UiExecutor> executor_;
    std::unique_ptr<ThemeManager> tm_;
    std::unique_ptr<IconProvider> icons_;
    std::unique_ptr<i18n::Translator> translator_;
    std::unique_ptr<keymap::Keymap> keymap_;
    std::unique_ptr<RichTooltip> tips_;
    std::unique_ptr<NotificationCenter> center_;
    std::unique_ptr<ToastHost> toasts_;
    std::unique_ptr<QWidget> window_; // ô nhập trước, dải tab, vùng vẽ giả, ô nhập sau (thứ tự Tab)
    QLineEdit* before_ = nullptr;
    QLineEdit* after_ = nullptr;
    FileTabBar* bar_ = nullptr;
    FakeSource fake_;

    // Cửa sổ mới với dải mới (dải theo EditionGate có lúc tạo).
    void makeWindow()
    {
        toasts_.reset();
        window_ = std::make_unique<QWidget>();
        auto* col = new QVBoxLayout(window_.get());
        col->setContentsMargins({});
        col->setSpacing(0);
        before_ = new QLineEdit;
        bar_ = new FileTabBar;
        auto* canvas = new QWidget;
        after_ = new QLineEdit;
        col->addWidget(before_);
        col->addWidget(bar_);
        col->addWidget(canvas, 1);
        col->addWidget(after_);
        window_->resize(1366, 768);
        toasts_ = std::make_unique<ToastHost>(window_.get(), *center_);
        toasts_->setTimeout(1h);
        window_->show();
        QVERIFY(QTest::qWaitForWindowExposed(window_.get()));
        // Con trỏ của test trước có thể đứng đúng chỗ tab của cửa sổ mới: Qt bỏ lần di chuột tới cùng điểm, nên đưa
        // chuột ra chỗ trống của cửa sổ mới trước.
        moveTo(QPoint(window_->width() / 3, window_->height() - 40));
    }

    QWindow* win() const { return window_->windowHandle(); }
    QTabBar* tabs() const { return bar_->tabBar(); }
    QPoint at(QWidget* w, QPoint p) const { return w->mapTo(window_.get(), p); }
    QPoint tabCenter(int index) const { return at(tabs(), tabs()->tabRect(index).center()); }
    QPoint closeCenter(int index) const // × của tab (.ft-tab .x 18 px, lề phải --sp-3, giữa phần cao --h-ftab - 4)
    {
        const QRect r = tabs()->tabRect(index);
        const int h = px(tok::h_ftab) - px(tok::sp_2);
        return at(tabs(), QPoint(r.right() + 1 - px(tok::sp_3) - 9, r.bottom() + 1 - h / 2));
    }
    // Di chuột không nhấn qua QCursor::setPos (QTest::mouseMove(QWidget*)): nền offscreen tự phát vào/ra cửa sổ như
    // nền tảng thật (chuột sang popup ảnh rồi về cửa sổ chính); QTest::mouseMove(QWindow*) thì không.
    void moveTo(QPoint inWindow) { QTest::mouseMove(window_.get(), inWindow); }
    void hoverTab(int index) { moveTo(tabCenter(index)); }
    void mouseAway() { moveTo(QPoint(window_->width() / 2, window_->height() / 2)); }

    QList<QAbstractButton*> cells() const
    {
        QWidget* popup = bar_->thumbnailPopup();
        return popup ? popup->findChildren<QAbstractButton*>() : QList<QAbstractButton*>{};
    }
    bool popupVisible() const { return bar_->thumbnailPopup() && bar_->thumbnailPopup()->isVisible(); }
    int loadedCells() const
    {
        const auto all = cells();
        return int(std::count_if(all.begin(), all.end(), [](QAbstractButton* c) { return !c->icon().isNull(); }));
    }
    bool loaded() const { return popupVisible() && loadedCells() == cells().size(); }

    Button* button(const QString& icon) const
    {
        for (Button* b : bar_->findChildren<Button*>())
            if (b->iconName() == icon)
                return b;
        return nullptr;
    }
    QMenu* menu(const QString& name) const { return bar_->findChild<QMenu*>(name); }

    // Menu chuột phải của tab (sự kiện như nền tảng gửi sau cú nhấp phải).
    QMenu* openMenu(int index)
    {
        const QPoint pos = tabs()->tabRect(index).center();
        QContextMenuEvent e(QContextMenuEvent::Mouse, pos, tabs()->mapToGlobal(pos));
        QApplication::sendEvent(tabs(), &e);
        QMenu* m = menu(u"fileTabMenu"_s);
        return m && spinUntil([&] { return m->isVisible(); }) ? m : nullptr;
    }
    QAction* item(QMenu* m, const QString& id) const
    {
        for (QAction* a : m->actions())
            if (a->objectName() == id)
                return a;
        return nullptr;
    }

    QString lastToast() const { return toasts_->toasts().isEmpty() ? QString() : toasts_->toasts().last()->text(); }

private slots:
    void initTestCase()
    {
        QVERIFY(dir_.isValid());
        store_ = std::make_unique<settings::Store>(dir_.filePath(u"user.json"_s));
        settings::Store::setInstance(store_.get());
        QVERIFY(loadFonts()); // chữ Plex nhúng: ảnh không phụ thuộc font của máy
        executor_ = std::make_unique<UiExecutor>();
        tm_ = std::make_unique<ThemeManager>(*store_);
        icons_ = std::make_unique<IconProvider>();
        translator_ = std::make_unique<i18n::Translator>();
        keymap_ = std::make_unique<keymap::Keymap>(*store_, keymap::Platform::Windows);
        tips_ = std::make_unique<RichTooltip>();
        tips_->setKeymap(keymap_.get());
        center_ = std::make_unique<NotificationCenter>();
    }

    void init()
    {
        tm_->setTheme(Theme::Dark);
        tm_->setDensity(Density::Compact);
        fake_.calls = 0;
        fake_.cancels = 0;
        fake_.delayMs = 0;
        makeWindow();
    }

    void cleanup()
    {
        probe::disable();
        while (QWidget* p = QApplication::activePopupWidget())
            p->close();
        sched::waitIdle(); // việc tạo ảnh còn chạy dùng fake_
        toasts_.reset();
        window_.reset();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents();
        if (translator_->language() != u"vi"_s)
            QVERIFY(translator_->setLanguage(u"vi"_s));
    }

    void cleanupTestCase()
    {
        sched::waitIdle();
        center_.reset();
        tips_.reset();
        keymap_.reset();
        translator_.reset();
        icons_.reset();
        tm_.reset();
        settings::Store::setInstance(nullptr);
        store_.reset();
        QCoreApplication::processEvents();
        executor_.reset();
    }

    // Chỗ nối DOC-018: setCurrent, removeDocument không phát activateRequested; addDocument trùng id là cập nhật;
    // tên có "&" không thành phím tắt.
    void sessionHooks()
    {
        QSignalSpy activate(bar_, &FileTabBar::activateRequested);
        bar_->addDocument(doc(1, u"A.dwg"_s));
        bar_->addDocument(doc(2, u"B.dwg"_s));
        bar_->addDocument(doc(3, u"C.dwg"_s));
        QCOMPARE(bar_->documents(), (QList<quint64>{1, 2, 3}));
        QCOMPARE(bar_->current(), FileTabBar::kStart);
        bar_->setCurrent(2);
        QCOMPARE(bar_->current(), quint64(2));
        bar_->updateDocument(doc(2, u"R&D.dwg"_s, true));
        QCOMPARE(tabs()->tabText(2), u"R&&D.dwg"_s);
        QVERIFY(tabs()->accessibleTabName(2).startsWith(u"R&D.dwg"_s));
        bar_->addDocument(doc(1, u"A2.dwg"_s)); // đã có: cập nhật, không thêm tab
        QCOMPARE(bar_->documents(), (QList<quint64>{1, 2, 3}));
        QCOMPARE(tabs()->tabText(1), u"A2.dwg"_s);
        bar_->removeDocument(2);
        QCOMPARE(bar_->documents(), (QList<quint64>{1, 3}));
        bar_->removeDocument(FileTabBar::kStart); // Start không đóng
        QCOMPARE(tabs()->count(), 3);
        QCOMPARE(activate.count(), 0);
        bar_->setCurrent(FileTabBar::kStart);
        QTest::mouseClick(win(), Qt::LeftButton, {}, tabCenter(2));
        QCOMPARE(activate.count(), 1);
        QCOMPARE(activate[0][0].toULongLong(), quint64(3));
        QCOMPARE(activate[0][1].toString(), QString());
    }

    // Tiêu chí 1: ảnh trong cache hiện cùng lượt mở popup, không gọi nguồn; bấm ảnh nhảy tới layout.
    void thumbnailCached()
    {
        bar_->setThumbnailSource(fake_.function());
        bar_->addDocument(doc(1, u"Nha-pho.dwg"_s, false, false, {u"A1-Mat-bang"_s, u"A2-Mat-dung"_s}));
        bar_->addDocument(doc(2, u"Ket-cau.dwg"_s));
        hoverTab(1);
        QVERIFY(spinUntil([&] { return loaded(); }));
        QCOMPARE(fake_.calls.load(), 3);
        QStringList labels;
        for (QAbstractButton* c : cells())
            labels << c->text();
        QCOMPARE(labels, (QStringList{u"Model"_s, u"A1-Mat-bang"_s, u"A2-Mat-dung"_s}));
        mouseAway();
        QVERIFY(spinUntil([&] { return !popupVisible(); }));

        const auto hovered = Clock::now();
        hoverTab(1);
        QVERIFY(spinUntil([&] { return popupVisible(); }));
        const double shown = ms(Clock::now() - hovered);
        QCOMPARE(loadedCells(), 3); // ngay lượt mở popup, không chờ việc nền
        QCOMPARE(fake_.calls.load(), 3);
        qInfo("ảnh trong cache hiện %.1f ms sau khi rê (chờ rê %lld ms)", shown,
              qint64(FileTabBar::kThumbDelay.count()));
        QVERIFY2(!kTimed || shown <= 150, qPrintable(u"%1 ms"_s.arg(shown)));

        QSignalSpy activate(bar_, &FileTabBar::activateRequested);
        QTest::mouseMove(cells()[2]);
        QTest::mouseClick(cells()[2], Qt::LeftButton);
        QCOMPARE(activate.count(), 1);
        QCOMPARE(activate[0][0].toULongLong(), quint64(1));
        QCOMPARE(activate[0][1].toString(), u"A2-Mat-dung"_s);
        QVERIFY(!popupVisible());
        QCOMPARE(bar_->current(), quint64(1));

        // Tài liệu đổi: ảnh cũ bỏ, lần rê sau tạo lại
        bar_->updateDocument(doc(1, u"Nha-pho.dwg"_s, true, false, {u"A1-Mat-bang"_s, u"A2-Mat-dung"_s}));
        mouseAway();
        hoverTab(1);
        QVERIFY(spinUntil([&] { return loaded(); }));
        QCOMPARE(fake_.calls.load(), 6);
        // Start không có ảnh
        hoverTab(0);
        spinFor(FileTabBar::kThumbDelay * 2);
        QVERIFY(!popupVisible());
    }

    // Tiêu chí 1: chưa có ảnh — ô giữ chỗ ngay, ảnh hiện khi xong; luồng UI không khựng trong lúc chờ.
    void thumbnailPending()
    {
        fake_.delayMs = 300;
        bar_->setThumbnailSource(fake_.function());
        bar_->addDocument(doc(1, u"Nha-pho.dwg"_s, false, false, {u"A1"_s, u"loi"_s}));
        probe::enable();
        probe::setStallThreshold(17ms); // lượt sự kiện > 16,6 ms
        probe::reset();
        hoverTab(1);
        QVERIFY(spinUntil([&] { return popupVisible(); }));
        const auto shownAt = Clock::now();
        QCOMPARE(cells().size(), 3);
        QCOMPARE(loadedCells(), 0); // ô giữ chỗ
        QVERIFY(spinUntil([&] { return loadedCells() == 2; }));
        const double waited = ms(Clock::now() - shownAt);
        sched::waitIdle();
        QCoreApplication::processEvents();
        QCOMPARE(loadedCells(), 2); // "loi": nguồn trả ảnh rỗng, ô giữ chỗ ở lại
        const auto stalls = probe::stalls();
        probe::disable();
        double worst = 0;
        for (const auto& s : stalls)
            worst = std::max(worst, ms(s.duration));
        qInfo("ảnh hiện sau %.0f ms (nguồn 300 ms); %zu lượt sự kiện ≥ 17 ms, dài nhất %.1f ms", waited, stalls.size(),
              worst);
        QVERIFY(waited >= 250);
        QVERIFY2(!kTimed || stalls.empty(), stalls.empty() ? "" : stalls.front().where.c_str());
    }

    // Tiêu chí 1: rời chuột thì việc tạo ảnh đang chạy thấy CancelToken và dừng sớm.
    void thumbnailCancel()
    {
        fake_.delayMs = 5000;
        bar_->setThumbnailSource(fake_.function());
        bar_->addDocument(doc(1, u"Nha-pho.dwg"_s, false, false, {u"A1"_s}));
        bar_->addDocument(doc(2, u"Ket-cau.dwg"_s));
        hoverTab(1);
        QVERIFY(spinUntil([&] { return fake_.calls >= 1; }));
        const auto left = Clock::now();
        mouseAway();
        QVERIFY(spinUntil([&] { return !popupVisible(); }));
        QVERIFY(spinUntil([&] { return fake_.cancels.load() == fake_.calls.load(); }));
        sched::waitIdle();
        const double stopped = ms(Clock::now() - left);
        qInfo("%d việc tạo ảnh dừng sau %.0f ms kể từ lúc rời (chờ ẩn %lld ms)", fake_.calls.load(), stopped,
              qint64(FileTabBar::kThumbGrace.count()));
        QVERIFY(stopped < 2500); // nguồn 5 s: dừng nhờ hủy
        // Sang tab khác cũng hủy việc của tab cũ
        const int calls = fake_.calls, cancels = fake_.cancels;
        hoverTab(1);
        QVERIFY(spinUntil([&] { return fake_.calls > calls; }));
        hoverTab(2);
        QVERIFY(spinUntil([&] { return fake_.cancels > cancels; }));
        mouseAway();
        QVERIFY(spinUntil([&] { return !popupVisible(); }));
        sched::waitIdle();
        QCOMPARE(fake_.cancels.load(), fake_.calls.load());
    }

    // Tiêu chí 2: mọi mục menu phát đúng macro (bảng đối chiếu, theo prototype và AutoCAD).
    void menuMacros()
    {
        const QString path = dir_.filePath(u"du-an/B.dwg"_s);
        bar_->addDocument(doc(1, u"A.dwg"_s, true, false, {}, dir_.filePath(u"A.dwg"_s)));
        bar_->addDocument(doc(2, u"B.dwg"_s, false, false, {}, path));
        bar_->addDocument(doc(3, u"C.dwg"_s, true));
        bar_->setCurrent(1);
        QSignalSpy commands(bar_, &FileTabBar::commandRequested);
        const auto emitted = [&] {
            Emitted out;
            for (const QList<QVariant>& args : commands)
                out << qMakePair(args[0].toULongLong(), args[1].toString());
            commands.clear();
            return out;
        };
        struct Row {
            int tab;
            const char* item;
            Emitted want;
        };
        const QList<Row> table = {
            {2, "NEW", {{2, u"^C^C_NEW"_s}}},
            {2, "OPEN", {{2, u"^C^C_OPEN"_s}}},
            {2, "QSAVE", {{2, u"^C^C_QSAVE"_s}}},
            {2, "SAVEALL", {{1, u"^C^C_QSAVE"_s}, {3, u"^C^C_QSAVE"_s}}}, // như prototype: từng bản vẽ chưa lưu
            {2, "CLOSE", {{2, u"^C^C_CLOSE"_s}}},
            {2, "CLOSEALL", {{2, u"^C^C_CLOSEALL"_s}}},
            {2, "_closeOthers", {{1, u"^C^C_CLOSE"_s}, {3, u"^C^C_CLOSE"_s}}},
            {0, "NEW", {{0, u"^C^C_NEW"_s}}},
            {0, "OPEN", {{0, u"^C^C_OPEN"_s}}},
            {0, "SAVEALL", {{1, u"^C^C_QSAVE"_s}, {3, u"^C^C_QSAVE"_s}}},
            {0, "CLOSEALL", {{0, u"^C^C_CLOSEALL"_s}}},
        };
        for (const Row& row : table) {
            QMenu* m = openMenu(row.tab);
            QVERIFY(m);
            QAction* a = item(m, QString::fromLatin1(row.item));
            QVERIFY2(a && a->isEnabled(), row.item);
            a->trigger();
            m->hide();
            QCOMPARE(emitted(), row.want);
        }
        QCOMPARE(bar_->current(), quint64(1)); // menu không tự đổi tab: lệnh mang id

        QMenu* m = openMenu(0); // Start: mục của một bản vẽ bị vô hiệu
        for (const char* id : {"QSAVE", "CLOSE", "_closeOthers", "_copyPath", "_openFolder"})
            QVERIFY2(!item(m, QString::fromLatin1(id))->isEnabled(), id);
        m->hide();
        m = openMenu(3); // chưa lưu lần nào: không có đường dẫn
        QVERIFY(!item(m, u"_copyPath"_s)->isEnabled());
        QVERIFY(!item(m, u"_openFolder"_s)->isEnabled());
        m->hide();
        // Phím của mục theo Keymap (Windows): Mới Ctrl+N, Đóng Ctrl+F4
        QCOMPARE(RichTooltip::info(item(m, u"NEW"_s)).keyId, u"new"_s);
        QCOMPARE(RichTooltip::info(item(m, u"CLOSE"_s)).keyId, u"close"_s);

        // Sao chép đường dẫn (dạng của nền tảng), mở thư mục chứa (hàm giả); không phát lệnh
        m = openMenu(2);
        item(m, u"_copyPath"_s)->trigger();
        m->hide();
        QCOMPARE(QGuiApplication::clipboard()->text(), QDir::toNativeSeparators(path));
        QVERIFY(lastToast().contains(QDir::toNativeSeparators(path)));
        QStringList opened;
        bool openOk = true;
        bar_->setFolderOpener([&](const QString& dir) {
            opened << dir;
            return openOk;
        });
        m = openMenu(2);
        item(m, u"_openFolder"_s)->trigger();
        m->hide();
        QCOMPARE(opened, QStringList{QFileInfo(path).absolutePath()});
        const int toastsBefore = int(center_->rowCount());
        openOk = false; // không mở được: báo
        m = openMenu(2);
        item(m, u"_openFolder"_s)->trigger();
        m->hide();
        QCOMPARE(center_->rowCount(), toastsBefore + 1);
        QCOMPARE(emitted(), Emitted{});

        // Lưu tất cả khi không có gì chưa lưu: báo, không phát lệnh
        bar_->updateDocument(doc(1, u"A.dwg"_s));
        bar_->updateDocument(doc(3, u"C.dwg"_s));
        m = openMenu(1);
        item(m, u"SAVEALL"_s)->trigger();
        m->hide();
        QCOMPARE(emitted(), Emitted{});
        QCOMPARE(center_->rowCount(), toastsBefore + 2);
        QCOMPARE(toasts_->toasts().last()->severity(), Severity::Info);
    }

    // Tiêu chí 2: chuột giữa, × phát CLOSE cho đúng tab (không tự kích hoạt, không hỏi lưu); "+" phát NEW.
    void closeGestures()
    {
        bar_->addDocument(doc(1, u"A.dwg"_s, true));
        bar_->addDocument(doc(2, u"B.dwg"_s));
        bar_->addDocument(doc(3, u"C.dwg"_s));
        bar_->setCurrent(2);
        QSignalSpy commands(bar_, &FileTabBar::commandRequested);
        QSignalSpy activate(bar_, &FileTabBar::activateRequested);
        QTest::mouseClick(win(), Qt::MiddleButton, {}, tabCenter(3));
        QCOMPARE(commands.count(), 1);
        QCOMPARE(commands[0][0].toULongLong(), quint64(3));
        QCOMPARE(commands[0][1].toString(), u"^C^C_CLOSE"_s);
        QTest::mouseClick(win(), Qt::LeftButton, {}, closeCenter(1)); // × của tab chưa lưu
        QCOMPARE(commands.count(), 2);
        QCOMPARE(commands[1][0].toULongLong(), quint64(1));
        QCOMPARE(commands[1][1].toString(), u"^C^C_CLOSE"_s);
        QTest::mouseClick(win(), Qt::MiddleButton, {}, tabCenter(0)); // Start không đóng
        QCOMPARE(commands.count(), 2);
        QCOMPARE(activate.count(), 0);
        QCOMPARE(bar_->current(), quint64(2));
        QTest::mouseClick(button(u"plus"_s), Qt::LeftButton);
        QCOMPARE(commands.count(), 3);
        QCOMPARE(commands[2][0].toULongLong(), quint64(2));
        QCOMPARE(commands[2][1].toString(), u"^C^C_NEW"_s);
    }

    // DoD-U 8 (UI0-011): Viewer — "+" mở bản vẽ, mọi tab có ổ khóa, mục cần Pro mở trang nâng cấp; lên Pro thì đổi ngay.
    void viewerProfile()
    {
        edition::EditionGate gate; // Viewer tới khi có giấy phép
        QList<edition::UpgradeRequest> pages;
        gate.upgrade().setHandler([&](const edition::UpgradeRequest& r) { pages << r; });
        makeWindow(); // dải tạo sau EditionGate
        bar_->addDocument(doc(1, u"A.dwg"_s, true));
        QSignalSpy commands(bar_, &FileTabBar::commandRequested);
        QVERIFY(tabs()->accessibleTabName(1).endsWith(u"chỉ đọc"_s));
        const int viewerWidth = tabs()->tabRect(1).width();
        QTest::mouseClick(button(u"plus"_s), Qt::LeftButton);
        QCOMPARE(commands.count(), 1);
        QCOMPARE(commands[0][1].toString(), u"^C^C_OPEN"_s);
        QCOMPARE(button(u"plus"_s)->accessibleName(), FileTabBar::tr("Mở bản vẽ"));

        QMenu* m = openMenu(1);
        QCOMPARE(RichTooltip::info(item(m, u"NEW"_s)).need, Edition::Pro); // huy hiệu Pro
        QCOMPARE(RichTooltip::info(item(m, u"OPEN"_s)).need, Edition::None);
        item(m, u"QSAVE"_s)->trigger();
        m->hide();
        QCOMPARE(commands.count(), 1);
        QCOMPARE(pages.size(), 1);
        QCOMPARE(pages[0].feature, u"QSAVE"_s);
        QCOMPARE(pages[0].need, Edition::Pro);

        gate.setEdition(edition::Edition::Pro);
        QVERIFY(!tabs()->accessibleTabName(1).endsWith(u"chỉ đọc"_s));
        QVERIFY(spinUntil([&] { return tabs()->tabRect(1).width() < viewerWidth; })); // bỏ ổ khóa
        QTest::mouseClick(button(u"plus"_s), Qt::LeftButton);
        QCOMPARE(commands.count(), 2);
        QCOMPARE(commands[1][1].toString(), u"^C^C_NEW"_s);
        m = openMenu(1);
        QCOMPARE(RichTooltip::info(item(m, u"NEW"_s)).need, Edition::None);
        item(m, u"QSAVE"_s)->trigger();
        m->hide();
        QCOMPARE(commands.count(), 3);
        QCOMPARE(commands[2][1].toString(), u"^C^C_QSAVE"_s);
        QCOMPARE(pages.size(), 1);
        toasts_.reset();
        window_.reset(); // trước gate
    }

    // Tiêu chí 3: 50 tab — ▾ liệt kê đủ; tab đang mở luôn hiện, tab hiện vừa dải; chuyển tab ≤ 100 ms tới khung kế.
    void fiftyTabs()
    {
        QStringList names;
        for (int i = 1; i <= 50; ++i) {
            names << u"Ban-ve-%1.dwg"_s.arg(i, 2, 10, QChar(u'0'));
            bar_->addDocument(doc(quint64(i), names.last(), i % 3 == 0, i % 7 == 0));
        }
        Button* more = button(u"down"_s);
        QVERIFY(more && more->isVisible());
        const auto visibleTabs = [&] {
            int n = 0, right = 0;
            for (int i = 0; i < tabs()->count(); ++i)
                if (tabs()->isTabVisible(i)) {
                    ++n;
                    right = std::max(right, tabs()->tabRect(i).right());
                }
            return std::pair{n, at(tabs(), QPoint(right, 0)).x()};
        };
        auto [shown, right] = visibleTabs();
        qInfo("50 tab ở 1366 px: hiện %d tab (cả Start)", shown);
        QVERIFY(shown > 2 && shown < 51);
        QVERIFY(right < at(button(u"plus"_s), {}).x());
        QVERIFY(at(button(u"plus"_s), QPoint(button(u"plus"_s)->width(), 0)).x() <= at(more, {}).x());
        QCOMPARE(truncations(window_.get()), QString());

        QSignalSpy activate(bar_, &FileTabBar::activateRequested);
        QTest::mouseClick(more, Qt::LeftButton);
        QMenu* list = menu(u"fileTabList"_s);
        QVERIFY(spinUntil([&] { return list->isVisible(); }));
        QStringList listed;
        for (QAction* a : list->actions())
            listed << a->text();
        QCOMPARE(listed, QStringList{FileTabBar::tr("Start")} + names);
        list->actions()[45]->trigger(); // Ban-ve-45, đang ẩn
        list->hide();
        QCOMPARE(activate.count(), 1);
        QCOMPARE(activate[0][0].toULongLong(), quint64(45));
        QCOMPARE(bar_->current(), quint64(45));
        QVERIFY(tabs()->isTabVisible(45));
        std::tie(shown, right) = visibleTabs();
        QVERIFY(right < at(button(u"plus"_s), {}).x());

        // Chuyển tab tới khung kế (không có regen: chỉ phần của dải), qua ▾ và Ctrl+Tab (next)
        PaintCounter painted(tabs());
        std::vector<double> times;
        for (int round = 0; round < 20; ++round) {
            QAction* pick = nullptr;
            if (round % 2 == 0) { // mở menu không tính
                QTest::mouseClick(more, Qt::LeftButton);
                QVERIFY(spinUntil([&] { return list->isVisible(); }));
                pick = list->actions()[(round * 7 + 3) % 51];
            }
            const int paints = painted.paints;
            const auto t0 = Clock::now();
            if (pick) {
                pick->trigger();
                list->hide();
            } else {
                bar_->next();
            }
            QVERIFY(spinUntil([&] { return painted.paints > paints; }));
            times.push_back(ms(Clock::now() - t0));
        }
        std::sort(times.begin(), times.end());
        qInfo("chuyển tab tới khung kế (50 tab): p50 %.1f ms, max %.1f ms", times[times.size() / 2], times.back());
        QVERIFY2(!kTimed || times.back() <= 100, qPrintable(u"%1 ms"_s.arg(times.back())));
    }

    // Tiêu chí 4: thả 5 tệp DWG/DXF → openRequested đúng thứ tự; tệp khác bị từ chối kèm toast lỗi nêu tên.
    void dropFiles()
    {
        QSignalSpy open(bar_, &FileTabBar::openRequested);
        const auto drop = [&](const QList<QUrl>& urls) {
            QMimeData mime;
            mime.setUrls(urls);
            const QPoint pos = bar_->rect().center();
            QDragEnterEvent enter(pos, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(bar_, &enter);
            if (!enter.isAccepted())
                return false;
            QDropEvent dropped(QPointF(pos), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(bar_, &dropped);
            return true;
        };
        QStringList paths;
        QList<QUrl> urls;
        for (const char* name : {"Nha-pho.dwg", "Ket-cau.DXF", "Dien.dwg", "Nuoc.dxf", "Mat-bang.DWG"}) {
            paths << dir_.filePath(QString::fromLatin1(name));
            urls << QUrl::fromLocalFile(paths.last());
        }
        QVERIFY(drop(urls));
        QCOMPARE(open.count(), 1);
        QCOMPARE(open[0][0].toStringList(), paths);

        const int notes = center_->rowCount();
        QVERIFY(drop({QUrl::fromLocalFile(dir_.filePath(u"Bao-gia.pdf"_s)),
                      QUrl::fromLocalFile(dir_.filePath(u"ghi-chu.txt"_s))}));
        QCOMPARE(open.count(), 1);
        QCOMPARE(center_->rowCount(), notes + 1);
        QCOMPARE(toasts_->toasts().last()->severity(), Severity::Error);
        QVERIFY(lastToast().contains(u"“Bao-gia.pdf”"_s));
        QVERIFY(lastToast().contains(u"“ghi-chu.txt”"_s));

        // Trộn: mở phần đúng, báo phần sai
        QVERIFY(drop({QUrl::fromLocalFile(paths[0]), QUrl::fromLocalFile(dir_.filePath(u"anh.png"_s))}));
        QCOMPARE(open.count(), 2);
        QCOMPARE(open[1][0].toStringList(), QStringList{paths[0]});
        QCOMPARE(center_->rowCount(), notes + 2);
        QVERIFY(!drop({QUrl(u"https://trioncad.vn/mau.dwg"_s)})); // URL web: không nhận
        QCOMPARE(open.count(), 2);
    }

    // Quy tắc thả dùng chung (dải tab, trang Start): DWG, DXF, DWT không phân biệt hoa thường, theo thứ tự thả; câu từ
    // chối nêu tên, số ít/số nhiều; URL web bỏ qua.
    void readDrop()
    {
        const auto read = [](const QList<QUrl>& urls) {
            QMimeData mime;
            mime.setUrls(urls);
            return FileTabBar::readDrop(mime);
        };
        const QString a = dir_.filePath(u"Mau.DWT"_s), b = dir_.filePath(u"Nha.dwg"_s), c = dir_.filePath(u"Ket.Dxf"_s);
        FileTabBar::Drop d = read({QUrl::fromLocalFile(a), QUrl::fromLocalFile(dir_.filePath(u"Bao-gia.pdf"_s)),
                                   QUrl::fromLocalFile(b), QUrl(u"https://trioncad.vn/mau.dwg"_s),
                                   QUrl::fromLocalFile(c)});
        QCOMPARE(d.drawings, (QStringList{a, b, c}));
        QCOMPARE(d.rejection, FileTabBar::tr("TrionCAD mở được tệp DWG, DXF, DWT. %1 không phải bản vẽ.", nullptr, 1)
                                  .arg(u"“Bao-gia.pdf”"_s));
        QVERIFY(!d.empty());

        d = read({QUrl::fromLocalFile(dir_.filePath(u"anh.png"_s)), QUrl::fromLocalFile(dir_.filePath(u"dwg"_s))});
        QVERIFY(d.drawings.isEmpty()); // "dwg" không có đuôi: không phải bản vẽ
        QCOMPARE(d.rejection, FileTabBar::tr("TrionCAD mở được tệp DWG, DXF, DWT. %1 không phải bản vẽ.", nullptr, 2)
                                  .arg(u"“anh.png”, “dwg”"_s));

        QVERIFY(read({QUrl(u"https://trioncad.vn/mau.dwg"_s)}).empty()); // không có tệp trên máy: không nhận kéo
        QVERIFY(read({}).empty());
    }

    // Kéo tab bản vẽ đổi chỗ (moved); Start đứng yên ở đầu.
    void dragReorder()
    {
        bar_->addDocument(doc(1, u"A.dwg"_s));
        bar_->addDocument(doc(2, u"B.dwg"_s));
        bar_->addDocument(doc(3, u"C.dwg"_s));
        QSignalSpy moved(bar_, &FileTabBar::moved);
        QTest::mousePress(win(), Qt::LeftButton, {}, tabCenter(1));
        QTest::mouseMove(win(), tabCenter(2));
        QTest::mouseMove(win(), tabCenter(3));
        QTest::mouseRelease(win(), Qt::LeftButton, {}, tabCenter(3));
        QCOMPARE(bar_->documents(), (QList<quint64>{2, 3, 1}));
        QCOMPARE(moved.count(), 1);
        QCOMPARE(moved[0][0].toULongLong(), quint64(1));
        QCOMPARE(moved[0][1].toInt(), 2);
        QTest::mousePress(win(), Qt::LeftButton, {}, tabCenter(1)); // B lên trước Start: không được
        QTest::mouseMove(win(), tabCenter(0));
        QTest::mouseRelease(win(), Qt::LeftButton, {}, tabCenter(0));
        QCOMPARE(bar_->documents(), (QList<quint64>{2, 3, 1}));
        QCOMPARE(tabs()->tabText(0), FileTabBar::tr("Start"));
        QTest::mousePress(win(), Qt::LeftButton, {}, tabCenter(0)); // Start không kéo được
        QTest::mouseMove(win(), tabCenter(3));
        QTest::mouseRelease(win(), Qt::LeftButton, {}, tabCenter(3));
        QCOMPARE(tabs()->tabText(0), FileTabBar::tr("Start"));
        QCOMPARE(moved.count(), 1);
    }

    // DoD-U 3, 7: Tab đi qua dải, ←→ đổi tab, Shift+F10 / phím Menu mở menu của tab đang mở, Esc đóng; Ctrl+Tab (gắn
    // ở integration) qua next/previous vòng cả Start; tên trợ năng nêu trạng thái bằng chữ.
    void keyboard()
    {
        bar_->addDocument(doc(1, u"A.dwg"_s, true));
        bar_->addDocument(doc(2, u"B.dwg"_s, false, true));
        window_->activateWindow();
        before_->setFocus(Qt::OtherFocusReason);
        QVERIFY(spinUntil([&] { return QApplication::focusWidget() == before_; }));
        QSignalSpy activate(bar_, &FileTabBar::activateRequested);
        QTest::keyClick(before_, Qt::Key_Tab);
        QCOMPARE(QApplication::focusWidget(), tabs());
        QTest::keyClick(tabs(), Qt::Key_Right);
        QCOMPARE(activate.count(), 1);
        QCOMPARE(activate[0][0].toULongLong(), quint64(1));
        QTest::keyClick(tabs(), Qt::Key_Tab);
        QCOMPARE(QApplication::focusWidget(), button(u"plus"_s));
        QTest::keyClick(button(u"plus"_s), Qt::Key_Tab); // ▾ ẩn khi đủ chỗ
        QCOMPARE(QApplication::focusWidget(), after_);
        QTest::keyClick(after_, Qt::Key_Backtab, Qt::ShiftModifier);
        QTest::keyClick(button(u"plus"_s), Qt::Key_Backtab, Qt::ShiftModifier);
        QCOMPARE(QApplication::focusWidget(), tabs());

        QTest::keyClick(tabs(), Qt::Key_F10, Qt::ShiftModifier);
        QMenu* m = menu(u"fileTabMenu"_s);
        QVERIFY(spinUntil([&] { return m->isVisible(); }));
        QVERIFY(item(m, u"CLOSE"_s)->isEnabled()); // menu của A.dwg
        QTest::keyClick(m, Qt::Key_Escape);
        QVERIFY(spinUntil([&] { return !m->isVisible(); }));
        QVERIFY(spinUntil([&] { return QApplication::focusWidget() == tabs(); }));
        QTest::keyClick(tabs(), Qt::Key_Menu);
        QVERIFY(spinUntil([&] { return m->isVisible(); }));
        QTest::keyClick(m, Qt::Key_Escape);
        QVERIFY(spinUntil([&] { return !m->isVisible(); }));

        activate.clear();
        bar_->next(); // A → B
        bar_->next(); // B → Start
        bar_->previous(); // Start → B
        QCOMPARE(activate.count(), 3);
        QCOMPARE(activate[1][0].toULongLong(), FileTabBar::kStart);
        QCOMPARE(bar_->current(), quint64(2));

        QAccessibleInterface* a11y = QAccessible::queryAccessibleInterface(tabs());
        QVERIFY(a11y && a11y->childCount() >= 3);
        QCOMPARE(bar_->accessibleName(), FileTabBar::tr("Tab tệp"));
        QCOMPARE(a11y->child(1)->text(QAccessible::Name), FileTabBar::tr("%1, chưa lưu").arg(u"A.dwg"_s));
        QCOMPARE(a11y->child(2)->text(QAccessible::Name), FileTabBar::tr("%1, chỉ đọc").arg(u"B.dwg"_s));
        QCOMPARE(button(u"plus"_s)->accessibleName(), FileTabBar::tr("Bản vẽ mới"));
    }

    // DoD-H 6: rảnh không timer — sau khi rê, ảnh hiện rồi ẩn, dải không còn timer chạy.
    void idle()
    {
        bar_->setThumbnailSource(fake_.function());
        bar_->addDocument(doc(1, u"A.dwg"_s, false, false, {u"A1"_s}));
        const auto active = [&] {
            const auto all = bar_->findChildren<QTimer*>();
            return int(std::count_if(all.begin(), all.end(), [](const QTimer* t) { return t->isActive(); }));
        };
        QCOMPARE(active(), 0);
        hoverTab(1);
        QCOMPARE(active(), 1); // chờ rê
        QVERIFY(spinUntil([&] { return loaded(); }));
        QCOMPARE(active(), 0);
        mouseAway();
        QCOMPARE(active(), 1); // chờ ẩn
        QVERIFY(spinUntil([&] { return !popupVisible(); }));
        QCOMPARE(active(), 0);
    }

    // DoD-U: chữ dài hơn 40% (vi-XL) không bị cắt — tab Start, menu, ▾, ô ảnh — cả gọn và thoáng.
    void pseudoNoTruncation()
    {
        QVERIFY(translator_->setLanguage(i18n::kPseudoLanguage.toString()));
        QVERIFY(spinUntil([&] { return FileTabBar::tr("Start").size() > 5 && tabs()->tabText(0).size() > 5; }));
        bar_->setThumbnailSource(fake_.function());
        bar_->addDocument(doc(1, u"Nha-pho.dwg"_s, true, true, {u"A1"_s}, dir_.filePath(u"Nha-pho.dwg"_s)));
        bar_->addDocument(doc(2, u"Ket-cau.dwg"_s));
        for (Density d : {Density::Comfortable, Density::Compact}) {
            tm_->setDensity(d);
            QCoreApplication::processEvents();
            hoverTab(1);
            QVERIFY(spinUntil([&] { return loaded(); }));
            QCOMPARE(truncations(window_.get()), QString());
            QCOMPARE(truncations(bar_->thumbnailPopup()), QString());
            for (QAbstractButton* c : cells()) // ô ảnh tự vẽ: chữ đo bằng chính font của nó
                QVERIFY(QFontMetrics(trion::theme::font(TextRole::Caption)).horizontalAdvance(c->text())
                        <= c->width() - 2 * px(tok::sp_2));
            mouseAway();
            QVERIFY(spinUntil([&] { return !popupVisible(); }));
            QMenu* m = openMenu(1);
            QVERIFY(m);
            QCOMPARE(truncations(window_.get()), QString());
            m->hide();
        }
    }

    void baseline_data()
    {
        QTest::addColumn<bool>("light");
        for (bool light : {false, true})
            QTest::addRow("%s", light ? "light" : "dark") << light;
    }

    // DoD-U 1: dải (Start, tab đang mở chưa lưu, tab chỉ đọc đang rê, tab thường, +) và hàng ảnh (ảnh, ô giữ chỗ).
    void baseline()
    {
        QFETCH(bool, light);
        tm_->setTheme(light ? Theme::Light : Theme::Dark);
        QCoreApplication::processEvents(); // Qt 6.11 gửi ApplicationPaletteChange qua hàng đợi
        bar_->setThumbnailSource(fake_.function());
        bar_->addDocument(doc(1, u"Nha-pho-3-tang.dwg"_s, true));
        bar_->addDocument(doc(2, u"Ket-cau.dwg"_s, false, true, {u"A1-Mat-bang"_s, u"loi"_s}));
        bar_->addDocument(doc(3, u"Dien-nuoc.dwg"_s));
        bar_->setCurrent(1);
        hoverTab(2);
        QVERIFY(spinUntil([&] { return popupVisible() && loadedCells() == 2; }));
        sched::waitIdle();
        QCoreApplication::processEvents();

        const QColor bg = tm_->color(tok::c_base);
        const int pad = 10, barH = bar_->height();
        QWidget* popup = bar_->thumbnailPopup();
        const QPoint popupAt(pad + tabs()->x() + tabs()->tabRect(2).x(), pad + barH + px(tok::sp_2));
        QImage sheet(QSize(std::max(kSheetW, popupAt.x() + popup->width() + pad), popupAt.y() + popup->height() + pad),
                     QImage::Format_ARGB32_Premultiplied);
        sheet.fill(bg);
        QPainter p(&sheet);
        bar_->render(&p, QPoint(pad, pad), QRegion(0, 0, kSheetW - 2 * pad, barH));
        popup->render(&p, popupAt, {}, QWidget::DrawWindowBackground | QWidget::DrawChildren);
        p.end();

        const QString name = u"filetabs-%1-100-%2.png"_s.arg(light ? u"light"_s : u"dark"_s)
                                 .arg(QLatin1StringView(kPlatform));
        const QString out = qEnvironmentVariable("TRION_UPDATE_BASELINE");
        if (!out.isEmpty()) {
            QVERIFY(sheet.save(out + u'/' + name, "PNG", 0));
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

QTEST_MAIN(FileTabsTest)
#include "file_tabs_test.moc"
