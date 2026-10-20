// UI0-002 tiêu chí 2, 3: đủ tên ngữ nghĩa 07.1 §13.2 ở hai chủ đề, token vùng vẽ không đổi theo chủ đề,
// ghi đè và lựa chọn chủ đề sống qua khởi động lại (Store mới cùng tệp), "Đặt lại" trả giá trị tokens.css.
#include "app/theme/ThemeManager.h"

#include <QFile>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <map>

using namespace Qt::StringLiterals;
using namespace trion;
using namespace trion::theme;

namespace {

constexpr tok::Mode kDark{}, kLight{.light = true};

QString hex(const QColor& c) { return c.name(QColor::HexArgb); }
QString qs(std::string_view s) { return QString::fromLatin1(s.data(), qsizetype(s.size())); }

std::string_view cssName(const SemEntry& e)
{
    return e.kind == tok::Kind::Color ? tok::name(tok::ColorId{e.token}) : tok::name(tok::NumId{e.token});
}

// 07.1 §13.2 → token, theo styleguide.html mục Màu ("--c-sel-w: selection.window: viền liền"…).
const std::map<std::string_view, std::string_view> kSpec = {
    {"selection.window", "--c-sel-w"},
    {"selection.crossing", "--c-sel-c"},
    {"highlight.hover", "--c-hl-hover"},
    {"highlight.selected", "--c-hl-sel"},
    {"highlight.result", "--c-hl-res"},
    {"grip.cold", "--c-grip-cold"},
    {"grip.hover", "--c-grip-hover"},
    {"grip.hot", "--c-grip-hot"},
    {"snap.marker", "--c-snap"},
    {"tracking.vector", "--c-track"},
    {"dyn.tooltip.bg", "--c-dyn-bg"},
    {"dyn.tooltip.text", "--c-dyn-t"},
    {"fade.xref", "--fade-xref"},
    {"fade.locked", "--fade-locked"},
    {"paper.bg", "--c-paper"},
    {"paper.printable", "--c-paper-print"},
    {"paper.shadow", "--c-paper-shadow"},
    {"status.on", "--c-on"},
    {"status.off", "--c-off"},
    // 6 phiên bản: Viewer, Viewer Pro, Pro, Dùng thử, Giáo dục, Doanh nghiệp
    {"edition.badge.viewer", "--c-ed-viewer"},
    {"edition.badge.viewerPro", "--c-ed-vp"},
    {"edition.badge.pro", "--c-ed-pro"},
    {"edition.badge.trial", "--c-ed-trial"},
    {"edition.badge.education", "--c-ed-edu"},
    {"edition.badge.enterprise", "--c-ed-ent"},
    {"compare.onlyA", "--c-cmp-a"},
    {"compare.onlyB", "--c-cmp-b"},
    {"compare.common", "--c-cmp-common"},
};

} // namespace

class SemanticTest : public QObject {
    Q_OBJECT
    QTemporaryDir dir_;
    QString file() const { return dir_.filePath(u"user.json"_s); }

private slots:
    void init() { QFile::remove(file()); }

    void everyNameResolvesInBothThemes()
    {
        settings::Store store(file());
        const SemanticColors sem(store);
        for (const auto& [name, css] : kSpec) {
            const auto s = semFromName(name);
            QVERIFY2(s, name.data());
            QCOMPARE(qs(cssName(semEntry(*s))), qs(css));
            if (semEntry(*s).kind == tok::Kind::Num) {
                QVERIFY(sem.fade(*s) > 0 && sem.fade(*s) < 1);
                continue;
            }
            for (const tok::Mode m : {kDark, kLight})
                QVERIFY2(sem.color(*s, m).isValid() && sem.color(*s, m).alpha() > 0, name.data());
        }
        // Huy hiệu phiên bản theo chủ đề giao diện, có màu chữ riêng; tên phụ đều tra ngược được.
        QCOMPARE(hex(sem.color(Sem::EditionPro, kDark)), u"#ff3cc8c0"_s);
        QCOMPARE(hex(sem.color(Sem::EditionPro, kLight)), u"#ff06706a"_s);
        QCOMPARE(hex(sem.color(Sem::EditionTrialText, kLight)), u"#ffffffff"_s);
        QCOMPARE(hex(sem.color(Sem::StatusOn, kLight)), u"#ff06706a"_s);
        for (const SemEntry& e : semEntries())
            QVERIFY(semFromName(e.name) == e.sem);
        QVERIFY(!semFromName("grip.khong-co"));
    }

