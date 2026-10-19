/* TrionCAD prototype · vùng vẽ: dựng hình (Canvas 2D), biến đổi tọa độ, chọn đối tượng, bắt điểm (07.1 §9). */
(function(){
  'use strict';
  var TC = window.TC;

  /* ---------- bảng màu ACI 1–255 ---------- */
  var ACI = [null, '#FF0000','#FFFF00','#00FF00','#00FFFF','#0000FF','#FF00FF','#FFFFFF','#808080','#C0C0C0'];
  function hsv(h, s, v){ var f = function(n){ var k = (n + h / 60) % 6; return Math.round((v - v * s * Math.max(0, Math.min(k, 4 - k, 1))) * 255); }; return '#' + [f(5), f(3), f(1)].map(function(x){ return ('0' + x.toString(16)).slice(-2); }).join('').toUpperCase(); }
  var SV = [[1,1],[.5,1],[1,.8],[.5,.8],[1,.6],[.5,.6],[1,.5],[.5,.5],[1,.3],[.5,.3]];
  for (var i = 10; i < 250; i++) ACI[i] = hsv((Math.floor(i / 10) - 1) * 15, SV[i % 10][0], SV[i % 10][1]);
  [51, 80, 105, 130, 190, 255].forEach(function(g, k){ var h = ('0' + g.toString(16)).slice(-2).toUpperCase(); ACI[250 + k] = '#' + h + h + h; });
  TC.ACI = ACI;
  TC.aciName = function(n){ return { 1:'Đỏ', 2:'Vàng', 3:'Xanh lá', 4:'Xanh lơ', 5:'Xanh dương', 6:'Tím', 7:'Trắng' }[n] || ('Màu ' + n); };

  var LT = { CENTER:[1200, 200, 200, 200], DASHED:[600, 300], HIDDEN:[300, 150] };

  /* ---------- trải phẳng thực thể thành nguyên tố hình học trong tọa độ thế giới ---------- */
  function xf(ins){
    var r = (ins.rot || 0) * Math.PI / 180, c = Math.cos(r), s = Math.sin(r), sx = ins.sx || 1, sy = ins.sy || sx;
    return { m:function(p){ var x = p[0] * sx, y = p[1] * sy; return [ins.p[0] + x * c - y * s, ins.p[1] + x * s + y * c]; }, rot:ins.rot || 0, sx:sx, sy:sy, k:Math.abs(sy) };
  }
  TC.flatExt = TC.flatExt || {};
  function angMap(a, t){ var m = t.sx < 0 && t.sy < 0 ? 180 + a : t.sx < 0 ? 180 - a : t.sy < 0 ? -a : a; return m + t.rot; }
  function flat(doc, e, t, out, host){
    var P = t ? t.m : function(p){ return p; };
    var o = function(pr){ pr.e = host || e; pr.src = e; out.push(pr); };
    switch (e.t){
      case 'line': o({ k:'seg', a:P(e.p1), b:P(e.p2) }); break;
      case 'pline': { var pts = e.pts.map(P), n = pts.length, bg = e.bulges || [], seg = function(a, b, i){ var B = bg[i] || 0; if (!B){ o({ k:'seg', a:a, b:b, vi:i, w:e.w }); return; } if (t && (t.sx < 0) !== (t.sy < 0)) B = -B; var th = 4 * Math.atan(B), c = Math.hypot(b[0] - a[0], b[1] - a[1]), r = c / (2 * Math.sin(Math.abs(th) / 2)), mx = (a[0] + b[0]) / 2, my = (a[1] + b[1]) / 2, ux = (b[0] - a[0]) / c, uy = (b[1] - a[1]) / c, hh = r * Math.cos(th / 2) * (B > 0 ? 1 : -1), cc = [mx - uy * hh, my + ux * hh], A0 = Math.atan2(a[1] - cc[1], a[0] - cc[0]) * 180 / Math.PI, A1 = Math.atan2(b[1] - cc[1], b[0] - cc[0]) * 180 / Math.PI; o(B > 0 ? { k:'arc', c:cc, r:r, a0:A0, a1:A1, vi:i, w:e.w } : { k:'arc', c:cc, r:r, a0:A1, a1:A0, vi:i, w:e.w }); };
        for (var i = 0; i < n - 1; i++) seg(pts[i], pts[i + 1], i); if (e.closed && n > 2) seg(pts[n - 1], pts[0], n - 1); break; }
      case 'circle': o({ k:'circ', c:P(e.c), r:e.r * (t ? t.k : 1) }); break;
      case 'spline': { var sp = splinePts(e.pts, e.closed).map(P); for (var si = 0; si < sp.length - 1; si++) o({ k:'seg', a:sp[si], b:sp[si + 1], sm:1, first:si === 0, last:si === sp.length - 2 }); break; }
      case 'xline': case 'ray': { var q0 = P(e.p), dd = e.dir, far = 1e7; o({ k:'seg', a:e.t === 'ray' ? q0 : [q0[0] - dd[0] * far, q0[1] - dd[1] * far], b:[q0[0] + dd[0] * far, q0[1] + dd[1] * far], inf:1 }); break; }
      case 'point': o({ k:'pt', p:P(e.p) }); break;
      // bảng v1.0 (kiểu bảng, kiểu ô, ô gộp hai chiều, công thức, field) do screens9.js vẽ; trang không nạp screens9.js vẽ kiểu cũ dưới đây
      case 'table': { if (TC.flatTable){ TC.flatTable({ doc:doc, e:e, t:t, P:P, o:o, sub:function(x){ flat(doc, x, t, out, host || e); } }); break; } var X0 = [e.p[0]], Y0 = [e.p[1]], mg = e.merge || [], th = e.th || 250; e.cols.forEach(function(w){ X0.push(X0[X0.length - 1] + w); }); e.rows.forEach(function(h){ Y0.push(Y0[Y0.length - 1] - h); }); var nc = e.cols.length, nr = e.rows.length, merged = function(r, j){ return mg.some(function(m){ return m[0] === r && j > m[1] && j <= m[2]; }); };
        Y0.forEach(function(y){ o({ k:'seg', a:P([X0[0], y]), b:P([X0[nc], y]) }); }); o({ k:'seg', a:P([X0[0], Y0[0]]), b:P([X0[0], Y0[nr]]) }); o({ k:'seg', a:P([X0[nc], Y0[0]]), b:P([X0[nc], Y0[nr]]) });
        for (var tr = 0; tr < nr; tr++) for (var tj = 1; tj < nc; tj++) if (!merged(tr, tj)) o({ k:'seg', a:P([X0[tj], Y0[tr]]), b:P([X0[tj], Y0[tr + 1]]) });
        (e.cells || []).forEach(function(row, r2){ row.forEach(function(txt, c2){ if (txt == null || txt === '') return; var m2 = mg.filter(function(m){ return m[0] === r2 && m[1] === c2; })[0], xa = X0[c2], xb = X0[m2 ? m2[2] + 1 : c2 + 1], hd = r2 < (e.head || 0), num2 = !hd && /^[\d.,\s]+$/.test(String(txt)); o({ k:'text', p:P([num2 ? xb - th * .5 : hd ? (xa + xb) / 2 : xa + th * .5, (Y0[r2] + Y0[r2 + 1]) / 2]), h:hd ? th * 1.1 : th, s:String(txt), rot:0, al:num2 ? 'rm' : hd ? 'm' : 'lm', style:e.style || 'Standard', bold:hd }); }); });
        break; }
      case 'vpframe': { var vf = null; doc.layouts.forEach(function(L){ L.vports.forEach(function(q){ if (q.id === e.vp) vf = q; }); }); if (vf){ var vc = [[vf.x, vf.y], [vf.x + vf.w, vf.y], [vf.x + vf.w, vf.y + vf.h], [vf.x, vf.y + vf.h]]; for (var vi = 0; vi < 4; vi++) o({ k:'seg', a:vc[vi], b:vc[(vi + 1) % 4] }); } break; }
      case 'dima': { var ca = P(e.c), sw2 = ((e.a1 - e.a0) % 360 + 360) % 360, am = (e.a0 + sw2 / 2) * Math.PI / 180, ra = e.r; o({ k:'arc', c:ca, r:ra, a0:e.a0, a1:e.a1 }); [e.a0, e.a1].forEach(function(an){ o({ k:'tick', p:[ca[0] + ra * Math.cos(an * Math.PI / 180), ca[1] + ra * Math.sin(an * Math.PI / 180)] }); }); var rd = (e.a0 + sw2 / 2 + 270) % 360; if (rd > 90 && rd < 270) rd -= 180; o({ k:'text', p:[ca[0] + (ra + 230) * Math.cos(am), ca[1] + (ra + 230) * Math.sin(am)], h:220, s:e.over || (Math.round(sw2 * 10) / 10) + '°', rot:rd, al:'m', style:'Standard' }); e._meas = sw2; break; }
      case 'arc': { var a0 = t ? angMap(e.a0, t) : e.a0, a1 = t ? angMap(e.a1, t) : e.a1; if (t && (t.sx < 0) !== (t.sy < 0)) { var tmp = a0; a0 = a1; a1 = tmp; } o({ k:'arc', c:P(e.c), r:e.r * (t ? t.k : 1), a0:a0, a1:a1 }); break; }
      case 'ellipse': o({ k:'ell', c:P(e.c), rx:e.rx * (t ? Math.abs(t.sx) : 1), ry:e.ry * (t ? t.k : 1), rot:(e.rot || 0) + (t ? t.rot : 0), a0:e.a0, a1:e.a1 }); break;
      case 'mtext': { var lines = mtextLines(e), mr = (e.rot || 0) * Math.PI / 180, mal = e.al === 'c' ? .5 : e.al === 'r' ? 1 : 0; lines.forEach(function(s2, li){ var ox = e.w * mal, oy = -e.h * (1 + li * (e.ls || 1.6)); o({ k:'text', p:P([e.p[0] + ox * Math.cos(mr) - oy * Math.sin(mr), e.p[1] + ox * Math.sin(mr) + oy * Math.cos(mr)]), h:e.h * (t ? t.k : 1), s:e.caps ? s2.toUpperCase() : s2, rot:(e.rot || 0) + (t ? t.rot : 0), al:e.al === 'c' ? 'c' : e.al === 'r' ? 'r' : 'l', style:e.style, bold:e.bold, ital:e.ital }); }); if (!lines.length) o({ k:'seg', a:P(e.p), b:P([e.p[0] + e.w, e.p[1]]) }); break; }
      case 'mleader': { var a = P(e.pts[0]), b = P(e.pts[1]), dir = b[0] >= a[0] ? 1 : -1, l = P([e.pts[1][0] + dir * e.h * 1.2, e.pts[1][1]]); o({ k:'seg', a:b, b:l }); [a].concat((e.more || []).map(P)).forEach(function(a2){ o({ k:'seg', a:a2, b:b }); var an = Math.atan2(b[1] - a2[1], b[0] - a2[0]), z = e.h * .9; o({ k:'tri', pts:[a2, [a2[0] + z * Math.cos(an + .3), a2[1] + z * Math.sin(an + .3)], [a2[0] + z * Math.cos(an - .3), a2[1] + z * Math.sin(an - .3)]] }); }); String(e.s || '').split('\n').forEach(function(s2, li){ o({ k:'text', p:[l[0] + dir * e.h * .4, l[1] + e.h * .35 - li * e.h * 1.5], h:e.h, s:s2, rot:0, al:dir > 0 ? 'l' : 'r', style:e.style || 'Standard' }); }); break; }
      case 'dimr': { var c0 = P(e.c), ra = (e.ang || 45) * Math.PI / 180, q = [c0[0] + e.r * Math.cos(ra), c0[1] + e.r * Math.sin(ra)], q0 = e.dia ? [c0[0] - e.r * Math.cos(ra), c0[1] - e.r * Math.sin(ra)] : c0; o({ k:'seg', a:q0, b:q }); o({ k:'tick', p:q }); if (e.dia) o({ k:'tick', p:q0 }); o({ k:'text', p:[(q0[0] + q[0]) / 2 - 120 * Math.sin(ra), (q0[1] + q[1]) / 2 + 120 * Math.cos(ra)], h:220, s:(e.dia ? 'Ø' : 'R') + Math.round(e.dia ? e.r * 2 : e.r), rot:((e.ang || 45) + 90) % 180 - 90, al:'m', style:'Standard' }); break; }
      case 'image': { var w = e.w, h2 = e.h, c4 = [[0, 0], [w, 0], [w, h2], [0, h2]].map(function(q){ var r2 = (e.rot || 0) * Math.PI / 180; return P([e.p[0] + q[0] * Math.cos(r2) - q[1] * Math.sin(r2), e.p[1] + q[0] * Math.sin(r2) + q[1] * Math.cos(r2)]); }); if (e.clip && !e.rot){ var cl = e.clip; c4 = [[Math.max(c4[0][0], cl[0]), Math.max(c4[0][1], cl[1])], [Math.min(c4[1][0], cl[2]), Math.max(c4[1][1], cl[1])], [Math.min(c4[2][0], cl[2]), Math.min(c4[2][1], cl[3])], [Math.max(c4[3][0], cl[0]), Math.min(c4[3][1], cl[3])]]; } o({ k:'img', pts:c4, name:e.name, fade:e.fade || 0, bright:e.bright == null ? 50 : e.bright, contrast:e.contrast == null ? 50 : e.contrast, show:e.show !== false }); if (e.frame !== false) for (var j = 0; j < 4; j++) o({ k:'seg', a:c4[j], b:c4[(j + 1) % 4] }); break; }
      case 'text': case 'attdef': {
        if (e.t === 'attdef' && !host){ o({ k:'text', p:P(e.p), h:e.h, s:e.tag, rot:e.rot || 0, al:e.al || 'l', style:e.style, tag:e.tag }); break; }
        var s = e.s; if (e.t === 'attdef' && host && host.attrs && host.attrs[e.tag] != null) s = host.attrs[e.tag];
        o({ k:'text', p:P(e.p), h:e.h * (t ? t.k : 1), s:s, rot:(e.rot || 0) + (t ? t.rot : 0), al:e.al || 'l', style:e.style, tag:e.tag }); break; }
      case 'hatch': o({ k:'hatch', loops:e.loops.map(function(l){ return l.map(P); }), pat:e.pat, sc:(e.sc || 1) * (t ? t.k : 1), ang:e.ang || 0 }); break;
      case 'dim': {
        if (e.dir === 'a'){ var A1 = P(e.p1), B1 = P(e.p2), L1 = Math.hypot(B1[0] - A1[0], B1[1] - A1[1]) || 1, ux = (B1[0] - A1[0]) / L1, uy = (B1[1] - A1[1]) / L1, nx = -uy, ny = ux, of = e.off || 0, sg = of < 0 ? -1 : 1, D1 = [A1[0] + nx * of, A1[1] + ny * of], D2 = [B1[0] + nx * of, B1[1] + ny * of];
          o({ k:'seg', a:A1, b:[D1[0] + nx * 160 * sg, D1[1] + ny * 160 * sg] }); o({ k:'seg', a:B1, b:[D2[0] + nx * 160 * sg, D2[1] + ny * 160 * sg] }); o({ k:'seg', a:[D1[0] - ux * 200, D1[1] - uy * 200], b:[D2[0] + ux * 200, D2[1] + uy * 200] }); o({ k:'tick', p:D1 }); o({ k:'tick', p:D2 });
          var ra2 = Math.atan2(uy, ux) * 180 / Math.PI, fl = ra2 > 90 || ra2 <= -90; if (fl) ra2 += ra2 > 0 ? -180 : 180; var tn = fl ? -1 : 1; e._meas = L1;
          o({ k:'text', p:[(D1[0] + D2[0]) / 2 + nx * 210 * tn + (e.tdx || 0), (D1[1] + D2[1]) / 2 + ny * 210 * tn + (e.tdy || 0)], h:220, s:e.over ? e.over.replace('<>', String(Math.round(L1))) : String(Math.round(L1)), rot:ra2, al:'m', style:'Standard' }); break; }
        var a = e.p1, b = e.p2, h = e.dir === 'h';
        var d1 = h ? [a[0], e.pos] : [e.pos, a[1]], d2 = h ? [b[0], e.pos] : [e.pos, b[1]], ov = 160 * (h ? (e.pos < a[1] ? -1 : 1) : (e.pos < a[0] ? -1 : 1));
        o({ k:'seg', a:a, b:h ? [a[0], e.pos + ov] : [e.pos + ov, a[1]] }); o({ k:'seg', a:b, b:h ? [b[0], e.pos + ov] : [e.pos + ov, b[1]] });
        o({ k:'seg', a:h ? [d1[0] - 200, d1[1]] : [d1[0], d1[1] - 200], b:h ? [d2[0] + 200, d2[1]] : [d2[0], d2[1] + 200] });
        o({ k:'tick', p:d1 }); o({ k:'tick', p:d2 });
        var len = h ? Math.abs(b[0] - a[0]) : Math.abs(b[1] - a[1]); e._meas = len;
        var tp = h ? [(d1[0] + d2[0]) / 2 + (e.tdx || 0), e.pos + 210 + (e.tdy || 0)] : [e.pos - 210 + (e.tdx || 0), (d1[1] + d2[1]) / 2 + (e.tdy || 0)];
        o({ k:'text', p:tp, h:220, s:e.over ? e.over.replace('<>', String(Math.round(len))) : String(Math.round(len)), rot:h ? 0 : 90, al:'m', style:'Standard' });
        break; }
      case 'insert': {
        var bd = doc.blocks[e.name]; if (!bd) break; var tt = xf(e), n0 = out.length;
        bd.ents.forEach(function(be){ if (be.t === 'attdef' && doc.attdisp === 'OFF') return; flat(doc, be, tt, out, e); });
        if (e.clip){ var cb = e.clip, keep = out.splice(n0).map(function(pr){ return clipPrim(pr, cb); }).filter(Boolean); Array.prototype.push.apply(out, keep); o({ k:'seg', a:[cb[0], cb[1]], b:[cb[2], cb[1]], clipb:1 }); o({ k:'seg', a:[cb[2], cb[1]], b:[cb[2], cb[3]], clipb:1 }); o({ k:'seg', a:[cb[2], cb[3]], b:[cb[0], cb[3]], clipb:1 }); o({ k:'seg', a:[cb[0], cb[3]], b:[cb[0], cb[1]], clipb:1 }); }
        o({ k:'ins', p:e.p }); break; }
      // loại đối tượng do các tệp Đợt 4 đăng ký (TC.flatExt[t]): ghép từ các nguyên tố có sẵn (seg, arc, circ, text, hatch, img…)
      default: if (TC.flatExt[e.t]) TC.flatExt[e.t]({ doc:doc, e:e, t:t, P:P, o:o, host:host, sub:function(x, tt, h){ flat(doc, x, tt || t, out, h || host || e); } });
    }
  }
  // cắt nguyên tố theo khung chữ nhật [x0, y0, x1, y1] (clip xref, block)
  function clipPrim(pr, b){
    var inR = function(q){ return q[0] >= b[0] && q[0] <= b[2] && q[1] >= b[1] && q[1] <= b[3]; };
    if (pr.k === 'seg'){ var t0 = 0, t1 = 1, d = [pr.b[0] - pr.a[0], pr.b[1] - pr.a[1]], P2 = [-d[0], d[0], -d[1], d[1]], Q = [pr.a[0] - b[0], b[2] - pr.a[0], pr.a[1] - b[1], b[3] - pr.a[1]]; for (var i = 0; i < 4; i++){ if (P2[i] === 0){ if (Q[i] < 0) return null; continue; } var r = Q[i] / P2[i]; if (P2[i] < 0){ if (r > t1) return null; if (r > t0) t0 = r; } else { if (r < t0) return null; if (r < t1) t1 = r; } } return Object.assign({}, pr, { a:[pr.a[0] + d[0] * t0, pr.a[1] + d[1] * t0], b:[pr.a[0] + d[0] * t1, pr.a[1] + d[1] * t1] }); }
    if (pr.k === 'circ' || pr.k === 'arc' || pr.k === 'ell') return inR(pr.c) ? pr : null;
    if (pr.k === 'text' || pr.k === 'tick' || pr.k === 'pt') return inR(pr.p) ? pr : null;
    if (pr.k === 'hatch') return pr.loops.every(function(l){ return l.every(inR); }) ? pr : null;
    return pr;
  }
  function bbox(prims){
    var b = [Infinity, Infinity, -Infinity, -Infinity];
    function add(x, y){ if (x < b[0]) b[0] = x; if (y < b[1]) b[1] = y; if (x > b[2]) b[2] = x; if (y > b[3]) b[3] = y; }
    prims.forEach(function(p){
      if (p.k === 'seg'){ add(p.a[0], p.a[1]); add(p.b[0], p.b[1]); }
      else if (p.k === 'circ' || p.k === 'arc'){ add(p.c[0] - p.r, p.c[1] - p.r); add(p.c[0] + p.r, p.c[1] + p.r); }
      else if (p.k === 'ell'){ var m = Math.max(p.rx, p.ry); add(p.c[0] - m, p.c[1] - m); add(p.c[0] + m, p.c[1] + m); }
      else if (p.k === 'text'){ var w = p.h * 0.62 * p.s.length; add(p.p[0] - w, p.p[1] - p.h); add(p.p[0] + w, p.p[1] + p.h); }
      else if (p.k === 'hatch') p.loops.forEach(function(l){ l.forEach(function(q){ add(q[0], q[1]); }); });
      else if (p.k === 'tick' || p.k === 'ins' || p.k === 'pt') add(p.p[0], p.p[1]);
      else if (p.k === 'tri' || p.k === 'img') p.pts.forEach(function(q){ add(q[0], q[1]); });
    });
    return b;
  }
  function splinePts(pts, closed){ var n = pts.length; if (n < 3) return pts.slice(); var out = [], P0 = function(i){ return closed ? pts[(i + n) % n] : pts[Math.max(0, Math.min(n - 1, i))]; }, m = closed ? n : n - 1; for (var i = 0; i < m; i++){ var a = P0(i - 1), b = P0(i), c = P0(i + 1), d = P0(i + 2); for (var k = 0; k < 10; k++){ var t = k / 10, t2 = t * t, t3 = t2 * t; out.push([0.5 * (2 * b[0] + (-a[0] + c[0]) * t + (2 * a[0] - 5 * b[0] + 4 * c[0] - d[0]) * t2 + (-a[0] + 3 * b[0] - 3 * c[0] + d[0]) * t3), 0.5 * (2 * b[1] + (-a[1] + c[1]) * t + (2 * a[1] - 5 * b[1] + 4 * c[1] - d[1]) * t2 + (-a[1] + 3 * b[1] - 3 * c[1] + d[1]) * t3)]); } } out.push(closed ? pts[0] : pts[n - 1]); return out; }
  TC.splinePts = splinePts;
  // MTEXT: ngắt dòng theo bề rộng khung (ước lượng 0,62 × chiều cao cho mỗi ký tự)
  function mtextLines(e){ var per = Math.max(4, Math.floor(e.w / (e.h * .62))), out = []; String(e.s || '').split('\n').forEach(function(par){ var cur = ''; par.split(' ').forEach(function(w){ if (cur && (cur + ' ' + w).length > per){ out.push(cur); cur = w; } else cur = cur ? cur + ' ' + w : w; }); out.push(cur); }); return out; }
  TC.mtextLines = mtextLines;
  // tính lại một đối tượng sau khi sửa; temp = đối tượng xem trước, không đăng ký vào bản vẽ
  TC.prepEnt = function(doc, e, temp){ var out = []; flat(doc, e, null, out, null); e._p = out; e._bb = bbox(out); if (!temp) doc.byId[e.id] = e; return e; };
  TC.prepDoc = function(doc){
    function prep(list){ list.forEach(function(e){ var out = []; flat(doc, e, null, out, null); e._p = out; e._bb = bbox(out); }); }
    prep(doc.ents); doc.layouts.forEach(function(l){ prep(l.ents); });
    var b = [Infinity, Infinity, -Infinity, -Infinity];
    doc.ents.forEach(function(e){ if (e._bb[0] < b[0]) b[0] = e._bb[0]; if (e._bb[1] < b[1]) b[1] = e._bb[1]; if (e._bb[2] > b[2]) b[2] = e._bb[2]; if (e._bb[3] > b[3]) b[3] = e._bb[3]; });
    if (!isFinite(b[0])) b = doc.initView ? [doc.initView.c[0] - doc.initView.h * .7, doc.initView.c[1] - doc.initView.h / 2, doc.initView.c[0] + doc.initView.h * .7, doc.initView.c[1] + doc.initView.h / 2] : [0, 0, 42000, 29700];
    doc.ext = b;
    doc.byId = {}; doc.ents.forEach(function(e){ doc.byId[e.id] = e; }); doc.layouts.forEach(function(l){ l.ents.forEach(function(e){ doc.byId[e.id] = e; }); });
  };
  TC.docExt = function(doc){ var b = [Infinity, Infinity, -Infinity, -Infinity]; doc.ents.forEach(function(e){ if (e.t === 'xline' || e.t === 'ray' || !e._bb) return; if (e._bb[0] < b[0]) b[0] = e._bb[0]; if (e._bb[1] < b[1]) b[1] = e._bb[1]; if (e._bb[2] > b[2]) b[2] = e._bb[2]; if (e._bb[3] > b[3]) b[3] = e._bb[3]; }); if (b[0] < Infinity) doc.ext = b; return doc.ext; };
  TC.layerOf = function(doc, e){ return doc.layerMap[e.layer] || doc.layerMap['0']; };
  TC.layerVisible = function(doc, name){ var l = doc.layerMap[name]; if (!l) return true; var t = doc.tempLayers[name] || {}; var on = t.on != null ? t.on : l.on, frz = t.frz != null ? t.frz : l.frz; return on && !frz; };
  TC.entVisible = function(doc, e){ return TC.layerVisible(doc, e.layer); };

  /* ---------- hình học ---------- */
  var G = TC.geo = {
    dist:function(a, b){ return Math.hypot(a[0] - b[0], a[1] - b[1]); },
    segDist:function(p, a, b){ var dx = b[0] - a[0], dy = b[1] - a[1], l = dx * dx + dy * dy, t = l ? ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / l : 0; t = Math.max(0, Math.min(1, t)); return Math.hypot(p[0] - a[0] - t * dx, p[1] - a[1] - t * dy); },
    foot:function(p, a, b){ var dx = b[0] - a[0], dy = b[1] - a[1], l = dx * dx + dy * dy, t = l ? ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / l : 0; return { t:t, q:[a[0] + t * dx, a[1] + t * dy] }; },
    inArc:function(ang, a0, a1){ var n = function(x){ return ((x % 360) + 360) % 360; }; ang = n(ang); a0 = n(a0); a1 = n(a1); return a0 <= a1 ? ang >= a0 && ang <= a1 : ang >= a0 || ang <= a1; },
    segInt:function(a, b, c, d){ var r = [b[0] - a[0], b[1] - a[1]], s = [d[0] - c[0], d[1] - c[1]], den = r[0] * s[1] - r[1] * s[0]; if (Math.abs(den) < 1e-9) return null; var t = ((c[0] - a[0]) * s[1] - (c[1] - a[1]) * s[0]) / den, u = ((c[0] - a[0]) * r[1] - (c[1] - a[1]) * r[0]) / den; if (t < -1e-9 || t > 1 + 1e-9 || u < -1e-9 || u > 1 + 1e-9) return null; return [a[0] + t * r[0], a[1] + t * r[1]]; },
    polyArea:function(pts){ var a = 0; for (var i = 0, n = pts.length; i < n; i++){ var p = pts[i], q = pts[(i + 1) % n]; a += p[0] * q[1] - q[0] * p[1]; } return Math.abs(a) / 2; },
    polyLen:function(pts, closed){ var L = 0; for (var i = 0; i < pts.length - 1; i++) L += G.dist(pts[i], pts[i + 1]); if (closed) L += G.dist(pts[pts.length - 1], pts[0]); return L; },
    inPoly:function(p, pts){ var c = false; for (var i = 0, j = pts.length - 1; i < pts.length; j = i++){ var a = pts[i], b = pts[j]; if (((a[1] > p[1]) !== (b[1] > p[1])) && (p[0] < (b[0] - a[0]) * (p[1] - a[1]) / (b[1] - a[1]) + a[0])) c = !c; } return c; },
    ang:function(a, b){ return Math.atan2(b[1] - a[1], b[0] - a[0]) * 180 / Math.PI; }
  };

  /* ---------- một vùng vẽ gắn với một bản vẽ ---------- */
  function View(host){
    this.host = host;
    this.base = document.createElement('canvas'); this.base.className = 'dw';
    this.over = document.createElement('canvas'); this.over.className = 'dw';
    host.appendChild(this.base); host.appendChild(this.over);
    this.bx = this.base.getContext('2d'); this.ox = this.over.getContext('2d');
    this.mouse = null; this.hover = null; this.snap = null; this.drag = null; this.temp = [];
    this.textMode = 'fixed';
  }
  TC.View = View;
  View.prototype.setDoc = function(doc){ this.doc = doc; if (!doc.view) this.zoomView(doc.initView || null); this.resize(); };
  View.prototype.resize = function(){
    var r = this.host.getBoundingClientRect(), d = window.devicePixelRatio || 1;
    this.W = r.width; this.H = r.height; this.dpr = d;
    [this.base, this.over].forEach(function(c){ c.width = Math.max(1, Math.round(r.width * d)); c.height = Math.max(1, Math.round(r.height * d)); });
    this.draw();
  };
  View.prototype.space = function(){ return this.doc.space === 'model' ? null : this.doc.layouts[this.doc.space]; };
  View.prototype.vw = function(){ var L = this.space(); return L ? (L.view = L.view || this.fitPaper(L)) : this.doc.view; };
  View.prototype.fitPaper = function(L){ var s = Math.min((this.W - 60) / L.paper.w, (this.H - 60) / L.paper.h); return { cx:L.paper.w / 2, cy:L.paper.h / 2, s:s }; };
  // màn hình ↔ tọa độ của không gian đang xem (Model: mm bản vẽ; Layout: mm giấy)
  View.prototype.toS = function(p, v){ v = v || this.vw(); return [this.W / 2 + (p[0] - v.cx) * v.s, this.H / 2 - (p[1] - v.cy) * v.s]; };
  View.prototype.toW = function(x, y, v){ v = v || this.vw(); return [v.cx + (x - this.W / 2) / v.s, v.cy - (y - this.H / 2) / v.s]; };
  // điểm trong không gian Model: trực tiếp, hoặc qua viewport đang kích hoạt
  View.prototype.vpXf = function(vp){ var L = this.space(), pv = this.vw(); var k = pv.s / vp.scale; var cx = this.W / 2 + (vp.x + vp.w / 2 - pv.cx) * pv.s, cy = this.H / 2 - (vp.y + vp.h / 2 - pv.cy) * pv.s; return { cx:vp.c[0], cy:vp.c[1], s:k, sx:cx, sy:cy }; };
  View.prototype.modelView = function(){
    var L = this.space(); if (!L) return { v:this.doc.view, clip:null };
    var vp = this.doc.activeVp && L.vports.filter(function(q){ return q.id === this.doc.activeVp; }, this)[0]; if (!vp) return null;
    var x = this.vpXf(vp), pv = this.vw();
    return { v:{ cx:x.cx - (x.sx - this.W / 2) / x.s, cy:x.cy + (x.sy - this.H / 2) / x.s, s:x.s }, vp:vp, clip:this.vpRect(vp, pv) };
  };
  View.prototype.vpRect = function(vp, pv){ var a = this.toS([vp.x, vp.y + vp.h], pv), b = this.toS([vp.x + vp.w, vp.y], pv); return [a[0], a[1], b[0] - a[0], b[1] - a[1]]; };
  View.prototype.mToS = function(p){ var mv = this.modelView(); return mv ? this.toS(p, mv.v) : null; };
  View.prototype.sToM = function(x, y){ var mv = this.modelView(); return mv ? this.toW(x, y, mv.v) : null; };

  View.prototype.zoomView = function(z){
    var doc = this.doc, ext = doc.ext, c, h;
    if (z && z.c) { c = z.c; h = z.h; } else { c = [(ext[0] + ext[2]) / 2, (ext[1] + ext[3]) / 2]; h = Math.max(ext[3] - ext[1], (ext[2] - ext[0]) * ((this.H || 700) / (this.W || 1400))) * 1.06; }
    doc.view = { cx:c[0], cy:c[1], s:(this.H || 700) / h };
  };
  View.prototype.pushHist = function(){ var v = this.vw(); var h = this.doc.viewHist; h.push({ sp:this.doc.space, cx:v.cx, cy:v.cy, s:v.s }); if (h.length > 30) h.shift(); };
  View.prototype.zoomAt = function(x, y, f){
    var mv = this.doc.space !== 'model' && this.doc.activeVp ? this.modelView() : null;
    if (mv && !mv.vp.locked){ var vp = mv.vp; var p = this.toW(x, y, mv.v); vp.scale /= f; var nv = this.modelView().v; var q = this.toW(x, y, nv); vp.c = [vp.c[0] + p[0] - q[0], vp.c[1] + p[1] - q[1]]; this.draw(); TC.emit('view'); return; }
    var v = this.vw(), w = this.toW(x, y, v); v.s = TC.clamp(v.s * f, 1e-5, 500);
    var w2 = this.toW(x, y, v); v.cx += w[0] - w2[0]; v.cy += w[1] - w2[1];
    this.draw(); TC.emit('view');
  };
  View.prototype.panBy = function(dx, dy){
    var mv = this.doc.space !== 'model' && this.doc.activeVp ? this.modelView() : null;
    if (mv){ var vp = mv.vp; vp.c = [vp.c[0] - dx / mv.v.s, vp.c[1] + dy / mv.v.s]; this.draw(); return; }
    var v = this.vw(); v.cx -= dx / v.s; v.cy += dy / v.s; this.draw();
  };
  View.prototype.zoomExtents = function(){
    this.pushHist();
    var L = this.space();
    if (L){ L.view = this.fitPaper(L); }
    else { var e = this.doc.ext, pad = 1.06; var s = Math.min(this.W / ((e[2] - e[0]) * pad), this.H / ((e[3] - e[1]) * pad)); this.doc.view = { cx:(e[0] + e[2]) / 2, cy:(e[1] + e[3]) / 2, s:s }; }
    this.draw(); TC.emit('view');
  };
  View.prototype.zoomRect = function(a, b){
    this.pushHist(); var v = this.vw(); var p = this.toW(a[0], a[1]), q = this.toW(b[0], b[1]);
    var w = Math.abs(q[0] - p[0]), h = Math.abs(q[1] - p[1]); if (w < 1e-6 || h < 1e-6) return;
    v.cx = (p[0] + q[0]) / 2; v.cy = (p[1] + q[1]) / 2; v.s = Math.min(this.W / w, this.H / h) * 0.96; this.draw(); TC.emit('view');
  };
  View.prototype.zoomPrev = function(){ var h = this.doc.viewHist; var z = h.pop(); if (!z) return false; this.doc.space = z.sp; var v = this.vw(); v.cx = z.cx; v.cy = z.cy; v.s = z.s; this.draw(); TC.emit('view'); return true; };
  View.prototype.zoomTo = function(bb, keep){
    this.pushHist(); var v = this.doc.view; var w = Math.max(bb[2] - bb[0], 1), h = Math.max(bb[3] - bb[1], 1);
    v.cx = (bb[0] + bb[2]) / 2; v.cy = (bb[1] + bb[3]) / 2; if (!keep) v.s = Math.min(this.W / (w * 1.6), this.H / (h * 1.6), 2); this.draw(); TC.emit('view');
  };

  /* ---------- vẽ ---------- */
  View.prototype.color = function(e, pr, onPaper){
    var c = pr && pr.src && pr.src.color != null ? pr.src.color : e.color != null ? e.color : null;
    if (pr && pr.src && pr.src !== e && (pr.src.layer !== '0' || pr.src.color != null)) { var bl = this.doc.layerMap[pr.src.layer]; if (pr.src.color == null && bl) c = bl.color; }
    if (c == null) c = (this.doc.layerMap[e.layer] || {}).color || 7;
    if (c === 7 || c === 255) return onPaper ? '#000000' : '#FFFFFF';
    if (typeof c === 'string') return c;
    return ACI[c] || '#FFFFFF';
  };
  View.prototype.lw = function(e){ if (!TC.S.toggles.lw) return 1; var w = e.lw != null ? e.lw : (this.doc.layerMap[e.layer] || {}).lw || 0.25; return Math.max(1, Math.round(w * 4 * 10) / 10); };
  View.prototype.textFont = function(pr, px){ var st = this.doc.styleMap[pr.style] || {}; return (pr.ital ? 'italic ' : '') + (st.bold || pr.bold ? '600 ' : '') + px + 'px ' + (st.css || 'Arial, "Helvetica Neue", sans-serif'); };
  View.prototype.textStr = function(pr){
    if (this.textMode !== 'raw') return pr.s;
    var st = this.doc.styleMap[pr.style] || {}; return st.enc === 'TCVN3' ? TC.tcvn3(pr.s, st.upper) : st.enc === 'VNI' ? TC.rawText(this.doc, { s:pr.s, style:pr.style }) : pr.s;
  };
  View.prototype.drawPrims = function(ctx, e, v, onPaper, style){
    var self = this, S = function(p){ return self.toS(p, v); }, s = v.s;
    var col = style && style.color, lw = style && style.lw || this.lw(e);
    var lay = this.doc.layerMap[e.layer] || {}; var lt = e.lt || lay.lt;
    if (e.t === 'vpframe' && !style) return;
    var frameOff = this.doc.frame === 0;
    e._p.forEach(function(pr){ if (pr.fbg && self.print) return; // nền xám của field không in
      if (pr.k === 'ins' || (pr.clipb && frameOff && !style)) return;
      ctx.strokeStyle = ctx.fillStyle = col || self.color(e, pr, onPaper);
      ctx.lineWidth = pr.w && !style ? Math.max(lw, pr.w * s) : lw;
      if (pr.k === 'pt'){ var pq = S(pr.p), pd = self.doc.pd || { mode:0, size:5 }, z = pd.abs ? Math.max(2, pd.size * s / 2) : Math.max(2, self.H * pd.size / 200), md = pd.mode & 31; ctx.beginPath(); if (md === 0 || style) { ctx.arc(pq[0], pq[1], style ? 3 : 1.5, 0, 7); ctx.fill(); } if (md === 2) { ctx.moveTo(pq[0] - z, pq[1]); ctx.lineTo(pq[0] + z, pq[1]); ctx.moveTo(pq[0], pq[1] - z); ctx.lineTo(pq[0], pq[1] + z); } if (md === 3) { ctx.moveTo(pq[0] - z, pq[1] - z); ctx.lineTo(pq[0] + z, pq[1] + z); ctx.moveTo(pq[0] + z, pq[1] - z); ctx.lineTo(pq[0] - z, pq[1] + z); } if (md === 4) { ctx.moveTo(pq[0], pq[1]); ctx.lineTo(pq[0], pq[1] - z); } ctx.stroke(); if (pd.mode & 32){ ctx.beginPath(); ctx.arc(pq[0], pq[1], z, 0, 7); ctx.stroke(); } if (pd.mode & 64) ctx.strokeRect(pq[0] - z, pq[1] - z, 2 * z, 2 * z); return; }
      var dash = !style && LT[lt] && pr.k === 'seg' && (pr.src === e) ? LT[lt].map(function(x){ return x * s; }) : null;
      ctx.setLineDash(dash && dash[0] > 3 ? dash : []);
      if (pr.k === 'seg'){ var a = S(pr.a), b = S(pr.b); ctx.beginPath(); ctx.moveTo(a[0], a[1]); ctx.lineTo(b[0], b[1]); ctx.stroke(); }
      else if (pr.k === 'circ'){ var c = S(pr.c); ctx.beginPath(); ctx.arc(c[0], c[1], Math.max(.5, pr.r * s), 0, Math.PI * 2); ctx.stroke(); }
      else if (pr.k === 'arc'){ var c2 = S(pr.c); ctx.beginPath(); ctx.arc(c2[0], c2[1], Math.max(.5, pr.r * s), -pr.a0 * Math.PI / 180, -pr.a1 * Math.PI / 180, true); ctx.stroke(); }
      else if (pr.k === 'ell'){ var c3 = S(pr.c); ctx.beginPath(); if (pr.a0 != null) ctx.ellipse(c3[0], c3[1], Math.max(.5, pr.rx * s), Math.max(.5, pr.ry * s), -pr.rot * Math.PI / 180, -pr.a0 * Math.PI / 180, -pr.a1 * Math.PI / 180, true); else ctx.ellipse(c3[0], c3[1], Math.max(.5, pr.rx * s), Math.max(.5, pr.ry * s), -pr.rot * Math.PI / 180, 0, Math.PI * 2); ctx.stroke(); }
      else if (pr.k === 'tri'){ var q0 = S(pr.pts[0]), q1 = S(pr.pts[1]), q2 = S(pr.pts[2]); ctx.beginPath(); ctx.moveTo(q0[0], q0[1]); ctx.lineTo(q1[0], q1[1]); ctx.lineTo(q2[0], q2[1]); ctx.closePath(); ctx.fill(); }
      else if (pr.k === 'img'){ if (!pr.show) return; var c0 = S(pr.pts[0]), c1 = S(pr.pts[1]), c2 = S(pr.pts[2]), c3 = S(pr.pts[3]); ctx.save(); ctx.beginPath(); ctx.moveTo(c0[0], c0[1]); ctx.lineTo(c1[0], c1[1]); ctx.lineTo(c2[0], c2[1]); ctx.lineTo(c3[0], c3[1]); ctx.closePath(); ctx.clip(); if (style){ ctx.globalAlpha = .25; ctx.fill(); ctx.restore(); return; } var bx = Math.min(c0[0], c1[0], c2[0], c3[0]), by = Math.min(c0[1], c1[1], c2[1], c3[1]), bw = Math.max(c0[0], c1[0], c2[0], c3[0]) - bx, bh = Math.max(c0[1], c1[1], c2[1], c3[1]) - by, L0 = 30 + pr.bright * .5; var gr = ctx.createLinearGradient(bx, by, bx + bw, by + bh); gr.addColorStop(0, 'hsl(140,18%,' + L0 + '%)'); gr.addColorStop(.55, 'hsl(90,14%,' + (L0 + 8) + '%)'); gr.addColorStop(1, 'hsl(35,20%,' + (L0 - 4) + '%)'); ctx.fillStyle = gr; ctx.globalAlpha = 1 - pr.fade / 100; ctx.fillRect(bx, by, bw, bh); ctx.strokeStyle = 'rgba(255,255,255,.35)'; ctx.lineWidth = 1; for (var k2 = 0; k2 < 6; k2++){ ctx.beginPath(); ctx.moveTo(bx, by + bh * (.2 + k2 * .13)); ctx.bezierCurveTo(bx + bw * .3, by + bh * (.1 + k2 * .12), bx + bw * .6, by + bh * (.35 + k2 * .1), bx + bw, by + bh * (.25 + k2 * .12)); ctx.stroke(); } ctx.restore(); }
      else if (pr.k === 'tick'){ var t = S(pr.p), d = Math.max(2.5, Math.min(6, 90 * s)); ctx.lineWidth = Math.max(1.4, lw); ctx.beginPath(); ctx.moveTo(t[0] - d, t[1] + d); ctx.lineTo(t[0] + d, t[1] - d); ctx.stroke(); }
      else if (pr.k === 'text'){
        var px = pr.h * s * 1.32; var tp = S(pr.p);
        if (px < 2.2){ if (px > .4) { var w = pr.h * s * 0.6 * pr.s.length; ctx.globalAlpha = .55; ctx.fillRect(tp[0] - (pr.al === 'm' || pr.al === 'c' ? w / 2 : 0), tp[1] - px / 2, w, Math.max(.6, px * .6)); ctx.globalAlpha = 1; } return; }
        ctx.save(); ctx.translate(tp[0], tp[1]); if (pr.rot) ctx.rotate(-pr.rot * Math.PI / 180);
        ctx.font = self.textFont(pr, px); ctx.textAlign = pr.al === 'm' || pr.al === 'c' ? 'center' : pr.al === 'r' || pr.al === 'rm' ? 'right' : 'left'; ctx.textBaseline = pr.al === 'm' || pr.al === 'lm' || pr.al === 'rm' ? 'middle' : 'alphabetic';
        ctx.fillText(self.textStr(pr), 0, 0); ctx.restore();
      }
      else if (pr.k === 'hatch'){
        ctx.beginPath(); pr.loops.forEach(function(l){ l.forEach(function(q, k){ var sp = S(q); if (k) ctx.lineTo(sp[0], sp[1]); else ctx.moveTo(sp[0], sp[1]); }); ctx.closePath(); });
        if (pr.pat === 'SOLID' || style){ ctx.globalAlpha = style ? .28 : .9; ctx.fill('evenodd'); ctx.globalAlpha = 1; return; }
        var gap = 3.175 * pr.sc * s;
        if (gap < 2.6){ ctx.globalAlpha = .3; ctx.fill('evenodd'); ctx.globalAlpha = 1; return; }
        ctx.save(); ctx.clip('evenodd'); ctx.lineWidth = 1; ctx.setLineDash([]);
        var bb = bbox([pr]), p0 = S([bb[0], bb[1]]), p1 = S([bb[2], bb[3]]);
        hatchPaint(ctx, pr.pat, gap, pr.ang || 0, [Math.min(p0[0], p1[0]), Math.min(p0[1], p1[1]), Math.max(p0[0], p1[0]), Math.max(p0[1], p1[1])]);
        ctx.restore();
      }
    });
    ctx.setLineDash([]);
  };
  // vẽ mẫu hatch trong không gian màn hình; dùng chung cho vùng vẽ và gallery mẫu
  var PK = { ANSI31:'l45', ANSI32:'steel', ANSI37:'x', 'AR-CONC':'conc', 'AR-SAND':'dots', 'AR-B816':'brick', LINE:'l0', NET:'net', DOTS:'dots', HONEY:'honey', EARTH:'earth', 'TCVN-BETONG':'conc', 'TCVN-BTCT':'btct', 'TCVN-GACH':'l45', 'TCVN-DAT':'earth', 'TCVN-CAT':'dots', 'TCVN-GO':'wood', 'TCVN-KIMLOAI':'steel', 'TCVN-CACHNHIET':'insul', 'TCVN-NUOC':'wave', 'TCVN-DA':'stone' };
  function lines(ctx, b, gap, deg, dash){ var a = -deg * Math.PI / 180, c = Math.cos(a), sn = Math.sin(a), cx = (b[0] + b[2]) / 2, cy = (b[1] + b[3]) / 2, R = Math.hypot(b[2] - b[0], b[3] - b[1]) / 2 + gap; ctx.setLineDash(dash || []); ctx.beginPath(); for (var k = -R; k <= R; k += gap){ var px = cx - sn * k, py = cy + c * k; ctx.moveTo(px - c * R, py - sn * R); ctx.lineTo(px + c * R, py + sn * R); } ctx.stroke(); ctx.setLineDash([]); }
  function hatchPaint(ctx, pat, gap, ang, b){
    var kind = PK[pat] || 'l45', i, j, rnd = function(n){ var x = Math.sin(n * 12.9898) * 43758.5453; return x - Math.floor(x); };
    if (kind === 'l45') lines(ctx, b, gap * Math.SQRT2, 45 + ang);
    else if (kind === 'l0') lines(ctx, b, gap * 1.4, ang);
    else if (kind === 'x'){ lines(ctx, b, gap * 1.8, 45 + ang); lines(ctx, b, gap * 1.8, -45 + ang); }
    else if (kind === 'net'){ lines(ctx, b, gap * 1.8, ang); lines(ctx, b, gap * 1.8, 90 + ang); }
    else if (kind === 'steel'){ lines(ctx, b, gap * 2.4, 45 + ang); ctx.save(); ctx.translate(gap * .45, 0); lines(ctx, b, gap * 2.4, 45 + ang); ctx.restore(); }
    else if (kind === 'wave'){ ctx.beginPath(); for (j = b[1]; j < b[3] + gap; j += gap * 1.6){ ctx.moveTo(b[0], j); for (i = b[0]; i < b[2] + gap; i += gap / 2) ctx.lineTo(i, j + Math.sin(i / gap * 2) * gap * .25); } ctx.stroke(); }
    else if (kind === 'wood'){ ctx.beginPath(); for (j = b[1]; j < b[3] + gap; j += gap * .9){ ctx.moveTo(b[0], j); for (i = b[0]; i < b[2] + gap; i += gap) ctx.lineTo(i, j + Math.sin(i / (gap * 6) + j / (gap * 4)) * gap * .5); } ctx.stroke(); }
    else if (kind === 'brick'){ var bh = gap * 1.6, bw = gap * 3.6, r = 0; ctx.beginPath(); for (j = b[1]; j < b[3] + bh; j += bh, r++){ ctx.moveTo(b[0], j); ctx.lineTo(b[2], j); for (i = b[0] + (r % 2) * bw / 2; i < b[2]; i += bw){ ctx.moveTo(i, j); ctx.lineTo(i, j + bh); } } ctx.stroke(); }
    else if (kind === 'earth'){ var st = gap * 2.4; ctx.beginPath(); for (j = b[1]; j < b[3] + st; j += st) for (i = b[0]; i < b[2] + st; i += st){ var hz = ((i - b[0]) / st + (j - b[1]) / st) % 2 < 1; for (var q = 0; q < 3; q++){ var o2 = q * st / 3 + st / 6; if (hz){ ctx.moveTo(i, j + o2); ctx.lineTo(i + st * .8, j + o2); } else { ctx.moveTo(i + o2, j); ctx.lineTo(i + o2, j + st * .8); } } } ctx.stroke(); }
    else if (kind === 'insul'){ ctx.beginPath(); var hh = Math.max(gap * 2.2, 8); for (j = b[1]; j < b[3] + hh; j += hh){ ctx.moveTo(b[0], j + hh / 2); for (i = b[0]; i < b[2] + hh; i += hh / 2) ctx.lineTo(i, j + ((i - b[0]) / (hh / 2) % 2 < 1 ? 0 : hh)); } ctx.stroke(); }
    else if (kind === 'honey'){ var hx = gap * 2.2; ctx.beginPath(); for (j = b[1], r = 0; j < b[3] + hx; j += hx * .87, r++) for (i = b[0] + (r % 2) * hx * .75; i < b[2] + hx; i += hx * 1.5){ for (var k2 = 0; k2 <= 6; k2++){ var aa = k2 * Math.PI / 3, X = i + hx / 2 * Math.cos(aa), Y = j + hx / 2 * Math.sin(aa); if (k2) ctx.lineTo(X, Y); else ctx.moveTo(X, Y); } } ctx.stroke(); }
    else { // chấm: cát, bê tông, BTCT, đá
      var dg = gap * 1.3, n = 0; for (j = b[1]; j < b[3]; j += dg) for (i = b[0]; i < b[2]; i += dg){ n++; var x = i + rnd(n) * dg, y = j + rnd(n + 7) * dg; ctx.fillRect(x, y, 1.2, 1.2); if ((kind === 'conc' || kind === 'btct') && n % 9 === 0){ ctx.beginPath(); ctx.moveTo(x, y); ctx.lineTo(x + dg * .7, y + dg * .2); ctx.lineTo(x + dg * .25, y + dg * .65); ctx.closePath(); ctx.stroke(); } if (kind === 'stone' && n % 5 === 0){ ctx.beginPath(); ctx.arc(x, y, dg * .35, 0, 7); ctx.stroke(); } }
      if (kind === 'btct') lines(ctx, b, gap * 4, 45 + ang);
    }
  }
  TC.hatchPaint = hatchPaint; TC.HATCH_KIND = PK;
  View.prototype.drawModel = function(ctx, v, onPaper, clip, vp){
    var doc = this.doc, self = this, W = this.W, H = this.H, frz = vp && vp.frz || [];
    var vis = function(e){ if (!TC.entVisible(doc, e) || frz.indexOf(e.layer) >= 0) return false; var a = self.toS([e._bb[0], e._bb[1]], v), b = self.toS([e._bb[2], e._bb[3]], v); var r = clip || [0, 0, W, H]; return !(b[0] < r[0] || a[0] > r[0] + r[2] || a[1] < r[1] || b[1] > r[1] + r[3]); };
    var dr = function(e){ var lk = (doc.layerMap[e.layer] || {}).lck || e.xref; if (lk) ctx.globalAlpha = e.xref ? .6 : .5; self.drawPrims(ctx, e, v, onPaper); if (lk) ctx.globalAlpha = 1; };
    doc.ents.forEach(function(e){ if (e.t === 'hatch' && vis(e)) dr(e); });
    doc.ents.forEach(function(e){ if (e.t !== 'hatch' && vis(e)) dr(e); });
  };
  View.prototype.draw = function(){
    if (!this.doc || !this.W) return;
    var ctx = this.bx, d = this.dpr, W = this.W, H = this.H, self = this;
    ctx.setTransform(d, 0, 0, d, 0, 0); ctx.clearRect(0, 0, W, H);
    var L = this.space();
    if (!L){
      ctx.fillStyle = TC.cssv('--c-canvas'); ctx.fillRect(0, 0, W, H);
      if (TC.S.toggles.grid) this.drawGrid(ctx, this.doc.view);
      if (this.doc.cmp && TC.cmpDraw) TC.cmpDraw(this, ctx); else this.drawModel(ctx, this.doc.view, false, null);
    } else {
      var pv = this.vw();
      ctx.fillStyle = TC.cssv('--c-layout-bg'); ctx.fillRect(0, 0, W, H);
      var a = this.toS([0, L.paper.h], pv), b = this.toS([L.paper.w, 0], pv);
      ctx.fillStyle = TC.cssv('--c-paper-shadow'); ctx.fillRect(a[0] + 6, a[1] + 6, b[0] - a[0], b[1] - a[1]);
      ctx.fillStyle = TC.cssv('--c-paper'); ctx.fillRect(a[0], a[1], b[0] - a[0], b[1] - a[1]);
      var m = L.margin, pa = this.toS([m[0], L.paper.h - m[1]], pv), pb = this.toS([L.paper.w - m[2], m[3]], pv);
      ctx.strokeStyle = TC.cssv('--c-paper-print'); ctx.setLineDash([5, 4]); ctx.lineWidth = 1; ctx.strokeRect(pa[0], pa[1], pb[0] - pa[0], pb[1] - pa[1]); ctx.setLineDash([]);
      L.vports.forEach(function(vp){
        var r = self.vpRect(vp, pv), x = self.vpXf(vp);
        var mv = { cx:x.cx - (x.sx - W / 2) / x.s, cy:x.cy + (x.sy - H / 2) / x.s, s:x.s };
        ctx.save(); ctx.beginPath(); ctx.rect(r[0], r[1], r[2], r[3]); ctx.clip();
        self.drawModel(ctx, mv, true, r, vp); ctx.restore();
        var act = self.doc.activeVp === vp.id;
        ctx.strokeStyle = act ? TC.cssv('--c-accent') : '#8C949E'; ctx.lineWidth = act ? 2.5 : 1; ctx.strokeRect(r[0], r[1], r[2], r[3]);
        if (vp.locked){ var lx = r[0] + r[2] - 16, ly = r[1] + 7; ctx.strokeStyle = ctx.fillStyle = act ? TC.cssv('--c-accent') : '#6A717B'; ctx.lineWidth = 1.4; ctx.beginPath(); ctx.arc(lx + 4, ly + 4, 2.6, Math.PI, 0); ctx.stroke(); ctx.fillRect(lx, ly + 4, 8, 6); }
      });
      L.ents.forEach(function(e){ if (TC.entVisible(self.doc, e)) self.drawPrims(ctx, e, pv, true); });
    }
    this.drawOver();
  };
  View.prototype.drawGrid = function(ctx, v){
    var step = 1000; while (step * v.s < 12) step *= 5; while (step * v.s > 80) step /= 5;
    var p0 = this.toW(0, this.H, v), p1 = this.toW(this.W, 0, v);
    ctx.fillStyle = TC.cssv('--c-canvas-grid-major');
    for (var x = Math.floor(p0[0] / step) * step; x < p1[0]; x += step) for (var y = Math.floor(p0[1] / step) * step; y < p1[1]; y += step){ var s = this.toS([x, y], v); ctx.fillRect(s[0], s[1], 1, 1); }
  };

  /* ---------- lớp trên: tô sáng, con trỏ, bắt điểm, đường tạm ---------- */
  View.prototype.drawOver = function(){
    var ctx = this.ox, d = this.dpr, W = this.W, H = this.H, self = this, doc = this.doc;
    ctx.setTransform(d, 0, 0, d, 0, 0); ctx.clearRect(0, 0, W, H);
    if (!doc) return;
    var paper = !!this.space(), mv = this.modelView();
    var vFor = function(e){ return e._paper ? self.vw() : mv ? mv.v : null; };
    // đã chọn: quầng màu + nét mảnh; rê chuột: nét đậm (07.1 §9.4, khác cả màu lẫn độ đậm)
    var selC = TC.cssv('--c-hl-sel'), hovC = TC.cssv('--c-hl-hover'), resC = TC.cssv('--c-hl-res');
    (doc.sel || []).forEach(function(id){ var e = doc.byId[id]; if (!e) return; var v = vFor(e) || self.vw(); ctx.save(); if (mv && mv.clip && !e._paper) { ctx.beginPath(); ctx.rect.apply(ctx, mv.clip); ctx.clip(); }
      ctx.globalAlpha = .38; self.drawPrims(ctx, e, v, paper, { color:selC, lw:5 }); ctx.globalAlpha = 1; self.drawPrims(ctx, e, v, paper, { color:selC, lw:1.4 }); ctx.restore(); });
    (doc.found || []).forEach(function(id){ var e = doc.byId[id]; if (!e) return; var v = vFor(e) || self.vw(); ctx.globalAlpha = .5; self.drawPrims(ctx, e, v, paper, { color:resC, lw:5 }); ctx.globalAlpha = 1; });
    if (this.hover && doc.sel.indexOf(this.hover) < 0){ var he = doc.byId[this.hover]; if (he) { var hv = vFor(he) || self.vw(); ctx.globalAlpha = .22; this.drawPrims(ctx, he, hv, paper, { color:hovC, lw:5 }); ctx.globalAlpha = 1; this.drawPrims(ctx, he, hv, paper, { lw:2.2 }); } }
    // đồ họa tạm của lệnh
    this.temp.forEach(function(g){ self.drawTemp(ctx, g); });
    // lớp phủ của tính năng: markup, chế độ đếm, đám mây so sánh
    (TC.overlays || []).forEach(function(f){ f(ctx, self); });
    // khung chọn Window (viền liền) / Crossing (viền đứt)
    if (this.drag && this.drag.kind === 'sel'){
      var a = this.drag.a, b = this.drag.b, cross = b[0] < a[0];
      ctx.fillStyle = TC.cssv(cross ? '--c-sel-cf' : '--c-sel-wf'); ctx.strokeStyle = TC.cssv(cross ? '--c-sel-c' : '--c-sel-w');
      ctx.setLineDash(cross ? [6, 4] : []); ctx.lineWidth = 1;
      ctx.fillRect(Math.min(a[0], b[0]), Math.min(a[1], b[1]), Math.abs(b[0] - a[0]), Math.abs(b[1] - a[1]));
      ctx.strokeRect(Math.min(a[0], b[0]) + .5, Math.min(a[1], b[1]) + .5, Math.abs(b[0] - a[0]), Math.abs(b[1] - a[1])); ctx.setLineDash([]);
    }
    if (this.drag && this.drag.kind === 'zwin'){ var za = this.drag.a, zb = this.drag.b; ctx.strokeStyle = TC.cssv('--c-cross'); ctx.setLineDash([4, 3]); ctx.strokeRect(Math.min(za[0], zb[0]) + .5, Math.min(za[1], zb[1]) + .5, Math.abs(zb[0] - za[0]), Math.abs(zb[1] - za[1])); ctx.setLineDash([]); }
    // marker bắt điểm
    if (this.snap) this.drawSnap(ctx, this.snap.s, this.snap.k);
    // con trỏ: chữ thập + ô chọn (chờ), chỉ chữ thập (chọn điểm), chỉ ô chọn (chọn đối tượng)
    var m = this.mouse;
    if (m && this.cursor !== 'pan' && this.cursor !== 'none'){
      var sp = this.snap ? this.snap.s : m, cs = this.cursorSize || 56;
      ctx.strokeStyle = TC.cssv('--c-cross'); ctx.lineWidth = 1; ctx.globalAlpha = .9;
      if (this.cursor !== 'pick'){ ctx.beginPath(); ctx.moveTo(sp[0] - cs, sp[1] + .5); ctx.lineTo(sp[0] + cs, sp[1] + .5); ctx.moveTo(sp[0] + .5, sp[1] - cs); ctx.lineTo(sp[0] + .5, sp[1] + cs); ctx.stroke(); }
      if (this.cursor !== 'point'){ ctx.strokeRect(m[0] - 4.5, m[1] - 4.5, 9, 9); }
      ctx.globalAlpha = 1;
      if (this.badge){ ctx.fillStyle = TC.cssv('--c-dyn-bg'); ctx.strokeStyle = TC.cssv('--c-dyn-b'); ctx.fillRect(m[0] + 10, m[1] - 24, 18, 18); ctx.strokeRect(m[0] + 10.5, m[1] - 23.5, 17, 17); ctx.fillStyle = TC.cssv('--c-dyn-t'); ctx.font = '600 9px ' + TC.cssv('--f-mono'); ctx.fillText(this.badge, m[0] + 12, m[1] - 11); }
    }
  };
  View.prototype.drawSnap = function(ctx, p, k){
    var c = TC.cssv('--c-snap'), r = 6, x = p[0], y = p[1];
    ctx.strokeStyle = c; ctx.lineWidth = 2; ctx.beginPath();
    switch (k){
      case 'end': ctx.rect(x - r, y - r, 2 * r, 2 * r); break;
      case 'mid': ctx.moveTo(x, y - r - 1); ctx.lineTo(x + r + 1, y + r); ctx.lineTo(x - r - 1, y + r); ctx.closePath(); break;
      case 'cen': ctx.arc(x, y, r, 0, 7); break;
      case 'gcen': ctx.arc(x, y, r, 0, 7); ctx.moveTo(x + 1.5, y); ctx.arc(x, y, 1.5, 0, 7); break;
      case 'node': ctx.arc(x, y, r, 0, 7); ctx.moveTo(x - 4, y - 4); ctx.lineTo(x + 4, y + 4); ctx.moveTo(x + 4, y - 4); ctx.lineTo(x - 4, y + 4); break;
      case 'quad': ctx.moveTo(x, y - r - 1); ctx.lineTo(x + r + 1, y); ctx.lineTo(x, y + r + 1); ctx.lineTo(x - r - 1, y); ctx.closePath(); break;
      case 'int': ctx.moveTo(x - r, y - r); ctx.lineTo(x + r, y + r); ctx.moveTo(x + r, y - r); ctx.lineTo(x - r, y + r); break;
      case 'ins': ctx.rect(x - r, y - r, 7, 7); ctx.rect(x - 1, y - 1, 7, 7); break;
      case 'per': ctx.moveTo(x - r, y - r); ctx.lineTo(x - r, y + r); ctx.lineTo(x + r, y + r); ctx.moveTo(x - r, y); ctx.lineTo(x, y); ctx.lineTo(x, y + r); break;
      case 'tan': ctx.arc(x, y + 1, r - 1, 0, 7); ctx.moveTo(x - r - 2, y - r + 1); ctx.lineTo(x + r + 2, y - r + 1); break;
      case 'nea': ctx.moveTo(x - r, y - r); ctx.lineTo(x + r, y - r); ctx.lineTo(x - r, y + r); ctx.lineTo(x + r, y + r); ctx.closePath(); break;
      case 'app': ctx.rect(x - r, y - r, 2 * r, 2 * r); ctx.moveTo(x - 4, y - 4); ctx.lineTo(x + 4, y + 4); ctx.moveTo(x + 4, y - 4); ctx.lineTo(x - 4, y + 4); break;
      case 'par': ctx.moveTo(x - r, y + 2); ctx.lineTo(x + 2, y - r); ctx.moveTo(x - 2, y + r); ctx.lineTo(x + r, y - 2); break;
    }
    ctx.stroke();
  };
  TC.SNAP_NAMES = { end:'Điểm cuối', mid:'Trung điểm', cen:'Tâm', gcen:'Tâm hình học', node:'Điểm', quad:'Góc phần tư', int:'Giao điểm', ext:'Kéo dài', ins:'Điểm chèn', per:'Vuông góc', tan:'Tiếp xúc', nea:'Gần nhất', app:'Giao biểu kiến', par:'Song song' };
  View.prototype.drawTemp = function(ctx, g){
    var self = this, S = function(p){ return g.paper ? self.toS(p) : (self.mToS(p) || self.toS(p)); };
    ctx.setLineDash(g.dash || []); ctx.lineWidth = g.w || 1.2; ctx.strokeStyle = g.color || TC.cssv('--c-rubber'); ctx.fillStyle = g.fill || 'transparent'; ctx.globalAlpha = g.alpha || 1; ctx.lineCap = ctx.lineJoin = g.cap || 'butt';
    if (g.k === 'line'){ var a = S(g.a), b = S(g.b); ctx.beginPath(); ctx.moveTo(a[0], a[1]); ctx.lineTo(b[0], b[1]); ctx.stroke(); }
    else if (g.k === 'poly'){ ctx.beginPath(); g.pts.forEach(function(p, k){ var s = S(p); if (k) ctx.lineTo(s[0], s[1]); else ctx.moveTo(s[0], s[1]); }); if (g.closed) ctx.closePath(); if (g.fill) ctx.fill(); ctx.stroke(); }
    else if (g.k === 'sline'){ ctx.beginPath(); ctx.moveTo(g.a[0], g.a[1]); ctx.lineTo(g.b[0], g.b[1]); ctx.stroke(); }
    else if (g.k === 'dimq'){ // kích thước tạm của Đo nhanh (màn hình)
      ctx.strokeStyle = TC.cssv('--c-track'); ctx.lineWidth = 1; ctx.beginPath(); ctx.moveTo(g.a[0], g.a[1]); ctx.lineTo(g.b[0], g.b[1]); ctx.stroke();
      [g.a, g.b].forEach(function(p){ ctx.beginPath(); ctx.arc(p[0], p[1], 2.5, 0, 7); ctx.fillStyle = TC.cssv('--c-track'); ctx.fill(); });
    }
    else if (g.k === 'ents'){ var mv0 = self.modelView(), v0 = g.paper || !mv0 ? self.vw() : mv0.v; g.list.forEach(function(e){ self.drawPrims(ctx, e, v0, !!self.space(), g.real ? null : { color:g.color || TC.cssv('--c-rubber'), lw:g.w || 1 }); }); }
    else if (g.k === 'xents'){ var mv1 = self.modelView(), v1 = !mv1 ? self.vw() : mv1.v, m = g.m, ox = self.W / 2 - v1.cx * v1.s, oy = self.H / 2 + v1.cy * v1.s, A2 = m[0], B2 = -m[1], C2 = -m[2], D2 = m[3], E2 = v1.s * m[4] + ox - (A2 * ox + C2 * oy), F2 = -v1.s * m[5] + oy - (B2 * ox + D2 * oy), k2 = Math.sqrt(Math.abs(m[0] * m[3] - m[1] * m[2])) || 1;
      ctx.save(); ctx.transform(A2, B2, C2, D2, E2, F2); g.list.forEach(function(e){ self.drawPrims(ctx, e, v1, !!self.space(), { color:g.color || TC.cssv('--c-rubber'), lw:(g.w || 1) / k2 }); }); ctx.restore(); }
    else if (g.k === 'pt'){ var p = S(g.p); ctx.beginPath(); ctx.arc(p[0], p[1], 3, 0, 7); ctx.fillStyle = g.color || TC.cssv('--c-accent'); ctx.fill(); }
    ctx.setLineDash([]); ctx.globalAlpha = 1; ctx.lineCap = ctx.lineJoin = 'butt';
  };

  /* ---------- tìm đối tượng dưới con trỏ ---------- */
  View.prototype.spaceEnts = function(){
    var L = this.space(); if (!L) return this.doc.ents;
    if (this.doc.activeVp) return this.doc.ents;
    L.ents.forEach(function(e){ e._paper = true; }); return L.ents;
  };
  View.prototype.hitTest = function(x, y, tolPx){
    var doc = this.doc, L = this.space(), mv = this.modelView(), paperPick = L && !doc.activeVp;
    var v = paperPick ? this.vw() : (mv ? mv.v : null); if (!v) return null;
    if (mv && mv.clip && !paperPick){ var c = mv.clip; if (x < c[0] || x > c[0] + c[2] || y < c[1] || y > c[1] + c[3]) return null; }
    var p = this.toW(x, y, v), tol = (tolPx || 5) / v.s, best = null, bd = Infinity, hatch = null;
    var list = paperPick ? L.ents : doc.ents;
    for (var i = list.length - 1; i >= 0; i--){
      var e = list[i]; if (!TC.entVisible(doc, e)) continue;
      var b = e._bb; if (p[0] < b[0] - tol || p[0] > b[2] + tol || p[1] < b[1] - tol || p[1] > b[3] + tol) continue;
      for (var k = 0; k < e._p.length; k++){
        var pr = e._p[k], dd = Infinity;
        if (pr.k === 'seg') dd = G.segDist(p, pr.a, pr.b);
        else if (pr.k === 'circ') dd = Math.abs(G.dist(p, pr.c) - pr.r);
        else if (pr.k === 'arc'){ var an = G.ang(pr.c, p); if (G.inArc(an, pr.a0, pr.a1)) dd = Math.abs(G.dist(p, pr.c) - pr.r); }
        else if (pr.k === 'ell'){ var q = [(p[0] - pr.c[0]) / pr.rx, (p[1] - pr.c[1]) / pr.ry]; dd = Math.abs(Math.hypot(q[0], q[1]) - 1) * Math.min(pr.rx, pr.ry); }
        else if (pr.k === 'text'){ var w = pr.h * 0.62 * pr.s.length, x0 = pr.al === 'm' || pr.al === 'c' ? pr.p[0] - w / 2 : pr.al === 'r' ? pr.p[0] - w : pr.p[0], y0 = pr.al === 'm' ? pr.p[1] - pr.h / 2 : pr.p[1];
          if (pr.rot) { var rr = -pr.rot * Math.PI / 180, dx = p[0] - pr.p[0], dy = p[1] - pr.p[1], lx = dx * Math.cos(rr) - dy * Math.sin(rr), ly = dx * Math.sin(rr) + dy * Math.cos(rr); if (lx > -w / 2 - tol && lx < w / 2 + tol && ly > -pr.h - tol && ly < pr.h + tol) dd = 0; }
          else if (p[0] > x0 - tol && p[0] < x0 + w + tol && p[1] > y0 - tol && p[1] < y0 + pr.h * 1.15 + tol) dd = 0; }
        else if (pr.k === 'img'){ if (G.inPoly(p, pr.pts)) dd = tol * .9; }
        else if (pr.k === 'pt') dd = G.dist(p, pr.p);
        else if (pr.k === 'hatch'){ if (!hatch){ var inside = 0; pr.loops.forEach(function(l){ if (G.inPoly(p, l)) inside++; }); if (inside % 2) hatch = e; } continue; }
        if (dd < bd){ bd = dd; best = e; }
      }
    }
    if (best && bd <= tol) return best.id;
    return hatch ? hatch.id : null;
  };
  View.prototype.boxSelect = function(a, b){
    var doc = this.doc, L = this.space(), paperPick = L && !doc.activeVp, mv = this.modelView(), v = paperPick ? this.vw() : mv ? mv.v : null; if (!v) return [];
    var p = this.toW(Math.min(a[0], b[0]), Math.max(a[1], b[1]), v), q = this.toW(Math.max(a[0], b[0]), Math.min(a[1], b[1]), v);
    var cross = b[0] < a[0], r = [p[0], p[1], q[0], q[1]], out = [];
    var inR = function(pt){ return pt[0] >= r[0] && pt[0] <= r[2] && pt[1] >= r[1] && pt[1] <= r[3]; };
    var edges = [[[r[0], r[1]], [r[2], r[1]]], [[r[2], r[1]], [r[2], r[3]]], [[r[2], r[3]], [r[0], r[3]]], [[r[0], r[3]], [r[0], r[1]]]];
    (paperPick ? L.ents : doc.ents).forEach(function(e){
      if (!TC.entVisible(doc, e)) return; var bb = e._bb;
      var inside = bb[0] >= r[0] && bb[2] <= r[2] && bb[1] >= r[1] && bb[3] <= r[3];
      if (inside) { out.push(e.id); return; }
      if (!cross || bb[2] < r[0] || bb[0] > r[2] || bb[3] < r[1] || bb[1] > r[3]) return;
      var hit = e._p.some(function(pr){
        if (pr.k === 'seg') return inR(pr.a) || inR(pr.b) || edges.some(function(ed){ return G.segInt(pr.a, pr.b, ed[0], ed[1]); });
        if (pr.k === 'circ' || pr.k === 'arc' || pr.k === 'ell'){ for (var t = 0; t < 32; t++){ var an = t / 32 * 360; if (pr.k === 'arc' && !G.inArc(an, pr.a0, pr.a1)) continue; var rx = pr.rx || pr.r, ry = pr.ry || pr.r; if (inR([pr.c[0] + rx * Math.cos(an * Math.PI / 180), pr.c[1] + ry * Math.sin(an * Math.PI / 180)])) return true; } return false; }
        if (pr.k === 'text' || pr.k === 'pt') return inR(pr.p);
        if (pr.k === 'hatch') return pr.loops.some(function(l){ return l.some(inR); });
        return false;
      });
      if (hit) out.push(e.id);
    });
    return out;
  };

  /* ---------- bắt điểm (OSNAP) ---------- */
  var PRIO = { end:1, int:2, mid:3, cen:4, quad:5, node:5, ins:6, per:7, gcen:4, tan:8, nea:9 };
  View.prototype.snapAt = function(x, y, base){
    if (!TC.S.toggles.osnap) return [];
    var doc = this.doc, mv = this.modelView(), L = this.space(), paperPick = L && !doc.activeVp;
    var v = paperPick ? this.vw() : mv ? mv.v : null; if (!v) return [];
    var self = this, M = TC.S.osnap, ap = 10, p = this.toW(x, y, v), tol = ap / v.s, cand = [], segs = [];
    var S = function(q){ return self.toS(q, v); };
    function push(k, q){ if (!M[k]) return; var s = S(q), d = Math.hypot(s[0] - x, s[1] - y); if (d <= ap) cand.push({ k:k, p:q, s:s, d:d }); }
    (paperPick ? L.ents : doc.ents).forEach(function(e){
      if (!TC.entVisible(doc, e)) return; var b = e._bb; if (p[0] < b[0] - tol || p[0] > b[2] + tol || p[1] < b[1] - tol || p[1] > b[3] + tol) return;
      e._p.forEach(function(pr){
        if (pr.k === 'pt') { push('node', pr.p); return; }
        if (pr.k === 'seg'){ if (!pr.sm || pr.first) push('end', pr.a); if (!pr.sm || pr.last) push('end', pr.b); if (!pr.sm && !pr.inf) push('mid', [(pr.a[0] + pr.b[0]) / 2, (pr.a[1] + pr.b[1]) / 2]); if (pr.inf) push('mid', e.p);
          if (G.segDist(p, pr.a, pr.b) <= tol) { segs.push(pr); if (base && M.per){ var f = G.foot(base, pr.a, pr.b); if (f.t >= 0 && f.t <= 1) push('per', f.q); } if (M.nea){ var f2 = G.foot(p, pr.a, pr.b); push('nea', f2.q); } } }
        else if (pr.k === 'circ' || pr.k === 'arc'){
          var near = Math.abs(G.dist(p, pr.c) - pr.r) <= tol * 1.5 || G.dist(p, pr.c) <= tol;
          if (near) push('cen', pr.c);
          if (pr.k === 'arc'){ var r = pr.r, a0 = pr.a0 * Math.PI / 180, a1 = pr.a1 * Math.PI / 180; push('end', [pr.c[0] + r * Math.cos(a0), pr.c[1] + r * Math.sin(a0)]); push('end', [pr.c[0] + r * Math.cos(a1), pr.c[1] + r * Math.sin(a1)]); var am = ((pr.a0 + ((pr.a1 - pr.a0 + 360) % 360) / 2)) * Math.PI / 180; push('mid', [pr.c[0] + r * Math.cos(am), pr.c[1] + r * Math.sin(am)]); }
          [0, 90, 180, 270].forEach(function(an){ if (pr.k === 'arc' && !G.inArc(an, pr.a0, pr.a1)) return; push('quad', [pr.c[0] + pr.r * Math.cos(an * Math.PI / 180), pr.c[1] + pr.r * Math.sin(an * Math.PI / 180)]); });
          if (near && M.nea){ var an2 = Math.atan2(p[1] - pr.c[1], p[0] - pr.c[0]); push('nea', [pr.c[0] + pr.r * Math.cos(an2), pr.c[1] + pr.r * Math.sin(an2)]); }
          if (near) segs.push(pr);
        }
        else if (pr.k === 'ell'){ push('cen', pr.c); }
        else if (pr.k === 'ins' || (pr.k === 'text' && pr.src === e)) push('ins', pr.p);
      });
    });
    // giao điểm giữa các đoạn gần con trỏ
    if (M.int) for (var i = 0; i < segs.length; i++) for (var j = i + 1; j < segs.length; j++){ if (segs[i].k === 'seg' && segs[j].k === 'seg'){ var q = G.segInt(segs[i].a, segs[i].b, segs[j].a, segs[j].b); if (q) push('int', q); } }
    cand.sort(function(a, b){ return (a.d - b.d) * 0.6 + ((PRIO[a.k] || 9) - (PRIO[b.k] || 9)) * 1.6; });
    var seen = {}; return cand.filter(function(c){ var key = c.k + Math.round(c.p[0]) + ',' + Math.round(c.p[1]); if (seen[key]) return false; seen[key] = 1; return true; });
  };

  /* ---------- tia đo nhanh: khoảng cách tới hình học gần nhất theo 4 hướng ---------- */
  View.prototype.quickRays = function(x, y){
    var doc = this.doc, mv = this.modelView(); if (!mv) return null; var v = mv.v, p = this.toW(x, y, v), far = 1e7;
    var hits = { l:null, r:null, u:null, d:null };
    var rays = { l:[p, [p[0] - far, p[1]]], r:[p, [p[0] + far, p[1]]], u:[p, [p[0], p[1] + far]], d:[p, [p[0], p[1] - far]] };
    doc.ents.forEach(function(e){
      if (!TC.entVisible(doc, e) || e.t === 'text' || e.t === 'dim' || e.layer === 'KT-TRUC' || e.layer === 'KT-HATCH-TUONG') return;
      e._p.forEach(function(pr){ if (pr.k !== 'seg') return;
        for (var k in rays){ var q = G.segInt(rays[k][0], rays[k][1], pr.a, pr.b); if (q){ var d = G.dist(p, q); if (d > 1e-6 && (!hits[k] || d < hits[k].d)) hits[k] = { q:q, d:d }; } }
      });
    });
    return { p:p, hits:hits, v:v };
  };
  TC.roomAt = function(doc, p){
    var best = null, ba = Infinity;
    doc.ents.forEach(function(e){
      if (!TC.entVisible(doc, e)) return;
      if (e.t === 'hatch' && e.pat !== 'SOLID') e._p[0].loops.slice(1).forEach(function(l){ if (G.inPoly(p, l)){ var a = G.polyArea(l); if (a < ba && a > 2e5){ ba = a; best = l; } } });
      if (e.t === 'pline' && e.closed && G.inPoly(p, e.pts)){ var a2 = G.polyArea(e.pts); if (a2 < ba){ ba = a2; best = e.pts; } }
    });
    return best;
  };
})();
