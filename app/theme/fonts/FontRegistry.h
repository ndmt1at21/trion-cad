#pragma once
// Font giao diện (UI0-003): IBM Plex Sans 400/500/600 và IBM Plex Mono 400/500 nhúng resource (:/trion/fonts),
// đủ 134 chữ tiếng Việt có dấu; vai trò chữ theo thang token (styleguide mục Chữ); giấy phép SIL OFL 1.1.
//
// Thứ tự khởi động: QApplication → loadFonts() → ThemeManager. ThemeManager đặt font ứng dụng theo --f-ui; font nào
// Qt đã giải trước khi nạp Plex thì giữ font dự phòng.
//
// Font dự phòng khi thiếu glyph: các họ sau Plex trong --f-ui / --f-mono (Segoe UI; Cascadia Mono, Consolas…), rồi
// font dự phòng của hệ thống, Qt chọn theo từng glyph. Plex không có ⌀ ∠ ⊥ ∥ (ký hiệu CAD): các glyph này luôn đi
// đường dự phòng; chữ tiếng Việt thì không bao giờ.
//
// Dùng (đọc ngay trong paintEvent/sizeHint, không cache, như token):
//   p.setFont(trion::theme::font(TextRole::Mono));
//   title->setFont(trion::theme::font(TextRole::Heading, Weight::Bold));

#include "core/base/error.h"

#include <QFont>
#include <QString>

#include <cstdint>

namespace trion::theme {

// Vai trò chữ = họ chữ (--f-ui hoặc --f-mono) + cỡ trên thang --fs-*; độ đậm truyền riêng (Weight).
// Nguồn: styleguide.html mục Chữ, design/prototype/css/app.css.
enum class TextRole : std::uint8_t {
    Ui,          // Sans --fs (12 px): chữ giao diện mặc định
    Label,       // Sans --fs-s (11,5 px): nhãn phụ, palette, dòng lệnh phụ, tooltip
    Caption,     // Sans --fs-xs (10,5 px): nhãn panel, chú thích
    Badge,       // Sans --fs-2xs (10 px): huy hiệu nhỏ
    Heading,     // Sans --fs-m (13 px): đầu mục
    Emphasis,    // Sans --fs-l (15 px): đoạn nhấn
    Title,       // Sans --fs-xl (18 px): tiêu đề hộp thoại lớn
    Display,     // Sans --fs-2xl (24 px): số lớn, tiêu đề màn hình
    Mono,        // Mono --fs (12 px): dòng lệnh (.cmd-in), số trong bảng (.tbl td.num)
    MonoLabel,   // Mono --fs-s (11,5 px): ô số (.numf, UI0-007)
    MonoCaption, // Mono --fs-xs (10,5 px): tọa độ thanh trạng thái, tên lệnh trong tooltip, phím tắt (kbd)
};

// Độ đậm: 400, --fw-m (500), --fw-b (600). Plex Mono chỉ nhúng 400/500: Bold trên Mono ra mặt 500 (Qt chọn mặt
// gần nhất).
enum class Weight : std::uint8_t { Regular, Medium, Bold };

// Nạp 5 font nhúng vào QFontDatabase. Gọi một lần, trên luồng UI, sau QApplication và trước ThemeManager.
// Lỗi (resource hỏng): Error{"ui", "font-load", …} nêu tệp; ứng dụng vẫn chạy bằng font dự phòng.
Expected<void> loadFonts();

// Font của vai trò theo chế độ hiện tại của ThemeManager::instance() (cỡ qua ThemeManager::font: px của token → pt
// theo DPI logic), danh sách họ chữ đủ font dự phòng.
// Giới hạn của Qt: QFont dựng ở cỡ pixel nguyên (QFontDatabasePrivate::load làm tròn qRound), nên --fs-s 11,5 px
// hiện 12 px, --fs-xs 10,5 px hiện 11 px (pointSizeF vẫn đúng token). Cỡ lẻ thật chỉ có khi tự vẽ bằng QRawFont.
QFont font(TextRole role, Weight weight = Weight::Regular);

// Văn bản giấy phép SIL OFL 1.1 của Plex (OFL.txt nhúng), cho hộp Giới thiệu.
QString fontLicense();

} // namespace trion::theme
