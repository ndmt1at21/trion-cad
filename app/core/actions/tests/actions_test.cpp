// UI0-015 · ActionRegistry: hành động → chuỗi lệnh, toggle gắn sysvar, phím theo phạm vi Toàn cục/Vùng vẽ.
//   everyActionMacro  tiêu chí 1: mọi hành động bấm qua nút (ribbon) cho đúng một chuỗi lệnh như đặc tả; mọi lệnh của
//                     danh mục và mọi hành động KDEF của Keymap có QAction
//   surfacesAndKeys   tiêu chí 1: mục Menu, nút QToolBar, mọi phím của Keymap cho đúng chuỗi của hành động giữ phím
//   sysvarTwoWay      tiêu chí 2: sysvar đổi từ nơi khác → QAction và nút đổi ngay (đồng bộ), bấm không tự đảo
//   cleanScreenCycle  UIA-001: Ctrl+0 gửi '_CLEANSCREENON hoặc '_CLEANSCREENOFF theo CLEANSCREENSTATE; integration
//                     nhóm 5: Ctrl+9 gửi '_COMMANDLINE hoặc '_COMMANDLINEHIDE theo CLISTATE
//   viewportScope     tiêu chí 3: phím Vùng vẽ không chạy ở ô văn bản; Ctrl+C ở ô là chép chữ, ở vùng vẽ là COPYCLIP
//   latency           tiêu chí 4: phím/bấm → chuỗi tới chỗ nối INT-002; không timer
//   keymapFollows, retranslate, gate, metadata: phím đổi theo Keymap, nhãn theo ngôn ngữ, cổng UI0-011, phiên bản

#include "app/core/actions/ActionRegistry.h"

#include "app/core/i18n/Translator.h"
#include "app/core/keymap/Keymap.h"
#include "app/core/UiExecutor.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/cmdline/CommandInput.h"
#include "app/widgets/menu/Menu.h"
#include "app/widgets/tooltip/RichTooltip.h"
#include "core/base/sched/Scheduler.h"
#include "core/base/settings/Settings.h"
#include "tests/timing.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QLineEdit>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QtTest>
#include <QVBoxLayout>

#include <algorithm>
#include <chrono>
#include <memory>
#include <utility>
#include <vector>

using namespace Qt::StringLiterals;
using namespace trion;
using namespace trion::actions;
using Clock = std::chrono::steady_clock;

