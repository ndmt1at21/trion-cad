// Test UI0-109: ca vàng (golden.tsv: giá trị, mã lỗi, chuỗi hiển thị lại RGN-013 từng ký tự), khớp INT-105 từng bit,
// câu lỗi, biểu thức lồng sâu và dài, kiểm vòng hiển thị → đọc lại, không cấp phát, hiệu năng.
#include "app/widgets/numeric/ValueParser.h"
#include "cmd/parse/ValueParser.h"
#include "core/regen/numfmt/NumFormat.h"
#include "tests/timing.h"

#include <QFile>
#include <QTest>

#include <algorithm>
#include <atomic>
#include <cfloat>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <new>
#include <numbers>
#include <string>
#include <vector>

using namespace trion::ui;
namespace cmd = trion::cmd;
using trion::regen::AngularUnits;
using trion::regen::kMaxNumText;
using trion::regen::LinearUnits;
using trion::regen::UnitContext;

// Thay operator new chỉ khi không sanitizer (cùng điều kiện với trion::test::kSanitized, nhưng cần ở tiền xử lý).
#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
#define TRION_SANITIZED 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
#define TRION_SANITIZED 1
#endif
#endif
#ifndef TRION_SANITIZED
#define TRION_SANITIZED 0
#endif

// Đếm cấp phát của cả tiến trình để kiểm "không cấp phát". Sanitizer tự định nghĩa new/delete nên không thay.
#if !TRION_SANITIZED
static std::atomic<long> g_allocs{0};
void* operator new(std::size_t n)
{
    g_allocs.fetch_add(1, std::memory_order_relaxed);
    if (void* p = std::malloc(n ? n : 1))
        return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n)
{
    return operator new(n);
}
void operator delete(void* p) noexcept
{
    std::free(p);
}
void operator delete[](void* p) noexcept
{
    std::free(p);
}
void operator delete(void* p, std::size_t) noexcept
{
    std::free(p);
}
void operator delete[](void* p, std::size_t) noexcept
{
    std::free(p);
}
#endif

