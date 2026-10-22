// UI0-004 · IconProvider. Tiêu chí 1: raster đúng cỡ pixel thiết bị, khớp baseline 3 DPI × 2 chủ đề × 5 trạng thái.
// 2: đổi chủ đề, 550 biểu tượng đang hiện tô lại ≤ 30 ms luồng UI (kiểm ở bản release, vòng tốt nhất), không raster
// lại SVG. 3: lần vẽ thứ hai 0 raster; cache ≤ 16 MB, loại LRU; raster trước bộ Viewer, danh sách cho trước ở nền. 4:
// khóa khác vô hiệu về hình (huy hiệu); tương phản ≥ 3:1 với --c-base và --c-raised ở cả hai chủ đề.
#include "app/core/UiExecutor.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "core/base/sched/Scheduler.h"
#include "core/base/settings/Settings.h"
#include "tests/timing.h"

#include <QElapsedTimer>
#include <QFile>
#include <QPainter>
#include <QSvgRenderer>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

using namespace Qt::StringLiterals;
using namespace trion;
using theme::IconProvider;
using theme::IconState;
using theme::Theme;
using theme::ThemeManager;

namespace {

using trion::test::kTimed; // ngưỡng thời gian chỉ kiểm ở bản release (tests/timing.h)

constexpr IconState kStates[] = {IconState::Normal, IconState::Hover, IconState::On, IconState::Disabled,
                                 IconState::Locked};
const char* const kStateNames[] = {"Normal", "Hover", "On", "Disabled", "Locked"};

// Raster thẳng từ SVG ở cỡ px với nét gốc stroke, độc lập với IconProvider (đối chiếu tiêu chí 1).
QImage direct(const QString& name, int px, const QByteArray& stroke)
{
    QFile f(u":/trion/icons/"_s + name + u".svg"_s);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QByteArray svg = f.readAll();
    const QByteArray root = "stroke-width=\"1.25\"";
    svg.replace(svg.indexOf(root), root.size(), "stroke-width=\"" + stroke + '"');
    QImage img(px, px, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QSvgRenderer r(svg);
    QPainter p(&img);
    r.render(&p);
    return img;
}

// 550 biểu tượng đang hiện: cả bộ (323) ở 16 px và 227 biểu tượng đầu ở 32 px.
QList<std::pair<QIcon, int>> shown()
{
    QList<std::pair<QIcon, int>> out;
    const QStringList names = IconProvider::names();
    for (const QString& n : names)
        out.append({IconProvider::icon(n), 16});
    for (const QString& n : names.first(550 - names.size()))
        out.append({IconProvider::icon(n), 32});
    return out;
}

// Vẽ như widget: QIcon::paint lên thiết bị có DPR.
void paintAll(const QList<std::pair<QIcon, int>>& icons, qreal dpr)
{
    QImage target(QSize(32, 32) * dpr, QImage::Format_ARGB32_Premultiplied);
    target.setDevicePixelRatio(dpr);
    target.fill(Qt::transparent);
    QPainter p(&target);
    for (const auto& [icon, logical] : icons)
        icon.paint(&p, QRect(0, 0, logical, logical));
}

std::pair<QIcon::Mode, QIcon::State> modeFor(IconState s)
{
    return {s == IconState::Hover      ? QIcon::Active
            : s == IconState::Disabled ? QIcon::Disabled
                                       : QIcon::Normal,
            s == IconState::On ? QIcon::On : QIcon::Off};
}

// Bảng baseline ở một DPR: mỗi dòng một (chủ đề, trạng thái) trên nền --c-base; mỗi cột một biểu tượng mẫu ở 16
// rồi 32 px logic. Toạ độ chẵn nên ở 150% vẫn trùng lưới pixel.
QImage sheet(ThemeManager& tm, qreal dpr)
{
    const QStringList sample = {u"layers"_s, u"bulboff"_s, u"lw"_s, u"osnap"_s, u"zoom"_s};
    constexpr int kRow = 40, kWidth = 320;
    QImage img(QSize(kWidth, 10 * kRow) * dpr, QImage::Format_ARGB32_Premultiplied);
    img.setDevicePixelRatio(dpr);
    QPainter p(&img);
    int y = 0;
    for (const Theme t : {Theme::Dark, Theme::Light}) {
        tm.setTheme(t);
        for (const IconState s : kStates) {
            p.fillRect(QRect(0, y, kWidth, kRow), tm.color(tok::c_base));
            const auto [mode, state] = modeFor(s);
            int x = 4;
            for (const int logical : {16, 32})
                for (const QString& n : sample) {
                    IconProvider::icon(n, s == IconState::Locked)
                        .paint(&p, QRect(x, y + 4, logical, logical), Qt::AlignCenter, mode, state);
                    x += logical + 8;
                }
            y += kRow;
        }
    }
    return img;
}

// Tỉ lệ tương phản WCAG 2 giữa hai màu đục.
double contrast(const QColor& a, const QColor& b)
{
    const auto lum = [](const QColor& c) {
        const auto ch = [](double v) { return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
        return 0.2126 * ch(c.redF()) + 0.7152 * ch(c.greenF()) + 0.0722 * ch(c.blueF());
    };
    const double la = lum(a), lb = lum(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

} // namespace

class IconProviderTest : public QObject {
    Q_OBJECT
    QTemporaryDir dir_;
    std::unique_ptr<settings::Store> store_;
    std::unique_ptr<ui::UiExecutor> executor_;
    std::unique_ptr<ThemeManager> tm_;

private slots:
    void initTestCase()
    {
        QVERIFY(dir_.isValid());
        store_ = std::make_unique<settings::Store>(dir_.filePath(u"user.json"_s));
        settings::Store::setInstance(store_.get());
        executor_ = std::make_unique<ui::UiExecutor>();
        tm_ = std::make_unique<ThemeManager>(*store_);
    }

    void init() { tm_->setTheme(Theme::Dark); }

    void cleanupTestCase()
    {
        tm_.reset();
        settings::Store::setInstance(nullptr);
        store_.reset();
        sched::waitIdle();
        QCoreApplication::processEvents();
        executor_.reset();
    }

    // Tiêu chí 1: 16 px logic → 16/20/24/28/32 px ở 100–200% (32 → 32…64), mỗi cỡ raster riêng từ SVG và khớp
    // raster thẳng ở cỡ đó (không phóng ảnh cỡ khác).
    void rastersAtDeviceSize()
    {
        IconProvider icons;
        const QIcon icon = IconProvider::icon(u"layers"_s);
        for (const int logical : {16, 32})
            for (const qreal dpr : {1.0, 1.25, 1.5, 1.75, 2.0}) {
                const quint64 before = icons.stats().rasters;
                const QPixmap pm = icon.pixmap(QSize(logical, logical), dpr);
                const int px = qRound(logical * dpr);
                QCOMPARE(pm.size(), QSize(px, px));
                QCOMPARE(pm.devicePixelRatio(), dpr);
                QCOMPARE(icons.stats().rasters, before + 1);
                const QImage want = direct(u"layers"_s, px, logical >= 32 ? "0.775" : "1.25");
                const QImage got = pm.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
                int worst = 0;
                for (int y = 0; y < px; ++y)
                    for (int x = 0; x < px; ++x)
                        worst = std::max(worst, std::abs(qAlpha(got.pixel(x, y)) - qAlpha(want.pixel(x, y))));
                QVERIFY2(worst <= 1, qPrintable(u"%1 px: lệch alpha %2"_s.arg(px).arg(worst)));
            }
    }

    // Tiêu chí 1: bảng 2 chủ đề × 5 trạng thái ở 100/150/200% khớp ảnh trong tests/baseline/ (nhúng vào test).
    void matchesBaseline()
    {
        IconProvider icons;
        const bool update = qEnvironmentVariableIsSet("TRION_UPDATE_BASELINE");
        for (const int percent : {100, 150, 200}) {
            const QString file = u"dpr%1.png"_s.arg(percent);
            const QImage got = sheet(*tm_, percent / 100.0).convertToFormat(QImage::Format_ARGB32);
            if (update) {
                QVERIFY(got.save(QStringLiteral(TRION_ICONS_DIR "/tests/baseline/") + file));
                continue;
            }
            const QImage want = QImage(u":/baseline/"_s + file).convertToFormat(QImage::Format_ARGB32);
            QVERIFY2(!want.isNull(), qPrintable(file + u": chưa có baseline, chạy test với TRION_UPDATE_BASELINE=1"_s));
            QCOMPARE(got.size(), want.size());
            int bad = 0, worst = 0;
            for (int y = 0; y < got.height(); ++y)
                for (int x = 0; x < got.width(); ++x) {
                    const QRgb a = got.pixel(x, y), b = want.pixel(x, y);
                    const int d = std::max(
                        {std::abs(qRed(a) - qRed(b)), std::abs(qGreen(a) - qGreen(b)), std::abs(qBlue(a) - qBlue(b))});
                    worst = std::max(worst, d);
                    bad += d > 0;
                }
            if (bad)
                got.save(file); // để so: thư mục chạy test
            QVERIFY2(bad == 0, qPrintable(u"%1: %2 pixel lệch, lớn nhất %3"_s.arg(file).arg(bad).arg(worst)));
        }
    }

    // Tiêu chí 2: lượt vẽ đầu sau khi đổi chủ đề tô lại 550 biểu tượng từ mặt nạ, không raster lại SVG.
    void themeSwitchRecolor550()
    {
        const auto list = shown();
        QCOMPARE(list.size(), 550);
        for (const qreal dpr : {1.0, 2.0}) {
            qint64 best = std::numeric_limits<qint64>::max();
            for (int round = 0; round < 5; ++round) {
                tm_->setTheme(Theme::Dark);
                IconProvider icons; // cache mới: lần đổi đầu tiên, không có sẵn ảnh của chủ đề Sáng
                paintAll(list, dpr);
                tm_->setTheme(Theme::Light);
                const IconProvider::Stats before = icons.stats();
                QElapsedTimer clock;
                clock.start();
                paintAll(list, dpr);
                best = std::min(best, clock.nsecsElapsed());
                QCOMPARE(icons.stats().rasters, before.rasters);
                QCOMPARE(icons.stats().tints, before.tints + 550);
            }
            qInfo("doi chu de, 550 bieu tuong, DPR %.0f: %.2f ms luong UI (vong tot nhat/5)", dpr, double(best) / 1e6);
            QVERIFY2(!kTimed || best <= 30'000'000, qPrintable(QString::number(double(best) / 1e6)));
        }
    }

    // Tiêu chí 3: lần vẽ thứ hai lấy hết từ cache.
    void secondPaintNoRaster()
    {
        const auto list = shown();
        IconProvider icons;
        QElapsedTimer clock;
        clock.start();
        paintAll(list, 1.5);
        const qint64 cold = clock.nsecsElapsed();
        const IconProvider::Stats first = icons.stats();
        QCOMPARE(first.rasters, quint64(550)); // mỗi (tên, cỡ) một mặt nạ
        QCOMPARE(first.tints, quint64(550));
        clock.restart();
        paintAll(list, 1.5);
        const qint64 warm = clock.nsecsElapsed();
        QCOMPARE(icons.stats().rasters, first.rasters);
        QCOMPARE(icons.stats().tints, first.tints);
        QVERIFY(icons.stats().bytes <= IconProvider::kCacheBytes);
        qInfo("550 bieu tuong o 150%%: lan dau %.2f ms, lan hai %.2f ms; cache %lld KB", double(cold) / 1e6,
              double(warm) / 1e6, qlonglong(icons.stats().bytes / 1024));
    }

    // Tiêu chí 3: vượt 16 MB thì loại mục lâu nhất không dùng; mục dùng gần đây còn.
    void cacheBoundedLru()
    {
        IconProvider icons;
        const QStringList names = IconProvider::names();
        const auto draw = [&](const QString& n) { QVERIFY(!icons.pixmap(n, 64, 2, IconState::Normal).isNull()); };
        draw(names[0]); // 128 px: mặt nạ 16 KB + ảnh 64 KB; 323 biểu tượng vượt 16 MB
        qsizetype peak = 0;
        for (qsizetype i = 1; i < names.size(); ++i) {
            draw(names[i]);
            draw(names[0]); // giữ names[0] là mục mới dùng nhất
            peak = std::max(peak, icons.stats().bytes);
        }
        QVERIFY(peak <= IconProvider::kCacheBytes);
        QVERIFY(peak > IconProvider::kCacheBytes - 128 * 1024); // đã đầy, phải loại
        const quint64 rasters = icons.stats().rasters;
        draw(names[0]);
        QCOMPARE(icons.stats().rasters, rasters);
        draw(names[1]); // lâu nhất không dùng: đã bị loại, raster lại
        QCOMPARE(icons.stats().rasters, rasters + 1);
    }

    // Raster trước bộ Viewer ở nền: luồng UI chỉ gửi việc; xong thì vẽ bộ Viewer không raster thêm.
    void prewarmViewerInBackground()
    {
        {
            IconProvider gone; // hủy khi việc nền còn chạy: kết quả bị bỏ
            gone.prewarm(2);
        }
        sched::waitIdle();
        QCoreApplication::processEvents();

        IconProvider icons;
        const QStringList viewer = IconProvider::names(true);
        QVERIFY(viewer.size() >= 80);
        icons.prewarm(1.5);
        QCOMPARE(icons.stats().rasters, quint64(0));
        sched::waitIdle();
        QTRY_COMPARE(icons.stats().rasters, quint64(2 * viewer.size()));
        for (const QString& n : viewer)
            for (const int logical : {16, 32})
                QVERIFY(!icons.pixmap(n, logical, 1.5, IconState::Normal).isNull());
        QCOMPARE(icons.stats().rasters, quint64(2 * viewer.size()));
    }

    // Raster trước danh sách cho trước (ribbon): bỏ tên lạ, tên trùng; xong thì vẽ không raster thêm; xin lại tên đã
    // có cả hai cỡ thì không gửi việc. idpoint (nút chọn điểm của ô số UI-C09) nằm trong bộ Viewer.
    void prewarmList()
    {
        IconProvider icons;
        const QStringList list = {u"circle"_s, u"arc"_s, u"circle"_s, u"khong-co"_s, u"hatch"_s};
        icons.prewarm(1.25, list);
        sched::waitIdle();
        QTRY_COMPARE(icons.stats().rasters, quint64(2 * 3));
        for (const QString& n : {u"circle"_s, u"arc"_s, u"hatch"_s})
            for (const int logical : {16, 32})
                QVERIFY(!icons.pixmap(n, logical, 1.25, IconState::Hover).isNull());
        QCOMPARE(icons.stats().rasters, quint64(2 * 3));
        icons.prewarm(1.25, list);
        sched::waitIdle();
        QCoreApplication::processEvents();
        QCOMPARE(icons.stats().rasters, quint64(2 * 3));
        QVERIFY(IconProvider::names(true).contains(u"idpoint"_s));
    }

    // Tiêu chí 4: bỏ màu (chỉ xét alpha), Locked vẫn khác Disabled nhờ huy hiệu ổ khóa.
    void lockedDiffersFromDisabledByShape()
    {
        IconProvider icons;
        int least = std::numeric_limits<int>::max();
        for (const Theme t : {Theme::Dark, Theme::Light}) {
            tm_->setTheme(t);
            for (const QString& n : IconProvider::names())
                for (const int logical : {16, 32}) {
                    const QImage locked = icons.pixmap(n, logical, 1, IconState::Locked).toImage();
                    const QImage disabled = icons.pixmap(n, logical, 1, IconState::Disabled).toImage();
                    int shape = 0;
                    for (int y = 0; y < logical; ++y)
                        for (int x = 0; x < logical; ++x)
                            shape += std::abs(qAlpha(locked.pixel(x, y)) - qAlpha(disabled.pixel(x, y))) > 128;
                    least = std::min(least, shape);
                    QVERIFY2(shape >= logical / 2,
                             qPrintable(u"%1 @%2: %3 pixel khác hình"_s.arg(n).arg(logical).arg(shape)));
                }
        }
        qInfo("khoa/vo hieu: it nhat %d pixel khac hinh", least);
    }

    // Tiêu chí 4: màu nét đúng token; tương phản ≥ 3:1 với --c-base, --c-raised (Disabled chỉ báo, WCAG miễn).
    void contrastAgainstBackgrounds()
    {
        IconProvider icons;
        for (const Theme t : {Theme::Dark, Theme::Light}) {
            tm_->setTheme(t);
            for (int i = 0; i < 5; ++i) {
                // Pixel phủ kín nhất ở góc trên trái (ngoài huy hiệu khóa).
                const QImage img =
                    icons.pixmap(u"rect"_s, 16, 1, kStates[i]).toImage().convertToFormat(QImage::Format_ARGB32);
                QRgb ink = 0;
                for (int y = 0; y < 7; ++y)
                    for (int x = 0; x < 7; ++x)
                        if (qAlpha(img.pixel(x, y)) > qAlpha(ink))
                            ink = img.pixel(x, y);
                const tok::ColorId token[] = {tok::c_text_2, tok::c_text, tok::c_on, tok::c_text_dis, tok::c_text_2};
                const QColor want = tm_->color(token[i]);
                const int tol = kStates[i] == IconState::Locked ? 2 : 0; // bỏ premultiply ở alpha 191 làm tròn
                QVERIFY2(std::abs(qRed(ink) - want.red()) <= tol && std::abs(qGreen(ink) - want.green()) <= tol &&
                             std::abs(qBlue(ink) - want.blue()) <= tol,
                         kStateNames[i]);
                QCOMPARE(qAlpha(ink), kStates[i] == IconState::Locked ? 191 : 255);
                const double a = qAlpha(ink) / 255.0;
                for (const tok::ColorId bgId : {tok::c_base, tok::c_raised}) {
                    const QColor bg = tm_->color(bgId);
                    const QColor seen = QColor::fromRgbF(float(want.redF() * a + bg.redF() * (1 - a)),
                                                         float(want.greenF() * a + bg.greenF() * (1 - a)),
                                                         float(want.blueF() * a + bg.blueF() * (1 - a)));
                    const double ratio = contrast(seen, bg);
                    qInfo("%s %s tren %s: %.2f:1", t == Theme::Dark ? "Toi" : "Sang", kStateNames[i],
                          bgId.i == tok::c_base.i ? "base" : "raised", ratio);
                    if (kStates[i] != IconState::Disabled)
                        QVERIFY2(ratio >= 3, kStateNames[i]);
                }
            }
        }
    }

    // QIcon::Mode/State → trạng thái; paint() theo DPR thiết bị; tên lạ.
    void qiconMapsModes()
    {
        IconProvider icons;
        const QIcon icon = IconProvider::icon(u"rect"_s);
        const QIcon locked = IconProvider::icon(u"rect"_s, true);
        QVERIFY(!icon.isNull());
        QCOMPARE(icon.name(), u"rect"_s);
        const auto want = [&](IconState s) { return icons.pixmap(u"rect"_s, 16, 2, s).toImage(); };
        const struct {
            QIcon::Mode mode;
            QIcon::State state;
            IconState want;
        } cases[] = {
            {QIcon::Normal, QIcon::Off, IconState::Normal},   {QIcon::Active, QIcon::Off, IconState::Hover},
            {QIcon::Selected, QIcon::Off, IconState::Hover},  {QIcon::Normal, QIcon::On, IconState::On},
            {QIcon::Active, QIcon::On, IconState::On},        {QIcon::Disabled, QIcon::Off, IconState::Disabled},
            {QIcon::Disabled, QIcon::On, IconState::Disabled}};
        for (const auto& c : cases) {
            QCOMPARE(icon.pixmap(QSize(16, 16), 2, c.mode, c.state).toImage(), want(c.want));
            QCOMPARE(locked.pixmap(QSize(16, 16), 2, c.mode, c.state).toImage(), want(IconState::Locked));
        }
        QImage target(32, 32, QImage::Format_ARGB32_Premultiplied);
        target.setDevicePixelRatio(2);
        target.fill(Qt::transparent);
        {
            QPainter p(&target);
            icon.paint(&p, QRect(0, 0, 16, 16));
        }
        QCOMPARE(target, want(IconState::Normal));

        QTest::ignoreMessage(QtWarningMsg, "IconProvider: không có biểu tượng \"khong-co\"");
        QVERIFY(IconProvider::icon(u"khong-co"_s).isNull());
        QVERIFY(icons.pixmap(u"khong-co"_s, 16, 1, IconState::Normal).isNull());
    }
};

QTEST_MAIN(IconProviderTest)
#include "icon_provider_test.moc"
