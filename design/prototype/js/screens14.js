/* TrionCAD prototype · Đợt 4: khung ứng dụng (C02 thuộc tính nhanh, C04 trạng thái layer, C05 bộ lọc layer, C11 duyệt layer, D12 view, D13 UCS, K06 phím tắt, K08 workspace, K13 biến hệ thống, bản macOS). */
(function(){
  'use strict';
  var TC = window.TC, $ = TC.$, $$ = TC.$$, esc = TC.esc, ic = TC.ic, G = TC.geo, IMPL = TC.cmd.impl, ED = TC.ed, SC = TC.screens, H = SC.h;
  function A(){ return TC.app; } function V(){ return TC.app.view; } function D(){ return TC.app.doc(); }
  var OPEN = SC.open4 = SC.open4 || {}; // mã màn hình → hàm mở (danh mục gọi khi bấm); tiện ích mở ở SC.cat (D1, PRO, MOD, RUN, plan, pick, flash)
  function busy(){ return TC.cmdline.busy(); }

  /* ================= Thuộc tính nhanh UI-C02 ================= */
  // trường mặc định theo loại (07.2 UI-C02); chọn trường cho từng loại là sau v1.0
  var QF = { line:['Độ dài'], circle:['Bán kính'], arc:['Bán kính'], ellipse:['Bán trục lớn', 'Bán trục nhỏ'], text:['Nội dung', 'Chiều cao'], mtext:['Nội dung', 'Chiều cao'], insert:['Tên', 'Góc xoay'], hatch:['Tên mẫu', 'Tỉ lệ', 'Góc'], dim:['Kiểu kích thước', 'Ghi đè chữ'], dimr:['Kiểu kích thước', 'Ghi đè chữ'], dima:['Kiểu kích thước', 'Ghi đè chữ'], pline:['Bề rộng toàn cục', 'Đóng', 'Diện tích', 'Độ dài'], mleader:['Kiểu multileader', 'Nội dung'], image:['Tên'], table:['Kiểu bảng'], attdef:['Tag', 'Giá trị mặc định'] };
  var QP = TC.qp = { loc:TC.load('qpLoc', 'cursor'), auto:TC.load('qpAuto', true), pos:TC.load('qpPos', null), filter:'all', el:null, force:false };
  var dummy = document.createElement('div');
  function qpRows(d, list){
    var e0 = list[0], keys = ['Màu', 'Layer', 'Linetype'].concat(list.every(function(e){ return e.t === e0.t; }) ? QF[e0.t] || [] : []);
    return keys.map(function(k){
      var vals = list.map(function(e){ return TC.propRows(d, e).filter(function(r){ return r[0] === k; })[0]; });
      if (!vals[0]) return null;
      var same = vals.every(function(v){ return v && v[1] === vals[0][1]; });
      return same ? vals[0] : [k, '<span class="varies">*Khác nhau*</span>', 'html'];
    }).filter(Boolean);
  }
  function rowHTML(r){
    var k = r[0], kind = r[2], ed = kind !== 'ro' && TC.propEditable(k, kind);
    return '<div class="pr' + (kind === 'ro' ? ' ro' : '') + (ed ? ' edt' : '') + '" data-pk="' + esc(k) + '"><span class="pk">' + esc(k) + '</span><span class="pv' + (kind === 'num' || kind === 'ro' ? ' mono' : '') + '">' + (kind === 'html' ? r[1] : '<span>' + r[1] + '</span>') + (ed && TC.propIsMenu(k) ? '<i class="pdd">' + ic('down', 'xs') + '</i>' : '') + '</span></div>';
  }
  QP.show = function(force){
    var d = D(); if (force) QP.force = true;
    if (!d || A().active === 'start' || !TC.can('PRO') || busy() || !d.sel.length || !(TC.S.toggles.qp || QP.force)){ QP.hide(); return; }
    var sel = d.sel.map(function(id){ return d.byId[id]; }).filter(Boolean); if (!sel.length){ QP.hide(); return; }
    var types = {}; sel.forEach(function(e){ types[e.t] = (types[e.t] || 0) + 1; }); var tk = Object.keys(types);
    if (QP.filter !== 'all' && !types[QP.filter]) QP.filter = 'all';
    var shown = QP.filter === 'all' ? sel : sel.filter(function(e){ return e.t === QP.filter; });
    var N = TC.TYPE_NAMES, label = QP.filter === 'all' ? (tk.length === 1 ? N[tk[0]] + ' (' + sel.length + ')' : 'Tất cả (' + TC.n(sel.length) + ')') : N[QP.filter] + ' (' + types[QP.filter] + ')';
    var fresh = !QP.el, el = QP.el || (QP.el = TC.el('<div class="qp" role="dialog" aria-label="Thuộc tính nhanh"></div>'));
    var rows = qpRows(d, shown), mixed = QP.filter === 'all' && tk.length > 1;
    el.innerHTML = '<div class="qp-h"><button class="dd grow" type="button" data-qp="type" aria-haspopup="menu" aria-label="Loại đối tượng"><span class="lbl">' + esc(label) + '</span>' + ic('down', 'xs') + '</button>' +
      '<button class="ib sm" type="button" data-qp="full" aria-label="Mở palette Thuộc tính" data-tip="Mở palette Thuộc tính" data-key="Ctrl+1">' + ic('more') + '</button>' +
      '<button class="ib sm" type="button" data-qp="set" aria-label="Thiết lập Thuộc tính nhanh" data-tip="Thiết lập">' + ic('gear') + '</button>' +
      '<button class="ib sm" type="button" data-qp="x" aria-label="Đóng (Esc)" data-tip="Đóng" data-key="Esc">' + ic('x') + '</button></div>' +
      '<div class="qp-b">' + rows.map(rowHTML).join('') + (mixed ? '<div class="qp-note">Chọn một loại ở trên để thấy trường riêng của loại đó.</div>' : '') + '</div>' +
      '<div class="qp-c" aria-hidden="true">' + rows.length + ' thuộc tính · rê chuột để mở</div>';
    el._shown = shown;
    if (fresh){ $('#cv').appendChild(el); bindQP(el); }
    if (fresh || QP.loc === 'cursor') place(el);
    el.classList.remove('col'); armCollapse();
  };
  function place(el){
    var cv = $('#cv'), W = cv.clientWidth, Hh = cv.clientHeight, w = el.offsetWidth || 252, h = el.offsetHeight || 140, x, y;
    if (QP.loc === 'float' && QP.pos){ x = QP.pos[0]; y = QP.pos[1]; }
    else if (QP.loc === 'float'){ x = W - w - 14; y = 40; }
    else { var m = V().mouse || [W / 2, Hh / 2]; x = m[0] + 22; y = m[1] - h - 18; if (x + w > W - 8) x = m[0] - w - 22; if (y < 34) y = m[1] + 22; } // góc phần tư trên phải của con trỏ, lật khi sát mép
    el.style.left = TC.clamp(x, 6, W - w - 6) + 'px'; el.style.top = TC.clamp(y, 30, Hh - h - 6) + 'px';
  }
  var colT = 0;
  function armCollapse(){ clearTimeout(colT); if (!QP.auto || !QP.el) return; colT = setTimeout(function(){ if (QP.el && !QP.el.matches(':hover') && !QP.el.contains(document.activeElement)) QP.el.classList.add('col'); }, 3000); }
  QP.hide = function(){ clearTimeout(colT); if (QP.el){ QP.el.remove(); QP.el = null; } QP.force = false; };
  function bindQP(el){
    el.addEventListener('mouseenter', function(){ clearTimeout(colT); el.classList.remove('col'); });
    el.addEventListener('mouseleave', armCollapse);
    el.addEventListener('mousedown', function(e){ e.stopPropagation(); if (QP.loc !== 'float' || !e.target.closest('.qp-h') || e.target.closest('button')) return; var sx = e.clientX, sy = e.clientY, ox = el.offsetLeft, oy = el.offsetTop; function mv(ev){ el.style.left = (ox + ev.clientX - sx) + 'px'; el.style.top = (oy + ev.clientY - sy) + 'px'; } function up(){ removeEventListener('mousemove', mv); removeEventListener('mouseup', up); QP.pos = [el.offsetLeft, el.offsetTop]; TC.save('qpPos', QP.pos); } addEventListener('mousemove', mv); addEventListener('mouseup', up); e.preventDefault(); });
    el.addEventListener('dblclick', function(e){ e.stopPropagation(); });
    el.addEventListener('keydown', function(e){ if (e.key === 'Escape' && !e.target.closest('input')){ e.stopPropagation(); QP.hide(); $('#cv').focus(); } });
    el.addEventListener('click', function(e){
      var b = e.target.closest('[data-qp]'), d = D();
      if (b){ var k = b.dataset.qp, r = b.getBoundingClientRect();
        if (k === 'x'){ QP.hide(); return; }
        if (k === 'full'){ QP.hide(); A().showPalette('props'); return; }
        if (k === 'set'){ qpMenu(r.right, r.bottom + 2, true); return; }
        if (k === 'type'){ var types = {}; d.sel.forEach(function(id){ var x = d.byId[id]; if (x) types[x.t] = (types[x.t] || 0) + 1; }); TC.menu([{ label:'Tất cả (' + TC.n(d.sel.length) + ')', check:QP.filter === 'all', act:function(){ QP.filter = 'all'; QP.show(); } }].concat(Object.keys(types).map(function(t){ return { label:TC.TYPE_NAMES[t] + ' (' + types[t] + ')', check:QP.filter === t, act:function(){ QP.filter = t; QP.show(); } }; })), r.left, r.bottom + 2, { minWidth:r.width }); return; }
      }
      var row = e.target.closest('.pr.edt'); if (row && !e.target.closest('input')) TC.propEdit(row, el._shown, d, dummy);
    });
  }
  function qpMenu(x, y, right){
    TC.menu([{ h:'Thuộc tính nhanh' }, { label:'Bật khi chọn đối tượng', kbd:'Ctrl+Shift+P', check:!!TC.S.toggles.qp, act:function(){ A().toggle('qp'); } }, { sep:1 }, { label:'Vị trí: theo con trỏ', check:QP.loc === 'cursor', act:function(){ QP.loc = 'cursor'; TC.save('qpLoc', 'cursor'); QP.show(); } }, { label:'Vị trí: cố định (kéo thanh tiêu đề để dời)', check:QP.loc === 'float', act:function(){ QP.loc = 'float'; TC.save('qpLoc', 'float'); if (QP.el){ QP.el.remove(); QP.el = null; } QP.show(); } }, { label:'Tự thu gọn khi không dùng', check:QP.auto, act:function(){ QP.auto = !QP.auto; TC.save('qpAuto', QP.auto); armCollapse(); } }, { sep:1 }, { label:'Chọn trường cho từng loại đối tượng… <span class="faint">(sau v1.0)</span>', dis:true }, { label:'Mở palette Thuộc tính', icon:'props', kbd:'Ctrl+1', act:function(){ A().showPalette('props'); } }], x, y, { alignRight:!!right, up:!right, minWidth:270 });
  }
  TC.qpMenu = qpMenu;
  TC.on('sel', function(){ setTimeout(function(){ var d = D(); if (!d || !d.sel.length){ QP.hide(); return; } if (TC.S.toggles.qp || QP.force) QP.show(); }, 0); });
  TC.on('doc', function(){ QP.hide(); });
  ['space', 'edition'].forEach(function(ev){ TC.on(ev, function(){ QP.hide(); }); });
  var run0 = TC.cmd.run; TC.cmd.run = function(n){ if (!/^'/.test(String(n))) QP.hide(); return run0.apply(this, arguments); };
  // nhấp đúp vào đường thẳng, cung, tròn… mở Thuộc tính nhanh (07.1 §9.10)
  var dbl0 = TC.cad.hook.dbl;
  TC.cad.hook.dbl = function(e, m){
    if (e && !TC.isViewer() && /^(line|arc|circle|ellipse|point|xline|ray)$/.test(e.t) && !(D().layerMap[e.layer] || {}).lck){ var d = D(); d.sel = [e.id]; TC.cad.afterSel(); QP.show(true); return true; }
    return dbl0 ? dbl0(e, m) : false;
  };
  OPEN['UI-C02'] = function(){ var d = SC.cat.plan(); if (!TC.S.toggles.qp) A().toggle('qp'); SC.cat.flash('[data-tgl="qp"]'); setTimeout(function(){ var e = d.ents.filter(function(x){ return x.t === 'line' && x.layer === 'KT-TUONG'; })[3]; if (!e) return; var v = V(), s = v.toS ? v.toS([(e.p1[0] + e.p2[0]) / 2, (e.p1[1] + e.p2[1]) / 2]) : null; if (s) v.mouse = s; d.sel = [e.id]; TC.cad.afterSel(); TC.toast('Thuộc tính nhanh bật ở thanh trạng thái (Ctrl+Shift+P). Chọn đối tượng là popup hiện cạnh con trỏ; nhấp đúp vào đường thẳng, cung, tròn cũng mở popup.', { type:'info', log:false, ms:8000 }); }, 120); };

  /* ================= Biến hệ thống UI-K13 ================= */
  // [tên, kiểu, mặc định, nơi lưu, mô tả, miền] · kiểu: i số nguyên, r số thực, s chuỗi, b 0/1, e liệt kê, bit mã bit
  var W_D = 'Bản vẽ', W_U = 'Người dùng', W_N = 'Không lưu';
  var DEFS = [
    ['ANGBASE', 'r', 0, W_D, 'Hướng của góc 0 (độ, đo từ trục X)'], ['ANGDIR', 'e', 0, W_D, 'Chiều dương của góc', { 0:'Ngược chiều kim đồng hồ', 1:'Thuận chiều kim đồng hồ' }],
    ['ATTDIA', 'b', 0, W_U, 'Nhập attribute bằng hộp thoại khi chèn block'], ['ATTREQ', 'b', 1, W_U, 'Hỏi giá trị attribute khi chèn block'],
    ['AUNITS', 'e', 0, W_D, 'Đơn vị góc', { 0:'Độ thập phân', 1:'Độ/phút/giây', 2:'Grad', 3:'Radian', 4:'Đơn vị trắc địa' }], ['AUPREC', 'i', 0, W_D, 'Số chữ số thập phân của góc', [0, 8]],
    ['CANNOSCALE', 's', '1:100', W_D, 'Tỉ lệ chú thích hiện hành'], ['CECOLOR', 's', 'BYLAYER', W_D, 'Màu cho đối tượng vẽ mới'], ['CELTSCALE', 'r', 1, W_D, 'Tỉ lệ linetype cho đối tượng mới', [0]],
    ['CELTYPE', 's', 'BYLAYER', W_D, 'Linetype cho đối tượng vẽ mới'], ['CELWEIGHT', 'i', -1, W_D, 'Độ dày nét cho đối tượng mới (−1 ByLayer, −2 ByBlock, −3 mặc định)', [-3, 211]],
    ['CLAYER', 's', '0', W_D, 'Layer hiện hành'], ['CMDECHO', 'b', 1, W_N, 'Hiện lệnh và prompt khi LISP gọi (command …)'],
    ['DIMSCALE', 'r', 100, W_D, 'Tỉ lệ tổng thể của kích thước', [0]], ['DIMSTYLE', 's', 'TCVN-1-100', W_D, 'Kiểu kích thước hiện hành (chỉ đọc, đổi bằng DIMSTYLE)', null, true],
    ['DYNMODE', 'i', 3, W_U, 'Nhập động: 0 tắt, 1 nhập con trỏ, 2 nhập kích thước, 3 cả hai', [-3, 3]], ['FIELDDISPLAY', 'b', 1, W_U, 'Nền xám cho field trên bản vẽ (không in ra)'], ['FILEDIA', 'b', 1, W_U, 'Hiện hộp thoại chọn tệp (0: hỏi tên tệp trên dòng lệnh)'],
    ['FILLMODE', 'b', 1, W_D, 'Tô đặc hatch, solid và polyline có bề rộng'], ['GRIDMODE', 'b', 0, W_D, 'Hiện lưới'],
    ['INSUNITS', 'e', 4, W_D, 'Đơn vị của nội dung chèn vào', { 0:'Không đơn vị', 1:'Inch', 2:'Feet', 4:'Milimét', 5:'Xentimét', 6:'Mét' }], ['LTSCALE', 'r', 100, W_D, 'Tỉ lệ linetype toàn cục', [0]],
    ['LUNITS', 'e', 2, W_D, 'Đơn vị độ dài', { 1:'Khoa học', 2:'Thập phân', 3:'Kỹ thuật', 4:'Kiến trúc', 5:'Phân số' }], ['LUPREC', 'i', 2, W_D, 'Số chữ số thập phân của độ dài', [0, 8]],
    ['LWDISPLAY', 'b', 0, W_D, 'Hiển thị độ dày nét'], ['MEASUREMENT', 'e', 1, W_D, 'Hệ đo cho mẫu hatch và linetype', { 0:'Anh (inch)', 1:'Mét' }],
    ['MIRRTEXT', 'b', 0, W_D, 'Lật chữ khi đối xứng (MIRROR)'], ['MODEMACRO', 's', '', W_N, 'Chuỗi DIESEL hiện ở ô tùy biến trên thanh trạng thái'], ['MOVEBAK', 's', '', W_U, 'Thư mục chứa tệp .bak khi lưu (MOVEBAK)'],
    ['ORTHOMODE', 'b', 0, W_D, 'Chế độ Ortho'], ['OSMODE', 'bit', 255, W_U, 'Chế độ bắt điểm đang bật (mã bit, 16384 = tắt tạm)', [0, 32767]],
    ['PDMODE', 'i', 0, W_D, 'Kiểu hiển thị của điểm (DDPTYPE)', [0, 100]], ['PDSIZE', 'r', 0, W_D, 'Cỡ hiển thị của điểm'],
    ['PEDITACCEPT', 'b', 0, W_U, 'PEDIT tự chuyển line, arc thành polyline, không hỏi'], ['PICKADD', 'e', 2, W_U, 'Cách cộng dồn vùng chọn', { 0:'Thay vùng chọn', 1:'Cộng dồn', 2:'Cộng dồn, giữ khi gọi SELECT' }],
    ['PICKBOX', 'i', 3, W_U, 'Cỡ ô chọn (pixel)', [0, 50]], ['PICKFIRST', 'b', 1, W_U, 'Chọn trước rồi gọi lệnh'],
    ['PICKSTYLE', 'e', 1, W_U, 'Chọn theo nhóm, hatch liên kết', { 0:'Không', 1:'Chọn theo nhóm', 2:'Hatch liên kết', 3:'Cả hai' }], ['PLINEWID', 'r', 0, W_D, 'Bề rộng mặc định của polyline', [-1]],
    ['QPLOCATION', 'e', 0, W_U, 'Vị trí của Thuộc tính nhanh', { 0:'Theo con trỏ', 1:'Cố định' }], ['QPMODE', 'e', 0, W_U, 'Thuộc tính nhanh khi chọn đối tượng', { 0:'Tắt', 1:'Bật cho mọi đối tượng' }],
    ['SAVETIME', 'i', 10, W_U, 'Số phút giữa hai lần tự lưu (0: tắt)', [0, 600]], ['SELECTIONCYCLING', 'e', 0, W_U, 'Chọn luân phiên khi đối tượng chồng nhau', { 0:'Tắt', 1:'Bật, không hiện danh sách', 2:'Bật, hiện danh sách' }],
    ['SNAPMODE', 'b', 0, W_D, 'Chế độ Snap'], ['TEXTSIZE', 'r', 250, W_D, 'Chiều cao chữ mặc định', [0]], ['TEXTSTYLE', 's', 'Standard', W_D, 'Kiểu chữ hiện hành'],
    ['TRIMEXTENDMODE', 'e', 1, W_U, 'Chế độ của TRIM, EXTEND', { 0:'Chuẩn (chọn cạnh cắt)', 1:'Nhanh' }], ['UCSICON', 'bit', 1, W_D, 'Biểu tượng UCS: 1 bật, 2 hiện tại gốc, 3 cả hai', [0, 3]],
    ['VISRETAIN', 'b', 1, W_D, 'Giữ thay đổi layer của xref khi nạp lại'], ['XREFNOTIFY', 'e', 2, W_U, 'Báo xref thay đổi hoặc thiếu', { 0:'Không báo', 1:'Báo thiếu', 2:'Báo thiếu và thay đổi' }],
    ['ZOOMFACTOR', 'i', 60, W_U, 'Mức zoom mỗi nấc lăn chuột', [3, 100]],
    ['TCFONTDOC', 'e', 1, W_U, 'TrionCAD · Bác sĩ Font khi mở bản vẽ', { 0:'Tắt', 1:'Banner', 2:'Chỉ chỉ báo trên thanh trạng thái' }], ['TCPROMPTLANG', 'e', 0, W_U, 'TrionCAD · Ngôn ngữ prompt trên dòng lệnh', { 0:'Tiếng Việt', 1:'English' }]
  ];
  var DEF = {}; DEFS.forEach(function(r){ DEF[r[0]] = { n:r[0], t:r[1], def:r[2], where:r[3], desc:r[4], dom:r[5] || null, ro:!!r[6] }; });
  var OSB = { end:1, mid:2, cen:4, node:8, quad:16, int:32, ins:64, per:128, tan:256, nea:512, app:2048, ext:4096, par:8192 };
  // biến gắn với trạng thái thật của prototype; các biến khác lưu theo nơi lưu
  var LIVE = {
    ORTHOMODE:['ortho'], GRIDMODE:['grid'], SNAPMODE:['snap'], LWDISPLAY:['lw'], QPMODE:['qp'],
    DYNMODE:{ get:function(){ return TC.S.toggles.dyn ? 3 : -3; }, set:function(v){ if (!!TC.S.toggles.dyn !== (v > 0)) A().toggle('dyn'); } },
    OSMODE:{ get:function(){ var M = TC.S.osnap, v = 0; Object.keys(OSB).forEach(function(k){ if (M[k]) v |= OSB[k]; }); return TC.S.toggles.osnap ? v : v | 16384; }, set:function(v){ var M = TC.S.osnap; Object.keys(OSB).forEach(function(k){ M[k] = !!(v & OSB[k]); }); TC.save('osnap', M); if (!!TC.S.toggles.osnap === !!(v & 16384)) A().toggle('osnap'); } },
    CLAYER:{ get:function(){ var d = D(); return d && ED ? ED.clayer(d) : '0'; }, set:function(v){ return ED.setCurLayer(v) ? true : 'Không có layer “' + v + '” hoặc layer đang đóng băng.'; } },
    MIRRTEXT:{ get:function(){ return TC.S.mirrtext || 0; }, set:function(v){ TC.S.mirrtext = v; } },
    FIELDDISPLAY:{ get:function(){ return TC.S.fieldBg !== false ? 1 : 0; }, set:function(v){ TC.S.fieldBg = !!v; TC.save('fieldbg', TC.S.fieldBg); var d = D(); if (d){ TC.prepDoc(d); V().draw(); } } },
    QPLOCATION:{ get:function(){ return QP.loc === 'float' ? 1 : 0; }, set:function(v){ QP.loc = v ? 'float' : 'cursor'; TC.save('qpLoc', QP.loc); } },
    CANNOSCALE:{ get:function(){ var d = D(); return d && d.annoScale || '1:100'; }, set:function(v){ var d = D(); if (!/^\d+(\.\d+)?:\d+(\.\d+)?$/.test(v)) return 'Tỉ lệ có dạng 1:100.'; d.annoScale = v; } },
    TEXTSTYLE:{ get:function(){ var d = D(); return d && ED ? ED.cur(d).tstyle : 'Standard'; }, set:function(v){ var d = D(); if (!d.styleMap[v]) return 'Không có kiểu chữ “' + v + '”.'; ED.cur(d).tstyle = v; } },
    TEXTSIZE:{ get:function(){ var d = D(); return d && ED ? ED.cur(d).th || 250 : 250; }, set:function(v){ ED.cur(D()).th = v; } },
    DIMSTYLE:{ get:function(){ var d = D(); return d && ED ? ED.cur(d).dimstyle : 'TCVN-1-100'; } },
    UCSICON:{ get:function(){ return (TC.S.ucsIcon !== false ? 1 : 0) | (TC.S.ucsOrigin ? 2 : 0); }, set:function(v){ TC.S.ucsIcon = !!(v & 1); TC.S.ucsOrigin = !!(v & 2); if (TC.ucsIconApply) TC.ucsIconApply(); } }
  };
  var userSV = TC.load('sv', {}), sessSV = {};
  var SV = TC.SV = { defs:DEF, list:DEFS.map(function(r){ return r[0]; }), log:{},
    def:function(n){ return DEF[String(n).toUpperCase()] || null; },
    get:function(n){ n = n.toUpperCase(); var df = DEF[n], L = LIVE[n]; if (!df) return null; if (Array.isArray(L)) return TC.S.toggles[L[0]] ? 1 : 0; if (L && L.get) return L.get(); var d = D(), store = df.where === W_D ? (d && d.sysvars || {}) : df.where === W_U ? userSV : sessSV; return n in store ? store[n] : df.def; },
    // kiểm tra kiểu và miền giá trị; trả { v } hoặc { err }
    parse:function(n, raw){ var df = DEF[n], s = String(raw).trim(); if (df.ro) return { err:n + ' chỉ đọc.' };
      if (df.t === 's') return { v:s };
      if (!/^-?\d+(\.\d+)?$/.test(s)) return { err:'Cần một số' + (df.t === 'r' ? '' : ' nguyên') + '. Dấu thập phân là dấu chấm.' };
      var v = Number(s); if (df.t !== 'r' && v !== Math.round(v)) return { err:'Cần một số nguyên.' };
      if (df.t === 'b' && v !== 0 && v !== 1) return { err:'Chỉ nhận 0 hoặc 1.' };
      if (df.t === 'e' && !(String(v) in df.dom)) return { err:'Giá trị hợp lệ: ' + Object.keys(df.dom).map(function(k){ return k + ' (' + df.dom[k] + ')'; }).join(', ') + '.' };
      if (Array.isArray(df.dom)){ if (df.dom.length === 1 && v <= df.dom[0]) return { err:'Phải lớn hơn ' + df.dom[0] + '.' }; if (df.dom.length === 2 && (v < df.dom[0] || v > df.dom[1])) return { err:'Phải từ ' + df.dom[0] + ' đến ' + df.dom[1] + '.' }; }
      return { v:v }; },
    set:function(n, v, src){ n = n.toUpperCase(); var df = DEF[n], L = LIVE[n], old = SV.get(n); if (!df) return 'Không có biến ' + n + '.';
      if (Array.isArray(L)){ if ((TC.S.toggles[L[0]] ? 1 : 0) !== (v ? 1 : 0)) A().toggle(L[0]); }
      else if (L && L.set){ var r = L.set(v); if (typeof r === 'string') return r; }
      else { var d = D(), store = df.where === W_D ? (d.sysvars = d.sysvars || {}) : df.where === W_U ? userSV : sessSV; store[n] = v; if (df.where === W_U) TC.save('sv', userSV); }
      if (String(old) !== String(SV.get(n))){ SV.log[n] = { old:old, src:src || 'SETVAR', t:Date.now() }; TC.emit('sysvar', n); }
      return true; },
    fmt:function(n, v){ var df = DEF[n]; if (v == null) v = SV.get(n); return df.t === 's' ? '"' + v + '"' : String(v); }
  };
  // theo dõi: biến LISP hay đổi rồi quên trả lại (07.3 UI-K13)
  var MON = TC.load('svMon', null) || { on:true, toast:true, list:{ OSMODE:255, CMDECHO:1, FILEDIA:1, PICKFIRST:1, ORTHOMODE:0, ATTREQ:1, ATTDIA:0, PEDITACCEPT:0, MIRRTEXT:0, PICKADD:2 } };
  SV.mon = MON;
  function saveMon(){ TC.save('svMon', MON); }
  SV.drift = function(){ return Object.keys(MON.list).filter(function(n){ return String(SV.get(n)) !== String(MON.list[n]); }); };
  // nhiều biến đổi liền nhau (một LISP) gom thành một thông báo
  var pend = [], pendT = 0;
  TC.on('sysvar', function(n){ if (!(n in MON.list) || !MON.on) return; A().renderStatus(); if (!MON.toast || String(SV.get(n)) === String(MON.list[n]) || /^Khôi phục|^Trình duyệt|^Đặt về/.test((SV.log[n] || {}).src || '')) return; if (pend.indexOf(n) < 0) pend.push(n); clearTimeout(pendT); pendT = setTimeout(function(){ var list = pend.slice(), s = SV.log[list[0]] || {}; pend = [];
    TC.toast(list.length === 1 ? 'Biến <b class="mono">' + list[0] + '</b> vừa đổi ' + esc(SV.fmt(list[0], MON.list[list[0]])) + ' → <b class="mono">' + esc(SV.fmt(list[0])) + '</b>' + (s.src && s.src !== 'SETVAR' ? ' (do ' + esc(s.src) + ')' : '') + '.' : list.length + ' biến hệ thống vừa đổi' + (s.src && s.src !== 'SETVAR' ? ' (do ' + esc(s.src) + ')' : '') + ': <span class="mono">' + list.join(', ') + '</span>.', { type:'warn', log:false, act:{ label:'Khôi phục', fn:function(){ list.forEach(function(v){ SV.set(v, MON.list[v], 'Khôi phục'); }); A().renderStatus(); } } }); }, 250); });
  TC.on('statusrender', function(){
    var dr = MON.on && !TC.isViewer() && A().active !== 'start' ? SV.drift() : [], host = $('#sb-tasks'); if (!host) return;
    var old = $('#sb-sysv'); if (old) old.remove(); if (!dr.length) return;
    host.insertAdjacentHTML('afterend', '<button class="sb-chip warn" type="button" id="sb-sysv" data-tip="Theo dõi biến hệ thống" data-desc="' + esc(dr.join(', ')) + ' khác giá trị chuẩn. Bấm để xem giá trị cũ, mới và khôi phục.">' + ic('warn') + TC.n(dr.length) + ' biến đã đổi</button>');
  });
  document.addEventListener('click', function(e){ var b = e.target.closest('#sb-sysv'); if (b) driftPop(b); });
  function driftPop(b){
    var r = b.getBoundingClientRect(), dr = SV.drift();
    var pop = TC.el('<div class="menu svpop" role="dialog" aria-label="Biến hệ thống đã đổi"><div class="mh">Biến hệ thống đã đổi</div>' + dr.map(function(n){ var s = SV.log[n] || {}; return '<div class="svr"><span class="mono"><b>' + n + '</b></span><span class="mono faint">' + esc(SV.fmt(n, MON.list[n])) + ' → </span><span class="mono" style="color:var(--c-warn)">' + esc(SV.fmt(n)) + '</span><span class="faint svsrc">' + esc(s.src && s.src !== 'SETVAR' ? 'do ' + s.src : s.src ? 'đổi bằng SETVAR' : '') + '</span><button class="btn sm" type="button" data-svr="' + n + '">Khôi phục</button></div>'; }).join('') +
      '<div class="ms"></div><div class="hrow" style="padding:4px 6px"><button class="btn sm primary" type="button" data-svr="*">Khôi phục tất cả</button><span class="grow"></span><button class="btn sm ghost" type="button" data-svr="open">Theo dõi biến…</button></div></div>');
    document.body.appendChild(pop); pop.style.left = Math.max(8, Math.min(r.left, innerWidth - pop.offsetWidth - 8)) + 'px'; pop.style.top = (r.top - pop.offsetHeight - 6) + 'px';
    function close(){ pop.remove(); document.removeEventListener('mousedown', out, true); }
    function out(e){ if (!pop.contains(e.target)) close(); }
    document.addEventListener('mousedown', out, true);
    pop.addEventListener('click', function(e){ var x = e.target.closest('[data-svr]'); if (!x) return; var k = x.dataset.svr; close(); if (k === 'open'){ TC.cmd.run('SYSVARMONITOR'); return; } (k === '*' ? dr : [k]).forEach(function(n){ SV.set(n, MON.list[n], 'Khôi phục'); }); A().renderStatus(); TC.cmdline.msg('Đã khôi phục ' + (k === '*' ? dr.length + ' biến' : k) + ' về giá trị chuẩn.'); });
  }
  // SETVAR, và gõ thẳng tên biến trên dòng lệnh (EA)
  IMPL.SETVAR = function(ctx, pre){
    var ask = function(){ return pre ? Promise.resolve(pre) : ctx.val('Tên biến hoặc [Liệt kê · ?]', ''); };
    return ask().then(function(n){ n = String(n || '').trim().toUpperCase(); if (!n) return;
      if (n === '?'){ SV.list.forEach(function(k){ ctx.out('<span class="mono">' + k + '</span> = <span class="mono">' + esc(SV.fmt(k)) + '</span> · ' + esc(DEF[k].desc), 'muted'); }); return; }
      var df = DEF[n]; if (!df){ ctx.out('<span class="err">Không có biến hệ thống “' + esc(n) + '”.</span> Gõ ? để liệt kê, hoặc mở trình duyệt biến (SYSVDLG).'); return; }
      if (df.ro){ ctx.out('<b class="mono">' + n + '</b> = <span class="mono">' + esc(SV.fmt(n)) + '</span> (chỉ đọc) · ' + esc(df.desc)); return; }
      var loop = function(){ return ctx.val('Giá trị mới cho ' + n + ' <' + SV.fmt(n) + '>', '').then(function(v){ if (v === '' || v == null) return; var p = SV.parse(n, v); if (p.err){ ctx.out('<span class="err">' + esc(p.err) + '</span>'); return loop(); } var r = SV.set(n, p.v, 'SETVAR'); if (r !== true){ ctx.out('<span class="err">' + esc(r) + '</span>'); return loop(); } ctx.out('<b class="mono">' + n + '</b> = <span class="mono">' + esc(SV.fmt(n)) + '</span>'); A().renderStatus(); }); };
      return loop(); });
  };
  IMPL.SYSVDLG = function(){ SC.sysvars('all'); };
  IMPL.SYSVARMONITOR = function(){ SC.sysvars('mon'); };
  SC.sysvars = function(tab, filt){
    var st = { q:'', f:filt || 'all', sel:null, tab:tab || 'all' };
    var dlg = TC.dialog({ id:'sysv', title:'Biến hệ thống', cmd:tab === 'mon' ? 'SYSVARMONITOR' : 'SYSVDLG', code:'UI-K13', w:940, h:600, flush:true,
      body:'<div class="svw">' + H.tabs([['all', 'Tất cả biến'], ['mon', 'Theo dõi']], st.tab, 'svt') + '<div class="svp" id="sv-all"' + (st.tab === 'all' ? '' : ' hidden') + '>' +
        '<div class="pal-tools"><label class="search" style="width:300px">' + ic('search') + '<input id="sv-q" placeholder="Tên hoặc mô tả: OSMODE, bắt điểm, tự lưu…" aria-label="Tìm biến"></label><span class="seg" id="sv-f"></span><span class="grow"></span><button class="btn sm" type="button" data-sva="imp">' + ic('open') + 'Nhập…</button><button class="btn sm" type="button" data-sva="exp">' + ic('saveas') + 'Xuất…</button></div>' +
        '<div class="svmain"><div class="tblwrap" id="sv-t"></div><div class="svd" id="sv-d"></div></div></div>' +
        '<div class="svp" id="sv-mon"' + (st.tab === 'mon' ? '' : ' hidden') + '></div></div>',
      foot:'<span class="faint" id="sv-ft" style="font-size:var(--fs-s)"></span><span class="grow"></span><button class="btn primary" type="button" data-x="close">Đóng</button>' });
    var el = dlg.el;
    function rowsOf(){ var q = TC.fold(st.q.trim()), dr = SV.drift(); return SV.list.filter(function(n){ var df = DEF[n], v = SV.get(n); if (st.f === 'chg' && String(v) === String(df.def)) return false; if (st.f === 'mon' && !(n in MON.list)) return false; return !q || TC.fold(n + ' ' + df.desc).indexOf(q) >= 0; }); }
    function renderAll(){
      var list = rowsOf(), nChg = SV.list.filter(function(n){ return String(SV.get(n)) !== String(DEF[n].def); }).length;
      $('#sv-f', el).innerHTML = [['all', 'Tất cả (' + SV.list.length + ')'], ['chg', 'Khác mặc định (' + nChg + ')'], ['mon', 'Đang theo dõi (' + Object.keys(MON.list).length + ')']].map(function(f){ return '<button type="button" data-svf="' + f[0] + '" aria-pressed="' + (st.f === f[0]) + '">' + f[1] + '</button>'; }).join('');
      $('#sv-t', el).innerHTML = list.length ? '<table class="tbl svt"><colgroup><col style="width:150px"><col style="width:132px"><col style="width:104px"><col style="width:96px"><col></colgroup><thead><tr><th>Tên</th><th>Giá trị</th><th>Mặc định</th><th>Lưu ở</th><th>Mô tả</th></tr></thead><tbody>' + list.map(function(n){ var df = DEF[n], v = SV.get(n), chg = String(v) !== String(df.def); return '<tr data-svn="' + n + '" class="' + (st.sel === n ? 'is-sel' : '') + '"><td class="mono">' + (n in MON.list ? '<span class="svmon" title="Đang theo dõi">' + ic('eye', 's12') + '</span>' : '') + n + '</td><td class="mono svv' + (chg ? ' chg' : '') + '" data-svv="' + n + '"' + (df.ro ? ' title="Chỉ đọc"' : ' title="Bấm để sửa"') + '>' + esc(SV.fmt(n, v)) + (df.t === 'e' ? ' <span class="faint">' + esc(df.dom[v] || '') + '</span>' : '') + '</td><td class="mono faint">' + esc(SV.fmt(n, df.def)) + '</td><td>' + df.where + '</td><td>' + esc(df.desc) + '</td></tr>'; }).join('') + '</tbody></table>'
        : '<div class="empty">' + ic('search') + '<b>Không có biến khớp “' + esc(st.q) + '”</b>Thử tên tiếng Anh (OSMODE) hoặc mô tả tiếng Việt (bắt điểm).</div>';
      $('#sv-ft', el).textContent = 'Hiện ' + list.length + '/' + SV.list.length + ' biến · bấm vào giá trị để sửa · biến đổi giá trị có chấm màu';
      detail();
    }
    function detail(){
      var n = st.sel, box = $('#sv-d', el); if (!n){ box.innerHTML = '<div class="empty">' + ic('gear') + '<b>Chọn một biến</b>Xem kiểu, miền giá trị và đặt lại mặc định.</div>'; return; }
      var df = DEF[n], v = SV.get(n), kind = { i:'Số nguyên', r:'Số thực', s:'Chuỗi', b:'Bật/tắt (0, 1)', e:'Liệt kê', bit:'Mã bit (cộng các giá trị)' }[df.t], rng = Array.isArray(df.dom) ? (df.dom.length === 1 ? '> ' + df.dom[0] : df.dom[0] + ' – ' + df.dom[1]) : '';
      var body = '';
      if (df.t === 'e') body = '<div class="stack" style="gap:2px">' + Object.keys(df.dom).map(function(k){ return '<label class="chk"><input type="radio" name="sv-e" value="' + k + '"' + (String(v) === k ? ' checked' : '') + '> <span class="mono">' + k + '</span> · ' + esc(df.dom[k]) + '</label>'; }).join('') + '</div>';
      else if (n === 'OSMODE') body = '<div class="svbits">' + Object.keys(OSB).map(function(k){ return '<label class="chk"><input type="checkbox" data-bit="' + OSB[k] + '"' + (v & OSB[k] ? ' checked' : '') + '> ' + TC.SNAP_NAMES[k] + ' <span class="mono faint">' + OSB[k] + '</span></label>'; }).join('') + '<label class="chk"><input type="checkbox" data-bit="16384"' + (v & 16384 ? ' checked' : '') + '> Tắt tạm mọi chế độ (F3) <span class="mono faint">16384</span></label></div>';
      else if (df.t === 'b') body = '<label class="chk"><input type="checkbox" id="sv-b"' + (v ? ' checked' : '') + '> Bật (1)</label>';
      box.innerHTML = '<div class="svh"><b class="mono">' + n + '</b>' + (n in MON.list ? '<span class="tag info">Đang theo dõi</span>' : '') + '</div><p>' + esc(df.desc) + '</p>' +
        '<dl class="kv"><dt>Kiểu</dt><dd>' + kind + (rng ? ' · ' + rng : '') + '</dd><dt>Giá trị</dt><dd class="mono">' + esc(SV.fmt(n, v)) + '</dd><dt>Mặc định</dt><dd class="mono">' + esc(SV.fmt(n, df.def)) + '</dd><dt>Lưu ở</dt><dd>' + df.where + (df.where === W_U ? ' (hồ sơ người dùng)' : df.where === W_D ? ' (trong tệp DWG)' : ' (mất khi đóng TrionCAD)') + '</dd></dl>' +
        (df.ro ? '<p class="note">Biến chỉ đọc.</p>' : body + (df.t !== 'e' && df.t !== 'b' && n !== 'OSMODE' ? '<div class="field"><label for="sv-in">Giá trị mới</label><input class="inp mono" id="sv-in" value="' + esc(v) + '" spellcheck="false"><span class="fmsg" id="sv-m"></span></div>' : '') +
          '<div class="hrow" style="margin-top:8px"><button class="btn sm" type="button" data-sva="def"' + (String(v) === String(df.def) ? ' disabled' : '') + '>' + ic('undo') + 'Đặt về mặc định</button><button class="btn sm ghost" type="button" data-sva="mon">' + ic(n in MON.list ? 'eyeoff' : 'eye') + (n in MON.list ? 'Bỏ theo dõi' : 'Theo dõi biến này') + '</button></div>');
    }
    function commit(n, raw){ var p = SV.parse(n, raw), m = $('#sv-m', el), inp = $('#sv-in', el); if (p.err){ if (m){ m.className = 'fmsg err'; m.innerHTML = ic('warn', 'xs') + esc(p.err); } if (inp) inp.classList.add('err'); return false; } var r = SV.set(n, p.v, 'Trình duyệt biến'); if (r !== true){ if (m){ m.className = 'fmsg err'; m.innerHTML = ic('warn', 'xs') + esc(r); } return false; } A().renderStatus(); renderAll(); renderMon(); return true; }
    function cellEdit(td){ var n = td.dataset.svv, df = DEF[n]; if (df.ro) return; if (df.t === 'e' || df.t === 'b'){ var r = td.getBoundingClientRect(), items = df.t === 'b' ? { 0:'Tắt', 1:'Bật' } : df.dom; TC.menu(Object.keys(items).map(function(k){ return { label:'<span class="mono">' + k + '</span> · ' + esc(items[k]), check:String(SV.get(n)) === k, act:function(){ commit(n, k); } }; }), r.left, r.bottom + 1, { minWidth:r.width + 120 }); return; }
      var inp = TC.el('<input class="pinp mono" spellcheck="false">'); inp.value = SV.get(n); td.innerHTML = ''; td.appendChild(inp); inp.focus(); inp.select();
      var done = false; function fin(ok){ if (done) return; done = true; if (ok && String(inp.value) !== String(SV.get(n)) && !commit(n, inp.value)){ done = false; inp.classList.add('err'); TC.toast(esc(SV.parse(n, inp.value).err || 'Giá trị chưa hợp lệ.'), { type:'warn', log:false }); inp.focus(); return; } renderAll(); }
      inp.addEventListener('keydown', function(e){ e.stopPropagation(); if (e.key === 'Enter'){ e.preventDefault(); fin(true); } else if (e.key === 'Escape'){ e.preventDefault(); fin(false); } }); inp.addEventListener('blur', function(){ setTimeout(function(){ fin(true); }, 0); }); }
    function renderMon(){
      var names = Object.keys(MON.list), dr = SV.drift();
      $('#sv-mon', el).innerHTML = '<div class="pal-note">' + ic('info') + '<span>LISP và script hay đổi biến (tắt bắt điểm, tắt hộp thoại tệp…) rồi quên trả lại. TrionCAD so mỗi biến dưới đây với <b>giá trị chuẩn</b>; khi khác, thanh trạng thái hiện cảnh báo để khôi phục.</span></div>' +
        '<div class="pal-tools">' + H.chk('mon-on', 'Báo trên thanh trạng thái khi một biến bị đổi', MON.on) + H.chk('mon-toast', 'Hiện thông báo ngay khi đổi', MON.toast) + '<span class="grow"></span><button class="btn sm" type="button" data-sva="add">' + ic('plus') + 'Thêm biến…</button><button class="btn sm" type="button" data-sva="base">Lấy giá trị hiện tại làm chuẩn</button><button class="btn sm primary" type="button" data-sva="all"' + (dr.length ? '' : ' disabled') + '>Khôi phục tất cả (' + dr.length + ')</button></div>' +
        '<div class="tblwrap" style="flex:1;min-height:0;overflow:auto"><table class="tbl"><colgroup><col style="width:150px"><col style="width:110px"><col style="width:110px"><col><col style="width:170px"></colgroup><thead><tr><th>Biến</th><th>Giá trị chuẩn</th><th>Hiện tại</th><th>Trạng thái</th><th></th></tr></thead><tbody>' + names.map(function(n){ var bad = dr.indexOf(n) >= 0, s = SV.log[n] || {}; return '<tr><td class="mono">' + n + '</td><td class="mono">' + esc(SV.fmt(n, MON.list[n])) + '</td><td class="mono"' + (bad ? ' style="color:var(--c-warn)"' : '') + '>' + esc(SV.fmt(n)) + '</td><td>' + (bad ? '<span class="tag warn">Đã đổi' + (s.src && s.src !== 'SETVAR' ? ' · do ' + esc(s.src) : '') + '</span>' : '<span class="tag ok">Đúng chuẩn</span>') + '</td><td class="ctr"><button class="btn sm" type="button" data-svm="' + n + '"' + (bad ? '' : ' disabled') + '>Khôi phục</button> <button class="ib sm" type="button" data-svx="' + n + '" aria-label="Bỏ ' + n + ' khỏi danh sách" data-tip="Bỏ khỏi danh sách">' + ic('x') + '</button></td></tr>'; }).join('') + '</tbody></table></div>';
    }
    el.addEventListener('dtab', function(e){ st.tab = e.detail; $('#sv-all', el).hidden = st.tab !== 'all'; $('#sv-mon', el).hidden = st.tab !== 'mon'; if (st.tab === 'mon') renderMon(); });
    $('#sv-q', el).addEventListener('input', function(){ st.q = this.value; renderAll(); });
    el.addEventListener('change', function(e){ var t = e.target, n = st.sel;
      if (t.id === 'mon-on'){ MON.on = t.checked; saveMon(); A().renderStatus(); return; } if (t.id === 'mon-toast'){ MON.toast = t.checked; saveMon(); return; }
      if (t.name === 'sv-e') commit(n, t.value); else if (t.id === 'sv-b') commit(n, t.checked ? 1 : 0);
      else if (t.dataset.bit){ var v = 0; $$('[data-bit]', el).forEach(function(c){ if (c.checked) v |= Number(c.dataset.bit); }); commit('OSMODE', v); } });
    el.addEventListener('keydown', function(e){ if (e.target.id === 'sv-in' && e.key === 'Enter'){ e.preventDefault(); e.stopPropagation(); commit(st.sel, e.target.value); } });
    el.addEventListener('click', function(e){
      var f = e.target.closest('[data-svf]'); if (f){ st.f = f.dataset.svf; renderAll(); return; }
      var cell = e.target.closest('[data-svv]'); if (cell && !e.target.closest('input')){ st.sel = cell.dataset.svv; $$('tr.is-sel', el).forEach(function(r){ r.classList.remove('is-sel'); }); cell.parentNode.classList.add('is-sel'); detail(); cellEdit(cell); return; }
      var tr = e.target.closest('[data-svn]'); if (tr && !e.target.closest('input')){ st.sel = tr.dataset.svn; renderAll(); return; }
      var m = e.target.closest('[data-svm]'); if (m){ SV.set(m.dataset.svm, MON.list[m.dataset.svm], 'Khôi phục'); A().renderStatus(); renderMon(); renderAll(); return; }
      var x = e.target.closest('[data-svx]'); if (x){ delete MON.list[x.dataset.svx]; saveMon(); A().renderStatus(); renderMon(); renderAll(); return; }
      var a = e.target.closest('[data-sva]'); if (!a) return; var k = a.dataset.sva, n = st.sel;
      if (k === 'def'){ SV.set(n, DEF[n].def, 'Đặt về mặc định'); A().renderStatus(); renderAll(); renderMon(); }
      else if (k === 'mon'){ if (n in MON.list) delete MON.list[n]; else MON.list[n] = SV.get(n); saveMon(); A().renderStatus(); renderAll(); }
      else if (k === 'all'){ SV.drift().forEach(function(v){ SV.set(v, MON.list[v], 'Khôi phục'); }); A().renderStatus(); renderMon(); renderAll(); }
      else if (k === 'base'){ Object.keys(MON.list).forEach(function(v){ MON.list[v] = SV.get(v); }); saveMon(); A().renderStatus(); renderMon(); TC.toast('Đã lấy giá trị hiện tại làm chuẩn cho ' + Object.keys(MON.list).length + ' biến.', { log:false }); }
      else if (k === 'add'){ var r = a.getBoundingClientRect(); TC.menu(SV.list.filter(function(v){ return !(v in MON.list) && !DEF[v].ro; }).map(function(v){ return { label:'<span class="mono">' + v + '</span> <span class="faint">· ' + esc(DEF[v].desc.slice(0, 40)) + '</span>', act:function(){ MON.list[v] = SV.get(v); saveMon(); renderMon(); } }; }), r.left, r.bottom + 2, { minWidth:360 }); }
      else if (k === 'exp') TC.toast('Đã xuất ' + SV.list.length + ' biến ra <span class="mono">bien-he-thong.csv</span> (Tên, Giá trị, Mặc định, Lưu ở, Mô tả).', { act:{ label:'Mở thư mục', fn:function(){} } });
      else if (k === 'imp') TC.confirm({ title:'Nhập danh sách biến', msg:'Nhập <b class="mono">bien-chuan-cong-ty.csv</b>: đổi 4 biến khác giá trị hiện tại (OSMODE, LTSCALE, PICKBOX, SAVETIME).', detail:'Biến lưu trong bản vẽ chỉ áp dụng cho bản vẽ đang mở. Biến không có trong TrionCAD được bỏ qua và ghi vào nhật ký.', ok:'Nhập 4 biến' }).then(function(y){ if (!y) return; SV.set('OSMODE', 4133 & ~16384, 'bien-chuan-cong-ty.csv'); SV.set('LTSCALE', 50, 'bien-chuan-cong-ty.csv'); SV.set('PICKBOX', 5, 'bien-chuan-cong-ty.csv'); SV.set('SAVETIME', 5, 'bien-chuan-cong-ty.csv'); A().renderStatus(); renderAll(); TC.toast('Đã nhập 4 biến.', { log:false }); });
    });
    TC.on('sysvar', function(){ if (!dlg.closed){ renderAll(); if (st.tab === 'mon') renderMon(); } });
    renderAll(); if (st.tab === 'mon') renderMon();
    return dlg;
  };
  OPEN['UI-K13'] = function(){ SC.cat.MOD(); SV.set('OSMODE', 0, 'thongkethep.lsp'); SV.set('CMDECHO', 0, 'thongkethep.lsp'); SV.set('FILEDIA', 0, 'thongkethep.lsp'); A().renderStatus(); SC.sysvars('all', 'chg'); setTimeout(function(){ SC.cat.flash('#sb-sysv'); }, 200); TC.toast('Mô phỏng: <span class="mono">thongkethep.lsp</span> đổi OSMODE, CMDECHO, FILEDIA rồi không trả lại. Thanh trạng thái hiện cảnh báo; bấm để khôi phục.', { type:'info', log:false, ms:9000 }); };

  /* ================= Layer: tiện ích chung cho C04, C05, C11 ================= */
  // đổi thuộc tính layer có hoàn tác (chụp cả danh sách layer)
  function layerRun(d, label, fn){
    ED.layerSnap(d);
    ED.run(label, function(){ var done = ED.state(function(){ return { L:d.layers.map(function(l){ return Object.assign({}, l); }), cl:d.clayer }; }, function(s2){ d.layers = s2.L.map(function(l){ return Object.assign({}, l); }); d.layerMap = {}; d.layers.forEach(function(l){ d.layerMap[l.name] = l; }); d.clayer = s2.cl; V().draw(); A().refreshPalettes(); A().renderRibbon(); }); fn(); done(); });
    V().draw(); A().refreshPalettes(); A().renderRibbon(); TC.emit('layers');
  }
  SC.layerRun = layerRun;
  function allEnts(d){ return d.ents.concat.apply(d.ents, d.layouts.map(function(L){ return L.ents; })); }
  function wild(q){ return new RegExp('^' + String(q).replace(/[.+^${}()|[\]\\]/g, '\\$&').replace(/\*/g, '.*').replace(/\?/g, '.') + '$', 'i'); }
  function swatch(c){ return '<i class="sw" style="background:' + (c === 7 ? '#FFFFFF' : TC.ACI[c] || '#FFF') + '"></i>'; }

  /* ================= Trạng thái layer UI-C04 (LAYERSTATE) ================= */
  var LPROPS = [['on', 'Bật/Tắt'], ['frz', 'Đóng băng'], ['lck', 'Khóa'], ['plot', 'In'], ['vpfrz', 'Đóng băng ở VP mới'], ['color', 'Màu'], ['lt', 'Linetype'], ['lw', 'Độ dày nét'], ['transp', 'Trong suốt'], ['pstyle', 'Plot style'], ['vpov', 'Đóng băng trong viewport']];
  function snap(d){ var o = {}; d.layers.forEach(function(l){ o[l.name] = { on:l.on !== false, frz:!!l.frz, lck:!!l.lck, plot:l.plot !== false, vpfrz:!!l.vpfrz, color:l.color, lt:l.lt, lw:l.lw, transp:l.transp || 0 }; }); return o; }
  function states(d){
    if (!d.layerStates){ var base = snap(d), mk = function(name, sp, desc, fn){ var L = JSON.parse(JSON.stringify(base)); Object.keys(L).forEach(function(n){ fn(n, L[n]); }); return { name:name, space:sp, desc:desc, layers:L, date:'14/10/2026' }; };
      d.layerStates = d.id === 'd1' ? [
        mk('In mặt bằng kiến trúc', 'Model', 'Tắt nội thất, kết cấu; khóa trục', function(n, x){ if (/NOITHAT|^KC-|TBVS/.test(n)) x.on = false; if (n === 'KT-TRUC') x.lck = true; }),
        mk('Bố trí nội thất', 'Model', 'Bật nội thất, khóa tường và cột', function(n, x){ if (/^KC-/.test(n)) x.on = false; if (/TUONG|COT/.test(n)) x.lck = true; }),
        mk('Phối hợp kết cấu', 'Model', 'Kết cấu màu nổi, kiến trúc in màu 8', function(n, x){ if (/^KT-/.test(n) && !/TRUC|KHUNG/.test(n)) x.color = 8; if (/NOITHAT|TBVS/.test(n)) x.on = false; }),
        mk('A1 · Mặt bằng (in)', 'A1-Mat bang', 'Trạng thái khi in tờ KT-04', function(n, x){ if (/VIEWPORT|^KC-/.test(n)) x.plot = false; })] : [];
    }
    return d.layerStates;
  }
  function sameAs(d, st){ return d.layers.every(function(l){ var x = st.layers[l.name]; if (!x) return true; return x.on === (l.on !== false) && x.frz === !!l.frz && x.lck === !!l.lck && x.color === l.color; }); }
  SC.layerStates = function(selName){
    var d = D(); if (!d) return; var list = states(d), cur = selName || (list[0] && list[0].name), opt = TC.load('lsOpt', { noXref:true, offMissing:false, asVp:false, props:{ on:1, frz:1, lck:1, plot:1, vpfrz:1, color:1, lt:1, lw:1, transp:1, pstyle:1, vpov:1 } });
    var inVp = d.space !== 'model' && d.activeVp;
    var dlg = TC.dialog({ id:'lstates', title:'Trạng thái layer', cmd:'LAYERSTATE', code:'UI-C04', w:820, h:590,
      body:'<div class="lsw"><div class="lsl"><div class="tblwrap" id="ls-t"></div><div class="lsb">' +
        [['new', 'Mới…', 'plus'], ['upd', 'Cập nhật', 'refresh'], ['edit', 'Sửa…', 'bedit'], ['ren', 'Đổi tên', null], ['del', 'Xóa', 'trash'], '|', ['imp', 'Nhập…', 'open'], ['exp', 'Xuất…', 'saveas']].map(function(b){ return b === '|' ? '<span class="sep"></span>' : '<button class="btn sm" type="button" data-ls="' + b[0] + '">' + (b[2] ? ic(b[2]) : '') + b[1] + '</button>'; }).join('') + '</div></div>' +
        '<div class="stack" style="gap:6px">' + H.chk('ls-noxref', 'Không liệt kê trạng thái layer của xref', opt.noXref) + H.chk('ls-off', 'Khi khôi phục: tắt các layer không có trong trạng thái', opt.offMissing) + H.chk('ls-vp', 'Áp dụng như ghi đè thuộc tính của viewport' + (inVp ? '' : ' <span class="faint">(chỉ khi đang trong một viewport)</span>'), opt.asVp && inVp, !inVp) + '</div>' +
        H.grp('Thuộc tính layer cần khôi phục <span class="grow"></span><button class="btn sm ghost" type="button" data-ls="all">Chọn tất cả</button><button class="btn sm ghost" type="button" data-ls="none">Bỏ chọn</button>', '<div class="lsp">' + LPROPS.map(function(p){ return H.chk('', p[1], !!opt.props[p[0]]).replace('<input', '<input data-lsp="' + p[0] + '"'); }).join('') + '</div>') + '</div>',
      foot:'<span class="faint" id="ls-ft" style="font-size:var(--fs-s)"></span><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Đóng</button><button class="btn primary" type="button" id="ls-go">Khôi phục</button>' });
    var el = dlg.el;
    function save(){ TC.save('lsOpt', opt); }
    function render(){
      list = states(d);
      $('#ls-t', el).innerHTML = list.length ? '<table class="tbl"><colgroup><col style="width:200px"><col style="width:104px"><col style="width:104px"><col></colgroup><thead><tr><th>Tên</th><th>Không gian</th><th>Giống bản vẽ</th><th>Mô tả</th></tr></thead><tbody>' + list.map(function(s){ return '<tr data-lsn="' + esc(s.name) + '" class="' + (s.name === cur ? 'is-sel' : '') + '"><td data-lsname>' + esc(s.name) + '</td><td>' + esc(s.space) + '</td><td>' + (sameAs(d, s) ? '<span class="tag ok">Có</span>' : '<span class="faint">Không</span>') + '</td><td>' + esc(s.desc || '') + '</td></tr>'; }).join('') + '</tbody></table>'
        : '<div class="empty">' + ic('layers') + '<b>Bản vẽ chưa có trạng thái layer</b>Bấm Mới… để lưu bật/tắt, đóng băng, màu… hiện tại của mọi layer, rồi khôi phục bất cứ lúc nào.</div>';
      var s = list.filter(function(x){ return x.name === cur; })[0];
      $$('[data-ls="upd"],[data-ls="edit"],[data-ls="ren"],[data-ls="del"],[data-ls="exp"]', el).forEach(function(b){ b.disabled = !s; });
      $('#ls-go', el).disabled = !s; $('#ls-go', el).textContent = s ? 'Khôi phục “' + s.name + '”' : 'Khôi phục';
      $('#ls-ft', el).textContent = s ? Object.keys(s.layers).length + ' layer trong trạng thái · lưu ngày ' + s.date : list.length + ' trạng thái';
    }
    function restore(s){
      var props = Object.keys(opt.props).filter(function(k){ return opt.props[k]; }), n = 0, vp = inVp && opt.asVp ? d.layouts[d.space].vports.filter(function(q){ return q.id === d.activeVp; })[0] : null;
      if (vp){ vp.frz = d.layers.filter(function(l){ var x = s.layers[l.name]; return x && x.frz; }).map(function(l){ return l.name; }); V().draw(); TC.toast('Đã áp “' + esc(s.name) + '” như ghi đè cho viewport hiện hành.', { log:false }); return; }
      layerRun(d, 'Khôi phục trạng thái layer', function(){ d.layers.forEach(function(l){ var x = s.layers[l.name]; if (!x){ if (opt.offMissing && l.name !== ED.clayer(d)){ l.on = false; n++; } return; } props.forEach(function(k){ if (k in x && !(k === 'frz' && l.name === ED.clayer(d))){ l[k] = x[k]; } }); n++; }); });
      d.curLayerState = s.name; TC.cmdline.msg('Đã khôi phục trạng thái layer <b>' + esc(s.name) + '</b>: ' + n + ' layer.');
    }
    function stName(v, except){ return !v.trim() ? 'Nhập tên.' : list.some(function(x){ return x.name.toLowerCase() === v.trim().toLowerCase() && x.name !== except; }) ? 'Đã có trạng thái “' + v.trim() + '”.' : null; }
    el.addEventListener('change', function(e){ var t = e.target; if (t.dataset.lsp){ opt.props[t.dataset.lsp] = t.checked ? 1 : 0; save(); } else if (t.id === 'ls-noxref') { opt.noXref = t.checked; save(); } else if (t.id === 'ls-off') { opt.offMissing = t.checked; save(); } else if (t.id === 'ls-vp') { opt.asVp = t.checked; save(); } });
    el.addEventListener('dblclick', function(e){ var r = e.target.closest('[data-lsn]'); if (r){ cur = r.dataset.lsn; $('#ls-go', el).click(); } });
    el.addEventListener('click', function(e){
      var r = e.target.closest('[data-lsn]'); if (r){ cur = r.dataset.lsn; render(); return; }
      var b = e.target.closest('[data-ls]'); if (!b) return; var k = b.dataset.ls, s = list.filter(function(x){ return x.name === cur; })[0];
      if (k === 'all' || k === 'none'){ LPROPS.forEach(function(p){ opt.props[p[0]] = k === 'all' ? 1 : 0; }); $$('[data-lsp]', el).forEach(function(c){ c.checked = k === 'all'; }); save(); return; }
      if (k === 'new') return SC.layerStateNew(d).then(function(st){ if (st){ cur = st.name; render(); } });
      if (k === 'upd') return TC.confirm({ title:'Cập nhật trạng thái layer', msg:'Ghi đè “<b>' + esc(s.name) + '</b>” bằng thiết lập layer hiện tại (' + d.layers.length + ' layer)?', ok:'Cập nhật' }).then(function(y){ if (y){ s.layers = snap(d); s.date = TC.date(); render(); TC.toast('Đã cập nhật “' + esc(s.name) + '”.', { log:false }); } });
      if (k === 'ren') return SC.ask('Đổi tên trạng thái layer', 'Tên mới', s.name, function(v){ return stName(v, s.name); }, 'UI-C04').then(function(v){ if (v){ s.name = v.trim(); cur = s.name; render(); } });
      if (k === 'del') return TC.confirm({ title:'Xóa trạng thái layer', msg:'Xóa trạng thái “<b>' + esc(s.name) + '</b>”? Layer trong bản vẽ không đổi.', ok:'Xóa trạng thái', danger:true }).then(function(y){ if (y){ d.layerStates = list.filter(function(x){ return x !== s; }); cur = d.layerStates[0] && d.layerStates[0].name; render(); } });
      if (k === 'edit') return stateEdit(d, s).then(render);
      if (k === 'exp') return TC.toast('Đã xuất “' + esc(s.name) + '” ra <span class="mono">' + esc(s.name.replace(/[^\w]+/g, '-')) + '.las</span>.', { act:{ label:'Mở thư mục', fn:function(){} } });
      if (k === 'imp'){ var rr = b.getBoundingClientRect(); TC.menu([{ label:'Từ tệp trạng thái .las…', icon:'open', act:function(){ var st = { name:'Chuẩn công ty · in A3', space:'Model', desc:'Nhập từ mau-in-A3.las', layers:snap(d), date:TC.date() }; Object.keys(st.layers).forEach(function(n){ if (/NOITHAT|TBVS/.test(n)) st.layers[n].on = false; }); list.push(st); cur = st.name; render(); TC.toast('Đã nhập 1 trạng thái từ <span class="mono">mau-in-A3.las</span>. 3 layer trong tệp không có trong bản vẽ được bỏ qua.', { log:false }); } }, { label:'Từ bản vẽ hoặc mẫu (.dwg, .dwt)…', icon:'file', act:function(){ TC.confirm({ title:'Nhập trạng thái layer', msg:'<b class="mono">Mau-khung-ten-cong-ty.dwt</b> có 2 trạng thái layer: “In đen trắng”, “Bản nháp”.', detail:'Trạng thái trùng tên sẽ hỏi ghi đè. Layer chưa có trong bản vẽ không được tạo thêm.', ok:'Nhập 2 trạng thái' }).then(function(y){ if (!y) return; ['In đen trắng', 'Bản nháp'].forEach(function(nm, i){ var st = { name:nm, space:'Model', desc:'Nhập từ Mau-khung-ten-cong-ty.dwt', layers:snap(d), date:TC.date() }; if (!i) Object.keys(st.layers).forEach(function(n){ st.layers[n].color = 7; }); list.push(st); }); cur = 'In đen trắng'; render(); }); } }], rr.left, rr.bottom + 2); return; }
    });
    $('#ls-go', el).addEventListener('click', function(){ var s = list.filter(function(x){ return x.name === cur; })[0]; if (!s) return; restore(s); dlg.close(true); });
    render();
  };
  SC.layerStateNew = function(d){
    d = d || D(); var list = states(d), n = 1; while (list.some(function(x){ return x.name === 'Trạng thái ' + n; })) n++;
    return new Promise(function(res){
      var dlg = TC.dialog({ id:'lsnew', title:'Trạng thái layer mới', code:'UI-C04', w:440, help:false, onClose:function(r){ if (r !== true) res(null); },
        body:'<div class="stack"><div class="field"><label for="lsn-n">Tên trạng thái</label><input class="inp" id="lsn-n" value="Trạng thái ' + n + '"><span class="fmsg" id="lsn-m"></span></div><div class="field"><label for="lsn-d">Mô tả</label><input class="inp" id="lsn-d" placeholder="Ví dụ: tắt nội thất để in mặt bằng"></div><p class="faint" style="font-size:var(--fs-s);margin:0">Lưu bật/tắt, đóng băng, khóa, in, màu, linetype, độ dày nét, trong suốt của ' + d.layers.length + ' layer' + (d.space !== 'model' ? ' (không gian: ' + esc(d.layouts[d.space].name) + ')' : '') + '.</p></div>',
        foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="lsn-ok">Lưu trạng thái</button>' });
      setTimeout(function(){ $('#lsn-n', dlg.el).select(); }, 0);
      $('#lsn-ok', dlg.el).addEventListener('click', function(){ var v = $('#lsn-n', dlg.el).value.trim(), err = !v ? 'Nhập tên trạng thái.' : list.some(function(x){ return x.name.toLowerCase() === v.toLowerCase(); }) ? 'Đã có trạng thái “' + v + '”. Chọn tên khác hoặc dùng Cập nhật.' : null; if (err){ var m = $('#lsn-m', dlg.el); m.className = 'fmsg err'; m.innerHTML = ic('warn', 'xs') + esc(err); $('#lsn-n', dlg.el).classList.add('err'); return; } var st = { name:v, space:d.space === 'model' ? 'Model' : d.layouts[d.space].name, desc:$('#lsn-d', dlg.el).value, layers:snap(d), date:TC.date() }; list.push(st); d.curLayerState = v; dlg.close(true); TC.toast('Đã lưu trạng thái layer “' + esc(v) + '”.', { log:false, act:{ label:'Quản lý', fn:function(){ SC.layerStates(v); } } }); res(st); });
    });
  };
  function stateEdit(d, s){
    return new Promise(function(res){
      var L = JSON.parse(JSON.stringify(s.layers));
      var dlg = TC.dialog({ id:'lsedit', title:'Sửa trạng thái layer · ' + esc(s.name), code:'UI-C04', w:760, h:520, flush:true, onClose:function(r){ res(r); },
        body:'<div style="display:flex;flex-direction:column;height:100%;min-height:0"><div class="pal-tools"><button class="btn sm" type="button" data-le="add">' + ic('plus') + 'Thêm layer…</button><button class="btn sm" type="button" data-le="rm">' + ic('minus') + 'Bỏ khỏi trạng thái</button><span class="grow"></span><span class="faint" style="font-size:var(--fs-s)">Bấm biểu tượng để đổi giá trị lưu trong trạng thái; bản vẽ chưa đổi.</span></div><div style="flex:1;overflow:auto;min-height:0" id="le-t"></div></div>',
        foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="le-ok">Lưu thay đổi</button>' });
      var sel = null;
      function render(){ var names = Object.keys(L); $('#le-t', dlg.el).innerHTML = '<table class="tbl"><colgroup><col><col style="width:44px"><col style="width:44px"><col style="width:44px"><col style="width:44px"><col style="width:110px"><col style="width:110px"><col style="width:90px"></colgroup><thead><tr><th>Layer</th><th class="ctr">Bật</th><th class="ctr">Băng</th><th class="ctr">Khóa</th><th class="ctr">In</th><th>Màu</th><th>Linetype</th><th class="num">Độ dày nét</th></tr></thead><tbody>' + names.map(function(n){ var x = L[n], b = function(k, on, off){ var v = x[k], isOn = k === 'frz' || k === 'lck' ? !v : v; return '<td class="ctr"><button class="tgl' + (isOn ? '' : ' off') + '" type="button" data-lek="' + k + '" data-len="' + esc(n) + '" aria-pressed="' + !!v + '">' + ic(isOn ? on : off) + '</button></td>'; }; return '<tr data-lerow="' + esc(n) + '" class="' + (sel === n ? 'is-sel' : '') + '"><td>' + esc(n) + '</td>' + b('on', 'bulb', 'bulboff') + b('frz', 'sun', 'snow') + b('lck', 'unlock', 'lock') + b('plot', 'print', 'x') + '<td>' + swatch(x.color) + ' ' + (x.color <= 7 ? TC.aciName(x.color) : x.color) + '</td><td>' + esc(x.lt) + '</td><td class="num">' + TC.d(x.lw) + ' mm</td></tr>'; }).join('') + '</tbody></table>'; }
      dlg.el.addEventListener('click', function(e){ var t = e.target.closest('[data-lek]'); if (t){ var x = L[t.dataset.len]; x[t.dataset.lek] = !x[t.dataset.lek]; render(); return; } var r = e.target.closest('[data-lerow]'); if (r){ sel = r.dataset.lerow; render(); return; } var b = e.target.closest('[data-le]'); if (!b) return;
        if (b.dataset.le === 'rm' && sel){ delete L[sel]; sel = null; render(); }
        else if (b.dataset.le === 'add'){ var rr = b.getBoundingClientRect(), miss = d.layers.filter(function(l){ return !L[l.name]; }); TC.menu(miss.length ? miss.map(function(l){ return { label:swatch(l.color) + esc(l.name), act:function(){ L[l.name] = { on:l.on !== false, frz:!!l.frz, lck:!!l.lck, plot:l.plot !== false, vpfrz:false, color:l.color, lt:l.lt, lw:l.lw, transp:0 }; sel = l.name; render(); } }; }) : [{ label:'Mọi layer đã có trong trạng thái', dis:true }], rr.left, rr.bottom + 2); } });
      $('#le-ok', dlg.el).addEventListener('click', function(){ s.layers = L; s.date = TC.date(); dlg.close(true); });
      render();
    });
  }
  IMPL.LAYERSTATE = function(){ SC.layerStates(); };
  OPEN['UI-C04'] = function(){ SC.cat.PRO(); SC.layerStates(); };

  /* ================= Bộ lọc layer UI-C05 ================= */
  var COND = [['name', 'Tên', 116], ['on', 'Bật', 80], ['frz', 'Đóng băng', 92], ['lck', 'Khóa', 80], ['plot', 'In', 80], ['color', 'Màu', 92], ['lt', 'Linetype', 104], ['lw', 'Độ dày nét', 82], ['used', 'Đang dùng', 82]];
  function filters(d){ if (!d.layerFilters) d.layerFilters = d.id === 'd1' ? [{ id:'f1', type:'prop', name:'Kiến trúc', rows:[{ name:'KT-*' }] }, { id:'f2', type:'prop', name:'Nét mảnh', rows:[{ lw:'0.13' }, { lw:'0.18' }] }, { id:'g1', type:'group', name:'Phối hợp kết cấu', layers:['KT-TRUC', 'KT-COT', 'KC-COT', 'KC-DAM'] }] : []; return d.layerFilters; }
  function condOk(c, l){
    if (c.name && !wild(c.name).test(l.name)) return false;
    var B = function(k, v){ return v === '' || v == null || (v === 'yes') === !!(k === 'on' || k === 'plot' ? l[k] !== false : l[k]); };
    if (!B('on', c.on) || !B('frz', c.frz) || !B('lck', c.lck) || !B('plot', c.plot)) return false;
    if (c.color && String(l.color) !== String(c.color)) return false; if (c.lt && l.lt !== c.lt) return false; if (c.lw && TC.d(l.lw) !== TC.d(Number(c.lw))) return false;
    if (c.used === 'yes' && !l.count) return false; if (c.used === 'no' && l.count) return false;
    return true;
  }
  function inFilter(d, f, l){ return f.type === 'group' ? f.layers.indexOf(l.name) >= 0 : f.rows.some(function(c){ return condOk(c, l); }); }
  // danh sách layer (palette, Quản lý layer, duyệt layer) lọc theo bộ lọc tự tạo; có đảo bộ lọc
  TC.layerFilterTest = function(d, l){ var ls = TC.lstate, f = (TC.isViewer() ? [] : filters(d)).filter(function(x){ return x.id === ls.filter; })[0]; if (!f) return true; var ok = inFilter(d, f, l); return ls.invert ? !ok : ok; };
  SC.layerTreeHTML = function(d, ls){
    var F = filters(d), cnt = function(f){ return d.layers.filter(function(l){ return inFilter(d, f, l); }).length; }, node = function(f, icn){ return '<button class="tn" type="button" data-lmf="' + f.id + '" data-lff="' + f.id + '" aria-selected="' + (ls.filter === f.id) + '" style="padding-left:18px">' + ic(icn, 's12') + esc(f.name) + '<span class="cnt">' + cnt(f) + '</span></button>'; };
    var props = F.filter(function(f){ return f.type === 'prop'; }), groups = F.filter(function(f){ return f.type === 'group'; });
    return '<div class="gh lfh">Bộ lọc thuộc tính<button class="ib sm" type="button" data-lfnew="prop" aria-label="Bộ lọc thuộc tính mới" data-tip="Bộ lọc thuộc tính mới">' + ic('plus') + '</button></div>' + (props.map(function(f){ return node(f, 'filter'); }).join('') || '<div class="faint lfe">Chưa có</div>') +
      '<div class="gh lfh">Bộ lọc nhóm<button class="ib sm" type="button" data-lfnew="group" aria-label="Bộ lọc nhóm mới" data-tip="Bộ lọc nhóm mới">' + ic('plus') + '</button></div>' + (groups.map(function(f){ return node(f, 'folder'); }).join('') || '<div class="faint lfe">Kéo layer vào đây sau khi tạo nhóm</div>');
  };
  SC.layerTreeFoot = function(ls){ return '<label class="chk lfinv"><input type="checkbox" id="lf-inv"' + (ls.invert ? ' checked' : '') + '> Đảo bộ lọc</label>'; };
  SC.layerTreeBind = function(root, tree, d, render){
    var ls = TC.lstate;
    tree.addEventListener('click', function(e){ var n = e.target.closest('[data-lfnew]'); if (n){ e.stopPropagation(); if (n.dataset.lfnew === 'prop') SC.layerFilterEdit(d, null, render); else { var g = { id:'g' + Date.now(), type:'group', name:'Nhóm ' + (filters(d).filter(function(f){ return f.type === 'group'; }).length + 1), layers:ls.sel ? [ls.sel] : [] }; filters(d).push(g); ls.filter = g.id; render(); TC.toast('Đã tạo bộ lọc nhóm “' + esc(g.name) + '”. Kéo layer từ bảng vào nhóm, hoặc chuột phải › Chọn layer từ bản vẽ.', { type:'info', log:false }); } } });
    tree.addEventListener('change', function(e){ if (e.target.id === 'lf-inv'){ ls.invert = e.target.checked; render(); A().refreshPalettes(); } });
    tree.addEventListener('dblclick', function(e){ var f = e.target.closest('[data-lff]'); if (!f) return; var flt = filters(d).filter(function(x){ return x.id === f.dataset.lff; })[0]; if (flt && flt.type === 'prop') SC.layerFilterEdit(d, flt, render); });
    tree.addEventListener('contextmenu', function(e){ var f = e.target.closest('[data-lff]'); if (!f) return; e.preventDefault(); var flt = filters(d).filter(function(x){ return x.id === f.dataset.lff; })[0];
      TC.menu([{ label:'Hiển thị bộ lọc này', act:function(){ ls.filter = flt.id; render(); } }, { label:'Đổi tên…', act:function(){ SC.ask('Đổi tên bộ lọc', 'Tên bộ lọc', flt.name, null, 'UI-C05').then(function(v){ if (v){ flt.name = v; render(); } }); } }, flt.type === 'prop' ? { label:'Thuộc tính…', act:function(){ SC.layerFilterEdit(d, flt, render); } } : { label:'Chọn layer từ bản vẽ…', icon:'select', act:function(){ pickInto(d, flt, render); } },
        flt.type === 'prop' ? { label:'Chuyển thành bộ lọc nhóm', act:function(){ flt.layers = d.layers.filter(function(l){ return inFilter(d, flt, l); }).map(function(l){ return l.name; }); flt.type = 'group'; delete flt.rows; render(); } } : { label:'Bỏ layer đang chọn khỏi nhóm', dis:!ls.sel || flt.layers.indexOf(ls.sel) < 0, act:function(){ flt.layers = flt.layers.filter(function(x){ return x !== ls.sel; }); render(); } },
        { sep:1 }, { label:'Cô lập các layer của bộ lọc', icon:'isolate', act:function(){ TC.layers.isolate(d, d.layers.filter(function(l){ return inFilter(d, flt, l); }).map(function(l){ return l.name; })); render(); } }, { label:'Xóa bộ lọc', icon:'trash', act:function(){ d.layerFilters = filters(d).filter(function(x){ return x !== flt; }); if (ls.filter === flt.id) ls.filter = 'all'; render(); } }], e.clientX, e.clientY); });
    // kéo layer từ bảng vào bộ lọc nhóm
    root.addEventListener('dragstart', function(e){ var r = e.target.closest && e.target.closest('[data-lrow]'); if (!r) return; e.dataTransfer.setData('text/x-layer', r.dataset.lrow); e.dataTransfer.effectAllowed = 'copy'; });
    tree.addEventListener('dragover', function(e){ var f = e.target.closest('[data-lff]'), flt = f && filters(d).filter(function(x){ return x.id === f.dataset.lff; })[0]; if (flt && flt.type === 'group'){ e.preventDefault(); f.classList.add('is-drop'); } });
    tree.addEventListener('dragleave', function(e){ var f = e.target.closest('[data-lff]'); if (f) f.classList.remove('is-drop'); });
    tree.addEventListener('drop', function(e){ var f = e.target.closest('[data-lff]'); if (!f) return; e.preventDefault(); var flt = filters(d).filter(function(x){ return x.id === f.dataset.lff; })[0], n = e.dataTransfer.getData('text/x-layer'); if (flt && n && flt.layers.indexOf(n) < 0){ flt.layers.push(n); TC.toast('Đã thêm <b>' + esc(n) + '</b> vào nhóm “' + esc(flt.name) + '”.', { log:false }); } render(); });
  };
  function pickInto(d, flt, render){
    var top = TC.topDialog(); if (top) top.hide(true);
    TC.cmdline.msg('Chọn đối tượng để thêm layer của chúng vào nhóm “' + esc(flt.name) + '”, Enter để xong.');
    TC.cad.getSel({}).then(function(ids){ var add = {}; (ids || []).forEach(function(id){ var e = d.byId[id]; if (e) add[e.layer] = 1; }); Object.keys(add).forEach(function(n){ if (flt.layers.indexOf(n) < 0) flt.layers.push(n); }); d.sel = []; V().drawOver(); if (top) top.hide(false); render(); TC.toast('Đã thêm ' + Object.keys(add).length + ' layer vào “' + esc(flt.name) + '”.', { log:false }); });
  }
  SC.layerFilterEdit = function(d, flt, done){
    d = d || D(); var isNew = !flt, F = filters(d), work = flt ? JSON.parse(JSON.stringify(flt)) : { id:'f' + Date.now(), type:'prop', name:'Bộ lọc ' + (F.filter(function(f){ return f.type === 'prop'; }).length + 1), rows:[{ name:'KT-*' }] };
    var dlg = TC.dialog({ id:'lflt', title:'Thuộc tính bộ lọc layer', code:'UI-C05', w:900, h:600, flush:true,
      body:'<div class="lfw"><div class="field" style="max-width:340px"><label for="lf-n">Tên bộ lọc</label><input class="inp" id="lf-n" value="' + esc(work.name) + '"><span class="fmsg" id="lf-m"></span></div>' +
        '<div class="gh">Định nghĩa bộ lọc <span class="faint" style="font-weight:400;text-transform:none;letter-spacing:0">· mỗi hàng là một điều kiện, layer khớp một hàng bất kỳ thì hiện; Tên dùng được * và ?</span></div><div class="tblwrap lfc" id="lf-c"></div><div><button class="btn sm" type="button" id="lf-add">' + ic('plus') + 'Thêm điều kiện</button></div>' +
        '<div class="gh">Xem trước <span id="lf-cnt" class="faint" style="font-weight:400;text-transform:none;letter-spacing:0"></span></div><div class="tblwrap lfpv" id="lf-pv"></div></div>',
      foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="lf-ok">' + (isNew ? 'Tạo bộ lọc' : 'Lưu bộ lọc') + '</button>' });
    var el = dlg.el, ACIS = [1, 2, 3, 4, 5, 6, 7, 8, 9, 140];
    function yn(k, v){ return '<select class="inp" data-lfk="' + k + '"><option value=""' + (!v ? ' selected' : '') + '>Bất kỳ</option><option value="yes"' + (v === 'yes' ? ' selected' : '') + '>' + ({ on:'Bật', frz:'Đóng băng', lck:'Khóa', plot:'In', used:'Có' }[k]) + '</option><option value="no"' + (v === 'no' ? ' selected' : '') + '>' + ({ on:'Tắt', frz:'Không băng', lck:'Không khóa', plot:'Không in', used:'Rỗng' }[k]) + '</option></select>'; }
    function renderC(){ $('#lf-c', el).innerHTML = '<table class="tbl"><colgroup>' + COND.map(function(c){ return '<col style="width:' + c[2] + 'px">'; }).join('') + '<col style="width:32px"></colgroup><thead><tr>' + COND.map(function(c){ return '<th>' + c[1] + '</th>'; }).join('') + '<th></th></tr></thead><tbody>' + work.rows.map(function(r, i){ return '<tr data-lfr="' + i + '"><td><input class="inp mono" data-lfk="name" value="' + esc(r.name || '') + '" placeholder="*" spellcheck="false"></td><td>' + yn('on', r.on) + '</td><td>' + yn('frz', r.frz) + '</td><td>' + yn('lck', r.lck) + '</td><td>' + yn('plot', r.plot) + '</td><td><select class="inp" data-lfk="color"><option value="">Bất kỳ</option>' + ACIS.map(function(c){ return '<option value="' + c + '"' + (String(r.color) === String(c) ? ' selected' : '') + '>' + (c <= 7 ? TC.aciName(c) : 'Màu ' + c) + '</option>'; }).join('') + '</select></td><td><select class="inp" data-lfk="lt"><option value="">Bất kỳ</option>' + SC.ltList(d).map(function(t){ return '<option' + (r.lt === t ? ' selected' : '') + '>' + esc(t) + '</option>'; }).join('') + '</select></td><td><select class="inp" data-lfk="lw"><option value="">Bất kỳ</option>' + ['0.13', '0.18', '0.25', '0.35', '0.50', '0.70'].map(function(w){ return '<option' + (r.lw && TC.d(Number(r.lw)) === w ? ' selected' : '') + '>' + w + '</option>'; }).join('') + '</select></td><td>' + yn('used', r.used) + '</td><td class="ctr"><button class="ib sm" type="button" data-lfdel="' + i + '" aria-label="Xóa điều kiện"' + (work.rows.length < 2 ? ' disabled' : '') + '>' + ic('x') + '</button></td></tr>'; }).join('') + '</tbody></table>'; }
    function renderPv(){ var hit = d.layers.filter(function(l){ return work.rows.some(function(c){ return condOk(c, l); }); }); $('#lf-cnt', el).textContent = '· khớp ' + hit.length + '/' + d.layers.length + ' layer'; $('#lf-pv', el).innerHTML = hit.length ? '<table class="tbl"><colgroup><col><col style="width:50px"><col style="width:50px"><col style="width:50px"><col style="width:110px"><col style="width:110px"><col style="width:90px"><col style="width:80px"></colgroup><thead><tr><th>Tên</th><th class="ctr">Bật</th><th class="ctr">Băng</th><th class="ctr">Khóa</th><th>Màu</th><th>Linetype</th><th class="num">Độ dày</th><th class="num">Đối tượng</th></tr></thead><tbody>' + hit.map(function(l){ return '<tr><td>' + esc(l.name) + '</td><td class="ctr">' + ic(l.on !== false ? 'bulb' : 'bulboff', 's12') + '</td><td class="ctr">' + ic(l.frz ? 'snow' : 'sun', 's12') + '</td><td class="ctr">' + ic(l.lck ? 'lock' : 'unlock', 's12') + '</td><td>' + swatch(l.color) + ' ' + (l.color <= 7 ? TC.aciName(l.color) : l.color) + '</td><td>' + esc(l.lt) + '</td><td class="num">' + TC.d(l.lw) + '</td><td class="num">' + TC.n(l.count || 0) + '</td></tr>'; }).join('') + '</tbody></table>' : '<div class="empty">' + ic('filter') + '<b>Không layer nào khớp</b>Nới điều kiện, ví dụ đổi Tên thành KT-* hoặc để trống.</div>'; }
    el.addEventListener('input', function(e){ var k = e.target.dataset.lfk, tr = e.target.closest('[data-lfr]'); if (k && tr){ work.rows[Number(tr.dataset.lfr)][k] = e.target.value; renderPv(); } });
    el.addEventListener('change', function(e){ var k = e.target.dataset.lfk, tr = e.target.closest('[data-lfr]'); if (k && tr){ work.rows[Number(tr.dataset.lfr)][k] = e.target.value; renderPv(); } });
    el.addEventListener('click', function(e){ var x = e.target.closest('[data-lfdel]'); if (x){ work.rows.splice(Number(x.dataset.lfdel), 1); renderC(); renderPv(); } });
    $('#lf-add', el).addEventListener('click', function(){ work.rows.push({}); renderC(); renderPv(); });
    $('#lf-ok', el).addEventListener('click', function(){ var v = $('#lf-n', el).value.trim(), err = !v ? 'Nhập tên bộ lọc.' : F.some(function(f){ return f.name === v && f.id !== work.id; }) ? 'Đã có bộ lọc “' + v + '”.' : null; if (err){ var m = $('#lf-m', el); m.className = 'fmsg err'; m.innerHTML = ic('warn', 'xs') + esc(err); return; } work.name = v; if (isNew) F.push(work); else Object.assign(flt, work); TC.lstate.filter = work.id; dlg.close(true); if (done) done(); A().refreshPalettes(); });
    renderC(); renderPv();
  };
  OPEN['UI-C05'] = function(){ var d = SC.cat.PRO(); TC.lstate.filter = 'f1'; SC.layerManager(); setTimeout(function(){ SC.layerFilterEdit(d, filters(d)[0], function(){ var t = TC.topDialog(); }); }, 250); };

  /* ================= Duyệt layer UI-C11 (LAYWALK) ================= */
  var lv0 = TC.layerVisible;
  TC.layerVisible = function(doc, name){ if (doc.walk) return !!(doc.walk.show[name] || doc.walk.hold[name]); return lv0(doc, name); };
  IMPL.LAYWALK = function(){ SC.layWalk(); };
  SC.layWalk = function(){
    var d = D(); if (!d) return; var st = { q:'', flt:'all', sel:[], anchor:null, restore:true }, hold = {};
    d.walk = { show:{}, hold:hold };
    var dlg = TC.dialog({ id:'laywalk', title:'Duyệt layer', cmd:'LAYWALK', code:'UI-C11', w:360, h:600, modal:false, flush:true,
      onClose:function(){ var keep = !st.restore && st.sel.length, shown = d.walk; d.walk = null; if (keep){ layerRun(d, 'Duyệt layer', function(){ d.layers.forEach(function(l){ var on = !!(shown.show[l.name] || shown.hold[l.name]); if (l.name === ED.clayer(d)) on = true; l.on = on; }); }); TC.cmdline.msg('Đã giữ trạng thái từ Duyệt layer: ' + Object.keys(shown.show).length + ' layer bật.'); } else V().draw(); TC.emit('layers'); },
      body:'<div class="lww"><div class="pal-tools"><label class="search grow">' + ic('search') + '<input id="lw-q" placeholder="Lọc tên: KT-*, *CUA*" aria-label="Lọc layer" spellcheck="false"></label><select class="inp" id="lw-f" aria-label="Bộ lọc" style="width:120px"></select></div><div class="lwl" id="lw-l" role="listbox" aria-multiselectable="true" tabindex="0"></div>' +
        '<div class="pal-foot" id="lw-ft"></div></div>',
      foot:'<button class="btn" type="button" id="lw-purge" data-tip="Dọn" data-desc="Xóa các layer rỗng (không có đối tượng) trong số đang chọn.">' + ic('erase') + 'Dọn</button><label class="chk" style="margin-left:6px"><input type="checkbox" id="lw-rs" checked> Khôi phục trạng thái khi đóng</label><span class="grow"></span><button class="btn primary" type="button" data-x="close">Đóng</button>' });
    var el = dlg.el; el.style.left = '64px'; el.style.top = '170px';
    $('#lw-f', el).innerHTML = '<option value="all">Tất cả</option><option value="used">Đang dùng</option>' + filters(d).map(function(f){ return '<option value="' + f.id + '">' + esc(f.name) + '</option>'; }).join('');
    function names(){ var re = st.q.trim() ? (/[*?]/.test(st.q) ? wild(st.q.trim()) : null) : null, q = TC.fold(st.q.trim()), f = filters(d).filter(function(x){ return x.id === st.flt; })[0]; return d.layers.filter(function(l){ if (st.flt === 'used' && !l.count) return false; if (f && !inFilter(d, f, l)) return false; return re ? re.test(l.name) : !q || TC.fold(l.name).indexOf(q) >= 0; }).map(function(l){ return l.name; }); }
    function apply(){ d.walk.show = {}; st.sel.forEach(function(n){ d.walk.show[n] = 1; }); if (!st.sel.length) d.layers.forEach(function(l){ if (lv0(d, l.name)) d.walk.show[l.name] = 1; }); V().draw(); }
    function render(){ var list = names();
      $('#lw-l', el).innerHTML = list.map(function(n){ var l = d.layerMap[n]; return '<div class="lwr' + (st.sel.indexOf(n) >= 0 ? ' is-sel' : '') + (l.count ? '' : ' nil') + '" role="option" aria-selected="' + (st.sel.indexOf(n) >= 0) + '" data-lw="' + esc(n) + '">' + (hold[n] ? '<span class="lwh" title="Luôn hiện (nhấp đúp để bỏ)">*</span>' : '<span class="lwh"></span>') + swatch(l.color) + '<span class="grow">' + esc(n) + '</span><span class="faint mono">' + (l.count ? TC.n(l.count) : 'rỗng') + '</span></div>'; }).join('') || '<div class="empty">Không layer nào khớp bộ lọc.</div>';
      var emp = st.sel.filter(function(n){ var l = d.layerMap[n]; return l && !l.count && n !== '0' && n !== 'Defpoints' && n !== ED.clayer(d); });
      $('#lw-purge', el).disabled = !emp.length; $('#lw-purge', el).dataset.n = emp.length;
      $('#lw-ft', el).innerHTML = '<span>' + (st.sel.length ? 'Đang hiện ' + st.sel.length + ' layer' + (Object.keys(hold).length ? ' + ' + Object.keys(hold).length + ' luôn hiện' : '') : 'Chọn layer để chỉ hiện đối tượng của layer đó') + '</span><span class="grow"></span><span class="faint">Ctrl, Shift để chọn nhiều</span>';
    }
    function pick(n, e){ var list = names(); if (e && e.shiftKey && st.anchor){ var a = list.indexOf(st.anchor), b = list.indexOf(n); st.sel = list.slice(Math.min(a, b), Math.max(a, b) + 1); } else if (e && (e.ctrlKey || e.metaKey)){ var i = st.sel.indexOf(n); if (i >= 0) st.sel.splice(i, 1); else st.sel.push(n); st.anchor = n; } else { st.sel = [n]; st.anchor = n; } apply(); render(); }
    $('#lw-q', el).addEventListener('input', function(){ st.q = this.value; var list = names(); if (/[*?]/.test(st.q)){ st.sel = list.slice(); apply(); } render(); });
    $('#lw-f', el).addEventListener('change', function(){ st.flt = this.value; render(); });
    $('#lw-rs', el).addEventListener('change', function(){ st.restore = this.checked; });
    var L = $('#lw-l', el);
    L.addEventListener('click', function(e){ var r = e.target.closest('[data-lw]'); if (r) pick(r.dataset.lw, e); });
    L.addEventListener('dblclick', function(e){ var r = e.target.closest('[data-lw]'); if (!r) return; var n = r.dataset.lw; if (hold[n]) delete hold[n]; else hold[n] = 1; V().draw(); render(); });
    L.addEventListener('keydown', function(e){ var list = names(); if (!list.length) return; var i = list.indexOf(st.anchor || list[0]); if (e.key === 'ArrowDown' || e.key === 'ArrowUp'){ e.preventDefault(); i = TC.clamp(i + (e.key === 'ArrowDown' ? 1 : -1), 0, list.length - 1); pick(list[i], e.shiftKey ? e : null); var r = $('[data-lw="' + list[i].replace(/"/g, '') + '"]', L); if (r) r.scrollIntoView({ block:'nearest' }); } else if (e.key === 'a' && (e.ctrlKey || e.metaKey)){ e.preventDefault(); st.sel = list.slice(); apply(); render(); } });
    L.addEventListener('contextmenu', function(e){ e.preventDefault(); var list = names(), r = e.target.closest('[data-lw]'), n = r && r.dataset.lw; TC.menu([{ label:hold[n] ? 'Bỏ luôn hiện' : 'Luôn hiện layer này', dis:!n, act:function(){ if (hold[n]) delete hold[n]; else hold[n] = 1; V().draw(); render(); } }, { sep:1 }, { label:'Chọn tất cả', kbd:'Ctrl+A', act:function(){ st.sel = list.slice(); apply(); render(); } }, { label:'Đảo vùng chọn', act:function(){ st.sel = list.filter(function(x){ return st.sel.indexOf(x) < 0; }); apply(); render(); } }, { label:'Chọn các layer rỗng', act:function(){ st.sel = list.filter(function(x){ return !d.layerMap[x].count; }); apply(); render(); } }, { sep:1 }, { label:'Lưu vùng chọn thành bộ lọc nhóm…', icon:'folder', dis:!st.sel.length, act:function(){ SC.ask('Bộ lọc nhóm mới', 'Tên nhóm', 'Duyệt ' + TC.date(), null, 'UI-C05').then(function(v){ if (v){ filters(d).push({ id:'g' + Date.now(), type:'group', name:v, layers:st.sel.slice() }); $('#lw-f', el).insertAdjacentHTML('beforeend', '<option>' + esc(v) + '</option>'); TC.toast('Đã lưu ' + st.sel.length + ' layer thành bộ lọc nhóm “' + esc(v) + '”.', { log:false }); } }); } }, { label:'Chọn đối tượng trên các layer này', icon:'select', dis:!st.sel.length, act:function(){ d.sel = d.ents.filter(function(x){ return st.sel.indexOf(x.layer) >= 0; }).map(function(x){ return x.id; }); TC.cad.afterSel(); } }], e.clientX, e.clientY); });
    $('#lw-purge', el).addEventListener('click', function(){ var emp = st.sel.filter(function(n){ var l = d.layerMap[n]; return l && !l.count && n !== '0' && n !== 'Defpoints' && n !== ED.clayer(d); }); if (!emp.length) return; TC.confirm({ title:'Dọn layer rỗng', msg:'Xóa ' + emp.length + ' layer rỗng: <b>' + emp.map(esc).join(', ') + '</b>?', detail:'Layer không có đối tượng nào. Hoàn tác được bằng Ctrl+Z.', ok:'Xóa ' + emp.length + ' layer', danger:true }).then(function(y){ if (!y) return; layerRun(d, 'Dọn layer', function(){ d.layers = d.layers.filter(function(l){ return emp.indexOf(l.name) < 0; }); emp.forEach(function(n){ delete d.layerMap[n]; }); }); st.sel = st.sel.filter(function(n){ return emp.indexOf(n) < 0; }); apply(); render(); TC.cmdline.msg('Đã xóa ' + emp.length + ' layer rỗng.'); }); });
    apply(); render(); setTimeout(function(){ L.focus(); }, 0);
  };
  OPEN['UI-C11'] = function(){ SC.cat.plan(); SC.layWalk(); setTimeout(function(){ var r = $('[data-lw="KT-TUONG"]'); if (r) r.click(); var r2 = $('[data-lw="KT-CUA"]'); if (r2) r2.dispatchEvent(new MouseEvent('click', { bubbles:true, ctrlKey:true })); }, 120); };

  /* ================= lệnh layer v1.0 (07.1 §7.2 panel Layer mở rộng) ================= */
  function pickLayers(ctx, text){
    var got = {};
    var step = function(){ return ctx.point(text, { pick:true, opts:[['Tên', 'N']] }).then(function(r){ if (r.kw === 'N') return SC.layerPickList(D(), Object.keys(got)).then(function(list){ (list || []).forEach(function(n){ got[n] = 1; }); return Object.keys(got); }); if (r.enter) return Object.keys(got); var h = V().hitTest(r.s[0], r.s[1]), e = h && D().byId[h]; if (e){ got[e.layer] = 1; ctx.out('Layer đã chọn: <b>' + Object.keys(got).map(esc).join(', ') + '</b>', 'muted'); } else ctx.out('Không có đối tượng tại điểm này.', 'muted'); return step(); }); };
    return step();
  }
  SC.layerPickList = function(d, pre){
    return new Promise(function(res){
      var on = {}; (pre || []).forEach(function(n){ on[n] = 1; });
      var dlg = TC.dialog({ id:'lpick', title:'Chọn layer', w:380, h:480, flush:true, help:false, onClose:function(r){ res(r === true ? Object.keys(on) : null); },
        body:'<div style="display:flex;flex-direction:column;height:100%"><div class="pal-tools"><label class="search grow">' + ic('search') + '<input id="lp-q" placeholder="Tìm layer" aria-label="Tìm layer"></label></div><div style="flex:1;overflow:auto;padding:4px 8px" id="lp-l"></div></div>',
        foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="lp-ok">Chọn</button>' });
      function render(){ var q = TC.fold($('#lp-q', dlg.el).value); $('#lp-l', dlg.el).innerHTML = d.layers.filter(function(l){ return !q || TC.fold(l.name).indexOf(q) >= 0; }).map(function(l){ return '<label class="chk" style="height:22px"><input type="checkbox" data-lp="' + esc(l.name) + '"' + (on[l.name] ? ' checked' : '') + '> ' + swatch(l.color) + esc(l.name) + ' <span class="faint mono" style="margin-left:auto">' + TC.n(l.count || 0) + '</span></label>'; }).join(''); }
      $('#lp-q', dlg.el).addEventListener('input', render);
      dlg.el.addEventListener('change', function(e){ var n = e.target.dataset.lp; if (n){ if (e.target.checked) on[n] = 1; else delete on[n]; } });
      $('#lp-ok', dlg.el).addEventListener('click', function(){ dlg.close(true); });
      render();
    });
  }
  function moveLayer(d, from, to){ var n = 0; allEnts(d).forEach(function(e){ if (from.indexOf(e.layer) >= 0){ ED.mod(e, function(x){ x.layer = to; }); n++; } }); return n; }
  IMPL.LAYMRG = function(ctx){
    ctx.badge('LM'); var d = D();
    return pickLayers(ctx, 'Chọn đối tượng trên layer cần gộp hoặc [Tên · N], Enter để xong').then(function(src){
      if (!src.length) return;
      return ctx.point('Chọn đối tượng trên layer đích hoặc [Tên · N]', { pick:true, opts:[['Tên', 'N']] }).then(function(r){
        var to = null; if (r.kw === 'N') return SC.ask('Gộp vào layer', 'Layer đích', ED.clayer(d), function(v){ return !d.layerMap[v] ? 'Không có layer “' + v + '”.' : null; }, 'UI-C03').then(function(v){ return v; });
        var h = r.s && V().hitTest(r.s[0], r.s[1]); return h ? d.byId[h].layer : null;
      }).then(function(to){
        if (!to) return; src = src.filter(function(n){ return n !== to && n !== '0' && n !== 'Defpoints'; }); if (!src.length){ ctx.out('Không có layer nguồn hợp lệ (khác layer đích, không phải 0, Defpoints).'); return; }
        var cnt = allEnts(d).filter(function(e){ return src.indexOf(e.layer) >= 0; }).length;
        return TC.confirm({ title:'Gộp layer', msg:'Gộp <b>' + src.map(esc).join(', ') + '</b> vào <b>' + esc(to) + '</b>?', detail:TC.n(cnt) + ' đối tượng chuyển sang layer đích, rồi xóa ' + src.length + ' layer nguồn. Hoàn tác được bằng Ctrl+Z.', ok:'Gộp ' + src.length + ' layer' }).then(function(y){
          if (!y){ ctx.out('*Hủy*'); return; } var n = 0;
          ED.run('LAYMRG', function(){ n = moveLayer(d, src, to); layerRun(d, 'LAYMRG', function(){ if (src.indexOf(d.clayer) >= 0) d.clayer = to; d.layers = d.layers.filter(function(l){ return src.indexOf(l.name) < 0; }); src.forEach(function(x){ delete d.layerMap[x]; }); }); });
          ctx.out('Đã gộp ' + src.length + ' layer vào <b>' + esc(to) + '</b>: ' + TC.n(n) + ' đối tượng.'); });
      });
    });
  };
  IMPL.LAYDEL = function(ctx){
    ctx.badge('LD'); var d = D();
    return pickLayers(ctx, 'Chọn đối tượng trên layer cần xóa hoặc [Tên · N], Enter để xong').then(function(src){
      var bad = src.filter(function(n){ return n === '0' || n === 'Defpoints' || n === ED.clayer(d); }); src = src.filter(function(n){ return bad.indexOf(n) < 0; });
      if (bad.length) ctx.out('Bỏ qua ' + bad.map(esc).join(', ') + ': layer 0, Defpoints và layer hiện hành không xóa được.');
      if (!src.length) return;
      var ents = allEnts(d).filter(function(e){ return src.indexOf(e.layer) >= 0; });
      return TC.confirm({ title:'Xóa layer và đối tượng', msg:'Xóa <b>' + src.map(esc).join(', ') + '</b> cùng <b>' + TC.n(ents.length) + ' đối tượng</b> trên các layer này?', detail:'Định nghĩa block dùng các layer này cũng được dọn. Hoàn tác được bằng Ctrl+Z cho tới khi đóng bản vẽ.', ok:'Xóa ' + src.length + ' layer, ' + TC.n(ents.length) + ' đối tượng', danger:true }).then(function(y){
        if (!y){ ctx.out('*Hủy*'); return; }
        ED.run('LAYDEL', function(){ ents.forEach(function(e){ ED.del(e); }); layerRun(d, 'LAYDEL', function(){ d.layers = d.layers.filter(function(l){ return src.indexOf(l.name) < 0; }); src.forEach(function(x){ delete d.layerMap[x]; }); }); });
        ctx.out('Đã xóa ' + src.length + ' layer và ' + TC.n(ents.length) + ' đối tượng.'); });
    });
  };
  IMPL.LAYCUR = function(ctx){ var d = D(), to = ED.clayer(d); return ctx.sel('Chọn đối tượng cần chuyển sang layer hiện hành (' + to + ')').then(function(ids){ ids = ED.editable(ids, ctx); if (!ids.length) return; ED.run('LAYCUR', function(){ ids.forEach(function(id){ ED.mod(d.byId[id], function(x){ x.layer = to; }); }); }); d.sel = []; TC.emit('sel'); ctx.out('Đã chuyển ' + TC.n(ids.length) + ' đối tượng sang layer hiện hành <b>' + esc(to) + '</b>.'); }); };
  IMPL.COPYTOLAYER = function(ctx){
    var d = D();
    return ctx.sel('Chọn đối tượng cần sao chép').then(function(ids){ ids = ids.filter(function(id){ return d.byId[id]; }); if (!ids.length) return;
      return ctx.point('Chọn đối tượng trên layer đích hoặc [Tên · N]', { pick:true, opts:[['Tên', 'N']] }).then(function(r){
        if (r.kw === 'N') return SC.ask('Sao chép sang layer', 'Layer đích', ED.clayer(d), function(v){ return !d.layerMap[v] ? 'Không có layer “' + v + '”.' : null; }, 'UI-C03');
        var h = r.s && V().hitTest(r.s[0], r.s[1]); return h ? d.byId[h].layer : null;
      }).then(function(to){ if (!to) return; ctx.out(TC.n(ids.length) + ' đối tượng sẽ được sao chép sang layer <b>' + esc(to) + '</b>.');
        return ctx.point('Điểm gốc hoặc [Khoảng dịch · D] <thoát: giữ nguyên vị trí>', { opts:[['Khoảng dịch', 'D']] }).then(function(b){
          var go = function(dx, dy){ var out = []; ED.run('COPYTOLAYER', function(){ ids.forEach(function(id){ var e = d.byId[id], c = ED.xform(ED.clean(e), ED.T.move(dx, dy)); c.layer = to; out.push(ED.add(c, ED.listOf(d, e)).id); }); }); d.sel = out; TC.cad.afterSel(); ctx.out('Đã sao chép ' + TC.n(out.length) + ' đối tượng sang layer <b>' + esc(to) + '</b>.'); };
          if (b.enter) return go(0, 0);
          if (b.kw === 'D') return ctx.val('Khoảng dịch dx,dy', '0,0').then(function(v){ var p = String(v).split(',').map(Number); go(p[0] || 0, p[1] || 0); });
          return ctx.point('Điểm đích', { base:b.p, rubber:true, onMove:function(p, temp){ temp.push(ED.temp(ids.map(function(id){ return ED.xform(ED.clean(d.byId[id]), ED.T.move(p[0] - b.p[0], p[1] - b.p[1])); }))); } }).then(function(t){ if (t.p) go(t.p[0] - b.p[0], t.p[1] - b.p[1]); });
        }); });
    });
  };

  /* ================= Quản lý view UI-D12 (VIEW) ================= */
  var STDV = ['Trên', 'Dưới', 'Trái', 'Phải', 'Trước', 'Sau', 'Isometric Tây Nam', 'Isometric Đông Nam', 'Isometric Đông Bắc', 'Isometric Tây Bắc'];
  function viewProps(d, v){ v.cat = v.cat || ''; v.ucs = v.ucs || 'Thế giới'; if (v.snap == null) v.snap = true; v.anno = v.anno || '1:100'; v.vs = v.vs || '2D Wireframe'; v.bg = v.bg || 'Mặc định'; return v; }
  function goView(d, v){ if (d.space !== 'model') A().setSpace('model'); var vv = V(); vv.pushHist(); d.view = { cx:v.c[0], cy:v.c[1], s:vv.H / v.h }; vv.draw(); TC.emit('view'); if (v.layers && !TC.isViewer()){ layerRun(d, 'Khôi phục view', function(){ d.layers.forEach(function(l){ var x = v.layers[l.name]; if (x){ l.on = x.on; l.frz = x.frz && l.name !== ED.clayer(d); } }); }); } d.curView = v.name; }
  SC.viewManager = function(sel){
    var d = D(); if (!d) return; var viewer = TC.isViewer(), cur = sel || (d.views[0] ? 'm:' + d.views[0].name : 'cur');
    d.views.forEach(function(v){ viewProps(d, v); if (v.snap && !v.layers) v.layers = snap(d); });
    var lviews = d.layouts.length ? [{ name:'Khung tên ' + d.layouts[0].name, layout:0, c:[d.layouts[0].paper.w - 95, 30], h:70 }] : [];
    var dlg = TC.dialog({ id:'viewman', title:'Quản lý view', cmd:'VIEW', code:'UI-D12', w:880, h:560, flush:true,
      body:'<div class="vmw"><div class="tree vmt" id="vm-t"></div><div class="vmp"><div class="vmpv"><canvas id="vm-c" width="300" height="170" class="pvc"></canvas></div><div id="vm-p"></div></div><div class="vmb">' +
        [['set', 'Đặt hiện hành', 'check', 'V'], ['new', 'Mới…', 'plus', 'PRO'], ['lay', 'Cập nhật layer', 'layers', 'PRO'], ['box', 'Sửa khung bao…', 'zwin', 'PRO'], ['del', 'Xóa', 'trash', 'PRO']].map(function(b){ var lock = !TC.can(b[3]); return '<button class="btn sm" type="button" data-vm="' + b[0] + '"' + (lock ? ' data-need="PRO"' : '') + '>' + ic(b[2]) + b[1] + (lock ? ' <span class="ed xs" data-ed="pro">Pro</span>' : '') + '</button>'; }).join('') + '</div></div>',
      foot:(viewer ? '<span class="faint" style="font-size:var(--fs-s)">Viewer khôi phục được view có sẵn; tạo, sửa view cần TrionCAD Pro.</span>' : '<span class="faint" style="font-size:var(--fs-s)">Nhấp đúp một view để đặt hiện hành</span>') + '<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Đóng</button><button class="btn primary" type="button" id="vm-ok">Đặt hiện hành</button>' });
    var el = dlg.el;
    function find(k){ var t = k.slice(0, 2), n = k.slice(2); if (t === 'm:') return d.views.filter(function(v){ return v.name === n; })[0]; if (t === 'l:') return lviews.filter(function(v){ return v.name === n; })[0]; return null; }
    function tree(){ var node = function(k, label, icn, lvl){ return '<button class="tn" type="button" data-vk="' + esc(k) + '" aria-selected="' + (cur === k) + '" style="padding-left:' + (6 + lvl * 14) + 'px">' + ic(icn, 's12') + esc(label) + (d.curView && k === 'm:' + d.curView ? '<span class="cnt">hiện hành</span>' : '') + '</button>'; };
      $('#vm-t', el).innerHTML = node('cur', 'Hiện hành', 'monitor', 0) + '<div class="gh vmg">View Model</div>' + (d.views.map(function(v){ return node('m:' + v.name, v.name, 'views', 1); }).join('') || '<div class="faint lfe">Chưa có view</div>') + '<div class="gh vmg">View Layout</div>' + (lviews.map(function(v){ return node('l:' + v.name, v.name, 'layout', 1); }).join('') || '<div class="faint lfe">Chưa có view</div>') + '<div class="gh vmg">View chuẩn</div>' + STDV.map(function(n){ return node('s:' + n, n, 'iso', 1); }).join(''); }
    function props(){
      var v = find(cur), vv = V().vw(), rows;
      if (cur === 'cur') rows = [['Tâm X', TC.d(d.view ? d.view.cx : 0)], ['Tâm Y', TC.d(d.view ? d.view.cy : 0)], ['Cao', TC.d(V().H / vv.s)], ['Rộng', TC.d(V().W / vv.s)], ['UCS', 'Thế giới'], ['Kiểu hiển thị', '2D Wireframe'], ['Tỉ lệ chú thích', d.annoScale || '1:100']];
      else if (cur.indexOf('s:') === 0) rows = [['Tên', cur.slice(2)], ['Loại', 'View chuẩn (hướng nhìn)'], ['Ghi chú', cur === 's:Trên' ? 'Đang dùng: mặt bằng' : 'Hữu ích với bản vẽ có đối tượng 3D']];
      else rows = [['Tên', v.name, 'txt', 'name'], ['Nhóm', v.cat || '(không)', 'txt', 'cat'], ['UCS', v.ucs], ['Ảnh chụp trạng thái layer', v.layers ? 'Đã lưu (' + Object.keys(v.layers).length + ' layer)' : 'Không'], ['Tỉ lệ chú thích', v.anno || '1:100'], ['Kiểu hiển thị', v.vs || '2D Wireframe'], ['Nền', v.bg || 'Mặc định'], ['Khung bao: tâm', TC.d(v.c[0]) + ', ' + TC.d(v.c[1])], ['Khung bao: cao', TC.d(v.h)], ['Không gian', v.layout != null ? 'Layout ' + d.layouts[v.layout].name : 'Model']];
      $('#vm-p', el).innerHTML = '<div class="vmrows">' + rows.map(function(r){ var ed = r[2] === 'txt' && !viewer; return '<div class="pr' + (ed ? ' edt' : ' ro') + '"' + (ed ? ' data-vmf="' + r[3] + '"' : '') + '><span class="pk">' + esc(r[0]) + '</span><span class="pv"><span>' + esc(r[1]) + '</span></span></div>'; }).join('') + '</div>';
      var std = cur.indexOf('s:') === 0, mv = cur.indexOf('m:') === 0;
      $$('[data-vm]', el).forEach(function(b){ var k = b.dataset.vm; b.disabled = (k === 'lay' || k === 'box' || k === 'del') && !mv || (k === 'set' && cur === 'cur'); });
      $('#vm-ok', el).disabled = cur === 'cur';
      paint(v, std);
    }
    function paint(v, std){ var c = $('#vm-c', el), x = c.getContext('2d'); x.fillStyle = TC.cssv('--c-canvas'); x.fillRect(0, 0, c.width, c.height);
      if (std && cur !== 's:Trên'){ x.fillStyle = TC.cssv('--c-text-3'); x.font = '11px ' + TC.cssv('--f-ui'); x.textAlign = 'center'; x.fillText('Bản vẽ chỉ có đối tượng 2D', c.width / 2, c.height / 2); return; }
      var tgt = v && v.layout == null ? v : null, fake = { W:c.width, H:c.height, doc:d, dpr:1 }; fake.__proto__ = TC.View.prototype;
      var vw = tgt ? { cx:tgt.c[0], cy:tgt.c[1], s:c.height / tgt.h } : (function(){ var b = d.ext, s = Math.min(c.width / (b[2] - b[0]), c.height / (b[3] - b[1])) * .92; return { cx:(b[0] + b[2]) / 2, cy:(b[1] + b[3]) / 2, s:s }; })();
      d.ents.forEach(function(e){ if (TC.entVisible(d, e) && e._bb && e._bb[2] > vw.cx - c.width / 2 / vw.s && e._bb[0] < vw.cx + c.width / 2 / vw.s) fake.drawPrims(x, e, vw, false, { color:TC.cssv('--c-text-2'), lw:.6 }); }); }
    function render(){ tree(); props(); }
    el.addEventListener('click', function(e){
      var n = e.target.closest('[data-vk]'); if (n){ cur = n.dataset.vk; render(); return; }
      var f = e.target.closest('[data-vmf]'); if (f && !e.target.closest('input')){ var v = find(cur), k = f.dataset.vmf, pv = $('.pv', f), inp = TC.el('<input class="pinp" spellcheck="false">'); inp.value = k === 'cat' ? v.cat || '' : v.name; pv.innerHTML = ''; pv.appendChild(inp); inp.focus(); inp.select(); var fin = function(ok){ if (!inp.isConnected) return; var val = inp.value.trim(); if (ok && k === 'name' && val && !d.views.some(function(x){ return x !== v && x.name === val; })){ v.name = val; cur = 'm:' + val; } else if (ok && k === 'cat') v.cat = val; render(); }; inp.addEventListener('keydown', function(ev){ ev.stopPropagation(); if (ev.key === 'Enter') fin(true); else if (ev.key === 'Escape') fin(false); }); inp.addEventListener('blur', function(){ fin(true); }); return; }
      var b = e.target.closest('[data-vm]'); if (!b) return; if (b.dataset.need){ SC.upgrade('Tạo và sửa view đã đặt tên', 'PRO'); return; }
      var k = b.dataset.vm, v = find(cur);
      if (k === 'set') return setCur();
      if (k === 'new') return SC.viewNew(d).then(function(nv){ if (nv){ cur = 'm:' + nv.name; render(); A().renderRibbon(); } });
      if (k === 'lay'){ v.layers = snap(d); v.snap = true; render(); TC.toast('Đã cập nhật ảnh chụp layer cho view “' + esc(v.name) + '”.', { log:false }); return; }
      if (k === 'del') return TC.confirm({ title:'Xóa view', msg:'Xóa view “<b>' + esc(v.name) + '</b>”?', ok:'Xóa view', danger:true }).then(function(y){ if (y){ d.views = d.views.filter(function(x){ return x !== v; }); cur = d.views[0] ? 'm:' + d.views[0].name : 'cur'; render(); A().renderRibbon(); } });
      if (k === 'box'){ dlg.hide(true); TC.cmdline.msg('Chỉ hai góc của khung bao mới cho view “' + esc(v.name) + '”.'); TC.cad.getPoint({}).then(function(a){ if (!a){ dlg.hide(false); return; } TC.cad.getPoint({ base:a.p, rubber:'rect' }).then(function(c2){ if (c2){ v.c = [(a.p[0] + c2.p[0]) / 2, (a.p[1] + c2.p[1]) / 2]; v.h = Math.max(Math.abs(c2.p[1] - a.p[1]), Math.abs(c2.p[0] - a.p[0]) / 1.6); } dlg.hide(false); render(); }); }); }
    });
    el.addEventListener('dblclick', function(e){ if (e.target.closest('[data-vk]')) setCur(); });
    function setCur(){ if (cur === 'cur') return; if (cur.indexOf('s:') === 0){ if (cur !== 's:Trên') TC.toast('Bản vẽ chỉ có đối tượng 2D (Z = 0); hướng ' + esc(cur.slice(2)) + ' không có gì để xem.', { type:'info', log:false }); else { V().zoomExtents(); dlg.close(true); } return; } var v = find(cur); if (v.layout != null){ A().setSpace(v.layout); dlg.close(true); return; } dlg.close(true); goView(d, v); TC.cmdline.msg('View hiện hành: <b>' + esc(v.name) + '</b>' + (v.layers && !viewer ? ' (đã khôi phục trạng thái layer)' : '') + '.'); }
    $('#vm-ok', el).addEventListener('click', setCur);
    TC.listKeys($('#vm-t', el), '[data-vk]', function(n, enter){ cur = n.dataset.vk; render(); if (enter) setCur(); });
    render();
  };
  SC.viewNew = function(d){
    var n = 1; while (d.views.some(function(v){ return v.name === 'View ' + n; })) n++;
    return new Promise(function(res){
      var box = null, dlg = TC.dialog({ id:'viewnew', title:'View mới', code:'UI-D12', w:520, onClose:function(r){ if (r !== true) res(null); },
        body:'<div class="stack"><div class="frow" style="--lab-w:120px"><span class="lab">Tên view</span><span class="hrow"><input class="inp" id="vn-n" value="View ' + n + '" style="flex:1"></span></div><div class="frow" style="--lab-w:120px"><span class="lab">Nhóm view</span><span class="hrow"><input class="inp" id="vn-c" list="vn-cl" placeholder="(không)" style="flex:1"><datalist id="vn-cl"><option>Mặt bằng</option><option>Mặt cắt</option><option>Chi tiết</option></datalist></span></div><span class="fmsg" id="vn-m"></span>' +
          H.grp('Khung bao', H.rad('vnb', 'cur', 'Màn hình hiện tại', true) + '<div class="hrow">' + H.rad('vnb', 'win', 'Chọn khung trên bản vẽ', false) + '<button class="btn sm" type="button" id="vn-pick">' + ic('zwin') + 'Chọn…</button><span class="faint" id="vn-bt" style="font-size:var(--fs-s)"></span></div>') +
          H.grp('Thiết lập', H.chk('vn-snap', 'Lưu ảnh chụp trạng thái layer cùng view', true) + H.row('UCS', H.sel('vn-ucs', ['Thế giới', 'Trước đó'], 'Thế giới', 160), 120) + H.row('Kiểu hiển thị', H.sel('vn-vs', ['2D Wireframe', 'Wireframe', 'Hidden', 'Shaded'], '2D Wireframe', 160), 120) + H.row('Nền', H.sel('vn-bg', ['Mặc định', 'Màu đơn', 'Chuyển màu'], 'Mặc định', 160), 120)) + '</div>',
        foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="vn-ok">Tạo view</button>' });
      var el = dlg.el;
      $('#vn-pick', el).addEventListener('click', function(){ dlg.hide(true); TC.cmdline.msg('Chỉ góc thứ nhất của khung view.'); TC.cad.getPoint({}).then(function(a){ if (!a){ dlg.hide(false); return; } TC.cmdline.msg('Chỉ góc đối diện.'); TC.cad.getPoint({ base:a.p, rubber:'rect' }).then(function(b){ if (b){ box = [a.p, b.p]; $('[name="vnb"][value="win"]', el).checked = true; $('#vn-bt', el).textContent = TC.d(Math.abs(b.p[0] - a.p[0]), 0) + ' × ' + TC.d(Math.abs(b.p[1] - a.p[1]), 0); } dlg.hide(false); }); }); });
      $('#vn-ok', el).addEventListener('click', function(){ var nm = $('#vn-n', el).value.trim(), m = $('#vn-m', el), err = !nm ? 'Nhập tên view.' : d.views.some(function(v){ return v.name.toLowerCase() === nm.toLowerCase(); }) ? 'Đã có view “' + nm + '”.' : null; if (err){ m.className = 'fmsg err'; m.innerHTML = ic('warn', 'xs') + esc(err); return; }
        var vv = V().vw(), useBox = $('[name="vnb"][value="win"]', el).checked && box, v = { name:nm, cat:$('#vn-c', el).value.trim(), c:useBox ? [(box[0][0] + box[1][0]) / 2, (box[0][1] + box[1][1]) / 2] : [vv.cx, vv.cy], h:useBox ? Math.max(Math.abs(box[1][1] - box[0][1]), 1) : V().H / vv.s, ucs:$('#vn-ucs', el).value, vs:$('#vn-vs', el).value, bg:$('#vn-bg', el).value, snap:$('#vn-snap', el).checked, anno:d.annoScale || '1:100' };
        if (v.snap) v.layers = snap(d); d.views.push(v); dlg.close(true); TC.cmdline.msg('Đã tạo view <b>' + esc(nm) + '</b>.'); res(v); });
    });
  };
  IMPL.VIEW = function(ctx, pre){ SC.viewManager(); };
  OPEN['UI-D12'] = function(){ SC.cat.PRO(); SC.viewManager(); };

  /* ================= Quản lý UCS UI-D13 (UCSMAN), biểu tượng UCS (UCSICON) ================= */
  TC.ucsIconApply = function(){ var el = $('.cv-ucs'); if (!el) return; el.style.display = TC.S.ucsIcon === false ? 'none' : ''; if (TC.S.ucsOrigin && D() && V()){ var s = V().toS([0, 0]); if (s[0] > 10 && s[1] > 40 && s[0] < V().W - 70 && s[1] < V().H - 10){ el.style.left = (s[0] - 10) + 'px'; el.style.bottom = (V().H - s[1] - 10) + 'px'; return; } } el.style.left = ''; el.style.bottom = ''; };
  TC.on('view', TC.ucsIconApply); TC.on('viewchg', TC.ucsIconApply);
  SC.ucsMan = function(tab){
    var d = D(); if (!d) return; d.ucsList = d.ucsList || [{ name:'Mặt bằng xoay 30°', o:[48000, 0], ang:30 }, { name:'Gốc trục A-1', o:[0, 0], ang:0 }];
    var cur = d.ucsCur || 'Thế giới', set = Object.assign({ on:TC.S.ucsIcon !== false, origin:!!TC.S.ucsOrigin, allvp:true, pick:false, perVp:true, plan:false }, d.ucsSet || {}), selN = cur, selO = 'Trên';
    var dlg = TC.dialog({ id:'ucsman', title:'Quản lý UCS', cmd:'UCSMAN', code:'UI-D13', w:600, h:460,
      body:H.tabs([['named', 'UCS đã đặt tên'], ['ortho', 'UCS trực giao'], ['set', 'Thiết lập']], tab || 'named', 'ucs') +
        '<div style="padding-top:10px;flex:1;min-height:0">' + H.pane('named', tab || 'named', '<div class="ucw"><div class="tree ucl" id="uc-n" tabindex="0"></div><div class="stack ucb"><button class="btn sm" type="button" data-uc="cur">' + ic('check') + 'Đặt hiện hành</button><button class="btn sm" type="button" data-uc="det">' + ic('info') + 'Chi tiết…</button><button class="btn sm" type="button" data-uc="ren">Đổi tên</button><button class="btn sm" type="button" data-uc="del">' + ic('trash') + 'Xóa</button></div></div><p class="faint ucnote">UCS hiện hành: <b id="uc-cur"></b></p>') +
        H.pane('ortho', tab || 'named', '<div class="ucw"><div class="tblwrap ucl"><table class="tbl"><colgroup><col><col style="width:110px"></colgroup><thead><tr><th>Tên</th><th class="num">Độ sâu</th></tr></thead><tbody id="uc-o"></tbody></table></div><div class="stack ucb"><button class="btn sm" type="button" data-uc="ocur">' + ic('check') + 'Đặt hiện hành</button><button class="btn sm" type="button" data-uc="odet">' + ic('info') + 'Chi tiết…</button><button class="btn sm" type="button" data-uc="oreset">Đặt lại</button></div></div>' + H.row('Tương đối với', H.sel('uc-rel', ['Thế giới'].concat(d.ucsList.map(function(u){ return u.name; })), 'Thế giới', 200), 110)) +
        H.pane('set', tab || 'named', H.grp('Biểu tượng UCS', H.chk('uc-on', 'Bật biểu tượng', set.on) + H.chk('uc-org', 'Hiện tại gốc tọa độ UCS', set.origin) + H.chk('uc-all', 'Áp dụng cho mọi viewport đang mở', set.allvp) + H.chk('uc-pick', 'Cho chọn biểu tượng UCS (kéo để dời gốc)', set.pick) + '<div><button class="btn sm" type="button" data-uc="iconprops">' + ic('gear') + 'Thuộc tính biểu tượng…</button></div>') + H.grp('UCS', H.chk('uc-pvp', 'Lưu UCS theo từng viewport', set.perVp) + H.chk('uc-plan', 'Chuyển về mặt bằng khi đổi UCS', set.plan))) + '</div>',
      foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="uc-ok">Áp dụng</button>' });
    var el = dlg.el, ORT = [['Trên', 0], ['Dưới', 0], ['Trước', 0], ['Sau', 0], ['Trái', 0], ['Phải', 0]];
    function names(){ return [{ name:'Thế giới' }, { name:'Trước đó' }].concat(d.ucsList); }
    function render(){
      $('#uc-n', el).innerHTML = names().map(function(u){ return '<button class="tn" type="button" data-ucn="' + esc(u.name) + '" aria-selected="' + (selN === u.name) + '">' + ic('iso', 's12') + esc(u.name) + (u.name === cur ? '<span class="cnt">hiện hành</span>' : '') + '</button>'; }).join('');
      $('#uc-o', el).innerHTML = ORT.map(function(o){ return '<tr data-uco="' + o[0] + '" class="' + (selO === o[0] ? 'is-sel' : '') + '"><td>' + ic('iso', 's12') + ' ' + o[0] + '</td><td class="num mono">' + TC.d(o[1]) + '</td></tr>'; }).join('');
      $('#uc-cur', el).textContent = cur; var fixed = selN === 'Thế giới' || selN === 'Trước đó'; $('[data-uc="ren"]', el).disabled = fixed; $('[data-uc="del"]', el).disabled = fixed || selN === cur;
    }
    function details(u){ var o = u.o || [0, 0], a = (u.ang || 0) * Math.PI / 180; TC.dialog({ id:'ucsdet', title:'Chi tiết UCS · ' + esc(u.name), code:'UI-D13', w:380, help:false, body:'<dl class="kv" style="grid-template-columns:90px 1fr">' + [['Gốc', TC.d(o[0]) + ', ' + TC.d(o[1]) + ', 0.00'], ['Trục X', TC.d(Math.cos(a), 4) + ', ' + TC.d(Math.sin(a), 4) + ', 0.0000'], ['Trục Y', TC.d(-Math.sin(a), 4) + ', ' + TC.d(Math.cos(a), 4) + ', 0.0000'], ['Trục Z', '0.0000, 0.0000, 1.0000'], ['Tương đối với', 'Thế giới']].map(function(r){ return '<dt>' + r[0] + '</dt><dd class="mono">' + r[1] + '</dd>'; }).join('') + '</dl>', foot:'<span class="grow"></span><button class="btn primary" type="button" data-x="close">Đóng</button>' }); }
    el.addEventListener('click', function(e){
      var n = e.target.closest('[data-ucn]'); if (n){ selN = n.dataset.ucn; render(); return; }
      var o = e.target.closest('[data-uco]'); if (o){ selO = o.dataset.uco; render(); return; }
      var b = e.target.closest('[data-uc]'); if (!b) return; var k = b.dataset.uc, u = names().filter(function(x){ return x.name === selN; })[0];
      if (k === 'cur'){ cur = selN; render(); }
      else if (k === 'det') details(u);
      else if (k === 'ren') SC.ask('Đổi tên UCS', 'Tên mới', u.name, function(v){ return names().some(function(x){ return x.name === v && x !== u; }) ? 'Đã có UCS “' + v + '”.' : null; }, 'UI-D13').then(function(v){ if (v){ if (cur === u.name) cur = v; u.name = v; selN = v; render(); } });
      else if (k === 'del') TC.confirm({ title:'Xóa UCS', msg:'Xóa UCS “<b>' + esc(u.name) + '</b>”?', ok:'Xóa UCS', danger:true }).then(function(y){ if (y){ d.ucsList = d.ucsList.filter(function(x){ return x !== u; }); selN = 'Thế giới'; render(); } });
      else if (k === 'ocur'){ cur = selO === 'Trên' ? 'Thế giới' : 'Trực giao: ' + selO; render(); if (selO !== 'Trên') TC.toast('UCS trực giao ' + esc(selO) + ' hữu ích khi vẽ trên mặt đứng của mô hình 3D.', { type:'info', log:false }); }
      else if (k === 'odet') details({ name:selO, o:[0, 0], ang:0 });
      else if (k === 'oreset'){ ORT.forEach(function(x){ x[1] = 0; }); render(); }
      else if (k === 'iconprops') iconProps();
    });
    el.addEventListener('dblclick', function(e){ if (e.target.closest('[data-ucn]')){ cur = selN; render(); } });
    $('#uc-ok', el).addEventListener('click', function(){ d.ucsCur = cur; d.ucsSet = { allvp:$('#uc-all', el).checked, pick:$('#uc-pick', el).checked, perVp:$('#uc-pvp', el).checked, plan:$('#uc-plan', el).checked }; TC.S.ucsIcon = $('#uc-on', el).checked; TC.S.ucsOrigin = $('#uc-org', el).checked; TC.ucsIconApply(); dlg.close(true); TC.cmdline.msg('UCS hiện hành: <b>' + esc(cur) + '</b>.'); });
    TC.listKeys($('#uc-n', el), '[data-ucn]', function(n, enter){ selN = n.dataset.ucn; render(); if (enter){ cur = selN; render(); } });
    render();
  };
  function iconProps(){
    var o = TC.load('ucsIconProps', { style:'2d', lw:1, size:50, cm:'Trắng', cl:'Đen' });
    var dlg = TC.dialog({ id:'ucsicon', title:'Biểu tượng UCS', cmd:'UCSICON', code:'UI-D13', w:520,
      body:'<div class="uciw"><div class="stack">' + H.grp('Kiểu', H.rad('uci-s', '2d', '2D', o.style === '2d') + H.rad('uci-s', '3d', '3D (hình nón ở đầu trục)', o.style === '3d') + H.row('Độ dày nét', H.sel('uci-lw', ['1', '2', '3'], String(o.lw), 70), 90)) + H.grp('Cỡ biểu tượng', '<div class="hrow"><input type="range" id="uci-sz" min="5" max="95" value="' + o.size + '" style="flex:1" aria-label="Cỡ biểu tượng"><span class="mono" id="uci-szv">' + o.size + '</span></div>') + H.grp('Màu', H.row('Model', H.sel('uci-cm', ['Trắng', 'Xám', 'Xanh lơ'], o.cm, 110), 90) + H.row('Layout', H.sel('uci-cl', ['Đen', 'Xám', 'Xanh dương'], o.cl, 110), 90)) + '</div><div class="ucipv"><canvas id="uci-c" width="150" height="150" class="pvc"></canvas><span class="faint" style="font-size:var(--fs-xs)">Xem trước</span></div></div>',
      foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="uci-ok">Áp dụng</button>' });
    function pv(){ var c = $('#uci-c', dlg.el), x = c.getContext('2d'), s = 30 + Number($('#uci-sz', dlg.el).value), w = Number($('#uci-lw', dlg.el).value), col = { 'Trắng':'#E8EAED', 'Xám':'#9AA1AA', 'Xanh lơ':'#5BD6CF' }[$('#uci-cm', dlg.el).value], is3 = $('[name="uci-s"]:checked', dlg.el).value === '3d'; x.fillStyle = TC.cssv('--c-canvas'); x.fillRect(0, 0, 150, 150); x.strokeStyle = x.fillStyle = col; x.lineWidth = w; var ox = 30, oy = 120; x.beginPath(); x.moveTo(ox, oy); x.lineTo(ox + s, oy); x.moveTo(ox, oy); x.lineTo(ox, oy - s); x.stroke(); if (is3){ [[ox + s, oy, 1, 0], [ox, oy - s, 0, -1]].forEach(function(a){ x.beginPath(); x.moveTo(a[0] + a[2] * 8, a[1] + a[3] * 8); x.lineTo(a[0] + a[3] * 4, a[1] - a[2] * 4); x.lineTo(a[0] - a[3] * 4, a[1] + a[2] * 4); x.fill(); }); } else { x.strokeRect(ox, oy - 8, 8, 8); } x.font = '11px Arial'; x.fillText('X', ox + s + 4, oy + 4); x.fillText('Y', ox - 4, oy - s - 6); $('#uci-szv', dlg.el).textContent = $('#uci-sz', dlg.el).value; }
    dlg.el.addEventListener('input', pv); dlg.el.addEventListener('change', pv);
    $('#uci-ok', dlg.el).addEventListener('click', function(){ o = { style:$('[name="uci-s"]:checked', dlg.el).value, lw:Number($('#uci-lw', dlg.el).value), size:Number($('#uci-sz', dlg.el).value), cm:$('#uci-cm', dlg.el).value, cl:$('#uci-cl', dlg.el).value }; TC.save('ucsIconProps', o); var ui = $('.cv-ucs'); if (ui){ ui.style.width = ui.style.height = (40 + o.size * .5) + 'px'; ui.style.color = { 'Trắng':'#C9CED6', 'Xám':'#9AA1AA', 'Xanh lơ':'#5BD6CF' }[o.cm]; } dlg.close(true); });
    pv();
  }
  IMPL.UCSMAN = function(ctx, pre){ SC.ucsMan(pre); };
  IMPL.UCSICON = function(ctx){ return ctx.kw('Biểu tượng UCS', [['Bật', 'ON'], ['Tắt', 'OFF'], ['Tại gốc', 'OR'], ['Không tại gốc', 'N'], ['Thuộc tính', 'P']], TC.S.ucsIcon === false ? 'ON' : 'OFF').then(function(k){ if (k === 'P') return iconProps(); if (k === 'ON' || k === 'OFF') TC.S.ucsIcon = k === 'ON'; else TC.S.ucsOrigin = k === 'OR'; TC.ucsIconApply(); ctx.out('Biểu tượng UCS: ' + (TC.S.ucsIcon === false ? 'tắt' : 'bật') + (TC.S.ucsOrigin ? ', hiện tại gốc' : '') + '.'); }); };
  OPEN['UI-D13'] = function(){ SC.cat.PRO(); SC.ucsMan(); };

  /* ================= ViewCube, xoay 3D, điều khiển viewport (07.1 §9.13) ================= */
  function cube(){ var c = $('#vcube'); if (TC.S.vcube && !c){ c = TC.el('<div class="vcube" id="vcube" aria-label="ViewCube"><svg viewBox="0 0 120 120" aria-hidden="true"><circle cx="60" cy="62" r="44" class="vcr"/><g class="vcd"><text x="60" y="13">B</text><text x="111" y="66">Đ</text><text x="60" y="117">N</text><text x="9" y="66">T</text></g><path d="M38 40h44v44H38z" class="vcf" data-vcf="Trên"/><path d="M38 40l8-8h44l-8 8z" class="vcs" data-vcf="Sau"/><path d="M82 40l8-8v44l-8 8z" class="vcs" data-vcf="Phải"/><text x="60" y="66" class="vct">TRÊN</text></svg><button type="button" class="vch" data-vcf="home" aria-label="Về view gốc" data-tip="Về view gốc">' + ic('home', 's12') + '</button><button type="button" class="vcu" data-vcf="ucs">WCS ' + ic('down', 'xs') + '</button></div>'); $('#cv').appendChild(c); cubeTop(); c.addEventListener('click', function(e){ var f = e.target.closest('[data-vcf]'); if (!f) return; var k = f.getAttribute('data-vcf'); if (k === 'home' || k === 'Trên'){ V().zoomExtents(); return; } if (k === 'ucs'){ var r = f.getBoundingClientRect(); TC.menu([{ label:'WCS (Thế giới)', check:true }, { sep:1 }, { label:'UCS mới…', act:function(){ TC.cmd.run('UCS'); } }, { label:'Quản lý UCS…', act:function(){ TC.cmd.run('UCSMAN'); } }], r.left, r.bottom + 2); return; } TC.toast('Bản vẽ chỉ có đối tượng 2D; hướng nhìn ' + esc(k) + ' dùng với bản vẽ có đối tượng 3D.', { type:'info', log:false }); }); } else if (!TC.S.vcube && c) c.remove(); }
  function cubeTop(){ var c = $('#vcube'), b = $('#banners'); if (c) c.style.top = ((b ? b.offsetHeight : 0) + 30) + 'px'; }
  ['doc', 'view', 'banners'].forEach(function(ev){ TC.on(ev, function(){ setTimeout(cubeTop, 0); }); });
  IMPL.NAVVCUBE = function(ctx){ return ctx.kw('ViewCube', [['Bật', 'ON'], ['Tắt', 'OFF']], TC.S.vcube ? 'OFF' : 'ON').then(function(k){ TC.S.vcube = k === 'ON'; cube(); ctx.out('ViewCube ' + (TC.S.vcube ? 'bật. Mặc định ẩn ở bản vẽ 2D; tự hiện khi bản vẽ có đối tượng 3D.' : 'tắt.')); }); };
  IMPL['3DORBIT'] = function(ctx){ ctx.out('Xoay 3D: kéo chuột để xoay quanh mô hình, Esc để thoát. Bản vẽ này chỉ có đối tượng 2D (Z = 0) nên giữ hướng nhìn mặt bằng.'); TC.toast('Bản vẽ chỉ có đối tượng 2D. Xoay 3D, ViewCube và kiểu hiển thị Shaded có tác dụng với bản vẽ có đối tượng 3D.', { type:'info', log:false }); };
  TC.vpcMenu = function(k, b){
    var r = b.getBoundingClientRect(), d = D();
    var items = k === 'm' ? [{ label:'Phóng to viewport', icon:'fullscreen', dis:!d || d.space === 'model', act:function(){ TC.cmd.run('VPMAX'); } }, { label:'Cấu hình viewport…', icon:'ws', need:'PRO', act:function(){ TC.cmd.run('VPORTS'); } }, { sep:1 }, { label:'ViewCube', check:!!TC.S.vcube, act:function(){ TC.S.vcube = !TC.S.vcube; cube(); } }, { label:'Biểu tượng UCS', check:TC.S.ucsIcon !== false, act:function(){ TC.S.ucsIcon = TC.S.ucsIcon === false; TC.ucsIconApply(); } }, { label:'Navigation bar <span class="faint">(sau v1.0)</span>', check:false, dis:true }]
      : k === 'v' ? STDV.map(function(x, i){ return { label:x, check:i === 0, act:function(){ if (i) TC.toast('Bản vẽ chỉ có đối tượng 2D.', { type:'info', log:false }); } }; }).concat([{ sep:1 }, { label:'Trình quản lý view…', icon:'views', act:function(){ TC.cmd.run('VIEW'); } }]).concat(d && d.views.length ? [{ label:'View đã đặt tên', sub:d.views.map(function(v){ return { label:esc(v.name), act:function(){ goView(d, v); } }; }) }] : [])
      : ['2D Wireframe', 'Wireframe', 'Hidden', 'Shaded', 'Realistic'].map(function(x, i){ return { label:x, check:i === 0, act:function(){ if (i) TC.toast('Bản vẽ chỉ có đối tượng 2D; kiểu hiển thị 3D không đổi hình.', { type:'info', log:false }); } }; });
    TC.menu(items, r.left, r.bottom + 2, { returnFocus:b });
  };

  /* ================= nhãn phím theo nền tảng (07.1 §13.6) ================= */
  var MACSYS = { 'Ctrl+Y':'⇧⌘Z', 'Ctrl+F4':'⌘W', 'Ctrl+Tab':'⌃Tab', 'Ctrl+Shift+Tab':'⌃⇧Tab', 'Ctrl+PgDn':'⌥⌘→', 'Ctrl+PgUp':'⌥⌘←', 'Delete':'⌫', 'Del':'⌫' };
  var CMDKEYS = /^Ctrl\+(Shift\+)?[NOSPZXCVAQ0-9]$/; // phím hệ thống dùng ⌘ trên macOS; phím CAD giữ Ctrl (⌃)
  TC.isMac = function(){ return TC.S.platform === 'mac'; };
  TC.keyLabel = function(s){
    if (!s || !TC.isMac()) return s;
    return String(s).split(' · ').map(function(k){ k = k.trim(); if (MACSYS[k]) return MACSYS[k]; var m = k.split('+'), key = m.pop(), mods = m, cmd = CMDKEYS.test(k); return (mods.indexOf('Ctrl') >= 0 && !cmd ? '⌃' : '') + (mods.indexOf('Alt') >= 0 ? '⌥' : '') + (mods.indexOf('Shift') >= 0 ? '⇧' : '') + (cmd ? '⌘' : '') + key; }).join(' · ');
  };

  /* ================= Phím tắt UI-K06 ================= */
  // [id, nhóm, tên, lệnh, phím mặc định (cách nhau " · "), phạm vi, phím thay thế khi laptop macOS không có phím F riêng]
  var KDEF = [
    ['help', 'Chung', 'Trợ giúp theo ngữ cảnh', 'HELP', 'F1', 'Toàn cục', '⌘?'], ['hist', 'Chung', 'Lịch sử dòng lệnh đầy đủ', 'TEXTSCR', 'F2', 'Toàn cục', '⌃⌘H'],
    ['osnap', 'Hỗ trợ vẽ', 'Bật/tắt bắt điểm', 'OSNAP', 'F3', 'Toàn cục', '⌃F'], ['isop', 'Hỗ trợ vẽ', 'Đổi mặt isometric', 'ISOPLANE', 'F5 · Ctrl+E', 'Toàn cục', '⌃E'],
    ['grid', 'Hỗ trợ vẽ', 'Lưới', 'GRIDMODE', 'F7 · Ctrl+G', 'Toàn cục', '⌃G'], ['ortho', 'Hỗ trợ vẽ', 'Ortho', 'ORTHOMODE', 'F8 · Ctrl+L', 'Toàn cục', '⌃L'],
    ['snap', 'Hỗ trợ vẽ', 'Snap', 'SNAPMODE', 'F9 · Ctrl+B', 'Toàn cục', '⌃B'], ['polar', 'Hỗ trợ vẽ', 'Polar', 'AUTOSNAP', 'F10 · Ctrl+U', 'Toàn cục', '⌃U'],
    ['otrack', 'Hỗ trợ vẽ', 'Theo dõi bắt điểm', 'AUTOSNAP', 'F11', 'Toàn cục', '⌃K'], ['dyn', 'Hỗ trợ vẽ', 'Nhập động', 'DYNMODE', 'F12', 'Toàn cục', '⌃D'],
    ['qp', 'Hỗ trợ vẽ', 'Thuộc tính nhanh', 'QPMODE', 'Ctrl+Shift+P', 'Toàn cục'], ['infer', 'Hỗ trợ vẽ', 'Suy luận ràng buộc', 'CONSTRAINTINFER', 'Ctrl+Shift+I', 'Toàn cục'], ['cycle', 'Hỗ trợ vẽ', 'Chọn luân phiên', 'SELECTIONCYCLING', 'Ctrl+W', 'Vùng vẽ'],
    ['new', 'Tệp', 'Bản vẽ mới', 'NEW', 'Ctrl+N', 'Toàn cục'], ['open', 'Tệp', 'Mở bản vẽ', 'OPEN', 'Ctrl+O', 'Toàn cục'], ['save', 'Tệp', 'Lưu', 'QSAVE', 'Ctrl+S', 'Toàn cục'], ['saveas', 'Tệp', 'Lưu thành', 'SAVEAS', 'Ctrl+Shift+S', 'Toàn cục'],
    ['plot', 'Tệp', 'In', 'PLOT', 'Ctrl+P', 'Toàn cục'], ['close', 'Tệp', 'Đóng bản vẽ', 'CLOSE', 'Ctrl+F4', 'Toàn cục'], ['quit', 'Tệp', 'Thoát TrionCAD', 'QUIT', 'Ctrl+Q', 'Toàn cục'],
    ['undo', 'Sửa', 'Hoàn tác', 'U', 'Ctrl+Z', 'Toàn cục'], ['redo', 'Sửa', 'Làm lại', 'REDO', 'Ctrl+Y', 'Toàn cục'], ['cut', 'Sửa', 'Cắt', 'CUTCLIP', 'Ctrl+X', 'Vùng vẽ'], ['copy', 'Sửa', 'Sao chép', 'COPYCLIP', 'Ctrl+C', 'Vùng vẽ'],
    ['copybase', 'Sửa', 'Sao chép có điểm gốc', 'COPYBASE', 'Ctrl+Shift+C', 'Vùng vẽ'], ['paste', 'Sửa', 'Dán', 'PASTECLIP', 'Ctrl+V', 'Vùng vẽ'], ['pasteblock', 'Sửa', 'Dán dạng block', 'PASTEBLOCK', 'Ctrl+Shift+V', 'Vùng vẽ'],
    ['selall', 'Sửa', 'Chọn tất cả', 'SELECTALL', 'Ctrl+A', 'Vùng vẽ'], ['erase', 'Sửa', 'Xóa đối tượng đã chọn', 'ERASE', 'Delete', 'Vùng vẽ'],
    ['props', 'Palette, hiển thị', 'Palette Thuộc tính', 'PROPERTIES', 'Ctrl+1', 'Toàn cục'], ['dc', 'Palette, hiển thị', 'DesignCenter', 'ADCENTER', 'Ctrl+2', 'Toàn cục'], ['tp', 'Palette, hiển thị', 'Tool Palettes', 'TOOLPALETTES', 'Ctrl+3', 'Toàn cục'], ['ssm', 'Palette, hiển thị', 'Sheet Set Manager', 'SHEETSET', 'Ctrl+4', 'Toàn cục'], ['qc', 'Palette, hiển thị', 'Máy tính nhanh', 'QUICKCALC', 'Ctrl+8', 'Toàn cục'],
    ['cmdl', 'Palette, hiển thị', 'Bật/tắt dòng lệnh', 'COMMANDLINE', 'Ctrl+9', 'Toàn cục'], ['clean', 'Palette, hiển thị', 'Clean Screen', 'CLEANSCREENON', 'Ctrl+0', 'Toàn cục'], ['coord', 'Palette, hiển thị', 'Kiểu hiển thị tọa độ', 'COORDS', 'Ctrl+I', 'Toàn cục'],
    ['ntab', 'Palette, hiển thị', 'Tab tệp kế / trước', '—', 'Ctrl+Tab · Ctrl+Shift+Tab', 'Toàn cục'], ['nlay', 'Palette, hiển thị', 'Layout kế / trước', '—', 'Ctrl+PgDn · Ctrl+PgUp', 'Toàn cục'], ['vpnext', 'Palette, hiển thị', 'Chuyển viewport kích hoạt', 'VPNEXT', 'Ctrl+R', 'Vùng vẽ']
  ];
  var FIXED = [['Esc', 'Hủy lệnh, bỏ chọn'], ['Enter · Space', 'Xác nhận; lặp lệnh vừa dùng'], ['Tab', 'Luân phiên điểm bắt, chuyển ô nhập động'], ['↑ · ↓', 'Lịch sử nhập trên dòng lệnh'], ['Shift (giữ)', 'Tạm đảo Ortho'], ['Shift + chuột phải', 'Menu ghi đè bắt điểm'], ['Chuột giữa (giữ) · nhấp đúp', 'Pan · Zoom toàn bộ']];
  var RESERVED = { 'Ctrl+Alt+Delete':'Windows', 'Alt+F4':'Windows', 'Alt+Tab':'Windows', 'Ctrl+Shift+Esc':'Windows', 'Ctrl+Space':'bộ gõ (đổi kiểu gõ tiếng Việt)', 'Ctrl+Shift+3':'macOS (chụp màn hình)', 'Ctrl+Shift+4':'macOS (chụp màn hình)', 'Ctrl+H':'macOS (ẩn ứng dụng)', 'Ctrl+M':'macOS (thu nhỏ cửa sổ)' };
  var userKeys = TC.load('keys', {}); // id hoặc lệnh → chuỗi phím (thay mặc định)
  function keysOf(id){ var def = KDEF.filter(function(k){ return k[0] === id; })[0]; return id in userKeys ? userKeys[id] : def ? def[4] : ''; }
  function allRows(){ var seen = {}; KDEF.forEach(function(k){ seen[k[3]] = 1; }); return KDEF.map(function(k){ return { id:k[0], grp:k[1], name:k[2], cmd:k[3], scope:k[5], alt:k[6] }; }).concat(Object.keys(TC.CMDS).filter(function(n){ return !seen[n]; }).sort().map(function(n){ var c = TC.CMDS[n]; return { id:n, grp:'Lệnh khác', name:c.desc, cmd:n, scope:'Toàn cục' }; })); }
  SC.keyRows = function(){ return allRows().map(function(r){ return { id:r.id, name:r.name, cmd:r.cmd, keys:keysOf(r.id) }; }).filter(function(r){ return r.keys; }); }; // CUI (UI-K07) đọc bảng phím một nguồn
  function owner(combo, except){ var hit = null; allRows().forEach(function(r){ if (r.id !== except && keysOf(r.id).split(' · ').indexOf(combo) >= 0) hit = r; }); return hit; }
  function comboOf(e){ var k = e.key; if (/^(Control|Shift|Alt|Meta)$/.test(k)) return null; if (k === ' ') k = 'Space'; else if (k.length === 1) k = k.toUpperCase(); else if (k === 'PageDown') k = 'PgDn'; else if (k === 'PageUp') k = 'PgUp'; return (e.ctrlKey || e.metaKey ? 'Ctrl+' : '') + (e.altKey ? 'Alt+' : '') + (e.shiftKey ? 'Shift+' : '') + k; }
  var ACT = { infer:function(){ A().toggle('infer'); }, osnap:function(){ A().toggle('osnap'); }, grid:function(){ A().toggle('grid'); }, ortho:function(){ A().toggle('ortho'); }, snap:function(){ A().toggle('snap'); }, polar:function(){ A().toggle('polar'); }, otrack:function(){ A().toggle('otrack'); }, dyn:function(){ A().toggle('dyn'); }, qp:function(){ A().toggle('qp'); }, cycle:function(){ A().toggle('cycle'); }, isop:function(){ TC.S.isoFace = ((TC.S.isoFace || 0) + 1) % 3; TC.cmdline.msg('&lt;Mặt isometric: ' + ['Trái', 'Trên', 'Phải'][TC.S.isoFace] + '&gt;'); },
    cmdl:function(){ $('#app').classList.toggle('no-cmd'); setTimeout(function(){ V().resize(); }, 0); }, clean:function(){ A().cleanScreen(); }, help:function(){ TC.openHelp(TC.cmdline.current()); }, hist:function(){ SC.textWindow(); },
    vpnext:function(){ var d = D(); if (!d || d.space === 'model'){ TC.cmdline.msg('&lt;Ctrl+R: chỉ dùng trong layout có nhiều viewport&gt;'); return; } var vps = d.layouts[d.space].vports; if (!vps.length) return; var i = vps.map(function(v){ return v.id; }).indexOf(d.activeVp); d.activeVp = vps[(i + 1) % vps.length].id; V().draw(); A().renderStatus(); TC.cmdline.msg('&lt;Viewport kích hoạt: ' + ((i + 1) % vps.length + 1) + '/' + vps.length + '&gt;'); } };
  // phím mới của Đợt 4 và phím người dùng gán: chạy trước bộ xử lý phím của khung (shell.js)
  var NEWDEF = { 'Ctrl+E':'isop', F5:'isop', 'Ctrl+G':'grid', 'Ctrl+L':'ortho', 'Ctrl+B':'snap', 'Ctrl+U':'polar', 'Ctrl+W':'cycle', 'Ctrl+Shift+P':'qp', 'Ctrl+2':'dc', 'Ctrl+3':'tp', 'Ctrl+4':'ssm', 'Ctrl+8':'qc', 'Ctrl+Shift+I':'infer', 'Ctrl+R':'vpnext', 'Ctrl+Q':'quit' };
  document.addEventListener('keydown', function(e){
    if (e.defaultPrevented || A().active === 'start' && !/^F\d+$/.test(e.key)) return;
    var t = e.target, inField = (t.tagName === 'INPUT' || t.tagName === 'TEXTAREA' || t.tagName === 'SELECT' || t.isContentEditable) && t.id !== 'cmd-in';
    if (inField || TC.topDialog() && TC.topDialog().el.getAttribute('aria-modal') === 'true' || $('.menu') || keyCap) return;
    var c = comboOf(e); if (!c) return;
    if (TC.isMac() && e.ctrlKey && !e.metaKey){ var mac = KDEF.filter(function(k){ return k[6] === '⌃' + e.key.toUpperCase(); })[0]; if (mac){ e.preventDefault(); run(mac[0], mac[3]); return; } }
    var hit = allRows().filter(function(r){ return keysOf(r.id).split(' · ').indexOf(c) >= 0; })[0];
    if (hit && (hit.id in userKeys || NEWDEF[c] === hit.id || hit.grp === 'Lệnh khác')){ e.preventDefault(); run(hit.id, hit.cmd); return; }
    if (!hit && KDEF.some(function(k){ return k[4].split(' · ').indexOf(c) >= 0 && k[0] in userKeys; })){ e.preventDefault(); } // phím mặc định đã bị người dùng gỡ
  });
  function run(id, cmd){ if (ACT[id]){ if (TC.isViewer() && /grid|ortho|snap|polar|otrack|dyn|qp|cycle|isop|infer/.test(id)){ TC.cmdline.msg('&lt;Có trong TrionCAD Pro&gt;'); return; } ACT[id](); } else if (cmd && TC.CMDS[cmd]) TC.cmd.run(cmd); }
  var keyCap = false;
  SC.shortcuts = function(selId){
    var st = { q:'', f:'all', sel:selId || 'save', cap:false }, draft = JSON.parse(JSON.stringify(userKeys));
    var kOf = function(id){ var def = KDEF.filter(function(k){ return k[0] === id; })[0]; return id in draft ? draft[id] : def ? def[4] : ''; };
    var dlg = TC.dialog({ id:'keys', title:'Phím tắt', cmd:'TCKEYS', code:'UI-K06', w:960, h:620, flush:true, onClose:function(){ keyCap = false; },
      body:'<div class="kyw">' + H.tabs([['k', 'Phím tắt'], ['o', 'Phím ghi đè tạm thời · sau v1.0']], 'k', 'kyt') + '<div class="pal-tools"><label class="search" style="width:300px">' + ic('search') + '<input id="ky-q" placeholder="Tìm lệnh hoặc phím: lưu, Ctrl+S, F3" aria-label="Tìm phím tắt" spellcheck="false"></label><span class="seg" id="ky-f"></span><span class="grow"></span><button class="btn sm" type="button" data-ky="imp">' + ic('open') + 'Nhập…</button><button class="btn sm" type="button" data-ky="exp">' + ic('saveas') + 'Xuất…</button><button class="btn sm" type="button" data-ky="print">' + ic('print') + 'In bảng phím tắt…</button></div>' +
        '<div class="kymain"><div class="tblwrap" id="ky-t"></div><div class="kyd" id="ky-d"></div></div></div>',
      foot:'<button class="btn ghost" type="button" data-ky="resetall">Đặt lại tất cả</button><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="ky-ok">Lưu phím tắt</button>' });
    var el = dlg.el;
    function chips(s){ return s ? s.split(' · ').map(function(k){ return '<kbd class="kc">' + esc(TC.keyLabel(k)) + '</kbd>'; }).join('') : '<span class="faint">—</span>'; }
    function rows(){ var q = TC.fold(st.q.trim()); return allRows().filter(function(r){ var k = kOf(r.id); if (st.f === 'set' && !k) return false; if (st.f === 'chg' && !(r.id in draft)) return false; if (st.f === 'all' && r.grp === 'Lệnh khác' && !k && !q) return false; return !q || TC.fold(r.name + ' ' + r.cmd + ' ' + k + ' ' + TC.keyLabel(k)).indexOf(q) >= 0; }); }
    function render(){
      var list = rows(), nChg = Object.keys(draft).length, grp = '';
      $('#ky-f', el).innerHTML = [['all', 'Thường dùng'], ['set', 'Đã gán'], ['chg', 'Đã đổi (' + nChg + ')']].map(function(f){ return '<button type="button" data-kyf="' + f[0] + '" aria-pressed="' + (st.f === f[0]) + '">' + f[1] + '</button>'; }).join('');
      $('#ky-t', el).innerHTML = '<table class="tbl kyt"><colgroup><col><col style="width:150px"><col style="width:210px"><col style="width:96px"></colgroup><thead><tr><th>Lệnh hoặc hành động</th><th>Tên lệnh</th><th>Phím tắt</th><th>Phạm vi</th></tr></thead><tbody>' + list.map(function(r){ var h = r.grp !== grp ? '<tr class="kyg"><td colspan="4">' + esc(r.grp) + '</td></tr>' : ''; grp = r.grp; return h + '<tr data-kyr="' + esc(r.id) + '" class="' + (st.sel === r.id ? 'is-sel' : '') + '"><td>' + esc(r.name) + (r.id in draft ? ' <span class="tag info">đã đổi</span>' : '') + '</td><td class="mono faint">' + esc(r.cmd) + '</td><td>' + chips(kOf(r.id)) + (TC.isMac() && r.alt ? '<kbd class="kc alt" title="Thay thế khi bàn phím không có phím F riêng">' + r.alt + '</kbd>' : '') + '</td><td>' + r.scope + '</td></tr>'; }).join('') + '</tbody></table>' + (st.f === 'all' && !st.q ? '<p class="faint" style="padding:8px 10px;margin:0;font-size:var(--fs-s)">Lệnh chưa có phím tắt không hiện ở đây; tìm theo tên để gán, ví dụ “offset”.</p>' : '');
      detail();
    }
    function detail(){
      var r = allRows().filter(function(x){ return x.id === st.sel; })[0], box = $('#ky-d', el); if (!r){ box.innerHTML = ''; return; }
      var ks = kOf(r.id) ? kOf(r.id).split(' · ') : [];
      box.innerHTML = '<div class="svh"><b>' + esc(r.name) + '</b></div><p class="mono faint" style="margin:0">' + esc(r.cmd) + ' · ' + esc(r.grp) + '</p>' +
        '<div class="gh">Phím đang gán</div><div class="kyks">' + (ks.length ? ks.map(function(k, i){ return '<span class="kyk"><kbd class="kc">' + esc(TC.keyLabel(k)) + '</kbd><button class="ib sm" type="button" data-kyx="' + i + '" aria-label="Gỡ ' + esc(k) + '">' + ic('x', 's12') + '</button></span>'; }).join('') : '<span class="faint">Chưa có phím tắt</span>') + '</div>' +
        '<div class="gh">Thêm phím</div><button class="kycap" type="button" id="ky-cap">' + (st.cap ? 'Nhấn tổ hợp phím… <span class="faint">(Esc để thôi)</span>' : 'Bấm vào đây rồi nhấn tổ hợp phím') + '</button><div id="ky-msg"></div>' +
        (TC.isMac() && r.alt ? '<div class="note">Trên laptop macOS, phím F cần bấm kèm Fn. TrionCAD thêm tổ hợp thay thế <kbd class="kc">' + r.alt + '</kbd>.</div>' : '') +
        '<div class="hrow"><button class="btn sm" type="button" data-ky="def"' + (r.id in draft ? '' : ' disabled') + '>' + ic('undo') + 'Đặt về mặc định</button></div>';
    }
    function setKeys(id, list){ var def = KDEF.filter(function(k){ return k[0] === id; })[0], v = list.join(' · '); if (def && v === def[4]) delete draft[id]; else if (!def && !v) delete draft[id]; else draft[id] = v; }
    function capture(e){
      if (!st.cap) return; e.preventDefault(); e.stopPropagation();
      if (e.key === 'Escape'){ st.cap = keyCap = false; detail(); return; }
      var c = comboOf(e); if (!c) return; var msg = $('#ky-msg', el), r = st.sel;
      if (RESERVED[c]){ msg.innerHTML = '<div class="fmsg err">' + ic('warn', 'xs') + esc(TC.keyLabel(c)) + ' do ' + RESERVED[c] + ' giữ, không gán được.</div>'; return; }
      if (!/^(Ctrl|Alt|F\d)/.test(c) && !/^F\d+$/.test(c)){ msg.innerHTML = '<div class="fmsg err">' + ic('warn', 'xs') + 'Phím đơn ' + esc(c) + ' dùng để gõ lệnh trên vùng vẽ. Dùng kèm Ctrl, Alt hoặc phím F. Muốn gõ tắt kiểu “L” thì đặt ở Viết tắt lệnh.</div><button class="btn sm" type="button" data-ky="alias" style="margin-top:6px">Mở Viết tắt lệnh…</button>'; return; }
      st.cap = keyCap = false;
      var own = (function(){ var hit = null; allRows().forEach(function(x){ if (x.id !== r && kOf(x.id).split(' · ').indexOf(c) >= 0) hit = x; }); return hit; })();
      if (own){ detail(); $('#ky-msg', el).innerHTML = '<div class="kyconf">' + ic('warn') + '<div><b>' + esc(TC.keyLabel(c)) + '</b> đang dùng cho <b>' + esc(own.name) + '</b> (' + esc(own.cmd) + ').<div class="hrow" style="margin-top:6px"><button class="btn sm primary" type="button" data-kyre="' + esc(c) + '" data-kyfrom="' + esc(own.id) + '">Gán lại cho lệnh này</button><button class="btn sm ghost" type="button" data-ky="nocap">Giữ như cũ</button></div></div></div>'; return; }
      var cur = kOf(r) ? kOf(r).split(' · ') : []; if (cur.indexOf(c) < 0) cur.push(c); setKeys(r, cur); render();
    }
    el.addEventListener('keydown', capture, true);
    $('#ky-q', el).addEventListener('input', function(){ st.q = this.value; render(); });
    el.addEventListener('click', function(e){
      var f = e.target.closest('[data-kyf]'); if (f){ st.f = f.dataset.kyf; render(); return; }
      var r = e.target.closest('[data-kyr]'); if (r){ st.sel = r.dataset.kyr; st.cap = keyCap = false; render(); return; }
      if (e.target.closest('#ky-cap')){ st.cap = keyCap = true; detail(); $('#ky-cap', el).focus(); return; }
      var x = e.target.closest('[data-kyx]'); if (x){ var cur = kOf(st.sel).split(' · '); cur.splice(Number(x.dataset.kyx), 1); setKeys(st.sel, cur); render(); return; }
      var re = e.target.closest('[data-kyre]'); if (re){ var c = re.dataset.kyre, from = re.dataset.kyfrom; setKeys(from, kOf(from).split(' · ').filter(function(k){ return k !== c; })); var cur2 = kOf(st.sel) ? kOf(st.sel).split(' · ') : []; cur2.push(c); setKeys(st.sel, cur2); render(); return; }
      var b = e.target.closest('[data-ky]'); if (!b) return; var k = b.dataset.ky;
      if (k === 'def'){ delete draft[st.sel]; render(); }
      else if (k === 'nocap'){ detail(); }
      else if (k === 'alias'){ TC.cmd.run('ALIASEDIT'); }
      else if (k === 'resetall') TC.confirm({ title:'Đặt lại phím tắt', msg:'Đặt lại ' + Object.keys(draft).length + ' phím đã đổi về mặc định của TrionCAD?', ok:'Đặt lại' }).then(function(y){ if (y){ draft = {}; render(); } });
      else if (k === 'exp') TC.toast('Đã xuất phím tắt ra <span class="mono">phim-tat.tckeys</span>. Mang sang máy khác bằng Nhập….', { act:{ label:'Mở thư mục', fn:function(){} } });
      else if (k === 'imp') TC.confirm({ title:'Nhập phím tắt', msg:'<b class="mono">phim-tat-phong-KT.tckeys</b> có 6 phím: 4 phím mới, 2 phím trùng với phím hiện có (Ctrl+Shift+D, F4).', detail:'Phím trùng sẽ chuyển sang lệnh trong tệp nhập. Có thể Hủy trước khi Lưu phím tắt.', ok:'Nhập 6 phím' }).then(function(y){ if (!y) return; draft.OFFSET = 'Ctrl+Shift+O'; draft.TCLENGTH = 'Ctrl+Shift+L'; draft.TCAREA = 'Ctrl+Shift+A'; draft.FIXFONT = 'Ctrl+Shift+F'; draft.TCBATCHPLOT = 'Ctrl+Shift+D'; draft.TCLIB = 'F4'; st.f = 'chg'; render(); });
      else if (k === 'print') cheatSheet(function(id){ return kOf(id); });
    });
    el.addEventListener('dtab', function(e){ if (e.detail === 'o'){ TC.toast('Phím ghi đè tạm thời (giữ phím để tạm bật bắt điểm, Ortho…) có ở sau v1.0.', { type:'info', log:false }); $('[data-dt="k"]', el).click(); } });
    $('#ky-ok', el).addEventListener('click', function(){ var n = Object.keys(draft).length; userKeys = draft; TC.save('keys', userKeys); dlg.close(true); TC.cmdline.msg('Đã lưu phím tắt' + (n ? ' (' + n + ' phím khác mặc định)' : '') + '.'); });
    render();
  };
  function cheatSheet(kOf){
    var groups = {}; KDEF.forEach(function(k){ (groups[k[1]] = groups[k[1]] || []).push([k[2], kOf(k[0])]); });
    var al = [['L', 'LINE'], ['PL', 'PLINE'], ['C', 'CIRCLE'], ['A', 'ARC'], ['REC', 'RECTANG'], ['H', 'HATCH'], ['T', 'MTEXT'], ['DIM', 'DIM'], ['M', 'MOVE'], ['CO', 'COPY'], ['RO', 'ROTATE'], ['MI', 'MIRROR'], ['SC', 'SCALE'], ['TR', 'TRIM'], ['EX', 'EXTEND'], ['O', 'OFFSET'], ['F', 'FILLET'], ['X', 'EXPLODE'], ['I', 'INSERT'], ['B', 'BLOCK'], ['LA', 'LAYER'], ['MA', 'MATCHPROP']];
    var col = function(t, rows, cls){ return '<section' + (cls ? ' class="' + cls + '"' : '') + '><h4>' + esc(t) + '</h4><table>' + rows.map(function(r){ return '<tr><td>' + esc(r[0]) + '</td><td>' + (r[1] ? r[1].split(' · ').map(function(k){ return '<kbd>' + esc(TC.keyLabel(k)) + '</kbd>'; }).join(' ') : '—') + '</td></tr>'; }).join('') + '</table></section>'; };
    var dlg = TC.dialog({ id:'keysheet', title:'In bảng phím tắt', code:'UI-K06', w:1000, h:720, flush:true,
      body:'<div class="ksw"><div class="kspaper"><header><b>TrionCAD · Bảng phím tắt</b><span>' + (TC.isMac() ? 'macOS' : 'Windows') + ' · in ngày ' + TC.date() + '</span></header><div class="kscols">' + Object.keys(groups).map(function(g){ return col(g, groups[g]); }).join('') + col('Phím cố định', FIXED.map(function(f){ return [f[1], f[0]]; }), 'fx') + '<section><h4>Gõ tắt lệnh hay dùng</h4><table>' + al.map(function(a){ return '<tr><td>' + a[1] + '</td><td><kbd>' + a[0] + '</kbd></td></tr>'; }).join('') + '</table></section></div><footer>Gõ lệnh tiếng Anh hoặc từ khóa tiếng Việt trên dòng lệnh · F1 mở trợ giúp của lệnh đang chạy</footer></div></div>',
      foot:'<span class="faint" style="font-size:var(--fs-s)">Khổ A4 ngang, một trang, đen trắng</span><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Đóng</button><button class="btn" type="button" id="ks-pdf">' + ic('pdf') + 'Lưu PDF…</button><button class="btn primary" type="button" id="ks-print">' + ic('print') + 'In…</button>' });
    $('#ks-pdf', dlg.el).addEventListener('click', function(){ dlg.close(true); TC.toast('Đã lưu <span class="mono">TrionCAD-phim-tat.pdf</span> (A4 ngang, 1 trang).', { act:{ label:'Mở', fn:function(){} } }); });
    $('#ks-print', dlg.el).addEventListener('click', function(){ dlg.close(true); TC.cmd.run('PLOT'); });
  }
  IMPL.TCKEYS = function(){ SC.shortcuts(); };
  OPEN['UI-K06'] = function(){ SC.cat.PRO(); SC.shortcuts('save'); };

  /* ================= Workspace UI-K08 (WORKSPACE, WSSAVE, WSSETTINGS) ================= */
  var BUILTIN = [['2d', 'Soạn thảo 2D'], ['classic', 'Cổ điển'], ['viewer', 'Viewer']];
  var WS = TC.WS = TC.load('wsUser', null) || { list:[{ name:'Kiến trúc · 2 màn hình', base:'2d', pal:{ right:['props', 'layers'], rightActive:'props', rightPinned:true, left:['tcvnlib', 'blocks', 'fontdoc'], leftActive:'tcvnlib', leftPinned:true }, ft:true, lt:true, cmd:true }, { name:'In ấn và xuất', base:'2d', pal:{ right:['props', 'layers'], rightActive:'layers', rightPinned:false, left:[], leftActive:null, leftPinned:false }, ft:true, lt:true, cmd:true }], cur:null, mine:'Kiến trúc · 2 màn hình', autosave:false, hide:{}, order:null };
  function saveWS(){ TC.save('wsUser', WS); }
  function wsNames(){ var all = BUILTIN.map(function(b){ return b[1]; }).concat(WS.list.map(function(u){ return u.name; })); if (WS.order) all.sort(function(a, b){ var i = WS.order.indexOf(a), j = WS.order.indexOf(b); return (i < 0 ? 99 : i) - (j < 0 ? 99 : j); }); return all; }
  function curName(){ return WS.cur || (BUILTIN.filter(function(b){ return b[0] === A().ws; })[0] || BUILTIN[0])[1]; }
  function capture(){ return { base:A().ws, pal:JSON.parse(JSON.stringify(A().pal)), ft:!$('#ft').hidden, lt:!$('#lt').hidden, cmd:!$('#app').classList.contains('no-cmd') }; }
  SC.wsSwitch = function(name){
    if (WS.autosave && WS.cur){ var u0 = WS.list.filter(function(u){ return u.name === WS.cur; })[0]; if (u0) Object.assign(u0, capture()); }
    var b = BUILTIN.filter(function(x){ return x[1] === name; })[0];
    if (b){ WS.cur = null; saveWS(); A().setWs(b[0]); return; }
    var u = WS.list.filter(function(x){ return x.name === name; })[0]; if (!u) return;
    WS.cur = u.name; saveWS(); Object.keys(u.pal).forEach(function(k){ A().pal[k] = JSON.parse(JSON.stringify(u.pal[k])); }); $('#ft').hidden = !u.ft; $('#lt').hidden = !u.lt; $('#app').classList.toggle('no-cmd', !u.cmd);
    A().setWs(u.base); A().renderDocks(); TC.cmdline.msg('&lt;Workspace: ' + esc(u.name) + '&gt;');
  };
  TC.wsMenu = function(btn){
    var r = btn.getBoundingClientRect(), cur = curName(), names = wsNames().filter(function(n){ return !WS.hide[n]; });
    TC.menu([{ h:'Workspace' }, { label:'Workspace của tôi: ' + esc(WS.mine || names[0]), icon:'home', act:function(){ SC.wsSwitch(WS.mine || names[0]); } }, { sep:1 }].concat(names.map(function(n){ return { label:esc(n) + (WS.list.some(function(u){ return u.name === n; }) ? '' : ' <span class="faint">· có sẵn</span>'), check:n === cur, act:function(){ SC.wsSwitch(n); } }; })).concat([{ sep:1 }, { label:'Lưu workspace hiện tại…', icon:'save', act:function(){ TC.cmd.run('WSSAVE'); } }, { label:'Thiết lập workspace…', icon:'gear', act:function(){ TC.cmd.run('WSSETTINGS'); } }, { label:'Tùy biến giao diện…', icon:'gear', need:'PRO', kbd:'CUI', act:function(){ TC.cmd.run('CUI'); } }]), r.right, r.top, { alignRight:true, up:true, minWidth:260 });
  };
  SC.wsSave = function(){
    return new Promise(function(res){
      var dlg = TC.dialog({ id:'wssave', title:'Lưu workspace', cmd:'WSSAVE', code:'UI-K08', w:480, onClose:function(r){ if (r !== true) res(null); },
        body:'<div class="stack"><div class="field"><label for="ws-n">Tên workspace</label><input class="inp" id="ws-n" list="ws-nl" value="' + esc(WS.cur || 'Workspace của tôi') + '"><datalist id="ws-nl">' + WS.list.map(function(u){ return '<option>' + esc(u.name) + '</option>'; }).join('') + '</datalist><span class="fmsg" id="ws-m"></span></div>' +
          H.grp('Lưu lại', '<ul class="wsl"><li>' + ic('check', 's12') + (A().ws === 'classic' ? 'Thanh menu và toolbar (workspace Cổ điển)' : 'Ribbon') + '</li><li>' + ic('check', 's12') + 'Palette đang mở, vị trí dock, ghim hoặc tự ẩn</li><li>' + ic('check', 's12') + 'Tab tệp, tab layout, dòng lệnh: hiện hoặc ẩn</li><li>' + ic('check', 's12') + 'Mục trên thanh trạng thái</li></ul>') + H.chk('ws-go', 'Chuyển sang workspace này sau khi lưu', true) + '</div>',
        foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="ws-ok">Lưu workspace</button>' });
      setTimeout(function(){ $('#ws-n', dlg.el).select(); }, 0);
      $('#ws-ok', dlg.el).addEventListener('click', function(){ var v = $('#ws-n', dlg.el).value.trim(), m = $('#ws-m', dlg.el);
        if (!v || BUILTIN.some(function(b){ return b[1].toLowerCase() === v.toLowerCase(); })){ m.className = 'fmsg err'; m.innerHTML = ic('warn', 'xs') + (v ? '“' + esc(v) + '” là workspace có sẵn, không ghi đè được. Chọn tên khác.' : 'Nhập tên workspace.'); return; }
        var ex = WS.list.filter(function(u){ return u.name === v; })[0], go = function(){ var snapW = Object.assign({ name:v }, capture()); if (ex) Object.assign(ex, snapW); else WS.list.push(snapW); if ($('#ws-go', dlg.el).checked) WS.cur = v; saveWS(); dlg.close(true); TC.cmdline.msg('Đã lưu workspace <b>' + esc(v) + '</b>.'); A().renderStatus(); res(v); };
        if (ex) TC.confirm({ title:'Ghi đè workspace', msg:'Đã có workspace “<b>' + esc(v) + '</b>”. Ghi đè bằng bố cục hiện tại?', ok:'Ghi đè' }).then(function(y){ if (y) go(); }); else go(); });
    });
  };
  SC.wsSettings = function(){
    var order = wsNames(), hide = JSON.parse(JSON.stringify(WS.hide)), sel = order[0];
    var dlg = TC.dialog({ id:'wsset', title:'Thiết lập workspace', cmd:'WSSETTINGS', code:'UI-K08', w:560,
      body:'<div class="stack">' + H.row('Workspace của tôi', H.sel('wss-mine', order, WS.mine || order[0], 260), 150) + '<p class="faint" style="margin:-4px 0 0 150px;font-size:var(--fs-s)">Mục “Workspace của tôi” đầu menu workspace (bánh răng trên thanh trạng thái) chuyển nhanh về workspace này.</p>' +
        H.grp('Hiện trong menu và thứ tự', '<div class="wsord"><div class="wsol" id="wss-l" tabindex="0"></div><div class="stack" style="gap:4px"><button class="btn sm" type="button" data-wss="up">' + ic('up') + 'Lên</button><button class="btn sm" type="button" data-wss="down">' + ic('down') + 'Xuống</button><button class="btn sm" type="button" data-wss="ren">Đổi tên</button><button class="btn sm" type="button" data-wss="del">' + ic('trash') + 'Xóa</button></div></div>') +
        H.grp('Khi chuyển workspace', H.rad('wss-a', '0', 'Không lưu thay đổi của workspace đang dùng', !WS.autosave) + H.rad('wss-a', '1', 'Tự lưu thay đổi vào workspace đang dùng', WS.autosave)) + '</div>',
      foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="wss-ok">Lưu thiết lập</button>' });
    var el = dlg.el, isUser = function(n){ return WS.list.some(function(u){ return u.name === n; }); };
    function render(){ $('#wss-l', el).innerHTML = order.map(function(n){ return '<div class="wsor' + (n === sel ? ' is-sel' : '') + '" data-wsn="' + esc(n) + '"><input type="checkbox" data-wsh="' + esc(n) + '"' + (hide[n] ? '' : ' checked') + ' aria-label="Hiện ' + esc(n) + ' trong menu"><span class="grow">' + esc(n) + '</span>' + (isUser(n) ? '' : '<span class="faint" style="font-size:var(--fs-xs)">có sẵn</span>') + '</div>'; }).join(''); $('[data-wss="ren"]', el).disabled = $('[data-wss="del"]', el).disabled = !isUser(sel); }
    el.addEventListener('click', function(e){ var r = e.target.closest('[data-wsn]'); if (r && !e.target.closest('input')){ sel = r.dataset.wsn; render(); return; } var b = e.target.closest('[data-wss]'); if (!b) return; var k = b.dataset.wss, i = order.indexOf(sel);
      if (k === 'up' && i > 0){ order.splice(i, 1); order.splice(i - 1, 0, sel); } else if (k === 'down' && i < order.length - 1){ order.splice(i, 1); order.splice(i + 1, 0, sel); }
      else if (k === 'ren') return SC.ask('Đổi tên workspace', 'Tên mới', sel, function(v){ return order.indexOf(v) >= 0 && v !== sel ? 'Đã có workspace “' + v + '”.' : null; }, 'UI-K08').then(function(v){ if (!v) return; WS.list.forEach(function(u){ if (u.name === sel) u.name = v; }); if (WS.cur === sel) WS.cur = v; order[order.indexOf(sel)] = v; sel = v; render(); });
      else if (k === 'del') return TC.confirm({ title:'Xóa workspace', msg:'Xóa workspace “<b>' + esc(sel) + '</b>”?', ok:'Xóa workspace', danger:true }).then(function(y){ if (!y) return; WS.list = WS.list.filter(function(u){ return u.name !== sel; }); if (WS.cur === sel) WS.cur = null; order = order.filter(function(n){ return n !== sel; }); sel = order[0]; render(); });
      render(); });
    el.addEventListener('change', function(e){ var h = e.target.dataset.wsh; if (h) { if (e.target.checked) delete hide[h]; else hide[h] = 1; } });
    $('#wss-ok', el).addEventListener('click', function(){ WS.order = order; WS.hide = hide; WS.mine = $('#wss-mine', el).value; WS.autosave = $('[name="wss-a"]:checked', el).value === '1'; saveWS(); dlg.close(true); });
    render();
  };
  IMPL.WSSAVE = function(){ return SC.wsSave(); };
  IMPL.WSSETTINGS = function(){ SC.wsSettings(); };
  IMPL.WORKSPACE = function(ctx){
    return ctx.kw('Workspace hiện hành: ' + curName(), [['Đặt hiện hành', 'C'], ['Lưu thành', 'SA'], ['Thiết lập', 'SE'], ['Liệt kê', '?']], 'C').then(function(k){
      if (k === 'SA') return SC.wsSave(); if (k === 'SE') return SC.wsSettings();
      if (k === '?'){ wsNames().forEach(function(n){ ctx.out((n === curName() ? '● ' : '○ ') + esc(n), 'muted'); }); return; }
      var names = wsNames(); return ctx.kw('Chọn workspace', names.map(function(n, i){ return [n, String(i + 1)]; }), String(names.indexOf(curName()) + 1)).then(function(v){ var n = names[Number(v) - 1]; if (n) SC.wsSwitch(n); });
    });
  };
  OPEN['UI-K08'] = function(){ SC.cat.PRO(); var b = $('#sb-ws'); if (b){ SC.cat.flash('#sb-ws'); TC.wsMenu(b); } };

  /* ================= Bản macOS (07.1 §13.6) ================= */
  TC.S.platform = TC.load('platform', 'win');
  var SHEETS = { open:1, saveas:1, plot:1, pdf:1, img:1, b03:1, b03m:1 };
  // hộp thoại tệp, in: dạng sheet gắn vào cửa sổ; Tùy chọn đổi tên thành Cài đặt
  var dlg0 = TC.dialog;
  TC.dialog = function(o){ if (TC.isMac() && o.id === 'opts') o = Object.assign({}, o, { title:String(o.title).replace(/^Tùy chọn/, 'Cài đặt') }); var api = dlg0(o); if (TC.isMac() && SHEETS[o.id]){ var tb = $('#tb').getBoundingClientRect(); api.el.classList.add('sheet'); api.el.style.top = (tb.bottom - 1) + 'px'; macPaths(api.el); new MutationObserver(function(){ macPaths(api.el); }).observe(api.el, { childList:true, subtree:true }); } return api; };
  // đường dẫn kiểu macOS trong hộp thoại tệp (D:\DuAn\… → ~/DuAn/…)
  function macPaths(root){ var w = document.createTreeWalker(root, NodeFilter.SHOW_TEXT), n; while ((n = w.nextNode())) if (/[A-Z]:\\|\\\\SRV/.test(n.nodeValue)) n.nodeValue = n.nodeValue.replace(/\\\\SRV-01\\/g, '/Volumes/SRV-01/').replace(/[A-Z]:\\/g, '~/').replace(/\\/g, '/'); }
  function macMenus(){
    var M = TC.CLASSIC_MENUS, get = function(n){ return (M.filter(function(m){ return m[0] === n; })[0] || [n, []])[1]; }, recent = (TC.RECENT || []).slice(0, 6);
    var conv = function(list){ return list.filter(function(x){ return x === '|' || !/^(Thoát|Tùy chọn…)$/.test(x[0]); }).map(function(x){ if (x === '|') return { sep:1 }; var c = x[1] && TC.CMDS[x[1]]; return { label:esc(x[0]), icon:c && c.icon, kbd:x[2] || '', dis:x[3] === 5 || !x[1], need:c && c.need !== 'V' ? c.need : null, act:function(){ if (x[1] && TC.CMDS[x[1]]) TC.cmd.run(x[1]); } }; }); };
    return [
      ['TrionCAD', [{ label:'Giới thiệu TrionCAD', act:function(){ SC.about(); } }, { label:'Kiểm tra cập nhật…', act:function(){ SC.update(); } }, { sep:1 }, { label:'Cài đặt…', icon:'gear', kbd:'⌘,', act:function(){ SC.options(); } }, { label:'Giấy phép và tài khoản…', icon:'key', act:function(){ SC.license(); } }, { sep:1 }, { label:'Dịch vụ', sub:[{ label:'Không có dịch vụ áp dụng', dis:true }] }, { sep:1 }, { label:'Ẩn TrionCAD', kbd:'⌘H' }, { label:'Ẩn ứng dụng khác', kbd:'⌥⌘H' }, { label:'Hiện tất cả', dis:true }, { sep:1 }, { label:'Thoát TrionCAD', kbd:'⌘Q', act:function(){ TC.cmd.run('QUIT'); } }]],
      ['Tệp', conv(get('Tệp')).slice(0, 2).concat([{ label:'Mở gần đây', sub:recent.map(function(r){ return { label:esc(r.name), icon:'file', act:function(){ TC.files.openRecent(r); } }; }).concat([{ sep:1 }, { label:'Xóa danh sách', dis:true }]) }]).concat(conv(get('Tệp')).slice(2))],
      ['Sửa', conv(get('Sửa')).concat([{ sep:1 }, { label:'Bắt đầu đọc chính tả…', dis:true }, { label:'Biểu tượng và ký tự đặc biệt', kbd:'⌃⌘Space', dis:true }])],
      ['Hiển thị', conv(get('Hiển thị')).concat([{ sep:1 }, { label:'Ẩn thanh tab tệp', check:$('#ft').hidden, act:function(){ $('#ft').hidden = !$('#ft').hidden; V().resize(); } }, { label:'Vào toàn màn hình', kbd:'⌃⌘F', act:fullScreen }])],
      ['Chèn', conv(get('Chèn'))], ['Định dạng', conv(get('Định dạng'))], ['Công cụ', conv(get('Công cụ'))], ['Vẽ', conv(get('Vẽ'))], ['Kích thước', conv(get('Kích thước'))], ['Sửa đổi', conv(get('Sửa đổi'))], ['Công cụ VN', conv(get('Công cụ VN'))], ['Mở rộng', conv(get('Mở rộng'))],
      ['Cửa sổ', [{ label:'Thu nhỏ', kbd:'⌘M' }, { label:'Phóng to' }, { label:'Xếp cửa sổ', sub:[{ label:'Sang trái màn hình' }, { label:'Sang phải màn hình' }, { label:'Đầy màn hình' }] }, { sep:1 }, { label:'Hiện tab trước', kbd:'⌃⇧Tab', act:function(){ cycleTab(-1); } }, { label:'Hiện tab kế', kbd:'⌃Tab', act:function(){ cycleTab(1); } }, { label:'Chuyển tab sang cửa sổ mới', dis:true }, { label:'Gộp tất cả cửa sổ', dis:true }, { sep:1 }, { label:'Đưa tất cả ra trước' }, { sep:1 }].concat(A().docs.filter(function(d){ return d.open; }).map(function(d){ return { label:esc(d.name) + (d.dirty ? ' •' : ''), check:A().active === d.id, act:function(){ A().activate(d.id); } }; }))],
      ['Trợ giúp', [{ label:'Tìm lệnh…', icon:'search', act:function(){ $('#tb-q').focus(); } }, { sep:1 }, { label:'Trợ giúp TrionCAD', icon:'book', kbd:'⌘?', act:function(){ TC.openHelp(); } }, { label:'Bảng phím tắt', icon:'keyboard', act:function(){ cheatSheet(keysOf); } }, { label:'Chuyển từ AutoCAD sang', act:function(){ TC.openHelp('migrate'); } }, { sep:1 }, { label:'Liên hệ hỗ trợ', icon:'chat', act:function(){ SC.support(); } }]]
    ];
  }
  function cycleTab(dir){ var o = ['start'].concat(A().docs.filter(function(d){ return d.open; }).map(function(d){ return d.id; })), i = o.indexOf(A().active); A().activate(o[(i + dir + o.length) % o.length]); }
  function fullScreen(){ var on = $('#app').classList.toggle('mac-fs'); TC.toast(on ? 'Toàn màn hình (⌃⌘F). Đưa chuột lên mép trên để hiện thanh menu; nút xanh lá hoặc ⌃⌘F để thoát.' : 'Đã thoát toàn màn hình.', { type:'info', log:false }); setTimeout(function(){ V().resize(); }, 0); }
  function macBar(){
    var bar = $('#macbar'); if (!TC.isMac()){ if (bar) bar.remove(); return; }
    var menus = macMenus(), now = new Date(), dd = ['CN', 'Th 2', 'Th 3', 'Th 4', 'Th 5', 'Th 6', 'Th 7'][now.getDay()] + ' ' + now.getDate() + ' thg ' + (now.getMonth() + 1) + '  ' + now.getHours() + ':' + ('0' + now.getMinutes()).slice(-2);
    var html = '<span class="mbg" aria-hidden="true"></span>' + menus.map(function(m, i){ return '<button type="button" data-mm="' + i + '"' + (i === 0 ? ' class="app"' : '') + '>' + esc(m[0]) + '</button>'; }).join('') + '<span class="grow"></span><button type="button" data-mm="ime" class="ime" data-tip="Nguồn nhập: Tiếng Việt · Telex" data-desc="Khi dòng lệnh đang chờ tên lệnh, TrionCAD tạm bỏ qua bộ gõ để “line” không thành “lĩne”.">VI</button><span class="clock">' + dd + '</span>';
    if (!bar){ bar = TC.el('<nav class="macbar" id="macbar" aria-label="Thanh menu macOS"></nav>'); $('#app').insertBefore(bar, $('#app').firstChild); bar.addEventListener('click', function(e){ var b = e.target.closest('[data-mm]'); if (!b) return; var r = b.getBoundingClientRect(); if (b.dataset.mm === 'ime'){ TC.menu([{ h:'Nguồn nhập' }, { label:'Tiếng Việt – Telex', check:true }, { label:'Tiếng Việt – VNI' }, { label:'ABC' }, { sep:1 }, { label:'EVKey, OpenKey cũng dùng được', dis:true }], r.right, r.bottom, { alignRight:true }); return; } var m = macMenus()[Number(b.dataset.mm)]; TC.menu(m[1], r.left, r.bottom, { returnFocus:b, minWidth:240 }); }); }
    bar.innerHTML = html;
  }
  TC.on('title', function(){
    var tb = $('#tb'); if (!tb || !TC.isMac()) return;
    if (!$('.mac-lights', tb)) tb.insertAdjacentHTML('afterbegin', '<div class="mac-lights"><button type="button" class="ml r" aria-label="Đóng cửa sổ" data-run="QUIT"></button><button type="button" class="ml y" aria-label="Thu nhỏ cửa sổ"></button><button type="button" class="ml g" aria-label="Vào toàn màn hình" id="ml-g"></button></div>');
    var g = $('#ml-g', tb), t = 0; g.onclick = fullScreen; g.onmouseenter = function(){ t = setTimeout(function(){ var r = g.getBoundingClientRect(); TC.menu([{ label:'Vào toàn màn hình', kbd:'⌃⌘F', act:fullScreen }, { sep:1 }, { label:'Xếp cửa sổ sang trái màn hình' }, { label:'Xếp cửa sổ sang phải màn hình' }, { label:'Đưa sang màn hình khác', dis:true }], r.left, r.bottom + 4, { minWidth:250 }); }, 700); }; g.onmouseleave = function(){ clearTimeout(t); };
    macBar();
  });
  TC.setPlatform = function(p){ TC.S.platform = p; TC.save('platform', p); $('#app').classList.toggle('mac', p === 'mac'); $('#app').classList.remove('mac-fs'); A().renderTitle(); macBar(); A().renderRibbon(); A().renderStatus(); setTimeout(function(){ V().resize(); A().fitRibbon(); }, 0); };
  // touchpad trên macOS: kéo hai ngón để pan, chụm để zoom (07.1 §13.6)
  document.addEventListener('wheel', function(e){ if (!TC.isMac() || e.ctrlKey || !e.target.closest('#cv')) return; var tp = Math.abs(e.deltaX) > .5 || (e.deltaY % 1 !== 0) || Math.abs(e.deltaY) < 30; if (!tp) return; e.preventDefault(); e.stopImmediatePropagation(); V().panBy(-e.deltaX, -e.deltaY); }, { capture:true, passive:false });
  var pm0 = TC.protoMenu;
  TC.protoMenu = function(){ var m = pm0(), i = m.map(function(x){ return x && x.h; }).indexOf('Giao diện'); m.splice(i, 0, { label:'Doanh nghiệp', check:TC.S.edition === 'ent', act:function(){ TC.S.edition = 'ent'; TC.save('edition', 'ent'); TC.emit('edition'); TC.toast('Đang xem với phiên bản <b>TrionCAD Doanh nghiệp</b>.', { type:'info', log:false }); } }); var j = m.map(function(x){ return x && x.h; }).indexOf('Giao diện') + 1; m.splice(j, 0, { label:'Nền tảng', sub:[{ label:'Windows', check:!TC.isMac(), act:function(){ TC.setPlatform('win'); } }, { label:'macOS', check:TC.isMac(), act:function(){ TC.setPlatform('mac'); } }] }); return m; };
  OPEN['UI-MAC'] = function(){ SC.cat.D1(); TC.setPlatform('mac'); TC.toast('Bản macOS: thanh menu hệ thống thay nút logo, ba nút cửa sổ bên trái, ⌘ cho lệnh hệ thống, hộp thoại tệp dạng sheet, kéo hai ngón để pan. Đổi lại Windows ở nút Prototype › Nền tảng.', { type:'info', log:false, ms:10000 }); };
  setTimeout(function(){ if (TC.isMac()) TC.setPlatform('mac'); }, 0);

  /* ================= Nhiều cửa sổ bản vẽ: chia ô (07.1 §9.15, P1) ================= */
  var TILE = { mode:null, view:null, doc:null };
  SC.tile = function(mode){
    var ws = $('#ws'), p = $('#cv2'); ws.classList.remove('tiled-v', 'tiled-h');
    if (!mode || A().active === 'start'){ if (p) p.remove(); TILE = { mode:null, view:null, doc:null }; setTimeout(function(){ V().resize(); }, 0); return; }
    var others = A().docs.filter(function(d){ return d.open && d.id !== A().active; });
    if (!others.length){ TC.toast('Mở thêm một bản vẽ để chia ô.', { type:'info', log:false, act:{ label:'Mở…', fn:function(){ TC.cmd.run('OPEN'); } } }); return; }
    if (!p){ p = TC.el('<div class="cv2" id="cv2" tabindex="0" role="region" aria-label="Bản vẽ thứ hai"><div class="pane-h"><span class="nm"></span><span class="faint">bấm để làm việc với bản vẽ này</span></div></div>'); ws.insertBefore(p, $('#dock-r')); TILE.view = new TC.View(p);
      p.addEventListener('mousedown', function(e){ if (e.button !== 0) return; var d2 = TILE.doc; TILE.doc = A().doc(); A().activate(d2.id); });
      p.addEventListener('wheel', function(e){ e.preventDefault(); var r = p.getBoundingClientRect(); TILE.view.zoomAt(e.clientX - r.left, e.clientY - r.top, e.deltaY < 0 ? 1.2 : 1 / 1.2); }, { passive:false }); }
    TILE.mode = mode; TILE.doc = TILE.doc && TILE.doc.open && TILE.doc.id !== A().active ? TILE.doc : others[0];
    ws.classList.add(mode === 'h' ? 'tiled-h' : 'tiled-v');
    setTimeout(function(){ V().resize(); paintTile(); }, 0);
  };
  function paintTile(){ if (!TILE.mode || !TILE.view) return; var p = $('#cv2'); $('.nm', p).textContent = TILE.doc.name; TILE.view.setDoc(TILE.doc); TILE.view.draw(); }
  TC.on('doc', function(d){ if (!TILE.mode) return; if (!d || A().active === 'start'){ SC.tile(null); return; } if (!TILE.doc || TILE.doc === d || !TILE.doc.open){ var o = A().docs.filter(function(x){ return x.open && x !== d; })[0]; if (!o){ SC.tile(null); return; } TILE.doc = o; } setTimeout(paintTile, 0); });
  TC.on('theme', function(){ if (TILE.view) TILE.view.draw(); });
  TC.tileMenu = function(){ return [{ label:'Một cửa sổ (tab)', check:!TILE.mode, act:function(){ SC.tile(null); } }, { label:'Chia ô dọc', check:TILE.mode === 'v', act:function(){ SC.tile('v'); } }, { label:'Chia ô ngang', check:TILE.mode === 'h', act:function(){ SC.tile('h'); } }]; };

  OPEN['UI-MR'] = function(){ SC.cat.plan(); setTimeout(function(){ var t = $$('.rb-tab').filter(function(b){ return b.textContent.trim() === 'Mở rộng'; })[0]; if (t) t.click(); SC.cat.flash('#rb'); TC.toast('Tab Mở rộng: công cụ kiểu Express Tools, giữ tên lệnh gốc (BURST, NCOPY, GATTE, TXTEXP, FLATTEN, EXTRIM, MPEDIT, SAVEALL…). Phần P2 (ARCTEXT, TEXTMASK, BREAKLINE, SUPERHATCH…) có từ bản sau v1.0, xem mục UI-MR2.', { type:'info', log:false, ms:9000 }); }, 80); };

})();
