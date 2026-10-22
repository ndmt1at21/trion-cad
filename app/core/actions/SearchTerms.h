#pragma once
// Viết tắt và từ khóa tiếng Việt của lệnh, dùng chung cho ô Tìm lệnh (UIA-002, CommandSearch) và tự hoàn thành của
// dòng lệnh (UIA-108, Autocomplete). search_terms.cpp sinh từ danh mục RAW của prototype (design/prototype/js/cmd.js)
// bằng search_gen.mjs; ctest actions_search_terms_check bắt khi danh mục đổi mà chưa sinh lại.
// ponytail: chỉ mục tạm thay INT-017 (cmd/data/commands/*.json: alias, từ khóa); bỏ khi có.

#include <span>

namespace trion::actions {

// Latin-1, mỗi mục kẹp giữa hai dấu phẩy để khớp ngay trên chuỗi tĩnh (contains(",C,") trùng, contains(",C") đầu mục).
struct SearchTerms {
    const char* name;     // "CIRCLE"
    const char* aliases;  // ",C,": viết tắt (không gồm tên lệnh); rỗng nếu không có
    const char* keywords; // ",duong tron,hinh tron,vong tron,": từ khóa tiếng Việt đã gấp (foldForSearch)
};
std::span<const SearchTerms> searchTerms(); // thứ tự danh mục của prototype (gần với độ thông dụng)

} // namespace trion::actions
