// UIA-101: QAT đầy đủ (Qat.h). Cửa sổ mẫu như cửa sổ chính: thanh tiêu đề (có QAT), hàng dưới ribbon (nhận QAT khi
// "Hiện bên dưới ribbon"), ô nhập giả làm vùng vẽ. ActionRegistry với sink ghi chuỗi lệnh; EditionGate với trang nâng
// cấp giả; nguồn hoàn tác/làm lại giả: stack 10.000 bước, mỗi kịch bản dùng n bước đầu (mới nhất trước).
//   1  undoStepK: 100 kịch bản ngẫu nhiên (seed cố định; stack 0…10.000 bước, 1/4 là Làm lại): mở danh sách bằng
//      Alt+↓, chọn bước thứ k bằng ↓ rồi Enter → đúng một chuỗi lệnh "^C^C_UNDO k" ("^C^C_MREDO k"), nguồn chỉ bị hỏi 12
//      bước. Hoãn (cần DOC-008): so hash bản vẽ với k lần Ctrl+Z.
//   2  undoListOpenTime: stack 10.000 bước, Alt+↓ tới lần vẽ đầu của danh sách ≤ 100 ms CPU luồng (kTimed; mục tiêu 1
//      khung, số đo luôn in).
//   3  persistence: thêm (menu tùy biến, "Thêm vào QAT"), bớt, dưới ribbon giữ sau khi dựng lại với cùng Store và với
//      Store mới đọc cùng tệp; về trên ribbon; "Đặt lại mặc định" đúng 7 mục; Viewer danh sách riêng (Mở, In).
//   DoD-U: addFromRibbon (nút, mục bật/tắt, trùng, lệnh khóa mở trang nâng cấp, bớt), menuBar (QAction MENUBAR gắn
//   sysvar của ActionRegistry), keyboard
//   (Tab qua nút, Enter chạy lệnh, menu tùy biến bằng phím, Esc trả focus), pseudoNoTruncation (vi-XL, gọn và thoáng,
//   cả menu tùy biến và danh sách hoàn tác).
#include "app/shell/titlebar/CommandSearch.h"
#include "app/shell/titlebar/Qat.h"
#include "app/shell/titlebar/TitleBar.h"

#include "app/core/actions/ActionRegistry.h"
#include "app/core/edition/EditionGate.h"
#include "app/core/i18n/Translator.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/core/keymap/Keymap.h"
#include "app/core/UiExecutor.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/controls/Controls.h"
#include "app/widgets/menu/Menu.h"
#include "app/widgets/tooltip/RichTooltip.h"
#include "core/base/sched/Scheduler.h"
#include "core/base/settings/Settings.h"
#include "tests/timing.h"

#include <QAction>
#include <QApplication>
#include <QBoxLayout>
#include <QLineEdit>
#include <QPointer>
#include <QScreen>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>
#include <chrono>
#include <memory>
#include <random>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;
using namespace trion;
using namespace trion::shell;
using edition::Edition;
using trion::test::kTimed;

namespace {

constexpr double kOpenBudgetMs = 100; // DoD-L: đổi trạng thái giao diện (mở menu) ≤ 100 ms, mục tiêu 1 khung
constexpr int kStack = 10000;         // tiêu chí 2: undo stack 10.000 bước
const QStringList kDefaults{u"NEW"_s, u"OPEN"_s, u"QSAVE"_s, u"SAVEAS"_s, u"U"_s, u"REDO"_s, u"PLOT"_s};

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

// Phím tới popup đang mở (Menu) nếu có, không thì tới widget đang focus.
void key(Qt::Key k, Qt::KeyboardModifiers m = Qt::NoModifier)
{
    QWidget* to = QApplication::activePopupWidget() ? QApplication::activePopupWidget() : QApplication::focusWidget();
    QVERIFY(to);
    QTest::keyClick(to, k, m);
}

// Đếm lần vẽ của một widget.
class Watcher final : public QObject {
public:
    explicit Watcher(QWidget* w) { w->installEventFilter(this); }
    int paints = 0;

protected:
    bool eventFilter(QObject*, QEvent* e) override
    {
        paints += e->type() == QEvent::Paint;
        return false;
    }
};

// Mục chọn được của menu (bỏ vạch chia, tiêu đề nhóm).
QList<QAction*> rows(const QMenu* m)
{
    QList<QAction*> out;
    for (QAction* a : m->actions())
        if (!a->isSeparator())
            out << a;
    return out;
}

double ms(std::chrono::nanoseconds d)
{
    return std::chrono::duration<double, std::milli>(d).count();
}

// CPU của luồng (Linux, không phụ thuộc tải máy dùng chung); nơi chưa có: thời gian thực, chỉ để báo.
std::chrono::nanoseconds now()
{
    const std::chrono::nanoseconds cpu = trion::test::threadCpu();
    return cpu.count() >= 0 ? cpu : std::chrono::steady_clock::now().time_since_epoch();
}

QString truncations(QWidget* root)
{
    QStringList out;
    for (const i18n::Truncation& t : i18n::findTruncations(root))
        out << t.toString();
    return out.join(u"; "_s);
}

} // namespace

