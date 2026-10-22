#pragma once
// UI0-013 · Kiểm khả năng tiếp cận của một cửa sổ qua QAccessible (07.1 §13.3; DoD-U 7): tên và vai trò của mọi phần
// tử nhận focus, thứ tự Tab (đi hết vòng, không bẫy), viền focus nhìn thấy, cây QAccessible ra JSON để so baseline.
// Test của bảng mẫu và các hộp thoại (tests/a11y_test.cpp) là cổng CI; bước integration chạy audit() trên cửa sổ chính.
//
//   const QStringList issues = trion::a11y::audit(window);   // cửa sổ đang hiện và active; rỗng = đạt
//   QVERIFY2(issues.isEmpty(), qPrintable(issues.join(u'\n')));
//   const QByteArray json = trion::a11y::treeJson(window);    // so với tệp baseline
//
// Trạng thái "khóa theo phiên bản" và "lỗi" không có cờ riêng trong QAccessible::State: theo quy ước của UI0-011
// (LockBadge.h: button->setAccessibleDescription(EditionGate::availableIn(need))) và UI0-009/UI0-007 (FieldMessage,
// NumericField: mô tả tiếp cận = câu lỗi), trình đọc màn hình đọc chúng qua mô tả, nên JSON ghi "description".
//
// Chỉ luồng UI; cần ThemeManager (màu --c-focus).

#include <QByteArray>
#include <QStringList>

class QWidget;

namespace trion::a11y {

// Phần tử nhận focus (focusable, đang hiện) trong cây QAccessible của root thiếu tên, hoặc vai trò chỉ là Client/
// NoRole (widget tự làm không khai vai trò). Mỗi mục: "<lớp> '<objectName>': <lỗi>".
QStringList checkNames(QWidget* root);

// Đi Tab trong cửa sổ đang active (window đang hiện): từ điểm dừng đầu (widget nhận Tab đầu tiên trong chuỗi focus),
// Tab đến khi về lại điểm đầu. Vi phạm: Tab không rời khỏi một widget (bẫy: ô nhiều dòng giữ Tab, bảng Tab giữa các ô),
// vòng không về điểm đầu, điểm dừng không tới được, điểm dừng không có viền --c-focus (so ảnh vùng quanh widget trước
// và sau khi focus: ít nhất 2 × cạnh ngắn điểm ảnh đổi màu mà trước hoặc sau là màu --c-focus). order (nếu có) nhận
// các điểm dừng theo thứ tự Tab. Xong thì trả focus về chỗ cũ. Gửi phím Tab thật: ô nhiều dòng giữ Tab sẽ nhận một ký
// tự tab.
// ponytail: chỉ Tab, không Shift+Tab; widget tự giữ Shift+Tab thì chưa bắt được.
QStringList checkFocus(QWidget* window, QStringList* order = nullptr);

// checkNames + checkFocus.
QStringList audit(QWidget* window);

// Cây QAccessible từ root ra JSON ổn định để so baseline: mỗi nút {role, name, description, value, state, children}
// (khóa sắp xếp, trường rỗng bỏ), không địa chỉ, không tọa độ; trạng thái chỉ các cờ không phụ thuộc lúc chạy (bật,
// vô hiệu, chỉ đọc, nhận focus…; không focused, hotTracked). Nút ẩn bỏ.
// ponytail: ô của bảng, cây, danh sách (con của nút vai trò Table/Tree/List) không xuất: bảng 10.000 dòng. Xuất khi
// có view cần so ô.
QByteArray treeJson(QWidget* root);

} // namespace trion::a11y
