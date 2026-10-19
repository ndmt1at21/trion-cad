/* TrionCAD prototype · Đợt 5 (sau v1.0): máy tính nhanh (I10), kiểm tra chính tả (F10), hyperlink (F15). */
(function(){
  'use strict';
  var TC = window.TC, $ = TC.$, $$ = TC.$$, esc = TC.esc, ic = TC.ic, G = TC.geo, IMPL = TC.cmd.impl, ED = TC.ed, SC = TC.screens, H = SC.h;
  function A(){ return TC.app; } function V(){ return TC.app.view; } function D(){ return TC.app.doc(); }
  var OPEN = SC.open5 = SC.open5 || {}; // mã màn hình → hàm mở (danh mục gọi khi bấm); tiện ích mở ở SC.cat (D1, PRO, MOD, RUN, plan, pick, flash)
  function CTRL(){ return TC.isMac && TC.isMac() ? '⌘' : 'Ctrl'; }
  function ib(k, icon, tip, x){ return '<button class="ib" type="button" data-q="' + k + '" aria-label="' + esc(tip) + '" data-tip="' + esc(tip) + '"' + (x || '') + '>' + ic(icon) + '</button>'; }
  function allEnts(d){ return d.ents.concat.apply(d.ents, d.layouts.map(function(L){ return L.ents; })); }

  /* ================= Máy tính nhanh UI-I10 (QUICKCALC, Ctrl+8) ================= */
  // dùng chung cho palette và hộp "tính cho ô": lịch sử, bộ nhớ, biến, mục đang mở, quy đổi đơn vị
  var QDEF = { hist:[['3600*2+450', '7650'], ['(3900-220)/2', '1840'], ['2.5m+300', '2800']], mem:0, open:{ pad:1 }, ut:'len', ua:'m', ub:'mm', uv:'1', x:'',
    vars:[['phi', 'c', '1.618034', 'Hằng số', 'Tỉ lệ vàng'], ['g', 'c', '9.81', 'Hằng số', 'Gia tốc trọng trường, m/s²'], ['kl_thep', 'c', '7850', 'Hằng số', 'Khối lượng riêng của thép, kg/m³'], ['kl_btct', 'c', '2500', 'Hằng số', 'Khối lượng riêng bê tông cốt thép, kg/m³'],
      ['dee', 'f', 'dist(cur,cur)', 'Lấy từ bản vẽ', 'Khoảng cách giữa hai điểm'], ['mee', 'f', '(cur+cur)/2', 'Lấy từ bản vẽ', 'Trung điểm của hai điểm'], ['ille', 'f', 'ill(cur,cur,cur,cur)', 'Lấy từ bản vẽ', 'Giao điểm của hai đường qua bốn điểm'], ['bk', 'f', 'rad', 'Lấy từ bản vẽ', 'Bán kính của đường tròn hoặc cung được chọn'],
      ['h_tang', 'c', '3600', 'Của tôi', 'Chiều cao tầng điển hình, mm']] };
  var QC = Object.assign(JSON.parse(JSON.stringify(QDEF)), TC.load('qcalc', {})); QC.prop = null; QC.vs = null;
  function qSave(){ TC.save('qcalc', { hist:QC.hist.slice(-40), mem:QC.mem, open:QC.open, ut:QC.ut, ua:QC.ua, ub:QC.ub, uv:QC.uv, x:QC.x, vars:QC.vars }); }
  // số trong máy tính theo quy ước bản vẽ: dấu chấm thập phân, tối đa 6 chữ số lẻ
  function fmt(v){ if (Array.isArray(v)) return '[' + v.map(fmt).join(', ') + ']'; if (!isFinite(v)) return String(v); var a = Math.abs(v), s = a && (a >= 1e15 || a < 1e-6) ? v.toExponential(6).replace(/\.?0+e/, 'e') : v.toFixed(6).replace(/\.?0+$/, ''); return s === '-0' ? '0' : s; }

  /* ---------- tính biểu thức ---------- */
  var U = { mm:1, cm:10, dm:100, m:1e3, km:1e6, 'in':25.4, ft:304.8, "'":304.8, '"':25.4 };
  function lex(s){
    var out = [], re = /\s*(?:((?:\d+\.?\d*|\.\d+)(?:e[+-]?\d+)?)(mm|cm|dm|km|ft|in|m|'|")?|([A-Za-z_]\w*)|(\S))/gy, m;
    while (re.lastIndex < s.length && (m = re.exec(s))){
      if (m[1] != null){ var v = Number(m[1]) * (m[2] ? U[m[2]] : 1), p = out[out.length - 1]; if (m[2] === '"' && p && p.ft){ p.v += v; p.ft = false; } else out.push({ t:'n', v:v, ft:m[2] === "'" }); } // 3'6" = 3 feet 6 inch
      else if (m[3]) out.push({ t:'id', v:m[3] }); else out.push({ t:m[4] });
    }
    return out;
  }
  function ill(a, b, c, d){ var r = [b[0] - a[0], b[1] - a[1]], s = [d[0] - c[0], d[1] - c[1]], k = r[0] * s[1] - r[1] * s[0]; if (Math.abs(k) < 1e-12) throw 'Hai đường song song, không có giao điểm.'; var t = ((c[0] - a[0]) * s[1] - (c[1] - a[1]) * s[0]) / k; return [a[0] + t * r[0], a[1] + t * r[1]]; }
  var RAD = Math.PI / 180;
  var FN = { sin:function(x){ return Math.sin(x * RAD); }, cos:function(x){ return Math.cos(x * RAD); }, tan:function(x){ return Math.tan(x * RAD); }, asin:function(x){ return Math.asin(x) / RAD; }, acos:function(x){ return Math.acos(x) / RAD; }, atan:function(x){ return Math.atan(x) / RAD; },
    sqrt:function(x){ return Math.sqrt(x); }, abs:function(x){ return Math.abs(x); }, round:function(x){ return Math.round(x); }, trunc:function(x){ return Math.trunc(x); }, log:function(x){ return Math.log10(x); }, ln:function(x){ return Math.log(x); }, exp:function(x){ return Math.exp(x); },
    r2d:function(x){ return x / RAD; }, d2r:function(x){ return x * RAD; }, min:function(a, b){ return Math.min(a, b); }, max:function(a, b){ return Math.max(a, b); },
    dist:function(a, b){ return G.dist(a, b); }, ang:function(a, b){ return (G.ang(a, b) + 360) % 360; }, ill:function(a, b, c, d){ return ill(a, b, c, d); } };
  var VECF = { dist:1, ang:1, ill:1 }, COMMA = 'Dấu thập phân là dấu chấm: gõ 12.5, không gõ 12,5.';
  function varOf(n){ n = String(n).toLowerCase(); return QC.vars.filter(function(v){ return v[0].toLowerCase() === n; })[0]; }
  // số có đơn vị (2.5m = 2500), + − × ÷ ^, ngoặc, tọa độ [x,y], hàm, biến; cur và rad lấy lần lượt từ q (giá trị đã chỉ trên bản vẽ)
  function calc(src, q, dep){
    var tk = lex(String(src).replace(/^\s*=/, '').replace(/×/g, '*').replace(/÷/g, '/').replace(/−/g, '-').replace(/π/g, 'pi')), i = 0;
    function E(m){ throw { m:m }; }
    function is(t){ return tk[i] && tk[i].t === t; }
    function eat(t){ if (!is(t)) E(is(',') && t === ')' ? COMMA : t === ')' ? 'Thiếu dấu đóng ngoặc “)”.' : t === ']' ? 'Thiếu dấu “]” của tọa độ.' : 'Tọa độ viết dạng [x,y].'); i++; }
    function sc(v, f){ if (Array.isArray(v)) E(f + ' cần một số, không nhận tọa độ.'); return v; }
    function sum(){ var v = prod(); while (is('+') || is('-')){ var s = tk[i++].t === '+' ? 1 : -1, w = prod(); if (Array.isArray(v) !== Array.isArray(w)) E('Không cộng, trừ số với tọa độ được.'); v = Array.isArray(v) ? [v[0] + s * w[0], v[1] + s * w[1]] : v + s * w; } return v; }
    function prod(){ var v = neg(); while (is('*') || is('/')){ var op = tk[i++].t, w = neg(); if (Array.isArray(w)){ if (op === '/' || Array.isArray(v)) E('Không nhân hai tọa độ hay chia cho tọa độ được.'); var t2 = v; v = w; w = t2; } if (op === '/' && !w) E('Không chia cho 0 được.'); v = Array.isArray(v) ? v.map(function(x){ return op === '*' ? x * w : x / w; }) : op === '*' ? v * w : v / w; } return v; }
    function neg(){ if (is('-')){ i++; var v = neg(); return Array.isArray(v) ? [-v[0], -v[1]] : -v; } if (is('+')){ i++; return neg(); } var b = atom(); if (is('^')){ i++; return Math.pow(sc(b, 'Lũy thừa'), sc(neg(), 'Lũy thừa')); } return b; }
    function atom(){
      var t = tk[i++]; if (!t) E('Biểu thức còn thiếu.');
      if (t.t === 'n') return t.v;
      if (t.t === '('){ var v = sum(); eat(')'); return v; }
      if (t.t === '['){ var x = sc(sum(), 'Tọa độ'), y; eat(','); y = sc(sum(), 'Tọa độ'); if (is(',')){ i++; sum(); } eat(']'); return [x, y]; }
      if (t.t === 'id'){
        var n = t.v.toLowerCase(), f = FN[n];
        if (is('(')){ i++; if (!f) E('Không có hàm “' + t.v + '”. Xem mục Hàm khoa học.'); var a = []; if (!is(')')){ a.push(sum()); while (is(',')){ i++; a.push(sum()); } } eat(')');
          if (a.length !== f.length) E('Hàm ' + n + ' cần ' + f.length + ' giá trị.');
          a.forEach(function(z){ if (Array.isArray(z) !== !!VECF[n]) E(VECF[n] ? 'Hàm ' + n + ' nhận tọa độ, ví dụ ' + n + '([0,0],[3,4]).' : 'Hàm ' + n + ' cần một số.'); });
          try { return f.apply(null, a); } catch (m){ E(m); } }
        if (n === 'pi') return Math.PI; if (n === 'e') return Math.E;
        if (n === 'ans'){ if (QC.ans == null) E('Chưa có kết quả trước (ans).'); return QC.ans; }
        if (n === 'cur' || n === 'rad'){ if (!q || !q.length) E('Bấm Enter để chỉ trên bản vẽ cho ' + n + '.'); return q.shift(); }
        var vr = varOf(n);
        if (vr && vr[1] === 'c'){ if ((dep || 0) > 6) E('Biến lồng nhau quá sâu.'); var r = calc(vr[2], q, (dep || 0) + 1); if (r.err) E('Biến ' + vr[0] + ': ' + r.err); return r.v; }
        if (vr) E('Bấm Enter để chỉ trên bản vẽ cho ' + vr[0] + '.');
        if (f) E('Hàm ' + n + ' cần dấu ngoặc, ví dụ ' + n + '(30).');
        E('Chưa có biến “' + t.v + '”. Tạo trong mục Biến.');
      }
      E(t.t === ',' ? COMMA : 'Biểu thức chưa đúng ở “' + t.t + '”.');
    }
    try {
      if (!tk.length) return { err:'Nhập biểu thức.' };
      var v = sum(), lt = tk[i]; if (lt) E(lt.t === ',' ? COMMA : 'Biểu thức chưa đúng ở “' + (lt.t === 'n' ? fmt(lt.v) : lt.t === 'id' ? lt.v : lt.t) + '”.');
      if ((Array.isArray(v) ? v : [v]).some(function(x){ return !isFinite(x); })) E('Kết quả không xác định (chia cho 0, căn số âm?).');
      return { v:v };
    } catch (x){ return { err:x && x.m != null ? String(x.m) : 'Biểu thức chưa đúng.' }; }
  }
  // biến kiểu hàm thay bằng biểu thức của nó; cur, rad trong đó là giá trị cần chỉ trên bản vẽ khi tính
  function expand(src, dep){ return String(src).replace(/[A-Za-z_]\w*/g, function(n, at, s){ var v = varOf(n); return v && v[1] === 'f' && !/[\d.]/.test(s.charAt(at - 1)) && (dep || 0) < 6 ? '(' + expand(v[2], (dep || 0) + 1) + ')' : n; }); }
  function needs(ex){ return lex(ex).filter(function(t){ return t.t === 'id' && /^(cur|rad)$/i.test(t.v); }).map(function(t){ return t.v.toLowerCase(); }); }
  function evalAsk(src){
    var ex = expand(src), nd = needs(ex), np = 0;
    if (!nd.length) return Promise.resolve(calc(ex));
    return pickSeq(nd.map(function(k){ return k === 'rad' ? { k:'rad', text:'Chọn đường tròn hoặc cung để lấy bán kính' } : { k:'pt', text:'Chỉ định điểm ' + (++np), base:np % 2 === 0 }; }))
      .then(function(vals){ return vals ? calc(ex, vals) : { err:'Đã hủy lấy giá trị trên bản vẽ.' }; });
  }
  // lấy giá trị trên bản vẽ: ẩn hộp thoại đang mở, chạy 'QUICKCALC trong suốt để có prompt và bắt điểm; Esc quay lại mà không hủy lệnh bên ngoài
  function pickSeq(list){
    return new Promise(function(res){
      var hid = $$('.dlg, .scrim').filter(function(x){ return x.style.visibility !== 'hidden'; }), key = function(e){ if (e.key !== 'Escape') return; e.preventDefault(); e.stopPropagation(); TC.cmdline.submit(''); };
      hid.forEach(function(x){ x.style.visibility = 'hidden'; }); document.addEventListener('keydown', key, true);
      TC.cmd.run("'QUICKCALC", { picks:list, done:function(v){ document.removeEventListener('keydown', key, true); hid.forEach(function(x){ x.style.visibility = ''; }); if (!hid.length) showQC(); res(v); } });
    });
  }

  /* ---------- quy đổi đơn vị ---------- */
  var UNITS = { len:['Độ dài', [['mm', 'Milimét', 1], ['cm', 'Xentimét', 10], ['dm', 'Đềximét', 100], ['m', 'Mét', 1e3], ['km', 'Kilômét', 1e6], ['in', 'Inch', 25.4], ['ft', 'Feet', 304.8], ['yd', 'Yard', 914.4], ['mi', 'Dặm', 1609344]], ['m', 'mm']],
    area:['Diện tích', [['mm²', 'Milimét vuông', 1], ['cm²', 'Xentimét vuông', 100], ['m²', 'Mét vuông', 1e6], ['ha', 'Hécta', 1e10], ['km²', 'Kilômét vuông', 1e12], ['sào', 'Sào Bắc Bộ, 360 m²', 3.6e8], ['công', 'Công Nam Bộ, 1.000 m²', 1e9], ['ft²', 'Feet vuông', 92903.04], ['ac', 'Mẫu Anh (acre)', 4046856422.4]], ['m²', 'sào']],
    vol:['Thể tích', [['mm³', 'Milimét khối', 1], ['cm³', 'Xentimét khối', 1e3], ['l', 'Lít', 1e6], ['m³', 'Mét khối', 1e9], ['ft³', 'Feet khối', 28316846.592], ['gal', 'Gallon Mỹ', 3785411.784]], ['m³', 'l']],
    ang:['Góc', [['°', 'Độ', 1], ['rad', 'Radian', 1 / RAD], ['grad', 'Grad', .9], ['%', 'Độ dốc theo %', 0]], ['°', '%']] };
  function conv(t, v, a, b){ var L = UNITS[t][1], f = function(c){ return L.filter(function(u){ return u[0] === c; })[0] || L[0]; }, A0 = f(a), B0 = f(b), x = A0[0] === '%' ? Math.atan(v / 100) / RAD : v * A0[2]; return B0[0] === '%' ? Math.tan(x * RAD) * 100 : x / B0[2]; }

  /* ---------- giao diện máy tính: palette (mode 'pal') hoặc hộp thoại tính cho một ô (mode 'for') ---------- */
  var PAD = [['C', 'clr', 'Xóa ô nhập'], ['⌫', 'bs', 'Xóa lùi'], ['√', 'sqrt(', 'Căn bậc hai'], ['÷', '/', 'Chia', 'op'], ['1/x', 'inv', 'Nghịch đảo'],
    ['7'], ['8'], ['9'], ['×', '*', 'Nhân', 'op'], ['x²', '^2', 'Bình phương'], ['4'], ['5'], ['6'], ['−', '-', 'Trừ', 'op'], ['x³', '^3', 'Lập phương'],
    ['1'], ['2'], ['3'], ['+', '+', 'Cộng', 'op'], ['xʸ', '^', 'Lũy thừa'], ['0'], ['.', '.', 'Dấu thập phân'], ['π', 'pi', 'Số pi'], ['('], [')'],
    ['MS', 'ms', 'Lưu kết quả vào bộ nhớ'], ['M+', 'mp', 'Cộng kết quả vào bộ nhớ'], ['MR', 'mr', 'Lấy số trong bộ nhớ'], ['MC', 'mc', 'Xóa bộ nhớ'], ['=', 'eq', 'Tính (Enter)', 'eq']];
  var SCI = ['sin', 'cos', 'tan', 'log', ['10ˣ', '10^', 'Mười mũ x'], 'asin', 'acos', 'atan', 'ln', ['eˣ', 'exp(', 'e mũ x'], 'r2d', 'd2r', 'abs', 'round', 'trunc'];
  function keys(list, sci){ return '<div class="qc-pad">' + list.map(function(k){ k = Array.isArray(k) ? k : [k, k + '(']; var v = k[1] || k[0], c = k[3] || (/^[\d.]$/.test(k[0]) ? 'num' : 'fn'); return '<button type="button" tabindex="-1" class="' + c + '" data-k="' + esc(v) + '"' + (k[2] ? ' data-tip="' + esc(k[2]) + '"' : '') + '>' + (k[1] === 'bs' ? ic('arrowL', 's12') : esc(k[0])) + '</button>'; }).join('') + '</div>' + (sci ? '<p class="qc-note">Góc tính bằng độ; r2d, d2r đổi giữa radian và độ.</p>' : ''); }
  function qcHTML(mode, tgt){
    return '<div class="pal-tools">' + ib('clr', 'erase', 'Xóa ô nhập') + ib('clh', 'trash', 'Xóa lịch sử') + '<i class="vsep"></i>' +
      ib('pt', 'idpoint', 'Lấy tọa độ điểm', ' data-desc="Chỉ một điểm trên bản vẽ (có bắt điểm); tọa độ [x, y] vào ô biểu thức."') + ib('dist', 'dist', 'Khoảng cách giữa hai điểm') + ib('ang', 'angle', 'Góc của đường qua hai điểm', ' data-desc="Đo từ trục X, ngược chiều kim đồng hồ, bằng độ."') + ib('ill', 'inters', 'Giao điểm của hai đường qua bốn điểm') +
      '<span class="grow"></span>' + (mode === 'pal' ? ib('cl', 'keyboard', 'Dán kết quả vào dòng lệnh') : '') + '</div>' +
      '<div class="qc-body"><div class="qc-hist" role="list" aria-label="Lịch sử tính"></div><div class="qc-in">' +
      (tgt ? '<div class="qc-for">' + ic('calc', 's12') + '<span>Tính cho ô <b>' + esc(tgt.label) + '</b>' + (tgt.value ? ' · đang là <span class="mono">' + esc(tgt.value) + '</span>' : '') + '</span></div>' : '') +
      '<input class="inp mono" data-qi aria-label="Biểu thức" spellcheck="false" autocomplete="off" placeholder="Biểu thức, ví dụ (3900-220)/2">' +
      '<div class="qc-res"><span data-qr></span><span class="grow"></span><span class="tag" data-qm hidden></span></div></div>' + (mode === 'pal' ? '<div class="qc-act"></div>' : '') + '<div class="qc-secs"></div></div>';
  }
  function hist(root){ var h = $('.qc-hist', root); h.innerHTML = QC.hist.length ? QC.hist.map(function(x, k){ return '<button class="qc-h" type="button" role="listitem" data-h="' + k + '"><span class="ex">' + esc(x[0]) + '</span><span class="r">= ' + esc(x[1]) + '</span></button>'; }).join('') : '<p class="qc-e">Lịch sử trống. Gõ biểu thức rồi Enter, ví dụ <span class="mono">3600*2+450</span> hoặc <span class="mono">2.5m+300</span>.</p>'; h.scrollTop = h.scrollHeight; }
  function secs(root){
    var o = QC.open, sec = function(k, name, body){ return '<button class="pgh" type="button" data-qs="' + k + '" aria-expanded="' + !!o[k] + '">' + ic('down', 'xs') + name + '</button>' + (o[k] ? body() : ''); };
    $('.qc-secs', root).innerHTML = sec('pad', 'Bàn phím số', function(){ return keys(PAD); }) + sec('sci', 'Hàm khoa học', function(){ return keys(SCI, 1); }) + sec('unit', 'Quy đổi đơn vị', unitHTML) + sec('var', 'Biến', varHTML);
    unitCalc(root);
  }
  function unitHTML(){
    var L = UNITS[QC.ut][1], op = function(c){ return L.map(function(u){ return '<option value="' + esc(u[0]) + '"' + (u[0] === c ? ' selected' : '') + '>' + esc(u[0] + ' · ' + u[1]) + '</option>'; }).join(''); };
    return '<div class="qc-unit"><span class="lab">Loại</span><select class="inp" data-qu="t" aria-label="Loại đơn vị">' + Object.keys(UNITS).map(function(k){ return '<option value="' + k + '"' + (k === QC.ut ? ' selected' : '') + '>' + UNITS[k][0] + '</option>'; }).join('') + '</select><span></span>' +
      '<span class="lab">Từ</span><select class="inp" data-qu="a" aria-label="Đổi từ">' + op(QC.ua) + '</select><span></span>' +
      '<span class="lab">Sang</span><select class="inp" data-qu="b" aria-label="Đổi sang">' + op(QC.ub) + '</select>' + ib('swap', 'swap', 'Đổi chiều quy đổi') +
      '<span class="lab">Giá trị</span><input class="inp mono" data-qu="v" aria-label="Giá trị cần đổi" value="' + esc(QC.uv) + '" spellcheck="false" autocomplete="off"><span></span>' +
      '<span class="lab">Kết quả</span><input class="inp mono" data-qu="r" aria-label="Kết quả quy đổi" readonly>' + ib('uins', 'arrowR', 'Đưa kết quả vào ô biểu thức') + '</div>';
  }
  function unitCalc(root){ var r = $('[data-qu="r"]', root); if (!r) return; var s = $('[data-qu="v"]', root).value.trim(), v = calc(s); r.value = !s || v.err || Array.isArray(v.v) ? '' : fmt(conv(QC.ut, v.v, QC.ua, QC.ub)); $('[data-qu="v"]', root).classList.toggle('err', !!s && (!!v.err || Array.isArray(v.v))); }
  function varHTML(){
    var gs = [], cur = varOf(QC.vs || ''); QC.vars.forEach(function(v){ if (gs.indexOf(v[3]) < 0) gs.push(v[3]); });
    return '<div class="qc-var"><div class="pal-tools">' + ib('vnew', 'plus', 'Biến mới') + ib('ved', 'bedit', 'Sửa biến', cur ? '' : ' disabled') + ib('vdel', 'trash', 'Xóa biến', cur ? '' : ' disabled') + '<span class="grow"></span>' + ib('vins', 'arrowR', 'Đưa biến vào ô biểu thức', cur ? '' : ' disabled') + '</div>' +
      '<div class="tree" role="listbox" aria-label="Biến">' + gs.map(function(g){ return '<div class="qc-vg">' + esc(g) + '</div>' + QC.vars.filter(function(v){ return v[3] === g; }).map(function(v){ return '<button class="tn" type="button" role="option" data-v="' + esc(v[0]) + '" aria-selected="' + (v === cur) + '"><span class="mono">' + esc(v[0]) + '</span><span class="cnt">' + esc(v[1] === 'f' ? v[2] : '= ' + v[2]) + '</span></button>'; }).join(''); }).join('') + '</div>' +
      '<div class="qc-vd">' + (cur ? '<b class="mono">' + esc(cur[0]) + '</b> · ' + esc(cur[4] || '') + '<br>' + (cur[1] === 'f' ? 'Hàm ' + esc(cur[2]) + ': khi tính sẽ hỏi chỉ trên bản vẽ.' : 'Hằng số = ' + esc(cur[2])) : 'Nhấp đúp một biến để đưa vào ô biểu thức. Trong biểu thức, cur lấy một điểm, rad lấy bán kính trên bản vẽ.') + '</div></div>';
  }
  function act(root){
    var a = $('.qc-act', root), p = QC.prop; if (!a) return;
    a.innerHTML = '<button class="btn sm" type="button" data-q="cl">Dán vào dòng lệnh</button><button class="btn sm" type="button" data-q="prop"' + (p ? '' : ' disabled') + '>Áp vào Thuộc tính</button>' +
      '<span class="faint">' + (p ? 'Ô đang sửa: <b>' + esc(p.key) + '</b> · ' + esc(p.type) : 'Bấm một ô số trong palette Thuộc tính để áp kết quả vào ô đó.') + '</span>';
  }
  function prev(root, err){
    if (!root) return; var i = $('[data-qi]', root), r = $('[data-qr]', root), m = $('[data-qm]', root), s = i.value.trim(); QC.x = i.value;
    m.hidden = !QC.mem; m.textContent = 'M = ' + fmt(QC.mem); i.classList.toggle('err', !!err);
    if (err){ r.className = 'fmsg err'; r.innerHTML = ic('warn', 'xs') + esc(err); return; }
    r.className = ''; if (!s){ r.innerHTML = ''; return; }
    var ex = expand(s), nd = needs(ex), v = nd.length ? null : calc(ex);
    r.innerHTML = nd.length ? '<span class="faint qc-hint">Enter để chỉ ' + nd.length + ' giá trị trên bản vẽ</span>' : v.err ? '' : '<span class="faint">= ' + esc(fmt(v.v)) + '</span>';
  }
  function live(root){ return document.contains(root) ? root : root.dataset.mode === 'pal' ? $('.qc[data-mode="pal"]') : null; }
  function ins(root, s){ var i = $('[data-qi]', root), a = i.selectionStart, b = i.selectionEnd; i.setRangeText(s, a == null ? i.value.length : a, b == null ? i.value.length : b, 'end'); i.focus(); prev(root); }
  // tính: Enter, nút =, hoặc trước khi đưa kết quả đi; lịch sử giữ 40 dòng
  function run(root){
    var i = $('[data-qi]', root), s = i.value.trim(); if (!s) return Promise.resolve(null);
    return evalAsk(s).then(function(r){
      root = live(root); if (!root) return null; i = $('[data-qi]', root);
      if (r.err){ prev(root, r.err); i.focus(); return null; }
      var res = fmt(r.v); QC.ans = r.v; QC.last = res;
      if (s !== res){ QC.hist.push([s, res]); QC.hist = QC.hist.slice(-40); $$('.qc').forEach(hist); }
      i.value = res; prev(root); qSave(); i.focus(); i.select(); return r.v;
    });
  }
  function key(root, k){
    var i = $('[data-qi]', root);
    if (k === 'clr'){ i.value = ''; prev(root); }
    else if (k === 'bs'){ var a = i.selectionStart, b = i.selectionEnd; if (a === b && a > 0) a--; i.setRangeText('', a, b, 'end'); prev(root); }
    else if (k === 'inv'){ i.value = i.value.trim() ? '1/(' + i.value.trim() + ')' : '1/('; prev(root); }
    else if (k === 'eq') return run(root);
    else if (k === 'ms' || k === 'mp') return run(root).then(function(v){ if (v == null) return; if (Array.isArray(v)){ prev(live(root), 'Bộ nhớ chỉ giữ một số.'); return; } QC.mem = k === 'ms' ? v : QC.mem + v; qSave(); prev(live(root)); });
    else if (k === 'mr') ins(root, fmt(QC.mem));
    else if (k === 'mc'){ QC.mem = 0; qSave(); prev(root); }
    else ins(root, k);
    i.focus();
  }
  // bốn nút lấy từ bản vẽ: tọa độ, khoảng cách, góc, giao điểm hai đường qua bốn điểm
  function grab(root, k){
    if (TC.cad.mode() !== 'idle'){ prev(root, 'Lệnh ' + (TC.cmdline.current() || '') + ' đang chờ trên bản vẽ. Kết thúc lệnh (Esc) rồi lấy giá trị.'); return; }
    var L = k === 'pt' ? [{ k:'pt', text:'Chỉ định điểm để lấy tọa độ' }] : k === 'ill' ? [{ k:'pt', text:'Điểm đầu của đường thứ nhất' }, { k:'pt', text:'Điểm cuối của đường thứ nhất', base:1 }, { k:'pt', text:'Điểm đầu của đường thứ hai', line:1 }, { k:'pt', text:'Điểm cuối của đường thứ hai', base:1, line:1 }] : [{ k:'pt', text:'Chỉ định điểm thứ nhất' }, { k:'pt', text:'Chỉ định điểm thứ hai', base:1 }];
    pickSeq(L).then(function(p){ root = live(root); if (!root || !p) return; var v;
      try { v = k === 'pt' ? p[0] : k === 'dist' ? G.dist(p[0], p[1]) : k === 'ang' ? (G.ang(p[0], p[1]) + 360) % 360 : ill(p[0], p[1], p[2], p[3]); } catch (m){ prev(root, m); return; }
      ins(root, fmt(v).replace(/\s/g, '')); });
  }
  // dán vào dòng lệnh: tọa độ [x, y] thành x,y để dùng ngay khi lệnh hỏi điểm
  function toCmd(root){
    var go = function(){ var r = live(root), v = r && $('[data-qi]', r).value.trim(), c = $('#cmd-in'); if (!v || !c) return; c.value = v.replace(/^\[|\]$/g, '').replace(/\s/g, ''); c.focus(); c.setSelectionRange(c.value.length, c.value.length); TC.cmdline.msg('Đã dán <span class="mono">' + esc(c.value) + '</span> vào dòng lệnh. Enter để nhập.', 'faint'); };
    var s = $('[data-qi]', root).value.trim(); if (!s){ prev(root, 'Chưa có kết quả để dán.'); return; }
    var ex = expand(s), r = needs(ex).length ? null : calc(ex); if (r && !r.err && fmt(r.v) === s) go(); else run(root).then(function(v){ if (v != null) go(); });
  }
  function bind(root, mode){
    root.dataset.mode = mode;
    root.addEventListener('mousedown', function(e){ if (e.target.closest('[data-k]')) e.preventDefault(); }); // bấm phím số không lấy mất con trỏ của ô biểu thức
    root.addEventListener('click', function(e){
      var k = e.target.closest('[data-k]'); if (k){ key(root, k.dataset.k); return; }
      var h = e.target.closest('[data-h]'); if (h){ var x = QC.hist[Number(h.dataset.h)], i0 = $('[data-qi]', root); if (x){ i0.value = x[0]; prev(root); i0.focus(); } return; }
      // vẽ lại sau khi sự kiện bấm xong: nút vừa bấm còn trong palette nên palette bay ra (màn hình hẹp) không tự thu lại
      var s = e.target.closest('[data-qs]'); if (s){ QC.open[s.dataset.qs] = !QC.open[s.dataset.qs]; qSave(); setTimeout(function(){ secs(root); var b = $('[data-qs="' + s.dataset.qs + '"]', root); if (b) b.focus(); }, 0); return; }
      var v = e.target.closest('[data-v]'); if (v){ QC.vs = v.dataset.v; setTimeout(function(){ secs(root); var nv = $('[data-v="' + v.dataset.v + '"]', root); if (nv) nv.focus(); }, 0); return; }
      var q = e.target.closest('[data-q]'); if (!q || q.disabled) return; var c = q.dataset.q, inp = $('[data-qi]', root);
      if (c === 'clr'){ inp.value = ''; prev(root); inp.focus(); }
      else if (c === 'clh'){ var old = QC.hist; if (!old.length) return; QC.hist = []; qSave(); hist(root); TC.toast('Đã xóa ' + old.length + ' dòng lịch sử máy tính.', { log:false, act:{ label:'Hoàn tác', fn:function(){ QC.hist = old; qSave(); $$('.qc').forEach(hist); } } }); }
      else if (/^(pt|dist|ang|ill)$/.test(c)) grab(root, c);
      else if (c === 'cl') toCmd(root);
      else if (c === 'prop') run(root).then(function(r1){ if (r1 == null || !QC.prop) return; var e1 = propTgt(QC.prop.key, '').apply(r1); if (e1) prev(live(root), e1); });
      else if (c === 'swap'){ var t = QC.ua; QC.ua = QC.ub; QC.ub = t; qSave(); setTimeout(function(){ secs(root); var b = $('[data-q="swap"]', root); if (b) b.focus(); }, 0); }
      else if (c === 'uins'){ var ur = $('[data-qu="r"]', root).value; if (ur) ins(root, ur); }
      else if (c === 'vnew' || c === 'ved') varDlg(c === 'ved' ? QC.vs : null, root);
      else if (c === 'vdel') varDel(QC.vs, root);
      else if (c === 'vins' && QC.vs) ins(root, QC.vs);
    });
    root.addEventListener('dblclick', function(e){ var v = e.target.closest('[data-v]'); if (v) ins(root, v.dataset.v); });
    root.addEventListener('contextmenu', function(e){
      var h = e.target.closest('[data-h]'); if (!h) return; e.preventDefault(); var n = Number(h.dataset.h), x = QC.hist[n]; if (!x) return;
      TC.menu([{ label:'Đưa biểu thức vào ô nhập', act:function(){ ins(root, x[0]); } }, { label:'Đưa kết quả vào ô nhập', act:function(){ ins(root, x[1].replace(/\s/g, '')); } }, { label:'Sao chép kết quả', icon:'copyclip', act:function(){ TC.copyText(x[1]); } },
        mode === 'pal' ? { label:'Dán kết quả vào dòng lệnh', icon:'keyboard', act:function(){ $('[data-qi]', root).value = x[1]; toCmd(root); } } : null, { sep:1 },
        { label:'Xóa dòng này', act:function(){ QC.hist.splice(n, 1); qSave(); hist(root); } }, { label:'Xóa lịch sử', icon:'trash', act:function(){ $('[data-q="clh"]', root).click(); } }], e.clientX, e.clientY);
    });
    root.addEventListener('input', function(e){ var t = e.target; if (t.matches('[data-qi]')){ root._hi = null; prev(root); } else if (t.matches('[data-qu="v"]')){ QC.uv = t.value; qSave(); unitCalc(root); } });
    root.addEventListener('change', function(e){ var u = e.target.dataset && e.target.dataset.qu; if (!u || u === 'v' || u === 'r') return; if (u === 't'){ QC.ut = e.target.value; QC.ua = UNITS[QC.ut][2][0]; QC.ub = UNITS[QC.ut][2][1]; } else QC['u' + u] = e.target.value; qSave(); secs(root); var f = $('[data-qu="' + u + '"]', root); if (f) f.focus(); });
    root.addEventListener('keydown', function(e){
      var t = e.target;
      if (t.matches('[data-v]') && e.key === 'Enter'){ e.preventDefault(); e.stopPropagation(); ins(root, t.dataset.v); return; }
      if (!t.matches('[data-qi]')) return;
      if (e.key === 'Enter'){ if (mode === 'for' && t.value.trim() === QC.last) return; e.preventDefault(); e.stopPropagation(); run(root); } // trong hộp "tính cho ô": Enter lần hai (đã ra kết quả) để áp dụng
      else if (e.key === 'Escape' && mode === 'pal' && t.value){ e.preventDefault(); e.stopPropagation(); t.value = ''; prev(root); }
      else if (e.key === 'ArrowUp' || e.key === 'ArrowDown'){ e.preventDefault(); var n = QC.hist.length; if (!n) return; root._hi = TC.clamp((root._hi == null ? n : root._hi) + (e.key === 'ArrowUp' ? -1 : 1), 0, n); t.value = root._hi >= n ? '' : QC.hist[root._hi][0]; prev(root); }
    });
  }
  function draw(root, mode, tgt){ root.innerHTML = qcHTML(mode, tgt); bind(root, mode); hist(root); secs(root); act(root); $('[data-qi]', root).value = tgt ? tgt.value || '' : QC.x || ''; prev(root); }
  TC.PALETTES.qcalc = { name:'Máy tính nhanh', hidden:function(){ return TC.isViewer(); }, render:function(host){
    var root = $('.qc', host); if (root && root._b){ act(root); return; } // palette khác làm mới thì giữ nguyên ô đang gõ
    host.innerHTML = '<div class="qc"></div>'; root = $('.qc', host); root._b = 1; draw(root, 'pal');
  } };
  // mở palette (dock trái mặc định để cùng thấy palette Thuộc tính); màn hình hẹp thì mở phần bay ra
  function showQC(){
    var a = A(); if (a.pal.left.indexOf('qcalc') < 0 && a.pal.right.indexOf('qcalc') < 0) a.pal.left.push('qcalc');
    var vis = $('.palhost:not([hidden]) .qc[data-mode="pal"]'); if (vis) return vis;
    a.showPalette('qcalc'); var f = $('.dock.flyout [data-fly="qcalc"]'), h = f && f.closest('.dock').querySelector('.palhost'); if (f && h && h.hidden) f.click();
    return $('.qc[data-mode="pal"]');
  }
  // biến người dùng: hằng số (giá trị, biểu thức) hoặc hàm lấy từ bản vẽ (có cur, rad)
  function varDlg(name, root){
    var v = name ? varOf(name) : null, kind = v ? v[1] : 'c', gs = QC.vars.map(function(x){ return x[3]; }).filter(function(g, k, a){ return a.indexOf(g) === k; });
    var dlg = TC.dialog({ id:'qcvar', title:v ? 'Sửa biến' : 'Biến mới', cmd:'QUICKCALC', code:'UI-I10', w:470,
      body:'<div class="stack">' + H.row('Loại', SC.seg('qcvk', ['Hằng số', 'Hàm lấy từ bản vẽ'], kind === 'f' ? 'Hàm lấy từ bản vẽ' : 'Hằng số'), 92) +
        H.row('<label for="qv-n">Tên</label>', '<input class="inp mono" id="qv-n" value="' + esc(v ? v[0] : '') + '" style="width:200px" spellcheck="false" autocomplete="off" placeholder="ví dụ h_tang">', 92) +
        H.row('<label for="qv-g">Nhóm</label>', '<input class="inp" id="qv-g" list="qv-gl" value="' + esc(v ? v[3] : 'Của tôi') + '" style="width:200px" autocomplete="off"><datalist id="qv-gl">' + gs.map(function(g){ return '<option value="' + esc(g) + '">'; }).join('') + '</datalist>', 92) +
        H.row('<label for="qv-v" id="qv-vl"></label>', '<input class="inp mono grow" id="qv-v" value="' + esc(v ? v[2] : '') + '" spellcheck="false" autocomplete="off">', 92) +
        H.row('<label for="qv-d">Mô tả</label>', '<input class="inp grow" id="qv-d" value="' + esc(v ? v[4] : '') + '">', 92) + '<span class="fmsg hint" id="qv-m"></span></div>',
      foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="qv-ok">' + (v ? 'Lưu biến' : 'Tạo biến') + '</button>', focus:'#qv-n' });
    var m = $('#qv-m', dlg.el), hint = function(){ $('#qv-vl', dlg.el).textContent = kind === 'f' ? 'Biểu thức' : 'Giá trị'; m.className = 'fmsg hint'; m.textContent = kind === 'f' ? 'cur lấy một điểm, rad lấy bán kính khi tính. Ví dụ: dist(cur,cur)/2' : 'Số hoặc biểu thức, ví dụ 3600 hoặc 2*1.8m'; };
    var bad = function(t, id){ m.className = 'fmsg err'; m.innerHTML = ic('warn', 'xs') + esc(t); var f = $(id || '#qv-v', dlg.el); f.classList.add('err'); f.focus(); };
    hint(); dlg.el.addEventListener('segchange', function(e){ kind = e.detail === 'Hằng số' ? 'c' : 'f'; hint(); });
    dlg.el.addEventListener('input', function(e){ e.target.classList.remove('err'); });
    $('#qv-ok', dlg.el).addEventListener('click', function(){
      var n = $('#qv-n', dlg.el).value.trim(), x = $('#qv-v', dlg.el).value.trim(), ex = expand(x), nd = needs(ex), dup = varOf(n);
      if (!/^[A-Za-z_]\w*$/.test(n)) return bad('Tên bắt đầu bằng chữ cái không dấu, chỉ gồm chữ, số và dấu gạch dưới.', '#qv-n');
      if (FN[n.toLowerCase()] || /^(pi|e|ans|cur|rad)$/i.test(n)) return bad('“' + n + '” là tên có sẵn của máy tính. Chọn tên khác.', '#qv-n');
      if (dup && dup !== v) return bad('Đã có biến “' + dup[0] + '”.', '#qv-n');
      if (!x) return bad(kind === 'f' ? 'Nhập biểu thức.' : 'Nhập giá trị.');
      if (kind === 'c' && nd.length) return bad('Hằng số không lấy giá trị trên bản vẽ. Chọn loại Hàm lấy từ bản vẽ.');
      if (kind === 'f' && !nd.length) return bad('Hàm cần ít nhất một cur hoặc rad. Nếu không, chọn loại Hằng số.');
      var r = calc(ex, nd.map(function(k){ return k === 'rad' ? 1 : [0, 0]; })); if (r.err && !/song song|chia cho 0|không xác định/.test(r.err)) return bad(r.err);
      var rec = [n, kind, x, $('#qv-g', dlg.el).value.trim() || 'Của tôi', $('#qv-d', dlg.el).value.trim()];
      if (v) QC.vars[QC.vars.indexOf(v)] = rec; else QC.vars.push(rec); QC.vs = n; qSave(); dlg.close(true);
      $$('.qc').forEach(function(r2){ secs(r2); }); var rr = live(root); if (rr) prev(rr);
    });
  }
  function varDel(name, root){ var v = varOf(name || ''); if (!v) return; TC.confirm({ title:'Xóa biến', msg:'Xóa biến <b class="mono">' + esc(v[0]) + '</b>?', detail:'Biểu thức nào đang dùng biến này sẽ báo lỗi.', ok:'Xóa biến', danger:true }).then(function(y){ if (!y) return; QC.vars = QC.vars.filter(function(x){ return x !== v; }); QC.vs = null; qSave(); $$('.qc').forEach(function(r){ secs(r); }); }); }

  /* ---------- tính cho một ô: ô số UI-C09 trong hộp thoại, ô số đang sửa của palette Thuộc tính ---------- */
  SC.qcalcFor = function(tgt){
    if (!TC.can('PRO')){ SC.upgrade('Máy tính nhanh', 'PRO'); return; }
    var dlg = TC.dialog({ id:'qcf', title:'Máy tính nhanh', cmd:'QUICKCALC', code:'UI-I10', w:316, flush:true, body:'<div class="qc"></div>', focus:'[data-qi]',
      foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="qcf-ok">Áp dụng</button>' });
    var root = $('.qc', dlg.el); if (root._b) return dlg; root._b = 1; draw(root, 'for', tgt); QC.last = $('[data-qi]', root).value.trim();
    dlg.el.style.top = Math.max(12, (innerHeight - dlg.el.offsetHeight) / 2.4) + 'px'; // đặt lại vị trí khi đã có nội dung
    setTimeout(function(){ $('[data-qi]', root).select(); }, 0);
    $('#qcf-ok', dlg.el).addEventListener('click', function(){ run(root).then(function(v){ if (v == null){ if (!$('[data-qi]', root).value.trim()) prev(root, 'Nhập số hoặc biểu thức.'); return; } var e = tgt.apply(v); if (e){ prev(root, e); return; } dlg.close(true); TC.cmdline.msg('Máy tính nhanh: đã đưa <span class="mono">' + esc(fmt(v)) + '</span> vào ô ' + esc(tgt.label) + '.', 'faint'); }); });
    return dlg;
  };
  function labOf(box){ var i = $('input', box), r = box.closest('.hrow'), f = box.closest('.field'), p = r && r.previousElementSibling; return ((i.getAttribute('aria-label') || '') || (p && p.classList.contains('lab') ? p.textContent : '') || (f && $('label', f) ? $('label', f).textContent : '')).trim() || 'số'; }
  function numTgt(box){
    var i = $('input', box), lab = labOf(box);
    return { label:lab, value:i.value, apply:function(v){ if (Array.isArray(v)) return 'Ô ' + lab + ' cần một số; kết quả đang là tọa độ.'; var mn = box.dataset.min; if (mn !== '' && v <= Number(mn)) return lab + ' phải lớn hơn ' + mn + '.'; i.value = fmt(v); TC.numCommit(box); i.dispatchEvent(new Event('input', { bubbles:true })); setTimeout(function(){ if (document.activeElement === i){ i.setSelectionRange(0, 0); i.scrollLeft = 0; } }, 0); return null; } };
  }
  function propTgt(key, val){ return { label:key, value:val, apply:function(v){ if (Array.isArray(v)) return 'Ô ' + key + ' cần một số; kết quả đang là tọa độ.'; return applyProp(key, v) ? null : 'Ô “' + key + '” không còn trong Thuộc tính (vùng chọn đã đổi). Bấm lại ô cần sửa.'; } }; }
  // áp vào Thuộc tính đúng như người dùng sửa: mở ô, gõ giá trị, Enter (một bước hoàn tác)
  function applyProp(key, v){
    var find = function(){ return $$('#pal-right .pr.edt, #pal-left .pr.edt').filter(function(r){ return r.dataset.pk === key; })[0]; }, row = find();
    if (!row){ A().showPalette('props'); row = find(); } if (!row) return false;
    row.click(); var i = $('.pinp', row); if (!i) return false; i.value = fmt(v); i.dispatchEvent(new KeyboardEvent('keydown', { key:'Enter', bubbles:true })); return true;
  }
  function pcalc(row){ var i = row && $('.pinp', row); if (!i) return; var k = row.dataset.pk, val = i.value; i.dispatchEvent(new KeyboardEvent('keydown', { key:'Escape', bubbles:true })); SC.qcalcFor(propTgt(k, val)); }
  document.addEventListener('mousedown', function(e){ if (e.target.closest && e.target.closest('.numf [data-calc], [data-pcalc]')) e.preventDefault(); }, true);
  // bắt ở pha capture: palette Thuộc tính tự mở lại ô khi bấm trong hàng, nên chặn trước khi tới hàng
  document.addEventListener('click', function(e){
    var b = e.target.closest && e.target.closest('.numf [data-calc]'); if (b){ var box = b.closest('[data-num]'); if (!TC.can('PRO')) SC.upgrade('Máy tính nhanh', 'PRO'); else if (!box.classList.contains('is-dis')) SC.qcalcFor(numTgt(box)); return; }
    var p = e.target.closest && e.target.closest('[data-pcalc]'); if (p){ e.stopPropagation(); e.preventDefault(); pcalc(p.closest('.pr.edt')); }
  }, true);
  // ô số đang sửa trong Thuộc tính: nhớ để nút "Áp vào Thuộc tính" dùng, và gắn nút máy tính vào ô
  document.addEventListener('focusin', function(e){
    var t = e.target, row = t.classList && t.classList.contains('pinp') && t.classList.contains('mono') && t.closest('.pr.edt'); if (!row || !t.closest('#pal-right, #pal-left')) return;
    var d = D(), e0 = d && d.byId[d.sel[0]]; QC.prop = { key:row.dataset.pk, sel:d ? d.sel.join() : '', type:e0 ? (TC.TYPE_NAMES[e0.t] || e0.t) + (d.sel.length > 1 ? ' (' + d.sel.length + ')' : '') : 'đối tượng vẽ mới' }; $$('.qc[data-mode="pal"]').forEach(act);
    if (TC.can('PRO') && !$('[data-pcalc]', t.parentNode)) t.parentNode.appendChild(TC.el('<button type="button" class="ib sm pcalc" data-pcalc tabindex="-1" aria-label="Máy tính nhanh" data-tip="Máy tính nhanh" data-cmd="QUICKCALC" data-key="Ctrl+8">' + ic('calc') + '</button>'));
  });
  TC.on('sel', function(){ var d = D(); if (QC.prop && (!d || d.sel.join() !== QC.prop.sel)){ QC.prop = null; $$('.qc[data-mode="pal"]').forEach(act); } });
  // Ctrl+8 trong ô số mở máy tính cho ô đó; trong ô biểu thức của palette thì ẩn palette
  document.addEventListener('keydown', function(e){
    if (!(e.ctrlKey || e.metaKey) || e.key !== '8' || !e.target.closest) return; var t = e.target, box = t.closest('[data-num]');
    if (box && t.tagName === 'INPUT' && !t.readOnly){ e.preventDefault(); e.stopPropagation(); if (!TC.can('PRO')) SC.upgrade('Máy tính nhanh', 'PRO'); else SC.qcalcFor(numTgt(box)); }
    else if (t.classList.contains('pinp') && $('[data-pcalc]', t.parentNode)){ e.preventDefault(); e.stopPropagation(); pcalc(t.closest('.pr.edt')); }
    else if (t.matches('.qc[data-mode="pal"] [data-qi]')){ e.preventDefault(); e.stopPropagation(); TC.cmd.run('QUICKCALC'); }
  }, true);
  IMPL.QUICKCALC = function(ctx, pre){
    if (pre && pre.picks){ // lấy điểm, bán kính cho máy tính (gọi trong suốt từ pickSeq)
      var out = []; ctx.badge('QC');
      return pre.picks.reduce(function(p, q){ return p.then(function(ok){
          if (!ok) return false;
          if (q.k === 'rad') return ED.pick(ctx, q.text, { types:['circle', 'arc'], bad:'Chọn đường tròn hoặc cung', locked:true }).then(function(r){ if (!r || !r.e) return false; out.push(r.e.r); return true; });
          var b = q.base ? out[out.length - 1] : null, ln = q.line ? [{ k:'line', a:out[0], b:out[1], color:TC.cssv('--c-accent'), w:1.4 }] : [];
          return ctx.point(q.text, { base:b, rubber:!!b, temp:ln }).then(function(r){ if (!r || !r.p) return false; out.push(r.p); return true; }); }); }, Promise.resolve(true))
        .then(function(ok){ V().temp = []; V().drawOver(); pre.done(ok ? out : null); }, function(){ pre.done(null); });
    }
    var vis = $('.palhost:not([hidden]) .qc[data-mode="pal"]');
    if (!vis){ showQC(); ctx.out('Máy tính nhanh: gõ biểu thức rồi Enter; ' + CTRL() + '+8 để ẩn, hiện.'); return; }
    if (vis.closest('.dock').classList.contains('flyout')) vis.closest('.palhost').hidden = true; else A().showPalette('qcalc', true);
  };
  OPEN['UI-I10'] = function(){
    var d = SC.cat.plan(), c = d.ents.filter(function(e){ return e.t === 'circle' && e.layer === 'KT-TBVS'; })[0];
    if (c){ d.sel = [c.id]; TC.cad.afterSel(); QC.prop = { key:'Bán kính', sel:d.sel.join(), type:'Circle' }; }
    QC.open.pad = 1; QC.x = '(3900-220)/2'; var root = $('.qc[data-mode="pal"]'); if (root) root.parentNode.innerHTML = '';
    showQC();
    TC.toast('Rê chuột vào một ô số trong hộp thoại, hoặc bấm một ô số trong Thuộc tính: nút máy tính hiện ra để tính riêng cho ô đó.', { type:'info', log:false, ms:9000, act:c ? { label:'Thử với ô Bán kính', fn:function(){ SC.qcalcFor(propTgt('Bán kính', TC.d(c.r))); } } : null });
  };

  /* ================= Kiểm tra chính tả UI-F10 (SPELL) ================= */
  var SPDEF = { lang:'vien', tone:'old', caps:true, num:true, web:true, legacy:false, acc:true, telex:true, dim:true, att:true, tbl:true, xref:false, auto:true };
  var SP = { set:Object.assign({}, SPDEF, TC.load('spell', {})), user:TC.load('spelluser', ['TrionCAD', 'Lavabo', 'Vinyl']), ign:{} };
  function spSave(){ TC.save('spell', SP.set); TC.save('spelluser', SP.user); }
  var LANGS = [['vien', 'Tiếng Việt + Tiếng Anh (Mỹ)'], ['vi', 'Tiếng Việt'], ['en', 'Tiếng Anh (Mỹ)']];
  function langName(){ return LANGS.filter(function(l){ return l[0] === SP.set.lang; })[0][1]; }
  // từ điển mẫu (dấu kiểu cũ): xếp hạng gợi ý; tiếng Việt hợp lệ được nhận theo luật ghép vần, không cần có trong danh sách
  var VI = {}; ('a ai an anh ảnh áo ăn âm ẩm ấy bà bài bàn bản bảng bao báo bảo bậc bằng bắc bê bể bên bếp bị biên biến biết bình bó bỏ bố bộ bồn bông bởi bơm bức bước cả các cách cái cảm can cạnh cánh cao cáp cát cầu cấp cắt cây chân chất chỉ chi chia chiếu chiều chịu chính cho chỗ chọn chống chồng chú chủ chung chuẩn chữ chưa chứa chức có cọc con công cổ cốt cột cơ cũ của cùng cuối cửa cứng cường ' +
    'dài dàn dán dạng dày dầm dẫn dây dọc dốc dụng dùng dưới dương đá đã đai đặt đất đầu đây để đến đèn đi điểm điện điều định đó đỏ độ đổ đơn đối đúng được đường gác gạch gần gia giá giải giằng giao gian giếng giữ giữa ghép ghi gỗ hai hàn hàng hành hạng hầm hệ hiện hình hộ hộp hòa hoàn hoạt họa hơn hở hướng in kê kết khác khách khi khí kho khoan khoảng khối khô không khung kích kiểm kim kính kỹ ký ' +
    'là lại làm lan lát lắp lấy lên lề lệ liên lối lớn lớp lợp lực lưới lượng mã mái màu mạch mặt máy mép mét mỗi mới móng một mức nam nào nắp nặng nằm nền nét ngang ngăn ngầm ngoài ngói ngủ người nguyên nhà nhau nhôm nhỏ nhựa những nóc nối nội nước ô ở ốp ống phải phần phân phẳng phía phòng phơi phụ phủ quy ra rào rãnh riêng rộng sách sạch sai sàn sảnh sát sau sắt sân sẽ sinh số sổ sơn sử ' +
    'tác tại tạm tầng tắm tấm tâm tận thang thanh thành thay thạch thấm thấp theo thép thế thể thiết thiện thoát thông thống thờ thủy thứ thước thường tiết tiếp tim tính tình tỉ tĩnh tô tôn tông tổng trái trát trần trên trệt trì trình trong trục trụ tra tráng trước tuyến từ tủ tường tự vách và vào văn vật vẽ về vệ việc viên vị vòi với vữa vườn xà xây xem xi xử xuống ý').split(' ').forEach(function(w, k){ VI[w] = k + 1; });
  var COMPF = {}; 'phòng khách|phòng ngủ|phòng tắm|phòng ăn|nhà vệ sinh|vệ sinh|cầu thang|ban công|mặt bằng|mặt cắt|mặt đứng|tường bao|tường ngăn|cao độ|kích thước|bê tông|cốt thép|cửa đi|cửa sổ|ghi chú|hoàn thiện|chống thấm|điều hòa|lan can|sân thượng|mái tôn|trần thạch cao|thạch cao|gạch lát|thép hộp|sơn nước|bản vẽ|thống kê|chi tiết|trục định vị|tầng trệt|nhà bếp|sảnh chính|lối đi|bồn cầu'.split('|').forEach(function(c){ COMPF[TC.fold(c)] = c; });
  var EN = {}; ('loggia lavabo garage villa toilet level plan section detail scale note notes door window wall floor roof stair stairs up down room living bedroom kitchen dining bath bathroom balcony terrace concrete steel rebar beam column slab foundation grid axis elevation drawing title date revision project owner client designer drawn checked approved sheet total area length width height thickness typical ' +
    'see refer to and of the for with by at on in is not all dimension dimensions mm cm dm km kg kn mpa gpa kw hz ml cad dwg dxf pdf xlsx layout model paper viewport block layer text style print plot line circle arc polyline hatch leader table field xref image north south east west ok yes no file web link email www http https pvc upvc inox vinyl granite marble gypsum glass aluminium aluminum tile ceramic wood paint waterproof membrane insulation pipe duct cable light lamp switch socket fan air conditioner pump tank valve trioncad autocad sofa tivi wifi internet camera karaoke panel pallet silicon').split(' ').forEach(function(w){ EN[w] = 1; });
  // luật ghép vần tiếng Việt: âm đầu + vần (không dấu thanh) + thanh; âm cuối p, t, c, ch chỉ đi với sắc, nặng
  var TONE = ['', '\u0301', '\u0300', '\u0309', '\u0303', '\u0323']; // ngang, sắc, huyền, hỏi, ngã, nặng
  var RHY = ' a ac ach ai am an ang anh ao ap at au ay ăc ăm ăn ăng ăp ăt âc âm ân âng âp ât âu ây e ec em en eng eo ep et ê êch êm ên ênh êp êt êu i ia ich iêc iêm iên iêng iêp iêt iêu im in inh ip it iu o oa oac oach oai oam oan oang oanh oao oap oat oay oăc oăm oăn oăng oăt oc oe oen oeo oet oi om on ong ooc oong op ot ô ôc ôi ôm ôn ông ôp ôt ơ ơi ơm ơn ơp ơt ' +
    'u ua uân uâng uât uây uc uê uêch uênh ui um un ung uôc uôi uôm uôn uông uôt up ut uy uya uych uyên uyêt uyn uynh uyp uyt uyu uơ ư ưa ưc ưi ưm ưn ưng ươc ươi ươm ươn ương ươp ươt ươu ưt ưu y yêm yên yêt yêu ynh yt ';
  var ONS = ['ngh', 'gh', 'gi', 'ng', 'nh', 'ch', 'kh', 'ph', 'th', 'tr', 'qu', 'b', 'c', 'd', 'đ', 'g', 'h', 'k', 'l', 'm', 'n', 'p', 'r', 's', 't', 'v', 'x', ''], RH = {};
  RHY.trim().split(' ').forEach(function(r){ RH[r] = 1; });
  function sp1(w){ var d = w.normalize('NFD'), b = '', t = 0; for (var i = 0; i < d.length; i++){ var k = TONE.indexOf(d[i]); if (k > 0){ if (t) return null; t = k; } else b += d[i]; } return { b:b.normalize('NFC'), t:t }; }
  function okSyl(b, t){
    for (var i = 0; i < ONS.length; i++){
      var o = ONS[i]; if (b.lastIndexOf(o, 0) !== 0) continue; var r = b.slice(o.length), f = r.charAt(0);
      if (!(RH[r] || (o === 'gi' && (!r || RH['i' + r])))) continue;
      if ((o === 'k' || o === 'gh' || o === 'ngh') && 'ieêy'.indexOf(f) < 0) continue;
      if ((o === 'c' || o === 'ng' || o === 'g') && f && 'ieêy'.indexOf(f) >= 0 && !(o === 'g' && f === 'i')) continue;
      if (/(p|t|c|ch)$/.test(r) && t !== 1 && t !== 5) continue;
      return true;
    }
    return false;
  }
  // vị trí dấu thanh: nguyên âm có mũ, móc trước; ươ đặt trên ơ; có âm cuối thì nguyên âm cuối cụm; oa, oe, uy không âm cuối theo kiểu đã chọn
  function place(b, t, st){
    if (!t) return b; var i0 = /^qu./.test(b) || /^gi[aăâeêioôơuưy]/.test(b) ? 2 : 0, a = -1, z = -1;
    for (var i = i0; i < b.length; i++){ if ('aăâeêioôơuưy'.indexOf(b[i]) >= 0){ if (a < 0) a = i; z = i; } else if (a >= 0) break; }
    if (a < 0) return b; var cl = b.slice(a, z + 1), k = cl.indexOf('ươ') >= 0 ? cl.indexOf('ơ') : cl.search(/[ăâêôơư]/);
    if (k < 0) k = cl.length === 1 ? 0 : z + 1 < b.length ? cl.length - 1 : cl.length === 2 ? (st === 'new' && /^(oa|oe|uy)$/.test(cl) ? 1 : 0) : 1;
    var p = a + k; return b.slice(0, p) + (b[p] + TONE[t]).normalize('NFC') + b.slice(p + 1);
  }
  function styleOf(){ return SP.set.tone === 'new' ? 'new' : 'old'; }
  function restyle(w){ return w.split(' ').map(function(x){ var s = sp1(x); return s ? place(s.b, s.t, styleOf()) : x; }).join(' '); }
  function caseLike(src, rep){ return src === src.toUpperCase() && src !== src.toLowerCase() ? rep.toUpperCase() : src.charAt(0) !== src.charAt(0).toLowerCase() ? rep.charAt(0).toUpperCase() + rep.slice(1) : rep; }
  // phím Telex còn sót: s f r x j là dấu thanh; aa ee oo aw ow uw w dd là chữ có mũ, móc
  function telex(w){
    var t = 0, f = w.search(/[aeiouy]/); if (f < 0) return null;
    var s = w.slice(0, f) + w.slice(f).replace(/[sfrxj]/g, function(c){ t = 'sfrxj'.indexOf(c) + 1; return ''; });
    s = s.replace(/^dd/, 'đ').replace(/aa/g, 'â').replace(/ee/g, 'ê').replace(/oo/g, 'ô').replace(/aw/g, 'ă').replace(/ow/g, 'ơ').replace(/uw/g, 'ư');
    if (/w/.test(s)) s = (/uo/.test(s) ? s.replace('uo', 'ươ') : /u/.test(s) ? s.replace('u', 'ư') : /o/.test(s) ? s.replace('o', 'ơ') : /a/.test(s) ? s.replace('a', 'ă') : s.replace('w', 'ư')).replace(/w/g, '');
    s = s.replace(/uơ(?=[a-zđ])/, 'ươ');
    return s !== w && okSyl(s, t) ? place(s, t, styleOf()) : null;
  }
  // thiếu dấu: thử thêm mũ, móc, dấu thanh; chỉ gợi ý từ có trong từ điển
  function accCands(w){
    var bs = [''], out = []; if (w.length > 7) return out; // âm tiết tiếng Việt dài nhất 7 chữ (nghiêng)
    Array.from(w).forEach(function(c, i){ var o = c === 'a' ? 'aăâ' : c === 'e' ? 'eê' : c === 'o' ? 'oôơ' : c === 'u' ? 'uư' : c === 'd' && !i ? 'dđ' : c, n = []; bs.forEach(function(p){ for (var k = 0; k < o.length; k++) n.push(p + o[k]); }); bs = n; });
    bs.forEach(function(b){ if (!okSyl(b, 1)) return; for (var t = 0; t < 6; t++) if (okSyl(b, t)){ var x = place(b, t, 'old'); if (VI[x]) out.push(x); } });
    return out.sort(function(a, b){ return VI[a] - VI[b]; }).slice(0, 5).map(restyle);
  }
  function lev(a, b){ var m = [], i, j; for (i = 0; i <= b.length; i++) m[i] = [i]; for (j = 0; j <= a.length; j++) m[0][j] = j; for (i = 1; i <= b.length; i++) for (j = 1; j <= a.length; j++) m[i][j] = b[i - 1] === a[j - 1] ? m[i - 1][j - 1] : 1 + Math.min(m[i - 1][j - 1], m[i][j - 1], m[i - 1][j]); return m[b.length][a.length]; }
  var VIF = null; // từ điển đã bỏ dấu, tính một lần
  function near(w){
    var f = TC.fold(w), out = [], seen = {};
    if (!VIF){ VIF = {}; Object.keys(VI).forEach(function(x){ VIF[x] = TC.fold(x); }); }
    if (SP.set.lang !== 'en') Object.keys(VI).forEach(function(x){ if (Math.abs(x.length - w.length) < 3 && lev(VIF[x], f) <= 1) out.push([lev(x, w), restyle(x)]); });
    if (SP.set.lang !== 'vi') Object.keys(EN).forEach(function(x){ if (Math.abs(x.length - w.length) < 3){ var k = lev(x, w); if (k <= 2) out.push([k, x]); } });
    return out.sort(function(a, b){ return a[0] - b[0]; }).map(function(x){ return x[1]; }).filter(function(x){ if (seen[x]) return false; seen[x] = 1; return true; }).slice(0, 5);
  }
  var SPK = { tone:['Dấu thanh sai vị trí', 'warn'], telex:['Còn sót phím Telex', 'warn'], acc:['Thiếu dấu', 'warn'], comp:['Thiếu dấu', 'warn'], unk:['Không có trong từ điển', 'danger'] };
  // kiểm một đoạn chữ → [{ i, j, w, k, sug }]
  function spCheck(text){
    var S = SP.set, vi = S.lang !== 'en', en = S.lang !== 'vi', toks = [], out = [];
    String(text).replace(/\S+/g, function(ch, at){
      if (S.web && /:\/\/|^www\.|@|\\|\/|\.[a-z]{2,4}$/i.test(ch)) return ch; // địa chỉ web, email, đường dẫn, tên tệp
      ch.replace(/[0-9A-Za-z\u00C0-\u00D6\u00D8-\u00F6\u00F8-\u1EF9]+/g, function(w, k){ toks.push({ i:at + k, j:at + k + w.length, w:w }); return w; }); return ch;
    });
    var ws = toks.filter(function(x){ var w = x.w, l = w.toLowerCase(); return !(w.length < 2 || (S.num && /\d/.test(w)) || (S.caps && w === w.toUpperCase() && w !== l) || SP.ign[l] || SP.user.some(function(u){ return u.toLowerCase() === l; }) || (en && EN[l])); });
    if (vi && S.acc) for (var a = 0; a < ws.length; a++) for (var n = 3; n > 1; n--){ // cụm từ thiếu dấu: phong khach → phòng khách
      var g = ws.slice(a, a + n); if (g.length < n || g.some(function(x, k){ return /[^\x00-\x7f]/.test(x.w) || (k && text.slice(g[k - 1].j, x.i) !== ' '); })) continue;
      var src = text.slice(g[0].i, g[n - 1].j), c = COMPF[TC.fold(src)]; if (!c || src.toLowerCase() === c) continue;
      out.push({ i:g[0].i, j:g[n - 1].j, w:src, k:'comp', sug:[caseLike(src, restyle(c))] }); g.forEach(function(x){ x.done = 1; }); a += n - 1; break;
    }
    ws.forEach(function(x){
      if (x.done) return; var l = x.w.toLowerCase(), s = vi && sp1(l), iss = function(k, sug){ out.push({ i:x.i, j:x.j, w:x.w, k:k, sug:sug.map(function(z){ return caseLike(x.w, z); }) }); };
      if (s && okSyl(s.b, s.t)){ var e1 = place(s.b, s.t, styleOf()); if (S.tone === 'both' ? l !== e1 && l !== place(s.b, s.t, 'new') : l !== e1) iss('tone', [e1]); return; }
      var tx = vi && S.telex && telex(l); if (tx){ iss('telex', [tx]); return; }
      if (vi && S.acc && !/[^\x00-\x7f]/.test(l)){ var cs = accCands(l); if (cs.length){ iss('acc', cs); return; } }
      iss('unk', near(l));
    });
    return out.sort(function(p, q){ return p.i - q.i; });
  }
  // các đoạn chữ trong phạm vi: text, mtext, multileader, attribute, chữ kích thước, ô bảng
  function spFields(d, scope, ids){
    var S = SP.set, out = [], leg = function(st){ var s = d.styleMap[st] || {}; return S.legacy && (s.enc === 'TCVN3' || s.enc === 'VNI'); };
    (scope === 'cur' ? [d.space] : ['model'].concat(d.layouts.map(function(L, i){ return i; }))).forEach(function(sp){
      var where = sp === 'model' ? 'Model' : d.layouts[sp].name;
      (sp === 'model' ? d.ents : d.layouts[sp].ents).forEach(function(e){
        if ((ids && ids.indexOf(e.id) < 0) || !TC.entVisible(d, e)) return;
        var add = function(f, s, x){ if (s && /[A-Za-z\u00C0-\u1EF9]/.test(s)) out.push(Object.assign({ e:e, f:f, s:String(s), sp:sp, where:where }, x)); };
        if (/^(text|mtext|mleader|attdef)$/.test(e.t) && !leg(e.style)) add('s', e.s, { kind:{ text:'Text', mtext:'MText', mleader:'Multileader', attdef:'Attribute definition' }[e.t] });
        if (S.dim && /^dim/.test(e.t) && e.over) add('over', e.over, { kind:'Chữ kích thước' });
        if (S.att && e.t === 'insert' && e.attrs && (!e.xref || S.xref)) Object.keys(e.attrs).forEach(function(k){ var ad = ((d.blocks[e.name] || {}).ents || []).filter(function(x){ return x.t === 'attdef' && x.tag === k; })[0]; if (!ad || !leg(ad.style)) add('attr', e.attrs[k], { tag:k, kind:'Attribute ' + k }); });
        if (S.tbl && e.t === 'table') (e.cells || []).forEach(function(row, r){ row.forEach(function(c, j){ add('cell', c, { r:r, c:j, kind:'Ô bảng ' + String.fromCharCode(65 + j) + (r + 1) }); }); });
      });
    });
    return out;
  }
  function fset(x, f, v){ if (f.f === 's') x.s = v; else if (f.f === 'over') x.over = v; else if (f.f === 'attr'){ x.attrs = Object.assign({}, x.attrs); x.attrs[f.tag] = v; } else { x.cells = x.cells.map(function(r){ return r.slice(); }); x.cells[f.r][f.c] = v; } }
  // khung của từ trên bản vẽ, đo bằng font vẽ thật (chữ không xoay); để phóng tới đúng từ và gạch lượn sóng dưới từ
  var SPM = null, MC = document.createElement('canvas').getContext('2d');
  function wordBB(f, x){
    if (f.f !== 's') return null; var w = f.s.slice(x.i, x.j), ps = (f.e._p || []).filter(function(q){ return q.k === 'text' && !q.rot; }), pr = f.e.t === 'text' ? ps[0] : ps.filter(function(q){ return q.s.indexOf(w) >= 0; })[0]; if (!pr) return null;
    var i = f.e.t === 'text' ? x.i : pr.s.indexOf(w), k = pr.h * 1.32 / 100; MC.font = V().textFont(pr, 100);
    var a = MC.measureText(pr.s.slice(0, i)).width * k, b = MC.measureText(w).width * k, W = MC.measureText(pr.s).width * k, x0 = pr.p[0] - (pr.al === 'm' || pr.al === 'c' ? W / 2 : pr.al === 'r' ? W : 0), y0 = pr.p[1] - (pr.al === 'm' ? pr.h / 2 : 0);
    return [x0 + a, y0 - pr.h * .3, x0 + a + b, y0 + pr.h];
  }
  (TC.overlays = TC.overlays || []).push(function(ctx, v){
    if (!SPM || !SPM.bb || v.doc !== SPM.d) return; var S = function(p){ return SPM.e._paper ? v.toS(p) : (v.mToS(p) || v.toS(p)); }, a = S([SPM.bb[0], SPM.bb[1]]), b = S([SPM.bb[2], SPM.bb[1]]); if (!a || !b || b[0] - a[0] < 4) return;
    ctx.save(); ctx.strokeStyle = TC.cssv('--c-grip-hot'); ctx.lineWidth = 1.5; ctx.beginPath(); for (var x = a[0], k = 0; x <= b[0] + .5; x += 3, k++) ctx[k ? 'lineTo' : 'moveTo'](x, a[1] + 2 + (k % 2) * 2.5); ctx.stroke(); ctx.restore();
  });
  function bbOf(f){ var e = f.e, b = e._bb; if (f.f === 'attr'){ var p = (e._p || []).filter(function(q){ return q.k === 'text' && q.s === f.s; })[0]; if (p){ var w = p.h * .62 * p.s.length; b = [p.p[0] - w, p.p[1] - p.h, p.p[0] + w, p.p[1] + p.h * 2]; } } var m = Math.max(b[3] - b[1], (b[2] - b[0]) * .12); return [b[0] - m, b[1] - m * 2, b[2] + m, b[3] + m * 2]; }
  // hộp thoại SPELL: không modal để thấy chữ trên bản vẽ; mỗi lần Sửa là một bước hoàn tác
  SC.spell = function(o){
    var d = D(); if (!d) return; o = o || {};
    var dlg = TC.dialog({ id:'spell', title:'Kiểm tra chính tả', cmd:'SPELL', code:'UI-F10', w:720, modal:false,
      body:'<div class="spw"><div class="sp-top"><span class="lab">Phạm vi</span><select class="inp" id="sp-sc" aria-label="Phạm vi kiểm tra" style="width:212px"></select><button class="ib" type="button" id="sp-pick" aria-label="Chọn đối tượng trên bản vẽ" data-tip="Chọn đối tượng trên bản vẽ">' + ic('select') + '</button>' +
        '<span class="lab" style="margin-left:10px">Từ điển</span><select class="inp" id="sp-lg" aria-label="Từ điển chính" style="width:188px"></select><span class="grow"></span><button class="btn" type="button" id="sp-go">' + ic('refresh') + 'Kiểm tra lại</button></div>' +
        '<div class="sp-st" id="sp-st" role="status"></div><div id="sp-main"></div><div class="tblwrap sp-list" id="sp-list"></div></div>',
      foot:'<button class="btn ghost" type="button" id="sp-dict">' + ic('book') + 'Từ điển…</button><button class="btn ghost" type="button" id="sp-set">' + ic('gear') + 'Thiết lập…</button><span class="grow"></span><span class="faint" id="sp-n" style="font-size:var(--fs-s)"></span><button class="btn" type="button" data-x="close">Đóng</button>',
      onClose:function(){ st.stop = true; d.found = []; SPM = null; V().drawOver(); var n = st.iss.length, fx = st.iss.filter(function(x){ return x.st === 'fix'; }).length; if (st.done) TC.cmdline.msg('Kiểm tra chính tả: ' + (n ? 'sửa ' + fx + '/' + n + ' từ.' : 'không có lỗi.')); } });
    if (dlg.el._go){ dlg.el._go(o); return dlg; }
    var st = { scope:'all', ids:[], f:[], iss:[], cur:-1, acts:[], run:false, stop:false, done:false, k:0, part:0 }, waitSel = false;
    var cvr = $('#cv').getBoundingClientRect(), el = dlg.el; el.style.left = Math.max(12, Math.min(innerWidth - el.offsetWidth - 12, cvr.right - el.offsetWidth - 12)) + 'px'; el.style.top = Math.max(12, Math.min(innerHeight - el.offsetHeight - 12, cvr.top + 8)) + 'px';
    $('#sp-lg', el).innerHTML = LANGS.map(function(l){ return '<option value="' + l[0] + '"' + (l[0] === SP.set.lang ? ' selected' : '') + '>' + l[1] + '</option>'; }).join('');
    function start(){
      st.stop = false; st.done = false; st.acts = []; st.cur = -1; st.iss = []; st.k = 0; d.found = []; SPM = null; V().drawOver();
      st.f = st.scope === 'sel' && !st.ids.length ? [] : spFields(d, st.scope, st.scope === 'sel' ? st.ids : null);
      if (!st.f.length){ st.done = true; render(); return; }
      st.run = true; var n = st.f.length, step = Math.max(1, Math.ceil(n / 6));
      (function tick(){ // đọc từng phần để có tiến trình và nút Dừng với bản vẽ lớn
        if (st.stop || !document.contains(el)){ st.run = false; st.part = st.k < n ? st.k : 0; return fin(); }
        for (var e = Math.min(n, st.k + step); st.k < e; st.k++){ var f = st.f[st.k]; spCheck(f.s).forEach(function(x){ x.f = f; x.st = ''; st.iss.push(x); }); }
        if (st.k < n){ render(); setTimeout(tick, 90); } else { st.run = false; st.part = 0; fin(); }
      })();
    }
    function fin(){ st.done = true; st.cur = -1; next(); render(); }
    function next(){ var n = st.iss.length; for (var k = 1; k <= n; k++){ var j = (Math.max(st.cur, -1) + k) % n; if (!st.iss[j].st){ st.cur = j; show(); return; } } st.cur = -1; d.found = []; SPM = null; V().drawOver(); }
    function show(){ var x = st.iss[st.cur]; if (!x) return; if (x.f.sp !== d.space) A().setSpace(x.f.sp); d.found = [x.f.e.id]; var wb = wordBB(x.f, x), h = wb && (wb[3] - wb[1]) / 1.3; SPM = { d:d, e:x.f.e, bb:x.st ? null : wb }; SC.zoomBeside(wb ? [wb[0] - h * 15, wb[1] - h * 5, wb[2] + h * 15, wb[3] + h * 5] : bbOf(x.f), el); }
    function ctxHTML(x){ var s = x.f.s, a = Math.max(0, x.i - 60), b = Math.min(s.length, x.j + 60); return (a ? '…' : '') + esc(s.slice(a, x.i)) + '<mark>' + esc(s.slice(x.i, x.j)) + '</mark>' + esc(s.slice(x.j, b)) + (b < s.length ? '…' : ''); }
    function B(k, label, cls, dis){ return '<button class="btn sm' + (cls ? ' ' + cls : '') + '" type="button" data-sa="' + k + '"' + (dis ? ' disabled' : '') + '>' + label + '</button>'; }
    function render(){
      var s = $('#sp-st', el), m = $('#sp-main', el), L = $('#sp-list', el), n = st.iss.length, left = st.iss.filter(function(x){ return !x.st; }).length, fx = st.iss.filter(function(x){ return x.st === 'fix'; }).length, x = st.iss[st.cur];
      var sc = [['all', 'Cả bản vẽ'], ['cur', d.space === 'model' ? 'Không gian hiện tại (Model)' : 'Layout hiện tại (' + d.layouts[d.space].name + ')'], ['sel', 'Đối tượng chọn' + (st.ids.length ? ' (' + TC.n(st.ids.length) + ')' : '')]];
      $('#sp-sc', el).innerHTML = sc.map(function(o2){ return '<option value="' + o2[0] + '"' + (o2[0] === st.scope ? ' selected' : '') + '>' + esc(o2[1]) + '</option>'; }).join('');
      s.innerHTML = st.run ? '<span class="spin"></span>Đang kiểm tra ' + TC.n(st.k) + '/' + TC.n(st.f.length) + ' đối tượng chữ…<span class="prog"><i style="width:' + Math.round(st.k / st.f.length * 100) + '%"></i></span><button class="btn sm" type="button" id="sp-stop">Dừng</button>'
        : st.done && st.f.length && n ? (left ? ic('warn', 's12') : '<span class="sp-ok">' + ic('okc', 's12') + '</span>') + '<span>Đã kiểm tra ' + TC.n(st.part || st.f.length) + (st.part ? '/' + TC.n(st.f.length) : '') + ' đối tượng chữ' + (st.part ? ' (đã dừng)' : '') + ': <b>' + n + '</b> từ cần xem, còn <b>' + left + '</b>.</span>' : '';
      if (!x){
        var skip = [SP.set.caps ? 'từ viết hoa toàn bộ' : '', SP.set.num ? 'từ có chữ số' : '', SP.set.web ? 'địa chỉ web' : ''].filter(Boolean), noSel = st.scope === 'sel' && !st.ids.length;
        m.innerHTML = '<div class="empty sp-done">' + (st.run ? '<span class="spin"></span><b>Đang kiểm tra…</b>' : !st.done ? '' : !st.f.length ? ic('findtext') + '<b>' + (noSel ? 'Chưa chọn đối tượng' : 'Phạm vi không có chữ để kiểm tra') + '</b><span>' + (noSel ? 'Bấm nút chọn cạnh Phạm vi rồi chọn chữ trên bản vẽ.' : 'Chọn phạm vi Cả bản vẽ, hoặc chọn đối tượng chữ khác.') + '</span>'
          : !n ? ic('okc') + '<b>Không có lỗi chính tả</b><span>Đã kiểm tra ' + TC.n(st.part || st.f.length) + ' đối tượng chữ' + (st.part ? ' (đã dừng giữa chừng)' : '') + '.' + (skip.length ? ' Bỏ qua ' + skip.join(', ') + ' theo Thiết lập.' : '') + '</span>'
          : ic('okc') + '<b>Đã xem hết ' + n + ' từ</b><span>Sửa ' + fx + ', bỏ qua ' + (n - fx) + '. Bấm Kiểm tra lại để quét lần nữa.</span>') + '</div>';
      } else {
        var lk = (d.layerMap[x.f.e.layer] || {}).lck, same = st.iss.filter(function(y){ return !y.st && y.w.toLowerCase() === x.w.toLowerCase(); }).length;
        var why = { tone:'Theo kiểu đặt dấu ' + (SP.set.tone === 'new' ? 'mới (hoà, thuỷ, khoẻ)' : 'cũ (hòa, thủy, khỏe)') + ' trong Thiết lập.', telex:'Bộ gõ chưa chuyển hết phím Telex (s f r x j, aa, ow, uw, dd).', acc:'Từ chưa có dấu tiếng Việt.', comp:'Cụm từ chưa có dấu tiếng Việt.', unk:'Không có trong từ điển ' + langName() + ' và từ điển người dùng.' }[x.k];
        m.innerHTML = '<div class="sp-main"><div class="sp-cur"><div class="hrow"><span class="gh">' + (x.st ? { fix:'Đã sửa', skip:'Đã bỏ qua', add:'Đã thêm vào từ điển' }[x.st] : 'Từ cần xem') + '</span><span class="tag ' + SPK[x.k][1] + '">' + SPK[x.k][0] + '</span><span class="grow"></span><span class="faint mono">' + (st.cur + 1) + '/' + n + '</span></div>' +
          '<div class="sp-ctx">' + ctxHTML(x) + '</div><div class="sp-meta faint">' + esc(x.f.kind) + ' · layer ' + esc(x.f.e.layer) + ' · ' + esc(x.f.where) + (lk ? ' · <span class="tag warn">layer khóa, không sửa được</span>' : '') + '<span class="grow"></span><button class="btn ghost sm" type="button" id="sp-zoom">' + ic('zoom') + 'Phóng tới</button></div>' +
          '<div class="frow" style="--lab-w:64px"><label class="lab" for="sp-to">Sửa thành</label><input class="inp" id="sp-to" value="' + esc(x.st === 'fix' ? x.rep : x.sug[0] || x.w) + '" spellcheck="false" autocomplete="off"' + (x.st ? ' disabled' : '') + '>' +
          '<span class="lab" style="align-self:start;padding-top:3px">Gợi ý</span><div class="tree sp-sug" role="listbox" aria-label="Gợi ý">' + (x.sug.length ? x.sug.map(function(z, k){ return '<button class="tn" type="button" role="option" data-sg="' + k + '" aria-selected="' + (!x.st && !k) + '">' + esc(z) + '</button>'; }).join('') : '<span class="faint sp-nos">Không có gợi ý. Gõ từ đúng vào ô Sửa thành.</span>') + '</div></div>' +
          '<p class="faint sp-why">' + esc(why) + '</p></div><div class="sp-acts">' + B('fix', 'Sửa', 'primary', x.st || lk) + B('all', 'Sửa tất cả' + (same > 1 ? ' (' + same + ')' : ''), '', x.st || lk || same < 2) + '<div class="hsep"></div>' +
          B('skip', 'Bỏ qua', '', x.st) + B('ign', 'Bỏ qua tất cả', '', x.st) + B('add', 'Thêm vào từ điển', '', x.st || x.k === 'tone' || x.k === 'comp') + '<div class="hsep"></div>' + B('undo', ic('undo') + 'Hoàn tác', 'ghost', !st.acts.length) + '</div></div>';
      }
      L.hidden = !n;
      L.innerHTML = n ? '<table class="tbl"><colgroup><col style="width:26px"><col style="width:150px"><col style="width:150px"><col><col style="width:84px"></colgroup><thead><tr><th></th><th>Từ</th><th>Lý do</th><th>Ngữ cảnh</th><th>Vị trí</th></tr></thead><tbody>' + st.iss.map(function(y, k){
        var s2 = y.f.s, a = Math.max(0, y.i - 24); return '<tr data-si="' + k + '"' + (k === st.cur ? ' class="is-sel"' : '') + '><td class="ctr">' + (y.st === 'fix' ? ic('check', 's12') : y.st ? '<span class="faint">–</span>' : k === st.cur ? ic('right', 's12') : '') + '</td><td>' + (y.st === 'fix' ? '<s>' + esc(y.w) + '</s> ' + esc(y.rep) : esc(y.w)) + '</td><td><span class="tag' + (y.st ? '' : ' ' + SPK[y.k][1]) + '">' + SPK[y.k][0] + '</span></td><td class="faint">' + (a ? '…' : '') + esc(s2.slice(a, y.j + 30)) + '</td><td>' + esc(y.f.where) + '</td></tr>';
      }).join('') + '</tbody></table>' : '';
      $('#sp-n', el).textContent = n ? 'Đã sửa ' + fx + ' · bỏ qua ' + (n - fx - left) + ' · còn ' + left : '';
    }
    function snap(){ return { i:st.iss.map(function(x){ return [x.i, x.j, x.st, x.rep]; }), s:st.f.map(function(f){ return f.s; }), cur:st.cur }; }
    function back(sn){ sn.i.forEach(function(a, k){ var x = st.iss[k]; x.i = a[0]; x.j = a[1]; x.st = a[2]; x.rep = a[3]; }); sn.s.forEach(function(s, k){ st.f[k].s = s; }); st.cur = sn.cur; }
    function fix(y, rep){ var f = y.f, ns = f.s.slice(0, y.i) + rep + f.s.slice(y.j), dl = rep.length - (y.j - y.i); ED.mod(f.e, function(z){ fset(z, f, ns); }); f.s = ns; st.iss.forEach(function(o2){ if (o2 !== y && o2.f === f && o2.i >= y.j){ o2.i += dl; o2.j += dl; } }); y.st = 'fix'; y.rep = rep; y.j = y.i + rep.length; }
    function act(k){
      if (k === 'undo') return undo();
      var x = st.iss[st.cur]; if (!x || x.st) return; var l = x.w.toLowerCase(), same = st.iss.filter(function(y){ return !y.st && y.w.toLowerCase() === l; }), rec = { sn:snap() };
      if (k === 'skip') x.st = 'skip';
      else if (k === 'ign'){ SP.ign[l] = 1; same.forEach(function(y){ y.st = 'skip'; }); rec.ign = l; }
      else if (k === 'add'){ SP.user.push(x.w); spSave(); same.forEach(function(y){ y.st = 'add'; }); rec.add = x.w; }
      else {
        var rep = $('#sp-to', el).value.trim(), list = (k === 'all' ? same : [x]).filter(function(y){ return !(d.layerMap[y.f.e.layer] || {}).lck; });
        if (!rep){ var t = $('#sp-to', el); t.classList.add('err'); t.focus(); return; } if (!list.length) return;
        ED.run('SPELL', function(){ list.forEach(function(y){ fix(y, caseLike(y.w, y === x ? rep : rep.toLowerCase())); }); }); V().draw(); rec.edit = true;
      }
      st.acts.push(rec); next(); render();
    }
    function undo(){
      var a = st.acts[st.acts.length - 1]; if (!a) return;
      if (a.edit){ var u = d.undo || []; if (!u.length || u[u.length - 1].label !== 'SPELL'){ TC.toast('Không hoàn tác trong hộp này được: đã có thao tác khác sau lần sửa. Dùng Ctrl+Z trên bản vẽ.', { type:'warn', log:false }); return; } ED.undo(1); }
      st.acts.pop(); if (a.ign) delete SP.ign[a.ign]; if (a.add){ SP.user = SP.user.filter(function(w){ return w !== a.add; }); spSave(); } back(a.sn); show(); render();
    }
    function sug(b){ $$('[data-sg]', el).forEach(function(x){ x.setAttribute('aria-selected', String(x === b)); }); var t = $('#sp-to', el); if (t && !t.disabled) t.value = b.textContent; }
    el.addEventListener('click', function(e){
      var a = e.target.closest('[data-sa]'); if (a){ if (!a.disabled) act(a.dataset.sa); return; }
      var r = e.target.closest('[data-si]'); if (r){ st.cur = Number(r.dataset.si); show(); render(); return; }
      var g = e.target.closest('[data-sg]'); if (g){ sug(g); return; }
      if (e.target.closest('#sp-zoom')) show();
      else if (e.target.closest('#sp-go')) start();
      else if (e.target.closest('#sp-stop')) st.stop = true;
      else if (e.target.closest('#sp-pick')){ dlg.hide(true); waitSel = true; TC.cad.clearSel(); TC.cmd.run('SELECT'); TC.cmdline.msg('Chọn đối tượng chữ cần kiểm tra, Enter để quay lại hộp thoại.'); }
      else if (e.target.closest('#sp-set')) SC.spellSettings().then(function(ch){ if (ch) start(); });
      else if (e.target.closest('#sp-dict')) SC.spellDict().then(function(ch){ if (ch){ $('#sp-lg', el).value = SP.set.lang; start(); } });
    });
    el.addEventListener('dblclick', function(e){ var g = e.target.closest('[data-sg]'); if (g){ sug(g); act('fix'); } });
    el.addEventListener('change', function(e){ if (e.target.id === 'sp-sc'){ st.scope = e.target.value; if (st.scope === 'sel' && D().sel.length) st.ids = D().sel.slice(); start(); } else if (e.target.id === 'sp-lg'){ SP.set.lang = e.target.value; spSave(); start(); } });
    // bàn phím: Enter trong ô Sửa thành hoặc trên một gợi ý để sửa; ↑↓ chọn gợi ý
    el.addEventListener('keydown', function(e){
      var inS = e.target.closest && e.target.closest('.sp-sug'), here = inS || e.target.id === 'sp-to'; if (!here) return;
      if (e.key === 'Enter'){ e.preventDefault(); e.stopPropagation(); if (inS && e.target.dataset.sg != null) sug(e.target); act('fix'); }
      else if (e.key === 'ArrowDown' || e.key === 'ArrowUp'){ var L = $$('[data-sg]', el); if (!L.length) return; e.preventDefault(); var i = L.findIndex(function(b){ return b.getAttribute('aria-selected') === 'true'; }); i = TC.clamp(i + (e.key === 'ArrowDown' ? 1 : -1), 0, L.length - 1); sug(L[i]); if (inS) L[i].focus(); }
    });
    el.addEventListener('input', function(e){ if (e.target.id === 'sp-to') e.target.classList.remove('err'); });
    TC.on('cmdend', function(c){ if (c === 'SELECT' && waitSel){ waitSel = false; st.ids = D().sel.slice(); st.scope = 'sel'; dlg.hide(false); start(); } });
    el._go = function(o2){ if (o2.scope){ st.scope = o2.scope; st.ids = o2.ids || []; } el.focus(); start(); };
    el._go(o);
    return dlg;
  };
  SC.spellSettings = function(){
    return new Promise(function(res){
      var S = SP.set;
      var dlg = TC.dialog({ id:'spset', title:'Thiết lập chính tả', cmd:'SPELL', code:'UI-F10', w:540, onClose:function(r){ if (r !== true) res(false); },
        body:'<div class="stack">' + H.grp('Kiểm tra cả', '<div class="grid2" style="row-gap:5px">' + H.chk('ss-dim', 'Chữ của kích thước', S.dim) + H.chk('ss-att', 'Attribute của block', S.att) + H.chk('ss-tbl', 'Chữ trong bảng', S.tbl) + H.chk('ss-xref', 'Block gắn từ tham chiếu ngoài', S.xref) + '</div>') +
          H.grp('Bỏ qua', '<div class="stack" style="gap:5px">' + H.chk('ss-caps', 'Từ viết hoa toàn bộ (TCVN, BTCT, MẶT BẰNG)', S.caps) + H.chk('ss-num', 'Từ có chữ số (D20, 2T, KT-01)', S.num) + H.chk('ss-web', 'Địa chỉ web, email, đường dẫn tệp', S.web) + H.chk('ss-leg', 'Chữ mã TCVN3, VNI chưa chuyển sang Unicode', S.legacy) + '</div>') +
          H.grp('Tiếng Việt', H.row('Kiểu đặt dấu', '<span class="stack" style="gap:4px">' + H.rad('ss-tone', 'old', 'Kiểu cũ: hòa, thủy, khỏe', S.tone === 'old') + H.rad('ss-tone', 'new', 'Kiểu mới: hoà, thuỷ, khoẻ', S.tone === 'new') + H.rad('ss-tone', 'both', 'Chấp nhận cả hai kiểu', S.tone === 'both') + '</span>', 96) +
            H.chk('ss-acc', 'Báo từ thiếu dấu (phong khach → phòng khách)', S.acc) + H.chk('ss-tx', 'Báo phím gõ Telex còn sót (tuwowngf → tường)', S.telex)) + '</div>',
        foot:'<button class="btn ghost" type="button" id="ss-def">Khôi phục mặc định</button><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="ss-ok">Lưu thiết lập</button>' });
      var M = { dim:'ss-dim', att:'ss-att', tbl:'ss-tbl', xref:'ss-xref', caps:'ss-caps', num:'ss-num', web:'ss-web', legacy:'ss-leg', acc:'ss-acc', telex:'ss-tx' };
      $('#ss-def', dlg.el).addEventListener('click', function(){ Object.keys(M).forEach(function(k){ $('#' + M[k], dlg.el).checked = SPDEF[k]; }); $$('[name="ss-tone"]', dlg.el).forEach(function(r){ r.checked = r.value === SPDEF.tone; }); });
      $('#ss-ok', dlg.el).addEventListener('click', function(){ Object.keys(M).forEach(function(k){ S[k] = $('#' + M[k], dlg.el).checked; }); S.tone = $('[name="ss-tone"]:checked', dlg.el).value; spSave(); dlg.close(true); res(true); });
    });
  };
  SC.spellDict = function(){
    return new Promise(function(res){
      var q = '', sel = null, ch = false;
      var dlg = TC.dialog({ id:'spdict', title:'Từ điển chính tả', cmd:'SPELL', code:'UI-F10', w:480, onClose:function(){ res(ch); },
        body:'<div class="stack">' + H.row('<label for="sd-lang">Từ điển chính</label>', H.sel('sd-lang', LANGS.map(function(l){ return l[1]; }), langName(), 250), 104) + '<div class="hsep"></div>' +
          '<div class="hrow"><span class="gh">Từ điển người dùng</span><span class="faint mono grow" style="font-size:var(--fs-xs)">tc-nguoidung.dic</span><span class="faint" id="sd-n" style="font-size:var(--fs-s)"></span></div>' +
          '<div class="stack" style="gap:3px"><div class="hrow"><input class="inp grow" id="sd-w" placeholder="Từ mới, ví dụ Loggia" aria-label="Từ mới" spellcheck="false" autocomplete="off"><button class="btn" type="button" id="sd-add">' + ic('plus') + 'Thêm</button></div><span class="fmsg" id="sd-m"></span></div>' +
          '<label class="search">' + ic('search') + '<input id="sd-q" placeholder="Tìm trong từ điển người dùng" aria-label="Tìm từ"></label><div class="tblwrap" style="height:168px"><div class="tree" id="sd-l" role="listbox" aria-label="Từ điển người dùng" tabindex="0"></div></div>' +
          '<div class="hrow"><button class="btn sm" type="button" id="sd-del" disabled>' + ic('trash') + 'Xóa từ</button><span class="grow"></span><button class="btn sm ghost" type="button" id="sd-imp">Nhập .dic…</button><button class="btn sm ghost" type="button" id="sd-exp">Xuất .dic…</button></div></div>',
        foot:'<span class="grow"></span><button class="btn" type="button" data-x="close">Đóng</button>', focus:'#sd-w' });
      var m = $('#sd-m', dlg.el);
      function list(){ var L = SP.user.filter(function(w){ return !q || TC.fold(w).indexOf(TC.fold(q)) >= 0; }).sort(function(a, b){ return a.localeCompare(b, 'vi'); });
        $('#sd-l', dlg.el).innerHTML = L.length ? L.map(function(w){ return '<button class="tn" type="button" role="option" data-dw="' + esc(w) + '" aria-selected="' + (w === sel) + '">' + esc(w) + '</button>'; }).join('') : '<span class="faint" style="padding:6px 8px">' + (q ? 'Không có từ khớp “' + esc(q) + '”.' : 'Chưa có từ nào. Thêm từ chuyên ngành, tên riêng để không bị báo sai.') + '</span>';
        $('#sd-n', dlg.el).textContent = TC.n(SP.user.length) + ' từ'; $('#sd-del', dlg.el).disabled = !sel; }
      function add(){ var i = $('#sd-w', dlg.el), w = i.value.trim(), bad = !w ? 'Nhập một từ.' : /\s/.test(w) ? 'Mỗi lần thêm một từ, không có dấu cách.' : SP.user.some(function(u){ return u.toLowerCase() === w.toLowerCase(); }) ? 'Từ “' + w + '” đã có trong từ điển người dùng.' : '';
        if (bad){ m.className = 'fmsg err'; m.innerHTML = ic('warn', 'xs') + esc(bad); i.classList.add('err'); i.focus(); return; }
        SP.user.push(w); spSave(); ch = true; sel = w; i.value = ''; m.className = 'fmsg hint'; m.textContent = 'Đã thêm “' + w + '”.'; list(); i.focus(); }
      dlg.el.addEventListener('click', function(e){
        var t = e.target.closest('[data-dw]'); if (t){ sel = t.dataset.dw; list(); return; }
        if (e.target.closest('#sd-add')) add();
        else if (e.target.closest('#sd-del') && sel){ var w = sel; SP.user = SP.user.filter(function(u){ return u !== w; }); spSave(); ch = true; sel = null; list(); TC.toast('Đã xóa “' + esc(w) + '” khỏi từ điển người dùng.', { log:false, act:{ label:'Hoàn tác', fn:function(){ SP.user.push(w); spSave(); if (document.contains(dlg.el)) list(); } } }); }
        else if (e.target.closest('#sd-imp, #sd-exp')) TC.toast('Hộp thoại ' + (e.target.closest('#sd-imp') ? 'Mở' : 'Lưu') + ' tệp của hệ điều hành (tệp .dic, mỗi dòng một từ).', { type:'info', log:false });
      });
      dlg.el.addEventListener('keydown', function(e){ if (e.key === 'Enter' && e.target.id === 'sd-w'){ e.preventDefault(); e.stopPropagation(); add(); } else if (e.key === 'Delete' && e.target.closest('#sd-l') && sel) $('#sd-del', dlg.el).click(); });
      dlg.el.addEventListener('input', function(e){ if (e.target.id === 'sd-q'){ q = e.target.value; list(); } else if (e.target.id === 'sd-w'){ e.target.classList.remove('err'); m.textContent = ''; } });
      dlg.el.addEventListener('change', function(e){ if (e.target.id === 'sd-lang'){ SP.set.lang = LANGS.filter(function(l){ return l[1] === e.target.value; })[0][0]; spSave(); ch = true; } });
      TC.listKeys($('#sd-l', dlg.el), '[data-dw]', function(b){ sel = b.dataset.dw; list(); var f = $('[data-dw][aria-selected="true"]', dlg.el); if (f) f.focus(); });
      list();
    });
  };
  // trình soạn MText: gạch chân lượn sóng từ sai (bỏ qua từ đang gõ dở), chuột phải lên từ để có gợi ý
  SC.mtSpell = function(M, box){
    var ta = M.ta, mir = TC.el('<div class="mte-sp" aria-hidden="true"></div>'), iss = [];
    box.appendChild(mir);
    function sync(){
      if (!document.contains(ta)){ ob.disconnect(); return; }
      mir.hidden = !SP.set.auto; if (!SP.set.auto){ iss = []; return; }
      ['font', 'lineHeight', 'textTransform', 'textAlign', 'letterSpacing'].forEach(function(k){ mir.style[k] = ta.style[k]; });
      var s = ta.value, at = document.activeElement === ta && ta.selectionStart === ta.selectionEnd ? ta.selectionEnd : -1, h = '', k = 0;
      iss = spCheck(s).filter(function(x){ return x.j !== at; });
      iss.forEach(function(x, n){ h += esc(s.slice(k, x.i)) + '<span class="sx" data-n="' + n + '">' + esc(s.slice(x.i, x.j)) + '</span>'; k = x.j; });
      mir.innerHTML = h + esc(s.slice(k)) + '\u200b';
    }
    var ob = new MutationObserver(sync); ob.observe(ta, { attributes:true, attributeFilter:['style'] });
    ta.addEventListener('input', sync); ta.addEventListener('mouseup', sync); ta.addEventListener('blur', sync);
    ta.addEventListener('keyup', function(e){ if (/^(Arrow|Home|End|PageUp|PageDown)/.test(e.key)) sync(); });
    ta.addEventListener('contextmenu', function(ev){
      var hit = null; $$('.sx', mir).forEach(function(sp){ Array.prototype.forEach.call(sp.getClientRects(), function(r){ if (ev.clientX >= r.left - 1 && ev.clientX <= r.right + 1 && ev.clientY >= r.top - 2 && ev.clientY <= r.bottom + 3) hit = iss[Number(sp.dataset.n)]; }); });
      if (!hit) return; ev.preventDefault(); ev.stopPropagation();
      var put = function(rep){ ta.focus(); ta.setSelectionRange(hit.i, hit.j); M.insert(rep); sync(); }, l = hit.w.toLowerCase();
      TC.menu([{ h:SPK[hit.k][0] }].concat(hit.sug.length ? hit.sug.map(function(z){ return { label:'<b>' + esc(z) + '</b>', act:function(){ put(z); } }; }) : [{ label:'Không có gợi ý', dis:true }]).concat([{ sep:1 },
        { label:'Bỏ qua tất cả', act:function(){ SP.ign[l] = 1; sync(); ta.focus(); } }, { label:'Thêm vào từ điển', dis:hit.k === 'tone' || hit.k === 'comp', act:function(){ SP.user.push(hit.w); spSave(); sync(); ta.focus(); } }, { sep:1 },
        { label:'Thiết lập chính tả…', icon:'gear', act:function(){ SC.spellSettings().then(function(){ M.apply(); }); } }]), ev.clientX, ev.clientY);
    });
    sync();
  };
  SC.mtSpellPanel = function(M){
    return { name:'Chính tả', icon:'spell', prio:4, items:[
      { t:'lg', label:'Kiểm tra\nchính tả', icon:'spell', id:'mte-spell', toggle:true, on:function(){ return !!SP.set.auto; }, desc:'Gạch chân lượn sóng từ sai khi gõ; chuột phải lên từ để xem gợi ý.', act:function(){ SP.set.auto = !SP.set.auto; spSave(); M.apply(); } },
      { t:'col', items:[ { t:'s', label:'Từ điển', icon:'book', id:'mte-dict', menuOnly:true, menu:function(){ return [{ h:'Từ điển chính' }].concat(LANGS.map(function(l){ return { label:l[1], check:SP.set.lang === l[0], act:function(){ SP.set.lang = l[0]; spSave(); M.apply(); } }; })).concat([{ sep:1 }, { label:'Từ điển người dùng…', act:function(){ SC.spellDict().then(function(){ M.apply(); }); } }]); } },
        { t:'s', label:'Thiết lập', icon:'gear', id:'mte-sset', act:function(){ SC.spellSettings().then(function(){ M.apply(); }); } } ] } ] };
  };
  IMPL.SPELL = function(){ var d = D(), ids = d.sel.slice(); SC.spell(ids.length ? { scope:'sel', ids:ids } : { scope:'all' }); };
  // ghi chú của Banve-01 có vài lỗi mẫu: thiếu dấu, dấu sai vị trí, cụm từ thiếu dấu, từ lạ, phím Telex sót
  var NOTES = ['1. Tường bao xây gach dày 220, tường ngăn dày 110.', '2. Cao độ ±0.000 lấy theo mặt sàn hoàn thiện tầng 1.', '3. Kích thước ghi bằng mm, cao độ ghi bằng m. Ống điều hoà đi âm trần.', '4. Cửa đi, cửa sổ xem bảng thống kê bản vẽ KT-12; sàn phong khach lát gạch 600x600.', '5. Lan can logia, tuwowngf rào dùng thép hộp sơn tĩnh điện.'];
  function seedNotes(d){
    var notes = d.ents.filter(function(e){ return e.t === 'text' && e.layer === 'KT-GHICHU' && /^\d\. /.test(e.s) && e.p[0] < 40000; }).sort(function(a, b){ return b.p[1] - a.p[1]; });
    NOTES.forEach(function(s, k){ var e = notes[k]; if (!e){ e = { t:'text', p:[13600, 8000 - k * 480], h:220, s:s, style:'VNTIME', al:'l', layer:'KT-GHICHU', id:++d.seq }; d.ents.push(e); } e.s = s; TC.prepEnt(d, e); });
  }
  OPEN['UI-F10'] = function(){
    var d = SC.cat.MOD(); seedNotes(d); SP.ign = {}; TC.cad.clearSel(); V().draw();
    setTimeout(function(){ SC.spell({ scope:'all' }); TC.toast('Thử thêm: trong trình soạn MText, từ sai được gạch chân lượn sóng.', { type:'info', log:false, ms:9000, act:{ label:'Mở MText mẫu', fn:mtDemo } }); }, 60);
  };
  // gõ thử một ghi chú mới trong trình soạn MText: từ sai hiện gạch lượn sóng ngay khi gõ
  function mtDemo(){
    var top = TC.topDialog(); if (top && top.id === 'spell') top.close(); SC.cat.MOD(); TC.cad.clearSel(); V().zoomTo([12600, 3200, 21600, 9000]);
    TC.cmd.run('MTEXT'); setTimeout(function(){ TC.cad.feedPoint([13600, 5500]); setTimeout(function(){ TC.cad.feedPoint([20900, 4200]); setTimeout(function(){ var M = ED.mte && ED.mte(); if (!M) return; M.ta.value = 'Ghi chú hoàn thiện: tuwowngf sơn nước màu trắng, trần thạch cao. Ống điều hoà đi âm trần; sàn phong khach lát gạch.'; M.ta.setSelectionRange(M.ta.value.length, M.ta.value.length); M.ta.dispatchEvent(new Event('input')); }, 200); }, 140); }, 140);
  }

  /* ================= Hyperlink UI-F15 (HYPERLINK) ================= */
  // liên kết lưu trên đối tượng: { to:'web'|'file'|'view'|'layout', a:địa chỉ hoặc tên, t:chữ hiển thị }
  // tệp mẫu coi như có trên máy; tệp ngoài danh sách báo không tìm thấy (thử trạng thái lỗi)
  var HLF = [['D:\\DuAn\\NhaO-2T\\HoSo\\Thuyet-minh-kien-truc.pdf', 'Thuyết minh kiến trúc', '2,4 MB'], ['D:\\DuAn\\NhaO-2T\\KienTruc\\KT-12-Thong-ke-cua.dwg', 'Bản vẽ KT-12: thống kê cửa', '860 KB'], ['D:\\DuAn\\NhaO-2T\\KetCau\\Ketcau-T2.dwg', 'Kết cấu sàn tầng 2', '1,2 MB'], ['D:\\DuAn\\NhaO-2T\\HoSo\\Du-toan.xlsx', 'Dự toán', '310 KB'], ['D:\\DuAn\\NhaO-2T\\HoSo\\Anh-hien-trang\\lo-B7.jpg', 'Ảnh hiện trạng lô B7', '3,1 MB']];
  var HLW = [['https://thuvien.trioncad.vn/tcvn/5574-2018', 'TCVN 5574:2018 Kết cấu bê tông và bê tông cốt thép'], ['https://thuvien.trioncad.vn/tcvn/4085-2011', 'TCVN 4085:2011 Kết cấu gạch đá'], ['https://trioncad.vn/huong-dan/hyperlink', 'Hướng dẫn gắn hyperlink']];
  function hlDir(){ return ((D() || {}).path || '').split('\\').slice(0, -1); }
  function hlAbs(a){ if (!/^\.\.?\\/.test(a)) return a; var dir = hlDir(); a.split('\\').forEach(function(s){ if (s === '..') dir.pop(); else if (s !== '.') dir.push(s); }); return dir.join('\\'); }
  function hlRel(a){ var dir = hlDir(), p = a.split('\\'), k = 0; if (!dir.length || (p[0] || '').toLowerCase() !== dir[0].toLowerCase()) return a; while (k < dir.length && k < p.length - 1 && p[k].toLowerCase() === dir[k].toLowerCase()) k++; return (k === dir.length ? '.\\' : new Array(dir.length - k + 1).join('..\\')) + p.slice(k).join('\\'); }
  function hlFile(a){ var x = hlAbs(a).toLowerCase(); return HLF.filter(function(f){ return f[0].toLowerCase() === x; })[0]; }
  function hlWhere(L){ return L.to === 'view' ? 'View “' + L.a + '” trong bản vẽ' : L.to === 'layout' ? 'Layout “' + L.a + '” trong bản vẽ' : L.a; }
  // mở liên kết: web, tệp bằng ứng dụng mặc định; view, layout thì chuyển tới (Viewer cũng mở được)
  function hlOpen(L){
    var d = D(), name = L.a.split('\\').pop(), ext = (name.match(/\.(\w+)$/) || [])[1] || '';
    if (L.to === 'layout'){ var i = d.layouts.map(function(x){ return x.name; }).indexOf(L.a); if (i < 0){ TC.toast('Bản vẽ không còn layout “' + esc(L.a) + '”. Sửa liên kết: chuột phải › Hyperlink › Sửa.', { type:'warn', log:false }); return; } A().setSpace(i); TC.cmdline.msg('Đã chuyển tới layout “' + esc(L.a) + '”.'); return; }
    if (L.to === 'view'){ var v = d.views.filter(function(x){ return x.name === L.a; })[0]; if (!v){ TC.toast('Bản vẽ không còn view “' + esc(L.a) + '”. Sửa liên kết: chuột phải › Hyperlink › Sửa.', { type:'warn', log:false }); return; } if (d.space !== 'model') A().setSpace('model'); var vv = V(); vv.pushHist(); d.view = { cx:v.c[0], cy:v.c[1], s:vv.H / v.h }; vv.draw(); TC.emit('view'); TC.cmdline.msg('Đã chuyển tới view “' + esc(L.a) + '”.'); return; }
    if (L.to === 'web'){ TC.toast('Đang mở <b>' + esc(L.a) + '</b> bằng trình duyệt mặc định.', { type:'info', log:false }); return; }
    if (/^(exe|bat|cmd|com|msi|vbs|js|lsp|fas|vlx|scr|ps1)$/i.test(ext)){ TC.confirm({ title:'Mở tệp chạy được', msg:'Liên kết trỏ tới tệp chạy được <b>' + esc(name) + '</b>.', detail:'TrionCAD không mở thẳng loại tệp này để tránh mã độc. Mở thư mục chứa tệp để kiểm tra trước.', ok:'Mở thư mục chứa tệp' }).then(function(y){ if (y) TC.toast('Đã mở thư mục ' + esc(hlAbs(L.a).split('\\').slice(0, -1).join('\\')) + '.', { log:false }); }); return; }
    if (!hlFile(L.a)){ TC.toast('Không tìm thấy <b>' + esc(name) + '</b> tại ' + esc(hlAbs(L.a).split('\\').slice(0, -1).join('\\') || 'thư mục bản vẽ') + '. Kiểm tra lại đường dẫn: chuột phải › Hyperlink › Sửa.', { type:'err', log:false }); return; }
    TC.toast(/^(dwg|dxf)$/i.test(ext) ? 'Đang mở <b>' + esc(name) + '</b> trong TrionCAD.' : 'Đang mở <b>' + esc(name) + '</b> bằng ứng dụng mặc định.', { type:'info', log:false });
  }
  // rê chuột: huy hiệu liên kết cạnh con trỏ và tooltip; cad.hook.move chỉ chạy khi lệnh chờ điểm nên bọc thẳng cad.move
  var tip = null, LP = null;
  function tipOff(){ if (tip){ tip.remove(); tip = null; } }
  function hlHover(v){ var d = v.doc, e = d && v.mouse && v.hover && !v.drag && TC.cad.mode() === 'idle' && d.byId[v.hover]; return e && e.link ? e : null; }
  var mv0 = TC.cad.move;
  TC.cad.move = function(m, ev){
    mv0(m, ev); var e = hlHover(V()); if (!e){ tipOff(); return; }
    var r = $('#cv').getBoundingClientRect(), L = e.link;
    if (!tip){ tip = TC.el('<div class="tip hl-tip" role="tooltip"></div>'); document.body.appendChild(tip); }
    if (tip._id !== e.id){ tip._id = e.id; tip.innerHTML = '<span class="tn">' + ic('link', 's12') + '<span>' + esc(L.t || hlWhere(L)) + '</span></span>' + (L.t ? '<span class="tc">' + esc(hlWhere(L)) + '</span>' : '') + '<span class="tf">' + CTRL() + ' + bấm để mở</span>'; }
    var x = r.left + m[0] + 16, y = r.top + m[1] + 32; tip.style.left = Math.max(4, Math.min(x, innerWidth - tip.offsetWidth - 6)) + 'px'; tip.style.top = (y + tip.offsetHeight > innerHeight - 6 ? r.top + m[1] - tip.offsetHeight - 10 : y) + 'px';
  };
  var lv0 = TC.cad.leave; TC.cad.leave = function(){ lv0(); tipOff(); };
  document.addEventListener('mousedown', tipOff, true);
  (TC.overlays = TC.overlays || []).push(function(ctx, v){
    var e = hlHover(v); if (!e) return; var m = v.mouse, x = Math.round(m[0]) + 10, y = Math.round(m[1]) + 9, c = TC.cssv('--c-accent');
    ctx.save(); ctx.fillStyle = TC.cssv('--c-dyn-bg'); ctx.strokeStyle = c; ctx.lineWidth = 1; ctx.fillRect(x, y, 20, 18); ctx.strokeRect(x + .5, y + .5, 19, 17);
    ctx.translate(x + 2, y + 1); ctx.lineWidth = 1.4; ctx.lineCap = 'square'; (LP || (LP = TC.ICONS.link.match(/d="[^"]+"/g).map(function(s){ return new Path2D(s.slice(3, -1)); }))).forEach(function(p){ ctx.stroke(p); }); ctx.restore();
  });
  var ck0 = TC.cad.click;
  TC.cad.click = function(p, ev){ if (ev && (ev.ctrlKey || ev.metaKey) && TC.cad.mode() === 'idle'){ var d = D(), h = V().hitTest(p[0], p[1]), e = h && d.byId[h]; if (e && e.link){ tipOff(); hlOpen(e.link); return true; } } return ck0(p, ev); };
  function hlStrip(es){ ED.run('HYPERLINK', function(){ es.forEach(function(e){ ED.mod(e, function(x){ delete x.link; }); }); }); V().draw(); }
  function hlAsk(n){ return TC.confirm({ title:'Gỡ hyperlink', msg:'Gỡ liên kết khỏi <b>' + TC.n(n) + '</b> đối tượng?', detail:'Đối tượng giữ nguyên, chỉ bỏ liên kết. Hoàn tác được bằng Ctrl+Z.', ok:'Gỡ ' + TC.n(n) + ' liên kết', danger:true }); }
  // menu chuột phải: Viewer mở, sao chép được; gắn, sửa, gỡ là của Pro (huy hiệu khóa, bấm thì mở trang nâng cấp)
  TC.cad.ctxExt.push(function(it, first, d){
    if (!first) return; var L = first.link, pro = TC.can('PRO'), lk = pro ? '' : ' <span class="ed xs" data-ed="pro">Pro</span>', es = d.sel.map(function(id){ return d.byId[id]; }).filter(function(e){ return e && e.link && !(d.layerMap[e.layer] || {}).lck; });
    var p = function(f, fn){ return function(){ if (pro) fn(); else SC.upgrade(f, 'PRO'); }; };
    if (L) it.push({ label:'Hyperlink', icon:'link', sub:[{ label:'Mở “' + esc(L.t || L.a.split('\\').pop()) + '”', act:function(){ hlOpen(L); } }, { label:'Sao chép liên kết', icon:'copyclip', act:function(){ TC.copyText(L.a); } }, { sep:1 },
      { label:'Sửa hyperlink…' + lk, act:p('Sửa hyperlink', function(){ TC.cmd.run('HYPERLINK'); }) }, { label:'Gỡ hyperlink' + lk, dis:pro && !es.length, act:p('Gỡ hyperlink', function(){ hlAsk(es.length).then(function(y){ if (y){ hlStrip(es); TC.cmdline.msg('Đã gỡ liên kết khỏi ' + TC.n(es.length) + ' đối tượng.'); } }); }) }] });
    else it.push({ label:'Gắn hyperlink…' + lk, icon:'link', act:p('Gắn hyperlink', function(){ TC.cmd.run('HYPERLINK'); }) });
  });
  SC.hlCount = function(d){ return d ? allEnts(d).filter(function(e){ return e.link; }).length : 0; };
  SC.hyperlink = function(es){
    return new Promise(function(res){
      var d = D(), has = es.filter(function(e){ return e.link; }), L0 = has.length ? has[0].link : null, inner = !!L0 && (L0.to === 'view' || L0.to === 'layout'), pick = inner ? L0.to + ':' + L0.a : '', rt = 'Tệp gần đây', n = es.length;
      var addr = L0 && !inner ? L0.a : '', rel = !L0 || L0.to !== 'file' || /^\.\.?\\/.test(L0.a);
      var dlg = TC.dialog({ id:'hl', title:L0 ? 'Sửa hyperlink' : 'Gắn hyperlink', cmd:'HYPERLINK', code:'UI-F15', w:660, focus:'#hl-t', onClose:function(r){ if (r !== true) res(null); },
        body:'<div class="stack">' + H.row('<label for="hl-t">Chữ hiển thị</label>', '<input class="inp grow" id="hl-t" value="' + esc(L0 ? L0.t || '' : '') + '" placeholder="Để trống thì tooltip hiện đường dẫn" autocomplete="off">', 92) +
          '<div class="hlw"><div class="tree hl-nav" role="tablist" aria-label="Liên kết tới"><span class="gh">Liên kết tới</span>' + [['out', 'globe', 'Tệp hoặc trang web'], ['in', 'views', 'View, layout trong bản vẽ']].map(function(t){ return '<button class="tn" type="button" role="tab" data-hk="' + t[0] + '" aria-selected="' + ((t[0] === 'in') === inner) + '">' + ic(t[1]) + esc(t[2]) + '</button>'; }).join('') + '</div><div class="stack" id="hl-p"></div></div>' +
          (n > 1 ? '<div class="note">' + ic('info') + '<span>Gắn cùng một liên kết cho ' + TC.n(n) + ' đối tượng' + (has.length ? '; ' + TC.n(has.length) + ' đối tượng đang có liên kết sẽ được thay' : '') + '.</span></div>' : '') + '</div>',
        foot:(has.length ? '<button class="btn" type="button" id="hl-rm">' + ic('trash') + 'Gỡ liên kết' + (has.length > 1 ? ' (' + TC.n(has.length) + ')' : '') + '</button>' : '') + '<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="hl-ok">' + (n > 1 ? 'Gắn cho ' + TC.n(n) + ' đối tượng' : L0 ? 'Lưu liên kết' : 'Gắn liên kết') + '</button>' });
      function kindOf(a){ return !a ? '' : /^(https?:\/\/|www\.)/i.test(a) ? 'web' : /^[a-z]:\\|^\\\\|^\.\.?\\/i.test(a) || /\.(dwg|dxf|pdf|xlsx?|docx?|jpe?g|png|tiff?|txt|csv)$/i.test(a) ? 'file' : /^[\w-]+(\.[\w-]+)+(\/\S*)?$/.test(a) ? 'web' : 'file'; }
      function inDoc(){ var seen = {}; allEnts(d).forEach(function(e){ var L = e.link; if (!L || L.to === 'view' || L.to === 'layout') return; var s = seen[L.a] || (seen[L.a] = [L.a, L.t || '', 0]); s[2]++; }); return Object.keys(seen).map(function(k){ var s = seen[k]; return [s[0], s[1], TC.n(s[2]) + ' đối tượng']; }); }
      function recent(){ var list = rt === 'Tệp gần đây' ? HLF : rt === 'Trang web đã mở' ? HLW.map(function(w){ return [w[0], w[1], '']; }) : inDoc();
        return list.length ? list.map(function(x){ return '<button class="tn" type="button" role="option" data-ha="' + esc(x[0]) + '" data-ht="' + esc(x[1]) + '" data-tip="' + esc(x[1] || x[0]) + '" aria-selected="' + (!!addr && hlAbs(x[0]).toLowerCase() === hlAbs(addr).toLowerCase()) + '"><span class="mono hl-a">' + esc(x[0]) + '</span><span class="cnt">' + esc(x[2] || '') + '</span></button>'; }).join('') : '<span class="faint" style="padding:6px 8px">Bản vẽ chưa có liên kết tới tệp hoặc trang web.</span>'; }
      function opt(k, name, icon){ var v = k + ':' + name; return '<button class="tn" type="button" role="option" data-hv="' + esc(v) + '" aria-selected="' + (v === pick) + '">' + ic(icon) + esc(name) + '</button>'; }
      function check(){
        var a = $('#hl-a', dlg.el), m = $('#hl-m', dlg.el); if (!a) return; var v = a.value.trim(), k = kindOf(v), f = k === 'file' && hlFile(v); a.classList.remove('err'); m.className = 'fmsg hint';
        m.innerHTML = !v ? 'Gõ đường dẫn tệp, địa chỉ web, hoặc chọn trong danh sách bên dưới.' : k === 'web' ? ic('globe', 'xs') + 'Trang web · mở bằng trình duyệt mặc định' + (/^https?:/i.test(v) ? '' : ' · tự thêm https://') : f ? ic('file', 'xs') + esc(f[1]) + ' · ' + esc(f[2]) : ic('warn', 'xs') + 'Không thấy tệp này trên máy. Vẫn gắn được; kiểm tra lại đường dẫn.';
      }
      function pane(){
        var p = $('#hl-p', dlg.el);
        if (inner){ p.innerHTML = '<div class="tblwrap" style="height:228px"><div class="tree" id="hl-v" role="listbox" aria-label="View và layout"><div class="qc-vg">Model · view đã đặt tên</div>' + (d.views.length ? d.views.map(function(v){ return opt('view', v.name, 'views'); }).join('') : '<span class="faint hl-e">Bản vẽ chưa có view đã đặt tên. Tạo trong Trình quản lý view (VIEW).</span>') +
            '<div class="qc-vg">Layout</div>' + (d.layouts.length ? d.layouts.map(function(L){ return opt('layout', L.name, 'layout'); }).join('') : '<span class="faint hl-e">Bản vẽ chưa có layout.</span>') + '</div></div><span class="fmsg hint" id="hl-m">' + CTRL() + ' + bấm vào đối tượng sẽ chuyển tới view hoặc layout đã chọn.</span>'; return; }
        p.innerHTML = H.row('<label for="hl-a">Địa chỉ</label>', '<input class="inp mono grow" id="hl-a" value="' + esc(addr) + '" placeholder="D:\\…\\tep.pdf hoặc https://…" spellcheck="false" autocomplete="off"><button class="btn sm" type="button" id="hl-f">' + ic('open') + 'Chọn tệp…</button>', 56) + '<span class="fmsg" id="hl-m"></span>' +
          SC.seg('hlr', ['Tệp gần đây', 'Trang web đã mở', 'Liên kết trong bản vẽ'], rt) + '<div class="tblwrap" style="height:136px"><div class="tree hl-r" id="hl-r" role="listbox" aria-label="Địa chỉ gần đây">' + recent() + '</div></div>' + H.chk('hl-rel', 'Dùng đường dẫn tương đối với thư mục bản vẽ', rel);
        check();
      }
      function bad(t){ var m = $('#hl-m', dlg.el); m.className = 'fmsg err'; m.innerHTML = ic('warn', 'xs') + esc(t); var a = $('#hl-a', dlg.el); if (a){ a.classList.add('err'); a.focus(); } }
      function ok(){
        var t = $('#hl-t', dlg.el).value.trim(), L;
        if (inner){ if (!pick) return bad('Chọn một view hoặc layout.'); var k = pick.indexOf(':'); L = { to:pick.slice(0, k), a:pick.slice(k + 1) }; }
        else { var a = $('#hl-a', dlg.el).value.trim(); if (!a) return bad('Nhập đường dẫn tệp hoặc địa chỉ trang web.'); L = kindOf(a) === 'web' ? { to:'web', a:/^https?:\/\//i.test(a) ? a : 'https://' + a } : { to:'file', a:$('#hl-rel', dlg.el).checked ? hlRel(hlAbs(a)) : hlAbs(a) }; }
        if (t) L.t = t; dlg.close(true); res({ link:L });
      }
      dlg.el.addEventListener('click', function(e){
        var t = e.target.closest('[data-hk]'); if (t){ var was = inner; inner = t.dataset.hk === 'in'; if (was !== inner){ if (!was) addr = $('#hl-a', dlg.el).value; $$('[data-hk]', dlg.el).forEach(function(b){ b.setAttribute('aria-selected', String(b === t)); }); pane(); } return; }
        var a = e.target.closest('[data-ha]'); if (a){ addr = a.dataset.ha; $('#hl-a', dlg.el).value = addr; if (!$('#hl-t', dlg.el).value.trim()) $('#hl-t', dlg.el).value = a.dataset.ht; $$('[data-ha]', dlg.el).forEach(function(b){ b.setAttribute('aria-selected', String(b === a)); }); check(); return; }
        var v = e.target.closest('[data-hv]'); if (v){ pick = v.dataset.hv; $$('[data-hv]', dlg.el).forEach(function(b){ b.setAttribute('aria-selected', String(b === v)); }); var m = $('#hl-m', dlg.el); m.className = 'fmsg hint'; m.textContent = CTRL() + ' + bấm vào đối tượng sẽ chuyển tới ' + (pick.indexOf('view:') === 0 ? 'view' : 'layout') + ' “' + pick.slice(pick.indexOf(':') + 1) + '”.'; return; }
        var f = e.target.closest('#hl-f'); if (f){ var r = f.getBoundingClientRect(); TC.menu([{ h:'Tệp trong thư mục dự án' }].concat(HLF.map(function(x){ return { label:esc(x[1]) + ' <span class="faint">· ' + esc(x[0].split('\\').pop()) + '</span>', icon:'file', act:function(){ addr = x[0]; $('#hl-a', dlg.el).value = x[0]; if (!$('#hl-t', dlg.el).value.trim()) $('#hl-t', dlg.el).value = x[1]; check(); } }; })), r.left, r.bottom + 2, { returnFocus:f, minWidth:300 }); return; }
        if (e.target.closest('#hl-rm')) hlAsk(has.length).then(function(y){ if (y){ dlg.close(true); res({ remove:true }); } });
        else if (e.target.closest('#hl-ok')) ok();
      });
      dlg.el.addEventListener('dblclick', function(e){ if (e.target.closest('[data-ha], [data-hv]')) ok(); });
      dlg.el.addEventListener('input', function(e){ if (e.target.id === 'hl-a'){ addr = e.target.value; check(); } });
      dlg.el.addEventListener('segchange', function(e){ rt = e.detail; $('#hl-r', dlg.el).innerHTML = recent(); });
      pane();
    });
  };
  IMPL.HYPERLINK = function(ctx){
    return ctx.sel('Chọn đối tượng để gắn hyperlink').then(function(ids){
      var ok = ED.editable(ids, ctx); if (!ok.length) return; var d = D(), es = ok.map(function(id){ return d.byId[id]; });
      return SC.hyperlink(es).then(function(r){
        if (!r) return;
        if (r.remove){ var w = es.filter(function(e){ return e.link; }); hlStrip(w); ctx.out('Đã gỡ liên kết khỏi ' + TC.n(w.length) + ' đối tượng.'); return; }
        ED.run('HYPERLINK', function(){ es.forEach(function(e){ ED.mod(e, function(x){ x.link = Object.assign({}, r.link); }); }); }); V().draw();
        ctx.out('Đã gắn liên kết <b>' + esc(r.link.t || hlWhere(r.link)) + '</b> cho ' + TC.n(es.length) + ' đối tượng. Rê chuột lên đối tượng để xem; ' + CTRL() + ' + bấm để mở.');
      });
    });
  };
  // dữ liệu mẫu: vài liên kết trên ghi chú, tên bản vẽ của Banve-01 (không vào hoàn tác)
  function seedLinks(d){
    var by = function(re){ return d.ents.filter(function(e){ return e.t === 'text' && e.p[0] < 40000 && re.test(e.s); })[0]; }, set = function(e, L){ if (e) e.link = L; return e; };
    set(by(/^GHI CHÚ/), { to:'file', a:'..\\HoSo\\Thuyet-minh-kien-truc.pdf', t:'Thuyết minh kiến trúc' });
    set(by(/^1\. /), { to:'web', a:HLW[1][0], t:HLW[1][1] });
    set(by(/^4\. /), { to:'file', a:'.\\KT-12-Thong-ke-cua.dwg', t:'Bản vẽ KT-12: thống kê cửa' });
    set(by(/^MẶT BẰNG TẦNG 1$/), { to:'layout', a:'A1-Mat bang', t:'Layout A1-Mat bang (in tỉ lệ 1:50)' });
    return by(/^GHI CHÚ/);
  }
  OPEN['UI-F15'] = function(){
    var d = SC.cat.MOD(), e = seedLinks(d); V().zoomTo([-1200, -6200, 21800, 9800]); TC.cad.clearSel();
    if (e){ d.sel = [e.id]; TC.cad.afterSel(); setTimeout(function(){ TC.cmd.run('HYPERLINK'); }, 60); }
    TC.toast('Rê chuột lên chữ GHI CHÚ, ghi chú 1, 4 hay tên MẶT BẰNG TẦNG 1: con trỏ có huy hiệu liên kết; ' + CTRL() + ' + bấm để mở.', { type:'info', log:false, ms:9000 });
  };
})();
