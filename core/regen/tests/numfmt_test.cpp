// RGN-013: ca vàng định dạng số (docs/specs/regen/number-format.md §6), tham chiếu số nguyên cho mọi ca hòa,
// 32 luồng, đếm cấp phát, benchmark.
#include "core/regen/numfmt/Digits.h"
#include "core/regen/numfmt/NumFormat.h"

#include <QTest>

#include <algorithm>
#include <atomic>
#include <cfloat>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <new>
#include <numbers>
#include <random>
#include <thread>
#include <vector>

// Đếm cấp phát của cả tiến trình (thay operator new toàn cục) để kiểm "0 cấp phát" của đường nóng. Dưới ASan, TSan
// không thay: runtime của sanitizer tự định nghĩa new/delete (TSan: trùng ký hiệu; ASan: Qt cấp phát bằng bản của ASan
// nên delete lệch cặp). Preset dev và release vẫn kiểm.
#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
#define TRION_COUNT_ALLOCS 0
#elif defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
#define TRION_COUNT_ALLOCS 0
#endif
#endif
#ifndef TRION_COUNT_ALLOCS
#define TRION_COUNT_ALLOCS 1
#endif
static std::atomic<long> g_allocs{0};
#if TRION_COUNT_ALLOCS
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

using namespace trion::regen;

