#pragma once
// UI0-009 · Thông điệp (07.1 §12, §9.14; màn hình UI-A14). Prototype: TC.toast, TC.ctip, TC.notes trong js/ui.js,
// A.banner/renderBanners trong js/shell.js; css/app.css .toast .banner .ctip .empty .fmsg.
//
//   lớp                dùng khi                             hành vi
//   CanvasBanner       trạng thái bản vẽ người dùng cần biết  mép trên vùng vẽ, ở lại đến khi đóng, có hành động;
//                                                           tối đa 2 cùng lúc, banner cuối đếm số còn chờ
//   ToastHost, Toast   tác vụ nền xong hoặc lỗi               góc dưới phải, trên thanh trạng thái; tự ẩn sau 5 s (rê
//                                                           chuột hay focus vào toast thì dừng đếm), lỗi ở lại;
//                                                           tối đa 3 cùng lúc (toast mới đẩy toast cũ nhất ra);
//                                                           mọi toast có mục trong NotificationCenter
//   NotificationCenter danh sách thông báo của phiên         model: mới nhất ở hàng 0, đếm chưa đọc
//   CursorTip          lỗi tức thời trong lệnh                cửa sổ riêng lệch (16, 18) px khỏi điểm, tự ẩn sau 2 s
//   EmptyState         danh sách, palette chưa có gì          biểu tượng, câu giải thích, nút hành động gợi ý
//   FieldMessage       giá trị nhập không hợp lệ              dòng dưới ô nhập; lỗi có biểu tượng và được đọc
//
// Chung cho mọi lớp:
//  - Mỗi mức (Severity) có biểu tượng riêng, không chỉ khác màu. Màu, cỡ đọc token lúc vẽ.
//  - Không lấy focus: hiện lên không đổi focus, không kích hoạt cửa sổ. Nút trên toast, banner nhận focus bằng Tab,
//    không nhận bằng cú nhấp (vùng vẽ, dòng lệnh giữ phím gõ); đóng khi đang giữ focus thì focus về chỗ cũ. Esc đóng
//    toast đang có focus.
//  - Hiện ngay trong lượt gọi: khung kế tiếp đã vẽ. Rảnh không timer: timer 5 s, 2 s chỉ chạy khi có toast, chú thích
//    đang hiện (cả hai PreciseTimer: không ẩn sớm).
//  - Trình đọc màn hình: toast lỗi có vai trò AlertMessage, phát QAccessible::Alert và thông báo Assertive; toast khác,
//    banner, chú thích có vai trò StatusBar (role="status" của prototype) và thông báo Polite; FieldMessage báo lỗi mới
//    như aria-live="polite".
//  - Chữ do bên gọi dịch sẵn (tr()), chỉ là chữ thường (không HTML). Luồng UI.
//
// Dùng (main, sau ThemeManager, IconProvider):
//   NotificationCenter notes;                       // một cho cả phiên; UIL-001 (UI-L14) là popup đọc model này
//   ToastHost toasts(&mainWindow, notes);
//   toasts.followJobs(presenter);                   // UI0-112: tác vụ nền lỗi → toast lỗi, xong sau ≥ 2 s → toast
//   toasts.post(Severity::Ok, tr("Đã xuất 24 tờ PDF"), {tr("Mở thư mục"), [dir] { openFolder(dir); }});
//   auto* banners = new CanvasBanner(canvas);       // con của vùng vẽ, phủ mép trên, rộng theo vùng vẽ
//   banners->add(u"readonly"_s, Severity::Warning, tr("Bản vẽ chỉ đọc"), {{tr("Lưu thành…"), saveAs}});
//   auto* tip = new CursorTip(canvas);              // cửa sổ riêng: hiện/ẩn không làm vùng vẽ vẽ lại
//   tip->showAt(cursor, tr("Không có giao điểm với đường biên"));
//   auto* empty = new EmptyState(u"insert"_s);      // trong retranslate() của palette:
//   empty->setText(tr("Bản vẽ chưa có block nào"), tr("…")); empty->setAction(tr("Mở Thư viện TCVN"));
//   auto* why = new FieldMessage(scaleEdit);        // dưới ô; why->setError(tr("Tỉ lệ phải lớn hơn 0."))
//
// ponytail: chưa nối sự kiện ứng dụng: UIA-015 (EventRouter, event_map.json) gọi các API này, giữ bộ banner theo
// document và thay chữ chung của followJobs bằng câu riêng theo loại việc. Chưa có bóng --sh-pop cho toast và chú thích
// (như Menu, RichTooltip: viền --c-border-ctl tách khỏi nền). Nút đóng .ib.sm (20 px) dùng Button Icon cao --h-ctl
// (Controls chưa có cỡ nhỏ cho nút biểu tượng).

