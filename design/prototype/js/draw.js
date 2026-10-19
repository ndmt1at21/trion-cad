/* TrionCAD prototype · lệnh vẽ và sửa của bản biên tập (Đợt 3). Prompt và chip tùy chọn theo 07.2 UI-E10; xem trước theo 07.1 §9.8. */
(function(){
  'use strict';
  var TC = window.TC, $ = TC.$, esc = TC.esc, G = TC.geo, IMPL = TC.cmd.impl, ED = TC.ed, T = ED.T, polar = ED.polar, sweep = ED.sweep;
  function V(){ return TC.app.view; } function D(){ return TC.app.doc(); }
  var LAST = ED.last = { r:1000, offset:500, fillet:0, chamA:500, chamB:500, poly:4, rot:0, sc:1, cloud:600, plw:0 };
  function mid(a, b){ return [(a[0] + b[0]) / 2, (a[1] + b[1]) / 2]; }
  function unit(v){ var L = Math.hypot(v[0], v[1]) || 1; return [v[0] / L, v[1] / L]; }
  function sub(a, b){ return [a[0] - b[0], a[1] - b[1]]; }
  function add2(a, v, k){ return [a[0] + v[0] * (k == null ? 1 : k), a[1] + v[1] * (k == null ? 1 : k)]; }
  function dot(a, b){ return a[0] * b[0] + a[1] * b[1]; }
  function cross(a, b){ return a[0] * b[1] - a[1] * b[0]; }
  function lerp(a, b, t){ return [a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t]; }
  function prev(e){ return ED.temp([e], { real:true }); }
  function addNow(e, list){ var x = ED.add(e, list); V().draw(); return x; }
  function lineInt(a, b, c, d){ var r = sub(b, a), s = sub(d, c), den = cross(r, s); if (Math.abs(den) < 1e-12) return null; var t = cross(sub(c, a), s) / den; return add2(a, r, t); }
  ED.lineInt = lineInt;
  function ctip(r, msg){ var s = r && (r.s || (r.p && (V().mToS(r.p) || V().toS(r.p)))) || V().mouse || [100, 100]; TC.ctip($('#cv'), s[0], s[1], msg); }

  /* ================= vẽ ================= */
  IMPL.LINE = function(ctx){
    ctx.badge('L');
    var first = null, last = null, marks = [];
    function next(){
      var o = marks.length >= 2 ? [['Đóng', 'C'], ['Hoàn tác', 'U']] : [['Hoàn tác', 'U']];
      return ctx.point(last ? 'Điểm tiếp theo' : 'Điểm đầu', { base:last, opts:last ? o : null }).then(function(r){
        if (r.enter) return;
        if (r.kw === 'U'){ if (marks.length){ var m = marks.pop(); ED.back(m.mk); last = m.from; } else ctx.out('Không còn đoạn để hoàn tác.'); return next(); }
        if (r.kw === 'C'){ if (marks.length >= 2) addNow(ED.make({ t:'line', p1:last, p2:first })); return; }
        if (!r.p) return next();
        if (last){ marks.push({ mk:ED.mark(), from:last }); addNow(ED.make({ t:'line', p1:last, p2:r.p })); } else first = r.p;
        last = r.p; return next();
      });
    }
    return ED.run('LINE', next);
  };

  // polyline: đoạn thẳng và cung nối tiếp (bulge), bề rộng toàn cục
  IMPL.PLINE = function(ctx){
    ctx.badge('PL');
    var pts = [], bul = [], arc = false, w = LAST.plw || 0, closed = false, fixA = null;
    function tangent(){ var n = pts.length; if (n < 2) return null; var a = pts[n - 2], b = pts[n - 1], B = bul[n - 2] || 0, d = unit(sub(b, a)); if (!B) return d; var h = 2 * Math.atan(B); return [d[0] * Math.cos(h) - d[1] * Math.sin(h), d[0] * Math.sin(h) + d[1] * Math.cos(h)]; }
    function bulgeDir(p, t){ var a = pts[pts.length - 1], c = sub(p, a), al = Math.atan2(cross(t, c), dot(t, c)); return Math.tan(al / 2); }
    function bulgeTo(p){ if (fixA != null) return Math.tan(fixA * Math.PI / 720); return bulgeDir(p, tangent() || [1, 0]); }
    function bulge3(a, m, b){ var u = sub(a, m), v = sub(b, m), ang = Math.atan2(Math.abs(cross(u, v)), dot(u, v)), th = 2 * (Math.PI - ang), s = cross(sub(m, a), sub(b, m)) > 0 ? 1 : -1; return s * Math.tan(th / 4); }
    function ent(p, b){ var bb = bul.slice(); if (p) bb[pts.length - 1] = b || 0; var e = ED.make({ t:'pline', pts:p ? pts.concat([p]) : pts.slice(), bulges:bb, closed:closed }); if (w) e.w = w; return e; }
    function push(p, b){ bul[pts.length - 1] = b || 0; pts.push(p); fixA = null; }
    function step(){
      var n = pts.length;
      if (!n) return ctx.point('Điểm đầu').then(function(r){ if (!r.p) return; pts.push(r.p); ctx.out('Bề rộng hiện tại của polyline là ' + TC.d(w) + '.'); return step(); });
      var o = arc ? [['Góc', 'A'], ['Tâm', 'CE'], ['Đóng', 'CL'], ['Hướng', 'D'], ['Nửa bề rộng', 'H'], ['Đoạn thẳng', 'L'], ['Bán kính', 'R'], ['Điểm thứ hai', 'S'], ['Hoàn tác', 'U'], ['Bề rộng', 'W']] : [['Cung', 'A'], ['Nửa bề rộng', 'H'], ['Độ dài', 'L'], ['Hoàn tác', 'U'], ['Bề rộng', 'W']];
      if (!arc && n >= 2) o.splice(1, 0, ['Đóng', 'C']);
      if (arc && n < 2) o = o.filter(function(x){ return x[1] !== 'CL'; });
      return ctx.point(arc ? 'Điểm cuối của cung' : 'Điểm tiếp theo', { base:pts[n - 1], opts:o, rubber:!arc, onMove:function(p, temp){ temp.push(prev(ent(p, arc ? bulgeTo(p) : 0))); } }).then(function(r){
        if (r.enter) return;
        var k = r.kw, a = pts[n - 1];
        if (k === 'A' && !arc){ arc = true; return step(); }
        if (k === 'A') return ctx.val('Góc bao của cung (dương: ngược chiều kim đồng hồ)', '90').then(function(v){ fixA = Number(v) || 90; return step(); });
        if (k === 'L' && arc){ arc = false; return step(); }
        if (k === 'L') return ctx.val('Độ dài đoạn', '1000').then(function(v){ var t = tangent() || [1, 0]; push(add2(a, t, Number(v) || 0), 0); return step(); });
        if (k === 'C' || k === 'CL'){ bul[n - 1] = arc ? bulgeTo(pts[0]) : 0; closed = true; return; }
        if (k === 'U'){ if (n > 1){ pts.pop(); bul.pop(); } else ctx.out('Không còn đoạn để hoàn tác.'); return step(); }
        if (k === 'W' || k === 'H') return ctx.val(k === 'W' ? 'Bề rộng đầu' : 'Nửa bề rộng đầu', TC.d(k === 'W' ? w : w / 2)).then(function(v){ w = (Number(v) || 0) * (k === 'H' ? 2 : 1); LAST.plw = w; return ctx.val(k === 'W' ? 'Bề rộng cuối' : 'Nửa bề rộng cuối', TC.d(k === 'W' ? w : w / 2)).then(function(){ return step(); }); });
        if (k === 'S') return ctx.point('Điểm thứ hai của cung', { base:a }).then(function(q){ if (!q.p) return step(); var m = q.p; return ctx.point('Điểm cuối của cung', { base:m, onMove:function(p, temp){ temp.push(prev(ent(p, bulge3(a, m, p)))); } }).then(function(z){ if (z.p) push(z.p, bulge3(a, m, z.p)); return step(); }); });
        if (k === 'CE') return ctx.point('Tâm của cung', { base:a }).then(function(q){ if (!q.p) return step(); var c = q.p, rr = G.dist(c, a), a0 = G.ang(c, a); var endOf = function(p){ return polar(c, rr, G.ang(c, p)); }, bOf = function(p){ return Math.tan(sweep(a0, G.ang(c, p)) * Math.PI / 720); }; return ctx.point('Điểm cuối của cung', { base:c, onMove:function(p, temp){ temp.push(prev(ent(endOf(p), bOf(p)))); } }).then(function(z){ if (z.p) push(endOf(z.p), bOf(z.p)); return step(); }); });
        if (k === 'R') return ctx.val('Bán kính cung', TC.d(LAST.r)).then(function(v){ var R = Math.abs(Number(v)) || LAST.r; LAST.r = R; var bOf = function(p){ var L = G.dist(a, p); if (L > 2 * R) return null; var th = 2 * Math.asin(L / (2 * R)), s = cross(tangent() || [1, 0], sub(p, a)) >= 0 ? 1 : -1; return s * Math.tan(th / 4); }; return ctx.point('Điểm cuối của cung', { base:a, onMove:function(p, temp){ var b = bOf(p); if (b != null) temp.push(prev(ent(p, b))); } }).then(function(z){ var b = z.p && bOf(z.p); if (b != null) push(z.p, b); else if (z.p) ctip(z, 'Dây cung dài hơn đường kính'); return step(); }); });
        if (k === 'D') return ctx.point('Hướng tiếp tuyến tại điểm đầu', { base:a }).then(function(q){ if (!q.p) return step(); var t = unit(sub(q.p, a)); return ctx.point('Điểm cuối của cung', { base:a, onMove:function(p, temp){ temp.push(prev(ent(p, bulgeDir(p, t)))); } }).then(function(z){ if (z.p) push(z.p, bulgeDir(z.p, t)); return step(); }); });
        if (r.p) push(r.p, arc ? bulgeTo(r.p) : 0);
        return step();
      });
    }
    function finish(){ if (pts.length >= 2) ED.run('PLINE', function(){ ED.add(ent()); }); }
    return step().then(finish, function(err){ finish(); throw err; });
  };

  IMPL.CIRCLE = function(ctx, pre){
    ctx.badge('C');
    var mk = function(c, r){ return ED.make({ t:'circle', c:c, r:Math.max(.01, r) }); };
    var done = function(c, r){ LAST.r = r; return ED.run('CIRCLE', function(){ ED.add(mk(c, r)); }); };
    var first = pre === '2P' || pre === '3P' || pre === 'T' || pre === 'TTT' ? Promise.resolve({ kw:pre }) : ctx.point('Tâm đường tròn', { opts:[['3 điểm', '3P'], ['2 điểm', '2P'], ['Tiếp-tiếp-bán kính', 'T']] });
    return first.then(function(r){
      if (r.kw === '2P') return ctx.point('Điểm đầu của đường kính').then(function(a){ if (!a.p) return; return ctx.point('Điểm cuối của đường kính', { base:a.p, onMove:function(p, temp){ temp.push(prev(mk(mid(a.p, p), G.dist(a.p, p) / 2))); } }).then(function(b){ if (b.p) return done(mid(a.p, b.p), G.dist(a.p, b.p) / 2); }); });
      if (r.kw === '3P' || r.kw === 'TTT') return ctx.point('Điểm thứ nhất trên đường tròn').then(function(a){ if (!a.p) return; return ctx.point('Điểm thứ hai trên đường tròn', { base:a.p }).then(function(b){ if (!b.p) return; return ctx.point('Điểm thứ ba trên đường tròn', { onMove:function(p, temp){ var c = ED.circum(a.p, b.p, p); if (c) temp.push(prev(mk(c, G.dist(c, p)))); } }).then(function(q){ if (!q.p) return; var c = ED.circum(a.p, b.p, q.p); if (c) return done(c, G.dist(c, q.p)); ctx.out('Ba điểm thẳng hàng, không tạo được đường tròn.'); }); }); });
      if (r.kw === 'T') return ttr(ctx);
      if (!r.p) return;
      var c = r.p, dia = pre === 'D';
      function ask(){ return ctx.point(dia ? 'Đường kính' : 'Bán kính', { base:c, opts:dia ? [['Bán kính', 'R']] : [['Đường kính', 'D']], def:TC.d(dia ? LAST.r * 2 : LAST.r), onMove:function(p, temp){ temp.push(prev(mk(c, G.dist(c, p) / (dia ? 2 : 1)))); } }).then(function(q){
        if (q.kw === 'D' || q.kw === 'R'){ dia = q.kw === 'D'; return ask(); }
        return done(c, q.enter ? LAST.r : G.dist(c, q.p) / (dia ? 2 : 1)); }); }
      return ask();
    });
  };
  // đường tròn tiếp xúc hai đoạn thẳng: chọn nghiệm có tiếp điểm gần điểm đã chọn
  function ttr(ctx){
    return ED.pick(ctx, 'Điểm trên đối tượng tiếp xúc thứ nhất', { types:['line'], bad:'Prototype: chọn đoạn thẳng' }).then(function(a){ if (!a.e) return;
      return ED.pick(ctx, 'Điểm trên đối tượng tiếp xúc thứ hai', { types:['line'] }).then(function(b){ if (!b.e) return;
        return ctx.val('Bán kính', TC.d(LAST.r)).then(function(v){ var r = Math.abs(Number(v)) || LAST.r, best = null, bd = Infinity;
          [-1, 1].forEach(function(s1){ [-1, 1].forEach(function(s2){ var n1 = unit([-(a.e.p2[1] - a.e.p1[1]), a.e.p2[0] - a.e.p1[0]]), n2 = unit([-(b.e.p2[1] - b.e.p1[1]), b.e.p2[0] - b.e.p1[0]]); var X = lineInt(add2(a.e.p1, n1, s1 * r), add2(a.e.p2, n1, s1 * r), add2(b.e.p1, n2, s2 * r), add2(b.e.p2, n2, s2 * r)); if (!X) return; var dd = G.dist(G.foot(X, a.e.p1, a.e.p2).q, a.p) + G.dist(G.foot(X, b.e.p1, b.e.p2).q, b.p); if (dd < bd){ bd = dd; best = X; } }); });
          if (!best){ ctx.out('Hai đường song song: không có đường tròn tiếp xúc với bán kính này.'); return; }
          LAST.r = r; return ED.run('CIRCLE', function(){ ED.add(ED.make({ t:'circle', c:best, r:r })); }); }); }); });
  }

  // cung: 3 điểm và các cách dựng ở nút split của ribbon (07.1 §7.2)
  IMPL.ARC = function(ctx, pre){
    ctx.badge('A');
    var Q = { SCE:['', 'C'], SCA:['', 'C', 'A'], SCL:['', 'C', 'L'], SEA:['', 'E', 'A'], SED:['', 'E', 'D'], SER:['', 'E', 'R'], CSE:['C'], CSA:['C', '', 'A'], CSL:['C', '', 'L'] }[pre] || [];
    function ask(text, o){ var f = Q.shift(); if (f) return Promise.resolve({ kw:f }); return ctx.point(text, o); }
    var mk = function(o){ return o && o.r > 0 ? ED.make(Object.assign({ t:'arc' }, o)) : null; };
    var pv = function(o, temp){ var e = mk(o); if (e) temp.push(prev(e)); };
    function fin(o){ if (!o || !(o.r > 0)){ ctx.out('Không tạo được cung với các điểm này.'); return; } return ED.run('ARC', function(){ ED.add(mk(o)); }); }
    function sce(s, c, e){ return { c:c, r:G.dist(c, s), a0:G.ang(c, s), a1:G.ang(c, e) }; }
    function sca(s, c, A){ var a0 = G.ang(c, s), r = G.dist(c, s); return A >= 0 ? { c:c, r:r, a0:a0, a1:a0 + A } : { c:c, r:r, a0:a0 + A, a1:a0 }; }
    function sea(s, e, A){ var th = A * Math.PI / 180, L = G.dist(s, e); if (!L || !th) return null; var r = L / 2 / Math.sin(Math.abs(th) / 2), u = unit(sub(e, s)), n = [-u[1], u[0]], h = r * Math.cos(th / 2) * (th > 0 ? 1 : -1), c = add2(mid(s, e), n, h), o = { c:c, r:r, a0:G.ang(c, s), a1:G.ang(c, e) }; if (th < 0){ var t = o.a0; o.a0 = o.a1; o.a1 = t; } return o; }
    function sed(s, e, d){ var c = sub(e, s); return sea(s, e, 2 * Math.atan2(cross(d, c), dot(d, c)) * 180 / Math.PI); }
    function ser(s, e, R){ var L = G.dist(s, e); return L > 2 * R ? null : sea(s, e, 2 * Math.asin(L / (2 * R)) * 180 / Math.PI); }
    function chordA(r, L){ return 2 * Math.asin(Math.min(1, L / (2 * r))) * 180 / Math.PI; }
    if (pre === 'CONT'){
      var d = D(), le = d.lastEnt && d.byId[d.lastEnt];
      if (!le || (le.t !== 'line' && le.t !== 'arc' && le.t !== 'pline')){ ctx.out('Chưa có đoạn thẳng hoặc cung vừa vẽ để nối tiếp.'); return; }
      var s0, dir0;
      if (le.t === 'line'){ s0 = le.p2; dir0 = unit(sub(le.p2, le.p1)); } else if (le.t === 'arc'){ s0 = polar(le.c, le.r, le.a1); dir0 = [-Math.sin(le.a1 * Math.PI / 180), Math.cos(le.a1 * Math.PI / 180)]; } else { s0 = le.pts[le.pts.length - 1]; dir0 = unit(sub(s0, le.pts[le.pts.length - 2])); }
      return ctx.point('Điểm cuối của cung (nối tiếp)', { base:s0, onMove:function(p, temp){ pv(sed(s0, p, dir0), temp); } }).then(function(z){ if (z.p) return fin(sed(s0, z.p, dir0)); });
    }
    function fromCenter(c){ return ctx.point('Điểm đầu của cung', { base:c }).then(function(s){ if (!s.p) return; return endCS(s.p, c); }); }
    function endCS(S, C){ return ask('Điểm cuối của cung', { base:C, opts:[['Góc', 'A'], ['Độ dài dây cung', 'L']], onMove:function(p, temp){ pv(sce(S, C, p), temp); } }).then(function(z){
      if (z.kw === 'A') return ctx.point('Góc bao', { base:C, num:true, dyn:'ang', onMove:function(p, temp){ pv(sca(S, C, sweep(G.ang(C, S), G.ang(C, p))), temp); } }).then(function(q){ return fin(sca(S, C, q.num != null ? q.num : sweep(G.ang(C, S), G.ang(C, q.p)))); });
      if (z.kw === 'L') return ctx.val('Độ dài dây cung', TC.d(G.dist(C, S))).then(function(v){ return fin(sca(S, C, chordA(G.dist(C, S), Number(v)))); });
      if (z.p) return fin(sce(S, C, z.p)); }); }
    return ask('Điểm đầu của cung', { opts:[['Tâm', 'C']] }).then(function(r){
      if (r.kw === 'C') return ctx.point('Tâm của cung').then(function(c){ if (c.p) return fromCenter(c.p); });
      if (!r.p) return; var S = r.p;
      return ask('Điểm thứ hai của cung', { base:S, opts:[['Tâm', 'C'], ['Điểm cuối', 'E']] }).then(function(q){
        if (q.kw === 'C') return ctx.point('Tâm của cung', { base:S }).then(function(c){ if (c.p) return endCS(S, c.p); });
        if (q.kw === 'E') return ctx.point('Điểm cuối của cung', { base:S }).then(function(e){ if (!e.p) return; var E = e.p;
          return ask('Tâm của cung', { base:S, opts:[['Góc', 'A'], ['Hướng', 'D'], ['Bán kính', 'R']], onMove:function(p, temp){ pv(sce(S, p, E), temp); } }).then(function(z){
            if (z.kw === 'A') return ctx.val('Góc bao (dương: ngược chiều kim đồng hồ)', '90').then(function(v){ return fin(sea(S, E, Number(v) || 90)); });
            if (z.kw === 'D') return ctx.point('Hướng tiếp tuyến tại điểm đầu', { base:S, onMove:function(p, temp){ pv(sed(S, E, unit(sub(p, S))), temp); } }).then(function(k){ if (k.p) return fin(sed(S, E, unit(sub(k.p, S)))); });
            if (z.kw === 'R') return ctx.val('Bán kính cung', TC.d(G.dist(S, E))).then(function(v){ return fin(ser(S, E, Math.abs(Number(v)) || G.dist(S, E))); });
            if (z.p) return fin(sce(S, z.p, E)); }); });
        if (!q.p) return; var M = q.p;
        return ctx.point('Điểm cuối của cung', { base:M, onMove:function(p, temp){ pv(ED.arc3(S, M, p), temp); } }).then(function(z){ if (z.p) return fin(ED.arc3(S, M, z.p)); });
      });
    });
  };

  // bo hoặc vát mọi đỉnh của polyline (RECTANG Bo góc, FILLET › Polyline)
  function plCorner(e, r, cham){
    var P = e.pts, n = P.length, B = e.bulges || [], out = [], bl = [], cnt = 0;
    for (var i = 0; i < n; i++){
      var p = P[i], inner = e.closed || (i > 0 && i < n - 1), pb = B[(i - 1 + n) % n] || 0, nb = B[i] || 0;
      if (!inner || pb || nb){ out.push(p); bl.push(nb); continue; }
      var a = P[(i - 1 + n) % n], b = P[(i + 1) % n], u1 = unit(sub(a, p)), u2 = unit(sub(b, p)), th = Math.acos(TC.clamp(dot(u1, u2), -1, 1));
      if (th < 1e-3 || Math.PI - th < 1e-3){ out.push(p); bl.push(0); continue; }
      var t = cham || r / Math.tan(th / 2); t = Math.min(t, G.dist(a, p) / 2, G.dist(b, p) / 2);
      out.push(add2(p, u1, t)); bl.push(cham ? 0 : Math.tan((Math.PI - th) / 4) * (cross(sub(p, a), sub(b, p)) > 0 ? 1 : -1)); out.push(add2(p, u2, t)); bl.push(0); cnt++;
    }
    e.pts = out; e.bulges = bl; return cnt;
  }
  ED.plCorner = plCorner;
  IMPL.RECTANG = function(ctx){
    ctx.badge('REC');
    var fr = LAST.rfil || 0, ch = LAST.rch || 0, wd = LAST.rw || 0, rot = 0;
    if (fr || ch || wd) ctx.out('Chế độ hiện tại: ' + (fr ? 'Bo góc = ' + TC.d(fr) : ch ? 'Vát góc = ' + TC.d(ch) : '') + (wd ? '  Bề rộng = ' + TC.d(wd) : ''));
    function rect(a, b){ var r = rot * Math.PI / 180, co = Math.cos(r), si = Math.sin(r), w = b[0] - a[0], h = b[1] - a[1]; if (rot){ var dx = b[0] - a[0], dy = b[1] - a[1]; w = dx * co + dy * si; h = -dx * si + dy * co; } var Q = function(x, y){ return [a[0] + x * co - y * si, a[1] + x * si + y * co]; }; var e = ED.make({ t:'pline', pts:[Q(0, 0), Q(w, 0), Q(w, h), Q(0, h)], closed:true }); if (wd) e.w = wd; if (fr) plCorner(e, fr); else if (ch) plCorner(e, 0, ch); return e; }
    function done(a, b){ return ED.run('RECTANG', function(){ var e = ED.add(rect(a, b)); ctx.out('Hình chữ nhật ' + TC.d(Math.abs(b[0] - a[0])) + ' × ' + TC.d(Math.abs(b[1] - a[1])) + ', diện tích ' + TC.d(Math.abs((b[0] - a[0]) * (b[1] - a[1])) / 1e6) + ' m².'); return e; }); }
    function first(){ return ctx.point('Góc thứ nhất', { opts:[['Vát góc', 'C'], ['Cao độ', 'E'], ['Bo góc', 'F'], ['Độ dày', 'T'], ['Bề rộng', 'W']] }).then(function(r){
      if (r.kw === 'F') return ctx.val('Bán kính bo góc', TC.d(fr)).then(function(v){ fr = Math.max(0, Number(v) || 0); ch = 0; LAST.rfil = fr; LAST.rch = 0; return first(); });
      if (r.kw === 'C') return ctx.val('Khoảng vát thứ nhất', TC.d(ch)).then(function(v){ ch = Math.max(0, Number(v) || 0); fr = 0; LAST.rch = ch; LAST.rfil = 0; return ctx.val('Khoảng vát thứ hai', TC.d(ch)).then(function(){ return first(); }); });
      if (r.kw === 'W') return ctx.val('Bề rộng nét', TC.d(wd)).then(function(v){ wd = Math.max(0, Number(v) || 0); LAST.rw = wd; return first(); });
      if (r.kw === 'E' || r.kw === 'T') return ctx.val(r.kw === 'E' ? 'Cao độ' : 'Độ dày', '0.00').then(function(){ return first(); });
      if (r.p) return second(r.p); }); }
    function second(a){ return ctx.point('Góc đối', { base:a, rubber:false, dyn:'wh', opts:[['Diện tích', 'A'], ['Kích thước', 'D'], ['Xoay', 'R']], onMove:function(p, temp){ temp.push(prev(rect(a, p))); } }).then(function(r){
      if (r.kw === 'R') return ctx.point('Góc xoay', { base:a, num:true, dyn:'ang' }).then(function(q){ rot = q.num != null ? q.num : q.p ? G.ang(a, q.p) : 0; return second(a); });
      if (r.kw === 'D') return ctx.val('Chiều dài', TC.d(LAST.rL || 3000)).then(function(L){ L = Number(L) || 3000; LAST.rL = L; return ctx.val('Chiều rộng', TC.d(LAST.rW || 2000)).then(function(W){ W = Number(W) || 2000; LAST.rW = W; var cnr = function(p){ return [a[0] + (p[0] >= a[0] ? L : -L), a[1] + (p[1] >= a[1] ? W : -W)]; }; return ctx.point('Chỉ định phía của góc đối', { base:a, rubber:false, onMove:function(p, temp){ temp.push(prev(rect(a, cnr(p)))); } }).then(function(q){ if (q.p) return done(a, cnr(q.p)); }); }); });
      if (r.kw === 'A') return ctx.val('Diện tích (mm²)', '6000000').then(function(S){ return ctx.val('Chiều dài', TC.d(LAST.rL || 3000)).then(function(L){ L = Number(L) || 3000; return done(a, [a[0] + L, a[1] + (Number(S) || 0) / L]); }); });
      if (r.p) return done(a, r.p); }); }
    return first();
  };

  IMPL.POLYGON = function(ctx){
    ctx.badge('POL');
    function cPoly(c, p, n, k){ var R = G.dist(c, p), a0 = G.ang(c, p), out = []; if (k === 'C'){ R = R / Math.cos(Math.PI / n); a0 += 180 / n; } for (var i = 0; i < n; i++) out.push(polar(c, R, a0 + i * 360 / n)); return ED.make({ t:'pline', pts:out, closed:true }); }
    function ePoly(a, b, n){ var L = G.dist(a, b), u = unit(sub(b, a)), c = add2(mid(a, b), [-u[1], u[0]], L / 2 / Math.tan(Math.PI / n)); return cPoly(c, a, n, 'I'); }
    var done = function(e){ return ED.run('POLYGON', function(){ ED.add(e); }); };
    return ctx.val('Số cạnh', String(LAST.poly)).then(function(v){ var n = TC.clamp(Math.round(Number(v)) || LAST.poly, 3, 1024); LAST.poly = n;
      return ctx.point('Tâm đa giác', { opts:[['Cạnh', 'E']] }).then(function(r){
        if (r.kw === 'E') return ctx.point('Điểm đầu của cạnh').then(function(a){ if (!a.p) return; return ctx.point('Điểm cuối của cạnh', { base:a.p, onMove:function(p, temp){ if (G.dist(a.p, p) > 1e-6) temp.push(prev(ePoly(a.p, p, n))); } }).then(function(b){ if (b.p) return done(ePoly(a.p, b.p, n)); }); });
        if (!r.p) return; var c = r.p;
        return ctx.kw('Cách dựng', [['Nội tiếp đường tròn', 'I'], ['Ngoại tiếp đường tròn', 'C']], LAST.polyIC || 'I').then(function(k){ LAST.polyIC = k;
          return ctx.point('Bán kính đường tròn', { base:c, onMove:function(p, temp){ if (G.dist(c, p) > 1e-6) temp.push(prev(cPoly(c, p, n, k))); } }).then(function(q){ if (q.p) return done(cPoly(c, q.p, n, k)); }); }); }); });
  };

  IMPL.ELLIPSE = function(ctx, pre){
    ctx.badge('EL');
    var arcMode = pre === 'A';
    function ell(c, a, rb){ return ED.make({ t:'ellipse', c:c, rx:Math.max(1, G.dist(c, a)), ry:Math.max(1, rb), rot:G.ang(c, a) }); }
    function third(c, a){ return ctx.point('Khoảng cách tới trục kia', { base:c, opts:[['Xoay', 'R']], onMove:function(p, temp){ temp.push(prev(ell(c, a, G.dist(c, p)))); } }).then(function(q){
      if (q.kw === 'R') return ctx.val('Góc xoay quanh trục chính', '45').then(function(v){ return fin(ell(c, a, Math.abs(G.dist(c, a) * Math.cos((Number(v) || 0) * Math.PI / 180)))); });
      if (q.p) return fin(ell(c, a, G.dist(c, q.p))); }); }
    function fin(e){ if (!arcMode) return ED.run('ELLIPSE', function(){ ED.add(e); });
      return ctx.point('Góc đầu', { base:e.c, num:true, dyn:'ang' }).then(function(s){ var a0 = s.num != null ? s.num : G.ang(e.c, s.p) - e.rot; return ctx.point('Góc cuối', { base:e.c, num:true, dyn:'ang', onMove:function(p, temp){ var x = ED.clean(e); x.a0 = a0; x.a1 = G.ang(e.c, p) - e.rot; temp.push(prev(x)); } }).then(function(z){ e.a0 = a0; e.a1 = z.num != null ? z.num : G.ang(e.c, z.p) - e.rot; return ED.run('ELLIPSE', function(){ ED.add(e); }); }); }); }
    var first = pre === 'C' ? Promise.resolve({ kw:'C' }) : ctx.point(arcMode ? 'Đầu mút trục của cung ellipse' : 'Đầu mút trục của ellipse', { opts:arcMode ? [['Tâm', 'C']] : [['Cung', 'A'], ['Tâm', 'C']] });
    return first.then(function(r){
      if (r.kw === 'A'){ arcMode = true; return IMPL.ELLIPSE(ctx, 'A'); }
      if (r.kw === 'C') return ctx.point('Tâm ellipse').then(function(c){ if (!c.p) return; return ctx.point('Đầu mút trục', { base:c.p }).then(function(a){ if (a.p) return third(c.p, a.p); }); });
      if (!r.p) return; return ctx.point('Đầu mút kia của trục', { base:r.p }).then(function(b){ if (b.p) return third(mid(r.p, b.p), b.p); });
    });
  };

  IMPL.SPLINE = function(ctx, pre){
    ctx.badge('SPL'); ctx.out('Phương pháp hiện tại: ' + (pre === 'CV' ? 'Điểm điều khiển' : 'Điểm đi qua') + '  Nút: Dây cung');
    var pts = [];
    function step(){
      var n = pts.length, o = !n ? [['Phương pháp', 'M'], ['Nút', 'K'], ['Đối tượng', 'O']] : n >= 3 ? [['Tiếp tuyến cuối', 'T'], ['Dung sai', 'L'], ['Hoàn tác', 'U'], ['Đóng', 'C']] : [['Tiếp tuyến cuối', 'T'], ['Dung sai', 'L'], ['Hoàn tác', 'U']];
      return ctx.point(n ? 'Điểm tiếp theo' : 'Điểm đầu', { base:pts[n - 1], rubber:n < 2, opts:o, onMove:function(p, temp){ if (n) temp.push(prev(ED.make({ t:'spline', pts:pts.concat([p]) }))); } }).then(function(r){
        if (r.enter || r.kw === 'C'){ if (n >= 2) return ED.run('SPLINE', function(){ ED.add(ED.make({ t:'spline', pts:pts, closed:r.kw === 'C' })); }); return; }
        if (r.kw === 'U'){ pts.pop(); return step(); }
        if (r.kw === 'M') return ctx.kw('Phương pháp', [['Điểm đi qua', 'F'], ['Điểm điều khiển', 'CV']], 'F').then(step);
        if (r.kw === 'K') return ctx.kw('Kiểu tham số nút', [['Dây cung', 'C'], ['Căn bậc hai', 'S'], ['Đều', 'U']], 'C').then(step);
        if (r.kw === 'O'){ ctx.out('Chuyển polyline khớp spline thành spline: chọn polyline đã PEDIT › Spline.'); return step(); }
        if (r.kw === 'L') return ctx.val('Dung sai khớp', '0.00').then(step);
        if (r.kw === 'T') return ctx.point('Tiếp tuyến cuối', { base:pts[n - 1] }).then(step);
        if (r.p) pts.push(r.p); return step();
      });
    }
    return step();
  };

  IMPL.XLINE = function(ctx){
    ctx.badge('XL');
    var mk = function(p, dir){ return ED.make({ t:'xline', p:p, dir:unit(dir) }); };
    function loop(p0, dirFn, text){ return ED.run('XLINE', function again(){ return ctx.point(text, { base:p0, onMove:function(p, temp){ var d = dirFn(p); if (d) temp.push(prev(mk(p0 || p, d))); } }).then(function(r){ if (!r.p) return; var d = dirFn(r.p); if (d) addNow(mk(p0 || r.p, d)); return again(); }); }); }
    return ctx.point('Chỉ định một điểm', { opts:[['Ngang', 'H'], ['Dọc', 'V'], ['Góc', 'A'], ['Phân giác', 'B'], ['Song song', 'O']] }).then(function(r){
      if (r.kw === 'H') return loop(null, function(){ return [1, 0]; }, 'Điểm đi qua');
      if (r.kw === 'V') return loop(null, function(){ return [0, 1]; }, 'Điểm đi qua');
      if (r.kw === 'A') return ctx.val('Góc của đường dựng hình', '0').then(function(v){ var a = (Number(v) || 0) * Math.PI / 180; return loop(null, function(){ return [Math.cos(a), Math.sin(a)]; }, 'Điểm đi qua'); });
      if (r.kw === 'B') return ctx.point('Đỉnh của góc').then(function(v){ if (!v.p) return; return ctx.point('Điểm đầu của góc', { base:v.p }).then(function(a){ if (!a.p) return; return loop(v.p, function(p){ var u1 = unit(sub(a.p, v.p)), u2 = unit(sub(p, v.p)); return [u1[0] + u2[0], u1[1] + u2[1]]; }, 'Điểm cuối của góc'); }); });
      if (r.kw === 'O') return ctx.val('Khoảng cách song song', TC.d(LAST.offset)).then(function(v){ var dd = Number(v) || LAST.offset; return ED.pick(ctx, 'Chọn đoạn thẳng', { types:['line', 'xline'] }).then(function(q){ if (!q.e) return; var a = q.e.t === 'line' ? q.e.p1 : q.e.p, u = q.e.t === 'line' ? unit(sub(q.e.p2, q.e.p1)) : q.e.dir, n = [-u[1], u[0]]; return ctx.point('Chỉ định phía', { onMove:function(p, temp){ var s = dot(sub(p, a), n) >= 0 ? 1 : -1; temp.push(prev(mk(add2(a, n, s * dd), u))); } }).then(function(z){ if (!z.p) return; var s = dot(sub(z.p, a), n) >= 0 ? 1 : -1; return ED.run('XLINE', function(){ ED.add(mk(add2(a, n, s * dd), u)); }); }); }); });
      if (r.p) return loop(r.p, function(p){ return G.dist(r.p, p) > 1e-6 ? sub(p, r.p) : null; }, 'Điểm đi qua');
    });
  };
  IMPL.RAY = function(ctx){ ctx.badge('RAY'); return ctx.point('Điểm bắt đầu').then(function(r){ if (!r.p) return; var s = r.p; return ED.run('RAY', function again(){ return ctx.point('Điểm đi qua', { base:s, onMove:function(p, temp){ if (G.dist(s, p) > 1e-6) temp.push(prev(ED.make({ t:'ray', p:s, dir:unit(sub(p, s)) }))); } }).then(function(q){ if (!q.p) return; addNow(ED.make({ t:'ray', p:s, dir:unit(sub(q.p, s)) })); return again(); }); }); }); };
  IMPL.POINT = function(ctx){ var pd = D().pd || { mode:0, size:5 }; ctx.out('Chế độ hiện tại: PDMODE = ' + pd.mode + '  PDSIZE = ' + pd.size + ' · đổi kiểu điểm bằng DDPTYPE'); return ED.run('POINT', function again(){ return ctx.point('Chỉ định một điểm').then(function(r){ if (!r.p) return; addNow(ED.make({ t:'point', p:r.p })); return again(); }); }); };

  // đường đi dọc đối tượng: chia đều, chia theo khoảng
  function pathOf(e){
    var parts = [];
    (e._p || []).forEach(function(pr){
      if (pr.k === 'seg' && !pr.inf) parts.push({ L:G.dist(pr.a, pr.b), at:function(s){ return lerp(pr.a, pr.b, s / this.L); } });
      else if (pr.k === 'arc'){ var sw = sweep(pr.a0, pr.a1) * Math.PI / 180; parts.push({ L:pr.r * sw, at:function(s){ return polar(pr.c, pr.r, pr.a0 + s / pr.r * 180 / Math.PI); } }); }
      else if (pr.k === 'circ') parts.push({ L:2 * Math.PI * pr.r, at:function(s){ return polar(pr.c, pr.r, s / pr.r * 180 / Math.PI); } });
    });
    var total = parts.reduce(function(s, p){ return s + p.L; }, 0);
    return { L:total, at:function(s){ for (var i = 0; i < parts.length; i++){ if (s <= parts[i].L + 1e-9) return parts[i].at(s); s -= parts[i].L; } return parts.length ? parts[parts.length - 1].at(parts[parts.length - 1].L) : null; } };
  }
  ED.pathOf = pathOf;
  function marksAlong(ctx, e, ss, blk){ var path = pathOf(e); return ED.run(blk ? 'Chèn block dọc đối tượng' : 'Chia điểm', function(){ ss.forEach(function(s){ var p = path.at(s); if (!p) return; ED.add(blk ? ED.make({ t:'insert', name:blk, p:p, sx:1, sy:1, rot:0 }) : ED.make({ t:'point', p:p })); }); ctx.out('Đã tạo ' + ss.length + ' ' + (blk ? 'block' : 'điểm') + ' dọc đối tượng (dài ' + TC.d(path.L) + ').' + (blk ? '' : ' Đổi kiểu điểm bằng DDPTYPE để nhìn rõ hơn.')); }); }
  function askBlock(ctx){ return ctx.val('Tên block cần chèn', 'TAB-DAU-GIUONG').then(function(v){ if (!D().blocks[v]){ ctx.out('Không có block “' + esc(v) + '” trong bản vẽ.'); return null; } return v; }); }
  IMPL.DIVIDE = function(ctx){ return ED.pick(ctx, 'Chọn đối tượng cần chia', { types:['line', 'pline', 'arc', 'circle', 'spline', 'ellipse'] }).then(function(r){ if (!r.e) return; var e = r.e;
    return ctx.kw('Số đoạn hoặc', [['Block', 'B']], '4').then(function(k){ var go = function(n, blk){ n = Math.round(n); if (!(n >= 2 && n <= 32767)){ ctx.out('Số đoạn phải từ 2 đến 32767.'); return; } var L = pathOf(e).L, closed = e.t === 'circle' || e.closed, ss = []; for (var i = closed ? 0 : 1; i < n; i++) ss.push(L * i / n); return marksAlong(ctx, e, ss, blk); };
      if (k === 'B') return askBlock(ctx).then(function(b){ if (b) return ctx.val('Số đoạn', '4').then(function(n){ return go(Number(n), b); }); });
      return go(Number(k)); }); }); };
  IMPL.MEASURE = function(ctx){ return ED.pick(ctx, 'Chọn đối tượng cần chia theo khoảng', { types:['line', 'pline', 'arc', 'circle', 'spline'] }).then(function(r){ if (!r.e) return; var e = r.e;
    return ctx.kw('Độ dài mỗi đoạn hoặc', [['Block', 'B']], '1000').then(function(k){ var go = function(L0, blk){ if (!(L0 > 0)){ ctx.out('Độ dài phải lớn hơn 0.'); return; } var L = pathOf(e).L, ss = []; for (var s = L0; s < L - 1e-6 && ss.length < 5000; s += L0) ss.push(s); return marksAlong(ctx, e, ss, blk); };
      if (k === 'B') return askBlock(ctx).then(function(b){ if (b) return ctx.val('Độ dài mỗi đoạn', '1000').then(function(v){ return go(Number(v), b); }); });
      return go(Number(k)); }); }); };

  // đám mây sửa đổi: polyline có cung lồi ra ngoài
  function cloud(poly, arcLen){ var pts = [], bl = [], n = poly.length, area = 0; for (var i = 0; i < n; i++){ var p = poly[i], q = poly[(i + 1) % n]; area += p[0] * q[1] - q[0] * p[1]; } var bg = area > 0 ? -0.5 : 0.5;
    for (i = 0; i < n; i++){ var a = poly[i], b = poly[(i + 1) % n], k = Math.max(1, Math.round(G.dist(a, b) / arcLen)); for (var j = 0; j < k; j++){ pts.push(lerp(a, b, j / k)); bl.push(bg); } }
    return ED.make({ t:'pline', pts:pts, bulges:bl, closed:true }); }
  ED.cloud = cloud;
  IMPL.REVCLOUD = function(ctx, pre){
    ctx.badge('RC'); var mode = pre || LAST.cloudMode || 'R';
    ctx.out('Độ dài cung tối thiểu: ' + TC.d(LAST.cloud) + '  Kiểu: Thường  Chế độ: ' + { R:'Chữ nhật', P:'Đa giác', F:'Tự do' }[mode]);
    var done = function(poly){ return ED.run('REVCLOUD', function(){ ED.add(cloud(poly, LAST.cloud)); ctx.out('Đã tạo đám mây sửa đổi.'); }); };
    function start(){ return ctx.point(mode === 'F' ? 'Điểm đầu (kéo chuột để vẽ)' : 'Góc thứ nhất', { opts:[['Độ dài cung', 'A'], ['Đối tượng', 'O'], ['Chữ nhật', 'R'], ['Đa giác', 'P'], ['Tự do', 'F'], ['Kiểu', 'S'], ['Sửa', 'M']], def:'Đối tượng' }).then(function(r){
      if (r.kw === 'A') return ctx.val('Độ dài cung tối thiểu', TC.d(LAST.cloud)).then(function(v){ LAST.cloud = Math.max(50, Number(v) || LAST.cloud); return start(); });
      if (r.kw === 'R' || r.kw === 'P' || r.kw === 'F'){ mode = LAST.cloudMode = r.kw; return start(); }
      if (r.kw === 'S') return ctx.kw('Kiểu cung', [['Thường', 'N'], ['Nét bút', 'C']], 'N').then(start);
      if (r.kw === 'M'){ ctx.out('Sửa: chọn đám mây có sẵn rồi vẽ lại một đoạn. Prototype: dùng grip để chỉnh.'); return; }
      if (r.kw === 'O' || r.enter) return ED.pick(ctx, 'Chọn đối tượng kín', { types:['pline', 'circle'] }).then(function(q){ if (!q.e) return; var e = q.e, poly = e.t === 'circle' ? Array.from({ length:24 }, function(_, i){ return polar(e.c, e.r, i * 15); }) : e.pts; if (e.t === 'pline' && !e.closed){ ctx.out('Polyline chưa đóng.'); return; } return ctx.kw('Đảo chiều cung?', [['Có', 'Y'], ['Không', 'N']], 'N').then(function(k){ return ED.run('REVCLOUD', function(){ var c = cloud(k === 'Y' ? poly.slice().reverse() : poly, LAST.cloud); c.layer = e.layer; ED.add(c); ED.del(e); }); }); });
      if (!r.p) return; var a = r.p;
      if (mode === 'R') return ctx.point('Góc đối', { base:a, rubber:false, dyn:'wh', onMove:function(p, temp){ temp.push(prev(cloud([a, [p[0], a[1]], p, [a[0], p[1]]], LAST.cloud))); } }).then(function(q){ if (q.p) return done([a, [q.p[0], a[1]], q.p, [a[0], q.p[1]]]); });
      var pts = [a];
      if (mode === 'F'){ ctx.out('Rê chuột theo vùng cần khoanh; bấm để kết thúc.'); }
      return (function more(){ return ctx.point('Điểm tiếp theo', { base:pts[pts.length - 1], onMove:function(p, temp){ if (mode === 'F' && G.dist(pts[pts.length - 1], p) > LAST.cloud) pts.push(p); temp.push(prev(cloud(pts.concat([p]), LAST.cloud))); } }).then(function(q){ if (q.enter || (mode === 'F' && q.p)){ if (q.p) pts.push(q.p); return pts.length >= 3 ? done(pts) : null; } if (q.p) pts.push(q.p); return more(); }); })();
    }); }
    return start();
  };

  /* ================= sửa ================= */
  function selEdit(ctx, text){ return ctx.sel(text).then(function(ids){ return ED.editable(ids, ctx); }); }
  IMPL.ERASE = function(ctx){ return selEdit(ctx).then(function(ids){ if (!ids.length) return; return ED.run('ERASE', function(){ ids.forEach(function(id){ ED.del(D().byId[id]); }); ctx.out('Đã xóa ' + TC.n(ids.length) + ' đối tượng.'); }); }); };

  IMPL.MOVE = function(ctx){
    ctx.badge('M');
    return selEdit(ctx).then(function(ids){ if (!ids.length) return;
      var go = function(t){ return ED.run('MOVE', function(){ ED.apply(ids, t); TC.cad.clearSel(); ctx.out('Đã di chuyển ' + TC.n(ids.length) + ' đối tượng.'); }); };
      return ctx.point('Điểm gốc', { opts:[['Độ dời', 'D']] }).then(function(r){
        if (r.kw === 'D') return ctx.val('Độ dời (dx,dy)', '0,0').then(function(v){ var m = String(v).split(','); return go(T.move(Number(m[0]) || 0, Number(m[1]) || 0)); });
        if (!r.p) return; var b = r.p;
        return ctx.point('Điểm thứ hai', { base:b, def:'dùng điểm gốc làm độ dời', onMove:function(p, temp){ temp.push(ED.xprev(ids, T.move(p[0] - b[0], p[1] - b[1]))); } }).then(function(q){ if (q.enter) return go(T.move(b[0], b[1])); if (q.p) return go(T.move(q.p[0] - b[0], q.p[1] - b[1])); });
      }); });
  };
  IMPL.COPY = function(ctx){
    ctx.badge('CO');
    var multi = LAST.copyM !== false;
    return ctx.sel().then(function(ids){ if (!ids.length) return;
      ctx.out('Chế độ sao chép hiện tại: ' + (multi ? 'Nhiều' : 'Một'));
      var base, marks = [], n = 0;
      function getBase(){ return ctx.point('Điểm gốc', { opts:[['Độ dời', 'D'], ['Chế độ', 'O']] }).then(function(r){
        if (r.kw === 'O') return ctx.kw('Chế độ sao chép', [['Một', 'S'], ['Nhiều', 'M']], multi ? 'M' : 'S').then(function(k){ multi = k === 'M'; LAST.copyM = multi; return getBase(); });
        if (r.kw === 'D') return ctx.val('Độ dời (dx,dy)', '0,0').then(function(v){ var m = String(v).split(','); ED.apply(ids, T.move(Number(m[0]) || 0, Number(m[1]) || 0), true); n++; V().draw(); });
        if (!r.p) return; base = r.p; return next(); }); }
      function next(){
        return ctx.point('Điểm thứ hai', { base:base, opts:marks.length ? [['Mảng', 'A'], ['Thoát', 'E'], ['Hoàn tác', 'U']] : [['Mảng', 'A']], def:marks.length ? 'thoát' : 'dùng điểm gốc làm độ dời', onMove:function(p, temp){ temp.push(ED.xprev(ids, T.move(p[0] - base[0], p[1] - base[1]))); } }).then(function(q){
          if (q.enter || q.kw === 'E'){ if (!marks.length && q.enter){ ED.apply(ids, T.move(base[0], base[1]), true); n++; } return; }
          if (q.kw === 'U'){ if (marks.length){ ED.back(marks.pop()); n--; } return next(); }
          if (q.kw === 'A') return ctx.val('Số phần tử của mảng', '3').then(function(v){ var k = Math.max(2, Math.round(Number(v)) || 3), fit = false;
            return (function arr(){ return ctx.point('Điểm thứ hai', { base:base, opts:[['Vừa khít', 'F']], onMove:function(p, temp){ for (var i = 1; i < k; i++){ var f = fit ? i / (k - 1) : i; temp.push(ED.xprev(ids, T.move((p[0] - base[0]) * f, (p[1] - base[1]) * f))); } } }).then(function(z){
              if (z.kw === 'F'){ fit = true; return arr(); } if (!z.p) return next(); marks.push(ED.mark()); for (var i = 1; i < k; i++){ var f = fit ? i / (k - 1) : i; ED.apply(ids, T.move((z.p[0] - base[0]) * f, (z.p[1] - base[1]) * f), true); } n += k - 1; V().draw(); return multi ? next() : null; }); })(); });
          if (q.p){ marks.push(ED.mark()); ED.apply(ids, T.move(q.p[0] - base[0], q.p[1] - base[1]), true); n++; V().draw(); return multi ? next() : null; }
        });
      }
      return ED.run('COPY', getBase).then(function(){ TC.cad.clearSel(); if (n) ctx.out('Đã tạo ' + n + ' bản sao.'); });
    });
  };
  IMPL.ROTATE = function(ctx){
    ctx.badge('RO');
    return selEdit(ctx).then(function(ids){ if (!ids.length) return;
      ctx.out('Góc dương theo UCS hiện tại: ngược chiều kim đồng hồ, 0° hướng sang phải');
      return ctx.point('Điểm gốc').then(function(r){ if (!r.p) return; var b = r.p, copy = false, ref = 0;
        var go = function(a){ LAST.rot = Math.round(a * 100) / 100; return ED.run('ROTATE', function(){ ED.apply(ids, T.rot(b, a), copy); TC.cad.clearSel(); ctx.out('Đã xoay ' + TC.n(ids.length) + ' đối tượng ' + TC.d(a) + '°' + (copy ? ' (giữ bản gốc)' : '') + '.'); }); };
        function ask(){ return ctx.point('Góc xoay', { base:b, num:true, dyn:'ang', opts:[['Sao chép', 'C'], ['Tham chiếu', 'R']], def:TC.d(LAST.rot, 0), onMove:function(p, temp){ temp.push(ED.xprev(ids, T.rot(b, G.ang(b, p) - ref))); } }).then(function(q){
          if (q.kw === 'C'){ copy = true; ctx.out('Xoay một bản sao của các đối tượng đã chọn.'); return ask(); }
          if (q.kw === 'R') return ctx.point('Góc tham chiếu', { base:b, num:true, dyn:'ang', def:'0' }).then(function(z){ ref = z.num != null ? z.num : z.p ? G.ang(b, z.p) : 0; return ctx.point('Góc mới', { base:b, num:true, dyn:'ang', onMove:function(p, temp){ temp.push(ED.xprev(ids, T.rot(b, G.ang(b, p) - ref))); } }).then(function(k){ return go((k.num != null ? k.num : k.p ? G.ang(b, k.p) : ref) - ref); }); });
          return go(q.num != null ? q.num : q.enter ? LAST.rot : G.ang(b, q.p)); }); }
        return ask(); }); });
  };
  IMPL.SCALE = function(ctx){
    ctx.badge('SC');
    return selEdit(ctx).then(function(ids){ if (!ids.length) return;
      var bb = ED.bbox(ids), half = Math.max(1, Math.hypot(bb[2] - bb[0], bb[3] - bb[1]) / 2);
      return ctx.point('Điểm gốc').then(function(r){ if (!r.p) return; var b = r.p, copy = false;
        var fOf = function(p){ return Math.max(.05, Math.round(G.dist(b, p) / half * 20) / 20); };
        var go = function(k){ if (!(k > 0)){ ctx.out('Hệ số tỉ lệ phải lớn hơn 0.'); return ask(); } LAST.sc = k; return ED.run('SCALE', function(){ ED.apply(ids, T.sc(b, k), copy); TC.cad.clearSel(); ctx.out('Đã đổi tỉ lệ ' + TC.n(ids.length) + ' đối tượng, hệ số ' + TC.d(k, 4) + '.'); }); };
        function ask(){ return ctx.point('Hệ số tỉ lệ', { base:b, num:true, dyn:false, opts:[['Sao chép', 'C'], ['Tham chiếu', 'R']], def:TC.d(LAST.sc, 4), onMove:function(p, temp){ temp.push(ED.xprev(ids, T.sc(b, fOf(p)))); TC.cmdline.status('Hệ số theo con trỏ: ' + TC.d(fOf(p)) + ' · gõ số để nhập chính xác'); } }).then(function(q){
          if (q.kw === 'C'){ copy = true; return ask(); }
          if (q.kw === 'R') return ctx.val('Độ dài tham chiếu', '1').then(function(a){ return ctx.val('Độ dài mới', '1').then(function(z){ return go((Number(z) || 1) / (Number(a) || 1)); }); });
          return go(q.num != null ? q.num : q.enter ? LAST.sc : fOf(q.p)); }); }
        return ask(); }); });
  };
  IMPL.MIRROR = function(ctx){
    ctx.badge('MI');
    return selEdit(ctx).then(function(ids){ if (!ids.length) return;
      return ctx.point('Điểm thứ nhất của trục đối xứng').then(function(a){ if (!a.p) return;
        return ctx.point('Điểm thứ hai của trục đối xứng', { base:a.p, onMove:function(p, temp){ if (G.dist(a.p, p) > 1e-6) temp.push(ED.temp(ED.ghost(ids, T.mir(a.p, p)))); } }).then(function(b){ if (!b.p || G.dist(a.p, b.p) < 1e-6) return; var t = T.mir(a.p, b.p);
          V().temp = [ED.temp(ED.ghost(ids, t))]; V().drawOver();
          return ctx.kw('Xóa đối tượng nguồn?', [['Có', 'Y'], ['Không', 'N']], 'N').then(function(k){ V().temp = []; return ED.run('MIRROR', function(){ ED.apply(ids, t, k !== 'Y'); TC.cad.clearSel(); if (!TC.S.mirrtext) ctx.out('MIRRTEXT = 0: chữ giữ chiều đọc.'); }); }); }); }); });
  };
  function inBox(box, p){ if (!box) return true; return p[0] >= Math.min(box.p[0], box.q[0]) && p[0] <= Math.max(box.p[0], box.q[0]) && p[1] >= Math.min(box.p[1], box.q[1]) && p[1] <= Math.max(box.p[1], box.q[1]); }
  function stretchEnt(e, box, dx, dy){
    var mv = function(p){ return inBox(box, p) ? [p[0] + dx, p[1] + dy] : p; };
    switch (e.t){
      case 'line': e.p1 = mv(e.p1); e.p2 = mv(e.p2); break;
      case 'pline': case 'spline': e.pts = e.pts.map(mv); break;
      case 'mleader': e.pts = e.pts.map(mv); break;
      case 'hatch': e.loops = e.loops.map(function(l){ return l.map(mv); }); break;
      case 'dim': { var a = inBox(box, e.p1), b = inBox(box, e.p2); if (a && b) ED.xform(e, T.move(dx, dy)); else { if (a) e.p1 = [e.p1[0] + dx, e.p1[1] + dy]; if (b) e.p2 = [e.p2[0] + dx, e.p2[1] + dy]; } break; }
      default: { var k = e.p || e.c || (e.pts && e.pts[0]); if (k && inBox(box, k)) ED.xform(e, T.move(dx, dy)); }
    }
    return e;
  }
  IMPL.STRETCH = function(ctx){
    ctx.badge('S'); ctx.out('Chọn đối tượng cần kéo giãn bằng khung Crossing (kéo từ phải sang trái)…');
    var d = D(); if (!d.sel.length) TC.cad.lastBox = null;
    return selEdit(ctx, 'Chọn đối tượng').then(function(ids){ if (!ids.length) return; var box = TC.cad.lastBox;
      return ctx.point('Điểm gốc', { opts:[['Độ dời', 'D']] }).then(function(r){ if (!r.p) return; var b = r.p;
        return ctx.point('Điểm thứ hai', { base:b, def:'dùng điểm gốc làm độ dời', onMove:function(p, temp){ temp.push(ED.temp(ids.slice(0, 300).map(function(id){ return stretchEnt(ED.clean(d.byId[id]), box, p[0] - b[0], p[1] - b[1]); }))); } }).then(function(q){ if (!q.p) return; var dx = q.p[0] - b[0], dy = q.p[1] - b[1];
          return ED.run('STRETCH', function(){ ids.forEach(function(id){ ED.mod(d.byId[id], function(x){ stretchEnt(x, box, dx, dy); }); }); TC.cad.clearSel(); }); }); }); });
  };

  // offset: line, tròn, cung, ellipse (gần đúng), polyline (nối góc vát nhọn, giữ bulge của cung bo)
  function offsetEnt(e, dist, p){
    var o = ED.clean(e);
    if (e.t === 'line' || e.t === 'xline'){ var a = e.t === 'line' ? e.p1 : e.p, u = e.t === 'line' ? unit(sub(e.p2, e.p1)) : e.dir, n = [-u[1], u[0]], sd = dot(sub(p, a), n), dd = dist == null ? Math.abs(sd) : dist, s = sd >= 0 ? 1 : -1; if (e.t === 'line'){ o.p1 = add2(e.p1, n, s * dd); o.p2 = add2(e.p2, n, s * dd); } else o.p = add2(e.p, n, s * dd); return o; }
    if (e.t === 'circle' || e.t === 'arc'){ var dc = G.dist(e.c, p), d2 = dist == null ? Math.abs(dc - e.r) : dist, nr = e.r + (dc > e.r ? d2 : -d2); if (nr <= 1e-6) return null; o.r = nr; return o; }
    if (e.t === 'ellipse'){ var de = dist == null ? 200 : dist, out = G.dist(e.c, p) > Math.min(e.rx, e.ry) ? 1 : -1; o.rx += out * de; o.ry += out * de; return o.rx > 0 && o.ry > 0 ? o : null; }
    if (e.t === 'pline' || e.t === 'spline'){
      var P = e.pts, n = P.length, closed = !!e.closed, B = e.bulges || [], m = closed ? n : n - 1, best = 0, bd = Infinity;
      for (var i = 0; i < m; i++){ var q = G.segDist(p, P[i], P[(i + 1) % n]); if (q < bd){ bd = q; best = i; } }
      var A = P[best], Bp = P[(best + 1) % n], nb = unit([-(Bp[1] - A[1]), Bp[0] - A[0]]), side = dot(sub(p, A), nb) >= 0 ? 1 : -1, dd2 = dist == null ? bd : dist;
      if ((B[best] || 0) !== 0){ var am = ED.plSegMid(e, best), cc = ED.circum(A, am, Bp); if (cc) side = (G.dist(p, cc) < G.dist(cc, A)) === (B[best] > 0) ? 1 : -1; }
      var k = side * dd2, tans = [];
      for (i = 0; i < m; i++){ var a1 = P[i], b1 = P[(i + 1) % n], ch = unit(sub(b1, a1)), h = 2 * Math.atan(B[i] || 0), ts = [ch[0] * Math.cos(-h) - ch[1] * Math.sin(-h), ch[0] * Math.sin(-h) + ch[1] * Math.cos(-h)], te = [ch[0] * Math.cos(h) - ch[1] * Math.sin(h), ch[0] * Math.sin(h) + ch[1] * Math.cos(h)]; tans.push({ ts:ts, te:te }); }
      var NP = [];
      for (var j = 0; j < n; j++){
        var inS = j > 0 || closed ? tans[(j - 1 + m) % m] : null, outS = j < m ? tans[j] : null, v = P[j];
        if (!inS || !outS){ var t0 = (outS || inS)[outS ? 'ts' : 'te']; NP.push(add2(v, [-t0[1], t0[0]], k)); continue; }
        var n1 = [-inS.te[1], inS.te[0]], n2 = [-outS.ts[1], outS.ts[0]];
        if (dot(n1, n2) > 0.9999){ NP.push(add2(v, n2, k)); continue; }
        var X = lineInt(add2(v, n1, k), add2(add2(v, n1, k), inS.te), add2(v, n2, k), add2(add2(v, n2, k), outS.ts)); NP.push(X || add2(v, n2, k));
      }
      o.pts = NP;
      if (closed){ var ar0 = 0, ar1 = 0; for (i = 0; i < n; i++){ ar0 += cross(P[i], P[(i + 1) % n]); ar1 += cross(NP[i], NP[(i + 1) % n]); } if (ar0 * ar1 <= 0 || Math.abs(ar1) < 1) return null; }
      return o;
    }
    return null;
  }
  ED.offsetEnt = offsetEnt;
  IMPL.OFFSET = function(ctx){
    ctx.badge('O'); ctx.out('Thiết lập hiện tại: Xóa nguồn = ' + (LAST.offErase ? 'Có' : 'Không') + '  Layer = ' + (LAST.offLayer === 'C' ? 'Hiện hành' : 'Nguồn'));
    var dist = LAST.offset, through = !!LAST.offThrough, marks = [];
    function askD(){ return ctx.point('Khoảng cách offset', { opts:[['Đi qua', 'T'], ['Xóa', 'E'], ['Layer', 'L']], def:through ? 'Đi qua' : TC.d(dist), num:true, dyn:false }).then(function(r){
      if (r.kw === 'T'){ through = LAST.offThrough = true; return; }
      if (r.kw === 'E') return ctx.kw('Xóa đối tượng nguồn sau khi offset?', [['Có', 'Y'], ['Không', 'N']], LAST.offErase ? 'Y' : 'N').then(function(k){ LAST.offErase = k === 'Y'; return askD(); });
      if (r.kw === 'L') return ctx.kw('Layer cho đối tượng offset', [['Hiện hành', 'C'], ['Nguồn', 'S']], LAST.offLayer || 'S').then(function(k){ LAST.offLayer = k; return askD(); });
      if (r.num != null){ if (r.num <= 0){ ctx.out('Khoảng cách phải lớn hơn 0.'); return askD(); } dist = LAST.offset = r.num; through = LAST.offThrough = false; return; }
      if (r.p) return ctx.point('Điểm thứ hai', { base:r.p }).then(function(q){ if (q.p){ dist = LAST.offset = G.dist(r.p, q.p); through = LAST.offThrough = false; } });
    }); }
    var make = function(e, p){ var o = offsetEnt(e, through ? null : dist, p); if (o && LAST.offLayer === 'C') o.layer = ED.clayer(); return o; };
    function loop(){
      return ED.pick(ctx, 'Chọn đối tượng cần offset', { opts:[['Thoát', 'E'], ['Hoàn tác', 'U']], def:'Thoát', types:['line', 'xline', 'circle', 'arc', 'pline', 'ellipse', 'spline'], bad:'Không offset được loại đối tượng này' }).then(function(r){
        if (r.enter || r.kw === 'E') return;
        if (r.kw === 'U'){ if (marks.length) ED.back(marks.pop()); return loop(); }
        var e = r.e, multi = false;
        return (function side(){ return ctx.point(through ? 'Điểm đi qua' : 'Chỉ định điểm về phía cần offset', { opts:[['Thoát', 'E'], ['Nhiều', 'M'], ['Hoàn tác', 'U']], def:multi ? 'đối tượng kế' : 'Thoát', onMove:function(p, temp){ var o = make(e, p); if (o) temp.push(prev(o)); } }).then(function(q){
          if (q.kw === 'E') return;
          if (q.enter) return multi ? loop() : null;
          if (q.kw === 'M'){ multi = true; return side(); }
          if (q.kw === 'U'){ if (marks.length) ED.back(marks.pop()); return loop(); }
          var o = make(e, q.p); if (!o){ ctip(q, 'Không offset được về phía này'); return side(); }
          marks.push(ED.mark()); var ne = addNow(o, ED.listOf(D(), e)); if (LAST.offErase && !multi){ ED.del(e); V().draw(); }
          if (multi){ e = ne; return side(); } return loop(); }); })();
      });
    }
    return askD().then(function(){ return ED.run('OFFSET', loop); });
  };

  /* ---------- cắt, kéo dài (chế độ Nhanh: mọi đối tượng là cạnh cắt) ---------- */
  function cutters(e){ var d = D(), out = [], b = e._bb, tol = 1; ED.spaceList(d).forEach(function(x){ if (x === e || !TC.entVisible(d, x) || /^(hatch|text|mtext|attdef|point|image|vpframe)$/.test(x.t)) return; var c = x._bb; if (c[2] < b[0] - tol || c[0] > b[2] + tol || c[3] < b[1] - tol || c[1] > b[3] + tol) return; x._p.forEach(function(pr){ if (pr.k === 'seg' || pr.k === 'arc' || pr.k === 'circ') out.push(pr); }); }); return out; }
  function allBounds(e){ var d = D(), out = []; ED.spaceList(d).forEach(function(x){ if (x === e || !TC.entVisible(d, x) || /^(hatch|text|mtext|attdef|point|image|vpframe)$/.test(x.t)) return; x._p.forEach(function(pr){ if (pr.k === 'seg' || pr.k === 'arc' || pr.k === 'circ') out.push(pr); }); }); return out; }
  function segT(a, b, c, d2){ var r = sub(b, a), s = sub(d2, c), den = cross(r, s); if (Math.abs(den) < 1e-12) return null; var t = cross(sub(c, a), s) / den, u = cross(sub(c, a), r) / den; return u < -1e-9 || u > 1 + 1e-9 ? null : t; }
  function circT(a, b, pr){ var dx = b[0] - a[0], dy = b[1] - a[1], fx = a[0] - pr.c[0], fy = a[1] - pr.c[1], A2 = dx * dx + dy * dy, B2 = 2 * (fx * dx + fy * dy), C2 = fx * fx + fy * fy - pr.r * pr.r, disc = B2 * B2 - 4 * A2 * C2; if (disc < 0 || !A2) return []; var sq = Math.sqrt(disc); return [(-B2 - sq) / (2 * A2), (-B2 + sq) / (2 * A2)].filter(function(t){ return pr.k !== 'arc' || G.inArc(G.ang(pr.c, [a[0] + dx * t, a[1] + dy * t]), pr.a0, pr.a1); }); }
  function lineTs(a, b, prs){ var ts = []; prs.forEach(function(pr){ if (pr.k === 'seg'){ var t = segT(a, b, pr.a, pr.b); if (t != null) ts.push(t); } else Array.prototype.push.apply(ts, circT(a, b, pr)); }); return ts; }
  // giao của đường tròn (tâm c, bán kính r) với các cạnh: trả về góc (độ)
  function circAngs(c, r, prs){ var out = []; prs.forEach(function(pr){ if (pr.k === 'seg'){ circT(pr.a, pr.b, { k:'circ', c:c, r:r }).forEach(function(t){ if (t >= -1e-9 && t <= 1 + 1e-9) out.push(G.ang(c, lerp(pr.a, pr.b, t))); }); } else { var dd = G.dist(c, pr.c); if (dd < 1e-9 || dd > r + pr.r || dd < Math.abs(r - pr.r)) return; var a = (r * r - pr.r * pr.r + dd * dd) / (2 * dd), h = Math.sqrt(Math.max(0, r * r - a * a)), u = unit(sub(pr.c, c)), m0 = add2(c, u, a); [add2(m0, [-u[1], u[0]], h), add2(m0, [u[1], -u[0]], h)].forEach(function(q){ if (pr.k !== 'arc' || G.inArc(G.ang(pr.c, q), pr.a0, pr.a1)) out.push(G.ang(c, q)); }); } }); return out; }
  // kết quả cắt tại điểm bấm: { del:[phần bị xóa để xem trước], apply:fn }
  function trimAt(e, p){
    var prs = cutters(e);
    if (e.t === 'line'){
      var L2 = Math.pow(G.dist(e.p1, e.p2), 2) || 1, tc = dot(sub(p, e.p1), sub(e.p2, e.p1)) / L2, ts = lineTs(e.p1, e.p2, prs).filter(function(t){ return t > 1e-6 && t < 1 - 1e-6; });
      var lo = null, hi = null; ts.forEach(function(t){ if (t < tc && (lo == null || t > lo)) lo = t; if (t > tc && (hi == null || t < hi)) hi = t; });
      var P = function(t){ return lerp(e.p1, e.p2, t); };
      return { del:[[P(lo == null ? 0 : lo), P(hi == null ? 1 : hi)]], whole:lo == null && hi == null, apply:function(){
        if (lo == null && hi == null){ ED.del(e); return; }
        var A0 = e.p1.slice(), B0 = e.p2.slice(), P2 = function(t){ return lerp(A0, B0, t); };
        if (lo != null) ED.mod(e, function(x){ x.p2 = P2(lo); }); else ED.mod(e, function(x){ x.p1 = P2(hi); });
        if (lo != null && hi != null){ var nl = ED.clean(e); delete nl.id; nl.p1 = P2(hi); nl.p2 = B0; ED.add(nl, ED.listOf(D(), e)); } } };
    }
    if (e.t === 'circle' || e.t === 'arc'){
      var angs = circAngs(e.c, e.r, prs), ac = G.ang(e.c, p), base = e.t === 'arc' ? e.a0 : ac, rel = function(a){ return sweep(base, a); };
      if (e.t === 'arc') angs = angs.filter(function(a){ var s = rel(a); return s > 1e-4 && s < sweep(e.a0, e.a1) - 1e-4; });
      if (e.t === 'circle' && angs.length < 2) return { del:[], whole:true, arc:{ c:e.c, r:e.r, a0:0, a1:360 }, apply:function(){ ED.del(e); } };
      var rc = rel(ac), lo2 = null, hi2 = null;
      angs.forEach(function(a){ var s = rel(a); if (e.t === 'circle'){ if (lo2 == null || s > rel(lo2)) lo2 = a; if (hi2 == null || s < rel(hi2)) hi2 = a; } else { if (s < rc && (lo2 == null || s > rel(lo2))) lo2 = a; if (s > rc && (hi2 == null || s < rel(hi2))) hi2 = a; } });
      if (e.t === 'circle') return { del:[], arc:{ c:e.c, r:e.r, a0:lo2, a1:hi2 }, apply:function(){ var list = ED.listOf(D(), e), n = Object.assign(ED.clean(e), { t:'arc', a0:hi2, a1:lo2 }); delete n.id; ED.del(e); ED.add(n, list); } };
      var A0 = lo2 == null ? e.a0 : lo2, A1 = hi2 == null ? e.a1 : hi2;
      return { del:[], arc:{ c:e.c, r:e.r, a0:A0, a1:A1 }, whole:lo2 == null && hi2 == null, apply:function(){ if (lo2 == null && hi2 == null){ ED.del(e); return; } var oa1 = e.a1; if (lo2 != null) ED.mod(e, function(x){ x.a1 = lo2; }); else ED.mod(e, function(x){ x.a0 = hi2; }); if (lo2 != null && hi2 != null){ var n2 = Object.assign(ED.clean(e), { a0:hi2, a1:oa1 }); delete n2.id; ED.add(n2, ED.listOf(D(), e)); } } };
    }
    if (e.t === 'pline'){
      // đường đi theo các đoạn (cung tính theo dây cung); tham số s = chỉ số đoạn + t
      var Pp = e.pts, n = Pp.length, m = e.closed ? n : n - 1, hits = [], best = 0, bd = Infinity, tcl = 0;
      for (var i = 0; i < m; i++){ var a = Pp[i], b = Pp[(i + 1) % n]; lineTs(a, b, prs).forEach(function(t){ if (t > 1e-6 && t < 1 - 1e-6) hits.push(i + t); }); var f = G.foot(p, a, b), dq = G.dist(p, lerp(a, b, TC.clamp(f.t, 0, 1))); if (dq < bd){ bd = dq; best = i; tcl = TC.clamp(f.t, 0, 1); } }
      var sc = best + tcl, cut = hits.sort(function(x, y){ return x - y; });
      if (e.closed && cut.length < 2) return { del:[Pp.concat([Pp[0]])], whole:true, apply:function(){ ED.del(e); } };
      var lo3 = null, hi3 = null; cut.forEach(function(s){ if (s < sc && (lo3 == null || s > lo3)) lo3 = s; if (s > sc && (hi3 == null || s < hi3)) hi3 = s; });
      if (e.closed && cut.length >= 2){ if (lo3 == null) lo3 = cut[cut.length - 1]; if (hi3 == null) hi3 = cut[0]; }
      var at = function(s){ var k = Math.min(Math.floor(s), m - 1), t = s - k; return lerp(Pp[k], Pp[(k + 1) % n], t); };
      var piece = function(s0, s1){ var out = [at(s0)]; for (var j = Math.floor(s0) + 1; j <= Math.ceil(s1) - 1; j++) out.push(Pp[j % n]); out.push(at(s1)); return out; };
      var delPts = e.closed && lo3 != null && hi3 != null && lo3 > hi3 ? piece(lo3, m).concat(piece(0, hi3).slice(1)) : piece(lo3 == null ? 0 : lo3, hi3 == null ? m : hi3);
      return { del:[delPts], whole:lo3 == null && hi3 == null, apply:function(){
        if (lo3 == null && hi3 == null){ ED.del(e); return; }
        var list = ED.listOf(D(), e), mkp = function(pts){ var x = Object.assign(ED.clean(e), { pts:pts, closed:false, bulges:[] }); delete x.id; return x; };
        if (e.closed){ var keep = lo3 > hi3 ? piece(hi3, lo3) : piece(hi3, m).concat(piece(0, lo3).slice(1)); ED.del(e); ED.add(mkp(keep), list); return; }
        var parts = []; if (lo3 != null) parts.push(piece(0, lo3)); if (hi3 != null) parts.push(piece(hi3, m)); ED.del(e); parts.forEach(function(pp){ if (pp.length >= 2) ED.add(mkp(pp), list); }); } };
    }
    return null;
  }
  function extendAt(e, p){
    var prs = allBounds(e);
    if (e.t === 'line' || (e.t === 'pline' && !e.closed)){
      var P = e.t === 'line' ? [e.p1, e.p2] : e.pts, endB = G.dist(p, P[P.length - 1]) < G.dist(p, P[0]), a = endB ? P[P.length - 2] : P[1], b = endB ? P[P.length - 1] : P[0];
      var ts = lineTs(a, b, prs).filter(function(t){ return t > 1 + 1e-6; }); if (!ts.length) return null; var t = Math.min.apply(null, ts), q = lerp(a, b, t);
      return { add:[b, q], apply:function(){ ED.mod(e, function(x){ if (x.t === 'line'){ if (endB) x.p2 = q; else x.p1 = q; } else { if (endB) x.pts[x.pts.length - 1] = q; else x.pts[0] = q; } }); } };
    }
    if (e.t === 'arc'){ var angs = circAngs(e.c, e.r, prs), endA = G.dist(p, polar(e.c, e.r, e.a1)) < G.dist(p, polar(e.c, e.r, e.a0)), sw = sweep(e.a0, e.a1); var cand = angs.map(function(an){ return endA ? sweep(e.a1, an) : sweep(an, e.a0); }).filter(function(s){ return s > 1e-4 && s < 360 - sw; }); if (!cand.length) return null; var s = Math.min.apply(null, cand); return { arc:endA ? { c:e.c, r:e.r, a0:e.a1, a1:e.a1 + s } : { c:e.c, r:e.r, a0:e.a0 - s, a1:e.a0 }, apply:function(){ ED.mod(e, function(x){ if (endA) x.a1 = e.a1 + s; else x.a0 = e.a0 - s; }); } }; }
    return null;
  }
  function trimPreview(res, temp, ext){
    var bg = TC.cssv('--c-canvas'), col = ext ? TC.cssv('--c-accent') : TC.cssv('--c-rubber');
    if (res.del) res.del.forEach(function(pts){ if (pts.length < 2) return; temp.push({ k:'poly', pts:pts, color:bg, w:3.5 }, { k:'poly', pts:pts, color:col, w:1.4, dash:[5, 4], alpha:.85 }); });
    if (res.add) temp.push({ k:'poly', pts:res.add, color:col, w:1.4, dash:[5, 4] });
    if (res.arc && res.arc.a0 != null){ var a = res.arc, sw = sweep(a.a0, a.a1) || 360, pts = []; for (var i = 0; i <= 32; i++) pts.push(polar(a.c, a.r, a.a0 + sw * i / 32)); temp.push({ k:'poly', pts:pts, color:bg, w:3.5 }, { k:'poly', pts:pts, color:col, w:1.4, dash:[5, 4], alpha:.85 }); }
  }
  function trimCmd(ctx, extend){
    ctx.badge(extend ? 'EX' : 'TR');
    ctx.out('Thiết lập hiện tại: Chiếu = UCS, Cạnh = Không kéo dài, Chế độ = Nhanh · mọi đối tượng là ' + (extend ? 'cạnh biên' : 'cạnh cắt'));
    var marks = [];
    function loop(){
      return ctx.point(extend ? 'Chọn đối tượng cần kéo dài, hoặc Shift+chọn để cắt' : 'Chọn đối tượng cần cắt, hoặc Shift+chọn để kéo dài', { pick:true, opts:[[extend ? 'Cạnh biên' : 'Cạnh cắt', extend ? 'B' : 'T'], ['Khung chéo', 'C'], ['Chế độ', 'O'], ['Chiếu', 'P'], ['Xóa', 'R'], ['Hoàn tác', 'U']], dyn:false,
        onMove:function(p, temp, hov){ var e = hov && D().byId[hov]; if (!e || (D().layerMap[e.layer] || {}).lck) return; var sh = window.event && window.event.shiftKey, ex = sh ? !extend : extend, r = ex ? extendAt(e, p) : trimAt(e, p); if (r) trimPreview(r, temp, ex); } }).then(function(r){
        if (r.enter) return;
        if (r.kw === 'U'){ if (marks.length) ED.back(marks.pop()); else ctx.out('Không còn bước để hoàn tác.'); return loop(); }
        if (r.kw === 'R') return ctx.sel('Chọn đối tượng cần xóa').then(function(ids){ ids = ED.editable(ids, ctx); marks.push(ED.mark()); ids.forEach(function(id){ ED.del(D().byId[id]); }); V().draw(); return loop(); });
        if (r.kw === 'O') return ctx.kw('Chế độ', [['Nhanh', 'Q'], ['Chuẩn', 'S']], 'Q').then(function(k){ if (k === 'S') ctx.out('Chế độ Chuẩn: chọn cạnh cắt trước rồi mới chọn đoạn cần cắt. Prototype giữ chế độ Nhanh.'); return loop(); });
        if (r.kw === 'T' || r.kw === 'B' || r.kw === 'P' || r.kw === 'C'){ ctx.out({ T:'Cạnh cắt: ở chế độ Nhanh mọi đối tượng đều là cạnh cắt.', B:'Cạnh biên: ở chế độ Nhanh mọi đối tượng đều là cạnh biên.', P:'Chiếu: UCS.', C:'Khung chéo: kéo khung qua các đoạn cần cắt.' }[r.kw]); return loop(); }
        var id = V().hitTest(r.s[0], r.s[1]), e = id && D().byId[id];
        if (!e){ ctip(r, 'Không có đối tượng tại đây'); return loop(); }
        if ((D().layerMap[e.layer] || {}).lck){ ctip(r, 'Đối tượng nằm trên layer bị khóa'); return loop(); }
        var ex = r.shift ? !extend : extend, res = ex ? extendAt(e, r.p) : trimAt(e, r.p);
        if (!res){ ctip(r, ex ? 'Không có cạnh biên để kéo dài tới' : 'Không cắt được loại đối tượng này'); return loop(); }
        marks.push(ED.mark()); res.apply(); V().draw();
        return loop();
      });
    }
    return ED.run(extend ? 'EXTEND' : 'TRIM', loop);
  }
  IMPL.TRIM = function(ctx){ return trimCmd(ctx, false); };
  IMPL.EXTEND = function(ctx){ return trimCmd(ctx, true); };

  /* ---------- bo góc, vát góc giữa hai đoạn thẳng ---------- */
  function corner(l1, p1, l2, p2, r, cham){
    var X = lineInt(l1.p1, l1.p2, l2.p1, l2.p2); if (!X) return null;
    var side = function(l, p){ var u = unit(sub(l.p2, l.p1)); return dot(sub(p, X), u) >= 0 ? u : [-u[0], -u[1]]; };
    var u1 = side(l1, p1), u2 = side(l2, p2), th = Math.acos(TC.clamp(dot(u1, u2), -1, 1)); if (th < 1e-4 || Math.PI - th < 1e-4) return null;
    var t1 = cham ? cham[0] : r ? r / Math.tan(th / 2) : 0, t2 = cham ? cham[1] : t1, T1 = add2(X, u1, t1), T2 = add2(X, u2, t2);
    var far = function(l, u){ return dot(sub(l.p1, X), u) > dot(sub(l.p2, X), u) ? l.p1 : l.p2; }, F1 = far(l1, u1), F2 = far(l2, u2);
    if (dot(sub(F1, X), u1) < t1 - 1e-6 || dot(sub(F2, X), u2) < t2 - 1e-6) return null;
    var res = { l1:[F1, T1], l2:[F2, T2] };
    if (cham) res.join = [T1, T2];
    else if (r){ var bis = unit([u1[0] + u2[0], u1[1] + u2[1]]), c = add2(X, bis, r / Math.sin(th / 2)), a0 = G.ang(c, T1), a1 = G.ang(c, T2); if (sweep(a0, a1) > 180){ var tt = a0; a0 = a1; a1 = tt; } res.arc = { c:c, r:r, a0:a0, a1:a1 }; }
    return res;
  }
  function cornerTemp(res, l1, l2){ var L = [ED.make({ t:'line', p1:res.l1[0], p2:res.l1[1], layer:l1.layer }), ED.make({ t:'line', p1:res.l2[0], p2:res.l2[1], layer:l2.layer })]; if (res.arc) L.push(ED.make(Object.assign({ t:'arc', layer:l1.layer }, res.arc))); if (res.join) L.push(ED.make({ t:'line', p1:res.join[0], p2:res.join[1], layer:l1.layer })); return ED.temp(L, { color:TC.cssv('--c-accent'), w:1.6 }); }
  function cornerApply(res, l1, l2, trim){ if (trim !== false){ ED.mod(l1, function(x){ x.p1 = res.l1[0]; x.p2 = res.l1[1]; }); ED.mod(l2, function(x){ x.p1 = res.l2[0]; x.p2 = res.l2[1]; }); } var list = ED.listOf(D(), l1); if (res.arc) ED.add(ED.make(Object.assign({ t:'arc', layer:l1.layer }, res.arc)), list); if (res.join) ED.add(ED.make({ t:'line', p1:res.join[0], p2:res.join[1], layer:l1.layer }), list); V().draw(); }
  function cornerCmd(ctx, cham){
    ctx.badge(cham ? 'CHA' : 'F');
    var trim = LAST.trim !== false, multi = false, marks = [];
    ctx.out(cham ? 'Chế độ CẮT hiện tại · Khoảng vát 1 = ' + TC.d(LAST.chamA) + ', Khoảng vát 2 = ' + TC.d(LAST.chamB) : 'Thiết lập hiện tại: Chế độ = ' + (trim ? 'CẮT' : 'KHÔNG CẮT') + ', Bán kính = ' + TC.d(LAST.fillet));
    var opts = cham ? [['Hoàn tác', 'U'], ['Polyline', 'P'], ['Khoảng cách', 'D'], ['Góc', 'A'], ['Cắt', 'T'], ['Phương pháp', 'E'], ['Nhiều', 'M']] : [['Hoàn tác', 'U'], ['Polyline', 'P'], ['Bán kính', 'R'], ['Cắt', 'T'], ['Nhiều', 'M']];
    function first(){
      return ED.pick(ctx, cham ? 'Chọn đường thứ nhất' : 'Chọn đối tượng thứ nhất', { opts:opts, types:['line', 'pline'], bad:'Prototype: bo, vát giữa đoạn thẳng hoặc trên polyline' }).then(function(r){
        if (r.enter) return;
        if (r.kw === 'R') return ctx.val('Bán kính bo góc', TC.d(LAST.fillet)).then(function(v){ LAST.fillet = Math.max(0, Number(v) || 0); return first(); });
        if (r.kw === 'D') return ctx.val('Khoảng vát thứ nhất', TC.d(LAST.chamA)).then(function(v){ LAST.chamA = Math.max(0, Number(v) || 0); return ctx.val('Khoảng vát thứ hai', TC.d(LAST.chamA)).then(function(z){ LAST.chamB = Math.max(0, Number(z) || 0); return first(); }); });
        if (r.kw === 'A') return ctx.val('Khoảng vát trên đường thứ nhất', TC.d(LAST.chamA)).then(function(v){ LAST.chamA = Number(v) || 0; return ctx.val('Góc vát so với đường thứ nhất', '45').then(function(a){ LAST.chamB = LAST.chamA * Math.tan((Number(a) || 45) * Math.PI / 180); return first(); }); });
        if (r.kw === 'E') return ctx.kw('Phương pháp vát', [['Khoảng cách', 'D'], ['Góc', 'A']], 'D').then(first);
        if (r.kw === 'T') return ctx.kw('Chế độ cắt', [['Cắt', 'T'], ['Không cắt', 'N']], trim ? 'T' : 'N').then(function(k){ trim = LAST.trim = k === 'T'; return first(); });
        if (r.kw === 'M'){ multi = true; return first(); }
        if (r.kw === 'U'){ if (marks.length) ED.back(marks.pop()); return first(); }
        if (r.kw === 'P' || r.e.t === 'pline'){ var go = function(e){ marks.push(ED.mark()); var n = 0; ED.mod(e, function(x){ n = plCorner(x, LAST.fillet, cham ? LAST.chamA : 0); }); V().draw(); ctx.out(n + ' đỉnh đã được ' + (cham ? 'vát' : 'bo') + ' góc.'); return multi ? first() : null; }; if (r.e && r.e.t === 'pline') return go(r.e); return ED.pick(ctx, 'Chọn polyline 2D', { types:['pline'] }).then(function(q){ if (q.e) return go(q.e); }); }
        var l1 = r.e, p1 = r.p;
        return ED.pick(ctx, cham ? 'Chọn đường thứ hai, hoặc Shift+chọn để nối thành góc' : 'Chọn đối tượng thứ hai, hoặc Shift+chọn để tạo góc vuông', { types:['line'], onMove:function(p, temp, hov){ var e2 = hov && D().byId[hov]; if (e2 && e2 !== l1 && e2.t === 'line'){ var sh = window.event && window.event.shiftKey, res = corner(l1, p1, e2, p, sh ? 0 : LAST.fillet, cham && !sh ? [LAST.chamA, LAST.chamB] : null); if (res) temp.push(cornerTemp(res, l1, e2)); } } }).then(function(q){
          if (!q.e || q.e === l1) return multi ? first() : null;
          var res = corner(l1, p1, q.e, q.p, q.shift ? 0 : LAST.fillet, cham && !q.shift ? [LAST.chamA, LAST.chamB] : null);
          if (!res){ ctip(q, 'Hai đường song song, hoặc ' + (cham ? 'khoảng vát' : 'bán kính') + ' lớn hơn đoạn thẳng'); return multi ? first() : null; }
          marks.push(ED.mark()); cornerApply(res, l1, q.e, trim); return multi ? first() : null;
        });
      });
    }
    return ED.run(cham ? 'CHAMFER' : 'FILLET', first);
  }
  IMPL.FILLET = function(ctx){ return cornerCmd(ctx, false); };
  IMPL.CHAMFER = function(ctx){ return cornerCmd(ctx, true); };

  /* ---------- phá khối, nối, bẻ, kéo dài thêm ---------- */
  function insT(e){ var sx = e.sx || 1, sy = e.sy || sx, r = e.rot || 0, a = r * Math.PI / 180, f = function(p){ var x = p[0] * sx, y = p[1] * sy; return [e.p[0] + x * Math.cos(a) - y * Math.sin(a), e.p[1] + x * Math.sin(a) + y * Math.cos(a)]; };
    if (Math.abs(Math.abs(sx) - Math.abs(sy)) > 1e-9) return { f:f, rot:r, k:Math.sqrt(Math.abs(sx * sy)), nonuni:true };
    if (sx * sy > 0) return { f:f, rot:sx > 0 ? r : r + 180, k:Math.abs(sx) };
    return { f:f, rot:0, k:Math.abs(sx), mir:true, th:(sx > 0 ? r : r + 180) / 2 }; }
  function explode(d, e){
    if (e.t === 'insert'){ var bd = d.blocks[e.name]; if (!bd) return null; var t = insT(e);
      return bd.ents.map(function(be){ var c = ED.clean(be); delete c.id;
        if (c.t === 'attdef'){ c.t = 'text'; c.s = e.attrs && e.attrs[c.tag] != null ? e.attrs[c.tag] : c.s; delete c.tag; }
        if (c.layer === '0'){ c.layer = e.layer; if (c.color == null && e.color != null) c.color = e.color; }
        if (t.nonuni && (c.t === 'arc' || c.t === 'circle')){ var sw = c.t === 'circle' ? 360 : sweep(c.a0, c.a1), a0 = c.t === 'circle' ? 0 : c.a0, pts = []; for (var i = 0; i <= 24; i++) pts.push(t.f(polar(c.c, c.r, a0 + sw * i / 24))); return { t:'pline', pts:pts, closed:c.t === 'circle', layer:c.layer, color:c.color }; }
        return ED.xform(c, t); });
    }
    if (e.t === 'pline' || e.t === 'dim' || e.t === 'dimr' || e.t === 'dima' || e.t === 'mleader' || e.t === 'mtext' || e.t === 'spline'){
      var out = []; e._p.forEach(function(pr){ var base = { layer:e.layer }; if (e.color != null) base.color = e.color;
        if (pr.k === 'seg') out.push(Object.assign({ t:'line', p1:pr.a, p2:pr.b }, base));
        else if (pr.k === 'arc') out.push(Object.assign({ t:'arc', c:pr.c, r:pr.r, a0:pr.a0, a1:pr.a1 }, base));
        else if (pr.k === 'text') out.push(Object.assign({ t:'text', p:pr.p, h:pr.h, s:pr.s, rot:pr.rot, al:pr.al, style:pr.style }, base));
        else if (pr.k === 'tick') out.push(Object.assign({ t:'line', p1:[pr.p[0] - 90, pr.p[1] - 90], p2:[pr.p[0] + 90, pr.p[1] + 90] }, base)); });
      return e.t === 'spline' ? null : out;
    }
    return null;
  }
  IMPL.EXPLODE = function(ctx){ return selEdit(ctx).then(function(ids){ if (!ids.length) return; var d = D(), n = 0, skip = 0, made = 0;
    return ED.run('EXPLODE', function(){ ids.forEach(function(id){ var e = d.byId[id], parts = e && explode(d, e); if (!parts){ skip++; return; } var list = ED.listOf(d, e); ED.del(e); parts.forEach(function(p){ ED.add(p, list); made++; }); n++; }); TC.cad.clearSel(); ctx.out('Đã phá ' + n + ' đối tượng thành ' + made + ' đối tượng.' + (skip ? ' ' + skip + ' đối tượng không phá được (line, cung, tròn, chữ, hatch).' : '')); }); }); };
  // nối các đoạn có điểm cuối trùng nhau thành một polyline
  function segsOf(e){ if (e.t === 'line') return [{ a:e.p1, b:e.p2, g:0 }]; if (e.t === 'arc') return [{ a:polar(e.c, e.r, e.a0), b:polar(e.c, e.r, e.a1), g:Math.tan(sweep(e.a0, e.a1) * Math.PI / 720) }]; if (e.t === 'pline' && !e.closed){ var s = []; for (var i = 0; i < e.pts.length - 1; i++) s.push({ a:e.pts[i], b:e.pts[i + 1], g:(e.bulges || [])[i] || 0 }); return s; } return null; }
  ED.join = function(ents){
    var pool = ents.map(function(e){ return { e:e, s:segsOf(e) }; }).filter(function(x){ return x.s; }); if (pool.length < 2) return null;
    var tol = 1e-3, eq = function(p, q){ return G.dist(p, q) < tol; }, chain = pool.shift(), segs = chain.s.slice(), used = [chain.e], grew = true;
    var rev = function(s){ return s.slice().reverse().map(function(x){ return { a:x.b, b:x.a, g:-x.g }; }); };
    while (grew){ grew = false; for (var i = 0; i < pool.length; i++){ var s = pool[i].s, A = segs[0].a, B = segs[segs.length - 1].b, a = s[0].a, b = s[s.length - 1].b;
      if (eq(B, a)) segs = segs.concat(s); else if (eq(B, b)) segs = segs.concat(rev(s)); else if (eq(A, b)) segs = s.concat(segs); else if (eq(A, a)) segs = rev(s).concat(segs); else continue;
      used.push(pool[i].e); pool.splice(i, 1); grew = true; break; } }
    if (used.length < 2) return null;
    var closed = eq(segs[0].a, segs[segs.length - 1].b), pts = segs.map(function(x){ return x.a; }), bl = segs.map(function(x){ return x.g; });
    if (!closed) pts.push(segs[segs.length - 1].b);
    return { used:used, e:Object.assign(ED.clean(used[0]), { t:'pline', pts:pts, bulges:bl, closed:closed, p1:undefined, p2:undefined, c:undefined, r:undefined, a0:undefined, a1:undefined }) };
  };
  IMPL.JOIN = function(ctx){ return selEdit(ctx, 'Chọn đối tượng nguồn và các đối tượng cần nối').then(function(ids){ if (!ids.length) return; var d = D(), j = ED.join(ids.map(function(id){ return d.byId[id]; }));
    if (!j){ ctx.out('Không nối được: các đối tượng phải có điểm cuối trùng nhau (line, cung, polyline hở).'); return; }
    Object.keys(j.e).forEach(function(k){ if (j.e[k] === undefined) delete j.e[k]; }); delete j.e.id;
    return ED.run('JOIN', function(){ var list = ED.listOf(d, j.used[0]); j.used.forEach(function(e){ ED.del(e); }); ED.add(j.e, list); TC.cad.clearSel(); ctx.out('Đã nối ' + j.used.length + ' đối tượng thành 1 polyline' + (j.e.closed ? ' kín' : '') + (j.used.length < ids.length ? '; ' + (ids.length - j.used.length) + ' đối tượng không nối được' : '') + '.'); }); }); };
  IMPL.BREAK = function(ctx){ return ED.pick(ctx, 'Chọn đối tượng', { types:['line', 'circle', 'arc'], bad:'Prototype: bẻ line, cung, đường tròn' }).then(function(r){ if (!r.e) return; var e = r.e, p1 = r.p;
    return ctx.point('Điểm thứ hai', { opts:[['Điểm thứ nhất', 'F']] }).then(function(q){ var go = function(a, b){ return ED.run('BREAK', function(){ var list = ED.listOf(D(), e);
        if (e.t === 'line'){ var L2 = Math.pow(G.dist(e.p1, e.p2), 2), ta = TC.clamp(dot(sub(a, e.p1), sub(e.p2, e.p1)) / L2, 0, 1), tb = TC.clamp(dot(sub(b, e.p1), sub(e.p2, e.p1)) / L2, 0, 1), t0 = Math.min(ta, tb), t1 = Math.max(ta, tb), P0 = lerp(e.p1, e.p2, t0), P1 = lerp(e.p1, e.p2, t1), o2 = e.p2; if (t0 > 1e-6) ED.mod(e, function(x){ x.p2 = P0; }); else ED.del(e); if (t1 < 1 - 1e-6){ var n = Object.assign(ED.clean(e), { p1:P1, p2:o2 }); delete n.id; ED.add(n, list); } }
        else if (e.t === 'circle'){ var n2 = Object.assign(ED.clean(e), { t:'arc', a0:G.ang(e.c, b), a1:G.ang(e.c, a) }); delete n2.id; ED.del(e); ED.add(n2, list); }
        else { var a0 = e.a0, oa1 = e.a1, sA = sweep(a0, G.ang(e.c, a)), sB = sweep(a0, G.ang(e.c, b)), lo = Math.min(sA, sB), hi = Math.max(sA, sB); if (lo > 1e-6) ED.mod(e, function(x){ x.a1 = a0 + lo; }); else ED.del(e); if (hi < sweep(a0, oa1) - 1e-6){ var n3 = Object.assign(ED.clean(e), { a0:a0 + hi, a1:oa1 }); delete n3.id; ED.add(n3, list); } }
        ctx.out('Đã bẻ đối tượng.'); }); };
      if (q.kw === 'F') return ctx.point('Điểm thứ nhất').then(function(a){ if (!a.p) return; return ctx.point('Điểm thứ hai', { base:a.p }).then(function(b){ if (b.p) return go(a.p, b.p); }); });
      if (q.p) return go(p1, q.p); }); }); };
  IMPL.BREAKATPOINT = function(ctx){ return ED.pick(ctx, 'Chọn đối tượng', { types:['line', 'arc', 'pline'] }).then(function(r){ if (!r.e) return; var e = r.e;
    return ctx.point('Điểm bẻ', { onMove:function(p, temp){ temp.push({ k:'pt', p:p, color:TC.cssv('--c-grip-hot') }); } }).then(function(q){ if (!q.p) return; return ED.run('BREAKATPOINT', function(){ var list = ED.listOf(D(), e);
      if (e.t === 'line'){ var f = G.foot(q.p, e.p1, e.p2), P = lerp(e.p1, e.p2, TC.clamp(f.t, 0, 1)), o2 = e.p2; ED.mod(e, function(x){ x.p2 = P; }); var n = Object.assign(ED.clean(e), { p1:P, p2:o2 }); delete n.id; ED.add(n, list); }
      else if (e.t === 'arc'){ var A = G.ang(e.c, q.p), o1 = e.a1; ED.mod(e, function(x){ x.a1 = A; }); var n2 = Object.assign(ED.clean(e), { a0:A, a1:o1 }); delete n2.id; ED.add(n2, list); }
      else { var best = 0, bd = Infinity, tt = 0; for (var i = 0; i < e.pts.length - 1; i++){ var ff = G.foot(q.p, e.pts[i], e.pts[i + 1]), dq = G.dist(q.p, lerp(e.pts[i], e.pts[i + 1], TC.clamp(ff.t, 0, 1))); if (dq < bd){ bd = dq; best = i; tt = TC.clamp(ff.t, 0, 1); } } var M = lerp(e.pts[best], e.pts[best + 1], tt), A2 = e.pts.slice(0, best + 1).concat([M]), B2 = [M].concat(e.pts.slice(best + 1)); ED.mod(e, function(x){ x.pts = A2; x.bulges = []; x.closed = false; }); var n3 = Object.assign(ED.clean(e), { pts:B2, bulges:[] }); delete n3.id; ED.add(n3, list); }
      ctx.out('Đã bẻ thành 2 đối tượng.'); }); }); }); };
  IMPL.LENGTHEN = function(ctx){
    var mode = LAST.lenMode || 'T', val = LAST.lenVal || 1000;
    return ED.pick(ctx, 'Chọn đối tượng cần đo', { opts:[['Thêm bớt', 'DE'], ['Phần trăm', 'P'], ['Tổng', 'T'], ['Động', 'DY']], def:{ DE:'Thêm bớt', P:'Phần trăm', T:'Tổng', DY:'Động' }[mode], types:['line', 'arc'] }).then(function(r){
      if (r.e){ var e = r.e; ctx.out('Độ dài hiện tại: ' + TC.d(e.t === 'line' ? G.dist(e.p1, e.p2) : e.r * sweep(e.a0, e.a1) * Math.PI / 180) + (e.t === 'arc' ? ', góc bao: ' + TC.d(sweep(e.a0, e.a1)) + '°' : '')); return IMPL.LENGTHEN(ctx); }
      var k = r.kw || mode; LAST.lenMode = k;
      var ask = k === 'DY' ? Promise.resolve(null) : ctx.val(k === 'DE' ? 'Độ dài thêm (âm để bớt)' : k === 'P' ? 'Phần trăm độ dài' : 'Tổng độ dài', TC.d(k === 'P' ? 100 : val)).then(function(v){ val = Number(v) || 0; LAST.lenVal = val; return val; });
      return ask.then(function(){ return ED.run('LENGTHEN', function loop(){ return ED.pick(ctx, 'Chọn đối tượng cần đổi độ dài (gần đầu muốn kéo)', { types:['line'], opts:[['Hoàn tác', 'U']] }).then(function(q){
        if (!q.e) return; var e = q.e, endB = G.dist(q.p, e.p2) < G.dist(q.p, e.p1), A = endB ? e.p1 : e.p2, B = endB ? e.p2 : e.p1, L = G.dist(A, B), u = unit(sub(B, A));
        var newLen = function(p){ return k === 'DE' ? L + val : k === 'P' ? L * val / 100 : k === 'T' ? val : dot(sub(p, A), u); };
        var go = function(nl){ if (nl <= 0){ ctip(q, 'Độ dài mới phải lớn hơn 0'); return loop(); } ED.mod(e, function(x){ var Q = add2(A, u, nl); if (endB) x.p2 = Q; else x.p1 = Q; }); V().draw(); return loop(); };
        if (k === 'DY') return ctx.point('Điểm cuối mới', { base:A, onMove:function(p, temp){ temp.push({ k:'line', a:A, b:add2(A, u, Math.max(1, newLen(p))), color:TC.cssv('--c-accent'), w:1.6 }); } }).then(function(z){ if (z.p) return go(newLen(z.p)); });
        return go(newLen()); }); }); });
    });
  };

  // sửa polyline (UI-E10 PEDIT): đóng/mở, nối, bề rộng, sửa đỉnh, khớp cong, bỏ cong, đảo chiều
  IMPL.PEDIT = function(ctx, pre){
    ctx.badge('PE');
    var start = pre && pre.t ? Promise.resolve({ e:pre }) : ED.pick(ctx, 'Chọn polyline', { opts:[['Nhiều', 'M']], types:['pline', 'line', 'arc'] });
    return start.then(function(r){
      if (r.kw === 'M'){ ctx.out('Chọn nhiều: các thao tác áp dụng cho mọi polyline đã chọn (MPEDIT).'); return IMPL.PEDIT(ctx); }
      if (!r.e) return; var e = r.e, marks = [];
      var conv = e.t === 'pline' ? Promise.resolve(e) : ctx.kw('Đối tượng chọn không phải polyline. Chuyển thành polyline?', [['Có', 'Y'], ['Không', 'N']], 'Y').then(function(k){ if (k !== 'Y') return null; var seg = e.t === 'line' ? { pts:[e.p1, e.p2], bulges:[0] } : { pts:[polar(e.c, e.r, e.a0), polar(e.c, e.r, e.a1)], bulges:[Math.tan(sweep(e.a0, e.a1) * Math.PI / 720)] }; var n = Object.assign({ t:'pline', layer:e.layer, closed:false }, seg); if (e.color != null) n.color = e.color; var list = ED.listOf(D(), e); ED.del(e); return ED.add(n, list); });
      return ED.run('PEDIT', function(){ return conv.then(function(pl){ if (!pl) return; e = pl; V().draw(); return menu(); }); });
      function menu(){
        var o = [[e.closed ? 'Mở' : 'Đóng', e.closed ? 'O' : 'C'], ['Nối', 'J'], ['Bề rộng', 'W'], ['Sửa đỉnh', 'E'], ['Khớp cong', 'F'], ['Spline', 'S'], ['Bỏ cong', 'D'], ['Tạo linetype', 'L'], ['Đảo chiều', 'R'], ['Hoàn tác', 'U']];
        return ctx.kw('Nhập một tùy chọn', o, '').then(function(k){
          var mk = ED.mark();
          if (!k) return;
          if (k === 'U'){ if (marks.length) ED.back(marks.pop()); return menu(); }
          marks.push(mk);
          if (k === 'C' || k === 'O'){ ED.mod(e, function(x){ x.closed = k === 'C'; }); }
          else if (k === 'R'){ ED.mod(e, function(x){ x.pts.reverse(); if (x.bulges){ var n = x.pts.length, b = x.bulges, nb = []; for (var i = 0; i < n - 1; i++) nb[i] = -(b[n - 2 - i] || 0); if (x.closed) nb[n - 1] = -(b[n - 1] || 0); x.bulges = nb; } }); ctx.out('Đã đảo chiều polyline.'); }
          else if (k === 'W') return ctx.val('Bề rộng mới cho mọi đoạn', TC.d(e.w || 0)).then(function(v){ ED.mod(e, function(x){ x.w = Math.max(0, Number(v) || 0); }); V().draw(); return menu(); });
          else if (k === 'F' || k === 'S'){ ED.mod(e, function(x){ if (!x.fitOrig) x.fitOrig = { pts:x.pts.slice(), bulges:(x.bulges || []).slice() }; x.pts = TC.splinePts(x.fitOrig.pts, x.closed); if (x.closed) x.pts.pop(); x.bulges = []; }); ctx.out(k === 'F' ? 'Đã khớp cong qua các đỉnh.' : 'Đã khớp spline theo các đỉnh điều khiển.'); }
          else if (k === 'D'){ if (e.fitOrig) ED.mod(e, function(x){ x.pts = x.fitOrig.pts; x.bulges = x.fitOrig.bulges; delete x.fitOrig; }); else ED.mod(e, function(x){ x.bulges = []; }); ctx.out('Đã bỏ cong.'); }
          else if (k === 'L') return ctx.kw('Tạo linetype liên tục qua đỉnh', [['Bật', 'ON'], ['Tắt', 'OFF']], 'OFF').then(menu);
          else if (k === 'J') return ctx.sel('Chọn đối tượng cần nối').then(function(ids){ var d = D(), j = ED.join([e].concat(ids.map(function(id){ return d.byId[id]; }).filter(function(x){ return x && x !== e; }))); if (!j || j.used.length < 2){ ctx.out('0 đoạn được nối vào polyline.'); return menu(); } Object.keys(j.e).forEach(function(kk){ if (j.e[kk] === undefined) delete j.e[kk]; }); var keep = j.e; j.used.forEach(function(x){ if (x !== e) ED.del(x); }); ED.mod(e, function(x){ x.pts = keep.pts; x.bulges = keep.bulges; x.closed = keep.closed; }); ctx.out((j.used.length - 1) + ' đoạn đã được nối vào polyline.'); V().draw(); return menu(); });
          else if (k === 'E') return vertex(0).then(menu);
          V().draw(); return menu();
        });
      }
      function vertex(i){ var mark = function(){ V().temp = [{ k:'pt', p:e.pts[i], color:TC.cssv('--c-grip-hot') }, { k:'poly', pts:[[e.pts[i][0] - 120, e.pts[i][1] - 120], [e.pts[i][0] + 120, e.pts[i][1] + 120]], color:TC.cssv('--c-grip-hot'), w:2 }, { k:'poly', pts:[[e.pts[i][0] - 120, e.pts[i][1] + 120], [e.pts[i][0] + 120, e.pts[i][1] - 120]], color:TC.cssv('--c-grip-hot'), w:2 }]; V().drawOver(); }; mark();
        return ctx.kw('Đỉnh ' + (i + 1) + '/' + e.pts.length, [['Kế', 'N'], ['Trước', 'P'], ['Bẻ', 'B'], ['Chèn', 'I'], ['Di chuyển', 'M'], ['Tạo lại', 'R'], ['Duỗi thẳng', 'S'], ['Tiếp tuyến', 'T'], ['Bề rộng', 'W'], ['Thoát', 'X']], 'N').then(function(k){
          var n = e.pts.length;
          if (k === 'N') return vertex((i + 1) % n); if (k === 'P') return vertex((i - 1 + n) % n);
          if (k === 'M') return ctx.point('Vị trí mới của đỉnh', { base:e.pts[i] }).then(function(q){ if (q.p) ED.mod(e, function(x){ x.pts[i] = q.p; }); V().draw(); return vertex(i); });
          if (k === 'I') return ctx.point('Vị trí đỉnh mới', { base:e.pts[i] }).then(function(q){ if (q.p) ED.mod(e, function(x){ x.pts.splice(i + 1, 0, q.p); if (x.bulges) x.bulges.splice(i + 1, 0, 0); }); V().draw(); return vertex(i + 1); });
          if (k === 'S'){ ED.mod(e, function(x){ if (x.bulges) x.bulges[i] = 0; }); V().draw(); return vertex(i); }
          if (k === 'X'){ V().temp = []; V().drawOver(); return; }
          ctx.out('Tùy chọn này dùng prompt như AutoCAD; prototype minh họa Kế, Trước, Di chuyển, Chèn, Duỗi thẳng.'); return vertex(i);
        }); }
    });
  };
})();
