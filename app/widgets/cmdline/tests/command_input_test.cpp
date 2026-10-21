// UIA-106 · CommandInput: 134 chữ có dấu gõ bằng Telex và VNI qua chuỗi QInputMethodEvent giả lập như bộ gõ gửi
// (preedit từng bước rồi commit NFC hoặc NFD; không preedit, commit từng phím thay chữ trước con trỏ hoặc gửi dấu
// rời) ra NFC đúng 100%; chờ tên lệnh bỏ qua bộ gõ ("lm", "dd", "aa", "ow" ra LM, DD, AA, OW), tắt tùy chọn thì gõ
// có dấu; 10.000 chuỗi phím ngẫu nhiên không mất, không lặp; ký tự hiện trong ô ≤ 16 ms p99 (UiProbe typing).
//
// Nền tảng giả: test đóng vai plugin input method của Qt (ibus, fcitx5, cocoa, windows đều làm vậy): ô nhận input
// method (Qt::ImEnabled qua QInputMethodQueryEvent) thì phím đi vào bộ gõ giả và ô nhận QInputMethodEvent, không thì
// ô nhận thẳng QKeyEvent của phím vật lý.
#include "app/widgets/cmdline/CommandInput.h"

#include "app/core/perf/UiProbe.h"
#include "app/core/UiExecutor.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/ThemeManager.h"
#include "core/base/sched/Scheduler.h"
#include "tests/timing.h"

#include <QDebug>
#include <QImage>
#include <QInputMethodEvent>
#include <QSet>
#include <QTemporaryDir>
#include <QTest>
#include <QTextCharFormat>
#include <QTimer>

#include <functional>
#include <memory>
#include <random>

using namespace Qt::StringLiterals;
using namespace trion;
namespace probe = trion::ui::probe;
using trion::ui::CommandInput;

