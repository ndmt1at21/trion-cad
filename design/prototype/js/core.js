/* TrionCAD prototype · tiện ích dùng chung và trạng thái. Script thường (không module) để mở được bằng file://. */
(function(){
  'use strict';
  var TC = window.TC = window.TC || {};
  TC.$ = function(s, r){ return (r || document).querySelector(s); };
  TC.$$ = function(s, r){ return Array.prototype.slice.call((r || document).querySelectorAll(s)); };
  TC.esc = function(s){ return String(s == null ? '' : s).replace(/[&<>"']/g, function(c){ return { '&':'&amp;', '<':'&lt;', '>':'&gt;', '"':'&quot;', "'":'&#39;' }[c]; }); };
  TC.el = function(html){ var t = document.createElement('template'); t.innerHTML = html.trim(); return t.content.firstElementChild; };
  TC.fold = function(s){ return String(s).normalize('NFD').replace(/[̀-ͯ]/g, '').replace(/đ/g, 'd').replace(/Đ/g, 'D').toLowerCase(); };
  TC.clamp = function(v, a, b){ return Math.max(a, Math.min(b, v)); };

  // số trên giao diện chung theo kiểu Việt Nam (1.250); số bản vẽ luôn dùng dấu chấm thập phân (07.1 §13.4)
  TC.n = function(v){ return Number(v).toLocaleString('vi-VN'); };
  TC.d = function(v, p){ return Number(v).toFixed(p == null ? 2 : p); };
  TC.date = function(dt){ dt = dt || new Date(); var p = function(x){ return (x < 10 ? '0' : '') + x; }; return p(dt.getDate()) + '/' + p(dt.getMonth() + 1) + '/' + dt.getFullYear(); };
  TC.size = function(b){ return b > 1048576 ? (b / 1048576).toFixed(1).replace('.', ',') + ' MB' : Math.round(b / 1024) + ' KB'; };

  // sự kiện
  var subs = {};
  TC.on = function(ev, fn){ (subs[ev] = subs[ev] || []).push(fn); };
  TC.emit = function(ev, data){ (subs[ev] || []).slice().forEach(function(fn){ fn(data); }); };

  // lưu tiện ích theo từng người xem; trang vẫn chạy khi bộ nhớ bị chặn
  TC.load = function(k, def){ try { var v = localStorage.getItem('tc-proto-' + k); return v == null ? def : JSON.parse(v); } catch (e) { return def; } };
  TC.save = function(k, v){ try { localStorage.setItem('tc-proto-' + k, JSON.stringify(v)); } catch (e) {} };

  // phiên bản và cổng tính năng (07.1 §2)
  TC.EDITIONS = {
    viewer: { name:'TrionCAD Viewer', badge:'Viewer', rank:0 },
    vp:     { name:'TrionCAD Viewer Pro', badge:'Viewer Pro', rank:1 },
    trial:  { name:'TrionCAD Pro', badge:'Dùng thử', rank:2 },
    pro:    { name:'TrionCAD Pro', badge:'Pro', rank:2 },
    edu:    { name:'TrionCAD Giáo dục', badge:'Giáo dục', rank:2 },
    ent:    { name:'TrionCAD Doanh nghiệp', badge:'Doanh nghiệp', rank:3 }
  };
  TC.NEED = { V:0, VP:1, PRO:2, ENT:3 };
  TC.S = {
    edition: TC.load('edition', 'viewer'),
    trialDays: 12,
    theme: TC.load('theme', 'auto'),
    density: TC.load('density', 'compact'),
    lite: TC.load('lite', false),
    onboarded: TC.load('onboarded', false),
    toggles: TC.load('toggles', { grid:false, snap:false, ortho:false, polar:true, osnap:true, otrack:true, dyn:true, lw:false, transp:true, cycle:false, iso:false, qp:false }),
    osnap: TC.load('osnap', { end:true, mid:true, cen:true, gcen:false, node:true, quad:true, int:true, ext:false, ins:true, per:true, tan:false, nea:false, app:false, par:false }),
    coordMode: 'abs',
    licSim: 'ok', offDays: 5, // mô phỏng trạng thái giấy phép (UI-L03)
    pal: TC.load('pal', { right:['props','layers','fontdoc'], rightActive:'props', rightPinned:true, left:['tcvnlib','blocks'], leftActive:null, leftPinned:false })
  };
  TC.can = function(need){ return TC.EDITIONS[TC.S.edition].rank >= TC.NEED[need || 'V']; };
  TC.isViewer = function(){ return TC.EDITIONS[TC.S.edition].rank < 2; };
  TC.applyTheme = function(){
    var r = document.documentElement;
    if (TC.S.theme === 'auto') r.removeAttribute('data-theme'); else r.setAttribute('data-theme', TC.S.theme);
    if (TC.S.density === 'comfortable') r.setAttribute('data-density', 'comfortable'); else r.removeAttribute('data-density');
    if (TC.S.lite) r.setAttribute('data-lite', ''); else r.removeAttribute('data-lite');
    TC.emit('theme');
  };
  TC.cssv = function(name){ return getComputedStyle(document.documentElement).getPropertyValue(name).trim(); };
})();
