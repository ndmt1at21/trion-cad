// UIA-007 · Dòng lệnh: F2 (HistoryWindow.h), dạng nổi (CommandLine::setFloating), thiết lập (InputSearchOptions.h), cổng
// phiên bản (CommandLine::notifyLocked). Theo tiêu chí nghiệm thu:
//   1 history100k: F2 mở 100.000 dòng: CPU luồng UI từ dựng tới hết khung đầu ≤ 150 ms (release); tìm: find trên 100.000
//     khóa ≤ 50 ms CPU, lượt nền về đúng các dòng (không dấu, không hoa/thường), không dòng nào khớp: EmptyState; dòng
//     mới vào ngay (cả khi đang lọc). historyCopy: sao chép giữ nguyên văn (mọi dòng, dòng chọn, Ctrl+C), nút ^ và F2.
//   2 floating: chuột ngoài hàng ô nhập đi xuyên xuống vùng vẽ (childAt và nhấn thật); kéo dải nắm di chuyển; thả sát
//     mép dưới → dockRequested; đang dock kéo ra → floatRequested; độ trong suốt đổi là nền vẽ lại theo (điểm ảnh). 60 fps
//     khi pan dưới dòng lệnh nổi: Hoãn (cần vùng vẽ thật, RND).
//   3 editionNotice: Viewer: mọi lệnh 03 Phụ lục A có cờ Pro/Viewer Pro trong danh mục cho đúng câu 07.1 §2, không
//     submitted; chip phát upgradeRequested. "Bản vẽ không đổi" tới chỗ nối: Hoãn (cần INT-002, DOC).
//   4 settings: Lưu ghi Store, dòng lệnh áp ngay (số dòng, cỡ chữ, độ trễ, tìm biến, tìm tiếng Việt, bộ gõ); Hủy và Esc
//     không đổi; xóa AutoCorrect (Delete) chỉ áp khi Lưu.
//   DoD-U: pseudoNoTruncation (vi-XL), phím (Tab, Enter, Esc, Delete, Ctrl+C) trong từng ca.
#include "app/core/actions/ActionRegistry.h"
#include "app/core/edition/EditionGate.h"
#include "app/core/i18n/Translator.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/core/UiExecutor.h"
#include "app/shell/cmdline/Autocomplete.h"
#include "app/shell/cmdline/CommandLine.h"
#include "app/shell/cmdline/HistoryWindow.h"
#include "app/shell/cmdline/InputSearchOptions.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/cmdline/CommandInput.h"
#include "app/widgets/message/Messages.h"
#include "app/widgets/numeric/NumericField.h"
#include "app/widgets/views/VirtualTableView.h"
#include "core/base/sched/Scheduler.h"
#include "core/base/text/Fold.h"
#include "tests/timing.h"

#include <QAbstractButton>
#include <QAbstractScrollArea>
#include <QApplication>
#include <QBoxLayout>
#include <QCheckBox>
#include <QClipboard>
#include <QJsonObject>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <functional>
#include <map>
#include <memory>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;
using namespace trion;
using ui::CommandLine;

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

double cpuMs(std::chrono::nanoseconds d)
{
    return d.count() < 0 ? qQNaN() : std::chrono::duration<double, std::milli>(d).count();
}
bool withinCpu(double ms, double budget)
{
    return !test::kTimed || qIsNaN(ms) || ms <= budget;
}

QString truncations(QWidget* root)
{
    QStringList out;
    for (const i18n::Truncation& t : i18n::findTruncations(root))
        out << t.toString();
    return out.join(u"; "_s);
}

void send(QWidget* w, QEvent::Type type, QPoint global, Qt::MouseButton button, Qt::MouseButtons buttons)
{
    QMouseEvent e(type, QPointF(w->mapFromGlobal(global)), QPointF(global), button, buttons, Qt::NoModifier);
    QCoreApplication::sendEvent(w, &e);
}
// Kéo w (dải nắm) từ điểm local from đi một đoạn d (tọa độ màn hình).
void drag(QWidget* w, QPoint from, QPoint d)
{
    const QPoint g = w->mapToGlobal(from);
    send(w, QEvent::MouseButtonPress, g, Qt::LeftButton, Qt::LeftButton);
    send(w, QEvent::MouseMove, g + d / 2, Qt::NoButton, Qt::LeftButton);
    send(w, QEvent::MouseMove, g + d, Qt::NoButton, Qt::LeftButton);
    send(w, QEvent::MouseButtonRelease, g + d, Qt::LeftButton, Qt::NoButton);
}

