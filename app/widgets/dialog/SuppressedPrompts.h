#pragma once
// UI0-008 · "Không hỏi lại" của hộp thoại xác nhận (07.1 §12): kết quả đã nhớ, lưu ở Store (NT-010) khóa
// "ui/suppressedPrompts" = {"<mã>": {"title": "Xóa layout", "accepted": true}}, nên sống qua khởi động lại.
// ConfirmDialog đọc, ghi qua remembered/remember. Trang Tùy chọn (UID-019) hiện model này, khôi phục từng mục, tất cả:
//
//   auto* hidden = new SuppressedPrompts(Store::instance(), page); // dòng: tiêu đề (DisplayRole), mã (IdRole), theo mã
//   view->setModel(hidden);
//   connect(restoreAll, &QPushButton::clicked, hidden, &SuppressedPrompts::restoreAll);
//   hidden->restore(view->currentIndex().row());                    // hỏi lại một hộp thoại
//
// Model tự nạp lại khi khóa đổi (cả khi ConfirmDialog nhớ thêm trong lúc trang đang mở). Luồng UI.
// ponytail: tiêu đề lưu theo ngôn ngữ lúc người dùng đánh dấu, đổi ngôn ngữ thì danh sách vẫn chữ cũ; cần thì lưu chuỗi
// nguồn và dịch lúc hiện. Đổi gì cũng nạp lại cả model (reset), danh sách chỉ vài chục dòng.
#include "core/base/settings/Settings.h"

#include <QAbstractListModel>
#include <QList>
#include <QString>

#include <optional>
#include <utility>

namespace trion::ui {

class SuppressedPrompts : public QAbstractListModel {
    Q_OBJECT
public:
    enum Role { IdRole = Qt::UserRole + 1 }; // mã hộp thoại (QString)

    explicit SuppressedPrompts(settings::Store& store = settings::Store::instance(), QObject* parent = nullptr);

    // Kết quả đã nhớ của hộp thoại id: true = làm, false = hủy; chưa nhớ (hoặc dữ liệu hỏng): nullopt.
    static std::optional<bool> remembered(const QString& id,
                                          const settings::Store& store = settings::Store::instance());
    // Nhớ kết quả của id; title là tên hiện trong Tùy chọn.
    static void remember(const QString& id, const QString& title, bool accepted,
                         settings::Store& store = settings::Store::instance());

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    void restore(int row); // hỏi lại hộp thoại ở dòng row; ngoài khoảng thì bỏ qua
    void restoreAll();

private:
    void reload();

    settings::Store& store_;
    QList<std::pair<QString, QString>> rows_; // mã, tiêu đề
};

} // namespace trion::ui
