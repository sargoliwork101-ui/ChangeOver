# -*- coding: utf-8 -*-
"""Generate Documentation/<module>_process.html pages (dynamic flowchart +
real code tree) for every module and the whole system.

Rule: AI_AGENT_RULES.md «صفحهٔ فرآیند HTML هر ماژول و کل سیستم».
The pages are DYNAMIC: the flowchart boxes are sized from their text and
the code tree is drawn as a real tree (nodes/branches) that expands and
collapses on click. Edit the DATA below (read from the real code), then:
    python3 tools/make_process_pages.py
Never edit the generated HTML by hand.
"""
import json, os, re


# ==================== Top menu — shared by every page ====================
# [EN] One sticky, categorised menu on every generated page: the reader does not
#      have to hunt through a flat list of seventeen links. Groups are fixed here
#      so both generators (process pages, alarm pages, simulator) stay in sync.
# [FA] یک منوی چسبیده و دسته‌بندی‌شده در بالای همهٔ صفحه‌ها: خواننده لازم نیست
#      دنبال یک ردیف هفده‌تایی از لینک بگردد. دسته‌ها اینجا یک‌جا تعریف شده‌اند
#      تا هر سه سازنده (صفحه‌های ماژول، صفحه‌های خطا، شبیه‌ساز) یکی بمانند.
ALARM_MENU = [
    ('fault_7_technical', 'خطای ۷ — خطای فنی شارژر'),
    ('fault_6_battery_lost', 'خطای ۶ — قطع باتری'),
    ('fault_0_adc', 'خطای ۰ — مبدل اندازه‌گیری'),
    ('input_overvoltage', 'اضافه‌ولتاژ ورودی'),
    ('dead_battery', 'باتری خراب'),
    ('imbalance_latched', 'عدم‌توازن قفل‌شده'),
    ('selftest', 'خودآزمایی روشن‌شدن'),
    ('charge_full', 'شارژ کامل'),
    ('charging', 'در حال شارژ'),
    ('thermal_rest', 'استراحت حرارتی'),
    ('input_ok', 'ورودی سالم بدون شارژ'),
    ('battery_run', 'کار روی باتری'),
    ('reserved_bits', 'بیت‌های رزرو خطا'),
]

MENU_GROUPS = [
    ('شروع و نمای کلی', [
        ('کل سیستم — از روشن‌شدن تا آخر', 'system'),
        ('معماری کارها (چه کسی چه کاری می‌کند)', 'rtosmap'),
        ('کارها (تسک‌ها) — اولویت و دوره', 'rtos'),
    ]),
    ('زنجیرهٔ کنترل', [
        ('اندازه‌گیری', 'measurement'),
        ('خطاها (قطع باتری)', 'fault'),
        ('جیتر — قطع ایمنی سخت‌افزاری', 'jitter'),
        ('شارژر', 'charging'),
        ('عدم‌توازن باتری', 'imbalance'),
        ('تعویض خروجی', 'changeover'),
        ('تغذیهٔ میکرو', 'mcupowerpath'),
        ('ثبت سابقه', 'diag'),
    ]),
    ('خطاها و آلارم‌ها', 'alarms'),
    ('چراغ و بوق', [
        ('فلوچارت چراغ و بوق', 'ui'),
        ('شبیه‌ساز چراغ‌ها و بوق', 'sim'),
    ]),
    ('ارتباط و تنظیم', [
        ('ارتباط با پنل', 'esplink'),
        ('جدول تصحیح جریان', 'callut'),
        ('ناظر حفاظت', 'protection'),
        ('قفل خواندن برنامه', 'flashlock'),
    ]),
]


def menu_target_path(target):
    if target == 'alarms':
        return os.path.join(ROOT, 'Documentation', 'alarms', 'index.html')
    if target == 'sim':
        return os.path.join(ROOT, 'Firmware', 'Modules', 'Ui', 'alarm_simulator.html')
    if target.startswith('alarm:'):
        return os.path.join(ROOT, 'Documentation', 'alarms', target.split(':', 1)[1] + '.html')
    return target_path(target)


def build_menu(from_dir, current=None, alarms=None):
    """Return the menu groups with hrefs resolved from one page folder.

    [FA] دسته‌ها را برای یک پوشهٔ مشخص آماده می‌کند: مسیر هر لینک نسبت به همان
    صفحه حساب می‌شود و خانهٔ همین صفحه نشان‌دار می‌شود.
    """
    groups = []
    for label, items in MENU_GROUPS:
        if items == 'alarms':
            items = [('فهرست کامل خطاها و آلارم‌ها', 'alarms')]
            for slug, title in (alarms or ALARM_MENU):
                items.append((title, 'alarm:' + slug))
            wide = True
        else:
            wide = False
        out = []
        group_on = False
        for text, target in items:
            path = menu_target_path(target)
            on = (current is not None and target == current)
            if on:
                group_on = True
            out.append({'t': text,
                        'href': os.path.relpath(path, from_dir).replace(os.sep, '/'),
                        'on': on})
        groups.append({'t': label, 'items': out, 'wide': wide, 'on': group_on})
    return groups


MENU_CSS = r"""
  .topnav { position:sticky; top:0; z-index:60; background:#0E1218; border:1.5px solid #35507A;
            border-radius:12px; margin:0 0 16px; padding:7px 10px; box-shadow:0 4px 14px rgba(0,0,0,.35); }
  .tnwrap { display:flex; flex-wrap:wrap; gap:6px; align-items:center; }
  .tbrand { font-size:13px; color:#9AA7BC; padding:0 6px 0 12px; margin-left:4px; border-left:1px solid #33415C; }
  .tgrp { position:relative; }
  .tbtn { font-family:inherit; font-size:14.5px; color:#E6EAF2; background:#1B2029;
          border:1.5px solid #35507A; border-radius:9px; padding:5px 12px; cursor:pointer; }
  .tbtn:hover { border-color:#7FB0E6; }
  .tgrp.active > .tbtn { background:#24466B; border-color:#7FB0E6; }
  .tgrp.open > .tbtn { background:#24466B; border-color:#9CC6F5; }
  .tdrop { display:none; position:absolute; top:calc(100% + 6px); right:0; min-width:260px;
           background:#151A22; border:1.5px solid #35507A; border-radius:10px; padding:6px;
           box-shadow:0 10px 26px rgba(0,0,0,.55); max-height:70vh; overflow:auto; }
  .tgrp.open > .tdrop { display:block; }
  .tdrop.wide { min-width:540px; column-count:2; column-gap:6px; }
  .tdrop a { display:block; padding:6px 10px; border-radius:7px; color:#D8E1EE;
             text-decoration:none; font-size:14.5px; break-inside:avoid; }
  .tdrop a:hover { background:#24466B; color:#fff; }
  .tdrop a.on { background:#2E4E77; color:#fff; font-weight:700; }
  @media (max-width:760px){ .tdrop.wide { min-width:min(92vw,420px); column-count:1; } }
"""

MENU_JS = r"""
function renderMenu(groups, mountId){
  var host = document.getElementById(mountId);
  if (!host || !groups) return;
  var wrap = document.createElement('div'); wrap.className = 'tnwrap';
  var brand = document.createElement('span'); brand.className = 'tbrand';
  brand.textContent = 'صفحه‌های دستگاه'; wrap.appendChild(brand);
  function closeAll(){
    var open = host.querySelectorAll('.tgrp.open');
    for (var i = 0; i < open.length; i++){
      open[i].classList.remove('open');
      var bb = open[i].querySelector('.tbtn'); if (bb) bb.setAttribute('aria-expanded', 'false');
    }
  }
  groups.forEach(function(g){
    var box = document.createElement('div');
    box.className = 'tgrp' + (g.on ? ' active' : '');
    var btn = document.createElement('button');
    btn.type = 'button'; btn.className = 'tbtn';
    btn.textContent = g.t + ' ▾'; btn.setAttribute('aria-expanded', 'false');
    var drop = document.createElement('div');
    drop.className = 'tdrop' + (g.wide ? ' wide' : '');
    g.items.forEach(function(it){
      var a = document.createElement('a');
      a.href = it.href; a.textContent = it.t;
      if (it.on){ a.className = 'on'; a.setAttribute('aria-current', 'page'); }
      drop.appendChild(a);
    });
    btn.addEventListener('click', function(ev){
      ev.stopPropagation();
      var wasOpen = box.classList.contains('open');
      closeAll();
      if (!wasOpen){ box.classList.add('open'); btn.setAttribute('aria-expanded', 'true'); }
    });
    box.appendChild(btn); box.appendChild(drop); wrap.appendChild(box);
  });
  host.appendChild(wrap);
  document.addEventListener('click', closeAll);
  document.addEventListener('keydown', function(ev){ if (ev.key === 'Escape') closeAll(); });
}
"""