// Vùng vẽ giả: nền đỏ, đếm lần nhấn chuột.
struct Canvas : QWidget {
    int presses = 0;
    void paintEvent(QPaintEvent*) override { QPainter(this).fillRect(rect(), Qt::red); }
    void mousePressEvent(QMouseEvent*) override { ++presses; }
};

// 03 Phụ lục A (docs/03-chuc-nang-autocad.md), lệnh có trong danh mục tạm của app/core/actions.
// clang-format off
const QStringList kAppendixA = {
    u"NEW"_s, u"OPEN"_s, u"SAVE"_s, u"SAVEAS"_s, u"QSAVE"_s, u"CLOSE"_s, u"RECOVER"_s, u"AUDIT"_s, u"PURGE"_s, u"UNITS"_s,
    u"OPTIONS"_s, u"ZOOM"_s, u"PAN"_s, u"REGEN"_s, u"REDRAW"_s, u"VIEWRES"_s, u"LINE"_s, u"XLINE"_s, u"PLINE"_s,
    u"RECTANG"_s, u"POLYGON"_s, u"CIRCLE"_s, u"ARC"_s, u"ELLIPSE"_s, u"SPLINE"_s, u"POINT"_s, u"DDPTYPE"_s, u"DIVIDE"_s,
    u"MEASURE"_s, u"BOUNDARY"_s, u"HATCH"_s, u"HATCHEDIT"_s, u"ERASE"_s, u"OOPS"_s, u"MOVE"_s, u"COPY"_s, u"ROTATE"_s,
    u"SCALE"_s, u"MIRROR"_s, u"STRETCH"_s, u"OFFSET"_s, u"TRIM"_s, u"EXTEND"_s, u"BREAK"_s, u"BREAKATPOINT"_s, u"JOIN"_s,
    u"FILLET"_s, u"CHAMFER"_s, u"EXPLODE"_s, u"LENGTHEN"_s, u"ARRAYRECT"_s, u"ARRAYPOLAR"_s, u"MATCHPROP"_s, u"PEDIT"_s,
    u"DRAWORDER"_s, u"UNDO"_s, u"U"_s, u"REDO"_s, u"COPYCLIP"_s, u"CUTCLIP"_s, u"PASTECLIP"_s, u"SELECT"_s, u"QSELECT"_s,
    u"SELECTSIMILAR"_s, u"TEXT"_s, u"MTEXT"_s, u"STYLE"_s, u"TEXTEDIT"_s, u"FIND"_s, u"DIMLINEAR"_s, u"DIMALIGNED"_s,
    u"DIMANGULAR"_s, u"DIMRADIUS"_s, u"DIMDIAMETER"_s, u"DIMCONTINUE"_s, u"DIMBASELINE"_s, u"DIMSTYLE"_s, u"DIMEDIT"_s,
    u"DIMTEDIT"_s, u"MLEADER"_s, u"LAYER"_s, u"LAYISO"_s, u"LAYUNISO"_s, u"LAYOFF"_s, u"LAYON"_s, u"LAYFRZ"_s, u"LAYTHW"_s,
    u"LAYMCUR"_s, u"LAYMCH"_s, u"COLOR"_s, u"LINETYPE"_s, u"LTSCALE"_s, u"LWEIGHT"_s, u"PROPERTIES"_s, u"BLOCK"_s,
    u"INSERT"_s, u"WBLOCK"_s, u"BASE"_s, u"RENAME"_s, u"ATTDEF"_s, u"EATTEDIT"_s, u"ATTSYNC"_s, u"ATTDISP"_s, u"XREF"_s,
    u"XATTACH"_s, u"XBIND"_s, u"XCLIP"_s, u"IMAGEATTACH"_s, u"LAYOUT"_s, u"MVIEW"_s, u"VPORTS"_s, u"PAGESETUP"_s, u"PLOT"_s,
    u"PREVIEW"_s, u"EXPORTPDF"_s, u"STYLESMANAGER"_s, u"DIST"_s, u"AREA"_s, u"LIST"_s, u"ID"_s, u"MEASUREGEOM"_s,
    u"SETVAR"_s, u"SCRIPT"_s, u"APPLOAD"_s, u"FIXFONT"_s,
};
// Nhóm Vẽ và Sửa của Phụ lục A: lệnh sửa bản vẽ chắc chắn, phải bị khóa ở Viewer.
const QStringList kDrawModify = {
    u"LINE"_s, u"XLINE"_s, u"PLINE"_s, u"RECTANG"_s, u"POLYGON"_s, u"CIRCLE"_s, u"ARC"_s, u"ELLIPSE"_s, u"SPLINE"_s,
    u"HATCH"_s, u"ERASE"_s, u"MOVE"_s, u"COPY"_s, u"ROTATE"_s, u"SCALE"_s, u"MIRROR"_s, u"STRETCH"_s, u"OFFSET"_s,
    u"TRIM"_s, u"EXTEND"_s, u"BREAK"_s, u"JOIN"_s, u"FILLET"_s, u"CHAMFER"_s, u"EXPLODE"_s, u"LENGTHEN"_s,
};
// clang-format on

} // namespace

