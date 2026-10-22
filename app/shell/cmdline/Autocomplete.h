#pragma once
// UIA-108 · Tự hoàn thành, gợi ý khi gõ sai, AutoCorrect của dòng lệnh (07.1 §8 Thành phần 4 và "Gõ sai tên lệnh";
// prototype js/cmd.js matchCmds, updateAC, paintAC, lev, "Ý bạn là"; css/app.css .ac .ai .af).
//
// Danh sách hiện phía trên dòng lệnh khi gõ ở dấu nhắc lệnh (CommandLine tạo, cập nhật, chuyển phím tới): lệnh, viết
// tắt, biến hệ thống (biểu tượng bánh răng, tên --c-info), lệnh LISP/plugin (setExtraCommands), từ khóa tiếng Việt
// ("cắt" → TRIM, "bo góc" → FILLET; gõ không dấu, chữ hoa cũng khớp). Mỗi dòng: biểu tượng, tên, viết tắt, mô tả ngắn
// (nhãn đã dịch của lệnh), huy hiệu Pro / Viewer Pro khi lệnh bị khóa theo phiên bản (setLockQuery).
//
// Xếp hạng = mức khớp + tần suất dùng (2 điểm mỗi lần, tối đa 30; Store kUsage, còn sau khởi động lại):
//   100 tên, viết tắt, lỗi gõ đã học (AutoCorrect) trùng chữ gõ    80 tên bắt đầu bằng chữ gõ
//    70 viết tắt bắt đầu bằng chữ gõ    65 trùng một từ khóa tiếng Việt    55 từ khóa bắt đầu bằng chữ gõ
//    50 nhãn chứa chữ gõ (giữa chuỗi, từ 2 ký tự)    40 tên chứa chữ gõ (giữa chuỗi, từ 3 ký tự)
// Biến hệ thống chỉ khớp tên, từ 2 ký tự (như prototype). Bằng điểm: dùng nhiều hơn trước, rồi lệnh theo thứ tự danh
// mục (thông dụng trước), lệnh bổ sung, biến. Hiện tối đa kMaxRows dòng.
// Dòng chọn sẵn (Enter, Space, Tab nhận): dòng trùng chữ gõ nếu có, kể cả khi dòng dùng nhiều hơn đứng trên nó (gõ
// "LE" rồi Enter vẫn là QLEADER như AutoCAD) và khi đó gửi đúng chữ đã gõ ("LE", như chạy .scr); không thì dòng đầu
// khớp từ mức 55 và gửi tên của dòng ("REC" → RECTANG, "cắt" → TRIM). Khớp giữa chuỗi chỉ gợi ý, không chọn sẵn: Enter
// gửi đúng chữ đã gõ (completion).
//
// Gõ sai: không dòng nào khớp thì danh sách là các gợi ý gần nhất (suggest), không chọn sẵn; ↓ rồi Enter, hoặc Tab,
// chọn gợi ý. Gợi ý: tên, viết tắt (và biến nếu bật tìm biến) cách chữ gõ ≤ 1 (chữ gõ ≤ 4 ký tự) hoặc ≤ 2 theo
// Damerau–Levenshtein bản OSA (sai, thiếu, thừa, đảo hai ký tự kề đều tính 1: lev của prototype tính đảo là 2), gần
// trước, rồi dùng nhiều, rồi thứ tự danh mục. CommandLine::reportUnknown in dòng "Không có lệnh “LNE”. Ý bạn là: LINE,
// LEN?" với gợi ý bấm được. Chọn cùng một gợi ý cho cùng một lỗi gõ kLearnAfter lần (bấm ở dòng lịch sử hoặc chọn
// trong danh sách) thì lỗi gõ ấy thành AutoCorrect: từ đó gõ "LNE" là LINE. Xem, xóa: corrections(), removeCorrection()
// (hộp INPUTSEARCHOPTIONS, UIA-007).
// ponytail: tần suất, AutoCorrect lưu trong Store (kUsage, kAutoCorrect), sysvar là danh sách tên tạm; INT-017
// (History, TypoSuggester có trọng số bàn phím) và INT-005/INT-107 (SysvarRegistry: mô tả, giá trị hiện tại) thay.
//
// Popup là widget con của cửa sổ chứa dòng lệnh (như .ac của prototype), không nhận focus: phím, bộ gõ tiếng Việt vẫn ở
// ô nhập. ponytail: dòng lệnh nổi trong cửa sổ riêng (UIA-007) thì popup bị cắt theo cửa sổ đó; cần thì đổi sang cửa sổ
// Qt::ToolTip. Chưa bóng --sh-pop (như popup của ComboBox, CommandSearch).
//
// Dùng (CommandLine tạo; bước integration nối):
//   cmd->autocomplete()->setLockQuery([&](const QString& c) { return RichTooltip::info(actions.action(c)).need; });
//   cmd->autocomplete()->setExtraCommands({{u"MYCMD"_s, {}}}); // ponytail: LISP (defun c:…), plugin nối khi có

#include "core/base/settings/Settings.h"

#include <QJsonObject>
#include <QList>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <cstdint>
#include <functional>
#include <vector>

class QAbstractItemView;
class QLabel;
class QListView;

