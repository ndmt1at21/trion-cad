#pragma once
// UIH-001 · Trạng thái của hộp thoại In (UI-H02): một nguồn cho mọi trường. Widget đổi → setter (kiểm giá trị, áp quy
// tắc liên đới qua plot::normalize) → changed() nếu thiết lập đổi → hộp thoại đồng bộ mọi widget từ settings().
// Không widget; test được riêng; -PLOT (parsePlotCommand) dùng cùng giá trị ban đầu và quy tắc.
//
// Quy tắc (như AutoCAD, 07.2 UI-H02):
//   thiết bị      PDF, PNG của TrionCAD: In ra tệp bắt buộc. In ra tệp: số bản 1 và khóa. Đổi thiết bị: khổ đang chọn
//                 không có ở thiết bị mới thì lấy khổ cùng cỡ (±0,5 mm), không có nữa thì khổ mặc định của thiết bị.
//   vùng in       Layout chỉ ở layout, Giới hạn chỉ ở Model, View khi bản vẽ có view có tên; vùng Layout không canh
//                 giữa; Cửa sổ chưa chọn thì chưa in được (canPlot).
//   tỉ lệ         Vừa khổ giấy, hoặc mm = đơn vị (> 0); tỉ lệ chuẩn 1:N là 1 = N.
//   tùy chọn in   In theo kiểu in thì in độ dày nét bật và khóa; Paper space sau cùng, ẩn đối tượng Paper space chỉ ở
//                 layout. Lưu thay đổi vào layout: khóa theo phiên bản do hộp thoại xử lý.
//   thiết lập     chọn thiết lập có tên hoặc <Lần in trước>: chép mọi trường của nó; <Không>: giữ giá trị đang sửa.

#include "app/dialogs/plot/PlotSettings.h"

#include <QObject>

namespace trion::ui {

class PlotSettingsModel : public QObject {
    Q_OBJECT
public:
    // Lựa chọn của ô "Thiết lập trang" ngoài tên thiết lập có tên.
    static inline const QString kNoSetup;                        // <Không>
    static inline const QString kPreviousSetup = QStringLiteral("\x01previous"); // <Lần in trước>

    enum class Option : std::uint8_t {
        Background,     // In ở nền
        Lineweights,    // In độ dày nét
        Transparency,   // In độ trong suốt
        PlotStyles,     // In theo kiểu in
        PaperSpaceLast, // In Paper space sau cùng
        HidePaperSpace, // Ẩn đối tượng Paper space
        Stamp,          // Bật dấu in
        SaveToLayout,   // Lưu thay đổi vào layout (Pro)
    };
    static constexpr int kOptionCount = 8;

    // Giá trị ban đầu theo plot::initialSettings; máy in hệ thống từ cache của lần quét trước (setSystemPrinters khi
    // lần quét mới xong). Thiết lập đang chọn: thiết lập có tên mà layout đang gán, không thì <Không>.
    explicit PlotSettingsModel(plot::PlotContext context, QObject* parent = nullptr);

    const plot::PlotContext& context() const { return ctx_; }
    const plot::PlotSettings& settings() const { return s_; }

    // Danh sách cho ô chọn.
    const QList<plot::Device>& devices() const { return devices_; } // dựng sẵn, rồi máy in hệ thống
    const plot::Device* device() const; // thiết bị đang chọn; nullptr: không có trong danh sách (máy in vắng)
    QList<plot::Paper> papers() const;  // khổ của thiết bị; thiết bị vắng: chỉ khổ đang đặt
    QList<plot::AreaKind> areas() const;
    bool printersLoaded() const { return printersLoaded_; }
    void setSystemPrinters(const QList<plot::Device>& printers); // kết quả quét nền

    QString setup() const { return setup_; }
    void selectSetup(const QString& choice); // kNoSetup, kPreviousSetup hoặc tên thiết lập có tên

    void setDevice(const QString& name);
    void setPaper(const QString& name);
    void setCopies(int copies); // kẹp 1–999
    void setToFile(bool on);
    void setArea(plot::AreaKind kind);
    void setView(const QString& name);
    void setWindow(const QRectF& window); // hai góc theo thứ tự bất kỳ
    void setCentered(bool on);
    void setOffset(double x, double y);
    void setFit(bool on);
    void setStandardScale(int index); // chỉ số trong plot::standardScales()
    void setCustomScale(double mm, double units);
    void setScaleLineweights(bool on);
    void setPlotStyle(const QString& name); // rỗng: không dùng kiểu in
    void setShowPlotStyles(bool on);
    void setShadeMode(plot::ShadeMode mode);
    void setShadeQuality(plot::ShadeQuality quality);
    void setShadeDpi(int dpi);
    void setOption(Option option, bool on);
    void setOrientation(plot::Orientation orientation);
    void setUpsideDown(bool on);

    bool option(Option option) const;
    bool optionEnabled(Option option) const;
    bool toFileForced() const;
    bool copiesEnabled() const { return !s_.toFile; }
    bool centerEnabled() const { return s_.setup.area.kind != plot::AreaKind::Layout; }
    int scaleIndex() const; // tỉ lệ chuẩn đang dùng; -1: tùy chỉnh
    bool canPlot() const;   // Cửa sổ chưa chọn: false

signals:
    void changed(); // thiết lập, lựa chọn thiết lập hoặc danh sách thiết bị đã đổi

private:
    template <class Edit>
    void edit(Edit apply);

    plot::PlotContext ctx_;
    QList<plot::Device> devices_;
    plot::PlotSettings s_;
    QString setup_;
    bool printersLoaded_ = false;
};

} // namespace trion::ui
