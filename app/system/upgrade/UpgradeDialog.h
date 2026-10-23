#pragma once
// UIL-014 · Trang nâng cấp UI-L04 (07.3 §6 UI-L04; 07.1 §2 hành vi cổng tính năng; prototype SC.upgrade trong
// design/prototype/js/screens2.js: hộp `up` 560, PRICE, BENEFIT, shownUp).
//
// Bấm tính năng khóa → EditionGate::upgrade() (UpgradeRouter, UI0-011: tự mở tối đa một lần mỗi tính năng mỗi phiên,
// lần sau phát noticeRequested) → handler UpgradePage::showUpgrade → hộp thoại modal 560 px, không nút Trợ giúp:
//   [ảnh 150×150]  [Có trong TrionCAD Pro]                         thẻ .tag: Pro --c-accent, Viewer Pro --c-info
//                  Tên tính năng                                    --fs-xl đậm
//                  Lợi ích 1–2 câu                                  --c-text-2
//                  Từ 2.990.000 đ/năm                               --fs-l đậm
//                  hoặc 349.000 đ/tháng · 7.990.000 đ vĩnh viễn     --fs-s, --c-text-3
//   chân: [Để sau] [Đã có key? Kích hoạt] ……………… [↗ Mua] [Dùng thử 30 ngày]
// Nội dung (lợi ích vi/en, ảnh, giá theo đồng) đọc từ upgrade-content.json (nhúng :/trion/upgrade/), cập nhật theo bản
// phát hành; tính năng không có mục dùng nội dung chung của phiên bản. Chữ giao diện qua tr(), số tiền qua UiFormat.
// Đổi ngôn ngữ khi đang mở: dịch lại cả nội dung JSON (vi/en; vi-XL dùng vi).
//   Mua (mở web)   trang mua UI-L16 chọn sẵn gói (UpgradeRouter::url: #pro, #vp, kèm mã tính năng) rồi đóng; không mở
//                  được trình duyệt thì ở lại, báo lỗi kèm địa chỉ (FieldMessage, được đọc).
//   Để sau, Esc    đóng, không làm gì.
//   Kích hoạt      mốc VP (UIL-016) · Dùng thử: mốc EA (UIL-019). Chỗ gắn: ẩn tới khi task đó đặt hook. Có Dùng thử
//                  thì nó là nút chính (Enter), không thì Mua là nút chính.
// Lần sau trong phiên UpgradeRouter không gọi handler mà phát noticeRequested: UpgradePage hiện toast
// "X có trong TrionCAD Y." kèm "Xem" (UpgradeRouter::open: mở lại trang, bỏ qua giới hạn).
//
// Lắp (integration, luồng UI, sau EditionGate và ToastHost):
//   auto* upgrade = new trion::upgrade::UpgradePage(gate, &window, toasts, this); // setHandler + toast "Xem"
//   upgrade->setActivateHook(…);   // UIL-016: mở UI-L01 tab key
//   upgrade->setTrialHook(…);      // UIL-019: mở UI-L05
// Gọi trang: gate.upgrade().request({feature, need, name}) (giới hạn mỗi phiên) hoặc .open() (luôn mở); EditionGate::bind
// đã làm thế cho mọi QAction khóa.
//
// ponytail: ảnh minh họa là biểu tượng của bộ (IconProvider) phóng lớn kèm ổ khóa, chưa có ảnh riêng từng tính năng
// (trường "image" của JSON trỏ ảnh khi có). Địa chỉ trang mua là địa chỉ tạm của UpgradeRouter::url (UIL-021 chốt).
// "Dùng thử" chỉ ẩn khi đang dùng thử; đã dùng thử trước đây hay chưa do PLF-004 báo (UIL-019 gỡ hook khi đã dùng).

#include "app/core/edition/EditionGate.h"
#include "app/widgets/dialog/DialogFrame.h"
#include "core/base/error.h"

#include <QHash>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QStringView>
#include <QUrl>

#include <functional>

class QLabel;

namespace trion::ui {
class FieldMessage;
class ToastHost;
} // namespace trion::ui

namespace trion::upgrade {

// Giá theo đồng; 0: không bán theo kỳ đó.
struct Price {
    qint64 year = 0;
    qint64 month = 0;
    qint64 perpetual = 0;
};

// Nội dung một trang đã chọn theo tính năng, phiên bản và ngôn ngữ.
struct PageContent {
    QString benefit;     // lợi ích 1–2 câu
    QString image;       // tên biểu tượng (IconProvider)
    Price price;         // của phiên bản cần
    bool generic = true; // nội dung chung của phiên bản (tính năng không có mục)
};

// upgrade-content.json: "editions" (Pro, ViewerPro: image, price{year, month, perpetual}, benefit{vi, en}) và
// "features" (mã → need, image, benefit{vi, en}). Chỉ đọc sau khi dựng; chép rẻ (QHash chia sẻ dữ liệu).
class UpgradeContent {
public:
    // Lỗi (JSON hỏng, thiếu Pro/ViewerPro, giá năm không dương, thiếu bản vi/en, need lạ): Error{"upgrade", "content",
    // mô tả kèm mục}.
    static Expected<UpgradeContent> parse(const QByteArray& json);
    // Bản nhúng :/trion/upgrade/upgrade-content.json, đọc lần gọi đầu. Luồng UI.
    static const Expected<UpgradeContent>& builtin();