namespace trion::ui {

enum class Edition : std::uint8_t; // app/widgets/tooltip/RichTooltip.h

// Tùy chọn của hộp INPUTSEARCHOPTIONS (UIA-007); đổi là áp ngay cho danh sách đang mở.
inline const settings::Setting<bool> kSearchSysvars{"cmdline/searchSysvars", true};
inline const settings::Setting<bool> kSearchVietnamese{"cmdline/searchVietnamese", true};
// Số lần dùng theo tên lệnh, biến: {"LENGTHEN": 20}.
inline const settings::Setting<QJsonObject> kUsage{"cmdline/usage", {}};
// Số lần chọn gợi ý theo lỗi gõ (viết hoa): {"LNE": {"LINE": 3}}; ≥ kLearnAfter là AutoCorrect.
inline const settings::Setting<QJsonObject> kAutoCorrect{"cmdline/autoCorrect", {}};

class Autocomplete : public QWidget {
    Q_OBJECT
public:
    static constexpr int kMaxRows =
        9; // vừa max-height 300 px của .ac, không phải cuộn (prototype: 9 lệnh + 3 biến, cuộn)
    static constexpr int kMaxSuggestions = 3; // "Ý bạn là" (INT-017: tối đa 3 gợi ý)
    static constexpr int kLearnAfter = 3;     // 07.1 §8: chọn cùng gợi ý 3 lần

    struct Extra { // lệnh LISP / plugin
        QString name;
        QString description{}; // đã dịch; rỗng: "Lệnh LISP hoặc plugin"
    };
    struct Correction {
        QString typo, command;
    };

    // anchor: dòng lệnh. Popup nằm trong anchor->window(), mép dưới cách mép trên anchor --sp-1. Cần ThemeManager,
    // IconProvider, settings::Store::instance().
    explicit Autocomplete(QWidget* anchor);
    ~Autocomplete() override;

    // ponytail: chỗ nối lệnh LISP (defun c:…), plugin (LSP-…, PLG-…); thay danh sách cũ. Tên trùng lệnh thì bỏ.
    void setExtraCommands(const QList<Extra>& commands);
    // Huy hiệu khóa của lệnh (Edition::None: dùng được). Chưa đặt: không có huy hiệu.
    void setLockQuery(std::function<Edition(const QString& command)> query);

    QStringList search(const QString& typed) const; // tên các dòng sẽ hiện, theo thứ tự (cho test, QA)
    QString completion(const QString& typed) const; // chữ Enter/Space gửi khi chưa chọn dòng; rỗng: gửi chữ đã gõ
    QStringList suggest(const QString& typed) const; // "Ý bạn là": tên hoặc viết tắt gần nhất, ≤ kMaxSuggestions
    QString resolve(const QString& name) const; // tên lệnh/biến của tên, viết tắt; rỗng nếu không có
    QString correction(const QString& typo) const; // lệnh AutoCorrect của lỗi gõ đã học; rỗng nếu chưa
    int nameCount() const; // số tên trong chỉ mục: lệnh, viết tắt, biến, lệnh bổ sung

    void recordUse(const QString& name); // một lần dùng (tên hoặc viết tắt; tên lạ bỏ qua)
    // Một lần chọn command cho lỗi gõ typo; đủ kLearnAfter lần thì phát learned.
    void recordPick(const QString& typo, const QString& command);
    static QList<Correction> corrections(); // AutoCorrect đã học, theo lỗi gõ
    static void removeCorrection(const QString& typo);

    // Dòng lệnh gọi: chữ đang gõ ở dấu nhắc lệnh → hiện, cập nhật hoặc ẩn danh sách (vẫn theo chữ gõ: đổi thiết lập,
    // chủ đề, ngôn ngữ thì tính lại); dismiss: đóng và thôi theo (Esc, gửi, có prompt, mất focus).
    void refresh(const QString& typed);
    void dismiss();
    void step(int delta); // ↑ −1, ↓ +1, vòng; chưa chọn dòng nào thì ↓ là dòng đầu, ↑ dòng cuối
    bool accept(bool orFirst); // nhận dòng đang chọn (orFirst: chưa chọn thì dòng đầu); false: không có dòng
    QAbstractItemView* view() const;

signals:
    void accepted(const QString& name); // dòng được nhận (phím, chuột): dòng lệnh gửi như gõ tên rồi Enter
    void learned(const QString& typo, const QString& command); // vừa thêm AutoCorrect

protected:
    void paintEvent(QPaintEvent* e) override;
    void changeEvent(QEvent* e) override;

private:
    struct Entry;
    struct Query;
    class Model;
    class Delegate;

    struct Ranked {
        std::vector<int> rows; // vị trí trong chỉ mục, theo thứ tự hiện
        int current = -1;      // dòng chọn sẵn
        int exact = -1;        // dòng trùng chữ gõ
        bool typo = false;     // rows là gợi ý gõ sai
    };

    const std::vector<Entry>& index() const; // dựng khi cần
    Ranked rank(const QString& typed) const;
    std::vector<int> nearest(const Query& q, QStringList* terms) const;
    void place();
    void select(int row);
    void retranslate();

    QPointer<QWidget> anchor_;
    std::function<Edition(const QString&)> lock_;
    QList<Extra> extras_;
    mutable std::vector<Entry> index_;
    std::vector<int> rows_;
    int current_ = -1, exact_ = -1;
    QString typed_, typo_;                  // chữ đang gõ; typo_: chữ gõ khi danh sách là gợi ý gõ sai
    QString said_;                          // dòng vừa báo cho trình đọc màn hình
    bool live_ = false;                     // đang theo chữ gõ (refresh tới dismiss)
    int nameW_ = 0, aliasW_ = 0, rowW_ = 0; // cột tên, viết tắt, bề rộng cần của dòng (theo các dòng đang hiện)
    Model* model_;
    QListView* list_;
    QLabel* footer_;
};

} // namespace trion::ui
