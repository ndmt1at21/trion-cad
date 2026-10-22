#pragma once
// UIA-004 · Tab Start (07.1 UI-A02, §9.11 kéo-thả; prototype SC.start, drawRecentThumb trong js/screens1.js, thả tệp
// trong bindCanvas của js/shell.js; css/app.css .start .start-in .tpl .big .rf .rf-grid .rf-list .learn .news .card
// .empty .drop-hint). Ba cột như .start-in: kSideWidth | phần còn lại | kAsideWidth, rộng tối đa kMaxWidth, giữa trang;
// cả trang cuộn khi cột hai bên cao hơn cửa sổ, danh sách tệp cuộn riêng.
//
//   TẠO MỚI  [TCVN A3 ngang][TCVN A3 dọc]… │ TỆP GẦN ĐÂY    [Tìm…][Ngày sửa ▾][▦|☰] │ HỌC TẬP   ▶ Chuyển từ AutoCAD…
//            [Duyệt mẫu…]                  │ ┌─────┐┌─────┐┌─────┐                  │ MẸO       Gõ tiếng Việt…
//   MỞ       [Mở tệp…]                     │ │ ảnh ││ ảnh ││ ảnh │ lưới / danh sách │ THÔNG BÁO TrionCAD 1.0.2…
//            [Mở thư mục gần đây][Phục hồi]│ │ tên ││ tên ││ tên │                  │
//   [TrionCAD Pro  Dùng thử: còn 12 ngày]  │ └─────┘└─────┘└─────┘                  │
//
// - Tạo mới (ẩn ở hồ sơ Viewer của UI0-011, 07.1 §2): mẫu TCVN, Trống (mm/inch) → macroRequested("^C^C_NEW <tệp mẫu>")
//   [đề xuất]; Mẫu của tôi → "^C^C_QNEW" (mẫu mặc định trong Tùy chọn); Duyệt mẫu… → "^C^C_NEW" (hộp chọn mẫu).
//   Mở tệp… → "^C^C_OPEN"; Phục hồi bản vẽ lỗi → "^C^C_RECOVER"; Mở thư mục gần đây: menu thư mục của tệp gần đây →
//   browseRequested(thư mục). Không tự chạy lệnh: bước integration nối macroRequested vào bộ định tuyến (INT-002).
// - Tệp gần đây (RecentFiles, lọc/sắp qua RecentFilter): lưới thẻ (ảnh 16:10, tên, ngày · cỡ · phiên bản, thư mục, ghim)
//   hoặc bảng (VirtualTableView); cả hai ảo hóa (chỉ vẽ mục đang thấy). Bấm, Enter → openRequested({đường dẫn}); tệp
//   "Không tìm thấy" thì toast cảnh báo có nút "Xóa khỏi danh sách". Ghim bấm ở góc thẻ (cột ghim của bảng). Chuột phải,
//   phím Menu, Shift+F10: Mở, Mở chỉ đọc, Mở thư mục chứa, Sao chép đường dẫn | Ghim/Bỏ ghim, Xóa khỏi danh sách (toast
//   có Hoàn tác). Ô tìm lọc không phân biệt dấu mỗi phím; sắp theo ngày sửa, tên, kích thước; kiểu xem và cách sắp lưu
//   trong Store ("start/view", "start/sort"). Trống: EmptyState "Mở bản vẽ mẫu", "Xem video 2 phút".
// - Ảnh xem trước: ThumbnailQueue cho thẻ đang thấy (đổi theo cuộn, cỡ cửa sổ, bộ lọc), điền dần khi xong; rời tab
//   (hideEvent) hủy mọi việc. Hiện tab (showEvent) kiểm lại tệp ở nền (RecentFiles::refresh).
// - Học tập, Mẹo, Thông báo (setNotifications, PLF-006), thẻ phiên bản (EditionGate: tên, huy hiệu, số ngày dùng thử
//   setTrialDays, hạn thuê bao setExpiry; nút Dùng thử/Mua mở trang nâng cấp qua EditionGate::upgrade(), Trạng thái giấy
//   phép → screenRequested("license")).
// - Thả tệp lên trang: gợi ý "Thả tệp DWG, DXF để mở" (.drop-hint) trong lúc kéo; DWG, DXF, DWT → openRequested(đường
//   dẫn theo thứ tự thả), tệp khác bị từ chối kèm toast lỗi nêu tên (như FileTabBar).
// Màu, cỡ đọc token lúc vẽ/khi ThemeManager::changed; chữ dịch lại khi LanguageChange. Rảnh không timer. Luồng UI.
//
// Dùng (bước integration, sau ThemeManager, IconProvider, RichTooltip, ToastHost, EditionGate; RecentFiles do main giữ):
//   auto* start = new trion::shell::StartPage(recent);
//   start->thumbnails().setEmbeddedSource(dwgPreview);  // DWG-026
//   start->thumbnails().setRenderSource(offscreen);     // RND-021
//   mainWindow.setRegion(Region::Canvas, stack{start, canvas});   // hiện khi tab Start (FileTabBar::kStart) đang mở
//   connect(start, &StartPage::openRequested, app, &Application::requestOpen);
//   connect(start, &StartPage::macroRequested, router, &CommandRouter::submit);
//   connect(start, &StartPage::screenRequested, screens, …);  // "license", "update", "sample", "migrate", "video-…"
//   router.add(u"NEWTAB"_s, [&] { tabs->setCurrent(FileTabBar::kStart); /* hiện start */ });  // docs/specs/commands/NEWTAB.md
//
// ponytail: tên tệp mẫu TCVN tạm (VN-008 mẫu .dwt A0–A4, CMD-241 TemplateResolver thay); "Mở chỉ đọc" phát
// openReadOnlyRequested (DOC-018 mở chỉ đọc); id của screenRequested cho video, bản vẽ mẫu do bước integration nối tới
// trang web/tệp mẫu.

