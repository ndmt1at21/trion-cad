// UI0-007 · Ô số chuẩn UI-C09 (NumericField.h). Mỗi tiêu chí nghiệm thu một nhóm hàm test:
//   1  display, unitsChange: hiển thị theo UNITS (1250.00, 4'-1 1/2"); đổi UNITS khi ô đang hiện: chữ mới ngay khi
//      setSpec trả về, ảnh chụp lúc đó bằng ảnh của ô dựng mới với UNITS mới (khung kế tiếp đã đúng), giá trị không
//      đổi, không phát; UiProbe ui_state (setSpec → hết khung kế tiếp) chỉ báo số.
//   2  commitEnter, commitFocusOut: sai → viền --c-danger, câu dưới ô, mô tả trợ năng và QAccessibleAnnouncementEvent,
//      không phát, Enter không tới nút xác nhận; đúng → định dạng lại, phát đúng 1 lần (rời ô sau đó không phát lại),
//      Enter tới nút xác nhận; chặn dưới "… phải lớn hơn 0.".
//   3  stepKeys, mixed, escape: ↑ ↓, Shift ×10, nút bước, ô góc 1°; *Khác nhau* (gõ giá trị mới phát 1 lần); Esc trả
//      giá trị trước khi sửa, Esc lần nữa tới hộp thoại.
//   4  pickDistance, pickAngle, pickCancel: test đóng vai INT-003 và vùng vẽ (FakePrompt). ⌖ ẩn hộp thoại ≤ 100 ms (đo
//      từ cú nhấp đến khi ẩn; ngưỡng chỉ ở release), 2 điểm → khoảng cách / góc, quay lại với giá trị; Esc không đổi.
//   5  typingLatency: UiProbe typing p99 ≤ 16 ms (ngưỡng ở release, tests/timing.h); accessibleName: tên đọc gồm nhãn
//      và đơn vị, giá trị là chữ trong ô, mô tả là câu lỗi.
//   6  states: tấm ảnh 4 loại ô × 8 trạng thái so baseline theo nền tảng ở Tối/Sáng × 100/150/200%; viền focus, lỗi
//      đúng màu token ở mép. Ghi lại baseline: TRION_UPDATE_BASELINE=<thư mục> chạy test, chép field-*-<nền tảng>.png
//      vào tests/baseline. heights, locked; pseudoNoTruncation: vi-XL, findTruncations rỗng.
#include "app/widgets/numeric/NumericField.h"

#include "app/core/i18n/Translator.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/core/perf/UiProbe.h"
#include "app/core/UiExecutor.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/generated/Tokens.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/dialog/DialogFrame.h"
#include "core/base/sched/Scheduler.h"
#include "tests/timing.h"

#include <QAbstractButton>
#include <QAccessible>
#include <QApplication>
#include <QElapsedTimer>
#include <QFormLayout>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <cmath>
#include <functional>
#include <memory>
#include <numbers>

using namespace Qt::StringLiterals;
using namespace trion::theme;
using namespace trion::ui;
namespace i18n = trion::i18n;
namespace regen = trion::regen;
namespace sched = trion::sched;
namespace settings = trion::settings;