namespace {

// Đặc tả chuỗi lệnh (tiêu chí 1), viết tay, không suy từ mã. Lệnh thường: ^C^C_TÊN; lệnh trong suốt: '_TÊN.
const QSet<QString> kTransparentSpec = {
    u"ATTDISP"_s,      u"BASE"_s,     u"COLOR"_s,         u"DDPTYPE"_s,     u"DIST"_s,       u"DSETTINGS"_s,
    u"HELP"_s,         u"ID"_s,       u"LAYER"_s,         u"LINETYPE"_s,    u"LWEIGHT"_s,    u"PAN"_s,
    u"QUICKCALC"_s,    u"SCRIPT"_s,   u"SETVAR"_s,        u"STYLE"_s,       u"TEXTSCR"_s,    u"UNITS"_s,
    u"ZOOM"_s,         u"ADCENTER"_s, u"CLEANSCREENON"_s, u"COMMANDLINE"_s, u"PROPERTIES"_s, u"SHEETSET"_s,
    u"TOOLPALETTES"_s, u"VPNEXT"_s,
};

// Hành động gắn sysvar: giá trị đặt trước, chuỗi khi bấm, trạng thái chọn (cycle: không chọn được).
struct SysvarSpec {
    const char* id;
    const char* sysvar;
    int value;
    const char* macro;
    bool on;
};
const SysvarSpec kSysvarSpec[] = {
    {"osnap", "OSMODE", 4133, "'OSMODE 20517", true}, // bit 16384 bật là TẮT
    {"grid", "GRIDMODE", 0, "'GRIDMODE 1", false},
    {"snap", "SNAPMODE", 1, "'SNAPMODE 0", true},
    {"ortho", "ORTHOMODE", 0, "'ORTHOMODE 1", false},
    {"polar", "AUTOSNAP", 63, "'AUTOSNAP 55", true},  // bit 8
    {"otrack", "AUTOSNAP", 63, "'AUTOSNAP 47", true}, // bit 16
    {"dyn", "DYNMODE", 3, "'DYNMODE -3", true},       // tắt bằng đổi dấu
    {"qp", "QPMODE", 0, "'QPMODE 1", false},
    {"infer", "CONSTRAINTINFER", 0, "'CONSTRAINTINFER 1", false},
    {"cycle", "SELECTIONCYCLING", -2, "'SELECTIONCYCLING 2", false},
    {"isop", "SNAPISOPAIR", 2, "'SNAPISOPAIR 0", false}, // Phải → Trái
    {"iso", "SNAPSTYL", 0, "'SNAPSTYL 1", false},        // nút Isometric (UIA-112)
    {"coord", "COORDS", 1, "'COORDS 2", false},
    {"PICKSTYLE", "PICKSTYLE", 3, "'PICKSTYLE 2", true},
    {"LWDISPLAY", "LWDISPLAY", 0, "'LWDISPLAY 1", false},
    {"TRANSPARENCYDISPLAY", "TRANSPARENCYDISPLAY", 1, "'TRANSPARENCYDISPLAY 0", true},
    {"MENUBAR", "MENUBAR", 0, "'MENUBAR 1", false}, // "Hiện thanh menu" của QAT (UIA-101)
};

const SysvarSpec* sysvarSpec(const QString& id)
{
    for (const SysvarSpec& s : kSysvarSpec)
        if (id == QLatin1StringView(s.id))
            return &s;
    return nullptr;
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

class ActionsTest : public QObject {
    Q_OBJECT

    QTemporaryDir dir_;
    std::unique_ptr<settings::Store> store_;
    std::unique_ptr<ui::UiExecutor> executor_;
    std::unique_ptr<theme::ThemeManager> tm_;
    std::unique_ptr<theme::IconProvider> icons_;
    std::unique_ptr<i18n::Translator> translator_;
    std::unique_ptr<keymap::Keymap> keymap_;
    std::unique_ptr<ui::RichTooltip> tips_;
    std::unique_ptr<Sysvars> sysvars_;
    std::unique_ptr<ActionRegistry> reg_;
    std::unique_ptr<QWidget> window_; // cửa sổ chính: vùng vẽ, dòng lệnh, ô văn bản của palette
    QWidget* viewport_ = nullptr;
    ui::CommandInput* cmdline_ = nullptr;
    QLineEdit* field_ = nullptr;
    QStringList sent_; // chuỗi tới chỗ nối INT-002

    QStringList take() { return std::exchange(sent_, {}); }

    static void focus(QWidget* w)
    {
        w->setFocus();
        QTRY_COMPARE(QApplication::focusWidget(), w);
    }

    // Phím cho QAction đang giữ tổ hợp c, theo Keymap (nền tảng Windows trong test).
    static void press(QWidget* w, QKeyCombination c) { QTest::keyClick(w, c.key(), c.keyboardModifiers()); }

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
        sysvars_ = std::make_unique<Sysvars>();

        const auto t0 = Clock::now();
        reg_ = std::make_unique<ActionRegistry>(*keymap_, *sysvars_);
        qInfo("ActionRegistry: %lld hành động dựng trong %.2f ms", qlonglong(reg_->actions().size()),
              ms(Clock::now() - t0));
        reg_->setSink([this](const QString& m) { sent_ << m; });

        window_ = std::make_unique<QWidget>();
        auto* box = new QVBoxLayout(window_.get());
        viewport_ = new QWidget;
        viewport_->setFocusPolicy(Qt::StrongFocus);
        viewport_->setMinimumSize(200, 120);
        cmdline_ = new ui::CommandInput;
        field_ = new QLineEdit;
        box->addWidget(viewport_);
        box->addWidget(cmdline_);
        box->addWidget(field_);
        reg_->install(window_.get());
        reg_->addViewport(viewport_);
        window_->resize(480, 320);
        window_->show();
        window_->activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(window_.get()));
    }

    void init()
    {
        focus(viewport_);
        take();
    }