namespace {

constexpr double kPi = std::numbers::pi;

// Cờ của ca vàng.
constexpr unsigned V = 1; // cần đối chiếu QA-110: hành vi AutoCAD suy ra, chưa có tài liệu xác nhận
constexpr unsigned U = 2; // UNITMODE = 1
constexpr unsigned R = 4; // góc cho bằng radian (mặc định: độ, test đổi sang radian)

// Ca vàng: chuỗi kỳ vọng, giá trị, kiểu đơn vị (LUNITS/AUNITS), độ chính xác, DIMZIN (góc: DIMAZIN), cờ, DIMRND.
struct Case {
    const char* want;
    double v;
    int unit, prec, zin = 0;
    unsigned flags = 0;
    double rnd = 0;
};

struct Group {
    const char* name;
    bool angle;
    UnitContext base;
    std::vector<Case> cases;
};

UnitContext with(auto&& f)
{
    UnitContext c;
    f(c);
    return c;
}

const std::vector<Group>& groups()
{
    static const std::vector<Group> all = [] {
        std::vector<Group> g;
        // Ví dụ của tài liệu AutoLISP rtos (x = 17.5), DIMZIN 0.
        g.push_back({"rtos-doc",
                     false,
                     {},
                     {
                         {"1.7500E+01", 17.5, 1, 4},
                         {"17.50", 17.5, 2, 2},
                         {"1'-5.50\"", 17.5, 3, 2},
                         {"1'-5 1/2\"", 17.5, 4, 2},
                         {"17 1/2", 17.5, 5, 2},
                         {"1'5.50\"", 17.5, 3, 2, 0, U},
                         {"1'5-1/2\"", 17.5, 4, 2, 0, U},
                         {"17-1/2", 17.5, 5, 2, 0, U},
                     }});
        g.push_back({"decimal",
                     false,
                     {},
                     {
                         {"0", 0.0, 2, 0},
                         {"0.0000", 0.0, 2, 4},
                         {"1", 1.0, 2, 0},
                         {"1.0000", 1.0, 2, 4},
                         {"12.5000", 12.5, 2, 4},
                         {"0.5000", 0.5, 2, 4},
                         {"3", 3.14159265358979, 2, 0},
                         {"3.1", 3.14159265358979, 2, 1},
                         {"3.14", 3.14159265358979, 2, 2},
                         {"3.142", 3.14159265358979, 2, 3},
                         {"3.1416", 3.14159265358979, 2, 4},
                         {"3.14159", 3.14159265358979, 2, 5},
                         {"3.141593", 3.14159265358979, 2, 6},
                         {"3.1415927", 3.14159265358979, 2, 7},
                         {"3.14159265", 3.14159265358979, 2, 8},
                         {"-3.1416", -3.14159265358979, 2, 4},
                         {"1234.57", 1234.5678, 2, 2},
                         {"1000000.0000", 1e6, 2, 4},
                         {"0.6667", 2.0 / 3, 2, 4},
                         {"0.0001", 0.0001, 2, 4},
                         {"0.0000", 0.00004, 2, 4},
                         {"0.0001", 0.00006, 2, 4},
                         {"0.0000", -0.00004, 2, 4, 0, V},
                         {"0", -0.4, 2, 0, 0, V},
                         {"0.0000", -0.0, 2, 4, 0, V},
                         {"100.00", 99.996, 2, 2},
                         {"10.0000", 9.99999999, 2, 4},
                         {"1.0000", 0.99995, 2, 4},
                         {"0.00000000", 1e-9, 2, 8},
                         {"0.00000001", 5e-9, 2, 8},
                         {"0.0001", 5e-5, 2, 4},
                         {"0.0000", 1e-300, 2, 4},
                         {"0.0000", 4.9406564584124654e-324, 2, 4},
                         {"1000000000000000.0000", 1e15, 2, 4},
                         {"123456789012345680", 123456789012345678.0, 2, 0},
                         {"100000000000000000000.00", 1e20, 2, 2},
                         // Tọa độ VN-2000 (≈ 1e9): giá trị nhị phân 2345678901.23449993…
                         {"2345678901.2345", 2345678901.2345, 2, 4},
                         {"2345678901.23449993", 2345678901.2345, 2, 8, 0, V},
                         {"-2345678901.2345", -2345678901.2345, 2, 4},
                         {"1234567.891", 1234567.891, 2, 3},
                         {"-987654321.123456", -987654321.123456, 2, 6},
                         {"1.05", 1.05, 2, 2},
                         {"7.12", 7.123, 2, 2},
                         // rtos với độ chính xác > 8 (LISP hay dùng 16)
                         {"0.3333333333333333", 1.0 / 3, 2, 16, 0, V},
                         {"0.1000000000000000", 0.1, 2, 16, 0, V},
                     }});
        // Biên .5: hòa chính xác (nửa xa số 0) và gần hòa (theo giá trị nhị phân, không bù).
        g.push_back({"tie",
                     false,
                     {},
                     {
                         {"1", 0.5, 2, 0, 0, V},
                         {"2", 1.5, 2, 0, 0, V},
                         {"3", 2.5, 2, 0, 0, V},
                         {"-3", -2.5, 2, 0, 0, V},
                         {"-1", -0.5, 2, 0, 0, V},
                         {"10", 9.5, 2, 0, 0, V},
                         {"0.13", 0.125, 2, 2, 0, V},
                         {"0.38", 0.375, 2, 2, 0, V},
                         {"-0.13", -0.125, 2, 2, 0, V},
                         {"0.3", 0.25, 2, 1, 0, V},
                         {"0.063", 0.0625, 2, 3, 0, V},
                         {"1235", 1234.5, 2, 0, 0, V},
                         {"1000000001", 1000000000.5, 2, 0, 0, V},
                         {"2345678901.3", 2345678901.25, 2, 1, 0, V},
                         {"0.0039063", 0.00390625, 2, 7, 0, V},
                         {"0.00390625", 0.00390625, 2, 8},
                         {"2.67", 2.675, 2, 2, 0, V},
                         {"1.00", 1.005, 2, 2, 0, V},
                         {"1.01", 1.015, 2, 2, 0, V},
                         {"1.02", 1.025, 2, 2, 0, V},
                         {"0.5", 0.45, 2, 1, 0, V},
                         {"0.3", 0.35, 2, 1, 0, V},
                         {"0.9", 0.95, 2, 1, 0, V},
                     }});
        g.push_back({"dimzin-decimal",
                     false,
                     {},
                     {
                         {".5000", 0.5, 2, 4, 4},      {"12.5", 12.5, 2, 4, 8},
                         {".5", 0.5, 2, 4, 12}, // ví dụ tài liệu DIMZIN
                         {"12", 12.0, 2, 4, 8},        {"0", 0.0, 2, 4, 8},
                         {"0", 0.0, 2, 4, 12, V},      {".0000", 0.0, 2, 4, 4, V},
                         {"-.5000", -0.5, 2, 4, 4, V}, {"-.5", -0.5, 2, 4, 12, V},
                         {"1.05", 1.05, 2, 4, 8},      {"100", 100.0, 2, 2, 8},
                         {"10.1", 10.1, 2, 4, 8},      {"3", 3.0, 2, 0, 8},
                         {"0", 0.001, 2, 2, 12, V},    {"0.5000", 0.5, 2, 4, 3},
                         {".5000", 0.5, 2, 4, 7},      {"12.5", 12.5, 2, 4, 11},
                         {".5", 0.5, 2, 4, 15},        {"1234567.891", 1234567.891, 2, 6, 8},
                         {".25", 0.25, 2, 8, 12},      {"0", 0.4, 2, 0, 4, V},
                     }});
        g.push_back({"scientific",
                     false,
                     {},
                     {
                         {"0.0000E+00", 0.0, 1, 4},
                         {"1.0000E+00", 1.0, 1, 4},
                         {"1.23E+05", 123456.0, 1, 2},
                         {"1.235E-04", 0.000123456, 1, 3},
                         {"-1.7500E+01", -17.5, 1, 4},
                         {"2.345679E+09", 2345678901.2345, 1, 6},
                         {"1.0000E+01", 9.99996, 1, 4},
                         {"2E+01", 17.5, 1, 0},
                         {"1.8E+01", 17.5, 1, 1},
                         {"1E+01", 9.5, 1, 0},
                         {"4.941E-324", 4.9406564584124654e-324, 1, 3},
                         {"5.0000E-01", 0.5, 1, 4, 4},
                         {"-1.0000E-10", -1e-10, 1, 4},
                         {"1.2346E+09", 1234567890.0, 1, 4},
                         {"1.00E+100", 1e100, 1, 2, 0, V},
                         {"1.00E-100", 1e-100, 1, 2, 0, V},
                         {"1.7977E+308", DBL_MAX, 1, 4, 0, V},
                         {"1.3E+00", 1.25, 1, 1, 0, V},
                         {"9E+00", 8.5, 1, 0, 0, V},
                         {"1.235E+03", 1234.5, 1, 3, 0, V},
                         {"1.23E+04", 12250.0, 1, 2, 0, V},
                         {"1.75E+01", 17.5, 1, 4, 8, V},
                         {"1E+01", 10.0, 1, 4, 8, V},
                         {"0E+00", 0.0, 1, 4, 12, V},
                         {"3.1415926536E+00", kPi, 1, 10, 0, V},
                     }});
        g.push_back({"engineering",
                     false,
                     {},
                     {
                         {"6.00\"", 6.0, 3, 2, 0},
                         {"2'", 24.0, 3, 2, 0},
                         {"0'-6.00\"", 6.0, 3, 2, 1},
                         {"2'-0.00\"", 24.0, 3, 2, 1},
                         {"0'-6.00\"", 6.0, 3, 2, 2},
                         {"2'", 24.0, 3, 2, 2},
                         {"6.00\"", 6.0, 3, 2, 3},
                         {"2'-0.00\"", 24.0, 3, 2, 3},
                         {"0'-0.00\"", 0.0, 3, 2, 1},
                         {"0.00\"", 0.0, 3, 2, 0, V},
                         {"0'", 0.0, 3, 2, 2, V},
                         {"0.00\"", 0.0, 3, 2, 3, V},
                         {"1'-6\"", 17.5, 3, 0},
                         {"2'", 23.999, 3, 2},
                         {"2'-0.00\"", 23.999, 3, 2, 1},
                         {"1'", 11.996, 3, 2},
                         {"1'-0.00\"", 11.996, 3, 2, 1},
                         {"83'-4.0000\"", 1000.0, 3, 4},
                         {"10288065'-9.50\"", 123456789.5, 3, 2},
                         {"83333333'-4.0000\"", 1e9, 3, 4},
                         {"1'-0.50\"", 12.5, 3, 2},
                         {"11.9999\"", 11.9999, 3, 4},
                         {"-1'-5.50\"", -17.5, 3, 2, 0, V},
                         {"1'-5.5\"", 17.5, 3, 2, 8, V},
                         {"2'-0\"", 24.0, 3, 2, 9, V},
                         {"1'-.50\"", 12.5, 3, 2, 4, V},
                         {".50\"", 0.5, 3, 2, 4, V},
                         {"0'6.00\"", 6.0, 3, 2, 1, U | V},
                         {"0.00\"", -0.004, 3, 2, 0, V},
                         {"5.13\"", 5.125, 3, 2, 0, V},
                     }});
        g.push_back({"architectural",
                     false,
                     {},
                     {
                         {"1/2\"", 0.5, 4, 4, 0},
                         {"0'-0 1/2\"", 0.5, 4, 4, 1, V},
                         {"0'-0 1/2\"", 0.5, 4, 4, 2, V},
                         {"1/2\"", 0.5, 4, 4, 3, V},
                         {"1'-0 1/2\"", 12.5, 4, 4, 0},
                         {"6\"", 6.0, 4, 4, 0},
                         {"0'-6\"", 6.0, 4, 4, 1},
                         {"0'-6\"", 6.0, 4, 4, 2},
                         {"6\"", 6.0, 4, 4, 3},
                         {"2'", 24.0, 4, 4, 0},
                         {"2'-0\"", 24.0, 4, 4, 1},
                         {"2'", 24.0, 4, 4, 2},
                         {"2'-0\"", 24.0, 4, 4, 3},
                         {"0\"", 0.0, 4, 4, 0, V},
                         {"0'-0\"", 0.0, 4, 4, 1},
                         {"0'", 0.0, 4, 4, 2, V},
                         {"0\"", 0.0, 4, 4, 3, V},
                         {"1'-6\"", 17.5, 4, 0},
                         {"1'-5\"", 16.5, 4, 0, 0, V},
                         {"1'-5 1/2\"", 17.4, 4, 2},
                         {"1'-5 1/4\"", 17.3, 4, 2},
                         {"1'-5 1/4\"", 17.125, 4, 2, 0, V},
                         {"1'-5 1/16\"", 17.0625, 4, 4},
                         {"1'-5 1/8\"", 17.0625, 4, 3, 0, V},
                         {"1/256\"", 0.00390625, 4, 8},
                         {"8'-4 3/4\"", 100.75, 4, 8},
                         {"1'", 11.99, 4, 4},
                         {"1'-0\"", 11.99, 4, 4, 1},
                         {"-1'-5 1/2\"", -17.5, 4, 2, 0, V},
                         {"-1/2\"", -0.5, 4, 4, 0, V},
                         {"0\"", 0.01, 4, 4, 0, V},
                         {"102880'-7 7/8\"", 1234567.875, 4, 3},
                         {"6-1/2\"", 6.5, 4, 4, 0, U | V},
                         {"1'0-1/2\"", 12.5, 4, 4, 0, U | V},
                         {"2'0\"", 24.0, 4, 4, 1, U | V},
                         {"2'6\"", 30.0, 4, 4, 0, U},
                         {"3\"", 3.0, 4, 0},
                         {"83333333'-4\"", 1e9, 4, 4},
                         {"1\"", 0.75, 4, 1},
                         {"1/2\"", 0.25, 4, 1, 0, V},
                         {"6\"", 5.999, 4, 8},
                         {"5 15/16\"", 5.9375, 4, 4},
                         {"1'-5 1/2\"", 17.5, 4, 4, 8, V},
                     }});
        g.push_back({"fractional",
                     false,
                     {},
                     {
                         {"1/2", 0.5, 5, 2},
                         {"0", 0.0, 5, 2},
                         {"17", 17.0, 5, 4},
                         {"3 3/4", 3.75, 5, 2},
                         {"4", 3.75, 5, 1},
                         {"3 1/2", 3.25, 5, 1, 0, V},
                         {"1/16", 0.0625, 5, 4},
                         {"1/8", 0.0625, 5, 3, 0, V},
                         {"21/64", 0.33, 5, 8},
                         {"171/256", 2.0 / 3, 5, 8},
                         {"1", 1 - 1e-9, 5, 8},
                         {"-3 3/4", -3.75, 5, 2, 0, V},
                         {"-1/2", -0.5, 5, 2, 0, V},
                         {"0", -0.001, 5, 2, 0, V},
                         {"1000000000 1/2", 1000000000.5, 5, 1},
                         {"12345 11/16", 12345.6789, 5, 4},
                         {"4", 3.5, 5, 0},
                         {"3", 2.5, 5, 0, 0, V},
                         {"1/2", 0.5, 5, 2, 0, U},
                         {"3-3/4", 3.75, 5, 2, 0, U},
                         {"0", 0.4, 5, 0},
                         {"123456789012345680", 123456789012345678.0, 5, 8},
                     }});
        // DIMLUNIT 6: vùng Việt Nam (thập phân ',', nhóm '.'), Pháp (nhóm NBSP), không nhóm.
        g.push_back({"windows-vi",
                     false,
                     with([](UnitContext& c) {
                         c.regionDecimal = U',';
                         c.regionGroup = U'.';
                     }),
                     {
                         {"1.234.567,89", 1234567.891, 6, 2, 0, V},
                         {"1.234,50", 1234.5, 6, 2, 0, V},
                         {"123,00", 123.0, 6, 2, 0, V},
                         {"0,5000", 0.5, 6, 4, 0, V},
                         {",5", 0.5, 6, 4, 12, V},
                         {"-1.234.567,89", -1234567.891, 6, 2, 0, V},
                         {"1.000,00", 999.999, 6, 2, 0, V},
                         {"1.000.000.000", 1e9, 6, 0, 0, V},
                         {"12,5", 12.5, 6, 4, 8, V},
                         {"100.000", 100000.0, 6, 0, 0, V},
                     }});
        g.push_back({"windows-fr",
                     false,
                     with([](UnitContext& c) {
                         c.regionDecimal = U',';
                         c.regionGroup = U'\u00A0';
                     }),
                     {
                         {"1\xC2\xA0"
                          "234,5",
                          1234.5, 6, 1, 0, V},
                         {"12\xC2\xA0"
                          "345\xC2\xA0"
                          "678,9",
                          12345678.9, 6, 1, 0, V},
                     }});
        g.push_back({"windows-nogroup", false, {}, {{"1234.5", 1234.5, 6, 1, 0, V}}});
        g.push_back({"dimrnd",
                     false,
                     {},
                     {
                         {"3.25", 3.37, 2, 2, 0, 0, 0.25},
                         {"3.50", 3.38, 2, 2, 0, 0, 0.25},
                         {"3.00", 2.6, 2, 2, 0, 0, 1.0},
                         {"3", 2.5, 2, 0, 0, V, 1.0},
                         {"0.50", 0.375, 2, 2, 0, V, 0.25},
                         {"1'-5 1/2\"", 17.3, 4, 4, 0, 0, 0.5},
                         {"1235", 1234.56, 2, 0, 0, 0, 5.0},
                         {"1230.00", 1234.56, 2, 2, 0, 0, 10.0},
                         {"-3.25", -3.37, 2, 2, 0, V, 0.25},
                         {"0.1", 0.07, 2, 1, 0, 0, 0.1},
                         {"0.0", 0.04, 2, 1, 0, 0, 0.1},
                         {"7.10", 7.123, 2, 2, 0, 0, 0.05},
                         {"3.37", 3.37, 2, 2, 0, 0, 0.0},
                         {"3.37", 3.37, 2, 2, 0, 0, -1.0},
                         {"1.2346E+09", 1234567890.4, 1, 4, 0, 0, 1.0},
                         {"2'-6\"", 29.7, 4, 0, 0, 0, 6.0},
                     }});
        g.push_back({"dimdsep-comma",
                     false,
                     with([](UnitContext& c) { c.dimdsep = U','; }),
                     {
                         {"12,50", 12.5, 2, 2},
                         {",5000", 0.5, 2, 4, 4},
                         {"12", 12.0, 2, 2, 8},
                         {"1'-5,50\"", 17.5, 3, 2, 0, V},
                         {"1,7500E+01", 17.5, 1, 4, 0, V},
                         {"1'-5 1/2\"", 17.5, 4, 2},
                     }});
        g.push_back({"dimdsep-space", false, with([](UnitContext& c) { c.dimdsep = U' '; }), {{"12 50", 12.5, 2, 2}}});
        g.push_back({"mtext-frac0",
                     false,
                     with([](UnitContext& c) { c.form = TextForm::Mtext; }),
                     {
                         {"1'-5\\S1/2;\"", 17.5, 4, 2, 0, V},
                         {"\\S1/2;\"", 0.5, 4, 4, 0, V},
                         {"1'-0\\S1/2;\"", 12.5, 4, 4, 0, V},
                         {"17\\S1/2;", 17.5, 5, 2, 0, V},
                         {"\\S3/4;", 0.75, 5, 2, 0, V},
                         {"3", 3.0, 5, 2},
                         {"17\\S1/16;", 17.0625, 5, 4, 0, V},
                         {"2'", 24.0, 4, 4},
                         {"17.50", 17.5, 2, 2},
                         {"1'-5.50\"", 17.5, 3, 2},
                     }});
        g.push_back({"mtext-frac1",
                     false,
                     with([](UnitContext& c) {
                         c.form = TextForm::Mtext;
                         c.dimfrac = 1;
                     }),
                     {
                         {"1'-5\\S1#2;\"", 17.5, 4, 2, 0, V},
                         {"17\\S1#2;", 17.5, 5, 2, 0, V},
                     }});
        g.push_back({"mtext-frac2",
                     false,
                     with([](UnitContext& c) {
                         c.form = TextForm::Mtext;
                         c.dimfrac = 2;
                     }),
                     {
                         {"1'-5 1/2\"", 17.5, 4, 2, 0, V},
                         {"17 1/2", 17.5, 5, 2, 0, V},
                     }});

        // Ví dụ của tài liệu AutoLISP angtos (a = π) và mô tả UNITMODE ("N 45d E", "N45dE").
        g.push_back({"angtos-doc",
                     true,
                     {},
                     {
                         {"180", kPi, 0, 0, 0, R},
                         {"180.0000", kPi, 0, 4, 0, R},
                         {"180d0'0\"", kPi, 1, 4, 0, R},
                         {"3.1416r", kPi, 3, 4, 0, R},
                         {"W", kPi, 4, 2, 0, R},
                         {"N 45d E", kPi / 4, 4, 0, 0, R},
                         {"N45dE", kPi / 4, 4, 0, 0, R | U},
                         {"315.0000", -kPi / 4, 0, 4, 0, R},
                     }});
        g.push_back({"degrees",
                     true,
                     {},
                     {
                         {"0", 0.0, 0, 0},
                         {"45", 45.0, 0, 0},
                         {"45.0000", 45.0, 0, 4},
                         {"90.00", 90.0, 0, 2},
                         {"30.25", 30.25, 0, 2},
                         {"359.9", 359.9, 0, 1},
                         {"315.0000", -45.0, 0, 4},
                         {"45", 405.0, 0, 0},
                         {"0", 720.0, 0, 0},
                         {"12.345678", 12.345678, 0, 6},
                         {"33.33333300", 33.333333, 0, 8},
                         {"57.2958", 1.0, 0, 4, 0, R},
                         {"0.0000", 1e-10, 0, 4, 0, R},
                         {"0.0000", -1e-17, 0, 4, 0, R | V},
                         {"360.00", 359.99999, 0, 2, 0, V},
                         {"23", 22.5, 0, 0, 0, V},
                         {".5000", 0.5, 0, 4, 1},
                         {"12.5", 12.5, 0, 4, 2},
                         {".5", 0.5, 0, 4, 3},
                         {"45", 45.0, 0, 4, 2},
                         {"0", 0.0, 0, 4, 3, V},
                         {"30.00000000", 30.0, 0, 12, 0, V},
                     }});
        g.push_back({"dms",
                     true,
                     {},
                     {
                         {"45d", 45.4, 1, 0},
                         {"45d", 45.0, 1, 0},
                         {"46d", 45.5, 1, 0, 0, V},
                         {"45d30'", 45.5, 1, 2},
                         {"45d30'", 45.5, 1, 1, 0, V},
                         {"45d30'15\"", 45.504166667, 1, 4},
                         {"45d30'15\"", 45.504166667, 1, 3, 0, V},
                         {"45d30'15.5\"", 45.50430555555556, 1, 5},
                         {"45d30'15.50\"", 45.50430555555556, 1, 6},
                         {"45d30'15.500\"", 45.50430555555556, 1, 7},
                         {"45d30'15.5000\"", 45.50430555555556, 1, 8},
                         {"45d30'15.5\"", 45.50430555555556, 1, 8, 2, V},
                         {"0d0'0\"", 0.0, 1, 4},
                         {"360d0'0\"", 359.99999, 1, 4, 0, V},
                         {"10d1'", 10.0166666667, 1, 2},
                         {"10d0'1\"", 10.000277778, 1, 4},
                         {"315d0'", -45.0, 1, 2},
                         {"0d30'", 0.5, 1, 2},
                         {"30d0'", 29.999999, 1, 2},
                         {"45d30'15\"", 45.504166667, 1, 4, 1, V},
                         {"45d30'0\"", 45.5, 1, 4},
                         {"180d0'0.0000\"", 180.0, 1, 8},
                     }});
        g.push_back({"grads-radians",
                     true,
                     {},
                     {
                         {"50g", 45.0, 2, 0},
                         {"50.0000g", 45.0, 2, 4},
                         {"200.00g", 180.0, 2, 2},
                         {"300g", -90.0, 2, 0},
                         {"0.5000g", 0.45, 2, 4},
                         {".5000g", 0.45, 2, 4, 1, V},
                         {"50g", 45.0, 2, 4, 2, V},
                         {"0.7854r", 45.0, 3, 4},
                         {"1.00r", 1.0, 3, 2, 0, R},
                         {"5.2832r", -1.0, 3, 4, 0, R},
                         {"0r", 0.0, 3, 0},
                         {".5000r", 0.5, 3, 4, 1, R | V},
                         {"1r", 1.0, 3, 4, 2, R | V},
                         {"6.2832r", 2 * kPi - 1e-9, 3, 4, 0, R},
                     }});
        g.push_back({"surveyor",
                     true,
                     {},
                     {
                         {"N 45d E", 45.0, 4, 0},
                         {"N45dE", 45.0, 4, 0, 0, U},
                         {"N 45d0' E", 45.0, 4, 2},
                         {"N 45d0'0\" E", 45.0, 4, 4},
                         {"E", 0.0, 4, 2},
                         {"N", 90.0, 4, 2},
                         {"S", 270.0, 4, 2},
                         {"W", 180.0, 4, 4},
                         {"N 60d E", 30.0, 4, 0},
                         {"N 30d W", 120.0, 4, 0},
                         {"S 60d W", 210.0, 4, 0},
                         {"S 30d E", 300.0, 4, 0},
                         {"N 45d30' W", 135.5, 4, 2},
                         {"S 44d45' W", 225.25, 4, 2},
                         {"S 45d30'15\" E", 315.504166667, 4, 4},
                         {"N 29d30'0.0\" E", 60.5, 4, 5},
                         {"N", 89.9999999, 4, 2, 0, V},
                         {"E", 359.9999999, 4, 2, 0, V},
                         {"S 45d E", -45.0, 4, 0},
                         {"N30d0'W", 120.0, 4, 2, 0, U},
                         {"N 45d0' E", 45.0, 4, 1, 0, V},
                     }});
        // ANGBASE/ANGDIR khi người gọi yêu cầu (thanh trạng thái, DYN).
        g.push_back({"angbase-90",
                     true,
                     with([](UnitContext& c) {
                         c.useAngbase = true;
                         c.angbase = kPi / 2;
                     }),
                     {
                         {"0", 90.0, 0, 0},
                         {"270", 0.0, 0, 0},
                         {"90", 180.0, 0, 0},
                         {"45d30'", 135.5, 1, 2},
                         {"1.5708r", 180.0, 3, 4},
                         {"N 45d E", 45.0, 4, 0, 0, V},
                     }});
        g.push_back({"angdir-cw",
                     true,
                     with([](UnitContext& c) {
                         c.useAngbase = true;
                         c.angdir = true;
                     }),
                     {
                         {"270", 90.0, 0, 0},
                         {"315", 45.0, 0, 0},
                         {"0", 0.0, 0, 0},
                     }});
        g.push_back({"angbase-90-cw",
                     true,
                     with([](UnitContext& c) {
                         c.useAngbase = true;
                         c.angbase = kPi / 2;
                         c.angdir = true;
                     }),
                     {
                         {"90", 0.0, 0, 0},
                         {"45", 45.0, 0, 0},
                         {"270", 180.0, 0, 0},
                     }});
        g.push_back({"angbase-off",
                     true,
                     with([](UnitContext& c) {
                         c.angbase = kPi / 2;
                         c.angdir = true;
                     }),
                     {{"90", 90.0, 0, 0}}});
        g.push_back({"angbase-30",
                     true,
                     with([](UnitContext& c) {
                         c.useAngbase = true;
                         c.angbase = kPi / 6;
                     }),
                     {{"50g", 75.0, 2, 0}}});
        g.push_back({"angle-mtext",
                     true,
                     with([](UnitContext& c) { c.form = TextForm::Mtext; }),
                     {
                         {"45%%d", 45.0, 0, 0, 0, V},
                         {"45.50%%d", 45.5, 0, 2, 0, V},
                         {"45%%d30'0\"", 45.5, 1, 4, 0, V},
                         {"50g", 45.0, 2, 0},
                         {"0.7854r", 45.0, 3, 4},
                     }});
        g.push_back({"angle-dsep",
                     true,
                     with([](UnitContext& c) { c.dimdsep = U','; }),
                     {
                         {"45,50", 45.5, 0, 2, 0, V},
                         {"45d30'15,5\"", 45.50430555555556, 1, 5, 0, V},
                         {"3,1416r", kPi, 3, 4, 0, R | V},
                     }});
        return g;
    }();
    return all;
}

UnitContext contextOf(const Group& g, const Case& k)
{
    UnitContext c = g.base;
    c.unitmode = c.unitmode || (k.flags & U);
    if (g.angle) {
        c.aunits = AngularUnits(k.unit);
        c.auprec = k.prec;
        c.dimazin = std::uint8_t(k.zin);
    } else {
        c.lunits = LinearUnits(k.unit);
        c.luprec = k.prec;
        c.dimzin = std::uint8_t(k.zin);
        c.dimrnd = k.rnd;
    }
    return c;
}

std::size_t run(const Group& g, const Case& k, std::span<char> out)
{
    const UnitContext c = contextOf(g, k);
    if (!g.angle)
        return formatLinear(k.v, c, out);
    return formatAngle(k.flags & R ? k.v : k.v * (kPi / 180), c, out);
}

QByteArray runString(const Group& g, const Case& k)
{
    char b[kMaxNumText];
    return QByteArray(b, qsizetype(run(g, k, b)));
}

} // namespace