namespace {

constexpr double kRadPerDeg = std::numbers::pi / 180;
constexpr int kBenchRounds = trion::test::kSanitized ? 2 : trion::test::kTimed ? 50 : 5;

struct Case {
    int line = 0;
    std::string kind, input, expect, display;
    ValueSpec spec;
};

// \t, \xHH, \\ trong cột chuỗi.
std::string unescape(const QByteArray& f)
{
    std::string s;
    for (qsizetype i = 0; i < f.size(); ++i) {
        if (f[i] != '\\' || i + 1 >= f.size()) {
            s += f[i];
            continue;
        }
        const char n = f[++i];
        if (n == 't') {
            s += '\t';
        } else if (n == 'x' && i + 2 < f.size()) {
            unsigned v = 0;
            std::from_chars(f.data() + i + 1, f.data() + i + 3, v, 16);
            s += char(v);
            i += 2;
        } else {
            s += n;
        }
    }
    return s;
}

ValueKind kindOf(const std::string& k)
{
    return k == "dist"    ? ValueKind::Distance
           : k == "ang"   ? ValueKind::Angle
           : k == "scale" ? ValueKind::Scale
           : k == "int"   ? ValueKind::Integer
                          : ValueKind::Real;
}

const std::vector<Case>& cases()
{
    static const std::vector<Case> all = [] {
        std::vector<Case> v;
        QFile f(QStringLiteral(":/tests/golden.tsv"));
        if (!f.open(QIODevice::ReadOnly))
            qFatal("thiếu tài nguyên golden.tsv");
        int line = 0;
        while (!f.atEnd()) {
            ++line;
            QByteArray l = f.readLine();
            l.chop(l.endsWith('\n') ? 1 : 0);
            if (l.isEmpty() || l.startsWith('#'))
                continue;
            const QList<QByteArray> c = l.split('\t');
            if (c.size() != 5)
                qFatal("golden.tsv:%d: cần 5 cột", line);
            Case k;
            k.line = line;
            k.kind = c[0].toStdString();
            k.input = unescape(c[2]);
            k.expect = c[3].toStdString();
            k.display = unescape(c[4]);
            k.spec.kind = kindOf(k.kind);
            for (const QByteArray& t : c[1].split(' ')) {
                const int n = t.mid(1).toInt();
                if (t == "-")
                    continue;
                if (t.startsWith("base="))
                    k.spec.units.angbase = t.mid(5).toDouble() * kRadPerDeg;
                else if (t == "cw")
                    k.spec.units.angdir = true;
                else if (t == "dir")
                    k.spec.units.useAngbase = true;
                else if (t == "um")
                    k.spec.units.unitmode = true;
                else if (t[0] == 'L')
                    k.spec.units.lunits = LinearUnits(n);
                else if (t[0] == 'P')
                    k.spec.units.luprec = n;
                else if (t[0] == 'A')
                    k.spec.units.aunits = AngularUnits(n);
                else if (t[0] == 'Q')
                    k.spec.units.auprec = n;
                else if (t[0] == 'Z')
                    k.spec.units.dimzin = std::uint8_t(n);
                else if (t[0] == 'U')
                    k.spec.insunits = n;
                else
                    qFatal("golden.tsv:%d: ngữ cảnh lạ '%s'", line, t.constData());
            }
            v.push_back(std::move(k));
        }
        return v;
    }();
    return all;
}

QString show(std::string_view s)
{
    return QString::fromUtf8(s.data(), qsizetype(s.size()));
}

std::string display(double v, const ValueSpec& spec)
{
    char b[kMaxNumText];
    return {b, formatValue(v, spec, b)};
}

// SplitMix64: số giả ngẫu nhiên rẻ, tất định theo chỉ số.
std::uint64_t mix(std::uint64_t z)
{
    z += 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

double unit(std::uint64_t z) // [0, 1)
{
    return double(z >> 11) * 0x1p-53;
}

} // namespace

class NumericValueParserTest : public QObject {
    Q_OBJECT
private slots:
    // Tiêu chí 1: ≥ 300 ca vàng. Giá trị kỳ vọng tính bằng phân số chính xác (Python fractions), không lấy từ
    // parser; độ dài, số ≤ 8 ulp, góc ≤ 1e-14 rad tương đối. Chuỗi hiển thị lại khớp RGN-013 từng ký tự.
    void golden()
    {
        QStringList fails;
        int ok = 0, err = 0, expr = 0;
        for (const Case& k : cases()) {
            const ValueResult r = parseValue(k.input, k.spec);
            const QString got =
                r ? QString::number(r.value, 'g', 17) + QStringLiteral(" «%1»").arg(show(display(r.value, k.spec)))
                  : QStringLiteral("!") + show(errorCode(r.error));
            const QString where = QStringLiteral("golden.tsv:%1 %2 \"%3\": muốn %4 «%5», được %6")
                                      .arg(k.line)
                                      .arg(QString::fromStdString(k.kind), show(k.input),
                                           QString::fromStdString(k.expect), show(k.display), got);
            expr += k.input.find('=') != std::string::npos;
            if (k.expect[0] == '!') {
                ++err;
                if (r || errorCode(r.error) != std::string_view(k.expect).substr(1))
                    fails << where;
                continue;
            }
            ++ok;
            std::string_view e = k.expect;
            const bool rad = e.back() == 'r';
            if (rad)
                e.remove_suffix(1);
            double want = std::strtod(std::string(e).c_str(), nullptr);
            if (k.kind == "ang" && !rad)
                want *= kRadPerDeg;
            const double tol =
                k.kind == "ang" ? 1e-14 * std::max(1.0, std::fabs(want)) : 8 * DBL_EPSILON * std::fabs(want);
            if (!r || !(std::fabs(r.value - want) <= tol) || display(r.value, k.spec) != k.display)
                fails << where;
        }
        qInfo("%d ca vàng: %d hợp lệ, %d lỗi, %d có biểu thức", int(cases().size()), ok, err, expr);
        QVERIFY(cases().size() >= 300);
        QVERIFY2(fails.isEmpty(), qPrintable(fails.join('\n')));
    }

