// UI0-106: nút và điều khiển nhập (Controls.h). Mỗi tiêu chí nghiệm thu một nhóm hàm test:
//   1  states: tấm ảnh 13 biến thể × 8 trạng thái (mặc định, rê, nhấn, focus, vô hiệu, bật, khóa, lỗi) so baseline
//      theo nền tảng ở Tối/Sáng × 100/150/200%; trên chính tấm ảnh: viền focus và lỗi đúng màu token ở mép, ô khóa
//      khác ô mặc định về hình (mặt nạ điểm ảnh khác nền), không chỉ về màu. Ghi lại baseline:
//      TRION_UPDATE_BASELINE=<thư mục> chạy test, chép sheet-*-<nền tảng>.png vào tests/baseline.
//   2  comboFilter: 10.000 mục; "lop" khớp "Lớp", "LỚP"; "duong" khớp "ĐƯỜNG"; mỗi phím (lọc + bố cục + vẽ danh
//      sách) ≤ 16 ms ở vòng tốt nhất, chỉ kiểm ở release (tests/timing.h); UiProbe typing/autocomplete (phím qua cửa
//      sổ đến hết khung) chỉ báo số. Model đổi khi popup mở: khóa gấp dựng lại.
//   3  keyboardActivate, keyboardGroups, keyboardPopups: Space/Enter kích hoạt; mũi tên trong SegmentedGroup và
//      TabBar; Alt+↓ mở DropDown, ComboBox, SplitButton; Esc đóng không đổi giá trị; Esc của SearchField xóa chữ.
//   4  heights: cao theo --h-ctl, --h-btn, --h-ctl-l, --h-ftab ở gọn và thoáng; pseudoNoTruncation: bố cục mẫu ở
//      ngôn ngữ giả vi-XL, findTruncations rỗng.
//   locked: khóa không đổi giá trị (toggle, nhóm chọn, danh sách, slider, ô tìm), nút vẫn phát clicked;
//   splitButton: nhấp vùng mũi tên mở menu, phần chính phát clicked.
#include "app/widgets/controls/Controls.h"

#include "app/core/i18n/Translator.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/core/perf/UiProbe.h"
#include "app/core/UiExecutor.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/generated/Tokens.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "core/base/sched/Scheduler.h"
#include "core/base/text/Fold.h"
#include "tests/timing.h"

#include <QApplication>
#include <QBoxLayout>
#include <QElapsedTimer>
#include <QHoverEvent>
#include <QJsonObject>
#include <QLabel>
#include <QListView>
#include <QPainter>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>
#include <functional>
#include <memory>

using namespace Qt::StringLiterals;
using namespace trion::theme;
using namespace trion::ui;
namespace i18n = trion::i18n;
namespace sched = trion::sched;
namespace settings = trion::settings;

