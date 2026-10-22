// UIA-112 · ModeToggles: nút chế độ hỗ trợ vẽ trên thanh trạng thái (UI-A08).
//   toggleSameTurn     tiêu chí 1: bấm nút, phím F → nút đổi trong cùng lượt sự kiện (lõi giả áp 'TÊN giá-trị vào
//                      Sysvars như lệnh trong suốt đặt sysvar); DoD-L: thời gian xử lý, ui_state của UiProbe
//   externalSet        tiêu chí 1: Sysvars::set từ ngoài (giả LISP, .scr) → nút, dấu chọn menu đổi ngay
//   transparentInLine  tiêu chí 2: F8 khi đang LINE gửi 'ORTHOMODE … (trong suốt, không ^C^C), LINE còn chạy; cùng
//                      hàng token như gõ 'ORTHOMODE (bản ghi INT-002 thật: Hoãn)
//   osnapMenu          ô chọn đúng bit OSMODE; bật/tắt từng chế độ gửi mặt nạ đúng, menu mở tiếp
//   polarMenu          góc tăng, góc người dùng → 'POLARANG …; chọn góc thì bật Polar
//   otherMenus         SNAPTYPE, DYNMODE/DYNPROMPT; Isometric (SNAPSTYL, mặt SNAPISOPAIR); "Thiết lập…" →
//                      '_+DSETTINGS <tab>, '_LWEIGHT
//   tooltips           tiêu chí 3: tên, sysvar · phím, mô tả; tên cho trình đọc màn hình
//   withoutColor       tiêu chí 3: bật/tắt khác nhau ở ảnh xám (gạch dưới); trợ năng báo trạng thái chọn
//   viewerProfile      tiêu chí 4: Viewer còn 3 nút; "Thiết lập…" khóa → trang nâng cấp, không gửi lệnh
//   keyboard           DoD-U: Tab tới nút, Space/Enter bật tắt, Shift+F10 / phím Menu mở menu, Esc đóng
//   baseline           DoD-U: ảnh Tối/Sáng × 100/200% (tắt, bật, rê, nhấn, focus bàn phím, vô hiệu); ghi lại:
//                      TRION_UPDATE_BASELINE=<thư mục>
//   pseudoNoTruncation DoD-U: vi-XL không cắt chữ (menu, tooltip), cả gọn và thoáng

#include "app/shell/status/ModeToggles.h"

#include "app/core/actions/ActionRegistry.h"
#include "app/core/i18n/Translator.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/core/keymap/Keymap.h"
#include "app/core/perf/UiProbe.h"
#include "app/core/UiExecutor.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/generated/Tokens.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/tooltip/RichTooltip.h"
#include "core/base/sched/Scheduler.h"
#include "core/base/settings/Settings.h"
#include "tests/timing.h"

#include <QAccessible>
#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QDir>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QtTest>
#include <QVBoxLayout>

#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;
using namespace trion;
using theme::Density;
using theme::Theme;
using Clock = std::chrono::steady_clock;

namespace {

constexpr int kNoise = 8; // baseline: lệch ≤ 8/255 mỗi kênh bỏ qua, điểm lệch hơn được tối đa 0,1% ảnh

// Lõi giả thay INT-002 (bộ định tuyến lệnh): đọc chuỗi lệnh như hàng token. "^C^C" hủy lệnh đang chạy; "'TÊN" là lệnh
// trong suốt (không đụng lệnh đang chạy), "'TÊN giá-trị-nguyên" đặt sysvar như lõi (Sysvars tạm chỉ có số nguyên);
// "'_LỆNH …" là lệnh trong suốt mở hộp thoại; token khác là lệnh thường. Khoảng trắng là Enter.
struct FakeCore {
    actions::Sysvars* sysvars = nullptr;
    bool apply = true;  // false: lõi chưa áp (bận), sysvar giữ nguyên
    QStringList macros; // chuỗi tới chỗ nối, theo thứ tự
    QString running;    // lệnh thường đang chạy

    void submit(const QString& macro)
    {
        macros << macro;
        QStringList tokens = macro.split(u' ', Qt::SkipEmptyParts);
        if (!tokens.isEmpty() && tokens[0].startsWith(u"^C^C"_s)) {
            running.clear();
            tokens[0].remove(0, 4);
        }
        for (qsizetype i = 0; i < tokens.size(); ++i) {
            const QString& t = tokens[i];
            if (t.isEmpty())
                continue;
            if (!t.startsWith(u'\'')) {
                running = t.mid(t.startsWith(u'_') ? 1 : 0);
                continue;
            }
            bool number = false;
            const int value = tokens.value(i + 1).toInt(&number);
            if (number && !t.startsWith(u"'_"_s) && apply)
                sysvars->set(t.mid(1), value);
            i += number ? 1 : 0;
        }
    }
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

QAction* item(const QMenu* m, const QString& text)
{
    for (QAction* a : m->actions())
        if (a->text() == text)
            return a;
    return nullptr;
}

QMenu* menuOf(const QToolButton* b)
{
    return b->findChild<QMenu*>();
}

QString truncations(QWidget* root)
{
    QStringList out;
    for (const i18n::Truncation& t : i18n::findTruncations(root))
        out << t.toString();
    return out.join(u"; "_s);
}

double ms(Clock::duration d)
{
    return std::chrono::duration<double, std::milli>(d).count();
}

double percentile(std::vector<double> v, double p)
{
    std::sort(v.begin(), v.end());
    return v[std::min(v.size() - 1, size_t(p * double(v.size())))];
}

} // namespace

class ModeTogglesTest : public QObject {
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
    std::unique_ptr<QWidget> window_; // cửa sổ chính: vùng vẽ và dải nút
    QWidget* viewport_ = nullptr;
    shell::ModeToggles* modes_ = nullptr;
    FakeCore core_;
    QStringList upgrades_; // trang nâng cấp đã mở (mã tính năng)

