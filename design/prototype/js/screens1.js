/* TrionCAD prototype · màn hình nhóm 1: Start, menu ứng dụng, lần chạy đầu, tệp (mở, phục hồi, thiếu tham chiếu, tệp lớn), Layer, Tìm chữ, cửa sổ văn bản. */
(function(){
  'use strict';
  var TC = window.TC, $ = TC.$, $$ = TC.$$, esc = TC.esc, ic = TC.ic;
  var SC = TC.screens = TC.screens || {};
  function A(){ return TC.app; }

  /* ================= tab Start UI-A02 ================= */
  var st = { view:TC.load('startView', 'grid'), q:'', sort:'date', empty:false };
  SC.start = function(){
    var host = $('#start'), viewer = TC.isViewer(), ed = TC.EDITIONS[TC.S.edition];
    var list = TC.RECENT.filter(function(r){ return !st.q || TC.fold(r.name + ' ' + r.path).indexOf(TC.fold(st.q)) >= 0; });
    list.sort(function(a, b){ if (a.pinned !== b.pinned) return a.pinned ? -1 : 1; if (st.sort === 'name') return a.name.localeCompare(b.name); if (st.sort === 'size') return parseFloat(b.size.replace(',', '.')) - parseFloat(a.size.replace(',', '.')); return b.date.split('/').reverse().join('').localeCompare(a.date.split('/').reverse().join('')); });
    if (st.empty) list = [];
    var tpls = ['A3 ngang','A3 dọc','A1 ngang','A2 ngang','A4 dọc','Trống (mm)','Trống (inch)','Mẫu của tôi'];
    host.innerHTML = '<div class="start-in">' +
      '<div class="side">' +
        (viewer ? '' : '<section><h2>Tạo mới</h2><div class="tpl-grid">' + tpls.map(function(t){ return '<button class="tpl" type="button" data-tpl="' + t + '"><i></i>' + (t.indexOf('Trống') < 0 && t.indexOf('Mẫu') < 0 ? 'TCVN ' : '') + t + '</button>'; }).join('') + '</div><button class="btn sm ghost" type="button" style="margin-top:6px" data-tpl="browse">Duyệt mẫu…</button></section>') +
        '<section><h2>Mở</h2><div class="stack" style="gap:6px"><button class="big" type="button" data-run="OPEN">' + ic('open', 'md') + '<span>Mở tệp…<small>DWG, DXF, DWT · Ctrl+O</small></span></button>' +
        '<button class="big" type="button" id="st-folders">' + ic('folder', 'md') + '<span>Mở thư mục gần đây<small>D:\\DuAn\\NhaO-2T, \\\\SRV-01\\DuAn…</small></span></button>' +
        '<button class="big" type="button" data-run="RECOVER">' + ic('recover', 'md') + '<span>Phục hồi bản vẽ lỗi<small>Mở tệp có lỗi cấu trúc để xem</small></span></button></div></section>' +
        '<section class="card"><div class="hrow" style="justify-content:space-between"><b>' + esc(ed.name) + '</b>' + A().edBadge() + '</div>' +
          (TC.S.edition === 'trial' ? '<p>Còn ' + TC.S.trialDays + ' ngày dùng thử. Hết hạn, phần mềm chuyển về Viewer; bản vẽ vẫn mở và in được.</p><div class="prog"><i style="width:' + (TC.S.trialDays / 30 * 100) + '%"></i></div><button class="btn primary sm" type="button" data-buy>Mua TrionCAD Pro</button>' :
           viewer ? '<p>Xem, đo, in và xuất PDF miễn phí. Sửa bản vẽ cần TrionCAD Pro.</p><button class="btn primary sm" type="button" data-trial>Dùng thử Pro 30 ngày</button>' : '<p>Thuê bao còn hạn đến 18/10/2027.</p><button class="btn sm" type="button" data-lic>Trạng thái giấy phép</button>') + '</section>' +
      '</div>' +
      '<section style="min-width:0"><div class="rf-tools"><h2 style="margin:0">Tệp gần đây</h2><span class="grow"></span>' +
        '<label class="search" style="width:240px">' + ic('search') + '<input id="st-q" placeholder="Tìm trong tệp gần đây" value="' + esc(st.q) + '" aria-label="Tìm trong tệp gần đây"></label>' +
        '<button class="dd" type="button" id="st-sort" aria-haspopup="menu"><span class="lbl">' + ic('sort') + { date:'Ngày sửa', name:'Tên', size:'Kích thước' }[st.sort] + '</span>' + ic('down', 'xs') + '</button>' +
        '<span class="seg" role="group" aria-label="Kiểu xem"><button type="button" data-sv="grid" aria-pressed="' + (st.view === 'grid') + '" aria-label="Lưới">' + ic('gridview') + '</button><button type="button" data-sv="list" aria-pressed="' + (st.view === 'list') + '" aria-label="Danh sách">' + ic('listview') + '</button></span></div>' +
        (list.length ? (st.view === 'grid' ? '<div class="rf-grid">' + list.map(function(r, i){ return '<button class="rf" type="button" data-rf="' + esc(r.name) + '"><canvas width="380" height="238"></canvas><span class="n">' + esc(r.name) + '</span><span class="m"><span>' + esc(r.date) + '</span><span>' + esc(r.size) + '</span><span>' + esc(r.ver) + '</span></span><span class="m" style="margin-top:-4px">' + esc(r.path) + '</span><span class="pin' + (r.pinned ? ' on' : '') + '" role="button" aria-label="' + (r.pinned ? 'Bỏ ghim' : 'Ghim') + '" data-pinf="' + esc(r.name) + '">' + ic('pin') + '</span></button>'; }).join('') + '</div>'
          : '<div class="tblwrap"><table class="tbl rf-list"><colgroup><col><col style="width:110px"><col style="width:90px"><col style="width:90px"><col style="width:36px"></colgroup><thead><tr><th>Tên</th><th>Ngày sửa</th><th>Kích thước</th><th>Phiên bản</th><th></th></tr></thead><tbody>' + list.map(function(r){ return '<tr data-rf="' + esc(r.name) + '"><td>' + ic('file', 's12') + ' <b style="font-weight:500">' + esc(r.name) + '</b> <span class="faint">' + esc(r.path) + '</span></td><td>' + r.date + '</td><td>' + r.size + '</td><td>' + r.ver + '</td><td><span class="pin' + (r.pinned ? ' on' : '') + '" role="button" data-pinf="' + esc(r.name) + '" aria-label="Ghim">' + ic(r.pinned ? 'star' : 'pin', 's12') + '</span></td></tr>'; }).join('') + '</tbody></table></div>')
          : '<div class="empty" style="padding:48px 16px">' + ic('file') + '<b>' + (st.q ? 'Không có tệp khớp “' + esc(st.q) + '”' : 'Chưa có tệp gần đây') + '</b>' + (st.q ? '' : 'Mở một bản vẽ, hoặc kéo-thả tệp DWG vào đây.') + '<div class="hrow" style="justify-content:center;margin-top:6px"><button class="btn primary" type="button" data-sample>Mở bản vẽ mẫu</button><button class="btn" type="button" data-video>' + ic('play') + 'Xem video 2 phút</button></div></div>') +
      '</section>' +
      '<div class="side">' +
        '<section><h2>Học tập</h2><div class="learn">' + [['Chuyển từ AutoCAD sang trong 1 ngày','12 phút · các lệnh và phím tắt giữ nguyên'],['Đo diện tích phòng bằng một cú bấm','2 phút · Đo nhanh'],['Bác sĩ Font sửa chữ TCVN3, VNI thế nào','3 phút']].map(function(x){ return '<a href="#" data-video><span class="th">' + ic('play', 'md') + '</span><span>' + esc(x[0]) + '<span class="faint" style="display:block;font-size:var(--fs-xs)">' + esc(x[1]) + '</span></span></a>'; }).join('') + '</div></section>' +
        '<section><h2>Mẹo</h2><div class="card"><p>Gõ tiếng Việt vào dòng lệnh để tìm lệnh: “đo”, “in”, “layer”. Tên lệnh AutoCAD vẫn dùng được như cũ.</p></div></section>' +
        '<section><h2>Thông báo</h2><div class="news"><a href="#" data-upd>' + ic('update') + '<span>TrionCAD Viewer 1.0.2 đã có<span>Sửa lỗi hiển thị hatch khi zoom sâu · 14/10/2026</span></span></a><a href="#" data-video>' + ic('forum') + '<span>Bộ khung tên TCVN 5571 cập nhật<span>Cộng đồng · 10/10/2026</span></span></a></div></section>' +
      '</div></div>';
    $$('.rf canvas', host).forEach(function(c){ var r = TC.RECENT.filter(function(x){ return x.name === c.parentNode.dataset.rf; })[0]; drawRecentThumb(c, r); });
    var q = $('#st-q', host); q.addEventListener('input', function(){ st.q = q.value; SC.start(); var q2 = $('#st-q'); q2.focus(); q2.setSelectionRange(q2.value.length, q2.value.length); });
    $('#st-sort', host).addEventListener('click', function(){ var r = this.getBoundingClientRect(); TC.menu([['date','Ngày sửa'],['name','Tên'],['size','Kích thước']].map(function(s){ return { label:s[1], check:st.sort === s[0], act:function(){ st.sort = s[0]; SC.start(); } }; }), r.left, r.bottom + 2); });
    $('#st-folders', host).addEventListener('click', function(){ var r = this.getBoundingClientRect(); TC.menu(['D:\\DuAn\\NhaO-2T\\KienTruc','D:\\DuAn\\NhaO-2T\\KetCau','\\\\SRV-01\\DuAn\\2026\\THCS-AnPhu','D:\\Khao-sat\\Lo-B7'].map(function(p){ return { label:esc(p), icon:'folder', act:function(){ TC.files.open(p); } }; }), r.left, r.bottom + 2); });
    host.onclick = function(e){
      var sv = e.target.closest('[data-sv]'); if (sv){ st.view = sv.dataset.sv; TC.save('startView', st.view); SC.start(); return; }
      var pin = e.target.closest('[data-pinf]'); if (pin){ e.stopPropagation(); var r = TC.RECENT.filter(function(x){ return x.name === pin.dataset.pinf; })[0]; r.pinned = !r.pinned; SC.start(); return; }
      var rf = e.target.closest('[data-rf]'); if (rf){ TC.files.openRecent(TC.RECENT.filter(function(x){ return x.name === rf.dataset.rf; })[0]); return; }
      if (e.target.closest('[data-trial]')){ SC.upgrade('Sửa bản vẽ', 'PRO'); return; }
      if (e.target.closest('[data-buy]')){ TC.toast('Mở trang mua TrionCAD Pro trên trình duyệt.', { type:'info', log:false }); return; }
      if (e.target.closest('[data-lic]')){ SC.license(); return; }
      if (e.target.closest('[data-sample]')){ st.empty = false; A().openDoc('d1'); return; }
      if (e.target.closest('[data-video]')){ e.preventDefault(); TC.toast('Mở video hướng dẫn trong trình duyệt.', { type:'info', log:false }); return; }
      if (e.target.closest('[data-upd]')){ e.preventDefault(); SC.update(true); return; }
      var tp = e.target.closest('[data-tpl]'); if (tp) TC.toast('Tạo bản vẽ từ mẫu: chọn mẫu TCVN (UI-J11) thiết kế ở Đợt 3.', { type:'info', log:false });
    };
    host.oncontextmenu = function(e){
      var rf = e.target.closest('[data-rf]'); if (!rf) return; e.preventDefault(); var r = TC.RECENT.filter(function(x){ return x.name === rf.dataset.rf; })[0];
      TC.menu([{ label:'Mở', icon:'open', act:function(){ TC.files.openRecent(r); } }, { label:'Mở chỉ đọc', icon:'lock', act:function(){ TC.files.openRecent(r, true); } }, { label:'Mở thư mục chứa', icon:'folder', act:function(){ TC.toast('Mở ' + esc(r.path) + ' trong File Explorer.', { type:'info', log:false }); } }, { label:'Sao chép đường dẫn', icon:'copyclip', act:function(){ TC.copyText(r.path + '\\' + r.name); } }, { sep:1 }, { label:r.pinned ? 'Bỏ ghim' : 'Ghim', icon:'pin', act:function(){ r.pinned = !r.pinned; SC.start(); } }, { label:'Xóa khỏi danh sách', icon:'x', act:function(){ TC.RECENT.splice(TC.RECENT.indexOf(r), 1); SC.start(); TC.toast('Đã xóa “' + esc(r.name) + '” khỏi danh sách. Tệp trên ổ đĩa không bị xóa.', { log:false }); } }], e.clientX, e.clientY);
    };
  };
  SC.startEmpty = function(on){ st.empty = on; A().activate('start'); };
  // thumbnail: bản vẽ thật nếu có, còn lại vẽ phác theo tên để mỗi tệp một hình
  function drawRecentThumb(c, r){
    var d = r && r.doc && A().docs.filter(function(x){ return x.id === r.doc; })[0];
    if (d) { TC.thumb(c, d, 'model'); return; }
    // tệp mẫu chưa có dữ liệu riêng: dùng một góc nhìn khác của bản vẽ mẫu để thumbnail trông thật
    var seed = 0; for (var i = 0; i < r.name.length; i++) seed = (seed * 31 + r.name.charCodeAt(i)) >>> 0;
    var d1 = A().docs.filter(function(x){ return x.id === 'd1'; })[0], d2 = A().docs.filter(function(x){ return x.id === 'd2'; })[0];
    var presets = [[d1, { c:[54000, 2300], h:16000 }], [d1, { c:[102000, 5200], h:17000 }], [d2, null], [d1, 0], [d1, 1], [d1, { c:[6000, 3850], h:31000 }]];
    var pick = presets[seed % presets.length], save = pick[0].initView;
    if (typeof pick[1] === 'number') { TC.thumb(c, pick[0], pick[1]); return; }
    if (pick[1]) pick[0].initView = pick[1];
    TC.thumb(c, pick[0], 'model'); pick[0].initView = save;
    if (r.broken){ var ctx = c.getContext('2d'); ctx.fillStyle = 'rgba(240,106,97,.16)'; ctx.fillRect(0, 0, c.width, c.height); }
  }

  /* ================= menu ứng dụng UI-A03 ================= */
  SC.appMenu = function(btn){
    if ($('.amenu')) { $('.amenu').remove(); btn.setAttribute('aria-expanded', 'false'); return; }
    var viewer = TC.isViewer(), r = btn.getBoundingClientRect();
    var items = [
      ['new', 'Mới', 'NEW', 'PRO', 'Ctrl+N'], ['open', 'Mở', 'OPEN', 'V', 'Ctrl+O'], ['save', 'Lưu', 'QSAVE', 'PRO', 'Ctrl+S'], ['saveas', 'Lưu thành', 'SAVEAS', 'PRO', 'Ctrl+Shift+S'],
      ['pdf', 'Xuất', 'EXPORTPDF', 'V'], ['print', 'In', 'PLOT', 'V', 'Ctrl+P'], ['wrench', 'Tiện ích bản vẽ', 'UTIL', 'V'], ['fileclose', 'Đóng', 'CLOSE', 'V']
    ];
    var el = TC.el('<div class="amenu" role="menu" aria-label="Menu ứng dụng"><div class="ah"><label class="search">' + ic('search') + '<input id="am-q" placeholder="Tìm lệnh hoặc tệp gần đây" aria-label="Tìm trong menu ứng dụng"></label></div>' +
      '<div class="al">' + items.filter(function(i){ return !viewer || i[3] !== 'PRO'; }).map(function(i){ return '<button class="ai" type="button" role="menuitem" data-am="' + i[2] + '">' + ic(i[0], 'md') + '<span class="grow">' + i[1] + '</span>' + (i[2] === 'EXPORTPDF' || i[2] === 'PLOT' || i[2] === 'UTIL' || i[2] === 'CLOSE' ? ic('right', 'xs') : '') + '</button>'; }).join('') + '</div>' +
      '<div class="ar" id="am-r"></div><div class="af"><button class="btn sm" type="button" data-am="OPTIONS">' + ic('gear') + 'Tùy chọn</button><button class="btn sm" type="button" data-am="QUIT">' + ic('exit') + 'Thoát TrionCAD</button></div></div>');
    document.body.appendChild(el); el.style.left = r.left + 'px'; el.style.top = (r.bottom + 2) + 'px'; btn.setAttribute('aria-expanded', 'true');
    var right = $('#am-r', el);
    function recent(q){ var l = TC.RECENT.filter(function(x){ return !q || TC.fold(x.name).indexOf(TC.fold(q)) >= 0; }); var cm = q ? TC.matchCmds(q).slice(0, 4) : [];
      right.innerHTML = (cm.length ? '<div class="rh">Lệnh</div>' + cm.map(function(m){ return '<button class="rfi" type="button" data-amrun="' + m.c.name + '"><span class="n">' + ic(m.c.icon, 's12') + ' ' + esc(m.c.desc) + '</span><span class="faint mono" style="font-size:var(--fs-xs)">' + m.c.name + '</span></button>'; }).join('') : '') +
        '<div class="rh"><span>Tệp gần đây</span><span>' + ic('sort', 's12') + ' Ngày</span></div>' + l.map(function(x){ return '<button class="rfi" type="button" data-amrf="' + esc(x.name) + '"><span class="n">' + (x.pinned ? ic('pin', 's12') + ' ' : '') + esc(x.name) + '</span><span class="faint" style="font-size:var(--fs-xs)">' + x.date + '</span><span class="p">' + esc(x.path) + '</span></button>'; }).join(''); }
    recent('');
    $('#am-q', el).addEventListener('input', function(){ recent(this.value); });
    setTimeout(function(){ $('#am-q', el).focus(); }, 0);
    function close(){ el.remove(); btn.setAttribute('aria-expanded', 'false'); document.removeEventListener('mousedown', out, true); document.removeEventListener('keydown', esc1, true); }
    function out(e){ if (!el.contains(e.target) && !btn.contains(e.target) && !e.target.closest('.menu')) close(); }
    function esc1(e){ if (e.key === 'Escape' && !$('.menu')) { close(); btn.focus(); } }
    document.addEventListener('mousedown', out, true); document.addEventListener('keydown', esc1, true);
    el.addEventListener('click', function(e){
      var b = e.target.closest('[data-am]'); var rr = e.target.closest('[data-amrf]'); var ru = e.target.closest('[data-amrun]');
      if (rr){ close(); TC.files.openRecent(TC.RECENT.filter(function(x){ return x.name === rr.dataset.amrf; })[0]); return; }
      if (ru){ close(); TC.cmd.run(ru.dataset.amrun); return; }
      if (!b) return; var k = b.dataset.am, br = b.getBoundingClientRect();
      if (k === 'EXPORTPDF') { TC.menu([{ label:'PDF…', icon:'pdf', act:function(){ close(); SC.exportPdf(); } }, { label:'Ảnh (PNG, JPG, TIF, BMP)…', icon:'image', act:function(){ close(); SC.exportImage(); } }, { label:'DXF…', need:'PRO' }], br.right, br.top, { level:1 }); return; }
      if (k === 'PLOT') { TC.menu([{ label:'In…', icon:'print', kbd:'Ctrl+P', act:function(){ close(); SC.plot(); } }, { label:'In hàng loạt theo khung tên…', icon:'batchplot', need:'VP' }, { label:'In nhiều layout…', icon:'publish', need:'VP' }, { label:'Thiết lập trang…', act:function(){ close(); SC.pageSetup(); } }, { label:'Xem trước bản in', icon:'preview', act:function(){ close(); SC.preview(); } }], br.right, br.top, { level:1 }); return; }
      if (k === 'UTIL') { TC.menu([{ label:'Kiểm tra lỗi', need:'PRO' }, { label:'Phục hồi…', icon:'recover', act:function(){ close(); TC.files.recover(); } }, { label:'Dọn dẹp', need:'PRO' }, { label:'Thuộc tính bản vẽ', icon:'info', act:function(){ close(); SC.dwgProps(); } }, { label:'Đơn vị', need:'PRO' }], br.right, br.top, { level:1 }); return; }
      if (k === 'CLOSE') { TC.menu([{ label:'Bản vẽ này', kbd:'Ctrl+F4', act:function(){ close(); TC.cmd.run('CLOSE'); } }, { label:'Tất cả bản vẽ', act:function(){ close(); A().docs.forEach(function(d){ if (d.open) A().closeDoc(d.id, true); }); } }], br.right, br.top, { level:1 }); return; }
      close(); TC.cmd.run(k);
    });
  };

  /* ================= lần chạy đầu UI-A15 ================= */
  SC.onboarding = function(){
    var viewer = TC.isViewer();
    var steps = [['lang','Ngôn ngữ'],['theme','Chủ đề']].concat(viewer ? [] : [['ui','Kiểu giao diện'],['migrate','Chuyển từ AutoCAD'],['units','Đơn vị và mẫu']]).concat([['privacy','Quyền riêng tư'],['edition','Phiên bản'],['tour','Hướng dẫn nhanh']]);
    var i = 0, ans = { lang:'vi', theme:TC.S.theme === 'light' ? 'light' : 'dark', ui:'ribbon', tele:false, ed:TC.S.edition === 'viewer' ? 'viewer' : 'trial', units:'mm' };
    var d = TC.dialog({ id:'onb', title:'Chào mừng đến với TrionCAD', w:640, help:false, body:'<div id="onb-b"></div>', foot:'<span class="faint" id="onb-n" style="font-size:var(--fs-xs)"></span><span class="grow"></span><button class="btn ghost" type="button" id="onb-skip">Bỏ qua, xem lại trong Tùy chọn</button><button class="btn" type="button" id="onb-back">Quay lại</button><button class="btn primary" type="button" id="onb-next">Tiếp</button>' });
    function card(name, val, title, sub, pic){ return '<label class="grp" style="cursor:pointer;gap:6px;' + (ans[name] === val ? 'border-color:var(--c-accent);box-shadow:0 0 0 1px var(--c-accent)' : '') + '"><span class="hrow"><input type="radio" name="' + name + '" value="' + val + '"' + (ans[name] === val ? ' checked' : '') + ' style="accent-color:var(--c-accent)"><b>' + title + '</b></span>' + (pic || '') + '<span class="muted" style="font-size:var(--fs-s)">' + sub + '</span></label>'; }
    function render(){
      var s = steps[i][0], b = '';
      $('#onb-n', d.el).textContent = 'Bước ' + (i + 1) + ' / ' + steps.length + ' · ' + steps[i][1];
      var dots = '<div class="hrow" style="gap:4px;margin-bottom:14px">' + steps.map(function(x, k){ return '<i style="height:4px;flex:1;border-radius:2px;background:' + (k <= i ? 'var(--c-accent)' : 'var(--c-active)') + '"></i>'; }).join('') + '</div>';
      if (s === 'lang') b = '<h3 style="margin:0 0 4px">Chọn ngôn ngữ</h3><p class="muted" style="margin-bottom:12px">Đổi được bất cứ lúc nào trong Tùy chọn › Hệ thống, không cần cài lại.</p><div class="grid2">' + card('lang','vi','Tiếng Việt','Nhãn tiếng Việt; tên lệnh tiếng Anh luôn hiện kèm.') + card('lang','en','English','Interface and prompts in English.') + '</div>';
      if (s === 'theme') b = '<h3 style="margin:0 0 4px">Chọn chủ đề</h3><p class="muted" style="margin-bottom:12px">Nền vùng vẽ đặt riêng; mặc định tối cho Model, trắng cho Layout.</p><div class="grid2">' + card('theme','dark','Tối','Đỡ mỏi mắt khi vẽ lâu. Mặc định.', '<span style="height:64px;border-radius:3px;background:#1A1D21;box-shadow:inset 0 0 0 1px #2C3036;display:block"></span>') + card('theme','light','Sáng','Hợp phòng sáng, máy chiếu.', '<span style="height:64px;border-radius:3px;background:#F2F4F6;box-shadow:inset 0 0 0 1px #D0D5DB;display:block"></span>') + '</div>';
      if (s === 'ui') b = '<h3 style="margin:0 0 12px">Kiểu giao diện</h3><div class="grid2">' + card('ui','ribbon','Ribbon','Lệnh theo tab và panel, như AutoCAD từ bản 2009.') + card('ui','classic','Cổ điển','Thanh menu và toolbar kiểu cũ, không có ribbon.') + '</div>';
      if (s === 'migrate') b = '<h3 style="margin:0 0 4px">Đã tìm thấy AutoCAD 2024 trên máy</h3><p class="muted" style="margin-bottom:12px">Chuyển viết tắt lệnh, đường dẫn hỗ trợ, mẫu bản vẽ, CTB và LISP khởi động sang TrionCAD.</p><div class="note acc">' + ic('info') + '<span>Mở trình hướng dẫn Chuyển cài đặt (UI-K12, thiết kế ở Đợt 3).</span></div>';
      if (s === 'units') b = '<h3 style="margin:0 0 12px">Đơn vị và mẫu mặc định</h3><div class="frow"><span class="lab">Đơn vị</span><select class="inp"><option>Milimét (mm)</option><option>Mét (m)</option><option>Inch</option></select><span class="lab">Mẫu cho bản vẽ mới</span><select class="inp"><option>TCVN A3 ngang (mm)</option><option>TCVN A1 ngang (mm)</option><option>Trống (mm)</option></select></div>';
      if (s === 'privacy') b = '<h3 style="margin:0 0 4px">Gửi dữ liệu sử dụng ẩn danh?</h3><p class="muted" style="margin-bottom:12px">Giúp TrionCAD sửa lỗi và chạy nhanh hơn trên phần cứng phổ biến ở Việt Nam. Không gửi nội dung bản vẽ. Máy chủ đặt tại Việt Nam.</p>' +
        '<label class="chk" style="font-size:var(--fs-m)"><input type="checkbox" id="onb-tele"' + (ans.tele ? ' checked' : '') + '> Đồng ý gửi báo lỗi và số liệu hiệu năng ẩn danh</label><p class="faint" style="font-size:var(--fs-s);margin-top:8px">Mặc định tắt. Xem chi tiết dữ liệu thu thập ở Tùy chọn › Hệ thống › Quyền riêng tư.</p>';
      if (s === 'edition') b = '<h3 style="margin:0 0 12px">Bạn muốn dùng phiên bản nào?</h3><div class="stack">' + card('ed','viewer','TrionCAD Viewer · miễn phí','Mở, xem, đo, in, xuất PDF. Bác sĩ Font hiển thị đúng chữ tiếng Việt.') + card('ed','trial','Dùng thử TrionCAD Pro 30 ngày','Vẽ, sửa, lưu DWG, LISP. Cần email; hết hạn tự về Viewer, không khóa dữ liệu.') + card('ed','key','Kích hoạt bằng key','Đã mua giấy phép hoặc dùng giấy phép công ty.') + '</div>';
      if (s === 'tour') b = '<h3 style="margin:0 0 4px">Hướng dẫn nhanh 5 bước</h3><p class="muted" style="margin-bottom:12px">Trên bản vẽ mẫu: mở, xem, đo, đọc thuộc tính, in PDF. Mất khoảng 2 phút.</p><ol style="margin:0;padding-left:18px;display:grid;gap:4px;color:var(--c-text-2)"><li>Lăn chuột để zoom, giữ chuột giữa để pan</li><li>Bấm Đo › Đo nhanh rồi bấm vào một phòng</li><li>Chọn một đối tượng để xem Thuộc tính</li><li>Bác sĩ Font: chữ TCVN3 đã hiện đúng</li><li>In & Xuất › PDF</li></ol>';
      $('#onb-b', d.el).innerHTML = dots + b;
      $('#onb-back', d.el).disabled = i === 0; $('#onb-next', d.el).textContent = i === steps.length - 1 ? 'Bắt đầu hướng dẫn' : 'Tiếp';
      $$('input[type="radio"]', d.el).forEach(function(r){ r.addEventListener('change', function(){ ans[r.name] = r.value; if (r.name === 'theme'){ TC.S.theme = r.value; TC.applyTheme(); } render(); }); });
      var tl = $('#onb-tele', d.el); if (tl) tl.addEventListener('change', function(){ ans.tele = tl.checked; });
    }
    $('#onb-back', d.el).addEventListener('click', function(){ i = Math.max(0, i - 1); render(); });
    $('#onb-skip', d.el).addEventListener('click', function(){ finish(false); });
    $('#onb-next', d.el).addEventListener('click', function(){ if (i < steps.length - 1){ i++; render(); } else finish(true); });
    function finish(tour){ TC.S.onboarded = true; TC.save('onboarded', true); TC.save('theme', TC.S.theme); d.close(true); if (ans.ed === 'trial' && TC.S.edition === 'viewer') SC.trialStart(); if (tour) SC.tour(); }
    render();
  };
  // coach mark trên giao diện thật
  SC.tour = function(){
    if (A().active === 'start') A().activate('d1');
    var steps = [['#cv','Lăn chuột để zoom quanh con trỏ, giữ chuột giữa để pan. Nhấp đúp chuột giữa để xem toàn bộ.'],['#rb','Tab Đo › Đo nhanh: rê chuột thấy khoảng cách, bấm trong một phòng để có diện tích.'],['#dock-r','Chọn một đối tượng: palette Thuộc tính hiện thông tin, bấm vào giá trị để sao chép.'],['#sb-font','Chỉ báo Bác sĩ Font: chữ TCVN3 và font thiếu đã được hiển thị đúng.'],['#cmd','Dòng lệnh nhận mọi phím gõ. Gõ “in” hoặc PLOT để in, xuất PDF.']];
    var i = 0, ring = null, pop = null;
    function show(){
      if (ring) ring.remove(); if (pop) pop.remove();
      if (i >= steps.length) return;
      var t = $(steps[i][0]); if (!t || !t.offsetParent) { i++; return show(); }
      var r = t.getBoundingClientRect();
      ring = TC.el('<div style="position:fixed;z-index:870;border:2px solid var(--c-accent);border-radius:4px;pointer-events:none;box-shadow:0 0 0 9999px rgba(0,0,0,.35)"></div>'); document.body.appendChild(ring);
      ring.style.left = (r.left - 3) + 'px'; ring.style.top = (r.top - 3) + 'px'; ring.style.width = (r.width + 6) + 'px'; ring.style.height = (r.height + 6) + 'px';
      pop = TC.el('<div class="mcard" style="position:fixed;z-index:871;width:300px" role="dialog" aria-label="Hướng dẫn nhanh"><b>Bước ' + (i + 1) + ' / ' + steps.length + '</b><p class="muted">' + steps[i][1] + '</p><div class="acts"><button class="btn sm ghost" type="button" data-t="x">Kết thúc</button><span class="grow"></span><button class="btn sm primary" type="button" data-t="n">' + (i === steps.length - 1 ? 'Xong' : 'Tiếp') + '</button></div></div>');
      document.body.appendChild(pop);
      var px = TC.clamp(r.left + r.width / 2 - 150, 8, innerWidth - 308), py = r.bottom + 10 + pop.offsetHeight > innerHeight ? r.top - pop.offsetHeight - 10 : r.bottom + 10;
      if (r.height > innerHeight * .5) { py = r.top + 20; px = TC.clamp(r.left + 20, 8, innerWidth - 308); }
      pop.style.left = px + 'px'; pop.style.top = py + 'px';
      pop.addEventListener('click', function(e){ var b = e.target.closest('[data-t]'); if (!b) return; if (b.dataset.t === 'x') i = steps.length; else i++; show(); });
      $('[data-t="n"]', pop).focus();
    }
    show();
  };

  /* ================= tệp: mở UI-B01, phục hồi UI-B04/B10, tệp lớn UI-B13, thả tệp ================= */
  var F = TC.files = {};
  var FOLDERS = { 'D:\\DuAn\\NhaO-2T\\KienTruc':['Banve-01.dwg','Chi-tiet-cau-thang.dwg','Thong-ke-cua.dxf','Banve-01.bak'], 'D:\\DuAn\\NhaO-2T\\KetCau':['Ketcau-T2.dwg','Thong-ke-thep-mong.dxf','KC-luoi-truc.dwg'], '\\\\SRV-01\\DuAn\\2026\\THCS-AnPhu':['MB-Truong-THCS-An-Phu.dwg','MC-Truong-THCS.dwg'], 'D:\\Khao-sat\\Lo-B7':['Hien-trang-khu-dat.dwg'] };
  F.open = function(folder){
    var cur = folder || 'D:\\DuAn\\NhaO-2T\\KienTruc', filt = 'dwg', sel = [];
    var d = TC.dialog({ id:'open', title:'Mở bản vẽ', cmd:'OPEN', w:880, h:540, flush:true, code:'UI-B01',
      body:'<div style="display:grid;grid-template-columns:190px minmax(0,1fr) 230px;height:100%;min-height:0">' +
        '<nav class="vt-list" style="border-right:1px solid var(--c-border);padding:6px;background:var(--c-sunken);overflow:auto" aria-label="Vị trí">' + ['Gần đây'].concat(Object.keys(FOLDERS)).map(function(f){ return '<button class="vt" type="button" data-f="' + esc(f) + '" style="display:flex;gap:7px;align-items:center;width:100%;height:26px;padding:0 8px;border-radius:2px;color:var(--c-text-2);text-align:left;white-space:nowrap;overflow:hidden">' + ic(f === 'Gần đây' ? 'clock' : 'folder', 's12') + '<span style="overflow:hidden;text-overflow:ellipsis">' + esc(f.split('\\').filter(Boolean).pop()) + '</span></button>'; }).join('') + '</nav>' +
        '<div style="display:flex;flex-direction:column;min-height:0"><div class="pal-tools"><span class="mono faint" id="op-path" style="font-size:var(--fs-xs);flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap"></span><label class="search" style="width:180px">' + ic('search') + '<input id="op-q" placeholder="Tìm trong thư mục" aria-label="Tìm"></label></div><div style="flex:1;overflow:auto" id="op-list"></div></div>' +
        '<aside style="border-left:1px solid var(--c-border);padding:10px;display:grid;align-content:start;gap:8px" id="op-prev" aria-live="polite"></aside></div>',
      foot:'<span class="lab muted" style="font-size:var(--fs-s)">Tên tệp</span><input class="inp grow" id="op-name" aria-label="Tên tệp"><select class="inp" id="op-filt" aria-label="Loại tệp" style="width:190px"><option value="dwg">Bản vẽ (*.dwg)</option><option value="dxf">DXF (*.dxf)</option><option value="dwt">Mẫu (*.dwt)</option><option value="all">Mọi tệp CAD</option></select><button class="btn ghost" type="button" data-x="cancel">Hủy</button><span style="display:inline-flex"><button class="btn primary" type="button" id="op-go" style="border-radius:2px 0 0 2px">Mở</button><button class="btn primary" type="button" id="op-more" aria-label="Tùy chọn mở" style="border-radius:0 2px 2px 0;padding:0 6px;border-left:1px solid rgba(0,0,0,.25)">' + ic('down', 'xs') + '</button></span>' });
    function files(){ var list = cur === 'Gần đây' ? TC.RECENT.map(function(r){ return r.name; }) : FOLDERS[cur] || []; var q = TC.fold($('#op-q', d.el).value); return list.filter(function(n){ var ext = n.split('.').pop().toLowerCase(); return (filt === 'all' ? ['dwg','dxf','dwt'].indexOf(ext) >= 0 : ext === filt) && (!q || TC.fold(n).indexOf(q) >= 0); }); }
    function info(n){ var r = TC.RECENT.filter(function(x){ return x.name === n; })[0] || {}; return { date:r.date || '05/10/2026', size:r.size || '1,2 MB', ver:r.ver || (n.endsWith('.dxf') ? 'DXF 2013' : 'DWG 2018'), by:n.indexOf('Ketcau') === 0 ? 'ZWCAD 2025' : 'AutoCAD 2024', r:r }; }
    function render(){
      $('#op-path', d.el).textContent = cur;
      $$('[data-f]', d.el).forEach(function(b){ b.style.background = b.dataset.f === cur ? 'var(--c-accent-soft)' : ''; b.style.color = b.dataset.f === cur ? 'var(--c-text)' : ''; });
      var fl = files();
      $('#op-list', d.el).innerHTML = fl.length ? '<table class="tbl"><colgroup><col><col style="width:96px"><col style="width:84px"><col style="width:84px"></colgroup><thead><tr><th>Tên</th><th>Ngày sửa</th><th>Phiên bản</th><th class="num">Kích thước</th></tr></thead><tbody>' + fl.map(function(n){ var i = info(n); return '<tr data-file="' + esc(n) + '" class="' + (sel.indexOf(n) >= 0 ? 'is-sel' : '') + '"><td>' + ic('file', 's12') + ' ' + esc(n) + '</td><td>' + i.date + '</td><td>' + i.ver + '</td><td class="num">' + i.size + '</td></tr>'; }).join('') + '</tbody></table>' : '<div class="empty">' + ic('folder') + '<b>Thư mục không có tệp ' + (filt === 'all' ? 'CAD' : filt.toUpperCase()) + '</b>Đổi bộ lọc loại tệp hoặc chọn thư mục khác.</div>';
      var p = $('#op-prev', d.el);
      if (!sel.length){ p.innerHTML = '<span class="faint" style="font-size:var(--fs-s)">Chọn một tệp để xem trước. Giữ Ctrl để chọn nhiều tệp, mỗi tệp mở thành một tab.</span>'; }
      else { var n = sel[sel.length - 1], i2 = info(n); p.innerHTML = '<canvas width="420" height="280" style="width:100%;border-radius:2px;background:var(--c-canvas)"></canvas><b style="word-break:break-all">' + esc(n) + '</b><div class="frow" style="--lab-w:90px;font-size:var(--fs-s)"><span class="lab">Phiên bản</span><span>' + i2.ver + '</span><span class="lab">Lưu lần cuối</span><span>' + i2.by + '</span><span class="lab">Ngày sửa</span><span>' + i2.date + '</span><span class="lab">Kích thước</span><span>' + i2.size + '</span></div>' + (sel.length > 1 ? '<span class="tag acc">' + sel.length + ' tệp sẽ mở thành ' + sel.length + ' tab</span>' : '');
        var c = $('canvas', p), r = i2.r && i2.r.name ? i2.r : { name:n }; drawRecentThumb(c, r); }
      $('#op-name', d.el).value = sel.join('; ');
    }
    d.el.addEventListener('click', function(e){ var f = e.target.closest('[data-f]'); if (f){ cur = f.dataset.f; sel = []; render(); return; } var t = e.target.closest('[data-file]'); if (t){ var n = t.dataset.file; if (e.ctrlKey || e.metaKey) { sel = sel.indexOf(n) >= 0 ? sel.filter(function(x){ return x !== n; }) : sel.concat([n]); } else sel = [n]; render(); } });
    d.el.addEventListener('dblclick', function(e){ var t = e.target.closest('[data-file]'); if (t){ sel = [t.dataset.file]; go(false); } });
    $('#op-q', d.el).addEventListener('input', render);
    $('#op-filt', d.el).addEventListener('change', function(){ filt = this.value; render(); });
    $('#op-go', d.el).addEventListener('click', function(){ go(false); });
    $('#op-more', d.el).addEventListener('click', function(){ var r = this.getBoundingClientRect(); TC.menu([{ label:'Mở', act:function(){ go(false); } }, { label:'Mở chỉ đọc', icon:'lock', act:function(){ go(true); } }], r.right, r.top, { alignRight:true, up:true }); });
    function go(ro){ if (!sel.length){ $('#op-name', d.el).classList.add('err'); TC.toast('Chọn ít nhất một tệp.', { type:'warn', log:false }); return; } var list = sel.slice(); d.close(true); list.forEach(function(n){ F.openRecent(TC.RECENT.filter(function(x){ return x.name === n; })[0] || { name:n, path:cur }, ro); }); }
    render();
  };
  F.openRecent = function(r, ro){
    if (!r) return;
    if (r.doc){ var d0 = A().docs.filter(function(x){ return x.id === r.doc; })[0]; if (d0){ d0.readonly = !!ro; A().openDoc(d0.id); return; } }
    if (r.big) return F.openLarge(r);
    if (r.broken) return F.openBroken(r);
    var exist = A().docs.filter(function(x){ return x.name === r.name; })[0]; if (exist){ A().openDoc(exist.id); return; }
    var docs = TC.buildDocs(), nd = r.name.indexOf('Ketcau') >= 0 || r.name.indexOf('thep') >= 0 ? docs[1] : docs[0];
    nd.id = 'f' + Date.now(); nd.name = r.name; nd.path = (r.path || 'D:\\') + '\\' + r.name; nd.readonly = !!ro; nd.ver = r.ver || nd.ver;
    TC.prepDoc(nd); A().docs.push(nd); A().openDoc(nd.id);
  };
  F.openDropped = function(f){
    if (!/\.(dwg|dxf|dwt)$/i.test(f.name)){ TC.toast('TrionCAD mở được tệp DWG, DXF, DWT. “' + esc(f.name) + '” không phải bản vẽ.', { type:'err', log:false }); return; }
    F.openRecent({ name:f.name, path:'(kéo-thả)', size:TC.size(f.size), ver:'DWG' });
    TC.toast('Đã mở “' + esc(f.name) + '”. Prototype hiển thị bản vẽ mẫu thay cho nội dung tệp.', { type:'info', log:false });
  };
  // mở tệp lớn: khung hình đầu tiên xuất hiện sớm, xem/pan/zoom được trong khi nạp (UI-B13)
  F.openLarge = function(r){
    r = r || TC.RECENT.filter(function(x){ return x.big; })[0];
    var docs = TC.buildDocs(), d = docs[0]; d.id = 'big' + Date.now(); d.name = r.name; d.path = r.path + '\\' + r.name; d.bytes = 38.2 * 1048576;
    var base = d.ents.slice(); var all = []; var n = 0;
    for (var gx = 0; gx < 4; gx++) for (var gy = 0; gy < 3; gy++){ base.forEach(function(e){ var c = JSON.parse(JSON.stringify(e)); c.id = ++n; shift(c, gx * 145000, gy * 42000); all.push(c); }); }
    function shift(e, dx, dy){ var P = function(p){ return [p[0] + dx, p[1] + dy]; }; if (e.p1) { e.p1 = P(e.p1); e.p2 = P(e.p2); } if (e.pts) e.pts = e.pts.map(P); if (e.c) e.c = P(e.c); if (e.p) e.p = P(e.p); if (e.loops) e.loops = e.loops.map(function(l){ return l.map(P); }); if (e.t === 'dim') { e.pos += e.dir === 'h' ? dy : dx; } }
    d.ents = []; d.layouts = []; d.seq = n; TC.prepDoc(d); A().docs.push(d);
    var total = all.length, k = 0, cancelled = false;
    A().openDoc(d.id);
    var v = A().view; v.pushHist(); d.view = { cx:290000, cy:40000, s:v.H / 140000 }; v.draw();
    function tick(){
      if (cancelled) return;
      var chunk = Math.ceil(total / 24); var add = all.slice(k, k + chunk); k += add.length;
      add.forEach(function(e){ d.ents.push(e); }); TC.prepDoc(d); if (A().doc() === d) v.draw();
      var pct = Math.round(k / total * 100);
      A().setTask('<span class="sb-task">' + '<span class="spin"></span>Đang nạp ' + pct + '% · ' + TC.n(Math.round(1240000 * k / total)) + ' đối tượng<span class="prog"><i style="width:' + pct + '%"></i></span><button class="ib sm" type="button" id="load-x" aria-label="Hủy nạp tệp">' + ic('x') + '</button></span>');
      var x = $('#load-x'); if (x) x.onclick = function(){ cancelled = true; A().setTask(''); TC.toast('Đã dừng nạp. Bản vẽ hiện ' + pct + '% nội dung; mở lại để nạp đủ.', { type:'warn' }); };
      if (k < total) setTimeout(tick, 160); else { A().setTask(''); TC.toast('Đã mở <b>' + esc(r.name) + '</b>: 1.240.000 đối tượng trong 3,9 giây.', { kind:'task' }); }
    }
    TC.cmdline.msg('Đang nạp tệp lớn ở nền. Bạn đã xem, pan, zoom được; lệnh sửa chờ đến khi nạp xong.');
    setTimeout(tick, 200);
  };
  F.recover = function(){ F.openBroken(TC.RECENT.filter(function(x){ return x.broken; })[0]); };
  F.openBroken = function(r){
    r = r || TC.RECENT.filter(function(x){ return x.broken; })[0];
    TC.confirm({ title:'Không mở được bản vẽ', msg:'<b>' + esc(r.name) + '</b> có lỗi cấu trúc. Chạy Phục hồi để đọc phần còn dùng được?', detail:'Phục hồi không sửa tệp gốc. Bản đã phục hồi mở dưới dạng tab mới, gắn nhãn [Đã phục hồi].', ok:'Phục hồi' }).then(function(y){
      if (!y) return;
      var d = TC.dialog({ id:'rec', title:'Đang phục hồi ' + esc(r.name), cmd:'RECOVER', w:480, help:false, body:'<div class="stack"><div class="prog"><i id="rec-p" style="width:0%"></i></div><div class="mono faint" id="rec-log" style="font-size:var(--fs-xs);height:120px;overflow:auto;white-space:pre-wrap"></div></div>', foot:'<span class="grow"></span><button class="btn" type="button" data-x="cancel">Hủy</button>' });
      var logs = ['Đọc tiêu đề tệp … DWG 2004 (AC1018)', 'Đọc bảng lớp đối tượng … xong', 'Kiểm tra 18.402 đối tượng …', '  12 đối tượng có tham chiếu hỏng → gỡ liên kết', '  3 block lồng vòng → tách', '  1 bảng layer trùng tên “0” → gộp', 'Dựng lại chỉ mục không gian …', 'Phục hồi xong.'], k = 0;
      var t = setInterval(function(){ if (d.closed) { clearInterval(t); return; } var log = $('#rec-log', d.el); log.textContent += logs[k] + '\n'; log.scrollTop = 1e5; $('#rec-p', d.el).style.width = ((k + 1) / logs.length * 100) + '%'; k++; if (k >= logs.length){ clearInterval(t); setTimeout(function(){ d.close(true); report(r); }, 400); } }, 300);
    });
  };
  function report(r){
    var d = TC.dialog({ id:'recrep', title:'Báo cáo phục hồi', code:'UI-B10', w:560, body:'<div class="stack"><div class="hrow" style="gap:24px"><div><div class="faint" style="font-size:var(--fs-xs)">Lỗi tìm thấy</div><div style="font-size:var(--fs-2xl);font-weight:600">16</div></div><div><div class="faint" style="font-size:var(--fs-xs)">Đã sửa</div><div style="font-size:var(--fs-2xl);font-weight:600;color:var(--c-ok)">16</div></div><div><div class="faint" style="font-size:var(--fs-xs)">Đối tượng giữ lại</div><div style="font-size:var(--fs-2xl);font-weight:600">18.387</div></div></div>' +
      '<details open><summary style="cursor:pointer">Nhật ký chi tiết</summary><pre class="mono" style="font-size:var(--fs-xs);color:var(--c-text-2);background:var(--c-sunken);padding:8px;border-radius:2px;max-height:150px;overflow:auto;margin:6px 0 0">AcDbLine(4A2F)  tham chiếu layer hỏng  → đặt về layer 0\nAcDbBlockReference(51C0)  block lồng vòng  → tách khỏi BLK-CUA-01\nAcDbLayerTable  2 layer tên “0”  → gộp\n… 13 mục khác</pre></details>' +
      (TC.isViewer() ? '<div class="note">' + ic('info') + '<span>Viewer phục hồi để xem. Lưu bản đã phục hồi thành tệp mới cần TrionCAD Pro.</span></div>' : '') + '</div>',
      foot:'<button class="btn" type="button" id="rr-save">Lưu báo cáo…</button><span class="grow"></span><button class="btn" type="button" id="rr-saveas">Lưu thành…' + (TC.can('PRO') ? '' : ' <span class="ed xs" data-ed="pro">Pro</span>') + '</button><button class="btn primary" type="button" id="rr-open">Mở bản đã phục hồi</button>' });
    $('#rr-save', d.el).addEventListener('click', function(){ TC.toast('Đã lưu báo cáo phục hồi ra <span class="mono">' + esc(r.name.replace(/\.dwg$/, '')) + '-phuc-hoi.txt</span>.', { log:false }); });
    $('#rr-saveas', d.el).addEventListener('click', function(){ if (!TC.can('PRO')) SC.upgrade('Lưu bản vẽ đã phục hồi', 'PRO'); else { $('#rr-open', d.el).click(); setTimeout(function(){ TC.ed.saveAs(A().doc()); }, 60); } });
    $('#rr-open', d.el).addEventListener('click', function(){
      d.close(true); var docs = TC.buildDocs(), nd = docs[0]; nd.id = 'rec' + Date.now(); nd.name = r.name; nd.path = r.path + '\\' + r.name; nd.recovered = true; nd.ver = 'DWG 2004'; TC.prepDoc(nd); A().docs.push(nd); A().openDoc(nd.id);
      A().banner(nd, { id:'recovered', type:'warn', icon:'recover', html:'Bản vẽ đã được phục hồi. Lưu thành tệp mới để giữ bản đã sửa; tệp gốc không bị thay đổi.', acts:[{ label:'Lưu thành…', need:'PRO', feature:'Lưu bản vẽ đã phục hồi', fn:function(){ TC.ed.saveAs(nd); } }, { label:'Xem báo cáo', fn:function(){ report(r); } }] });
    });
  }
  SC.missingRefs = function(){
    var d0 = A().doc(); if (!d0) return; var fd = TC.fontDoctor.summary(d0);
    var groups = [['Font', d0.styles.filter(function(s){ return s.missing && s.count; }).map(function(s){ return { n:s.font, path:'(font hệ thống)', st:'Đã thay bằng ' + s.sub, ok:true, act:'font' }; })], ['Xref', d0.xrefs.map(function(x){ return { n:x.name, path:x.path, st:x.status, ok:x.status !== 'Không tìm thấy', act:'find' }; })], ['Ảnh', d0.images.map(function(x){ return { n:x.name, path:x.path, st:x.status, ok:false, act:'find' }; })], ['PDF underlay', []], ['Plot style', [{ n:'TCVN.ctb', path:'C:\\TrionCAD\\Plot Styles\\TCVN.ctb', st:'Tìm thấy', ok:true }]], ['Shape', []]];
    var d = TC.dialog({ id:'refs', title:'Tham chiếu của bản vẽ', code:'UI-B06', w:720, body:'<div class="stack">' + groups.filter(function(g){ return g[1].length; }).map(function(g){ return '<div><div class="gh" style="margin-bottom:6px">' + g[0] + ' (' + g[1].length + ')</div><div class="tblwrap"><table class="tbl"><colgroup><col style="width:180px"><col><col style="width:150px"><col style="width:150px"></colgroup><tbody>' + g[1].map(function(it){ return '<tr><td>' + esc(it.n) + '</td><td class="mono faint" style="font-size:var(--fs-xs)">' + esc(it.path) + '</td><td><span class="tag ' + (it.ok ? 'ok' : 'danger') + '">' + esc(it.st) + '</span></td><td>' + (it.act === 'font' ? '<button class="btn sm" type="button" data-rf="font">Bác sĩ Font</button>' : it.act === 'find' && !it.ok ? '<button class="btn sm" type="button" data-rf="find">Tìm tệp…</button> <button class="btn sm ghost" type="button" data-rf="skip">Bỏ qua</button>' : '') + '</td></tr>'; }).join('') + '</tbody></table></div></div>'; }).join('') +
      '<label class="chk"><input type="checkbox"> Nhớ lựa chọn cho các lần sau</label>' + (TC.isViewer() ? '<p class="faint" style="font-size:var(--fs-xs)">Viewer tìm tệp để hiển thị; sửa đường dẫn và lưu vào bản vẽ cần TrionCAD Pro.</p>' : '') + '</div>',
      foot:'<button class="btn" type="button" data-rf="folder">' + ic('folder') + 'Tìm trong thư mục…</button><span class="grow"></span><button class="btn primary" type="button" data-x="close">Đóng</button>' });
    d.el.addEventListener('click', function(e){ var b = e.target.closest('[data-rf]'); if (!b) return; if (b.dataset.rf === 'font'){ d.close(); A().showPalette('fontdoc'); } else if (b.dataset.rf === 'find' || b.dataset.rf === 'folder'){ var x = d0.xrefs.filter(function(q){ return q.status === 'Không tìm thấy'; })[0]; if (x){ x.status = 'Đã nạp'; x.found = 'D:\\DuAn\\NhaO-2T\\HienTrang\\Hien-trang.dwg'; } TC.toast('Tìm thấy 1 xref trong D:\\DuAn\\NhaO-2T\\HienTrang. Đã nạp để xem.', {}); d.close(); A().unbanner(d0, 'missing'); A().refreshPalettes(); } else if (b.dataset.rf === 'skip') b.closest('tr').style.opacity = .5; });
  };

  /* ================= Quản lý layer UI-C03 (chế độ xem trong Viewer) ================= */
  SC.layerManager = function(){
    var d0 = A().doc(); if (!d0) return; var ls = TC.lstate;
    var tools = [['new','Layer mới','PRO'],['snow','Layer mới đóng băng ở mọi viewport','PRO'],['trash','Xóa','PRO'],['laycur','Đặt hiện hành','PRO'],['refresh','Làm mới','V'],['layers','Trạng thái layer','PRO'],['filter','Bộ lọc thuộc tính mới','PRO'],['lib','Nạp bộ layer chuẩn TCVN…','PRO'],['gear','Thiết lập','PRO']];
    var dlg = TC.dialog({ id:'layman', title:'Quản lý layer', cmd:'LAYER', code:'UI-C03', w:1080, h:560, modal:false, flush:true,
      body:'<div style="display:flex;flex-direction:column;height:100%;min-height:0">' + (TC.isViewer() ? '<div class="pal-note">' + ic('info') + '<span>Chế độ xem: bật/tắt và đóng băng là tạm thời, <b>không lưu vào tệp</b>. Đổi màu, linetype, khóa layer cần TrionCAD Pro.</span></div>' : '') +
        '<div class="pal-tools">' + tools.map(function(t){ var lock = !TC.can(t[2]); return '<button class="ib" type="button" data-lmt="' + t[0] + '" aria-label="' + t[1] + '" data-tip="' + t[1] + '"' + (lock ? ' data-need="' + t[2] + '"' : '') + '>' + ic(t[0]) + '</button>' + (t[0] === 'refresh' || t[0] === 'layers' ? '<span class="sep"></span>' : ''); }).join('') + '<span class="grow"></span><label class="search" style="width:240px">' + ic('search') + '<input id="lm-q" placeholder="Tìm layer, có wildcard: KT-*" value="' + esc(ls.q) + '" aria-label="Tìm layer"></label></div>' +
        '<div style="display:grid;grid-template-columns:180px minmax(0,1fr);flex:1;min-height:0"><div class="tree" style="border-right:1px solid var(--c-border);padding:6px;overflow:auto" id="lm-tree"></div><div style="overflow:auto;min-height:0" id="lm-body"></div></div>' +
        '<div class="pal-foot" id="lm-foot"></div></div>' });
    function render(){
      var t = TC.layerTable(d0, true);
      $('#lm-body', dlg.el).innerHTML = t.html;
      var used = d0.layers.filter(function(l){ return l.count; }).length;
      var tn = function(f){ return '<button class="tn" type="button" data-lmf="' + f[0] + '" aria-selected="' + (ls.filter === f[0]) + '">' + ic(f[0] === 'all' ? 'layers' : 'filter', 's12') + f[1] + '<span class="cnt">' + f[2] + '</span></button>'; };
      $('#lm-tree', dlg.el).innerHTML = [['all','Tất cả', d0.layers.length], ['used','Layer đang dùng', used]].map(tn).join('') + (SC.layerTreeHTML && !TC.isViewer() ? SC.layerTreeHTML(d0, ls) : '') + tn(['vp','Viewport có ghi đè', 0]) + '<div class="gh" style="padding:8px 6px 4px">Xref</div>' + d0.xrefs.map(function(x){ return '<button class="tn" type="button" disabled>' + ic('xref', 's12') + esc(x.name) + '<span class="cnt">' + (x.status === 'Đã nạp' ? 6 : 0) + '</span></button>'; }).join('') + (SC.layerTreeFoot && !TC.isViewer() ? SC.layerTreeFoot(ls) : '');
      $('#lm-foot', dlg.el).innerHTML = '<span>Hiện ' + t.n + '/' + d0.layers.length + ' layer · ' + (ls.sel ? '1 đang chọn' : '0 đang chọn') + (TC.isViewer() ? '' : ' · hiện hành: <b>' + esc(TC.ed.clayer(d0)) + '</b>') + '</span><span class="grow"></span>' + (TC.isViewer() ? (TC.layers.changed(d0) ? '<button class="btn sm" type="button" id="lm-restore">' + ic('undo') + 'Khôi phục như tệp gốc</button>' : '<span class="faint">Không có thay đổi tạm thời</span>') : '<span class="faint">Nhấp đúp để đặt hiện hành · F2 đổi tên · chuột phải để có thêm thao tác</span>');
      var rs = $('#lm-restore', dlg.el); if (rs) rs.onclick = function(){ TC.layers.restore(d0); render(); };
    }
    TC.bindLayerTable($('#lm-body', dlg.el), d0, function(){ render(); A().refreshPalettes(); });
    if (SC.layerTreeBind && !TC.isViewer()) SC.layerTreeBind($('#lm-body', dlg.el), $('#lm-tree', dlg.el), d0, render);
    $('#lm-q', dlg.el).addEventListener('input', function(){ ls.q = this.value; render(); });
    dlg.el.addEventListener('click', function(e){ var f = e.target.closest('[data-lmf]'); if (f){ ls.filter = f.dataset.lmf === 'vp' ? 'all' : f.dataset.lmf; render(); A().refreshPalettes(); } var t = e.target.closest('[data-lmt]'); if (t){ if (t.dataset.need && !TC.can(t.dataset.need)) SC.upgrade(t.dataset.tip, t.dataset.need); else if (TC.layerTool) TC.layerTool(t.dataset.lmt, d0, render, t); else render(); } });
    TC.on('layers', function(){ if (!dlg.closed) render(); });
    render();
  };
  // dropdown layer trên ribbon: đổi bật/tắt, đóng băng ngay trong danh sách
  SC.layerDrop = function(btn){
    var d0 = A().doc(); if (!d0) return; var r = btn.getBoundingClientRect();
    var pop = TC.el('<div class="menu" role="dialog" aria-label="Layer" style="width:300px;padding:0"><div style="padding:6px"><label class="search">' + ic('search') + '<input id="ld-q" placeholder="Tìm layer" aria-label="Tìm layer"></label></div><div id="ld-b" style="max-height:360px;overflow:auto"></div>' + (TC.isViewer() ? '<div class="pal-foot">Thay đổi tạm thời, không lưu vào tệp</div>' : '') + '</div>');
    document.body.appendChild(pop); pop.style.left = r.left + 'px'; pop.style.top = (r.bottom + 2) + 'px';
    function render(){ var q = TC.fold($('#ld-q', pop).value); $('#ld-b', pop).innerHTML = '<table class="tbl"><colgroup><col style="width:26px"><col style="width:26px"><col style="width:26px"><col style="width:22px"><col></colgroup><tbody>' + d0.layers.filter(function(l){ return !q || TC.fold(l.name).indexOf(q) >= 0; }).map(function(l){ var on = TC.layers.get(d0, l.name, 'on'), fz = TC.layers.get(d0, l.name, 'frz'); return '<tr><td><button class="tgl' + (on ? '' : ' off') + '" type="button" data-ldt="on" data-ln="' + esc(l.name) + '" aria-label="Bật/tắt ' + esc(l.name) + '">' + ic(on ? 'bulb' : 'bulboff') + '</button></td><td><button class="tgl' + (fz ? ' off' : '') + '" type="button" data-ldt="frz" data-ln="' + esc(l.name) + '" aria-label="Đóng băng ' + esc(l.name) + '">' + ic(fz ? 'snow' : 'sun') + '</button></td><td>' + (TC.isViewer() ? '<button class="tgl" type="button" disabled aria-label="Khóa">' + ic('unlock') + '</button>' : '<button class="tgl' + (l.lck ? ' off' : '') + '" type="button" data-ldt="lck" data-ln="' + esc(l.name) + '" aria-label="Khóa ' + esc(l.name) + '">' + ic(l.lck ? 'lock' : 'unlock') + '</button>') + '</td><td><i class="sw" style="background:' + (l.color === 7 ? '#FFF' : TC.ACI[l.color]) + '"></i></td><td' + (TC.isViewer() ? '' : ' data-ldset="' + esc(l.name) + '" style="cursor:pointer" title="Đặt hiện hành"') + '>' + (l.name === (TC.ed ? TC.ed.clayer(d0) : 'KT-TUONG') ? '<b>' + esc(l.name) + '</b>' : esc(l.name)) + '</td></tr>'; }).join('') + '</tbody></table>'; }
    render(); setTimeout(function(){ $('#ld-q', pop).focus(); }, 0);
    $('#ld-q', pop).addEventListener('input', render);
    pop.addEventListener('click', function(e){ var st = e.target.closest('[data-ldset]'); if (st && TC.ed){ if (TC.ed.setCurLayer(st.dataset.ldset, d0)){ pop.remove(); TC.cmdline.msg('Layer hiện hành: <b>' + esc(st.dataset.ldset) + '</b>.'); } return; } var t = e.target.closest('[data-ldt]'); if (!t) return; TC.layers.set(d0, t.dataset.ln, t.dataset.ldt, !TC.layers.get(d0, t.dataset.ln, t.dataset.ldt)); A().view.draw(); render(); TC.emit('layers'); });
    function out(e){ if (!pop.contains(e.target) && e.target !== btn) { pop.remove(); document.removeEventListener('mousedown', out, true); } }
    document.addEventListener('mousedown', out, true);
    pop.addEventListener('keydown', function(e){ if (e.key === 'Escape'){ pop.remove(); btn.focus(); } });
  };

  /* ================= Tìm chữ UI-F09 (Viewer: chỉ tìm) ================= */
  SC.find = function(){
    var d0 = A().doc(); if (!d0) return;
    var dlg = TC.dialog({ id:'find', title:'Tìm chữ', cmd:'FIND', code:'UI-F09', w:620, modal:false,
      body:'<div class="stack"><div class="frow"><label class="lab" for="fd-q">Tìm</label><input class="inp" id="fd-q" placeholder="Ví dụ: tường, cua di, KT-12"><label class="lab" for="fd-r">Thay bằng</label><span class="hrow"><input class="inp grow" id="fd-r" disabled placeholder="Thay chữ cần TrionCAD Pro"><span class="ed xs" data-ed="pro">Pro</span></span><span class="lab">Phạm vi</span><select class="inp" id="fd-sc"><option>Toàn bản vẽ</option><option>Không gian hiện tại</option><option>Vùng chọn</option></select></div>' +
        '<div class="grid2"><div class="stack" style="gap:4px"><label class="chk"><input type="checkbox" id="fd-case"> Phân biệt hoa thường</label><label class="chk"><input type="checkbox" id="fd-word"> Nguyên từ</label><label class="chk"><input type="checkbox" id="fd-acc"> Phân biệt dấu tiếng Việt</label></div><div class="stack" style="gap:4px"><label class="chk"><input type="checkbox" id="fd-blk" checked> Tìm trong block, attribute</label><label class="chk"><input type="checkbox" id="fd-old" checked> Tìm cả chữ mã TCVN3, VNI</label><label class="chk"><input type="checkbox" id="fd-dim" checked> Chữ của kích thước</label></div></div>' +
        '<div class="tblwrap" style="height:200px" id="fd-res"><div class="empty">' + ic('findtext') + 'Gõ chữ cần tìm rồi bấm Tìm. Tắt “Phân biệt dấu” thì “duong” khớp “đường”.</div></div></div>',
      foot:'<span class="faint" id="fd-n" style="font-size:var(--fs-s)"></span><span class="grow"></span><button class="btn" type="button" id="fd-set" disabled>Tạo tập chọn</button><button class="btn" type="button" id="fd-rep">Thay tất cả <span class="ed xs" data-ed="pro">Pro</span></button><button class="btn primary" type="button" id="fd-go">Tìm</button>' });
    var hits = [];
    function search(){
      var q = $('#fd-q', dlg.el).value.trim(); if (!q) return;
      var cs = $('#fd-case', dlg.el).checked, acc = $('#fd-acc', dlg.el).checked, word = $('#fd-word', dlg.el).checked, blk = $('#fd-blk', dlg.el).checked, dim = $('#fd-dim', dlg.el).checked;
      var norm = function(s){ s = cs ? s : s.toLowerCase(); return acc ? s : TC.fold(s); }, nq = norm(q);
      var test = function(s){ var ns = norm(s); return word ? new RegExp('(^|\\s)' + nq.replace(/[.*+?^${}()|[\]\\]/g, '\\$&') + '(\\s|$)').test(ns) : ns.indexOf(nq) >= 0; };
      hits = [];
      d0.ents.forEach(function(e){
        if (e.t === 'text' && test(e.s)) hits.push({ e:e, s:e.s, k:'Text', sp:'Model' });
        if (dim && e.t === 'dim' && test(String(Math.round(e._meas)))) hits.push({ e:e, s:String(Math.round(e._meas)), k:'Chữ kích thước', sp:'Model' });
        if (blk && e.t === 'insert' && e.attrs) Object.keys(e.attrs).forEach(function(k){ if (test(e.attrs[k])) hits.push({ e:e, s:e.attrs[k], k:'Attribute ' + k, sp:'Model' }); });
      });
      d0.layouts.forEach(function(L){ L.ents.forEach(function(e){ if (blk && e.t === 'insert' && e.attrs) Object.keys(e.attrs).forEach(function(k){ if (test(e.attrs[k])) hits.push({ e:e, s:e.attrs[k], k:'Attribute ' + k, sp:L.name }); }); }); });
      $('#fd-n', dlg.el).textContent = hits.length + ' kết quả';
      $('#fd-set', dlg.el).disabled = !hits.length;
      $('#fd-res', dlg.el).innerHTML = hits.length ? '<table class="tbl"><colgroup><col style="width:110px"><col style="width:120px"><col></colgroup><thead><tr><th>Vị trí</th><th>Loại</th><th>Đoạn chữ</th></tr></thead><tbody>' + hits.map(function(h, i){ var st = d0.styleMap[h.e.style] || {}; return '<tr data-hit="' + i + '"><td>' + esc(h.sp) + '</td><td>' + esc(h.k) + '</td><td>' + esc(h.s) + (st.enc === 'TCVN3' || st.enc === 'VNI' ? ' <span class="tag warn">' + st.enc + '</span>' : '') + '</td></tr>'; }).join('') + '</tbody></table>' : '<div class="empty">' + ic('search') + '<b>Không tìm thấy “' + esc(q) + '”</b>Thử tắt “Phân biệt dấu” hoặc mở rộng phạm vi.</div>';
      d0.found = hits.map(function(h){ return h.e.id; }); A().view.drawOver();
    }
    $('#fd-go', dlg.el).addEventListener('click', search);
    $('#fd-q', dlg.el).addEventListener('keydown', function(e){ if (e.key === 'Enter'){ e.preventDefault(); e.stopPropagation(); search(); } });
    $('#fd-rep', dlg.el).addEventListener('click', function(){ SC.upgrade('Tìm và thay thế chữ', 'PRO'); });
    $('#fd-set', dlg.el).addEventListener('click', function(){ d0.sel = hits.map(function(h){ return h.e.id; }); TC.cad.afterSel(); TC.cmdline.msg('Đã tạo tập chọn ' + d0.sel.length + ' đối tượng từ kết quả tìm.'); });
    dlg.el.addEventListener('click', function(e){ var tr = e.target.closest('[data-hit]'); if (!tr) return; $$('[data-hit]', dlg.el).forEach(function(x){ x.classList.toggle('is-sel', x === tr); }); var h = hits[Number(tr.dataset.hit)]; if (h.sp !== 'Model') { A().setSpace(d0.layouts.map(function(l){ return l.name; }).indexOf(h.sp)); return; } if (d0.space !== 'model') A().setSpace('model'); d0.found = [h.e.id]; A().view.zoomTo(h.e._bb); });
  };

  /* ================= cửa sổ văn bản UI-I03 ================= */
  SC.textWindow = function(text, title){
    var content = text || TC.cmdline.hist().map(function(h){ return h.text; }).join('\n');
    var dlg = TC.dialog({ id:'txtw' + (text ? 'l' : 'h'), title:title || 'Lịch sử dòng lệnh', cmd:text ? 'LIST' : 'TEXTSCR', code:'UI-I03', w:760, h:480, modal:false, flush:true,
      body:'<div style="display:flex;flex-direction:column;height:100%"><div class="pal-tools"><label class="search grow">' + ic('search') + '<input id="tw-q" placeholder="Tìm trong nội dung" aria-label="Tìm"></label><button class="btn sm" type="button" id="tw-copy">' + ic('copyclip') + 'Sao chép</button><button class="btn sm" type="button" id="tw-save">' + ic('saveas') + 'Lưu ra tệp…</button></div><pre id="tw-pre" class="mono" style="flex:1;margin:0;padding:10px 12px;overflow:auto;font-size:var(--fs-s);color:var(--c-text);white-space:pre-wrap"></pre></div>' });
    function paint(q){ var pre = $('#tw-pre', dlg.el); if (!q) { pre.textContent = content; return; } pre.innerHTML = esc(content).replace(new RegExp(esc(q).replace(/[.*+?^${}()|[\]\\]/g, '\\$&'), 'gi'), function(m){ return '<mark style="background:var(--c-warn-soft);color:var(--c-text)">' + m + '</mark>'; }); }
    paint('');
    $('#tw-q', dlg.el).addEventListener('input', function(){ paint(this.value); });
    $('#tw-copy', dlg.el).addEventListener('click', function(){ TC.copyText(content); });
    $('#tw-save', dlg.el).addEventListener('click', function(){ TC.toast('Đã lưu ra <span class="mono">' + (text ? 'list' : 'lich-su-lenh') + '.txt</span>.', { log:false }); });
  };
  SC.dwgProps = function(){
    var d0 = A().doc(); if (!d0) return;
    TC.dialog({ id:'dwgprops', title:'Thuộc tính bản vẽ', cmd:'DWGPROPS', code:'UI-B08', w:520, body:'<div class="tabs" role="tablist"><button class="tab" type="button" aria-selected="true">Chung</button><button class="tab" type="button" aria-selected="false">Tóm tắt</button><button class="tab" type="button" aria-selected="false">Thống kê</button><button class="tab" type="button" aria-selected="false">Tùy biến</button></div><div class="frow" style="padding-top:12px"><span class="lab">Tên tệp</span><span>' + esc(d0.name) + '</span><span class="lab">Vị trí</span><span class="mono" style="font-size:var(--fs-xs)">' + esc(d0.path) + '</span><span class="lab">Kích thước</span><span>' + TC.size(d0.bytes) + '</span><span class="lab">Phiên bản</span><span>' + esc(d0.ver) + '</span><span class="lab">Lưu lần cuối bởi</span><span>' + esc(d0.savedBy) + '</span><span class="lab">Ngày sửa</span><span>' + esc(d0.modified) + '</span></div><p class="faint" style="font-size:var(--fs-xs);margin-top:10px">Viewer chỉ xem. Đầy đủ các tab thiết kế ở Đợt 3.</p>', foot:'<span class="grow"></span><button class="btn primary" type="button" data-x="close">Đóng</button>' });
  };
})();