class NumFormatTest : public QObject {
    Q_OBJECT
private slots:
    void golden_data()
    {
        QTest::addColumn<int>("group");
        QTest::addColumn<int>("index");
        int total = 0, verify = 0;
        for (int gi = 0; gi < int(groups().size()); ++gi) {
            const Group& g = groups()[std::size_t(gi)];
            for (int i = 0; i < int(g.cases.size()); ++i) {
                const bool v = g.cases[std::size_t(i)].flags & V;
                QTest::addRow("%s/%d%s", g.name, i, v ? " [đối chiếu]" : "") << gi << i;
                ++total;
                verify += v;
            }
        }
        qInfo("ca vàng: %d (chắc chắn %d, cần đối chiếu QA-110 %d)", total, total - verify, verify);
    }
    void golden()
    {
        QFETCH(int, group);
        QFETCH(int, index);
        const Group& g = groups()[std::size_t(group)];
        const Case& k = g.cases[std::size_t(index)];
        QCOMPARE(runString(g, k), QByteArray(k.want));
    }

    // Mọi x = i/1024 (nhị phân chính xác, nhiều ca hòa ở mọi độ chính xác) so với tham chiếu số nguyên:
    // |x|·10^10 = |i|·5^10 chính xác, làm tròn nửa xa số 0 bằng phép chia nguyên.
    void tiesAgainstIntegerReference()
    {
        UnitContext c;
        char b[64];
        for (int p = 0; p <= 8; ++p) {
            c.luprec = p;
            std::uint64_t unit = 1;
            for (int i = p; i < 10; ++i)
                unit *= 10;
            for (std::int64_t i = -20000; i <= 20000; ++i) {
                std::uint64_t q = std::uint64_t(i < 0 ? -i : i) * 9765625u, r = q % unit;
                q = q / unit + (2 * r >= unit);
                std::string d = std::to_string(q);
                if (p) {
                    if (d.size() <= std::size_t(p))
                        d.insert(0, std::size_t(p) + 1 - d.size(), '0');
                    d.insert(d.size() - std::size_t(p), 1, '.');
                }
                if (i < 0 && q)
                    d.insert(0, 1, '-');
                const std::size_t n = formatLinear(double(i) / 1024, c, b);
                if (std::string_view(b, n) != d)
                    QFAIL(qPrintable(QStringLiteral("%1/1024 p%2: %3 ≠ %4")
                                         .arg(i)
                                         .arg(p)
                                         .arg(QString::fromLatin1(b, qsizetype(n)), QString::fromStdString(d))));
            }
        }
    }

