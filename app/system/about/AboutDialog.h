#pragma once
// UIL-011 · Giới thiệu TrionCAD (lệnh ABOUT, màn hình UI-L13; 07.3 §6 UI-L13; 05 §15 Qt LGPL, SBOM; prototype SC.about
// trong design/prototype/js/screens2.js: hộp `about` 600). Đặc tả lệnh: docs/specs/commands/ABOUT.md.
//
// Hộp thoại không chế độ 600 px (Đóng = nút chính, Esc; F1 → ABOUT):
//   [logo] TrionCAD [huy hiệu phiên bản]           EditionBadge: Pro, Viewer, "Dùng thử: còn 12 ngày"…
//          0.1.0 · bản build dev · kênh Nội bộ     Mono --fs-s
//   ┌ THÔNG TIN HỆ THỐNG ────────────────────────┐ hệ điều hành · CPU · RAM / Đồ họa: backend · GPU · độ phân giải ·
//   │ … [Sao chép]                                │ tỉ lệ / Ngôn ngữ · Hồ sơ. Đọc ở nền (CPU, RAM; GPU qua hook RND-001),
//   └─────────────────────────────────────────────┘ chưa về thì "Đang đọc…", nút Sao chép vô hiệu
//   ┌ THƯ VIỆN MÃ NGUỒN MỞ ──────────────────────┐ bảng tên | giấy phép từ licenses.json (gen_licenses.py sinh lúc build
//   │ Qt 6 | LGPL-3.0, liên kết động …            │ từ third_party.json; CI lỗi khi thiếu văn bản). Văn bản giấy phép…
//   └─────────────────────────────────────────────┘ (Enter, nhấp đúp): hộp văn bản đầy đủ; Qt kèm cách thay thư viện Qt
//   AutoCAD® là nhãn hiệu của Autodesk, Inc. …
//   [Điều khoản sử dụng] [Chính sách riêng tư] [Liên hệ hỗ trợ]
// Không mở được trang web: dòng lỗi kèm địa chỉ (FieldMessage). Liên hệ hỗ trợ: đóng hộp này rồi mở UI-L17.
//
// Lắp (integration, DialogHost của ShellAssembly):
//   dialogs.add(u"about"_s, [this](QWidget* p) {
//       return new trion::about::AboutDialog(gate, {.graphics = …RND-001…, .profile = …, .support = …UIL-006…}, p);
//   });
//   router.add(u"ABOUT"_s, [this] { dialogs.open(u"about"_s); });
//
// ponytail: chỗ nối tạm. CPU, RAM đọc thẳng (cpuid, /proc/cpuinfo, sysconf, GlobalMemoryStatusEx, sysctl) tới khi có
// hồ sơ phần cứng NT-008; backend và GPU do RND-001 đặt Sources::graphics (chưa có: "chưa rõ"); hồ sơ hiện hành là tên
// do bên gọi đưa (NT-010 chưa có phạm vi hồ sơ); bản build và kênh từ biến CMake TRION_BUILD_ID, TRION_CHANNEL (pipeline
// phát hành 05 §17 đặt); loại giấy phép, hạn dùng chưa có (PLF-001, UIL-018); địa chỉ Điều khoản, Chính sách riêng tư,
// trang hỗ trợ là địa chỉ tạm trên trioncad.vn (UIL-021 chốt); Liên hệ hỗ trợ chưa có Sources::support thì mở trang
// hỗ trợ trên web (UIL-006 nối UI-L17).

#include "app/widgets/dialog/DialogFrame.h"
#include "core/base/error.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <functional>

class QLabel;
class QStandardItemModel;

namespace trion::edition {
class EditionGate;
}

namespace trion::ui {
class FieldMessage;
class VirtualTableView;
} // namespace trion::ui

