#pragma once
// UI0-107 · Tooltip 2 mức (07.1 §7.1; prototype showTip trong js/ui.js, .tip trong css/app.css, styleguide.html mục
// Menu, tooltip):
//   mức 1, rê 0,45 s: tên, huy hiệu phiên bản khi khóa, tên lệnh tiếng Anh · phím theo nền tảng (Keymap, UI0-114);
//   mức 2, rê thêm 1,5 s: mô tả, "Nhấn F1 để xem trợ giúp" (khi có lệnh).
// Rời phần tử, bấm, lăn chuột, gõ phím: ẩn ngay. Đặt dưới phần tử (hết chỗ thì lên trên), không che con trỏ. F1 khi
// đang rê phần tử có lệnh: phát helpRequested(lệnh), phím F1 toàn cục không chạy.
// Rảnh không timer, không bộ lọc toàn ứng dụng: timer chỉ chạy từ lúc vào phần tử đến khi hiện mức cuối, bộ lọc sự
// kiện của ứng dụng chỉ cài khi đang rê. Màu, cỡ đọc token mỗi lần hiện.
//
// Dùng (main, sau ThemeManager và Keymap):
//   trion::ui::RichTooltip tips;  tips.setKeymap(&keymap);
//   RichTooltip::attach(circle, {.name = tr("Đường tròn"), .command = u"CIRCLE"_s, .keyId = u"circle"_s,
//                                .description = tr("Vẽ đường tròn theo tâm và bán kính.")});
//   RichTooltip::attach(saveAction, {.command = u"QSAVE"_s, .keyId = u"save"_s}); // mục Menu: tên là chữ của action
//   connect(&tips, &RichTooltip::helpRequested, help, &HelpViewer::openCommand);
// Đổi ngôn ngữ: chủ widget gọi lại attach() với chuỗi mới trong retranslate(). Phần tử tự vẽ (mục menu, ô lưới)
// gọi hover()/leave() như Menu.
//
// ponytail: mức 2 chưa có ảnh minh họa hay animation (07.1 §7.1), chưa hiện bí danh lệnh ("CIRCLE · C"); thêm khi
// ribbon (UIA) có dữ liệu đó.

#include <QObject>
#include <QPointer>
#include <QRect>
#include <QString>
#include <QTimer>

#include <chrono>
#include <cstdint>

class QPainter;
class QWidget;

namespace trion::keymap {
class Keymap;
}

namespace trion::ui {

// Bản cần để mở khóa tính năng (07.1 §2): huy hiệu .ed.xs cạnh tên ở tooltip và mục Menu.
enum class Edition : std::uint8_t { None, Pro, ViewerPro };

struct TipInfo {
    QString name{};        // tên đã dịch; rỗng: chữ của QAction (bỏ &) hoặc accessibleName của widget
    QString command{};     // tên lệnh tiếng Anh ("CIRCLE"); có lệnh mới có F1 và dòng "Nhấn F1…"
    QString keyId{};       // id hành động trong Keymap: phím hiện theo nền tảng, theo Keymap khi đổi
    QString description{}; // mô tả mức 2, đã dịch
    Edition need = Edition::None; // khóa theo phiên bản (UI0-011 đặt): huy hiệu, trợ năng báo "khóa"
};

class RichTooltip : public QObject {
    Q_OBJECT
public:
    explicit RichTooltip(QObject* parent = nullptr); // trên luồng UI, sau ThemeManager
    ~RichTooltip() override;

    // RichTooltip tạo gần nhất còn sống (main tạo một; test tạo riêng); nullptr nếu chưa có.
    static RichTooltip* instance();

    // Gắn hoặc thay thông tin của widget hay QAction (lưu trong thuộc tính động của đối tượng). Widget: cần
    // instance(), tự bắt vào/ra và nuốt tooltip thường của Qt. QAction: Menu đo, vẽ lại các mục dùng nó.
    static void attach(QObject* target, const TipInfo& info);
    static TipInfo info(const QObject* target); // {} nếu chưa gắn
    static bool hasInfo(const QObject* target);
    static QString name(const QObject* target); // TipInfo::name hoặc tên suy ra (xem TipInfo)

    // Huy hiệu .ed.xs (Pro, Viewer Pro), theo token hiện tại; dùng chung với Menu.
    static QString editionName(Edition e);
    static QSize badgeSize(Edition e);
    static void paintBadge(QPainter& p, const QRect& r, Edition e);

    const keymap::Keymap* keymap() const;
    void setKeymap(const keymap::Keymap* keymap); // phải sống lâu hơn RichTooltip hoặc tới lần đặt sau
    QString keyText(const QString& keyId) const;  // "F5 · Ctrl+E", "⌘S"…; rỗng nếu không có phím

    // Mặc định 450 ms và 1.500 ms (styleguide); test rút ngắn.
    void setDelays(std::chrono::milliseconds level1, std::chrono::milliseconds level2);

    // Con trỏ vào phần tử target (tọa độ màn hình: anchor là vùng phần tử, cursor là điểm nóng con trỏ). Cùng
    // target thì chỉ cập nhật vị trí. Widget đã attach thì tự gọi.
    void hover(QObject* target, const QRect& anchor, const QPoint& cursor);
    void leave(); // ẩn ngay, dừng timer

    QObject* target() const { return target_; }
    int level() const { return level_; } // 0 ẩn, 1, 2
    QWidget* tipWindow() const;          // cho test, QA

signals:
    void helpRequested(const QString& command); // F1 khi đang rê phần tử có lệnh
    void keysChanged();                         // Keymap đổi hoặc đặt Keymap khác: nhãn phím đổi

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void advance();
    void showLevel(int level);
    QPoint place(const QSize& size) const;

    QPointer<QObject> target_;
    QRect anchor_;
    QPoint cursor_;
    int level_ = 0;
    bool filtering_ = false; // bộ lọc của ứng dụng đang cài
    QTimer timer_{this};     // con của RichTooltip: test, QA kiểm rảnh không timer
    std::chrono::milliseconds level1_{450}, level2_{1500};
    QPointer<const keymap::Keymap> keymap_;
    QMetaObject::Connection keymapChanged_;
    QWidget* window_ = nullptr; // sở hữu
};

} // namespace trion::ui
