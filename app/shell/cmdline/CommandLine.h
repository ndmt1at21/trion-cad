#pragma once
// UIA-107 · Dòng lệnh UI-A06 (07.1 §8; design/README §Quy ước "Prompt"; prototype js/cmd.js CL.mount, renderPrompt,
// CL.enter, browse, CL.submit; css/app.css .cmd .cmd-hist .cmd-in .chip). Trên: lịch sử (0–10 dòng, mặc định 3; thông
// báo của lệnh cũng vào đây; 100.000 dòng trong bộ nhớ, chỉ vẽ dòng đang thấy, lăn chuột để xem dòng cũ). Dưới: khung
// .cmd-in gồm dấu nhắc (">>" khi lệnh trong suốt, mũi tên), tên lệnh in đậm, lời nhắc, chip tùy chọn bấm được ("Bán
// kính · R", chữ tắt gạch chân), giá trị mặc định <10.0000>, rồi ô nhập CommandInput (UIA-106).
//
// Phím trong ô nhập
//   Enter, Space  gửi chữ đã gõ (submitted). Space là chữ khi prompt cho phép dấu cách (allowSpace) và khi đang gõ biểu
//                 thức LISP "(" ở dấu nhắc lệnh. Không có prompt mà ô trống: lặp lệnh vừa dùng.
//   Esc           xóa ô; có prompt: cancelRequested; không prompt: deselectRequested (Esc lần hai bỏ vùng chọn).
//   ↑ / ↓         không prompt: lệnh gần đây; có prompt: giá trị đã nhập gần đây.
//   Chuột phải, phím Menu: Lệnh gần đây, Giá trị đã nhập gần đây, Sao chép lịch sử, Tùy chọn dòng lệnh.
// Gõ "," ngay sau chữ số (trừ prompt chuỗi): một lần mỗi phiên, lịch sử nhắc dấu thập phân luôn là "." (07.1 §8).
// Ô nhập bỏ qua bộ gõ khi chờ tên lệnh (CommandInput::setAwaitingCommand): bật khi không có prompt, tắt khi có prompt
// và khi chữ đang gõ bắt đầu bằng "(" (LISP: chuỗi trong biểu thức gõ được tiếng Việt).
//
// Token (submitted): chữ đã gõ, bỏ khoảng trắng hai đầu (trừ prompt chuỗi); khớp một từ khóa của prompt (chữ tắt,
// "_" + chữ tắt, tên toàn cục, không phân biệt hoa/thường, cả từ khóa ẩn) thì là chữ tắt của từ khóa, đúng như bấm
// chip (resolve). Gõ phím và chạy cùng chuỗi bằng .scr cho cùng dãy token: Space/Enter tách như dòng script.
//
// Lệnh COMMANDLINE / COMMANDLINEHIDE (Ctrl+9, Keymap "cmdl"; docs/specs/commands/COMMANDLINE.md): ẩn/hiện widget này
// là việc của cửa sổ chính (bước integration), không ở đây.
//
// Dùng (luồng UI, sau ThemeManager, IconProvider, settings::Store; RichTooltip nếu có thì menu có tooltip):
//   auto* cmd = new trion::ui::CommandLine;
//   mainWindow.setRegion(Region::CommandLine, cmd);           // UIA-001 đặt setProperty("compact", …) ở màn hẹp
//   QObject::connect(cmd, &CommandLine::submitted, router, …);  // ponytail: INT-002 nhận đầu vào
//   QObject::connect(cmd, &CommandLine::commandRequested, router, …); // macro "^C^CLINE" như ActionRegistry::setSink
//   cmd->setPrompt({.command = u"CIRCLE"_s, .text = tr("Chỉ định bán kính hoặc"),
//                   .keywords = {{tr("Đường kính"), u"D"_s, u"_Diameter"_s}}, .defaultValue = u"10.0000"_s});
//   cmd->appendHistory(tr("Đã chọn %n đối tượng", nullptr, n));
//   cmd->clearPrompt();                                          // lệnh xong: về dấu nhắc lệnh

#include "core/base/settings/Settings.h"

#include <QList>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <cstdint>
#include <optional>

class QHBoxLayout;
class QKeyEvent;

