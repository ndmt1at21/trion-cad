#pragma once
// UIB-002 · Hộp thoại Mở tệp (07.1 UI-B01, §13.6 hộp thoại tệp của hệ điều hành; prototype TC.files.open trong
// js/screens1.js: lưới vị trí | danh sách | xem trước 230 px, bộ lọc loại tệp, nút Mở thả xuống "Mở chỉ đọc").
//
// OpenFileDialog::open(done) hiện hộp chọn tệp của hệ điều hành, modal với cửa sổ cha:
// - Windows: IFileOpenDialog gốc + IFileDialogCustomize: nút Mở thả xuống (Mở, Mở chỉ đọc; EnableOpenDropDown), nhóm
//   nhãn thông tin (phiên bản, lưu lần cuối bởi, ngày sửa, kích thước) cập nhật theo OnSelectionChange qua
//   FilePreviewService (ảnh xem trước nằm ở khung xem trước của Explorer, PLF-115). Show() chặn trong vòng sự kiện modal
//   của hệ điều hành (sự kiện, kết quả nền của Qt vẫn chạy), nên done chạy trước khi open() trả về.
// - Nơi khác (Linux; macOS tới mốc macOS) và khi setNative(false): QFileDialog của Qt (không native) thêm FilePreviewPane
//   vào bên phải danh sách; nút Mở là SplitButton có menu (Mở, Mở chỉ đọc). open() trả về ngay, done chạy khi đóng.
// Chung: bộ lọc Bản vẽ (*.dwg), DXF (*.dxf), Mẫu bản vẽ (*.dwt), Mọi tệp CAD (không phân biệt hoa thường); chọn nhiều tệp;
// nhớ thư mục cuối (Store "file/openDir", mặc định thư mục Tài liệu); thư mục gần đây (setPlaces) ở cột vị trí; Esc, Hủy
// đóng hộp, focus về widget có focus lúc mở. Viewer, Viewer Pro (setViewer, UI0-011): mở được DWG, DXF, DWT, luôn chỉ
// đọc, không có mục "Mở chỉ đọc" riêng.
// Kết quả: done(OpenResult{đường dẫn tuyệt đối dấu '/', theo thứ tự hộp trả, chỉ đọc}); Hủy: paths rỗng. Sau done, mỗi
// tệp một macroRequested(openMacro(path, readOnly)) theo thứ tự.
//
// Dùng (bước integration, luồng UI, sau ThemeManager, IconProvider, UiExecutor; FilePreviewService giữ ở Application):
//   auto* openDlg = new trion::files::OpenFileDialog(store, *previews, &mainWindow);
//   openDlg->setViewer(gate.profile() == UiProfile::Viewer);          // và theo EditionGate::profileChanged
//   router.add(u"OPEN"_s, [=] {                                        // Ctrl+O, QAT, menu ứng dụng, trang Start
//       openDlg->setPlaces(recent.folders(6));
//       openDlg->open([=](const OpenResult& r) { if (!r.paths.isEmpty()) app->requestOpen(r.paths); });
//   });
// ponytail: chỗ nối INT-002/DOC-018/CMD-240: macroRequested phát mỗi tệp một chuỗi lệnh
// "^C^C_OPEN \"D:/DuAn/Bản vẽ.dwg\"" (chỉ đọc: "_ReadOnly " trước đường dẫn; dấu '/' vì '\' là "dừng chờ nhập" trong
// macro). Bộ định tuyến tạm bỏ đối số nên OPEN có đường dẫn lại mở hộp này: chỉ nối macroRequested vào router khi INT-002
// tách OPEN có đường dẫn (CMD-240: FILEDIA=1 gọi open(done) rồi tự mở các tệp, cờ chỉ đọc của DOC-018). macOS: NSOpenPanel
// dạng sheet có accessory view xem trước làm ở mốc macOS (T26–27), tới đó dùng hộp của Qt.

#include "app/dialogs/file/FilePreviewService.h"
#include "core/base/settings/Settings.h"

#include <QPointer>
#include <QStringList>
#include <QWidget>

#include <functional>
#include <memory>

class QFileDialog;
class QLabel;

namespace trion::settings {
class Store;
}
namespace trion::ui {
class Button;
class Glyph;
class SplitButton;
} // namespace trion::ui

namespace trion::files {

struct OpenResult {
    QStringList paths;     // tuyệt đối, dấu '/' (UNC: "//máy/chia-sẻ/…"); rỗng: Hủy, Esc
    bool readOnly = false; // "Mở chỉ đọc"; Viewer: luôn true
};

// Khung xem trước bên phải danh sách tệp (prototype #op-prev): ảnh 3:2 (không có ảnh: chữ "Không có ảnh xem trước"),
// tên tệp, Phiên bản, Lưu lần cuối bởi, Ngày sửa, Kích thước; lỗi đọc: biểu tượng cảnh báo và câu giải thích; chọn nhiều
// tệp: "N tệp sẽ mở thành N tab"; chưa chọn: lời gợi ý. Màu, cỡ đọc token khi vẽ.
class FilePreviewPane final : public QWidget {
    Q_OBJECT
public:
    static constexpr int kWidth = 230;      // cột xem trước của prototype (grid 190px | 1fr | 230px)
    static constexpr int kLabelWidth = 90;  // --lab-w của .frow trong khung xem trước

    explicit FilePreviewPane(QWidget* parent = nullptr);