TEMPLATE = r"""<!DOCTYPE html>
<html lang="fa" dir="rtl">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>__TITLE__</title>
<style>
  body { font-family: Vazirmatn, Tahoma, sans-serif; background:#12151B; color:#E6EAF2; margin:0; padding:24px 14px 48px; line-height:1.9; }
  .wrap { max-width: none; margin: 0; }
  h1 { font-size: 26px; margin: 6px 0 2px; }
  h2 { font-size: 20px; margin: 30px 0 8px; border-bottom: 3px solid #35507A; padding-bottom: 4px; }
  a { color:#7FB0E6; }
  .card { background:#1B2029; border:1.5px solid #35507A; border-radius:12px; padding:14px 20px; margin:12px 0; }
  .ok { background:#20301F; border-color:#548235; }
  ol, ul { margin: 6px 0; padding-right: 22px; }
  li { margin: 4px 0; }
  b.num { color:#FF8080; }
  table { border-collapse: collapse; width:100%; background:#1B2029; margin:10px 0; }
  th { background:#24466B; color:#fff; padding:8px 10px; font-size:15px; }
  td { border:1px solid #33415C; padding:7px 10px; font-size:15px; }
  tr:nth-child(even) td { background:#171C26; }
  .small { font-size: 13.5px; color:#9AA7BC; }
  svg text { font-family: Vazirmatn, Tahoma, sans-serif; }
  .tnode { cursor: pointer; }
  .edge { cursor: pointer; }
  .edge .hit { stroke: rgba(0,0,0,0); stroke-width: 12; fill: none; }
.edge .rhit { stroke-width: 10; }
  .fnode { cursor: pointer; }
  svg.sel .edge, svg.sel .fnode { opacity: .12; }
  svg.sel .edge.hl, svg.sel .fnode.hl { opacity: 1; }
  .snode { cursor: pointer; }
  svg.sel .snode { opacity: .12; }
  svg.sel .snode.hl { opacity: 1; }
  .snode.hl rect { stroke-width: 4; }
  .edge.hl .vis { stroke-width: 4.5; }
  .fnode.hl rect, .fnode.hl polygon { stroke-width: 4; }
  .fnode a text { text-decoration: underline; }
  .fnode a:hover text { fill: #B45309; }
  .errl tspan { fill: #8AB8FF; text-decoration: underline; }
  .errl:hover tspan { fill: #FFB86B; }
  a.errl { color: #8AB8FF; text-decoration: underline; }
  #einfo { background:#16283F; border:1px solid #35507A; border-radius:8px; padding:6px 12px; margin:4px 4px 8px; font-size:14.5px; }
  .subhead { cursor: pointer; }
  .subhead:hover rect { stroke-width: 3; }
  .tnode:hover rect { stroke-width: 3; }
/*__MENUCSS__*/
</style>
</head>
<body>
<div class="wrap">
<nav class="topnav" id="menu" aria-label="منوی صفحه‌های دستگاه"></nav>
<h1 id="ttl"></h1>
<p class="small" id="sub"></p>
<h2>توضیح ساده</h2>
<div class="card"><ol id="steps"></ol></div>
<div id="exwrap"><h2>یک مثال عددی</h2><div class="card ok"><ol id="example"></ol></div></div>
<div id="glosswrap"><h2>واژه‌نامه — این کلمه‌ها یعنی چه؟</h2>
<div class="card"><p class="small">هر کلمهٔ فنی که در این صفحه به‌کار رفته، این‌جا به زبان ساده معنی شده است. اگر جایی باز هم گنگ بود، بگو تا همان جمله را هم ساده‌تر کنم.</p>
<table id="gloss"><tr><th style="width:22%">کلمه</th><th>یعنی چه (به زبان ساده)</th></tr></table></div></div>
<h2>فلوچارت</h2>
<div class="card"><div id="flow"></div><p class="small" id="legend"></p></div>
<h2>درخت کد</h2>
<p class="small">درخت واقعی کد است؛ روی هر گره کلیک کن تا شاخه‌هایش باز یا بسته شوند.</p>
<div class="card"><div id="tree"></div></div>
<h2 id="condsh">شرط‌ها و حفاظت‌ها</h2>
<table id="conds"><tr><th style="width:26%" id="condh1">اگر این اتفاق افتاد</th><th style="width:37%" id="condh2">برنامه چه کار می‌کند</th><th id="condh3">عددش</th></tr></table>
<h2 id="outsh">فرمان‌ها و خروجی‌ها</h2>
<table id="outs"><tr><th style="width:26%" id="outh1">به کجا</th><th style="width:37%" id="outh2">چه فرمانی / چه خروجی‌ای</th><th id="outh3">کی و با چه شرطی</th></tr></table>
<div class="card" style="background:#2E2A18;border-color:#BF8F00"><b>مرحلهٔ بعدی:</b> این صفحه را با هم مرور می‌کنیم. هر جا عدد یا شرطی ایراد داشت، بگو کدام است؛ اول از روی کد بررسی می‌کنیم، بعد اصلاح می‌کنیم.</div>
</div>
<script>/*__MENUJS__*/</script>
<script>const DATA = /*__DATA__*/;</script>
<script>
var COLORS = {
  blue:  {f:'#DDEBF7', o:'#1F4E79', t:'#12263F'},
  green: {f:'#C6EFCE', o:'#548235', t:'#12263F'},
  yellow:{f:'#FFE699', o:'#BF8F00', t:'#1F3864'},
  red:   {f:'#FFC7CE', o:'#C00000', t:'#6E0000'},
  gray:  {f:'#EDEDED', o:'#808080', t:'#2B2B2B'},
  navy:  {f:'#1F3864', o:'#1F3864', t:'#FFFFFF'}
};
var LINES = { blue:'#6FA8DC', green:'#7BC47F', yellow:'#E3B341', red:'#FF8080', gray:'#9AA7BC', navy:'#6FA8DC' };
var LEGEND = [['blue','در حال انجام کار'],['green','انجام شد / سالم'],['yellow','سؤال و تصمیم'],['red','خطا و توقف'],['gray','صبر و مکث'],['navy','شروع و پایان']];
var cv = document.createElement('canvas').getContext('2d');
function tw(t, bold, sz){ cv.font = (bold ? '700 ' : '') + sz + 'px Vazirmatn, Tahoma'; return cv.measureText(t).width; }
function esc(s){ return s.replace(/&/g,'&amp;').replace(/</g,'&lt;'); }
function etxt(s){ var e = esc(s), m = (typeof DATA !== 'undefined' && DATA.errmap) || {}; if (!m['۶'] && !m['۷']) return e; return e.replace(/خطای ([۰-۹]+)/g, function(w, dg){ var h = m[dg]; return h ? '</tspan><a class="errl" href="' + h + '"><tspan>' + w + '</tspan></a><tspan>' : w; }); }
function linkErrs(el){ var m = (typeof DATA !== 'undefined' && DATA.errmap) || {}; if (!el || (!m['۶'] && !m['۷'])) return; var wk = document.createTreeWalker(el, NodeFilter.SHOW_TEXT, null), ns = [], cur; while ((cur = wk.nextNode())){ var pv = cur.parentNode; if (pv && !pv.closest('a') && /خطای [۰-۹]/.test(cur.nodeValue)) ns.push(cur); } ns.forEach(function(tn){ var fg = document.createDocumentFragment(); tn.nodeValue.split(/(خطای [۰-۹]+)/).forEach(function(pt){ if (!pt) return; var mm2 = /^خطای ([۰-۹]+)$/.exec(pt); if (mm2 && m[mm2[1]]){ var a = document.createElement('a'); a.className = 'errl'; a.href = m[mm2[1]]; a.textContent = pt; fg.appendChild(a); } else fg.appendChild(document.createTextNode(pt)); }); tn.parentNode.replaceChild(fg, tn); }); }
function linesOf(n){ var a = []; if (n.t) a.push({s:n.t, b:true}); (n.l || []).forEach(function(x){ a.push({s:x, b:false}); }); if (n.ref) a.push({s:n.ref, b:false, r:true}); return a; }
function wrapLine(t, bold, sz, maxW){
  if (tw(t, bold, sz) <= maxW) return [t];
  var words = t.split(' '), lines = [], cur = '';
  words.forEach(function(w){
    var test = cur ? cur + ' ' + w : w;
    if (tw(test, bold, sz) <= maxW || !cur){ cur = test; } else { lines.push(cur); cur = w; }
  });
  if (cur) lines.push(cur);
  return lines;
}
function wrappedLines(n){
  if (n._wl) return n._wl;
  var out = [];
  linesOf(n).forEach(function(x){ wrapLine(x.s, x.b, 15, (n.k === 'dia' ? 215 : 330)).forEach(function(part){ out.push({s: part, b: x.b, r: x.r}); }); });
  n._wl = out; return out;
}
function sideLines(sd){
  if (sd._wl) return sd._wl;
  var maxW = (sd.d === 'l') ? 200 : 240, out = [];
  sd.l.forEach(function(x){ wrapLine(x, false, 13.5, maxW).forEach(function(part){ out.push(part); }); });
  sd._wl = out; return out;
}
function nodeSize(n){
  if (n.k === 'sub') return { w: Math.min(Math.max(tw(n.t || '', true, 15) + 96, 280), 470), h: 46 };
  var ls = wrappedLines(n), mx = 0;
  ls.forEach(function(x){ mx = Math.max(mx, tw(x.s, x.b, 15)); });
  if (n.k === 'dia') return { w: Math.min(Math.max(mx / 0.60 + 44, 235), 540), h: Math.max(88, ls.length * 27 + 54) };
  return { w: Math.min(Math.max(mx + 48, 150), 400), h: ls.length * 25 + 30 };
}
function afterLines(s){
  if (!s.after) return [];
  if (s._al) return s._al;
  var maxW = (s.d === 'l') ? 200 : 240, out = [];
  wrapLine('در ادامه: ' + s.after, false, 11.5, maxW).forEach(function(part){ out.push(part); });
  s._al = out; return out;
}
function sideSize(s){
  var ls = sideLines(s), mx = 0; ls.forEach(function(x){ mx = Math.max(mx, tw(x, false, 13.5)); });
  var al = afterLines(s);
  al.forEach(function(x){ mx = Math.max(mx, tw(x, false, 11.5)); });
  return { w: mx + 38, h: ls.length * 23 + 26 + (al.length ? al.length * 17 + 6 : 0) };
}
function drawSub(n, cx, y, idx){
  var sz = nodeSize(n), x = cx - sz.w / 2, out = '';
  out += '<g class="subhead" data-i="' + idx + '"><rect x="' + x + '" y="' + y + '" width="' + sz.w + '" height="46" rx="10" fill="#EDEDED" stroke="#808080" stroke-width="2"/>' +
    '<text x="' + cx + '" y="' + (y + 29) + '" text-anchor="middle" font-size="15" font-weight="700" fill="#1F3864">' + esc(n.t) + (n._open ? '  ‹ بستن' : '  › بازکردن جزئیات') + '</text></g>';

  if (!n._open) return out;
  var kids = n.flow || [], ky = y + 78, maxW = 200, sizes = [];
  kids.forEach(function(kd){ var ks = nodeSize(kd); sizes.push(ks); maxW = Math.max(maxW, ks.w); });
  var frameW = Math.min(maxW + 44, 560), frameY1 = ky;
  sizes.forEach(function(ks){ frameY1 += ks.h + 30; });
  frameY1 -= 18;
  out += '<rect x="' + (cx - frameW / 2) + '" y="' + (y + 56) + '" width="' + frameW + '" height="' + (frameY1 - y - 56) + '" rx="12" fill="#F8FBFF" stroke="#9DB8D9" stroke-width="1.8" stroke-dasharray="7 5"/>';
  kids.forEach(function(kd, ki){
    if (ki > 0) out += '<path d="M' + cx + ' ' + (ky - 30) + ' V' + (ky - 4) + '" stroke="#1F4E79" stroke-width="2" fill="none" marker-end="url(#arw)"/>';
    out += drawNode(kd, cx, ky);
    ky += sizes[ki].h + 30;
  });
  return out;
}
function drawNode(n, cx, y, idx){
  var sz = nodeSize(n); DRAWN[idx] = sz; c = COLORS[n.c || 'blue'], x = cx - sz.w / 2, out = '<g class="fnode' + (n.go ? ' go' : '') + '" data-i="' + idx + '" data-t="' + esc(n.t || '') + '" data-go="' + esc(n.go || '') + '">';
  if (n.k === 'pill'){
    out += '<rect x="' + x + '" y="' + y + '" width="' + sz.w + '" height="' + sz.h + '" rx="' + (sz.h/2) + '" fill="' + c.f + '" stroke="' + c.o + '" stroke-width="2"/>';
  } else if (n.k === 'dia'){
    var cy = y + sz.h / 2;
    out += '<polygon points="' + cx + ',' + y + ' ' + (x + sz.w) + ',' + cy + ' ' + cx + ',' + (y + sz.h) + ' ' + x + ',' + cy + '" fill="' + c.f + '" stroke="' + c.o + '" stroke-width="2"/>';
  } else {
    out += '<rect x="' + x + '" y="' + y + '" width="' + sz.w + '" height="' + sz.h + '" rx="10" fill="' + c.f + '" stroke="' + c.o + '" stroke-width="2"/>';
  }
  var ls = wrappedLines(n), ty = y + sz.h / 2 - (ls.length - 1) * 12.5, tt = '';
  ls.forEach(function(ln, i){
    if (ln.r){
      var rt = '<text x="' + cx + '" y="' + (ty + i * 25 + 5) + '" text-anchor="middle" font-size="12" font-family="Consolas, monospace" fill="' + (c.t === '#FFFFFF' ? '#B9CDE4' : '#33506F') + '">' + esc(ln.s) + '</text>';
      tt += n.rurl ? '<a href="' + n.rurl + '" target="_blank">' + rt + '</a>' : rt;
    } else {
      tt += '<text x="' + cx + '" y="' + (ty + i * 25 + 5) + '" text-anchor="middle" font-size="15" ' + (ln.b ? 'font-weight="700"' : '') + ' fill="' + c.t + '">' + '<tspan>' + etxt(ln.s) + '</tspan>' + '</text>';
    }
  });
  var aw = (n.k === 'dia') ? sz.w * 0.60 : sz.w - 14;
  out += '<g class="fx" data-w="' + aw + '" data-cx="' + cx + '" data-cy="' + (y + sz.h / 2) + '">' + tt + '</g>';
  if (n.go) out += '<text x="' + (x + sz.w - 11) + '" y="' + (y + 18) + '" font-size="14" fill="#B45309">↗</text>';
  return out + '</g>';
}
var SIDEGEO = {};
var SIDECOLX = 0;
var DRAWN = {};
function drawSide(s, nodeCx, nodeW, cy, dir, oIdx, oTitle, kind){
  var pre = SIDEGEO[oIdx + ':' + kind];
  var sz = sideSize(s), c = COLORS[s.c || 'gray'];
  var x = pre ? pre.x : ((dir === 'r') ? (SIDECOLX || (1390 - sz.w)) : 10);
  var y = pre ? pre.y : cy - sz.h / 2, out = '';
  SIDEGEO[oIdx + ':' + kind] = { x: x, y: y, w: sz.w, h: sz.h, cy: cy, dir: dir };
  var x1 = (dir === 'r') ? nodeCx + nodeW / 2 : nodeCx - nodeW / 2;
  var x2 = (dir === 'r') ? x : x + sz.w;
  out += '<g class="edge" data-sk="' + kind + '" data-fi="' + oIdx + '" data-ti="" data-f="' + esc(oTitle || '') + '" data-t="' + esc(s.l[0] || '') + '" data-l="' + esc(s.lab || (s.d === 'l' ? 'بررسی جدا' : '')) + '">';
  out += '<path class="hit" d="M' + x1 + ' ' + cy + ' H' + x2 + '"/>';
  out += '<path class="vis" d="M' + x1 + ' ' + cy + ' H' + x2 + '" stroke="' + (LINES[s.c || 'gray']) + '" stroke-width="2" stroke-dasharray="6 4" fill="none" marker-end="url(#arw)"/>';
  var slab = s.lab || (s.d === 'l' ? 'بررسی جدا' : null);
  if (slab){
    var mxp = (x1 + x2) / 2, lw = tw(slab, true, 13.5);
    out += '<rect x="' + (mxp - lw / 2 - 5) + '" y="' + (cy - 9.5) + '" width="' + (lw + 10) + '" height="19" fill="#1B2029"/>';
    out += '<text x="' + mxp + '" y="' + (cy + 4.5) + '" text-anchor="middle" font-size="13.5" font-weight="700" fill="' + (LINES[s.c || 'gray']) + '">' + esc(slab) + '</text>';
  }
  out += '</g><g class="snode" data-i="' + oIdx + '" data-k="' + kind + '">';
  out += '<rect x="' + x + '" y="' + y + '" width="' + sz.w + '" height="' + sz.h + '" rx="9" fill="' + c.f + '" stroke="' + c.o + '" stroke-width="1.6"/>';
  var sl = sideLines(s), st = '';
  sl.forEach(function(ln, i){
    st += '<text x="' + (x + sz.w / 2) + '" y="' + (y + sz.h / 2 - (sl.length - 1) * 11.5 + i * 23 + 5) + '" text-anchor="middle" font-size="13.5" fill="' + c.t + '">' + '<tspan>' + etxt(ln) + '</tspan>' + '</text>';
  });
  var al = afterLines(s);
  al.forEach(function(ln, i){
    st += '<text x="' + (x + sz.w / 2) + '" y="' + (y + sz.h - al.length * 17 - 2 + i * 17 + 4) + '" text-anchor="middle" font-size="11.5" fill="#33506F">' + '<tspan>' + etxt(ln) + '</tspan>' + '</text>';
  });
  out += '<g class="fx" data-w="' + (sz.w - 12) + '" data-cx="' + (x + sz.w / 2) + '" data-cy="' + (y + sz.h / 2) + '">' + st + '</g>';
  return out + '</g>';
}
function vlabel(x, y, s, col){
  var w = tw(s, true, 13.5) + 16;
  return '<g transform="translate(' + x + ',' + y + ') rotate(-90)"><rect x="' + (-w/2) + '" y="-11" width="' + w + '" height="22" fill="#1B2029"/><text x="0" y="4.5" text-anchor="middle" font-size="13.5" font-weight="700" fill="' + (col || '#D7E3F5') + '">' + esc(s) + '</text></g>';
}
function renderFlow(){
  DRAWN = {};
  var W = 1400, cx = 640, GAP = 62, y = 26, pos = [], out = '';
  out += '<defs><marker id="arw" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6.5" markerHeight="6.5" orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 z" fill="#6FA8DC"/></marker></defs>';
  var loops = [];
  DATA.flow.forEach(function(n, i){
    if (n.k === 'sec'){
      y += 18;
      out += '<text x="' + (W - 15) + '" y="' + (y + 6) + '" direction="rtl" text-anchor="start" font-size="16" font-weight="700" fill="#E6EAF2">' + esc(n.t) + '</text>';
      out += '<line x1="15" y1="' + (y + 16) + '" x2="' + (W - 15) + '" y2="' + (y + 16) + '" stroke="#33415C" stroke-width="2"/>';
      y += 34; pos.push(null); return;
    }
    var sz = nodeSize(n);
    if (n.k === 'sub' && n._open){
      var kh = 78 - 46;
      (n.flow || []).forEach(function(kd){ kh += nodeSize(kd).h + 30; });
      sz = { w: sz.w, h: kh + 14 };
    }
    pos.push({ y: y, h: sz.h, w: sz.w, cy: y + (n.k === 'sub' ? 23 : sz.h / 2) });
    y += sz.h + GAP;
    if (n.loop) loops.push({ from: i, to: n.loop.to, lab: n.loop.lab, col: n.loop.col });
  });
  var firstIdx = -1;
  DATA.flow.forEach(function(n, i){ if (firstIdx < 0 && n.k !== 'sec' && pos[i]) firstIdx = i; });
  var retPillIdx = -1;
  DATA.flow.forEach(function(n, i){ if (n.k === 'pill' && (n.t || '').indexOf('برگشت') >= 0) retPillIdx = i; });
  var lastReal = -1; pos.forEach(function(p2, i2){ if (p2) lastReal = i2; });
  var needTerm = !(DATA.flow[lastReal] && DATA.flow[lastReal].k === 'pill');
  var reusePill = (!needTerm && retPillIdx === lastReal);
  var termPos = null;
  if (needTerm){ termPos = { y: y, h: 44, w: 320, cy: y + 22, _term: true }; pos.push(termPos); y += 44 + GAP; }
  else if (reusePill){ termPos = pos[retPillIdx]; }
  var maxHalf0 = 0; pos.forEach(function(p2){ if (p2) maxHalf0 = Math.max(maxHalf0, p2.w / 2); });
  var nRet = 0; DATA.flow.forEach(function(n2){ if (n2.side && n2.side.ret) nRet++; });
  SIDECOLX = Math.round(cx + maxHalf0 + 26 + nRet * 15 + 16);
  SIDEGEO = {};
  DATA.flow.forEach(function(n, i){
    if (!pos[i] || n.k === 'sub') return;
    [['side', n.side, 'r'], ['note', n.note, 'l']].forEach(function(kv){
      var fld = kv[1]; if (!fld) return;
      var dir = fld.d || kv[2];
      var sz3 = sideSize(fld);
      SIDEGEO[i + ':' + kv[0]] = { x: (dir === 'r') ? SIDECOLX : 10, y: pos[i].cy - sz3.h / 2, w: sz3.w, h: sz3.h, cy: pos[i].cy, dir: dir };
    });
  });
  var H = y - GAP + 30;
  if (termPos){
    /* [EN] A page whose own "go back" pill IS this terminator: open the
       clickable wrapper BEFORE the pill is drawn, close it below.
       [FA] صفحه‌ای که خودش کادر «برگشت» را دارد: پوشش کلیک‌پذیر پیش از
       کشیدن کادر باز و پایین بسته می‌شود. */
    var termLabel = '↺ پایان این دور — دور بعد از شروع';
    out += '<g class="termwrap" data-t="' + esc(reusePill ? (DATA.flow[retPillIdx].t || termLabel) : termLabel) + '">';
    if (reusePill){ out += drawNode(DATA.flow[retPillIdx], cx, termPos.y, retPillIdx); }
    else { out += drawNode({ k: 'pill', c: 'navy', t: termLabel }, cx, termPos.y, DATA.flow.length); }
    /* [EN] draw the missing half of the loop: a dashed line from this pill back
       up the left side into the start node, otherwise the return tracks look
       like they end in nothing (user note 2026-10-20).
       [FA] نیمهٔ گمشدهٔ حلقه: خط‌چین از این جعبه تا گرهٔ شروع، در غیر این صورت
       خط‌های برگشت به‌نظر به هیچ‌جا نمی‌رسند. */
    if (firstIdx >= 0 && pos[firstIdx]){
      var st0 = pos[firstIdx], ts = DRAWN[DATA.flow.length] || { w: 320, h: 44 };
      var lx = cx - maxHalf0 - 44;
      var dLoop = 'M' + (cx - ts.w / 2) + ' ' + (termPos.y + ts.h / 2) + ' H' + lx + ' V' + st0.cy + ' H' + (cx - st0.w / 2 - 2);
      out += '<g class="edge" data-rf="side" data-ret="1" data-fi="" data-ti="' + firstIdx + '" data-f="پایان پاس" data-t="' + esc(DATA.flow[firstIdx].t || '') + '" data-l="دور بعد">';
      out += '<path class="hit rhit" d="' + dLoop + '"/>';
      out += '<path class="vis" d="' + dLoop + '" stroke="#7BC47F" stroke-width="2.4" stroke-dasharray="9 6" fill="none" marker-end="url(#arw)"/>';
      out += '</g>';
      out += vlabel(lx, (termPos.y + st0.cy) / 2, 'دور بعد', '#7BC47F');
    }
    out += '</g>';
  }
  function edgeX(n2, p2, dy, side){
    if (n2 && n2.k === 'dia'){ var f = 1 - Math.min(1, Math.abs(dy) / (p2.h / 2)); return cx + side * (p2.w / 2) * f; }
    return cx + side * p2.w / 2;
  }
  function stubY(g, lane){
    var lim = 1e9;
    Object.keys(SIDEGEO).forEach(function(k){ var q = SIDEGEO[k];
      if (q && q.dir === 'r' && q.y > g.y + g.h + 1 && q.x <= g.x + 20 && q.x >= lane - 1) lim = Math.min(lim, q.y); });
    var dy = Math.min(10, lim - (g.y + g.h) - 2);
    return g.y + g.h + Math.max(4, dy);
  }
  var laneBase = 1e9, nEnd = 0;
  DATA.flow.forEach(function(n, i){ var g0 = SIDEGEO[i + ':side']; if (n.side && n.side.ret && g0) laneBase = Math.min(laneBase, g0.x); if (n.side && n.side.ret === 'END') nEnd++; });
  var retK = 0, endK = 0;
  DATA.flow.forEach(function(n, i){
    if (!n.side || !n.side.ret) return;
    var g = SIDEGEO[i + ':side']; if (!g || g.dir !== 'r') return;
    var tgt = null, tTitle = '', tIdx = -1;
    if (n.side.ret === 'END'){
      tgt = termPos; tIdx = DATA.flow.length;
      tTitle = 'برگشت به اولِ دور بعد' +
        ((firstIdx >= 0) ? ' — ' + (DATA.flow[firstIdx].t || '') : '');
      if (!tgt && firstIdx >= 0){ tIdx = firstIdx; tgt = pos[firstIdx]; }
    }
    else if (n.side.ret === 'NODE'){ tIdx = n.side.retTo; tgt = pos[tIdx]; tTitle = (DATA.flow[tIdx] && DATA.flow[tIdx].t) || ''; }
    else { var j = i + 1; while (j < pos.length && !pos[j]) j++; tgt = pos[j]; tTitle = (DATA.flow[j] && DATA.flow[j].t) || ''; tIdx = j; }
    if (!tgt) return;
    var lane = laneBase - 16 - (retK++) * 15;
    var dd;
    if (n.side.ret === 'END'){
      var dz = DRAWN[tIdx] || { w: 216, h: 44 };
      var exStep = nEnd > 1 ? Math.min(22, (dz.w - 48) / (nEnd - 1)) : 0;
      var exX = cx - dz.w / 2 + 24 + endK * exStep;
      var gapTop = 0;
      pos.forEach(function(p3){ if (p3 && p3 !== tgt && p3.y + p3.h < tgt.y) gapTop = Math.max(gapTop, p3.y + p3.h); });
      var apTop = (gapTop ? gapTop + 6 : tgt.y - 16);
      var apStep = nEnd > 1 ? Math.max(5, Math.min(13, (tgt.y - 12 - apTop) / (nEnd - 1))) : 0;
      var apY = Math.max(apTop, tgt.y - 12 - endK * apStep); endK++;
      dd = 'M' + (g.x + 8) + ' ' + (g.y + g.h) + ' V' + stubY(g, lane) + ' H' + lane + ' V' + apY + ' H' + exX + ' V' + (tgt.y - 2);
    } else {
      var ty = tgt.cy - Math.min(18, tgt.h / 4);
      dd = 'M' + (g.x + 8) + ' ' + (g.y + g.h) + ' V' + stubY(g, lane) + ' H' + lane + ' V' + ty + ' H' + (edgeX(DATA.flow[tIdx], tgt, ty - tgt.cy, 1) - 2);
    }
    var linkTi = (n.side.ret === 'END' && firstIdx >= 0) ? firstIdx : tIdx;
    out += '<g class="edge" data-rf="side" data-fi="' + i + '" data-ti="' + linkTi + '"' +
           (n.side.ret === 'END' ? ' data-ret="1"' : '') +
           ' data-f="' + esc((n.side && n.side.l && n.side.l[0]) || n.t || '') + '" data-t="' + esc(tTitle) + '" data-l="برگشت">';
    out += '<path class="hit rhit" d="' + dd + '"/>';
    out += '<path class="vis" d="' + dd + '" stroke="' + (LINES[n.side.c || 'gray']) + '" stroke-width="1.8" fill="none" marker-end="url(#arw)"/>';
    out += '</g>';
  });
  var maxHalf = 0; pos.forEach(function(p){ if (p) maxHalf = Math.max(maxHalf, p.w / 2); });
  var loopEntrySeen = {};
  loops.forEach(function(L, li){
    var a = pos[L.from], b = pos[L.to], lane = cx - maxHalf - 24 - li * 30;
    var LOOPCOLS = ['#6FA8DC', '#7BC47F', '#B18CFF', '#E3B341', '#FF9E64'];
    var col = L.col === 'red' ? '#FF8080' : LOOPCOLS[li % LOOPCOLS.length];
    var nth = loopEntrySeen[L.to] || 0; loopEntrySeen[L.to] = nth + 1;
    var ldy = (nth - 1) * Math.min(18, b.h / 4);
    var dstr = 'M' + (cx - a.w / 2) + ' ' + a.cy + ' H' + lane + ' V' + (b.cy + ldy) + ' H' + (edgeX(DATA.flow[L.to], b, ldy, -1) + 2);
    out += '<g class="edge" data-fi="' + L.from + '" data-ti="' + L.to + '" data-f="' + esc(DATA.flow[L.from].t || '') + '" data-t="' + esc(DATA.flow[L.to].t || '') + '" data-l="' + esc(L.lab || '') + '">';
    out += '<path class="hit" d="' + dstr + '"/>';
    out += '<path class="vis" d="' + dstr + '" stroke="' + col + '" stroke-width="2.2" fill="none" marker-end="url(#arw)"/>';
    if (L.lab) out += vlabel(lane, (a.cy + b.cy) / 2, L.lab, col);
    out += '</g>';
  });
  DATA.flow.forEach(function(n, i){
    if (!pos[i]) return;
    var p = pos[i];
    var pj = i - 1; while (pj > 0 && !pos[pj]) pj--;
    var secBreak = false;
    for (var qb = i - 1; qb > pj; qb--){ if (DATA.flow[qb].k === 'sec' && DATA.flow[qb].brk) secBreak = true; }
    if (i > 0 && pos[pj] && !secBreak && !DATA.flow[pj].brk){
      var prevBottom = pos[pj].y + pos[pj].h;
      var pn = DATA.flow[pj];
      out += '<g class="edge" data-fi="' + pj + '" data-ti="' + i + '" data-f="' + esc(pn.t || '') + '" data-t="' + esc(n.t || '') + '" data-l="' + esc(pn.elab || '') + '">';
      out += '<path class="hit" d="M' + cx + ' ' + prevBottom + ' V' + p.y + '"/>';
      out += '<path class="vis" d="M' + cx + ' ' + prevBottom + ' V' + (p.y - 4) + '" stroke="#6FA8DC" stroke-width="2.2" fill="none" marker-end="url(#arw)"/>';
      if (pn.elab) out += vlabel(cx, (prevBottom + p.y) / 2, pn.elab, '#D7E3F5');
      out += '</g>';
    }
    if (n.k === 'sub'){ out += drawSub(n, cx, p.y, i); return; }
    if (!(reusePill && i === retPillIdx)){ out += drawNode(n, cx, p.y, i); }
    if (n.side) out += drawSide(n.side, cx, p.w, p.cy, n.side.d || 'r', i, n.t, 'side');
    if (n.note) out += drawSide(n.note, cx, p.w, p.cy, n.note.d || 'l', i, n.t, 'note');
  });
  document.getElementById('flow').innerHTML = '<div id="einfo">برای دیدن ارتباط هر خط، روی خودِ خط کلیک کن تا آن خط و دو کادرِ دو سرش روشن شوند. روی هر کادر هم کلیک کنی، همهٔ خط‌های وصل به آن روشن می‌شوند. کادرهایی که علامت ↗ دارند، لینک صفحهٔ همان ماژول‌اند. خط‌چین سبزِ «دور بعد» یعنی این کار در این دور تمام شده و دور بعد از همان کادرِ بالا از نو شروع می‌شود. برای برداشتن روشنایی، روی جای خالی شکل کلیک کن.</div><svg viewBox="0 0 ' + W + ' ' + H + '" width="66%" style="display:block;margin:0 auto">' + out + '</svg>';
  document.querySelectorAll('.subhead').forEach(function(g){
    g.addEventListener('click', function(){
      var n = DATA.flow[+g.getAttribute('data-i')];
      n._open = !n._open; renderFlow();
    });
  });
  document.getElementById('legend').innerHTML = 'معنای رنگ‌ها: ' + LEGEND.map(function(x){
    return '<span style="display:inline-block;width:13px;height:13px;background:' + COLORS[x[0]].f + ';border:2px solid ' + COLORS[x[0]].o + ';vertical-align:-2px;margin:0 4px 0 2px"></span>' + x[1];
  }).join(' &nbsp; ');
}
/* ---- real tree (root at right, grows left, click to collapse) ---- */
function treeMeasure(n, d){
  n._w = Math.min(tw(n.t, false, 13.5) + 26, 600); n._d = d;
  (n.c || []).forEach(function(ch){ treeMeasure(ch, d + 1); });
}
function treeLayout(n, cursor){
  var kids = (n._open && n.c) ? n.c : [];
  if (!kids.length){ n._y = cursor.v; cursor.v += 42; return; }
  kids.forEach(function(ch){ treeLayout(ch, cursor); });
  n._y = (kids[0]._y + kids[kids.length - 1]._y) / 2;
}
function treeDraw(n, levelX, out){
  var x = levelX[n._d] - n._w, y = n._y;
  (n._open ? (n.c || []) : []).forEach(function(ch){ treeDraw(ch, levelX, out); });
  (n._open ? (n.c || []) : []).forEach(function(ch){
    var cx2 = levelX[ch._d], cy2 = ch._y + 15, px = x, py = y + 15, mx = (px + cx2) / 2;
    out.push('<path d="M' + px + ' ' + py + ' H' + mx + ' V' + cy2 + ' H' + cx2 + '" stroke="#46608C" stroke-width="2" fill="none"/>');
  });
  var has = (n.c || []).length > 0;
  out.push('<g class="tnode" data-tid="' + n._id + '"><rect x="' + x + '" y="' + y + '" width="' + n._w + '" height="30" rx="8" fill="' + (n._d === 0 ? '#1F3864' : '#DDEBF7') + '" stroke="#6FA8DC" stroke-width="1.6"/>' +
    '<g class="fx" data-w="' + (n._w - 12) + '" data-cx="' + (x + n._w / 2) + '" data-cy="' + (y + 15) + '"><text x="' + (x + n._w / 2) + '" y="' + (y + 20) + '" text-anchor="middle" font-size="13.5" fill="' + (n._d === 0 ? '#FFFFFF' : '#12263F') + '">' + esc(n.t) + (has ? (n._open ? ' ‹' : ' ›') : '') + '</text></g></g>');
}
function fitText(){
  var gs = document.querySelectorAll('.fx');
  for (var i = 0; i < gs.length; i++){
    var g = gs[i]; if (!g.getBBox) continue;
    var bb = g.getBBox(), aw = parseFloat(g.getAttribute('data-w'));
    if (bb.width > aw && bb.width > 0){
      var sc = (aw / bb.width).toFixed(3), gx = g.getAttribute('data-cx'), gy = g.getAttribute('data-cy');
      g.setAttribute('transform', 'translate(' + gx + ' ' + gy + ') scale(' + sc + ') translate(' + (-gx) + ' ' + (-gy) + ')');
    }
  }
}
function renderTree(){
  var idc = { v: 0 };
  (function walk(n, d){ n._id = idc.v++; if (n._open === undefined) n._open = d < 2; (n.c || []).forEach(function(ch){ walk(ch, d + 1); }); })(DATA.tree, 0);
  treeMeasure(DATA.tree, 0);
  var maxD = 0, levelW = {};
  (function scan(n){ maxD = Math.max(maxD, n._d); levelW[n._d] = Math.max(levelW[n._d] || 0, n._w); (n.c || []).forEach(scan); })(DATA.tree);
  var levelX = {}, xr = 992;
  for (var d = 0; d <= maxD; d++){ levelX[d] = xr; xr -= (levelW[d] || 120) + 52; }
  var cursor = { v: 26 };
  treeLayout(DATA.tree, cursor);
  var out = [];
  treeDraw(DATA.tree, levelX, out);
  document.getElementById('tree').innerHTML = '<svg viewBox="0 0 1000 ' + (cursor.v + 16) + '" width="66%" style="display:block;margin:0 auto">' + out.join('') + '</svg>'; fitText();
  var nodes = document.querySelectorAll('.tnode');
  nodes.forEach(function(g){
    g.addEventListener('click', function(){
      var id = +g.getAttribute('data-tid'), found = null;
      (function find(n){ if (n._id === id) found = n; (n.c || []).forEach(find); })(DATA.tree);
      if (found && found.c && found.c.length){ found._open = !found._open; renderTree(); }
    });
  });
}
function firstNodeOf(svg){
  var ns = svg.querySelectorAll('.fnode');
  for (var i = 0; i < ns.length; i++){ if (ns[i].getAttribute('data-i') === '0' || ns[i].getAttribute('data-i') === '1') return ns[i]; }
  return ns[0] || null;
}
function wireFlow(){
  var fc = document.getElementById('flow');
  if (!fc || !fc.addEventListener || fc._ew) return;
  fc._ew = true;
  fc.addEventListener('click', function(ev){
    var svg = fc.querySelector('svg'); if (!svg) return;
    var info = document.getElementById('einfo');
    function clear(){
      svg.classList.remove('sel');
      var hs = svg.querySelectorAll('.hl');
      for (var q = 0; q < hs.length; q++) hs[q].classList.remove('hl');
    }
    if (ev.target.closest && ev.target.closest('a')) return;
    var tw = ev.target.closest ? ev.target.closest('.termwrap') : null;
    if (tw){
      clear();
      svg.classList.add('sel'); tw.classList.add('hl');
      var fn0 = firstNodeOf(svg), fIdx = fn0 ? fn0.getAttribute('data-i') : '';
      var rs = svg.querySelectorAll('.edge'), rc = 0, dest = '';
      for (var q8 = 0; q8 < rs.length; q8++){
        var e8 = rs[q8];
        var isBack = e8.getAttribute('data-ret') === '1' ||
                     (e8.getAttribute('data-ti') !== '' && e8.getAttribute('data-ti') === fIdx && e8.getAttribute('data-rf') === 'side');
        if (isBack){ e8.classList.add('hl'); rc++; }
      }
      if (fn0) fn0.classList.add('hl');
      var twName = (tw.getAttribute('data-t') || 'برگشت به دور بعد');
      if (info) info.textContent = rc
        ? '«' + twName + '»: ' + rc + ' خطِ برگشت روشن شد — کار از نو از («' + (fn0 ? fn0.getAttribute('data-t') : '') + '») شروع می‌شود.'
        : '«' + twName + '»: در این صفحه برگشت‌ها مستقیم به شروع دور وصل شده‌اند و خطشان آن بالا روشن می‌شود.';
      return;
    }
    var sg = ev.target.closest ? ev.target.closest('.snode') : null;
    var eg = ev.target.closest ? ev.target.closest('.edge') : null;
    var ng = ev.target.closest ? ev.target.closest('.fnode') : null;
    clear();
    if (sg){
      svg.classList.add('sel'); sg.classList.add('hl');
      var si = sg.getAttribute('data-i'), sk = sg.getAttribute('data-k'), ces = svg.querySelectorAll('.edge'), cc = 0;
      for (var q5 = 0; q5 < ces.length; q5++){ var ce = ces[q5]; if (ce.getAttribute('data-fi') === si && ((ce.getAttribute('data-ti') === '' && ce.getAttribute('data-sk') === sk) || ce.getAttribute('data-rf') === sk)){ ce.classList.add('hl'); cc++; } }
      if (info) info.textContent = 'box \u06a9\u0646\u0627\u0631\u06cc \u00ab' + ((sg.textContent || '').replace(/\s+/g, ' ').trim()) + '\u00bb \u2014 ' + cc + ' \u062e\u0637\u0650 \u062e\u0648\u062f\u0634 \u0631\u0648\u0634\u0646 \u0634\u062f. \u0628\u0631\u0627\u06cc \u0628\u0631\u062f\u0627\u0634\u062a\u0646 \u0631\u0648\u0634\u0646\u0627\u06cc\u06cc\u060c \u0631\u0648\u06cc \u062c\u0627\u06cc \u062e\u0627\u0644\u06cc \u0634\u06a9\u0644 \u06a9\u0644\u06cc\u06a9 \u06a9\u0646.';
    } else if (eg){
      svg.classList.add('sel'); eg.classList.add('hl');
      var fi3 = eg.getAttribute('data-fi'), ti3 = eg.getAttribute('data-ti'), rf3 = eg.getAttribute('data-rf'), ns3 = svg.querySelectorAll('.fnode');
      for (var q4 = 0; q4 < ns3.length; q4++){ var di3 = ns3[q4].getAttribute('data-i'); if ((ti3 && di3 === ti3) || (!rf3 && di3 === fi3)) ns3[q4].classList.add('hl'); }
      var sk3 = eg.getAttribute('data-sk'), want3 = rf3 || sk3;
      if (want3){ var sn3 = svg.querySelectorAll('.snode'); for (var q6 = 0; q6 < sn3.length; q6++){ if (sn3[q6].getAttribute('data-i') === fi3 && sn3[q6].getAttribute('data-k') === want3) sn3[q6].classList.add('hl'); } }
      if (info) info.textContent = 'ارتباط: از «' + eg.getAttribute('data-f') + '» به «' + eg.getAttribute('data-t') + '»' + (eg.getAttribute('data-l') ? ' — ' + eg.getAttribute('data-l') : '');
    } else if (ng){
      var go = ng.getAttribute('data-go');
      if (go){ location.href = go; return; }
      svg.classList.add('sel'); ng.classList.add('hl');
      var i2 = ng.getAttribute('data-i'), cnt = 0, es = svg.querySelectorAll('.edge');
      for (var q2 = 0; q2 < es.length; q2++){ if (es[q2].getAttribute('data-fi') === i2 || es[q2].getAttribute('data-ti') === i2){ es[q2].classList.add('hl'); cnt++; } }
      var sn4 = svg.querySelectorAll('.snode'); for (var q7 = 0; q7 < sn4.length; q7++){ if (sn4[q7].getAttribute('data-i') === i2) sn4[q7].classList.add('hl'); }
      if (info) info.textContent = 'box «' + (ng.getAttribute('data-t') || '') + '» — ' + cnt + ' خطِ وصل به آن روشن شد. برای برداشتن روشنایی، روی جای خالی شکل کلیک کن.';
    } else {
      if (info) info.textContent = 'برای دیدن ارتباط هر خط، روی خودِ خط کلیک کن تا آن خط و دو کادرِ دو سرش روشن شوند. روی هر کادر هم کلیک کنی، همهٔ خط‌های وصل به آن روشن می‌شوند. کادرهایی که علامت ↗ دارند، لینک صفحهٔ همان ماژول‌اند. خط‌چین سبزِ «دور بعد» یعنی این کار در این دور تمام شده و دور بعد از همان کادرِ بالا از نو شروع می‌شود. برای برداشتن روشنایی، روی جای خالی شکل کلیک کن.';
    }
  });
}
function fill(id, rows){ var el = document.getElementById(id); rows.forEach(function(r){ var li = document.createElement('li'); li.innerHTML = r; el.appendChild(li); }); }
function fillTable(id, rows){
  var tb = document.getElementById(id);
  rows.forEach(function(r){
    var tr = document.createElement('tr');
    r.forEach(function(c){ var td = document.createElement('td'); td.innerHTML = c; tr.appendChild(td); });
    tb.appendChild(tr);
  });
}
function render(){
  document.getElementById('ttl').textContent = DATA.title;
  document.getElementById('sub').textContent = DATA.sub || '';
  if (DATA.menu){ renderMenu(DATA.menu, 'menu'); }
  fill('steps', DATA.steps || []);
  if (DATA.example){ fill('example', DATA.example); } else { document.getElementById('exwrap').style.display = 'none'; }
  if (DATA.condHead){ DATA.condHead.forEach(function(v, i){ var th = document.getElementById('condh' + (i + 1)); if (th) th.textContent = v; }); }
  if (DATA.outHead){ DATA.outHead.forEach(function(v, i){ var th = document.getElementById('outh' + (i + 1)); if (th) th.textContent = v; }); }
  if (DATA.hideOuts){ var oh = document.getElementById('outsh'), ot = document.getElementById('outs'); if (oh) oh.style.display = 'none'; if (ot) ot.style.display = 'none'; }
  if (DATA.gloss){ fillTable('gloss', DATA.gloss); }
  else { document.getElementById('glosswrap').style.display = 'none'; }
  fillTable('conds', DATA.conds || []);
  fillTable('outs', DATA.outs || []);
  linkErrs(document.getElementById('steps')); linkErrs(document.getElementById('example')); linkErrs(document.getElementById('conds')); linkErrs(document.getElementById('outs'));
  renderFlow(); renderTree(); fitText(); wireFlow();
}
if (document.fonts && document.fonts.ready){ document.fonts.ready.then(function(){ render(); }); } else { render(); }
</script>
</body>
</html>
"""

# node helpers to keep DATA compact
def N(k, c, t=None, l=None, **kw):
    d = {'k': k, 'c': c}
    if t: d['t'] = t
    if l: d['l'] = l
    d.update(kw)
    return d

def SIDE(c, l, d='r', lab=None, **kw):
    r = {'c': c, 'l': l, 'd': d}
    if lab: r['lab'] = lab
    r.update(kw)
    return r

def T(t, *c):
    d = {'t': t}
    if c: d['c'] = list(c)
    return d


# ================= Plain-language layer (2026-10-20) =======================
# [EN] The flowcharts are read by the owner, not by a programmer. Every
#      displayed string therefore goes through PLAIN_RULES (jargon -> Persian)
#      and every page carries the same glossary, so no box depends on a word
#      the reader has not been given. Code names, links and the matching keys
#      stay untouched: this runs last, after GO/AFTER matching.
# [FA] این فلوچارت‌ها را صاحب دستگاه می‌خواند، نه برنامه‌نویس. پس هر متنِ
#      نمایش‌داده‌شده از PLAIN_RULES می‌گذرد (اصطلاح فنی -> فارسی) و هر صفحه
#      واژه‌نامهٔ یکسان دارد تا هیچ کادری به کلمه‌ای که معنی‌اش داده نشده
#      تکیه نکند. نام کد، لینک‌ها و کلیدهای تطبیق دست نمی‌خورند؛ این گام
#      آخرِ کار است، بعد از تطبیق GO/AFTER.
PLAIN_RULES = [
    (r'func__[A-Za-z0-9_]+\s*[:：]\s*', ''),
    (r'\bSnapshot\b', 'عکس اندازه‌گیری'),
    (r'\bsnapshot\b', 'عکس اندازه‌گیری'),
    (r'\bNVM\b', 'حافظهٔ ماندگار'),
    (r'\bcalibration\b', 'جدول تصحیح'),
    (r'\bwatchdog\b', 'نگهبان'),
    (r'\bcount خام\b', 'عدد خام مبدل'),
    (r'\bشمارش خام\b', 'عدد خام مبدل'),
    (r'\bmedian\b', 'میانه'),
    (r'\bfilter\b', 'صافی'),
    (r'\bduty\b', 'درصد توان'),
    (r'\brelay\b', 'رله'),
    (r'\bframe\b', 'بستهٔ خواندن'),
    (r'\bLED\b', 'چراغ'),
    (r'\bbit\b', 'پرچم'),
    (r'\bpass\b', 'دور'),
    (r'\bpasses\b', 'دورها'),
    (r'\bchannel\b', 'کانال'),
    (r'\bchannels\b', 'کانال‌ها'),
    (r'\bmode test دستی\b', 'حالت تست دستی'),
    (r'\bmode test\b', 'حالت تست'),
    (r'\bmode\b', 'حالت'),
    (r'\breset\b', 'ریست'),
    (r'set می‌شود', 'فعال می‌شود'),
    (r'set شده', 'فعال شده'),
    (r'set است', 'فعال است'),
    (r'set باشند', 'فعال باشند'),
    (r'set باشد', 'فعال باشد'),
    (r'\bset\b', 'فعال'),
    (r'\brecord\b', 'بستهٔ ذخیره'),
    (r'\bbyte\b', 'بایت'),
    (r'\btrip\b', 'قطع ایمنی'),
    (r'\bcomparator\b', 'مقایسه‌گر'),
    (r'\bCRC-16\b', 'کد کنترل خطا'),
    (r'\bCRC\b', 'کد کنترل خطا'),
    (r'ماسک خطا', 'فهرست خطاهای فعال'),
    (r'\bflash\b', 'فلش'),
    (r'\bpin\b', 'پایه'),
    (r'\btaskها\b', 'کارها'),
    (r'\btaskی\b', 'کاری'),
    (r'\btask\b', 'کار'),
    (r'\bBSP\b', 'لایهٔ سخت‌افزار'),
    (r'\bkick\b', 'تازه‌کردن'),
    (r'\bJitter\b', 'جیتر'),
    (r'پاسها', 'دورها'),
    (r'\bپاس\b', 'دور'),
    (r'فریم‌ها', 'بسته‌های خواندن'),
    (r'\bفریم\b', 'بستهٔ خواندن'),
    (r'پارامترهای', 'تنظیم‌های'),
    (r'پارامترها', 'تنظیم‌ها'),
    (r'\bپارامتر\b', 'تنظیم'),
    (r'\bتریپ\b', 'قطع ایمنی'),
    (r'کالیبراسیون', 'جدول تصحیح'),
    (r'\bESP\b', 'ماژول پنل'),
    (r'\bRAM\b', 'حافظهٔ موقت'),
    (r'\bCRC\b', 'کد کنترل خطا'),
    (r'\bPWM\b', 'پالس روشن/خاموش'),
    (r'\bDMA\b', 'انتقال مستقیم داده'),
    (r'\bTCB\b', 'بلوک کنترل کار'),
    (r'\brecordهای\b', 'بسته‌های ذخیره'),
    (r'\bpassهای\b', 'دورهای'),
    (r'\bpassها\b', 'دورها'),
    (r'\btripی\b', 'قطع ایمنی‌ای'),
    (r'\bboxهای\b', 'کادرهای'),
    (r'\bbyteها\b', 'بایت‌ها'),
    (r'\bcycleهای\b', 'چرخه‌های شارژ'),
    (r'\bchannelهای\b', 'کانال‌های'),
    (r'\bchannelها\b', 'کانال‌ها'),
    (r'\bchannelی\b', 'کانالی'),
    (r'\bframeهای\b', 'بسته‌های خواندن'),
    (r'\bparameterهای\b', 'تنظیم‌های'),
    (r'\bparameterها\b', 'تنظیم‌ها'),
    (r'\bparameter\b', 'تنظیم'),
    (r'\btimerها\b', 'زمان‌سنج‌ها'),
    (r'\btimer\b', 'زمان‌سنج'),
    (r'\bpinی\b', 'پایه‌ای'),
    (r'\blink\b', 'پیوند'),
    (r'\bcheck\b', 'بررسی'),
    (r'\bnoise\b', 'نویز'),
    (r'\bsignal\b', 'سیگنال'),
    (r'\bbox\b', 'کادر'),
    (r'\bgain\b', 'بهره'),
    (r'time-slicing', 'تقسیم زمان'),
    (r'\bvolatile\b', 'فرّار (volatile)'),
    (r'\bcycle شارژ\b', 'چرخهٔ شارژ'),
    (r'\bcycle\b', 'چرخهٔ شارژ'),
    (r'\bstate\b', 'وضعیت'),
    (r'\btrip\b', 'قطع ایمنی'),
    (r'\bJIT\b', 'جیتر'),
    (r'\bPID\b', 'کنترل‌کنندهٔ PID'),
    (r'\boffset\b', 'جبران خطای صفر'),
    (r'\bhysteresis\b', 'فاصلهٔ امن'),
    (r'\banalog\b', 'آنالوگ'),
    (r'\btelemetry\b', 'گزارش دوره‌ای'),
    (r'\bDiag\b', 'سابقه'),
    (r'(\d)\s*ms\b', r'\1 میلی‌ثانیه'),
    (r'(\d)\s*s\b', r'\1 ثانیه'),
]