namespace trion::ui {

class CommandInput;

// Số dòng lịch sử hiện (CLIPROMPTLINES; 0–10, mặc định 3). Hộp INPUTSEARCHOPTIONS (UIA-007) ghi; đổi là áp ngay.
inline const settings::Setting<int> kHistoryLines{"cmdline/historyLines", 3};

class CommandLine : public QWidget {
    Q_OBJECT
    // Màn hẹp (07.1 §3.2, 1366–1600 px): lịch sử còn 1 dòng. MainWindow (UIA-001) đặt qua setProperty("compact", …).
    Q_PROPERTY(bool compact READ isCompact WRITE setCompact)

public:
    // ponytail: dữ liệu prompt do lệnh đưa qua setPrompt; INT-003 (PromptModel) thay.
    struct Keyword {
        QString label{};     // nhãn đã dịch, hiện trên chip ("Bán kính")
        QString key{};       // chữ tắt kiểu AutoCAD, gạch chân trên chip; là token khi bấm chip hoặc gõ ("R")
        QString global{};    // tên toàn cục ("_Radius"), gõ "_Radius" hay "Radius" cũng ra key; rỗng nếu không có
        bool hidden = false; // từ khóa ẩn (MO, RO, SC, MI của grip): không có chip, gõ vẫn ra key
    };
    struct Prompt {
        QString command{};       // tên lệnh in đậm trước lời nhắc ("LINE"); rỗng: không hiện
        QString text{};          // lời nhắc đã dịch ("Chỉ định điểm đầu hoặc")
        QList<Keyword> keywords{}; // theo thứ tự chip
        QString defaultValue{}; // đã định dạng ("10.0000") → hiện <10.0000>; rỗng: không có
        bool allowSpace = false; // prompt chuỗi: Space là chữ, chỉ Enter gửi
        bool nested = false;     // prompt của lệnh trong suốt ('ZOOM giữa LINE): dấu ">>"
    };
    enum class Kind : std::uint8_t {
        Echo,    // lời nhắc kèm giá trị đã nhập (dòng lệnh tự ghi khi gửi)
        Message, // thông báo của lệnh ("Đã chọn 125 đối tượng")
        Error,   // lỗi: chữ --c-danger kèm biểu tượng (không chỉ bằng màu)
    };

    explicit CommandLine(QWidget* parent = nullptr);
    ~CommandLine() override;

    // Lệnh hỏi: prompt mới hiện ngay ở khung kế tiếp (một lần vẽ, không bố trí lại lần hai).
    void setPrompt(const Prompt& prompt);
    // Về dấu nhắc lệnh (không có lệnh chạy).
    void clearPrompt();
    bool hasPrompt() const { return prompt_.has_value(); }

    // Thêm một dòng vào lịch sử; dòng mới luôn hiện (cuộn về cuối). Thông báo, lỗi được báo cho trình đọc màn hình.
    void appendHistory(const QString& text, Kind kind = Kind::Message);
    int historyCount() const;
    QString historyLine(int index) const;

    bool isCompact() const { return compact_; }
    void setCompact(bool compact);

    CommandInput* input() const { return input_; }
    // Ô chip của chữ tắt key, tọa độ của widget này; rỗng nếu prompt không có chip đó (từ khóa ẩn, không có prompt).
    QRect keywordRect(const QString& key) const;

    // Token của chữ đã gõ ở prompt: chữ tắt của từ khóa khớp, không thì giữ nguyên. INT-002 dùng lại cho .scr.
    // ponytail: chỉ khớp nguyên chữ tắt/tên toàn cục; tiền tố kiểu AutoCAD ("Dia" của Diameter), nhãn tiếng Việt: INT-003.
    static QString resolve(const Prompt& prompt, const QString& text);

signals:
    void submitted(const QString& token); // ponytail: INT-002 nhận
    void cancelRequested();               // Esc khi có prompt
    void deselectRequested();             // Esc khi không có prompt: bỏ vùng chọn
    void commandRequested(const QString& macro); // mục menu: "^C^CLINE", "^C^C_INPUTSEARCHOPTIONS"

protected:
    bool eventFilter(QObject* watched, QEvent* e) override;
    void paintEvent(QPaintEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;
    void changeEvent(QEvent* e) override;

private:
    class HistoryView;
    class PromptView;

    bool ownsKey(const QKeyEvent* e) const; // Enter, Space (trừ khi là chữ), Esc, ↑, ↓
    bool spaceIsText() const;
    void submit();
    void submitToken(const QString& typed);
    void escape();
    void browse(int step);
    void promptChanged();
    void textChanged(const QString& text);
    void updateAwaiting();
    void relayout();
    void retranslate();
    QString placeholder() const;
    QRect frameRect() const;
    QString historyText() const;

    std::optional<Prompt> prompt_;
    HistoryView* history_ = nullptr;
    PromptView* promptView_ = nullptr;
    CommandInput* input_ = nullptr;
    QHBoxLayout* frame_ = nullptr; // .cmd-in
    // ponytail: lệnh và giá trị gần đây chỉ trong phiên; INT-017 History (lưu theo profile, theo loại prompt) thay.
    QStringList recent_, entered_;
    QString typedCommand_; // tên vừa gõ ở dấu nhắc lệnh, chờ prompt đầu của lệnh để đổi bí danh thành tên lệnh
    int browse_ = -1;
    bool compact_ = false;
    bool commaHinted_ = false;
    bool composing_ = false; // bộ gõ đang có chữ soạn dở (preedit) trong ô nhập
};

} // namespace trion::ui
