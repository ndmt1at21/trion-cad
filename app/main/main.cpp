// UI0-001 · Điểm vào của ứng dụng trion. Thứ tự khởi động và vòng đời: app/main/Application.h.
#include "app/core/display/DisplayContext.h"
#include "app/core/perf/UiProbe.h"
#include "app/main/Application.h"
#include "app/main/SingleInstance.h"
#include "app/main/StartupTrace.h"

#include <QApplication>

using namespace Qt::StringLiterals;
using trion::app::Application;
using trion::app::SingleInstance;

int main(int argc, char** argv)
{
    trion::app::StartupTrace trace; // mốc 0 và "main"; hủy cuối cùng: mốc "exit", ghi tệp lần cuối
    trion::ui::DisplayContext::initMain();
    QApplication qapp(argc, argv);
    QGuiApplication::setApplicationDisplayName(u"TrionCAD"_s);
    trace.mark(u"qt"_s);
    trion::ui::probe::configure(QCoreApplication::arguments());
    const trion::app::Options options = Application::parseOptions(QCoreApplication::arguments());
    trace.setOutput(options.tracePath);

    SingleInstance instance(options.settingsPath);
    if (!instance.listen()) {
        if (instance.forward(options.paths)) {
            trace.mark(u"forwarded"_s);
            return 0;
        }
        if (!instance.listen()) // phiên đang chạy không nhận; nếu nó vừa thoát thì forward() đã giành được khóa
            return 1;
    }
    trace.mark(u"instance"_s);

    Application app(options, trace);
    QObject::connect(&instance, &SingleInstance::received, &app, &Application::requestOpen);
    if (!options.paths.isEmpty())
        app.requestOpen(options.paths);
    const int code = QCoreApplication::exec();
    trion::ui::probe::disable(); // ghi JSON của --ui-probe=<tệp.json>
    return code;
}