#include "app/shell/start/RecentFiles.h"
#include "app/shell/start/ThumbnailQueue.h"

#include <QDate>
#include <QList>
#include <QPointer>
#include <QStringList>
#include <QWidget>

#include <cstdint>
#include <functional>

class QAbstractItemView;
class QBoxLayout;
class QLabel;
class QProgressBar;
class QScrollArea;
class QStackedWidget;

namespace trion::ui {
class Button;
class DropDown;
class EmptyState;
class Menu;
class SearchField;
class SegmentedGroup;
class VirtualTableView;
} // namespace trion::ui

namespace trion::shell {

class StartPage final : public QWidget {
    Q_OBJECT
public:
    // Một mục Thông báo (PLF-006): biểu tượng của bộ ("update", "forum"), tiêu đề, dòng phụ (nguồn · ngày), id phát qua
    // screenRequested khi bấm ("update": UI-L06; địa chỉ web: bước integration mở trình duyệt).
    struct News {
        QString icon;
        QString title;
        QString detail;
        QString id;
    };
    enum class View : std::uint8_t { Grid, List };
    using FolderOpener = std::function<bool(const QString& dir)>;

    static constexpr int kSideWidth = 240;    // .start-in cột trái
    static constexpr int kAsideWidth = 280;   // .start-in cột phải
    static constexpr int kMaxWidth = 1360;    // .start-in max-width
    static constexpr int kGutter = 28;        // .start-in gap, padding
    static constexpr int kCardMinWidth = 176; // .rf-grid minmax(176px, 1fr)
    static constexpr int kSearchWidth = 240;  // #st-q

    explicit StartPage(RecentFiles& recent, QWidget* parent = nullptr); // recent sống lâu hơn trang
    ~StartPage() override;

    ThumbnailQueue& thumbnails() { return thumbs_; }
    RecentFilter& filter() { return filter_; }
    RecentFiles& recent() { return recent_; }