namespace {

using trion::test::kTimed;

// Baseline theo nền tảng: FreeType của Linux và Windows offscreen dựng chữ khác nhau (như fonts_test). Dung sai:
// lệch ≤ kNoise/255 mỗi kênh bỏ qua; điểm lệch hơn được tối đa 0,1% ảnh.
#if defined(Q_OS_WIN)
constexpr auto kPlatform = "windows";
#elif defined(Q_OS_MACOS)
constexpr auto kPlatform = "macos";
#else
constexpr auto kPlatform = "linux";
#endif
constexpr int kNoise = 8;
constexpr double kKeyBudgetMs = 16; // tiêu chí 2, DoD-L "danh sách tự hoàn thành cập nhật sau mỗi phím"
constexpr int kCellW = 140, kCellH = 34, kInset = 4;

enum State { Default, Hover, Pressed, Focus, Disabled, On, Locked, Error, kStates };

void focusByKeyboard(QWidget* w)
{
    w->setFocus(Qt::TabFocusReason);
    w->window()->setAttribute(Qt::WA_KeyboardFocusChange); // như khi focus đến bằng phím Tab
}

void hover(QWidget* w)
{
    w->setAttribute(Qt::WA_UnderMouse);
}

// Một hàng của tấm ảnh: dựng điều khiển, rồi (sau khi hiện) đặt trạng thái; false: trạng thái không áp dụng, ô trống.
struct Row {
    const char* name;
    int width; // 0: theo sizeHint
    std::function<QWidget*(QWidget*)> make;
    std::function<bool(QWidget*, State)> apply;
    State popup = kStates; // trạng thái mở popup (vẽ ở lượt sau, xem states())
};

bool applyButton(QWidget* w, State s)
{
    auto* b = static_cast<Button*>(w);
    switch (s) {
    case Hover:
        hover(b);
        break;
    case Pressed:
        b->setDown(true);
        break;
    case Focus:
        focusByKeyboard(b);
        break;
    case Disabled:
        b->setEnabled(false);
        break;
    case On: // nút tách: menu đang mở; nút chính, nguy hiểm: không có trạng thái bật
        if (auto* split = qobject_cast<SplitButton*>(b)) {
            split->showDropMenu();
            return QApplication::activePopupWidget() != nullptr;
        }
        if (b->kind() == Button::Kind::Primary || b->kind() == Button::Kind::Danger)
            return false;
        b->setCheckable(true);
        b->setChecked(true);
        break;
    case Locked:
        b->setLocked(true);
        break;
    case Error:
        b->setError(true);
        break;
    default:
        break;
    }
    return true;
}

bool applyCombo(QWidget* w, State s)
{
    auto* d = static_cast<DropDown*>(w);
    switch (s) {
    case Hover:
        hover(d);
        break;
    case Pressed: // đang mở danh sách
        d->showPopup();
        return QApplication::activePopupWidget() != nullptr;
    case Focus:
        focusByKeyboard(d);
        break;
    case Disabled:
        d->setEnabled(false);
        break;
    case On:
        return false;
    case Locked:
        d->setLocked(true);
        break;
    case Error:
        d->setError(true);
        break;
    default:
        break;
    }
    return true;
}

QList<Row> rows()
{
    const auto button = [](QString text, Button::Kind kind, QString icon = {}) {
        return [=](QWidget* parent) -> QWidget* {
            auto* b = new Button(text, kind, parent);
            if (!icon.isEmpty())
                b->setIconName(icon);
            return b;
        };
    };
    const auto combo = [](bool filter) {
        return [=](QWidget* parent) -> QWidget* {
            DropDown* c = filter ? new ComboBox(parent) : new DropDown(parent);
            c->addItems({u"ISO A1 (841 × 594 mm)"_s, u"ISO A3 (420 × 297 mm)"_s}); // i18n-ok: dữ liệu thử
            return c;
        };
    };
    return {
        {"button", 0, button(u"Xem trước…"_s, Button::Kind::Normal), applyButton},
        {"primary", 0, button(u"In"_s, Button::Kind::Primary, u"print"_s), applyButton},
        {"danger", 0, button(u"Xóa 3 layout"_s, Button::Kind::Danger), applyButton},
        {"ghost", 0, button(u"Hủy"_s, Button::Kind::Ghost), applyButton},
        {"icon", 0, button({}, Button::Kind::Icon, u"osnap"_s), applyButton},
        {"toggle", 0, [](QWidget* p) -> QWidget* { return new ToggleButton(u"Lưới"_s, Button::Kind::Normal, p); },
         applyButton},
        {"split", 0,
         [](QWidget* p) -> QWidget* {
             auto* split = new SplitButton(u"In"_s, nullptr, p);
             auto* menu = new QMenu(split);
             menu->addAction(u"In hàng loạt"_s); // i18n-ok: dữ liệu thử
             split->setDropMenu(menu);
             return split;
         },
         applyButton, On},
        {"segmented", 0,
         [](QWidget* p) -> QWidget* {
             auto* g = new SegmentedGroup(p);
             for (const QString& s : {u"mm"_s, u"m"_s, u"in"_s})
                 g->addSegment(s);
             return g;
         },
         [](QWidget* w, State s) {
             auto* g = static_cast<SegmentedGroup*>(w);
             switch (s) {
             case Hover:
                 hover(g->segment(1));
                 break;
             case Pressed:
                 g->segment(1)->setDown(true);
                 break;
             case Focus:
                 focusByKeyboard(g->segment(0));
                 break;
             case Disabled:
                 g->setEnabled(false);
                 break;
             case On:
                 g->setCurrentIndex(1);
                 break;
             case Locked:
                 g->segment(2)->setLocked(true);
                 break;
             case Error:
                 g->setError(true);
                 break;
             default:
                 break;
             }
             return true;
         }},
        {"dropdown", kCellW - 2 * kInset, combo(false), applyCombo, Pressed},
        {"combobox", kCellW - 2 * kInset, combo(true), applyCombo, Pressed},
        {"search", kCellW - 2 * kInset,
         [](QWidget* p) -> QWidget* {
             auto* f = new SearchField(p);
             f->setPlaceholderText(u"Tìm layer"_s); // i18n-ok: dữ liệu thử
             return f;
         },
         [](QWidget* w, State s) {
             auto* f = static_cast<SearchField*>(w);
             switch (s) {
             case Hover:
                 hover(f);
                 break;
             case Pressed: // đang gõ
                 focusByKeyboard(f);
                 f->setText(u"KT"_s); // i18n-ok: dữ liệu thử
                 break;
             case Focus:
                 focusByKeyboard(f);
                 break;
             case Disabled:
                 f->setEnabled(false);
                 break;
             case On:
                 return false;
             case Locked:
                 f->setLocked(true);
                 break;
             case Error:
                 f->setText(u"KT-01?"_s); // i18n-ok: dữ liệu thử
                 f->setError(true);
                 break;
             default:
                 break;
             }
             return true;
         }},
        {"tabs", kCellW - 2 * kInset,
         [](QWidget* p) -> QWidget* {
             auto* t = new TabBar(p);
             for (const QString& s : {u"Chung"_s, u"Tóm tắt"_s, u"Thống kê"_s})
                 t->addTab(s); // i18n-ok: dữ liệu thử
             return t;
         },
         [](QWidget* w, State s) {
             auto* t = static_cast<TabBar*>(w);
             switch (s) {
             case Hover: {
                 const QPointF at = t->tabRect(1).center();
                 QHoverEvent e(QEvent::HoverMove, at, t->mapToGlobal(at), QPointF(-1, -1));
                 QCoreApplication::sendEvent(t, &e);
                 hover(t);
                 break;
             }
             case Pressed: // nhấn là chọn tab ngay
                 t->setCurrentIndex(1);
                 break;
             case Focus:
                 focusByKeyboard(t);
                 break;
             case Disabled:
                 t->setEnabled(false);
                 break;
             case On:
                 return false; // tab đang mở có ở mọi ô
             case Locked:
                 t->setTabLocked(1, true);
                 break;
             case Error:
                 t->setTabError(1, true);
                 break;
             default:
                 break;
             }
             return true;
         }},
        {"slider", kCellW - 2 * kInset,
         [](QWidget* p) -> QWidget* {
             auto* s = new Slider(Qt::Horizontal, p);
             s->setRange(0, 100);
             s->setValue(40);
             return s;
         },
         [](QWidget* w, State s) {
             auto* sl = static_cast<Slider*>(w);
             switch (s) {
             case Hover:
                 hover(sl);
                 break;
             case Pressed:
                 sl->setSliderDown(true);
                 break;
             case Focus:
                 focusByKeyboard(sl);
                 break;
             case Disabled:
                 sl->setEnabled(false);
                 break;
             case On:
                 return false;
             case Locked:
                 sl->setLocked(true);
                 break;
             case Error:
                 sl->setError(true);
                 break;
             default:
                 break;
             }
             return true;
         }},
    };
}

QPoint cellOrigin(int row, int state)
{
    return {state * kCellW + kInset, row * kCellH};
}

// Màu điểm ảnh giữa mép trái của điều khiển trong ô (viền 2 px nằm trong mép).
QColor leftEdge(const QImage& sheet, int row, State s)
{
    const qreal dpr = sheet.devicePixelRatio();
    return sheet.pixelColor(int(cellOrigin(row, s).x() * dpr), int((row * kCellH + kCellH / 2) * dpr));
}

// const: operator[] lồng nhau được.
const QJsonObject probeJson()
{
    return probe::toJson();
}

bool near(const QColor& a, const QColor& b)
{
    return std::abs(a.red() - b.red()) <= 2 && std::abs(a.green() - b.green()) <= 2 &&
           std::abs(a.blue() - b.blue()) <= 2;
}

} // namespace