    QStringList features() const; // mã có mục riêng, theo thứ tự chữ cái
    // Bản cần: requested nếu khác None, không thì need trong mục của tính năng, không có mục thì Pro.
    ui::Edition need(const QString& feature, ui::Edition requested) const;
    // language: "en" lấy bản English, còn lại (vi, vi-XL) bản tiếng Việt. need None coi như Pro.
    PageContent page(const QString& feature, ui::Edition need, QStringView language) const;

private:
    struct Text {
        QString vi, en;
    };
    struct Feature {
        ui::Edition need = ui::Edition::Pro;
        QString image;
        Text benefit;
    };
    struct EditionContent {
        QString image;
        Text benefit;
        Price price;
    };
    QHash<QString, Feature> features_;
    EditionContent pro_, viewerPro_;
};

// Hộp thoại UI-L04 cho một yêu cầu. Thường do UpgradePage dựng; dựng tay được (test, QA-011).
class UpgradeDialog : public ui::DialogFrame {
    Q_OBJECT
public:
    using Hook = std::function<void(const edition::UpgradeRequest&)>;
    struct Hooks {
        std::function<bool(const QUrl&)> openUrl; // Mua: mở trang web; false nếu không mở được. Rỗng: không mở được
        Hook activate;                            // "Đã có key? Kích hoạt"; rỗng: ẩn
        Hook trial;                               // "Dùng thử 30 ngày"; rỗng: ẩn
    };

    // Cần ThemeManager, IconProvider. request.need None: lấy theo content.need(). Modal, tự hủy khi đóng.
    UpgradeDialog(const edition::UpgradeRequest& request, UpgradeContent content, Hooks hooks,
                  QWidget* parent = nullptr);

    edition::UpgradeRequest request() const { return request_; }
    // Đổi tính năng khi đang mở (lần mở kế của UpgradePage): nội dung mới, xóa dòng lỗi.
    void setRequest(const edition::UpgradeRequest& request);
    PageContent content() const; // nội dung đang hiện (theo ngôn ngữ hiện tại)

    void accept() override; // nút chính (Enter): Dùng thử nếu có, không thì Mua

protected:
    void changeEvent(QEvent* e) override;

private:
    void buy();
    void finish(const Hook& hook); // đóng rồi gọi hook với yêu cầu
    void restyle();
    void retranslate();

    edition::UpgradeRequest request_;
    UpgradeContent content_;
    Hooks hooks_;
    QWidget* image_ = nullptr; // Illustration trong .cpp
    QLabel* tag_ = nullptr;
    QLabel* feature_ = nullptr;
    QLabel* benefit_ = nullptr;
    QLabel* price_ = nullptr;
    QLabel* priceNote_ = nullptr;
    ui::FieldMessage* error_ = nullptr;
    ui::Button* later_ = nullptr;
    ui::Button* activate_ = nullptr;
    ui::Button* buy_ = nullptr; // Mua: nút thường khi có Dùng thử, không thì là nút xác nhận
};

// Handler của EditionGate::upgrade() và toast của lần sau. Một trang mở một lúc. Luồng UI; gate, window, toasts sống
// lâu hơn UpgradePage.
class UpgradePage : public QObject {
    Q_OBJECT
public:
    using Hook = UpgradeDialog::Hook;
    using UrlOpener = std::function<bool(const QUrl&)>;

    // Đặt gate.upgrade().setHandler(showUpgrade); toasts khác nullptr thì noticeRequested → toast Info "X có trong
    // TrionCAD Y." kèm "Xem". window: cha của hộp thoại.
    UpgradePage(edition::EditionGate& gate, QWidget* window, ui::ToastHost* toasts = nullptr,
                QObject* parent = nullptr);
    ~UpgradePage() override; // gỡ handler (UpgradeRouter quay về trang web), đóng trang đang mở

    // Mở trang (không xét giới hạn mỗi phiên: việc của UpgradeRouter::request). Đang mở thì thay nội dung và đưa lên.
    // Nội dung nhúng hỏng (không xảy ra với bản đã qua test): mở thẳng trang mua.
    void showUpgrade(const edition::UpgradeRequest& request);
    UpgradeDialog* dialog() const { return dialog_; } // đang mở; nullptr nếu không

    void setActivateHook(Hook hook); // UIL-016 (mốc VP)
    void setTrialHook(Hook hook);    // UIL-019 (mốc EA); ẩn khi đang dùng thử
    void setUrlOpener(UrlOpener opener); // test thay; rỗng: QDesktopServices::openUrl

private:
    edition::EditionGate& gate_;
    QPointer<QWidget> window_;
    QPointer<ui::ToastHost> toasts_;
    QPointer<UpgradeDialog> dialog_;
    Hook activate_, trial_;
    UrlOpener opener_;
};

} // namespace trion::upgrade
