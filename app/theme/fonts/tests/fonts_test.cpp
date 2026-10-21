// UI0-003: font Plex nhúng và vai trò chữ. Mỗi tiêu chí nghiệm thu một nhóm hàm test:
//   1  vietnamese: 134 chữ có dấu có trong từng tệp Plex và được dựng bằng chính Plex Sans/Mono ở mọi độ đậm,
//      0 glyph rơi về font khác; fallback: ký hiệu Plex thiếu (⌀ ∠ ⊥) đi font dự phòng, không ra ô trống.
//   2  stackedMarksFitRow: hộp bao mực chữ dấu chồng tầng nằm trọn dòng --h-row (22 px) ở DPR 1 / 1,5 / 2;
//      baseline: ảnh chụp so tests/baseline theo nền tảng (ghi lại: TRION_UPDATE_BASELINE=<thư mục>, chép vào
//      tests/baseline, thêm vào CMakeLists.txt).
//   3  rolesFromTokens: cỡ, độ đậm mọi vai trò lấy từ token theo chế độ ThemeManager.
//   4  loadTime: nạp font trên luồng UI ≤ 30 ms (vòng tốt nhất); license: OFL.txt nhúng, đọc qua API.
//   loadError: nhánh lỗi của loadFonts()/fontLicense() khi resource mất.
#include "app/theme/fonts/FontRegistry.h"

#include "app/theme/generated/Tokens.h"
#include "app/theme/ThemeManager.h"
#include "tests/timing.h"

#include <QElapsedTimer>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QGlyphRun>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QRawFont>
#include <QScreen>
#include <QSet>
#include <QTemporaryDir>
#include <QTextLayout>
#include <QtTest>

#include <algorithm>
#include <memory>

using namespace Qt::StringLiterals;
using namespace trion;
using namespace trion::theme;

namespace {

using trion::test::kTimed; // ngưỡng thời gian chỉ kiểm ở bản release (tests/timing.h)

constexpr double kLoadBudgetMs = 30; // tiêu chí 4 [đề xuất]

// Baseline theo nền tảng: FreeType + fontconfig (Linux, hinting nhẹ) và FreeType của Qt (Windows offscreen, hinting
// đủ) cho độ rộng chữ khác nhau, không so chung được. Dung sai: lệch ≤ kNoise/255 (khử răng cưa) bỏ qua; điểm lệch
// hơn được tối đa 0,1% ảnh (một glyph 12 px bị thay đã chạm ~40 điểm ảnh ở 100%).
#if defined(Q_OS_WIN)
constexpr auto kPlatform = "windows";
#elif defined(Q_OS_MACOS)
constexpr auto kPlatform = "macos";
#else
constexpr auto kPlatform = "linux";
#endif
constexpr int kNoise = 8;

const QStringList kFiles = {u"IBMPlexSans-Regular.ttf"_s, u"IBMPlexSans-Medium.ttf"_s, u"IBMPlexSans-SemiBold.ttf"_s,
                            u"IBMPlexMono-Regular.ttf"_s, u"IBMPlexMono-Medium.ttf"_s};

// Chuỗi thử dấu chồng tầng của styleguide.html mục Chữ.
const QString kStacked =
    u"ẤẦẨẪẬ ẮẰẲẴẶ ẾỀỂỄỆ ỐỒỔỖỘ ỚỜỞỠỢ ỨỪỬỮỰ Đ · Quản lý thiết lập trang · Đóng băng ở viewport mới"_s;

// 134 chữ cái tiếng Việt có dấu: 12 nguyên âm × 6 thanh, bỏ a e i o u y không dấu, thêm đ; hoa và thường.
// Dựng bằng chuẩn hóa NFC (nguyên âm + dấu thanh tổ hợp) thay vì gõ tay 134 ký tự.
QString vietnameseLetters()
{
    QString out;
    const char16_t tones[] = {0, 0x0300, 0x0309, 0x0303, 0x0301, 0x0323}; // ngang, huyền, hỏi, ngã, sắc, nặng
    for (QChar v : u"aăâeêioôơuưy"_s)
        for (char16_t tone : tones)
            for (QChar c : {v, v.toUpper()}) {
                if (!tone && c.unicode() < 0x80)
                    continue; // a e i o u y không dấu
                QString s(c);
                if (tone)
                    s += QChar(tone);
                out += s.normalized(QString::NormalizationForm_C);
            }
    return out + u"đĐ"_s;
}

QList<QGlyphRun> glyphRuns(const QFont& f, const QString& text)
{
    QTextLayout layout(text, f);
    layout.beginLayout();
    layout.createLine();
    layout.endLayout();
    return layout.glyphRuns();
}

// usWeightClass trong bảng OS/2 của mặt font thật sự dùng (không phải độ đậm yêu cầu).
int weightClass(const QRawFont& raw)
{
    const QByteArray os2 = raw.fontTable("OS/2");
    return os2.size() >= 6 ? (uchar(os2[4]) << 8 | uchar(os2[5])) : -1;
}

// Vẽ chữ vào dòng cao rowH đặt giữa khung cao gấp ba (mực tràn khỏi dòng vẫn thấy), căn giữa dọc như ô của view;
// trả hộp bao mực (alpha > 0) theo điểm ảnh thiết bị.
QRect inkBox(const QFont& f, const QString& text, qreal rowH, qreal dpr)
{
    const int width = int(QFontMetricsF(f).horizontalAdvance(text)) + 8;
    QImage img(QSize(width, int(3 * rowH)) * dpr, QImage::Format_ARGB32_Premultiplied);
    img.setDevicePixelRatio(dpr);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setFont(f);
    p.drawText(QRectF(0, rowH, width, rowH), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextDontClip, text);
    p.end();
    QRect box;
    for (int y = 0; y < img.height(); ++y) {
        const auto* line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int x = 0; x < img.width(); ++x)
            if (qAlpha(line[x]))
                box |= QRect(x, y, 1, 1);
    }
    return box;
}

} // namespace

