// UI0-006 · trion_gallery: bảng mẫu thành phần chạy riêng (Gallery.h), khởi tạo theo thứ tự của ứng dụng
// (app/main/Application.h): Store → UiExecutor → Translator → loadFonts → ThemeManager → IconProvider → Keymap →
// RichTooltip → cửa sổ → DisplayContext. Thoát: cửa sổ và IconProvider trước, chờ việc nền dừng, rồi phần còn lại.
//   trion_gallery [--settings <tệp>] [--ui-probe[=<tệp.json>]]
#include "app/core/display/DisplayContext.h"
#include "app/core/i18n/Translator.h"
#include "app/core/keymap/Keymap.h"
#include "app/core/perf/UiProbe.h"
#include "app/core/UiExecutor.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/gallery/Gallery.h"
#include "app/widgets/tooltip/RichTooltip.h"
#include "core/base/sched/Scheduler.h"
#include "core/base/settings/Settings.h"

#include <QApplication>

using namespace Qt::StringLiterals;
namespace settings = trion::settings;

int main(int argc, char** argv)
{
    trion::ui::DisplayContext::initMain();
    QApplication app(argc, argv);
    QGuiApplication::setApplicationDisplayName(u"TrionCAD"_s);
    trion::ui::probe::configure(QCoreApplication::arguments());
    const QStringList args = QCoreApplication::arguments();
    const qsizetype at = args.indexOf(u"--settings"_s);
    settings::Store store(at > 0 && at + 1 < args.size() ? args[at + 1] : settings::Store::defaultFilePath());
    settings::Store::setInstance(&store);
    trion::ui::UiExecutor executor;
    int code = 0;
    {
        const trion::i18n::Translator translator;
        if (const auto fonts = trion::theme::loadFonts(); !fonts) // vẫn chạy bằng font dự phòng
            qWarning("%s", fonts.error().message.c_str());
        trion::theme::ThemeManager themes(store);
        {
            trion::theme::IconProvider icons;
            const trion::keymap::Keymap keys(store);
            trion::ui::RichTooltip tips;
            tips.setKeymap(&keys);
            trion::ui::GalleryWindow window;
            trion::ui::DisplayContext display(&window);
            window.show();
            code = QApplication::exec();
        }
        trion::sched::waitIdle(); // việc nền (model mẫu, raster biểu tượng) dừng trước khi hủy UiExecutor
    }
    trion::ui::probe::disable(); // ghi JSON của --ui-probe=<tệp.json>
    return code;
}
