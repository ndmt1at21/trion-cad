#pragma once
// UIA-109 · Phần vẽ của vùng vẽ (07.1 §9.1 con trỏ, §9.3 khung chọn; prototype js/canvas.js View.prototype.drawOver):
// chữ thập, ô chọn (PICKBOX), ô bắt điểm (APERTURE), huy hiệu con trỏ, vòng bận, khung Window/Crossing, lasso, đa giác.
// CanvasWidget (phần nhận sự kiện) giữ một Overlay, đổi nó theo chuột, phím, API rồi báo vùng bẩn bằng cursorRegion và
// shapeRegion của trạng thái cũ và mới; paintEvent vẽ cảnh (chỗ nối) rồi paintOverlay.
// ponytail: QPainter trên widget; RND-001 thay bằng cửa sổ QRhi, RND-020 (chữ thập, ô chọn) và RND-012 (khung, đa giác)
// vẽ cùng Overlay trên GPU; phần nhận sự kiện và API của CanvasWidget giữ nguyên.
//
// Tọa độ: pixel logic của widget; điểm nóng con trỏ là pixel nguyên (làm tròn xuống) để nét 1 px sắc ở mọi tỉ lệ.
// Màu: vùng vẽ không theo chủ đề giao diện (07.1 §13.2): --c-cross và vòng bận lấy giá trị :root của tokens.css; khung
// chọn, huy hiệu qua màu ngữ nghĩa Sem::Selection*, Sem::DynTooltip* (ghi đè được ở UI-D10).

#include "app/theme/generated/Tokens.h"

#include <QColor>
#include <QPoint>
#include <QPolygonF>
#include <QRegion>
#include <QString>

#include <cstdint>
#include <optional>

class QPainter;

namespace trion::shell {

// Trạng thái con trỏ (07.1 §9.1). Lệnh và lõi tương tác (INT-003, INT-016) đặt qua CanvasWidget::setCursorState.
enum class CursorState : std::uint8_t {
    Idle,       // Chờ: chữ thập kèm ô chọn
    PickPoint,  // Chọn điểm: chữ thập
    PickObject, // Chọn đối tượng: chỉ ô chọn
    Pan,        // Kéo pan: bàn tay (con trỏ hệ thống)
    Zoom,       // Zoom thời gian thực: kính lúp (con trỏ từ bộ biểu tượng)
    Busy,       // Bận mà vẫn cho tương tác: chữ thập kèm vòng tiến trình nhỏ
};

// Kiểu vùng chọn (07.1 §9.3). Khung kéo: Window, Crossing; lasso: WPolygon (Window), CPolygon (Crossing), Fence;
// lệnh hỏi F / WP / CP: Fence, WPolygon, CPolygon.
enum class SelectMode : std::uint8_t { Window, Crossing, WPolygon, CPolygon, Fence };

struct Overlay {
    std::optional<QPoint> mouse; // điểm nóng con trỏ; không có: con trỏ ngoài vùng vẽ (không vẽ con trỏ)
    CursorState cursor = CursorState::Idle;
    int crossHalf = 0;    // nửa chiều dài chữ thập (CURSORSIZE % cạnh dài vùng vẽ)
    int pickHalf = 0;     // nửa cạnh ô chọn (PICKBOX); 0: không vẽ
    int apertureHalf = 0; // nửa cạnh ô bắt điểm (APERTURE) khi đang bắt điểm; 0: không vẽ
    QString badge;        // biểu tượng huy hiệu (IconProvider); rỗng: không vẽ
    double busyTurn = 0;  // góc quay của vòng bận, độ
    QPolygonF shape;      // khung (4 đỉnh), lasso, đa giác đang dựng; < 2 đỉnh: không vẽ
    SelectMode mode = SelectMode::Window;

    bool sameCursor(const Overlay& o) const
    {
        return mouse == o.mouse && cursor == o.cursor && crossHalf == o.crossHalf && pickHalf == o.pickHalf &&
               apertureHalf == o.apertureHalf && badge == o.badge && busyTurn == o.busyTurn;
    }
    bool sameShape(const Overlay& o) const { return mode == o.mode && shape == o.shape; }
};

bool hasCross(CursorState s);   // Chờ, Chọn điểm, Bận
bool hasPickbox(CursorState s); // Chờ, Chọn đối tượng
bool drawsCursor(CursorState s); // con trỏ do vùng vẽ vẽ (con trỏ hệ thống ẩn): mọi trạng thái trừ Pan, Zoom

// Màu token trên vùng vẽ: giá trị :root của tokens.css, không theo chủ đề giao diện.
QColor canvasColor(tok::ColorId id);

// Vẽ khung chọn rồi con trỏ. Cần ThemeManager; huy hiệu cần IconProvider.
void paintOverlay(QPainter& p, const Overlay& o);
// Vùng (pixel logic) mà con trỏ, khung chiếm: hợp của trạng thái cũ và mới là vùng phải vẽ lại.
QRegion cursorRegion(const Overlay& o);
QRegion shapeRegion(const Overlay& o);

} // namespace trion::shell