PLAIN_FIELDS = ('title', 'sub')
PLAIN_LIST_FIELDS = ('steps', 'example')
PLAIN_FLOW_FIELDS = ('t', 'l', 'elab')

GLOSSARY = [
    ['نیم باتری (نیمهٔ بالا / نیمهٔ پایین)',
     'باتری این دستگاه از دو باتری ۱۲ ولتی که پشت سر هم بسته شده‌اند ساخته می‌شود و مجموعشان ۲۴ ولت است. به هر کدام از این دو تا یک «نیم» می‌گوییم: نیمهٔ پایین (همان ۱۲ ولتی که ولتاژش مستقیم اندازه‌گیری می‌شود) و نیمهٔ بالا (از ۲۴ ولت کل منهای نیمهٔ پایین حساب می‌شود، چون سیم جدا ندارد).'],
    ['نیمِ نصب‌شده / نصب‌نشده',
     'یعنی کانال شارژ آن نیم روی این برد وجود دارد یا نه. اگر یک کانال نصب نشده باشد (فلگ ساختنی‌اش صفر باشد)، همان نیم در تشخیص‌ها کنار گذاشته می‌شود تا خطای بی‌مورد ندهد. امروز هر دو کانال نصب‌اند، پس هر دو نیم بررسی می‌شوند.'],
    ['پاس (دور)',
     'یک بار کامل اجرای همان کار: از اول تابع تا آخرش، بعد خواب کوتاه و برگشت به همان نقطه برای دور بعد. هر کار (task) دور خودش را دارد؛ مثلاً کار کنترل هر ۱۰ میلی‌ثانیه یک دور تازه دارد.'],
    ['عکس اندازه‌گیری',
     'یک بستهٔ کامل از همهٔ ولتاژها و جریان‌ها که با هم و در یک لحظه خوانده شده‌اند. ماژول‌های دیگر به‌جای خواندن مستقیم سخت‌افزار، همین عکس را می‌خوانند تا همه یک تصویر یکسان ببینند.'],
    ['عکس «معتبر»',
     'یعنی این عکس از دادهٔ کامل و تازه ساخته شده است: یک بستهٔ خواندنِ کامل از مبدل آمده و دست‌کم ۳ بستهٔ پشت سر هم خوانده شده باشد. معتبر بودن ربطی به «خوب بودن» ولتاژها ندارد؛ اگر مبدل سالم باشد و باتری ۰ ولت باشد، عکس معتبر است.'],
    ['بستهٔ خواندن (frame)',
     'یک بار کامل خواندن همهٔ کانال‌های مبدل آنالوگ؛ یعنی یک ردیف عدد که با هم از سخت‌افزار گرفته می‌شود.'],
    ['عدد خام مبدل',
     'عددی که مبدل آنالوگ می‌دهد (مثلاً بین ۰ تا ۴۰۹۵) و هنوز به ولت یا میلی‌آمپر تبدیل نشده است.'],
    ['میانه (median)',
     'چند نمونه را اندازه می‌گیریم، مرتب می‌کنیم و نمونهٔ وسط را برمی‌داریم. یک پرش تک‌نمونه‌ای (نویز) می‌تواند حذف شود ولی تغییر واقعیِ مقدار دیده می‌شود.'],
    ['صافی / میانگین متحرک',
     'میانگین چند نمونهٔ آخر را می‌گیریم تا موج‌های ریز صاف شود و مقدار آرام‌تر حرکت کند.'],
    ['نگهبان (watchdog)',
     'یک شمارندهٔ سخت‌افزاری که اگر برنامه سر وقت آن را تازه نکند، خودش برد را ریست می‌کند. یعنی اگر برنامه جایی گیر کند، دستگاه به‌جای ماندن در حالت خراب، از نو روشن می‌شود.'],
    ['تسک (task)',
     'یک کار مستقل که سیستم هم‌زمان با کارهای دیگر اجرا می‌کند؛ هر کدام اولویت و دورهٔ خودش را دارد.'],
    ['پرچم (bit)',
     'یک علامت بله/خیر داخل یک عدد. مثلاً «پرچم خطای ۶» یعنی «خطای قطع باتری دیده شد یا نه».'],
    ['حافظهٔ ماندگار (NVM)',
     'بخشی از حافظهٔ میکرو که با قطع برق پاک نمی‌شود؛ تنظیم‌ها و سابقه‌ها اینجا می‌مانند.'],
    ['جدول تصحیح (calibration)',
     'جدولی که عدد خام مبدل را به مقدار واقعی (ولت/میلی‌آمپر) تبدیل می‌کند؛ با اندازه‌گیری روی دستگاه پر می‌شود.'],
    ['شارژ سریع (BULK)',
     'فاز اول شارژ: شارژر با بیشترین جریان مجاز (سقف ۶۵۰ میلی‌آمپر) شارژ می‌کند تا ولتاژ باتری به ۱۴٫۳ ولت برسد.'],
    ['جذب (ABSORB)',
     'فاز دوم: ولتاژ روی ۱۴٫۴ ولت نگه داشته می‌شود و جریان کم‌کم کم می‌شود تا باتری کامل پر شود.'],
    ['نگه‌داری (FLOAT)',
     'فاز آخر: باتری پر است و شارژر فقط ولتاژش را زیر نظر می‌گیرد؛ در این دستگاه پالس را صفر می‌کند (پارک) و اگر ولتاژ افت کند، شارژ را از نو شروع می‌کند.'],
    ['چرخهٔ شارژ (cycle)',
     'یک بار کامل شارژ: از لحظه‌ای که شارژ شروع می‌شود تا وقتی باتری کامل می‌شود و شارژر به حالت نگه‌داری می‌رود. اگر شارژ نیمه‌کاره رها شود، آن چرخه ناقص است و به آن استناد نمی‌شود.'],
    ['درصد توان (duty)',
     'در پالس‌های روشن/خاموشی که ترانس را راه می‌اندازند، نسبت زمانی که پالس روشن است به کل دوره. صفر یعنی خاموش کامل و ۱۰۰۰ در هزار یعنی روشنِ کامل؛ هر چه بیشتر، توان بیشتری به باتری می‌رسد.'],
    ['قطع ایمنی (trip)',
     'خروجی مقایسه‌گر سخت‌افزاری: وقتی جریان از حد مجاز بگذرد، خودِ سخت‌افزار آن کانال را فوراً می‌بندد و یک پرچم می‌گذارد. نام دیگرش در این صفحه «قطع ایمنی جیتر» است.'],
    ['رله (relay)',
     'یک کلید برقی که با فرمان برنامه باز یا بسته می‌شود؛ اینجا برای قطع کامل ورودی ترانس و ایمنی نهایی است.'],
    ['بهره (gain)',
     'ضریبی که عدد خام مبدل را به مقدار واقعی تبدیل می‌کند؛ «خط بهره» یعنی همان خط راستی که از آخرین نقطهٔ جدول با همین ضریب ادامه می‌دهد.'],
    ['جبران خطای صفر (offset)',
     'عددی که پنل می‌دهد تا خطای ثابت اندازه‌گیری (مثلاً وقتی جریان واقعاً صفر است ولی مبدل عددی جز صفر می‌دهد) جبران شود.'],
    ['جدول نصب کانال‌ها (CHG_INSTALLED_CHANNEL_MASK)',
     'یک عدد در کد که هر بیتش می‌گوید یک کانال شارژ روی این برد نصب است یا نه. بیت ۰ برای باتری بالا و بیت ۱ برای باتری پایین. اگر بیتی صفر باشد، همان باتری در بررسی‌ها کنار گذاشته می‌شود.'],
    ['UART (خط سریال)', 'خط ارتباطی سریال بین میکرو و پنل؛ بایت‌ها یکی‌یکی و پشت سر هم می‌آیند و DMA آن‌ها را بدون درگیرکردن پردازنده جابه‌جا می‌کند.'],
    ['DMA', 'یک واحد سخت‌افزاری که داده را خودش از مبدل یا سریال به حافظه می‌برد؛ پردازنده درگیر جابه‌جایی نمی‌شود و وقتش را برای تصمیم‌ها نگه می‌دارد.'],
    ['PWM (پالس روشن/خاموش)', 'سیگنالی که با روشن و خاموش‌شدن سریع، توان را کم و زیاد می‌کند؛ شارژر با آن می‌گوید با چه درصد توانی کار کند.'],
    ['EXTI (وقفهٔ خارجی)', 'ورودی سخت‌افزاری که با تغییر یک پایه، برنامه را فوراً خبر می‌کند؛ مثل افتادن برق ورودی یا قطع ایمنی جیتر.'],
    ['فهرست خطاهای فعال', 'یک عدد بیتی که می‌گوید کدام خطاها همین حالا فعال‌اند؛ هر بیت یعنی یک خطای مشخص.'],
    ['استک (stack)', 'حافظهٔ ثابتی که هر کار برای متغیرهای موقت خودش دارد؛ اندازه‌اش از پیش تعیین شده و وسط کار بزرگ نمی‌شود.'],
    ['تیک (tick)', 'ضربان زمان‌بند سیستم؛ اینجا هر تیک ۱ میلی‌ثانیه است و نوبت‌دهی کارها با آن شمرده می‌شود.'],
    ['کنترل‌کنندهٔ PID',
     'یک تنظیم‌کنندهٔ خودکار: خروجی را کم و زیاد می‌کند تا مقدار اندازه‌گیری‌شده به عدد هدف برسد. اینجا جریان شارژ را با آن ثابت نگه می‌دارند.'],
    ['RTOS (سیستم‌عامل کوچک)',
     'برنامه‌ای که چند کار را نوبتی و با اولویت روی همان یک میکرو اجرا می‌کند؛ هر کار دورهٔ خودش را دارد.'],
    ['نگهبان سخت‌افزاری (IWDG)',
     'شمارنده‌ای در خود میکرو که اگر برنامه سر وقت تازه‌اش نکند، برد را ریست می‌کند؛ تقریباً هر ۱ ثانیه یک بار باید تازه شود.'],
    ['مبدل آنالوگ (ADC)',
     'قطعه‌ای که ولتاژ واقعی را به عدد تبدیل می‌کند تا برنامه بتواند بخواند.'],
    ['کد کنترل خطا (CRC)',
     'یک عدد کوچک که همراه هر بسته فرستاده می‌شود تا گیرنده بفهمد بسته سالم رسیده یا خراب شده است.'],
    ['جیتر (Jitter)',
     'مقایسه‌گر سخت‌افزاری که جریان زیاد یا قطع‌شدن باتری را همان لحظه می‌بیند و پرچم می‌دهد، بدون اینکه منتظر دور بعدی برنامه بماند.'],
    ['وقفه (interrupt)',
     'یک رویداد سخت‌افزاری که کار جاری را لحظه‌ای کنار می‌زند تا فوراً رسیدگی شود؛ مثلاً آمدن یک بایت از پنل یا لبهٔ سیگنال جیتر.'],
]


def plain_text(value):
    for pattern, replacement in PLAIN_RULES:
        value = re.sub(pattern, replacement, value)
    return value


def plain_module(module):
    for field in PLAIN_FIELDS:
        if isinstance(module.get(field), str):
            module[field] = plain_text(module[field])
    for field in PLAIN_LIST_FIELDS:
        if isinstance(module.get(field), list):
            module[field] = [plain_text(x) for x in module[field]]
    for row_key in ('conds', 'outs'):
        if isinstance(module.get(row_key), list):
            module[row_key] = [[plain_text(c) for c in row] for row in module[row_key]]
    if isinstance(module.get('tree'), dict):
        def walk(node):
            if isinstance(node.get('t'), str):
                node['t'] = plain_text(node['t'])
            for kid in node.get('c', []) or []:
                walk(kid)
        walk(module['tree'])
    for node in module.get('flow', []):
        for field in PLAIN_FLOW_FIELDS:
            if isinstance(node.get(field), str):
                node[field] = plain_text(node[field])
            elif isinstance(node.get(field), list):
                node[field] = [plain_text(x) for x in node[field]]
        for side_key in ('side', 'note'):
            side = node.get(side_key)
            if isinstance(side, dict):
                if isinstance(side.get('l'), list):
                    side['l'] = [plain_text(x) for x in side['l']]
                for extra in ('after', 'lab'):
                    if isinstance(side.get(extra), str):
                        side[extra] = plain_text(side[extra])
    return module


MODULES = {}

# ================= Charger (from charger.c, verified) =================
MODULES['charging'] = {
 'file': 'charging_process.html',
 'title': 'فرآیند شارژ — توضیح ساده',
 'sub': 'رفتار فعلی برد از روی کد شارژر؛ هر دستگاه دو باتری ۱۲ ولتی و برای هر باتری یک channel شارژ جدا با رفتار یکسان دارد.',
 'steps': [
   '<b>شروع:</b> وقتی ورودی بین <b class="num">۲۲ تا ۲۸ ولت</b> باشد و باتری وصل باشد، شارژ می‌تواند شروع شود؛ وگرنه شارژر صبر می‌کند و جریانی نمی‌دهد.',
   '<b>آرام‌سازی:</b> هر بار باتری تازه وصل شود، <b class="num">۱۵ ثانیه</b> صبر می‌کنیم تا اتصال جا بیفتد؛ بعد شارژ آرام و بدون پرش شروع می‌شود.',
   '<b>شارژ سریع:</b> جریان کامل با سقف <b class="num">۶۵۰ میلی‌آمپر</b> تا ولتاژ باتری به <b class="num">۱۴٫۳ ولت</b> برسد؛ جریان با PID تنظیم می‌شود.',
   '<b>جذب:</b> ولتاژ روی <b class="num">۱۴٫۴ ولت</b> ثابت می‌ماند و جریان کم‌کم پایین می‌آید؛ اگر ولتاژ زیر ۱۴٫۳ بیفتد، شارژ برمی‌گردد به شارژ سریع و ۱۰ دقیقه از اول شمرده می‌شود.',
   '<b>کامل‌شدن:</b> ۱۰ دقیقه در جذب + جریان زیر <b class="num">۵۰ میلی‌آمپر</b> برای <b class="num">۶۰ ثانیه</b>؛ یا سقف امن ۱ ساعت.',
   '<b>نگه‌داری:</b> PWM به صفر می‌رود و شارژر پارک می‌ماند؛ <b class="num">۱۳٫۵ ولت</b> فقط هدف اسمی این فاز است. افت زیر <b class="num">۱۲٫۸ ولت</b> یعنی شارژ از نو.',
 ],
 'example': [
   'باتری ۱۱٫۹ ولتی وصل می‌شود؛ ۱۵ ثانیه آرام‌سازی، بعد شارژ سریع با حدود ۶۴۰ میلی‌آمپر.',
   'ولتاژ بالا می‌رود تا ۱۴٫۳؛ وارد جذب می‌شود و جریان از ۶۰۰ به زیر ۵۰ میلی‌آمپر می‌آید.',
   'بعد از ۱۰ دقیقه و ۶۰ ثانیه ثبات، شارژ کامل است؛ PWM به صفر می‌رود و باتری رها می‌شود.',
 ],
 'flow': [
   N('sec','navy','بخش ۱: بررسی‌های کلی — در هر pass، قبل از شارژ'),
   N('pill','navy','شروع pass شارژر', ref='func__Charger_Evaluate'),
   N('dia','yellow','ذخیره در حافظهٔ ماندگار در حال انجام است؟', side=SIDE('gray',['در این مدت هر دو خروجی شارژر صفر است','وضعیت و زمان شارژ از بین نمی‌رود','ذخیره که تمام شد، شارژ از همان‌جا ادامه پیدا می‌کند'], 'r', 'بله'), elab='نه'),
   N('dia','yellow','قفل خطای فنی (خطای ۷) از قبل set شده؟', ref='func__Charger_FinalDisconnect', side=SIDE('red',['قطع نهایی آمد: relay باز است و هر دو خروجی صفرند','این قفل فقط با خاموش و روشن‌کردن برد پاک می‌شود'], 'r', 'بله'), elab='نه'),
   N('dia','yellow','mode test دستی پنل فعال است؟', ref='func__Charger_IsManualTestModeActive', side=SIDE('gray',['در این mode، duty را پنل می‌فرستد','بررسی‌های باتری در این mode انجام نمی‌شوند','اگر ۳ ثانیه از پنل خبری نشد، شارژر خودش از این mode بیرون می‌آید'], 'r', 'بله'), elab='نه'),
   N('dia','yellow','سیم باتری قطع است؟ (خطای ۶)', ref='func__Fault_Get: وضعیت خطای ۶ از ماژول خطا', side=SIDE('red',['همان channel می‌ایستد و خروجی‌اش صفر می‌شود','تا وقتی خطای ۶ هست، همین‌طور می‌ماند','خطا که پاک شد، channel خاموش می‌شود و شارژ را آرام از ۱٪ از نو شروع می‌کند'], 'r', 'بله'), elab='نه'),
   N('box','blue','ثبت وضعیت زنده برای پنل و سابقه', ['duty و state هر channel و جریان و تخمین همین pass'], ref='func__Charger_CaptureDiag'),
   N('dia','yellow','اندازه‌گیری سالم و شارژ مجاز است؟', side=SIDE('gray',['توقف امن: همهٔ خروجی‌ها صفر می‌شوند','قفل‌هایی که از قبل بودند سر جایشان می‌مانند: قفل نهایی، قفل قطع باتری و قفل حرارتی'], 'r', 'نه'), elab='بله'),
   N('dia','yellow','خطای نهایی (FINAL_FAULT) روی یکی از channelها هست؟', ref='func__Charger_FinalDisconnect', side=SIDE('red',['relay باز می‌شود و هر دو خروجی صفر می‌مانند','این قفل تا خاموش و روشن‌شدن برد می‌ماند'], 'r', 'بله'), elab='نه'),
   N('dia','yellow','ولتاژ ورودی بین ۲۲ و ۲۸ ولت است؟', ref='func__Charger_SafeIdle', side=SIDE('gray',['شارژر در حالت صبر (INPUT_WAIT) می‌ماند و شارژ صفر است','ورودی که درست شد، شارژ خودش ادامه پیدا می‌کند'], 'r', 'نه'),
     note=SIDE('red',['خطای فنی (خطای ۷) جدا از بقیه بررسی می‌شود','نشانهٔ اول: duty بالای ۲۰٪ است، ولی جریان همان channel صفر است','نشانهٔ دوم: relay باز است، duty صفر است و trip JIT ثبت شده است','یکی از این دو که باشد، قفل فنی می‌خورد، خطای ۷ می‌آید و در Diag ثبت می‌شود'],'l'), elab='بله'),
   N('dia','yellow','پرچم trip سخت‌افزاری یکی از channelها آمده؟', ref='func__Charger_HandleJitTrip', side=SIDE('red',['همان channel فوری با duty صفر می‌ایستد','trip شمرده می‌شود؛ بعد از ۳ ثانیه یکی‌یکی با درصد توان کمتر برمی‌گردد','trip سوم همان channel: خطای نهایی، relay باز می‌شود و خطای ۷ می‌آید'], 'r', 'بله'), elab='نه'),
   N('sec','navy','بخش ۲: بررسی هر channel (۱ و ۲ جدا)'),
   N('pill','navy','برای هر channel', ref='func__Charger_Evaluate: حلقهٔ channelها'),
   N('dia','yellow','channel از پنل خاموش شده؟', ref='func__Charger_RegulateChannel: خاموشی channel از پنل', side=SIDE('gray',['این channel خاموش می‌ماند و خروجی‌اش صفر است','از پنل که وصل شود، شارژ آرام از ۱ درصد توان شروع می‌شود'], 'r', 'بله'), elab='نه'),
   N('dia','yellow','قفل باتری خراب یا بودجهٔ عدم‌توازن تمام شده؟', ref='func__Charger_DeadMask', side=SIDE('red',['شارژ این channel بسته است و خروجی‌اش صفر است','اگر باتری خراب باشد: تا وقتی باتری عوض شود','اگر بودجهٔ عدم‌توازن باشد: تا وقتی بودجه تمام شود'], 'r', 'بله'), elab='نه'),
   N('dia','yellow','ولتاژ باتری بین ۲ و ۱۵ ولت است؟', ref='func__Charger_BatteryVoltageIsValid', side=SIDE('gray',['channel خاموش می‌ماند و صبر می‌کند','این بررسی جلوی شارژشدن باتری قطع یا کاملاً خراب را می‌گیرد'], 'r', 'نه'), elab='بله'),
   N('dia','yellow','جریان از ۹۵۰ میلی‌آمپر بالاتر رفته؟', ref='func__Charger_ResetChannelToOff', side=SIDE('red',['channel خاموش می‌شود؛ دفعهٔ بعد آرام از اول شروع می‌کند','جلوی جریان خیلی زیاد را Jitter سخت‌افزاری زودتر می‌گیرد'], 'r', 'بله'), elab='نه'),
   N('sec','navy','بخش ۳: فازهای شارژ'),
   N('dia','yellow','اتصال باتری ۱۵ ثانیه پایدار بوده؟', ref='func__Charger_BulkStartSettled', side=SIDE('gray',['channel خاموش می‌ماند','۱۵ ثانیه از وقتی شمرده می‌شود که ولتاژ باتری دیده شد'], 'r', 'نه'), elab='بله'),
   N('box','blue','شارژ سریع (BULK)', ['شروع آرام با ۱ درصد توان','PID جریان را تا سقف ۶۵۰ میلی‌آمپر نگه می‌دارد'], ref='func__Charger_RegulateChannel → PidStep → ApplyDuty',
     side=SIDE('gray',['داغ‌شدن: اگر ۱ ساعت نزدیک سقف جریان کار کند','۱۰ دقیقه استراحت می‌کند (زرد چشمک می‌زند) و بعد ادامه می‌دهد','این شمارش فقط در شارژ سریع و نزدیک سقف جریان است'])),
   N('dia','yellow','ولتاژ به ۱۴٫۳ رسید؟', loop={'to': 'شارژ سریع (BULK)', 'lab': 'نه'}, elab='بله'),
   N('box','blue','جذب (ABSORB)', ['ولتاژ با PID روی ۱۴٫۴ ولت نگه داشته می‌شود','بالای ۱۴٫۸ ولت، سقف پشتیبان جلوی duty را می‌گیرد و آن را پایین می‌آورد'], ref='RegulateChannel: نگه‌داری ولتاژ جذب',
     note=SIDE('red',['شمارش زمان باتری خراب، جدا انجام می‌شود','این شمارنده فقط وقتی می‌شمارد که channel واقعاً در حال شارژ باشد','اگر شارژ کامل شود، یا مکث از ۱ دقیقه بیشتر شود، شمارنده صفر می‌شود','اگر شارژ به ۲۴ ساعت برسد و باتری کامل نشود، باتری خراب قفل می‌شود (بخش ۲)'],'l'),
     loop={'to': 'شارژ سریع (BULK)', 'lab': 'افت زیر ۱۴٫۳: برگشت به شارژ سریع', 'col': 'red'}),
   N('dia','yellow','کامل شد؟', ['۱۰ دقیقه در جذب مانده و جریان ۶۰ ثانیه زیر ۵۰ میلی‌آمپر بوده؟','یا سقف امنِ ۱ ساعت (پس از کم‌شدن جریان) پر شده؟'], loop={'to': 'جذب (ABSORB)', 'lab': 'نه'}, elab='بله'),
   N('box','green','نگه‌داری (FLOAT) — شارژ کامل', ['duty تا صفر پایین می‌آید و پارک می‌ماند','۱۳٫۵ ولت هدف اسمی این فاز است؛ با duty صفر اعمال نمی‌شود','سبز ثابت و زرد خاموش می‌شود'], ref='func__Charger_RegulateChannel: پارک duty در نگه‌داری'),
   N('dia','yellow','ولتاژ زیر ۱۲٫۸ افتاد؟ (باتری دارد خالی می‌شود)', loop={'to': 'شارژ سریع (BULK)', 'lab': 'بله: شارژ از نو'}, elab='نه'),
   N('box','green','شارژ این channel کامل شد — باتری آماده'),
   N('sec','navy','بخش ۴: فرمان‌ها و خروجی‌ها — شارژر به کجا فرمان می‌دهد', brk=True),
   N('box','blue','فرمان شارژ: duty حساب‌شده با گیرهٔ سقف', ['duty به PWM هر channel فرستاده می‌شود؛ هر جا شارژ متوقف است، duty صفر است'], ref='func__Charger_ApplyDuty → BspPwm_SetDutyPermille'),
   N('box','navy','relay قطع نهایی در خطای نهایی باز می‌شود', ['برق ورودی که قطع شود، relay رها می‌شود تا باتری خالی نشود'], ref='func__Charger_FinalDisconnect → BspGpio RELAY'),
   N('box','green','اطلاع به چراغ و بوق، پنل و نمایش وضعیت', ['باتری خراب که قفل باشد، جلوی خروجی دستگاه گرفته می‌شود'], ref='func__Charger_IsChargeComplete / func__Charger_DeadBlocksOutput'),
 ],
 'tree': T('ارزیابی شارژر — هر pass کنترل',
   T('بررسی‌های کلی (به همین ترتیب)',
     T('ذخیره در حافظهٔ ماندگار؟ → خروجی صفر، پایان دور'),
     T('قفل فنی؟ → قطع نهایی'),
     T('mode دستی: درخواست پنل؛ ۳ ثانیه سکوت یعنی خروج از این mode'),
     T('خطای ۶: توقف channelی که باتری‌اش قطع است'),
     T('ثبت وضعیت زنده برای پنل و سابقه'),
     T('اندازه‌گیری/حالت کلی بد؟ → توقف امن'),
     T('خطای نهایی؟ → قطع نهایی'),
     T('ورودی بد؟ → صبر برای ورودی'),
     T('trip JIT: توقف و شمارش؛ سومی = نهایی'),
     T('احیای JIT: یکی‌یکی بعد از ۳ ثانیه')),
   T('برای هر channel (۱ و ۲)',
     T('شمارش زمان باتری خراب'),
     T('استراحت حرارتی ۱ ساعت → ۱۰ دقیقه'),
     T('mode دستی؟ → duty پنل'),
     T('تنظیم عادی channel',
       T('جلوگیری‌ها: خاموشی از پنل / باتری خراب / عدم‌توازن'),
       T('ولتاژ باتری خارج از ۲ تا ۱۵؟ → خاموش'),
       T('جریان بالای ۹۵۰؟ → reset به خاموش'),
       T('۱۵ ثانیه پایداری → شارژ سریع از ۱٪'),
       T('شارژ سریع (PID، سقف ۶۵۰)'),
       T('جذب ۱۴٫۴: افت ولتاژ یعنی برگشت و شمارش از اول'),
       T('کامل: ۱۰ دقیقه + ۵۰ میلی‌آمپرِ ۶۰ ثانیه'),
       T('نگه‌داری: پارک duty صفر؛ افت زیر ۱۲٫۸ = از نو'),
       T('فرمان: duty با سقف مجاز به PWM'))),
   T('خطای فنی (۷) جدا بررسی می‌شود: قفل و ثبت در Diag'),
   T('خروجی‌ها: PWM، relay، چراغ و بوق، گرفتن جلوی خروجی، پنل')),
 'conds': [
   ['ورودی از ۲۲ ولت پایین‌تر یا از ۲۸ ولت بالاتر رفت','شارژ صفر می‌شود و برد در صبر می‌ماند؛ برگشت خودش ادامه می‌دهد.','ورودی معتبر: ۲۲ تا ۲۸ ولت'],
   ['سیم باتری وسط شارژ کشیده شد','<b>خطای ۶:</b> شارژ همان لحظه صفر، چراغ قرمز چشمک و سه بوق؛ با وصل دوباره و ۱۵ ثانیه آرام‌سازی به حالت عادی برمی‌گردد.','قطع باتری (خطای ۶)'],
   ['channel شارژ جریان نمی‌دهد (خرابی داخلی)','<b>خطای ۷:</b> قفل فنی؛ relay و خروجی‌ها قطع تا خاموش و روشن‌شدن برد.','duty بالای ۲۰٪، جریان صفر و ولتاژ باتری بالای ۶ ولت'],
   ['ولتاژ باتری در جذب زیر ۱۴٫۳ افتاد','برمی‌گردد به شارژ سریع و شمارش ۱۰ دقیقه از اول شروع می‌شود.','۱۴٫۳ ولت'],
   ['یک ساعت پشت‌سرهم نزدیک سقف جریان رفت','۱۰ دقیقه استراحت؛ چراغ زرد چشمک می‌زند؛ بعد شارژ ادامه دارد.','۱ ساعت کار، ۱۰ دقیقه استراحت'],
   ['شارژ ۲۴ ساعت فعال بود ولی باتری کامل نشد','<b>باتری خراب:</b> شارژ قفل، قرمز ثابت؛ پاک‌شدن فقط با نبودن باتری برای ۳ ثانیه.','پیش‌فرض ۲۴ ساعت (parameter ۱۲۵)؛ مکث بلندتر از ۱ دقیقه شمارش را صفر می‌کند (۱۲۶)'],
   ['جریان ناگهانی (trip JIT)','channel فوری می‌ایستد؛ احیا یکی‌یکی بعد از ۳ ثانیه با درصد توان کمتر؛ trip سوم همان channel = خطای نهایی و قطع relay.','قفل ۳ ثانیه'],
   ['باتری در نگه‌داری زیر ۱۲٫۸ افتاد','شارژ از اول شروع می‌شود (شارژ سریع).','۱۲٫۸ ولت'],
 ],
 'outs': [
   ['PWM هر channel','درصد توان حساب‌شدهٔ PID با گیرهٔ سقف مجاز','در هر pass؛ در همهٔ توقف‌ها صفر'],
   ['relay قطع نهایی','بازکردن relay و قطع فیزیکی ورودی ترانس','خطای نهایی (trip سوم JIT) یا قفل فنی (۷)؛ تا پایان همین روشن‌بودن'],
   ['چراغ‌ها و بوق','وضعیت channel: شارژ، کامل (سبز ثابت)، استراحت (زرد چشمک‌زن)، باتری خراب (قرمز ثابت + بوق جدا)','«شارژ کامل» فقط وقتی همهٔ channelهای فعال در نگه‌داری‌اند'],
   ['تعویض خروجی (Changeover)','جلوی خروجیِ باتری «خراب» قفل‌شده گرفته می‌شود','تا پاک‌شدن قفل با تعویض واقعی باتری'],
   ['پنل و Diag','وضعیت فاز و duty هر channel، شمارندهٔ باتری خراب، ثبت «شارژ کامل» و خطاها','در هر pass؛ پنل parameterها را هم از همین راه تنظیم می‌کند'],
 ],
}


