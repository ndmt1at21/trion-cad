#pragma once
// Chủ đề giao diện (UI0-002): Tối/Sáng/Theo hệ thống, mật độ gọn/thoáng, Giao diện nhẹ, giảm chuyển động.
// Lựa chọn lưu qua Store: "ui/theme" (dark|light|system), "ui/density" (compact|comfortable), "ui/lite".
//
// Cách áp, chọn theo số đo (tests/theme_test.cpp, 2.000 widget): QPalette toàn ứng dụng mang màu chủ đề; TrionStyle
// (QProxyStyle trên Fusion) đọc token cho chiều cao theo mật độ, thời gian chuyển động và vẽ phẳng nút, ô nhập,
// ô chọn, menu, tab, tooltip; QSS sinh từ qss/app.qss.in chỉ gắn vào lớp ít thể hiện (đầu cột, thanh tiến trình).
// QSS toàn ứng dụng phải polish lại mọi widget mỗi lần đổi (~1 s với 2.000 widget) và palette() trong QSS bị đóng
// băng lúc polish, nên không dùng.
//
// Widget tự vẽ (UI0-003, UI0-004, UI0-106…): đọc token ngay trong paintEvent/sizeHint, không cache giá trị:
//   auto& t = trion::theme::ThemeManager::instance();
//   p.fillRect(r, t.color(tok::c_chrome)); int h = qRound(t.px(tok::h_row)); anim.setDuration(t.ms(tok::t));
//   p.setPen(t.color(Sem::GripHot)); p.setFont(t.font(tok::fs_s));
// Đổi chủ đề: Qt gửi PaletteChange (→ update); đổi mật độ: StyleChange (→ update, updateGeometry), tự động.
// Dùng thời gian, bóng (Giao diện nhẹ), màu ngữ nghĩa, hoặc có cache: connect(&t, &ThemeManager::changed, …).

#include "app/theme/SemanticColors.h"
#include "app/theme/generated/Tokens.h"
#include "core/base/settings/Settings.h"

#include <QColor>
#include <QFont>
#include <QObject>
#include <QPalette>
#include <QString>

namespace trion::theme {

enum class Theme { Dark, Light, System };
enum class Density { Compact, Comfortable };

inline const settings::Setting<QString> kTheme{"ui/theme", QStringLiteral("dark")};
inline const settings::Setting<QString> kDensity{"ui/density", QStringLiteral("compact")};
inline const settings::Setting<bool> kLite{"ui/lite", false};

class ThemeManager : public QObject {
    Q_OBJECT
public:
    // Nạp lựa chọn từ store và áp ngay cho QApplication (nếu có). Theo dõi store: ai ghi các khóa trên
    // (setter dưới đây, UI-D10, nhập profile) thì chế độ mới được áp.
    explicit ThemeManager(settings::Store& store = settings::Store::instance(), QObject* parent = nullptr);
    ~ThemeManager() override;

    // ThemeManager tạo gần nhất còn sống (main tạo một; test tạo riêng). Chưa có thì dừng (TRION_CHECK).
    static ThemeManager& instance();

    Theme theme() const;
    void setTheme(Theme t);
    Density density() const;
    void setDensity(Density d);
    bool lite() const { return store_.get(kLite); }
    void setLite(bool on) { store_.set(kLite, on); }
    // Hệ điều hành bật giảm chuyển động (DisplayContext::systemReducedMotion của UI0-012 nối vào); không lưu.
    bool reducedMotion() const { return reducedMotion_; }
    void setReducedMotion(bool on);

    // Biến thể đang hiệu lực và tra token theo nó.
    tok::Mode mode() const { return mode_; }
    QColor color(tok::ColorId id) const { return toQColor(tok::color(id, mode_)); }
    double px(tok::PxId id) const { return tok::px(id, mode_); }
    int ms(tok::MsId id) const { return tok::ms(id, mode_); }
    tok::Shadow shadow(tok::ShadowId id) const { return tok::shadow(id, mode_); }
    // Font giao diện (--f-ui) cỡ theo token; pointSizeF giữ cỡ lẻ (--fs-s 11,5 px) nhưng Qt dựng chữ ở cỡ pixel nguyên
    // (11,5 → 12 px ở 100%); vai trò chữ và font Mono: app/theme/fonts/FontRegistry.h.
    QFont font(tok::PxId size) const;

    // Màu ngữ nghĩa 07.1 §13.2 theo chế độ hiện tại, kể cả ghi đè của người dùng.
    QColor color(Sem s) const { return semantic_.color(s, mode_); }
    double fade(Sem s) const { return semantic_.fade(s); }
    SemanticColors& semantic() { return semantic_; }

    QPalette palette() const { return makePalette(mode_); }
    QString styleSheet() const { return qss_; } // QSS từ qss/app.qss.in cho chế độ hiện tại

    static QPalette makePalette(tok::Mode m);
    static QString makeStyleSheet(tok::Mode m);

signals:
    // Đã áp chế độ mới (chủ đề, mật độ, nhẹ, giảm chuyển động) hoặc màu ngữ nghĩa đổi.
    void changed();

private:
    tok::Mode wantedMode() const;
    void apply(tok::Mode m);

    settings::Store& store_;
    SemanticColors semantic_;
    tok::Mode mode_;
    bool reducedMotion_ = false;
    bool systemLight_ = false;
    bool applied_ = false;
    QString qss_;
};

} // namespace trion::theme