class QatTest : public QObject {
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
    std::unique_ptr<QWidget> host_;
    QVBoxLayout* column_ = nullptr;
    TitleBar* bar_ = nullptr;
    QWidget* strip_ = nullptr;    // hàng dưới ribbon của cửa sổ chính
    QLineEdit* canvas_ = nullptr; // vùng vẽ giả
    QStringList sent_;            // chuỗi lệnh của ActionRegistry tới INT-002
    QStringList macros_;          // Qat::macroRequested
    QStringList upgrades_;        // tính năng đã yêu cầu nâng cấp
    std::vector<UndoStep> steps_; // stack giả, mới nhất trước
    int undoSize_ = 0, redoSize_ = 0;
    QList<int> limits_; // số bước nguồn bị hỏi

    Qat* qat() const { return bar_->qat(); }
    ui::SplitButton* split(const QString& command) const
    {
        return qobject_cast<ui::SplitButton*>(qat()->button(command));
    }
    QAction* toggle(const QString& text) const
    {
        for (QAction* a : qat()->menu()->actions())
            if (a->text() == text)
                return a;
        return nullptr;
    }

    // Lệnh của các nút đang hiện, theo thứ tự trên thanh.
    QStringList shown() const
    {
        QStringList out;
        QLayout* row = qat()->layout();
        for (int i = 0; i < row->count(); ++i)
            if (QWidget* w = row->itemAt(i)->widget(); w && !w->isHidden())
                if (const QString c = ui::RichTooltip::info(w).command; !c.isEmpty())
                    out << c;
        return out;
    }
    int separators() const
    {
        int n = 0;
        QLayout* row = qat()->layout();
        for (int i = 0; i < row->count(); ++i)
            if (QWidget* w = row->itemAt(i)->widget(); w && !w->isHidden() && !qobject_cast<QAbstractButton*>(w))
                ++n;
        return n;
    }

    std::vector<UndoStep> top(int size, int limit)
    {
        limits_ << limit;
        return std::vector<UndoStep>(steps_.begin(), steps_.begin() + std::min(size, limit));
    }

    // Dựng thanh tiêu đề như cửa sổ chính (integration): nguồn bước, chuỗi lệnh, vị trí trên/dưới ribbon.
    void build()
    {
        bar_ = new TitleBar(*reg_, *gate_);
        column_->insertWidget(0, bar_);
        Qat* q = bar_->qat();
        q->setUndoSource([this](int n) { return top(undoSize_, n); });
        q->setRedoSource([this](int n) { return top(redoSize_, n); });
        connect(q, &Qat::macroRequested, this, [this](const QString& m) { macros_ << m; });
        const auto place = [this, q](bool below) {
            if (below)
                strip_->layout()->addWidget(q);
            strip_->setVisible(below);
        };
        connect(q, &Qat::belowRibbonChanged, strip_, place);
        place(q->isBelowRibbon());
        QCoreApplication::processEvents(); // QLayout hiện widget vừa chèn bằng lời gọi đăng hàng đợi
    }
    void destroy()
    {
        const QPointer<Qat> q = qat(); // dưới ribbon: không còn là con của thanh
        delete bar_;
        delete q.data();
        bar_ = nullptr;
    }
    void rebuild()
    {
        destroy();
        build();
    }

    void focus(QWidget* w)
    {
        host_->activateWindow();
        w->setFocus(Qt::TabFocusReason);
        QVERIFY(spinUntil([&] { return QApplication::focusWidget() == w; }));
    }

