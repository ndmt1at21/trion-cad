#pragma once
// UIH-001 tiêu chí 6, phần kiểm được trước PLT-114: chuỗi trả lời của -PLOT "cấu hình chi tiết" (mỗi phần tử là một
// dòng của .scr; chuỗi rỗng là Enter: lấy mặc định) → PlotSettings, để so với thiết lập của hộp thoại In. Mặc định của
// mỗi câu là giá trị ban đầu của hộp thoại (initialSettings). Trình tự prompt như AutoCAD:
//   Detailed plot configuration? [Yes/No]           chỉ nhận Yes
//   layout name                                     rỗng, hoặc đúng layout của context ("Model" ở Model)
//   output device name                              Device::name
//   paper size                                      Paper::name hoặc tên hiện của khổ ("ISO A1")
//   paper units [Inches/Millimeters]
//   drawing orientation [Portrait/Landscape]
//   Plot upside down? [Yes/No]
//   plot area [Display/Extents/Limits/View/Window]  layout: [Display/Extents/Layout/View/Window]
//     View: tên view · Window: góc dưới trái "x,y", góc trên phải "x,y"
//   plot scale                                      "1:100", "1=100" (mm = đơn vị) hoặc Fit
//   plot offset                                     "x,y" hoặc Center (chỉ Model)
//   Plot with plot styles? [Yes/No] · plot style table name ("." là không) · Plot with lineweights? [Yes/No]
//   Model: shade plot setting [As displayed/Wireframe/Hidden/Rendered]
//   layout: Scale lineweights with plot scale? · Plot paper space first? · Hide paperspace objects? [Yes/No]
//   thiết bị tệp (PDF, PNG): file name · máy in: Write the plot to a file [Yes/No], Yes thì file name
//   Save changes to page setup [Yes/No] · Proceed with plot [Yes/No]
// Từ khóa nhận viết tắt như AutoCAD (tiền tố, không phân biệt hoa/thường). Sai hoặc thiếu trả lời: Error ("plot",
// "script", câu nêu prompt sai).
// ponytail: tập con cho test; PLT-114 làm lệnh thật (prompt, `?`, LISP, -PUBLISH) giữ trình tự này.

#include "app/dialogs/plot/PlotSettings.h"
#include "core/base/error.h"

#include <QStringList>

namespace trion::plot {

Expected<PlotSettings> parsePlotCommand(const QStringList& answers, const PlotContext& context,
                                        const QList<Device>& devices);

} // namespace trion::plot
