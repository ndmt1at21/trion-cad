#pragma once
// UIA-010 · Dải tab tệp (07.1 §5.6 UI-A09, §9.11 kéo-thả). Prototype: A.renderFileTabs, menu chuột phải, ảnh rê và
// TC.thumb trong js/shell.js; css/app.css .ft .ft-tab .ft-dot .ft-add .ft-thumb.
//
//   [⌂ Start][🔒 Nha-pho.dwg ● ×][Ket-cau.dwg ×][+]                                              [▾]
//
// - Tab Start luôn ở đầu, không đóng, không kéo được. Mỗi bản vẽ một tab: ổ khóa khi chỉ đọc (hoặc hồ sơ Viewer của
//   UI0-011), tên (dài thì "…", tab rộng tối đa kMaxTabWidth), chấm --c-accent khi chưa lưu, nút ×. Trình đọc màn
//   hình đọc "tên, chưa lưu, chỉ đọc" (không chỉ dựa vào màu, chấm).
// - Không đủ chỗ: các tab cuối ẩn đi, tab đang mở luôn hiện (như fitTabs của prototype); nút ▾ ở mép phải liệt kê mọi
//   tab. Tab không bao giờ bị co hay cắt chữ vì thiếu chỗ.
// - Bấm tab, ←→ (cả tới tab đang ẩn), next()/previous() → activateRequested. Kéo tab bản vẽ sang chỗ khác → moved.
//   Chuột giữa, nút × → commandRequested(id, "^C^C_CLOSE"); tệp chưa lưu do lệnh CLOSE hỏi, ở đây không hỏi.
// - Menu chuột phải (phím Menu, Shift+F10: menu của tab đang mở): Mới, Mở…, Lưu, Lưu tất cả | Đóng, Đóng tất cả,
//   Đóng các tab khác | Sao chép đường dẫn, Mở thư mục chứa. Mục lệnh phát commandRequested (NEW, OPEN, QSAVE; Lưu tất
//   cả: QSAVE cho từng bản vẽ chưa lưu như prototype; CLOSE, CLOSEALL; Đóng các tab khác: CLOSE cho từng tab khác).
//   Mục cần Pro (Mới, Lưu, Lưu tất cả, như prototype) có huy hiệu khi khóa và mở trang nâng cấp (EditionGate).
//   Sao chép đường dẫn, mở thư mục là việc giao diện (clipboard, trình quản lý tệp).
// - Rê tab bản vẽ kThumbDelay: hàng ảnh Model và từng layout dưới tab, bấm ảnh → activateRequested(id, layout). Ảnh
//   lấy từ nguồn setThumbnailSource trên luồng nền (sched, ưu tiên Background), về luồng UI bằng ui::post; rời tab
//   và ô ảnh thì hủy (CancelToken). Ảnh xong vào cache LRU theo dung lượng (kThumbCacheBytes) nên lần rê sau hiện ngay;
//   đang chờ là ô giữ chỗ màu nền vùng vẽ/layout, luồng UI không chờ. Chưa có nguồn: không hiện, tab có tooltip là
//   đường dẫn.
// - Thả tệp lên dải: DWG, DXF, DWT (như prototype) → openRequested(đường dẫn theo thứ tự thả); tệp khác bị từ chối kèm
//   toast lỗi nêu tên (ToastHost::instance()). Quy tắc lọc và câu từ chối dùng chung với trang Start: readDrop().
// Màu, cỡ đọc token mỗi lần vẽ, đo. Rảnh không timer: timer chỉ chạy từ lúc rê tới khi ảnh hiện hoặc ẩn. Luồng UI.
//
// Dùng (bước integration, sau ThemeManager, IconProvider, RichTooltip, ToastHost; EditionGate tạo trước dải để dải
// theo profileChanged):
//   auto* tabs = new trion::shell::FileTabBar;
//   mainWindow.setRegion(Region::FileTabs, tabs);
//   connect(tabs, &FileTabBar::commandRequested, router, …);   // kích hoạt tài liệu id rồi chạy macro (INT-002)
//   connect(tabs, &FileTabBar::activateRequested, session, …); // rồi session gọi tabs->setCurrent(id)
//   connect(tabs, &FileTabBar::openRequested, app, &Application::requestOpen);
//   Ctrl+Tab / Ctrl+Shift+Tab (Keymap "ntab", khung cửa sổ tự xử lý) → tabs->next() / tabs->previous().
//
// ponytail: kéo tab chỉ đổi chỗ, không có ảnh tab bay theo con trỏ hay hiệu ứng trượt. Ô ảnh lỗi (nguồn trả ảnh rỗng)
// giữ ô giữ chỗ.

