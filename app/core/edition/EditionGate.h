#pragma once
// UI0-011 · Cổng phiên bản trong UI (07.1 §1 nguyên tắc 8, §2; styleguide.html mục "Phiên bản và khóa tính năng";
// README task §4.2: PLF-002 khả năng, INT-020 lệnh, UI0-011 giao diện).
//
// EditionGate: phiên bản đang dùng, bảng khả năng 07.1 §2, tín hiệu khi giấy phép đổi lúc đang chạy (không khởi động
// lại), UiProfile cho khung chọn ribbon và workspace. bind(ActionRegistry): QAction của tính năng chưa có mang
// TipInfo::need (huy hiệu "Pro"/"Viewer Pro" ở mục menu, tooltip, ribbon; trình đọc màn hình báo khóa); bấm, phím tắt
// hay mục menu của nó không gửi lệnh tới INT-002 mà gửi yêu cầu nâng cấp kèm mã tính năng tới upgrade(). Tính năng
// khóa vẫn hiện, không ẩn; riêng Viewer, Viewer Pro dùng ribbon giản lược UI-M01 (UiProfile::Viewer).
//
// ponytail: nguồn giấy phép tạm thay PLF-001/PLF-002 (platform/license/Capabilities): setEdition đặt phiên bản (mặc
// định Viewer như khi chưa có giấy phép), không lưu, không kiểm chữ ký; bảng khả năng viết tay theo 07.1 §2 trong
// EditionGate.cpp. Có PLF-002 thì bảng sinh từ Capabilities::has/minimumEdition, setEdition thay bằng subscribe();
// API ở đây giữ nguyên. Cờ của lệnh lấy từ ActionRegistry::need (danh mục prototype) tới khi INT-101/INT-020 ghi khả
// năng riêng cho từng lệnh.
//
// Dùng (main, luồng UI, sau ActionRegistry):
//   trion::edition::EditionGate gate;                   // Viewer tới khi có giấy phép
//   gate.bind(actions);                                 // khóa QAction theo phiên bản
//   gate.upgrade().setHandler(showUpgrade);             // UI-L04 (UIL-014); chưa có: trang mua trên web
//   connect(&gate, &EditionGate::profileChanged, …);    // ribbon, workspace Viewer/đầy đủ (UIA-001, UIA-017)
//   if (gate.locked(need))                              // nút palette khóa (Controls::setLocked vẫn phát clicked)
//       gate.upgrade().request({.feature = u"QSELECT"_s, .need = need, .name = tr("Chọn nhanh")});

#include "app/widgets/tooltip/RichTooltip.h"

#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QUrl>

#include <cstdint>
#include <functional>

namespace trion::actions {
class ActionRegistry;
}

namespace trion::edition {

// Phiên bản (PLF-001, PLF-002 Edition).
enum class Edition : std::uint8_t { Viewer, ViewerPro, Pro, Trial, Education, Enterprise };
inline constexpr int kEditionCount = 6;

// Khả năng: mỗi dòng bảng 07.1 §2 một mục (PLF-002 Capability), theo thứ tự của bảng.
enum class Capability : std::uint8_t {
    View,        // mở, xem DWG/DXF; layout, viewport; zoom/pan
    Measure,     // đo khoảng cách, diện tích, góc, tọa độ
    LayerToggle, // bật/tắt layer tạm thời (từ Pro lưu được)
    Print,       // in và xuất PDF từng layout
    FontView,    // Bác sĩ Font: hiển thị đúng
    FontConvert, // Bác sĩ Font: chuyển đổi và lưu bản sao, chạy hàng loạt
    Markup,      // markup lưu ra tệp ghi chú riêng
    BatchTools,  // in hàng loạt theo khung tên, so sánh bản vẽ, đếm block, xuất bảng thống kê
    Edit,        // sửa bản vẽ, lưu DWG/DXF
    Scripting,   // LISP, script, plugin
    Fleet,       // giấy phép floating, cổng quản trị, cài đặt hàng loạt
};
inline constexpr int kCapabilityCount = 11;

// Bộ giao diện cho khung: Viewer (Viewer, Viewer Pro: ribbon giản lược UI-M01, workspace và thanh trạng thái Viewer,
// bản vẽ chỉ xem) hoặc đầy đủ. Về Viewer khi đang chạy: bản vẽ đang mở giữ nguyên, khung chuyển chỉ xem.
// ponytail: chỉ giá trị và tín hiệu; đặt bản vẽ đang mở thành chỉ xem (và quyền lưu tạm canSaveDocument của PLF-002
// cho bản đã sửa trước khi hạ cấp) là việc của DOC/INT-020.
enum class UiProfile : std::uint8_t { Viewer, Full };

// Yêu cầu nâng cấp khi bấm tính năng khóa.
struct UpgradeRequest {
    QString feature; // mã tính năng: id hành động ("LINE", "ortho") hoặc mã màn hình đặt ("QSELECT")
    ui::Edition need = ui::Edition::Pro; // bản cần (Pro, Viewer Pro)
    QString name; // tên tính năng đã dịch ("Vẽ đoạn thẳng"): tiêu đề trang, chú thích
};

// Mở trang nâng cấp: UI-L04 đăng ký xử lý (UIL-014 showUpgrade), chưa có thì mở trang mua trên web. Tự mở tối đa một
// lần cho mỗi tính năng mỗi phiên (styleguide.html, prototype SC.upgrade: shownUp); lần sau phát noticeRequested để
// nơi hiện thông báo (toast UI0-008) hiện "X có trong TrionCAD Y" kèm liên kết "Xem" gọi open(). Trạng thái "đã mở"
// chỉ trong bộ nhớ, không lưu.
class UpgradeRouter : public QObject {
    Q_OBJECT
public:
    enum class Outcome : std::uint8_t { Page, Notice }; // đã mở trang | hiện chú thích có liên kết
    using Handler = std::function<void(const UpgradeRequest&)>;
    using UrlOpener = std::function<bool(const QUrl&)>;