class FontsTest : public QObject {
    Q_OBJECT
    QTemporaryDir dir_;
    std::unique_ptr<settings::Store> store_;
    std::unique_ptr<ThemeManager> tm_;
    double populateMs_ = 0;
    double firstLoadMs_ = 0;

    // Ảnh baseline: hai dòng --h-row (Sans --fs, Mono --fs) chuỗi thử của styleguide, độ phủ mực dạng xám (chữ đen
    // trên nền trắng), không phụ thuộc màu chủ đề.
    QImage sample(qreal dpr) const
    {
        const qreal rowH = tm_->px(tok::h_row);
        const int width = 720;
        QImage img(QSize(width, int(2 * rowH)) * dpr, QImage::Format_ARGB32_Premultiplied);
        img.setDevicePixelRatio(dpr);
        img.fill(Qt::transparent);
        QPainter p(&img);
        int row = 0;
        for (TextRole role : {TextRole::Ui, TextRole::Mono}) {
            p.setFont(font(role));
            p.drawText(QRectF(0, row++ * rowH, width, rowH), Qt::AlignLeft | Qt::AlignVCenter, kStacked);
        }
        p.end();
        QImage gray(img.size(), QImage::Format_Grayscale8);
        for (int y = 0; y < img.height(); ++y) {
            const auto* in = reinterpret_cast<const QRgb*>(img.constScanLine(y));
            uchar* out = gray.scanLine(y);
            for (int x = 0; x < img.width(); ++x)
                out[x] = uchar(255 - qAlpha(in[x]));
        }
        return gray;
    }

private slots:
    void initTestCase()
    {
        // Thứ tự như main: QApplication → loadFonts() → ThemeManager. Dựng CSDL font hệ thống đo riêng: ứng dụng Qt
        // nào cũng trả lúc dùng font lần đầu, không thuộc phần nạp Plex.
        QElapsedTimer clock;
        clock.start();
        QFontDatabase::families();
        populateMs_ = double(clock.nsecsElapsed()) / 1e6;
        clock.restart();
        const Expected<void> loaded = loadFonts();
        firstLoadMs_ = double(clock.nsecsElapsed()) / 1e6;
        QVERIFY2(loaded, loaded ? "" : loaded.error().message.c_str());
        QVERIFY(dir_.isValid());
        store_ = std::make_unique<settings::Store>(dir_.filePath(u"user.json"_s));
        tm_ = std::make_unique<ThemeManager>(*store_);
    }

