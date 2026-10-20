#pragma once
// UI0-105 · Đo độ trễ và thời gian khung trong ứng dụng, watchdog luồng UI (README §3 DoD-L, DoD-H 6).
//
// Bật/tắt
//  - Tắt mặc định ở mọi bản build. UI0-001 gọi configure(app.arguments()) ngay sau khi tạo QApplication:
//    `--ui-probe` bật; `--ui-probe=<tệp.json>` bật và ghi JSON khi tắt hoặc khi ứng dụng thoát (QA-011 đọc).
//    Test và benchmark (UI0-014, QA-011) gọi enable()/disable() trực tiếp. enable/disable chỉ trên luồng UI.
//  - Khi tắt: không event filter, không luồng; Scope, TRION_UI_PROBE_SCOPE, record(), untilFrame() chỉ đọc
//    một cờ nguyên tử, không cấp phát.
//
// Đo gì (bật)
//  - Khung hình = một lần giao UpdateRequest/Expose cho cửa sổ (vẽ + flush backing store). Probe là event
//    filter của ứng dụng; gặp hai sự kiện này nó tự giao lại ngay trong filter để biết đúng lúc giao xong.
//  - typing / ui_state: từ lúc phím/nút chuột tới QWindow đến hết khung kế tiếp của cửa sổ đó; phím có ký tự
//    in được (không Ctrl/Meta) là typing. Nếu vòng sự kiện đã xử lý hết việc còn chờ mà chưa có khung (thao
//    tác không vẽ gì, hoặc kết quả về sau qua việc nền) thì bỏ mẫu; ca bất đồng bộ dùng untilFrame().
//  - scroll_frame: lăn chuột, kéo (di chuột khi giữ nút), đổi kích thước cửa sổ: từ input đầu tiên chưa được
//    vẽ đến hết khung kế tiếp, tức thời gian luồng UI làm ra một khung; fps = 1000 / số đó.
//  - menu_open / dialog_open: QMenu/QDialog hiện đến hết khung đầu của nó, tính từ input còn chờ khung (cú
//    nhấp/phím mở nó) nếu có, không thì từ lúc hiện. Cửa sổ ẩn trước khung đầu thì bỏ.
//  - Các loại còn lại do nơi sở hữu đo: Scope (đồng bộ), record() (đã tự đo), untilFrame() (đến khung kế).
//
// Watchdog: luồng UI ở trong một sự kiện (hoặc một lượt vòng sự kiện) liên tục ≥ ngưỡng (mặc định 50 ms) là
// khựng. Luồng watchdog báo ngay khi đang khựng (stderr, kể cả khi treo hẳn); luồng UI chốt độ dài thật khi
// chạy tiếp, nên không lần khựng nào bị sót dù luồng watchdog thức muộn. Báo kèm loại sự kiện, lớp đối tượng
// nhận và vùng đo (TRION_UI_PROBE_SCOPE) đang mở. Luồng UI chờ trong vòng sự kiện (aboutToBlock) thì watchdog
// ngủ không thời hạn: không timer, không polling.
//
// JSON (toJson, cho QA-011), thời gian tính bằng ms:
//   {"format": "trion-ui-probe/1",
//    "ops": {"<tên>": {"count": n, "p50_ms": x|null, "p99_ms": x|null, "max_ms": x|null,
//                      "budget_ms": ngưỡng máy tầm trung, "budget_low_ms": ngưỡng máy thấp}, ...},
//    "stalls": {"threshold_ms": 50, "count": n, "items": [{"ms": x, "where": "...", "live": true}, ...]}}
// "ops" luôn đủ mọi tên của Op (count 0 thì p50/p99/max là null); scroll_frame có thêm "fps_p50", "fps_p99".
// Ngưỡng là p99 theo bảng DoD-L.

#include <QStringList>
#include <QJsonObject>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

class QWidget;

