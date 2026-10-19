/* TrionCAD prototype · Đợt 2 (Viewer Pro), phần 1: công cụ trên vùng vẽ.
   Palette Đếm UI-G09, Tổng chiều dài / diện tích UI-I04, So sánh bản vẽ UI-I07, Markup UI-M02. */
(function(){
  'use strict';
  var TC = window.TC, $ = TC.$, $$ = TC.$$, esc = TC.esc, ic = TC.ic, G = TC.geo;
  var SC = TC.screens, P = TC.PALETTES, IMPL = TC.cmd.impl;
  function A(){ return TC.app; } function V(){ return TC.app.view; } function D(){ return TC.app.doc(); }
  TC.overlays = TC.overlays || [];
  var proLock = function(){ return TC.can('PRO') ? '' : ' <span class="ed xs" data-ed="pro">Pro</span>'; };
  // điểm của một không gian ('model' hoặc chỉ số layout) ra màn hình
  function S(v, p, sp){ return sp === 'model' ? v.mToS(p) : v.toS(p); }
  // phóng tới hộp bao trong không gian đang xem (Model hoặc giấy của layout)
  function zoomBB(bb, min){
    var v = V(), d = D(); min = min || (d.space === 'model' ? 6000 : 60);
    var cx = (bb[0] + bb[2]) / 2, cy = (bb[1] + bb[3]) / 2, w = Math.max(bb[2] - bb[0], min) / 2, h = Math.max(bb[3] - bb[1], min) / 2; bb = [cx - w, cy - h, cx + w, cy + h];
    if (d.space === 'model') { v.zoomTo(bb); return; }
    v.pushHist(); var pv = v.vw(); pv.cx = (bb[0] + bb[2]) / 2; pv.cy = (bb[1] + bb[3]) / 2;
    pv.s = Math.min(v.W / Math.max(bb[2] - bb[0], 1) / 1.6, v.H / Math.max(bb[3] - bb[1], 1) / 1.6, 8); v.draw(); TC.emit('view');
  }
  TC.zoomBB = zoomBB;
  function union(list){ var b = [Infinity, Infinity, -Infinity, -Infinity]; list.forEach(function(x){ if (!x) return; b = [Math.min(b[0], x[0]), Math.min(b[1], x[1]), Math.max(b[2], x[2]), Math.max(b[3], x[3])]; }); return b; }
  function fake(c, d){ var f = Object.create(TC.View.prototype); f.W = c.width; f.H = c.height; f.doc = d; f.dpr = 1; return f; }
  // ảnh nhỏ của một đối tượng (block) hoặc một vùng thế giới
  function entThumb(c, d, e){
    var x = c.getContext('2d'), bb = e._bb, w = Math.max(bb[2] - bb[0], 1), h = Math.max(bb[3] - bb[1], 1), f = fake(c, d);
    x.fillStyle = TC.cssv('--c-canvas'); x.fillRect(0, 0, c.width, c.height);
    f.drawPrims(x, e, { cx:(bb[0] + bb[2]) / 2, cy:(bb[1] + bb[3]) / 2, s:Math.min((c.width - 12) / w, (c.height - 12) / h) }, false, { color:'#D5D9DF', lw:1 });
  }
  function regionThumb(c, d, bb){ var save = d.initView; d.initView = { c:[(bb[0] + bb[2]) / 2, (bb[1] + bb[3]) / 2], h:Math.max(bb[3] - bb[1], (bb[2] - bb[0]) * c.height / c.width) * 1.04 }; TC.thumb(c, d, 'model'); d.initView = save; }
  TC.regionThumb = regionThumb;
  function exitOnDoc(fn){ TC.on('doc', fn); }

  /* ================= Palette Đếm UI-G09 ================= */
  var CS = { scope:'model', q:'', sort:'n', on:false, name:null, idx:0, items:[] };
  var SCOPES = { 'Model':'model', 'Layout':'layout', 'Toàn bộ':'all' };
  function overlaps(items){ var seen = {}, dup = []; items.forEach(function(it){ var e = it.e, k = it.sp + '|' + Math.round(e.p[0]) + ',' + Math.round(e.p[1]) + '|' + (e.rot || 0) + '|' + e.sx + '|' + e.sy; if (seen[k]) dup.push(it); else seen[k] = it; }); return dup; }
  function countGroups(d, all){
    var list = [], g = {};
    if (CS.scope !== 'layout') d.ents.forEach(function(e){ if (e.t === 'insert') list.push({ e:e, sp:'model' }); });
    if (CS.scope !== 'model') d.layouts.forEach(function(L, i){ L.ents.forEach(function(e){ if (e.t === 'insert') list.push({ e:e, sp:i }); }); });
    list.forEach(function(it){ var k = it.e.name; (g[k] = g[k] || { name:k, items:[], desc:(d.blocks[k] || {}).desc || '' }).items.push(it); });
    var out = Object.keys(g).map(function(k){ g[k].over = overlaps(g[k].items); return g[k]; });
    if (!all && CS.q) out = out.filter(function(x){ return TC.fold(x.name + ' ' + x.desc).indexOf(TC.fold(CS.q)) >= 0; });
    out.sort(function(a, b){ return CS.sort === 'n' ? b.items.length - a.items.length || a.name.localeCompare(b.name) : a.name.localeCompare(b.name); });
    return out;
  }
  P.count = { name:'Đếm', hidden:function(){ return !TC.can('VP'); }, render:function(host, d){
    if (!d){ host.innerHTML = ''; return; }
    var groups = countGroups(d), all = countGroups(d, true), total = all.reduce(function(s, x){ return s + x.items.length; }, 0), dups = all.reduce(function(s, x){ return s + x.over.length; }, 0);
    var cur = Object.keys(SCOPES).filter(function(k){ return SCOPES[k] === CS.scope; })[0];
    host.innerHTML = '<div class="pal-tools">' + SC.seg('cscope', Object.keys(SCOPES), cur) + '<span class="grow"></span><button class="ib" type="button" data-ca="refresh" aria-label="Làm mới" data-tip="Làm mới" data-desc="Đếm lại theo bản vẽ hiện tại.">' + ic('refresh') + '</button><button class="ib" type="button" data-ca="more" aria-label="Thao tác khác">' + ic('more') + '</button></div>' +
      '<div class="pal-tools"><label class="search grow">' + ic('search') + '<input id="cn-q" placeholder="Tìm block, ví dụ cửa" value="' + esc(CS.q) + '" aria-label="Tìm block"></label><button class="dd" type="button" data-ca="sort" style="width:92px"><span class="lbl">' + (CS.sort === 'n' ? 'Số lượng' : 'Tên A–Z') + '</span>' + ic('down', 'xs') + '</button></div>' +
      (dups ? '<div class="pal-note">' + ic('warn') + '<span>' + dups + ' block nằm chồng lên block cùng tên, cùng vị trí. Số đếm có thể bị dư.</span><button class="btn sm" type="button" data-ca="dups" style="margin-left:auto;flex:none">Xem</button></div>' : '') +
      '<div class="pal-body" id="cn-list">' + (groups.length ? groups.map(function(x){ var sel = CS.on && CS.name === x.name;
        return '<div class="cn-row' + (sel ? ' is-sel' : '') + '" role="button" tabindex="0" data-cn="' + esc(x.name) + '" aria-pressed="' + sel + '"><canvas width="84" height="60" data-thumb="' + esc(x.name) + '" aria-hidden="true"></canvas><span class="nm"><b>' + esc(x.name) + '</b><span>' + esc(x.desc || 'Block') + '</span></span>' + (x.over.length ? '<span class="tag warn" title="' + x.over.length + ' block chồng nhau">' + x.over.length + ' chồng</span>' : '') + '<span class="n">' + x.items.length + '</span></div>' +
          (sel ? '<div class="cn-nav"><button class="ib sm" type="button" data-ca="prev" aria-label="Block trước">' + ic('left') + '</button><span class="mono">' + (CS.idx + 1) + ' / ' + CS.items.length + '</span><button class="ib sm" type="button" data-ca="next" aria-label="Block kế">' + ic('right') + '</button><span class="grow"></span><button class="btn sm ghost" type="button" data-ca="exit">Thoát</button></div>' : ''); }).join('')
        : '<div class="empty">' + ic('count') + '<b>' + (CS.q ? 'Không có block khớp “' + esc(CS.q) + '”' : 'Không có block') + '</b>' + (CS.q ? 'Thử từ khóa khác.' : 'Phạm vi này chưa có block nào.') + '</div>') + '</div>' +
      '<div class="pal-foot"><span>' + TC.n(total) + ' block · ' + all.length + ' loại</span><span class="grow"></span><button class="btn sm" type="button" data-ca="xls">' + ic('stats') + 'Xuất Excel</button><button class="btn sm" type="button" data-ca="table" data-tip="Tạo bảng đếm trong bản vẽ" data-desc="Bảng số lượng block chèn vào bản vẽ, cập nhật được. Cần quyền sửa bản vẽ.">Tạo bảng' + proLock() + '</button></div>';
    $$('[data-thumb]', host).forEach(function(c){ var x = all.filter(function(q){ return q.name === c.dataset.thumb; })[0]; if (x) entThumb(c, d, x.items[0].e); });
    var q = $('#cn-q', host);
    q.addEventListener('input', function(){ CS.q = this.value; CS.focusQ = true; P.count.render(host, d); var q2 = $('#cn-q', host); q2.focus(); q2.setSelectionRange(q2.value.length, q2.value.length); });
    host.onclick = function(e){
      var sg = e.target.closest('[data-seg="cscope"] button'); if (sg){ CS.scope = SCOPES[sg.dataset.v]; countExit(); P.count.render(host, d); return; }
      var r = e.target.closest('[data-cn]'); if (r){ if (CS.on && CS.name === r.dataset.cn) countExit(); else countEnter(r.dataset.cn, 0); return; }
      var a = e.target.closest('[data-ca]'); if (!a) return; var k = a.dataset.ca;
      if (k === 'prev' || k === 'next'){ CS.idx = (CS.idx + (k === 'next' ? 1 : -1) + CS.items.length) % CS.items.length; showItem(); }
      else if (k === 'exit') countExit();
      else if (k === 'refresh'){ P.count.render(host, d); TC.cmdline.msg('Đã đếm lại: ' + TC.n(total) + ' block, ' + all.length + ' loại.'); }
      else if (k === 'sort'){ var rr = a.getBoundingClientRect(); TC.menu([{ label:'Số lượng giảm dần', check:CS.sort === 'n', act:function(){ CS.sort = 'n'; P.count.render(host, d); } }, { label:'Tên A–Z', check:CS.sort === 'a', act:function(){ CS.sort = 'a'; P.count.render(host, d); } }], rr.left, rr.bottom + 2, { minWidth:rr.width }); }
      else if (k === 'dups'){ var dd = []; all.forEach(function(x){ x.over.forEach(function(it){ if (it.sp === d.space) dd.push(it.e.id); }); }); d.found = dd; if (dd.length) zoomBB(union(dd.map(function(id){ return d.byId[id]._bb; }))); TC.cmdline.msg('Tô sáng ' + dd.length + ' block chồng nhau. Bản biên tập (Pro) xóa được bằng OVERKILL.'); }
      else if (k === 'xls') TC.toast('Đã xuất <span class="mono">Dem-block_' + esc(d.name.replace(/\.\w+$/, '')) + '.xlsx</span>: ' + all.length + ' loại, ' + TC.n(total) + ' block, cột ảnh xem trước.', { act:{ label:'Mở tệp', fn:function(){} }, kind:'task' });
      else if (k === 'table'){ if (!TC.can('PRO')) SC.upgrade('Tạo bảng đếm trong bản vẽ', 'PRO'); else TC.toast('Chỉ định điểm chèn bảng đếm (bản biên tập, Đợt 3).', { type:'info', log:false }); }
      else if (k === 'more'){ var r2 = a.getBoundingClientRect(); TC.menu([{ label:'Chèn field đếm vào chữ', need:'PRO', act:function(){} }, { label:'Tô sáng mọi block đã đếm', icon:'count', act:function(){ d.found = []; all.forEach(function(x){ x.items.forEach(function(it){ if (it.sp === d.space) d.found.push(it.e.id); }); }); V().drawOver(); } }, { label:'Bỏ tô sáng', act:function(){ countExit(); } }], r2.right, r2.bottom + 2, { alignRight:true }); }
    };
  } };
  TC.count = { enter:function(n){ A().showPalette('count'); countEnter(n, 0); } };
  function countEnter(name, idx){
    var d = D(), x = countGroups(d, true).filter(function(g){ return g.name === name; })[0]; if (!x) return;
    CS.on = true; CS.name = name; CS.items = x.items; CS.idx = TC.clamp(idx || 0, 0, x.items.length - 1);
    showItem();
  }
  function showItem(){
    var d = D(), it = CS.items[CS.idx]; if (!it) return;
    if (it.sp !== d.space) A().setSpace(it.sp);
    d.found = CS.items.filter(function(x){ return x.sp === d.space; }).map(function(x){ return x.e.id; });
    zoomBB(it.e._bb); countBar(); A().refreshPalettes();
    TC.cmdline.status('Đếm <b>' + esc(CS.name) + '</b>: ' + CS.items.length + ' block · đang xem ' + (CS.idx + 1) + ' / ' + CS.items.length);
  }
  function countExit(){ var d = D(); CS.on = false; CS.items = []; if (d) { d.found = []; V().drawOver(); } countBar(); A().refreshPalettes(); }
  function countBar(){
    var cv = $('#cv'), b = $('.cbar', cv);
    if (!CS.on){ if (b) b.remove(); return; }
    if (!b){ b = TC.el('<div class="cbar" role="toolbar" aria-label="Chế độ đếm"></div>'); cv.appendChild(b);
      b.addEventListener('click', function(e){ var t = e.target.closest('[data-cb]'); if (!t) return; if (t.dataset.cb === 'exit') { countExit(); return; } CS.idx = (CS.idx + (t.dataset.cb === 'next' ? 1 : -1) + CS.items.length) % CS.items.length; showItem(); }); }
    b.innerHTML = ic('count') + '<b>' + esc(CS.name) + '</b><span class="muted">' + CS.items.length + ' block</span><span class="sep"></span><button class="ib sm" type="button" data-cb="prev" aria-label="Block trước" data-tip="Block trước">' + ic('left') + '</button><span class="mono">' + (CS.idx + 1) + ' / ' + CS.items.length + '</span><button class="ib sm" type="button" data-cb="next" aria-label="Block kế" data-tip="Block kế">' + ic('right') + '</button><span class="sep"></span><button class="btn sm ghost" type="button" data-cb="exit">Thoát</button>';
    b.style.top = ($('#banners').offsetHeight + 8) + 'px';
  }
  TC.on('sel', function(){ var d = D(); if (CS.on && d && !(d.found || []).length){ CS.on = false; CS.items = []; countBar(); A().refreshPalettes(); } });
  exitOnDoc(function(){ if (CS.on){ CS.on = false; CS.items = []; } countBar(); });
  TC.overlays.push(function(ctx, v){
    var d = v.doc, it = CS.on && CS.items[CS.idx]; if (!it || it.sp !== d.space) return;
    var bb = it.e._bb, a = S(v, [bb[0], bb[3]], it.sp), b = S(v, [bb[2], bb[1]], it.sp); if (!a || !b) return;
    ctx.save(); ctx.strokeStyle = TC.cssv('--c-accent'); ctx.lineWidth = 1.5; ctx.setLineDash([5, 3]); ctx.strokeRect(a[0] - 7, a[1] - 7, b[0] - a[0] + 14, b[1] - a[1] + 14); ctx.restore();
  });
  IMPL.COUNT = function(ctx){
    var d = D(), first = d.sel.map(function(id){ return d.byId[id]; }).filter(function(e){ return e && e.t === 'insert'; })[0];
    A().showPalette('count');
    if (first){ if (first._paper) CS.scope = 'all'; TC.cad.clearSel(); countEnter(first.name, 0); ctx.out('Đếm block <b>' + esc(first.name) + '</b>: ' + CS.items.length + ' block. Dùng ◀ ▶ để duyệt từng cái.'); }
    else ctx.out('Palette Đếm: bấm một block để tô sáng và duyệt từng cái.');
  };

  /* ================= Tổng chiều dài, tổng diện tích UI-I04 ================= */
  var LEN_T = [['line','Line'],['pline','Polyline'],['arc','Cung'],['spline','Spline'],['circle','Đường tròn'],['ellipse','Ellipse']];
  var AREA_T = [['pline','Polyline kín'],['circle','Đường tròn'],['ellipse','Ellipse'],['hatch','Hatch']];
  function lenOf(e){
    if (e.t === 'line') return G.dist(e.p1, e.p2);
    if (e.t === 'pline') return G.polyLen(e.pts, e.closed);
    if (e.t === 'arc') return e.r * (((e.a1 - e.a0) % 360 + 360) % 360) * Math.PI / 180;
    if (e.t === 'circle') return 2 * Math.PI * e.r;
    if (e.t === 'ellipse') return Math.PI * (3 * (e.rx + e.ry) - Math.sqrt((3 * e.rx + e.ry) * (e.rx + 3 * e.ry)));
    return null;
  }
  function areaOf(e){
    if (e.t === 'pline') return e.closed ? G.polyArea(e.pts) : null;
    if (e.t === 'circle') return Math.PI * e.r * e.r;
    if (e.t === 'ellipse') return Math.PI * e.rx * e.ry;
    if (e.t === 'hatch') return Math.abs(e._p[0].loops.reduce(function(s, l, k){ return s + (k ? -1 : 1) * G.polyArea(l); }, 0));
    return null;
  }
  var TS = { unit:{ len:'m', area:'m²' }, prec:2, tab:'layer', off:{}, hl:true };
  function totalsCmd(kind){ return function(ctx){ ctx.badge(kind === 'len' ? 'ΣL' : 'ΣA'); return ctx.sel(kind === 'len' ? 'Chọn các đường cần cộng chiều dài' : 'Chọn các đối tượng kín (polyline kín, hatch, đường tròn)').then(function(ids){ if (!ids.length) return; TC.cad.clearSel(); SC.totals(kind, ids); ctx.out((kind === 'len' ? 'Tổng chiều dài' : 'Tổng diện tích') + ' của ' + ids.length + ' đối tượng đã chọn: xem bảng kết quả.'); }); }; }
  IMPL.TCLENGTH = totalsCmd('len'); IMPL.TCAREA = totalsCmd('area');
  SC.totals = function(kind, ids){
    var old = $('#tt-b'); if (old) $('[data-x="close"]', old.closest('.dlg')).click();
    var d = D(), types = kind === 'len' ? LEN_T : AREA_T, fn = kind === 'len' ? lenOf : areaOf, isL = kind === 'len';
    var all = ids.map(function(id){ return d.byId[id]; }).filter(Boolean), sub = null;
    var dlg = TC.dialog({ id:'tot', title:isL ? 'Tổng chiều dài' : 'Tổng diện tích', cmd:isL ? 'TCLENGTH' : 'TCAREA', code:'UI-I04', w:490, modal:false, body:'<div class="stack" id="tt-b"></div>',
      foot:'<button class="btn" type="button" data-tt="again">' + ic('select') + 'Chọn lại</button><button class="btn ghost" type="button" data-tt="insert" data-tip="Chèn thành text hoặc bảng" data-desc="Chèn kết quả vào bản vẽ. Cần quyền sửa bản vẽ (bản biên tập).">Chèn vào bản vẽ' + proLock() + '</button><span class="grow"></span><button class="btn" type="button" data-tt="copy">' + ic('copyclip') + 'Sao chép</button><button class="btn" type="button" data-tt="xls" aria-haspopup="menu">' + ic('stats') + 'Xuất' + ic('down', 'xs') + '</button>',
      onClose:function(){ d.found = []; V().drawOver(); } });
    var cr = $('#cv').getBoundingClientRect(); dlg.el.style.left = Math.max(12, cr.right - 502) + 'px'; dlg.el.style.top = (cr.top + 44) + 'px';
    function calc(){
      var inc = [], skip = 0, byL = {}, byT = {}, tot = 0, counts = {};
      all.forEach(function(e){ var v = fn(e); if (v == null) { skip++; return; } counts[e.t] = (counts[e.t] || 0) + 1; if (TS.off[kind + e.t]) return; inc.push(e); tot += v;
        (byL[e.layer] = byL[e.layer] || { n:0, v:0, ids:[] }); byL[e.layer].n++; byL[e.layer].v += v; byL[e.layer].ids.push(e.id);
        var tn = types.filter(function(t){ return t[0] === e.t; })[0][1]; (byT[tn] = byT[tn] || { n:0, v:0, ids:[] }); byT[tn].n++; byT[tn].v += v; byT[tn].ids.push(e.id); });
      return { inc:inc, skip:skip, byL:byL, byT:byT, tot:tot, counts:counts };
    }
    function fmt(v){ var u = TS.unit[kind]; return isL ? (u === 'm' ? TC.d(v / 1000, TS.prec) : TC.d(v, TS.prec)) + ' ' + u : (u === 'm²' ? TC.d(v / 1e6, TS.prec) : TC.d(v, TS.prec)) + ' ' + u; }
    var R;
    function render(){
      R = calc(); var g = TS.tab === 'layer' ? R.byL : R.byT, keys = Object.keys(g).sort(function(a, b){ return g[b].v - g[a].v; });
      $('#tt-b', dlg.el).innerHTML =
        '<div class="tt-sum"><div><span class="faint">' + (isL ? 'Tổng chiều dài' : 'Tổng diện tích') + (sub ? ' · ' + esc(sub.name) : '') + '</span><b class="tt-big mono">' + fmt(sub ? sub.v : R.tot) + '</b></div><span class="grow"></span>' + SC.seg('ttu', isL ? ['mm','m'] : ['mm²','m²'], TS.unit[kind]) + '<select class="inp" id="tt-prec" aria-label="Độ chính xác" style="width:auto">' + [0, 1, 2, 3, 4].map(function(p){ return '<option value="' + p + '"' + (p === TS.prec ? ' selected' : '') + '>' + (p ? '0.' + '0'.repeat(p) : '0') + '</option>'; }).join('') + '</select></div>' +
        '<div class="faint" style="font-size:var(--fs-s)">' + TC.n(R.inc.length) + ' đối tượng · ' + Object.keys(R.byL).length + ' layer' + (R.skip ? ' · bỏ qua ' + R.skip + ' đối tượng ' + (isL ? 'không có chiều dài (chữ, kích thước, block, hatch)' : 'không kín (đường hở, chữ, block)') : '') + '</div>' +
        '<div class="grp" style="padding:8px 10px;gap:6px"><span class="gh">Bộ lọc loại</span><div class="hrow" style="gap:4px 12px">' + types.map(function(t){ var n = R.counts[t[0]] || 0; return '<label class="chk' + (n ? '' : ' dis') + '"><input type="checkbox" data-tf="' + t[0] + '"' + (TS.off[kind + t[0]] ? '' : ' checked') + (n ? '' : ' disabled') + '> ' + t[1] + ' <span class="faint">' + n + '</span></label>'; }).join('') + '</div></div>' +
        '<div class="tabs" role="tablist" style="padding:0"><button class="tab" role="tab" type="button" data-tab="layer" aria-selected="' + (TS.tab === 'layer') + '">Theo layer</button><button class="tab" role="tab" type="button" data-tab="type" aria-selected="' + (TS.tab === 'type') + '">Theo loại</button><span class="grow"></span><label class="chk" style="font-size:var(--fs-s)"><input type="checkbox" id="tt-hl"' + (TS.hl ? ' checked' : '') + '> Tô sáng trên bản vẽ</label></div>' +
        '<div class="tblwrap" style="max-height:236px"><table class="tbl"><colgroup><col><col style="width:46px"><col style="width:104px"><col style="width:64px"></colgroup><thead><tr><th>' + (TS.tab === 'layer' ? 'Layer' : 'Loại') + '</th><th class="num">Số</th><th class="num">Tổng</th><th>Tỉ lệ</th></tr></thead><tbody>' +
          keys.map(function(k){ var r = g[k], pct = R.tot ? r.v / R.tot * 100 : 0; return '<tr data-grp="' + esc(k) + '" class="' + (sub && sub.name === k ? 'is-sel' : '') + '" title="Bấm để tô sáng riêng nhóm này"><td>' + esc(k) + '</td><td class="num">' + r.n + '</td><td class="num">' + fmt(r.v) + '</td><td><span class="bar"><i style="width:' + pct.toFixed(1) + '%"></i></span></td></tr>'; }).join('') +
          (keys.length ? '' : '<tr><td colspan="4" class="faint">Không có đối tượng phù hợp với bộ lọc.</td></tr>') + '</tbody></table></div>';
      d.found = TS.hl ? (sub ? sub.ids : R.inc.map(function(e){ return e.id; })) : []; V().drawOver();
    }
    dlg.el.addEventListener('click', function(e){
      var u = e.target.closest('[data-seg="ttu"] button'); if (u){ TS.unit[kind] = u.dataset.v; render(); return; }
      var tb = e.target.closest('[data-tab]'); if (tb){ TS.tab = tb.dataset.tab; sub = null; render(); return; }
      var gr = e.target.closest('[data-grp]'); if (gr){ var g = (TS.tab === 'layer' ? R.byL : R.byT)[gr.dataset.grp]; sub = sub && sub.name === gr.dataset.grp ? null : { name:gr.dataset.grp, v:g.v, ids:g.ids }; render(); return; }
      var a = e.target.closest('[data-tt]'); if (!a) return; var k = a.dataset.tt;
      if (k === 'again'){ dlg.close(); TC.cmd.run(isL ? 'TCLENGTH' : 'TCAREA'); }
      else if (k === 'copy') TC.copyText(fmt(sub ? sub.v : R.tot));
      else if (k === 'insert'){ if (!TC.can('PRO')) SC.upgrade('Chèn tổng chiều dài, diện tích vào bản vẽ', 'PRO'); }
      else if (k === 'xls'){ var r = a.getBoundingClientRect(); var nm = (isL ? 'Tong-chieu-dai_' : 'Tong-dien-tich_') + d.name.replace(/\.\w+$/, ''); TC.menu([{ label:'Excel (.xlsx)', act:function(){ TC.toast('Đã xuất <span class="mono">' + esc(nm) + '.xlsx</span>: trang tổng, trang theo layer, trang theo loại.', { act:{ label:'Mở tệp', fn:function(){} }, kind:'task' }); } }, { label:'CSV (UTF-8)', act:function(){ TC.toast('Đã xuất <span class="mono">' + esc(nm) + '.csv</span>.', { log:false }); } }], r.left, r.top - 4, { up:true }); }
    });
    dlg.el.addEventListener('change', function(e){
      if (e.target.id === 'tt-prec'){ TS.prec = Number(e.target.value); render(); }
      else if (e.target.id === 'tt-hl'){ TS.hl = e.target.checked; render(); }
      else if (e.target.dataset.tf){ TS.off[kind + e.target.dataset.tf] = !e.target.checked; sub = null; render(); }
    });
    render();
  };

  /* ================= So sánh bản vẽ UI-I07 ================= */
  // bản vẽ B: bản sửa đổi của bản vẽ đang mở (mẫu cho prototype; bản thật so sánh hai tệp DWG)
  var REVS = {
    d1:[{ name:'Banve-01_rev-A.dwg', path:'D:\\DuAn\\NhaO-2T\\KienTruc\\Nhan-ve', date:'14/10/2026 10:22', note:'T.Q.Bảo gửi qua Zalo' }, { name:'Banve-01.bak', path:'D:\\DuAn\\NhaO-2T\\KienTruc', date:'16/10/2026 17:40', note:'Bản lưu trước' }],
    d2:[{ name:'Ketcau-T2_KS-sua.dwg', path:'D:\\DuAn\\NhaO-2T\\KetCau\\Nhan-ve', date:'15/10/2026 08:05', note:'Kỹ sư kết cấu sửa' }]
  };
  function buildB(a, rev){
    var ents = a.ents.slice(), ch = [], seq = 900000;
    var b = { id:a.id + 'B', name:rev.name, path:rev.path + '\\' + rev.name, blocks:a.blocks, layers:a.layers, layerMap:a.layerMap, styles:a.styles, styleMap:a.styleMap, layouts:[], ents:[], tempLayers:{} };
    function find(fn){ return a.ents.filter(fn)[0]; }
    function ins(n){ return function(e){ return e.t === 'insert' && e.name === n; }; }
    function txt(s){ return function(e){ return e.t === 'text' && e.s === s; }; }
    function del(e, what){ if (!e) return; ents.splice(ents.indexOf(e), 1); ch.push({ kind:'del', a:e, what:what, prop:'Có ở A, không có ở B' }); }
    function add(o, what){ o.id = ++seq; ents.push(o); ch.push({ kind:'add', b:o, what:what, prop:'Có ở B, không có ở A' }); }
    function mod(e, patch, what, prop){ if (!e) return; var o = Object.assign({}, e, patch); delete o._p; delete o._bb; ents[ents.indexOf(e)] = o; ch.push({ kind:'mod', a:e, b:o, what:what, prop:prop }); }
    if (a.id === 'd1'){
      del(find(ins('BAN-TRA')), 'Block BAN-TRA');
      mod(find(ins('BAN-AN-6')), { p:[4700, 1600] }, 'Block BAN-AN-6', 'Vị trí X: 5000.00 → 4700.00');
      mod(find(function(e){ return e.t === 'insert' && e.name === 'CUA-DI-1C' && Math.abs(e.p[0] - 5000) < 1 && Math.abs(e.p[1] - 6955) < 1; }), { sy:700, p:[5000, 6845] }, 'Block CUA-DI-1C (cửa WC)', 'Hướng mở: vào trong → ra ngoài; Vị trí Y: 6955.00 → 6845.00');
      del(find(function(e){ return e.t === 'circle' && e.r === 60; }), 'Đường tròn (phễu thu sàn WC)');
      mod(find(txt('P. NGỦ 2')), { s:'P. NGỦ CON' }, 'Text', 'Nội dung: “P. NGỦ 2” → “P. NGỦ CON”');
      add({ t:'insert', name:'TU-AO', p:[10990, 6450], sx:1, sy:1, rot:0, layer:'KT-NOITHAT' }, 'Block TU-AO');
      add({ t:'dim', p1:[7855, 7500], p2:[11590, 7500], pos:5600, dir:'h', style:'TCVN-1-100', layer:'KT-DIM' }, 'Kích thước 3735');
      add({ t:'text', p:[13600, 6080], h:220, s:'5. Sàn vệ sinh hạ cốt −0.050 so với sàn phòng.', style:'VNTIME', al:'l', layer:'KT-GHICHU' }, 'Text (ghi chú 5)');
    } else {
      mod(find(txt('D2 220×400')), { s:'D2 220×450' }, 'Text', 'Nội dung: “D2 220×400” → “D2 220×450”');
      del(find(function(e){ return e.t === 'text' && e.s === 'Ø8a200' && e.p[0] === 9750 && e.p[1] > 6000; }), 'Text Ø8a200');
      add({ t:'line', p1:[4350, 6900], p2:[7350, 6900], layer:'KC-THEP' }, 'Line (thép bổ sung)');
      add({ t:'text', p:[5850, 7060], h:180, s:'Ø10a150', style:'Standard', al:'m', layer:'KC-TEXT' }, 'Text Ø10a150');
    }
    b.ents = ents; TC.prepDoc(b); b.initView = a.initView;
    ch.forEach(function(c, i){ c.n = i + 1; var e = c.a || c.b; c.type = TC.TYPE_NAMES[e.t] || e.t; c.layer = e.layer; c.bb = union([c.a && c.a._bb, c.b && c.b._bb]); });
    // xóa → sửa → thêm, theo thứ tự trên bản vẽ cho dễ duyệt
    ch.sort(function(x, y){ return (y.bb[3] - x.bb[3]) || (x.bb[0] - y.bb[0]); }); ch.forEach(function(c, i){ c.n = i + 1; });
    return { doc:b, changes:ch };
  }
  var KIND = { add:['Thêm', '+', 'onlyB'], del:['Xóa', '−', 'onlyA'], mod:['Sửa', '~', 'mod'] };
  SC.compare = function(){
    var d = D(); if (!d) { TC.toast('Mở một bản vẽ để so sánh.', { type:'warn', log:false }); return; }
    var revs = REVS[d.id] || REVS.d1, ri = 0, src = 'file';
    var dlg = TC.dialog({ id:'cmpset', title:'So sánh bản vẽ', cmd:'COMPARE', code:'UI-I07', w:720,
      body:'<div class="stack"><div class="grid2">' +
        '<div class="grp"><span class="gh">Bản vẽ A · hiện hành</span><canvas width="300" height="150" id="cm-ta" class="cm-thumb" aria-label="Xem trước bản vẽ A"></canvas><b>' + esc(d.name) + '</b><span class="faint mono" style="font-size:var(--fs-xs)">' + esc(d.path) + ' · ' + esc(d.modified) + '</span></div>' +
        '<div class="grp"><span class="gh">Bản vẽ B · so với</span><canvas width="300" height="150" id="cm-tb" class="cm-thumb" aria-label="Xem trước bản vẽ B"></canvas>' +
          '<label class="chk"><input type="radio" name="cm-src" value="file" checked> Tệp khác</label><span class="hrow" style="flex-wrap:nowrap"><select class="inp grow" id="cm-file">' + revs.filter(function(r){ return !/\.bak$/.test(r.name); }).map(function(r, i){ return '<option value="' + i + '">' + esc(r.name) + ' · ' + esc(r.note) + '</option>'; }).join('') + '</select><button class="btn" type="button" id="cm-browse">Duyệt…</button></span>' +
          '<label class="chk"><input type="radio" name="cm-src" value="bak"' + (revs.some(function(r){ return /\.bak$/.test(r.name); }) ? '' : ' disabled') + '> Bản lưu trước của tệp này' + (revs.some(function(r){ return /\.bak$/.test(r.name); }) ? ' <span class="faint">(' + esc(revs.filter(function(r){ return /\.bak$/.test(r.name); })[0].date) + ')</span>' : '') + '</label></div></div>' +
        '<div class="grp"><span class="gh">Thiết lập</span><div class="frow" style="--lab-w:150px"><span class="lab">Loại đối tượng</span><span class="hrow" style="gap:4px 14px">' + ['Hình học','Chữ','Kích thước','Hatch','Block'].map(function(x){ return '<label class="chk"><input type="checkbox" checked> ' + x + '</label>'; }).join('') + '</span>' +
          '<label class="lab" for="cm-tol">Dung sai</label><span class="hrow">' + TC.numField({ id:'cm-tol', value:'0.01', label:'Dung sai', pick:false, prec:2, style:'width:120px' }) + '<span class="faint" style="font-size:var(--fs-s)">Lệch nhỏ hơn dung sai coi như giống nhau.</span></span>' +
          '<span class="lab">Chữ</span><label class="chk"><input type="checkbox" checked> So cả định dạng chữ (kiểu chữ, chiều cao, góc)</label></div></div>' +
        '<div class="note acc">' + ic('info') + '<span>So sánh chỉ đọc hai tệp. Không có gì được ghi vào bản vẽ A hay B.</span></div></div>',
      foot:'<span class="faint" style="font-size:var(--fs-s)" id="cm-st"></span><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="cm-go">So sánh</button>' });
    TC.thumb($('#cm-ta', dlg.el), d, 'model');
    function rev(){ return src === 'bak' ? revs.filter(function(r){ return /\.bak$/.test(r.name); })[0] : revs.filter(function(r){ return !/\.bak$/.test(r.name); })[Number($('#cm-file', dlg.el).value)]; }
    function prevB(){ var R = buildB(d, rev()); TC.thumb($('#cm-tb', dlg.el), R.doc, 'model'); return R; }
    prevB();
    dlg.el.addEventListener('change', function(e){ if (e.target.name === 'cm-src'){ src = e.target.value; prevB(); } if (e.target.id === 'cm-file') prevB(); });
    $('#cm-browse', dlg.el).addEventListener('click', function(){ TC.toast('Hộp thoại chọn tệp DWG, DXF (UI-B01). Prototype dùng các tệp mẫu trong danh sách.', { type:'info', log:false }); });
    $('#cm-go', dlg.el).addEventListener('click', function(){
      var go = this; go.disabled = true; $('#cm-st', dlg.el).innerHTML = '<span class="hrow" style="gap:6px"><span class="spin"></span>Đang so sánh ' + TC.n(d.ents.length) + ' đối tượng…</span>';
      setTimeout(function(){ var R = prevB(); R.rev = rev(); dlg.close(true); cmpStart(d, R); }, 650);
    });
  };
  function cmpStart(d, R){
    if (d.cmp) cmpEnd(true);
    var c = d.cmp = { B:R.doc, rev:R.rev, list:R.changes, aSet:{}, bList:[], mode:'over', show:{ a:true, b:true, c:true }, idx:-1, clouds:false, margin:300, shape:'Đám mây', filter:'all' };
    c.list.forEach(function(ch){ if (ch.a) c.aSet[ch.a.id] = 1; if (ch.b) c.bList.push(ch.b); });
    d.sel = []; d.found = []; if (d.space !== 'model') A().setSpace('model');
    A().showPalette('cmp'); A().setCtxTab(cmpTab());
    V().zoomTo(union(c.list.map(function(x){ return x.bb; }))); legend();
    var n = cnt(c);
    TC.cmdline.msg('So sánh xong: <b>' + c.list.length + ' thay đổi</b> (' + n.add + ' thêm, ' + n.del + ' xóa, ' + n.mod + ' sửa). Bấm vào một thay đổi hoặc dùng ◀ ▶ để duyệt.');
  }
  function cnt(c){ var n = { add:0, del:0, mod:0 }; c.list.forEach(function(x){ n[x.kind]++; }); return n; }
  function cmpEnd(quiet){
    var d = D(); if (!d || !d.cmp) return; d.cmp = null;
    A().pal.right = A().pal.right.filter(function(p){ return p !== 'cmp'; }); if (A().pal.rightActive === 'cmp') A().pal.rightActive = 'props';
    A().setCtxTab(null); A().renderDocks(); legend(); V().draw();
    if (!quiet) TC.cmdline.msg('Đã đóng so sánh bản vẽ.');
  }
  function shown(c, ch){ return (c.filter === 'all' || c.filter === ch.kind) && (ch.kind === 'add' ? c.show.b : ch.kind === 'del' ? c.show.a : c.show.a || c.show.b); }
  function goTo(i){
    var d = D(), c = d.cmp; if (!c || !c.list[i]) return; c.idx = i; var ch = c.list[i];
    zoomBB(ch.bb, 5000); A().setCtxTab(cmpTab(), true); A().refreshPalettes();
    TC.cmdline.status('Thay đổi ' + ch.n + '/' + c.list.length + ': <b>' + KIND[ch.kind][0] + '</b> · ' + esc(ch.what) + ' · ' + esc(ch.layer) + (ch.kind === 'mod' ? ' · ' + esc(ch.prop) : ''));
  }
  function step(dir){ var c = D().cmp, vis = c.list.map(function(x, i){ return shown(c, x) ? i : -1; }).filter(function(i){ return i >= 0; }); if (!vis.length) return; var k = vis.indexOf(c.idx); goTo(vis[k < 0 ? (dir > 0 ? 0 : vis.length - 1) : (k + dir + vis.length) % vis.length]); }
  TC.cmpClick = function(p){
    var v = V(), d = D(), c = d.cmp, x = p[0]; if (c.mode === 'side') x = x < v.W / 2 ? x + v.W / 4 : x - v.W / 4;
    var w = v.toW(x, p[1], d.view), tol = 10 / d.view.s, hit = -1;
    c.list.forEach(function(ch, i){ var b = ch.bb, m = c.margin + tol; if (shown(c, ch) && w[0] >= b[0] - m && w[0] <= b[2] + m && w[1] >= b[1] - m && w[1] <= b[3] + m) hit = i; });
    if (hit >= 0) goTo(hit); else TC.ctip($('#cv'), p[0], p[1], 'Không có thay đổi tại đây');
  };
  TC.cmpDraw = function(v, ctx){
    var d = v.doc, c = d.cmp, W = v.W, H = v.H;
    function drawList(list, style){
      list.forEach(function(e){ if (e.t === 'hatch' && TC.entVisible(d, e)) v.drawPrims(ctx, e, d.view, false, style ? { color:style.color, lw:1 } : null); });
      list.forEach(function(e){ if (e.t !== 'hatch' && TC.entVisible(d, e)) v.drawPrims(ctx, e, d.view, false, style); });
    }
    if (c.mode === 'side'){
      [[d.ents, -1], [c.B.ents, 1]].forEach(function(s){ ctx.save(); ctx.beginPath(); ctx.rect(s[1] < 0 ? 0 : W / 2, 0, W / 2, H); ctx.clip(); ctx.translate(s[1] * W / 4, 0); drawList(s[0]); ctx.restore(); });
      ctx.fillStyle = TC.cssv('--c-border-ctl'); ctx.fillRect(Math.round(W / 2), 0, 1, H);
      return;
    }
    if (c.show.c) drawList(d.ents.filter(function(e){ return !c.aSet[e.id]; }), { color:TC.cssv('--c-cmp-common'), lw:1 });
    if (c.show.a) drawList(d.ents.filter(function(e){ return c.aSet[e.id]; }), { color:TC.cssv('--c-cmp-a'), lw:2 });
    if (c.show.b) drawList(c.bList, { color:TC.cssv('--c-cmp-b'), lw:2 });
  };
  // đám mây sửa đổi trên màn hình: đi vòng theo chiều kim đồng hồ, mỗi đoạn một cung lồi ra ngoài
  function cloud(ctx, r, col, w, step){
    var x = r[0], y = r[1], X = r[0] + r[2], Y = r[1] + r[3]; step = step || 12;
    ctx.save(); ctx.strokeStyle = col; ctx.lineWidth = w; ctx.setLineDash([]); ctx.beginPath();
    [[x, y, X, y], [X, y, X, Y], [X, Y, x, Y], [x, Y, x, y]].forEach(function(e){ var len = Math.hypot(e[2] - e[0], e[3] - e[1]), n = Math.max(1, Math.round(len / step)), dx = (e[2] - e[0]) / n, dy = (e[3] - e[1]) / n, ang = Math.atan2(dy, dx), rr = Math.hypot(dx, dy) / 2;
      for (var k = 0; k < n; k++){ var sx = e[0] + dx * k, sy = e[1] + dy * k; ctx.moveTo(sx, sy); ctx.arc(sx + dx / 2, sy + dy / 2, rr, ang + Math.PI, ang, false); } });
    ctx.stroke(); ctx.restore();
  }
  TC.cloudPath = cloud;
  function polyCloud(ctx, pts, col, w, step){ step = step || 12; ctx.save(); ctx.strokeStyle = col; ctx.lineWidth = w; ctx.beginPath(); pts.forEach(function(a, i){ var b = pts[(i + 1) % pts.length], len = Math.hypot(b[0] - a[0], b[1] - a[1]), n = Math.max(1, Math.round(len / step)), dx = (b[0] - a[0]) / n, dy = (b[1] - a[1]) / n, ang = Math.atan2(dy, dx), rr = Math.hypot(dx, dy) / 2; for (var k = 0; k < n; k++){ var sx = a[0] + dx * k, sy = a[1] + dy * k; ctx.moveTo(sx, sy); ctx.arc(sx + dx / 2, sy + dy / 2, rr, ang + Math.PI, ang, false); } }); ctx.stroke(); ctx.restore(); }
  function rectS(v, bb, m, view){ var a = v.toS([bb[0] - m, bb[3] + m], view), b = v.toS([bb[2] + m, bb[1] - m], view); var r = [a[0], a[1], b[0] - a[0], b[1] - a[1]]; if (r[2] < 18){ r[0] -= (18 - r[2]) / 2; r[2] = 18; } if (r[3] < 18){ r[1] -= (18 - r[3]) / 2; r[3] = 18; } return r; }
  function pill(ctx, x, y, kind, n){
    var t = KIND[kind][1] + n, w = 12 + t.length * 6.4, h = 16, A0 = TC.cssv('--c-cmp-a'), B0 = TC.cssv('--c-cmp-b');
    ctx.save(); ctx.font = '600 10px ' + TC.cssv('--f-mono'); ctx.textBaseline = 'middle'; ctx.textAlign = 'center';
    ctx.beginPath(); ctx.rect(x, y - h, w, h); ctx.fillStyle = kind === 'del' ? A0 : B0; ctx.fill();
    if (kind === 'mod'){ ctx.beginPath(); ctx.rect(x, y - h, w / 2, h); ctx.fillStyle = A0; ctx.fill(); }
    ctx.fillStyle = '#101214'; ctx.fillText(t, x + w / 2, y - h / 2 + .5); ctx.restore();
  }
  TC.overlays.push(function(ctx, v){
    var d = v.doc, c = d && d.cmp; if (!c || d.space !== 'model') return;
    var acc = TC.cssv('--c-accent'), col = { add:TC.cssv('--c-cmp-b'), del:TC.cssv('--c-cmp-a'), mod:TC.cssv('--c-warn') };
    (c.mode === 'side' ? [-1, 1] : [0]).forEach(function(hf){
      ctx.save(); if (hf){ ctx.beginPath(); ctx.rect(hf < 0 ? 0 : v.W / 2, 0, v.W / 2, v.H); ctx.clip(); ctx.translate(hf * v.W / 4, 0); }
      c.list.forEach(function(ch, i){
        if (!shown(c, ch)) return; var r = rectS(v, ch.bb, c.margin, d.view), cur = i === c.idx;
        if (c.clouds || cur){ if (c.shape === 'Chữ nhật'){ ctx.strokeStyle = cur ? acc : col[ch.kind]; ctx.lineWidth = cur ? 2 : 1.4; ctx.strokeRect(r[0], r[1], r[2], r[3]); } else cloud(ctx, r, cur ? acc : col[ch.kind], cur ? 2 : 1.4); }
        pill(ctx, r[0], r[1] - 3, ch.kind, ch.n);
      });
      ctx.restore();
    });
  });
  function legend(){
    var cv = $('#cv'), el = $('.cmp-leg', cv), d = D(), c = d && d.cmp;
    $$('.cmp-half', cv).forEach(function(x){ x.remove(); });
    if (!c){ if (el) el.remove(); return; }
    if (!el){ el = TC.el('<div class="cmp-leg" role="note"></div>'); cv.appendChild(el); }
    var top = ($('#banners').offsetHeight + 8) + 'px';
    el.style.top = top;
    el.hidden = c.mode === 'side';
    el.innerHTML = '<span><i style="background:var(--c-cmp-a)"></i>Chỉ có ở A <b>−</b></span><span><i style="background:var(--c-cmp-b)"></i>Chỉ có ở B <b>+</b></span><span><i style="background:var(--c-cmp-common)"></i>Giống nhau</span><span class="faint">A: ' + esc(d.name) + ' · B: ' + esc(c.B.name) + '</span>';
    if (c.mode === 'side') [['A · ' + d.name + ' (hiện hành)', 0], ['B · ' + c.B.name, 1]].forEach(function(h){ var x = TC.el('<div class="cmp-half">' + esc(h[0]) + '</div>'); x.style.top = top; x.style.left = h[1] ? '75%' : '25%'; cv.appendChild(x); });
  }
  exitOnDoc(function(d){ legend(); if (d && d.cmp) A().setCtxTab(cmpTab(), true); else if (A().ctxTab && A().ctxTab.id === 'cmp') A().setCtxTab(null); });
  function cmpTab(){
    var d = D(), c = d.cmp, n = cnt(c), set = function(fn){ return function(){ fn(); A().setCtxTab(cmpTab(), true); legend(); V().draw(); A().refreshPalettes(); }; };
    var cur = c.idx >= 0 ? c.list[c.idx].n + ' / ' + c.list.length : '– / ' + c.list.length;
    return { id:'cmp', name:'So sánh bản vẽ', ctx:true, panels:[
      { name:'Bản vẽ', icon:'compare', prio:3, items:[ { t:'lg', label:'Đổi bản\nvẽ B', icon:'open', id:'cmp-b', desc:'Chọn tệp khác hoặc bản lưu trước để so với bản vẽ hiện hành.', act:function(){ SC.compare(); } },
        { t:'col', items:[ { t:'s', label:'A: ' + d.name, icon:'file', id:'cmp-a-info', act:function(){ TC.toast('A: <b>' + esc(d.name) + '</b> · ' + esc(d.path), { type:'info', log:false }); } }, { t:'s', label:'B: ' + c.B.name, icon:'file', id:'cmp-b-info', act:function(){ TC.toast('B: <b>' + esc(c.B.name) + '</b> · ' + esc(c.rev.note) + ' · ' + esc(c.rev.date), { type:'info', log:false }); } }, { t:'s', label:'Dung sai 0.01', icon:'gear', id:'cmp-tol', act:function(){ SC.compare(); } } ] } ] },
      { name:'Chế độ xem', icon:'overlay', prio:1, items:[ { t:'lg', label:'Chồng\nhình', icon:'overlay', id:'cmp-ov', toggle:true, on:function(){ return c.mode === 'over'; }, desc:'Một khung: màu theo nhóm thay đổi, kèm ký hiệu − + ~.', act:set(function(){ c.mode = 'over'; }) }, { t:'lg', label:'Cạnh\nnhau', icon:'split', id:'cmp-sd', toggle:true, on:function(){ return c.mode === 'side'; }, desc:'Hai khung A và B đồng bộ zoom, pan.', act:set(function(){ c.mode = 'side'; }) } ] },
      { name:'Hiện nhóm', icon:'eye', prio:2, items:[ { t:'col', items:[ { t:'s', label:'Chỉ có ở A (−)', icon:'eye', sw:TC.cssv('--c-cmp-a'), id:'cmp-sa', toggle:true, on:function(){ return c.show.a; }, act:set(function(){ c.show.a = !c.show.a; }) }, { t:'s', label:'Chỉ có ở B (+)', icon:'eye', sw:TC.cssv('--c-cmp-b'), id:'cmp-sb', toggle:true, on:function(){ return c.show.b; }, act:set(function(){ c.show.b = !c.show.b; }) }, { t:'s', label:'Giống nhau', icon:'eye', sw:TC.cssv('--c-cmp-common'), id:'cmp-sc', toggle:true, on:function(){ return c.show.c; }, act:set(function(){ c.show.c = !c.show.c; }) } ] } ] },
      { name:'Thay đổi', icon:'list', prio:1, items:[ { t:'lg', label:'Trước', icon:'left', id:'cmp-prev', act:function(){ step(-1); } }, { t:'lg', label:'Sau', icon:'right', id:'cmp-next', act:function(){ step(1); } },
        { t:'col', items:[ { t:'s', label:cur + ' thay đổi', icon:'list', id:'cmp-pos', act:function(){ A().showPalette('cmp'); } }, { t:'s', label:n.add + ' thêm · ' + n.del + ' xóa · ' + n.mod + ' sửa', icon:'filter', id:'cmp-sum', act:function(){ A().showPalette('cmp'); } }, { t:'s', label:'Danh sách thay đổi', icon:'list', id:'cmp-list', act:function(){ A().showPalette('cmp'); } } ] } ] },
      { name:'Đánh dấu', icon:'cloud', prio:2, items:[ { t:'lg', label:'Tạo\nđám mây', icon:'cloud', id:'cmp-cloud', desc:TC.can('PRO') ? 'Vẽ đám mây quanh mọi thay đổi vào bản vẽ.' : 'Đám mây quanh mọi thay đổi được lưu thành markup (tệp .tcmk), không sửa DWG.', act:function(){ makeClouds(); } },
        { t:'col', gap:3, items:[ { t:'dd', label:'Hình đánh dấu', id:'cmp-shape', w:126, text:function(){ return c.shape; }, act:function(b){ var r = b.getBoundingClientRect(); TC.menu(['Đám mây','Chữ nhật'].map(function(s){ return { label:s, check:c.shape === s, act:set(function(){ c.shape = s; }) }; }), r.left, r.bottom + 2, { minWidth:r.width }); } }, { t:'dd', label:'Lề', id:'cmp-margin', w:126, text:function(){ return 'Lề ' + c.margin + ' mm'; }, act:function(b){ var r = b.getBoundingClientRect(); TC.menu([0, 150, 300, 500, 1000].map(function(m){ return { label:m + ' mm', check:c.margin === m, act:set(function(){ c.margin = m; }) }; }), r.left, r.bottom + 2, { minWidth:r.width }); } } ] } ] },
      { name:'Xuất', icon:'pdf', prio:3, items:[ { t:'col', items:[ { t:'s', label:'Ảnh chụp', icon:'snapshot', id:'cmp-shot', act:function(){ TC.toast('Đã chụp vùng đang xem vào clipboard.', { act:{ label:'Lưu PNG', fn:function(){} }, log:false }); } }, { t:'s', label:'Báo cáo PDF', icon:'pdf', id:'cmp-pdf', act:function(){ TC.toast('Đã xuất <span class="mono">So-sanh_' + esc(d.name.replace(/\.\w+$/, '')) + '.pdf</span>: trang tổng hợp và ' + c.list.length + ' trang chi tiết, mỗi thay đổi một ảnh.', { act:{ label:'Mở tệp', fn:function(){} }, kind:'task' }); } }, { t:'s', label:'Báo cáo CSV', icon:'stats', id:'cmp-csv', act:function(){ TC.toast('Đã xuất <span class="mono">So-sanh_' + esc(d.name.replace(/\.\w+$/, '')) + '.csv</span>: ' + c.list.length + ' dòng.', { log:false }); } } ] } ] },
      { name:'Đóng', icon:'x', prio:1, items:[ { t:'lg', label:'Đóng so\nsánh', icon:'x', id:'cmp-close', act:function(){ cmpEnd(); } } ] }
    ] };
  }
  function makeClouds(){
    var d = D(), c = d.cmp; c.clouds = true; V().drawOver();
    if (TC.can('PRO')) { TC.toast('Đã vẽ ' + c.list.length + ' đám mây vào bản vẽ trên layer <b>TC-REVCLOUD</b>. Ctrl+Z để hoàn tác.', { log:false }); return; }
    var list = mks(d); c.list.forEach(function(ch){ var m = c.margin; list.push(mkNew({ type:'cloud', sp:'model', pts:[[ch.bb[0] - m, ch.bb[1] - m], [ch.bb[2] + m, ch.bb[3] + m]], color:ch.kind === 'add' ? '#4DA3FF' : ch.kind === 'del' ? '#FF9F1C' : '#FFD60A', text:KIND[ch.kind][0] + ' · ' + ch.what + (ch.kind === 'mod' ? ': ' + ch.prop : ''), src:'compare' })); });
    TC.emit('markups');
    TC.toast('Đã tạo ' + c.list.length + ' đám mây thành markup trong <b>' + esc(d.mkFile) + '</b>. Tệp DWG không bị sửa.', { act:{ label:'Mở danh sách', fn:function(){ A().showPalette('markup'); } } });
  }
  P.cmp = { name:'So sánh', hidden:function(){ var d = D(); return !d || !d.cmp; }, render:function(host, d){
    if (!d || !d.cmp){ host.innerHTML = '<div class="empty">' + ic('compare') + '<b>Chưa so sánh</b>Gõ COMPARE hoặc mở Công cụ › So sánh.</div>'; return; }
    var c = d.cmp, n = cnt(c), list = c.list.filter(function(x){ return shown(c, x); });
    host.innerHTML = '<div class="pal-tools"><span class="seg" data-seg="cmpf">' + [['all','Tất cả', c.list.length], ['add','Thêm', n.add], ['del','Xóa', n.del], ['mod','Sửa', n.mod]].map(function(f){ return '<button type="button" data-v="' + f[0] + '" aria-pressed="' + (c.filter === f[0]) + '">' + f[1] + ' <span class="faint">' + f[2] + '</span></button>'; }).join('') + '</span></div>' +
      '<div class="pal-tools"><button class="ib" type="button" data-cp="prev" aria-label="Thay đổi trước">' + ic('left') + '</button><span class="mono grow" style="text-align:center;font-size:var(--fs-s)">' + (c.idx >= 0 ? c.list[c.idx].n : '–') + ' / ' + c.list.length + '</span><button class="ib" type="button" data-cp="next" aria-label="Thay đổi kế">' + ic('right') + '</button></div>' +
      '<div class="pal-body"><table class="tbl cmp-tbl"><colgroup><col style="width:30px"><col style="width:54px"><col><col style="width:82px"></colgroup><thead><tr><th class="num">#</th><th>Thay đổi</th><th>Đối tượng</th><th>Layer</th></tr></thead><tbody>' +
        list.map(function(ch){ var i = c.list.indexOf(ch); return '<tr data-ci="' + i + '" class="' + (i === c.idx ? 'is-sel' : '') + '"><td class="num">' + ch.n + '</td><td><span class="ktag ' + ch.kind + '">' + KIND[ch.kind][1] + ' ' + KIND[ch.kind][0] + '</span></td><td title="' + esc(ch.what) + '">' + esc(ch.type) + '</td><td title="' + esc(ch.layer) + '">' + esc(ch.layer) + '</td></tr>' + (i === c.idx ? '<tr class="is-sel cmp-det"><td></td><td colspan="3"><b>' + esc(ch.what) + '</b><br>' + esc(ch.prop) + '</td></tr>' : ''); }).join('') +
        (list.length ? '' : '<tr><td colspan="4" class="faint">Không có thay đổi trong nhóm đang hiện.</td></tr>') + '</tbody></table></div>' +
      '<div class="pal-foot"><span>A: ' + esc(d.name) + '</span><span>B: ' + esc(c.B.name) + '</span></div>';
    host.onclick = function(e){
      var f = e.target.closest('[data-seg="cmpf"] button'); if (f){ c.filter = f.dataset.v; P.cmp.render(host, d); V().drawOver(); return; }
      var r = e.target.closest('[data-ci]'); if (r){ goTo(Number(r.dataset.ci)); return; }
      var s = e.target.closest('[data-cp]'); if (s) step(s.dataset.cp === 'next' ? 1 : -1);
    };
  } };
  IMPL.COMPARE = function(){ SC.compare(); };

  /* ================= Markup UI-M02 ================= */
  var ME = 'Nguyễn Minh An';
  var MCOL = [['Đỏ','#FF4D4F'],['Cam','#FF9F1C'],['Vàng','#FFD60A'],['Xanh lá','#3DDC84'],['Xanh dương','#4DA3FF'],['Tím','#C77DFF'],['Trắng','#FFFFFF'],['Đen','#15181C']];
  var MT = { cloud:['Đám mây','cloud'], rect:['Chữ nhật','rect'], ellipse:['Ellipse','ellipse'], arrow:['Mũi tên','arrow'], line:['Đường thẳng','line'], free:['Nét tự do','freehand'], hl:['Bút dạ','highlight'], text:['Chữ ghi chú','textnote'], leader:['Ghi chú có đường dẫn','leader'], tick:['Dấu tick','tick'], dist:['Khoảng cách','dist'], area:['Diện tích','area'], stamp:['Con dấu','stamp'], snap:['Ảnh chụp vùng','snapshot'] };
  var STAMPS = { ok:['ĐÃ DUYỆT','#2FB36D'], fix:['CẦN SỬA','#F08C2E'], no:['KHÔNG DUYỆT','#E5484D'] };
  var MK = TC.markup = { props:TC.load('mkprops', { color:'#FF4D4F', w:2, alpha:100, size:14 }), stamp:'fix', cur:null, who:'all', st:'all', seen:false, MCOL:MCOL, STAMPS:STAMPS, MT:MT };
  function stamp(t){ var x = new Date(); return TC.date(x) + ' ' + ('0' + x.getHours()).slice(-2) + ':' + ('0' + x.getMinutes()).slice(-2); }
  function initials(n){ var p = n.split(' '); return p.slice(0, -1).map(function(x){ return x[0]; }).join('.').toUpperCase() + '.' + p[p.length - 1].toUpperCase(); }
  function mkNew(o){ var d = D(); return Object.assign({ id:++d.mkSeq, sp:d.space, color:MK.props.color, w:MK.props.w, alpha:MK.props.alpha, author:ME, time:stamp(), status:'open', comments:[], hidden:false }, o); }
  function mks(d){
    if (d.markups) return d.markups;
    d.markups = []; d.mkSeq = 0; d.mkFile = d.name.replace(/\.\w+$/, '.tcmk');
    if (d.id !== 'd1') return d.markups;
    var add = function(o){ d.markups.push(Object.assign({ id:++d.mkSeq, sp:'model', w:2, alpha:100, status:'open', comments:[], hidden:false }, o)); };
    add({ type:'cloud', pts:[[5600, 6780], [7900, 9060]], color:'#FF4D4F', author:'Lê Thu Chi', time:'14/10/2026 09:12', text:'Kho thiếu cửa thông gió.', comments:[{ who:'Trần Quốc Bảo', t:'14/10/2026 10:40', s:'Đề xuất thêm cửa chớp 400×400 trên vách giáp sảnh.' }] });
    add({ type:'leader', pts:[[9900, 400], [12500, 2500]], h:300, color:'#FF9F1C', author:'Trần Quốc Bảo', time:'14/10/2026 10:35', text:'Kiểm tra lại vị trí chậu rửa\nso với ống cấp nước.', status:'done', comments:[{ who:ME, t:'15/10/2026 08:20', s:'Đã dời chậu sang trái 300, xem bản rev-A.' }] });
    add({ type:'stamp', pts:[[10700, -4520]], h:620, stamp:'fix', author:'Lê Thu Chi', time:'14/10/2026 09:30', text:'Sửa theo các ghi chú rồi gửi lại.' });
    return d.markups;
  }
  TC.markupsOf = mks;
  function vis(d, m){ return !m.hidden && m.sp === d.space && (MK.who === 'all' || m.author === MK.who) && (MK.st === 'all' || m.status === MK.st); }
  function lumDark(hex){ var n = parseInt(hex.slice(1), 16); return (0.299 * (n >> 16) + 0.587 * ((n >> 8) & 255) + 0.114 * (n & 255)) > 150; }
  function textBox(ctx, x, y, s, px, col, paper, al){
    var lines = String(s).split('\n'), pad = Math.max(3, px * .35), lh = px * 1.25;
    ctx.font = '500 ' + px + 'px ' + TC.cssv('--f-ui'); var w = Math.max.apply(null, lines.map(function(l){ return ctx.measureText(l).width; })) + pad * 2, h = lines.length * lh + pad * 2;
    var bx = al === 'r' ? x - w : x, by = y - h / 2;
    ctx.save(); ctx.globalAlpha = Math.min(1, ctx.globalAlpha + .1); ctx.fillStyle = paper ? 'rgba(255,255,255,.9)' : 'rgba(15,17,19,.82)'; ctx.fillRect(bx, by, w, h); ctx.lineWidth = 1; ctx.strokeStyle = col; ctx.strokeRect(bx + .5, by + .5, w - 1, h - 1);
    ctx.fillStyle = col; ctx.textBaseline = 'middle'; lines.forEach(function(l, i){ ctx.fillText(l, bx + pad, by + pad + lh * (i + .5)); }); ctx.restore();
    return [bx, by, w, h];
  }
  function arrowHead(ctx, a, b, size){ var an = Math.atan2(b[1] - a[1], b[0] - a[0]); ctx.beginPath(); ctx.moveTo(b[0], b[1]); ctx.lineTo(b[0] - size * Math.cos(an - .4), b[1] - size * Math.sin(an - .4)); ctx.lineTo(b[0] - size * Math.cos(an + .4), b[1] - size * Math.sin(an + .4)); ctx.closePath(); ctx.fill(); }
  function drawMk(ctx, v, m, hot){
    var d = v.doc, paper = d.space !== 'model', s = paper ? v.vw().s : d.view.s;
    var p = m.pts.map(function(q){ return paper ? v.toS(q) : v.toS(q, d.view); }), col = m.type === 'stamp' ? STAMPS[m.stamp][1] : m.color, box = null;
    ctx.save(); ctx.globalAlpha = (m.alpha || 100) / 100; ctx.strokeStyle = ctx.fillStyle = col; ctx.lineWidth = m.w || 2; ctx.lineCap = ctx.lineJoin = 'round'; ctx.setLineDash([]);
    var line = function(pts){ ctx.beginPath(); pts.forEach(function(q, i){ if (i) ctx.lineTo(q[0], q[1]); else ctx.moveTo(q[0], q[1]); }); ctx.stroke(); };
    var rect = function(){ var x0 = Math.min(p[0][0], p[1][0]), y0 = Math.min(p[0][1], p[1][1]); return [x0, y0, Math.abs(p[1][0] - p[0][0]), Math.abs(p[1][1] - p[0][1])]; };
    if (hot){ ctx.save(); ctx.globalAlpha = .45; ctx.strokeStyle = TC.cssv('--c-accent'); ctx.lineWidth = (m.w || 2) + 6; var bb0 = m.pts.length > 1 ? rect() : [p[0][0] - 20, p[0][1] - 20, 40, 40]; ctx.strokeRect(bb0[0] - 6, bb0[1] - 6, bb0[2] + 12, bb0[3] + 12); ctx.restore(); }
    switch (m.type){
      case 'cloud': if (p.length > 2) polyCloud(ctx, p, col, m.w || 2, 14); else cloud(ctx, rect(), col, m.w || 2, 14); box = p.length > 2 ? null : rect(); break;
      case 'rect': box = rect(); ctx.strokeRect(box[0], box[1], box[2], box[3]); break;
      case 'ellipse': box = rect(); ctx.beginPath(); ctx.ellipse(box[0] + box[2] / 2, box[1] + box[3] / 2, box[2] / 2, box[3] / 2, 0, 0, 7); ctx.stroke(); break;
      case 'line': line(p); break;
      case 'arrow': line(p); arrowHead(ctx, p[0], p[1], 9 + (m.w || 2) * 2); break;
      case 'free': line(p); break;
      case 'hl': ctx.globalAlpha = .38; ctx.lineWidth = 14; line(p); break;
      case 'text': box = textBox(ctx, p[0][0], p[0][1], m.text, Math.max(8, m.h * s), col, paper); break;
      case 'leader': line([p[1], p[0]]); arrowHead(ctx, p[1], p[0], 9 + (m.w || 2) * 2); box = textBox(ctx, p[1][0], p[1][1], m.text, Math.max(8, m.h * s), col, paper, p[1][0] < p[0][0] ? 'r' : 'l'); break;
      case 'tick': { var k = Math.max(7, m.h * s); ctx.lineWidth = Math.max(2.5, k / 6); line([[p[0][0] - k * .55, p[0][1]], [p[0][0] - k * .15, p[0][1] + k * .45], [p[0][0] + k * .6, p[0][1] - k * .5]]); break; }
      case 'dist': { line(p); [p[0], p[1]].forEach(function(q){ ctx.beginPath(); ctx.arc(q[0], q[1], 3, 0, 7); ctx.fill(); }); textBox(ctx, (p[0][0] + p[1][0]) / 2 + 6, (p[0][1] + p[1][1]) / 2 - 12, TC.d(G.dist(m.pts[0], m.pts[1])), 11, col, paper); break; }
      case 'area': { ctx.beginPath(); p.forEach(function(q, i){ if (i) ctx.lineTo(q[0], q[1]); else ctx.moveTo(q[0], q[1]); }); ctx.closePath(); ctx.save(); ctx.globalAlpha *= .14; ctx.fill(); ctx.restore(); ctx.stroke(); var ce = p.reduce(function(a, q){ return [a[0] + q[0] / p.length, a[1] + q[1] / p.length]; }, [0, 0]); textBox(ctx, ce[0] - 30, ce[1], TC.d(G.polyArea(m.pts) / 1e6) + ' m²', 11, col, paper); break; }
      case 'stamp': { var hh = Math.max(14, m.h * s), lab = STAMPS[m.stamp][0], sub = initials(m.author) + ' · ' + m.time.split(' ')[0]; ctx.translate(p[0][0], p[0][1]); ctx.rotate(-8 * Math.PI / 180); ctx.font = '700 ' + (hh * .42) + 'px ' + TC.cssv('--f-ui'); var w2 = Math.max(ctx.measureText(lab).width, hh * 2.2) + hh * .6; ctx.lineWidth = Math.max(1.5, hh / 16); ctx.strokeRect(-w2 / 2, -hh / 2, w2, hh); ctx.strokeRect(-w2 / 2 + 3, -hh / 2 + 3, w2 - 6, hh - 6); ctx.textAlign = 'center'; ctx.textBaseline = 'middle'; ctx.fillText(lab, 0, -hh * .08); ctx.font = '500 ' + (hh * .2) + 'px ' + TC.cssv('--f-mono'); ctx.fillText(sub, 0, hh * .28); ctx.setTransform(v.dpr, 0, 0, v.dpr, 0, 0); box = [p[0][0] - w2 / 2, p[0][1] - hh / 2, w2, hh]; break; }
      case 'snap': { box = rect(); ctx.setLineDash([6, 4]); ctx.lineWidth = 1.5; ctx.strokeRect(box[0], box[1], box[2], box[3]); ctx.setLineDash([]); break; }
    }
    // số thứ tự, cùng số trong danh sách markup
    var bx = box ? [box[0], box[1]] : p.reduce(function(a, q){ return [Math.min(a[0], q[0]), Math.min(a[1], q[1])]; }, [Infinity, Infinity]);
    m._badge = [bx[0] - 4, bx[1] - 4];
    ctx.globalAlpha = 1; ctx.beginPath(); ctx.arc(m._badge[0], m._badge[1], 8, 0, 7); ctx.fillStyle = m.status === 'done' ? '#8A919B' : col; ctx.fill();
    ctx.fillStyle = m.status === 'done' || lumDark(col) ? '#101214' : '#FFFFFF'; ctx.font = '600 9.5px ' + TC.cssv('--f-mono'); ctx.textAlign = 'center'; ctx.textBaseline = 'middle'; ctx.fillText(m.status === 'done' ? '✓' : String(m.n), m._badge[0], m._badge[1] + .5);
    ctx.restore();
  }
  TC.overlays.push(function(ctx, v){
    var d = v.doc; if (!d || !TC.can('VP') || d.cmp) return; // đang so sánh: ẩn markup để màu không lẫn với màu nhóm thay đổi
    var list = mks(d); list.forEach(function(m, i){ m.n = i + 1; m._badge = null; if (vis(d, m)) drawMk(ctx, v, m, MK.cur === m.id); });
  });
  TC.markupHit = function(p){
    var d = D(); if (!d || !TC.can('VP') || !d.markups) return false;
    var hit = d.markups.filter(function(m){ return m._badge && Math.hypot(p[0] - m._badge[0], p[1] - m._badge[1]) <= 10; })[0]; if (!hit) return false;
    MK.cur = hit.id; A().showPalette('markup'); V().drawOver(); return true;
  };
  TC.on('markups', function(){ A().refreshPalettes(); V().drawOver(); });
  TC.on('edition', function(){ if (A().active !== 'start' && V()) V().drawOver(); }); // Viewer không có markup
  function addMk(m){ var d = D(); mks(d).push(m); MK.cur = m.id; TC.emit('markups'); TC.cmdline.msg('Đã thêm markup <b>' + esc(MT[m.type][0]) + '</b> #' + d.markups.length + ' vào ' + esc(d.mkFile) + ' (không sửa tệp DWG).'); }
  function editText(wp, ph){
    return new Promise(function(res){
      var v = V(), s = v.mToS(wp) || v.toS(wp);
      var ta = TC.el('<textarea class="mk-edit" rows="2" aria-label="Nội dung ghi chú"></textarea>'); ta.placeholder = ph || 'Nhập ghi chú · Enter để xong · Shift+Enter xuống dòng';
      ta.style.left = s[0] + 'px'; ta.style.top = (s[1] - 14) + 'px'; ta.style.borderColor = MK.props.color; $('#cv').appendChild(ta); setTimeout(function(){ ta.focus(); }, 0);
      var done = false; function fin(ok){ if (done) return; done = true; var t = ta.value.trim(); ta.remove(); res(ok && t ? t : null); $('#cv').focus({ preventScroll:true }); }
      ta.addEventListener('keydown', function(e){ e.stopPropagation(); if (e.key === 'Enter' && !e.shiftKey){ e.preventDefault(); fin(true); } else if (e.key === 'Escape'){ e.preventDefault(); fin(false); } });
      ta.addEventListener('blur', function(){ fin(true); });
    });
  }
  function shot(p1, p2){
    var v = V(), a = v.mToS(p1) || v.toS(p1), b = v.mToS(p2) || v.toS(p2), x = Math.min(a[0], b[0]), y = Math.min(a[1], b[1]), w = Math.max(8, Math.abs(b[0] - a[0])), h = Math.max(8, Math.abs(b[1] - a[1]));
    var keep = v.mouse; v.mouse = null; v.drawOver();
    var c = document.createElement('canvas'), k = v.dpr; c.width = Math.round(w * k); c.height = Math.round(h * k); var g = c.getContext('2d');
    try { g.drawImage(v.base, x * k, y * k, w * k, h * k, 0, 0, c.width, c.height); g.drawImage(v.over, x * k, y * k, w * k, h * k, 0, 0, c.width, c.height); } catch (e) {}
    v.mouse = keep; v.drawOver();
    var t = document.createElement('canvas'), sc = Math.min(1, 220 / c.width); t.width = Math.max(1, Math.round(c.width * sc)); t.height = Math.max(1, Math.round(c.height * sc)); t.getContext('2d').drawImage(c, 0, 0, t.width, t.height);
    return { img:t.toDataURL('image/png'), px:Math.round(w * k) + ' × ' + Math.round(h * k) + ' px' };
  }
  function ellPts(a, b){ var cx = (a[0] + b[0]) / 2, cy = (a[1] + b[1]) / 2, rx = Math.abs(b[0] - a[0]) / 2, ry = Math.abs(b[1] - a[1]) / 2, out = []; for (var i = 0; i <= 48; i++){ var t = i / 48 * Math.PI * 2; out.push([cx + rx * Math.cos(t), cy + ry * Math.sin(t)]); } return out; }
  function rectPts(a, b){ return [a, [b[0], a[1]], b, [a[0], b[1]], a]; }
  IMPL.TCMARKUP = function(ctx, pre){
    var d = D(), k = String(pre || '').toLowerCase(), pr = MK.props, tmp = { color:pr.color, w:pr.w, alpha:pr.alpha / 100 };
    mks(d);
    if (k === 'list'){ A().showPalette('markup'); return; }
    if (k.indexOf('stamp:') === 0){ MK.stamp = k.split(':')[1]; k = 'stamp'; }
    if (!MT[k]) return ctx.kw('Chọn công cụ markup', [['Đám mây','C'],['Chữ nhật','R'],['Mũi tên','A'],['Chữ','T'],['Nét tự do','F'],['Con dấu','S'],['Danh sách','L']], 'C').then(function(x){ var map = { C:'cloud', R:'rect', A:'arrow', T:'text', F:'free', S:'stamp', L:'list' }; return IMPL.TCMARKUP(ctx, map[String(x).toUpperCase()] || 'cloud'); });
    if (!MK.seen){ MK.seen = true; A().showPalette('markup'); }
    ctx.badge('MK');
    var px = function(n){ return n / (d.space === 'model' ? d.view.s : V().vw().s); }; // cỡ trên màn hình → đơn vị bản vẽ
    var loop = function(make){ return make().then(function(ok){ if (ok) return loop(make); }); };
    var two = function(a1, a2, prev){ return ctx.point(a1).then(function(r){ if (!r || !r.p) return null; var p1 = r.p; return ctx.point(a2, { base:p1, rubber:false, onMove:function(p, temp){ prev(p1, p, temp); } }).then(function(r2){ return r2 && r2.p ? [p1, r2.p] : null; }); }); };
    var tp = function(o){ return Object.assign({ color:tmp.color, w:tmp.w, alpha:tmp.alpha, cap:'round' }, o); };
    var name = MT[k][0];
    ctx.out('Markup <b>' + esc(name) + '</b>: ' + (k === 'free' || k === 'hl' ? 'kéo chuột để vẽ.' : 'chỉ điểm trên bản vẽ.') + ' Enter hoặc Esc để kết thúc. Lưu vào ' + esc(d.mkFile) + ', không sửa DWG.');
    if (k === 'cloud' || k === 'rect' || k === 'ellipse' || k === 'snap') return loop(function(){
      return two('Chỉ định góc thứ nhất', 'Chỉ định góc đối diện', function(a, b, temp){ temp.push(tp({ k:'poly', pts:k === 'ellipse' ? ellPts(a, b) : rectPts(a, b), dash:k === 'cloud' || k === 'snap' ? [6, 4] : null, w:k === 'snap' ? 1.5 : tmp.w })); }).then(function(pp){
        if (!pp) return false;
        if (k === 'snap'){ var sh = shot(pp[0], pp[1]); addMk(mkNew({ type:'snap', pts:pp, img:sh.img, text:'Ảnh chụp ' + sh.px })); TC.toast('Đã chụp vùng ' + sh.px + ' vào danh sách markup và clipboard.', { act:{ label:'Lưu PNG', fn:function(){} }, log:false }); return true; }
        var m = mkNew({ type:k, pts:pp }); return editText(pp[1], 'Ghi chú cho ' + name.toLowerCase() + ' (không bắt buộc) · Enter để xong').then(function(t){ if (t) m.text = t; addMk(m); return true; });
      }); });
    if (k === 'line' || k === 'arrow' || k === 'dist') return loop(function(){
      return two(k === 'dist' ? 'Chỉ định điểm đầu cần đo' : 'Chỉ định điểm đầu', k === 'arrow' ? 'Chỉ định đầu mũi tên' : 'Chỉ định điểm cuối', function(a, b, temp){ temp.push(tp({ k:'line', a:a, b:b })); var s = V().mToS([(a[0] + b[0]) / 2, (a[1] + b[1]) / 2]); if (k === 'dist' && s){ TC.cad.clearLabels(); TC.cad.label(TC.d(G.dist(a, b)), s[0], s[1] - 14); } }).then(function(pp){ TC.cad.clearLabels(); if (!pp) return false; addMk(mkNew({ type:k, pts:pp, text:k === 'dist' ? 'Khoảng cách ' + TC.d(G.dist(pp[0], pp[1])) + ' mm' : '' })); return true; }); });
    if (k === 'free' || k === 'hl') return loop(function(){
      return Promise.race([TC.cad.stroke(k === 'hl' ? { color:pr.color === '#FF4D4F' ? '#FFD60A' : pr.color, w:14, alpha:.38 } : tmp), ctx.val('Kéo chuột để vẽ hoặc Enter để kết thúc', null).then(function(){ return 'end'; })]).then(function(pts){
        if (!pts || pts === 'end'){ TC.cad.endMode(); return false; }
        addMk(mkNew({ type:k, pts:pts, color:k === 'hl' && pr.color === '#FF4D4F' ? '#FFD60A' : pr.color })); return true;
      }); });
    if (k === 'text') return loop(function(){ return ctx.point('Chỉ định vị trí chữ ghi chú').then(function(r){ if (!r || !r.p) return false; return editText(r.p).then(function(t){ if (t) addMk(mkNew({ type:'text', pts:[r.p], text:t, h:px(pr.size) })); return true; }); }); });
    if (k === 'leader') return loop(function(){ return two('Chỉ định điểm cần ghi chú (đầu mũi tên)', 'Chỉ định vị trí chữ', function(a, b, temp){ temp.push(tp({ k:'line', a:a, b:b })); }).then(function(pp){ if (!pp) return false; return editText(pp[1]).then(function(t){ if (t) addMk(mkNew({ type:'leader', pts:pp, text:t, h:px(pr.size) })); return true; }); }); });
    if (k === 'tick') return loop(function(){ return ctx.point('Chỉ định vị trí dấu tick').then(function(r){ if (!r || !r.p) return false; addMk(mkNew({ type:'tick', pts:[r.p], h:px(22) })); return true; }); });
    if (k === 'stamp') return loop(function(){ return ctx.point('Chỉ định vị trí con dấu “' + STAMPS[MK.stamp][0] + '”').then(function(r){ if (!r || !r.p) return false; addMk(mkNew({ type:'stamp', pts:[r.p], stamp:MK.stamp, h:px(46) })); return true; }); });
    if (k === 'area'){ var pts = []; var nxt = function(){ return ctx.point(pts.length ? 'Chỉ định điểm kế tiếp hoặc Enter để xong' : 'Chỉ định góc đầu tiên của vùng', { base:pts[pts.length - 1], rubber:false, onMove:function(p, temp){ if (pts.length) temp.push(tp({ k:'poly', pts:pts.concat([p]), closed:true, fill:'rgba(255,77,79,.10)' })); } }).then(function(r){ if (r && r.p){ pts.push(r.p); return nxt(); } if (pts.length > 2){ addMk(mkNew({ type:'area', pts:pts, text:'Diện tích ' + TC.d(G.polyArea(pts) / 1e6) + ' m²' })); } }); }; return nxt(); }
  };
  function mkTitle(m){ return m.text ? m.text.split('\n')[0] : m.type === 'stamp' ? 'Con dấu “' + STAMPS[m.stamp][0] + '”' : MT[m.type][0]; }
  P.markup = { name:'Markup', hidden:function(){ return !TC.can('VP'); }, render:function(host, d){
    if (!d){ host.innerHTML = ''; return; }
    var list = mks(d), authors = list.map(function(m){ return m.author; }).filter(function(x, i, a){ return a.indexOf(x) === i; });
    var rows = list.map(function(m, i){ m.n = i + 1; return m; }).filter(function(m){ return (MK.who === 'all' || m.author === MK.who) && (MK.st === 'all' || m.status === MK.st); });
    var open = list.filter(function(m){ return m.status === 'open'; }).length;
    host.innerHTML = '<div class="pal-tools"><button class="dd grow" type="button" data-mf="who"><span class="lbl">' + (MK.who === 'all' ? 'Mọi tác giả' : esc(MK.who)) + '</span>' + ic('down', 'xs') + '</button><button class="dd" type="button" data-mf="st" style="width:96px"><span class="lbl">' + (MK.st === 'all' ? 'Mọi trạng thái' : MK.st === 'open' ? 'Đang mở' : 'Đã xử lý') + '</span>' + ic('down', 'xs') + '</button><button class="ib" type="button" data-ma="more" aria-label="Thao tác khác">' + ic('more') + '</button></div>' +
      '<div class="pal-note info">' + ic('info') + '<span>Lưu trong <b>' + esc(d.mkFile) + '</b> cạnh bản vẽ. Không sửa tệp DWG.</span></div>' +
      '<div class="pal-body">' + (rows.length ? rows.map(function(m){ var sel = MK.cur === m.id, col = m.type === 'stamp' ? STAMPS[m.stamp][1] : m.color;
        return '<div class="mk-item' + (sel ? ' is-sel' : '') + (m.hidden ? ' is-hidden' : '') + '" data-mk="' + m.id + '" role="button" tabindex="0" aria-expanded="' + sel + '"><span class="mk-n" style="background:' + (m.status === 'done' ? 'var(--c-text-3)' : col) + ';color:' + (m.status === 'done' || lumDark(col) ? '#101214' : '#FFF') + '">' + m.n + '</span>' +
          '<span class="mk-main"><span class="mk-t">' + ic(MT[m.type][1], 's12') + '<span>' + esc(mkTitle(m)) + '</span></span><span class="mk-meta">' + esc(m.author) + ' · ' + esc(m.time.slice(0, 5) + m.time.slice(10)) + (m.src === 'compare' ? ' · từ So sánh' : '') + '</span></span>' +
          '<span class="mk-side"><span class="tag ' + (m.status === 'done' ? 'ok' : 'warn') + '">' + (m.status === 'done' ? 'Đã xử lý' : 'Mở') + '</span>' + (m.comments.length ? '<span class="mk-cc">' + ic('chat', 's12') + m.comments.length + '</span>' : '') + '</span></div>' +
          (sel ? '<div class="mk-det">' + (m.img ? '<img src="' + m.img + '" alt="Ảnh chụp vùng">' : '') + (m.text && m.text.indexOf('\n') > 0 ? '<p class="mk-full">' + esc(m.text).replace(/\n/g, '<br>') + '</p>' : '') +
            m.comments.map(function(c){ return '<div class="mk-c"><b>' + esc(c.who) + '</b><span class="faint"> · ' + esc(c.t) + '</span><p>' + esc(c.s) + '</p></div>'; }).join('') +
            '<label class="search mk-reply">' + ic('chat') + '<input data-reply="' + m.id + '" placeholder="Trả lời, Enter để gửi" aria-label="Trả lời markup ' + m.n + '"></label>' +
            '<div class="hrow" style="gap:4px;flex-wrap:nowrap"><button class="btn sm" type="button" data-ma="zoom">' + ic('zwin') + 'Zoom</button><button class="btn sm" type="button" data-ma="status">' + (m.status === 'done' ? 'Mở lại' : ic('check') + 'Đã xử lý') + '</button><span class="grow"></span><button class="ib sm" type="button" data-ma="hide" aria-label="' + (m.hidden ? 'Hiện' : 'Ẩn') + ' markup" data-tip="' + (m.hidden ? 'Hiện' : 'Ẩn') + '">' + ic(m.hidden ? 'eye' : 'eyeoff') + '</button><button class="ib sm" type="button" data-ma="del" aria-label="Xóa markup" data-tip="' + (m.author === ME ? 'Xóa' : 'Chỉ người tạo xóa được') + '"' + (m.author === ME ? '' : ' disabled') + '>' + ic('trash') + '</button></div></div>' : ''); }).join('')
        : '<div class="empty">' + ic('markup') + '<b>' + (list.length ? 'Không có markup khớp bộ lọc' : 'Chưa có markup') + '</b>' + (list.length ? 'Đổi tác giả hoặc trạng thái.' : 'Dùng tab Ghi chú: đám mây, mũi tên, chữ, con dấu. Markup không sửa tệp DWG.') + '</div>') + '</div>' +
      '<div class="pal-foot"><span>' + list.length + ' markup · ' + open + ' mở</span><span class="grow"></span><button class="btn sm" type="button" data-ma="pdf">' + ic('pdf') + 'Xuất PDF</button></div>';
    host.onclick = function(e){
      var f = e.target.closest('[data-mf]'); if (f){ var r = f.getBoundingClientRect(), w = f.dataset.mf; TC.menu(w === 'who' ? [{ label:'Mọi tác giả', check:MK.who === 'all', act:function(){ MK.who = 'all'; TC.emit('markups'); } }].concat(authors.map(function(a){ return { label:esc(a) + (a === ME ? ' (tôi)' : ''), check:MK.who === a, act:function(){ MK.who = a; TC.emit('markups'); } }; })) : [['all','Mọi trạng thái'],['open','Đang mở'],['done','Đã xử lý']].map(function(s){ return { label:s[1], check:MK.st === s[0], act:function(){ MK.st = s[0]; TC.emit('markups'); } }; }), r.left, r.bottom + 2, { minWidth:r.width }); return; }
      var a = e.target.closest('[data-ma]'), it = e.target.closest('[data-mk]');
      if (a){ var k = a.dataset.ma, m = list.filter(function(x){ return x.id === MK.cur; })[0];
        if (k === 'more'){ var r2 = a.getBoundingClientRect(); TC.menu([{ label:'Nhập markup từ tệp…', icon:'open', act:importMk }, { label:'Xuất PDF kèm markup…', icon:'pdf', act:exportPdf }, { sep:1 }, { label:'Hiện tất cả', icon:'eye', act:function(){ list.forEach(function(x){ x.hidden = false; }); TC.emit('markups'); } }, { label:'Ẩn tất cả', icon:'eyeoff', act:function(){ list.forEach(function(x){ x.hidden = true; }); TC.emit('markups'); } }, { sep:1 }, { label:'Đưa markup vào bản vẽ (layer TC-MARKUP)', need:'PRO', act:function(){ TC.toast('Đã đưa ' + list.length + ' markup vào layer TC-MARKUP.', { log:false }); } }], r2.right, r2.bottom + 2, { alignRight:true }); return; }
        if (k === 'pdf'){ exportPdf(); return; }
        if (!m) return;
        if (k === 'zoom') zoomMk(m);
        else if (k === 'status'){ m.status = m.status === 'done' ? 'open' : 'done'; m.comments.push({ who:ME, t:stamp(), s:m.status === 'done' ? 'Đã đánh dấu đã xử lý.' : 'Mở lại.' }); TC.emit('markups'); }
        else if (k === 'hide'){ m.hidden = !m.hidden; TC.emit('markups'); }
        else if (k === 'del' && m.author === ME) TC.confirm({ title:'Xóa markup', msg:'Xóa markup #' + m.n + ' “' + esc(mkTitle(m)) + '”?', detail:'Bình luận của markup cũng bị xóa. Tệp DWG không bị ảnh hưởng.', ok:'Xóa markup', danger:true }).then(function(y){ if (y){ d.markups = list.filter(function(x){ return x !== m; }); MK.cur = null; TC.emit('markups'); } });
        return; }
      if (it && !e.target.closest('.mk-det')){ var id = Number(it.dataset.mk); MK.cur = MK.cur === id ? null : id; var mm = list.filter(function(x){ return x.id === id; })[0]; if (MK.cur && mm) zoomMk(mm, true); TC.emit('markups'); }
    };
    host.onkeydown = function(e){ var inp = e.target.closest('[data-reply]'); if (inp && e.key === 'Enter' && inp.value.trim()){ var m = list.filter(function(x){ return x.id === Number(inp.dataset.reply); })[0]; m.comments.push({ who:ME, t:stamp(), s:inp.value.trim() }); TC.emit('markups'); setTimeout(function(){ var n = $('[data-reply="' + m.id + '"]'); if (n) n.focus(); }, 0); } if (e.target.closest('[data-reply]')) e.stopPropagation(); };
  } };
  function zoomMk(m, soft){ var d = D(); if (m.sp !== d.space) A().setSpace(m.sp); var b = union([m.pts.reduce(function(a, q){ return [Math.min(a[0], q[0]), Math.min(a[1], q[1]), Math.max(a[2], q[0]), Math.max(a[3], q[1])]; }, [Infinity, Infinity, -Infinity, -Infinity])]); var pad = m.h ? m.h * 3 : 0; b = [b[0] - pad, b[1] - pad, b[2] + pad * 4, b[3] + pad]; if (soft){ var v = V(), s = v.mToS([(b[0] + b[2]) / 2, (b[1] + b[3]) / 2]); if (s && s[0] > 40 && s[0] < v.W - 40 && s[1] > 40 && s[1] < v.H - 40) return; } zoomBB(b); }
  function importMk(){
    var d = D();
    TC.confirm({ title:'Nhập markup', msg:'Gộp markup từ <b>Banve-01 (Pham Hoang Nam).tcmk</b>?', detail:'2 markup của Phạm Hoàng Nam (chủ đầu tư), gửi 17/10/2026. Markup trùng vị trí và nội dung được bỏ qua; markup của bạn giữ nguyên.', ok:'Gộp 2 markup' }).then(function(y){
      if (!y) return; var list = mks(d);
      list.push(Object.assign(mkNew({ type:'cloud', sp:'model', pts:[[110, 110], [3200, 2500]], color:'#C77DFF', author:'Phạm Hoàng Nam', time:'17/10/2026 20:05', text:'Phòng khách muốn rộng hơn, bỏ bớt sofa góc?' }), { comments:[] }));
      list.push(Object.assign(mkNew({ type:'arrow', sp:'model', pts:[[-2400, 7400], [-150, 7300]], color:'#C77DFF', author:'Phạm Hoàng Nam', time:'17/10/2026 20:09', text:'Cửa sổ P. ngủ 1 làm cửa đi ra sân?' }), { comments:[] }));
      TC.emit('markups'); TC.toast('Đã gộp 2 markup của Phạm Hoàng Nam vào ' + esc(d.mkFile) + '.', { log:false });
    });
  }
  function exportPdf(){ var d = D(); TC.toast('Đã xuất <span class="mono">' + esc(d.name.replace(/\.\w+$/, '')) + '_markup.pdf</span>: bản vẽ kèm ' + mks(d).length + ' markup và trang tổng hợp bình luận.', { act:{ label:'Mở tệp', fn:function(){} }, kind:'task' }); }
  exitOnDoc(function(){ MK.cur = null; });
})();