#include "app/widgets/controls/Controls.h"
#include "core/base/sched/Scheduler.h"

#include <QCache>
#include <QHash>
#include <QImage>
#include <QPointer>
#include <QStringList>
#include <QTabBar>
#include <QTimer>
#include <QWidget>

#include <chrono>
#include <functional>

class QMimeData;

namespace trion::ui {
class Menu;
}

namespace trion::shell {

// Một tab bản vẽ.
struct DocTab {
    quint64 id = 0;        // khác FileTabBar::kStart
    QString name;          // chữ trên tab ("Nha-pho.dwg")
    QString path;          // đường dẫn đầy đủ; rỗng: chưa lưu lần nào (không sao chép, không mở thư mục được)
    bool dirty = false;    // chưa lưu: chấm sau tên
    bool readOnly = false; // ổ khóa trước tên
    QStringList layouts;   // tên layout theo thứ tự tab layout, không gồm Model: một ảnh mỗi layout khi rê
};

class FileTabBar final : public QWidget {
    Q_OBJECT
public:
    static constexpr quint64 kStart = 0;            // id của tab Start
    static constexpr QStringView kModel = u"Model"; // layout của Model space (giá trị CTAB) trong activateRequested
    static constexpr int kMaxTabWidth = 240;        // .ft-tab max-width
    static constexpr QSize kThumbSize{150, 96};     // .ft-thumb canvas, px logic
    // Rê bao lâu mới mở ảnh: lướt qua dải không bật popup; ảnh đã cache hiện cùng lúc (≤ 150 ms, tiêu chí 1).
    // Prototype 650 ms.
    static constexpr std::chrono::milliseconds kThumbDelay{100};
    static constexpr std::chrono::milliseconds kThumbGrace{200}; // rời tab tới khi ẩn ảnh: kịp đưa chuột xuống ảnh
    static constexpr qsizetype kThumbCacheBytes = 32 * 1024 * 1024; // [đề xuất] ~140 ảnh ở 200%

    // Chạy trên luồng nền, có thể nhiều lời gọi cùng lúc. size: px thiết bị. Trả QImage rỗng nếu cancel bật giữa
    // chừng hoặc lỗi. Mọi thứ nó dùng phải sống tới khi việc nền xong (sched::waitIdle).
    using ThumbnailSource =
        std::function<QImage(quint64 id, const QString& layout, QSize size, sched::CancelToken cancel)>;
    using FolderOpener = std::function<bool(const QString& dir)>;

    explicit FileTabBar(QWidget* parent = nullptr);
    ~FileTabBar() override; // hủy việc tạo ảnh còn chạy

    // ponytail: chỗ nối tạm thay phiên DOC-018 (danh sách tài liệu theo thứ tự tab, tài liệu hiện hành); có DOC-018 thì
    // dải theo sự kiện của phiên.
    void addDocument(const DocTab& doc);    // thêm vào cuối; id đã có: như updateDocument
    void updateDocument(const DocTab& doc); // theo id; bỏ ảnh đã cache của tài liệu (nội dung có thể đã đổi)
    void removeDocument(quint64 id);        // không phát activateRequested; phiên chọn tab mới bằng setCurrent
    void setCurrent(quint64 id);            // không phát activateRequested
    quint64 current() const;
    QList<quint64> documents() const; // id bản vẽ theo thứ tự tab (không gồm Start)

    // ponytail: RND-021 (render ngoài màn hình) thay nguồn ảnh; rỗng: không có ảnh khi rê.
    void setThumbnailSource(ThumbnailSource source);
    void setFolderOpener(FolderOpener opener); // test thay hàm giả; rỗng: QDesktopServices::openUrl(thư mục)

