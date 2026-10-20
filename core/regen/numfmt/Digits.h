#pragma once
// Phần nội bộ dùng chung của LinearFormat.cpp và AngularFormat.cpp (không phải API công khai).

#include <bit>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace trion::regen::detail {

// Số lẻ tối đa của các kiểu thập phân: LUPREC chỉ tới 8 nhưng rtos của LISP hay gọi với 16 để lấy đủ chữ số.
inline constexpr int kMaxDecimals = 16;

// 10^0…10^22: đều biểu diễn chính xác bằng double.
inline constexpr double kPow10[] = {1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,  1e8,  1e9,  1e10, 1e11,
                                    1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};

// Ghi tuần tự vào bộ đệm người gọi; phần không vừa vẫn được đếm để trả độ dài cần.
struct Writer {
    char* p;
    std::size_t cap;
    std::size_t n = 0;

    void put(char c) noexcept
    {
        if (n < cap)
            p[n] = c;
        ++n;
    }
    void put(std::string_view s) noexcept
    {
        for (char c : s)
            put(c);
    }
    // Một ký tự Unicode (dấu thập phân, dấu nhóm do người gọi đặt) dạng UTF-8.
    void putCode(char32_t c) noexcept
    {
        if (c < 0x80) {
            put(char(c));
            return;
        }
        if (c < 0x800) {
            put(char(0xC0 | (c >> 6)));
        } else {
            if (c < 0x10000) {
                put(char(0xE0 | (c >> 12)));
            } else {
                put(char(0xF0 | ((c >> 18) & 0x07)));
                put(char(0x80 | ((c >> 12) & 0x3F)));
            }
            put(char(0x80 | ((c >> 6) & 0x3F)));
        }
        put(char(0x80 | (c & 0x3F)));
    }
    void putUInt(std::uint64_t v) noexcept
    {
        if (v < 100) { // phút, giây, inch, tử/mẫu phân số: tránh to_chars
            if (v >= 10)
                put(char('0' + v / 10));
            put(char('0' + v % 10));
            return;
        }
        char b[20];
        const auto r = std::to_chars(b, b + sizeof b, v);
        put(std::string_view(b, std::size_t(r.ptr - b)));
    }
};

inline void putNonFinite(Writer& o, double v) noexcept
{
    o.put(std::isnan(v) ? "nan" : v < 0 ? "-inf" : "inf");
}

// |x|·10^k có đúng là nửa số nguyên (tận cùng …5 chính xác) không. x = m·2^e nên 2·x·10^k = m·5^k·2^(e+k+1):
// là số nguyên lẻ khi và chỉ khi số mũ của 2 bằng 0 và, nếu k < 0, phần lẻ của m chia hết cho 5^-k.
inline bool isHalfway(double x, int k) noexcept
{
    const auto bits = std::bit_cast<std::uint64_t>(x);
    const int be = int((bits >> 52) & 0x7FF);
    std::uint64_t m = bits & ((std::uint64_t(1) << 52) - 1);
    int e = -1074;
    if (be != 0) {
        m |= std::uint64_t(1) << 52;
        e = be - 1075;
    }
    if (m == 0)
        return false;
    const int t = std::countr_zero(m);
    if (t + e + k + 1 != 0)
        return false;
    const std::uint64_t odd = m >> t;
    std::uint64_t p5 = 1;
    for (int i = 0; i < -k; ++i)
        if ((p5 *= 5) > odd)
            return false;
    return odd % p5 == 0;
}

// Số đã làm tròn: chuỗi chữ số phần nguyên (≤ 309 chữ số) và đúng p chữ số lẻ, không dấu.
struct Fixed {
    char intBuf[312];
    char fracBuf[kMaxDecimals];
    std::string_view ip, fp;
};

// Chữ số của số nguyên w ≥ 0 (double đã là số nguyên).
inline void wholeDigits(double w, Fixed& r) noexcept
{
    char* end = w < 0x1p63 ? std::to_chars(r.intBuf, r.intBuf + sizeof r.intBuf, std::uint64_t(w)).ptr
                           : std::to_chars(r.intBuf, r.intBuf + sizeof r.intBuf, w, std::chars_format::fixed, 0).ptr;
    r.ip = {r.intBuf, std::size_t(end - r.intBuf)};
}

