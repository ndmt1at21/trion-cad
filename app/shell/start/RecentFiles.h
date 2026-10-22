#pragma once
// UIA-004 · Dịch vụ tệp gần đây (07.1 UI-A02 mục 3, UI-A03 cột phải; prototype TC.RECENT trong js/scene.js, SC.start và
// SC.appMenu trong js/screens1.js). Dùng chung cho tab Start (StartPage) và menu ứng dụng (UIA-003).
//
// RecentFiles: model bảng phẳng, một dòng một tệp theo thứ tự mở (mở gần nhất ở dòng 0): tên, đường dẫn, ngày sửa, kích
// thước, phiên bản DWG, ghim, "Không tìm thấy". Lưu trong Store "recent/files" (JSON) ngay khi đổi (Store gom và ghi
// nền), nạp lại lúc tạo; tối đa Store "recent/limit" mục (mặc định kDefaultLimit [đề xuất]): thêm quá thì bỏ mục cũ nhất
// chưa ghim, mục ghim không bị bỏ.
// refresh(): kiểm lại mọi tệp ở luồng nền (sched, Background): còn ở đường dẫn không (đã xóa, đổi tên, ổ mạng mất →
// missing), kích thước, ngày sửa, phiên bản đọc từ mã đầu tệp (AC1032 → "DWG 2018"; DXF: biến $ACADVER). Kết quả về luồng
// UI qua ui::post thành một lô (dataChanged), luồng UI không chạm đĩa. Trong lúc chờ, model hiện số liệu đã lưu.
//
// RecentFilter: proxy của từng view (tab Start, menu ứng dụng): lọc theo tên và thư mục không phân biệt dấu, hoa/thường
// (Fold.h, "ket cau" khớp "Kết cấu"; khóa gấp sẵn mỗi mục, mỗi phím chỉ gấp chữ gõ), sắp mục ghim lên đầu rồi theo ngày
// sửa (mới trước), tên (A→Z) hoặc kích thước (lớn trước); bằng nhau thì theo thứ tự mở.
//
// Dùng (main, luồng UI, sau Store, UiExecutor, Translator):
//   RecentFiles recent(store);                    // main giữ; StartPage, AppMenu dùng chung
//   recent.add(path);                             // phiên DOC-018 báo tệp vừa mở hoặc lưu thành
//   RecentFilter menuList(recent); menuList.setQuery(text);
//
// ponytail: đường dẫn so bằng QDir::cleanPath (không phân biệt hoa/thường trên Windows, macOS), kiểm tồn tại bằng
// QFileInfo mỗi lần refresh(): NT-011 (chuẩn hóa đường dẫn, FileWatcher theo dõi xóa/đổi tên) thay. Phiên bản đọc 6
// byte đầu (DXF: 4 KB đầu): DWG-026 readPreview trả phiên bản thay. Tên tệp, thư mục không theo dõi đổi ngôn ngữ (không
// dịch); chữ ngày, dung lượng theo UiFormat lúc data() được gọi.

#include "core/base/sched/Scheduler.h"

#include <QAbstractTableModel>
#include <QDateTime>
#include <QSortFilterProxyModel>
#include <QString>

#include <cstdint>
#include <vector>

namespace trion::settings {
class Store;
}

namespace trion::shell {

class RecentFiles final : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column { Name, Modified, Size, Version, Pin, ColumnCount }; // cột của dạng danh sách (.rf-list)
    enum Role {
        PathRole = Qt::UserRole + 1, // QString: đường dẫn đầy đủ, dấu '/'
        FolderRole,                  // QString: thư mục chứa theo hệ điều hành ("D:\DuAn\KetCau")
        PinnedRole,                  // bool
        MissingRole,                 // bool: không thấy tệp ở đường dẫn (lần refresh gần nhất)
        MetaRole,                    // QString: "16/10/2026 · 4,8 MB · DWG 2018" cho thẻ của dạng lưới
    };