    // Bộ đệm nhỏ: trả độ dài đầy đủ, chỉ ghi phần vừa (như snprintf); bộ đệm rỗng không ghi gì.
    void truncatesLikeSnprintf()
    {
        UnitContext c;
        c.lunits = LinearUnits::Architectural;
        c.luprec = 2;
        char b[4] = {'x', 'x', 'x', 'x'};
        QCOMPARE(formatLinear(17.5, c, std::span<char>(b, 3)), std::size_t(9)); // 1'-5 1/2"
        QCOMPARE(QByteArray(b, 4), QByteArray("1'-x"));
        QCOMPARE(formatLinear(17.5, c, std::span<char>()), std::size_t(9));
        c.aunits = AngularUnits::Surveyor;
        c.auprec = 4;
        QCOMPARE(formatAngle(kPi / 4, c, std::span<char>(b, 2)), std::size_t(11)); // N 45d0'0" E
        QCOMPARE(QByteArray(b, 2), QByteArray("N "));
    }

    // kMaxNumText đủ cho kết quả dài nhất (±DBL_MAX, 16 số lẻ, dấu nhóm 4 byte).
    void maxLengthFits()
    {
        UnitContext c;
        c.luprec = 16;
        c.regionGroup = U'\U0001F600';
        c.regionDecimal = U'\U0001F600';
        std::size_t worst = 0;
        for (int u = 1; u <= 6; ++u) {
            c.lunits = LinearUnits(u);
            for (double v : {DBL_MAX, -DBL_MAX, -DBL_TRUE_MIN})
                worst = std::max(worst, linearString(v, c).size());
        }
        QVERIFY(worst > 700);
        QVERIFY(worst <= kMaxNumText);
        for (int u = 0; u <= 4; ++u) {
            c.aunits = AngularUnits(u);
            c.auprec = 8;
            QVERIFY(angleString(6.2831853, c).size() < 64);
        }
    }