MODULES['measurement'] = {
 'file': 'measurement_process.html',
 'title': 'فرآیند اندازه‌گیری — توضیح ساده',
 'sub': 'این ماژول هر ۱ میلی‌ثانیه ولتاژها و جریان‌ها را از مبدل آنالوگ می‌خواند، تمیزشان می‌کند و یک «عکس» معتبر به همهٔ ماژول‌ها می‌دهد.',
 'steps': [
   '<b>خواندن:</b> عددهای خام از مبدل آنالوگ با DMA می‌آیند؛ در هر دور فقط یک بستهٔ کامل و تازه پذیرفته می‌شود.',
   '<b>گرم‌شدن:</b> تا سه بستهٔ کامل و تازهٔ پشت سر هم نگذرد، خروجی «نامعتبر» است؛ از بستهٔ سوم به بعد معتبر می‌شود.',
   '<b>تمیزکردن جریان:</b> اول «میانه» گرفته می‌شود (پرشِ یک‌نمونه‌ای را حذف می‌کند)، بعد میانگین متحرک با پنجرهٔ پیش‌فرض ۱۰ نمونه؛ هر دو از پنل قابل تنظیم‌اند.',
   '<b>تمیزکردن ولتاژ:</b> میانهٔ ۵ نمونه روی عدد خام هر دو باتری گرفته می‌شود؛ ولتاژ ورودی صاف نمی‌شود تا افتِ سریع فوراً دیده شود.',
   '<b>تبدیل:</b> ولتاژ با فرمول مقسم به میلی‌ولت و جریان با جدول تصحیح به میلی‌آمپر تبدیل می‌شود؛ جبران خطای صفر را پنل می‌دهد و حداکثر ±۵ ولت است.',
   '<b>دو نیمهٔ باتری:</b> دستگاه دو باتری ۱۲ ولتی دارد که پشت سر هم بسته شده‌اند و مجموعشان ۲۴ ولت است. «باتری پایین» همان ۱۲ ولتی است که ولتاژش مستقیم اندازه‌گیری می‌شود؛ «باتری بالا» از ۲۴ ولت کل منهای باتری پایین حساب می‌شود، چون سیم جداگانه ندارد.',
   '<b>انتشار:</b> عکس کامل زیر قفل هسته منتشر می‌شود و پرچم «معتبر» آخر از همه روشن می‌شود تا هیچ ماژولی دادهٔ نیمه‌کاره نبیند.',
 ],
 'example': [
   'شارژر دارد با جریان پله‌ای کار می‌کند و یک نمونهٔ جریان ناگهان ۳ آمپر پرش می‌کند.',
   'میانه آن پرش یک‌نمونه‌ای را حذف می‌کند؛ میانگین ۱۰تایی هم موج جریان را صاف می‌کند.',
   'پس شارژر و بقیه عدد آرام و درست را می‌بینند و بی‌جهت واکنش نشان نمی‌دهند.',
 ],
 'flow': [
   N('sec','navy','بخش ۱: گرفتن یک بستهٔ تازه و سنجش اعتبارش'),
   N('pill','navy','شروع دور اندازه‌گیری (هر ۱ میلی‌ثانیه)', ref='func__Measurement_Run'),
   N('dia','yellow','یک بستهٔ کامل و تازه از مبدل رسید؟', ref='func__BspAdc_GetRaw', side=SIDE('red',['عکس این دور «نامعتبر» است','شمارش گرم‌شدن از نو شروع می‌شود تا داده قابل اعتماد شود'], 'r', 'نه'), elab='بله', note=SIDE('gray',['DMA پیوسته بستهٔ تازه می‌آورد','اگر DMA قطع شود، بستهٔ قبلی کهنه است و معتبر نیست'],'l')),
   N('box','blue','شمارش بسته‌های تازهٔ پشت سر هم', ['دو بستهٔ اول دورهٔ گرم‌شدن است و خروجی نامعتبر می‌ماند','از بستهٔ سوم به بعد عکس معتبر می‌شود'], ref='func__Measurement_Run: warmup'),
   N('dia','yellow','تنظیم صافیِ جریان از پنل عوض شده؟', side=SIDE('gray',['صافی‌ها از نو شروع می‌کنند تا نمونهٔ کهنه با تنظیم تازه قاطی نشود'], 'r', 'بله'), elab='نه'),
   N('sec','navy','بخش ۲: تمیزکردن عددها و تبدیل به ولت و میلی‌آمپر'),
   N('box','blue','تمیزکردن جریان‌ها روی عدد خام مبدل', ['میانه، پرش یک‌نمونه‌ای را حذف می‌کند','میانگین متحرک با پنجرهٔ پیش‌فرض ۱۰ نمونه'], ref='func__Measurement_ApplyCurrentFilters'),
   N('box','blue','تمیزکردن ولتاژ باتری‌ها', ['میانهٔ ۵ نمونه روی عدد خام','ولتاژ ورودی صاف نمی‌شود تا سریع دیده شود'], ref='func__Measurement_Run: median ولتاژ'),
   N('box','blue','تبدیل عدد خام به ولت و میلی‌آمپر', ['ولتاژ: فرمول مقسم → میلی‌ولت','جریان: جدول تصحیح → میلی‌آمپر','جبران خطای صفر از پنل، حداکثر ±۵ ولت'], ref='func__Measurement_CountsToMv'),
   N('box','blue','ساختن دو نیمهٔ باتری (بالا و پایین)', ['باتری پایین: همان ۱۲ ولتی که مستقیم اندازه می‌گیریم','باتری بالا: ۲۴ ولت کل منهای باتری پایین (منفی نمی‌شود)'], ref='func__Measurement_Run: ساختن نیم‌ها'),
   N('box','blue','خواندن حضور ورودی از پایهٔ منطقی برد', [], ref='func__BspGpio_Read: حضور ورودی'),
   N('sec','navy','بخش ۳: دادن عکس اندازه‌گیری به بقیهٔ ماژول‌ها'),
   N('box','green','انتشار عکس اندازه‌گیری زیر قفل', ['پرچم «معتبر» آخر از همه روشن می‌شود','همهٔ ماژول‌ها همین عکس را می‌خوانند'], ref='func__Measurement_GetSnapshot'),
 ],
 'tree': T('اندازه‌گیری — هر ۱ میلی‌ثانیه',
   T('گرفتن یک بستهٔ کامل با DMA',
     T('بسته نرسید؟ → نامعتبر + گرم‌شدن از نو')),
   T('گرم‌شدن: ۳ بستهٔ تازهٔ پشت سر هم'),
   T('تنظیم صافی عوض شد؟ → صافی‌ها از نو'),
   T('زنجیرهٔ جریان: میانه + میانگین متحرک'),
   T('زنجیرهٔ ولتاژ: میانهٔ ۵ نمونه'),
   T('تبدیل: ولتاژ با فرمول مقسم، جریان با جدول تصحیح'),
   T('باتری پایین و باتری بالا'),
   T('انتشار عکس زیر قفل؛ معتبر آخر')),
 'conds': [
   ['بستهٔ مبدل نرسید یا ناپایدار بود','خروجی «نامعتبر» می‌شود و بقیهٔ ماژول‌ها روی دادهٔ قبلی تصمیم تازه نمی‌گیرند.','۳ بستهٔ کامل؛ دو بستهٔ اول نامعتبر'],
   ['یک نمونهٔ جریان پرش بزرگ داشت','میانه آن را حذف می‌کند؛ میانگین متحرک هم موج را صاف می‌کند.','پنجرهٔ میانگین پیش‌فرض ۱۰ (تا ۳۰۰ از پنل)'],
   ['تنظیم صافی وسط کار از پنل عوض شد','صافی‌ها همان‌جا از نو شروع می‌کنند؛ نمونهٔ کهنه وارد محاسبهٔ تازه نمی‌شود.','بدون قفل بین کارها'],
   ['جبران خطای صفر از پنل داده شد','بعد از تبدیل اعمال می‌شود و هیچ‌وقت از سقف رد نمی‌شود.','حداکثر ±۵۰۰۰ میلی‌ولت'],
 ],
 'outs': [
   ['عکس اندازه‌گیری','ولتاژ ورودی، ولتاژ دو باتری، جریان هر کانال و پرچم «معتبر»','شارژر، ماژول خطا، عدم‌توازن، تعویض خروجی، چراغ‌ها و پنل، همه همین عکس را می‌خوانند'],
   ['تنظیمات صافی از پنل','اندازهٔ میانه و پنجرهٔ میانگین و جبران خطای صفرِ ولتاژها','از پنل می‌آید و در دور بعدی اندازه‌گیری اعمال می‌شود'],
 ],
}

MODULES['fault'] = {
 'file': 'fault_process.html',
 'title': 'فرآیند خطاها — توضیح ساده',
 'sub': 'این ماژول نگهبان قطع‌شدن باتری است: فقط او تصمیم می‌گیرد «خطای ۶» روشن شود یا پاک شود؛ شارژر و چراغ‌ها از تصمیم او پیروی می‌کنند.',
 'steps': [
   '<b>قانون ۱ (در حال شارژ):</b> اگر شارژر مشغول شارژ یکی از دو باتری باشد و ولتاژ همان باتری بالای <b class="num">۱۴٫۸ ولت</b> برود و <b class="num">۱۵۰ میلی‌ثانیه</b> بالا بماند، یعنی سیم آن باتری قطع شده است (چون باتری‌ای که نیست، ولتاژ شارژر را بالا می‌برد).',
   '<b>قانون ۲ (باتری نیست):</b> اگر ولتاژ ورودی سالم باشد و ولتاژ هر کدام از دو باتری زیر <b class="num">۶ ولت</b> بیفتد و <b class="num">۱ ثانیه</b> بماند، آن باتری قطع است.',
   '<b>بازیابی:</b> وقتی هر دو باتری دست‌کم <b class="num">۷ ولت</b> شدند و ۱ ثانیه پایدار ماندند، خطا پاک می‌شود (۷ به‌جای ۶ یعنی یک ولت فاصلهٔ امن، تا باتری خالیِ وصل‌شده پشت سر هم خطا نگیرد).',
   '<b>فقط باتری‌های نصب‌شده بررسی می‌شوند:</b> اگر کانال شارژ یک باتری روی این برد نصب نشده باشد (یعنی در جدول <b>CHG_INSTALLED_CHANNEL_MASK</b> برای آن باتری صفر نوشته شده باشد)، همان باتری هیچ‌وقت «قطع» حساب نمی‌شود و خطا نمی‌دهد. روی این دستگاه هر دو کانال نصب‌اند، پس هر دو باتری بررسی می‌شوند.',
   'در حالت تست دستی یا وقتی عکس اندازه‌گیری نامعتبر است، همهٔ شمارش‌ها همان‌جا <b>متوقف می‌شوند</b>؛ نه خطای تازه‌ای می‌آید، نه خطایی پاک می‌شود.',
   '<b>خطای ۷ (فنی شارژر):</b> این خطا را شارژر خودش می‌سازد و ماژول خطا در آن دخالتی ندارد؛ شرح کاملش در <a href="../Charger/charging_process.html">صفحهٔ شارژر</a> است.',
 ],
 'example': [
   'وسط شارژ، سیم باتری پایین کشیده می‌شود؛ شارژر همان باتری را شارژ می‌کرد و ولتاژش یک‌دفعه به ۱۵ ولت می‌پرد.',
   'وقتی ۱۵۰ میلی‌ثانیه بالای ۱۴٫۸ ماند، خطای ۶ روشن می‌شود: شارژر متوقف می‌شود، چراغ قرمز چشمک می‌زند و بوق می‌آید.',
   'سیم را برمی‌گردانی؛ هر دو باتری بالای ۷ ولت می‌ایستند و بعد از ۱ ثانیه خطا پاک می‌شود و شارژ آرام از سر گرفته می‌شود.',
 ],
 'flow': [
   N('sec','navy','بخش ۱: بررسی‌های اول و باتری‌های نصب‌شده'),
   N('pill','navy','شروع بررسی خطاها (هر دور کنترل)', ref='func__Fault_Evaluate'),
   N('dia','yellow','حالت تست دستی از پنل روشن است؟', ref='func__Charger_IsManualTestModeActive', side=SIDE('gray',['همهٔ شمارش‌ها همان‌جا می‌ایستند','در حالت تست دستی نه خطای تازه‌ای می‌آید، نه خطایی پاک می‌شود'], 'r', 'بله'), elab='نه'),
   N('dia','yellow','عکس اندازه‌گیری معتبر است؟', side=SIDE('gray',['همه‌چیز همان‌جا می‌ایستد','وقتی عکس معتبر برگشت، شمارش از صفر شروع می‌شود'], 'r', 'نه'), elab='بله'),
   N('box','blue','مشخص می‌شود کدام باتری نصب است', ['باتری‌ای که کانال شارژش نصب نیست، بررسی نمی‌شود'], ref='CHG_INSTALLED_CHANNEL_MASK: نیم‌های نصب‌شده'),
   N('sec','navy','بخش ۲: دو قانون خطای ۶'),
   N('dia','yellow','قانون ۱: باتری در حال شارژ بالای ۱۴٫۸ ولت رفت و ۱۵۰ میلی‌ثانیه ماند؟', ref='func__Fault_Evaluate: قانون ۱', side=SIDE('red',['خطای ۶ روشن می‌شود (باتری قطع است)'], 'r', 'بله', retToText='خروجی: پرچم خطای ۶'), elab='نه'),
   N('dia','yellow','قانون ۲: ورودی سالم است و یک باتری زیر ۶ ولت افتاد و ۱ ثانیه ماند؟', ref='func__Fault_Evaluate: قانون ۲', side=SIDE('red',['خطای ۶ روشن می‌شود (باتری قطع است)'], 'r', 'بله', retToText='خروجی: پرچم خطای ۶'), elab='نه'),
   N('sec','navy','بخش ۳: بازیابی و خروجی'),
   N('dia','yellow','بازیابی: هر دو باتری بالای ۷ ولت‌اند و باتری در حال شارژ بالای ۱۴٫۸ نیست، ۱ ثانیه ماند؟', ref='func__Fault_Clear', side=SIDE('green',['خطای ۶ پاک می‌شود','شارژر آن کانال را آرام از ۱٪ از نو شروع می‌کند'], 'r', 'بله'), elab='نه'),
   N('box','green','خروجی: پرچم خطای ۶', ['شارژر آن کانال را متوقف می‌کند','دستگاه به حالت خطا می‌رود و چراغ و بوق خطای ۶ می‌آید'], note=SIDE('gray',['شارژر همین وضعیت را می‌خواند','و کانال همان باتری را متوقف می‌کند'],'l'), ref='func__Fault_Set: bit خطای ۶'),
 ],
 'tree': T('بررسی خطاها — هر دور کنترل',
   T('حالت تست دستی؟ → همه‌چیز همان‌جا می‌ایستد'),
   T('عکس نامعتبر؟ → توقف و صفرشدن شمارش'),
   T('باتری‌های نصب‌شده مشخص می‌شوند'),
   T('قانون ۱: در حال شارژ + بالای ۱۴٫۸ (۱۵۰ میلی‌ثانیه) → خطای ۶'),
   T('قانون ۲: ورودی سالم + زیر ۶ ولت (۱ ثانیه) → خطای ۶'),
   T('بازیابی: هر دو باتری ≥ ۷ ولت (۱ ثانیه) → پاک‌شدن خطای ۶'),
   T('خروجی پرچم خطای ۶ به شارژر و حالت کلی')),
 'conds': [
   ['باتری در حال شارژ بالای ۱۴٫۸ ولت پرید','یعنی سیم همان باتری قطع است؛ بعد از ۱۵۰ میلی‌ثانیه خطای ۶ روشن می‌شود.','۱۴٫۸ ولت / ۱۵۰ میلی‌ثانیه (از پنل قابل تنظیم)'],
   ['یک باتری زیر ۶ ولت افتاد و ورودی سالم بود','آن باتری قطع حساب می‌شود؛ بعد از ۱ ثانیه خطای ۶ روشن می‌شود.','۶ ولت / ۱ ثانیه'],
   ['باتری برگشت و هر دو بالای ۷ ولت ایستادند','بعد از ۱ ثانیه پایداری خطا پاک می‌شود.','۷ ولت (یک ولت فاصلهٔ امن) / ۱ ثانیه'],
   ['عکس اندازه‌گیری نامعتبر یا حالت تست دستی','هیچ تصمیمی گرفته نمی‌شود؛ شمارش‌ها متوقف می‌شوند یا صفر می‌شوند.','—'],
 ],
 'outs': [
   ['وضعیت خطای ۶','روشن یا پاک‌شدن خطای قطع باتری','شارژر کانال را متوقف یا آزاد می‌کند؛ دستگاه به حالت خطا می‌رود؛ چراغ قرمز چشمک و بوق می‌آید'],
   ['parameterهای آستانه از پنل','آستانهٔ قطع، نبودن، برگشت و زمان‌های پایداری (شناسه‌های ۲۷ تا ۳۴)','از پنل می‌آید؛ عدد بیرون از بازه در خود ماژول به نزدیک‌ترین حد مجاز برده می‌شود'],
 ],
}

MODULES['imbalance'] = {
 'file': 'imbalance_process.html',
 'title': 'فرآیند عدم‌توازن باتری — توضیح ساده',
 'sub': 'این ماژول اختلاف ولتاژ دو باتری (باتری پایین و باتری بالا) را زیر نظر می‌گیرد؛ اگر اختلافشان زیاد و ماندگار شد یک «رویداد» ثبت می‌کند و اگر تکرار شد، قفل عدم‌توازن می‌زند.',
 'steps': [
   '<b>اختلاف:</b> قدرمطلق اختلاف ولتاژ دو باتری حساب می‌شود؛ فقط وقتی هر دو باتری در یک وضعیت‌اند (هر دو در حال شارژ یا هر دو در حال استراحت) مقایسه معنا دارد.',
   '<b>سه پنجره:</b> هنگام دشارژ فوری با حد <b class="num">۵۰۰ میلی‌ولت</b>؛ حین شارژ و در استراحت با حد <b class="num">۳۰۰ میلی‌ولت</b> و بعد از زمان انتظار خودش. پنجرهٔ دشارژ و استراحت تازه بعد از این‌که اولین شارژ کامل (FLOAT) از روشن‌شدن دیده شد باز می‌شوند.',
   '<b>ماندگاری:</b> اختلاف باید به اندازهٔ زمان پایداری بالای حد بماند تا «نامزد رویداد» شود؛ پرش‌های کوتاه حساب نیستند.',
   '<b>ثبت رویداد:</b> فقط در پایان یک چرخهٔ کامل (شارژی که تا کامل‌شدن رفت) رویداد ثبت می‌شود؛ شارژ نیمه‌کاره نه رویداد حساب می‌شود نه پاک‌شدن.',
   '<b>قفل:</b> وقتی رویدادها به حد قفل (پیش‌فرض <b class="num">۱۰ رویداد</b>) رسیدند، قفل می‌خورد: چراغ قرمز چشمک‌زن و بوق دوره‌ای؛ گرفتن جلوی خروجی و بودجهٔ cycle شارژ هم از همین قفل می‌آید.',
   '<b>پاک‌شدن:</b> ۳ چرخهٔ کاملِ بی‌رویداد فقط شمارندهٔ رویداد را صفر می‌کند؛ تعویض واقعی باتری (۳ ثانیه نبودن باتری) همه‌چیز حتی قفل را پاک می‌کند.',
 ],
 'example': [
   'باتری بالا ۱۲٫۹ ولت و باتری پایین ۱۲٫۴ ولت است؛ اختلافشان ۵۰۰ میلی‌ولت (نیم ولت) است.',
   'دستگاه روی باتری دارد کار می‌کند (دشارژ)؛ حد دشارژ ۵۰۰ است و اختلاف بالای حد می‌ماند.',
   'اگر این cycle تا کامل‌شدن شارژ رفته باشد، یک رویداد ثبت می‌شود؛ با رسیدن رویدادها به ۱۰، قفل عدم‌توازن می‌خورد.',
 ],
 'flow': [
   N('sec','navy','بخش ۱: مقایسهٔ ولتاژ دو باتری'),
   N('pill','navy','شروع بررسی عدم‌توازن (هر دور کنترل)', ref='func__Imbalance_Evaluate'),
   N('dia','yellow','ورودی‌ها معتبرند؟', side=SIDE('gray',['همهٔ timerها می‌ایستند و جلو نمی‌روند','تصمیم تازه‌ای گرفته نمی‌شود'], 'r', 'نه'), elab='بله'),
   N('box','blue','اختلاف ولتاژ دو باتری حساب می‌شود (قدرمطلق)', [], ref='func__Imbalance_Evaluate: اختلاف'),
   N('dia','yellow','هر دو باتری در یک وضعیت‌اند؟', side=SIDE('gray',['دو باتری در یک وضعیت نیستند، پس مقایسه نمی‌شوند','اختلافی که خود شارژر ساخته، خطا حساب نمی‌شود'], 'r', 'نه'), elab='بله'),
   N('box','blue','مشخص می‌شود کدام حد مقایسه فعال است', ['در دشارژ: بی‌درنگ، حد ۵۰۰ میلی‌ولت','در حال شارژ: بعد از زمان انتظار، حد ۳۰۰ میلی‌ولت','در استراحت: بعد از زمان انتظار، حد ۳۰۰ میلی‌ولت','دشارژ و استراحت: فقط بعد از اولین شارژ کامل (وضعیت نگه‌داری FLOAT)'], ref='func__Imbalance_Evaluate: پنجره‌ها'),
   N('sec','navy','بخش ۲: پایداری و رویداد cycle'),
   N('dia','yellow','اختلاف بالای حد پنجره ماند و زمان پایداری پر شد؟', side=SIDE('yellow',['این cycle نامزد رویداد می‌شود','پرش کوتاه حساب نمی‌شود'], 'r', 'بله'), elab='نه'),
   N('dia','yellow','چرخهٔ شارژ با شارژ کامل تمام شد؟', side=SIDE('gray',['نامزد این چرخه کنار گذاشته می‌شود و رویدادی ثبت نمی‌شود','شارژ نیمه‌کاره نه رویداد حساب می‌شود، نه پاک‌شدن'], 'r', 'نه'), elab='بله'),
   N('dia','yellow','در این cycle نامزد عدم‌توازن بود؟', side=SIDE('gray',['نامزدی نبود، پس رویدادی ثبت نمی‌شود','این cycle تمیز حساب می‌شود؛ ۳ cycle تمیز پشت سر هم، شمارندهٔ رویداد را صفر می‌کند'], 'r', 'نه'), elab='بله'),
   N('box','blue','ثبت یک رویداد برای این چرخهٔ شارژ', ['شمارندهٔ رویداد یکی بالا می‌رود'], ref='func__Imbalance_Evaluate: ثبت رویداد', note=SIDE('gray',['رویداد و قفل در NVM می‌مانند','پاک‌شدن کامل: نبود باتری ۳ ثانیه'],'l')),
   N('sec','navy','بخش ۳: قفل و پاک‌شدن'),
   N('dia','yellow','رویدادها به حد قفل رسیدند؟ (پیش‌فرض ۱۰)', side=SIDE('red',['قفل عدم‌توازن می‌خورد','چراغ قرمز چشمک می‌زند و بوق دوره‌ای می‌آید','جلوی خروجی گرفته می‌شود و شارژ بودجهٔ cycle دارد'], 'r', 'بله'), elab='نه'),
   N('box','green','پاک‌شدن‌ها', ['۳ cycle کامل و بدون رویداد: فقط شمارندهٔ رویداد صفر می‌شود','تعویض واقعی باتری (۳ ثانیه نبودن باتری): همه‌چیز صفر می‌شود'], ref='func__Imbalance_Evaluate: پاک‌شدن'),
 ],
 'tree': T('ارزیابی عدم‌توازن — هر pass',
   T('ورودی نامعتبر؟ → timerها می‌ایستند'),
   T('اختلاف قدرمطلق ولتاژ دو باتری'),
   T('هم‌وضعیت بودن دو باتری'),
   T('پنجره‌ها: دشارژ / شارژ / استراحت'),
   T('ماندگاری بالای حد → نامزد رویداد'),
   T('پایان cycle کامل؟ → ثبت رویداد'),
   T('رسیدن به حد قفل → قفل، گرفتن جلوی خروجی و بودجهٔ شارژ'),
   T('پاک‌شدن: ۳ cycle پاک / تعویض باتری')),
 'conds': [
   ['اختلاف دو باتری در دشارژ از ۵۰۰ میلی‌ولت رد شد','اگر ماندگار شود، آن چرخهٔ شارژ نامزد رویداد می‌شود.','حد دشارژ ۵۰۰ میلی‌ولت (شناسهٔ ۱۰۹)'],
   ['اختلاف در شارژ یا استراحت از ۳۰۰ میلی‌ولت رد شد','بعد از زمان انتظار هر پنجره و با ماندگاری، نامزد رویداد می‌شود.','حد ۳۰۰ میلی‌ولت (۱۰۸)؛ انتظارها ۱۱۰ و ۱۱۱؛ پایداری ۱۱۲'],
   ['رویدادها به حد قفل رسیدند','قفل عدم‌توازن: قرمز چشمک‌زن و بوق دوره‌ای؛ جلوی خروجی دستگاه گرفته می‌شود و شارژ فقط به اندازهٔ بودجهٔ cycleهای بعد از قفل اجازه دارد.','پیش‌فرض ۱۰ رویداد (۱۱۴)؛ جلوگیری از خروجی (۱۱۷)؛ بودجه (۱۱۸)'],
   ['۳ چرخهٔ کامل بدون رویداد پشت سر هم آمد','فقط شمارندهٔ رویداد صفر می‌شود؛ قفل سر جایش می‌ماند.','شناسهٔ ۱۳۶، پیش‌فرض ۳ cycle'],
   ['باتری ۳ ثانیه نبود (تعویض واقعی)','شمارنده، قفل و بودجه همه صفر می‌شوند.','۳ ثانیه نبودن باتری'],
 ],
 'outs': [
   ['قفل و وضعیت عدم‌توازن','چراغ قرمز چشمک‌زن و الگوی بوق دوره‌ای (شناسه‌های ۱۱۵، ۱۱۶، ۱۲۳، ۱۲۴، ۱۳۲، ۱۳۳)','ماژول نمایش از همین خروجی چهره می‌سازد'],
   ['گرفتن جلوی خروجی','باتری قفل‌شده روی خروجی دستگاه نمی‌رود (شناسهٔ ۱۱۷)','به تعویض خروجی می‌رود'],
   ['اجازهٔ شارژ با بودجه','تا پرشدن بودجهٔ cycleهای بعد از قفل، شارژ آزاد است؛ بعدش channelها خاموش می‌مانند (۱۱۸)','به شارژر می‌رود'],
   ['حافظهٔ ماندگار','رویدادها، قفل و cycleهای قفل در NVM می‌مانند','با خاموش و روشن‌شدن پاک نمی‌شوند'],
 ],
}


