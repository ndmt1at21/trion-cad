// UI0-108: VirtualTableView, VirtualTreeView. Mỗi tiêu chí nghiệm thu một nhóm hàm test (số đo 10⁶ dòng, bộ nhớ, fps,
// sắp 100.000 dòng có thời gian: views_bench.cpp):
//   1  paintsVisibleRowsOnly: mỗi khung vẽ đúng số ô đang nhìn, như nhau với 10³ và 10⁵ dòng, cuộn giữa danh sách;
//      sizeHintVisibleRowsOnly: đo rộng cột chỉ đọc dòng đang nhìn.
//   2  sortInBackgroundKeepsSelection: bấm đầu cột → model->sort; lúc trả về chưa đổi gì (việc ở nền), luồng UI đọc
//      data() ít hơn N/4 lần; xong thì đúng thứ tự, giữ chọn và dòng hiện hành.
//   3  keyboardNavigation, multiSelection, editWithF2, typeToJump, typeToJumpSlices.
//   4  columnStatePersists, columnMenu (bố cục cột theo khóa view), densityKeepsTopRow (22 ↔ 26 px).
//   5  idleNoTimers (rảnh: 0 timer, 0 lần vẽ), stickyHeader, treeExpandCollapse, pseudoNoTruncation (vi-XL,
//      findTruncations), baseline (Tối/Sáng × 100/150/200%, ảnh theo nền tảng; ghi lại: TRION_UPDATE_BASELINE=<thư mục>
//      chạy test, chép views-*-<nền tảng>.png vào tests/baseline).
#include "app/widgets/views/tests/SampleModels.h"
#include "app/widgets/views/VirtualTableView.h"
#include "app/widgets/views/VirtualTreeView.h"

#include "app/core/i18n/Translator.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/core/UiExecutor.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/generated/Tokens.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "core/base/sched/Scheduler.h"
#include "core/base/settings/Settings.h"

#include <QAbstractEventDispatcher>
#include <QApplication>
#include <QBoxLayout>
#include <QDir>
#include <QHeaderView>
#include <QHoverEvent>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include <chrono>
#include <functional>
#include <memory>
#include <set>

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;
using namespace trion::theme;
using namespace trion::ui;
namespace i18n = trion::i18n;
namespace sched = trion::sched;
namespace settings = trion::settings;

namespace {

#if defined(Q_OS_WIN)
constexpr auto kPlatform = "windows";
#elif defined(Q_OS_MACOS)
constexpr auto kPlatform = "macos";
#else
constexpr auto kPlatform = "linux";
#endif
// Ảnh baseline: lệch ≤ kNoise/255 mỗi kênh bỏ qua; điểm lệch hơn được tối đa 0,1% ảnh (như UI0-106).
constexpr int kNoise = 8;

#ifdef Q_OS_MACOS
constexpr auto kEditKey = Qt::Key_Return;
#else
constexpr auto kEditKey = Qt::Key_F2;
#endif

// Vòng sự kiện thật đến khi cond() hoặc quá hạn (QEventLoop + nhịp 1 ms, như UI0-105).
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

bool settle(const BackgroundModelBase& m)
{
    return spinUntil([&] { return !m.busy(); });
}

int px(tok::PxId id)
{
    return qRound(ThemeManager::instance().px(id));
}

bool show(QWidget& w)
{
    w.show();
    w.activateWindow();
    return QTest::qWaitForWindowExposed(&w) && QTest::qWaitForWindowActive(&w);
}

// Đếm ô được vẽ.
struct CountingDelegate : ViewItemDelegate {
    using ViewItemDelegate::ViewItemDelegate;
    mutable int painted = 0;
    void paint(QPainter* p, const QStyleOptionViewItem& o, const QModelIndex& i) const override
    {
        ++painted;
        ViewItemDelegate::paint(p, o, i);
    }
};

std::set<int> selectedRows(const QAbstractItemView& v)
{
    std::set<int> out;
    for (const QModelIndex& i : v.selectionModel()->selectedRows())
        out.insert(i.row());
    return out;
}

std::set<std::uint64_t> selectedKeys(const QAbstractItemView& v, const sample::TableModel& m)
{
    std::set<std::uint64_t> out;
    for (const QModelIndex& i : v.selectionModel()->selectedRows())
        out.insert(m.row(i.row()).key);
    return out;
}

// Ô đang nhìn: điểm giữa ô (row, column).
QPoint at(const VirtualTableView& v, int row, int column = 0)
{
    return v.visualRect(v.model()->index(row, column)).center();
}

void click(VirtualTableView& v, int row, Qt::KeyboardModifiers mods = {})
{
    QTest::mouseClick(v.viewport(), Qt::LeftButton, mods, at(v, row));
}

// Bảng mẫu đã dựng xong, đang hiện và active.
struct Table {
    sample::TableModel model;
    VirtualTableView view;
    explicit Table(int rows, QSize size = {420, 300})
    {
        model.load(sample::makeItems(rows));
        view.setModel(&model);
        view.resize(size);
    }
    bool ready() { return settle(model) && show(view); }
};

} // namespace