    // "Giá trị khớp INT-105": chuỗi không '=' mà INT-105 đọc được thì ô số cho đúng double đó (từng bit); INT-105 báo
    // lỗi không thuộc phần UI0-109 mở rộng (hậu tố đơn vị, feet-inch ở LUNITS thập phân) thì cùng mã lỗi.
    void matchesInt105()
    {
        QStringList fails;
        int compared = 0;
        for (const Case& k : cases()) {
            if (k.input.find('=') != std::string::npos)
                continue;
            UnitContext c = k.spec.units;
            cmd::Parsed<double> ref;
            switch (k.spec.kind) {
            case ValueKind::Distance:
                ref = cmd::parseDistance(k.input, c);
                break;
            case ValueKind::Angle:
                ref = cmd::parseAngle(k.input, c);
                break;
            case ValueKind::Scale:
                ref = cmd::parseDistance(k.input, UnitContext{}); // thập phân: số, phân số, hỗn số
                break;
            case ValueKind::Integer: {
                const cmd::Parsed<int> i = cmd::parseInt(k.input);
                ref = {double(i.value), i.error, i.end};
                break;
            }
            case ValueKind::Real:
                ref = cmd::parseReal(k.input);
                break;
            }
            const ValueResult r = parseValue(k.input, k.spec);
            const std::string_view code = cmd::errorCode(ref.error);
            if (ref) {
                ++compared;
                if (!r || std::memcmp(&r.value, &ref.value, sizeof r.value) != 0)
                    fails << QStringLiteral("golden.tsv:%1 \"%2\": INT-105 %3, ô số %4")
                                 .arg(k.line)
                                 .arg(show(k.input))
                                 .arg(ref.value, 0, 'g', 17)
                                 .arg(r ? QString::number(r.value, 'g', 17) : show(errorCode(r.error)));
            } else if (code != "syntax" && code != "feet-inch" && (r || errorCode(r.error) != code)) {
                fails << QStringLiteral("golden.tsv:%1 \"%2\": INT-105 !%3, ô số %4")
                             .arg(k.line)
                             .arg(show(k.input), show(code), show(errorCode(r.error)));
            }
        }
        qInfo("%d ca so từng bit với INT-105", compared);
        QVERIFY(compared >= 100);
        QVERIFY2(fails.isEmpty(), qPrintable(fails.join('\n')));
    }

