/* TrionCAD prototype · bản vẽ mẫu (đơn vị mm, trục Y hướng lên).
   Banve-01.dwg: nhà ở 2 tầng, Model có 2 mặt bằng + 1 mặt cắt trong khung A3 tỉ lệ 1:100; 2 layout A1.
   Ketcau-T2.dwg: mặt bằng kết cấu tầng 2. Dữ liệu là mẫu cho prototype. */
(function(){
  'use strict';
  var TC = window.TC;

  /* ---------- TCVN3 (ABC): mã hóa để hiện "chữ lỗi" như AutoCAD thấy khi thiếu font ---------- */
  var T3 = { 'ă':'¨','â':'©','ê':'ª','ô':'«','ơ':'¬','ư':'\u00AD','đ':'®','Ă':'¡','Â':'¢','Ê':'£','Ô':'¤','Ơ':'¥','Ư':'¦','Đ':'§',
    'à':'µ','ả':'¶','ã':'·','á':'¸','ạ':'¹','ằ':'»','ẳ':'¼','ẵ':'½','ắ':'¾','ặ':'Æ','ầ':'Ç','ẩ':'È','ẫ':'É','ấ':'Ê','ậ':'Ë',
    'è':'Ì','ẻ':'Î','ẽ':'Ï','é':'Ð','ẹ':'Ñ','ề':'Ò','ể':'Ó','ễ':'Ô','ế':'Õ','ệ':'Ö','ì':'×','ỉ':'Ø','ĩ':'Ü','í':'Ý','ị':'Þ',
    'ò':'ß','ỏ':'á','õ':'â','ó':'ã','ọ':'ä','ồ':'å','ổ':'æ','ỗ':'ç','ố':'è','ộ':'é','ờ':'ê','ở':'ë','ỡ':'ì','ớ':'í','ợ':'î',
    'ù':'ï','ủ':'ñ','ũ':'ò','ú':'ó','ụ':'ô','ừ':'õ','ử':'ö','ữ':'÷','ứ':'ø','ự':'ù','ỳ':'ú','ỷ':'û','ỹ':'ü','ý':'ý','ỵ':'þ' };
  // font "H" (.VnTimeH) dùng mã chữ thường để vẽ chữ hoa có dấu
  TC.tcvn3 = function(s, upperFont){ return Array.from(upperFont ? s.toLowerCase() : s).map(function(c){ return T3[c] || c; }).join('').replace(/\u00AD/g, '\u00AD'); };
  TC.vniSample = { 'Cửa đi':'Cöûa ñi', 'Phòng khách':'Phoøng khaùch', 'P. NGỦ 1':'P. NGUÛ 1', 'P. NGỦ 2':'P. NGUÛ 2', 'BẾP':'BEÁP', 'KHO':'KHO', 'WC':'WC', 'PHÒNG KHÁCH + ĂN':'PHOØNG KHAÙCH + AÊN',
    'PHÒNG SINH HOẠT':'PHOØNG SINH HOAÏT', 'P. NGỦ 3':'P. NGUÛ 3', 'P. NGỦ 4':'P. NGUÛ 4', 'SẢNH':'SAÛNH', 'BAN CÔNG':'BAN COÂNG', 'P. NGỦ MASTER':'P. NGUÛ MASTER' };
  TC.rawText = function(doc, e){
    var st = doc.styleMap[e.style] || {};
    if (st.enc === 'TCVN3') return TC.tcvn3(e.s, st.upper);
    if (st.enc === 'VNI') return TC.vniSample[e.s] || e.s.normalize('NFD').replace(/[\u0300-\u036f]/g, function(m){ return { '\u0300':'ø', '\u0301':'ù', '\u0303':'õ', '\u0309':'û', '\u0323':'ï' }[m] || ''; });
    return e.s;
  };

  /* ---------- trình dựng ---------- */
  function B(doc){ this.doc = doc; this.list = doc.ents; }
  B.prototype.add = function(o, list){ o.id = ++this.doc.seq; (list || this.list).push(o); return o; };
  function rp(x0, y0, x1, y1){ return [[x0,y0],[x1,y0],[x1,y1],[x0,y1]]; }
  TC.rp = rp;

  var LAYERS = [
    ['0', 7, 'Continuous', 0.25, ''],
    ['Defpoints', 7, 'Continuous', 0.25, 'Điểm gốc kích thước, không in'],
    ['KT-TRUC', 1, 'CENTER', 0.18, 'Trục định vị'],
    ['KT-TUONG', 7, 'Continuous', 0.5, 'Mép tường xây'],
    ['KT-HATCH-TUONG', 8, 'Continuous', 0.13, 'Ký hiệu vật liệu tường'],
    ['KT-COT', 7, 'Continuous', 0.5, 'Cột'],
    ['KT-CUA', 2, 'Continuous', 0.25, 'Cửa đi'],
    ['KT-CUASO', 4, 'Continuous', 0.25, 'Cửa sổ, vách kính'],
    ['KT-NOITHAT', 8, 'Continuous', 0.18, 'Đồ đạc, nội thất'],
    ['KT-TBVS', 140, 'Continuous', 0.18, 'Thiết bị vệ sinh'],
    ['KT-THANG', 9, 'Continuous', 0.25, 'Cầu thang'],
    ['KT-DIM', 3, 'Continuous', 0.18, 'Kích thước'],
    ['KT-TEXT', 7, 'Continuous', 0.25, 'Tên phòng, diện tích'],
    ['KT-KYHIEU', 2, 'Continuous', 0.25, 'Cao độ, hướng Bắc, ký hiệu'],
    ['KT-GHICHU', 7, 'Continuous', 0.25, 'Ghi chú chung'],
    ['KT-MATCAT', 7, 'Continuous', 0.5, 'Nét cắt mặt cắt'],
    ['KT-KHUNG', 7, 'Continuous', 0.5, 'Khung bản vẽ, khung tên'],
    ['KT-VIEWPORT', 8, 'Continuous', 0.13, 'Khung viewport (không in)'],
    ['KC-COT', 7, 'Continuous', 0.5, ''], ['KC-DAM', 6, 'Continuous', 0.35, ''], ['KC-THEP', 1, 'Continuous', 0.25, ''], ['KC-DIM', 3, 'Continuous', 0.18, ''], ['KC-TEXT', 7, 'Continuous', 0.25, '']
  ];
  function makeDoc(o){
    var doc = { id:o.id, name:o.name, path:o.path, ver:o.ver || 'DWG 2018', bytes:o.bytes, savedBy:o.savedBy, modified:o.modified, seq:0,
      ents:[], blocks:{}, layouts:[], views:[], layers:[], styles:[], dimstyles:['Standard','TCVN-1-100','TCVN-1-50'], xrefs:[], images:[],
      pageSetups:[], units:{ name:'mm', prec:2 }, banners:[], sel:[], viewHist:[], space:'model', tempLayers:{}, recovered:false };
    LAYERS.forEach(function(l){ doc.layers.push({ name:l[0], color:l[1], lt:l[2], lw:l[3], desc:l[4], on:true, frz:false, lck:false, plot:l[0] !== 'Defpoints' && l[0] !== 'KT-VIEWPORT', vpfrz:false }); });
    doc.styles = [
      { name:'Standard', font:'Arial', kind:'TTF', enc:'Unicode', h:0 },
      { name:'VNTIME', font:'vntime.shx', kind:'SHX', enc:'TCVN3', missing:true, sub:'TrionVN-Times.shx', css:'"Times New Roman", Times, serif', h:0 },
      { name:'VNTIMEH', font:'.VnTimeH', kind:'TTF', enc:'TCVN3', upper:true, missing:true, sub:'TrionVN-Times (hoa)', css:'"Times New Roman", Times, serif', h:0 },
      { name:'VNI', font:'VNI-Times', kind:'TTF', enc:'VNI', missing:true, sub:'Times New Roman', css:'"Times New Roman", Times, serif', h:0 },
      { name:'ROMANS', font:'romans.shx', kind:'SHX', enc:'Không rõ', css:'"Arial Narrow", Arial, sans-serif', h:0 }
    ];
    doc.styleMap = {}; doc.styles.forEach(function(s){ doc.styleMap[s.name] = s; });
    doc.layerMap = {}; doc.layers.forEach(function(l){ doc.layerMap[l.name] = l; });
    return doc;
  }

  /* ---------- block dùng chung ---------- */
  function defBlocks(doc){
    var b = new B(doc), bl = doc.blocks;
    function def(name, desc, fn){ var list = []; fn(function(o){ return b.add(o, list); }); bl[name] = { name:name, desc:desc, ents:list }; }
    def('CUA-DI-1C', 'Cửa đi một cánh, cỡ 1 = bề rộng lỗ cửa', function(a){ a({ t:'line', p1:[0,0], p2:[0,1], layer:'0' }); a({ t:'arc', c:[0,0], r:1, a0:0, a1:90, layer:'0' }); });
    def('CUA-SO', 'Cửa sổ, x = bề rộng, y = độ dày tường', function(a){ [-0.5, 0.5, -0.1, 0.1].forEach(function(y){ a({ t:'line', p1:[0,y], p2:[1,y], layer:'0' }); }); });
    def('SOFA-L', 'Sofa góc chữ L 2200×2200', function(a){ a({ t:'pline', pts:[[0,0],[2200,0],[2200,800],[800,800],[800,2200],[0,2200]], closed:true, layer:'0' }); a({ t:'pline', pts:[[200,2200],[200,200],[2200,200]], layer:'0' }); });
    def('BAN-TRA', 'Bàn trà 900×700', function(a){ a({ t:'pline', pts:rp(0,0,900,700), closed:true, layer:'0' }); a({ t:'pline', pts:rp(60,60,840,640), closed:true, layer:'0' }); });
    def('KE-TIVI', 'Kệ tivi 1800×400', function(a){ a({ t:'pline', pts:rp(0,0,1800,400), closed:true, layer:'0' }); a({ t:'line', p1:[300,300], p2:[1500,300], layer:'0' }); });
    def('BAN-AN-6', 'Bàn ăn 6 ghế 1800×1000', function(a){
      a({ t:'pline', pts:rp(0,0,1800,1000), closed:true, layer:'0' });
      [175,675,1175].forEach(function(x){ a({ t:'pline', pts:rp(x,-550,x+450,-100), closed:true, layer:'0' }); a({ t:'line', p1:[x,-480], p2:[x+450,-480], layer:'0' }); a({ t:'pline', pts:rp(x,1100,x+450,1550), closed:true, layer:'0' }); a({ t:'line', p1:[x,1480], p2:[x+450,1480], layer:'0' }); });
    });
    def('BEP-L', 'Bếp chữ L có chậu rửa và bếp nấu', function(a){
      a({ t:'pline', pts:[[0,0],[3190,0],[3190,4090],[2590,4090],[2590,600],[0,600]], closed:true, layer:'0' });
      a({ t:'pline', pts:rp(1100,90,1900,510), closed:true, layer:'0' }); a({ t:'pline', pts:rp(1160,140,1480,470), closed:true, layer:'0' }); a({ t:'pline', pts:rp(1520,140,1840,470), closed:true, layer:'0' });
      a({ t:'pline', pts:rp(2590,2140,3190,3040), closed:true, layer:'0' });
      [[2740,2370],[3040,2370],[2740,2810],[3040,2810]].forEach(function(p){ a({ t:'circle', c:p, r:110, layer:'0' }); });
    });
    def('TU-LANH', 'Tủ lạnh 700×700', function(a){ a({ t:'pline', pts:rp(0,0,700,700), closed:true, layer:'0' }); a({ t:'line', p1:[0,0], p2:[700,700], layer:'0' }); });
    def('GIUONG-1600', 'Giường đôi 1600×2000', function(a){ a({ t:'pline', pts:rp(0,0,1600,2000), closed:true, layer:'0' }); a({ t:'pline', pts:rp(150,1560,750,1910), closed:true, layer:'0' }); a({ t:'pline', pts:rp(850,1560,1450,1910), closed:true, layer:'0' }); a({ t:'line', p1:[0,1010], p2:[1600,1010], layer:'0' }); });
    def('GIUONG-1800', 'Giường đôi 1800×2000', function(a){ a({ t:'pline', pts:rp(0,0,1800,2000), closed:true, layer:'0' }); a({ t:'pline', pts:rp(150,1560,850,1910), closed:true, layer:'0' }); a({ t:'pline', pts:rp(950,1560,1650,1910), closed:true, layer:'0' }); a({ t:'line', p1:[0,1010], p2:[1800,1010], layer:'0' }); });
    def('TAB-DAU-GIUONG', 'Tab đầu giường 450×450', function(a){ a({ t:'pline', pts:rp(0,0,450,450), closed:true, layer:'0' }); a({ t:'circle', c:[225,225], r:120, layer:'0' }); });
    def('TU-AO', 'Tủ áo 600×1800', function(a){ a({ t:'pline', pts:rp(0,0,600,1800), closed:true, layer:'0' }); a({ t:'line', p1:[300,0], p2:[300,1800], layer:'0' }); });
    def('BAN-LV', 'Bàn làm việc 600×1200 và ghế', function(a){ a({ t:'pline', pts:rp(0,0,600,1200), closed:true, layer:'0' }); a({ t:'pline', pts:rp(-470,360,-90,740), closed:true, layer:'0' }); });
    def('BON-CAU', 'Bồn cầu', function(a){ a({ t:'pline', pts:rp(-250,0,250,250), closed:true, layer:'0' }); a({ t:'ellipse', c:[0,-280], rx:200, ry:270, layer:'0' }); });
    def('LAVABO', 'Chậu rửa', function(a){ a({ t:'pline', pts:rp(0,0,595,450), closed:true, layer:'0' }); a({ t:'ellipse', c:[297,210], rx:190, ry:140, layer:'0' }); });
    def('KE-KHO', 'Kệ kho 1900×400', function(a){ a({ t:'pline', pts:rp(0,0,1900,400), closed:true, layer:'0' }); a({ t:'line', p1:[0,200], p2:[1900,200], layer:'0' }); });
    // khung A3 ngang theo TCVN, vẽ theo mm giấy, chèn với tỉ lệ 100 trong Model
    def('KHUNG-A3', 'Khung bản vẽ A3 ngang và khung tên', function(a){ titleFrame(a, 420, 297); });
    def('KHUNG-A1', 'Khung bản vẽ A1 ngang và khung tên (TCVN 5571)', function(a){ titleFrame(a, 841, 594); });
  }
  function titleFrame(a, W, H){
    a({ t:'pline', pts:rp(0,0,W,H), closed:true, layer:'0', lw:0.13 });
    a({ t:'pline', pts:rp(25,5,W - 5,H - 5), closed:true, layer:'0', lw:0.5 });
    var x0 = W - 5 - 180, y0 = 5, rows = [0, 8, 16, 24, 32, 40, 52];
    a({ t:'pline', pts:rp(x0, y0, W - 5, y0 + 52), closed:true, layer:'0', lw:0.5 });
    [8, 16, 24, 32, 40].forEach(function(y){ a({ t:'line', p1:[x0, y0 + y], p2:[x0 + 70, y0 + y], layer:'0', lw:0.18 }); });
    a({ t:'line', p1:[x0 + 70, y0], p2:[x0 + 70, y0 + 52], layer:'0' });
    a({ t:'line', p1:[x0 + 70, y0 + 20], p2:[W - 5, y0 + 20], layer:'0' }); a({ t:'line', p1:[x0 + 70, y0 + 36], p2:[W - 5, y0 + 36], layer:'0' });
    a({ t:'line', p1:[x0 + 135, y0], p2:[x0 + 135, y0 + 20], layer:'0' }); a({ t:'line', p1:[x0 + 25, y0], p2:[x0 + 25, y0 + 40], layer:'0', lw:0.18 });
    var L = function(x, y, s, h){ a({ t:'text', p:[x0 + x, y0 + y], h:h || 1.8, s:s, style:'VNTIMEH', al:'l', layer:'0' }); };
    L(2, 42.5, 'ĐƠN VỊ THIẾT KẾ', 2.2);
    [['CHỦ TRÌ',33],['THIẾT KẾ',25],['VẼ',17],['KIỂM TRA',9],['DUYỆT',1.5]].forEach(function(r){ L(2, r[1] + 1.2, r[0]); });
    L(72, 45, 'CÔNG TRÌNH'); L(72, 29, 'TÊN BẢN VẼ'); L(137, 13.5, 'KÝ HIỆU'); L(72, 13.5, 'TỈ LỆ'); L(100, 13.5, 'NGÀY');
    var A = function(tag, x, y, h, def, al){ a({ t:'attdef', tag:tag, p:[x0 + x, y0 + y], h:h, s:def, style:'VNTIMEH', al:al || 'l', layer:'0' }); };
    A('DONVI', 2, 46.5, 2.6, '[TÊN CÔNG TY]'); A('CONGTRINH', 72, 39, 2.8, 'NHÀ Ở GIA ĐÌNH 2 TẦNG'); A('TENBV', 72, 23, 3.2, 'MẶT BẰNG TẦNG 1');
    A('KYHIEU', 137, 5, 4, 'KT-01'); A('TILE', 72, 5, 3, '1:100'); A('NGAY', 100, 5, 3, '18/10/2026');
    A('CHUTRI', 27, 34, 1.8, 'N.M.AN'); A('THIETKE', 27, 26, 1.8, 'T.Q.BẢO'); A('VE', 27, 18, 1.8, 'L.T.CHI'); A('KIEMTRA', 27, 10, 1.8, 'N.M.AN');
  }

  /* ---------- mặt bằng: tường, cửa, nội thất, kích thước, chữ ---------- */
  var AX = [0,3900,7800,11700], AY = [0,4800,9000];
  function plan(doc, ox, oy, v){
    var b = new B(doc), E = function(o){ return b.add(o); };
    var X = function(x){ return x + ox; }, Y = function(y){ return y + oy; }, P = function(p){ return [X(p[0]), Y(p[1])]; };
    var rooms = v === 1 ? [
      { r:[110,110,7745,4745], n:'PHÒNG KHÁCH + ĂN', at:[3950,4150], h:260 },
      { r:[7855,110,11590,4745], n:'BẾP', at:[9300,2650] },
      { r:[110,4855,3845,8890], n:'P. NGỦ 1', at:[1900,6350] },
      { r:[3955,4855,7745,6845] },
      { r:[3955,6955,5645,8890], n:'WC', at:[4800,8150], small:true },
      { r:[5755,6955,7745,8890], n:'KHO', at:[6900,8150], small:true },
      { r:[7855,4855,11590,8890], n:'P. NGỦ 2', at:[9700,6350] } ] : [
      { r:[110,110,7745,4745], n:'PHÒNG SINH HOẠT', at:[3950,2600], h:260 },
      { r:[7855,110,11590,4745], n:'P. NGỦ 3', at:[9700,1600] },
      { r:[110,4855,3845,8890], n:'P. NGỦ MASTER', at:[1950,6350], h:240 },
      { r:[3955,4855,7745,6845] },
      { r:[3955,6955,5645,8890], n:'WC', at:[4800,8150], small:true },
      { r:[5755,6955,7745,8890], n:'SẢNH', at:[6750,8150], small:true },
      { r:[7855,4855,11590,8890], n:'P. NGỦ 4', at:[9700,6350] } ];
    // lỗ mở: [loại, trục, a, b, dày, hướng (h/v), bản lề ở a?, mở về +?]
    var ops = [
      ['door', 0, 2750, 3650, 220, 'h', true, true],
      ['door', 7800, 3300, 4100, 110, 'v', false, true],
      ['door', 4800, 2700, 3500, 110, 'h', false, true],
      ['door', 4800, 8200, 9000, 110, 'h', true, true],
      ['door', 6900, 4300, 5000, 110, 'h', false, true],
      ['door', 6900, 6100, 6800, 110, 'h', true, true],
      ['void', 4800, 4300, 7400, 110, 'h'],
      ['win', 0, 4800, 6600, 220, 'h'], ['win', 0, 9200, 10600, 220, 'h'], ['win', 9000, 4500, 5200, 220, 'h'],
      ['win', 0, 1400, 3200, 220, 'v'], ['win', 0, 6000, 7800, 220, 'v'], ['win', 11700, 6200, 7800, 220, 'v'], ['win', 11700, 1200, 2200, 220, 'v']
    ];
    if (v === 2) { ops[0] = ['win', 0, 2750, 3650, 220, 'h']; ops.push(['win', 9000, 8600, 10400, 220, 'h']); }
    // trục
    AX.forEach(function(x){ E({ t:'line', p1:[X(x), Y(-2600)], p2:[X(x), Y(9800)], layer:'KT-TRUC' }); });
    AY.forEach(function(y){ E({ t:'line', p1:[X(-2600), Y(y)], p2:[X(12500), Y(y)], layer:'KT-TRUC' }); });
    // hatch tường: biên ngoài, lòng phòng và lỗ mở là lỗ thủng (chẵn-lẻ)
    var loops = [rp(-110,-110,11810,9110)].concat(rooms.map(function(o){ return rp(o.r[0],o.r[1],o.r[2],o.r[3]); }));
    ops.forEach(function(o){ var a = o[2], bb = o[3], c = o[1], t = o[4] / 2; loops.push(o[5] === 'h' ? rp(a, c - t, bb, c + t) : rp(c - t, a, c + t, bb)); });
    E({ t:'hatch', loops:loops.map(function(l){ return l.map(P); }), pat:'ANSI31', sc:40, ang:0, layer:'KT-HATCH-TUONG', assoc:false });
    // mép tường: cạnh các hình chữ nhật, cắt đi đoạn trùng lỗ mở
    var faces = [];
    function edges(r){ faces.push([r[0],r[1],r[2],r[1]], [r[2],r[1],r[2],r[3]], [r[0],r[3],r[2],r[3]], [r[0],r[1],r[0],r[3]]); }
    edges([-110,-110,11810,9110]); rooms.forEach(function(o){ edges(o.r); });
    ops.forEach(function(o){
      var a = o[2], bb = o[3], c = o[1], t = o[4] / 2, out = [];
      faces.forEach(function(f){
        var horiz = f[1] === f[3];
        if (o[5] === 'h' && horiz && (Math.abs(f[1] - (c - t)) < 1 || Math.abs(f[1] - (c + t)) < 1)){
          var x1 = Math.min(f[0], f[2]), x2 = Math.max(f[0], f[2]);
          if (bb <= x1 || a >= x2) { out.push(f); return; }
          if (a > x1) out.push([x1, f[1], a, f[1]]); if (bb < x2) out.push([bb, f[1], x2, f[1]]);
        } else if (o[5] === 'v' && !horiz && (Math.abs(f[0] - (c - t)) < 1 || Math.abs(f[0] - (c + t)) < 1)){
          var y1 = Math.min(f[1], f[3]), y2 = Math.max(f[1], f[3]);
          if (bb <= y1 || a >= y2) { out.push(f); return; }
          if (a > y1) out.push([f[0], y1, f[0], a]); if (bb < y2) out.push([f[0], bb, f[0], y2]);
        } else out.push(f);
      });
      faces = out;
      if (o[5] === 'h') faces.push([a, c - t, a, c + t], [bb, c - t, bb, c + t]); else faces.push([c - t, a, c + t, a], [c - t, bb, c + t, bb]);
    });
    faces.forEach(function(f){ if (Math.hypot(f[2] - f[0], f[3] - f[1]) > 1) E({ t:'line', p1:[X(f[0]), Y(f[1])], p2:[X(f[2]), Y(f[3])], layer:'KT-TUONG' }); });
    // cửa đi và cửa sổ: block
    ops.forEach(function(o){
      var a = o[2], bb = o[3], c = o[1], t = o[4] / 2, w = bb - a;
      if (o[0] === 'door'){
        var hinge, closed, open;
        if (o[5] === 'h'){ hinge = [o[6] ? a : bb, c + (o[7] ? t : -t)]; closed = [o[6] ? 1 : -1, 0]; open = [0, o[7] ? 1 : -1]; }
        else { hinge = [c + (o[7] ? t : -t), o[6] ? a : bb]; closed = [0, o[6] ? 1 : -1]; open = [o[7] ? 1 : -1, 0]; }
        var rot = Math.atan2(closed[1], closed[0]) * 180 / Math.PI;
        var left = [-closed[1], closed[0]], mir = left[0] * open[0] + left[1] * open[1] < 0;
        E({ t:'insert', name:'CUA-DI-1C', p:P(hinge), sx:w, sy:mir ? -w : w, rot:rot, layer:'KT-CUA' });
      } else if (o[0] === 'win'){
        E({ t:'insert', name:'CUA-SO', p:o[5] === 'h' ? P([a, c]) : P([c, a]), sx:w, sy:o[4], rot:o[5] === 'h' ? 0 : 90, layer:'KT-CUASO' });
      }
    });
    // cột 300×300
    [[0,0],[3900,0],[7800,0],[11700,0],[0,4800],[3900,4800],[7800,4800],[11700,4800],[0,9000],[3900,9000],[7800,9000],[11700,9000]].forEach(function(p){
      E({ t:'hatch', loops:[rp(p[0]-150,p[1]-150,p[0]+150,p[1]+150).map(P)], pat:'SOLID', layer:'KT-COT', color:8 });
      E({ t:'pline', pts:rp(p[0]-150,p[1]-150,p[0]+150,p[1]+150).map(P), closed:true, layer:'KT-COT' });
    });
    // nội thất
    var I = function(name, x, y, layer, rot, sx, sy){ return E({ t:'insert', name:name, p:P([x, y]), sx:sx || 1, sy:sy || sx || 1, rot:rot || 0, layer:layer || 'KT-NOITHAT' }); };
    if (v === 1){
      I('SOFA-L', 400, 200); I('BAN-TRA', 1600, 1400); I('KE-TIVI', 600, 4300); I('BAN-AN-6', 5000, 1600);
      I('BEP-L', 8400, 110); I('TU-LANH', 8900, 4045);
      I('GIUONG-1600', 1100, 6890); I('TAB-DAU-GIUONG', 600, 8390); I('TAB-DAU-GIUONG', 2750, 8390); I('TU-AO', 3245, 5900);
      I('GIUONG-1800', 8800, 6890); I('TAB-DAU-GIUONG', 8300, 8390); I('TAB-DAU-GIUONG', 10650, 8390); I('BAN-LV', 10990, 5200);
      I('KE-KHO', 5800, 8490);
    } else {
      I('SOFA-L', 400, 200); I('BAN-TRA', 1600, 1400); I('KE-TIVI', 600, 4300);
      I('GIUONG-1600', 9500, 2600, 'KT-NOITHAT', 0); I('TU-AO', 10990, 300); I('BAN-LV', 8455, 300, 'KT-NOITHAT', 90);
      I('GIUONG-1800', 1000, 6890); I('TAB-DAU-GIUONG', 500, 8390); I('TAB-DAU-GIUONG', 2850, 8390); I('TU-AO', 3245, 5000);
      I('GIUONG-1600', 9100, 6890); I('TAB-DAU-GIUONG', 8600, 8390); I('TAB-DAU-GIUONG', 10750, 8390);
    }
    I('BON-CAU', 4300, 8640, 'KT-TBVS'); I('LAVABO', 5050, 8440, 'KT-TBVS');
    E({ t:'circle', c:P([4350, 7300]), r:60, layer:'KT-TBVS' });
    // cầu thang
    for (var k = 0; k <= 12; k++) E({ t:'line', p1:P([4300 + k*260, 5150]), p2:P([4300 + k*260, 6550]), layer:'KT-THANG' });
    E({ t:'line', p1:P([4300,5150]), p2:P([7420,5150]), layer:'KT-THANG' }); E({ t:'line', p1:P([4300,6550]), p2:P([7420,6550]), layer:'KT-THANG' });
    E({ t:'pline', pts:[[4400,5850],[7250,5850]].map(P), layer:'KT-THANG' }); E({ t:'pline', pts:[[7250,5760],[7420,5850],[7250,5940]].map(P), closed:true, layer:'KT-THANG' });
    E({ t:'pline', pts:[[5600,5150],[5900,5650],[5750,5700],[6050,6550]].map(P), layer:'KT-THANG' });
    E({ t:'text', p:P([4520, 6050]), h:170, s:v === 1 ? 'LÊN' : 'XUỐNG', style:'Standard', al:'l', layer:'KT-THANG' });
    // kích thước
    function dim(p1, p2, pos, dir){ E({ t:'dim', p1:P(p1), p2:P(p2), pos:dir === 'h' ? Y(pos) : X(pos), dir:dir, style:'TCVN-1-100', layer:'KT-DIM' }); }
    dim([0,-500],[3900,-500],-1200,'h'); dim([3900,-500],[7800,-500],-1200,'h'); dim([7800,-500],[11700,-500],-1200,'h'); dim([0,-1360],[11700,-1360],-1900,'h');
    dim([-500,0],[-500,4800],-1200,'v'); dim([-500,4800],[-500,9000],-1200,'v'); dim([-1360,0],[-1360,9000],-1900,'v');
    // bóng trục
    ['1','2','3','4'].forEach(function(n, k){ E({ t:'circle', c:P([AX[k], -3000]), r:400, layer:'KT-TRUC' }); E({ t:'text', p:P([AX[k], -3000]), h:340, s:n, style:'Standard', al:'m', layer:'KT-TRUC', color:7 }); });
    ['A','B','C'].forEach(function(n, k){ E({ t:'circle', c:P([-3000, AY[k]]), r:400, layer:'KT-TRUC' }); E({ t:'text', p:P([-3000, AY[k]]), h:340, s:n, style:'Standard', al:'m', layer:'KT-TRUC', color:7 }); });
    // tên phòng và diện tích: lấy diện tích thật của lòng phòng
    rooms.forEach(function(o){
      if (!o.n) return;
      var ar = (o.r[2] - o.r[0]) * (o.r[3] - o.r[1]) / 1e6;
      E({ t:'text', p:P(o.at), h:o.h || (o.small ? 220 : 300), s:o.n, style:'VNI', al:'m', layer:'KT-TEXT' });
      E({ t:'text', p:P([o.at[0], o.at[1] - (o.small ? 320 : 400)]), h:o.small ? 170 : 200, s:'S = ' + ar.toFixed(2) + ' m²', style:'Standard', al:'m', layer:'KT-TEXT', color:9 });
    });
    // cao độ
    E({ t:'pline', pts:[[1700,2800],[1560,3000],[1840,3000]].map(P), closed:true, layer:'KT-KYHIEU' }); E({ t:'line', p1:P([1560,3000]), p2:P([2700,3000]), layer:'KT-KYHIEU' });
    E({ t:'text', p:P([2130,3170]), h:180, s:v === 1 ? '±0.000' : '+3.600', style:'Standard', al:'m', layer:'KT-KYHIEU' });
    // tên bản vẽ, ghi chú (chữ mã TCVN3)
    E({ t:'text', p:P([5850,-4400]), h:480, s:v === 1 ? 'MẶT BẰNG TẦNG 1' : 'MẶT BẰNG TẦNG 2', style:'VNTIMEH', al:'m', layer:'KT-TEXT' });
    E({ t:'line', p1:P([3150,-4760]), p2:P([8550,-4760]), layer:'KT-TEXT' }); E({ t:'line', p1:P([3150,-4820]), p2:P([8550,-4820]), layer:'KT-TEXT' });
    E({ t:'text', p:P([5850,-5150]), h:280, s:'TL 1:100', style:'Standard', al:'m', layer:'KT-TEXT' });
    if (v === 1){
      E({ t:'text', p:P([13600,8550]), h:300, s:'GHI CHÚ:', style:'VNTIMEH', al:'l', layer:'KT-GHICHU' });
      ['1. Tường bao xây gạch dày 220, tường ngăn dày 110.','2. Cao độ ±0.000 lấy theo mặt sàn hoàn thiện tầng 1.','3. Kích thước ghi bằng mm, cao độ ghi bằng m.','4. Cửa đi, cửa sổ xem bảng thống kê bản vẽ KT-12.']
        .forEach(function(s, k){ E({ t:'text', p:P([13600, 8000 - k*480]), h:220, s:s, style:'VNTIME', al:'l', layer:'KT-GHICHU' }); });
      // hướng Bắc
      E({ t:'circle', c:P([17600,1500]), r:900, layer:'KT-KYHIEU' });
      E({ t:'pline', pts:[[17600,2500],[17250,700],[17600,1050]].map(P), closed:true, layer:'KT-KYHIEU' });
      E({ t:'hatch', loops:[[[17600,2500],[17950,700],[17600,1050]].map(P)], pat:'SOLID', layer:'KT-KYHIEU' });
      E({ t:'pline', pts:[[17600,2500],[17950,700],[17600,1050]].map(P), closed:true, layer:'KT-KYHIEU' });
      E({ t:'text', p:P([17600,2800]), h:360, s:'B', style:'Standard', al:'m', layer:'KT-KYHIEU' });
    } else {
      // ban công tầng 2
      E({ t:'pline', pts:[[3900,9110],[3900,10300],[7800,10300],[7800,9110]].map(P), layer:'KT-TUONG' });
      E({ t:'pline', pts:[[3980,9110],[3980,10220],[7720,10220],[7720,9110]].map(P), layer:'KT-TUONG' });
      E({ t:'text', p:P([5850,9750]), h:220, s:'BAN CÔNG', style:'VNI', al:'m', layer:'KT-TEXT' });
    }
  }

  /* ---------- mặt cắt 1-1 ---------- */
  function section(doc, ox, oy){
    var b = new B(doc), E = function(o){ return b.add(o); }, P = function(p){ return [p[0] + ox, p[1] + oy]; };
    var lv = [0, 3600, 7200, 10200];
    E({ t:'line', p1:P([-1500,-450]), p2:P([13200,-450]), layer:'KT-MATCAT' });
    E({ t:'hatch', loops:[rp(-1500,-900,13200,-450).map(P)], pat:'ANSI31', sc:60, layer:'KT-HATCH-TUONG' });
    lv.forEach(function(y, k){
      if (k > 0) { E({ t:'hatch', loops:[rp(-110, y - 120, 11810, y).map(P)], pat:'SOLID', layer:'KT-MATCAT', color:8 }); E({ t:'pline', pts:rp(-110, y - 120, 11810, y).map(P), closed:true, layer:'KT-MATCAT' }); }
      else E({ t:'pline', pts:rp(-110, -450, 11810, 0).map(P), closed:true, layer:'KT-MATCAT' });
      E({ t:'pline', pts:[[12400, y],[12260, y + 200],[12540, y + 200]].map(P), closed:true, layer:'KT-KYHIEU' });
      E({ t:'line', p1:P([12260, y + 200]), p2:P([13500, y + 200]), layer:'KT-KYHIEU' });
      E({ t:'text', p:P([12900, y + 380]), h:180, s:k === 0 ? '±0.000' : '+' + (y / 1000).toFixed(3), style:'Standard', al:'m', layer:'KT-KYHIEU' });
    });
    [0, 3900, 7800, 11700].forEach(function(x, k){
      var t = (k === 0 || k === 3) ? 110 : 55;
      for (var f = 0; f < 3; f++){
        var y0 = lv[f], y1 = lv[f + 1] - 120;
        if (k === 0 && f < 2){ E({ t:'pline', pts:rp(x - t, y0, x + t, y0 + 900).map(P), closed:true, layer:'KT-MATCAT' }); E({ t:'pline', pts:rp(x - t, y0 + 2400, x + t, y1).map(P), closed:true, layer:'KT-MATCAT' });
          [-40, 40].forEach(function(d){ E({ t:'line', p1:P([x + d, y0 + 900]), p2:P([x + d, y0 + 2400]), layer:'KT-CUASO' }); }); }
        else E({ t:'pline', pts:rp(x - t, y0, x + t, y1).map(P), closed:true, layer:'KT-MATCAT' });
      }
      E({ t:'line', p1:P([x, -1300]), p2:P([x, 11000]), layer:'KT-TRUC' });
      E({ t:'circle', c:P([x, -1800]), r:400, layer:'KT-TRUC' }); E({ t:'text', p:P([x, -1800]), h:340, s:String(k + 1), style:'Standard', al:'m', layer:'KT-TRUC', color:7 });
    });
    E({ t:'pline', pts:[[-600,10200],[5850,12400],[12300,10200]].map(P), layer:'KT-MATCAT' });
    for (var s2 = 0; s2 < 12; s2++){ E({ t:'line', p1:P([4300 + s2*260, s2*300]), p2:P([4300 + s2*260, (s2 + 1)*300]), layer:'KT-THANG' }); E({ t:'line', p1:P([4300 + s2*260, (s2 + 1)*300]), p2:P([4300 + (s2 + 1)*260, (s2 + 1)*300]), layer:'KT-THANG' }); }
    E({ t:'text', p:P([5850,-3200]), h:480, s:'MẶT CẮT 1-1', style:'VNTIMEH', al:'m', layer:'KT-TEXT' });
    E({ t:'line', p1:P([3800,-3560]), p2:P([7900,-3560]), layer:'KT-TEXT' }); E({ t:'line', p1:P([3800,-3620]), p2:P([7900,-3620]), layer:'KT-TEXT' });
    E({ t:'text', p:P([5850,-3950]), h:280, s:'TL 1:100', style:'Standard', al:'m', layer:'KT-TEXT' });
  }

  function frameA3(doc, x, y, attrs){
    var b = new B(doc);
    b.add({ t:'insert', name:'KHUNG-A3', p:[x, y], sx:100, sy:100, rot:0, layer:'KT-KHUNG', attrs:attrs });
  }

  function buildDoc1(){
    var doc = makeDoc({ id:'d1', name:'Banve-01.dwg', path:'D:\\DuAn\\NhaO-2T\\KienTruc\\Banve-01.dwg', bytes:4.8 * 1048576, savedBy:'AutoCAD 2024', modified:'16/10/2026 17:42' });
    defBlocks(doc);
    plan(doc, 0, 0, 1);
    plan(doc, 48000, 0, 2);
    section(doc, 96000, 1000);
    frameA3(doc, -15000, -11000, { TENBV:'MẶT BẰNG TẦNG 1', KYHIEU:'KT-01', TILE:'1:100' });
    frameA3(doc, 33000, -11000, { TENBV:'MẶT BẰNG TẦNG 2', KYHIEU:'KT-02', TILE:'1:100' });
    frameA3(doc, 81000, -11000, { TENBV:'MẶT CẮT 1-1', KYHIEU:'KT-03', TILE:'1:100' });
    // layout A1: hai viewport mặt bằng tỉ lệ 1:50; layout mặt cắt: một viewport
    var lb = new B(doc);
    var L1 = { name:'A1-Mat bang', paper:{ w:841, h:594, name:'ISO A1 (841,00 × 594,00 mm)' }, margin:[5,5,5,5], ents:[], vports:[], setup:'A1-PDF' };
    lb.add({ t:'insert', name:'KHUNG-A1', p:[0,0], sx:1, sy:1, rot:0, layer:'KT-KHUNG', attrs:{ TENBV:'MẶT BẰNG TẦNG 1, TẦNG 2', KYHIEU:'KT-04', TILE:'1:50' } }, L1.ents);
    L1.vports.push({ id:'vp1', x:35, y:70, w:380, h:505, c:[5850, 2300], scale:50, locked:true, frz:['KT-KHUNG'] });
    L1.vports.push({ id:'vp2', x:425, y:70, w:405, h:505, c:[48000 + 5850, 2600], scale:50, locked:true, frz:['KT-KHUNG'] });
    var L2 = { name:'A1-Mat cat', paper:{ w:841, h:594, name:'ISO A1 (841,00 × 594,00 mm)' }, margin:[5,5,5,5], ents:[], vports:[], setup:'A1-PDF' };
    lb.add({ t:'insert', name:'KHUNG-A1', p:[0,0], sx:1, sy:1, rot:0, layer:'KT-KHUNG', attrs:{ TENBV:'MẶT CẮT 1-1', KYHIEU:'KT-05', TILE:'1:50' } }, L2.ents);
    L2.vports.push({ id:'vp3', x:35, y:70, w:795, h:505, c:[96000 + 5900, 1000 + 4200], scale:50, locked:false, frz:['KT-KHUNG'] });
    doc.layouts = [L1, L2];
    doc.views = [ { name:'Mặt bằng tầng 1', c:[8400, 2300], h:16000 }, { name:'Mặt bằng tầng 2', c:[48000 + 6000, 2300], h:16000 }, { name:'Mặt cắt 1-1', c:[96000 + 6000, 5200], h:18000 }, { name:'Phòng khách', c:[3900, 2400], h:6200 } ];
    doc.pageSetups = [
      { name:'A1-PDF', device:'PDF của TrionCAD', paper:'ISO A1 (841,00 × 594,00 mm)', area:'Layout', scale:'1:1', ctb:'TCVN.ctb', orient:'Ngang', where:'Tệp' },
      { name:'A3-Phong-KT', device:'Canon iR-ADV C3530 (Phòng KT)', paper:'ISO A3 (420 × 297 mm)', area:'Layout', scale:'Vừa khổ giấy', ctb:'TCVN.ctb', orient:'Ngang', where:'\\\\SRV-01\\Canon-KT' }
    ];
    doc.xrefs = [ { name:'KC-luoi-truc', type:'Attach', status:'Đã nạp', path:'..\\KetCau\\KC-luoi-truc.dwg', found:'D:\\DuAn\\NhaO-2T\\KetCau\\KC-luoi-truc.dwg', size:'412 KB', date:'12/10/2026' },
      { name:'KT-nen-hien-trang', type:'Overlay', status:'Không tìm thấy', path:'\\\\SRV-01\\Chung\\Hien-trang.dwg', found:'', size:'', date:'' } ];
    doc.images = [ { name:'logo-cong-ty.png', status:'Không tìm thấy', path:'D:\\Tai-nguyen\\logo-cong-ty.png' } ];
    doc.initView = { c:[8400, 2300], h:16000 };
    return finish(doc);
  }
  function buildDoc2(){
    var doc = makeDoc({ id:'d2', name:'Ketcau-T2.dwg', path:'D:\\DuAn\\NhaO-2T\\KetCau\\Ketcau-T2.dwg', bytes:1.9 * 1048576, savedBy:'ZWCAD 2025', modified:'15/10/2026 09:12', ver:'DWG 2013' });
    defBlocks(doc);
    var b = new B(doc), E = function(o){ return b.add(o); };
    AX.forEach(function(x){ E({ t:'line', p1:[x, -2600], p2:[x, 9800], layer:'KT-TRUC' }); });
    AY.forEach(function(y){ E({ t:'line', p1:[-2600, y], p2:[12500, y], layer:'KT-TRUC' }); });
    AX.forEach(function(x){ AY.forEach(function(y){ E({ t:'hatch', loops:[rp(x - 110, y - 110, x + 110, y + 110)], pat:'SOLID', layer:'KC-COT', color:8 }); E({ t:'pline', pts:rp(x - 110, y - 110, x + 110, y + 110), closed:true, layer:'KC-COT' }); }); });
    AY.forEach(function(y){ for (var k = 0; k < 3; k++) { E({ t:'line', p1:[AX[k] + 110, y - 110], p2:[AX[k + 1] - 110, y - 110], layer:'KC-DAM' }); E({ t:'line', p1:[AX[k] + 110, y + 110], p2:[AX[k + 1] - 110, y + 110], layer:'KC-DAM' }); E({ t:'text', p:[(AX[k] + AX[k + 1]) / 2, y + 260], h:200, s:'D' + (k + 1) + ' 220×400', style:'Standard', al:'m', layer:'KC-TEXT' }); } });
    AX.forEach(function(x){ for (var k = 0; k < 2; k++) { E({ t:'line', p1:[x - 110, AY[k] + 110], p2:[x - 110, AY[k + 1] - 110], layer:'KC-DAM' }); E({ t:'line', p1:[x + 110, AY[k] + 110], p2:[x + 110, AY[k + 1] - 110], layer:'KC-DAM' }); } });
    [[1950,2400],[5850,2400],[9750,2400],[1950,6900],[9750,6900]].forEach(function(p){ E({ t:'line', p1:[p[0] - 1500, p[1]], p2:[p[0] + 1500, p[1]], layer:'KC-THEP' }); E({ t:'text', p:[p[0], p[1] + 160], h:180, s:'Ø8a200', style:'Standard', al:'m', layer:'KC-TEXT' }); });
    ['1','2','3','4'].forEach(function(n, k){ E({ t:'circle', c:[AX[k], -3000], r:400, layer:'KT-TRUC' }); E({ t:'text', p:[AX[k], -3000], h:340, s:n, style:'Standard', al:'m', layer:'KT-TRUC', color:7 }); });
    ['A','B','C'].forEach(function(n, k){ E({ t:'circle', c:[-3000, AY[k]], r:400, layer:'KT-TRUC' }); E({ t:'text', p:[-3000, AY[k]], h:340, s:n, style:'Standard', al:'m', layer:'KT-TRUC', color:7 }); });
    E({ t:'text', p:[5850, -4400], h:480, s:'MẶT BẰNG KẾT CẤU SÀN TẦNG 2', style:'Standard', al:'m', layer:'KC-TEXT' });
    E({ t:'text', p:[5850, -5150], h:280, s:'TL 1:100 · Bê tông B20, thép CB300-V', style:'Standard', al:'m', layer:'KC-TEXT' });
    E({ t:'dim', p1:[0, -500], p2:[11700, -500], pos:-1500, dir:'h', style:'TCVN-1-100', layer:'KC-DIM' });
    E({ t:'dim', p1:[-500, 0], p2:[-500, 9000], pos:-1500, dir:'v', style:'TCVN-1-100', layer:'KC-DIM' });
    doc.layouts = [{ name:'A3-KC-02', paper:{ w:420, h:297, name:'ISO A3 (420,00 × 297,00 mm)' }, margin:[5,5,5,5], ents:[], vports:[{ id:'vk1', x:30, y:50, w:380, h:240, c:[5850, 2000], scale:100, locked:true }], setup:'A3' }];
    new B(doc).add({ t:'insert', name:'KHUNG-A3', p:[0,0], sx:1, sy:1, rot:0, layer:'KT-KHUNG', attrs:{ TENBV:'MẶT BẰNG KẾT CẤU SÀN TẦNG 2', KYHIEU:'KC-02', TILE:'1:100' } }, doc.layouts[0].ents);
    doc.views = []; doc.pageSetups = [{ name:'A3', device:'PDF của TrionCAD', paper:'ISO A3 (420 × 297 mm)', area:'Layout', scale:'1:1', ctb:'monochrome.ctb', orient:'Ngang', where:'Tệp' }];
    doc.initView = { c:[4600, 2000], h:16000 };
    doc.styles.forEach(function(s){ s.missing = false; });
    return finish(doc);
  }
  // đếm đối tượng theo layer, kiểu chữ, bảng mã
  function finish(doc){
    var cnt = {}; doc.layers.forEach(function(l){ cnt[l.name] = 0; });
    function walk(list){ list.forEach(function(e){ cnt[e.layer] = (cnt[e.layer] || 0) + 1; }); }
    walk(doc.ents); doc.layouts.forEach(function(l){ walk(l.ents); });
    doc.layers.forEach(function(l){ l.count = cnt[l.name] || 0; });
    var st = {}; doc.styles.forEach(function(s){ st[s.name] = 0; });
    function walkText(list, mul){ list.forEach(function(e){
      if (e.t === 'text' || e.t === 'attdef') st[e.style] = (st[e.style] || 0) + (mul || 1);
      if (e.t === 'insert') { var bd = doc.blocks[e.name]; if (bd) walkText(bd.ents.filter(function(x){ return x.t === 'text' || x.t === 'attdef'; }), 1); }
    }); }
    walkText(doc.ents); doc.layouts.forEach(function(l){ walkText(l.ents); });
    doc.styles.forEach(function(s){ s.count = st[s.name] || 0; });
    return doc;
  }

  TC.buildDocs = function(){ return [buildDoc1(), buildDoc2()]; };
  // tệp gần đây cho tab Start (mẫu)
  TC.RECENT = [
    { name:'Banve-01.dwg', path:'D:\\DuAn\\NhaO-2T\\KienTruc', date:'16/10/2026', size:'4,8 MB', ver:'DWG 2018', doc:'d1', pinned:true },
    { name:'Ketcau-T2.dwg', path:'D:\\DuAn\\NhaO-2T\\KetCau', date:'15/10/2026', size:'1,9 MB', ver:'DWG 2013', doc:'d2', pinned:false },
    { name:'MB-Truong-THCS-An-Phu.dwg', path:'\\\\SRV-01\\DuAn\\2026\\THCS-AnPhu', date:'12/10/2026', size:'38,2 MB', ver:'DWG 2018', big:true },
    { name:'Dien-nuoc-T1-T3.dwg', path:'D:\\DuAn\\Biet-thu-Thao-Dien\\ME', date:'08/10/2026', size:'6,4 MB', ver:'DWG 2010' },
    { name:'Hien-trang-khu-dat.dwg', path:'D:\\Khao-sat\\Lo-B7', date:'02/10/2026', size:'12,1 MB', ver:'DWG 2004', broken:true },
    { name:'Mau-khung-ten-cong-ty.dwt', path:'D:\\Tai-nguyen\\Mau', date:'28/09/2026', size:'0,3 MB', ver:'DWG 2018' },
    { name:'Chi-tiet-cau-thang.dwg', path:'D:\\DuAn\\NhaO-2T\\KienTruc', date:'25/09/2026', size:'2,2 MB', ver:'DWG 2018' },
    { name:'Thong-ke-thep-mong.dxf', path:'D:\\DuAn\\NhaO-2T\\KetCau', date:'20/09/2026', size:'0,9 MB', ver:'DXF 2013' }
  ];
})();
