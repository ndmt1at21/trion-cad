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
  function angMap(a, t){ var m = t.sx < 0 && t.sy < 0 ? 180 + a : t.sx < 0 ? 180 - a : t.sy < 0 ? -a : a; return m + t.rot; }
  function flat(doc, e, t, out, host){
    var P = t ? t.m : function(p){ return p; };
    var o = function(pr){ pr.e = host || e; pr.src = e; out.push(pr); };
    switch (e.t){
      case 'line': o({ k:'seg', a:P(e.p1), b:P(e.p2) }); break;
      case 'pline': { var pts = e.pts.map(P); for (var i = 0; i < pts.length - 1; i++) o({ k:'seg', a:pts[i], b:pts[i + 1], vi:i }); if (e.closed) o({ k:'seg', a:pts[pts.length - 1], b:pts[0], vi:pts.length - 1 }); break; }
      case 'circle': o({ k:'circ', c:P(e.c), r:e.r * (t ? t.k : 1) }); break;
      case 'arc': { var a0 = t ? angMap(e.a0, t) : e.a0, a1 = t ? angMap(e.a1, t) : e.a1; if (t && (t.sx < 0) !== (t.sy < 0)) { var tmp = a0; a0 = a1; a1 = tmp; } o({ k:'arc', c:P(e.c), r:e.r * (t ? t.k : 1), a0:a0, a1:a1 }); break; }
      case 'ellipse': o({ k:'ell', c:P(e.c), rx:e.rx * (t ? Math.abs(t.sx) : 1), ry:e.ry * (t ? t.k : 1), rot:t ? t.rot : 0 }); break;
      case 'text': case 'attdef': {
        if (e.t === 'attdef' && !host) break;
        var s = e.s; if (e.t === 'attdef' && host && host.attrs && host.attrs[e.tag] != null) s = host.attrs[e.tag];
        o({ k:'text', p:P(e.p), h:e.h * (t ? t.k : 1), s:s, rot:(e.rot || 0) + (t ? t.rot : 0), al:e.al || 'l', style:e.style, tag:e.tag }); break; }
      case 'hatch': o({ k:'hatch', loops:e.loops.map(function(l){ return l.map(P); }), pat:e.pat, sc:(e.sc || 1) * (t ? t.k : 1), ang:e.ang || 0 }); break;
      case 'dim': {
        var a = e.p1, b = e.p2, h = e.dir === 'h';
        var d1 = h ? [a[0], e.pos] : [e.pos, a[1]], d2 = h ? [b[0], e.pos] : [e.pos, b[1]], ov = 160 * (h ? (e.pos < a[1] ? -1 : 1) : (e.pos < a[0] ? -1 : 1));
        o({ k:'seg', a:a, b:h ? [a[0], e.pos + ov] : [e.pos + ov, a[1]] }); o({ k:'seg', a:b, b:h ? [b[0], e.pos + ov] : [e.pos + ov, b[1]] });
        o({ k:'seg', a:h ? [d1[0] - 200, d1[1]] : [d1[0], d1[1] - 200], b:h ? [d2[0] + 200, d2[1]] : [d2[0], d2[1] + 200] });
        o({ k:'tick', p:d1 }); o({ k:'tick', p:d2 });
        var len = h ? Math.abs(b[0] - a[0]) : Math.abs(b[1] - a[1]); e._meas = len;
        var tp = h ? [(d1[0] + d2[0]) / 2, e.pos + 210] : [e.pos - 210, (d1[1] + d2[1]) / 2];
        o({ k:'text', p:tp, h:220, s:e.over || String(Math.round(len)), rot:h ? 0 : 90, al:'m', style:'Standard' });
        break; }
      case 'insert': {
        var bd = doc.blocks[e.name]; if (!bd) break; var tt = xf(e);
        bd.ents.forEach(function(be){ flat(doc, be, tt, out, e); });
        o({ k:'ins', p:e.p }); break; }
    }
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
      else if (p.k === 'tick' || p.k === 'ins') add(p.p[0], p.p[1]);
    });
    return b;
  }
  TC.prepDoc = function(doc){
    function prep(list){ list.forEach(function(e){ var out = []; flat(doc, e, null, out, null); e._p = out; e._bb = bbox(out); }); }
    prep(doc.ents); doc.layouts.forEach(function(l){ prep(l.ents); });
    var b = [Infinity, Infinity, -Infinity, -Infinity];
    doc.ents.forEach(function(e){ if (e._bb[0] < b[0]) b[0] = e._bb[0]; if (e._bb[1] < b[1]) b[1] = e._bb[1]; if (e._bb[2] > b[2]) b[2] = e._bb[2]; if (e._bb[3] > b[3]) b[3] = e._bb[3]; });
    doc.ext = b;
    doc.byId = {}; doc.ents.forEach(function(e){ doc.byId[e.id] = e; }); doc.layouts.forEach(function(l){ l.ents.forEach(function(e){ doc.byId[e.id] = e; }); });
  };
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
    return ACI[c] || '#FFFFFF';
  };
  View.prototype.lw = function(e){ if (!TC.S.toggles.lw) return 1; var w = e.lw != null ? e.lw : (this.doc.layerMap[e.layer] || {}).lw || 0.25; return Math.max(1, Math.round(w * 4 * 10) / 10); };
  View.prototype.textFont = function(pr, px){ var st = this.doc.styleMap[pr.style] || {}; return (st.bold ? '600 ' : '') + px + 'px ' + (st.css || 'Arial, "Helvetica Neue", sans-serif'); };
  View.prototype.textStr = function(pr){
    if (this.textMode !== 'raw') return pr.s;
    var st = this.doc.styleMap[pr.style] || {}; return st.enc === 'TCVN3' ? TC.tcvn3(pr.s, st.upper) : st.enc === 'VNI' ? TC.rawText(this.doc, { s:pr.s, style:pr.style }) : pr.s;
  };
  View.prototype.drawPrims = function(ctx, e, v, onPaper, style){
    var self = this, S = function(p){ return self.toS(p, v); }, s = v.s;
    var col = style && style.color, lw = style && style.lw || this.lw(e);
    var lay = this.doc.layerMap[e.layer] || {}; var lt = e.lt || lay.lt;
    e._p.forEach(function(pr){
      if (pr.k === 'ins') return;
      ctx.strokeStyle = ctx.fillStyle = col || self.color(e, pr, onPaper);
      ctx.lineWidth = lw;
      var dash = !style && LT[lt] && pr.k === 'seg' && (pr.src === e) ? LT[lt].map(function(x){ return x * s; }) : null;
      ctx.setLineDash(dash && dash[0] > 3 ? dash : []);
      if (pr.k === 'seg'){ var a = S(pr.a), b = S(pr.b); ctx.beginPath(); ctx.moveTo(a[0], a[1]); ctx.lineTo(b[0], b[1]); ctx.stroke(); }
      else if (pr.k === 'circ'){ var c = S(pr.c); ctx.beginPath(); ctx.arc(c[0], c[1], Math.max(.5, pr.r * s), 0, Math.PI * 2); ctx.stroke(); }
      else if (pr.k === 'arc'){ var c2 = S(pr.c); ctx.beginPath(); ctx.arc(c2[0], c2[1], Math.max(.5, pr.r * s), -pr.a0 * Math.PI / 180, -pr.a1 * Math.PI / 180, true); ctx.stroke(); }
      else if (pr.k === 'ell'){ var c3 = S(pr.c); ctx.beginPath(); ctx.ellipse(c3[0], c3[1], Math.max(.5, pr.rx * s), Math.max(.5, pr.ry * s), -pr.rot * Math.PI / 180, 0, Math.PI * 2); ctx.stroke(); }
      else if (pr.k === 'tick'){ var t = S(pr.p), d = Math.max(2.5, Math.min(6, 90 * s)); ctx.lineWidth = Math.max(1.4, lw); ctx.beginPath(); ctx.moveTo(t[0] - d, t[1] + d); ctx.lineTo(t[0] + d, t[1] - d); ctx.stroke(); }
      else if (pr.k === 'text'){
        var px = pr.h * s * 1.32; var tp = S(pr.p);
        if (px < 2.2){ if (px > .4) { var w = pr.h * s * 0.6 * pr.s.length; ctx.globalAlpha = .55; ctx.fillRect(tp[0] - (pr.al === 'm' || pr.al === 'c' ? w / 2 : 0), tp[1] - px / 2, w, Math.max(.6, px * .6)); ctx.globalAlpha = 1; } return; }
        ctx.save(); ctx.translate(tp[0], tp[1]); if (pr.rot) ctx.rotate(-pr.rot * Math.PI / 180);
        ctx.font = self.textFont(pr, px); ctx.textAlign = pr.al === 'm' || pr.al === 'c' ? 'center' : pr.al === 'r' ? 'right' : 'left'; ctx.textBaseline = pr.al === 'm' ? 'middle' : 'alphabetic';
        ctx.fillText(self.textStr(pr), 0, 0); ctx.restore();
      }
      else if (pr.k === 'hatch'){
        ctx.beginPath(); pr.loops.forEach(function(l){ l.forEach(function(q, k){ var sp = S(q); if (k) ctx.lineTo(sp[0], sp[1]); else ctx.moveTo(sp[0], sp[1]); }); ctx.closePath(); });
        if (pr.pat === 'SOLID' || style){ ctx.globalAlpha = style ? .28 : .9; ctx.fill('evenodd'); ctx.globalAlpha = 1; return; }
        var gap = 3.175 * pr.sc * s;
        if (gap < 2.6){ ctx.globalAlpha = .3; ctx.fill('evenodd'); ctx.globalAlpha = 1; return; }
        ctx.save(); ctx.clip('evenodd'); ctx.lineWidth = 1; ctx.setLineDash([]);
        var bb = bbox([pr]), p0 = S([bb[0], bb[1]]), p1 = S([bb[2], bb[3]]);
        var x0 = Math.min(p0[0], p1[0]), x1 = Math.max(p0[0], p1[0]), y0 = Math.min(p0[1], p1[1]), y1 = Math.max(p0[1], p1[1]);
        var step = gap * Math.SQRT2; ctx.beginPath();
        for (var k = x0 - (y1 - y0); k < x1; k += step){ ctx.moveTo(k, y1); ctx.lineTo(k + (y1 - y0), y0); }
        ctx.stroke(); ctx.restore();
      }
    });
    ctx.setLineDash([]);
  };
  View.prototype.drawModel = function(ctx, v, onPaper, clip, vp){
    var doc = this.doc, self = this, W = this.W, H = this.H, frz = vp && vp.frz || [];
    var vis = function(e){ if (!TC.entVisible(doc, e) || frz.indexOf(e.layer) >= 0) return false; var a = self.toS([e._bb[0], e._bb[1]], v), b = self.toS([e._bb[2], e._bb[3]], v); var r = clip || [0, 0, W, H]; return !(b[0] < r[0] || a[0] > r[0] + r[2] || a[1] < r[1] || b[1] > r[1] + r[3]); };
    doc.ents.forEach(function(e){ if (e.t === 'hatch' && vis(e)) self.drawPrims(ctx, e, v, onPaper); });
    doc.ents.forEach(function(e){ if (e.t !== 'hatch' && vis(e)) self.drawPrims(ctx, e, v, onPaper); });
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
        if (pr.k === 'text') return inR(pr.p);
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
        if (pr.k === 'seg'){ push('end', pr.a); push('end', pr.b); push('mid', [(pr.a[0] + pr.b[0]) / 2, (pr.a[1] + pr.b[1]) / 2]);
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
