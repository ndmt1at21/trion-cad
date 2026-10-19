/* TrionCAD prototype · Đợt 3: Hatch UI-E01, Mảng UI-E02, E03–E09, lệnh chọn và thuộc tính, lệnh layer của bản biên tập; quản lý tab ngữ cảnh theo vùng chọn. */
(function(){
  'use strict';
  var TC = window.TC, $ = TC.$, $$ = TC.$$, esc = TC.esc, ic = TC.ic, G = TC.geo, IMPL = TC.cmd.impl, ED = TC.ed, T = ED.T, SC = TC.screens, H = SC.h;
  function A(){ return TC.app; } function V(){ return TC.app.view; } function D(){ return TC.app.doc(); }
  function ctip(r, msg){ var s = r && r.s || V().mouse || [100, 100]; TC.ctip($('#cv'), s[0], s[1], msg); }
  function num(v, d){ var n = TC.parseNum(v, 'none'); return n.err ? d : n.v; }

  /* ================= tab ngữ cảnh theo vùng chọn, layout (07.1 §7.11) ================= */
  var OWNED = { mte:1, cmp:1, arr:1, hatchc:1 };
  ED.ctxRefresh = function(){
    var app = A(), d = D(); if (!d || app.active === 'start') return;
    var cur = app.ctxTab && app.ctxTab.id; if (OWNED[cur]) return;
    var t = null;
    if (!TC.isViewer() && !TC.cmdline.busy()){
      var sel = d.sel.map(function(id){ return d.byId[id]; }).filter(Boolean);
      if (sel.length && sel.length <= 50 && sel.every(function(e){ return e.t === 'hatch'; })) t = hatchTab(sel);
      else if (sel.length === 1 && sel[0].t === 'image' && SC.imageTab) t = SC.imageTab(sel[0]);
      else if (sel.length === 1 && sel[0].t === 'vpframe' && SC.layoutTab) t = SC.layoutTab();
      else if (d.space !== 'model' && SC.layoutTab) t = SC.layoutTab();
    }
    if (!t && !cur) return;
    app.setCtxTab(t, t && cur === t.id);
  };
  ['sel', 'space', 'doc', 'cmdend', 'edition'].forEach(function(ev){ TC.on(ev, function(){ setTimeout(ED.ctxRefresh, 0); }); });

  /* ================= Hatch UI-E01 ================= */
  var HS = ED.hset = Object.assign({ pat:'ANSI31', type:'Mẫu', color:null, bg:'Không', transp:0, ang:0, sc:40, assoc:true, anno:false, sep:false, island:'Thường', gap:0, order:'Sau đường biên', keep:'Không giữ', bset:'Viewport hiện hành', layer:'Dùng hiện hành', relPaper:false, cross:false }, TC.load('hset', {}));
  var PATS = SC.PATS = [['ANSI31', 'ANSI', 'Gạch chéo 45°'], ['ANSI32', 'ANSI', 'Thép'], ['ANSI37', 'ANSI', 'Lưới chéo'], ['LINE', 'ANSI', 'Đường song song'], ['NET', 'ANSI', 'Lưới vuông'], ['SOLID', 'Khác', 'Tô đặc'], ['AR-CONC', 'Khác', 'Bê tông'], ['AR-SAND', 'Khác', 'Cát'], ['AR-B816', 'Khác', 'Gạch block'], ['DOTS', 'Khác', 'Chấm'], ['HONEY', 'Khác', 'Tổ ong'], ['EARTH', 'Khác', 'Đất'], ['TCVN-BETONG', 'TCVN', 'Bê tông'], ['TCVN-BTCT', 'TCVN', 'Bê tông cốt thép'], ['TCVN-GACH', 'TCVN', 'Gạch xây'], ['TCVN-DAT', 'TCVN', 'Đất tự nhiên'], ['TCVN-CAT', 'TCVN', 'Cát'], ['TCVN-DA', 'TCVN', 'Đá dăm, sỏi'], ['TCVN-GO', 'TCVN', 'Gỗ'], ['TCVN-KIMLOAI', 'TCVN', 'Kim loại'], ['TCVN-CACHNHIET', 'TCVN', 'Vật liệu cách nhiệt'], ['TCVN-NUOC', 'TCVN', 'Nước']];
  SC.patThumb = function(c, pat, light){ var x = c.getContext('2d'), w = c.width, h = c.height; x.fillStyle = light ? '#FFFFFF' : TC.cssv('--c-canvas'); x.fillRect(0, 0, w, h); x.strokeStyle = x.fillStyle = light ? '#333' : '#C9CED6'; x.lineWidth = 1; if (pat === 'SOLID'){ x.fillRect(4, 4, w - 8, h - 8); return; } x.save(); x.beginPath(); x.rect(2, 2, w - 4, h - 4); x.clip(); TC.hatchPaint(x, pat, Math.max(3, w / 12), 0, [0, 0, w, h]); x.restore(); };
  var HC = { cmd:false, made:[], hover:null };
  function polyArea(l){ return G.polyArea(l); }
  // vùng kín tại điểm: polyline kín, đường tròn, lòng phòng (lỗ của hatch tường); island bên trong là lỗ
  function regionAt(p){
    var d = D(), best = null, ba = Infinity, cand = [];
    ED.spaceList(d).forEach(function(e){
      if (!TC.entVisible(d, e)) return;
      if (e.t === 'pline' && e.closed && e.pts.length > 2) cand.push(e.bulges && e.bulges.some(Boolean) ? arcPoly(e) : e.pts);
      else if (e.t === 'circle') cand.push(Array.from({ length:48 }, function(_, i){ return ED.polar(e.c, e.r, i * 7.5); }));
      else if (e.t === 'hatch' && e.pat !== 'SOLID') e.loops.slice(1).forEach(function(l){ cand.push(l); });
    });
    cand.forEach(function(l){ if (G.inPoly(p, l)){ var a = polyArea(l); if (a < ba && a > 1){ ba = a; best = l; } } });
    if (!best) return null;
    var holes = HS.island === 'Bỏ qua' ? [] : cand.filter(function(l){ return l !== best && polyArea(l) < ba && G.inPoly(l[0], best) && !G.inPoly(p, l); });
    if (HS.island === 'Ngoài cùng') holes = holes.filter(function(l){ return !holes.some(function(o){ return o !== l && G.inPoly(l[0], o); }); });
    return { outer:best, loops:[best].concat(holes), area:ba - holes.reduce(function(s, l){ return s + polyArea(l); }, 0) };
  }
  ED.regionAt = regionAt;
  function arcPoly(e){ var out = []; (e._p || []).forEach(function(pr){ if (pr.k === 'seg') out.push(pr.a); else if (pr.k === 'arc'){ var sw = ED.sweep(pr.a0, pr.a1); for (var i = 0; i < 8; i++) out.push(ED.polar(pr.c, pr.r, pr.a0 + sw * i / 8)); } }); return out; }
  // vùng không kín: khoanh đầu mút hở gần nhất (07.2 UI-E01)
  function gapHint(r){
    var d = D(), p = r.p, ends = [], tol = Math.max(1, HS.gap);
    ED.spaceList(d).forEach(function(e){ if (!TC.entVisible(d, e) || e.t !== 'line') return; if (G.dist(e.p1, p) < 20000) ends.push(e.p1); if (G.dist(e.p2, p) < 20000) ends.push(e.p2); });
    var open = ends.filter(function(q, i){ return !ends.some(function(o, j){ return j !== i && G.dist(o, q) <= tol; }); }).sort(function(a, b){ return G.dist(a, p) - G.dist(b, p); });
    var gap = open[0];
    if (gap){ var near = open.filter(function(q){ return q !== gap; }).sort(function(a, b){ return G.dist(a, gap) - G.dist(b, gap); })[0], w = near ? G.dist(gap, near) : 0; V().temp = [{ k:'poly', pts:Array.from({ length:33 }, function(_, i){ return ED.polar(gap, 260, i * 11.25); }), color:TC.cssv('--c-danger'), w:2 }]; V().drawOver(); setTimeout(function(){ V().temp = []; V().drawOver(); }, 2600); ctip(r, 'Vùng không kín' + (w ? ': khe hở khoảng ' + TC.d(w, 0) + ' mm' : '') + '. Tăng Dung sai khe hở (Tùy chọn) hoặc chọn đối tượng.'); }
    else ctip(r, 'Không tìm thấy vùng kín tại đây');
  }
  function hatchTargets(){ return HC.edit || HC.made; }
  function setHS(k, v){ HS[k] = v; TC.save('hset', HS); var list = hatchTargets(); if (list && list.length){ var fn = function(){ list.forEach(function(e){ ED.mod(e, function(x){ if (k === 'pat'){ x.pat = v; } if (k === 'sc') x.sc = v; if (k === 'ang') x.ang = v; if (k === 'color'){ if (v == null) delete x.color; else x.color = v; } if (k === 'assoc') x.assoc = v; if (k === 'type' && v === 'Tô đặc') x.pat = 'SOLID'; }); }); }; if (HC.cmd) fn(); else ED.run('HATCHEDIT', fn); V().draw(); } A().setCtxTab(hatchTab(HC.edit), true); }
  function hatchTab(edit){
    if (edit && !HC.cmd) HC.edit = edit; else if (!edit) HC.edit = null;
    var IOt = function(label, icon, id, k){ return { t:'s', label:label, icon:icon, id:id, toggle:true, on:function(){ return !!HS[k]; }, act:function(){ setHS(k, !HS[k]); } }; };
    var dd = function(id, label, w, k, list, extra){ return Object.assign({ t:'dd', id:id, label:label, w:w, text:function(){ return String(HS[k]); }, act:function(b){ var r = b.getBoundingClientRect(); TC.menu(list.map(function(x){ var lock = /\(1\.0\)/.test(x); return { label:x, check:HS[k] === x, dis:lock, act:function(){ setHS(k, x); } }; }), r.left, r.bottom + 2, { minWidth:r.width }); } }, extra || {}); };
    var sel = function(kw){ return function(){ if (HC.cmd) TC.cmdline.submit(kw); else TC.cmd.run('HATCH'); }; };
    var catOf = function(){ var p = PATS.filter(function(x){ return x[0] === HS.pat; })[0]; return p ? p[1] : 'ANSI'; };
    return { id:HC.cmd ? 'hatchc' : 'hatch', name:edit && !HC.cmd ? 'Sửa hatch' : 'Tạo hatch', ctx:true, panels:[
      { name:'Biên', icon:'hatch', prio:2, items:[ { t:'lg', label:'Chọn\nđiểm', icon:'idpoint', id:'h-pick', desc:'Bấm vào trong vùng kín để tô. Rê chuột để xem trước vùng.', act:sel('') }, { t:'col', items:[ { t:'s', label:'Chọn đối tượng', icon:'select', id:'h-sel', act:sel('S') }, { t:'s', label:'Bớt biên', icon:'minus', id:'h-rem', act:function(){ TC.toast('Bớt biên: chọn island cần bỏ khỏi vùng tô.', { type:'info', log:false }); } }, { t:'s', label:'Tạo lại biên', icon:'refresh', id:'h-rec', act:function(){ var l = hatchTargets() || []; if (!l.length) return; ED.run('Tạo lại biên', function(){ l.forEach(function(e){ ED.add(ED.make({ t:'pline', pts:e.loops[0].slice(), closed:true, layer:e.layer })); }); }); TC.cmdline.msg('Đã tạo lại đường biên cho ' + l.length + ' hatch.'); } } ] },
        { t:'col', gap:3, items:[ { t:'s', label:'Hiện đối tượng biên', icon:'eye', id:'h-show', act:function(){ var l = hatchTargets() || []; V().temp = l.map(function(e){ return { k:'poly', pts:e.loops[0], closed:true, color:TC.cssv('--c-accent'), w:2 }; }); V().drawOver(); } }, dd('h-keep', 'Giữ đường biên', 140, 'keep', ['Không giữ', 'Polyline', 'Region']), dd('h-set', 'Tập biên', 140, 'bset', ['Viewport hiện hành', 'Tập chọn hiện tại']) ] } ] },
      { name:'Mẫu', icon:'hatch', prio:1, items:[ { t:'gal', id:'h-gal', label:'Mẫu hatch', w:296, items:PATS, cur:function(){ return PATS.map(function(x){ return x[0]; }).indexOf(HS.pat); },
        paint:function(c, it){ SC.patThumb(c, it[0]); }, tip:function(it){ return it[0] + ' · ' + it[2] + ' (' + it[1] + ')'; },
        hover:function(it){ var l = hatchTargets() || []; l.forEach(function(e){ if (HC.hover == null) e._pat0 = e.pat; e.pat = it ? it[0] : e._pat0; TC.prepEnt(D(), e); }); HC.hover = it ? it[0] : null; V().draw(); },
        pick:function(it){ var l = hatchTargets() || []; l.forEach(function(e){ if (e._pat0) e.pat = e._pat0; delete e._pat0; }); HC.hover = null; setHS('pat', it[0]); if (it[0] === 'SOLID') HS.type = 'Tô đặc'; else HS.type = 'Mẫu'; },
        more:function(b){ SC.hatchGallery(b); } } ] },
      { name:'Thuộc tính', icon:'props', prio:1, items:[ { t:'col', gap:3, items:[ dd('h-type', 'Loại', 130, 'type', ['Mẫu', 'Tô đặc', 'Người dùng', 'Gradient (1.0)']), { t:'dd', id:'h-color', label:'Màu hatch', w:130, html:function(){ return SC.colorSw(HS.color, ED.clayer()); }, text:function(){ return SC.colorLabel(HS.color); }, act:function(){ SC.colorPicker(HS.color).then(function(c){ if (c !== undefined) setHS('color', c); }); } }, dd('h-bg', 'Màu nền', 130, 'bg', ['Không', 'Trắng', 'Xám 8', 'Chọn màu…']) ] },
        { t:'col', gap:3, items:[ { t:'num', id:'h-tr', label:'Trong suốt', w:56, get:function(){ return String(HS.transp); }, set:function(v){ setHS('transp', TC.clamp(Math.round(num(v, 0)), 0, 90)); } }, { t:'num', id:'h-ang', label:'Góc', w:56, get:function(){ return String(HS.ang); }, set:function(v){ setHS('ang', num(v, 0)); } }, { t:'num', id:'h-sc', label:'Tỉ lệ', w:56, get:function(){ return String(HS.sc); }, set:function(v){ var n = num(v, HS.sc); if (n > 0) setHS('sc', n); } } ] },
        { t:'col', items:[ dd('h-lay', 'Layer ghi đè', 140, 'layer', ['Dùng hiện hành', 'KT-HATCH-TUONG', 'KT-NOITHAT', '0']), IOt('Tương đối Paper space', 'layout', 'h-rel', 'relPaper'), IOt('Đan chéo', 'grid', 'h-x', 'cross') ] } ] },
      { name:'Gốc', icon:'idpoint', prio:3, items:[ { t:'lg', label:'Đặt\ngốc', icon:'idpoint', id:'h-org', act:function(){ TC.toast('Đặt gốc: chỉ một điểm, mẫu hatch bắt đầu từ đó (gạch ốp lát thẳng mép).', { type:'info', log:false }); } }, { t:'col', items:[ { t:'s', label:'Gốc theo góc', icon:'move', id:'h-orgc', menuOnly:true, menu:function(){ return ['Dưới trái', 'Dưới phải', 'Trên trái', 'Trên phải', 'Tâm', 'Dùng gốc hiện hành'].map(function(x){ return { label:x, check:HS.origin === x, act:function(){ setHS('origin', x); } }; }); } }, IOt('Lưu làm mặc định', 'save', 'h-orgs', 'orgSave') ] } ] },
      { name:'Tùy chọn', icon:'gear', prio:2, items:[ { t:'col', items:[ IOt('Liên kết', 'group', 'h-assoc', 'assoc'), IOt('Chú thích theo tỉ lệ', 'anno', 'h-anno', 'anno'), { t:'s', label:'Khớp thuộc tính', icon:'match', id:'h-match', menuOnly:true, menu:function(){ return [{ label:'Dùng gốc hatch hiện hành', act:function(){ TC.cmd.run('MATCHPROP'); } }, { label:'Dùng gốc của hatch nguồn', act:function(){ TC.cmd.run('MATCHPROP'); } }]; } } ] },
        { t:'col', gap:3, items:[ { t:'num', id:'h-gap', label:'Dung sai khe hở', w:56, get:function(){ return String(HS.gap); }, set:function(v){ HS.gap = Math.max(0, num(v, 0)); TC.save('hset', HS); } }, IOt('Tạo các hatch riêng', 'copy', 'h-sep', 'sep'), dd('h-isl', 'Nhận diện island', 130, 'island', ['Thường', 'Ngoài cùng', 'Bỏ qua', 'Không']) ] },
        { t:'col', items:[ dd('h-ord', 'Thứ tự hiển thị', 150, 'order', ['Không đổi', 'Sau đường biên', 'Trước đường biên', 'Xuống dưới cùng', 'Lên trên cùng']), { t:'s', label:'Hộp thoại cổ điển', icon:'more', id:'h-classic', act:function(){ TC.toast('Hộp thoại Hatch cổ điển có từ v1.0. Mọi thiết lập có trên tab này.', { type:'info', log:false }); } } ] } ] },
      { name:'Đóng', icon:'close', prio:1, items:[ { t:'lg', label:HC.cmd ? 'Đóng tạo\nhatch' : 'Đóng sửa\nhatch', icon:'close', id:'h-close', act:function(){ if (HC.cmd) TC.cmdline.submit(''); else { TC.cad.clearSel(); } } } ] }
    ] };
  }
  SC.hatchGallery = function(btn){
    var cat = 'Tất cả', q = '';
    var dlg = TC.dialog({ id:'hgal', title:'Mẫu hatch', code:'UI-E01', w:560, modal:false, help:false, body:'<div class="stack"><div class="hrow"><span class="seg" data-seg="hcat">' + ['Tất cả', 'ANSI', 'ISO', 'Khác', 'TCVN', 'Người dùng', 'Tùy biến'].map(function(c){ return '<button type="button" data-v="' + c + '" aria-pressed="' + (c === cat) + '">' + c + '</button>'; }).join('') + '</span><label class="search grow">' + ic('search') + '<input id="hg-q" placeholder="Tìm mẫu: gạch, bê tông, ANSI31" aria-label="Tìm mẫu hatch"></label></div><div class="hgal" id="hg-l"></div></div>', foot:'<span class="faint" style="font-size:var(--fs-s)">Rê lên mẫu để xem trước trên vùng hatch; bấm để chọn.</span><span class="grow"></span><button class="btn primary" type="button" data-x="close">Đóng</button>' });
    function list(){ var F = TC.fold(q), items = PATS.filter(function(p){ return (cat === 'Tất cả' || p[1] === cat) && (!F || TC.fold(p[0] + ' ' + p[2]).indexOf(F) >= 0); });
      $('#hg-l', dlg.el).innerHTML = items.length ? items.map(function(p){ return '<button type="button" class="hgi' + (p[0] === HS.pat ? ' on' : '') + '" data-hp="' + p[0] + '"><canvas width="72" height="52"></canvas><b>' + esc(p[0]) + '</b><span>' + esc(p[2]) + '</span></button>'; }).join('') : '<div class="empty">' + (cat === 'Người dùng' || cat === 'Tùy biến' ? 'Chưa có mẫu. Thêm tệp .pat vào đường dẫn hỗ trợ (Tùy chọn › Tệp).' : 'Không có mẫu khớp “' + esc(q) + '”.') + '</div>';
      $$('[data-hp]', dlg.el).forEach(function(b){ SC.patThumb($('canvas', b), b.dataset.hp); }); }
    dlg.el.addEventListener('segchange', function(e){ cat = e.detail; list(); });
    $('#hg-q', dlg.el).addEventListener('input', function(){ q = this.value; list(); });
    var gal = { hover:function(it){ var c = A().ctxTab; if (!c) return; c.panels.forEach(function(p){ p.items.forEach(function(x){ if (x.t === 'gal' && x.hover) x.hover(it); }); }); } };
    dlg.el.addEventListener('mouseover', function(e){ var b = e.target.closest('[data-hp]'); if (b) gal.hover(PATS.filter(function(p){ return p[0] === b.dataset.hp; })[0]); });
    dlg.el.addEventListener('mouseleave', function(){ gal.hover(null); });
    dlg.el.addEventListener('click', function(e){ var b = e.target.closest('[data-hp]'); if (!b) return; gal.hover(null); var c = A().ctxTab; if (c) c.panels.forEach(function(p){ p.items.forEach(function(x){ if (x.t === 'gal' && x.pick) x.pick(PATS.filter(function(q2){ return q2[0] === b.dataset.hp; })[0]); }); }); else { HS.pat = b.dataset.hp; TC.save('hset', HS); } list(); });
    list();
  };
  IMPL.HATCH = function(ctx){
    ctx.badge('H');
    HC.cmd = true; HC.made = []; HC.edit = null; var marks = [];
    A().setCtxTab(hatchTab());
    ctx.out('Thiết lập hiện tại: mẫu ' + HS.pat + ', tỉ lệ ' + TC.d(HS.sc) + ', góc ' + HS.ang + '°, liên kết: ' + (HS.assoc ? 'có' : 'không') + '. Rê chuột vào vùng kín để xem trước.');
    function make(loops, extra){ var e = ED.make(Object.assign({ t:'hatch', loops:loops, pat:HS.type === 'Tô đặc' ? 'SOLID' : HS.pat, sc:HS.sc, ang:HS.ang, assoc:HS.assoc }, extra || {})); if (HS.color != null) e.color = HS.color; if (HS.layer && HS.layer !== 'Dùng hiện hành' && D().layerMap[HS.layer]) e.layer = HS.layer; return e; }
    function addH(loops){ marks.push(ED.mark()); var e; if (!HS.sep && HC.made.length && HC.made[0].loops){ e = HC.made[0]; ED.mod(e, function(x){ x.loops = x.loops.concat(loops); }); } else { e = ED.add(make(loops)); HC.made.push(e); if (HS.order === 'Xuống dưới cùng') ED.order(e, 0); } V().draw(); return e; }
    function loop(){
      return ctx.point('Chọn điểm bên trong', { pick:true, dyn:false, opts:[['Chọn đối tượng', 'S'], ['Hoàn tác', 'U'], ['Thiết lập', 'T']], onMove:function(p, temp){ var g = regionAt(p); if (g){ g.loops.forEach(function(l, i){ temp.push({ k:'poly', pts:l, closed:true, fill:i ? null : TC.cssv('--c-sel-wf'), color:TC.cssv('--c-accent'), w:i ? 1 : 1.8, dash:i ? [3, 3] : null }); }); } } }).then(function(r){
        if (r.enter) return;
        if (r.kw === 'U'){ if (marks.length){ ED.back(marks.pop()); HC.made = HC.made.filter(function(e){ return D().byId[e.id]; }); } else ctx.out('Không còn bước để hoàn tác.'); return loop(); }
        if (r.kw === 'T'){ TC.toast('Hộp thoại Hatch cổ điển có từ v1.0. Mọi thiết lập có trên tab ngữ cảnh Tạo hatch.', { type:'info', log:false }); return loop(); }
        if (r.kw === 'S') return ctx.sel('Chọn đối tượng kín').then(function(ids){ var d = D(), n = 0; ids.forEach(function(id){ var e = d.byId[id]; if (!e) return; var l = e.t === 'circle' ? Array.from({ length:48 }, function(_, i){ return ED.polar(e.c, e.r, i * 7.5); }) : e.t === 'pline' && e.closed ? (e.bulges && e.bulges.some(Boolean) ? arcPoly(e) : e.pts.slice()) : null; if (l){ addH([l]); n++; } }); TC.cad.clearSel(); ctx.out(n ? 'Đã tô ' + n + ' vùng.' : 'Không có đối tượng kín (polyline đóng, đường tròn) trong vùng chọn.'); return loop(); });
        var g = regionAt(r.p); if (!g){ gapHint(r); return loop(); }
        addH(g.loops); ctx.out('Diện tích vùng tô: ' + TC.d(g.area / 1e6) + ' m²' + (g.loops.length > 1 ? ' (trừ ' + (g.loops.length - 1) + ' island)' : '') + '.');
        return loop();
      });
    }
    function fin(){ HC.cmd = false; HC.made = []; var app = A(); if (app.ctxTab && app.ctxTab.id === 'hatchc') app.setCtxTab(null); setTimeout(ED.ctxRefresh, 0); }
    return ED.run('HATCH', loop).then(fin, function(err){ fin(); throw err; });
  };
  IMPL.HATCHEDIT = function(ctx, pre){ var go = function(e){ D().sel = [e.id]; TC.cad.afterSel(); setTimeout(function(){ ED.ctxRefresh(); TC.cmdline.msg('Sửa hatch trên tab ngữ cảnh <b>Sửa hatch</b>: mẫu, tỉ lệ, góc, màu. Esc để bỏ chọn.'); }, 0); }; if (pre && pre.t) return go(pre); return ED.pick(ctx, 'Chọn hatch', { types:['hatch'] }).then(function(r){ if (r.e) go(r.e); }); };

  /* ================= Mảng UI-E02 ================= */
  var ARR = null;
  function clampInt(v, a, b){ return TC.clamp(Math.round(num(v, a)) || a, a, b || 10000); }
  function mul(A2, B2){ return [A2[0] * B2[0] + A2[2] * B2[1], A2[1] * B2[0] + A2[3] * B2[1], A2[0] * B2[2] + A2[2] * B2[3], A2[1] * B2[2] + A2[3] * B2[3], A2[0] * B2[4] + A2[2] * B2[5] + A2[4], A2[1] * B2[4] + A2[3] * B2[5] + A2[5]]; }
  function arrXf(R){ var out = [], i, j;
    if (R.kind === 'rect'){ for (i = 0; i < R.rows; i++) for (j = 0; j < R.cols; j++) if (i || j) out.push(T.move(j * R.dx, i * R.dy)); return out; }
    var full = Math.abs(R.fill) >= 360, step = R.fill / (full ? R.n : Math.max(1, R.n - 1)) * (R.ccw ? 1 : -1);
    for (i = 1; i < R.n; i++){ var t = T.rot(R.c, step * i); if (!R.rot){ var q = t.f(R.ref); t = T.move(q[0] - R.ref[0], q[1] - R.ref[1]); } out.push(t); }
    return out; }
  function arrGrips(R){ var g = [];
    if (R.kind === 'rect'){ var b = R.base; g.push({ p:b, kind:'sq', k:'base', prompt:'Điểm gốc mới của mảng' }, { p:[b[0] + R.dx, b[1]], kind:'sq', k:'dx', prompt:'Khoảng cách cột' }, { p:[b[0] + (R.cols - 1) * R.dx + R.dx * .35, b[1]], kind:'tri', k:'cols', prompt:'Số cột (kéo để thêm, bớt)' }, { p:[b[0], b[1] + R.dy], kind:'sq', k:'dy', prompt:'Khoảng cách hàng' }, { p:[b[0], b[1] + (R.rows - 1) * R.dy + R.dy * .35], kind:'tri', k:'rows', prompt:'Số hàng (kéo để thêm, bớt)' }); }
    else { g.push({ p:R.c, kind:'sq', k:'c', prompt:'Tâm mới của mảng' }); var t = arrXf(R)[0]; if (t) g.push({ p:t.f(R.ref), kind:'tri', k:'step', prompt:'Góc giữa các phần tử' }); }
    return g; }
  TC.overlays.push(function(ctx, v){ if (!ARR) return; arrGrips(ARR).forEach(function(g){ var s = v.mToS(g.p) || v.toS(g.p); g.s = s; ED.drawGrip(ctx, g, 'cold'); }); });
  function gripSet(R, g, p, final){
    if (g.k === 'base'){ if (!final) return; ED.apply(R.ids, T.move(p[0] - R.base[0], p[1] - R.base[1])); R.base = p; V().draw(); }
    if (g.k === 'dx') R.dx = p[0] - R.base[0] || R.dx; if (g.k === 'dy') R.dy = p[1] - R.base[1] || R.dy;
    if (g.k === 'cols') R.cols = TC.clamp(Math.round((p[0] - R.base[0]) / R.dx) + 1, 1, 200); if (g.k === 'rows') R.rows = TC.clamp(Math.round((p[1] - R.base[1]) / R.dy) + 1, 1, 200);
    if (g.k === 'c') R.c = p; if (g.k === 'step'){ var a = ED.sweep(G.ang(R.c, R.ref), G.ang(R.c, p)); R.fill = Math.min(360, a * (Math.abs(R.fill) >= 360 ? R.n : R.n - 1)); }
  }
  function arrTab(){
    var R = ARR, ref = function(){ A().setCtxTab(arrTab(), true); if (R.pv) R.pv(); }, N = function(id, label, get, set, w){ return { t:'num', id:id, label:label, w:w || 64, get:get, set:function(v){ set(v); ref(); } }; };
    var common = [{ name:'Thuộc tính', icon:'props', prio:2, items:[ { t:'lg', label:'Liên kết', icon:'group', id:'ar-as', toggle:true, on:function(){ return false; }, desc:'Mảng liên kết (sửa lại số lượng, khoảng cách sau khi tạo) có từ v1.0. Prototype tạo mảng không liên kết.', act:function(){ TC.toast('Mảng liên kết có từ v1.0. Ở bản EA mảng luôn tạo dạng không liên kết.', { type:'info', log:false }); } }, { t:'lg', label:'Điểm\ngốc', icon:'idpoint', id:'ar-b', act:function(){ TC.cmdline.submit('B'); } } ].concat(R.kind === 'polar' ? [{ t:'col', items:[ { t:'s', label:'Xoay phần tử', icon:'rotate', id:'ar-rot', toggle:true, on:function(){ return R.rot; }, act:function(){ R.rot = !R.rot; ref(); } }, { t:'s', label:'Ngược chiều kim đồng hồ', icon:'undo', id:'ar-dir', toggle:true, on:function(){ return R.ccw; }, act:function(){ R.ccw = !R.ccw; ref(); } } ] }] : []) },
      { name:'Đóng', icon:'close', prio:1, items:[ { t:'lg', label:'Đóng\nmảng', icon:'close', id:'ar-x', act:function(){ TC.cmdline.submit('X'); } } ] }];
    if (R.kind === 'rect') return { id:'arr', name:'Mảng', ctx:true, panels:[
      { name:'Cột', icon:'array', prio:1, items:[ { t:'col', gap:3, items:[ N('ar-c', 'Số cột', function(){ return String(R.cols); }, function(v){ R.cols = clampInt(v, 1, 500); }), N('ar-cd', 'Khoảng cách', function(){ return TC.d(R.dx); }, function(v){ R.dx = num(v, R.dx) || R.dx; }), N('ar-ct', 'Tổng', function(){ return TC.d(R.dx * (R.cols - 1)); }, function(v){ if (R.cols > 1) R.dx = num(v, R.dx * (R.cols - 1)) / (R.cols - 1); }) ] } ] },
      { name:'Hàng', icon:'array', prio:1, items:[ { t:'col', gap:3, items:[ N('ar-r', 'Số hàng', function(){ return String(R.rows); }, function(v){ R.rows = clampInt(v, 1, 500); }), N('ar-rd', 'Khoảng cách', function(){ return TC.d(R.dy); }, function(v){ R.dy = num(v, R.dy) || R.dy; }), N('ar-rt', 'Tổng', function(){ return TC.d(R.dy * (R.rows - 1)); }, function(v){ if (R.rows > 1) R.dy = num(v, R.dy * (R.rows - 1)) / (R.rows - 1); }) ] }, { t:'col', gap:3, items:[ N('ar-el', 'Cao độ tăng', function(){ return '0.00'; }, function(){}) ] } ] },
      { name:'Cấp', icon:'layers', prio:4, items:[ { t:'col', gap:3, items:[ N('ar-lv', 'Số cấp', function(){ return '1'; }, function(){ TC.toast('Mảng nhiều cấp (3D) có sau v1.0.', { type:'info', log:false }); }), { t:'s', label:'Mảng 3D: sau v1.0', icon:'info', id:'ar-lvn', act:function(){} } ] } ] }].concat(common) };
    return { id:'arr', name:'Mảng', ctx:true, panels:[
      { name:'Phần tử', icon:'array', prio:1, items:[ { t:'col', gap:3, items:[ N('ar-n', 'Số phần tử', function(){ return String(R.n); }, function(v){ R.n = clampInt(v, 2, 1000); }), N('ar-st', 'Góc giữa', function(){ return TC.d(R.fill / (Math.abs(R.fill) >= 360 ? R.n : R.n - 1), 1); }, function(v){ var a = num(v, 60); R.fill = Math.min(360, a * (Math.abs(R.fill) >= 360 ? R.n : R.n - 1)); }), N('ar-f', 'Góc phủ', function(){ return TC.d(R.fill, 0); }, function(v){ R.fill = TC.clamp(num(v, 360), -360, 360) || 360; }) ] } ] },
      { name:'Hàng', icon:'array', prio:3, items:[ { t:'col', gap:3, items:[ N('ar-pr', 'Số hàng', function(){ return '1'; }, function(){ TC.toast('Mảng tròn nhiều hàng: dùng prompt Hàng · ROW.', { type:'info', log:false }); }), N('ar-prd', 'Khoảng cách', function(){ return '0.00'; }, function(){}) ] } ] }].concat(common) };
  }
  function arrayEdit(ctx, R, label){
    ARR = R; A().setCtxTab(arrTab());
    R.pv = function(){ var tmp = arrXf(R).map(function(t){ return ED.xprev(R.ids, t); }); V().temp = tmp; V().drawOver(); };
    var near = function(r){ var best = null, bd = 9; arrGrips(R).forEach(function(g){ var s = V().mToS(g.p) || V().toS(g.p), dd = Math.hypot(s[0] - r.s[0], s[1] - r.s[1]); if (dd < bd){ bd = dd; best = g; } }); return best; };
    function loop(){
      return ctx.point('Chọn grip để sửa mảng', { pick:true, dyn:false, def:'Thoát', opts:[['Liên kết', 'AS'], ['Điểm gốc', 'B'], ['Số lượng', 'COU'], ['Khoảng cách', 'S'], ['Cột', 'COL'], ['Hàng', 'R'], ['Cấp', 'L'], ['Thoát', 'X']], onMove:function(p, temp){ arrXf(R).forEach(function(t){ temp.push(ED.xprev(R.ids, t)); }); } }).then(function(r){
        if (r.enter || r.kw === 'X') return finish();
        var rect = R.kind === 'rect', ref = function(){ A().setCtxTab(arrTab(), true); R.pv(); };
        if (r.kw === 'AS'){ ctx.out('Mảng liên kết có từ v1.0. Ở bản EA mảng tạo dạng không liên kết.'); return loop(); }
        if (r.kw === 'L'){ ctx.out('Cấp (Levels) dùng cho mảng 3D: sau v1.0.'); return loop(); }
        if (r.kw === 'B') return ctx.point(rect ? 'Điểm gốc' : 'Tâm của mảng', { base:rect ? R.base : R.c }).then(function(q){ if (q.p){ if (rect) R.base = q.p; else R.c = q.p; } ref(); return loop(); });
        if (r.kw === 'COU' || r.kw === 'COL') return ctx.val(rect ? 'Số cột' : 'Số phần tử', String(rect ? R.cols : R.n)).then(function(v){ if (rect) R.cols = clampInt(v, 1, 500); else R.n = clampInt(v, 2, 1000); return rect && r.kw === 'COU' ? ctx.val('Số hàng', String(R.rows)).then(function(w){ R.rows = clampInt(w, 1, 500); ref(); return loop(); }) : (ref(), loop()); });
        if (r.kw === 'R') return ctx.val('Số hàng', String(rect ? R.rows : 1)).then(function(v){ if (rect) R.rows = clampInt(v, 1, 500); ref(); return loop(); });
        if (r.kw === 'S') return ctx.val(rect ? 'Khoảng cách giữa các cột' : 'Góc giữa các phần tử', rect ? TC.d(R.dx) : TC.d(R.fill / R.n)).then(function(v){ if (rect){ R.dx = num(v, R.dx) || R.dx; return ctx.val('Khoảng cách giữa các hàng', TC.d(R.dy)).then(function(w){ R.dy = num(w, R.dy) || R.dy; ref(); return loop(); }); } R.fill = Math.min(360, num(v, 60) * R.n); ref(); return loop(); });
        var g = r.s && near(r);
        if (!g){ ctip(r, 'Bấm vào một grip của mảng, hoặc Enter để kết thúc'); return loop(); }
        return ctx.point(g.prompt, { base:g.p, onMove:function(p, temp){ if (g.k === 'base'){ var mv = T.move(p[0] - R.base[0], p[1] - R.base[1]); temp.push(ED.xprev(R.ids, mv)); arrXf(R).forEach(function(t){ temp.push({ k:'xents', list:R.ids.map(function(id){ return D().byId[id]; }), m:mul(t.m, mv.m) }); }); return; } var keep = JSON.stringify([R.dx, R.dy, R.cols, R.rows, R.c, R.fill]); gripSet(R, g, p); arrXf(R).forEach(function(t){ temp.push(ED.xprev(R.ids, t)); }); var k = JSON.parse(keep); R.dx = k[0]; R.dy = k[1]; R.cols = k[2]; R.rows = k[3]; R.c = k[4]; R.fill = k[5]; } }).then(function(q){ if (q.p) gripSet(R, g, q.p, true); ref(); return loop(); });
      });
    }
    function finish(){ V().temp = []; var xs = arrXf(R); xs.forEach(function(t){ ED.apply(R.ids, t, true); }); TC.cad.clearSel(); ctx.out('Đã tạo mảng ' + (R.kind === 'rect' ? R.cols + ' cột × ' + R.rows + ' hàng' : R.n + ' phần tử, góc phủ ' + TC.d(R.fill, 0) + '°') + ' (' + (xs.length * R.ids.length) + ' đối tượng mới, không liên kết).'); }
    var done = function(){ ARR = null; var app = A(); if (app.ctxTab && app.ctxTab.id === 'arr') app.setCtxTab(null); setTimeout(ED.ctxRefresh, 0); };
    ED.begin(label); var mk0 = ED.mark();
    R.pv();
    return loop().then(function(x){ ED.commit(); done(); return x; }, function(err){ V().temp = []; ED.back(mk0); ED.commit(); done(); throw err; });
  }
  IMPL.ARRAYRECT = function(ctx){ ctx.badge('AR'); return ctx.sel().then(function(ids){ ids = ED.editable(ids, ctx); if (!ids.length) return; var bb = ED.bbox(ids), w = bb[2] - bb[0], h = bb[3] - bb[1]; ctx.out('Kiểu = Chữ nhật  Liên kết = Không'); return arrayEdit(ctx, { kind:'rect', ids:ids, cols:4, rows:3, dx:Math.round(w * 1.5) || 1000, dy:Math.round(h * 1.5) || 1000, base:[bb[0], bb[1]] }, 'ARRAYRECT'); }); };
  IMPL.ARRAYPOLAR = function(ctx){ ctx.badge('AR'); return ctx.sel().then(function(ids){ ids = ED.editable(ids, ctx); if (!ids.length) return; ctx.out('Kiểu = Tròn  Liên kết = Không'); return ctx.point('Tâm của mảng', { opts:[['Điểm gốc', 'B'], ['Trục xoay', 'A']] }).then(function(r){ if (!r.p) return; var bb = ED.bbox(ids); return arrayEdit(ctx, { kind:'polar', ids:ids, c:r.p, n:6, fill:360, rot:true, ccw:true, ref:[(bb[0] + bb[2]) / 2, (bb[1] + bb[3]) / 2] }, 'ARRAYPOLAR'); }); }); };
  IMPL.ARRAY = function(ctx){ return ctx.kw('Kiểu mảng', [['Chữ nhật', 'R'], ['Theo đường', 'PA'], ['Tròn', 'PO']], 'R').then(function(k){ if (k === 'PA'){ ctx.out('Mảng theo đường (ARRAYPATH) có từ v1.0.'); return; } return (k === 'PO' ? IMPL.ARRAYPOLAR : IMPL.ARRAYRECT)(ctx); }); };
  // hộp thoại mảng cổ điển, luôn tạo mảng không liên kết
  IMPL.ARRAYCLASSIC = function(ctx){
    var d = D(), ids = d.sel.slice(), kind = 'rect', p = { rows:3, cols:4, dy:1000, dx:1500, ang:0, cx:0, cy:0, n:6, fill:360, rot:true };
    return new Promise(function(res, rej){
      var dlg = TC.dialog({ id:'arrc', title:'Mảng', cmd:'ARRAYCLASSIC', code:'UI-E02', w:620, onClose:function(r){ if (r !== true) res(); },
        body:'<div class="arrc"><div class="stack"><div class="hrow">' + H.rad('ak', 'rect', 'Mảng chữ nhật', true) + H.rad('ak', 'polar', 'Mảng tròn', false) + '</div><div id="ac-f"></div></div><div class="stack ds-btns"><button class="btn" type="button" id="ac-sel">' + ic('select') + 'Chọn đối tượng</button><span class="faint" id="ac-n" style="font-size:var(--fs-s)"></span><canvas id="ac-pv" width="150" height="120" class="pvc"></canvas><button class="btn" type="button" id="ac-prev">Xem trước &lt;</button></div></div>',
        foot:'<span class="faint" style="font-size:var(--fs-s)">Hộp thoại cổ điển luôn tạo mảng không liên kết.</span><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="ac-ok">OK</button>' });
      function form(){ $('#ac-f', dlg.el).innerHTML = kind === 'rect' ? '<div class="grid2">' + H.row('Số hàng', H.num('ac-r', String(p.rows), { prec:0, unit:'none', w:80 }), 70) + H.row('Số cột', H.num('ac-c', String(p.cols), { prec:0, unit:'none', w:80 }), 70) + '</div>' + H.grp('Khoảng cách', H.row('Khoảng cách hàng', H.num('ac-dy', TC.d(p.dy), { pick:true }), 140) + H.row('Khoảng cách cột', H.num('ac-dx', TC.d(p.dx), { pick:true }), 140) + H.row('Góc mảng', H.num('ac-a', '0', { prec:0, unit:'deg', pick:true }), 140)) + '<p class="faint" style="font-size:var(--fs-s)">Khoảng cách âm thì mảng đi xuống dưới hoặc sang trái.</p>'
        : H.grp('Tâm', H.row('X', H.num('ac-cx', TC.d(p.cx), { pick:true }), 30) + H.row('Y', H.num('ac-cy', TC.d(p.cy)), 30)) + H.grp('Cách đặt', H.row('Tổng số phần tử', H.num('ac-n2', String(p.n), { prec:0, unit:'none', w:80 }), 140) + H.row('Góc phủ', H.num('ac-fill', String(p.fill), { prec:0, unit:'deg', w:80 }), 140)) + H.chk('ac-rot', 'Xoay các phần tử khi tạo mảng', p.rot); pv(); }
      function read(){ var g = function(id, dd){ var el = $('#' + id, dlg.el); return el ? num(el.value, dd) : dd; }; p.rows = Math.max(1, Math.round(g('ac-r', p.rows))); p.cols = Math.max(1, Math.round(g('ac-c', p.cols))); p.dy = g('ac-dy', p.dy); p.dx = g('ac-dx', p.dx); p.cx = g('ac-cx', p.cx); p.cy = g('ac-cy', p.cy); p.n = Math.max(2, Math.round(g('ac-n2', p.n))); p.fill = g('ac-fill', p.fill); var rt = $('#ac-rot', dlg.el); if (rt) p.rot = rt.checked; }
      function pv(){ var c = $('#ac-pv', dlg.el), x = c.getContext('2d'); x.fillStyle = TC.cssv('--c-canvas'); x.fillRect(0, 0, 150, 120); x.fillStyle = '#C9CED6'; if (kind === 'rect') for (var i = 0; i < Math.min(p.rows, 6); i++) for (var j = 0; j < Math.min(p.cols, 8); j++) x.fillRect(12 + j * 17, 100 - i * 17, 9, 9); else for (var k = 0; k < Math.min(p.n, 24); k++){ var a = -k * (p.fill >= 360 ? 360 / p.n : p.fill / (p.n - 1)) * Math.PI / 180; x.fillRect(75 + 40 * Math.cos(a) - 4, 60 + 40 * Math.sin(a) - 4, 8, 8); } $('#ac-n', dlg.el).textContent = ids.length ? 'Đã chọn ' + ids.length + ' đối tượng' : 'Chưa chọn đối tượng'; }
      function xfs(){ read(); if (kind === 'rect'){ var a = p.ang * Math.PI / 180, out = []; for (var i = 0; i < p.rows; i++) for (var j = 0; j < p.cols; j++) if (i || j) out.push(T.move(j * p.dx * Math.cos(a) - i * p.dy * Math.sin(a), j * p.dx * Math.sin(a) + i * p.dy * Math.cos(a))); return out; } var bb = ED.bbox(ids); return arrXf({ kind:'polar', n:p.n, fill:p.fill, c:[p.cx, p.cy], rot:p.rot, ccw:true, ref:[(bb[0] + bb[2]) / 2, (bb[1] + bb[3]) / 2] }); }
      dlg.el.addEventListener('change', function(e){ if (e.target.name === 'ak'){ read(); kind = e.target.value; if (kind === 'polar' && ids.length){ var bb = ED.bbox(ids); p.cx = Math.round((bb[0] + bb[2]) / 2); p.cy = Math.round(bb[1] - 2000); } form(); } else { read(); pv(); } });
      $('#ac-sel', dlg.el).addEventListener('click', function(){ dlg.hide(true); TC.cad.clearSel(); ctx.sel('Chọn đối tượng cho mảng').then(function(x){ ids = ED.editable(x, ctx); dlg.hide(false); pv(); }, function(){ dlg.hide(false); }); });
      $('#ac-prev', dlg.el).addEventListener('click', function(){ if (!ids.length){ TC.ctip(dlg.el, 400, 200, 'Chọn đối tượng trước'); return; } dlg.hide(true); V().temp = xfs().map(function(t){ return ED.xprev(ids, t); }); V().drawOver(); ctx.kw('Chấp nhận mảng?', [['Chấp nhận', 'A'], ['Sửa', 'M'], ['Hủy', 'C']], 'A').then(function(k){ V().temp = []; V().drawOver(); if (k === 'A') $('#ac-ok', dlg.el).click(); else if (k === 'C') dlg.close(); else dlg.hide(false); }, function(){ V().temp = []; dlg.hide(false); }); });
      $('#ac-ok', dlg.el).addEventListener('click', function(){ if (!ids.length){ TC.ctip(dlg.el, 400, 200, 'Chọn đối tượng trước'); return; } var list = xfs(); dlg.close(true); ED.run('ARRAYCLASSIC', function(){ list.forEach(function(t){ ED.apply(ids, t, true); }); TC.cad.clearSel(); ctx.out('Đã tạo mảng ' + (list.length + 1) + ' phần tử (không liên kết).'); }); res(); });
      form();
    });
  };

  /* ================= Đường biên UI-E03 ================= */
  IMPL.BOUNDARY = function(ctx){
    return new Promise(function(res){
      var dlg = TC.dialog({ id:'bnd', title:'Tạo đường biên', cmd:'BOUNDARY', code:'UI-E03', w:420, onClose:function(r){ if (r !== 'pick') res(); },
        body:'<div class="stack"><button class="btn" type="button" id="bd-pick" style="justify-self:start">' + ic('idpoint') + 'Chọn điểm</button>' + H.chk('bd-isl', 'Nhận diện island', true) + H.grp('Giữ đường biên', H.row('Loại đối tượng', H.sel('bd-t', ['Polyline', 'Region (1.0)'], 'Polyline', 160), 110)) + H.grp('Tập biên', H.sel('bd-s', ['Viewport hiện hành', 'Tập chọn hiện tại'], 'Viewport hiện hành', 200) + '<button class="btn sm" type="button">' + ic('select') + 'Mới</button>') + '</div>',
        foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button>' });
      $('#bd-pick', dlg.el).addEventListener('click', function(){ var isl = $('#bd-isl', dlg.el).checked, old = HS.island; dlg.close('pick'); var n = 0;
        ED.run('BOUNDARY', function loop(){ return ctx.point('Chọn điểm bên trong', { pick:true, dyn:false, onMove:function(p, temp){ var g = regionAt(p); if (g) temp.push({ k:'poly', pts:g.outer, closed:true, color:TC.cssv('--c-accent'), w:2, fill:TC.cssv('--c-sel-wf') }); } }).then(function(r){ if (r.enter){ ctx.out('Đã tạo ' + n + ' polyline đường biên.'); return; } HS.island = isl ? old : 'Bỏ qua'; var g = regionAt(r.p); HS.island = old; if (!g){ gapHint(r); return loop(); } g.loops.forEach(function(l){ ED.add(ED.make({ t:'pline', pts:l.slice(), closed:true })); n++; }); V().draw(); ctx.out('Đã tạo đường biên, diện tích ' + TC.d(g.area / 1e6) + ' m² (' + TC.d(g.area, 0) + ' mm²), chu vi ' + TC.d(G.polyLen(g.outer, true) / 1000) + ' m.'); return loop(); }); }).then(res, res); });
    });
  };

  /* ================= Xóa trùng lặp UI-E04 ================= */
  IMPL.OVERKILL = function(ctx){
    return ctx.sel('Chọn đối tượng cần kiểm tra trùng').then(function(ids){ if (!ids.length) return;
      return new Promise(function(res){
        var dlg = TC.dialog({ id:'ovk', title:'Xóa đối tượng trùng lặp', cmd:'OVERKILL', code:'UI-E04', w:520, onClose:function(){ res(); },
          body:'<div class="stack">' + H.grp('Thông số', H.row('Dung sai', H.num('ok-tol', '0.000001', { prec:6, unit:'none', w:120 }), 80) + H.gh('Bỏ qua thuộc tính khi so sánh') + '<div class="mpgrid c3">' + ['Màu', 'Layer', 'Linetype', 'Tỉ lệ linetype', 'Độ dày nét', 'Độ trong suốt', 'Plot style', 'Độ dày đùn', 'Vật liệu'].map(function(x){ return H.chk('', x, false); }).join('') + '</div>') +
            H.grp('Tùy chọn', H.chk('ok-pl', 'Tối ưu đoạn trong polyline', true) + '<div style="padding-left:20px" class="stack">' + H.chk('', 'Bỏ qua bề rộng polyline', false) + H.chk('', 'Không phá polyline', false) + '</div>' + H.chk('ok-ov', 'Gộp đoạn thẳng hàng chồng một phần', true) + H.chk('ok-ee', 'Gộp đoạn thẳng hàng nối tiếp', false) + H.chk('', 'Giữ đối tượng liên kết', true)) + '<span class="faint" style="font-size:var(--fs-s)">Kiểm tra ' + TC.n(ids.length) + ' đối tượng đã chọn. Hoàn tác được bằng Ctrl+Z.</span></div>',
          foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="ok-go">Xóa trùng lặp</button>' });
        $('#ok-go', dlg.el).addEventListener('click', function(){ var tol = Math.max(1e-9, num($('#ok-tol', dlg.el).value, 1e-6)), merge = $('#ok-ov', dlg.el).checked, ee = $('#ok-ee', dlg.el).checked, d = D(), lines = [], del = 0, mrg = 0, key = function(e){ return e.layer + '|' + (e.color == null ? '' : e.color) + '|' + (e.lt || ''); };
          dlg.close(true);
          ED.run('OVERKILL', function(){
            var ents = ids.map(function(id){ return d.byId[id]; }).filter(Boolean), seen = [];
            ents.forEach(function(e){ if (!d.byId[e.id]) return; var dup = seen.some(function(o){ if (o.t !== e.t || key(o) !== key(e)) return false; if (e.t === 'line') return (G.dist(o.p1, e.p1) <= tol && G.dist(o.p2, e.p2) <= tol) || (G.dist(o.p1, e.p2) <= tol && G.dist(o.p2, e.p1) <= tol); if (e.t === 'circle' || e.t === 'arc') return G.dist(o.c, e.c) <= tol && Math.abs(o.r - e.r) <= tol && (e.t === 'circle' || (Math.abs(o.a0 - e.a0) < 1e-6 && Math.abs(o.a1 - e.a1) < 1e-6)); if (e.t === 'text') return G.dist(o.p, e.p) <= tol && o.s === e.s; if (e.t === 'insert') return G.dist(o.p, e.p) <= tol && o.name === e.name && o.rot === e.rot; return false; }); if (dup){ ED.del(e); del++; } else { seen.push(e); if (e.t === 'line') lines.push(e); } });
            if (merge || ee){ var groups = {}; lines.forEach(function(e){ if (!d.byId[e.id]) return; var u = [e.p2[0] - e.p1[0], e.p2[1] - e.p1[1]], L = Math.hypot(u[0], u[1]); if (!L) return; u = [u[0] / L, u[1] / L]; if (u[0] < -1e-9 || (Math.abs(u[0]) < 1e-9 && u[1] < 0)) u = [-u[0], -u[1]]; var c = -u[1] * e.p1[0] + u[0] * e.p1[1], k = key(e) + '|' + u[0].toFixed(6) + ',' + u[1].toFixed(6) + '|' + c.toFixed(3); (groups[k] = groups[k] || { u:u, list:[] }).list.push(e); });
              Object.keys(groups).forEach(function(k){ var g = groups[k]; if (g.list.length < 2) return; var proj = function(p){ return p[0] * g.u[0] + p[1] * g.u[1]; }, segs = g.list.map(function(e){ var a = proj(e.p1), b = proj(e.p2); return { e:e, a:Math.min(a, b), b:Math.max(a, b), pa:a < b ? e.p1 : e.p2, pb:a < b ? e.p2 : e.p1 }; }).sort(function(x, y){ return x.a - y.a; }), cur = segs[0];
                for (var i = 1; i < segs.length; i++){ var s = segs[i], touch = s.a <= cur.b + tol; if (touch && (s.a < cur.b - tol ? merge : ee)){ if (s.b > cur.b){ cur.b = s.b; cur.pb = s.pb; } ED.mod(cur.e, function(x){ x.p1 = cur.pa; x.p2 = cur.pb; }); ED.del(s.e); mrg++; } else cur = s; } }); }
            ctx.out(del || mrg ? 'Đã xóa ' + TC.n(del) + ' đối tượng trùng, gộp ' + TC.n(mrg) + ' đoạn.' : 'Không tìm thấy đối tượng trùng lặp.');
            TC.toast(del || mrg ? 'Đã xóa <b>' + TC.n(del) + '</b> đối tượng trùng, gộp <b>' + TC.n(mrg) + '</b> đoạn.' : 'Không có đối tượng trùng lặp.', { log:false, act:del || mrg ? { label:'Hoàn tác', fn:function(){ ED.undo(1); } } : null });
            TC.cad.clearSel();
          }); });
      });
    });
  };

  /* ================= Quản lý nhóm UI-E05 ================= */
  IMPL.CLASSICGROUP = function(ctx){
    var d = D(); d.groups = d.groups || [{ name:'NOITHAT-PK', desc:'Bộ sofa, bàn trà phòng khách', sel:true, ids:d.ents.filter(function(e){ return e.t === 'insert' && /SOFA|BAN-TRA|KE-TIVI/.test(e.name); }).slice(0, 3).map(function(e){ return e.id; }) }, { name:'*A1', desc:'(không tên)', sel:true, ids:[], anon:true }];
    var cur = d.groups[0].name, showAnon = false;
    return new Promise(function(res){
      var dlg = TC.dialog({ id:'grp', title:'Quản lý nhóm', cmd:'CLASSICGROUP', code:'UI-E05', w:600, onClose:function(){ V().temp = []; V().drawOver(); res(); },
        body:'<div class="stack"><div class="tblwrap" style="height:150px"><table class="tbl"><thead><tr><th>Tên nhóm</th><th class="ctr">Chọn được</th><th class="num">Số đối tượng</th></tr></thead><tbody id="gp-l"></tbody></table></div>' + H.chk('gp-anon', 'Hiện nhóm không tên', false) +
          H.grp('Nhận diện nhóm', '<div class="hrow">' + H.row('Tên', '<input class="inp" id="gp-n" style="width:180px">', 40) + H.row('Mô tả', '<input class="inp" id="gp-d" style="width:220px">', 50) + '</div><div class="hrow"><button class="btn sm" type="button" id="gp-find">Tìm tên…</button><button class="btn sm" type="button" id="gp-hl">Tô sáng</button></div>') +
          H.grp('Tạo nhóm', '<div class="hrow"><button class="btn sm" type="button" id="gp-new">' + ic('plus') + 'Mới…</button>' + H.chk('gp-s', 'Chọn được', true) + H.chk('gp-u', 'Không tên', false) + '</div>') +
          H.grp('Sửa nhóm', '<div class="hrow">' + ['Bớt đối tượng', 'Thêm đối tượng', 'Đổi tên', 'Sắp xếp lại…', 'Mô tả', 'Rã nhóm', 'Bật/tắt chọn được'].map(function(x, i){ return '<button class="btn sm" type="button" data-ge="' + i + '">' + x + '</button>'; }).join('') + '</div>') + '</div>',
        foot:'<span class="faint" style="font-size:var(--fs-s)">Bấm một đối tượng thuộc nhóm chọn được thì chọn cả nhóm (PICKSTYLE).</span><span class="grow"></span><button class="btn primary" type="button" data-x="close">Đóng</button>' });
      function G0(){ return d.groups.filter(function(g){ return g.name === cur; })[0]; }
      function list(){ $('#gp-l', dlg.el).innerHTML = d.groups.filter(function(g){ return showAnon || !g.anon; }).map(function(g){ return '<tr data-gn="' + esc(g.name) + '" class="' + (g.name === cur ? 'is-sel' : '') + '"><td>' + esc(g.name) + '</td><td class="ctr">' + (g.sel ? 'Có' : 'Không') + '</td><td class="num">' + g.ids.filter(function(id){ return d.byId[id]; }).length + '</td></tr>'; }).join(''); var g = G0(); $('#gp-n', dlg.el).value = g ? g.name : ''; $('#gp-d', dlg.el).value = g ? g.desc : ''; }
      dlg.el.addEventListener('click', function(e){ var r = e.target.closest('[data-gn]'); if (r){ cur = r.dataset.gn; list(); } var b = e.target.closest('[data-ge]'); if (b){ var g = G0(); if (!g) return; var k = Number(b.dataset.ge);
        if (k === 0 || k === 1){ dlg.hide(true); ctx.sel(k ? 'Chọn đối tượng cần thêm vào nhóm' : 'Chọn đối tượng cần bớt khỏi nhóm').then(function(ids){ if (k) ids.forEach(function(id){ if (g.ids.indexOf(id) < 0) g.ids.push(id); }); else g.ids = g.ids.filter(function(id){ return ids.indexOf(id) < 0; }); TC.cad.clearSel(); dlg.hide(false); list(); }, function(){ dlg.hide(false); }); }
        if (k === 2) SC.ask('Đổi tên nhóm', 'Tên mới', g.name, function(v){ return d.groups.some(function(x){ return x.name === v && x !== g; }) ? 'Đã có nhóm “' + v + '”.' : null; }, 'UI-E05').then(function(v){ if (v){ g.name = v; cur = v; list(); } });
        if (k === 4) SC.ask('Mô tả nhóm', 'Mô tả', g.desc, null, 'UI-E05').then(function(v){ if (v != null){ g.desc = v; list(); } });
        if (k === 5){ d.groups = d.groups.filter(function(x){ return x !== g; }); cur = d.groups[0] ? d.groups[0].name : ''; list(); }
        if (k === 6){ g.sel = !g.sel; list(); }
        if (k === 3) TC.toast('Sắp xếp lại thứ tự đối tượng trong nhóm: chọn vị trí cũ, vị trí mới, số đối tượng.', { type:'info', log:false }); } });
      $('#gp-anon', dlg.el).addEventListener('change', function(){ showAnon = this.checked; list(); });
      $('#gp-hl', dlg.el).addEventListener('click', function(){ var g = G0(); if (!g) return; D().found = g.ids.slice(); V().drawOver(); TC.cmdline.msg('Nhóm ' + esc(g.name) + ': ' + g.ids.length + ' đối tượng đang được tô sáng.'); });
      $('#gp-find', dlg.el).addEventListener('click', function(){ dlg.hide(true); ED.pick(ctx, 'Chọn đối tượng để tìm nhóm').then(function(r){ dlg.hide(false); if (!r.e) return; var g = d.groups.filter(function(x){ return x.ids.indexOf(r.e.id) >= 0; }); TC.toast(g.length ? 'Đối tượng thuộc nhóm: <b>' + g.map(function(x){ return esc(x.name); }).join(', ') + '</b>' : 'Đối tượng không thuộc nhóm nào.', { type:'info', log:false }); }, function(){ dlg.hide(false); }); });
      $('#gp-new', dlg.el).addEventListener('click', function(){ var anon = $('#gp-u', dlg.el).checked, s = $('#gp-s', dlg.el).checked; var go = function(name){ dlg.hide(true); ctx.sel('Chọn đối tượng cho nhóm mới').then(function(ids){ if (ids.length){ d.groups.push({ name:name, desc:$('#gp-d', dlg.el).value, sel:s, ids:ids.slice(), anon:anon }); cur = name; } TC.cad.clearSel(); dlg.hide(false); list(); }, function(){ dlg.hide(false); }); }; if (anon) go('*A' + (d.groups.length + 1)); else SC.ask('Nhóm mới', 'Tên nhóm', 'NHOM-' + (d.groups.length + 1), function(v){ return d.groups.some(function(x){ return x.name === v; }) ? 'Đã có nhóm “' + v + '”.' : /\s/.test(v) ? 'Tên nhóm không chứa khoảng trắng.' : null; }, 'UI-E05').then(function(v){ if (v) go(v); }); });
      list();
    });
  };

  /* ================= Chọn nhanh UI-E06 ================= */
  var QPROPS = { '*':[['Màu', 'color'], ['Layer', 'layer'], ['Linetype', 'lt'], ['Độ dày nét', 'lw']], line:[['Độ dài', 'len']], pline:[['Đóng', 'closed'], ['Diện tích', 'area'], ['Độ dài', 'len']], circle:[['Bán kính', 'r'], ['Diện tích', 'area']], arc:[['Bán kính', 'r']], text:[['Nội dung', 's'], ['Kiểu chữ', 'style'], ['Chiều cao', 'h']], mtext:[['Nội dung', 's'], ['Kiểu chữ', 'style'], ['Chiều cao', 'h']], insert:[['Tên', 'name'], ['Góc xoay', 'rot']], hatch:[['Tên mẫu', 'pat'], ['Tỉ lệ', 'sc'], ['Diện tích', 'area']], dim:[['Kiểu kích thước', 'style'], ['Giá trị đo', 'meas']] };
  function qval(d, e, k){ switch (k){ case 'color': return e.color == null ? 'ByLayer' : String(e.color); case 'lt': return e.lt || 'ByLayer'; case 'lw': return e.lw == null ? 'ByLayer' : TC.d(e.lw); case 'len': return e.t === 'line' ? G.dist(e.p1, e.p2) : G.polyLen(e.pts, e.closed); case 'area': return e.t === 'circle' ? Math.PI * e.r * e.r : e.t === 'hatch' ? Math.abs(e.loops.reduce(function(s, l, i){ return s + (i ? -1 : 1) * G.polyArea(l); }, 0)) : G.polyArea(e.pts); case 'closed': return e.closed ? 'Có' : 'Không'; case 'meas': return e._meas; case 'rot': return e.rot || 0; default: return e[k]; } }
  SC.qvalue = qval;
  IMPL.QSELECT = function(ctx){
    var d = D(), list0 = ED.spaceList(d).filter(function(e){ return TC.entVisible(d, e); }), types = {};
    list0.forEach(function(e){ types[e.t] = (types[e.t] || 0) + 1; });
    return new Promise(function(res){
      var dlg = TC.dialog({ id:'qsel', title:'Chọn nhanh', cmd:'QSELECT', code:'UI-E06', w:540, onClose:function(){ res(); },
        body:'<div class="stack">' + H.row('Áp dụng cho', H.sel('qs-scope', d.sel.length ? ['Vùng chọn hiện tại', 'Toàn bộ bản vẽ'] : ['Toàn bộ bản vẽ'], d.sel.length ? 'Vùng chọn hiện tại' : 'Toàn bộ bản vẽ', 250) + '<button class="ib" type="button" id="qs-pick" data-tip="Chọn đối tượng">' + ic('select') + '</button>', 110) +
          H.row('Loại đối tượng', '<select class="inp" id="qs-t" style="width:250px"><option value="*">Nhiều loại</option>' + Object.keys(types).map(function(t){ return '<option value="' + t + '">' + esc(TC.TYPE_NAMES[t] || t) + ' (' + types[t] + ')</option>'; }).join('') + '</select>', 110) +
          '<div class="frow" style="--lab-w:110px;align-items:start"><span class="lab">Thuộc tính</span><div class="tblwrap" style="height:130px"><div class="tree" id="qs-p"></div></div></div>' +
          H.row('Toán tử', H.sel('qs-op', ['= Bằng', '≠ Khác', '> Lớn hơn', '< Nhỏ hơn', '* Khớp wildcard', 'Chọn tất cả'], '= Bằng', 250), 110) + H.row('Giá trị', '<span id="qs-v"></span>', 110) +
          H.grp('Cách áp dụng', H.rad('qsh', 'in', 'Đưa vào tập chọn mới', true) + H.rad('qsh', 'out', 'Loại khỏi tập chọn mới', false)) + H.chk('qs-add', 'Cộng vào tập chọn hiện tại', false) + '</div>',
        foot:'<button class="btn ghost" type="button" disabled data-tip="Lưu truy vấn có tên: sau v1.0">Lưu truy vấn…</button><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="qs-ok">Chọn</button>' });
      var prop = 'layer';
      function props(){ var t = $('#qs-t', dlg.el).value, ps = QPROPS['*'].concat(t !== '*' ? QPROPS[t] || [] : []); if (!ps.some(function(p){ return p[1] === prop; })) prop = 'layer'; $('#qs-p', dlg.el).innerHTML = ps.map(function(p){ return '<button class="tn" type="button" data-qp="' + p[1] + '" aria-selected="' + (p[1] === prop) + '">' + esc(p[0]) + '</button>'; }).join(''); values(); }
      function pool(){ var t = $('#qs-t', dlg.el).value, sc = $('#qs-scope', dlg.el).value === 'Vùng chọn hiện tại' ? d.sel.map(function(id){ return d.byId[id]; }).filter(Boolean) : list0; return sc.filter(function(e){ return t === '*' || e.t === t; }); }
      function values(){ var vals = {}; pool().forEach(function(e){ var v = qval(d, e, prop); if (v != null && typeof v !== 'number') vals[v] = 1; }); var cat = Object.keys(vals); $('#qs-v', dlg.el).innerHTML = cat.length && cat.length < 60 ? '<select class="inp" id="qs-val" style="width:250px">' + cat.sort().map(function(v){ return '<option>' + esc(v) + '</option>'; }).join('') + '</select>' : '<input class="inp" id="qs-val" style="width:250px" placeholder="' + (prop === 'len' || prop === 'area' || prop === 'r' || prop === 'h' ? 'ví dụ 3000' : 'giá trị') + '">'; }
      dlg.el.addEventListener('click', function(e){ var p = e.target.closest('[data-qp]'); if (p){ prop = p.dataset.qp; props(); } });
      dlg.el.addEventListener('change', function(e){ if (e.target.id === 'qs-t' || e.target.id === 'qs-scope') props(); });
      $('#qs-pick', dlg.el).addEventListener('click', function(){ dlg.hide(true); TC.cad.clearSel(); ctx.sel().then(function(ids){ d.sel = ids; dlg.hide(false); var s = $('#qs-scope', dlg.el); s.innerHTML = SC.opts(['Vùng chọn hiện tại', 'Toàn bộ bản vẽ'], 'Vùng chọn hiện tại'); props(); }, function(){ dlg.hide(false); }); });
      $('#qs-ok', dlg.el).addEventListener('click', function(){ var op = $('#qs-op', dlg.el).value[0], raw = ($('#qs-val', dlg.el) || {}).value || '', nv = parseFloat(raw), re = new RegExp('^' + raw.replace(/[.+^${}()|[\]\\]/g, '\\$&').replace(/\*/g, '.*').replace(/\?/g, '.') + '$', 'i');
        var hit = pool().filter(function(e){ var v = qval(d, e, prop); if (op === 'C') return true; if (op === '*') return re.test(String(v)); if (typeof v === 'number' && !isNaN(nv)){ return op === '=' ? Math.abs(v - nv) < 1e-6 : op === '≠' ? Math.abs(v - nv) >= 1e-6 : op === '>' ? v > nv : v < nv; } return op === '=' ? String(v) === raw : op === '≠' ? String(v) !== raw : false; });
        var out = $('[name="qsh"]:checked', dlg.el).value === 'out', ids = (out ? pool().filter(function(e){ return hit.indexOf(e) < 0; }) : hit).map(function(e){ return e.id; });
        d.sel = $('#qs-add', dlg.el).checked ? d.sel.concat(ids.filter(function(id){ return d.sel.indexOf(id) < 0; })) : ids; dlg.close(true); TC.cad.afterSel(); ctx.out('Đã chọn ' + TC.n(d.sel.length) + ' đối tượng.'); });
      props();
    });
  };

  /* ================= Bộ lọc chọn UI-E07 ================= */
  IMPL.FILTER = function(ctx){
    var d = D(); d.filters = d.filters || { 'Tuong-dai':[['Đối tượng', '=', 'Line'], ['Layer', '=', 'KT-TUONG'], ['Độ dài', '>', '3000']] };
    var rows = [['Đối tượng', '=', 'Line'], ['Layer', '=', 'KT-TUONG']];
    return new Promise(function(res){
      var dlg = TC.dialog({ id:'flt', title:'Bộ lọc chọn', cmd:'FILTER', code:'UI-E07', w:640, onClose:function(){ res(); },
        body:'<div class="stack"><div class="tblwrap" style="height:140px"><table class="tbl"><thead><tr><th>Điều kiện</th><th>Toán tử</th><th>Giá trị</th></tr></thead><tbody id="fl-l"></tbody></table></div><div class="hrow"><button class="btn sm" type="button" id="fl-edit">Sửa dòng</button><button class="btn sm" type="button" id="fl-del">Xóa dòng</button><button class="btn sm" type="button" id="fl-clr">Xóa danh sách</button></div>' +
          H.grp('Chọn bộ lọc', '<div class="hrow">' + H.sel('fl-k', ['Đối tượng', 'Layer', 'Màu', 'Linetype', 'Độ dài', 'Bán kính', 'Tên block', 'Kiểu chữ', '** Bắt đầu AND', '** Kết thúc AND', '** Bắt đầu OR', '** Kết thúc OR', '** Bắt đầu XOR', '** Bắt đầu NOT'], 'Layer', 170) + H.sel('fl-op', ['=', '!=', '<', '>', '<=', '>=', '*'], '=', 60) + '<input class="inp" id="fl-v" style="width:150px" value="KT-CUA"><button class="btn sm" type="button" id="fl-add">Thêm vào danh sách</button><button class="btn sm ghost" type="button" id="fl-from">Thêm từ đối tượng chọn &lt;</button></div>') +
          H.grp('Bộ lọc đã lưu', '<div class="hrow">' + H.sel('fl-saved', Object.keys(d.filters), Object.keys(d.filters)[0], 170) + '<button class="btn sm" type="button" id="fl-load">Nạp</button><input class="inp" id="fl-name" placeholder="Tên bộ lọc" style="width:140px"><button class="btn sm" type="button" id="fl-save">Lưu thành</button><button class="btn sm" type="button" id="fl-dels">Xóa bộ lọc</button></div>') + '</div>',
        foot:'<span class="faint" style="font-size:var(--fs-s)" id="fl-n"></span><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="fl-ok">Áp dụng</button>' });
      var sel = 0, map = { 'Đối tượng':function(e){ return TC.TYPE_NAMES[e.t] || e.t; }, 'Layer':function(e){ return e.layer; }, 'Màu':function(e){ return e.color == null ? 'ByLayer' : String(e.color); }, 'Linetype':function(e){ return e.lt || 'ByLayer'; }, 'Độ dài':function(e){ return e.t === 'line' ? G.dist(e.p1, e.p2) : e.t === 'pline' ? G.polyLen(e.pts, e.closed) : null; }, 'Bán kính':function(e){ return e.r; }, 'Tên block':function(e){ return e.name; }, 'Kiểu chữ':function(e){ return e.style; } };
      function test(e){ var stack = [{ op:'AND', v:[] }]; rows.forEach(function(r){ if (/^\*\* Bắt đầu (\w+)/.test(r[0])) stack.push({ op:RegExp.$1, v:[] }); else if (/^\*\* Kết thúc/.test(r[0])){ var g = stack.pop(), val = g.op === 'OR' ? g.v.some(Boolean) : g.op === 'XOR' ? g.v.filter(Boolean).length === 1 : g.op === 'NOT' ? !g.v[0] : g.v.every(Boolean); stack[stack.length - 1].v.push(val); } else { var f = map[r[0]], v = f ? f(e) : null, x = r[2], n = parseFloat(x), ok; if (v == null) ok = false; else if (typeof v === 'number' && !isNaN(n)) ok = { '=':Math.abs(v - n) < 1e-6, '!=':Math.abs(v - n) >= 1e-6, '<':v < n, '>':v > n, '<=':v <= n, '>=':v >= n }[r[1]]; else ok = r[1] === '*' ? new RegExp('^' + x.replace(/\*/g, '.*') + '$', 'i').test(v) : r[1] === '!=' ? String(v) !== x : String(v) === x; stack[stack.length - 1].v.push(!!ok); } }); return stack[0].v.every(Boolean); }
      function list(){ $('#fl-l', dlg.el).innerHTML = rows.length ? rows.map(function(r, i){ return '<tr data-fr="' + i + '" class="' + (i === sel ? 'is-sel' : '') + '"><td>' + esc(r[0]) + '</td><td class="mono">' + esc(r[1] || '') + '</td><td>' + esc(r[2] || '') + '</td></tr>'; }).join('') : '<tr><td colspan="3" class="faint">Danh sách trống: mọi đối tượng đều khớp.</td></tr>'; var n = ED.spaceList(D()).filter(function(e){ return TC.entVisible(D(), e) && test(e); }).length; $('#fl-n', dlg.el).textContent = TC.n(n) + ' đối tượng khớp bộ lọc'; }
      dlg.el.addEventListener('click', function(e){ var r = e.target.closest('[data-fr]'); if (r){ sel = Number(r.dataset.fr); list(); } });
      $('#fl-add', dlg.el).addEventListener('click', function(){ var k = $('#fl-k', dlg.el).value; rows.splice(sel + 1, 0, /^\*\*/.test(k) ? [k, '', ''] : [k, $('#fl-op', dlg.el).value, $('#fl-v', dlg.el).value]); sel++; list(); });
      $('#fl-del', dlg.el).addEventListener('click', function(){ rows.splice(sel, 1); sel = Math.max(0, sel - 1); list(); });
      $('#fl-clr', dlg.el).addEventListener('click', function(){ rows = []; list(); });
      $('#fl-edit', dlg.el).addEventListener('click', function(){ var r = rows[sel]; if (!r) return; $('#fl-k', dlg.el).value = r[0]; $('#fl-op', dlg.el).value = r[1] || '='; $('#fl-v', dlg.el).value = r[2] || ''; rows.splice(sel, 1); list(); });
      $('#fl-from', dlg.el).addEventListener('click', function(){ dlg.hide(true); ED.pick(ctx, 'Chọn đối tượng mẫu').then(function(r){ dlg.hide(false); if (!r.e) return; rows.push(['Đối tượng', '=', TC.TYPE_NAMES[r.e.t] || r.e.t], ['Layer', '=', r.e.layer]); list(); }, function(){ dlg.hide(false); }); });
      $('#fl-load', dlg.el).addEventListener('click', function(){ rows = JSON.parse(JSON.stringify(d.filters[$('#fl-saved', dlg.el).value] || [])); list(); });
      $('#fl-save', dlg.el).addEventListener('click', function(){ var n = $('#fl-name', dlg.el).value.trim(); if (!n) { $('#fl-name', dlg.el).classList.add('err'); return; } d.filters[n] = JSON.parse(JSON.stringify(rows)); $('#fl-saved', dlg.el).innerHTML = SC.opts(Object.keys(d.filters), n); TC.toast('Đã lưu bộ lọc “' + esc(n) + '”.', { log:false }); });
      $('#fl-dels', dlg.el).addEventListener('click', function(){ delete d.filters[$('#fl-saved', dlg.el).value]; $('#fl-saved', dlg.el).innerHTML = SC.opts(Object.keys(d.filters), ''); });
      $('#fl-ok', dlg.el).addEventListener('click', function(){ var dd = D(), ids = ED.spaceList(dd).filter(function(e){ return TC.entVisible(dd, e) && test(e); }).map(function(e){ return e.id; }); dlg.close(true); dd.sel = ids; TC.cad.afterSel(); ctx.out('Bộ lọc chọn được ' + TC.n(ids.length) + ' đối tượng.'); });
      list();
    });
  };

  /* ================= Chọn tương tự UI-E08 ================= */
  var SS = ED.ssset = TC.load('ssset', { color:false, layer:true, lt:false, lts:false, lw:false, plot:false, style:true, name:true });
  function similar(a, b){ if (a.t !== b.t) return false; if (SS.layer && a.layer !== b.layer) return false; if (SS.color && a.color !== b.color) return false; if (SS.lt && a.lt !== b.lt) return false; if (SS.lw && a.lw !== b.lw) return false; if (SS.style && (a.style || '') !== (b.style || '')) return false; if (SS.name && (a.name || '') !== (b.name || '')) return false; return true; }
  SC.similarSettings = function(){ return new Promise(function(res){ var L = [['color', 'Màu'], ['layer', 'Layer'], ['lt', 'Linetype'], ['lts', 'Tỉ lệ linetype'], ['lw', 'Độ dày nét'], ['plot', 'Plot style'], ['style', 'Kiểu đối tượng (kiểu chữ, kích thước, bảng…)'], ['name', 'Tên (block, xref, ảnh)']];
    var dlg = TC.dialog({ id:'ssset', title:'Thiết lập Chọn tương tự', code:'UI-E08', cmd:'SELECTSIMILAR', w:420, onClose:function(){ res(); }, body:'<div class="stack"><p class="faint" style="font-size:var(--fs-s)">Luôn so khớp loại đối tượng. Thêm các thuộc tính phải giống nhau:</p>' + L.map(function(x){ return H.chk('', x[1], SS[x[0]]).replace('<input', '<input data-ss="' + x[0] + '"'); }).join('') + '</div>', foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="ss-ok">OK</button>' });
    $('#ss-ok', dlg.el).addEventListener('click', function(){ $$('[data-ss]', dlg.el).forEach(function(c){ SS[c.dataset.ss] = c.checked; }); TC.save('ssset', SS); dlg.close(true); }); }); };
  IMPL.SELECTSIMILAR = function(ctx){
    var d = D(), go = function(ids){ var src = ids.map(function(id){ return d.byId[id]; }).filter(Boolean); if (!src.length) return; var out = ED.spaceList(d).filter(function(e){ return TC.entVisible(d, e) && src.some(function(s){ return similar(s, e); }); }).map(function(e){ return e.id; }); d.sel = out; TC.cad.afterSel(); ctx.out('Đã chọn ' + TC.n(out.length) + ' đối tượng tương tự (' + [SS.layer ? 'cùng layer' : '', SS.name ? 'cùng tên' : '', SS.style ? 'cùng kiểu' : '', SS.color ? 'cùng màu' : ''].filter(Boolean).join(', ') + ').'); };
    if (d.sel.length) return go(d.sel.slice());
    return ctx.point('Chọn đối tượng', { pick:true, opts:[['Thiết lập', 'SE']] }).then(function(r){ if (r.kw === 'SE') return SC.similarSettings().then(function(){ return IMPL.SELECTSIMILAR(ctx); }); if (r.s){ var id = V().hitTest(r.s[0], r.s[1]); if (id) return go([id]); } });
  };

  /* ================= Sửa đa tuyến song song UI-E09 ================= */
  var MLT = [['Chữ thập kín', 'CC'], ['Chữ T kín', 'CT'], ['Nối góc', 'CJ'], ['Cắt một', 'CS'], ['Chữ thập hở', 'OC'], ['Chữ T hở', 'OT'], ['Thêm đỉnh', 'AV'], ['Cắt tất cả', 'CA'], ['Chữ thập hợp nhất', 'MC'], ['Chữ T hợp nhất', 'MT'], ['Xóa đỉnh', 'DV'], ['Hàn tất cả', 'WA']];
  function mlIcon(k){ var L = function(d){ return '<path d="' + d + '"/>'; }, h2 = L('M4 18h40M4 24h40'), v2 = L('M18 4v40M24 4v40'), map = {
    CC:L('M4 18h14M24 18h20M4 24h14M24 24h20M18 4v14M24 4v14M18 24v20M24 24v20'), OC:L('M4 18h40M4 24h40M18 4v14M24 4v14M18 24v20M24 24v20'), MC:L('M4 18h14M24 18h20M4 24h14M24 24h20M18 4v14M24 4v14M18 24v20M24 24v20') + '<path d="M18 18h6v6h-6z" stroke-dasharray="2 2"/>',
    CT:L('M4 18h40M4 24h14M24 24h20M18 24v20M24 24v20'), OT:L('M4 18h40M4 24h40M18 24v20M24 24v20'), MT:L('M4 18h40M4 24h14M24 24h20M18 24v20M24 24v20') + '<path d="M18 24h6" stroke-dasharray="2 2"/>',
    CJ:L('M4 18h20v26M4 24h14v20'), AV:h2 + '<circle cx="24" cy="21" r="3"/>', DV:h2 + '<path d="M20 17l8 8M28 17l-8 8"/>', CS:L('M4 18h14M28 18h16M4 24h40'), CA:L('M4 18h14M28 18h16M4 24h14M28 24h16'), WA:h2 + '<path d="M18 14v14M28 14v14" stroke-dasharray="2 2"/>' };
    return '<svg viewBox="0 0 48 48" width="44" height="44" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="square" aria-hidden="true">' + (map[k] || v2) + '</svg>'; }
  IMPL.MLEDIT = function(ctx){
    return new Promise(function(res){
      var dlg = TC.dialog({ id:'mled', title:'Công cụ sửa đa tuyến song song', cmd:'MLEDIT', code:'UI-E09', w:520, onClose:function(){ res(); },
        body:'<div class="stack"><p class="faint" style="font-size:var(--fs-s)">Bấm một công cụ để dùng (tường giao chữ thập, chữ T, nối góc…).</p><div class="mlg">' + MLT.map(function(t){ return '<button type="button" data-ml="' + t[1] + '">' + mlIcon(t[1]) + '<span>' + t[0] + '</span></button>'; }).join('') + '</div></div>', foot:'<span class="grow"></span><button class="btn primary" type="button" data-x="close">Đóng</button>' });
      dlg.el.addEventListener('click', function(e){ var b = e.target.closest('[data-ml]'); if (!b) return; var t = MLT.filter(function(x){ return x[1] === b.dataset.ml; })[0]; dlg.close(true); var has = D().ents.some(function(x){ return x.t === 'mline'; }); ctx.out('<b>' + esc(t[0]) + '</b>: chọn đa tuyến thứ nhất, rồi đa tuyến thứ hai.'); if (!has) ctx.out('Bản vẽ chưa có đa tuyến song song. Vẽ bằng MLINE (v1.0) hoặc mở bản vẽ có MLINE.', 'faint'); });
    });
  };

  /* ================= lệnh chọn, thuộc tính, thứ tự hiển thị ================= */
  IMPL.ADDSELECTED = function(ctx){
    return ED.pick(ctx, 'Chọn đối tượng mẫu', { locked:true }).then(function(r){ if (!r.e) return; var e = r.e, map = { line:'LINE', pline:'PLINE', circle:'CIRCLE', arc:'ARC', ellipse:'ELLIPSE', spline:'SPLINE', text:'TEXT', mtext:'MTEXT', dim:'DIMLINEAR', dimr:e.dia ? 'DIMDIAMETER' : 'DIMRADIUS', dima:'DIMANGULAR', mleader:'MLEADER', hatch:'HATCH', insert:'INSERT', point:'POINT', xline:'XLINE', ray:'RAY', image:'IMAGEATTACH' }, c = map[e.t];
      if (!c){ ctx.out('Không tạo được đối tượng cùng loại với ' + esc(TC.TYPE_NAMES[e.t] || e.t) + '.'); return; }
      var ov = { layer:e.layer }; if (e.color != null) ov.color = e.color; if (e.lt) ov.lt = e.lt; if (e.lw != null) ov.lw = e.lw;
      if (e.t === 'hatch'){ HS.pat = e.pat; HS.sc = e.sc || HS.sc; HS.ang = e.ang || 0; }
      if (e.t === 'text' || e.t === 'mtext'){ ED.cur().tstyle = e.style; ED.cur().th = e.h; }
      setTimeout(function(){ ED.override = ov; TC.cmd.run(c, e.t === 'insert' ? e.name : null); }, 0);
    });
  };
  IMPL.DRAWORDER = function(ctx, pre){
    return ctx.sel().then(function(ids){ if (!ids.length) return;
      var go = function(k){ var d = D(), ents = ids.map(function(id){ return d.byId[id]; }).filter(Boolean);
        var place = function(to){ return ED.run('DRAWORDER', function(){ ents.forEach(function(e, i){ var list = ED.listOf(d, e); ED.order(e, to(list, i)); }); V().draw(); TC.cad.clearSel(); ctx.out('Đã đổi thứ tự hiển thị của ' + ents.length + ' đối tượng.'); }); };
        if (k === 'F') return place(function(list){ return list.length - 1; });
        if (k === 'B') return place(function(list, i){ return i; });
        return ED.pick(ctx, 'Chọn đối tượng tham chiếu').then(function(r){ if (!r.e) return; return place(function(list){ var j = list.indexOf(r.e); return k === 'A' ? Math.min(list.length - 1, j + 1) : Math.max(0, j); }); }); };
      if (pre === 'F' || pre === 'B') return go(pre);
      return ctx.kw('Thứ tự hiển thị', [['Trên đối tượng', 'A'], ['Dưới đối tượng', 'U'], ['Lên trên cùng', 'F'], ['Xuống dưới cùng', 'B']], 'B').then(go); });
  };
  IMPL.TEXTTOFRONT = function(ctx){ return ctx.kw('Đưa lên trên cùng', [['Chữ', 'T'], ['Kích thước', 'D'], ['Đường dẫn', 'L'], ['Tất cả', 'A']], 'A').then(function(k){ var d = D(), test = function(e){ var tx = e.t === 'text' || e.t === 'mtext', dm = /^dim/.test(e.t), ld = e.t === 'mleader'; return k === 'T' ? tx : k === 'D' ? dm : k === 'L' ? ld : tx || dm || ld; }, list = d.ents.filter(test); return ED.run('TEXTTOFRONT', function(){ list.forEach(function(e){ ED.order(e, d.ents.length - 1); }); V().draw(); ctx.out(TC.n(list.length) + ' đối tượng được đưa lên trên cùng.'); }); }); };
  IMPL.HATCHTOBACK = function(ctx){ var d = D(), list = d.ents.filter(function(e){ return e.t === 'hatch'; }); return ED.run('HATCHTOBACK', function(){ list.forEach(function(e, i){ ED.order(e, i); }); V().draw(); ctx.out(TC.n(list.length) + ' hatch được đưa xuống dưới cùng.'); }); };
  IMPL.SETBYLAYER = function(ctx){ ctx.out('Thiết lập hiện tại: Màu, Linetype, Độ dày nét, Độ trong suốt, Vật liệu'); return ctx.sel().then(function(ids){ ids = ED.editable(ids, ctx); if (!ids.length) return; return ctx.kw('Đổi ByBlock thành ByLayer?', [['Có', 'Y'], ['Không', 'N']], 'Y').then(function(){ return ctx.kw('Bao gồm block?', [['Có', 'Y'], ['Không', 'N']], 'Y').then(function(){ return ED.run('SETBYLAYER', function(){ var n = 0; ids.forEach(function(id){ var e = D().byId[id]; if (e.color != null || e.lt || e.lw != null){ ED.mod(e, function(x){ delete x.color; delete x.lt; delete x.lw; }); n++; } }); TC.cad.clearSel(); ctx.out(n + ' đối tượng đã đổi về ByLayer.'); }); }); }); }); };
  // lệnh layer của bản biên tập: hiện hành, khớp, khóa, trạng thái trước (LAYERP)
  function layerSnap(d){ var s = { cl:d.clayer, L:{} }; d.layers.forEach(function(l){ s.L[l.name] = { on:l.on, frz:l.frz, lck:l.lck, color:l.color, lt:l.lt, lw:l.lw }; }); (d.layerHist = d.layerHist || []).push(s); if (d.layerHist.length > 50) d.layerHist.shift(); }
  var lastSnap = 0, set0 = TC.layers.set;
  TC.layers.set = function(d, n, k, v){ if (!TC.isViewer() && Date.now() - lastSnap > 400){ layerSnap(d); lastSnap = Date.now(); } set0(d, n, k, v); if (!TC.isViewer()){ d.dirty = true; TC.emit('dirty', d); } };
  ED.layerSnap = function(d){ layerSnap(d || D()); lastSnap = Date.now(); };
  ED.setCurLayer = function(name, d){ d = d || D(); if (!d.layerMap[name]) return false; var l = d.layerMap[name]; if (l.frz){ TC.toast('Layer <b>' + esc(name) + '</b> đang đóng băng, không đặt hiện hành được.', { type:'warn', log:false }); return false; } ED.layerSnap(d); d.clayer = name; if (!l.on) TC.toast('Layer hiện hành <b>' + esc(name) + '</b> đang tắt: đối tượng vẽ mới sẽ không hiện.', { type:'warn', log:false }); A().renderRibbon(); A().refreshPalettes(); TC.emit('layers'); return true; };
  IMPL.LAYMCUR = function(ctx){ return ED.pick(ctx, 'Chọn đối tượng có layer sẽ thành layer hiện hành', { locked:true }).then(function(r){ if (r.e && ED.setCurLayer(r.e.layer)) ctx.out('<b>' + esc(r.e.layer) + '</b> là layer hiện hành.'); }); };
  IMPL.LAYMCH = function(ctx){ return ctx.sel('Chọn đối tượng cần đổi layer').then(function(ids){ ids = ED.editable(ids, ctx); if (!ids.length) return;
    var go = function(name){ return ED.run('LAYMCH', function(){ ids.forEach(function(id){ ED.mod(D().byId[id], function(x){ x.layer = name; }); }); TC.cad.clearSel(); ctx.out(TC.n(ids.length) + ' đối tượng đã chuyển sang layer <b>' + esc(name) + '</b>.'); }); };
    return ED.pick(ctx, 'Chọn đối tượng trên layer đích', { opts:[['Tên', 'N']], locked:true }).then(function(r){ if (r.kw === 'N') return SC.ask('Đổi sang layer', 'Tên layer đích', ED.clayer(), function(v){ return D().layerMap[v] ? null : 'Không có layer “' + v + '”.'; }, 'UI-C03').then(function(v){ if (v) return go(v); }); if (r.e) return go(r.e.layer); }); }); };
  function lockCmd(ctx, lock){ return ED.pick(ctx, lock ? 'Chọn đối tượng trên layer cần khóa' : 'Chọn đối tượng trên layer cần mở khóa', { locked:true }).then(function(r){ if (!r.e) return; var d = D(), n = r.e.layer; ED.run(lock ? 'LAYLCK' : 'LAYULK', function(){ var done = ED.state(function(){ return { v:d.layerMap[n].lck }; }, function(s){ d.layerMap[n].lck = s.v; V().draw(); A().refreshPalettes(); }); TC.layers.set(d, n, 'lck', lock); done(); }); V().draw(); A().refreshPalettes(); ctx.out('Layer <b>' + esc(n) + '</b> đã ' + (lock ? 'khóa: đối tượng hiện mờ, không sửa được.' : 'mở khóa.')); }); }
  IMPL.LAYLCK = function(ctx){ return lockCmd(ctx, true); };
  IMPL.LAYULK = function(ctx){ return lockCmd(ctx, false); };
  IMPL.LAYERP = function(ctx){ var d = D(), s = (d.layerHist || []).pop(); if (!s){ ctx.out('Không còn trạng thái layer trước đó.'); return; } d.clayer = s.cl; d.layers.forEach(function(l){ var x = s.L[l.name]; if (x) Object.assign(l, x); }); lastSnap = Date.now(); V().draw(); A().renderRibbon(); A().refreshPalettes(); ctx.out('Đã khôi phục trạng thái layer trước đó.'); };
  IMPL.UCS = function(ctx){ ctx.out('Tên UCS hiện tại: *THẾ GIỚI*'); return ctx.point('Gốc UCS', { opts:[['Mặt', 'F'], ['Đặt tên', 'NA'], ['Đối tượng', 'OB'], ['Trước', 'P'], ['View', 'V'], ['Thế giới', 'W'], ['X', 'X'], ['Y', 'Y'], ['Z', 'Z'], ['Trục Z', 'ZA']], def:'Thế giới' }).then(function(r){ if (r.enter || r.kw === 'W'){ ctx.out('UCS hiện hành: Thế giới.'); return; } if (r.p) return ctx.point('Điểm trên trục X', { base:r.p, def:'chấp nhận' }).then(function(q){ ctx.out('Đã đặt UCS mới tại ' + TC.d(r.p[0]) + ', ' + TC.d(r.p[1]) + '. Biểu tượng UCS theo gốc mới (prototype giữ hệ tọa độ thế giới).'); }); ctx.out('Tùy chọn UCS “' + esc(r.kw) + '”: prompt như AutoCAD.'); }); };
  IMPL.PLAN = function(ctx){ return ctx.kw('Nhìn vuông góc mặt bằng', [['UCS hiện hành', 'C'], ['UCS', 'U'], ['Thế giới', 'W']], 'C').then(function(){ V().zoomExtents(); ctx.out('Đang nhìn mặt bằng của UCS hiện hành.'); }); };
  IMPL.BASE = function(ctx){ var d = D(), b = d.basePt || [0, 0]; return ctx.point('Điểm gốc chèn', { def:TC.d(b[0]) + ',' + TC.d(b[1]) + ',0.00' }).then(function(r){ if (r.p){ d.basePt = r.p; d.dirty = true; TC.emit('dirty', d); ctx.out('Điểm gốc chèn của bản vẽ: ' + TC.d(r.p[0]) + ', ' + TC.d(r.p[1]) + '. Dùng khi chèn bản vẽ này làm block hoặc xref.'); } }); };
})();
