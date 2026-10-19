/* TrionCAD prototype · Đợt 5 (sau v1.0): làm sạch hình học và sức khỏe bản vẽ (I08), kiểm tra TCVN, chuẩn .dws, dịch layer (I09). */
(function(){
  'use strict';
  var TC = window.TC, $ = TC.$, $$ = TC.$$, esc = TC.esc, ic = TC.ic, G = TC.geo, IMPL = TC.cmd.impl, ED = TC.ed, SC = TC.screens, H = SC.h;
  function A(){ return TC.app; } function V(){ return TC.app.view; } function D(){ return TC.app.doc(); }
  var OPEN = SC.open5 = SC.open5 || {}; // mã màn hình → hàm mở (danh mục gọi khi bấm); tiện ích mở ở SC.cat (D1, PRO, MOD, RUN, plan, pick, flash)

  /* ---------- dùng chung ---------- */
  var EPS = 1e-3;
  function pro(){ return TC.can('PRO'); }
  function mid(a, b){ return [(a[0] + b[0]) / 2, (a[1] + b[1]) / 2]; }
  function unit(a, b){ var L = G.dist(a, b) || 1; return [(b[0] - a[0]) / L, (b[1] - a[1]) / L]; }
  function bbOf(list){ var b = [Infinity, Infinity, -Infinity, -Infinity]; list.forEach(function(e){ var q = e._bb; if (!q) return; b = [Math.min(b[0], q[0]), Math.min(b[1], q[1]), Math.max(b[2], q[2]), Math.max(b[3], q[3])]; }); return b; }
  function grow(b, m){ return [b[0] - m, b[1] - m, b[2] + m, b[3] + m]; }
  function hhmm(){ var t = new Date(); return ('0' + t.getHours()).slice(-2) + ':' + ('0' + t.getMinutes()).slice(-2); }
  function fname(d){ return SC.att.ascii(d.name.replace(/\.\w+$/, '')); }
  function lockTag(){ return pro() ? '' : ' <span class="ed xs" data-ed="pro">Pro</span>'; }
  // mọi đối tượng cấp trên cùng: Model và các layout, kèm không gian
  function allEnts(d){ var out = d.ents.map(function(e){ return { e:e, sp:'model' }; }); d.layouts.forEach(function(L, i){ L.ents.forEach(function(e){ out.push({ e:e, sp:i }); }); }); return out; }
  // panel nổi sát mép phải vùng vẽ: không chặn vùng vẽ, cao theo vùng vẽ, kéo đi được; panel thứ hai lệch xuống để không che nhau
  function dockR(dlg, w){
    var r = $('#cv').getBoundingClientRect(), bn = ($('#banners') || {}).offsetHeight || 0, y0 = r.height - bn >= 492 ? r.top + bn : r.top, h = Math.round(Math.max(380, Math.min(660, r.bottom - y0 - 12))), el = dlg.el, k = $$('.dlg.rvd').filter(function(x){ return x !== el; }).length;
    el.classList.add('rvd'); el.style.width = w + 'px'; el.style.height = h + 'px'; // đủ chỗ thì đặt dưới banner để không che nút của banner
    el.style.left = Math.max(12, r.right - w - 10 - k * 28) + 'px'; el.style.top = Math.min(innerHeight - h - 12, y0 + 6 + k * 28) + 'px';
  }
  // phóng tới vùng ở phần vùng vẽ không bị panel che; đối tượng trong layout thì chuyển sang layout đó
  function zoomTo(d, bb, sp, el){
    if (sp != null && sp !== d.space) A().setSpace(sp);
    var m = d.space === 'model' ? 900 : 12, c = mid([bb[0], bb[1]], [bb[2], bb[3]]), w = Math.max(bb[2] - bb[0], m), h = Math.max(bb[3] - bb[1], m); // Model: mm bản vẽ; layout: mm giấy
    bb = [c[0] - w / 2, c[1] - h / 2, c[0] + w / 2, c[1] + h / 2];
    if (el && document.contains(el)) SC.zoomBeside(grow(bb, Math.max(w, h) * .25), el); else V().zoomTo(bb);
  }
  // quét xong: đưa focus vào danh sách (↑↓ chọn, Enter sửa, Del bỏ qua) nếu focus đang ở trong panel
  function listFocus(dlg){ var l = dlg && $('.rv-list', dlg.el); if (l && dlg.el.contains(document.activeElement)) l.focus({ preventScroll:true }); }
  // thẻ mức độ: biểu tượng + chữ, không dùng màu làm tín hiệu duy nhất
  var SEV = { err:['Lỗi', 'error'], warn:['Cảnh báo', 'warn'], hint:['Gợi ý', 'info'] };
  function sevIc(k){ return '<span class="sev ' + k + '" title="' + SEV[k][0] + '">' + ic(SEV[k][1]) + '</span>'; }
  // tô sáng đối tượng trên lớp phủ (cùng kiểu tô kết quả tìm của vùng vẽ)
  function glow(ctx, v, list, col, a, thin){
    var d = v.doc, mv = v.modelView(); ctx.save();
    list.forEach(function(x){ var e = x.e || x, sp = x.sp == null ? 'model' : x.sp; if (!e._p || !d.byId[e.id]) return; var view = sp === 'model' ? (mv && (d.space === 'model' || d.activeVp) ? mv.v : null) : (d.space === sp && !d.activeVp ? v.vw() : null); if (!view) return;
      ctx.globalAlpha = a || .5; v.drawPrims(ctx, e, view, sp !== 'model', { color:col, lw:5 }); ctx.globalAlpha = 1; v.drawPrims(ctx, e, view, sp !== 'model', { color:thin || col, lw:1.6 }); });
    ctx.restore();
  }

  /* ================= Làm sạch hình học UI-I08 (TCCLEANUP) ================= */
  // dung sai theo loại: lưu theo người dùng; vấn đề, mục bỏ qua, nhật ký sửa lưu theo bản vẽ (d.gc)
  var GC = TC.load('gc', { tol:{ gap:5, over:50, under:50, ang:1, dup:0.01, zero:0.001 }, on:{ gap:true, over:true, under:true, ang:true, dup:true, zero:true }, lay:'geo', pick:[] });
  var GK = { // tên, mô tả, đơn vị, hình đánh dấu (path 18×18 dùng chung cho SVG và canvas), màu trên vùng vẽ
    gap:['Khe hở', 'Hai đầu mút gần nhau nhưng không chạm', 'mm', 'M16 9a7 7 0 1 1-14 0a7 7 0 1 1 14 0z', '--c-snap'],
    over:['Đoạn thừa', 'Đầu đoạn vượt quá đường giao', 'mm', 'M9 .8 17.2 9 9 17.2.8 9z', '--c-grip-hot'],
    under:['Đoạn hụt', 'Đầu đoạn chưa chạm tới đường', 'mm', 'M9 1 17.4 16.4H.6z', '--c-cmp-b'],
    ang:['Góc lệch', 'Lệch nhỏ so với phương ngang, đứng', '°', 'M2 2h14v14H2z', '--c-hl-res'],
    dup:['Đối tượng trùng', 'Trùng hình học, cùng layer', 'mm', 'M1 5h12v12H1zM5 1h12v12', '--c-hl-sel'],
    zero:['Độ dài 0', 'Line, polyline không có chiều dài', 'mm', 'M6 1h6l5 5v6l-5 5H6l-5-5V6z', '--c-grip-hover'] };
  var GORD = ['gap', 'over', 'under', 'ang', 'dup', 'zero'];
  var PATHS = {}; function path2(k){ return PATHS[k] || (PATHS[k] = new Path2D(GK[k][3])); }
  function mk(k, n){ return '<svg class="gcm" viewBox="0 0 18 18" aria-hidden="true"><path d="' + GK[k][3] + '" style="fill:var(' + GK[k][4] + ')"/>' + (n != null ? '<text x="9" y="' + (k === 'under' ? 14 : 12.2) + '">' + n + '</text>' : '') + '</svg>'; }
  function gsave(){ TC.save('gc', GC); }
  function geoLayer(n){ return !/^Defpoints$|-(DIM|TEXT|TRUC|KHUNG|VIEWPORT|GHICHU|KYHIEU)$|HATCH/i.test(n); }
  function gcOf(d){ return d.gc || (d.gc = { list:[], ign:{}, log:[], sel:null, hot:null, filter:'all', scope:'all', ids:null, at:null }); }
  // đối tượng được quét: Model, layer đang hiện, theo lọc layer và phạm vi
  function gcEnts(d, o){
    var S = gcOf(d), lay = o ? o.lay : GC.lay, ids = !o && S.scope === 'sel' && S.ids ? S.ids.reduce(function(m, id){ m[id] = 1; return m; }, {}) : null;
    return d.ents.filter(function(e){ if (ids && !ids[e.id]) return false; if (!TC.entVisible(d, e)) return false; return lay === 'all' || (lay === 'pick' ? GC.pick.indexOf(e.layer) >= 0 : geoLayer(e.layer)); });
  }
  function near(a, b, t){ return Math.abs(a[0] - b[0]) <= t && Math.abs(a[1] - b[1]) <= t; }
  function sameGeo(a, b, t){
    if (a.t !== b.t || a.layer !== b.layer) return false;
    switch (a.t){
      case 'line': return (near(a.p1, b.p1, t) && near(a.p2, b.p2, t)) || (near(a.p1, b.p2, t) && near(a.p2, b.p1, t));
      case 'circle': return near(a.c, b.c, t) && Math.abs(a.r - b.r) <= t;
      case 'arc': return near(a.c, b.c, t) && Math.abs(a.r - b.r) <= t && Math.abs(a.a0 - b.a0) < 1e-6 && Math.abs(a.a1 - b.a1) < 1e-6;
      case 'pline': { if (a.pts.length !== b.pts.length || !a.closed !== !b.closed || (a.bulges || []).some(Boolean) || (b.bulges || []).some(Boolean)) return false; var f = a.pts.every(function(p, i){ return near(p, b.pts[i], t); }), n = a.pts.length; return f || a.pts.every(function(p, i){ return near(p, b.pts[n - 1 - i], t); }); }
      case 'text': case 'mtext': return near(a.p, b.p, t) && a.s === b.s && Math.abs(a.h - b.h) <= t;
      case 'insert': return near(a.p, b.p, t) && a.name === b.name && (a.rot || 0) === (b.rot || 0) && a.sx === b.sx && a.sy === b.sy;
    }
    return false;
  }
  function midOf(e){ return e.t === 'line' ? mid(e.p1, e.p2) : e.p || e.c || (e.pts ? mid(e.pts[0], e.pts[1]) : mid([e._bb[0], e._bb[1]], [e._bb[2], e._bb[3]])); }
  function lineX(a, b, c, d){ var r = [b[0] - a[0], b[1] - a[1]], s = [d[0] - c[0], d[1] - c[1]], den = r[0] * s[1] - r[1] * s[0]; if (Math.abs(den) < 1e-9) return null; var t = ((c[0] - a[0]) * s[1] - (c[1] - a[1]) * s[0]) / den; return [a[0] + t * r[0], a[1] + t * r[1]]; }
  // quét: độ dài 0 → trùng → góc lệch → đầu mút hở (khe hở, đoạn hụt, đoạn thừa; mỗi đầu mút thuộc tối đa một vấn đề)
  // o (Sức khỏe bản vẽ): { lay, on } thay cho thiết lập của panel, quét cả Model
  function scan(d, o){
    var T = GC.tol, on = o ? o.on : GC.on, out = [], src = gcEnts(d, o), skip = {};
    src.forEach(function(e){ var L = e.t === 'line' ? G.dist(e.p1, e.p2) : e.t === 'pline' ? G.polyLen(e.pts, e.closed) : null; if (L != null && L <= T.zero){ skip[e.id] = 1; if (on.zero) out.push({ k:'zero', e:[e], p:(e.p1 || e.pts[0]).slice(), v:L, key:'z' + e.id }); } });
    var bucket = {};
    src.forEach(function(e){ if (skip[e.id]) return; var k = e.t + '|' + e.layer, list = bucket[k] || (bucket[k] = []), o = list.filter(function(x){ return sameGeo(x, e, T.dup); })[0]; if (o){ skip[e.id] = 1; if (on.dup) out.push({ k:'dup', e:[e, o], p:midOf(e), v:0, key:'d' + e.id }); } else list.push(e); });
    var segs = [], ends = [], rings = [];
    function seg(e, i, a, b, ea, eb){ var L = G.dist(a, b); if (L < EPS) return; var s = { e:e, i:i, a:a, b:b, len:L }; segs.push(s); if (ea) ends.push({ s:s, w:'a', p:a, u:unit(b, a) }); if (eb) ends.push({ s:s, w:'b', p:b, u:unit(a, b) }); }
    src.forEach(function(e){ if (skip[e.id]) return;
      if (e.t === 'line') seg(e, -1, e.p1, e.p2, true, true);
      else if (e.t === 'pline'){ var n = e.pts.length, bg = e.bulges || []; for (var i = 0; i < n - (e.closed ? 0 : 1); i++) if (!bg[i]) seg(e, i, e.pts[i], e.pts[(i + 1) % n], !e.closed && i === 0, !e.closed && i === n - 2); }
      else if (e.t === 'circle' || e.t === 'arc') rings.push(e); });
    if (on.ang) segs.forEach(function(s){ var a = Math.atan2(s.b[1] - s.a[1], s.b[0] - s.a[0]) * 180 / Math.PI, m = ((a % 90) + 90) % 90, dv = Math.min(m, 90 - m), h = ((a % 180) + 180) % 180; if (dv > 1e-4 && dv <= T.ang) out.push({ k:'ang', s:s, e:[s.e], p:mid(s.a, s.b), v:dv, horiz:h < 45 || h > 135, key:'a' + s.e.id + '.' + s.i }); });
    function onSeg(p, s){ return p[0] >= Math.min(s.a[0], s.b[0]) - EPS && p[0] <= Math.max(s.a[0], s.b[0]) + EPS && p[1] >= Math.min(s.a[1], s.b[1]) - EPS && p[1] <= Math.max(s.a[1], s.b[1]) + EPS && G.segDist(p, s.a, s.b) <= EPS; }
    // ponytail: dò đầu mút O(n²), đủ nhanh với vài nghìn đoạn; bản vẽ lớn hơn thì dùng lưới không gian
    function touches(E){ return ends.some(function(F){ return F !== E && near(F.p, E.p, EPS); }) || segs.some(function(S){ return S !== E.s && onSeg(E.p, S); }) || rings.some(function(r){ return Math.abs(G.dist(E.p, r.c) - r.r) <= EPS; }); }
    ends.forEach(function(E){ E.free = !touches(E); });
    var dang = ends.filter(function(E){ return E.free; }), used = [];
    function eKey(E){ return E.s.e.id + E.w + E.s.i; }
    if (on.gap){ var pairs = [];
      dang.forEach(function(A0, i){ dang.slice(i + 1).forEach(function(B0){ if (A0.s.e === B0.s.e && A0.s.e.t !== 'pline') return; var dd = G.dist(A0.p, B0.p); if (dd > EPS && dd <= T.gap) pairs.push([dd, A0, B0]); }); });
      pairs.sort(function(x, y){ return x[0] - y[0]; }).forEach(function(q){ if (used.indexOf(q[1]) >= 0 || used.indexOf(q[2]) >= 0) return; used.push(q[1], q[2]); out.push({ k:'gap', ends:[q[1], q[2]], e:q[1].s.e === q[2].s.e ? [q[1].s.e] : [q[1].s.e, q[2].s.e], p:mid(q[1].p, q[2].p), v:q[0], key:'g' + [eKey(q[1]), eKey(q[2])].sort().join() }); }); }
    function hit(E, q){ var best = null; segs.forEach(function(S){ if (S === E.s) return; var h = G.segInt(E.p, q, S.a, S.b); if (h){ var t = G.dist(E.p, h); if (t > EPS && (!best || t < best.t)) best = { t:t, h:h, S:S }; } }); return best; }
    if (on.under) dang.forEach(function(E){ if (used.indexOf(E) >= 0) return; var b = hit(E, [E.p[0] + E.u[0] * T.under, E.p[1] + E.u[1] * T.under]); if (b){ used.push(E); out.push({ k:'under', end:E, hit:b.h, e:[E.s.e, b.S.e], p:E.p.slice(), v:b.t, key:'u' + eKey(E) }); } });
    if (on.over) dang.forEach(function(E){ if (used.indexOf(E) >= 0) return; var L = Math.min(T.over, E.s.len - EPS), b = hit(E, [E.p[0] - E.u[0] * L, E.p[1] - E.u[1] * L]); if (b){ used.push(E); out.push({ k:'over', end:E, hit:b.h, e:[E.s.e, b.S.e], p:E.p.slice(), v:b.t, key:'o' + eKey(E) }); } });
    // đoạn lệch góc: giữ đầu đã nối, dời đầu còn hở (line nối cả hai đầu: giữ điểm đầu)
    out.forEach(function(it){ if (it.k !== 'ang') return; var s = it.s, ea = ends.filter(function(E){ return E.s === s && E.w === 'a'; })[0], eb = ends.filter(function(E){ return E.s === s && E.w === 'b'; })[0]; it.anchor = eb && !eb.free && (!ea || ea.free) ? 'b' : 'a'; var A0 = it.anchor === 'b' ? s.b : s.a, B0 = it.anchor === 'b' ? s.a : s.b; it.to = it.horiz ? [B0[0], A0[1]] : [A0[0], B0[1]]; it.from = B0.slice(); it.shift = G.dist(it.to, it.from); });
    out.forEach(function(it){ if (it.k !== 'gap') return; var P = it.ends[0], Q = it.ends[1];
      if (P.s.e === Q.s.e){ it.how = 'close'; it.to = P.p; return; }
      if (P.s.e.t === 'line' && Q.s.e.t === 'line' && P.u[0] * Q.u[0] + P.u[1] * Q.u[1] < -.99999 && Math.abs((Q.p[0] - P.p[0]) * P.u[1] - (Q.p[1] - P.p[1]) * P.u[0]) < .01){ it.how = 'join'; return; }
      var X = lineX(P.s.a, P.s.b, Q.s.a, Q.s.b); if (X && G.dist(X, P.p) <= 3 * T.gap && G.dist(X, Q.p) <= 3 * T.gap){ it.how = 'corner'; it.to = X; } else { it.how = 'mid'; it.to = mid(P.p, Q.p); } });
    out.forEach(function(it){ it.ign = !!gcOf(d).ign[it.key]; it.lay = it.e[0].layer; it.lock = it.e.some(function(e){ return (d.layerMap[e.layer] || {}).lck; }); });
    out.sort(function(x, y){ return GORD.indexOf(x.k) - GORD.indexOf(y.k) || (y.p[1] - x.p[1]) || (x.p[0] - y.p[0]); });
    out.forEach(function(it, i){ it.n = i + 1; });
    return out;
  }
  // mô tả cách sửa: hiện trong chi tiết dòng và báo cáo
  function howText(it){
    var T = GC.tol;
    switch (it.k){
      case 'gap': return { close:'Đóng polyline: dời đỉnh cuối về đỉnh đầu.', join:'Nối hai line thẳng hàng thành một.', corner:'Kéo hai đầu về giao điểm, tạo góc kín.', mid:'Dời hai đầu về điểm giữa.' }[it.how];
      case 'over': return 'Cắt phần thừa ' + TC.d(it.v) + ' mm tại giao điểm.';
      case 'under': return 'Kéo dài ' + TC.d(it.v) + ' mm tới đường gần nhất.';
      case 'ang': return 'Xoay về đúng phương ' + (it.horiz ? 'ngang' : 'đứng') + ', giữ đầu đã nối; đầu kia dời ' + TC.d(it.shift) + ' mm.';
      case 'dup': return 'Xóa bản trùng, giữ bản vẽ trước (dung sai ' + TC.d(T.dup, 3) + ' mm).';
      case 'zero': return 'Xóa đối tượng (không thấy được, làm sai bắt điểm và đếm).';
    }
  }
  function whatText(it){
    var tn = function(e){ return (TC.TYPE_NAMES[e.t] || e.t); };
    switch (it.k){
      case 'gap': return it.how === 'close' ? 'Đầu và cuối polyline cách nhau ' + TC.d(it.v) + ' mm.' : 'Đầu ' + tn(it.e[0]) + ' và đầu ' + tn(it.e[1]) + ' cách nhau ' + TC.d(it.v) + ' mm (dung sai ' + TC.d(GC.tol.gap) + ').';
      case 'over': return tn(it.e[0]) + ' vượt quá ' + tn(it.e[1]) + ' ' + TC.d(it.v) + ' mm (dung sai ' + TC.d(GC.tol.over) + ').';
      case 'under': return tn(it.e[0]) + ' còn cách ' + tn(it.e[1]) + ' ' + TC.d(it.v) + ' mm (dung sai ' + TC.d(GC.tol.under) + ').';
      case 'ang': return 'Đoạn ' + tn(it.e[0]) + ' lệch ' + TC.d(it.v, 3) + '° so với phương ' + (it.horiz ? 'ngang' : 'đứng') + ' (dung sai ' + TC.d(GC.tol.ang) + '°).';
      case 'dup': return 'Hai ' + tn(it.e[0]) + ' trùng nhau trên layer ' + esc(it.lay) + '.';
      case 'zero': return tn(it.e[0]) + ' dài ' + TC.d(it.v, 4) + ' mm.';
    }
  }
  function valText(it){ return it.k === 'dup' || it.k === 'zero' ? (TC.TYPE_NAMES[it.e[0].t] || it.e[0].t) : TC.d(it.v, 2) + (it.k === 'ang' ? '°' : ' mm'); }
  // sửa một vấn đề trong nhóm hoàn tác đang mở; đối tượng đã mất (đã sửa ở bước trước) thì bỏ qua
  function setEnd(E, p){ var e = E.s.e; ED.mod(e, function(x){ if (x.t === 'line') x[E.w === 'a' ? 'p1' : 'p2'] = p.slice(); else x.pts[E.w === 'a' ? E.s.i : (E.s.i + 1) % x.pts.length] = p.slice(); }); }
  function moveVerts(d, from, to){ d.ents.forEach(function(e){ if (e.t === 'line'){ var w = near(e.p1, from, EPS) ? 'p1' : near(e.p2, from, EPS) ? 'p2' : null; if (w) ED.mod(e, function(x){ x[w] = to.slice(); }); } else if (e.t === 'pline' && e.pts.some(function(p){ return near(p, from, EPS); })) ED.mod(e, function(x){ x.pts = x.pts.map(function(p){ return near(p, from, EPS) ? to.slice() : p; }); }); }); }
  function fixOne(d, it){
    if (it.e.some(function(e){ return !d.byId[e.id]; })) return false;
    switch (it.k){
      case 'zero': ED.del(it.e[0]); break;
      case 'dup': ED.del(it.e[0]); break;
      case 'ang': moveVerts(d, it.from, it.to); break;
      case 'under': case 'over': setEnd(it.end, it.hit); break;
      case 'gap': { var P = it.ends[0], Q = it.ends[1];
        if (it.how === 'close') ED.mod(P.s.e, function(x){ x.pts.pop(); x.closed = true; });
        else if (it.how === 'join'){ var far = Q.w === 'a' ? Q.s.e.p2 : Q.s.e.p1; setEnd(P, far); ED.del(Q.s.e); }
        else { setEnd(P, it.to); setEnd(Q, it.to); } break; }
    }
    return true;
  }
  // hình xem trước của cách sửa: phần thêm (xanh), phần bỏ (đỏ), nét đứt
  function ghost(it){
    var add = [], del = [];
    switch (it.k){
      case 'gap': if (it.how === 'join' || it.how === 'close') add.push([it.ends[0].p, it.ends[1].p]); else { add.push([it.ends[0].p, it.to], [it.ends[1].p, it.to]); } break;
      case 'under': add.push([it.end.p, it.hit]); break;
      case 'over': del.push([it.hit, it.end.p]); break;
      case 'ang': { var s = it.s, A0 = it.anchor === 'b' ? s.b : s.a; del.push([A0, it.from]); add.push([A0, it.to]); break; }
    }
    return { add:add, del:del, ents:it.k === 'dup' || it.k === 'zero' ? [it.e[0]] : [] };
  }

  /* ---------- dữ liệu mẫu: phần dán từ tệp khảo sát (OPEN UI-I08, UI-I09) ---------- */
  // đặt dưới khung A3 thứ nhất của Banve-01.dwg; có sẵn khe hở, đoạn thừa, đoạn hụt, góc lệch, trùng, độ dài 0 và layer, chữ, kích thước không theo chuẩn công ty
  var KS_O = [-12000, -26000];
  var KS_LAY = [['Ranh dat', 1, 'Continuous', 0.3], ['Nha hien trang', 3, 'Continuous', 0.25], ['Duong', 8, 'Continuous', 0.25], ['Layer1', 5, 'Continuous', 0.25], ['Text', 2, 'Continuous', 0.25]];
  function addLayer(d, l){ if (d.layerMap[l[0]]) return d.layerMap[l[0]]; var o = { name:l[0], color:l[1], lt:l[2], lw:l[3], desc:l[4] || '', on:true, frz:false, lck:false, plot:true, vpfrz:false, count:0 }; d.layers.push(o); d.layerMap[o.name] = o; return o; }
  function demo(d){
    if (d.id !== 'd1' || d.ents.some(function(e){ return e.src === 'KS'; })) return;
    KS_LAY.forEach(function(l){ addLayer(d, l); });
    if (d.dimstyles.indexOf('KS-DIM') < 0) d.dimstyles.push('KS-DIM');
    var P = function(p){ return [KS_O[0] + p[0], KS_O[1] + p[1]]; }, E = function(o){ o.id = ++d.seq; o.src = 'KS'; d.ents.push(o); TC.prepEnt(d, o); return o; };
    var L = function(lay, a, b){ E({ t:'line', p1:P(a), p2:P(b), layer:lay }); }, T = function(s, p, h, al, lay){ E({ t:'text', p:P(p), h:h, s:s, style:'Standard', al:al || 'l', layer:lay || 'Text' }); };
    E({ t:'pline', pts:[[0, 0], [24000, 600], [23400, 12000], [-300, 11500]].map(P), closed:true, layer:'Ranh dat' });
    L('Duong', [-2000, -1500], [11000, -1500]); L('Duong', [11002.5, -1500], [26000, -1500]);
    var N = 'Nha hien trang';
    L(N, [3000, 2500], [12000, 2500]); L(N, [12000, 2500], [3000, 2500]); L(N, [12000, 2503.5], [12000, 9000]); L(N, [12000, 9000], [3000, 9000]); L(N, [3000, 9000], [3000, 2462]);
    L(N, [7500, 2500], [7500, 8962]); L(N, [3000, 6000], [7471, 6000]); L(N, [12000, 4000], [14500, 4031]); L(N, [14500, 4031], [14500, 2500]); L(N, [5200, 7100], [5200, 7100]);
    L('Layer1', [16000, 3000], [19042, 3000]); L('Layer1', [19000, 3000], [19000, 5500]); L('Layer1', [19000, 5500], [16003, 5500]); L('Layer1', [16000, 5498], [16000, 3000]);
    L('Layer1', [500, 11000], [23000, 11140]); L('Layer1', [13020, 8000], [13020, 8000.0004]);
    [[6000, 10200, 800], [15000, 10100, 800], [21000, 8400, 1100]].forEach(function(c){ E({ t:'circle', c:P(c), r:c[2], layer:'0' }); });
    T('HIỆN TRẠNG KHU ĐẤT LÔ B7', [12000, -3600], 500, 'm'); T('NHÀ CŨ (PHÁ DỠ)', [5250, 4200], 250, 'm'); T('NHÀ CŨ (PHÁ DỠ)', [5250, 4200], 250, 'm');
    T('KHO TẠM', [17500, 4250], 250, 'm'); T('ĐƯỜNG NỘI BỘ', [12000, -2400], 300, 'm'); T('Nguồn: Hien-trang-khu-dat.dwg, khảo sát 05/2026', [0, -4500], 150, 'l'); T('Cây xanh hiện trạng', [21000, 6900], 200, 'm', '0');
    E({ t:'dim', p1:P([0, 0]), p2:P([24000, 600]), pos:KS_O[1] - 900, dir:'h', style:'KS-DIM', layer:'Text' });
    ED.changed(d);
  }
  function demoBB(){ return [KS_O[0] - 2500, KS_O[1] - 5000, KS_O[0] + 26500, KS_O[1] + 12500]; }

  /* ---------- panel Làm sạch hình học: tab Hình học và Sức khỏe bản vẽ ---------- */
  var GP = { dlg:null, d:null, tab:'geo', busy:null, set:false, all:false };
  function gcVis(d){ var S = gcOf(d); return S.list.filter(function(it){ return (S.showIgn || !it.ign) && (S.filter === 'all' || it.k === S.filter); }); }
  function gcCur(d){ var S = gcOf(d); return S.list.filter(function(it){ return it.key === S.sel; })[0] || null; }
  function gcTodo(d){ return gcVis(d).filter(function(it){ return !it.ign && !it.lock; }); }
  // quét lại, giữ dòng đang chọn; dòng vừa sửa biến mất thì chọn dòng kế ở cùng vị trí
  function rescan(){ var d = GP.d, S = gcOf(d), v0 = gcVis(d).map(function(x){ return x.key; }), i0 = v0.indexOf(S.sel); S.list = scan(d); S.at = hhmm(); var v1 = gcVis(d); if (!v1.some(function(x){ return x.key === S.sel; })) S.sel = v1.length ? v1[Math.min(Math.max(i0, 0), v1.length - 1)].key : null; }
  function gcOpen(tab){
    var d = D(); if (!d) return; GP.d = d; if (tab) GP.tab = tab;
    if (GP.dlg && document.contains(GP.dlg.el)){ var t = $('[data-dt="' + GP.tab + '"]', GP.dlg.el); if (t) t.click(); GP.dlg.el.focus(); return GP.dlg; }
    var dlg = GP.dlg = TC.dialog({ id:'gclean', title:'Làm sạch hình học', cmd:'TCCLEANUP', code:'UI-I08', w:440, modal:false, flush:true, focus:'.tabs [aria-selected="true"]',
      body:'<div class="rv">' + H.tabs([['geo', 'Hình học'], ['health', 'Sức khỏe bản vẽ']], GP.tab, 'gct') + '<div class="rv-pane" data-dp="geo"' + (GP.tab === 'geo' ? '' : ' hidden') + ' id="gc-geo"></div><div class="rv-pane" data-dp="health"' + (GP.tab === 'health' ? '' : ' hidden') + ' id="gc-hl"></div></div>',
      foot:'<span></span>', onClose:function(){ GP.dlg = null; GP.all = false; GP.set = false; if (GP.busy) clearInterval(GP.busy.t); GP.busy = null; V().drawOver(); } });
    dockR(dlg, 440);
    var el = dlg.el;
    el.addEventListener('dtab', function(e){ GP.tab = e.detail; GP.all = false; gcRender(); V().drawOver(); });
    el.addEventListener('click', gcClick);
    el.addEventListener('change', gcChange);
    el.addEventListener('mouseover', function(e){ var r = e.target.closest('.rv-row[data-k]'), S = gcOf(GP.d), k = r ? r.dataset.k : null; if (k !== S.hot){ S.hot = k; V().drawOver(); } });
    el.addEventListener('mouseleave', function(){ var S = gcOf(GP.d); if (S.hot){ S.hot = null; V().drawOver(); } });
    el.addEventListener('dblclick', function(e){ var r = e.target.closest('.rv-row[data-k]'); if (r && !e.target.closest('button')) gcAct('zoom', r.dataset.k); });
    el.addEventListener('keydown', function(e){
      if (!e.target.closest('.rv-list')) return; var d = GP.d, S = gcOf(d), vis = gcVis(d), i = vis.map(function(x){ return x.key; }).indexOf(S.sel);
      if (e.key === 'ArrowDown' || e.key === 'ArrowUp'){ e.preventDefault(); i = TC.clamp(i + (e.key === 'ArrowDown' ? 1 : -1), 0, vis.length - 1); if (vis[i]){ S.sel = vis[i].key; gcRender(); gcAct('zoom', S.sel); } }
      else if (e.key === 'Enter' && S.sel){ e.preventDefault(); gcAct('fix', S.sel); }
      else if (e.key === 'Delete' && S.sel){ e.preventDefault(); gcAct('ign', S.sel); }
    });
    gcRender(); return dlg;
  }
  // quét có tiến trình (mở panel, Quét lại): 3 bước, có nút Dừng
  function gcScanAnim(done){
    var d = GP.d, n = gcEnts(d).length, steps = ['Đọc ' + TC.n(n) + ' đối tượng', 'Dò đầu mút, giao điểm', 'So trùng lặp, độ dài 0'];
    if (GP.busy) clearInterval(GP.busy.t);
    GP.busy = { k:0, steps:steps, n:n, t:setInterval(function(){ if (!GP.busy) return; GP.busy.k++; if (GP.busy.k >= steps.length){ clearInterval(GP.busy.t); GP.busy = null; rescan(); var S = gcOf(d); if (!S.sel){ var v = gcVis(d); S.sel = v.length ? v[0].key : null; } gcRender(); V().drawOver(); listFocus(GP.dlg); if (done) done(); } else gcRender(); }, 220) };
    gcRender();
  }
  function cntBy(list){ var c = {}; list.forEach(function(it){ if (!it.ign) c[it.k] = (c[it.k] || 0) + 1; }); return c; }
  function sumText(list){ var c = cntBy(list); return GORD.filter(function(k){ return c[k]; }).map(function(k){ return TC.n(c[k]) + ' ' + GK[k][0].toLowerCase(); }).join(', '); }
  function gcRender(){
    if (!GP.dlg || !document.contains(GP.dlg.el)) return;
    var el = GP.dlg.el, d = GP.d, S = gcOf(d), foot = $('.dlg-f', el), host = $('#gc-geo', el), lst = $('.rv-list', host), top = lst ? lst.scrollTop : 0, foc = lst && lst === document.activeElement;
    if (GP.tab === 'health'){ hlRender(); return; }
    var bar = '<div class="rv-bar"><select class="inp sm" id="gc-scope" aria-label="Phạm vi quét" style="width:136px"><option value="all"' + (S.scope === 'all' ? ' selected' : '') + '>Cả Model</option>' + (S.ids ? '<option value="sel"' + (S.scope === 'sel' ? ' selected' : '') + '>Vùng chọn (' + TC.n(S.ids.length) + ')</option>' : '') + '<option value="pick">Chọn đối tượng…</option></select>' +
      '<select class="inp sm" id="gc-lay" aria-label="Layer được quét" style="width:150px" data-tip="Layer được quét" data-desc="Layer hình học: bỏ qua trục, kích thước, chữ, hatch, khung. Layer tắt, đóng băng luôn bỏ qua."><option value="geo"' + (GC.lay === 'geo' ? ' selected' : '') + '>Layer hình học</option><option value="all"' + (GC.lay === 'all' ? ' selected' : '') + '>Tất cả layer</option><option value="pick"' + (GC.lay === 'pick' ? ' selected' : '') + '>' + (GC.lay === 'pick' ? esc(GC.pick.length + ' layer đã chọn') : 'Chọn layer…') + '</option></select>' +
      '<span class="grow"></span><button class="ib" type="button" data-gc="scan" aria-label="Quét lại" data-tip="Quét lại" data-desc="Quét lại phạm vi đang chọn với dung sai hiện tại."' + (GP.busy ? ' disabled' : '') + '>' + ic('refresh') + '</button><button class="ib" type="button" data-gc="set" aria-pressed="' + GP.set + '" aria-label="Thiết lập dung sai" data-tip="Thiết lập dung sai">' + ic('gear') + '</button></div>';
    if (GP.set){ host.innerHTML = bar + setHTML(); foot.innerHTML = '<button class="btn" type="button" data-gc="def">Mặc định</button><span class="grow"></span><button class="btn ghost" type="button" data-gc="setx">Hủy</button><button class="btn primary" type="button" data-gc="apply">Áp dụng, quét lại</button>'; return; }
    if (GP.busy){ var b = GP.busy; host.innerHTML = bar + '<div class="rv-busy" role="status"><div class="hrow"><span class="spin"></span><b>Đang quét ' + (S.scope === 'sel' ? 'vùng chọn' : 'Model') + '</b><span class="grow"></span><span class="mono faint">' + Math.round((b.k + 1) / (b.steps.length + 1) * 100) + '%</span></div><div class="prog"><i style="width:' + Math.round((b.k + 1) / (b.steps.length + 1) * 100) + '%"></i></div><ol class="rv-steps">' + b.steps.map(function(s, i){ return '<li class="' + (i < b.k ? 'done' : i === b.k ? 'cur' : '') + '">' + (i < b.k ? ic('check', 's12') : '<i></i>') + s + '</li>'; }).join('') + '</ol></div>'; foot.innerHTML = '<span class="faint rv-s">Vùng vẽ vẫn dùng được trong lúc quét.</span><span class="grow"></span><button class="btn" type="button" data-gc="stop">Dừng</button>'; return; }
    var all = S.list.filter(function(it){ return !it.ign; }), nIgn = S.list.length - all.length, c = cntBy(S.list), vis = gcVis(d), todo = gcTodo(d), cur = gcCur(d);
    var chip = function(k, lab, n){ return '<button class="rv-chip" type="button" data-gf="' + k + '" aria-pressed="' + (S.filter === k) + '">' + lab + ' <b>' + TC.n(n) + '</b></button>'; };
    var notes = (!pro() ? '<div class="note warn rv-note">' + ic('lock') + '<span>Bản ' + esc(TC.EDITIONS[TC.S.edition].badge) + ' chỉ xem kết quả quét. Sửa hình học cần TrionCAD Pro.</span><button class="btn sm" type="button" data-gc="up">Nâng cấp</button></div>' : '') +
      (d.space !== 'model' ? '<div class="note rv-note">' + ic('info') + '<span>Đang ở layout. Đánh dấu vấn đề chỉ hiện trong Model.</span><button class="btn sm" type="button" data-gc="model">Về Model</button></div>' : '') +
      (GP.all ? '<div class="note acc rv-note">' + ic('preview') + '<span><b>Xem trước trên bản vẽ:</b> sửa ' + TC.n(todo.length) + ' vấn đề (' + sumText(todo) + '). Nét xanh là phần thêm, nét đỏ là phần bỏ, đối tượng tô đỏ sẽ bị xóa.</span></div>' : '');
    var body;
    if (!S.at) body = '<div class="empty">' + ic('geoclean') + '<b>Chưa quét</b><span>Bấm Quét lại để tìm khe hở, đoạn thừa, đoạn hụt, góc lệch, đối tượng trùng, độ dài 0.</span></div>';
    else if (!all.length && !nIgn) body = '<div class="empty">' + ic('okc') + '<b>Không có vấn đề hình học</b><span>Đã quét ' + TC.n(gcEnts(d).length) + ' đối tượng ' + (S.scope === 'sel' ? 'trong vùng chọn' : 'trong Model') + ' với dung sai hiện tại.</span><button class="btn sm" type="button" data-gc="set">Đổi dung sai</button></div>';
    else body = '<div class="rv-chips" role="group" aria-label="Lọc theo loại">' + chip('all', 'Tất cả', all.length) + GORD.filter(function(k){ return c[k]; }).map(function(k){ return chip(k, mk(k) + GK[k][0], c[k]); }).join('') + '</div>' + notes +
      '<div class="rv-list" role="listbox" tabindex="0" aria-label="Vấn đề hình học"' + (cur ? ' aria-activedescendant="gcr-' + cur.key + '"' : '') + '>' + (vis.length ? vis.map(function(it){ return gcRow(it, it === cur, !GP.all); }).join('') : '<div class="empty">' + ic('filter') + '<span>Không có vấn đề trong nhóm đang lọc.</span></div>') + '</div>';
    host.innerHTML = bar + (S.at && (all.length || nIgn) ? '' : notes) + body;
    var nl = $('.rv-list', host); if (nl){ nl.scrollTop = top; var se = $('.rv-row.is-sel', nl); if (se) se.scrollIntoView({ block:'nearest' }); if (foc) nl.focus({ preventScroll:true }); }
    foot.innerHTML = GP.all ? '<button class="btn" type="button" data-gc="back">Quay lại</button><span class="grow"></span><span class="faint rv-s">Một bước hoàn tác</span><button class="btn primary" type="button" data-gc="go">Sửa ' + TC.n(todo.length) + ' vấn đề</button>'
      : '<button class="btn" type="button" data-gc="rpt"' + (S.at ? '' : ' disabled') + '>' + ic('list') + 'Báo cáo</button>' + (nIgn ? '<label class="chk rv-s"><input type="checkbox" id="gc-ign"' + (S.showIgn ? ' checked' : '') + '> Hiện ' + TC.n(nIgn) + ' mục bỏ qua</label>' : '') + '<span class="grow"></span><button class="btn primary" type="button" data-gc="all"' + (todo.length ? '' : ' disabled') + ' data-tip="Sửa tất cả" data-desc="Xem trước mọi cách sửa trên bản vẽ, rồi sửa trong một bước hoàn tác.">Sửa ' + (S.filter === 'all' ? 'tất cả' : GK[S.filter][0].toLowerCase()) + (todo.length ? ' ' + TC.n(todo.length) : '') + lockTag() + '</button>';
  }
  function gcRow(it, sel, det){
    return '<div class="rv-row' + (sel ? ' is-sel' : '') + (it.ign ? ' ign' : '') + '" role="option" id="gcr-' + it.key + '" aria-selected="' + sel + '" data-k="' + it.key + '">' + mk(it.k, it.n) + '<span class="t">' + GK[it.k][0] + ' <b class="mono">' + valText(it) + '</b>' + (it.ign ? ' <span class="tag">bỏ qua</span>' : '') + '</span><span class="m" title="Layer ' + esc(it.lay) + '">' + esc(it.lay) + '</span>' +
      '<span class="acts"><button class="ib sm" type="button" data-ga="zoom" aria-label="Zoom tới" data-tip="Zoom tới">' + ic('zoom') + '</button>' + (it.ign ? '<button class="ib sm" type="button" data-ga="unign" aria-label="Hiện lại" data-tip="Bỏ đánh dấu bỏ qua">' + ic('eye') + '</button>' : '<button class="ib sm" type="button" data-ga="ign" aria-label="Bỏ qua" data-tip="Bỏ qua" data-key="Del">' + ic('eyeoff') + '</button><button class="ib sm" type="button" data-ga="fix" aria-label="Sửa" data-tip="Sửa" data-key="Enter"' + (it.lock ? ' disabled' : '') + '>' + ic('check') + '</button>') + '</span></div>' +
      (sel && det ? '<div class="rv-det"><p>' + whatText(it) + '</p><p><b>Cách sửa:</b> ' + howText(it) + '</p>' + (it.lock ? '<div class="note warn">' + ic('lock') + '<span>Layer ' + esc(it.lay) + ' đang khóa. Mở khóa layer để sửa.</span></div>' : '') +
        '<div class="hrow">' + (it.ign ? '<button class="btn sm" type="button" data-ga="unign">Bỏ đánh dấu bỏ qua</button>' : '<button class="btn sm primary" type="button" data-ga="fix"' + (it.lock ? ' disabled' : '') + '>Sửa' + lockTag() + '</button><button class="btn sm" type="button" data-ga="ign">Bỏ qua</button>') + '<button class="btn sm ghost" type="button" data-ga="zoom">' + ic('zoom') + 'Zoom tới</button><span class="grow"></span><span class="faint mono rv-s">' + TC.d(it.p[0], 0) + ', ' + TC.d(it.p[1], 0) + '</span></div></div>' : '');
  }
  function setHTML(){
    var big = GC.tol.gap > 50 || GC.tol.ang > 2 || GC.tol.over > 200 || GC.tol.under > 200;
    return '<div class="rv-set"><div class="gh">Dung sai theo loại</div>' + GORD.map(function(k){ var z = k === 'zero' || k === 'dup'; return '<div class="gcs"><label class="chk"><input type="checkbox" data-gon="' + k + '"' + (GC.on[k] ? ' checked' : '') + '>' + mk(k) + GK[k][0] + '</label>' + TC.numField({ id:'gt-' + k, value:TC.d(GC.tol[k], z ? 3 : 2), label:'Dung sai ' + GK[k][0], unit:k === 'ang' ? 'deg' : 'len', prec:z ? 3 : 2, min:0, step:k === 'ang' ? .1 : z ? .001 : 1, pick:k !== 'ang', style:'width:104px' }) + '<span class="faint d">' + GK[k][1] + ' · ' + (k === 'ang' ? 'độ' : 'mm') + '</span></div>'; }).join('') +
      (big ? '<div class="note warn">' + ic('warn') + '<span>Dung sai lớn có thể sửa nhầm chi tiết thật (ranh đất, khe co giãn). Nên xem trước trước khi sửa tất cả.</span></div>' : '') +
      '<div class="gh">Layer được quét</div><div class="stack" style="gap:5px">' + H.rad('gcl', 'geo', 'Layer hình học <span class="faint">(bỏ trục, kích thước, chữ, hatch, khung)</span>', GC.lay === 'geo') + H.rad('gcl', 'all', 'Tất cả layer', GC.lay === 'all') + H.rad('gcl', 'pick', 'Chọn layer… <span class="faint mono">' + esc(GC.pick.slice(0, 3).join(', ') + (GC.pick.length > 3 ? '…' : '')) + '</span>', GC.lay === 'pick') + '</div><p class="faint rv-s" style="margin:0">Layer tắt, đóng băng luôn bỏ qua. Dung sai lưu cho mọi bản vẽ.</p></div>';
  }
  function gcAct(a, key){
    var d = GP.d, S = gcOf(d), it = S.list.filter(function(x){ return x.key === key; })[0]; if (!it) return;
    S.sel = key;
    if (a === 'zoom'){ zoomTo(d, bbOf(it.e), 'model', GP.dlg && GP.dlg.el); gcRender(); return; }
    if (a === 'ign' || a === 'unign'){ if (a === 'ign') S.ign[key] = 1; else delete S.ign[key]; it.ign = a === 'ign'; if (a === 'ign') S.log.push({ k:it.k, v:it.v, lay:it.lay, p:it.p, act:'Bỏ qua', at:hhmm() }); var v0 = gcVis(d).map(function(x){ return x.key; }); if (v0.indexOf(key) < 0){ var all0 = S.list.filter(function(x){ return S.showIgn || !x.ign || x.key === key; }), j = all0.indexOf(it); var nx = all0.slice(j + 1).concat(all0.slice(0, j)).filter(function(x){ return v0.indexOf(x.key) >= 0; })[0]; S.sel = nx ? nx.key : null; } gcRender(); V().drawOver(); return; }
    if (a === 'fix'){
      if (!pro()){ SC.upgrade('Làm sạch hình học', 'PRO'); return; } if (it.lock) return;
      ED.run('Làm sạch hình học: ' + GK[it.k][0].toLowerCase(), function(){ fixOne(d, it); });
      S.log.push({ k:it.k, v:it.v, lay:it.lay, p:it.p, act:'Đã sửa', at:hhmm() });
      TC.cmdline.msg('TCCLEANUP: đã sửa ' + GK[it.k][0].toLowerCase() + ' #' + it.n + ' (' + howText(it).replace(/\.$/, '').toLowerCase() + '). Ctrl+Z để hoàn tác.');
    }
  }
  function gcClick(e){
    var d = GP.d, S = gcOf(d), b = e.target.closest('[data-gc]'), f = e.target.closest('[data-gf]'), r = e.target.closest('.rv-row[data-k]'), a = e.target.closest('[data-ga]');
    if (e.target.closest('#gc-hl') || e.target.closest('[data-hk]')) return hlClick(e);
    if (e.target.closest('[data-pick]') && e.target.closest('.rv-set')){ pickLen(e.target.closest('[data-num]')); return; }
    if (f){ S.filter = f.dataset.gf; var v = gcVis(d); if (!v.some(function(x){ return x.key === S.sel; })) S.sel = v.length ? v[0].key : null; gcRender(); V().drawOver(); return; }
    if (a && r){ gcAct(a.dataset.ga, r.dataset.k); return; }
    if (a){ var c = gcCur(d); if (c) gcAct(a.dataset.ga, c.key); return; }
    if (r){ S.sel = r.dataset.k; gcRender(); V().drawOver(); var l = $('.rv-list', GP.dlg.el); if (l) l.focus({ preventScroll:true }); return; }
    if (!b || b.disabled) return; var k = b.dataset.gc;
    if (k === 'scan'){ GP.all = false; gcScanAnim(); }
    else if (k === 'stop'){ clearInterval(GP.busy.t); GP.busy = null; gcRender(); TC.cmdline.msg('TCCLEANUP: đã dừng quét. Danh sách giữ kết quả lần quét trước.'); }
    else if (k === 'set' || k === 'setx'){ GP.set = k === 'set' ? !GP.set : false; GP.all = false; gcRender(); }
    else if (k === 'def'){ GC.tol = { gap:5, over:50, under:50, ang:1, dup:0.01, zero:0.001 }; GC.on = { gap:true, over:true, under:true, ang:true, dup:true, zero:true }; GC.lay = 'geo'; gcRender(); }
    else if (k === 'apply'){ var bad = $$('.rv-set .numf', GP.dlg.el).map(TC.numCommit).filter(function(x){ return x.err; }); if (bad.length) return; GORD.forEach(function(kk){ var inp = $('#gt-' + kk, GP.dlg.el); GC.tol[kk] = Number(inp.value); GC.on[kk] = $('[data-gon="' + kk + '"]', GP.dlg.el).checked; }); gsave(); GP.set = false; gcScanAnim(); }
    else if (k === 'all'){ if (!pro()){ SC.upgrade('Làm sạch hình học', 'PRO'); return; } GP.all = true; gcRender(); V().drawOver(); }
    else if (k === 'back'){ GP.all = false; gcRender(); V().drawOver(); }
    else if (k === 'go'){ var list = gcTodo(d), n0 = list.length; GP.all = false;
      ED.run('Làm sạch hình học: sửa ' + n0 + ' vấn đề', function(){ ['dup', 'zero', 'ang', 'gap', 'under', 'over'].forEach(function(kk){ list.filter(function(x){ return x.k === kk; }).forEach(function(x){ if (fixOne(d, x)) S.log.push({ k:x.k, v:x.v, lay:x.lay, p:x.p, act:'Đã sửa', at:hhmm() }); }); }); });
      TC.toast('Đã sửa <b>' + TC.n(n0) + '</b> vấn đề hình học: ' + sumText(list) + '.', { log:false, act:{ label:'Hoàn tác', fn:function(){ ED.undo(1); } } }); TC.cmdline.msg('TCCLEANUP: đã sửa ' + TC.n(n0) + ' vấn đề trong một bước hoàn tác.'); }
    else if (k === 'rpt') gcReport(d);
    else if (k === 'up') SC.upgrade('Làm sạch hình học', 'PRO');
    else if (k === 'model'){ A().setSpace('model'); gcRender(); }
  }
  function gcChange(e){
    var d = GP.d, S = gcOf(d), t = e.target;
    if (t.closest('#gc-hl')) return;
    if (t.id === 'gc-ign'){ S.showIgn = t.checked; gcRender(); V().drawOver(); }
    else if (t.id === 'gc-scope'){ if (t.value === 'pick'){ t.value = S.scope; pickScope(); } else { S.scope = t.value; gcScanAnim(); } }
    else if (t.id === 'gc-lay' || t.name === 'gcl'){ var v = t.value; if (v === 'pick'){ SC.layerPickList(d, GC.pick).then(function(list){ if (list && list.length){ GC.pick = list; GC.lay = 'pick'; gsave(); } gcRender(); if (list && list.length && t.id === 'gc-lay') gcScanAnim(); }); return; } GC.lay = v; gsave(); if (t.id === 'gc-lay') gcScanAnim(); }
  }
  // chọn phạm vi trên bản vẽ: ẩn panel, chọn đối tượng, Enter để kết thúc
  function pickScope(){
    var d = GP.d, S = gcOf(d), dlg = GP.dlg; dlg.hide(true); TC.cad.clearSel(); TC.cmdline.msg('TCCLEANUP: chọn đối tượng cần quét, Enter để kết thúc.');
    var esc1 = function(e){ if (e.key === 'Escape'){ TC.cad.endSel(); } };
    document.addEventListener('keydown', esc1, true);
    TC.cad.getSel({}).then(function(ids){ document.removeEventListener('keydown', esc1, true); dlg.hide(false); if (ids && ids.length){ S.ids = ids.slice(); S.scope = 'sel'; TC.cad.clearSel(); gcScanAnim(); } else gcRender(); });
    var enter = function(e){ if (e.key === 'Enter' && !e.target.closest('.dlg')){ e.preventDefault(); document.removeEventListener('keydown', enter, true); TC.cad.endSel(); } }; document.addEventListener('keydown', enter, true);
  }
  // nút lấy từ bản vẽ của ô dung sai (UI-C09): chỉ hai điểm, khoảng cách thành giá trị
  function pickLen(box){
    var dlg = GP.dlg; dlg.hide(true); TC.cmdline.msg('Chỉ điểm thứ nhất để đo dung sai (Esc để hủy).');
    var stop = function(e){ if (e.key === 'Escape') TC.cad.endMode(); }; document.addEventListener('keydown', stop, true);
    var fin = function(v){ document.removeEventListener('keydown', stop, true); dlg.hide(false); if (v != null){ $('input', box).value = TC.d(v, 3); TC.numCommit(box); } };
    TC.cad.getPoint({}).then(function(a){ if (!a) return fin(); TC.cmdline.msg('Chỉ điểm thứ hai.'); TC.cad.getPoint({ base:a.p }).then(function(b){ fin(b ? G.dist(a.p, b.p) : null); }); });
  }
  // lớp phủ: hình đánh dấu theo loại kèm số thứ tự; dòng đang rê hoặc chọn tô sáng đối tượng và hiện cách sửa
  TC.overlays.push(function(ctx, v){
    var d = v.doc; if (!GP.dlg || !document.contains(GP.dlg.el) || GP.d !== d || GP.tab !== 'geo' || GP.set || (d.space !== 'model' && !d.activeVp)) return;
    var S = gcOf(d), vis = gcVis(d), S2 = function(p){ return v.mToS(p); }, acc = TC.cssv('--c-accent'), addC = TC.cssv('--c-track'), delC = TC.cssv('--c-grip-hot');
    var focus = GP.all ? gcTodo(d) : vis.filter(function(it){ return it.key === S.hot || it.key === S.sel; });
    focus.forEach(function(it){ if (it.e.every(function(e){ return d.byId[e.id]; })) glow(ctx, v, it.k === 'dup' || it.k === 'zero' ? [] : it.e, TC.cssv('--c-hl-res'), GP.all ? .12 : .35); var g = ghost(it); ctx.save(); ctx.lineWidth = 2; ctx.setLineDash([6, 4]);
      [[g.add, addC], [g.del, delC]].forEach(function(z){ ctx.strokeStyle = z[1]; z[0].forEach(function(l){ var a = S2(l[0]), b = S2(l[1]); if (!a || !b) return; if (G.dist(a, b) < 3){ ctx.beginPath(); ctx.arc(b[0], b[1], 4, 0, 7); ctx.stroke(); return; } ctx.beginPath(); ctx.moveTo(a[0], a[1]); ctx.lineTo(b[0], b[1]); ctx.stroke(); }); });
      ctx.restore(); if (g.ents.length) glow(ctx, v, g.ents, delC, .45); });
    ctx.save(); ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
    vis.slice().reverse().forEach(function(it){ var s = S2(it.p); if (!s || s[0] < -20 || s[1] < -20 || s[0] > v.W + 20 || s[1] > v.H + 20) return; var on = it.key === S.sel || it.key === S.hot, k = on ? 1.25 : 1;
      ctx.save(); ctx.translate(s[0] + 11, s[1] - 11); ctx.scale(k, k); ctx.translate(-9, -9); ctx.globalAlpha = it.ign ? .45 : 1;
      if (on){ ctx.lineWidth = 4; ctx.strokeStyle = acc; ctx.stroke(path2(it.k)); }
      ctx.fillStyle = TC.cssv(GK[it.k][4]); ctx.fill(path2(it.k)); ctx.lineWidth = 1; ctx.strokeStyle = TC.cssv('--c-canvas'); ctx.stroke(path2(it.k));
      ctx.fillStyle = TC.cssv('--c-canvas'); ctx.font = '600 ' + (it.n > 99 ? 7.5 : 9.5) + 'px ' + TC.cssv('--f-mono'); ctx.fillText(String(it.n), 9, it.k === 'under' ? 12 : 9.5); ctx.restore();
      ctx.strokeStyle = TC.cssv(GK[it.k][4]); ctx.lineWidth = 1; ctx.beginPath(); ctx.moveTo(s[0] - 3, s[1]); ctx.lineTo(s[0] + 3, s[1]); ctx.moveTo(s[0], s[1] - 3); ctx.lineTo(s[0], s[1] + 3); ctx.stroke(); });
    ctx.restore();
  });
  // sửa, hoàn tác, đổi bản vẽ: quét lại ngay (không hiện tiến trình)
  var gcT = 0;
  TC.on('dirty', function(d){ if (!GP.dlg || !document.contains(GP.dlg.el) || d !== GP.d || GP.busy) return; clearTimeout(gcT); gcT = setTimeout(function(){ if (!GP.dlg) return; var S = gcOf(GP.d); if (S.at) rescan(); gcRender(); V().drawOver(); }, 0); });
  TC.on('doc', function(d){ if (!GP.dlg || !document.contains(GP.dlg.el) || !d) return; GP.d = d; GP.all = false; var S = gcOf(d); if (!S.at){ rescan(); var v = gcVis(d); S.sel = v.length ? v[0].key : null; } gcRender(); });
  TC.on('edition', function(){ if (GP.dlg && document.contains(GP.dlg.el)){ GP.all = false; gcRender(); } });
  TC.on('space', function(){ if (GP.dlg && document.contains(GP.dlg.el)) gcRender(); });

  /* ---------- Sức khỏe bản vẽ: tóm tắt một trang, mỗi mục có nút sửa nhanh ---------- */
  var PROXY0 = { d1:[3, 'AecDbWall · AutoCAD Architecture'] }; // dữ liệu mẫu: bản vẽ nhận từ đối tác dùng AutoCAD Architecture
  function usedBlocks(d){ var u = {}; function w(list){ list.forEach(function(e){ if (e.t === 'insert' && !u[e.name]){ u[e.name] = 1; if (d.blocks[e.name]) w(d.blocks[e.name].ents); } }); } w(d.ents); d.layouts.forEach(function(L){ w(L.ents); }); return u; }
  function health(d){
    var geo = scan(d, { lay:'all', on:{ dup:true, zero:true } });
    var u = usedBlocks(d), ub = Object.keys(d.blocks).filter(function(n){ return !/^\*/.test(n) && !u[n]; }), ul = d.layers.filter(function(l){ return !l.count && l.name !== '0' && l.name !== 'Defpoints' && l.name !== ED.clayer(d); });
    var zero = geo.filter(function(it){ return it.k === 'zero'; }), dup = geo.filter(function(it){ return it.k === 'dup'; });
    var px = d.proxyGone ? null : PROXY0[d.id], xr = d.xrefs.filter(function(r){ return r.status === 'Không tìm thấy'; }), im = d.images.filter(function(r){ return r.status === 'Không tìm thấy'; });
    var fs = d.styles.filter(function(s){ return s.missing && s.count; }), regs = d.purgedRegs ? 0 : (d.id === 'd1' ? 1240 : 86);
    var save = regs * 1500 + ub.length * 24000 + ul.length * 900 + (dup.length + zero.length) * 180 + (px ? 160000 : 0), mb = d.bytes || 1048576;
    var it = [
      { id:'reg', t:'Ứng dụng đăng ký (regapps)', n:regs, sev:regs > 500 ? 'warn' : regs ? 'hint' : 'ok', d:regs ? 'Do plugin, bản vẽ thuê ngoài để lại; làm tệp nặng, mở chậm, copy dán chậm.' : 'Không còn regapps thừa.', a:'Dọn dẹp…', cmd:'PURGE', pen:regs > 500 ? 8 : regs ? 3 : 0 },
      { id:'blk', t:'Block, layer không dùng', n:ub.length + ul.length, sev:ub.length + ul.length ? 'hint' : 'ok', d:ub.length + ul.length ? TC.n(ub.length) + ' block' + (ub.length ? ' (' + esc(ub.slice(0, 2).join(', ') + (ub.length > 2 ? '…' : '')) + ')' : '') + ', ' + TC.n(ul.length) + ' layer rỗng.' : 'Mọi block, layer đều đang dùng.', a:'Dọn dẹp…', cmd:'PURGE', pen:Math.min(4, ub.length + ul.length) },
      { id:'zero', t:'Đối tượng độ dài 0', n:zero.length, sev:zero.length ? 'warn' : 'ok', d:zero.length ? 'Không thấy được nhưng làm sai bắt điểm, đếm và trích xuất.' : 'Không có.', a:zero.length ? 'Xóa ' + TC.n(zero.length) + ' đối tượng' : '', pen:Math.min(4, zero.length * 2), list:zero },
      { id:'dup', t:'Đối tượng trùng', n:dup.length, sev:dup.length ? 'warn' : 'ok', d:dup.length ? 'Nét vẽ hai lần: in đậm hơn, đo tổng chiều dài sai.' : 'Không có.', a:'Xóa trùng lặp…', pen:Math.min(5, Math.ceil(dup.length / 2)), list:dup },
      { id:'proxy', t:'Đối tượng proxy', n:px ? px[0] : 0, sev:px ? 'warn' : 'ok', d:px ? esc(px[1]) + '. Hiện bằng hình đại diện, không sửa được.' : 'Không có.', a:px ? 'Phá thành hình thường' : '', pen:px ? 4 : 0 },
      { id:'xref', t:'Tham chiếu thiếu', n:xr.length + im.length, sev:xr.length + im.length ? 'err' : 'ok', d:xr.length + im.length ? esc(xr.map(function(r){ return r.name; }).concat(im.map(function(r){ return r.name; })).join(', ')) + ' · không tìm thấy tệp.' : 'Mọi xref, ảnh đều tìm thấy.', a:'Tham chiếu ngoài', pen:Math.min(12, (xr.length + im.length) * 6) },
      { id:'font', t:'Font bị thay thế', n:fs.length, sev:fs.length ? 'warn' : 'ok', d:fs.length ? esc(fs.map(function(s){ return s.font + ' → ' + (s.sub || 'Arial'); }).slice(0, 2).join(', ') + (fs.length > 2 ? '…' : '')) : 'Mọi font đều có trên máy.', a:'Bác sĩ Font', pen:Math.min(6, fs.length * 2) },
      { id:'audit', t:'Lỗi cấu trúc (AUDIT)', n:d.auditAt ? 0 : null, sev:d.auditAt ? 'ok' : 'hint', d:d.auditAt ? 'Đã kiểm tra lúc ' + d.auditAt + '.' : 'Chưa kiểm tra trong phiên này.', a:'Kiểm tra lỗi', cmd:'AUDIT', pen:0 },
      { id:'size', t:'Dung lượng có thể giảm', n:save, sev:save > 1048576 ? 'warn' : save > 102400 ? 'hint' : 'ok', d:'Tệp ' + TC.size(mb) + (save > 102400 ? ' → khoảng ' + TC.size(Math.max(mb - save, mb * .35)) + ' sau khi dọn.' : ', không giảm thêm được đáng kể.'), a:save > 102400 ? 'Dọn dẹp…' : '', cmd:'PURGE', pen:save > 1048576 ? 2 : 0, size:true }
    ];
    var score = Math.max(0, 100 - it.reduce(function(s, x){ return s + x.pen; }, 0));
    return { items:it, score:score, lv:score >= 80 ? ['ok', 'Tốt'] : score >= 50 ? ['warn', 'Cần dọn'] : ['err', 'Kém'], at:hhmm() };
  }
  function hlRender(){
    var el = GP.dlg.el, d = GP.d, h = GP.h = health(d), host = $('#gc-hl', el), bad = h.items.filter(function(x){ return x.sev !== 'ok'; }), R = 22, C = 2 * Math.PI * R;
    var ico = { ok:'okc', hint:'info', warn:'warn', err:'error' };
    host.innerHTML = '<div class="hh-top"><svg class="hh-ring s-' + h.lv[0] + '" viewBox="0 0 56 56" role="img" aria-label="Điểm sức khỏe ' + h.score + ' trên 100"><circle cx="28" cy="28" r="' + R + '" class="bg"/><circle cx="28" cy="28" r="' + R + '" class="fg" style="stroke-dasharray:' + (C * h.score / 100).toFixed(1) + ' ' + C.toFixed(1) + '"/><text x="28" y="33">' + h.score + '</text></svg>' +
      '<div class="hh-sum"><b>' + h.score + '/100 · ' + h.lv[1] + '</b><span>' + (bad.length ? TC.n(bad.length) + ' mục cần xem. Mỗi mục có nút sửa nhanh hoặc mở công cụ phù hợp.' : 'Bản vẽ sạch, không cần dọn.') + '</span><span class="faint rv-s">' + esc(d.name) + ' · ' + TC.size(d.bytes || 0) + ' · ' + esc(d.ver) + ' · lưu bởi ' + esc(d.savedBy || '') + '</span></div></div>' +
      '<div class="hh-list">' + h.items.map(function(x){ return '<div class="hh-row s-' + x.sev + '" data-h="' + x.id + '"><span class="hh-i">' + ic(ico[x.sev]) + '</span><div class="hh-t"><b>' + x.t + '</b><span>' + x.d + '</span></div><span class="hh-n mono">' + (x.n == null ? '—' : x.size ? (x.n > 102400 ? '≈ ' + TC.size(x.n) : '0') : TC.n(x.n)) + '</span>' + (x.a ? '<button class="btn sm" type="button" data-ha="' + x.id + '"' + (x.cmd || x.id === 'xref' || x.id === 'font' ? '' : (!pro() ? ' data-lock="1"' : '')) + '>' + x.a + (x.cmd || x.id === 'xref' || x.id === 'font' ? '' : lockTag()) + '</button>' : '<span></span>') + '</div>'; }).join('') + '</div>';
    $('.dlg-f', el).innerHTML = '<span class="faint rv-s">Kiểm tra lúc ' + h.at + '</span><span class="grow"></span><button class="btn" type="button" data-hk="rpt">' + ic('list') + 'Xuất báo cáo</button><button class="btn" type="button" data-hk="scan">' + ic('refresh') + 'Kiểm tra lại</button>';
  }
  function hlClick(e){
    var d = GP.d, b = e.target.closest('[data-ha]'), k = e.target.closest('[data-hk]'); if (k){ if (k.dataset.hk === 'scan'){ hlRender(); TC.toast('Đã kiểm tra lại sức khỏe bản vẽ: ' + GP.h.score + '/100.', { log:false }); } else hlReport(d); return; }
    if (!b) return; var x = GP.h.items.filter(function(i){ return i.id === b.dataset.ha; })[0]; if (!x) return;
    if (x.cmd){ TC.cmd.run(x.cmd); return; }
    if (x.id === 'xref'){ A().showPalette('xref'); return; } if (x.id === 'font'){ A().showPalette('fontdoc'); return; }
    if (!pro()){ SC.upgrade('Sửa nhanh sức khỏe bản vẽ', 'PRO'); return; }
    if (x.id === 'zero') TC.confirm({ title:'Xóa đối tượng độ dài 0', msg:'Xóa <b>' + TC.n(x.list.length) + '</b> đối tượng độ dài 0?', detail:x.list.map(function(i){ return esc(i.lay); }).join(', ') + '. Hoàn tác được bằng Ctrl+Z.', ok:'Xóa ' + TC.n(x.list.length) + ' đối tượng', danger:true }).then(function(y){ if (y){ ED.run('Sức khỏe bản vẽ: xóa độ dài 0', function(){ x.list.forEach(function(i){ fixOne(d, i); }); }); TC.toast('Đã xóa ' + TC.n(x.list.length) + ' đối tượng độ dài 0.', { log:false, act:{ label:'Hoàn tác', fn:function(){ ED.undo(1); } } }); } });
    else if (x.id === 'dup'){ if (d.space !== 'model') A().setSpace('model'); d.sel = []; x.list.forEach(function(i){ i.e.forEach(function(en){ if (d.sel.indexOf(en.id) < 0) d.sel.push(en.id); }); }); TC.cad.afterSel(); TC.cmd.run('OVERKILL'); }
    else if (x.id === 'proxy') TC.confirm({ title:'Phá đối tượng proxy', msg:'Phá <b>' + TC.n(x.n) + '</b> đối tượng proxy thành line, polyline thường?', detail:'Dữ liệu riêng của AutoCAD Architecture (chiều cao tường, kiểu tường) sẽ mất; hình vẽ giữ nguyên. Hoàn tác được bằng Ctrl+Z.', ok:'Phá ' + TC.n(x.n) + ' đối tượng' }).then(function(y){ if (!y) return; ED.run('Phá đối tượng proxy', function(){ var done = ED.state(function(){ return { g:!!d.proxyGone }; }, function(s){ d.proxyGone = s.g; if (GP.dlg && GP.tab === 'health') hlRender(); }); d.proxyGone = true; done(); }); TC.toast('Đã phá ' + TC.n(x.n) + ' đối tượng proxy thành hình thường.', { log:false, act:{ label:'Hoàn tác', fn:function(){ ED.undo(1); } } }); });
  }
  TC.on('cmdend', function(n){ if (n === 'AUDIT'){ var d = D(); if (d){ d.auditAt = hhmm(); if (GP.dlg && document.contains(GP.dlg.el) && GP.tab === 'health') hlRender(); } } });
  TC.on('dirty', function(d){ if (GP.dlg && document.contains(GP.dlg.el) && GP.tab === 'health' && d === GP.d) setTimeout(function(){ if (GP.dlg) hlRender(); }, 0); });

  /* ---------- báo cáo: xem trước trên giấy trắng, xuất PDF, CSV (dùng chung I08, I09) ---------- */
  // o: { title, cmd, code, file, head:[[nhãn, giá trị]], kpi:[[nhãn, số, lớp]], secs:[{ h, cols:[[tên, rộng, lớp]], rows:[[ô…]], note }] }
  function report(o){
    var d = D(), pages = Math.max(1, Math.ceil(o.secs.reduce(function(s, x){ return s + x.rows.length + 4; }, 10) / 42)), rows = o.secs.reduce(function(s, x){ return s + x.rows.length; }, 0);
    var dlg = TC.dialog({ id:'rpt', title:'Xem trước báo cáo', cmd:o.cmd, code:o.code, w:820, h:Math.min(760, innerHeight - 24), flush:true,
      body:'<div class="rpt"><article class="rpt-pg" aria-label="Trang báo cáo"><header><div><b>' + esc(o.title) + '</b><span>TrionCAD Pro · ' + TC.date() + ' ' + hhmm() + '</span></div><span class="rpt-logo">' + ic('logo') + 'TrionCAD</span></header>' +
        '<dl class="rpt-head">' + o.head.map(function(h){ return '<dt>' + esc(h[0]) + '</dt><dd>' + h[1] + '</dd>'; }).join('') + '</dl>' +
        (o.kpi ? '<div class="rpt-kpi">' + o.kpi.map(function(k){ return '<div><span>' + esc(k[0]) + '</span><b>' + k[1] + '</b></div>'; }).join('') + '</div>' : '') +
        o.secs.map(function(s){ return '<h4>' + esc(s.h) + '</h4>' + (s.rows.length ? '<table><colgroup>' + s.cols.map(function(c){ return '<col' + (c[1] ? ' style="width:' + c[1] + 'px"' : '') + '>'; }).join('') + '</colgroup><thead><tr>' + s.cols.map(function(c){ return '<th class="' + (c[2] || '') + '">' + esc(c[0]) + '</th>'; }).join('') + '</tr></thead><tbody>' + s.rows.map(function(r){ return '<tr>' + r.map(function(c, i){ return '<td class="' + (s.cols[i][2] || '') + '">' + c + '</td>'; }).join('') + '</tr>'; }).join('') + '</tbody></table>' : '<p class="rpt-none">Không có mục nào.</p>') + (s.note ? '<p class="rpt-note">' + s.note + '</p>' : ''); }).join('') +
        '<footer><span>' + esc(d ? d.path : '') + '</span><span>Trang 1/' + pages + '</span></footer></article></div>',
      foot:'<span class="faint rv-s">' + TC.n(rows) + ' dòng · khổ A4 đứng · ' + pages + ' trang</span><span class="grow"></span><button class="btn" type="button" id="rp-csv">' + ic('stats') + 'Xuất CSV</button><button class="btn primary" type="button" id="rp-pdf">' + ic('pdf') + 'Xuất PDF</button>' });
    $('#rp-csv', dlg.el).addEventListener('click', function(){ TC.toast('Đã xuất <span class="mono">' + esc(o.file) + '.csv</span>: ' + TC.n(rows) + ' dòng, mã UTF-8 mở được bằng Excel.', { log:false, act:{ label:'Mở thư mục', fn:function(){} } }); });
    $('#rp-pdf', dlg.el).addEventListener('click', function(){ dlg.close(true); TC.toast('Đã xuất <span class="mono">' + esc(o.file) + '.pdf</span>: ' + pages + ' trang.', { kind:'task', act:{ label:'Mở tệp', fn:function(){} } }); });
    return dlg;
  }
  function gcReport(d){
    var S = gcOf(d), open = S.list.filter(function(x){ return !x.ign; }), ign = S.list.filter(function(x){ return x.ign; }), fixed = S.log.filter(function(x){ return x.act === 'Đã sửa'; });
    report({ title:'Báo cáo làm sạch hình học', cmd:'TCCLEANUP', code:'UI-I08', file:'Lam-sach-hinh-hoc_' + fname(d),
      head:[['Bản vẽ', esc(d.name)], ['Phạm vi', S.scope === 'sel' ? TC.n(S.ids.length) + ' đối tượng đã chọn' : 'Model, ' + TC.n(gcEnts(d).length) + ' đối tượng được quét'], ['Layer', GC.lay === 'geo' ? 'Layer hình học (bỏ trục, kích thước, chữ, hatch, khung)' : GC.lay === 'all' ? 'Tất cả layer' : esc(GC.pick.join(', '))], ['Dung sai', GORD.filter(function(k){ return GC.on[k]; }).map(function(k){ return GK[k][0] + ' ' + TC.d(GC.tol[k], k === 'zero' || k === 'dup' ? 3 : 2) + (k === 'ang' ? '°' : ''); }).join(' · ')]],
      kpi:[['Còn lại', TC.n(open.length)], ['Đã sửa trong phiên', TC.n(fixed.length)], ['Bỏ qua', TC.n(ign.length)], ['Lần quét', S.at || '—']],
      secs:[{ h:'Vấn đề còn lại', cols:[['#', 30, 'num'], ['Loại', 110], ['Giá trị', 76, 'num'], ['Layer', 120], ['Vị trí X, Y', 130, 'num'], ['Cách sửa', 0]], rows:open.map(function(x){ return [x.n, GK[x.k][0], valText(x), esc(x.lay), TC.d(x.p[0], 0) + ', ' + TC.d(x.p[1], 0), howText(x)]; }) },
        { h:'Đã xử lý trong phiên', cols:[['Lúc', 46], ['Loại', 110], ['Giá trị', 76, 'num'], ['Layer', 120], ['Vị trí X, Y', 130, 'num'], ['Kết quả', 0]], rows:S.log.map(function(x){ return [x.at, GK[x.k][0], x.k === 'dup' ? 'trùng' : TC.d(x.v, 2), esc(x.lay), TC.d(x.p[0], 0) + ', ' + TC.d(x.p[1], 0), x.act]; }), note:'Mỗi lần sửa là một bước hoàn tác; Sửa tất cả là một bước.' }] });
  }
  function hlReport(d){
    var h = GP.h || health(d), lab = { ok:'Đạt', hint:'Nên xem', warn:'Cần dọn', err:'Cần sửa' };
    report({ title:'Báo cáo sức khỏe bản vẽ', cmd:'TCCLEANUP', code:'UI-I08', file:'Suc-khoe_' + fname(d), head:[['Bản vẽ', esc(d.name)], ['Phiên bản', esc(d.ver) + ' · lưu bởi ' + esc(d.savedBy || '')], ['Dung lượng', TC.size(d.bytes || 0)]],
      kpi:[['Điểm', h.score + '/100'], ['Đánh giá', h.lv[1]], ['Mục cần xem', TC.n(h.items.filter(function(x){ return x.sev !== 'ok'; }).length)]],
      secs:[{ h:'Các mục kiểm tra', cols:[['Mục', 170], ['Giá trị', 80, 'num'], ['Đánh giá', 76], ['Chi tiết', 0], ['Gợi ý xử lý', 110]], rows:h.items.map(function(x){ return [x.t, x.n == null ? '—' : x.size ? (x.n > 102400 ? '≈ ' + TC.size(x.n) : '0') : TC.n(x.n), lab[x.sev], x.d, x.a || '—']; }), note:'Điểm tính từ 100, trừ theo từng mục. Số regapps, proxy là dữ liệu mẫu của prototype.' }] });
  }

  IMPL.TCCLEANUP = function(ctx, pre){
    var d = D(), S = gcOf(d), health0 = pre === 'health' || /^H/i.test(String(pre || ''));
    if (d.sel.length && !health0){ S.ids = d.sel.slice(); S.scope = 'sel'; TC.cad.clearSel(); } else if (!health0 && S.scope === 'sel' && !S.ids) S.scope = 'all';
    gcOpen(health0 ? 'health' : 'geo');
    if (health0){ ctx.out('Sức khỏe bản vẽ ' + esc(d.name) + ': ' + GP.h.score + '/100 (' + GP.h.lv[1].toLowerCase() + ').'); return; }
    ctx.out('Quét ' + (S.scope === 'sel' ? TC.n(S.ids.length) + ' đối tượng đã chọn' : 'Model') + ': khe hở, đoạn thừa, đoạn hụt, góc lệch, đối tượng trùng, độ dài 0.');
    gcScanAnim(function(){ var S2 = gcOf(GP.d); TC.cmdline.msg('TCCLEANUP: ' + (S2.list.length ? TC.n(S2.list.filter(function(x){ return !x.ign; }).length) + ' vấn đề (' + sumText(S2.list) + ').' : 'không có vấn đề hình học.')); });
  };
  /* ================= Kiểm tra bản vẽ theo TCVN UI-I09 (TCCHECK) và chuẩn .dws (CHECKSTANDARDS) ================= */
  // Trích dẫn là dữ liệu mẫu: chỉ ghi số hiệu tiêu chuẩn có thật; điều khoản cụ thể chờ chuyên gia đối chiếu (báo cáo liệt kê)
  var REFS = { 'TCVN 5571:2012':'Bản vẽ xây dựng · khung tên', 'TCVN 7284-0:2003':'Chữ viết trên tài liệu kỹ thuật (ISO 3098-0)', 'TCVN 8-20:2002':'Bản vẽ kỹ thuật · nét vẽ (ISO 128-20)', 'TCVN 5570:2012':'Bản vẽ xây dựng · ký hiệu đường nét, đường trục',
    'TCVN 5705:1993':'Quy tắc ghi kích thước', 'TCVN 6909:2001':'Bộ mã ký tự tiếng Việt 16-bit (Unicode)', 'TCVN 5572:2012':'Bản vẽ kết cấu bê tông, bê tông cốt thép', 'TCVN 5574:2018':'Thiết kế kết cấu bê tông, bê tông cốt thép', 'QĐ-KT-03':'Quy định trình bày bản vẽ của công ty (mẫu)' };
  var SETS = { kt:['TCVN kiến trúc', 'TrionCAD'], kc:['TCVN kết cấu', 'TrionCAD'], cty:['Quy tắc công ty An Phú', 'Công ty'] };
  var TAGN = { DONVI:'Đơn vị thiết kế', CONGTRINH:'Công trình', TENBV:'Tên bản vẽ', KYHIEU:'Ký hiệu', TILE:'Tỉ lệ', NGAY:'Ngày', THIETKE:'Thiết kế', KIEMTRA:'Kiểm tra', CHUTRI:'Chủ trì', VE:'Người vẽ' };
  function list(s){ return String(s).split(/[\s,;]+/).filter(Boolean); }
  function wild(p){ return new RegExp('^' + p.replace(/[.+^${}()|[\]\\]/g, '\\$&').replace(/\*/g, '.*').replace(/\?/g, '.') + '$', 'i'); }
  function nearIn(ser, v){ return ser.reduce(function(b, x){ var db = Math.abs(b - v), dx = Math.abs(x - v); return dx < db - 1e-9 || (Math.abs(dx - db) < 1e-9 && x > b) ? x : b; }, ser[0]); }
  // khung tên: block KHUNG-*; tỉ lệ đọc từ ô TILE (1:100), không có thì theo hệ số chèn
  function frames(d){ return allEnts(d).filter(function(x){ return x.e.t === 'insert' && /^KHUNG/i.test(x.e.name); }).map(function(x){ var t = String(SC.att.attrsOf(d, x.e).TILE || ''), m = t.match(/1\s*:\s*(\d+)/); x.k = x.sp === 'model' ? (m ? +m[1] : Math.abs(x.e.sx) || 100) : 1; x.tile = m ? +m[1] : null; return x; }); }
  function scaleOf(d, p, fr){ var f = (fr || frames(d)).filter(function(x){ return x.sp === 'model' && x.e._bb && p[0] >= x.e._bb[0] && p[0] <= x.e._bb[2] && p[1] >= x.e._bb[1] && p[1] <= x.e._bb[3]; })[0]; return f ? f.k : (+String(d.annoScale || '1:100').split(':')[1] || 100); }
  // chữ cấp trên cùng kèm chiều cao khi in = chiều cao ÷ tỉ lệ (layout: 1)
  function texts(d){ var fr = frames(d); return allEnts(d).filter(function(x){ return (x.e.t === 'text' || x.e.t === 'mtext') && x.e.h; }).map(function(x){ x.k = x.sp === 'model' ? scaleOf(d, x.e.p, fr) : 1; x.ph = x.e.h / x.k; return x; }); }
  function onLayers(d, names){ return allEnts(d).filter(function(x){ return names.indexOf(x.e.layer) >= 0; }); }
  // thay đổi bảng layer, kiểu chữ, linetype, kiểu kích thước có hoàn tác: chụp cả bảng rồi dựng lại
  function tableOps(d, fn){
    var done = ED.state(function(){ return { L:d.layers.map(function(l){ var o = Object.assign({}, l); delete o.count; return o; }), S:d.styles.map(function(s){ var o = Object.assign({}, s); delete o.count; return o; }), lt:SC.ltList(d).slice(), dim:d.dimstyles.slice(), B:Object.keys(d.blocks).map(function(n){ return [n, d.blocks[n].ents.map(function(e){ return [e.layer, e.style || null]; })]; }) }; },
      function(s){ d.layers = s.L.map(function(l){ return Object.assign({}, l); }); d.layerMap = {}; d.layers.forEach(function(l){ d.layerMap[l.name] = l; }); d.styles = s.S.map(function(x){ return Object.assign({}, x); }); d.styleMap = {}; d.styles.forEach(function(x){ d.styleMap[x.name] = x; }); d.linetypes = s.lt.slice(); d.dimstyles = s.dim.slice(); s.B.forEach(function(b){ var bd = d.blocks[b[0]]; if (bd) bd.ents.forEach(function(e, i){ if (b[1][i]){ e.layer = b[1][i][0]; if (b[1][i][1]) e.style = b[1][i][1]; } }); }); TC.prepDoc(d); A().refreshPalettes(); TC.emit('layers'); });
    fn(); done(); TC.prepDoc(d);
  }
  function styleCount(d){ var c = {}; function w(list){ list.forEach(function(e){ if (e.style && (e.t === 'text' || e.t === 'mtext' || e.t === 'attdef')) c[e.style] = (c[e.style] || 0) + 1; }); } w(d.ents); d.layouts.forEach(function(L){ w(L.ents); }); Object.keys(d.blocks).forEach(function(n){ w(d.blocks[n].ents); }); d.styles.forEach(function(s){ s.count = c[s.name] || 0; }); }
  var RULES = [
    { id:'KT01', set:'kt', name:'Khung tên đủ ô bắt buộc', sev:'err', ref:'TCVN 5571:2012', why:'Khung tên là căn cứ nhận diện bản vẽ khi phát hành, lưu trữ. Ô bắt buộc phải có giá trị thật, không để trống hay giữ giá trị mẫu như [TÊN CÔNG TY].', p:{ fields:'DONVI, CONGTRINH, TENBV, KYHIEU, TILE, NGAY, THIETKE, KIEMTRA' }, pl:{ fields:'Ô bắt buộc (tag attribute)' },
      check:function(d, p){ var fr = frames(d), out = []; list(p.fields).forEach(function(tag){ var bad = fr.filter(function(f){ var v = SC.att.attrsOf(d, f.e)[tag]; return v != null && (!String(v).trim() || /^\[.*\]$/.test(String(v).trim())); }); if (!bad.length) return; var v0 = String(SC.att.attrsOf(d, bad[0].e)[tag]).trim(), pj = SC.project && SC.project[tag];
        out.push({ title:'Ô ' + (TAGN[tag] || tag) + ' ' + (v0 ? 'còn giá trị mẫu “' + v0 + '”' : 'để trống'), n:bad.length, unit:'khung tên', objs:bad, key:'KT01.' + tag, fix:pj ? { label:'Điền “' + pj + '”', desc:'Lấy từ thông tin dự án (Quản lý bộ bản vẽ).', run:function(){ bad.forEach(function(f){ ED.mod(f.e, function(x){ x.attrs = Object.assign({}, x.attrs || {}); x.attrs[tag] = pj; }); }); } } : null }); }); return out; } },
    { id:'KT02', set:'kt', name:'Chữ không nhỏ hơn cỡ tối thiểu khi in', sev:'err', ref:'TCVN 7284-0:2003', why:'Chữ quá nhỏ khó đọc trên bản in, bản chụp. Chiều cao khi in = chiều cao trong Model ÷ tỉ lệ của khung tên chứa chữ.', p:{ min:1.8 }, pl:{ min:'Chiều cao in tối thiểu (mm)' },
      check:function(d, p){ var s = texts(d).filter(function(t){ return t.ph < +p.min - 1e-6; }); if (!s.length) return []; var lo = Math.min.apply(null, s.map(function(t){ return t.ph; }));
        return [{ title:'Chữ cao dưới ' + TC.d(+p.min, 1) + ' mm khi in', sub:'Nhỏ nhất ' + TC.d(lo, 2) + ' mm', n:s.length, unit:'chữ', objs:s, key:'KT02', fix:{ label:'Đặt chiều cao in ' + TC.d(+p.min, 1) + ' mm', desc:'Chiều cao trong bản vẽ = ' + TC.d(+p.min, 1) + ' × tỉ lệ (1:100 → ' + TC.d(+p.min * 100, 0) + ').', run:function(){ s.forEach(function(t){ ED.mod(t.e, function(x){ x.h = +p.min * t.k; }); }); } } }]; } },
    { id:'KT03', set:'kt', name:'Chiều cao chữ theo dãy chuẩn', sev:'hint', ref:'TCVN 7284-0:2003', why:'Dùng cỡ chữ trong dãy chuẩn giúp bản vẽ đồng nhất và vẫn rõ khi in thu nhỏ khổ giấy.', p:{ series:'1.8, 2.5, 3.5, 5, 7, 10, 14, 20', min:1.8 }, pl:{ series:'Dãy chiều cao in (mm)', min:'Bỏ qua chữ nhỏ hơn (mm)' },
      check:function(d, p){ var ser = list(p.series).map(Number), g = {}, s = texts(d).filter(function(t){ return t.ph >= +p.min - 1e-6 && Math.abs(nearIn(ser, t.ph) - t.ph) > .05; }); if (!s.length) return []; s.forEach(function(t){ var k = TC.d(t.ph, 1); (g[k] = g[k] || []).push(t); });
        var keys = Object.keys(g).sort(function(a, b){ return a - b; });
        return [{ title:'Chiều cao chữ ngoài dãy chuẩn', sub:keys.map(function(k){ return k + ' mm'; }).join(', '), n:s.length, unit:'chữ', objs:s, key:'KT03', rows:keys.map(function(k){ return [k + ' mm · ' + TC.n(g[k].length) + ' chữ', '→ ' + TC.d(nearIn(ser, +k), 1) + ' mm']; }), fix:{ label:'Làm tròn về dãy chuẩn', desc:'Mỗi chữ đổi sang cỡ gần nhất trong dãy (bằng nhau thì lấy cỡ lớn hơn).', run:function(){ s.forEach(function(t){ ED.mod(t.e, function(x){ x.h = nearIn(ser, t.ph) * t.k; }); }); } } }]; } },
    { id:'KT04', set:'kt', name:'Độ dày nét theo dãy chuẩn', sev:'warn', ref:'TCVN 8-20:2002', why:'Độ dày nét lấy theo dãy chuẩn để bản in thống nhất giữa các máy in và các bộ môn.', p:{ series:'0.13, 0.18, 0.25, 0.35, 0.5, 0.7, 1, 1.4, 2' }, pl:{ series:'Dãy độ dày nét (mm)' },
      check:function(d, p){ var ser = list(p.series).map(Number), bad = d.layers.filter(function(l){ return l.lw != null && !ser.some(function(x){ return Math.abs(x - l.lw) < 1e-6; }); }); if (!bad.length) return [];
        return [{ title:'Layer có độ dày nét ngoài dãy chuẩn', sub:bad.map(function(l){ return l.name + ' ' + TC.d(l.lw); }).join(', '), n:bad.length, unit:'layer', objs:onLayers(d, bad.map(function(l){ return l.name; })), key:'KT04', rows:bad.map(function(l){ return [esc(l.name) + ' · ' + TC.d(l.lw) + ' mm', '→ ' + TC.d(nearIn(ser, l.lw)) + ' mm']; }), fix:{ label:'Đổi về độ dày gần nhất', run:function(){ tableOps(d, function(){ bad.forEach(function(l){ d.layerMap[l.name].lw = nearIn(ser, l.lw); }); }); } } }]; } },
    { id:'KT05', set:'kt', name:'Nét cơ bản đậm hơn nét mảnh', sev:'hint', ref:'TCVN 8-20:2002', why:'Nét cơ bản (tường cắt, cột) cần đậm rõ hơn nét mảnh (kích thước, trục, hatch) để đọc nhanh hình chính.', p:{ thick:'KT-TUONG, KT-COT, KT-MATCAT', thin:'KT-DIM, KT-TRUC, KT-HATCH-TUONG', ratio:2 }, pl:{ thick:'Layer nét cơ bản', thin:'Layer nét mảnh', ratio:'Tỉ lệ tối thiểu' },
      check:function(d, p){ var lw = function(n){ var l = d.layerMap[n]; return l ? l.lw : null; }, tk = list(p.thick).map(lw).filter(function(x){ return x != null; }), tn = list(p.thin).map(lw).filter(function(x){ return x != null; }); if (!tk.length || !tn.length) return []; var a = Math.min.apply(null, tk), b = Math.max.apply(null, tn);
        return a + 1e-9 < +p.ratio * b ? [{ title:'Nét cơ bản ' + TC.d(a) + ' mm chưa gấp ' + p.ratio + ' lần nét mảnh ' + TC.d(b) + ' mm', n:0, key:'KT05' }] : []; } },
    { id:'KT06', set:'kt', name:'Trục định vị dùng nét chấm gạch', sev:'warn', ref:'TCVN 5570:2012', why:'Trục định vị vẽ bằng nét chấm gạch mảnh để phân biệt với nét hình.', p:{ layers:'*TRUC*', lt:'CENTER, TCVN-TRUC' }, pl:{ layers:'Layer trục (cho phép * ?)', lt:'Linetype hợp lệ' },
      check:function(d, p){ var re = list(p.layers).map(wild), ok = list(p.lt), bad = d.layers.filter(function(l){ return re.some(function(r){ return r.test(l.name); }) && ok.indexOf(l.lt) < 0; }); if (!bad.length) return [];
        return [{ title:'Layer trục không dùng nét chấm gạch', sub:bad.map(function(l){ return l.name + ' (' + l.lt + ')'; }).join(', '), n:bad.length, unit:'layer', objs:onLayers(d, bad.map(function(l){ return l.name; })), key:'KT06', fix:{ label:'Đặt linetype ' + ok[0], run:function(){ tableOps(d, function(){ bad.forEach(function(l){ d.layerMap[l.name].lt = ok[0]; }); }); } } }]; } },
    { id:'KT07', set:'kt', name:'Kích thước dùng kiểu theo tỉ lệ', sev:'warn', ref:'TCVN 5705:1993', why:'Kích thước dùng kiểu ứng với tỉ lệ bản vẽ (TCVN-1-100 cho 1:100) để chữ, mũi tên đúng cỡ khi in.', p:{ pattern:'TCVN-1-{tỉ lệ}' }, pl:{ pattern:'Mẫu tên kiểu kích thước' },
      check:function(d, p){ var fr = frames(d), bad = d.ents.filter(function(e){ return /^dim/.test(e.t) && e.style !== p.pattern.replace('{tỉ lệ}', scaleOf(d, e.p1 || e.c, fr)); }).map(function(e){ return { e:e, sp:'model', want:p.pattern.replace('{tỉ lệ}', scaleOf(d, e.p1 || e.c, fr)) }; }); if (!bad.length) return [];
        var names = bad.map(function(x){ return x.e.style; }).filter(function(n, i, a){ return a.indexOf(n) === i; });
        return [{ title:'Kích thước dùng kiểu ' + names.join(', ') + ' thay vì ' + bad[0].want, n:bad.length, unit:'kích thước', objs:bad, key:'KT07', fix:{ label:'Đổi sang ' + bad[0].want, run:function(){ bad.forEach(function(x){ if (d.dimstyles.indexOf(x.want) < 0) d.dimstyles.push(x.want); ED.mod(x.e, function(z){ z.style = x.want; }); }); } } }]; } },
    { id:'KT08', set:'kt', name:'Kiểu chữ dùng font Unicode', sev:'warn', ref:'TCVN 6909:2001', why:'Font mã TCVN3, VNI chỉ hiện đúng khi máy có đúng font đó. Chữ Unicode hiện đúng ở mọi máy và mọi phần mềm.', p:{},
      check:function(d){ var L = d.styles.filter(function(s){ return (s.enc === 'TCVN3' || s.enc === 'VNI') && s.count; }); if (!L.length) return []; var names = L.map(function(s){ return s.name; });
        return [{ title:'Kiểu chữ dùng font mã cũ: ' + names.join(', '), n:L.reduce(function(s, x){ return s + x.count; }, 0), unit:'chữ', objs:allEnts(d).filter(function(x){ return names.indexOf(x.e.style) >= 0 && /text/.test(x.e.t); }), key:'KT08', rows:L.map(function(s){ return [esc(s.name) + ' · ' + esc(s.font), s.enc + ' · ' + TC.n(s.count) + ' chữ']; }), act:{ label:'Chuyển bằng Bác sĩ Font…', run:function(){ SC.fontConvert(d); } } }]; } },
    { id:'KT09', set:'kt', name:'Ký hiệu bản vẽ không trùng', sev:'err', ref:'TCVN 5571:2012', why:'Mỗi tờ cần một ký hiệu riêng để tra cứu, in theo bộ và đối chiếu khi sửa đổi.', p:{},
      check:function(d){ var seen = {}, out = []; frames(d).forEach(function(f){ var k = SC.att.attrsOf(d, f.e).KYHIEU; if (!k) return; (seen[k] = seen[k] || []).push(f); }); Object.keys(seen).forEach(function(k){ if (seen[k].length > 1) out.push({ title:'Ký hiệu ' + k + ' dùng ở ' + seen[k].length + ' khung tên', n:seen[k].length, unit:'khung tên', objs:seen[k], key:'KT09.' + k }); }); return out; } },
    { id:'KT10', set:'kt', name:'Tỉ lệ khung tên khớp tỉ lệ viewport', sev:'err', ref:null, why:'Tỉ lệ ghi trong khung tên phải khớp tỉ lệ viewport để người đọc đo đúng trên bản in. Kiểm tra bổ trợ của TrionCAD, không gắn điều khoản TCVN.', p:{},
      check:function(d){ var out = []; frames(d).filter(function(f){ return f.sp !== 'model' && f.tile; }).forEach(function(f){ var L = d.layouts[f.sp], bad = L.vports.filter(function(v){ return Math.round(v.scale) !== f.tile; }); if (bad.length) out.push({ title:'Layout ' + L.name + ': khung tên 1:' + f.tile + ', viewport 1:' + bad.map(function(v){ return Math.round(v.scale); }).join(', 1:'), n:bad.length, unit:'viewport', objs:[f], key:'KT10.' + L.name }); }); return out; } },
    { id:'KC01', set:'kc', name:'Ghi chú cốt thép đúng mẫu', sev:'warn', ref:'TCVN 5572:2012', why:'Ghi chú cốt thép theo một mẫu thống nhất (Ø8a200, 2Ø16) để đọc dễ và Thống kê thép đọc được tự động.', p:{ layers:'KC-*' }, pl:{ layers:'Layer ghi chú thép' },
      check:function(d, p){ var re = list(p.layers).map(wild), s = texts(d).filter(function(t){ return re.some(function(r){ return r.test(t.e.layer); }) && /[Øø∅Φφ]|\bphi\b|\bd\d+\s*@/i.test(t.e.s) && !/^(\d+\s*)?Ø\d+(\s*a\s*\d+)?$/.test(String(t.e.s).trim()); }); return s.length ? [{ title:'Ghi chú thép không theo mẫu Ø8a200', n:s.length, unit:'chữ', objs:s, key:'KC01' }] : []; } },
    { id:'KC02', set:'kc', name:'Bê tông ghi theo cấp độ bền B', sev:'hint', ref:'TCVN 5574:2018', why:'Bộ quy tắc yêu cầu ghi cấp độ bền bê tông theo ký hiệu B (B20, B25), không dùng mác M cũ.', p:{},
      check:function(d){ var s = texts(d).filter(function(t){ return /\bM\s?\d{3}\b/.test(t.e.s); }); return s.length ? [{ title:'Ghi mác bê tông M thay vì cấp độ bền B', n:s.length, unit:'chữ', objs:s, key:'KC02' }] : []; } },
    { id:'KC03', set:'kc', name:'Ký hiệu thép đủ lớn khi in', sev:'warn', ref:'TCVN 5572:2012', why:'Ký hiệu cốt thép cần đủ lớn để đọc rõ trên bản in công trường.', p:{ min:2.5 }, pl:{ min:'Chiều cao in tối thiểu (mm)' },
      check:function(d, p){ var s = texts(d).filter(function(t){ return /Ø/.test(t.e.s) && t.ph < +p.min - 1e-6; }); return s.length ? [{ title:'Ký hiệu thép cao dưới ' + TC.d(+p.min, 1) + ' mm khi in', n:s.length, unit:'chữ', objs:s, key:'KC03', fix:{ label:'Đặt chiều cao in ' + TC.d(+p.min, 1) + ' mm', run:function(){ s.forEach(function(t){ ED.mod(t.e, function(x){ x.h = +p.min * t.k; }); }); } } }] : []; } },
    { id:'CT01', set:'cty', name:'Tên layer theo mẫu công ty', sev:'err', ref:'QĐ-KT-03', why:'Mẫu tên layer: <bộ môn>-<nội dung>, chữ hoa không dấu (KT-TUONG). Layer lạ thường đến từ tệp đối tác, tệp khảo sát.', p:{ pattern:'^(KT|KC|DN|CN|ME)-[A-Z0-9-]+$', exempt:'0, Defpoints' }, pl:{ pattern:'Mẫu tên (biểu thức chính quy)', exempt:'Layer miễn kiểm' },
      check:function(d, p){ var re; try { re = new RegExp(p.pattern); } catch (e){ return [{ title:'Mẫu tên layer không hợp lệ: ' + p.pattern, n:0, key:'CT01.x' }]; } var ex = list(p.exempt), bad = d.layers.filter(function(l){ return ex.indexOf(l.name) < 0 && !re.test(l.name); }); if (!bad.length) return []; var names = bad.map(function(l){ return l.name; });
        return [{ title:'Layer đặt tên sai mẫu: ' + names.join(', '), n:bad.length, unit:'layer', objs:onLayers(d, names), key:'CT01', act:{ label:'Dịch layer theo chuẩn…', run:function(){ SC.layTrans(names); } } }]; } },
    { id:'CT02', set:'cty', name:'Không vẽ trực tiếp trên layer 0', sev:'warn', ref:'QĐ-KT-03', why:'Layer 0 chỉ dùng trong block. Đối tượng vẽ thẳng trên layer 0 không theo được quy định màu, nét khi in.', p:{ to:'KT-GHICHU' }, pl:{ to:'Layer chuyển tới khi sửa' },
      check:function(d, p){ var s = d.ents.filter(function(e){ return e.layer === '0'; }).map(function(e){ return { e:e, sp:'model' }; }); if (!s.length) return []; var opts = d.layers.map(function(l){ return l.name; }).filter(function(n){ return n !== '0' && n !== 'Defpoints'; });
        var f = { title:'Đối tượng vẽ trên layer 0', n:s.length, unit:'đối tượng', objs:s, key:'CT02', opt:{ label:'Chuyển sang layer', list:opts, val:opts.indexOf(p.to) >= 0 ? p.to : opts[0] } };
        f.fix = { label:'Chuyển sang layer đã chọn', run:function(){ s.forEach(function(x){ ED.mod(x.e, function(z){ z.layer = f.opt.val; }); }); } }; return [f]; } },
    { id:'CT03', set:'cty', name:'Màu, nét theo layer (ByLayer)', sev:'hint', ref:'QĐ-KT-03', why:'Màu, linetype, độ dày nét nên lấy theo layer để đổi đồng loạt và in đúng plot style. Màu riêng đôi khi là chủ ý (cột tô xám): xem trước khi sửa.', p:{},
      check:function(d){ var s = allEnts(d).filter(function(x){ return x.e.t !== 'vpframe' && (x.e.color != null || x.e.lt || x.e.lw != null); }); return s.length ? [{ title:'Đối tượng đặt màu, nét riêng', n:s.length, unit:'đối tượng', objs:s, key:'CT03', fix:{ label:'Đặt về ByLayer', run:function(){ s.forEach(function(x){ ED.mod(x.e, function(z){ delete z.color; delete z.lt; delete z.lw; }); }); } } }] : []; } },
    { id:'CT04', set:'cty', name:'Không để đối tượng ngoài khung in', sev:'hint', ref:'QĐ-KT-03', why:'Đối tượng nằm ngoài khung in dễ bị bỏ sót hoặc in nhầm. Đưa vào khung, chuyển sang bản vẽ khác hoặc xóa.', p:{},
      check:function(d){ var fr = frames(d).filter(function(f){ return f.sp === 'model'; }).map(function(f){ return f.e._bb; }); if (!fr.length) return []; var s = d.ents.filter(function(e){ var b = e._bb; return b && e.t !== 'xline' && e.t !== 'ray' && !/^KHUNG/.test(e.name || '') && !fr.some(function(f){ return b[0] >= f[0] && b[1] >= f[1] && b[2] <= f[2] && b[3] <= f[3]; }); }).map(function(e){ return { e:e, sp:'model' }; });
        return s.length ? [{ title:'Đối tượng nằm ngoài mọi khung in', n:s.length, unit:'đối tượng', objs:s, key:'CT04', act:{ label:'Chọn ' + TC.n(s.length) + ' đối tượng', sel:true, run:function(){ if (d.space !== 'model') A().setSpace('model'); d.sel = s.map(function(x){ return x.e.id; }); TC.cad.afterSel(); } } }] : []; } },
    { id:'CT05', set:'cty', name:'Khung tên dùng block của công ty', sev:'warn', ref:'QĐ-KT-03', why:'Khung tên dùng block chuẩn của công ty để điền tự động và in hàng loạt theo khung tên.', p:{ names:'KHUNG-A1, KHUNG-A2, KHUNG-A3' }, pl:{ names:'Tên block khung tên hợp lệ' },
      check:function(d, p){ var ok = list(p.names), s = frames(d).filter(function(f){ return ok.indexOf(f.e.name) < 0; }); return s.length ? [{ title:'Khung tên dùng block lạ', n:s.length, unit:'khung tên', objs:s, key:'CT05' }] : []; } }
  ];
  // trạng thái quy tắc theo người dùng: bật/tắt bộ, bật/tắt từng quy tắc, mức độ, tham số (quy tắc công ty)
  var CKS = TC.load('ck', { sets:{ kt:true, kc:false, cty:true }, rules:{} });
  function rule(r){ var o = CKS.rules[r.id] || {}; return { r:r, on:o.on !== false, sev:o.sev || r.sev, p:Object.assign({}, r.p, o.p || {}) }; }
  function ckOf(d){ return d.ck || (d.ck = { list:[], pass:[], ign:{}, notes:{}, log:[], sel:null, hot:null, filter:'all', showIgn:false, at:null, oi:0 }); }
  function runCheck(d){
    styleCount(d); var S = ckOf(d), out = [], pass = [];
    RULES.forEach(function(r0){ if (!CKS.sets[r0.set]) return; var R = rule(r0); if (!R.on) return; var f = r0.check(d, R.p); if (!f.length) pass.push(R); f.forEach(function(x){ x.R = R; x.sev = R.sev; x.ign = !!S.ign[x.key]; out.push(x); }); });
    var so = { err:0, warn:1, hint:2 }; out.sort(function(a, b){ return so[a.sev] - so[b.sev] || RULES.indexOf(a.R.r) - RULES.indexOf(b.R.r); });
    S.list = out; S.pass = pass; S.at = hhmm(); return out;
  }

  /* ---------- chuẩn .dws: so layer, kiểu chữ, linetype, kiểu kích thước với tệp chuẩn ---------- */
  var DWS = {
    'AnPhu-KT.dws':{ path:'\\\\SRV-01\\Chung\\Chuan\\AnPhu-KT.dws', date:'02/09/2026', by:'Phòng Kỹ thuật', desc:'Chuẩn của công ty cho kiến trúc, kết cấu: layer, kiểu chữ Unicode, kiểu kích thước TCVN.',
      layers:[['0', 7, 'Continuous', .25], ['Defpoints', 7, 'Continuous', .25], ['KT-TRUC', 1, 'CENTER', .18], ['KT-TUONG', 7, 'Continuous', .5], ['KT-HATCH-TUONG', 8, 'Continuous', .13], ['KT-COT', 7, 'Continuous', .5], ['KT-CUA', 2, 'Continuous', .25], ['KT-CUASO', 4, 'Continuous', .25], ['KT-NOITHAT', 8, 'Continuous', .13], ['KT-TBVS', 140, 'Continuous', .18], ['KT-THANG', 8, 'Continuous', .25], ['KT-DIM', 3, 'Continuous', .18], ['KT-TEXT', 7, 'Continuous', .25], ['KT-KYHIEU', 2, 'Continuous', .25], ['KT-GHICHU', 7, 'Continuous', .25], ['KT-MATCAT', 7, 'Continuous', .5], ['KT-KHUNG', 7, 'Continuous', .5], ['KT-VIEWPORT', 8, 'Continuous', .13],
        ['KT-HIENTRANG', 8, 'DASHED', .18, 'Công trình hiện trạng, phá dỡ'], ['KT-RANHDAT', 1, 'TCVN-RANH', .35, 'Ranh giới khu đất'], ['KT-SANVUON', 3, 'Continuous', .18, 'Sân vườn, cây xanh'], ['KT-DUONG', 8, 'Continuous', .25, 'Đường, vỉa hè'], ['KC-COT', 7, 'Continuous', .5], ['KC-DAM', 6, 'Continuous', .35], ['KC-THEP', 1, 'Continuous', .25], ['KC-DIM', 3, 'Continuous', .18], ['KC-TEXT', 7, 'Continuous', .25]],
      dims:['Standard', 'TCVN-1-100', 'TCVN-1-50', 'TCVN-1-20'], lts:['Continuous', 'CENTER', 'DASHED', 'TCVN-TRUC', 'TCVN-RANH'], styles:[['Standard', 'Arial', 'TTF'], ['TCVN-Times', 'TrionVN-Times.shx', 'SHX', '"Times New Roman", Times, serif'], ['TCVN-Arial', 'TrionVN-Arial.shx', 'SHX']] },
    'TrionCAD-TCVN.dws':{ path:'C:\\TrionCAD\\Chuan\\TrionCAD-TCVN.dws', date:'15/08/2026', by:'TrionCAD', desc:'Chuẩn mẫu đi kèm TrionCAD: nét, kiểu chữ, kiểu kích thước theo TCVN; layer theo bộ môn.',
      layers:[['0', 7, 'Continuous', .25], ['Defpoints', 7, 'Continuous', .25], ['KT-TRUC', 1, 'TCVN-TRUC', .18], ['KT-TUONG', 7, 'Continuous', .5], ['KT-COT', 7, 'Continuous', .5], ['KT-CUA', 2, 'Continuous', .25], ['KT-DIM', 3, 'Continuous', .18], ['KT-TEXT', 7, 'Continuous', .25], ['KT-KHUNG', 7, 'Continuous', .5]],
      dims:['Standard', 'TCVN-1-100', 'TCVN-1-50', 'TCVN-1-20', 'TCVN-1-200'], lts:['Continuous', 'CENTER', 'DASHED', 'HIDDEN', 'TCVN-TRUC', 'TCVN-KHUAT', 'TCVN-CAT', 'TCVN-RANH'], styles:[['Standard', 'Arial', 'TTF'], ['TCVN-Times', 'TrionVN-Times.shx', 'SHX', '"Times New Roman", Times, serif'], ['TCVN-Arial', 'TrionVN-Arial.shx', 'SHX']] }
  };
  var PLUG = { lay:['Layer', 'Tên và thuộc tính layer: màu, linetype, độ dày nét.', 'layers'], dim:['Kiểu kích thước', 'Tên kiểu kích thước.', 'dim'], lt:['Linetype', 'Tên linetype.', 'line'], sty:['Kiểu chữ', 'Tên kiểu chữ và font; đổi kiểu thì Bác sĩ Font chuyển mã chữ cũ sang Unicode.', 'mtext'] };
  function stdOf(d){ return d.std || (d.std = { files:[], plug:{ lay:true, dim:true, lt:true, sty:true }, notify:'status', auto:false, showIgn:false, ign:{}, log:[], prefer:null }); }
  function dwsMerged(d){ var S = stdOf(d), m = { layers:{}, dims:[], lts:[], styles:{} }; S.files.slice().reverse().forEach(function(f){ var w = DWS[f]; if (!w) return; w.layers.forEach(function(l){ m.layers[l[0]] = { name:l[0], color:l[1], lt:l[2], lw:l[3], desc:l[4] || '', src:f }; }); w.dims.forEach(function(x){ if (m.dims.indexOf(x) < 0) m.dims.push(x); }); w.lts.forEach(function(x){ if (m.lts.indexOf(x) < 0) m.lts.push(x); }); w.styles.forEach(function(s){ m.styles[s[0]] = { name:s[0], font:s[1], kind:s[2], css:s[3], src:f }; }); }); return m; }
  function tokens(n){ return TC.fold(n).split(/[^a-z0-9]+/).filter(function(t){ return t.length > 2 && !/^(layer|lop)\d*$/.test(t); }); }
  function guess(name, cands){ var tk = tokens(name), best = null, bs = 0; cands.forEach(function(c){ var f = TC.fold(c).replace(/[^a-z0-9]/g, ''), s = tk.filter(function(t){ return f.indexOf(t) >= 0; }).length; if (s > bs){ bs = s; best = c; } }); return best; }
  function dwsProblems(d){
    var S = stdOf(d), M = dwsMerged(d), out = [], P = S.plug; if (!S.files.length) return out;
    var lnames = Object.keys(M.layers), ents = allEnts(d), cnt = function(fn){ return ents.filter(function(x){ return fn(x.e); }); };
    if (P.lay) d.layers.forEach(function(l){ var w = M.layers[l.name], objs = cnt(function(e){ return e.layer === l.name; });
      if (!w) out.push({ k:'lay-miss', kind:'lay', name:l.name, title:'Layer “' + l.name + '” không có trong tệp chuẩn', n:objs.length, unit:'đối tượng', objs:objs, opts:lnames.filter(function(n){ return n !== '0' && n !== 'Defpoints'; }), rec:guess(l.name, lnames), key:'L:' + l.name });
      else { var diff = ['color', 'lt', 'lw'].filter(function(k){ return k === 'lw' ? Math.abs((l.lw || 0) - w.lw) > 1e-6 : l[k] !== w[k]; }); if (diff.length) out.push({ k:'lay-prop', kind:'lay', name:l.name, title:'Thuộc tính layer “' + l.name + '” khác chuẩn: ' + diff.map(function(k){ return { color:'màu', lt:'linetype', lw:'độ dày nét' }[k]; }).join(', '), n:objs.length, unit:'đối tượng', objs:objs, opts:[l.name], rec:l.name, key:'P:' + l.name }); } });
    if (P.sty){ var sn = Object.keys(M.styles); d.styles.forEach(function(s){ if (M.styles[s.name]) return; out.push({ k:'sty-miss', kind:'sty', name:s.name, title:'Kiểu chữ “' + s.name + '” không có trong tệp chuẩn', n:s.count || 0, unit:'chữ', objs:cnt(function(e){ return e.style === s.name && /text/.test(e.t); }), opts:sn, rec:/time/i.test(s.font + s.css) ? 'TCVN-Times' : sn.indexOf('TCVN-Arial') >= 0 ? 'TCVN-Arial' : sn[0], key:'S:' + s.name }); }); }
    if (P.lt) SC.ltList(d).forEach(function(n){ if (M.lts.indexOf(n) >= 0 || /^By/.test(n)) return; var ly = d.layers.filter(function(l){ return l.lt === n; }); out.push({ k:'lt-miss', kind:'lt', name:n, title:'Linetype “' + n + '” không có trong tệp chuẩn', n:ly.length, unit:'layer dùng', objs:cnt(function(e){ return e.lt === n; }), opts:M.lts, rec:{ HIDDEN:'DASHED', DASHED2:'DASHED', CENTER2:'CENTER' }[n] || null, key:'T:' + n, lays:ly }); });
    if (P.dim) d.dimstyles.forEach(function(n){ if (M.dims.indexOf(n) >= 0) return; var objs = cnt(function(e){ return /^dim/.test(e.t) && e.style === n; }); out.push({ k:'dim-miss', kind:'dim', name:n, title:'Kiểu kích thước “' + n + '” không có trong tệp chuẩn', n:objs.length, unit:'kích thước', objs:objs, opts:M.dims, rec:M.dims.indexOf('TCVN-1-100') >= 0 ? 'TCVN-1-100' : M.dims[0], key:'D:' + n }); });
    out.forEach(function(x){ x.ign = !!S.ign[x.key]; x.pick = x.pick || x.rec; x.sev = x.k === 'lay-prop' ? 'hint' : 'warn'; });
    return out;
  }
  // xem trước thay đổi: thuộc tính | hiện tại | sau khi sửa (ô khác nhau tô nhấn)
  function dwsRows(d, x){
    var M = dwsMerged(d), R = x.pick, cl = SC.colorLabel;
    if (x.kind === 'lay'){ var l = d.layerMap[x.name] || {}, w = M.layers[R] || {}; return [['Tên', x.name, R || '—'], ['Màu', SC.colorSw(l.color) + cl(l.color), R ? SC.colorSw(w.color) + cl(w.color) : '—'], ['Linetype', l.lt, R ? w.lt : '—'], ['Độ dày nét', TC.d(l.lw || 0) + ' mm', R ? TC.d(w.lw) + ' mm' : '—'], ['Đối tượng', TC.n(x.n), x.k === 'lay-miss' ? (R ? 'chuyển sang ' + esc(R) : '—') : 'giữ layer']].concat(x.k === 'lay-miss' ? [['Layer cũ', 'có', R ? 'xóa sau khi chuyển' : '—']] : []); }
    if (x.kind === 'sty'){ var s = d.styleMap[x.name] || {}, t = M.styles[R] || {}; return [['Tên', x.name, R || '—'], ['Font', esc(s.font || ''), R ? esc(t.font) : '—'], ['Bảng mã', esc(s.enc || 'Unicode'), R ? 'Unicode' + (s.enc === 'TCVN3' || s.enc === 'VNI' ? ' (Bác sĩ Font chuyển mã)' : '') : '—'], ['Chữ dùng kiểu này', TC.n(x.n), R ? 'đổi sang ' + esc(R) : '—']]; }
    if (x.kind === 'lt') return [['Tên', x.name, R || '—'], ['Mẫu nét', esc((SC.LTS[x.name] || [x.name])[0]), R ? esc((SC.LTS[R] || [R])[0]) : '—'], ['Layer dùng', TC.n(x.lays.length) + (x.lays.length ? ' (' + esc(x.lays.map(function(l){ return l.name; }).join(', ')) + ')' : ''), R ? 'đổi sang ' + esc(R) : '—'], ['Đối tượng dùng riêng', TC.n(x.objs.length), R && x.objs.length ? 'đổi sang ' + esc(R) : '—']];
    return [['Tên', x.name, R || '—'], ['Kích thước dùng kiểu này', TC.n(x.n), R ? 'đổi sang ' + esc(R) : '—']];
  }
  function dwsFix(d, x){
    var M = dwsMerged(d), R = x.pick; if (!R) return false;
    var add = function(){ if (x.kind === 'lay' && !d.layerMap[R]){ var w = M.layers[R]; addLayer(d, [w.name, w.color, w.lt, w.lw, w.desc]); } if (x.kind === 'sty' && !d.styleMap[R]){ var t = M.styles[R], o = { name:R, font:t.font, kind:t.kind, enc:'Unicode', css:t.css, h:0 }; d.styles.push(o); d.styleMap[R] = o; } if (x.kind === 'lt' && SC.ltList(d).indexOf(R) < 0) d.linetypes.push(R); if (x.kind === 'dim' && d.dimstyles.indexOf(R) < 0) d.dimstyles.push(R); };
    if (x.k === 'lay-prop'){ var w = M.layers[R]; tableOps(d, function(){ Object.assign(d.layerMap[R], { color:w.color, lt:w.lt, lw:w.lw }); }); return true; }
    tableOps(d, function(){ add();
      Object.keys(d.blocks).forEach(function(n){ d.blocks[n].ents.forEach(function(e){ if (x.kind === 'lay' && e.layer === x.name) e.layer = R; if (x.kind === 'sty' && e.style === x.name) e.style = R; }); });
      if (x.kind === 'lay'){ d.layers = d.layers.filter(function(l){ return l.name !== x.name; }); delete d.layerMap[x.name]; if (d.clayer === x.name) d.clayer = R; }
      if (x.kind === 'sty'){ d.styles = d.styles.filter(function(s){ return s.name !== x.name; }); delete d.styleMap[x.name]; }
      if (x.kind === 'lt'){ d.layers.forEach(function(l){ if (l.lt === x.name) l.lt = R; }); d.linetypes = SC.ltList(d).filter(function(n){ return n !== x.name; }); }
      if (x.kind === 'dim') d.dimstyles = d.dimstyles.filter(function(n){ return n !== x.name; }); });
    allEnts(d).forEach(function(o){ var e = o.e; if (x.kind === 'lay' && e.layer === x.name) ED.mod(e, function(z){ z.layer = R; }); if (x.kind === 'sty' && e.style === x.name && /text/.test(e.t)) ED.mod(e, function(z){ z.style = R; }); if (x.kind === 'lt' && e.lt === x.name) ED.mod(e, function(z){ z.lt = R; }); if (x.kind === 'dim' && /^dim/.test(e.t) && e.style === x.name) ED.mod(e, function(z){ z.style = R; }); });
    return true;
  }

  /* ---------- panel kiểm tra: một khung cho TCVN (quy tắc) và chuẩn .dws (đi qua từng vấn đề) ---------- */
  var CK = { dlg:null, d:null, mode:'tcvn', busy:null, note:null };
  function cur(d){ return CK.mode === 'tcvn' ? ckOf(d) : stdOf(d); }
  function ckItems(d){ var S = ckOf(d), T = stdOf(d); if (CK.mode === 'tcvn') return S.list.filter(function(x){ return (S.showIgn || !x.ign) && (S.filter === 'all' || x.sev === S.filter); }); return (T.list || []).filter(function(x){ return (T.showIgn || !x.ign) && (!T.filter || T.filter === 'all' || x.kind === T.filter); }); }
  function ckSel(d){ var S = cur(d); return (CK.mode === 'tcvn' ? S.list : S.list || []).filter(function(x){ return x.key === S.sel; })[0] || null; }
  function ckRun(d){ if (CK.mode === 'tcvn') runCheck(d); else { var T = stdOf(d); T.list = dwsProblems(d); T.at = hhmm(); } }
  function recheck(){ var d = CK.d, S = cur(d), v0 = ckItems(d).map(function(x){ return x.key; }), i0 = v0.indexOf(S.sel); ckRun(d); var v1 = ckItems(d); if (!v1.some(function(x){ return x.key === S.sel; })){ var nx = v1.filter(function(x){ return !x.ign; }); S.sel = nx.length ? nx[Math.min(Math.max(i0, 0), nx.length - 1)].key : null; } }
  function ckOpen(mode){
    var d = D(); if (!d) return; CK.d = d; CK.mode = mode || CK.mode;
    if (CK.dlg && document.contains(CK.dlg.el)){ var t = $('[data-dt="' + CK.mode + '"]', CK.dlg.el); if (t && t.getAttribute('aria-selected') !== 'true') t.click(); else ckRender(); return CK.dlg; }
    var dlg = CK.dlg = TC.dialog({ id:'ckpanel', title:CK.mode === 'tcvn' ? 'Kiểm tra bản vẽ theo TCVN' : 'Kiểm tra chuẩn .dws', cmd:CK.mode === 'tcvn' ? 'TCCHECK' : 'CHECKSTANDARDS', code:'UI-I09', w:448, modal:false, flush:true, focus:'.tabs [aria-selected="true"]',
      body:'<div class="rv">' + H.tabs([['tcvn', 'TCVN và quy tắc'], ['dws', 'Tệp chuẩn .dws']], CK.mode, 'ckt') + '<div class="rv-pane" id="ck-b"></div></div>', foot:'<span></span>',
      onClose:function(){ CK.dlg = null; CK.note = null; if (CK.busy) clearInterval(CK.busy.t); CK.busy = null; V().drawOver(); } });
    dockR(dlg, 448);
    var el = dlg.el;
    el.addEventListener('dtab', function(e){ CK.mode = e.detail; CK.note = null; var t = $('.dlg-h .t', el); t.innerHTML = (CK.mode === 'tcvn' ? 'Kiểm tra bản vẽ theo TCVN' : 'Kiểm tra chuẩn .dws') + ' <span class="cmdn">' + (CK.mode === 'tcvn' ? 'TCCHECK' : 'CHECKSTANDARDS') + '</span>'; var S = cur(CK.d); if (!S.at) ckAnim(); else { recheck(); ckRender(); } V().drawOver(); });
    el.addEventListener('click', ckClick);
    el.addEventListener('change', ckChange);
    el.addEventListener('mouseover', function(e){ var r = e.target.closest('.rv-row[data-k]'), S = cur(CK.d), k = r ? r.dataset.k : null; if (k !== S.hot){ S.hot = k; V().drawOver(); } });
    el.addEventListener('mouseleave', function(){ var S = cur(CK.d); if (S.hot){ S.hot = null; V().drawOver(); } });
    el.addEventListener('dblclick', function(e){ var r = e.target.closest('.rv-row[data-k]'); if (r && !e.target.closest('button')) ckAct('zoom', r.dataset.k); });
    el.addEventListener('keydown', function(e){
      if (!e.target.closest('.rv-list') || e.target.closest('textarea, input, select')) return; var d = CK.d, S = cur(d), vis = ckItems(d), i = vis.map(function(x){ return x.key; }).indexOf(S.sel);
      if (e.key === 'ArrowDown' || e.key === 'ArrowUp'){ e.preventDefault(); i = TC.clamp(i + (e.key === 'ArrowDown' ? 1 : -1), 0, vis.length - 1); if (vis[i]){ S.sel = vis[i].key; S.oi = 0; ckRender(); ckAct('zoom', S.sel); } }
      else if (e.key === 'Enter' && S.sel){ e.preventDefault(); ckAct('fix', S.sel); }
      else if (e.key === 'Delete' && S.sel){ e.preventDefault(); ckAct('ign', S.sel); }
    });
    var S = cur(d); if (!S.at) ckAnim(); else { recheck(); ckRender(); }
    return dlg;
  }
  function ckAnim(done){
    var d = CK.d, steps = CK.mode === 'tcvn' ? RULES.filter(function(r){ return CKS.sets[r.set] && rule(r).on; }).map(function(r){ return r.id + ' · ' + r.name; }) : stdOf(d).files.length ? Object.keys(PLUG).filter(function(k){ return stdOf(d).plug[k]; }).map(function(k){ return 'So ' + PLUG[k][0].toLowerCase() + ' với ' + stdOf(d).files.join(', '); }) : [];
    if (CK.busy) clearInterval(CK.busy.t);
    if (!steps.length){ CK.busy = null; ckRun(d); ckRender(); if (done) done(); return; }
    CK.busy = { k:0, steps:steps, t:setInterval(function(){ if (!CK.busy) return; CK.busy.k += CK.mode === 'tcvn' ? 3 : 1; if (CK.busy.k >= steps.length){ clearInterval(CK.busy.t); CK.busy = null; recheck(); var S = cur(d); if (!S.sel){ var v = ckItems(d); S.sel = v.length ? v[0].key : null; } ckRender(); V().drawOver(); listFocus(CK.dlg); if (done) done(); } else ckRender(); }, 160) };
    ckRender();
  }
  var KIND = { lay:['Layer', 'layers'], sty:['Kiểu chữ', 'mtext'], lt:['Linetype', 'line'], dim:['Kiểu kích thước', 'dim'] };
  function ckRender(){
    if (!CK.dlg || !document.contains(CK.dlg.el)) return;
    var el = CK.dlg.el, d = CK.d, S = cur(d), host = $('#ck-b', el), foot = $('.dlg-f', el), lst = $('.rv-list', host), top = lst ? lst.scrollTop : 0, tc = CK.mode === 'tcvn', foc = lst && lst === document.activeElement;
    var bar = tc ? '<div class="rv-bar"><span class="lab">Bộ quy tắc</span>' + Object.keys(SETS).map(function(k){ var n = RULES.filter(function(r){ return r.set === k && rule(r).on; }).length; return '<button class="rv-chip" type="button" data-set="' + k + '" aria-pressed="' + !!CKS.sets[k] + '" data-tip="' + esc(SETS[k][0]) + '" data-desc="' + (SETS[k][1] === 'TrionCAD' ? 'Bộ quy tắc của TrionCAD, cập nhật theo phiên bản.' : 'Bộ quy tắc của công ty, sửa được trong Bộ quy tắc.') + '">' + (CKS.sets[k] ? ic('check', 's12') : '') + esc(SETS[k][0].replace('Quy tắc công ty An Phú', 'Công ty')) + ' <b>' + n + '</b></button>'; }).join('') + '<span class="grow"></span><button class="ib" type="button" data-ck="rules" aria-label="Bộ quy tắc" data-tip="Bộ quy tắc" data-desc="Bật, tắt quy tắc; sửa tham số quy tắc công ty.">' + ic('gear') + '</button><button class="ib" type="button" data-ck="run" aria-label="Kiểm tra lại" data-tip="Kiểm tra lại"' + (CK.busy ? ' disabled' : '') + '>' + ic('refresh') + '</button></div>'
      : '<div class="rv-bar"><span class="lab">Tệp chuẩn</span>' + (S.files.length ? S.files.map(function(f, i){ return '<span class="tag' + (i ? '' : ' acc') + '" title="' + esc(DWS[f] ? DWS[f].path : f) + '">' + ic('shield', 's12') + esc(f) + '</span>'; }).join('') : '<span class="faint">chưa gắn</span>') + '<span class="grow"></span><button class="btn sm" type="button" data-ck="std">Cấu hình…</button><button class="ib" type="button" data-ck="run" aria-label="Kiểm tra lại" data-tip="Kiểm tra lại"' + (CK.busy || !S.files.length ? ' disabled' : '') + '>' + ic('refresh') + '</button></div>';
    if (CK.busy){ var b = CK.busy, pc = Math.min(99, Math.round(b.k / b.steps.length * 100)); host.innerHTML = bar + '<div class="rv-busy" role="status"><div class="hrow"><span class="spin"></span><b>' + (tc ? 'Đang kiểm tra ' + b.steps.length + ' quy tắc' : 'Đang so với tệp chuẩn') + '</b><span class="grow"></span><span class="mono faint">' + pc + '%</span></div><div class="prog"><i style="width:' + pc + '%"></i></div><ol class="rv-steps">' + b.steps.slice(Math.max(0, b.k - 2), b.k + 3).map(function(s, i){ var j = Math.max(0, b.k - 2) + i; return '<li class="' + (j < b.k ? 'done' : j === b.k ? 'cur' : '') + '">' + (j < b.k ? ic('check', 's12') : '<i></i>') + esc(s) + '</li>'; }).join('') + '</ol></div>'; foot.innerHTML = '<span class="faint rv-s">Vùng vẽ vẫn dùng được trong lúc kiểm tra.</span><span class="grow"></span><button class="btn" type="button" data-ck="stop">Dừng</button>'; return; }
    if (!tc && !S.files.length){ host.innerHTML = bar + '<div class="empty">' + ic('shield') + '<b>Chưa gắn tệp chuẩn</b><span>Gắn tệp .dws của công ty để kiểm tra layer, kiểu chữ, kiểu kích thước, linetype. Có thể tạo .dws từ một bản vẽ mẫu.</span><button class="btn sm primary" type="button" data-ck="std">Gắn tệp chuẩn…</button></div>'; foot.innerHTML = '<span class="grow"></span><button class="btn" type="button" data-x="close">Đóng</button>'; return; }
    if (tc && !Object.keys(SETS).some(function(k){ return CKS.sets[k]; })){ host.innerHTML = bar + '<div class="empty">' + ic('filter') + '<b>Chưa chọn bộ quy tắc</b><span>Bật ít nhất một bộ quy tắc ở trên để kiểm tra.</span></div>'; foot.innerHTML = '<span class="grow"></span><button class="btn" type="button" data-x="close">Đóng</button>'; return; }
    var L = S.list || [], open = L.filter(function(x){ return !x.ign; }), nIgn = L.length - open.length, vis = ckItems(d), sel = ckSel(d), fixed = S.log.filter(function(x){ return x.act === 'Đã sửa'; }).length, ignN = S.log.filter(function(x){ return x.act === 'Bỏ qua'; }).length;
    var chip = function(k, lab, n){ return '<button class="rv-chip" type="button" data-cf="' + k + '" aria-pressed="' + ((S.filter || 'all') === k) + '">' + lab + ' <b>' + TC.n(n) + '</b></button>'; };
    var chips = tc ? chip('all', 'Tất cả', open.length) + ['err', 'warn', 'hint'].map(function(k){ var n = open.filter(function(x){ return x.sev === k; }).length; return n ? chip(k, ic(SEV[k][1], 's12') + SEV[k][0], n) : ''; }).join('') + '<button class="rv-chip" type="button" data-cf="pass" aria-pressed="' + (S.filter === 'pass') + '">' + ic('okc', 's12') + 'Đạt <b>' + TC.n(S.pass.length) + '</b></button>'
      : chip('all', 'Tất cả', open.length) + Object.keys(KIND).map(function(k){ var n = open.filter(function(x){ return x.kind === k; }).length; return n ? chip(k, ic(KIND[k][1], 's12') + KIND[k][0], n) : ''; }).join('');
    var lock = !pro() ? '<div class="note warn rv-note">' + ic('lock') + '<span>Bản ' + esc(TC.EDITIONS[TC.S.edition].badge) + ' chỉ xem kết quả. Sửa cần TrionCAD Pro.</span><button class="btn sm" type="button" data-ck="up">Nâng cấp</button></div>' : '';
    var done = !open.length && L.length + S.log.length > 0 || (!L.length && S.at);
    var summary = done ? '<div class="ck-done">' + ic('okc') + '<div><b>' + (tc ? 'Kiểm tra xong' : 'Kiểm tra chuẩn xong') + (open.length ? '' : ': không còn vấn đề') + '</b><span>' + (tc ? TC.n(S.pass.length) + ' quy tắc đạt' : 'So với ' + esc(S.files.join(', '))) + ' · đã sửa ' + TC.n(fixed) + ' · bỏ qua ' + TC.n(nIgn) + '</span></div></div>' : '';
    var rows;
    if (tc && S.filter === 'pass') rows = S.pass.map(function(R){ return '<div class="rv-row pass" role="option" aria-selected="false"><span class="sev ok">' + ic('okc') + '</span><span class="t">' + esc(R.r.name) + '</span><span class="m">' + R.r.id + '</span><span></span></div>'; }).join('') || '<div class="empty"><span>Chưa có quy tắc nào đạt.</span></div>';
    else rows = vis.map(function(x){ return ckRow(x, x === sel); }).join('') || (L.length ? '<div class="empty">' + ic('filter') + '<span>Không có mục trong nhóm đang lọc.</span></div>' : '');
    host.innerHTML = bar + (L.length || S.pass && S.pass.length ? '<div class="rv-chips" role="group" aria-label="Lọc">' + chips + '</div>' : '') + lock + summary + '<div class="rv-list" role="listbox" tabindex="0" aria-label="' + (tc ? 'Kết quả kiểm tra' : 'Vấn đề so với chuẩn') + '"' + (sel ? ' aria-activedescendant="ckr-' + esc(sel.key) + '"' : '') + '>' + rows + '</div>';
    var nl = $('.rv-list', host); if (nl){ nl.scrollTop = top; var se = $('.rv-row.is-sel', nl); if (se) se.scrollIntoView({ block:'nearest' }); if (foc) nl.focus({ preventScroll:true }); }
    var nFix = open.filter(function(x){ return tc ? x.fix : x.pick; }).length, hasNext = open.length > 1 || (open.length === 1 && (!sel || sel.key !== open[0].key));
    foot.innerHTML = (tc ? '<button class="btn" type="button" data-ck="rpt">' + ic('list') + 'Báo cáo</button>' : '<button class="btn" type="button" data-ck="std">Thiết lập…</button><button class="btn ghost" type="button" data-ck="rpt">Báo cáo</button>') + (nIgn ? '<label class="chk rv-s"><input type="checkbox" id="ck-ign"' + (S.showIgn ? ' checked' : '') + '> Hiện ' + TC.n(nIgn) + ' bỏ qua</label>' : '') + '<span class="grow"></span>' +
      '<button class="btn" type="button" data-ck="next"' + (hasNext ? '' : ' disabled') + ' data-tip="Vấn đề kế" data-key="↓">Vấn đề kế</button>' +
      (tc ? '<button class="btn primary" type="button" data-ck="fixall"' + (nFix ? '' : ' disabled') + ' data-tip="Sửa tự động" data-desc="Áp mọi cách sửa tự động của các mục đang hiện, một bước hoàn tác.">Sửa tự động ' + TC.n(nFix) + lockTag() + '</button>' : '<button class="btn primary" type="button" data-ck="fix"' + (sel && sel.pick && !sel.ign ? '' : ' disabled') + ' data-key="Enter">Sửa' + lockTag() + '</button>');
  }
  function ckRow(x, on){
    var tc = CK.mode === 'tcvn', S = cur(CK.d), note = tc && S.notes[x.key];
    var head = '<div class="rv-row' + (on ? ' is-sel' : '') + (x.ign ? ' ign' : '') + '" role="option" id="ckr-' + esc(x.key) + '" aria-selected="' + on + '" data-k="' + esc(x.key) + '">' + (tc ? sevIc(x.sev) : '<span class="sev ' + x.sev + '">' + ic(KIND[x.kind][1]) + '</span>') + '<span class="t">' + esc(x.title) + (x.ign ? ' <span class="tag">bỏ qua</span>' : '') + (note ? ' ' + ic('textnote', 's12') : '') + '</span><span class="m">' + (x.n ? TC.n(x.n) + ' ' + x.unit : tc ? x.R.r.id : '') + '</span>' +
      '<span class="acts">' + (x.objs && x.objs.length ? '<button class="ib sm" type="button" data-ca="zoom" aria-label="Zoom tới" data-tip="Zoom tới">' + ic('zoom') + '</button>' : '') + (x.ign ? '<button class="ib sm" type="button" data-ca="unign" aria-label="Hiện lại" data-tip="Bỏ đánh dấu bỏ qua">' + ic('eye') + '</button>' : '<button class="ib sm" type="button" data-ca="ign" aria-label="Bỏ qua" data-tip="Bỏ qua" data-key="Del">' + ic('eyeoff') + '</button>') + '</span></div>';
    if (!on) return head;
    var objs = x.objs || [], oi = Math.min(S.oi || 0, Math.max(0, objs.length - 1)), o = objs[oi];
    var stepper = objs.length ? '<div class="ck-obj"><button class="ib sm" type="button" data-ca="prev" aria-label="Đối tượng trước"' + (objs.length > 1 ? '' : ' disabled') + '>' + ic('left') + '</button><span class="mono rv-s">' + (oi + 1) + '/' + TC.n(objs.length) + '</span><button class="ib sm" type="button" data-ca="nexto" aria-label="Đối tượng kế"' + (objs.length > 1 ? '' : ' disabled') + '>' + ic('right') + '</button><span class="ck-on">' + esc(TC.TYPE_NAMES[o.e.t] || o.e.t) + ' · ' + esc(o.e.layer) + ' · ' + esc(SC.att.where(CK.d, o.sp)) + (o.e.t === 'text' || o.e.t === 'mtext' ? ' · “' + esc(String(o.e.s).slice(0, 26)) + '”' + (o.ph ? ' ' + TC.d(o.ph, 1) + ' mm' : '') : o.e.t === 'insert' ? ' · ' + esc(o.e.name) : '') + '</span><button class="btn sm ghost" type="button" data-ca="zoom">' + ic('zoom') + 'Zoom</button></div>' : '';
    var det;
    if (tc){ var R = x.R, ref = R.r.ref;
      det = '<p>' + esc(R.r.why) + '</p>' + (x.sub ? '<p><b>Chi tiết:</b> ' + esc(x.sub) + '</p>' : '') + (x.rows ? '<div class="ck-mini">' + x.rows.map(function(r){ return '<span>' + r[0] + '</span><span>' + r[1] + '</span>'; }).join('') + '</div>' : '') +
        '<div class="ck-ref">' + ic('book', 's12') + '<div>' + (ref ? '<span><b>' + esc(ref) + '</b> · ' + esc(REFS[ref] || '') + '</span><span class="faint">' + (/^TCVN/.test(ref) ? 'Điều khoản cụ thể: chờ chuyên gia đối chiếu.' : 'Tài liệu nội bộ của công ty.') + '</span>' : '<b>Kiểm tra bổ trợ của TrionCAD</b><span class="faint">Không gắn điều khoản TCVN.</span>') + '</div>' + (ref && /^TCVN/.test(ref) ? '<span class="tag warn" title="Số hiệu tiêu chuẩn là thật; nội dung, điều khoản cần chuyên gia kiểm trước khi phát hành">Trích dẫn mẫu</span>' : '') + '</div>' + stepper +
        (x.opt ? '<div class="frow" style="--lab-w:118px"><label class="lab" for="ck-opt">' + esc(x.opt.label) + '</label><select class="inp" id="ck-opt">' + SC.opts(x.opt.list, x.opt.val) + '</select></div>' : '') +
        (CK.note === x.key ? '<div class="ck-note"><textarea class="inp" id="ck-nt" rows="2" placeholder="Ví dụ: chủ đầu tư yêu cầu giữ nguyên; sửa ở bản rev-B" aria-label="Ghi chú cho mục này">' + esc(note || '') + '</textarea><div class="hrow"><span class="grow"></span><button class="btn sm ghost" type="button" data-ca="notex">Hủy</button><button class="btn sm" type="button" data-ca="notes">Lưu ghi chú</button></div></div>' : note ? '<p class="ck-nv">' + ic('textnote', 's12') + esc(note) + '</p>' : '') +
        '<div class="hrow">' + (x.ign ? '<button class="btn sm" type="button" data-ca="unign">Bỏ đánh dấu bỏ qua</button>' : (x.fix ? '<button class="btn sm primary" type="button" data-ca="fix" data-tip="' + esc(x.fix.label) + '"' + (x.fix.desc ? ' data-desc="' + esc(x.fix.desc) + '"' : '') + '>Sửa tự động' + lockTag() + '</button>' : '') + (x.act ? '<button class="btn sm' + (x.fix ? '' : ' primary') + '" type="button" data-ca="act">' + esc(x.act.label) + '</button>' : '') + (!x.fix && !x.act ? '<span class="faint rv-s">Không có cách sửa tự động.</span>' : '') + '<button class="btn sm" type="button" data-ca="ign">Bỏ qua</button>') + '<button class="btn sm ghost" type="button" data-ca="note">' + ic('textnote') + 'Ghi chú</button></div>' + (x.fix ? '<p class="faint rv-s">Sửa tự động: ' + esc(x.fix.label) + '.' + (x.fix.desc ? ' ' + esc(x.fix.desc) : '') + '</p>' : '');
    } else {
      var rec = x.rec, M = dwsMerged(CK.d), sw = function(n){ var w = M.layers[n]; return w ? SC.colorSw(w.color) : ''; };
      det = '<div class="ck-sec"><span class="gh">Vấn đề</span><p>' + esc(x.title) + (x.n ? ' · ' + TC.n(x.n) + ' ' + x.unit : '') + '.</p></div>' +
        '<div class="ck-sec"><span class="gh">Thay bằng</span><div class="ck-rep" role="radiogroup" aria-label="Thay bằng">' + x.opts.map(function(n){ return '<label class="ck-opt' + (n === x.pick ? ' on' : '') + '"><input type="radio" name="ck-rep" value="' + esc(n) + '"' + (n === x.pick ? ' checked' : '') + (x.ign ? ' disabled' : '') + '>' + (x.kind === 'lay' ? sw(n) : '') + '<span>' + esc(n) + '</span>' + (n === rec && x.k !== 'lay-prop' ? '<span class="tag acc">đề xuất</span>' : '') + '</label>'; }).join('') + '</div>' + (x.pick ? '' : '<span class="fmsg err">' + ic('warn', 'xs') + 'Chọn một mục để thay. Chưa có mục nào khớp tên “' + esc(x.name) + '”.</span>') + '</div>' +
        '<div class="ck-sec"><span class="gh">Xem trước thay đổi</span><table class="tbl ck-pv"><colgroup><col style="width:30%"><col><col></colgroup><thead><tr><th>Thuộc tính</th><th>Hiện tại</th><th>Sau khi sửa</th></tr></thead><tbody>' + dwsRows(CK.d, x).map(function(r){ var ch = String(r[1]).replace(/<[^>]+>/g, '') !== String(r[2]).replace(/<[^>]+>/g, '') && r[2] !== '—'; return '<tr' + (ch ? ' class="ch"' : '') + '><td>' + r[0] + '</td><td title="' + esc(String(r[1]).replace(/<[^>]+>/g, '')) + '">' + r[1] + '</td><td title="' + esc(String(r[2]).replace(/<[^>]+>/g, '')) + '">' + r[2] + '</td></tr>'; }).join('') + '</tbody></table></div>' + stepper +
        '<label class="chk rv-s"><input type="checkbox" data-ca="ignbox"' + (x.ign ? ' checked' : '') + '> Đánh dấu bỏ qua vấn đề này</label>';
    }
    return head + '<div class="rv-det ck-det">' + det + '</div>';
  }
  function ckAct(a, key){
    var d = CK.d, S = cur(d), x = (S.list || []).filter(function(i){ return i.key === key; })[0], tc = CK.mode === 'tcvn'; if (!x) return;
    if (S.sel !== key){ S.sel = key; S.oi = 0; }
    if (a === 'zoom' || a === 'prev' || a === 'nexto'){ var objs = x.objs || []; if (!objs.length){ ckRender(); return; } if (a !== 'zoom') S.oi = (S.oi + (a === 'prev' ? -1 : 1) + objs.length) % objs.length; var o = objs[Math.min(S.oi, objs.length - 1)]; zoomTo(d, o.e._bb, o.sp, CK.dlg && CK.dlg.el); ckRender(); V().drawOver(); return; }
    if (a === 'ign' || a === 'unign' || a === 'ignbox'){ var on = a === 'ign' || (a === 'ignbox' && !x.ign); if (on){ S.ign[key] = 1; S.log.push({ t:x.title, act:'Bỏ qua', at:hhmm(), sev:x.sev, note:S.notes && S.notes[key] }); } else delete S.ign[key]; x.ign = on; if (on){ var nx = ckItems(d).filter(function(i){ return !i.ign && i.key !== key; }); var all0 = (S.list || []).filter(function(i){ return !i.ign || i.key === key; }), j = all0.indexOf(x); var after = all0.slice(j + 1).concat(all0.slice(0, j)).filter(function(i){ return !i.ign; })[0]; if (!S.showIgn) S.sel = after ? after.key : null; } ckRender(); V().drawOver(); return; }
    if (a === 'note'){ CK.note = CK.note === key ? null : key; ckRender(); var t = $('#ck-nt', CK.dlg.el); if (t) t.focus(); return; }
    if (a === 'notex'){ CK.note = null; ckRender(); return; }
    if (a === 'notes'){ var v = $('#ck-nt', CK.dlg.el).value.trim(); if (v) S.notes[key] = v; else delete S.notes[key]; CK.note = null; ckRender(); return; }
    if (a === 'act'){ var r = x.act.run(); if (!x.act.sel) TC.cmdline.msg('TCCHECK: ' + esc(x.act.label.replace(/…$/, '')) + '.'); else TC.cmdline.msg('TCCHECK: đã chọn ' + TC.n(x.objs.length) + ' đối tượng; sửa, xóa hoặc chuyển rồi kiểm tra lại.'); return r; }
    if (a === 'fix'){
      if (!pro()){ SC.upgrade(tc ? 'Kiểm tra bản vẽ theo TCVN' : 'Kiểm tra chuẩn .dws', 'PRO'); return; }
      if (tc){ if (!x.fix) return; ED.run('TCCHECK: ' + x.fix.label, function(){ x.fix.run(d); }); S.log.push({ t:x.title, act:'Đã sửa', at:hhmm(), sev:x.sev, how:x.fix.label, note:S.notes[x.key] }); TC.cmdline.msg('TCCHECK: ' + esc(x.R.r.id) + ' · ' + esc(x.fix.label) + '. Ctrl+Z để hoàn tác.'); }
      else { if (!x.pick || x.ign) return; ED.run('CHECKSTANDARDS: ' + x.name + ' → ' + x.pick, function(){ dwsFix(d, x); }); S.log.push({ t:x.title, act:'Đã sửa', at:hhmm(), sev:x.sev, how:x.name + ' → ' + x.pick }); TC.cmdline.msg('CHECKSTANDARDS: đã thay “' + esc(x.name) + '” bằng “' + esc(x.pick) + '”.'); A().refreshPalettes(); A().renderRibbon(); }
    }
  }
  function ckClick(e){
    var d = CK.d, S = cur(d), b = e.target.closest('[data-ck]'), f = e.target.closest('[data-cf]'), st = e.target.closest('[data-set]'), r = e.target.closest('.rv-row[data-k]'), a = e.target.closest('[data-ca]');
    if (e.target.closest('input[type="radio"], select, textarea, input[type="checkbox"]')) return;
    if (st){ CKS.sets[st.dataset.set] = !CKS.sets[st.dataset.set]; TC.save('ck', CKS); recheck(); ckRender(); V().drawOver(); return; }
    if (f){ S.filter = f.dataset.cf; var v = ckItems(d); if (S.filter !== 'pass' && !v.some(function(x){ return x.key === S.sel; })) S.sel = v.length ? v[0].key : null; ckRender(); V().drawOver(); return; }
    if (a && a.tagName === 'BUTTON'){ var k0 = r ? r.dataset.k : S.sel; if (k0) ckAct(a.dataset.ca, k0); return; }
    if (r && !e.target.closest('.rv-det')){ S.sel = r.dataset.k === S.sel ? null : r.dataset.k; S.oi = 0; CK.note = null; ckRender(); V().drawOver(); var l = $('.rv-list', CK.dlg.el); if (l) l.focus({ preventScroll:true }); return; }
    if (!b || b.disabled) return; var k = b.dataset.ck;
    if (k === 'run') ckAnim(function(){ var S2 = cur(d); TC.cmdline.msg((CK.mode === 'tcvn' ? 'TCCHECK' : 'CHECKSTANDARDS') + ': ' + TC.n((S2.list || []).filter(function(x){ return !x.ign; }).length) + ' vấn đề.'); });
    else if (k === 'stop'){ clearInterval(CK.busy.t); CK.busy = null; ckRender(); }
    else if (k === 'rules') ruleEditor();
    else if (k === 'std') stdConfig();
    else if (k === 'up') SC.upgrade('Kiểm tra bản vẽ theo TCVN', 'PRO');
    else if (k === 'rpt') ckReport(d);
    else if (k === 'next'){ var vis = ckItems(d).filter(function(x){ return !x.ign; }), i = vis.map(function(x){ return x.key; }).indexOf(S.sel), nx = vis[(i + 1) % vis.length]; if (nx){ S.sel = nx.key; S.oi = 0; ckRender(); ckAct('zoom', nx.key); } }
    else if (k === 'fix'){ if (S.sel) ckAct('fix', S.sel); }
    else if (k === 'fixall'){ if (!pro()){ SC.upgrade('Kiểm tra bản vẽ theo TCVN', 'PRO'); return; } var L = ckItems(d).filter(function(x){ return !x.ign && x.fix; });
      TC.confirm({ title:'Sửa tự động', msg:'Sửa tự động <b>' + TC.n(L.length) + '</b> mục?', detail:L.map(function(x){ return esc(x.R.r.id) + ' · ' + esc(x.fix.label); }).join('<br>') + '<br>Một bước hoàn tác.', ok:'Sửa ' + TC.n(L.length) + ' mục', w:520 }).then(function(y){ if (!y) return; ED.run('TCCHECK: sửa tự động ' + L.length + ' mục', function(){ L.forEach(function(x){ x.fix.run(d); S.log.push({ t:x.title, act:'Đã sửa', at:hhmm(), sev:x.sev, how:x.fix.label }); }); }); TC.toast('Đã sửa tự động ' + TC.n(L.length) + ' mục.', { log:false, act:{ label:'Hoàn tác', fn:function(){ ED.undo(1); } } }); }); }
  }
  function ckChange(e){
    var d = CK.d, S = cur(d), t = e.target, x = ckSel(d);
    if (t.id === 'ck-ign'){ S.showIgn = t.checked; ckRender(); V().drawOver(); }
    else if (t.name === 'ck-rep' && x){ x.pick = t.value; ckRender(); V().drawOver(); }
    else if (t.id === 'ck-opt' && x && x.opt){ x.opt.val = t.value; }
    else if (t.dataset.ca === 'ignbox' && x) ckAct('ignbox', x.key);
  }
  // lớp phủ: đối tượng của mục đang chọn (đậm) và mục đang rê (nhạt); đối tượng đang xem có khung đứt quanh
  TC.overlays.push(function(ctx, v){
    var d = v.doc; if (!CK.dlg || !document.contains(CK.dlg.el) || CK.d !== d || CK.busy) return;
    var S = cur(d), L = S.list || [], sel = L.filter(function(x){ return x.key === S.sel; })[0], hot = L.filter(function(x){ return x.key === S.hot && x !== sel; })[0], c = TC.cssv('--c-hl-res');
    if (hot && hot.objs) glow(ctx, v, hot.objs.slice(0, 600), c, .25);
    if (!sel || !sel.objs || !sel.objs.length) return;
    var M = CK.mode === 'dws' && sel.kind === 'lay' && sel.pick ? dwsMerged(d).layers[sel.pick] : null;
    glow(ctx, v, sel.objs.slice(0, 1500), c, sel.objs.length > 40 || /insert/.test(sel.objs[0].e.t) ? .28 : .45, M ? (M.color === 7 ? '#FFFFFF' : TC.ACI[M.color] || c) : null);
    var o = sel.objs[Math.min(S.oi || 0, sel.objs.length - 1)], b = o && o.e._bb; if (!b) return;
    var mv = v.modelView(), view = o.sp === 'model' ? (mv && (d.space === 'model' || d.activeVp) ? mv.v : null) : (d.space === o.sp && !d.activeVp ? v.vw() : null); if (!view) return;
    var a1 = v.toS([b[0], b[3]], view), a2 = v.toS([b[2], b[1]], view); ctx.save(); ctx.strokeStyle = TC.cssv('--c-accent'); ctx.lineWidth = 1.5; ctx.setLineDash([5, 3]); ctx.strokeRect(Math.min(a1[0], a2[0]) - 7.5, Math.min(a1[1], a2[1]) - 7.5, Math.abs(a2[0] - a1[0]) + 15, Math.abs(a2[1] - a1[1]) + 15); ctx.restore();
  });
  var ckT = 0;
  TC.on('dirty', function(d){ if (!CK.dlg || !document.contains(CK.dlg.el) || d !== CK.d || CK.busy) return; clearTimeout(ckT); ckT = setTimeout(function(){ if (!CK.dlg) return; if (cur(CK.d).at) recheck(); ckRender(); V().drawOver(); A().renderStatus(); }, 0); });
  TC.on('doc', function(d){ if (!CK.dlg || !document.contains(CK.dlg.el) || !d) return; CK.d = d; CK.note = null; if (!cur(d).at) recheck(); ckRender(); });
  TC.on('edition', function(){ if (CK.dlg && document.contains(CK.dlg.el)) ckRender(); });
  function ckReport(d){
    var tc = CK.mode === 'tcvn', S = cur(d), L = S.list || [], refs = {};
    if (tc){ L.concat(S.pass.map(function(R){ return { R:R }; })).forEach(function(x){ var r = x.R.r.ref; if (r && /^TCVN/.test(r)) (refs[r] = refs[r] || []).push(x.R.r.id); });
      report({ title:'Báo cáo kiểm tra bản vẽ theo TCVN', cmd:'TCCHECK', code:'UI-I09', file:'Kiem-tra-TCVN_' + fname(d), head:[['Bản vẽ', esc(d.name)], ['Bộ quy tắc', Object.keys(SETS).filter(function(k){ return CKS.sets[k]; }).map(function(k){ return SETS[k][0]; }).join(', ')], ['Tỉ lệ', 'Theo khung tên chứa đối tượng; ngoài khung: ' + esc(d.annoScale || '1:100')]],
        kpi:[['Lỗi', TC.n(L.filter(function(x){ return !x.ign && x.sev === 'err'; }).length)], ['Cảnh báo', TC.n(L.filter(function(x){ return !x.ign && x.sev === 'warn'; }).length)], ['Gợi ý', TC.n(L.filter(function(x){ return !x.ign && x.sev === 'hint'; }).length)], ['Quy tắc đạt', TC.n(S.pass.length)]],
        secs:[{ h:'Kết quả', cols:[['Mã', 44], ['Mức', 64], ['Nội dung', 0], ['Số', 44, 'num'], ['Căn cứ', 104], ['Ghi chú', 110]], rows:L.map(function(x){ return [x.R.r.id, SEV[x.sev][0] + (x.ign ? ' (bỏ qua)' : ''), esc(x.title), x.n ? TC.n(x.n) : '—', esc(x.R.r.ref || 'TrionCAD'), esc(S.notes[x.key] || '')]; }) },
          { h:'Đã xử lý trong phiên', cols:[['Lúc', 46], ['Kết quả', 70], ['Nội dung', 0]], rows:S.log.map(function(x){ return [x.at, x.act, esc(x.t) + (x.how ? ' → ' + esc(x.how) : '') + (x.note ? '<br><i>Ghi chú: ' + esc(x.note) + '</i>' : '')]; }) },
          { h:'Trích dẫn cần chuyên gia kiểm', cols:[['Tiêu chuẩn', 120], ['Tên gọi trong bộ quy tắc', 0], ['Quy tắc', 120]], rows:Object.keys(refs).map(function(r){ return [esc(r), esc(REFS[r] || ''), refs[r].filter(function(x, i, a){ return a.indexOf(x) === i; }).join(', ')]; }), note:'Dữ liệu mẫu của prototype: chỉ ghi số hiệu tiêu chuẩn. Điều khoản cụ thể và cách hiểu từng quy tắc cần chuyên gia TCVN đối chiếu trước khi phát hành bộ quy tắc.' }] });
    } else report({ title:'Báo cáo kiểm tra chuẩn .dws', cmd:'CHECKSTANDARDS', code:'UI-I09', file:'Kiem-tra-chuan_' + fname(d), head:[['Bản vẽ', esc(d.name)], ['Tệp chuẩn', S.files.map(function(f){ return esc(f) + ' <span style="color:var(--c-canvas-axis)">' + esc(DWS[f] ? DWS[f].path : '') + '</span>'; }).join('<br>')], ['Mục kiểm', Object.keys(PLUG).filter(function(k){ return S.plug[k]; }).map(function(k){ return PLUG[k][0]; }).join(', ')]],
        kpi:[['Vấn đề còn lại', TC.n(L.filter(function(x){ return !x.ign; }).length)], ['Đã sửa', TC.n(S.log.filter(function(x){ return x.act === 'Đã sửa'; }).length)], ['Bỏ qua', TC.n(L.filter(function(x){ return x.ign; }).length)], ['Lần kiểm', S.at || '—']],
        secs:[{ h:'Vấn đề', cols:[['Loại', 90], ['Nội dung', 0], ['Số', 44, 'num'], ['Đề xuất thay bằng', 120]], rows:L.map(function(x){ return [KIND[x.kind][0], esc(x.title) + (x.ign ? ' (bỏ qua)' : ''), TC.n(x.n), esc(x.pick || '—')]; }) }, { h:'Đã xử lý trong phiên', cols:[['Lúc', 46], ['Kết quả', 70], ['Nội dung', 0]], rows:S.log.map(function(x){ return [x.at, x.act, esc(x.t) + (x.how ? ' → ' + esc(x.how) : '')]; }) }] });
  }
  /* ---------- trình sửa bộ quy tắc: bật/tắt, mức độ, tham số (quy tắc công ty) ---------- */
  function addCopy(x){ var b = RULES.filter(function(r){ return r.id === x.base; })[0]; if (b && !RULES.some(function(r){ return r.id === x.id; })) RULES.push(Object.assign({}, b, { id:x.id, set:'cty', name:x.name, base:b.id })); }
  (CKS.extra || []).forEach(addCopy);
  function ruleEditor(){
    var W = JSON.parse(JSON.stringify(CKS)), sel = (ckSel(CK.d || D()) || {}).R ? ckSel(CK.d || D()).R.r.id : RULES[0].id, errs = {};
    var dlg = TC.dialog({ id:'ckrules', title:'Bộ quy tắc kiểm tra', cmd:'TCCHECK', code:'UI-I09', w:880, h:Math.min(580, innerHeight - 24), flush:true,
      body:'<div class="ckr"><div class="ckr-l" id="cr-l" role="tree" aria-label="Quy tắc"></div><div class="ckr-d" id="cr-d"></div></div>',
      foot:'<button class="btn" type="button" id="cr-imp">Nhập…</button><button class="btn" type="button" id="cr-exp">Xuất…</button><button class="btn ghost" type="button" id="cr-def">Khôi phục mặc định</button><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="cr-ok">Lưu quy tắc</button>' });
    var el = dlg.el, Rr = function(id){ return RULES.filter(function(r){ return r.id === id; })[0]; }, st = function(r){ return W.rules[r.id] || (W.rules[r.id] = {}); };
    function left(){ $('#cr-l', el).innerHTML = Object.keys(SETS).map(function(k){ var rs = RULES.filter(function(r){ return r.set === k; }), on = rs.filter(function(r){ return st(r).on !== false; }).length;
      return '<div class="ckr-set"><label class="chk"><input type="checkbox" data-crs="' + k + '"' + (W.sets[k] ? ' checked' : '') + '> <b>' + esc(SETS[k][0]) + '</b></label><span class="grow"></span><span class="faint mono rv-s">' + on + '/' + rs.length + '</span></div>' + rs.map(function(r){ var o = st(r); return '<div class="ckr-r' + (r.id === sel ? ' is-sel' : '') + (W.sets[k] ? '' : ' off') + '" data-crr="' + r.id + '" role="treeitem" tabindex="-1" aria-selected="' + (r.id === sel) + '"><input type="checkbox" data-cro="' + r.id + '"' + (o.on !== false ? ' checked' : '') + ' aria-label="Bật quy tắc ' + r.id + '">' + sevIc(o.sev || r.sev) + '<span class="mono faint">' + r.id + '</span><span class="n">' + esc(r.name) + '</span>' + (errs[r.id] ? ic('warn', 's12') : '') + '</div>'; }).join(''); }).join(''); }
    function right(){ var r = Rr(sel), o = st(r), ed = r.set === 'cty', p = Object.assign({}, r.p, o.p || {}), keys = Object.keys(r.pl || {});
      $('#cr-d', el).innerHTML = '<div class="ckr-h"><span class="mono faint">' + r.id + '</span><b>' + esc(r.name) + '</b>' + (ed ? '<span class="tag acc">sửa được</span>' : '<span class="tag">' + ic('lock', 's12') + 'TrionCAD</span>') + '</div>' +
        '<div class="frow" style="--lab-w:120px"><span class="lab">Bộ quy tắc</span><span>' + esc(SETS[r.set][0]) + (r.base ? ' · sao chép từ ' + r.base : '') + '</span><span class="lab">Trạng thái</span><label class="chk"><input type="checkbox" data-cro="' + r.id + '"' + (o.on !== false ? ' checked' : '') + '> Bật quy tắc này</label><label class="lab" for="cr-sev">Mức độ</label><select class="inp" id="cr-sev" style="width:150px"' + (ed ? '' : ' disabled') + '>' + ['err', 'warn', 'hint'].map(function(k){ return '<option value="' + k + '"' + ((o.sev || r.sev) === k ? ' selected' : '') + '>' + SEV[k][0] + '</option>'; }).join('') + '</select></div>' +
        '<div class="gh">Tham số</div>' + (keys.length ? '<div class="frow" style="--lab-w:200px">' + keys.map(function(k){ return '<label class="lab" for="cr-p-' + k + '">' + esc(r.pl[k]) + '</label><span class="stack" style="gap:2px"><input class="inp' + (k === 'pattern' ? ' mono' : '') + (errs[r.id] && errs[r.id][k] ? ' err' : '') + '" id="cr-p-' + k + '" data-crp="' + k + '" value="' + esc(p[k]) + '"' + (ed ? '' : ' readonly') + ' spellcheck="false">' + (errs[r.id] && errs[r.id][k] ? '<span class="fmsg err">' + ic('warn', 'xs') + esc(errs[r.id][k]) + '</span>' : '') + '</span>'; }).join('') + '</div>' : '<p class="faint rv-s" style="margin:0">Quy tắc không có tham số.</p>') +
        '<div class="gh">Căn cứ</div><div class="ck-ref">' + ic('book', 's12') + '<div>' + (r.ref ? '<span><b>' + esc(r.ref) + '</b> · ' + esc(REFS[r.ref] || '') + '</span><span class="faint">' + (/^TCVN/.test(r.ref) ? 'Điều khoản cụ thể: chờ chuyên gia đối chiếu.' : 'Tài liệu nội bộ của công ty.') + '</span>' : '<b>Kiểm tra bổ trợ của TrionCAD</b><span class="faint">Không gắn điều khoản TCVN.</span>') + '</div>' + (r.ref && /^TCVN/.test(r.ref) ? '<span class="tag warn">Trích dẫn mẫu</span>' : '') + '</div>' +
        '<div class="gh">Giải thích hiện trong kết quả</div><p class="rv-s" style="margin:0;color:var(--c-text-2);line-height:1.5">' + esc(r.why) + '</p>' +
        (ed ? '' : '<div class="note">' + ic('info') + '<span>Bộ quy tắc TCVN do TrionCAD cập nhật theo phiên bản: chỉ bật, tắt được. Muốn đổi tham số hay mức độ, sao chép sang quy tắc công ty.</span><button class="btn sm" type="button" id="cr-copy">Sao chép</button></div>'); }
    function check(){ errs = {}; RULES.filter(function(r){ return r.set === 'cty'; }).forEach(function(r){ var p = Object.assign({}, r.p, (W.rules[r.id] || {}).p || {}), e = {}; Object.keys(r.pl || {}).forEach(function(k){ var v = String(p[k]).trim(); if (!v) e[k] = 'Nhập một giá trị.'; else if (k === 'pattern'){ try { new RegExp(v); } catch (x){ var m = String(x.message); e[k] = 'Biểu thức chưa đúng: ' + (/group/i.test(m) ? 'thiếu dấu “)”.' : /character class/i.test(m) ? 'thiếu dấu “]”.' : /repeat/i.test(m) ? 'ký tự lặp * + ? đứng sai chỗ.' : 'kiểm tra lại cú pháp.'); } } else if (/min|ratio/.test(k) && !(+v > 0)) e[k] = 'Nhập số lớn hơn 0, dấu thập phân là dấu chấm.'; }); if (Object.keys(e).length) errs[r.id] = e; }); $('#cr-ok', el).disabled = Object.keys(errs).length > 0; }
    el.addEventListener('click', function(e){ var r = e.target.closest('[data-crr]'); if (r && !e.target.closest('input')){ sel = r.dataset.crr; left(); right(); return; }
      if (e.target.id === 'cr-copy'){ var b = Rr(sel), n = RULES.filter(function(x){ return x.set === 'cty'; }).length + 1, x = { id:'CT' + ('0' + n).slice(-2), base:b.base || b.id, name:b.name + ' (công ty)' }; W.extra = (W.extra || []).concat([x]); addCopy(x); W.rules[x.id] = { p:Object.assign({}, b.p, (W.rules[b.id] || {}).p || {}), sev:(W.rules[b.id] || {}).sev || b.sev }; sel = x.id; left(); right(); TC.toast('Đã sao chép ' + b.id + ' thành ' + x.id + ' trong Quy tắc công ty. Sửa tham số rồi bấm Lưu quy tắc.', { type:'info', log:false }); } });
    el.addEventListener('change', function(e){ var t = e.target; if (t.dataset.crs){ W.sets[t.dataset.crs] = t.checked; left(); return; } if (t.dataset.cro){ st(Rr(t.dataset.cro)).on = t.checked; left(); right(); return; } if (t.id === 'cr-sev'){ st(Rr(sel)).sev = t.value; left(); } });
    el.addEventListener('input', function(e){ var t = e.target; if (!t.dataset.crp) return; var o = st(Rr(sel)); o.p = o.p || {}; o.p[t.dataset.crp] = t.value; check(); var had = t.classList.contains('err'); if (had !== !!(errs[sel] && errs[sel][t.dataset.crp])){ var pos = t.selectionStart; right(); left(); var t2 = $('#cr-p-' + t.dataset.crp, el); t2.focus(); t2.setSelectionRange(pos, pos); } });
    TC.listKeys(el, '.ckr-r', function(r){ sel = r.dataset.crr; left(); right(); var n = $('.ckr-r.is-sel', el); if (n) n.focus(); });
    $('#cr-def', el).addEventListener('click', function(){ TC.confirm({ title:'Khôi phục mặc định', msg:'Đặt lại mọi quy tắc về mặc định của TrionCAD?', detail:'Quy tắc sao chép trong Quy tắc công ty vẫn giữ. Thay đổi áp dụng khi bấm Lưu quy tắc.', ok:'Khôi phục mặc định' }).then(function(y){ if (y){ W.rules = {}; W.sets = { kt:true, kc:false, cty:true }; check(); left(); right(); } }); });
    $('#cr-exp', el).addEventListener('click', function(){ TC.toast('Đã xuất <span class="mono">AnPhu-quy-tac.tcrule</span>: ' + RULES.filter(function(r){ return r.set === 'cty'; }).length + ' quy tắc công ty và trạng thái bật, tắt.', { log:false, act:{ label:'Mở thư mục', fn:function(){} } }); });
    $('#cr-imp', el).addEventListener('click', function(){ var rc = this.getBoundingClientRect(); TC.menu([{ h:'Bộ quy tắc gần đây' }, { label:'AnPhu-quy-tac.tcrule · \\\\SRV-01\\Chung\\Chuan', act:function(){ TC.toast('Bộ quy tắc trên máy chủ giống bộ đang dùng. Không có thay đổi.', { type:'info', log:false }); } }, { label:'Doi-tac-XD-Hoang-Long.tcrule', act:function(){ TC.toast('Đã nạp 4 quy tắc của đối tác vào Quy tắc công ty (đang tắt). Bật để dùng.', { log:false }); } }, { sep:1 }, { label:'Duyệt tệp .tcrule…', act:function(){ TC.toast('Hộp chọn tệp .tcrule (JSON) của hệ điều hành.', { type:'info', log:false }); } }], rc.left, rc.top - 4, { up:true }); });
    $('#cr-ok', el).addEventListener('click', function(){ if (this.disabled) return; CKS = W; TC.save('ck', CKS); dlg.close(true); TC.toast('Đã lưu bộ quy tắc. Kiểm tra lại với ' + RULES.filter(function(r){ return CKS.sets[r.set] && rule(r).on; }).length + ' quy tắc.', { log:false }); if (CK.dlg && document.contains(CK.dlg.el) && CK.mode === 'tcvn') ckAnim(); });
    check(); left(); right();
  }

  /* ---------- Cấu hình chuẩn (STANDARDS): gắn tệp .dws, mục kiểm, thiết lập ---------- */
  function dwsSum(f){ var w = DWS[f]; return w ? TC.n(w.layers.length) + ' layer · ' + w.dims.length + ' kiểu kích thước · ' + w.lts.length + ' linetype · ' + w.styles.length + ' kiểu chữ' : ''; }
  function stdConfig(){
    var d = D(), S = stdOf(d), W = { files:S.files.slice(), plug:Object.assign({}, S.plug), notify:S.notify, auto:S.auto, showIgn:S.showIgn, prefer:S.prefer }, sel = W.files[0] || null, tab = 'f';
    var dlg = TC.dialog({ id:'stdcfg', title:'Cấu hình chuẩn', cmd:'STANDARDS', code:'UI-I09', w:720, h:Math.min(480, innerHeight - 24), flush:true,
      body:'<div class="rv">' + H.tabs([['f', 'Tệp chuẩn'], ['p', 'Mục kiểm tra'], ['s', 'Thiết lập']], 'f', 'stdt') + '<div class="rv-pane" id="sd-b"></div></div>',
      foot:'<button class="btn" type="button" id="sd-chk">' + ic('shield') + 'Kiểm tra chuẩn…</button><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="sd-ok">Lưu cấu hình</button>' });
    var el = dlg.el;
    function render(){ var b = $('#sd-b', el);
      if (tab === 'f') b.innerHTML = W.files.length ? '<div class="std-f"><div class="stack" style="min-height:0;gap:6px"><span class="gh">Tệp chuẩn gắn với ' + esc(d.name) + '</span><div class="tblwrap std-l" role="listbox" tabindex="0" aria-label="Tệp chuẩn">' + W.files.map(function(f, i){ return '<div class="std-r' + (f === sel ? ' is-sel' : '') + '" role="option" aria-selected="' + (f === sel) + '" data-sf="' + esc(f) + '"><span class="mono faint">' + (i + 1) + '</span>' + ic('shield') + '<span class="grow"><b>' + esc(f) + '</b><span class="faint mono">' + esc(DWS[f] ? DWS[f].path : '') + '</span></span></div>'; }).join('') + '</div><div class="hrow" style="gap:4px"><button class="btn sm" type="button" id="sd-add">' + ic('plus') + 'Gắn tệp…</button><button class="btn sm" type="button" id="sd-rm"' + (sel ? '' : ' disabled') + '>Bỏ</button><button class="ib" type="button" id="sd-up" aria-label="Lên (ưu tiên hơn)" data-tip="Ưu tiên hơn"' + (W.files.indexOf(sel) > 0 ? '' : ' disabled') + '>' + ic('up') + '</button><button class="ib" type="button" id="sd-dn" aria-label="Xuống" data-tip="Ưu tiên kém hơn"' + (sel && W.files.indexOf(sel) < W.files.length - 1 ? '' : ' disabled') + '>' + ic('down') + '</button></div><p class="faint rv-s" style="margin:0">Trùng tên thì tệp ở trên được ưu tiên.</p></div>' +
        (sel && DWS[sel] ? '<div class="std-desc"><b>' + esc(sel) + '</b><dl class="kv"><dt>Đường dẫn</dt><dd class="mono rv-s">' + esc(DWS[sel].path) + '</dd><dt>Ngày</dt><dd>' + DWS[sel].date + '</dd><dt>Người lập</dt><dd>' + esc(DWS[sel].by) + '</dd><dt>Nội dung</dt><dd>' + dwsSum(sel) + '</dd></dl><p>' + esc(DWS[sel].desc) + '</p></div>' : '<div></div>') + '</div>'
        : '<div class="empty">' + ic('shield') + '<b>Chưa gắn tệp chuẩn</b><span>Tệp .dws chứa layer, kiểu chữ, kiểu kích thước, linetype chuẩn của công ty. Gắn tệp để kiểm tra và sửa theo chuẩn.</span><div class="hrow" style="justify-content:center"><button class="btn sm primary" type="button" id="sd-add">' + ic('plus') + 'Gắn tệp…</button><button class="btn sm" type="button" id="sd-mk">Tạo .dws từ bản vẽ này…</button></div></div>';
      else if (tab === 'p') b.innerHTML = '<div class="std-p"><span class="gh">Mục được so với tệp chuẩn</span>' + Object.keys(PLUG).map(function(k){ return '<label class="std-pl"><input type="checkbox" data-sp="' + k + '"' + (W.plug[k] ? ' checked' : '') + '>' + ic(PLUG[k][2]) + '<span><b>' + PLUG[k][0] + '</b><span class="faint">' + PLUG[k][1] + '</span></span></label>'; }).join('') + '</div>';
      else b.innerHTML = '<div class="std-p"><span class="gh">Thông báo khi bản vẽ vi phạm chuẩn</span>' + H.rad('sdn', 'count', 'Chỉ báo trên thanh trạng thái, kèm số vấn đề', W.notify === 'count' || W.notify === 'status') + H.rad('sdn', 'icon', 'Chỉ biểu tượng trên thanh trạng thái', W.notify === 'icon') + H.rad('sdn', 'off', 'Tắt thông báo', W.notify === 'off') +
        '<span class="gh">Khi kiểm tra</span>' + H.chk('sd-auto', 'Tự sửa vấn đề chỉ có một cách thay (thuộc tính layer khác chuẩn)', W.auto) + H.chk('sd-ign', 'Hiện cả vấn đề đã đánh dấu bỏ qua', W.showIgn) +
        H.row('Tệp ưu tiên khi thay', '<select class="inp" id="sd-pre" style="width:220px"' + (W.files.length ? '' : ' disabled') + '>' + SC.opts(W.files.length ? W.files : ['(chưa gắn tệp)'], W.prefer || W.files[0]) + '</select>', 160) + '</div>';
      $('#sd-chk', el).disabled = !W.files.length; }
    el.addEventListener('dtab', function(e){ tab = e.detail; render(); });
    el.addEventListener('click', function(e){ var r = e.target.closest('[data-sf]'); if (r){ sel = r.dataset.sf; render(); return; } var id = e.target.closest('button') && e.target.closest('button').id;
      if (id === 'sd-add'){ var rc = e.target.closest('button').getBoundingClientRect(), av = Object.keys(DWS).filter(function(f){ return W.files.indexOf(f) < 0; }); TC.menu([{ h:'Tệp chuẩn đã dùng' }].concat(av.length ? av.map(function(f){ return { label:esc(f) + ' <span class="faint">' + esc(DWS[f].path) + '</span>', icon:'shield', act:function(){ W.files.push(f); sel = f; render(); } }; }) : [{ label:'Đã gắn mọi tệp gần đây', dis:true }]).concat([{ sep:1 }, { label:'Duyệt tệp .dws…', icon:'open', act:function(){ TC.toast('Hộp chọn tệp của hệ điều hành, lọc *.dws.', { type:'info', log:false }); } }]), rc.left, rc.bottom + 2); }
      else if (id === 'sd-rm' && sel){ W.files = W.files.filter(function(f){ return f !== sel; }); sel = W.files[0] || null; render(); }
      else if (id === 'sd-up' || id === 'sd-dn'){ var i = W.files.indexOf(sel), j = i + (id === 'sd-up' ? -1 : 1); W.files.splice(i, 1); W.files.splice(j, 0, sel); render(); }
      else if (id === 'sd-mk') SC.ask('Tạo tệp chuẩn từ bản vẽ', 'Tên tệp .dws', fname(d) + '.dws', function(v){ return /\.dws$/i.test(v) ? null : 'Tên tệp cần đuôi .dws.'; }, 'UI-I09').then(function(v){ if (v) TC.toast('Đã lưu <span class="mono">' + esc(v) + '</span>: ' + d.layers.length + ' layer, ' + d.dimstyles.length + ' kiểu kích thước, ' + SC.ltList(d).length + ' linetype, ' + d.styles.length + ' kiểu chữ.', { log:false }); }); });
    el.addEventListener('change', function(e){ var t = e.target; if (t.dataset.sp) W.plug[t.dataset.sp] = t.checked; if (t.name === 'sdn') W.notify = t.value; if (t.id === 'sd-auto') W.auto = t.checked; if (t.id === 'sd-ign') W.showIgn = t.checked; if (t.id === 'sd-pre') W.prefer = t.value; });
    function save(){ var ch = JSON.stringify([S.files, S.plug]) !== JSON.stringify([W.files, W.plug]);
      ED.run('STANDARDS', function(){ var done = ED.state(function(){ return { files:S.files, plug:S.plug, notify:S.notify, auto:S.auto, showIgn:S.showIgn, prefer:S.prefer }; }, function(s){ Object.assign(S, s); A().renderStatus(); if (CK.dlg && document.contains(CK.dlg.el)){ recheck(); ckRender(); } }); Object.assign(S, { files:W.files.slice(), plug:Object.assign({}, W.plug), notify:W.notify, auto:W.auto, showIgn:W.showIgn, prefer:W.prefer }); done(); });
      if (S.auto && S.files.length){ var auto = dwsProblems(d).filter(function(x){ return x.k === 'lay-prop' && !x.ign; }); if (auto.length){ ED.run('CHECKSTANDARDS: tự sửa ' + auto.length + ' layer', function(){ auto.forEach(function(x){ dwsFix(d, x); S.log.push({ t:x.title, act:'Đã sửa', at:hhmm(), how:'tự sửa theo chuẩn' }); }); }); TC.toast('Đã tự sửa thuộc tính ' + auto.length + ' layer theo chuẩn.', { log:false }); } }
      A().renderStatus(); if (CK.dlg && document.contains(CK.dlg.el) && CK.mode === 'dws'){ if (ch) ckAnim(); else { recheck(); ckRender(); } } }
    $('#sd-ok', el).addEventListener('click', function(){ save(); dlg.close(true); TC.cmdline.msg('STANDARDS: ' + (S.files.length ? 'bản vẽ dùng ' + S.files.join(', ') + '.' : 'bản vẽ không gắn tệp chuẩn.')); });
    $('#sd-chk', el).addEventListener('click', function(){ save(); dlg.close(true); ckOpen('dws'); });
    render(); return dlg;
  }
  IMPL.STANDARDS = function(){ stdConfig(); };

  /* ---------- Dịch layer (LAYTRANS): ánh xạ layer nguồn → layer chuẩn ---------- */
  var LTO = TC.load('laytrans', { color:true, lt:true, transp:true, blocks:true, log:false, show:true });
  var MAPS = { 'Khao-sat-sang-AnPhu.dws':{ 'Ranh dat':'KT-RANHDAT', 'Nha hien trang':'KT-HIENTRANG', 'Duong':'KT-DUONG', 'Layer1':'KT-HIENTRANG', 'Text':'KT-TEXT' }, 'Doi-tac-Hoang-Long.dws':{ 'A-WALL':'KT-TUONG', 'A-DOOR':'KT-CUA', 'A-ANNO-TEXT':'KT-TEXT' } };
  SC.layTrans = function(pre){
    var d = D(); if (!d) return; if (!pro()){ SC.upgrade('Dịch layer', 'PRO'); return; }
    var src = 'AnPhu-KT.dws', T = {}, map = {}, fsel = [], anchor = null, tsel = null, msel = null, q = '', only = false;
    function loadStd(f){ src = f; T = {}; (DWS[f] || DWS['AnPhu-KT.dws']).layers.forEach(function(l){ if (l[0] === '0' || l[0] === 'Defpoints') return; T[l[0]] = { name:l[0], color:l[1], lt:l[2], lw:l[3], tr:0, ps:'ByColor', desc:l[4] || '' }; }); Object.keys(map).forEach(function(s){ if (T[map[s].name]) map[s] = Object.assign({}, T[map[s].name]); }); }
    loadStd(src);
    (pre || []).forEach(function(n){ var g = guess(n, Object.keys(T)); if (g) map[n] = Object.assign({}, T[g]); }); fsel = (pre || []).slice(0, 1);
    var dlg = TC.dialog({ id:'laytrans', title:'Dịch layer', cmd:'LAYTRANS', code:'UI-I09', w:900, h:Math.min(620, innerHeight - 24), flush:true,
      body:'<div class="ltr"><div class="ltr-top"><section class="ltr-col"><header><b>Dịch từ</b><span class="faint">' + esc(d.name) + '</span><span class="grow"></span><label class="search" style="width:150px;height:24px">' + ic('search') + '<input id="lt-q" placeholder="Lọc tên layer" aria-label="Lọc layer nguồn" spellcheck="false"></label></header><div class="ltr-list" id="lt-f" role="listbox" aria-multiselectable="true" tabindex="0" aria-label="Layer nguồn"></div><div class="ltr-pv"><canvas id="lt-pv" width="420" height="96" aria-label="Nội dung layer đang chọn"></canvas><span class="faint rv-s" id="lt-pvt"></span></div></section>' +
        '<div class="ltr-mid"><button class="btn" type="button" id="lt-map" data-tip="Ánh xạ" data-desc="Ánh xạ các layer nguồn đang chọn sang layer đích đang chọn.">Ánh xạ' + ic('right') + '</button><button class="btn" type="button" id="lt-same" data-tip="Ánh xạ cùng tên" data-desc="Ánh xạ mọi layer có cùng tên ở hai bên: layer giữ tên, thuộc tính theo chuẩn.">Cùng tên</button></div>' +
        '<section class="ltr-col"><header><b>Dịch sang</b><span class="faint" id="lt-src"></span><span class="grow"></span><button class="btn sm" type="button" id="lt-load">Nạp…</button><button class="btn sm" type="button" id="lt-new">Layer mới…</button></header><div class="ltr-list" id="lt-t" role="listbox" tabindex="0" aria-label="Layer đích"></div></section></div>' +
        '<section class="ltr-map"><header><b>Ánh xạ dịch layer</b><span class="faint mono" id="lt-n"></span><span class="grow"></span><button class="btn sm" type="button" id="lt-edit">Sửa layer đích…</button><button class="btn sm" type="button" id="lt-rm">Bỏ ánh xạ</button><button class="btn sm" type="button" id="lt-save">Lưu ánh xạ…</button><button class="btn sm" type="button" id="lt-open">Nạp ánh xạ…</button></header><div class="tblwrap ltr-tw"><table class="tbl"><colgroup><col style="width:20%"><col style="width:20%"><col style="width:13%"><col style="width:14%"><col style="width:11%"><col style="width:10%"><col></colgroup><thead><tr><th>Layer cũ</th><th>Layer mới</th><th>Màu</th><th>Linetype</th><th class="num">Độ dày nét</th><th class="num">Trong suốt</th><th>Plot style</th></tr></thead><tbody id="lt-m"></tbody></table></div></section></div>',
      foot:'<button class="btn" type="button" id="lt-set">' + ic('gear') + 'Thiết lập…</button><span class="faint rv-s" id="lt-sum"></span><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="lt-go">Dịch</button>' });
    var el = dlg.el;
    function srcList(){ var qq = TC.fold(q); return d.layers.filter(function(l){ return (!qq || TC.fold(l.name).indexOf(qq) >= 0) && (!only || l.count); }); }
    function objsOf(n){ var top = allEnts(d).filter(function(x){ return x.e.layer === n; }).length, blk = n === '0' ? 0 : Object.keys(d.blocks).reduce(function(s, b){ return s + d.blocks[b].ents.filter(function(e){ return e.layer === n; }).length; }, 0); return { top:top, blk:blk }; }
    function render(){
      $('#lt-src', el).textContent = src;
      $('#lt-f', el).innerHTML = srcList().map(function(l){ var m = map[l.name], on = fsel.indexOf(l.name) >= 0; return '<div class="ltr-r' + (on ? ' is-sel' : '') + (m ? ' mapped' : '') + '" role="option" aria-selected="' + on + '" data-lf="' + esc(l.name) + '">' + SC.colorSw(l.color) + '<span class="grow">' + esc(l.name) + '</span>' + (m ? '<span class="tag acc">→ ' + esc(m.name) + '</span>' : '') + '<span class="faint mono">' + TC.n(l.count || 0) + '</span></div>'; }).join('') || '<div class="empty"><span>Không layer nào khớp.</span></div>';
      $('#lt-t', el).innerHTML = Object.keys(T).map(function(n){ var t = T[n], on = n === tsel; return '<div class="ltr-r' + (on ? ' is-sel' : '') + '" role="option" aria-selected="' + on + '" data-lt="' + esc(n) + '">' + SC.colorSw(t.color) + '<span class="grow">' + esc(n) + (t.isNew ? ' <span class="tag info">mới</span>' : '') + '</span><span class="faint rv-s">' + esc(t.lt) + ' · ' + TC.d(t.lw) + '</span></div>'; }).join('');
      var keys = Object.keys(map); $('#lt-n', el).textContent = keys.length ? '(' + keys.length + ')' : '';
      $('#lt-m', el).innerHTML = keys.map(function(s){ var t = map[s], on = s === msel; return '<tr data-lm="' + esc(s) + '" class="' + (on ? 'is-sel' : '') + '"><td title="' + esc(s) + '">' + esc(s) + '</td><td title="' + esc(t.name) + '">' + (s === t.name ? '<span class="faint">(giữ tên)</span> ' : '') + esc(t.name) + '</td><td>' + SC.colorSw(t.color) + ' ' + esc(SC.colorLabel(t.color)) + '</td><td>' + esc(t.lt) + '</td><td class="num">' + TC.d(t.lw) + '</td><td class="num">' + t.tr + '</td><td>' + esc(t.ps) + '</td></tr>'; }).join('') || '<tr><td colspan="7" class="faint">Chưa có ánh xạ. Chọn layer nguồn bên trái, layer đích bên phải rồi bấm Ánh xạ; hoặc Cùng tên, Nạp ánh xạ.</td></tr>';
      var st = stats(); $('#lt-sum', el).textContent = keys.length ? TC.n(keys.length) + ' layer → ' + TC.n(keys.map(function(s){ return map[s].name; }).filter(function(n, i, a){ return a.indexOf(n) === i; }).length) + ' layer chuẩn · ' + TC.n(st.top + st.blk) + ' đối tượng' : '';
      var go = $('#lt-go', el); go.disabled = !keys.length; go.textContent = keys.length ? 'Dịch ' + TC.n(keys.length) + ' layer' : 'Dịch';
      $('#lt-map', el).disabled = !fsel.length || !tsel; $('#lt-edit', el).disabled = !msel; $('#lt-rm', el).disabled = !msel; $('#lt-save', el).disabled = !keys.length;
      preview();
    }
    function stats(){ var top = 0, blk = 0, mv = 0; Object.keys(map).forEach(function(s){ var o = objsOf(s); top += o.top; if (LTO.blocks) blk += o.blk; if (s !== map[s].name) mv += o.top + (LTO.blocks ? o.blk : 0); }); return { top:top, blk:blk, mv:mv }; }
    // xem nhanh layer đang chọn: chỉ vẽ đối tượng của layer đó, vừa khung
    function preview(){ var c = $('#lt-pv', el), x = c.getContext('2d'), n = fsel[fsel.length - 1], t = $('#lt-pvt', el); x.fillStyle = TC.cssv('--c-canvas'); x.fillRect(0, 0, c.width, c.height);
      if (!LTO.show || !n){ t.textContent = LTO.show ? 'Chọn một layer để xem đối tượng trên layer đó.' : 'Xem nội dung layer đang tắt trong Thiết lập.'; return; }
      var es = d.ents.filter(function(e){ return e.layer === n && e._bb && e.t !== 'xline'; }); if (!es.length){ t.textContent = n + ': không có đối tượng trong Model.'; return; }
      var b = bbOf(es), f = Object.create(TC.View.prototype); f.W = c.width; f.H = c.height; f.doc = d; f.dpr = 1; var v = { cx:(b[0] + b[2]) / 2, cy:(b[1] + b[3]) / 2, s:Math.min((c.width - 16) / Math.max(1, b[2] - b[0]), (c.height - 12) / Math.max(1, b[3] - b[1])) };
      es.forEach(function(e){ f.drawPrims(x, e, v, false); }); var o = objsOf(n); t.textContent = n + ': ' + TC.n(o.top) + ' đối tượng' + (o.blk ? ', ' + TC.n(o.blk) + ' trong block' : '') + '.'; }
    function pickSrc(n, e){ var ls = srcList().map(function(l){ return l.name; }); if (e && e.shiftKey && anchor){ var a = ls.indexOf(anchor), b = ls.indexOf(n); fsel = ls.slice(Math.min(a, b), Math.max(a, b) + 1); } else if (e && (e.ctrlKey || e.metaKey)){ fsel = fsel.indexOf(n) >= 0 ? fsel.filter(function(x){ return x !== n; }) : fsel.concat([n]); anchor = n; } else { fsel = [n]; anchor = n; if (map[n]) { tsel = map[n].name; msel = n; } } render(); }
    function editTarget(s){ var t = map[s]; if (!t) return;
      var e2 = TC.dialog({ id:'ltedit', title:'Sửa layer đích', cmd:'LAYTRANS', code:'UI-I09', w:440, body:'<div class="stack"><p class="rv-s" style="margin:0">Thuộc tính layer <b>' + esc(t.name) + '</b> sau khi dịch. Áp cho mọi ánh xạ tới layer này.</p><div class="frow" style="--lab-w:110px"><span class="lab">Màu</span><span class="hrow"><button class="btn sm" type="button" id="le-c">' + SC.colorSw(t.color) + esc(SC.colorLabel(t.color)) + '</button></span><label class="lab" for="le-lt">Linetype</label><select class="inp" id="le-lt" style="width:200px">' + SC.opts(DWS[src] ? DWS[src].lts : SC.ltList(d), t.lt) + '</select><label class="lab" for="le-lw">Độ dày nét</label><select class="inp" id="le-lw" style="width:120px">' + SC.opts(['0.13', '0.18', '0.25', '0.35', '0.50', '0.70', '1.00'], TC.d(t.lw)) + '</select><label class="lab" for="le-tr">Trong suốt</label><span class="stack" style="gap:2px">' + TC.numField({ id:'le-tr', value:String(t.tr), unit:'none', prec:0, min:-1, pick:false, label:'Độ trong suốt', style:'width:96px' }) + '</span><label class="lab" for="le-ps">Plot style</label><select class="inp" id="le-ps" style="width:160px">' + SC.opts(['ByColor', 'Normal', 'Nét mảnh', 'Nét đậm'], t.ps) + '</select></div></div>',
        foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="le-ok">Lưu layer đích</button>' }), col = t.color;
      $('#le-c', e2.el).addEventListener('click', function(){ SC.colorPicker(col).then(function(c){ if (c !== undefined && c != null && c !== 0){ col = c; $('#le-c', e2.el).innerHTML = SC.colorSw(c) + esc(SC.colorLabel(c)); } }); });
      $('#le-ok', e2.el).addEventListener('click', function(){ var tr = TC.numCommit($('[data-num]', e2.el)); if (tr.err) return; if (tr.v > 90){ var box = $('[data-num]', e2.el); box.classList.add('err'); $('#le-tr-msg', e2.el).className = 'fmsg err'; $('#le-tr-msg', e2.el).innerHTML = ic('warn', 'xs') + 'Độ trong suốt từ 0 đến 90.'; return; } var nv = { color:col, lt:$('#le-lt', e2.el).value, lw:+$('#le-lw', e2.el).value, tr:Math.round(tr.v), ps:$('#le-ps', e2.el).value }; Object.keys(map).forEach(function(k){ if (map[k].name === t.name) Object.assign(map[k], nv); }); if (T[t.name]) Object.assign(T[t.name], nv); e2.close(true); render(); }); }
    el.addEventListener('click', function(e){
      var r = e.target.closest('[data-lf]'); if (r){ pickSrc(r.dataset.lf, e); return; }
      var t = e.target.closest('[data-lt]'); if (t){ tsel = t.dataset.lt; render(); return; }
      var m = e.target.closest('[data-lm]'); if (m){ msel = m.dataset.lm; fsel = [msel]; tsel = map[msel].name; render(); return; }
      var id = e.target.closest('button') && e.target.closest('button').id; if (!id) return;
      if (id === 'lt-map'){ fsel.forEach(function(n){ map[n] = Object.assign({}, T[tsel]); }); msel = fsel[0]; render(); }
      else if (id === 'lt-same'){ var n0 = 0; d.layers.forEach(function(l){ if (T[l.name] && !map[l.name]){ map[l.name] = Object.assign({}, T[l.name]); n0++; } }); render(); TC.toast(n0 ? 'Đã ánh xạ ' + n0 + ' layer cùng tên: giữ tên, thuộc tính theo ' + esc(src) + '.' : 'Không còn layer cùng tên chưa ánh xạ.', { type:n0 ? 'ok' : 'info', log:false }); }
      else if (id === 'lt-rm' && msel){ delete map[msel]; msel = null; render(); }
      else if (id === 'lt-edit' && msel) editTarget(msel);
      else if (id === 'lt-new') SC.ask('Layer mới trong danh sách đích', 'Tên layer', 'KT-', function(v){ return T[v] ? 'Đã có layer “' + v + '”.' : /[<>\/\\":;?*|=`]/.test(v) ? 'Tên layer không chứa < > / \\ " : ; ? * | = `' : null; }, 'UI-I09').then(function(v){ if (v){ T[v] = { name:v, color:7, lt:'Continuous', lw:.25, tr:0, ps:'ByColor', desc:'', isNew:true }; tsel = v; render(); } });
      else if (id === 'lt-load'){ var rc = e.target.closest('button').getBoundingClientRect(); TC.menu([{ h:'Nạp layer chuẩn từ' }].concat(Object.keys(DWS).map(function(f){ return { label:esc(f), icon:'shield', check:f === src, act:function(){ loadStd(f); tsel = null; render(); } }; }), [{ label:'Mau-KT-AnPhu.dwt', icon:'layout', act:function(){ loadStd('AnPhu-KT.dws'); render(); TC.toast('Mẫu Mau-KT-AnPhu.dwt có cùng bộ layer với AnPhu-KT.dws.', { type:'info', log:false }); } }, { sep:1 }, { label:'Duyệt tệp .dwg, .dws, .dwt…', icon:'open', act:function(){ TC.toast('Hộp chọn tệp của hệ điều hành.', { type:'info', log:false }); } }]), rc.left, rc.bottom + 2); }
      else if (id === 'lt-save') SC.ask('Lưu ánh xạ', 'Tên tệp ánh xạ (.dws)', 'Khao-sat-sang-AnPhu.dws', function(v){ return /\.(dws|dwg)$/i.test(v) ? null : 'Tên tệp cần đuôi .dws hoặc .dwg.'; }, 'UI-I09').then(function(v){ if (!v) return; MAPS[v] = {}; Object.keys(map).forEach(function(s){ MAPS[v][s] = map[s].name; }); TC.toast('Đã lưu ' + Object.keys(map).length + ' ánh xạ vào <span class="mono">' + esc(v) + '</span>.', { log:false }); });
      else if (id === 'lt-open'){ var rc2 = e.target.closest('button').getBoundingClientRect(); TC.menu([{ h:'Ánh xạ đã lưu' }].concat(Object.keys(MAPS).map(function(f){ var n = Object.keys(MAPS[f]).filter(function(s){ return d.layerMap[s]; }).length; return { label:esc(f) + ' <span class="faint">· ' + n + '/' + Object.keys(MAPS[f]).length + ' khớp</span>', act:function(){ var k = 0; Object.keys(MAPS[f]).forEach(function(s){ if (d.layerMap[s] && T[MAPS[f][s]]){ map[s] = Object.assign({}, T[MAPS[f][s]]); k++; } }); render(); TC.toast(k ? 'Đã nạp ' + k + ' ánh xạ từ ' + esc(f) + '.' : 'Không có layer nào của bản vẽ khớp ánh xạ trong ' + esc(f) + '.', { type:k ? 'ok' : 'warn', log:false }); } }; })), rc2.left, rc2.bottom + 2); }
      else if (id === 'lt-set'){ var s2 = TC.dialog({ id:'ltset', title:'Thiết lập dịch layer', cmd:'LAYTRANS', code:'UI-I09', w:460, body:'<div class="stack" style="gap:7px">' + H.chk('ls-c', 'Ép màu đối tượng về ByLayer', LTO.color) + H.chk('ls-l', 'Ép linetype đối tượng về ByLayer', LTO.lt) + H.chk('ls-t', 'Ép độ trong suốt đối tượng về ByLayer', LTO.transp) + H.chk('ls-b', 'Dịch cả đối tượng trong block', LTO.blocks) + H.chk('ls-g', 'Ghi nhật ký dịch (.log cạnh bản vẽ)', LTO.log) + H.chk('ls-s', 'Hiện nội dung layer khi chọn', LTO.show) + '<p class="faint rv-s" style="margin:0">Đối tượng trong block trên layer 0 luôn giữ layer 0 để block vẫn đổi màu theo layer chèn.</p></div>', foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="ls-ok">Lưu thiết lập</button>' });
        $('#ls-ok', s2.el).addEventListener('click', function(){ LTO = { color:$('#ls-c', s2.el).checked, lt:$('#ls-l', s2.el).checked, transp:$('#ls-t', s2.el).checked, blocks:$('#ls-b', s2.el).checked, log:$('#ls-g', s2.el).checked, show:$('#ls-s', s2.el).checked }; TC.save('laytrans', LTO); s2.close(true); render(); }); }
      else if (id === 'lt-go'){ var keys = Object.keys(map), st = stats(), del = keys.filter(function(s){ return s !== map[s].name && s !== '0' && s !== 'Defpoints'; });
        TC.confirm({ title:'Dịch layer', w:480, msg:'Dịch <b>' + TC.n(keys.length) + '</b> layer sang chuẩn ' + esc(src) + '?', detail:TC.n(st.mv) + ' đối tượng đổi layer' + (LTO.blocks ? ' (' + TC.n(st.blk) + ' trong block)' : '') + (st.top + st.blk - st.mv ? ', ' + TC.n(st.top + st.blk - st.mv) + ' đối tượng giữ layer, nhận thuộc tính theo chuẩn' : '') + '. ' + (del.length ? TC.n(del.length) + ' layer cũ sẽ bị xóa: ' + esc(del.slice(0, 5).join(', ')) + (del.length > 5 ? '…' : '') + '. ' : '') + (LTO.color || LTO.lt ? 'Ép ' + [LTO.color ? 'màu' : '', LTO.lt ? 'linetype' : '', LTO.transp ? 'độ trong suốt' : ''].filter(Boolean).join(', ') + ' về ByLayer. ' : '') + 'Hoàn tác được bằng Ctrl+Z.', ok:'Dịch ' + TC.n(keys.length) + ' layer' }).then(function(y){ if (!y) return; translate(keys); dlg.close(true); }); }
    });
    function translate(keys){
      var st = stats();
      ED.run('LAYTRANS: dịch ' + keys.length + ' layer', function(){
        tableOps(d, function(){ keys.forEach(function(s){ var t = map[s], L = d.layerMap[t.name] || addLayer(d, [t.name, t.color, t.lt, t.lw, t.desc]); Object.assign(L, { color:t.color, lt:t.lt, lw:t.lw, tr:t.tr, ps:t.ps }); });
          if (LTO.blocks) Object.keys(d.blocks).forEach(function(n){ d.blocks[n].ents.forEach(function(e){ if (e.layer !== '0' && map[e.layer]) e.layer = map[e.layer].name; }); });
          keys.forEach(function(s){ if (s === map[s].name || s === '0' || s === 'Defpoints') return; d.layers = d.layers.filter(function(l){ return l.name !== s; }); delete d.layerMap[s]; if (d.clayer === s) d.clayer = map[s].name; }); });
        allEnts(d).forEach(function(o){ var t = map[o.e.layer]; if (!t) return; ED.mod(o.e, function(z){ z.layer = t.name; if (LTO.color) delete z.color; if (LTO.lt) delete z.lt; if (LTO.transp) delete z.transp; }); });
      });
      A().refreshPalettes(); A().renderRibbon();
      TC.toast('Đã dịch <b>' + TC.n(keys.length) + '</b> layer, ' + TC.n(st.top + st.blk) + ' đối tượng theo ' + esc(src) + '.' + (LTO.log ? ' Nhật ký: <span class="mono">' + esc(fname(d)) + '.log</span>.' : ''), { log:false, act:{ label:'Hoàn tác', fn:function(){ ED.undo(1); } } });
      TC.cmdline.msg('LAYTRANS: đã dịch ' + keys.length + ' layer (' + keys.slice(0, 4).map(function(s){ return esc(s) + ' → ' + esc(map[s].name); }).join(', ') + (keys.length > 4 ? '…' : '') + ').');
    }
    $('#lt-q', el).addEventListener('input', function(){ q = this.value; render(); });
    $('#lt-f', el).addEventListener('keydown', function(e){ var ls = srcList().map(function(l){ return l.name; }); if (!ls.length) return; var i = ls.indexOf(anchor || ls[0]); if (e.key === 'ArrowDown' || e.key === 'ArrowUp'){ e.preventDefault(); i = TC.clamp(i + (e.key === 'ArrowDown' ? 1 : -1), 0, ls.length - 1); pickSrc(ls[i], e.shiftKey ? e : null); var rr = $('[data-lf="' + ls[i].replace(/"/g, '') + '"]', el); if (rr) rr.scrollIntoView({ block:'nearest' }); } });
    $('#lt-t', el).addEventListener('keydown', function(e){ var ls = Object.keys(T); if (e.key === 'ArrowDown' || e.key === 'ArrowUp'){ e.preventDefault(); var i = TC.clamp(ls.indexOf(tsel) + (e.key === 'ArrowDown' ? 1 : -1), 0, ls.length - 1); tsel = ls[i]; render(); } else if (e.key === 'Enter' && fsel.length && tsel){ e.preventDefault(); $('#lt-map', el).click(); } });
    render(); var f0 = fsel[0] && $('[data-lf="' + fsel[0].replace(/"/g, '') + '"]', el); if (f0) f0.scrollIntoView({ block:'nearest' });
    return dlg;
  };
  IMPL.LAYTRANS = function(){ SC.layTrans(); };

  /* ---------- chỉ báo chuẩn trên thanh trạng thái (bản vẽ có gắn .dws) ---------- */
  TC.on('statusrender', function(){
    var d = D(), sb = $('#sb'), gpu = sb && $('#sb-gpu', sb); if (!d || !gpu || A().active === 'start' || !pro() || !d.std || !d.std.files.length || d.std.notify === 'off') return;
    var n = dwsProblems(d).filter(function(x){ return !x.ign; }).length, tip = n ? TC.n(n) + ' vấn đề so với ' + d.std.files.join(', ') + '. Bấm để kiểm tra chuẩn.' : 'Bản vẽ khớp ' + d.std.files.join(', ') + '.';
    gpu.insertAdjacentHTML('beforebegin', n && d.std.notify !== 'icon' ? '<button class="sb-chip warn" type="button" id="sb-std" data-tip="Chuẩn CAD" data-cmd="CHECKSTANDARDS" data-desc="' + esc(tip) + '">' + ic('shield') + 'Chuẩn: ' + TC.n(n) + '</button>' : '<button class="sbt" type="button" id="sb-std" aria-label="Chuẩn CAD" data-tip="Chuẩn CAD" data-cmd="CHECKSTANDARDS" data-desc="' + esc(tip) + '">' + ic('shield') + '</button>');
  });
  document.addEventListener('click', function(e){ if (e.target.closest('#sb-std')) TC.cmd.run('CHECKSTANDARDS'); });
  // về tab Start (không còn bản vẽ): đóng các panel nổi
  TC.on('title', function(){ if (A().active !== 'start') return; if (GP.dlg) GP.dlg.close(); if (CK.dlg) CK.dlg.close(); });

  IMPL.TCCHECK = function(ctx){ var d = D(); ckOpen('tcvn'); ctx.out('Kiểm tra ' + esc(d.name) + ' theo ' + Object.keys(SETS).filter(function(k){ return CKS.sets[k]; }).map(function(k){ return SETS[k][0]; }).join(', ') + '.'); };
  IMPL.CHECKSTANDARDS = function(ctx){ var d = D(), S = stdOf(d); if (!S.files.length){ ctx.out('Bản vẽ chưa gắn tệp chuẩn. Gắn tệp .dws trong Cấu hình chuẩn (STANDARDS).'); } ckOpen('dws'); };

  // danh mục: dán phần khảo sát có lỗi vào vùng trống dưới khung A3, mở panel và chọn vấn đề đầu tiên
  OPEN['UI-I08'] = function(){
    var d = SC.cat.MOD(); if (A().active !== 'd1'){ A().openDoc('d1'); d = SC.cat.MOD(); } demo(d); TC.cad.clearSel(); var S = gcOf(d); S.scope = 'all'; S.ids = null; S.filter = 'all'; S.sel = null; GP.set = false; GP.all = false;
    setTimeout(function(){ TC.cmd.run('TCCLEANUP'); setTimeout(function(){ if (GP.dlg) SC.zoomBeside(demoBB(), GP.dlg.el); }, 80); }, 60);
  };
  // danh mục: bản vẽ có phần khảo sát dán vào và gắn sẵn AnPhu-KT.dws; mở Kiểm tra TCVN (tab .dws, STANDARDS, LAYTRANS dùng cùng khung)
  OPEN['UI-I09'] = function(){
    var d = SC.cat.MOD(); if (A().active !== 'd1'){ A().openDoc('d1'); d = SC.cat.MOD(); } demo(d); TC.cad.clearSel(); var T = stdOf(d); if (!T.files.length) T.files = ['AnPhu-KT.dws']; var C = ckOf(d); C.filter = 'all'; C.sel = null; CK.mode = 'tcvn';
    if (CK.dlg) CK.dlg.close(); setTimeout(function(){ TC.cmd.run('TCCHECK'); A().renderStatus(); setTimeout(function(){ var S = ckOf(d); if (CK.dlg && S.sel) ckAct('zoom', S.sel); }, 1100); }, 60);
    TC.toast('Cùng khung: tab Tệp chuẩn .dws (CHECKSTANDARDS), Cấu hình chuẩn (STANDARDS), Dịch layer (LAYTRANS). Chỉ báo “Chuẩn” trên thanh trạng thái mở kiểm tra chuẩn.', { type:'info', log:false, ms:9000 });
  };
})();