// Làm tròn a ≥ 0 hữu hạn tới p chữ số lẻ: nửa xa số 0, trên giá trị nhị phân chính xác (không bù sai số).
inline void roundFixed(double a, int p, Fixed& r) noexcept
{
    double w = std::floor(a);
    const double f = a - w; // chính xác
    r.fp = {r.fracBuf, std::size_t(p)};
    // Đường nhanh (p ≤ 8): y = f·10^p < 2^27 nên lệch giá trị đúng ≤ 2^-27; phần lẻ của y cách 0,5 quá 1e-7 thì
    // floor(y + 0,5) chắc chắn bằng kết quả làm tròn đúng. Gần hòa hoặc p > 8: đường chính xác bên dưới.
    if (p <= 8) {
        const double y = f * kPow10[p];
        const double k = std::floor(y);
        const double d = y - k;
        if (std::fabs(d - 0.5) > 1e-7) {
            auto n = std::uint64_t(k) + (d > 0.5);
            if (n == std::uint64_t(kPow10[p])) {
                w += 1;
                n = 0;
            }
            for (int i = p - 1; i >= 0; --i, n /= 10)
                r.fracBuf[i] = char('0' + n % 10);
            wholeDigits(w, r);
            return;
        }
    }
    // to_chars cho kết quả làm tròn đúng ngoài trường hợp hòa; trường hợp hòa nhận ra bằng isHalfway và tự cộng,
    // nên không phụ thuộc quy tắc hòa của thư viện chuẩn.
    const bool tie = isHalfway(f, p);
    char t[kMaxDecimals + 4];
    // Hòa: in p + 1 chữ số là chính xác ("0.dd…d5"), bỏ chữ số 5 rồi cộng 1 vào chữ số cuối.
    std::to_chars(t, t + sizeof t, f, std::chars_format::fixed, tie ? p + 1 : p);
    bool carry = !tie && t[0] == '1';
    for (int i = 0; i < p; ++i)
        r.fracBuf[i] = t[2 + i];
    if (tie) {
        int i = p - 1;
        for (; i >= 0 && r.fracBuf[i] == '9'; --i)
            r.fracBuf[i] = '0';
        if (i >= 0)
            ++r.fracBuf[i];
        else
            carry = true;
    }
    if (carry)
        w += 1; // có phần lẻ thì w < 2^53 nên phép cộng chính xác
    wholeDigits(w, r);
}

// Bỏ số 0 theo DIMZIN/DIMAZIN: cuối (12.5000 → 12.5, 12.000 → 12), đầu (0.5 → .5; "0" đứng một mình giữ lại).
inline void suppressZeros(std::string_view& ip, std::string_view& fp, bool lead, bool trail) noexcept
{
    if (trail)
        while (!fp.empty() && fp.back() == '0')
            fp.remove_suffix(1);
    if (lead && ip == "0" && !fp.empty())
        ip = {};
}

// Phần nguyên (nhóm 3 chữ số bằng `group` nếu khác 0), dấu thập phân `sep`, phần lẻ.
inline void putDigits(Writer& o, std::string_view ip, std::string_view fp, char32_t sep, char32_t group = 0) noexcept
{
    for (std::size_t i = 0; i < ip.size(); ++i) {
        if (group && i && (ip.size() - i) % 3 == 0)
            o.putCode(group);
        o.put(ip[i]);
    }
    if (!fp.empty()) {
        o.putCode(sep ? sep : U'.'); // DIMDSEP 0 (tệp hỏng): dùng '.'
        o.put(fp);
    }
}

// Số thập phân hoàn chỉnh: dấu trừ chỉ khi kết quả làm tròn khác 0 (không in "-0.00").
inline void putFixed(Writer& o, bool neg, double a, int p, bool lead, bool trail, char32_t sep, char32_t group) noexcept
{
    Fixed r;
    roundFixed(a, p, r);
    const bool zero = r.ip == "0" && r.fp.find_first_not_of('0') == std::string_view::npos;
    std::string_view ip = r.ip, fp = r.fp;
    suppressZeros(ip, fp, lead, trail);
    if (neg && !zero)
        o.put('-');
    putDigits(o, ip, fp, sep, group);
}

} // namespace trion::regen::detail