    // Tệp thả từ ngoài (dải tab, trang Start): bản vẽ DWG, DXF, DWT theo thứ tự thả; câu từ chối nêu tên các tệp khác
    // ("TrionCAD mở được tệp DWG, DXF, DWT. “a.pdf” không phải bản vẽ.", rỗng nếu không có). URL web bỏ qua: không có
    // tệp trên máy thì empty() (không nhận kéo).
    struct Drop {
        QStringList drawings;
        QString rejection;
        bool empty() const { return drawings.isEmpty() && rejection.isEmpty(); }
    };
    static Drop readDrop(const QMimeData& data);

    QTabBar* tabBar() const;         // test, trợ năng
    QWidget* thumbnailPopup() const;          // nullptr khi chưa hiện lần nào; ô ảnh là QAbstractButton có icon khi xong

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void next();     // tab kế, vòng lại (cả Start, tab đang ẩn)
    void previous(); // tab trước, vòng lại

signals:
    void activateRequested(quint64 id, const QString& layout); // layout rỗng: giữ không gian đang dùng của bản vẽ
    // Bước integration kích hoạt tài liệu id (kStart: không tài liệu) nếu chưa, rồi chạy macro qua INT-002.
    void commandRequested(quint64 id, const QString& macro);
    void openRequested(const QStringList& paths);
    void moved(quint64 id, int index); // kéo xong: index là vị trí trong documents()

protected:
    void paintEvent(QPaintEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void changeEvent(QEvent* e) override; // LanguageChange, StyleChange: chữ, cỡ
    void hideEvent(QHideEvent* e) override;
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dropEvent(QDropEvent* e) override;

private:
    class Tabs;
    class Cell;
    class Popup;
    struct ThumbKey {
        quint64 id;
        QString layout;
        QSize size; // px thiết bị
        friend bool operator==(const ThumbKey&, const ThumbKey&) = default;
        friend size_t qHash(const ThumbKey& k, size_t seed = 0)
        {
            return qHashMulti(seed, k.id, k.layout, k.size.width(), k.size.height());
        }
    };

    int indexOf(quint64 id) const;
    quint64 idAt(int index) const;
    QString label(int index) const;    // chữ trên tab: "Start" hoặc tên tệp
    QString iconName(int index) const; // "home" (Start), "lock" (chỉ đọc, Viewer) hoặc rỗng
    void syncTab(int index); // chữ, tên trợ năng, tooltip từ docs_
    void retranslate();
    void fit(); // ẩn tab không vừa, đặt chỗ tab, +, ▾
    void showMenu(int index, const QPoint& globalPos);
    void showList();
    void command(quint64 id, const QString& name); // commandRequested(id, "^C^C_" + name)
    bool viewer() const; // hồ sơ Viewer của UI0-011: mọi tab chỉ xem, "+" mở bản vẽ
    void hovered(int index); // Tabs: tab dưới chuột (-1: rời dải)
    void showThumbnails();
    void hideThumbnails();
    void thumbnailReady(quint64 id, const QString& layout, QSize size, QImage image);
    void dropThumbnails(quint64 id); // tài liệu đổi hoặc đóng: bỏ ảnh đã cache, ẩn popup của nó

    Tabs* tabs_;
    ui::Button* add_;
    ui::Button* more_;
    ui::Menu* menu_; // menu chuột phải, dựng lại mỗi lần mở
    ui::Menu* list_; // menu của ▾
    QPointer<Popup> popup_;
    QHash<quint64, DocTab> docs_;
    ThumbnailSource source_;
    FolderOpener opener_;
    QCache<ThumbKey, QImage> cache_{kThumbCacheBytes};
    sched::CancelSource cancel_; // việc tạo ảnh của popup đang hiện
    QTimer showTimer_, hideTimer_;
    static constexpr quint64 kNone = ~quint64(0);
    quint64 hoverId_ = kNone;    // tab dưới chuột đã xử lý (chờ, đang hiện hoặc đã bấm: không hiện lại)
    quint64 currentId_ = kStart; // currentChanged của QTabBar báo cả khi chỉ số đổi do kéo tab
    quint64 epoch_ = 0;          // tăng khi tài liệu đổi: ảnh của việc nền cũ không vào cache
    bool silent_ = false;        // đổi tab theo lệnh của phiên: không phát activateRequested
};

} // namespace trion::shell
