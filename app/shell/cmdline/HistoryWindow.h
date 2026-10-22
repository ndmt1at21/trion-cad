#pragma once
// UIA-007 · Lịch sử dòng lệnh đầy đủ (F2, lệnh TEXTSCR; "Cửa sổ văn bản" UI-I03 của prototype: SC.textWindow trong
// js/screens1.js, 760×480, không chế độ). Thân: hàng công cụ (.pal-tools: ô tìm "Tìm trong nội dung", nút Sao chép), dưới
// là danh sách ảo hóa (VirtualTableView, UI0-108) mọi dòng của CommandLine, chữ Mono --fs-s; mở ra ở cuối (dòng mới
// nhất), dòng mới của dòng lệnh vào ngay (đang ở cuối thì cuộn theo).
//
// Tìm: chỉ hiện các dòng chứa chữ gõ, không phân biệt dấu và hoa/thường (text::foldForSearch), đoạn khớp tô --c-warn-soft
// như <mark> của prototype; không dòng nào khớp: EmptyState. Khóa tìm (chữ đã fold của từng dòng) dựng ở nền ngay khi mở
// (NT-005); mỗi lượt tìm chạy ở nền trên khóa đó, lượt mới hủy lượt cũ; kết quả về luồng UI qua ui::post.
// Sao chép (nút; Ctrl+C trong danh sách): các dòng đang chọn, không chọn thì mọi dòng đang hiện, nguyên văn, nối "\n".
// ponytail: "Lưu ra tệp…" của prototype chưa có (cần hộp lưu tệp, ghi nền); dòng dài hơn bề rộng hiện rút gọn "…"
// (sao chép vẫn nguyên văn) vì danh sách cao dòng đều, không xuống dòng.
//
// Dùng (bước integration, với DialogHost của UI0-110):
//   dialogs.add(u"textscr"_s, [cmd](QWidget* parent) { return new HistoryWindow(*cmd, parent); });
//   router.add(u"TEXTSCR"_s, [&] { dialogs.open(u"textscr"_s); });  // F2 (QAction TEXTSCR), nút ^ của dòng lệnh

#include "app/widgets/dialog/DialogFrame.h"
#include "core/base/sched/Scheduler.h"

#include <QStringList>
#include <QStringView>

#include <optional>
#include <vector>

namespace trion::ui {

class CommandLine;
class EmptyState;

class HistoryWindow : public DialogFrame {
    Q_OBJECT
public:
    // source sống lâu hơn cửa sổ hoặc hủy trước (thôi theo dòng mới). Cần ThemeManager, IconProvider, UiExecutor.
    explicit HistoryWindow(CommandLine& source, QWidget* parent = nullptr);
    ~HistoryWindow() override;

    int rowCount() const;           // dòng đang hiện (sau lọc)
    QString rowText(int row) const; // chữ nguyên văn
    void copy();                    // nút Sao chép

    // Vị trí các dòng (đã fold) chứa needle (đã fold, khác rỗng), tăng dần. Hàm thuần, chạy ở luồng nền.
    static std::vector<int> find(const QStringList& folded, QStringView needle);

signals:
    void searched(); // lọc theo ô tìm đã áp (kể cả khi xóa ô tìm)

protected:
    void changeEvent(QEvent* e) override;

private:
    class Model;
    class Delegate;
    class List;

    void search();
    void run(sched::CancelSource& source, const QString& needle);
    void adopt(QStringList folded, std::vector<int> rows, const QString& needle);
    void filter(std::optional<std::vector<int>> rows, const QString& needle);
    void appended(const QString& line);
    void retranslate();

    Model* model_ = nullptr;
    Delegate* delegate_ = nullptr;
    List* list_ = nullptr;
    SearchField* field_ = nullptr;
    Button* copy_ = nullptr;
    EmptyState* empty_ = nullptr;
    QStringList folded_;          // khóa tìm của các dòng đầu, cùng thứ tự với model (đủ khi đã dựng xong)
    sched::CancelSource warm_, job_; // dựng khóa lúc mở; lượt tìm
};

} // namespace trion::ui