MODULES['changeover'] = {
 'file': 'changeover_process.html',
 'title': 'فرآیند تعویض خروجی — توضیح ساده',
 'sub': 'این ماژول تصمیم می‌گیرد خروجی دستگاه از باتری بیاید یا باتری برای محافظت کنار گذاشته شود؛ و تنها جایی است که خط محافظ باتری را تکان می‌دهد.',
 'steps': [
   '<b>اول اندازه‌گیری:</b> اگر عکس اندازه‌گیری نامعتبر باشد، هیچ تصمیمی گرفته نمی‌شود و حالت فعلی می‌ماند.',
   '<b>خطا:</b> هر bit خطایی که set باشد، حالت کلی «خطا» می‌شود و هیچ pinی تکان نمی‌خورد.',
   '<b>باتری قفل‌شده:</b> باتری‌ای که برای عدم‌توازن قفل شده (با علامت پنل) یا باتری «خراب» قفل‌شده، هیچ‌وقت روی خروجی نمی‌رود؛ با آن مثل قطع بحرانی رفتار می‌شود.',
   '<b>قطع نرم:</b> ولتاژ کل باتری زیر <b class="num">۲۱ ولت</b> قفل باتری کم را می‌بندد؛ فقط با رسیدن به <b class="num">۲۱٫۲ ولت</b> باز می‌شود (hysteresis).',
   '<b>قطع:</b> اگر شرط قطع (قفل باتری کم یا زیر <b class="num">۲۰٫۸ ولت</b>)، <b class="num">۳ ثانیه</b> پیوسته بماند، خط محافظ فعال و خروجی از باتری جدا می‌شود.',
   '<b>وصل دوباره:</b> فقط وقتی ورودی حاضر است، ولتاژ به ۲۱٫۲ رسیده و ورودی <b class="num">۳۰ ثانیه</b> پایدار مانده و بعد هم ۳ ثانیهٔ تأیید گذشت؛ عمداً بدون برق ورودی وصل نمی‌شود تا باتری ضعیف زیر بار برنگردد.',
 ],
 'example': [
   'دستگاه روی باتری کار می‌کند و ولتاژ کل به ۲۰٫۹ ولت می‌رسد؛ قفل باتری کم بسته می‌شود.',
   '۳ ثانیه در همین وضعیت می‌ماند؛ خط محافظ فعال می‌شود و خروجی از باتری جدا می‌شود.',
   'برق ورودی می‌آید و باتری شارژ می‌شود؛ بعد از پایداری ۳۰ ثانیه‌ای ورودی و رسیدن به ۲۱٫۲ ولت و ۳ ثانیه تأیید، خروجی دوباره وصل می‌شود.',
 ],
 'flow': [
   N('sec','navy','بخش ۱: بررسی‌های اول ایمنی'),
   N('pill','navy','شروع ارزیابی تعویض خروجی (هر pass)', ref='func__Changeover_Evaluate'),
   N('dia','yellow','Snapshot معتبر است؟', side=SIDE('gray',['حالت فعلی و خط محافظ دست نمی‌خورد','timer قطع و timer وصل از صفر شروع می‌شوند','timer پایداری ورودی با ساعت سیستم جلو می‌رود؛ نه از اول می‌شود، نه می‌ایستد'], 'r', 'نه'), elab='بله'),
   N('dia','yellow','bit خطایی set است؟', side=SIDE('red',['حالت کلی دستگاه «خطا» می‌شود','هیچ پایه‌ای تکان نمی‌خورد'], 'r', 'بله'), elab='نه'),
   N('dia','yellow','باتری برای خروجی ممنوع است؟ (قفل عدم‌توازن از پنل، یا باتری خراب قفل)', ref='func__Changeover_Evaluate: باتری‌های ممنوع برای خروجی', side=SIDE('red',['خط محافظ فعال و حالت «امن»','این باتری هرگز به خروجی وصل نمی‌شود'], 'r', 'بله'), elab='نه'),
   N('sec','navy','بخش ۲: قفل باتری کم و قطع خروجی'),
   N('box','blue','به‌روزرسانی قفل باتری کم با hysteresis', ['زیر ۲۱ ولت: قفل بسته می‌شود','بالای ۲۱٫۲ ولت: قفل باز می‌شود','بین این دو ولتاژ: قفل همان قبلی می‌ماند'], ref='func__Changeover_Evaluate: قفل باتری کم'),
   N('dia','yellow','شرط قطع ۳ ثانیه پیوسته ماند؟ (قفل باتری کم یا زیر ۲۰٫۸ ولت)', ref='func__Changeover_Evaluate: CutTimer', side=SIDE('red',['خط محافظ فعال؛ خروجی از باتری جدا','حالت «امن» تا وصل دوباره'], 'r', 'بله'), elab='نه'),
   N('sec','navy','بخش ۳: پایداری ورودی و وصل دوباره'),
   N('box','blue','ردیابی پایداری ورودی', ['تا وقتی ورودی حاضر است، پایداری شمرده می‌شود؛ با هر قطع‌شدن، شمارش از صفر شروع می‌شود'], ref='func__Changeover_Evaluate: پایداری ورودی'),
   N('dia','yellow','ورودی برگشت؟', ['ولتاژ بالای ۲۱٫۲ ولت است؟','ورودی ۳۰ ثانیه پایدار مانده و ۳ ثانیه هم تأیید شده؟'], ref='func__Changeover_Evaluate: ReconnectTimer', side=SIDE('green',['خط محافظ آزاد و خروجی وصل','بدون برق ورودی، خروجی عمداً وصل نمی‌شود'], 'r', 'بله'), elab='نه'),
   N('box','green','حالت عادی از روی حضور ورودی', ['ورودی هست: حالت «ورودی»','ورودی نیست: حالت «باتری»؛ اگر محافظ فعال بود «امن» می‌ماند'], note=SIDE('gray',['پایهٔ خروجی فقط همان لحظهٔ قطع یا وصل تکان می‌خورد','بقیهٔ passها خروجی سر جایش می‌ماند'],'l'), ref='func__Changeover_Evaluate: حالت خروجی'),
 ],
 'tree': T('ارزیابی تعویض خروجی — هر pass',
   T('عکس نامعتبر؟ → حفظ حالت'),
   T('خطا؟ → حالت خطا، بدون حرکت pin'),
   T('باتری ممنوع: عدم‌توازن / باتری خراب → محافظ و حالت امن'),
   T('قفل باتری کم: ۲۱ بستن / ۲۱٫۲ بازکردن'),
   T('شرط قطع ۳ ثانیه → محافظ فعال (امن)'),
   T('پایداری ورودی ۳۰ ثانیه'),
   T('وصل دوباره: ورودی + ۲۱٫۲ + ۳ ثانیه → آزاد'),
   T('عادی: ورودی/باتری از روی حضور ورودی')),
 'conds': [
   ['ولتاژ کل باتری زیر ۲۱ ولت رفت','قفل باتری کم بسته می‌شود و شمارش قطع ۳ ثانیه‌ای شروع می‌شود.','۲۱ ولت؛ بازشدن در ۲۱٫۲ ولت'],
   ['ولتاژ زیر ۲۰٫۸ ولت رفت (بحرانی)','بدون صبر برای قفل هم شرط قطع حساب می‌شود؛ بعد از ۳ ثانیه خروجی جدا می‌شود.','۲۰٫۸ ولت / ۳ ثانیه'],
   ['باتری عدم‌توازن یا «خراب» قفل شد','با علامت جلوگیری در پنل، باتری هیچ‌وقت روی خروجی نمی‌رود و خط محافظ فعال می‌ماند.','شناسهٔ جلوگیری عدم‌توازن ۱۱۷؛ باتری خراب ۱۲۷'],
   ['برق ورودی برگشت و باتری شارژ شد','فقط با ۳۰ ثانیه پایداری ورودی و ۲۱٫۲ ولت و ۳ ثانیه تأیید، خروجی دوباره وصل می‌شود.','۳۰ ثانیه + ۳ ثانیه'],
 ],
 'outs': [
   ['خط محافظ باتری (pin منطقی BSP)','در قطع و برای باتری‌های ممنوع فعال می‌شود؛ فقط در وصل دوباره آزاد می‌شود','تنها فرمان سخت‌افزاری این ماژول'],
   ['حالت کلی دستگاه','خطا / امن / ورودی / باتری','شارژر و نمایش از همین حالت پیروی می‌کنند'],
 ],
}

MODULES['ui'] = {
 'file': 'ui_process.html',
 'title': 'فرآیند چراغ‌ها و بوق — توضیح ساده',
 'sub': 'این ماژول از روی وضعیت بقیهٔ ماژول‌ها یک «چهره» برای سه چراغ (سبز، زرد، قرمز) و بوق انتخاب می‌کند؛ همیشه بالاترین اولویت برنده است.',
 'steps': [
   '<b>اولویت خطاها:</b> اول خطای فنی (هر سه چراغ با هم چشمک)، بعد اضافه‌ولتاژ ورودی، بعد قطع باتری (خطای ۶)، بعد باتری خراب، بعد عدم‌توازن قفل‌شده.',
   '<b>خطای ۶:</b> سبز ثابت + قرمز چشمک ۱ ثانیه‌ای با ۵۰٪ روشن و ۳ بوق پشت سر هم؛ همین ترکیب آن را از بقیهٔ خطاها جدا می‌کند.',
   '<b>باتری خراب:</b> قرمز ثابت؛ <b>عدم‌توازن:</b> قرمز چشمک‌زن با بوق دوره‌ای جدا تا دو حکم قاطی نشوند.',
   '<b>حالت عادی با ورودی:</b> شارژ کامل = سبز ثابت و زرد خاموش؛ در حال شارژ = زرد چشمک‌زن؛ استراحت حرارتی = زرد چشمک ۱ ثانیه‌ای با سبز ثابت؛ ورودی سالم بدون شارژ = سبز ثابت.',
   '<b>روی باتری (بدون ورودی):</b> درصد باتری از روی ولتاژ خوانده می‌شود؛ سبز با ریتم درصد چشمک می‌زند و در درصدهای پایین بوق هشدار می‌آید (۴۰٪، ۲۰٪، ۱۰٪ و بحرانی).',
   '<b>دو جدول درصد:</b> جدول درصد هنگام شارژ و هنگام کار روی باتری جدا هستند تا حد «کامل» با حد مصرف قاطی نشود.',
   '<b>صفحهٔ هر آلارم:</b> هر خطا و هر حالت یک صفحهٔ جدا دارد (فلوچارت، چراغ، بوق، عددها و راه رفع). از «فهرست خطاها و آلارم‌ها» در بالای همین صفحه باز کن.',
 ],
 'example': [
   'برق ورودی وصل است و شارژر در حال شارژ است: زرد چشمک می‌زند و سبز ثابت است.',
   'شارژ کامل می‌شود: چشمک زرد کاملاً می‌ایستد و فقط سبز ثابت می‌ماند.',
   'وسط شارژ سیم باتری کشیده می‌شود: چهره فوری عوض می‌شود؛ قرمز چشمک‌زن و ۳ بوق (خطای ۶).',
 ],
 'flow': [
   N('sec','navy','بخش ۱: اولویت خطاها — اولین برنده نمایش داده می‌شود'),
   N('pill','navy','شروع pass نمایش (هر pass)', ref='func__Ui_Tick'),
   N('dia','yellow','Snapshot معتبر است؟', side=SIDE('gray',['فازهای چشمک از اول شروع می‌شوند','درصد پایدار قبلی نگه داشته می‌شود'], 'r', 'نه'), elab='بله'),
   N('box','blue','وضعیت ورودی با hysteresis خوانده می‌شود', ['حضور ورودی و اضافه‌ولتاژ ورودی (بالای ۲۸ ولت)'], ref='func__Ui_UpdateInputState'),
   N('dia','yellow','خطای فنی (۷) set است؟', ref='func__Ui_ScenarioTechnicalFault_Tick', side=SIDE('red',['هر سه چراغ هم‌فاز چشمک می‌زنند + بوق','بالاترین اولویت؛ فقط با reset پاک می‌شود'], 'r', 'بله'), elab='نه', note=SIDE('red',['بالاترین اولویت نمایش است','حتی از اضافه‌ولتاژ هم جلوتر است'],'l')),
   N('dia','yellow','اضافه‌ولتاژ ورودی هست؟', side=SIDE('red',['چهرهٔ اضافه‌ولتاژ (چشمک + بوق دوره‌ای)'], 'r', 'بله'), elab='نه'),
   N('dia','yellow','خطای ۶ (قطع باتری) set است؟', side=SIDE('red',['قرمز چشمک ۱ ثانیه‌ای ۵۰٪ + ۳ بوق'], 'r', 'بله'), elab='نه'),
   N('dia','yellow','باتری «خراب» قفل شده؟', side=SIDE('red',['قرمز ثابت تا تعویض باتری'], 'r', 'بله'), elab='نه'),
   N('dia','yellow','عدم‌توازن قفل شده؟', side=SIDE('red',['فقط قرمز چشمک‌زن + بوق دوره‌ای جدا'], 'r', 'بله'), elab='نه'),
   N('sec','navy','بخش ۲: نمایش عادی — وقتی خطایی نیست'),
   N('box','blue','درصد باتری از جدول شارژ', ['شارژ کامل یعنی یا شارژر بگوید کامل است، یا درصد به ۱۰۰ برسد و بالای ۹۵ بماند'], ref='func__Ui_UpdateChargingFullHysteresis'),
   N('dia','yellow','ورودی حاضر است؟', side=SIDE('blue',['چهرهٔ کار روی باتری: سبز با ریتم درصد باتری چشمک می‌زند','بوق هشدار در ۴۰٪، ۲۰٪، ۱۰٪ و بحرانی'], 'r', 'نه'), elab='بله'),
   N('dia','yellow','شارژ کامل است؟', side=SIDE('green',['سبز ثابت و زرد کاملاً خاموش'], 'r', 'بله'), elab='نه', ref='func__Ui_UpdateChargingFullHysteresis'),
   N('dia','yellow','استراحت حرارتی فعال است؟', side=SIDE('yellow',['زرد چشمک ۱ ثانیه‌ای ۵۰٪ با سبز ثابت'], 'r', 'بله'), elab='نه'),
   N('dia','yellow','channelی در حال شارژ است؟', side=SIDE('blue',['چهرهٔ شارژ؛ زرد چشمک‌زن با حداقل روشنایی'], 'r', 'بله'), elab='نه'),
   N('box','green','ورودی سالم: سبز ثابت', ['channelی در حال شارژ نیست و خطایی هم نیست'], ref='func__Ui_ScenarioInputOk'),
 ],
 'tree': T('نمایش چراغ و بوق — هر pass',
   T('عکس نامعتبر؟ → reset فازها'),
   T('وضعیت ورودی با hysteresis'),
   T('اولویت چهره‌ها',
     T('۱: خطای فنی — هر سه هم‌فاز'),
     T('۲: اضافه‌ولتاژ ورودی'),
     T('۳: خطای ۶ — قرمز چشمک + ۳ بوق'),
     T('۴: باتری خراب — قرمز ثابت'),
     T('۵: عدم‌توازن — قرمز چشمک‌زن')),
   T('عادی با ورودی',
     T('کامل: سبز ثابت، زرد خاموش'),
     T('استراحت حرارتی: زرد چشمک'),
     T('شارژ: زرد چشمک‌زن'),
     T('بدون شارژ: سبز ثابت')),
   T('روی باتری: درصد + چشمک سبز + بوق‌ها')),
 'conds': [
   ['خطای فنی (۷) set شد','هر سه چراغ با یک فاز چشمک می‌زنند و بوق می‌آید؛ بالاتر از همهٔ چهره‌ها.','دوره ۱ ثانیه، ۵۰٪'],
   ['سیم باتری کشیده شد (خطای ۶)','قرمز چشمک‌زن (۱ ثانیه، ۵۰٪) و ۳ بوق پشت سر هم.','۳ بوق، دورهٔ تکرار ۳ ثانیه'],
   ['شارژ کامل شد','سبز ثابت می‌ماند و چشمک زرد کاملاً خاموش می‌شود؛ تا وقتی channelی در حال شارژ است، چهرهٔ شارژ می‌ماند.','کامل از گفتهٔ شارژر، یا درصد ۱۰۰ (و ماندن بالای ۹۵)'],
   ['دستگاه روی باتری است و درصد پایین آمد','در ۴۰٪، ۲۰٪ و ۱۰٪ بوق‌های هشدار پله‌ای و در حد بحرانی بوق بحرانی می‌آید.','آستانه‌های درصد از پنل (جدول کار روی باتری)'],
 ],
 'outs': [
   ['سه چراغ سبز، زرد، قرمز (BSP)','چهرهٔ انتخاب‌شده با ریتم و فاز خودش','در هر pass تازه می‌شود'],
   ['بوق (BSP)','الگوهای بوق هر چهره: ۳ بوق خطای ۶، بوق دوره‌ای عدم‌توازن، هشدارهای درصدی','همراه چهرهٔ فعال'],
   ['خودآزمایی روشن‌شدن','هر سه چراغ در شروع برای اطمینان از سالم‌بودنشان روشن می‌شوند','۵۰۰ میلی‌ثانیه در راه‌اندازی'],
 ],
}

MODULES['protection'] = {
 'file': 'protection_process.html',
 'title': 'فرآیند ناظر حفاظت — توضیح ساده',
 'sub': 'ناظر سبک برنامه: تنها کارش این است که تا وقتی اندازه‌گیری معتبر نیست، خطای «مبدل» (bit ۰) را نگه دارد. این ماژول فعلاً در ساخت اصلی خاموش است.',
 'steps': [
   'اگر عکس اندازه‌گیری نباشد یا نامعتبر باشد، bit خطای مبدل <b>set</b> می‌شود.',
   'با اولین عکس معتبر، همان bit <b>پاک</b> می‌شود.',
   'این خطا عمداً <b>قفل نمی‌شود</b>: لحظه‌ای است؛ چون passهای اول روشن‌شدن همیشه قبل از آماده‌شدن اندازه‌گیری اجرا می‌شوند و خطای قفل‌شده دستگاه را برای همیشه در حالت امن نگه می‌داشت.',
   'نظارت اضافه‌جریان و باتری ضعیف این‌جا نیست؛ در شارژر و ماژول خطا انجام می‌شود.',
   'ماژول با پیش‌فرض خاموش ساخته می‌شود (فقط با روشن‌کردن آگاهانه در پیکربندی فعال می‌شود).',
 ],
 'flow': [
   N('sec','navy','بخش ۱: بررسی ناظر — فعلاً در نسخهٔ ساخته‌شده خاموش است'),
   N('pill','navy','شروع pass ناظر', ref='func__Protection_Run'),
   N('dia','yellow','Snapshot معتبر است؟', side=SIDE('green',['پاک‌کردن bit خطای مبدل (bit ۰)'], 'r', 'بله'), elab='نه'),
   N('box','red','set کردن FAULT_ADC', ['لحظه‌ای است و قفل نمی‌شود','با اولین عکس معتبر خودش پاک می‌شود'], ref='func__Fault_Set(FAULT_ADC)', note=SIDE('gray',['تنها ماژول خاموش build است','MODULE_PROTECTION=0'],'l')),
 ],
 'tree': T('ناظر حفاظت',
   T('عکس نامعتبر؟ → set bit مبدل'),
   T('عکس معتبر؟ → پاک bit مبدل'),
   T('قفل ندارد؛ ماژول پیش‌فرض خاموش است')),
 'conds': [
   ['اندازه‌گیری نامعتبر شد','bit خطای مبدل set می‌شود؛ دستگاه از این bit مثل بقیهٔ خطاها به حالت خطا می‌رود.','bit ۰'],
   ['اندازه‌گیری معتبر شد','bit پاک می‌شود و کار عادی ادامه دارد.','بدون قفل'],
 ],
 'outs': [
   ['وضعیت خطای مبدل','آمدن و پاک‌شدن لحظه‌ای خطا','به ماژول خطا می‌رود و از آن‌جا حالت کلی و شارژر پیروی می‌کنند'],
 ],
}


MODULES['callut'] = {
 'file': 'callut_process.html',
 'title': 'فرآیند تبدیل جریان با جدول تصحیح — توضیح ساده',
 'sub': 'این ماژول جدول تبدیل جریان باتری را نگه می‌دارد: با چند نقطهٔ اندازه‌گیری‌شده روی میز، شمارش مبدل به میلی‌آمپر واقعی تبدیل می‌شود. ولتاژها از این جدول نیستند و با فرمول مقسم حساب می‌شوند.',
 'steps': [
   'برای جریان هر channel یک جدول جدا از نقطه‌ها هست: شمارش مبدل ← جریان واقعی (میلی‌آمپر).',
   'بین دو نقطه، مقدار با <b>خط راست بین همان دو نقطه</b> حساب می‌شود (درون‌یابی خطی)؛ پایین‌تر از اولین نقطه مقدار همان نقطه می‌ماند و بالاتر از آخرین نقطه، خط سادهٔ بهره و جبران خطای صفر ادامه پیدا می‌کند.',
   'جدول‌ها از پنل قابل خواندن و نوشتن‌اند؛ قبل از قبول‌شدن، جدول تازه کامل بررسی می‌شود (ترتیب نقطه‌ها، تعداد و بازه‌ها).',
   'اگر record جدول خراب باشد یا اصلاً نباشد، جدول خاموش می‌ماند و تبدیل جریان با خط سادهٔ بهره و جبران خطای صفر انجام می‌شود؛ جدول آمادهٔ کارخانه وجود ندارد.',
   'همین جدول‌ها هستند که اختلاف واقعی قطعات هر برد را جبران می‌کنند؛ برای همین هر برد جدول خودش را دارد.',
 ],
 'example': [
   'روی میز با منبع دقیق، در شمارش ۱۰۰۰ جریان ۲۰۰ میلی‌آمپر و در شمارش ۲۰۰۰ جریان ۴۰۰ میلی‌آمپر خوانده شده است.',
   'حالا شمارش ۱۵۰۰ می‌آید؛ جدول دقیقاً وسط دو نقطه را می‌دهد: ۳۰۰ میلی‌آمپر.',
   'اگر شمارش ۲۵۰۰ بیاید (بالاتر از آخرین نقطه)، مقدار روی آخرین نقطه نمی‌ایستد و با خط سادهٔ بهره و جبران خطای صفر جلو می‌رود.',
 ],
 'flow': [
   N('sec','navy','بخش ۱: تبدیل با جدول فعلی'),
   N('pill','navy','درخواست تبدیل یک شمارش'),
   N('box','blue','جدول جریانِ همان channel انتخاب می‌شود', ['فقط جریان جدول دارد؛ ولتاژ با فرمول مقسم است'], ref='func__CalLut_Active'),
   N('dia','yellow','شمارش داخل بازهٔ جدول است؟', side=SIDE('gray',['پایین‌تر از اولین نقطه: مقدار همان اولین نقطه','بالاتر از آخرین نقطه: با خط سادهٔ بهره و جبران خطای صفر ادامه پیدا می‌کند'], 'r', 'نه'), elab='بله'),
   N('box','blue','دو نقطهٔ دورِ شمارش پیدا می‌شود', [], ref='func__CalLut_Points'),
   N('box','green','درون‌یابی خطی بین دو نقطه', ['خروجی: میلی‌آمپر واقعی'], ref='func__CalLut_Counts + func__CalLut_BatteryMa', note=SIDE('gray',['جدول در RAM است و از NVM می‌آید','از پنل هم قابل نوشتن است'],'l')),
   N('sec','navy','بخش ۲: نوشتن جدول تازه از پنل'),
   N('dia','yellow','جدول تازه کامل و درست است؟ (ترتیب، تعداد، بازه)', side=SIDE('red',['قبول نمی‌شود و جدول قبلی می‌ماند'], 'r', 'نه'), elab='بله'),
   N('box','green','نشستن جدول تازه و ذخیره در حافظهٔ ماندگار', ['اگر جدول ذخیره‌شده خراب بود، جدول خاموش و خط سادهٔ بهره و جبران خطای صفر فعال می‌شود'], ref='func__CalLut_Commit'),
 ],
 'tree': T('جدول تصحیح جریان',
   T('تبدیل: جست‌وجوی دو نقطه + درون‌یابی خطی'),
   T('پایین‌تر از جدول → اولین نقطه؛ بالاتر → خط سادهٔ بهره و جبران خطای صفر'),
   T('نوشتن از پنل → بررسی کامل → قبول یا رد'),
   T('record خراب یا غایب → خط سادهٔ بهره و جبران خطای صفر')),
 'conds': [
   ['شمارش بیرون از جدول بود','پایین‌تر از اولین نقطه، مقدار همان نقطه برمی‌گردد؛ بالاتر از آخرین نقطه، خط سادهٔ بهره و جبران خطای صفر ادامه پیدا می‌کند.','—'],
   ['جدول تازه از پنل ناقص یا به‌هم‌ریخته بود','قبول نمی‌شود و جدول قبلی سر جایش می‌ماند.','بررسی ترتیب و تعداد نقطه‌ها'],
   ['جدول ذخیره‌شده خراب بود','جدول خاموش می‌ماند و تبدیل جریان با خط سادهٔ بهره و جبران خطای صفر انجام می‌شود.','—'],
 ],
 'outs': [
   ['جریان واقعی (میلی‌آمپر)','تبدیل شمارش جریان برای ماژول اندازه‌گیری','در هر pass اندازه‌گیری'],
   ['جدول‌ها در حافظهٔ ماندگار','خواندن و نوشتن از پنل با بررسی کامل','ذخیره با record معتبر و نسخه'],
 ],
}