#include "app/theme/generated/Tokens.h"
#include "app/widgets/controls/Controls.h"

#include <QAbstractListModel>
#include <QDateTime>
#include <QLabel>
#include <QList>
#include <QPointer>
#include <QTimer>
#include <QWidget>

#include <chrono>
#include <cstdint>
#include <functional>
#include <vector>

class QHBoxLayout;
class QVBoxLayout;

namespace trion::ui {

class BannerRow;
class ProgressPresenter;

// Mức thông điệp: .toast ok / info / warn / err. Biểu tượng okc / info / warn / error (iconName), màu --c-ok /
// --c-info / --c-warn / --c-danger.
enum class Severity : std::uint8_t { Ok, Info, Warning, Error };
QString iconName(Severity s);

// Biểu tượng một màu của bộ (IconProvider) cỡ cố định, tô bằng token lúc vẽ (.ic, màu currentColor của prototype).
// Cỡ là số px logic không đổi theo chế độ (--ic, .ic.s12, .empty .ic). canvas: màu theo vùng vẽ (giá trị :root của
// token, không đổi theo chủ đề giao diện, như màu ngữ nghĩa dyn.tooltip.*).
class Glyph final : public QWidget {
    Q_OBJECT
public:
    Glyph(int size, QWidget* parent);
    void set(const QString& name, tok::ColorId ink, bool canvas = false);
    QString name() const { return name_; }

protected:
    void paintEvent(QPaintEvent* e) override;

private:
    QString name_;
    tok::ColorId ink_;
    int size_;
    bool canvas_ = false;
};

// Hành động trên toast, banner: nút .btn.sm, bấm thì gọi run (toast đóng sau đó, banner ở lại).
struct MessageAction {
    QString label; // đã dịch, nêu việc sẽ làm ("Mở thư mục"), không "OK"
    std::function<void()> run;
    Button::Kind kind = Button::Kind::Normal; // banner: Primary (.btn.primary), Ghost; toast luôn Normal
};

// Trung tâm thông báo (prototype TC.notes): thông báo của phiên, mới nhất ở hàng 0. Chuông trên thanh trạng thái
// (UIA-009) đọc unreadCount(); popup UI-L14 (UIL-001) là view của model này. Chỉ luồng UI.
// ponytail: giữ trong bộ nhớ (mất khi thoát), tối đa kMaxEntries mục, bỏ mục cũ nhất; UIL-001 lưu qua phiên. Chữ dịch
// lúc đăng, đổi ngôn ngữ không dịch lại mục cũ.
class NotificationCenter final : public QAbstractListModel {
    Q_OBJECT
public:
    using Id = quint64; // tăng dần từ 1, không dùng lại
    static constexpr int kMaxEntries = 500;
    enum Role {
        IdRole = Qt::UserRole + 1, // Id
        SeverityRole,              // int(Severity)
        KindRole,                  // QString: "task", "update", "license", "tip"… (nhóm lọc của popup)
        TimeRole,                  // QDateTime lúc đăng, giờ máy
        ReadRole,                  // bool
    };
    // Qt::DisplayRole: chữ thông báo.

    explicit NotificationCenter(QObject* parent = nullptr);

    Id add(const QString& text, Severity severity, const QString& kind = QStringLiteral("task"));
    void markRead(Id id); // id không còn: bỏ qua
    void markAllRead();
    void remove(Id id);
    int unreadCount() const { return unread_; }

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

signals:
    void unreadChanged(int count);

private:
    struct Entry {
        Id id;
        QString text;
        Severity severity;
        QString kind;
        QDateTime time;
        bool read = false;
    };
    int row(Id id) const;
    void setUnread(int count);

    QList<Entry> entries_; // mới nhất trước
    Id next_ = 1;
    int unread_ = 0;
};

// Một toast (.toast): [biểu tượng mức] chữ xuống dòng, rộng tối đa 420 px [hành động] [×]. ToastHost tạo, đặt chỗ và
// hủy; nơi khác chỉ đọc. Sống đến khi đóng (hủy bằng deleteLater): giữ bằng QPointer.
class Toast final : public QWidget {
    Q_OBJECT
public:
    Severity severity() const { return severity_; }
    QString text() const;
    NotificationCenter::Id entry() const { return entry_; } // mục của toast trong NotificationCenter
    bool counting() const { return timer_.isActive(); } // đang đếm giờ tự ẩn (không lỗi, không rê, không focus)
    Button* actionButton() const { return action_; } // nullptr nếu không có hành động
    Button* closeButton() const { return close_; }

signals:
    void dismissed(bool byUser); // hết giờ (false); bấm × hoặc hành động (true)

protected:
    bool event(QEvent* e) override; // Enter, Leave: dừng, đếm tiếp; Esc: đóng; LanguageChange
    void paintEvent(QPaintEvent* e) override;

private:
    friend class ToastHost;
    Toast(Severity severity, const QString& text, MessageAction action, QWidget* window);
    void start(std::chrono::milliseconds timeout);
    void hold(); // rê chuột hoặc focus trong toast: dừng; ra: đếm tiếp phần còn lại
    QSize fit(int maxWidth) const;