namespace {

using trion::test::kTimed;

// Baseline theo nền tảng (FreeType của Linux và Windows dựng chữ khác nhau), dung sai như controls_test.
#if defined(Q_OS_WIN)
constexpr auto kPlatform = "windows";
#elif defined(Q_OS_MACOS)
constexpr auto kPlatform = "macos";
#else
constexpr auto kPlatform = "linux";
#endif
constexpr int kNoise = 8;
constexpr double kHideBudgetMs = 100; // tiêu chí 4
constexpr double kKeyBudgetMs = 16;   // tiêu chí 5, DoD-L "ký tự gõ hiện trong ô nhập"
constexpr int kCellW = 220, kCellH = 76, kInset = 4;

enum State { Default, Hover, Pressed, Focus, Disabled, Locked, Error, Mixed, kStates };

ValueSpec mm(ValueKind kind = ValueKind::Distance)
{
    ValueSpec s{kind, {}, 4}; // INSUNITS 4: milimét
    s.units.luprec = 2;
    return s;
}

ValueSpec feetInch()
{
    ValueSpec s{ValueKind::Distance, {}, 1};
    s.units.lunits = regen::LinearUnits::Architectural;
    s.units.luprec = 1; // phân số mẫu 2
    return s;
}

ValueSpec degrees()
{
    return {ValueKind::Angle, {}, 0}; // AUNITS 0, AUPREC 0
}

QLineEdit* edit(const NumericField* f)
{
    return f->findChild<QLineEdit*>();
}

QAbstractButton* part(const NumericField* f, const char* name)
{
    return f->findChild<QAbstractButton*>(QLatin1StringView(name));
}

QLabel* message(const NumericField* f)
{
    return f->findChild<QLabel*>();
}

void typeInto(NumericField* f, const QString& text)
{
    edit(f)->setFocus();
    edit(f)->selectAll();
    QTest::keyClicks(edit(f), text);
}

bool near(const QColor& a, const QColor& b)
{
    return std::abs(a.red() - b.red()) <= kNoise && std::abs(a.green() - b.green()) <= kNoise &&
           std::abs(a.blue() - b.blue()) <= kNoise;
}

// Màu ở mép trái hộp, giữa chiều cao (viền 2 px khi focus, lỗi).
QColor edge(NumericField* f)
{
    const QImage img = f->grab().toImage();
    return img.pixelColor(1, edit(f)->parentWidget()->height() / 2);
}

QJsonValue op(const char* name)
{
    return trion::ui::probe::toJson()["ops"].toObject()[QLatin1StringView(name)];
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

// Thông báo trợ năng đã gửi (QAccessibleAnnouncementEvent).
QStringList g_said;
void collect(QAccessibleEvent* e)
{
    if (e->type() == QAccessible::Announcement)
        g_said << static_cast<QAccessibleAnnouncementEvent*>(e)->message();
}

// Đóng vai INT-003 và vùng vẽ: sau pickRequested, hai lần nhấn chuột trên cửa sổ chính là hai điểm → khoảng cách,
// hoặc góc của đoạn (trục y màn hình hướng xuống); Esc hủy.
class FakePrompt : public QObject {
public:
    FakePrompt(QWidget* drawing, NumericField* field) : drawing_(drawing), field_(field)
    {
        connect(field, &NumericField::pickRequested, this, [this] {
            ++requests;
            points_.clear();
            drawing_->installEventFilter(this);
        });
    }
    int requests = 0;

protected:
    bool eventFilter(QObject*, QEvent* e) override
    {
        if (e->type() == QEvent::MouseButtonPress) {
            points_ << static_cast<QMouseEvent*>(e)->position();
            if (points_.size() == 2) {
                drawing_->removeEventFilter(this);
                const QPointF d = points_[1] - points_[0];
                field_->finishPick(field_->spec().kind == ValueKind::Angle ? std::atan2(-d.y(), d.x())
                                                                           : std::hypot(d.x(), d.y()));
            }
            return true;
        }
        if (e->type() == QEvent::KeyPress && static_cast<QKeyEvent*>(e)->key() == Qt::Key_Escape) {
            drawing_->removeEventFilter(this);
            field_->cancelPick();
            return true;
        }
        return false;
    }

private:
    QWidget* drawing_;
    NumericField* field_;
    QList<QPointF> points_;
};

} // namespace

// Form mẫu của tiêu chí 6: ba hàng nhãn + ô số (lỗi, góc, *Khác nhau*); chuỗi qua tr() để vi-XL kéo dài.
class FieldSample : public QWidget {
    Q_OBJECT
public:
    FieldSample()
    {
        auto* form = new QFormLayout(this);
        for (int i = 0; i < 3; ++i)
            form->addRow(labels[i] = new QLabel, fields[i] = new NumericField);
        fields[0]->setSpec(mm());
        fields[0]->setValue(1250);
        fields[1]->setSpec(degrees());
        fields[1]->setValue(0.5);
        fields[2]->setSpec(mm(ValueKind::Scale));
        fields[2]->setMixed();
        retranslate();
    }
    QLabel* labels[3];
    NumericField* fields[3];

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
        const QString names[] = {tr("Bán kính"), tr("Góc xoay"), tr("Tỉ lệ")};
        for (int i = 0; i < 3; ++i) {
            labels[i]->setText(names[i]);
            fields[i]->setLabel(names[i]);
        }
    }
};