    // Tiêu chí 2: mọi lỗi có câu giải thích (không ném ngoại lệ); "12,5" gợi ý "12.5".
    void errorTexts()
    {
        ValueSpec mm;
        mm.kind = ValueKind::Distance;
        mm.insunits = 4;
        const auto text = [&](std::string_view s, const ValueSpec& spec) {
            const ValueResult r = parseValue(s, spec);
            return r ? QString() : errorText(r.error, s, spec);
        };
        QCOMPARE(text("12,5", mm), QStringLiteral("Dấu thập phân là dấu chấm: gõ 12.5"));
        QCOMPARE(text(" 1,5m ", mm), QStringLiteral("Dấu thập phân là dấu chấm: gõ 1.5m"));
        QCOMPARE(text("=1,5+2,25", mm), QStringLiteral("Dấu thập phân là dấu chấm: gõ =1.5+2.25"));
        QCOMPARE(text("1,000.5", mm), QStringLiteral("Dấu thập phân là dấu chấm: gõ 12.5")); // sửa vẫn sai: mẫu
        QCOMPARE(text("=1/0", mm), QStringLiteral("Không chia được cho 0."));
        QCOMPARE(text("=(1+2", mm), QStringLiteral("Ngoặc chưa khớp hoặc lồng quá 32 tầng."));
        QCOMPARE(text("=1e308*10", mm), QStringLiteral("Giá trị quá lớn hoặc quá gần 0."));
        QVERIFY(text("=1+", mm).startsWith(QStringLiteral("Biểu thức chưa đúng.")));
        QVERIFY(text("abc", mm).startsWith(QStringLiteral("Chưa đọc được độ dài.")));
        QVERIFY(text("", mm) == QStringLiteral("Nhập một giá trị."));
        ValueSpec none = mm;
        none.insunits = 0;
        QVERIFY(text("12.5m", none).startsWith(QStringLiteral("Bản vẽ chưa đặt đơn vị")));
        // Mọi mã × mọi loại ô: có câu, không rỗng.
        for (int e = 1; e <= int(ValueError::Units); ++e)
            for (int kind = 0; kind <= int(ValueKind::Real); ++kind) {
                ValueSpec s;
                s.kind = ValueKind(kind);
                QVERIFY(!errorText(ValueError(e), "x", s).isEmpty());
                QVERIFY(errorCode(ValueError(e)) != "unknown");
            }
        QVERIFY(errorText(ValueError::None, "", mm).isEmpty());
        QCOMPARE(errorCode(ValueError::None), std::string_view("ok"));
    }