MODULES['esplink'] = {
 'file': 'esplink_process.html',
 'title': 'فرآیند link پنل (ESP) — توضیح ساده',
 'sub': 'این ماژول زبان صحبت میکرو با پنل ESP است: frameهای نسخه‌دار با کنترل خطای قوی، و ۱۴۳ شناسهٔ parameter که پنل می‌خواند و می‌نویسد.',
 'steps': [
   '<b>قاب frame:</b> هر پیام با دو byte شروع، نسخه، نوع، طول و در پایان CRC-16 می‌آید؛ frame خراب یا بی‌نسخه دور ریخته می‌شود.',
   '<b>خواندن:</b> پنل مقدار شناسه‌ها را می‌خواهد؛ چون همه در یک frame جا نمی‌شوند، پاسخ در چند تکه فرستاده می‌شود و پنل خودش مقایسه می‌کند.',
   '<b>نوشتن:</b> هر نوشتن اول به ماژول صاحب آن شناسه می‌رود؛ همان ماژول عدد را در بازهٔ امن خودش جا می‌دهد و مقدار اعمال‌شده را برمی‌گرداند. عدد بیرون از بازه رد نمی‌شود؛ به نزدیک‌ترین حد مجاز برده و همان انجام می‌شود. ردشدن فقط برای شناسهٔ ناشناخته است.',
   '<b>ترکیب چند عدد:</b> جوربودن چند عدد با هم (مثل دو درصد شارژ) را خود پنل قبل از فرستادن بررسی می‌کند؛ داخل میکرو برای ترکیب عددها شرط جداگانه‌ای نیست و هر عدد جدا در بازهٔ خودش جا داده می‌شود.',
   '<b>فرمان‌ها:</b> mode test دستی شارژر و خاموش/روشن هر channel شارژ از همین راه می‌آیند؛ شارژر در task خودش آن‌ها را اجرا می‌کند، نه وسط link. ذخیره فرمان جدا ندارد و خودکار است.',
   '<b>ذخیره:</b> تنظیمات ماندگار در record نسخه‌دار NVM می‌نشینند و با خاموش و روشن‌شدن می‌مانند.',
 ],
 'example': [
   'از پنل سقف شارژ کامل را از ۱۴٫۴ به ۱۴٫۲ ولت تغییر می‌دهی.',
   'frame با CRC درست می‌رسد؛ شناسه به ماژول شارژر می‌رود؛ شارژر بازه را check می‌کند (بین ۱۱ تا ۱۴٫۶) و قبول می‌کند.',
   'از آن لحظه شارژر با سقف تازه کار می‌کند و مقدار تازه در حافظهٔ ماندگار هم می‌نشیند.',
 ],
 'flow': [
   N('sec','navy','بخش ۱: دریافت و اعتبار frame'),
   N('pill','navy','رسیدن byteها از UART', ref='func__EspLink_Run'),
   N('dia','yellow','frame کامل و CRC درست است؟', ref='func__EspLink_Run: بررسی CRC-16', side=SIDE('red',['این frame دور ریخته می‌شود و جوابی ندارد','noise خط هیچ اثری روی برنامه ندارد'], 'r', 'نه'), elab='بله'),
   N('dia','yellow','نسخه و نوع frame شناخته‌شده است؟', side=SIDE('red',['این frame نادیده گرفته می‌شود'], 'r', 'نه'), elab='بله'),
   N('dia','yellow','درخواست نوشتن است؟', side=SIDE('blue',['خواندن است: مقدار شناسه‌ها از ماژول صاحب جمع می‌شود','جواب در چند frame، تکه‌تکه فرستاده می‌شود؛ پنل مقدارها را خودش مقایسه می‌کند'], 'r', 'نه'), note=SIDE('gray',['frameهای دیگری هم هستند: هم‌گام‌سازی زمان، درخواست ثبت سابقه و جدول تصحیح جریان','هرکدام همین‌جا راه خودشان را می‌روند'],'l'), elab='بله'),
   N('sec','navy','بخش ۲: خواندن و نوشتن'),
   N('box','blue','نوشتن: فرستادن به ماژول صاحب شناسه', ['هر ماژول مقدار را در بازهٔ خودش گیره می‌کند'], ref='func__EspLink_ApplyParam: فرستادن به ماژول صاحب'),
   N('dia','yellow','مقدار داخل بازهٔ مجاز بود؟', side=SIDE('red',['عدد به نزدیک‌ترین حد بازه برده و همان انجام می‌شود','مقدار اعمال‌شده به پنل گزارش می‌شود','ردشدن فقط برای شناسهٔ ناشناخته است'], 'r', 'نه'), elab='بله'),
   N('sec','navy','بخش ۳: اعمال و پاسخ'),
   N('box','green','اعمال مقدار + ذخیرهٔ ماندگار (اگر لازم است)', ['فرمان‌هایی مثل mode دستی به شارژر سپرده می‌شوند تا در task خودش اجرا کند'], note=SIDE('gray',['فرستادن دوره‌ای وضعیت‌ها جدا انجام می‌شود','ربطی به پاسخ همین frame ندارد'],'l'), ref='func__EspLink_ApplyParam + NVM'),
 ],
 'tree': T('link پنل — هر pass ارتباط',
   T('مونتاژ frame و بررسی CRC-16'),
   T('نسخه/نوع ناشناخته → رد'),
   T('خواندن: پاسخ تکه‌ای ۱۴۳ شناسه'),
   T('نوشتن: اعتبارسنجی در ماژول صاحب',
     T('بازه بد → گیره به نزدیک‌ترین حد بازه'),
     T('ترکیب چند عدد → check با خود پنل')),
   T('فرمان‌ها: mode دستی و channelها'),
   T('ذخیرهٔ NVM نسخه‌دار')),
 'conds': [
   ['frame خراب یا CRC نادرست رسید','frame کامل دور ریخته می‌شود؛ هیچ مقداری عوض نمی‌شود.','CRC-16'],
   ['پنل عدد بیرون از بازه نوشت','ماژول صاحب، عدد را به نزدیک‌ترین حد بازه می‌برد، همان را انجام می‌دهد و به پنل گزارش می‌کند؛ مقدار قبلی نمی‌ماند.','بازهٔ هر شناسه در ماژول خودش'],
   ['ترکیب چند درصد شارژ با هم ناسازگار شد','خود پنل پیش از فرستادن ترکیب را اصلاح می‌کند؛ میکرو سد ترکیبی ندارد و موقع ذخیره هم جلوی ترکیب را نمی‌گیرد.','check ترکیب با پنل'],
   ['link وسط mode دستی قطع شد','شارژر بعد از ۳ ثانیه سکوت خودش از mode دستی بیرون می‌آید.','۳ ثانیه سکوت link'],
 ],
 'outs': [
   ['مقادیر زنده به پنل','وضعیت‌ها، اندازه‌گیری‌ها، وضعیت شارژ و خطاها','در پاسخ به خواندن پنل'],
   ['مقادیر و فرمان‌ها به ماژول‌ها','تنظیم parameter، mode دستی، خاموش/روشن channelها','بعد از جاافتادن عدد در بازهٔ ماژول صاحب'],
   ['record NVM','ذخیرهٔ ماندگار تنظیمات با نسخه و CRC','خودکار، با کمی تأخیر بعد از تغییر ماندگار'],
 ],
}

MODULES['mcupowerpath'] = {
 'file': 'mcupowerpath_process.html',
 'title': 'فرآیند مسیر تغذیهٔ میکرو — توضیح ساده',
 'sub': 'این ماژول تصمیم می‌گیرد خودِ میکرو از باتری تغذیه شود یا از برق ورودی؛ تا وقتی برق هست باتری بی‌جهت برای مغز دستگاه خالی نشود.',
 'steps': [
   '<b>شروع:</b> میکرو از باتری راه می‌افتد؛ باتری وصل است و مدار آماده است.',
   '<b>رفتن روی ورودی:</b> اگر ولتاژ ورودی دست‌کم <b class="num">۲۲ ولت</b> شد و <b class="num">۵ ثانیه</b> پیوسته ماند، مسیر باتری میکرو قطع و میکرو از ورودی تغذیه می‌شود.',
   '<b>نوار مرده:</b> بین <b class="num">۲۱٫۵ تا ۲۲ ولت</b> هیچ تغییری نمی‌شود و timer هم لغو می‌شود تا مسیر چپ و راست نپرد.',
   '<b>برگشت به باتری:</b> اگر ورودی زیر <b class="num">۲۱٫۵ ولت</b> افتاد، برگشت به باتری فوری است.',
   '<b>قطع ناگهانی ورودی:</b> لبهٔ افتادن signal حضور ورودی همان لحظه در وقفه مسیر باتری را برمی‌گرداند؛ منتظر دور بعدی نمی‌ماند.',
   'اگر اندازه‌گیری نامعتبر باشد، مسیر فعلی حفظ و timer لغو می‌شود.',
 ],
 'example': [
   'دستگاه روی باتری روشن است؛ برق ورودی وصل می‌شود و روی ۲۴ ولت می‌ایستد.',
   '۵ ثانیه می‌گذرد و ورودی پایدار می‌ماند؛ مسیر باتری میکرو قطع می‌شود و میکرو از ورودی کار می‌کند.',
   'برق می‌رود؛ همان لحظه با لبهٔ signal، میکرو دوباره از باتری تغذیه می‌شود و دستگاه بی‌وقفه ادامه می‌دهد.',
 ],
 'flow': [
   N('sec','navy','بخش ۱: تشخیص و رفتن روی ورودی'),
   N('pill','navy','شروع: میکرو از باتری تغذیه می‌شود', ref='func__McuPowerPath_Run'),
   N('dia','yellow','لبهٔ افتادن حضور ورودی آمد؟ (وقفه)', ref='func__McuPowerPath_OnInputIrq', side=SIDE('red',['برگشت فوری به باتری، همان لحظه در وقفه','منتظر دور بعدی نمی‌ماند'], 'r', 'بله'), elab='نه', note=SIDE('red',['برگشت با وقفه، فوری انجام می‌شود','حتی وسط pass کنترل هم صبر نمی‌کند'],'l')),
   N('dia','yellow','Snapshot معتبر است؟', side=SIDE('gray',['مسیر فعلی حفظ و timer لغو می‌شود'], 'r', 'نه'), elab='بله'),
   N('dia','yellow','ولتاژ ورودی دست‌کم ۲۲ ولت است؟', side=SIDE('gray',['ورودی کافی نیست: روی ورودی نمی‌رویم','در نوار مردهٔ ۲۱٫۵ تا ۲۲ ولت، وضعیت فعلی می‌ماند و timer لغو می‌شود'], 'r', 'نه'), elab='بله'),
   N('dia','yellow','۵ ثانیه پیوسته بالای ۲۲ ماند؟', ref='func__McuPowerPath_Run: timer احراز', side=SIDE('gray',['هنوز روی باتری می‌مانیم','افتادن زیر ۲۲ timer را از اول می‌کند'], 'r', 'نه'), elab='بله'),
   N('box','blue','قطع مسیر باتری میکرو', ['میکرو از برق ورودی تغذیه می‌شود','باتری دیگر برای تغذیهٔ میکرو خالی نمی‌شود'], ref='func__McuPowerPath_Run: قطع مسیر باتری'),
   N('sec','navy','بخش ۲: ماندن و برگشت به باتری'),
   N('dia','yellow','ورودی زیر ۲۱٫۵ ولت افتاد؟', side=SIDE('green',['برگشت فوری به باتری'], 'r', 'بله'), elab='نه'),
   N('box','green','ماندن روی ورودی', ['در نوار ۲۱٫۵ تا ۲۲ ولت هیچ تغییری داده نمی‌شود'], ref='func__McuPowerPath_Run'),
 ],
 'tree': T('مسیر تغذیهٔ میکرو — هر pass',
   T('وقفه: لبهٔ افتادن ورودی → باتری فوری'),
   T('عکس نامعتبر؟ → حفظ مسیر، لغو timer'),
   T('بالای ۲۲ ولت × ۵ ثانیه → روی ورودی'),
   T('نوار مرده ۲۱٫۵ تا ۲۲ → بدون تغییر'),
   T('زیر ۲۱٫۵ ولت → برگشت به باتری')),
 'conds': [
   ['ورودی ۲۲ ولت شد ولی قبل از ۵ ثانیه افتاد','timer لغو می‌شود و میکرو روی باتری می‌ماند.','۵ ثانیه پایداری'],
   ['ورودی بین ۲۱٫۵ و ۲۲ ولت ماند','مسیر عوض نمی‌شود؛ وضعیت قبلی می‌ماند تا تغذیه چپ و راست نپرد.','نوار مرده ۵۰۰ میلی‌ولت'],
   ['برق ورودی ناگهان قطع شد','برگشت به باتری در همان وقفه انجام می‌شود.','بدون انتظار دور بعدی'],
 ],
 'outs': [
   ['pin مسیر تغذیه (BSP)','قطع مسیر باتری وقتی روی ورودی هستیم؛ وصل باتری در برگشت','تنها فرمان سخت‌افزاری این ماژول'],
   ['وضعیت مسیر فعلی','روی ورودی هستیم یا روی باتری','برای پنل و Diag خوانده می‌شود'],
 ],
}

MODULES['jitter'] = {
 'file': 'jitter_process.html',
 'title': 'فرآیند Jitter (trip سخت‌افزاری) — توضیح ساده',
 'sub': 'Jitter نگهبان سخت‌افزاری اضافه‌جریان است: comparator روی برد در چند میکروثانیه trip می‌دهد و این ماژول آن را برای هر channel قفل می‌کند تا شارژر ببیند.',
 'steps': [
   'comparator سخت‌افزاری هر channel جدا دیده‌بانی می‌کند؛ وقتی جریان از حد سخت‌افزار رد شود، وقفه می‌آید.',
   'این ماژول برای همان channel یک پرچم «trip» قفل می‌کند؛ پرچم تا وقتی شارژر آن را پاک نکند می‌ماند.',
   'شارژر پرچم را می‌خواند: channel را همان لحظه متوقف می‌کند، بعد از ۳ ثانیه یکی‌یکی احیا می‌کند و در احیا پرچم را پاک می‌کند.',
   'شمارش trip و trip سوم و قطع نهایی کار شارژر است، نه این ماژول؛ این‌جا فقط دیدن و قفل‌کردن رویداد است.',
 ],
 'example': [
   'جریان channel ۱ ناگهان از حد comparator رد می‌شود؛ وقفه می‌آید و پرچم trip channel ۱ قفل می‌شود.',
   'شارژر در دور بعدی پرچم را می‌بیند، channel ۱ را متوقف می‌کند و channel ۲ به کارش ادامه می‌دهد.',
   'بعد از ۳ ثانیه نوبت احیای channel ۱ می‌رسد؛ شارژر پرچم را پاک و با درصد توان کمتر شروع می‌کند.',
 ],
 'flow': [
   N('sec','navy','بخش ۱: مسیر trip — از comparator تا شارژر'),
   N('pill','navy','comparator سخت‌افزاری هر channel آماده است', ref='func__Jitter_Run + وقفهٔ BSP'),
   N('dia','yellow','وقفهٔ trip channelی آمد؟', side=SIDE('gray',['tripی نیامده: پرچمی نیست و این pass کاری نیست'], 'r', 'نه'), elab='بله'),
   N('box','red','قفل پرچم trip همان channel', ['پرچم تا پاک‌شدن توسط شارژر می‌ماند','channel دیگر دست نمی‌خورد'], ref='func__Jitter_Run', note=SIDE('gray',['وقفه فقط پرچم می‌گذارد','کار اصلی در pass کنترل شارژر است'],'l')),
   N('box','blue','شارژر پرچم را می‌خواند', ['channel فوری می‌ایستد، trip شمرده می‌شود و احیا یکی‌یکی انجام می‌شود'], ref='func__Jitter_Run: خواندن پرچم'),
   N('box','green','پاک‌شدن پرچم هنگام احیا', ['شروع دوباره با درصد توان کمتر از قبل trip'], ref='func__Jitter_Run: پاک‌کردن'),
 ],
 'tree': T('Jitter — دیده‌بان سخت‌افزاری',
   T('وقفهٔ comparator channel ۱ → پرچم ۱'),
   T('وقفهٔ comparator channel ۲ → پرچم ۲'),
   T('خواندن پرچم توسط شارژر'),
   T('پاک‌کردن پرچم هنگام احیای شارژر')),
 'conds': [
   ['جریان از حد comparator رد شد','پرچم همان channel قفل می‌شود؛ شارژر channel را متوقف می‌کند.','حد سخت‌افزاری روی برد'],
   ['trip وسط احیا دوباره آمد','پرچم دوباره قفل می‌شود و شمارش trip شارژر بالا می‌رود؛ سومی قطع نهایی است.','قفل ۳ ثانیه و احیای یکی‌یکی در شارژر'],
 ],
 'outs': [
   ['پرچم trip هر channel','قفل‌شده تا پاک‌شدن','شارژر می‌خواند و پاک می‌کند؛ خطای فنی هم از همین پرچم استفاده می‌کند'],
 ],
}

MODULES['diag'] = {
 'file': 'diag_process.html',
 'title': 'فرآیند ثبت سابقه — توضیح ساده',
 'sub': 'این ماژول دفتر ثبت دستگاه است: خطاها، خاموش و روشن‌شدن‌ها، زمان شارژ هر channel و کمینه/بیشینه‌ها را در حافظهٔ ماندگار flash نگه می‌دارد.',
 'steps': [
   '<b>ثبت خطا:</b> هر خطایی که set می‌شود با زمان و عکس اندازه‌گیری همان لحظه ثبت می‌شود؛ دفتر خطا ۱۰ جای آخر را نگه می‌دارد و قدیمی‌ترین می‌رود.',
   '<b>رویدادهای برق:</b> خاموش و روشن‌شدن‌ها جدا ثبت می‌شوند (۲۰ رویداد آخر).',
   '<b>زمان شارژ:</b> ثانیه‌های شارژ هر channel جمع می‌شود و کامل‌شدن شارژ هر channel هم شمرده می‌شود.',
   '<b>کمینه/بیشینه:</b> کمترین و بیشترین ولتاژها و جریان‌های دیده‌شده نگه داشته می‌شود.',
   '<b>ذخیرهٔ امن:</b> record در دو صفحهٔ flash با شماره و نسخه نوشته می‌شود تا اگر وسط نوشتن برق رفت، نسخهٔ قبلی سالم بماند؛ ساعت برد هم با زمان پنل میزان می‌شود.',
 ],
 'example': [
   'خطای ۶ وسط شارژ می‌آید؛ همان لحظه با زمان و ولتاژهای آن لحظه در دفتر ثبت می‌شود.',
   'دستگاه خاموش و روشن می‌شود؛ رویداد برق هم ثبت می‌شود و دفتر بعد از روشن‌شدن سر جایش هست.',
   'از پنل سابقه خوانده می‌شود و معلوم است خطا کی و در چه ولتاژی آمده است.',
 ],
 'flow': [
   N('sec','navy','بخش ۱: رویدادها و آمار'),
   N('pill','navy','دفتر ثبت آماده می‌شود (record سالم از flash خوانده می‌شود)', ref='func__Diag_Init'),
   N('dia','yellow','خطایی set شد؟', ref='func__Diag_RecordFault', side=SIDE('red',['خطا با زمان و عکس همان لحظه ثبت می‌شود','دفتر فقط ۱۰ خطای آخر را نگه می‌دارد؛ قدیمی‌ترین حذف می‌شود'], 'r', 'بله'), elab='نه'),
   N('dia','yellow','خاموش یا روشن‌شدن بود؟', side=SIDE('blue',['ثبت رویداد برق','۲۰ رویداد آخر نگه داشته می‌شود'], 'r', 'بله'), elab='نه'),
   N('box','blue','جمع‌کردن زمان شارژ و کامل‌شدن هر channel', [], ref='func__Diag_AddChargeSec'),
   N('box','blue','به‌روزرسانی کمینه و بیشینه‌ها', ['ولتاژها و جریان‌های دیده‌شده'], ref='func__Diag_UpdateMinMax'),
   N('sec','navy','بخش ۲: ذخیره در flash'),
   N('dia','yellow','زمان ذخیرهٔ خودکار رسید؟', ref='func__Diag_SaveNow', side=SIDE('green',['نوشتن record در صفحهٔ دیگر flash با شماره و نسخه','وسط نوشتن برق برود، نسخهٔ قبلی سالم می‌ماند'], 'r', 'بله'), elab='نه'),
   N('box','green','record آمادهٔ خواندن از پنل است', ['ساعت برد با زمان پنل هم‌زمان می‌شود'], note=SIDE('gray',['دو صفحهٔ flash یکی‌درمیان نوشته می‌شوند','تا سایش flash بین دو صفحه پخش شود'],'l'), ref='func__Diag_GetRecord'),
 ],
 'tree': T('ثبت سابقه',
   T('رویدادها',
     T('خطا → ثبت با زمان و عکس (۱۰ آخر)'),
     T('برق → خاموش/روشن (۲۰ آخر)'),
     T('کامل‌شدن شارژ هر channel')),
   T('جمع زمان شارژ channelها'),
   T('کمینه/بیشینهٔ ولتاژ و جریان'),
   T('ذخیرهٔ دوصفحه‌ای flash با نسخه'),
   T('ساعت برد از پنل')),
 'conds': [
   ['دفتر خطا پر شد','قدیمی‌ترین خطا می‌رود و ۱۰ خطای آخر همیشه هست.','۱۰ خطا / ۲۰ رویداد برق'],
   ['وسط ذخیره برق رفت','record قبلی سالم می‌ماند چون نوشتن روی صفحهٔ دیگر است و شماره/نسخه کنترل می‌شود.','دو صفحهٔ flash + شمارهٔ ترتیبی'],
   ['record flash خراب بود','record پذیرفته نمی‌شود و از نسخهٔ سالم یا پیش‌فرض شروع می‌شود.','عدد شناسایی + نسخه'],
 ],
 'outs': [
   ['record سابقه به پنل','خطاها، رویدادها، زمان‌های شارژ و کمینه/بیشینه‌ها','پنل می‌خواند'],
   ['ذخیره در flash','record ماندگار دستگاه','خودکار و با تأخیر کوتاه بعد از رویداد یا تغییر'],
 ],
}

MODULES['rtos'] = {
 'file': 'rtos_process.html',
 'title': 'فرآیند taskها (RTOS) — توضیح ساده',
 'sub': 'این بخش اسکلت زمانی برنامه است: پنج task با حافظهٔ ثابت (بدون حافظهٔ پویا) و یک ترتیب ثابت اجرا؛ watchdog هم مراقب است taskها سر وقت جواب بدهند.',
 'steps': [
   '<b>task اندازه‌گیری:</b> هر ۱ میلی‌ثانیه یک pass اندازه‌گیری و اعلام زنده‌بودن به watchdog.',
   '<b>task کنترل:</b> مغز تصمیم دستگاه با ترتیب ثابت: اول خطاها، بعد Jitter، خطای فنی شارژر، مسیر تغذیه، عدم‌توازن، تعویض خروجی، شارژر و ثبت سابقه؛ بعد هم اعلام به watchdog.',
   '<b>task ارتباط:</b> link پنل ESP را می‌چرخاند.',
   '<b>task نمایش:</b> چراغ‌ها و بوق را تازه می‌کند.',
   '<b>task حفاظت:</b> ناظر سبک (bit مبدل) را اجرا می‌کند؛ در ساخت اصلی خاموش است.',
   '<b>قانون کلی:</b> هر task اول عکس اندازه‌گیری را صفر و تازه می‌خواند تا هیچ‌وقت با دادهٔ کهنه یا نیمه‌کاره کار نکند؛ هیچ taskی مستقیم نمی‌خوابد و حافظهٔ پویا هم ندارد.',
 ],
 'flow': [
   N('sec','navy','بخش ۱: راه‌اندازی'),
   N('pill','navy','روشن‌شدن: ساخت پنج task با حافظهٔ ثابت', ref='func__Rtos_Start'),
   N('sec','navy','بخش ۲: پنج task هم‌زمان'),
   N('box','blue','task اندازه‌گیری — هر ۱ میلی‌ثانیه', ['خواندن مبدل، filter و انتشار عکس','اعلام زنده‌بودن به watchdog'], ref='func__TaskMeasurement'),
   N('box','blue','task کنترل — ترتیب ثابت تصمیم', ['خطاها → Jitter → خطای فنی → تغذیهٔ میکرو','→ عدم‌توازن → تعویض خروجی → شارژر → Diag → watchdog'], ref='func__TaskControl', note=SIDE('gray',['ترتیب تصمیم در هر pass ثابت است','حافظهٔ پویا نداریم؛ همه‌چیز از اول و ثابت ساخته شده است'],'l')),
   N('box','blue','task ارتباط — link پنل ESP', [], ref='func__TaskComm'),
   N('box','blue','task نمایش — چراغ‌ها و بوق', [], ref='func__TaskUi'),
   N('box','gray','task حفاظت — ناظر bit مبدل', ['در ساخت اصلی خاموش است'], ref='func__TaskProtection'),
   N('sec','navy','بخش ۳: watchdog'),
   N('dia','yellow','taskی سر وقت اعلام زنده‌بودن نکرد؟', side=SIDE('red',['watchdog سخت‌افزاری دستگاه را reset می‌کند','یعنی برنامه جایی گیر کرده است'], 'r', 'بله'), elab='نه'),
   N('box','green','چرخه ادامه دارد', ['حافظه از قبل و ثابت گرفته شده است؛ حافظهٔ پویا و خواب مستقیم نداریم'], ref='func__BspIwdg_CheckIn: اعلام زنده‌بودن'),
 ],
 'tree': T('taskها (RTOS)',
   T('اندازه‌گیری: هر ۱ میلی‌ثانیه + watchdog'),
   T('کنترل: ترتیب ثابت ماژول‌ها',
     T('خطاها، Jitter و خطای فنی'),
     T('تغذیهٔ میکرو و عدم‌توازن'),
     T('تعویض خروجی و شارژر')),
   T('ارتباط: link ESP'),
   T('نمایش: چراغ و بوق'),
   T('حفاظت: ناظر (خاموش در ساخت اصلی)'),
   T('watchdog: گیرکردن = reset')),
 'conds': [
   ['taskی گیر کرد و اعلام زنده‌بودن نکرد','watchdog دستگاه را reset می‌کند تا از حالت گیرکرده بیرون بیاید.','پنجرهٔ watchdog'],
   ['عکس اندازه‌گیری کهنه بود','هر task قبل از کار عکس را صفر و تازه می‌خواند؛ با دادهٔ نامعتبر تصمیم نمی‌گیرد.','—'],
 ],
 'outs': [
   ['ترتیب و زمان‌بندی اجرا','هر ماژول در task و نوبت خودش اجرا می‌شود','مغز دستگاه (کنترل) همیشه با تازه‌ترین خطاها و اندازه‌گیری کار می‌کند'],
   ['watchdog','reset خودکار هنگام گیرکردن','اعلام زنده‌بودن هر task در هر چرخه'],
 ],
}


