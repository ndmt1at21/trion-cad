// UIA-107 · Dòng lệnh (CommandLine.h). Nhóm theo tiêu chí nghiệm thu:
//   1  typingLatency: ký tự hiện trong ô: CPU luồng UI từ phím tới hết khung ≤ 16 ms (kiểm ở release; p99 thời gian
//      thật của UiProbe chỉ báo vì máy dùng chung). promptInOneFrame: mọi prompt mẫu hiện ở lần vẽ đầu sau setPrompt,
//      đúng bề rộng cuối, không vẽ/bố trí lại lần hai.
//   2  chipAndKeySameToken: bấm chip, gõ chữ tắt (thường, hoa, "_" + chữ tắt, tên toàn cục) cho cùng token trên mọi
//      prompt mẫu (lấy từ prototype); từ khóa ẩn của grip (ST, MO, RO, SC, MI) không có chip, gõ vẫn ra token.
//   3  scriptEquivalence: 1.000 phiên gõ ngẫu nhiên (hạt cố định) qua bàn phím cho cùng dãy token như tách cùng chuỗi
//      theo luật .scr (Space/Enter là Enter, trừ prompt chuỗi và biểu thức LISP); tên ở dấu nhắc lệnh mà danh sách tự
//      hoàn thành (UIA-108) có dòng chọn sẵn thì là chữ Autocomplete::completion (như AutoCAD). Bản ghi INT-002 thật:
//      Hoãn (cần INT-002).
//      idleRepeat: Enter/Space khi rảnh lặp lệnh vừa dùng (cả lệnh chạy từ ribbon, bí danh thành tên lệnh).
//   4  history10k: 10.000 dòng không thêm widget nào; thêm dòng và cuộn: lượt xử lý dài nhất của luồng UI ≤ 16,6 ms
//      (CPU luồng; kiểm ở release).
//   Còn lại: escapeAndBrowse, lispAndAwaiting, placeholder, commaHintOnce, compactAndLines (Q_PROPERTY compact,
//   kHistoryLines), contextMenu (bàn phím, sao chép lịch sử, macro), disabledState; DoD-U: pseudoNoTruncation (vi-XL),
//   keyboardOnly, baseline (ảnh Tối/Sáng 100% theo nền tảng; ghi lại: TRION_UPDATE_BASELINE=<thư mục>).
#include "app/shell/cmdline/CommandLine.h"

#include "app/core/i18n/Translator.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/core/perf/UiProbe.h"
#include "app/core/UiExecutor.h"
#include "app/shell/cmdline/Autocomplete.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/cmdline/CommandInput.h"
#include "core/base/sched/Scheduler.h"
#include "tests/timing.h"

#include <QAbstractEventDispatcher>
#include <QAbstractScrollArea>
#include <QApplication>
#include <QBoxLayout>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QInputMethodEvent>
#include <QJsonObject>
#include <QMenu>
#include <QMouseEvent>
#include <QPointer>
#include <QPainter>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QWheelEvent>
#include <QtTest>

#include <functional>
#include <memory>
#include <optional>
#include <random>
#include <vector>

#if defined(__linux__)
#include <ctime>
#endif

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;
using namespace trion;
using trion::ui::CommandInput;
using trion::ui::CommandLine;
namespace probe = trion::ui::probe;