    void canvasNamesIgnoreUiTheme()
    {
        settings::Store store(file());
        const SemanticColors sem(store);
        int canvas = 0;
        for (const SemEntry& e : semEntries()) {
            if (!e.canvas || e.kind != tok::Kind::Color)
                continue;
            ++canvas;
            QCOMPARE(hex(sem.color(e.sem, kLight)), hex(sem.color(e.sem, kDark)));
            QCOMPARE(hex(sem.color(e.sem, {.light = true, .comfortable = true, .lite = true})),
                     hex(toQColor(tok::color(tok::ColorId{e.token}, kDark))));
        }
        QCOMPARE(canvas, 22);
        // tokens.css khối Sáng ghi đè grip.*, dyn.*, paper-shadow; vùng vẽ vẫn tối nên giữ giá trị :root.
        QVERIFY(tok::color(tok::c_grip_cold, kLight) != tok::color(tok::c_grip_cold, kDark));
        QCOMPARE(hex(sem.color(Sem::GripCold, kLight)), u"#ff4c8dff"_s);
        QCOMPARE(hex(sem.color(Sem::DynTooltipBg, kLight)), u"#ff262a30"_s);
        QCOMPARE(hex(sem.color(Sem::PaperShadow, kLight)), u"#8c000000"_s); // rgba(0,0,0,.55)

        // Qua ThemeManager: đổi chủ đề giao diện, màu vùng vẽ giữ nguyên, màu giao diện đổi.
        ThemeManager tm(store);
        const QColor grip = tm.color(Sem::GripHot), on = tm.color(Sem::StatusOn);
        QSignalSpy spy(&tm, &ThemeManager::changed);
        tm.setTheme(Theme::Light);
        QCOMPARE(spy.count(), 1);
        QVERIFY(tm.mode().light);
        QCOMPARE(tm.color(Sem::GripHot), grip);
        QVERIFY(tm.color(Sem::StatusOn) != on);
    }

    void overridesSurviveRestartAndReset()
    {
        const QColor mine(0x12, 0x34, 0x56, 0xEE);
        {
            settings::Store store(file());
            ThemeManager tm(store);
            QSignalSpy spy(&tm, &ThemeManager::changed);
            tm.semantic().setColor(Sem::GripHot, mine);
            tm.semantic().setFade(Sem::FadeXref, 0.7);
            tm.semantic().setColor(Sem::EditionPro, mine);
            QCOMPARE(spy.count(), 3); // widget vùng vẽ, UI-D10 vẽ lại
            QCOMPARE(tm.color(Sem::GripHot), mine);
            tm.setTheme(Theme::Light);
            tm.setDensity(Density::Comfortable);
            tm.setLite(true);
        } // ~Store ghi nốt xuống đĩa
        {
            settings::Store store(file()); // "khởi động lại"
            ThemeManager tm(store);
            QCOMPARE(tm.theme(), Theme::Light);
            QCOMPARE(tm.density(), Density::Comfortable);
            QVERIFY(tm.lite());
            QVERIFY(tm.mode().light && tm.mode().comfortable && tm.mode().lite);
            SemanticColors& sem = tm.semantic();
            QCOMPARE(tm.color(Sem::GripHot), mine);
            QCOMPARE(tm.fade(Sem::FadeXref), 0.7);
            QCOMPARE(sem.color(Sem::EditionPro, kDark), mine); // ghi đè áp cho cả hai chủ đề
            QVERIFY(sem.isOverridden(Sem::GripHot) && !sem.isOverridden(Sem::GripCold));

            sem.reset(Sem::GripHot); // "Đặt lại" một màu: đúng giá trị tokens.css
            QCOMPARE(hex(tm.color(Sem::GripHot)), u"#ffff5b57"_s);
            QVERIFY(!sem.isOverridden(Sem::GripHot));
            QCOMPARE(tm.fade(Sem::FadeXref), 0.7);
            sem.resetAll();
            QCOMPARE(tm.fade(Sem::FadeXref), 0.5);
            QCOMPARE(hex(tm.color(Sem::EditionPro)), u"#ff06706a"_s);
            QVERIFY(!store.value(QLatin1StringView(kSemanticOverrides.key)).isObject());
        }
        {
            settings::Store store(file());
            QVERIFY(!SemanticColors(store).isOverridden(Sem::FadeXref));
        }
    }

    void badValuesInFileFallBackToDefaults()
    {
        // Tệp cài đặt sửa tay: giá trị sai kiểu, màu hỏng, tên lạ → dùng tokens.css, không lỗi.
        QFile f(file());
        QVERIFY(f.open(QIODevice::WriteOnly));
        const QJsonObject colors{{u"grip.hot"_s, u"khong-phai-mau"_s}, {u"fade.xref"_s, u"x"_s},
                                 {u"fade.locked"_s, 7}, {u"grip.khong-co"_s, u"#ff0000"_s}};
        f.write(QJsonDocument(QJsonObject{{u"ui/theme"_s, u"tim"_s}, {u"ui/density"_s, 3}, {u"ui/lite"_s, u"co"_s},
                                          {u"ui/semanticColors"_s, colors}})
                    .toJson());
        f.close();
        settings::Store store(file());
        ThemeManager tm(store);
        QCOMPARE(tm.theme(), Theme::Dark);
        QCOMPARE(tm.density(), Density::Compact);
        QVERIFY(!tm.lite());
        QCOMPARE(hex(tm.color(Sem::GripHot)), u"#ffff5b57"_s);
        QCOMPARE(tm.fade(Sem::FadeXref), 0.5);
        QCOMPARE(tm.fade(Sem::FadeLocked), 1.0); // số ngoài 0..1 bị kẹp
    }
};

QTEST_GUILESS_MAIN(SemanticTest)
#include "semantic_test.moc"
