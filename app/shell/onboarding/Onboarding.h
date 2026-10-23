#pragma once
// UIA-016 · Lần chạy đầu UI-A15 (07.1 UI-A15, bản V bỏ bước 3, 4, 5; prototype SC.onboarding trong js/screens1.js:
// hộp 'onb' rộng 640 px "Chào mừng đến với TrionCAD", thanh chấm bước, thẻ chọn, chân "Bước i / n · tên",
// [Bỏ qua, xem lại trong Tùy chọn] [Quay lại] [Tiếp]); modal theo cửa sổ chính như TC.dialog. Các bước dựng sẵn:
//   lang     Ngôn ngữ: Tiếng Việt / English — áp ngay (Translator::setLanguage, lưu "ui/language")
//   theme    Chủ đề: Tối / Sáng — áp ngay (ThemeManager::setTheme, lưu "ui/theme")
//   privacy  Quyền riêng tư: đồng ý gửi telemetry ẩn danh, mặc định tắt (kTelemetryLevel)
//   edition  Phiên bản, chỉ khi đang Viewer: Viewer miễn phí / dùng thử Pro 30 ngày / kích hoạt bằng key
//   tour     Hướng dẫn nhanh: nút cuối "Bắt đầu hướng dẫn" đóng hộp, chạy QuickTour trên cửa sổ chính
// Lựa chọn áp ngay khi chọn (không chờ bước cuối): bỏ qua ở bước nào cũng giữ những gì đã chọn trước đó; bước chưa
// tới giữ mặc định (telemetry tắt, Viewer).
// Bỏ qua (nút Bỏ qua, Esc, nút Đóng) ở bất kỳ bước nào: đóng hộp, ghi cờ kOnboarded, focus về cửa sổ chính (widget
// đã có focus, không có thì vùng vẽ). Hết bước cuối: cũng vậy, rồi chạy hướng dẫn nhanh.
//
// Khung bước mở rộng được (UIA-117, không sửa bước cũ): addStep(Step, trước bước id), removeStep(id) trước start():
//   onb->addStep({u"ui"_s, [] { return tr("Kiểu giao diện"); }, makeUiStylePage}, u"privacy"_s);
//   onb->addStep({u"migrate"_s, …, makeMigratePage, [] { return autocadFound(); }}, u"privacy"_s); // CUS-003
// Trang của bước tự áp lựa chọn và tự dịch lại (LanguageChange); bước Phiên bản đủ (PLF-003, PLF-004) nối
// trialRequested / activationRequested.
//
// Dùng (Application, luồng UI; cửa sổ chính đã hiện):
//   if (shell::Onboarding::needed(store))                    // lần chạy đầu
//       QTimer::singleShot(0, &window, [&] {                 // sau khung đầu: không làm chậm khởi động
//           auto* onb = new shell::Onboarding(window, store);
//           onb->setAttribute(Qt::WA_DeleteOnClose);
//           connect(onb, &shell::Onboarding::trialRequested, …);   // UI-L05 (UIL-019) / PLF-004
//           onb->start();
//       });
// "Xem lại" của Tùy chọn (UID-019): như trên, không xét needed().
//
// ponytail: gửi telemetry tạm thay PLF-008: setTelemetrySink là chỗ nối duy nhất để gói rời máy, chỉ gọi khi đã đồng ý
// (mức basic, full); PLF-008 (platform/telemetry) thay bằng Telemetry::track, giữ khóa kTelemetryLevel (UIL-005 đọc/ghi
// cùng khóa).
#include "app/widgets/dialog/DialogFrame.h"
#include "core/base/settings/Settings.h"

#include <QHash>
#include <QJsonObject>
#include <QPointer>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <functional>
#include <vector>

class QLabel;
class QVBoxLayout;

namespace trion::shell {

class MainWindow;
class QuickTour;

// Cờ "đã chạy": có thì lần sau không hiện (Tùy chọn vẫn "Xem lại" được).
inline const settings::Setting<bool> kOnboarded{"onboarding/done", false};
// Mức telemetry (UI-L09): "off" | "basic" (lỗi và hiệu năng, ẩn danh) | "full"; mặc định tắt. Bước Quyền riêng tư chỉ
// đổi off ↔ basic, giữ "full" nếu đã chọn ở UI-L09.
inline const settings::Setting<QString> kTelemetryLevel{"privacy/telemetry", QStringLiteral("off")};

class Onboarding : public ui::DialogFrame {
    Q_OBJECT
public:
    // Một bước của khung.
    struct Step {
        QString id;                     // ổn định, ASCII ("lang"): addStep(…, trước id), stepIds()
        std::function<QString()> title; // tên đã dịch cho "Bước i / n · tên"; gọi lại khi đổi ngôn ngữ
        std::function<QWidget*()> page;  // dựng trang ở lần đầu vào bước; hộp sở hữu
        std::function<bool()> visible{}; // xét lúc start(); rỗng: luôn hiện
    };
    // Lựa chọn ở bước Phiên bản.
    enum class EditionChoice : std::uint8_t { Viewer, Trial, Key };