    void closePopups()
    {
        for (int i = 0; i < 5 && QApplication::activePopupWidget(); ++i)
            QApplication::activePopupWidget()->close();
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
        reg_ = std::make_unique<actions::ActionRegistry>(*keymap_, *sysvars_);
        reg_->setSink([this](const QString& m) { sent_ << m; });
        gate_ = std::make_unique<edition::EditionGate>();
        gate_->bind(*reg_);
        gate_->upgrade().setHandler([this](const edition::UpgradeRequest& r) { upgrades_ << r.feature; });
        connect(&gate_->upgrade(), &edition::UpgradeRouter::noticeRequested, this,
                [this](const edition::UpgradeRequest& r) { upgrades_ << r.feature; });
        gate_->setEdition(Edition::Pro);

        steps_.reserve(kStack);
        for (int i = 1; i <= kStack; ++i) // mô tả rỗng ở một phần ba số bước: danh sách hiện tên lệnh
            steps_.push_back({u"CMD%1"_s.arg(i), i % 3 ? u"Bước %1"_s.arg(i) : QString()}); // i18n-ok: dữ liệu mẫu

        host_ = std::make_unique<QWidget>();
        column_ = new QVBoxLayout(host_.get());
        column_->setContentsMargins({});
        column_->setSpacing(0);
        strip_ = new QWidget;
        new QHBoxLayout(strip_);
        canvas_ = new QLineEdit;
        column_->addWidget(strip_);
        column_->addWidget(canvas_);
        column_->addStretch(1);
        build();
        host_->resize(1366, 600);
        host_->show();
        QVERIFY(QTest::qWaitForWindowExposed(host_.get()));
    }

    void init()
    {
        tm_->setDensity(theme::Density::Compact);
        sent_.clear();
        macros_.clear();
        upgrades_.clear();
        limits_.clear();
        focus(canvas_);
    }

    void cleanup()
    {
        closePopups();
        if (translator_->language() != u"vi"_s) {
            QVERIFY(translator_->setLanguage(u"vi"_s));
            QVERIFY(spinUntil([&] { return toggle(u"Đặt lại mặc định"_s); }));
        }
        gate_->setEdition(Edition::Pro);
        if (qat()->isBelowRibbon())
            toggle(u"Hiện bên dưới ribbon"_s)->trigger();
        store_->reset(kViewerItems);
        qat()->reset();
        QCoreApplication::processEvents();
    }