namespace {

#if defined(Q_OS_WIN)
constexpr auto kPlatform = "windows";
#elif defined(Q_OS_MACOS)
constexpr auto kPlatform = "macos";
#else
constexpr auto kPlatform = "linux";
#endif
constexpr int kNoise = 8;     // baseline: lệch ≤ 8/255 mỗi kênh bỏ qua, điểm lệch hơn được tối đa 0,1% ảnh
constexpr int kSheetW = 640;  // bề rộng tấm ảnh baseline
constexpr double kFrameMs = 16.6;

// Prompt mẫu lấy từ prototype (js/cmd.js, editor.js, screens*.js) và 07.1 §8 (CIRCLE <10.0000>, "Bán kính · R").
struct KwSpec {
    const char* label;
    const char* key;
    const char* global = "";
    bool hidden = false;
};
struct PromptSpec {
    const char* command;
    const char* text;
    std::vector<KwSpec> keywords;
    const char* def = "";
    bool allowSpace = false;
    bool nested = false;
};
// clang-format off
const std::vector<PromptSpec> kPrompts = {
    {"CIRCLE", "Chỉ định bán kính hoặc", {{"Đường kính", "D", "_Diameter"}}, "10.0000"},                       // i18n-ok: dữ liệu thử
    {"DIST", "Chỉ định điểm thứ hai hoặc", {{"Nhiều điểm", "M", "_Multiple"}}},                               // i18n-ok: dữ liệu thử
    {"DIST", "Chỉ định điểm kế tiếp hoặc", {{"Hoàn tác", "U", "_Undo"}, {"Tổng", "T", "_Total"}}},            // i18n-ok: dữ liệu thử
    {"AREA", "Chỉ định điểm góc đầu tiên hoặc", {{"Đối tượng", "O", "_Object"}, {"Cộng diện tích", "A", "_Add"},  // i18n-ok: dữ liệu thử
                                                 {"Trừ diện tích", "S", "_Subtract"}}},                       // i18n-ok: dữ liệu thử
    {"MEASUREGEOM", "Nhập tùy chọn", {{"Khoảng cách", "D", "_Distance"}, {"Bán kính", "R", "_Radius"},        // i18n-ok: dữ liệu thử
                                      {"Góc", "A", "_Angle"}, {"Diện tích", "AR", "_ARea"}, {"Nhanh", "Q", "_Quick"},  // i18n-ok: dữ liệu thử
                                      {"Thoát", "X", "_eXit"}}, "Nhanh"},                                     // i18n-ok: dữ liệu thử
    {"ZOOM", "Chỉ định góc cửa sổ, nhập hệ số (nX) hoặc", {{"Tất cả", "A", "_All"}, {"Toàn bộ", "E", "_Extents"},  // i18n-ok: dữ liệu thử
                                                           {"Trước đó", "P", "_Previous"}, {"Cửa sổ", "W", "_Window"},  // i18n-ok: dữ liệu thử
                                                           {"Đối tượng", "O", "_Object"}}, "thời gian thực", false, true},  // i18n-ok: dữ liệu thử
    {"SELECT", "Chọn đối tượng", {{"Tất cả", "ALL"}, {"Cuối", "L"}, {"Trước đó", "P"}}},                      // i18n-ok: dữ liệu thử
    {"MTEXT", "Góc đối", {{"Chiều cao", "H"}, {"Canh lề", "J"}, {"Giãn dòng", "L"}, {"Xoay", "R"}, {"Kiểu", "S"},  // i18n-ok: dữ liệu thử
                          {"Rộng", "W"}, {"Cột", "C"}}},                                                      // i18n-ok: dữ liệu thử
    {"MTEXT", "Điểm canh", {{"Trên trái", "TL"}, {"Trên giữa", "TC"}, {"Trên phải", "TR"}, {"Giữa trái", "ML"},  // i18n-ok: dữ liệu thử
                            {"Giữa tâm", "MC"}, {"Giữa phải", "MR"}, {"Dưới trái", "BL"}, {"Dưới giữa", "BC"},  // i18n-ok: dữ liệu thử
                            {"Dưới phải", "BR"}}, "TL"},                                                      // i18n-ok: dữ liệu thử
    {"", "Điểm kéo giãn", {{"Điểm gốc", "B"}, {"Sao chép", "C"}, {"Hoàn tác", "U"}, {"Thoát", "X"},           // i18n-ok: dữ liệu thử
                           {"Kéo giãn", "ST", "", true}, {"Di chuyển", "MO", "", true}, {"Xoay", "RO", "", true},  // i18n-ok: dữ liệu thử
                           {"Tỉ lệ", "SC", "", true}, {"Đối xứng", "MI", "", true}}},                         // i18n-ok: dữ liệu thử
    {"EXPLODE", "Tùy chọn phá", {{"Tất cả", "A"}, {"Màu", "C"}, {"Layer", "LA"}, {"Linetype", "LT"},          // i18n-ok: dữ liệu thử
                                 {"Độ dày nét", "LW"}, {"Kế thừa từ block cha", "I"}, {"Phá", "E"}}, "E"},    // i18n-ok: dữ liệu thử
    {"WIPEOUT", "Khung vùng che", {{"Bật", "ON"}, {"Tắt", "OFF"}, {"Hiện nhưng không in", "D"}}, "ON"},       // i18n-ok: dữ liệu thử
    {"BATTSYNC", "Chọn block cần đồng bộ", {{"Liệt kê", "?"}, {"Tên", "N"}, {"Chọn", "S"}}, "S"},             // i18n-ok: dữ liệu thử
    {"MTEXT", "Dòng chữ đầu tiên của chú thích", {}, "MText", true},                                          // i18n-ok: dữ liệu thử
};
// clang-format on

// Chữ mẫu qua bộ dịch đang cài: ở vi-XL được kéo dài như chuỗi thật (biến, không phải hằng: lupdate không thu).
QString text(const char* source)
{
    return QCoreApplication::translate("CommandLineTest", source);
}

CommandLine::Prompt make(const PromptSpec& s)
{
    CommandLine::Prompt p{QString::fromLatin1(s.command), text(s.text), {}, text(s.def), s.allowSpace, s.nested};
    for (const KwSpec& k : s.keywords)
        p.keywords.append({text(k.label), QString::fromLatin1(k.key), QString::fromLatin1(k.global), k.hidden});
    return p;
}

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

void spinFor(std::chrono::milliseconds d)
{
    QEventLoop loop;
    QTimer::singleShot(d, &loop, &QEventLoop::quit);
    loop.exec();
}

// Thời gian CPU của luồng gọi. Linux: CLOCK_THREAD_CPUTIME_ID; nơi khác chưa có (GetThreadTimes của Windows nhảy theo
// nhịp 15,6 ms): -1, số đo in "nan" và không kiểm.
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
bool cpuSupported()
{
    return threadCpu().count() >= 0;
}
double cpuMs(std::chrono::nanoseconds d)
{
    return cpuSupported() ? std::chrono::duration<double, std::milli>(d).count() : qQNaN();
}
// Ngưỡng CPU chỉ kiểm ở bản tối ưu (kTimed) và khi đo được.
bool withinCpu(double ms, double budget)
{
    return !test::kTimed || qIsNaN(ms) || ms <= budget;
}

// Lượt vòng sự kiện bận nhất của luồng UI (giữa hai lần aboutToBlock), tính bằng CPU của luồng: không phụ thuộc tải.
struct Busy : QObject {
    std::chrono::nanoseconds last = threadCpu(), longest{0};
    Busy()
    {
        connect(QAbstractEventDispatcher::instance(), &QAbstractEventDispatcher::aboutToBlock, this, [this] {
            const auto now = threadCpu();
            longest = std::max(longest, now - last);
            last = now;
        });
    }
    double ms() const { return cpuMs(longest); }
};

// Đếm lần vẽ của một widget, ghi bề rộng lúc vẽ.
struct Paints : QObject {
    int count = 0;
    int width = -1;
    bool eventFilter(QObject* o, QEvent* e) override
    {
        if (e->type() == QEvent::Paint) {
            ++count;
            width = static_cast<QWidget*>(o)->width();
        }
        return false;
    }
};

QWidget* promptView(CommandLine* c)
{
    return c->findChild<QWidget*>(u"prompt"_s);
}
QAbstractScrollArea* historyView(CommandLine* c)
{
    return c->findChild<QAbstractScrollArea*>(u"history"_s);
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

} // namespace

class CommandLineTest : public QObject {
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
    QStringList tokens_;

