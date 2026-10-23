// UIA-004: tab Start (StartPage.h), tệp gần đây (RecentFiles.h), ảnh xem trước (ThumbnailQueue.h). DWG-026, RND-021,
// PLF-002, PLF-006 và corpus QA-006 chưa có: nguồn ảnh giả (đếm lời gọi, chậm theo ý, thấy CancelToken, lỗi theo tên),
// tệp tạm trên đĩa, phiên bản đặt qua EditionGate::setEdition.
//   1  startupContent: 200 tệp gần đây → trang có nội dung ≤ 150 ms (release), ảnh chưa có ở khung đầu rồi điền dần;
//      không lượt sự kiện nào ≥ 17 ms trong lúc điền (UiProbe, release).
//   2  cachedThumbnails: ảnh đã cache vẽ ≤ 5 ms/ảnh (release), hiện lại tab không gọi nguồn. slowRender: nguồn render
//      400 ms → ảnh ≤ 1 s/tệp (release); rời tab → mọi việc đang chạy thấy hủy ≤ 50 ms (release).
//   3  search: 200 mục, mỗi phím ≤ 16 ms (release); "ket cau" khớp "Kết cấu" (tên và thư mục).
//   4  missingFiles: xóa, đổi tên → "Không tìm thấy" (kiểm ở nền khi hiện tab), bấm không mở mà báo, xóa khỏi danh sách.
//      brokenFiles: 100 tệp rác, nguồn trả ảnh rỗng/null/ném lỗi → ô giữ chỗ, trang vẫn dùng được. Corpus QA-006: Hoãn.
//   5  viewerProfile: Viewer, Viewer Pro không có Tạo mới. editionCard: tên, huy hiệu, số ngày dùng thử, nút.
//   Còn lại: recentStore, versions (mã AC10xx, DXF $ACADVER), menuActions, commands, dropFiles, keyboard, pixels (kiểm
//   điểm ảnh không có chữ thay ảnh baseline), pseudoNoTruncation (vi-XL, 1366 và 1920, lưới và bảng), idle.
#include "app/shell/start/StartPage.h"

#include "app/core/UiExecutor.h"
#include "app/core/edition/EditionBadge.h"
#include "app/core/edition/EditionGate.h"
#include "app/core/i18n/Translator.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/core/keymap/Keymap.h"
#include "app/core/perf/UiProbe.h"
#include "app/theme/ThemeManager.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/widgets/controls/Controls.h"
#include "app/widgets/message/Messages.h"
#include "app/widgets/tooltip/RichTooltip.h"
#include "core/base/settings/Settings.h"
#include "core/base/text/Fold.h"
#include "tests/timing.h"

#include <QAbstractButton>
#include <QAccessible>
#include <QApplication>
#include <QBoxLayout>
#include <QClipboard>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QProgressBar>
#include <QRandomGenerator>
#include <QScrollBar>
#include <QSet>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QtTest>

#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;
using namespace trion::theme;
using namespace trion::ui;
using trion::shell::RecentFiles;
using trion::shell::RecentFilter;
using trion::shell::StartPage;
using trion::shell::ThumbnailQueue;
using trion::test::kTimed;
namespace edition = trion::edition;
namespace i18n = trion::i18n;
namespace keymap = trion::keymap;
namespace probe = trion::ui::probe;
namespace sched = trion::sched;
namespace settings = trion::settings;
namespace text = trion::text;
using Clock = std::chrono::steady_clock;

namespace {

constexpr QRgb kThumbColor = qRgb(60, 200, 192); // ảnh của nguồn render giả
constexpr QRgb kEmbeddedColor = qRgb(200, 120, 40);

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

qint64 nowNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count();
}

int px(tok::PxId id)
{
    return qRound(ThemeManager::instance().px(id));
}

QString truncations(QWidget* root)
{
    QStringList out;
    for (const i18n::Truncation& t : i18n::findTruncations(root))
        out << t.toString();
    return out.join(u"; "_s);
}

// Nguồn ảnh giả: ảnh nhúng cho tệp có "emb" trong tên; render chậm delayMs (xét hủy mỗi 2 ms), tệp "hong…" lỗi theo
// 4 kiểu: QImage(), ảnh 0×0, ném ngoại lệ, định dạng Invalid.
struct FakeSources {
    std::atomic<int> embeddedCalls{0};
    std::atomic<int> renderCalls{0};
    std::atomic<int> cancels{0};
    std::atomic<int> delayMs{0};
    std::atomic<int> embeddedRendered{0}; // render cho tệp có ảnh nhúng: phải là 0
    std::atomic<qint64> lastCancelNs{0};
    std::mutex mutex;
    QSet<QString> rendered; // tệp đã gọi render (không trùng)

    int renderedFiles()
    {
        const std::lock_guard lock(mutex);
        return int(rendered.size());
    }

    void reset()
    {
        {
            const std::lock_guard lock(mutex);
            rendered.clear();
        }
        embeddedRendered = 0;
        embeddedCalls = 0;
        renderCalls = 0;
        cancels = 0;
        delayMs = 0;
        lastCancelNs = 0;
    }

    ThumbnailQueue::EmbeddedSource embedded()
    {
        return [this](const QString& path, sched::CancelToken) {
            ++embeddedCalls;
            if (!QFileInfo(path).fileName().contains(u"emb"))
                return QImage();
            QImage image(120, 90, QImage::Format_RGB32); // ảnh nhúng nhỏ, khác tỉ lệ 16:10
            image.fill(kEmbeddedColor);
            return image;
        };
    }

    ThumbnailQueue::RenderSource render()
    {
        return [this](const QString& path, QSize size, sched::CancelToken cancel) {
            ++renderCalls;
            {
                const std::lock_guard lock(mutex);
                rendered.insert(path);
            }
            if (QFileInfo(path).fileName().contains(u"emb"))
                ++embeddedRendered;
            const auto until = Clock::now() + std::chrono::milliseconds(delayMs.load());
            while (Clock::now() < until) {
                if (cancel.cancelled()) {
                    lastCancelNs = std::max(lastCancelNs.load(), nowNs());
                    ++cancels;
                    return QImage();
                }
                std::this_thread::sleep_for(2ms);
            }
            const QString name = QFileInfo(path).fileName();
            if (name.startsWith(u"hong")) {
                switch (name.mid(4, 3).toInt() % 4) {
                case 0:
                    return QImage();
                case 1:
                    return QImage(QSize(0, 0), QImage::Format_ARGB32);
                case 2:
                    throw std::runtime_error("tệp hỏng");
                default:
                    return QImage(size, QImage::Format_Invalid);
                }
            }
            QImage image(size, QImage::Format_ARGB32_Premultiplied);
            image.fill(kThumbColor);
            return image;
        };
    }
};

struct Seed {
    QString path;
    qint64 size = 1000;
    qint64 modified = 0; // ms từ 1970, 0: không có
    QString version = u"DWG 2018"_s;
    bool pinned = false;
};

bool writeFile(const QString& path, const QByteArray& bytes)
{
    QFile f(path);
    return f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size();
}

QByteArray dwg(const char* code)
{
    return QByteArray(code) + QByteArray(122, '\0');
}

} // namespace

class StartTest : public QObject {
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
    std::unique_ptr<edition::EditionGate> gate_;
    std::unique_ptr<RecentFiles> recent_;
    std::unique_ptr<QWidget> window_; // ô nhập trước trang (thứ tự Tab), trang
    std::unique_ptr<ToastHost> toasts_;
    QLineEdit* before_ = nullptr;
    StartPage* page_ = nullptr;
    FakeSources fake_;
    QStringList folderOpened_;
    int caseNo_ = 0;

    QString files() const { return dir_.filePath(u"files"_s); }

    // Ghi danh sách tệp gần đây vào Store như phiên trước để lại.
    void seed(const QList<Seed>& entries)
    {
        QJsonArray list;
        for (const Seed& s : entries) {
            QJsonObject o{{"path"_L1, s.path}, {"size"_L1, double(s.size)}, {"version"_L1, s.version}};
            if (s.modified)
                o.insert("modified"_L1, double(s.modified));
            if (s.pinned)
                o.insert("pinned"_L1, true);
            list.append(o);
        }
        store_->setValue(u"recent/files"_s, list);
    }