    QStringList take() { return std::exchange(core_.macros, {}); }
    QToolButton* button(const char* id) const { return modes_->button(QString::fromLatin1(id)); }

    QString tipText() const // các dòng đang hiện của tooltip
    {
        QStringList out;
        for (QLabel* l : tips_->tipWindow()->findChildren<QLabel*>())
            if (!l->isHidden())
                out << l->text();
        return out.join(u'\n');
    }

    void hover(QToolButton* b)
    {
        tips_->hover(b, QRect(b->mapToGlobal(QPoint(0, 0)), b->size()), b->mapToGlobal(b->rect().center()));
    }

private slots:
    void initTestCase()
    {
        QVERIFY(dir_.isValid());
        store_ = std::make_unique<settings::Store>(dir_.filePath(u"user.json"_s));
        settings::Store::setInstance(store_.get());
        executor_ = std::make_unique<ui::UiExecutor>();
        QVERIFY(theme::loadFonts());
        tm_ = std::make_unique<theme::ThemeManager>(*store_);
        icons_ = std::make_unique<theme::IconProvider>();
        translator_ = std::make_unique<i18n::Translator>();
        keymap_ = std::make_unique<keymap::Keymap>(*store_, keymap::Platform::Windows);
        tips_ = std::make_unique<ui::RichTooltip>();
        tips_->setKeymap(keymap_.get());
        sysvars_ = std::make_unique<actions::Sysvars>();
        core_.sysvars = sysvars_.get();
        reg_ = std::make_unique<actions::ActionRegistry>(*keymap_, *sysvars_);
        reg_->setSink([this](const QString& m) { core_.submit(m); });
        gate_ = std::make_unique<edition::EditionGate>();
        gate_->setEdition(edition::Edition::Pro);
        gate_->bind(*reg_);
        gate_->upgrade().setHandler([this](const edition::UpgradeRequest& r) { upgrades_ << r.feature; });

        window_ = std::make_unique<QWidget>();
        auto* box = new QVBoxLayout(window_.get());
        viewport_ = new QWidget;
        viewport_->setFocusPolicy(Qt::StrongFocus);
        viewport_->setMinimumSize(320, 160);
        modes_ = new shell::ModeToggles(*reg_, *sysvars_);
        box->addWidget(viewport_);
        box->addWidget(modes_);
        connect(modes_, &shell::ModeToggles::macroRequested, this, [this](const QString& m) { core_.submit(m); });
        connect(gate_.get(), &edition::EditionGate::profileChanged, modes_, &shell::ModeToggles::setProfile);
        modes_->setProfile(gate_->profile());
        reg_->install(window_.get());
        reg_->addViewport(viewport_);
        window_->resize(480, 320);
        window_->show();
        window_->activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(window_.get()));
    }

    void init()
    {
        if (QWidget* popup = QApplication::activePopupWidget())
            popup->close();
        window_->activateWindow(); // offscreen: cửa sổ tooltip vừa ẩn không trả lại cửa sổ kích hoạt
        QVERIFY(QTest::qWaitForWindowActive(window_.get()));
        viewport_->setFocus();
        QTRY_COMPARE(QApplication::focusWidget(), viewport_);
        core_.apply = true;
        take();
    }

