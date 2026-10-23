// UIC-017 · Bộ chọn màu (ColorPickerDialog.h). Theo tiêu chí nghiệm thu:
//   1 aciTable: 255/255 ô (bảng 10–249, hàng 1–9, hàng xám) đúng RGB của RGN-002 (điểm ảnh giữa ô), chuỗi rê
//     "Chỉ số màu N (tên) · RGB r, g, b" cả 255 ô; nhấp chọn, nhấp đúp chọn và đóng.
//   2 conversions, trueColorTab: HSL ↔ RGB ↔ hex lệch ≤ 1 mỗi kênh (200.000 RGB ngẫu nhiên seed cố định + biên, 100.000
//     HSL nguyên + biên); trong hộp: đổi mô hình qua lại không trôi, ô số có chặn trên, mã hex, vùng chọn, thanh sáng.
//   3 colorField: ô "Màu" nhận số, tên vi/en, ByLayer, ByBlock, #RRGGBB; sai: viền --c-danger, câu lỗi, Enter dừng ở ô,
//     Esc trả chữ cũ.
//   4 pickFromDrawing: tới chỗ nối INT-009 (pickRequested → finishPick/cancelPick): ByLayer giữ ByLayer, màu cụ
//     thể đúng, Esc (cancelPick) không đổi; nút ⌖ chỉ bật khi có nơi nối. Bấm đối tượng thật: Hoãn (cần INT-009).
//   5 colorBook: tab Color Book "Có sau v1.0"; màu Color Book hiện tên ở ô, đúng RGB ở ô so sánh.
//   6 openTime (CPU luồng UI ≤ 150 ms, kiểm ở release), keyboard (mũi tên đi và chọn, Home/End, Enter chọn và đóng).
//   DoD-U: recentColors (8 màu theo hồ sơ, rỗng), trueColorOnly (UID-020), askHook (móc COLOR), pseudoNoTruncation
//   (vi-XL mọi tab), accessibility (a11y::checkNames + checkFocus, 0 vi phạm ở tab ACI và True Color).
#include "app/dialogs/pickers/ColorPickerDialog.h"
#include "app/dialogs/pickers/ColorWidgets.h"

#include "app/core/a11y/AccessibilityAudit.h"
#include "app/core/i18n/Translator.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/core/UiExecutor.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/generated/Tokens.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/message/Messages.h"
#include "app/widgets/numeric/NumericField.h"
#include "app/widgets/tooltip/RichTooltip.h"
#include "core/base/sched/Scheduler.h"
#include "tests/timing.h"

#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPointer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>
#include <array>
#include <functional>
#include <memory>
#include <random>
#include <set>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;
using namespace trion;
using ui::Color;
using ui::ColorPickerDialog;
using ui::SwatchGrid;
using Mode = ColorPickerDialog::Mode;
using Tab = ColorPickerDialog::Tab;
using regen::Rgb;

namespace trion::regen {
char* toString(const Rgb& c) // QCOMPARE in giá trị khi sai (ADL)
{
    return QTest::toString(u"%1,%2,%3"_s.arg(c.r).arg(c.g).arg(c.b));
}
} // namespace trion::regen

namespace trion::ui {
char* toString(const Color& c)
{
    return QTest::toString(u"%1 [%2]"_s.arg(c.label(), hexName(c.rgb())));
}
} // namespace trion::ui

namespace {

constexpr int kNoise = 2;

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

// QTest::qWaitForWindowActive đúng ngay khi cửa sổ cha đang active: chờ chính widget active.
bool waitActive(QWidget* w)
{
    w->activateWindow();
    return spinUntil([w] { return w->isActiveWindow(); });
}

double cpuMs(std::chrono::nanoseconds d)
{
    return d.count() < 0 ? qQNaN() : std::chrono::duration<double, std::milli>(d).count();
}

bool near(const QColor& a, const QColor& b)
{
    return std::abs(a.red() - b.red()) <= kNoise && std::abs(a.green() - b.green()) <= kNoise &&
           std::abs(a.blue() - b.blue()) <= kNoise;
}

int channelDiff(Rgb a, Rgb b)
{
    return std::max({std::abs(a.r - b.r), std::abs(a.g - b.g), std::abs(a.b - b.b)});
}

QColor qcolor(Rgb c)
{
    return QColor(c.r, c.g, c.b);
}

QColor pixel(QWidget* w, QPoint p)
{
    return w->grab().toImage().pixelColor(p);
}

// Mép trái giữa chiều cao: viền 2 px của ô (lỗi, focus).
QColor edge(QWidget* w)
{
    return pixel(w, QPoint(1, w->height() / 2));
}

QString truncations(QWidget* root)
{
    QStringList out;
    for (const i18n::Truncation& t : i18n::findTruncations(root))
        out << t.toString();
    return out.join(u"; "_s);
}

void hoverAt(QWidget* w, QPoint p)
{
    QMouseEvent e(QEvent::MouseMove, QPointF(p), QPointF(w->mapToGlobal(p)), Qt::NoButton, Qt::NoButton,
                  Qt::NoModifier);
    QCoreApplication::sendEvent(w, &e);
}

void doubleClick(QWidget* w, QPoint p)
{
    const QPointF g(w->mapToGlobal(p));
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(p), g, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(w, &press);
    QMouseEvent dbl(QEvent::MouseButtonDblClick, QPointF(p), g, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(w, &dbl);
}

// Chuỗi rê mong đợi (07.2 UI-C06, prototype col-hov).
QString hoverText(int n)
{
    // i18n-ok: tên ACI 1–7 mong đợi ở tiếng Việt
    static const QString kNames[] = {u"Đỏ"_s,         u"Vàng"_s, u"Xanh lá"_s, u"Xanh lơ"_s,
                                     u"Xanh dương"_s, u"Tím"_s,  u"Trắng"_s};
    const Rgb c = regen::aciRgb(n);
    const QString rgb = u"%1, %2, %3"_s.arg(c.r).arg(c.g).arg(c.b);
    if (n <= 7)
        return u"Chỉ số màu %1 (%2) · RGB %3"_s.arg(QString::number(n), kNames[n - 1], rgb); // i18n-ok: chữ mong đợi
    return u"Chỉ số màu %1 · RGB %2"_s.arg(QString::number(n), rgb); // i18n-ok: chữ mong đợi
}

} // namespace

class ColorPickerTest : public QObject {
    Q_OBJECT

