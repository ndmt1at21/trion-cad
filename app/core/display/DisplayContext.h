#pragma once
// UI0-012 · Màn hình và DPI (07.1 §13.1, §3.2, §1 mục 6, 9).
//
// - Hệ số lẻ không làm tròn: 125%, 175% là DPR 1,25 / 1,75 (initMain). Chữ, bố cục theo DPR mới là việc của Qt (vẽ
//   lại ở cỡ pixel mới, kích thước logic giữ nguyên); biểu tượng raster thẳng ở cỡ pixel thiết bị (IconProvider).
// - Theo dõi từng cửa sổ cấp cao: đổi DPR (kéo sang màn hình tỉ lệ khác, đổi tỉ lệ trong cài đặt hệ điều hành) và đổi
//   màn hình → tín hiệu. Bộ biểu tượng Viewer raster trước ở nền (IconProvider::prewarm) cho DPR của mọi màn hình lúc
//   tạo, và cho DPR mới gặp khi đang chạy.
// - ScreenClass của màn hình chứa cửa sổ chính, theo bề rộng logic (px độc lập thiết bị, 1920×1080 ở 150% là
//   1280 px): khung đọc qua API, không tự đo.
// - Giảm chuyển động của hệ điều hành → ThemeManager::setReducedMotion (mọi --t* về 0 ms).
// Mật độ gọn/thoáng và Giao diện nhẹ toàn ứng dụng do ThemeManager áp (UI0-002), không qua đây.
//
// Dùng, trên luồng UI:
//   int main(int argc, char** argv) {
//       trion::ui::DisplayContext::initMain();              // trước QApplication
//       QApplication app(argc, argv);
//       ... loadFonts(); ThemeManager tm; UiExecutor ex; IconProvider icons; MainWindow w; w.show();
//       trion::ui::DisplayContext display(&w);              // sau ThemeManager, IconProvider; giữ suốt đời ứng dụng
//       return app.exec();
//   }
//   auto& d = trion::ui::DisplayContext::instance();
//   d.track(floatingPalette);                               // UI0-010: palette nổi, cửa sổ cấp cao khác
//   connect(&d, &DisplayContext::devicePixelRatioChanged, viewport, [viewport](QWidget* w, qreal dpr) {
//       if (w == viewport->window()) ...;                   // RND: dựng lại framebuffer ở cỡ pixel mới
//   });
//   if (d.screenClass() == trion::ui::ScreenClass::Wide) ...; // UI0-010, UIA-001: bố cục mặc định
//
// ponytail: màn hình đọc thẳng từ QScreen nên chỉ mã liên kết Qt dùng được. Khi có NT-008 (core/base/hw/profile.h:
// DPI, tần số màn hình) thì nguồn màn hình cho lõi (RND) đi qua hồ sơ đó; DisplayContext đẩy thay đổi vào hồ sơ.

#include <QAbstractNativeEventFilter>
#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>

class QScreen;
class QWidget;

namespace trion::ui {

// Lớp màn hình theo bề rộng logic (07.1 §3.2; styleguide bảng kích thước, ngưỡng 1600 px).
enum class ScreenClass : quint8 {
    Small, // ≤ 1366 px (1366×768; cả 1920×1080 ở 150%): như Narrow, mọi thứ phải vừa ở cỡ nhỏ nhất được hỗ trợ
    Narrow, // 1367–1599 px: palette tự ẩn thành dải 22 px, dòng lệnh 1 dòng lịch sử, panel ribbon co theo ưu tiên
    Wide,   // ≥ 1600 px: palette Thuộc tính dock mở sẵn, dòng lệnh 3 dòng lịch sử
};

class DisplayContext : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
    // Kiểu đủ cho moc: tham số tín hiệu dùng được qua QSignalSpy và kết nối hàng đợi mà không cần qRegisterMetaType.
    Q_MOC_INCLUDE(<QScreen>)
    Q_MOC_INCLUDE(<QWidget>)
public:
    // Gọi trong main trước khi tạo QApplication (gọi sau thì TRION_CHECK): DPR đúng tỉ lệ hệ điều hành, không làm tròn
    // (Qt::HighDpiScaleFactorRoundingPolicy::PassThrough). Biến QT_SCALE_FACTOR_ROUNDING_POLICY vẫn ghi đè được.
    static void initMain();

    // Sau ThemeManager và IconProvider. Theo dõi cửa sổ chính `window`; áp "giảm chuyển động" của hệ điều hành vào
    // ThemeManager::instance() (Windows: theo dõi cả lúc người dùng đổi).
    explicit DisplayContext(QWidget* window, QObject* parent = nullptr);
    ~DisplayContext() override;

    // DisplayContext tạo gần nhất còn sống. Chưa có thì dừng (TRION_CHECK).
    static DisplayContext& instance();

    // Theo dõi thêm một cửa sổ cấp cao (palette nổi, hộp thoại không modal); widget hủy thì tự bỏ, gọi lại thì bỏ qua.
    void track(QWidget* window);

    // Lớp của màn hình chứa cửa sổ chính; đổi khi cửa sổ sang màn hình khác, hoặc màn hình đổi độ phân giải, tỉ lệ.
    ScreenClass screenClass() const { return class_; }
    static ScreenClass classify(int logicalWidth);

    // "Giảm chuyển động" của hệ điều hành lúc gọi. Windows: tắt "Hiệu ứng hoạt ảnh" (SPI_GETCLIENTAREAANIMATION).
    // Linux: KDE AnimationDurationFactor=0 (kdeglobals), GTK gtk-enable-animations=false (gtk-3.0/settings.ini).
    static bool systemReducedMotion();

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override; // Windows

signals:
    // Cửa sổ được theo dõi có DPR mới. Phát sau khi screenClass() đã cập nhật.
    void devicePixelRatioChanged(QWidget* window, qreal dpr);
    // Cửa sổ được theo dõi sang màn hình khác (cùng hay khác DPR).
    void screenChanged(QWidget* window, QScreen* screen);
    void screenClassChanged(trion::ui::ScreenClass screenClass);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct Seen {
        QScreen* screen;
        qreal dpr;
    };
    void refresh(QWidget* window);
    void watch(QScreen* screen);
    void updateClass();
    void warm(qreal dpr);

    QPointer<QWidget> main_;
    QHash<const QObject*, Seen> seen_;
    QList<qreal> warmed_; // DPR đã raster trước bộ Viewer
    ScreenClass class_ = ScreenClass::Wide;
    QMetaObject::Connection screenGeometry_;
};

} // namespace trion::ui