    void showHint();                                     // chưa chọn tệp
    void showLoading(const QString& path, int count);    // đang đọc: tên tệp, giá trị "…"
    void showInfo(const FilePreviewInfo& info, int count); // count: số tệp đang chọn
    void setCount(int count);                              // nhãn "N tệp sẽ mở thành N tab" khi N > 1

    QString path() const { return path_; }   // tệp đang hiện, rỗng khi gợi ý
    bool loading() const { return loading_; }
    // Chữ của từng dòng (test): Phiên bản, Lưu lần cuối bởi, Ngày sửa, Kích thước; lỗi; nhãn số tệp.
    QString value(int row) const;
    QString errorText() const;
    QString countText() const;
    const QImage& thumbnail() const { return thumbnail_; }

protected:
    void changeEvent(QEvent* e) override;
    bool eventFilter(QObject* watched, QEvent* e) override; // vẽ ô ảnh, nhãn số tệp

private:
    void retranslate();
    void applyTheme();

    QWidget* image_;
    QLabel* name_;
    QWidget* form_;
    QLabel* labels_[4];
    QLabel* values_[4];
    QWidget* errorRow_;
    ui::Glyph* errorIcon_;
    QLabel* error_;
    QLabel* count_;
    QLabel* hint_;
    QImage thumbnail_;
    QString path_;
    int countValue_ = 0;
    bool loading_ = false;
    bool hasInfo_ = false;
    FilePreviewInfo info_;
};

class OpenFileDialog final : public QObject {
    Q_OBJECT
public:
    using Callback = std::function<void(const OpenResult&)>;
    enum Filter { Dwg, Dxf, Dwt, AllCad, FilterCount }; // thứ tự trong ô loại tệp; mặc định Dwg (prototype)

    inline static const settings::Setting<QString> kLastDir{"file/openDir", QString()};

    // store, previews sống lâu hơn hộp thoại; parent: cửa sổ chính (modal với nó, focus trả về nó).
    OpenFileDialog(settings::Store& store, FilePreviewService& previews, QWidget* parent);
    ~OpenFileDialog() override;

    void setViewer(bool viewer);
    bool viewer() const { return viewer_; }
    void setPlaces(const QStringList& folders); // thư mục gần đây cho cột vị trí
    // Windows: true (mặc định) dùng IFileOpenDialog gốc; false dùng hộp của Qt. Nơi khác luôn là hộp của Qt.
    void setNative(bool native) { native_ = native; }
    bool native() const;

    // Hiện hộp thoại; đang mở thì đưa lên trước, done mới thay done cũ. done chạy một lần trên luồng UI.
    void open(Callback done = {});
    bool isOpen() const { return open_; }

    static QString filterLabel(Filter f);       // "Bản vẽ (*.dwg)"…: theo ngôn ngữ giao diện
    static QStringList filterPatterns(Filter f); // {"*.dwg"}; AllCad: cả ba
    static bool matches(const QString& fileName, Filter f); // không phân biệt hoa thường
    static QString openMacro(const QString& path, bool readOnly);
    // Chữ dòng row của thông tin (0 phiên bản, 1 lưu lần cuối bởi, 2 ngày sửa, 3 kích thước); dùng chung hộp gốc, khung.
    static QString infoValue(const FilePreviewInfo& info, int row);

    // Hộp của Qt đang dùng (test, trợ năng); nullptr khi chưa mở lần nào hoặc đang dùng hộp gốc.
    QFileDialog* qtDialog() const;
    FilePreviewPane* previewPane() const { return pane_; }

#if defined(Q_OS_WIN)
    // Windows, không hiện (test): dựng IFileOpenDialog đủ cấu hình như open(). true nếu mọi lời gọi COM thành công.
    bool prepareNative();
    // Chữ nhãn thông tin đã đặt cho hộp gốc (test): Phiên bản, Lưu lần cuối bởi, Ngày sửa, Kích thước.
    QStringList nativeInfo() const;
#endif

signals:
    void macroRequested(const QString& macro); // mỗi tệp một lệnh OPEN, sau done

protected:
    bool eventFilter(QObject* watched, QEvent* e) override; // nút Mở chuẩn bật/tắt, đổi ngôn ngữ

private:
    struct Native; // Windows: IFileOpenDialog và sự kiện của nó

    void openQt();
    void createQt();
    void retranslateQt();
    void selectionChanged();
    void previewReady(const FilePreviewInfo& info);
    void finish(const OpenResult& result);
    QString startDir() const;

    settings::Store& store_;
    FilePreviewService& previews_;
    QPointer<QWidget> parent_;
    QPointer<QWidget> returnFocus_; // widget có focus lúc mở
    QPointer<QFileDialog> qt_; // con của cửa sổ cha
    FilePreviewPane* pane_ = nullptr;
    ui::SplitButton* openSplit_ = nullptr;
    ui::Button* openPlain_ = nullptr; // Viewer: nút Mở không thả xuống
    QStringList places_;
    QStringList accepted_; // filesSelected của lần chấp nhận
    Callback done_;
    std::unique_ptr<Native> win_;
    int selected_ = 0;      // số tệp đang chọn (hộp của Qt)
    bool viewer_ = false;
    bool native_ = true;
    bool open_ = false;
    bool readOnly_ = false; // mục "Mở chỉ đọc" vừa chọn
};

} // namespace trion::files
