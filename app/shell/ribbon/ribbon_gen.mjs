#!/usr/bin/env node
// Sinh nội dung 8 tab ribbon của workspace Soạn thảo 2D (07.1 §7.2–§7.10, trừ tab Tham số P2) từ TC.RIBBON_PRO của
// prototype (design/prototype/js/ribbons.js; UIA-102). Thân IIFE của ribbons.js được chạy nguyên văn với TC giả: hàm
// act, menu của điều khiển được gọi thật, TC.cmd.run ghi lại lệnh và tham số; TC.toast (prototype báo "có ở v1.0,
// sau v1.0") nghĩa là chưa có lệnh: bỏ điều khiển đó.
//   node app/shell/ribbon/ribbon_gen.mjs          ghi lại resources/<tab>.json, ribbon_strings.cpp
//   node app/shell/ribbon/ribbon_gen.mjs --check  so với tệp trong kho (ctest ribbon_prototype_check)
// ponytail: nội dung tạm theo prototype; UIA-006 hoàn thiện theo bảng 07.1 (cột Phát hành), khi đó tệp JSON sửa tay
// và bỏ bước --check này.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.join(here, '../../..');
const read = f => fs.readFileSync(path.join(root, f), 'utf8');

// Danh mục lệnh RAW (như actions_gen.mjs): TC.CMDS của prototype.
const cmdSrc = read('design/prototype/js/cmd.js');
const rb = cmdSrc.indexOf('var RAW = ['), re = cmdSrc.indexOf('];', rb);
if (rb < 0 || re < 0) throw new Error('cmd.js: không thấy đoạn var RAW = [ … ];');
const CMDS = {};
for (const r of new Function(cmdSrc.slice(rb, re + 2) + '\nreturn RAW;')()) {
  const f = r.split('|');
  CMDS[f[0]] = { name: f[0], need: f[2], desc: f[4], icon: f[6] };
}

// TC giả. Gọi lệnh ghi vào rec; view, màn hình trả giá trị rỗng.
let rec = [];
const run = (cmd, pre) => rec.push({ cmd, args: pre == null ? '' : String(pre) });
const view = { // lệnh ZOOM tương ứng với lời gọi vùng vẽ của prototype
  zoomExtents: () => run('ZOOM', 'E'), zoomPrev: () => run('ZOOM', 'P'),
  zoomAt: (x, y, f) => run('ZOOM', f > 1 ? '2X' : '.5X'), W: 0, H: 0, resize() {}, draw() {}, pushHist() {},
};
const SCREEN_CMD = { exportPdf: 'EXPORTPDF' }; // màn hình của prototype là chính lệnh đó
const empty = () => Object.assign([], { then: () => empty() }); // mảng rỗng, cũng là "promise" không bao giờ xong
const screens = new Proxy({}, { get: (_, k) => (...a) => { if (SCREEN_CMD[k]) run(SCREEN_CMD[k], a[0]); return empty(); } });
const doc = { blocks: {}, views: [], layouts: [], styles: [], dimstyles: [], sel: [], layerMap: {}, byId: {} };
let menus = [];
const TC = {
  $: () => ({ hidden: false, classList: { toggle() {} } }), $$: () => [], esc: String, ic: () => '', CMDS,
  S: { toggles: {}, pickstyle: 0 }, cmd: { run }, toast: () => rec.push({ later: true }), menu: items => menus.push(items),
  app: { doc: () => doc, view, renderRibbon() {}, refreshPalettes() {}, showPalette() {}, toggle() {}, setSpace() {} },
  screens, markup: { props: {}, STAMPS: {}, MCOL: [] }, layers: {}, ed: { cur: () => ({}), clayer: () => '', saveAs() {} },
  clamp: (v, a, b) => Math.min(b, Math.max(a, v)), d: String, tileMenu: () => [], save() {}, emit() {}, ACI: [],
  aciName: String,
};
const rbSrc = read('design/prototype/js/ribbons.js');
const bb = rbSrc.indexOf("'use strict';"), be = rbSrc.lastIndexOf('})();');
if (bb < 0 || be < bb) throw new Error('ribbons.js: không thấy thân IIFE');
const P = new Function('window', rbSrc.slice(bb, be) + '\nreturn { insertGal, viewsMenu, M_ };')({ TC });
if (!TC.RIBBON_PRO) throw new Error('ribbons.js: không có TC.RIBBON_PRO');

