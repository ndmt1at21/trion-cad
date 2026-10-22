#include "app/core/display/DisplayContext.h"

#include "app/core/UiExecutor.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "core/base/assert.h"

#include <QCoreApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QWidget>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif !defined(Q_OS_MACOS)
#include <QSettings>
#include <QStandardPaths>
#endif

#include <utility>

using namespace Qt::StringLiterals;

namespace trion::ui {

namespace {
DisplayContext* g_instance = nullptr; // chỉ luồng UI
} // namespace

void DisplayContext::initMain()
{
    TRION_CHECK(!QCoreApplication::instance() && "DisplayContext::initMain() phải gọi trước QApplication");
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
}

DisplayContext::DisplayContext(QWidget* window, QObject* parent) : QObject(parent), main_(window)
{
    TRION_ASSERT_UI_THREAD();
    g_instance = this;
    // Bộ Viewer raster trước ở DPR của mọi màn hình (nền): kéo sang màn hình khác thì khung đầu chỉ còn raster biểu
    // tượng ngoài bộ. Màn hình cắm thêm hoặc đổi tỉ lệ khi đang chạy: raster lúc cửa sổ sang DPR đó (refresh).
    warm(window->devicePixelRatio());
    for (const QScreen* s : QGuiApplication::screens())
        warm(s->devicePixelRatio());
    track(window);
    watch(window->screen());
    theme::ThemeManager::instance().setReducedMotion(systemReducedMotion());
#ifdef Q_OS_WIN
    QCoreApplication::instance()->installNativeEventFilter(this);
#endif
}

DisplayContext::~DisplayContext()
{
#ifdef Q_OS_WIN
    if (QCoreApplication::instance())
        QCoreApplication::instance()->removeNativeEventFilter(this);
#endif
    if (g_instance == this)
        g_instance = nullptr;
}

DisplayContext& DisplayContext::instance()
{
    TRION_CHECK(g_instance && "chưa tạo DisplayContext");
    return *g_instance;
}

DisplayContext* DisplayContext::find()
{
    return g_instance;
}

void DisplayContext::track(QWidget* window)
{
    TRION_CHECK(window && window->isWindow());
    if (seen_.contains(window))
        return;
    seen_.insert(window, {window->screen(), window->devicePixelRatio()});
    window->installEventFilter(this);
    connect(window, &QObject::destroyed, this, [this](QObject* o) { seen_.remove(o); });
}

ScreenClass DisplayContext::classify(int logicalWidth)
{
    return logicalWidth >= 1600 ? ScreenClass::Wide : logicalWidth > 1366 ? ScreenClass::Narrow : ScreenClass::Small;
}

bool DisplayContext::eventFilter(QObject* watched, QEvent* event)
{
    // DevicePixelRatioChange (Qt ≥ 6.6): sang màn hình tỉ lệ khác hoặc hệ điều hành đổi tỉ lệ; Qt gửi hai lần khi
    // đổi màn hình, refresh() chỉ phát khi giá trị khác. ScreenChangeInternal: sang màn hình khác kể cả cùng DPR.
    // Show: lần hiện đầu có thể đặt cửa sổ lên màn hình khác lúc track().
    switch (event->type()) {
    case QEvent::DevicePixelRatioChange:
    case QEvent::ScreenChangeInternal:
    case QEvent::Show:
        refresh(static_cast<QWidget*>(watched)); // chỉ cài filter lên widget qua track()
        break;
    default:
        break;
    }
    return false;
}

void DisplayContext::refresh(QWidget* window)
{
    const auto it = seen_.find(window);
    if (it == seen_.end())
        return;
    const qreal dpr = window->devicePixelRatio();
    QScreen* const screen = window->screen();
    const Seen was = std::exchange(*it, Seen{screen, dpr}); // trước khi phát: người nghe có thể track() thêm
    if (screen != was.screen && window == main_)
        watch(screen);
    if (dpr != was.dpr) {
        warm(dpr);
        emit devicePixelRatioChanged(window, dpr);
    }
    if (screen != was.screen)
        emit screenChanged(window, screen);
}

void DisplayContext::watch(QScreen* screen)
{
    disconnect(screenGeometry_);
    if (screen) // đổi độ phân giải hoặc tỉ lệ của chính màn hình đó (chiếu máy chiếu, cài đặt hệ điều hành)
        screenGeometry_ = connect(screen, &QScreen::geometryChanged, this, &DisplayContext::updateClass);
    updateClass();
}

void DisplayContext::updateClass()
{
    const QScreen* screen = main_ ? main_->screen() : nullptr;
    if (!screen)
        return;
    const ScreenClass c = classify(screen->geometry().width());
    if (c != class_) {
        class_ = c;
        emit screenClassChanged(c);
    }
}

void DisplayContext::warm(qreal dpr)
{
    // Biểu tượng khác bộ Viewer raster khi vẽ lần đầu ở DPR này (IconProvider::pixmap, ~0,1–0,2 ms mỗi cái).
    if (warmed_.contains(dpr))
        return;
    warmed_.append(dpr);
    theme::IconProvider::instance().prewarm(dpr);
}

bool DisplayContext::systemReducedMotion()
{
#if defined(Q_OS_WIN)
    BOOL animate = TRUE;
    return SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animate, 0) && !animate;
#elif defined(Q_OS_MACOS)
    // ponytail: chưa đọc (mốc macOS, 07.1 §13.6; UI0-013): NSWorkspace.accessibilityDisplayShouldReduceMotion và
    // thông báo NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification, trong một tệp .mm.
    return false;
#else
    // ponytail: chỉ đọc tệp cấu hình KDE và GTK, một lần lúc tạo DisplayContext. GNOME giữ "Giảm hoạt ảnh"
    // (org.gnome.desktop.interface enable-animations) trong dconf, đọc được qua GIO hoặc cổng Settings của
    // xdg-desktop-portal (khóa reduced-motion của org.freedesktop.appearance, tín hiệu SettingChanged cho cả lúc đang
    // chạy) bằng QtDBus: làm khi có bản Linux cho người dùng.
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    const QSettings kde(dir + u"/kdeglobals"_s, QSettings::IniFormat);
    if (kde.value(u"KDE/AnimationDurationFactor"_s, 1.0).toDouble() == 0.0) // Hoạt ảnh: "Tức thì"
        return true;
    const QSettings gtk(dir + u"/gtk-3.0/settings.ini"_s, QSettings::IniFormat);
    const QString on = gtk.value(u"Settings/gtk-enable-animations"_s).toString();
    return on.compare(u"false"_s, Qt::CaseInsensitive) == 0 || on == u"0"_s;
#endif
}

bool DisplayContext::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result)
{
    Q_UNUSED(result)
#ifdef Q_OS_WIN
    // Bật/tắt "Hiệu ứng hoạt ảnh" (Cài đặt › Trợ năng): Windows gửi WM_SETTINGCHANGE tới các cửa sổ cấp cao.
    const auto* msg = static_cast<const MSG*>(message);
    if (eventType == "windows_generic_MSG" && msg->message == WM_SETTINGCHANGE &&
        msg->wParam == SPI_SETCLIENTAREAANIMATION)
        theme::ThemeManager::instance().setReducedMotion(systemReducedMotion());
#else
    Q_UNUSED(eventType)
    Q_UNUSED(message)
#endif
    return false;
}

} // namespace trion::ui
