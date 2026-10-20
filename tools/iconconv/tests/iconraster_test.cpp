// UI0-103 tiêu chí 2: raster QtSvg của SVG đã chuyển khớp ảnh tham chiếu chụp từ trình duyệt
// (capture_ref.py). Ảnh tham chiếu là nét đen trên nền trong suốt nên chỉ so alpha (độ phủ). Pixel "khác" khi
// |Δalpha| > kAlphaTol: khử răng cưa hai bên lệch tới 127/255 (đo trên 646 ảnh) vì Skia lấy mẫu điểm con còn
// Qt cộng dồn diện tích, chỗ các nét cùng path chồng nhau bị đậm hơn; nét thiếu, thừa hay lệch nửa pixel thì
// vượt xa (metricRejectsWrongIcon). Mỗi biểu tượng: pixel khác ≤ 2% (5/256 ở 16 px, 20/1024 ở 32 px).
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QSvgRenderer>
#include <QtTest>

namespace {

const QString kIcons = QStringLiteral(TRION_SOURCE_DIR "/app/theme/icons/");
constexpr int kAlphaTol = 96;     // 38% thang alpha
constexpr double kMaxDiff = 0.02; // tiêu chí 2

QByteArray readFile(const QString& path) {
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

// Raster như bộ biểu tượng lúc chạy sẽ làm: cỡ 32 px đổi độ dày nét gốc (lần xuất hiện đầu, trên <svg>).
// dx: dời hình sang trái dx px (chỉ để kiểm thước đo).
QImage render(QByteArray svg, int size, const QByteArray& rootStrokeWidth, double dx = 0) {
    const QByteArray base = "stroke-width=\"1.25\"";
    if (const qsizetype at = svg.indexOf(base); at >= 0)
        svg.replace(at, base.size(), "stroke-width=\"" + rootStrokeWidth + '"');
    QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QSvgRenderer renderer(svg);
    renderer.setViewBox(QRectF(dx * 16 / size, 0, 16, 16));
    QPainter p(&img);
    renderer.render(&p);
    return img;
}

// Tỉ lệ pixel khác giữa hai ảnh cùng cỡ.
double diffRatio(const QImage& a, const QImage& b) {
    int n = 0;
    for (int y = 0; y < a.height(); ++y)
        for (int x = 0; x < a.width(); ++x)
            if (qAbs(qAlpha(a.pixel(x, y)) - qAlpha(b.pixel(x, y))) > kAlphaTol)
                ++n;
    return double(n) / (a.width() * a.height());
}

struct Sheet {
    QImage image;
    int size = 0, pitch = 0, offset = 0;
    QByteArray rootStrokeWidth;
};

} // namespace

class IconRasterTest : public QObject {
    Q_OBJECT
    QJsonObject layout_;
    QStringList names_; // thứ tự ô trên ảnh tham chiếu
    QList<Sheet> sheets_;
    double worst_ = 0;
    QString worstName_;

    QImage reference(const Sheet& s, qsizetype index) const {
        const int cols = layout_.value("columns").toInt();
        return s.image.copy(int(index % cols) * s.pitch + s.offset, int(index / cols) * s.pitch + s.offset,
                            s.size, s.size);
    }

  private slots:
    void initTestCase() {
        layout_ = QJsonDocument::fromJson(readFile(kIcons + "ref/layout.json")).object();
        for (const QJsonValue v : layout_.value("names").toArray())
            names_ << v.toString();
        for (const QJsonValue v : layout_.value("sheets").toArray()) {
            const QJsonObject o = v.toObject();
            sheets_.append({QImage(kIcons + "ref/" + o.value("file").toString()), o.value("size").toInt(),
                            o.value("pitch").toInt(), o.value("offset").toInt(),
                            o.value("rootStrokeWidth").toString().toUtf8()});
            QVERIFY2(!sheets_.last().image.isNull(), qPrintable(o.value("file").toString()));
        }
        QCOMPARE(sheets_.size(), 2);
    }

    // Biểu tượng mới trong manifest mà chưa có ảnh tham chiếu → chạy lại capture_ref.py.
    void referenceCoversManifest() {
        const QJsonArray icons =
            QJsonDocument::fromJson(readFile(kIcons + "manifest.json")).object().value("icons").toArray();
        QVERIFY(!icons.isEmpty());
        for (const QJsonValue v : icons) {
            const QString name = v.toObject().value("name").toString();
            QVERIFY2(names_.contains(name), qPrintable(name + ": chưa có ảnh tham chiếu"));
        }
    }

    void matchesReference_data() {
        QTest::addColumn<int>("sheet");
        QTest::addColumn<qsizetype>("index");
        for (qsizetype s = 0; s < sheets_.size(); ++s)
            for (qsizetype i = 0; i < names_.size(); ++i)
                QTest::addRow("%s@%d", qPrintable(names_[i]), sheets_[s].size) << int(s) << i;
    }

    void matchesReference() {
        QFETCH(int, sheet);
        QFETCH(qsizetype, index);
        const Sheet& s = sheets_[sheet];
        const QString& name = names_[index];
        const QByteArray svg = readFile(kIcons + "src/" + name + ".svg");
        QVERIFY2(!svg.isEmpty(), qPrintable(name + ".svg"));
        const double r = diffRatio(render(svg, s.size, s.rootStrokeWidth), reference(s, index));
        if (r > worst_) {
            worst_ = r;
            worstName_ = name + '@' + QString::number(s.size);
        }
        QVERIFY2(r <= kMaxDiff, qPrintable(QString("%1: %2% pixel khác").arg(name).arg(r * 100, 0, 'f', 2)));
    }

    // Thước đo phải phân biệt: biểu tượng khác, hoặc đúng biểu tượng mà lệch nửa pixel, đều vượt ngưỡng.
    void metricRejectsWrongIcon() {
        const qsizetype a = names_.indexOf("save"), b = names_.indexOf("open");
        QVERIFY(a >= 0 && b >= 0);
        const QByteArray svg = readFile(kIcons + "src/save.svg");
        QVERIFY(diffRatio(render(svg, 16, "1.25"), reference(sheets_[0], a)) <= kMaxDiff);
        QVERIFY(diffRatio(render(svg, 16, "1.25"), reference(sheets_[0], b)) > kMaxDiff);
        QVERIFY(diffRatio(render(svg, 16, "1.25", 0.5), reference(sheets_[0], a)) > kMaxDiff);
    }

    void cleanupTestCase() {
        qInfo("tỉ lệ pixel khác lớn nhất: %.2f%% (%s)", worst_ * 100, qPrintable(worstName_));
    }
};

QTEST_MAIN(IconRasterTest)
#include "iconraster_test.moc"