MODULES['system'] = {
 'file': 'system_process.html',
 'title': 'فرآیند کل دستگاه — توضیح ساده',
 'sub': 'نمای کلی همهٔ برنامه: از روشن‌شدن تا چرخهٔ همیشگی تصمیم؛ هر ماژول صفحهٔ خودش را هم دارد و این‌جا فقط کنار هم دیده می‌شوند.',
 'steps': [
   '<b>روشن‌شدن:</b> اول سخت‌افزار (مبدل، PWM، پایه‌ها و وقفه‌ها) و بعد ماژول‌ها راه می‌افتند و تنظیمات ماندگار از حافظه خوانده می‌شود؛ چراغ‌ها خودآزمایی می‌شوند.',
   '<b>عکس مشترک:</b> task اندازه‌گیری هر ۱ میلی‌ثانیه یک عکس تازه و معتبر از ولتاژها و جریان‌ها منتشر می‌کند؛ همه با همان عکس کار می‌کنند.',
   '<b>ترتیب تصمیم در task کنترل ثابت است:</b> خطاها ← Jitter ← خطای فنی شارژر ← مسیر تغذیهٔ میکرو ← عدم‌توازن ← تعویض خروجی ← شارژر ← ثبت سابقه. یعنی اول ایمنی، بعد خروجی، بعد شارژ.',
   '<b>عمل:</b> شارژر duty channelها و relay را می‌راند، تعویض خروجی خط محافظ باتری را، و مسیر تغذیه تعیین می‌کند میکرو از کجا تغذیه شود.',
   '<b>نمایش و ارتباط جدا از چرخهٔ کنترل کار می‌کنند:</b> چراغ و بوق از روی وضعیت‌ها چهره می‌سازند و link ESP همان وضعیت‌ها را به پنل می‌دهد و تنظیمات را برمی‌گرداند.',
   '<b>نگهبان‌ها:</b> Jitter در همین چرخه پرچم trip را می‌گیرد و watchdog اگر taskی سر وقت اعلام زنده‌بودن نکند دستگاه را reset می‌کند.',
 ],
 'example': [
   'برق ورودی وصل می‌شود: اندازه‌گیری آن را می‌بیند؛ مسیر تغذیه بعد از ۵ ثانیه میکرو را روی ورودی می‌برد.',
   'شارژر هر دو باتری را شارژ می‌کند؛ چراغ زرد چشمک می‌زند و پنل همان وضعیت را نشان می‌دهد.',
   'یک باتری کامل می‌شود و دیگری نه: عدم‌توازن زیر نظر است؛ اگر اختلاف ماندگار از حد رد شود رویداد می‌خورد و دستگاه با چراغ و خروجی واکنش نشان می‌دهد.',
 ],
 'flow': [
   N('sec','navy','بخش ۱: راه‌اندازی دستگاه'),
   N('pill','navy','روشن‌شدن دستگاه', ref='func__Rtos_Start: روشن‌شدن و taskها'),
   N('box','blue','راه‌اندازی سخت‌افزار از راه BSP', ['مبدل و DMA، PWM، پایه‌ها، وقفهٔ Jitter','خواندن تنظیمات ماندگار از حافظه'], ref='func__BspGpio_Init: راه‌اندازی پایه‌ها'),
   N('box','blue','خودآزمایی چراغ‌ها و ساخت taskها', ['پنج task با حافظهٔ ثابت؛ بدون حافظهٔ پویا'], ref='func__Rtos_Start: ساخت taskها'),
   N('sec','navy','بخش ۲: چرخهٔ همیشگی — هر pass کنترل'),
   N('box','blue','۱) اندازه‌گیری: عکس تازه و معتبر', ['هر ۱ میلی‌ثانیه؛ همهٔ ماژول‌ها همین عکس را می‌خوانند'], ref='func__Measurement_Run'),
   N('box','blue','۲) بررسی خطاها: خطای ۶ (قطع باتری)', ['تشخیص قطع باتری و پاک‌شدنش این‌جا تصمیم گرفته می‌شود'], ref='func__TaskControl: Fault'),
   N('box','red','جیتر: پرچم قطع ایمنی سخت‌افزاری', ['هر کانالی که جریانش از حد مجاز بگذرد، پرچم قطع ایمنی‌اش قفل می‌شود'], ref='func__Jitter_Run'),
   N('box','blue','خطای فنی شارژر: خطای ۷', ['بعد از Jitter بررسی می‌شود و قفل نهایی می‌سازد'], ref='func__Charger_EvaluateTechnicalFault'),
   N('box','blue','۳) مسیر تغذیهٔ میکرو', ['اگر ورودی ۵ ثانیه بالای ۲۲ ولت بماند، میکرو از ورودی تغذیه می‌شود'], ref='func__McuPowerPath_Run'),
   N('box','blue','۴) نابرابری ولتاژ دو باتری', ['اختلاف ولتاژ باتری بالا و پایین و قفل عدم‌توازن بررسی می‌شود'], ref='func__Imbalance_Evaluate'),
   N('box','blue','۵) تعویض خروجی', ['حالت کلی دستگاه و خط محافظ باتری'], ref='func__TaskControl: Changeover'),
   N('box','blue','۶) شارژر: فازها و فرمان درصد توان', ['درصد توان کانال‌ها و رلهٔ قطع نهایی'], ref='func__TaskControl: Charger_Evaluate', note=SIDE('gray',['ترتیب دورها همیشه همین است','بعد از دور آخر، از اول شروع می‌شود'],'l')),
   N('box','blue','۷) ثبت سابقه', ['خطاها، رویداد برق و زمان شارژ در حافظهٔ فلش ثبت می‌شود'], ref='func__Diag_UpdateMinMax'),
   N('box','green','هم‌زمان: نمایش چراغ و بوق', ['چهرهٔ دستگاه از روی وضعیت‌ها ساخته می‌شود'], ref='func__Ui_Tick'),
   N('box','green','هم‌زمان: link پنل', ['پنل از همین‌جا می‌خواند و تنظیم می‌کند'], ref='func__EspLink_Run'),
   N('dia','yellow','همهٔ taskها سر وقت اعلام زنده‌بودن کردند؟', side=SIDE('red',['watchdog سخت‌افزاری دستگاه را reset می‌کند','یعنی یک task جایی گیر کرده است'], 'r', 'نه'), elab='بله'),
   N('pill','navy','برگشت به اول چرخه'),
 ],
 'tree': T('کل برنامهٔ دستگاه',
   T('راه‌اندازی',
     T('BSP: مبدل، PWM، پایه، وقفه'),
     T('خواندن تنظیمات ماندگار'),
     T('خودآزمایی چراغ‌ها')),
   T('task اندازه‌گیری: عکس تازه هر ۱ میلی‌ثانیه'),
   T('task کنترل (ترتیب ثابت)',
     T('خطاها (۶)'),
     T('Jitter: قفل پرچم trip'),
     T('خطای فنی (۷)'),
     T('مسیر تغذیهٔ میکرو'),
     T('نابرابری ولتاژ دو باتری'),
     T('تعویض خروجی و حالت کلی'),
     T('شارژر: فازها، PID، relay'),
     T('ثبت سابقه')),
   T('task نمایش: چراغ و بوق'),
   T('task ارتباط: link ESP و پنل'),
   T('watchdog: اعلام زنده‌بودن taskها')),
 'conds': [
   ['برق ورودی آمد','مسیر تغذیه بعد از ۵ ثانیه میکرو را روی ورودی می‌برد و شارژر با دیدن ورودی سالم شارژ را شروع می‌کند.','۲۲ ولت / ۵ ثانیه'],
   ['خطای بزرگی آمد (خطای ۶ یا خطای ۷)','اول از همه در ترتیب کنترل دیده می‌شود: شارژ متوقف، حالت کلی خطا و چهرهٔ خطا روی چراغ‌ها.','ترتیب ثابت کنترل'],
   ['باتری ضعیف شد','تعویض خروجی بعد از ۳ ثانیه پایداری خروجی را از باتری جدا می‌کند تا باتری آسیب نبیند.','۲۱ ولت / ۳ ثانیه'],
   ['taskی گیر کرد','watchdog دستگاه را reset می‌کند و برنامه از راه‌اندازی سالم شروع می‌کند.','پنجرهٔ watchdog'],
 ],
 'outs': [
   ['به سخت‌افزار','PWM دو channel شارژ، relay قطع نهایی، خط محافظ باتری، مسیر تغذیهٔ میکرو، چراغ‌ها و بوق','فقط از راه BSP؛ منطق ماژول‌ها پایه نمی‌شناسد'],
   ['به پنل ESP','همهٔ وضعیت‌ها و اندازه‌گیری‌ها برای نمایش؛ تنظیمات و فرمان‌ها برعکس','link frame نسخه‌دار با CRC-16'],
   ['به حافظهٔ ماندگار','تنظیمات، سابقهٔ خطاها و شمارنده‌ها','recordهای نسخه‌دار؛ با خاموشی نمی‌پرد'],
 ],
}

MODULES['flashlock'] = {
 'file': 'flashlock_process.html',
 'title': 'فرآیند قفل خواندن حافظه — توضیح ساده',
 'sub': 'این ماژول یک کار یک‌باره می‌کند: در اولین روشن‌شدنی که کلیدش روشن است، خواندن حافظهٔ برد را قفل می‌کند تا کسی نتواند برنامه را از روی برد بردارد. بعد از آن هیچ کاری نمی‌کند.',
 'steps': [
   '<b>شروع:</b> در نسخهٔ معمولی کلید این ماژول <b class="num">خاموش</b> است و هیچ چیز نوشته نمی‌شود.',
   '<b>اولین روشن‌شدن با کلید روشن:</b> برد می‌بیند قفل نیست، گزینه‌های کاربر را نگه می‌دارد، قفل خواندن را روشن می‌کند و خودش یک بار reset می‌شود.',
   '<b>بوت دوم و بعد:</b> برد می‌بیند قفل است و هیچ کاری نمی‌کند؛ پس reset تکراری و حلقهٔ بوت وجود ندارد.',
   '<b>اگر نوشتن شکست بخورد:</b> برد قفل نمی‌شود و باز هم بالا می‌آید؛ قفل‌نشدن هرگز باعث بوت‌نشدن نمی‌شود.',
   '<b>بعد از قفل:</b> کار عادی برد و تصحیح اندازه‌گیری با پنل مثل قبل است؛ فقط خواندن فلش با دیباگر ممکن نیست.',
   'هزینه‌اش این است که هر بار برنامه‌ریزی بعدی، کل تراشه را پاک می‌کند؛ پس تنظیم‌ها و جدول تصحیح جریان باید پشتیبان داشته باشند.',
 ],
 'flow': [
   N('sec','navy','بخش ۱: بررسی وضعیت قفل'),
   N('pill','navy','شروع برد (بوت)', ref='func__App_Init'),
   N('dia','yellow','قطعه از قبل قفل است؟', side=SIDE('red',['اقدام یک‌باره قبلاً انجام شده است','این بوت هیچ بایتی نوشته نمی‌شود','برد بوت عادی را ادامه می‌دهد'], 'r', 'بله'), elab='نه'),
   N('box','blue','نگه‌داشتن گزینه‌های کاربر تراشه', ['منبع watchdog و رفتار reset باید دست‌نخورده بماند'], ref='func__BspFlashLock_ApplyLevel1'),
   N('sec','navy','بخش ۲: نوشتن قفل'),
   N('box','blue','پاک‌کردن بلوک تنظیمات تراشه و روشن‌کردن قفل خواندن', ['پاک‌کردن این بلوک اجباری است: بدون آن قفل روشن نمی‌شود','مسدودکننده؛ پیش از شروع زمان‌بند اجرا می‌شود'], ref='func__BspFlashLock_ApplyLevel1', note=SIDE('gray',['مقدار قفل سطح ۱ برای این تراشه 0xA5 است','همان مقداری که سازنده در کد مرجع استفاده می‌کند'],'l')),
   N('dia','yellow','نوشتن موفق بود؟', side=SIDE('red',['قطعه باز می‌ماند','reset نمی‌شود و برد باز هم بالا می‌آید'], 'r', 'نه'), elab='بله'),
   N('sec','navy','بخش ۳: اعمال و بوت دوباره'),
   N('box','green','بارگذاری تنظیمات تازه و reset برد', ['کنترل بعد از reset برنمی‌گردد؛ بوت از اول شروع می‌شود'], ref='func__BspFlashLock_LaunchReset'),
   N('pill','navy','بوت دوباره، این بار با قطعهٔ قفل‌شده', ref='func__FlashLock_ApplyOnce'),
 ],
 'tree': T('قفل یک‌بارمصرف خواندن حافظه',
   T('کلید زمان‌کامپایل: خاموش به‌صورت پیش‌فرض',
     T('خاموش → ماژول کامپایل و تست می‌شود، ولی صدا زده نمی‌شود'),
     T('روشن → یک بار در شروع برنامه صدا زده می‌شود')),
   T('گام ۱: خواندن وضعیت قفل از خود تراشه'),
   T('گام ۲: نوشتن قفل سطح ۱ با نگه‌داشتن گزینه‌های کاربر'),
   T('گام ۳: بارگذاری تنظیمات و reset؛ بوت بعدی فقط «قفل است» را می‌بیند'),
   T('شکست در هر گام → قطعه باز می‌ماند و برد بوت می‌شود')),
 'conds': [
   ['قطعه از قبل قفل بود','هیچ نوشتنی انجام نمی‌شود و برد بوت عادی را ادامه می‌دهد.','خواندن وضعیت قفل از خود تراشه'],
   ['نوشتن قفل شکست خورد','قطعه باز می‌ماند، reset نمی‌شود و برد باز هم بالا می‌آید؛ بوت بعدی دوباره تلاش می‌کند.','نتیجهٔ نوشتن'],
   ['برق وسط نوشتن قطع شد','دفعهٔ بعد همان پرسش اول تکرار می‌شود؛ چیزی ذخیره نمی‌شود که کهنه بماند.','—'],
   ['کلید ماژول خاموش است','هیچ‌کدام از این گام‌ها اجرا نمی‌شود.','کلید زمان‌کامپایل'],
 ],
 'outs': [
   ['قفل خواندن تراشه','خواندن فلش با دیباگر ممکن نیست؛ برنامه بیرون نمی‌آید','یک بار، در اولین روشن‌شدن با کلید روشن'],
   ['reset یک‌باره','برد یک بار reset می‌شود و بعد کار عادی','فقط همان اولین بوت'],
   ['هشدار برای برنامه‌ریزی بعدی','هر نوشتن بعدی کل تراشه را پاک می‌کند؛ پشتیبان ناحیهٔ داده لازم است','ابزار لپ‌تاپ، نه خود برد'],
 ],
}

# ================= RTOS architecture map (2026-10-20) =================
# [EN] The RTOS page answers "what does each thread do"; this one answers
#      "how are they built, how do they share data, who supervises them".
#      Priorities, periods, stacks and watchdog numbers stay in the firmware
#      headers; the note under each section names the header it came from so
#      a stale number is visible at a glance.
# [FA] صفحهٔ taskها می‌گوید هر task چه می‌کند؛ این صفحه می‌گوید آن‌ها چطور
#      ساخته می‌شوند، داده را چطور تقسیم می‌کنند و چه کسی نگهبانی‌شان می‌کند.
#      عددها در هدرهای فرم‌ور می‌مانند و زیر هر بخش نام هدر آمده است.
MODULES['rtosmap'] = {
 'file': 'rtos_architecture.html',
 'title': 'معماری تسک‌های RTOS — اولویت، دوره و ارتباط',
 'sub': 'این صفحه نمای معماری است، نه یک pass از منطق: پنج task چطور ساخته می‌شوند، با چه اولویتی می‌چرخند، چه دادهٔ مشترکی دارند و watchdog چطور نگهبانی‌شان می‌کند.',
 'steps': [
   '<b>راه‌اندازی:</b> اول سخت‌افزار بالا می‌آید و watchdog روشن می‌شود؛ بعد تنظیمات ماندگار و جدول تصحیح جریان خوانده می‌شوند؛ آخر پنج task با حافظهٔ ثابت ساخته می‌شوند و زمان‌بند شروع می‌شود.',
   '<b>اولویت:</b> عدد بزرگ‌تر زودتر اجرا می‌شود. حفاظت <b class="num">۱۱</b> (فعلاً خاموش)، کنترل و اندازه‌گیری هر دو <b class="num">۱۰</b>، ارتباط <b class="num">۹</b> و نمایش <b class="num">۸</b>.',
   '<b>دوره:</b> اندازه‌گیری هر <b class="num">۱</b>، کنترل هر <b class="num">۱۰</b>، نمایش هر <b class="num">۱۰</b>، حفاظت هر <b class="num">۵</b> و ارتباط هر <b class="num">۱۰۰</b> میلی‌ثانیه. دو task هم‌اولویت با time-slicing نوبتی می‌چرخند و نوبت هر کدام یک تیک (۱ میلی‌ثانیه) است.',
   '<b>دادهٔ مشترک:</b> همه از یک عکس اندازه‌گیری می‌خوانند، ماسک خطا را task کنترل می‌سازد و بقیه می‌خوانند، و تنظیم‌های پنل را task ارتباط می‌نویسد.',
   '<b>watchdog:</b> هر task در هر چرخه اعلام زنده‌بودن می‌کند؛ فقط task کنترل تازه‌سازی می‌زند و آن هم تنها وقتی همهٔ شکاف‌ها تازه باشند.',
   '<b>قاعده:</b> هیچ taskی با حلقهٔ مشغول یا HAL_Delay نمی‌خوابد و هیچ حافظهٔ پویایی گرفته نمی‌شود.',
   '<b>«pass» یعنی چه؟</b> یک دور کامل اجرای همان task: از اول تابع تا آخرش و برگشت به همان نقطه. task کنترل هر ۱۰ و task اندازه‌گیری هر ۱ میلی‌ثانیه یک pass تازه دارد.',
 ],
 'example': [
   'میکرو روشن می‌شود؛ watchdog با پنجرهٔ حدود ۱ ثانیه شروع می‌شود و اولین تازه‌سازی در همان میلی‌ثانیه‌های اول از پاس کنترل می‌آید.',
   'اگر task نمایش گیر کند و ۱۵۰۰ میلی‌ثانیه اعلام حضور نکند، پاس کنترل تازه‌سازی نمی‌کند و watchdog سخت‌افزاری برد را reset می‌کند - حتی اگر بقیهٔ taskها سالم باشند.',
   'اگر مبدل خراب باشد ولی task اندازه‌گیری زنده بماند، اعلام حضور می‌رسد و برد reset نمی‌شود؛ شارژر با عکس نامعتبر در حالت امن می‌ماند.',
 ],
 'flow': [
   N('sec', 'navy', 'بخش ۱: از ریست تا زمان‌بند (منبع: rtos_app.c و app.c)'),
   N('pill', 'navy', 'روشن‌شدن برد', ref='func__App_Start'),
   N('box', 'blue', 'آماده‌سازی سخت‌افزار و نگهبان', ['کلاک، مبدل، تایمرها، DMA و UART', 'watchdog حدود ۱ ثانیه، پیش از زمان‌بند'], ref='func__BspIwdg_Init'),
   N('box', 'blue', 'خواندن حافظهٔ ماندگار و جدول تصحیح', ['بازپخش تنظیم‌های پنل از فلش', 'جدول تصحیح جریان و سابقهٔ ثبت وضعیت', 'قفل خواندن: خاموش'], ref='func__App_Init'),
   N('box', 'blue', 'ساخت پنج کار (task)', ['حافظهٔ ثابت: TCB و استک هر task', 'هر task پشت فلگ MODULE_* خودش', 'شکست ساخت = توقف بی‌پایان؛ watchdog ریست می‌کند'], ref='func__Rtos_Start'),
   N('box', 'blue', 'روشن‌کردن زمان‌بند', ['تیک ۱۰۰۰ هرتز', 'این فراخوانی هرگز برنمی‌گردد'], ref='func__Rtos_Start: osKernelStart'),
   N('sec', 'navy', 'بخش ۲: پنج task - اولویت، دوره، استک (منبع: rtos_config.h)'),
   N('box', 'gray', 'task حفاظت — اولویت ۱۱', ['دوره ۵ میلی‌ثانیه، استک ۱۹۲ کلمه', 'بالاترین اولویت سیستم: Protection_Run', 'این task فعلاً ساخته نمی‌شود'], ref='func__TaskProtection', go='protection'),
   N('box', 'blue', 'task کنترل — اولویت ۱۰', ['دوره ۱۰ میلی‌ثانیه، استک ۲۵۶ کلمه', 'ترتیب ثابت تصمیم؛ در پایان kick watchdog', 'شکاف watchdog ندارد: خودِ صدا زدن سند زنده‌بودن است'], ref='func__TaskControl', go='system'),
   N('box', 'blue', 'task اندازه‌گیری — اولویت ۱۰', ['دوره ۱ میلی‌ثانیه، استک ۱۹۲ کلمه', 'هر پاس یک frame کامل DMA را تبدیل می‌کند', 'عکس مشترک را منتشر می‌کند'], ref='func__TaskMeasurement', go='measurement'),
   N('box', 'yellow', 'task ارتباط — اولویت ۹', ['دوره ۱۰۰ میلی‌ثانیه، استک ۲۵۶ کلمه', 'تجزیهٔ پیام پنل، اعمال تنظیم‌ها و فرستادن گزارش دوره‌ای'], ref='func__TaskComm', go='esplink'),
   N('box', 'green', 'task نمایش — اولویت ۸', ['دوره ۱۰ میلی‌ثانیه، استک ۱۲۸ کلمه', 'چراغ و بوق از روی عکس و وضعیت‌ها', 'تست برد در شروع حدود ۱٫۶۵ ثانیه'], ref='func__TaskUi', go='ui'),
   N('sec', 'navy', 'بخش ۳: ترتیب ثابت یک پاس کنترل (منبع: task_control.c)'),
   N('box', 'blue', '۱) خطاها → ۲) Jitter → ۳) خطای فنی شارژر', ['ماسک خطا اول تازه می‌شود تا همین پاس دیده شود'], ref='func__TaskControl: Fault_Evaluate'),
   N('box', 'blue', '۴) مسیر تغذیهٔ میکرو → ۵) عدم‌توازن', ['عدم‌توازن قبل از تعویض خروجی تا وتوی همان پاس اثر کند'], ref='func__TaskControl: Imbalance_Evaluate'),
   N('box', 'blue', '۶) تعویض خروجی → ۷) شارژر → ۸) ثبت سابقه', ['حالت کلی دستگاه ساخته می‌شود و به شارژر داده می‌شود'], ref='func__TaskControl: Charger_Evaluate'),
   N('box', 'green', '۹) تازه‌سازی watchdog و خواب تا پاس بعد', ['فقط اگر همهٔ شکاف‌ها تازه باشند'], ref='func__BspIwdg_PollKick'),
   N('sec', 'navy', 'بخش ۴: دادهٔ مشترک و مالک هر کدام'),
   N('box', 'blue', 'عکس اندازه‌گیری: نویسنده task اندازه‌گیری', ['خواننده: کنترل، نمایش، ارتباط', 'کپی اتمیک با osKernelLock()'], ref='func__Measurement_GetSnapshot'),
   N('box', 'blue', 'ماسک خطا: نویسنده task کنترل', ['خواننده: ارتباط و Diag', 'در همان پاس: عدم‌توازن و شارژر', 'قفل هنگام نوشتن، volatile برای خواندن'], ref='func__Fault_Get'),
   N('box', 'blue', 'پارامترهای پنل: نویسنده task ارتباط', ['اثر روی اندازه‌گیری، شارژر، نمایش، خطا', 'در فلش ماندگار می‌شوند'], ref='func__EspLink_Run'),
   N('box', 'blue', 'سابقهٔ دیاگ: نویسنده task کنترل', ['خواننده: ارتباط برای فرستادن به پنل'], ref='func__Diag_GetRecord'),
   N('sec', 'navy', 'بخش ۵: watchdog (منبع: bsp_iwdg.h)'),
   N('dia', 'yellow', 'همهٔ شکاف‌های watchdog زیر ۱۵۰۰ میلی‌ثانیه تازه‌اند؟', ref='func__BspIwdg_PollKick', side={'c': 'red', 'l': ['یک task مرده یا گیر کرده است', 'هیچ تازه‌سازی‌ای انجام نمی‌شود', 'watchdog سخت‌افزاری برد را reset می‌کند', 'بعد از ریست از «روشن‌شدن برد» شروع می‌شود'], 'd': 'r', 'lab': 'نه', 'retToText': 'روشن‌شدن برد'}, elab='بله'),
   N('pill', 'navy', 'تازه‌سازی انجام شد؛ چرخه ادامه دارد'),
   N('sec', 'navy', 'بخش ۶: وقفه‌ها و DMA که مسیر داده را می‌سازند'),
   N('box', 'blue', 'EXTI: تریپ Jitter و ورود ۲۴ ولت', ['PB2 و PB6 فقط لبهٔ پایین‌رونده', 'PB4 ورودی: وصل فوری باتری همان‌جا'], ref='func__BspExti_OnIrq'),
   N('box', 'blue', 'USART1 + DMA: حلقهٔ RX و TX', ['بافر دریافت ۲۵۶ بایت، بافر فرستادن ۱۰۲۴ بایت', 'DMA بایت‌به‌بایت CPU را درگیر نمی‌کند', 'فریم‌ها به task ارتباط می‌رسند'], ref='func__EspLink_Run: UART'),
   N('box', 'blue', 'TIM2 و TIM3 + ADC + DMA', ['ترکیب دو گیت PWM و تریگر مبدل', 'دو frame حلقوی برای task اندازه‌گیری'], ref='func__BspAdc_Init'),
 ],
 'tree': {'t': 'معماری taskها', 'c': [
   {'t': 'راه‌اندازی', 'c': [{'t': 'آماده‌سازی سخت‌افزار و نگهبان'}, {'t': 'خواندن حافظهٔ ماندگار و جدول تصحیح'}, {'t': 'ساخت پنج کار (task)'}, {'t': 'روشن‌کردن زمان‌بند'}]},
   {'t': 'حفاظت: اولویت ۱۱، ۵ میلی‌ثانیه (خاموش)'},
   {'t': 'کنترل: اولویت ۱۰، ۱۰ میلی‌ثانیه', 'c': [{'t': 'ترتیب ثابت نُه مرحله‌ای'}, {'t': 'تازه‌سازی watchdog در پایان'}]},
   {'t': 'اندازه‌گیری: اولویت ۱۰، ۱ میلی‌ثانیه'},
   {'t': 'ارتباط: اولویت ۹، ۱۰۰ میلی‌ثانیه'},
   {'t': 'نمایش: اولویت ۸، ۱۰ میلی‌ثانیه'},
   {'t': 'دادهٔ مشترک', 'c': [{'t': 'عکس اندازه‌گیری'}, {'t': 'ماسک خطا'}, {'t': 'پارامترهای پنل'}, {'t': 'سابقهٔ دیاگ'}]},
   {'t': 'watchdog: فقط کنترل تازه‌سازی می‌زند'},
 ]},
 'conds': [
   ['یک task اعلام زنده‌بودن نکرد', 'تازه‌سازی watchdog انجام نمی‌شود و برد reset می‌شود تا از حالت گیرکرده بیرون بیاید.', '۱۵۰۰ میلی‌ثانیه'],
   ['مبدل پاسخ نداد', 'task اندازه‌گیری زنده می‌ماند و اعلام حضور می‌کند؛ شارژر با عکس نامعتبر در حالت امن می‌ماند و برد reset نمی‌شود.', 'عکس نامعتبر'],
   ['task نمایش در شروع مشغول تست برد است', 'حدود ۱٫۶۵ ثانیه طول می‌کشد؛ مهلت بوت watchdog ۱۵۰۰ میلی‌ثانیه است، پس از ۱٫۵ ثانیه تا اولین اعلام حضور نمایش چند پاس تازه‌سازی عقب می‌افتد، ولی تایم‌اوت یک‌ثانیه‌ای واچ‌داگ پر نمی‌شود (حاشیهٔ حدود ۰٫۸ ثانیه).', '۱۵۰۰ میلی‌ثانیه مهلت بوت'],
   ['ترکیب ماژول‌ها task کنترل نداشت', 'watchdog عمداً روشن نمی‌شود چون هیچ‌کس نمی‌تواند تازه‌سازی بزند.', 'BSP_IWDG_PUMPER_EXISTS'],
 ],
 'outs': [
   ['به سخت‌افزار', 'PWM، relay، خط محافظ باتری، مسیر تغذیهٔ میکرو، چراغ‌ها و بوق', 'فقط از راه BSP؛ منطق ماژول‌ها پایه نمی‌شناسد'],
   ['به پنل ESP', 'عکس اندازه‌گیری، ماسک خطا، سابقهٔ دیاگ و وضعیت شارژ', 'task ارتباط، هر ۱۰۰ میلی‌ثانیه'],
   ['به حافظهٔ ماندگار', 'تنظیم‌های پنل، جدول تصحیح جریان، سابقهٔ دیاگ و شمارنده‌ها', 'recordهای نسخه‌دار؛ با خاموشی نمی‌پرد'],
 ],
}


DIRS = {'charging': 'Charger', 'measurement': 'Measurement', 'fault': 'Fault',
        'imbalance': 'Imbalance', 'changeover': 'Changeover', 'ui': 'Ui',
        'protection': 'Protection', 'callut': 'CalLut', 'esplink': 'EspLink',
        'mcupowerpath': 'McuPowerPath', 'jitter': 'Jitter', 'diag': 'Diag',
        'rtos': 'Rtos', 'flashlock': 'FlashLock', 'rtosmap': 'Rtos'}
NAVLABELS = [('system', 'کل سیستم'), ('charging', 'شارژر'), ('measurement', 'اندازه‌گیری'),
             ('fault', 'خطاها'), ('imbalance', 'عدم‌توازن'), ('changeover', 'تعویض خروجی'),
             ('ui', 'چراغ و بوق'), ('protection', 'ناظر حفاظت'), ('callut', 'جدول تصحیح جریان'),
             ('esplink', 'ارتباط با پنل'), ('mcupowerpath', 'تغذیهٔ میکرو'), ('jitter', 'جیتر (Jitter)'),
             ('diag', 'ثبت سابقه'), ('flashlock', 'قفل خواندن'), ('rtos', 'کارها (تسک‌ها)'),
             ('rtosmap', 'معماری کارها')]
ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

def target_path(key):
    if key == 'system':
        return os.path.join(ROOT, 'Documentation', 'system_process.html')
    if key == 'rtosmap':
        # [EN] architecture view lives next to the other Documentation pages
        # [FA] نمای معماری کنار بقیهٔ صفحه‌های Documentation می‌نشیند
        return os.path.join(ROOT, 'Documentation', 'rtos_architecture.html')
    return os.path.join(ROOT, 'Firmware', 'Modules', DIRS[key], MODULES[key]['file'])

GH_BLOB = 'https://github.com/sargoliwork101-ui/ChangeOver/blob/arena/bb1aee1b-changeover/'
GH_TREE = 'https://github.com/sargoliwork101-ui/ChangeOver/tree/arena/bb1aee1b-changeover/'
TOKEN_RE = re.compile(r'[A-Za-z][A-Za-z0-9_]{3,}')

IDENT_RE = re.compile(r'([A-Za-z_][A-Za-z0-9_]*)\s*\(')

CTRL_WORDS = {'if', 'for', 'while', 'switch', 'else', 'return', 'case', 'do',
              'sizeof', 'assert', 'typedef'}
DEF_RE = re.compile(
    r'^\s*(?:static\s+|inline\s+|extern\s+)*(?:const\s+)?'
    r'[A-Za-z_][A-Za-z0-9_]*[\s\*]+(?:[A-Za-z_][A-Za-z0-9_]*[\s\*]+)*'
    r'([A-Za-z_][A-Za-z0-9_]*)\s*\(')
MACRO_RE = re.compile(r'^\s*#\s*define\s+([A-Za-z_][A-Za-z0-9_]*)')