    explicit UpgradeRouter(QObject* parent = nullptr); // opener mặc định QDesktopServices::openUrl

    // ponytail: UI-L04 (UIL-014) chưa có; handler rỗng thì mở trang web url().
    void setHandler(Handler handler);
    void setUrlOpener(UrlOpener opener); // test thay hàm giả; rỗng: mặc định

    // Tính năng khóa được bấm. Lần đầu của feature trong phiên: mở trang (Page). Lần sau, hoặc mở trang web thất bại:
    // phát noticeRequested, trả Notice.
    Outcome request(const UpgradeRequest& r);
    // Mở trang ngay, bỏ qua giới hạn mỗi phiên (liên kết "Xem" của chú thích, chip của dòng lệnh). false nếu không mở
    // được trang web.
    bool open(const UpgradeRequest& r);

    // ponytail: trang mua của prototype (thanh-toan.html#pro, #vp) kèm mã tính năng; địa chỉ thật do trang mua UI-L16
    // (UIL-021) chốt.
    static QUrl url(const UpgradeRequest& r);
    static QString noticeText(const UpgradeRequest& r); // "Vẽ đoạn thẳng có trong TrionCAD Pro."

signals:
    void noticeRequested(const trion::edition::UpgradeRequest& request);

private:
    Handler handler_;
    UrlOpener opener_;
    QSet<QString> shown_; // tính năng đã mở trang trong phiên
};

class EditionGate : public QObject {
    Q_OBJECT
public:
    explicit EditionGate(QObject* parent = nullptr); // luồng UI; phiên bản Viewer
    ~EditionGate() override;                         // gỡ cổng khỏi ActionRegistry đã bind

    // EditionGate tạo gần nhất còn sống (main tạo một; test tạo riêng); nullptr nếu chưa có.
    static EditionGate* instance();

    Edition edition() const { return edition_; }
    // Đổi phiên bản lúc đang chạy (kích hoạt, dùng thử hết hạn…): cập nhật khóa của mọi QAction đã bind (huy hiệu
    // menu, tooltip, ribbon đổi theo), rồi phát editionChanged và profileChanged (nếu hồ sơ đổi), đồng bộ. Trùng
    // phiên bản hiện tại: không làm gì.
    void setEdition(Edition e);

    bool has(Capability c) const { return has(edition_, c); }
    static bool has(Edition e, Capability c);
    // Bản thấp nhất có c theo Viewer < Viewer Pro < Pro < Doanh nghiệp (câu "… có trong TrionCAD Pro").
    static Edition minimumEdition(Capability c);

    UiProfile profile() const { return profile(edition_); }
    static UiProfile profile(Edition e);

    // Phần tử có cờ phiên bản (ActionRegistry::need, nút, hàng palette) bị khóa ở phiên bản hiện tại: cờ Viewer Pro
    // theo các dòng Viewer Pro của bảng, cờ Pro theo dòng sửa bản vẽ; None không bao giờ khóa.
    bool locked(ui::Edition need) const;

    // Gắn cổng vào registry (sống tới khi EditionGate hủy hoặc bind lần sau): đặt TipInfo::need theo phiên bản cho mọi
    // QAction, ActionRegistry::setGate chặn hành động khóa và gửi UpgradeRequest{id, need, tên} tới upgrade().
    void bind(actions::ActionRegistry& registry);

    UpgradeRouter& upgrade() { return upgrade_; }

    // "Có trong TrionCAD Pro": trình đọc màn hình của huy hiệu, mô tả trợ năng của điều khiển khóa, nhãn đầu trang
    // nâng cấp. Rỗng nếu need là None.
    static QString availableIn(ui::Edition need);

    // Dòng lệnh khi lệnh bị khóa (07.1 §2): "Lệnh LINE có trong TrionCAD Pro. [Dùng thử 30 ngày] [Tìm hiểu]".
    // UIA-007 ghi text vào lịch sử, hai chip bấm được gọi upgrade().open() (ponytail: chip dùng thử mở UI-L05 khi có
    // UIL-019, PLF-004).
    struct CommandNotice {
        QString text;  // "Lệnh LINE có trong TrionCAD Pro."
        QString trial; // "Dùng thử 30 ngày"
        QString learn; // "Tìm hiểu"
    };
    static CommandNotice commandNotice(const QString& command, ui::Edition need);

signals:
    void editionChanged(trion::edition::Edition edition);
    void profileChanged(trion::edition::UiProfile profile);

private:
    bool allow(const QString& id); // cổng của ActionRegistry
    void applyLocks();

    Edition edition_ = Edition::Viewer;
    QPointer<actions::ActionRegistry> registry_;
    UpgradeRouter upgrade_;
};

} // namespace trion::edition