class CmdlineExtrasTest : public QObject {
    Q_OBJECT

    QTemporaryDir dir_;
    std::unique_ptr<settings::Store> store_;
    std::unique_ptr<ui::UiExecutor> executor_;
    std::unique_ptr<theme::ThemeManager> tm_;
    std::unique_ptr<theme::IconProvider> icons_;
    std::unique_ptr<i18n::Translator> translator_;
    std::unique_ptr<QWidget> window_;
    QVBoxLayout* column_ = nullptr;
    Canvas* canvas_ = nullptr;
    CommandLine* cmd_ = nullptr;
    QStringList tokens_, macros_;

    // Như bước integration: nổi trên vùng vẽ, bám lại ô dock dưới.
    void floatAt(QPoint global)
    {
        column_->removeWidget(cmd_);
        cmd_->setParent(canvas_);
        cmd_->setFloating(true, global);
        cmd_->show();
    }
    void dock()
    {
        cmd_->setFloating(false);
        cmd_->setParent(window_.get());
        column_->addWidget(cmd_);
        cmd_->show();
    }
    QAbstractScrollArea* history() const
    {
        QWidget* host = cmd_->isFloating() ? cmd_->parentWidget() : cmd_;
        return host->findChild<QAbstractScrollArea*>(u"history"_s);
    }
    ui::HistoryWindow* openHistory()
    {
        auto* w = new ui::HistoryWindow(*cmd_, window_.get());
        w->setAttribute(Qt::WA_DeleteOnClose);
        w->show();
        return QTest::qWaitForWindowExposed(w) ? w : nullptr;
    }
    // Lượt nền dựng khóa tìm xong: một lượt tìm ra kết quả.
    static bool search(ui::HistoryWindow* w, const QString& q)
    {
        QSignalSpy done(w, &ui::HistoryWindow::searched);
        w->findChild<QLineEdit*>(u"search"_s)->setText(q);
        return spinUntil([&] { return done.count() > 0; });
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
        window_ = std::make_unique<QWidget>();
        column_ = new QVBoxLayout(window_.get());
        column_->setContentsMargins({});
        column_->setSpacing(0);
        canvas_ = new Canvas;
        cmd_ = new CommandLine;
        column_->addWidget(canvas_, 1);
        column_->addWidget(cmd_);
        window_->resize(1366, 600);
        window_->show();
        QVERIFY(QTest::qWaitForWindowExposed(window_.get()));
        connect(cmd_, &CommandLine::submitted, this, [this](const QString& t) { tokens_ << t; });
        connect(cmd_, &CommandLine::commandRequested, this, [this](const QString& m) { macros_ << m; });
        connect(cmd_, &CommandLine::floatRequested, this, &CmdlineExtrasTest::floatAt, Qt::QueuedConnection);
        connect(cmd_, &CommandLine::dockRequested, this, &CmdlineExtrasTest::dock, Qt::QueuedConnection);
    }