namespace {

// 67 chữ thường có dấu của tiếng Việt; thêm 67 chữ hoa là đủ 134.
const QString kLower = u"áàảãạăắằẳẵặâấầẩẫậéèẻẽẹêếềểễệíìỉĩịóòỏõọôốồổỗộơớờởỡợúùủũụưứừửữựýỳỷỹỵđ"_s; // i18n-ok: bộ chữ mẫu

enum class Method { Telex, Vni };
// Cách bộ gõ đưa chữ vào ô: preedit rồi commit NFC; preedit rồi commit NFD; không preedit, mỗi phím đổi chữ thì
// commit chữ mới (NFD) thay chữ trước con trỏ; không preedit, gửi riêng dấu (tổ hợp NFD tách lượt).
enum class Style { PreeditNfc, PreeditNfd, Replace, LoneMark };

QString nfc(const QString& s)
{
    return s.normalized(QString::NormalizationForm_C);
}
QString nfd(const QString& s)
{
    return s.normalized(QString::NormalizationForm_D);
}

// Dấu mà phím thêm vào chữ cuối `last` theo kiểu gõ; 0 nếu phím là chữ thường (gõ tiếp). U+0111: d thành đ.
char16_t markOf(QChar key, QChar last, Method m)
{
    const QChar k = key.toLower();
    const QChar base = nfd(QString(last)).front().toLower();
    const bool vowel = QStringView(u"aeiouy").contains(base);
    const QStringView tones = m == Method::Telex ? u"sfrxj" : u"12345";
    static constexpr char16_t kTone[] = {0x0301, 0x0300, 0x0309, 0x0303, 0x0323};
    if (base == u'd' && k == (m == Method::Telex ? u'd' : u'9'))
        return 0x0111;
    if (m == Method::Telex ? (k == base && QStringView(u"aeo").contains(k))
                           : (k == u'6' && QStringView(u"aeo").contains(base)))
        return 0x0302;
    if (base == u'a' && k == (m == Method::Telex ? u'w' : u'8'))
        return 0x0306;
    if (QStringView(u"ou").contains(base) && k == (m == Method::Telex ? u'w' : u'7'))
        return 0x031B;
    if (vowel && tones.contains(k))
        return kTone[tones.indexOf(k)];
    return 0;
}

// Phím để gõ một chữ có dấu: chữ gốc (hoa thì Shift), phím dấu mũ/trăng/móc/đ, phím thanh.
QString keysFor(QChar ch, Method m)
{
    const bool telex = m == Method::Telex;
    if (ch.toLower() == QChar(0x0111))
        return QString(ch == QChar(0x0111) ? u'd' : u'D') + (telex ? u'd' : u'9');
    const QString d = nfd(QString(ch));
    QString keys(d.front());
    for (const QChar mark : QStringView(d).mid(1)) {
        const QStringView tones = telex ? u"sfrxj" : u"12345";
        switch (mark.unicode()) {
        case 0x0302:
            keys += telex ? d.front().toLower() : u'6';
            break;
        case 0x0306:
            keys += telex ? u'w' : u'8';
            break;
        case 0x031B:
            keys += telex ? u'w' : u'7';
            break;
        case 0x0301:
            keys += tones[0];
            break;
        case 0x0300:
            keys += tones[1];
            break;
        case 0x0309:
            keys += tones[2];
            break;
        case 0x0303:
            keys += tones[3];
            break;
        case 0x0323:
            keys += tones[4];
            break;
        }
    }
    return keys;
}

bool imEnabled(QWidget& w)
{
    QInputMethodQueryEvent q(Qt::ImEnabled);
    QCoreApplication::sendEvent(&w, &q);
    return q.value(Qt::ImEnabled).toBool();
}

// Bộ gõ giả (Telex hoặc VNI) cho một từ, đưa chữ vào ô theo `style`. Ô không nhận input method thì phím đi thẳng.
// Trả chuỗi bộ gõ tạo ra (NFC) để so.
QString type(CommandInput& w, const QString& keys, Method m, Style style, bool underline = true)
{
    if (!imEnabled(w)) {
        for (const QChar k : keys)
            QTest::keyClick(&w, k.toLatin1());
        return keys;
    }
    QString word;
    for (const QChar k : keys) {
        char16_t mark = word.isEmpty() ? 0 : markOf(k, word.back(), m);
        // Bộ gõ giả chỉ dựng chữ dựng sẵn một mã (thứ tự dấu lạ như "á" + mũ thì coi phím là chữ thường).
        const QString next = mark && mark != 0x0111 ? nfc(QString(word.back()) + QChar(mark)) : QString();
        if (mark && mark != 0x0111 && next.size() != 1)
            mark = 0;
        if (mark == 0x0111)
            word.back() = word.back().isUpper() ? QChar(0x0110) : QChar(0x0111);
        else if (mark)
            word.back() = next.front();
        else
            word += k;
        if (style == Style::PreeditNfc || style == Style::PreeditNfd) {
            QList<QInputMethodEvent::Attribute> attrs{{QInputMethodEvent::Cursor, int(word.size()), 1}};
            if (underline) {
                QTextCharFormat f;
                f.setUnderlineStyle(QTextCharFormat::SingleUnderline);
                attrs.append({QInputMethodEvent::TextFormat, 0, int(word.size()), f});
            }
            QInputMethodEvent e(word, attrs);
            QCoreApplication::sendEvent(&w, &e);
            continue;
        }
        QInputMethodEvent e;
        if (!mark)
            e.setCommitString(QString(k));
        else if (style == Style::LoneMark && mark != 0x0111)
            e.setCommitString(QString(QChar(mark))); // chỉ gửi dấu, chữ gốc đã nằm trong ô
        else
            e.setCommitString(nfd(QString(word.back())), -1, 1);
        QCoreApplication::sendEvent(&w, &e);
    }
    if (style == Style::PreeditNfc || style == Style::PreeditNfd) {
        QInputMethodEvent e;
        e.setCommitString(style == Style::PreeditNfc ? word : nfd(word));
        QCoreApplication::sendEvent(&w, &e);
    }
    return word;
}

// Vòng sự kiện thật đến khi cond() đúng hoặc quá hạn (như spinUntil của ui_probe_test).
bool spinUntil(const std::function<bool()>& cond, std::chrono::milliseconds timeout = std::chrono::seconds(10))
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

QJsonValue typing()
{
    const QJsonObject j = probe::toJson();
    return j["ops"].toObject()["typing"];
}

} // namespace

class CommandInputTest : public QObject {
    Q_OBJECT

    QTemporaryDir dir_;
    std::unique_ptr<settings::Store> store_;
    std::unique_ptr<ui::UiExecutor> executor_;
    std::unique_ptr<theme::ThemeManager> tm_;

private slots:
    void initTestCase()
    {
        QVERIFY(dir_.isValid());
        store_ = std::make_unique<settings::Store>(dir_.filePath(u"user.json"_s));
        settings::Store::setInstance(store_.get());
        executor_ = std::make_unique<ui::UiExecutor>();
        QVERIFY(theme::loadFonts());
        tm_ = std::make_unique<theme::ThemeManager>(*store_);
    }

    void init() { store_->reset(ui::kImeBypass); }