// Phần chọn tay, có lý do:
const TAB_ID = { // 8 tab của 07.1 §7.2–§7.10; tab Tham số (§7.5) là P2, sau v1.0
  'Trang chủ': 'home', 'Chèn': 'insert', 'Chú thích': 'annotate', 'Hiển thị': 'view', 'Quản lý': 'manage',
  'Xuất': 'output', 'Công cụ VN': 'vntools', 'Mở rộng': 'express',
};
const CMD = { 'Dòng lệnh': 'COMMANDLINE' }; // prototype bật/tắt vùng dòng lệnh (Ctrl+9 = COMMANDLINE của Keymap)
const SOURCE = { // dropdown: nguồn danh sách động (model do UIA-104, INT-… đặt qua RibbonControls::setModel)
  layerdd: 'layers', vstyle: 'visualstyles', 'rb-ts': 'textstyles', 'rb-ds': 'dimstyles', 'rb-ms': 'mleaderstyles',
  'rb-bs': 'tablestyles', 'rb-color': 'colors', 'rb-lt': 'linetypes', 'rb-lw': 'lineweights', 'rb-anno': 'annoscales',
};
const MENU_SOURCE = new Map([[P.insertGal, 'blocks'], [P.viewsMenu, 'views']]); // menu theo bản vẽ
const NUMERIC = { // ô số của prototype → sysvar AutoCAD tương ứng; độ trong suốt là thanh trượt theo 07.1 §7.2
  'rb-th': { type: 'number', sysvar: 'TEXTSIZE', above: 0 },
  'rb-transp': { type: 'slider', sysvar: 'CETRANSPARENCY', min: 0, max: 90 },
};
const ARGS = { 'ZOOM|Tất cả': 'A' }; // prototype gọi zoomExtents (gần đúng); ZOOM Tất cả của AutoCAD là A
const LABEL = { // nhãn prototype sai bảng thuật ngữ app/i18n/glossary.tsv (ctest i18n_lint_glossary)
  'Linetype': 'Kiểu đường', 'Tạo linetype': 'Tạo kiểu đường', 'Workspace': 'Không gian làm việc',
  'Quản lý plot style': 'Quản lý kiểu in', 'Block thành xref': 'Block thành tham chiếu ngoài',
  'Leader nhanh': 'Đường dẫn nhanh', 'Xuất kiểu dim': 'Xuất kiểu kích thước', 'Nhập kiểu dim': 'Nhập kiểu kích thước',
};
const GLOSSARY_OK = { // ngoại lệ có lý do (chú thích "//: glossary-ok:" cho lupdate)
  'Khung nhìn': '"khung nhìn" ở đây là View đã đặt tên (07.1 §7.6 "Khung nhìn đã đặt tên"), không phải Viewport',
};

const dropped = [];
const btn = { getBoundingClientRect: () => ({ left: 0, top: 0, right: 0, bottom: 0, width: 0, height: 0 }) };
function call(fn, where) {
  rec = []; menus = [];
  try { fn(btn); } catch (e) { throw new Error(`${where}: prototype lỗi khi chạy với TC giả: ${e.message}`); }
  return rec;
}
const label = s => LABEL[s] ?? s;
const plain = s => /<|&/.test(s) ? null : s; // nhãn có HTML: mục "(v1.0)", "(sau v1.0)"

