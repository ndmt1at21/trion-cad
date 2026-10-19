/* TrionCAD prototype · Đợt 5 (sau v1.0): Sheet Set Manager (H08), quản lý máy in và PC3 (H10), thống kê thép (J14). */
(function(){
  'use strict';
  var TC = window.TC, $ = TC.$, $$ = TC.$$, esc = TC.esc, ic = TC.ic, G = TC.geo, IMPL = TC.cmd.impl, ED = TC.ed, SC = TC.screens, H = SC.h;
  function A(){ return TC.app; } function V(){ return TC.app.view; } function D(){ return TC.app.doc(); }
  var OPEN = SC.open5 = SC.open5 || {}; // mã màn hình → hàm mở (danh mục gọi khi bấm); tiện ích mở ở SC.cat (D1, PRO, MOD, RUN, plan, pick, flash)
  var rp = TC.rp;
  function clone(o){ return JSON.parse(JSON.stringify(o)); }
  // số trên giao diện kiểu Việt với số chữ số thập phân cố định
  function fx(v, p){ return Number(v).toLocaleString('vi-VN', { minimumFractionDigits:p || 0, maximumFractionDigits:p || 0 }); }
  function nf(o){ return '<span class="nfw">' + TC.numField(o) + '</span>'; }
  function fmsg(el, err){ if (!el) return; el.className = 'fmsg' + (err ? ' err' : ''); el.innerHTML = err ? ic('warn', 'xs') + esc(err) : ''; }
  function steps(list, k){ return list.map(function(s, i){ return '<li><button class="wz-s' + (i === k ? ' cur' : i < k ? ' done' : '') + '" type="button" disabled><span class="wz-n">' + (i < k ? ic('check', 's12') : i + 1) + '</span>' + s + '</button></li>'; }).join(''); }
  function flyIn(){ $$('.dock.flyout .palhost').forEach(function(h){ h.hidden = true; }); }

  /* ================= Sheet Set Manager UI-H08 (SHEETSET) =================
     Không làm palette thứ hai: palette Bộ bản vẽ (TCSHEETS, screens11.js) thêm tab View của tờ, View Model;
     thuộc tính bộ và tờ tùy biến, tờ mới từ mẫu, publish, lưu trữ, mở và ghi .dst. */
  var SS = SC.sheetSet, PJ = SC.project, X = SC.ssmApi, LBL = 'TCVN-TEN-VIEW', CO = 'TCVN-CHI-TIET', SCS = [20, 25, 50, 75, 100, 200];
  Object.assign(SS, { tab:'list', vq:'', mq:'', vsel:null, msel:null, sc:50, desc:'Hồ sơ thiết kế bản vẽ thi công, nhà ở 2 tầng lô B7.', dir:'D:\\DuAn\\NhaO-2T', res:['D:\\DuAn\\NhaO-2T\\KienTruc', 'D:\\DuAn\\NhaO-2T\\KetCau'], tpl:'TCVN-A3-KT.dwt', askTpl:true, fname:'{SoHieu} {TenBanVe}', pso:'TCVN-in-an.dwt', dstToo:true,
    custom:[{ k:'SOHD', n:'Số hợp đồng', own:'p', v:'HĐ 118/2026/AP' }, { k:'MADA', n:'Mã dự án', own:'p', v:'AP-NO2T' }, { k:'LANPH', n:'Lần phát hành', own:'s', v:'A' }, { k:'NGUOIVE', n:'Người vẽ', own:'s', v:'' }] });
  // thuộc tính tùy biến vào danh sách nguồn của ánh xạ khung tên (p.* của bộ, t.* của tờ)
  function syncSrc(){ var S = X.SRC; for (var i = S.length - 1; i >= 0; i--) if (S[i][2]) S.splice(i, 1); SS.custom.forEach(function(c){ S.push([(c.own === 's' ? 't.' : 'p.') + c.k, (c.own === 's' ? 'Tờ · ' : 'Dự án · ') + c.n, 1]); if (c.own === 'p' && PJ[c.k] == null) PJ[c.k] = c.v; }); }
  syncSrc();
  var TPL = [['TCVN-A3-KT.dwt', 'A3-KT', 'A3', 100, 'KT', 'Kiến trúc · A3 ngang · khung tên TCVN 5571'], ['TCVN-A1-KT.dwt', 'A1-KT', 'A1', 50, 'KT', 'Kiến trúc · A1 ngang'], ['TCVN-A3-KC.dwt', 'A3-KC', 'A3', 100, 'KC', 'Kết cấu · A3 ngang'], ['TCVN-A1-KC.dwt', 'A1-KC', 'A1', 50, 'KC', 'Kết cấu · A1 ngang'], ['Cong-ty-A3.dwt', 'A3-AnPhu', 'A3', 100, 'KT', 'Mẫu công ty An Phú · A3 ngang']];
  var BMDIR = { KT:'KienTruc', KC:'KetCau', 'ĐN':'DienNuoc', CN:'CapThoatNuoc', ME:'CoDien' };
  // view lưu trong tệp bộ bản vẽ (tờ của tệp chưa mở) và view có tên của bản vẽ nguồn chưa mở
  var XV = { 'Chi-tiet-cau-thang.dwg:CT-CAU-THANG':[['1', 'Mặt bằng cầu thang', '1:25'], ['2', 'Mặt cắt A-A', '1:25']], 'Chi-tiet-cau-thang.dwg:CT-LAN-CAN':[['1', 'Chi tiết lan can', '1:10'], ['2', 'Chi tiết tay vịn', '1:5']], 'Ket-cau-mong.dwg:CT-MONG':[['1', 'Móng M1', '1:25'], ['2', 'Móng M2', '1:25'], ['3', 'Giằng móng GM1', '1:25']] };
  var XR = [['D:\\DuAn\\NhaO-2T\\KienTruc', 'Chi-tiet-cau-thang.dwg', [['Cầu thang tầng 1', 4200, 3600], ['Cầu thang tầng 2', 4200, 3600], ['Mặt cắt A-A', 5200, 7400]]], ['D:\\DuAn\\NhaO-2T\\KetCau', 'Ket-cau-mong.dwg', [['Mặt bằng móng', 15800, 12400], ['Móng M1', 2400, 1800], ['Móng M2', 2600, 2000]]]];

  /* ---------- view của tờ: viewport của layout, view có tên trong khung (Model), view lưu trong tệp bộ ---------- */
  function inBB(p, b){ return !!b && p[0] >= b[0] && p[0] <= b[2] && p[1] >= b[1] && p[1] <= b[3]; }
  function vRect(v){ var w = v.w || v.h * 1.35; return [v.c[0] - w / 2, v.c[1] - v.h / 2, v.c[0] + w / 2, v.c[1] + v.h / 2]; }
  function vpRect(vp){ var hw = vp.w * vp.scale / 2, hh = vp.h * vp.scale / 2; return [vp.c[0] - hw, vp.c[1] - hh, vp.c[0] + hw, vp.c[1] + hh]; }
  function iou(a, b){ var ix = Math.max(0, Math.min(a[2], b[2]) - Math.max(a[0], b[0])), iy = Math.max(0, Math.min(a[3], b[3]) - Math.max(a[1], b[1])), i = ix * iy, ar = function(r){ return (r[2] - r[0]) * (r[3] - r[1]); }; return i / (ar(a) + ar(b) - i || 1); }
  // view có tên mà viewport đang hiện: theo tên đã gắn khi đặt view, không có thì theo vùng trùng nhiều nhất
  function viewFor(d, vp){ if (vp.view) return d.views.filter(function(v){ return v.name === vp.view; })[0] || null; var r = vpRect(vp), best = null, b = .3; d.views.forEach(function(v){ var k = iou(r, vRect(v)); if (k > b){ b = k; best = v; } }); return best; }
  function vid(d, v){ return v.uid || (v.uid = 'nv' + (++d.seq)); }
  function cap(s){ s = String(s).toLowerCase(); return s.charAt(0).toUpperCase() + s.slice(1); }
  function viewsOf(s){
    if (!s.d) return (XV[s.key] || [['1', cap(s.title), s.scale]]).map(function(x){ return { id:x[0], key:s.key + '#' + x[0], s:s, no:x[0], name:x[1], scale:x[2], off:true }; });
    if (s.sp === 'model') return s.e ? s.d.views.filter(function(v){ return inBB(v.c, s.e._bb); }).map(function(v, i){ var id = vid(s.d, v); return { id:id, key:s.key + '#' + id, s:s, v:v, no:v.no || String(i + 1), name:v.name, scale:s.scale, list:s.d.ents }; }) : [];
    var L = s.d.layouts[s.sp]; return L ? L.vports.map(function(vp, i){ var nv = viewFor(s.d, vp); return { id:vp.id, key:s.key + '#' + vp.id, s:s, vp:vp, L:L, no:vp.no || String(i + 1), name:vp.name || (nv && nv.name) || 'Viewport ' + (i + 1), scale:'1:' + vp.scale, list:L.ents }; }) : [];
  }
  // nhãn view và callout có liên kết tới view (ssRef: tờ, view; co = callout) trong các bản vẽ đang mở của bộ
  function refsAll(){ var out = []; SS.docs.forEach(function(id){ var d = X.docBy(id); if (!d) return; var f = function(list, sp){ list.forEach(function(e){ if (e.t === 'insert' && e.ssRef) out.push({ e:e, d:d, sp:sp, co:!!e.ssRef.co }); }); }; f(d.ents, 'model'); d.layouts.forEach(function(L, i){ f(L.ents, i); }); }); return out; }
  function want(w, co){ var o = { SO:String(w.no), TO:w.s.no }; if (!co){ o.TEN = String(w.name).toUpperCase(); o.TILE = 'TL ' + w.scale; } return o; }
  function scan(L){
    var VS = L.map(function(s){ return [s, viewsOf(s)]; }), VK = {}, R = refsAll();
    VS.forEach(function(z){ z[1].forEach(function(w){ VK[w.key] = w; w.co = []; w.lbl = null; }); });
    R.forEach(function(r){ var w = VK[r.e.ssRef.k + '#' + r.e.ssRef.v], a = r.e.attrs || {}, wt = w && want(w, r.co);
      r.i = { w:w, want:wt, lost:!w, stale:!!wt && Object.keys(wt).some(function(k){ return String(a[k] || '') !== wt[k]; }), txt:(a.SO || '?') + '/' + (a.TO || '?'), host:L.filter(function(s){ return s.d === r.d && (r.sp === 'model' ? s.sp === 'model' && s.e && inBB(r.e.p, s.e._bb) : s.sp === r.sp); })[0] || null };
      if (w){ if (r.co) w.co.push(r); else w.lbl = r; } });
    return { VS:VS, VK:VK, R:R };
  }
  function ensureLabel(d){
    if (d.blocks[LBL]) return; var list = [], a = function(o){ o.id = ++d.seq; o.layer = '0'; list.push(o); };
    a({ t:'circle', c:[6, 0], r:6 }); a({ t:'line', p1:[0, 0], p2:[12, 0] }); a({ t:'line', p1:[12, 0], p2:[100, 0], lw:.5 });
    a({ t:'attdef', tag:'SO', prompt:'Số view', p:[6, 2.8], h:3, s:'1', style:'Standard', al:'m' }); a({ t:'attdef', tag:'TO', prompt:'Số hiệu tờ', p:[6, -3], h:2.2, s:'KT 01', style:'Standard', al:'m' });
    a({ t:'attdef', tag:'TEN', prompt:'Tên view', p:[15, 1.4], h:3.5, s:'TÊN VIEW', style:'Standard', al:'l' }); a({ t:'attdef', tag:'TILE', prompt:'Tỉ lệ', p:[15, -4.6], h:2.5, s:'TL 1:100', style:'Standard', al:'l' });
    d.blocks[LBL] = { name:LBL, desc:'Nhãn view của bộ bản vẽ: số view / số hiệu tờ, tên view, tỉ lệ', ents:list };
  }
  function layerOf(d){ return d.layerMap['KT-KYHIEU'] ? 'KT-KYHIEU' : '0'; }
  function labelEnt(d, w, p, k){ ensureLabel(d); return { t:'insert', name:LBL, p:p, sx:k, sy:k, rot:0, layer:layerOf(d), attrs:want(w, false), ssRef:{ k:w.s.key, v:w.id } }; }
  function calloutEnt(d, w, p, k){ SC.ensureBlock(d, CO); return { t:'insert', name:CO, p:p, sx:k, sy:k, rot:0, layer:layerOf(d), attrs:want(w, true), ssRef:{ k:w.s.key, v:w.id, co:1 } }; }
  function annoK(d){ return d.space === 'model' || d.activeVp ? (Number(String(d.annoScale || '1:100').split(':')[1]) || 100) : 1; }
  function sheetAt(d, sp){ return X.sheets().filter(function(s){ return s.d === d && s.sp === sp; })[0] || null; }
  function vpGeo(m, sc, p){ var w = m.w / sc, h = m.h / sc; return { x:p[0] - w / 2, y:p[1] - h / 2, w:w, h:h }; }
  function nextVpNo(L){ var n = 0; L.vports.forEach(function(vp, i){ n = Math.max(n, Number(vp.no || i + 1) || 0); }); return String(n + 1); }
  // đặt view Model lên tờ: viewport khóa hiển thị, gắn tên và số view, kèm nhãn view; một bước hoàn tác
  function placeView(d, sp, m, sc, p){
    var L = d.layouts[sp], s = sheetAt(d, sp), g = vpGeo(m, sc, p), id = 'vp' + (++d.seq), no = nextVpNo(L), vp = { id:id, x:g.x, y:g.y, w:g.w, h:g.h, c:m.v.c.slice(), scale:sc, locked:true, frz:['KT-KHUNG'], name:m.name, no:no, view:m.name }, all = L.vports.concat([vp]);
    ED.run('Đặt view lên tờ', function(){
      var done = ED.state(function(){ return L.vports.map(function(q){ return q.id; }); }, function(ids){ L.vports = all.filter(function(q){ return ids.indexOf(q.id) >= 0; }); V().draw(); });
      L.vports.push(vp); done(); ED.add({ t:'vpframe', vp:id, layer:d.layerMap['KT-VIEWPORT'] ? 'KT-VIEWPORT' : '0' }, L.ents);
      if (s) ED.add(labelEnt(d, { id:id, no:no, name:m.name, scale:'1:' + sc, s:s }, [g.x, g.y - 9], 1), L.ents);
    });
    V().draw(); X.refresh(); return { vp:vp, s:s };
  }
  function goView(w){
    if (!w) return; X.nav(w.s); if (w.off || !w.s.d) return; var d = w.s.d;
    if (w.vp){ var f = w.L.ents.filter(function(e){ return e.t === 'vpframe' && e.vp === w.vp.id; })[0]; if (f){ d.sel = [f.id]; TC.cad.afterSel(); } }
    else if (w.v) V().zoomTo(vRect(w.v));
  }
  // đổi số, tên view: nhãn view cùng tờ cập nhật trong cùng bước hoàn tác; callout ở tờ khác báo lệch để cập nhật
  function renameView(w, no, name){
    var o = w.vp || w.v; if (!o || !no || !name || (String(w.no) === no && w.name === name)){ X.refresh(); return; }
    X.withDoc(w.s.d.id, function(){ ED.run('Đổi số, tên view', function(){
      var done = ED.state(function(){ return { no:o.no || null, name:o.name || null }; }, function(s){ o.no = s.no; o.name = s.name; X.refresh(); });
      o.no = no; o.name = name; done();
      if (w.lbl) ED.mod(w.lbl.e, function(x){ x.attrs = Object.assign({}, x.attrs, { SO:no, TEN:name.toUpperCase() }); });
    }); });
    V().draw(); X.refresh();
  }
  function updRefs(R){
    var by = {}, n = 0; R.forEach(function(r){ if (r.i.stale) (by[r.d.id] = by[r.d.id] || []).push(r); });
    Object.keys(by).forEach(function(id){ X.withDoc(id, function(){ ED.run('Cập nhật nhãn, callout', function(){ by[id].forEach(function(r){ ED.mod(r.e, function(x){ x.attrs = Object.assign({}, x.attrs, r.i.want); }); n++; }); }); }); });
    V().draw(); X.refresh();
    TC.toast(n ? 'Đã cập nhật ' + n + ' nhãn, callout theo số hiệu tờ và tên view hiện tại (Ctrl+Z ở từng bản vẽ).' : 'Nhãn và callout đã khớp với bộ bản vẽ.', { log:false });
  }

  /* ---------- tab View của tờ ---------- */
  function svTab(L){
    var Z = scan(L), F = TC.fold(SS.vq), cur = Z.VK[SS.vsel] || null, rows = '', vis = [], bad = Z.R.filter(function(r){ return r.i.stale || r.i.lost; });
    Z.VS.forEach(function(z){ var s = z[0], vs = z[1].filter(function(w){ return !F || TC.fold(s.no + ' ' + s.title + ' ' + w.name).indexOf(F) >= 0; }); if (!vs.length) return;
      rows += '<div class="ssv-sh" title="' + esc(s.file + ' › ' + s.where) + '"><b class="mono">' + esc(s.no) + '</b><span class="t">' + esc(s.title) + '</span><span class="faint">' + esc(s.sp === 'model' ? 'Model' : s.where) + (s.d ? '' : ' · tệp chưa mở') + '</span></div>' +
        vs.map(function(w){ vis.push(w); var n = w.co.length, nb = w.co.filter(function(r){ return r.i.stale; }).length, on = w === cur;
          return '<div class="ssv-row' + (on ? ' is-sel' : '') + '" role="option" aria-selected="' + on + '" tabindex="-1" data-vk="' + esc(w.key) + '"><span class="no mono">' + esc(w.no) + '</span><span class="t" title="' + esc(w.name) + '">' + esc(w.name) + '</span><span class="mono faint">' + esc(w.scale) + '</span><span>' +
            (w.off ? '<span class="faint">Lưu trong tệp</span>' : w.lbl ? '<span class="tag ' + (w.lbl.i.stale ? 'warn' : 'ok') + '" title="' + (w.lbl.i.stale ? 'Nhãn chưa khớp số hiệu tờ hoặc tên view' : 'Nhãn view khớp') + '">' + ic(w.lbl.i.stale ? 'warn' : 'check', 's12') + 'Nhãn</span>' : '<span class="faint">Chưa có nhãn</span>') + '</span>' +
            '<span class="ssv-co' + (nb ? ' warn' : '') + '" title="' + esc(n ? n + ' callout chỉ tới view này' + (nb ? ', ' + nb + ' chưa khớp' : '') : 'Chưa có callout chỉ tới view này') + '">' + (n ? ic(nb ? 'warn' : 'detailsym', 's12') + n : '<span class="faint">—</span>') + '</span></div>'; }).join(''); });
    var first = cur && vis.indexOf(cur) >= 0 ? cur : vis[0];
    rows = rows.replace('data-vk="' + esc(first ? first.key : '') + '"', 'data-vk="' + esc(first ? first.key : '') + '" data-first'); // ô nhận Tab đầu tiên trong danh sách
    var body = '<div class="ssm-h"><span class="gh">View của tờ</span><label class="search grow">' + ic('search') + '<input data-vq placeholder="Tìm số hiệu tờ, tên view" value="' + esc(SS.vq) + '" aria-label="Tìm view"></label></div>' +
      '<div class="ssm-upd"><span class="' + (bad.length ? 'warn' : 'ok') + '">' + ic(bad.length ? 'warn' : 'okc', 's12') + '</span><span class="grow">' + (bad.length ? bad.length + ' nhãn, callout chưa khớp số hiệu tờ hoặc tên view' : 'Nhãn view và callout khớp với bộ bản vẽ') + '</span><button class="btn sm' + (bad.length ? ' primary' : '') + '" type="button" data-sv="upd"' + (bad.length ? '' : ' disabled') + '>' + ic('refresh') + 'Cập nhật nhãn, callout</button></div>' +
      '<div class="ssv-list" role="listbox" aria-label="View của tờ"><div class="ssv-row ssv-hd" aria-hidden="true"><span>Số</span><span>Tên view</span><span>Tỉ lệ</span><span>Nhãn view</span><span>Callout</span></div>' +
      (rows || '<div class="empty">' + (F ? 'Không có view khớp “' + esc(SS.vq) + '”. Tìm không cần dấu: “mat bang” khớp “Mặt bằng”.' : '<b>Các tờ chưa có view</b>Kéo một view ở tab View Model thả vào tờ để tạo viewport có tên và nhãn.') + '</div>') + '</div>';
    var det = '<div class="ssm-det">' + (cur ? '<div class="hrow" style="flex-wrap:nowrap"><input class="inp sm mono" data-vno value="' + esc(cur.no) + '" aria-label="Số view" style="width:44px"' + (cur.off ? ' disabled' : '') + ' spellcheck="false"><input class="inp sm grow" data-vname value="' + esc(cur.name) + '" aria-label="Tên view" spellcheck="false"' + (cur.off ? ' disabled' : '') + '><span class="mono faint">' + esc(cur.scale) + '</span></div>' +
        '<div class="faint ell">' + ic(cur.vp ? 'layout' : cur.v ? 'model' : 'file', 's12') + ' ' + esc(cur.s.no + ' · ' + cur.s.file + ' › ' + cur.s.where) + (cur.vp ? ' · viewport ' + Math.round(cur.vp.w) + ' × ' + Math.round(cur.vp.h) + ' mm' : cur.off ? ' · tệp chưa mở' : ' · view trong khung') + '</div>' +
        '<div class="ssv-cl">' + (cur.co.length ? cur.co.slice(0, 3).map(function(r){ return '<span class="' + (r.i.stale ? 'warn' : '') + '">' + ic(r.i.stale ? 'warn' : 'detailsym', 's12') + '<span class="ell">Callout <b class="mono">' + esc(r.i.txt) + '</b> ở ' + esc(r.i.host ? r.i.host.no + ' ' + cap(r.i.host.title) : r.d.name) + (r.i.stale ? ' · cần ghi ' + esc(r.i.want.SO + '/' + r.i.want.TO) : '') + '</span></span>'; }).join('') + (cur.co.length > 3 ? '<span class="faint">và ' + (cur.co.length - 3) + ' callout khác</span>' : '')
          : '<span class="faint">Chưa có callout chỉ tới view này. Mở mặt bằng có chi tiết rồi bấm Đặt callout.</span>') + '</div>' +
        '<div class="hrow"><button class="btn sm" type="button" data-sv="go">' + ic(cur.off ? 'open' : 'zwin') + (cur.off ? 'Mở tệp' : 'Đi tới view') + '</button>' + (cur.off ? '' : '<button class="btn sm ghost" type="button" data-sv="lbl">' + ic('textnote') + (cur.lbl ? 'Đặt lại nhãn' : 'Đặt nhãn view') + '</button>') + '<button class="btn sm ghost" type="button" data-sv="co" data-tip="Đặt callout chỉ tới view" data-desc="Đặt ký hiệu chi tiết trong bản vẽ đang mở, ghi số view và số hiệu tờ của view này.">' + ic('detailsym') + 'Đặt callout</button></div>'
      : '<span class="faint">Chọn một view để đổi số, tên; đặt nhãn view hoặc callout chỉ tới view.</span>') + '</div>';
    return { body:body, det:det, wire:function(host){
      var root = $('.ssm', host), all = $$('[data-vk]', root);
      if (SS.vkb){ var f = $('.ssv-row.is-sel', root); if (f) f.focus(); SS.vkb = 0; }
      var fi = $('[data-first]', root); if (fi) fi.tabIndex = 0;
      root.addEventListener('click', function(e){ var r = e.target.closest('[data-vk]'); if (r){ SS.vsel = r.dataset.vk; SS.vkb = 1; X.refresh(); return; }
        var b = e.target.closest('[data-sv]'); if (!b || b.disabled) return; var k = b.dataset.sv;
        if (k === 'upd') updRefs(Z.R);
        else if (k === 'go') goView(cur);
        else if (k === 'lbl' && cur){ goView(cur); flyIn(); TC.cmd.run('SHEETSET', { label:cur }); }
        else if (k === 'co' && cur){ flyIn(); TC.cmd.run('SHEETSET', { callout:cur }); } });
      root.addEventListener('dblclick', function(e){ var r = e.target.closest('[data-vk]'); if (r) goView(Z.VK[r.dataset.vk]); });
      root.addEventListener('keydown', function(e){ var r = e.target.closest && e.target.closest('[data-vk]'); if (!r) return; var i = all.indexOf(r);
        if (e.key === 'ArrowDown' || e.key === 'ArrowUp'){ e.preventDefault(); var n = all[TC.clamp(i + (e.key === 'ArrowDown' ? 1 : -1), 0, all.length - 1)]; SS.vsel = n.dataset.vk; SS.vkb = 1; X.refresh(); }
        else if (e.key === 'Enter'){ e.preventDefault(); goView(Z.VK[r.dataset.vk]); }
        else if (e.key === 'F2' && cur && !cur.off){ e.preventDefault(); var t = $('[data-vname]', root); t.focus(); t.select(); } });
      root.addEventListener('input', function(e){ if (!e.target.hasAttribute('data-vq')) return; SS.vq = e.target.value; X.refresh(); var q = $('[data-vq]', host); if (q){ q.focus(); q.setSelectionRange(q.value.length, q.value.length); } });
      root.addEventListener('change', function(e){ if (cur && (e.target.hasAttribute('data-vno') || e.target.hasAttribute('data-vname'))) renameView(cur, $('[data-vno]', root).value.trim(), $('[data-vname]', root).value.trim()); });
      root.addEventListener('contextmenu', function(e){ var r = e.target.closest('[data-vk]'); if (!r) return; e.preventDefault(); var w = Z.VK[r.dataset.vk]; SS.vsel = w.key;
        TC.menu([{ label:'Đi tới view', icon:'zwin', kbd:'Enter', act:function(){ goView(w); } }, { label:'Đổi số, tên…', kbd:'F2', dis:w.off, act:function(){ SS.vkb = 0; X.refresh(); var t = $('[data-vname]', host); if (t){ t.focus(); t.select(); } } }, { sep:1 },
          { label:w.lbl ? 'Đặt lại nhãn view' : 'Đặt nhãn view', icon:'textnote', dis:w.off, act:function(){ goView(w); flyIn(); TC.cmd.run('SHEETSET', { label:w }); } }, { label:'Đặt callout chỉ tới view', icon:'detailsym', act:function(){ flyIn(); TC.cmd.run('SHEETSET', { callout:w }); } },
          { label:'Chọn các callout trên bản vẽ', dis:!w.co.length, act:function(){ var c = w.co.filter(function(r){ return r.d === D(); }); if (!c.length){ TC.toast('Callout của view này nằm ở bản vẽ khác: mở bản vẽ đó trước.', { type:'info', log:false }); return; } D().sel = c.map(function(r){ return r.e.id; }); TC.cad.afterSel(); } }], e.clientX, e.clientY); X.refresh(); });
    } };
  }

  /* ---------- tab View Model: vị trí tài nguyên → bản vẽ → view có tên; kéo view vào tờ để tạo viewport ---------- */
  function dim(m){ return fx(m.w / 1000, 1) + ' × ' + fx(m.h / 1000, 1) + ' m'; }
  function resList(L){
    return SS.res.map(function(dir){ var files = [];
      A().docs.forEach(function(d){ if (d.path.replace(/\\[^\\]+$/, '') !== dir || (SS.docs.indexOf(d.id) < 0 && !d.open)) return;
        files.push({ name:d.name, d:d, views:d.views.map(function(v){ var r = vRect(v), pl = []; d.layouts.forEach(function(Ly, i){ Ly.vports.forEach(function(vp){ if (viewFor(d, vp) === v) pl.push({ s:L.filter(function(s){ return s.d === d && s.sp === i; })[0] || null, vp:vp }); }); });
          return { key:d.id + '|' + vid(d, v), name:v.name, v:v, d:d, w:r[2] - r[0], h:r[3] - r[1], file:d.name, placed:pl }; }) }); });
      XR.forEach(function(x){ if (x[0] === dir) files.push({ name:x[1], d:null, views:x[2].map(function(v){ return { key:x[1] + '|' + v[0], name:v[0], w:v[1], h:v[2], file:x[1], placed:[] }; }) }); });
      return { dir:dir, files:files }; });
  }
  function thumb(c, m){
    var x = c.getContext('2d'), W = c.width, Hh = c.height; x.fillStyle = TC.cssv('--c-canvas'); x.fillRect(0, 0, W, Hh);
    if (!m.d){ x.fillStyle = TC.cssv('--c-canvas-grid-major'); x.font = '12px ' + TC.cssv('--f-ui'); x.textAlign = 'center'; x.fillText('Tệp chưa mở', W / 2, Hh / 2 + 4); return; }
    var r = vRect(m.v), f = Object.create(TC.View.prototype), v = { cx:(r[0] + r[2]) / 2, cy:(r[1] + r[3]) / 2, s:Math.min(W / (r[2] - r[0]), Hh / (r[3] - r[1])) };
    f.W = W; f.H = Hh; f.doc = m.d; f.dpr = 1;
    m.d.ents.forEach(function(e){ var b = e._bb; if (b && !(b[2] < r[0] || b[0] > r[2] || b[3] < r[1] || b[1] > r[3]) && TC.entVisible(m.d, e) && e.t !== 'xline' && e.t !== 'ray') f.drawPrims(x, e, v, false); });
  }
  function startPlace(m, sp){ var d = m.d; flyIn(); if (A().active !== d.id) A().activate(d.id); if (D().space !== sp) A().setSpace(sp); TC.cmd.run('SHEETSET', { place:m }); }
  function placeMenu(b, m){
    var d = m.d, r = b.getBoundingClientRect(), lay = X.sheets().filter(function(s){ return s.d === d && s.sp !== 'model'; });
    TC.menu([{ h:'Tờ của ' + d.name }].concat(lay.map(function(s){ return { label:'<span class="mono">' + esc(s.no) + '</span> ' + esc(cap(s.title)) + ' <span class="faint">· ' + esc(s.where) + '</span>', icon:'layout', act:function(){ startPlace(m, s.sp); } }; }),
      lay.length ? [] : [{ label:'Bản vẽ chưa có tờ (layout)', dis:true }], [{ sep:1 }, { label:'Tờ mới từ mẫu rồi đặt view…', icon:'new', act:function(){ newSheet({ doc:d, after:function(sp){ startPlace(m, sp); } }); } }]), r.left, r.top, { up:true, returnFocus:b, minWidth:300 });
  }
  var DRG = null; // view đang kéo từ palette sang vùng vẽ
  function mvTab(L){
    var F = TC.fold(SS.mq), RS = resList(L), MK = {}, cur = null, rows = '', vis = [];
    RS.forEach(function(g){ var fr = '';
      g.files.forEach(function(f){ var vs = f.views.filter(function(m){ return !F || TC.fold(m.name + ' ' + f.name).indexOf(F) >= 0; }); if (F && !vs.length) return;
        fr += '<div class="ssv-dwg">' + ic('file', 's12') + '<b class="ell">' + esc(f.name) + '</b>' + (f.d ? '<span class="tag">Đang mở</span>' : '<span class="faint">chưa mở</span>') + '<span class="faint grow" style="text-align:right">' + f.views.length + ' view</span></div>' +
          (vs.length ? vs.map(function(m){ MK[m.key] = m; vis.push(m); if (m.key === SS.msel) cur = m; var on = m === cur, pl = m.placed;
            return '<div class="ssv-row ssv-m' + (on ? ' is-sel' : '') + '" role="option" aria-selected="' + on + '" tabindex="-1" draggable="' + !!m.d + '" data-mk="' + esc(m.key) + '" title="' + esc(m.d ? 'Kéo thả vào một tờ đang mở để tạo viewport' : 'Tệp chưa mở') + '"><span class="faint">' + ic('views', 's12') + '</span><span class="t">' + esc(m.name) + '</span><span class="mono faint">' + dim(m) + '</span><span>' +
              (pl.length ? '<span class="tag ok" title="' + esc('Đã đặt trên ' + pl.map(function(q){ return (q.s ? q.s.no : '?') + ' (1:' + q.vp.scale + ')'; }).join(', ')) + '">' + esc(pl[0].s ? pl[0].s.no : 'Đã đặt') + (pl.length > 1 ? ' +' + (pl.length - 1) : '') + '</span>' : '<span class="faint">' + (m.d ? 'Chưa đặt' : '—') + '</span>') + '</span></div>'; }).join('')
            : '<div class="ssv-none faint">Chưa có view đặt tên.' + (f.d ? '<button class="btn sm ghost" type="button" data-mv="newview" data-doc="' + f.d.id + '">' + ic('plus') + 'Tạo view…</button>' : '') + '</div>'); });
      if (fr) rows += '<div class="ssv-loc" title="' + esc(g.dir) + '">' + ic('folder', 's12') + '<span class="ell">' + esc(g.dir) + '</span></div>' + fr; });
    var first = cur || vis[0];
    if (first) rows = rows.replace('data-mk="' + esc(first.key) + '"', 'data-mk="' + esc(first.key) + '" data-first');
    var body = '<div class="ssm-h"><span class="gh">View Model</span><label class="search grow">' + ic('search') + '<input data-mq placeholder="Tìm view, bản vẽ nguồn" value="' + esc(SS.mq) + '" aria-label="Tìm view Model"></label><button class="ib" type="button" data-mv="res" aria-label="Thêm vị trí tài nguyên" data-tip="Thêm vị trí tài nguyên" data-desc="Thư mục chứa bản vẽ nguồn có view đặt tên để đặt lên tờ.">' + ic('folder') + '</button></div>' +
      '<div class="ssv-list" role="listbox" aria-label="View Model">' + (rows || '<div class="empty">' + (F ? 'Không có view khớp “' + esc(SS.mq) + '”.' : '<b>Chưa có vị trí tài nguyên</b>Thêm thư mục chứa bản vẽ nguồn trong Thuộc tính bộ.') + '</div>') + '</div>';
    var det = '<div class="ssm-det">' + (cur ? '<div class="ssv-md"><canvas class="ssv-th" width="176" height="108" aria-label="Xem trước view ' + esc(cur.name) + '"></canvas><div class="stack" style="gap:4px;min-width:0"><b class="ell">' + esc(cur.name) + '</b><span class="faint ell">' + esc(cur.file) + ' · ' + dim(cur) + '</span>' +
        '<span class="faint ell">' + (cur.placed.length ? 'Đã đặt trên ' + esc(cur.placed.map(function(q){ return (q.s ? q.s.no : '?') + ' (1:' + q.vp.scale + ')'; }).join(', ')) : cur.d ? 'Chưa đặt lên tờ nào' : 'Mở ' + esc(cur.file) + ' để đặt view của tệp này') + '</span>' +
        '<span class="hrow" style="flex-wrap:nowrap;gap:6px"><select class="inp sm" data-msc aria-label="Tỉ lệ viewport" style="width:82px">' + SCS.map(function(k){ return '<option value="' + k + '"' + (k === SS.sc ? ' selected' : '') + '>1:' + k + '</option>'; }).join('') + '</select><span class="faint mono" style="font-size:var(--fs-xs)">' + Math.round(cur.w / SS.sc) + ' × ' + Math.round(cur.h / SS.sc) + ' mm</span></span>' +
        '<span class="hrow"><button class="btn sm primary" type="button" data-mv="place" aria-haspopup="menu"' + (cur.d ? '' : ' disabled') + '>' + ic('layout') + 'Đặt lên tờ' + ic('down', 'xs') + '</button></span></div></div>' +
        (cur.d ? '<span class="faint" style="font-size:var(--fs-xs)">Hoặc kéo view thả vào một tờ đang mở trên vùng vẽ: viewport theo tỉ lệ đã chọn, kèm nhãn view.</span>' : '')
      : '<span class="faint">Chọn một view để xem trước. Kéo view thả vào tờ đang mở (tab layout) để tạo viewport có nhãn view.</span>') + '</div>';
    return { body:body, det:det, wire:function(host){
      var root = $('.ssm', host), all = $$('[data-mk]', root), c = $('.ssv-th', root);
      if (c && cur) thumb(c, cur);
      if (SS.vkb){ var f = $('.ssv-m.is-sel', root); if (f) f.focus(); SS.vkb = 0; }
      var fi = $('[data-first]', root); if (fi) fi.tabIndex = 0;
      root.addEventListener('click', function(e){ var r = e.target.closest('[data-mk]'); if (r){ SS.msel = r.dataset.mk; SS.vkb = 1; X.refresh(); return; }
        var b = e.target.closest('[data-mv]'); if (!b || b.disabled) return; var k = b.dataset.mv;
        if (k === 'place' && cur) placeMenu(b, cur);
        else if (k === 'res') TC.toast('Hộp thoại chọn thư mục của Windows: thư mục chứa bản vẽ nguồn có view đặt tên.', { type:'info', log:false });
        else if (k === 'newview'){ var d = X.docBy(b.dataset.doc); if (!d) return; if (A().active !== d.id) A().activate(d.id); if (D().space !== 'model') A().setSpace('model'); SC.viewNew(d).then(function(v){ if (v) X.refresh(); }); } });
      root.addEventListener('dblclick', function(e){ var r = e.target.closest('[data-mk]'); if (!r) return; var m = MK[r.dataset.mk]; if (m && m.d){ X.nav({ d:m.d, sp:'model', key:'' }); V().zoomTo(vRect(m.v)); } });
      root.addEventListener('keydown', function(e){ var r = e.target.closest && e.target.closest('[data-mk]'); if (!r) return; var i = all.indexOf(r);
        if (e.key === 'ArrowDown' || e.key === 'ArrowUp'){ e.preventDefault(); var n = all[TC.clamp(i + (e.key === 'ArrowDown' ? 1 : -1), 0, all.length - 1)]; SS.msel = n.dataset.mk; SS.vkb = 1; X.refresh(); }
        else if (e.key === 'Enter'){ e.preventDefault(); var m = MK[r.dataset.mk]; if (m && m.d){ var b = $('[data-mv="place"]', root); if (b) placeMenu(b, m); } } });
      root.addEventListener('input', function(e){ if (!e.target.hasAttribute('data-mq')) return; SS.mq = e.target.value; X.refresh(); var q = $('[data-mq]', host); if (q){ q.focus(); q.setSelectionRange(q.value.length, q.value.length); } });
      root.addEventListener('change', function(e){ if (e.target.hasAttribute('data-msc')){ SS.sc = Number(e.target.value); X.refresh(); } });
      root.addEventListener('dragstart', function(e){ var r = e.target.closest && e.target.closest('[data-mk]'); if (!r) return; var m = MK[r.dataset.mk]; if (!m || !m.d){ e.preventDefault(); return; } DRG = { m:m, p:null }; e.dataTransfer.effectAllowed = 'copy'; e.dataTransfer.setData('text/plain', m.name); });
    } };
  }
  // kéo view từ palette thả vào tờ đang mở: xem trước khung viewport theo con trỏ
  document.addEventListener('dragover', function(e){
    if (!DRG) return; var cv = e.target.closest && e.target.closest('#cv'), d = D();
    if (!cv){ if (DRG.p){ DRG.p = null; V().drawOver(); } return; }
    e.preventDefault(); var r = cv.getBoundingClientRect(), ok = d === DRG.m.d && d.space !== 'model' && !d.activeVp;
    DRG.p = ok ? V().toW(e.clientX - r.left, e.clientY - r.top) : null; e.dataTransfer.dropEffect = ok ? 'copy' : 'none'; V().drawOver();
  });
  document.addEventListener('drop', function(e){
    if (!DRG) return; var cv = e.target.closest && e.target.closest('#cv'), m = DRG.m, p = DRG.p, d = D(); DRG = null; V().drawOver(); if (!cv) return; e.preventDefault();
    if (d !== m.d) TC.toast('View “' + esc(m.name) + '” thuộc <b>' + esc(m.d.name) + '</b>. Mở một tờ của bản vẽ đó rồi thả view vào.', { type:'info', log:false });
    else if (d.space === 'model' || !p) TC.toast('Thả view vào một tờ: chuyển sang tab layout trước.', { type:'info', log:false });
    else { var res = placeView(d, d.space, m, SS.sc, p); TC.cmdline.msg('Đã đặt view <b>' + esc(m.name) + '</b> lên tờ ' + esc(res.s ? res.s.no : d.layouts[d.space].name) + ' · 1:' + SS.sc + ' · số view ' + res.vp.no + '. Hoàn tác bằng Ctrl+Z.'); }
  });
  document.addEventListener('dragend', function(){ if (DRG){ DRG = null; V().drawOver(); } });
  TC.overlays.push(function(x, v){
    if (!DRG || !DRG.p) return; var g = vpGeo(DRG.m, SS.sc, DRG.p), a = v.toS([g.x, g.y + g.h]), b = v.toS([g.x + g.w, g.y]);
    x.save(); x.setLineDash([5, 4]); x.strokeStyle = TC.cssv('--c-accent'); x.lineWidth = 1.5; x.strokeRect(a[0], a[1], b[0] - a[0], b[1] - a[1]); x.setLineDash([]);
    x.fillStyle = TC.cssv('--c-accent'); x.font = '12px ' + TC.cssv('--f-ui'); x.fillText(DRG.m.name + ' · 1:' + SS.sc + ' · ' + Math.round(g.w) + ' × ' + Math.round(g.h) + ' mm', a[0] + 4, a[1] - 6); x.restore();
  });

  /* ---------- lệnh SHEETSET: mở palette ở tab Danh sách tờ; đặt view, nhãn view, callout từ palette ---------- */
  IMPL.SHEETSET = function(ctx, pre){
    var d = D();
    if (pre && pre.place){ var m = pre.place, L0 = d.layouts[d.space];
      if (d !== m.d || !L0){ ctx.out('Mở một tờ (tab layout) của ' + esc(m.d.name) + ' để đặt view.'); return; }
      var go = function(){ return ctx.point('Tâm viewport của view “' + esc(m.name) + '” · 1:' + SS.sc + '. Chỉ điểm hoặc đổi tỉ lệ', { opts:SCS.map(function(k){ return ['1:' + k, String(k)]; }), onMove:function(p, temp){ var g = vpGeo(m, SS.sc, p); temp.push({ k:'poly', pts:rp(g.x, g.y, g.x + g.w, g.y + g.h), closed:true, paper:true, color:TC.cssv('--c-accent'), dash:[5, 4] }); } }).then(function(r){
        if (r.kw){ SS.sc = Number(r.kw); X.refresh(); return go(); } if (!r.p) return;
        var res = placeView(d, d.space, m, SS.sc, r.p); ctx.out('Đã đặt view <b>' + esc(m.name) + '</b> lên tờ ' + esc(res.s ? res.s.no : L0.name) + ': viewport 1:' + SS.sc + ' khóa hiển thị, số view ' + res.vp.no + ', kèm nhãn view. Một bước hoàn tác.'); }); };
      return go(); }
    if (pre && pre.label){ var w = pre.label, k = w.vp ? 1 : annoK(d), rr = w.v ? vRect(w.v) : null, def = w.vp ? [w.vp.x, w.vp.y - 9] : [rr[0] + (rr[2] - rr[0]) * .08, rr[1] + 9 * k];
      if (w.s.d !== d){ ctx.out('Mở tờ ' + esc(w.s.no) + ' để đặt nhãn view.'); return; }
      return ctx.point('Điểm đặt nhãn view ' + esc(w.no) + ' “' + esc(w.name) + '” (Enter: dưới góc trái view)', { onMove:function(p, temp){ temp.push(ED.temp([labelEnt(d, w, p, k)], { real:true })); } }).then(function(r){ var p = r.p || (r.enter ? def : null); if (!p) return; var old = w.lbl && w.lbl.e;
        ED.run('Đặt nhãn view', function(){ if (old && ED.listOf(d, old)) ED.del(old); ED.add(labelEnt(d, w, p, k), w.list); }); V().draw(); X.refresh();
        ctx.out('Đã đặt nhãn view <b>' + esc(w.no + ' · ' + w.name) + '</b>. Số view, số hiệu tờ, tên và tỉ lệ theo bộ bản vẽ.'); }); }
    if (pre && pre.callout){ var w2 = pre.callout, k2 = annoK(d);
      if (SS.docs.indexOf(d.id) < 0){ ctx.out('Bản vẽ ' + esc(d.name) + ' chưa thuộc bộ ' + esc(SS.name) + ': thêm vào bộ để callout được theo dõi khi đánh số lại tờ.'); return; }
      return ctx.point('Điểm đặt callout ' + esc(w2.no + '/' + w2.s.no) + ' chỉ tới view “' + esc(w2.name) + '”', { onMove:function(p, temp){ temp.push(ED.temp([calloutEnt(d, w2, p, k2)], { real:true })); } }).then(function(r){ if (!r.p) return;
        ED.run('Đặt callout', function(){ ED.add(calloutEnt(d, w2, r.p, k2)); }); V().draw(); X.refresh();
        ctx.out('Đã đặt callout <b>' + esc(w2.no + '/' + w2.s.no) + '</b>. Khi đánh số lại tờ, tab View của tờ báo callout chưa khớp để cập nhật.'); }); }
    SS.tab = 'list'; return IMPL.TCSHEETS(ctx);
  };
  // ARCHIVE, OPENSHEETSET: chờ lead thêm vào bảng RAW (cmd.js); nút trong palette đã gọi thẳng
  IMPL.ARCHIVE = function(){ archive(); };
  IMPL.OPENSHEETSET = function(){ dstOpen(); };

  /* ---------- phần palette dùng chung với screens11.js: thanh tab, nội dung tab View ---------- */
  SC.ssm = {
    tabs:function(L){ var nv = 0, nm = 0; L.forEach(function(s){ nv += viewsOf(s).length; }); resList(L).forEach(function(g){ g.files.forEach(function(f){ nm += f.views.length; }); });
      return '<div class="tabs ssm-tabs" role="tablist" aria-label="Sheet Set Manager">' + [['list', 'Danh sách tờ', L.length], ['sv', 'View của tờ', nv], ['mv', 'View Model', nm]].map(function(t){ return '<button class="tab" role="tab" type="button" data-sst="' + t[0] + '" aria-selected="' + (SS.tab === t[0]) + '" tabindex="' + (SS.tab === t[0] ? 0 : -1) + '">' + t[1] + '<span class="cnt">' + t[2] + '</span></button>'; }).join('') + '</div>'; },
    view:function(tab, L){ return tab === 'sv' ? svTab(L) : mvTab(L); },
    newSheet:function(){ newSheet(); }, props:function(){ setProps(); }, sheetProps:function(s){ sheetProps(s); }, dstOpen:function(){ dstOpen(); }, dstSave:function(){ dstSave(); }, archive:function(){ archive(); }, sets:function(){ return setMenu(); },
    publishMenu:function(){ return [{ label:'Publish dùng thiết lập trang thay thế', sub:['A1-PDF · PDF, A1, TCVN.ctb', 'A3-PDF · PDF, A3, TCVN.ctb', 'A3-Phong-KT · Canon, A3'].map(function(x){ return { label:esc(x), act:function(){ X.printSet(x.split(' · ')[0]); } }; }) }]; }
  };
  document.addEventListener('click', function(e){ var t = e.target.closest('[data-sst]'); if (!t) return; SS.tab = t.dataset.sst; X.refresh(); var n = $('[data-sst="' + SS.tab + '"]'); if (n) n.focus(); });
  document.addEventListener('keydown', function(e){ var t = e.target.closest && e.target.closest('[data-sst]'); if (!t || (e.key !== 'ArrowLeft' && e.key !== 'ArrowRight')) return; e.preventDefault(); var K = ['list', 'sv', 'mv'], i = (K.indexOf(t.dataset.sst) + (e.key === 'ArrowRight' ? 1 : 2)) % 3; SS.tab = K[i]; X.refresh(); var n = $('[data-sst="' + SS.tab + '"]'); if (n) n.focus(); });
  // mở tờ trong bản vẽ khác hoặc chuyển không gian: palette biết tờ nào đang mở
  TC.on('space', function(){ if (SS.tab !== 'list') X.refresh(); });

  /* ---------- thuộc tính bộ (tùy biến) ---------- */
  function setProps(){
    var C = clone(SS.custom), RES = SS.res.slice(), tab = 'set';
    var dlg = TC.dialog({ id:'ssprops', title:'Thuộc tính bộ bản vẽ · ' + esc(SS.name), cmd:'SHEETSET', code:'UI-H08', w:780, h:560, flush:true,
      body:'<div class="vtabs" style="height:100%"><div class="vt-list" role="tablist" aria-label="Nhóm thuộc tính">' + [['set', 'Bộ bản vẽ', 'sheetset'], ['new', 'Tạo tờ, nhãn, callout', 'new'], ['cp', 'Thuộc tính tùy biến', 'props']].map(function(t){ return '<button class="vt" type="button" role="tab" data-spt="' + t[0] + '" aria-selected="' + (t[0] === tab) + '">' + ic(t[2]) + t[1] + '</button>'; }).join('') + '</div><div class="vt-body" id="sp-b"></div></div>',
      foot:'<span class="fmsg" id="sp-m"></span><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="sp-ok">Lưu thuộc tính bộ</button>' });
    var $d = function(s){ return $(s, dlg.el); }, V0 = { name:SS.name, desc:SS.desc, tpl:SS.tpl, ask:SS.askTpl, fname:SS.fname, pso:SS.pso, dst:SS.dstToo };
    function pane(){
      var B = '';
      if (tab === 'set') B = '<div class="stack">' + H.row('Tên bộ', '<input class="inp grow" id="sp-n" value="' + esc(V0.name) + '" spellcheck="false">', 130) + H.row('Mô tả', '<input class="inp grow" id="sp-ds" value="' + esc(V0.desc) + '">', 130) +
        H.row('Tệp bộ bản vẽ', '<span class="mono faint ell" style="font-size:var(--fs-xs)">' + esc(SS.dir + '\\' + V0.name + '.tcs') + '</span>', 130) + H.row('', H.chk('sp-dst', 'Lưu kèm <b>' + esc(V0.name) + '.dst</b> (mở được bằng AutoCAD)', V0.dst), 130) +
        H.row('Thiết lập trang thay thế', '<select class="inp" id="sp-pso" style="width:300px">' + SC.opts(['TCVN-in-an.dwt', 'Cong-ty-in-an.dwt', '(không)'], V0.pso) + '</select>', 130) +
        H.grp('Vị trí view Model', '<div class="stack" style="gap:4px">' + RES.map(function(r, i){ return '<span class="hrow" style="flex-wrap:nowrap">' + ic('folder', 's12') + '<span class="mono grow ell" style="font-size:var(--fs-xs)">' + esc(r) + '</span><button class="ib sm" type="button" data-sprm="' + i + '" aria-label="Bỏ vị trí ' + esc(r) + '">' + ic('x') + '</button></span>'; }).join('') + (RES.length ? '' : '<span class="faint">Chưa có thư mục nào: tab View Model sẽ trống.</span>') + '</div><button class="btn sm" type="button" id="sp-add" style="justify-self:start">' + ic('plus') + 'Thêm thư mục…</button>') + '</div>';
      else if (tab === 'new') B = '<div class="stack">' + H.grp('Tờ mới', H.row('Thư mục lưu tờ', '<span class="mono faint" style="font-size:var(--fs-xs)">' + esc(SS.dir) + '\\&lt;bộ môn&gt;</span>', 130) + H.row('Mẫu tờ mặc định', '<select class="inp" id="sp-tpl" style="width:300px">' + TPL.map(function(t){ return '<option value="' + esc(t[0]) + '"' + (t[0] === V0.tpl ? ' selected' : '') + '>' + esc(t[0] + ' › ' + t[1]) + '</option>'; }).join('') + '</select>', 130) + H.row('', H.chk('sp-ask', 'Hỏi chọn mẫu mỗi lần tạo tờ', V0.ask), 130) +
          H.row('Tên tệp tờ mới', '<input class="inp mono" id="sp-fn" value="' + esc(V0.fname) + '" style="width:300px" spellcheck="false">', 130) + H.row('', '<span class="faint" style="font-size:var(--fs-xs)">{SoHieu}, {TenBanVe}, {BoMon}. Ví dụ: ' + esc(fnameOf(V0.fname, 'KT 08', 'MẶT ĐỨNG TRỤC 1-4', 'KT')) + '.dwg</span>', 130)) +
        H.grp('Nhãn và callout', H.row('Block nhãn view', '<select class="inp" style="width:300px"><option>TCVN-TEN-VIEW · số view / số hiệu tờ, tên, tỉ lệ</option></select>', 130) + H.row('Block callout', '<select class="inp" style="width:300px"><option>TCVN-CHI-TIET · ký hiệu chi tiết</option><option>TCVN-MAT-CAT · ký hiệu mặt cắt</option></select>', 130) + '<span class="faint" style="font-size:var(--fs-xs)">Lấy từ Thư viện TCVN. Nhãn, callout có số view và số hiệu tờ liên kết với bộ bản vẽ.</span>') + '</div>';
      else B = '<div class="stack"><p class="faint" style="font-size:var(--fs-s);margin:0">Thuộc tính của bộ có một giá trị chung; thuộc tính của tờ có giá trị riêng từng tờ (giá trị dưới đây là mặc định). Dùng làm nguồn trong Ánh xạ khung tên và field.</p>' +
        '<div class="tblwrap"><table class="tbl sp-cp"><colgroup><col><col style="width:110px"><col style="width:240px"><col style="width:34px"></colgroup><thead><tr><th>Tên thuộc tính</th><th>Thuộc về</th><th>Giá trị (bộ) / mặc định (tờ)</th><th></th></tr></thead><tbody>' +
        C.map(function(c, i){ return '<tr><td><input class="inp sm" data-cpn="' + i + '" value="' + esc(c.n) + '" aria-label="Tên thuộc tính ' + (i + 1) + '" spellcheck="false"></td><td><select class="inp sm" data-cpo="' + i + '" aria-label="Thuộc về"><option value="p"' + (c.own === 'p' ? ' selected' : '') + '>Bộ</option><option value="s"' + (c.own === 's' ? ' selected' : '') + '>Tờ</option></select></td><td><input class="inp sm" data-cpv="' + i + '" value="' + esc(c.v) + '" aria-label="Giá trị" spellcheck="false"></td><td class="ctr"><button class="ib sm" type="button" data-cpx="' + i + '" aria-label="Xóa thuộc tính ' + esc(c.n) + '">' + ic('trash') + '</button></td></tr>'; }).join('') +
        (C.length ? '' : '<tr><td colspan="4" class="faint">Chưa có thuộc tính tùy biến.</td></tr>') + '</tbody></table></div><button class="btn sm" type="button" id="sp-cpa" style="justify-self:start">' + ic('plus') + 'Thêm thuộc tính</button></div>';
      $d('#sp-b').innerHTML = B;
    }
    function keep(){ var g = function(id){ var e = $d(id); return e ? (e.type === 'checkbox' ? e.checked : e.value) : null; }; if (tab === 'set'){ V0.name = g('#sp-n').trim(); V0.desc = g('#sp-ds'); V0.dst = g('#sp-dst'); V0.pso = g('#sp-pso'); } if (tab === 'new'){ V0.tpl = g('#sp-tpl'); V0.ask = g('#sp-ask'); V0.fname = g('#sp-fn'); } }
    dlg.el.addEventListener('click', function(e){
      var t = e.target.closest('[data-spt]'); if (t){ keep(); tab = t.dataset.spt; $$('[data-spt]', dlg.el).forEach(function(x){ x.setAttribute('aria-selected', String(x === t)); }); pane(); return; }
      var rm = e.target.closest('[data-sprm]'); if (rm){ keep(); RES.splice(Number(rm.dataset.sprm), 1); pane(); return; }
      var cx = e.target.closest('[data-cpx]'); if (cx){ C.splice(Number(cx.dataset.cpx), 1); pane(); return; }
      if (e.target.closest('#sp-add')){ keep(); var nx = ['D:\\DuAn\\NhaO-2T\\DienNuoc', '\\\\SRV-01\\Chung\\Chi-tiet-mau'].filter(function(x){ return RES.indexOf(x) < 0; })[0]; if (nx){ RES.push(nx); pane(); } else TC.toast('Hộp thoại chọn thư mục của Windows.', { type:'info', log:false }); }
      if (e.target.closest('#sp-cpa')){ C.push({ k:'', n:'Thuộc tính ' + (C.length + 1), own:'s', v:'' }); pane(); var n = $$('[data-cpn]', dlg.el).pop(); if (n){ n.focus(); n.select(); } }
    });
    dlg.el.addEventListener('input', function(e){ var t = e.target; if (t.dataset.cpn) C[t.dataset.cpn].n = t.value; else if (t.dataset.cpv) C[t.dataset.cpv].v = t.value; });
    dlg.el.addEventListener('change', function(e){ var t = e.target; if (t.dataset.cpo) C[t.dataset.cpo].own = t.value; });
    $d('#sp-ok').addEventListener('click', function(){
      keep(); var names = C.map(function(c){ return c.n.trim().toLowerCase(); }), err = !V0.name ? 'Nhập tên bộ bản vẽ.' : /[\\/:*?"<>|]/.test(V0.name) ? 'Tên bộ không chứa \\ / : * ? " < > |' : names.some(function(n){ return !n; }) ? 'Thuộc tính tùy biến cần có tên.' : names.some(function(n, i){ return names.indexOf(n) !== i; }) ? 'Hai thuộc tính trùng tên. Đặt tên khác.' : '';
      fmsg($d('#sp-m'), err); if (err){ if (/thuộc tính/i.test(err) && tab !== 'cp') $d('[data-spt="cp"]').click(); return; }
      C.forEach(function(c){ c.n = c.n.trim(); c.k = c.k || TC.fold(c.n).toUpperCase().replace(/[^A-Z0-9]+/g, '').slice(0, 12) || 'TT' + Date.now() % 1e4; if (c.own === 'p') PJ[c.k] = c.v; });
      Object.assign(SS, { name:V0.name, desc:V0.desc, dstToo:V0.dst, pso:V0.pso, tpl:V0.tpl, askTpl:V0.ask, fname:V0.fname, res:RES, custom:C }); syncSrc();
      dlg.close(true); X.refresh(); TC.cmdline.msg('Đã lưu thuộc tính bộ <b>' + esc(SS.name) + '</b>: ' + C.length + ' thuộc tính tùy biến, ' + RES.length + ' vị trí view Model.');
    });
    pane();
  }
  /* ---------- thuộc tính tờ ---------- */
  function sheetProps(s){
    if (!s) return; var m = SS.meta[s.key] || {}, cp = Object.assign({}, m.cp || {}), sc = SS.custom.filter(function(c){ return c.own === 's'; }), ps = s.d ? s.d.pageSetups.map(function(p){ return p.name; }) : [];
    var dlg = TC.dialog({ id:'shprops', title:'Thuộc tính tờ · ' + esc(s.no), cmd:'SHEETSET', code:'UI-H08', w:600,
      body:'<div class="stack"><div class="frow" style="--lab-w:150px"><span class="lab">Số hiệu</span><span class="hrow"><b class="mono">' + esc(s.no) + '</b><span class="faint" style="font-size:var(--fs-xs)">tự đánh theo bộ môn và thứ tự trong bộ</span></span>' +
        '<label class="lab" for="sh-t">Tên bản vẽ</label><input class="inp" id="sh-t" value="' + esc(s.title) + '" spellcheck="false"><label class="lab" for="sh-bm">Bộ môn</label><select class="inp" id="sh-bm" style="width:200px">' + X.BMS.map(function(b){ return '<option value="' + esc(b[0]) + '"' + (b[0] === s.bm ? ' selected' : '') + '>' + esc(b[0] + ' · ' + b[1]) + '</option>'; }).join('') + '</select>' +
        '<span class="lab">Tệp, vị trí</span><span class="mono faint ell" style="font-size:var(--fs-xs)" title="' + esc(s.dir + '\\' + s.file) + '">' + esc(s.file + ' › ' + s.where) + '</span>' +
        '<label class="lab" for="sh-ds">Mô tả</label><input class="inp" id="sh-ds" value="' + esc(m.desc || '') + '" placeholder="Ghi chú cho tờ (không in)">' +
        '<span class="lab">Khi in cả bộ</span>' + H.chk('sh-pub', 'Đưa tờ này vào', !m.noPub) +
        '<label class="lab" for="sh-ps">Thiết lập trang</label><select class="inp" id="sh-ps" style="width:260px"><option value="">Theo layout</option>' + ps.map(function(p){ return '<option' + (p === m.ps ? ' selected' : '') + '>' + esc(p) + '</option>'; }).join('') + '</select></div>' +
        H.grp('Thuộc tính tùy biến của tờ', sc.length ? '<div class="frow" style="--lab-w:138px">' + sc.map(function(c){ return '<label class="lab" for="sh-c-' + c.k + '">' + esc(c.n) + '</label><input class="inp" id="sh-c-' + c.k + '" data-shc="' + c.k + '" value="' + esc(cp[c.k] != null ? cp[c.k] : c.v) + '" placeholder="' + esc(c.v ? 'Mặc định: ' + c.v : '') + '">'; }).join('') + '</div>' : '<span class="faint">Bộ chưa có thuộc tính tùy biến của tờ. Thêm ở Thuộc tính bộ.</span>') + '</div>',
      foot:'<span class="fmsg" id="sh-m"></span><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="sh-ok">Lưu thuộc tính tờ</button>' });
    $('#sh-ok', dlg.el).addEventListener('click', function(){
      var t = $('#sh-t', dlg.el).value.trim(); if (!t){ fmsg($('#sh-m', dlg.el), 'Nhập tên bản vẽ.'); $('#sh-t', dlg.el).classList.add('err'); return; }
      $$('[data-shc]', dlg.el).forEach(function(i){ cp[i.dataset.shc] = i.value; });
      X.setMeta(s.key, { title:t === s.title ? m.title || null : t, bm:$('#sh-bm', dlg.el).value, desc:$('#sh-ds', dlg.el).value.trim(), noPub:!$('#sh-pub', dlg.el).checked, ps:$('#sh-ps', dlg.el).value, cp:cp });
      dlg.close(true); X.refresh(); TC.cmdline.msg('Đã lưu thuộc tính tờ <b>' + esc(s.no) + '</b>. “Cập nhật tất cả khung tên” để ghi vào khung tên.');
    });
  }
  /* ---------- tờ mới từ mẫu (.dwt, layout mẫu): tệp mới hoặc layout mới trong bản vẽ hiện hành ---------- */
  function fnameOf(tpl, no, title, bm){ return tpl.replace(/\{SoHieu\}/g, no.replace(/\s+/g, '-')).replace(/\{TenBanVe\}/g, cap(TC.fold(title)).replace(/[\\/:*?"<>|]+/g, '-')).replace(/\{BoMon\}/g, bm).trim(); }
  function newSheet(o){
    o = o || {}; var d0 = o.doc || D(), canLay = !!d0 && SS.docs.indexOf(d0.id) >= 0, t0 = TPL.filter(function(x){ return x[0] === SS.tpl; })[0] || TPL[0];
    var st = { t:t0, bm:t0[4], title:'', where:o.doc ? 'lay' : 'file', open:true, fn:null };
    var dlg = TC.dialog({ id:'ssnew', title:'Tờ mới từ mẫu', cmd:'SHEETSET', code:'UI-H08', w:820, focus:'#sn-t',
      body:'<div class="ssn"><div class="stack" style="gap:6px"><span class="gh">Mẫu tờ</span><div class="tree ssn-l" id="sn-l" role="listbox" tabindex="0" aria-label="Mẫu tờ"></div><button class="btn sm ghost" type="button" id="sn-br" style="justify-self:start">' + ic('open') + 'Mẫu khác (.dwt)…</button></div><div class="stack" id="sn-f"></div><div class="stack" style="gap:6px"><span class="gh">Xem trước</span><canvas id="sn-pv" width="236" height="168" class="pvc" aria-label="Xem trước khung tên"></canvas><span class="faint" id="sn-pi" style="font-size:var(--fs-xs)"></span></div></div>',
      foot:'<span class="fmsg" id="sn-m"></span><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="sn-ok"></button>' });
    var $d = function(s){ return $(s, dlg.el); };
    function no(){ return X.nextNo(st.bm); }
    function dir(){ return SS.dir + '\\' + (BMDIR[st.bm] || st.bm); }
    function fn(){ return st.fn != null ? st.fn : fnameOf(SS.fname, no(), st.title || 'Ten ban ve', st.bm); }
    function list(){ $d('#sn-l').innerHTML = TPL.map(function(t){ return '<button class="tn" type="button" role="option" data-snt="' + esc(t[0]) + '" aria-selected="' + (t === st.t) + '">' + ic('layout', 's12') + '<span class="ell"><b>' + esc(t[0]) + '</b><br><span class="faint">› ' + esc(t[1]) + ' · 1:' + t[3] + '</span></span></button>'; }).join(''); }
    function form(){
      $d('#sn-f').innerHTML = '<div class="frow" style="--lab-w:92px"><label class="lab" for="sn-bm">Bộ môn</label><select class="inp" id="sn-bm" style="width:170px">' + X.BMS.map(function(b){ return '<option value="' + esc(b[0]) + '"' + (b[0] === st.bm ? ' selected' : '') + '>' + esc(b[0] + ' · ' + b[1]) + '</option>'; }).join('') + '</select><span class="lab">Số hiệu</span><span class="hrow" style="flex-wrap:nowrap"><b class="mono" id="sn-no">' + esc(no()) + '</b><span class="faint" style="font-size:var(--fs-xs)">tự đánh theo bộ môn</span></span>' +
        '<label class="lab" for="sn-t">Tên bản vẽ</label><input class="inp" id="sn-t" value="' + esc(st.title) + '" placeholder="Ví dụ: MẶT ĐỨNG TRỤC 1-4" spellcheck="false"></div>' +
        H.grp('Tạo trong', H.rad('sn-w', 'file', 'Tệp mới', st.where === 'file') + '<div class="frow ssn-sub" style="--lab-w:62px"><label class="lab" for="sn-fn">Tên tệp</label><span class="hrow" style="flex-wrap:nowrap"><input class="inp mono grow" id="sn-fn" value="' + esc(fn()) + '" spellcheck="false"' + (st.where === 'file' ? '' : ' disabled') + '><span class="faint">.dwg</span></span><span class="lab">Thư mục</span><span class="mono faint ell" style="font-size:var(--fs-xs)">' + esc(dir()) + '</span></div>' +
          '<label class="chk' + (canLay ? '' : ' dis') + '"><input type="radio" name="sn-w" value="lay"' + (st.where === 'lay' ? ' checked' : '') + (canLay ? '' : ' disabled') + '> Layout mới trong ' + (d0 ? '<b>' + esc(d0.name) + '</b>' : 'bản vẽ hiện hành') + '</label>' + (canLay ? '' : '<span class="faint ssn-sub" style="font-size:var(--fs-xs)">Bản vẽ hiện hành chưa thuộc bộ: thêm vào bộ ở nút + của palette.</span>')) +
        H.chk('sn-open', 'Mở tờ sau khi tạo', st.open, st.where === 'lay');
      $d('#sn-ok').textContent = 'Tạo tờ ' + no();
    }
    function pv(){ var P = X.PAPER[st.t[2]], big = P[0] >= 594, k = big ? P[0] / 841 : P[0] / 420, ref = d0 || X.docBy('d1');
      X.paint($d('#sn-pv'), ref, [{ t:'insert', name:big ? 'KHUNG-A1' : 'KHUNG-A3', p:[0, 0], sx:k, sy:k, rot:0, layer:'KT-KHUNG', attrs:{ TENBV:(st.title || 'TÊN BẢN VẼ').toUpperCase(), KYHIEU:no(), TILE:'1:' + st.t[3], CONGTRINH:PJ.CONGTRINH || '' } }], true);
      $d('#sn-pi').textContent = st.t[5] + ' · khổ ' + st.t[2] + ' ' + P[0] + ' × ' + P[1] + ' mm'; }
    function err(){ var t = st.title.trim(), f = fn().trim(), e = !t ? 'Nhập tên bản vẽ.' : st.where === 'file' && !f ? 'Nhập tên tệp.' : st.where === 'file' && /[\\/:*?"<>|]/.test(f) ? 'Tên tệp không chứa \\ / : * ? " < > |' : st.where === 'file' && X.sheets().some(function(s){ return s.file.toLowerCase() === (f + '.dwg').toLowerCase(); }) ? 'Bộ đã có tệp ' + f + '.dwg. Đặt tên khác.' : st.where === 'lay' && d0.layouts.some(function(L){ return L.name === layName(); }) ? 'Bản vẽ đã có layout “' + layName() + '”.' : ''; return e; }
    function layName(){ return no().replace(/\s+/g, '-') + ' ' + cap(st.title.trim()); }
    dlg.el.addEventListener('click', function(e){ var t = e.target.closest('[data-snt]'); if (t){ st.t = TPL.filter(function(x){ return x[0] === t.dataset.snt; })[0]; st.bm = st.t[4]; st.fn = null; list(); form(); pv(); return; } if (e.target.closest('#sn-br')) TC.toast('Hộp thoại chọn mẫu .dwt; TrionCAD liệt kê các layout trong mẫu để chọn.', { type:'info', log:false }); });
    dlg.el.addEventListener('input', function(e){ var t = e.target; if (t.id === 'sn-t'){ st.title = t.value; if (st.fn == null){ var f = $d('#sn-fn'); if (f) f.value = fn(); } $d('#sn-t').classList.remove('err'); fmsg($d('#sn-m'), ''); pv(); } else if (t.id === 'sn-fn') st.fn = t.value; });
    dlg.el.addEventListener('change', function(e){ var t = e.target; if (t.id === 'sn-bm'){ st.bm = t.value; st.fn = null; form(); pv(); } else if (t.name === 'sn-w'){ st.where = t.value; form(); } else if (t.id === 'sn-open') st.open = t.checked; });
    TC.listKeys($d('#sn-l'), '[data-snt]', function(it){ it.click(); $d('#sn-l').focus(); });
    $d('#sn-ok').addEventListener('click', function(){
      var e = err(); fmsg($d('#sn-m'), e); if (e){ $d(/tệp|Tên tệp/.test(e) ? '#sn-fn' : '#sn-t').classList.add('err'); return; }
      var t = st.t, P = X.PAPER[t[2]], title = st.title.trim().toUpperCase(), n0 = no(), paper = { w:P[0], h:P[1], name:'ISO ' + t[2] + ' (' + P[0] + ',00 × ' + P[1] + ',00 mm)' };
      dlg.close(true);
      if (st.where === 'file'){
        var nd = TC.newDoc({ name:fn().trim() + '.dwg', template:t[0], paper:paper, scale:t[3], layout:t[1], title:title, code:n0, project:PJ.CONGTRINH, frame:true });
        nd.path = dir() + '\\' + nd.name; nd.layouts[0].vports = []; nd.layouts[0].ents = nd.layouts[0].ents.filter(function(x){ return x.t !== 'vpframe'; });
        TC.prepDoc(nd); A().docs.push(nd); SS.docs.push(nd.id); X.setMeta(nd.id + ':L:' + t[1], { bm:st.bm });
        if (st.open){ A().openDoc(nd.id); A().setSpace(0); }
        X.refresh(); TC.toast('Đã tạo tờ <b>' + esc(n0 + ' ' + title) + '</b> trong tệp mới <span class="mono">' + esc(nd.name) + '</span> từ mẫu ' + esc(t[0]) + '. Tờ trống: kéo view ở tab View Model vào để tạo viewport.', { log:false, act:st.open ? null : { label:'Mở tờ', fn:function(){ A().openDoc(nd.id); A().setSpace(0); } } });
        if (o.after && st.open) o.after(0);
        return;
      }
      var d = d0; if (A().active !== d.id) A().activate(d.id);
      var big = P[0] >= 594, k = big ? P[0] / 841 : P[0] / 420, L = { name:layName(), paper:paper, margin:[5, 5, 5, 5], ents:[], vports:[], setup:'' };
      ED.run('Tờ mới từ mẫu', function(){
        var done = ED.state(function(){ return { n:d.layouts.length }; }, function(s){ if (s.n < d.layouts.length) d.layouts.splice(s.n); else if (d.layouts.indexOf(L) < 0) d.layouts.push(L); A().renderLayoutTabs(); });
        d.layouts.push(L); done();
        ED.add({ t:'insert', name:big ? 'KHUNG-A1' : 'KHUNG-A3', p:[0, 0], sx:k, sy:k, rot:0, layer:'KT-KHUNG', attrs:{ TENBV:title, KYHIEU:n0, TILE:'1:' + t[3], CONGTRINH:PJ.CONGTRINH || '' } }, L.ents);
      });
      X.setMeta(d.id + ':L:' + L.name, { bm:st.bm }); A().renderLayoutTabs(); A().setSpace(d.layouts.length - 1); X.refresh();
      TC.cmdline.msg('Đã tạo tờ <b>' + esc(n0 + ' ' + title) + '</b>: layout “' + esc(L.name) + '” từ mẫu ' + esc(t[0]) + ' › ' + esc(t[1]) + '. Ctrl+Z để bỏ.');
      if (o.after) o.after(d.layouts.length - 1);
    });
    list(); form(); pv();
  }

  /* ---------- mở bộ .dst của AutoCAD: xem trước ánh xạ trước khi nhập ---------- */
  var DSTF = { file:'THCS-AnPhu.dst', dir:'\\\\SRV-01\\DuAn\\2026\\THCS-AnPhu', ver:'AutoCAD 2018', name:'THCS An Phú',
    subs:[['A - Kiến trúc', 'KT', 'Kien truc', [['A-101', 'MẶT BẰNG TỔNG THỂ', '1:500'], ['A-102', 'MẶT BẰNG TẦNG 1', '1:100'], ['A-103', 'MẶT BẰNG TẦNG 2', '1:100'], ['A-104', 'MẶT BẰNG TẦNG 3', '1:100'], ['A-105', 'MẶT BẰNG MÁI', '1:100'], ['A-201', 'MẶT ĐỨNG TRỤC 1-12', '1:100'], ['A-202', 'MẶT ĐỨNG TRỤC A-E', '1:100'], ['A-301', 'MẶT CẮT 1-1', '1:100'], ['A-302', 'MẶT CẮT 2-2', '1:100']]],
      ['Phát hành lần 1', 'KT', 'Kien truc', [['A-501', 'CHI TIẾT CẦU THANG', '1:25'], ['A-502', 'CHI TIẾT KHU VỆ SINH', '1:25'], ['A-503', 'CHI TIẾT CỬA', '1:20']], 'A - Kiến trúc'],
      ['S - Kết cấu', 'KC', 'Ket cau', [['S-101', 'MẶT BẰNG MÓNG', '1:100'], ['S-102', 'CHI TIẾT MÓNG M1, M2', '1:25'], ['S-103', 'MẶT BẰNG DẦM SÀN TẦNG 2', '1:100'], ['S-104', 'MẶT BẰNG DẦM SÀN TẦNG 3', '1:100'], ['S-105', 'MẶT BẰNG DẦM SÀN MÁI', '1:100'], ['S-201', 'CHI TIẾT CỘT', '1:25'], ['S-202', 'CHI TIẾT DẦM TẦNG 2', '1:25'], ['S-203', 'CHI TIẾT DẦM TẦNG 3', '1:25']]],
      ['E - Điện', 'ĐN', 'Dien', [['E-101', 'MẶT BẰNG CẤP ĐIỆN TẦNG 1', '1:100'], ['E-102', 'MẶT BẰNG CẤP ĐIỆN TẦNG 2', '1:100'], ['E-103', 'MẶT BẰNG CẤP ĐIỆN TẦNG 3', '1:100', 1], ['E-104', 'SƠ ĐỒ NGUYÊN LÝ CẤP ĐIỆN', '—', 1]]]],
    props:[['Project Name', 'Bộ', 'TRƯỜNG THCS AN PHÚ', 'p.CONGTRINH'], ['Client', 'Bộ', 'Ban QLDA đầu tư xây dựng TP. Thủ Đức', 'p.CHUDAUTU'], ['Project Address', 'Bộ', 'Phường An Phú, TP. Thủ Đức', 'p.DIADIEM'], ['Project Phase', 'Bộ', 'Construction Documents', 'p.GIAIDOAN'], ['Contract No.', 'Bộ', '07/2026/HĐTV', 'p.SOHD'], ['Drawn By', 'Tờ', '', 't.NGUOIVE'], ['Revision', 'Tờ', 'A', 't.LANPH'], ['Checked By', 'Tờ', 'L.V.DŨNG', 'new']] };
  var PHASE = { 'Construction Documents':'Thiết kế bản vẽ thi công', 'Design Development':'Thiết kế kỹ thuật', 'Schematic Design':'Thiết kế cơ sở' };
  var SETS = {}; // các bộ đã mở trong phiên; prototype giữ một bộ hiện hành trong palette
  function snapSet(){ return { name:SS.name, docs:SS.docs.slice(), extra:SS.extra, order:SS.order.slice(), meta:clone(SS.meta), pj:Object.assign({}, PJ), res:SS.res.slice(), custom:clone(SS.custom), map:clone(SS.map), fmt:SS.fmt, desc:SS.desc, dir:SS.dir }; }
  function useSet(name){
    if (SS.name === name || !SETS[name]) return; SETS[SS.name] = snapSet(); var s = SETS[name];
    Object.keys(PJ).forEach(function(k){ delete PJ[k]; }); Object.assign(PJ, s.pj);
    Object.assign(SS, { name:s.name, docs:s.docs.slice(), extra:s.extra, order:s.order.slice(), meta:clone(s.meta), res:s.res.slice(), custom:clone(s.custom), map:clone(s.map), fmt:s.fmt, desc:s.desc, dir:s.dir, sel:null, vsel:null, msel:null, q:'', vq:'', mq:'' });
    syncSrc(); X.refresh();
  }
  SC.ssmUse = useSet;
  function setMenu(){ if (!SETS[SS.name]) SETS[SS.name] = null; return Object.keys(SETS).map(function(n){ return { label:esc(n), check:n === SS.name, act:function(){ useSet(n); } }; }).concat([{ sep:1 }, { label:'Mở bộ .dst của AutoCAD…', icon:'open', act:dstOpen }, { label:'Mở tệp bộ bản vẽ (.tcs)…', icon:'open', act:function(){ TC.toast('Chọn tệp bộ bản vẽ .tcs của TrionCAD.', { type:'info', log:false }); } }]); }
  function dstOpen(){
    var nT = DSTF.subs.reduce(function(a, s){ return a + s[3].length; }, 0), nMiss = DSTF.subs.reduce(function(a, s){ return a + s[3].filter(function(x){ return x[3]; }).length; }, 0);
    var st = { bm:DSTF.subs.map(function(s){ return s[1]; }), num:'keep', props:DSTF.props.map(function(p){ return p[3]; }) };
    var TGT = [['p.CONGTRINH', 'Dự án · Tên công trình'], ['p.CHUDAUTU', 'Dự án · Chủ đầu tư'], ['p.DIADIEM', 'Dự án · Địa điểm'], ['p.GIAIDOAN', 'Dự án · Giai đoạn'], ['p.SOHD', 'Dự án · Số hợp đồng'], ['t.NGUOIVE', 'Tờ · Người vẽ'], ['t.LANPH', 'Tờ · Lần phát hành'], ['new', 'Thuộc tính tùy biến mới'], ['', 'Bỏ qua']];
    var tree = '<div class="tree dst-tree">' + [[0, 'sheetset', '<b>' + esc(DSTF.name) + '</b>', nT + ' tờ']].concat(DSTF.subs.map(function(s){ return [s[4] ? 2 : 1, 'folder', 'Subset “' + esc(s[0]) + '”', s[3].length + ' tờ' + (s[3].some(function(x){ return x[3]; }) ? ' · ' + s[3].filter(function(x){ return x[3]; }).length + ' thiếu' : '')]; }),
      [[1, 'props', 'Thuộc tính tùy biến', DSTF.props.length], [1, 'views', 'Vị trí view Model', 2], [1, 'detailsym', 'Block nhãn, callout (Imperial)', 3], [1, 'print', 'Thiết lập trang thay thế: acad-PSO.dwt', '']]).map(function(r){ return '<div class="tn" style="padding-left:' + (6 + r[0] * 16) + 'px">' + ic(r[1], 's12') + '<span class="ell">' + r[2] + '</span><span class="cnt">' + r[3] + '</span></div>'; }).join('') + '</div>';
    var dlg = TC.dialog({ id:'dstopen', title:'Mở bộ bản vẽ .dst của AutoCAD', cmd:'SHEETSET', code:'UI-H08', w:1000, h:640, flush:true,
      body:'<div class="dst"><div class="dst-file">' + ic('sheetset', 'md') + '<span class="grow" style="display:grid;min-width:0"><b class="mono">' + esc(DSTF.file) + '</b><span class="faint mono ell" style="font-size:var(--fs-xs)">' + esc(DSTF.dir) + '</span></span><span class="faint" style="font-size:var(--fs-s)">' + esc(DSTF.ver) + ' · ' + nT + ' tờ · ' + DSTF.subs.length + ' subset</span><button class="btn sm" type="button" id="ds-f" aria-haspopup="menu">' + ic('open') + 'Chọn tệp khác' + ic('down', 'xs') + '</button></div>' +
        '<div class="dst-cols"><div class="stack dst-l"><span class="gh">Trong tệp .dst</span>' + tree + '</div><div class="stack dst-r" id="ds-r"></div></div></div>',
      foot:'<span class="faint" style="font-size:var(--fs-s)">Tệp .dst và bản vẽ gốc không đổi. TrionCAD lưu bộ thành THCS-AnPhu.tcs.</span><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="ds-ok">Nhập bộ ' + nT + ' tờ</button>' });
    function render(){
      $('#ds-r', dlg.el).innerHTML = '<span class="gh">Nhập vào TrionCAD</span>' +
        '<div class="tblwrap"><table class="tbl"><colgroup><col><col style="width:64px"><col style="width:200px"></colgroup><thead><tr><th>Subset của AutoCAD</th><th class="num">Tờ</th><th>Bộ môn</th></tr></thead><tbody>' + DSTF.subs.map(function(s, i){ return '<tr><td>' + (s[4] ? '<span class="faint">' + esc(s[4]) + ' › </span>' : '') + esc(s[0]) + '</td><td class="num">' + s[3].length + '</td><td><select class="inp sm" data-dsb="' + i + '" aria-label="Bộ môn cho subset ' + esc(s[0]) + '" style="width:100%">' + X.BMS.map(function(b){ return '<option value="' + esc(b[0]) + '"' + (b[0] === st.bm[i] ? ' selected' : '') + '>' + esc(b[0] + ' · ' + b[1]) + '</option>'; }).join('') + '</select></td></tr>'; }).join('') + '</tbody></table></div>' +
        H.grp('Số hiệu tờ', H.rad('ds-num', 'keep', 'Giữ số hiệu của AutoCAD (A-101, S-201…)', st.num === 'keep') + H.rad('ds-num', 're', 'Đánh lại theo bộ môn của TrionCAD (KT 01, KC 01…)', st.num === 're')) +
        '<div class="tblwrap dst-props"><table class="tbl"><colgroup><col style="width:130px"><col style="width:46px"><col><col style="width:200px"></colgroup><thead><tr><th>Thuộc tính</th><th>Của</th><th>Giá trị</th><th>Nhập thành</th></tr></thead><tbody>' + DSTF.props.map(function(p, i){ var v = PHASE[p[2]] ? p[2] + ' → ' + PHASE[p[2]] : p[2];
          return '<tr' + (st.props[i] ? '' : ' class="faint"') + '><td class="mono" style="font-size:var(--fs-xs)">' + esc(p[0]) + '</td><td>' + p[1] + '</td><td title="' + esc(v) + '">' + (v ? esc(v) : '<span class="faint">(trống)</span>') + '</td><td><select class="inp sm" data-dsp="' + i + '" aria-label="Nhập ' + esc(p[0]) + ' thành" style="width:100%">' + TGT.filter(function(t){ return !t[0] || t[0] === 'new' || (p[1] === 'Tờ') === (t[0][0] === 't'); }).map(function(t){ return '<option value="' + t[0] + '"' + (t[0] === st.props[i] ? ' selected' : '') + '>' + esc(t[1]) + '</option>'; }).join('') + '</select></td></tr>'; }).join('') + '</tbody></table></div>' +
        '<div class="stack" style="gap:6px">' + (nMiss ? '<div class="note warn">' + ic('warn') + '<span>' + nMiss + ' tờ trỏ tới tệp không tìm thấy (E-103, E-104). Vẫn nhập; chỉ lại đường dẫn sau ở danh sách tờ.</span></div>' : '') +
          '<div class="note">' + ic('info') + '<span>Subset lồng “Phát hành lần 1” (3 tờ) gộp vào bộ môn của subset cha. Block nhãn, callout của mẫu Imperial thay bằng TCVN-TEN-VIEW, TCVN-CHI-TIET khi đặt view mới; nhãn có sẵn trong bản vẽ giữ nguyên.</span></div></div>';
    }
    dlg.el.addEventListener('change', function(e){ var t = e.target; if (t.dataset.dsb) st.bm[t.dataset.dsb] = t.value; else if (t.dataset.dsp){ st.props[t.dataset.dsp] = t.value; render(); } else if (t.name === 'ds-num') st.num = t.value; });
    $('#ds-f', dlg.el).addEventListener('click', function(){ var r = this.getBoundingClientRect(); TC.menu([{ h:'Tệp .dst gần đây' }, { label:esc(DSTF.file) + ' <span class="faint">· ' + esc(DSTF.ver) + '</span>', check:true, act:function(){} }, { sep:1 }, { label:'Duyệt…', icon:'open', act:function(){ TC.toast('Hộp thoại mở tệp của Windows (.dst, .tcs).', { type:'info', log:false }); } }], r.left, r.bottom + 2, { minWidth:r.width + 120 }); });
    $('#ds-ok', dlg.el).addEventListener('click', function(){
      if (!SETS[SS.name]) SETS[SS.name] = snapSet();
      var extra = [], meta = {}, pj = { DONVI:PJ.DONVI, logo:PJ.logo || '' }, custom = clone(SS.custom);
      DSTF.subs.forEach(function(sb, si){ sb[3].forEach(function(x){ var f = x[0] + ' ' + cap(TC.fold(x[1])) + '.dwg', key = f + ':' + x[0]; extra.push({ key:key, d:null, e:null, sp:0, file:f, dir:DSTF.dir + '\\' + sb[2], where:x[0], bm:st.bm[si], title:x[1], scale:x[2], paper:'A1', date:'05/10/2026', missing:!!x[3], at0:{} }); if (st.num === 'keep') meta[key] = { no:x[0] }; }); });
      DSTF.props.forEach(function(p, i){ var tg = st.props[i], v = PHASE[p[2]] || p[2]; if (!tg) return; if (tg === 'new'){ custom.push({ k:TC.fold(p[0]).toUpperCase().replace(/[^A-Z0-9]+/g, ''), n:p[0], own:p[1] === 'Tờ' ? 's' : 'p', v:v }); return; }
        var k = tg.slice(2); if (tg[0] === 'p') pj[k] = v; else custom.forEach(function(c){ if (c.k === k) c.v = v; }); });
      custom.forEach(function(c){ if (c.own === 'p' && pj[c.k] == null) pj[c.k] = c.v; });
      SETS['THCS-AnPhu'] = { name:'THCS-AnPhu', docs:[], extra:extra, order:[], meta:meta, pj:pj, res:[DSTF.dir + '\\Model\\Kien truc', DSTF.dir + '\\Model\\Ket cau'], custom:custom, map:clone(SS.map), fmt:'pad', desc:'Nhập từ ' + DSTF.file + ' (' + DSTF.ver + ').', dir:DSTF.dir };
      var from = SS.name; dlg.close(true); SS.tab = 'list'; useSet('THCS-AnPhu');
      TC.toast('Đã mở bộ <b>THCS-AnPhu</b>: ' + nT + ' tờ, ' + DSTF.subs.length + ' subset thành 3 bộ môn' + (nMiss ? ', ' + nMiss + ' tờ thiếu tệp' : '') + '. Bộ ' + esc(from) + ' vẫn ở menu Mở bộ khác.', { type:nMiss ? 'warn' : 'ok', act:{ label:'Về ' + esc(from), fn:function(){ useSet(from); } } });
    });
    render();
  }
  /* ---------- lưu thành .dst ---------- */
  function dstSave(){
    var L = X.sheets(), mdl = L.filter(function(s){ return s.sp === 'model'; }), miss = L.filter(function(s){ return s.missing; });
    var dlg = TC.dialog({ id:'dstsave', title:'Lưu bộ bản vẽ thành .dst', cmd:'SHEETSET', code:'UI-H08', w:640,
      body:'<div class="stack"><div class="frow" style="--lab-w:96px"><label class="lab" for="dv-n">Tên tệp</label><span class="hrow" style="flex-wrap:nowrap"><input class="inp mono grow" id="dv-n" value="' + esc(SS.name) + '" spellcheck="false"><span class="faint">.dst</span></span><span class="lab">Thư mục</span><span class="mono faint" style="font-size:var(--fs-xs)">' + esc(SS.dir) + '</span><label class="lab" for="dv-v">Định dạng</label><select class="inp" id="dv-v" style="width:280px"><option>AutoCAD 2013 trở lên</option><option>AutoCAD 2007–2012</option></select></div>' +
        H.grp('Ghi vào .dst', H.chk('dv-sub', 'Bộ môn thành subset (KT, KC, ĐN, CN)', true) + H.chk('dv-cp', 'Thông tin dự án và thuộc tính tùy biến thành Custom Properties', true) + H.chk('dv-res', 'Vị trí view Model, block nhãn view, block callout', true) + H.chk('dv-no', 'Số hiệu tờ (KT 01…) làm Sheet Number', true)) +
        (mdl.length ? H.grp('Tờ là khung trong Model (' + mdl.length + ' tờ: ' + esc(mdl.slice(0, 3).map(function(s){ return s.no; }).join(', ')) + (mdl.length > 3 ? '…' : '') + ')', '<span class="faint" style="font-size:var(--fs-s)">Sheet Set Manager của AutoCAD chỉ nhận tờ là layout.</span>' + H.rad('dv-m', 'ref', 'Ghi thành tờ tham chiếu (mở được bằng AutoCAD, không publish được)', true) + H.rad('dv-m', 'skip', 'Bỏ qua, chỉ ghi tờ là layout', false)) : '') +
        '<div class="note">' + ic('info') + '<span>Trạng thái in, ánh xạ khung tên và mẫu tên tệp chỉ có trong <b>' + esc(SS.name) + '.tcs</b>. Mở lại .dst bằng TrionCAD vẫn giữ các mục này nếu .tcs ở cùng thư mục.</span></div>' +
        (miss.length ? '<div class="note warn">' + ic('warn') + '<span>' + miss.length + ' tờ trỏ tới tệp không tìm thấy (' + esc(miss.map(function(s){ return s.no; }).join(', ')) + '): vẫn ghi đường dẫn cũ.</span></div>' : '') + '</div>',
      foot:'<span class="fmsg" id="dv-m"></span><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="dv-ok">Lưu thành .dst</button>' });
    $('#dv-ok', dlg.el).addEventListener('click', function(){ var n = $('#dv-n', dlg.el).value.trim(); if (!n || /[\\/:*?"<>|]/.test(n)){ $('#dv-n', dlg.el).classList.add('err'); fmsg($('#dv-m', dlg.el), !n ? 'Nhập tên tệp.' : 'Tên tệp không chứa \\ / : * ? " < > |'); return; }
      var skip = $('[name="dv-m"]:checked', dlg.el), k = skip && skip.value === 'skip' ? L.length - mdl.length : L.length; dlg.close(true);
      TC.toast('Đã lưu <span class="mono">' + esc(n) + '.dst</span> (' + esc($('#dv-v', dlg.el).value) + '): ' + k + ' tờ, ' + X.BMS.filter(function(b){ return L.some(function(s){ return s.bm === b[0]; }); }).length + ' subset. Mở được bằng Sheet Set Manager của AutoCAD.', { act:{ label:'Mở thư mục', fn:function(){} } }); });
  }
  /* ---------- lưu trữ bộ bản vẽ (ARCHIVE): cùng bố cục với Đóng gói gửi đi (UI-B11) ---------- */
  var AR = { tab:'sh', off:{}, col:{}, pkg:'zip', fmt:'Giữ nguyên', dst:true, fonts:true, uni:true, noprn:false, purge:false, note:'', job:null };
  var FSIZE = { 'Chi-tiet-cau-thang.dwg':'2,2 MB', 'Ket-cau-mong.dwg':'3,1 MB', 'Dien-T1-T2.dwg':'2,7 MB' };
  var ARG = [['set', 'Bộ bản vẽ', 'sheetset'], ['dwg', 'Bản vẽ', 'file'], ['xref', 'Tham chiếu ngoài', 'xref'], ['img', 'Ảnh', 'image'], ['font', 'Font', 'mtext'], ['ctb', 'Plot style', 'plotstyle'], ['tpl', 'Mẫu', 'layout'], ['pc3', 'Máy in (PC3)', 'print'], ['rep', 'Báo cáo', 'list']];
  function kb(s){ var m = String(s).match(/([\d,]+)\s*(KB|MB)/); return m ? Number(m[1].replace(',', '.')) * (m[2] === 'MB' ? 1024 : 1) : 0; }
  function arFiles(L){
    var F = [], add = function(o){ o.inc = !o.miss; F.push(o); }, on = L.filter(function(s){ return !AR.off[s.key]; }), seen = {}, docs = [];
    add({ g:'set', name:SS.name + '.tcs', dir:SS.dir, size:'18 KB', type:'Bộ bản vẽ TrionCAD' }); if (AR.dst) add({ g:'set', name:SS.name + '.dst', dir:SS.dir, size:'24 KB', type:'Bộ bản vẽ cho AutoCAD' });
    on.forEach(function(s){ if (s.d && docs.indexOf(s.d) < 0) docs.push(s.d); if (seen[s.file]) return; seen[s.file] = 1; var n = on.filter(function(x){ return x.file === s.file; }).length; add({ g:'dwg', name:s.file, dir:s.dir, size:s.missing ? '—' : s.d ? TC.size(s.d.bytes) : FSIZE[s.file] || '1,4 MB', type:n + ' tờ', miss:s.missing }); });
    docs.forEach(function(d){ d.xrefs.forEach(function(x){ var p = x.found || x.path; add({ g:'xref', name:p.split('\\').pop(), dir:p.replace(/\\[^\\]+$/, ''), size:x.size || '—', type:'Xref của ' + d.name, miss:x.status === 'Không tìm thấy' }); }); d.images.forEach(function(x){ add({ g:'img', name:x.name, dir:x.path.replace(/\\[^\\]+$/, ''), size:x.size || '—', type:'Ảnh của ' + d.name, miss:x.status === 'Không tìm thấy' }); }); });
    if (AR.fonts) [['TrionVN-Times.shx', '41 KB', AR.uni ? 'Font Unicode thay VNTIME' : 'Font SHX'], ['romans.shx', '24 KB', 'Font SHX']].forEach(function(f){ add({ g:'font', name:f[0], dir:'C:\\TrionCAD\\Fonts', size:f[1], type:f[2] }); });
    ['TCVN.ctb', 'monochrome.ctb'].forEach(function(n){ add({ g:'ctb', name:n, dir:'C:\\TrionCAD\\Plot Styles', size:'5 KB', type:'Plot style' }); });
    [SS.tpl, SS.pso].filter(function(n){ return n && n !== '(không)'; }).forEach(function(n){ add({ g:'tpl', name:n, dir:'C:\\TrionCAD\\Template', size:'0,3 MB', type:n === SS.pso ? 'Thiết lập trang thay thế' : 'Mẫu tờ mới' }); });
    if (!AR.noprn) add({ g:'pc3', name:'Canon iR-ADV C3530 Phong KT.pc3', dir:'C:\\TrionCAD\\Plotters', size:'2 KB', type:'Máy in của thiết lập trang' });
    add({ g:'rep', name:SS.name + ' - Bao-cao-luu-tru.txt', dir:'(tạo khi lưu trữ)', size:'3 KB', type:'Báo cáo lưu trữ' });
    return F;
  }
  function arName(){ return SS.name + '_luu-tru_' + TC.date().replace(/\//g, '-') + (AR.pkg === 'zip' ? '.zip' : ''); }
  function archive(){
    var dlg = TC.dialog({ id:'ssarch', title:'Lưu trữ bộ bản vẽ · ' + esc(SS.name), cmd:'ARCHIVE', code:'UI-H08', w:980, h:620, flush:true, body:'<div class="tx" id="ar-b"></div>', foot:'<span></span>', onClose:function(){ if (AR.job) TC.toast('Lưu trữ vẫn chạy ở nền. Theo dõi ở thanh trạng thái.', { type:'info', log:false }); } });
    var L = [], F = [];
    function render(){
      if (!document.contains(dlg.el)) return; L = X.sheets(); F = arFiles(L);
      var on = L.filter(function(s){ return !AR.off[s.key]; }), inc = F.filter(function(f){ return f.inc; }), miss = F.filter(function(f){ return f.miss; }), size = inc.reduce(function(a, f){ return a + kb(f.size); }, 0) * (AR.pkg === 'zip' ? .58 : 1), run = !!AR.job;
      var body = AR.tab === 'sh' ? '<table class="tbl"><colgroup><col style="width:30px"><col style="width:70px"><col><col style="width:210px"><col style="width:110px"></colgroup><thead><tr><th></th><th>Số hiệu</th><th>Tên bản vẽ</th><th>Tệp</th><th>Trạng thái</th></tr></thead><tbody>' +
          X.BMS.map(function(b){ var g = L.filter(function(s){ return s.bm === b[0]; }); return g.length ? '<tr class="tx-g"><td></td><td colspan="4"><b style="font-weight:var(--fw-m)">' + esc(b[1]) + '</b><span class="faint"> · ' + g.filter(function(s){ return !AR.off[s.key]; }).length + '/' + g.length + ' tờ</span></td></tr>' + g.map(function(s){ return '<tr class="' + (AR.off[s.key] ? 'faint' : '') + '"><td class="ctr"><input type="checkbox" data-aro="' + esc(s.key) + '"' + (AR.off[s.key] ? '' : ' checked') + ' aria-label="Lưu trữ tờ ' + esc(s.no) + '"></td><td class="mono">' + esc(s.no) + '</td><td title="' + esc(s.title) + '">' + esc(s.title) + '</td><td class="mono faint" style="font-size:var(--fs-xs)" title="' + esc(s.dir + '\\' + s.file) + '">' + esc(s.file) + '</td><td>' + (s.missing ? '<span class="tag warn">' + ic('warn', 's12') + 'Thiếu tệp</span>' : '<span class="faint">' + esc(s.where) + '</span>') + '</td></tr>'; }).join('') : ''; }).join('') + '</tbody></table>'
        : '<table class="tbl"><colgroup><col><col style="width:200px"><col style="width:80px"><col style="width:150px"></colgroup><thead><tr><th>Tệp</th><th>Loại</th><th class="num">Dung lượng</th><th>Trạng thái</th></tr></thead><tbody>' + ARG.map(function(g){ var G2 = F.filter(function(f){ return f.g === g[0]; }); if (!G2.length) return ''; var col = AR.col[g[0]];
          return '<tr class="tx-g"><td colspan="4"><button class="tx-tw" type="button" data-arc="' + g[0] + '" aria-expanded="' + !col + '">' + ic(col ? 'right' : 'down', 'xs') + '<b>' + g[1] + '</b><span class="faint"> · ' + G2.filter(function(f){ return f.inc; }).length + '/' + G2.length + ' tệp</span>' + (G2.some(function(f){ return f.miss; }) ? ' ' + ic('warn', 's12 warn') : '') + '</button></td></tr>' + (col ? '' : G2.map(function(f){ return '<tr class="' + (f.inc ? '' : 'faint') + '"><td style="padding-left:26px" title="' + esc(f.dir + '\\' + f.name) + '">' + ic(g[2], 's12') + ' ' + esc(f.name) + '</td><td class="faint">' + esc(f.type) + '</td><td class="num">' + esc(f.size) + '</td><td>' + (f.miss ? '<span class="tag warn">' + ic('warn', 's12') + 'Không tìm thấy</span>' : '<span class="tag ok">' + ic('check', 's12') + 'Đưa vào</span>') + '</td></tr>'; }).join('')); }).join('') + '</tbody></table>';
      $('#ar-b', dlg.el).innerHTML = '<div class="tx-main' + (run ? ' is-busy' : '') + '"><div class="tx-left"><div class="tabs" role="tablist">' + [['sh', 'Tờ (' + on.length + '/' + L.length + ')'], ['tree', 'Cây tệp (' + inc.length + ')']].map(function(t){ return '<button class="tab" role="tab" type="button" data-art="' + t[0] + '" aria-selected="' + (AR.tab === t[0]) + '">' + t[1] + '</button>'; }).join('') + '</div><div class="pal-body">' + body + '</div>' +
          (miss.length ? '<div class="note warn" style="margin:8px">' + ic('warn') + '<span>Gói lưu trữ thiếu ' + miss.length + ' tệp: ' + miss.map(function(f){ return '<b>' + esc(f.name) + '</b>'; }).join(', ') + '. Báo cáo ghi rõ để bổ sung sau.</span></div>' : '') +
          '<div class="pal-tools tx-foot"><span class="faint" style="font-size:var(--fs-s)">' + on.length + ' tờ · ' + inc.length + ' tệp · khoảng ' + (size > 1024 ? fx(size / 1024, 1) + ' MB' : Math.round(size) + ' KB') + (AR.pkg === 'zip' ? ' sau khi nén' : '') + '</span></div></div>' +
        '<div class="tx-right"><div class="grp"><span class="gh">Lưu trữ</span>' + H.row('Kiểu gói', SC.seg('arpkg', ['ZIP', 'Thư mục'], AR.pkg === 'zip' ? 'ZIP' : 'Thư mục'), 70) + H.row('Định dạng', H.sel('ar-fmt', ['Giữ nguyên', 'DWG 2018', 'DWG 2013'], AR.fmt, 150), 70) +
          '<dl class="tx-sum"><dt>Lưu vào</dt><dd class="mono" title="' + esc('D:\\DuAn\\_Luu-tru\\' + arName()) + '">D:\\DuAn\\_Luu-tru\\' + esc(arName()) + '</dd></dl></div>' +
          '<div class="grp"><span class="gh">Tùy chọn</span>' + H.chk('ar-dst', 'Kèm ' + esc(SS.name) + '.dst để mở bằng AutoCAD', AR.dst) + H.chk('ar-fonts', 'Kèm font (kể cả SHX tiếng Việt)', AR.fonts) + H.chk('ar-uni', 'Chuyển font sang Unicode', AR.uni) + H.chk('ar-noprn', 'Đặt máy in của layout về “Không”', AR.noprn) + H.chk('ar-purge', 'Dọn dẹp bản vẽ', AR.purge) + '</div>' +
          '<div class="grp"><label class="gh" for="ar-note">Ghi chú lưu trữ</label><textarea class="inp" id="ar-note" rows="4" style="height:auto;padding:5px 7px;resize:vertical" placeholder="Ví dụ: Hồ sơ phát hành lần 2, đã duyệt ngày 18/10.">' + esc(AR.note) + '</textarea></div></div></div>';
      var f = $('.dlg-f', dlg.el), J = AR.job;
      f.innerHTML = run ? '<button class="btn ghost" type="button" data-x="close">Chạy ở nền</button><span class="tx-prog"><span class="prog grow"><i style="width:' + J.p.toFixed(0) + '%"></i></span><span class="mono">' + Math.round(J.p) + '%</span><span class="faint">' + esc(J.label) + '</span></span><button class="btn danger" type="button" data-ar="stop">Hủy</button>'
        : '<button class="btn" type="button" data-ar="rep">' + ic('list') + 'Xem báo cáo</button><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" data-ar="go"' + (on.length ? '' : ' disabled') + '>' + ic('pkg') + 'Lưu trữ ' + on.length + ' tờ · ' + inc.length + ' tệp</button>';
    }
    function report(){ var inc = F.filter(function(f){ return f.inc; }), miss = F.filter(function(f){ return f.miss; }); return ['TrionCAD · Báo cáo lưu trữ bộ bản vẽ · ' + TC.date(), 'Bộ: ' + SS.name + ' · ' + L.filter(function(s){ return !AR.off[s.key]; }).length + '/' + L.length + ' tờ', 'Gói: D:\\DuAn\\_Luu-tru\\' + arName() + ' · ' + AR.fmt, ''].concat(AR.note ? ['Ghi chú:', AR.note, ''] : [], ['Tệp (' + inc.length + '):'], inc.map(function(f){ return '  ' + f.name + '  ·  ' + f.type + '  ·  ' + f.size; }), miss.length ? ['', 'Thiếu (' + miss.length + '):'].concat(miss.map(function(f){ return '  ' + f.name + '  ·  ' + f.dir; })) : []).join('\n'); }
    function start(){
      var inc = F.filter(function(f){ return f.inc; }), S = ['Thu thập tệp của ' + L.filter(function(s){ return !AR.off[s.key]; }).length + ' tờ'].concat(AR.uni ? ['Chuyển font sang Unicode'] : [], AR.purge ? ['Dọn dẹp bản vẽ'] : [], AR.noprn ? ['Đặt máy in về “Không”'] : [], inc.map(function(f){ return 'Sao chép ' + f.name; }), [AR.pkg === 'zip' ? 'Nén ZIP' : 'Ghi thư mục', 'Ghi báo cáo lưu trữ']), k = 0, name = arName(), miss = F.filter(function(f){ return f.miss; }).length;
      var J = AR.job = { p:0, label:S[0], stop:false };
      var tick = function(){ render(); A().setTask(AR.job ? '<span class="sb-task"><span class="spin"></span>Lưu trữ ' + esc(SS.name) + ' ' + Math.round(AR.job.p) + '%<span class="prog"><i style="width:' + AR.job.p.toFixed(0) + '%"></i></span><button class="ib sm" type="button" id="ar-x" aria-label="Hủy lưu trữ">' + ic('x') + '</button></span>' : ''); var x = $('#ar-x'); if (x) x.onclick = function(){ if (AR.job) AR.job.stop = true; }; };
      tick();
      var t = setInterval(function(){
        if (J.stop){ clearInterval(t); AR.job = null; tick(); TC.toast('Đã hủy lưu trữ. Chưa tạo tệp nào.', { type:'warn', log:false }); return; }
        k++; J.p = Math.min(100, k / S.length * 100); J.label = S[Math.min(k, S.length - 1)]; tick(); if (k < S.length) return;
        clearInterval(t); AR.job = null; tick(); if (document.contains(dlg.el)) dlg.close(true);
        TC.toast('Đã lưu trữ bộ <b>' + esc(SS.name) + '</b>: ' + inc.length + ' tệp vào <span class="mono">' + esc(name) + '</span>' + (miss ? '. Thiếu ' + miss + ' tệp: xem báo cáo.' : '.'), { type:miss ? 'warn' : 'ok', act:{ label:'Mở thư mục', fn:function(){} }, kind:'task' });
      }, 200);
    }
    dlg.el.addEventListener('click', function(e){
      var tb = e.target.closest('[data-art]'); if (tb){ AR.tab = tb.dataset.art; render(); return; }
      var cl = e.target.closest('[data-arc]'); if (cl){ AR.col[cl.dataset.arc] = !AR.col[cl.dataset.arc]; render(); var c2 = $('[data-arc="' + cl.dataset.arc + '"]', dlg.el); if (c2) c2.focus(); return; }
      var sg = e.target.closest('[data-seg="arpkg"] button'); if (sg){ AR.pkg = sg.dataset.v === 'ZIP' ? 'zip' : 'dir'; render(); return; }
      var b = e.target.closest('[data-ar]'); if (!b || b.disabled) return;
      if (b.dataset.ar === 'go') start(); else if (b.dataset.ar === 'stop'){ if (AR.job) AR.job.stop = true; } else if (b.dataset.ar === 'rep') SC.textWindow(report(), 'Báo cáo lưu trữ · ' + SS.name);
    });
    dlg.el.addEventListener('change', function(e){ var t = e.target; if (t.dataset.aro){ AR.off[t.dataset.aro] = !t.checked; render(); } else if (/^ar-(dst|fonts|uni|noprn|purge)$/.test(t.id)){ AR[t.id.slice(3)] = t.checked; render(); } else if (t.id === 'ar-fmt'){ AR.fmt = t.value; render(); } });
    dlg.el.addEventListener('input', function(e){ if (e.target.id === 'ar-note') AR.note = e.target.value; });
    render();
  }

  /* ---------- danh mục: trạng thái demo của Sheet Set Manager ---------- */
  // tên, số cho viewport; nhãn view của KT 04; hai callout trên mặt bằng chỉ tới tờ chi tiết cầu thang (một callout ghi số hiệu cũ)
  function demoH08(d){
    var L1 = d.layouts.filter(function(L){ return L.name === 'A1-Mat bang'; })[0], L2 = d.layouts.filter(function(L){ return L.name === 'A1-Mat cat'; })[0], add = function(e, list){ e.id = ++d.seq; if (list !== d.ents) e._paper = true; list.push(e); TC.prepEnt(d, e); return e; };
    if (L1){ [['vp1', '1', 'Mặt bằng tầng 1'], ['vp2', '2', 'Mặt bằng tầng 2']].forEach(function(z){ var vp = L1.vports.filter(function(q){ return q.id === z[0]; })[0]; if (vp && !vp.name){ vp.no = z[1]; vp.name = z[2]; vp.view = z[2]; } }); }
    if (L2){ var v3 = L2.vports.filter(function(q){ return q.id === 'vp3'; })[0]; if (v3 && !v3.name){ v3.no = '1'; v3.name = 'Mặt cắt 1-1'; v3.view = 'Mặt cắt 1-1'; } }
    var L = X.sheets(), s4 = L.filter(function(s){ return s.d === d && s.where === 'A1-Mat bang'; })[0], stair = 'Chi-tiet-cau-thang.dwg:CT-CAU-THANG', sst = L.filter(function(s){ return s.key === stair; })[0];
    if (s4 && L1 && !L1.ents.some(function(e){ return e.ssRef; })){ var w = viewsOf(s4)[0], vp = L1.vports[0]; add(labelEnt(d, w, [vp.x, vp.y - 9], 1), L1.ents); }
    if (sst && !d.ents.some(function(e){ return e.ssRef && e.ssRef.co; })){ SC.ensureBlock(d, CO);
      add({ t:'insert', name:CO, p:[6300, 6600], sx:100, sy:100, rot:0, layer:layerOf(d), attrs:{ SO:'1', TO:sst.no }, ssRef:{ k:stair, v:'1', co:1 } }, d.ents);
      add({ t:'insert', name:CO, p:[48000 + 6300, 6600], sx:100, sy:100, rot:0, layer:layerOf(d), attrs:{ SO:'2', TO:sst.bm + ' ' + ('0' + (sst.idx + 1)).slice(-2) }, ssRef:{ k:stair, v:'2', co:1 } }, d.ents); }
  }
  OPEN['UI-H08'] = function(){
    var d = SC.cat.PRO(); if (SETS['NhaO-2T']) useSet('NhaO-2T'); if (d.id !== 'd1') A().activate('d1'); d = D();
    SS.q = ''; SS.vq = ''; SS.mq = ''; demoH08(d);
    A().setSpace(0); TC.cmd.run('SHEETSET'); SS.tab = 'sv'; SS.vsel = 'd1:L:A1-Mat bang#vp1'; SS.msel = 'd1|' + vid(d, d.views[3]); X.refresh();
    setTimeout(function(){ d.layouts[0].view = null; V().draw(); }, 80); // khung tờ vừa vùng vẽ sau khi palette mở
  };
  // UI-H07 (screens11.js) mở cùng palette: về bộ NhaO-2T, tab Danh sách tờ
  var o7 = SC.open4 && SC.open4['UI-H07']; if (o7) SC.open4['UI-H07'] = function(){ if (SETS['NhaO-2T']) useSet('NhaO-2T'); SS.tab = 'list'; o7(); };

  /* ================= Quản lý máy in, trình sửa PC3 UI-H10 (PLOTTERMANAGER) =================
     Danh sách cấu hình PC3 bên trái; trình sửa bên phải: Chung, Cổng, Thiết lập thiết bị (nguồn và khổ giấy, đồ họa,
     khổ tùy chỉnh, vùng in được, hiệu chỉnh PMP, lọc khổ). Trình tạo khổ tùy chỉnh và hiệu chỉnh PMP theo bước. */
  var PDIR = 'C:\\Users\\An\\AppData\\Roaming\\TrionCAD\\Plotters';
  var STD = { iso:[['ISO A0', 1189, 841], ['ISO A1', 841, 594], ['ISO A2', 594, 420], ['ISO A3', 420, 297], ['ISO A4', 297, 210], ['ISO A3 kéo dài', 891, 420]], office:[['ISO A3', 420, 297], ['ISO A4', 297, 210], ['ISO A5', 210, 148], ['JIS B4', 364, 257]], roll:[['ISO A0', 1189, 841], ['ISO A1', 841, 594], ['ISO A2', 594, 420], ['ISO A3', 420, 297], ['ARCH D', 914, 610]] };
  var PSRC = { pdf:['—'], img:['—'], office:['Tự chọn khay', 'Khay 1', 'Khay 2', 'Khay tay'], roll:['Cuộn 1 (914 mm)', 'Tờ rời'] };
  var PORTS = [['\\\\SRV-01\\Canon-KT', 'Mạng', 'Máy in dùng chung phòng Kiến trúc'], ['192.168.1.45:9100', 'Mạng (TCP/IP)', 'HP DesignJet T650'], ['USB001', 'USB', 'Cổng máy in USB ảo 001'], ['\\\\SRV-01\\Ricoh-HC', 'Mạng', 'Máy in phòng Hành chính'], ['PORTPROMPT:', 'Cục bộ', 'Hỏi tên tệp khi in'], ['LPT1', 'Song song', 'Cổng máy in'], ['COM1', 'Nối tiếp', 'Cổng nối tiếp']];
  function pc(o){ return Object.assign({ desc:'', kind:'sys', port:{ m:'port', n:'' }, src:'', media:'Giấy thường', std:'iso', mg:[0, 0, 0, 0], area:{}, custom:[], hide:{}, gfx:{ depth:'Màu thật (24 bit)', dpi:600, ras:'Cao', ttf:'text', merge:'over', dither:true }, pmp:null, used:0 }, o); }
  var PCS = [
    pc({ name:'PDF của TrionCAD', file:'PDF cua TrionCAD.pc3', kind:'pdf', drv:'TrionCAD PDF 1.0 (tích hợp)', desc:'PDF vector, giữ layer, chữ SHX tìm được. Mặc định của In và Xuất PDF.', def:true, port:{ m:'file', n:'D:\\DuAn\\NhaO-2T\\PDF' }, used:4 }),
    pc({ name:'DWG To PDF', file:'DWG To PDF.pc3', kind:'pdf', drv:'TrionCAD PDF 1.0 · tương thích DWG To PDF', desc:'Chuyển từ AutoCAD 2024 (TCMIGRATE). Thiết lập trang trỏ tới DWG To PDF.pc3 in qua trình PDF của TrionCAD.', from:'AutoCAD 2024', port:{ m:'file', n:'D:\\DuAn\\NhaO-2T\\PDF' }, hide:{ 'ISO A3 kéo dài':true }, used:2 }),
    pc({ name:'PNG của TrionCAD', file:'PNG cua TrionCAD.pc3', kind:'img', drv:'TrionCAD Raster 1.0 (tích hợp)', desc:'Ảnh PNG 300 DPI, nền trắng.', port:{ m:'file', n:'D:\\DuAn\\NhaO-2T\\PNG' }, gfx:{ depth:'Màu thật (24 bit)', dpi:300, ras:'Cao', ttf:'shape', merge:'over', dither:false } }),
    pc({ name:'Canon iR-ADV C3530 (Phòng KT)', file:'Canon iR-ADV C3530 Phong KT.pc3', drv:'Canon Generic Plus UFR II 2.80 · máy in Windows', desc:'Máy in A3 màu dùng chung của phòng Kiến trúc.', port:{ m:'port', n:'\\\\SRV-01\\Canon-KT' }, src:'Tự chọn khay', std:'office', mg:[4.2, 4.2, 4.2, 4.2], area:{ 'ISO A3':[5, 5, 5, 5] }, used:3 }),
    pc({ name:'HP DesignJet T650 (khổ A1)', file:'HP DesignJet T650 kho A1.pc3', drv:'HP DesignJet T650 HPGL2 4.1 · máy in Windows', desc:'Máy in khổ lớn, cuộn 914 mm: bản vẽ A1, A0 và khổ dài.', port:{ m:'port', n:'192.168.1.45:9100' }, src:'Cuộn 1 (914 mm)', roll:914, std:'roll', mg:[5, 5, 5, 5], custom:[['A1 kéo dài', 841, 1189, [5, 5, 5, 5]], ['A0 cuộn 914', 914, 1189, [5, 17, 5, 5]]], pmp:{ file:'HP-DesignJet-T650.pmp', dx:-0.30, dy:0.16, date:'12/09/2026' }, used:2 }),
    pc({ name:'Microsoft Print to PDF', file:'Microsoft Print to PDF.pc3', drv:'Microsoft Print To PDF · máy in Windows', desc:'Máy in PDF của Windows. Nên dùng PDF của TrionCAD để giữ layer và chữ tìm được.', port:{ m:'port', n:'PORTPROMPT:' }, src:'Tự chọn khay', std:'office' }),
    pc({ name:'Ricoh MP 2014 (cũ)', file:'Ricoh MP 2014 cu.pc3', drv:'Ricoh MP 2014 PCL6', desc:'Máy in phòng Hành chính (đã thay).', port:{ m:'port', n:'\\\\SRV-01\\Ricoh-HC' }, src:'Khay 1', std:'office', miss:true, used:1 })
  ];
  var PNODES = [['Giấy', [['src', 'Nguồn và khổ giấy'], ['media', 'Loại giấy']]], ['Đồ họa', [['vec', 'Đồ họa vector'], ['ras', 'Ảnh raster'], ['ttf', 'Chữ TrueType'], ['merge', 'Gộp nét chồng']]], ['Khổ giấy, hiệu chỉnh', [['cust', 'Khổ giấy tùy chỉnh'], ['area', 'Sửa vùng in được'], ['pmp', 'Hiệu chỉnh tỉ lệ (PMP)'], ['filt', 'Lọc khổ giấy']]], ['Khác', [['drv', 'Thuộc tính driver']]]];
  var PM = { sel:0, tab:'gen', node:'src', size:null, q:'', dlg:null };
  function sizes(c){ return STD[c.std].map(function(s){ return { name:s[0], w:s[1], h:s[2], mg:c.area[s[0]] || c.mg, std:true, edited:!!c.area[s[0]] }; }).concat(c.custom.map(function(s){ return { name:s[0], w:s[1], h:s[2], mg:s[3], std:false }; })); }
  function pcIcon(c){ return c.kind === 'pdf' ? 'pdf' : c.kind === 'img' ? 'image' : 'print'; }
  function mgTxt(m){ return m.map(function(x){ return fx(x, x % 1 ? 1 : 0); }).join(' · '); }
  // khổ giấy và vùng in được: giấy trắng, vùng in nét đứt, lề ghi số
  function paperPv(cv, w, h, mg){
    var x = cv.getContext('2d'), W = cv.width, Hh = cv.height, s = Math.min((W - 28) / w, (Hh - 28) / h), x0 = (W - w * s) / 2, y0 = (Hh - h * s) / 2;
    x.clearRect(0, 0, W, Hh); x.fillStyle = TC.cssv('--c-sunken'); x.fillRect(0, 0, W, Hh);
    x.fillStyle = TC.cssv('--c-paper-shadow'); x.fillRect(x0 + 3, y0 + 3, w * s, h * s); x.fillStyle = TC.cssv('--c-paper'); x.fillRect(x0, y0, w * s, h * s);
    x.strokeStyle = TC.cssv('--c-accent'); x.setLineDash([4, 3]); x.lineWidth = 1.2; x.strokeRect(x0 + mg[2] * s, y0 + mg[0] * s, (w - mg[2] - mg[3]) * s, (h - mg[0] - mg[1]) * s); x.setLineDash([]);
    x.fillStyle = TC.cssv('--c-text-2'); x.font = '11px ' + TC.cssv('--f-mono'); x.textAlign = 'center'; x.fillText(fx(w, 0) + ' × ' + fx(h, 0) + ' mm', W / 2, y0 + h * s / 2 + 4);
  }
  SC.pc3 = function(dev){
    if (!TC.can('PRO')){ SC.upgrade('Quản lý máy in, sửa cấu hình PC3', 'PRO'); return; }
    if (PM.dlg && !PM.dlg.closed) PM.dlg.close(true);
    var W = PCS.map(clone), base = PCS.map(clone);
    var ix = dev ? W.map(function(c){ return c.name; }).indexOf(dev) : -1; if (ix >= 0){ PM.sel = ix; PM.size = null; } PM.sel = TC.clamp(PM.sel, 0, W.length - 1);
    function changed(i){ return !base[i] || JSON.stringify(W[i]) !== JSON.stringify(base[i]); }
    function dirty(){ return W.filter(function(c, i){ return changed(i); }).length; }
    var dlg = PM.dlg = TC.dialog({ id:'pc3', title:'Quản lý máy in', cmd:'PLOTTERMANAGER', code:'UI-H10', w:1010, h:620, flush:true, body:'<div class="pc3" id="pm-b"></div>', foot:'<span></span>',
      onClose:function(r){ if (r === true || !dirty()) return; TC.confirm({ title:'Bỏ thay đổi', msg:'Bỏ thay đổi ở <b>' + dirty() + ' cấu hình máy in</b>?', detail:'Các tệp PC3 giữ như trước khi mở Quản lý máy in.', ok:'Bỏ thay đổi', danger:true }).then(function(y){ if (y) dlg.close(true); }); return false; } });
    var $d = function(s){ return $(s, dlg.el); };
    function cur(){ return W[PM.sel]; }
    function head(c){ return '<div class="pc3-h">' + ic(pcIcon(c), 'md') + '<span class="grow" style="display:grid;min-width:0"><b class="ell">' + esc(c.name) + '</b><span class="faint mono ell" style="font-size:var(--fs-xs)">' + esc(PDIR + '\\' + c.file) + '</span></span>' + (c.from ? '<span class="tag">Từ ' + esc(c.from) + '</span>' : '') + (c.def ? '<span class="tag acc">Mặc định</span>' : '') + (c.miss ? '<span class="tag danger">' + ic('error', 's12') + 'Thiếu driver</span>' : '') + '</div>'; }
    function genPane(c){
      return '<div class="stack">' + (c.miss ? '<div class="note err">' + ic('error') + '<span>Không tìm thấy driver <b>' + esc(c.drv) + '</b> trên máy này. Cấu hình vẫn giữ khổ giấy, vùng in và cổng; chọn driver khác để dùng lại, hoặc xóa cấu hình. ' + c.used + ' thiết lập trang đang trỏ tới máy in này.</span></div><div class="hrow"><button class="btn sm" type="button" data-pm="drvpick" aria-haspopup="menu">' + ic('print') + 'Chọn driver khác' + ic('down', 'xs') + '</button><button class="btn sm ghost" type="button" data-pm="del">' + ic('trash') + 'Xóa cấu hình…</button></div>' : '') +
        '<div class="frow" style="--lab-w:120px"><label class="lab" for="pm-n">Tên cấu hình</label><span class="hrow" style="flex-wrap:nowrap"><input class="inp grow" id="pm-n" data-pf="name" value="' + esc(c.name) + '" spellcheck="false"><span class="faint mono" style="font-size:var(--fs-xs)">.pc3</span></span>' +
        '<label class="lab" for="pm-ds">Mô tả</label><textarea class="inp" id="pm-ds" data-pf="desc" rows="2" style="height:auto;padding:5px 7px;resize:vertical">' + esc(c.desc) + '</textarea>' +
        '<span class="lab">Trình điều khiển</span><span>' + esc(c.drv) + (c.kind === 'sys' ? '' : ' <span class="tag acc">TrionCAD</span>') + '</span>' +
        '<span class="lab">Cổng</span><span class="mono" style="font-size:var(--fs-s)">' + esc(c.port.m === 'file' ? 'In ra tệp · ' + c.port.n : c.port.n || '—') + '</span>' +
        '<span class="lab">Đang dùng bởi</span><span>' + (c.used ? c.used + ' thiết lập trang trong các bản vẽ đang mở' : '<span class="faint">Chưa thiết lập trang nào</span>') + '</span>' +
        '<span class="lab">Khổ giấy</span><span>' + sizes(c).filter(function(s){ return !c.hide[s.name]; }).length + ' khổ hiện trong hộp In' + (c.custom.length ? ' · ' + c.custom.length + ' khổ tùy chỉnh' : '') + (c.pmp ? ' · có hiệu chỉnh PMP' : '') + '</span></div>' +
        '<div class="hrow"><button class="btn sm" type="button" data-pm="def"' + (c.def || c.miss ? ' disabled' : '') + '>' + ic('check') + 'Đặt làm máy in mặc định</button>' + (c.kind === 'sys' && !c.miss ? '<button class="btn sm ghost" type="button" data-pm="winprop">' + ic('ext') + 'Thuộc tính driver của Windows…</button>' : '') + '</div></div>';
    }
    function portPane(c){
      var sys = c.kind === 'sys', list = PORTS.filter(function(p){ return PM.allPorts || p[1] !== 'Song song' && p[1] !== 'Nối tiếp'; });
      return '<div class="stack">' + (sys ? '' : '<div class="note">' + ic('info') + '<span>Trình điều khiển của TrionCAD luôn in ra tệp. Đặt thư mục mặc định cho tệp kết quả.</span></div>') +
        '<label class="chk' + (sys ? '' : ' dis') + '"><input type="radio" name="pm-pm" value="port"' + (c.port.m === 'port' ? ' checked' : '') + (sys ? '' : ' disabled') + '> In ra cổng</label>' +
        '<div class="tblwrap pc3-ports' + (c.port.m === 'port' ? '' : ' is-dim') + '"><table class="tbl"><colgroup><col style="width:30px"><col style="width:190px"><col style="width:120px"><col></colgroup><thead><tr><th></th><th>Cổng</th><th>Loại</th><th>Mô tả</th></tr></thead><tbody>' +
          list.map(function(p){ var on = c.port.m === 'port' && c.port.n === p[0]; return '<tr data-pport="' + esc(p[0]) + '" class="' + (on ? 'is-sel' : '') + '"><td class="ctr">' + (on ? ic('check', 's12') : '') + '</td><td class="mono">' + esc(p[0]) + '</td><td>' + esc(p[1]) + '</td><td class="faint">' + esc(p[2]) + '</td></tr>'; }).join('') + '</tbody></table></div>' +
        '<div class="hrow"><label class="chk' + (sys ? '' : ' dis') + '"><input type="checkbox" id="pm-allp"' + (PM.allPorts ? ' checked' : '') + (sys ? '' : ' disabled') + '> Hiện mọi cổng</label><span class="grow"></span><button class="btn sm" type="button" data-pm="netport"' + (sys ? '' : ' disabled') + '>' + ic('plus') + 'Thêm cổng mạng…</button><button class="btn sm" type="button" data-pm="cfgport"' + (sys && c.port.m === 'port' ? '' : ' disabled') + '>Cấu hình cổng…</button></div>' +
        H.rad('pm-pm', 'file', 'In ra tệp', c.port.m === 'file') + '<div class="frow" style="--lab-w:128px;padding-left:20px"><label class="lab" for="pm-fd">Thư mục mặc định</label><span class="hrow" style="flex-wrap:nowrap"><input class="inp mono grow" id="pm-fd" data-pf="filedir" value="' + esc(c.port.m === 'file' ? c.port.n : 'D:\\DuAn\\NhaO-2T\\PDF') + '" style="font-size:var(--fs-xs)"' + (c.port.m === 'file' ? '' : ' disabled') + '><button class="btn sm" type="button" data-pm="browse"' + (c.port.m === 'file' ? '' : ' disabled') + '>Duyệt…</button></span></div></div>';
    }
    function nodePane(c){
      var k = PM.node, S = sizes(c), sel = S.filter(function(s){ return s.name === PM.size; })[0] || S.filter(function(s){ return !c.hide[s.name]; })[0] || S[0]; PM.size = sel && sel.name;
      var sizeTbl = function(mode){ return '<div class="tblwrap pc3-sz"><table class="tbl"><colgroup>' + (mode === 'filt' ? '<col style="width:30px">' : '') + '<col><col style="width:116px"><col style="width:132px"></colgroup><thead><tr>' + (mode === 'filt' ? '<th></th>' : '') + '<th>Khổ giấy</th><th>Kích thước (mm)</th><th title="Lề trên · dưới · trái · phải (mm)">Lề T · D · Tr · Ph</th></tr></thead><tbody>' +
        S.filter(function(s){ return mode !== 'area' || s.std; }).map(function(s){ return '<tr data-psz="' + esc(s.name) + '" class="' + (s === sel && mode !== 'filt' ? 'is-sel' : '') + (c.hide[s.name] && mode !== 'filt' ? ' faint' : '') + '">' + (mode === 'filt' ? '<td class="ctr"><input type="checkbox" data-phide="' + esc(s.name) + '"' + (c.hide[s.name] ? '' : ' checked') + ' aria-label="Hiện ' + esc(s.name) + ' trong hộp In"></td>' : '') + '<td>' + esc(s.name) + (s.std ? '' : ' <span class="tag info">tùy chỉnh</span>') + (s.edited ? ' <span class="tag warn">đã sửa lề</span>' : '') + '</td><td class="mono">' + s.w + ' × ' + s.h + '</td><td class="mono faint">' + mgTxt(s.mg) + '</td></tr>'; }).join('') + '</tbody></table></div>'; };
      if (k === 'src') return '<div class="stack">' + H.row('Nguồn giấy', '<select class="inp" data-pf="src" style="width:220px"' + (c.kind === 'sys' ? '' : ' disabled') + '>' + SC.opts(c.kind === 'sys' ? PSRC[c.std === 'roll' ? 'roll' : 'office'] : ['—'], c.src || '—') + '</select>' + (c.roll ? '<span class="faint">Cuộn rộng ' + c.roll + ' mm</span>' : ''), 92) + sizeTbl('src') +
        (sel ? '<div class="pc3-pvw"><canvas id="pm-pv" class="pc3-cv" width="220" height="150" aria-label="Xem trước khổ giấy"></canvas><div class="frow" style="--lab-w:110px;font-size:var(--fs-s)"><span class="lab">Khổ</span><b>' + esc(sel.name) + '</b><span class="lab">Vùng in được</span><span class="mono">' + fx(sel.w - sel.mg[2] - sel.mg[3], 1) + ' × ' + fx(sel.h - sel.mg[0] - sel.mg[1], 1) + ' mm</span><span class="lab">Trong hộp In</span><span>' + (c.hide[sel.name] ? 'Đang ẩn (Lọc khổ giấy)' : 'Hiện') + '</span></div></div>' : '') + '</div>';
      if (k === 'media') return '<div class="stack">' + H.row('Loại giấy', '<select class="inp" data-pf="media" style="width:220px"' + (c.kind === 'sys' ? '' : ' disabled') + '>' + SC.opts(['Giấy thường', 'Giấy can (bóng mờ)', 'Giấy phủ (coated)', 'Giấy ảnh'], c.media) + '</select>', 92) + '<span class="faint" style="font-size:var(--fs-s)">Máy in khổ lớn chỉnh lượng mực theo loại giấy. Trình điều khiển của TrionCAD không dùng mục này.</span></div>';
      if (k === 'vec') return '<div class="stack">' + H.row('Độ sâu màu', '<select class="inp" data-pg="depth" style="width:220px">' + SC.opts(['Đen trắng (1 bit)', 'Thang xám (8 bit)', '256 màu (8 bit)', 'Màu thật (24 bit)'], c.gfx.depth) + '</select>', 110) + H.row('Độ phân giải', '<select class="inp" data-pg="dpi" style="width:120px">' + SC.opts(['300', '600', '1200'], String(c.gfx.dpi)) + '</select><span class="faint">DPI</span>', 110) + H.row('', H.chk('pm-dith', 'Làm mịn chuyển màu (dither)', c.gfx.dither), 110) + '</div>';
      if (k === 'ras') return '<div class="stack">' + H.row('Chất lượng ảnh', '<select class="inp" data-pg="ras" style="width:220px">' + SC.opts(['Nháp', 'Thường', 'Cao', 'Tối đa'], c.gfx.ras) + '</select>', 110) + '<span class="faint" style="font-size:var(--fs-s)">Áp dụng cho ảnh raster, PDF underlay và viewport có shade. “Tối đa” in chậm hơn, tệp lớn hơn.</span></div>';
      if (k === 'ttf') return '<div class="stack">' + H.rad('pm-ttf', 'text', 'In dạng chữ (tệp nhỏ, tìm và copy được trong PDF)', c.gfx.ttf === 'text') + H.rad('pm-ttf', 'shape', 'In dạng hình (đúng nét chữ khi máy in thiếu font tiếng Việt)', c.gfx.ttf === 'shape') + '</div>';
      if (k === 'merge') return '<div class="stack">' + H.rad('pm-mg', 'over', 'Nét vẽ sau đè lên nét vẽ trước', c.gfx.merge === 'over') + H.rad('pm-mg', 'blend', 'Trộn màu chỗ các nét giao nhau', c.gfx.merge === 'blend') + '</div>';
      if (k === 'cust') return '<div class="stack"><div class="tblwrap pc3-sz"><table class="tbl"><colgroup><col><col style="width:116px"><col style="width:132px"><col style="width:64px"></colgroup><thead><tr><th>Khổ tùy chỉnh</th><th>Kích thước (mm)</th><th title="Lề trên · dưới · trái · phải (mm)">Lề T · D · Tr · Ph</th><th></th></tr></thead><tbody>' +
          (c.custom.length ? c.custom.map(function(s, i){ return '<tr><td>' + esc(s[0]) + '</td><td class="mono">' + s[1] + ' × ' + s[2] + '</td><td class="mono faint">' + mgTxt(s[3]) + '</td><td class="ctr"><button class="ib sm" type="button" data-pce="' + i + '" aria-label="Sửa ' + esc(s[0]) + '" data-tip="Sửa">' + ic('wrench') + '</button><button class="ib sm" type="button" data-pcx="' + i + '" aria-label="Xóa ' + esc(s[0]) + '" data-tip="Xóa">' + ic('trash') + '</button></td></tr>'; }).join('') : '<tr><td colspan="4" class="faint">Chưa có khổ tùy chỉnh. Thêm khổ dài, khổ cuộn khi bản vẽ không vừa khổ chuẩn.</td></tr>') + '</tbody></table></div>' +
          '<div class="hrow"><button class="btn sm" type="button" data-pm="addsz"' + (c.miss ? ' disabled' : '') + '>' + ic('plus') + 'Thêm khổ giấy…</button><span class="faint" style="font-size:var(--fs-xs)">Khổ tùy chỉnh lưu trong tệp PC3' + (c.pmp ? ' và ' + esc(c.pmp.file) : '') + '.</span></div></div>';
      if (k === 'area') return '<div class="stack"><span class="faint" style="font-size:var(--fs-s)">Lề của driver không đúng thực tế (nét sát mép bị cắt): sửa vùng in được của từng khổ chuẩn.</span>' + sizeTbl('area') +
          (sel && sel.std ? '<div class="pc3-pvw"><canvas id="pm-pv" class="pc3-cv" width="220" height="150" aria-label="Xem trước vùng in được"></canvas><div class="stack" style="gap:6px"><b>' + esc(sel.name) + '</b><div class="pc3-mg">' + ['Trên', 'Dưới', 'Trái', 'Phải'].map(function(l, i){ return '<span class="lab">' + l + '</span>' + nf({ id:'pm-m' + i, value:TC.d(sel.mg[i], 1), label:'Lề ' + l.toLowerCase(), pick:false, prec:1, unit:'none', style:'width:84px' }); }).join('') + '</div><button class="btn sm ghost" type="button" data-pm="mgreset"' + (sel.edited ? '' : ' disabled') + ' style="justify-self:start">Về lề của driver</button></div></div>' : '') + '</div>';
      if (k === 'pmp') return '<div class="stack">' + (c.pmp ? '<div class="pc3-card">' + ic('ruler', 'md') + '<span class="grow" style="display:grid;gap:2px"><b class="mono">' + esc(c.pmp.file) + '</b><span class="faint" style="font-size:var(--fs-s)">Hiệu chỉnh ngày ' + esc(c.pmp.date) + ' · chiều dài ' + (c.pmp.dx > 0 ? '+' : '') + fx(c.pmp.dx, 2) + ' %, chiều rộng ' + (c.pmp.dy > 0 ? '+' : '') + fx(c.pmp.dy, 2) + ' %</span></span></div>' : '<div class="note">' + ic('info') + '<span>Chưa hiệu chỉnh. Máy in kéo giấy có thể in dài hơn hoặc ngắn hơn vài phần nghìn; hiệu chỉnh để bản in đo đúng tỉ lệ.</span></div>') +
          '<div class="hrow"><button class="btn sm' + (c.pmp ? '' : ' primary') + '" type="button" data-pm="cal"' + (c.miss || c.kind !== 'sys' ? ' disabled' : '') + '>' + ic('ruler') + 'Hiệu chỉnh máy in…</button>' + (c.pmp ? '<button class="btn sm ghost" type="button" data-pm="pmpx">Gỡ tệp PMP</button>' : '') + '</div>' + (c.kind !== 'sys' ? '<span class="faint" style="font-size:var(--fs-xs)">PDF, PNG luôn đúng tỉ lệ: không cần hiệu chỉnh.</span>' : '') + '</div>';
      if (k === 'filt'){ var nOn = S.filter(function(s){ return !c.hide[s.name]; }).length; return '<div class="stack"><span class="faint" style="font-size:var(--fs-s)">Chỉ khổ được đánh dấu hiện trong danh sách khổ giấy của hộp In và Thiết lập trang. Đang hiện ' + nOn + '/' + S.length + ' khổ.</span>' + sizeTbl('filt') + '</div>'; }
      return '<div class="stack"><span style="font-size:var(--fs-s)">' + (c.kind === 'sys' ? 'Mục riêng của driver (chất lượng in, tiết kiệm mực, khay ra giấy) nằm trong hộp thoại của Windows.' : 'Trình điều khiển tích hợp của TrionCAD không có mục riêng: mọi thiết lập ở các mục bên trái.') + '</span>' + (c.kind === 'sys' ? '<button class="btn sm" type="button" data-pm="winprop" style="justify-self:start"' + (c.miss ? ' disabled' : '') + '>' + ic('ext') + 'Mở thuộc tính driver của Windows…</button>' : '') + '</div>';
    }
    function render(){
      if (!document.contains(dlg.el)) return; var c = cur(), F = TC.fold(PM.q), n = dirty(), fa = document.activeElement, fid = fa && dlg.el.contains(fa) ? (fa.id ? '#' + fa.id : fa.name ? '[name="' + fa.name + '"][value="' + fa.value + '"]' : fa.dataset.pg ? '[data-pg="' + fa.dataset.pg + '"]' : fa.dataset.pf ? '[data-pf="' + fa.dataset.pf + '"]' : fa.dataset.phide ? '[data-phide="' + CSS.escape(fa.dataset.phide) + '"]' : null) : null; // giữ con trỏ sau khi vẽ lại
      var items = W.map(function(x, i){ if (F && TC.fold(x.name + ' ' + x.drv).indexOf(F) < 0) return ''; var on = i === PM.sel; return '<button class="pc3-i' + (on ? ' is-sel' : '') + '" type="button" role="option" aria-selected="' + on + '" tabindex="' + (on ? 0 : -1) + '" data-pi="' + i + '">' + ic(pcIcon(x)) + '<span class="grow" style="display:grid;min-width:0"><b class="ell">' + esc(x.name) + '</b><span class="faint ell">' + esc(x.miss ? 'Thiếu driver' : x.kind === 'sys' ? x.port.n : 'Tích hợp · in ra tệp') + '</span></span>' + (changed(i) ? '<span class="tag warn">Đã sửa</span>' : x.miss ? '<span class="tag danger">Lỗi</span>' : x.def ? '<span class="tag acc">Mặc định</span>' : '') + '</button>'; }).join('');
      $d('#pm-b').innerHTML = '<div class="pc3-l"><div class="pal-tools"><label class="search grow">' + ic('search') + '<input id="pm-q" placeholder="Tìm máy in" value="' + esc(PM.q) + '" aria-label="Tìm máy in"></label></div><div class="pc3-list" role="listbox" aria-label="Cấu hình máy in">' + (items || '<div class="empty">Không có máy in khớp “' + esc(PM.q) + '”.</div>') + '</div>' +
        '<div class="pal-tools pc3-lf"><button class="btn sm" type="button" data-pm="add" aria-haspopup="menu">' + ic('plus') + 'Thêm máy in' + ic('down', 'xs') + '</button><span class="grow"></span><button class="ib" type="button" data-pm="dir" aria-label="Mở thư mục Plotters" data-tip="Mở thư mục Plotters">' + ic('folder') + '</button></div></div>' +
        '<div class="pc3-r">' + head(c) + '<div class="tabs" role="tablist">' + [['gen', 'Chung'], ['port', 'Cổng'], ['dev', 'Thiết lập thiết bị']].map(function(t){ return '<button class="tab" role="tab" type="button" data-pt="' + t[0] + '" aria-selected="' + (PM.tab === t[0]) + '">' + t[1] + '</button>'; }).join('') + '</div>' +
        '<div class="pc3-p">' + (PM.tab === 'gen' ? genPane(c) : PM.tab === 'port' ? portPane(c) : '<div class="pc3-dev"><div class="tree pc3-tree" role="tree" aria-label="Thiết lập thiết bị">' + PNODES.map(function(g){ return '<div class="gh pc3-tg">' + esc(g[0]) + '</div>' + g[1].map(function(nd){ var badge = nd[0] === 'cust' && c.custom.length ? c.custom.length : nd[0] === 'pmp' && c.pmp ? '✓' : nd[0] === 'area' && Object.keys(c.area).length ? Object.keys(c.area).length : nd[0] === 'filt' && Object.keys(c.hide).length ? '−' + Object.keys(c.hide).length : ''; return '<button class="tn" type="button" role="treeitem" data-pn="' + nd[0] + '" aria-selected="' + (PM.node === nd[0]) + '"><span>' + nd[1] + '</span><span class="cnt">' + badge + '</span></button>'; }).join(''); }).join('') + '</div><div class="pc3-dd">' + nodePane(c) + '</div></div>') + '</div></div>';
      var cv = $d('#pm-pv'), sz = sizes(c).filter(function(s){ return s.name === PM.size; })[0]; if (cv && sz) paperPv(cv, sz.w, sz.h, sz.mg);
      $('.dlg-f', dlg.el).innerHTML = '<button class="btn" type="button" data-pm="saveas"' + (c.miss ? ' disabled' : '') + '>' + ic('saveas') + 'Lưu thành…</button><button class="btn ghost" type="button" data-pm="reset"' + (changed(PM.sel) && base[PM.sel] ? '' : ' disabled') + '>Bỏ sửa cấu hình này</button><span class="grow"></span>' +
        (n ? '<span class="faint" style="font-size:var(--fs-s)">' + n + ' cấu hình đã sửa</span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" data-pm="save">' + ic('save') + 'Lưu ' + n + ' cấu hình</button>' : '<button class="btn primary" type="button" data-x="close">Đóng</button>');
      if (fid && fid !== '#pm-q'){ var fn = $d(fid); if (fn) fn.focus(); }
    }
    function save(){
      var n = dirty(), c0 = W.filter(function(c, i){ return changed(i); }).map(function(c){ return c.name; });
      PCS.splice.apply(PCS, [0, PCS.length].concat(W.map(clone))); base = PCS.map(clone);
      // hộp In đang mở: thêm khổ tùy chỉnh mới của máy in đang chọn vào danh sách khổ giấy
      var pd = $('#pl-dev'), pp = $('#pl-paper'), dv = pd && W.filter(function(c){ return c.name === pd.value; })[0], add = 0;
      if (dv && pp) dv.custom.forEach(function(s){ var nm = s[0] + ' (' + s[1] + ' × ' + s[2] + ' mm)'; if (!SC.PAPERS.some(function(q){ return q[0] === nm; })){ SC.PAPERS.push([nm, Math.max(s[1], s[2]), Math.min(s[1], s[2])]); pp.insertAdjacentHTML('beforeend', '<option>' + esc(nm) + '</option>'); add++; } });
      dlg.close(true); TC.toast('Đã lưu ' + n + ' cấu hình máy in: ' + c0.map(esc).join(', ') + '.' + (add ? ' Hộp In có thêm ' + add + ' khổ giấy tùy chỉnh.' : ''), { log:false });
    }
    function addPrinter(b){ var r = b.getBoundingClientRect(), mk = function(o){ W.push(pc(o)); PM.sel = W.length - 1; PM.tab = 'gen'; PM.size = null; render(); };
      TC.menu([{ h:'Máy in của Windows chưa có cấu hình' }, { label:'Brother HL-L2350DW <span class="faint">· USB002</span>', icon:'print', act:function(){ mk({ name:'Brother HL-L2350DW', file:'Brother HL-L2350DW.pc3', drv:'Brother HL-L2350DW series · máy in Windows', port:{ m:'port', n:'USB001' }, src:'Tự chọn khay', std:'office', mg:[4.2, 4.2, 4.2, 4.2] }); } }, { label:'Epson L3150 <span class="faint">· mạng</span>', icon:'print', act:function(){ mk({ name:'Epson L3150', file:'Epson L3150.pc3', drv:'EPSON L3150 Series · máy in Windows', port:{ m:'port', n:'192.168.1.52:9100' }, src:'Tự chọn khay', std:'office', mg:[3, 3, 3, 3] }); } },
        { sep:1 }, { h:'Trình điều khiển của TrionCAD' }, { label:'PDF (khổ, độ phân giải riêng)', icon:'pdf', act:function(){ var k = 1; while (W.some(function(c){ return c.name === 'PDF ' + k; })) k++; mk({ name:'PDF ' + k, file:'PDF ' + k + '.pc3', kind:'pdf', drv:'TrionCAD PDF 1.0 (tích hợp)', port:{ m:'file', n:'D:\\DuAn\\NhaO-2T\\PDF' } }); } }, { label:'Ảnh PNG', icon:'image', act:function(){ mk({ name:'PNG 600 DPI', file:'PNG 600 DPI.pc3', kind:'img', drv:'TrionCAD Raster 1.0 (tích hợp)', port:{ m:'file', n:'D:\\DuAn\\NhaO-2T\\PNG' }, gfx:{ depth:'Màu thật (24 bit)', dpi:600, ras:'Cao', ttf:'shape', merge:'over', dither:false } }); } }], r.left, r.top, { up:true, returnFocus:b, minWidth:280 }); }
    dlg.el.addEventListener('click', function(e){
      var c = cur(), it = e.target.closest('[data-pi]'); if (it){ PM.sel = Number(it.dataset.pi); PM.size = null; render(); var f = $d('.pc3-i.is-sel'); if (f) f.focus(); return; }
      var t = e.target.closest('[data-pt]'); if (t){ PM.tab = t.dataset.pt; render(); var f2 = $d('[data-pt="' + PM.tab + '"]'); if (f2) f2.focus(); return; }
      var nd = e.target.closest('[data-pn]'); if (nd){ PM.node = nd.dataset.pn; render(); var f3 = $d('[data-pn="' + PM.node + '"]'); if (f3) f3.focus(); return; }
      var pt = e.target.closest('[data-pport]'); if (pt && c.kind === 'sys'){ c.port = { m:'port', n:pt.dataset.pport }; render(); return; }
      var sz = e.target.closest('[data-psz]'); if (sz && !e.target.closest('input')){ PM.size = sz.dataset.psz; render(); return; }
      var ce = e.target.closest('[data-pce]'); if (ce){ sizeWizard(c, Number(ce.dataset.pce), render); return; }
      var cx = e.target.closest('[data-pcx]'); if (cx){ var i = Number(cx.dataset.pcx), s = c.custom[i]; TC.confirm({ title:'Xóa khổ giấy tùy chỉnh', msg:'Xóa khổ <b>' + esc(s[0]) + '</b> (' + s[1] + ' × ' + s[2] + ' mm) khỏi ' + esc(c.name) + '?', detail:'Thiết lập trang đang dùng khổ này sẽ về khổ gần nhất khi in. Có hiệu lực khi bấm Lưu.', ok:'Xóa 1 khổ giấy', danger:true }).then(function(y){ if (y){ c.custom.splice(i, 1); render(); } }); return; }
      var b = e.target.closest('[data-pm]'); if (!b || b.disabled) return; var k = b.dataset.pm;
      if (k === 'save') save();
      else if (k === 'add') addPrinter(b);
      else if (k === 'dir') TC.toast('Mở <span class="mono">' + esc(PDIR) + '</span> trong File Explorer.', { type:'info', log:false });
      else if (k === 'def'){ W.forEach(function(x){ x.def = x === c; }); render(); }
      else if (k === 'winprop') TC.toast('Mở hộp thoại thuộc tính của driver ' + esc(c.drv.split(' · ')[0]) + ' (Windows).', { type:'info', log:false });
      else if (k === 'drvpick'){ var r = b.getBoundingClientRect(); TC.menu([{ h:'Driver có trên máy' }].concat(['Ricoh Generic PCL6 V4.21', 'Microsoft PCL6 Class Driver', 'Canon Generic Plus UFR II 2.80'].map(function(nm){ return { label:esc(nm), icon:'print', act:function(){ c.drv = nm + ' · máy in Windows'; c.miss = false; render(); } }; })), r.left, r.bottom + 2, { returnFocus:b, minWidth:260 }); }
      else if (k === 'del') TC.confirm({ title:'Xóa cấu hình máy in', msg:'Xóa <b>' + esc(c.file) + '</b>?', detail:c.used + ' thiết lập trang đang trỏ tới máy in này sẽ báo “Không tìm thấy máy in” khi in. Có hiệu lực khi bấm Lưu.', ok:'Xóa 1 cấu hình', danger:true }).then(function(y){ if (!y) return; W.splice(PM.sel, 1); base.splice(PM.sel, 1); base.push(null); PM.sel = 0; render(); });
      else if (k === 'netport') TC.toast('Thêm cổng mạng: nhập tên máy chủ hoặc địa chỉ IP (hộp thoại của Windows).', { type:'info', log:false });
      else if (k === 'cfgport') TC.toast('Cấu hình cổng ' + esc(c.port.n) + ' (hộp thoại của Windows).', { type:'info', log:false });
      else if (k === 'browse') TC.toast('Hộp thoại chọn thư mục của Windows.', { type:'info', log:false });
      else if (k === 'addsz') sizeWizard(c, -1, render);
      else if (k === 'cal') pmpWizard(c, render);
      else if (k === 'pmpx') TC.confirm({ title:'Gỡ tệp hiệu chỉnh', msg:'Gỡ <b>' + esc(c.pmp.file) + '</b> khỏi ' + esc(c.name) + '?', detail:'Bản in dùng lại tỉ lệ của driver. Tệp PMP vẫn ở thư mục Plotters.', ok:'Gỡ tệp PMP', danger:true }).then(function(y){ if (y){ c.pmp = null; render(); } });
      else if (k === 'mgreset'){ delete c.area[PM.size]; render(); }
      else if (k === 'reset'){ W[PM.sel] = clone(base[PM.sel]); render(); }
      else if (k === 'saveas') SC.ask('Lưu cấu hình thành', 'Tên tệp PC3 mới', c.name + ' (bản sao)', function(v){ return W.some(function(x){ return x.name.toLowerCase() === v.toLowerCase(); }) ? 'Đã có cấu hình tên này.' : /[\\/:*?"<>|]/.test(v) ? 'Tên không chứa \\ / : * ? " < > |' : null; }, 'UI-H10').then(function(v){ if (!v) return; var n = clone(c); n.name = v; n.file = TC.fold(v).replace(/[^\w\s-]+/g, '') + '.pc3'; n.def = false; n.from = null; n.used = 0; W.push(n); PM.sel = W.length - 1; render(); });
    });
    dlg.el.addEventListener('change', function(e){
      var c = cur(), t = e.target;
      if (t.dataset.pf === 'src' || t.dataset.pf === 'media') c[t.dataset.pf] = t.value;
      else if (t.dataset.pf === 'filedir') c.port.n = t.value;
      else if (t.dataset.pg) c.gfx[t.dataset.pg] = t.dataset.pg === 'dpi' ? Number(t.value) : t.value;
      else if (t.id === 'pm-dith') c.gfx.dither = t.checked;
      else if (t.name === 'pm-ttf') c.gfx.ttf = t.value;
      else if (t.name === 'pm-mg') c.gfx.merge = t.value;
      else if (t.name === 'pm-pm') c.port = t.value === 'file' ? { m:'file', n:'D:\\DuAn\\NhaO-2T\\PDF' } : { m:'port', n:(PORTS[0] || [''])[0] };
      else if (t.id === 'pm-allp') PM.allPorts = t.checked;
      else if (t.dataset.phide){ if (t.checked) delete c.hide[t.dataset.phide]; else c.hide[t.dataset.phide] = true; }
      else return;
      render();
    });
    dlg.el.addEventListener('input', function(e){ var c = cur(), t = e.target; if (t.id === 'pm-q'){ PM.q = t.value; render(); var q = $d('#pm-q'); q.focus(); q.setSelectionRange(q.value.length, q.value.length); } else if (t.dataset.pf === 'name' || t.dataset.pf === 'desc'){ c[t.dataset.pf] = t.value; var it = $d('.pc3-i.is-sel b'); if (it && t.dataset.pf === 'name') it.textContent = t.value; } });
    dlg.el.addEventListener('focusout', function(e){ var t = e.target; if (t.dataset && t.dataset.pf === 'name'){ var c = cur(), v = t.value.trim(); if (!v){ c.name = base[PM.sel] ? base[PM.sel].name : 'Máy in'; } else c.file = TC.fold(v).replace(/[^\w\s-]+/g, '').trim() + '.pc3'; setTimeout(render, 0); } });
    dlg.el.addEventListener('numchange', function(e){ var c = cur(), m = /^pm-m(\d)$/.exec(e.target.id || (e.target.querySelector && e.target.querySelector('input') || {}).id || ''); if (!m) return; var sz = sizes(c).filter(function(s){ return s.name === PM.size; })[0]; if (!sz) return; var mg = sz.mg.slice(); mg[Number(m[1])] = TC.clamp(e.detail, 0, 50); c.area[PM.size] = mg; setTimeout(function(){ var a = document.activeElement, id = a && a.id; render(); var n = id && $d('#' + id); if (n) n.focus(); }, 0); });
    // ↑↓ trong danh sách máy in (chỉ khi con trỏ đang ở danh sách)
    dlg.el.addEventListener('keydown', function(e){ var it = e.target.closest && e.target.closest('.pc3-i'); if (!it || (e.key !== 'ArrowDown' && e.key !== 'ArrowUp')) return; e.preventDefault(); var all = $$('.pc3-i', dlg.el); all[TC.clamp(all.indexOf(it) + (e.key === 'ArrowDown' ? 1 : -1), 0, all.length - 1)].click(); });
    render();
    return dlg;
  };
  // trình tạo khổ giấy tùy chỉnh theo bước; i ≥ 0: sửa khổ có sẵn
  function sizeWizard(c, i, done){
    var ST = ['Bắt đầu', 'Kích thước', 'Vùng in được', 'Tên khổ', 'Xong'], s0 = i >= 0 ? c.custom[i] : null, std = sizes(c).filter(function(s){ return s.std; });
    var st = { k:i >= 0 ? 1 : 0, from:'new', base:std[1] ? std[1].name : std[0].name, w:s0 ? s0[1] : 841, h:s0 ? s0[2] : 1500, mg:s0 ? s0[3].slice() : c.mg.slice(), name:s0 ? s0[0] : 'Khổ tùy chỉnh ' + (c.custom.length + 1), test:false };
    var dlg = TC.dialog({ id:'pmsz', title:(i >= 0 ? 'Sửa' : 'Thêm') + ' khổ giấy tùy chỉnh · ' + esc(c.name), cmd:'PLOTTERMANAGER', code:'UI-H10', w:680, h:470, flush:true, body:'<div class="wz"><ol class="wz-steps" id="sz-s" aria-label="Các bước"></ol><div class="wz-body" id="sz-b"></div></div>', foot:'<span class="fmsg" id="sz-m"></span><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn" type="button" id="sz-back">Quay lại</button><button class="btn primary" type="button" id="sz-next"></button>' });
    var $d = function(s){ return $(s, dlg.el); };
    function err(){ if (st.k === 1){ if (!(st.w > 0) || !(st.h > 0)) return 'Rộng và cao phải lớn hơn 0.'; if (c.roll && Math.min(st.w, st.h) > c.roll) return 'Một cạnh phải không quá ' + c.roll + ' mm (bề rộng cuộn giấy).'; if (Math.max(st.w, st.h) > 15000) return 'Chiều dài tối đa 15 000 mm.'; }
      if (st.k === 2 && (st.mg[0] + st.mg[1] >= st.h || st.mg[2] + st.mg[3] >= st.w)) return 'Lề lớn hơn khổ giấy: giảm lề.';
      if (st.k === 3){ var nm = st.name.trim(); if (!nm) return 'Nhập tên khổ giấy.'; if (sizes(c).some(function(x, j){ return x.name.toLowerCase() === nm.toLowerCase() && !(i >= 0 && !x.std && x.name === s0[0]); })) return 'Máy in đã có khổ tên này.'; } return ''; }
    function render(){
      $d('#sz-s').innerHTML = steps(ST, st.k); var B = '';
      if (st.k === 0) B = '<div class="stack">' + H.rad('sz-f', 'new', 'Tạo khổ mới', st.from === 'new') + H.rad('sz-f', 'base', 'Dựa trên khổ có sẵn', st.from === 'base') + '<div style="padding-left:20px"><select class="inp" id="sz-base" style="width:220px"' + (st.from === 'base' ? '' : ' disabled') + '>' + SC.opts(std.map(function(s){ return s.name; }), st.base) + '</select></div><p class="faint" style="font-size:var(--fs-s);margin:0">Khổ tùy chỉnh dùng cho bản vẽ dài (mặt đứng, mặt cắt địa hình) hoặc in hết bề rộng cuộn giấy.</p></div>';
      if (st.k === 1) B = '<div class="wz-cols"><div class="stack"><div class="frow" style="--lab-w:70px"><span class="lab">Rộng</span>' + nf({ id:'sz-w', value:TC.d(st.w, 1), label:'Rộng', pick:false, prec:1, unit:'len', style:'width:120px' }) + '<span class="lab">Cao</span>' + nf({ id:'sz-h', value:TC.d(st.h, 1), label:'Cao', pick:false, prec:1, unit:'len', style:'width:120px' }) + '<span class="lab">Đơn vị</span><span>mm</span></div>' + (c.roll ? '<span class="faint" style="font-size:var(--fs-xs)">Cuộn giấy rộng ' + c.roll + ' mm: một cạnh không quá ' + c.roll + ' mm.</span>' : '') + '</div><canvas id="sz-pv" class="pc3-cv" width="240" height="190" aria-label="Xem trước khổ giấy"></canvas></div>';
      if (st.k === 2) B = '<div class="wz-cols"><div class="stack"><span class="faint" style="font-size:var(--fs-s)">Vùng in được là phần máy in vẽ tới. Lề quá nhỏ làm mất nét sát mép.</span><div class="pc3-mg">' + ['Trên', 'Dưới', 'Trái', 'Phải'].map(function(l, j){ return '<span class="lab">' + l + '</span>' + nf({ id:'sz-m' + j, value:TC.d(st.mg[j], 1), label:'Lề ' + l.toLowerCase(), pick:false, prec:1, unit:'none', style:'width:96px' }); }).join('') + '</div><span class="mono faint" id="sz-pa" style="font-size:var(--fs-s)">Vùng in được ' + fx(st.w - st.mg[2] - st.mg[3], 1) + ' × ' + fx(st.h - st.mg[0] - st.mg[1], 1) + ' mm</span></div><canvas id="sz-pv" class="pc3-cv" width="240" height="190" aria-label="Xem trước vùng in được"></canvas></div>';
      if (st.k === 3) B = '<div class="stack">' + H.row('Tên khổ', '<input class="inp" id="sz-n" value="' + esc(st.name) + '" style="width:280px" spellcheck="false">', 90) + H.row('Hiện trong', '<span>Hộp In, Thiết lập trang · danh sách khổ của ' + esc(c.name) + '</span>', 90) + H.row('Lưu trong', '<span class="mono" style="font-size:var(--fs-xs)">' + esc(c.file) + (c.pmp ? ' + ' + esc(c.pmp.file) : '') + '</span>', 90) + '</div>';
      if (st.k === 4) B = '<div class="stack"><div class="wz-found"><b>' + esc(st.name) + '</b><span>' + fx(st.w, 0) + ' × ' + fx(st.h, 0) + ' mm · vùng in được ' + fx(st.w - st.mg[2] - st.mg[3], 1) + ' × ' + fx(st.h - st.mg[0] - st.mg[1], 1) + ' mm</span></div><span class="faint" style="font-size:var(--fs-s)">Khổ có hiệu lực khi bấm Lưu ở Quản lý máy in.</span><div class="hrow"><button class="btn" type="button" id="sz-test"' + (c.kind !== 'sys' || st.test === 'run' ? ' disabled' : '') + '>' + ic('print') + 'In trang thử</button><span class="faint" id="sz-tm" style="font-size:var(--fs-s)">' + (st.test === 'done' ? 'Đã gửi trang thử tới ' + esc(c.name) + '.' : st.test === 'run' ? 'Đang gửi…' : c.kind === 'sys' ? 'Một tờ có khung vùng in được và thước đo ở bốn cạnh.' : '') + '</span></div></div>';
      $d('#sz-b').innerHTML = B; $d('#sz-back').disabled = st.k === 0 || (i >= 0 && st.k === 1); $d('#sz-next').textContent = st.k === 4 ? (i >= 0 ? 'Lưu khổ giấy' : 'Thêm khổ giấy') : 'Tiếp'; fmsg($d('#sz-m'), '');
      var cv = $d('#sz-pv'); if (cv) paperPv(cv, st.w, st.h, st.k === 2 ? st.mg : [0, 0, 0, 0]);
      var f = $d('#sz-b input:not([disabled]), #sz-b select:not([disabled])'); if (f) setTimeout(function(){ f.focus(); }, 0);
    }
    dlg.el.addEventListener('change', function(e){ var t = e.target; if (t.name === 'sz-f'){ st.from = t.value; render(); } else if (t.id === 'sz-base'){ st.base = t.value; } });
    dlg.el.addEventListener('input', function(e){ if (e.target.id === 'sz-n') st.name = e.target.value; });
    dlg.el.addEventListener('numchange', function(e){ var id = (e.target.querySelector && e.target.querySelector('input') || e.target).id; if (id === 'sz-w') st.w = e.detail; else if (id === 'sz-h') st.h = e.detail; else if (/^sz-m\d$/.test(id)) st.mg[Number(id.slice(-1))] = Math.max(0, e.detail); var cv = $d('#sz-pv'); if (cv) paperPv(cv, st.w, st.h, st.k === 2 ? st.mg : [0, 0, 0, 0]); var pa = $d('#sz-pa'); if (pa) pa.textContent = 'Vùng in được ' + fx(st.w - st.mg[2] - st.mg[3], 1) + ' × ' + fx(st.h - st.mg[0] - st.mg[1], 1) + ' mm'; fmsg($d('#sz-m'), err()); });
    dlg.el.addEventListener('click', function(e){ if (!e.target.closest('#sz-test')) return; st.test = 'run'; render(); setTimeout(function(){ if (!document.contains(dlg.el)) return; st.test = 'done'; render(); TC.toast('Đã gửi trang thử khổ ' + esc(st.name) + ' tới ' + esc(c.name) + '.', { log:false }); }, 900); });
    $d('#sz-back').addEventListener('click', function(){ st.k = Math.max(0, st.k - 1); render(); });
    $d('#sz-next').addEventListener('click', function(){
      $$('[data-num] input', dlg.el).forEach(function(inp){ TC.numCommit(inp.closest('[data-num]')); });
      var e = err(); if (e){ fmsg($d('#sz-m'), e); return; }
      if (st.k === 0 && st.from === 'base'){ var b = std.filter(function(s){ return s.name === st.base; })[0]; st.w = b.w; st.h = b.h; st.mg = b.mg.slice(); st.name = b.name + ' (sửa)'; }
      if (st.k < 4){ st.k++; render(); return; }
      var rec = [st.name.trim(), Math.round(st.w * 10) / 10, Math.round(st.h * 10) / 10, st.mg.slice()]; if (i >= 0) c.custom[i] = rec; else c.custom.push(rec);
      PM.size = rec[0]; dlg.close(true); done();
    });
    render();
  }
  // hiệu chỉnh tỉ lệ máy in (PMP): in hình chữ nhật đã biết kích thước, đo, nhập; hệ số bù lưu trong tệp PMP
  function pmpWizard(c, done){
    var ST = ['Bắt đầu', 'Hình hiệu chỉnh', 'In tờ hiệu chỉnh', 'Đo và nhập', 'Lưu PMP'], st = { k:0, L:400, B:250, printed:false, run:false, had:false, mL:null, mB:null, file:TC.fold(c.name).replace(/[^\w]+/g, '-').replace(/^-|-$/g, '') + '.pmp', check:true };
    var dlg = TC.dialog({ id:'pmpw', title:'Hiệu chỉnh máy in · ' + esc(c.name), cmd:'PLOTTERMANAGER', code:'UI-H10', w:680, h:470, flush:true, body:'<div class="wz"><ol class="wz-steps" id="pw-s" aria-label="Các bước"></ol><div class="wz-body" id="pw-b"></div></div>', foot:'<span class="fmsg" id="pw-m"></span><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn" type="button" id="pw-back">Quay lại</button><button class="btn primary" type="button" id="pw-next"></button>' });
    var $d = function(s){ return $(s, dlg.el); };
    function dev(m, n){ return m ? (m - n) / n * 100 : null; }
    function rect(cv){ var x = cv.getContext('2d'), W = cv.width, Hh = cv.height, s = Math.min((W - 60) / st.L, (Hh - 50) / st.B), x0 = (W - st.L * s) / 2, y0 = (Hh - st.B * s) / 2 - 4; x.clearRect(0, 0, W, Hh); x.fillStyle = TC.cssv('--c-paper'); x.fillRect(8, 8, W - 16, Hh - 16); x.strokeStyle = TC.cssv('--c-paper-print'); x.lineWidth = 1.5; x.strokeRect(x0, y0, st.L * s, st.B * s);
      x.fillStyle = TC.cssv('--c-paper-print'); x.font = '11px ' + TC.cssv('--f-mono'); x.textAlign = 'center'; x.fillText('Dài ' + st.L + ' mm', W / 2, y0 + st.B * s + 16); x.save(); x.translate(x0 - 10, Hh / 2); x.rotate(-Math.PI / 2); x.fillText('Rộng ' + st.B + ' mm', 0, 0); x.restore(); }
    function err(){ if (st.k === 1 && (!(st.L >= 50) || !(st.B >= 50))) return 'Cạnh hình hiệu chỉnh tối thiểu 50 mm.'; if (st.k === 2 && !st.printed && !st.had) return 'In tờ hiệu chỉnh trước, hoặc đánh dấu đã có tờ in.';
      if (st.k === 3){ if (!st.mL || !st.mB) return 'Nhập chiều dài và chiều rộng đo được.'; if (Math.abs(dev(st.mL, st.L)) > 5 || Math.abs(dev(st.mB, st.B)) > 5) return 'Sai lệch trên 5 %: đo lại, hoặc kiểm tra tờ đã in đúng 100 % (không “vừa khổ giấy”).'; }
      if (st.k === 4 && !/^[\w.-]+\.pmp$/i.test(st.file.trim())) return 'Tên tệp PMP gồm chữ không dấu, số, dấu gạch và đuôi .pmp.'; return ''; }
    function render(){
      $d('#pw-s').innerHTML = steps(ST, st.k); var B = '', dL = dev(st.mL, st.L), dB = dev(st.mB, st.B);
      if (st.k === 0) B = '<div class="stack"><p style="margin:0">Máy in kéo giấy (nhất là máy khổ lớn) có thể in dài hơn hoặc ngắn hơn vài phần nghìn. TrionCAD in một hình chữ nhật đã biết kích thước; bạn đo bằng thước thép rồi nhập số đo, TrionCAD bù sai lệch cho mọi bản in sau.</p><div class="note">' + ic('info') + '<span>Dùng thước có vạch mm, đo mép ngoài nét. Đặt tờ in trên mặt phẳng, để giấy ổn định nhiệt độ vài phút.</span></div>' + (c.pmp ? '<div class="note warn">' + ic('warn') + '<span>Đang có hiệu chỉnh ' + esc(c.pmp.file) + ' (' + esc(c.pmp.date) + '). Hiệu chỉnh mới sẽ thay thế.</span></div>' : '') + '</div>';
      if (st.k === 1) B = '<div class="wz-cols"><div class="stack"><div class="frow" style="--lab-w:70px"><span class="lab">Dài</span>' + nf({ id:'pw-l', value:TC.d(st.L, 0), label:'Dài', pick:false, prec:0, unit:'len', style:'width:110px' }) + '<span class="lab">Rộng</span>' + nf({ id:'pw-w', value:TC.d(st.B, 0), label:'Rộng', pick:false, prec:0, unit:'len', style:'width:110px' }) + '</div><span class="faint" style="font-size:var(--fs-xs)">Hình càng lớn, phép đo càng chính xác. 400 × 250 mm vừa khổ A3 và cuộn giấy.</span></div><canvas id="pw-pv" class="pc3-cv" width="240" height="170" aria-label="Hình chữ nhật hiệu chỉnh"></canvas></div>';
      if (st.k === 2) B = '<div class="stack"><div class="hrow"><button class="btn' + (st.printed ? '' : ' primary') + '" type="button" id="pw-print"' + (st.run ? ' disabled' : '') + '>' + ic('print') + (st.printed ? 'In lại tờ hiệu chỉnh' : 'In tờ hiệu chỉnh') + '</button>' + (st.run ? '<span class="spin"></span><span class="faint">Đang gửi tới ' + esc(c.name) + '…</span>' : st.printed ? '<span class="ok">' + ic('okc', 's12') + '</span><span>Đã in hình ' + st.L + ' × ' + st.B + ' mm, tỉ lệ 1:1, ' + esc(c.src || 'khổ mặc định') + '.</span>' : '') + '</div>' + H.chk('pw-had', 'Đã có tờ hiệu chỉnh in trước đó với cùng kích thước', st.had) + '<span class="faint" style="font-size:var(--fs-s)">Tờ hiệu chỉnh in đúng 100 %, không co theo khổ giấy, không dùng hiệu chỉnh cũ.</span></div>';
      if (st.k === 3) B = '<div class="stack"><div class="frow pw-meas" style="--lab-w:120px"><span class="lab">Dài đo được</span><span class="hrow">' + TC.numField({ id:'pw-ml', value:st.mL ? TC.d(st.mL, 1) : '', label:'Dài đo được', pick:false, prec:1, unit:'len', style:'width:110px' }) + '<span class="faint">thiết kế ' + st.L + ' mm</span><b class="mono" id="pw-dl"></b></span><span class="lab">Rộng đo được</span><span class="hrow">' + TC.numField({ id:'pw-mw', value:st.mB ? TC.d(st.mB, 1) : '', label:'Rộng đo được', pick:false, prec:1, unit:'len', style:'width:110px' }) + '<span class="faint">thiết kế ' + st.B + ' mm</span><b class="mono" id="pw-db"></b></span></div><div id="pw-note"></div></div>';
        (dL != null && dB != null && Math.abs(dL) <= 5 && Math.abs(dB) <= 5 ? '<div class="note acc">' + ic('ruler') + '<span>Hệ số bù: chiều dài <b class="mono">' + fx(st.L / st.mL, 5) + '</b>, chiều rộng <b class="mono">' + fx(st.B / st.mB, 5) + '</b>. Bản in 1:100 dài 40 m sẽ lệch ' + fx(Math.abs(dL) * 400 / 100 * 10, 0) + ' mm nếu không hiệu chỉnh.</span></div>' : '') + '</div>';
      if (st.k === 4) B = '<div class="stack">' + H.row('Tên tệp PMP', '<input class="inp mono" id="pw-f" value="' + esc(st.file) + '" style="width:280px" spellcheck="false">', 110) + H.row('Gắn vào', '<span>' + esc(c.file) + '</span>', 110) + H.row('', H.chk('pw-chk', 'In tờ kiểm tra sau khi lưu', st.check), 110) + '<div class="wz-found"><b>' + (dL > 0 ? '+' : '') + fx(dL, 2) + ' % · ' + (dB > 0 ? '+' : '') + fx(dB, 2) + ' %</b><span>sai lệch dài, rộng được bù</span></div></div>';
      $d('#pw-b').innerHTML = B; $d('#pw-back').disabled = st.k === 0; $d('#pw-next').textContent = st.k === 4 ? 'Lưu hiệu chỉnh' : 'Tiếp'; fmsg($d('#pw-m'), '');
      var cv = $d('#pw-pv'); if (cv) rect(cv); if (st.k === 3) meas();
    }
    // sai lệch, hệ số bù: cập nhật tại chỗ để Tab giữa hai ô đo không mất con trỏ
    function meas(){ var dL = dev(st.mL, st.L), dB = dev(st.mB, st.B), sg = function(v){ return v == null ? '' : (v > 0 ? '+' : '') + fx(v, 2) + ' %'; }, ok = dL != null && dB != null && Math.abs(dL) <= 5 && Math.abs(dB) <= 5;
      [['#pw-dl', dL], ['#pw-db', dB]].forEach(function(z){ var e = $d(z[0]); if (e){ e.textContent = sg(z[1]); e.className = 'mono' + (z[1] != null && Math.abs(z[1]) > 5 ? ' danger' : ''); } });
      var n = $d('#pw-note'); if (n) n.innerHTML = ok ? '<div class="note acc">' + ic('ruler') + '<span>Hệ số bù: chiều dài <b class="mono">' + fx(st.L / st.mL, 5) + '</b>, chiều rộng <b class="mono">' + fx(st.B / st.mB, 5) + '</b>. Không hiệu chỉnh, một nét dài 40 m vẽ ở 1:100 sẽ lệch ' + fx(Math.abs(dL) * 400 / 100, 1) + ' mm trên giấy.</span></div>' : ''; }
    dlg.el.addEventListener('click', function(e){ if (!e.target.closest('#pw-print')) return; st.run = true; render(); setTimeout(function(){ if (!document.contains(dlg.el)) return; st.run = false; st.printed = true; render(); }, 1100); });
    dlg.el.addEventListener('change', function(e){ if (e.target.id === 'pw-had') st.had = e.target.checked; else if (e.target.id === 'pw-chk') st.check = e.target.checked; });
    dlg.el.addEventListener('input', function(e){ if (e.target.id === 'pw-f') st.file = e.target.value; });
    dlg.el.addEventListener('numchange', function(e){ var id = (e.target.querySelector && e.target.querySelector('input') || e.target).id; if (id === 'pw-l') st.L = e.detail; else if (id === 'pw-w') st.B = e.detail; else if (id === 'pw-ml') st.mL = e.detail; else if (id === 'pw-mw') st.mB = e.detail; else return; if (st.k === 3) meas(); else { var cv = $d('#pw-pv'); if (cv) rect(cv); } });
    $d('#pw-back').addEventListener('click', function(){ st.k = Math.max(0, st.k - 1); render(); });
    $d('#pw-next').addEventListener('click', function(){
      $$('[data-num] input', dlg.el).forEach(function(inp){ if (inp.value.trim()) TC.numCommit(inp.closest('[data-num]')); });
      var e = err(); if (e){ fmsg($d('#pw-m'), e); return; } if (st.k < 4){ st.k++; render(); return; }
      c.pmp = { file:st.file.trim(), dx:Math.round(dev(st.mL, st.L) * 100) / 100, dy:Math.round(dev(st.mB, st.B) * 100) / 100, date:TC.date() }; dlg.close(true); done();
      if (st.check) TC.toast('Đã gửi tờ kiểm tra hiệu chỉnh tới ' + esc(c.name) + '. Đo lại: hình phải đúng ' + st.L + ' × ' + st.B + ' mm.', { log:false });
    });
    render();
  }
  IMPL.PLOTTERMANAGER = function(){ SC.pc3(); };
  OPEN['UI-H10'] = function(){ SC.cat.PRO(); PM.sel = 4; PM.tab = 'dev'; PM.node = 'cust'; PM.size = null; PM.q = ''; SC.pc3(); };

  /* ================= Thống kê thép UI-J14 (TCREBAR) =================
     Nhận dạng ký hiệu thép trên bản vẽ (2Ø16 L=4250, Ø8a200, số hiệu trong vòng tròn) hoặc block thanh thép có attribute;
     bảng thống kê mẫu TCVN có phác hình, tổng hợp theo đường kính; kiểm tra ký hiệu thiếu thông tin; chèn bảng, xuất Excel. */
  var RB = SC.rebar = { ids:null, src:'both', tab:'tbl', ov:{}, nck:{}, cover:25, hook:10, sep:10, sum:true, sel:null, dlg:null };
  var RB_SRC = { text:'Ký hiệu chữ', blk:'Block thanh thép', both:'Cả hai' }, DSTD = [6, 8, 10, 12, 14, 16, 18, 20, 22, 25, 28, 32, 36, 40];
  var LRE = /^(D|DM|DC|DP|C|CT|S)(\d+[A-Z]?)\s+(\d+)\s*[×xX]\s*(\d+)$/;
  // trọng lượng đơn vị (kg/m) = π·d²/4 × 7850 kg/m³, d tính bằng mm
  function kgm(d){ return Math.PI * d * d / 4 * 7850 / 1e6; }
  function rbNorm(s){ return String(s || '').replace(/%%[cC]/g, 'Ø').replace(/[∅Φφø]/g, 'Ø').replace(/\s+/g, ' ').trim(); }
  function rbParse(s){ s = rbNorm(s); var m;
    if ((m = /^(\d+)\s*Ø\s*(\d+)(?:\s*[,;]?\s*L\s*=?\s*(\d+))?$/i.exec(s))) return { n:+m[1], d:+m[2], L:m[3] ? +m[3] : null };
    if ((m = /^Ø\s*(\d+)\s*(?:a|@)\s*(\d+)(?:\s*[,;]?\s*L\s*=?\s*(\d+))?$/i.exec(s))) return { d:+m[1], a:+m[2], L:m[3] ? +m[3] : null };
    return /Ø/.test(s) ? { bad:s } : null; }
  // tự kiểm tra khi nạp: nhận dạng ký hiệu, trọng lượng đơn vị Ø16 = 1,578 kg/m
  console.assert(JSON.stringify(rbParse('2%%c16 L=4250')) === '{"n":2,"d":16,"L":4250}' && rbParse('Ø8 a200').a === 200 && rbParse('3Ø L=3200').bad && !rbParse('D1 220×400') && Math.abs(kgm(16) - 1.578) < .001, 'TCREBAR: nhận dạng ký hiệu thép sai');
  // khung chữ thật theo canh lề (bbox của chữ không theo canh giữa dọc)
  function tbox(t){ var h = t.h || 200, w = h * .62 * String(t.s).length, al = t.al || 'l', x0 = al === 'm' || al === 'c' ? t.p[0] - w / 2 : al === 'r' || al === 'rm' ? t.p[0] - w : t.p[0], cy = al === 'm' || al === 'lm' || al === 'rm' ? t.p[1] : t.p[1] + h * .4; return { x0:x0, x1:x0 + w, cx:x0 + w / 2, cy:cy, h:h }; }
  function hz(l){ return l.t === 'line' && Math.abs(l.p1[1] - l.p2[1]) < 1; }
  function xr(l){ return [Math.min(l.p1[0], l.p2[0]), Math.max(l.p1[0], l.p2[0])]; }
  function member(l, beams, labs){ var m = LRE.exec(l.s), b = tbox(l), kind = /^C/.test(m[1]) ? 'C' : /^S/.test(m[1]) ? 'S' : 'D', sp = beams.filter(function(e){ var r = xr(e); return hz(e) && b.cx > r[0] && b.cx < r[1] && Math.abs(e.p1[1] - b.cy) < b.h * 3; })[0];
    return { key:l.s, name:(kind === 'C' ? 'Cột ' : kind === 'S' ? 'Sàn ' : 'Dầm ') + m[1] + m[2] + ' ' + m[3] + '×' + m[4], kind:kind, b:+m[3], hh:+m[4], span:sp ? xr(sp)[1] - xr(sp)[0] : 0, n:labs.filter(function(x){ return x.s === l.s; }).length || 1 }; }
  function memFromCK(s){ var m = /^(Dầm|Cột|Sàn)\s+(\S+)\s+(\d+)\s*[×x]\s*(\d+)(?:\s*[·,(]\s*(\d+)\s*CK\)?)?/i.exec(String(s || '').trim()); if (!m) return { key:'?' + s, name:String(s || 'Cấu kiện khác'), kind:'', n:1 }; return { key:m[1] + ' ' + m[2], name:m[1] + ' ' + m[2] + ' ' + m[3] + '×' + m[4], kind:/^C/i.test(m[1]) ? 'C' : /^S/i.test(m[1]) ? 'S' : 'D', b:+m[3], hh:+m[4], span:0, n:m[5] ? +m[5] : 1 }; }
  // phạm vi rải thép sàn: giữa mép trong của hai dầm song song hai bên nét thép
  function slabRange(bl, beams){ var y = bl.p1[1], cx = (bl.p1[0] + bl.p2[0]) / 2, ys = beams.filter(function(e){ var r = xr(e); return hz(e) && cx > r[0] && cx < r[1]; }).map(function(e){ return e.p1[1]; }), up = ys.filter(function(v){ return v > y; }), dn = ys.filter(function(v){ return v < y; }); return up.length && dn.length ? Math.min.apply(null, up) - Math.max.apply(null, dn) : 0; }
  function rbScan(d){
    var all = ED.spaceList(d), src = RB.ids ? RB.ids.map(function(id){ return d.byId[id]; }).filter(Boolean) : all.slice();
    var txt = all.filter(function(e){ return e.t === 'text'; }), circ = all.filter(function(e){ return e.t === 'circle'; }), labs = txt.filter(function(e){ return LRE.test(e.s); }), bars = all.filter(function(e){ return hz(e) && /THEP/i.test(e.layer); }), beams = all.filter(function(e){ return hz(e) && /DAM/i.test(e.layer); });
    var items = [], nText = 0, nBlk = 0;
    function shape(it, k, mem, bl){
      if (k.n){ it.n = k.n;
        if (k.d <= RB.sep && !k.L && (mem.kind === 'C' || mem.kind === 'D') && mem.b){ var a = mem.b - 2 * RB.cover, b = mem.hh - 2 * RB.cover; it.sh = { k:'dai', a:a, b:b }; it.L = 2 * (a + b) + 2 * RB.hook * k.d; it.how = 'Đai ' + a + '×' + b + ', móc ' + RB.hook + 'd'; }
        else { it.L = k.L || (bl ? Math.round(xr(bl)[1] - xr(bl)[0]) : null); it.sh = { k:'thang', a:it.L }; if (!k.L && bl) it.how = 'Dài theo nét thép trên bản vẽ'; } return; }
      it.a = k.a;
      if (mem.kind === 'D' || mem.kind === 'C'){ var a2 = mem.b - 2 * RB.cover, b2 = mem.hh - 2 * RB.cover; it.sh = { k:'dai', a:a2, b:b2 }; it.L = k.L || 2 * (a2 + b2) + 2 * RB.hook * k.d; it.n = mem.span ? Math.floor((mem.span) / k.a) + 1 : null; it.how = (mem.span ? 'Rải ' + fx(mem.span, 0) + ' mm (nhịp thông thủy) / a' + k.a + ' + 1' : 'Chưa có nhịp dầm') + ' · đai ' + a2 + '×' + b2; }
      else { it.L = k.L || (bl ? Math.round(xr(bl)[1] - xr(bl)[0]) : null); it.sh = { k:'thang', a:it.L }; var rg = bl ? slabRange(bl, beams) : 0; it.n = rg ? Math.floor(rg / k.a) + 1 : null; it.how = rg ? 'Rải ' + fx(rg, 0) + ' mm giữa mép dầm / a' + k.a + ' + 1' + (k.L ? '' : ' · dài theo nét thép') : ''; }
    }
    if (RB.src !== 'blk') src.forEach(function(t){
      if (t.t !== 'text') return; var k = rbParse(t.s); if (!k) return; nText++;
      var b = tbox(t), it = { id:t.id, ids:[t.id], src:'text', raw:rbNorm(t.s), d:k.d, iss:[], bb:[b.x0, b.cy - b.h, b.x1, b.cy + b.h] };
      var c = circ.filter(function(c){ return c.r > b.h * .4 && c.r < b.h * 2.2 && Math.abs(c.c[1] - b.cy) < b.h && c.c[0] < b.x0 + b.h * .3 && b.x0 - c.c[0] < c.r + b.h * 2.5; })[0];
      if (c){ var mt = txt.filter(function(x){ var q = tbox(x); return /^\d+[a-z]?$/i.test(x.s) && Math.hypot(q.cx - c.c[0], q.cy - c.c[1]) < c.r; })[0]; if (mt){ it.mark = mt.s; it.ids.push(c.id, mt.id); it.bb[0] = c.c[0] - c.r; } }
      var lab = labs.map(function(l){ var q = tbox(l); return [l, Math.hypot(q.cx - b.cx, q.cy - b.cy)]; }).filter(function(z){ return z[1] < b.h * 9; }).sort(function(p, q){ return p[1] - q[1]; })[0];
      var bl = bars.filter(function(l){ var r = xr(l); return b.cx >= r[0] && b.cx <= r[1] && Math.abs(l.p1[1] - b.cy) < b.h * 1.6; })[0];
      var mem = lab ? member(lab[0], beams, labs) : bl ? { key:'San', name:'Sàn', kind:'S', n:1 } : { key:'Khac', name:'Cấu kiện khác', kind:'', n:1 };
      it.mem = mem; if (bl) it.ids.push(bl.id);
      if (k.bad){ it.bad = true; it.iss.push(['err', 'Không đọc được ký hiệu “' + k.bad + '”: thiếu đường kính hoặc số lượng.', 'parse']); } else shape(it, k, mem, bl);
      items.push(it); });
    if (RB.src !== 'text') src.forEach(function(e){
      if (e.t !== 'insert' || e.name !== 'TC-THEP') return; nBlk++; var a = X.attrsOf(d, e), k = rbParse(a.KH), mem = memFromCK(a.CK), bb = e._bb || [e.p[0], e.p[1], e.p[0], e.p[1]];
      var it = { id:e.id, ids:[e.id], src:'blk', raw:rbNorm(a.KH), d:k && k.d, mark:a.SH || '', iss:[], bb:bb.slice(), mem:mem };
      if (!k || k.bad){ it.bad = true; it.iss.push(['err', 'Block TC-THEP có KH “' + (a.KH || '') + '” không đọc được.', 'parse']); } else shape(it, k, mem, null);
      items.push(it); });
    // ghi đè trong bảng (số hiệu, chiều dài, số thanh), rồi kiểm tra
    items.forEach(function(it){ var o = RB.ov[it.id] || {}; if (o.mark) it.mark = o.mark; if (o.L){ it.L = o.L; if (it.sh) it.sh.a = it.sh.k === 'thang' ? o.L : it.sh.a; } if (o.n) it.n = o.n;
      if (it.bad) return;
      if (!it.mark) it.iss.push(['warn', '“' + it.raw + '” chưa có số hiệu thanh (vòng tròn số cạnh ký hiệu).', 'mark']);
      if (!it.L) it.iss.push(['err', 'Thiếu chiều dài thanh “' + it.raw + '”: ghi L=… sau ký hiệu, hoặc nhập trong bảng.', 'L']);
      if (!it.n) it.iss.push(['err', 'Không xác định được phạm vi rải “' + it.raw + '”: nhập số thanh trong bảng.', 'n']);
      if (DSTD.indexOf(it.d) < 0) it.iss.push(['warn', 'Đường kính Ø' + it.d + ' không có trong dãy thép thông dụng.', 'd']); });
    var byM = {}; items.forEach(function(it){ if (it.mark && !it.bad) (byM[it.mark] = byM[it.mark] || []).push(it); });
    var conf = [];
    Object.keys(byM).forEach(function(m){ var g = byM[m], sig = function(it){ return it.d + '|' + (it.L || '') + '|' + (it.sh ? it.sh.k : ''); }; if (g.some(function(it){ return sig(it) !== sig(g[0]); })){ g.forEach(function(it){ it.conf = true; }); conf.push({ lvl:'err', msg:'Số hiệu ' + m + ' dùng cho hai thanh khác nhau: ' + g.filter(function(it, i){ return g.map(sig).indexOf(sig(it)) === i; }).map(function(it){ return it.raw + (it.L && !/L\s*=/.test(it.raw) ? ' L=' + it.L : '') + ' (' + it.mem.name + ')'; }).join(' và ') + '. Đổi số hiệu một trong hai trên bản vẽ.', items:g, k:'conf' }); } });
    // dòng bảng: gom theo cấu kiện, rồi theo số hiệu, đường kính, chiều dài, hình dạng
    var mems = {}, order = [];
    items.forEach(function(it){ if (it.bad) return; var mk = it.mem.key; if (!mems[mk]){ mems[mk] = { key:mk, name:it.mem.name, kind:it.mem.kind, nck:RB.nck[mk] || it.mem.n, rows:{}, list:[] }; order.push(mk); } var g = mems[mk], rk = mk + '|' + (it.mark || '#' + it.id) + '|' + it.d + '|' + (it.L || '') + '|' + (it.sh ? it.sh.k + (it.sh.b || '') : '');
      var r = g.rows[rk]; if (!r){ r = g.rows[rk] = { key:rk, mark:it.mark, d:it.d, L:it.L, sh:it.sh, n1:0, items:[], iss:[] }; g.list.push(r); } r.items.push(it); r.n1 = it.n == null || r.n1 == null ? null : r.n1 + it.n; r.iss = r.iss.concat(it.iss); if (it.conf) r.conf = true; });
    var KO = { D:0, C:1, S:2, '':3 };
    var groups = order.map(function(k){ return mems[k]; }).sort(function(a, b){ return (KO[a.kind] || 0) - (KO[b.kind] || 0) || a.name.localeCompare(b.name, 'vi'); });
    groups.forEach(function(g){ g.list.sort(function(a, b){ return (Number(a.mark) || 999) - (Number(b.mark) || 999); }); g.list.forEach(function(r){ r.tot = r.n1 != null ? r.n1 * g.nck : null; r.len = r.tot != null && r.L ? r.tot * r.L / 1000 : null; r.kg = r.len != null ? r.len * kgm(r.d) : null; r.lvl = r.conf || r.iss.some(function(x){ return x[0] === 'err'; }) ? 'err' : r.iss.length ? 'warn' : ''; }); });
    var iss = conf.concat([].concat.apply([], items.map(function(it){ return it.iss.map(function(x){ return { lvl:x[0], msg:x[1], items:[it], k:x[2] }; }); })));
    iss.sort(function(a, b){ return (a.lvl === 'err' ? 0 : 1) - (b.lvl === 'err' ? 0 : 1); });
    return { groups:groups, iss:iss, nText:nText, nBlk:nBlk, nRows:groups.reduce(function(a, g){ return a + g.list.length; }, 0) };
  }
  function rbSum(groups){ var by = {}, lo = 0, hi = 0, skip = 0; groups.forEach(function(g){ g.list.forEach(function(r){ if (r.kg == null){ skip++; return; } var x = by[r.d] = by[r.d] || { d:r.d, len:0, kg:0 }; x.len += r.len; x.kg += r.kg; if (r.d <= RB.sep) lo += r.kg; else hi += r.kg; }); });
    return { ds:Object.keys(by).map(function(k){ return by[k]; }).sort(function(a, b){ return a.d - b.d; }), lo:lo, hi:hi, skip:skip }; }
  // phác hình thanh thép trong hộp thoại (SVG) và trong bảng chèn vào bản vẽ (block trong ô)
  function sk(r){ var sh = r.sh || { k:'thang', a:r.L }, T = function(x, y, s, a){ return '<text x="' + x + '" y="' + y + '"' + (a ? ' text-anchor="' + a + '"' : '') + '>' + esc(s) + '</text>'; };
    if (sh.k === 'dai') return '<svg class="tkt-sk" viewBox="0 0 140 26" role="img" aria-label="Đai ' + sh.a + ' × ' + sh.b + '"><path d="M40 8.5h30v14H40z"/><path d="M40 12.5 44 8.5M43.5 12.5 47.5 8.5"/>' + T(55, 6.5, sh.a, 'middle') + T(74, 18.5, sh.b) + '</svg>';
    return '<svg class="tkt-sk" viewBox="0 0 140 26" role="img" aria-label="Thanh thẳng dài ' + (sh.a || '?') + '"><path d="M12 17.5h96"/>' + T(60, 13, sh.a ? fx(sh.a, 0) : '?', 'middle') + '</svg>'; }
  function skBlock(d, sh){ var nm = 'TK-THEP-' + (sh.k === 'dai' ? 'D' + sh.a + 'X' + sh.b : 'T' + sh.a); if (d.blocks[nm]) return nm; var L = [], a = function(o){ o.id = ++d.seq; o.layer = '0'; L.push(o); };
    if (sh.k === 'dai'){ a({ t:'pline', pts:rp(0, 0, 9, 5), closed:true }); a({ t:'line', p1:[0, 3.6], p2:[1.4, 5] }); a({ t:'line', p1:[1.2, 3.6], p2:[2.6, 5] }); a({ t:'text', p:[4.5, 5.6], h:1.8, s:String(sh.a), style:'Standard', al:'c' }); a({ t:'text', p:[9.8, 2.5], h:1.8, s:String(sh.b), style:'Standard', al:'lm' }); }
    else { a({ t:'line', p1:[0, 0], p2:[24, 0] }); a({ t:'text', p:[12, .7], h:1.8, s:String(sh.a), style:'Standard', al:'c' }); }
    d.blocks[nm] = { name:nm, desc:'Phác hình thanh thép (bảng thống kê thép)', ents:L }; return nm; }
  function rbTables(d, R, p, k){
    var cells = [['BẢNG THỐNG KÊ CỐT THÉP', '', '', '', '', '', '', '', ''], ['Cấu kiện', 'Số hiệu', 'Hình dạng, kích thước (mm)', 'Đường kính (mm)', 'Dài 1 thanh (mm)', 'Số lượng 1 CK', 'Tổng số thanh', 'Tổng dài (m)', 'Khối lượng (kg)']], merge = [[0, 0, 8]], cf = {}, r = 2, S = rbSum(R.groups), lay = d.layerMap['KC-TEXT'] ? 'KC-TEXT' : ED.clayer(d);
    R.groups.forEach(function(g){ var r0 = r; g.list.forEach(function(x){ cells.push([r === r0 ? g.name + (g.nck > 1 ? ' (' + g.nck + ' CK)' : '') : '', x.mark || '', x.L ? '' : '—', String(x.d), x.L ? String(x.L) : '', x.n1 != null ? String(x.n1) : '', x.tot != null ? String(x.tot) : '', x.len != null ? TC.d(x.len, 2) : '', x.kg != null ? TC.d(x.kg, 2) : '']); if (x.L && x.sh) cf[r + ',2'] = { blk:skBlock(d, x.sh) }; r++; }); if (r - 1 > r0) merge.push([r0, 0, 0, r - 1]); });
    [['Tổng thép Ø ≤ ' + RB.sep + ' (CB240-T)', S.lo], ['Tổng thép Ø > ' + RB.sep + ' (CB300-V)', S.hi], ['TỔNG CỘNG', S.lo + S.hi]].forEach(function(z){ cells.push([z[0], '', '', '', '', '', '', '', TC.d(z[1], 2)]); merge.push([r, 0, 7]); r++; });
    var cols = [26, 10, 32, 12, 14, 13, 13, 14, 15], rows = [10, 14].concat(cells.slice(2).map(function(){ return 7; }));
    var main = { t:'table', p:p, cols:cols.map(function(w){ return w * k; }), rows:rows.map(function(h){ return h * k; }), th:2.5 * k, head:2, cells:cells, merge:merge, cf:cf, tstyle:'TCVN-thong-ke-thep', style:'Standard', layer:lay }, out = [main];
    if (RB.sum){ var c2 = [['TỔNG HỢP THÉP THEO ĐƯỜNG KÍNH', '', '', '', ''], ['Đường kính (mm)', 'Mác thép', 'Tổng dài (m)', 'KL đơn vị (kg/m)', 'Khối lượng (kg)']].concat(S.ds.map(function(x){ return ['Ø' + x.d, x.d <= RB.sep ? 'CB240-T' : 'CB300-V', TC.d(x.len, 2), TC.d(kgm(x.d), 3), TC.d(x.kg, 2)]; }), [['Tổng cộng', '', '', '', TC.d(S.lo + S.hi, 2)]]), wm = cols.reduce(function(a, b){ return a + b; }, 0);
      out.push({ t:'table', p:[p[0] + (wm + 12) * k, p[1]], cols:[18, 18, 18, 20, 22].map(function(w){ return w * k; }), rows:[10, 14].concat(c2.slice(2).map(function(){ return 7; })).map(function(h){ return h * k; }), th:2.5 * k, head:2, cells:c2, merge:[[0, 0, 4], [c2.length - 1, 0, 3]], tstyle:'TCVN-thong-ke-thep', style:'Standard', layer:lay }); }
    return out;
  }
  function rbExcel(d, R){ var nm = 'Thong-ke-thep_' + TC.fold(d.name.replace(/\.\w+$/, '')).replace(/[^\w-]+/g, '-'), S = rbSum(R.groups); return { label:'Excel (.xlsx)', act:function(){ TC.toast('Đã xuất <span class="mono">' + esc(nm) + '.xlsx</span>: sheet Thống kê (' + R.nRows + ' dòng, phác hình dạng ảnh) và sheet Tổng hợp (' + S.ds.length + ' đường kính, ' + fx(S.lo + S.hi, 2) + ' kg).', { act:{ label:'Mở tệp', fn:function(){} }, kind:'task' }); } }; }
  function rbBB(items){ var b = [Infinity, Infinity, -Infinity, -Infinity]; items.forEach(function(it){ b = [Math.min(b[0], it.bb[0]), Math.min(b[1], it.bb[1]), Math.max(b[2], it.bb[2]), Math.max(b[3], it.bb[3])]; }); var m = Math.max(b[2] - b[0], b[3] - b[1]) * .6 + 1200; return [b[0] - m, b[1] - m, b[2] + m, b[3] + m]; }
  function rbFlash(d, items, zoom){ d.found = [].concat.apply([], items.map(function(it){ return it.ids; })); if (zoom && RB.dlg) SC.zoomBeside(rbBB(items), RB.dlg.el); V().drawOver(); }
  function rbDialog(d){
    if (RB.dlg && !RB.dlg.closed) RB.dlg.close('re');
    var dlg = RB.dlg = TC.dialog({ id:'rebar', title:'Thống kê thép', cmd:'TCREBAR', code:'UI-J14', w:940, h:620, modal:false, flush:true, focus:'[data-rbt][aria-selected="true"]', body:'<div class="tkt" id="rb-b"></div>', foot:'<span></span>',
      onClose:function(r){ if (RB.dlg === dlg) RB.dlg = null; d.found = []; V().drawOver(); if (r !== 'done' && r !== 're' && TC.cmdline.current() === 'TCREBAR') TC.cmd.cancel(); } });
    var cr = $('#cv').getBoundingClientRect(); dlg.el.style.left = Math.max(12, cr.right - 952) + 'px'; dlg.el.style.top = Math.max(12, Math.min(cr.top + 6, innerHeight - dlg.el.offsetHeight - 12)) + 'px';
    var R = null;
    function inp(kind, r, ph){ return '<input class="inp sm tkt-in" data-rbk="' + kind + ':' + esc(r.key) + '" value="" placeholder="' + esc(ph) + '" aria-label="' + esc(ph) + ', số hiệu ' + esc(r.mark || 'chưa có') + '" spellcheck="false">'; }
    function render(){
      if (!document.contains(dlg.el)) return; var fa = document.activeElement, fk = fa && fa.dataset ? fa.dataset.rbk : null;
      R = RB.res = rbScan(d); var S = rbSum(R.groups), nE = R.iss.filter(function(x){ return x.lvl === 'err'; }).length, nW = R.iss.length - nE, B = '';
      if (RB.tab === 'tbl') B = !R.nRows ? '<div class="empty">' + ic('rebar') + '<b>Không tìm thấy ký hiệu thép</b><span>TrionCAD đọc chữ dạng <span class="mono">2Ø16 L=4250</span>, <span class="mono">Ø8a200</span> (cả <span class="mono">%%c</span>, Φ), số hiệu trong vòng tròn cạnh ký hiệu, và block TC-THEP có attribute SH, KH, CK.</span><button class="btn sm" type="button" data-rb="S">' + ic('select') + 'Chọn lại vùng</button></div>'
        : '<div class="tblwrap tkt-tbl"><table class="tbl"><colgroup><col style="width:58px"><col style="width:150px"><col style="width:46px"><col style="width:96px"><col style="width:74px"><col style="width:70px"><col style="width:90px"><col style="width:88px"><col style="width:30px"></colgroup><thead><tr><th>Số hiệu</th><th>Hình dạng, kích thước</th><th class="num">Ø</th><th class="num">Dài 1 thanh</th><th class="num">SL 1 CK</th><th class="num">Tổng SL</th><th class="num">Tổng dài (m)</th><th class="num">KL (kg)</th><th></th></tr></thead><tbody>' +
          R.groups.map(function(g){ return '<tr class="tkt-g"><td colspan="9"><span class="tkt-gn">' + esc(g.name) + '</span><label class="tkt-ck">Số cấu kiện<input class="inp sm tkt-in" data-rbk="ck:' + esc(g.key) + '" value="' + g.nck + '" aria-label="Số cấu kiện ' + esc(g.name) + '"></label></td></tr>' + g.list.map(function(r){ var on = RB.sel === r.key, how = r.items.map(function(it){ return it.how; }).filter(Boolean)[0];
            return '<tr data-rbr="' + esc(r.key) + '" class="' + (on ? 'is-sel' : '') + '"' + (how ? ' title="' + esc(how) + '"' : '') + '><td>' + (r.mark ? '<span class="tkt-mk mono' + (r.conf ? ' bad' : '') + '">' + esc(r.mark) + '</span>' : inp('mark', r, 'Số hiệu')) + '</td><td>' + (r.L ? sk(r) : '<span class="faint">—</span>') + '</td><td class="num">' + r.d + '</td><td class="num">' + (r.L ? fx(r.L, 0) : inp('L', r, 'mm')) + '</td><td class="num">' + (r.n1 != null ? r.n1 : inp('n', r, 'thanh')) + '</td><td class="num">' + (r.tot != null ? r.tot : '—') + '</td><td class="num">' + (r.len != null ? fx(r.len, 2) : '—') + '</td><td class="num">' + (r.kg != null ? fx(r.kg, 2) : '—') + '</td><td class="ctr">' +
              (r.lvl ? '<span class="' + (r.lvl === 'err' ? 'danger' : 'warn') + '" title="' + esc((r.conf ? 'Số hiệu trùng với thanh khác. ' : '') + r.iss.map(function(x){ return x[1]; }).join(' ')) + '">' + ic(r.lvl === 'err' ? 'error' : 'warn', 's12') + '</span>' : '') + '</td></tr>'; }).join(''); }).join('') + '</tbody></table></div>' +
          '<div class="tkt-tot"><span>' + R.nRows + ' dòng · ' + R.groups.length + ' cấu kiện</span><span class="grow"></span><span>Ø ≤ ' + RB.sep + ' <b class="mono">' + fx(S.lo, 2) + '</b> kg</span><span>Ø > ' + RB.sep + ' <b class="mono">' + fx(S.hi, 2) + '</b> kg</span><span>Tổng <b class="mono">' + fx(S.lo + S.hi, 2) + '</b> kg</span></div>';
      else if (RB.tab === 'sum') B = '<div class="stack" style="padding:12px"><div class="tblwrap"><table class="tbl"><colgroup><col style="width:110px"><col style="width:110px"><col><col><col></colgroup><thead><tr><th>Đường kính</th><th>Mác thép</th><th class="num">Tổng dài (m)</th><th class="num">KL đơn vị (kg/m)</th><th class="num">Khối lượng (kg)</th></tr></thead><tbody>' +
          S.ds.map(function(x){ return '<tr><td class="mono">Ø' + x.d + '</td><td>' + (x.d <= RB.sep ? 'CB240-T' : 'CB300-V') + '</td><td class="num">' + fx(x.len, 2) + '</td><td class="num">' + fx(kgm(x.d), 3) + '</td><td class="num">' + fx(x.kg, 2) + '</td></tr>'; }).join('') +
          '<tr class="tkt-st"><td colspan="4">Thép Ø ≤ ' + RB.sep + ' (CB240-T)</td><td class="num">' + fx(S.lo, 2) + '</td></tr><tr class="tkt-st"><td colspan="4">Thép Ø > ' + RB.sep + ' (CB300-V)</td><td class="num">' + fx(S.hi, 2) + '</td></tr><tr class="tkt-st tkt-gt"><td colspan="4">Tổng cộng</td><td class="num">' + fx(S.lo + S.hi, 2) + '</td></tr></tbody></table></div>' +
          '<span class="faint" style="font-size:var(--fs-s)">Trọng lượng đơn vị = π·d²/4 × 7 850 kg/m³. ' + (S.skip ? S.skip + ' dòng còn thiếu chiều dài hoặc số thanh chưa tính vào tổng: xem tab Kiểm tra.' : 'Mọi dòng đã đủ thông tin.') + '</span>' + H.chk('rb-sum', 'Chèn kèm bảng tổng hợp theo đường kính, đặt bên phải bảng thống kê', RB.sum) + '</div>';
      else B = '<div class="stack" style="padding:12px">' + (R.iss.length ? '<div class="tkt-iss" role="list">' + R.iss.map(function(x, i){ return '<div class="tkt-is" role="listitem"><span class="' + (x.lvl === 'err' ? 'danger' : 'warn') + '">' + ic(x.lvl === 'err' ? 'error' : 'warn', 's12') + '</span><span class="grow"><span>' + esc(x.msg) + '</span><span class="faint">' + esc(x.items.map(function(it){ return it.mem ? it.mem.name : ''; }).filter(function(v, j, a){ return v && a.indexOf(v) === j; }).join(', ')) + (x.items[0].src === 'blk' ? ' · block TC-THEP' : '') + '</span></span>' + (/^(L|n|mark)$/.test(x.k) ? '<button class="btn sm ghost" type="button" data-rbfix="' + i + '">Nhập trong bảng</button>' : '') + '<button class="btn sm" type="button" data-rbz="' + i + '">' + ic('zwin') + 'Zoom tới</button></div>'; }).join('') + '</div>' : '<div class="note acc">' + ic('okc') + '<span>Mọi ký hiệu thép đủ thông tin: số hiệu, đường kính, chiều dài, số lượng.</span></div>') +
        H.grp('Quy tắc nhận dạng', '<div class="tkt-rules"><span class="lab">Lớp bảo vệ</span>' + nf({ id:'rb-cov', value:String(RB.cover), label:'Lớp bảo vệ', pick:false, prec:0, unit:'none', style:'width:84px' }) + '<span class="faint">mm · kích thước đai = tiết diện − 2 × lớp bảo vệ</span><span class="lab">Móc đai</span>' + nf({ id:'rb-hook', value:String(RB.hook), label:'Móc đai', pick:false, prec:0, unit:'none', style:'width:84px' }) + '<span class="faint">× d mỗi đầu (móc 135°)</span><span class="lab">Mác thép</span>' + nf({ id:'rb-sep', value:String(RB.sep), label:'Đường kính chia mác', pick:false, prec:0, unit:'none', style:'width:84px' }) + '<span class="faint">Ø ≤ giá trị này: CB240-T, lớn hơn: CB300-V</span></div><span class="faint" style="font-size:var(--fs-xs)">Ký hiệu đường kính: Ø, %%c, Φ. Số hiệu: số trong vòng tròn bên trái ký hiệu. Cấu kiện: nhãn dầm, cột gần ký hiệu (D1 220×400). Thép sàn rải giữa mép hai dầm.</span>') + '</div>';
      $('#rb-b', dlg.el).innerHTML = '<div class="tkt-bar"><span class="lab">Nguồn</span>' + SC.seg('rbsrc', [RB_SRC.text, RB_SRC.blk, RB_SRC.both], RB_SRC[RB.src]) + '<span class="faint ell grow">' + (RB.ids ? RB.ids.length + ' đối tượng đã chọn' : 'Toàn bộ ' + (d.space === 'model' ? 'Model' : 'layout')) + ' · ' + esc(d.name) + ' · ' + R.nText + ' ký hiệu chữ, ' + R.nBlk + ' block</span><button class="btn sm" type="button" data-rb="S">' + ic('select') + 'Chọn lại</button></div>' +
        '<div class="tabs" role="tablist">' + [['tbl', 'Bảng thống kê', R.nRows, ''], ['sum', 'Tổng hợp theo đường kính', S.ds.length, ''], ['chk', 'Kiểm tra', R.iss.length, nE ? 'danger' : nW ? 'warn' : '']].map(function(t){ return '<button class="tab" role="tab" type="button" data-rbt="' + t[0] + '" aria-selected="' + (RB.tab === t[0]) + '">' + t[1] + '<span class="tkt-n' + (t[3] ? ' ' + t[3] : '') + '">' + t[2] + '</span></button>'; }).join('') + '</div><div class="tkt-body">' + B + '</div>';
      $('.dlg-f', dlg.el).innerHTML = '<button class="btn" type="button" data-rb="R">' + ic('refresh') + 'Quét lại</button><button class="btn" type="button" data-rb="X" aria-haspopup="menu"' + (R.nRows ? '' : ' disabled') + '>' + ic('stats') + 'Xuất Excel' + ic('down', 'xs') + '</button><span class="grow"></span>' + (nE ? '<span class="faint" style="font-size:var(--fs-s)">' + nE + ' lỗi cần sửa</span>' : '') + '<button class="btn ghost" type="button" data-x="close">Đóng</button><button class="btn primary" type="button" data-rb="C"' + (R.nRows ? '' : ' disabled') + '>' + ic('table') + 'Chèn bảng thống kê</button>';
      if (fk){ var n = $('[data-rbk="' + fk.replace(/"/g, '\\"') + '"]', dlg.el); if (n) n.focus(); }
    }
    function act(k, b){ if (TC.cmdline.current() === 'TCREBAR') TC.cmdline.submit(k); else if (k === 'X') xlsMenu(b); }
    function xlsMenu(b){ var r = b.getBoundingClientRect(), x = rbExcel(d, R); TC.menu([x, { label:'CSV (UTF-8)', act:function(){ TC.toast('Đã xuất bảng thống kê thép thành CSV (' + R.nRows + ' dòng).', { log:false }); } }], r.left, r.top, { up:true, returnFocus:b }); }
    RB.xls = function(){ var b = $('[data-rb="X"]', dlg.el); if (b) xlsMenu(b); }; RB.ui = render;
    dlg.el.addEventListener('click', function(e){
      var s = e.target.closest('[data-seg="rbsrc"] button'); if (s){ RB.src = Object.keys(RB_SRC).filter(function(k){ return RB_SRC[k] === s.dataset.v; })[0]; render(); return; }
      var t = e.target.closest('[data-rbt]'); if (t){ RB.tab = t.dataset.rbt; render(); var f = $('[data-rbt="' + RB.tab + '"]', dlg.el); if (f) f.focus(); return; }
      var row = e.target.closest('[data-rbr]'); if (row && !e.target.closest('input')){ RB.sel = row.dataset.rbr; var r0 = null; R.groups.forEach(function(g){ g.list.forEach(function(r){ if (r.key === RB.sel) r0 = r; }); }); $$('[data-rbr]', dlg.el).forEach(function(x){ x.classList.toggle('is-sel', x === row); }); if (r0) rbFlash(d, r0.items, false); return; }
      var z = e.target.closest('[data-rbz]'); if (z){ rbFlash(d, R.iss[Number(z.dataset.rbz)].items, true); return; }
      var fxb = e.target.closest('[data-rbfix]'); if (fxb){ var it = R.iss[Number(fxb.dataset.rbfix)].items[0]; RB.tab = 'tbl'; render(); var rk = null; R.groups.forEach(function(g){ g.list.forEach(function(r){ if (r.items.indexOf(it) >= 0) rk = r.key; }); }); var f2 = rk && $('[data-rbk$="' + rk.replace(/"/g, '\\"') + '"]', dlg.el); if (f2){ f2.focus(); f2.scrollIntoView({ block:'nearest' }); } return; }
      var b = e.target.closest('[data-rb]'); if (!b || b.disabled) return; var k = b.dataset.rb;
      if (k === 'R'){ RB.ov = {}; render(); TC.cmdline.msg('Đã quét lại ký hiệu thép trên bản vẽ.'); } else act(k, b);
    });
    dlg.el.addEventListener('dblclick', function(e){ var row = e.target.closest('[data-rbr]'); if (!row || e.target.closest('input')) return; R.groups.forEach(function(g){ g.list.forEach(function(r){ if (r.key === row.dataset.rbr) rbFlash(d, r.items, true); }); }); });
    // nhập trong bảng: áp dụng khi rời ô (Tab, Enter), vẽ lại sau đó để không mất vị trí con trỏ
    dlg.el.addEventListener('change', function(e){ var t = e.target;
      if (t.id === 'rb-sum'){ RB.sum = t.checked; return; }
      var k = t.dataset.rbk; if (!k) return; var i = k.indexOf(':'), kind = k.slice(0, i), key = k.slice(i + 1), v = t.value.trim();
      if (kind === 'ck'){ var n = parseInt(v, 10); if (n > 0) RB.nck[key] = n; }
      else R.groups.forEach(function(g){ g.list.forEach(function(r){ if (r.key !== key) return; r.items.forEach(function(it){ var o = RB.ov[it.id] = RB.ov[it.id] || {}; if (kind === 'mark' && v) o.mark = v; if (kind === 'L' && +v > 0) o.L = Math.round(+v); if (kind === 'n' && +v > 0) o.n = Math.round(+v / r.items.length) || 1; }); }); });
      setTimeout(render, 0); });
    dlg.el.addEventListener('numchange', function(e){ var id = (e.target.querySelector && e.target.querySelector('input') || e.target).id, map = { 'rb-cov':'cover', 'rb-hook':'hook', 'rb-sep':'sep' }; if (!map[id] || !(e.detail >= 0)) return; RB[map[id]] = Math.round(e.detail); setTimeout(render, 0); });
    render(); return dlg;
  }
  IMPL.TCREBAR = function(ctx, pre){
    var d = D(); ctx.badge('TK'); RB.ov = {}; RB.sel = null;
    var pick = function(){ return ctx.sel('Chọn ký hiệu thép, block thanh thép (Enter: cả ' + (d.space === 'model' ? 'Model' : 'layout') + ')').then(function(ids){ RB.ids = ids.length ? ids.slice() : null; TC.cad.clearSel(); }); };
    var start = pre === 'ALL' ? Promise.resolve(RB.ids = null) : d.sel.length ? Promise.resolve(RB.ids = d.sel.slice()) : pick();
    function loop(){
      if (!RB.dlg) rbDialog(d);
      return ctx.kw('Thống kê thép: kiểm tra, sửa trong hộp thoại rồi', [['Chèn bảng', 'C'], ['Chọn lại', 'S'], ['Xuất Excel', 'X']], 'C').then(function(k){
        var R = RB.res || rbScan(d);
        if (k === 'S'){ RB.dlg.hide(true); return pick().then(function(){ if (RB.dlg){ RB.dlg.hide(false); RB.ov = {}; RB.sel = null; if (RB.ui) RB.ui(); } return loop(); }); }
        if (k === 'X'){ if (RB.xls) RB.xls(); return loop(); }
        if (k !== 'C') return loop();
        if (!R.nRows){ ctx.out('Chưa có dòng nào để chèn. Chọn lại vùng có ký hiệu thép.'); return loop(); }
        var nE = R.iss.filter(function(x){ return x.lvl === 'err'; }).length;
        return (nE ? TC.confirm({ title:'Chèn bảng thống kê thép', msg:'Còn <b>' + nE + ' lỗi</b> ở tab Kiểm tra.', detail:'Dòng thiếu chiều dài hoặc số thanh để trống ô và không tính vào tổng. Sửa trên bản vẽ rồi Quét lại để bảng đầy đủ.', ok:'Vẫn chèn bảng', cancel:'Xem kiểm tra' }) : Promise.resolve(true)).then(function(y){
          if (!y){ RB.tab = 'chk'; if (RB.ui) RB.ui(); return loop(); }
          var dlg = RB.dlg; if (dlg) dlg.hide(true); var k2 = d.space === 'model' || d.activeVp ? 100 : 1;
          return ctx.point('Điểm chèn bảng thống kê thép (góc trên trái) hoặc', { opts:[['Quay lại', 'B']], onMove:function(p, temp){ temp.push(ED.temp(rbTables(d, R, p, k2), { real:true })); } }).then(function(r){
            if (!r.p){ if (dlg) dlg.hide(false); return loop(); }
            var T = rbTables(d, R, r.p, k2), S = rbSum(R.groups); ED.run('TCREBAR', function(){ T.forEach(function(t){ ED.add(ED.make(t)); }); }); V().draw(); if (dlg) dlg.close('done');
            ctx.out('Đã chèn bảng thống kê thép ' + R.nRows + ' dòng' + (T.length > 1 ? ' và bảng tổng hợp ' + S.ds.length + ' đường kính' : '') + ': tổng <b>' + fx(S.lo + S.hi, 2) + ' kg</b> (Ø ≤ ' + RB.sep + ': ' + fx(S.lo, 2) + ' kg; Ø > ' + RB.sep + ': ' + fx(S.hi, 2) + ' kg). Kiểu bảng TCVN-thong-ke-thep, xuất lại bằng TCEXCELOUT.'); }); }); });
    }
    return start.then(function(){ TC.cad.clearSel(); return loop(); }).then(function(){ if (RB.dlg) RB.dlg.close('done'); d.found = []; V().drawOver(); }, function(err){ if (RB.dlg) RB.dlg.close('done'); d.found = []; V().drawOver(); throw err; });
  };
  // bản vẽ Ketcau-T2 chỉ có thép sàn Ø8a200: thêm ghi chú thép dầm, số hiệu, block cột và vài lỗi điển hình để thử TCREBAR
  function ensureBarBlock(d){ if (d.blocks['TC-THEP']) return; var L = [], a = function(o){ o.id = ++d.seq; o.layer = '0'; L.push(o); };
    a({ t:'circle', c:[0, 0], r:1.8 }); a({ t:'attdef', tag:'SH', prompt:'Số hiệu', p:[0, 0], h:1.8, s:'1', style:'Standard', al:'m' }); a({ t:'attdef', tag:'KH', prompt:'Ký hiệu (2Ø16 L=4250)', p:[2.8, 0], h:2, s:'2Ø16', style:'Standard', al:'lm' }); a({ t:'attdef', tag:'CK', prompt:'Cấu kiện (Cột C1 220×220 · 12 CK)', p:[2.8, -2.8], h:1.5, s:'', style:'Standard', al:'lm' });
    d.blocks['TC-THEP'] = { name:'TC-THEP', desc:'Thanh thép có attribute SH (số hiệu), KH (ký hiệu), CK (cấu kiện, số cấu kiện)', ents:L }; }
  function demoJ14(d){
    if (d.ents.some(function(e){ return e.t === 'text' && e.s === '2Ø16 L=4250'; })) return;
    var add = function(o){ o.id = ++d.seq; d.ents.push(o); TC.prepEnt(d, o); return o; }, K = d.layerMap['KC-TEXT'] ? 'KC-TEXT' : '0';
    var mark = function(x, y, s){ add({ t:'circle', c:[x, y], r:150, layer:K }); add({ t:'text', p:[x, y], h:150, s:s, style:'Standard', al:'m', layer:K }); };
    var note = function(x, y, s, mk){ if (mk) mark(x - 230, y + 70, mk); add({ t:'text', p:[x, y], h:180, s:s, style:'Standard', al:'l', layer:K }); };
    [[700, ['2Ø16 L=4250', '1'], ['2Ø14 L=4250', '2'], ['Ø6a150', '3']], [4600, ['2Ø18 L=4300', '4'], ['2Ø14 L=4300', '5'], ['Ø6a150', '6']], [8500, ['2Ø16 L=4300', '1'], ['2Ø14', '8'], ['Ø6a150', '9']]].forEach(function(z){ [1, 2, 3].forEach(function(j){ note(z[0], 400 + j * 300, z[j][0], z[j][1]); }); });
    note(4600, 5900, '3Ø L=3200', '7');
    [[1950, 2560, '10'], [5850, 2560, '10'], [9750, 2560, '10'], [1950, 7060, '11']].forEach(function(z){ mark(z[0] - 600, z[1], z[2]); });
    ensureBarBlock(d); [[13000, 5300, '12', '4Ø16 L=3600'], [13000, 4300, '13', '18Ø6']].forEach(function(z){ add({ t:'insert', name:'TC-THEP', p:[z[0], z[1]], sx:100, sy:100, rot:0, layer:K, attrs:{ SH:z[2], KH:z[3], CK:'Cột C1 220×220 · 12 CK' } }); });
    TC.docExt(d);
  }
  OPEN['UI-J14'] = function(){
    SC.cat.PRO(); A().openDoc('d2'); var d = D(); if (d.space !== 'model') A().setSpace('model'); demoJ14(d); TC.cad.clearSel(); RB.tab = 'tbl'; RB.src = 'both';
    var v = V(); v.zoomTo([-1200, -1800, 16500, 9800]); setTimeout(function(){ TC.cmd.run('TCREBAR', 'ALL'); setTimeout(function(){ if (RB.dlg) SC.zoomBeside([-1200, -1800, 16500, 9800], RB.dlg.el); }, 80); }, 60);
  };
})();
