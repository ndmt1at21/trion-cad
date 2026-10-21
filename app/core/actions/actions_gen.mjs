#!/usr/bin/env node
// Sinh commands.cpp từ danh mục lệnh RAW của prototype (design/prototype/js/cmd.js; UI0-015). Đoạn RAW được chạy
// nguyên văn, không chép tay.
//   node app/core/actions/actions_gen.mjs          ghi lại commands.cpp
//   node app/core/actions/actions_gen.mjs --check  so với tệp trong kho (ctest actions_prototype_check)
// ponytail: metadata tạm thay INT-101/INT-001 (cmd/data/commands/*.json → CommandMetadata.inc); bỏ khi có.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const src = fs.readFileSync(path.join(here, '../../../design/prototype/js/cmd.js'), 'utf8');
const begin = src.indexOf('var RAW = ['), end = src.indexOf('];', begin);
if (begin < 0 || end < 0) throw new Error('cmd.js: không thấy đoạn var RAW = [ … ];');
const RAW = new Function(src.slice(begin, end + 2) + '\nreturn RAW;')();

// Nhãn RAW sai bảng thuật ngữ app/i18n/glossary.tsv (UI0-005, ctest i18n_lint_glossary) thì dùng nhãn đúng bảng.
const GLOSSARY = {
  HATCH: 'Hatch (tô vật liệu)', QLEADER: 'Đường dẫn nhanh', LINETYPE: 'Quản lý kiểu đường (Linetype)',
  MKLTYPE: 'Tạo kiểu đường (Linetype) từ hình vẽ', STYLESMANAGER: 'Quản lý kiểu in (CTB, STB)',
  XATTACH: 'Gắn DWG làm tham chiếu ngoài', XCLIP: 'Clip tham chiếu ngoài, block',
  XBIND: 'Bind đối tượng phụ thuộc của tham chiếu ngoài', REFEDIT: 'Sửa tại chỗ block, tham chiếu ngoài',
  SUPERHATCH: 'Hatch bằng ảnh, block hoặc tham chiếu ngoài', BLOCKTOXREF: 'Đổi block thành tham chiếu ngoài',
  BTABLE: 'Bảng thuộc tính của block', WORKSPACE: 'Không gian làm việc',
  WSSAVE: 'Lưu không gian làm việc hiện tại', WSSETTINGS: 'Thiết lập không gian làm việc',
};
// Ngoại lệ có lý do: chú thích "//: glossary-ok: …" cho lupdate (thành <extracomment> trong .ts).
const GLOSSARY_OK = { EXPLODE: '"phá khối" là tên quen của EXPLODE (phá đối tượng phức), không phải Block' };

// Dòng RAW: TÊN|viết tắt|phiên bản|đợt thiết kế|mô tả|từ khóa tiếng Việt|biểu tượng|vị trí ribbon. Lấy tên, phiên bản
// tối thiểu (V, VP, PRO), mô tả (nhãn tiếng Việt của hành động), biểu tượng.
const rows = RAW.map(r => {
  const [name, , need, , label, , icon] = r.split('|');
  if (!/^[A-Z0-9]+$/.test(name) || !['V', 'VP', 'PRO'].includes(need) || !label || !/^[a-z0-9]+$/.test(icon))
    throw new Error(`cmd.js RAW: dòng lạ "${r}"`);
  return { name, need, label: GLOSSARY[name] ?? label, icon };
});
const names = rows.map(r => r.name);
names.forEach((n, i) => { if (names.indexOf(n) !== i) throw new Error(`cmd.js RAW: trùng lệnh ${n}`); });
for (const n of [...Object.keys(GLOSSARY), ...Object.keys(GLOSSARY_OK)])
  if (!names.includes(n)) throw new Error(`actions_gen.mjs: ${n} không còn trong RAW, bỏ khỏi GLOSSARY/GLOSSARY_OK`);

const q = JSON.stringify; // chuỗi C++: chữ UTF-8 giữ nguyên, chỉ thoát " và \
const text = '// Sinh bởi actions_gen.mjs từ danh mục RAW của design/prototype/js/cmd.js, không sửa tay.\n' +
  '// ponytail: metadata lệnh tạm thay INT-101/INT-001; bỏ tệp này khi có registry lệnh.\n' +
  '#include "app/core/actions/ActionRegistry.h"\n\nnamespace trion::actions {\n\n' +
  'std::span<const CommandInfo> commands()\n{\n    // clang-format off\n' +
  '    static constexpr CommandInfo kAll[] = {\n' +
  rows.map(r => (GLOSSARY_OK[r.name] ? `        //: glossary-ok: ${GLOSSARY_OK[r.name]}\n` : '') +
    `        {${q(r.name)}, ${q(r.need)}, QT_TRANSLATE_NOOP("actions", ${q(r.label)}), ${q(r.icon)}},\n`).join('') +
  '    };\n    // clang-format on\n    return kAll;\n}\n\n} // namespace trion::actions\n';

const file = path.join(here, 'commands.cpp');
if (!process.argv.includes('--check')) {
  fs.writeFileSync(file, text);
  console.log(`actions: ghi ${rows.length} lệnh vào commands.cpp`);
  process.exit(0);
}
const disk = fs.existsSync(file) ? fs.readFileSync(file, 'utf8').replace(/\r\n/g, '\n') : '';
if (disk !== text) {
  console.error('commands.cpp cũ so với RAW của prototype → chạy lại: node app/core/actions/actions_gen.mjs');
  process.exit(1);
}
console.log(`actions: commands.cpp khớp ${rows.length} dòng RAW của prototype`);
