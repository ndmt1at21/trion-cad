// UIA-108 · Tự hoàn thành, gợi ý khi gõ sai, AutoCorrect (Autocomplete.h, nối vào CommandLine). Nhóm theo tiêu chí:
//   1  latency: chỉ mục ≥ 3.000 tên (lệnh, viết tắt, ≥ 250 biến, 2.500 lệnh LISP giả); mỗi phím: CPU luồng UI từ phím
//      tới lần vẽ danh sách, p99 ≤ 16 ms ở trạng thái ổn định (kiểm ở release; lượt gõ đầu raster glyph, biểu tượng lần
//      đầu và p50/p99 thời gian thật của UiProbe chỉ báo).
//   2  typos100: 100 lỗi gõ mẫu (25 sai, 25 thiếu, 25 thừa, 25 đảo một ký tự) → lệnh đúng có trong ≤ 3 gợi ý ở ≥ 95 ca;
//      unknownLine: dòng "Không có lệnh “LNE”. Ý bạn là: LINE, …, LEN?" có gợi ý bấm được; autoCorrect: chọn cùng gợi ý
//      3 lần thì lỗi gõ tự sửa, xem và xóa được.
//   3  frequency: sau 20 lần dùng LEN, gõ "le" đưa LENGTHEN (LEN) lên đầu (Enter vẫn là "LE" gõ đúng); còn sau khi đọc
//      lại Store từ tệp và dựng dòng lệnh mới.
//   4  optionsApplyNow: tắt "tìm biến", "tìm theo tiếng Việt" thì danh sách đang mở đổi ngay.
//   Còn lại: vietnameseKeywords ("cắt" → TRIM, "bo góc" → FILLET, cả qua bộ gõ khi danh sách mở), ranking (giữa chuỗi
//   không chọn sẵn, biến, LISP, khóa phiên bản), keyboardOnly (↑↓ vòng, Tab, Enter, Esc, chuột; focus không rời ô
//   nhập), geometry (vị trí, cỡ, nền dòng chọn theo token), pseudoNoTruncation (vi-XL: 0 chữ cắt).
#include "app/shell/cmdline/Autocomplete.h"

#include "app/core/actions/ActionRegistry.h"
#include "app/core/actions/SearchTerms.h"
#include "app/core/i18n/Translator.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/core/perf/UiProbe.h"
#include "app/core/UiExecutor.h"
#include "app/shell/cmdline/CommandLine.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/cmdline/CommandInput.h"
#include "app/widgets/tooltip/RichTooltip.h"
#include "core/base/sched/Scheduler.h"
#include "tests/timing.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QBoxLayout>
#include <QElapsedTimer>
#include <QInputMethodEvent>
#include <QJsonObject>
#include <QLineEdit>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <random>
#include <vector>

#if defined(__linux__)
#include <ctime>
#endif

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;
using namespace trion;
using trion::ui::Autocomplete;
using trion::ui::CommandLine;
namespace probe = trion::ui::probe;