    void cleanupTestCase()
    {
        window_.reset();
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

    void toggleSameTurn()
    {
        // Đặc tả chuỗi viết tay theo giá trị AutoCAD: bấm đổi từ value; phím F (nút không có phím: bấm lại) đổi về.
        struct Toggle {
            const char* id;
            const char* sysvar;
            int value;
            const char* first;
            const char* back;
            Qt::Key key;
        };
        const Toggle kToggles[] = {
            {"grid", "GRIDMODE", 0, "'GRIDMODE 1", "'GRIDMODE 0", Qt::Key_F7},
            {"snap", "SNAPMODE", 1, "'SNAPMODE 0", "'SNAPMODE 1", Qt::Key_F9},
            {"dyn", "DYNMODE", 3, "'DYNMODE -3", "'DYNMODE 3", Qt::Key_F12}, // tắt bằng đổi dấu, nhớ chế độ
            {"ortho", "ORTHOMODE", 0, "'ORTHOMODE 1", "'ORTHOMODE 0", Qt::Key_F8},
            {"polar", "AUTOSNAP", 63, "'AUTOSNAP 55", "'AUTOSNAP 63", Qt::Key_F10},  // bit 8
            {"otrack", "AUTOSNAP", 63, "'AUTOSNAP 47", "'AUTOSNAP 63", Qt::Key_F11}, // bit 16
            {"osnap", "OSMODE", 4133, "'OSMODE 20517", "'OSMODE 4133", Qt::Key_F3},  // bit 16384 bật là TẮT
            {"LWDISPLAY", "LWDISPLAY", 0, "'LWDISPLAY 1", "'LWDISPLAY 0", Qt::Key_unknown},
            {"TRANSPARENCYDISPLAY", "TRANSPARENCYDISPLAY", 1, "'TRANSPARENCYDISPLAY 0", "'TRANSPARENCYDISPLAY 1",
             Qt::Key_unknown},
        };
        std::vector<double> took;
        for (const Toggle& t : kToggles) {
            QToolButton* b = button(t.id);
            QVERIFY2(b && b->isVisible(), t.id);
            sysvars_->set(QString::fromLatin1(t.sysvar), t.value);
            const bool was = b->isChecked();
            auto t0 = Clock::now();
            QTest::mouseClick(b, Qt::LeftButton);
            took.push_back(ms(Clock::now() - t0));
            QCOMPARE(b->isChecked(), !was); // ngay khi sự kiện bấm xử lý xong: lõi giả áp lệnh trong lời gọi sink
            QCOMPARE(take(), QStringList{QString::fromLatin1(t.first)});
            t0 = Clock::now();
            if (t.key != Qt::Key_unknown)
                QTest::keyClick(viewport_, t.key);
            else
                QTest::mouseClick(b, Qt::LeftButton);
            took.push_back(ms(Clock::now() - t0));
            QCOMPARE(b->isChecked(), was);
            QCOMPARE(take(), QStringList{QString::fromLatin1(t.back)});
        }

        // Lõi chưa áp (bận): bấm chỉ gửi lệnh, nút không tự đảo
        core_.apply = false;
        QToolButton* grid = button("grid");
        const bool was = grid->isChecked();
        QTest::mouseClick(grid, Qt::LeftButton);
        QCOMPARE(grid->isChecked(), was);
        QCOMPARE(take().size(), 1);
        core_.apply = true;

        QToolButton* ortho = button("ortho");
        for (int i = 0; i < 100; ++i) { // thêm mẫu cho p99: F8 liên tiếp
            const bool on = ortho->isChecked();
            const auto t0 = Clock::now();
            QTest::keyClick(viewport_, Qt::Key_F8);
            took.push_back(ms(Clock::now() - t0));
            QCOMPARE(ortho->isChecked(), !on);
        }
        QCOMPARE(take().size(), 100);

        const double p50 = percentile(took, 0.5), p99 = percentile(took, 0.99);
        qInfo("bấm/phím → nút đổi (đồng bộ): p50 %.3f ms, p99 %.3f ms (%zu lần)", p50, p99, took.size());
        QVERIFY2(!test::kTimed || p99 <= 16.0, qPrintable(QString::number(p99))); // trong 1 khung 60 Hz

        // UiProbe: bấm chuột tới cửa sổ đến hết khung kế tiếp (ui_state, DoD-L ≤ 100 ms). Chỉ báo số: máy chung,
        // khung offscreen đến theo timer, probe bỏ mẫu khi vòng sự kiện rảnh trước khung.
        ui::probe::reset();
        ui::probe::enable();
        const QPoint at = window_->mapFromGlobal(ortho->mapToGlobal(ortho->rect().center()));
        for (int i = 0; i < 20; ++i) {
            QTest::mouseClick(window_->windowHandle(), Qt::LeftButton, {}, at);
            QTest::qWait(20);
        }
        const QJsonObject ops = ui::probe::toJson().value(u"ops"_s).toObject();
        ui::probe::disable();
        const QJsonObject state = ops.value(u"ui_state"_s).toObject();
        qInfo("UiProbe ui_state: p50 %.2f / p99 %.2f ms (%d mẫu)", state.value(u"p50_ms"_s).toDouble(),
              state.value(u"p99_ms"_s).toDouble(), state.value(u"count"_s).toInt());
        QCOMPARE(take().size(), 20);
    }

    void externalSet()
    {
        QToolButton* grid = button("grid");
        QToolButton* osnap = button("osnap");
        QMenu* m = menuOf(osnap);
        sysvars_->set(u"GRIDMODE"_s, 0);
        QVERIFY(!grid->isChecked());
        sysvars_->set(u"GRIDMODE"_s, 1); // (setvar "GRIDMODE" 1) của LISP, dòng .scr: đổi ngay, không chờ vòng sự kiện
        QVERIFY(grid->isChecked());
        sysvars_->set(u"OSMODE"_s, 1);
        QVERIFY(osnap->isChecked());
        QVERIFY(item(m, u"Điểm cuối"_s)->isChecked());
        QVERIFY(!item(m, u"Trung điểm"_s)->isChecked());
        sysvars_->set(u"OSMODE"_s, 1 | 16384); // OSNAP tắt, giữ chế độ
        QVERIFY(!osnap->isChecked());
        QVERIFY(item(m, u"Điểm cuối"_s)->isChecked());
        QCOMPARE(take(), QStringList());                    // đổi từ ngoài không gửi lệnh ngược lại
        QVERIFY(modes_->findChildren<QTimer*>().isEmpty()); // theo tín hiệu, không timer, không polling
    }

    void transparentInLine()
    {
        QToolButton* ortho = button("ortho");
        sysvars_->set(u"ORTHOMODE"_s, 0);
        reg_->action(u"LINE"_s)->trigger();
        QCOMPARE(take(), QStringList{u"^C^C_LINE"_s});
        QCOMPARE(core_.running, u"LINE"_s);

        QTest::keyClick(viewport_, Qt::Key_F8); // F8 giữa LINE
        const QStringList f8 = take();
        QCOMPARE(f8, QStringList{u"'ORTHOMODE 1"_s}); // trong suốt, không ^C^C
        QCOMPARE(core_.running, u"LINE"_s);           // LINE không bị ngắt
        QVERIFY(ortho->isChecked());
        QTest::mouseClick(ortho, Qt::LeftButton); // bấm nút giữa LINE: như F8
        QCOMPARE(take(), QStringList{u"'ORTHOMODE 0"_s});
        QCOMPARE(core_.running, u"LINE"_s);

        // Gõ 'ORTHOMODE trên dòng lệnh: tên lệnh trong suốt, Enter, giá trị, Enter: cùng hàng token với F8.
        // ponytail: so hàng token tới chỗ nối; bản ghi INT-002 thật (InputPipeline::submit) Hoãn tới INT-002.
        QCOMPARE(f8.first().split(u' '), (QStringList{u"'ORTHOMODE"_s, u"1"_s}));
        core_.submit(u"^C^C"_s);
        QVERIFY(core_.running.isEmpty());
    }

    void osnapMenu()
    {
        QToolButton* osnap = button("osnap");
        QMenu* m = menuOf(osnap);
        QVERIFY(m);
        sysvars_->set(u"OSMODE"_s, 4133); // Điểm cuối, Tâm, Giao điểm, Kéo dài
        // Bit OSMODE của AutoCAD, viết tay
        const std::pair<const char*, int> kModes[] = {
            {"Điểm cuối", 1},         {"Trung điểm", 2},   {"Tâm", 4},
            {"Tâm hình học", 1024},   {"Điểm", 8},         {"Góc phần tư", 16},
            {"Giao điểm", 32},        {"Kéo dài", 4096},   {"Điểm chèn", 64},
            {"Vuông góc", 128},       {"Tiếp xúc", 256},   {"Gần nhất", 512},
            {"Giao biểu kiến", 2048}, {"Song song", 8192},
        };
        for (const auto& [label, bit] : kModes) {
            QAction* a = item(m, QString::fromUtf8(label));
            QVERIFY2(a && a->isCheckable(), label);
            QCOMPARE(a->isChecked(), (4133 & bit) != 0);
        }

        // Chuột phải mở menu; bấm một chế độ: gửi mặt nạ mới, menu mở tiếp, dấu chọn theo sysvar mới
        const QPoint at = osnap->rect().center();
        QContextMenuEvent open(QContextMenuEvent::Mouse, at, osnap->mapToGlobal(at));
        QApplication::sendEvent(osnap, &open);
        QVERIFY(m->isVisible());
        QAction* mid = item(m, u"Trung điểm"_s);
        QTest::mouseClick(m, Qt::LeftButton, {}, m->actionGeometry(mid).center());
        QCOMPARE(take(), QStringList{u"'OSMODE 4135"_s});
        QVERIFY(m->isVisible());
        QVERIFY(mid->isChecked());
        m->setActiveAction(mid);
        QTest::keyClick(m, Qt::Key_Return); // bàn phím: Enter cũng chỉ bật/tắt
        QCOMPARE(take(), QStringList{u"'OSMODE 4133"_s});
        QVERIFY(m->isVisible());
        QVERIFY(!mid->isChecked());
        core_.apply = false; // lõi chưa áp: dấu chọn không tự đảo
        QTest::mouseClick(m, Qt::LeftButton, {}, m->actionGeometry(mid).center());
        QCOMPARE(take(), QStringList{u"'OSMODE 4135"_s});
        QVERIFY(!mid->isChecked());
        core_.apply = true;
        QTest::keyClick(m, Qt::Key_Escape);
        QVERIFY(!m->isVisible());

        // OSNAP đang tắt (bit 16384): đổi chế độ, chọn/bỏ tất cả giữ bit tắt
        sysvars_->set(u"OSMODE"_s, 4133 | 16384);
        item(m, u"Song song"_s)->trigger();
        QCOMPARE(take(), QStringList{u"'OSMODE 28709"_s});
        item(m, u"Chọn tất cả"_s)->trigger();
        QCOMPARE(take(), QStringList{u"'OSMODE 32767"_s});
        item(m, u"Bỏ tất cả"_s)->trigger();
        QCOMPARE(take(), QStringList{u"'OSMODE 16384"_s});
        sysvars_->set(u"OSMODE"_s, 4133);
        item(m, u"Chọn tất cả"_s)->trigger();
        QCOMPARE(take(), QStringList{u"'OSMODE 16383"_s});
        item(m, u"Bỏ tất cả"_s)->trigger();
        QCOMPARE(take(), QStringList{u"'OSMODE 0"_s});
        item(m, u"Thiết lập bắt điểm…"_s)->trigger();
        QCOMPARE(take(), QStringList{u"'_+DSETTINGS 2"_s});
    }

    void polarMenu()
    {
        QToolButton* polar = button("polar");
        QMenu* m = menuOf(polar);
        sysvars_->set(u"AUTOSNAP"_s, 55); // Polar tắt (bit 8)
        sysvars_->set(u"POLARANG"_s, 45);
        QStringList shown;
        for (QAction* a : m->actions())
            if (a->isVisible() && !a->isSeparator())
                shown << a->text();
        QCOMPARE(shown, (QStringList{u"90°"_s, u"45°"_s, u"30°"_s, u"22.5°"_s, u"18°"_s, u"15°"_s, u"10°"_s, u"5°"_s,
                                     u"Góc khác…"_s, u"Thiết lập theo dõi…"_s}));
        QVERIFY(item(m, u"45°"_s)->isChecked());
        QVERIFY(!item(m, u"90°"_s)->isChecked());

        item(m, u"30°"_s)->trigger(); // Polar đang tắt: đặt góc rồi bật Polar
        QCOMPARE(take(), (QStringList{u"'POLARANG 30"_s, u"'AUTOSNAP 63"_s}));
        QVERIFY(polar->isChecked());
        QVERIFY(item(m, u"30°"_s)->isChecked());
        QVERIFY(!item(m, u"45°"_s)->isChecked());
        item(m, u"22.5°"_s)->trigger(); // Polar đã bật: chỉ đặt góc; số thực dấu chấm thập phân
        QCOMPARE(take(), QStringList{u"'POLARANG 22.5"_s});
        // ponytail: Sysvars tạm chỉ có số nguyên, lõi giả không đặt được 22.5: dấu chọn còn ở 30 (INT-005 thay)
        QVERIFY(item(m, u"30°"_s)->isChecked());

        // Góc người dùng (POLARANG ngoài danh sách, đặt từ dòng lệnh hay hộp thoại): hiện và được chọn
        sysvars_->set(u"POLARANG"_s, 7);
        QAction* user = item(m, u"7°"_s);
        QVERIFY(user && user->isVisible() && user->isChecked());
        QVERIFY(!item(m, u"30°"_s)->isChecked());
        user->trigger();
        QCOMPARE(take(), QStringList{u"'POLARANG 7"_s});
        QVERIFY(user->isChecked());
        sysvars_->set(u"POLARANG"_s, 90);
        QVERIFY(!user->isVisible());
        item(m, u"Góc khác…"_s)->trigger(); // không kèm giá trị: dòng lệnh hỏi
        QCOMPARE(take(), QStringList{u"'POLARANG"_s});
        item(m, u"Thiết lập theo dõi…"_s)->trigger();
        QCOMPARE(take(), QStringList{u"'_+DSETTINGS 1"_s});
    }

    void otherMenus()
    {
        QMenu* snap = menuOf(button("snap"));
        sysvars_->set(u"SNAPTYPE"_s, 0);
        QVERIFY(item(snap, u"Snap lưới"_s)->isChecked());
        QVERIFY(!item(snap, u"Snap polar"_s)->isChecked());
        item(snap, u"Snap polar"_s)->trigger();
        QCOMPARE(take(), QStringList{u"'SNAPTYPE 1"_s});
        QVERIFY(item(snap, u"Snap polar"_s)->isChecked());
        QVERIFY(!item(snap, u"Snap lưới"_s)->isChecked());

        QMenu* dyn = menuOf(button("dyn"));
        sysvars_->set(u"DYNMODE"_s, 3);
        sysvars_->set(u"DYNPROMPT"_s, 1);
        QAction* pointer = item(dyn, u"Nhập con trỏ"_s);
        QAction* dimension = item(dyn, u"Nhập kích thước"_s);
        QAction* prompt = item(dyn, u"Prompt động"_s);
        QVERIFY(pointer->isChecked() && dimension->isChecked() && prompt->isChecked());
        pointer->trigger();
        QCOMPARE(take(), QStringList{u"'DYNMODE 2"_s});
        QVERIFY(!pointer->isChecked() && dimension->isChecked());
        sysvars_->set(u"DYNMODE"_s, -3); // DYN tắt (F12) vẫn nhớ chế độ: đổi chế độ giữ trạng thái tắt
        QVERIFY(!button("dyn")->isChecked());
        QVERIFY(pointer->isChecked());
        dimension->trigger();
        QCOMPARE(take(), QStringList{u"'DYNMODE -1"_s});
        QVERIFY(!button("dyn")->isChecked() && !dimension->isChecked());
        prompt->trigger();
        QCOMPARE(take(), QStringList{u"'DYNPROMPT 0"_s});
        QVERIFY(!prompt->isChecked());

        // Isometric (P1, hành động "iso" gắn SNAPSTYL của ActionRegistry): bật/tắt, menu mặt trái/trên/phải
        QToolButton* iso = button("iso");
        QVERIFY(iso);
        sysvars_->set(u"SNAPSTYL"_s, 0);
        QVERIFY(!iso->isChecked());
        iso->click();
        QCOMPARE(take(), QStringList{u"'SNAPSTYL 1"_s});
        QVERIFY(iso->isChecked());
        QMenu* faces = menuOf(iso);
        sysvars_->set(u"SNAPISOPAIR"_s, 0);
        QVERIFY(item(faces, u"Mặt trái"_s)->isChecked());
        item(faces, u"Mặt phải"_s)->trigger();
        QCOMPARE(take(), QStringList{u"'SNAPISOPAIR 2"_s});
        QVERIFY(item(faces, u"Mặt phải"_s)->isChecked() && !item(faces, u"Mặt trái"_s)->isChecked());
        sysvars_->set(u"SNAPSTYL"_s, 0);

        // "Thiết lập…": tab +DSETTINGS của AutoCAD theo prototype (tabOf); lineweight mở LWEIGHT
        const std::pair<const char*, const char*> kSettings[] = {
            {"grid", "'_+DSETTINGS 0"},  {"snap", "'_+DSETTINGS 0"},   {"dyn", "'_+DSETTINGS 4"},
            {"ortho", "'_+DSETTINGS 0"}, {"otrack", "'_+DSETTINGS 2"}, {"LWDISPLAY", "'_LWEIGHT"},
            {"iso", "'_+DSETTINGS 0"},
        };
        for (const auto& [id, macro] : kSettings) {
            QMenu* m = menuOf(button(id));
            QAction* settings = m ? item(m, u"Thiết lập…"_s) : nullptr;
            QVERIFY2(settings, id);
            settings->trigger();
            QCOMPARE(take(), QStringList{QString::fromLatin1(macro)});
        }
        QVERIFY(!menuOf(button("TRANSPARENCYDISPLAY"))); // 07.1 §10: không có menu
    }

    void tooltips()
    {
        tips_->setDelays(1ms, 1ms);
        struct Tip {
            const char* id;
            const char* name;
            const char* line; // dòng 2: sysvar · phím theo Keymap (Windows)
        };
        const Tip kTips[] = {
            {"grid", "Lưới", "GRIDMODE · F7 · Ctrl+G"},
            {"snap", "Snap", "SNAPMODE · F9 · Ctrl+B"},
            {"dyn", "Nhập động", "DYNMODE · F12"},
            {"ortho", "Ortho", "ORTHOMODE · F8 · Ctrl+L"},
            {"polar", "Polar", "AUTOSNAP · F10 · Ctrl+U"},
            {"otrack", "Theo dõi bắt điểm", "AUTOSNAP · F11"},
            {"osnap", "Bắt điểm", "OSMODE · F3"},
            {"LWDISPLAY", "Hiển thị độ dày nét", "LWDISPLAY"},
            {"TRANSPARENCYDISPLAY", "Hiển thị độ trong suốt", "TRANSPARENCYDISPLAY"},
        };
        for (const Tip& t : kTips) {
            QToolButton* b = button(t.id);
            const QString name = QString::fromUtf8(t.name), line = QString::fromUtf8(t.line);
            QCOMPARE(b->accessibleName(), name);
            hover(b);
            QVERIFY(spinUntil([&] { return tips_->level() == 2; }));
            const QStringList lines = tipText().split(u'\n');
            QCOMPARE(lines.value(0), name);
            QCOMPARE(lines.value(1), line);
            const QString sysvar = line.section(u' ', 0, 0);
            QVERIFY2(lines.value(2).contains(u"Biến hệ thống %1."_s.arg(sysvar)), qPrintable(lines.value(2)));
            QVERIFY(b->accessibleDescription().contains(sysvar));
            tips_->leave();
        }
    }

    void withoutColor()
    {
        QToolButton* b = button("ortho");
        QTest::mouseMove(viewport_, QPoint(5, 5)); // không rê trên nút
        b->setAttribute(Qt::WA_UnderMouse, false);
        const auto gray = [&] { // nút trên nền thanh trạng thái, ảnh xám
            QImage img(b->size(), QImage::Format_ARGB32_Premultiplied);
            img.fill(tm_->color(tok::c_chrome));
            b->render(&img, {}, {}, QWidget::DrawChildren);
            return img.convertToFormat(QImage::Format_Grayscale8);
        };
        for (const bool light : {false, true}) {
            tm_->setTheme(light ? Theme::Light : Theme::Dark);
            QCoreApplication::processEvents();
            sysvars_->set(u"ORTHOMODE"_s, 0);
            const QImage off = gray();
            sysvars_->set(u"ORTHOMODE"_s, 1);
            const QImage on = gray();
            // Gạch dưới (cao --sp-1, lùi --sp-2 hai bên) khác rõ ở ảnh xám: phân biệt bằng hình, không cần màu
            const int inset = qRound(tm_->px(tok::sp_2)), h = qRound(tm_->px(tok::sp_1));
            int weak = 0;
            for (int y = on.height() - h; y < on.height(); ++y)
                for (int x = inset; x < on.width() - inset; ++x)
                    weak += std::abs(on.constScanLine(y)[x] - off.constScanLine(y)[x]) <= 64;
            QVERIFY2(weak == 0, light ? "Sáng" : "Tối");
        }
        tm_->setTheme(Theme::Dark);
        QCoreApplication::processEvents();

        // Trình đọc màn hình: nút bật/tắt, trạng thái chọn theo sysvar
        QAccessibleInterface* ai = QAccessible::queryAccessibleInterface(b);
        QVERIFY(ai && ai->state().checkable);
        QVERIFY(ai->state().checked);
        sysvars_->set(u"ORTHOMODE"_s, 0);
        QVERIFY(!ai->state().checked);
    }

    void viewerProfile()
    {
        auto restore = qScopeGuard([this] { gate_->setEdition(edition::Edition::Pro); });
        const auto visible = [this] {
            QStringList ids;
            for (QToolButton* b : modes_->findChildren<QToolButton*>())
                if (b->isVisible())
                    ids << b->defaultAction()->objectName();
            return ids;
        };
        gate_->setEdition(edition::Edition::Viewer); // profileChanged → setProfile(Viewer)
        QCOMPARE(visible(), (QStringList{u"osnap"_s, u"LWDISPLAY"_s, u"TRANSPARENCYDISPLAY"_s}));
        QToolButton* osnap = button("osnap");
        QCOMPARE(osnap->accessibleName(), u"Bắt điểm (khi đo)"_s);

        // Bắt điểm khi đo: F3 và chế độ dùng được (OSMODE có ở Viewer)
        sysvars_->set(u"OSMODE"_s, 4133);
        QTest::keyClick(viewport_, Qt::Key_F3);
        QCOMPARE(take(), QStringList{u"'OSMODE 20517"_s});
        QMenu* m = menuOf(osnap);
        m->popup(osnap->mapToGlobal(QPoint(0, 0))); // aboutToShow: huy hiệu khóa của "Thiết lập…"
        QAction* settings = item(m, u"Thiết lập bắt điểm…"_s);
        QCOMPARE(ui::RichTooltip::info(settings).need, ui::Edition::Pro);
        item(m, u"Trung điểm"_s)->trigger();
        QCOMPARE(take(), QStringList{u"'OSMODE 20519"_s});
        settings->trigger(); // khóa: trang nâng cấp, không gửi lệnh
        QCOMPARE(take(), QStringList());
        QCOMPARE(upgrades_, QStringList{u"DSETTINGS"_s});
        m->hide();
        QMenu* lw = menuOf(button("LWDISPLAY"));
        lw->popup(osnap->mapToGlobal(QPoint(0, 0)));
        QAction* lwSettings = item(lw, u"Thiết lập…"_s);
        QCOMPARE(ui::RichTooltip::info(lwSettings).need, ui::Edition::Pro); // LWEIGHT: Pro trong danh mục lệnh
        lwSettings->trigger();
        lw->hide();
        QCOMPARE(take(), QStringList());
        QCOMPARE(upgrades_, (QStringList{u"DSETTINGS"_s, u"LWEIGHT"_s}));

        restore.dismiss();
        gate_->setEdition(edition::Edition::Pro);
        QCOMPARE(visible().size(), 10); // 9 nút EA + Isometric (P1)
        QCOMPARE(osnap->accessibleName(), u"Bắt điểm"_s);
        m->popup(osnap->mapToGlobal(QPoint(0, 0)));
        QCOMPARE(ui::RichTooltip::info(settings).need, ui::Edition::None);
        m->hide();
    }

    void keyboard()
    {
        QToolButton* grid = button("grid");
        QToolButton* snap = button("snap");
        QTest::keyClick(viewport_, Qt::Key_Tab); // vùng vẽ → nút đầu
        QCOMPARE(QApplication::focusWidget(), grid);
        sysvars_->set(u"GRIDMODE"_s, 0);
        QTest::keyClick(grid, Qt::Key_Space);
        QCOMPARE(take(), QStringList{u"'GRIDMODE 1"_s});
        QVERIFY(grid->isChecked());
        QTest::keyClick(grid, Qt::Key_Return);
        QCOMPARE(take(), QStringList{u"'GRIDMODE 0"_s});
        QVERIFY(!grid->isChecked());
        QTest::keyClick(grid, Qt::Key_Tab);
        QCOMPARE(QApplication::focusWidget(), snap);

        // Shift+F10 mở menu của nút; mũi tên chọn chế độ, Enter bật/tắt (menu mở tiếp), Esc đóng, focus về nút
        QToolButton* osnap = button("osnap");
        osnap->setFocus(Qt::TabFocusReason);
        QMenu* m = menuOf(osnap);
        sysvars_->set(u"OSMODE"_s, 4133);
        QTest::keyClick(osnap, Qt::Key_F10, Qt::ShiftModifier);
        QVERIFY(m->isVisible());
        QTest::keyClick(m, Qt::Key_Down);
        QCOMPARE(m->activeAction(), item(m, u"Điểm cuối"_s));
        QTest::keyClick(m, Qt::Key_Return);
        QCOMPARE(take(), QStringList{u"'OSMODE 4132"_s});
        QVERIFY(m->isVisible());
        QTest::keyClick(m, Qt::Key_Escape);
        QVERIFY(!m->isVisible());
        QTRY_COMPARE(QApplication::focusWidget(), osnap);
        QTest::keyClick(osnap, Qt::Key_Menu);
        QVERIFY(m->isVisible());
        QTest::keyClick(m, Qt::Key_Escape);
        QVERIFY(!m->isVisible());
    }

    void baseline_data()
    {
        QTest::addColumn<bool>("light");
        QTest::addColumn<qreal>("dpr");
        for (const bool light : {false, true})
            for (const qreal dpr : {1.0, 2.0})
                QTest::addRow("%s-%d", light ? "light" : "dark", qRound(dpr * 100)) << light << dpr;
    }

    // Hàng 1: dải nút bật/tắt xen kẽ. Hàng 2: rê (tắt), rê (bật), nhấn, focus bàn phím, vô hiệu.
    void baseline()
    {
        QFETCH(bool, light);
        QFETCH(qreal, dpr);
        tm_->setTheme(light ? Theme::Light : Theme::Dark);
        QCoreApplication::processEvents(); // Qt 6.11 gửi ApplicationPaletteChange qua hàng đợi
        const std::pair<const char*, int> kState[] = {
            {"GRIDMODE", 0}, {"SNAPMODE", 1},  {"DYNMODE", 3},   {"ORTHOMODE", 0},           {"AUTOSNAP", 47},
            {"SNAPSTYL", 0}, {"OSMODE", 4133}, {"LWDISPLAY", 0}, {"TRANSPARENCYDISPLAY", 1},
        };
        for (const auto& [name, value] : kState)
            sysvars_->set(QString::fromLatin1(name), value);
        for (QToolButton* b : modes_->findChildren<QToolButton*>())
            b->setAttribute(Qt::WA_UnderMouse, false);

        const int pad = 8, gap = 8;
        const QSize strip = modes_->size();
        QImage sheet(QSize(strip.width() + 2 * pad, 2 * strip.height() + 2 * pad + gap) * dpr,
                     QImage::Format_ARGB32_Premultiplied);
        sheet.setDevicePixelRatio(dpr);
        sheet.fill(tm_->color(tok::c_chrome));
        QPainter p(&sheet);
        modes_->render(&p, QPoint(pad, pad), {}, QWidget::DrawChildren);
        int x = pad;
        const int y = pad + strip.height() + gap;
        const auto draw = [&](QToolButton* b) {
            b->render(&p, QPoint(x, y), {}, QWidget::DrawChildren);
            x += b->width() + gap;
        };
        for (const char* id : {"grid", "snap"}) { // rê: tắt, bật
            button(id)->setAttribute(Qt::WA_UnderMouse, true);
            draw(button(id));
            button(id)->setAttribute(Qt::WA_UnderMouse, false);
        }
        button("ortho")->setDown(true);
        draw(button("ortho"));
        button("ortho")->setDown(false);
        button("dyn")->setFocus(Qt::TabFocusReason);
        window_->setAttribute(Qt::WA_KeyboardFocusChange); // như phím Tab thật (setFocus ngoài phím Tab xóa cờ)
        QVERIFY(button("dyn")->hasFocus());
        draw(button("dyn"));
        viewport_->setFocus();
        window_->setAttribute(Qt::WA_KeyboardFocusChange, false);
        button("LWDISPLAY")->setEnabled(false);
        draw(button("LWDISPLAY"));
        button("LWDISPLAY")->setEnabled(true);
        p.end();

        // Không có chữ: một bộ ảnh cho mọi nền tảng
        const QString name = u"mode-toggles-%1-%2.png"_s.arg(light ? u"light"_s : u"dark"_s).arg(qRound(dpr * 100));
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
        for (int yy = 0; yy < sheet.height(); ++yy) {
            const auto* a = reinterpret_cast<const QRgb*>(sheet.constScanLine(yy));
            const auto* b = reinterpret_cast<const QRgb*>(want.constScanLine(yy));
            for (int xx = 0; xx < sheet.width(); ++xx) {
                const int d =
                    std::max({std::abs(qRed(a[xx]) - qRed(b[xx])), std::abs(qGreen(a[xx]) - qGreen(b[xx])),
                              std::abs(qBlue(a[xx]) - qBlue(b[xx])), std::abs(qAlpha(a[xx]) - qAlpha(b[xx]))});
                maxDiff = std::max(maxDiff, d);
                off += d > kNoise;
            }
        }
        qInfo("%s: %lld điểm ảnh lệch > %d/255, lệch lớn nhất %d/255", qPrintable(name), qint64(off), kNoise, maxDiff);
        if (off > sheet.width() * sheet.height() / 1000)
            sheet.save(QDir::temp().filePath(u"got-"_s + name)); // để so bằng mắt
        QVERIFY(off <= sheet.width() * sheet.height() / 1000);
    }

    // DoD-U: chữ dài hơn 40% (vi-XL) không bị cắt: menu (cả menu đang đóng), tooltip; gọn và thoáng.
    void pseudoNoTruncation()
    {
        QToolButton* osnap = button("osnap");
        const QString source = osnap->accessibleName();
        QVERIFY(translator_->setLanguage(i18n::kPseudoLanguage.toString()));
        QVERIFY(spinUntil([&] { return osnap->accessibleName().size() > source.size(); }));
        for (const Density d : {Density::Comfortable, Density::Compact}) {
            tm_->setDensity(d);
            QVERIFY(spinUntil([&] { return truncations(modes_).isEmpty(); }, 5s));
            QCOMPARE(truncations(modes_), QString());
        }
        tips_->setDelays(1ms, 1ms);
        hover(osnap);
        QVERIFY(spinUntil([&] { return tips_->level() == 2; }));
        QCOMPARE(truncations(tips_->tipWindow()), QString());
        tips_->leave();
        QVERIFY(translator_->setLanguage(u"vi"_s));
        QVERIFY(spinUntil([&] { return osnap->accessibleName() == source; }));
    }
};

QTEST_MAIN(ModeTogglesTest)
#include "mode_toggles_test.moc"