    QTemporaryDir dir_;
    std::unique_ptr<settings::Store> store_;
    std::unique_ptr<ui::UiExecutor> executor_;
    std::unique_ptr<theme::ThemeManager> tm_;
    std::unique_ptr<theme::IconProvider> icons_;
    std::unique_ptr<i18n::Translator> translator_;
    std::unique_ptr<ui::RichTooltip> tips_;
    std::unique_ptr<QWidget> window_;

    std::unique_ptr<ColorPickerDialog> open(const Color& c, Mode mode = Mode::Full)
    {
        auto d = std::make_unique<ColorPickerDialog>(c, mode, window_.get());
        d->open();
        if (!QTest::qWaitForWindowExposed(d.get()) || !waitActive(d.get()))
            return nullptr;
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        return d;
    }
    static SwatchGrid* grid(QWidget* d, const char* name) { return d->findChild<SwatchGrid*>(QLatin1StringView(name)); }
    static QLineEdit* field(QWidget* d) { return d->findChild<QLineEdit*>(u"color"_s); }
    static QLineEdit* hex(QWidget* d) { return d->findChild<QLineEdit*>(u"hex"_s); }
    static ui::NumericField* channel(QWidget* d, int i)
    {
        return d->findChild<ui::NumericField*>(u"channel%1"_s.arg(i));
    }
    static QLineEdit* channelEdit(QWidget* d, int i) { return channel(d, i)->findChild<QLineEdit*>(); }
    static void setModel(QWidget* d, int m) { d->findChild<ui::SegmentedGroup*>(u"model"_s)->segment(m)->click(); }
    static std::array<int, 3> channels(QWidget* d)
    {
        return {qRound(channel(d, 0)->value()), qRound(channel(d, 1)->value()), qRound(channel(d, 2)->value())};
    }
    // Gõ vào ô rồi rời ô (nhận như Enter nhưng không tới nút xác nhận).
    static void typeAndLeave(QLineEdit* e, const QString& text, QWidget* next)
    {
        e->setFocus();
        e->selectAll();
        if (std::all_of(text.begin(), text.end(), [](QChar c) { return c.unicode() < 128; }))
            QTest::keyClicks(e, text);
        else
            e->insert(text); // QTest::keyClicks chỉ nhận ASCII: chữ có dấu chèn như bộ gõ
        next->setFocus();
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
        tips_ = std::make_unique<ui::RichTooltip>();
        window_ = std::make_unique<QWidget>();
        window_->resize(1366, 768);
        window_->show();
        QVERIFY(QTest::qWaitForWindowExposed(window_.get()));
    }

    void init() { store_->reset(ui::kRecentColors); }