namespace {

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

// Thời gian CPU của luồng gọi (Linux); nơi khác -1: số đo "nan", không kiểm (như commandline_test).
std::chrono::nanoseconds threadCpu()
{
#if defined(__linux__)
    timespec t{};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
    return std::chrono::seconds(t.tv_sec) + std::chrono::nanoseconds(t.tv_nsec);
#else
    return std::chrono::nanoseconds(-1);
#endif
}
double cpuMs(std::chrono::nanoseconds d)
{
    return threadCpu().count() >= 0 ? std::chrono::duration<double, std::milli>(d).count() : qQNaN();
}

QJsonObject probeOp(const char* name)
{
    return probe::toJson()["ops"].toObject()[QLatin1StringView(name)].toObject();
}

QString truncations(QWidget* root)
{
    QStringList out;
    for (const i18n::Truncation& t : i18n::findTruncations(root))
        out << t.toString();
    return out.join(u"; "_s);
}

// Đếm lần vẽ của một widget.
struct Paints : QObject {
    int count = 0;
    bool eventFilter(QObject*, QEvent* e) override
    {
        if (e->type() == QEvent::Paint)
            ++count;
        return false;
    }
};

// 100 lỗi gõ mẫu [đề xuất của tiêu chí 2]: lỗi gõ → lệnh đúng.
struct Typo {
    const char* typed;
    const char* command;
};
// clang-format off
constexpr Typo kTypos[] = {
    // sai một ký tự
    {"LIBE", "LINE"}, {"CIRCKE", "CIRCLE"}, {"RECTSNG", "RECTANG"}, {"POLYGIN", "POLYGON"}, {"ELLIPDE", "ELLIPSE"},
    {"HATVH", "HATCH"}, {"SPLUNE", "SPLINE"}, {"MTEXR", "MTEXT"}, {"MOBE", "MOVE"}, {"COPT", "COPY"},
    {"ROTSTE", "ROTATE"}, {"MIRTOR", "MIRROR"}, {"STRETVH", "STRETCH"}, {"SCSLE", "SCALE"}, {"TRUM", "TRIM"},
    {"EXTWND", "EXTEND"}, {"FIKLET", "FILLET"}, {"CHAMGER", "CHAMFER"}, {"ARTAY", "ARRAY"}, {"ERAAE", "ERASE"},
    {"EXPLIDE", "EXPLODE"}, {"OFFAET", "OFFSET"}, {"LAYEE", "LAYER"}, {"MATCHPTOP", "MATCHPROP"}, {"INSERY", "INSERT"},
    // thiếu một ký tự
    {"LNE", "LINE"}, {"CIRLE", "CIRCLE"}, {"RECTNG", "RECTANG"}, {"POLYON", "POLYGON"}, {"ELIPSE", "ELLIPSE"},
    {"HTCH", "HATCH"}, {"SPLIN", "SPLINE"}, {"MTXT", "MTEXT"}, {"MVE", "MOVE"}, {"CPY", "COPY"},
    {"ROTAE", "ROTATE"}, {"MIROR", "MIRROR"}, {"STRECH", "STRETCH"}, {"SCLE", "SCALE"}, {"TIM", "TRIM"},
    {"EXTED", "EXTEND"}, {"FILET", "FILLET"}, {"CHAMER", "CHAMFER"}, {"ARAY", "ARRAY"}, {"ERSE", "ERASE"},
    {"EXPODE", "EXPLODE"}, {"OFSET", "OFFSET"}, {"LAER", "LAYER"}, {"DIMLNEAR", "DIMLINEAR"},
    {"PROPERTES", "PROPERTIES"},
    // thừa một ký tự
    {"LINEE", "LINE"}, {"CIRCCLE", "CIRCLE"}, {"RECTTANG", "RECTANG"}, {"POLLYGON", "POLYGON"}, {"ELLLIPSE", "ELLIPSE"},
    {"HATCHH", "HATCH"}, {"SPPLINE", "SPLINE"}, {"MTEXXT", "MTEXT"}, {"MOOVE", "MOVE"}, {"COPPY", "COPY"},
    {"ROTTATE", "ROTATE"}, {"MIRRORR", "MIRROR"}, {"STRETCHH", "STRETCH"}, {"SCALLE", "SCALE"}, {"TRIMM", "TRIM"},
    {"EXTTEND", "EXTEND"}, {"FILLLET", "FILLET"}, {"CHAMFFER", "CHAMFER"}, {"ARRRAY", "ARRAY"}, {"ERASSE", "ERASE"},
    {"EXPLOODE", "EXPLODE"}, {"OFFSETT", "OFFSET"}, {"LAYYER", "LAYER"}, {"ZOOOM", "ZOOM"}, {"REGENN", "REGEN"},
    // đảo hai ký tự kề
    {"LNIE", "LINE"}, {"CIRLCE", "CIRCLE"}, {"RECTAGN", "RECTANG"}, {"POLYOGN", "POLYGON"}, {"ELLIPES", "ELLIPSE"},
    {"HACTH", "HATCH"}, {"SPILNE", "SPLINE"}, {"MTETX", "MTEXT"}, {"MVOE", "MOVE"}, {"CPOY", "COPY"},
    {"ROTAET", "ROTATE"}, {"MIRORR", "MIRROR"}, {"STERTCH", "STRETCH"}, {"SCAEL", "SCALE"}, {"TIRM", "TRIM"},
    {"EXTNED", "EXTEND"}, {"FILELT", "FILLET"}, {"CHAMFRE", "CHAMFER"}, {"ARRYA", "ARRAY"}, {"EARSE", "ERASE"},
    {"EXPOLDE", "EXPLODE"}, {"OFFEST", "OFFSET"}, {"LAYRE", "LAYER"}, {"ZOMO", "ZOOM"}, {"REGNE", "REGEN"},
};
// clang-format on
static_assert(std::size(kTypos) == 100);

} // namespace

class AutocompleteTest : public QObject {
    Q_OBJECT

    QTemporaryDir dir_;
    std::unique_ptr<settings::Store> store_;
    std::unique_ptr<ui::UiExecutor> executor_;
    std::unique_ptr<theme::ThemeManager> tm_;
    std::unique_ptr<theme::IconProvider> icons_;
    std::unique_ptr<i18n::Translator> translator_;
    std::unique_ptr<QWidget> window_;
    QLineEdit* before_ = nullptr; // ô đứng trước dòng lệnh theo thứ tự Tab
    CommandLine* cmd_ = nullptr;
    Autocomplete* ac_ = nullptr;
    QStringList tokens_;