    void cleanupTestCase()
    {
        probe::disable();
        tm_.reset();
        settings::Store::setInstance(nullptr);
        store_.reset();
        sched::waitIdle();
        QCoreApplication::processEvents();
        executor_.reset();
    }

    void monoFont()
    {
        CommandInput w;
        QCOMPARE(w.font().families(), theme::font(theme::TextRole::Mono).families());
        QVERIFY(!w.hasFrame());
        QVERIFY(!w.accessibleName().isEmpty());
    }

    // Tiêu chí 1: 134 chữ × Telex, VNI × 4 cách đưa chữ của bộ gõ → NFC đúng 100%.
    void all134Characters()
    {
        QSet<QChar> distinct;
        for (const QChar c : kLower)
            distinct.insert(c);
        QCOMPARE(kLower.size(), 67);
        QCOMPARE(distinct.size(), 67);
        const QString all = kLower + kLower.toUpper();
        QCOMPARE(nfc(all), all);

        CommandInput w;
        int ok = 0, total = 0;
        for (const Method m : {Method::Telex, Method::Vni})
            for (const Style s : {Style::PreeditNfc, Style::PreeditNfd, Style::Replace, Style::LoneMark})
                for (const QChar ch : all) {
                    w.clear();
                    const QString keys = keysFor(ch, m);
                    QCOMPARE(type(w, keys, m, s), QString(ch)); // bộ gõ giả tạo đúng chữ
                    ++total;
                    if (w.text() == QString(ch))
                        ++ok;
                    else
                        qWarning() << "sai:" << keys << int(m) << int(s) << w.text() << nfd(w.text()).toUcs4();
                }
        QCOMPARE(total, 134 * 2 * 4);
        QCOMPARE(ok, total);
    }

    // Preedit hiện tại chỗ (giữa văn bản), commit thay preedit; bộ gõ không gửi định dạng thì ô tự gạch chân.
    void preeditInPlace()
    {
        CommandInput w;
        w.insert(u"ab"_s);
        w.setCursorPosition(1);
        type(w, u"aas"_s, Method::Telex, Style::PreeditNfd, false);
        QCOMPARE(w.text(), u"aấb"_s); // i18n-ok: chữ mẫu
        QCOMPARE(w.cursorPosition(), 2);

        // Ô không focus (không vẽ con trỏ): preedit không định dạng khác chữ đã commit đúng ở nét gạch chân.
        CommandInput pre, plain;
        for (CommandInput* x : {&pre, &plain})
            x->resize(200, 24);
        QInputMethodEvent e(u"xy"_s, {});
        QCoreApplication::sendEvent(&pre, &e);
        plain.setText(u"xy"_s); // i18n-ok: chữ mẫu của test
        QVERIFY(pre.text().isEmpty());
        QVERIFY(pre.grab().toImage() != plain.grab().toImage());
    }

    // Tiêu chí 2: chờ tên lệnh thì bộ gõ bị bỏ qua (Telex lẫn VNI), chữ ra hoa theo phím vật lý.
    void awaitingCommandBypassesIme_data()
    {
        QTest::addColumn<QString>("keys");
        QTest::addColumn<QString>("bypassed");
        QTest::addColumn<QString>("vietnamese");
        // i18n-ok: chữ mẫu
        QTest::newRow("lm") << u"lm"_s << u"LM"_s << u"lm"_s;
        QTest::newRow("dd") << u"dd"_s << u"DD"_s << u"đ"_s;
        QTest::newRow("aa") << u"aa"_s << u"AA"_s << u"â"_s;
        QTest::newRow("ow") << u"ow"_s << u"OW"_s << u"ơ"_s; // i18n-ok: chữ mẫu
    }
    void awaitingCommandBypassesIme()
    {
        QFETCH(QString, keys);
        QFETCH(QString, bypassed);
        QFETCH(QString, vietnamese);
        CommandInput w;
        w.setAwaitingCommand(true);
        QVERIFY(w.imeBypassed());
        QVERIFY(!imEnabled(w));
        type(w, keys, Method::Telex, Style::PreeditNfc);
        QCOMPARE(w.text(), bypassed);

        // Tắt tùy chọn: áp ngay, bộ gõ hoạt động như mọi ô nhập.
        w.clear();
        store_->set(ui::kImeBypass, false);
        QVERIFY(!w.imeBypassed());
        QVERIFY(imEnabled(w));
        type(w, keys, Method::Telex, Style::PreeditNfc);
        QCOMPARE(w.text(), vietnamese);

        // Bật lại tùy chọn; hết chờ tên lệnh (trong lệnh) thì gõ có dấu.
        w.clear();
        store_->set(ui::kImeBypass, true);
        QVERIFY(!imEnabled(w));
        w.setAwaitingCommand(false);
        QVERIFY(imEnabled(w));
        type(w, keys, Method::Telex, Style::Replace);
        QCOMPARE(w.text(), vietnamese);
    }

