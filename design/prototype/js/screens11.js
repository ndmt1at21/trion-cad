/* TrionCAD prototype · Đợt 4: công cụ VN và bộ bản vẽ (H07 bộ bản vẽ, J05 trình sửa khung tên đầy đủ, J06 khung in trong Model, J07 đánh số, J08 sao chép tăng số, J09 công cụ text, J10 ký hiệu, H11 phần còn lại). */
(function(){
  'use strict';
  var TC = window.TC, $ = TC.$, $$ = TC.$$, esc = TC.esc, ic = TC.ic, G = TC.geo, IMPL = TC.cmd.impl, ED = TC.ed, SC = TC.screens, H = SC.h;
  function A(){ return TC.app; } function V(){ return TC.app.view; } function D(){ return TC.app.doc(); }
  var OPEN = SC.open4 = SC.open4 || {}; // mã màn hình → hàm mở (danh mục gọi khi bấm); tiện ích mở ở SC.cat (D1, PRO, MOD, RUN, plan, pick, flash)
  var P = TC.PALETTES, T = ED.T, rp = TC.rp;
  function docBy(id){ return A().docs.filter(function(x){ return x.id === id; })[0]; }
  function num(v, def){ var n = TC.parseNum(v, 'none'); return n.err ? def : n.v; }
  function bbOf(pts){ var b = [Infinity, Infinity, -Infinity, -Infinity]; pts.forEach(function(q){ b = [Math.min(b[0], q[0]), Math.min(b[1], q[1]), Math.max(b[2], q[0]), Math.max(b[3], q[1])]; }); return b; }
  function withIds(d, list){ list.forEach(function(o){ o.id = ++d.seq; }); return list; }
  // lõi sửa (ED) luôn ghi vào bản vẽ hiện hành: tạm đặt bản vẽ id làm hiện hành, chạy fn, rồi trả lại
  function withDoc(id, fn){ var app = A(), keep = app.active; app.active = id; try { return fn(app.doc()); } finally { app.active = keep; if (keep !== id && app.doc()) TC.emit('dirty', app.doc()); } }
  // vẽ danh sách đối tượng (chưa thêm vào bản vẽ) vừa khung canvas: xem trước khung tên, ký hiệu
  function paint(c, d, list, paper){
    var x = c.getContext('2d'), W = c.width, Hh = c.height, f = Object.create(TC.View.prototype); f.W = W; f.H = Hh; f.doc = d; f.dpr = 1;
    x.fillStyle = paper ? '#FFFFFF' : TC.cssv('--c-canvas'); x.fillRect(0, 0, W, Hh);
    var es = list.map(function(e){ return TC.prepEnt(d, Object.assign({ layer:'0' }, e), true); }), b = bbOf([].concat.apply([], es.map(function(e){ return [[e._bb[0], e._bb[1]], [e._bb[2], e._bb[3]]]; })));
    if (!isFinite(b[0])) return; var v = { cx:(b[0] + b[2]) / 2, cy:(b[1] + b[3]) / 2, s:Math.min((W - 16) / (b[2] - b[0] || 1), (Hh - 16) / (b[3] - b[1] || 1)) };
    es.forEach(function(e){ f.drawPrims(x, e, v, !!paper); });
  }

  /* ================= khung tên TCVN 5571 mẫu v1.0 (dùng chung J05, J06, H07) ================= */
  var PAPER = { A0:[1189, 841], A1:[841, 594], A2:[594, 420], A3:[420, 297], A4:[297, 210] };
  var TBF = [['DONVI', 'Đơn vị thiết kế'], ['CONGTRINH', 'Công trình'], ['HANGMUC', 'Hạng mục'], ['GIAIDOAN', 'Giai đoạn'], ['TENBV', 'Tên bản vẽ'], ['KYHIEU', 'Ký hiệu'], ['TILE', 'Tỉ lệ'], ['NGAY', 'Ngày'], ['SOTO', 'Số tờ / tổng số'], ['CHUNHIEM', 'Chủ nhiệm'], ['CHUTRI', 'Chủ trì'], ['THIETKE', 'Thiết kế'], ['VE', 'Người vẽ'], ['KIEMTRA', 'Kiểm tra'], ['DUYET', 'Duyệt']];
  function tbLabel(t){ return (TBF.filter(function(f){ return f[0] === t; })[0] || [t, t])[1]; }
  // khung bản vẽ W × H mm giấy và khung tên 180 × 52 ở góc dưới phải: đủ 15 ô attribute, ô logo
  function frameDef(W, Hh, logo){
    var L = [], a = function(o){ o.layer = '0'; L.push(o); }, R = function(x0, y0, x1, y1, lw){ a({ t:'pline', pts:rp(x0, y0, x1, y1), closed:true, lw:lw }); }, S = function(x0, y0, x1, y1, lw){ a({ t:'line', p1:[x0, y0], p2:[x1, y1], lw:lw || .18 }); };
    var x = W - 185, y = 5, Tx = function(dx, dy, s){ a({ t:'text', p:[x + dx, y + dy], h:1.6, s:s, style:'Standard', al:'l' }); }, At = function(tag, dx, dy, h, s){ a({ t:'attdef', tag:tag, prompt:tbLabel(tag), p:[x + dx, y + dy], h:h, s:s || '', style:'Standard', al:'l' }); };
    R(0, 0, W, Hh, .13); R(25, 5, W - 5, Hh - 5, .5); R(x, y, x + 180, y + 52, .5);
    S(x + 70, y, x + 70, y + 52, .5); S(x, y + 40, x + 180, y + 40, .35); S(x + 22, y + 40, x + 22, y + 52); S(x + 20, y, x + 20, y + 40); S(x + 50, y, x + 50, y + 40);
    for (var i = 1; i < 6; i++) S(x, y + i * 20 / 3, x + 70, y + i * 20 / 3);
    [['DUYỆT', 'DUYET'], ['KIỂM TRA', 'KIEMTRA'], ['VẼ', 'VE'], ['THIẾT KẾ', 'THIETKE'], ['CHỦ TRÌ', 'CHUTRI'], ['CHỦ NHIỆM', 'CHUNHIEM']].forEach(function(r, k){ Tx(1.5, k * 20 / 3 + 2.3, r[0]); At(r[1], 21.5, k * 20 / 3 + 2.3, 1.8); });
    Tx(24, 48.6, 'ĐƠN VỊ THIẾT KẾ'); At('DONVI', 24, 43, 1.8, '[TÊN CÔNG TY]');
    if (logo) a({ t:'image', p:[x + 1.5, y + 41.5], w:19, h:9, name:logo, frame:false });
    S(x + 70, y + 30, x + 180, y + 30); S(x + 70, y + 14, x + 180, y + 14); S(x + 140, y + 30, x + 140, y + 40); S(x + 95, y, x + 95, y + 14); S(x + 125, y, x + 125, y + 14); S(x + 160, y, x + 160, y + 14);
    Tx(72, 49, 'CÔNG TRÌNH'); At('CONGTRINH', 72, 42.6, 2.6); Tx(72, 37.6, 'HẠNG MỤC'); At('HANGMUC', 72, 32, 2.2); Tx(142, 37.6, 'GIAI ĐOẠN'); At('GIAIDOAN', 142, 32, 1.8);
    Tx(72, 27.6, 'TÊN BẢN VẼ'); At('TENBV', 72, 18.6, 3.2, 'TÊN BẢN VẼ');
    Tx(72, 11, 'TỈ LỆ'); At('TILE', 72, 3.6, 2.6, '1:100'); Tx(97, 11, 'NGÀY'); At('NGAY', 97, 3.6, 2.6, TC.date()); Tx(127, 11, 'KÝ HIỆU'); At('KYHIEU', 127, 3.2, 3.6, 'KT 01'); Tx(162, 11, 'SỐ TỜ'); At('SOTO', 162, 3.6, 2.6, '1/1');
    return L;
  }
  function slug(s){ return TC.fold(s).toUpperCase().replace(/[^A-Z0-9]+/g, '-').replace(/^-|-$/g, ''); }
  // block khung mẫu theo khổ, hướng (và mẫu công ty: thêm logo của mẫu); tạo khi dùng lần đầu
  function ensureFrame(d, size, por, tpl){
    var nm = 'TCVN-' + size + (por ? '-DOC' : '') + (tpl ? '-' + slug(tpl.name) : '');
    if (!d.blocks[nm]){ var p = PAPER[size]; d.blocks[nm] = { name:nm, desc:'Khổ ' + size + (por ? ' dọc' : ' ngang') + ', khung tên TCVN 5571 ' + (tpl ? '(mẫu công ty ' + tpl.name + ')' : '(mẫu TrionCAD)'), ents:withIds(d, frameDef(por ? p[1] : p[0], por ? p[0] : p[1], tpl ? tpl.logo : PJ.logo)) }; }
    return nm;
  }
  function hasTag(d, name, tag){ var b = d.blocks[name]; return !!(b && b.ents.some(function(x){ return x.t === 'attdef' && x.tag === tag; })); }
  function isTB(d, name){ return hasTag(d, name, 'TENBV') || hasTag(d, name, 'KYHIEU'); }
  function attrsOf(d, e){ var o = {}; ((d.blocks[e.name] || {}).ents || []).forEach(function(a){ if (a.t === 'attdef') o[a.tag] = e.attrs && e.attrs[a.tag] != null ? e.attrs[a.tag] : a.s; }); return o; }
  function missingTags(d, name){ return TBF.map(function(f){ return f[0]; }).filter(function(t){ return !hasTag(d, name, t); }); }
  // đổi định nghĩa khung tên theo mẫu v1.0 (giữ tên block và giá trị attribute của mọi lần chèn); gọi trong ED.run
  function redefine(d, name){ var bd = d.blocks[name], p = bd.ents.filter(function(x){ return x.t === 'pline'; })[0].pts[2], done = ED.state(function(){ return bd.ents; }, function(s){ bd.ents = s; TC.prepDoc(d); }); bd.ents = withIds(d, frameDef(p[0], p[1], PJ.logo)); done(); TC.prepDoc(d); }
  function tbPaint(c, d, name, attrs){ paint(c, d, [{ t:'insert', name:name, p:[0, 0], sx:1, sy:1, rot:0, layer:'KT-KHUNG', attrs:attrs || {} }], true); }
  // ảnh logo (mô phỏng): cùng cách vẽ ảnh raster trên vùng vẽ
  function logoPaint(c, name){ var x = c.getContext('2d'), w = c.width, h = c.height; x.clearRect(0, 0, w, h); if (!name) return; var g = x.createLinearGradient(0, 0, w, h); g.addColorStop(0, 'hsl(140,18%,52%)'); g.addColorStop(.55, 'hsl(90,14%,60%)'); g.addColorStop(1, 'hsl(35,20%,48%)'); x.fillStyle = g; x.fillRect(2, 2, w - 4, h - 4); }
  function pickLogo(b, done){ var r = b.getBoundingClientRect(); TC.menu([{ h:'D:\\Tai-nguyen\\Logo' }, { label:'logo-an-phu.png <span class="faint">· 600 × 240 px</span>', icon:'image', act:function(){ done('logo-an-phu.png'); } }, { label:'logo-an-phu-den-trang.png <span class="faint">· 600 × 240 px</span>', icon:'image', act:function(){ done('logo-an-phu-den-trang.png'); } }, { sep:1 }, { label:'Duyệt…', icon:'open', act:function(){ TC.toast('Hộp thoại chọn ảnh PNG, JPG của hệ điều hành.', { type:'info', log:false }); } }], r.left, r.bottom + 2, { returnFocus:b, minWidth:260 }); }

  /* ================= bộ bản vẽ: thông tin dự án, danh sách tờ, ánh xạ khung tên (UI-H07) ================= */
  var PJ = SC.project = { CONGTRINH:'NHÀ Ở GIA ĐÌNH 2 TẦNG', HANGMUC:'KIẾN TRÚC, KẾT CẤU, ĐIỆN NƯỚC', CHUDAUTU:'Ông Nguyễn Văn Hải', DIADIEM:'Lô B7, KĐT An Phú, TP. Thủ Đức', DONVI:'CÔNG TY TNHH THIẾT KẾ AN PHÚ', GIAIDOAN:'Thiết kế bản vẽ thi công', CHUTRI:'N.M.AN', CHUNHIEM:'T.Q.BẢO', KIEMTRA:'L.V.DŨNG', logo:'' };
  var PJF = [['CONGTRINH', 'Tên công trình'], ['HANGMUC', 'Hạng mục'], ['CHUDAUTU', 'Chủ đầu tư'], ['DIADIEM', 'Địa điểm'], ['DONVI', 'Đơn vị thiết kế'], ['GIAIDOAN', 'Giai đoạn'], ['CHUTRI', 'Chủ trì'], ['CHUNHIEM', 'Chủ nhiệm'], ['KIEMTRA', 'Kiểm tra']];
  var STAGES = ['Thiết kế cơ sở', 'Thiết kế kỹ thuật', 'Thiết kế bản vẽ thi công'];
  var BMS = [['KT', 'Kiến trúc'], ['KC', 'Kết cấu'], ['ĐN', 'Điện'], ['CN', 'Cấp thoát nước'], ['ME', 'Cơ điện']];
  // nguồn dữ liệu của attribute khung tên: p.* thông tin dự án, s.* thông tin tờ
  var SRC = [['', '(nhập tay)']].concat(PJF.map(function(f){ return ['p.' + f[0], 'Dự án · ' + f[1]]; }), [['s.no', 'Tờ · Số hiệu'], ['s.title', 'Tờ · Tên bản vẽ'], ['s.scale', 'Tờ · Tỉ lệ'], ['s.paper', 'Tờ · Khổ giấy'], ['s.date', 'Tờ · Ngày'], ['s.count', 'Tờ · Số tờ / tổng số']]);
  function srcLabel(k){ return (SRC.filter(function(s){ return s[0] === k; })[0] || ['', ''])[1]; }
  var SS = SC.sheetSet = { name:'NhaO-2T', fmt:'pad', docs:['d1', 'd2'], order:[], meta:{}, sel:null, q:'', open:{ proj:false, map:false }, tpls:[], job:null,
    map:{ DONVI:'p.DONVI', CONGTRINH:'p.CONGTRINH', HANGMUC:'p.HANGMUC', GIAIDOAN:'p.GIAIDOAN', TENBV:'s.title', KYHIEU:'s.no', TILE:'s.scale', NGAY:'s.date', SOTO:'s.count', CHUNHIEM:'p.CHUNHIEM', CHUTRI:'p.CHUTRI', THIETKE:'', VE:'', KIEMTRA:'p.KIEMTRA', DUYET:'' } };
  // tờ của các tệp chưa mở (mô phỏng: thông tin tờ lưu trong tệp bộ bản vẽ)
  SS.extra = [['Chi-tiet-cau-thang.dwg', 'KienTruc', 'KT', '25/09/2026', [['CT-CAU-THANG', 'CHI TIẾT CẦU THANG', '1:25'], ['CT-LAN-CAN', 'CHI TIẾT LAN CAN, TAY VỊN', '1:10']]],
    ['Ket-cau-mong.dwg', 'KetCau', 'KC', '21/09/2026', [['Model', 'MẶT BẰNG MÓNG', '1:100'], ['CT-MONG', 'CHI TIẾT MÓNG M1, M2', '1:25']]],
    ['Dien-T1-T2.dwg', 'DienNuoc', 'ĐN', '14/10/2026', [['A3-DN-01', 'MẶT BẰNG CẤP ĐIỆN TẦNG 1', '1:100'], ['A3-DN-02', 'MẶT BẰNG CẤP ĐIỆN TẦNG 2', '1:100'], ['A3-DN-03', 'SƠ ĐỒ NGUYÊN LÝ CẤP ĐIỆN', '—']]],
    ['Cap-thoat-nuoc.dwg', 'CapThoatNuoc', 'CN', '10/10/2026', [['Model', 'MẶT BẰNG CẤP THOÁT NƯỚC', '1:100']], true]
  ].reduce(function(out, f){ return out.concat(f[4].map(function(s){ return { key:f[0] + ':' + s[0], d:null, e:null, sp:s[0] === 'Model' ? 'model' : 0, file:f[0], dir:(f[5] ? '\\\\SRV-01\\DuAn\\NhaO-2T\\' : 'D:\\DuAn\\NhaO-2T\\') + f[1], where:s[0], bm:f[2], title:s[1], scale:s[2], paper:'A3', date:f[3], missing:!!f[5], at0:{} }; })); }, []);
  function setMeta(k, o){ SS.meta[k] = Object.assign(SS.meta[k] || {}, o); }
  function init(){ if (SS.inited) return; SS.inited = true; var d1 = docBy('d1'), d2 = docBy('d2');
    if (d1) d1.ents.forEach(function(e){ var k = e.t === 'insert' && e.attrs && e.attrs.KYHIEU; if (/^KT-0[123]$/.test(k)) setMeta('d1:e' + e.id, { st:k === 'KT-02' ? 'm' : 'p', at:'12/10/2026' }); });
    if (d2 && d2.layouts[0]) setMeta('d2:L:' + d2.layouts[0].name, { st:'p', at:'13/10/2026' });
    ['Ket-cau-mong.dwg:Model', 'Ket-cau-mong.dwg:CT-MONG'].forEach(function(k){ setMeta(k, { st:'p', at:'22/09/2026' }); }); }
  function paperOf(w, h){ var a = Math.max(w, h); return ['A0', 'A1', 'A2', 'A3', 'A4'].filter(function(k){ return Math.abs(PAPER[k][0] - a) / a < .03; })[0] || Math.round(w) + '×' + Math.round(h); }
  function bmOf(k, d){ var m = /^([A-ZĐ]{2,3})[\s\-]?\d/.exec(k || ''); return m ? m[1] : /KetCau/i.test(d.path) ? 'KC' : 'KT'; }
  function sheetOf(d, k, where, e, li, L){
    var a = e ? attrsOf(d, e) : {}, key = d.id + ':' + k, m = SS.meta[key] || {}, k0 = e ? Math.abs(e.sx) || 1 : 1;
    return { key:key, d:d, e:e || null, sp:li == null ? 'model' : li, file:d.name, dir:d.path.replace(/\\[^\\]+$/, ''), where:where, bm:m.bm || bmOf(a.KYHIEU, d), title:m.title || a.TENBV || where,
      scale:a.TILE || (L && L.vports[0] ? '1:' + Math.round(L.vports[0].scale) : '—'), paper:L ? paperOf(L.paper.w, L.paper.h) : paperOf((e._bb[2] - e._bb[0]) / k0, (e._bb[3] - e._bb[1]) / k0), date:a.NGAY || (d.modified || '').split(' ')[0], at0:a };
  }
  // danh sách tờ: khung tên trong Model và layout của các bản vẽ trong bộ (đọc lại mỗi lần) + tờ của tệp chưa mở; thứ tự theo SS.order
  function sheets(){
    init(); var out = [];
    SS.docs.forEach(function(id){ var d = docBy(id); if (!d) return;
      d.ents.forEach(function(e){ if (e.t === 'insert' && isTB(d, e.name)) out.push(sheetOf(d, 'e' + e.id, 'Model', e)); });
      d.layouts.forEach(function(L, i){ var e = L.ents.filter(function(x){ return x.t === 'insert' && isTB(d, x.name); })[0]; out.push(sheetOf(d, 'L:' + L.name, L.name, e, i, L)); }); });
    out = out.concat(SS.extra.map(function(s){ var m = SS.meta[s.key] || {}; return Object.assign({}, s, { title:m.title || s.title, bm:m.bm || s.bm }); })).filter(function(s){ return !(SS.meta[s.key] || {}).hidden; });
    out.forEach(function(s){ if (SS.order.indexOf(s.key) < 0) SS.order.push(s.key); });
    var bi = function(b){ var i = BMS.map(function(x){ return x[0]; }).indexOf(b); return i < 0 ? 99 : i; };
    out.sort(function(a, b){ return bi(a.bm) - bi(b.bm) || SS.order.indexOf(a.key) - SS.order.indexOf(b.key); });
    var cnt = {}, k = {}; out.forEach(function(s){ cnt[s.bm] = (cnt[s.bm] || 0) + 1; });
    out.forEach(function(s){ s.idx = k[s.bm] = (k[s.bm] || 0) + 1; s.cnt = cnt[s.bm]; s.no = s.bm + ' ' + (SS.fmt === 'pad' ? ('0' + s.idx).slice(-2) : s.idx + '/' + s.cnt); var m = SS.meta[s.key] || {}; s.st = m.st || ''; s.at = m.at || ''; });
    return out;
  }
  function byKey(k){ return sheets().filter(function(s){ return s.key === k; })[0]; }
  function srcVal(src, s){ if (!src) return null; var k = src.slice(2); if (src[0] === 'p') return PJ[k]; return k === 'count' ? s.idx + '/' + s.cnt : s[k]; }
  function tbVals(s){ var o = {}; Object.keys(SS.map).forEach(function(t){ var v = srcVal(SS.map[t], s); if (v != null && v !== '') o[t] = String(v); }); return o; }
  function projVals(){ var o = {}; Object.keys(SS.map).forEach(function(t){ var s = SS.map[t]; if (s && s[0] === 'p' && PJ[s.slice(2)]) o[t] = PJ[s.slice(2)]; }); return o; }
  function sheetFor(d, e){ return SS.docs.indexOf(d.id) < 0 ? null : sheets().filter(function(s){ return s.e === e; })[0] || null; }
  function nextNo(bm, plus){ var n = sheets().filter(function(s){ return s.bm === bm; }).length + (plus || 1); return bm + ' ' + (SS.fmt === 'pad' ? ('0' + n).slice(-2) : n + '/' + n); }
  function refresh(){ var app = A(); if (app.pal.leftActive === 'sheets' || app.pal.rightActive === 'sheets') app.refreshPalettes(); }
  // ghi attribute khung tên của mọi tờ theo ánh xạ; mỗi bản vẽ đang mở một bước hoàn tác
  function updateAll(extra){
    var L = sheets(), by = {}, n = 0, docs = [], skip = {}, off = {}, bad = [];
    L.forEach(function(s){ if (s.e) (by[s.d.id] = by[s.d.id] || []).push(s); else if (!s.d){ if (s.missing) bad.push(s.file); else off[s.file] = (off[s.file] || 0) + 1; } });
    Object.keys(by).forEach(function(id){ withDoc(id, function(d){ var k = 0;
      ED.run('Cập nhật khung tên', function(){ by[id].forEach(function(s){ var w = Object.assign(tbVals(s), extra || {}), set = {}; Object.keys(w).forEach(function(t){ if (!hasTag(d, s.e.name, t)) skip[t] = 1; else if (String(s.at0[t]) !== String(w[t])) set[t] = w[t]; }); if (Object.keys(set).length){ ED.mod(s.e, function(x){ x.attrs = Object.assign({}, x.attrs, set); }); k++; } }); });
      if (k){ n += k; docs.push(d.name); } }); });
    var nOff = Object.keys(off).reduce(function(a, f){ return a + off[f]; }, 0), sk = Object.keys(skip);
    TC.toast((n ? 'Đã cập nhật <b>' + n + ' khung tên</b> trong ' + docs.length + ' bản vẽ đang mở (Ctrl+Z ở từng bản vẽ)' : 'Khung tên trong các bản vẽ đang mở đã khớp') + (nOff ? ' và ' + nOff + ' tờ ở ' + Object.keys(off).length + ' tệp chưa mở' : '') + '.' + (bad.length ? ' Bỏ qua ' + bad.length + ' tờ: thiếu ' + esc(bad[0]) + '.' : '') + (sk.length ? ' Khung cũ chưa có ô ' + sk.map(function(t){ return tbLabel(t).toLowerCase(); }).join(', ') + '.' : ''),
      { type:bad.length ? 'warn' : 'ok', act:sk.length ? { label:'Nâng khung lên v1.0', fn:function(){ TC.cmd.run('TCTITLE'); } } : null });
    V().draw(); refresh(); return n;
  }
  // tờ đã in bị sửa sau đó: xét đối tượng của bước hoàn tác mới nhất (khung trong Model, layout, vùng Model mà viewport của layout nhìn thấy)
  function touches(s, e, list){
    var d = s.d, b = e._bb, hit = function(r){ return !(b[2] < r[0] || b[0] > r[2] || b[3] < r[1] || b[1] > r[3]); };
    if (s.sp === 'model') return list === d.ents && hit(s.e._bb);
    var L = d.layouts[s.sp]; if (!L) return false; if (list === L.ents) return true;
    return list === d.ents && L.vports.some(function(vp){ var hw = vp.w * vp.scale / 2, hh = vp.h * vp.scale / 2; return hit([vp.c[0] - hw, vp.c[1] - hh, vp.c[0] + hw, vp.c[1] + hh]); });
  }
  TC.on('dirty', function(d){
    if (!d || !d.dirty || SS.docs.indexOf(d.id) < 0) return; var g = (d.undo || [])[(d.undo || []).length - 1]; if (!g || g._ss) return; g._ss = 1;
    sheets().forEach(function(s){ if (s.d === d && s.st === 'p' && g.ops.some(function(op){ return op.e && op.e._bb && touches(s, op.e, op.list || ED.listOf(d, op.e)); })) setMeta(s.key, { st:'m' }); });
    refresh();
  });
  function nav(s){
    if (!s) return;
    if (!s.d){ TC.toast(s.missing ? 'Không tìm thấy <b>' + esc(s.file) + '</b> tại ' + esc(s.dir) + '. Chỉ lại đường dẫn tệp trong bộ bản vẽ.' : 'Mở <b>' + esc(s.file) + '</b> › ' + esc(s.where) + ' (tệp mô phỏng trong prototype).', { type:s.missing ? 'warn' : 'info', log:false }); return; }
    if (A().active !== s.d.id) A().activate(s.d.id);
    if (s.sp === 'model'){ A().setSpace('model'); if (s.e){ V().zoomTo(s.e._bb); s.d.found = [s.e.id]; V().drawOver(); setTimeout(function(){ s.d.found = []; V().drawOver(); }, 2500); } }
    else A().setSpace(s.sp);
  }
  function printSet(){
    if (SS.job) return; var L = sheets(), i = 0, ok = 0, bad = [], job = SS.job = { stop:false, i:0, n:L.length };
    var t = setInterval(function(){
      if (job.stop || i >= L.length){ clearInterval(t); SS.job = null; A().setTask(''); refresh(); TC.toast((job.stop ? 'Đã hủy xuất bộ bản vẽ sau ' + ok + ' tờ' : 'Đã xuất ' + ok + '/' + L.length + ' tờ thành <b>' + esc(SS.name) + '_bo-ban-ve.pdf</b>') + (bad.length ? ' · ' + bad.length + ' tờ lỗi: không tìm thấy ' + esc(bad[0]) : '') + '.', { type:bad.length || job.stop ? 'warn' : 'ok', act:{ label:'Mở thư mục', fn:function(){} } }); return; }
      var s = L[i++]; job.i = i; if (s.missing) bad.push(s.file); else { ok++; setMeta(s.key, { st:'p', at:TC.date() }); }
      A().setTask('<span class="sb-task"><span class="spin"></span>Xuất bộ bản vẽ ' + i + '/' + L.length + '<span class="prog"><i style="width:' + Math.round(i / L.length * 100) + '%"></i></span><button class="ib sm" type="button" id="ss-x" aria-label="Hủy xuất bộ bản vẽ">' + ic('x') + '</button></span>');
      var x = $('#ss-x'); if (x) x.onclick = function(){ job.stop = true; }; refresh();
    }, 260);
    refresh();
  }
  function moveSheet(k, dir){ var L = sheets(), s = L.filter(function(x){ return x.key === k; })[0], g = L.filter(function(x){ return x.bm === s.bm; }), j = g.indexOf(s) + dir; if (j < 0 || j >= g.length) return; var a = SS.order.indexOf(k), b = SS.order.indexOf(g[j].key); SS.order[a] = g[j].key; SS.order[b] = k; SS.kb = 1; refresh(); }
  function pickSheet(k, kb){ var s = byKey(k), d = D(); SS.sel = k; SS.kb = kb; if (d && s && s.d === d && s.e && d.space === 'model'){ d.found = [s.e.id]; V().drawOver(); } refresh(); }
  function stTag(s){ return s.missing ? '<span class="tag danger" title="' + esc('Không tìm thấy ' + s.dir + '\\' + s.file) + '">' + ic('error', 's12') + 'Thiếu tệp</span>' : s.st === 'p' ? '<span class="tag ok" title="Đã in ' + esc(s.at) + '">' + ic('check', 's12') + 'Đã in</span>' : s.st === 'm' ? '<span class="tag warn" title="Đã in ' + esc(s.at) + ', bản vẽ sửa sau đó">' + ic('warn', 's12') + 'Sửa sau khi in</span>' : '<span class="faint">Chưa in</span>'; }
  P.sheets = { name:'Bộ bản vẽ', render:function(host){
    if (!TC.can('PRO')){ host.innerHTML = '<div class="empty">' + ic('sheetset') + '<b>Quản lý bộ bản vẽ</b>Đánh số tờ theo bộ môn, cập nhật khung tên hàng loạt, in và đóng gói cả bộ có trong TrionCAD Pro.<button class="btn primary sm" type="button" data-up style="margin-top:8px">Dùng thử Pro 30 ngày</button></div>'; host.onclick = function(e){ if (e.target.closest('[data-up]')) SC.upgrade('Quản lý bộ bản vẽ', 'PRO'); }; return; }
    var L = sheets(), F = TC.fold(SS.q), nf = {}, cur = null, old = $('.pal-body', host), top = old ? old.scrollTop : 0;
    L.forEach(function(s){ nf[s.file] = 1; if (s.key === SS.sel) cur = s; var w = tbVals(s); s.diff = s.e ? Object.keys(w).filter(function(t){ return hasTag(s.d, s.e.name, t) && String(s.at0[t]) !== w[t]; }) : []; });
    var stale = L.filter(function(s){ return s.diff.length; }).length, kt = L.filter(function(s){ return s.bm === 'KT'; }).length || 1, shown = L.filter(function(s){ return !F || TC.fold(s.no + ' ' + s.title + ' ' + s.file).indexOf(F) >= 0; });
    var pgh = function(k, label){ return '<button class="pgh" type="button" data-sg="' + k + '" aria-expanded="' + !!SS.open[k] + '">' + ic('down', 'xs') + label + '</button>'; };
    var bms = BMS.map(function(b){ return b; }); shown.forEach(function(s){ if (!bms.some(function(b){ return b[0] === s.bm; })) bms.push([s.bm, 'Bộ môn ' + s.bm]); });
    var rows = bms.map(function(b){ var g = shown.filter(function(s){ return s.bm === b[0]; }); if (!g.length) return ''; return '<div class="ssm-grp">' + esc(b[1]) + ' · ' + esc(b[0]) + '<span>' + g.length + ' tờ</span></div>' + g.map(function(s){
      return '<div class="ssm-row' + (s === cur ? ' is-sel' : '') + '" role="option" aria-selected="' + (s === cur) + '" tabindex="' + (s === cur || (!cur && s === shown[0]) ? 0 : -1) + '" draggable="true" data-sk="' + esc(s.key) + '" data-bm="' + esc(s.bm) + '" title="' + esc(s.file + ' › ' + s.where) + '"><span class="hd" aria-hidden="true">' + ic('drag', 's12') + '</span><span class="no mono">' + esc(s.no) + (s.diff.length ? '<i title="' + esc('Khung tên chưa khớp: ' + s.diff.join(', ')) + '"></i>' : '') + '</span><span class="t">' + esc(s.title) + '</span><span class="mono">' + esc(s.scale) + '</span><span>' + esc(s.paper) + '</span><span class="mono">' + esc(s.date) + '</span><span>' + stTag(s) + '</span></div>'; }).join(''); }).join('');
    var proj = pgh('proj', 'Thông tin dự án') + (SS.open.proj ? '<div class="ssm-sec"><div class="frow" style="--lab-w:104px">' + PJF.map(function(f){ var k = f[0], bad = k === 'CONGTRINH' && !PJ[k].trim();
        return '<label class="lab" for="pj-' + k + '">' + f[1] + '</label>' + (k === 'GIAIDOAN' ? '<select class="inp sm" id="pj-' + k + '" data-pj="' + k + '">' + SC.opts(STAGES, PJ[k]) + '</select>' : '<span class="stack" style="gap:2px"><input class="inp sm' + (bad ? ' err' : '') + '" id="pj-' + k + '" data-pj="' + k + '" value="' + esc(PJ[k]) + '" spellcheck="false">' + (bad ? '<span class="fmsg err">' + ic('warn', 'xs') + 'Nhập tên công trình để điền vào khung tên.</span>' : '') + '</span>'); }).join('') +
      '<span class="lab">Logo</span><span class="hrow" style="flex-wrap:nowrap"><canvas class="ssm-logo" width="56" height="22" aria-hidden="true"></canvas><span class="' + (PJ.logo ? 'mono ' : '') + 'faint grow ell" style="font-size:var(--fs-xs)">' + esc(PJ.logo || 'Chưa có logo') + '</span><button class="btn sm" type="button" data-sa="logo">' + ic('image') + 'Chọn ảnh…</button></span></div></div>' : '');
    var list = '<div class="ssm-h"><span class="gh">Danh sách tờ</span><label class="search grow">' + ic('search') + '<input data-sq placeholder="Tìm số hiệu, tên bản vẽ, tệp" value="' + esc(SS.q) + '" aria-label="Tìm tờ"></label>' + SC.seg('ssfmt', ['KT 01', 'KT 1/' + kt], SS.fmt === 'pad' ? 'KT 01' : 'KT 1/' + kt) + '</div>' +
      '<div class="ssm-upd"><span class="' + (stale ? 'warn' : 'ok') + '">' + ic(stale ? 'warn' : 'okc', 's12') + '</span><span class="grow">' + (stale ? stale + ' khung tên chưa khớp với bộ bản vẽ' : 'Mọi khung tên đã khớp với bộ bản vẽ') + '</span><button class="btn sm' + (stale ? ' primary' : '') + '" type="button" data-sa="upd">' + ic('refresh') + 'Cập nhật tất cả khung tên</button></div>' +
      '<div class="ssm-list" role="listbox" aria-label="Danh sách tờ"><div class="ssm-row ssm-hd" aria-hidden="true"><span></span><span>Số hiệu</span><span>Tên bản vẽ</span><span>Tỉ lệ</span><span>Khổ</span><span>Ngày</span><span>Trạng thái</span></div>' +
      (rows || '<div class="empty">' + (L.length ? 'Không có tờ khớp “' + esc(SS.q) + '”. Tìm không cần dấu: “mat bang” khớp “MẶT BẰNG”.' : '<b>Bộ bản vẽ chưa có tờ</b>Thêm layout và khung tên trong Model của bản vẽ đang mở.<button class="btn sm" type="button" data-sa="addcur" style="margin-top:6px">Thêm tờ từ bản vẽ hiện hành</button>') + '</div>') + '</div>';
    var map = pgh('map', 'Ánh xạ khung tên') + (SS.open.map ? '<div class="ssm-sec"><p class="faint" style="font-size:var(--fs-s)">Attribute của block khung tên ↔ thông tin dự án và thông tin tờ. Dùng chung với Khung tên (TCTITLE).</p><div class="frow" style="--lab-w:104px">' + TBF.map(function(f){ return '<label class="lab mono" for="mt-' + f[0] + '" title="' + esc(f[1]) + '">' + f[0] + '</label><select class="inp sm" id="mt-' + f[0] + '" data-mt="' + f[0] + '">' + SRC.map(function(s){ return '<option value="' + s[0] + '"' + ((SS.map[f[0]] || '') === s[0] ? ' selected' : '') + '>' + esc(s[1]) + '</option>'; }).join('') + '</select>'; }).join('') + '</div></div>' : '');
    var det = '<div class="ssm-det">' + (cur ? '<div class="hrow" style="flex-wrap:nowrap"><b class="mono">' + esc(cur.no) + '</b><input class="inp sm grow" data-st value="' + esc(cur.title) + '" aria-label="Tên bản vẽ" spellcheck="false"><select class="inp sm" data-sb aria-label="Bộ môn" style="width:72px">' + bms.map(function(b){ return '<option value="' + esc(b[0]) + '"' + (b[0] === cur.bm ? ' selected' : '') + '>' + esc(b[0]) + '</option>'; }).join('') + '</select></div>' +
        '<div class="faint ell">' + ic(cur.sp === 'model' ? 'model' : 'layout', 's12') + ' ' + esc(cur.file) + ' › ' + esc(cur.where) + (cur.e ? ' · khung ' + esc(cur.e.name) : '') + ' · ' + esc(cur.paper) + ' · ' + esc(cur.scale) + '</div>' +
        '<div class="hrow"><button class="btn sm" type="button" data-sa="open">' + ic('open') + (cur.d ? 'Mở tờ' : cur.missing ? 'Tìm tệp…' : 'Mở tệp') + '</button>' + (cur.e ? '<button class="btn sm ghost" type="button" data-sa="tb">' + ic('attedit') + 'Sửa khung tên…</button>' : '') + '<span class="grow"></span>' + stTag(cur) + (cur.at ? '<span class="faint mono" style="font-size:var(--fs-xs)">' + esc(cur.at) + '</span>' : '') + '</div>'
      : '<span class="faint">Chọn một tờ để xem chi tiết. Kéo hàng (hoặc Alt+↑↓) để sắp xếp; số hiệu tự đánh lại theo bộ môn.</span>') + '</div>';
    var job = SS.job, foot = '<div class="pal-foot ssm-acts">' + (job ? '<span class="spin"></span><span>Đang xuất ' + job.i + '/' + job.n + ' tờ</span><span class="prog grow"><i style="width:' + Math.round(job.i / job.n * 100) + '%"></i></span><button class="btn sm" type="button" data-sa="stop">Hủy</button>'
      : '<button class="btn sm" type="button" data-sa="print" aria-haspopup="menu">' + ic('print') + 'In cả bộ' + ic('down', 'xs') + '</button><button class="btn sm" type="button" data-sa="pack">' + ic('pkg') + 'Đóng gói gửi đi</button><button class="btn sm" type="button" data-sa="list" aria-haspopup="menu">' + ic('table') + 'Xuất danh mục' + ic('down', 'xs') + '</button>') + '</div>';
    host.innerHTML = '<div class="ssm"><div class="pal-tools"><span class="ssm-name grow">' + ic('sheetset') + '<b>' + esc(SS.name) + '</b><span class="faint">' + L.length + ' tờ · ' + Object.keys(nf).length + ' tệp</span></span><button class="ib" type="button" data-sa="add" aria-label="Thêm tờ" data-tip="Thêm tờ" aria-haspopup="menu">' + ic('plus') + '</button><button class="ib" type="button" data-sa="more" aria-label="Tùy chọn bộ bản vẽ" data-tip="Tùy chọn bộ bản vẽ" aria-haspopup="menu">' + ic('more') + '</button></div>' +
      '<div class="pal-body">' + proj + list + map + '</div>' + det + foot + '</div>';
    var lg = $('.ssm-logo', host); if (lg) logoPaint(lg, PJ.logo);
    $('.pal-body', host).scrollTop = top;
    if (SS.kb){ var fr = $('.ssm-row.is-sel', host); if (fr) fr.focus(); SS.kb = 0; }
    var act = function(k, b){ var r = b.getBoundingClientRect(), d = D();
      if (k === 'upd') updateAll();
      else if (k === 'logo') pickLogo(b, function(n){ PJ.logo = n; refresh(); });
      else if (k === 'print') TC.menu([{ label:'Xuất PDF cả bộ · ' + L.length + ' tờ', icon:'pdf', act:printSet }, { sep:1 }, { label:'In theo khung tên… <span class="faint">UI-H06</span>', icon:'batchplot', act:function(){ SC.batchPlot(); } }, { label:'In nhiều layout, nhiều bản vẽ… <span class="faint">UI-H04</span>', icon:'publish', act:function(){ SC.publish(); } }], r.left, r.top, { up:true, returnFocus:b, minWidth:260 });
      else if (k === 'stop'){ if (SS.job) SS.job.stop = true; }
      else if (k === 'pack') TC.cmd.run('ETRANSMIT');
      else if (k === 'list') TC.menu([{ label:'Bảng trong bản vẽ…', icon:'table', act:function(){ TC.cmd.run('TCSHEETS', 'T'); } }, { label:'Excel (.xlsx)…', icon:'stats', act:function(){ TC.toast('Đã xuất danh mục ' + L.length + ' tờ thành <b>' + esc(SS.name) + '_danh-muc-ban-ve.xlsx</b> (cột STT, số hiệu, tên bản vẽ, tỉ lệ, khổ giấy, ngày, trạng thái).', { act:{ label:'Mở thư mục', fn:function(){} } }); } }], r.left, r.top, { up:true, returnFocus:b, minWidth:220 });
      else if (k === 'open' && cur){ if (cur.missing) TC.toast('Hộp thoại chọn tệp: chỉ lại đường dẫn <b>' + esc(cur.file) + '</b>.', { type:'info', log:false }); else nav(cur); }
      else if (k === 'tb' && cur){ nav(cur); TC.cmd.run('TCTITLE', cur.e); }
      else if (k === 'addcur' || k === 'add'){ var items = [{ label:'Từ bản vẽ hiện hành' + (d ? ' <span class="faint">· ' + esc(d.name) + '</span>' : ''), icon:'file', dis:!d || SS.docs.indexOf(d.id) >= 0, act:function(){ SS.docs.push(d.id); refresh(); TC.toast('Đã thêm các layout và khung tên trong Model của <b>' + esc(d.name) + '</b> vào bộ.', { log:false }); } }, { label:'Từ tệp…', icon:'open', act:function(){ TC.toast('Chọn nhiều tệp DWG (UI-B01); tờ của tệp chưa mở được thêm mà không cần mở tệp.', { type:'info', log:false }); } }];
        if (k === 'addcur') items[0].act(); else TC.menu(items, r.left, r.bottom + 2, { returnFocus:b, minWidth:240 }); }
      else if (k === 'more') TC.menu([{ label:'Đổi tên bộ…', act:function(){ SC.ask('Đổi tên bộ bản vẽ', 'Tên bộ', SS.name, null, 'UI-H07').then(function(v){ if (v){ SS.name = v; refresh(); } }); } }, { label:'Số hiệu dạng', sub:[{ label:'KT 01', check:SS.fmt === 'pad', act:function(){ SS.fmt = 'pad'; refresh(); } }, { label:'KT 1/' + kt, check:SS.fmt === 'frac', act:function(){ SS.fmt = 'frac'; refresh(); } }] }, { label:'Bỏ trạng thái in của mọi tờ', act:function(){ L.forEach(function(s){ setMeta(s.key, { st:'', at:'' }); }); refresh(); } }, { sep:1 }, { label:'Lưu bộ bản vẽ', icon:'save', act:function(){ TC.toast('Đã lưu <span class="mono">' + esc(SS.name) + '.tcs</span> cạnh thư mục dự án.', { log:false }); } }, { label:'Mở bộ bản vẽ khác…', icon:'open', act:function(){ TC.toast('Chọn tệp bộ bản vẽ .tcs.', { type:'info', log:false }); } }], r.right, r.bottom + 2, { alignRight:true, returnFocus:b });
    };
    host.onclick = function(e){
      var g = e.target.closest('[data-sg]'); if (g){ SS.open[g.dataset.sg] = !SS.open[g.dataset.sg]; refresh(); return; }
      var sg = e.target.closest('[data-seg="ssfmt"] button'); if (sg){ SS.fmt = /\//.test(sg.dataset.v) ? 'frac' : 'pad'; refresh(); return; }
      var row = e.target.closest('[data-sk]'); if (row){ pickSheet(row.dataset.sk); return; }
      var b = e.target.closest('[data-sa]'); if (b) act(b.dataset.sa, b);
    };
    host.ondblclick = function(e){ var row = e.target.closest('[data-sk]'); if (row) nav(byKey(row.dataset.sk)); };
    host.onchange = function(e){ var t = e.target; if (t.dataset.pj){ PJ[t.dataset.pj] = t.value.trim(); refresh(); } else if (t.dataset.mt){ SS.map[t.dataset.mt] = t.value; refresh(); } else if (cur && t.hasAttribute('data-st')){ setMeta(cur.key, { title:t.value.trim() || null }); refresh(); } else if (cur && t.hasAttribute('data-sb')){ setMeta(cur.key, { bm:t.value }); refresh(); } };
    host.oninput = function(e){ if (!e.target.hasAttribute('data-sq')) return; SS.q = e.target.value; refresh(); var q = $('[data-sq]', host); if (q){ q.focus(); q.setSelectionRange(q.value.length, q.value.length); } };
    host.onkeydown = function(e){ var row = e.target.closest && e.target.closest('[data-sk]'); if (!row) return; var all = $$('[data-sk]', host), i = all.indexOf(row);
      if (e.key === 'ArrowDown' || e.key === 'ArrowUp'){ e.preventDefault(); var dir = e.key === 'ArrowDown' ? 1 : -1; if (e.altKey) moveSheet(row.dataset.sk, dir); else pickSheet(all[TC.clamp(i + dir, 0, all.length - 1)].dataset.sk, 1); }
      else if (e.key === 'Enter'){ e.preventDefault(); nav(byKey(row.dataset.sk)); } };
    // kéo để sắp xếp trong cùng bộ môn; số hiệu tự đánh lại
    var drag = null, clear = function(){ $$('.drop-b, .drop-a, .dragging', host).forEach(function(x){ x.classList.remove('drop-b', 'drop-a', 'dragging'); }); };
    host.ondragstart = function(e){ var r = e.target.closest && e.target.closest('[data-sk]'); if (!r) return; drag = r; r.classList.add('dragging'); e.dataTransfer.effectAllowed = 'move'; e.dataTransfer.setData('text/plain', r.dataset.sk); };
    host.ondragover = function(e){ var r = e.target.closest && e.target.closest('[data-sk]'); $$('.drop-b, .drop-a', host).forEach(function(x){ x.classList.remove('drop-b', 'drop-a'); }); if (!drag || !r || r === drag || r.dataset.bm !== drag.dataset.bm) return; e.preventDefault(); var b = r.getBoundingClientRect(); r.classList.add(e.clientY > b.top + b.height / 2 ? 'drop-a' : 'drop-b'); };
    host.ondrop = function(e){ var r = e.target.closest && e.target.closest('[data-sk]'); if (!drag || !r || r === drag) return; e.preventDefault(); var after = r.classList.contains('drop-a'), k = drag.dataset.sk; SS.order.splice(SS.order.indexOf(k), 1); var j = SS.order.indexOf(r.dataset.sk); SS.order.splice(after ? j + 1 : j, 0, k); drag = null; clear(); SS.sel = k; refresh(); };
    host.ondragend = function(){ drag = null; clear(); };
    host.oncontextmenu = function(e){ var r = e.target.closest('[data-sk]'); if (!r) return; e.preventDefault(); var s = byKey(r.dataset.sk); SS.sel = s.key;
      TC.menu([{ label:'Mở tờ', icon:'open', kbd:'Enter', act:function(){ nav(s); } }, { label:'Sửa khung tên…', icon:'attedit', dis:!s.e, act:function(){ nav(s); TC.cmd.run('TCTITLE', s.e); } }, { sep:1 },
        { label:'Bộ môn', sub:BMS.map(function(b){ return { label:b[0] + ' · ' + b[1], check:s.bm === b[0], act:function(){ setMeta(s.key, { bm:b[0] }); refresh(); } }; }) }, { label:'Lên', kbd:'Alt+↑', act:function(){ moveSheet(s.key, -1); } }, { label:'Xuống', kbd:'Alt+↓', act:function(){ moveSheet(s.key, 1); } },
        { label:s.st ? 'Bỏ trạng thái đã in' : 'Đánh dấu đã in', act:function(){ setMeta(s.key, s.st ? { st:'', at:'' } : { st:'p', at:TC.date() }); refresh(); } }, { sep:1 },
        { label:'Bỏ khỏi bộ bản vẽ', icon:'trash', act:function(){ setMeta(s.key, { hidden:true }); SS.sel = null; refresh(); TC.toast('Đã bỏ tờ <b>' + esc(s.no + ' ' + s.title) + '</b> khỏi bộ. Tệp và khung tên không đổi.', { log:false, act:{ label:'Hoàn tác', fn:function(){ setMeta(s.key, { hidden:false }); refresh(); } } }); } }], e.clientX, e.clientY); refresh(); };
  } };
  // bảng danh mục bản vẽ chèn vào bản vẽ (TCSHEETS T): mm giấy × tỉ lệ chú thích trong Model
  function listTable(p, k){ var L = sheets(); return { t:'table', p:p, cols:[10, 18, 84, 16, 12].map(function(x){ return x * k; }), rows:[9, 7].concat(L.map(function(){ return 6; })).map(function(x){ return x * k; }), th:2.5 * k, head:2, cells:[['DANH MỤC BẢN VẼ', '', '', '', ''], ['STT', 'SỐ HIỆU', 'TÊN BẢN VẼ', 'TỈ LỆ', 'KHỔ']].concat(L.map(function(s, i){ return [String(i + 1), s.no, s.title, s.scale, s.paper]; })), merge:[[0, 0, 4]], style:'Standard', layer:'KT-TEXT' }; }
  IMPL.TCSHEETS = function(ctx, pre){
    if (pre === 'T'){ var d = D(), k = d.space === 'model' || d.activeVp ? 100 : 1;
      return ctx.point('Điểm chèn bảng danh mục (góc trên trái)', { onMove:function(p, temp){ temp.push(ED.temp([listTable(p, k)], { real:true })); } }).then(function(r){ if (!r.p) return; ED.run('TCSHEETS', function(){ ED.add(ED.make(listTable(r.p, k))); }); V().draw(); ctx.out('Đã chèn bảng danh mục ' + sheets().length + ' tờ. Xuất lại ra Excel bằng TCEXCELOUT.'); }); }
    var app = A(); if (app.pal.left.indexOf('sheets') < 0 && app.pal.right.indexOf('sheets') < 0) app.pal.left.push('sheets');
    app.showPalette('sheets'); var side = app.pal.left.indexOf('sheets') >= 0 ? 'l' : 'r', h = $('#dock-' + side + ' .palhost');
    if (h && h.hidden){ h.hidden = false; $$('#dock-' + side + ' .vtab').forEach(function(v){ v.setAttribute('aria-expanded', String(v.dataset.fly === 'sheets')); }); }
    var L = sheets(); ctx.out('Bộ bản vẽ <b>' + esc(SS.name) + '</b>: ' + L.length + ' tờ. Kéo để sắp xếp, “Cập nhật tất cả khung tên” để ghi số hiệu và thông tin dự án vào khung tên.');
  };
  // khung tên: menu chuột phải trên block khung tên
  TC.cad.ctxExt.push(function(it, first, d){ if (first && first.t === 'insert' && isTB(d, first.name) && !TC.isViewer()) it.push({ label:'Sửa khung tên…', icon:'attedit', act:function(){ TC.cmd.run('TCTITLE', first); } }); });

  /* ================= Khung tên UI-J05: trình sửa đầy đủ, ánh xạ, logo, mẫu công ty (v1.0) ================= */
  var SHEETK = { TENBV:1, KYHIEU:1, TILE:1, SOTO:1 }, PEOPLE = { CHUNHIEM:1, CHUTRI:1, THIETKE:1, VE:1, KIEMTRA:1, DUYET:1 };
  var TI = { tpl:'tcvn', size:'A3', por:false, sc:'1:100', bm:'KT', fill:true };
  IMPL.TCTITLE = function(ctx, pre){
    var d = D(), all = [];
    d.ents.forEach(function(e){ if (e.t === 'insert' && isTB(d, e.name)) all.push({ e:e, where:'Model' }); }); d.layouts.forEach(function(L){ L.ents.forEach(function(e){ if (e.t === 'insert' && isTB(d, e.name)) all.push({ e:e, where:L.name }); }); });
    var cur = all.filter(function(x){ return x.e === pre; })[0] || all[0], tab = all.length ? 'e' : 'i', logo = PJ.logo, scope = 'one', inSet = SS.docs.indexOf(d.id) >= 0;
    TI.sc = d.space !== 'model' && !d.activeVp ? '1:1 (trong layout)' : /1:1 /.test(TI.sc) ? '1:100' : TI.sc;
    return new Promise(function(res){
      var dlg = TC.dialog({ id:'tct', title:'Khung tên', cmd:'TCTITLE', code:'UI-J05', w:940, h:660, onClose:function(r){ V().temp = []; V().drawOver(); if (r !== true) res(); },
        body:H.tabs([['e', 'Sửa khung tên (' + all.length + ')'], ['m', 'Ánh xạ attribute'], ['i', 'Chèn khung tên']], tab, 'tct') + '<div class="tct-b">' + H.pane('e', tab, '<div class="tce"><div class="stack" style="gap:4px"><span class="gh">Khung tên trong bản vẽ</span><div class="tree" id="tc-l"></div></div><div class="stack" id="tc-f" style="gap:10px;min-width:0"></div></div>') + H.pane('m', tab, '<div id="tc-m"></div>') + H.pane('i', tab, '<div id="tc-i"></div>') + '</div>',
        foot:'<button class="btn ghost" type="button" id="tc-save">' + ic('save') + 'Lưu thành mẫu công ty…</button><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Đóng</button><button class="btn primary" type="button" id="tc-ok"></button>' });
      var $d = function(s){ return $(s, dlg.el); }, same = function(){ return all.filter(function(x){ return x.e.name === cur.e.name; }).length; };
      function okLabel(){ var b = $d('#tc-ok'); b.disabled = tab !== 'i' && !cur; b.textContent = tab === 'i' ? 'Chèn khung tên' : scope === 'one' ? 'Áp dụng cho 1 khung' : scope === 'doc' ? 'Áp dụng cho ' + same() + ' khung' : 'Áp dụng cho bộ (' + sheets().length + ' tờ)'; }
      function list(){ $d('#tc-l').innerHTML = all.map(function(x, i){ var a = attrsOf(d, x.e); return '<button class="tn" type="button" data-tc="' + i + '" aria-selected="' + (x === cur) + '">' + ic('layout', 's12') + esc(a.KYHIEU || x.e.name) + '<span class="cnt">' + esc(x.where) + '</span></button>'; }).join('') || '<span class="faint">Chưa có khung tên.</span>'; form(); }
      function form(){
        var box = $d('#tc-f'); okLabel();
        if (!cur){ box.innerHTML = '<div class="empty">' + ic('layout') + '<b>Bản vẽ chưa có khung tên</b>Chèn khung tên TCVN 5571 có attribute để quản lý thông tin tờ.<button class="btn sm" type="button" data-go="i" style="margin-top:6px">Chèn khung tên</button></div>'; return; }
        var a = attrsOf(d, cur.e), miss = missingTags(d, cur.e.name), s = inSet ? sheetFor(d, cur.e) : null, want = s ? tbVals(s) : projVals(), nd = 0;
        var fld = function(f){ var t = f[0], v = a[t] != null ? a[t] : (cur.e.attrs || {})[t] || '', w = want[t], st = SS.map[t], diff = w != null && String(w) !== String(v); if (diff) nd++;
          return '<label class="lab" for="tcf-' + t + '">' + f[1] + '</label><span class="fi"><input class="inp" id="tcf-' + t + '" data-tf="' + t + '" value="' + esc(v) + '"' + (w != null ? ' data-want="' + esc(w) + '"' : '') + ' spellcheck="false">' + (miss.indexOf(t) >= 0 ? '<span class="tag info" title="Ô mới của mẫu v1.0">mới</span>' : st ? '<span class="tag' + (diff ? ' warn' : '') + '" title="' + esc(srcLabel(st) + (diff ? ': ' + w : '')) + '">' + (st[0] === 'p' ? 'Dự án' : 'Tờ') + '</span>' : '<span class="tag tg0" aria-hidden="true"></span>') + '</span>'; };
        box.innerHTML = '<div class="hrow" style="flex-wrap:nowrap"><span class="tag">' + esc(cur.e.name) + '</span><span class="faint ell grow" style="font-size:var(--fs-s)">' + esc(cur.where) + (s ? ' · tờ ' + esc(s.no) + ' trong bộ ' + esc(SS.name) : inSet ? '' : ' · bản vẽ chưa thuộc bộ bản vẽ, chỉ tự điền thông tin dự án') + '</span><button class="btn sm" type="button" id="tc-fill">' + ic('refresh') + 'Tự điền <b id="tc-nd"></b> ô từ bộ bản vẽ</button></div>' +
          (miss.length ? '<div class="note warn">' + ic('warn') + '<div class="stack" style="gap:4px"><span>Khung <b>' + esc(cur.e.name) + '</b> thiếu ' + miss.length + ' ô so với mẫu TCVN 5571 v1.0: ' + miss.map(tbLabel).join(', ') + '.</span>' + H.chk('tc-up', 'Khi áp dụng, cập nhật định nghĩa khung theo mẫu v1.0 (giữ nội dung các ô cũ, thêm ô logo)', true) + '</div></div>' : '') +
          '<div class="tcf2"><div class="tcg">' + H.gh('Công trình và tờ').replace('class="gh"', 'class="gh" style="grid-column:1 / -1"') + TBF.filter(function(f){ return !PEOPLE[f[0]]; }).map(fld).join('') + '</div><div class="tcg">' + H.gh('Người thực hiện').replace('class="gh"', 'class="gh" style="grid-column:1 / -1"') + TBF.filter(function(f){ return PEOPLE[f[0]]; }).map(fld).join('') +
            '<span class="lab">Logo</span><span class="stack" style="gap:4px" id="tc-lgw"></span></div></div>' +
          '<span class="fmsg" id="tc-msg" aria-live="polite"></span>' +
          H.grp('Áp dụng cho', H.rad('tcs', 'one', 'Chỉ khung này', scope === 'one') + H.rad('tcs', 'doc', 'Các ô chung cho mọi khung <b>' + esc(cur.e.name) + '</b> trong bản vẽ (' + same() + ' khung)', scope === 'doc') + (inSet ? H.rad('tcs', 'set', 'Các ô chung cho cả bộ bản vẽ <b>' + esc(SS.name) + '</b> (' + sheets().length + ' tờ, UI-H07): ghi thông tin dự án, số hiệu, số tờ', scope === 'set') : '<label class="chk dis"><input type="radio" disabled> Cả bộ bản vẽ: thêm bản vẽ vào bộ ở Quản lý bộ bản vẽ (TCSHEETS)</label>'));
        logoRow(); $d('#tc-nd').textContent = nd; $d('#tc-fill').disabled = !nd;
        V().temp = cur.where === 'Model' ? [{ k:'poly', pts:rp(cur.e._bb[0], cur.e._bb[1], cur.e._bb[2], cur.e._bb[3]), closed:true, color:TC.cssv('--c-accent'), w:2 }] : []; V().drawOver();
      }
      function fillAll(){ $$('[data-want]', dlg.el).forEach(function(i){ i.value = i.dataset.want; }); $$('.tcf2 .tag.warn', dlg.el).forEach(function(x){ x.classList.remove('warn'); }); var nd = $d('#tc-nd'); if (nd){ nd.textContent = 0; $d('#tc-fill').disabled = true; } }
      function logoRow(){ var w = $d('#tc-lgw'); if (!w) return; w.innerHTML = '<span class="hrow" style="flex-wrap:nowrap"><canvas width="64" height="26" class="ssm-logo" id="tc-lg" aria-hidden="true"></canvas><span class="' + (logo ? 'mono ' : '') + 'faint ell" style="font-size:var(--fs-xs)">' + esc(logo || 'Chưa có logo') + '</span></span><span class="hrow"><button class="btn sm" type="button" id="tc-lgb">' + ic('image') + 'Chọn ảnh logo…</button>' + (logo ? '<button class="btn sm ghost" type="button" id="tc-lgx">Bỏ logo</button>' : '') + '</span>'; logoPaint($d('#tc-lg'), logo); }
      function mapPane(){ var s = cur && inSet ? sheetFor(d, cur.e) : null;
        $d('#tc-m').innerHTML = '<div class="stack"><p class="faint" style="font-size:var(--fs-s)">Mỗi attribute của khung tên lấy giá trị từ thông tin dự án hoặc thông tin tờ của bộ bản vẽ (UI-H07). Ánh xạ dùng chung cho Quản lý bộ bản vẽ và Tạo khung in (TCFRAME).</p><div class="tblwrap" style="max-height:420px"><table class="tbl"><colgroup><col style="width:110px"><col style="width:150px"><col style="width:230px"><col></colgroup><thead><tr><th>Attribute</th><th>Ô trong khung</th><th>Nguồn dữ liệu</th><th>Giá trị' + (cur ? ' · ' + esc(attrsOf(d, cur.e).KYHIEU || '') : '') + '</th></tr></thead><tbody>' +
          TBF.map(function(f){ var src = SS.map[f[0]] || '', v = src ? srcVal(src, s || {}) : (cur ? attrsOf(d, cur.e)[f[0]] : ''); return '<tr><td class="mono">' + f[0] + '</td><td>' + esc(f[1]) + (cur && !hasTag(d, cur.e.name, f[0]) ? ' <span class="tag info">mẫu v1.0</span>' : '') + '</td><td><select class="inp sm" data-tm="' + f[0] + '" aria-label="Nguồn cho ' + f[0] + '" style="width:100%">' + SRC.map(function(o){ return '<option value="' + o[0] + '"' + (o[0] === src ? ' selected' : '') + '>' + esc(o[1]) + '</option>'; }).join('') + '</select></td><td class="' + (src ? '' : 'faint') + '">' + esc(v == null || v === '' ? (src[0] === 's' && !s ? '(theo tờ khi thuộc bộ)' : '—') : v) + '</td></tr>'; }).join('') + '</tbody></table></div></div>'; }
      function insAttrs(){ var n = sheets().filter(function(s){ return s.bm === TI.bm; }).length + 1; return Object.assign(TI.fill ? projVals() : {}, { TENBV:'TÊN BẢN VẼ', KYHIEU:nextNo(TI.bm), TILE:/1:1 /.test(TI.sc) ? '1:100' : TI.sc, NGAY:TC.date(), SOTO:n + '/' + n }); }
      function insName(){ var tp = TI.tpl.indexOf('co:') === 0 ? SS.tpls.filter(function(t){ return 'co:' + t.name === TI.tpl; })[0] : null; return ensureFrame(d, TI.size, TI.por, tp); }
      function insPane(){ var box = $d('#tc-i');
        box.innerHTML = '<div class="tci"><div class="stack">' + H.grp('Mẫu', [['tcvn', 'TCVN 5571 · mẫu TrionCAD (15 ô, ô logo)']].concat(SS.tpls.map(function(t){ return ['co:' + t.name, 'Mẫu công ty: <b>' + esc(t.name) + '</b> <span class="faint">· lưu ' + esc(t.at) + '</span>']; })).map(function(x){ return H.rad('tcm', x[0], x[1], TI.tpl === x[0]); }).join('')) +
          '<div class="hrow">' + H.row('Khổ giấy', H.sel('tc-p', ['A0', 'A1', 'A2', 'A3', 'A4'], TI.size, 80), 70) + SC.seg('tcor', ['Ngang', 'Dọc'], TI.por ? 'Dọc' : 'Ngang') + '</div>' + H.row('Tỉ lệ', H.sel('tc-s', ['1:1 (trong layout)', '1:20', '1:50', '1:100', '1:200'], TI.sc, 170), 70) + H.row('Bộ môn', '<select class="inp" id="tc-bm" style="width:170px">' + BMS.map(function(b){ return '<option value="' + b[0] + '"' + (b[0] === TI.bm ? ' selected' : '') + '>' + b[0] + ' · ' + b[1] + '</option>'; }).join('') + '</select>', 70) +
          H.chk('tc-fill2', 'Tự điền từ thông tin dự án (UI-H07) và field: số hiệu <b class="mono" id="tc-no"></b>, ngày ' + TC.date(), TI.fill) + '<p class="faint" style="font-size:var(--fs-s)">Trong Model, khung phóng theo tỉ lệ (1:100 → ×100) để in hàng loạt theo khung tên (UI-H06).</p></div><div class="stack" style="gap:6px"><canvas id="tc-pv" width="300" height="212" class="pvc"></canvas><span class="faint" style="font-size:var(--fs-s)" id="tc-pvi"></span></div></div>'; insPv(); }
      function insPv(){ var nm = insName(), at = TI.fill ? insAttrs() : {}; $d('#tc-no').textContent = insAttrs().KYHIEU; tbPaint($d('#tc-pv'), d, nm, at); var p = PAPER[TI.size]; $d('#tc-pvi').textContent = 'Block ' + nm + ' · ' + (TI.por ? p[1] + ' × ' + p[0] : p[0] + ' × ' + p[1]) + ' mm giấy · ' + (/1:1 /.test(TI.sc) ? 'chèn 1:1 trong layout' : 'chèn ×' + TI.sc.split(':')[1] + ' trong Model'); }
      function show(){ okLabel(); if (tab === 'e') list(); else if (tab === 'm') mapPane(); else insPane(); }
      function apply(){
        var v = {}, msg = $d('#tc-msg'); $$('[data-tf]', dlg.el).forEach(function(i){ v[i.dataset.tf] = i.value.trim(); });
        var dup = all.filter(function(x){ return x !== cur && v.KYHIEU && attrsOf(d, x.e).KYHIEU === v.KYHIEU; })[0];
        if (dup && scope !== 'set'){ msg.className = 'fmsg err'; msg.innerHTML = ic('warn', 'xs') + 'Ký hiệu ' + esc(v.KYHIEU) + ' đã dùng ở khung ' + esc(dup.where) + '. Đổi ký hiệu hoặc áp dụng cho cả bộ để đánh số tự động.'; $d('#tcf-KYHIEU').classList.add('err'); $d('#tcf-KYHIEU').focus(); return; }
        var name = cur.e.name, up = $d('#tc-up') && $d('#tc-up').checked, logoCh = logo !== PJ.logo, common = {}, n = 0;
        Object.keys(v).forEach(function(t){ if (!SHEETK[t]) common[t] = v[t]; });
        if (scope === 'set'){ Object.keys(v).forEach(function(t){ var sm = SS.map[t]; if (sm && sm[0] === 'p') PJ[sm.slice(2)] = v[t]; }); var s = sheetFor(d, cur.e); if (s && v.TENBV && v.TENBV !== s.title) setMeta(s.key, { title:v.TENBV }); }
        PJ.logo = logo;
        ED.run('TCTITLE', function(){
          if (up || (logoCh && hasTag(d, name, 'SOTO'))) (scope === 'one' ? [name] : all.map(function(x){ return x.e.name; }).filter(function(nm, i, a2){ return a2.indexOf(nm) === i && (scope === 'set' || nm === name); })).forEach(function(nm){ redefine(d, nm); });
          all.forEach(function(x){ if (x === cur || (scope !== 'one' && (scope === 'set' || x.e.name === name))){ ED.mod(x.e, function(z){ z.attrs = Object.assign({}, z.attrs, x === cur ? v : common); }); n++; } });
        });
        if (scope === 'set') updateAll({ THIETKE:v.THIETKE, VE:v.VE, DUYET:v.DUYET });
        V().draw(); refresh(); if (scope !== 'set') TC.toast('Đã cập nhật ' + n + ' khung tên' + (up ? ', định nghĩa ' + esc(name) + ' theo mẫu v1.0' : '') + (logoCh && logo ? ', logo ' + esc(logo) : '') + '. Hoàn tác bằng Ctrl+Z.', { log:false });
        all.forEach(function(x){ x.e = d.byId[x.e.id] || x.e; }); show();
      }
      dlg.el.addEventListener('click', function(e){
        var t = e.target.closest('[data-tc]'); if (t){ cur = all[Number(t.dataset.tc)]; list(); return; }
        var g = e.target.closest('[data-go]'); if (g){ $('[data-dt="' + g.dataset.go + '"]', dlg.el).click(); return; }
        var o = e.target.closest('[data-seg="tcor"] button'); if (o){ TI.por = o.dataset.v === 'Dọc'; insPv(); return; }
        if (e.target.closest('#tc-fill')){ fillAll(); return; }
        if (e.target.closest('#tc-lgb')) pickLogo(e.target.closest('#tc-lgb'), function(n){ logo = n; logoRow(); });
        if (e.target.closest('#tc-lgx')){ logo = ''; logoRow(); }
      });
      dlg.el.addEventListener('change', function(e){ var t = e.target;
        if (t.name === 'tcs'){ scope = t.value; if (scope === 'set') fillAll(); okLabel(); } else if (t.dataset.tm){ SS.map[t.dataset.tm] = t.value; mapPane(); refresh(); }
        else if (t.name === 'tcm'){ TI.tpl = t.value; insPv(); } else if (t.id === 'tc-p'){ TI.size = t.value; insPv(); } else if (t.id === 'tc-s'){ TI.sc = t.value; insPv(); } else if (t.id === 'tc-bm'){ TI.bm = t.value; insPv(); } else if (t.id === 'tc-fill2'){ TI.fill = t.checked; insPv(); } });
      dlg.el.addEventListener('input', function(e){ if (e.target.dataset.tf === 'KYHIEU'){ e.target.classList.remove('err'); $d('#tc-msg').textContent = ''; } });
      dlg.el.addEventListener('dtab', function(e){ tab = e.detail; $$('.tct-b > [data-dp]', dlg.el).forEach(function(p){ p.hidden = p.dataset.dp !== tab; }); show(); });
      $d('#tc-save').addEventListener('click', function(){ SC.ask('Lưu thành mẫu công ty', 'Tên mẫu khung tên', 'An Phú · ' + TI.size, function(v){ return SS.tpls.some(function(t){ return t.name === v; }) ? 'Đã có mẫu “' + v + '”.' : null; }, 'UI-J05').then(function(nm){ if (!nm) return; SS.tpls.push({ name:nm, logo:logo || PJ.logo, at:TC.date() }); TC.toast('Đã lưu mẫu <b>' + esc(nm) + '</b> vào <span class="mono">\\\\SRV-01\\Chung\\Mau-khung-ten</span>, dùng chung cho cả công ty. Chọn ở tab Chèn khung tên hoặc Tạo khung in (TCFRAME).', { log:false }); if (tab === 'i') insPane(); }); });
      $d('#tc-ok').addEventListener('click', function(){
        if (tab !== 'i'){ if (cur) apply(); return; }
        var nm = insName(), at = TI.fill ? insAttrs() : { TILE:TI.sc }, k = /1:1 /.test(TI.sc) ? 1 : Number(TI.sc.split(':')[1]), mk = function(p){ return { t:'insert', name:nm, p:p, sx:k, sy:k, rot:0, layer:'KT-KHUNG', attrs:at }; };
        dlg.close(true);
        ctx.point('Điểm chèn khung tên (góc dưới trái)', { onMove:function(p, temp){ temp.push(ED.temp([mk(p)], { real:true })); } }).then(function(r){ if (r.p){ ED.run('TCTITLE', function(){ ED.add(ED.make(mk(r.p))); }); V().draw(); refresh(); ctx.out('Đã chèn khung tên ' + esc(nm) + ' (' + esc(at.KYHIEU || '') + ')' + (k > 1 ? ' theo tỉ lệ 1:' + k : '') + '. Sửa nội dung bằng TCTITLE › Sửa khung tên.'); } res(); }, function(){ res(); });
      });
      show();
    });
  };

  /* ================= Tạo khung in trong Model theo tỉ lệ UI-J06 (TCFRAME) ================= */
  var FR = { size:'A3', por:false, sc:100, tpl:'tcvn', mode:'arr', n:2, m:1, gap:20, auto:true, bm:'KT', fill:true };
  IMPL.TCFRAME = function(ctx){
    var d = D(); if (d.space !== 'model' || d.activeVp){ ctx.out('TCFRAME đặt khung trong Model. Chuyển sang tab Model rồi chạy lại; trong layout dùng Khung tên (TCTITLE).'); return; }
    var inSet = SS.docs.indexOf(d.id) >= 0, start = function(){ return inSet ? sheets().filter(function(s){ return s.bm === FR.bm; }).length + 1 : 1; };
    return new Promise(function(res){
      var dlg = TC.dialog({ id:'tcf', title:'Tạo khung in trong Model theo tỉ lệ', cmd:'TCFRAME', code:'UI-J06', w:800, onClose:function(r){ if (r !== true) res(); },
        body:'<div class="tfw"><div class="stack">' + H.grp('Khổ giấy, tỉ lệ, mẫu', '<div class="hrow">' + H.row('Khổ giấy', H.sel('tf-p', ['A0', 'A1', 'A2', 'A3', 'A4'], FR.size, 80), 80) + SC.seg('tfor', ['Ngang', 'Dọc'], FR.por ? 'Dọc' : 'Ngang') + '</div>' + H.row('Tỉ lệ', H.sel('tf-s', ['1:20', '1:25', '1:50', '1:75', '1:100', '1:200', '1:500'], '1:' + FR.sc, 120), 80) + H.row('Mẫu khung tên', '<select class="inp" id="tf-t" style="width:260px"><option value="tcvn">TCVN 5571 · mẫu TrionCAD</option>' + SS.tpls.map(function(t){ return '<option value="co:' + esc(t.name) + '"' + (FR.tpl === 'co:' + t.name ? ' selected' : '') + '>Mẫu công ty: ' + esc(t.name) + '</option>'; }).join('') + '</select>', 80)) +
          H.grp('Cách đặt', H.rad('tfm', 'one', 'Từng khung: chỉ điểm chèn từng khung, Enter để kết thúc', FR.mode === 'one') + H.rad('tfm', 'arr', 'Mảng nhiều khung', FR.mode === 'arr') + '<div class="hrow" style="padding-left:20px" id="tf-arr">' + H.num('tf-n', String(FR.n), { prec:0, unit:'none', w:64, label:'Số cột' }) + '<span>cột ×</span>' + H.num('tf-m', String(FR.m), { prec:0, unit:'none', w:64, label:'Số hàng' }) + '<span>hàng, cách</span>' + H.num('tf-g', String(FR.gap), { prec:0, unit:'none', w:72, label:'Khoảng cách giữa khung' }) + '<span>mm giấy</span></div><span class="fmsg" id="tf-msg" aria-live="polite"></span>') +
          H.grp('Số tờ', H.chk('tf-auto', 'Tự đánh số tờ', FR.auto) + '<div class="hrow">' + H.row('Bộ môn', '<select class="inp" id="tf-bm" style="width:140px">' + BMS.map(function(b){ return '<option value="' + b[0] + '"' + (b[0] === FR.bm ? ' selected' : '') + '>' + b[0] + ' · ' + b[1] + '</option>'; }).join('') + '</select>', 80) + H.row('Bắt đầu', H.num('tf-st', String(start()), { prec:0, unit:'none', w:64, label:'Số tờ bắt đầu' }), 56) + '</div><span class="names" id="tf-nos"></span>' + H.chk('tf-fill', 'Tự điền thông tin dự án vào khung tên (UI-H07)', FR.fill)) +
        '</div><div class="stack" style="gap:6px"><span class="gh">Xem trước khung</span><canvas id="tf-pv" width="300" height="212" class="pvc"></canvas><div class="frow" style="--lab-w:118px;font-size:var(--fs-s)" id="tf-info"></div></div></div>',
        foot:'<span class="faint" style="font-size:var(--fs-s)">Kết quả là block khung tên có attribute, in ngay được bằng In theo khung tên (UI-H06).</span><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="tf-ok"></button>' });
      var $d = function(s){ return $(s, dlg.el); };
      function nm(){ var tp = FR.tpl.indexOf('co:') === 0 ? SS.tpls.filter(function(t){ return 'co:' + t.name === FR.tpl; })[0] : null; return ensureFrame(d, FR.size, FR.por, tp); }
      function count(){ return FR.mode === 'arr' ? FR.n * FR.m : 1; }
      function noAt(i, total){ var k = (FR.st || 1) + i; return FR.bm + ' ' + (SS.fmt === 'pad' ? ('0' + k).slice(-2) : k + '/' + total); }
      function read(){ var bad = null, g = function(id, lo, hi, lab){ var v = num($d('#' + id).value, NaN); if (!(v >= lo && v <= hi) || Math.round(v) !== v){ bad = bad || lab + ' từ ' + lo + ' đến ' + hi + '.'; $d('#' + id).closest('.numf').classList.add('err'); return null; } $d('#' + id).closest('.numf').classList.remove('err'); return v; };
        FR.n = g('tf-n', 1, 20, 'Số cột') || FR.n; FR.m = g('tf-m', 1, 20, 'Số hàng') || FR.m; var gp = g('tf-g', 0, 500, 'Khoảng cách'); if (gp != null) FR.gap = gp; FR.st = g('tf-st', 1, 999, 'Số bắt đầu') || 1;
        var m = $d('#tf-msg'); m.className = 'fmsg ' + (bad ? 'err' : ''); m.innerHTML = bad ? ic('warn', 'xs') + 'Nhập ' + esc(bad.charAt(0).toLowerCase() + bad.slice(1)) : ''; $d('#tf-ok').disabled = !!bad; return !bad; }
      function upd(){ read(); $$('#tf-arr .numf input', dlg.el).forEach(function(i){ i.disabled = FR.mode !== 'arr'; }); var total = (inSet ? start() - 1 : 0) + count(), n = count(), p = PAPER[FR.size], W = FR.por ? p[1] : p[0], Hh = FR.por ? p[0] : p[1];
        $d('#tf-nos').innerHTML = FR.auto ? '<span class="mono">' + [0, 1, 2].slice(0, Math.min(n, 3)).map(function(i){ return esc(noAt(i, total)); }).join(', ') + (n > 3 ? ' … ' + esc(noAt(n - 1, total)) : '') + (FR.mode === 'one' ? ' …' : '') + '</span>' : '<span class="faint">Không đánh số: ô Ký hiệu để trống.</span>';
        tbPaint($d('#tf-pv'), d, nm(), attrsFor(0, total));
        $d('#tf-info').innerHTML = '<span class="lab">Khung trong Model</span><span class="mono">' + TC.n(W * FR.sc) + ' × ' + TC.n(Hh * FR.sc) + '</span><span class="lab">Vùng vẽ trong khung</span><span class="mono">' + TC.n((W - 30) * FR.sc) + ' × ' + TC.n((Hh - 10) * FR.sc) + '</span><span class="lab">Block</span><span class="mono">' + esc(nm()) + '</span><span class="lab">Số khung</span><span>' + (FR.mode === 'arr' ? n + ' (' + FR.n + ' × ' + FR.m + ')' : 'chỉ điểm từng khung') + '</span>';
        $d('#tf-ok').textContent = FR.mode === 'arr' && n > 1 ? 'Đặt ' + n + ' khung' : 'Đặt khung'; }
      function attrsFor(i, total){ var no = FR.auto ? noAt(i, total) : ''; return Object.assign(FR.fill ? projVals() : {}, { TENBV:'TÊN BẢN VẼ', KYHIEU:no, TILE:'1:' + FR.sc, NGAY:TC.date(), SOTO:FR.auto ? ((FR.st || 1) + i) + '/' + total : '' }); }
      dlg.el.addEventListener('change', function(e){ var t = e.target; if (t.id === 'tf-p') FR.size = t.value; else if (t.id === 'tf-s') FR.sc = Number(t.value.split(':')[1]); else if (t.id === 'tf-t') FR.tpl = t.value; else if (t.name === 'tfm') FR.mode = t.value; else if (t.id === 'tf-auto') FR.auto = t.checked; else if (t.id === 'tf-fill') FR.fill = t.checked; else if (t.id === 'tf-bm'){ FR.bm = t.value; $d('#tf-st').value = String(start()); } upd(); });
      dlg.el.addEventListener('numchange', upd);
      dlg.el.addEventListener('click', function(e){ var o = e.target.closest('[data-seg="tfor"] button'); if (o){ FR.por = o.dataset.v === 'Dọc'; upd(); } });
      $d('#tf-ok').addEventListener('click', function(){ if (!read()) return; var name = nm(), p = PAPER[FR.size], W = FR.por ? p[1] : p[0], Hh = FR.por ? p[0] : p[1], sc = FR.sc, placed = [], k = 0;
        dlg.close(true);
        var frames = function(base, i0, total){ var out = [], n = FR.mode === 'arr' ? FR.n : 1, m = FR.mode === 'arr' ? FR.m : 1; for (var r = 0; r < m; r++) for (var c = 0; c < n; c++) out.push({ t:'insert', name:name, p:[base[0] + c * (W + FR.gap) * sc, base[1] + (m - 1 - r) * (Hh + FR.gap) * sc], sx:sc, sy:sc, rot:0, layer:'KT-KHUNG', attrs:attrsFor(i0 + r * n + c, total) }); return out; };
        var total0 = (inSet ? start() - 1 : 0);
        (function loop(){ var total = total0 + (FR.mode === 'arr' ? count() : k + 1);
          return ctx.point(FR.mode === 'arr' ? 'Điểm chèn mảng khung (góc dưới trái)' : 'Điểm chèn khung ' + (FR.auto ? noAt(k, total) : (k + 1)) + ' (góc dưới trái) hoặc Enter để kết thúc', { onMove:function(q, temp){ temp.push(ED.temp(frames(q, k, total), { real:true })); } }).then(function(r){
            if (!r.p) return fin(); var L = frames(r.p, k, total); ED.run('TCFRAME', function(){ L.forEach(function(o){ placed.push(ED.add(ED.make(o)).attrs.KYHIEU); }); }); k += L.length; V().draw(); refresh(); return FR.mode === 'one' ? loop() : fin(); }); })().then(null, function(err){ fin(); if (err !== 'cancel') throw err; });
        function fin(){ res(); if (!placed.length) return; ctx.out('Đã tạo ' + placed.length + ' khung ' + FR.size + ' · 1:' + sc + (placed[0] ? ' (' + esc(placed[0]) + (placed.length > 1 ? ' → ' + esc(placed[placed.length - 1]) : '') + ')' : '') + '.'); TC.toast('Đã tạo <b>' + placed.length + ' khung ' + FR.size + ' 1:' + sc + '</b> có attribute. In ngay bằng In theo khung tên.', { log:false, act:{ label:'In theo khung tên', fn:function(){ SC.batchPlot(); } } }); placed = []; }
      });
      upd();
    });
  };

  /* ================= Đánh số tự động UI-J07 (TCNUMBER) ================= */
  var NPRE = [['Trục số · 1, 2, 3', { pre:'', kind:'num', start:1, step:1, dig:1, suf:'' }], ['Trục chữ · A, B, C', { pre:'', kind:'abc', start:1, step:1, dig:1, suf:'' }], ['Phòng · P.101', { pre:'P.', kind:'num', start:101, step:1, dig:3, suf:'' }], ['Cọc · C1', { pre:'C', kind:'num', start:1, step:1, dig:1, suf:'' }], ['Thiết bị · TB-01', { pre:'TB-', kind:'num', start:1, step:1, dig:2, suf:'' }], ['Dầm · D1 220×400', { pre:'D', kind:'num', start:1, step:1, dig:1, suf:' 220×400' }]];
  var NUM = { tgt:'text', tag:'', pre:'', kind:'num', start:1, step:1, dig:1, suf:'', order:'sel', preset:'' }, NDLG = null;
  function letters(n){ var s = ''; n = Math.max(1, Math.round(n)); while (n > 0){ var r = (n - 1) % 26; s = String.fromCharCode(65 + r) + s; n = Math.floor((n - 1) / 26); } return s; }
  function numVal(i){ var v = NUM.start + i * NUM.step, a = String(Math.abs(v)); return NUM.pre + (NUM.kind === 'abc' ? letters(v) : (v < 0 ? '-' : '') + ('0000000' + a).slice(-Math.max(NUM.dig, a.length))) + NUM.suf; }
  function numTargets(d){
    var out = [], skip = 0;
    d.sel.forEach(function(id){ var e = d.byId[id]; if (!e) return; if (NUM.tgt === 'attr' ? e.t === 'insert' && hasTag(d, e.name, NUM.tag) : e.t === 'text' || e.t === 'mtext') out.push(e); else skip++; });
    if (NUM.order === 'x' || NUM.order === 'y') out.sort(function(a, b){ return NUM.order === 'x' ? a.p[0] - b.p[0] || b.p[1] - a.p[1] : b.p[1] - a.p[1] || a.p[0] - b.p[0]; });
    if (NUM.order === 'rc'){ var tol = Math.max.apply(null, out.map(function(e){ return e.h || 250; }).concat([1])) * 1.5; out.sort(function(a, b){ return Math.abs(a.p[1] - b.p[1]) > tol ? b.p[1] - a.p[1] : a.p[0] - b.p[0]; }); }
    return { list:out, skip:skip };
  }
  function strOf(d, e, tag){ return e.t === 'insert' ? attrsOf(d, e)[tag] : e.s; }
  function setStr(e, tag, v){ ED.mod(e, function(x){ if (x.t === 'insert'){ x.attrs = Object.assign({}, x.attrs); x.attrs[tag] = v; } else x.s = v; }); }
  // nhãn xem trước gắn với điểm trên bản vẽ (đi theo pan/zoom)
  function pinPt(e){ if (e.t === 'text') return [leftOf(e) + estW(e) / 2, e.p[1] + e.h * (/m$/.test(e.al || '') ? 1.15 : 1.75)]; if (e.t === 'mtext') return [e.p[0] + e.w / 2, e.p[1] + e.h * .7]; var b = e._bb; return [(b[0] + b[2]) / 2, b[3] + (b[3] - b[1]) * .25]; }
  function pins(list){ TC.cad.reset(); list.slice(0, 80).forEach(function(x){ TC.cad.pin(esc(x[1]), pinPt(x[0]), 'area'); }); }
  TC.on('sel', function(){ if (NDLG) NDLG.upd(); if (XDLG) XDLG.upd(); });
  function besideCanvas(dlg){ var cr = $('#cv').getBoundingClientRect(); dlg.el.style.left = (cr.left + 12) + 'px'; dlg.el.style.top = Math.max(12, Math.min(cr.top + 8, innerHeight - dlg.el.offsetHeight - 12)) + 'px'; }
  IMPL.TCNUMBER = function(ctx, pre){
    if (pre === 'P') return numPoints(ctx);
    if (NDLG){ NDLG.dlg.el.focus(); NDLG.upd(); return; }
    var d = D(), tags = function(){ var o = {}; d.sel.concat(d.ents.filter(function(e){ return e.t === 'insert'; }).slice(0, 400).map(function(e){ return e.id; })).forEach(function(id){ var e = d.byId[id]; if (e && e.t === 'insert') ((d.blocks[e.name] || {}).ents || []).forEach(function(a){ if (a.t === 'attdef') o[a.tag] = (o[a.tag] || e.name); }); }); return o; };
    var tg = tags(); if (!NUM.tag) NUM.tag = Object.keys(tg)[0] || '';
    var dlg = TC.dialog({ id:'tcnum', title:'Đánh số tự động', cmd:'TCNUMBER', code:'UI-J07', w:440, h:Math.min(720, innerHeight - 24), modal:false,
      body:'<div class="stack tnw">' + H.grp('Đích', SC.seg('tnt', ['Text có sẵn', 'Attribute', 'Text mới'], { text:'Text có sẵn', attr:'Attribute', 'new':'Text mới' }[NUM.tgt]) + '<div id="tn-tag">' + H.row('Tag', '<select class="inp" id="tn-tg" style="width:250px">' + Object.keys(tg).map(function(k){ return '<option value="' + esc(k) + '"' + (k === NUM.tag ? ' selected' : '') + '>' + esc(k) + ' · ' + esc(tg[k]) + '</option>'; }).join('') + '</select>', 76) + '</div>') +
        H.grp('Định dạng', H.row('Mẫu có sẵn', '<select class="inp" id="tn-pre" style="width:200px"><option value="">(tự đặt)</option>' + NPRE.map(function(p){ return '<option' + (p[0] === NUM.preset ? ' selected' : '') + '>' + esc(p[0]) + '</option>'; }).join('') + '</select><button class="btn sm ghost" type="button" id="tn-save">Lưu mẫu…</button>', 76) + '<div class="frow" style="--lab-w:76px"><label class="lab" for="tn-p">Tiền tố</label><span class="hrow"><input class="inp" id="tn-p" style="width:96px" spellcheck="false"><label class="lab" for="tn-s">Hậu tố</label><input class="inp" id="tn-s" style="width:110px" spellcheck="false"></span></div>' +H.row('Kiểu', SC.seg('tnk', ['Số', 'Chữ cái'], NUM.kind === 'abc' ? 'Chữ cái' : 'Số'), 76) + H.row('Bắt đầu', H.num('tn-b', '1', { prec:0, unit:'none', w:72, label:'Số bắt đầu' }) + '<span class="lab">Bước</span>' + H.num('tn-st', '1', { prec:0, unit:'none', w:64, label:'Bước' }), 76) + H.row('Số chữ số', H.sel('tn-d', ['1', '01', '001', '0001'], '1', 96), 76)) +
        H.grp('Thứ tự', H.sel('tn-o', ['Theo thứ tự chọn', 'X tăng: trái → phải', 'Y giảm: trên → dưới', 'Theo hàng rồi cột'], 'Theo thứ tự chọn')) + H.grp('Xem trước <span class="faint" style="text-transform:none;letter-spacing:0;font-weight:400">· giá trị mới hiện trên vùng vẽ</span>', '<div id="tn-pv"></div>') + '</div>',
      foot:'<button class="btn ghost" type="button" id="tn-pt">' + ic('idpoint') + 'Chỉ điểm từng cái</button><span class="grow"></span><button class="btn ghost" type="button" data-x="close">Đóng</button><button class="btn primary" type="button" id="tn-ok"></button>',
      onClose:function(){ NDLG = null; TC.cad.reset(); } });
    besideCanvas(dlg);
    var $d = function(s){ return $(s, dlg.el); }, ORD = ['sel', 'x', 'y', 'rc'], DIG = ['1', '01', '001', '0001'];
    function sync(){ $d('#tn-p').value = NUM.pre; $d('#tn-s').value = NUM.suf; $d('#tn-b').value = String(NUM.start); $d('#tn-st').value = String(NUM.step); $d('#tn-d').value = DIG[NUM.dig - 1] || '1'; $d('#tn-o').selectedIndex = ORD.indexOf(NUM.order); $$('[data-seg="tnk"] button', dlg.el).forEach(function(b){ b.setAttribute('aria-pressed', String((b.dataset.v === 'Chữ cái') === (NUM.kind === 'abc'))); }); }
    function read(){ NUM.pre = $d('#tn-p').value; NUM.suf = $d('#tn-s').value; NUM.start = Math.round(num($d('#tn-b').value, NUM.start)); NUM.step = Math.round(num($d('#tn-st').value, NUM.step)) || 1; NUM.dig = DIG.indexOf($d('#tn-d').value) + 1; NUM.order = ORD[$d('#tn-o').selectedIndex]; NUM.tag = ($d('#tn-tg') || {}).value || NUM.tag; }
    function upd(){
      var T2 = numTargets(D()), L = T2.list, ok = $d('#tn-ok'), pv = $d('#tn-pv'), d2 = D();
      $d('#tn-tag').hidden = NUM.tgt !== 'attr';
      if (NUM.tgt === 'new'){ pv.innerHTML = '<div class="note">' + ic('info') + '<span>Bấm <b>Chỉ điểm vị trí</b> rồi chỉ điểm trên bản vẽ: mỗi lần bấm tạo một text mới <b class="mono">' + esc(numVal(0)) + '</b>, <b class="mono">' + esc(numVal(1)) + '</b>… (xem trước theo con trỏ).</span></div>'; ok.textContent = 'Chỉ điểm vị trí…'; ok.disabled = false; TC.cad.reset(); return; }
      var rows = L.map(function(e, i){ return [e, strOf(d2, e, NUM.tag) || '', numVal(i)]; }), ch = rows.filter(function(r){ return r[1] !== r[2]; }).length;
      pv.innerHTML = L.length ? '<div class="tblwrap" style="max-height:188px"><table class="tbl"><colgroup><col style="width:34px"><col><col></colgroup><thead><tr><th class="num">#</th><th>Hiện tại</th><th>Mới</th></tr></thead><tbody>' + rows.slice(0, 40).map(function(r, i){ return '<tr><td class="num">' + (i + 1) + '</td><td class="faint">' + esc(r[1]) + '</td><td class="' + (r[1] === r[2] ? 'faint' : '') + '"><b class="mono" style="font-weight:500">' + esc(r[2]) + '</b></td></tr>'; }).join('') + '</tbody></table></div>' + '<p class="faint" style="font-size:var(--fs-s);margin-top:6px">' + L.length + ' ' + (NUM.tgt === 'attr' ? 'block có ' + esc(NUM.tag) : 'text') + ' · ' + ch + ' thay đổi' + (T2.skip ? ' · bỏ qua ' + T2.skip + ' đối tượng không phải ' + (NUM.tgt === 'attr' ? 'block có tag này' : 'text') : '') + '</p>'
        : '<div class="empty" style="padding:14px 8px">' + ic('select') + '<b>Chưa chọn ' + (NUM.tgt === 'attr' ? 'block có attribute ' + esc(NUM.tag) : 'text') + '</b>Chọn trên bản vẽ (bấm hoặc kéo khung); danh sách và giá trị mới cập nhật ngay.<button class="btn sm" type="button" id="tn-sel" style="margin-top:4px">' + ic('select') + 'Chọn đối tượng</button></div>';
      ok.textContent = 'Đánh số ' + L.length + ' ' + (NUM.tgt === 'attr' ? 'block' : 'text'); ok.disabled = !ch;
      pins(rows.filter(function(r){ return r[1] !== r[2]; }).map(function(r){ return [r[0], '→ ' + r[2]]; }));
    }
    NDLG = { dlg:dlg, upd:upd, sync:sync };
    dlg.el.addEventListener('input', function(e){ if (/^tn-(p|s)$/.test(e.target.id)){ read(); NUM.preset = ''; $d('#tn-pre').value = ''; upd(); } });
    dlg.el.addEventListener('numchange', function(){ read(); upd(); });
    dlg.el.addEventListener('change', function(e){ var t = e.target; if (t.id === 'tn-pre'){ var p = NPRE.filter(function(x){ return x[0] === t.value; })[0]; NUM.preset = t.value; if (p) Object.assign(NUM, p[1]); sync(); } else read(); upd(); });
    dlg.el.addEventListener('click', function(e){
      var s = e.target.closest('[data-seg="tnt"] button'); if (s){ NUM.tgt = { 'Text có sẵn':'text', Attribute:'attr', 'Text mới':'new' }[s.dataset.v]; upd(); return; }
      var k = e.target.closest('[data-seg="tnk"] button'); if (k){ NUM.kind = k.dataset.v === 'Chữ cái' ? 'abc' : 'num'; upd(); return; }
      if (e.target.closest('#tn-sel')){ dlg.hide(true); TC.cmdline.msg('Chọn đối tượng cần đánh số rồi nhấn Enter. Esc để quay lại.'); var esc1 = function(ev){ if (ev.key === 'Escape'){ ev.stopPropagation(); TC.cad.endSel(); } }; document.addEventListener('keydown', esc1, true); TC.cad.getSel({}).then(function(){ document.removeEventListener('keydown', esc1, true); dlg.hide(false); upd(); }); return; }
      if (e.target.closest('#tn-save')){ read(); SC.ask('Lưu mẫu đánh số', 'Tên mẫu', NUM.pre ? NUM.pre + numVal(0).slice(NUM.pre.length) + '…' : 'Mẫu ' + (NPRE.length + 1), null, 'UI-J07').then(function(v){ if (!v) return; NPRE.push([v, { pre:NUM.pre, kind:NUM.kind, start:NUM.start, step:NUM.step, dig:NUM.dig, suf:NUM.suf }]); NUM.preset = v; $d('#tn-pre').insertAdjacentHTML('beforeend', '<option selected>' + esc(v) + '</option>'); TC.toast('Đã lưu mẫu đánh số <b>' + esc(v) + '</b>.', { log:false }); }); return; }
      if (e.target.closest('#tn-pt') || (e.target.closest('#tn-ok') && NUM.tgt === 'new')){ read(); dlg.hide(true); TC.cad.reset(); TC.cmd.run('TCNUMBER', 'P'); return; }
      if (e.target.closest('#tn-ok')){ read(); var d2 = D(), L = numTargets(d2).list; if (!L.length) return; ED.run('TCNUMBER', function(){ L.forEach(function(x, i){ if (strOf(d2, x, NUM.tag) !== numVal(i)) setStr(x, NUM.tag, numVal(i)); }); }); V().draw(); TC.toast('Đã đánh số ' + L.length + ' ' + (NUM.tgt === 'attr' ? 'block' : 'text') + ': <b class="mono">' + esc(numVal(0)) + '</b> → <b class="mono">' + esc(numVal(L.length - 1)) + '</b>. Hoàn tác bằng Ctrl+Z.', { log:false }); upd(); }
    });
    sync(); upd();
  };
  // chỉ điểm từng cái: bấm vào text (hoặc chỉ điểm vị trí text mới); mỗi lần bấm là một bước hoàn tác
  function numPoints(ctx){
    var d = D(), k = 0, made = 0, cur = ED.cur();
    var ghost = function(p){ return { t:'text', p:p, h:cur.th, s:numVal(k), al:'m', style:cur.tstyle, layer:ED.clayer() }; };
    var back = function(){ if (NDLG){ NDLG.dlg.hide(false); NDLG.sync(); NDLG.upd(); } };
    function step(){ return ctx.point(NUM.tgt === 'new' ? 'Vị trí text “' + numVal(k) + '”' : 'Bấm vào ' + (NUM.tgt === 'attr' ? 'block có ' + NUM.tag : 'text') + ' để đặt “' + numVal(k) + '”', { pick:NUM.tgt !== 'new', opts:[['Hoàn tác', 'U'], ['Xong', 'X']], onMove:function(p, temp){ if (NUM.tgt === 'new') temp.push(ED.temp([ghost(p)], { color:TC.cssv('--c-accent') })); } }).then(function(r){
      if (!r || r.enter || r.kw === 'X') return fin();
      if (r.kw === 'U'){ if (made){ ED.undo(1); made--; k--; } else ctx.out('Chưa có số nào để hoàn tác.'); return step(); }
      var v = V(), s = r.s || v.mToS(r.p) || v.toS(r.p), id = v.hitTest(s[0], s[1]), e = id && d.byId[id], hit = e && (NUM.tgt === 'attr' ? e.t === 'insert' && hasTag(d, e.name, NUM.tag) : e.t === 'text' || e.t === 'mtext');
      if (!hit && NUM.tgt !== 'new'){ TC.ctip($('#cv'), s[0], s[1], NUM.tgt === 'attr' ? 'Không phải block có attribute ' + NUM.tag : 'Không có text tại đây'); return step(); }
      var val = numVal(k); ED.run('TCNUMBER', function(){ if (hit && NUM.tgt !== 'new') setStr(e, NUM.tag, val); else ED.add(ED.make(ghost(r.p))); }); k++; made++; V().draw(); return step(); }); }
    function fin(){ if (made) ctx.out('Đã đánh số ' + made + ' đối tượng, tới <b class="mono">' + esc(numVal(k - 1)) + '</b>.'); NUM.start += k * NUM.step; back(); }
    return step().then(null, function(err){ back(); throw err; });
  }

  /* ================= Sao chép tăng số UI-J08 (TCINCCOPY) ================= */
  var IC = { part:'L' };
  function letterAdd(ch, n){ var up = ch === ch.toUpperCase(), i = ch.toUpperCase().charCodeAt(0) - 64 + n, s = letters(i); return up ? s : s.toLowerCase(); }
  // tăng phần số thứ part (L cuối, F đầu, hoặc thứ n) k lần bước step; không có số thì tăng chữ cái cuối (A → B, C1 → C2, 1.05 → 1.10)
  function incStr(s, step, part, k){
    var ps = [], re = /\d+(?:\.\d+)?/g, m; while ((m = re.exec(s))) ps.push({ i:m.index, s:m[0] });
    if (!ps.length){ var lm = /([A-Za-z])([^A-Za-z]*)$/.exec(s); return lm ? s.slice(0, lm.index) + letterAdd(lm[1], Math.round(step * k)) + lm[2] : s; }
    var q = part === 'F' ? ps[0] : part === 'L' ? ps[ps.length - 1] : ps[Math.min(ps.length, Math.max(1, Number(part))) - 1], dec = (q.s.split('.')[1] || '').length, out = Math.max(0, Number(q.s) + step * k).toFixed(dec);
    if (!dec && /^0\d/.test(q.s) && out.length < q.s.length) out = ('000000' + out).slice(-q.s.length);
    return s.slice(0, q.i) + out + s.slice(q.i + q.s.length);
  }
  IMPL.TCINCCOPY = function(ctx, pre){
    var d = D(), src = null, tag = null, s0 = '', step = 1, k = 0, made = 0;
    var get = pre && pre.t ? Promise.resolve({ e:pre }) : ED.pick(ctx, 'Chọn text hoặc block có attribute chứa số', { types:['text', 'mtext', 'insert'], bad:'Chọn text hoặc block có attribute' });
    return get.then(function(r){
      if (!r || !r.e) return; src = r.e;
      if (src.t === 'insert'){ var a = attrsOf(d, src); tag = Object.keys(a).filter(function(t){ return /\d|^[A-Za-z]$/.test(a[t]); })[0]; if (!tag){ ctx.out('<span class="err">Block ' + esc(src.name) + ' không có attribute chứa số.</span> Chọn text hoặc block như bóng trục, số cọc.'); return; } s0 = a[tag]; }
      else s0 = src.s;
      if (!/\d|[A-Za-z]/.test(s0)){ ctx.out('<span class="err">“' + esc(s0) + '” không có số hoặc chữ cái để tăng.</span> Chọn text dạng C1, 1.05, A.'); return; }
      var dec = ((/\d+\.(\d+)(?!.*\d)/.exec(s0) || [])[1] || '').length; step = dec ? (dec === 2 ? .05 : Math.pow(10, -dec)) : 1;
      ctx.out('Nguồn: <b class="mono">' + esc(s0) + '</b>' + (tag ? ' (attribute ' + esc(tag) + ')' : '') + ' · bước ' + step + ' · phần số ' + ({ L:'cuối', F:'đầu' }[IC.part] || 'thứ ' + IC.part) + '. Chỉ điểm vị trí các bản sao.');
      return loop();
    });
    function val(){ return incStr(s0, step, IC.part, k + 1); }
    function copyAt(p){ var c = ED.clean(src); delete c.id; ED.xform(c, T.move(p[0] - src.p[0], p[1] - src.p[1])); if (tag){ c.attrs = Object.assign({}, c.attrs); c.attrs[tag] = val(); } else c.s = val(); return c; }
    function loop(){ return ctx.point('Vị trí bản sao “' + val() + '” hoặc', { base:src.p, opts:[['Bước', 'B'], ['Phần số', 'P'], ['Hoàn tác', 'U']], onMove:function(p, temp){ temp.push(ED.temp([copyAt(p)], { real:true })); } }).then(function(r){
      if (r.kw === 'B') return ctx.val('Bước tăng (số âm để giảm)', String(step)).then(function(v){ var n2 = num(v, NaN); if (isFinite(n2) && n2) step = n2; else ctx.out('<span class="err">Bước phải là số khác 0.</span>'); return loop(); });
      if (r.kw === 'P') return ctx.kw('Phần số được tăng', [['Cuối', 'L'], ['Đầu', 'F'], ['Thứ 2', '2'], ['Thứ 3', '3']], IC.part).then(function(v){ IC.part = v; return loop(); });
      if (r.kw === 'U'){ if (made){ ED.undo(1); made--; k--; } else ctx.out('Chưa có bản sao để hoàn tác.'); return loop(); }
      if (!r.p){ if (made) ctx.out('Đã tạo ' + made + ' bản sao, tới <b class="mono">' + esc(incStr(s0, step, IC.part, k)) + '</b>.'); return; }
      var c = copyAt(r.p); ED.run('TCINCCOPY', function(){ ED.add(c); }); k++; made++; V().draw(); return loop(); }); }
  };

  /* ================= Công cụ text UI-J09 (TCTEXT) ================= */
  var TX = { tool:'case', mode:'upper', jOrd:'y', jSep:' ', jAs:'text', sBy:'line', sCh:';', aK:'l', aDist:false, p0:'', p1:'', s0:'', s1:'', op:'+', val:'0.450', which:'last' }, XDLG = null;
  var XTOOLS = [['case', 'Hoa, thường', 'case'], ['join', 'Nối text', 'txjoin'], ['split', 'Tách text', 'txsplit'], ['align', 'Canh hàng', 'alignl'], ['affix', 'Tiền tố, hậu tố', 'field'], ['num', 'Đổi số trong text', 'sigma']];
  // đổi hoa/thường đúng tiếng Việt: chuẩn hóa NFC để dấu đi cùng chữ (ư → Ư, đ → Đ, ờ → Ờ)
  function vnCase(s, mode){ s = String(s).normalize('NFC'); var up = function(c){ return c.toLocaleUpperCase('vi'); }, low = s.toLocaleLowerCase('vi');
    if (mode === 'upper') return up(s); if (mode === 'lower') return low;
    if (mode === 'title') return low.replace(/(^|[\s(“"'\-\/])(\S)/g, function(m, a, b){ return a + up(b); });
    return low.replace(/(^\s*|[.!?:]\s+|\n\s*)(\S)/g, function(m, a, b){ return a + up(b); }); }
  // chữ mã TCVN3: chữ hoa có dấu cần font “H” (.VnTimeH); đổi kiểu chữ theo cho đúng dấu
  function caseStyle(d, st, mode){ var s = d.styleMap[st] || {}; if (s.enc !== 'TCVN3') return st; var hs = d.styles.filter(function(x){ return x.enc === 'TCVN3' && !!x.upper === (mode === 'upper'); })[0]; return hs ? hs.name : st; }
  function affix(s){ if (TX.p0 && s.indexOf(TX.p0) === 0) s = TX.p1 + s.slice(TX.p0.length); else if (!TX.p0 && TX.p1) s = TX.p1 + s; if (TX.s0 && s.slice(-TX.s0.length) === TX.s0) s = s.slice(0, s.length - TX.s0.length) + TX.s1; else if (!TX.s0 && TX.s1) s = s + TX.s1; return s; }
  // cộng hoặc nhân số trong text, giữ số chữ số thập phân và dấu cao độ (±0.000 + 0.450 → +0.450)
  function numOp(s, op, v, which){
    var re = /(^|[\s=:(])([+\-±]?)(\d+(?:\.\d+)?)/g, ms = [], m; while ((m = re.exec(s))) ms.push(m); if (!ms.length) return s;
    var pick = which === 'all' ? ms : [which === 'first' ? ms[0] : ms[ms.length - 1]], out = s;
    pick.slice().reverse().forEach(function(m2){ var sign = m2[2], dec = (m2[3].split('.')[1] || '').length, x = (sign === '-' ? -1 : 1) * Number(m2[3]), y = op === '*' ? x * v : x + v, t = Math.abs(y).toFixed(dec), sg = sign ? (Math.abs(y) < 1e-9 ? '±' : y > 0 ? '+' : '-') : y < 0 ? '-' : '', at = m2.index + m2[1].length; out = out.slice(0, at) + sg + t + out.slice(at + m2[2].length + m2[3].length); });
    return out;
  }
  SC.vnCase = vnCase; SC.numOp = numOp; SC.incStr = incStr; // dùng lại cho TCASE, TCOUNT (Mở rộng) và các lệnh khác
  function estW(e){ return e.t === 'mtext' ? e.w : e.h * .62 * String(e.s).length; }
  function leftOf(e){ var w = estW(e), al = e.al || 'l'; return e.t === 'mtext' ? e.p[0] : /^(c|m)$/.test(al) ? e.p[0] - w / 2 : /^r/.test(al) ? e.p[0] - w : e.p[0]; }
  // kết quả của công cụ đang chọn: các thay đổi chữ (đổi s, kiểu) hoặc đối tượng mới / xóa / sửa vị trí
  function txCalc(d){
    var es = d.sel.map(function(id){ return d.byId[id]; }).filter(function(e){ return e && (e.t === 'text' || e.t === 'mtext'); }), R = { es:es, rows:[], mods:[], adds:[], dels:[], note:'' };
    if (TX.tool === 'case' || TX.tool === 'affix' || TX.tool === 'num'){
      es.forEach(function(e){ var ns = TX.tool === 'case' ? vnCase(e.s, TX.mode) : TX.tool === 'affix' ? affix(e.s) : numOp(e.s, TX.op, num(TX.val, 0), TX.which), st = TX.tool === 'case' ? caseStyle(d, e.style, TX.mode) : e.style; if (ns !== e.s || st !== e.style){ R.rows.push([e.s, ns, st !== e.style ? e.style + ' → ' + st : '']); R.mods.push([e, { s:ns, style:st }]); } });
      if (TX.tool === 'case' && R.mods.some(function(m){ return m[1].style !== m[0].style; })) R.note = 'Chữ mã TCVN3: kiểu chữ tự đổi giữa font thường và font “H” (.VnTimeH) để giữ đúng dấu khi đổi hoa, thường. Chuyển sang Unicode ở Bác sĩ Font để không cần đổi font.';
      return R; }
    var ord = es.slice(); if (TX.tool === 'join' && TX.jOrd !== 'sel') ord.sort(function(a, b){ return TX.jOrd === 'x' ? a.p[0] - b.p[0] : b.p[1] - a.p[1] || a.p[0] - b.p[0]; });
    if (TX.tool === 'join'){ if (es.length < 2){ R.note = 'Chọn ít nhất 2 text để nối.'; return R; } var e0 = ord[0], sep = TX.jSep === '\\n' ? '\n' : TX.jSep, s = ord.map(function(e){ return e.s; }).join(sep), asM = TX.jAs === 'mtext' || sep === '\n';
      var n = asM ? { t:'mtext', p:[Math.min.apply(null, ord.map(leftOf)), Math.max.apply(null, ord.map(function(e){ return e.p[1] + e.h; }))], w:Math.max.apply(null, ord.map(estW)) * (sep === '\n' ? 1.05 : ord.length), h:e0.h, s:s, style:e0.style, layer:e0.layer, al:'l' } : Object.assign(ED.clean(e0), { s:s }); delete n.id;
      R.adds.push(n); R.dels = ord.slice(); R.rows.push([ord.length + ' text', s, asM ? 'MTEXT' : 'TEXT']); return R; }
    if (TX.tool === 'split'){ es.forEach(function(e){ var parts = e.t === 'mtext' && TX.sBy === 'line' ? TC.mtextLines(e) : String(e.s).split(TX.sCh === ' ' ? /\s+/ : TX.sCh).map(function(x){ return x.trim(); }).filter(Boolean); if (parts.length < 2) return; var ls = (e.ls || 1.6667) * e.h, y0 = e.t === 'mtext' ? e.p[1] - e.h : e.p[1];
        parts.forEach(function(s2, i){ R.adds.push({ t:'text', p:[e.t === 'mtext' ? e.p[0] : leftOf(e), y0 - i * ls], h:e.h, s:s2, style:e.style, layer:e.layer, al:'l', rot:e.rot || 0 }); }); R.dels.push(e); R.rows.push([e.s.replace(/\n/g, ' ¶ '), parts.length + ' text: ' + parts.join(' | '), '']); });
      if (!R.dels.length) R.note = TX.sBy === 'line' ? 'Chọn MText nhiều dòng, hoặc đổi sang Tách theo ký tự cho TEXT.' : 'Không text nào chứa ký tự tách “' + TX.sCh + '”.'; return R; }
    if (TX.tool === 'align'){ if (!es.length) return R; var ref = es[0], L0 = leftOf(ref), w0 = estW(ref), X = TX.aK === 'l' ? L0 : TX.aK === 'c' ? L0 + w0 / 2 : L0 + w0, ys = es.slice().sort(function(a, b){ return b.p[1] - a.p[1]; });
      es.forEach(function(e){ var mid = /m$/.test(e.al || ''), al = e.t === 'mtext' ? TX.aK : (TX.aK === 'l' ? (mid ? 'lm' : 'l') : TX.aK === 'c' ? (mid ? 'm' : 'c') : (mid ? 'rm' : 'r')), x = e.t === 'mtext' ? X - (TX.aK === 'c' ? e.w / 2 : TX.aK === 'r' ? e.w : 0) : X, y = e.p[1];
        if (TX.aDist && es.length > 2){ var i = ys.indexOf(e); y = ys[0].p[1] + (ys[ys.length - 1].p[1] - ys[0].p[1]) * i / (ys.length - 1); }
        if (Math.abs(x - e.p[0]) > .01 || Math.abs(y - e.p[1]) > .01 || al !== (e.al || 'l')) R.mods.push([e, { p:[x, y], al:al }]); });
      R.rows.push([es.length + ' text', R.mods.length + ' text đổi vị trí', 'X = ' + TC.d(X)]); return R; }
    return R;
  }
  IMPL.TCTEXT = function(ctx, pre){
    var known = pre && XTOOLS.some(function(t){ return t[0] === pre; });
    if (XDLG){ var tb = known && $('[data-xt="' + pre + '"]', XDLG.dlg.el); if (tb) tb.click(); XDLG.dlg.el.focus(); XDLG.upd(); return; }
    if (known) TX.tool = pre;
    var dlg = TC.dialog({ id:'tctext', title:'Công cụ text', cmd:'TCTEXT', code:'UI-J09', w:600, h:560, modal:false, flush:true,
      body:'<div class="vtabs" style="height:100%"><div class="vt-list" role="tablist" aria-label="Công cụ">' + XTOOLS.map(function(t){ return '<button class="vt" type="button" role="tab" data-xt="' + t[0] + '" aria-selected="' + (TX.tool === t[0]) + '">' + ic(t[2]) + esc(t[1]) + '</button>'; }).join('') + '</div><div class="vt-body stack" id="tx-b"></div></div>',
      foot:'<span class="faint" id="tx-n" style="font-size:var(--fs-s)"></span><span class="grow"></span><button class="btn ghost" type="button" data-x="close">Đóng</button><button class="btn primary" type="button" id="tx-ok"></button>', onClose:function(){ XDLG = null; TC.cad.reset(); } });
    besideCanvas(dlg);
    var $d = function(s){ return $(s, dlg.el); };
    function pane(){
      var t = TX.tool, r = function(n, v, lab){ return H.rad(n, v, lab, TX[{ txm:'mode', txo:'jOrd', txs:'jSep', txa:'jAs', txb:'sBy', txk:'aK', txop:'op', txw:'which' }[n]] === v); }, B = '';
      if (t === 'case') B = H.grp('Đổi thành', r('txm', 'upper', 'HOA TOÀN BỘ') + r('txm', 'lower', 'thường toàn bộ') + r('txm', 'sentence', 'Hoa đầu câu') + r('txm', 'title', 'Hoa Mỗi Từ')) + '<p class="faint" style="font-size:var(--fs-s)">Đúng tiếng Việt có dấu: “đường ống” → “ĐƯỜNG ỐNG”, “ỨNG SUẤT” → “ứng suất”.</p>';
      if (t === 'join') B = H.grp('Thứ tự nối', r('txo', 'y', 'Trên → dưới') + r('txo', 'x', 'Trái → phải') + r('txo', 'sel', 'Theo thứ tự chọn')) + H.grp('Ký tự nối', r('txs', ' ', 'Khoảng trắng') + r('txs', ', ', 'Dấu phẩy và khoảng trắng') + r('txs', '', 'Không có') + r('txs', '\\n', 'Xuống dòng (kết quả là MTEXT)')) + H.grp('Kết quả', r('txa', 'text', 'Một TEXT tại vị trí text đầu') + r('txa', 'mtext', 'Một MTEXT'));
      if (t === 'split') B = H.grp('Tách', r('txb', 'line', 'MTEXT thành nhiều TEXT theo dòng') + r('txb', 'char', 'Theo ký tự') + '<div class="hrow" style="padding-left:20px">' + H.row('Ký tự tách', '<input class="inp mono" id="tx-ch" value="' + esc(TX.sCh) + '" style="width:64px" aria-label="Ký tự tách">', 70) + '</div>');
      if (t === 'align') B = H.grp('Canh theo một đường dọc', r('txk', 'l', 'Trái') + r('txk', 'c', 'Giữa') + r('txk', 'r', 'Phải') + '<span class="faint" style="font-size:var(--fs-s)">Đường canh lấy theo text chọn đầu tiên.</span>') + H.chk('tx-dist', 'Chia đều khoảng cách theo chiều dọc (giữ text trên cùng và dưới cùng)', TX.aDist);
      if (t === 'affix') B = H.grp('Tiền tố', H.row('Tìm ở đầu', '<input class="inp" id="tx-p0" value="' + esc(TX.p0) + '" style="width:150px" spellcheck="false">', 80) + H.row('Thay bằng', '<input class="inp" id="tx-p1" value="' + esc(TX.p1) + '" style="width:150px" spellcheck="false">', 80)) + H.grp('Hậu tố', H.row('Tìm ở cuối', '<input class="inp" id="tx-s0" value="' + esc(TX.s0) + '" style="width:150px" spellcheck="false">', 80) + H.row('Thay bằng', '<input class="inp" id="tx-s1" value="' + esc(TX.s1) + '" style="width:150px" spellcheck="false">', 80)) + '<p class="faint" style="font-size:var(--fs-s)">Để trống ô Tìm để thêm mới. Ví dụ đổi “S = ” thành “DT: ”.</p>';
      if (t === 'num') B = H.grp('Phép tính', r('txop', '+', 'Cộng') + r('txop', '*', 'Nhân') + H.row('Giá trị', H.num('tx-v', TX.val, { prec:3, unit:'none', w:96, label:'Giá trị' }), 60)) + H.grp('Số được đổi', r('txw', 'last', 'Số cuối') + r('txw', 'first', 'Số đầu') + r('txw', 'all', 'Mọi số')) + '<p class="faint" style="font-size:var(--fs-s)">Giữ số chữ số thập phân và dấu cao độ: dời toàn bộ cao độ +0.450 thì ±0.000 → +0.450, +3.600 → +4.050.</p>';
      $d('#tx-b').innerHTML = B + '<div class="gh">Xem trước</div><div id="tx-pv"></div>'; upd();
    }
    function upd(){
      var d = D(), R = txCalc(d), ok = $d('#tx-ok'), n = R.mods.length + R.dels.length, geo = TX.tool === 'join' || TX.tool === 'split' || TX.tool === 'align';
      $d('#tx-pv').innerHTML = (R.es.length ? (R.rows.length ? '<div class="tblwrap" style="max-height:200px"><table class="tbl"><colgroup><col><col><col style="width:' + (geo ? 70 : 150) + 'px"></colgroup><thead><tr><th>Hiện tại</th><th>Sau khi đổi</th><th>' + (geo ? 'Loại' : 'Kiểu chữ') + '</th></tr></thead><tbody>' + R.rows.slice(0, 30).map(function(x){ return '<tr><td class="faint" title="' + esc(x[0]) + '">' + esc(x[0]) + '</td><td title="' + esc(x[1]) + '">' + esc(x[1]) + '</td><td class="faint">' + esc(x[2]) + '</td></tr>'; }).join('') + '</tbody></table></div>' : '<p class="faint" style="font-size:var(--fs-s)">' + (R.note || 'Các text đã chọn không thay đổi với thiết lập này.') + '</p>') + (R.note && R.rows.length ? '<div class="note" style="margin-top:8px">' + ic('info') + '<span>' + esc(R.note) + '</span></div>' : '')
        : '<div class="empty" style="padding:16px 8px">' + ic('select') + '<b>Chưa chọn text</b>Chọn TEXT hoặc MTEXT trên bản vẽ; xem trước cập nhật ngay.</div>');
      $d('#tx-n').textContent = R.es.length ? R.es.length + ' text đã chọn' : '';
      var lab = { case:'Đổi ' + R.mods.length + ' text', join:'Nối ' + R.dels.length + ' text', split:'Tách ' + R.dels.length + ' text', align:'Canh ' + R.mods.length + ' text', affix:'Đổi ' + R.mods.length + ' text', num:'Đổi số ' + R.mods.length + ' text' }[TX.tool];
      ok.textContent = lab; ok.disabled = !n;
      // xem trước trên vùng vẽ: nhãn giá trị mới, hoặc bóng đối tượng mới/vị trí mới
      if (geo){ TC.cad.reset(); V().temp = [ED.temp(R.adds.concat(R.mods.map(function(m){ return Object.assign(ED.clean(m[0]), m[1]); })), { color:TC.cssv('--c-accent'), w:1.4 })]; V().drawOver(); }
      else pins(R.mods.map(function(m){ return [m[0], m[1].s]; }));
      XDLG.R = R;
    }
    XDLG = { dlg:dlg, upd:upd };
    dlg.el.addEventListener('click', function(e){
      var t = e.target.closest('[data-xt]'); if (t){ TX.tool = t.dataset.xt; $$('[data-xt]', dlg.el).forEach(function(b){ b.setAttribute('aria-selected', String(b === t)); }); pane(); return; }
      if (e.target.closest('#tx-ok')){ var R = XDLG.R, d = D(), n = 0; if (!R) return;
        ED.run('TCTEXT', function(){ R.mods.forEach(function(m){ ED.mod(m[0], function(x){ Object.assign(x, m[1]); }); n++; }); R.dels.forEach(function(e2){ ED.del(e2); }); R.adds.forEach(function(a){ var e2 = ED.add(a); d.sel.push(e2.id); }); });
        V().draw(); TC.cad.afterSel(); TC.toast({ case:'Đã đổi hoa, thường ' + n + ' text', join:'Đã nối ' + R.dels.length + ' text thành 1 ' + (R.adds[0] && R.adds[0].t === 'mtext' ? 'MTEXT' : 'TEXT'), split:'Đã tách thành ' + R.adds.length + ' text', align:'Đã canh ' + n + ' text', affix:'Đã đổi tiền tố, hậu tố ' + n + ' text', num:'Đã đổi số trong ' + n + ' text' }[TX.tool] + '. Hoàn tác bằng Ctrl+Z.', { log:false }); upd(); }
    });
    dlg.el.addEventListener('change', function(e){ var t = e.target, k = { txm:'mode', txo:'jOrd', txs:'jSep', txa:'jAs', txb:'sBy', txk:'aK', txop:'op', txw:'which' }[t.name]; if (k) TX[k] = t.value; if (t.id === 'tx-dist') TX.aDist = t.checked; upd(); });
    dlg.el.addEventListener('input', function(e){ var t = e.target; if (t.id === 'tx-ch') TX.sCh = t.value || ';'; else if (/^tx-[ps][01]$/.test(t.id)) TX[t.id.slice(3)] = t.value; else return; upd(); });
    dlg.el.addEventListener('numchange', function(e){ TX.val = String(e.detail); upd(); });
    pane();
  };

  /* ================= Ký hiệu kỹ thuật UI-J10 (TCSYMBOL) ================= */
  var SY = { kind:'axes', sc:100, elT:'cut', elM:'y', y0:0, elV:'+3.600', hx:[['1', 0], ['2', 3900], ['3', 3900], ['4', 3900]], vy:[['A', 0], ['B', 4800], ['C', 4200]], ends:'both', ext:20, dims:true, total:true, cutNo:'1', cutRef:'KT 05', detNo:'A', detRef:'KT 05', ang:0, slope:'2', slU:'%', mat:'TCVN-BTCT' };
  var SKINDS = [['elev', 'Cao độ', 'elev', 'Đặt cao độ'], ['axes', 'Trục định vị', 'axisgrid', 'Vẽ lưới trục'], ['cut', 'Mặt cắt', 'cutsym', 'Đặt ký hiệu mặt cắt'], ['detail', 'Chi tiết', 'detailsym', 'Đặt ký hiệu chi tiết'], ['north', 'Hướng Bắc', 'north', 'Đặt hướng Bắc'], ['slope', 'Độ dốc', 'slope', 'Đặt độ dốc'], ['mat', 'Vật liệu', 'hatch', 'Tô vật liệu']];
  function fmtElev(v){ return Math.abs(v) < .0005 ? '±0.000' : (v > 0 ? '+' : '-') + Math.abs(v).toFixed(3); }
  // block cao độ mặt bằng (vòng tròn tô hai góc phần tư); cao độ mặt cắt dùng TCVN-CAO-DO của thư viện
  function ensureElevPlan(d){ var nm = 'TCVN-CAO-DO-MB'; if (d.blocks[nm]) return nm; var q = function(a0){ var pts = [[0, 0]]; for (var i = 0; i <= 8; i++){ var a = (a0 + i * 90 / 8) * Math.PI / 180; pts.push([2 * Math.cos(a), 2 * Math.sin(a)]); } return pts; };
    d.blocks[nm] = { name:nm, desc:'Cao độ trên mặt bằng', lib:true, ents:withIds(d, [{ t:'circle', c:[0, 0], r:2, layer:'0' }, { t:'hatch', loops:[q(0)], pat:'SOLID', layer:'0' }, { t:'hatch', loops:[q(180)], pat:'SOLID', layer:'0' }, { t:'line', p1:[2, 0], p2:[15, 0], layer:'0' }, { t:'attdef', tag:'CAODO', prompt:'Cao độ', p:[3, .8], h:2.5, s:'±0.000', style:'Standard', al:'l', layer:'0' }]) }; return nm; }
  function elevEnt(d, p){ var v = SY.elM === 'y' ? fmtElev((p[1] - SY.y0) / 1000) : SY.elV, nm = SY.elT === 'plan' ? ensureElevPlan(d) : (SC.ensureBlock(d, 'TCVN-CAO-DO'), 'TCVN-CAO-DO'); return { t:'insert', name:nm, p:p, sx:SY.sc, sy:SY.sc, rot:0, layer:'KT-KYHIEU', attrs:{ CAODO:v } }; }
  function axesSum(list){ var out = [0]; for (var i = 1; i < list.length; i++) out.push(out[i - 1] + Number(list[i][1])); return out; }
  // lưới trục: đường trục (linetype tâm theo layer KT-TRUC), bóng trục, kích thước khoảng cách và tổng; p = giao của trục ngang đầu và trục dọc đầu
  function axesEnts(d, p){
    SC.ensureBlock(d, 'TCVN-TRUC'); var xs = axesSum(SY.hx), ys = axesSum(SY.vy), ex = SY.ext * SY.sc, R = 4 * SY.sc, x1 = xs[xs.length - 1], y1 = ys[ys.length - 1], out = [], X = function(x){ return p[0] + x; }, Y = function(y){ return p[1] + y; };
    SY.hx.forEach(function(a, i){ out.push({ t:'line', p1:[X(xs[i]), Y(-ex)], p2:[X(xs[i]), Y(y1 + ex)], layer:'KT-TRUC' }, { t:'insert', name:'TCVN-TRUC', p:[X(xs[i]), Y(-ex - R)], sx:SY.sc, sy:SY.sc, rot:0, layer:'KT-TRUC', attrs:{ TRUC:a[0] } }); if (SY.ends === 'both') out.push({ t:'insert', name:'TCVN-TRUC', p:[X(xs[i]), Y(y1 + ex + R)], sx:SY.sc, sy:SY.sc, rot:0, layer:'KT-TRUC', attrs:{ TRUC:a[0] } }); });
    SY.vy.forEach(function(a, j){ out.push({ t:'line', p1:[X(-ex), Y(ys[j])], p2:[X(x1 + ex), Y(ys[j])], layer:'KT-TRUC' }, { t:'insert', name:'TCVN-TRUC', p:[X(-ex - R), Y(ys[j])], sx:SY.sc, sy:SY.sc, rot:0, layer:'KT-TRUC', attrs:{ TRUC:a[0] } }); if (SY.ends === 'both') out.push({ t:'insert', name:'TCVN-TRUC', p:[X(x1 + ex + R), Y(ys[j])], sx:SY.sc, sy:SY.sc, rot:0, layer:'KT-TRUC', attrs:{ TRUC:a[0] } }); });
    var dim = function(a, b, pos, dir){ out.push({ t:'dim', p1:a, p2:b, pos:pos, dir:dir, style:'TCVN-1-100', layer:'KT-DIM' }); };
    if (SY.dims){ for (var i = 1; i < xs.length; i++) dim([X(xs[i - 1]), Y(-ex * .3)], [X(xs[i]), Y(-ex * .3)], Y(-ex * .45), 'h'); for (var j = 1; j < ys.length; j++) dim([X(-ex * .3), Y(ys[j - 1])], [X(-ex * .3), Y(ys[j])], X(-ex * .45), 'v'); }
    if (SY.total && xs.length > 2) dim([X(0), Y(-ex * .3)], [X(x1), Y(-ex * .3)], Y(-ex * .75), 'h'); if (SY.total && ys.length > 2) dim([X(-ex * .3), Y(0)], [X(-ex * .3), Y(y1)], X(-ex * .75), 'v');
    return out;
  }
  // ký hiệu mặt cắt: nét cắt đậm ở hai đầu, mũi tên hướng nhìn, số hiệu và tham chiếu tờ ("1-1 / KT 05")
  function cutEnts(a, b, side){
    var k = SY.sc, L = G.dist(a, b) || 1, u = [(b[0] - a[0]) / L, (b[1] - a[1]) / L], n = [-u[1], u[0]], out = [];
    if (side && (side[0] - a[0]) * n[0] + (side[1] - a[1]) * n[1] < 0) n = [-n[0], -n[1]];
    [[a, u], [b, [-u[0], -u[1]]]].forEach(function(z){ var E = z[0], w = z[1], P2 = function(s, t){ return [E[0] + w[0] * s * k + n[0] * t * k, E[1] + w[1] * s * k + n[1] * t * k]; }, tip = P2(0, 9), tri = [P2(-1.3, 6.2), tip, P2(1.3, 6.2)];
      out.push({ t:'line', p1:E, p2:P2(10, 0), lw:.5, layer:'KT-MATCAT' }, { t:'line', p1:E, p2:P2(0, 6.2), layer:'KT-KYHIEU' }, { t:'hatch', loops:[tri], pat:'SOLID', layer:'KT-KYHIEU' }, { t:'pline', pts:tri, closed:true, layer:'KT-KYHIEU' }, { t:'text', p:P2(-4, 5), h:3.5 * k, s:SY.cutNo, al:'m', style:'Standard', layer:'KT-KYHIEU' }); });
    var c = [a[0] - u[0] * 4 * k + n[0] * 5 * k, a[1] - u[1] * 4 * k + n[1] * 5 * k - 4.6 * k];
    if (SY.cutRef) out.push({ t:'text', p:c, h:2.2 * k, s:SY.cutNo + '-' + SY.cutNo + ' / ' + SY.cutRef, al:'m', style:'Standard', layer:'KT-KYHIEU' });
    return out;
  }
  function slopeEnts(a, b){ var k = SY.sc, L = G.dist(a, b) || 1, u = [(b[0] - a[0]) / L, (b[1] - a[1]) / L], n = [-u[1], u[0]], ang = Math.atan2(u[1], u[0]) * 180 / Math.PI, rot = ang > 90 ? ang - 180 : ang <= -90 ? ang + 180 : ang, up = rot === ang ? 1 : -1, tri = [[b[0] - u[0] * 3 * k + n[0] * k, b[1] - u[1] * 3 * k + n[1] * k], b, [b[0] - u[0] * 3 * k - n[0] * k, b[1] - u[1] * 3 * k - n[1] * k]];
    return [{ t:'line', p1:a, p2:b, layer:'KT-KYHIEU' }, { t:'hatch', loops:[tri], pat:'SOLID', layer:'KT-KYHIEU' }, { t:'pline', pts:tri, closed:true, layer:'KT-KYHIEU' }, { t:'text', p:[(a[0] + b[0]) / 2 + n[0] * up * 2.2 * k, (a[1] + b[1]) / 2 + n[1] * up * 2.2 * k], h:2.5 * k, s:'i = ' + SY.slope + SY.slU, rot:rot, al:'m', style:'Standard', layer:'KT-KYHIEU' }]; }
  function symPreview(d){ var k = SY.kind;
    if (k === 'elev') return [elevEnt(d, [0, SY.elM === 'y' ? SY.y0 + 3600 : 0])];
    if (k === 'axes') return axesEnts(d, [0, 0]);
    if (k === 'cut') return cutEnts([0, 0], [3000, 0], [0, 1]);
    if (k === 'detail'){ SC.ensureBlock(d, 'TCVN-CHI-TIET'); return [{ t:'insert', name:'TCVN-CHI-TIET', p:[0, 0], sx:SY.sc, sy:SY.sc, rot:0, layer:'KT-KYHIEU', attrs:{ SO:SY.detNo, TO:SY.detRef } }]; }
    if (k === 'north'){ SC.ensureBlock(d, 'TCVN-HUONG-BAC'); return [{ t:'insert', name:'TCVN-HUONG-BAC', p:[0, 0], sx:SY.sc, sy:SY.sc, rot:SY.ang, layer:'KT-KYHIEU' }]; }
    if (k === 'slope') return slopeEnts([0, 0], [2400, 0]);
    return [];
  }
  IMPL.TCSYMBOL = function(ctx, pre){
    var d = D(); if (pre && SKINDS.some(function(x){ return x[0] === pre; })) SY.kind = pre;
    var refs = sheets().map(function(s){ return s.no; });
    return new Promise(function(res){
      var dlg = TC.dialog({ id:'tcsym', title:'Ký hiệu kỹ thuật', cmd:'TCSYMBOL', code:'UI-J10', w:820, h:600, flush:true, onClose:function(r){ if (r !== true) res(); },
        body:'<div class="vtabs" style="height:100%"><div class="vt-list" role="tablist" aria-label="Loại ký hiệu">' + SKINDS.map(function(x){ return '<button class="vt" type="button" role="tab" data-sk2="' + x[0] + '" aria-selected="' + (SY.kind === x[0]) + '">' + ic(x[2]) + esc(x[1]) + '</button>'; }).join('') + '<div class="sy-sc">' + H.row('Tỉ lệ', H.sel('sy-sc', ['1:20', '1:50', '1:100', '1:200'], '1:' + SY.sc, 84), 36) + '<span class="faint" style="font-size:var(--fs-xs)">Cỡ ký hiệu theo mm giấy × tỉ lệ chú thích</span></div></div><div class="vt-body stack" id="sy-b"></div></div>',
        foot:'<span class="fmsg" id="sy-msg" aria-live="polite"></span><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="sy-ok"></button>' });
      var $d = function(s){ return $(s, dlg.el); };
      function axTable(key, title){ return '<div class="stack" style="gap:4px"><span class="gh">' + title + '</span><div class="tblwrap" style="max-height:176px"><table class="tbl axt"><colgroup><col style="width:56px"><col></colgroup><thead><tr><th>Tên</th><th>Khoảng cách tới trục trước (mm)</th></tr></thead><tbody>' + SY[key].map(function(a, i){ return '<tr><td><input class="inp sm mono" data-ax="' + key + '" data-i="' + i + '" data-c="0" value="' + esc(a[0]) + '" aria-label="Tên trục ' + (i + 1) + '"></td><td>' + (i ? '<input class="inp sm mono" data-ax="' + key + '" data-i="' + i + '" data-c="1" value="' + esc(a[1]) + '" aria-label="Khoảng cách trục ' + esc(a[0]) + '">' : '<span class="faint">trục gốc</span>') + '</td></tr>'; }).join('') + '</tbody></table></div><span class="hrow"><button class="btn sm" type="button" data-axa="' + key + '">' + ic('plus') + 'Thêm trục</button><button class="btn sm ghost" type="button" data-axr="' + key + '"' + (SY[key].length < 3 ? ' disabled' : '') + '>' + ic('minus') + 'Bớt trục cuối</button></span></div>'; }
      function body(){
        var k = SY.kind, B = '', r = function(n, v, lab, cur){ return H.rad(n, v, lab, cur === v); };
        if (k === 'elev') B = H.grp('Kiểu ký hiệu', r('sye', 'cut', 'Mặt cắt, mặt đứng (tam giác trên đường cao độ)', SY.elT) + r('sye', 'plan', 'Mặt bằng (vòng tròn tô hai góc)', SY.elT)) + H.grp('Giá trị', r('sym', 'y', 'Theo tọa độ Y so với gốc ±0.000', SY.elM) + '<div class="hrow" style="padding-left:20px">' + H.row('Gốc ±0.000 tại Y', H.num('sy-y0', TC.d(SY.y0), { w:110, label:'Y của cao độ ±0.000' }) + '<button class="btn sm" type="button" id="sy-pk">' + ic('idpoint') + 'Chỉ điểm gốc</button>', 110) + '</div>' + r('sym', 'man', 'Nhập tay', SY.elM) + '<div class="hrow" style="padding-left:20px"><input class="inp mono" id="sy-ev" value="' + esc(SY.elV) + '" style="width:110px" aria-label="Giá trị cao độ"></div>') + '<p class="faint" style="font-size:var(--fs-s)">Định dạng +1.200: 3 chữ số thập phân, đơn vị m. Giá trị xem trước theo con trỏ khi đặt. Tự cập nhật khi di chuyển: sau v1.0.</p>';
        if (k === 'axes') B = '<div class="axw">' + axTable('hx', 'Trục ngang · 1, 2, 3…') + axTable('vy', 'Trục dọc · A, B, C…') + '</div><span class="fmsg" id="sy-axm"></span><div class="hrow">' + H.row('Bóng trục', SC.seg('syend', ['Hai đầu', 'Đầu dưới, trái'], SY.ends === 'both' ? 'Hai đầu' : 'Đầu dưới, trái'), 70) + H.row('Kéo dài', H.num('sy-ext', String(SY.ext), { prec:0, unit:'none', w:70, label:'Kéo dài trục' }) + '<span class="faint">mm giấy</span>', 56) + '</div><div class="hrow">' + H.chk('sy-dim', 'Kích thước khoảng cách', SY.dims) + H.chk('sy-tot', 'Kích thước tổng', SY.total) + '<span class="grow"></span><button class="btn sm" type="button" id="sy-from">' + ic('select') + 'Lấy từ đối tượng có sẵn</button></div>';
        if (k === 'cut') B = H.grp('Ký hiệu', H.row('Số hiệu', '<input class="inp mono" id="sy-cn" value="' + esc(SY.cutNo) + '" style="width:60px" aria-label="Số hiệu mặt cắt"> <span class="faint">→ mặt cắt <b id="sy-cnn">' + esc(SY.cutNo + '-' + SY.cutNo) + '</b></span>', 90) + H.row('Vẽ ở tờ', '<select class="inp" id="sy-cr" style="width:120px"><option value="">(không ghi)</option>' + refs.map(function(n){ return '<option' + (n === SY.cutRef ? ' selected' : '') + '>' + esc(n) + '</option>'; }).join('') + '</select>', 90)) + '<p class="faint" style="font-size:var(--fs-s)">Chỉ điểm hai đầu đường cắt, rồi chỉ phía nhìn: mũi tên quay về phía đó.</p>';
        if (k === 'detail') B = H.grp('Ký hiệu', H.row('Số chi tiết', '<input class="inp mono" id="sy-dn" value="' + esc(SY.detNo) + '" style="width:60px" aria-label="Số chi tiết">', 90) + H.row('Vẽ ở tờ', '<select class="inp" id="sy-dr" style="width:120px">' + refs.map(function(n){ return '<option' + (n === SY.detRef ? ' selected' : '') + '>' + esc(n) + '</option>'; }).join('') + '</select>', 90));
        if (k === 'north') B = H.grp('Hướng', H.row('Góc so với phương dọc', H.num('sy-ang', String(SY.ang), { prec:0, unit:'deg', w:80, label:'Góc hướng Bắc' }) + '<span class="faint">độ, ngược chiều kim đồng hồ</span>', 150)) + '<p class="faint" style="font-size:var(--fs-s)">Hoặc chỉ hướng trên bản vẽ sau khi chọn điểm đặt.</p>';
        if (k === 'slope') B = H.grp('Độ dốc', H.row('Giá trị', '<input class="inp mono" id="sy-sl" value="' + esc(SY.slope) + '" style="width:70px" aria-label="Độ dốc">' + SC.seg('syu', ['%', '‰'], SY.slU), 60)) + '<p class="faint" style="font-size:var(--fs-s)">Chỉ điểm đầu và cuối mũi tên; mũi tên chỉ hướng xuống dốc. Chữ “i = 2%” luôn đọc xuôi.</p>';
        if (k === 'mat') B = '<span class="gh">Vật liệu theo TCVN 7570, 4614</span><div class="hgal sm" style="padding:0">' + SC.PATS.filter(function(p){ return p[1] === 'TCVN'; }).map(function(p){ return '<button type="button" class="hgi' + (p[0] === SY.mat ? ' on' : '') + '" data-mt2="' + p[0] + '"><canvas width="64" height="44"></canvas><b>' + esc(p[2]) + '</b><span class="mono">' + p[0] + '</span></button>'; }).join('') + '</div><p class="faint" style="font-size:var(--fs-s)">Mở lệnh Hatch với mẫu đã chọn: bấm vào vùng kín để tô. Gallery đầy đủ ở Thư viện TCVN.</p>';
        $d('#sy-b').innerHTML = B + (k === 'mat' ? '' : '<div class="gh">Xem trước</div><canvas id="sy-pv" width="' + (k === 'axes' ? 520 : 360) + '" height="' + (k === 'axes' ? 200 : 130) + '" class="pvc"></canvas>');
        $$('[data-mt2] canvas', dlg.el).forEach(function(c){ SC.patThumb(c, c.parentNode.dataset.mt2); });
        $d('#sy-ok').textContent = SKINDS.filter(function(x){ return x[0] === k; })[0][3]; pv();
      }
      function axCheck(){ var bad = null; ['hx', 'vy'].forEach(function(key){ SY[key].forEach(function(a, i){ if (i && !(Number(a[1]) > 0)) bad = bad || 'Khoảng cách trục ' + a[0] + ' phải lớn hơn 0.'; if (!String(a[0]).trim()) bad = bad || 'Tên trục không được để trống.'; }); }); var m = $d('#sy-axm'); if (m){ m.className = 'fmsg ' + (bad ? 'err' : 'hint'); m.innerHTML = bad ? ic('warn', 'xs') + esc(bad) : 'Tổng: ' + TC.n(axesSum(SY.hx).pop()) + ' × ' + TC.n(axesSum(SY.vy).pop()) + ' mm · ' + SY.hx.length + ' × ' + SY.vy.length + ' trục'; } $d('#sy-ok').disabled = !!bad; return !bad; }
      function pv(){ var c = $d('#sy-pv'); if (SY.kind === 'axes' && !axCheck()) return; if (c) paint(c, d, symPreview(d)); }
      function hideFor(p){ dlg.hide(true); return p.then(function(x){ dlg.hide(false); body(); return x; }, function(err){ dlg.hide(false); body(); if (err !== 'cancel') throw err; }); }
      dlg.el.addEventListener('click', function(e){
        var t = e.target.closest('[data-sk2]'); if (t){ SY.kind = t.dataset.sk2; $$('[data-sk2]', dlg.el).forEach(function(b){ b.setAttribute('aria-selected', String(b === t)); }); body(); return; }
        var sg = e.target.closest('[data-seg] button'); if (sg){ var g = sg.parentNode.dataset.seg; if (g === 'syend') SY.ends = sg.dataset.v === 'Hai đầu' ? 'both' : 'one'; if (g === 'syu') SY.slU = sg.dataset.v; pv(); return; }
        var ad = e.target.closest('[data-axa]'); if (ad){ var L = SY[ad.dataset.axa], last = L[L.length - 1]; L.push([/^\d+$/.test(last[0]) ? String(Number(last[0]) + 1) : letters(last[0].charCodeAt(0) - 63), last[1] || 3600]); body(); return; }
        var rm = e.target.closest('[data-axr]'); if (rm && !rm.disabled){ SY[rm.dataset.axr].pop(); body(); return; }
        var mt = e.target.closest('[data-mt2]'); if (mt){ SY.mat = mt.dataset.mt2; $$('[data-mt2]', dlg.el).forEach(function(b){ b.classList.toggle('on', b === mt); }); return; }
        if (e.target.closest('#sy-pk')) hideFor(ctx.point('Chỉ điểm cao độ ±0.000 (ví dụ mặt sàn tầng 1)').then(function(r){ if (r.p){ SY.y0 = r.p[1]; SY.elM = 'y'; } }));
        if (e.target.closest('#sy-from')) hideFor(ctx.sel('Chọn các đường trục có sẵn (đường dọc và ngang)').then(function(ids){ var vx = [], hy = []; ids.forEach(function(id){ var e2 = d.byId[id]; if (e2 && e2.t === 'line'){ var dx = Math.abs(e2.p2[0] - e2.p1[0]), dy = Math.abs(e2.p2[1] - e2.p1[1]); if (dx < dy * .02) vx.push(e2.p1[0]); else if (dy < dx * .02) hy.push(e2.p1[1]); } }); TC.cad.clearSel();
          var mk = function(vals, named){ vals = vals.sort(function(a, b){ return a - b; }).filter(function(v, i, a){ return !i || v - a[i - 1] > 1; }); return vals.map(function(v, i){ return [named(i), i ? Math.round(v - vals[i - 1]) : 0]; }); };
          if (vx.length > 1) SY.hx = mk(vx, function(i){ return String(i + 1); }); if (hy.length > 1) SY.vy = mk(hy, function(i){ return letters(i + 1); }); TC.cmdline.msg(vx.length + hy.length > 2 ? 'Đã lấy ' + SY.hx.length + ' trục ngang, ' + SY.vy.length + ' trục dọc từ bản vẽ.' : 'Cần chọn ít nhất 2 đường dọc hoặc 2 đường ngang.'); }));
      });
      dlg.el.addEventListener('change', function(e){ var t = e.target;
        if (t.id === 'sy-sc'){ SY.sc = Number(t.value.split(':')[1]); pv(); return; }
        if (t.name === 'sye'){ SY.elT = t.value; pv(); } if (t.name === 'sym'){ SY.elM = t.value; pv(); }
        if (t.id === 'sy-dim') SY.dims = t.checked; if (t.id === 'sy-tot') SY.total = t.checked; if (t.id === 'sy-cr') SY.cutRef = t.value; if (t.id === 'sy-dr') SY.detRef = t.value; pv(); });
      dlg.el.addEventListener('input', function(e){ var t = e.target;
        if (t.dataset.ax){ var v = t.dataset.c === '1' ? num(t.value, NaN) : t.value; SY[t.dataset.ax][Number(t.dataset.i)][Number(t.dataset.c)] = t.dataset.c === '1' ? (isFinite(v) ? v : 0) : v; t.classList.toggle('err', t.dataset.c === '1' ? !(v > 0) : !String(v).trim()); pv(); return; }
        if (t.id === 'sy-ev'){ SY.elV = t.value; SY.elM = 'man'; var ok2 = /^([+\-±])?\d+\.\d{3}$/.test(t.value.trim()); t.classList.toggle('err', !ok2); var m = $d('#sy-msg'); m.className = 'fmsg ' + (ok2 ? '' : 'err'); m.innerHTML = ok2 ? '' : ic('warn', 'xs') + 'Cao độ ghi dạng +1.200: dấu, 3 chữ số thập phân.'; $d('#sy-ok').disabled = !ok2; }
        if (t.id === 'sy-cn'){ SY.cutNo = t.value.trim() || '1'; $d('#sy-cnn').textContent = SY.cutNo + '-' + SY.cutNo; } if (t.id === 'sy-dn') SY.detNo = t.value.trim() || 'A'; if (t.id === 'sy-sl') SY.slope = t.value.trim() || '2';
        pv(); });
      dlg.el.addEventListener('numchange', function(e){ var inp = $('input', e.target), id = inp && inp.id; if (id === 'sy-y0') SY.y0 = e.detail; if (id === 'sy-ext') SY.ext = Math.max(5, e.detail); if (id === 'sy-ang') SY.ang = e.detail; pv(); });
      $d('#sy-ok').addEventListener('click', function(){ var k = SY.kind;
        if (k === 'axes' && !axCheck()) return;
        dlg.close(true);
        if (k === 'mat'){ ED.hset.pat = SY.mat; ED.hset.type = 'Mẫu'; TC.save('hset', ED.hset); res(); setTimeout(function(){ TC.cmd.run('HATCH'); }, 0); return; }
        var put = function(label, list){ ED.run('TCSYMBOL', function(){ list.forEach(function(o){ ED.add(ED.make(o)); }); }); V().draw(); ctx.out(label); };
        var flow = k === 'elev' ? function loop(){ return ctx.point('Điểm đặt cao độ' + (SY.elM === 'y' ? ' (giá trị theo Y, gốc ±0.000 tại Y = ' + TC.d(SY.y0) + ')' : ''), { onMove:function(p, temp){ temp.push(ED.temp([elevEnt(d, p)], { real:true })); } }).then(function(r){ if (!r.p) return; var e2 = elevEnt(d, r.p); put('Đã đặt cao độ <b class="mono">' + esc(e2.attrs.CAODO) + '</b>. Chỉ điểm tiếp hoặc Enter để kết thúc.', [e2]); return loop(); }); }
          : k === 'axes' ? function(){ return ctx.point('Điểm giao trục ' + SY.hx[0][0] + ' và ' + SY.vy[0][0] + ' (góc dưới trái lưới trục)', { onMove:function(p, temp){ temp.push(ED.temp(axesEnts(d, p), { real:true })); } }).then(function(r){ if (r.p) put('Đã vẽ lưới ' + SY.hx.length + ' × ' + SY.vy.length + ' trục: đường trục, bóng trục' + (SY.dims ? ', kích thước khoảng cách' : '') + (SY.total ? ', kích thước tổng' : '') + '.', axesEnts(d, r.p)); }); }
          : k === 'cut' ? function(){ return ctx.point('Đầu thứ nhất của đường cắt').then(function(r){ if (!r.p) return; var a = r.p; return ctx.point('Đầu thứ hai', { base:a, onMove:function(p, temp){ temp.push(ED.temp(cutEnts(a, p), { real:true })); } }).then(function(q){ if (!q.p) return; var b = q.p; return ctx.point('Phía nhìn', { base:[(a[0] + b[0]) / 2, (a[1] + b[1]) / 2], rubber:false, onMove:function(p, temp){ temp.push(ED.temp(cutEnts(a, b, p), { real:true })); } }).then(function(s){ if (s.p) put('Đã đặt ký hiệu mặt cắt ' + esc(SY.cutNo + '-' + SY.cutNo) + (SY.cutRef ? ' (vẽ ở tờ ' + esc(SY.cutRef) + ')' : '') + '.', cutEnts(a, b, s.p)); }); }); }); }
          : k === 'slope' ? function(){ return ctx.point('Đầu mũi tên độ dốc (phía cao)').then(function(r){ if (!r.p) return; var a = r.p; return ctx.point('Cuối mũi tên (phía thấp)', { base:a, rubber:false, onMove:function(p, temp){ if (G.dist(a, p) > 1) temp.push(ED.temp(slopeEnts(a, p), { real:true })); } }).then(function(q){ if (q.p) put('Đã đặt độ dốc i = ' + esc(SY.slope + SY.slU) + '.', slopeEnts(a, q.p)); }); }); }
          : function(){ var one = symPreview(d)[0]; return ctx.point('Điểm đặt ' + (k === 'north' ? 'mũi tên hướng Bắc' : 'ký hiệu chi tiết ' + SY.detNo), { onMove:function(p, temp){ temp.push(ED.temp([Object.assign({}, one, { p:p })], { real:true })); } }).then(function(r){ if (!r.p) return; if (k !== 'north') return put('Đã đặt ký hiệu chi tiết ' + esc(SY.detNo) + ' / ' + esc(SY.detRef) + '.', [Object.assign({}, one, { p:r.p })]);
              return ctx.point('Hướng Bắc', { base:r.p, def:String(SY.ang), num:true, dyn:'ang', onMove:function(p, temp){ temp.push(ED.temp([Object.assign({}, one, { p:r.p, rot:G.ang(r.p, p) - 90 })], { real:true })); } }).then(function(q){ var rot = q.num != null ? q.num : q.p ? G.ang(r.p, q.p) - 90 : SY.ang; put('Đã đặt mũi tên hướng Bắc (góc ' + TC.d(rot, 0) + '°).', [Object.assign({}, one, { p:r.p, rot:rot })]); }); }); };
        Promise.resolve(flow()).then(function(){ res(); }, function(err){ res(); if (err !== 'cancel') throw err; });
      });
      body();
    });
  };

  /* ================= Layout v1.0 UI-H11: viewport đa giác, theo đối tượng, VPCLIP, LAYVPI, CHSPACE, EXPORTLAYOUT ================= */
  function curL(){ var d = D(); return d && d.space !== 'model' ? d.layouts[d.space] : null; }
  function vpOf(d, e){ var f = null; d.layouts.forEach(function(L){ L.vports.forEach(function(q){ if (q.id === e.vp) f = q; }); }); return f; }
  // viewport đa giác: vp.clip = các đỉnh chuẩn hóa 0..1 theo khung chữ nhật bao; canvas.js cắt theo chữ nhật, lớp này cắt thêm theo đa giác
  function clipPts(vp){ return vp.clip.map(function(q){ return [vp.x + q[0] * vp.w, vp.y + q[1] * vp.h]; }); }
  function vpPoly(d, e){ var vp = vpOf(d, e); if (!vp || !vp.clip) return; var pts = clipPts(vp); e._p = pts.map(function(p, i){ return { k:'seg', a:p, b:pts[(i + 1) % pts.length], e:e, src:e }; }); }
  var pe0 = TC.prepEnt; TC.prepEnt = function(doc, e, temp){ pe0(doc, e, temp); if (e.t === 'vpframe') vpPoly(doc, e); return e; };
  var pd0 = TC.prepDoc; TC.prepDoc = function(doc){ pd0(doc); doc.layouts.forEach(function(L){ L.ents.forEach(function(e){ if (e.t === 'vpframe') vpPoly(doc, e); }); }); };
  var dm0 = TC.View.prototype.drawModel;
  TC.View.prototype.drawModel = function(ctx, v, onPaper, clip, vp){
    if (!(vp && vp.clip && clip)) return dm0.apply(this, arguments);
    var path = function(){ ctx.beginPath(); vp.clip.forEach(function(q, i){ var x = clip[0] + q[0] * clip[2], y = clip[1] + (1 - q[1]) * clip[3]; if (i) ctx.lineTo(x, y); else ctx.moveTo(x, y); }); ctx.closePath(); };
    ctx.save(); path(); ctx.clip(); dm0.apply(this, arguments); ctx.restore();
    // ponytail: View.draw vẽ viền viewport bằng strokeRect ngay sau lời gọi này; thay một lần bằng viền đa giác (canvas.js hỗ trợ vp.clip thì bỏ)
    if (ctx === this.bx) ctx.strokeRect = function(){ delete ctx.strokeRect; path(); ctx.stroke(); };
  };
  function vpSnap(vp){ return { x:vp.x, y:vp.y, w:vp.w, h:vp.h, c:vp.c.slice(), clip:vp.clip || null }; }
  // biên mới (mm giấy) cho viewport: khung chữ nhật bao theo đa giác, giữ nguyên nội dung Model ở đúng chỗ trên giấy
  function setClip(vp, pts){ var b = bbOf(pts), w = b[2] - b[0], h = b[3] - b[1]; vp.c = [vp.c[0] + (b[0] + w / 2 - vp.x - vp.w / 2) * vp.scale, vp.c[1] + (b[1] + h / 2 - vp.y - vp.h / 2) * vp.scale]; vp.x = b[0]; vp.y = b[1]; vp.w = w; vp.h = h; vp.clip = pts.map(function(p){ return [(p[0] - b[0]) / w, (p[1] - b[1]) / h]; }); }
  // ponytail: polyline có cung (bulge) lấy theo đỉnh; lấy mẫu cung khi cần biên cong chính xác
  function shapePts(e){ var ring = function(c, rx, ry, rot){ var out = [], r = (rot || 0) * Math.PI / 180; for (var i = 0; i < 48; i++){ var a = i / 48 * 2 * Math.PI, x = rx * Math.cos(a), y = ry * Math.sin(a); out.push([c[0] + x * Math.cos(r) - y * Math.sin(r), c[1] + x * Math.sin(r) + y * Math.cos(r)]); } return out; };
    if (e.t === 'pline') return e.closed && e.pts.length > 2 ? e.pts.slice() : null; if (e.t === 'circle') return ring(e.c, e.r, e.r); if (e.t === 'ellipse') return ring(e.c, e.rx, e.ry, e.rot); if (e.t === 'spline') return e.closed ? TC.splinePts(e.pts, true) : null; return null; }
  function polyPrompt(ctx, first){ var pts = [];
    return (function more(){ return ctx.point(pts.length ? 'Điểm kế tiếp' : first, { base:pts[pts.length - 1], opts:pts.length > 2 ? [['Đóng', 'C'], ['Hoàn tác', 'U']] : pts.length ? [['Hoàn tác', 'U']] : null, onMove:function(p, temp){ if (pts.length) temp.push({ k:'poly', pts:pts.concat([p]), closed:pts.length > 1, paper:true, color:TC.cssv('--c-accent'), dash:[5, 4] }); } }).then(function(q){ if (q.kw === 'U'){ pts.pop(); return more(); } if (q.p){ pts.push(q.p); return more(); } if (pts.length > 2) return pts; ctx.out('<span class="err">Cần ít nhất 3 điểm.</span>'); return null; }); })(); }
  function shapePick(ctx, text){ return ED.pick(ctx, text, { types:['pline', 'circle', 'ellipse', 'spline'], bad:'Chọn polyline kín, đường tròn, ellipse hoặc spline kín trong Paper' }).then(function(q){ if (!q.e) return null; var pts = shapePts(q.e); if (!pts){ ctx.out('<span class="err">Đối tượng chưa kín.</span> Dùng PEDIT › Đóng hoặc chọn đối tượng khác.'); return null; } return { pts:pts, e:q.e }; }); }
  // MVIEW › Đa giác, Theo đối tượng (screens7.js gọi)
  SC.mviewShape = function(ctx, L, k){
    var d = D(), mk = function(pts, src){ var r; ED.run('MVIEW', function(){ var b = bbOf(pts); r = SC.addVp(L, b[0], b[1], b[2] - b[0], b[3] - b[1]); r.vp.clip = pts.map(function(p){ return [(p[0] - b[0]) / (b[2] - b[0]), (p[1] - b[1]) / (b[3] - b[1])]; }); TC.prepEnt(d, r.f); if (src) ED.del(src); }); d.sel = [r.f.id]; V().draw(); TC.cad.afterSel(); ctx.out('Đã tạo viewport ' + (src ? 'theo đối tượng (' + (TC.TYPE_NAMES[src.t] || src.t) + ')' : 'đa giác ' + pts.length + ' đỉnh') + ', tỉ lệ 1:' + r.vp.scale + '. Nhấp đúp vào trong để chỉnh view.'); };
    if (k === 'O') return shapePick(ctx, 'Chọn đối tượng kín trong Paper làm biên viewport').then(function(s){ if (s) mk(s.pts, s.e); });
    return polyPrompt(ctx, 'Điểm đầu của viewport đa giác').then(function(pts){ if (pts) mk(pts); });
  };
  IMPL.VPCLIP = function(ctx, pre){
    var d = D(), L = curL(); if (!L){ ctx.out('VPCLIP dùng trong layout. Chuyển sang một tab layout trước.'); return; }
    if (d.activeVp){ d.activeVp = null; V().draw(); A().renderStatus(); }
    var go = function(f){ var vp = vpOf(d, f); if (!vp) return;
      var apply = function(pts, label, src){ ED.run('VPCLIP', function(){ var done = ED.state(function(){ return vpSnap(vp); }, function(s){ Object.assign(vp, s); TC.prepEnt(d, f); V().draw(); }); if (pts) setClip(vp, pts); else vp.clip = null; TC.prepEnt(d, f); done(); if (src) ED.del(src); }); d.sel = [f.id]; V().draw(); TC.cad.afterSel(); ctx.out(label); };
      return ctx.kw('Biên clip mới', [['Đa giác', 'P'], ['Đối tượng', 'O']].concat(vp.clip ? [['Xóa clip', 'D']] : []), 'P').then(function(k){
        if (k === 'D') return apply(null, 'Đã bỏ clip: viewport trở về khung chữ nhật bao.');
        if (k === 'O') return shapePick(ctx, 'Chọn đối tượng kín làm biên clip').then(function(s){ if (s) apply(s.pts, 'Đã clip viewport theo ' + (TC.TYPE_NAMES[s.e.t] || s.e.t) + '.', s.e); });
        return polyPrompt(ctx, 'Điểm đầu của biên clip').then(function(pts){ if (pts) apply(pts, 'Đã clip viewport theo đa giác ' + pts.length + ' đỉnh. Nội dung Model giữ nguyên vị trí trên giấy.'); }); }); };
    if (pre && pre.t === 'vpframe') return go(pre);
    var s = d.sel.length === 1 && d.byId[d.sel[0]]; if (s && s.t === 'vpframe') return go(s);
    return ED.pick(ctx, 'Chọn viewport cần clip', { types:['vpframe'], bad:'Bấm vào viền viewport' }).then(function(r){ if (r.e) return go(r.e); });
  };
  // LAYVPI: layer của đối tượng chọn chỉ hiện ở viewport hiện tại (đóng băng ở các viewport khác)
  IMPL.LAYVPI = function(ctx){
    var d = D(), L = curL(), vp = L && d.activeVp && L.vports.filter(function(q){ return q.id === d.activeVp; })[0];
    if (!L){ ctx.out('LAYVPI dùng trong layout có viewport. Chuyển sang một tab layout trước.'); return; }
    if (!vp){ ctx.out('Nhấp đúp vào trong một viewport để kích hoạt, rồi chạy lại LAYVPI.'); return; }
    return ctx.kw('Cô lập ở', [['Layout hiện tại', 'C'], ['Mọi layout', 'A']], 'C').then(function(k){ return ctx.sel('Chọn đối tượng trên các layer cần cô lập trong viewport này').then(function(ids){
      var names = []; ids.forEach(function(id){ var e = d.byId[id]; if (e && names.indexOf(e.layer) < 0) names.push(e.layer); }); TC.cad.clearSel(); if (!names.length) return;
      var others = []; (k === 'A' ? d.layouts : [L]).forEach(function(L2){ L2.vports.forEach(function(q){ if (q !== vp) others.push(q); }); });
      if (!others.length){ ctx.out('Layout chỉ có một viewport: không cần cô lập.'); return; }
      ED.run('LAYVPI', function(){ var done = ED.state(function(){ return others.map(function(q){ return (q.frz || []).slice(); }); }, function(s){ others.forEach(function(q, i){ q.frz = s[i]; }); V().draw(); }); others.forEach(function(q){ q.frz = (q.frz || []).concat(names.filter(function(n){ return (q.frz || []).indexOf(n) < 0; })); }); done(); });
      V().draw(); A().refreshPalettes(); ctx.out('Layer ' + names.map(function(n){ return '<b>' + esc(n) + '</b>'; }).join(', ') + ' chỉ hiện trong viewport hiện tại; đã đóng băng ở ' + others.length + ' viewport khác. Hoàn tác bằng Ctrl+Z.'); }); });
  };
  // CHSPACE: Model (qua viewport đang kích hoạt) → Paper, hoặc Paper → Model; kích thước trên giấy giữ nguyên theo tỉ lệ viewport
  IMPL.CHSPACE = function(ctx){
    var d = D(), L = curL(); if (!L){ ctx.out('CHSPACE dùng trong layout: kích hoạt viewport để chuyển đối tượng Model ra Paper, hoặc ở Paper để chuyển vào Model.'); return; }
    var toPaper = !!d.activeVp;
    return ctx.sel(toPaper ? 'Chọn đối tượng trong viewport để chuyển ra Paper' : 'Chọn đối tượng Paper để chuyển vào Model').then(function(ids){
      ids = ED.editable(ids, ctx).filter(function(id){ return d.byId[id] && d.byId[id].t !== 'vpframe'; }); if (!ids.length) return;
      var bb = ED.bbox(ids), mid = [(bb[0] + bb[2]) / 2, (bb[1] + bb[3]) / 2], vp = toPaper ? L.vports.filter(function(q){ return q.id === d.activeVp; })[0] : L.vports.filter(function(q){ return mid[0] >= q.x && mid[0] <= q.x + q.w && mid[1] >= q.y && mid[1] <= q.y + q.h; })[0] || L.vports[0];
      if (!vp){ ctx.out('Layout chưa có viewport để chuyển qua.'); return; }
      var ctr = [vp.x + vp.w / 2, vp.y + vp.h / 2], k = toPaper ? 1 / vp.scale : vp.scale, from = toPaper ? vp.c : ctr, to = toPaper ? ctr : vp.c, n = [];
      var t = { f:function(p){ return [to[0] + (p[0] - from[0]) * k, to[1] + (p[1] - from[1]) * k]; }, rot:0, k:k, m:[k, 0, 0, k, to[0] - from[0] * k, to[1] - from[1] * k] };
      ED.run('CHSPACE', function(){ ids.forEach(function(id){ var e = d.byId[id], c = ED.xform(ED.clean(e), t); delete c.id; ED.del(e); n.push(ED.add(c, toPaper ? L.ents : d.ents).id); }); });
      d.sel = []; TC.cad.afterSel(); V().draw();
      ctx.out('Đã chuyển ' + n.length + ' đối tượng từ ' + (toPaper ? 'Model ra Paper' : 'Paper vào Model') + ' qua viewport tỉ lệ 1:' + Math.round(vp.scale) + ' (kích thước ' + (toPaper ? 'chia ' : 'nhân ') + Math.round(vp.scale) + ', vị trí trên giấy giữ nguyên).');
    });
  };
  // EXPORTLAYOUT: layout → Model của bản vẽ mới, 1:1 mm giấy; nội dung viewport thu theo tỉ lệ, cắt theo biên viewport (chữ nhật hoặc đa giác)
  // cắt đoạn thẳng theo đa giác (kể cả đa giác lõm): giữ các khúc có trung điểm nằm trong
  function clipSeg(a, b, poly){ var ts = [0, 1], dx = b[0] - a[0], dy = b[1] - a[1], out = [];
    poly.forEach(function(p, i){ var q = poly[(i + 1) % poly.length], ex = q[0] - p[0], ey = q[1] - p[1], den = dx * ey - dy * ex; if (Math.abs(den) < 1e-12) return; var t = ((p[0] - a[0]) * ey - (p[1] - a[1]) * ex) / den, u = ((p[0] - a[0]) * dy - (p[1] - a[1]) * dx) / den; if (t > 0 && t < 1 && u >= 0 && u <= 1) ts.push(t); });
    ts.sort(function(x, y){ return x - y; });
    for (var i = 0; i < ts.length - 1; i++){ var m = (ts[i] + ts[i + 1]) / 2; if (ts[i + 1] - ts[i] > 1e-9 && G.inPoly([a[0] + dx * m, a[1] + dy * m], poly)) out.push([[a[0] + dx * ts[i], a[1] + dy * ts[i]], [a[0] + dx * ts[i + 1], a[1] + dy * ts[i + 1]]]); }
    return out; }
  // ponytail: cung, chữ, hatch giữ nguyên khi tâm (hoặc mọi đỉnh) nằm trong biên, không cắt dở; cắt chính xác khi cần bản xuất sạch hơn
  function primEnts(e, f, k, poly){ var out = [], inn = function(q){ return G.inPoly(q, poly); }, src = function(pr){ var s = pr.src || e, o = { layer:s.layer === '0' ? e.layer : s.layer }; if (s.color != null) o.color = s.color; else if (e.color != null) o.color = e.color; return o; };
    (e._p || []).forEach(function(pr){ var o = src(pr), c, p;
      if (pr.k === 'seg') clipSeg(f(pr.a), f(pr.b), poly).forEach(function(z){ out.push(Object.assign({}, o, { t:'line', p1:z[0], p2:z[1] })); });
      else if ((pr.k === 'arc' || pr.k === 'circ' || pr.k === 'ell') && inn(c = f(pr.c))) out.push(Object.assign(o, pr.k === 'arc' ? { t:'arc', c:c, r:pr.r * k, a0:pr.a0, a1:pr.a1 } : pr.k === 'circ' ? { t:'circle', c:c, r:pr.r * k } : { t:'ellipse', c:c, rx:pr.rx * k, ry:pr.ry * k, rot:pr.rot, a0:pr.a0, a1:pr.a1 }));
      else if (pr.k === 'text' && inn(p = f(pr.p))) out.push(Object.assign(o, { t:'text', p:p, h:pr.h * k, s:pr.s, rot:pr.rot || 0, al:pr.al || 'l', style:pr.style || 'Standard', bold:pr.bold }));
      else if (pr.k === 'tick' && inn(p = f(pr.p))) out.push(Object.assign(o, { t:'line', p1:[p[0] - .8, p[1] - .8], p2:[p[0] + .8, p[1] + .8] }));
      else if (pr.k === 'tri' && pr.pts.map(f).every(inn)) out.push(Object.assign(o, { t:'hatch', loops:[pr.pts.map(f)], pat:'SOLID' }));
      else if (pr.k === 'hatch'){ var ls = pr.loops.map(function(l){ return l.map(f); }); if (ls.every(function(l){ return l.every(inn); })) out.push(Object.assign(o, { t:'hatch', loops:ls, pat:pr.pat, sc:pr.sc * k, ang:pr.ang || 0 })); }
      else if (pr.k === 'pt' && inn(p = f(pr.p))) out.push(Object.assign(o, { t:'point', p:p })); });
    return out; }
  IMPL.EXPORTLAYOUT = function(ctx){
    var d = D(), L = curL(); if (!L){ ctx.out('EXPORTLAYOUT xuất layout hiện tại. Chuyển sang một tab layout trước.'); return; }
    var base = d.name.replace(/\.\w+$/, '') + '_' + L.name.replace(/[^\w\-]+/g, '-');
    return new Promise(function(res){
      var dlg = TC.dialog({ id:'expl', title:'Xuất layout thành Model', cmd:'EXPORTLAYOUT', code:'UI-H11', w:560, onClose:function(r){ if (r !== true) res(); },
        body:'<div class="stack"><p>Xuất mọi đối tượng nhìn thấy trong layout <b>' + esc(L.name) + '</b> vào Model của một bản vẽ mới, tỉ lệ 1:1 theo mm giấy.</p>' + H.row('Tên tệp', '<input class="inp" id="ex-n" value="' + esc(base) + '" style="width:300px" spellcheck="false"><span class="faint">.dwg</span>', 70) + H.row('Thư mục', '<span class="mono faint" style="font-size:var(--fs-xs)">' + esc(d.path.replace(/\\[^\\]+$/, '')) + '</span>', 70) + '<span class="fmsg" id="ex-m"></span>' +
          '<div class="note">' + ic('info') + '<span>' + L.vports.length + ' viewport: nội dung Model thu theo tỉ lệ viewport và cắt theo khung viewport; khung tên và đối tượng Paper giữ nguyên. Bản vẽ gốc không đổi.</span></div></div>',
        foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="ex-ok">Xuất bản vẽ mới</button>' });
      $('#ex-ok', dlg.el).addEventListener('click', function(){
        var nm = $('#ex-n', dlg.el).value.trim(); if (!nm || /[\\/:*?"<>|]/.test(nm)){ $('#ex-n', dlg.el).classList.add('err'); $('#ex-m', dlg.el).className = 'fmsg err'; $('#ex-m', dlg.el).innerHTML = ic('warn', 'xs') + 'Tên tệp không được trống hoặc chứa \\ / : * ? " &lt; &gt; |'; return; }
        dlg.close(true);
        var nd = TC.newDoc({ name:nm + '.dwg', frame:false }), used = {};
        nd.layers = JSON.parse(JSON.stringify(d.layers)); nd.layerMap = {}; nd.layers.forEach(function(l){ nd.layerMap[l.name] = l; }); nd.layouts = []; nd.views = [];
        L.ents.forEach(function(e){ if (e.t === 'vpframe') return; var c = ED.clean(e); c.id = ++nd.seq; nd.ents.push(c); if (e.t === 'insert') used[e.name] = 1; });
        Object.keys(used).forEach(function(b){ if (d.blocks[b]) nd.blocks[b] = JSON.parse(JSON.stringify(d.blocks[b])); });
        L.vports.forEach(function(vp){ var k = 1 / vp.scale, ctr = [vp.x + vp.w / 2, vp.y + vp.h / 2], f = function(p){ return [ctr[0] + (p[0] - vp.c[0]) * k, ctr[1] + (p[1] - vp.c[1]) * k]; }, hw = vp.w * vp.scale / 2, hh = vp.h * vp.scale / 2, win = [vp.c[0] - hw, vp.c[1] - hh, vp.c[0] + hw, vp.c[1] + hh], poly = vp.clip ? clipPts(vp) : rp(vp.x, vp.y, vp.x + vp.w, vp.y + vp.h);
          d.ents.forEach(function(e){ var b = e._bb; if (!b || e.t === 'xline' || e.t === 'ray' || !TC.entVisible(d, e) || (vp.frz || []).indexOf(e.layer) >= 0 || b[2] < win[0] || b[0] > win[2] || b[3] < win[1] || b[1] > win[3]) return; primEnts(e, f, k, poly).forEach(function(o){ o.id = ++nd.seq; nd.ents.push(o); }); }); });
        nd.initView = { c:[L.paper.w / 2, L.paper.h / 2], h:L.paper.h * 1.12 }; nd.dirty = true;
        TC.prepDoc(nd); A().docs.push(nd); A().openDoc(nd.id); V().zoomExtents();
        TC.toast('Đã xuất layout <b>' + esc(L.name) + '</b> thành Model của <b>' + esc(nd.name) + '</b> (1:1 mm giấy, ' + L.vports.length + ' viewport). Lưu bản vẽ mới bằng Ctrl+S.', { log:false, act:{ label:'Về bản vẽ gốc', fn:function(){ A().activate(d.id); } } });
        res();
      });
    });
  };

  /* ================= danh mục: trạng thái demo của từng màn hình ================= */
  var cat = function(){ return SC.cat; };
  function d1Model(){ var c = cat(), d = c.PRO(); if (d.id !== 'd1') A().activate('d1'); d = D(); if (d.space !== 'model') A().setSpace('model'); return d; }
  function zoom(bb){ var v = V(); v.zoomTo(bb); var d = D(); d.view.s = Math.min(v.W / ((bb[2] - bb[0]) * 1.08), v.H / ((bb[3] - bb[1]) * 1.08)); v.draw(); TC.emit('view'); }
  OPEN['UI-H07'] = function(){
    var d = d1Model(); SS.open = { proj:false, map:false }; SS.q = ''; var L = sheets(), s = L.filter(function(x){ return x.st === 'm'; })[0] || L[0]; SS.sel = s && s.key;
    zoom([30000, -12000, 76000, 19000]); TC.cmd.run('TCSHEETS');
  };
  OPEN['UI-J05'] = function(){ d1Model(); zoom([-16000, -12000, 28000, 19500]); setTimeout(function(){ TC.cmd.run('TCTITLE'); }, 60); };
  OPEN['UI-J06'] = function(){ d1Model(); zoom([124000, -14000, 214000, 22000]); FR.mode = 'arr'; FR.n = 2; FR.m = 1; setTimeout(function(){ TC.cmd.run('TCFRAME'); }, 60); };
  OPEN['UI-J07'] = function(){
    cat().PRO(); A().openDoc('d2'); var d = D(); if (d.space !== 'model') A().setSpace('model');
    var pr = NPRE.filter(function(p){ return /^Dầm/.test(p[0]); })[0]; Object.assign(NUM, pr[1], { tgt:'text', order:'rc', preset:pr[0] });
    d.sel = d.ents.filter(function(e){ return e.t === 'text' && /^D\d 220×400$/.test(e.s); }).map(function(e){ return e.id; }); zoom([-3200, -1800, 13400, 10200]); TC.cad.afterSel(); setTimeout(function(){ TC.cmd.run('TCNUMBER'); }, 60);
  };
  OPEN['UI-J08'] = function(){
    cat().PRO(); A().openDoc('d2'); var d = D(); if (d.space !== 'model') A().setSpace('model');
    var c1 = d.ents.filter(function(e){ return e.t === 'text' && e.s === 'C1' && e.layer === 'KC-TEXT'; })[0];
    if (!c1){ ED.run('TEXT', function(){ c1 = ED.add(ED.make({ t:'text', p:[-640, -560], h:220, s:'C1', style:'Standard', al:'m', layer:'KC-TEXT' })); }); }
    zoom([-3200, -1800, 13400, 10200]); TC.cad.clearSel(); setTimeout(function(){ TC.cmd.run('TCINCCOPY', c1); }, 60);
  };
  OPEN['UI-J09'] = function(){
    var d = d1Model(); TX.tool = 'case'; TX.mode = 'upper';
    d.sel = d.ents.filter(function(e){ return e.t === 'text' && e.layer === 'KT-GHICHU'; }).map(function(e){ return e.id; }); zoom([4600, 3200, 23400, 11000]); TC.cad.afterSel(); setTimeout(function(){ TC.cmd.run('TCTEXT'); }, 60);
  };
  OPEN['UI-J10'] = function(){ d1Model(); SY.kind = 'axes'; SY.y0 = 1000; zoom([118000, -6000, 150000, 16000]); setTimeout(function(){ TC.cmd.run('TCSYMBOL'); }, 60); };
  // layout minh họa phần v1.0: viewport chữ nhật 1:100, viewport đa giác (chữ L) 1:50, viewport tròn (theo đối tượng) 1:20
  OPEN['UI-H11'] = function(){
    var c = cat(), d = c.PRO(); if (d.id !== 'd1') A().activate('d1'); d = D();
    var i = d.layouts.map(function(L){ return L.name; }).indexOf('A3-Chi tiet');
    if (i < 0){ var L = { name:'A3-Chi tiet', paper:{ w:420, h:297, name:'ISO A3 (420,00 × 297,00 mm)' }, margin:[5, 5, 5, 5], ents:[], vports:[], setup:'A3-Phong-KT' }, add = function(o){ o.id = ++d.seq; L.ents.push(o); return o; };
      add({ t:'insert', name:'KHUNG-A3', p:[0, 0], sx:1, sy:1, rot:0, layer:'KT-KHUNG', attrs:{ TENBV:'CHI TIẾT WC, CẦU THANG', KYHIEU:'KT-06', TILE:'1:50; 1:20' } });
      var vp = function(id, x, y, w, h, cc, sc, clip){ L.vports.push({ id:id, x:x, y:y, w:w, h:h, c:cc, scale:sc, locked:false, frz:['KT-KHUNG'], clip:clip || null }); add({ t:'vpframe', vp:id, layer:'KT-VIEWPORT' }); };
      vp('vpd1', 30, 62, 200, 225, [5850, 3300], 100);
      var Lp = [[240, 62], [325, 62], [325, 200], [410, 200], [410, 287], [240, 287]], b = bbOf(Lp);
      vp('vpd2', b[0], b[1], b[2] - b[0], b[3] - b[1], [5800, 6600], 50, Lp.map(function(p){ return [(p[0] - b[0]) / (b[2] - b[0]), (p[1] - b[1]) / (b[3] - b[1])]; }));
      var C = [370, 130], r = 38, cp = []; for (var k = 0; k < 48; k++){ var a = k / 48 * 2 * Math.PI; cp.push([(Math.cos(a) + 1) / 2, (Math.sin(a) + 1) / 2]); }
      vp('vpd3', C[0] - r, C[1] - r, 2 * r, 2 * r, [4700, 8250], 20, cp);
      d.layouts.push(L); TC.prepDoc(d); A().renderLayoutTabs(); i = d.layouts.length - 1; }
    A().setSpace(i); var f = d.layouts[i].ents.filter(function(e){ return e.t === 'vpframe' && e.vp === 'vpd2'; })[0]; if (f){ d.sel = [f.id]; TC.cad.afterSel(); }
    TC.toast('Viewport đa giác (chữ L, 1:50) và theo đối tượng (tròn, 1:20). Tạo thêm: tab Layout › Chữ nhật ▾ › Đa giác, Theo đối tượng; Clip để đổi biên; chuột phải tab layout › Xuất layout thành Model.', { type:'info', log:false, ms:9000 });
  };
})();
