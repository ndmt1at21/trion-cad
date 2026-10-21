#pragma once
// Model mẫu cho test và benchmark của UI0-108: dữ liệu sinh ra (snapshot DOC-009 chưa có). Bảng: dựng và sắp ở nền qua
// BackgroundModel::refresh, đổi tên qua setData; cây: làm phẳng ở nền theo tập nút đang mở.
// ponytail: palette thật (Layer, Thuộc tính, cây block: UIC-*) dựng từ snapshot DOC-009, sửa qua lệnh INT-002.

#include "app/widgets/model/BackgroundModel.h"
#include "app/widgets/views/VirtualTreeView.h"

#include <QCoreApplication>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <climits>
#include <cstdint>
#include <memory>
#include <set>
#include <vector>

namespace sample {

using namespace Qt::StringLiterals;
using trion::sched::CancelToken;

struct Item {
    std::uint64_t key = 0;
    QString name;
    int value = 0;
};
using Items = std::vector<Item>;

// Tên kiểu layer có dấu, xoay vòng 5 mẫu: gõ "lop" khớp "Lớp", "LỚP"; "duong" khớp "ĐƯỜNG".
inline QString itemName(int i)
{
    static const QStringList words = {u"Lớp tường"_s, u"KT-CỬA"_s, u"ĐƯỜNG trục"_s, u"LỚP hoàn thiện"_s,
                                      u"Kích thước"_s}; // i18n-ok: dữ liệu thử
    return u"%1 %2"_s.arg(words[i % 5]).arg(i, 6, 10, QChar(u'0'));
}

inline std::shared_ptr<const Items> makeItems(int n)
{
    auto items = std::make_shared<Items>();
    items->reserve(std::size_t(n));
    for (int i = 0; i < n; ++i)
        items->push_back({std::uint64_t(i) + 1, itemName(i), (i * 7919) % 1000});
    return items;
}

struct TableRow {
    std::uint64_t key = 0;
    QString name;
    int value = 0;
    bool operator==(const TableRow&) const = default;
};

// Cột: 0 tên (sửa được), 1 giá trị, 2 khóa. sort() dựng lại thứ tự ở nền; data() đếm số lần gọi (luồng UI).
class TableModel : public trion::ui::BackgroundModel<TableRow> {
public:
    void load(std::shared_ptr<const Items> items)
    {
        items_ = std::move(items);
        rebuild();
    }
    const std::shared_ptr<const Items>& items() const { return items_; }

    int columnCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : 3; }
    QVariant data(const QModelIndex& index, int role) const override
    {
        ++dataCalls;
        if (!index.isValid())
            return {};
        const TableRow& r = row(index.row());
        if (role == Qt::DisplayRole || role == Qt::EditRole)
            return index.column() == 0   ? QVariant(r.name)
                   : index.column() == 1 ? QVariant(r.value)
                                         : QVariant(qulonglong(r.key));
        if (role == Qt::TextAlignmentRole && index.column() > 0) // .tbl td.num
            return QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter);
        return {};
    }
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (orientation == Qt::Horizontal && role == Qt::TextAlignmentRole && section > 0) // .tbl th.num
            return QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter);
        if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
            return {};
        return section == 0   ? QCoreApplication::translate("ViewsSample", "Tên")
               : section == 1 ? QCoreApplication::translate("ViewsSample", "Giá trị")
                              : QCoreApplication::translate("ViewsSample", "Mã");
    }
    Qt::ItemFlags flags(const QModelIndex& index) const override
    {
        const Qt::ItemFlags f = BackgroundModel::flags(index);
        return index.column() == 0 ? f | Qt::ItemIsEditable : f;
    }
    // Đổi tên: snapshot mới (palette thật: lệnh INT-002, snapshot mới của DOC-009).
    bool setData(const QModelIndex& index, const QVariant& value, int role) override
    {
        if (role != Qt::EditRole || index.column() != 0 || !index.isValid())
            return false;
        auto next = std::make_shared<Items>(*items_);
        const std::uint64_t key = row(index.row()).key;
        for (Item& it : *next)
            if (it.key == key)
                it.name = value.toString();
        items_ = std::move(next);
        rebuild();
        return true;
    }
    void sort(int column, Qt::SortOrder order) override
    {
        column_ = column;
        order_ = order;
        ++sorts;
        rebuild();
    }

    // Thứ tự của cách sắp (cột, chiều); khóa phân xử để thứ tự tất định.
    static bool before(const TableRow& a, const TableRow& b, int column, Qt::SortOrder order)
    {
        if (order == Qt::DescendingOrder)
            return before(b, a, column, Qt::AscendingOrder);
        if (column == 0)
            if (const int c = QString::compare(a.name, b.name, Qt::CaseInsensitive); c != 0)
                return c < 0;
        if (column == 1 && a.value != b.value)
            return a.value < b.value;
        return a.key < b.key;
    }

    mutable int dataCalls = 0;
    int sorts = 0;
    int column_ = -1;
    Qt::SortOrder order_ = Qt::AscendingOrder;