    View view() const { return view_; }
    void setView(View view);
    QAbstractItemView* recentView() const; // lưới hoặc bảng đang dùng (test, trợ năng)
    // Ô ảnh của thẻ lưới: px logic (theo bề rộng thẻ hiện tại) và px thiết bị (cỡ xin ThumbnailQueue).
    QSize thumbnailSize() const;

    // ponytail: PLF-002 thay: ngày dùng thử còn lại (phiên bản Dùng thử), hạn thuê bao (Pro, Giáo dục, Doanh nghiệp;
    // không hợp lệ: không hiện câu hạn).
    void setTrialDays(int days);
    void setExpiry(QDate date);
    // ponytail: PLF-006 (feed cập nhật, tin cộng đồng) thay; rỗng: ẩn mục Thông báo.
    void setNotifications(const QList<News>& items);
    void setFolderOpener(FolderOpener opener); // test thay hàm giả; rỗng: QDesktopServices::openUrl(thư mục)

signals:
    void openRequested(const QStringList& paths);
    void openReadOnlyRequested(const QString& path);
    void browseRequested(const QString& folder); // hộp thoại Mở bắt đầu ở thư mục (CMD-237)
    void macroRequested(const QString& macro);
    void screenRequested(const QString& id);

protected:
    void paintEvent(QPaintEvent* e) override; // nền --c-base
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void changeEvent(QEvent* e) override; // LanguageChange
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dragLeaveEvent(QDragLeaveEvent* e) override;
    void dropEvent(QDropEvent* e) override;
    bool eventFilter(QObject* watched, QEvent* e) override; // phím, chuột của lưới và bảng

private:
    class Card;
    class Grid;
    class GridDelegate;
    class ListDelegate;
    class Box;

    void build();
    void retranslate();
    void applyTheme();
    void syncEdition();
    void syncFolders();
    void syncEmpty();
    void requestThumbnails();
    void open(int row); // dòng của filter_; tệp không tìm thấy: toast
    void showMenu(int row, const QPoint& globalPos);
    void removePath(const QString& path); // toast có Hoàn tác
    void openFolder(const QString& path);
    int currentRow() const;

    RecentFiles& recent_;
    RecentFilter filter_;
    ThumbnailQueue thumbs_;
    FolderOpener opener_;
    View view_ = View::Grid;
    int trialDays_ = 0;
    QDate expiry_;

    QScrollArea* scroll_ = nullptr;
    QBoxLayout* content_ = nullptr; // ba cột; lề giữ trang rộng tối đa kMaxWidth
    QList<QLabel*> headings_;       // tiêu đề mục (.start h2) theo thứ tự dựng
    QWidget* create_ = nullptr;     // mục Tạo mới
    QList<Card*> templates_;        // theo thứ tự kTemplates
    ui::Button* browse_ = nullptr;  // Duyệt mẫu…
    Card* openFile_ = nullptr;
    Card* folders_ = nullptr;
    Card* recover_ = nullptr;
    Box* edition_ = nullptr;
    QLabel* editionName_ = nullptr;
    QLabel* editionBadge_ = nullptr;
    QLabel* editionText_ = nullptr;
    QProgressBar* trial_ = nullptr;
    ui::Button* editionAction_ = nullptr;
    QLabel* recentTitle_ = nullptr;
    ui::SearchField* search_ = nullptr;
    ui::DropDown* sort_ = nullptr;
    ui::SegmentedGroup* viewToggle_ = nullptr;
    QStackedWidget* stack_ = nullptr;
    Grid* grid_ = nullptr;
    ui::VirtualTableView* list_ = nullptr;
    ui::EmptyState* empty_ = nullptr;
    ui::Button* video_ = nullptr;
    QList<Card*> learn_;
    Box* tip_ = nullptr;
    QLabel* tipText_ = nullptr;
    QWidget* newsSection_ = nullptr;
    QBoxLayout* newsList_ = nullptr;
    QLabel* dropHint_ = nullptr;
    ui::Menu* menu_ = nullptr; // menu chuột phải, thư mục gần đây: dựng lại mỗi lần mở
};

} // namespace trion::shell