    // Gõ từng ký tự vào ô nhập như bàn phím ('\n' là Enter).
    void type(const QString& keys)
    {
        for (const QChar c : keys) {
            if (c == u'\n')
                QTest::keyClick(cmd_->input(), Qt::Key_Return);
            else
                QTest::keyClick(cmd_->input(), c.toLatin1());
        }
    }
    void key(Qt::Key k, Qt::KeyboardModifiers m = Qt::NoModifier) { QTest::keyClick(cmd_->input(), k, m); }
    // Dòng của danh sách đang hiện.
    QStringList rows() const
    {
        QStringList out;
        const QAbstractItemModel* m = ac_->view()->model();
        for (int r = 0; r < m->rowCount(); ++r)
            out << m->index(r, 0).data().toString();
        return out;
    }
    QString current() const { return ac_->view()->currentIndex().data().toString(); }
    void retype(const QString& keys) // ô trống rồi gõ
    {
        cmd_->input()->clear();
        type(keys);
    }

private slots:
    void initTestCase()
    {
        QVERIFY(dir_.isValid());
        store_ = std::make_unique<settings::Store>(dir_.filePath(u"user.json"_s));
        settings::Store::setInstance(store_.get());
        QVERIFY(theme::loadFonts());
        executor_ = std::make_unique<ui::UiExecutor>();
        tm_ = std::make_unique<theme::ThemeManager>(*store_);
        icons_ = std::make_unique<theme::IconProvider>();
        translator_ = std::make_unique<i18n::Translator>();
        QApplication::setCursorFlashTime(0);

        window_ = std::make_unique<QWidget>();
        auto* column = new QVBoxLayout(window_.get());
        column->setContentsMargins({});
        column->setSpacing(0);
        before_ = new QLineEdit;
        cmd_ = new CommandLine;
        ac_ = cmd_->autocomplete();
        column->addWidget(before_);
        column->addStretch(1); // chỗ vùng vẽ: danh sách mở lên trên dòng lệnh
        column->addWidget(cmd_);
        window_->resize(1366, 640);
        window_->show();
        QVERIFY(QTest::qWaitForWindowExposed(window_.get()));
        connect(cmd_, &CommandLine::submitted, this, [this](const QString& t) { tokens_ << t; });
    }

    void init()
    {
        tm_->setTheme(theme::Theme::Dark);
        tm_->setDensity(theme::Density::Compact);
        for (const auto* s : {&ui::kUsage, &ui::kAutoCorrect})
            store_->reset(*s);
        store_->reset(ui::kSearchSysvars);
        store_->reset(ui::kSearchVietnamese);
        store_->reset(ui::kImeBypass);
        cmd_->setEnabled(true);
        cmd_->clearPrompt();
        cmd_->input()->clear();
        ac_->setExtraCommands({});
        ac_->setLockQuery({});
        tokens_.clear();
        window_->activateWindow();
        cmd_->setFocus(Qt::OtherFocusReason);
        QVERIFY(spinUntil([&] { return QApplication::focusWidget() == cmd_->input(); }));
    }

    void cleanupTestCase()
    {
        probe::disable();
        window_.reset();
        translator_.reset();
        icons_.reset();
        tm_.reset();
        settings::Store::setInstance(nullptr);
        store_.reset();
        sched::waitIdle();
        QCoreApplication::processEvents();
        executor_.reset();
    }

