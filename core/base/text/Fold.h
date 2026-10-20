#pragma once
// So khớp tìm kiếm không phân biệt dấu và hoa/thường — tập con của NT-012 cho ô lọc, gõ để nhảy
// ("lop" khớp "Lớp", "duong" khớp "ĐƯỜNG"). Chuẩn hóa, code page, so sánh có dấu thuộc NT-012.

#include <QString>
#include <QStringView>

namespace trion::text {

// Bỏ dấu (NFD rồi bỏ dấu kết hợp; đ/Đ → d), case folding. Dùng làm khóa tìm kiếm.
QString foldForSearch(QStringView s);

// needle đã fold sẵn (gọi foldForSearch một lần cho mỗi lần gõ, không phải cho mỗi dòng).
bool containsFolded(QStringView haystack, QStringView foldedNeedle);
bool startsWithFolded(QStringView haystack, QStringView foldedNeedle);

} // namespace trion::text