    Severity severity_;
    NotificationCenter::Id entry_ = 0;
    Glyph* glyph_;
    QLabel* text_;
    Button* action_ = nullptr;
    Button* close_;
    QTimer timer_;
    int left_ = -1;          // ms còn lại khi đang dừng; -1: không dừng
    QPointer<QWidget> back_; // focus trước khi Tab vào toast: trả lại khi đóng
    bool hovered_ = false;
};

// Chồng toast ở góc dưới phải của window (.toasts: cách mép phải --sp-5, cách đáy --h-status + --sp-5, cách nhau
// --sp-4), toast mới nhất ở dưới cùng. Toast là widget con của window, phủ trên mọi widget khác.
class ToastHost final : public QObject {
    Q_OBJECT
public:
    static constexpr int kMaxVisible = 3; // [đề xuất] UI0-009 tiêu chí 1

    ToastHost(QWidget* window, NotificationCenter& center, QObject* parent = nullptr);
    ~ToastHost() override;

    // ToastHost tạo gần nhất còn sống (main tạo một; test tạo riêng); nullptr nếu chưa có. Cho widget sâu trong hộp
    // thoại cần nhắc nhanh (UI0-007).
    static ToastHost* instance();

    NotificationCenter& center() const { return center_; }

    // Thêm mục vào center rồi hiện toast; đã đủ kMaxVisible thì toast cũ nhất đóng (mục của nó vẫn chưa đọc). Lỗi ở
    // lại đến khi đóng; mức khác tự ẩn sau timeout(). Bấm × hoặc hành động: đóng và đánh dấu đã đọc.
    Toast* post(Severity severity, const QString& text, MessageAction action = {},
                const QString& kind = QStringLiteral("task"));

    // Tác vụ nền kết thúc (ProgressPresenter::finished): lỗi → toast lỗi nêu lý do; xong sau khi đã hiện tiến trình
    // (≥ 2 s) → toast; hủy hoặc xong nhanh → không. presenter sống lâu hơn ToastHost hoặc tự ngắt khi hủy.
    void followJobs(ProgressPresenter& presenter);

    QList<Toast*> toasts() const { return toasts_; } // đang hiện, cũ nhất trước

    std::chrono::milliseconds timeout() const { return timeout_; }
    void setTimeout(std::chrono::milliseconds timeout) { timeout_ = timeout; } // 5 s; test rút ngắn

protected:
    bool eventFilter(QObject* watched, QEvent* e) override; // window đổi cỡ: đặt lại chỗ

private:
    void dismiss(Toast* toast, bool byUser);
    void place();

    QPointer<QWidget> window_;
    NotificationCenter& center_;
    QList<Toast*> toasts_;
    std::chrono::milliseconds timeout_{5000};
};

// Banner trên vùng vẽ (.cv-banners, .banner): widget con của canvas, phủ mép trên, rộng bằng canvas; ẩn khi không có
// banner nào. Mỗi banner: [biểu tượng] chữ một dòng (hẹp thì "…") [hành động…] [×]. Hiện tối đa kMaxVisible theo thứ
// tự thêm; banner cuối đang hiện ghi "+n thông báo khác" khi còn banner chờ; đóng một cái thì cái chờ đầu tiên hiện.
// Không timer: ở lại đến khi người dùng bấm × hoặc bên gọi remove().
class CanvasBanner final : public QWidget {
    Q_OBJECT
public:
    static constexpr int kMaxVisible = 2; // UI0-009 tiêu chí 3

    explicit CanvasBanner(QWidget* canvas);

    // Thêm, hoặc thay banner cùng id tại chỗ cũ. icon rỗng: biểu tượng của mức (iconName); Info nền --c-banner, biểu
    // tượng --c-accent; Warning, Error nền pha 14% --c-warn, --c-danger như prototype.
    void add(const QString& id, Severity severity, const QString& text, const QList<MessageAction>& actions = {},
             const QString& icon = {});
    void remove(const QString& id); // bên gọi gỡ (trạng thái đã hết): không phát closed()
    bool contains(const QString& id) const;
    QStringList ids() const;               // theo thứ tự hiện
    int visibleCount() const;              // ≤ kMaxVisible
    QWidget* row(const QString& id) const; // widget của banner (test, QA); nullptr nếu không có

signals:
    void closed(const QString& id); // người dùng bấm ×

protected:
    bool eventFilter(QObject* watched, QEvent* e) override; // canvas đổi cỡ

private:
    void drop(BannerRow* row);
    void refresh();