    // Tiêu chí 1: chỉ mục ≥ 3.000 tên; danh sách cập nhật ≤ 16 ms p99 CPU sau mỗi phím (kiểm ở release).
    void latency()
    {
        int aliases = 0, commands = 0;
        for (const actions::SearchTerms& t : actions::searchTerms()) {
            ++commands;
            aliases += int(QLatin1StringView(t.aliases).count(u',')) - (*t.aliases ? 1 : 0);
        }
        const int base = ac_->nameCount(), sysvars = base - commands - aliases;
        QVERIFY2(sysvars >= 250, qPrintable(QString::number(sysvars)));
        std::mt19937 rng(108);
        QList<Autocomplete::Extra> lisp;
        for (int i = 0; i < 2500; ++i) { // tên 3–9 chữ: nhiều tên trùng tiền tố với lệnh thật
            QString name;
            for (int n = 3 + int(rng() % 7); n > 0; --n)
                name += QChar(u'A' + int(rng() % 26));
            lisp.append({name, {}});
        }
        QElapsedTimer build;
        build.start();
        ac_->setExtraCommands(lisp);
        const int size = ac_->nameCount();
        const double buildMs = double(build.nsecsElapsed()) / 1e6;
        QVERIFY2(size >= 3000, qPrintable(QString::number(size)));

        // Gõ dần rồi xóa dần các chuỗi mẫu: tên, viết tắt, từ khóa không dấu, biến, giữa chuỗi, lỗi gõ.
        const QStringList words{u"line"_s,      u"circle"_s, u"cat"_s,   u"bogoc"_s,  u"orthomode"_s,
                                u"dimlinear"_s, u"lnie"_s,   u"ctan"_s,  u"zoom"_s,   u"layer"_s,
                                u"rec"_s,       u"hatch"_s,  u"qsave"_s, u"abcdefg"_s};
        Paints paints;
        ac_->view()->viewport()->installEventFilter(&paints);
        std::vector<double> cpu;
        const auto press = [&](const std::function<void()>& send) {
            const int before = paints.count;
            const auto start = threadCpu();
            send();
            if (!ac_->isVisible())
                return; // danh sách đóng (ô trống, không gợi ý): không có lần vẽ để đo
            QVERIFY(spinUntil([&] { return paints.count > before; }));
            cpu.push_back(cpuMs(threadCpu() - start));
        };
        const auto pass = [&] {
            for (const QString& w : words) {
                for (const QChar c : w)
                    press([&] { QTest::keyClick(window_->windowHandle(), c.toLatin1()); });
                for (qsizetype i = 0; i < w.size(); ++i)
                    press([&] { QTest::keyClick(window_->windowHandle(), Qt::Key_Backspace); });
            }
        };
        // Lượt đầu: danh sách hiện lần đầu, glyph và biểu tượng mới raster (một lần mỗi phiên): chỉ báo. Hai lượt sau
        // (trạng thái ổn định): kiểm p99.
        pass();
        const double first = cpu.empty() ? qQNaN() : *std::ranges::max_element(cpu);
        cpu.clear();
        probe::enable();
        probe::reset();
        pass();
        pass();
        ac_->view()->viewport()->removeEventFilter(&paints);
        QVERIFY(cpu.size() > 150);
        std::ranges::sort(cpu);
        const double p50 = cpu[cpu.size() / 2], p99 = cpu[std::size_t(std::ceil(0.99 * double(cpu.size()))) - 1];
        QVERIFY(spinUntil([&] { return probeOp("autocomplete")["count"].toInt() >= int(cpu.size()); }));
        qInfo("chỉ mục %d tên (%d lệnh, %d viết tắt, %d biến, 2.500 LISP giả), dựng lại %.2f ms", size, commands,
              aliases, sysvars, buildMs);
        qInfo("autocomplete: CPU mỗi phím p50 %.2f ms, p99 %.2f ms, max %.2f ms (%zu phím; lượt đầu max %.2f ms); "
              "UiProbe p50 %.2f ms, p99 %.2f ms",
              p50, p99, cpu.back(), cpu.size(), first, probeOp("autocomplete")["p50_ms"].toDouble(),
              probeOp("autocomplete")["p99_ms"].toDouble());
        probe::disable();
        QVERIFY2(!test::kTimed || qIsNaN(p99) || p99 <= 16.0, qPrintable(QString::number(p99)));
        QCOMPARE(QApplication::focusWidget(), cmd_->input());
        QVERIFY(tokens_.isEmpty());
    }

    // Tiêu chí 2: 100 lỗi gõ mẫu → lệnh đúng trong gợi ý (≤ 3) ở ≥ 95 ca.
    void typos100()
    {
        int hit = 0;
        QStringList missed;
        for (const Typo& t : kTypos) {
            const QStringList terms = ac_->suggest(QString::fromLatin1(t.typed));
            QVERIFY(terms.size() <= Autocomplete::kMaxSuggestions);
            QStringList resolved;
            for (const QString& term : terms)
                resolved << ac_->resolve(term);
            if (resolved.contains(QLatin1StringView(t.command)))
                ++hit;
            else
                missed << u"%1 → %2 (%3)"_s.arg(QLatin1StringView(t.typed), QLatin1StringView(t.command),
                                                terms.join(u','));
        }
        qInfo("gõ sai: %d/100 ca có lệnh đúng trong gợi ý; trượt: %s", hit, qPrintable(missed.join(u"; "_s)));
        QVERIFY2(hit >= 95, qPrintable(missed.join(u"; "_s)));
        QCOMPARE(ac_->suggest(u"LNE"_s).mid(0, 1), QStringList{u"LINE"_s}); // 07.1 §8: LINE, …, LEN
        QVERIFY(ac_->suggest(u"LNE"_s).contains(u"LEN"_s));
        QVERIFY(ac_->suggest(u"QWXZ"_s).isEmpty());
    }