    // Nhánh lỗi và biên của đầu vào: không hữu hạn, độ chính xác ngoài miền, mã đơn vị lạ, DIMRND hỏng.
    void invalidInputs()
    {
        UnitContext c;
        QCOMPARE(linearString(std::nan(""), c), std::string("nan"));
        QCOMPARE(linearString(HUGE_VAL, c), std::string("inf"));
        QCOMPARE(linearString(-HUGE_VAL, c), std::string("-inf"));
        QCOMPARE(angleString(std::nan(""), c), std::string("nan"));
        QCOMPARE(angleString(-HUGE_VAL, c), std::string("-inf"));
        c.useAngbase = true;
        c.angbase = std::nan("");
        QCOMPARE(angleString(1.0, c), std::string("nan"));
        c = {};
        c.luprec = -3;
        QCOMPARE(linearString(2.4, c), std::string("2"));
        c.luprec = 99;
        QCOMPARE(linearString(0.1, c), std::string("0.1000000000000000")); // kẹp 16
        c.lunits = LinearUnits::Architectural;
        QCOMPARE(linearString(0.00390625, c), std::string("1/256\"")); // kẹp 8
        c.lunits = LinearUnits(9);
        c.luprec = 2;
        QCOMPARE(linearString(17.5, c), std::string("17.50")); // ngoài 1–6: thập phân
        c.lunits = LinearUnits::Fractional;
        c.form = TextForm::Mtext;
        c.dimfrac = 7;
        QCOMPARE(linearString(17.5, c), std::string("17\\S1/2;")); // DIMFRAC lạ: xếp ngang
        c = {};
        c.luprec = 2;
        for (double r : {std::nan(""), HUGE_VAL, -0.5, 1e-320}) {
            c.dimrnd = r;
            QCOMPARE(linearString(3.37, c), std::string("3.37")); // DIMRND hỏng hoặc thương tràn: bỏ qua
        }
        c.dimdsep = 0;
        QCOMPARE(linearString(3.37, c), std::string("3.37")); // DIMDSEP 0 (tệp hỏng): dùng '.'
        c.aunits = AngularUnits(7);
        c.auprec = -1;
        QCOMPARE(angleString(kPi, c), std::string("180")); // ngoài 0–4: độ thập phân, AUPREC kẹp 0
    }

