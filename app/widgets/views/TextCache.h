#pragma once
// Chữ một dòng đã shaping sẵn (QStaticText) cho delegate tự vẽ của danh sách ảo hóa (thẻ, dòng bảng của tab Start,
// palette Layer của Viewer): lọc, cuộn vẽ lại các dòng không shaping lại (~20–40 µs mỗi chuỗi). Khóa: bề rộng ô, cách
// rút gọn, chữ. Mỗi bản một font: đổi font (theme, mật độ) thì bỏ hết; delegate vẽ nhiều font thì mỗi font một bản.
// ponytail: QCache bỏ chuỗi lâu không dùng khi quá kCapacity mục (vài trăm dòng × vài chuỗi × hai bề rộng ô).
//
// Dùng (luồng UI, trong paint của delegate):
//   mutable ui::TextCache names_, counts_;
//   names_.draw(*p, option.font, rect, index.data().toString());          // căn trái, thiếu chỗ thì "…" bên phải
//   counts_.draw(*p, mono, rect, count, Qt::ElideRight, Qt::AlignRight);   // căn phải

#include <QCache>
#include <QFont>
#include <QStaticText>
#include <QString>

class QPainter;
class QRect;

namespace trion::ui {

class TextCache {
public:
    static constexpr int kCapacity = 2000;

    // Vẽ text trong box bằng font và bút của p: căn trái (align có Qt::AlignRight: căn phải), giữa theo chiều dọc;
    // rộng hơn box thì rút gọn theo mode. Trả bề rộng đã vẽ; box rộng ≤ 0 thì không vẽ, trả 0.
    qreal draw(QPainter& p, const QFont& font, const QRect& box, const QString& text,
               Qt::TextElideMode mode = Qt::ElideRight, Qt::Alignment align = Qt::AlignLeft);

    qsizetype size() const { return cache_.size(); } // số dòng đang giữ

private:
    QFont font_;
    QCache<QString, QStaticText> cache_{kCapacity};
};

} // namespace trion::ui