    // Tiêu chí 2: "Không có lệnh “LNE”. Ý bạn là: LINE, …?" — gợi ý bấm được (rê: con trỏ bàn tay), bấm chạy lệnh qua
    // commandRequested và tính một lần chọn; không có gợi ý thì chỉ cách xem danh sách lệnh.
    void unknownLine()
    {
        QSignalSpy run(cmd_, &CommandLine::commandRequested);
        auto* history = cmd_->findChild<QAbstractScrollArea*>(u"history"_s);
        QWidget* port = history->viewport();
        Paints paints;
        port->installEventFilter(&paints);
        cmd_->reportUnknown(u"lne"_s);
        QVERIFY(spinUntil([&] { return paints.count > 0; })); // chip có chỗ sau lần vẽ
        const QString line = cmd_->historyLine(cmd_->historyCount() - 1);
        const QString head = QCoreApplication::translate("trion::ui::CommandLine", "Không có lệnh “%1”.").arg(u"lne"_s);
        QVERIFY2(line.startsWith(head) && line.contains(u": LINE, "_s) && line.contains(u"LEN"_s) &&
                     line.endsWith(u'?'),
                 qPrintable(line));
        const int h = qRound(tm_->px(tok::h_cmdl)), count = cmd_->historyCount();
        const int y = (count - 1 - std::max(0, count - port->height() / h)) * h + h / 2; // giữa dòng mới nhất
        int x = 0;
        // QTest::mouseMove(QWidget*) không nút chỉ đặt QCursor::setPos (offscreen không sinh sự kiện): gửi qua cửa sổ.
        const auto hover = [&](QPoint at) {
            QTest::mouseMove(window_->windowHandle(), port->mapTo(window_.get(), at));
        };
        for (; x < port->width(); x += 2) {
            hover({x, y});
            if (port->cursor().shape() == Qt::PointingHandCursor)
                break;
        }
        QVERIFY2(x < port->width(), "không thấy chip gợi ý");
        QTest::mouseClick(port, Qt::LeftButton, {}, {x + 4, y});
        QCOMPARE(run.count(), 1);
        QCOMPARE(run.at(0).at(0).toString(), u"^C^CLINE"_s);
        QCOMPARE(store_->get(ui::kAutoCorrect)[u"LNE"_s].toObject()[u"LINE"_s].toInt(), 1);
        hover({1, 1});
        QCOMPARE(port->cursor().shape(), Qt::ArrowCursor);
        port->removeEventFilter(&paints);

        cmd_->reportUnknown(u"QWXZ"_s);
        QVERIFY(cmd_->historyLine(cmd_->historyCount() - 1).contains(u"F1"_s));
    }

    // Tiêu chí 2 (P1): gõ sai không có dòng khớp → danh sách gợi ý, không chọn sẵn (Enter gửi chữ đã gõ); ↓ rồi Enter
    // chọn gợi ý; 3 lần cùng gợi ý → AutoCorrect (báo trong lịch sử), từ đó gõ sai là lệnh đúng; xem, xóa.
    void autoCorrect()
    {
        QSignalSpy learned(ac_, &Autocomplete::learned);
        retype(u"lnie"_s);
        QVERIFY(ac_->isVisible());
        QCOMPARE(rows().value(0), u"LINE"_s);
        QVERIFY(current().isEmpty());
        key(Qt::Key_Return);
        QCOMPARE(tokens_, QStringList{u"LNIE"_s});
        for (int i = 0; i < Autocomplete::kLearnAfter; ++i) {
            type(u"lnie"_s);
            key(Qt::Key_Down);
            QCOMPARE(current(), u"LINE"_s);
            key(Qt::Key_Return);
        }
        QCOMPARE(tokens_.mid(1), (QStringList{u"LINE"_s, u"LINE"_s, u"LINE"_s}));
        QCOMPARE(learned.count(), 1);
        QVERIFY(cmd_->historyLine(cmd_->historyCount() - 1).contains(u"LNIE"_s));
        QCOMPARE(ac_->corrections().size(), 1);
        QCOMPARE(ac_->corrections().front().typo, u"LNIE"_s);
        QCOMPARE(ac_->corrections().front().command, u"LINE"_s);
        tokens_.clear();
        type(u"lnie"_s);
        QCOMPARE(current(), u"LINE"_s); // lỗi gõ đã học như một viết tắt: chọn sẵn
        key(Qt::Key_Return);
        type(u"lnie "_s); // Space cũng vậy
        QCOMPARE(tokens_, (QStringList{u"LINE"_s, u"LINE"_s}));
        Autocomplete::removeCorrection(u"lnie"_s);
        QVERIFY(ac_->corrections().isEmpty());
        tokens_.clear();
        type(u"lnie\n"_s);
        QCOMPARE(tokens_, QStringList{u"LNIE"_s});
        // Tab nhận gợi ý đầu khi chưa chọn dòng nào (tính một lần chọn)
        tokens_.clear();
        type(u"cirlce"_s);
        key(Qt::Key_Tab);
        QCOMPARE(tokens_, QStringList{u"CIRCLE"_s});
        QCOMPARE(store_->get(ui::kAutoCorrect)[u"CIRLCE"_s].toObject()[u"CIRCLE"_s].toInt(), 1);
    }

