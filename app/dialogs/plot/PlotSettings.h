#pragma once
// UIH-001 · Thiết lập in dùng chung cho hộp thoại In (UI-H02), -PLOT và việc in nền: thiết lập trang tối thiểu theo
// PLT-001, thiết bị dựng sẵn và máy in hệ thống, dữ liệu bản vẽ mà hộp thoại cần, giá trị ban đầu.
//
// ponytail: chỗ nối, chưa có lõi. PLT-001 thay PageSetup bằng plot/setup/page_setup.h (đọc/ghi LAYOUT,
// ACAD_PLOTSETTINGS) và danh mục khổ bằng media_catalog.h (giữ tên trường như dưới đây); PLT-005 thay nguồn máy in
// hệ thống (khổ của driver, vùng in được); DOC/PLT-001 điền PlotContext từ bản vẽ (tạm: integration hoặc test điền).
// Đơn vị giấy chỉ mm trong hộp thoại (07.2); inch chỉ đi qua -PLOT.

#include "core/regen/numfmt/UnitContext.h"

#include <QList>
#include <QRectF>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <functional>
#include <optional>
#include <span>

namespace trion::plot {

// Vùng in (PLT-101): Layout chỉ ở layout, Giới hạn chỉ ở Model (như AutoCAD).
enum class AreaKind : std::uint8_t { Layout, Extents, Display, Limits, View, Window };
enum class Orientation : std::uint8_t { Portrait, Landscape };
enum class PaperUnits : std::uint8_t { Inches, Millimeters };
// Viewport có shade: cách in và chất lượng (Tùy chỉnh: theo dpi).
enum class ShadeMode : std::uint8_t { AsDisplayed, Wireframe, Hidden, Rendered };
enum class ShadeQuality : std::uint8_t { Draft, Normal, Maximum, Custom };

struct PlotArea {
    AreaKind kind = AreaKind::Display;
    QString view;   // View: tên view có tên
    QRectF window;  // Window: hình chữ nhật theo tọa độ bản vẽ (chuẩn hóa); rỗng: chưa chọn
    bool operator==(const PlotArea&) const = default;
};

// Tỉ lệ: Vừa khổ giấy, hoặc mm trên giấy = đơn vị bản vẽ (1:100 là 1 = 100).
struct PlotScale {
    bool fit = false;
    double mm = 1;
    double units = 1;
    bool scaleLineweights = false; // co giãn độ dày nét theo tỉ lệ
    bool operator==(const PlotScale&) const = default;
};

struct PlotOffset {
    bool centered = false;
    double x = 0; // mm
    double y = 0;
    bool operator==(const PlotOffset&) const = default;
};

// Cờ tùy chọn của thiết lập trang (PLOTSETTINGS plotLayoutFlags).
struct PlotFlags {
    bool plotStyles = true;      // in theo kiểu in (plot style)
    bool lineweights = true;     // in độ dày nét
    bool transparency = false;   // in độ trong suốt
    bool paperSpaceLast = true;  // in Paper space sau cùng (chỉ layout)
    bool hidePaperSpace = false; // ẩn đối tượng Paper space (chỉ layout)
    bool showPlotStyles = true;  // hiện kiểu in trên layout
    bool operator==(const PlotFlags&) const = default;
};

struct ShadePlot {
    ShadeMode mode = ShadeMode::AsDisplayed;
    ShadeQuality quality = ShadeQuality::Normal;
    int dpi = 300; // ShadeQuality::Custom
    bool operator==(const ShadePlot&) const = default;
};

// Thiết lập trang (PLT-001 PageSetup, tập con): thiết bị, khổ, đơn vị, vùng in, tỉ lệ, offset, xoay (hướng giấy), in
// ngược, kiểu in, cờ tùy chọn; thêm in shade của viewport.
struct PageSetup {
    QString name;      // tên thiết lập có tên; rỗng: thiết lập riêng của layout
    QString device;    // tên ổn định của thiết bị (Device::name)
    QString paper;     // tên ổn định của khổ (Paper::name)
    PaperUnits units = PaperUnits::Millimeters;
    PlotArea area;
    PlotScale scale;
    PlotOffset offset;
    Orientation orientation = Orientation::Landscape;
    bool upsideDown = false;
    QString plotStyle; // tệp CTB/STB ("monochrome.ctb"); rỗng: không dùng kiểu in
    PlotFlags flags;
    ShadePlot shade;
    bool operator==(const PageSetup&) const = default;
};

// Một lần in: thiết lập trang và các lựa chọn chỉ của lần in.
struct PlotSettings {
    PageSetup setup;
    int copies = 1;            // 1–999; in ra tệp thì 1
    bool toFile = false;       // bắt buộc với PDF, PNG của TrionCAD
    QString file;              // in ra tệp: đường dẫn tệp ra (mặc định defaultFile)
    bool background = true;    // in ở nền
    bool stamp = false;        // bật dấu in (PLOTSTAMP, P2)
    bool saveToLayout = false; // lưu thay đổi vào layout (Pro)
    bool operator==(const PlotSettings&) const = default;
};

// Khổ giấy: tên ổn định kiểu AutoCAD ("ISO_full_bleed_A1_(841.00_x_594.00_MM)"), tên hiện, cỡ mm (cạnh như thiết
// bị khai). translatable: title là chuỗi nguồn cần dịch (khổ dài "A3 kéo dài"), không thì in nguyên ("ISO A1", tên
// khổ của driver).
struct Paper {
    QString name;
    QString title;
    double width = 0;
    double height = 0;
    bool translatable = false;
};

enum class DeviceKind : std::uint8_t { Pdf, Png, System };

struct Device {
    QString name;     // tên ổn định: kPdfDevice, kPngDevice hoặc tên máy in hệ thống
    DeviceKind kind = DeviceKind::System;
    QString location; // máy in hệ thống: vị trí, cổng (QPrinterInfo::location)
    QString model;    // máy in hệ thống: hãng, đời máy
    QList<Paper> papers;
    QString defaultPaper;
    bool isFile() const { return kind != DeviceKind::System; }
};

inline constexpr QStringView kPdfDevice = u"TrionCAD PDF";
inline constexpr QStringView kPngDevice = u"TrionCAD PNG";
inline constexpr int kMaxCopies = 999;
inline constexpr int kMinDpi = 72;
inline constexpr int kMaxDpi = 1200;

// PDF và PNG của TrionCAD với khổ ISO A0–A4 và khổ dài (prototype PAPERS).
const QList<Device>& builtinDevices();
const Paper* findPaper(const Device& device, const QString& name);
// Tỉ lệ chuẩn 1:N (prototype SCALES).
std::span<const int> standardScales();

// Dữ liệu bản vẽ cho hộp thoại In. ponytail: DOC/PLT-001 điền; tạm do integration (bản vẽ mẫu) hoặc test điền.
struct PlotContext {
    QString document;                     // tên bản vẽ không đuôi: tên tệp in ra
    QString layout;                       // layout đang in; rỗng: Model
    std::optional<PageSetup> layoutSetup; // thiết lập trang của layout (name: thiết lập có tên đang gán); nullopt: chưa
    QList<PageSetup> namedSetups;         // thiết lập trang có tên của bản vẽ
    std::optional<PageSetup> previous;    // <Lần in trước> (PlotRunner::previous)
    QStringList views;                    // view có tên
    QStringList plotStyles;               // tệp CTB/STB có sẵn
    QString outputDir;                    // thư mục tệp in ra
    regen::UnitContext units;             // LUNITS, LUPREC của bản vẽ: tọa độ cửa sổ in hiện qua RGN-013
    std::uint64_t owner = 0;              // khóa document cho BackgroundJobs: đóng bản vẽ hủy việc in
    bool isLayout() const { return !layout.isEmpty(); }
};

// Giá trị ban đầu (UIH-001 tiêu chí 2): thiết lập trang của layout; chưa có thì PDF của TrionCAD, ISO A3, Màn hình
// hiện tại và Vừa khổ giấy (Model) hoặc Layout 1:1 (layout). Đã chuẩn hóa như PlotSettingsModel.
PlotSettings initialSettings(const PlotContext& context);
// Áp quy tắc liên đới (thiết bị ↔ khổ, in ra tệp, số bản, vùng in theo ngữ cảnh, canh giữa, cờ) và tên tệp ra mặc
// định. devices: danh sách thiết bị đang biết (thiết bị không có trong danh sách: giữ tên, khổ như cũ).
void normalize(PlotSettings& s, const PlotContext& context, const QList<Device>& devices);
// "<thư mục>/<bản vẽ>-<layout hoặc Model>.pdf|.png".
QString defaultFile(const PlotContext& context, const PageSetup& setup);

// Máy in hệ thống (PLT-005 thay). Mặc định QPrinterInfo khi build có Qt PrintSupport, không thì rỗng. Quét chậm (mạng,
// driver): gọi ở nền. Mọi luồng; các lần quét chạy lần lượt.
using PrinterSource = std::function<QList<Device>()>;
void setPrinterSource(PrinterSource source); // test thay; {}: về mặc định
QList<Device> scanPrinters();                // quét (chặn), lưu vào cache
QList<Device> cachedPrinters();              // lần quét gần nhất trong phiên; rỗng nếu chưa quét

} // namespace trion::plot