    // n tệp DWG thật (mã AC1032) tên theo name(i), ngày sửa giảm dần theo i.
    QList<Seed> makeFiles(int n, const std::function<QString(int)>& name)
    {
        QList<Seed> out;
        QDir().mkpath(files());
        for (int i = 0; i < n; ++i) {
            const QString path = files() + u'/' + name(i);
            QDir().mkpath(QFileInfo(path).path());
            writeFile(path, dwg("AC1032"));
            out << Seed{.path = path, .size = 128, .modified = 1'760'000'000'000 - qint64(i) * 60'000};
        }
        return out;
    }

    void destroyPage()
    {
        probe::disable();
        while (QWidget* p = QApplication::activePopupWidget())
            p->close();
        sched::waitIdle(); // việc nền dùng fake_, recent_
        toasts_.reset();
        window_.reset();
        page_ = nullptr;
        QCoreApplication::processEvents();
        recent_.reset();
    }

    // Trang mới (RecentFiles nạp lại từ Store) trong cửa sổ cỡ size; show = false: để test tự hiện và đo.
    void makePage(QSize size = {1366, 768}, bool show = true)
    {
        destroyPage();
        recent_ = std::make_unique<RecentFiles>(*store_);
        window_ = std::make_unique<QWidget>();
        auto* col = new QVBoxLayout(window_.get());
        col->setContentsMargins({});
        col->setSpacing(0);
        before_ = new QLineEdit;
        page_ = new StartPage(*recent_);
        page_->thumbnails().setEmbeddedSource(fake_.embedded());
        page_->thumbnails().setRenderSource(fake_.render());
        page_->setFolderOpener([this](const QString& d) {
            folderOpened_ << d;
            return true;
        });
        col->addWidget(before_);
        col->addWidget(page_, 1);
        window_->resize(size);
        toasts_ = std::make_unique<ToastHost>(window_.get(), *center_);
        toasts_->setTimeout(1h);
        if (show)
            showWindow();
    }

    void showWindow()
    {
        window_->show();
        QVERIFY(QTest::qWaitForWindowExposed(window_.get()));
        QTest::mouseMove(window_.get(), QPoint(window_->width() - 4, window_->height() - 4)); // chuột không rê thẻ
    }

    QListView* grid() const { return page_->findChild<QListView*>(u"recentGrid"_s); }
    QAbstractItemView* list() const { return page_->findChild<QAbstractItemView*>(u"recentList"_s); }
    template <class T = QWidget>
    T* child(const QString& name) const
    {
        return page_->findChild<T*>(name);
    }
    QString label(const QString& name) const { return child<QLabel>(name)->text(); }
    QMenu* menu() const { return child<QMenu>(u"startMenu"_s); }
    QAction* item(const QString& id) const
    {
        for (QAction* a : menu()->actions())
            if (a->objectName() == id)
                return a;
        return nullptr;
    }
    QString lastToast() const { return toasts_->toasts().isEmpty() ? QString() : toasts_->toasts().last()->text(); }

    // Dòng của proxy đang thấy trên lưới (như StartPage::requestThumbnails).
    QStringList visiblePaths() const
    {
        QStringList out;
        const QRect port = grid()->viewport()->rect();
        for (int r = 0; r < page_->filter().rowCount(); ++r)
            if (grid()->visualRect(page_->filter().index(r, 0)).intersects(port))
                out << page_->filter().entry(r).path;
        return out;
    }
    int loaded(const QStringList& paths) const
    {
        return int(std::count_if(paths.begin(), paths.end(), [this](const QString& p) {
            return page_->thumbnails().image(p, page_->thumbnailSize()) != nullptr;
        }));
    }
    // Mọi thẻ đang thấy có ảnh, đã lỗi, hoặc tệp không còn trên đĩa (StartPage không xin ảnh cho tệp "Không tìm thấy":
    // lần kiểm nền xong trước lần xin ảnh thì thẻ đó không bao giờ có ảnh).
    bool settled(const QStringList& paths) const
    {
        return !page_->thumbnails().busy() && std::all_of(paths.begin(), paths.end(), [this](const QString& p) {
                   return page_->thumbnails().failed(p) || page_->thumbnails().image(p, page_->thumbnailSize()) ||
                          (recent_ && recent_->entry(recent_->indexOf(p)).missing);
               });
    }
    bool refreshed() const { return recent_ && !recent_->refreshing(); }

    // Thẻ của dòng proxy row trên lưới (tọa độ viewport): ô lưới trừ khoảng cách --sp-5 bên phải, dưới.
    QRect cardRect(int row) const
    {
        const int g = px(tok::sp_5);
        return grid()->visualRect(page_->filter().index(row, 0)).adjusted(0, 0, -g, -g);
    }
    QRect thumbRect(int row) const
    {
        const QRect c = cardRect(row);
        const int pad = px(tok::sp_4), w = c.width() - 2 * pad;
        return {c.left() + pad, c.top() + pad, w, w * 10 / 16};
    }
    int proxyRow(const QString& path) const
    {
        for (int r = 0; r < page_->filter().rowCount(); ++r)
            if (page_->filter().entry(r).path == path)
                return r;
        return -1;
    }

    void openContextMenu(QAbstractItemView* view, int row)
    {
        const QPoint pos = view->visualRect(page_->filter().index(row, 0)).center();
        QContextMenuEvent e(QContextMenuEvent::Mouse, pos, view->viewport()->mapToGlobal(pos));
        QApplication::sendEvent(view->viewport(), &e);
        QVERIFY(spinUntil([&] { return menu()->isVisible(); }));
    }

    void closeMenu()
    {
        menu()->hide();
        QVERIFY(spinUntil([&] { return !menu()->isVisible(); }));
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
        keymap_ = std::make_unique<keymap::Keymap>(*store_, keymap::Platform::Windows);
        tips_ = std::make_unique<RichTooltip>();
        tips_->setKeymap(keymap_.get());
        center_ = std::make_unique<NotificationCenter>();
        gate_ = std::make_unique<edition::EditionGate>();
    }

    void init()
    {
        tm_->setTheme(Theme::Dark);
        tm_->setDensity(Density::Compact);
        gate_->setEdition(edition::Edition::Pro);
        gate_->setTrialDays(0);
        fake_.reset();
        folderOpened_.clear();
        for (const char* key : {"recent/files", "recent/limit", "start/view", "start/sort"})
            store_->remove(QString::fromLatin1(key));
        QDir(files()).removeRecursively();
        ++caseNo_;
    }

    void cleanup()
    {
        destroyPage();
        if (translator_->language() != u"vi"_s)
            QVERIFY(translator_->setLanguage(u"vi"_s));
    }

    void cleanupTestCase()
    {
        destroyPage();
        sched::waitIdle();
        gate_.reset();
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

    // Dịch vụ tệp gần đây: thứ tự mở, ghim giữ khi mở lại, lưu và nạp lại, giới hạn bỏ mục cũ nhất chưa ghim, xóa và
    // hoàn tác, thư mục gần đây không trùng, Store hỏng một mục thì bỏ mục đó.
    void recentStore()
    {
        RecentFiles r(*store_);
        QCOMPARE(r.size(), 0);
        r.add(QDir::toNativeSeparators(u"D:/DuAn/A.dwg"_s));
        r.add(u"D:/DuAn/B.dwg"_s);
        r.add(u"E:/Khac/C.dxf"_s);
        QCOMPARE(r.entry(0).name, u"C.dxf"_s);
        QCOMPARE(r.entry(2).path, u"D:/DuAn/A.dwg"_s); // dấu '/'
        r.setPinned(u"D:/DuAn/A.dwg"_s, true);
        r.add(u"D:/DuAn/A.dwg"_s); // mở lại: lên đầu, giữ ghim
        QCOMPARE(r.entry(0).name, u"A.dwg"_s);
        QVERIFY(r.entry(0).pinned);
        QCOMPARE(r.indexOf(QDir::toNativeSeparators(u"D:/DuAn/B.dwg"_s)), 2);
        QCOMPARE(r.folders(5), (QStringList{QDir::toNativeSeparators(u"D:/DuAn"_s),
                                            QDir::toNativeSeparators(u"E:/Khac"_s)}));
        sched::waitIdle();
        QCoreApplication::processEvents();
        QVERIFY(r.entry(0).missing); // không có trên đĩa

        RecentFiles again(*store_); // phiên sau
        QCOMPARE(again.size(), 3);
        QCOMPARE(again.entry(0).path, u"D:/DuAn/A.dwg"_s);
        QVERIFY(again.entry(0).pinned);
        QVERIFY(again.entry(0).missing);

        store_->setValue(u"recent/limit"_s, 3);
        r.add(u"F:/D.dwg"_s);
        QCOMPARE(r.size(), 3);
        QCOMPARE(r.indexOf(u"D:/DuAn/A.dwg"_s), 1); // ghim: ở lại
        QCOMPARE(r.indexOf(u"D:/DuAn/B.dwg"_s), -1); // cũ nhất chưa ghim: bỏ
        const RecentFiles::Entry c = r.entry(2);
        QVERIFY(r.remove(c.path));
        QVERIFY(!r.remove(c.path));
        r.restore(2, c);
        QCOMPARE(r.entry(2).path, c.path);
        r.restore(0, c); // đã có: bỏ qua
        QCOMPARE(r.size(), 3);
        sched::waitIdle();
        QCoreApplication::processEvents();

        QJsonArray bad{QJsonObject{{"path"_L1, u"G:/x.dwg"_s}}, 42, QJsonObject{{"size"_L1, 1}},
                       QJsonObject{{"path"_L1, u"G:/x.dwg"_s}}};
        store_->setValue(u"recent/files"_s, bad);
        RecentFiles broken(*store_);
        QCOMPARE(broken.size(), 1);
    }

    // Phiên bản từ mã đầu tệp (bảng AutoCAD), DXF dạng chữ qua $ACADVER, DXF nhị phân, tệp rác.
    void versions()
    {
        QCOMPARE(RecentFiles::versionName("AC1032", false), u"DWG 2018"_s);
        QCOMPARE(RecentFiles::versionName("AC1027", false), u"DWG 2013"_s);
        QCOMPARE(RecentFiles::versionName("AC1024xx", false), u"DWG 2010"_s);
        QCOMPARE(RecentFiles::versionName("AC1015", false), u"DWG 2000"_s);
        QCOMPARE(RecentFiles::versionName("AC1014", false), u"DWG R14"_s);
        QCOMPARE(RecentFiles::versionName("AC1009", false), u"DWG R12"_s);
        QCOMPARE(RecentFiles::versionName("AC1018", true), u"DXF 2004"_s);
        QCOMPARE(RecentFiles::versionName("AC1040", false), u"AC1040"_s); // mới hơn bảng
        QCOMPARE(RecentFiles::versionName("AC10", false), QString());
        QCOMPARE(RecentFiles::versionName("ACAD10", false), QString());
        QCOMPARE(RecentFiles::versionName("MZ\x90\x00\x03\x00", false), QString());
        QDir().mkpath(files());
        const QString a = files() + u"/a.dwt"_s, b = files() + u"/b.DXF"_s, c = files() + u"/c.dxf"_s,
                      d = files() + u"/d.dwg"_s;
        QVERIFY(writeFile(a, dwg("AC1021")));
        QVERIFY(writeFile(b, "  0\r\nSECTION\r\n  2\r\nHEADER\r\n  9\r\n$ACADVER\r\n  1\r\nAC1027\r\n  9\r\n"));
        QVERIFY(writeFile(c, QByteArray("AutoCAD Binary DXF\r\n\x1a", 21) + QByteArray(64, '\x01')));
        QVERIFY(writeFile(d, "garbage"));
        QCOMPARE(RecentFiles::readVersion(a), u"DWG 2007"_s);
        QCOMPARE(RecentFiles::readVersion(b), u"DXF 2013"_s);
        QCOMPARE(RecentFiles::readVersion(c), u"DXF"_s);
        QCOMPARE(RecentFiles::readVersion(d), QString());
        QCOMPARE(RecentFiles::readVersion(files() + u"/khong-co.dwg"_s), QString());
    }

    // Tiêu chí 1: 200 tệp gần đây → trang có nội dung ≤ 150 ms (khởi động ấm), ảnh điền dần sau khung đầu, không lượt
    // sự kiện nào ≥ 17 ms lúc điền. Tệp trên đĩa đã sửa sau phiên trước, tệp sau mới hơn (như NTFS giữ giờ sửa từng
    // tệp): lần kiểm nền đầu đảo thứ tự lưới; chỉ thẻ thấy trước và sau lần đảo được xin ảnh, không thẻ thấy thoáng
    // qua trong lúc áp kết quả kiểm.
    void startupContent()
    {
        const QList<Seed> seeds = makeFiles(200, [](int i) { return u"Ban-ve-%1.dwg"_s.arg(i, 3, 10, QChar(u'0')); });
        for (qsizetype i = 0; i < seeds.size(); ++i) {
            QFile f(seeds[i].path);
            QVERIFY(f.open(QIODevice::ReadWrite));
            QVERIFY(f.setFileTime(QDateTime::fromMSecsSinceEpoch(1'770'000'000'000 + i * 1000, QTimeZone::UTC),
                                  QFileDevice::FileModificationTime));
        }
        seed(seeds);
        fake_.delayMs = 300; // ảnh về sau khung đầu
        makePage(); // ấm: font, biểu tượng, style đã nạp như sau khởi động
        QVERIFY(spinUntil([&] { return refreshed(); }));
        destroyPage();

        const auto start = Clock::now();
        makePage({1366, 768}, false);
        int paints = 0; // lần vẽ lưới đã có thẻ (bố trí xong)
        struct Counter final : QObject {
            int* n;
            QListView* view;
            Counter(int* p, QListView* v) : n(p), view(v) {}
            bool eventFilter(QObject*, QEvent* e) override
            {
                if (e->type() == QEvent::Paint && view->model()->rowCount() > 0
                    && view->visualRect(view->model()->index(0, 0)).isValid())
                    ++*n;
                return false;
            }
        } counter(&paints, grid());
        grid()->viewport()->installEventFilter(&counter);
        showWindow();
        QVERIFY(spinUntil([&] { return paints > 0; }));
        const double content = ms(Clock::now() - start);
        grid()->viewport()->removeEventFilter(&counter);
        QCOMPARE(page_->filter().rowCount(), 200);
        const QStringList shown = visiblePaths();
        qInfo("lưới %dx%d, ô %dx%d", grid()->viewport()->width(), grid()->viewport()->height(), grid()->gridSize().width(),
              grid()->gridSize().height());
        QVERIFY(shown.size() >= 4);
        QVERIFY(shown.size() < 200); // ảo hóa: chỉ thẻ đang thấy xin ảnh
        const int atFirstFrame = loaded(shown);

        probe::enable();
        probe::setStallThreshold(17ms);
        probe::reset();
        QSignalSpy ready(&page_->thumbnails(), &ThumbnailQueue::ready);
        QVERIFY(spinUntil([&] { return settled(visiblePaths()); }, 20s));
        const auto stalls = probe::stalls();
        probe::disable();
        double worst = 0;
        for (const auto& s : stalls)
            worst = std::max(worst, ms(s.duration));
        qInfo("nội dung sau %.1f ms (200 tệp); %lld thẻ đang thấy, %d có ảnh ở khung đầu, %lld ảnh về sau; %zu lượt sự "
              "kiện ≥ 17 ms, dài nhất %.1f ms; render %d lần, %d tệp",
              content, qint64(shown.size()), atFirstFrame, qint64(ready.count()), stalls.size(), worst,
              fake_.renderCalls.load(), fake_.renderedFiles());
        QCOMPARE(atFirstFrame, 0); // trang hiện trước, ảnh điền sau
        QCOMPARE(loaded(visiblePaths()), visiblePaths().size());
        QVERIFY(fake_.renderCalls.load() < 200);
        QVERIFY2(fake_.renderedFiles() <= 3 * shown.size(), qPrintable(QString::number(fake_.renderedFiles())));
        QVERIFY2(!kTimed || content <= 150, qPrintable(u"%1 ms"_s.arg(content)));
        QVERIFY2(!kTimed || stalls.empty(), stalls.empty() ? "" : stalls.front().where.c_str());
    }

    // Tiêu chí 2: ảnh đã cache vẽ ≤ 5 ms/ảnh; rời tab rồi quay lại không gọi nguồn lần nữa. Ảnh nhúng dùng trước render.
    void cachedThumbnails()
    {
        seed(makeFiles(40, [](int i) { return (i % 2 ? u"emb-%1.dwg"_s : u"rnd-%1.dwg"_s).arg(i); }));
        makePage();
        QVERIFY(spinUntil([&] { return settled(visiblePaths()) && loaded(visiblePaths()) > 0; }));
        const QStringList shown = visiblePaths();
        QCOMPARE(loaded(shown), shown.size());
        const int renders = fake_.renderCalls, embeds = fake_.embeddedCalls;
        QVERIFY(embeds >= shown.size()); // mọi tệp hỏi ảnh nhúng trước
        QCOMPARE(fake_.embeddedRendered.load(), 0); // có ảnh nhúng thì không render
        QVERIFY(renders > 0);

        QImage sheet(grid()->viewport()->size(), QImage::Format_ARGB32_Premultiplied);
        sheet.fill(Qt::black);
        QElapsedTimer t;
        t.start();
        constexpr int kRounds = 10;
        for (int i = 0; i < kRounds; ++i)
            grid()->viewport()->render(&sheet);
        const double perImage = double(t.nsecsElapsed()) / 1e6 / kRounds / shown.size();

        page_->hide(); // rời tab
        page_->show(); // quay lại
        QVERIFY(spinUntil([&] { return settled(visiblePaths()); }));
        QCOMPARE(fake_.renderCalls.load(), renders);
        QCOMPARE(fake_.embeddedCalls.load(), embeds);
        qInfo("vẽ %lld thẻ có ảnh trong cache: %.3f ms/ảnh", qint64(shown.size()), perImage);
        QVERIFY2(!kTimed || perImage <= 5, qPrintable(u"%1 ms/ảnh"_s.arg(perImage)));
    }

    // Tiêu chí 2: nguồn render chậm 400 ms → ảnh trong ≤ 1 s mỗi tệp; rời tab → việc đang chạy thấy hủy ≤ 50 ms.
    void slowRender()
    {
        seed(makeFiles(1, [](int) { return u"Mot.dwg"_s; }));
        fake_.delayMs = 400;
        makePage({1366, 768}, false);
        QSignalSpy ready(&page_->thumbnails(), &ThumbnailQueue::ready);
        const auto shownAt = Clock::now();
        showWindow();
        QVERIFY(spinUntil([&] { return ready.count() == 1; }, 5s));
        const double took = ms(Clock::now() - shownAt);
        QCOMPARE(loaded(visiblePaths()), 1);

        seed(makeFiles(60, [](int i) { return u"Cham-%1.dwg"_s.arg(i); }));
        destroyPage();
        fake_.reset();
        fake_.delayMs = 5000;
        makePage();
        QVERIFY(spinUntil([&] { return fake_.renderCalls.load() >= 2; }));
        const int running = page_->thumbnails().running();
        const qint64 hiddenAt = nowNs();
        page_->hide();
        QCOMPARE(page_->thumbnails().running(), 0); // hủy ngay trong lượt gọi
        sched::waitIdle();
        const double drained = double(nowNs() - hiddenAt) / 1e6;
        const double cancelled = double(fake_.lastCancelNs.load() - hiddenAt) / 1e6;
        qInfo("ảnh 1 tệp sau %.0f ms (nguồn 400 ms); rời tab: %d việc chờ, %d việc đang render thấy hủy sau %.1f ms, "
              "hàng đợi trống sau %.1f ms",
              took, running, fake_.cancels.load(), cancelled, drained);
        QCOMPARE(fake_.cancels.load(), fake_.renderCalls.load());
        QVERIFY(took >= 380);
        QVERIFY2(!kTimed || took <= 1000, qPrintable(u"%1 ms"_s.arg(took)));
        QVERIFY(drained < 2500); // nguồn 5 s: dừng nhờ hủy
        QVERIFY2(!kTimed || (cancelled <= 50 && drained <= 50), qPrintable(u"%1 / %2 ms"_s.arg(cancelled).arg(drained)));
    }

    // Tiêu chí 3: 200 mục, gõ "ket cau" (không dấu) lọc mỗi phím ≤ 16 ms, khớp "Kết cấu" ở tên và ở thư mục; ở bảng
    // cũng vậy; xóa chữ thì đủ lại.
    void search()
    {
        QList<Seed> seeds;
        for (int i = 0; i < 200; ++i) {
            const QString name = i % 10 == 0   ? u"Kết cấu móng %1.dwg"_s.arg(i)
                                 : i % 10 == 1 ? u"Kết cấu/Dầm %1.dwg"_s.arg(i) // khớp ở thư mục
                                 : i % 10 == 2 ? u"KẾT CẤU THÉP %1.dxf"_s.arg(i)
                                               : u"Kiến trúc tầng %1.dwg"_s.arg(i);
            seeds << Seed{.path = u"D:/DuAn/"_s + name, .modified = 1'760'000'000'000 - i * 1000LL};
        }
        seed(seeds);
        makePage();
        QVERIFY(spinUntil([&] { return refreshed(); }));
        auto* field = child<QLineEdit>(u"recentSearch"_s);
        window_->activateWindow();
        field->setFocus(Qt::OtherFocusReason);
        const QString needle = text::foldForSearch(u"ket cau"_s);
        for (QAbstractItemView* view : {static_cast<QAbstractItemView*>(grid()), list()}) {
            page_->setView(view == grid() ? StartPage::View::Grid : StartPage::View::List);
            QCoreApplication::processEvents(); // lần vẽ đầu của kiểu xem không tính vào phím
            double worst = 0, worstCpu = 0; // ngưỡng theo CPU luồng UI: thời gian thật trượt khi máy tải
            for (const QChar c : u"ket cau"_s) {
                const auto t = Clock::now();
                const auto cpu = trion::test::threadCpu();
                QTest::keyClick(field, c.toLatin1());
                QCoreApplication::processEvents();
                view->visualRect(page_->filter().index(0, 0)); // bố trí xong
                worst = std::max(worst, ms(Clock::now() - t));
                worstCpu = std::max(worstCpu, std::chrono::duration<double, std::milli>(trion::test::threadCpu() - cpu).count());
            }
            QCOMPARE(field->text(), u"ket cau"_s);
            QCOMPARE(page_->filter().rowCount(), 60);
            for (int r = 0; r < page_->filter().rowCount(); ++r)
                QVERIFY(page_->filter().entry(r).folded.contains(needle));
            qInfo("%s: 200 mục, mỗi phím ≤ %.2f ms (CPU luồng UI ≤ %.2f ms)", view == grid() ? "lưới" : "bảng", worst,
                  worstCpu);
            QVERIFY2(!kTimed || trion::test::threadCpu().count() < 0 || worstCpu <= 16,
                     qPrintable(u"CPU %1 ms"_s.arg(worstCpu)));
            field->clear();
            QCOMPARE(page_->filter().rowCount(), 200);
        }
        field->setText(u"không có tệp này"_s); // i18n-ok: chữ gõ thử
        QCOMPARE(page_->filter().rowCount(), 0);
        QVERIFY(child(u"recentEmpty"_s)->isVisible());
        const auto labels = child(u"recentEmpty"_s)->findChildren<QLabel*>();
        QVERIFY(std::any_of(labels.begin(), labels.end(), [](QLabel* l) { return l->text().contains(u"không có tệp này"); }));
        QTest::keyClick(field, Qt::Key_Escape); // Esc xóa chữ của ô tìm
        QCOMPARE(page_->filter().rowCount(), 200);
    }

    // Sắp: ghim lên đầu; ngày sửa mới trước, tên A→Z không phân biệt dấu, kích thước lớn trước; nhớ cách sắp, kiểu xem.
    void sorting()
    {
        seed({Seed{.path = u"D:/b/Ân.dwg"_s, .size = 10, .modified = 3000},
              Seed{.path = u"D:/a/Bình.dwg"_s, .size = 30, .modified = 1000},
              Seed{.path = u"D:/c/an-2.dwg"_s, .size = 20, .modified = 2000},
              Seed{.path = u"D:/d/Zeta.dwg"_s, .size = 5, .modified = 500, .pinned = true}});
        makePage();
        const auto names = [&] {
            QStringList out;
            for (int r = 0; r < page_->filter().rowCount(); ++r)
                out << page_->filter().entry(r).name;
            return out;
        };
        QCOMPARE(names(), (QStringList{u"Zeta.dwg"_s, u"Ân.dwg"_s, u"an-2.dwg"_s, u"Bình.dwg"_s}));
        auto* sort = child<QComboBox>(u"recentSort"_s);
        sort->setCurrentIndex(1);
        QCOMPARE(names(), (QStringList{u"Zeta.dwg"_s, u"an-2.dwg"_s, u"Ân.dwg"_s, u"Bình.dwg"_s}));
        sort->setCurrentIndex(2);
        QCOMPARE(names(), (QStringList{u"Zeta.dwg"_s, u"Bình.dwg"_s, u"an-2.dwg"_s, u"Ân.dwg"_s}));
        recent_->setPinned(u"D:/d/Zeta.dwg"_s, false);
        QCOMPARE(names().constLast(), u"Zeta.dwg"_s);
        page_->setView(StartPage::View::List);
        makePage(); // phiên sau: cách sắp và kiểu xem giữ lại
        QCOMPARE(child<QComboBox>(u"recentSort"_s)->currentIndex(), 2);
        QCOMPARE(page_->view(), StartPage::View::List);
        QCOMPARE(page_->recentView(), list());
        QVERIFY(list()->isVisible());
    }

    // Tiêu chí 4: tệp đã xóa hoặc đổi tên hiện "Không tìm thấy" sau lần kiểm nền khi hiện tab; bấm không mở mà báo kèm
    // nút xóa khỏi danh sách. Phiên bản, cỡ, ngày đọc từ tệp.
    void missingFiles()
    {
        QDir().mkpath(files());
        const QString a = files() + u"/Mat-bang.dwg"_s, b = files() + u"/Ket-cau.dxf"_s, c = files() + u"/Dien.dwg"_s;
        QVERIFY(writeFile(a, dwg("AC1032")));
        QVERIFY(writeFile(b, "  0\nSECTION\n  2\nHEADER\n  9\n$ACADVER\n  1\nAC1027\n"));
        QVERIFY(writeFile(c, dwg("AC1024")));
        seed({Seed{.path = a, .version = {}}, Seed{.path = b, .version = {}}, Seed{.path = c, .version = {}}});
        makePage();
        QVERIFY(spinUntil([&] { return refreshed(); }));
        const auto entry = [&](const QString& p) { return recent_->entry(recent_->indexOf(p)); };
        QCOMPARE(entry(a).version, u"DWG 2018"_s);
        QCOMPARE(entry(b).version, u"DXF 2013"_s);
        QCOMPARE(entry(c).version, u"DWG 2010"_s);
        QCOMPARE(entry(a).size, 128);
        QVERIFY(entry(a).modified.isValid());
        QVERIFY(!entry(a).missing && !entry(b).missing);

        QVERIFY(QFile::remove(a));
        QVERIFY(QFile::rename(b, files() + u"/Ket-cau-moi.dxf"_s));
        page_->hide(); // sang tab bản vẽ rồi quay lại
        page_->show();
        QVERIFY(spinUntil([&] { return refreshed() && entry(a).missing && entry(b).missing; }));
        QVERIFY(!entry(c).missing);
        QCOMPARE(entry(a).version, u"DWG 2018"_s); // giữ số liệu biết lần cuối
        const QString notFound = RecentFiles::tr("Không tìm thấy");
        const QModelIndex ia = recent_->index(recent_->indexOf(a), RecentFiles::Name);
        QVERIFY(ia.data(Qt::AccessibleTextRole).toString().contains(notFound));
        QVERIFY(!recent_->index(recent_->indexOf(c), 0).data(Qt::AccessibleTextRole).toString().contains(notFound));
        QVERIFY(ia.data(RecentFiles::MissingRole).toBool());

        QSignalSpy open(page_, &StartPage::openRequested);
        const int row = proxyRow(a);
        QTest::mouseClick(grid()->viewport(), Qt::LeftButton, {}, thumbRect(row).center());
        QCOMPARE(open.count(), 0);
        QCOMPARE(toasts_->toasts().last()->severity(), Severity::Warning);
        QVERIFY(lastToast().contains(u"Mat-bang.dwg"_s));
        QVERIFY(toasts_->toasts().last()->actionButton());
        toasts_->toasts().last()->actionButton()->click(); // Xóa khỏi danh sách
        QCOMPARE(recent_->indexOf(a), -1);
        QTest::mouseClick(grid()->viewport(), Qt::LeftButton, {}, thumbRect(proxyRow(c)).center());
        QCOMPARE(open.count(), 1);
        QCOMPARE(open[0][0].toStringList(), QStringList{c});
    }

    // Tiêu chí 4: 100 tệp rác (không phải DWG), nguồn ảnh trả QImage(), ảnh 0×0, ném lỗi, định dạng Invalid: thẻ giữ ô
    // giữ chỗ, mỗi tệp chỉ thử một lần, trang vẫn lọc, cuộn được. Corpus QA-006 thật: Hoãn (cần QA-006, DWG-026).
    void brokenFiles()
    {
        QDir().mkpath(files());
        QRandomGenerator rng(4);
        QList<Seed> seeds;
        for (int i = 0; i < 100; ++i) {
            const QString path = files() + u"/hong%1.dwg"_s.arg(i, 3, 10, QChar(u'0'));
            QByteArray junk(64 + int(rng.bounded(4000)), Qt::Uninitialized);
            for (char& ch : junk)
                ch = char(rng.bounded(256));
            junk[0] = 'X'; // không phải mã AC10xx
            QVERIFY(writeFile(path, junk));
            seeds << Seed{.path = path, .version = {}};
        }
        seed(seeds);
        makePage();
        QVERIFY(spinUntil([&] { return refreshed(); }));
        QScrollBar* bar = grid()->verticalScrollBar();
        QSet<QString> seen;
        for (;;) { // cuộn hết lưới: mỗi trang thẻ chờ ảnh xong (lỗi) rồi cuộn tiếp
            QVERIFY(spinUntil([&] { return settled(visiblePaths()); }));
            for (const QString& p : visiblePaths())
                seen.insert(p);
            if (bar->value() >= bar->maximum())
                break;
            bar->setValue(std::min(bar->maximum(), bar->value() + grid()->viewport()->height()));
        }
        QCOMPARE(seen.size(), 100);
        for (const QString& p : seen) {
            QVERIFY2(page_->thumbnails().failed(p), qPrintable(p));
            QVERIFY(!page_->thumbnails().image(p, page_->thumbnailSize()));
        }
        for (int i = 0; i < recent_->size(); ++i) {
            QVERIFY(recent_->entry(i).version.isEmpty());
            QVERIFY(!recent_->entry(i).missing);
        }
        const int calls = fake_.renderCalls;
        QVERIFY(calls >= 100);
        bar->setValue(0); // cuộn lại, rời tab rồi quay lại: tệp lỗi không thử lại
        page_->hide();
        page_->show();
        QVERIFY(spinUntil([&] { return settled(visiblePaths()); }));
        sched::waitIdle();
        QCOMPARE(fake_.renderCalls.load(), calls);
        grid()->viewport()->repaint(); // vẽ ô giữ chỗ không lỗi
        child<QLineEdit>(u"recentSearch"_s)->setText(u"hong05"_s); // i18n-ok: chữ gõ thử
        QCOMPARE(page_->filter().rowCount(), 10);
    }

    // Tiêu chí 5: Viewer, Viewer Pro không có Tạo mới (07.1 §2, đổi phiên bản lúc chạy); Pro, Dùng thử có.
    void viewerProfile()
    {
        gate_->setEdition(edition::Edition::Viewer);
        makePage();
        QVERIFY(!child(u"createSection"_s)->isVisible());
        QVERIFY(!child(u"template0"_s)->isVisible());
        QVERIFY(child(u"openFile"_s)->isVisible());
        gate_->setEdition(edition::Edition::ViewerPro);
        QVERIFY(!child(u"createSection"_s)->isVisible());
        gate_->setEdition(edition::Edition::Trial);
        QVERIFY(child(u"createSection"_s)->isVisible());
        gate_->setEdition(edition::Edition::Pro);
        QVERIFY(child(u"template7"_s)->isVisible());
        gate_->setEdition(edition::Edition::Viewer);
        QVERIFY(!child(u"browseTemplates"_s)->isVisible());
    }

    // Tiêu chí 5: thẻ phiên bản theo phiên bản (tên; huy hiệu edition::EditionBadge chung với thanh tiêu đề, cả cảnh
    // báo offline), số ngày dùng thử (PLF-002 qua EditionGate::setTrialDays), hạn thuê bao; nút: Dùng thử/Mua mở trang
    // nâng cấp, Trạng thái giấy phép → screenRequested.
    void editionCard()
    {
        QList<edition::UpgradeRequest> upgrades;
        gate_->upgrade().setHandler([&](const edition::UpgradeRequest& r) { upgrades << r; });
        makePage();
        QSignalSpy screens(page_, &StartPage::screenRequested);
        auto* action = child<QAbstractButton>(u"editionAction"_s);
        auto* bar = child<QProgressBar>(u"trialProgress"_s);

        gate_->setEdition(edition::Edition::Trial);
        gate_->setTrialDays(12);
        QCOMPARE(label(u"editionName"_s), StartPage::tr("TrionCAD Pro"));
        QVERIFY(qobject_cast<edition::EditionBadge*>(child(u"editionBadge"_s)));
        QCOMPARE(label(u"editionBadge"_s), edition::EditionBadge::label(*gate_));
        QVERIFY(label(u"editionBadge"_s).contains(u"12"_s));
        // .hrow: đủ chỗ thì tên và huy hiệu cùng dòng, căn giữa dọc, huy hiệu sát phải thẻ.
        const auto sameRow = [this] {
            const QRect n = child(u"editionName"_s)->geometry(), b = child(u"editionBadge"_s)->geometry();
            return std::abs(n.center().y() - b.center().y()) <= 1 && b.left() > n.left() &&
                   b.right() == child(u"editionAction"_s)->parentWidget()->contentsRect().right() - px(tok::sp_5);
        };
        QTRY_VERIFY(sameRow());
        QVERIFY(label(u"editionText"_s).contains(u"12"_s));
        QVERIFY(bar->isVisible());
        QCOMPARE(bar->value(), 12);
        QCOMPARE(action->text(), StartPage::tr("Mua TrionCAD Pro"));
        action->click();
        QCOMPARE(upgrades.size(), 1);
        QCOMPARE(upgrades[0].feature, u"buy"_s);
        gate_->setTrialDays(3);
        QVERIFY(label(u"editionBadge"_s).contains(u"3"_s));
        QVERIFY(label(u"editionText"_s).contains(u"3"_s));
        QCOMPARE(bar->value(), 3);
        QCOMPARE(bar->accessibleName(), label(u"editionBadge"_s));
        gate_->setOfflineDays(5); // lâu chưa xác minh giấy phép: cảnh báo như thanh tiêu đề
        QCOMPARE(label(u"editionBadge"_s), edition::EditionBadge::label(*gate_));
        QVERIFY(label(u"editionBadge"_s).contains(u"5"_s));
        gate_->setOfflineDays(-1);

        gate_->setEdition(edition::Edition::Viewer);
        QCOMPARE(label(u"editionName"_s), StartPage::tr("TrionCAD Viewer"));
        QCOMPARE(label(u"editionBadge"_s), edition::EditionBadge::tr("Viewer"));
        QVERIFY(!bar->isVisible());
        QCOMPARE(action->text(), StartPage::tr("Dùng thử Pro 30 ngày"));
        action->click();
        QCOMPARE(upgrades.size(), 2);
        QCOMPARE(upgrades[1].feature, u"trial"_s);
        QCOMPARE(upgrades[1].need, Edition::Pro);

        gate_->setEdition(edition::Edition::ViewerPro);
        QCOMPARE(label(u"editionName"_s), StartPage::tr("TrionCAD Viewer Pro"));
        QCOMPARE(label(u"editionBadge"_s), edition::EditionBadge::tr("Viewer Pro"));

        gate_->setEdition(edition::Edition::Education);
        QCOMPARE(label(u"editionName"_s), StartPage::tr("TrionCAD Giáo dục"));
        gate_->setEdition(edition::Edition::Enterprise);
        QCOMPARE(label(u"editionBadge"_s), edition::EditionBadge::tr("Doanh nghiệp"));
        gate_->setEdition(edition::Edition::Pro);
        QCOMPARE(label(u"editionBadge"_s), edition::EditionBadge::tr("Pro"));
        QVERIFY(!child(u"editionText"_s)->isVisible()); // chưa có hạn
        page_->setExpiry(QDate(2027, 10, 18));
        QVERIFY(label(u"editionText"_s).contains(u"18/10/2027"_s));
        QCOMPARE(action->text(), StartPage::tr("Trạng thái giấy phép"));
        action->click();
        QCOMPARE(screens.count(), 1);
        QCOMPARE(screens[0][0].toString(), u"license"_s);
        QCOMPARE(upgrades.size(), 2);
        gate_->upgrade().setHandler({});
    }

    // Lệnh: mẫu → NEW với tệp mẫu, Mẫu của tôi → QNEW, Duyệt mẫu… → NEW, Mở tệp… → OPEN, Phục hồi → RECOVER; thư mục
    // gần đây → browseRequested; Học tập, Thông báo, trạng thái rỗng → screenRequested.
    void commands()
    {
        makePage();
        QSignalSpy macros(page_, &StartPage::macroRequested);
        QSignalSpy screens(page_, &StartPage::screenRequested);
        QSignalSpy browse(page_, &StartPage::browseRequested);
        for (int i = 0; i < 8; ++i)
            child<QAbstractButton>(u"template%1"_s.arg(i))->click();
        child<QAbstractButton>(u"browseTemplates"_s)->click();
        child<QAbstractButton>(u"openFile"_s)->click();
        child<QAbstractButton>(u"recover"_s)->click();
        QStringList got;
        for (const auto& m : macros)
            got << m[0].toString();
        QCOMPARE(got, (QStringList{u"^C^C_NEW TCVN-A3-ngang.dwt"_s, u"^C^C_NEW TCVN-A3-doc.dwt"_s,
                                   u"^C^C_NEW TCVN-A1-ngang.dwt"_s, u"^C^C_NEW TCVN-A2-ngang.dwt"_s,
                                   u"^C^C_NEW TCVN-A4-doc.dwt"_s, u"^C^C_NEW Trong-mm.dwt"_s,
                                   u"^C^C_NEW Trong-inch.dwt"_s, u"^C^C_QNEW"_s, u"^C^C_NEW"_s, u"^C^C_OPEN"_s,
                                   u"^C^C_RECOVER"_s}));
        QCOMPARE(RichTooltip::info(child(u"openFile"_s)).command, u"OPEN"_s);
        QCOMPARE(RichTooltip::info(child(u"template0"_s)).command, u"NEW"_s);

        // Trống: chưa có thư mục gần đây, trạng thái rỗng có hai nút.
        auto* folders = child<QAbstractButton>(u"recentFolders"_s);
        QVERIFY(!folders->isEnabled());
        QVERIFY(child(u"recentEmpty"_s)->isVisible());
        child<EmptyState>(u"recentEmpty"_s)->actionButton()->click();
        child<QAbstractButton>(u"emptyVideo"_s)->click();
        for (int i = 0; i < 3; ++i)
            child<QAbstractButton>(u"lesson%1"_s.arg(i))->click();
        QVERIFY(!child(u"news"_s)->isVisible());
        page_->setNotifications({{u"update"_s, u"TrionCAD Viewer 1.0.2 đã có"_s, u"14/10/2026"_s, u"update"_s}, // i18n-ok: dữ liệu mẫu
                                 {u"forum"_s, u"Bộ khung tên TCVN 5571"_s, u"Cộng đồng"_s, u"https://trioncad.vn/a"_s}}); // i18n-ok: dữ liệu mẫu
        QVERIFY(child(u"news"_s)->isVisible());
        const auto news = child(u"news"_s)->findChildren<QAbstractButton*>();
        QCOMPARE(news.size(), 2);
        news[1]->click();
        QStringList ids;
        for (const auto& s : screens)
            ids << s[0].toString();
        QCOMPARE(ids, (QStringList{u"sample"_s, u"video-intro"_s, u"migrate"_s, u"video-measure"_s,
                                   u"video-fontdoctor"_s, u"https://trioncad.vn/a"_s}));
        page_->setNotifications({});
        QVERIFY(!child(u"news"_s)->isVisible());

        recent_->add(u"D:/DuAn/A&B/x.dwg"_s);
        recent_->add(u"D:/DuAn/A&B/y.dwg"_s);
        recent_->add(u"E:/Share/z.dwg"_s);
        QVERIFY(folders->isEnabled());
        folders->click();
        QVERIFY(spinUntil([&] { return menu()->isVisible(); }));
        QCOMPARE(menu()->actions().size(), 2);
        QCOMPARE(menu()->actions()[1]->text(), QDir::toNativeSeparators(u"D:/DuAn/A&&B"_s));
        menu()->actions()[1]->trigger();
        QCOMPARE(browse.count(), 1);
        QCOMPARE(browse[0][0].toString(), QDir::toNativeSeparators(u"D:/DuAn/A&B"_s));
        closeMenu();
    }

    // Menu chuột phải: Mở, Mở chỉ đọc, Mở thư mục chứa, Sao chép đường dẫn | Ghim/Bỏ ghim, Xóa khỏi danh sách (toast có
    // Hoàn tác). Bấm ghim ở góc thẻ, ở cột ghim của bảng.
    void menuActions()
    {
        const QList<Seed> seeds = makeFiles(3, [](int i) { return u"T%1.dwg"_s.arg(i); });
        seed(seeds);
        makePage();
        QVERIFY(spinUntil([&] { return refreshed(); }));
        QSignalSpy open(page_, &StartPage::openRequested);
        QSignalSpy readOnly(page_, &StartPage::openReadOnlyRequested);
        const QString p1 = seeds[1].path;

        openContextMenu(grid(), proxyRow(p1));
        QStringList ids;
        for (QAction* a : menu()->actions())
            if (!a->isSeparator())
                ids << a->objectName();
        QCOMPARE(ids, (QStringList{u"_open"_s, u"_openReadOnly"_s, u"_openFolder"_s, u"_copyPath"_s, u"_pin"_s,
                                   u"_remove"_s}));
        QCOMPARE(item(u"_pin"_s)->text(), StartPage::tr("Ghim"));
        item(u"_open"_s)->trigger();
        closeMenu();
        QCOMPARE(open.count(), 1);
        QCOMPARE(open[0][0].toStringList(), QStringList{p1});
        openContextMenu(grid(), proxyRow(p1));
        item(u"_openReadOnly"_s)->trigger();
        item(u"_openFolder"_s)->trigger();
        item(u"_copyPath"_s)->trigger();
        closeMenu();
        QCOMPARE(readOnly.count(), 1);
        QCOMPARE(readOnly[0][0].toString(), p1);
        QCOMPARE(folderOpened_, QStringList{files()});
        QCOMPARE(QGuiApplication::clipboard()->text(), QDir::toNativeSeparators(p1));
        QVERIFY(lastToast().contains(QDir::toNativeSeparators(p1)));

        openContextMenu(grid(), proxyRow(p1));
        item(u"_pin"_s)->trigger();
        closeMenu();
        QVERIFY(recent_->entry(recent_->indexOf(p1)).pinned);
        QCOMPARE(proxyRow(p1), 0); // ghim lên đầu
        openContextMenu(grid(), 0);
        QCOMPARE(item(u"_pin"_s)->text(), StartPage::tr("Bỏ ghim"));
        item(u"_remove"_s)->trigger();
        closeMenu();
        QCOMPARE(recent_->indexOf(p1), -1);
        QCOMPARE(toasts_->toasts().last()->severity(), Severity::Ok);
        toasts_->toasts().last()->actionButton()->click(); // Hoàn tác
        QCOMPARE(recent_->indexOf(p1), 1);
        QVERIFY(recent_->entry(1).pinned);

        // Nút ghim ở góc thẻ: không mở tệp.
        const int row2 = proxyRow(seeds[2].path);
        const QRect card = cardRect(row2);
        QTest::mouseMove(grid()->viewport(), card.center());
        const QPoint pin(card.right() - 12 - 11, card.top() + 12 + 11);
        QTest::mouseClick(grid()->viewport(), Qt::LeftButton, {}, pin);
        QVERIFY(recent_->entry(recent_->indexOf(seeds[2].path)).pinned);
        QCOMPARE(open.count(), 1);
        // Cột ghim của bảng.
        page_->setView(StartPage::View::List);
        const int r = proxyRow(seeds[2].path);
        const QRect pinCell = list()->visualRect(page_->filter().index(r, RecentFiles::Pin));
        QTest::mouseClick(list()->viewport(), Qt::LeftButton, {}, pinCell.center());
        QVERIFY(!recent_->entry(recent_->indexOf(seeds[2].path)).pinned);
        QCOMPARE(open.count(), 1);
        const QRect nameCell = list()->visualRect(page_->filter().index(0, RecentFiles::Name));
        QTest::mouseClick(list()->viewport(), Qt::LeftButton, {}, nameCell.center());
        QCOMPARE(open.count(), 2);
    }

    // §9.11: thả 5 tệp DWG/DXF/DWT → openRequested đúng thứ tự; tệp khác bị từ chối kèm toast lỗi; gợi ý thả hiện khi
    // kéo vào, ẩn khi ra; URL web không nhận.
    void dropFiles()
    {
        makePage();
        QSignalSpy open(page_, &StartPage::openRequested);
        auto* hint = child<QLabel>(u"dropHint"_s);
        const auto drop = [&](const QList<QUrl>& urls, bool leave = false) {
            QMimeData mime;
            mime.setUrls(urls);
            const QPoint pos = page_->rect().center();
            QDragEnterEvent enter(pos, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(page_, &enter);
            if (!enter.isAccepted())
                return false;
            if (!hint->isVisible())
                return false;
            if (leave) {
                QDragLeaveEvent out;
                QApplication::sendEvent(page_, &out);
                return !hint->isVisible();
            }
            QDropEvent dropped(QPointF(pos), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(page_, &dropped);
            return !hint->isVisible();
        };
        QStringList paths;
        QList<QUrl> urls;
        for (const char* name : {"Nha-pho.dwg", "Ket-cau.DXF", "Mau.dwt", "Nuoc.dxf", "Mat-bang.DWG"}) {
            paths << dir_.filePath(QString::fromLatin1(name));
            urls << QUrl::fromLocalFile(paths.last());
        }
        QVERIFY(drop(urls, true)); // kéo vào rồi ra: không mở
        QCOMPARE(open.count(), 0);
        QVERIFY(drop(urls));
        QCOMPARE(open.count(), 1);
        QCOMPARE(open[0][0].toStringList(), paths);
        const int notes = center_->rowCount();
        QVERIFY(drop({QUrl::fromLocalFile(dir_.filePath(u"Bao-gia.pdf"_s)), QUrl::fromLocalFile(paths[0])}));
        QCOMPARE(open.count(), 2);
        QCOMPARE(open[1][0].toStringList(), QStringList{paths[0]});
        QCOMPARE(center_->rowCount(), notes + 1);
        QCOMPARE(toasts_->toasts().last()->severity(), Severity::Error);
        QVERIFY(lastToast().contains(u"“Bao-gia.pdf”"_s));
        QVERIFY(!drop({QUrl(u"https://trioncad.vn/mau.dwg"_s)}));
        QCOMPARE(open.count(), 2);
    }

    // Bàn phím: Tab đi qua ô tìm, sắp xếp, kiểu xem tới lưới; Enter mở tệp hiện hành, phím Menu và Shift+F10 mở menu;
    // Enter, Space trên nút của trang; tên cho trình đọc màn hình.
    void keyboard()
    {
        const QList<Seed> seeds = makeFiles(3, [](int i) { return u"K%1.dwg"_s.arg(i); });
        seed(seeds);
        makePage();
        QSignalSpy open(page_, &StartPage::openRequested);
        QSignalSpy macros(page_, &StartPage::macroRequested);
        window_->activateWindow();
        before_->setFocus(Qt::OtherFocusReason);
        QVERIFY(spinUntil([&] { return QApplication::focusWidget() == before_; }));
        QTest::keyClick(before_, Qt::Key_Tab);
        QCOMPARE(QApplication::focusWidget(), child(u"template0"_s)); // trang bắt đầu ở Tạo mới
        auto* openFile = child<QAbstractButton>(u"openFile"_s);
        openFile->setFocus(Qt::TabFocusReason);
        QTest::keyClick(openFile, Qt::Key_Return);
        QTest::keyClick(openFile, Qt::Key_Space);
        QCOMPARE(macros.count(), 2);

        auto* field = child<QLineEdit>(u"recentSearch"_s);
        field->setFocus(Qt::TabFocusReason);
        QTest::keyClick(field, Qt::Key_Tab);
        QCOMPARE(QApplication::focusWidget(), child(u"recentSort"_s));
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Tab);
        QVERIFY(child(u"recentViewToggle"_s)->isAncestorOf(QApplication::focusWidget()));
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Tab);
        QCOMPARE(QApplication::focusWidget(), grid());
        grid()->setCurrentIndex(page_->filter().index(1, 0));
        QTest::keyClick(grid(), Qt::Key_Return);
        QCOMPARE(open.count(), 1);
        QCOMPARE(open[0][0].toStringList(), QStringList{page_->filter().entry(1).path});
        QTest::keyClick(grid(), Qt::Key_Menu);
        QVERIFY(spinUntil([&] { return menu()->isVisible(); }));
        QTest::keyClick(menu(), Qt::Key_Escape);
        QVERIFY(spinUntil([&] { return !menu()->isVisible(); }));
        QTest::keyClick(grid(), Qt::Key_F10, Qt::ShiftModifier);
        QVERIFY(spinUntil([&] { return menu()->isVisible(); }));
        QTest::keyClick(menu(), Qt::Key_Escape);
        QVERIFY(spinUntil([&] { return !menu()->isVisible(); }));
        QTest::keyClick(grid(), Qt::Key_Right); // mũi tên đi qua thẻ
        QCOMPARE(grid()->currentIndex().row(), 2);

        page_->setView(StartPage::View::List);
        list()->setFocus(Qt::TabFocusReason);
        list()->setCurrentIndex(page_->filter().index(0, 0));
        QTest::keyClick(list(), Qt::Key_Enter);
        QCOMPARE(open.count(), 2);
        QTest::keyClick(list(), Qt::Key_Menu);
        QVERIFY(spinUntil([&] { return menu()->isVisible(); }));
        QTest::keyClick(menu(), Qt::Key_Escape);

        QCOMPARE(grid()->accessibleName(), StartPage::tr("Tệp gần đây"));
        QCOMPARE(child(u"recentSearch"_s)->accessibleName(), StartPage::tr("Tìm trong tệp gần đây"));
        QCOMPARE(child(u"recover"_s)->accessibleName(), StartPage::tr("Phục hồi bản vẽ lỗi"));
        QVERIFY(!child(u"openFile"_s)->accessibleDescription().isEmpty());
        const QModelIndex first = recent_->index(0, RecentFiles::Name);
        QVERIFY(first.data(Qt::AccessibleTextRole).toString().startsWith(recent_->entry(0).name));
    }

    // Thay ảnh baseline: kiểm điểm ảnh không có chữ trên lưới (Tối, Sáng): thẻ có ảnh hiện đúng màu ảnh 1:1, ô giữ chỗ
    // (lỗi, không tìm thấy) màu nền vùng vẽ, ghim --c-accent chỉ ở thẻ đã ghim. Chữ "Không tìm thấy" kiểm bằng thuộc
    // tính (missingFiles): chữ khử răng cưa có viền màu, so điểm ảnh không tin cậy.
    void pixels()
    {
        QDir().mkpath(files());
        const QString shown = files() + u"/anh.dwg"_s, broken = files() + u"/hong001.dwg"_s;
        QVERIFY(writeFile(shown, dwg("AC1032")));
        QVERIFY(writeFile(broken, dwg("AC1032")));
        seed({Seed{.path = shown, .modified = 3000, .pinned = true}, Seed{.path = broken, .modified = 2000},
              Seed{.path = files() + u"/mat.dwg"_s, .modified = 1000}});
        for (const Theme theme : {Theme::Dark, Theme::Light}) {
            tm_->setTheme(theme);
            QCoreApplication::processEvents();
            makePage();
            QVERIFY(spinUntil([&] { return refreshed() && settled(visiblePaths()); }));
            QImage sheet(grid()->viewport()->size(), QImage::Format_ARGB32_Premultiplied);
            sheet.fill(Qt::transparent);
            grid()->viewport()->render(&sheet);
            const auto near = [](QRgb a, QColor b, int tol = 8) {
                return std::abs(qRed(a) - b.red()) <= tol && std::abs(qGreen(a) - b.green()) <= tol
                       && std::abs(qBlue(a) - b.blue()) <= tol;
            };
            const auto count = [&](const QRect& r, QColor c) { // điểm gần màu c
                int n = 0;
                for (int y = r.top(); y <= r.bottom(); ++y)
                    for (int x = r.left(); x <= r.right(); ++x)
                        n += near(sheet.pixel(x, y), c, 16);
                return n;
            };
            const QRect t0 = thumbRect(proxyRow(shown)), t1 = thumbRect(proxyRow(broken));
            const QRect t2 = thumbRect(proxyRow(files() + u"/mat.dwg"_s));
            QVERIFY(near(sheet.pixel(t0.center()), QColor(kThumbColor)));
            const QColor canvas = toQColor(tok::color(tok::c_canvas, tok::Mode{}));
            QVERIFY(near(sheet.pixel(t1.topLeft() + QPoint(4, 4)), canvas)); // ô giữ chỗ
            QVERIFY(near(sheet.pixel(t2.topLeft() + QPoint(4, 4)), canvas));
            const QRect c0 = cardRect(proxyRow(shown));
            QVERIFY(count(QRect(c0.right() - 12 - 22, c0.top() + 12, 22, 22), tm_->color(tok::c_accent)) >= 6); // ghim
            const QRect c1 = cardRect(proxyRow(broken));
            QVERIFY(count(QRect(c1.right() - 12 - 22, c1.top() + 12, 22, 22), tm_->color(tok::c_accent)) == 0);
        }
    }

    // DoD-U: vi-XL không cắt chữ ở 1366 và 1920 px, lưới và bảng, Dùng thử (chữ dài nhất), có thông báo, có tệp không tìm
    // thấy; chữ tự vẽ của thẻ ("Không tìm thấy") vừa thẻ; trạng thái rỗng.
    void pseudoNoTruncation()
    {
        QVERIFY(translator_->setLanguage(i18n::kPseudoLanguage.toString()));
        QVERIFY(spinUntil([&] { return StartPage::tr("Mở tệp…").size() > 8; }));
        gate_->setEdition(edition::Edition::Trial);
        QList<Seed> seeds = makeFiles(12, [](int i) { return u"Ban-ve-%1.dwg"_s.arg(i); });
        seeds << Seed{.path = files() + u"/khong-con.dwg"_s};
        // Thư mục gần đây dài không có chỗ xuống dòng (như thư mục tạm của Windows C:\Users\…\AppData\Local\Temp\…):
        // dòng phụ của "Mở thư mục gần đây" một dòng rút gọn giữa, đủ đường dẫn ở tooltip.
        const QString longDir = files() + u"/DuAnNhaOGiaDinhBaTangKetCauBeTongCotThepPhuongAnHaiNam2026"_s;
        seeds.prepend(Seed{.path = longDir + u"/Mat-bang.dwg"_s});
        seed(seeds);
        for (const QSize size : {QSize(1366, 768), QSize(1920, 1080)}) {
            makePage(size);
            QVERIFY(child(u"recentFolders"_s)->toolTip().contains(QDir::toNativeSeparators(longDir)));
            gate_->setTrialDays(12);
            page_->setExpiry(QDate(2027, 10, 18));
            page_->setNotifications({{u"update"_s, StartPage::tr("Thông báo"), StartPage::tr("Mẹo"), u"update"_s}});
            QVERIFY(spinUntil([&] { return refreshed(); }));
            const int cardW = cardRect(0).width() - 2 * px(tok::sp_4); // chữ tự vẽ của thẻ: biểu tượng 12 + 4 + chữ
            QVERIFY(QFontMetrics(font(TextRole::Caption)).horizontalAdvance(RecentFiles::tr("Không tìm thấy")) + 16
                    <= cardW);
            for (StartPage::View v : {StartPage::View::Grid, StartPage::View::List}) {
                page_->setView(v);
                QCoreApplication::processEvents();
                QCOMPARE(truncations(window_.get()), QString());
            }
            // .hrow xuống dòng: tên và huy hiệu "Dùng thử" không vừa một dòng của thẻ, huy hiệu dưới tên.
            QVERIFY(child(u"editionBadge"_s)->geometry().top() > child(u"editionName"_s)->geometry().bottom());
            QVERIFY(page_->width() == size.width());
        }
        child<QLineEdit>(u"recentSearch"_s)->setText(u"zzz"_s); // i18n-ok: chữ gõ thử
        QCoreApplication::processEvents();
        QCOMPARE(truncations(window_.get()), QString());
        gate_->setEdition(edition::Edition::Viewer);
        QCoreApplication::processEvents();
        QCOMPARE(truncations(window_.get()), QString());
    }

    // DoD-H 6: xong việc thì trang không còn timer chạy.
    void idle()
    {
        seed(makeFiles(10, [](int i) { return u"I%1.dwg"_s.arg(i); }));
        makePage();
        QVERIFY(spinUntil([&] { return refreshed() && settled(visiblePaths()); }));
        spinFor(50ms);
        const auto timers = page_->findChildren<QTimer*>();
        QCOMPARE(std::count_if(timers.begin(), timers.end(), [](const QTimer* t) { return t->isActive(); }), 0);
        QVERIFY(!page_->thumbnails().busy());
    }
};

QTEST_MAIN(StartTest)
#include "start_test.moc"
