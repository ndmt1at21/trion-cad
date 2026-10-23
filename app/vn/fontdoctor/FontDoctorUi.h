#pragma once
// UIJ-001 · Bác sĩ Font khi mở bản vẽ (UI-J01; 07.3 §1; prototype js/palettes.js FD.onOpen, FD.summary; js/shell.js
// renderStatus #sb-font): trạng thái của từng bản vẽ (mỗi tab tệp) cho banner trên vùng vẽ (FontDoctorBanner) và chỉ
// báo trên thanh trạng thái (FontStatusChip).
//
//   VN-004 quét bản vẽ id   scanStarted(id, đường dẫn) → scanProgress(id, 0…1)… → scanFinished(id, FontScan)
//   tab tệp đổi             setCurrent(id); FileTabBar::kStart (0): không bản vẽ
//   tab đóng                removeDocument(id)
//
// - Banner: bản vẽ quét xong có vấn đề (font thiếu hoặc chữ TCVN3/VNI), chế độ "banner" (FontDoctorUiSettings, đọc lúc
//   quét xong), tệp không ở danh sách "không nhắc lại". Ở lại tới khi Đóng (×, Esc) hay "Không nhắc lại với tệp này";
//   mỗi bản vẽ nhớ riêng: chuyển tab rồi quay lại, quét lại, banner đã đóng không hiện lại. Đang quét: chờ quét xong.
// - Chip: quét xong có vấn đề → "Font: 3 thay thế", kể cả khi banner đã đóng, tệp không nhắc lại, chế độ chỉ báo.
//   Đang quét lâu hơn kScanChipDelay → "Đang quét font" + tiến trình (bản vẽ nhỏ quét nhanh: chip không nháy). Không
//   vấn đề, chưa quét, tab Start: không chip.
// - Xem chi tiết, bấm chip → paletteRequested(Summary); Chuyển sang Unicode → paletteRequested(Convert) (bản Viewer:
//   banner mở trang nâng cấp). Không nhắc lại → FontDoctorUiSettings::mute(đường dẫn), mọi tab cùng tệp bỏ banner.
// Việc trên luồng UI mỗi lần mở bản vẽ: đổi chữ, hiện/ẩn, đặt chỗ (không đọc đĩa), ≤ 2 ms. Rảnh không timer: timer một
// lần chỉ chạy khi bản vẽ hiện hành đang quét chưa tới kScanChipDelay. Chỉ luồng UI.
//
// Dùng (integration; sau ThemeManager, IconProvider, RichTooltip, EditionGate; vùng vẽ, StatusBar của ShellAssembly):
//   auto* banner = new fontdoctor::FontDoctorBanner(canvas, gate); // con của vùng vẽ, phủ mép trên
//   banner->stackBelow(assembly.banner);                           // ngay dưới ui::CanvasBanner chung
//   auto* chip = new fontdoctor::FontStatusChip;                   // StatusBar, mục "fontDoctor" (FontStatusChip.h)
//   auto* fd = new fontdoctor::FontDoctorUi(*banner, *chip, fontdoctor::FontDoctorUiSettings(), this);
//   tab tệp đổi (cùng chỗ gọi FileTabBar::setCurrent): fd->setCurrent(id); tab đóng: fd->removeDocument(id)
//   connect(fd, &FontDoctorUi::paletteRequested, …);              // palette "fontdoc" (UIJ-002), tab Chuyển (UIJ-006)
//
// ponytail: scanStarted/scanProgress/scanFinished là chỗ nối tạm cho VN-004 (kết quả quét, tiến trình, sự kiện quét
// xong; StatusBar gọi nó là vn::scanEncodings) — VN-004 thay. Đường dẫn chỉ lấy lúc bắt đầu quét: Lưu thành… khi đang
// mở không đổi tệp được "không nhắc lại" (DOC báo đổi đường dẫn, chưa có).

#include "app/vn/fontdoctor/FontDoctorUiSettings.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include <chrono>
#include <cstdint>
#include <optional>

namespace trion::fontdoctor {

class FontDoctorBanner;
class FontStatusChip;

// Kết quả quét font một bản vẽ (VN-004): số font không có trên máy (đã thay bằng font tương đương), số đối tượng chữ
// theo bảng mã cũ. Kể cả chữ trong block lồng, attribute.
struct FontScan {
    int missingFonts = 0;
    qint64 tcvn3 = 0;
    qint64 vni = 0;

    qint64 legacy() const { return tcvn3 + vni; }
    bool hasIssues() const { return missingFonts > 0 || legacy() > 0; }
};

class FontDoctorUi final : public QObject {
    Q_OBJECT
public:
    enum class Tab : std::uint8_t { Summary, Convert }; // tab mở của palette UI-J02 (UIJ-002, UIJ-006)
    Q_ENUM(Tab)
    static constexpr std::chrono::milliseconds kScanChipDelay{500}; // 07.1 §12: thao tác > 0,5 s mới báo tiến trình

    // banner, chip do bên gọi giữ (thường là con của vùng vẽ, StatusBar); hủy trước FontDoctorUi thì được bỏ qua.
    FontDoctorUi(FontDoctorBanner& banner, FontStatusChip& chip, FontDoctorUiSettings settings = FontDoctorUiSettings(),
                 QObject* parent = nullptr);

    void scanStarted(quint64 doc, const QString& path); // path rỗng: bản vẽ chưa lưu (không "không nhắc lại" được)
    void scanProgress(quint64 doc, double fraction); // 0…1; bản vẽ chưa scanStarted: bỏ qua
    void scanFinished(quint64 doc, const FontScan& scan);
    void removeDocument(quint64 doc);
    void setCurrent(quint64 doc);

    quint64 current() const { return current_; }
    bool isScanning(quint64 doc) const;
    std::optional<FontScan> result(quint64 doc) const; // nullopt: chưa quét xong

    void setScanChipDelay(std::chrono::milliseconds delay) { delay_ = delay; } // test rút ngắn
    bool waiting() const { return timer_.isActive(); }                         // test: rảnh không timer

signals:
    void paletteRequested(trion::fontdoctor::FontDoctorUi::Tab tab);

private:
    struct Doc {
        QString path;
        std::optional<FontScan> scan; // nullopt: đang quét
        double progress = -1;         // < 0: chưa biết
        std::chrono::steady_clock::time_point started;
        bool banner = false; // quét xong: có vấn đề, chế độ banner, tệp không ở danh sách "không nhắc lại"
        bool closed = false; // người dùng đã đóng banner của bản vẽ này (giữ qua lần quét lại)
    };

    void syncBanner();
    void syncChip();
    void close(); // Đóng banner của bản vẽ hiện hành
    void mute();  // Không nhắc lại với tệp của bản vẽ hiện hành

    QPointer<FontDoctorBanner> banner_;
    QPointer<FontStatusChip> chip_;
    FontDoctorUiSettings settings_;
    QHash<quint64, Doc> docs_;
    quint64 current_ = 0;
    std::chrono::milliseconds delay_ = kScanChipDelay;
    QTimer timer_; // một lần: hết kScanChipDelay mà bản vẽ hiện hành còn quét thì hiện chip "Đang quét font"
};

} // namespace trion::fontdoctor