    // Tiêu chí 3: 20 lần dùng LEN → "le" đưa LENGTHEN lên đầu; còn sau khi đọc lại Store và dựng dòng lệnh mới.
    void frequency()
    {
        QCOMPARE(ac_->search(u"le"_s).value(0), u"QLEADER"_s); // chưa dùng: viết tắt LE trùng chữ gõ đứng đầu
        for (int i = 0; i < 20; ++i)
            type(u"len\n"_s);
        QCOMPARE(tokens_.size(), 20);
        QCOMPARE(tokens_.front(), u"LEN"_s); // dòng trùng chữ gõ: gửi đúng chữ đã gõ
        QCOMPARE(store_->get(ui::kUsage)[u"LENGTHEN"_s].toInt(), 20);
        type(u"le"_s);
        QCOMPARE(rows().value(0), u"LENGTHEN"_s);
        QCOMPARE(current(), u"QLEADER"_s); // Enter vẫn là viết tắt LE gõ đúng (như AutoCAD)
        key(Qt::Key_Return);
        QCOMPARE(tokens_.back(), u"LE"_s);
        // Lệnh chạy từ ribbon (chỉ thấy prompt) cũng được tính; prompt kế của cùng lệnh thì không.
        cmd_->clearPrompt();
        cmd_->setPrompt({u"CIRCLE"_s, u"?"_s});
        cmd_->setPrompt({u"CIRCLE"_s, u"?"_s});
        cmd_->clearPrompt();
        QCOMPARE(store_->get(ui::kUsage)[u"CIRCLE"_s].toInt(), 1);

        store_->flush();
        settings::Store again(store_->filePath()); // như khởi động lại: đọc tệp
        settings::Store::setInstance(&again);
        {
            CommandLine fresh;
            QCOMPARE(fresh.autocomplete()->search(u"le"_s).value(0), u"LENGTHEN"_s);
        }
        settings::Store::setInstance(store_.get());
    }

    // Tiêu chí 4: tắt "tìm biến", "tìm theo tiếng Việt" → danh sách đang mở đổi ngay.
    void optionsApplyNow()
    {
        type(u"orthom"_s);
        QCOMPARE(rows(), QStringList{u"ORTHOMODE"_s});
        store_->set(ui::kSearchSysvars, false);
        QVERIFY(!ac_->isVisible() || !rows().contains(u"ORTHOMODE"_s));
        QVERIFY(!ac_->search(u"orthom"_s).contains(u"ORTHOMODE"_s));
        QVERIFY(!ac_->suggest(u"ORTHOMODF"_s).contains(u"ORTHOMODE"_s));
        store_->set(ui::kSearchSysvars, true);
        QCOMPARE(rows(), QStringList{u"ORTHOMODE"_s}); // bật lại cũng áp ngay

        retype(u"cat"_s);
        QCOMPARE(rows().value(0), u"TRIM"_s);
        store_->set(ui::kSearchVietnamese, false);
        QVERIFY(!ac_->isVisible() || !rows().contains(u"TRIM"_s));
        QVERIFY(!ac_->search(u"bo góc"_s).contains(u"FILLET"_s));
        store_->set(ui::kSearchVietnamese, true);
        QCOMPARE(rows().value(0), u"TRIM"_s);
    }

    // "cắt" → TRIM, "bo góc" → FILLET (có dấu, không dấu, chữ hoa); qua bộ gõ khi danh sách đang mở: focus, bộ gõ vẫn ở
    // ô nhập, Enter chạy dòng chọn sẵn.
    void vietnameseKeywords()
    {
        QCOMPARE(ac_->search(u"cắt"_s).value(0), u"TRIM"_s);
        QCOMPARE(ac_->search(u"CAT"_s).value(0), u"TRIM"_s);
        QCOMPARE(ac_->search(u"bo góc"_s).value(0), u"FILLET"_s);
        QCOMPARE(ac_->search(u"BO GOC"_s).value(0), u"FILLET"_s);
        QCOMPARE(ac_->completion(u"cắt"_s), u"TRIM"_s);

        store_->set(ui::kImeBypass, false); // gõ tiếng Việt bằng bộ gõ ở dấu nhắc lệnh
        type(u"c"_s);
        QVERIFY(ac_->isVisible());
        QInputMethodEvent preedit(u"că"_s, {}); // i18n-ok: dữ liệu thử
        QApplication::sendEvent(cmd_->input(), &preedit);
        QInputMethodEvent commit;
        commit.setCommitString(u"cắt"_s, -1, 1); // thay "c" bằng "cắt" như bộ gõ Telex
        QApplication::sendEvent(cmd_->input(), &commit);
        QCOMPARE(cmd_->input()->text(), u"cắt"_s);
        QCOMPARE(QApplication::focusWidget(), cmd_->input());
        QVERIFY(ac_->isVisible());
        QCOMPARE(current(), u"TRIM"_s);
        key(Qt::Key_Return);
        QCOMPARE(tokens_, QStringList{u"TRIM"_s});
    }