    // window: cửa sổ chính (cha của hộp, chỗ chạy hướng dẫn); store: cờ đã chạy, mức telemetry. Cả hai sống lâu hơn
    // hộp. Chưa hiện: gọi start().
    explicit Onboarding(MainWindow& window, settings::Store& store = settings::Store::instance());
    ~Onboarding() override;

    // Lần chạy đầu: chưa có cờ kOnboarded.
    static bool needed(const settings::Store& store = settings::Store::instance());

    // Hiện hộp từ bước 1 với trang dựng lại theo cài đặt hiện tại (lần chạy đầu, "Xem lại"); đang hiện thì về bước 1.
    void start();

    // Chèn bước trước bước có id `before` (rỗng hoặc không có: cuối danh sách); bỏ bước (UIA-117 thay bước Phiên bản
    // bằng bản đủ: removeStep("edition") rồi addStep(…, "tour")). Có hiệu lực từ start() kế tiếp.
    void addStep(Step step, const QString& before = {});
    void removeStep(const QString& id);
    QStringList stepIds() const { return ids_; } // các bước của lượt đang chạy (đã lọc visible), theo thứ tự
    int currentStep() const { return current_; }                        // chỉ số trong stepIds()
    QWidget* page(const QString& id) const { return pages_.value(id); } // nullptr nếu chưa dựng

    void next(); // Tiếp; ở bước cuối: kết thúc rồi chạy hướng dẫn nhanh
    void back();
    void skip(); // Bỏ qua: kết thúc, không chạy hướng dẫn

    EditionChoice editionChoice() const;
    // Hướng dẫn nhanh đang chạy sau bước cuối (tự hủy khi xong); nullptr nếu không chạy.
    QuickTour* tour() const;

    // ponytail: chỗ nối gửi tạm thay PLF-008 (Telemetry::track tự lọc theo mức). Lúc kết thúc, nếu kTelemetryLevel là
    // "basic" hoặc "full", gọi sink một lần với {event: "onboarding", result: "done" | "skipped", step: id bước lúc kết
    // thúc}; mức khác (off, giá trị lạ): không gọi.
    void setTelemetrySink(std::function<void(const QJsonObject&)> sink) { sink_ = std::move(sink); }

    void accept() override; // nút Tiếp, Enter
    void reject() override; // Esc, nút Đóng, đóng cửa sổ: như Bỏ qua

signals:
    // Hộp đã đóng, cờ đã ghi, focus đã về cửa sổ chính; phát trước các tín hiệu dưới.
    void completed(bool skipped);
    // Kết thúc khi đã chọn ở bước Phiên bản. ponytail: UIA-117 (PLF-004 dùng thử, PLF-003 kích hoạt, UI-L05) thay.
    void trialRequested();
    void activationRequested();
    // Ngay trước hướng dẫn nhanh (chạy trên bản vẽ mẫu). ponytail: DOC-018 mở app/shell/resources/samples/…; chưa có
    // codec nên chưa có tệp mẫu, hướng dẫn chạy trên cửa sổ hiện tại.
    void sampleRequested();

protected:
    bool event(QEvent* e) override;       // LayoutRequest: cao theo trang
    void changeEvent(QEvent* e) override; // LanguageChange

private:
    void go(int index);
    void fit();
    void finish(bool tour);
    void retranslate();
    QWidget* buildPage(const QString& id);

    MainWindow& window_;
    settings::Store& store_;
    std::vector<Step> steps_;
    QStringList ids_;
    QHash<QString, QWidget*> pages_;
    int current_ = 0;
    bool finishing_ = false;
    QWidget* dots_;
    QVBoxLayout* pagesBox_;
    QLabel* stepLabel_;
    ui::Button* skip_;
    ui::Button* back_;
    ui::Button* next_ = nullptr;
    QPointer<QuickTour> tour_;
    std::function<void(const QJsonObject&)> sink_;
};

} // namespace trion::shell
