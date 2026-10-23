#pragma once
// UIJ-001 · Thiết lập giao diện của Bác sĩ Font (07.3 §1 UI-J01, UI-J04 "Khi có vấn đề font"; prototype js/palettes.js
// FD.settings.show, d.fdMuted). Hai khóa của Store (08 viết fontdoctor.showMode, fontdoctor.mutedFiles; Store đặt tên
// "nhóm/khóa"):
//   fontdoctor/showMode    "banner" (mặc định): banner trên vùng vẽ và chỉ báo trên thanh trạng thái; "indicator": chỉ
//                          chỉ báo. UIJ-005 (UI-J04) đặt; áp cho bản vẽ quét xong sau đó, không cần khởi động lại.
//   fontdoctor/mutedFiles  tệp đã chọn "Không nhắc lại với tệp này": không banner, vẫn có chỉ báo. Ghi đường dẫn
//                          chuẩn hóa (normalizedPath): đổi tên hay chuyển tệp là đường dẫn khác nên banner hiện lại.
//                          Giữ tối đa kMaxMuted tệp, mới nhất ở cuối, quá thì bỏ tệp cũ nhất.
// Đọc ở mọi luồng; ghi trên luồng của Store (luồng UI).

#include "core/base/settings/Settings.h"

#include <QString>
#include <QStringList>

#include <cstdint>

namespace trion::fontdoctor {

inline const settings::Setting<QString> kShowMode{"fontdoctor/showMode", QStringLiteral("banner")};
inline const settings::Setting<QStringList> kMutedFiles{"fontdoctor/mutedFiles", {}};

// Khi bản vẽ có vấn đề font (UI-J04): hiện banner trên vùng vẽ, hay chỉ hiện chỉ báo trên thanh trạng thái.
enum class ShowMode : std::uint8_t { Banner, Indicator };

class FontDoctorUiSettings {
public:
    static constexpr int kMaxMuted = 1000; // [đề xuất] tra 1.000 đường dẫn: vài chục µs mỗi lần mở bản vẽ

    explicit FontDoctorUiSettings(settings::Store& store = settings::Store::instance()) : store_(&store) {}

    ShowMode showMode() const; // giá trị lạ trong tệp: Banner
    void setShowMode(ShowMode mode);

    bool isMuted(const QString& path) const; // rỗng (bản vẽ chưa lưu lần nào): false
    void mute(const QString& path);          // rỗng: bỏ qua; đã có: chuyển về cuối (mới nhất)

    // Đường dẫn chuẩn hóa, không chạm đĩa (luồng UI, ổ mạng chậm): tuyệt đối, bỏ ".", "..", "/" thừa, dấu "/", Unicode
    // NFC (tên tiếng Việt từ macOS là NFD); Windows, macOS không phân biệt hoa thường. Không giải liên kết tượng trưng.
    static QString normalizedPath(const QString& path);

private:
    settings::Store* store_;
};

} // namespace trion::fontdoctor