// Mục danh sách (split, panel mở rộng) từ kết quả của M_ hay hàm menu của prototype.
function menuItems(list, where) {
  const out = [];
  for (const it of list) {
    if (it.sep) { if (out.length && !out.at(-1).separator) out.push({ separator: true }); continue; }
    const r = it.dis ? [{ later: true }] : call(it.act, `${where} › ${it.label}`);
    const c = r.find(x => x.cmd);
    if (!c || r.some(x => x.later) || !plain(it.label)) { dropped.push(`${where} › ${it.label}`); continue; }
    const item = { label: label(it.label), cmd: c.cmd };
    const args = ARGS[`${c.cmd}|${it.label}`] ?? c.args;
    if (args) item.args = args;
    if (it.icon && it.icon !== CMDS[c.cmd]?.icon) item.icon = it.icon;
    out.push(item);
  }
  while (out.length && out.at(-1).separator) out.pop();
  return out;
}

function control(c, where) {
  if (c.t === 'col' || c.t === 'row') {
    const items = c.items.map(x => control(x, where)).filter(Boolean);
    return items.length ? { [c.t === 'col' ? 'column' : 'row']: items } : null;
  }
  const at = `${where} › ${(c.label || c.id).replace(/\n/g, ' ')}`;
  if (c.t === 'dd') {
    if (!SOURCE[c.id]) throw new Error(`${at}: dropdown ${c.id} chưa có nguồn trong SOURCE`);
    call(c.act, at); // lệnh "Quản lý…" (mục cuối danh sách) là lệnh của dropdown
    const last = menus.at(-1)?.at(-1);
    const manage = last?.act ? call(last.act, at).find(x => x.cmd) : null;
    return { type: 'dropdown', id: c.id, label: label(c.label), source: SOURCE[c.id], width: c.w,
      ...(manage ? { cmd: manage.cmd } : {}), ...(c.need ? { need: c.need } : {}) };
  }
  if (c.t === 'num') {
    if (!NUMERIC[c.id]) throw new Error(`${at}: ô số ${c.id} chưa có sysvar trong NUMERIC`);
    return { ...NUMERIC[c.id], id: c.id, label: label(c.label), width: c.w };
  }
  if (!['lg', 's', 'io'].includes(c.t)) throw new Error(`${at}: loại điều khiển lạ ${c.t}`);
  const r = c.act ? call(c.act, at) : [];
  const ran = r.find(x => x.cmd);
  const cmd = c.cmd || CMD[c.label] || ran?.cmd;
  if (c.feature || r.some(x => x.later) || !cmd) { dropped.push(at); return null; }
  const out = { type: c.toggle ? 'toggle' : c.menu ? 'split' : 'button',
    size: { lg: 'large', s: 'small', io: 'icon' }[c.t], label: label(c.label), icon: c.icon, cmd };
  if (ran && ran.cmd === cmd && ran.args) out.args = ran.args;
  if (c.id) out.id = c.id;
  if (c.need) out.need = c.need;
  if (c.menu) {
    if (MENU_SOURCE.has(c.menu)) out.source = MENU_SOURCE.get(c.menu);
    else out.menu = menuItems(c.menu(), at);
    if (c.menuOnly) out.menuOnly = true;
  }
  return out;
}

function slug(s) { // "Định nghĩa block" → "dinh-nghia-block"
  return s.normalize('NFD').replace(/[̀-ͯ]/g, '').replace(/[đĐ]/g, 'd').toLowerCase()
    .replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '');
}

const tabs = [];
for (const t of TC.RIBBON_PRO.tabs) {
  if (!TAB_ID[t.name]) { dropped.push(`tab ${t.name}`); continue; }
  const panels = [];
  for (const p of t.panels) {
    const where = `${t.name} › ${p.name}`;
    const items = p.items.map(x => control(x, where)).filter(Boolean);
    const more = p.more ? menuItems(P.M_(p.more)(), `${where} (mở rộng)`) : [];
    if (!items.length) { dropped.push(`panel ${where}`); continue; }
    const panel = { id: slug(p.name), name: label(p.name), icon: p.icon, shrink: p.prio || 1, items };
    if (more.length) panel.more = more;
    if (panels.some(x => x.id === panel.id)) throw new Error(`${where}: trùng id panel ${panel.id}`);
    panels.push(panel);
  }
  tabs.push({ id: TAB_ID[t.name], name: label(t.name), panels });
}
if (tabs.length !== Object.keys(TAB_ID).length) throw new Error('RIBBON_PRO: thiếu tab của TAB_ID');