    void cleanupTestCase()
    {
        window_.reset();
        tips_.reset();
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
    void aciTable()
    {
        auto d = open(Color::byLayer());
        QVERIFY(d);
        QCOMPARE(d->tab(), Tab::Index);
        auto* hover = d->findChild<QLabel*>(u"hover"_s);
        std::set<int> seen;
        int wrongPixels = 0, wrongHover = 0;
        for (const char* name : {"aci", "standard", "grays"}) {
            SwatchGrid* g = grid(d.get(), name);
            QVERIFY(g && g->isVisible());
            const QImage img = g->grab().toImage();
            for (int i = 0; i < g->count(); ++i) {
                const int n = g->colorAt(i).index();
                seen.insert(n);
                const QPoint c = g->cellRect(i).center();
                if (img.pixelColor(c) != qcolor(regen::aciRgb(n))) {
                    ++wrongPixels;
                    qWarning("ACI %d: ô vẽ %s", n, qPrintable(img.pixelColor(c).name()));
                }
                hoverAt(g, c);
                if (hover->text() != hoverText(n)) {
                    ++wrongHover;
                    qWarning("ACI %d: rê «%s»", n, qPrintable(hover->text()));
                }
            }
        }
        QCOMPARE(int(seen.size()), 255);
        QCOMPARE(*seen.begin(), 1);
        QCOMPARE(*seen.rbegin(), 255);
        QCOMPARE(wrongPixels, 0);
        QCOMPARE(wrongHover, 0);
        QCOMPARE(hoverText(1), u"Chỉ số màu 1 (Đỏ) · RGB 255, 0, 0"_s);  // i18n-ok: chữ mong đợi
        QCOMPARE(hoverText(140), u"Chỉ số màu 140 · RGB 0, 191, 255"_s); // i18n-ok: chữ mong đợi
        // Bảng: cột là sắc độ (10, 20 … 240), hàng là sắc thái: ô 0 = 10, ô 1 = 20, ô 24 = 11.
        SwatchGrid* aci = grid(d.get(), "aci");
        QCOMPARE(aci->colorAt(1), Color::aci(20));
        QCOMPARE(aci->colorAt(24), Color::aci(11));
        QVERIFY(aci->cellRect(1).width() >= 12); // giãn theo bề ngang hộp (24 cột 1fr)

        // Nhấp chọn (ô so sánh mới đổi), nhấp đúp chọn và đóng.
        QSignalSpy accepted(d.get(), &ColorPickerDialog::colorAccepted);
        int cell = -1;
        for (int i = 0; i < aci->count(); ++i)
            if (aci->colorAt(i) == Color::aci(140))
                cell = i;
        QVERIFY(cell >= 0);
        QTest::mouseClick(aci, Qt::LeftButton, {}, aci->cellRect(cell).center());
        QCOMPARE(d->color(), Color::aci(140));
        auto* compare = d->findChild<ui::ColorCompare*>(u"compare"_s);
        QVERIFY(near(pixel(compare, compare->afterRect().center()), qcolor(regen::aciRgb(140))));
        QCOMPARE(field(d.get())->text(), Color::aci(140).label());
        QCOMPARE(accepted.size(), 0);
        doubleClick(aci, aci->cellRect(cell + 1).center());
        QCOMPARE(accepted.size(), 1);
        QCOMPARE(accepted[0][0].value<Color>(), aci->colorAt(cell + 1));
        QCOMPARE(d->result(), int(QDialog::Accepted));
        QVERIFY(!d->isVisible());
    }

    // Tiêu chí 2, hàm thuần: HSL ↔ RGB ↔ hex.
    void conversions()
    {
        std::vector<Rgb> rgbs;
        constexpr std::uint8_t kEdge[] = {0, 1, 2, 127, 128, 254, 255};
        for (const auto r : kEdge)
            for (const auto g : kEdge)
                for (const auto b : kEdge)
                    rgbs.push_back({r, g, b});
        for (int v = 0; v < 256; ++v)
            rgbs.push_back({std::uint8_t(v), std::uint8_t(v), std::uint8_t(v)});
        std::mt19937 rng(20261023);
        std::uniform_int_distribution<int> byte(0, 255);
        for (int i = 0; i < 200000; ++i)
            rgbs.push_back({std::uint8_t(byte(rng)), std::uint8_t(byte(rng)), std::uint8_t(byte(rng))});
        int worstHsl = 0, worstHex = 0;
        for (const Rgb c : rgbs) {
            worstHsl = std::max(worstHsl, channelDiff(ui::toRgb(ui::toHsl(c)), c));
            const std::optional<Rgb> back = ui::parseHex(ui::hexName(c));
            worstHex = std::max(worstHex, back ? channelDiff(*back, c) : 256);
        }
        // HSL như người dùng gõ (độ, %, % nguyên): rgb → hsl → rgb không trôi.
        std::vector<ui::Hsl> hsls;
        for (const int h : {0, 1, 59, 60, 179, 180, 359, 360})
            for (const int s : {0, 1, 50, 99, 100})
                for (const int l : {0, 1, 50, 99, 100})
                    hsls.push_back({double(h), s / 100.0, l / 100.0});
        std::uniform_int_distribution<int> hue(0, 360), percent(0, 100);
        for (int i = 0; i < 100000; ++i)
            hsls.push_back({double(hue(rng)), percent(rng) / 100.0, percent(rng) / 100.0});
        int worstTyped = 0;
        for (const ui::Hsl& h : hsls) {
            const Rgb c = ui::toRgb(h);
            worstTyped = std::max(worstTyped, channelDiff(ui::toRgb(ui::toHsl(c)), c));
        }
        qInfo("lệch lớn nhất mỗi kênh: rgb→hsl→rgb %d, rgb→hex→rgb %d, hsl gõ→rgb→hsl→rgb %d (%zu + %zu mẫu)", worstHsl,
              worstHex, worstTyped, rgbs.size(), hsls.size());
        QVERIFY(worstHsl <= 1);
        QVERIFY(worstHex <= 1);
        QVERIFY(worstTyped <= 1);
        // Biên.
        QCOMPARE(ui::toRgb({360, 1, 0.5}), (Rgb{255, 0, 0}));
        QCOMPARE(ui::toRgb({0, 1, 0.5}), (Rgb{255, 0, 0}));
        QCOMPARE(ui::toRgb({120, 1, 0.5}), (Rgb{0, 255, 0}));
        QCOMPARE(ui::toRgb({240, 1, 0.5}), (Rgb{0, 0, 255}));
        QCOMPARE(ui::toRgb({200, 0.7, 0}), (Rgb{0, 0, 0}));
        QCOMPARE(ui::toRgb({200, 0.7, 1}), (Rgb{255, 255, 255}));
        QCOMPARE(ui::toHsl({128, 128, 128}).s, 0.0);
        QCOMPARE(ui::hexName({0x3C, 0xC8, 0xC0}), u"#3CC8C0"_s);
        QCOMPARE(ui::parseHex(u" 3cc8c0 "), std::optional<Rgb>(Rgb{0x3C, 0xC8, 0xC0}));
        for (const QStringView bad : {u"#3CC8C", u"#3CC8C00", u"#GG0000", u"+12345", u"#-12345", u"", u"#"})
            QVERIFY2(!ui::parseHex(bad), qPrintable(bad.toString()));
    }

    // Tiêu chí 2, trong hộp: mô hình HSL/RGB qua lại không trôi; ô số chặn trên; mã hex; vùng chọn; thanh sáng.
    void trueColorTab()
    {
        const Rgb start{10, 200, 37};
        auto d = open(Color::rgb(start));
        QVERIFY(d);
        QCOMPARE(d->tab(), Tab::TrueColor);
        const ui::Hsl h0 = ui::toHsl(start);
        const std::array<int, 3> hslShown{qRound(h0.h), qRound(h0.s * 100), qRound(h0.l * 100)};
        QCOMPARE(channels(d.get()), hslShown);
        QCOMPARE(hex(d.get())->text(), u"#0AC825"_s);
        setModel(d.get(), 1);
        QCOMPARE(channels(d.get()), (std::array<int, 3>{10, 200, 37}));
        setModel(d.get(), 0);
        QCOMPARE(channels(d.get()), hslShown);
        QCOMPARE(d->color(), Color::rgb(start)); // đổi mô hình không đổi màu

        // Gõ sắc độ: S, L giữ số thực đang có.
        setModel(d.get(), 0);
        typeAndLeave(channelEdit(d.get(), 0), u"200"_s, hex(d.get()));
        const Rgb typed = ui::toRgb({200, h0.s, h0.l});
        QCOMPARE(d->color(), Color::rgb(typed));
        QCOMPARE(hex(d.get())->text(), ui::hexName(typed));
        setModel(d.get(), 1);
        QCOMPARE(channels(d.get()), (std::array<int, 3>{typed.r, typed.g, typed.b}));
        setModel(d.get(), 0);
        QCOMPARE(channels(d.get()), (std::array<int, 3>{200, hslShown[1], hslShown[2]}));

        // Mã hex → RGB đúng từng đơn vị; ô RGB → hex.
        setModel(d.get(), 1);
        typeAndLeave(hex(d.get()), u"#3cc8c0"_s, channelEdit(d.get(), 0));
        QCOMPARE(d->color(), Color::rgb({60, 200, 192}));
        QCOMPARE(channels(d.get()), (std::array<int, 3>{60, 200, 192}));
        QCOMPARE(hex(d.get())->text(), u"#3CC8C0"_s);
        typeAndLeave(channelEdit(d.get(), 0), u"61"_s, hex(d.get()));
        QCOMPARE(d->color(), Color::rgb({61, 200, 192}));
        QCOMPARE(hex(d.get())->text(), u"#3DC8C0"_s);
        // Chặn trên (NumericField::setUpperBound): 300 sai, màu không đổi.
        typeAndLeave(channelEdit(d.get(), 1), u"300"_s, hex(d.get()));
        QVERIFY(channel(d.get(), 1)->hasError());
        QCOMPARE(channel(d.get(), 1)->errorMessage(), u"Xanh lá không được lớn hơn 255."_s); // i18n-ok: chữ mong đợi
        QCOMPARE(d->color(), Color::rgb({61, 200, 192}));
        // Mã hex sai: viền lỗi, câu lỗi, màu không đổi.
        typeAndLeave(hex(d.get()), u"#12345"_s, channelEdit(d.get(), 0));
        QVERIFY(near(edge(hex(d.get())), tm_->color(tok::c_danger)));
        QCOMPARE(d->color(), Color::rgb({61, 200, 192}));
        QTest::keyClick(hex(d.get()), Qt::Key_Escape); // trả chữ cũ
        QCOMPARE(hex(d.get())->text(), u"#3DC8C0"_s);
        QVERIFY(d->isVisible());

        // Vùng sắc độ × bão hòa: nhấp đặt h, s nguyên; thanh độ sáng đặt l.
        auto* area = d->findChild<ui::HueSatArea*>(u"hueSat"_s);
        QVERIFY(area);
        QTest::mouseClick(area, Qt::LeftButton, {}, area->plane().center());
        const ui::Hsl picked = area->hsl();
        QCOMPARE(picked.h, std::round(picked.h));
        QVERIFY(std::abs(picked.h - 180) <= 1.5 && std::abs(picked.s - 0.5) <= 0.015);
        QVERIFY((area->markerPos() - area->plane().center()).manhattanLength() <= 2);
        const Rgb fromArea = ui::toRgb({picked.h, picked.s, ui::toHsl({61, 200, 192}).l});
        QCOMPARE(d->color(), Color::rgb(fromArea));
        auto* light = d->findChild<ui::Slider*>(u"lightness"_s);
        light->setValue(30);
        QCOMPARE(d->color(), Color::rgb(ui::toRgb({picked.h, picked.s, 0.30})));
        // Mũi tên trên vùng: ←→ sắc độ 1°, ↑↓ bão hòa 1 %.
        area->setFocus(Qt::TabFocusReason);
        QTest::keyClick(area, Qt::Key_Right);
        QTest::keyClick(area, Qt::Key_Up);
        QCOMPARE(area->hsl().h, picked.h + 1);
        QCOMPARE(qRound(area->hsl().s * 100), qRound(picked.s * 100) + 1);
        QCOMPARE(d->color(), Color::rgb(ui::toRgb({picked.h + 1, (qRound(picked.s * 100) + 1) / 100.0, 0.30})));
        d->reject();
    }

    // Tiêu chí 3.
    void colorField()
    {
        auto d = open(Color::byLayer());
        QVERIFY(d);
        QLineEdit* f = field(d.get());
        QWidget* away = grid(d.get(), "aci");
        QCOMPARE(d->focusWidget(), f); // focus đầu ở ô Màu như prototype
        QCOMPARE(f->text(), u"ByLayer"_s);
        const std::pair<QString, Color> ok[] = {
            {u"1"_s, Color::aci(1)},
            {u"255"_s, Color::aci(255)},
            {u"Màu 140"_s, Color::aci(140)}, // i18n-ok: chữ gõ
            {u"color 9"_s, Color::aci(9)},
            {u"đỏ"_s, Color::aci(1)}, // i18n-ok: chữ gõ
            {u"Do"_s, Color::aci(1)},
            {u"RED"_s, Color::aci(1)},
            {u"vàng"_s, Color::aci(2)},    // i18n-ok: chữ gõ
            {u"xanh lá"_s, Color::aci(3)}, // i18n-ok: chữ gõ
            {u"cyan"_s, Color::aci(4)},
            {u"Xanh Duong"_s, Color::aci(5)}, // i18n-ok: chữ gõ
            {u"magenta"_s, Color::aci(6)},
            {u"trắng"_s, Color::aci(7)}, // i18n-ok: chữ gõ
            {u"bylayer"_s, Color::byLayer()},
            {u"BYBLOCK"_s, Color::byBlock()},
            {u"By Layer"_s, Color::byLayer()},
            {u"#3cc8c0"_s, Color::rgb({0x3C, 0xC8, 0xC0})},
            {u"0A0B0C"_s, Color::rgb({10, 11, 12})},
        };
        for (const auto& [text, want] : ok) {
            typeAndLeave(f, text, away);
            QVERIFY2(d->color() == want, qPrintable(text));
            QCOMPARE(f->text(), want.label());
            QVERIFY(!d->findChild<ui::FieldMessage*>(u"colorMessage"_s)->isError());
        }
        auto* message = d->findChild<ui::FieldMessage*>(u"colorMessage"_s);
        for (const QString& bad :
             {u"0"_s, u"256"_s, u"abc"_s, u"#12345"_s, u"#GG0000"_s, u"Màu 300"_s}) { // i18n-ok: chữ gõ
            d->setColor(Color::aci(30));
            typeAndLeave(f, bad, away);
            QVERIFY2(message->isError(), qPrintable(bad));
            QVERIFY(message->text().startsWith(u"Không đọc được màu này."_s)); // i18n-ok: chữ mong đợi
            QVERIFY(near(edge(f), tm_->color(tok::c_danger)));
            QCOMPARE(d->color(), Color::aci(30));
            QCOMPARE(f->text(), bad); // chữ sai giữ để sửa
        }
        // Enter sai dừng ở ô; Esc trả chữ cũ, Esc nữa đóng hộp (Hủy).
        QSignalSpy accepted(d.get(), &ColorPickerDialog::colorAccepted);
        f->setFocus();
        f->selectAll();
        QTest::keyClicks(f, u"abc"_s); // i18n-ok: chữ gõ
        QTest::keyClick(f, Qt::Key_Return);
        QVERIFY(d->isVisible());
        QCOMPARE(accepted.size(), 0);
        QVERIFY(message->isError());
        QTest::keyClick(f, Qt::Key_Escape);
        QCOMPARE(f->text(), Color::aci(30).label());
        QVERIFY(!message->isError());
        QVERIFY(d->isVisible());
        QTest::keyClick(f, Qt::Key_Escape);
        QVERIFY(!d->isVisible());
        QCOMPARE(d->result(), int(QDialog::Rejected));
        QCOMPARE(accepted.size(), 0);

        // Enter đúng: chọn rồi tới nút xác nhận.
        d = open(Color::byLayer());
        QSignalSpy accepted2(d.get(), &ColorPickerDialog::colorAccepted);
        f = field(d.get());
        f->setFocus();
        f->selectAll();
        QTest::keyClicks(f, u"red"_s);
        QTest::keyClick(f, Qt::Key_Return);
        QCOMPARE(accepted2.size(), 1);
        QCOMPARE(accepted2[0][0].value<Color>(), Color::aci(1));
        QVERIFY(!d->isVisible());
    }

    // Tiêu chí 4 (tới chỗ nối INT-009).
    void pickFromDrawing()
    {
        auto d = open(Color::aci(5));
        QVERIFY(d);
        auto* button = d->findChild<ui::Button*>(u"pick"_s);
        QVERIFY(button && button->isVisible());
        QVERIFY(!button->isEnabled()); // chưa ai nối pickRequested
        QVERIFY(!ColorPickerDialog::pickPrompt().isEmpty());
        int requests = 0;
        const QMetaObject::Connection c =
            connect(d.get(), &ColorPickerDialog::pickRequested, this, [&] { ++requests; });
        QVERIFY(spinUntil([&] { return button->isEnabled(); })); // bật ở lượt sự kiện kế tiếp

        const auto pick = [&] {
            button->click();
            return d->isPicking() && d->isHiddenForPick() && !d->isVisible();
        };
        QVERIFY(pick());
        QCOMPARE(requests, 1);
        d->finishPick(Color::byLayer()); // đối tượng màu ByLayer: trả ByLayer, không phải màu của layer
        QVERIFY(d->isVisible() && !d->isPicking());
        QCOMPARE(d->color(), Color::byLayer());
        QCOMPARE(field(d.get())->text(), u"ByLayer"_s);
        QVERIFY(pick());
        d->finishPick(Color::aci(30));
        QCOMPARE(d->color(), Color::aci(30));
        QVERIFY(pick());
        d->finishPick(Color::rgb({1, 2, 3}));
        QCOMPARE(d->color(), Color::rgb({1, 2, 3}));
        QVERIFY(pick());
        d->cancelPick(); // Esc trên vùng vẽ
        QVERIFY(d->isVisible() && !d->isPicking());
        QCOMPARE(d->color(), Color::rgb({1, 2, 3}));
        d->finishPick(Color::aci(9)); // không đang lấy: bỏ qua
        QCOMPARE(d->color(), Color::rgb({1, 2, 3}));
        QCOMPARE(requests, 4);
        disconnect(c);
        QVERIFY(spinUntil([&] { return !button->isEnabled(); }));
        d->reject();
    }

    // Tiêu chí 5.
    void colorBook()
    {
        const Color ral = Color::book(u"RAL CLASSIC"_s, u"RAL 3020"_s, {193, 18, 28});
        auto d = open(ral);
        QVERIFY(d);
        QCOMPARE(d->tab(), Tab::Book);
        auto* book = d->findChild<ui::EmptyState*>(u"book"_s);
        QVERIFY(book && book->isVisible());
        bool later = false;
        for (const QLabel* l : book->findChildren<QLabel*>())
            later = later || l->text().contains(u"Có sau v1.0"_s); // i18n-ok: chữ mong đợi
        QVERIFY(later);
        QCOMPARE(field(d.get())->text(), u"RAL 3020"_s);
        auto* compare = d->findChild<ui::ColorCompare*>(u"compare"_s);
        QVERIFY(near(pixel(compare, compare->afterRect().center()), QColor(193, 18, 28)));
        QVERIFY(near(pixel(compare, compare->beforeRect().center()), QColor(193, 18, 28)));
        QCOMPARE(hex(d.get())->text(), u"#C1121C"_s); // tab True Color có RGB lưu kèm
        QSignalSpy accepted(d.get(), &ColorPickerDialog::colorAccepted);
        d->accept();
        QCOMPARE(accepted.size(), 1);
        QCOMPARE(accepted[0][0].value<Color>(), ral);                    // giữ nguyên màu Color Book
        QCOMPARE(store_->get(ui::kRecentColors), ui::kRecentColors.def); // Color Book không vào danh sách gần đây
    }

    // Tiêu chí 6: mở ≤ 150 ms (CPU luồng UI, kiểm ở release).
    void openTime()
    {
        QCoreApplication::processEvents();
        const auto t0 = test::threadCpu();
        auto d = std::make_unique<ColorPickerDialog>(Color::aci(1), Mode::Full, window_.get());
        d->open();
        QVERIFY(QTest::qWaitForWindowExposed(d.get()));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        d->repaint();
        const double ms = cpuMs(test::threadCpu() - t0);
        qInfo("mở bộ chọn màu đến hết khung đầu: CPU luồng UI %.1f ms (ngưỡng 150, kiểm ở release)", ms);
        QVERIFY2(!test::kTimed || qIsNaN(ms) || ms <= 150, qPrintable(QString::number(ms)));
        d->reject();
    }

    // Tiêu chí 6: mũi tên đi và chọn trong bảng, Home/End, Space; Enter chọn và đóng.
    void keyboard()
    {
        auto d = open(Color::byLayer());
        QVERIFY(d);
        SwatchGrid* aci = grid(d.get(), "aci");
        aci->setFocus(Qt::TabFocusReason);
        QVERIFY(aci->hasFocus());
        QCOMPARE(d->color(), Color::byLayer()); // vào lưới chưa đổi màu
        QTest::keyClick(aci, Qt::Key_Right);
        QCOMPARE(d->color(), Color::aci(20));
        QTest::keyClick(aci, Qt::Key_Right);
        QTest::keyClick(aci, Qt::Key_Down);
        QCOMPARE(d->color(), Color::aci(31));
        QCOMPARE(d->findChild<QLabel*>(u"hover"_s)->text(), hoverText(31));
        QCOMPARE(aci->accessibleDescription(), hoverText(31)); // trình đọc màn hình đọc ô vừa tới
        QTest::keyClick(aci, Qt::Key_Left);
        QTest::keyClick(aci, Qt::Key_Left);
        QTest::keyClick(aci, Qt::Key_Left); // đầu hàng: đứng yên
        QCOMPARE(d->color(), Color::aci(11));
        QTest::keyClick(aci, Qt::Key_Up);
        QTest::keyClick(aci, Qt::Key_Up);
        QCOMPARE(d->color(), Color::aci(10));
        QTest::keyClick(aci, Qt::Key_End);
        QCOMPARE(d->color(), Color::aci(249));
        QTest::keyClick(aci, Qt::Key_Home);
        QCOMPARE(d->color(), Color::aci(10));
        SwatchGrid* standard = grid(d.get(), "standard");
        standard->setFocus(Qt::TabFocusReason);
        QTest::keyClick(standard, Qt::Key_Space);
        QCOMPARE(d->color(), Color::aci(1));
        QTest::keyClick(standard, Qt::Key_Right);
        QCOMPARE(d->color(), Color::aci(2));
        // Tab vào lưới đã có ô chọn: con trỏ ở ô đó.
        aci->setFocus(Qt::TabFocusReason);
        d->setColor(Color::aci(77));
        standard->setFocus(Qt::TabFocusReason);
        aci->setFocus(Qt::TabFocusReason);
        QCOMPARE(aci->colorAt(aci->current()), Color::aci(77));
        QSignalSpy accepted(d.get(), &ColorPickerDialog::colorAccepted);
        QTest::keyClick(aci, Qt::Key_Right);
        QTest::keyClick(aci, Qt::Key_Return);
        QCOMPARE(accepted.size(), 1);
        QCOMPARE(accepted[0][0].value<Color>(), Color::aci(87));
        QVERIFY(!d->isVisible());
    }

    // Hàng 8 màu gần đây theo hồ sơ; rỗng.
    void recentColors()
    {
        auto d = open(Color::byLayer());
        QVERIFY(d);
        SwatchGrid* recent = grid(d.get(), "recent");
        QCOMPARE(recent->count(), 5); // mặc định như prototype: 1, 3, 8, 140, 30
        QCOMPARE(recent->colorAt(3), Color::aci(140));
        QTest::mouseClick(recent, Qt::LeftButton, {}, recent->cellRect(4).center());
        QCOMPARE(d->color(), Color::aci(30));
        d->setColor(Color::aci(77));
        d->accept();
        QCOMPARE(store_->get(ui::kRecentColors), (QStringList{u"77"_s, u"1"_s, u"3"_s, u"8"_s, u"140"_s, u"30"_s}));
        for (const Color& c :
             {Color::rgb({60, 200, 192}), Color::aci(1), Color::aci(2), Color::aci(4), Color::byBlock()}) {
            d = open(Color::byLayer());
            d->setColor(c);
            d->accept();
        }
        QCOMPARE(store_->get(ui::kRecentColors),
                 (QStringList{u"4"_s, u"2"_s, u"1"_s, u"#3CC8C0"_s, u"77"_s, u"3"_s, u"8"_s, u"140"_s}));
        d = open(Color::byLayer());
        recent = grid(d.get(), "recent");
        QCOMPARE(recent->count(), ColorPickerDialog::kRecentMax);
        QCOMPARE(recent->colorAt(3), Color::rgb({60, 200, 192}));
        QVERIFY(near(pixel(recent, recent->cellRect(3).center()), QColor(60, 200, 192)));
        d->reject();
        // Rỗng: câu "Chưa có màu nào" thay hàng ô.
        store_->set(ui::kRecentColors, QStringList{});
        d = open(Color::byLayer());
        QVERIFY(!grid(d.get(), "recent")->isVisible());
        QVERIFY(d->findChild<QLabel*>(u"recentEmpty"_s)->isVisible());
        d->reject();
    }

    // UID-020: chỉ True Color; kết quả luôn là true color.
    void trueColorOnly()
    {
        auto d = open(Color::byLayer(), Mode::TrueColorOnly);
        QVERIFY(d);
        QCOMPARE(d->tab(), Tab::TrueColor);
        QVERIFY(!d->findChild<ui::TabBar*>(u"tabs"_s)->isVisible());
        QVERIFY(!grid(d.get(), "aci")->isVisible());
        QVERIFY(!d->findChild<ui::Button*>(u"pick"_s)->isVisible());
        QCOMPARE(d->color().kind(), Color::Kind::True); // ByLayer không dùng được: bắt đầu từ màu mặc định
        QLineEdit* f = field(d.get());
        typeAndLeave(f, u"red"_s, hex(d.get()));
        QCOMPARE(d->color(), Color::rgb({255, 0, 0}));
        typeAndLeave(f, u"5"_s, hex(d.get()));
        QCOMPARE(d->color(), Color::rgb({0, 0, 255}));
        typeAndLeave(f, u"bylayer"_s, hex(d.get()));
        QVERIFY(d->findChild<ui::FieldMessage*>(u"colorMessage"_s)->isError());
        QCOMPARE(d->color(), Color::rgb({0, 0, 255}));
        d->setColor(Color::byBlock()); // bỏ qua
        QCOMPARE(d->color(), Color::rgb({0, 0, 255}));
        QTest::mouseClick(grid(d.get(), "recent"), Qt::LeftButton, {},
                          grid(d.get(), "recent")->cellRect(3).center()); // ACI 140 → RGB
        QCOMPARE(d->color(), Color::rgb(regen::aciRgb(140)));
        QSignalSpy accepted(d.get(), &ColorPickerDialog::colorAccepted);
        f->setFocus();
        d->accept();
        QCOMPARE(accepted.size(), 1);
        QCOMPARE(accepted[0][0].value<Color>(), Color::rgb({0, 191, 255}));
        d = open(Color::aci(1), Mode::TrueColorOnly);
        QCOMPARE(d->color(), Color::rgb({255, 0, 0}));
        d->reject();
    }

    // Móc cho lệnh COLOR: callback, không vòng sự kiện lồng; hộp tự hủy khi đóng.
    void askHook()
    {
        std::optional<Color> got;
        QPointer<ColorPickerDialog> d =
            ColorPickerDialog::ask(window_.get(), Color::aci(3), [&](const Color& c) { got = c; });
        QVERIFY(d && d->isModal());
        QVERIFY(QTest::qWaitForWindowExposed(d));
        d->setColor(Color::aci(6));
        d->confirmButton()->click();
        QCOMPARE(got, std::optional<Color>(Color::aci(6)));
        QVERIFY(spinUntil([&] { return d.isNull(); }));
        got.reset();
        d = ColorPickerDialog::ask(window_.get(), Color::aci(3), [&](const Color& c) { got = c; });
        QVERIFY(QTest::qWaitForWindowExposed(d));
        d->reject(); // Hủy: không gọi
        QVERIFY(spinUntil([&] { return d.isNull(); }));
        QVERIFY(!got);
    }

    // DoD-U: vi-XL (dài hơn 40%) không cắt chữ ở mọi tab, cả khi có câu lỗi, cả chế độ True Color.
    void pseudoNoTruncation()
    {
        QVERIFY(translator_->setLanguage(i18n::kPseudoLanguage.toString()));
        QVERIFY(spinUntil([] {
            return QCoreApplication::translate("trion::ui::ColorPickerDialog", "Chọn màu").size() >
                   8; // i18n-ok: khóa dịch
        }));
        auto d = open(Color::aci(140));
        QVERIFY(d);
        for (const Tab t : {Tab::Index, Tab::TrueColor, Tab::Book}) {
            d->setTab(t);
            QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
            QVERIFY2(truncations(d.get()).isEmpty(), qPrintable(truncations(d.get())));
        }
        d->setTab(Tab::Index);
        hoverAt(grid(d.get(), "aci"), grid(d.get(), "aci")->cellRect(0).center());
        typeAndLeave(field(d.get()), u"abc"_s, grid(d.get(), "aci")); // i18n-ok: chữ gõ
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QVERIFY2(truncations(d.get()).isEmpty(), qPrintable(truncations(d.get())));
        d->reject();
        d = open(Color::rgb({1, 2, 3}), Mode::TrueColorOnly);
        QVERIFY2(truncations(d.get()).isEmpty(), qPrintable(truncations(d.get())));
        d->reject();
        QVERIFY(translator_->setLanguage(u"vi"_s));
        QVERIFY(spinUntil([] {
            return QCoreApplication::translate("trion::ui::ColorPickerDialog", "Chọn màu").size() ==
                   8; // i18n-ok: khóa dịch
        }));
    }

    // DoD-U: tên, vai trò, thứ tự Tab, viền focus (a11y::checkNames + checkFocus) ở tab ACI và True Color.
    void accessibility()
    {
        for (const Tab t : {Tab::Index, Tab::TrueColor}) {
            auto d = open(Color::aci(30));
            QVERIFY(d);
            d->setTab(t);
            QVERIFY(waitActive(d.get()));
            QStringList order;
            const QStringList issues = a11y::checkNames(d.get()) + a11y::checkFocus(d.get(), &order);
            QVERIFY2(issues.isEmpty(), qPrintable(issues.join(u'\n')));
            QVERIFY(order.size() >= 8);
            qInfo("tab %d: %lld điểm dừng Tab", int(t), qint64(order.size()));
            d->reject();
        }
    }
};

QTEST_MAIN(ColorPickerTest)
#include "color_picker_test.moc"