    // Hàm trả std::string cho cùng kết quả với bản ghi bộ đệm; dấu ngoài ASCII ghi UTF-8 đúng.
    void stringHelpersAndUtf8()
    {
        UnitContext c;
        c.luprec = 2;
        c.dimdsep = U'\u066B'; // dấu thập phân Ả Rập, 2 byte
        QCOMPARE(linearString(12.5, c), std::string("12\xD9\xAB"
                                                    "50"));
        c.lunits = LinearUnits::WindowsDesktop;
        c.regionDecimal = U'\u2396'; // 3 byte
        c.regionGroup = U'\u202F';   // NNBSP, 3 byte
        QCOMPARE(linearString(1234.5, c), std::string("1\xE2\x80\xAF"
                                                      "234\xE2\x8E\x96"
                                                      "50"));
        for (const Group& g : groups())
            for (const Case& k : g.cases) {
                const UnitContext kc = contextOf(g, k);
                const std::string s =
                    g.angle ? angleString(k.flags & R ? k.v : k.v * (kPi / 180), kc) : linearString(k.v, kc);
                QCOMPARE(QByteArray::fromStdString(s), runString(g, k));
            }
    }

    // 32 luồng định dạng đồng thời toàn bộ ca vàng: không trạng thái chung, kết quả không đổi (TSan sạch).
    void concurrent32Threads()
    {
        (void)groups();
        std::atomic<int> bad{0};
        std::vector<std::thread> threads;
        for (int t = 0; t < 32; ++t)
            threads.emplace_back([&bad] {
                char b[kMaxNumText];
                for (int rep = 0; rep < 8; ++rep)
                    for (const Group& g : groups())
                        for (const Case& k : g.cases)
                            if (std::string_view(b, run(g, k, b)) != k.want)
                                bad.fetch_add(1, std::memory_order_relaxed);
            });
        for (auto& th : threads)
            th.join();
        QCOMPARE(bad.load(), 0);
    }