namespace trion::about {

// Phiên bản, bản build, kênh của bản đang chạy (CMake: PROJECT_VERSION, TRION_BUILD_ID, TRION_CHANNEL).
struct BuildInfo {
    QString version; // "0.1.0"
    QString build;   // "dev", "2026.10.23.1"
    QString channel; // "internal" | "beta" | "stable"
    static BuildInfo current();
    QString channelName() const; // đã dịch: "Nội bộ", "Beta", "Ổn định"
};

// Một thư viện bên thứ ba (licenses.json, gen_licenses.py).
struct Component {
    QString id;      // "qt"
    QString name;    // "Qt 6"
    QString license; // chữ trên bảng: "LGPL-3.0", "FreeType License"
    QString spdx;    // "LGPL-3.0-only"
    QString link;    // "dynamic" (Qt) hoặc rỗng
    QString url;
    QString notice;     // dòng bản quyền
    QStringList texts;  // mã văn bản giấy phép, theo thứ tự
};

struct Licenses {
    QList<Component> components;
    QHash<QString, QString> texts; // mã → văn bản
    // Lỗi: Error{"about", "licenses", mô tả}. Mọi luồng.
    static Expected<Licenses> parse(const QByteArray& json);
    static Expected<Licenses> load(); // :/trion/about/licenses.json; mọi luồng
};

// Phần đọc ở nền của thông tin hệ thống. Mọi luồng.
struct SystemInfo {
    QString os;      // "Windows 11 Version 23H2 (x86_64)"
    QString cpu;     // tên CPU ("Intel(R) Core(TM) i5-8250U CPU @ 1.60GHz"), không có thì kiến trúc
    int threads = 0; // luồng phần cứng
    qint64 ram = 0;  // byte; 0: không đọc được
    static SystemInfo read();
};

// Backend và GPU (RND-001). Chạy ở luồng nền: không chạm widget.
struct GraphicsInfo {
    QString backend; // "D3D11", "Metal", "Vulkan", "OpenGL"
    QString adapter; // "Intel UHD Graphics 620"
};

class AboutDialog : public ui::DialogFrame {
    Q_OBJECT
public:
    struct Sources {
        std::function<GraphicsInfo()> graphics;       // RND-001, gọi ở luồng nền; rỗng: "chưa rõ"
        QString profile;                              // hồ sơ hiện hành; rỗng: "<<Mặc định>>"
        std::function<void(QWidget* parent)> support; // mở UI-L17 (UIL-006); rỗng: trang hỗ trợ trên web
        std::function<bool(const QUrl&)> openUrl;     // test thay; rỗng: QDesktopServices::openUrl
    };

    // Cần ThemeManager, IconProvider, UiExecutor. gate sống lâu hơn hộp thoại. Đọc nền bắt đầu ở lần hiện đầu.
    AboutDialog(edition::EditionGate& gate, Sources sources = {}, QWidget* parent = nullptr);
    ~AboutDialog() override; // hủy việc nền còn chờ

    // Văn bản nút Sao chép: dòng sản phẩm (phiên bản, huy hiệu, bản build, kênh) và ba dòng thông tin hệ thống.
    QString systemText() const;
    bool isLoaded() const { return systemReady_ && graphicsReady_ && licensesReady_; } // mọi phần đọc nền đã về
    const Licenses& licenses() const { return list_; }
    // Văn bản giấy phép đầy đủ của thành phần (dòng bản quyền, các văn bản; Qt: thêm cách thay thư viện Qt trước).
    static QString licenseText(const Component& c, const Licenses& all);
    void showLicense(int row); // mở hộp văn bản giấy phép của dòng (nút, Enter, nhấp đúp)

signals:
    void loaded(); // isLoaded() vừa thành true (test, QA)

protected:
    void showEvent(QShowEvent* e) override;   // lần đầu: bắt đầu đọc nền
    void changeEvent(QEvent* e) override;
    bool eventFilter(QObject* watched, QEvent* e) override; // Enter trên bảng: văn bản giấy phép, không tới nút Đóng

private:
    void startLoading();
    void openLink(const QUrl& url);
    void contactSupport();
    void restyle();
    void retranslate();
    void fitHeights();
    QStringList systemLines() const;

    edition::EditionGate& gate_;
    Sources sources_;
    sched::CancelSource cancel_;
    bool started_ = false;
    bool systemReady_ = false, graphicsReady_ = false, licensesReady_ = false; // phần đọc nền đã về
    SystemInfo sys_;
    GraphicsInfo gpu_;
    Licenses list_;
    QString loadError_; // licenses.json hỏng (không xảy ra với bản build đã qua CI)

    QWidget* logo_ = nullptr;
    QLabel* product_ = nullptr;
    QLabel* version_ = nullptr;
    QLabel* systemHead_ = nullptr; // .gh của nhóm
    QLabel* info_ = nullptr;       // ba dòng thông tin hệ thống (pre.mono)
    ui::Button* copy_ = nullptr;
    QLabel* ossHead_ = nullptr;
    QStandardItemModel* model_ = nullptr;
    ui::VirtualTableView* table_ = nullptr;
    ui::Button* viewLicense_ = nullptr;
    QLabel* trademark_ = nullptr;
    ui::Button* terms_ = nullptr;
    ui::Button* privacy_ = nullptr;
    ui::Button* support_ = nullptr;
    ui::FieldMessage* error_ = nullptr;
    QUrl failedUrl_;
    bool copied_ = false;
};

} // namespace trion::about