    void cleanupTestCase()
    {
        host_.reset();
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

    // Tiêu chí 1.
    void undoStepK()
    {
        std::mt19937 rng(101); // cố định: kịch bản như nhau mọi lần chạy
        int empty = 0, picked = 0;
        for (int run = 0; run < 100; ++run) {
            const bool redo = run % 4 == 3;
            const int n = run < 6 ? run * 3 : int(rng() % (kStack + 1)); // gồm stack rỗng, ít hơn và nhiều hơn 12
            (redo ? redoSize_ : undoSize_) = n;
            macros_.clear();
            limits_.clear();
            ui::SplitButton* b = split(redo ? u"REDO"_s : u"U"_s);
            QVERIFY(b);
            focus(b);
            key(Qt::Key_Down, Qt::AltModifier);
            QVERIFY(spinUntil([&] { return QApplication::activePopupWidget() == b->dropMenu(); }));
            QCOMPARE(limits_, QList<int>{Qat::kListSteps});
            const QList<QAction*> list = rows(b->dropMenu());
            const int count = std::min(n, Qat::kListSteps);
            if (count == 0) { // rỗng: một dòng mờ, Enter không phát gì
                ++empty;
                QCOMPARE(list.size(), 1);
                QVERIFY(!list[0]->isEnabled());
                QCOMPARE(list[0]->text(), redo ? u"Không có bước để làm lại"_s : u"Không có bước để hoàn tác"_s);
                key(Qt::Key_Escape);
            } else {
                ++picked;
                QCOMPARE(b->dropMenu()->actions().constFirst()->text(), redo ? u"Làm lại tới"_s : u"Hoàn tác tới"_s);
                QCOMPARE(list.size(), count);
                const int k = 1 + int(rng() % unsigned(count));
                const UndoStep& s = steps_[size_t(k - 1)];
                const QString name = s.description.isEmpty() ? s.command : s.description;
                QCOMPARE(list[k - 1]->text(), k == 1 ? name : u"%1 (%2 bước)"_s.arg(name).arg(k));
                for (int i = 0; i < k; ++i)
                    key(Qt::Key_Down);
                QCOMPARE(b->dropMenu()->activeAction(), list[k - 1]);
                key(Qt::Key_Return);
                QCOMPARE(macros_, QStringList{(redo ? u"^C^C_MREDO %1"_s : u"^C^C_UNDO %1"_s).arg(k)});
            }
            QVERIFY(spinUntil([&] { return !b->dropMenu()->isVisible(); }));
            QCOMPARE(QApplication::focusWidget(), b);
            QVERIFY(sent_.isEmpty()); // mũi tên chỉ mở danh sách, không chạy U/REDO
        }
        qInfo("Danh sách hoàn tác: 100 kịch bản, %d chọn bước (macro đúng k), %d stack rỗng", picked, empty);
        QVERIFY(picked >= 90 && empty >= 1);
    }

    // Tiêu chí 2.
    void undoListOpenTime()
    {
        undoSize_ = kStack;
        ui::SplitButton* b = split(u"U"_s);
        focus(b);
        std::vector<double> cpu;
        for (int i = 0; i < 20; ++i) {
            Watcher w(b->dropMenu());
            const auto t0 = now();
            key(Qt::Key_Down, Qt::AltModifier);
            QVERIFY(spinUntil([&] { return w.paints > 0; }));
            cpu.push_back(ms(now() - t0));
            QCOMPARE(rows(b->dropMenu()).size(), Qat::kListSteps);
            key(Qt::Key_Escape);
            QVERIFY(spinUntil([&] { return !b->dropMenu()->isVisible(); }));
        }
        QVERIFY(std::all_of(limits_.begin(), limits_.end(), [](int n) { return n == Qat::kListSteps; }));
        std::sort(cpu.begin(), cpu.end());
        const QScreen* s = QGuiApplication::primaryScreen();
        const double frame = 1000 / (s && s->refreshRate() > 1 ? s->refreshRate() : 60);
        qInfo("Mở danh sách hoàn tác (stack %d bước, 20 lần, CPU luồng tới lần vẽ đầu): trung vị %.3f ms, lớn nhất "
              "%.3f ms (ngưỡng %.0f ms, 1 khung %.1f ms)",
              kStack, cpu[cpu.size() / 2], cpu.back(), kOpenBudgetMs, frame);
        if (test::threadCpu().count() >= 0)
            QVERIFY2(!kTimed || cpu.back() <= kOpenBudgetMs, qPrintable(QString::number(cpu.back())));
    }

    // Tiêu chí 3.
    void persistence()
    {
        QCOMPARE(shown(), kDefaults);
        QCOMPARE(separators(), 2);
        QVERIFY(!qat()->isBelowRibbon());
        QCOMPARE(qat()->parentWidget(), bar_);

        toggle(u"Xem trước bản in"_s)->trigger(); // thêm từ menu tùy biến
        qat()->add(u"LINE"_s);                    // "Thêm vào QAT" từ ribbon
        toggle(u"Lưu thành"_s)->trigger();        // bớt
        toggle(u"Hiện bên dưới ribbon"_s)->trigger();
        const QStringList want{u"NEW"_s, u"OPEN"_s, u"QSAVE"_s, u"U"_s, u"REDO"_s, u"PLOT"_s, u"PREVIEW"_s, u"LINE"_s};
        QCOMPARE(shown(), want);
        QCOMPARE(qat()->parentWidget(), strip_);

        rebuild(); // dựng lại với cùng Store
        QCOMPARE(shown(), want);
        QVERIFY(qat()->isBelowRibbon());
        QVERIFY(toggle(u"Hiện bên dưới ribbon"_s)->isChecked());
        QCOMPARE(qat()->parentWidget(), strip_);
        QVERIFY(toggle(u"Xem trước bản in"_s)->isChecked() && !toggle(u"Lưu thành"_s)->isChecked());

        store_->flush(); // khởi động lại: Store mới đọc cùng tệp
        {
            settings::Store again(store_->filePath());
            settings::Store::setInstance(&again);
            rebuild();
            QCOMPARE(shown(), want);
            QVERIFY(qat()->isBelowRibbon());
            QCOMPARE(qat()->parentWidget(), strip_);
            destroy();
            settings::Store::setInstance(store_.get());
        }
        build();

        // Về trên ribbon: thanh tiêu đề lấy lại QAT ngay sau nút menu ứng dụng; giữ sau khi dựng lại.
        toggle(u"Hiện bên dưới ribbon"_s)->trigger();
        QCOMPARE(qat()->parentWidget(), bar_);
        QVERIFY(strip_->isHidden());
        auto* app = bar_->findChild<QWidget*>(u"appMenuButton"_s);
        QCOMPARE(bar_->layout()->indexOf(qat()), bar_->layout()->indexOf(app) + 1);
        rebuild();
        QVERIFY(!qat()->isBelowRibbon());
        QCOMPARE(qat()->parentWidget(), bar_);
        QCOMPARE(shown(), want);

        // Đặt lại: đúng 7 mục mặc định, lệnh thêm từ ribbon bỏ, vạch chia quanh nhóm Hoàn tác.
        toggle(u"Đặt lại mặc định"_s)->trigger();
        QCOMPARE(shown(), kDefaults);
        QCOMPARE(separators(), 2);
        QVERIFY(!qat()->button(u"LINE"_s));
        rebuild();
        QCOMPARE(shown(), kDefaults);

        // Viewer: danh sách riêng (Mở, In; không vạch chia), tùy biến không lẫn với bản đầy đủ; đặt lại về Mở, In.
        gate_->setEdition(Edition::Viewer);
        QCOMPARE(shown(), (QStringList{u"OPEN"_s, u"PLOT"_s}));
        QCOMPARE(separators(), 0);
        toggle(u"Bác sĩ Font"_s)->trigger();
        QCOMPARE(shown(), (QStringList{u"OPEN"_s, u"PLOT"_s, u"FIXFONT"_s}));
        gate_->setEdition(Edition::Pro);
        QCOMPARE(shown(), kDefaults);
        gate_->setEdition(Edition::ViewerPro); // cùng bộ Viewer
        QCOMPARE(shown(), (QStringList{u"OPEN"_s, u"PLOT"_s, u"FIXFONT"_s}));
        toggle(u"Đặt lại mặc định"_s)->trigger();
        QCOMPARE(shown(), (QStringList{u"OPEN"_s, u"PLOT"_s}));

        // Bớt hết nhóm Hoàn tác: không còn vạch chia.
        gate_->setEdition(Edition::Pro);
        toggle(u"Hoàn tác"_s)->trigger();
        toggle(u"Làm lại"_s)->trigger();
        QCOMPARE(separators(), 0);
    }

    // "Thêm vào thanh truy cập nhanh" (RibbonBar::addToQatRequested): nút như mục có sẵn; bớt bằng menu tùy biến.
    void addFromRibbon()
    {
        QAction* line = reg_->action(u"LINE"_s);
        qat()->add(u"LINE"_s);
        qat()->add(u"LINE"_s); // lần hai: không thêm nút nữa
        qat()->add(u"NOSUCHCOMMAND"_s);
        QCOMPARE(shown(), kDefaults + QStringList{u"LINE"_s});
        ui::Button* b = qat()->button(u"LINE"_s);
        QVERIFY(b && b->isVisible() && !b->icon().isNull());
        QCOMPARE(b->accessibleName(), line->text());
        QCOMPARE(ui::RichTooltip::info(b).command, u"LINE"_s);
        QVERIFY(toggle(line->text()) && toggle(line->text())->isChecked());
        b->click();
        QCOMPARE(sent_, QStringList{u"^C^C_LINE"_s});

        // Lệnh khóa (Viewer thêm lệnh Pro): mở trang nâng cấp, không thêm.
        gate_->setEdition(Edition::Viewer);
        qat()->add(u"CIRCLE"_s);
        QCOMPARE(upgrades_, QStringList{u"CIRCLE"_s});
        QVERIFY(!qat()->button(u"CIRCLE"_s));
        toggle(u"Khớp thuộc tính"_s)->trigger(); // mục có sẵn khóa: cũng vậy, dấu chọn trả lại
        QCOMPARE(upgrades_.size(), 2);
        QVERIFY(!toggle(u"Khớp thuộc tính"_s)->isChecked());
        QCOMPARE(sent_.size(), 1);
        gate_->setEdition(Edition::Pro);

        // Bớt: bỏ dấu chọn của lệnh trong menu tùy biến; nút và mục bị xóa.
        const QPointer<ui::Button> gone = b;
        toggle(line->text())->trigger();
        QVERIFY(!qat()->button(u"LINE"_s) && !toggle(line->text()));
        QCOMPARE(shown(), kDefaults);
        QVERIFY(spinUntil([&] { return gone.isNull(); }));
    }

    // "Hiện thanh menu": QAction MENUBAR của ActionRegistry (gắn sysvar): bấm gửi 'MENUBAR qua sink như mọi lệnh, dấu
    // chọn chỉ theo sysvar MENUBAR (lõi đặt), không qua Qat::macroRequested.
    void menuBar()
    {
        QAction* t = toggle(u"Hiện thanh menu"_s);
        QVERIFY(t);
        QCOMPARE(t, reg_->action(u"MENUBAR"_s));
        QVERIFY(t->isCheckable() && !t->isChecked());
        t->trigger();
        QCOMPARE(sent_, QStringList{u"'MENUBAR 1"_s});
        QVERIFY(!t->isChecked()); // lõi chưa đổi sysvar: không tự đảo
        sysvars_->set(u"MENUBAR"_s, 1);
        QVERIFY(t->isChecked());
        t->trigger();
        QCOMPARE(sent_, (QStringList{u"'MENUBAR 1"_s, u"'MENUBAR 0"_s}));
        sysvars_->set(u"MENUBAR"_s, 0);
        QVERIFY(!t->isChecked());
        QVERIFY(macros_.isEmpty());
    }

    void keyboard()
    {
        // Tab từ nút menu ứng dụng qua các nút theo thứ tự trên thanh rồi tới ô Tìm lệnh.
        QList<QWidget*> want;
        for (const QString& c : kDefaults)
            want << qat()->button(c);
        want << qat()->findChild<ui::Button*>(u"qatMore"_s) << bar_->search();
        focus(bar_->findChild<QWidget*>(u"appMenuButton"_s));
        QList<QWidget*> got;
        for (int i = 0; i < want.size(); ++i) {
            key(Qt::Key_Tab);
            got << QApplication::focusWidget();
        }
        QCOMPARE(got, want);

        // Enter chạy lệnh của nút (nút tách: phần chính).
        for (const QString& c : kDefaults) {
            focus(qat()->button(c));
            key(Qt::Key_Return);
        }
        QCOMPARE(sent_, (QStringList{u"^C^C_NEW"_s, u"^C^C_OPEN"_s, u"^C^C_QSAVE"_s, u"^C^C_SAVEAS"_s, u"^C^C_U"_s,
                                     u"^C^C_REDO"_s, u"^C^C_PLOT"_s}));

        // Menu tùy biến: Enter mở, ↓ tới "Mới", Enter bỏ chọn (menu đóng, focus về nút ▾); lần nữa chọn lại; Esc đóng.
        auto* more = qat()->findChild<ui::Button*>(u"qatMore"_s);
        for (const bool on : {false, true}) {
            focus(more);
            key(Qt::Key_Return);
            QVERIFY(spinUntil([&] { return QApplication::activePopupWidget() == qat()->menu(); }));
            key(Qt::Key_Down);
            QCOMPARE(qat()->menu()->activeAction(), toggle(u"Mới"_s));
            key(Qt::Key_Return);
            QVERIFY(spinUntil([&] { return !qat()->menu()->isVisible(); }));
            QCOMPARE(QApplication::focusWidget(), more);
            QCOMPARE(!qat()->button(u"NEW"_s)->isHidden(), on);
        }
        key(Qt::Key_Return);
        QVERIFY(spinUntil([&] { return QApplication::activePopupWidget() == qat()->menu(); }));
        key(Qt::Key_Escape);
        QVERIFY(spinUntil([&] { return !qat()->menu()->isVisible(); }));
        QCOMPARE(QApplication::focusWidget(), more);
        QCOMPARE(shown(), kDefaults);
    }

    // DoD-U: chữ dài hơn 40% (vi-XL) không bị cắt ở QAT, menu tùy biến (cả lệnh thêm), danh sách hoàn tác.
    void pseudoNoTruncation()
    {
        QVERIFY(translator_->setLanguage(i18n::kPseudoLanguage.toString()));
        QVERIFY(spinUntil([&] { return qat()->menu()->actions().constLast()->text() != u"Đặt lại mặc định"_s; }));
        qat()->add(u"LINE"_s); // mục của lệnh thêm: chữ của QAction
        undoSize_ = Qat::kListSteps;
        for (const theme::Density d : {theme::Density::Comfortable, theme::Density::Compact}) {
            tm_->setDensity(d);
            ui::SplitButton* b = split(u"U"_s);
            focus(b);
            key(Qt::Key_Down, Qt::AltModifier); // danh sách chỉ có mục khi đã mở
            QVERIFY(spinUntil([&] { return b->dropMenu()->isVisible(); }));
            QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
            QCOMPARE(truncations(qat()), QString());
            QCOMPARE(truncations(b->dropMenu()), QString());
            key(Qt::Key_Escape);
        }
    }
};

QTEST_MAIN(QatTest)
#include "qat_test.moc"