    struct Entry {
        QString path{};        // tuyệt đối, dấu '/'
        QString name{};        // tên tệp
        QDateTime modified{};  // giờ sửa của tệp (UTC); không hợp lệ: chưa biết
        qint64 size = -1;      // byte; -1: chưa biết
        QString version{};     // "DWG 2018", "DXF 2013", "DWG R14"; rỗng: chưa biết hoặc không đọc được
        bool pinned = false;
        bool missing = false;  // không còn ở đường dẫn: đã xóa, đổi tên, ổ mạng không vào được
        QString folded{};      // khóa tìm: tên và thư mục đã gấp dấu
    };

    static constexpr int kDefaultLimit = 200; // [đề xuất] Tùy chọn › Mở & Lưu (UI-D10) đổi "recent/limit"

    explicit RecentFiles(settings::Store& store, QObject* parent = nullptr); // luồng UI; nạp từ store
    ~RecentFiles() override; // hủy lần kiểm tệp đang chạy (kết quả về sau bị bỏ)

    // Tệp vừa mở hoặc lưu: lên dòng 0 (đã có thì giữ ghim), rồi kiểm lại thông tin ở nền.
    void add(const QString& path);
    // Xóa khỏi danh sách (tệp trên ổ đĩa không bị xóa). false nếu không có.
    bool remove(const QString& path);
    // Đặt lại mục ở dòng row (hoàn tác "Xóa khỏi danh sách"); đã có đường dẫn đó thì bỏ qua.
    void restore(int row, const Entry& entry);
    void setPinned(const QString& path, bool pinned);

    int indexOf(const QString& path) const; // -1 nếu không có
    const Entry& entry(int row) const { return entries_[std::size_t(row)]; }
    int size() const { return int(entries_.size()); }
    // Thư mục chứa theo thứ tự mở, không trùng, tối đa max ("Mở thư mục gần đây").
    QStringList folders(int max) const;
    int limit() const;

    // Kiểm lại mọi tệp ở nền; lần gọi mới hủy lần đang chạy. refreshed() khi kết quả đã áp.
    void refresh();
    bool refreshing() const { return refreshing_; }

    // Tên trong phiên bản từ mã đầu tệp: "AC1032" → "DWG 2018"; mã lạ → chính mã đó; không phải mã AC → rỗng.
    static QString versionName(QByteArrayView code, bool dxf);
    // Đọc phiên bản từ tệp (luồng nào cũng được): DWG/DWT 6 byte đầu, DXF biến $ACADVER trong 4 KB đầu.
    static QString readVersion(const QString& path);

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

signals:
    void refreshed();
    // Tệp đã biết đổi ngày sửa hoặc kích thước (lần kiểm sau): ảnh xem trước cũ không còn đúng.
    void fileChanged(const QString& path);

private:
    struct Probe; // kết quả kiểm một tệp ở nền
    void load();
    void save();
    void trim();
    void apply(std::uint64_t generation, const std::vector<Probe>& probes);
    static void fold(Entry& e);

    settings::Store& store_;
    std::vector<Entry> entries_;
    sched::CancelSource cancel_;
    std::uint64_t generation_ = 0;
    bool refreshing_ = false;
};

// Lọc, sắp cho một view. Nguồn sống lâu hơn proxy.
class RecentFilter final : public QSortFilterProxyModel {
    Q_OBJECT
public:
    enum class Sort : std::uint8_t { Date, Name, Size };

    explicit RecentFilter(RecentFiles& files, QObject* parent = nullptr);

    RecentFiles& files() const { return files_; }
    void setQuery(const QString& text); // gõ: gấp dấu một lần rồi lọc lại
    QString query() const { return query_; }
    void setSortKey(Sort sort);
    Sort sortKey() const { return sort_; }
    const RecentFiles::Entry& entry(int row) const; // dòng của proxy

protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    RecentFiles& files_;
    QString query_, folded_;
    Sort sort_ = Sort::Date;
};

} // namespace trion::shell
