#!/usr/bin/env node
// Sinh keymap.json và ca vàng test từ prototype (UI0-114). Nguồn: design/prototype/js/screens14.js
// (MACSYS, CMDKEYS, TC.keyLabel, KDEF, RESERVED); đoạn mã đó được chạy nguyên văn, không chép tay.
//   node app/core/keymap/keymap_gen.mjs          ghi lại keymap.json, keymap_strings.cpp, tests/golden.json
//   node app/core/keymap/keymap_gen.mjs --check  so từng dòng KDEF, từng trường với tệp trong kho (ctest)
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const src = fs.readFileSync(path.join(here, '../../../design/prototype/js/screens14.js'), 'utf8');
const begin = src.indexOf('var MACSYS'), end = src.indexOf('\n', src.indexOf('var RESERVED'));
if (begin < 0 || end < begin) throw new Error('screens14.js: không thấy đoạn MACSYS … RESERVED');
const TC = { S: { platform: 'mac' } }; // TC giả: keyLabel chỉ cần nền tảng
const P = new Function('TC', src.slice(begin, end) + '\nreturn { MACSYS, CMDKEYS, KDEF, RESERVED };')(TC);

// KDEF [id, nhóm, tên, lệnh, phím " · ", phạm vi, phím thay thế macOS] → bản ghi; lệnh "—" giữ nguyên.
const SCOPE = { 'Toàn cục': 'global', 'Vùng vẽ': 'viewport' };
const actions = P.KDEF.map(([id, group, name, command, keys, scope, macAlt, ...rest]) => {
  if (!SCOPE[scope] || rest.length) throw new Error(`KDEF ${id}: phạm vi "${scope}" hoặc số trường lạ`);
  const keyList = keys.split(' · ');
  return { id, group, name, command, keys: keyList, scope: SCOPE[scope], ...(macAlt ? { macAlt } : {}) };
});
const keymap = { actions, reserved: P.RESERVED, macSystem: P.MACSYS, macCommandKeys: P.CMDKEYS.source };

// Ca vàng keyLabel macOS: mọi phím trong KDEF, RESERVED, MACSYS và lưới 8 tổ hợp phụ × 56 phím.
const MODS = ['', 'Ctrl+', 'Alt+', 'Shift+', 'Ctrl+Alt+', 'Ctrl+Shift+', 'Alt+Shift+', 'Ctrl+Alt+Shift+'];
const KEYS = [...'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789', ...Array.from({ length: 12 }, (_, i) => `F${i + 1}`),
  'Delete', 'Del', 'Tab', 'Space', 'Esc', 'PgUp', 'PgDn', '?'];
const inputs = new Set(['', 'Shift+Ctrl+S', 'Alt+Ctrl+Delete', 'Ctrl+S · Ctrl+L']);
P.KDEF.forEach(k => [k[4], ...k[4].split(' · ')].forEach(c => inputs.add(c)));
[...Object.keys(P.RESERVED), ...Object.keys(P.MACSYS)].forEach(c => inputs.add(c));
MODS.forEach(m => KEYS.forEach(k => inputs.add(m + k)));
const golden = { kdef: P.KDEF, keyLabelMac: [...inputs].map(s => [s, TC.keyLabel(s)]) };

// Mỗi dòng một bản ghi cho dễ đọc diff.
const rows = list => '[\n' + list.map(r => '    ' + JSON.stringify(r)).join(',\n') + '\n  ]';
const json = (fields) => '{\n' + Object.entries(fields).map(([k, v]) => `  "${k}": ${v}`).join(',\n') + '\n}\n';
// Chuỗi hiển thị của keymap.json cho lupdate (ngữ cảnh "keymap").
const shown = [...new Set([...actions.flatMap(a => [a.group, a.name]), ...Object.values(P.RESERVED)])];
const text = {
  'keymap.json': json({ actions: rows(actions), reserved: JSON.stringify(P.RESERVED),
    macSystem: JSON.stringify(P.MACSYS), macCommandKeys: JSON.stringify(keymap.macCommandKeys) }),
  'tests/golden.json': json({ kdef: rows(golden.kdef), keyLabelMac: rows(golden.keyLabelMac) }),
  'keymap_strings.cpp': '// Sinh bởi keymap_gen.mjs, không sửa tay: để lupdate thấy chuỗi hiển thị của keymap.json.\n' +
    '// Hiển thị qua QCoreApplication::translate("keymap", …).\n#include <QtGlobal>\n\n' +
    '[[maybe_unused]] static const char* const kStrings[] = {\n' +
    shown.map(t => `    QT_TRANSLATE_NOOP("keymap", ${JSON.stringify(t)}),\n`).join('') + '};\n',
};

if (!process.argv.includes('--check')) {
  for (const [f, t] of Object.entries(text)) fs.writeFileSync(path.join(here, f), t);
  console.log(`keymap: ghi ${actions.length} hành động, ${golden.keyLabelMac.length} ca vàng keyLabel`);
  process.exit(0);
}

const disk = f => JSON.parse(fs.readFileSync(path.join(here, f), 'utf8'));
const errs = [];
const km = disk('keymap.json');
if ((km.actions || []).length !== actions.length)
  errs.push(`keymap.json có ${(km.actions || []).length} hành động, KDEF có ${actions.length}`);
actions.forEach((a, i) => {
  const d = (km.actions || [])[i] || {};
  for (const k of new Set([...Object.keys(a), ...Object.keys(d)]))
    if (JSON.stringify(a[k]) !== JSON.stringify(d[k]))
      errs.push(`dòng KDEF ${i} (${a.id}) trường ${k}: prototype ${JSON.stringify(a[k])}, ` +
        `keymap.json ${JSON.stringify(d[k])}`);
});
for (const k of ['reserved', 'macSystem', 'macCommandKeys'])
  if (JSON.stringify(km[k]) !== JSON.stringify(keymap[k])) errs.push(`keymap.json: ${k} khác prototype`);
if (JSON.stringify(disk('tests/golden.json')) !== JSON.stringify(golden))
  errs.push('tests/golden.json cũ so với prototype');
const strings = fs.readFileSync(path.join(here, 'keymap_strings.cpp'), 'utf8').replace(/\r\n/g, '\n');
if (strings !== text['keymap_strings.cpp']) errs.push('keymap_strings.cpp cũ so với prototype');
if (errs.length) {
  console.error(errs.join('\n') + '\n→ chạy lại: node app/core/keymap/keymap_gen.mjs');
  process.exit(1);
}
console.log(`keymap: ${actions.length}/${P.KDEF.length} dòng KDEF khớp từng trường; ` +
  `${golden.keyLabelMac.length} ca vàng keyLabel còn mới`);
