#pragma once
// UI0-013 · Thông báo cho trình đọc màn hình (như aria-live): toast, banner, chú thích (UI0-009), tiến trình (UI0-112:
// bắt đầu, xong, lỗi; không đọc từng phần trăm).
//
//   trion::a11y::announce(toast, text);                                        // role="status"
//   trion::a11y::announce(toast, text, trion::a11y::Politeness::Assertive);    // role="alert": ngắt lời đang đọc
//
// Chỉ header (inline, chỉ cần QtGui): thành phần widget gọi được mà không link trion_a11y, nên bản build chỉ gồm vài
// thành phần (TRION_COMPONENTS) không phải cấu hình thêm app/core/a11y.
// Không có trình đọc màn hình (QAccessible::isActive() sai): không làm gì.

#include <QAccessible>
#include <QCoreApplication>
#include <QString>

namespace trion::a11y {

enum class Politeness { Polite, Assertive };

// source: đối tượng phát (widget đang hiện thông điệp); trình đọc màn hình lấy ngữ cảnh (cửa sổ) từ nó, nên nên là
// widget. nullptr: ứng dụng.
inline void announce(QObject* source, const QString& text, Politeness politeness = Politeness::Polite)
{
    if (text.isEmpty() || !QAccessible::isActive())
        return;
    QAccessibleAnnouncementEvent e(source ? source : QCoreApplication::instance(), text);
    e.setPoliteness(politeness == Politeness::Assertive ? QAccessible::AnnouncementPoliteness::Assertive
                                                        : QAccessible::AnnouncementPoliteness::Polite);
    QAccessible::updateAccessibility(&e);
}

} // namespace trion::a11y