    void init()
    {
        tokens_.clear();
        macros_.clear();
        if (cmd_->isFloating())
            dock();
    }

    void cleanupTestCase()
    {
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

    // Tiêu chí 1: 100.000 dòng; mở ≤ 150 ms CPU, tìm ≤ 50 ms CPU; lọc đúng; dòng mới vào ngay.
    void history100k()
    {
        const QString echo = u"LINE Chỉ định điểm kế tiếp hoặc: @%1,0"_s;  // i18n-ok: dữ liệu thử
        const QString note = u"Đã chọn %1 đối tượng"_s;                    // i18n-ok: dữ liệu thử
        for (int i = 0; cmd_->historyCount() < 100000; ++i) // dòng chào + 99.999 dòng: 10.000 dòng "đối tượng"
            cmd_->appendHistory((i % 10 ? echo : note).arg(i));
        QCOMPARE(cmd_->historyCount(), 100000);

        QCoreApplication::processEvents(); // việc còn chờ của 100.000 lần thêm dòng (vẽ lại lịch sử) không tính vào F2
        const auto t0 = test::threadCpu();
        QPointer<ui::HistoryWindow> w = new ui::HistoryWindow(*cmd_, window_.get());
        const double built = cpuMs(test::threadCpu() - t0);
        w->setAttribute(Qt::WA_DeleteOnClose);
        w->show();
        const double shown = cpuMs(test::threadCpu() - t0);
        QVERIFY(QTest::qWaitForWindowExposed(w));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        w->repaint();
        qInfo("F2: dựng %.1f ms, show %.1f ms (CPU, cộng dồn)", built, shown);
        const double open = cpuMs(test::threadCpu() - t0);
        qInfo("F2 100.000 dòng: CPU luồng UI %.1f ms (ngưỡng 150, kiểm ở release)", open);
        QVERIFY2(withinCpu(open, 150), qPrintable(QString::number(open)));
        QCOMPARE(w->rowCount(), 100000);
        auto* list = w->findChild<ui::VirtualTableView*>(u"lines"_s);
        QVERIFY(list && list->verticalScrollBar()->value() == list->verticalScrollBar()->maximum()); // mở ở cuối
        QCOMPARE(w->focusWidget(), w->findChild<QLineEdit*>(u"search"_s)); // gõ tìm ngay

        // Tìm: khóa 100.000 dòng, đo CPU của find (lượt nền).
        QStringList folded;
        for (const QString& l : cmd_->historyLines())
            folded << text::foldForSearch(l);
        const auto t1 = test::threadCpu();
        const std::vector<int> hits = ui::HistoryWindow::find(folded, u"doi tuong"_s); // i18n-ok: khóa tìm
        const double find = cpuMs(test::threadCpu() - t1);
        qInfo("tìm 100.000 dòng: CPU %.1f ms (ngưỡng 50), %zu dòng", find, hits.size());
        QVERIFY2(withinCpu(find, 50), qPrintable(QString::number(find)));
        QCOMPARE(int(hits.size()), 10000);

        QVERIFY(search(w, u"ĐỐI TƯỢNG"_s)); // i18n-ok: chữ tìm, không dấu/hoa vẫn khớp
        QCOMPARE(w->rowCount(), 10000);
        QCOMPARE(w->rowText(0), note.arg(0));
        QVERIFY(search(w, u"@99998,"_s));
        QCOMPARE(w->rowCount(), 1);
        cmd_->appendHistory(note.arg(u"@99998,"_s)); // dòng mới khớp: vào ngay
        QCOMPARE(w->rowCount(), 2);
        QVERIFY(search(w, u"không có đâu"_s)); // i18n-ok: chữ tìm
        QCOMPARE(w->rowCount(), 0);
        QVERIFY(w->findChild<ui::EmptyState*>()->isVisible());
        QVERIFY(!list->isVisible());
        QVERIFY(search(w, QString()));
        QCOMPARE(w->rowCount(), 100001); // chụp lúc mở + một dòng mới (dòng lệnh giữ 100.000 dòng cuối)
        cmd_->appendHistory(u"x"_s);
        QCOMPARE(w->rowCount(), 100002);
        QCOMPARE(w->rowText(w->rowCount() - 1), u"x"_s);
        w->close();
        QVERIFY(spinUntil([&] { return w.isNull(); }));
    }

    // Tiêu chí 1: sao chép giữ nguyên văn; nút ^ chạy TEXTSCR.
    void historyCopy()
    {
        const QStringList odd = {u"  hai khoảng trắng đầu, cuối  "_s, u"tab\tgiữa"_s, u"“ngoặc” <10.0000> @1,2<45"_s}; // i18n-ok: dữ liệu thử
        for (const QString& s : odd)
            cmd_->appendHistory(s);
        QPointer<ui::HistoryWindow> w = openHistory();
        QVERIFY(w);
        w->copy(); // không chọn: mọi dòng
        QCOMPARE(QGuiApplication::clipboard()->text(), cmd_->historyLines().join(u'\n'));
        auto* list = w->findChild<ui::VirtualTableView*>(u"lines"_s);
        const int n = w->rowCount();
        list->selectionModel()->select(list->model()->index(n - 1, 0), QItemSelectionModel::Select);
        list->selectionModel()->select(list->model()->index(n - 3, 0), QItemSelectionModel::Select);
        list->setFocus();
        QTest::keyClick(list, Qt::Key_C, Qt::ControlModifier);
        QCOMPARE(QGuiApplication::clipboard()->text(), odd[0] + u'\n' + odd[2]);
        QTest::keyClick(w, Qt::Key_Escape);
        QVERIFY(spinUntil([&] { return w.isNull(); }));

        auto* button = cmd_->findChild<QAbstractButton*>(u"historyButton"_s);
        QVERIFY(button && button->isVisible() && !button->accessibleName().isEmpty());
        QTest::mouseClick(button, Qt::LeftButton);
        QCOMPARE(macros_, QStringList{u"'_TEXTSCR"_s});
    }

    // Tiêu chí 2: nổi; chuột ngoài hàng ô nhập đi xuyên; kéo; bám lại; kéo ra; độ trong suốt.
    void floating()
    {
        store_->set(ui::kHistoryLines, 3);
        auto* grip = cmd_->findChild<QWidget*>(u"grip"_s);
        QVERIFY(grip);
        QSignalSpy out(cmd_, &CommandLine::floatRequested);
        drag(grip, QPoint(2, 5), QPoint(200, -300)); // đang dock: kéo ra
        QCOMPARE(out.count(), 1);
        QVERIFY(spinUntil([&] { return cmd_->isFloating() && cmd_->isVisible(); }));
        QCOMPARE(cmd_->parentWidget(), canvas_);
        QVERIFY(cmd_->width() < canvas_->width());
        QAbstractScrollArea* h = history();
        QVERIFY(h && h->isVisible());
        QCOMPARE(h->geometry().bottom() + 1, cmd_->y()); // lịch sử ngay trên hàng ô nhập
        QCOMPARE(h->width(), cmd_->width());

        // Chuột: điểm giữa lịch sử thuộc vùng vẽ; hàng ô nhập thuộc dòng lệnh.
        const QPoint inHistory = h->geometry().center();
        QCOMPARE(canvas_->childAt(inHistory), nullptr);
        QCOMPARE(window_->childAt(canvas_->mapTo(window_.get(), inHistory)), canvas_);
        const int before = canvas_->presses;
        QTest::mouseClick(window_->windowHandle(), Qt::LeftButton, {}, canvas_->mapTo(window_.get(), inHistory));
        QCOMPARE(canvas_->presses, before + 1);
        window_->activateWindow(); // ca trước có thể để hộp thoại active
        QVERIFY(QTest::qWaitForWindowActive(window_.get()));
        const QPoint inInput = cmd_->input()->geometry().center();
        QCOMPARE(cmd_->childAt(inInput), cmd_->input());
        QTest::mouseClick(window_->windowHandle(), Qt::LeftButton, {}, cmd_->mapTo(window_.get(), inInput));
        QCOMPARE(canvas_->presses, before + 1);
        QVERIFY(spinUntil([&] { return cmd_->input()->hasFocus(); }));

        // Nền trong suốt: điểm trống của lịch sử = --c-base trộn nền đỏ theo kFloatTransparency; đổi là vẽ lại ngay.
        const auto pixel = [&] {
            const QImage img = canvas_->grab().toImage();
            return img.pixelColor(h->geometry().topLeft() + QPoint(2, 1));
        };
        const auto expect = [&](int transparency) {
            const QColor base = tm_->color(tok::c_base);
            const double a = 1.0 - transparency / 100.0;
            return QColor::fromRgbF(float(base.redF() * a + (1 - a)), float(base.greenF() * a), float(base.blueF() * a));
        };
        for (const int t : {20, 60}) {
            store_->set(ui::kFloatTransparency, t);
            const QColor got = pixel(), want = expect(t);
            qInfo("trong suốt %d%%: %s, mong đợi %s", t, qPrintable(got.name()), qPrintable(want.name()));
            QVERIFY(std::abs(got.red() - want.red()) <= 2 && std::abs(got.green() - want.green()) <= 2
                    && std::abs(got.blue() - want.blue()) <= 2);
        }

        // Kéo: di chuyển trong vùng vẽ (lịch sử theo); thả sát mép dưới: bám lại.
        const QPoint at = cmd_->pos();
        drag(grip, QPoint(2, 5), QPoint(-40, -30));
        QCOMPARE(cmd_->pos(), at + QPoint(-40, -30));
        QCOMPARE(history()->geometry().bottom() + 1, cmd_->y());
        QVERIFY(cmd_->isFloating());
        QSignalSpy back(cmd_, &CommandLine::dockRequested);
        drag(grip, QPoint(2, 5), QPoint(0, canvas_->height()));
        QCOMPARE(cmd_->geometry().bottom() + 1, canvas_->height()); // kẹp trong vùng vẽ
        QCOMPARE(back.count(), 1);
        QVERIFY(spinUntil([&] { return !cmd_->isFloating() && cmd_->width() == window_->width(); }));
        QCOMPARE(cmd_->parentWidget(), window_.get());
        QCOMPARE(history()->parentWidget(), cmd_);
        QVERIFY(!history()->testAttribute(Qt::WA_TransparentForMouseEvents));
        store_->reset(ui::kFloatTransparency);
    }

    // Tiêu chí 3: Viewer, lệnh sửa của Phụ lục A → đúng câu 07.1 §2, không chạy; chip mở trang nâng cấp.
    void editionNotice()
    {
        edition::EditionGate gate; // Viewer khi chưa có giấy phép
        QCOMPARE(gate.edition(), edition::Edition::Viewer);
        std::map<QString, QString> need;
        for (const actions::CommandInfo& c : actions::commands())
            need[QString::fromLatin1(c.name)] = QString::fromLatin1(c.need);
        for (const QString& name : kDrawModify)
            QVERIFY2(need[name] == u"PRO"_s, qPrintable(name));
        QSignalSpy upgrades(cmd_, &CommandLine::upgradeRequested);
        int locked = 0;
        for (const QString& name : kAppendixA) {
            const auto it = need.find(name);
            if (it == need.end() || it->second == u"V"_s)
                continue;
            const ui::Edition e = it->second == u"VP"_s ? ui::Edition::ViewerPro : ui::Edition::Pro;
            QVERIFY2(gate.locked(e), qPrintable(name));
            cmd_->notifyLocked(name, e);
            QCOMPARE(cmd_->historyLine(cmd_->historyCount() - 1),
                     u"Lệnh %1 có trong TrionCAD %2. Dùng thử 30 ngày Tìm hiểu"_s // i18n-ok: câu mong đợi 07.1 §2
                         .arg(name, e == ui::Edition::Pro ? u"Pro"_s : u"Viewer Pro"_s));
            ++locked;
        }
        qInfo("Phụ lục A: %d lệnh khóa ở Viewer", locked);
        QVERIFY(locked >= kDrawModify.size());
        QVERIFY(tokens_.isEmpty());

        // Bấm chip "Tìm hiểu" của dòng cuối (TRION: tọa độ theo cách vẽ của lịch sử).
        cmd_->notifyLocked(u"LINE"_s, ui::Edition::Pro);
        QAbstractScrollArea* h = history();
        h->viewport()->repaint();
        const QFont ui = theme::font(theme::TextRole::Ui), bold = theme::font(theme::TextRole::Ui, theme::Weight::Bold),
                    label = theme::font(theme::TextRole::Label);
        const QString line = cmd_->historyLine(cmd_->historyCount() - 1);
        const edition::EditionGate::CommandNotice n = edition::EditionGate::commandNotice(u"LINE"_s, ui::Edition::Pro);
        const int bold0 = int(n.text.indexOf(u"LINE"_s));
        int x = qRound(tm_->px(tok::sp_5)) + QFontMetrics(ui).horizontalAdvance(n.text.left(bold0))
                + QFontMetrics(bold).horizontalAdvance(u"LINE"_s)
                + QFontMetrics(ui).horizontalAdvance(n.text.mid(bold0 + 4) + u' ');
        x += 2 * qRound(tm_->px(tok::sp_4)) + QFontMetrics(label).horizontalAdvance(n.trial)
             + QFontMetrics(ui).horizontalAdvance(u" "_s);
        const int chipW = 2 * qRound(tm_->px(tok::sp_4)) + QFontMetrics(label).horizontalAdvance(n.learn);
        const QPoint chip(x + chipW / 2, h->viewport()->height() - qRound(tm_->px(tok::h_cmdl)) / 2);
        QVERIFY(line.endsWith(n.learn));
        QTest::mouseClick(h->viewport(), Qt::LeftButton, {}, chip);
        QCOMPARE(upgrades.count(), 1);
        QCOMPARE(upgrades[0][0].toString(), u"LINE"_s);
        QVERIFY(tokens_.isEmpty());
    }

    // Tiêu chí 4: thiết lập lưu Store, áp ngay; Hủy không đổi; xóa AutoCorrect chỉ khi Lưu.
    void settings()
    {
        store_->set(ui::kAutoCorrect, QJsonObject{{u"LNE"_s, QJsonObject{{u"LINE"_s, 3}}},
                                                   {u"CRICLE"_s, QJsonObject{{u"CIRCLE"_s, 4}}}});
        const auto open = [&] {
            auto* d = new ui::InputSearchOptions(window_.get());
            d->setAttribute(Qt::WA_DeleteOnClose);
            d->open();
            return QTest::qWaitForWindowExposed(d) ? QPointer<ui::InputSearchOptions>(d)
                                                   : QPointer<ui::InputSearchOptions>();
        };
        QString removed;
        const auto edit = [&](ui::InputSearchOptions* d) {
            d->findChild<ui::NumericField*>()->setValue(5);
            const QList<ui::DropDown*> boxes = d->findChildren<ui::DropDown*>(); // cỡ chữ, độ trễ
            QCOMPARE(boxes.size(), 2);
            boxes[0]->setCurrentIndex(boxes[0]->findData(16));
            boxes[1]->setCurrentIndex(boxes[1]->findData(200));
            d->findChild<ui::Slider*>()->setValue(40);
            for (QCheckBox* c : d->findChildren<QCheckBox*>())
                c->setChecked(false);
            auto* table = d->findChild<ui::VirtualTableView*>(u"corrections"_s);
            QCOMPARE(table->model()->rowCount(), 2);
            table->setFocus();
            table->setCurrentIndex(table->model()->index(0, 0));
            removed = table->model()->index(0, 0).data().toString();
            QTest::keyClick(table, Qt::Key_Delete);
            QCOMPARE(table->model()->rowCount(), 1);
        };
        const QFont mono = theme::font(theme::TextRole::Mono);

        QPointer<ui::InputSearchOptions> d = open(); // Hủy: không đổi gì
        QVERIFY(d);
        QCOMPARE(truncations(d), QString());
        edit(d);
        QTest::keyClick(d, Qt::Key_Escape);
        QVERIFY(spinUntil([&] { return d.isNull(); }));
        QCOMPARE(store_->get(ui::kHistoryLines), 3);
        QCOMPARE(ui::Autocomplete::corrections().size(), 2);

        d = open(); // Lưu (Enter): ghi Store, áp ngay
        QVERIFY(d);
        edit(d);
        QVERIFY(d->confirmButton()->isDefault());
        d->confirmButton()->click();
        QVERIFY(spinUntil([&] { return d.isNull(); }));
        QCOMPARE(store_->get(ui::kHistoryLines), 5);
        QCOMPARE(store_->get(ui::kFontSize), 16);
        QCOMPARE(store_->get(ui::kFloatTransparency), 40);
        QCOMPARE(store_->get(ui::kCompletionDelay), 200);
        QVERIFY(!store_->get(ui::kSearchSysvars) && !store_->get(ui::kSearchVietnamese) && !store_->get(ui::kImeBypass));
        const QList<ui::Autocomplete::Correction> left = ui::Autocomplete::corrections();
        QCOMPARE(left.size(), 1);
        QVERIFY(left[0].typo != removed && left[0].picks >= 3);

        const double z = 16 / tm_->px(tok::fs);
        QCOMPARE(history()->height(), 5 * qRound(tm_->px(tok::h_cmdl) * z));
        QVERIFY(qAbs(cmd_->input()->font().pointSizeF() - mono.pointSizeF() * z) < 0.01);
        QVERIFY(!cmd_->autocomplete()->search(u"OSMODE"_s).contains(u"OSMODE"_s));
        QVERIFY(!cmd_->autocomplete()->search(u"cắt"_s).contains(u"TRIM"_s)); // i18n-ok: từ khóa tìm
        QVERIFY(!cmd_->input()->imeBypassed());

        // Độ trễ 200 ms: danh sách chưa hiện ngay sau phím, hiện sau đó; Enter trước khi hết trễ vẫn nhận dòng chọn sẵn.
        window_->activateWindow();
        cmd_->setFocus(Qt::OtherFocusReason);
        QVERIFY(spinUntil([&] { return cmd_->input()->hasFocus(); }));
        QTest::keyClicks(cmd_->input(), u"REC"_s);
        QVERIFY(!cmd_->autocomplete()->isVisible());
        QVERIFY(spinUntil([&] { return cmd_->autocomplete()->isVisible(); }));
        QTest::keyClick(cmd_->input(), Qt::Key_Escape);
        QTest::keyClick(cmd_->input(), Qt::Key_Escape);
        tokens_.clear();
        QTest::keyClicks(cmd_->input(), u"RECTA"_s); // Enter trước khi hết trễ: vẫn là dòng chọn sẵn RECTANG
        QTest::keyClick(cmd_->input(), Qt::Key_Return);
        QCOMPARE(tokens_, QStringList{u"RECTANG"_s});

        for (const auto* key : {&ui::kHistoryLines, &ui::kFontSize, &ui::kFloatTransparency, &ui::kCompletionDelay})
            store_->reset(*key);
        for (const auto* key : {&ui::kSearchSysvars, &ui::kSearchVietnamese, &ui::kImeBypass})
            store_->reset(*key);
        store_->reset(ui::kAutoCorrect);
        QCOMPARE(history()->height(), 3 * qRound(tm_->px(tok::h_cmdl)));
    }

    // DoD-U: vi-XL không cắt chữ ở hai hộp thoại và dòng thông báo nâng cấp.
    void pseudoNoTruncation()
    {
        QVERIFY(translator_->setLanguage(i18n::kPseudoLanguage.toString()));
        QVERIFY(spinUntil([&] { return QCoreApplication::translate("trion::ui::HistoryWindow", "Sao chép").size() > 8; })); // i18n-ok: khóa dịch
        QPointer<ui::HistoryWindow> w = openHistory();
        QVERIFY(w);
        QCOMPARE(truncations(w), QString());
        QVERIFY(search(w, u"không có đâu"_s)); // i18n-ok: chữ tìm; EmptyState
        QCOMPARE(truncations(w), QString());
        w->close();
        auto* d = new ui::InputSearchOptions(window_.get());
        d->setAttribute(Qt::WA_DeleteOnClose);
        d->open();
        QVERIFY(QTest::qWaitForWindowExposed(d));
        QCOMPARE(truncations(d), QString());
        d->reject();
        QVERIFY(translator_->setLanguage(u"vi"_s));
        QVERIFY(spinUntil([&] { return QCoreApplication::translate("trion::ui::HistoryWindow", "Sao chép").size() == 8; })); // i18n-ok: khóa dịch
    }
};

QTEST_MAIN(CmdlineExtrasTest)
#include "cmdline_extras_test.moc"
