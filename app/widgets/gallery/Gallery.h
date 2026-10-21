#pragma once
// UI0-006 · Bảng mẫu thành phần (styleguide.html mục Nút, Ô nhập, Bảng/cây, Menu/tooltip): mọi thành phần cơ bản của
// UI0-106, UI0-107, UI0-108 ở 8 trạng thái của 07.3 Phụ lục C; chọn chủ đề, mật độ, Giao diện nhẹ, ngôn ngữ (cả vi-XL)
// ngay trong trang. Mở bằng `trion --gallery` hoặc exe trion_gallery; QA-011 chụp (gallery_test giữ ảnh baseline).
//
// Trang: thanh chọn → "Dùng thử" (mỗi thành phần một bản thật, có tooltip 2 mức, Tab đi qua được) → "Trạng thái"
// (ma trận ảnh chụp thành phần × 8 trạng thái) → "Menu, tooltip" → "Bảng, cây" (ảnh trạng thái và bản thật 10.000
// dòng). Danh sách thành phần, trạng thái và lý do khi không có: docs/ui/thanh-phan.md.
//
// Ô trạng thái là ẢNH chụp, không phải widget sống: focus chỉ có ở một widget, mỗi lúc chỉ mở được một popup, còn rê
// và nhấn đổi theo chuột thật. stage() dựng từng điều khiển trong một vùng 0×0 của cửa sổ, đặt trạng thái, chụp ở
// DPR của màn hình rồi hủy. Ô focus cần cửa sổ đang active: chưa active thì chụp khi được active. Tự chụp lại khi đổi
// chủ đề, mật độ, Giao diện nhẹ (ThemeManager::changed) và DPR; đổi ngôn ngữ thì dựng lại cả trang.
//
// Dùng (sau QApplication, UiExecutor, Translator, loadFonts, ThemeManager, IconProvider, Keymap, RichTooltip như main):
//   trion::ui::GalleryWindow gallery;
//   gallery.show();
// Không có RichTooltip thì trang không gắn tooltip và không có ảnh tooltip; không có Translator thì ẩn chọn ngôn ngữ.

#include <QMainWindow>
#include <QStringList>

#include <memory>

class QScrollArea;

namespace trion::ui {

class GalleryWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit GalleryWindow(QWidget* parent = nullptr);
    ~GalleryWindow() override;

    // Chụp lại mọi ô ảnh ngay. Cửa sổ phải đang hiện và active (ô focus), không thì để đến lần active sau.
    // dpr 0: DPR của cửa sổ; test truyền 1 / 1,5 / 2 rồi render page() vào ảnh cùng DPR.
    void stage(qreal dpr = 0);
    bool isStaged() const { return !dirty_; } // đã chụp theo chủ đề, mật độ, ngôn ngữ hiện tại

    QWidget* page() const { return page_; } // nội dung cuộn của cửa sổ (QA-011, test chụp cả trang)

    // Chữ bị cắt (Truncation::toString của findTruncations) trên các điều khiển đã dựng ở lần chụp gần nhất: ô ảnh
    // không phải widget nên findTruncations(cửa sổ) không thấy chúng.
    const QStringList& stageTruncations() const { return truncations_; }

protected:
    void changeEvent(QEvent* e) override; // ActivationChange: chụp nếu còn nợ; LanguageChange: dựng lại trang
    bool event(QEvent* e) override;       // DevicePixelRatioChange: chụp lại

private:
    struct Page; // các ô ảnh của trang hiện tại
    class SampleModel;

    QWidget* buildPage();
    void rebuild();
    void scheduleStage();
    void syncBar(); // thanh chọn theo ThemeManager, Translator; cỡ bảng/cây thật theo mật độ

    QScrollArea* scroll_;
    QWidget* page_ = nullptr;
    std::unique_ptr<Page> shots_;
    std::unique_ptr<SampleModel> table_, tree_; // dữ liệu bản thật: giữ qua lần dựng lại trang
    QStringList truncations_;
    bool dirty_ = true;
    bool queued_ = false; // đã hẹn stage() ở lượt sự kiện sau
    bool rebuilding_ = false;
    bool staging_ = false;
};

} // namespace trion::ui