    // Xếp hạng, loại dòng: giữa chuỗi chỉ gợi ý (không chọn sẵn), biến, lệnh LISP, khóa phiên bản, tọa độ không mở danh
    // sách, prompt đang chạy thì không tự hoàn thành.
    void ranking()
    {
        QCOMPARE(ac_->search(u"ctan"_s), (QStringList{u"RECTANG"_s, u"GCTANGENT"_s})); // tìm giữa chuỗi
        QVERIFY(ac_->completion(u"ctan"_s).isEmpty());
        type(u"ctan\n"_s);
        QCOMPARE(tokens_, QStringList{u"CTAN"_s});
        QCOMPARE(ac_->completion(u"rect"_s), u"RECTANG"_s);
        QCOMPARE(ac_->completion(u"c"_s), u"c"_s); // viết tắt trùng: gửi đúng chữ gõ
        QCOMPARE(ac_->search(u"'zoo"_s).value(0), u"ZOOM"_s);
        for (const QString& s : {u"100,200"_s, u"@10<45"_s, u"12.5"_s, u"-5"_s, u"(setq a 1)"_s, u"#1,2"_s})
            QVERIFY2(ac_->search(s).isEmpty() && ac_->completion(s).isEmpty(), qPrintable(s));

        tokens_.clear();
        type(u"lwdisp"_s); // biến: biểu tượng bánh răng, mô tả "Biến hệ thống"
        QCOMPARE(current(), u"LWDISPLAY"_s);
        const QString sysvar = QCoreApplication::translate("trion::ui::Autocomplete", "Biến hệ thống");
        QVERIFY(ac_->view()->currentIndex().data(Qt::AccessibleTextRole).toString().contains(sysvar));
        key(Qt::Key_Return);
        QCOMPARE(tokens_, QStringList{u"LWDISPLAY"_s});

        ac_->setExtraCommands({{u"khungten"_s, u"Vẽ khung tên"_s}, {u"line"_s, {}}}); // i18n-ok: dữ liệu thử
        QCOMPARE(ac_->search(u"khung"_s).value(0), u"KHUNGTEN"_s);
        QVERIFY(ac_->search(u"khung ten"_s).contains(u"KHUNGTEN"_s)); // mô tả, giữa chuỗi
        QCOMPARE(ac_->resolve(u"KhungTen"_s), u"KHUNGTEN"_s);
        QCOMPARE(ac_->search(u"line"_s).count(u"LINE"_s), 1); // trùng tên lệnh: bỏ
        ac_->setExtraCommands({});

        ac_->setLockQuery([](const QString& c) { return c == u"LINE"_s ? ui::Edition::Pro : ui::Edition::None; });
        retype(u"l"_s);
        QCOMPARE(current(), u"LINE"_s);
        QVERIFY(ac_->view()
                    ->currentIndex()
                    .data(Qt::AccessibleTextRole)
                    .toString()
                    .endsWith(ui::RichTooltip::editionName(ui::Edition::Pro)));
        QCOMPARE(ac_->view()->visualRect(ac_->view()->currentIndex()).height(), 26); // .ai height

        cmd_->setPrompt({u"LINE"_s, u"?"_s});
        QVERIFY(!ac_->isVisible());
        type(u"c"_s);
        QVERIFY(!ac_->isVisible());
        cmd_->clearPrompt();
        retype(u"l"_s);
        cmd_->setEnabled(false);
        QVERIFY(!ac_->isVisible());
    }