    // Dọn ở đây, không để hàm hủy của lớp test (app/README.md: sau qExec TSan báo race giả).
    void cleanupTestCase()
    {
        tm_.reset();
        store_.reset();
    }

    // Tiêu chí 1.
    void vietnamese()
    {
        const QString letters = vietnameseLetters();
        QCOMPARE(letters.size(), 134);
        QCOMPARE(QSet<QChar>(letters.cbegin(), letters.cend()).size(), 134); // mỗi tổ hợp ra một chữ dựng sẵn

        for (const QString& file : kFiles) { // bảng cmap của từng tệp nhúng
            const QRawFont raw(u":/trion/fonts/"_s + file, 12);
            QVERIFY2(raw.isValid(), qPrintable(file));
            for (QChar c : letters)
                QVERIFY2(raw.supportsCharacter(c), qPrintable(file + u": thiếu "_s + c));
        }

        // Dựng thật bằng font của vai trò: mọi glyph run là Plex, đúng mặt theo độ đậm, không glyph 0 (ô trống).
        struct Case {
            TextRole role;
            Weight weight;
            QString family;
            int face; // usWeightClass của mặt phải dùng
        };
        const Case cases[] = {
            {TextRole::Ui, Weight::Regular, u"IBM Plex Sans"_s, 400},
            {TextRole::Ui, Weight::Medium, u"IBM Plex Sans"_s, 500},
            {TextRole::Ui, Weight::Bold, u"IBM Plex Sans"_s, 600},
            {TextRole::Mono, Weight::Regular, u"IBM Plex Mono"_s, 400},
            {TextRole::Mono, Weight::Medium, u"IBM Plex Mono"_s, 500},
            {TextRole::Mono, Weight::Bold, u"IBM Plex Mono"_s, 500}, // Mono không nhúng 600
        };
        for (const Case& c : cases) {
            qsizetype glyphs = 0;
            for (const QGlyphRun& run : glyphRuns(font(c.role, c.weight), letters)) {
                QCOMPARE(run.rawFont().familyName(), c.family);
                QCOMPARE(weightClass(run.rawFont()), c.face);
                QVERIFY(!run.glyphIndexes().contains(0));
                glyphs += run.glyphIndexes().size();
            }
            QCOMPARE(glyphs, letters.size());
        }
    }

    // Font dự phòng khi thiếu glyph.
    void fallback()
    {
        for (TextRole role : {TextRole::Ui, TextRole::Mono})
            for (QChar c : u"⌀∠⊥"_s) {
                const QFont f = font(role);
                QVERIFY(!QRawFont::fromFont(f).supportsCharacter(c)); // Plex thật sự thiếu
                const QList<QGlyphRun> runs = glyphRuns(f, QString(c));
                QCOMPARE(runs.size(), 1);
                const QString family = runs[0].rawFont().familyName();
                QVERIFY2(!family.startsWith("IBM Plex"_L1), qPrintable(family));
                QVERIFY2(runs[0].glyphIndexes().value(0) != 0, qPrintable(u"ô trống: "_s + c));
                qInfo("%s %s → %s", qPrintable(f.family()), qPrintable(QString(c)), qPrintable(family));
            }
    }

