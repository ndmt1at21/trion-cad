#pragma once
// UI0-001 · Ứng dụng trion: dựng các phần của ứng dụng theo thứ tự khởi động trên luồng UI, giữ chúng suốt đời ứng
// dụng, thoát có hủy việc nền hợp tác.
//
// main.cpp dựng phần đầu: StartupTrace → DisplayContext::initMain() (High-DPI) → QApplication → UiProbe::configure →
// parseOptions → SingleInstance (lần chạy sau chuyển đường dẫn rồi thoát ở đây). Application dựng tiếp, mỗi pha một mốc
// trong StartupTrace:
//   Store (tệp cài đặt của --settings) → UiExecutor → Translator → loadFonts → ThemeManager → IconProvider → Keymap →
//   [ready] → cửa sổ chính → DisplayContext (raster trước bộ biểu tượng Viewer ở nền cho DPR mọi màn hình) → hiện cửa
//   sổ.
// Thoát (hàm hủy): hủy token toàn ứng dụng, hủy cửa sổ và IconProvider (hủy việc raster còn chờ), chờ mọi việc nền dừng
// (sched::waitIdle), rồi hủy phần còn lại theo thứ tự ngược; UiExecutor và Store (ghi nốt cài đặt) cuối cùng.
//
// Luồng UI sở hữu document (UiExecutor.h: kết quả nền về luồng UI, requireUiThread chặn API ghi gọi sai luồng).
// ponytail: chưa có phiên nhiều bản vẽ DOC-018 và input pipeline INT-002, nên chưa có CoreSession; đường dẫn cần mở
// (dòng lệnh, lần chạy sau) chỉ tới openRequested(), DOC-018/UIB-002 nối. Cửa sổ chính là QMainWindow trống, UIA-001
// thay bằng cửa sổ thật.

#include "core/base/sched/Scheduler.h"

#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

class QCommandLineParser;
class QMainWindow;

namespace trion::settings {
class Store;
}
namespace trion::ui {
class DisplayContext;
class RichTooltip;
class UiExecutor;
} // namespace trion::ui
namespace trion::i18n {
class Translator;
}
namespace trion::theme {
class IconProvider;
class ThemeManager;
} // namespace trion::theme
namespace trion::keymap {
class Keymap;
}

namespace trion::app {

class StartupTrace;

// Dòng lệnh của trion (Application::parseOptions).
struct Options {
    QStringList paths;    // tệp cần mở, tuyệt đối theo thư mục hiện tại của tiến trình
    QString settingsPath; // --settings <tệp>; mặc định settings::Store::defaultFilePath(). Tuyệt đối
    QString tracePath;    // --startup-trace <tệp.json>
    bool quitAfterFirstFrame = false; // --quit-after-first-frame: thoát ngay sau khung đầu (test, đo khởi động)
    bool gallery = false;             // --gallery: bảng mẫu thành phần (UI0-006) thay cửa sổ chính, cho QA-011 chụp
};

class Application : public QObject {
    Q_OBJECT
public:
    // Đọc dòng lệnh (sau QApplication; --ui-probe[=<tệp.json>] đã qua UiProbe::configure). --help hoặc tùy chọn sai:
    // nạp ngôn ngữ người dùng đã chọn rồi để QCommandLineParser in trợ giúp hoặc lỗi (hộp thoại khi chạy ngoài console
    // trên Windows) và thoát tiến trình (mã 0 hoặc 1).
    static Options parseOptions(const QStringList& arguments);

    // Luồng UI, sau QApplication và SingleInstance::listen(). Đặt mốc từng pha vào trace (sống lâu hơn Application),
    // hiện cửa sổ chính, ghi trace khi có khung đầu.
    Application(const Options& options, StartupTrace& trace, QObject* parent = nullptr);
    ~Application() override;

    static Application* instance(); // luồng UI; nullptr nếu chưa có

    // Luồng UI: token bị hủy khi ứng dụng thoát. Việc nền dài chép token lúc gửi và kiểm thường xuyên (DoD-H 5: dừng
    // ≤ 50 ms): thoát chờ mọi việc nền dừng, việc không kiểm token làm thoát chậm. Không có Application: token không
    // hủy. ponytail: tác vụ có tiến trình của NT-013 (BackgroundJobs) chưa có; khi có, thoát gọi hủy mọi tác vụ của nó.
    static sched::CancelToken cancelToken();

    QMainWindow* mainWindow() const { return window_.get(); }

    // Luồng UI: tệp từ dòng lệnh hoặc từ lần chạy sau (SingleInstance::received). Đưa cửa sổ chính lên trước (bỏ thu
    // nhỏ), rồi phát openRequested nếu có đường dẫn.
    void requestOpen(const QStringList& paths);

signals:
    // Đường dẫn tuyệt đối cần mở. DOC-018/UIB-002 nối để mở thành tab; hiện chỉ ghi log mức info của "trion.app"
    // (QT_LOGGING_RULES="trion.app.info=true").
    void openRequested(const QStringList& paths);

private:
    static void addOptions(QCommandLineParser& parser);

    StartupTrace& trace_;
    sched::CancelSource cancel_;
    // Thứ tự khai báo = thứ tự tạo; hủy ngược lại (hàm hủy làm tường minh).
    std::unique_ptr<settings::Store> store_;
    std::unique_ptr<ui::UiExecutor> executor_;
    std::unique_ptr<i18n::Translator> translator_;
    std::unique_ptr<theme::ThemeManager> theme_;
    std::unique_ptr<theme::IconProvider> icons_;
    std::unique_ptr<keymap::Keymap> keymap_;
    std::unique_ptr<ui::RichTooltip> tips_;
    std::unique_ptr<QMainWindow> window_;
    std::unique_ptr<ui::DisplayContext> display_;
};

} // namespace trion::app