class ViewsTest : public QObject {
    Q_OBJECT
    QTemporaryDir dir_;
    std::unique_ptr<settings::Store> store_;
    std::unique_ptr<UiExecutor> executor_;
    std::unique_ptr<ThemeManager> tm_;
    std::unique_ptr<IconProvider> icons_;
    std::unique_ptr<i18n::Translator> translator_;

private slots:
    void initTestCase()
    {
        QVERIFY(dir_.isValid());
        store_ = std::make_unique<settings::Store>(dir_.filePath(u"user.json"_s));
        settings::Store::setInstance(store_.get());
        QVERIFY(loadFonts()); // chữ Plex nhúng: ảnh không phụ thuộc font của máy
        executor_ = std::make_unique<UiExecutor>();
        tm_ = std::make_unique<ThemeManager>(*store_);
        icons_ = std::make_unique<IconProvider>();
        translator_ = std::make_unique<i18n::Translator>();
        QApplication::setCursorFlashTime(0); // con trỏ editor không nhấp nháy: ảnh tất định
    }

    void init()
    {
        tm_->setTheme(Theme::Dark);
        tm_->setDensity(Density::Compact);
        QApplication::setKeyboardInputInterval(10'000); // gõ trong một hàm test luôn là gõ nối
    }

    void cleanupTestCase()
    {
        translator_.reset();
        icons_.reset();
        tm_.reset();
        settings::Store::setInstance(nullptr);
        store_.reset();
        sched::waitIdle();
        QCoreApplication::processEvents();
        executor_.reset();
    }

    // Tiêu chí 1 (phần không phụ thuộc tải): vẽ lại cả vùng nhìn chạm đúng số ô đang nhìn, cả khi model lớn gấp 100.
    void paintsVisibleRowsOnly()
    {
        int painted[2] = {};
        int reads[2] = {};
        const int sizes[2] = {1'000, 100'000};
        for (int k = 0; k < 2; ++k) {
            Table t(sizes[k]);
            auto* counter = new CountingDelegate(&t.view);
            t.view.setItemDelegate(counter);
            QVERIFY(t.ready());
            t.view.scrollTo(t.model.index(sizes[k] / 2, 0), QAbstractItemView::PositionAtTop);
            QCOMPARE(t.view.indexAt({0, 0}).row(), sizes[k] / 2);
            counter->painted = 0;
            t.model.dataCalls = 0;
            t.view.viewport()->repaint();
            const int page = t.view.viewport()->height() / t.view.rowHeight();
            QVERIFY(counter->painted >= page * 3);
            QVERIFY2(counter->painted <= (page + 1) * 3, qPrintable(QString::number(counter->painted)));
            painted[k] = counter->painted;
            reads[k] = t.model.dataCalls;
        }
        QCOMPARE(painted[1], painted[0]);
        QCOMPARE(reads[1], reads[0]);
    }

    // Tiêu chí 3: mũi tên, PgUp/PgDn, Home/End; chọn đi theo dòng hiện hành.
    void keyboardNavigation()
    {
        Table t(2'000);
        QVERIFY(t.ready());
        VirtualTableView& v = t.view;
        v.setCurrentIndex(t.model.index(0, 0));
        const int page = v.viewport()->height() / v.rowHeight();
        const auto row = [&] { return v.currentIndex().row(); };
        for (int k = 0; k < 3; ++k)
            QTest::keyClick(&v, Qt::Key_Down);
        QCOMPARE(row(), 3);
        QTest::keyClick(&v, Qt::Key_Up);
        QCOMPARE(row(), 2);
        QTest::keyClick(&v, Qt::Key_PageDown);
        QCOMPARE(row(), 2 + page);
        QTest::keyClick(&v, Qt::Key_PageUp);
        QCOMPARE(row(), 2);
        QTest::keyClick(&v, Qt::Key_End);
        QCOMPARE(row(), 1'999);
        QVERIFY(v.viewport()->rect().contains(v.visualRect(v.currentIndex())));
        QCOMPARE(selectedRows(v), std::set<int>{1'999});
        QTest::keyClick(&v, Qt::Key_Home);
        QCOMPARE(row(), 0);
        QCOMPARE(v.verticalScrollBar()->value(), 0);
        QCOMPARE(selectedRows(v), std::set<int>{0});
        QTest::keyClick(&v, Qt::Key_Right); // đổi cột hiện hành, vẫn dòng đó
        QCOMPARE(v.currentIndex().column(), 1);
        QCOMPARE(row(), 0);
        QTest::keyClick(&v, Qt::Key_Left);
        QCOMPARE(v.currentIndex().column(), 0);
    }

    // Tiêu chí 3: Shift/Ctrl chọn nhiều bằng chuột và bàn phím, Ctrl+A.
    void multiSelection()
    {
        Table t(2'000);
        QVERIFY(t.ready());
        VirtualTableView& v = t.view;
        click(v, 3);
        QCOMPARE(selectedRows(v), (std::set<int>{3}));
        click(v, 7, Qt::ShiftModifier);
        QCOMPARE(selectedRows(v), (std::set<int>{3, 4, 5, 6, 7}));
        click(v, 10, Qt::ControlModifier);
        QCOMPARE(selectedRows(v), (std::set<int>{3, 4, 5, 6, 7, 10}));
        click(v, 5, Qt::ControlModifier);
        QCOMPARE(selectedRows(v), (std::set<int>{3, 4, 6, 7, 10}));

        click(v, 1);
        for (int k = 0; k < 3; ++k)
            QTest::keyClick(&v, Qt::Key_Down, Qt::ShiftModifier);
        QCOMPARE(selectedRows(v), (std::set<int>{1, 2, 3, 4}));
        QTest::keyClick(&v, Qt::Key_Up, Qt::ShiftModifier);
        QCOMPARE(selectedRows(v), (std::set<int>{1, 2, 3}));
        QTest::keyClick(&v, Qt::Key_Down, Qt::ControlModifier);
        QTest::keyClick(&v, Qt::Key_Down, Qt::ControlModifier);
        QCOMPARE(v.currentIndex().row(), 5);
        QCOMPARE(selectedRows(v), (std::set<int>{1, 2, 3}));
        QTest::keyClick(&v, Qt::Key_Space, Qt::ControlModifier);
        QCOMPARE(selectedRows(v), (std::set<int>{1, 2, 3, 5}));

        // Shift từ dòng đã cuộn khuất: chọn trọn đoạn.
        click(v, 2);
        QTest::keyClick(&v, Qt::Key_End, Qt::ShiftModifier);
        QCOMPARE(int(selectedRows(v).size()), 1'998);
        QTest::keyClick(&v, Qt::Key_A, Qt::ControlModifier);
        QCOMPARE(int(v.selectionModel()->selectedRows().size()), 2'000);
    }

    // Tiêu chí 3: F2 sửa tại chỗ qua delegate (ô không sửa được → ô tên của dòng), editor theo .pinp; Enter ghi
    // (setData → model dựng lại ở nền), Esc bỏ.
    void editWithF2()
    {
        Table t(200);
        QVERIFY(t.ready());
        VirtualTableView& v = t.view;
        v.header()->resizeSection(0, 220);      // tên vừa ô: editor không tự giãn (QExpandingLineEdit)
        v.setCurrentIndex(t.model.index(5, 1)); // cột "Giá trị" không sửa được
        const QString before = t.model.row(5).name;
        QTest::keyClick(&v, kEditKey);
        auto* editor = qobject_cast<QLineEdit*>(QApplication::focusWidget());
        QVERIFY(editor);
        QCOMPARE(editor->parentWidget(), v.viewport());
        QCOMPARE(editor->text(), before);
        const int left = v.header()->sectionViewportPosition(0);
        const QRect cell(left, v.visualRect(t.model.index(5, 0)).top(), v.header()->sectionSize(0), v.rowHeight());
        QCOMPARE(editor->geometry().left(), cell.left() + px(tok::sp_4));
        QCOMPARE(editor->geometry().right(), cell.right() - px(tok::sp_4));
        QVERIFY(editor->height() <= cell.height() && editor->height() >= editor->fontMetrics().height());
        QVERIFY(qAbs(editor->geometry().center().y() - cell.center().y()) <= 1);

        editor->setText(u"Tên mới"_s); // i18n-ok: dữ liệu thử
        QTest::keyClick(editor, Qt::Key_Return);
        QVERIFY(spinUntil([&] { return !t.model.busy() && t.model.row(5).name == u"Tên mới"_s; }));

        QTest::keyClick(&v, kEditKey);
        editor = qobject_cast<QLineEdit*>(QApplication::focusWidget());
        QVERIFY(editor);
        editor->setText(u"Bỏ"_s); // i18n-ok: dữ liệu thử
        QTest::keyClick(editor, Qt::Key_Escape);
        QVERIFY(settle(t.model));
        QCOMPARE(t.model.row(5).name, u"Tên mới"_s);
        QTRY_VERIFY(v.hasFocus());
    }

    // Tiêu chí 3: gõ chữ nhảy tới dòng, không phân biệt dấu và hoa/thường; lặp phím đi qua các dòng; quay vòng.
    // Dòng i: 0 "Lớp tường", 1 "KT-CỬA", 2 "ĐƯỜNG trục", 3 "LỚP hoàn thiện", 4 "Kích thước", lặp lại.
    void typeToJump()
    {
        Table t(100);
        QVERIFY(t.ready());
        VirtualTableView& v = t.view;
        v.setCurrentIndex(t.model.index(0, 0));
        const auto fresh = [](QWidget* w, char first) { // phím đầu của lần tìm mới, các phím sau là gõ nối
            QApplication::setKeyboardInputInterval(0);
            spinFor(5ms);
            QTest::keyClick(w, first);
            QApplication::setKeyboardInputInterval(10'000);
        };
        const auto row = [&] { return v.currentIndex().row(); };
        fresh(&v, 'd');
        QTest::keyClicks(&v, u"uong"_s);
        QCOMPARE(row(), 2);
        QCOMPARE(selectedRows(v), std::set<int>{2});
        fresh(&v, 'l');
        QTest::keyClicks(&v, u"op"_s);
        QCOMPARE(row(), 3); // "LỚP hoàn thiện": tìm mới từ dòng sau dòng hiện hành
        QCOMPARE(t.model.row(row()).name.left(3), u"LỚP"_s);
        fresh(&v, 'k');
        QCOMPARE(row(), 4);
        QTest::keyClick(&v, 'k'); // lặp phím: dòng kế tiếp bắt đầu bằng "k"
        QCOMPARE(row(), 6);
        QTest::keyClick(&v, 'k');
        QCOMPARE(row(), 9);
        fresh(&v, 'z');
        QCOMPARE(row(), 9); // không dòng nào khớp: giữ nguyên
        v.setCurrentIndex(t.model.index(98, 0));
        fresh(&v, 'd');
        QCOMPARE(row(), 2);   // quay vòng về đầu
        v.setSearchColumn(2); // cột "Mã": khóa = dòng + 1
        fresh(&v, '7');
        QCOMPARE(row(), 6);
    }

    // Đo rộng cột (ResizeToContents, nhấp đúp mép đầu cột) chỉ đọc các dòng đang nhìn, gồm lề --sp-4 hai bên.
    void sizeHintVisibleRowsOnly()
    {
        Table t(100'000);
        QVERIFY(t.ready());
        VirtualTableView& v = t.view;
        v.scrollTo(t.model.index(50'000, 0), QAbstractItemView::PositionAtTop);
        t.model.dataCalls = 0;
        const int width = v.sizeHintForColumn(0);
        const int page = v.viewport()->height() / v.rowHeight();
        QVERIFY2(t.model.dataCalls <= (page + 2) * 20, qPrintable(QString::number(t.model.dataCalls)));
        for (int r = 50'000; r < 50'000 + page; ++r)
            QVERIFY(width >= v.fontMetrics().horizontalAdvance(t.model.row(r).name) + 2 * px(tok::sp_4));
        emit v.header()->sectionHandleDoubleClicked(0);
        QCOMPARE(v.header()->sectionSize(0), width);
    }

    // Tiêu chí 3, DoD-U 4: danh sách lớn quét theo lát ≤ 2 ms trên luồng UI, phím mới hủy lần quét cũ.
    void typeToJumpSlices()
    {
        Table t(50'000);
        auto items = std::make_shared<sample::Items>(*sample::makeItems(50'000));
        items->back().name = u"Zeta"_s; // i18n-ok: dữ liệu thử; dòng duy nhất bắt đầu bằng "z"
        t.model.load(items);
        QVERIFY(t.ready());
        VirtualTableView& v = t.view;
        v.setCurrentIndex(t.model.index(0, 0));
        QApplication::setKeyboardInputInterval(0);
        spinFor(5ms);
        QTest::keyClick(&v, 'z');
        QCOMPARE(v.currentIndex().row(), 0); // 50.000 lần fold > một lát: chưa xong khi trả về
        QApplication::setKeyboardInputInterval(10'000);
        QTest::keyClick(&v, 'x'); // "zx": không khớp; lần quét "z" phải dừng
        spinFor(2s);
        QCOMPARE(v.currentIndex().row(), 0);
        QApplication::setKeyboardInputInterval(0);
        spinFor(5ms);
        QTest::keyClick(&v, 'z');
        QVERIFY(spinUntil([&] { return v.currentIndex().row() == 49'999; }));
        QVERIFY(v.viewport()->rect().contains(v.visualRect(v.currentIndex())));
    }

    // Tiêu chí 2 (phần không phụ thuộc tải; thời gian, khựng: views_bench).
    void sortInBackgroundKeepsSelection()
    {
        constexpr int kRows = 20'000;
        Table t(kRows);
        t.view.setSortingEnabled(true);
        QVERIFY(t.ready());
        VirtualTableView& v = t.view;
        for (int r = 0; r < kRows; r += 397)
            v.selectionModel()->select(t.model.index(r, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        v.selectionModel()->setCurrentIndex(t.model.index(kRows / 2, 0), QItemSelectionModel::NoUpdate);
        const auto keys = selectedKeys(v, t.model);
        const std::uint64_t current = t.model.row(v.currentIndex().row()).key;
        QCOMPARE(int(keys.size()), (kRows + 396) / 397);

        const auto sortBy = [&](int column, Qt::SortOrder order) {
            t.model.dataCalls = 0;
            const QPoint header(v.header()->sectionViewportPosition(column) + v.header()->sectionSize(column) / 2,
                                v.header()->height() / 2);
            QTest::mouseClick(v.header()->viewport(), Qt::LeftButton, {}, header);
            QCOMPARE(v.header()->sortIndicatorSection(), column);
            QCOMPARE(v.header()->sortIndicatorOrder(), order);
            QVERIFY(t.model.busy()); // việc ở nền: chưa áp gì khi lời gọi trả về
            QVERIFY(settle(t.model));
            QVERIFY2(t.model.dataCalls < kRows / 4, qPrintable(QString::number(t.model.dataCalls)));
            for (int r = 1; r < kRows; ++r)
                QVERIFY(!sample::TableModel::before(t.model.row(r), t.model.row(r - 1), column, order));
            QCOMPARE(selectedKeys(v, t.model), keys);
            QCOMPARE(t.model.row(v.currentIndex().row()).key, current);
        };
        sortBy(0, Qt::AscendingOrder);
        sortBy(0, Qt::DescendingOrder);
        sortBy(1, Qt::AscendingOrder);
        QCOMPARE(t.model.sorts, 3);
    }

    // Tiêu chí 4: rộng, ẩn, cột sắp lưu theo khóa view; view mới cùng khóa nạp lại, khóa khác giữ mặc định.
    void columnStatePersists()
    {
        sample::TableModel model;
        model.load(sample::makeItems(300));
        QVERIFY(settle(model));
        {
            VirtualTableView v;
            v.setModel(&model);
            v.setSortingEnabled(true);
            v.setStateKey(u"test/columns"_s);
            v.header()->resizeSection(0, 150); // cột hiện cuối cùng giãn theo view (stretchLastSection)
            v.setColumnHidden(2, true);
            v.header()->setSortIndicator(1, Qt::DescendingOrder);
            QCOMPARE(model.column_, 1);
        }
        QVERIFY(store_->value(u"views/test/columns"_s).isString());
        model.sort(-1, Qt::AscendingOrder);
        VirtualTableView v;
        v.setModel(&model);
        v.setSortingEnabled(true);
        v.setStateKey(u"test/columns"_s);
        QCOMPARE(v.header()->sectionSize(0), 150);
        QVERIFY(v.isColumnHidden(2));
        QCOMPARE(v.header()->sortIndicatorSection(), 1);
        QCOMPARE(v.header()->sortIndicatorOrder(), Qt::DescendingOrder);
        QCOMPARE(model.column_, 1); // nạp lại cũng sắp lại model
        QCOMPARE(model.order_, Qt::DescendingOrder);
        VirtualTableView other;
        other.setModel(&model);
        other.setStateKey(u"test/other"_s);
        QVERIFY(!other.isColumnHidden(2));
        QCOMPARE(other.header()->sortIndicatorSection(), -1);
        QVERIFY(settle(model));
    }

    // Tiêu chí 4: menu chuột phải của đầu cột ẩn/hiện cột; cột hiện cuối cùng không ẩn được.
    void columnMenu()
    {
        Table t(50);
        t.view.setStateKey(u"test/menu"_s);
        QVERIFY(t.ready());
        VirtualTableView& v = t.view;
        const auto open = [&] {
            emit v.header()->customContextMenuRequested(QPoint(4, 4));
            return qobject_cast<QMenu*>(QApplication::activePopupWidget());
        };
        QMenu* menu = open();
        QVERIFY(menu);
        QCOMPARE(menu->actions().size(), 3);
        QCOMPARE(menu->actions()[1]->text(), QCoreApplication::translate("ViewsSample", "Giá trị"));
        menu->actions()[1]->trigger();
        menu->close();
        QVERIFY(v.isColumnHidden(1));
        menu = open();
        QVERIFY(menu);
        menu->actions()[2]->trigger();
        menu->close();
        QVERIFY(v.isColumnHidden(2));
        menu = open();
        QVERIFY(menu);
        QVERIFY(!menu->actions()[0]->isEnabled()); // cột hiện cuối cùng
        menu->actions()[1]->trigger();
        menu->close();
        QVERIFY(!v.isColumnHidden(1));
        VirtualTableView again;
        again.setModel(&t.model);
        again.setStateKey(u"test/menu"_s);
        QVERIFY(!again.isColumnHidden(1));
        QVERIFY(again.isColumnHidden(2));
    }

    // Tiêu chí 4: đổi mật độ 22 ↔ 26 px giữ dòng đầu đang hiện.
    void densityKeepsTopRow()
    {
        Table t(2'000);
        QVERIFY(t.ready());
        VirtualTableView& v = t.view;
        v.scrollTo(t.model.index(500, 0), QAbstractItemView::PositionAtTop);
        QCOMPARE(v.indexAt({0, 0}).row(), 500);
        QCOMPARE(v.rowHeight(), 22);
        tm_->setDensity(Density::Comfortable);
        QTRY_COMPARE(v.rowHeight(), 26);
        QCOMPARE(v.indexAt({0, 0}).row(), 500);
        QCOMPARE(v.visualRect(t.model.index(501, 0)).top(), 26);
        QTRY_COMPARE(v.verticalScrollBar()->pageStep(), v.viewport()->height() / 26);
        tm_->setDensity(Density::Compact);
        QTRY_COMPARE(v.rowHeight(), 22);
        QCOMPARE(v.indexAt({0, 0}).row(), 500);
        QCOMPARE(v.visualRect(t.model.index(501, 0)).top(), 22);
    }

    // Đầu cột dính trên vùng nhìn khi cuộn dọc, đi theo khi cuộn ngang.
    void stickyHeader()
    {
        Table t(2'000, {300, 240});
        QVERIFY(t.ready());
        VirtualTableView& v = t.view;
        v.header()->resizeSection(0, 260);
        v.header()->resizeSection(1, 200);
        QTRY_VERIFY(v.horizontalScrollBar()->maximum() > 0);
        const QRect head = v.header()->geometry();
        QVERIFY(v.header()->isVisible());
        QCOMPARE(head.bottom() + 1, v.viewport()->geometry().top());
        QCOMPARE(head.width(), v.viewport()->width());
        v.verticalScrollBar()->setValue(1'000);
        v.horizontalScrollBar()->setValue(120);
        QCOMPARE(v.header()->geometry(), head);
        QCOMPARE(v.header()->offset(), 120);
        QCOMPARE(v.indexAt({0, 0}).row(), 1'000);
        QCOMPARE(v.indexAt({0, 0}).column(), 0);
        QCOMPARE(v.header()->sectionViewportPosition(1), 140);
    }

    // Cây trên danh sách phẳng: mũi tên mở/đóng không đổi chọn; phải/trái mở, xuống con, đóng, lên cha; thụt lề.
    void treeExpandCollapse()
    {
        sample::TreeModel model;
        model.load(sample::makeTree(3, 3, 5));
        VirtualTreeView v;
        v.setModel(&model);
        v.resize(360, 400);
        QVERIFY(settle(model) && show(v));
        QCOMPARE(model.rowCount(), 3);
        const int step = px(tok::sp_5) - px(tok::sp_1) + px(tok::sp_2);
        const auto arrow = [&](int row) { // giữa ô mũi tên của dòng
            const QModelIndex i = model.index(row, 0);
            const int depth = i.data(VirtualTreeView::DepthRole).toInt();
            const int x = v.header()->sectionViewportPosition(0) + px(tok::sp_2) + depth * step + step / 2 - 2;
            return QPoint(x, v.visualRect(i).center().y());
        };
        QTest::mouseClick(v.viewport(), Qt::LeftButton, {}, arrow(0));
        QVERIFY(settle(model));
        QCOMPARE(model.rowCount(), 6);
        QVERIFY(v.isExpanded(model.index(0, 0)));
        QVERIFY(v.selectionModel()->selectedRows().isEmpty());
        QCOMPARE(model.index(1, 0).data(VirtualTreeView::DepthRole).toInt(), 1);
        QCOMPARE(v.visualRect(model.index(1, 0)).left() - v.visualRect(model.index(0, 0)).left(), step);

        v.setCurrentIndex(model.index(0, 0));
        QTest::keyClick(&v, Qt::Key_Left); // đóng
        QVERIFY(settle(model));
        QCOMPARE(model.rowCount(), 3);
        QTest::keyClick(&v, Qt::Key_Right); // mở
        QVERIFY(settle(model));
        QCOMPARE(model.rowCount(), 6);
        QCOMPARE(v.currentIndex().row(), 0);
        QTest::keyClick(&v, Qt::Key_Right); // xuống con đầu
        QCOMPARE(v.currentIndex().row(), 1);
        QTest::keyClick(&v, Qt::Key_Right); // mở nhóm con: 5 lá chèn ngay dưới
        QVERIFY(settle(model));
        QCOMPARE(model.rowCount(), 11);
        QCOMPARE(model.index(2, 0).data(VirtualTreeView::DepthRole).toInt(), 2);
        QVERIFY(!model.index(2, 0).data(VirtualTreeView::ExpandedRole).isValid()); // lá
        QTest::keyClick(&v, Qt::Key_Down);
        QTest::keyClick(&v, Qt::Key_Down);
        QCOMPARE(v.currentIndex().row(), 3);
        QTest::keyClick(&v, Qt::Key_Left); // lá: lên cha
        QCOMPARE(v.currentIndex().row(), 1);
        QTest::keyClick(&v, Qt::Key_Left); // đóng nhóm con
        QVERIFY(settle(model));
        QCOMPARE(model.rowCount(), 6);
        QTest::keyClick(&v, Qt::Key_Left); // lên nhóm gốc
        QCOMPARE(v.currentIndex().row(), 0);
        QCOMPARE(selectedRows(v), std::set<int>{0});
    }

    // Tiêu chí 5 (DoD-H 6): rảnh thì không timer nào của view, model; không vẽ lại.
    void idleNoTimers()
    {
        Table t(2'000);
        sample::TreeModel treeModel;
        treeModel.load(sample::makeTree(20, 0, 10), {1});
        VirtualTreeView tree;
        tree.setModel(&treeModel);
        tree.setStateKey(u"test/idle"_s);
        QVERIFY(t.ready() && settle(treeModel) && show(tree));
        spinFor(100ms); // việc trễ của lần hiện đầu (bố trí, fetchMore) chạy xong
        struct Paints : QObject {
            int count = 0;
            bool eventFilter(QObject*, QEvent* e) override
            {
                count += e->type() == QEvent::Paint;
                return false;
            }
        } paints;
        for (QWidget* w : {t.view.viewport(), tree.viewport(), t.view.header()->viewport(), tree.header()->viewport()})
            w->installEventFilter(&paints);
        spinFor(300ms);
        QCOMPARE(paints.count, 0);
        QList<QObject*> objects{&t.view, &tree, &t.model, &treeModel};
        objects += t.view.findChildren<QObject*>();
        objects += tree.findChildren<QObject*>();
        for (QObject* o : objects)
            QVERIFY2(QAbstractEventDispatcher::instance()->timersForObject(o).isEmpty(), o->metaObject()->className());
    }

    // DoD-U 3: ngôn ngữ giả vi-XL (+40%), cả hai mật độ: không chữ nào bị cắt ở đầu cột và ô đang nhìn.
    void pseudoNoTruncation()
    {
        QWidget w;
        auto* row = new QHBoxLayout(&w);
        Table t(30);
        sample::TreeModel treeModel;
        treeModel.load(sample::makeTree(4, 2, 3), {1, 2});
        auto* tree = new VirtualTreeView;
        tree->setModel(&treeModel);
        row->addWidget(&t.view);
        row->addWidget(tree);
        w.resize(900, 360);
        QVERIFY(settle(t.model) && settle(treeModel) && show(w));
        t.view.header()->resizeSection(0, 200);
        t.view.header()->resizeSection(1, 90);
        tree->header()->resizeSection(0, 260);
        const auto report = [&w] {
            QStringList out;
            for (const i18n::Truncation& x : i18n::findTruncations(&w))
                out << x.toString();
            return out.join(u"; "_s);
        };
        QCOMPARE(report(), QString());
        const QString vi = t.model.headerData(1, Qt::Horizontal, Qt::DisplayRole).toString();
        QVERIFY(translator_->setLanguage(i18n::kPseudoLanguage.toString()));
        QTRY_VERIFY(t.model.headerData(1, Qt::Horizontal, Qt::DisplayRole).toString().size() >= vi.size() + 2);
        QTRY_COMPARE(report(), QString());
        for (Density d : {Density::Comfortable, Density::Compact}) {
            tm_->setDensity(d);
            QTRY_COMPARE(report(), QString());
        }
        QVERIFY(translator_->setLanguage(u"vi"_s));
    }

    // DoD-U 1: bảng (thường, rê, chọn, chọn + focus bàn phím, editor) và cây (mở, đóng, lá, chọn, rê) ở Tối/Sáng ×
    // 100/150/200%, so baseline theo nền tảng.
    void baseline_data()
    {
        QTest::addColumn<bool>("light");
        QTest::addColumn<qreal>("dpr");
        for (bool light : {false, true})
            for (qreal dpr : {1.0, 1.5, 2.0})
                QTest::addRow("%s-%d", light ? "light" : "dark", qRound(dpr * 100)) << light << dpr;
    }
    void baseline()
    {
        QFETCH(bool, light);
        QFETCH(qreal, dpr);
        tm_->setTheme(light ? Theme::Light : Theme::Dark);
        QCoreApplication::processEvents();   // Qt 6.11 gửi ApplicationPaletteChange qua hàng đợi
        QCursor::setPos(QPoint(-100, -100)); // chuột ngoài mọi cửa sổ: rê chỉ ở dòng ta chọn
        constexpr int kW = 330, kH = 170;
        Table t(40, {kW, kH});
        sample::TreeModel treeModel;
        treeModel.load(sample::makeTree(3, 2, 2), {1, 2});
        VirtualTreeView tree;
        tree.setModel(&treeModel);
        tree.resize(kW, kH);
        QVERIFY(t.ready() && settle(treeModel) && show(tree));
        for (VirtualTableView* v : {&t.view, static_cast<VirtualTableView*>(&tree)}) {
            v->header()->resizeSection(0, 150);
            v->header()->resizeSection(1, 56);
            QHoverEvent hover(QEvent::HoverMove, at(*v, 1), v->viewport()->mapToGlobal(at(*v, 1)), QPointF(-1, -1));
            QCoreApplication::sendEvent(v->viewport(), &hover);
        }
        QImage sheet(QSize(kW * 3, kH) * dpr, QImage::Format_ARGB32_Premultiplied);
        sheet.setDevicePixelRatio(dpr);
        sheet.fill(tm_->color(tok::c_chrome));
        QPainter p(&sheet);
        // Bảng: dòng 1 rê, dòng 2 chọn, dòng 3 chọn và hiện hành có focus bàn phím.
        t.view.selectionModel()->select(t.model.index(2, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        t.view.selectionModel()->setCurrentIndex(t.model.index(3, 0),
                                                 QItemSelectionModel::Select | QItemSelectionModel::Rows);
        t.view.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&t.view));
        t.view.setFocus(Qt::TabFocusReason);
        t.view.setAttribute(Qt::WA_KeyboardFocusChange); // khung focus: focus đến từ bàn phím
        QVERIFY(t.view.hasFocus());
        t.view.render(&p, QPoint(0, 0));
        // Bảng: editor mở ở dòng 4.
        t.view.edit(t.model.index(4, 0));
        QVERIFY(qobject_cast<QLineEdit*>(QApplication::focusWidget()));
        t.view.render(&p, QPoint(kW, 0));
        // Cây: nhóm mở (0, 1), lá, nhóm đóng; dòng 1 rê, dòng 3 chọn.
        tree.selectionModel()->select(treeModel.index(3, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        tree.render(&p, QPoint(2 * kW, 0));
        p.end();

        const QString name = u"views-%1-%2-%3.png"_s.arg(light ? u"light"_s : u"dark"_s)
                                 .arg(qRound(dpr * 100))
                                 .arg(QLatin1StringView(kPlatform));
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

QTEST_MAIN(ViewsTest)
#include "views_test.moc"
