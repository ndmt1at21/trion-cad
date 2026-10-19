/* TrionCAD prototype · khung ứng dụng UI-A01: thanh tiêu đề, QAT, ribbon, tab tệp, dock palette, vùng vẽ, tab layout, thanh trạng thái. */
(function(){
  'use strict';
  var TC = window.TC, $ = TC.$, $$ = TC.$$, esc = TC.esc, ic = TC.ic;
  var A = TC.app = { docs:[], active:'start', view:null };

  /* ================= khung ================= */
  A.mount = function(root){
    root.innerHTML =
      '<header class="tb" id="tb"></header>' +
      '<nav class="rb" id="rb" aria-label="Ribbon"></nav>' +
      '<div class="ft" id="ft" role="tablist" aria-label="Bản vẽ đang mở"></div>' +
      '<div class="ws" id="ws">' +
        '<div class="dock left" id="dock-l"></div>' +
        '<div class="cv" id="cv" tabindex="0" aria-label="Vùng vẽ"><div class="cv-banners" id="banners"></div>' +
          '<div class="cv-vpc" id="vpc"><button type="button" data-vpc="m">[–]</button><button type="button" data-vpc="v">[Trên]</button><button type="button" data-vpc="s">[2D Wireframe]</button></div>' +
          '<svg class="cv-ucs" viewBox="0 0 64 64" fill="none" stroke="currentColor" stroke-width="1.5" aria-hidden="true"><path d="M10 54V12M10 54h42"/><path d="M6 18l4-7 4 7M46 50l7 4-7 4"/><path d="M10 54h7v-7h-7z"/><text x="56" y="47" fill="currentColor" stroke="none" font-size="11" font-family="Arial">X</text><text x="16" y="13" fill="currentColor" stroke="none" font-size="11" font-family="Arial">Y</text></svg>' +
        '</div>' +
        '<div class="dock right" id="dock-r"></div>' +
        '<div class="start" id="start" hidden></div>' +
      '</div>' +
      '<div class="lt" id="lt" role="tablist" aria-label="Model và layout"></div>' +
      '<section class="cmd" id="cmd" aria-label="Dòng lệnh"></section>' +
      '<footer class="sb" id="sb"></footer>' +
      '<div class="toasts" id="toasts" aria-live="polite"></div>';
    A.docs = TC.buildDocs(); A.docs.forEach(TC.prepDoc);
    A.view = new TC.View($('#cv'));
    renderTitle(); TC.cmdline.mount($('#cmd')); renderStatus(); bindCanvas(); bindKeys();
    A.openDoc('d1', true);
    A.openDoc('d2', true);
    A.activate('d1');
    new ResizeObserver(function(){ if (A.active !== 'start') A.view.resize(); fitRibbon(); }).observe($('#cv'));
    TC.on('edition', function(){ renderTitle(); renderRibbon(); renderStatus(); renderDocks(); A.renderFileTabs(); if (A.active === 'start') TC.screens.start(); TC.emit('sel'); });
    TC.on('theme', function(){ if (A.active !== 'start') A.view.draw(); });
    TC.on('view', function(){ TC.emit('viewchg'); });
  };
  A.doc = function(){ return A.docs.filter(function(d){ return d.id === A.active; })[0] || null; };

  /* ================= thanh tiêu đề, QAT, ô Tìm lệnh ================= */
  function renderTitle(){
    var ed = TC.EDITIONS[TC.S.edition], d = A.doc(), viewer = TC.isViewer();
    var qat = viewer ? [['open','Mở','OPEN','Ctrl+O'], ['print','In','PLOT','Ctrl+P']]
      : [['new','Mới','NEW','Ctrl+N'],['open','Mở','OPEN','Ctrl+O'],['save','Lưu','QSAVE','Ctrl+S'],['saveas','Lưu thành','SAVEAS','Ctrl+Shift+S'],'|',['undo','Hoàn tác','U','Ctrl+Z'],['redo','Làm lại','REDO','Ctrl+Y'],'|',['print','In','PLOT','Ctrl+P']];
    $('#tb').innerHTML =
      '<button class="tb-app" type="button" id="tb-app" aria-haspopup="menu" aria-expanded="false" data-tip="Menu ứng dụng" data-desc="Mở, xuất, in, tiện ích bản vẽ, tùy chọn và tệp gần đây.">' + ic('logo', 'logo') + '<span class="tb-brand">TrionCAD</span>' + ic('down', 'xs') + '</button>' +
      '<div class="qat" role="toolbar" aria-label="Thanh truy cập nhanh">' + qat.map(function(q){ if (q === '|') return '<span class="sep"></span>'; var split = q[0] === 'undo' || q[0] === 'redo' || q[0] === 'print', dis = d && !viewer && ((q[0] === 'undo' && !(d.undo || []).length) || (q[0] === 'redo' && !(d.redo || []).length)), b = '<button class="ib" type="button" data-run="' + q[2] + '" aria-label="' + q[1] + '" data-tip="' + q[1] + (q[0] === 'undo' && d && (d.undo || []).length ? ' ' + esc(d.undo[d.undo.length - 1].label) : '') + '" data-cmd="' + q[2] + '" data-key="' + q[3] + '"' + (dis ? ' disabled' : '') + '>' + ic(q[0]) + '</button>'; return split ? '<span class="qsplit">' + b + '<button class="ib qdd" type="button" data-qlist="' + q[0] + '" aria-label="' + (q[0] === 'print' ? 'Các kiểu in' : 'Danh sách ' + q[1].toLowerCase()) + '"' + (dis ? ' disabled' : '') + '>' + ic('down', 'xs') + '</button></span>' : b; }).join('') +
        '<button class="ib" type="button" id="qat-more" aria-label="Tùy biến thanh truy cập nhanh" data-tip="Tùy biến thanh truy cập nhanh">' + ic('down', 'xs') + '</button></div>' +
      '<div class="tb-doc" id="tb-doc">' + (d ? '<span class="tb-file">' + esc(d.name) + (d.dirty ? '<span class="tb-dirty">*</span>' : '') + '</span>' + (viewer ? '<span class="tb-flag">[Chỉ xem]</span>' : '') + (d.recovered ? '<span class="tb-flag">[Đã phục hồi]</span>' : '') + '<span>—</span>' : '') +
        '<span>' + ed.name + '</span>' + edBadge() + '</div>' +
      '<label class="tb-search" id="tb-search">' + ic('search') + '<input id="tb-q" type="search" placeholder="Tìm lệnh: “đường tròn”, CIRCLE, C…" aria-label="Tìm lệnh" autocomplete="off" spellcheck="false"></label>' +
      '<button class="proto-btn" type="button" id="proto" aria-haspopup="menu" data-tip="Công cụ prototype" data-desc="Đổi phiên bản, chủ đề, kích thước màn hình; mở danh mục 172 màn hình. Không thuộc sản phẩm.">' + ic('gear', 's12') + 'Prototype</button>' +
      '<button class="ib" type="button" id="tb-user" aria-label="Tài khoản" data-tip="Tài khoản">' + ic('user') + '</button>' +
      '<button class="ib" type="button" id="tb-help" aria-label="Trợ giúp" data-tip="Trợ giúp" data-key="F1">' + ic('help') + '</button>' +
      '<div class="win"><button type="button" aria-label="Thu nhỏ cửa sổ">' + ic('min') + '</button><button type="button" aria-label="Phóng to cửa sổ">' + ic('max') + '</button><button type="button" class="x" aria-label="Đóng TrionCAD" data-run="QUIT">' + ic('close') + '</button></div>';
    $('#tb-app').addEventListener('click', function(){ TC.screens.appMenu(this); });
    $$('[data-qlist]', $('#tb')).forEach(function(b){ b.addEventListener('click', function(){ var k = b.dataset.qlist, r = b.getBoundingClientRect(); if (k === 'print') TC.menu([{ label:'In…', icon:'print', kbd:'Ctrl+P', act:function(){ TC.cmd.run('PLOT'); } }, { label:'In nhiều layout…', icon:'publish', need:'VP', act:function(){ TC.cmd.run('PUBLISH'); } }, { label:'In theo khung tên…', icon:'batchplot', need:'VP', act:function(){ TC.cmd.run('TCBATCHPLOT'); } }, { label:'Xem trước', icon:'preview', act:function(){ TC.cmd.run('PREVIEW'); } }], r.left, r.bottom + 2, { returnFocus:b }); else if (TC.ed) TC.ed.undoMenu(b, k === 'redo'); }); });
    $('#qat-more').addEventListener('click', function(){ var r = this.getBoundingClientRect(); TC.menu(qatMenu(), r.left, r.bottom + 2, { returnFocus:this }); });
    $('#proto').addEventListener('click', function(){ var r = this.getBoundingClientRect(); TC.menu(TC.protoMenu(), r.right, r.bottom + 2, { alignRight:true, returnFocus:this, minWidth:260 }); });
    $('#tb-user').addEventListener('click', function(){ var r = this.getBoundingClientRect(); TC.menu(userMenu(), r.right, r.bottom + 2, { alignRight:true, returnFocus:this, minWidth:250 }); });
    $('#tb-help').addEventListener('click', function(){ var r = this.getBoundingClientRect(); TC.menu(helpMenu(), r.right, r.bottom + 2, { alignRight:true, returnFocus:this }); });
    TC.search.bind($('#tb-q'));
    TC.emit('title');
  }
  // giấy phép cần xác minh định kỳ mà lâu chưa kết nối: huy hiệu đếm ngược số ngày còn dùng offline (UI-L03)
  A.renderTitle = renderTitle;
  TC.on('dirty', function(){ renderTitle(); A.renderFileTabs(); });
  function edBadge(cls){ var e = TC.S.edition; var off = TC.S.licSim === 'offline' && e !== 'viewer'; return '<span class="ed' + (cls ? ' ' + cls : '') + (off ? ' warn' : '') + '" data-ed="' + e + '">' + (e === 'trial' ? 'Dùng thử: còn ' + TC.S.trialDays + ' ngày' : TC.EDITIONS[e].badge) + (off ? ' · còn ' + TC.S.offDays + ' ngày offline' : '') + '</span>'; }
  A.edBadge = edBadge;
  function qatMenu(){
    return [{ h:'Thêm vào thanh truy cập nhanh' }, { label:'Mở', check:true }, { label:'In', check:true }, { label:'Xem trước bản in', check:false, act:function(){} }, { label:'Bác sĩ Font', check:false, act:function(){} }, { label:'Khớp thuộc tính', check:false, need:'PRO' }, { label:'In hàng loạt', check:false, need:'VP' }, { sep:1 }, { label:'Hiện bên dưới ribbon', check:false }, { label:'Hiện thanh menu', check:false }];
  }
  function userMenu(){
    var ed = TC.EDITIONS[TC.S.edition];
    return [{ h:'Nguyễn Minh An · an.nguyen@congty.vn' }, { label:ed.name + (TC.S.edition === 'trial' ? ' · còn ' + TC.S.trialDays + ' ngày' : ''), icon:'license', act:function(){ TC.screens.license(); } }, { label:'Trạng thái giấy phép', icon:'key', act:function(){ TC.screens.license(); } }, { label:'Kích hoạt, nhập key…', icon:'key', act:function(){ TC.screens.activate(); } }, { label:'Quản lý tài khoản', icon:'ext', act:function(){ TC.openWeb('thanh-toan.html#tai-khoan'); } }, { sep:1 }, { label:'Đăng xuất', icon:'exit', act:function(){ TC.screens.signOut(); } }];
  }
  function helpMenu(){
    return [{ label:'Trợ giúp TrionCAD', icon:'book', kbd:'F1', act:function(){ TC.openHelp(); } }, { label:'Chuyển từ AutoCAD sang', icon:'play', act:function(){ TC.openHelp('migrate'); } }, { label:'Bảng phím tắt', icon:'keyboard', act:function(){ TC.openHelp('keys'); } }, { sep:1 }, { label:'Liên hệ hỗ trợ', icon:'chat', act:function(){ TC.screens.support(); } }, { label:'Gửi bản vẽ lỗi…', icon:'send', cmd:'TCREPORT', act:function(){ TC.cmd.run('TCREPORT'); } }, { label:'Góp ý', icon:'thumb', act:function(){ TC.screens.support('feedback'); } }, { sep:1 }, { label:'Kiểm tra cập nhật', icon:'update', act:function(){ TC.screens.update(); } }, { label:'Giới thiệu TrionCAD', icon:'info', act:function(){ TC.screens.about(); } }];
  }

  /* ================= ribbon ================= */
  // Dữ liệu ribbon: tab › panel › điều khiển. need = phiên bản tối thiểu; menu = split button.
  A.ws = TC.load('ws', '2d');
  function ribbonData(){
    if (TC.isViewer()) return TC.RIBBON_VIEWER;
    if (A.ws === 'viewer') return { tabs:TC.RIBBON_VIEWER.tabs };
    return TC.RIBBON_PRO;
  }
  A.setWs = function(ws){ A.ws = ws; TC.save('ws', ws); rbState.tab = 0; renderRibbon(); setTimeout(function(){ A.view.resize(); }, 0); TC.cmdline.msg('&lt;Workspace: ' + ({ '2d':'Soạn thảo 2D', classic:'Cổ điển', viewer:'Viewer' }[ws]) + '&gt;'); };
  var rbState = { tab:0, min:0 };
  function ctlHTML(c){
    var lock = c.need && !TC.can(c.need), lockB = lock ? '<span class="lkb" data-ed="' + (c.need === 'VP' ? 'vp' : 'pro') + '">' + ic('lock') + (c.need === 'VP' ? 'VP' : 'Pro') + '</span>' : '';
    var attrs = ' type="button" data-ctl="' + esc(c.id || c.cmd || c.label) + '" data-tip="' + esc((c.label || '').replace(/\n/g, ' ')) + '"' + (c.cmd ? ' data-cmd="' + esc(c.cmd) + '"' : '') + (c.key ? ' data-key="' + esc(c.key) + '"' : '') + (c.desc ? ' data-desc="' + esc(c.desc) + '"' : '') + (c.need ? ' data-need="' + c.need + '"' : '') + (c.toggle ? ' aria-pressed="' + !!c.on() + '"' : '') + (c.menu ? ' aria-haspopup="menu"' : '');
    if (c.t === 'lg') return '<button class="rb-lg' + (lock ? ' locked' : '') + '"' + attrs + '>' + ic(c.icon, 'lg') + '<span>' + esc(c.label).replace(/\n/g, '<br>') + (c.menu ? ic('down', 'xs') : '') + '</span>' + lockB + '</button>';
    if (c.t === 's' || c.t === 'io') return '<button class="rb-s' + (c.t === 'io' ? ' io' : '') + (lock ? ' locked' : '') + '"' + attrs + (c.t === 'io' ? ' aria-label="' + esc(c.label) + '"' : '') + '>' + ic(c.icon) + (c.sw ? '<i class="sw" style="background:' + c.sw + '"></i>' : '') + (c.t === 's' ? '<span class="t">' + esc(c.label) + '</span>' : '') + (c.menu ? ic('down', 'xs') : '') + lockB + '</button>';
    if (c.t === 'dd') return '<button class="rb-dd"' + attrs + ' style="width:' + (c.w || 160) + 'px" aria-label="' + esc(c.label) + '">' + (c.html ? c.html() : '') + '<span class="grow">' + esc(c.text ? c.text() : c.label) + '</span>' + ic('down', 'xs') + '</button>';
    if (c.t === 'num') return '<label class="rb-num" data-tip="' + esc(c.label) + '"' + (c.desc ? ' data-desc="' + esc(c.desc) + '"' : '') + '><span>' + esc(c.label) + '</span><input class="mono" data-numctl="' + esc(c.id) + '" value="' + esc(c.get()) + '" style="width:' + (c.w || 60) + 'px" aria-label="' + esc(c.label) + '" spellcheck="false"></label>';
    if (c.t === 'gal'){ var gi = c.cur(); return '<div class="rb-gal" data-galctl="' + esc(c.id) + '" style="width:' + (c.w || 280) + 'px"><div class="rb-gal-in" role="listbox" aria-label="' + esc(c.label) + '">' + c.items.map(function(it, i){ return '<button type="button" class="rb-gi' + (i === gi ? ' on' : '') + '" data-gi="' + i + '" role="option" aria-selected="' + (i === gi) + '" data-tip="' + esc(c.tip ? c.tip(it) : it[0]) + '"><canvas width="44" height="30"></canvas></button>'; }).join('') + '</div><span class="rb-gal-nav"><button type="button" data-galnav="-1" aria-label="Hàng trước">' + ic('up', 'xs') + '</button><button type="button" data-galnav="1" aria-label="Hàng sau">' + ic('down', 'xs') + '</button><button type="button" data-galmore aria-label="Mở toàn bộ gallery">' + ic('more', 'xs') + '</button></span></div>'; }
    if (c.t === 'col') return '<div class="rb-col"' + (c.gap ? ' style="gap:' + c.gap + 'px"' : '') + '>' + c.items.map(ctlHTML).join('') + '</div>';
    if (c.t === 'row') return '<div class="rb-row">' + c.items.map(ctlHTML).join('') + '</div>';
    return '';
  }
  var ctlIndex = {};
  function indexCtls(list){ list.forEach(function(c){ if (c.items && c.t !== 'gal') indexCtls(c.items); else ctlIndex[c.id || c.cmd || c.label] = c; }); }
  function renderRibbon(){
    if (A.ws === 'classic' && !TC.isViewer() && A.active !== 'start') return renderClassic();
    $$('.vtb').forEach(function(x){ x.remove(); }); $('#app').classList.remove('ws-classic');
    var data = ribbonData(), rb = $('#rb'); ctlIndex = {};
    var tabs = data.tabs.concat(A.ctxTab ? [A.ctxTab] : []);
    if (rbState.tab >= tabs.length) rbState.tab = 0;
    var tab = tabs[rbState.tab];
    rb.className = 'rb' + (rbState.min === 1 ? ' min-panels' : rbState.min === 2 ? ' min-tabs' : '');
    rb.innerHTML = '<div class="rb-tabs" role="tablist">' + tabs.map(function(t, i){ var lock = t.need && !TC.can(t.need); return '<button class="rb-tab' + (t.ctx ? ' ctx' : '') + '" role="tab" type="button" aria-selected="' + (i === rbState.tab) + '" data-tab="' + i + '">' + esc(t.name) + (lock ? '<span class="ed xs" data-ed="vp">VP</span>' : '') + '</button>'; }).join('') +
      '<span class="rb-tabs-r"><button class="ib" type="button" id="rb-min" aria-label="Thu gọn ribbon" data-tip="Thu gọn ribbon" data-desc="Đầy đủ → chỉ tên panel → chỉ tên tab.">' + ic(rbState.min === 2 ? 'down' : 'up', 'xs') + '</button></span></div>' +
      '<div class="rb-body" id="rb-body">' + tab.panels.map(function(p){
        indexCtls(p.items);
        return '<div class="rb-p" data-prio="' + (p.prio || 1) + '"><button class="rb-coll" type="button" aria-haspopup="true">' + ic(p.icon || 'more', 'lg') + '<span>' + esc(p.name) + ic('down', 'xs') + '</span></button>' +
          '<div class="rb-c">' + p.items.map(ctlHTML).join('') + '</div><div class="rb-l">' + (p.more ? '<button type="button" data-more="' + esc(p.name) + '" data-tip="Mở rộng panel ' + esc(p.name) + '">' + esc(p.name) + ic('down', 'xs') + '</button>' : esc(p.name)) + '</div></div>';
      }).join('') + (data.upsell ? '<div class="rb-up"><b>Sửa bản vẽ?</b><span>Dùng thử TrionCAD Pro 30 ngày, không cần thẻ.</span><button class="btn sm primary" type="button" data-trial>Dùng thử Pro 30 ngày</button></div>' : '') + '</div>';
    paintGal(rb);
    fitRibbon();
  }
  function paintGal(root){ $$('[data-galctl]', root).forEach(function(g){ var c = ctlIndex[g.dataset.galctl]; if (!c) return; $$('[data-gi]', g).forEach(function(b){ c.paint($('canvas', b), c.items[Number(b.dataset.gi)]); }); var on = $('.rb-gi.on', g), inn = $('.rb-gal-in', g); if (on) inn.scrollTop = Math.max(0, on.offsetTop - inn.offsetTop - 2); }); }
  // workspace Cổ điển: thanh menu và toolbar thay cho ribbon (07.1 §3.1)
  var TBON = TC.load('tbars', { Standard:1, Styles:1, Layers:1, Properties:1, Draw:1, Modify:1 });
  function tbHTML(t){ var body = '';
    if (Array.isArray(t[2])) body = t[2].map(function(b){ if (b === '|') return '<span class="sep"></span>'; var c = TC.CMDS[b[1]]; return '<button class="ib" type="button" data-tbc="' + b[1] + '" data-tbp="' + esc(b[2] || '') + '" data-tip="' + esc(c ? c.desc : b[1]) + '" data-cmd="' + b[1] + '"' + (c && c.need !== 'V' && !TC.can(c.need) ? ' data-need="' + c.need + '"' : '') + '>' + ic(b[0]) + '</button>'; }).join('');
    else { var K = TC.CLASSIC_CTL, list = t[2] === 'styles' ? [K.tstyle, K.dstyle, K.bstyle, K.mstyle] : t[2] === 'layers' ? [K.layer] : [K.color, K.lt, K.lw]; indexCtls(list); body = list.map(ctlHTML).join('') + (t[2] === 'layers' ? '<button class="ib" type="button" data-tbc="LAYMCUR" data-tip="Layer của đối tượng thành hiện hành">' + ic('laycur') + '</button><button class="ib" type="button" data-tbc="LAYERP" data-tip="Trạng thái layer trước">' + ic('undo') + '</button><button class="ib" type="button" data-tbc="LAYER" data-tip="Quản lý layer">' + ic('layers') + '</button>' : ''); }
    return '<div class="tbar' + (t[1] === 'L' || t[1] === 'R' ? ' v' : '') + '" role="toolbar" aria-label="Toolbar ' + esc(t[0]) + '" data-tbn="' + esc(t[0]) + '"><span class="tb-grip" aria-hidden="true"></span>' + body + '</div>'; }
  function renderClassic(){
    var rb = $('#rb'); ctlIndex = {}; rb.className = 'rb classic'; $('#app').classList.add('ws-classic');
    rb.innerHTML = '<div class="mbar" role="menubar" aria-label="Thanh menu">' + TC.CLASSIC_MENUS.map(function(m, i){ return '<button type="button" role="menuitem" aria-haspopup="menu" data-mb="' + i + '">' + esc(m[0]) + '</button>'; }).join('') + '<span class="grow"></span><span class="faint mbar-ws">Workspace: Cổ điển</span></div><div class="tbars">' + TC.CLASSIC_TB.filter(function(t){ return TBON[t[0]] && t[1] !== 'L' && t[1] !== 'R'; }).map(tbHTML).join('') + '</div>';
    $$('.vtb').forEach(function(x){ x.remove(); });
    TC.CLASSIC_TB.filter(function(t){ return TBON[t[0]] && (t[1] === 'L' || t[1] === 'R'); }).forEach(function(t){ var el = TC.el('<div class="vtb ' + (t[1] === 'L' ? 'l' : 'r') + '">' + tbHTML(t) + '</div>'); el.addEventListener('mousedown', function(e){ e.stopPropagation(); }); el.addEventListener('click', tbClick); el.addEventListener('contextmenu', tbMenu); $('#cv').appendChild(el); });
  }
  function tbClick(e){ var b = e.target.closest('[data-tbc]'); if (!b) return; if (b.dataset.need){ TC.screens.upgrade(b.dataset.tip, b.dataset.need); return; } if (b.dataset.tbc === 'LAYER') TC.screens.layerManager(); else TC.cmd.run(b.dataset.tbc, b.dataset.tbp || null); }
  function tbMenu(e){ e.preventDefault(); TC.menu([{ h:'Toolbar' }].concat(TC.CLASSIC_TB.map(function(t){ return { label:esc(t[0]), check:!!TBON[t[0]], act:function(){ TBON[t[0]] = TBON[t[0]] ? 0 : 1; TC.save('tbars', TBON); renderRibbon(); setTimeout(function(){ A.view.resize(); }, 0); } }; })).concat([{ sep:1 }, { label:'Khóa vị trí toolbar', check:true }, { label:'Chuyển về Soạn thảo 2D (ribbon)', act:function(){ A.setWs('2d'); } }]), e.clientX, e.clientY); }
  document.addEventListener('click', function(e){ var m = e.target.closest('[data-mb]'); if (m){ TC.classicMenu(Number(m.dataset.mb), m); return; } if (e.target.closest('.tbars')) tbClick(e); });
  document.addEventListener('contextmenu', function(e){ if (e.target.closest('.tbars, .mbar')) tbMenu(e); });
  // ô số và gallery trên ribbon
  document.addEventListener('change', function(e){ var n = e.target.closest && e.target.closest('[data-numctl]'); if (n && ctlIndex[n.dataset.numctl]) ctlIndex[n.dataset.numctl].set(n.value.trim()); });
  document.addEventListener('keydown', function(e){ var n = e.target.closest && e.target.closest('[data-numctl]'); if (!n) return; if (e.key === 'Enter'){ e.preventDefault(); n.blur(); } else if (e.key === 'Escape'){ e.preventDefault(); n.value = ctlIndex[n.dataset.numctl].get(); n.blur(); } else if (e.key === 'ArrowUp' || e.key === 'ArrowDown'){ e.preventDefault(); var v = parseFloat(n.value) || 0, st = Math.abs(v) >= 100 ? 10 : 1; n.value = String(Math.round((v + (e.key === 'ArrowUp' ? st : -st)) * 100) / 100); ctlIndex[n.dataset.numctl].set(n.value); } });
  document.addEventListener('mouseover', function(e){ var b = e.target.closest && e.target.closest('[data-gi]'); if (!b) return; var g = b.closest('[data-galctl]'), c = g && ctlIndex[g.dataset.galctl]; if (c && c.hover) c.hover(c.items[Number(b.dataset.gi)]); });
  document.addEventListener('mouseout', function(e){ var g = e.target.closest && e.target.closest('[data-galctl]'); if (!g || g.contains(e.relatedTarget)) return; var c = ctlIndex[g.dataset.galctl]; if (c && c.hover) c.hover(null); });
  document.addEventListener('click', function(e){ var b = e.target.closest('[data-gi]'), g = e.target.closest('[data-galctl]'); if (!g) return; var c = ctlIndex[g.dataset.galctl]; if (!c) return; if (b){ c.pick(c.items[Number(b.dataset.gi)]); return; } var nv = e.target.closest('[data-galnav]'); if (nv){ var inn = $('.rb-gal-in', g); inn.scrollTop += Number(nv.dataset.galnav) * 32; return; } if (e.target.closest('[data-galmore]') && c.more) c.more(e.target.closest('[data-galmore]')); });
  A.renderRibbon = renderRibbon;
  // tab ngữ cảnh (07.1 §7.11): hiện sau các tab thường, tự chọn khi mới mở
  A.ctxTab = null;
  A.setCtxTab = function(t, keep){ var n = ribbonData().tabs.length; if (t && !(keep && A.ctxTab)) rbState.tab = n; A.ctxTab = t; if (!t && rbState.tab >= n) rbState.tab = 0; if (A.active !== 'start') renderRibbon(); };
  function fitRibbon(){
    var app = $('#app'); if (app) app.classList.toggle('narrow', app.offsetWidth < 1600);
    var body = $('#rb-body'); if (!body) return;
    var panels = $$('.rb-p', body); function over(){ return body.scrollWidth > body.clientWidth + 1; }
    panels.forEach(function(p){ p.classList.remove('is-collapsed', 'is-compact'); });
    var order = panels.slice().sort(function(a, b){ return (b.dataset.prio - a.dataset.prio) || (panels.indexOf(b) - panels.indexOf(a)); });
    order.forEach(function(p){ if (over() && p.dataset.prio > 1) p.classList.add('is-collapsed'); });
    panels.slice().reverse().forEach(function(p){ if (over() && !p.classList.contains('is-collapsed')) p.classList.add('is-compact'); });
    panels.slice().reverse().forEach(function(p){ if (over()) p.classList.add('is-collapsed'); });
  }
  A.fitRibbon = fitRibbon;
  document.addEventListener('click', function(e){
    var t = e.target.closest('.rb-tab'); if (t){ rbState.tab = Number(t.dataset.tab); if (rbState.min === 2) rbState.min = 0; renderRibbon(); return; }
    if (e.target.closest('#rb-min')){ rbState.min = (rbState.min + 1) % 3; renderRibbon(); return; }
    if (e.target.closest('[data-trial]')){ TC.screens.upgrade('Sửa bản vẽ', 'PRO'); return; }
    var coll = e.target.closest('.rb-coll');
    if (coll){ var p = coll.closest('.rb-p'), r = coll.getBoundingClientRect(); var items = []; $$('.rb-c [data-ctl]', p).forEach(function(b){ var c = ctlIndex[b.dataset.ctl]; if (c) items.push({ label:(c.label || '').replace(/\n/g, ' '), icon:c.icon, need:c.need, cmd:c.cmd, act:function(){ ctlRun(c, b); } }); }); TC.menu(items, r.left, r.bottom + 2, { returnFocus:coll }); return; }
    var mo = e.target.closest('[data-more]'); if (mo){ var r2 = mo.getBoundingClientRect(); TC.menu(TC.panelMore(mo.dataset.more), r2.left, r2.bottom + 2, { returnFocus:mo }); return; }
    var b = e.target.closest('[data-ctl]'); if (b && b.closest('#rb')){ var c = ctlIndex[b.dataset.ctl]; if (c) ctlRun(c, b, e); }
  });
  function ctlRun(c, b, e){
    if (c.need && !TC.can(c.need)) { TC.screens.upgrade(c.feature || (c.label || '').replace(/\n/g, ' '), c.need); return; }
    if (c.menu){
      var r = b.getBoundingClientRect(), main = c.t === 'lg' ? (e && e.clientY < r.top + r.height * 0.55) : (e && e.clientX < r.left + 22);
      if (main && c.cmd && !c.menuOnly) { TC.cmd.run(c.cmd); return; }
      TC.menu(typeof c.menu === 'function' ? c.menu() : c.menu, r.left, r.bottom + 2, { returnFocus:b }); return;
    }
    if (c.act) { c.act(b); return; }
    if (c.cmd) TC.cmd.run(c.cmd);
  }
  TC.ctlRun = ctlRun;

  /* ================= tab tệp ================= */
  A.renderFileTabs = function(){
    var ft = $('#ft'), viewer = TC.isViewer();
    ft.innerHTML = '<button class="ft-tab" role="tab" type="button" data-doc="start" aria-selected="' + (A.active === 'start') + '">' + ic('home', 's12') + '<span class="nm">Start</span></button>' +
      A.docs.filter(function(d){ return d.open; }).map(function(d){
        return '<button class="ft-tab" role="tab" type="button" data-doc="' + d.id + '" aria-selected="' + (A.active === d.id) + '" title="' + esc(d.path) + '">' + (viewer || d.readonly ? ic('lock', 'ro') : '') + '<span class="nm">' + esc(d.name) + '</span>' + (d.dirty ? '<i class="ft-dot" aria-label="chưa lưu"></i>' : '') + '<span class="x" role="button" aria-label="Đóng ' + esc(d.name) + '" data-close="' + d.id + '">' + ic('x', 's12') + '</span></button>';
      }).join('') + '<button class="ft-add" type="button" aria-label="' + (viewer ? 'Mở bản vẽ' : 'Bản vẽ mới') + '" data-tip="' + (viewer ? 'Mở bản vẽ' : 'Bản vẽ mới') + '" data-run="' + (viewer ? 'OPEN' : 'NEW') + '">' + ic('plus') + '</button>';
  };
  document.addEventListener('click', function(e){
    var x = e.target.closest('[data-close]'); if (x){ e.stopPropagation(); A.closeDoc(x.dataset.close); return; }
    var t = e.target.closest('.ft-tab'); if (t){ A.activate(t.dataset.doc); }
    var r = e.target.closest('[data-run]'); if (r && !r.closest('.rb')) TC.cmd.run(r.dataset.run);
  });
  document.addEventListener('auxclick', function(e){ var t = e.target.closest('.ft-tab'); if (t && e.button === 1 && t.dataset.doc !== 'start') A.closeDoc(t.dataset.doc); });
  document.addEventListener('contextmenu', function(e){
    var t = e.target.closest('.ft-tab'); if (!t) return; e.preventDefault();
    var id = t.dataset.doc, d = A.docs.filter(function(x){ return x.id === id; })[0];
    var closeMany = function(ids){ if (A.closeDocs) A.closeDocs(ids); else ids.forEach(function(x){ A.closeDoc(x, true); }); };
    TC.menu([{ label:'Mới', icon:'new', need:'PRO', cmd:'NEW', act:function(){ TC.cmd.run('NEW'); } }, { label:'Mở…', icon:'open', kbd:'Ctrl+O', act:function(){ TC.cmd.run('OPEN'); } }, { label:'Lưu', icon:'save', need:'PRO', dis:!d, act:function(){ A.activate(id); TC.cmd.run('QSAVE'); } }, { label:'Lưu tất cả', need:'PRO', act:function(){ var n = 0; A.docs.forEach(function(x){ if (x.open && x.dirty){ x.dirty = false; x.savedOnce = true; n++; } }); renderTitle(); A.renderFileTabs(); TC.toast(n ? 'Đã lưu ' + n + ' bản vẽ.' : 'Không có bản vẽ nào cần lưu.', { log:false }); } }, { sep:1 },
      { label:'Đóng', icon:'fileclose', kbd:'Ctrl+F4', dis:!d, act:function(){ A.closeDoc(id); } }, { label:'Đóng tất cả', act:function(){ closeMany(A.docs.filter(function(x){ return x.open; }).map(function(x){ return x.id; })); } }, { label:'Đóng các tab khác', dis:!d, act:function(){ closeMany(A.docs.filter(function(x){ return x.open && x.id !== id; }).map(function(x){ return x.id; })); } }, { sep:1 },
      { label:'Sao chép đường dẫn', icon:'copyclip', dis:!d, act:function(){ copyText(d.path); } }, { label:'Mở thư mục chứa', icon:'folder', dis:!d, act:function(){ TC.toast('Mở thư mục ' + esc(d.path.replace(/\\[^\\]+$/, '')) + ' trong File Explorer.', { type:'info', log:false }); } }, { label:'Gửi bản vẽ lỗi…', icon:'send', dis:!d, act:function(){ TC.cmd.run('TCREPORT'); } }], e.clientX, e.clientY);
  });
  // rê lên tab tệp: thumbnail Model và các layout (07.1 §5.6)
  var thumbT = 0, thumbEl = null;
  document.addEventListener('mouseover', function(e){
    var t = e.target.closest && e.target.closest('.ft-tab'); if (!t || t.dataset.doc === 'start' || (thumbEl && thumbEl._for === t)) return;
    clearTimeout(thumbT); thumbT = setTimeout(function(){
      if (thumbEl) thumbEl.remove();
      var d = A.docs.filter(function(x){ return x.id === t.dataset.doc; })[0]; if (!d) return;
      var r = t.getBoundingClientRect();
      thumbEl = TC.el('<div class="ft-thumb" role="menu">' + ['model'].concat(d.layouts.map(function(l, i){ return i; })).map(function(sp){ return '<button type="button" data-sp="' + sp + '"><canvas width="300" height="192"></canvas>' + (sp === 'model' ? 'Model' : esc(d.layouts[sp].name)) + '</button>'; }).join('') + '</div>');
      thumbEl._for = t; document.body.appendChild(thumbEl);
      thumbEl.style.left = Math.min(r.left, innerWidth - thumbEl.offsetWidth - 8) + 'px'; thumbEl.style.top = (r.bottom + 4) + 'px';
      $$('canvas', thumbEl).forEach(function(c, i){ TC.thumb(c, d, i === 0 ? 'model' : i - 1); });
      thumbEl.addEventListener('click', function(ev){ var b = ev.target.closest('[data-sp]'); if (!b) return; A.activate(d.id); setSpace(b.dataset.sp === 'model' ? 'model' : Number(b.dataset.sp)); thumbEl.remove(); thumbEl = null; });
      thumbEl.addEventListener('mouseleave', function(){ if (thumbEl) { thumbEl.remove(); thumbEl = null; } });
    }, 650);
  });
  document.addEventListener('mousedown', function(e){ clearTimeout(thumbT); if (thumbEl && !thumbEl.contains(e.target)) { thumbEl.remove(); thumbEl = null; } }, true);
  document.addEventListener('mouseout', function(e){ var t = e.target.closest && e.target.closest('.ft-tab'); if (t && !t.contains(e.relatedTarget)) { clearTimeout(thumbT); setTimeout(function(){ if (thumbEl && !thumbEl.matches(':hover')) { thumbEl.remove(); thumbEl = null; } }, 200); } });
  function copyText(s){ try { navigator.clipboard.writeText(s).then(function(){ TC.toast('Đã sao chép: <span class="mono">' + esc(s) + '</span>', { log:false }); }, function(){ TC.toast('Không sao chép được. Chọn và sao chép thủ công: <span class="mono">' + esc(s) + '</span>', { type:'warn', log:false }); }); } catch (e) { TC.toast(esc(s), { log:false }); } }
  TC.copyText = copyText;
  // trang web của TrionCAD (thanh toán, tài khoản): bản thật mở trình duyệt mặc định
  TC.openWeb = function(path){ var w = null; try { w = window.open(path, '_blank'); } catch (e) {} if (!w) location.href = path; };

  // thumbnail bản vẽ (tab Start, tab tệp, hộp thoại mở)
  TC.thumb = function(c, d, sp){
    var ctx = c.getContext('2d'), W = c.width, H = c.height;
    var bg = sp === 'model' ? TC.cssv('--c-canvas') : TC.cssv('--c-layout-bg');
    ctx.fillStyle = bg; ctx.fillRect(0, 0, W, H);
    var fake = { W:W, H:H, doc:d, dpr:1 }; fake.__proto__ = TC.View.prototype;
    if (sp === 'model'){
      var z = d.initView || { c:[(d.ext[0] + d.ext[2]) / 2, (d.ext[1] + d.ext[3]) / 2], h:d.ext[3] - d.ext[1] };
      var v = { cx:z.c[0], cy:z.c[1], s:H / z.h };
      fake.drawModel(ctx, v, false, null);
    } else {
      var L = d.layouts[sp], s = Math.min((W - 16) / L.paper.w, (H - 16) / L.paper.h), pv = { cx:L.paper.w / 2, cy:L.paper.h / 2, s:s };
      var a = fake.toS([0, L.paper.h], pv), b = fake.toS([L.paper.w, 0], pv);
      ctx.fillStyle = '#FFFFFF'; ctx.fillRect(a[0], a[1], b[0] - a[0], b[1] - a[1]);
      L.vports.forEach(function(vp){
        var r = fake.vpRect(vp, pv), k = s / vp.scale, cx = W / 2 + (vp.x + vp.w / 2 - pv.cx) * s, cy = H / 2 - (vp.y + vp.h / 2 - pv.cy) * s;
        var mv = { cx:vp.c[0] - (cx - W / 2) / k, cy:vp.c[1] + (cy - H / 2) / k, s:k };
        ctx.save(); ctx.beginPath(); ctx.rect(r[0], r[1], r[2], r[3]); ctx.clip(); fake.drawModel(ctx, mv, true, r, vp); ctx.restore();
      });
      L.ents.forEach(function(e){ fake.drawPrims(ctx, e, pv, true); });
    }
  };

  /* ================= bản vẽ: mở, kích hoạt, đóng ================= */
  A.openDoc = function(id, silent){ var d = A.docs.filter(function(x){ return x.id === id; })[0]; if (!d) return; d.open = true; if (!silent) A.activate(id); else A.renderFileTabs(); };
  A.activate = function(id){
    TC.closeMenus(0);
    if (TC.cmdline && TC.cmdline.busy() && id !== A.active) TC.cmd.cancel();
    A.active = id;
    var start = id === 'start', d = A.doc();
    $('#app').classList.toggle('on-start', start);
    $('#start').hidden = !start; $('#rb').hidden = start; $('#lt').hidden = start; $('#cmd').hidden = start;
    ['#cv', '#dock-l', '#dock-r'].forEach(function(s){ $(s).hidden = start; });
    A.renderFileTabs(); renderTitle();
    if (start){ TC.screens.start(); renderStatus(); return; }
    renderRibbon(); renderDocks(); renderLayoutTabs();
    A.view.setDoc(d); renderBanners(); renderStatus();
    TC.emit('doc', d); TC.emit('sel');
    if (!d.welcomed){ d.welcomed = true; TC.fontDoctor && TC.fontDoctor.onOpen(d); TC.missingRefs && TC.missingRefs.onOpen(d); }
  };
  A.closeDoc = function(id, quiet){
    var d = A.docs.filter(function(x){ return x.id === id; })[0]; if (!d) return;
    d.open = false; d.welcomed = false; d.view = null; d.sel = []; d.banners = [];
    var openDocs = A.docs.filter(function(x){ return x.open; });
    if (A.active === id) A.activate(openDocs.length ? openDocs[openDocs.length - 1].id : 'start'); else A.renderFileTabs();
  };

  /* ================= banner trên vùng vẽ ================= */
  A.banner = function(d, b){ d.banners = d.banners.filter(function(x){ return x.id !== b.id; }); d.banners.push(b); if (d === A.doc()) renderBanners(); };
  A.unbanner = function(d, id){ d.banners = d.banners.filter(function(x){ return x.id !== id; }); if (d === A.doc()) renderBanners(); };
  function renderBanners(){
    var d = A.doc(), host = $('#banners'); if (!d) return;
    host.innerHTML = d.banners.map(function(b){ return '<div class="banner ' + (b.type || '') + '" role="status" data-b="' + b.id + '">' + ic(b.icon || 'info') + '<p>' + b.html + '</p><span class="acts">' + (b.acts || []).map(function(a, i){ return '<button class="btn sm' + (a.primary ? ' primary' : a.ghost ? ' ghost' : '') + '" type="button" data-ba="' + i + '">' + a.label + (a.need && !TC.can(a.need) ? ' <span class="ed xs" data-ed="' + (a.need === 'VP' ? 'vp' : 'pro') + '">' + (a.need === 'VP' ? 'VP' : 'Pro') + '</span>' : '') + '</button>'; }).join('') + '</span><button class="ib" type="button" aria-label="Đóng banner" data-bx>' + ic('x') + '</button></div>'; }).join('');
    $('#vpc').style.top = (host.offsetHeight + 6) + 'px';
    TC.emit('banners');
  }
  A.renderBanners = renderBanners;
  document.addEventListener('click', function(e){
    var bn = e.target.closest('.banner'); if (!bn) return; var d = A.doc(); var b = d && d.banners.filter(function(x){ return x.id === bn.dataset.b; })[0]; if (!b) return;
    if (e.target.closest('[data-bx]')) { A.unbanner(d, b.id); if (b.onClose) b.onClose(); return; }
    var a = e.target.closest('[data-ba]'); if (a){ var act = b.acts[Number(a.dataset.ba)]; if (act.need && !TC.can(act.need)) TC.screens.upgrade(act.feature || act.label, act.need); else act.fn(); }
  });

  /* ================= dock và palette ================= */
  A.pal = TC.S.pal;
  function renderDocks(){
    var viewer = TC.isViewer();
    var right = A.pal.right.filter(function(p){ return TC.PALETTES[p] && (!TC.PALETTES[p].hidden || !TC.PALETTES[p].hidden()); });
    var left = viewer ? [] : A.pal.left;
    var auto = ($('#app').offsetWidth || innerWidth) < 1600;
    dock($('#dock-r'), right, 'right', A.pal.rightPinned && !auto);
    dock($('#dock-l'), left, 'left', A.pal.leftPinned && !auto);
  }
  A.renderDocks = renderDocks;
  function dock(el, list, side, pinned){
    el.classList.toggle('empty-dock', !list.length);
    el.classList.toggle('flyout', false);
    if (!list.length){ el.innerHTML = ''; return; }
    var key = side === 'right' ? 'rightActive' : 'leftActive';
    var act = A.pal[key]; if (list.indexOf(act) < 0) act = A.pal[key] = list[0];
    var strip = '<div class="strip" role="tablist" aria-label="Palette tự ẩn">' + list.map(function(p){ return '<button class="vtab" type="button" data-fly="' + p + '" data-side="' + side + '" aria-expanded="false">' + esc(TC.PALETTES[p].name) + '</button>'; }).join('') + '</div>';
    var host = '<div class="palhost"><div class="pal-tabs" role="tablist">' + list.map(function(p){ return '<button class="pal-tab" role="tab" type="button" aria-selected="' + (p === act) + '" data-palt="' + p + '" data-side="' + side + '">' + esc(TC.PALETTES[p].name) + '</button>'; }).join('') +
      '<span class="acts"><button class="ib" type="button" data-palmore="' + side + '" hidden data-tip="Palette khác" style="min-width:36px;gap:1px"><span class="pm-n mono" style="font-size:var(--fs-xs)">+0</span>' + ic('down', 'xs') + '</button><button class="ib" type="button" data-pin="' + side + '" aria-label="' + (pinned ? 'Tự ẩn palette' : 'Ghim palette') + '" data-tip="' + (pinned ? 'Tự ẩn' : 'Ghim') + '" aria-pressed="' + pinned + '">' + ic('pin') + '</button><button class="ib" type="button" data-palmenu="' + side + '" aria-label="Tùy chọn palette">' + ic('more') + '</button></span></div><div class="pal" id="pal-' + side + '"></div></div>';
    el.innerHTML = side === 'right' ? (pinned ? host : host + strip) : (pinned ? host : strip + host);
    el.dataset.pinned = pinned;
    if (!pinned) { el.classList.add('flyout'); $('.palhost', el).hidden = true; }
    TC.PALETTES[act].render($('#pal-' + side), A.doc());
    fitTabs(el);
  }
  // tab palette không đủ chỗ: gom các tab không đang mở vào nút » (bên phải)
  function fitTabs(el){
    var bar = $('.pal-tabs', el); if (!bar || !bar.offsetWidth) return;
    var more = $('[data-palmore]', bar), tabs = $$('.pal-tab', bar), hid = [];
    for (var i = tabs.length - 1; i >= 0 && bar.scrollWidth > bar.clientWidth + 1; i--){ if (tabs[i].getAttribute('aria-selected') === 'true') continue; if (more.hidden){ more.hidden = false; i++; continue; } tabs[i].hidden = true; hid.unshift(tabs[i].dataset.palt); }
    more.hidden = !hid.length; more.dataset.list = hid.join(','); $('.pm-n', more).textContent = '+' + hid.length; more.setAttribute('aria-label', 'Palette khác: ' + hid.map(function(p){ return TC.PALETTES[p].name; }).join(', '));
  }
  A.showPalette = function(id, toggle){
    var side = A.pal.left.indexOf(id) >= 0 ? 'left' : 'right';
    if (A.pal[side].indexOf(id) < 0) A.pal[side].push(id);
    var el = $('#dock-' + (side === 'right' ? 'r' : 'l'));
    var shown = el.dataset.pinned === 'true' || !$('.palhost', el) || !$('.palhost', el).hidden;
    if (toggle && A.pal[side + 'Active'] === id && shown && el.dataset.pinned === 'true'){ A.pal[side + 'Pinned'] = false; renderDocks(); TC.save('pal', A.pal); return; }
    A.pal[side + 'Active'] = id; if (toggle !== 'fly') A.pal[side + 'Pinned'] = true;
    renderDocks(); TC.save('pal', A.pal);
  };
  A.refreshPalettes = function(){ ['right', 'left'].forEach(function(side){ var host = $('#pal-' + side); if (!host || (host.closest('.palhost') && host.closest('.palhost').hidden)) return; var id = A.pal[side + 'Active']; if (TC.PALETTES[id]) TC.PALETTES[id].render(host, A.doc()); }); };
  document.addEventListener('click', function(e){
    var t = e.target.closest('[data-palt]'); if (t){ A.pal[t.dataset.side + 'Active'] = t.dataset.palt; renderDocks(); keepFly(t.dataset.side); return; }
    var p = e.target.closest('[data-pin]'); if (p){ var s = p.dataset.pin; A.pal[s + 'Pinned'] = !(p.getAttribute('aria-pressed') === 'true'); renderDocks(); TC.save('pal', A.pal); return; }
    var f = e.target.closest('[data-fly]'); if (f){ var side = f.dataset.side, el = $('#dock-' + (side === 'right' ? 'r' : 'l')); A.pal[side + 'Active'] = f.dataset.fly; renderDocks(); el = $('#dock-' + (side === 'right' ? 'r' : 'l')); var h = $('.palhost', el); h.hidden = false; $$('.vtab', el).forEach(function(v){ v.setAttribute('aria-expanded', String(v.dataset.fly === f.dataset.fly)); }); return; }
    var mo = e.target.closest('[data-palmore]'); if (mo){ var rr = mo.getBoundingClientRect(), sd2 = mo.dataset.palmore; TC.menu(mo.dataset.list.split(',').map(function(id){ return { label:esc(TC.PALETTES[id].name), act:function(){ A.pal[sd2 + 'Active'] = id; renderDocks(); keepFly(sd2); } }; }), rr.right, rr.bottom + 2, { alignRight:true, returnFocus:mo }); return; }
    var pm = e.target.closest('[data-palmenu]'); if (pm){ var r = pm.getBoundingClientRect(), sd = pm.dataset.palmenu; TC.menu([{ label:'Ghim', check:A.pal[sd + 'Pinned'], act:function(){ A.pal[sd + 'Pinned'] = !A.pal[sd + 'Pinned']; renderDocks(); } }, { label:'Chuyển sang dock ' + (sd === 'right' ? 'trái' : 'phải'), dis:TC.isViewer(), act:function(){ var id = A.pal[sd + 'Active']; A.pal[sd] = A.pal[sd].filter(function(x){ return x !== id; }); var o = sd === 'right' ? 'left' : 'right'; A.pal[o].push(id); A.pal[o + 'Active'] = id; A.pal[o + 'Pinned'] = true; renderDocks(); } }, { label:'Nổi (kéo sang màn hình khác)', dis:true }, { sep:1 }, { label:'Mật độ thoáng', check:TC.S.density === 'comfortable', act:function(){ TC.S.density = TC.S.density === 'comfortable' ? 'compact' : 'comfortable'; TC.save('density', TC.S.density); TC.applyTheme(); A.fitRibbon(); } }], r.right, r.bottom + 2, { alignRight:true }); return; }
    // bấm ra ngoài palette tự ẩn thì thu lại
    $$('.dock.flyout .palhost').forEach(function(h){ if (!h.hidden && !h.contains(e.target) && !e.target.closest('.menu') && !e.target.closest('.dlg')) { h.hidden = true; $$('.vtab', h.parentNode).forEach(function(v){ v.setAttribute('aria-expanded', 'false'); }); } });
  });
  function keepFly(side){ var el = $('#dock-' + (side === 'right' ? 'r' : 'l')); if (el.classList.contains('flyout')) $('.palhost', el).hidden = false; }

  /* ================= tab layout ================= */
  function renderLayoutTabs(){
    var d = A.doc(); if (!d) return;
    $('#lt').innerHTML = '<button class="lt-tab" role="tab" type="button" aria-selected="' + (d.space === 'model') + '" data-sp="model">Model</button>' + d.layouts.map(function(l, i){ return '<button class="lt-tab" role="tab" type="button" aria-selected="' + (d.space === i) + '" data-sp="' + i + '">' + esc(l.name) + '</button>'; }).join('') +
      (TC.isViewer() ? '' : '<button class="lt-add" type="button" aria-label="Layout mới" data-tip="Layout mới">' + ic('plus') + '</button>');
  }
  A.renderLayoutTabs = renderLayoutTabs;
  function setSpace(sp){
    var d = A.doc(); if (!d || (sp !== 'model' && !d.layouts[sp])) return; // bản vẽ không có layout đó: giữ nguyên
    if (TC.cmdline.busy()) TC.cmd.cancel();
    d.space = sp; d.activeVp = null; d.sel = []; A.view.hover = null;
    renderLayoutTabs(); A.view.draw(); renderStatus(); TC.emit('sel'); TC.emit('space');
  }
  A.setSpace = setSpace;
  document.addEventListener('click', function(e){ var t = e.target.closest('.lt-tab'); if (t) setSpace(t.dataset.sp === 'model' ? 'model' : Number(t.dataset.sp)); if (e.target.closest('.lt-add')) TC.cmd.run('LAYOUT', 'N'); });
  document.addEventListener('contextmenu', function(e){
    var t = e.target.closest('.lt-tab, .lt'); if (!t) return; e.preventDefault(); var d = A.doc();
    if (!TC.isViewer() && TC.screens.layoutMenu){ var tb = e.target.closest('.lt-tab'); TC.screens.layoutMenu(e, tb ? (tb.dataset.sp === 'model' ? 'model' : Number(tb.dataset.sp)) : d.space); return; }
    TC.menu([{ label:'Layout mới', icon:'plus', need:'PRO' }, { label:'Từ mẫu…', need:'PRO' }, { label:'Xóa', need:'PRO' }, { label:'Đổi tên', need:'PRO' }, { label:'Di chuyển hoặc sao chép…', need:'PRO' }, { sep:1 }, { label:'Chọn tất cả layout', dis:true }, { label:'Kích hoạt layout trước', act:function(){ if (d.viewHist.length) A.view.zoomPrev(); } },
      { label:'Quản lý thiết lập trang…', icon:'print', act:function(){ TC.cmd.run('PAGESETUP'); } }, { label:'In…', icon:'print', kbd:'Ctrl+P', act:function(){ TC.cmd.run('PLOT'); } }, { label:'Xuất layout thành Model…', need:'PRO' }, { sep:1 }, { label:'Ẩn tab layout và Model', act:function(){ $('#lt').hidden = true; TC.toast('Đã ẩn tab layout. Bật lại ở Hiển thị › Giao diện.', { type:'info', log:false }); } }], e.clientX, e.clientY);
  });

  /* ================= thanh trạng thái UI-A08 ================= */
  function renderStatus(){
    var d = A.doc(), T = TC.S.toggles, viewer = TC.isViewer();
    var tg = function(k, icn, name, key, sv, need){ var dis = need && !TC.can(need); return '<button class="sbt' + (T[k] && !dis ? ' is-on' : '') + '" type="button" data-tgl="' + k + '" aria-pressed="' + !!T[k] + '"' + (dis ? ' aria-disabled="true"' : '') + ' aria-label="' + name + '" data-tip="' + name + '" data-key="' + (key || '') + '" data-desc="' + (sv ? 'Biến hệ thống ' + sv + '. ' : '') + 'Chuột phải để xem thiết lập.' + (dis ? ' Có trong TrionCAD Pro.' : '') + '">' + ic(icn) + '</button>'; };
    var sp = d ? (d.space === 'model' ? 'MODEL' : d.activeVp ? 'MODEL' : 'PAPER') : '';
    var font = d && TC.fontDoctor ? TC.fontDoctor.summary(d) : null;
    var vpScale = d && d.space !== 'model' && d.activeVp ? (function(){ var vp = d.layouts[d.space].vports.filter(function(v){ return v.id === d.activeVp; })[0]; return vp ? '1:' + Math.round(vp.scale) : ''; })() : '';
    $('#sb').innerHTML = (A.active === 'start' ? '<span class="sb-txt">Start</span>' :
      '<button class="sb-coord" type="button" id="sb-coord" data-tip="Tọa độ con trỏ" data-key="Ctrl+I" data-desc="Bấm để đổi kiểu hiển thị: tuyệt đối, tương đối, tắt.">0.00, 0.00, 0.00</button>' +
      '<button class="sb-txt" type="button" id="sb-space" data-tip="Không gian làm việc" data-desc="Trong layout: chuyển giữa Paper và Model trong viewport.">' + sp + '</button><span class="sb-sep"></span>' +
      (viewer ? '' : tg('grid','grid','Lưới','F7','GRIDMODE') + tg('snap','snap','Snap','F9','SNAPMODE') + tg('dyn','dyn','Nhập động','F12','DYNMODE') + tg('ortho','ortho','Ortho','F8','ORTHOMODE') + tg('polar','polar','Polar','F10','AUTOSNAP') + '<span class="hide-1366">' + tg('iso','iso','Isometric','F5','ISODRAFT') + '</span>' + tg('otrack','otrack','Theo dõi bắt điểm','F11','AUTOSNAP')) +
      tg('osnap','osnap','Bắt điểm' + (viewer ? ' (khi đo)' : ''),'F3','OSMODE') + tg('lw','lw','Hiển thị độ dày nét','','LWDISPLAY') + (viewer ? '' : tg('transp','transp','Hiển thị độ trong suốt','','TRANSPARENCYDISPLAY') + tg('cycle','cycle','Chọn luân phiên','Ctrl+W','SELECTIONCYCLING') + tg('qp','qprops','Thuộc tính nhanh','Ctrl+Shift+P','QPMODE')) +
      '<span class="sb-sep"></span>' +
      (d && d.space !== 'model' ? '<button class="sb-txt" type="button" id="sb-vpscale" data-tip="Tỉ lệ viewport" data-desc="' + (viewer ? 'Viewer chỉ xem tỉ lệ, không đổi được.' : 'Đổi tỉ lệ viewport đang kích hoạt.') + '">' + ic('vplock') + (vpScale || 'Paper') + (viewer ? '' : ic('down', 'xs')) + '</button>' :
        '<button class="sb-txt" type="button" id="sb-anno" data-tip="Tỉ lệ chú thích" data-desc="' + (viewer ? 'Viewer chỉ xem.' : 'Tỉ lệ cho đối tượng chú thích theo tỉ lệ.') + '">' + ic('anno') + esc(d && d.annoScale || '1:100') + (viewer ? '' : ic('down', 'xs')) + '</button>' + (viewer ? '' : tg('annovis','eye','Hiển thị chú thích ở mọi tỉ lệ','','ANNOALLVISIBLE') + tg('annoauto','annoauto','Tự thêm tỉ lệ khi đổi tỉ lệ chú thích','','ANNOAUTOSCALE')))) +
      (viewer || !d ? '' : '<button class="sb-txt hide-1366" type="button" id="sb-units" data-tip="Đơn vị bản vẽ" data-desc="Đơn vị hiện hành. Bấm để đổi nhanh hoặc mở hộp thoại Đơn vị (UNITS).">' + esc((d.units && d.units.name) || 'mm') + ic('down', 'xs') + '</button>' + '<button class="sbt" type="button" id="sb-iso" aria-label="Cô lập đối tượng" data-tip="Cô lập đối tượng" data-desc="Cô lập hoặc ẩn đối tượng đang chọn; kết thúc cô lập để hiện lại.">' + ic(d.iso || d.hidden ? 'unisolate' : 'isolate') + '</button>') +
      '<span class="sb-grow"></span>' +
      '<span id="sb-tasks"></span>' +
      (font && font.issues ? '<button class="sb-chip warn" type="button" id="sb-font" data-tip="Bác sĩ Font" data-desc="' + font.missing + ' font đã thay thế · ' + font.legacy + ' đối tượng chữ mã cũ. Bấm để mở Bác sĩ Font.">' + ic('warn') + 'Font: ' + font.missing + ' thay thế</button>' : '') +
      '<button class="sbt" type="button" id="sb-gpu" aria-label="Chỉ báo đồ họa" data-tip="Đồ họa: D3D11 · Intel UHD 620" data-desc="Khung hình hiện tại 60 FPS. Bấm để mở Thiết lập đồ họa.">' + ic('gpu') + '</button>' +
      '<button class="sbt" type="button" id="sb-taskbtn" aria-label="Tác vụ nền" data-tip="Tác vụ nền" data-desc="Tự lưu, in hàng loạt, chuyển font, nạp tệp; có tiến trình và nút hủy.">' + ic('tasks') + '</button>' +
      '<button class="sbt' + (TC.notes.some(function(n){ return !n.read; }) ? ' has-dot' : '') + '" type="button" id="sb-bell" aria-label="Thông báo" data-tip="Trung tâm thông báo">' + ic('bell') + '</button>' +
      '<button type="button" id="sb-ed" class="sb-edb" data-tip="Trạng thái giấy phép" style="display:inline-flex">' + edBadge() + '</button>' +
      (viewer ? '' : '<button class="sbt" type="button" id="sb-ws" aria-label="Chuyển không gian làm việc" data-tip="Không gian làm việc" data-desc="Soạn thảo 2D, Cổ điển, Viewer.">' + ic('gear') + '</button>') +
      '<button class="sbt" type="button" id="sb-clean" aria-label="Clean Screen" data-tip="Clean Screen" data-key="Ctrl+0" data-desc="Thu gọn toàn bộ giao diện, chỉ còn vùng vẽ, dòng lệnh, thanh trạng thái.">' + ic('fullscreen') + '</button>' +
      '<button class="sbt" type="button" id="sb-custom" aria-label="Tùy biến thanh trạng thái" data-tip="Tùy biến" data-desc="Chọn mục hiện trên thanh trạng thái.">' + ic('menu') + '</button>';
    TC.emit('statusrender');
  }
  A.renderStatus = renderStatus;
  TC.on('notes', function(){ var b = $('#sb-bell'); if (b) b.classList.toggle('has-dot', TC.notes.some(function(n){ return !n.read; })); });
  A.setTask = function(html){ var t = $('#sb-tasks'); if (t) t.innerHTML = html || ''; };
  document.addEventListener('click', function(e){
    var t = e.target.closest('[data-tgl]'); if (t && t.closest('#sb')){ if (t.getAttribute('aria-disabled') === 'true') { TC.screens.upgrade(t.dataset.tip, 'PRO'); return; } toggle(t.dataset.tgl); return; }
    var id = e.target.closest('#sb button') && e.target.closest('#sb button').id;
    if (id === 'sb-coord') { TC.S.coordMode = TC.S.coordMode === 'abs' ? 'off' : 'abs'; updateCoord(lastCoord); }
    else if (id === 'sb-space') { var d = A.doc(); if (d && d.space !== 'model'){ if (d.activeVp) { d.activeVp = null; } else { d.activeVp = d.layouts[d.space].vports[0].id; } A.view.draw(); renderStatus(); } }
    else if (id === 'sb-font') TC.cmd.run('FIXFONT');
    else if (id === 'sb-gpu') TC.cmd.run('GRAPHICSCONFIG');
    else if (id === 'sb-bell') TC.screens.notifications();
    else if (id === 'sb-ed') TC.screens.license();
    else if (id === 'sb-clean') A.cleanScreen();
    else if (id === 'sb-taskbtn') TC.screens.tasks(e.target.closest('button'));
    else if (id === 'sb-custom') { var r = e.target.closest('button').getBoundingClientRect(); TC.menu([{ h:'Hiện trên thanh trạng thái' }, { label:'Tọa độ', check:true }, { label:'Model / Paper', check:true }, { label:'Bắt điểm', check:true }, { label:'Hiển thị độ dày nét', check:true }, { label:'Tỉ lệ chú thích / viewport', check:true }, { label:'Chỉ báo Bác sĩ Font', check:true }, { label:'Chỉ báo đồ họa', check:true }, { label:'Tác vụ nền', check:true }, { label:'Thông báo', check:true }, { label:'Huy hiệu phiên bản', check:true }, { label:'Clean Screen', check:true }], r.right, r.top, { alignRight:true, up:true }); }
    else if (id === 'sb-vpscale' && !TC.isViewer()) { var r2 = e.target.closest('button').getBoundingClientRect(), dd = A.doc(), L0 = dd.layouts[dd.space], vp0 = dd.activeVp && L0.vports.filter(function(v){ return v.id === dd.activeVp; })[0]; if (!vp0){ TC.toast('Nhấp đúp vào trong một viewport để đổi tỉ lệ của nó.', { type:'info', log:false }); return; } TC.menu([20, 25, 50, 75, 100, 200, 500].map(function(k){ return { label:'1:' + k, check:Math.round(vp0.scale) === k, act:function(){ if (vp0.locked){ TC.toast('Viewport đang khóa hiển thị. Mở khóa trước khi đổi tỉ lệ.', { type:'warn', log:false }); return; } TC.ed.setVpScale(vp0, k); } }; }).concat([{ sep:1 }, { label:'Danh sách tỉ lệ…', act:function(){ TC.cmd.run('SCALELISTEDIT'); } }]), r2.left, r2.top, { up:true }); }
    else if (id === 'sb-vpscale' || id === 'sb-anno') { if (TC.isViewer()) TC.toast('Viewer chỉ xem tỉ lệ. Đổi tỉ lệ có trong TrionCAD Pro.', { type:'info', log:false }); }
    else if (id === 'sb-units') { var ru = e.target.closest('button').getBoundingClientRect(), du = A.doc(); TC.menu([{ h:'Đơn vị độ dài' }].concat([['mm', 'Milimét'], ['cm', 'Xentimét'], ['m', 'Mét']].map(function(u){ return { label:u[1] + ' <span class="faint mono">' + u[0] + '</span>', check:(du.units && du.units.name || 'mm') === u[0], act:function(){ du.units = Object.assign({}, du.units, { name:u[0] }); renderStatus(); TC.cmdline.msg('&lt;Đơn vị hiện: ' + u[1] + '. Tọa độ không đổi; chỉ đổi cách hiển thị và chèn.&gt;'); } }; })).concat([{ sep:1 }, { label:'Đơn vị bản vẽ…', icon:'dist', act:function(){ TC.cmd.run('UNITS'); } }]), ru.left, ru.top, { up:true }); }
    else if (id === 'sb-iso') { var ri = e.target.closest('button').getBoundingClientRect(), di = A.doc(); TC.menu([{ label:'Cô lập đối tượng', icon:'isolate', dis:!di.sel.length, act:function(){ TC.layers.isolateSel(); renderStatus(); } }, { label:'Ẩn đối tượng', icon:'eyeoff', dis:!di.sel.length, act:function(){ TC.layers.hideSel(); renderStatus(); } }, { label:'Kết thúc cô lập', icon:'unisolate', dis:!di.iso && !di.hidden, act:function(){ TC.layers.endIso(); renderStatus(); } }], ri.left, ri.top, { up:true }); }
    else if (id === 'sb-ws') TC.wsMenu(e.target.closest('button'));
  });
  document.addEventListener('contextmenu', function(e){
    var t = e.target.closest('#sb [data-tgl]'); if (!t) return; e.preventDefault();
    if (t.dataset.tgl === 'osnap') { TC.menu(osnapMenu(), e.clientX, e.clientY, { up:true }); return; }
    if (t.dataset.tgl === 'qp' && TC.qpMenu) { TC.qpMenu(e.clientX, e.clientY); return; }
    if (t.dataset.tgl === 'polar') { TC.menu(['90','45','30','22.5','18','15','10','5'].map(function(a){ return { label:a + '°', check:Number(a) === (TC.S.polarInc || 90), act:function(){ TC.S.polarInc = Number(a); TC.save('polarInc', TC.S.polarInc); if (!TC.S.toggles.polar) toggle('polar'); TC.cmdline.msg('&lt;Góc polar tăng: ' + a + '°&gt;'); } }; }).concat([{ sep:1 }, { label:'Thiết lập theo dõi…', need:'PRO', act:function(){ TC.cmd.run('DSETTINGS', 'polar'); } }]), e.clientX, e.clientY, { up:true }); return; }
    var tabOf = { grid:'snap', snap:'snap', ortho:'snap', otrack:'osnap', dyn:'dyn', cycle:'cyc', iso:'snap', transp:null };
    TC.menu([{ label:'Thiết lập…', need:t.dataset.tgl === 'lw' ? 'V' : 'PRO', act:function(){ if (t.dataset.tgl === 'lw') TC.cmd.run('LWEIGHT'); else TC.cmd.run('DSETTINGS', tabOf[t.dataset.tgl] || 'snap'); } }], e.clientX, e.clientY, { up:true });
  });
  function osnapMenu(){
    var M = TC.S.osnap, k = ['end','mid','cen','gcen','node','quad','int','ext','ins','per','tan','nea','app','par'];
    return [{ h:'Chế độ bắt điểm' }].concat(k.map(function(x){ return { label:TC.SNAP_NAMES[x], check:!!M[x], act:function(){ M[x] = !M[x]; TC.save('osnap', M); setTimeout(function(){ TC.menu(osnapMenu(), lastMenuXY[0], lastMenuXY[1], { up:true }); }, 0); } }; })).concat([{ sep:1 }, { label:'Chọn tất cả', act:function(){ k.forEach(function(x){ M[x] = true; }); TC.save('osnap', M); } }, { label:'Bỏ tất cả', act:function(){ k.forEach(function(x){ M[x] = false; }); TC.save('osnap', M); } }, { label:'Thiết lập bắt điểm…', need:'PRO' }]);
  }
  var lastMenuXY = [0, 0]; document.addEventListener('contextmenu', function(e){ lastMenuXY = [e.clientX, e.clientY]; }, true);
  TC.osnapMenu = osnapMenu;
  function toggle(k){
    var T = TC.S.toggles; T[k] = !T[k]; TC.save('toggles', T);
    var names = { grid:'Lưới', snap:'Snap', ortho:'Ortho', polar:'Polar', osnap:'Bắt điểm', otrack:'Theo dõi bắt điểm', dyn:'Nhập động', lw:'Hiển thị độ dày nét', transp:'Độ trong suốt', cycle:'Chọn luân phiên', iso:'Isometric', qp:'Thuộc tính nhanh', annovis:'Hiển thị chú thích ở mọi tỉ lệ', annoauto:'Tự thêm tỉ lệ' };
    TC.cmdline.msg('&lt;' + names[k] + ' ' + (T[k] ? 'bật' : 'tắt') + '&gt;');
    renderStatus(); if (k === 'lw' || k === 'grid' || k === 'annovis') A.view.draw(); TC.emit('toggle', k);
  }
  A.toggle = toggle;
  var lastCoord = null;
  function updateCoord(p){ lastCoord = p; var c = $('#sb-coord'); if (!c) return; c.textContent = TC.S.coordMode === 'off' ? 'Tọa độ tắt' : p ? TC.d(p[0]) + ', ' + TC.d(p[1]) + ', 0.00' : '—'; }
  A.updateCoord = updateCoord;
  A.cleanScreen = function(){ var on = $('#app').classList.toggle('clean'); TC.cmdline.msg(on ? '&lt;Clean Screen bật · Ctrl+0 để tắt&gt;' : '&lt;Clean Screen tắt&gt;'); setTimeout(function(){ A.view.resize(); }, 0); };

  /* ================= chuột và phím trên vùng vẽ ================= */
  function bindCanvas(){
    var cv = $('#cv'), V = A.view, lastMid = 0, clickPick = null, downAt = null;
    function local(e){ var r = cv.getBoundingClientRect(); return [e.clientX - r.left, e.clientY - r.top]; }
    cv.addEventListener('mousemove', function(e){
      var m = local(e); V.mouse = m;
      if (V.drag && V.drag.kind === 'pan'){ V.panBy(m[0] - V.drag.last[0], m[1] - V.drag.last[1]); V.drag.last = m; return; }
      if (V.drag && (V.drag.kind === 'sel' || V.drag.kind === 'zwin')){ V.drag.b = m; V.drawOver(); return; }
      if (downAt && !V.drag && Math.hypot(m[0] - downAt[0], m[1] - downAt[1]) > 4 && TC.cad.mode() === 'idle'){ V.drag = { kind:'sel', a:downAt, b:m }; V.hover = null; V.drawOver(); return; }
      TC.cad.move(m, e);
    });
    cv.addEventListener('mouseleave', function(){ V.mouse = null; V.hover = null; V.snap = null; TC.cad.leave(); V.drawOver(); });
    cv.addEventListener('wheel', function(e){ e.preventDefault(); var m = local(e); V.zoomAt(m[0], m[1], e.deltaY < 0 ? 1.2 : 1 / 1.2); TC.cad.move(m, e); }, { passive:false });
    cv.addEventListener('mousedown', function(e){
      if (e.target.closest('input, textarea, select, button')) return; // ô soạn chữ, thanh công cụ nổi trên canvas
      // chặn hành động mặc định: nếu không, trình duyệt kéo focus về #cv sau khi lệnh đã mở ô soạn chữ
      cv.focus({ preventScroll:true }); e.preventDefault(); TC.closeMenus(0);
      var m = local(e);
      if (e.button === 1){ e.preventDefault(); var now = Date.now(); if (now - lastMid < 350){ V.zoomExtents(); lastMid = 0; return; } lastMid = now; V.drag = { kind:'pan', last:m }; cv.classList.add('cur-panning'); return; }
      if (e.button !== 0) return;
      if (TC.cad.down(m, e)) return;
      if (clickPick){ var a = clickPick; clickPick = null; V.drag = null; TC.cad.boxDone(a, m, e); return; }
      downAt = m;
    });
    addEventListener('mouseup', function(e){
      var V2 = A.view;
      if (e.button === 1 && V2.drag && V2.drag.kind === 'pan'){ V2.drag = null; cv.classList.remove('cur-panning'); TC.emit('view'); return; }
      if (e.button !== 0) return;
      if (TC.cad.up()) return;
      if (V2.drag && V2.drag.kind === 'pan') { V2.drag = null; TC.emit('view'); return; }
      if (V2.drag && V2.drag.kind === 'zwin'){ var z = V2.drag; V2.drag = null; TC.cad.zwinDone(z.a, z.b); return; }
      if (V2.drag && V2.drag.kind === 'sel'){ var d = V2.drag; V2.drag = null; downAt = null; TC.cad.boxDone(d.a, d.b, e); return; }
      if (downAt){ var p = downAt; downAt = null; if (e.target.closest('#cv')) { var hit = TC.cad.click(p, e); if (!hit) { clickPick = p; V2.drag = { kind:'sel', a:p, b:p }; } } }
    });
    cv.addEventListener('dblclick', function(e){ TC.cad.dbl(local(e), e); });
    cv.addEventListener('contextmenu', function(e){ e.preventDefault(); clickPick = null; if (A.view.drag && A.view.drag.kind === 'sel') { A.view.drag = null; A.view.drawOver(); } TC.cad.context(local(e), e); });
    // kéo-thả tệp vào vùng vẽ / dải tab tệp (07.1 §9.11)
    ['#cv', '#ft', '#start'].forEach(function(s){
      var el = $(s);
      el.addEventListener('dragover', function(e){ if (e.dataTransfer && Array.prototype.indexOf.call(e.dataTransfer.types, 'Files') >= 0){ e.preventDefault(); if (!$('.drop-hint', el) && s !== '#ft') el.appendChild(TC.el('<div class="drop-hint">Thả tệp DWG, DXF để mở</div>')); } });
      el.addEventListener('dragleave', function(e){ if (!el.contains(e.relatedTarget)) { var h = $('.drop-hint', el); if (h) h.remove(); } });
      el.addEventListener('drop', function(e){ e.preventDefault(); var h = $('.drop-hint', el); if (h) h.remove(); var f = e.dataTransfer.files[0]; if (f) TC.files.openDropped(f); });
    });
    $('#vpc').addEventListener('click', function(e){
      var b = e.target.closest('[data-vpc]'); if (!b) return; if (TC.vpcMenu) return TC.vpcMenu(b.dataset.vpc, b); var r = b.getBoundingClientRect(), k = b.dataset.vpc;
      var items = k === 'm' ? [{ label:'Phóng to viewport', icon:'fullscreen' }, { label:'Cấu hình viewport', sub:[{ label:'Một' }, { label:'Hai: dọc' }, { label:'Hai: ngang' }, { label:'Bốn' }].map(function(x){ x.need = 'PRO'; return x; }) }, { sep:1 }, { label:'ViewCube', check:false }, { label:'Navigation bar', check:false, dis:true }]
        : k === 'v' ? ['Trên','Dưới','Trái','Phải','Trước','Sau','Isometric Tây Nam','Isometric Đông Nam'].map(function(x, i){ return { label:x, check:i === 0, act:function(){ if (i) TC.toast('Bản vẽ này chỉ có đối tượng 2D.', { type:'info', log:false }); } }; })
        : ['2D Wireframe','Wireframe','Hidden','Shaded','Realistic'].map(function(x, i){ return { label:x, check:i === 0 }; });
      TC.menu(items, r.left, r.bottom + 2, { returnFocus:b });
    });
  }
  function bindKeys(){
    document.addEventListener('keydown', function(e){
      if (e.defaultPrevented) return;
      var tag = e.target.tagName, inField = (tag === 'INPUT' || tag === 'TEXTAREA' || tag === 'SELECT' || e.target.isContentEditable) && e.target.id !== 'cmd-in';
      var top = TC.topDialog(); // hộp thoại không modal (in hàng loạt, tổng chiều dài) vẫn để phím tới vùng vẽ
      if ((top && (top.el.getAttribute('aria-modal') === 'true' || top.el.contains(e.target))) || document.querySelector('.menu') || document.querySelector('.full')) return;
      var k = e.key, ctrl = e.ctrlKey || e.metaKey;
      // phím chức năng giữ thói quen AutoCAD (07.1 §11)
      var fmap = { F3:'osnap', F7:'grid', F8:'ortho', F9:'snap', F10:'polar', F11:'otrack', F12:'dyn' };
      if (fmap[k]){ e.preventDefault(); if (TC.isViewer() && k !== 'F3') { TC.cmdline.msg('&lt;' + k + ': có trong TrionCAD Pro&gt;'); return; } toggle(fmap[k]); return; }
      if (k === 'F1'){ e.preventDefault(); TC.openHelp(TC.cmdline.current()); return; }
      if (k === 'F2'){ e.preventDefault(); TC.screens.textWindow(); return; }
      if (ctrl && (k === 'o' || k === 'O')){ e.preventDefault(); TC.cmd.run('OPEN'); return; }
      if (ctrl && (k === 'p' || k === 'P')){ e.preventDefault(); TC.cmd.run('PLOT'); return; }
      if (ctrl && k === '1'){ e.preventDefault(); TC.cmd.run('PROPERTIES'); return; }
      if (ctrl && k === '0'){ e.preventDefault(); A.cleanScreen(); return; }
      if (ctrl && k === '9'){ e.preventDefault(); $('#app').classList.toggle('no-cmd'); setTimeout(function(){ A.view.resize(); }, 0); return; }
      if (ctrl && (k === 'a' || k === 'A') && !inField){ e.preventDefault(); TC.cad.selectAll(); return; }
      if (ctrl && k === 'Tab'){ e.preventDefault(); var o = ['start'].concat(A.docs.filter(function(d){ return d.open; }).map(function(d){ return d.id; })); var i = o.indexOf(A.active); A.activate(o[(i + (e.shiftKey ? -1 : 1) + o.length) % o.length]); return; }
      if (ctrl && (k === 'PageDown' || k === 'PageUp')){ e.preventDefault(); var d = A.doc(); if (!d) return; var sp = ['model'].concat(d.layouts.map(function(l, i){ return i; })); var j = sp.indexOf(d.space); setSpace(sp[TC.clamp(j + (k === 'PageDown' ? 1 : -1), 0, sp.length - 1)]); return; }
      if (ctrl && k === 'F4'){ e.preventDefault(); if (A.doc()) A.closeDoc(A.active); return; }
      if (ctrl && (k === 'i' || k === 'I')){ e.preventDefault(); TC.S.coordMode = TC.S.coordMode === 'abs' ? 'off' : 'abs'; updateCoord(lastCoord); return; }
      if (ctrl && (k === 'z' || k === 'Z' || k === 'y' || k === 'Y' || k === 's' || k === 'S' || k === 'n' || k === 'N') && !inField){ e.preventDefault(); TC.cmd.run(e.shiftKey && k.toLowerCase() === 's' ? 'SAVEAS' : { z:'U', y:'REDO', s:'QSAVE', n:'NEW' }[k.toLowerCase()]); return; }
      if (ctrl && !inField && !TC.isViewer() && /^[cvxCVX]$/.test(k)){ e.preventDefault(); var kk = k.toLowerCase(); TC.cmd.run(kk === 'x' ? 'CUTCLIP' : kk === 'c' ? (e.shiftKey ? 'COPYBASE' : 'COPYCLIP') : (e.shiftKey ? 'PASTEBLOCK' : 'PASTECLIP')); return; }
      if (ctrl || e.altKey || inField || A.active === 'start') return;
      if (k === 'Tab' && TC.cad.cycleSnap()) { e.preventDefault(); return; }
      if (k === 'Tab' && TC.dyn && TC.dyn.tab($('#cmd-in'))) { e.preventDefault(); return; }
      if (k === 'Delete'){ e.preventDefault(); if (A.doc() && A.doc().sel.length) TC.cmd.run('ERASE'); return; }
      // mọi phím gõ trên vùng vẽ đều vào dòng lệnh (07.1 §8)
      TC.cmdline.key(e);
    });
  }
})();
