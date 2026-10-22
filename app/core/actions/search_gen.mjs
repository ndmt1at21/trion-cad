#!/usr/bin/env node
// Sinh search_terms.cpp (viết tắt, từ khóa tiếng Việt của lệnh cho ô Tìm lệnh UIA-002 và tự hoàn thành dòng lệnh
// UIA-108; SearchTerms.h) từ danh mục RAW của prototype (design/prototype/js/cmd.js). Đoạn RAW và HIDDEN được chạy
// nguyên văn, không chép tay.
//   node app/core/actions/search_gen.mjs          ghi lại search_terms.cpp
//   node app/core/actions/search_gen.mjs --check  so với tệp trong kho (ctest actions_search_terms_check)
// ponytail: chỉ mục tạm thay INT-017 (cmd/data/commands/*.json: alias, từ khóa); bỏ khi có.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const src = fs.readFileSync(path.join(here, '../../../design/prototype/js/cmd.js'), 'utf8');
const slice = (start, stop) => {
  const begin = src.indexOf(start), end = src.indexOf(stop, begin);
  if (begin < 0 || end < 0) throw new Error(`cmd.js: không thấy đoạn ${start} … ${stop}`);
  return src.slice(begin, end + stop.length);
};
const RAW = new Function(slice('var RAW = [', '];') + '\nreturn RAW;')();
const HIDDEN = new Function(slice('var HIDDEN = {', '};') + '\nreturn HIDDEN;')(); // lệnh nội bộ, không tìm

// Dòng RAW: TÊN|viết tắt|phiên bản|đợt thiết kế|mô tả|từ khóa tiếng Việt|biểu tượng|vị trí ribbon. Lấy tên, viết tắt
// (bỏ viết tắt trùng tên), từ khóa. Nhãn, biểu tượng, phiên bản lấy từ QAction của ActionRegistry; vị trí ribbon do
// RibbonModel (UIA-102) trả qua CommandSearch::setLocator.
const rows = RAW.map(r => r.split('|')).filter(([name]) => !HIDDEN[name]).map(([name, aliases, , , , keywords]) => {
  const alias = (aliases ?? '').split(',').filter(a => a && a !== name);
  if (!/^[A-Z0-9]+$/.test(name) || alias.some(a => !/^[A-Z0-9]+$/.test(a)) || !/^[a-z0-9 ,]+$/.test(keywords ?? ''))
    throw new Error(`cmd.js RAW: dòng lạ "${name}|${aliases}|…|${keywords}"`);
  // Dạng ",C,MT," và ",duong tron,hinh tron,": khớp trùng/đầu chuỗi bằng contains(",X,") / contains(",X") ngay trên
  // chuỗi tĩnh, không tách lúc chạy.
  const list = items => items.length ? `,${items.join(',')},` : '';
  return { name, alias: list(alias), keywords: list(keywords.split(',').map(k => k.trim()).filter(Boolean)) };
});

const q = JSON.stringify;
const text = '// Sinh bởi search_gen.mjs từ danh mục RAW của design/prototype/js/cmd.js, không sửa tay.\n' +
  '// ponytail: chỉ mục tìm lệnh tạm thay INT-017; bỏ tệp này khi có.\n' +
  '#include "app/core/actions/SearchTerms.h"\n\nnamespace trion::actions {\n\n' +
  'std::span<const SearchTerms> searchTerms()\n{\n    // clang-format off\n' +
  '    static constexpr SearchTerms kAll[] = {\n' +
  rows.map(r => `        {${q(r.name)}, ${q(r.alias)}, ${q(r.keywords)}},\n`).join('') +
  '    };\n    // clang-format on\n    return kAll;\n}\n\n} // namespace trion::actions\n';

const file = path.join(here, 'search_terms.cpp');
if (!process.argv.includes('--check')) {
  fs.writeFileSync(file, text);
  console.log(`actions: ghi ${rows.length} lệnh vào search_terms.cpp`);
  process.exit(0);
}
const disk = fs.existsSync(file) ? fs.readFileSync(file, 'utf8').replace(/\r\n/g, '\n') : '';
if (disk !== text) {
  console.error('search_terms.cpp cũ so với RAW của prototype → chạy lại: node app/core/actions/search_gen.mjs');
  process.exit(1);
}
console.log(`actions: search_terms.cpp khớp ${rows.length} dòng RAW của prototype`);