// Bố cục mẫu của tiêu chí 4: hàng nút, nhóm chọn, dải tab, danh sách, ô tìm; chuỗi qua tr() để vi-XL kéo dài.
class ControlsSample : public QWidget {
    Q_OBJECT
public:
    ControlsSample()
    {
        auto* col = new QVBoxLayout(this);
        auto* buttons = new QHBoxLayout;
        for (QWidget* w : std::initializer_list<QWidget*>{print, del, cancel, split, grid, unit})
            buttons->addWidget(w);
        unit->addSegment({});
        unit->addSegment({});
        col->addLayout(buttons);
        tabs->addTab({});
        tabs->addTab({});
        col->addWidget(tabs);
        col->addWidget(paper);
        col->addWidget(search);
        retranslate();
    }

    Button* print = new Button({}, Button::Kind::Primary);
    Button* del = new Button({}, Button::Kind::Danger);
    Button* cancel = new Button({}, Button::Kind::Ghost);
    SplitButton* split = new SplitButton;
    ToggleButton* grid = new ToggleButton;
    SegmentedGroup* unit = new SegmentedGroup;
    TabBar* tabs = new TabBar;
    ComboBox* paper = new ComboBox;
    SearchField* search = new SearchField;

protected:
    void changeEvent(QEvent* e) override
    {
        if (e->type() == QEvent::LanguageChange)
            retranslate();
        QWidget::changeEvent(e);
    }

private:
    void retranslate()
    {
        print->setText(tr("In"));
        del->setText(tr("Xóa 3 layout"));
        cancel->setText(tr("Hủy"));
        split->setText(tr("In hàng loạt"));
        grid->setText(tr("Lưới"));
        unit->segment(0)->setText(tr("Mét"));
        unit->segment(1)->setText(tr("Milimét"));
        tabs->setTabText(0, tr("Chung"));
        tabs->setTabText(1, tr("Thống kê"));
        search->setPlaceholderText(tr("Tìm layer"));
    }
};

class ControlsTest : public QObject {
    Q_OBJECT
    QTemporaryDir dir_;
    std::unique_ptr<settings::Store> store_;
    std::unique_ptr<UiExecutor> executor_;
    std::unique_ptr<ThemeManager> tm_;
    std::unique_ptr<IconProvider> icons_;
    std::unique_ptr<i18n::Translator> translator_;
    std::unique_ptr<QWidget> host_; // cửa sổ đang hiện, đang active: focus, popup, chuột

