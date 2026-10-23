#pragma once
// UIF-001 · Bộ so khớp của Tìm chữ (lệnh FIND; 07.2 UI-F09; 03 §7): hàm thuần, không chạm widget, chạy ở luồng nền.
//
// Nguồn chữ là chỗ nối: TextList, mỗi chữ tìm được của bản vẽ là một mục {id, loại, layout, chữ, mã cũ…}. Snapshot bất
// biến (shared_ptr<const TextList>) do bên gọi dựng, đọc được từ nhiều luồng cùng lúc.
// ponytail: tạm thay API tìm của lệnh FIND (CMD:FIND duyệt bản vẽ qua snapshot DOC-009: TEXT, MTEXT đã bỏ mã định dạng,
// ATTRIB, chữ của dim và multileader, ô bảng, hyperlink; chữ mã cũ giải bằng VN-001). Khi có API đó, TextList và
// FindMatcher thành lớp mỏng gọi nó; FindDialog giữ nguyên.
//
// So khớp: khóa so sánh dựng theo cùng một cách cho chuỗi tìm và cho chữ của bản vẽ.
//  - Phân biệt dấu tiếng Việt TẮT: bỏ dấu (tách NFD, bỏ dấu kết hợp; đ/Đ → d/D): "duong" khớp "đường", "ĐƯỜNG".
//    BẬT: so theo dạng NFD, dấu theo thứ tự chuẩn, nên chữ dựng sẵn (NFC) và chữ tổ hợp (NFD, bộ gõ "Unicode tổ hợp")
//    khớp nhau; đoạn khớp không dừng giữa một chữ ("ba" không khớp "bả", "ă" không khớp "ắ").
//  - Phân biệt hoa thường TẮT: case folding đơn giản của Unicode ("đường" khớp "ĐƯỜNG").
//  - Nguyên từ: ngay trước và sau đoạn khớp không phải chữ cái, chữ số, dấu kết hợp hay "_" ("12" khớp "KT-12", không
//    khớp "120").
//  - Wildcard: * là chuỗi bất kỳ (kể cả rỗng, tham lam), ? là đúng một chữ (cùng dấu của nó); tắt thì * ? là chữ
//  thường.
//  - Đoạn khớp: lần khớp đầu tiên, theo vị trí UTF-16 trong chữ gốc (để tô), phủ cả dấu kết hợp đứng sau.
// Lọc (accepts): phạm vi (toàn bản vẽ, layout hiện tại, tập chọn theo id), nhóm loại chữ, trong block (gồm attribute),
// trong xref, đối tượng ẩn. Chữ mã cũ (TCVN3, VNI): bật "Tìm cả chữ mã" thì so và hiện chữ đã giải mã (`decoded`, gõ
// Unicode vẫn ra), tắt thì so chữ gốc như đang lưu.
//
// Hiệu năng: mỗi mục dựng khóa vào bộ đệm của matcher (không cấp phát khi đã đủ chỗ) rồi chạy một QRegularExpression
// (JIT) trên khóa. Một FindMatcher chỉ dùng trên một luồng (bộ đệm chung): việc nền nào tạo matcher riêng của nó.
// ponytail: bảng tách dấu chỉ có chữ Latin (U+00C0–U+024F, U+1E00–U+1EFF), đủ cho tiếng Việt; chữ Hy Lạp, Kirin có
// dấu so nguyên dạng (NT-012 thay bằng chuẩn hóa đầy đủ).
//
// Dùng (luồng nền):
//   const FindMatcher m(u"duong"_s, options);
//   std::vector<Hit> hits;
//   m.scan(*texts, 0, int(texts->size()), hits, token);   // hits[i].item: chỉ số trong texts, theo thứ tự nguồn

#include "core/base/sched/Scheduler.h"

#include <QRegularExpression>
#include <QString>
#include <QStringView>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace trion::find {

// Loại chữ (cột Loại của bảng kết quả).
enum class TextKind : std::uint8_t {
    Text,      // TEXT
    MText,     // MTEXT (chữ đã bỏ mã định dạng; xuống dòng là '\n')
    Attribute, // giá trị attribute của block reference; detail = tag
    Dimension, // chữ của kích thước: chữ ghi đè hoặc số đo đã định dạng
    Leader,    // chữ của multileader
    Table,     // ô bảng; detail = địa chỉ ô ("B3")
    Hyperlink, // chữ hiện của hyperlink gắn với đối tượng
};

// Bảng mã của chữ mã cũ (font .VnTime → TCVN3, VNI-Times → VNI; VN-002 nhận dạng).
enum class Legacy : std::uint8_t { None, Tcvn3, Vni };

