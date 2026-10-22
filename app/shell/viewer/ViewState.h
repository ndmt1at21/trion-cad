#pragma once
// UIA-118 · Trạng thái hiển thị của Viewer (07.3 UI-M03 "Lưu trạng thái hiển thị", Viewer Pro): view, layout và trạng
// thái layer của một bản vẽ lưu vào tệp phụ .tcview cạnh bản vẽ ("BanVe.dwg" → "BanVe.tcview"), không sửa DWG. JSON
// gọn có phiên bản (khóa lạ bỏ qua; thiếu khóa, sai kiểu, sai miền là hỏng):
//   {"tcview":1,"layout":"Model","view":{"center":[x,y],"height":h},
//    "layers":[{"name":"KT-TUONG","on":false,"frozen":false},…]}
// layers chỉ gồm layer có trạng thái hiệu lực khác tệp (Viewer không ghi tệp nên trạng thái trong tệp là trạng thái lúc
// mở). Tệp là đầu vào không tin cậy (DoD-C 7): > ViewState::kMaxBytes hoặc hỏng thì bên gọi bỏ qua (ViewStateFile hiện
// banner); tên layer phải hợp lệ (isLayerName) vì sẽ thành đối số của lệnh.
//
// LayerMacro: chuỗi lệnh đổi bật/tắt, đóng băng theo tên mà palette Layer của Viewer, "Khôi phục như tệp gốc" và lúc nạp
// .tcview phát (cú pháp macro như ActionRegistry: khoảng trắng và ';' là Enter):
//   '_-LAYER _ON a,b _OFF c _THAW d _FREEZE e;          (nhóm rỗng bỏ qua; ';' cuối thoát -LAYER)
// Trong suốt (') để không hủy lệnh xem, đo đang chạy. Tên thoát ký tự đại diện của wcmatch (# @ . ~ [ ] , …) bằng `;
// danh sách có khoảng trắng đặt trong ngoặc kép như gõ ở dòng lệnh AutoCAD; tên không hợp lệ bị bỏ (không biểu diễn
// được, và không để tên lạ chèn lệnh).
// ponytail: INT-020 hiện chỉ cho phép LAYOFF, LAYON, LAYFRZ, LAYTHW, LAYISO, LAYUNISO ở Viewer; các lệnh đó chọn đối
// tượng, không nhận tên, nên palette dùng -LAYER ON/OFF/Freeze/Thaw (cần INT-020 cho phép ở chế độ sessionOnly,
// CMD-505 làm -LAYER). Bộ tách token tạm CommandRouter chưa hiểu ngoặc kép: INT-002 phải giữ "…" thành một token.
//
// Hàm thuần, gọi được từ mọi luồng (đọc, ghi tệp: chạy ở nền); chỉ Qt Core.
#include "core/base/error.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QList>
#include <QString>
#include <QStringList>

namespace trion::shell {

// Trạng thái hiệu lực của một layer (theo tên, không phân biệt hoa thường như bảng LAYER).
struct LayerFlags {
    QString name{};
    bool on = true;
    bool frozen = false;
    bool operator==(const LayerFlags&) const = default;
};

struct ViewState {
    static constexpr int kVersion = 1;
    static constexpr qint64 kMaxBytes = 1 << 20; // [đề xuất] 1 MB: lớn hơn thì bỏ qua khi nạp, không ghi khi lưu

    QString layout{};                // tab đang mở: "Model" hoặc tên layout (≤ 255 ký tự)
    double centerX = 0, centerY = 0; // VIEWCTR của tab đó, đơn vị bản vẽ
    double height = 1;               // VIEWSIZE (> 0)
    QList<LayerFlags> layers{};      // layer khác tệp
    bool operator==(const ViewState&) const = default;
};

// Tên layer dùng được: 1–255 ký tự UTF-16 hợp lệ, không có < > / \ " : ; ? * | = ` và ký tự điều khiển.
bool isLayerName(QStringView name);

// "D:/DuAn/BanVe.dwg" → "D:/DuAn/BanVe.tcview".
QString viewStatePath(const QString& drawing);

QByteArray toJson(const ViewState& state); // JSON gọn, đọc lại bằng fromJson cho cùng giá trị
// Lỗi (module "ui"): "too-large", "corrupt" (cú pháp, kiểu, miền, tên layer), "version" (bản mới hơn kVersion).
Expected<ViewState> fromJson(QByteArrayView json);

// Đọc .tcview của bản vẽ: lỗi như fromJson, thêm "missing" (chưa có tệp), "io". Không mở tệp bản vẽ.
Expected<ViewState> readViewState(const QString& drawing);
// Ghi nguyên tử (QSaveFile) vào viewStatePath(drawing): lỗi "too-large", "io", "path" (bản vẽ chính là .tcview).
Expected<void> writeViewState(const QString& drawing, const ViewState& state);

struct LayerMacro {
    QStringList on{}, off{}, thaw{}, freeze{}; // tên layer theo từng tùy chọn của -LAYER
    // Đưa từng layer về flags, cả bật/tắt lẫn đóng băng/rã đông (nạp .tcview).
    static LayerMacro to(const QList<LayerFlags>& target);
    QString text() const; // rỗng nếu không có tên hợp lệ nào
};

} // namespace trion::shell