    void cleanupTestCase()
    {
        window_.reset();
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

    void everyActionMacro()
    {
        for (const SysvarSpec& s : kSysvarSpec)
            sysvars_->set(QString::fromLatin1(s.sysvar), s.value);

        QSet<QString> ids;
        QToolButton button(window_.get()); // nút ribbon/toolbar: setDefaultAction như UIA-005
        button.show();
        for (QAction* a : reg_->actions()) {
            const QString id = a->objectName();
            QVERIFY2(!ids.contains(id), qPrintable(id));
            ids << id;
            QVERIFY2(!a->text().isEmpty(), qPrintable(id));
            QCOMPARE(reg_->action(id), a);

            QString want;
            if (const SysvarSpec* s = sysvarSpec(id)) {
                want = QString::fromLatin1(s->macro);
                QCOMPARE(a->isChecked(), s->on);
            } else {
                QCOMPARE(ui::RichTooltip::info(a).command, id); // lệnh thường: id là tên lệnh
                want = (kTransparentSpec.contains(id) ? u"'_"_s : u"^C^C_"_s) + id;
            }
            button.setDefaultAction(a);
            QTest::mouseClick(&button, Qt::LeftButton);
            QCOMPARE(take(), QStringList{want});
            QCOMPARE(reg_->macro(id), want);
        }
        qInfo("%lld hành động, mỗi hành động một chuỗi lệnh đúng đặc tả", qlonglong(ids.size()));

        for (const SysvarSpec& s : kSysvarSpec) // không sót hành động gắn sysvar nào của đặc tả
            QVERIFY2(ids.contains(QString::fromLatin1(s.id)), s.id);
        for (const CommandInfo& c : commands()) { // mọi lệnh của danh mục (tạm thay INT-001)
            QAction* a = reg_->action(QString::fromLatin1(c.name));
            QVERIFY2(a, c.name);
            QVERIFY2(!a->icon().isNull(), c.name);
        }
        for (const keymap::Action& k : keymap_->actions()) { // mọi hành động KDEF trừ "—" có QAction giữ phím
            if (k.command == u"—"_s)
                continue;
            const auto& all = reg_->actions();
            QVERIFY2(std::ranges::any_of(all, [&](QAction* a) { return ui::RichTooltip::info(a).keyId == k.id; }),
                     qPrintable(k.id));
        }
        QCOMPARE(reg_->macro(u"NOSUCHCOMMAND"_s), QString());
        QCOMPARE(reg_->action(u"NOSUCHCOMMAND"_s), nullptr);
    }

    void surfacesAndKeys()
    {
        // Menu (UI0-107): bấm mục
        ui::Menu menu;
        const QStringList items{u"U"_s, u"COPYCLIP"_s, u"ZOOM"_s};
        for (const QString& id : items)
            menu.addAction(reg_->action(id));
        for (const QString& id : items) {
            menu.popup(window_->mapToGlobal(QPoint(10, 10)));
            QVERIFY(QTest::qWaitForWindowExposed(&menu));
            QTest::mouseClick(&menu, Qt::LeftButton, {}, menu.actionGeometry(reg_->action(id)).center());
            QCOMPARE(take(), QStringList{reg_->macro(id)});
            QTRY_VERIFY(!menu.isVisible());
        }
        tips_->leave(); // tooltip của mục menu đang rê chặn F1 (UI0-107)

        // Toolbar cổ điển (UIA-105)
        QToolBar bar(window_.get());
        bar.addAction(reg_->action(u"LINE"_s));
        bar.show();
        QTest::mouseClick(bar.widgetForAction(reg_->action(u"LINE"_s)), Qt::LeftButton);
        QCOMPARE(take(), QStringList{u"^C^C_LINE"_s});

        // Tooltip: tên lệnh tiếng Anh và phím theo Keymap (RichTooltip, Menu hiện nhãn phím)
        QAction* save = reg_->action(u"QSAVE"_s);
        QCOMPARE(ui::RichTooltip::info(save).keyId, u"save"_s);
        QCOMPARE(ui::Menu::keyText(save), u"Ctrl+S"_s);

        // Mọi phím của Keymap, focus ở vùng vẽ (cả hai phạm vi chạy): đúng chuỗi của hành động giữ phím
        QHash<QString, QString> byKey; // keyId → id
        for (QAction* a : reg_->actions())
            byKey.insert(ui::RichTooltip::info(a).keyId, a->objectName());
        int keys = 0;
        for (const keymap::Action& k : keymap_->actions()) {
            if (k.command == u"—"_s)
                continue;
            for (const QKeyCombination c : k.bindings) {
                press(viewport_, c);
                QVERIFY2(take() == QStringList{reg_->macro(byKey.value(k.id))},
                         qPrintable(QKeySequence(c).toString() + u" → "_s + k.id));
                ++keys;
            }
        }
        QVERIFY(keys >= 40);
        qInfo("%d tổ hợp phím của Keymap cho đúng chuỗi lệnh", keys);
    }

    void sysvarTwoWay()
    {
        QAction* ortho = reg_->action(u"ortho"_s);
        QToolButton button(window_.get()); // nút thanh trạng thái (UIA-112)
        button.setDefaultAction(ortho);
        button.show();
        sysvars_->set(u"ORTHOMODE"_s, 0);
        QVERIFY(!ortho->isChecked());

        // Đổi từ nơi khác (SETVAR, LISP, .scr): QAction và nút đổi ngay trong lời gọi, không chờ vòng sự kiện
        sysvars_->set(u"ORTHOMODE"_s, 1);
        QVERIFY(ortho->isChecked());
        QVERIFY(button.isChecked());

        // Bấm: chỉ gửi lệnh trong suốt; trạng thái giữ theo sysvar (lõi chưa đổi), không tự đảo
        QTest::mouseClick(&button, Qt::LeftButton);
        QCOMPARE(take(), QStringList{u"'ORTHOMODE 0"_s});
        QVERIFY(ortho->isChecked());
        QVERIFY(button.isChecked());
        press(viewport_, QKeyCombination(Qt::Key_F8)); // phím F8 cũng vậy
        QCOMPARE(take(), QStringList{u"'ORTHOMODE 0"_s});
        QVERIFY(ortho->isChecked());
        sysvars_->set(u"ORTHOMODE"_s, 0); // lõi áp lệnh → nút tắt
        QVERIFY(!button.isChecked());

        // Một sysvar, hai toggle (AUTOSNAP: POLAR bit 8, OTRACK bit 16)
        QAction* polar = reg_->action(u"polar"_s);
        QAction* otrack = reg_->action(u"otrack"_s);
        sysvars_->set(u"AUTOSNAP"_s, 63);
        QVERIFY(polar->isChecked() && otrack->isChecked());
        sysvars_->set(u"AUTOSNAP"_s, 63 ^ 8);
        QVERIFY(!polar->isChecked() && otrack->isChecked());
        QCOMPARE(reg_->macro(u"polar"_s), u"'AUTOSNAP 63"_s);

        // OSMODE: bit 16384 bật là tắt bắt điểm, giữ các chế độ
        QAction* osnap = reg_->action(u"osnap"_s);
        sysvars_->set(u"OSMODE"_s, 4133 | 16384);
        QVERIFY(!osnap->isChecked());
        QCOMPARE(reg_->macro(u"osnap"_s), u"'OSMODE 4133"_s);

        // DYNMODE: âm là tắt (giữ lựa chọn), 0 bật lại thì 3
        QAction* dyn = reg_->action(u"dyn"_s);
        sysvars_->set(u"DYNMODE"_s, -2);
        QVERIFY(!dyn->isChecked());
        QCOMPARE(reg_->macro(u"dyn"_s), u"'DYNMODE 2"_s);
        sysvars_->set(u"DYNMODE"_s, 0);
        QCOMPARE(reg_->macro(u"dyn"_s), u"'DYNMODE 3"_s);

        // Nguồn sysvar tạm: đặt lại cùng giá trị không phát tín hiệu
        QSignalSpy spy(sysvars_.get(), &Sysvars::changed);
        sysvars_->set(u"DYNMODE"_s, 0);
        sysvars_->set(u"NEVERSET"_s, 0);
        QCOMPARE(spy.count(), 0);
        sysvars_->set(u"DYNMODE"_s, 1);
        QCOMPARE(spy.count(), 1);

        // Không timer, không polling: registry không có timer nào
        QVERIFY(reg_->findChildren<QTimer*>().isEmpty());
    }

    // UIA-001: Ctrl+0 đổi vòng bật/tắt theo CLEANSCREENSTATE như macro CUI của AutoCAD; trạng thái do CleanScreen đặt.
    // Ctrl+9 như vậy theo CLISTATE (1: dòng lệnh đang hiện; cửa sổ chính đặt khi ẩn/hiện vùng).
    void cleanScreenCycle()
    {
        const QKeyCombination ctrl9(Qt::ControlModifier, Qt::Key_9);
        sysvars_->set(u"CLISTATE"_s, 1);
        press(viewport_, ctrl9);
        QCOMPARE(take(), QStringList{u"'_COMMANDLINEHIDE"_s});
        sysvars_->set(u"CLISTATE"_s, 0);
        press(viewport_, ctrl9);
        QCOMPARE(take(), QStringList{u"'_COMMANDLINE"_s});
        QCOMPARE(reg_->macro(u"COMMANDLINE"_s), u"'_COMMANDLINE"_s);

        const QKeyCombination ctrl0(Qt::ControlModifier, Qt::Key_0);
        sysvars_->set(u"CLEANSCREENSTATE"_s, 0);
        press(viewport_, ctrl0);
        QCOMPARE(take(), QStringList{u"'_CLEANSCREENON"_s});
        sysvars_->set(u"CLEANSCREENSTATE"_s, 1);
        press(viewport_, ctrl0);
        QCOMPARE(take(), QStringList{u"'_CLEANSCREENOFF"_s});
        reg_->action(u"CLEANSCREENON"_s)->trigger(); // nút thanh trạng thái, mục menu: cùng chuỗi
        QCOMPARE(take(), QStringList{u"'_CLEANSCREENOFF"_s});
        sysvars_->set(u"CLEANSCREENSTATE"_s, 0);
        QCOMPARE(reg_->macro(u"CLEANSCREENON"_s), u"'_CLEANSCREENON"_s);
    }

    void viewportScope()
    {
        const auto* km = keymap_.get();
        QList<QKeyCombination> viewportKeys;
        for (const keymap::Action& k : km->actions())
            if (k.scope == keymap::Scope::Viewport)
                viewportKeys << k.bindings;
        QVERIFY(viewportKeys.size() >= 9);

        // Ô dòng lệnh, ô văn bản của palette: phím Vùng vẽ không chạy; Ctrl+C chép chữ
        for (QLineEdit* box : {static_cast<QLineEdit*>(cmdline_), field_}) {
            box->setText(u"LINE 10,20"_s); // i18n-ok: chữ người dùng gõ
            box->selectAll();
            focus(box);
            QGuiApplication::clipboard()->clear();
            press(box, QKeyCombination(Qt::ControlModifier, Qt::Key_C));
            QCOMPARE(take(), QStringList());
            QCOMPARE(QGuiApplication::clipboard()->text(), u"LINE 10,20"_s);
            for (const QKeyCombination c : viewportKeys) {
                press(box, c);
                QVERIFY2(take().isEmpty(), qPrintable(QKeySequence(c).toString()));
            }
            // Phím Toàn cục vẫn chạy ở ô văn bản (ô không dùng F8, Ctrl+S)
            press(box, QKeyCombination(Qt::Key_F8));
            QCOMPARE(take(), QStringList{reg_->macro(u"ortho"_s)});
            press(box, QKeyCombination(Qt::ControlModifier, Qt::Key_S));
            QCOMPARE(take(), QStringList{u"^C^C_QSAVE"_s});
        }

        // Vùng vẽ: Ctrl+C là COPYCLIP; viewport thứ hai cũng nhận phím Vùng vẽ
        focus(viewport_);
        press(viewport_, QKeyCombination(Qt::ControlModifier, Qt::Key_C));
        QCOMPARE(take(), QStringList{u"^C^C_COPYCLIP"_s});
        auto* second = new QWidget(window_.get());
        second->setFocusPolicy(Qt::StrongFocus);
        second->show();
        reg_->addViewport(second);
        focus(second);
        press(second, QKeyCombination(Qt::Key_Delete));
        QCOMPARE(take(), QStringList{u"^C^C_ERASE"_s});
        delete second;
    }

    void latency()
    {
        // Từ phím hoặc cú bấm đến khi chuỗi lệnh tới chỗ nối INT-002
        Clock::time_point got;
        int count = 0;
        reg_->setSink([&](const QString&) {
            got = Clock::now();
            ++count;
        });
        QToolButton button(window_.get());
        button.setDefaultAction(reg_->action(u"LINE"_s));
        button.show();
        std::vector<double> key, click;
        for (int i = 0; i < 300; ++i) {
            auto t0 = Clock::now();
            press(viewport_, QKeyCombination(Qt::ControlModifier, Qt::Key_C));
            key.push_back(ms(got - t0));
            t0 = Clock::now();
            QTest::mouseClick(&button, Qt::LeftButton);
            click.push_back(ms(got - t0));
        }
        reg_->setSink([this](const QString& m) { sent_ << m; });
        QCOMPARE(count, 600);
        const double keyP99 = percentile(key, 0.99), clickP99 = percentile(click, 0.99);
        qInfo("phím → chuỗi lệnh: p50 %.3f ms, p99 %.3f ms; bấm → chuỗi lệnh: p50 %.3f ms, p99 %.3f ms",
              percentile(key, 0.5), keyP99, percentile(click, 0.5), clickP99);
        QVERIFY2(!test::kTimed || (keyP99 <= 16 && clickP99 <= 16), "DoD-L: ≤ 16 ms");
    }

    void keymapFollows()
    {
        QAction* line = reg_->action(u"LINE"_s);
        QVERIFY(line->shortcuts().isEmpty());
        QVERIFY(!keymap_->assign(u"LINE"_s, u"Ctrl+Shift+L"_s)); // người dùng gán phím (UI-K06)
        QCOMPARE(line->shortcuts(), QList<QKeySequence>{QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_L)});
        press(viewport_, QKeyCombination(Qt::ControlModifier | Qt::ShiftModifier, Qt::Key_L));
        QCOMPARE(take(), QStringList{u"^C^C_LINE"_s});
        keymap_->reset(u"LINE"_s);
        QVERIFY(line->shortcuts().isEmpty());

        QAction* osnap = reg_->action(u"osnap"_s); // phím mặc định đổi
        QVERIFY(!keymap_->assign(u"osnap"_s, u"Ctrl+Alt+O"_s));
        QVERIFY(osnap->shortcuts().contains(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_O)));
        keymap_->reset(u"osnap"_s);
        QCOMPARE(osnap->shortcuts(), QList<QKeySequence>{QKeySequence(Qt::Key_F3)});
    }

    void retranslate()
    {
        QAction* line = reg_->action(u"LINE"_s);
        QAction* osnap = reg_->action(u"osnap"_s);
        const QString lineVi = line->text(), osnapVi = osnap->text();
        QCOMPARE(lineVi, QCoreApplication::translate("actions", "Vẽ đoạn thẳng"));
        QVERIFY(translator_->setLanguage(i18n::kPseudoLanguage.toString())); // mọi chuỗi dài thêm 40%
        QVERIFY(line->text().startsWith(lineVi) && line->text().size() > lineVi.size());
        QVERIFY(osnap->text().startsWith(osnapVi) && osnap->text().size() > osnapVi.size());
        QVERIFY(translator_->setLanguage(u"vi"_s));
        QCOMPARE(line->text(), lineVi);
    }

    void gate()
    {
        reg_->setGate([](const QString& id) { return id != u"LINE"_s; }); // UI0-011: LINE bị khóa
        reg_->action(u"LINE"_s)->trigger();
        QCOMPARE(take(), QStringList());
        reg_->action(u"QSAVE"_s)->trigger();
        QCOMPARE(take(), QStringList{u"^C^C_QSAVE"_s});
        reg_->setGate({});
    }

    void metadata()
    {
        QCOMPARE(reg_->need(u"LINE"_s), ui::Edition::Pro);
        QCOMPARE(reg_->need(u"OPEN"_s), ui::Edition::None);
        QCOMPARE(reg_->need(u"COUNT"_s), ui::Edition::ViewerPro);
        QCOMPARE(reg_->need(u"osnap"_s), ui::Edition::None);
        QCOMPARE(reg_->need(u"ortho"_s), ui::Edition::Pro);
        QCOMPARE(reg_->need(u"NOSUCHCOMMAND"_s), ui::Edition::None);
        QCOMPARE(reg_->action(u"ortho"_s)->text(), QCoreApplication::translate("keymap", "Ortho"));
        QVERIFY(reg_->action(u"ortho"_s)->isCheckable());
        QVERIFY(!reg_->action(u"isop"_s)->isCheckable()); // đổi vòng, không phải toggle
        QCOMPARE(ui::RichTooltip::info(reg_->action(u"ortho"_s)).command, u"ORTHOMODE"_s);
        QCOMPARE(reg_->action(u"COPYCLIP"_s)->shortcutContext(), Qt::WidgetShortcut);
        QCOMPARE(reg_->action(u"QSAVE"_s)->shortcutContext(), Qt::ApplicationShortcut);
    }
};

QTEST_MAIN(ActionsTest)
#include "actions_test.moc"