    // DoD-U bàn phím: Tab tới ô nhập; ↑↓ chọn (vòng), Tab/Enter nhận, Esc đóng (giữ chữ; Esc nữa mới xóa và bỏ vùng
    // chọn); bấm chuột vào dòng nhận dòng đó; focus không rời ô nhập; Shift+Tab ra lại.
    void keyboardOnly()
    {
        QSignalSpy deselect(cmd_, &CommandLine::deselectRequested);
        before_->setFocus();
        QVERIFY(spinUntil([&] { return before_->hasFocus(); }));
        QTest::keyClick(before_, Qt::Key_Tab);
        QVERIFY(spinUntil([&] { return cmd_->input()->hasFocus(); }));
        type(u"circ"_s);
        QCOMPARE(current(), u"CIRCLE"_s);
        const int n = int(rows().size());
        QVERIFY(n >= 2);
        key(Qt::Key_Up);
        QCOMPARE(ac_->view()->currentIndex().row(), n - 1); // vòng lên dòng cuối
        key(Qt::Key_Down);
        key(Qt::Key_Down);
        QCOMPARE(ac_->view()->currentIndex().row(), 1);
        key(Qt::Key_Up);
        key(Qt::Key_Tab);
        QCOMPARE(tokens_, QStringList{u"CIRCLE"_s});
        QCOMPARE(QApplication::focusWidget(), cmd_->input());

        type(u"circ"_s);
        key(Qt::Key_Escape);
        QVERIFY(!ac_->isVisible());
        QCOMPARE(cmd_->input()->text(), u"CIRC"_s);
        QCOMPARE(deselect.count(), 0);
        key(Qt::Key_Escape);
        QVERIFY(cmd_->input()->text().isEmpty());
        QCOMPARE(deselect.count(), 1);

        type(u"zo"_s);
        const QModelIndex zoom = ac_->view()->model()->index(int(rows().indexOf(u"ZOOM"_s)), 0);
        QVERIFY(zoom.isValid());
        QTest::mouseClick(ac_->view()->viewport(), Qt::LeftButton, {}, ac_->view()->visualRect(zoom).center());
        QCOMPARE(tokens_.back(), u"ZOOM"_s);
        QVERIFY(!ac_->isVisible());
        QCOMPARE(QApplication::focusWidget(), cmd_->input());

        type(u"li"_s); // bấm ra ngoài (focus sang ô khác): đóng
        QVERIFY(ac_->isVisible());
        before_->setFocus();
        QVERIFY(spinUntil([&] { return !ac_->isVisible(); }));
        QTest::keyClick(before_, Qt::Key_Tab);
        QVERIFY(spinUntil([&] { return cmd_->input()->hasFocus(); }));
        QTest::keyClick(cmd_->input(), Qt::Key_Backtab);
        QVERIFY(spinUntil([&] { return before_->hasFocus(); }));
    }

    // Vị trí (.ac: trái dòng lệnh + --sp-3, đáy cách đỉnh dòng lệnh --sp-1), cỡ (≥ 520 px, ≤ 300 px, dòng 26 px), nền
    // dòng chọn --c-accent-soft khác nền --c-raised của dòng khác, ở Tối và Sáng.
    void geometry()
    {
        for (const theme::Theme t : {theme::Theme::Dark, theme::Theme::Light}) {
            tm_->setTheme(t);
            retype(u"l"_s);
            QVERIFY(ac_->isVisible());
            QCOMPARE(ac_->parentWidget(), window_.get());
            const QPoint top = cmd_->mapTo(window_.get(), QPoint());
            const QRect g = ac_->geometry();
            QCOMPARE(g.left(), top.x() + qRound(tm_->px(tok::sp_3)));
            QCOMPARE(g.bottom() + 1 + qRound(tm_->px(tok::sp_1)), top.y());
            QVERIFY(g.width() >= 520 && g.height() <= 300);
            QCOMPARE(current(), u"LINE"_s);
            QCOMPARE(ac_->view()->currentIndex().row(), 0);
            const QImage shot = ac_->grab().toImage();
            const QPoint at = ac_->view()->mapTo(ac_, QPoint(2, 26 / 2));
            const QColor hot = shot.pixelColor(at), cold = shot.pixelColor(at + QPoint(0, 26));
            QVERIFY2(hot != cold, qPrintable(hot.name()));
            QCOMPARE(cold.name(), tm_->color(tok::c_raised).name());
        }
    }

    // DoD-U: vi-XL (chuỗi dài hơn 40%): danh sách, chú thích (cả chú thích gõ sai dài) không cắt chữ.
    void pseudoNoTruncation()
    {
        QVERIFY(translator_->setLanguage(i18n::kPseudoLanguage.toString()));
        const auto sysvar = [] { return QCoreApplication::translate("trion::ui::Autocomplete", "Biến hệ thống"); };
        QVERIFY(spinUntil([&] { return sysvar().size() > 13; }));
        for (const QString& w : {u"l"_s, u"lnie"_s, u"lwd"_s, u"dim"_s}) {
            retype(w);
            QVERIFY2(ac_->isVisible(), qPrintable(w));
            const QString cut = truncations(window_.get());
            QVERIFY2(cut.isEmpty(), qPrintable(w + u": "_s + cut));
        }
        QVERIFY(translator_->setLanguage(u"vi"_s));
        QVERIFY(spinUntil([&] { return sysvar().size() == 13; }));
    }
};

QTEST_MAIN(AutocompleteTest)
#include "autocomplete_test.moc"