    // Ngoặc lồng tới 32 tầng; sâu hơn là lỗi paren, không tràn ngăn xếp; chuỗi dài đọc theo thời gian tuyến tính.
    void nestingAndLength()
    {
        ValueSpec s;
        const auto nested = [](int depth) {
            return '=' + std::string(std::size_t(depth), '(') + '7' + std::string(std::size_t(depth), ')');
        };
        QCOMPARE(parseValue(nested(32), s).value, 7.0);
        QCOMPARE(parseValue(nested(33), s).error, ValueError::Paren);
        QCOMPARE(parseValue('=' + std::string(1'000'000, '('), s).error, ValueError::Paren);
        QCOMPARE(parseValue('=' + std::string(1'000'000, '-') + '5', s).value, 5.0); // dấu một ngôi: vòng lặp
        std::string sum = "=1";
        for (int i = 0; i < 100'000; ++i)
            sum += "+1";
        const auto t0 = std::chrono::steady_clock::now();
        QCOMPARE(parseValue(sum, s).value, 100'001.0);
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        qInfo("biểu thức 200 nghìn ký tự: %.1f ms", ms);
        QVERIFY2(!trion::test::kTimed || ms < 100, qPrintable(QString::number(ms)));
    }

    // Hiển thị lại rồi đọc lại (ô giữ chuỗi RGN-013, Enter không đổi thì giá trị lệch ≤ nửa đơn vị hiển thị): mọi loại
    // ô × LUNITS 1–5 × LUPREC 0–8 / AUNITS 0–4 × AUPREC 0–8, đơn vị bản vẽ, ANGBASE/ANGDIR.
    void roundTrip()
    {
        constexpr int kValues = trion::test::kSanitized ? 2'000 : 20'000;
        QStringList fails;
        for (int i = 0; i < kValues && fails.size() < 20; ++i) {
            const std::uint64_t z = mix(std::uint64_t(i));
            ValueSpec s;
            s.kind = ValueKind(z % 5);
            s.units.lunits = LinearUnits(1 + (z >> 8) % 5);
            s.units.luprec = int((z >> 16) % 9);
            s.units.aunits = AngularUnits((z >> 24) % 5);
            s.units.auprec = int((z >> 32) % 9);
            s.units.useAngbase = (z >> 40) & 1;
            s.units.angdir = (z >> 41) & 1;
            s.units.angbase = s.units.useAngbase ? 1.25 : 0;
            s.insunits = int((z >> 42) % 26);
            const double u = unit(mix(z));
            const double v = s.kind == ValueKind::Integer ? std::floor(u * 65536) - 32768
                             : s.kind == ValueKind::Angle ? (u - 0.5) * 20
                                                          : (u - 0.5) * std::pow(10.0, double((z >> 48) % 12));
            const std::string d = display(v, s);
            const ValueResult r = parseValue(d, s);
            double half = 0;
            if (s.kind == ValueKind::Angle) {
                const AngularUnits a = s.units.aunits;
                const double p = std::pow(10.0, -s.units.auprec);
                half = a == AngularUnits::Grads     ? 0.5 * p * std::numbers::pi / 200
                       : a == AngularUnits::Radians ? 0.5 * p
                       : a == AngularUnits::DegMinSec || a == AngularUnits::Surveyor
                           ? 0.5 * kRadPerDeg /
                                 (s.units.auprec == 0   ? 1
                                  : s.units.auprec <= 2 ? 60
                                                        : 3600)
                           : 0.5 * p * kRadPerDeg;
                // DMS 5–8: giây có số lẻ (đủ chặt); khảo sát làm tròn như DMS.
                if ((a == AngularUnits::DegMinSec || a == AngularUnits::Surveyor) && s.units.auprec > 4)
                    half = 0.5 * kRadPerDeg / 3600 * std::pow(10.0, 4 - s.units.auprec);
                const double tau = 2 * std::numbers::pi;
                if (!r || std::fabs(std::remainder(r.value - v, tau)) > half + 1e-12)
                    fails
                        << QStringLiteral("góc %1 «%2» → %3").arg(v, 0, 'g', 17).arg(show(d)).arg(r.value, 0, 'g', 17);
                continue;
            }
            const LinearUnits lu = s.kind == ValueKind::Distance ? s.units.lunits : LinearUnits::Decimal;
            const int prec = s.kind == ValueKind::Integer ? 0 : s.units.luprec;
            half = lu == LinearUnits::Architectural || lu == LinearUnits::Fractional ? 0.5 / double(1 << prec)
                   : lu == LinearUnits::Scientific ? 0.5 * std::pow(10.0, -prec) * std::max(1.0, std::fabs(v)) * 10
                                                   : 0.5 * std::pow(10.0, -prec);
            if (!r || std::fabs(r.value - v) > half * (1 + 1e-9) + 4 * DBL_EPSILON * std::fabs(v))
                fails << QStringLiteral("kind %1 L%2 P%3 U%4: %5 «%6» → %7")
                             .arg(int(s.kind))
                             .arg(int(lu))
                             .arg(prec)
                             .arg(s.insunits)
                             .arg(v, 0, 'g', 17)
                             .arg(show(d))
                             .arg(r ? QString::number(r.value, 'g', 17) : show(errorCode(r.error)));
        }
        QVERIFY2(fails.isEmpty(), qPrintable(fails.join('\n')));
    }

    // DoD-C 4 (tất định): FNV-1a từng bit của giá trị, mã lỗi và chuỗi hiển thị lại mọi ca vàng; Linux (libstdc++) và
    // Windows (libc++, UCRT) cho cùng hash. Đổi ca vàng hoặc quy tắc thì cập nhật hằng số.
    void deterministic()
    {
        std::uint64_t h = 0xCBF29CE484222325ull;
        const auto add = [&h](const void* p, std::size_t n) {
            for (std::size_t i = 0; i < n; ++i)
                h = (h ^ static_cast<const unsigned char*>(p)[i]) * 0x100000001B3ull;
        };
        for (const Case& k : cases()) {
            const ValueResult r = parseValue(k.input, k.spec);
            const std::string d = display(r.value, k.spec);
            add(&r.value, sizeof r.value);
            add(&r.error, sizeof r.error);
            add(d.data(), d.size());
        }
        QCOMPARE(QString::number(h, 16), QStringLiteral("85111fd9f71b5b14"));
    }

    // Tiêu chí 3: phân tích (cả định dạng lại) không cấp phát.
    void zeroAllocations()
    {
#if TRION_SANITIZED
        QSKIP("sanitizer: không thay operator new (kiểm ở preset dev, release, win)");
#else
        const std::vector<Case>& all = cases(); // nạp trước khi đếm
        std::size_t sink = 0;
        char b[kMaxNumText];
        const long before = g_allocs.load();
        for (const Case& k : all) {
            const ValueResult r = parseValue(k.input, k.spec);
            sink += std::size_t(r.error) + formatValue(r.value, k.spec, b);
        }
        const long hot = g_allocs.load() - before;
        QVERIFY(sink > 0);
        QCOMPARE(hot, 0L);
        const long before2 = g_allocs.load(); // bộ đếm có hoạt động
        ::operator delete(::operator new(16));
        QCOMPARE(g_allocs.load() - before2, 1L);
#endif
    }

    // Tiêu chí 3: ≤ 20 µs mỗi giá trị. Đo từng chuỗi (mọi ca vàng và ba biểu thức dài ≈ 100 ký tự), giữ lần nhanh
    // nhất qua các vòng (máy dùng chung); ngưỡng chặn trên chuỗi chậm nhất khi trion::test::kTimed.
    void benchmark()
    {
        std::vector<std::pair<std::string, ValueSpec>> in;
        for (const Case& k : cases())
            in.emplace_back(k.input, k.spec);
        ValueSpec mm;
        mm.kind = ValueKind::Distance;
        mm.insunits = 4;
        ValueSpec arch = mm;
        arch.units.lunits = LinearUnits::Architectural;
        ValueSpec deg;
        deg.kind = ValueKind::Angle;
        in.emplace_back("=((1200.5m+250cm)*3-(4'6\"/2))/7+((12.75*2)-(3.5e2/4))*1.5-0.25km+(((1+2)*3+4)*5)", mm);
        in.emplace_back("=(3'-6 1/2\"+4'1-1/2\")*2-(10'-2')/3+(1m+25.4mm)*2-((6\"*4)/2)+12'-0\"-((1'+2')*3)", arch);
        in.emplace_back("=((45d30'15\"+12.5)*2-(50g/2))/3+(0.25r*180)-((((30+15)*2)-90)+10d)*1.5+360/7", deg);
        for (std::size_t i = in.size() - 3; i < in.size(); ++i)
            QVERIFY2(parseValue(in[i].first, in[i].second), in[i].first.c_str());
        std::vector<double> best(in.size(), 1e300);
        volatile double sink = 0;
        for (int round = 0; round < kBenchRounds; ++round)
            for (std::size_t i = 0; i < in.size(); ++i) {
                const auto t0 = std::chrono::steady_clock::now();
                const double v = parseValue(in[i].first, in[i].second).value;
                const auto t1 = std::chrono::steady_clock::now();
                sink = sink + v;
                best[i] = std::min(best[i], std::chrono::duration<double, std::nano>(t1 - t0).count());
            }
        const double longest = *std::max_element(best.end() - 3, best.end());
        std::sort(best.begin(), best.end());
        const double p50 = best[best.size() / 2], p99 = best[best.size() * 99 / 100], worst = best.back();
        qInfo("%d chuỗi, lần nhanh nhất/%d vòng: p50 %.0f ns, p99 %.0f ns, chậm nhất %.0f ns; biểu thức dài %.0f ns",
              int(in.size()), kBenchRounds, p50, p99, worst, longest);
        QVERIFY2(!trion::test::kTimed || worst <= 20'000, qPrintable(QString::number(worst)));
    }
};

QTEST_GUILESS_MAIN(NumericValueParserTest)
#include "value_parser_test.moc"