    // Đường nóng (ghi bộ đệm người gọi) không cấp phát; hàm std::string thì có (kiểm bộ đếm hoạt động).
    void zeroAllocations()
    {
        if (!TRION_COUNT_ALLOCS)
            QSKIP("sanitizer: không thay operator new (kiểm ở preset dev, release)");
        (void)groups();
        char b[kMaxNumText];
        std::size_t sink = 0;
        const long before = g_allocs.load();
        for (const Group& g : groups())
            for (const Case& k : g.cases)
                sink += run(g, k, b);
        const long hot = g_allocs.load() - before;
        QVERIFY(sink > 0);
        QCOMPARE(hot, 0L);
        // Bộ đếm có hoạt động: gọi thẳng ::operator new (không bị trình biên dịch lược như biểu thức new). Không dùng
        // std::string: libc++ trên Windows cấp phát chuỗi trong libc++.dll, không qua operator new của tệp chạy.
        const long before2 = g_allocs.load();
        ::operator delete(::operator new(16));
        QCOMPARE(g_allocs.load() - before2, 1L);
    }

    // Đường nhanh của làm tròn thập phân so với to_chars (làm tròn đúng) trên giá trị ngẫu nhiên mọi cỡ và giá trị
    // gần hòa (k + 0,5)/10^p; bỏ ca hòa chính xác (to_chars hòa về chẵn), đã có tiesAgainstIntegerReference.
    void randomAgainstToChars()
    {
        std::mt19937_64 rng(7);
        std::uniform_real_distribution<double> mant(0.5, 1.0);
        std::uniform_int_distribution<int> ex(-40, 70);
        std::uniform_int_distribution<std::int64_t> k(0, 1000000000);
        UnitContext c;
        char b[kMaxNumText], r[kMaxNumText];
        for (int i = 0; i < 300000; ++i) {
            c.luprec = i % 9;
            double v = i % 3 ? std::ldexp(mant(rng), ex(rng)) : double(2 * k(rng) + 1) / (2 * std::pow(10.0, c.luprec));
            if (i & 1)
                v = -v;
            if (trion::regen::detail::isHalfway(v, c.luprec))
                continue;
            const std::string_view got(b, formatLinear(v, c, b));
            std::string_view want(
                r, std::size_t(std::to_chars(r, r + sizeof r, v, std::chars_format::fixed, c.luprec).ptr - r));
            if (want[0] == '-' && want.find_first_not_of("-0.") == std::string_view::npos)
                want.remove_prefix(1); // không in "-0.00"
            if (got != want)
                QFAIL(qPrintable(QStringLiteral("%1 p%2: %3 ≠ %4")
                                     .arg(v, 0, 'g', 17)
                                     .arg(c.luprec)
                                     .arg(QString::fromLatin1(got.data(), qsizetype(got.size())),
                                          QString::fromLatin1(want.data(), qsizetype(want.size())))));
        }
        // Khoa học: giá trị ngẫu nhiên và gần hòa (2k + 1)/2·10^s với k có p + 1 chữ số.
        c.lunits = LinearUnits::Scientific;
        std::uniform_int_distribution<int> sh(-12, 12);
        for (int i = 0; i < 300000; ++i) {
            c.luprec = i % 9;
            const auto lo = std::int64_t(trion::regen::detail::kPow10[c.luprec]);
            const std::int64_t m = lo + k(rng) % (9 * lo); // p + 1 chữ số, thêm ,5 thành gần hòa
            double v = i % 3 ? std::ldexp(mant(rng), ex(rng) * 2) : (double(m) + 0.5) * std::pow(10.0, sh(rng));
            if (i & 1)
                v = -v;
            char* e = std::to_chars(r, r + sizeof r, v, std::chars_format::scientific, c.luprec).ptr;
            char* ep = std::find(r, e, 'e');
            int exp = 0;
            std::from_chars(ep + 1 + (ep[1] == '+'), e, exp);
            if (trion::regen::detail::isHalfway(v, c.luprec - exp))
                continue;
            *ep = 'E';
            const std::string_view got(b, formatLinear(v, c, b)), want(r, std::size_t(e - r));
            if (got != want)
                QFAIL(qPrintable(QStringLiteral("khoa hoc %1 p%2: %3 ≠ %4")
                                     .arg(v, 0, 'g', 17)
                                     .arg(c.luprec)
                                     .arg(QString::fromLatin1(got.data(), qsizetype(got.size())),
                                          QString::fromLatin1(want.data(), qsizetype(want.size())))));
        }
    }

