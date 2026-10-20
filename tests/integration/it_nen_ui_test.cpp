// Integration test nhóm 1: UI0-101 (executor), UI0-102 (token), UI0-103 (biểu tượng),
// UI0-104 (dịch), UI0-114 (phím tắt) chạy cùng nhau trên một QApplication và một kho cài đặt.

#include "app/core/UiExecutor.h"
#include "app/core/i18n/Translator.h"
#include "app/core/keymap/Keymap.h"
#include "app/theme/generated/Tokens.h"
#include "core/base/sched/Scheduler.h"
#include "core/base/settings/Settings.h"

#include <QApplication>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPainter>
#include <QSignalSpy>
#include <QSvgRenderer>
#include <QTemporaryDir>
#include <QTest>

#include <atomic>
#include <memory>
#include <vector>

using namespace Qt::StringLiterals;
using trion::keymap::Keymap;
using trion::keymap::Platform;
using trion::settings::Store;

namespace {

// Nhãn theo quy ước retranslate() của UI0-104, dữ liệu đến từ việc nền qua UI0-101.
class CountLabel : public QLabel {
public:
    void setCount(int n)
    {
        count_ = n;
        retranslate();
    }
    int count() const { return count_; }

protected:
    void changeEvent(QEvent* e) override
    {
        if (e->type() == QEvent::LanguageChange)
            retranslate();
        QLabel::changeEvent(e);
    }

private:
    void retranslate() { setText(QCoreApplication::translate("keymap", "Lưu") + u" · "_s + QString::number(count_)); }
    int count_ = -1;
};

QByteArray readFile(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

QString hex(tok::Rgba c)
{
    return QColor(c.r, c.g, c.b).name();
}

// Raster SVG đã thay currentColor; trả số pixel phủ đậm (alpha ≥ 128) và số pixel sai màu (lệch kênh > 3).
std::pair<int, int> rasterize(QByteArray svg, tok::Rgba c, int size)
{
    svg.replace("currentColor", hex(c).toLatin1());
    if (size == 32)
        svg.replace("stroke-width=\"1.25\"", "stroke-width=\"0.775\""); // quy ước 32 px của UI0-103 (nét gốc)
    QSvgRenderer r(svg);
    if (!r.isValid())
        return {-1, -1};
    QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    r.render(&p);
    p.end();
    const QImage straight = img.convertToFormat(QImage::Format_ARGB32);
    int covered = 0, wrong = 0;
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            const QRgb px = straight.pixel(x, y);
            if (qAlpha(px) < 128) // bỏ mép khử răng cưa: bỏ premultiply ở alpha thấp làm tròn lệch nhiều
                continue;
            ++covered;
            if (qAbs(qRed(px) - c.r) > 3 || qAbs(qGreen(px) - c.g) > 3 || qAbs(qBlue(px) - c.b) > 3)
                ++wrong;
        }
    return {covered, wrong};
}

} // namespace