    QVBoxLayout* rows_;
    std::vector<BannerRow*> list_; // thứ tự thêm
};

// Chú thích cạnh con trỏ (.ctip): cửa sổ ToolTip riêng không nhận chuột, không kích hoạt, nên hiện/ẩn không làm vùng
// vẽ dựng lại cảnh. Đặt lệch kOffset từ điểm (sang trái, lên trên nếu tràn mép canvas: vẫn cách ≥ 16 px). Màu theo
// vùng vẽ, không theo chủ đề giao diện (07.1 §13.2): nền, chữ, viền thường là dyn.tooltip.*; lỗi viền --c-danger;
// thêm biểu tượng mức 12 px (prototype chỉ đổi màu viền). Gọi lại khi đang hiện: thay chữ, đặt lại chỗ, đếm lại.
// ponytail: màu cho nền vùng vẽ tối (mặc định); nền vùng vẽ sáng thêm cùng UI-D10 ("Màu…" theo ngữ cảnh).
class CursorTip final : public QWidget {
    Q_OBJECT
public:
    static constexpr QPoint kOffset{16, 18}; // prototype TC.ctip: left x + 16, top y + 18

    explicit CursorTip(QWidget* canvas); // sống cùng canvas (là cha)

    // pos: tọa độ trong canvas của điểm đang chỉ (điểm nóng con trỏ).
    void showAt(const QPoint& pos, const QString& text, Severity severity = Severity::Error);
    QString text() const;
    Severity severity() const { return severity_; }
    bool counting() const { return timer_.isActive(); }

    std::chrono::milliseconds timeout() const { return timeout_; }
    void setTimeout(std::chrono::milliseconds timeout) { timeout_ = timeout; } // 2 s; test rút ngắn

protected:
    void paintEvent(QPaintEvent* e) override;

private:
    QPointer<QWidget> canvas_;
    Glyph* glyph_;
    QLabel* text_;
    QTimer timer_;
    Severity severity_ = Severity::Error;
    std::chrono::milliseconds timeout_{2000};
};

// Trạng thái rỗng (.empty): biểu tượng 28 px --c-text-3, tiêu đề đậm, câu giải thích --c-text-2, nút .btn.sm gợi ý
// (07.1 §12: "Bản vẽ chưa có block nào — [Mở Thư viện TCVN]"), thêm nút cùng hàng bằng addAction (.empty .hrow: trang
// Start "[Mở bản vẽ mẫu] [▶ Xem video 2 phút]"). Chữ do chủ widget đặt trong retranslate().
// ponytail: hàng nút không xuống dòng như .hrow (flex-wrap): chỗ hẹp hơn cả hàng thì widget đòi rộng tối thiểu bằng
// hàng; làm khi palette hẹp cần hai nút.
class EmptyState final : public QWidget {
    Q_OBJECT
public:
    explicit EmptyState(const QString& icon = QStringLiteral("info"), QWidget* parent = nullptr);

    void setIconName(const QString& icon);
    void setText(const QString& title, const QString& explanation = {});
    void setAction(const QString& label); // rỗng: không có nút
    Button* actionButton() const { return action_; }
    // Nút nữa cùng hàng, sau các nút đã có (cách --sp-4; cỡ nhỏ, kiểu Normal; cả hàng canh giữa): chủ nối clicked, đặt
    // chữ trong retranslate(), biểu tượng (setIconName), kiểu (setKind).
    Button* addAction(const QString& label = {});

signals:
    void actionTriggered();

private:
    Glyph* glyph_;
    QLabel* title_;
    QLabel* explanation_;
    Button* action_;
    QHBoxLayout* actions_; // hàng nút
};

// Dòng dưới ô nhập (.fmsg, cỡ --fs-xs, cao tối thiểu 14 px để form không nhảy): lỗi = biểu tượng warn + câu --c-danger
// (07.1 §12: "Tỉ lệ phải lớn hơn 0"), gợi ý = câu --c-text-3. Câu xuống dòng khi dài. Trình đọc màn hình: là nhãn
// (QLabel), câu thành mô tả tiếp cận của field, lỗi mới phát thông báo Polite từ field. Viền lỗi của ô do ô tự vẽ
// (setError của Controls, NumericField).
class FieldMessage final : public QLabel {
    Q_OBJECT
public:
    explicit FieldMessage(QWidget* field, QWidget* parent = nullptr); // field: ô được mô tả; có thể nullptr

    void setError(const QString& text); // rỗng: xóa dòng
    void setHint(const QString& text);
    bool isError() const { return error_; }

protected:
    void paintEvent(QPaintEvent* e) override;

private:
    void set(const QString& text, bool error);

    QPointer<QWidget> field_;
    bool error_ = false;
};

} // namespace trion::ui