class NumericFieldTest : public QObject {
    Q_OBJECT
    QTemporaryDir dir_;
    std::unique_ptr<settings::Store> store_;
    std::unique_ptr<UiExecutor> executor_;
    std::unique_ptr<ThemeManager> tm_;
    std::unique_ptr<IconProvider> icons_;
    std::unique_ptr<i18n::Translator> translator_;
    std::unique_ptr<QWidget> host_; // cửa sổ đang hiện, active: focus, vùng vẽ của ⌖

    NumericField* make(const ValueSpec& spec, double value)
    {
        auto* f = new NumericField(spec.kind, host_.get());
        f->setSpec(spec);
        f->setValue(value);
        f->resize(kCellW - 2 * kInset, f->sizeHint().height());
        f->show();
        return f;
    }

    // Hộp thoại chứa một ô, con của host_ (cửa sổ chính), có nút xác nhận.
    std::unique_ptr<DialogFrame> dialogWith(NumericField* f)
    {
        auto dlg = std::make_unique<DialogFrame>(u"z-numfield"_s, host_.get());
        dlg->setBody(f);
        dlg->setConfirm(u"Áp dụng"_s); // i18n-ok: dữ liệu thử
        dlg->show();
        return dlg;
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
        host_ = std::make_unique<QWidget>();
        host_->resize(kCellW * 2, kCellH * 3);
        host_->show();
        QVERIFY(QTest::qWaitForWindowActive(host_.get()));
    }