struct TextItem {
    // Đối tượng chọn được, zoom được (handle): INSERT của attribute và của chữ trong block, bảng của ô, xref của chữ
    // trong xref.
    std::uint64_t id = 0;
    TextKind kind = TextKind::Text;
    Legacy legacy = Legacy::None;
    bool inBlock = false; // attribute, hoặc chữ trong định nghĩa block (qua INSERT)
    bool inXref = false;  // chữ trong bản vẽ tham chiếu ngoài
    bool hidden = false;  // không hiện: layer tắt hoặc đóng băng, đối tượng ẩn
    QString layout;       // "Model" hoặc tên layout: cột Vị trí, phạm vi Không gian hiện tại
    // Tag của attribute, ô bảng, tên block hoặc xref: thêm vào cột Loại; có thể rỗng.
    QString detail;
    // Chữ như lưu trong bản vẽ (chữ mã cũ: byte gốc đã giải theo code page ANSI).
    QString text;
    // Chữ mã cũ đã giải sang Unicode; rỗng nếu legacy == None.
    // ponytail: nguồn cấp sẵn (test: chữ viết tay); VN-001 (toUnicode) giải khi dựng nguồn.
    QString decoded;
};
using TextList = std::vector<TextItem>;

enum class Scope : std::uint8_t { Drawing, CurrentSpace, Selection };

// Nhóm loại chữ của hộp thoại, mỗi nhóm một ô đánh dấu (07.2 UI-F09 "loại chữ").
enum KindGroup : std::uint8_t {
    kAttributes = 1 << 0, // Attribute
    kDimLeader = 1 << 1,  // Dimension, Leader
    kTexts = 1 << 2,      // Text, MText
    kTables = 1 << 3,     // Table
    kHyperlinks = 1 << 4, // Hyperlink
    kAllKinds = (1 << 5) - 1,
};
KindGroup groupOf(TextKind kind);

struct FindOptions {
    bool matchCase = false;
    bool wholeWord = false;
    bool wildcard = false;
    bool matchAccents = false;      // Phân biệt dấu tiếng Việt
    bool inXrefs = false;           // Tìm trong xref
    bool inBlocks = true;           // Tìm trong block (gồm attribute)
    bool skipHidden = true;         // Bỏ qua đối tượng ẩn
    bool legacy = true;             // Tìm cả chữ mã TCVN3, VNI
    std::uint8_t kinds = kAllKinds; // KindGroup
    Scope scope = Scope::Drawing;
    QString space;                                               // Scope::CurrentSpace: layout hiện tại
    std::shared_ptr<const std::vector<std::uint64_t>> selection; // Scope::Selection: id đã sắp tăng; null = rỗng
};

struct Span {
    int start = 0;
    int length = 0;
    bool operator==(const Span&) const = default;
};

struct Hit {
    int item = 0;         // chỉ số trong TextList
    Span span;            // đoạn khớp trong chữ được so (text, hoặc decoded)
    bool decoded = false; // so trên TextItem::decoded
    bool operator==(const Hit&) const = default;
};

class FindMatcher {
public:
    // needle như người dùng gõ (không cắt khoảng trắng). Biên dịch mẫu ngay; dùng trên một luồng.
    FindMatcher(const QString& needle, FindOptions options);

    // Chuỗi tìm rỗng: không khớp gì.
    bool isEmpty() const { return !valid_; }
    const FindOptions& options() const { return options_; }

    // Đoạn khớp đầu tiên trong s (vị trí UTF-16 của s), nullopt nếu không khớp. Không xét lọc.
    std::optional<Span> match(QStringView s) const;
    // Mục qua bộ lọc: phạm vi, nhóm loại, block, xref, ẩn.
    bool accepts(const TextItem& item) const;
    // Chữ được so của mục: decoded khi bật "Tìm cả chữ mã" và mục có chữ giải mã, không thì text.
    bool usesDecoded(const TextItem& item) const;
    // Xét items[first, last) theo thứ tự, thêm kết quả vào out. Xét cancel mỗi kCheckEvery mục: thấy thì trả ngay.
    void scan(const TextList& items, int first, int last, std::vector<Hit>& out,
              const sched::CancelToken& cancel = {}) const;

    static constexpr int kCheckEvery = 256; // ~0,1 ms việc giữa hai lần xét hủy

private:
    FindOptions options_;
    QRegularExpression re_;
    bool valid_ = false;
    mutable std::vector<char16_t> key_; // bộ đệm khóa của chữ đang so
    mutable std::vector<int> map_;      // map_[k]: vị trí trong chữ gốc của ký tự khóa k
};

// Cả danh sách trên luồng gọi (test, đối chiếu): như scan(items, 0, size).
std::vector<Hit> findAll(const TextList& items, const QString& needle, const FindOptions& options);

} // namespace trion::find