// Lựa chọn gần nhất của split nhớ theo id (mặc định là lệnh), chung cho mọi tab: split cùng id phải cùng danh sách.
const splits = new Map();
const walk = x => x.row || x.column ? (x.row || x.column).forEach(walk) : x.type === 'split' && x.menu && (() => {
  const key = x.id || x.cmd, menu = JSON.stringify(x.menu);
  if (splits.has(key) && splits.get(key) !== menu) throw new Error(`split ${key}: cùng id nhưng khác danh sách`);
  splits.set(key, menu);
})();
tabs.forEach(t => t.panels.forEach(p => p.items.forEach(walk)));

// Nhãn cho lupdate: nhãn trùng nhãn lệnh (ngữ cảnh "actions", commands.cpp) dùng bản dịch đã có; còn lại "ribbon".
const actionLabels = new Set([...read('app/core/actions/commands.cpp')
  .matchAll(/QT_TRANSLATE_NOOP\("actions", ("(?:[^"\\]|\\.)*")\)/g)].map(m => JSON.parse(m[1])));
const labels = [];
const add = s => { if (s && !actionLabels.has(s) && !labels.includes(s)) labels.push(s); };
const collect = x => {
  if (x.row || x.column) return (x.row || x.column).forEach(collect);
  add(x.label);
  (x.menu || []).forEach(m => add(m.label));
};
for (const t of tabs) {
  add(t.name);
  for (const p of t.panels) { add(p.name); p.items.forEach(collect); (p.more || []).forEach(m => add(m.label)); }
}
for (const k of Object.keys(GLOSSARY_OK)) if (!labels.includes(k)) throw new Error(`GLOSSARY_OK: "${k}" không còn dùng`);

const q = JSON.stringify;
const text = {
  'ribbon_strings.cpp': '// Sinh bởi ribbon_gen.mjs cùng resources/*.json, không sửa tay: để lupdate thấy nhãn của ribbon.\n' +
    '// Hiển thị qua RibbonModel::text(): nhãn trùng nhãn lệnh dùng ngữ cảnh "actions" (commands.cpp), còn lại "ribbon".\n' +
    '#include <QtGlobal>\n\n[[maybe_unused]] static const char* const kStrings[] = {\n' +
    labels.map(s => (GLOSSARY_OK[s] ? `    //: glossary-ok: ${GLOSSARY_OK[s]}\n` : '') +
      `    QT_TRANSLATE_NOOP("ribbon", ${q(s)}),\n`).join('') + '};\n',
};
for (const t of tabs) text[`resources/${t.id}.json`] = JSON.stringify(t, null, 2) + '\n';

if (!process.argv.includes('--check')) {
  for (const [f, t] of Object.entries(text)) fs.writeFileSync(path.join(here, f), t);
  console.log(`ribbon: ghi ${tabs.length} tab, ${labels.length} nhãn ngữ cảnh "ribbon"; bỏ ${dropped.length} mục chưa có ` +
    `lệnh (prototype báo v1.0, sau v1.0, hoặc chỉ là thao tác giao diện):\n  ${dropped.join('\n  ')}`);
  process.exit(0);
}
const stale = Object.entries(text).filter(([f, t]) => {
  const p = path.join(here, f);
  return !fs.existsSync(p) || fs.readFileSync(p, 'utf8').replace(/\r\n/g, '\n') !== t;
}).map(([f]) => f);
if (stale.length) {
  console.error(`${stale.join(', ')} cũ so với prototype → chạy lại: node app/shell/ribbon/ribbon_gen.mjs`);
  process.exit(1);
}
console.log(`ribbon: ${tabs.length} tab và ${labels.length} nhãn khớp TC.RIBBON_PRO của prototype`);