namespace trion::ui::probe {

using Clock = std::chrono::steady_clock;

// Loại thao tác theo bảng DoD-L. Tên trong JSON (chú thích bên phải) ổn định: QA-011 dựa vào nó.
enum class Op : std::uint8_t {
    OsnapHitTest,    // osnap_hit_test    1 / 3 ms        Scope quanh truy vấn
    JigPreview,      // jig_preview       8 / 16 ms       Scope mỗi lần cập nhật xem trước
    ViewportFrame,   // viewport_frame    16,6 / 33 ms    record() thời gian khung vùng vẽ
    CommandComplete, // command_complete  20 / 50 ms      Scope quanh lệnh
    UiState,         // ui_state          100 ms          tự đo
    MenuOpen,        // menu_open         100 ms          tự đo
    Typing,          // typing            16 / 33 ms      tự đo
    Autocomplete,    // autocomplete      16 / 33 ms      untilFrame() khi áp danh sách
    DialogOpen,      // dialog_open       150 / 300 ms    tự đo
    PaletteOpen,     // palette_open      150 / 300 ms    untilFrame() khi palette có nội dung
    PaletteUpdate,   // palette_update    16 / 33 ms      Scope phần đồng bộ
    ScrollFrame,     // scroll_frame      16,6 / 33 ms    tự đo
    WarmStart,       // warm_start        1500 / 3000 ms  record() (UI0-001)
    Count
};
const char* name(Op op);

namespace detail {
extern std::atomic<bool> g_on;
} // namespace detail

// Bật nếu args có --ui-probe hoặc --ui-probe=<tệp.json>. Trả trạng thái sau khi xét.
bool configure(const QStringList& args);
void enable();
void disable(); // ghi JSON nếu configure() có tệp
inline bool enabled() noexcept { return detail::g_on.load(std::memory_order_relaxed); }

void setStallThreshold(std::chrono::milliseconds threshold); // mọi lúc, mọi luồng

// Mọi luồng: thêm một mẫu đã tự đo.
void record(Op op, Clock::duration duration);

// Luồng UI: mẫu `op` kết thúc khi hết khung kế tiếp của cửa sổ chứa widget. start mặc định: input còn chờ khung
// nếu có, không thì bây giờ. Cửa sổ phải đã tạo (gọi sau show()).
void untilFrame(Op op, const QWidget* widget, Clock::time_point start = {});

// Vùng đo trên mọi luồng. Scope(Op) ghi thời gian của vùng vào op; vùng của luồng UI dùng để báo khựng.
// ponytail: khi có NT-004 thì TRION_UI_PROBE_SCOPE mở thêm ZoneScopedN(name) của Tracy (name là chuỗi hằng).
class Scope {
public:
    explicit Scope(const char* zone) noexcept
    {
        if (enabled())
            enter(zone, Op::Count);
    }
    explicit Scope(Op op) noexcept
    {
        if (enabled())
            enter(name(op), op);
    }
    ~Scope()
    {
        if (zone_)
            leave();
    }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

private:
    void enter(const char* zone, Op op) noexcept;
    void leave() noexcept;
    const char* zone_ = nullptr;
    const char* prev_ = nullptr;
    Clock::time_point start_{};
    Op op_ = Op::Count;
};

#define TRION_UI_PROBE_CAT2(a, b) a##b
#define TRION_UI_PROBE_CAT(a, b) TRION_UI_PROBE_CAT2(a, b)
#define TRION_UI_PROBE_SCOPE(name) ::trion::ui::probe::Scope TRION_UI_PROBE_CAT(trionUiProbeScope_, __LINE__)(name)

struct Stall {
    Clock::duration duration; // độ dài thật khi luồng UI đã chạy tiếp; còn treo thì là độ dài đến lúc báo
    std::string where;        // "KeyPress -> QLineEdit [vùng đo]"
    bool live;                // luồng watchdog bắt được khi đang khựng
};
std::vector<Stall> stalls(); // mọi luồng; giữ tối đa 1000 lần đầu, "count" trong JSON đếm đủ

QJsonObject toJson(); // mọi luồng
void reset();         // xóa mẫu và danh sách khựng

// Cho test: số lần luồng watchdog thức dậy, số sự kiện filter đã thấy (chỉ tăng khi bật).
struct Counters {
    std::uint64_t watchdogWakeups = 0;
    std::uint64_t events = 0;
};
Counters counters();

} // namespace trion::ui::probe