    // Tiêu chí 2: mọi vai trò đặt trong dòng (đến --fs-m 13 px), mọi độ đậm. Emphasis, Title, Display (15/18/24 px)
    // là đoạn và tiêu đề, dòng cao theo --lh-text: 15 px đậm ở DPR 2 nhô 1 điểm ảnh thiết bị khỏi dòng 22 px.
    void stackedMarksFitRow()
    {
        const qreal rowH = tm_->px(tok::h_row);
        QCOMPARE(rowH, 22.0); // dòng gọn; dòng thoáng 26 px rộng hơn
        const QString text = vietnameseLetters() + u' ' + kStacked;
        int minGap = 1000;
        for (qreal dpr : {1.0, 1.5, 2.0})
            for (TextRole r : {TextRole::Ui, TextRole::Label, TextRole::Caption, TextRole::Badge, TextRole::Heading,
                               TextRole::Mono, TextRole::MonoLabel, TextRole::MonoCaption}) {
                for (Weight w : {Weight::Regular, Weight::Medium, Weight::Bold}) {
                    const QRect ink = inkBox(font(r, w), text, rowH, dpr);
                    const int top = qRound(rowH * dpr), bottom = qRound(2 * rowH * dpr) - 1;
                    QVERIFY2(ink.top() >= top && ink.bottom() <= bottom,
                             qPrintable(u"vai trò %1, đậm %2, DPR %3: mực %4–%5, dòng %6–%7"_s.arg(int(r))
                                            .arg(int(w))
                                            .arg(dpr)
                                            .arg(ink.top())
                                            .arg(ink.bottom())
                                            .arg(top)
                                            .arg(bottom)));
                    minGap = std::min({minGap, ink.top() - top, bottom - ink.bottom()});
                }
            }
        qInfo("khoảng hở nhỏ nhất giữa mực và mép dòng: %d điểm ảnh thiết bị", minGap);
    }

    void baseline_data()
    {
        QTest::addColumn<qreal>("dpr");
        QTest::newRow("100%") << 1.0;
        QTest::newRow("150%") << 1.5;
        QTest::newRow("200%") << 2.0;
    }

    void baseline()
    {
        QFETCH(qreal, dpr);
        const QImage got = sample(dpr);
        const QString name = u"stacked-%1-%2.png"_s.arg(qRound(dpr * 100)).arg(QLatin1StringView(kPlatform));
        if (const QString out = qEnvironmentVariable("TRION_UPDATE_BASELINE"); !out.isEmpty()) {
            QVERIFY(got.save(out + u'/' + name));
            QSKIP("đã ghi baseline");
        }
        const QImage want = QImage(u":/baseline/"_s + name).convertToFormat(QImage::Format_Grayscale8);
        QVERIFY2(!want.isNull(), qPrintable(u"thiếu baseline "_s + name));
        QCOMPARE(got.size(), want.size());
        qsizetype off = 0;
        int maxDiff = 0;
        for (int y = 0; y < got.height(); ++y)
            for (int x = 0; x < got.width(); ++x) {
                const int d = std::abs(int(got.constScanLine(y)[x]) - int(want.constScanLine(y)[x]));
                maxDiff = std::max(maxDiff, d);
                off += d > kNoise;
            }
        qInfo("%s: %lld điểm ảnh lệch > %d/255, lệch lớn nhất %d/255", qPrintable(name), qint64(off), kNoise, maxDiff);
        QVERIFY(off <= got.width() * got.height() / 1000);
    }

