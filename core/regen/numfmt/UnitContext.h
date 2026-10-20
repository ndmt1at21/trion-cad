#pragma once
// Ngữ cảnh đơn vị cho định dạng số (RGN-013). Struct thuần, người gọi điền từ biến header (DOC-010)
// hoặc từ dimstyle khi định dạng kích thước; thư viện không đọc trạng thái toàn cục nào.
// Ý nghĩa từng trường và hành vi khớp AutoCAD: docs/specs/regen/number-format.md.

#include <cstdint>

namespace trion::regen {

// Kiểu đơn vị độ dài, giá trị trùng LUNITS/DIMLUNIT (rtos dùng 1–5).
enum class LinearUnits : std::uint8_t {
    Scientific = 1,    // 1.7500E+01
    Decimal = 2,       // 17.50
    Engineering = 3,   // 1'-5.50"
    Architectural = 4, // 1'-5 1/2"
    Fractional = 5,    // 17 1/2
    WindowsDesktop = 6, // chỉ DIMLUNIT: thập phân theo dấu thập phân và dấu nhóm của vùng (regionDecimal, regionGroup)
};

// Kiểu đơn vị góc, giá trị trùng AUNITS/DIMAUNIT (angtos dùng 0–4).
enum class AngularUnits : std::uint8_t {
    Degrees = 0,   // 45.0000
    DegMinSec = 1, // 45d30'15"
    Grads = 2,     // 50.0000g
    Radians = 3,   // 0.7854r
    Surveyor = 4,  // N 45d30' E
};

// Dạng chuỗi ra.
enum class TextForm : std::uint8_t {
    Plain, // rtos, angtos, tọa độ, Properties, DYN: phân số "1/2", độ "d"
    // Nội dung MTEXT của kích thước: phân số xếp chồng theo dimfrac ("\S1/2;"), độ "%%d".
    // ponytail: dấu do người gọi đặt (dimdsep…) không được thoát kiểu MTEXT (\, {, }); thêm khi RGN-014 cần.
    Mtext,
};

struct UnitContext {
    // Độ dài
    LinearUnits lunits = LinearUnits::Decimal; // LUNITS, hoặc DIMLUNIT/DIMALTU khi định dạng kích thước
    int luprec = 4; // LUPREC (DIMDEC…): số chữ số thập phân 0–16; kiến trúc, phân số: mẫu 2^luprec, 0–8
    std::uint8_t dimzin = 0; // DIMZIN 0–15: bit 0–1 feet/inch (0–3), 4 bỏ số 0 đầu, 8 bỏ số 0 cuối
    double dimrnd = 0;       // DIMRND: làm tròn tới bội số này trước khi định dạng; 0 = tắt
    char32_t dimdsep = U'.'; // DIMDSEP: dấu thập phân (mọi kiểu trừ WindowsDesktop); rtos, tọa độ dùng '.'
    std::uint8_t dimfrac = 0; // DIMFRAC khi form = Mtext: 0 xếp ngang \S1/2;, 1 xếp chéo \S1#2;, 2 không xếp 1/2
    char32_t regionDecimal = U'.'; // DIMLUNIT 6: dấu thập phân của thiết lập vùng
    // DIMLUNIT 6: dấu nhóm nghìn của vùng, 0 = không nhóm.
    // ponytail: chỉ nhóm 3 chữ số; vùng nhóm kiểu 3;2 (Ấn Độ) cần thêm cỡ nhóm vào ngữ cảnh.
    char32_t regionGroup = 0;

    // Góc
    AngularUnits aunits = AngularUnits::Degrees; // AUNITS, hoặc DIMAUNIT
    int auprec = 0; // AUPREC (DIMADEC) 0–8; độ/phút/giây: 0 độ, 1–2 phút, 3–4 giây, 5–8 giây có 1–4 số lẻ
    std::uint8_t dimazin = 0; // DIMAZIN 0–3: 1 bỏ số 0 đầu, 2 bỏ số 0 cuối. angtos: dimazin = (DIMZIN >> 2) & 3
    double angbase = 0;  // ANGBASE (radian)
    bool angdir = false; // ANGDIR = 1: góc tăng theo chiều kim đồng hồ
    bool useAngbase = false; // đổi góc hướng sang hệ ANGBASE/ANGDIR (thanh trạng thái, DYN); angtos, kích thước: false

    bool unitmode = false; // UNITMODE = 1: feet-inch, phân số, khảo sát viết như khi nhập (1'5-1/2", N45dE)
    TextForm form = TextForm::Plain;
};

} // namespace trion::regen