    bool showHost()
    {
        host_ = std::make_unique<QWidget>();
        host_->resize(kCellW * 2, kCellH * 2);
        host_->show();
        return QTest::qWaitForWindowActive(host_.get());
    }

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
        QApplication::setCursorFlashTime(0); // con trỏ ô nhập không nhấp nháy: ảnh tất định
        QVERIFY(showHost());
    }

    void init()
    {
        tm_->setTheme(Theme::Dark);
        tm_->setDensity(Density::Compact);
        host_->activateWindow(); // popup của test trước có thể đã lấy active
        QVERIFY(QTest::qWaitForWindowActive(host_.get()));
    }

    // Dọn cả khi test hỏng giữa chừng; xử lý hết đổi cửa sổ active còn trong hàng đợi (popup mở/đóng), không thì
    // nó đến lúc test sau đang mở popup và đóng popup đó.
    void cleanup()
    {
        if (QWidget* p = QApplication::activePopupWidget())
            p->close();
        delete host_->layout();
        qDeleteAll(host_->findChildren<QWidget*>(Qt::FindDirectChildrenOnly));
        QCoreApplication::processEvents();
    }

    void cleanupTestCase()
    {
        host_.reset();
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
    void states_data()
    {
        QTest::addColumn<bool>("light");
        QTest::addColumn<qreal>("dpr");
        for (bool light : {false, true})
            for (qreal dpr : {1.0, 1.5, 2.0})
                QTest::addRow("%s-%d", light ? "light" : "dark", qRound(dpr * 100)) << light << dpr;
    }

    void states()
    {
        QFETCH(bool, light);
        QFETCH(qreal, dpr);
        tm_->setTheme(light ? Theme::Light : Theme::Dark);
        QCoreApplication::processEvents(); // Qt 6.11 gửi ApplicationPaletteChange qua hàng đợi
        const QList<Row> all = rows();
        const QColor bg = tm_->color(tok::c_base);
        QImage sheet(QSize(kCellW * kStates, kCellH * int(all.size())) * dpr, QImage::Format_ARGB32_Premultiplied);
        sheet.setDevicePixelRatio(dpr);
        sheet.fill(bg);
        QPainter p(&sheet);
        // Con trỏ chuột ngoài cửa sổ: ô vừa hiện không thành "đang rê". Ô mở popup vẽ sau cùng: offscreen đổi cửa sổ
        // active qua hàng đợi, popup đã đóng có thể còn giữ active và làm mất focus của các ô sau.
        QCursor::setPos(host_->frameGeometry().bottomRight() + QPoint(kCellW, kCellH));
        const auto cell = [&](int r, State s) {
            std::unique_ptr<QWidget> w(all[r].make(host_.get()));
            const auto fit = [&] {
                const QSize hint = w->sizeHint();
                w->resize(all[r].width ? all[r].width : std::min(hint.width(), kCellW - 2 * kInset), hint.height());
            };
            fit();
            w->show();
            const bool applies = all[r].apply(w.get(), s);
            QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest); // khóa: nút rộng thêm ổ khóa
            fit();
            if (applies)
                w->render(&p, cellOrigin(r, s) + QPoint(0, (kCellH - w->height()) / 2), {}, QWidget::DrawChildren);
            if (QWidget* popup = QApplication::activePopupWidget()) {
                popup->close();
                QCoreApplication::processEvents();
                host_->activateWindow();
                QVERIFY(QTest::qWaitForWindowActive(host_.get()));
            }
            host_->setAttribute(Qt::WA_KeyboardFocusChange, false);
        };
        for (int r = 0; r < all.size(); ++r)
            for (int s = 0; s < kStates; ++s)
                if (all[r].popup != s)
                    cell(r, State(s));
        for (int r = 0; r < all.size(); ++r)
            if (all[r].popup != kStates)
                cell(r, all[r].popup);
        p.end();
        const QString name = u"sheet-%1-%2-%3.png"_s.arg(light ? u"light"_s : u"dark"_s)
                                 .arg(qRound(dpr * 100))
                                 .arg(QLatin1StringView(kPlatform));
        const QString out = qEnvironmentVariable("TRION_UPDATE_BASELINE");
        if (!out.isEmpty())
            QVERIFY(sheet.save(out + u'/' + name, "PNG", 0)); // nén cao nhất

        // Viền focus, lỗi: 2 px màu token ở mép (ảnh không phụ thuộc font).
        for (int r : {0, 3, 8, 9, 10}) { // button, ghost, dropdown, combobox, search
            QVERIFY2(near(leftEdge(sheet, r, Focus), tm_->color(tok::c_focus)), all[r].name);
            QVERIFY2(near(leftEdge(sheet, r, Error), tm_->color(tok::c_danger)), all[r].name);
        }
        QVERIFY(near(leftEdge(sheet, 7, Error), tm_->color(tok::c_danger))); // segmented
        if (!out.isEmpty())
            QSKIP("đã ghi baseline");

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

    // Tiêu chí 2.
    void comboFilter()
    {
        // 10.000 tên layer: mỗi 5 dòng một tên có "Lớp"/"LỚP" (2.000 dòng), một tên có "ĐƯỜNG" (2.000 dòng).
        const QStringList words = {u"Lớp tường"_s, u"KT-CỬA"_s, u"ĐƯỜNG trục"_s, u"LỚP hoàn thiện"_s, u"Kích thước"_s};
        QStringList names;
        for (int i = 0; i < 10000; ++i)
            names << u"%1 %2"_s.arg(words[i % 5]).arg(i, 5, 10, QChar(u'0'));
        auto* combo = new ComboBox(host_.get());
        combo->addItems(names); // i18n-ok: dữ liệu thử
        combo->resize(240, combo->sizeHint().height());
        combo->show();
        combo->setCurrentIndex(7);

        QElapsedTimer clock;
        clock.start();
        combo->showPopup();
        const double openMs = double(clock.nsecsElapsed()) / 1e6;
        QWidget* popup = QApplication::activePopupWidget();
        QVERIFY(popup && popup->parentWidget() == combo);
        auto* edit = popup->findChild<SearchField*>();
        auto* list = popup->findChild<QListView*>();
        QVERIFY(edit && list);
        QCOMPARE(list->model()->rowCount(), 10000);
        QCOMPARE(list->currentIndex().row(), 7); // mở ở mục đang chọn

        QTest::keyClicks(edit, u"lop"_s);
        QCOMPARE(list->model()->rowCount(), 4000);
        for (int i = 0; i < list->model()->rowCount(); i += 97)
            QVERIFY(list->model()->index(i, 0).data().toString().toLower().contains(u"lớp"_s));
        edit->clear();
        QTest::keyClicks(edit, u"duong"_s);
        QCOMPARE(list->model()->rowCount(), 2000);
        QVERIFY(list->model()->index(0, 0).data().toString().startsWith(u"ĐƯỜNG"_s));

        // Mỗi phím: lọc 10.000 khóa đã gấp, đặt lại danh sách, bố cục, vẽ phần nhìn thấy. Vòng tốt nhất của 5 vòng.
        const QString typed = u"lop hoan thien 01"_s;
        double best = 1e9;
        for (int round = 0; round < 5; ++round) {
            edit->clear();
            list->viewport()->repaint();
            double worst = 0;
            for (QChar ch : typed) {
                QElapsedTimer key;
                key.start();
                QTest::keyClick(edit, char(ch.unicode()));
                list->viewport()->repaint();
                worst = std::max(worst, double(key.nsecsElapsed()) / 1e6);
            }
            best = std::min(best, worst);
        }
        QCOMPARE(list->model()->rowCount(), 200); // "LỚP hoàn thiện 01xxx"
        qInfo("ComboBox 10.000 mục: mở popup %.2f ms; phím chậm nhất của vòng tốt nhất %.2f ms (ngưỡng %.0f ms)",
              openMs, best, kKeyBudgetMs);
        QVERIFY2(!kTimed || best <= kKeyBudgetMs, qPrintable(QString::number(best)));

        // Đo trong ứng dụng (UiProbe): phím tới cửa sổ popup đến hết khung kế tiếp. Chỉ báo số (máy chung, tải nặng);
        // probe bỏ mẫu khi vòng sự kiện rảnh trước khung (khung offscreen đến theo timer), nên không đòi đủ mẫu.
        probe::reset();
        probe::enable();
        edit->clear();
        for (QChar ch : typed) {
            QTest::keyClick(popup->windowHandle(), char(ch.unicode()));
            QTest::qWait(20);
        }
        QCOMPARE(edit->text(), typed);
        const QJsonObject ops = probeJson()["ops"].toObject();
        probe::disable();
        qInfo("UiProbe: typing p50 %.2f / p99 %.2f ms (%d mẫu); autocomplete p50 %.2f / p99 %.2f ms (%d mẫu)",
              ops["typing"]["p50_ms"].toDouble(), ops["typing"]["p99_ms"].toDouble(), ops["typing"]["count"].toInt(),
              ops["autocomplete"]["p50_ms"].toDouble(), ops["autocomplete"]["p99_ms"].toDouble(),
              ops["autocomplete"]["count"].toInt());
        QVERIFY(ops["autocomplete"]["count"].toInt() >= 1);

        // Model đổi khi popup đang mở: lọc lại ở lượt vòng sự kiện kế, không chờ phím (integration nhóm 3 bắt lỗi
        // danh sách giữ số dòng nguồn cũ tới phím kế tiếp).
        edit->clear();
        QTest::keyClicks(edit, u"zz"_s);
        QVERIFY(popup->isVisible());
        QCOMPARE(list->model()->rowCount(), 0);
        QVERIFY(popup->findChild<QLabel*>()->isVisible()); // "Không có mục khớp"
        combo->setItemText(3, u"Lớp ZZ mới"_s);            // i18n-ok: dữ liệu thử
        QTRY_COMPARE(list->model()->rowCount(), 1);
        QVERIFY(list->isVisible());
        combo->insertItem(0, u"ZZ đầu"_s); // i18n-ok: dữ liệu thử; mục trên dời xuống dòng nguồn 4
        QTRY_COMPARE(list->model()->rowCount(), 2);
        QCOMPARE(list->model()->index(1, 0).data().toString(), u"Lớp ZZ mới"_s);
        QTest::keyClick(edit, Qt::Key_Down);
        QTest::keyClick(edit, Qt::Key_Return);
        QVERIFY(!popup->isVisible());
        QCOMPARE(combo->currentIndex(), 4);
    }

    // Tiêu chí 3.
    void keyboardActivate()
    {
        auto* b = new Button(u"Lưu"_s, Button::Kind::Normal, host_.get());
        auto* t = new ToggleButton(u"Lưới"_s, Button::Kind::Normal, host_.get());
        b->show();
        t->show();
        QSignalSpy clicked(b, &QAbstractButton::clicked);
        b->setFocus();
        QTest::keyClick(b, Qt::Key_Space);
        QTest::keyClick(b, Qt::Key_Return);
        QTest::keyClick(b, Qt::Key_Enter, Qt::KeypadModifier);
        QCOMPARE(clicked.size(), 3);
        QTest::keyClick(b, Qt::Key_Return, Qt::ControlModifier); // tổ hợp khác: không kích hoạt
        QCOMPARE(clicked.size(), 3);

        t->setFocus();
        QTest::keyClick(t, Qt::Key_Space);
        QVERIFY(t->isChecked());
        QTest::keyClick(t, Qt::Key_Return);
        QVERIFY(!t->isChecked());
        delete b;
        delete t;
    }

    void keyboardGroups()
    {
        auto* before = new QPushButton(host_.get());
        auto* g = new SegmentedGroup(host_.get());
        for (const QString& s : {u"mm"_s, u"m"_s, u"in"_s})
            g->addSegment(s);
        auto* after = new QPushButton(host_.get()); // thứ tự focus theo thứ tự tạo
        auto* tabs = new TabBar(host_.get());
        for (const QString& s : {u"Chung"_s, u"Tóm tắt"_s, u"Thống kê"_s})
            tabs->addTab(s); // i18n-ok: dữ liệu thử
        auto* row = new QHBoxLayout(host_.get());
        for (QWidget* w : std::initializer_list<QWidget*>{before, g, after, tabs}) {
            row->addWidget(w);
            w->show();
        }
        QSignalSpy changed(g, &SegmentedGroup::currentIndexChanged);

        // Tab vào nhóm dừng ở đoạn đang bật, mũi tên chuyển và chọn, Tab tiếp ra khỏi nhóm.
        before->setFocus();
        QTest::keyClick(before, Qt::Key_Tab);
        QCOMPARE(QApplication::focusWidget(), g->segment(0));
        QTest::keyClick(g->segment(0), Qt::Key_Right);
        QCOMPARE(g->currentIndex(), 1);
        QCOMPARE(QApplication::focusWidget(), g->segment(1));
        QTest::keyClick(g->segment(1), Qt::Key_Right);
        QTest::keyClick(g->segment(2), Qt::Key_Left);
        QCOMPARE(g->currentIndex(), 1);
        QCOMPARE(changed.size(), 3);
        QTest::keyClick(g->segment(1), Qt::Key_Tab);
        QCOMPARE(QApplication::focusWidget(), after);
        QTest::keyClick(after, Qt::Key_Backtab);
        QCOMPARE(QApplication::focusWidget(), g->segment(1));

        tabs->setFocus();
        QTest::keyClick(tabs, Qt::Key_Right);
        QTest::keyClick(tabs, Qt::Key_Right);
        QCOMPARE(tabs->currentIndex(), 2);
        QTest::keyClick(tabs, Qt::Key_Left);
        QCOMPARE(tabs->currentIndex(), 1);
    }

    void keyboardPopups()
    {
        const auto popupKey = [](int key, Qt::KeyboardModifiers mods = {}) {
            QTest::keyClick(QApplication::activePopupWidget()->windowHandle(), Qt::Key(key), mods);
        };
        for (bool filter : {false, true}) {
            DropDown* d = filter ? new ComboBox(host_.get()) : new DropDown(host_.get());
            d->addItems({u"A1"_s, u"A2"_s, u"A3"_s}); // i18n-ok: dữ liệu thử
            d->setCurrentIndex(1);
            d->show();
            d->setFocus();
            QSignalSpy activated(d, &QComboBox::activated);
            QTest::keyClick(d, Qt::Key_Down, Qt::AltModifier);
            QTRY_VERIFY(QApplication::activePopupWidget());
            QCOMPARE(QApplication::activePopupWidget()->parentWidget(), d);
            popupKey(Qt::Key_Down);
            popupKey(Qt::Key_Escape); // đóng, không đổi giá trị
            QTRY_VERIFY(!QApplication::activePopupWidget());
            QCOMPARE(d->currentIndex(), 1);
            QCOMPARE(activated.size(), 0);

            QTest::keyClick(d, Qt::Key_Down, Qt::AltModifier);
            QTRY_VERIFY(QApplication::activePopupWidget());
            popupKey(Qt::Key_Down);
            popupKey(Qt::Key_Return);
            QTRY_VERIFY(!QApplication::activePopupWidget());
            QCOMPARE(d->currentIndex(), 2);
            QCOMPARE(activated.size(), 1);
            delete d;
        }

        auto* menu = new QMenu(host_.get());
        QAction* batch = menu->addAction(u"In hàng loạt"_s); // i18n-ok: dữ liệu thử
        QSignalSpy triggered(batch, &QAction::triggered);
        auto* split = new SplitButton(u"In"_s, menu, host_.get());
        split->show();
        split->setFocus();
        QSignalSpy clicked(split, &QAbstractButton::clicked);
        QTest::keyClick(split, Qt::Key_Down, Qt::AltModifier);
        QTRY_COMPARE(QApplication::activePopupWidget(), menu);
        popupKey(Qt::Key_Escape);
        QTRY_VERIFY(!menu->isVisible());
        QCOMPARE(triggered.size(), 0);
        QCOMPARE(clicked.size(), 0);
        QTest::keyClick(split, Qt::Key_Space);
        QCOMPARE(clicked.size(), 1);
        delete split;
        delete menu;

        auto* search = new SearchField(host_.get());
        search->show();
        search->setFocus();
        QTest::keyClicks(search, u"kt"_s);
        QKeyEvent esc(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QApplication::sendEvent(search, &esc);
        QVERIFY(esc.isAccepted());
        QCOMPARE(search->text(), QString());
        QKeyEvent esc2(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QApplication::sendEvent(search, &esc2);
        QVERIFY(!esc2.isAccepted()); // rỗng: Esc về cho hộp thoại
        delete search;
    }

    // Tiêu chí 4.
    void heights()
    {
        for (Density density : {Density::Compact, Density::Comfortable}) {
            tm_->setDensity(density);
            const bool comfy = density == Density::Comfortable;
            const int ctl = comfy ? 26 : 24, btn = comfy ? 23 : 21, large = 28, tab = (comfy ? 30 : 28) + 2;
            QCOMPARE(qRound(tm_->px(tok::h_ctl)), ctl); // giá trị của tokens.css
            Button normal(u"In"_s), small(u"In"_s), big(u"In"_s), icon({}, Button::Kind::Icon);
            small.setControlSize(Button::Size::Small);
            big.setControlSize(Button::Size::Large);
            icon.setIconName(u"osnap"_s);
            QCOMPARE(normal.sizeHint().height(), ctl);
            QCOMPARE(small.sizeHint().height(), btn);
            QCOMPARE(big.sizeHint().height(), large);
            QCOMPARE(icon.sizeHint(), QSize(ctl, ctl));
            QCOMPARE(SplitButton(u"In"_s).sizeHint().height(), ctl);
            QCOMPARE(ToggleButton(u"Lưới"_s).sizeHint().height(), ctl);
            SegmentedGroup g;
            g.addSegment(u"mm"_s);
            QCOMPARE(g.sizeHint().height(), ctl);
            DropDown d;
            ComboBox c;
            QCOMPARE(d.sizeHint().height(), ctl);
            QCOMPARE(c.sizeHint().height(), ctl);
            QCOMPARE(SearchField().sizeHint().height(), ctl);
            QCOMPARE(Slider().sizeHint().height(), ctl);
            TabBar t;
            t.addTab(u"Chung"_s); // i18n-ok: dữ liệu thử
            QCOMPARE(t.sizeHint().height(), tab);
        }
    }

    void pseudoNoTruncation()
    {
        ControlsSample w;
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        const auto report = [&w] {
            QStringList out;
            for (const i18n::Truncation& t : i18n::findTruncations(&w))
                out << t.toString();
            return out.join(u"; "_s);
        };
        QCOMPARE(report(), QString());
        const QString vi = w.print->text();
        QVERIFY(translator_->setLanguage(i18n::kPseudoLanguage.toString()));
        QTRY_VERIFY(w.print->text().size() >= vi.size() + 2);
        QTRY_COMPARE(report(), QString());
        for (Density d : {Density::Comfortable, Density::Compact}) {
            tm_->setDensity(d);
            QTRY_COMPARE(report(), QString());
        }
        QVERIFY(translator_->setLanguage(u"vi"_s));
        QTRY_COMPARE(w.print->text(), vi);
    }

    // Khóa có hình riêng, không chỉ đổi màu: nút chữ, danh sách thêm ô ổ khóa (rộng thêm --ic + --sp-3); nút có
    // biểu tượng của bộ dùng bản có huy hiệu; tab mang biểu tượng; ô tìm thay kính lúp bằng ổ khóa. Slider: ảnh
    // baseline.
    void lockedHasShape()
    {
        const int slot = qRound(tm_->px(tok::ic)) + qRound(tm_->px(tok::sp_3));
        for (Button::Kind kind : {Button::Kind::Normal, Button::Kind::Primary, Button::Kind::Danger,
                                  Button::Kind::Ghost, Button::Kind::Segment}) {
            Button plain(u"Hủy"_s, kind), locked(u"Hủy"_s, kind);
            locked.setLocked(true);
            QCOMPARE(locked.sizeHint().width(), plain.sizeHint().width() + slot);
        }
        const auto alpha = [](const QIcon& icon) {
            return icon.pixmap(QSize(16, 16), 1.0).toImage().convertToFormat(QImage::Format_Alpha8);
        };
        Button icon({}, Button::Kind::Icon), lockedIcon({}, Button::Kind::Icon);
        icon.setIconName(u"osnap"_s);
        lockedIcon.setIconName(u"osnap"_s);
        lockedIcon.setLocked(true);
        QCOMPARE(lockedIcon.sizeHint(), icon.sizeHint());
        QVERIFY(alpha(lockedIcon.icon()) != alpha(icon.icon())); // huy hiệu ổ khóa
        DropDown plainDrop, lockedDrop;
        lockedDrop.setLocked(true);
        QCOMPARE(lockedDrop.sizeHint().width(), plainDrop.sizeHint().width() + slot);
        TabBar tabs;
        tabs.addTab(u"Chung"_s); // i18n-ok: dữ liệu thử
        tabs.setTabLocked(0, true);
        QCOMPARE(alpha(tabs.tabIcon(0)), alpha(IconProvider::icon(u"lock"_s)));
        SearchField search;
        const QImage glass = alpha(search.actions().constFirst()->icon());
        search.setLocked(true);
        QVERIFY(search.isReadOnly());
        QCOMPARE(alpha(search.actions().constFirst()->icon()), alpha(IconProvider::icon(u"lock"_s)));
        QVERIFY(glass != alpha(IconProvider::icon(u"lock"_s)));
    }

    void locked()
    {
        auto* t = new ToggleButton(u"Lưới"_s, Button::Kind::Normal, host_.get());
        t->setLocked(true);
        t->show();
        QSignalSpy clicked(t, &QAbstractButton::clicked);
        QTest::mouseClick(t, Qt::LeftButton);
        t->setFocus();
        QTest::keyClick(t, Qt::Key_Space);
        QVERIFY(!t->isChecked());
        QCOMPARE(clicked.size(), 2); // UI0-011 mở lời mời nâng cấp từ đây

        auto* g = new SegmentedGroup(host_.get());
        g->addSegment(u"mm"_s);
        g->addSegment(u"m"_s)->setLocked(true);
        g->show();
        QTest::mouseClick(g->segment(1), Qt::LeftButton);
        QCOMPARE(g->currentIndex(), 0);

        for (bool filter : {false, true}) {
            DropDown* d = filter ? new ComboBox(host_.get()) : new DropDown(host_.get());
            d->addItems({u"A1"_s, u"A2"_s}); // i18n-ok: dữ liệu thử
            d->setLocked(true);
            d->show();
            d->setFocus();
            d->showPopup();
            QTest::keyClick(d, Qt::Key_Down, Qt::AltModifier);
            QTest::keyClick(d, Qt::Key_Down);
            QVERIFY(!QApplication::activePopupWidget());
            QCOMPARE(d->currentIndex(), 0);
            delete d;
        }

        auto* s = new Slider(Qt::Horizontal, host_.get());
        s->setRange(0, 100);
        s->setValue(50);
        s->setLocked(true);
        s->resize(120, s->sizeHint().height());
        s->show();
        s->setFocus();
        QTest::keyClick(s, Qt::Key_Right);
        QTest::mouseClick(s, Qt::LeftButton, {}, QPoint(110, s->height() / 2));
        QCOMPARE(s->value(), 50);
        s->setLocked(false);
        QTest::keyClick(s, Qt::Key_Right);
        QCOMPARE(s->value(), 51);

        auto* f = new SearchField(host_.get());
        f->setLocked(true);
        f->show();
        f->setFocus();
        QTest::keyClicks(f, u"kt"_s);
        QCOMPARE(f->text(), QString());
    }

    void splitButton()
    {
        auto* menu = new QMenu(host_.get());
        menu->addAction(u"In hàng loạt"_s); // i18n-ok: dữ liệu thử
        auto* split = new SplitButton(u"In"_s, menu, host_.get());
        split->resize(split->sizeHint());
        split->show();
        QSignalSpy clicked(split, &QAbstractButton::clicked);
        const int zone = qRound(tm_->px(tok::ic));
        QCOMPARE(split->sizeHint().width(), Button(u"In"_s).sizeHint().width() + zone);
        QTest::mouseClick(split, Qt::LeftButton, {}, QPoint(split->width() - zone / 2, split->height() / 2));
        QTRY_COMPARE(QApplication::activePopupWidget(), menu);
        QCOMPARE(clicked.size(), 0);
        menu->close();
        QTest::mouseClick(split, Qt::LeftButton, {}, QPoint(zone / 2, split->height() / 2));
        QCOMPARE(clicked.size(), 1);
        QVERIFY(!menu->isVisible());
        delete split;
        delete menu;
    }
};

QTEST_MAIN(ControlsTest)
#include "controls_test.moc"