class IntegrationNenUi : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QVERIFY(dir_.isValid());
        store_ = std::make_unique<Store>(dir_.filePath(u"user.json"_s));
        Store::setInstance(store_.get());
        executor_ = std::make_unique<trion::ui::UiExecutor>();
        translator_ = std::make_unique<trion::i18n::Translator>();
        QCOMPARE(translator_->language(), u"vi"_s); // lần chạy đầu: tiếng Việt
    }

    void cleanupTestCase()
    {
        trion::sched::waitIdle();
        translator_.reset();
        executor_.reset();
        Store::setInstance(nullptr);
        store_.reset();
    }

    // Kết quả nền đổ về liên tục trong lúc đổi ngôn ngữ: nhãn cuối cùng mang giá trị cuối và đúng ngôn ngữ cuối.
    void backgroundResultsSurviveLanguageSwitch()
    {
        CountLabel label;
        label.show();
        // Mỗi luồng một Receiver (một khóa gộp): giá trị cuối của từng luồng luôn về, nên số lớn nhất
        // (do luồng tăng seq sau cùng gửi ở vòng cuối của nó) chắc chắn tới nhãn.
        constexpr int kThreads = 4, kPerThread = 5000;
        std::vector<trion::ui::Receiver> rxs;
        for (int t = 0; t < kThreads; ++t)
            rxs.emplace_back(&label);
        const auto before = executor_->stats();
        std::atomic<int> seq{0};
        for (int t = 0; t < kThreads; ++t)
            trion::sched::submit(trion::sched::Priority::Normal, [rx = rxs[t], &label, &seq] {
                for (int i = 0; i < kPerThread; ++i) {
                    const int n = ++seq;
                    trion::ui::postLatest(rx, [&label, n] {
                        if (n > label.count()) // các luồng gửi xen nhau; nhãn giữ số lớn nhất đã về
                            label.setCount(n);
                    });
                }
            });
        for (const char* lang : {"en", "vi", "en", "vi", "en"}) {
            QVERIFY(translator_->setLanguage(QString::fromLatin1(lang)));
            QCoreApplication::processEvents();
        }
        trion::sched::waitIdle();
        QTRY_COMPARE(label.count(), kThreads * kPerThread);
        QCOMPARE(label.text(), u"Save · "_s + QString::number(kThreads * kPerThread));
        QVERIFY(translator_->setLanguage(u"vi"_s));
        // QApplication gửi LanguageChange tới cửa sổ bằng sự kiện đăng hàng đợi: nhãn đổi ở lượt vòng sự kiện kế.
        QTRY_COMPARE(label.text(), u"Lưu · "_s + QString::number(kThreads * kPerThread));
        const auto after = executor_->stats();
        QCOMPARE(after.dropped, before.dropped);
        QVERIFY(after.ran - before.ran <= quint64(kThreads * kPerThread)); // gộp mới nhất thắng: không chạy thừa
    }

    // Receiver bị hủy giữa chừng, ngay lúc đổi ngôn ngữ: kết quả còn chờ bị bỏ, không chạm vào nhãn đã hủy.
    void receiverDestroyedWhileLanguageChanges()
    {
        auto label = std::make_unique<CountLabel>();
        label->show();
        const trion::ui::Receiver rx(label.get());
        const auto before = executor_->stats();
        CountLabel* raw = label.get();
        for (int i = 0; i < 1000; ++i)
            trion::ui::post(rx, [raw, i] { raw->setCount(i); });
        label.reset();
        QVERIFY(translator_->setLanguage(u"en"_s));
        QTRY_COMPARE(executor_->stats().dropped - before.dropped, quint64(1000));
        QVERIFY(translator_->setLanguage(u"vi"_s));
    }

    // Tên hành động của keymap dịch theo ngôn ngữ giao diện; nhãn phím chỉ theo nền tảng, không theo ngôn ngữ.
    void keymapNamesFollowLanguageLabelsFollowPlatform()
    {
        Keymap win(*store_, Platform::Windows), mac(*store_, Platform::Mac);
        const auto* save = win.action(u"save"_s);
        QVERIFY(save);
        QCOMPARE(save->keys, QStringList{u"Ctrl+S"_s});
        const QString combo = save->keys.first();
        for (const char* lang : {"vi", "en"}) {
            QVERIFY(translator_->setLanguage(QString::fromLatin1(lang)));
            QCOMPARE(trion::keymap::keyLabel(combo, Platform::Windows), u"Ctrl+S"_s);
            QCOMPARE(trion::keymap::keyLabel(combo, Platform::Mac), u"⌘S"_s);
        }
        QCOMPARE(QCoreApplication::translate("keymap", save->name.toUtf8().constData()), u"Save"_s);
        QCOMPARE(QCoreApplication::translate("keymap", save->group.toUtf8().constData()), u"File"_s);
        int untranslated = 0;
        for (const auto& a : win.actions())
            if (QCoreApplication::translate("keymap", a.name.toUtf8().constData()).isEmpty())
                ++untranslated;
        QCOMPARE(untranslated, 0);
        QVERIFY(translator_->setLanguage(u"vi"_s));
        QCOMPARE(QCoreApplication::translate("keymap", save->name.toUtf8().constData()), u"Lưu"_s);
        QVERIFY(mac.actionFor(trion::keymap::keyCombination(u"Ctrl+S"_s, Platform::Mac)));
    }

    // Ngôn ngữ (UI0-104) và phím ghi đè (UI0-114) cùng một tệp cài đặt: ghi nền, nạp lại đủ cả hai; đổi khóa
    // từ nơi khác thì Keymap báo đổi.
    void settingsSharedAcrossModules()
    {
        const QString path = dir_.filePath(u"shared.json"_s);
        {
            Store s(path);
            Store::setInstance(&s);
            trion::i18n::Translator t;
            QVERIFY(t.setLanguage(u"en"_s));
            Keymap km(s, Platform::Windows);
            const auto* osnap = km.action(u"osnap"_s);
            QVERIFY(osnap);
            QVERIFY(!km.assign(u"osnap"_s, u"Ctrl+Alt+O"_s));
            QCoreApplication::processEvents(); // lần ghi gom chạy ở luồng nền
            trion::sched::waitIdle();
            Store::setInstance(store_.get());
        }
        Store r(path);
        QCOMPARE(r.get(trion::i18n::kLanguage), u"en"_s);
        Keymap km(r, Platform::Windows);
        QVERIFY(km.action(u"osnap"_s)->keys.contains(u"Ctrl+Alt+O"_s));
        QSignalSpy changed(&km, &Keymap::changed);
        r.remove(u"keymap/overrides"_s);
        QVERIFY(changed.count() >= 1);
        QVERIFY(!km.action(u"osnap"_s)->keys.contains(u"Ctrl+Alt+O"_s));
    }

    // Hợp đồng giữa UI0-103 và UI0-102 cho IconProvider (UI0-004): SVG một màu currentColor, tô bằng màu token
    // ở cả hai chủ đề ra đúng màu đó; cỡ 32 px dùng cùng SVG với nét 0,775. Mọi biểu tượng của manifest có tệp.
    void iconsRecolorWithThemeTokens()
    {
        const QString root = QString::fromUtf8(TRION_SOURCE_DIR) + u"/app/theme/icons/"_s;
        const QJsonArray icons = QJsonDocument::fromJson(readFile(root + u"manifest.json"_s))["icons"_L1].toArray();
        QCOMPARE(icons.size(), 323);
        int viewer = 0;
        for (const auto mode : {tok::Mode{}, tok::Mode{.light = true}}) {
            const tok::Rgba c = tok::color(tok::c_text_2, mode);
            for (const QJsonValue& v : icons) {
                const QString name = v["name"_L1].toString();
                const QByteArray svg = readFile(root + u"src/"_s + name + u".svg"_s);
                QVERIFY2(!svg.isEmpty(), qPrintable(name));
                QVERIFY2(!svg.contains('#'), qPrintable(name)); // không màu viết thẳng
                for (int size : {16, 32}) {
                    const auto [covered, wrong] = rasterize(svg, c, size);
                    QVERIFY2(covered > 0, qPrintable(name + u" @"_s + QString::number(size)));
                    QVERIFY2(wrong == 0, qPrintable(name + u" @"_s + QString::number(size)));
                }
                if (!mode.light && v["viewer"_L1].toBool())
                    ++viewer;
            }
        }
        QVERIFY(viewer >= 80); // bộ Viewer (07.1 §13.5: khoảng 80)
    }

    // Sau mọi việc trên: rảnh thì executor không bị đánh thức, kho cài đặt ghi xong.
    void idleAfterwards()
    {
        trion::sched::waitIdle();
        QTest::qWait(50);
        const auto before = executor_->stats().wakeups;
        QTest::qWait(1000);
        QCOMPARE(executor_->stats().wakeups, before);
    }

private:
    QTemporaryDir dir_;
    std::unique_ptr<Store> store_;
    std::unique_ptr<trion::ui::UiExecutor> executor_;
    std::unique_ptr<trion::i18n::Translator> translator_;
};

QTEST_MAIN(IntegrationNenUi)
#include "it_nen_ui_test.moc"