# [EN] A link may only point at a line that really DEFINES the name
#      (AI_AGENT_RULES: the target must be the definition, not a call, an
#      assert or the first passing mention). Ranks: 0/1 = definition,
#      6 = prototype, 8 = call/mention, 9 = bare token.
# [FA] لینک فقط به خطی می‌رود که همان نام را واقعاً تعریف کند؛ نه صدا زدن،
#      نه assert، نه اولین اشارهٔ گذری.
DEF_RANK_MAX = 1
PROTO_RANK_MAX = 6


def definition_has_body(lines, index):
    for j in range(index, min(index + 15, len(lines))):
        line = re.sub(r'/\*.*?\*/', '', lines[j])
        if '{' in line:
            return True
        if j > index and ';' in line:
            return False
    return False


def build_index(base):
    cand = {}

    def offer(ident, rank, rel, ln):
        if ident not in cand or rank < cand[ident][0]:
            cand[ident] = (rank, rel, ln)

    for dp, dn, fn in os.walk(base):
        tester = 'Tester' in dp
        for f in sorted(fn):
            if not (f.endswith('.c') or f.endswith('.h')):
                continue
            path = os.path.join(dp, f)
            rel = os.path.relpath(path, ROOT).replace(os.sep, '/')
            base_rank = (6 if tester else 0) + (0 if f.endswith('.c') else 1)
            try:
                lines = open(path, encoding='utf-8', errors='ignore').read().split('\n')
            except OSError:
                continue
            in_comment = False
            for ln, line in enumerate(lines, 1):
                st = line.strip()
                if in_comment:
                    if '*/' in line:
                        in_comment = False
                        line = line.split('*/', 1)[1]
                        st = line.strip()
                    else:
                        continue
                if st.startswith(('*', '//')):
                    continue
                if st.startswith('/*'):
                    if '*/' not in st[2:]:
                        in_comment = True
                    continue
                if '/*' in line:
                    line = line.split('/*', 1)[0]
                    in_comment = True
                macro = MACRO_RE.match(line)
                if macro:
                    offer(macro.group(1), base_rank, rel, ln)
                    continue
                definition = DEF_RE.match(line)
                if (definition and definition.group(1) not in CTRL_WORDS and
                        definition_has_body(lines, ln - 1)):
                    offer(definition.group(1), base_rank, rel, ln)
                    continue
                proto = line.rstrip().endswith(';')
                for ident in set(IDENT_RE.findall(line)):
                    offer(ident, base_rank + (PROTO_RANK_MAX if proto else 8), rel, ln)
                for tok in set(TOKEN_RE.findall(line)):
                    offer(tok, base_rank + 9, rel, ln)
    return cand


def resolve_ref(ref, idx, fallback):
    toks = [t for t in TOKEN_RE.findall(ref or '')
            if len(t) >= 4 and ('_' in t or any(ch.isupper() for ch in t[1:]))]
    seen = {}
    for t in toks:
        seen.setdefault(t, len(seen))
    toks = list(seen)
    toks.sort(key=lambda t: (0 if t.startswith('func__') else 1, seen[t]))

    def url(v):
        if v[2] < 1:
            return None
        path = os.path.join(ROOT, v[1])
        try:
            line = open(path, encoding='utf-8', errors='ignore').read().split('\n')[v[2] - 1]
        except (OSError, IndexError):
            return None
        if ('(' not in line) and ('#define' not in line):
            return None
        return GH_BLOB + v[1] + '#L' + str(v[2])

    for t in toks:  # exact definition (function or #define)
        if t in idx and idx[t][0] <= DEF_RANK_MAX:
            link = url(idx[t])
            if link:
                return link
    for t in toks:  # suffix definition: RegulateChannel -> func__Charger_RegulateChannel
        best = None
        for ident, v in idx.items():
            if ident.endswith('_' + t) and v[0] <= DEF_RANK_MAX:
                if (best is None or v[0] < best[0] or
                        (v[0] == best[0] and len(ident) < len(best[3]))):
                    best = (v[0], v[1], v[2], ident)
        if best:
            link = url(best)
            if link:
                return link
    for t in toks:  # header prototype is the next best evidence
        if t in idx and idx[t][0] <= PROTO_RANK_MAX:
            link = url(idx[t])
            if link:
                return link
    for t in toks:  # last resort: a real mention (enum value, struct field)
        if t in idx:
            link = url(idx[t])
            if link:
                return link
    return fallback


def main():
    sys_idx = None
    for key, m in MODULES.items():
        path = target_path(key)
        if sys_idx is None:
            sys_idx = build_index(os.path.join(ROOT, 'Firmware'))
        if key == 'system':
            idx = sys_idx
            fallback = GH_TREE + 'Firmware'
        else:
            idx = build_index(os.path.join(ROOT, 'Firmware', 'Modules', DIRS[key]))
            # [EN] A definition anywhere in Firmware (rank +2) beats a
            #      prototype or a call in this module, so the link always
            #      lands on the real definition.
            # [FA] تعریف در کل Firmware (با +۲) از prototype و صدا زدن داخل
            #      ماژول جلوتر است تا لینک همیشه روی تعریف واقعی بنشیند.
            for k2, v2 in sys_idx.items():
                if k2 not in idx or v2[0] + 2 < idx[k2][0]:
                    idx[k2] = (v2[0] + 2, v2[1], v2[2])
            fallback = GH_TREE + 'Firmware/Modules/' + DIRS[key]
        GO = {
         'system': {'۱) اندازه‌گیری': 'measurement', '۲) بررسی خطاها': 'fault',
                    'جیتر: پرچم قطع ایمنی': 'jitter',
                    'خطای فنی': 'charging', '۳) مسیر تغذیه': 'mcupowerpath',
                    '۵) تعویض خروجی': 'changeover', '۶) شارژر': 'charging',
                    '۴) نابرابری ولتاژ': 'imbalance',
                    '۷) ثبت سابقه': 'diag', 'هم‌زمان: link پنل': 'esplink', 'هم‌زمان: نمایش': 'ui'},
         'charging': {'قفل خطای فنی (خطای ۷) از قبل set شده؟': 'alarm:fault_7_technical',
                      'خطای نهایی (FINAL_FAULT)': 'alarm:fault_7_technical',
                      'اطلاع به چراغ و بوق': 'ui', 'پرچم trip': 'jitter'},
         'measurement': {'یک بستهٔ کامل و تازه از مبدل رسید؟': 'alarm:fault_0_adc',
                         'تبدیل عدد خام': 'callut', 'انتشار عکس اندازه‌گیری': 'system'},
         'fault': {'خروجی: پرچم خطای ۶': 'alarm:fault_6_battery_lost'},
         'changeover': {'باتری برای خروجی ممنوع است': 'imbalance'},
         'ui': {'خطای فنی (۷) set است؟': 'alarm:fault_7_technical',
                'اضافه‌ولتاژ ورودی هست؟': 'alarm:input_overvoltage',
                'خطای ۶ (قطع باتری) set است؟': 'alarm:fault_6_battery_lost',
                'باتری «خراب» قفل شده؟': 'alarm:dead_battery',
                'عدم‌توازن قفل شده؟': 'alarm:imbalance_latched',
                'شارژ کامل است؟': 'alarm:charge_full',
                'استراحت حرارتی فعال است؟': 'alarm:thermal_rest',
                'channelی در حال شارژ است؟': 'alarm:charging',
                'ورودی سالم: سبز ثابت': 'alarm:input_ok'},
         'esplink': {'نوشتن: فرستادن به ماژول صاحب شناسه': 'system'},
         'diag': {'record آمادهٔ خواندن از پنل است': 'esplink'},
         'jitter': {'شارژر پرچم را می‌خواند': 'charging'},
         'protection': {'set کردن FAULT_ADC': 'alarm:fault_0_adc'},
         'rtos': {'task اندازه‌گیری': 'measurement', 'task کنترل': 'system', 'task ارتباط': 'esplink',
                  'task نمایش': 'ui', 'task حفاظت': 'protection'},
         'callut': {'نشستن جدول تازه': 'esplink'},
         'imbalance': {'رویدادها به حد قفل رسیدند؟': 'alarm:imbalance_latched',
                       'پاک‌شدن‌ها': 'ui'},
        }
        AFTER = {
         ('callut', 'شمارش داخل بازهٔ جدول است؟', 'side'): 'همان مقدارِ لبه برمی‌گردد و تبدیل تمام است',
         ('callut', 'درون‌یابی خطی بین دو نقطه', 'note'): 'خط اصلی، پایین همین box',
         ('callut', 'جدول تازه کامل و درست است؟', 'side'): 'جدول قبلی کار می‌کند؛ این درخواست تمام است',
         ('changeover', 'Snapshot معتبر است؟', 'side'): 'همین دور تمام؛ دور بعد دوباره از اول',
         ('changeover', 'bit خطایی set است؟', 'side'): 'فقط قفل باتری کم به‌روز می‌شود؛ قطع‌وصل نداریم',
         ('changeover', 'باتری برای خروجی ممنوع است؟', 'side'): 'فقط قفل باتری کم به‌روز می‌شود؛ قطع‌وصل نداریم',
         ('changeover', 'شرط قطع ۳ ثانیه پیوسته ماند؟ (قفل باتری کم یا زیر ۲۰٫۸ ولت)', 'side'): 'وصل دوباره فقط با برگشت ورودی و پایداری آن',
         ('changeover', 'ورودی برگشت؟', 'side'): 'حالت عادی، پایین همین جریان',
         ('changeover', 'حالت عادی از روی حضور ورودی', 'note'): 'دور بعد از اول همین فلوچارت',
         ('charging', 'ذخیره در حافظهٔ ماندگار در حال انجام است؟', 'side'): 'پایان این دور؛ دور بعد از اول',
         ('charging', 'قفل خطای فنی (خطای ۷) از قبل set شده؟', 'side'): 'پایان این pass؛ قفل تا خاموش و روشن‌شدن می‌ماند',
         ('charging', 'mode test دستی پنل فعال است؟', 'side'): 'با ۳ ثانیه سکوت link خارج می‌شود و شارژ عادی برمی‌گردد',
         ('charging', 'سیم باتری قطع است؟ (خطای ۶)', 'side'): 'channelهای دیگر ادامه دارند؛ این channel متوقف است',
         ('charging', 'اندازه‌گیری سالم و شارژ مجاز است؟', 'side'): 'پایان این دور؛ دور بعد از اول',
         ('charging', 'ولتاژ ورودی بین ۲۲ و ۲۸ ولت است؟', 'side'): 'پایان این دور؛ دور بعد دوباره از اول',
         ('charging', 'ولتاژ ورودی بین ۲۲ و ۲۸ ولت است؟', 'note'): 'خط اصلی؛ این بررسی جدا از شارژ انجام می‌شود',
         ('charging', 'channel از پنل خاموش شده؟', 'side'): 'channel بعدی، پایین همین بخش',
         ('charging', 'قفل باتری خراب یا بودجهٔ عدم‌توازن تمام شده؟', 'side'): 'channel بعدی، پایین همین بخش',
         ('charging', 'ولتاژ باتری بین ۲ و ۱۵ ولت است؟', 'side'): 'channel بعدی، پایین همین بخش',
         ('charging', 'جریان از ۹۵۰ میلی‌آمپر بالاتر رفته؟', 'side'): 'channel بعدی؛ شروع بعدی این channel با شروع نرم است',
         ('charging', 'اتصال باتری ۱۵ ثانیه پایدار بوده؟', 'side'): 'فازهای شارژ پایین، بدون این channel',
         ('charging', 'شارژ سریع (BULK)', 'side'): 'خط اصلی، سؤال «به ۱۴٫۳ رسید؟»',
         ('charging', 'جذب (ABSORB)', 'note'): 'خط اصلی؛ قطع ایمنی در همان کانال شمرده می‌شود',
         ('charging', 'جذب (ABSORB)', 'note'): 'خط اصلی، سؤال «کامل شد؟»',
         ('diag', 'خطایی set شد؟', 'side'): 'خط اصلی، سؤال بعدی',
         ('diag', 'خاموش یا روشن‌شدن بود؟', 'side'): 'خط اصلی، جمع‌کردن آمار',
         ('diag', 'زمان ذخیرهٔ خودکار رسید؟', 'side'): 'خط اصلی، record آمادهٔ خواندن',
         ('diag', 'record آمادهٔ خواندن از پنل است', 'note'): 'خط اصلی؛ پنل هر وقت خواست record را می‌خواند',
         ('esplink', 'frame کامل و CRC درست است؟', 'side'): 'منتظر frame بعدی؛ پاسخی فرستاده نمی‌شود',
         ('esplink', 'نسخه و نوع frame شناخته‌شده است؟', 'side'): 'منتظر frame بعدی',
         ('esplink', 'مقدار داخل بازهٔ مجاز بود؟', 'side'): 'مقدار گیره‌شده اعمال می‌شود و به پنل گزارش می‌شود',
         ('esplink', 'اعمال مقدار + ذخیرهٔ ماندگار (اگر لازم است)', 'note'): 'خط اصلی؛ telemetry سر وقت خودش فرستاده می‌شود',
         ('fault', 'حالت تست دستی از پنل روشن است؟', 'side'): 'پایان این دور؛ شمارش‌ها همان‌جا می‌ایستند',
         ('fault', 'عکس اندازه‌گیری معتبر است؟', 'side'): 'پایان این دور؛ دور بعد از اول',
         ('fault', 'قانون ۱: باتری در حال شارژ بالای ۱۴٫۸ ولت رفت و ۱۵۰ میلی‌ثانیه ماند؟', 'side'): 'خروجی خطای ۶، پایین همین جریان',
         ('fault', 'قانون ۲: ورودی سالم است و یک باتری زیر ۶ ولت افتاد و ۱ ثانیه ماند؟', 'side'): 'خروجی خطای ۶، پایین همین جریان',
         ('fault', 'بازیابی: هر دو باتری بالای ۷ ولت‌اند و باتری در حال شارژ بالای ۱۴٫۸ نیست، ۱ ثانیه ماند؟', 'side'): 'خروجی خطای ۶، پایین همین جریان',
         ('fault', 'خروجی: پرچم خطای ۶', 'note'): 'شارژر همین پرچم را در دور خودش می‌خواند',
         ('imbalance', 'ورودی‌ها معتبرند؟', 'side'): 'پایان این دور؛ دور بعد از اول',
         ('imbalance', 'هر دو باتری در یک وضعیت‌اند؟', 'side'): 'نامزد این دور خالی است؛ دور بعد دوباره',
         ('imbalance', 'اختلاف بالای حد پنجره ماند و زمان پایداری پر شد؟', 'side'): 'سؤال پایان چرخهٔ شارژ، پایین همین جریان',
         ('imbalance', 'چرخهٔ شارژ با شارژ کامل تمام شد؟', 'side'): 'سؤال قفل، پایین؛ بدون رویداد تازه',
         ('imbalance', 'ثبت یک رویداد برای این چرخهٔ شارژ', 'note'): 'خط اصلی، سؤال قفل',
         ('imbalance', 'رویدادها به حد قفل رسیدند؟ (پیش‌فرض ۱۰)', 'side'): 'قفل به تعویض خروجی و چراغ‌وبوق می‌رود',
         ('jitter', 'قفل پرچم trip همان channel', 'note'): 'خط اصلی؛ شارژر پرچم را در pass خودش می‌خواند',
         ('mcupowerpath', 'لبهٔ افتادن حضور ورودی آمد؟ (وقفه)', 'side'): 'خط اصلی؛ تغذیه همین حالا از باتری است',
         ('mcupowerpath', 'لبهٔ افتادن حضور ورودی آمد؟ (وقفه)', 'note'): 'خط اصلی؛ وقفه هر لحظه می‌تواند بیاید',
         ('mcupowerpath', 'Snapshot معتبر است؟', 'side'): 'سؤال‌های پایین این pass رد می‌شوند',
         ('mcupowerpath', '۵ ثانیه پیوسته بالای ۲۲ ماند؟', 'side'): 'سؤال برگشت، پایین؛ هنوز از باتری هستیم',
         ('mcupowerpath', 'ورودی زیر ۲۱٫۵ ولت افتاد؟', 'side'): 'پایان این دور؛ دور بعد از اول',
         ('measurement', 'یک بستهٔ کامل و تازه از مبدل رسید؟', 'side'): 'بقیهٔ این دور رد می‌شود؛ دور بعد (۱ میلی‌ثانیه) از اول',
         ('measurement', 'یک بستهٔ کامل و تازه از مبدل رسید؟', 'note'): 'خط اصلی؛ DMA همیشه در پس‌زمینه کار می‌کند',
         ('measurement', 'تنظیم صافیِ جریان از پنل عوض شده؟', 'side'): 'خط اصلی، تمیزکردن جریان و ولتاژ',
         ('protection', 'Snapshot معتبر است؟', 'side'): 'پایان این pass؛ خطای مبدل پاک است',
         ('protection', 'set کردن FAULT_ADC', 'note'): 'خط اصلی؛ این ماژول فعلاً کاری نمی‌کند',
         ('rtos', 'task کنترل — ترتیب ثابت تصمیم', 'note'): 'خط اصلی؛ ترتیب هر pass همین است',
         ('rtos', 'taskی سر وقت اعلام زنده‌بودن نکرد؟', 'side'): 'دستگاه از اول روشن می‌شود',
         ('ui', 'Snapshot معتبر است؟', 'side'): 'نمایش این pass همین‌جا می‌ایستد',
         ('ui', 'خطای فنی (۷) set است؟', 'side'): 'تمام؛ بقیهٔ سؤال‌ها این pass پرسیده نمی‌شوند',
         ('ui', 'خطای فنی (۷) set است؟', 'note'): 'خط اصلی؛ این فقط توضیح اولویت است',
         ('ui', 'اضافه‌ولتاژ ورودی هست؟', 'side'): 'تمام؛ بقیهٔ سؤال‌ها این pass پرسیده نمی‌شوند',
         ('ui', 'خطای ۶ (قطع باتری) set است؟', 'side'): 'تمام؛ بقیهٔ سؤال‌ها این pass پرسیده نمی‌شوند',
         ('ui', 'باتری «خراب» قفل شده؟', 'side'): 'تمام؛ بقیهٔ سؤال‌ها این pass پرسیده نمی‌شوند',
         ('ui', 'عدم‌توازن قفل شده؟', 'side'): 'تمام؛ بقیهٔ سؤال‌ها این pass پرسیده نمی‌شوند',
         ('ui', 'شارژ کامل است؟', 'side'): 'تمام؛ چهرهٔ نمایش نهایی همین است',
         ('ui', 'استراحت حرارتی فعال است؟', 'side'): 'تمام؛ چهرهٔ نمایش نهایی همین است',
         ('ui', 'channelی در حال شارژ است؟', 'side'): 'تمام؛ چهرهٔ نمایش نهایی همین است',
         ('system', '۶) شارژر: فازها و فرمان درصد توان', 'note'): 'خط اصلی؛ بعد از دور آخر، چرخه از اول',
        }
        for nd in m['flow']:
            lp = nd.get('loop')
            if lp and isinstance(lp.get('to'), str):
                hit = [i3 for i3, n3 in enumerate(m['flow'])
                       if (n3.get('t') or '').startswith(lp['to'])]
                if len(hit) != 1:
                    raise SystemExit('loop target not unique: %s %r -> %s' % (key, lp['to'], hit))
                lp['to'] = hit[0]
        after_hit = set()
        ret_hit = set()
        for n in m['flow']:
            if n.get('ref'):
                n['rurl'] = resolve_ref(n['ref'], idx, fallback)
            RET_END = {
             ('charging', 'ذخیره در حافظهٔ ماندگار در حال انجام است؟'), ('charging', 'قفل خطای فنی (خطای ۷) از قبل set شده؟'),
             ('charging', 'mode test دستی پنل فعال است؟'), ('charging', 'اندازه‌گیری سالم و شارژ مجاز است؟'),
             ('charging', 'ولتاژ ورودی بین ۲۲ و ۲۸ ولت است؟'),
             ('charging', 'خطای نهایی (FINAL_FAULT) روی یکی از channelها هست؟'),
             ('fault', 'حالت تست دستی از پنل روشن است؟'), ('fault', 'عکس اندازه‌گیری معتبر است؟'),
             ('imbalance', 'ورودی‌ها معتبرند؟'), ('imbalance', 'هر دو باتری در یک وضعیت‌اند؟'),
             ('changeover', 'Snapshot معتبر است؟'),
             ('esplink', 'frame کامل و CRC درست است؟'), ('esplink', 'نسخه و نوع frame شناخته‌شده است؟'),
             ('esplink', 'مقدار داخل بازهٔ مجاز بود؟'),
             ('measurement', 'یک بستهٔ کامل و تازه از مبدل رسید؟'),
             ('callut', 'شمارش داخل بازهٔ جدول است؟'), ('callut', 'جدول تازه کامل و درست است؟'),
             ('mcupowerpath', 'لبهٔ افتادن حضور ورودی آمد؟ (وقفه)'), ('mcupowerpath', 'Snapshot معتبر است؟'),
             ('system', 'همهٔ taskها سر وقت اعلام زنده‌بودن کردند؟'),
             ('charging', 'channel از پنل خاموش شده؟'), ('charging', 'قفل باتری خراب یا بودجهٔ عدم‌توازن تمام شده؟'),
             ('charging', 'ولتاژ باتری بین ۲ و ۱۵ ولت است؟'), ('charging', 'جریان از ۹۵۰ میلی‌آمپر بالاتر رفته؟'),
             ('charging', 'اتصال باتری ۱۵ ثانیه پایدار بوده؟'),
             ('changeover', 'bit خطایی set است؟'), ('changeover', 'باتری برای خروجی ممنوع است؟'),
             ('ui', 'Snapshot معتبر است؟'), ('ui', 'خطای فنی (۷) set است؟'), ('ui', 'اضافه‌ولتاژ ورودی هست؟'),
             ('ui', 'خطای ۶ (قطع باتری) set است؟'), ('ui', 'باتری «خراب» قفل شده؟'), ('ui', 'عدم‌توازن قفل شده؟'),
             ('ui', 'شارژ کامل است؟'), ('ui', 'استراحت حرارتی فعال است؟'), ('ui', 'channelی در حال شارژ است؟'),
             ('ui', 'ورودی حاضر است؟'),
             ('imbalance', 'چرخهٔ شارژ با شارژ کامل تمام شد؟'),
             ('imbalance', 'در این cycle نامزد عدم‌توازن بود؟'),
             ('mcupowerpath', 'ولتاژ ورودی دست‌کم ۲۲ ولت است؟'), ('mcupowerpath', '۵ ثانیه پیوسته بالای ۲۲ ماند؟'),
             ('mcupowerpath', 'ورودی زیر ۲۱٫۵ ولت افتاد؟'),
             ('protection', 'Snapshot معتبر است؟'),
             ('rtos', 'taskی سر وقت اعلام زنده‌بودن نکرد؟'),
             ('rtosmap', 'همهٔ شکاف‌های watchdog زیر'),
             ('jitter', 'وقفهٔ trip channelی آمد؟'),
             ('esplink', 'درخواست نوشتن است؟'),
            }
            next_channel_idx = None
            if key == 'charging':
                for i2, n2 in enumerate(m['flow']):
                    if n2.get('k') == 'pill' and 'برای هر channel' in (n2.get('t') or ''):
                        next_channel_idx = i2
                        break
            NEXT_CHANNEL = {
                'channel از پنل خاموش شده؟',
                'قفل باتری خراب یا بودجهٔ عدم‌توازن تمام شده؟',
                'ولتاژ باتری بین ۲ و ۱۵ ولت است؟',
                'جریان از ۹۵۰ میلی‌آمپر بالاتر رفته؟',
                'اتصال باتری ۱۵ ثانیه پایدار بوده؟',
            }
            # [EN] An explicit retToText turns a "go to the next pass" box
            #      into a real link to the pass-start node, resolved by title
            #      so the index can never drift from the node list.
            # [FA] retToText صریح، کادر «برو پاس بعد» را به گرهٔ شروع همان پاس
            #      وصل می‌کند؛ با عنوان پیدا می‌شود تا شماره هیچ‌وقت جابه‌جا نشود.
            for kind in ('side', 'note'):
                s2 = n.get(kind)
                if s2 and s2.get('retToText'):
                    hits = [i3 for i3, n3 in enumerate(m['flow'])
                            if (n3.get('t') or '').startswith(s2['retToText'])]
                    if len(hits) != 1:
                        raise SystemExit('retToText not unique: %s %r -> %s'
                                         % (key, s2['retToText'], hits))
                    s2['retTo'] = hits[0]
                    s2['ret'] = 'NODE'
                    s2.pop('retToText', None)
            for kind in ('side', 'note'):
                if n.get(kind):
                    aft = None
                    for (k2, t2, kind2), v2 in AFTER.items():
                        if k2 == key and kind2 == kind and (n.get('t') or '').startswith(t2):
                            aft = v2
                            after_hit.add((k2, t2, kind2))
                            break
                    if aft:
                        n[kind]['after'] = aft
                    if (kind == 'side' and n[kind].get('lab') in ('بله', 'نه')
                            and not n[kind].get('ret')):
                        if (key == 'charging' and
                                (n.get('t') or '') in NEXT_CHANNEL and
                                next_channel_idx is not None):
                            n[kind]['ret'] = 'NODE'
                            n[kind]['retTo'] = next_channel_idx
                        else:
                            hit = [k2t2 for k2t2 in RET_END
                                   if k2t2[0] == key and (n.get('t') or '').startswith(k2t2[1])]
                            ret_hit.update(hit)
                            is_end = bool(hit)
                            n[kind]['ret'] = 'END' if is_end else 'NEXT'
                        n[kind].pop('after', None)
            for sub, tgt in GO.get(key, {}).items():
                if sub in (n.get('t') or ''):
                    if tgt == 'sim':
                        tp = os.path.join(ROOT, 'Firmware', 'Modules', 'Ui', 'alarm_simulator.html')
                    elif tgt.startswith('alarm:'):
                        tp = os.path.join(ROOT, 'Documentation', 'alarms',
                                          tgt.split(':', 1)[1] + '.html')
                    else:
                        tp = target_path(tgt)
                    n['go'] = os.path.relpath(tp, os.path.dirname(path)).replace(os.sep, '/')
                    break
            # [EN] A box may also name a page directly (`go='protection'`). Normalise
            #      those to a real relative path too, otherwise the browser opens a
            #      page that does not exist.
            # [FA] بعضی کادرها نام صفحه را مستقیم می‌دهند (`go='protection'`). آن‌ها
            #      هم باید به مسیر واقعی تبدیل شوند، وگرنه مرورگر صفحهٔ ناموجود باز
            #      می‌کند.
            g0 = n.get('go')
            if g0 and not g0.endswith('.html') and '/' not in g0:
                if g0.startswith('alarm:'):
                    tp = os.path.join(ROOT, 'Documentation', 'alarms',
                                      g0.split(':', 1)[1] + '.html')
                else:
                    tp = target_path(g0)
                n['go'] = os.path.relpath(tp, os.path.dirname(path)).replace(os.sep, '/')
        orphan = [k3 for k3 in AFTER if k3[0] == key and k3 not in after_hit]
        if orphan:
            raise SystemExit('AFTER title no longer exists on page %r: %r' % (key, orphan))
        titles = [(x.get('t') or '') for x in m['flow']]
        orphan2 = [k3 for k3 in RET_END if k3[0] == key
                   and not any(x.startswith(k3[1]) for x in titles)]
        orphan3 = [s3 for s3 in GO.get(key, {}) if not any(x.startswith(s3) for x in titles)]
        if orphan3:
            raise SystemExit('GO text no longer exists on page %r: %r' % (key, orphan3))
        if orphan2:
            raise SystemExit('RET_END title no longer exists on page %r: %r' % (key, orphan2))
        alarm_dir = os.path.join(ROOT, 'Documentation', 'alarms')

        def alarm_href(slug):
            return os.path.relpath(os.path.join(alarm_dir, slug + '.html'),
                                   os.path.dirname(path)).replace(os.sep, '/')

        errmap = {'۰': alarm_href('fault_0_adc'),
                  '۶': alarm_href('fault_6_battery_lost'),
                  '۷': alarm_href('fault_7_technical')}
        missing_go = [x.get('go') for x in m['flow'] if x.get('go') and
                      not os.path.exists(os.path.normpath(os.path.join(os.path.dirname(path), x['go'])))]
        if missing_go:
            raise SystemExit('page %r links to a missing file: %r' % (key, missing_go))
        mm = dict(m); mm['menu'] = build_menu(os.path.dirname(path), key); mm['errmap'] = errmap
        mm['gloss'] = [list(row) for row in GLOSSARY]
        mm = plain_module(mm)
        html = TEMPLATE.replace('__TITLE__', m['title'])
        html = html.replace('/*__MENUCSS__*/', MENU_CSS).replace('/*__MENUJS__*/', MENU_JS)
        html = html.replace('/*__DATA__*/', json.dumps(mm, ensure_ascii=False))
        with open(path, 'w', encoding='utf-8') as f:
            f.write(html)
        print('wrote', path)

if __name__ == '__main__':
    main()