    // Gõ từng ký tự vào ô nhập như bàn phím ('\n' là Enter).
    void type(const QString& keys, QWidget* target = nullptr)
    {
        QWidget* w = target ? target : cmd_->input();
        for (const QChar c : keys) {
            if (c == u'\n')
                QTest::keyClick(w, Qt::Key_Return);
            else
                QTest::keyClick(w, c.toLatin1());
        }
    }

    void focusInput()
    {
        window_->activateWindow();
        cmd_->setFocus(Qt::OtherFocusReason);
        QVERIFY(spinUntil([&] { return QApplication::focusWidget() == cmd_->input(); }));
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
        QApplication::setCursorFlashTime(0); // con trỏ chữ không nhấp nháy: ảnh baseline ổn định

        window_ = std::make_unique<QWidget>();
        auto* column = new QVBoxLayout(window_.get());
        column->setContentsMargins({});
        column->setSpacing(0);
        before_ = new QLineEdit;
        cmd_ = new CommandLine;
        column->addWidget(before_);
        column->addStretch(1); // chỗ vùng vẽ
        column->addWidget(cmd_);
        window_->resize(1366, 300);
        window_->show();
        QVERIFY(QTest::qWaitForWindowExposed(window_.get()));
        connect(cmd_, &CommandLine::submitted, this, [this](const QString& t) { tokens_ << t; });
    }

