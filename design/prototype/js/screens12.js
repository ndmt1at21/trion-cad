/* TrionCAD prototype · Đợt 4: dữ liệu và tệp (I05 bảng diện tích phòng, I06 trích xuất dữ liệu, J15 nhập attribute, J12 Excel đầy đủ, B11 đóng gói gửi đi, B14 xử lý tệp hàng loạt). */
(function(){
  'use strict';
  var TC = window.TC, $ = TC.$, $$ = TC.$$, esc = TC.esc, ic = TC.ic, G = TC.geo, IMPL = TC.cmd.impl, ED = TC.ed, SC = TC.screens, H = SC.h;
  function A(){ return TC.app; } function V(){ return TC.app.view; } function D(){ return TC.app.doc(); }
  var OPEN = SC.open4 = SC.open4 || {}; // mã màn hình → hàm mở (danh mục gọi khi bấm); tiện ích mở ở SC.cat (D1, PRO, MOD, RUN, plan, pick, flash)
  function ctip(r, msg){ var s = r && r.s || V().mouse || [100, 100]; TC.ctip($('#cv'), s[0], s[1], msg); }
  function num2(v){ return v == null || v === '' || isNaN(v) ? '' : String(+Number(v).toFixed(2)); }
  function fileBase(s){ var b = TC.fold(s).replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '') || 'bang'; return b[0].toUpperCase() + b.slice(1); }
  function dirOf(p){ return p.replace(/\\[^\\]+$/, ''); }
  // tiêu đề cột sắp xếp được bằng chuột và bàn phím
  function sortTh(attr, k, lab, cur, dir, cls){ return '<th class="sortable' + (cls ? ' ' + cls : '') + '" aria-sort="' + (cur === k ? (dir > 0 ? 'ascending' : 'descending') : 'none') + '"><button type="button" class="srt" ' + attr + '="' + esc(k) + '">' + esc(lab) + (cur === k ? ic(dir > 0 ? 'up' : 'down', 'xs') : '') + '</button></th>'; }

  /* ================= Bảng diện tích phòng UI-I05 (TCROOMAREA) ================= */
  // phòng: vùng kín dò từ tường như BOUNDARY/HATCH (ED.regionAt, không trừ island) hoặc polyline kín, hatch đã chọn; tên từ chữ trong vùng, attribute hoặc nhập tay
  var RA = SC.RA = { src:'text', layer:'KT-TEXT', tag:'', sort:'i', dir:1, rows:[], hot:-1, seq:0, on:false, mode:'' };
  var RA_SRC = { text:'Chữ trong vùng', att:'Attribute', hand:'Nhập tay' }, AREA_TXT = /^\s*(S\s*=|[±+\-]?\d)/i;
  function loopAt(p){ var hs = ED.hset, old = hs.island; hs.island = 'Bỏ qua'; var g = ED.regionAt(p); hs.island = old; return g && g.outer; }
  function inLoop(e, l){ var p = e.p || e.c; return !!p && G.inPoly(p, l); }
  function roomName(d, l){
    var best = null;
    ED.spaceList(d).forEach(function(e){
      if (RA.src === 'att'){ if (!best && e.t === 'insert' && inLoop(e, l)){ var v = SC.att.attrsOf(d, e)[RA.tag]; if (v) best = { s:v }; } }
      else if ((e.t === 'text' || e.t === 'mtext') && (RA.layer === '*' || e.layer === RA.layer) && inLoop(e, l) && !AREA_TXT.test(e.s) && (!best || e.h > best.h)) best = e;
    });
    return best ? String(best.s).replace(/\s*\n\s*/g, ' ').trim() : '';
  }
  function roomAdd(d, l, area, from){
    var c = l.reduce(function(s, p){ return [s[0] + p[0] / l.length, s[1] + p[1] / l.length]; }, [0, 0]);
    if (RA.rows.some(function(r){ return Math.abs(r.area - area) < 1 && G.inPoly(c, r.loop); })) return null;
    var r = { loop:l, area:area, per:G.polyLen(l, true), from:from, note:'', i:RA.seq++, auto:RA.src !== 'hand' }; r.name = r.auto ? roomName(d, l) : '';
    RA.rows.push(r); return r;
  }
  function raRename(d){ RA.rows.forEach(function(r){ var n = r.auto && RA.src !== 'hand' && roomName(d, r.loop); if (n) r.name = n; }); } // nguồn mới không có tên thì giữ tên cũ
  function raView(){ var k = RA.sort; return RA.rows.slice().sort(function(a, b){ return RA.dir * (k === 'name' ? a.name.localeCompare(b.name, 'vi') : k === 'area' ? a.area - b.area : k === 'per' ? a.per - b.per : a.i - b.i); }); }
  function m2(r){ return TC.d(r.area / 1e6); } function m1(r){ return TC.d(r.per / 1000); }
  function raSum(list){ return list.reduce(function(s, r){ return s + Number(m2(r)); }, 0); } // cộng số đã làm tròn để bảng khớp từng dòng
  function raTable(list, p){
    var cells = [['BẢNG DIỆN TÍCH PHÒNG', '', '', '', ''], ['STT', 'Tên phòng', 'Diện tích (m²)', 'Chu vi (m)', 'Ghi chú']].concat(list.map(function(r, i){ return [String(i + 1), r.name, m2(r), m1(r), r.note]; }), [['Tổng cộng', '', TC.d(raSum(list)), '', '']]);
    return { t:'table', p:p, cols:[1000, 3800, 3100, 2400, 2600], rows:[900, 750].concat(list.map(function(){ return 650; }), [700]), th:250, head:2, cells:cells, merge:[[0, 0, 4], [cells.length - 1, 0, 1]], tstyle:'TCVN-dien-tich', style:'Standard' };
  }
  SC.roomTable = function(d, pts, p){ var keep = RA.rows; RA.rows = []; RA.seq = 0; pts.forEach(function(q){ var l = loopAt(q); if (l) roomAdd(d, l, G.polyArea(l), 'Dò từ tường'); }); var t = raTable(RA.rows, p); RA.rows = keep; return t; };
  // lớp phủ: phòng đã lấy tô nhạt, số thứ tự ở góc trên trái, dòng đang chọn đậm hơn
  function raOverlay(x, v){
    if (!RA.on || D() !== RA.d) return;
    var S = function(p){ return v.mToS(p) || v.toS(p); }, c = TC.cssv('--c-hl-res');
    x.save(); x.font = '600 11px ' + TC.cssv('--f-mono'); x.textAlign = 'center'; x.textBaseline = 'middle';
    raView().forEach(function(r, i){
      var hot = RA.rows.indexOf(r) === RA.hot, pts = r.loop.map(S), x0 = Math.min.apply(null, pts.map(function(s){ return s[0]; })), y0 = Math.min.apply(null, pts.map(function(s){ return s[1]; }));
      x.beginPath(); pts.forEach(function(s, k){ if (k) x.lineTo(s[0], s[1]); else x.moveTo(s[0], s[1]); }); x.closePath();
      x.fillStyle = c; x.globalAlpha = hot ? .24 : .1; x.fill(); x.globalAlpha = 1; x.strokeStyle = c; x.lineWidth = hot ? 2.6 : 1.4; x.stroke();
      x.beginPath(); x.arc(x0 + 14, y0 + 14, 9, 0, 7); x.fill(); x.fillStyle = TC.cssv('--c-canvas'); x.fillText(String(i + 1), x0 + 14, y0 + 14.5);
    });
    x.restore();
  }
  function raLayers(d){ var m = {}; ED.spaceList(d).forEach(function(e){ if (e.t === 'text' || e.t === 'mtext') m[e.layer] = 1; }); return Object.keys(m).sort(); }
  function raTags(d){ var t = []; ED.spaceList(d).forEach(function(e){ if (e.t === 'insert' && SC.att.hasAtt(d, e.name) && RA.rows.some(function(r){ return inLoop(e, r.loop); })){ var o = {}; o[e.name] = true; SC.att.tags(d, o).forEach(function(g){ if (t.indexOf(g) < 0) t.push(g); }); } }); return t; }
  function raDialog(){
    var d = RA.d;
    var dlg = TC.dialog({ id:'ra', title:'Bảng diện tích phòng', cmd:'TCROOMAREA', code:'UI-I05', w:580, modal:false, body:'<div class="stack" id="ra-b"></div>', foot:'<span></span>',
      onClose:function(r){ RA.dlg = null; RA.ui = null; if (r !== 'done' && TC.cmdline.current() === 'TCROOMAREA') TC.cmd.cancel(); } });
    var cr = $('#cv').getBoundingClientRect(); dlg.el.style.left = Math.max(12, cr.right - 592) + 'px'; dlg.el.style.top = (cr.top + 40) + 'px';
    RA.dlg = dlg; RA.ui = render;
    function warn(){ var n = RA.rows.filter(function(r){ return !r.name; }).length, w = $('#ra-w', dlg.el); if (w) w.innerHTML = n ? '<div class="note warn">' + ic('warn') + '<span>' + n + ' phòng chưa có tên' + (RA.src === 'text' ? ' (không có chữ' + (RA.layer === '*' ? '' : ' trên layer ' + esc(RA.layer)) + ' trong vùng)' : RA.src === 'att' ? ' (không có attribute trong vùng)' : '') + '. Gõ tên vào bảng; bảng vẫn chèn được với ô trống.</span></div>' : ''; }
    function render(){
      if (!document.contains(dlg.el)) return;
      var list = raView(), n = list.length, tags = RA.src === 'att' ? raTags(d) : [], pick = RA.mode === 'pick', place = RA.mode === 'place';
      if (RA.src === 'att' && tags.indexOf(RA.tag) < 0) RA.tag = tags.filter(function(t){ return /TEN|NAME|PHONG/.test(t); })[0] || tags[0] || '';
      $('#ra-b', dlg.el).innerHTML =
        '<div class="frow" style="--lab-w:118px"><span class="lab">Tên phòng lấy từ</span><span class="hrow">' + SC.seg('rasrc', [RA_SRC.text, RA_SRC.att, RA_SRC.hand], RA_SRC[RA.src]) + '</span>' +
          (RA.src === 'text' ? '<label class="lab" for="ra-layer">Layer chữ tên phòng</label><select class="inp" id="ra-layer" style="width:210px"><option value="*"' + (RA.layer === '*' ? ' selected' : '') + '>Mọi layer</option>' + raLayers(d).map(function(l){ return '<option' + (l === RA.layer ? ' selected' : '') + '>' + esc(l) + '</option>'; }).join('') + '</select>'
            : RA.src === 'att' ? '<label class="lab" for="ra-tag">Tag tên phòng</label>' + (tags.length ? '<select class="inp" id="ra-tag" style="width:210px">' + SC.opts(tags, RA.tag) + '</select>' : '<span class="faint" style="font-size:var(--fs-s)">Chưa có block có attribute trong các phòng đã chọn.</span>')
            : '<span></span><span class="faint" style="font-size:var(--fs-s)">Mỗi lần chỉ điểm, dòng lệnh hỏi tên phòng.</span>') + '</div>' +
        (n ? '<div class="tblwrap ra-tbl"><table class="tbl"><colgroup><col style="width:46px"><col><col style="width:104px"><col style="width:84px"><col style="width:112px"><col style="width:30px"></colgroup><thead><tr>' + sortTh('data-ras', 'i', 'STT', RA.sort, RA.dir, 'num') + sortTh('data-ras', 'name', 'Tên phòng', RA.sort, RA.dir) + sortTh('data-ras', 'area', 'Diện tích (m²)', RA.sort, RA.dir, 'num') + sortTh('data-ras', 'per', 'Chu vi (m)', RA.sort, RA.dir, 'num') + '<th>Ghi chú</th><th></th></tr></thead><tbody>' +
          list.map(function(r, i){ var k = RA.rows.indexOf(r); return '<tr data-rar="' + k + '"' + (k === RA.hot ? ' class="is-sel"' : '') + ' title="' + esc(r.from) + '"><td class="num">' + (i + 1) + '</td><td><input class="inp sm ra-in' + (r.name ? '' : ' miss') + '" data-ran="' + k + '" value="' + esc(r.name) + '" placeholder="Chưa có tên" aria-label="Tên phòng ' + (i + 1) + '" spellcheck="false"></td><td class="num">' + m2(r) + '</td><td class="num">' + m1(r) + '</td><td><input class="inp sm ra-in" data-rano="' + k + '" value="' + esc(r.note) + '" aria-label="Ghi chú phòng ' + (i + 1) + '"></td><td class="ctr"><button class="ib sm" type="button" data-radel="' + k + '" aria-label="Bỏ phòng ' + (i + 1) + ' khỏi bảng" data-tip="Bỏ khỏi bảng">' + ic('x') + '</button></td></tr>'; }).join('') +
          '</tbody><tfoot><tr><td></td><td>Tổng cộng · ' + n + ' phòng</td><td class="num">' + TC.d(raSum(list)) + '</td><td></td><td colspan="2"></td></tr></tfoot></table></div>'
          : '<div class="empty">' + ic('area') + '<b>Chưa có phòng nào</b><span>Bấm vào bên trong từng phòng trên bản vẽ: TrionCAD dò đường biên kín từ tường. Hoặc chọn polyline kín, hatch.</span><button class="btn sm" type="button" data-ra="S"' + (pick ? '' : ' disabled') + '>' + ic('select') + 'Chọn polyline kín, hatch</button></div>') +
        '<div id="ra-w"></div>' +
        '<div class="hrow faint" style="font-size:var(--fs-xs)"><span>Làm tròn 2 chữ số; tổng theo số đã làm tròn.</span><span class="grow"></span><label class="chk dis" data-tip="Tự cập nhật theo đường biên" data-desc="Diện tích trong bảng liên kết với đường biên qua field, tự cập nhật khi sửa tường. Có sau v1.0."><input type="checkbox" disabled> Tự cập nhật theo đường biên <span class="tag">Sau v1.0</span></label></div>';
      warn();
      $('.dlg-f', dlg.el).innerHTML = (pick ? '<span class="hrow faint" style="font-size:var(--fs-s);gap:6px">' + ic('idpoint', 's12') + 'Đang chỉ điểm trong phòng · Enter để xong</span>' : place ? '<span class="hrow faint" style="font-size:var(--fs-s);gap:6px">' + ic('idpoint', 's12') + 'Chỉ điểm chèn bảng trên bản vẽ</span>' : '<button class="btn" type="button" data-ra="A">' + ic('plus') + 'Thêm phòng</button>') +
        '<span class="grow"></span><button class="btn" type="button" data-ra="xls" aria-haspopup="menu"' + (n ? '' : ' disabled') + '>' + ic('stats') + 'Xuất Excel' + ic('down', 'xs') + '</button><button class="btn primary" type="button" data-ra="C"' + (n && !place ? '' : ' disabled') + '>' + ic('table') + 'Chèn bảng ' + n + ' phòng</button>';
    }
    dlg.el.addEventListener('click', function(e){
      var s = e.target.closest('[data-seg="rasrc"] button'); if (s){ RA.src = Object.keys(RA_SRC).filter(function(k){ return RA_SRC[k] === s.dataset.v; })[0]; raRename(d); render(); return; }
      var h = e.target.closest('[data-ras]'); if (h){ var k = h.dataset.ras; RA.dir = RA.sort === k ? -RA.dir : 1; RA.sort = k; render(); V().drawOver(); var f = $('[data-ras="' + k + '"]', dlg.el); if (f) f.focus(); return; }
      var x = e.target.closest('[data-radel]'); if (x){ RA.rows.splice(Number(x.dataset.radel), 1); RA.hot = -1; render(); V().drawOver(); return; }
      var row = e.target.closest('[data-rar]'); if (row && !e.target.closest('input, button')){ RA.hot = Number(row.dataset.rar); render(); V().drawOver(); return; }
      var b = e.target.closest('[data-ra]'); if (!b || b.disabled) return; var k2 = b.dataset.ra;
      if (k2 === 'xls'){ var rc = b.getBoundingClientRect(), nm = 'Dien-tich-phong_' + SC.att.ascii(SC.att.base(d.name)), cnt = RA.rows.length;
        TC.menu([{ label:'Excel (.xlsx)', act:function(){ TC.toast('Đã xuất <span class="mono">' + esc(nm) + '.xlsx</span>: ' + cnt + ' phòng và dòng tổng, số định dạng 0.00.', { act:{ label:'Mở tệp', fn:function(){} }, kind:'task' }); } }, { label:'CSV (UTF-8)', act:function(){ TC.toast('Đã xuất <span class="mono">' + esc(nm) + '.csv</span>: ' + cnt + ' phòng.', { log:false }); } }], rc.left, rc.top - 4, { up:true }); }
      else if (TC.cmdline.current() === 'TCROOMAREA') TC.cmdline.submit(k2);
    });
    // gõ tên: cập nhật ngay, không vẽ lại bảng để Tab đi tiếp được
    dlg.el.addEventListener('input', function(e){ var t = e.target;
      if (t.dataset.ran){ var r = RA.rows[Number(t.dataset.ran)]; r.name = t.value.trim(); r.auto = false; t.classList.toggle('miss', !r.name); warn(); }
      else if (t.dataset.rano) RA.rows[Number(t.dataset.rano)].note = t.value; });
    dlg.el.addEventListener('change', function(e){ var t = e.target; if (t.id === 'ra-layer'){ RA.layer = t.value; raRename(d); render(); } else if (t.id === 'ra-tag'){ RA.tag = t.value; raRename(d); render(); } });
    dlg.el.addEventListener('focusin', function(e){ var row = e.target.closest('[data-rar]'); if (row && Number(row.dataset.rar) !== RA.hot){ RA.hot = Number(row.dataset.rar); $$('[data-rar]', dlg.el).forEach(function(x){ x.classList.toggle('is-sel', x === row); }); V().drawOver(); } });
    render(); return dlg;
  }
  IMPL.TCROOMAREA = function(ctx){
    var d = D(); ctx.badge('S');
    Object.assign(RA, { rows:[], hot:-1, seq:0, d:d, on:true, mode:'' });
    if (RA.layer !== '*' && !d.layerMap[RA.layer]) RA.layer = '*';
    if (TC.overlays.indexOf(raOverlay) < 0) TC.overlays.push(raOverlay);
    raDialog();
    ctx.out('Chỉ điểm bên trong từng phòng: TrionCAD dò đường biên kín từ tường. Tên phòng lấy từ ' + (RA.src === 'hand' ? 'dòng lệnh' : RA.src === 'att' ? 'attribute' : 'chữ trong vùng' + (RA.layer === '*' ? '' : ' (layer ' + esc(RA.layer) + ')')) + '.');
    function ui(m){ RA.mode = m; if (RA.ui) RA.ui(); V().drawOver(); }
    function pick(){
      ui('pick');
      return ctx.point('Chỉ điểm bên trong phòng hoặc', { pick:true, dyn:false, opts:[['Chèn bảng', 'C'], ['Chọn đối tượng', 'S'], ['Hoàn tác', 'U']], onMove:function(p, temp){ var l = loopAt(p); if (l) temp.push({ k:'poly', pts:l, closed:true, color:TC.cssv('--c-accent'), w:1.8, fill:TC.cssv('--c-sel-wf') }); } }).then(function(r){
        if (r.enter) return RA.rows.length ? menu() : null;
        if (r.kw === 'C') return place();
        if (r.kw === 'U'){ var last = RA.rows.reduce(function(m, x){ return !m || x.i > m.i ? x : m; }, null); if (last){ RA.rows.splice(RA.rows.indexOf(last), 1); RA.hot = -1; ctx.out('Đã bỏ ' + (last.name ? '“' + esc(last.name) + '”' : 'phòng vừa thêm') + ' khỏi bảng.'); } else ctx.out('Không còn phòng để hoàn tác.'); return pick(); }
        if (r.kw === 'S') return ctx.sel('Chọn polyline kín, hatch').then(function(ids){ var n = 0; ids.forEach(function(id){ var e = d.byId[id]; if (!e) return; if (e.t === 'pline' && e.closed && e.pts.length > 2){ if (roomAdd(d, e.pts.slice(), G.polyArea(e.pts), 'Từ polyline kín')) n++; } else if (e.t === 'hatch'){ if (roomAdd(d, e.loops[0], Math.abs(e.loops.reduce(function(s, l, k){ return s + (k ? -1 : 1) * G.polyArea(l); }, 0)), 'Từ hatch')) n++; } }); TC.cad.clearSel(); ctx.out(n ? 'Đã thêm ' + n + ' phòng từ đối tượng đã chọn.' : 'Không có polyline kín, hatch mới trong vùng chọn.'); return pick(); });
        if (!r.p) return pick();
        var l = loopAt(r.p); if (!l){ ctip(r, 'Không tìm thấy vùng kín tại đây. Kiểm tra khe hở ở tường, hoặc chọn polyline kín (S).'); return pick(); }
        var row = roomAdd(d, l, G.polyArea(l), 'Dò từ tường'); if (!row){ ctip(r, 'Phòng này đã có trong bảng'); return pick(); }
        ctx.out('Phòng ' + RA.rows.length + (row.name ? ' “' + esc(row.name) + '”' : '') + ': ' + m2(row) + ' m², chu vi ' + m1(row) + ' m.');
        if (RA.src !== 'hand') return pick();
        ui('pick'); return ctx.val('Tên phòng', 'Phòng ' + RA.rows.length, { allowSpace:true }).then(function(v){ row.name = String(v).trim(); row.auto = false; return pick(); });
      });
    }
    function menu(){ ui('menu'); return ctx.kw('Sửa tên, ghi chú trong bảng rồi', [['Chèn bảng', 'C'], ['Thêm phòng', 'A']], 'C').then(function(k){ return k === 'A' ? pick() : k === 'C' ? place() : menu(); }); }
    function place(){
      if (!RA.rows.length){ ctx.out('Chưa có phòng nào. Chỉ điểm bên trong phòng trước.'); return pick(); }
      ui('place'); var list = raView();
      return ctx.point('Điểm chèn bảng (góc trên trái) hoặc', { opts:[['Quay lại', 'B']], onMove:function(p, temp){ temp.push(ED.temp([raTable(list, p)], { real:true })); } }).then(function(r){
        if (!r.p) return menu();
        ED.run('TCROOMAREA', function(){ ED.add(ED.make(raTable(list, r.p))); });
        ctx.out('Đã chèn bảng diện tích ' + list.length + ' phòng, tổng ' + TC.d(raSum(list)) + ' m² (kiểu bảng TCVN-dien-tich). Xuất lại ra Excel bằng TCEXCELOUT.');
      });
    }
    function fin(){ RA.on = false; RA.mode = ''; var i = TC.overlays.indexOf(raOverlay); if (i >= 0) TC.overlays.splice(i, 1); if (RA.dlg) RA.dlg.close('done'); V().drawOver(); }
    return pick().then(fin, function(err){ fin(); throw err; });
  };

  /* ================= Trích xuất dữ liệu UI-I06 (DATAEXTRACTION) ================= */
  // v1.0: bước 1–5 rút gọn, chạy trên dữ liệu thật của bản vẽ; bước 6 (kiểu bảng), 7 (lưu thiết lập .dxe) và nguồn nhiều bản vẽ có sau v1.0
  var DX_STEPS = ['Nguồn', 'Đối tượng', 'Thuộc tính', 'Tinh chỉnh', 'Đầu ra', 'Kiểu bảng', 'Lưu thiết lập'];
  var DX = SC.DX = { step:1, src:'doc', ids:null, kind:'block', att:false, on:null, cats:{}, props:{ name:true }, names:{}, comb:true, cnt:true, sort:'', dir:1, q:'', tq:'', pq:'', table:true, file:true, fmt:'XLSX', open:true, title:'BẢNG THỐNG KÊ' };
  var DXP = [
    ['name', 'Tên', 'Chung', function(d, e){ return e.t === 'insert' ? e.name : TC.TYPE_NAMES[e.t] || e.t; }],
    ['layer', 'Layer', 'Chung', function(d, e){ return e.layer; }],
    ['color', 'Màu', 'Chung', function(d, e){ return SC.colorLabel(e.color); }],
    ['space', 'Không gian', 'Chung', function(d, e, sp){ return sp === 'model' ? 'Model' : d.layouts[sp].name; }],
    ['x', 'Vị trí X', 'Hình học', function(d, e){ var p = e.p || e.c || e.p1; return p ? num2(p[0]) : ''; }],
    ['y', 'Vị trí Y', 'Hình học', function(d, e){ var p = e.p || e.c || e.p1; return p ? num2(p[1]) : ''; }],
    ['sx', 'Tỉ lệ X', 'Hình học', function(d, e){ return e.t === 'insert' ? num2(Math.abs(e.sx)) : ''; }],
    ['sy', 'Tỉ lệ Y', 'Hình học', function(d, e){ return e.t === 'insert' ? num2(Math.abs(e.sy)) : ''; }],
    ['rot', 'Góc xoay', 'Hình học', function(d, e){ return e.t === 'insert' || e.t === 'text' || e.t === 'mtext' ? num2(((e.rot || 0) % 360 + 360) % 360) : ''; }],
    ['len', 'Chiều dài', 'Hình học', function(d, e){ return num2(e.t === 'line' ? G.dist(e.p1, e.p2) : e.t === 'pline' ? G.polyLen(e.pts, e.closed) : e.t === 'circle' ? 2 * Math.PI * e.r : e.t === 'arc' ? e.r * ED.sweep(e.a0, e.a1) * Math.PI / 180 : null); }],
    ['area', 'Diện tích', 'Hình học', function(d, e){ return num2(e.t === 'pline' && e.closed ? G.polyArea(e.pts) : e.t === 'circle' ? Math.PI * e.r * e.r : null); }],
    ['txt', 'Nội dung', 'Chữ', function(d, e){ return e.t === 'text' || e.t === 'mtext' ? e.s : ''; }],
    ['th', 'Chiều cao chữ', 'Chữ', function(d, e){ return e.t === 'text' || e.t === 'mtext' ? num2(e.h) : ''; }],
    ['desc', 'Mô tả block', 'Block', function(d, e){ return e.t === 'insert' && d.blocks[e.name] ? d.blocks[e.name].desc || '' : ''; }]
  ];
  function dxObjs(d){
    var out = [];
    if (DX.src === 'sel') (DX.ids || []).forEach(function(id){ var e = d.byId[id]; if (e) out.push({ e:e, sp:e._paper ? d.space : 'model' }); });
    else { d.ents.forEach(function(e){ out.push({ e:e, sp:'model' }); }); d.layouts.forEach(function(L, i){ L.ents.forEach(function(e){ out.push({ e:e, sp:i }); }); }); }
    return out.filter(function(o){ return o.e.t !== 'vpframe'; });
  }
  function dxKey(e){ return e.t === 'insert' ? 'B:' + e.name : 'T:' + e.t; }
  function dxTypes(d){
    var m = {};
    dxObjs(d).forEach(function(o){ var k = dxKey(o.e), blk = o.e.t === 'insert'; (m[k] = m[k] || { k:k, name:blk ? o.e.name : TC.TYPE_NAMES[o.e.t] || o.e.t, n:0, blk:blk, att:blk && SC.att.hasAtt(d, o.e.name) }).n++; });
    return Object.keys(m).map(function(k){ return m[k]; }).filter(function(t){ return (DX.kind === 'all' || (DX.kind === 'block') === t.blk) && (!DX.att || t.att); }).sort(function(a, b){ return (b.blk - a.blk) || a.name.localeCompare(b.name); });
  }
  function dxIsOn(k){ return DX.on ? !!DX.on[k] : true; }
  function dxSel(d){ var ok = {}; dxTypes(d).forEach(function(t){ if (dxIsOn(t.k)) ok[t.k] = 1; }); return dxObjs(d).filter(function(o){ return ok[dxKey(o.e)]; }); }
  function dxProps(d, objs){
    var tags = [];
    objs.forEach(function(o){ if (o.e.t === 'insert' && d.blocks[o.e.name]) d.blocks[o.e.name].ents.forEach(function(a){ if (a.t === 'attdef' && tags.indexOf(a.tag) < 0) tags.push(a.tag); }); });
    return DXP.concat(tags.map(function(t){ return ['a:' + t, t, 'Attribute', function(d2, e){ return e.t === 'insert' ? SC.att.attrsOf(d2, e)[t] || '' : ''; }]; }))
      .filter(function(p){ return objs.some(function(o){ var v = p[3](d, o.e, o.sp); return v !== '' && v != null; }); });
  }
  function dxLab(p){ return DX.names[p[0]] || p[1]; }
  // lưới dữ liệu: cột đã chọn, gộp dòng giống nhau (cột Số lượng), lọc, sắp xếp
  function dxGrid(d){
    var objs = dxSel(d), cols = dxProps(d, objs).filter(function(p){ return DX.props[p[0]]; }), m = {}, rows = [];
    objs.forEach(function(o){ var v = cols.map(function(p){ return p[3](d, o.e, o.sp); }), k = v.join('\u0001'); if (DX.comb && m[k]){ m[k].n++; return; } var r = { v:v, n:1 }; if (DX.comb) m[k] = r; rows.push(r); });
    var head = cols.map(function(p){ return { k:p[0], lab:dxLab(p) }; }).concat(DX.cnt ? [{ k:'#n', lab:'Số lượng', num:true }] : []);
    rows.forEach(function(r){ r.cells = r.v.concat(DX.cnt ? [String(r.n)] : []); });
    if (DX.q){ var q = TC.fold(DX.q); rows = rows.filter(function(r){ return r.cells.some(function(c){ return TC.fold(c).indexOf(q) >= 0; }); }); }
    var si = head.map(function(h){ return h.k; }).indexOf(DX.sort); if (si < 0) si = 0;
    var cmp = function(x, y){ return String(x).localeCompare(String(y), 'vi', { numeric:true }); };
    rows.sort(function(a, b){ return DX.dir * cmp(a.cells[si], b.cells[si]) || a.cells.reduce(function(r, c, i){ return r || (i === si ? 0 : cmp(c, b.cells[i])); }, 0); });
    head.forEach(function(h, i){ h.num = h.num || (rows.length && rows.every(function(r){ return r.cells[i] === '' || !isNaN(Number(r.cells[i])); })); });
    return { objs:objs, head:head, rows:rows, si:si };
  }
  function dxTable(g, p){
    var th = 250, nc = g.head.length, W = g.head.map(function(h, i){ var L = Math.max(h.lab.length * th * 1.1 * .7, g.rows.reduce(function(m, r){ return Math.max(m, String(r.cells[i]).length); }, 0) * th * .62); return Math.round(TC.clamp(L + th * 1.6, 1000, 7000) / 100) * 100; });
    return { t:'table', p:p, cols:W, rows:[900, 750].concat(g.rows.map(function(){ return 650; })), th:th, head:2, cells:[[DX.title].concat(g.head.slice(1).map(function(){ return ''; })), g.head.map(function(h){ return h.lab; })].concat(g.rows.map(function(r){ return r.cells; })), merge:nc > 1 ? [[0, 0, nc - 1]] : [], tstyle:'TCVN-thong-ke', style:'Standard' };
  }
  function dxPath(d){ return dirOf(d.path) + '\\' + fileBase(DX.title) + '_' + SC.att.ascii(SC.att.base(d.name)) + (DX.fmt === 'XLSX' ? '.xlsx' : '.csv'); }
  IMPL.DATAEXTRACTION = function(ctx, pre){
    var d = D();
    if (d.sel.length){ DX.src = 'sel'; DX.ids = d.sel.slice(); } else if (DX.src === 'sel' && !(DX.ids || []).length) DX.src = 'doc';
    DX.step = typeof pre === 'number' ? pre : 1;
    return new Promise(function(res){
      var dlg = TC.dialog({ id:'dx', title:'Trích xuất dữ liệu', cmd:'DATAEXTRACTION', code:'UI-I06', w:900, h:600, flush:true, body:'<div class="wz"><ol class="wz-steps" id="dx-s" aria-label="Các bước"></ol><div class="wz-body" id="dx-b"></div></div>', foot:'<span></span>', onClose:function(r){ if (r !== 'pick' && r !== 'run') res(); } });
      var $d = function(s){ return $(s, dlg.el); }, G0;
      function card(type, name, val, on, title, desc, inner, dis){ return '<div class="wz-card' + (on ? ' on' : '') + (dis ? ' is-dim' : '') + '"><label class="chk' + (dis ? ' dis' : '') + '"><input type="' + type + '" name="' + name + '" value="' + val + '"' + (on ? ' checked' : '') + (dis ? ' disabled' : '') + '><b>' + title + '</b></label><span class="faint">' + desc + '</span>' + (on && inner ? '<div class="wz-in">' + inner + '</div>' : '') + '</div>'; }
      function s1(){
        var all = dxObjs(d), c = { b:0, t:0, g:0 }; all.forEach(function(o){ var t = o.e.t; if (t === 'insert') c.b++; else if (t === 'text' || t === 'mtext' || t === 'mleader') c.t++; else c.g++; });
        return '<div class="wz-cols"><div class="stack"><span class="gh">Lấy dữ liệu từ</span>' +
          card('radio', 'dx-src', 'doc', DX.src === 'doc', 'Bản vẽ hiện hành', esc(d.name) + ': Model và ' + d.layouts.length + ' layout.') +
          card('radio', 'dx-src', 'sel', DX.src === 'sel', 'Đối tượng chọn trên bản vẽ', 'Chỉ lấy các đối tượng đã chọn, ví dụ một tầng hoặc một khu.', '<span class="hrow"><button class="btn sm" type="button" data-dx="pick">' + ic('select') + 'Chọn trên bản vẽ &lt;</button><span class="faint" style="font-size:var(--fs-s)">' + ((DX.ids || []).length ? 'Đã chọn ' + TC.n(DX.ids.length) + ' đối tượng' : 'Chưa chọn') + '</span></span>') +
          card('radio', 'dx-src', 'many', false, 'Nhiều bản vẽ, bộ bản vẽ <span class="tag">Sau v1.0</span>', 'Gộp dữ liệu của nhiều tệp DWG vào một bảng.', '', true) +
          '</div><div class="stack"><span class="gh">Tìm thấy</span><div class="wz-found"><b class="mono">' + TC.n(all.length) + '</b><span>đối tượng</span></div><div class="stack" style="gap:3px;font-size:var(--fs-s)"><span class="hrow" style="gap:6px">' + ic('insert', 's12') + TC.n(c.b) + ' block</span><span class="hrow" style="gap:6px">' + ic('mtext', 's12') + TC.n(c.t) + ' chữ, ghi chú</span><span class="hrow" style="gap:6px">' + ic('pline', 's12') + TC.n(c.g) + ' đối tượng hình học</span></div>' +
          (DX.src === 'sel' && !(DX.ids || []).length ? '<div class="note warn">' + ic('warn') + '<span>Chưa chọn đối tượng nào. Bấm Chọn trên bản vẽ, hoặc lấy cả bản vẽ.</span></div>' : '') + '</div></div>';
      }
      function s2(){
        var T = dxTypes(d), q = TC.fold(DX.tq), list = T.filter(function(t){ return !q || TC.fold(t.name).indexOf(q) >= 0; }), on = T.filter(function(t){ return dxIsOn(t.k); });
        return '<div class="stack"><div class="hrow">' + SC.seg('dxkind', ['Chỉ block', 'Không phải block', 'Tất cả'], { block:'Chỉ block', geo:'Không phải block', all:'Tất cả' }[DX.kind]) + '<label class="chk"><input type="checkbox" id="dx-att"' + (DX.att ? ' checked' : '') + '> Chỉ block có attribute</label><span class="grow"></span><label class="search" style="width:220px">' + ic('search') + '<input id="dx-tq" value="' + esc(DX.tq) + '" placeholder="Tìm loại đối tượng" aria-label="Tìm loại đối tượng"></label></div>' +
          (T.length ? '<div class="tblwrap dx-list"><table class="tbl"><colgroup><col style="width:30px"><col><col style="width:170px"><col style="width:90px"></colgroup><thead><tr><th></th><th>Đối tượng</th><th>Loại</th><th class="num">Số lượng</th></tr></thead><tbody>' +
            list.map(function(t){ return '<tr><td class="ctr"><input type="checkbox" data-dxon="' + esc(t.k) + '"' + (dxIsOn(t.k) ? ' checked' : '') + ' aria-label="Lấy ' + esc(t.name) + '"></td><td>' + ic(t.blk ? 'insert' : 'pline', 's12') + ' ' + esc(t.name) + '</td><td class="faint">' + (t.blk ? (t.att ? 'Block có attribute' : 'Block') : 'Đối tượng hình học, chữ') + '</td><td class="num">' + TC.n(t.n) + '</td></tr>'; }).join('') + (list.length ? '' : '<tr><td colspan="4" class="faint">Không có loại nào khớp “' + esc(DX.tq) + '”.</td></tr>') + '</tbody></table></div>'
            : '<div class="empty">' + ic('filter') + '<b>Không có đối tượng phù hợp</b><span>Nguồn đã chọn không có ' + (DX.kind === 'block' ? 'block' + (DX.att ? ' có attribute' : '') : 'đối tượng') + ' nào. Đổi bộ lọc hoặc quay lại bước Nguồn.</span></div>') +
          '<div class="hrow"><button class="btn sm ghost" type="button" data-dx="tall">Chọn tất cả</button><button class="btn sm ghost" type="button" data-dx="tnone">Bỏ chọn</button><span class="grow"></span><span class="faint" style="font-size:var(--fs-s)">' + on.length + ' loại · ' + TC.n(on.reduce(function(s, t){ return s + t.n; }, 0)) + ' đối tượng sẽ trích xuất</span></div></div>';
      }
      function s3(){
        var objs = dxSel(d), P = dxProps(d, objs), cats = ['Attribute', 'Block', 'Chung', 'Hình học', 'Chữ'].filter(function(c){ return P.some(function(p){ return p[2] === c; }); }), q = TC.fold(DX.pq);
        var vis = P.filter(function(p){ return DX.cats[p[2]] !== false && (!q || TC.fold(p[1] + ' ' + dxLab(p)).indexOf(q) >= 0); });
        return '<div class="dx-props"><div class="stack"><span class="gh">Nhóm thuộc tính</span>' + cats.map(function(c){ return '<label class="chk"><input type="checkbox" data-dxcat="' + c + '"' + (DX.cats[c] !== false ? ' checked' : '') + '> ' + c + ' <span class="faint">' + P.filter(function(p){ return p[2] === c; }).length + '</span></label>'; }).join('') +
          '<p class="faint" style="font-size:var(--fs-xs);margin:0">Chỉ hiện thuộc tính có giá trị ở ít nhất một đối tượng đã chọn.</p></div>' +
          '<div class="stack" style="min-height:0"><div class="hrow"><span class="faint" style="font-size:var(--fs-s)">' + P.filter(function(p){ return DX.props[p[0]]; }).length + ' thuộc tính sẽ thành cột · sửa Tên cột để đặt tiêu đề trong bảng</span><span class="grow"></span><label class="search" style="width:200px">' + ic('search') + '<input id="dx-pq" value="' + esc(DX.pq) + '" placeholder="Tìm thuộc tính" aria-label="Tìm thuộc tính"></label></div>' +
          '<div class="tblwrap dx-list"><table class="tbl"><colgroup><col style="width:30px"><col style="width:150px"><col style="width:180px"><col style="width:90px"><col></colgroup><thead><tr><th></th><th>Thuộc tính</th><th>Tên cột</th><th>Nhóm</th><th>Ví dụ</th></tr></thead><tbody>' +
            vis.map(function(p){ var ex = ''; objs.some(function(o){ ex = p[3](d, o.e, o.sp); return ex !== ''; }); return '<tr><td class="ctr"><input type="checkbox" data-dxp="' + esc(p[0]) + '"' + (DX.props[p[0]] ? ' checked' : '') + ' aria-label="Thêm cột ' + esc(p[1]) + '"></td><td>' + (p[2] === 'Attribute' ? '<span class="mono">' + esc(p[1]) + '</span>' : esc(p[1])) + '</td><td><input class="inp sm" style="width:100%" data-dxn="' + esc(p[0]) + '" value="' + esc(dxLab(p)) + '" aria-label="Tên cột cho ' + esc(p[1]) + '"' + (DX.props[p[0]] ? '' : ' disabled') + '></td><td class="faint">' + p[2] + '</td><td class="faint" title="' + esc(ex) + '">' + esc(ex) + '</td></tr>'; }).join('') + (vis.length ? '' : '<tr><td colspan="5" class="faint">Không có thuộc tính nào khớp.</td></tr>') + '</tbody></table></div></div></div>';
      }
      function s4(){
        var g = G0 = dxGrid(d);
        return '<div class="stack"><div class="hrow"><label class="chk"><input type="checkbox" id="dx-comb"' + (DX.comb ? ' checked' : '') + '> Gộp các dòng giống nhau</label><label class="chk"><input type="checkbox" id="dx-cnt"' + (DX.cnt ? ' checked' : '') + '> Cột số lượng</label><label class="chk"><input type="checkbox" id="dx-nm"' + (DX.props.name ? ' checked' : '') + '> Cột tên</label><span class="grow"></span><label class="search" style="width:220px">' + ic('filter') + '<input id="dx-q" value="' + esc(DX.q) + '" placeholder="Lọc dòng" aria-label="Lọc dòng"></label></div>' +
          (g.head.length ? '<div class="tblwrap dx-grid"><table class="tbl" style="width:auto;min-width:100%"><thead><tr>' + g.head.map(function(h, i){ return sortTh('data-dxs', h.k, h.lab, g.head[g.si].k, DX.dir, h.num ? 'num' : ''); }).join('') + '</tr></thead><tbody>' +
            g.rows.map(function(r){ return '<tr>' + r.cells.map(function(c, i){ return '<td' + (g.head[i].num ? ' class="num"' : '') + '>' + esc(c) + '</td>'; }).join('') + '</tr>'; }).join('') + (g.rows.length ? '' : '<tr><td colspan="' + g.head.length + '" class="faint">Không có dòng nào khớp bộ lọc.</td></tr>') + '</tbody></table></div>'
            : '<div class="empty">' + ic('table') + '<b>Chưa có cột nào</b><span>Chọn ít nhất một thuộc tính ở bước Thuộc tính, hoặc bật Cột số lượng.</span></div>') +
          '<div class="hrow faint" style="font-size:var(--fs-s)"><span>' + g.rows.length + ' dòng × ' + g.head.length + ' cột · từ ' + TC.n(g.objs.length) + ' đối tượng</span><span class="grow"></span><span>Bấm tiêu đề cột để sắp xếp, bấm lại để đảo chiều.</span></div></div>';
      }
      function s5(){
        var g = G0 = dxGrid(d);
        return '<div class="wz-cols"><div class="stack"><span class="gh">Đầu ra</span>' +
          card('checkbox', 'dx-out', 'table', DX.table, 'Chèn bảng dữ liệu vào bản vẽ', 'Sau khi bấm nút, chỉ điểm chèn trên bản vẽ (xem trước theo con trỏ).', '<div class="frow" style="--lab-w:84px"><label class="lab" for="dx-title">Tiêu đề bảng</label><input class="inp" id="dx-title" value="' + esc(DX.title) + '"><span class="lab">Kiểu bảng</span><span class="hrow" style="gap:6px"><span class="mono">TCVN-thong-ke</span><span class="faint" style="font-size:var(--fs-xs)">chọn kiểu khác ở bước 6, sau v1.0</span></span></div>') +
          card('checkbox', 'dx-out', 'file', DX.file, 'Xuất dữ liệu ra tệp', 'Mở bằng Excel hoặc phần mềm dự toán.', '<div class="hrow">' + SC.seg('dxfmt', ['XLSX', 'CSV'], DX.fmt) + '</div><span class="hrow" style="flex-wrap:nowrap"><input class="inp grow mono" id="dx-path" value="' + esc(dxPath(d)) + '" style="font-size:var(--fs-xs)" aria-label="Tệp xuất"><button class="btn" type="button" data-dx="browse">Duyệt…</button></span><label class="chk"><input type="checkbox" id="dx-open"' + (DX.open ? ' checked' : '') + '> Mở tệp sau khi xuất</label>') +
          '<div class="note">' + ic('info') + '<span>Chọn kiểu bảng (bước 6), lưu thiết lập thành tệp .dxe để chạy lại (bước 7) và trích xuất từ nhiều bản vẽ có sau v1.0.</span></div></div>' +
          '<div class="stack"><span class="gh">Kết quả</span><div class="wz-found"><b class="mono">' + g.rows.length + '</b><span>dòng × ' + g.head.length + ' cột</span></div><div class="names">' + g.head.map(function(h){ return '<span>' + esc(h.lab) + '</span>'; }).join('') + '</div><span class="faint" style="font-size:var(--fs-s)">Từ ' + TC.n(g.objs.length) + ' đối tượng của ' + esc(d.name) + (DX.src === 'sel' ? ' (vùng chọn)' : '') + '.</span></div></div>';
      }
      function maxStep(){ var objs = dxSel(d); return !dxObjs(d).length ? 1 : !objs.length ? 2 : 5; }
      function render(){
        if (!document.contains(dlg.el)) return;
        var mx = maxStep(); if (DX.step > mx) DX.step = mx;
        $d('#dx-s').innerHTML = DX_STEPS.map(function(s, i){ var n = i + 1, st = n < DX.step ? 'done' : n === DX.step ? 'cur' : '', later = n > 5; return '<li><button type="button" class="wz-s ' + st + '" data-step="' + n + '"' + (n > mx ? ' disabled' : '') + (n === DX.step ? ' aria-current="step"' : '') + (later ? ' data-tip="' + s + '" data-desc="Có sau v1.0."' : '') + '><span class="wz-n">' + (st === 'done' ? ic('check', 's12') : n) + '</span>' + s + (later ? '<span class="tag">Sau v1.0</span>' : '') + '</button></li>'; }).join('');
        $d('#dx-b').innerHTML = [s1, s2, s3, s4, s5][DX.step - 1]();
        var f = $('.dlg-f', dlg.el), out = DX.table && DX.file ? 'Chèn bảng và xuất ' + DX.fmt : DX.table ? 'Chèn bảng' : DX.file ? 'Xuất ' + DX.fmt : 'Chọn đầu ra';
        f.innerHTML = (DX.step > 1 ? '<button class="btn" type="button" data-dx="back">' + ic('left', 'xs') + 'Quay lại</button>' : '') + '<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button>' +
          (DX.step < 5 ? '<button class="btn primary" type="button" data-dx="next"' + (DX.step < mx ? '' : ' disabled') + '>Tiếp ' + ic('right', 'xs') + '</button>' : '<button class="btn primary" type="button" data-dx="run"' + ((DX.table || DX.file) && G0 && G0.rows.length ? '' : ' disabled') + '>' + ic(DX.table ? 'table' : 'stats') + out + '</button>');
      }
      function go(){ // chèn bảng (chỉ điểm trên bản vẽ) rồi xuất tệp
        var g = dxGrid(d), path = DX.path || dxPath(d), name = path.split('\\').pop();
        dlg.close('run');
        var exp = function(){ if (DX.file) TC.toast('Đã xuất <span class="mono">' + esc(name) + '</span>: ' + g.rows.length + ' dòng × ' + g.head.length + ' cột.' + (DX.open ? ' Đang mở bằng Excel.' : ''), { act:{ label:DX.open ? 'Mở thư mục' : 'Mở tệp', fn:function(){} }, kind:'task' }); res(); };
        if (!DX.table){ ctx.out('Đã trích xuất ' + g.rows.length + ' dòng ra ' + esc(name) + '.'); exp(); return; }
        ctx.point('Điểm chèn bảng (góc trên trái)', { onMove:function(p, temp){ temp.push(ED.temp([dxTable(g, p)], { real:true })); } }).then(function(r){ if (r.p){ ED.run('DATAEXTRACTION', function(){ ED.add(ED.make(dxTable(g, r.p))); }); ctx.out('Đã chèn bảng ' + esc(DX.title) + ': ' + g.rows.length + ' dòng × ' + g.head.length + ' cột, từ ' + TC.n(g.objs.length) + ' đối tượng.'); } exp(); }, function(){ res(); });
      }
      dlg.el.addEventListener('click', function(e){
        var st = e.target.closest('[data-step]'); if (st && !st.disabled){ if (Number(st.dataset.step) <= 5){ DX.step = Number(st.dataset.step); render(); } return; }
        var sk = e.target.closest('[data-seg="dxkind"] button'); if (sk){ DX.kind = { 'Chỉ block':'block', 'Không phải block':'geo', 'Tất cả':'all' }[sk.dataset.v]; render(); return; }
        var sf = e.target.closest('[data-seg="dxfmt"] button'); if (sf){ DX.fmt = sf.dataset.v; DX.path = ''; render(); return; }
        var sh = e.target.closest('[data-dxs]'); if (sh){ var k = sh.dataset.dxs; DX.dir = G0 && G0.head[G0.si].k === k ? -DX.dir : 1; DX.sort = k; render(); var f2 = $('[data-dxs="' + k + '"]', dlg.el); if (f2) f2.focus(); return; }
        var b = e.target.closest('[data-dx]'); if (!b || b.disabled) return; var k2 = b.dataset.dx;
        if (k2 === 'next'){ DX.step++; render(); }
        else if (k2 === 'back'){ DX.step--; render(); }
        else if (k2 === 'run') go();
        else if (k2 === 'tall' || k2 === 'tnone'){ DX.on = {}; dxTypes(d).forEach(function(t){ DX.on[t.k] = k2 === 'tall'; }); render(); }
        else if (k2 === 'browse') TC.toast('Hộp thoại Lưu thành của Windows.', { type:'info', log:false });
        else if (k2 === 'pick'){ // hộp thoại modal đang ẩn vẫn giữ phím: Enter xong chọn, Esc quay lại không đổi
          var keyF = function(ev){ if (ev.key !== 'Enter' && ev.key !== 'Escape') return; ev.preventDefault(); ev.stopPropagation(); if (ev.key === 'Escape') TC.cad.clearSel(); TC.cmdline.submit(''); };
          dlg.hide(true); TC.cad.clearSel(); document.addEventListener('keydown', keyF, true);
          ctx.sel('Chọn đối tượng cần trích xuất').then(function(ids){ document.removeEventListener('keydown', keyF, true); if (ids.length){ DX.ids = ids; DX.src = 'sel'; } TC.cad.clearSel(); dlg.hide(false); render(); }, function(){ document.removeEventListener('keydown', keyF, true); dlg.close(); res(); }); }
      });
      dlg.el.addEventListener('change', function(e){
        var t = e.target;
        if (t.name === 'dx-src'){ DX.src = t.value; render(); }
        else if (t.name === 'dx-out'){ DX[t.value] = t.checked; render(); }
        else if (t.dataset.dxon != null){ if (!DX.on){ DX.on = {}; dxTypes(d).forEach(function(x){ DX.on[x.k] = true; }); } DX.on[t.dataset.dxon] = t.checked; render(); }
        else if (t.dataset.dxcat){ DX.cats[t.dataset.dxcat] = t.checked; render(); }
        else if (t.dataset.dxp){ DX.props[t.dataset.dxp] = t.checked; render(); }
        else if (t.dataset.dxn){ DX.names[t.dataset.dxn] = t.value.trim(); }
        else if (t.id === 'dx-att'){ DX.att = t.checked; render(); }
        else if (t.id === 'dx-comb' || t.id === 'dx-cnt'){ DX[t.id === 'dx-comb' ? 'comb' : 'cnt'] = t.checked; render(); }
        else if (t.id === 'dx-nm'){ DX.props.name = t.checked; render(); }
        else if (t.id === 'dx-title'){ DX.title = t.value.trim() || 'BẢNG THỐNG KÊ'; DX.path = ''; render(); }
        else if (t.id === 'dx-path') DX.path = t.value;
        else if (t.id === 'dx-open') DX.open = t.checked;
      });
      // ô tìm, lọc: lọc ngay khi gõ, giữ con trỏ trong ô
      dlg.el.addEventListener('input', function(e){ var t = e.target, k = { 'dx-tq':'tq', 'dx-pq':'pq', 'dx-q':'q' }[t.id]; if (!k) return; DX[k] = t.value; var pos = t.selectionStart; render(); var n = $('#' + t.id, dlg.el); n.focus(); n.setSelectionRange(pos, pos); });
      DX.path = ''; render();
    });
  };

  /* ================= Nhập attribute từ Excel UI-J15 (TCATTIN) ================= */
  // tệp mô phỏng: bảng đã xuất bằng TCATTOUT rồi sửa trong Excel. So với giá trị hiện tại nên nhập lần hai sẽ “không đổi”.
  var AI = { file:null, filt:'all', q:'' };
  function aiFiles(d){ var b = 'Khung-ten_' + SC.att.ascii(SC.att.base(d.name)); return [{ n:b + '.xlsx', dir:dirOf(d.path), t:'Sửa lúc 16:20 hôm nay', sheet:'Attribute' }, { n:b + '.csv', dir:dirOf(d.path), t:'Sửa lúc 09:05 hôm qua' }, { n:'Bang-khung-ten-cu.xlsx', dir:'D:\\DuAn\\Luu-tru', t:'Sửa ngày 12/09/2026', sheet:'Sheet1', bad:true }]; }
  function aiRead(d){
    var A2 = SC.att, rows = A2.inst(d, 'all').map(function(it, i){ var a = A2.attrsOf(d, it.e), v = Object.assign({}, a); v.NGAY = '25/10/2026'; if (/MẶT BẰNG/.test(a.TENBV || '')) v.THIETKE = 'L.T.CHI'; if (a.KYHIEU === 'KT-03') v.TENBV = 'MẶT CẮT 1-1 (TRỤC 1-4)'; v.GHICHU = i ? '' : 'Sửa theo góp ý chủ đầu tư'; return { n:i + 2, h:A2.handle(it.e), it:it, v:v }; });
    if (rows.length) rows.push({ n:rows.length + 2, h:'3A1', it:null, blk:'KHUNG-A3', lay:'Model', v:{ TENBV:'MẶT BẰNG MÁI', KYHIEU:'KT-06', NGAY:'25/10/2026' } });
    var ch = [], miss = [], same = 0, bad = {};
    rows.forEach(function(r){
      if (!r.it){ miss.push(r); return; }
      var a = A2.attrsOf(d, r.it.e);
      Object.keys(r.v).forEach(function(t){ if (!(t in a)){ (bad[t] = bad[t] || []).indexOf(r.it.e.name) < 0 && bad[t].push(r.it.e.name); return; } if (String(a[t]) === String(r.v[t])) same++; else ch.push({ r:r, tag:t, old:a[t], nw:r.v[t], on:true }); });
    });
    return { rows:rows, ch:ch, miss:miss, same:same, bad:Object.keys(bad).map(function(t){ return { tag:t, blks:bad[t] }; }) };
  }
  SC.attIn = function(){
    var d = D(); if (!d) return;
    var files = aiFiles(d), F = files.filter(function(f){ return f.n === (AI.file || SC.attLast); })[0] || null, R = null;
    AI.q = ''; AI.filt = 'all';
    function load(){ R = F && !F.bad ? aiRead(d) : null; }
    var dlg = TC.dialog({ id:'attin', title:'Nhập attribute từ Excel', cmd:'TCATTIN', code:'UI-J15', w:960, body:SC.attTabs('in') + '<div class="stack" id="ai-b"></div>', foot:'<span></span>' });
    function render(){
      var none = !SC.att.inst(d, 'all').length, on = R ? R.ch.filter(function(c){ return c.on; }).length : 0, warnN = R ? R.miss.length + R.bad.length : 0, q = TC.fold(AI.q);
      var hit = function(c){ return !q || TC.fold([c.tag, c.old, c.nw, c.r.h, c.r.it.e.name].join(' ')).indexOf(q) >= 0; };
      $('#ai-b', dlg.el).innerHTML =
        (F ? '<div class="ai-file">' + ic('stats', 'md') + '<span class="grow"><b class="mono">' + esc(F.n) + '</b><span class="faint mono">' + esc(F.dir) + '</span></span><span class="faint" style="font-size:var(--fs-s)">' + esc(F.t) + (F.sheet ? ' · sheet ' + esc(F.sheet) : '') + (R ? ' · ' + R.rows.length + ' dòng' : '') + '</span><button class="btn sm" type="button" data-ai="file" aria-haspopup="menu">' + ic('open') + 'Chọn tệp khác' + ic('down', 'xs') + '</button></div>' : '') +
        (none ? '<div class="empty">' + ic('insert') + '<b>Bản vẽ không có block có attribute</b><span>Nhập attribute cần khung tên hoặc block có attribute trong ' + esc(d.name) + '. Mở bản vẽ có khung tên rồi thử lại.</span></div>'
          : !F ? '<div class="empty">' + ic('open') + '<b>Chọn tệp Excel đã xuất từ bản vẽ này</b><span>Tệp XLSX hoặc CSV tạo bằng Xuất ra Excel, có cột Handle (có thể ẩn) để khớp đúng đối tượng.</span><button class="btn" type="button" data-ai="file" aria-haspopup="menu">' + ic('open') + 'Chọn tệp…</button></div>'
          : F.bad ? '<div class="note err">' + ic('error') + '<span><b>Tệp không có cột Handle</b> nên không xác định được dòng nào ứng với khung tên nào. Xuất lại bằng tab Xuất ra Excel (cột Handle có thể ẩn), sửa trong Excel rồi nhập tệp đó.</span></div>'
          : '<div class="hrow"><span class="tag ok">' + R.ch.length + ' thay đổi</span>' + (R.miss.length ? '<span class="tag warn">' + R.miss.length + ' dòng không tìm thấy đối tượng</span>' : '') + (R.bad.length ? '<span class="tag warn">' + R.bad.length + ' tag không có trong block</span>' : '') + '<span class="tag">' + R.same + ' giá trị không đổi</span><span class="grow"></span>' + SC.seg('aiflt', ['Tất cả', 'Thay đổi', 'Cảnh báo'], { all:'Tất cả', ch:'Thay đổi', warn:'Cảnh báo' }[AI.filt]) + '<label class="search" style="width:190px">' + ic('search') + '<input id="ai-q" value="' + esc(AI.q) + '" placeholder="Tìm tag, giá trị" aria-label="Tìm tag, giá trị"></label></div>' +
            '<div class="tblwrap ai-tbl"><table class="tbl"><colgroup><col style="width:30px"><col style="width:50px"><col style="width:60px"><col style="width:92px"><col style="width:100px"><col style="width:84px"><col><col style="width:24px"><col></colgroup><thead><tr><th></th><th class="num">Dòng</th><th>Handle</th><th>Block</th><th>Layout</th><th>Tag</th><th>Giá trị cũ</th><th></th><th>Giá trị mới</th></tr></thead><tbody>' +
              (AI.filt !== 'warn' ? R.ch.map(function(c, i){ return hit(c) ? '<tr><td class="ctr"><input type="checkbox" data-aion="' + i + '"' + (c.on ? ' checked' : '') + ' aria-label="Áp dụng ' + esc(c.tag) + ' dòng ' + c.r.n + '"></td><td class="num">' + c.r.n + '</td><td class="mono">' + c.r.h + '</td><td>' + esc(c.r.it.e.name) + '</td><td>' + esc(SC.att.where(d, c.r.it.sp)) + '</td><td class="mono">' + esc(c.tag) + '</td><td class="faint ai-old" title="' + esc(c.old) + '">' + esc(c.old) + '</td><td class="faint ctr">' + ic('right', 'xs') + '</td><td title="' + esc(c.nw) + '"><b style="font-weight:500">' + esc(c.nw) + '</b></td></tr>' : ''; }).join('') : '') +
              (AI.filt !== 'ch' ? R.miss.map(function(r){ return '<tr class="ai-warn"><td class="ctr">' + ic('warn', 's12') + '</td><td class="num">' + r.n + '</td><td class="mono">' + r.h + '</td><td>' + esc(r.blk) + '</td><td>' + esc(r.lay) + '</td><td colspan="4">Không tìm thấy đối tượng (có thể đã bị xóa hoặc tệp của bản vẽ khác): bỏ qua dòng này.</td></tr>'; }).join('') +
                R.bad.map(function(b){ return '<tr class="ai-warn"><td class="ctr">' + ic('warn', 's12') + '</td><td></td><td colspan="2">Cột <span class="mono">' + esc(b.tag) + '</span></td><td colspan="5">Block ' + b.blks.map(esc).join(', ') + ' không có tag này: bỏ qua cả cột. Thêm tag bằng Quản lý attribute nếu cần.</td></tr>'; }).join('') : '') +
              (!R.ch.length && AI.filt === 'ch' ? '<tr><td colspan="9" class="faint">Tệp không có giá trị nào khác bản vẽ: mọi giá trị đã khớp.</td></tr>' : '') + '</tbody></table></div>' +
            '<p class="faint" style="font-size:var(--fs-xs);margin:0">So theo cột Handle. Cả lần nhập là một bước hoàn tác (Ctrl+Z). Bỏ chọn một dòng để giữ giá trị cũ.</p>');
      $('.dlg-f', dlg.el).innerHTML = (R && warnN ? '<span class="faint" style="font-size:var(--fs-s)">' + warnN + ' cảnh báo sẽ được bỏ qua</span>' : '') + '<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" data-ai="go"' + (on ? '' : ' disabled') + '>Áp dụng ' + on + ' thay đổi</button>';
    }
    function apply(){
      var by = {}; R.ch.forEach(function(c){ if (c.on) (by[c.r.it.e.id] = by[c.r.it.e.id] || { e:c.r.it.e, v:{} }).v[c.tag] = c.nw; });
      var ents = Object.keys(by), n = R.ch.filter(function(c){ return c.on; }).length;
      ED.run('TCATTIN', function(){ ents.forEach(function(id){ ED.mod(by[id].e, function(x){ x.attrs = Object.assign({}, x.attrs || {}, by[id].v); }); }); });
      dlg.close(true); V().draw();
      TC.cmdline.msg('TCATTIN: ' + n + ' giá trị attribute trên ' + ents.length + ' block đã cập nhật từ ' + esc(F.n) + '.');
      TC.toast('Đã cập nhật ' + n + ' giá trị attribute trên ' + ents.length + ' khung tên. Cả lần nhập là một bước hoàn tác.', { act:{ label:'Hoàn tác', fn:function(){ ED.undo(1); } }, log:false });
    }
    dlg.el.addEventListener('click', function(e){
      var s = e.target.closest('[data-seg="aiflt"] button'); if (s){ AI.filt = { 'Tất cả':'all', 'Thay đổi':'ch', 'Cảnh báo':'warn' }[s.dataset.v]; render(); return; }
      var b = e.target.closest('[data-ai]'); if (!b || b.disabled) return;
      if (b.dataset.ai === 'go') apply();
      else { var rc = b.getBoundingClientRect(); TC.menu([{ h:'Tệp gần đây' }].concat(files.map(function(f){ return { label:esc(f.n) + ' <span class="faint">· ' + esc(f.t) + '</span>', icon:'stats', act:function(){ F = f; AI.file = f.n; load(); render(); } }; }), [{ sep:1 }, { label:'Duyệt…', icon:'open', act:function(){ TC.toast('Hộp thoại mở tệp của Windows (XLSX, CSV).', { type:'info', log:false }); } }]), rc.left, rc.bottom + 2); }
    });
    dlg.el.addEventListener('change', function(e){ var c = R && R.ch[Number(e.target.dataset.aion)]; if (c){ c.on = e.target.checked; var n = R.ch.filter(function(x){ return x.on; }).length, g = $('[data-ai="go"]', dlg.el); g.textContent = 'Áp dụng ' + n + ' thay đổi'; g.disabled = !n; } });
    dlg.el.addEventListener('input', function(e){ if (e.target.id !== 'ai-q') return; AI.q = e.target.value; render(); var n = $('#ai-q', dlg.el); n.focus(); n.setSelectionRange(n.value.length, n.value.length); });
    load(); render(); dlg.el.style.top = Math.max(12, (innerHeight - dlg.el.offsetHeight) / 2.4) + 'px'; // canh lại sau khi có nội dung
  };

  /* ================= Excel UI-J12: phần v1.0 ở screens7.js (XLSX, ô gộp, định dạng số, mở tệp sau khi xuất) ================= */

  /* ================= Đóng gói gửi đi UI-B11 (ETRANSMIT) ================= */
  var TX_SETS = {
    'Standard':{ pkg:'zip', fmt:'Giữ nguyên', dir:'D:\\DuAn\\_Gui-di', name:'inc', path:'tree', noprn:false, bind:false, bindk:'Bind', purge:false, uni:true, fonts:true, dlink:true, unload:false, desc:'Gói ZIP, giữ phiên bản và cấu trúc thư mục; chuyển font sang Unicode để người nhận đọc đúng chữ.' },
    'Gửi chủ đầu tư · DWG 2013':{ pkg:'zip', fmt:'DWG 2013', dir:'D:\\DuAn\\_Gui-CDT', name:'inc', path:'flat', noprn:true, bind:true, bindk:'Bind', purge:true, uni:true, fonts:true, dlink:true, unload:false, desc:'Một thư mục phẳng, DWG 2013, bind xref, dọn dẹp, máy in về “Không”.' } };
  var TX = SC.TX = { set:'Standard', tab:'tree', extra:[], off:{}, col:{}, note:'' };
  var TX_EXTRA = [['Ghi-chu-gui-doi-tac.pdf', 'D:\\DuAn\\NhaO-2T', '312 KB'], ['Thong-ke-cua.xlsx', 'D:\\DuAn\\NhaO-2T\\KienTruc', '18 KB']];
  var TX_G = [['dwg', 'Bản vẽ', 'file'], ['xref', 'Tham chiếu ngoài', 'xref'], ['img', 'Ảnh', 'image'], ['font', 'Font', 'mtext'], ['ctb', 'Plot style', 'plotstyle'], ['pc3', 'Máy in (PC3)', 'print'], ['extra', 'Tệp thêm', 'plus'], ['rep', 'Báo cáo', 'list']];
  function kb(s){ var m = String(s).match(/([\d,]+)\s*(KB|MB)/); return m ? Number(m[1].replace(',', '.')) * (m[2] === 'MB' ? 1024 : 1) : 0; }
  function txFiles(d){
    var S = TX_SETS[TX.set], F = [], add = function(o){ o.inc = !o.miss && !o.skip && !TX.off[o.id]; F.push(o); return o; };
    add({ id:'dwg', g:'dwg', name:d.name, dir:dirOf(d.path), size:TC.size(d.bytes), type:'Bản vẽ · ' + (S.fmt === 'Giữ nguyên' ? d.ver : d.ver + ' → ' + S.fmt), fixed:true, st:S.uni || S.purge || S.bind || S.noprn ? 'Sửa trong bản sao' : '' });
    d.xrefs.forEach(function(x, i){ var miss = x.status === 'Không tìm thấy', p = x.found || x.path; add({ id:'x' + i, g:'xref', name:p.split('\\').pop(), dir:dirOf(p), size:x.size || '—', type:'Xref ' + (x.type === 'Overlay' ? 'overlay' : 'attach'), miss:miss, skip:!miss && S.bind, st:miss ? 'Không tìm thấy' : S.bind ? S.bindk + ' vào bản vẽ' : '' }); });
    d.images.forEach(function(x, i){ var miss = x.status === 'Không tìm thấy'; add({ id:'i' + i, g:'img', name:x.name, dir:dirOf(x.path), size:x.size || '—', type:'Ảnh raster', miss:miss, st:miss ? 'Không tìm thấy' : '' }); });
    var FN = { VNTIME:['vntime.shx', 'TrionVN-Times.shx'], VNTIMEH:['VNTIMEH.TTF', 'TrionVN-Times.ttf'], VNI:['VNI-Times.ttf', 'TrionVN-Arial.ttf'], ROMANS:['romans.shx'] }, uni = {};
    d.styles.filter(function(s){ return s.count; }).forEach(function(s){
      var legacy = s.enc === 'TCVN3' || s.enc === 'VNI', fn = (FN[s.name] || [s.font])[0], sys = s.kind === 'TTF' && !legacy;
      add({ id:'f' + s.name, g:'font', name:sys ? s.font.toLowerCase().replace(/\s+/g, '') + '.ttf' : fn, dir:sys ? 'C:\\Windows\\Fonts' : 'C:\\TrionCAD\\Fonts', size:s.kind === 'SHX' && !s.missing ? '24 KB' : '—', type:(s.kind === 'SHX' ? 'Font SHX' : 'Font TrueType') + (legacy ? ' · ' + s.enc : ''), miss:s.missing && !(S.uni && legacy), skip:!S.fonts || sys || (S.uni && legacy),
        st:sys ? 'Font hệ thống, máy nhận đã có' : S.uni && legacy ? 'Không cần: chữ chuyển sang Unicode' : s.missing ? 'Không có trên máy này' : !S.fonts ? 'Không kèm font (thiết lập)' : '' });
      if (S.uni && legacy && FN[s.name] && FN[s.name][1]) uni[FN[s.name][1]] = s.kind;
    });
    Object.keys(uni).forEach(function(n){ add({ id:'u' + n, g:'font', name:n, dir:'C:\\TrionCAD\\Fonts', size:uni[n] === 'SHX' ? '41 KB' : '386 KB', type:(uni[n] === 'SHX' ? 'Font SHX tiếng Việt' : 'Font TrueType tiếng Việt') + ' · Unicode', skip:!S.fonts, st:S.fonts ? 'Font miễn phí của TrionCAD, gửi kèm được' : 'Không kèm font (thiết lập)' }); });
    var ctb = {}, dev = {}; d.pageSetups.forEach(function(p){ if (p.ctb) ctb[p.ctb] = 1; if (p.device && !/TrionCAD|Microsoft/.test(p.device)) dev[p.device] = 1; });
    Object.keys(ctb).forEach(function(n){ add({ id:'c' + n, g:'ctb', name:n, dir:'C:\\TrionCAD\\Plot Styles', size:'5 KB', type:'Plot style (CTB)' }); });
    Object.keys(dev).forEach(function(n){ add({ id:'p' + n, g:'pc3', name:SC.att.ascii(n.replace(/[()]/g, '')) + '.pc3', dir:'C:\\TrionCAD\\Plotters', size:'2 KB', type:'Cấu hình máy in', skip:S.noprn, st:S.noprn ? 'Không cần: máy in đặt về “Không”' : '' }); });
    TX.extra.forEach(function(x, i){ add({ id:'e' + i, g:'extra', name:x[0], dir:x[1], size:x[2], type:'Tệp thêm', rm:i }); });
    add({ id:'rep', g:'rep', name:SC.att.base(d.name) + ' - Bao-cao-dong-goi.txt', dir:'(tạo khi đóng gói)', size:'2 KB', type:'Báo cáo đóng gói', fixed:true });
    return F;
  }
  function txPkg(d){ var S = TX_SETS[TX.set]; return SC.att.base(d.name) + ' - ' + SC.att.ascii(TX.set) + (S.pkg === 'zip' ? '.zip' : ''); }
  function txReport(d, F){
    var S = TX_SETS[TX.set], inc = F.filter(function(f){ return f.inc; }), miss = F.filter(function(f){ return f.miss; });
    return ['TrionCAD · Báo cáo đóng gói gửi đi · ' + TC.date(), 'Bản vẽ: ' + d.path, 'Thiết lập: ' + TX.set + ' · ' + (S.pkg === 'zip' ? 'ZIP' : 'thư mục') + ' · ' + S.fmt + ' · ' + S.dir + '\\' + txPkg(d), 'Hành động: ' + ([S.noprn ? 'đặt máy in về “Không”' : '', S.bind ? S.bindk.toLowerCase() + ' xref' : '', S.purge ? 'dọn dẹp bản vẽ' : '', S.uni ? 'chuyển font sang Unicode' : ''].filter(Boolean).join(', ') || 'không'), '']
      .concat(TX.note ? ['Ghi chú:', TX.note, ''] : [], ['Tệp trong gói (' + inc.length + '):'], inc.map(function(f){ return '  ' + f.name + '  ·  ' + f.type + (f.size && f.size !== '—' ? '  ·  ' + f.size : ''); }),
        miss.length ? ['', 'Thiếu (' + miss.length + '), người nhận cần bổ sung:'].concat(miss.map(function(f){ return '  ' + f.name + '  ·  ' + f.type + '  ·  ' + f.dir; })) : [],
        F.filter(function(f){ return f.skip && f.st; }).length ? ['', 'Không đưa vào gói:'].concat(F.filter(function(f){ return f.skip && f.st; }).map(function(f){ return '  ' + f.name + '  ·  ' + f.st; })) : []).join('\n');
  }
  function txEdit(name, isNew, done){
    var S = JSON.parse(JSON.stringify(TX_SETS[name] || TX_SETS.Standard)), rad = function(nm, v, lab, cur){ return H.rad(nm, v, lab, cur === v); };
    var dlg = TC.dialog({ id:'txset', title:isNew ? 'Thiết lập đóng gói mới' : 'Sửa thiết lập đóng gói', cmd:'ETRANSMIT', code:'UI-B11', w:680,
      body:'<div class="stack">' + (isNew ? '<div class="frow" style="--lab-w:110px"><label class="lab" for="ts-n">Tên thiết lập</label><span class="field"><input class="inp" id="ts-n" value="' + esc(name + ' (bản sao)') + '" spellcheck="false"><span class="fmsg" id="ts-m"></span></span></div>' : H.row('Tên thiết lập', '<b>' + esc(name) + '</b>', 110)) +
        '<div class="grid2"><div class="stack">' +
          H.grp('Gói', H.row('Kiểu gói', SC.seg('tspkg', ['Thư mục', 'ZIP'], S.pkg === 'zip' ? 'ZIP' : 'Thư mục'), 92) + H.row('Định dạng tệp', H.sel('ts-fmt', ['Giữ nguyên', 'DWG 2018', 'DWG 2013', 'DWG 2010', 'DWG 2007', 'DWG 2004', 'DWG 2000'], S.fmt, 170), 92)) +
          H.grp('Thư mục đích', '<span class="hrow" style="flex-wrap:nowrap"><input class="inp grow mono" id="ts-dir" value="' + esc(S.dir) + '" style="font-size:var(--fs-xs)" aria-label="Thư mục đích"><button class="btn" type="button" id="ts-br">Duyệt…</button></span><span class="fmsg" id="ts-dm"></span>' + H.row('Khi trùng tên', '<select class="inp" id="ts-name" style="width:190px"><option value="ask"' + (S.name === 'ask' ? ' selected' : '') + '>Hỏi tên tệp</option><option value="inc"' + (S.name === 'inc' ? ' selected' : '') + '>Thêm số vào tên</option><option value="over"' + (S.name === 'over' ? ' selected' : '') + '>Ghi đè</option></select>', 92)) +
          H.grp('Cấu trúc đường dẫn', rad('ts-path', 'tree', 'Giữ cấu trúc thư mục (đường dẫn tương đối)', S.path) + rad('ts-path', 'flat', 'Đặt mọi tệp chung một thư mục', S.path) + rad('ts-path', 'keep', 'Giữ nguyên đường dẫn gốc', S.path)) +
        '</div><div class="stack">' +
          H.grp('Hành động', H.chk('ts-noprn', 'Đặt máy in mặc định là “Không”', S.noprn) + '<span class="hrow" style="gap:6px;flex-wrap:nowrap">' + H.chk('ts-bind', 'Bind xref', S.bind) + '<select class="inp sm" id="ts-bindk" aria-label="Kiểu bind" style="width:84px"' + (S.bind ? '' : ' disabled') + '>' + SC.opts(['Bind', 'Insert'], S.bindk) + '</select></span>' + H.chk('ts-purge', 'Dọn dẹp bản vẽ', S.purge) + '<span class="hrow" style="gap:6px">' + H.chk('ts-uni', 'Chuyển font sang Unicode', S.uni) + '<span class="tag acc">TrionCAD</span></span>') +
          H.grp('Kèm theo', H.chk('ts-fonts', 'Font (kể cả SHX tiếng Việt)', S.fonts) + H.chk('ts-dlink', 'Tệp từ liên kết dữ liệu', S.dlink) + H.chk('ts-unload', 'Tham chiếu chưa nạp', S.unload)) +
          H.grp('Mô tả', '<textarea class="inp" id="ts-desc" rows="3" style="height:auto;padding:5px 7px;resize:vertical">' + esc(S.desc || '') + '</textarea>') +
        '</div></div></div>',
      foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="ts-ok">' + (isNew ? 'Tạo thiết lập' : 'Lưu thiết lập') + '</button>' });
    var $t = function(s){ return $(s, dlg.el); };
    dlg.el.addEventListener('change', function(e){ if (e.target.id === 'ts-bind') $t('#ts-bindk').disabled = !e.target.checked; });
    $t('#ts-br').addEventListener('click', function(){ TC.toast('Hộp thoại chọn thư mục của Windows.', { type:'info', log:false }); });
    $t('#ts-ok').addEventListener('click', function(){
      var nm = isNew ? $t('#ts-n').value.trim() : name, dir = $t('#ts-dir').value.trim(), e1 = isNew ? (!nm ? 'Nhập tên thiết lập.' : TX_SETS[nm] ? 'Đã có thiết lập tên này. Đặt tên khác.' : '') : '', e2 = dir ? '' : 'Chọn thư mục đích.';
      [['#ts-n', '#ts-m', e1], ['#ts-dir', '#ts-dm', e2]].forEach(function(x){ var i = $t(x[0]), m = $t(x[1]); if (!i) return; i.classList.toggle('err', !!x[2]); m.className = 'fmsg' + (x[2] ? ' err' : ''); m.innerHTML = x[2] ? ic('warn', 'xs') + esc(x[2]) : ''; });
      if (e1 || e2) return;
      TX_SETS[nm] = { pkg:$t('[data-seg="tspkg"] [aria-pressed="true"]').dataset.v === 'ZIP' ? 'zip' : 'dir', fmt:$t('#ts-fmt').value, dir:dir, name:$t('#ts-name').value, path:$t('[name="ts-path"]:checked').value, noprn:$t('#ts-noprn').checked, bind:$t('#ts-bind').checked, bindk:$t('#ts-bindk').value, purge:$t('#ts-purge').checked, uni:$t('#ts-uni').checked, fonts:$t('#ts-fonts').checked, dlink:$t('#ts-dlink').checked, unload:$t('#ts-unload').checked, desc:$t('#ts-desc').value.trim() };
      dlg.close(true); done(nm);
    });
  }
  IMPL.ETRANSMIT = function(ctx){
    var d = D(), job = null;
    if (!TX_SETS[TX.set]) TX.set = 'Standard';
    return new Promise(function(res){
      var dlg = TC.dialog({ id:'tx', title:'Đóng gói gửi đi', cmd:'ETRANSMIT', code:'UI-B11', w:960, h:620, flush:true, body:'<div class="tx" id="tx-b"></div>', foot:'<span></span>', onClose:function(){ if (job) TC.toast('Đóng gói vẫn chạy ở nền. Theo dõi ở thanh trạng thái.', { type:'info', log:false }); res(); } });
      var F = [];
      function row(f, tree){
        var dis = f.fixed || f.miss || f.skip, st = f.miss ? '<span class="tag warn">' + ic('warn', 's12') + esc(f.st) + '</span>' : f.st ? '<span class="faint">' + esc(f.st) + '</span>' : '<span class="tag ok">' + ic('check', 's12') + 'Đưa vào gói</span>';
        return '<tr class="' + (f.inc ? '' : 'faint') + '"><td class="ctr"><input type="checkbox" data-txon="' + esc(f.id) + '"' + (f.inc ? ' checked' : '') + (dis ? ' disabled' : '') + ' aria-label="Đưa ' + esc(f.name) + ' vào gói"></td><td' + (tree ? ' style="padding-left:' + (f.g === 'dwg' ? 8 : 30) + 'px"' : '') + ' title="' + esc(f.dir + '\\' + f.name) + '">' + ic(TX_G.filter(function(g){ return g[0] === f.g; })[0][2], 's12') + ' ' + esc(f.name) + (f.rm != null ? ' <button class="ib sm" type="button" data-txrm="' + f.rm + '" aria-label="Bỏ tệp thêm ' + esc(f.name) + '">' + ic('x') + '</button>' : '') + '</td>' +
          (tree ? '' : '<td class="faint" title="' + esc(f.type) + '">' + esc(f.type) + '</td><td class="faint mono" style="font-size:var(--fs-xs)" title="' + esc(f.dir) + '">' + esc(f.dir) + '</td>') + '<td class="num">' + esc(f.size) + '</td><td title="' + esc(f.st || '') + '">' + st + '</td></tr>';
      }
      function render(){
        if (!document.contains(dlg.el)) return;
        var S = TX_SETS[TX.set]; F = txFiles(d);
        var inc = F.filter(function(f){ return f.inc; }), miss = F.filter(function(f){ return f.miss; }), legacy = F.filter(function(f){ return f.g === 'font' && f.miss; }), size = inc.reduce(function(s, f){ return s + kb(f.size); }, 0) * (S.pkg === 'zip' ? .62 : 1), run = !!job;
        var body = TX.tab === 'tree'
          ? '<table class="tbl"><colgroup><col style="width:30px"><col><col style="width:80px"><col style="width:250px"></colgroup><thead><tr><th></th><th>Tệp</th><th class="num">Dung lượng</th><th>Trạng thái</th></tr></thead><tbody>' + TX_G.map(function(g){ var L = F.filter(function(f){ return f.g === g[0]; }); if (!L.length) return ''; if (g[0] === 'dwg') return row(L[0], true); var col = TX.col[g[0]]; return '<tr class="tx-g"><td></td><td colspan="3" style="padding-left:20px"><button class="tx-tw" type="button" data-txcol="' + g[0] + '" aria-expanded="' + !col + '">' + ic(col ? 'right' : 'down', 'xs') + '<b>' + g[1] + '</b><span class="faint"> · ' + L.filter(function(f){ return f.inc; }).length + '/' + L.length + ' tệp</span>' + (L.some(function(f){ return f.miss; }) ? ' ' + ic('warn', 's12 warn') : '') + '</button></td></tr>' + (col ? '' : L.map(function(f){ return row(f, true); }).join('')); }).join('') + '</tbody></table>'
          : '<table class="tbl"><colgroup><col style="width:30px"><col style="width:210px"><col style="width:150px"><col><col style="width:72px"><col style="width:190px"></colgroup><thead><tr><th></th><th>Tệp</th><th>Loại</th><th>Thư mục</th><th class="num">Dung lượng</th><th>Trạng thái</th></tr></thead><tbody>' + F.map(function(f){ return row(f, false); }).join('') + '</tbody></table>';
        $('#tx-b', dlg.el).innerHTML = '<div class="tx-main' + (run ? ' is-busy' : '') + '"><div class="tx-left"><div class="tabs" role="tablist">' + [['tree', 'Cây tệp'], ['table', 'Bảng tệp']].map(function(t){ return '<button class="tab" role="tab" type="button" data-txtab="' + t[0] + '" aria-selected="' + (TX.tab === t[0]) + '">' + t[1] + '</button>'; }).join('') + '</div>' +
            '<div class="pal-body">' + body + '</div>' +
            (miss.length ? '<div class="note warn" style="margin:8px">' + ic('warn') + '<span>Gói sẽ thiếu ' + miss.length + ' tệp: ' + miss.map(function(f){ return '<b>' + esc(f.name) + '</b>'; }).join(', ') + '.' + (legacy.length ? ' Bật Chuyển font sang Unicode để người nhận đọc đúng chữ.' : ' Tìm lại đường dẫn trước khi đóng gói, hoặc gửi kèm sau.') + '</span>' + (legacy.length ? '<button class="btn sm" type="button" data-tx="uni">Bật</button>' : '<button class="btn sm" type="button" data-tx="find">Tìm tệp…</button>') + '</div>' : '') +
            '<div class="pal-tools tx-foot"><button class="btn sm" type="button" data-tx="add">' + ic('plus') + 'Thêm tệp…</button><span class="grow"></span><span class="faint" style="font-size:var(--fs-s)">' + inc.length + ' tệp · khoảng ' + (size > 1024 ? TC.d(size / 1024, 1).replace('.', ',') + ' MB' : Math.round(size) + ' KB') + (S.pkg === 'zip' ? ' sau khi nén' : '') + '</span></div></div>' +
          '<div class="tx-right"><div class="grp"><span class="gh">Thiết lập đóng gói</span><span class="hrow" style="flex-wrap:nowrap"><select class="inp grow" id="tx-set" aria-label="Thiết lập đóng gói">' + SC.opts(Object.keys(TX_SETS), TX.set) + '</select><button class="btn" type="button" data-tx="new">Mới…</button><button class="btn" type="button" data-tx="edit">Sửa…</button></span>' +
            '<dl class="tx-sum"><dt>Gói</dt><dd>' + (S.pkg === 'zip' ? 'ZIP' : 'Thư mục') + ' · ' + esc(S.fmt === 'Giữ nguyên' ? 'giữ ' + d.ver : S.fmt) + '</dd><dt>Lưu vào</dt><dd class="mono" title="' + esc(S.dir + '\\' + txPkg(d)) + '">' + esc(S.dir) + '\\' + esc(txPkg(d)) + '</dd><dt>Đường dẫn</dt><dd>' + { tree:'Giữ cấu trúc thư mục', flat:'Chung một thư mục', keep:'Giữ nguyên đường dẫn' }[S.path] + '</dd><dt>Hành động</dt><dd>' + ([S.uni ? 'Chuyển font sang Unicode' : '', S.purge ? 'Dọn dẹp' : '', S.bind ? S.bindk + ' xref' : '', S.noprn ? 'Máy in về “Không”' : ''].filter(Boolean).join(', ') || 'Không') + '</dd><dt>Kèm theo</dt><dd>' + ([S.fonts ? 'font' : '', S.dlink ? 'liên kết dữ liệu' : '', S.unload ? 'tham chiếu chưa nạp' : ''].filter(Boolean).join(', ') || 'Không') + '</dd></dl>' +
            (S.desc ? '<span class="faint" style="font-size:var(--fs-xs)">' + esc(S.desc) + '</span>' : '') + '</div>' +
            '<div class="grp"><label class="gh" for="tx-note">Ghi chú gửi kèm</label><textarea class="inp" id="tx-note" rows="5" style="height:auto;padding:5px 7px;resize:vertical" placeholder="Ví dụ: Bản vẽ kiến trúc phát hành lần 2, sửa theo góp ý ngày 15/10.">' + esc(TX.note) + '</textarea><span class="faint" style="font-size:var(--fs-xs)">Ghi vào báo cáo đóng gói, gửi kèm trong gói.</span></div></div></div>';
        var f = $('.dlg-f', dlg.el);
        f.innerHTML = run ? '<button class="btn ghost" type="button" data-x="close">Chạy ở nền</button><span class="tx-prog"><span class="prog grow"><i style="width:' + job.p.toFixed(0) + '%"></i></span><span class="mono">' + Math.round(job.p) + '%</span><span class="faint">' + esc(job.label) + '</span></span><button class="btn danger" type="button" data-tx="stop">Hủy</button>'
          : '<button class="btn" type="button" data-tx="report">' + ic('list') + 'Xem báo cáo</button><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" data-tx="go"' + (inc.length ? '' : ' disabled') + '>' + ic('pkg') + 'Đóng gói ' + inc.length + ' tệp</button>';
      }
      function start(){
        var S = TX_SETS[TX.set], inc = F.filter(function(f){ return f.inc; }), steps = ['Thu thập tệp tham chiếu'].concat(S.uni ? ['Chuyển font sang Unicode: ' + d.name] : [], S.purge ? ['Dọn dẹp bản vẽ'] : [], S.bind ? [S.bindk + ' xref vào bản vẽ'] : [], S.noprn ? ['Đặt máy in của layout về “Không”'] : [], S.fmt !== 'Giữ nguyên' ? ['Lưu thành ' + S.fmt] : [], inc.map(function(x){ return 'Sao chép ' + x.name; }), [S.pkg === 'zip' ? 'Nén ZIP' : 'Ghi thư mục', 'Ghi báo cáo đóng gói']);
        var k = 0, name = txPkg(d), miss = F.filter(function(x){ return x.miss; }).length;
        // tác vụ nền: tiến trình và nút hủy ở thanh trạng thái, đóng hộp thoại vẫn chạy tiếp
        var tick = function(){ render(); A().setTask(job ? '<span class="sb-task"><span class="spin"></span>Đóng gói ' + esc(d.name) + ' ' + Math.round(job.p) + '%<span class="prog"><i style="width:' + job.p.toFixed(0) + '%"></i></span><button class="ib sm" type="button" id="tx-x" aria-label="Hủy đóng gói">' + ic('x') + '</button></span>' : ''); var x = $('#tx-x'); if (x) x.onclick = function(){ if (job) job.stop = true; }; };
        job = { p:0, label:steps[0], stop:false }; tick();
        var t = setInterval(function(){
          if (job.stop){ clearInterval(t); job = null; tick(); TC.toast('Đã hủy đóng gói. Chưa tạo tệp nào trong ' + esc(S.dir) + '.', { type:'warn', log:false }); return; }
          k++; job.p = Math.min(100, k / steps.length * 100); job.label = steps[Math.min(k, steps.length - 1)]; tick();
          if (k < steps.length) return;
          clearInterval(t); job = null; tick(); dlg.close(true);
          TC.toast('Đã đóng gói ' + inc.length + ' tệp vào <span class="mono">' + esc(name) + '</span>' + (miss ? '. Thiếu ' + miss + ' tệp không tìm thấy: xem báo cáo.' : '.'), { type:miss ? 'warn' : 'ok', act:{ label:'Mở thư mục', fn:function(){ TC.toast('Mở ' + esc(S.dir) + ' trong File Explorer.', { type:'info', log:false }); } }, kind:'task' });
          ctx.out('ETRANSMIT: ' + inc.length + ' tệp → ' + esc(S.dir + '\\' + name) + '. Báo cáo: ' + esc(SC.att.base(d.name)) + ' - Bao-cao-dong-goi.txt');
        }, 230);
      }
      dlg.el.addEventListener('click', function(e){
        var tb = e.target.closest('[data-txtab]'); if (tb){ TX.tab = tb.dataset.txtab; render(); return; }
        var cl = e.target.closest('[data-txcol]'); if (cl){ TX.col[cl.dataset.txcol] = !TX.col[cl.dataset.txcol]; render(); var c2 = $('[data-txcol="' + cl.dataset.txcol + '"]', dlg.el); if (c2) c2.focus(); return; }
        var rm = e.target.closest('[data-txrm]'); if (rm){ TX.extra.splice(Number(rm.dataset.txrm), 1); render(); return; }
        var b = e.target.closest('[data-tx]'); if (!b || b.disabled) return; var k = b.dataset.tx;
        if (k === 'go') start();
        else if (k === 'stop'){ if (job) job.stop = true; }
        else if (k === 'report') SC.textWindow(txReport(d, F), 'Báo cáo đóng gói · ' + d.name);
        else if (k === 'add'){ var nx = TX_EXTRA.filter(function(x){ return !TX.extra.some(function(y){ return y[0] === x[0]; }); })[0]; if (nx){ TX.extra.push(nx); render(); } else TC.toast('Hộp thoại chọn tệp của Windows: chọn nhiều tệp để gửi kèm.', { type:'info', log:false }); }
        else if (k === 'uni'){ TX_SETS[TX.set].uni = true; render(); }
        else if (k === 'find'){ TX.wait = true; SC.missingRefs(); }
        else if (k === 'new' || k === 'edit') txEdit(TX.set, k === 'new', function(nm){ TX.set = nm; render(); });
      });
      // về từ hộp thoại tìm tham chiếu (UI-B06): đọc lại trạng thái xref, ảnh
      dlg.el.addEventListener('focusin', function(){ if (TX.wait && !$('#dlgt-refs')){ TX.wait = false; render(); } });
      dlg.el.addEventListener('change', function(e){ var t = e.target; if (t.dataset.txon){ TX.off[t.dataset.txon] = !t.checked; render(); } else if (t.id === 'tx-set'){ TX.set = t.value; render(); } });
      dlg.el.addEventListener('input', function(e){ if (e.target.id === 'tx-note') TX.note = e.target.value; });
      render();
    });
  };

  /* ================= Xử lý tệp hàng loạt UI-B14 (TCBATCH): màn hình chung với UI-J03 ở screens4.js ================= */
  IMPL.TCBATCH = function(){ SC.fontBatch('batch'); };

  /* ================= danh mục: dựng trạng thái demo rồi mở ================= */
  var ROOMS1 = [[2000, 2000], [9300, 2000], [1900, 6350], [9700, 6350], [4800, 8000], [6900, 8000], [5000, 5800]];
  OPEN['UI-I05'] = function(){
    SC.cat.MOD(); Object.assign(RA, { src:'text', layer:'KT-TEXT', sort:'i', dir:1 });
    setTimeout(function(){ TC.cmd.run('TCROOMAREA'); if (RA.dlg) SC.zoomBeside([-1200, -1600, 12600, 9800], RA.dlg.el);
      var k = 0; (function feed(){ if (k < ROOMS1.length && TC.cmdline.current() === 'TCROOMAREA' && TC.cad.feedPoint(ROOMS1[k++])) setTimeout(feed, 70); })(); }, 60);
  };
  OPEN['UI-I06'] = function(){
    var d = SC.cat.MOD(); TC.cad.clearSel();
    Object.assign(DX, { src:'doc', ids:null, kind:'block', att:false, on:{ 'B:CUA-DI-1C':true, 'B:CUA-SO':true }, cats:{}, props:{ name:true, sx:true }, names:{ name:'Loại cửa', sx:'Bề rộng (mm)' }, comb:true, cnt:true, sort:'name', dir:1, q:'', tq:'', pq:'', table:true, file:true, fmt:'XLSX', open:true, title:'BẢNG THỐNG KÊ CỬA' });
    setTimeout(function(){ TC.cmd.run('DATAEXTRACTION', 4); }, 60);
  };
  OPEN['UI-J15'] = function(){ var d = SC.cat.PRO(); AI.file = null; SC.attLast = 'Khung-ten_' + SC.att.ascii(SC.att.base(d.name)) + '.xlsx'; SC.attIn(); };
  OPEN['UI-J12'] = function(){
    var d = SC.cat.MOD(), t = d.ents.filter(function(e){ return e.t === 'table' && e.tstyle === 'TCVN-dien-tich'; })[0];
    if (!t) ED.run('TCROOMAREA', function(){ t = ED.add(ED.make(SC.roomTable(d, ROOMS1, [600, 17600]))); });
    d.sel = [t.id]; TC.cad.afterSel(); A().view.zoomTo([-4500, 7000, 18000, 19500]);
    setTimeout(function(){ TC.cmd.run('TCEXCELOUT'); }, 60);
  };
  OPEN['UI-B11'] = function(){ SC.cat.PRO(); Object.assign(TX, { set:'Standard', tab:'tree', extra:[], off:{}, col:{}, note:'' }); setTimeout(function(){ TC.cmd.run('ETRANSMIT'); }, 60); };
  OPEN['UI-B14'] = function(){
    SC.cat.PRO(); var F = SC.FB;
    if (F.job && !F.job.done){ SC.fontBatch('batch'); return; }
    if ($('#fb-b')) TC.dialog({ id:'fb' }).close();
    F.mode = ''; F.files = null; SC.fontBatch('batch');
    Object.assign(F, { ops:{ audit:true, purge:true, font:true, ver:true }, ver:'DWG 2013', out:'dir' }); F.ui();
  };
})();
