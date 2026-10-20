#include "app/theme/generated/Tokens.h"

#include <QTest>

using namespace Qt::StringLiterals;

namespace {
constexpr tok::Mode dark{};
constexpr tok::Mode light{.light = true};

QString hex(tok::Rgba c)
{
    return QString::asprintf("#%02X%02X%02X%02X", c.r, c.g, c.b, c.a);
}
QString text(std::string_view s)
{
    return QString::fromUtf8(s.data(), qsizetype(s.size()));
}
} // namespace

// Tra được lúc biên dịch: Tokens.h chỉ có hằng constexpr, không cấp phát.
static_assert(tok::px(tok::h_row, tok::Mode{.comfortable = true}) == 26);
static_assert(tok::find("--c-accent")->kind == tok::Kind::Color);

class TokensTest : public QObject {
    Q_OBJECT
private slots:
    void colorsFollowTheme()
    {
        QCOMPARE(hex(tok::color(tok::c_accent, dark)), u"#3CC8C0FF"_s);
        QCOMPARE(hex(tok::color(tok::c_accent, light)), u"#06706AFF"_s);
        QCOMPARE(hex(tok::color(tok::c_hover, dark)), u"#FFFFFF0E"_s); // rgba(255,255,255,.055)
        // Mật độ, Giao diện nhẹ không đổi màu.
        QCOMPARE(hex(tok::color(tok::c_text, {.light = true, .comfortable = true, .lite = true})),
                 hex(tok::color(tok::c_text, light)));
    }

    void sizesFollowDensity()
    {
        QCOMPARE(tok::px(tok::h_row, {}), 22.0);
        QCOMPARE(tok::px(tok::h_row, {.comfortable = true}), 26.0);
        QCOMPARE(tok::px(tok::fs_xs, {.comfortable = true}), 10.5);
        QCOMPARE(tok::px(tok::pal_w, {.light = true}), 272.0);
    }

    void motionOffInLiteAndReducedMotion()
    {
        QCOMPARE(tok::ms(tok::t, {}), 120);
        QCOMPARE(tok::ms(tok::t_fast, {}), 80);
        QCOMPARE(tok::ms(tok::t, {.lite = true}), 0);
        QCOMPARE(tok::ms(tok::t, {.reducedMotion = true}), 0);
    }

    void liteShadowIsBorderCtlOfTheme()
    {
        const tok::Shadow pop = tok::shadow(tok::sh_pop, dark);
        QCOMPARE(pop.y, 4.0);
        QCOMPARE(pop.blur, 14.0);
        QCOMPARE(hex(pop.color), u"#00000073"_s); // rgba(0,0,0,.45)
        // Giao diện nhẹ: viền 1 px màu --c-border-ctl của chủ đề đang dùng (var() lồng trong tokens.css).
        QCOMPARE(hex(tok::color(tok::c_border_ctl, dark)), u"#646B75FF"_s);
        QCOMPARE(hex(tok::color(tok::c_border_ctl, light)), u"#858D97FF"_s);
        for (const bool isLight : {false, true}) {
            const tok::Shadow s = tok::shadow(tok::sh_dlg, {.light = isLight, .lite = true});
            QCOMPARE(s.blur, 0.0);
            QCOMPARE(s.spread, 1.0);
            QCOMPARE(hex(s.color), hex(tok::color(tok::c_border_ctl, {.light = isLight})));
        }
    }

    void numbersAndStrings()
    {
        QCOMPARE(tok::num(tok::fw_m), 500.0);
        QCOMPARE(tok::num(tok::lh), 1.35);
        QCOMPARE(tok::num(tok::fade_xref), 0.5);
        QCOMPARE(text(tok::str(tok::cap)), u"square"_s);
        QCOMPARE(text(tok::str(tok::c_icon_fill)), u"none"_s);
        QVERIFY(text(tok::str(tok::f_ui)).startsWith(u"\"IBM Plex Sans\", system-ui"_s));
    }

    void namesRoundTrip()
    {
        QCOMPARE(text(tok::name(tok::c_accent)), u"--c-accent"_s);
        QCOMPARE(text(tok::name(tok::h_row)), u"--h-row"_s);
        QCOMPARE(text(tok::name(tok::sh_pop)), u"--sh-pop"_s);
        for (const tok::Token& t : tok::tokens)
            QCOMPARE(tok::find(t.name), &t);
        QVERIFY(!tok::find("--khong-co"));
    }
};

QTEST_GUILESS_MAIN(TokensTest)
#include "tokens_test.moc"
