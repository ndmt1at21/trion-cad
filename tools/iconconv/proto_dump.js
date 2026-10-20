// Đọc dữ liệu prototype bằng node và in JSON ra stdout: biểu tượng (TC.ICONS), vị trí lệnh trên
// ribbon/thanh công cụ/menu (nguồn tạm thay metadata INT-001), biểu tượng của ribbon Viewer và khung.
// Dùng: node proto_dump.js <design/prototype/js>
'use strict';
const fs = require('fs'), path = require('path');
const dir = path.resolve(process.argv[2]);
// DOM giả: Proxy nuốt mọi truy cập và lời gọi; các tệp dưới đây lúc nạp chỉ khai báo dữ liệu.
const stub = new Proxy(function () {}, { get: (t, k) => (k === Symbol.toPrimitive ? () => '' : stub), apply: () => stub, construct: () => stub });
Object.assign(global, { window: global, document: stub, localStorage: stub, location: stub,
  addEventListener: stub, removeEventListener: stub, matchMedia: stub, getComputedStyle: stub, requestAnimationFrame: stub });
for (const f of ['core.js', 'icons.js', 'cmd.js', 'ribbons.js']) require(path.join(dir, f));

const TC = global.TC, CMDS = TC.CMDS;
const positions = [], viewerIcons = new Set();
const add = (where, cmd, icon, viewer) => {
  if (typeof cmd === 'string' && cmd) positions.push({ where, cmd, icon: icon || (CMDS[cmd] && CMDS[cmd].icon) || '', viewer });
};
// ponytail: menu cần trạng thái ứng dụng (view đã lưu, layout…) ném lỗi ở đây và bị bỏ qua; INT-001 thay nguồn này.
const menuOf = (m) => { try { return typeof m === 'function' ? m() || [] : []; } catch (e) { return []; } };

function walkRibbon(rb, name, viewer) {
  for (const tab of rb.tabs) for (const p of tab.panels) {
    const where = name + ' › ' + tab.name + ' › ' + p.name;
    if (viewer && p.icon) viewerIcons.add(p.icon);
    const walk = (list) => list.forEach((it) => {
      if (viewer && it.icon) viewerIcons.add(it.icon);
      add(where, it.cmd, it.icon, viewer);
      if (it.items) walk(it.items);
      for (const e of menuOf(it.menu)) {
        if (viewer && e.icon) viewerIcons.add(e.icon);
        if (e.kbd in CMDS) add(where + ' › ' + String(it.label).replace(/\n/g, ' '), e.kbd, e.icon, viewer);  // mục M_: kbd là tên lệnh
      }
    });
    walk(p.items || []);
    for (const x of p.more || []) if (Array.isArray(x)) add(where + ' › mở rộng', x[1], x[4], viewer);
  }
}
walkRibbon(TC.RIBBON_VIEWER, 'Ribbon Viewer', true);
walkRibbon(TC.RIBBON_PRO, 'Ribbon Pro', false);
for (const [name, , list] of TC.CLASSIC_TB) if (Array.isArray(list)) for (const x of list) if (Array.isArray(x)) add('Thanh công cụ › ' + name, x[1], x[0], false);
for (const [name, list] of TC.CLASSIC_MENUS) for (const x of list) if (Array.isArray(x)) add('Menu › ' + name, x[1], '', false);

// Khung và thanh trạng thái (shell.js dựng DOM nên đọc tên biểu tượng viết thẳng trong mã).
const shell = fs.readFileSync(path.join(dir, 'shell.js'), 'utf8');
const frameIcons = new Set();
for (const re of [/\bic\('(\w+)'/g, /\bicon:'(\w+)'/g, /\btg\('\w+',\s*'(\w+)'/g]) for (const m of shell.matchAll(re)) frameIcons.add(m[1]);

process.stdout.write(JSON.stringify({ icons: TC.ICONS, positions, viewerIcons: [...viewerIcons].sort(), frameIcons: [...frameIcons].sort() }));