private:
    void rebuild()
    {
        refresh(items_, ++version_, [column = column_, order = order_](const Items& s, const CancelToken& cancel) {
            std::vector<TableRow> rows;
            rows.reserve(s.size());
            for (const Item& it : s) {
                if ((rows.size() & 4095) == 0 && cancel.cancelled())
                    return rows;
                rows.push_back({it.key, it.name, it.value});
            }
            if (column >= 0)
                std::sort(rows.begin(), rows.end(),
                          [&](const TableRow& a, const TableRow& b) { return before(a, b, column, order); });
            return rows;
        });
    }

    std::shared_ptr<const Items> items_ = std::make_shared<Items>();
    std::uint64_t version_ = 0;
};

// Cây: nút theo thứ tự trước (cha rồi tới con), mức depth; con của nút là các nút liền sau có mức lớn hơn.
struct Node {
    std::uint64_t key = 0;
    int depth = 0;
    bool parent = false;
    QString name;
};
using Nodes = std::vector<Node>;

// groups nhóm gốc, mỗi nhóm `sub` nhóm con, mỗi nhóm con `leaves` lá (sub = 0: lá ngay dưới nhóm gốc).
inline std::shared_ptr<const Nodes> makeTree(int groups, int sub, int leaves)
{
    auto nodes = std::make_shared<Nodes>();
    std::uint64_t key = 1;
    int leaf = 0;
    for (int g = 0; g < groups; ++g) {
        nodes->push_back({key++, 0, true, u"Nhóm %1"_s.arg(g)}); // i18n-ok: dữ liệu thử
        for (int s = 0; s < std::max(sub, 1); ++s) {
            if (sub > 0)
                nodes->push_back({key++, 1, true, u"Nhóm con %1.%2"_s.arg(g).arg(s)}); // i18n-ok: dữ liệu thử
            for (int l = 0; l < leaves; ++l)
                nodes->push_back({key++, sub > 0 ? 2 : 1, false, itemName(leaf++)});
        }
    }
    return nodes;
}

struct TreeRow {
    std::uint64_t key = 0;
    QString name;
    int depth = 0;
    signed char open = -1; // -1 lá, 0 đóng, 1 mở
    bool operator==(const TreeRow&) const = default;
};

class TreeModel : public trion::ui::BackgroundModel<TreeRow> {
public:
    using View = trion::ui::VirtualTreeView;

    void load(std::shared_ptr<const Nodes> nodes, std::set<std::uint64_t> open = {})
    {
        nodes_ = std::move(nodes);
        open_ = std::move(open);
        rebuild();
    }

    int columnCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : 2; }
    QVariant data(const QModelIndex& index, int role) const override
    {
        if (!index.isValid())
            return {};
        const TreeRow& r = row(index.row());
        if (role == Qt::DisplayRole)
            return index.column() == 0 ? QVariant(r.name) : QVariant(qulonglong(r.key));
        if (index.column() == 0 && role == View::DepthRole)
            return r.depth;
        if (index.column() == 0 && role == View::ExpandedRole && r.open >= 0)
            return r.open == 1;
        if (role == Qt::TextAlignmentRole && index.column() == 1)
            return QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter);
        return {};
    }
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (orientation == Qt::Horizontal && role == Qt::TextAlignmentRole && section > 0) // .tbl th.num
            return QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter);
        if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
            return {};
        return section == 0 ? QCoreApplication::translate("ViewsSample", "Tên")
                            : QCoreApplication::translate("ViewsSample", "Mã");
    }
    bool setData(const QModelIndex& index, const QVariant& value, int role) override
    {
        if (role != View::ExpandedRole || !index.isValid() || row(index.row()).open < 0)
            return false;
        const std::uint64_t key = row(index.row()).key;
        if (value.toBool())
            open_.insert(key);
        else
            open_.erase(key);
        rebuild();
        return true;
    }

private:
    // Làm phẳng ở nền: duyệt thứ tự trước, bỏ cây con của nút đóng.
    void rebuild()
    {
        refresh(nodes_, ++version_, [open = open_](const Nodes& s, const CancelToken& cancel) {
            std::vector<TreeRow> rows;
            int skip = INT_MAX; // đang ở trong cây con của nút đóng mức này
            for (std::size_t i = 0; i < s.size(); ++i) {
                if ((i & 4095) == 0 && cancel.cancelled())
                    return rows;
                const Node& n = s[i];
                if (n.depth > skip)
                    continue;
                skip = INT_MAX;
                const bool expanded = n.parent && open.count(n.key) > 0;
                rows.push_back({n.key, n.name, n.depth, static_cast<signed char>(n.parent ? expanded : -1)});
                if (n.parent && !expanded)
                    skip = n.depth;
            }
            return rows;
        });
    }

    std::shared_ptr<const Nodes> nodes_ = std::make_shared<Nodes>();
    std::set<std::uint64_t> open_;
    std::uint64_t version_ = 0;
};

} // namespace sample
