#pragma once
// Số, dung lượng, ngày giờ, thời lượng ở giao diện chung (UI0-005, 07.1 §13.4, DoD-U 6) theo ngôn ngữ giao diện
// hiện tại (Translator::instance()->language(); chưa có Translator: vi). Không theo locale của hệ điều hành hay
// QLocale::setDefault: cùng ngôn ngữ cho cùng chuỗi trên mọi máy. Gọi trên luồng UI.
//
//                   vi (cả vi-XL)                         en
//   formatCount     1.250 · 1.250.000                     1,250 · 1,250,000
//   formatNumber    12,5                                  12.5
//   formatBytes     512 B · 12,5 MB (cơ số 1024)          512 B · 12.5 MB
//   formatDate      05/03/2026 (dd/mm/yyyy, cả hai ngôn ngữ, 07.1 §13.4)
//   formatTime      14:05 · formatDateTime 05/03/2026 14:05
//   formatDuration  850 ms · 1,5 giây · 2 phút 5 giây     850 ms · 1.5 s · 2 min 5 s · 1 h 5 min
//
// KHÔNG dùng cho số của bản vẽ: tọa độ, kích thước, góc, tỉ lệ trong vùng vẽ, dòng lệnh, thanh trạng thái,
// Properties, ô nhập số UI-C09. Các số đó luôn dấu chấm thập phân theo UNITS, không đổi khi đổi ngôn ngữ, và chỉ
// định dạng bằng core/regen/numfmt (RGN-013: formatLinear, formatAngle).

#include <QDate>
#include <QDateTime>
#include <QString>
#include <QTime>

namespace trion::i18n {

QString formatCount(qint64 n);                   // số đếm: 1.250 / 1,250
QString formatNumber(double value, int decimals); // số thập phân cố định số chữ số: 12,5 / 12.5
QString formatBytes(qint64 bytes);               // B, KB, MB, GB, TB, PB, EB; dưới 100 một chữ số lẻ
QString formatDate(QDate date);                  // "" nếu không hợp lệ
QString formatTime(QTime time);                  // 24 giờ, không giây
QString formatDateTime(const QDateTime& dt);     // theo múi giờ của dt (người gọi đổi sang giờ máy nếu cần)
QString formatDuration(qint64 ms);               // < 0 coi như 0

} // namespace trion::i18n