    // Tiêu chí 3.
    void rolesFromTokens()
    {
        // Bảng mong đợi viết lại từ styleguide (không đọc bảng của FontRegistry).
        const tok::PxId sizes[] = {tok::fs,    tok::fs_s,   tok::fs_xs, tok::fs_2xs, tok::fs_m, tok::fs_l,
                                   tok::fs_xl, tok::fs_2xl, tok::fs,    tok::fs_s,   tok::fs_xs};
        const int weights[] = {400, int(tok::num(tok::fw_m)), int(tok::num(tok::fw_b))};
        QCOMPARE(weights[1], 500);
        QCOMPARE(weights[2], 600);
        const double dpi = QGuiApplication::primaryScreen()->logicalDotsPerInchY();
        for (Density d : {Density::Comfortable, Density::Compact}) {
            tm_->setDensity(d);
            QCOMPARE(tm_->mode().comfortable, d == Density::Comfortable);
            for (int r = 0; r <= int(TextRole::MonoCaption); ++r)
                for (int w = 0; w < 3; ++w) {
                    const QFont f = font(TextRole(r), Weight(w));
                    const double want = tok::px(sizes[r], tm_->mode());
                    QCOMPARE(f.pointSizeF() * dpi / 72, want); // cỡ yêu cầu đúng token
                    // Cỡ Qt dựng thật: QFontDatabasePrivate::load làm tròn pixelSize = qRound(pt × dpi / 72).
                    QCOMPARE(QRawFont::fromFont(f).pixelSize(), double(qRound(want)));
                    QCOMPARE(int(f.weight()), weights[w]);
                    QCOMPARE(f.families().value(0), r >= int(TextRole::Mono) ? u"IBM Plex Mono"_s : u"IBM Plex Sans"_s);
                }
        }
        // Hai mốc của tiêu chí: giao diện 12 px, nhãn phụ 11,5 px.
        QCOMPARE(QRawFont::fromFont(font(TextRole::Ui)).pixelSize(), 12.0);
        QEXPECT_FAIL("",
                     "Qt 6.11 dựng QFont ở cỡ pixel nguyên (QFontDatabasePrivate::load: qRound): 11,5 px thành 12 px",
                     Continue);
        QCOMPARE(QRawFont::fromFont(font(TextRole::Label)).pixelSize(), 11.5);
    }

    // Tiêu chí 4: OFL.txt nhúng và đọc được qua API (gói cài, hộp Giới thiệu: chưa có, xem báo cáo task).
    void license()
    {
        const QString text = fontLicense();
        QVERIFY(text.contains("SIL OPEN FONT LICENSE Version 1.1"_L1));
        QVERIFY(text.contains(u"with Reserved Font Name \"Plex\""_s));
    }

    // Nhánh lỗi: resource font mất (gỡ đăng ký) → Error nêu đủ 5 tệp, giấy phép rỗng; đăng ký lại thì đọc được.
    void loadError()
    {
        Q_CLEANUP_RESOURCE(plex_fonts);
        const Expected<void> loaded = loadFonts();
        const QString license = fontLicense();
        Q_INIT_RESOURCE(plex_fonts);
        QVERIFY(!loaded);
        QCOMPARE(loaded.error().module, std::string("ui"));
        QCOMPARE(loaded.error().code, std::string("font-load"));
        for (const QString& file : kFiles)
            QVERIFY(QString::fromStdString(loaded.error().message).contains(file));
        QVERIFY(license.isEmpty());
        QVERIFY(!fontLicense().isEmpty());
    }

    // Tiêu chí 4: chạy cuối vì gỡ rồi nạp lại font. Vòng tốt nhất trong 6 lần (lần đầu ở initTestCase).
    void loadTime()
    {
        double best = firstLoadMs_;
        for (int i = 0; i < 5; ++i) {
            QFontDatabase::removeAllApplicationFonts();
            QFontDatabase::families(); // dựng lại CSDL hệ thống, không tính
            QElapsedTimer clock;
            clock.start();
            const Expected<void> loaded = loadFonts();
            best = std::min(best, double(clock.nsecsElapsed()) / 1e6);
            QVERIFY(loaded);
        }
        qInfo("CSDL font hệ thống %.1f ms (không tính); nạp Plex: lần đầu %.2f ms, tốt nhất %.2f ms, ngân sách %.0f ms",
              populateMs_, firstLoadMs_, best, kLoadBudgetMs);
        QVERIFY2(!kTimed || best <= kLoadBudgetMs, qPrintable(QString::number(best)));
    }
};

QTEST_MAIN(FontsTest)
#include "fonts_test.moc"