    void init()
    {
        tm_->setTheme(Theme::Dark);
        tm_->setDensity(Density::Compact);
        host_->activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(host_.get()));
    }

    void cleanup()
    {
        QAccessible::installUpdateHandler(nullptr);
        QAccessible::setActive(false);
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
    void display()
    {
        NumericField* f = make(mm(), 1250);
        QCOMPARE(edit(f)->text(), u"1250.00"_s);
        f->setSpec(feetInch());
        f->setValue(49.5);
        QCOMPARE(edit(f)->text(), u"4'-1 1/2\""_s);
        f->setSpec(degrees());
        f->setValue(std::numbers::pi / 4);
        QCOMPARE(edit(f)->text(), u"45"_s);
        f->setSpec({ValueKind::Integer, {}, 0});
        f->setValue(12);
        QCOMPARE(edit(f)->text(), u"12"_s);
    }

    void unitsChange()
    {
        NumericField* f = make(mm(), 49.5);
        QVERIFY(QTest::qWaitForWindowExposed(host_.get()));
        QSignalSpy committed(f, &NumericField::valueCommitted);
        probe::enable();
        probe::reset();
        const auto start = probe::Clock::now();
        f->setSpec(feetInch());
        probe::untilFrame(probe::Op::UiState, f, start);
        // Chưa chạy vòng sự kiện: chữ và hình đã theo UNITS mới.
        QCOMPARE(edit(f)->text(), u"4'-1 1/2\""_s);
        QCOMPARE(f->value(), 49.5);
        const QImage now = f->grab().toImage();
        NumericField* fresh = make(feetInch(), 49.5);
        QCOMPARE(now, fresh->grab().toImage());
        QVERIFY(spinUntil([] { return op("ui_state")["count"].toInt() >= 1; }));
        qInfo("đổi UNITS → hết khung kế tiếp: %.2f ms", op("ui_state")["max_ms"].toDouble());
        probe::disable();
        QCOMPARE(committed.size(), 0);
        f->setSpec(mm()); // về lại: giá trị vẫn nguyên
        QCOMPARE(edit(f)->text(), u"49.50"_s);
        QCOMPARE(committed.size(), 0);
    }

    // Tiêu chí 2: Enter trong hộp thoại.
    void commitEnter()
    {
        auto* f = new NumericField;
        f->setLabel(u"Bán kính"_s); // i18n-ok: dữ liệu thử
        f->setSpec(mm());
        f->setValue(1250);
        const auto dlg = dialogWith(f);
        QVERIFY(QTest::qWaitForWindowActive(dlg.get()));
        QSignalSpy committed(f, &NumericField::valueCommitted), accepted(dlg.get(), &QDialog::accepted);
        g_said.clear();
        QAccessible::installUpdateHandler(collect);
        QAccessible::setActive(true);

        typeInto(f, u"12,5"_s);
        QTest::keyClick(edit(f), Qt::Key_Return);
        QVERIFY(f->hasError());
        QCOMPARE(f->errorMessage(), errorText(ValueError::Comma, "12,5", mm()));
        QVERIFY(message(f)->isVisible());
        QCOMPARE(message(f)->text(), f->errorMessage());
        QVERIFY(near(edge(f), tm_->color(tok::c_danger)));
        QCOMPARE(committed.size(), 0);
        QCOMPARE(accepted.size(), 0); // Enter sai dừng ở ô
        QVERIFY(dlg->isVisible());
        QCOMPARE(QAccessible::queryAccessibleInterface(edit(f))->text(QAccessible::Description), f->errorMessage());
        QCOMPARE(g_said, QStringList{f->errorMessage()});

        typeInto(f, u"=1200/2+50"_s);
        QTest::keyClick(edit(f), Qt::Key_Return);
        QCOMPARE(committed.size(), 1);
        QCOMPARE(committed[0][0].toDouble(), 650.0);
        QCOMPARE(edit(f)->text(), u"650.00"_s);
        QVERIFY(!f->hasError());
        QVERIFY(message(f)->isHidden());
        QCOMPARE(QAccessible::queryAccessibleInterface(edit(f))->text(QAccessible::Description), QString());
        QCOMPARE(accepted.size(), 1);  // Enter đúng tới nút xác nhận
        QCOMPARE(committed.size(), 1); // hộp thoại đóng, ô mất focus: không phát lại
    }

    // Tiêu chí 2: rời ô; chặn dưới.
    void commitFocusOut()
    {
        NumericField* f = make(mm(), 10);
        f->setLabel(u"Bán kính"_s); // i18n-ok: dữ liệu thử
        f->setLowerBound(0);
        auto* other = new QLineEdit(host_.get());
        other->move(0, kCellH);
        other->show();
        QSignalSpy committed(f, &NumericField::valueCommitted);

        typeInto(f, u"300"_s);
        other->setFocus();
        QCOMPARE(committed.size(), 1);
        QCOMPARE(committed[0][0].toDouble(), 300.0);
        QCOMPARE(edit(f)->text(), u"300.00"_s);
        edit(f)->setFocus();
        other->setFocus();
        QCOMPARE(committed.size(), 1); // chưa sửa: không phát lại

        typeInto(f, u"0"_s);
        other->setFocus();
        QCOMPARE(f->errorMessage(), u"Bán kính phải lớn hơn 0."_s);
        QVERIFY(near(edge(f), tm_->color(tok::c_danger)));
        typeInto(f, u"abc"_s); // i18n-ok: dữ liệu thử
        QTest::keyClick(edit(f), Qt::Key_Return);
        QCOMPARE(f->errorMessage(), errorText(ValueError::Syntax, "abc", mm()));
        QCOMPARE(committed.size(), 1);
        QCOMPARE(f->value(), 300.0);
    }

    // Tiêu chí 3.
    void stepKeys()
    {
        NumericField* f = make(mm(), 10);
        edit(f)->setFocus();
        QSignalSpy committed(f, &NumericField::valueCommitted);
        const auto last = [&] { return committed.last()[0].toDouble(); };
        QTest::keyClick(edit(f), Qt::Key_Up);
        QCOMPARE(last(), 11.0);
        QCOMPARE(edit(f)->text(), u"11.00"_s);
        QTest::keyClick(edit(f), Qt::Key_Up, Qt::ShiftModifier);
        QCOMPARE(last(), 21.0);
        QTest::keyClick(edit(f), Qt::Key_Down);
        QCOMPARE(last(), 20.0);
        QTest::mouseClick(part(f, "up"), Qt::LeftButton);
        QCOMPARE(last(), 21.0);
        // Shift khi bấm nút bước: qua cửa sổ để trạng thái phím bổ trợ của ứng dụng đổi như chuột thật.
        QAbstractButton* down = part(f, "down");
        QTest::mouseClick(host_->windowHandle(), Qt::LeftButton, Qt::ShiftModifier,
                          down->mapTo(host_.get(), down->rect().center()));
        QCOMPARE(last(), 11.0);
        QTest::mouseMove(host_->windowHandle(), QPoint(1, 1)); // nhả Shift cho các test sau
        typeInto(f, u"5"_s);                                   // từ chữ đang gõ
        QTest::keyClick(edit(f), Qt::Key_Up);
        QCOMPARE(last(), 6.0);
        QCOMPARE(committed.size(), 6);

        f->setSpec(degrees());
        f->setValue(0);
        QTest::keyClick(edit(f), Qt::Key_Up);
        QVERIFY(qFuzzyCompare(last(), std::numbers::pi / 180));
        QCOMPARE(edit(f)->text(), u"1"_s);
        f->setStep(std::numbers::pi / 4);
        QTest::keyClick(edit(f), Qt::Key_Down);
        QVERIFY(qFuzzyCompare(last(), std::numbers::pi / 180 - std::numbers::pi / 4));
    }

    void mixed()
    {
        NumericField* f = make(mm(), 10);
        auto* other = new QLineEdit(host_.get());
        other->move(0, kCellH);
        other->show();
        const QImage plain = f->grab().toImage();
        f->setMixed();
        QVERIFY(f->isMixed());
        QCOMPARE(edit(f)->text(), QString());
        QVERIFY(f->grab().toImage() != plain);
        QCOMPARE(QAccessible::queryAccessibleInterface(edit(f))->text(QAccessible::Description),
                 NumericField::tr("*Khác nhau*"));
        QSignalSpy committed(f, &NumericField::valueCommitted);

        edit(f)->setFocus(); // chưa gõ: Enter, ↑ ↓, rời ô không làm gì
        QTest::keyClick(edit(f), Qt::Key_Return);
        QTest::keyClick(edit(f), Qt::Key_Up);
        other->setFocus();
        QCOMPARE(committed.size(), 0);
        QVERIFY(f->isMixed());
        QVERIFY(!f->hasError());

        typeInto(f, u"25"_s); // gõ giá trị mới: phát 1 lần, bên gọi áp cho tất cả
        QTest::keyClick(edit(f), Qt::Key_Return);
        QCOMPARE(committed.size(), 1);
        QCOMPARE(committed[0][0].toDouble(), 25.0);
        QVERIFY(!f->isMixed());
        QCOMPARE(edit(f)->text(), u"25.00"_s);

        f->setMixed(); // Esc khi đang gõ: về *Khác nhau*
        typeInto(f, u"7"_s);
        QTest::keyClick(edit(f), Qt::Key_Escape);
        QVERIFY(f->isMixed());
        QCOMPARE(edit(f)->text(), QString());
        other->setFocus();
        QCOMPARE(committed.size(), 1);
    }

    void escape()
    {
        auto* f = new NumericField;
        f->setSpec(mm());
        f->setValue(1250);
        const auto dlg = dialogWith(f);
        QVERIFY(QTest::qWaitForWindowActive(dlg.get()));
        QSignalSpy committed(f, &NumericField::valueCommitted), rejected(dlg.get(), &QDialog::rejected);

        typeInto(f, u"999"_s);
        QTest::keyClick(edit(f), Qt::Key_Escape);
        QCOMPARE(edit(f)->text(), u"1250.00"_s);
        QVERIFY(dlg->isVisible());

        typeInto(f, u"abc"_s); // i18n-ok: dữ liệu thử
        QTest::keyClick(edit(f), Qt::Key_Return);
        QVERIFY(f->hasError());
        QTest::keyClick(edit(f), Qt::Key_Escape);
        QVERIFY(!f->hasError());
        QCOMPARE(edit(f)->text(), u"1250.00"_s);
        QCOMPARE(rejected.size(), 0);

        QTest::keyClick(edit(f), Qt::Key_Escape); // không còn gì để trả: Esc của hộp thoại
        QCOMPARE(rejected.size(), 1);
        QCOMPARE(committed.size(), 0);
        QCOMPARE(f->value(), 1250.0);
    }

    // Tiêu chí 4.
    void pickDistance()
    {
        auto* f = new NumericField;
        f->setSpec(mm());
        f->setValue(10);
        const auto dlg = dialogWith(f);
        QVERIFY(QTest::qWaitForWindowActive(dlg.get()));
        FakePrompt prompt(host_.get(), f);
        QSignalSpy committed(f, &NumericField::valueCommitted);

        QElapsedTimer clock;
        clock.start();
        QTest::mouseClick(part(f, "pick"), Qt::LeftButton);
        const double hideMs = double(clock.nsecsElapsed()) / 1e6;
        QVERIFY(!dlg->isVisible());
        QVERIFY(dlg->isHiddenForPick());
        QVERIFY(f->isPicking());
        QCOMPARE(prompt.requests, 1);
        qInfo("⌖ → hộp thoại ẩn: %.2f ms (ngưỡng %.0f ms)", hideMs, kHideBudgetMs);
        QVERIFY2(!kTimed || hideMs <= kHideBudgetMs, qPrintable(QString::number(hideMs)));
        QTRY_VERIFY(host_->isActiveWindow()); // phím, chuột về vùng vẽ

        QTest::mouseClick(host_.get(), Qt::LeftButton, {}, QPoint(20, 20));
        QVERIFY(f->isPicking());
        QTest::mouseClick(host_.get(), Qt::LeftButton, {}, QPoint(50, 60));
        QVERIFY(!f->isPicking());
        QVERIFY(dlg->isVisible());
        QCOMPARE(committed.size(), 1);
        QCOMPARE(committed[0][0].toDouble(), 50.0);
        QCOMPARE(edit(f)->text(), u"50.00"_s);
        QTRY_VERIFY(edit(f)->hasFocus());
    }

    void pickAngle()
    {
        auto* f = new NumericField(ValueKind::Angle);
        f->setSpec(degrees());
        const auto dlg = dialogWith(f);
        QVERIFY(QTest::qWaitForWindowActive(dlg.get()));
        FakePrompt prompt(host_.get(), f);
        QSignalSpy committed(f, &NumericField::valueCommitted);
        QTest::mouseClick(part(f, "pick"), Qt::LeftButton);
        QVERIFY(!dlg->isVisible());
        QTest::mouseClick(host_.get(), Qt::LeftButton, {}, QPoint(20, 60));
        QTest::mouseClick(host_.get(), Qt::LeftButton, {}, QPoint(60, 20));
        QVERIFY(dlg->isVisible());
        QCOMPARE(committed.size(), 1);
        QVERIFY(qFuzzyCompare(committed[0][0].toDouble(), std::numbers::pi / 4));
        QCOMPARE(edit(f)->text(), u"45"_s);
    }

    void pickCancel()
    {
        auto* f = new NumericField;
        f->setSpec(mm());
        f->setValue(10);
        const auto dlg = dialogWith(f);
        QVERIFY(QTest::qWaitForWindowActive(dlg.get()));
        QSignalSpy committed(f, &NumericField::valueCommitted);

        QTest::mouseClick(part(f, "pick"), Qt::LeftButton); // chưa ai nối pickRequested: không ẩn, không chờ
        QVERIFY(dlg->isVisible());
        QVERIFY(!f->isPicking());

        FakePrompt prompt(host_.get(), f);
        QTest::mouseClick(part(f, "pick"), Qt::LeftButton);
        QVERIFY(!dlg->isVisible());
        QTest::mouseClick(host_.get(), Qt::LeftButton, {}, QPoint(20, 20)); // một điểm rồi Esc
        QTest::keyClick(host_.get(), Qt::Key_Escape);
        QVERIFY(!f->isPicking());
        QVERIFY(dlg->isVisible());
        QCOMPARE(committed.size(), 0);
        QCOMPARE(f->value(), 10.0);
        QCOMPARE(edit(f)->text(), u"10.00"_s);
        QTRY_VERIFY(edit(f)->hasFocus());
        f->finishPick(99); // không đang lấy: bỏ qua
        QCOMPARE(committed.size(), 0);
    }

    // Tiêu chí 5: phím tới cửa sổ → hết khung kế tiếp (UiProbe tự đo typing).
    void typingLatency()
    {
        NumericField* f = make(mm(), 0);
        QVERIFY(QTest::qWaitForWindowExposed(host_.get()));
        edit(f)->setFocus();
        probe::enable();
        probe::reset();
        constexpr int kN = 200;
        for (int i = 0; i < kN; ++i) {
            if (i % 12 == 0)
                edit(f)->clear();
            QTest::keyClick(host_->windowHandle(), char('0' + i % 10));
            QVERIFY(spinUntil([&] { return op("typing")["count"].toInt() == i + 1; }));
        }
        const double p50 = op("typing")["p50_ms"].toDouble(), p99 = op("typing")["p99_ms"].toDouble();
        probe::disable();
        qInfo("typing: p50 %.2f ms, p99 %.2f ms (%d mẫu, ngưỡng %.0f ms)", p50, p99, kN, kKeyBudgetMs);
        QVERIFY2(!kTimed || p99 <= kKeyBudgetMs, qPrintable(QString::number(p99)));
    }

    void accessibleName()
    {
        NumericField* f = make(mm(), 1250);
        f->setLabel(u"Bán kính"_s); // i18n-ok: dữ liệu thử
        QAccessibleInterface* a = QAccessible::queryAccessibleInterface(edit(f));
        QCOMPARE(a->text(QAccessible::Name), u"Bán kính (milimét)"_s);
        QCOMPARE(a->text(QAccessible::Value), u"1250.00"_s);
        QCOMPARE(a->text(QAccessible::Description), QString());
        typeInto(f, u"=1/0"_s);
        QTest::keyClick(edit(f), Qt::Key_Return);
        QCOMPARE(a->text(QAccessible::Description), errorText(ValueError::DivideByZero, "=1/0", mm()));

        f->setSpec(feetInch());
        QCOMPARE(a->text(QAccessible::Name), u"Bán kính (feet và inch)"_s);
        QCOMPARE(a->text(QAccessible::Description), QString()); // đổi UNITS bỏ lỗi
        ValueSpec none = mm();
        none.insunits = 0;
        f->setSpec(none);
        QCOMPARE(a->text(QAccessible::Name), u"Bán kính"_s);
        f->setSpec(degrees());
        f->setLabel(u"Góc xoay"_s); // i18n-ok: dữ liệu thử
        QCOMPARE(a->text(QAccessible::Name), u"Góc xoay (độ)"_s);

        QAccessibleInterface* pick = QAccessible::queryAccessibleInterface(part(f, "pick"));
        QCOMPARE(pick->role(), QAccessible::Button);
        QCOMPARE(pick->text(QAccessible::Name), NumericField::tr("Lấy từ bản vẽ"));
        QCOMPARE(QAccessible::queryAccessibleInterface(part(f, "up"))->text(QAccessible::Name),
                 NumericField::tr("Tăng"));
    }

    // Tiêu chí 6.
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
        struct Row {
            ValueSpec spec;
            double value;
            QString invalid; // chữ sai cho trạng thái lỗi
        };
        const Row rows[] = {{mm(), 1250, u"12,5"_s},
                            {feetInch(), 49.5, u"4'1/0"_s},
                            {degrees(), std::numbers::pi / 4, u"=1/0"_s},
                            {{ValueKind::Integer, {}, 0}, 12, u"1.5"_s}};
        constexpr int kRows = int(std::size(rows));
        QImage sheet(QSize(kCellW * kStates, kCellH * kRows) * dpr, QImage::Format_ARGB32_Premultiplied);
        sheet.setDevicePixelRatio(dpr);
        sheet.fill(tm_->color(tok::c_base));
        QPainter p(&sheet);
        QCursor::setPos(host_->frameGeometry().bottomRight() + QPoint(kCellW, kCellH)); // chuột ngoài cửa sổ
        for (int r = 0; r < kRows; ++r) {
            for (int s = 0; s < kStates; ++s) {
                std::unique_ptr<NumericField> f(make(rows[r].spec, rows[r].value));
                QAbstractButton* pick = part(f.get(), "pick");
                switch (State(s)) {
                case Hover:
                    (pick->isHidden() ? part(f.get(), "up") : pick)->setAttribute(Qt::WA_UnderMouse);
                    break;
                case Pressed:
                    part(f.get(), "up")->setDown(true);
                    break;
                case Focus:
                    edit(f.get())->setFocus();
                    break;
                case Disabled:
                    f->setEnabled(false);
                    break;
                case Locked:
                    f->setLocked(true);
                    break;
                case Error:
                    edit(f.get())->setText(rows[r].invalid);
                    QTest::keyClick(edit(f.get()), Qt::Key_Return);
                    break;
                case Mixed:
                    f->setMixed();
                    break;
                default:
                    break;
                }
                QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
                const int w = kCellW - 2 * kInset;
                f->resize(w, f->hasHeightForWidth() ? f->heightForWidth(w) : f->sizeHint().height());
                f->render(&p, QPoint(s * kCellW + kInset, r * kCellH + kInset), {}, QWidget::DrawChildren);
            }
        }
        p.end();
        const QString name = u"field-%1-%2-%3.png"_s.arg(light ? u"light"_s : u"dark"_s)
                                 .arg(qRound(dpr * 100))
                                 .arg(QLatin1StringView(kPlatform));
        const QString out = qEnvironmentVariable("TRION_UPDATE_BASELINE");
        if (!out.isEmpty())
            QVERIFY(sheet.save(out + u'/' + name, "PNG", 0));

        // Viền focus, lỗi: màu token ở mép trái hộp (không phụ thuộc font).
        const int boxH = qRound(tm_->px(tok::h_ctl));
        const auto at = [&](int r, State s, qreal x) {
            return sheet.pixelColor((QPointF(s * kCellW + kInset + x, r * kCellH + kInset + boxH / 2) * dpr).toPoint());
        };
        for (int r = 0; r < kRows; ++r) {
            QVERIFY(near(at(r, Focus, 1), tm_->color(tok::c_focus)));
            QVERIFY(near(at(r, Error, 1), tm_->color(tok::c_danger)));
            QVERIFY(near(at(r, Default, 0.25), tm_->color(tok::c_border_ctl)));
        }
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
            sheet.save(QDir::temp().filePath(u"z-numfield-got-"_s + name)); // để so bằng mắt
        QVERIFY(off <= sheet.width() * sheet.height() / 1000);
    }

    void heights()
    {
        for (Density d : {Density::Compact, Density::Comfortable}) {
            tm_->setDensity(d);
            NumericField f;
            QCOMPARE(f.sizeHint().height(), qRound(tm_->px(tok::h_ctl)));
        }
    }

    void locked()
    {
        NumericField* f = make(mm(), 10);
        QSignalSpy committed(f, &NumericField::valueCommitted);
        f->setLocked(true);
        QVERIFY(part(f, "pick")->isHidden());
        QVERIFY(part(f, "up")->isHidden());
        QVERIFY(part(f, "down")->isHidden());
        edit(f)->setFocus();
        QTest::keyClick(edit(f), Qt::Key_Up);
        QTest::keyClicks(edit(f), u"5"_s);
        QTest::keyClick(edit(f), Qt::Key_Return);
        QCOMPARE(edit(f)->text(), u"10.00"_s);
        QCOMPARE(committed.size(), 0);
        f->setLocked(false);
        QVERIFY(!part(f, "pick")->isHidden());
        QVERIFY(!part(f, "up")->isHidden());
        QTest::keyClick(edit(f), Qt::Key_Up);
        QCOMPARE(committed.size(), 1);

        // ⌖ chỉ ở ô độ dài, góc; bỏ được (prototype pick:false).
        f->setPickEnabled(false);
        QVERIFY(part(f, "pick")->isHidden());
        f->setPickEnabled(true);
        QVERIFY(!part(f, "pick")->isHidden());
        f->setSpec(mm(ValueKind::Scale));
        QVERIFY(part(f, "pick")->isHidden());
        f->setSpec(degrees());
        QVERIFY(!part(f, "pick")->isHidden());
    }

    void pseudoNoTruncation()
    {
        FieldSample w;
        w.resize(420, 360);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        typeInto(w.fields[0], u"12,5"_s); // dòng lỗi dài
        QTest::keyClick(edit(w.fields[0]), Qt::Key_Return);
        QVERIFY(w.fields[0]->hasError());
        const auto report = [&w] {
            QStringList out;
            for (const i18n::Truncation& t : i18n::findTruncations(&w))
                out << t.toString();
            return out.join(u"; "_s);
        };
        QTRY_COMPARE(report(), QString());
        const QString vi = w.fields[0]->errorMessage();
        QVERIFY(translator_->setLanguage(i18n::kPseudoLanguage.toString()));
        QTRY_VERIFY(w.fields[0]->errorMessage().size() >= vi.size() + 2); // câu lỗi dịch lại theo ngôn ngữ mới
        QTRY_COMPARE(report(), QString());
        for (Density d : {Density::Comfortable, Density::Compact}) {
            tm_->setDensity(d);
            QTRY_COMPARE(report(), QString());
        }
        QVERIFY(translator_->setLanguage(u"vi"_s));
        QTRY_COMPARE(w.fields[0]->errorMessage(), vi);
    }
};

QTEST_MAIN(NumericFieldTest)
#include "numeric_field_test.moc"