    // Phím tắt (Ctrl+A) giữ nguyên khi bỏ qua bộ gõ; chữ hoa sẵn, số, dấu câu không đổi.
    void bypassKeepsShortcuts()
    {
        CommandInput w;
        w.setAwaitingCommand(true);
        QTest::keyClicks(&w, u"'z1._x"_s);
        QCOMPARE(w.text(), u"'Z1._X"_s);
        QTest::keyClick(&w, Qt::Key_A, Qt::ControlModifier);
        QCOMPARE(w.selectedText(), w.text());
    }

    // Tiêu chí 3: 10.000 chuỗi phím ngẫu nhiên qua ô (bỏ qua bộ gõ, Telex, VNI, mọi cách đưa chữ): ô giữ đúng chuỗi
    // phím (bỏ qua bộ gõ) hoặc đúng chữ bộ gõ tạo ra, không mất, không lặp. Ô không phụ thuộc thời gian giữa các phím;
    // thứ tự sự kiện ở 30 phím/s (bộ gõ kiểu hook, Windows) kiểm ở ime_bypass_test.
    void randomSequences()
    {
        std::mt19937 rng(21102026);
        const QString keys = u"adeowsfrjx126789lmnuiy"_s;
        CommandInput w;
        qint64 typed = 0;
        // Sanitizer làm QLineEdit chậm ~100 lần (bộ test phải < 60 s): ở đó chạy 1.000 chuỗi.
        const int rounds = test::kSanitized ? 1'000 : 10'000;
        for (int round = 0; round < rounds; ++round) {
            QString seq;
            for (int n = std::uniform_int_distribution<int>(1, 10)(rng); n > 0; --n)
                seq += keys[std::uniform_int_distribution<int>(0, int(keys.size()) - 1)(rng)];
            const bool bypass = round % 3 == 0;
            const auto m = Method(round % 2);
            const auto s = Style(std::uniform_int_distribution<int>(0, 3)(rng));
            w.setAwaitingCommand(bypass);
            w.clear();
            const QString made = type(w, seq, m, s);
            const QString expect = bypass ? seq.toUpper() : made;
            QVERIFY2(w.text() == expect,
                     qPrintable(u"vòng %1: phím '%2', mong '%3', ô '%4'"_s.arg(round).arg(seq, expect, w.text())));
            typed += seq.size();
        }
        qInfo("%d chuỗi, %lld phím: không mất, không lặp", rounds, typed);
    }

    // Tiêu chí 4: ký tự hiện trong ô ≤ 16 ms p99 sau phím (UiProbe: từ phím tới QWindow đến hết khung kế tiếp).
    // Đường bộ gõ (QInputMethodEvent không phải phím) đo bằng untilFrame từ lúc gửi sự kiện.
    void typingLatency()
    {
        CommandInput w;
        w.resize(400, 24);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        probe::enable();
        for (const bool bypass : {true, false}) {
            probe::reset();
            w.clear();
            w.setAwaitingCommand(bypass);
            constexpr int kN = 300;
            for (int i = 0; i < kN; ++i) {
                if (i % 60 == 0)
                    w.clear();
                if (bypass) {
                    QTest::keyClick(w.windowHandle(), char('a' + i % 26));
                } else {
                    const auto start = probe::Clock::now();
                    QInputMethodEvent e;
                    e.setCommitString(QString(kLower[i % kLower.size()]));
                    QCoreApplication::sendEvent(&w, &e);
                    probe::untilFrame(probe::Op::Typing, &w, start);
                }
                QVERIFY(spinUntil([&] { return typing()["count"].toInt() == i + 1; }));
            }
            const double p50 = typing()["p50_ms"].toDouble(), p99 = typing()["p99_ms"].toDouble();
            qInfo("typing %s: p50 %.2f ms, p99 %.2f ms (%d mẫu)", bypass ? "bỏ qua bộ gõ (phím)" : "bộ gõ (commit)",
                  p50, p99, kN);
            QVERIFY2(!test::kTimed || p99 <= 16.0, qPrintable(QString::number(p99)));
        }
        probe::disable();
    }
};

QTEST_MAIN(CommandInputTest)
#include "command_input_test.moc"
