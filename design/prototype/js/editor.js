/* TrionCAD prototype · lõi bản biên tập Pro (Đợt 3): hoàn tác, biến đổi, grip, nhập động, lưu và đóng, clipboard (07.1 §5, §9.5–9.8; UI-A12, B02, B03). */
(function(){
  'use strict';
  var TC = window.TC, $ = TC.$, $$ = TC.$$, esc = TC.esc, ic = TC.ic, G = TC.geo, IMPL = TC.cmd.impl;
  function A(){ return TC.app; } function V(){ return TC.app.view; } function D(){ return TC.app.doc(); }
  var ED = TC.ed = {};
  TC.S.mirrtext = 0; TC.S.polarInc = TC.load('polarInc', 90);

  /* ================= đối tượng ================= */
  function clean(e){ var o = {}; for (var k in e) if (k[0] !== '_') o[k] = e[k]; return JSON.parse(JSON.stringify(o)); }
  ED.clean = clean;
  function listOf(d, e){ if (d.ents.indexOf(e) >= 0) return d.ents; for (var i = 0; i < d.layouts.length; i++) if (d.layouts[i].ents.indexOf(e) >= 0) return d.layouts[i].ents; return null; }
  ED.listOf = listOf;
  ED.spaceList = function(d){ d = d || D(); return d.space === 'model' || d.activeVp ? d.ents : d.layouts[d.space].ents; };
  ED.clayer = function(d){ d = d || D(); if (!d.clayer || !d.layerMap[d.clayer]) d.clayer = d.layerMap['KT-TUONG'] ? 'KT-TUONG' : '0'; return d.clayer; };
  ED.cur = function(d){ d = d || D(); return d.cur || (d.cur = { color:null, lt:null, lw:null, tstyle:'Standard', th:250, dimstyle:'TCVN-1-100', mlstyle:'Standard' }); };
  // thuộc tính hiện hành cho đối tượng mới (UI-C01 khi không chọn gì)
  ED.make = function(o, d){ d = d || D(); var c = ED.cur(d), e = { layer:ED.clayer(d) }; if (c.color != null) e.color = c.color; if (c.lt) e.lt = c.lt; if (c.lw != null) e.lw = c.lw; if (ED.override) Object.assign(e, ED.override); return Object.assign(e, o); };
  TC.on('cmdend', function(){ ED.override = null; });
  ED.editable = function(ids, ctx){ var d = D(), ok = [], lk = 0; ids.forEach(function(id){ var e = d.byId[id]; if (!e) return; if ((d.layerMap[e.layer] || {}).lck) lk++; else ok.push(id); }); if (lk && ctx) ctx.out(TC.n(lk) + ' đối tượng nằm trên layer bị khóa, không sửa được.'); return ok; };

  /* ================= hoàn tác theo nhóm lệnh ================= */
  var cur = null;
  ED.begin = function(label){ if (cur){ cur.depth++; return; } cur = { label:label, ops:[], depth:0 }; };
  function log(op){ if (cur) cur.ops.push(op); else { ED.begin('Sửa'); cur.ops.push(op); ED.commit(); } }
  ED.add = function(e, list){ var d = D(); list = list || ED.spaceList(d); e.id = ++d.seq; if (list !== d.ents) e._paper = true; var i = list.length; list.push(e); TC.prepEnt(d, e); log({ k:'add', e:e, list:list, i:i }); d.lastEnt = e.id; return e; };
  ED.del = function(e){ var d = D(), list = listOf(d, e); if (!list) return; var i = list.indexOf(e); list.splice(i, 1); delete d.byId[e.id]; d.sel = d.sel.filter(function(id){ return id !== e.id; }); log({ k:'del', e:e, list:list, i:i }); };
  ED.mod = function(e, fn){ var b = clean(e); fn(e); TC.prepEnt(D(), e); log({ k:'mod', e:e, b:b, a:clean(e) }); return e; };
  ED.order = function(e, to){ var d = D(), list = listOf(d, e), from = list.indexOf(e); list.splice(from, 1); list.splice(to, 0, e); log({ k:'ord', e:e, list:list, from:from, to:to }); };
  // thay đổi không phải đối tượng (layer, kiểu, viewport): chụp trước, gọi hàm trả về sau khi đổi
  ED.state = function(get, set){ var b = JSON.parse(JSON.stringify(get())); return function(){ log({ k:'st', set:set, b:b, a:JSON.parse(JSON.stringify(get())) }); }; };
  ED.mark = function(){ return cur ? cur.ops.length : 0; };
  ED.back = function(mk){ if (!cur) return; var d = D(); while (cur.ops.length > mk) revert(d, cur.ops.pop()); TC.docExt(d); V().draw(); };
  ED.commit = function(label){ if (!cur) return; if (cur.depth){ cur.depth--; return; } var g = cur; cur = null; if (label) g.label = label; if (!g.ops.length) return; var d = D(); d.undo = d.undo || []; d.undo.push(g); if (d.undo.length > 300) d.undo.shift(); d.redo = []; changed(d); };
  // fn chạy ngay (đồng bộ) để biến gán bên trong dùng được liền sau; fn trả promise thì nhóm mở tới khi xong
  ED.run = function(label, fn){
    ED.begin(label); var r;
    try { r = fn(); } catch (err){ ED.commit(); return Promise.reject(err); }
    if (!r || typeof r.then !== 'function'){ ED.commit(); return Promise.resolve(r); }
    return r.then(function(x){ ED.commit(); return x; }, function(err){ ED.commit(); throw err; });
  };
  function assign(d, e, snap){ Object.keys(e).forEach(function(k){ if (k[0] !== '_' && k !== 'id') delete e[k]; }); Object.assign(e, JSON.parse(JSON.stringify(snap))); TC.prepEnt(d, e); }
  function revert(d, op){
    if (op.k === 'add'){ op.list.splice(op.list.indexOf(op.e), 1); delete d.byId[op.e.id]; }
    else if (op.k === 'del'){ op.list.splice(op.i, 0, op.e); TC.prepEnt(d, op.e); }
    else if (op.k === 'mod') assign(d, op.e, op.b);
    else if (op.k === 'ord'){ op.list.splice(op.list.indexOf(op.e), 1); op.list.splice(op.from, 0, op.e); }
    else if (op.k === 'st') op.set(JSON.parse(JSON.stringify(op.b)));
  }
  function forward(d, op){
    if (op.k === 'add'){ op.list.splice(op.i, 0, op.e); TC.prepEnt(d, op.e); }
    else if (op.k === 'del'){ op.list.splice(op.list.indexOf(op.e), 1); delete d.byId[op.e.id]; }
    else if (op.k === 'mod') assign(d, op.e, op.a);
    else if (op.k === 'ord'){ op.list.splice(op.list.indexOf(op.e), 1); op.list.splice(op.to, 0, op.e); }
    else if (op.k === 'st') op.set(JSON.parse(JSON.stringify(op.a)));
  }
  function recount(d){ var c = {}; var w = function(l){ l.forEach(function(e){ c[e.layer] = (c[e.layer] || 0) + 1; }); }; w(d.ents); d.layouts.forEach(function(L){ w(L.ents); }); d.layers.forEach(function(l){ l.count = c[l.name] || 0; }); }
  function changed(d){ d.dirty = true; d.sel = d.sel.filter(function(id){ return d.byId[id]; }); recount(d); TC.docExt(d); V().draw(); TC.emit('dirty', d); TC.emit('sel'); TC.emit('layers'); }
  ED.changed = changed;
  ED.undo = function(n){ var d = D(); if (!d) return 0; d.undo = d.undo || []; d.redo = d.redo || []; var k = 0, names = [];
    while (k < (n || 1) && d.undo.length){ var g = d.undo.pop(); for (var i = g.ops.length - 1; i >= 0; i--) revert(d, g.ops[i]); d.redo.push(g); names.push(g.label); k++; }
    if (k){ changed(d); TC.cmdline.msg('Hoàn tác: ' + names.map(function(x){ return '<b>' + esc(x) + '</b>'; }).join(', ')); } else TC.cmdline.msg('Không còn gì để hoàn tác.'); return k; };
  ED.redo = function(n){ var d = D(); if (!d) return 0; d.redo = d.redo || []; var k = 0, names = [];
    while (k < (n || 1) && d.redo.length){ var g = d.redo.pop(); g.ops.forEach(function(op){ forward(d, op); }); d.undo.push(g); names.push(g.label); k++; }
    if (k){ changed(d); TC.cmdline.msg('Làm lại: ' + names.map(function(x){ return '<b>' + esc(x) + '</b>'; }).join(', ')); } else TC.cmdline.msg('Không còn gì để làm lại.'); return k; };
  IMPL.U = function(){ ED.undo(1); };
  IMPL.REDO = function(){ ED.redo(1); };
  // danh sách hoàn tác nhiều bước trên QAT (07.1 §6, UI-A04)
  ED.undoMenu = function(b, redo){
    var d = D(), list = ((redo ? d.redo : d.undo) || []).slice(-12).reverse(), r = b.getBoundingClientRect();
    if (!list.length){ TC.menu([{ label:redo ? 'Không có bước để làm lại' : 'Không có bước để hoàn tác', dis:true }], r.left, r.bottom + 2); return; }
    TC.menu([{ h:redo ? 'Làm lại tới' : 'Hoàn tác tới' }].concat(list.map(function(g, i){ return { label:esc(g.label) + (i ? ' <span class="faint">(' + (i + 1) + ' bước)</span>' : ''), act:function(){ (redo ? ED.redo : ED.undo)(i + 1); } }; })), r.left, r.bottom + 2, { returnFocus:b, minWidth:220 });
  };

  /* ================= biến đổi hình học ================= */
  var T = ED.T = {
    move:function(dx, dy){ return { f:function(p){ return [p[0] + dx, p[1] + dy]; }, rot:0, k:1, m:[1, 0, 0, 1, dx, dy] }; },
    rot:function(c, deg){ var r = deg * Math.PI / 180, co = Math.cos(r), si = Math.sin(r); return { f:function(p){ var x = p[0] - c[0], y = p[1] - c[1]; return [c[0] + x * co - y * si, c[1] + x * si + y * co]; }, rot:deg, k:1, m:[co, si, -si, co, c[0] - c[0] * co + c[1] * si, c[1] - c[0] * si - c[1] * co] }; },
    sc:function(c, k){ return { f:function(p){ return [c[0] + (p[0] - c[0]) * k, c[1] + (p[1] - c[1]) * k]; }, rot:0, k:k, m:[k, 0, 0, k, c[0] * (1 - k), c[1] * (1 - k)] }; },
    mir:function(a, b){ var th = Math.atan2(b[1] - a[1], b[0] - a[0]), c2 = Math.cos(2 * th), s2 = Math.sin(2 * th); return { f:function(p){ var x = p[0] - a[0], y = p[1] - a[1]; return [a[0] + x * c2 + y * s2, a[1] + x * s2 - y * c2]; }, rot:0, k:1, mir:true, th:th * 180 / Math.PI, m:[c2, s2, s2, -c2, a[0] - a[0] * c2 - a[1] * s2, a[1] - a[0] * s2 + a[1] * c2] }; }
  };
  function angT(a, t){ return t.mir ? 2 * t.th - a : a + t.rot; }
  function polar(c, r, deg){ var a = deg * Math.PI / 180; return [c[0] + r * Math.cos(a), c[1] + r * Math.sin(a)]; }
  ED.polar = polar;
  function sweep(a0, a1){ return ((a1 - a0) % 360 + 360) % 360; }
  ED.sweep = sweep;
  ED.xform = function(e, t){
    var f = t.f;
    switch (e.t){
      case 'line': e.p1 = f(e.p1); e.p2 = f(e.p2); break;
      case 'pline': case 'spline': e.pts = e.pts.map(f); if (t.mir && e.bulges) e.bulges = e.bulges.map(function(b){ return -b; }); if (e.w) e.w *= t.k; break;
      case 'circle': e.c = f(e.c); e.r *= t.k; break;
      case 'arc': case 'dima': { e.c = f(e.c); e.r *= t.k; if (t.mir){ var a0 = angT(e.a1, t), a1 = angT(e.a0, t); e.a0 = a0; e.a1 = a1; } else { e.a0 += t.rot; e.a1 += t.rot; } break; }
      case 'ellipse': e.c = f(e.c); e.rx *= t.k; e.ry *= t.k; e.rot = angT(e.rot || 0, t); break;
      case 'text': case 'attdef': e.p = f(e.p); e.h *= t.k; if (t.mir){ if (TC.S.mirrtext) e.rot = angT(e.rot || 0, t); else if (Math.abs(Math.cos(t.th * Math.PI / 180)) < .5) e.al = e.al === 'r' ? 'l' : !e.al || e.al === 'l' ? 'r' : e.al; } else e.rot = (e.rot || 0) + t.rot; break;
      case 'mtext': { var hz = t.mir && Math.abs(Math.cos(t.th * Math.PI / 180)) < .5; e.p = f(hz ? [e.p[0] + e.w, e.p[1]] : e.p); e.w *= t.k; e.h *= t.k; if (!t.mir) e.rot = (e.rot || 0) + t.rot; break; }
      case 'mleader': e.pts = e.pts.map(f); e.h *= t.k; break;
      case 'dim': {
        if (e.dir === 'a'){ e.p1 = f(e.p1); e.p2 = f(e.p2); e.off = (e.off || 0) * t.k * (t.mir ? -1 : 1); break; }
        var h = e.dir === 'h', dl = f(h ? [e.p1[0], e.pos] : [e.pos, e.p1[1]]), o0 = f([0, 0]), ux = f([1, 0]); ux = [ux[0] - o0[0], ux[1] - o0[1]];
        e.p1 = f(e.p1); e.p2 = f(e.p2);
        if (Math.abs(ux[1]) < 1e-9) e.pos = h ? dl[1] : dl[0];
        else if (Math.abs(ux[0]) < 1e-9){ e.dir = h ? 'v' : 'h'; e.pos = e.dir === 'h' ? dl[1] : dl[0]; }
        else { var L = G.dist(e.p1, e.p2) || 1, nx = -(e.p2[1] - e.p1[1]) / L, ny = (e.p2[0] - e.p1[0]) / L; e.dir = 'a'; e.off = (dl[0] - e.p1[0]) * nx + (dl[1] - e.p1[1]) * ny; delete e.pos; }
        delete e.tdx; delete e.tdy; break; }
      case 'dimr': e.c = f(e.c); e.r *= t.k; e.ang = angT(e.ang || 45, t); break;
      case 'insert': e.p = f(e.p); e.sx *= t.k; e.sy *= t.k; if (t.mir){ e.rot = 2 * t.th - (e.rot || 0); e.sy = -e.sy; } else e.rot = (e.rot || 0) + t.rot; break;
      case 'hatch': e.loops = e.loops.map(function(l){ return l.map(f); }); e.sc = (e.sc || 1) * t.k; e.ang = angT(e.ang || 0, t); break;
      case 'image': e.p = f(e.p); e.w *= t.k; e.h *= t.k; if (!t.mir) e.rot = (e.rot || 0) + t.rot; break;
      case 'point': e.p = f(e.p); break;
      case 'xline': case 'ray': { var z0 = f([0, 0]), dd = f(e.dir), dx = dd[0] - z0[0], dy = dd[1] - z0[1], L2 = Math.hypot(dx, dy) || 1; e.p = f(e.p); e.dir = [dx / L2, dy / L2]; break; }
    }
    return e;
  };
  // bóng đối tượng khi xem trước: sao chép rồi biến đổi (dùng khi cần đúng hình, như đối xứng chữ)
  ED.ghost = function(ids, t){ var d = D(); return ids.slice(0, 400).map(function(id){ return TC.prepEnt(d, ED.xform(clean(d.byId[id]), t), true); }); };
  // xem trước nhanh: vẽ lại nguyên tố gốc qua ma trận affine (di chuyển, xoay, tỉ lệ)
  ED.xprev = function(ids, t){ var d = D(); return { k:'xents', list:ids.map(function(id){ return d.byId[id]; }).filter(Boolean), m:t.m }; };
  ED.temp = function(list, o){ var d = D(); return Object.assign({ k:'ents', list:list.map(function(e){ return e._p ? e : TC.prepEnt(d, e, true); }) }, o || {}); };
  ED.bbox = function(ids){ var d = D(), b = [Infinity, Infinity, -Infinity, -Infinity]; ids.forEach(function(id){ var e = d.byId[id]; if (!e || !e._bb) return; b[0] = Math.min(b[0], e._bb[0]); b[1] = Math.min(b[1], e._bb[1]); b[2] = Math.max(b[2], e._bb[2]); b[3] = Math.max(b[3], e._bb[3]); }); return b; };
  ED.apply = function(ids, t, copy){ var d = D(), out = []; ids.forEach(function(id){ var e = d.byId[id]; if (!e) return; if (copy){ out.push(ED.add(ED.xform(clean(e), t), listOf(d, e)).id); } else { ED.mod(e, function(x){ ED.xform(x, t); }); out.push(id); } }); return out; };

  // đường tròn qua 3 điểm; cung 3 điểm theo chiều đi qua điểm giữa
  function circum(a, b, c){ var dd = 2 * (a[0] * (b[1] - c[1]) + b[0] * (c[1] - a[1]) + c[0] * (a[1] - b[1])); if (Math.abs(dd) < 1e-9) return null; var a2 = a[0] * a[0] + a[1] * a[1], b2 = b[0] * b[0] + b[1] * b[1], c2 = c[0] * c[0] + c[1] * c[1]; return [(a2 * (b[1] - c[1]) + b2 * (c[1] - a[1]) + c2 * (a[1] - b[1])) / dd, (a2 * (c[0] - b[0]) + b2 * (a[0] - c[0]) + c2 * (b[0] - a[0])) / dd]; }
  ED.circum = circum;
  ED.arc3 = function(a, m, b){ var c = circum(a, m, b); if (!c) return null; var r = G.dist(c, a), A0 = G.ang(c, a), AM = G.ang(c, m), A1 = G.ang(c, b); return G.inArc(AM, A0, A1) ? { c:c, r:r, a0:A0, a1:A1 } : { c:c, r:r, a0:A1, a1:A0 }; };

  /* ================= chọn một đối tượng trong lệnh ================= */
  ED.pick = function(ctx, text, o){
    o = o || {};
    return ctx.point(text, { pick:true, opts:o.opts, def:o.def, hidden:o.hidden, onMove:o.onMove, dyn:false }).then(function(r){
      if (!r || r.enter || r.kw || r.num != null) return r;
      var v = V(), s = r.s || v.mToS(r.p) || v.toS(r.p), id = v.hitTest(s[0], s[1]), e = id && D().byId[id];
      var bad = !e ? 'Không có đối tượng tại đây' : o.types && o.types.indexOf(e.t) < 0 ? (o.bad || 'Đối tượng này không dùng được cho lệnh') : (D().layerMap[e.layer] || {}).lck && !o.locked ? 'Đối tượng nằm trên layer bị khóa' : null;
      if (bad){ TC.ctip($('#cv'), s[0], s[1], bad); return ED.pick(ctx, text, o); }
      r.e = e; r.s = s; return r;
    });
  };

  /* ================= grip (07.1 §9.5) ================= */
  var GR = ED.grip = { hover:null, hot:null, list:[], menu:null, t:0 };
  function mid(a, b){ return [(a[0] + b[0]) / 2, (a[1] + b[1]) / 2]; }
  function plSegMid(e, i){ var a = e.pts[i], b = e.pts[(i + 1) % e.pts.length], B = (e.bulges || [])[i] || 0; if (!B) return mid(a, b); var c = G.dist(a, b), s = B * c / 2, m = mid(a, b), ux = (b[0] - a[0]) / c, uy = (b[1] - a[1]) / c; return [m[0] + uy * s, m[1] - ux * s]; }
  ED.plSegMid = plSegMid;
  function cen(l){ var x = 0, y = 0; l.forEach(function(q){ x += q[0]; y += q[1]; }); return [x / l.length, y / l.length]; }
  function vpOf(d, e){ var f = null; d.layouts.forEach(function(L){ L.vports.forEach(function(q){ if (q.id === e.vp) f = q; }); }); return f; }
  function dimPts(e){
    if (e.dir === 'a'){ var L = G.dist(e.p1, e.p2) || 1, nx = -(e.p2[1] - e.p1[1]) / L, ny = (e.p2[0] - e.p1[0]) / L, of = e.off || 0; return { d1:[e.p1[0] + nx * of, e.p1[1] + ny * of], d2:[e.p2[0] + nx * of, e.p2[1] + ny * of], n:[nx, ny] }; }
    var h = e.dir === 'h'; return { d1:h ? [e.p1[0], e.pos] : [e.pos, e.p1[1]], d2:h ? [e.p2[0], e.pos] : [e.pos, e.p2[1]] };
  }
  ED.dimPts = dimPts;
  function gripsOf(d, e){
    var g = [], add = function(p, key, kind, x){ g.push(Object.assign({ p:p, key:key, kind:kind || 'sq', e:e }, x || {})); };
    switch (e.t){
      case 'line': add(e.p1, 'p1'); add(e.p2, 'p2'); add(mid(e.p1, e.p2), 'mid'); break;
      case 'pline': e.pts.forEach(function(p, i){ add(p, 'v' + i, 'sq', { vi:i, multi:'vtx' }); }); for (var i = 0, n = e.closed ? e.pts.length : e.pts.length - 1; i < n; i++) add(plSegMid(e, i), 's' + i, 'rect', { si:i, multi:'seg' }); break;
      case 'spline': e.pts.forEach(function(p, i){ add(p, 'v' + i, 'sq', { vi:i, multi:'spl' }); }); break;
      case 'circle': add(e.c, 'c', 'circ'); [0, 90, 180, 270].forEach(function(a){ add(polar(e.c, e.r, a), 'q' + a); }); break;
      case 'arc': add(polar(e.c, e.r, e.a0), 'a0'); add(polar(e.c, e.r, e.a1), 'a1'); add(polar(e.c, e.r, e.a0 + sweep(e.a0, e.a1) / 2), 'am', 'sq', { multi:'arc' }); add(e.c, 'c', 'circ'); break;
      case 'ellipse': add(e.c, 'c', 'circ'); [0, 180].forEach(function(a){ add(polar(e.c, e.rx, (e.rot || 0) + a), 'rx' + a); }); [90, 270].forEach(function(a){ add(polar(e.c, e.ry, (e.rot || 0) + a), 'ry' + a); }); break;
      case 'text': case 'attdef': case 'insert': case 'point': add(e.p, 'p'); break;
      case 'mtext': add(e.p, 'p'); add([e.p[0] + e.w, e.p[1] - e.h * .6], 'w', 'tri'); break;
      case 'mleader': add(e.pts[0], 'm0'); add(e.pts[1], 'm1'); break;
      case 'dim': { var q = dimPts(e); add(e.p1, 'p1', 'sq', { multi:'dim' }); add(e.p2, 'p2', 'sq', { multi:'dim' }); add(q.d1, 'd1', 'sq', { multi:'dim' }); add(q.d2, 'd2', 'sq', { multi:'dim' }); var tp = (e._p || []).filter(function(x){ return x.k === 'text'; })[0]; if (tp) add(tp.p, 'tx', 'sq', { multi:'dim' }); break; }
      case 'dimr': add(e.c, 'c'); add(polar(e.c, e.r, e.ang || 45), 'q'); break;
      case 'dima': add(e.c, 'c'); add(polar(e.c, e.r, e.a0 + sweep(e.a0, e.a1) / 2), 'r'); break;
      case 'hatch': add(cen(e.loops[0]), 'hc', 'circ', { multi:'hatch' }); break;
      case 'image': { var c4 = (e._p || []).filter(function(x){ return x.k === 'img'; })[0]; if (c4) c4.pts.forEach(function(p, i){ add(p, 'k' + i); }); break; }
      case 'xline': case 'ray': add(e.p, 'p'); add([e.p[0] + e.dir[0] * 1500, e.p[1] + e.dir[1] * 1500], 'dir', 'circ'); break;
      case 'vpframe': { var vp = vpOf(d, e); if (!vp) break; [[vp.x, vp.y], [vp.x + vp.w, vp.y], [vp.x + vp.w, vp.y + vp.h], [vp.x, vp.y + vp.h]].forEach(function(p, i){ add(p, 'k' + i); }); add([vp.x + vp.w / 2, vp.y + vp.h / 2], 'vc', 'sq'); add([vp.x + vp.w + 4, vp.y + vp.h / 2], 'vs', 'tri', { multi:'vps' }); break; }
    }
    return g;
  }
  ED.gripsOf = gripsOf;
  function gripOn(){ var d = D(); if (!d || TC.isViewer() || d.cmp || !d.sel.length || d.sel.length > 1000) return false; var c = TC.cmdline.current(); return !c || c === 'GRIPEDIT'; }
  function scrOf(e, p){ var v = V(); return e._paper ? v.toS(p) : v.mToS(p); }
  function same(a, b){ return a && b && a.e === b.e && a.key === b.key; }
  ED.drawGrip = function(ctx, g, st){ drawGrip(ctx, g, st); };
  function drawGrip(ctx, g, st){
    var s = [Math.round(g.s[0]) + .5, Math.round(g.s[1]) + .5], z = st === 'cold' ? 9 : 11;
    ctx.fillStyle = TC.cssv(st === 'hot' ? '--c-grip-hot' : st === 'hover' ? '--c-grip-hover' : '--c-grip-cold'); ctx.strokeStyle = st === 'hot' ? '#FFFFFF' : TC.cssv('--c-grip-border'); ctx.lineWidth = st === 'cold' ? 1 : st === 'hover' ? 1.5 : 2.5; ctx.setLineDash([]);
    ctx.beginPath();
    if (g.kind === 'rect') ctx.rect(s[0] - z * .8, s[1] - z * .32, z * 1.6, z * .64);
    else if (g.kind === 'tri'){ ctx.moveTo(s[0] + z * .6, s[1]); ctx.lineTo(s[0] - z * .45, s[1] - z * .55); ctx.lineTo(s[0] - z * .45, s[1] + z * .55); ctx.closePath(); }
    else if (g.kind === 'circ') ctx.arc(s[0], s[1], z / 2, 0, 7);
    else ctx.rect(s[0] - z / 2, s[1] - z / 2, z, z);
    ctx.fill(); ctx.stroke();
  }
  TC.overlays = TC.overlays || [];
  TC.overlays.push(function(ctx, v){
    GR.list = [];
    if (gripOn()){ var d = D(); d.sel.forEach(function(id){ var e = d.byId[id]; if (!e) return; gripsOf(d, e).forEach(function(g){ var s = scrOf(e, g.p); if (s){ g.s = s; GR.list.push(g); } }); }); }
    GR.list.forEach(function(g){ drawGrip(ctx, g, same(GR.hot, g) ? 'hot' : same(GR.hover, g) ? 'hover' : 'cold'); });
    // cung chỉ góc của nhập kích thước
    if (DY.arcS){ var a = DY.arcS; ctx.strokeStyle = TC.cssv('--c-track'); ctx.setLineDash([2, 3]); ctx.lineWidth = 1; ctx.beginPath(); ctx.moveTo(a.c[0], a.c[1]); ctx.lineTo(a.c[0] + 52, a.c[1]); ctx.stroke(); ctx.beginPath(); ctx.arc(a.c[0], a.c[1], 40, 0, -a.ang * Math.PI / 180, a.ang > 0); ctx.stroke(); ctx.setLineDash([]); }
  });
  var cad = TC.cad;
  cad.hook.grip = function(m){
    var best = null, bd = 8;
    GR.list.forEach(function(g){ var dd = Math.hypot(g.s[0] - m[0], g.s[1] - m[1]); if (dd < bd){ bd = dd; best = g; } });
    var was = GR.hover; GR.hover = best;
    if (!best){ if (GR.menu && document.querySelector('.menu') && Math.hypot(GR.menu[0] - m[0], GR.menu[1] - m[1]) > 70){ TC.closeMenus(0); GR.menu = null; } clearTimeout(GR.t); return false; }
    if (!same(was, best)){ clearTimeout(GR.t); if (best.multi) GR.t = setTimeout(function(){ if (same(GR.hover, best) && !TC.cmdline.busy()) gripMenu(best); }, 420); if (was) V().drawOver(); }
    var tip = $('.snap-tip'); if (tip) tip.remove();
    return true;
  };
  cad.hook.gripDown = function(m, e){ if (!GR.hover || TC.cmdline.busy()) return false; var g = GR.hover; clearTimeout(GR.t); TC.closeMenus(0); GR.menu = null; TC.cmd.run('GRIPEDIT', g); return true; };
  // menu grip đa năng khi rê lên grip (07.1 §9.5; kích thước: UI-F05)
  function gripMenu(g){
    var r = $('#cv').getBoundingClientRect(), it = [], run = function(op, mode){ return function(){ TC.cmd.run('GRIPEDIT', Object.assign({}, g, { op:op, mode:mode || 0 })); }; };
    if (g.multi === 'vtx') it = [{ label:'Kéo giãn', act:run() }, { label:'Thêm đỉnh', act:run('addv') }, { label:'Bớt đỉnh', act:function(){ removeVertex(g); } }];
    else if (g.multi === 'seg'){ var bul = (g.e.bulges || [])[g.si]; it = [{ label:'Kéo giãn', act:run() }, { label:'Thêm đỉnh', act:run('addv') }, bul ? { label:'Chuyển thành đoạn thẳng', act:function(){ toLine(g); } } : { label:'Chuyển thành cung', act:run('toarc') }]; }
    else if (g.multi === 'spl') it = [{ label:'Kéo giãn', act:run() }, { label:'Thêm điểm', act:run('addv') }, { label:'Bớt điểm', act:function(){ removeVertex(g); } }];
    else if (g.multi === 'arc') it = [{ label:'Kéo giãn', act:run() }, { label:'Bán kính', act:run('radius') }];
    else if (g.multi === 'hatch') it = [{ label:'Kéo giãn', act:run() }, { label:'Gốc', act:run('origin') }, { label:'Góc', act:run('hang') }, { label:'Tỉ lệ', act:run('hscale') }];
    else if (g.multi === 'dim') it = [{ label:'Kéo giãn', act:run() }, { label:'Nối tiếp dim', act:function(){ TC.app.doc().lastDim = g.e.id; TC.cmd.run('DIMCONTINUE'); } }, { label:'Dim cùng gốc', act:function(){ TC.app.doc().lastDim = g.e.id; TC.cmd.run('DIMBASELINE'); } }, { label:'Lật mũi tên', act:function(){ ED.mod(g.e, function(x){ x.flip = !x.flip; }); TC.cmdline.msg('Đã lật mũi tên.'); } },
      { label:'Vị trí chữ', sub:[{ label:'Đi cùng đường dim', act:run('txline') }, { label:'Chỉ di chuyển chữ', act:run('txonly') }, { label:'Di chuyển kèm đường dẫn', act:run('txlead') }, { label:'Trên đường dim, giữa', act:function(){ ED.mod(g.e, function(x){ delete x.tdx; delete x.tdy; }); } }, { label:'Đặt lại vị trí chữ', act:function(){ ED.mod(g.e, function(x){ delete x.tdx; delete x.tdy; }); } }] }];
    else if (g.multi === 'vps'){ var vp = vpOf(D(), g.e); it = [{ h:'Tỉ lệ viewport' }].concat([20, 50, 100, 200, 500].map(function(k){ return { label:'1:' + k, check:vp && Math.round(vp.scale) === k, act:function(){ setVpScale(vp, k); } }; })).concat([{ sep:1 }, { label:vp && vp.locked ? 'Mở khóa hiển thị' : 'Khóa hiển thị', act:function(){ var done = ED.state(function(){ return vp; }, function(s){ Object.assign(vp, s); }); vp.locked = !vp.locked; done(); V().draw(); } }]); }
    if (!it.length) return;
    GR.menu = g.s.slice(); TC.menu(it, r.left + g.s[0] + 10, r.top + g.s[1] + 10, { minWidth:160 });
  }
  function setVpScale(vp, k){ if (!vp) return; ED.begin('Tỉ lệ viewport'); var done = ED.state(function(){ return vp; }, function(s){ Object.assign(vp, s); TC.app.view.draw(); }); vp.scale = k; done(); ED.commit(); TC.app.renderStatus(); TC.cmdline.msg('Tỉ lệ viewport: 1:' + k); }
  ED.setVpScale = setVpScale;
  function removeVertex(g){ var e = g.e; if (e.pts.length <= 2){ TC.cmdline.msg('Polyline cần ít nhất 2 đỉnh.'); return; } ED.run('Bớt đỉnh', function(){ ED.mod(e, function(x){ x.pts.splice(g.vi, 1); if (x.bulges) x.bulges.splice(g.vi, 1); }); }); }
  function toLine(g){ ED.run('Chuyển thành đoạn thẳng', function(){ ED.mod(g.e, function(x){ x.bulges[g.si] = 0; }); }); }

  // kéo giãn một grip: sửa trực tiếp đối tượng (bản thật hoặc bản xem trước)
  function stretchGrip(e, g, p, op){
    var k = g.key, dx = p[0] - g.p[0], dy = p[1] - g.p[1], sh = function(q){ return [q[0] + dx, q[1] + dy]; };
    switch (e.t){
      case 'line': if (k === 'mid'){ e.p1 = sh(e.p1); e.p2 = sh(e.p2); } else e[k] = p; break;
      case 'pline': case 'spline':
        if (op === 'addv'){ var at = g.vi != null ? g.vi + 1 : g.si + 1; e.pts.splice(at, 0, p); if (e.bulges) e.bulges.splice(at, 0, 0); }
        else if (op === 'toarc'){ var a = e.pts[g.si], b = e.pts[(g.si + 1) % e.pts.length], c = circum(a, p, b); if (c){ var s = (b[0] - a[0]) * (p[1] - a[1]) - (b[1] - a[1]) * (p[0] - a[0]), sw = 2 * Math.asin(Math.min(1, G.dist(a, b) / (2 * G.dist(c, a)))); var far = G.dist(p, mid(a, b)) > G.dist(c, a); if (far) sw = 2 * Math.PI - sw; e.bulges = e.bulges || []; e.bulges[g.si] = Math.tan(sw / 4) * (s > 0 ? -1 : 1); } }
        else if (g.vi != null) e.pts[g.vi] = p;
        else { var n = e.pts.length; e.pts[g.si] = sh(e.pts[g.si]); e.pts[(g.si + 1) % n] = sh(e.pts[(g.si + 1) % n]); }
        break;
      case 'circle': if (k === 'c') e.c = p; else e.r = Math.max(1, G.dist(e.c, p)); break;
      case 'arc': if (k === 'c') e.c = p; else if (op === 'radius'){ e.r = Math.max(1, G.dist(e.c, p)); } else { var P0 = polar(e.c, e.r, e.a0), P1 = polar(e.c, e.r, e.a1), PM = polar(e.c, e.r, e.a0 + sweep(e.a0, e.a1) / 2); if (k === 'a0') P0 = p; else if (k === 'a1') P1 = p; else PM = p; var r3 = ED.arc3(P0, PM, P1); if (r3) Object.assign(e, r3); } break;
      case 'ellipse': if (k === 'c') e.c = p; else if (k.indexOf('rx') === 0) e.rx = Math.max(1, G.dist(e.c, p)); else e.ry = Math.max(1, G.dist(e.c, p)); break;
      case 'text': case 'attdef': case 'insert': case 'point': e.p = p; break;
      case 'mtext': if (k === 'p') e.p = p; else e.w = Math.max(e.h * 4, p[0] - e.p[0]); break;
      case 'mleader': e.pts[k === 'm0' ? 0 : 1] = p; break;
      case 'dim': {
        if (k === 'p1' || k === 'p2'){ e[k] = p; break; }
        if (k === 'tx'){ if (op === 'txline'){ moveDimLine(e, p); } else { var t0 = (e._p || []).filter(function(x){ return x.k === 'text'; })[0]; if (t0){ e.tdx = (e.tdx || 0) + dx; e.tdy = (e.tdy || 0) + dy; if (op === 'txlead') e.lead = 1; } } break; }
        moveDimLine(e, p); break; }
      case 'dimr': if (k === 'c') e.c = p; else e.ang = G.ang(e.c, p); break;
      case 'dima': if (k === 'c') e.c = p; else e.r = Math.max(100, G.dist(e.c, p)); break;
      case 'hatch': if (op === 'hang') e.ang = Math.round(G.ang(g.p, p)); else if (op === 'hscale') e.sc = Math.max(.1, Math.round(G.dist(g.p, p) / 50) / 2); else if (op === 'origin') e.org = p; else e.loops = e.loops.map(function(l){ return l.map(sh); }); break;
      case 'image': { if (k === 'k0'){ e.p = p; break; } var ar = e.h / e.w, w = Math.max(50, Math.abs(p[0] - e.p[0])); e.w = w; e.h = w * ar; break; }
      case 'xline': case 'ray': if (k === 'p') e.p = p; else { var L = G.dist(e.p, p) || 1; e.dir = [(p[0] - e.p[0]) / L, (p[1] - e.p[1]) / L]; } break;
    }
    return e;
  }
  function moveDimLine(e, p){ if (e.dir === 'a'){ var L = G.dist(e.p1, e.p2) || 1, nx = -(e.p2[1] - e.p1[1]) / L, ny = (e.p2[0] - e.p1[0]) / L; e.off = (p[0] - e.p1[0]) * nx + (p[1] - e.p1[1]) * ny; } else e.pos = e.dir === 'h' ? p[1] : p[0]; }
  ED.moveDimLine = moveDimLine;
  function vpStretch(vp, k, p, g){ if (k === 'vc'){ vp.x += p[0] - g.p[0]; vp.y += p[1] - g.p[1]; return; } var x0 = vp.x, y0 = vp.y, x1 = vp.x + vp.w, y1 = vp.y + vp.h; if (k === 'k0'){ x0 = p[0]; y0 = p[1]; } if (k === 'k1'){ x1 = p[0]; y0 = p[1]; } if (k === 'k2'){ x1 = p[0]; y1 = p[1]; } if (k === 'k3'){ x0 = p[0]; y1 = p[1]; } vp.x = Math.min(x0, x1); vp.y = Math.min(y0, y1); vp.w = Math.max(20, Math.abs(x1 - x0)); vp.h = Math.max(20, Math.abs(y1 - y0)); }

  // sửa bằng grip: Space/Enter luân phiên Kéo giãn → Di chuyển → Xoay → Tỉ lệ → Đối xứng
  var MODES = [['Kéo giãn', 'ST', 'Điểm kéo giãn'], ['Di chuyển', 'MO', 'Điểm đích'], ['Xoay', 'RO', 'Góc xoay'], ['Tỉ lệ', 'SC', 'Hệ số tỉ lệ'], ['Đối xứng', 'MI', 'Điểm thứ hai của trục đối xứng']];
  IMPL.GRIPEDIT = function(ctx, g){
    if (!g || !g.e) return;
    var d = D(), ids = ED.editable(d.sel.slice(), ctx), mi = g.mode || 0, base = g.p, copy = false, marks = [], refAng = 0, refLen = null;
    if (!ids.length) return;
    var bb = ED.bbox(ids), half = Math.max(1, Math.hypot(bb[2] - bb[0], bb[3] - bb[1]) / 2);
    if (g.e.t === 'vpframe') mi = 0;
    GR.hot = g; GR.hover = null; ED.begin('Grip');
    function xfFor(r, p){
      if (mi === 1) return T.move(p[0] - base[0], p[1] - base[1]);
      if (mi === 2) return T.rot(base, (r && r.num != null ? r.num : G.ang(base, p)) - refAng);
      if (mi === 3) return T.sc(base, r && r.num != null ? r.num : Math.max(.05, Math.round(G.dist(base, p) / (refLen || half) * 20) / 20));
      return T.mir(base, p);
    }
    function preview(p, temp){
      if (mi === 0){
        if (g.e.t === 'vpframe'){ var vp0 = vpOf(d, g.e), c = { x:vp0.x, y:vp0.y, w:vp0.w, h:vp0.h }; vpStretch(c, g.key, p, g); temp.push({ k:'poly', pts:[[c.x, c.y], [c.x + c.w, c.y], [c.x + c.w, c.y + c.h], [c.x, c.y + c.h]], closed:true, paper:true }); return; }
        temp.push(ED.temp([stretchGrip(clean(g.e), g, p, g.op)]));
        return;
      }
      var t = xfFor(null, p); temp.push(mi === 4 ? ED.temp(ED.ghost(ids, t)) : ED.xprev(ids, t));
    }
    function apply(r){
      var p = r.p, mk = ED.mark();
      if (mi === 0){
        if (g.e.t === 'vpframe'){ var vp = vpOf(d, g.e), done = ED.state(function(){ return vp; }, function(s){ Object.assign(vp, s); TC.prepEnt(d, g.e); V().draw(); }); vpStretch(vp, g.key, p, g); TC.prepEnt(d, g.e); done(); }
        else if (copy) ED.add(stretchGrip(clean(g.e), g, p, g.op), listOf(d, g.e));
        else ED.mod(g.e, function(x){ stretchGrip(x, g, p, g.op); });
      } else ED.apply(ids, xfFor(r, p || base), copy);
      marks.push(mk);
      if (!copy){ var ng = gripsOf(d, g.e).filter(function(x){ return x.key === g.key; })[0]; if (ng) base = g.p = ng.p; }
    }
    function step(){
      var m = MODES[mi], opts = [['Điểm gốc', 'B'], ['Sao chép', 'C'], ['Hoàn tác', 'U'], ['Thoát', 'X']];
      if (mi === 2 || mi === 3) opts.splice(2, 0, ['Tham chiếu', 'R']);
      ctx.out('** ' + m[0].toUpperCase() + (copy ? ' (nhiều bản)' : '') + ' **', 'muted');
      return ctx.point(m[2], { base:base, opts:opts, hidden:MODES.map(function(x){ return [x[0], x[1]]; }), num:mi === 2 || mi === 3, dyn:mi === 2 ? 'ang' : mi === 3 ? false : undefined, onMove:preview }).then(function(r){
        if (r.enter){ mi = (mi + 1) % 5; if (g.e.t === 'vpframe') mi = mi === 1 ? 1 : 0; return step(); }
        var hm = MODES.map(function(x){ return x[1]; }).indexOf(r.kw); if (hm >= 0){ mi = hm; return step(); }
        if (r.kw === 'X') return;
        if (r.kw === 'B') return ctx.point('Điểm gốc').then(function(q){ if (q.p) base = q.p; return step(); });
        if (r.kw === 'C'){ copy = true; return step(); }
        if (r.kw === 'U'){ if (marks.length) ED.back(marks.pop()); else ctx.out('Không còn bước để hoàn tác.'); return step(); }
        if (r.kw === 'R'){ if (mi === 2) return ctx.val('Góc tham chiếu', '0').then(function(v){ refAng = Number(v) || 0; return step(); }); return ctx.val('Độ dài tham chiếu', TC.d(half)).then(function(v){ refLen = Number(v) || half; return step(); }); }
        apply(r);
        if (copy) return step();
      });
    }
    return step().then(fin, function(err){ fin(); throw err; });
    function fin(){ GR.hot = null; ED.commit(MODES[mi][0] + ' bằng grip'); V().drawOver(); }
  };
  // chuột phải trên grip nóng (07.1 §9.9)
  cad.hook.ctx = function(m, e){
    if (TC.cmdline.current() !== 'GRIPEDIT') return false;
    var S = function(k){ return function(){ TC.cmdline.submit(k); }; }, pr = TC.cmdline.prompt(), curMode = pr ? MODES.map(function(x){ return x[2]; }).indexOf(pr.text) : -1;
    TC.menu([{ label:'Enter', kbd:'Enter', act:function(){ TC.cmdline.enter(); } }, { sep:1 }].concat(MODES.map(function(x, i){ return { label:x[0], check:i === curMode, act:S(x[1]) }; })).concat([{ sep:1 }, { label:'Điểm gốc', act:S('B') }, { label:'Sao chép', act:S('C') }, { label:'Tham chiếu', dis:curMode !== 2 && curMode !== 3, act:S('R') }, { label:'Hoàn tác', kbd:'Ctrl+Z', act:S('U') }, { sep:1 }, { label:'Thuộc tính', icon:'props', act:function(){ TC.app.showPalette('props'); } }, { label:'Thoát', kbd:'Esc', act:S('X') }]), e.clientX, e.clientY);
    return true;
  };

  /* ================= nhập động DYN (07.1 §9.7) ================= */
  var DY = TC.dyn = { lock:{}, field:0, txt:'', pr:null };
  DY.on = function(){ return !!(TC.S.toggles.dyn && !TC.isViewer() && TC.cmdline.busy() && TC.app.active !== 'start'); };
  DY.prompt = function(pr){ if (pr !== DY.pr){ DY.lock = {}; DY.field = 0; DY.txt = ''; } DY.pr = pr; render(); };
  DY.typed = function(v){ DY.txt = v; render(); };
  function num(v){ return /^-?\d*\.?\d+$/.test(v); }
  function fmode(){
    var pr = DY.pr, p = DY.p; if (!pr || !p || DY.pick || cad.mode() !== 'point' || pr.dyn === false) return null;
    var b = DY.base, an = function(a){ return (a % 360 + 360) % 360; };
    if (pr.dyn === 'ang' && b) return { kind:'ang', f:['A'], cur:{ A:an(G.ang(b, p)) } };
    if (pr.dyn === 'wh' && b) return { kind:'wh', f:['W', 'H'], cur:{ W:p[0] - b[0], H:p[1] - b[1] } };
    if (b) return { kind:'dim', f:['L', 'A'], cur:{ L:G.dist(b, p), A:an(G.ang(b, p)) } };
    return { kind:'pt', f:['X', 'Y'], cur:{ X:p[0], Y:p[1] } };
  }
  DY.tab = function(inp){ var md = DY.on() && fmode(); if (!md || md.f.length < 2) return false; var v = inp.value.trim(); if (v && !num(v)) return false; if (v) DY.lock[md.f[DY.field]] = Number(v); inp.value = ''; DY.txt = ''; DY.field = (DY.field + 1) % md.f.length; render(); return true; };
  DY.compose = function(v){
    var md = DY.on() && fmode(); if (!md) return v;
    var locked = Object.keys(DY.lock).length; if (v && !num(v)) return v; if (!locked && !(DY.field > 0 && v)) return v;
    var val = {}; md.f.forEach(function(k, i){ val[k] = DY.lock[k] != null ? DY.lock[k] : i === DY.field && v !== '' ? Number(v) : md.cur[k]; });
    DY.lock = {}; DY.field = 0; DY.txt = '';
    var r = function(x){ return Math.round(x * 1e4) / 1e4; };
    return md.kind === 'pt' ? '#' + r(val.X) + ',' + r(val.Y) : md.kind === 'wh' ? '@' + r(val.W) + ',' + r(val.H) : md.kind === 'ang' ? String(r(val.A)) : '@' + r(val.L) + '<' + r(val.A);
  };
  DY.down = function(){ var pr = DY.pr; if (!DY.on() || !pr || !pr.opts || !pr.opts.length) return false; var m = V().mouse || [120, 120], r = $('#cv').getBoundingClientRect(); TC.menu(pr.opts.map(function(o){ return { label:esc(o[0]) + ' <span class="faint">· ' + esc(o[1]) + '</span>', act:function(){ TC.cmdline.submit(o[1]); } }; }), r.left + m[0] + 16, r.top + m[1] + 44, { minWidth:180 }); return true; };
  cad.hook.move = function(m, p, base, pick){ DY.p = p; DY.base = base; DY.pick = pick; render(); };
  cad.hook.end = function(){ DY.p = null; DY.base = null; DY.arcS = null; render(); };
  var dynEl = null;
  function render(){
    var cv = $('#cv'); if (!cv) return;
    if (!dynEl || !cv.contains(dynEl)){ dynEl = TC.el('<div class="dynl" aria-hidden="true"></div>'); cv.appendChild(dynEl); }
    var v = V(), m = v && v.mouse, pr = DY.pr; DY.arcS = null;
    if (!DY.on() || !m || !pr){ dynEl.innerHTML = ''; return; }
    var md = fmode(), sc = function(q){ return v.mToS(q) || v.toS(q); }, kwTyped = DY.txt && !num(DY.txt) && !/^[@#]/.test(DY.txt);
    var box = function(k, i, x, y){ var foc = i === DY.field, lk = DY.lock[k] != null, val = lk ? DY.lock[k] : foc && DY.txt && !kwTyped ? DY.txt : TC.d(md.cur[k]); return '<span class="dyn dyn-f' + (foc ? ' is-focus' : '') + (lk ? ' is-lock' : '') + '"' + (x != null ? ' style="left:' + Math.round(x) + 'px;top:' + Math.round(y) + 'px"' : '') + '>' + (md.kind === 'dim' && k === 'A' || md.kind === 'ang' ? '' : '<i>' + ({ X:'X', Y:'Y', W:'Rộng', H:'Cao', L:'' }[k] || '') + '</i>') + '<b>' + esc(val) + (k === 'A' ? '°' : '') + '</b>' + (lk ? ic('lock', 'xs') : '') + '</span>'; };
    var h = '<div class="dyn-row" style="left:' + (m[0] + 18) + 'px;top:' + (m[1] + 20) + 'px"><span class="dyn dyn-pr">' + esc(pr.text) + (pr.opts && pr.opts.length ? '<span class="dk" title="↓ để xem tùy chọn">' + ic('down', 'xs') + '</span>' : '') + (kwTyped || (!md && DY.txt) ? '<b class="mono">' + esc(DY.txt) + '</b>' : '') + '</span>' + (md && (md.kind === 'pt' || md.kind === 'ang') ? md.f.map(function(k, i){ return box(k, i); }).join('') : '') + '</div>';
    if (md && md.kind === 'dim'){
      var sb = sc(DY.base), sp = sc(DY.p), mx = (sb[0] + sp[0]) / 2, my = (sb[1] + sp[1]) / 2, L = Math.hypot(sp[0] - sb[0], sp[1] - sb[1]) || 1, nx = (sp[1] - sb[1]) / L, ny = -(sp[0] - sb[0]) / L;
      if (L > 30){ h += box('L', 0, mx + nx * 16 - 28, my + ny * 16 - 10); var an = md.cur.A; DY.arcS = { c:sb, ang:an > 180 ? an - 360 : an }; var ma = -(DY.arcS.ang / 2) * Math.PI / 180; h += box('A', 1, sb[0] + Math.cos(ma) * 58 - 18, sb[1] + Math.sin(ma) * 58 - 10); }
    }
    if (md && md.kind === 'wh'){ var b0 = sc(DY.base), p0 = sc(DY.p); h += box('W', 0, (b0[0] + p0[0]) / 2 - 30, Math.max(b0[1], p0[1]) + 6) + box('H', 1, Math.max(b0[0], p0[0]) + 8, (b0[1] + p0[1]) / 2 - 10); }
    dynEl.innerHTML = h;
  }
  TC.on('viewchg', function(){ if (DY.pr) render(); });

  /* ================= lưu, lưu thành, đóng khi chưa lưu (UI-B02, B03) ================= */
  var FORMATS = [['DWG 2018', 'AutoCAD 2018 trở lên và TrionCAD', 1], ['DWG 2013', 'AutoCAD 2013 trở lên', 1], ['DWG 2010', 'AutoCAD 2010 trở lên', 1], ['DWG 2007', 'AutoCAD 2007 trở lên', 1], ['DWG 2004', 'AutoCAD 2004 trở lên', 1], ['DWG 2000', 'AutoCAD 2000 trở lên', 1], ['DXF 2018', 'Mọi phần mềm CAD đọc được DXF 2018', 1], ['DXF 2013', 'Mọi phần mềm CAD đọc được DXF 2013', 1], ['DXF 2010', 'Đọc được bằng phần mềm CAD cũ', 1], ['Mẫu bản vẽ (DWT)', 'Dùng làm mẫu khi tạo bản vẽ mới', 1], ['DWG R14 · R12', 'Cho phần mềm rất cũ (P2)', 0]];
  ED.FORMATS = FORMATS;
  var saveSet = TC.load('saveset', { fmt:'DWG 2018', noticeOff:false });
  function lossy(fmt){ var y = Number((fmt.match(/\d{4}/) || [2018])[0]); var out = []; if (y < 2018) out.push('Thumbnail độ phân giải cao được lưu dạng nhỏ.'); if (y < 2013) out.push('Kích thước liên kết (DIMASSOC) lưu thành kích thước thường, vẫn đúng hình.'); if (y < 2010) out.push('Độ trong suốt của 3 layer bị bỏ; đối tượng hiện đặc như bình thường.', 'Hatch có độ trong suốt lưu thành hatch thường.'); if (y < 2007) out.push('Đối tượng chú thích theo tỉ lệ (annotative) thành đối tượng thường ở tỉ lệ hiện hành.', 'Bảng (TABLE) lưu thành block không sửa được dạng bảng.'); return out; }
  function doSave(d, fmt, name, path){
    return new Promise(function(res){
      var go = function(){ d.dirty = false; d.ver = /^DWG/.test(fmt) ? fmt : d.ver; d.savedBy = 'TrionCAD Pro'; if (name) d.name = name; if (path) d.path = path; d.savedOnce = true; d.modified = TC.date() + ' ' + new Date().toTimeString().slice(0, 5); TC.emit('dirty', d); TC.cmdline.msg('Đã lưu <b>' + esc(d.name) + '</b> (' + esc(fmt) + ').'); TC.toast('Đã lưu <b>' + esc(d.name) + '</b>', { log:false, ms:2500 }); res(true); };
      // lần đầu lưu DWG: thông báo một lần (UI-B02)
      if (/^DWG/.test(fmt) && !saveSet.noticeOff && !TC.S._noticeShown){
        TC.S._noticeShown = true;
        var dlg = TC.dialog({ id:'tvnotice', title:'Lưu DWG bằng TrionCAD', code:'UI-B02', w:520, help:false,
          body:'<div class="stack"><div class="hrow" style="align-items:flex-start;gap:10px;flex-wrap:nowrap">' + ic('info', 'st') + '<p>Khi mở tệp này, AutoCAD có thể báo tệp <b>không được lưu bởi ứng dụng Autodesk</b>. Tệp vẫn mở và dùng bình thường; dữ liệu bản vẽ không thay đổi.</p></div><label class="chk"><input type="checkbox" id="tv-off"> Không hiện lại</label></div>',
          foot:'<button class="btn ghost" type="button" id="tv-learn">' + ic('ext') + 'Tìm hiểu</button><span class="grow"></span><button class="btn primary" type="button" id="tv-ok">Đã hiểu, lưu tệp</button>', onClose:function(){ if ($('#tv-off', dlg.el) && $('#tv-off', dlg.el).checked){ saveSet.noticeOff = true; TC.save('saveset', saveSet); } } });
        $('#tv-learn', dlg.el).addEventListener('click', function(){ TC.openHelp('dwg-notice'); });
        $('#tv-ok', dlg.el).addEventListener('click', function(){ dlg.close(true); go(); });
        return;
      }
      go();
    });
  }
  IMPL.QSAVE = function(){ var d = D(); if (!d) return; if (d.readonly){ TC.toast('Bản vẽ đang mở chỉ đọc. Lưu thành tệp khác để giữ thay đổi.', { type:'warn', log:false, act:{ label:'Lưu thành…', fn:function(){ TC.cmd.run('SAVEAS'); } } }); return; } if (!d.savedOnce && d.ver !== saveSet.fmt && /^DWG (2000|2004|2007|2010|2013)/.test(d.ver)) return ED.saveAs(d); return doSave(d, d.ver || saveSet.fmt); };
  IMPL.SAVEAS = function(){ return ED.saveAs(D()); };
  // hộp thoại Lưu thành: khung chọn tệp của hệ điều hành, thêm định dạng, gợi ý tương thích (UI-B02)
  ED.saveAs = function(d, fmt0){
    if (!d) return Promise.resolve(false);
    return new Promise(function(res){
      var fmt = fmt0 || (d.ver === 'DWG 2018' ? saveSet.fmt : 'DWG 2018'), dir = d.path.replace(/\\[^\\]+$/, ''), base = d.name.replace(/\.\w+$/, '');
      var files = [['Banve-01.dwg', '16/10/2026', '4,8 MB'], ['Banve-01-thue-ngoai.dwg', '02/10/2026', '4,6 MB'], ['Mat-dung.dwg', '28/09/2026', '2,1 MB'], ['Chi-tiet-cau-thang.dwg', '25/09/2026', '2,2 MB']];
      var dlg = TC.dialog({ id:'saveas', title:'Lưu bản vẽ thành', cmd:'SAVEAS', code:'UI-B02', w:760, h:520, flush:true,
        body:'<div class="sv"><div class="sv-bar"><button class="ib" type="button" aria-label="Thư mục trên">' + ic('up') + '</button><span class="sv-path mono">' + esc(dir) + '</span><label class="search" style="width:200px">' + ic('search') + '<input placeholder="Tìm trong thư mục" aria-label="Tìm trong thư mục"></label></div>' +
          '<div class="sv-main"><div class="tree sv-tree">' + ['Truy cập nhanh', 'D:\\DuAn', 'NhaO-2T', 'KienTruc', 'KetCau', '\\\\SRV-01\\DuAn', 'Desktop', 'Tài liệu'].map(function(t, i){ return '<button class="tn" type="button" aria-selected="' + (i === 3) + '" style="padding-left:' + (i > 1 && i < 5 ? 18 : 6) + 'px">' + ic(i === 0 ? 'pin' : 'folder', 's12') + esc(t) + '</button>'; }).join('') + '</div>' +
          '<div class="tblwrap"><table class="tbl"><thead><tr><th>Tên</th><th>Ngày sửa</th><th class="num">Kích thước</th></tr></thead><tbody>' + files.map(function(f){ return '<tr data-sf="' + esc(f[0]) + '"><td>' + ic('file', 's12') + ' ' + esc(f[0]) + '</td><td>' + f[1] + '</td><td class="num">' + f[2] + '</td></tr>'; }).join('') + '</tbody></table></div>' +
          '<div class="sv-prev"><canvas width="220" height="150"></canvas><div class="faint" style="font-size:var(--fs-xs)">Thumbnail lưu kèm tệp</div></div></div>' +
          '<div class="sv-form"><div class="frow" style="--lab-w:120px"><label class="lab" for="sv-name">Tên tệp</label><input class="inp" id="sv-name" value="' + esc(base) + '" spellcheck="false"></div>' +
          '<div class="frow" style="--lab-w:120px"><label class="lab" for="sv-fmt">Định dạng</label><div class="stack" style="gap:3px"><select class="inp" id="sv-fmt">' + FORMATS.map(function(f){ return '<option' + (f[0] === fmt ? ' selected' : '') + (f[2] ? '' : ' disabled') + '>' + esc(f[0]) + (f[0] === 'DWG 2018' ? ' (khuyến nghị)' : '') + '</option>'; }).join('') + '</select><span class="fmsg hint" id="sv-hint"></span></div></div>' +
          '<div class="frow" style="--lab-w:120px"><span class="lab"></span><label class="chk"><input type="checkbox" id="sv-def"' + (saveSet.fmt === fmt ? ' checked' : '') + '> Đặt làm định dạng mặc định</label></div>' +
          '<div id="sv-dwt" hidden><div class="frow" style="--lab-w:120px"><label class="lab" for="sv-desc">Mô tả mẫu</label><input class="inp" id="sv-desc" placeholder="Ví dụ: Mẫu kiến trúc A3, tỉ lệ 1:100"></div><div class="frow" style="--lab-w:120px;margin-top:6px"><span class="lab">Đơn vị đo</span>' + TC.screens.seg('svu', ['Mét', 'Anh'], 'Mét') + '</div></div>' +
          '<div id="sv-loss" class="note warn" hidden></div></div></div>',
        foot:'<span class="faint" style="font-size:var(--fs-s)">Hộp thoại chọn tệp của hệ điều hành, thêm phần định dạng và gợi ý tương thích của TrionCAD.</span><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="sv-go">Lưu</button>', onClose:function(r){ res(!!r); } });
      TC.thumb($('.sv-prev canvas', dlg.el), d, 'model');
      function upd(){ var f = $('#sv-fmt', dlg.el).value.replace(' (khuyến nghị)', ''), row = FORMATS.filter(function(x){ return x[0] === f; })[0]; fmt = f; $('#sv-hint', dlg.el).innerHTML = ic('info', 'xs') + 'Mở được bằng ' + esc(row[1]) + '.'; $('#sv-dwt', dlg.el).hidden = f.indexOf('DWT') < 0; var lo = lossy(f), box = $('#sv-loss', dlg.el); box.hidden = !lo.length; box.innerHTML = lo.length ? '<b>Lưu sang ' + esc(f) + ' sẽ làm giảm một số tính năng:</b><ul>' + lo.map(function(x){ return '<li>' + esc(x) + '</li>'; }).join('') + '</ul>' : ''; $('#sv-go', dlg.el).textContent = lo.length ? 'Vẫn lưu ' + f : 'Lưu'; }
      $('#sv-fmt', dlg.el).addEventListener('change', upd);
      dlg.el.addEventListener('click', function(e){ var r = e.target.closest('[data-sf]'); if (r){ $$('[data-sf]', dlg.el).forEach(function(x){ x.classList.toggle('is-sel', x === r); }); $('#sv-name', dlg.el).value = r.dataset.sf.replace(/\.\w+$/, ''); } });
      $('#sv-go', dlg.el).addEventListener('click', function(){
        var nm = $('#sv-name', dlg.el).value.trim(); if (!nm || /[\\/:*?"<>|]/.test(nm)){ $('#sv-name', dlg.el).classList.add('err'); TC.ctip(dlg.el, 140, 360, 'Tên tệp không được trống hoặc chứa \\ / : * ? " < > |'); return; }
        var ext = /DXF/.test(fmt) ? '.dxf' : /DWT/.test(fmt) ? '.dwt' : '.dwg', full = nm + ext, exists = files.some(function(f){ return f[0].toLowerCase() === full.toLowerCase(); }) && full !== d.name;
        if ($('#sv-def', dlg.el).checked){ saveSet.fmt = fmt; TC.save('saveset', saveSet); }
        var finish = function(){ dlg.close(true); doSave(d, fmt, full, dir + '\\' + full); TC.app.renderFileTabs(); };
        if (exists) TC.confirm({ title:'Ghi đè tệp', msg:'<b>' + esc(full) + '</b> đã có trong thư mục. Thay thế tệp đó?', ok:'Thay thế', danger:true }).then(function(y){ if (y) finish(); }); else finish();
      });
      upd();
    });
  };
  // đóng khi chưa lưu: một tệp hoặc nhiều tệp (UI-B03)
  var close0 = TC.app.closeDoc;
  TC.app.closeDoc = function(id, quiet){
    var d = TC.app.docs.filter(function(x){ return x.id === id; })[0];
    if (!d || !d.dirty || quiet || TC.isViewer()) return close0(id, quiet);
    var dlg = TC.dialog({ id:'b03', title:'TrionCAD', code:'UI-B03', w:440, help:false, body:'<div class="hrow" style="gap:12px;align-items:flex-start;flex-wrap:nowrap">' + ic('warn', 'st') + '<div class="stack" style="gap:4px"><p>Lưu thay đổi vào <b>' + esc(d.name) + '</b>?</p><p class="faint" style="font-size:var(--fs-s)">' + TC.n((d.undo || []).length) + ' thay đổi chưa lưu · lần lưu trước ' + esc(d.modified || '') + '</p></div></div>',
      foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn" type="button" id="b3-no">Không lưu</button><button class="btn primary" type="button" id="b3-yes">Lưu</button>' });
    $('#b3-no', dlg.el).addEventListener('click', function(){ dlg.close(true); d.dirty = false; close0(id); });
    $('#b3-yes', dlg.el).addEventListener('click', function(){ dlg.close(true); Promise.resolve(IMPL.QSAVE()).then(function(){ if (!d.dirty) close0(id); }); });
  };
  TC.app.closeDocs = function(ids){
    var dirty = TC.app.docs.filter(function(x){ return ids.indexOf(x.id) >= 0 && x.dirty; });
    if (!dirty.length || TC.isViewer()){ ids.forEach(function(id){ close0(id, true); }); return; }
    var dlg = TC.dialog({ id:'b03m', title:'Đóng nhiều bản vẽ', code:'UI-B03', w:520, help:false,
      body:'<div class="stack"><p>Các bản vẽ sau có thay đổi chưa lưu:</p><div class="tblwrap"><table class="tbl"><thead><tr><th class="ctr" style="width:30px"></th><th>Tệp</th><th>Thư mục</th><th class="num">Thay đổi</th></tr></thead><tbody>' + dirty.map(function(x){ return '<tr><td class="ctr"><input type="checkbox" checked data-sv="' + x.id + '" aria-label="Lưu ' + esc(x.name) + '"></td><td>' + esc(x.name) + '</td><td class="faint">' + esc(x.path.replace(/\\[^\\]+$/, '')) + '</td><td class="num">' + TC.n((x.undo || []).length) + '</td></tr>'; }).join('') + '</tbody></table></div></div>',
      foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn" type="button" id="b3m-no">Không lưu</button><button class="btn primary" type="button" id="b3m-yes">Lưu các tệp đã chọn</button>' });
    $('#b3m-no', dlg.el).addEventListener('click', function(){ dlg.close(true); ids.forEach(function(id){ var x = TC.app.docs.filter(function(q){ return q.id === id; })[0]; if (x) x.dirty = false; close0(id, true); }); });
    $('#b3m-yes', dlg.el).addEventListener('click', function(){ var keep = $$('[data-sv]', dlg.el).filter(function(c){ return c.checked; }).map(function(c){ return c.dataset.sv; }); dlg.close(true); keep.forEach(function(id){ var x = TC.app.docs.filter(function(q){ return q.id === id; })[0]; if (x){ x.dirty = false; x.savedOnce = true; } }); if (keep.length) TC.toast('Đã lưu ' + keep.length + ' bản vẽ.', { log:false }); ids.forEach(function(id){ var x = TC.app.docs.filter(function(q){ return q.id === id; })[0]; if (x) x.dirty = false; close0(id, true); }); });
  };
  IMPL.CLOSE = function(){ var d = D(); if (d) TC.app.closeDoc(d.id); };

  /* ================= clipboard ================= */
  var CLIP = ED.clip = { ents:null, base:null };
  function toClip(ids, base){ var d = D(), bb = ED.bbox(ids); CLIP.ents = ids.map(function(id){ return clean(d.byId[id]); }); CLIP.base = base || [bb[0], bb[1]]; CLIP.from = d.name; }
  IMPL.COPYCLIP = function(ctx){ return ctx.sel().then(function(ids){ if (!ids.length) return; toClip(ids); ctx.out('Đã sao chép ' + TC.n(ids.length) + ' đối tượng vào clipboard.'); }); };
  IMPL.CUTCLIP = function(ctx){ return ctx.sel().then(function(ids){ ids = ED.editable(ids, ctx); if (!ids.length) return; toClip(ids); ED.run('CUTCLIP', function(){ ids.forEach(function(id){ ED.del(D().byId[id]); }); }); ctx.out('Đã cắt ' + TC.n(ids.length) + ' đối tượng vào clipboard.'); }); };
  IMPL.COPYBASE = function(ctx){ return ctx.sel().then(function(ids){ if (!ids.length) return; return ctx.point('Điểm gốc').then(function(r){ if (!r.p) return; toClip(ids, r.p); TC.cad.clearSel(); ctx.out('Đã sao chép ' + TC.n(ids.length) + ' đối tượng kèm điểm gốc.'); }); }); };
  function pasteAt(ctx, label, fn){
    if (!CLIP.ents){ ctx.out('Clipboard trống. Dùng Ctrl+C để sao chép đối tượng trước.'); return; }
    var d = D(), src = CLIP.ents.map(function(e){ return TC.prepEnt(d, clean(e), true); });
    return ctx.point('Điểm chèn', { onMove:function(p, temp){ temp.push({ k:'xents', list:src, m:T.move(p[0] - CLIP.base[0], p[1] - CLIP.base[1]).m }); } }).then(function(r){ if (!r.p) return; return ED.run(label, function(){ fn(r.p); }); });
  }
  IMPL.PASTECLIP = function(ctx){ return pasteAt(ctx, 'PASTECLIP', function(p){ var t = T.move(p[0] - CLIP.base[0], p[1] - CLIP.base[1]), ids = CLIP.ents.map(function(e){ return ED.add(ED.xform(clean(e), t)).id; }); D().sel = ids; TC.cad.afterSel(); ctx.out('Đã dán ' + TC.n(ids.length) + ' đối tượng' + (CLIP.from !== D().name ? ' từ ' + esc(CLIP.from) : '') + '.'); }); };
  IMPL.PASTEORIG = function(ctx){ if (!CLIP.ents){ ctx.out('Clipboard trống.'); return; } ED.run('PASTEORIG', function(){ CLIP.ents.forEach(function(e){ ED.add(clean(e)); }); }); ctx.out('Đã dán ' + TC.n(CLIP.ents.length) + ' đối tượng về tọa độ gốc.'); };
  IMPL.PASTEBLOCK = function(ctx){ return pasteAt(ctx, 'PASTEBLOCK', function(p){ var d = D(), nm = 'A$C' + Math.random().toString(16).slice(2, 8).toUpperCase(); d.blocks[nm] = { name:nm, desc:'Dán dạng block', ents:CLIP.ents.map(function(e){ return ED.xform(clean(e), T.move(-CLIP.base[0], -CLIP.base[1])); }) }; ED.add(ED.make({ t:'insert', name:nm, p:p, sx:1, sy:1, rot:0 })); ctx.out('Đã dán thành block <b>' + nm + '</b>. Đổi tên block bằng RENAME.'); }); };
})();
