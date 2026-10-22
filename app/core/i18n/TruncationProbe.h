#pragma once
// Tìm chữ bị cắt trên giao diện (UI0-005; 07.3 Phụ lục C: chuỗi tiếng Việt dài hơn 40% không bị cắt). Dùng cùng
// ngôn ngữ giả vi-XL; QA-011 gọi trên mọi màn hình, mỗi cửa sổ cấp cao một lần:
//
//   Translator::instance()->setLanguage(kPseudoLanguage.toString());
//   QCoreApplication::processEvents();   // LanguageChange → retranslate(), rồi bố cục lại
//   for (const Truncation& t : findTruncations(window))
//       qWarning("%s", qUtf8Printable(t.toString()));
//
// Quét root và mọi widget con đang hiện và thấy được (visibleRegion không rỗng; vùng cuộn: của viewport; trang tab nền
// Qt dời ra ngoài cửa sổ thì bỏ): QLabel; nút (QPushButton, QCheckBox, QRadioButton, QToolButton có hiện chữ); tab của
// QTabBar; đầu cột/dòng của QHeaderView; ô đang trong vùng nhìn của view (bảng, cây, danh sách); QMenu con của root
// (cả menu đang đóng: so với bề rộng tối đa của menu và màn hình). Bị cắt = cỡ cần để hiện đủ
// chữ, tính bằng chính style đang dùng, lớn hơn cỡ đang có theo chiều ngang hoặc dọc (chữ đã rút gọn "…" cũng
// tính). Chỉ đọc. root phải đang hiện và đã bố cục xong.
//
// ponytail: chưa quét tiêu đề QGroupBox/QDockWidget, chữ của QComboBox, placeholder ô nhập; menu chỉ báo mục có
// chữ dài nhất; phần mực của dấu chồng tầng vượt chiều cao font không đo (QA-011 xem ảnh chụp vi-XL).

#include <QList>
#include <QSize>
#include <QString>

class QWidget;

namespace trion::i18n {

struct Truncation {
    QWidget* widget = nullptr; // widget vẽ chữ: QLabel, nút, QTabBar, QHeaderView, view chứa ô, QMenu
    QString text;              // chữ bị cắt, nguyên văn (kể cả dấu & phím tắt)
    QSize need;                // cỡ cần để hiện đủ (px); chiều không đo để 0
    QSize have;                // cỡ đang có (px)

    QString toString() const;  // "QPushButton 'luu': «Lưu bản vẽ» cần 96×17, có 80×17" cho nhật ký QA
};

QList<Truncation> findTruncations(QWidget* root);

} // namespace trion::i18n