    // ns/giá trị đo theo lô 16 giá trị (một lần gọi đồng hồ ~25 ns). Máy dùng chung với tiến trình khác nên mỗi kiểu
    // đo 25 vòng ngắn, giữ vòng có p99 thấp nhất (nhiễu do tải ngoài, không phải do mã). Ngưỡng p99 ≤ 150 ns cho
    // toàn bộ mẫu chỉ chặn ở bản release.
    void benchmark()
    {
        struct Kind {
            const char* name;
            UnitContext c;
            bool angle;
            double lo, hi;
        };
        auto lin = [](LinearUnits u, int p, int zin = 0) {
            UnitContext c;
            c.lunits = u;
            c.luprec = p;
            c.dimzin = std::uint8_t(zin);
            return c;
        };
        auto ang = [](AngularUnits u, int p) {
            UnitContext c;
            c.aunits = u;
            c.auprec = p;
            return c;
        };
        const Kind kinds[] = {
            {"toa-do VN-2000 p4", lin(LinearUnits::Decimal, 4), false, 5e8, 2.5e9},
            {"kich-thuoc p2 dimzin8", lin(LinearUnits::Decimal, 2, 8), false, 0, 1e4},
            {"khoa-hoc p4", lin(LinearUnits::Scientific, 4), false, -1e6, 1e6},
            {"ky-thuat p4", lin(LinearUnits::Engineering, 4), false, 0, 1e5},
            {"kien-truc p4", lin(LinearUnits::Architectural, 4), false, -1e5, 1e5},
            {"phan-so p8", lin(LinearUnits::Fractional, 8), false, 0, 1e3},
            {"do p4", ang(AngularUnits::Degrees, 4), true, -10, 10},
            {"do-phut-giay p8", ang(AngularUnits::DegMinSec, 8), true, -10, 10},
            {"khao-sat p4", ang(AngularUnits::Surveyor, 4), true, -10, 10},
        };
        constexpr int kValues = 4096, kBatch = 16, kRounds = 25;
        std::mt19937_64 rng(42);
        char b[kMaxNumText];
        volatile std::size_t sink = 0;
        std::vector<double> values(kValues), all;
        QString report;
        for (const Kind& k : kinds) {
            std::uniform_real_distribution<double> dist(k.lo, k.hi);
            for (double& v : values)
                v = dist(rng);
            std::vector<double> best;
            for (int round = 0; round < kRounds; ++round) {
                std::vector<double> ns;
                for (int i = 0; i < kValues; i += kBatch) {
                    std::size_t s = 0;
                    const auto t0 = std::chrono::steady_clock::now();
                    for (int j = i; j < i + kBatch; ++j)
                        s += k.angle ? formatAngle(values[std::size_t(j)], k.c, b)
                                     : formatLinear(values[std::size_t(j)], k.c, b);
                    const auto t1 = std::chrono::steady_clock::now();
                    sink = sink + s;
                    ns.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count() / kBatch);
                }
                std::sort(ns.begin(), ns.end());
                if (best.empty() || ns[ns.size() * 99 / 100] < best[best.size() * 99 / 100])
                    best = ns;
            }
            all.insert(all.end(), best.begin(), best.end());
            report += QString::asprintf("  %-24s p50 %6.1f ns  p99 %6.1f ns\n", k.name, best[best.size() / 2],
                                        best[best.size() * 99 / 100]);
        }
        std::sort(all.begin(), all.end());
        const double bestP50 = all[all.size() / 2], bestP99 = all[all.size() * 99 / 100];
        qInfo().noquote() << "\n" + report;
        qInfo("tat ca (vong tot nhat/%d moi kieu): p50 %.1f ns, p99 %.1f ns/gia tri", kRounds, bestP50, bestP99);
#ifdef NDEBUG
        QVERIFY2(bestP99 <= 150, qPrintable(QString::number(bestP99)));
#endif
    }
};

QTEST_GUILESS_MAIN(NumFormatTest)
#include "numfmt_test.moc"
