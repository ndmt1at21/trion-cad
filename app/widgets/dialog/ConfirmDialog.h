#pragma once
// UI0-008 · Xác nhận hành động phá hủy hoặc khó hoàn tác (07.1 §12, §1 mục 4; prototype TC.confirm).
//
// Nút chính nêu hành động kèm số lượng ("Xóa 3 layout", không "OK"): hàm dựng của builder đòi nhãn và số lượng, nhãn
// phải có động từ và %1 (isActionLabel, TRION_ASSERT ở bản debug và sanitizer: dựng ở ngôn ngữ nào thì kiểm bản dịch
// đó). %1 trong nhãn và câu hỏi là số lượng, định dạng theo ngôn ngữ giao diện (formatCount: 1.250 / 1,250), đậm trong
// câu hỏi. Tên, giá trị khác trong câu hỏi: người gọi điền sẵn (giữ %1 cho số lượng).
//
//   ConfirmDialog(tr("Xóa layout"), tr("Xóa %1 layout", nullptr, n), n)   // n: lupdate tạo dạng số nhiều English
//       .question(tr("Xóa %1 layout đã chọn? Đối tượng trên các layout này cũng bị xóa.", nullptr, n))
//       .detail(tr("Hoàn tác được bằng Ctrl+Z."))
//       .danger()                     // nút chính đỏ (.btn.danger)
//       .dontAsk(u"layout.delete"_s)  // ô "Không hỏi lại"; mã lưu ở SuppressedPrompts
//       .ask(mainWindow, [=](bool yes) { if (yes) run(…); });
//
// ask(): đã nhớ "Không hỏi lại" thì gọi done(kết quả đã nhớ) ngay, không hiện gì, trả nullptr. Không thì dựng hộp thoại
// modal, mở bằng open() (không exec lồng), tự xóa khi đóng; done gọi đúng một lần lúc đóng: true = nút chính, false =
// Hủy, Esc, nút Đóng. Đã đánh dấu "Không hỏi lại" thì nhớ kết quả đó, kể cả Hủy (như "luôn làm theo lựa chọn này").
// Focus đầu: nút chính (Enter); irreversible() (không hoàn tác được) thì nút Hủy: Enter khi đó là Hủy, không làm nhầm.
// Chữ do người gọi dịch lúc dựng; hộp thoại modal ngắn nên không dịch lại khi đổi ngôn ngữ (trang Tùy chọn không mở
// được trong lúc nó hiện).
// ponytail: không có nút thứ ba (prototype o.alt) và sheet macOS (UI0-111).
#include <QCoreApplication>
#include <QString>

#include <functional>

class QWidget;

namespace trion::ui {

class DialogFrame;

class ConfirmDialog {
    Q_DECLARE_TR_FUNCTIONS(trion::ui::ConfirmDialog)
public:
    // title: tiêu đề hộp thoại, cũng là tên trong danh sách "Không hỏi lại" của Tùy chọn. action: nhãn nút chính đã
    // dịch, động từ và %1 = số lượng. count: số đối tượng chịu tác động, ≥ 1.
    ConfirmDialog(QString title, QString action, qint64 count);

    ConfirmDialog& question(QString text);       // câu hỏi; %1 = số lượng (đậm)
    ConfirmDialog& detail(QString text);         // dòng phụ nhỏ, mờ (.muted): hậu quả, cách hoàn tác
    ConfirmDialog& danger(bool on = true);       // nút chính kiểu Danger thay Primary
    ConfirmDialog& irreversible(bool on = true); // không hoàn tác được: focus đầu ở Hủy
    ConfirmDialog& dontAsk(QString id); // có ô "Không hỏi lại"; id: mã ổn định, ASCII ("layout.delete")

    DialogFrame* ask(QWidget* parent, std::function<void(bool accepted)> done) const;

    // Nhãn nút chính hợp lệ: có %1 và còn chữ khác ngoài %1, '&', khoảng trắng, không phải chữ chung chung (OK, Có,
    // Đồng ý, Xác nhận, Tiếp tục, Yes, Confirm, Continue, Accept; không phân biệt hoa/thường).
    // ponytail: danh sách tay, không kiểm được từ loại; thêm từ khi CI gặp.
    static bool isActionLabel(const QString& action);

private:
    QString title_;
    QString action_;
    qint64 count_;
    QString question_;
    QString detail_;
    QString dontAsk_;
    bool danger_ = false;
    bool irreversible_ = false;
};

} // namespace trion::ui
