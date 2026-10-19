/* TrionCAD prototype · bộ điều khiển vùng vẽ: chờ, chọn đối tượng, chờ điểm (có bắt điểm), pan, zoom cửa sổ, đo nhanh. */
(function(){
  'use strict';
  var TC = window.TC, $ = TC.$, esc = TC.esc, G = TC.geo;
  var M = { kind:'idle' }, cyc = 0, labels = [];
  var cad = TC.cad = {};
  // móc cho bản biên tập (editor.js): grip, nhập động, nhấp đúp theo loại đối tượng
  cad.hook = {};
  function endHook(){ if (cad.hook.end) cad.hook.end(); }
  function V(){ return TC.app.view; } function D(){ return TC.app.doc(); }
  cad.mode = function(){ return M.kind; };

  function clearLabels(){ labels.forEach(function(n){ n.remove(); }); labels = []; var c = $('.mcard'); if (c && !c._keep) c.remove(); }
  cad.clearLabels = clearLabels;
  function label(html, sx, sy, cls, keep){ var n = TC.el('<div class="mlabel' + (cls ? ' ' + cls : '') + '">' + html + '</div>'); $('#cv').appendChild(n); n.style.left = sx + 'px'; n.style.top = sy + 'px'; if (!keep) labels.push(n); return n; }
  cad.label = label;
  // nhãn gắn với điểm thế giới: đặt lại khi pan/zoom
  var pins = [];
  cad.pin = function(html, wp, cls){ var n = label(html, 0, 0, cls, true); pins.push({ n:n, p:wp }); place(); return n; };
  function place(){ pins = pins.filter(function(q){ return document.contains(q.n); }); pins.forEach(function(q){ var s = V().mToS(q.p); if (s) { q.n.style.left = s[0] + 'px'; q.n.style.top = s[1] + 'px'; q.n.hidden = false; } else q.n.hidden = true; }); var c = $('.mcard'); if (c && c._wp) { var s2 = V().mToS(c._wp); if (s2) { c.style.left = (s2[0] + 14) + 'px'; c.style.top = (s2[1] + 14) + 'px'; } } }
  TC.on('view', place); TC.on('viewchg', place);
  cad.card = function(html, wp){
    var old = $('.mcard'); if (old) old.remove();
    var c = TC.el('<div class="mcard" role="status">' + html + '</div>'); c._wp = wp; c._keep = true; $('#cv').appendChild(c); place();
    var r = $('#cv').getBoundingClientRect(); if (c.offsetLeft + c.offsetWidth > r.width - 8) c.style.left = (r.width - c.offsetWidth - 8) + 'px'; if (c.offsetTop + c.offsetHeight > r.height - 8) c.style.top = (r.height - c.offsetHeight - 8) + 'px';
    return c;
  };
  cad.reset = function(){ V().temp = []; clearLabels(); pins.forEach(function(q){ q.n.remove(); }); pins = []; var c = $('.mcard'); if (c) c.remove(); var t = $('.snap-tip'); if (t) t.remove(); V().drawOver(); };

  function snapTip(){ var old = $('.snap-tip'); if (old) old.remove(); var v = V(); if (!v.snap && M.trk && v.mouse){ var tk = TC.el('<div class="snap-tip">' + M.trk + '</div>'); $('#cv').appendChild(tk); tk.style.left = (v.mouse[0] + 14) + 'px'; tk.style.top = (v.mouse[1] + 18) + 'px'; return; } if (!v.snap) return; var n = TC.el('<div class="snap-tip">' + TC.SNAP_NAMES[v.snap.k] + (v.snapList && v.snapList.length > 1 ? ' · Tab ' + (cyc % v.snapList.length + 1) + '/' + v.snapList.length : '') + '</div>'); $('#cv').appendChild(n); n.style.left = (v.snap.s[0] + 12) + 'px'; n.style.top = (v.snap.s[1] - 28) + 'px'; }
  function worldAt(m){ var v = V(); return v.sToM(m[0], m[1]) || v.toW(m[0], m[1]); }

  cad.move = function(m, e){
    var v = V(), d = D(); if (!d) return;
    if (M.kind === 'idle' && !v.drag && cad.hook.grip && cad.hook.grip(m)){ v.hover = null; v.cursor = 'idle'; TC.app.updateCoord(worldAt(m)); v.drawOver(); return; }
    if (M.kind === 'idle' || M.kind === 'select'){
      v.cursor = M.kind === 'select' ? 'pick' : 'idle';
      var h = v.drag || d.cmp ? null : v.hitTest(m[0], m[1]);
      v.hover = h; v.snap = null; TC.app.updateCoord(worldAt(m));
      v.drawOver(); return;
    }
    if (M.kind === 'point' && M.pick){ v.cursor = 'pick'; v.snap = null; M.cur = worldAt(m); v.hover = v.hitTest(m[0], m[1]); v.temp = (M.temp || []).slice(); if (M.onMove) M.onMove(M.cur, v.temp, v.hover); TC.app.updateCoord(M.cur); if (cad.hook.move) cad.hook.move(m, M.cur, null, true); v.drawOver(); return; }
    if (M.kind === 'point'){
      v.cursor = 'point'; v.hover = null;
      var list = M.nosnap ? [] : v.snapAt(m[0], m[1], M.base); v.snapList = list;
      if (!list.length) cyc = 0;
      v.snap = list.length ? list[cyc % list.length] : null;
      var p = v.snap ? v.snap.p : worldAt(m), trk = [];
      M.trk = null;
      if (TC.S.toggles.ortho && M.base){ var dx = p[0] - M.base[0], dy = p[1] - M.base[1]; p = Math.abs(dx) > Math.abs(dy) ? [p[0], M.base[1]] : [M.base[0], p[1]]; }
      else if (!v.snap && !TC.isViewer()) p = track(v, m, p, trk);
      if (v.snap && TC.S.toggles.otrack && !TC.isViewer()) acquire(v.snap.p);
      M.cur = p; TC.app.updateCoord(p);
      v.temp = (M.temp || []).slice().concat(trk);
      if (M.base && M.rubber !== false) v.temp.push({ k:'line', a:M.base, b:p });
      if (M.onMove) M.onMove(p, v.temp);
      if (cad.hook.move) cad.hook.move(m, p, M.base);
      snapTip(); v.drawOver(); return;
    }
    if (M.kind === 'pan'){ v.cursor = 'pan'; $('#cv').classList.add('cur-pan'); v.drawOver(); return; }
    if (M.kind === 'zwin'){ v.cursor = 'point'; v.drawOver(); return; }
    if (M.kind === 'quick'){ quickMove(m, e); return; }
    if (M.kind === 'stroke'){
      v.cursor = 'point'; v.hover = null; TC.app.updateCoord(worldAt(m));
      if (M.pts){ var ls = M.spts[M.spts.length - 1]; if (Math.hypot(m[0] - ls[0], m[1] - ls[1]) > 2){ M.pts.push(worldAt(m)); M.spts.push(m); } v.temp = [{ k:'poly', pts:M.pts.slice(), color:M.o.color, w:M.o.w, alpha:M.o.alpha, cap:'round' }]; }
      v.drawOver();
    }
  };
  // Polar: hút theo bội số góc tăng; theo dõi bắt điểm: gióng ngang/dọc qua điểm đã lấy (dừng trên điểm bắt 0,4 s)
  var acqT = 0, acqP = null;
  function acquire(q){ var k = Math.round(q[0]) + ',' + Math.round(q[1]); if (acqP === k) return; acqP = k; clearTimeout(acqT); acqT = setTimeout(function(){ if (M.kind !== 'point') return; M.acq = (M.acq || []).filter(function(a){ return Math.round(a[0]) + ',' + Math.round(a[1]) !== k; }).concat([q]).slice(-2); if (V().mouse) cad.move(V().mouse, {}); }, 400); }
  function track(v, m, p, out){
    var mv = v.modelView(), vs = (mv ? mv.v : v.vw()).s, col = TC.cssv('--c-track'), far = 3000 / vs;
    var plus = function(q){ var z = 4 / vs; out.push({ k:'line', a:[q[0] - z, q[1]], b:[q[0] + z, q[1]], color:col }, { k:'line', a:[q[0], q[1] - z], b:[q[0], q[1] + z], color:col }); };
    (M.acq || []).forEach(plus);
    if (TC.S.toggles.otrack) for (var i = 0; i < (M.acq || []).length; i++){ var A = M.acq[i], sA = v.toS(A, mv ? mv.v : v.vw());
      if (Math.abs(sA[1] - m[1]) < 6){ var q = [p[0], A[1]], dd = q[0] - A[0]; out.push({ k:'line', a:A, b:[q[0] + Math.sign(dd || 1) * far, q[1]], dash:[2, 3], color:col }); M.trk = 'Kéo dài: ' + TC.d(Math.abs(dd)) + ' < ' + (dd < 0 ? 180 : 0) + '°'; return q; }
      if (Math.abs(sA[0] - m[0]) < 6){ var q2 = [A[0], p[1]], d2 = q2[1] - A[1]; out.push({ k:'line', a:A, b:[q2[0], q2[1] + Math.sign(d2 || 1) * far], dash:[2, 3], color:col }); M.trk = 'Kéo dài: ' + TC.d(Math.abs(d2)) + ' < ' + (d2 < 0 ? 270 : 90) + '°'; return q2; } }
    if (M.base && TC.S.toggles.polar){ var B = M.base, inc = TC.S.polarInc || 90, dx = p[0] - B[0], dy = p[1] - B[1], an = Math.round(Math.atan2(dy, dx) * 180 / Math.PI / inc) * inc, r = an * Math.PI / 180, ux = Math.cos(r), uy = Math.sin(r), t = dx * ux + dy * uy;
      if (t > 0 && Math.abs(-dx * uy + dy * ux) * vs < 8){ out.push({ k:'line', a:B, b:[B[0] + ux * (t + far), B[1] + uy * (t + far)], dash:[2, 3], color:col }); M.trk = 'Polar: ' + TC.d(t) + ' < ' + ((an % 360 + 360) % 360) + '°'; return [B[0] + ux * t, B[1] + uy * t]; } }
    return p;
  }
  cad.leave = function(){ var t = $('.snap-tip'); if (t) t.remove(); if (M.kind === 'quick') { V().temp = []; clearLabels(); } };
  cad.cycleSnap = function(){ var v = V(); if (M.kind !== 'point' || !v.snapList || v.snapList.length < 2) return false; cyc++; cad.move(v.mouse, {}); return true; };

  cad.down = function(m, e){
    var v = V();
    if (M.kind === 'idle' && cad.hook.gripDown && cad.hook.gripDown(m, e)) return true;
    if (M.kind === 'point'){ var p = M.cur || worldAt(m); var r = M.resolve; M = { kind:'idle' }; v.snap = null; var t = $('.snap-tip'); if (t) t.remove(); endHook(); r({ p:p, s:m, shift:e.shiftKey }); return true; }
    if (M.kind === 'pan'){ v.drag = { kind:'pan', last:m }; $('#cv').classList.add('cur-panning'); return true; }
    if (M.kind === 'zwin'){ v.drag = { kind:'zwin', a:m, b:m }; return true; }
    if (M.kind === 'quick'){ quickClick(m, e); return true; }
    if (M.kind === 'stroke'){ M.pts = [worldAt(m)]; M.spts = [m]; return true; }
    return false;
  };
  // nhả chuột: kết thúc một nét kéo
  cad.up = function(){ if (M.kind !== 'stroke' || !M.pts) return false; var r = M.resolve, pts = M.pts; M = { kind:'idle' }; V().temp = []; V().cursor = 'idle'; V().drawOver(); r(pts.length > 1 ? pts : null); return true; };
  cad.zwinDone = function(a, b){ var r = M.resolve; M = { kind:'idle' }; V().drawOver(); if (Math.hypot(b[0] - a[0], b[1] - a[1]) > 4) V().zoomRect(a, b); if (r) r(true); };

  // bấm chọn: mặc định cộng dồn vùng chọn, Shift + bấm để bớt (07.1 §9.3)
  cad.click = function(p, e){
    var v = V(), d = D(); if (d.cmp){ if (TC.cmpClick) TC.cmpClick(p); return true; }
    if (TC.markupHit && TC.markupHit(p)) return true;
    var h = v.hitTest(p[0], p[1]); if (!h) return false;
    if (e.shiftKey) d.sel = d.sel.filter(function(x){ return x !== h; }); else if (d.sel.indexOf(h) < 0) d.sel.push(h);
    if (M.kind === 'select' && M.onPick) M.onPick(h, e.shiftKey);
    afterSel(); return true;
  };
  cad.boxDone = function(a, b, e){
    var v = V(), d = D(); v.drag = null;
    if (d.cmp || Math.hypot(b[0] - a[0], b[1] - a[1]) < 3) { v.drawOver(); return; }
    var mv0 = v.modelView(), vv = mv0 && !(v.space() && !d.activeVp) ? mv0.v : v.vw(); cad.lastBox = { p:v.toW(a[0], a[1], vv), q:v.toW(b[0], b[1], vv), cross:b[0] < a[0] };
    var ids = v.boxSelect(a, b);
    if (e.shiftKey) d.sel = d.sel.filter(function(x){ return ids.indexOf(x) < 0; }); else ids.forEach(function(id){ if (d.sel.indexOf(id) < 0) d.sel.push(id); });
    if (M.kind === 'select' && M.onPick) M.onPick(null, e.shiftKey, ids);
    afterSel();
  };
  function afterSel(){
    var d = D(); V().drawOver(); TC.emit('sel');
    if (M.kind === 'idle' && d.sel.length) TC.cmdline.status('Đã chọn ' + TC.n(d.sel.length) + ' đối tượng');
  }
  cad.afterSel = afterSel;
  cad.selectAll = function(){ var d = D(); if (!d) return; d.sel = V().spaceEnts().filter(function(e){ return TC.entVisible(d, e); }).map(function(e){ return e.id; }); afterSel(); TC.cmdline.msg('Đã chọn ' + TC.n(d.sel.length) + ' đối tượng'); };
  cad.clearSel = function(){ var d = D(); if (!d) return; d.sel = []; d.found = []; V().drawOver(); TC.emit('sel'); };

  cad.dbl = function(m, e){
    var v = V(), d = D(); if (!d || d.cmp || M.kind !== 'idle') return;
    if (d.space !== 'model'){
      var L = d.layouts[d.space], pw = v.toW(m[0], m[1]);
      var vp = L.vports.filter(function(q){ return pw[0] >= q.x && pw[0] <= q.x + q.w && pw[1] >= q.y && pw[1] <= q.y + q.h; })[0];
      d.activeVp = vp ? vp.id : null; d.sel = []; v.draw(); TC.app.renderStatus(); TC.emit('sel');
      TC.cmdline.msg(vp ? 'Đã vào Model trong viewport (tỉ lệ 1:' + vp.scale + (vp.locked ? ', đã khóa' : '') + '). Nhấp đúp ra ngoài viewport để về Paper.' : 'Đã về Paper.');
      return;
    }
    var h = v.hitTest(m[0], m[1]);
    if (h && cad.hook.dbl && cad.hook.dbl(d.byId[h], m)) return;
    if (h){ if (d.sel.indexOf(h) < 0) d.sel = [h]; afterSel(); TC.app.showPalette('props'); if (TC.isViewer()) TC.cmdline.msg('Thuộc tính ở chế độ chỉ đọc. Bấm vào một giá trị để sao chép.'); }
  };

  cad.context = function(m, e){
    var d = D(); if (!d) return;
    if (cad.hook.ctx && cad.hook.ctx(m, e)) return;
    if (M.kind === 'point' || M.kind === 'select' || TC.cmdline.busy()){
      var items = [{ label:'Enter', kbd:'Enter', act:function(){ TC.cmdline.enter(); } }, { label:'Hủy', kbd:'Esc', act:function(){ TC.cmd.cancel(); } }, { sep:1 }];
      (TC.cmdline.options() || []).forEach(function(o){ items.push({ label:o.label, act:function(){ TC.cmdline.submit(o.key); } }); });
      if (TC.cmdline.options().length) items.push({ sep:1 });
      if (M.kind === 'point') items.push({ label:'Ghi đè bắt điểm', sub:snapOverride() });
      items.push({ label:'Pan', icon:'pan', act:function(){ TC.cmd.run("'PAN"); } }, { label:'Zoom', icon:'zoom', act:function(){ TC.cmd.run("'ZOOM"); } });
      TC.menu(items, e.clientX, e.clientY); return;
    }
    var last = TC.cmd.last(), sel = d.sel.length, viewer = TC.isViewer(), first = sel ? d.byId[d.sel[0]] : null;
    var it = [];
    if (last) it.push({ label:'Lặp lệnh ' + last, kbd:'Enter', act:function(){ TC.cmd.run(last); } });
    if (!viewer && d.undo && d.undo.length) it.push({ label:'Hoàn tác ' + esc(d.undo[d.undo.length - 1].label), icon:'undo', kbd:'Ctrl+Z', act:function(){ TC.cmd.run('U'); } });
    if (!viewer && d.redo && d.redo.length) it.push({ label:'Làm lại', icon:'redo', kbd:'Ctrl+Y', act:function(){ TC.cmd.run('REDO'); } });
    it.push({ label:'Lệnh gần đây', sub:TC.cmd.recent().map(function(c){ return { label:c, act:function(){ TC.cmd.run(c); } }; }).concat(TC.cmd.recent().length ? [] : [{ label:'Chưa có lệnh', dis:true }]) });
    if (!viewer) it.push({ label:'Clipboard', sub:[{ label:'Cắt', kbd:'Ctrl+X', act:function(){ TC.cmd.run('CUTCLIP'); } }, { label:'Sao chép', kbd:'Ctrl+C', act:function(){ TC.cmd.run('COPYCLIP'); } }, { label:'Sao chép có điểm gốc', kbd:'Ctrl+Shift+C', act:function(){ TC.cmd.run('COPYBASE'); } }, { label:'Dán', kbd:'Ctrl+V', act:function(){ TC.cmd.run('PASTECLIP'); } }, { label:'Dán dạng block', kbd:'Ctrl+Shift+V', act:function(){ TC.cmd.run('PASTEBLOCK'); } }, { label:'Dán về tọa độ gốc', act:function(){ TC.cmd.run('PASTEORIG'); } }] });
    it.push({ label:'Cô lập', sub:[{ label:'Cô lập đối tượng', dis:!sel, act:function(){ TC.layers.isolateSel(); } }, { label:'Ẩn đối tượng', dis:!sel, act:function(){ TC.layers.hideSel(); } }, { label:'Kết thúc cô lập', act:function(){ TC.layers.endIso(); } }] });
    it.push({ sep:1 });
    if (sel){
      if (!viewer) [['Xóa','ERASE','erase','Del'],['Di chuyển','MOVE','move'],['Sao chép','COPY','copy'],['Tỉ lệ','SCALE','scale'],['Xoay','ROTATE','rotate']].forEach(function(x){ it.push({ label:x[0], icon:x[2], kbd:x[3], need:'PRO', cmd:x[1], act:function(){ TC.cmd.run(x[1]); } }); });
      if (!viewer) it.push({ label:'Thứ tự hiển thị', sub:[['Lên trên cùng','F'],['Xuống dưới cùng','B']].map(function(o){ return { label:o[0], act:function(){ TC.cmd.run('DRAWORDER', o[1]); } }; }) }, { label:'Tạo đối tượng cùng loại', need:'PRO', act:function(){ TC.cmd.run('ADDSELECTED'); } });
      it.push({ label:'Chọn tương tự', need:'PRO', act:function(){ TC.cmd.run('SELECTSIMILAR'); } }, { label:'Chọn nhanh…', need:'PRO', act:function(){ TC.cmd.run('QSELECT'); } }, { label:'Bỏ chọn tất cả', act:function(){ cad.clearSel(); } });
      if (!viewer && first && (first.t === 'text' || first.t === 'mtext')) it.push({ label:'Sửa chữ', act:function(){ cad.hook.dbl && cad.hook.dbl(first); } });
      if (!viewer && first && first.t === 'insert') it.push({ label:'Sửa attribute…', act:function(){ TC.cmd.run('EATTEDIT'); } }, { label:'Chọn các block cùng tên', act:function(){ d.sel = d.ents.filter(function(x){ return x.t === 'insert' && x.name === first.name; }).map(function(x){ return x.id; }); afterSel(); } });
      if (!viewer && first && first.t === 'hatch') it.push({ label:'Sửa hatch', act:function(){ cad.hook.dbl && cad.hook.dbl(first); } });
      if (first && first.t === 'insert') it.push({ label:'Đếm block “' + esc(first.name) + '”', need:'VP', act:function(){ TC.cmd.run('COUNT'); } });
      it.push({ label:'Liệt kê thông tin', icon:'list', act:function(){ TC.cmd.run('LIST'); } }, { label:'Thuộc tính', icon:'props', kbd:'Ctrl+1', act:function(){ TC.app.showPalette('props'); } });
      if (first && first.t === 'text' && TC.fontDoctor) it.push({ sep:1 }, { label:'Bác sĩ Font: kiểu chữ “' + esc(first.style) + '”', icon:'fontdoc', act:function(){ TC.cmd.run('FIXFONT'); } });
    } else {
      it.push({ label:'Pan', icon:'pan', act:function(){ TC.cmd.run('PAN'); } }, { label:'Zoom', icon:'zoom', sub:[{ label:'Toàn bộ đối tượng', icon:'zext', act:function(){ V().zoomExtents(); } }, { label:'Cửa sổ', icon:'zwin', act:function(){ TC.cmd.run('ZOOM', 'W'); } }, { label:'Trước đó', icon:'zprev', act:function(){ V().zoomPrev(); } }] });
      it.push({ label:'Đo', icon:'measure', sub:[{ label:'Đo nhanh', act:function(){ TC.cmd.run('MEASUREGEOM'); } }, { label:'Khoảng cách', act:function(){ TC.cmd.run('DIST'); } }, { label:'Diện tích', act:function(){ TC.cmd.run('AREA'); } }, { label:'Tọa độ điểm', act:function(){ TC.cmd.run('ID'); } }] });
      it.push({ label:'Chọn tất cả', kbd:'Ctrl+A', act:function(){ cad.selectAll(); } }, { label:'Tìm…', icon:'findtext', act:function(){ TC.cmd.run('FIND'); } });
      it.push({ sep:1 }, { label:'Tùy chọn…', icon:'gear', act:function(){ TC.cmd.run('OPTIONS'); } });
    }
    TC.menu(it, e.clientX, e.clientY);
  };
  function snapOverride(){ return [{ label:'Điểm theo dõi tạm', need:'PRO' }, { label:'Từ (From)', need:'PRO' }, { label:'Trung điểm giữa 2 điểm', need:'PRO' }, { sep:1 }].concat(['end','mid','cen','quad','int','ins','per','nea'].map(function(k){ return { label:TC.SNAP_NAMES[k], act:function(){ var keep = TC.S.osnap; var once = {}; for (var x in keep) once[x] = x === k; TC.S._osnapSave = keep; TC.S.osnap = once; TC.cmdline.msg('Bắt điểm một lần: ' + TC.SNAP_NAMES[k]); } }; })).concat([{ sep:1 }, { label:'Thiết lập bắt điểm…', need:'PRO' }]); }

  /* ---------- các lệnh dùng: chờ điểm, chờ chọn ---------- */
  cad.getPoint = function(o){
    return new Promise(function(res){
      M = { kind:'point', resolve:function(r){ if (TC.S._osnapSave){ TC.S.osnap = TC.S._osnapSave; delete TC.S._osnapSave; } res(r); }, base:o.base, temp:o.temp || [], onMove:o.onMove, rubber:o.rubber, pick:o.pick, nosnap:o.nosnap };
      var v = V(); v.cursor = 'point'; v.hover = null; if (v.mouse) cad.move(v.mouse, {}); else v.drawOver();
    });
  };
  cad.cancelPoint = function(){ endHook(); if (M.resolve) { var r = M.resolve; M = { kind:'idle' }; r(null); } else M = { kind:'idle' }; var v = V(); v.snap = null; v.temp = []; v.cursor = 'idle'; var t = $('.snap-tip'); if (t) t.remove(); $('#cv').classList.remove('cur-pan', 'cur-panning'); v.drawOver(); };
  cad.feedPoint = function(p){ if (M.kind === 'point'){ var r = M.resolve; M = { kind:'idle' }; endHook(); r({ p:p }); return true; } return false; };
  cad.isPick = function(){ return M.kind === 'point' && !!M.pick; };
  cad.curBase = function(){ return M.base; };
  cad.curPoint = function(){ return M.cur || null; };
  cad.getSel = function(o){
    return new Promise(function(res){ M = { kind:'select', resolve:res, onPick:o && o.onPick }; V().cursor = 'pick'; V().drawOver(); });
  };
  cad.endSel = function(){ var r = M.resolve; M = { kind:'idle' }; V().cursor = 'idle'; if (r) r(D().sel.slice()); };
  cad.stroke = function(o){ return new Promise(function(res){ M = { kind:'stroke', resolve:res, pts:null, o:o || {} }; V().cursor = 'point'; V().drawOver(); }); };
  cad.pan = function(){ return new Promise(function(res){ M = { kind:'pan', resolve:res }; V().cursor = 'pan'; $('#cv').classList.add('cur-pan'); V().drawOver(); }); };
  cad.zwin = function(){ return new Promise(function(res){ M = { kind:'zwin', resolve:res }; V().cursor = 'point'; }); };
  cad.endMode = function(){ endHook(); var r = M.resolve; M = { kind:'idle' }; $('#cv').classList.remove('cur-pan', 'cur-panning'); V().cursor = 'idle'; V().temp = []; V().drawOver(); if (r) r(null); };

  /* ---------- Đo nhanh UI-I01: kích thước tạm quanh con trỏ; bấm trong vùng kín để có diện tích ---------- */
  var qsum = null;
  cad.quick = function(){ return new Promise(function(res){ M = { kind:'quick', resolve:res }; qsum = null; V().cursor = 'point'; }); };
  function quickMove(m){
    var v = V(); v.cursor = 'point'; v.hover = null; var r = v.quickRays(m[0], m[1]);
    labels.forEach(function(n){ n.remove(); }); labels = [];
    v.temp = (qsum ? qsum.temp : []).slice(); if (!r) { v.drawOver(); return; }
    TC.app.updateCoord(r.p);
    var S = function(p){ return v.toS(p, r.v); };
    [['l', 'r'], ['d', 'u']].forEach(function(pair){
      var a = r.hits[pair[0]], b = r.hits[pair[1]]; if (!a || !b) return;
      var sa = S(a.q), sb = S(b.q), len = G.dist(a.q, b.q); if (Math.hypot(sb[0] - sa[0], sb[1] - sa[1]) < 18) return;
      v.temp.push({ k:'dimq', a:sa, b:sb });
      label(TC.d(len), (sa[0] + sb[0]) / 2 + (pair[0] === 'd' ? 28 : 0), (sa[1] + sb[1]) / 2 - (pair[0] === 'l' ? 12 : 0));
    });
    if (qsum) label('Tổng: ' + TC.d(qsum.a / 1e6, 2) + ' m² · ' + qsum.n + ' vùng · Shift+bấm để cộng thêm', m[0] + 70, m[1] + 30, 'area');
    v.drawOver();
  }
  function quickClick(m, e){
    var v = V(), d = D(), mv = v.modelView(); if (!mv) return; var p = v.toW(m[0], m[1], mv.v);
    var poly = TC.roomAt(d, p);
    if (!poly){ TC.ctip($('#cv'), m[0], m[1], 'Không có vùng kín tại đây'); return; }
    var a = G.polyArea(poly), per = G.polyLen(poly, true);
    if (!e.shiftKey || !qsum) qsum = { a:0, per:0, n:0, temp:[] };
    qsum.a += a; qsum.per += per; qsum.n++;
    qsum.temp.push({ k:'poly', pts:poly, closed:true, fill:TC.cssv('--c-sel-wf'), color:TC.cssv('--c-sel-w'), w:1.5 });
    var c = poly.reduce(function(s, q){ return [s[0] + q[0] / poly.length, s[1] + q[1] / poly.length]; }, [0, 0]);
    cad.pin('Diện tích ' + TC.d(a / 1e6) + ' m² · Chu vi ' + TC.d(per / 1000) + ' m', c, 'area');
    TC.cmdline.msg('Diện tích = ' + TC.d(a / 1e6) + ' m², Chu vi = ' + TC.d(per / 1000) + ' m' + (qsum.n > 1 ? ' · Tổng ' + qsum.n + ' vùng = ' + TC.d(qsum.a / 1e6) + ' m²' : ''));
    quickMove(m);
  }
  cad.quickEnd = function(){ qsum = null; cad.endMode(); clearLabels(); pins.forEach(function(q){ q.n.remove(); }); pins = []; };
})();