    void init()
    {
        tm_->setTheme(theme::Theme::Dark);
        tm_->setDensity(theme::Density::Compact);
        store_->reset(ui::kHistoryLines);
        store_->reset(ui::kImeBypass);
        cmd_->setEnabled(true);
        cmd_->setCompact(false);
        cmd_->clearPrompt();
        cmd_->input()->clear();
        tokens_.clear();
        focusInput();
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

    // Tiêu chí 1: ký tự hiện trong ô ≤ 16 ms p99 sau phím (UiProbe: từ phím tới QWindow đến hết khung kế tiếp), ở dấu
    // nhắc lệnh (bỏ qua bộ gõ, chữ hoa; khung gồm cả danh sách tự hoàn thành UIA-108) và ở prompt. Đo trạng thái ổn
    // định: một lượt gõ cùng dãy phím trước để danh sách hiện lần đầu, glyph và biểu tượng mới raster (một lần mỗi phiên).
    void typingLatency()
    {
        constexpr int kN = 200;
        for (int i = 0; i < kN; ++i) {
            if (i % 40 == 0)
                cmd_->input()->clear();
            QTest::keyClick(window_->windowHandle(), char('a' + i % 26));
            QCoreApplication::processEvents();
        }
        cmd_->input()->clear();
        probe::enable();
        for (const bool prompting : {false, true}) {
            probe::reset();
            std::chrono::nanoseconds worst{0}; // CPU của luồng UI từ phím tới hết khung (keyClick xử lý đồng bộ)
            if (prompting)
                cmd_->setPrompt(make(kPrompts[0]));
            for (int i = 0; i < kN; ++i) {
                if (i % 40 == 0)
                    cmd_->input()->clear();
                const auto cpu = threadCpu();
                QTest::keyClick(window_->windowHandle(), char('a' + i % 26));
                QVERIFY(spinUntil([&] { return probeOp("typing")["count"].toInt() == i + 1; }));
                worst = std::max(worst, threadCpu() - cpu);
            }
            QCOMPARE(cmd_->input()->text().size(), kN % 40 ? kN % 40 : 40);
            const double p50 = probeOp("typing")["p50_ms"].toDouble(), p99 = probeOp("typing")["p99_ms"].toDouble();
            const double cpu = cpuMs(worst);
            qInfo("typing %s: p50 %.2f ms, p99 %.2f ms (%d mẫu); phím tốn nhất %.2f ms CPU",
                  prompting ? "ở prompt" : "ở dấu nhắc lệnh", p50, p99, kN, cpu);
            // Máy dùng chung: kiểm CPU của luồng UI (không phụ thuộc tải); p99 thời gian thật chỉ báo.
            QVERIFY2(withinCpu(cpu, 16.0), qPrintable(QString::number(cpu)));
        }
        QVERIFY(tokens_.isEmpty());
        probe::disable();
    }

    // Tiêu chí 1: prompt mới hiện trong 1 khung: lần vẽ đầu sau setPrompt đã có bề rộng cuối (đủ chỗ ở 1366 px), sau
    // đó không vẽ lại; thời gian tới hết khung đó chỉ báo.
    void promptInOneFrame()
    {
        QWidget* view = promptView(cmd_);
        QVERIFY(view);
        Paints paints;
        view->installEventFilter(&paints);
        probe::enable();
        probe::reset();
        Busy busy;
        for (const PromptSpec& spec : kPrompts) {
            spinFor(20ms); // xong việc vẽ của prompt trước
            paints.count = 0;
            const auto start = probe::Clock::now();
            cmd_->setPrompt(make(spec));
            probe::untilFrame(probe::Op::UiState, view, start);
            QVERIFY(spinUntil([&] { return paints.count > 0; }));
            const int first = paints.width;
            spinFor(50ms);
            QCOMPARE(paints.count, 1);
            QCOMPARE(first, view->width());
            QCOMPARE(view->width(), view->sizeHint().width()); // không rút gọn ở 1366 px
            QVERIFY(view->accessibleName().contains(text(spec.text)));
            QCOMPARE(cmd_->input()->accessibleDescription(), view->accessibleName());
        }
        view->removeEventFilter(&paints);
        QVERIFY(spinUntil([&] { return probeOp("ui_state")["count"].toInt() == int(kPrompts.size()); }));
        qInfo("prompt tới hết khung: p50 %.2f ms, p99 %.2f ms; lượt dài nhất %.2f ms CPU",
              probeOp("ui_state")["p50_ms"].toDouble(), probeOp("ui_state")["p99_ms"].toDouble(), busy.ms());
        QVERIFY2(withinCpu(busy.ms(), kFrameMs), qPrintable(QString::number(busy.ms())));
        probe::disable();
    }

    // Tiêu chí 2: bấm chip và gõ chữ tắt cho cùng token trên mọi prompt mẫu; từ khóa ẩn gõ được, không có chip.
    void chipAndKeySameToken()
    {
        QWidget* view = promptView(cmd_);
        int chips = 0, hidden = 0;
        for (const PromptSpec& spec : kPrompts) {
            const CommandLine::Prompt p = make(spec);
            for (const CommandLine::Keyword& k : p.keywords) {
                QStringList want;
                cmd_->setPrompt(p);
                QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
                const QRect r = cmd_->keywordRect(k.key);
                if (k.hidden) {
                    QVERIFY2(r.isNull(), qPrintable(k.key));
                    ++hidden;
                } else {
                    QVERIFY2(!r.isNull() && cmd_->rect().contains(r), qPrintable(k.key));
                    QTest::mouseClick(view, Qt::LeftButton, {}, r.center() - view->pos());
                    want << k.key;
                    ++chips;
                }
                QStringList typed{k.key.toLower(), k.key, u'_' + k.key.toLower()};
                if (!k.global.isEmpty())
                    typed << k.global << k.global.mid(1).toUpper();
                for (const QString& t : typed) {
                    cmd_->setPrompt(p);
                    type(t + u'\n');
                    want << k.key;
                }
                if (!p.allowSpace) { // Space gửi như Enter
                    cmd_->setPrompt(p);
                    type(k.key.toLower() + u' ');
                    want << k.key;
                }
                QCOMPARE(tokens_, want);
                tokens_.clear();
            }
            cmd_->setPrompt(p); // chữ không phải từ khóa giữ nguyên
            type(u"100,200\n"_s);
            QCOMPARE(tokens_, QStringList{u"100,200"_s});
            tokens_.clear();
        }
        qInfo("%d chip, %d từ khóa ẩn trên %d prompt mẫu", chips, hidden, int(kPrompts.size()));
        QVERIFY(chips >= 40 && hidden == 5);
    }

    // Tiêu chí 3: 1.000 phiên gõ ngẫu nhiên cho cùng dãy token như tách cùng chuỗi theo luật .scr. Lệnh giả: mỗi token
    // đưa dòng lệnh sang trạng thái kế tiếp (rảnh hoặc một prompt) của một vòng cố định. Tên ở dấu nhắc lệnh qua tự
    // hoàn thành (UIA-108): token mong đợi tính ngay trước phím tách, theo danh sách và tần suất dùng lúc đó.
    void scriptEquivalence()
    {
        std::vector<std::optional<CommandLine::Prompt>> states = {
            std::nullopt, make(kPrompts[0]), make(kPrompts.back()), std::nullopt, make(kPrompts[6]),
            make(kPrompts[9]), std::nullopt, make(kPrompts[5]),
        };
        std::size_t step = 0;
        const auto advance = [&] {
            ++step;
            if (const auto& s = states[step % states.size()])
                cmd_->setPrompt(*s);
            else
                cmd_->clearPrompt();
        };
        QMetaObject::Connection driver = connect(cmd_, &CommandLine::submitted, this, advance);

        // Luật .scr (giống INT-002 chạy script): Space/Enter là Enter, trừ prompt chuỗi (chỉ xuống dòng) và biểu thức
        // LISP ở dấu nhắc lệnh; Enter trống khi rảnh lặp lệnh vừa dùng; chữ ở dấu nhắc lệnh viết hoa (bỏ qua bộ gõ).
        // Token phím c gửi (nếu có), tính trước khi gõ c.
        std::size_t refStep = 0;
        QString last, buffer;
        const auto expect = [&](QChar c) -> std::optional<QString> {
            const auto& s = states[refStep % states.size()];
            const bool spaceText = s ? s->allowSpace : buffer.startsWith(u'(');
            if (c != u'\n' && (c != u' ' || spaceText)) {
                buffer += !s && !buffer.startsWith(u'(') ? c.toUpper() : c;
                return std::nullopt;
            }
            QString token;
            if (s) {
                token = CommandLine::resolve(*s, s->allowSpace ? buffer : buffer.trimmed());
            } else {
                const QString name = buffer.trimmed(), done = cmd_->autocomplete()->completion(name);
                token = name.isEmpty() ? last : done.isEmpty() ? name : done;
                buffer.clear();
                if (token.isEmpty())
                    return std::nullopt;
                if (!token.startsWith(u'('))
                    last = token.toUpper();
            }
            buffer.clear();
            const auto& next = states[++refStep % states.size()];
            if (next && !next->nested && !next->command.isEmpty())
                last = next->command;
            return token;
        };

        std::mt19937 rng(20261022); // mt19937 giống nhau mọi nền tảng; uniform_int_distribution thì không (libstdc++/libc++)
        const auto below = [&](int n) { return int(rng() % unsigned(n)); };
        const QString letters = u"abcdeflmoprsuxz"_s, digits = u"0123456789"_s, marks = u",.@<-_'"_s;
        constexpr int kSessions = 1000;
        int total = 0;
        for (int session = 0; session < kSessions; ++session) {
            QString keys;
            for (int i = 4 + below(45); i > 0; --i) {
                const int r = below(100);
                const QString& from = r < 45 ? letters : r < 65 ? digits : r < 75 ? marks : QString();
                if (!from.isEmpty())
                    keys += from[below(int(from.size()))];
                else
                    keys += r < 78 ? u'(' : r < 80 ? u')' : r < 92 ? u' ' : u'\n';
            }
            if (!keys.endsWith(u'\n'))
                keys += u'\n';
            tokens_.clear();
            QStringList want;
            for (const QChar c : keys) {
                if (const std::optional<QString> token = expect(c))
                    want << *token;
                type(QString(c));
            }
            QVERIFY2(tokens_ == want, qPrintable(u"phiên %1 «%2»: %3 ≠ %4"_s.arg(session).arg(keys, tokens_.join(u'|'), want.join(u'|'))));
            total += int(want.size());
        }
        disconnect(driver);
        qInfo("%d phiên, %d token khớp", kSessions, total);
        QVERIFY(total > 2 * kSessions);
    }

    // Tiêu chí 3: Enter/Space khi rảnh và ô trống lặp lệnh vừa dùng; lệnh từ ribbon (chỉ có prompt) cũng lặp được; bí
    // danh vừa gõ thành tên lệnh khi lệnh hỏi prompt đầu.
    void idleRepeat()
    {
        CommandLine fresh;
        QStringList got;
        connect(&fresh, &CommandLine::submitted, this, [&](const QString& t) { got << t; });
        type(u"\n "_s, fresh.input()); // chưa có lệnh nào: không gửi gì
        QVERIFY(got.isEmpty());
        type(u"line\n\n "_s, fresh.input());
        QCOMPARE(got, (QStringList{u"LINE"_s, u"LINE"_s, u"LINE"_s}));
        got.clear();
        fresh.setPrompt(make(kPrompts[0])); // CIRCLE chạy từ ribbon
        fresh.clearPrompt();
        type(u"\n"_s, fresh.input());
        QCOMPARE(got, QStringList{u"CIRCLE"_s});
        type(u"l\n"_s, fresh.input()); // bí danh L chạy LINE
        fresh.setPrompt({u"LINE"_s, text("Chỉ định điểm đầu")}); // i18n-ok: dữ liệu thử
        fresh.clearPrompt();
        got.clear();
        type(u" "_s, fresh.input());
        QCOMPARE(got, QStringList{u"LINE"_s});
        QTest::keyClick(fresh.input(), Qt::Key_Up);
        QCOMPARE(fresh.input()->text(), u"LINE"_s);
        QTest::keyClick(fresh.input(), Qt::Key_Up);
        QCOMPARE(fresh.input()->text(), u"CIRCLE"_s); // "L" không còn trong danh sách
        QVERIFY(fresh.historyLine(fresh.historyCount() - 1).endsWith(u" LINE"_s)); // "Lệnh: LINE"
    }

    // Esc: có prompt → hủy lệnh; rảnh (Esc lần hai sau khi lệnh hủy) → bỏ vùng chọn. ↑/↓: lệnh gần đây khi rảnh, giá
    // trị đã nhập khi có prompt; dòng lệnh echo vào lịch sử.
    void escapeAndBrowse()
    {
        QSignalSpy cancel(cmd_, &CommandLine::cancelRequested), deselect(cmd_, &CommandLine::deselectRequested);
        connect(cmd_, &CommandLine::cancelRequested, cmd_, &CommandLine::clearPrompt); // như INT-002 khi hủy
        cmd_->setPrompt(make(kPrompts[0]));
        type(u"12"_s);
        QTest::keyClick(cmd_->input(), Qt::Key_Escape);
        QCOMPARE(cancel.count(), 1);
        QCOMPARE(deselect.count(), 0);
        QVERIFY(cmd_->input()->text().isEmpty() && !cmd_->hasPrompt());
        QTest::keyClick(cmd_->input(), Qt::Key_Escape);
        QCOMPARE(cancel.count(), 1);
        QCOMPARE(deselect.count(), 1);
        disconnect(cmd_, &CommandLine::cancelRequested, cmd_, &CommandLine::clearPrompt);

        type(u"move\ncopy\n"_s);
        QTest::keyClick(cmd_->input(), Qt::Key_Up);
        QCOMPARE(cmd_->input()->text(), u"COPY"_s);
        QTest::keyClick(cmd_->input(), Qt::Key_Up);
        QCOMPARE(cmd_->input()->text(), u"MOVE"_s);
        QTest::keyClick(cmd_->input(), Qt::Key_Down);
        QCOMPARE(cmd_->input()->text(), u"COPY"_s);
        QTest::keyClick(cmd_->input(), Qt::Key_Down);
        QVERIFY(cmd_->input()->text().isEmpty());
        cmd_->setPrompt(make(kPrompts[0]));
        type(u"25\n"_s);
        cmd_->setPrompt(make(kPrompts[0]));
        type(u"7.5\n"_s);
        cmd_->setPrompt(make(kPrompts[0]));
        QTest::keyClick(cmd_->input(), Qt::Key_Up);
        QCOMPARE(cmd_->input()->text(), u"7.5"_s);
        QTest::keyClick(cmd_->input(), Qt::Key_Up);
        QCOMPARE(cmd_->input()->text(), u"25"_s);
        QCOMPARE(cmd_->historyLine(cmd_->historyCount() - 1),
                 u"CIRCLE "_s + text(kPrompts[0].text) + u": 7.5"_s); // echo: lệnh, lời nhắc, giá trị
    }

    // Bỏ qua bộ gõ khi chờ tên lệnh; tắt khi có prompt và khi gõ "(" của LISP ở dấu nhắc lệnh (Space là chữ).
    void lispAndAwaiting()
    {
        CommandInput* in = cmd_->input();
        QVERIFY(in->awaitingCommand());
        cmd_->setPrompt(make(kPrompts[0]));
        QVERIFY(!in->awaitingCommand());
        cmd_->clearPrompt();
        QVERIFY(in->awaitingCommand());
        type(u"(setq a "_s);
        QVERIFY(!in->awaitingCommand());
        type(u"\"ab\")\n"_s);
        QCOMPARE(tokens_, QStringList{u"(setq a \"ab\")"_s});
        QVERIFY(in->awaitingCommand());
        tokens_.clear();
        cmd_->setPrompt(make(kPrompts.back())); // prompt chuỗi: Space là chữ
        type(u"Ghi chu 1\n"_s);
        QCOMPARE(tokens_, QStringList{u"Ghi chu 1"_s});
    }

    // Placeholder (chữ --f-ui --c-text-3 vẽ dưới ô nhập): hiện khi rảnh và ô trống; ẩn khi bộ gõ đang soạn chữ (tắt bỏ
    // qua bộ gõ), khi có chữ, khi có prompt.
    void placeholder()
    {
        store_->set(ui::kImeBypass, false);
        const auto ink = [&] { // điểm ảnh có mực (khác nền --c-raised) trong ô nhập: placeholder ~52 chữ, "ơ" một chữ
            QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
            const QImage img = cmd_->grab(cmd_->input()->geometry()).toImage();
            const QColor c = tm_->color(tok::c_raised);
            int n = 0;
            for (int y = 0; y < img.height(); ++y)
                for (int x = 0; x < img.width(); ++x) {
                    const QRgb px = img.pixel(x, y);
                    n += std::max({std::abs(qRed(px) - c.red()), std::abs(qGreen(px) - c.green()),
                                   std::abs(qBlue(px) - c.blue())}) > 40;
                }
            return n;
        };
        const int shown = ink();
        QVERIFY2(shown > 300, qPrintable(QString::number(shown)));
        QInputMethodEvent preedit(u"ơ"_s, {}); // i18n-ok: dữ liệu thử
        QCoreApplication::sendEvent(cmd_->input(), &preedit);
        QVERIFY(cmd_->input()->text().isEmpty());
        QVERIFY2(ink() < shown / 3, qPrintable(QString::number(ink())));
        QInputMethodEvent commit;
        commit.setCommitString(u"ơ"_s); // i18n-ok: dữ liệu thử
        QCoreApplication::sendEvent(cmd_->input(), &commit);
        QVERIFY2(ink() < shown / 3, qPrintable(QString::number(ink())));
        cmd_->input()->clear();
        QCOMPARE(ink(), shown);
        cmd_->setPrompt(make(kPrompts[0]));
        QVERIFY2(ink() < shown / 3, qPrintable(QString::number(ink())));
    }

    // 07.1 §8: gõ "," sau chữ số nhắc dấu thập phân một lần; prompt chuỗi không nhắc.
    void commaHintOnce()
    {
        CommandLine fresh;
        fresh.setPrompt(make(kPrompts.back()));
        type(u"1,5"_s, fresh.input());
        QCOMPARE(fresh.historyCount(), 1); // chỉ dòng chào
        fresh.setPrompt(make(kPrompts[0]));
        fresh.input()->clear();
        type(u"1,5"_s, fresh.input());
        QCOMPARE(fresh.historyCount(), 2);
        QVERIFY(fresh.historyLine(1).contains(u"12.5"_s));
        fresh.input()->clear();
        type(u"3,4"_s, fresh.input());
        QCOMPARE(fresh.historyCount(), 2);
    }

    // Tiêu chí 4: 10.000 dòng không tạo widget theo dòng; thêm dòng và cuộn bằng bánh xe: lượt xử lý dài nhất của luồng
    // UI ≤ 16,6 ms; scroll_frame (thời gian thật) chỉ báo.
    void history10k()
    {
        CommandLine c;
        c.resize(1366, c.sizeHint().height());
        c.show();
        QVERIFY(QTest::qWaitForWindowExposed(&c));
        const auto widgets = c.findChildren<QWidget*>().size();
        for (int i = 0; i < 10000; ++i)
            c.appendHistory(u"Đã chọn %1 đối tượng"_s.arg(i), i % 50 ? CommandLine::Kind::Message : CommandLine::Kind::Error); // i18n-ok: dữ liệu thử
        QCOMPARE(c.historyCount(), 10001);
        QCOMPARE(c.findChildren<QWidget*>().size(), widgets);
        QAbstractScrollArea* view = historyView(&c);
        QCOMPARE(view->verticalScrollBar()->value(), view->verticalScrollBar()->maximum()); // dòng cuối đang hiện
        QCOMPARE(view->verticalScrollBar()->maximum(), 10001 - 3);

        Paints paints;
        view->viewport()->installEventFilter(&paints);
        spinFor(50ms);
        Busy busy;
        for (int i = 0; i < 200; ++i) {
            const int before = paints.count;
            c.appendHistory(u"Dòng %1"_s.arg(i)); // i18n-ok: dữ liệu thử
            QVERIFY(spinUntil([&] { return paints.count > before; }));
        }
        const double append = busy.ms();
        probe::enable();
        probe::reset();
        Busy scroll;
        for (int i = 0; i < 120; ++i) {
            const int before = paints.count;
            const QPointF pos = view->viewport()->mapTo(&c, view->viewport()->rect().center());
            QWheelEvent e(pos, c.mapToGlobal(pos), QPoint(), QPoint(0, i < 60 ? 120 : -120), Qt::NoButton,
                          Qt::NoModifier, Qt::NoScrollPhase, false);
            QCoreApplication::sendEvent(c.windowHandle(), &e);
            QVERIFY(spinUntil([&] { return paints.count > before; }));
        }
        QVERIFY(view->verticalScrollBar()->value() == view->verticalScrollBar()->maximum());
        view->viewport()->removeEventFilter(&paints);
        qInfo("10.000 dòng: thêm dòng lượt dài nhất %.2f ms CPU; cuộn lượt dài nhất %.2f ms CPU, scroll_frame p50 %.2f "
              "p99 %.2f ms",
              append, scroll.ms(), probeOp("scroll_frame")["p50_ms"].toDouble(),
              probeOp("scroll_frame")["p99_ms"].toDouble());
        probe::disable();
        QVERIFY2(withinCpu(append, kFrameMs), qPrintable(QString::number(append)));
        QVERIFY2(withinCpu(scroll.ms(), kFrameMs), qPrintable(QString::number(scroll.ms())));
    }

    // Q_PROPERTY compact (MainWindow đặt ở màn hẹp): 1 dòng lịch sử; số dòng từ Store 0–10, đổi là áp ngay.
    void compactAndLines()
    {
        QAbstractScrollArea* view = historyView(cmd_);
        const int row = qRound(tm_->px(tok::h_cmdl));
        const auto rows = [&] { return view->isVisible() ? view->height() / row : 0; };
        const auto settle = [&] { // dòng lệnh rồi cửa sổ bố trí lại
            QVERIFY(spinUntil([&] { return cmd_->height() == cmd_->sizeHint().height(); }));
        };
        settle();
        QCOMPARE(rows(), 3);
        const int full = cmd_->height();
        QVERIFY(cmd_->setProperty("compact", true));
        settle();
        QCOMPARE(rows(), 1);
        QCOMPARE(cmd_->height(), full - 2 * row);
        cmd_->setProperty("compact", false);
        store_->set(ui::kHistoryLines, 10);
        settle();
        QCOMPARE(rows(), 10);
        store_->set(ui::kHistoryLines, 25);
        settle();
        QCOMPARE(rows(), 10);
        store_->set(ui::kHistoryLines, 0);
        settle();
        QCOMPARE(rows(), 0);
        cmd_->setProperty("compact", true);
        settle();
        QCOMPARE(rows(), 0);
        cmd_->setProperty("compact", false);
        store_->reset(ui::kHistoryLines);
        tm_->setDensity(theme::Density::Comfortable); // --h-cmdl 17 → 19 px
        settle();
        QCOMPARE(view->height(), 3 * qRound(tm_->px(tok::h_cmdl)));
        QCOMPARE(cmd_->input()->height(), qRound(tm_->px(tok::h_cmdin)) - 2);
    }

    // Menu chuột phải (cả phím Menu): Lệnh gần đây, Giá trị đã nhập gần đây (rỗng: "Chưa có"), Sao chép lịch sử, Tùy
    // chọn dòng lệnh; lệnh đi ra bằng macro như ActionRegistry.
    void contextMenu()
    {
        QSignalSpy macros(cmd_, &CommandLine::commandRequested);
        type(u"line\n"_s);
        const auto open = [&] {
            QContextMenuEvent e(QContextMenuEvent::Keyboard, QPoint(10, 10), cmd_->input()->mapToGlobal(QPoint(10, 10)));
            QCoreApplication::sendEvent(cmd_->input(), &e);
            QPointer<QMenu> m;
            spinUntil([&] {
                for (QMenu* x : cmd_->findChildren<QMenu*>(Qt::FindDirectChildrenOnly))
                    if (x->isVisible())
                        m = x;
                return m != nullptr;
            });
            return m;
        };
        QPointer<QMenu> menu = open();
        QVERIFY(menu);
        const QList<QAction*> items = menu->actions();
        QCOMPARE(items.size(), 5);
        QCOMPARE(items[0]->menu()->actions().first()->text(), u"LINE"_s);
        QCOMPARE(truncations(window_.get()), QString());
        // Bàn phím: ↓ tới "Sao chép lịch sử", Enter
        for (int i = 0; i < 5 && menu->activeAction() != items[2]; ++i)
            QTest::keyClick(menu, Qt::Key_Down);
        QCOMPARE(menu->activeAction(), items[2]);
        QTest::keyClick(menu, Qt::Key_Return);
        QVERIFY(spinUntil([&] { return !menu || !menu->isVisible(); }));
        const QString copied = QGuiApplication::clipboard()->text();
        QCOMPARE(copied.count(u'\n'), cmd_->historyCount() - 1);
        QVERIFY(copied.endsWith(u" LINE"_s));

        menu = open();
        QVERIFY(menu);
        menu->actions()[0]->menu()->actions().first()->trigger();
        menu->actions()[4]->trigger();
        QCOMPARE(macros.count(), 2);
        QCOMPARE(macros[0][0].toString(), u"^C^CLINE"_s);
        QCOMPARE(macros[1][0].toString(), u"^C^C_INPUTSEARCHOPTIONS"_s);
        menu->close();

        CommandLine empty; // chưa có lệnh, giá trị: mục "Chưa có" mờ
        QContextMenuEvent e(QContextMenuEvent::Mouse, QPoint(5, 5), QPoint(5, 5));
        QCoreApplication::sendEvent(&empty, &e);
        QMenu* none = empty.findChild<QMenu*>(Qt::FindDirectChildrenOnly);
        QVERIFY(none);
        for (int i : {0, 1})
            QVERIFY(none->actions()[i]->menu()->actions().size() == 1
                    && !none->actions()[i]->menu()->actions().first()->isEnabled());
        none->close();

        cmd_->setPrompt(make(kPrompts[0]));
        type(u"42\n"_s);
        cmd_->setPrompt(make(kPrompts[0]));
        menu = open();
        QVERIFY(menu);
        menu->actions()[1]->menu()->actions().first()->trigger(); // giá trị vào ô nhập để sửa tiếp
        QCOMPARE(cmd_->input()->text(), u"42"_s);
        menu->close();
    }

    // Vô hiệu: chip không bấm được, chữ mờ; lỗi có biểu tượng (ảnh baseline).
    void disabledState()
    {
        cmd_->setPrompt(make(kPrompts[0]));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        const QRect r = cmd_->keywordRect(u"D"_s);
        cmd_->setEnabled(false);
        QTest::mouseClick(promptView(cmd_), Qt::LeftButton, {}, r.center() - promptView(cmd_)->pos());
        QVERIFY(tokens_.isEmpty());
        QVERIFY(!cmd_->input()->isEnabled());
    }

    // DoD-U: chữ dài hơn 40% (vi-XL) không bị cắt ở 1366 px: prompt mẫu đủ chỗ (không rút gọn), menu, dòng lịch sử
    // của dòng lệnh (chào, nhắc dấu thập phân, echo) vừa bề rộng; cả gọn và thoáng.
    void pseudoNoTruncation()
    {
        QVERIFY(translator_->setLanguage(i18n::kPseudoLanguage.toString()));
        QVERIFY(spinUntil([&] { return text("Chọn đối tượng").size() > 16; })); // i18n-ok: dữ liệu thử
        auto* fresh = new CommandLine; // dòng chào, nhắc bằng ngôn ngữ giả
        static_cast<QVBoxLayout*>(window_->layout())->insertWidget(1, fresh);
        QVERIFY(spinUntil([&] { return fresh->isVisible() && fresh->width() == window_->width(); }));
        fresh->setFocus();
        fresh->setPrompt(make(kPrompts[0]));
        type(u"1,5\n"_s, fresh->input());
        fresh->clearPrompt();
        type(u"line\n"_s, fresh->input());
        for (const theme::Density d : {theme::Density::Comfortable, theme::Density::Compact}) {
            tm_->setDensity(d);
            QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
            for (const PromptSpec& spec : kPrompts) {
                fresh->setPrompt(make(spec));
                QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
                QWidget* view = promptView(fresh);
                QVERIFY2(view->width() >= view->sizeHint().width(),
                         qPrintable(u"%1: %2 < %3"_s.arg(text(spec.text)).arg(view->width()).arg(view->sizeHint().width())));
            }
            const QAbstractScrollArea* history = historyView(fresh);
            const QFontMetrics bold(theme::font(theme::TextRole::Ui, theme::Weight::Bold)); // cận trên mọi đoạn
            for (int i = 0; i < fresh->historyCount(); ++i) {
                const int need = qRound(tm_->px(tok::sp_5)) + bold.horizontalAdvance(fresh->historyLine(i));
                QVERIFY2(need <= history->viewport()->width(), qPrintable(fresh->historyLine(i)));
            }
            QCOMPARE(truncations(window_.get()), QString());
        }
        QContextMenuEvent e(QContextMenuEvent::Keyboard, QPoint(5, 5), fresh->mapToGlobal(QPoint(5, 5)));
        QCoreApplication::sendEvent(fresh, &e);
        QMenu* menu = fresh->findChild<QMenu*>(Qt::FindDirectChildrenOnly);
        QVERIFY(menu && spinUntil([&] { return menu->isVisible(); }));
        QCOMPARE(truncations(fresh), QString());
        menu->close();
        delete fresh;
        QVERIFY(translator_->setLanguage(u"vi"_s));
        QVERIFY(spinUntil([&] { return text("Chọn đối tượng").size() == 14; })); // i18n-ok: dữ liệu thử
    }

    // DoD-U bàn phím: Tab tới ô nhập; gõ, Enter, Esc, ↑ không cần chuột; Shift+Tab ra lại.
    void keyboardOnly()
    {
        before_->setFocus();
        QVERIFY(spinUntil([&] { return before_->hasFocus(); }));
        QTest::keyClick(before_, Qt::Key_Tab);
        QVERIFY(spinUntil([&] { return cmd_->input()->hasFocus(); }));
        QTest::keyClicks(QApplication::focusWidget(), u"circle"_s);
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Return);
        QCOMPARE(tokens_, QStringList{u"CIRCLE"_s});
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Up);
        QCOMPARE(cmd_->input()->text(), u"CIRCLE"_s);
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Escape);
        QVERIFY(cmd_->input()->text().isEmpty());
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Backtab);
        QVERIFY(spinUntil([&] { return before_->hasFocus(); }));
    }

    // DoD-U: ảnh Tối/Sáng 100%: rảnh (lịch sử có echo, thông báo, lỗi), prompt có chip (rê chip đầu, ô nhập có focus),
    // prompt lồng ">>" ở dạng compact; so baseline theo nền tảng.
    void baseline_data()
    {
        QTest::addColumn<bool>("light");
        QTest::newRow("dark") << false;
        QTest::newRow("light") << true;
    }

    void baseline()
    {
        QFETCH(bool, light);
        tm_->setTheme(light ? theme::Theme::Light : theme::Theme::Dark);
        QCoreApplication::processEvents(); // Qt 6.11 gửi ApplicationPaletteChange qua hàng đợi
        QWidget host;
        auto* column = new QVBoxLayout(&host);
        column->setContentsMargins({});
        column->setSpacing(0);
        CommandLine idle, chips, nested;
        for (CommandLine* c : {&idle, &chips, &nested})
            column->addWidget(c);
        host.resize(kSheetW, 10);
        host.show();
        host.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&host));
        type(u"line\n"_s, idle.input());
        idle.appendHistory(text("Đã chọn 125 đối tượng")); // i18n-ok: dữ liệu thử
        idle.appendHistory(text("Không có lệnh “LNE”."), CommandLine::Kind::Error); // i18n-ok: dữ liệu thử
        chips.setPrompt(make(kPrompts[0]));
        type(u"12"_s, chips.input());
        CommandLine::Prompt inner = make(kPrompts[2]); // lệnh trong suốt: ">>"
        inner.nested = true;
        nested.setCompact(true);
        nested.setPrompt(inner);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        chips.setFocus();
        QVERIFY(spinUntil([&] { return chips.input()->hasFocus(); }));
        const QRect r = chips.keywordRect(u"D"_s);
        QWidget* view = promptView(&chips);
        QMouseEvent move(QEvent::MouseMove, QPointF(r.center() - view->pos()), view->mapToGlobal(QPointF(r.center() - view->pos())),
                         Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(view, &move);

        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QImage sheet(host.size(), QImage::Format_ARGB32_Premultiplied);
        sheet.fill(tm_->color(tok::c_chrome));
        QPainter p(&sheet);
        host.render(&p);
        p.end();

        const QString name = u"commandline-%1-100-%2.png"_s.arg(light ? u"light"_s : u"dark"_s).arg(QLatin1StringView(kPlatform));
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

QTEST_MAIN(CommandLineTest)
#include "commandline_test.moc"
