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
  .edge .hit { stroke: rgba(0,0,0,0); stroke-width: 16; fill: none; }
  .fnode { cursor: pointer; }
  svg.sel .edge, svg.sel .fnode { opacity: .12; }
  svg.sel .edge.hl, svg.sel .fnode.hl { opacity: 1; }
  .edge.hl .vis { stroke-width: 4.5; }
  .fnode.hl rect, .fnode.hl polygon { stroke-width: 4; }
  .fnode a text { text-decoration: underline; }
  .fnode a:hover text { fill: #B45309; }
  #einfo { background:#16283F; border:1px solid #35507A; border-radius:8px; padding:6px 12px; margin:4px 4px 8px; font-size:14.5px; }
  .subhead { cursor: pointer; }
  .subhead:hover rect { stroke-width: 3; }
  .tnode:hover rect { stroke-width: 3; }
</style>
</head>
<body>
<div class="wrap">
<h1 id="ttl"></h1>
<p class="small" id="sub"></p>
<p class="small" id="nav"></p>
<h2>توضیح ساده</h2>
<div class="card"><ol id="steps"></ol></div>
<div id="exwrap"><h2>یک مثال عددی</h2><div class="card ok"><ol id="example"></ol></div></div>
<h2>فلوچارت</h2>
<div class="card"><div id="flow"></div><p class="small" id="legend"></p></div>
<h2>درخت کد</h2>
<p class="small">درخت واقعی کد است؛ روی هر گره کلیک کن تا شاخه‌اش باز یا بسته شود.</p>
<div class="card"><div id="tree"></div></div>
<h2>شرط‌ها و حفاظت‌ها</h2>
<table id="conds"><tr><th style="width:26%">اگر این اتفاق افتاد</th><th style="width:37%">برنامه چه کار می‌کند</th><th>عددش</th></tr></table>
<h2>فرمان‌ها و خروجی‌ها</h2>
<table id="outs"><tr><th style="width:26%">به کجا</th><th style="width:37%">چه فرمانی / چه خروجی‌ای</th><th>کی و با چه شرطی</th></tr></table>
<div class="card" style="background:#2E2A18;border-color:#BF8F00"><b>مرحلهٔ بعدی:</b> این صفحه را با هم مرور می‌کنیم. هر جا عدد یا شرطی ایراد داشت، بگو کدام است؛ اول از روی کد چک می‌کنیم، بعد اصلاح می‌کنیم.</div>
</div>
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
var LEGEND = [['blue','در حال کار'],['green','کامل / سالم'],['yellow','سؤال و تصمیم'],['red','خطا و توقف'],['gray','صبر و مکث'],['navy','شروع و پایان']];
var cv = document.createElement('canvas').getContext('2d');
function tw(t, bold, sz){ cv.font = (bold ? '700 ' : '') + sz + 'px Vazirmatn, Tahoma'; return cv.measureText(t).width; }
function esc(s){ return s.replace(/&/g,'&amp;').replace(/</g,'&lt;'); }
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
  wrapLine('بعد: ' + s.after, false, 11.5, maxW).forEach(function(part){ out.push(part); });
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
  var sz = nodeSize(n), c = COLORS[n.c || 'blue'], x = cx - sz.w / 2, out = '<g class="fnode' + (n.go ? ' go' : '') + '" data-i="' + idx + '" data-t="' + esc(n.t || '') + '" data-go="' + esc(n.go || '') + '">';
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
      tt += '<text x="' + cx + '" y="' + (ty + i * 25 + 5) + '" text-anchor="middle" font-size="15" ' + (ln.b ? 'font-weight="700"' : '') + ' fill="' + c.t + '">' + esc(ln.s) + '</text>';
    }
  });
  var aw = (n.k === 'dia') ? sz.w * 0.60 : sz.w - 14;
  out += '<g class="fx" data-w="' + aw + '" data-cx="' + cx + '" data-cy="' + (y + sz.h / 2) + '">' + tt + '</g>';
  if (n.go) out += '<text x="' + (x + sz.w - 11) + '" y="' + (y + 18) + '" font-size="14" fill="#B45309">↗</text>';
  return out + '</g>';
}
var SIDEGEO = {};
function drawSide(s, nodeCx, nodeW, cy, dir, oIdx, oTitle, kind){
  SIDEGEO[oIdx + ':' + kind] = null;
  var sz = sideSize(s), c = COLORS[s.c || 'gray'];
  var x = (dir === 'r') ? 1390 - sz.w : 10;
  var y = cy - sz.h / 2, out = '';
  SIDEGEO[oIdx + ':' + kind] = { x: x, y: y, w: sz.w, h: sz.h, cy: cy, dir: dir };
  var x1 = (dir === 'r') ? nodeCx + nodeW / 2 : nodeCx - nodeW / 2;
  var x2 = (dir === 'r') ? x : x + sz.w;
  out += '<g class="edge" data-fi="' + oIdx + '" data-ti="" data-f="' + esc(oTitle || '') + '" data-t="' + esc(s.l[0] || '') + '" data-l="' + esc(s.lab || (s.d === 'l' ? 'موازی' : '')) + '">';
  out += '<path class="hit" d="M' + x1 + ' ' + cy + ' H' + x2 + '"/>';
  out += '<path class="vis" d="M' + x1 + ' ' + cy + ' H' + x2 + '" stroke="' + (LINES[s.c || 'gray']) + '" stroke-width="2" stroke-dasharray="6 4" fill="none" marker-end="url(#arw)"/>';
  var slab = s.lab || (s.d === 'l' ? 'موازی' : null);
  if (slab){
    var mxp = (x1 + x2) / 2, lw = tw(slab, true, 13.5);
    out += '<rect x="' + (mxp - lw / 2 - 5) + '" y="' + (cy - 9.5) + '" width="' + (lw + 10) + '" height="19" fill="#1B2029"/>';
    out += '<text x="' + mxp + '" y="' + (cy + 4.5) + '" text-anchor="middle" font-size="13.5" font-weight="700" fill="' + (LINES[s.c || 'gray']) + '">' + esc(slab) + '</text>';
  }
  out += '<rect x="' + x + '" y="' + y + '" width="' + sz.w + '" height="' + sz.h + '" rx="9" fill="' + c.f + '" stroke="' + c.o + '" stroke-width="1.6"/>';
  var sl = sideLines(s), st = '';
  sl.forEach(function(ln, i){
    st += '<text x="' + (x + sz.w / 2) + '" y="' + (y + sz.h / 2 - (sl.length - 1) * 11.5 + i * 23 + 5) + '" text-anchor="middle" font-size="13.5" fill="' + c.t + '">' + esc(ln) + '</text>';
  });
  var al = afterLines(s);
  al.forEach(function(ln, i){
    st += '<text x="' + (x + sz.w / 2) + '" y="' + (y + sz.h - al.length * 17 - 2 + i * 17 + 4) + '" text-anchor="middle" font-size="11.5" fill="#33506F">' + esc(ln) + '</text>';
  });
  out += '<g class="fx" data-w="' + (sz.w - 12) + '" data-cx="' + (x + sz.w / 2) + '" data-cy="' + (y + sz.h / 2) + '">' + st + '</g>';
  return out + '</g>';
}
function vlabel(x, y, s, col){
  var w = tw(s, true, 13.5) + 16;
  return '<g transform="translate(' + x + ',' + y + ') rotate(-90)"><rect x="' + (-w/2) + '" y="-11" width="' + w + '" height="22" fill="#1B2029"/><text x="0" y="4.5" text-anchor="middle" font-size="13.5" font-weight="700" fill="' + (col || '#D7E3F5') + '">' + esc(s) + '</text></g>';
}
function renderFlow(){
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
  var lastReal = -1; pos.forEach(function(p2, i2){ if (p2) lastReal = i2; });
  var needTerm = !(DATA.flow[lastReal] && DATA.flow[lastReal].k === 'pill');
  var termPos = null;
  if (needTerm){ termPos = { y: y, h: 44, w: 320, cy: y + 22, _term: true }; pos.push(termPos); y += 44 + GAP; }
  var H = y - GAP + 30;
  DATA.flow.forEach(function(n, i){
    if (!pos[i]) return;
    var p = pos[i];
    var pj = i - 1; while (pj > 0 && !pos[pj]) pj--;
    if (i > 0 && pos[pj]){
      var prevBottom = pos[pj].y + pos[pj].h;
      var pn = DATA.flow[pj];
      out += '<g class="edge" data-fi="' + pj + '" data-ti="' + i + '" data-f="' + esc(pn.t || '') + '" data-t="' + esc(n.t || '') + '" data-l="' + esc(pn.elab || '') + '">';
      out += '<path class="hit" d="M' + cx + ' ' + prevBottom + ' V' + p.y + '"/>';
      out += '<path class="vis" d="M' + cx + ' ' + prevBottom + ' V' + (p.y - 4) + '" stroke="#6FA8DC" stroke-width="2.2" fill="none" marker-end="url(#arw)"/>';
      if (pn.elab) out += vlabel(cx, (prevBottom + p.y) / 2, pn.elab, '#D7E3F5');
      out += '</g>';
    }
    if (n.k === 'sub'){ out += drawSub(n, cx, p.y, i); return; }
    out += drawNode(n, cx, p.y, i);
    if (n.side) out += drawSide(n.side, cx, p.w, p.cy, n.side.d || 'r', i, n.t, 'side');
    if (n.note) out += drawSide(n.note, cx, p.w, p.cy, n.note.d || 'l', i, n.t, 'note');
  });
  if (termPos) out += drawNode({ k: 'pill', c: 'navy', t: '↺ برگشت به اول — پاس بعد' }, cx, termPos.y, DATA.flow.length);
  var retK = 0;
  DATA.flow.forEach(function(n, i){
    if (!n.side || !n.side.ret) return;
    var g = SIDEGEO[i + ':side']; if (!g || g.dir !== 'r') return;
    var tgt = null;
    if (n.side.ret === 'END'){ tgt = termPos; }
    else { var j = i + 1; while (j < pos.length && !pos[j]) j++; tgt = pos[j]; }
    if (!tgt) return;
    var lane = g.x - 18 - (retK++ % 5) * 12;
    var dd = 'M' + (g.x + 8) + ' ' + (g.y + g.h) + ' V' + (g.y + g.h + 10) + ' H' + lane + ' V' + tgt.cy + ' H' + (cx + tgt.w / 2 + 4);
    out += '<path d="' + dd + '" stroke="' + (LINES[n.side.c || 'gray']) + '" stroke-width="1.8" fill="none" marker-end="url(#arw)"/>';
  });
  var maxHalf = 0; pos.forEach(function(p){ if (p) maxHalf = Math.max(maxHalf, p.w / 2); });
  loops.forEach(function(L, li){
    var a = pos[L.from], b = pos[L.to], lane = cx - maxHalf - 24 - li * 30;
    var LOOPCOLS = ['#6FA8DC', '#7BC47F', '#B18CFF', '#E3B341', '#FF9E64'];
    var col = L.col === 'red' ? '#FF8080' : LOOPCOLS[li % LOOPCOLS.length];
    var dstr = 'M' + (cx - a.w / 2) + ' ' + a.cy + ' H' + lane + ' V' + b.cy + ' H' + (cx - b.w / 2 - 4);
    out += '<g class="edge" data-fi="' + L.from + '" data-ti="' + L.to + '" data-f="' + esc(DATA.flow[L.from].t || '') + '" data-t="' + esc(DATA.flow[L.to].t || '') + '" data-l="' + esc(L.lab || '') + '">';
    out += '<path class="hit" d="' + dstr + '"/>';
    out += '<path class="vis" d="' + dstr + '" stroke="' + col + '" stroke-width="2.2" fill="none" marker-end="url(#arw)"/>';
    if (L.lab) out += vlabel(lane, (a.cy + b.cy) / 2, L.lab, col);
    out += '</g>';
  });
  document.getElementById('flow').innerHTML = '<div id="einfo">برای دیدن ارتباط هر خط، روی آن کلیک کن؛ روی هر باکس هم کلیک کنی، همهٔ خط‌های وصل به آن روشن می‌شوند. باکس‌های با علامت ↗ لینک صفحهٔ ماژول خودشان‌اند. برای برگشت، روی جای خالی شکل کلیک کن.</div><svg viewBox="0 0 ' + W + ' ' + H + '" width="66%" style="display:block">' + out + '</svg>';
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
  document.getElementById('tree').innerHTML = '<svg viewBox="0 0 1000 ' + (cursor.v + 16) + '" width="100%">' + out.join('') + '</svg>'; fitText();
  var nodes = document.querySelectorAll('.tnode');
  nodes.forEach(function(g){
    g.addEventListener('click', function(){
      var id = +g.getAttribute('data-tid'), found = null;
      (function find(n){ if (n._id === id) found = n; (n.c || []).forEach(find); })(DATA.tree);
      if (found && found.c && found.c.length){ found._open = !found._open; renderTree(); }
    });
  });
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
    var eg = ev.target.closest ? ev.target.closest('.edge') : null;
    var ng = ev.target.closest ? ev.target.closest('.fnode') : null;
    clear();
    if (eg){
      svg.classList.add('sel'); eg.classList.add('hl');
      var fi = eg.getAttribute('data-fi'), ti = eg.getAttribute('data-ti');
      var ns = svg.querySelectorAll('.fnode');
      for (var q = 0; q < ns.length; q++){ if (ns[q].getAttribute('data-i') === fi || ns[q].getAttribute('data-i') === ti) ns[q].classList.add('hl'); }
      if (info) info.textContent = 'ارتباط: از «' + eg.getAttribute('data-f') + '» به «' + eg.getAttribute('data-t') + '»' + (eg.getAttribute('data-l') ? ' — ' + eg.getAttribute('data-l') : '');
    } else if (ng){
      var go = ng.getAttribute('data-go');
      if (go){ location.href = go; return; }
      svg.classList.add('sel'); ng.classList.add('hl');
      var i2 = ng.getAttribute('data-i'), cnt = 0, es = svg.querySelectorAll('.edge');
      for (var q2 = 0; q2 < es.length; q2++){ if (es[q2].getAttribute('data-fi') === i2 || es[q2].getAttribute('data-ti') === i2){ es[q2].classList.add('hl'); cnt++; } }
      if (info) info.textContent = 'باکس «' + (ng.getAttribute('data-t') || '') + '» — ' + cnt + ' خطِ وصل به آن روشن شد. برای برگشت، روی جای خالی شکل کلیک کن.';
    } else {
      if (info) info.textContent = 'برای دیدن ارتباط هر خط، روی آن کلیک کن؛ روی هر باکس هم کلیک کنی، همهٔ خط‌های وصل به آن روشن می‌شوند. باکس‌های با علامت ↗ لینک صفحهٔ ماژول خودشان‌اند. برای برگشت، روی جای خالی شکل کلیک کن.';
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
  if (DATA.nav){ document.getElementById('nav').innerHTML = 'صفحه‌های دیگر: ' + DATA.nav.map(function(x){ return '<a href="' + x.href + '">' + x.t + '</a>'; }).join(' | '); }
  fill('steps', DATA.steps || []);
  if (DATA.example){ fill('example', DATA.example); } else { document.getElementById('exwrap').style.display = 'none'; }
  fillTable('conds', DATA.conds || []);
  fillTable('outs', DATA.outs || []);
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

def SIDE(c, l, d='r', lab=None):
    r = {'c': c, 'l': l, 'd': d}
    if lab: r['lab'] = lab
    return r

def T(t, *c):
    d = {'t': t}
    if c: d['c'] = list(c)
    return d

MODULES = {}

# ================= Charger (from charger.c, verified) =================
MODULES['charging'] = {
 'file': 'charging_process.html',
 'title': 'فرآیند شارژ — توضیح ساده',
 'sub': 'رفتار فعلی برد از روی کد شارژر؛ هر دستگاه دو باتری ۱۲ ولتی و برای هر باتری یک کانال شارژ جدا با رفتار یکسان دارد.',
 'steps': [
   '<b>شروع:</b> وقتی ورودی بین <b class="num">۲۲ تا ۲۸ ولت</b> باشد و باتری وصل باشد، شارژ می‌تواند شروع شود؛ وگرنه شارژر صبر می‌کند و جریانی نمی‌دهد.',
   '<b>آرام‌سازی:</b> هر بار باتری تازه وصل شود، <b class="num">۱۵ ثانیه</b> صبر می‌کنیم تا اتصال جا بیفتد؛ بعد شارژ آرام و بدون پرش شروع می‌شود.',
   '<b>شارژ سریع:</b> جریان کامل با سقف <b class="num">۶۵۰ میلی‌آمپر</b> تا ولتاژ باتری به <b class="num">۱۴٫۳ ولت</b> برسد؛ تنظیم با PID است.',
   '<b>جذب:</b> ولتاژ روی <b class="num">۱۴٫۴ ولت</b> ثابت می‌ماند و جریان کم‌کم پایین می‌آید؛ افت زیر ۱۴٫۳ یعنی برگشت به شارژ سریع و از اول شمردن ۱۰ دقیقه.',
   '<b>کامل‌شدن:</b> ۱۰ دقیقه در جذب + جریان زیر <b class="num">۵۰ میلی‌آمپر</b> برای <b class="num">۶۰ ثانیه</b>؛ یا سقف امن ۱ ساعت.',
   '<b>نگه‌داری:</b> باتری روی <b class="num">۱۳٫۵ ولت</b> می‌ماند و duty صفر است؛ افت زیر <b class="num">۱۲٫۸ ولت</b> یعنی شارژ از نو.',
 ],
 'example': [
   'باتری ۱۱٫۹ ولتی وصل می‌شود؛ ۱۵ ثانیه آرام‌سازی، بعد شارژ سریع با حدود ۶۴۰ میلی‌آمپر.',
   'ولتاژ بالا می‌رود تا ۱۴٫۳؛ وارد جذب می‌شود و جریان از ۶۰۰ به زیر ۵۰ میلی‌آمپر می‌آید.',
   'بعد از ۱۰ دقیقه و ۶۰ ثانیه ثبات، شارژ کامل است و باتری روی ۱۳٫۵ ولت نگه داشته می‌شود.',
 ],
 'flow': [
   N('sec','navy','بخش ۱: بررسی‌های کلی — در هر پاس، قبل از شارژ'),
   N('pill','navy','شروع پاس شارژر', ref='func__Charger_Evaluate'),
   N('dia','yellow','ذخیرهٔ حافظه (NVM) در حال انجام است؟', side=SIDE('gray',['هر دو خروجی شارژر صفر می‌شود','از پاس بعد، شارژ دوباره از اول شروع می‌شود'], 'r', 'بله'), elab='نه'),
   N('dia','yellow','قفل خطای فنی (خطای ۷) از قبل ست شده؟', ref='FinalDisconnect', side=SIDE('red',['قطع نهایی: رله باز و هر دو خروجی صفر می‌شوند','این قفل تا خاموش و روشن‌شدن برد سر جایش می‌ماند'], 'r', 'بله'), elab='نه'),
   N('dia','yellow','مود تست دستی پنل فعال است؟', ref='EnterManualTestMode', side=SIDE('gray',['duty را پنل می‌فرستد؛ گیت‌های باتری کنار می‌روند','اگر لینک ۳ ثانیه ساکت بماند، شارژر خودش از مود دستی بیرون می‌آید (dead-man)'], 'r', 'بله'), elab='نه'),
   N('dia','yellow','سیم باتری قطع است؟ (خطای ۶)', ref='func__Fault_Get: آینهٔ بیت خطا', side=SIDE('red',['آن کانال متوقف و خروجی‌اش صفر می‌شود','با برگشت باتری و پاک‌شدن خطا، کانال دوباره آزاد می‌شود'], 'r', 'بله'), elab='نه'),
   N('dia','yellow','اندازه‌گیری سالم و حالت کلی عادی است؟', side=SIDE('gray',['توقف امن: همهٔ خروجی‌ها صفر می‌شوند','قفل‌های خطا (نهایی، قطع باتری و حرارتی) سر جایشان می‌مانند'], 'r', 'نه'), elab='بله'),
   N('dia','yellow','ولتاژ ورودی بین ۲۲ و ۲۸ ولت است؟', ref='SafeIdle + INPUT_WAIT', side=SIDE('gray',['شارژر صبر می‌کند (INPUT_WAIT) و شارژ صفر است','با درست‌شدن ورودی، شارژ خودش ادامه پیدا می‌کند'], 'r', 'نه'),
     note=SIDE('red',['بررسی جدا و موازی: خطای فنی (۷)','نشانهٔ ۱: duty بالای ۲۰٪ است ولی جریان همان کانال صفر است','نشانهٔ ۲: رله باز است، duty صفر است و تریپ JIT ثبت شده است','نتیجه: قفل فنی، ست‌شدن خطای ۷ و ثبت در دیاگ'],'l'), elab='بله'),
   N('sec','navy','بخش ۲: بررسی هر کانال (۱ و ۲ جدا)'),
   N('pill','navy','برای هر کانال', ref='func__Charger_Evaluate: حلقهٔ کانال‌ها'),
   N('dia','yellow','کانال از پنل خاموش شده؟', ref='RegulateChannel: گیت ESP', side=SIDE('gray',['این کانال خاموش می‌ماند و خروجی‌اش صفر است','از پنل وصل شود، با شروع نرم از duty یک‌درصد شروع می‌کند'], 'r', 'بله'), elab='نه'),
   N('dia','yellow','قفل باتری خراب یا بودجهٔ عدم‌توازن تمام شده؟', ref='DeadMask / Imbalance_GetOutputs', side=SIDE('red',['شارژ این کانال بسته است و خروجی‌اش صفر است','باتری خراب: تا تعویض باتری؛ عدم‌توازن: تا پایان بودجه'], 'r', 'بله'), elab='نه'),
   N('dia','yellow','ولتاژ باتری بین ۲ و ۱۵ ولت است؟', ref='BatteryVoltageIsValid', side=SIDE('gray',['کانال خاموش و منتظر می‌ماند','همین جلوی شارژِ باتریِ قطع یا کاملاً خراب را می‌گیرد'], 'r', 'نه'), elab='بله'),
   N('dia','yellow','جریان از ۹۵۰ میلی‌آمپر بالاتر رفته؟', ref='ResetChannelToOff', side=SIDE('red',['ریست کانال به خاموشی؛ شروع بعدی از شروع نرم','حفاظت سریعِ اضافه‌جریان با JIT سخت‌افزاری است'], 'r', 'بله'), elab='نه'),
   N('sec','navy','بخش ۳: فازهای شارژ'),
   N('dia','yellow','اتصال باتری ۱۵ ثانیه پایدار بوده؟', ref='BulkStartSettled', side=SIDE('gray',['کانال خاموش می‌ماند','شمارش از لحظهٔ دیده‌شدن ولتاژ باتری شروع می‌شود'], 'r', 'نه'), elab='بله'),
   N('box','blue','شارژ سریع (BULK)', ['شروع نرم از duty یک‌درصد','PID جریان را تا سقف ۶۵۰ میلی‌آمپر نگه می‌دارد'], ref='RegulateChannel → PidStep → ApplyDuty',
     side=SIDE('gray',['داغ‌شدن: ۱ ساعت نزدیکِ سقف جریان','۱۰ دقیقه استراحت (زرد چشمک‌زن) و بعد ادامه','این شمارش فقط در شارژ سریع و نزدیکِ سقف جریان انجام می‌شود']),
     note=SIDE('gray',['تریپ JIT: توقف فوری همان کانال','احیا پس از ۳ ثانیه، یکی‌یکی، با duty کمتر','تریپ سومِ همان کانال = خطای نهایی و قطع رله'],'l')),
   N('dia','yellow','ولتاژ به ۱۴٫۳ رسید؟', loop={'to': 16, 'lab': 'نه'}, elab='بله'),
   N('box','blue','جذب (ABSORB)', ['نگه‌داری ۱۴٫۴ ولت با PID؛ سقف پشتیبان ۱۴٫۸ ولت','بالای ۱۴٫۶ ولت: duty پله‌ای کم می‌شود'], ref='RegulateChannel: پنجرهٔ جذب',
     note=SIDE('red',['شمارش موازیِ باتری خراب (سناریوی ۶)','این شمارنده فقط وقتی می‌شمارد که کانال واقعاً در حال شارژ باشد','اگر شارژ کامل شود یا مکث از ۱ دقیقه بیشتر شود، شمارنده صفر می‌شود','اگر به ۲۴ ساعتِ بی‌نتیجه برسد، باتری خراب قفل می‌شود (بخش ۲)'],'l'),
     loop={'to': 16, 'lab': 'بله: زیر ۱۴٫۳، برگرد به شارژ سریع', 'col': 'red'}),
   N('dia','yellow','کامل شد؟ ۱۰ دقیقه در جذب و جریان زیر ۵۰ میلی‌آمپر برای ۶۰ ثانیه — یا سقف امنِ ۱ ساعت (پس از کم‌شدن جریان)', loop={'to': 18, 'lab': 'نه'}, elab='بله'),
   N('box','green','نگه‌داری (FLOAT) — شارژ کامل', ['duty صفر است و باتری روی ۱۳٫۵ ولت نگه داشته می‌شود','سبز ثابت و زرد خاموش می‌شود'], ref='RegulateChannel: پارک duty'),
   N('dia','yellow','ولتاژ زیر ۱۲٫۸ افتاد؟ (باتری دارد خالی می‌شود)', loop={'to': 16, 'lab': 'بله: شارژ از نو'}, elab='نه'),
   N('pill','navy','باتری آمادهٔ کار'),
   N('sec','navy','بخش ۴: فرمان‌ها و خروجی‌ها — شارژر به کجا فرمان می‌دهد'),
   N('box','blue','فرمان شارژ: duty حساب‌شده با گیرهٔ سقف', ['duty به PWM هر کانال می‌رود؛ در همهٔ توقف‌ها صفر است'], ref='ApplyDuty → BspPwm_SetDutyPermille'),
   N('box','navy','رلهٔ قطع نهایی در خطای نهایی باز می‌شود', ['با قطع ورودی، کویل رها می‌شود تا باتری خالی نشود'], ref='FinalDisconnect → BspGpio RELAY'),
   N('box','green','اطلاع به چراغ و بوق، پنل و نمایش وضعیت', ['وتوی خروجی دستگاه وقتی باتری خراب قفل است'], ref='IsChargeComplete / DeadBlocksOutput'),
 ],
 'tree': T('ارزیابی شارژر — هر پاس کنترل',
   T('گیت‌های کلی (به همین ترتیب)',
     T('ذخیرهٔ NVM؟ → خروجی صفر، پایان پاس'),
     T('قفل فنی؟ → قطع نهایی'),
     T('مود دستی: درخواست پنل + dead-man ۳ ثانیه'),
     T('آینهٔ خطای ۶: توقف کانال قطع‌باتری'),
     T('ثبت وضعیت زنده برای پنل و دیاگ'),
     T('اندازه‌گیری/حالت کلی بد؟ → توقف امن'),
     T('خطای نهایی؟ → قطع نهایی'),
     T('ورودی بد؟ → صبر برای ورودی'),
     T('تریپ JIT: توقف و شمارش؛ سومی = نهایی'),
     T('احیای JIT: یکی‌یکی بعد از ۳ ثانیه')),
   T('برای هر کانال (۱ و ۲)',
     T('شمارش باتری خراب (سناریوی ۶)'),
     T('استراحت حرارتی ۱ ساعت → ۱۰ دقیقه'),
     T('مود دستی؟ → duty پنل'),
     T('تنظیم عادی کانال',
       T('گیت‌ها: پنل / باتری خراب / عدم‌توازن'),
       T('ولتاژ باتری خارج از ۲ تا ۱۵؟ → خاموش'),
       T('جریان بالای ۹۵۰؟ → ریست به خاموش'),
       T('۱۵ ثانیه پایداری → شارژ سریع از ۱٪'),
       T('شارژ سریع (PID، سقف ۶۵۰)'),
       T('جذب ۱۴٫۴: افت = برگشت + ریست شمارش'),
       T('کامل: ۱۰ دقیقه + ۵۰ میلی‌آمپرِ ۶۰ ثانیه'),
       T('نگه‌داری ۱۳٫۵: افت زیر ۱۲٫۸ = از نو'),
       T('فرمان: duty با گیرهٔ سقف → PWM'))),
   T('بررسی موازی: خطای فنی (۷) → قفل + دیاگ'),
   T('خروجی‌ها: PWM، رله، چراغ/بوق، وتو، پنل')),
 'conds': [
   ['ورودی از ۲۲ ولت پایین‌تر یا از ۲۸ ولت بالاتر رفت','شارژ صفر می‌شود و برد در صبر می‌ماند؛ برگشت خودش ادامه می‌دهد.','ورودی معتبر: ۲۲ تا ۲۸ ولت'],
   ['سیم باتری وسط شارژ کشیده شد','<b>خطای ۶:</b> شارژ همان لحظه صفر، چراغ قرمز چشمک و سه بوق؛ با وصل دوباره و ۱۵ ثانیه آرام‌سازی به حالت عادی برمی‌گردد.','قطع باتری (خطای ۶)'],
   ['کانال شارژ جریان نمی‌دهد (خرابی داخلی)','<b>خطای ۷:</b> قفل فنی؛ رله و خروجی‌ها قطع تا خاموش و روشن‌شدن برد.','duty بالای ۲۰٪ و جریان صفر؛ سقف خطای سخت ۹۵۰ میلی‌آمپر'],
   ['ولتاژ باتری در جذب زیر ۱۴٫۳ افتاد','برمی‌گردد به شارژ سریع و شمارش ۱۰ دقیقه از اول شروع می‌شود.','۱۴٫۳ ولت'],
   ['یک ساعت پشت‌سرهم نزدیک سقف جریان رفت','۱۰ دقیقه استراحت؛ چراغ زرد چشمک می‌زند؛ بعد شارژ ادامه دارد.','۱ ساعت کار، ۱۰ دقیقه استراحت'],
   ['شارژ ۲۴ ساعت فعال بود ولی باتری کامل نشد','<b>باتری خراب:</b> شارژ قفل، قرمز ثابت؛ پاک‌شدن فقط با نبودن باتری برای ۳ ثانیه.','پیش‌فرض ۲۴ ساعت (پارامتر ۱۲۵)؛ مکث بلندتر از ۱ دقیقه شمارش را صفر می‌کند (۱۲۶)'],
   ['جریان ناگهانی (تریپ JIT)','کانال فوری می‌ایستد؛ احیا یکی‌یکی بعد از ۳ ثانیه با duty کمتر؛ تریپ سوم همان کانال = خطای نهایی و قطع رله.','قفل ۳ ثانیه'],
   ['باتری در نگه‌داری زیر ۱۲٫۸ افتاد','شارژ از اول شروع می‌شود (شارژ سریع).','۱۲٫۸ ولت'],
 ],
 'outs': [
   ['PWM هر کانال','duty حساب‌شدهٔ PID با گیرهٔ سقف مجاز','در هر پاس؛ در همهٔ توقف‌ها صفر'],
   ['رلهٔ قطع نهایی','بازکردن رله و قطع فیزیکی ورودی ترانس','خطای نهایی (تریپ سوم JIT) یا قفل فنی (۷)؛ تا پایان همین روشن‌بودن'],
   ['چراغ‌ها و بوق','وضعیت کانال: شارژ، کامل (سبز ثابت)، استراحت (زرد چشمک‌زن)، باتری خراب (قرمز ثابت + بوق جدا)','«شارژ کامل» فقط وقتی همهٔ کانال‌های فعال در نگه‌داری‌اند'],
   ['تعویض خروجی (Changeover)','وتوی خروجی باتریِ «خراب» قفل‌شده','تا پاک‌شدن قفل با تعویض واقعی باتری'],
   ['پنل و دیاگ','وضعیت فاز و duty هر کانال، شمارندهٔ باتری خراب، ثبت «شارژ کامل» و خطاها','در هر پاس؛ پنل پارامترها را هم از همین راه تنظیم می‌کند'],
 ],
}


MODULES['measurement'] = {
 'file': 'measurement_process.html',
 'title': 'فرآیند اندازه‌گیری — توضیح ساده',
 'sub': 'این ماژول هر ۱ میلی‌ثانیه ولتاژها و جریان‌ها را از مبدل آنالوگ می‌خواند، تمیزشان می‌کند و یک «عکس» معتبر به همهٔ ماژول‌ها می‌دهد.',
 'steps': [
   '<b>خواندن:</b> شمارش‌های خام از مبدل آنالوگ با DMA می‌آیند؛ هر پاس فقط یک فریم کامل و پایدار قبول است.',
   '<b>گرم‌شدن:</b> ۳ فریم اول فقط برای جاافتادن است و خروجی هنوز «نامعتبر» حساب می‌شود.',
   '<b>تمیزکردن جریان:</b> اول مدین (پرش‌های تک‌نمونه‌ای را می‌کشد)، بعد میانگین متحرک با پنجرهٔ پیش‌فرض ۱۰ نمونه؛ هر دو از پنل قابل تنظیم‌اند.',
   '<b>تمیزکردن ولتاژ:</b> مدین ۵ روی شمارش خام باتری‌ها؛ ولتاژ ورودی بدون فیلتر می‌ماند تا سریع دیده شود.',
   '<b>تبدیل:</b> شمارش با جدول کالیبراسیون به میلی‌ولت و میلی‌آمپر تبدیل می‌شود؛ آفست تنظیم از پنل حداکثر ±۵ ولت است.',
   '<b>نیم‌های باتری:</b> نیم پایین همان باتری ۱۲ ولت است و نیم بالا = کل ۲۴ ولت منهای ۱۲ ولت.',
   '<b>انتشار:</b> عکس کامل زیر قفل هسته منتشر می‌شود و بیت «معتبر» آخر از همه ست می‌شود تا هیچ ماژولی دادهٔ نیمه‌کاره نبیند.',
 ],
 'example': [
   'شارژر دارد با جریان پله‌ای کار می‌کند و یک نمونهٔ جریان ناگهان ۳ آمپر پرش می‌کند.',
   'مدین آن پرش تک‌نمونه‌ای را حذف می‌کند؛ میانگین ۱۰تایی هم موج جریان را صاف می‌کند.',
   'پس شارژر و بقیه عدد آرام و درست را می‌بینند و بی‌جهت واکنش نشان نمی‌دهند.',
 ],
 'flow': [
   N('sec','navy','بخش ۱: گرفتن فریم و اعتبار'),
   N('pill','navy','شروع پاس اندازه‌گیری (هر ۱ میلی‌ثانیه)', ref='func__Measurement_Run'),
   N('dia','yellow','فریم کامل و پایدار از مبدل رسید؟', ref='BspAdc_GetRaw', side=SIDE('red',['Snapshot این پاس «نامعتبر» اعلام می‌شود','گرم‌شدن از اول شروع می‌شود تا داده قابل اعتماد شود'], 'r', 'نه'), elab='بله', note=SIDE('gray',['DMA پیوسته فریم تازه می‌آورد','قطع DMA یعنی فریم کهنه و نامعتبر'],'l')),
   N('box','blue','شمارش فریم‌های پایدار', ['سه فریم اول دورهٔ گرم‌شدن است؛ خروجی هنوز نامعتبر است'], ref='func__Measurement_Run: warmup'),
   N('dia','yellow','تنظیم فیلتر از پنل عوض شده؟', side=SIDE('gray',['فیلترها ریست می‌شوند تا نمونهٔ کهنه با تنظیم تازه قاطی نشود'], 'r', 'بله'), elab='نه'),
   N('sec','navy','بخش ۲: تمیزکردن و کالیبراسیون'),
   N('box','blue','تمیزکردن جریان‌ها روی count خام', ['مدین ضد پرش تک‌نمونه‌ای','میانگین متحرک (پنجرهٔ پیش‌فرض ۱۰ نمونه)'], ref='ApplyCurrentFilters'),
   N('box','blue','تمیزکردن ولتاژ باتری‌ها', ['مدین ۵ روی شمارش خام','ولتاژ ورودی بدون فیلتر می‌ماند'], ref='func__Measurement_Run: مدین ولتاژ'),
   N('box','blue','تبدیل با جدول کالیبراسیون', ['شمارش → میلی‌ولت و میلی‌آمپر','آفست از پنل داده می‌شود، حداکثر ±۵ ولت'], ref='func__Measurement_CountsToMv'),
   N('box','blue','ساختن نیم‌های باتری', ['نیم پایین = باتری ۱۲ ولت','نیم بالا = کل ۲۴ ولت منهای نیم پایین (کمتر از صفر نمی‌شود)'], ref='func__Measurement_Run: ساختن نیم‌ها'),
   N('box','blue','خواندن حضور ورودی از پایهٔ منطقی برد', [], ref='func__BspGpio_Read: حضور ورودی'),
   N('sec','navy','بخش ۳: خروجی Snapshot برای بقیهٔ ماژول‌ها'),
   N('box','green','انتشار Snapshot زیر قفل', ['بیت «معتبر» آخر از همه ست می‌شود','همهٔ ماژول‌ها همین عکس را می‌خوانند'], ref='Measurement_GetSnapshot'),
 ],
 'tree': T('اندازه‌گیری — هر ۱ میلی‌ثانیه',
   T('گرفتن فریم کامل DMA',
     T('فریم نیست؟ → نامعتبر + گرم‌شدن از نو')),
   T('گرم‌شدن: ۳ فریم پایدار'),
   T('تغییر تنظیم فیلتر؟ → ریست فیلترها'),
   T('زنجیرهٔ جریان: مدین + میانگین متحرک'),
   T('زنجیرهٔ ولتاژ: مدین ۵'),
   T('تبدیل با جدول کالیبراسیون + آفست'),
   T('نیم پایین و نیم بالا'),
   T('انتشار عکس زیر قفل؛ معتبر آخر')),
 'conds': [
   ['فریم مبدل نرسید یا ناپایدار بود','خروجی «نامعتبر» می‌شود و بقیهٔ ماژول‌ها روی دادهٔ قبلی تصمیم تازه نمی‌گیرند.','۳ فریم گرم‌شدن'],
   ['یک نمونهٔ جریان پرش بزرگ داشت','مدین آن را حذف می‌کند؛ میانگین متحرک هم موج را صاف می‌کند.','پنجرهٔ میانگین پیش‌فرض ۱۰ (تا ۳۰۰ از پنل)'],
   ['تنظیم فیلتر وسط کار از پنل عوض شد','فیلترها همان‌جا ریست می‌شوند؛ نمونهٔ کهنه وارد محاسبهٔ تازه نمی‌شود.','بدون قفل بین تسک‌ها'],
   ['آفست کالیبراسیون از پنل داده شد','بعد از تبدیل اعمال می‌شود و هیچ‌وقت از سقف رد نمی‌شود.','حداکثر ±۵۰۰۰ میلی‌ولت'],
 ],
 'outs': [
   ['عکس اندازه‌گیری (Snapshot)','ولتاژ ورودی، دو نیم باتری، جریان هر کانال و بیت معتبر','شارژر، فالت، عدم‌توازن، تعویض خروجی، چراغ‌ها و پنل همه از همین Snapshot را می‌خوانند'],
   ['تنظیمات فیلتر از پنل','اندازهٔ مدین و پنجرهٔ میانگین و آفست‌های ولتاژ','از پنل می‌آید و در پاس بعدی اندازه‌گیری اعمال می‌شود'],
 ],
}

MODULES['fault'] = {
 'file': 'fault_process.html',
 'title': 'فرآیند خطاها — توضیح ساده',
 'sub': 'این ماژول نگهبان قطع‌شدن باتری است: فقط او تصمیم می‌گیرد «خطای ۶» ست شود یا پاک شود؛ شارژر و چراغ‌ها از تصمیم او پیروی می‌کنند.',
 'steps': [
   '<b>قانون ۱ (در حال شارژ):</b> اگر شارژر روی نیمی در حال شارژ است و همان نیم بالای <b class="num">۱۴٫۸ ولت</b> رفت و <b class="num">۱۵۰ میلی‌ثانیه</b> ماند، یعنی سیم آن نیم قطع است.',
   '<b>قانون ۲ (نبودن):</b> اگر ورودی سالم است و هر کدام از نیم‌ها زیر <b class="num">۶ ولت</b> افتاد و <b class="num">۱ ثانیه</b> ماند، باتری قطع است.',
   '<b>بازیابی:</b> وقتی هر دو نیم دست‌کم <b class="num">۷ ولت</b> شدند و ۱ ثانیه پایدار ماندند، خطا پاک می‌شود (۷ به‌جای ۶ یعنی هیسترزیس تا باتری خالیِ وصل خطا نگیرد).',
   '<b>فقط نیم‌های نصب‌شده</b> قضاوت می‌شوند؛ نیمی که کانالش نصب نیست هیچ‌وقت «قطع» حساب نمی‌شود.',
   'در مود تست دستی یا وقتی اندازه‌گیری نامعتبر است، همهٔ شمارش‌ها <b>فریز</b> می‌شوند؛ نه خطای تازه ست می‌شود نه خطا پاک.',
 ],
 'example': [
   'وسط شارژ سیم باتری پایین کشیده می‌شود؛ شارژر روی آن نیم در حال شارژ است و ولتاژش به ۱۵ ولت می‌پرد.',
   'بعد از ۱۵۰ میلی‌ثانیه ماندن بالای ۱۴٫۸، خطای ۶ ست می‌شود: شارژر متوقف، چراغ قرمز چشمک و بوق می‌آید.',
   'سیم را برمی‌گردانی؛ هر دو نیم بالای ۷ ولت می‌ایستند و بعد از ۱ ثانیه خطا پاک و شارژ با آرام‌سازی ادامه پیدا می‌کند.',
 ],
 'flow': [
   N('sec','navy','بخش ۱: گیت‌ها و نیم‌های نصب‌شده'),
   N('pill','navy','شروع ارزیابی خطاها (هر پاس)', ref='func__Fault_Evaluate'),
   N('dia','yellow','مود تست دستی پنل فعال است؟', ref='EnterManualTestMode', side=SIDE('gray',['همهٔ شمارش‌ها فریز می‌شوند','در مود دستی نه خطای تازه‌ای ست می‌شود، نه خطایی پاک'], 'r', 'بله'), elab='نه'),
   N('dia','yellow','Snapshot معتبر است؟', side=SIDE('gray',['همه‌چیز فریز می‌شود؛ با برگشت Snapshot، شمارش از صفر شروع می‌شود'], 'r', 'نه'), elab='بله'),
   N('box','blue','مشخص می‌شود کدام نیم‌ها نصب‌اند', ['نیمی که کانالش نصب نیست، قضاوت نمی‌شود'], ref='CHG_INSTALLED_CHANNEL_MASK: نیم‌های نصب‌شده'),
   N('sec','navy','بخش ۲: دو قانون خطای ۶'),
   N('dia','yellow','قانون ۱: نیمِ در حال شارژ بالای ۱۴٫۸ ولت رفت و ۱۵۰ms ماند؟', ref='Fault_Evaluate: قانون ۱', side=SIDE('red',['ست‌کردن خطای ۶ (قطع باتری)'], 'r', 'بله'), elab='نه'),
   N('dia','yellow','قانون ۲: ورودی سالم است و یک نیم زیر ۶ ولت افتاد و ۱ ثانیه ماند؟', ref='Fault_Evaluate: قانون ۲', side=SIDE('red',['ست‌کردن خطای ۶ (قطع باتری)'], 'r', 'بله'), elab='نه'),
   N('sec','navy','بخش ۳: بازیابی و خروجی'),
   N('dia','yellow','بازیابی: هر دو نیم بالای ۷ ولت و بدون شارژ، ۱ ثانیه ماندند؟', ref='Fault_Clear', side=SIDE('green',['پاک‌کردن خطای ۶','شارژر کانال را آزاد می‌کند و با ۱۵ ثانیه آرام‌سازی برمی‌گردد'], 'r', 'بله'), elab='نه'),
   N('box','green','خروجی: بیت خطای ۶', ['شارژر کانال را متوقف می‌کند','حالت کلی دستگاه خطا می‌شود و چراغ/بوق خطای ۶ می‌آید'], note=SIDE('gray',['شارژر همین بیت را می‌خواند','و کانال همان نیم را متوقف می‌کند'],'l'), ref='func__Fault_Set: بیت خطای ۶'),
 ],
 'tree': T('ارزیابی خطاها — هر پاس',
   T('مود دستی؟ → فریز همه‌چیز'),
   T('Snapshot نامعتبر؟ → فریز و صفر شمارش'),
   T('نیم‌های نصب‌شده مشخص می‌شوند'),
   T('قانون ۱: در حال شارژ + بالای ۱۴٫۸ (۱۵۰ms) → خطای ۶'),
   T('قانون ۲: ورودی سالم + زیر ۶ ولت (۱s) → خطای ۶'),
   T('بازیابی: هر دو ≥ ۷ ولت (۱s) → پاک خطای ۶'),
   T('خروجی بیت خطای ۶ به شارژر و حالت کلی')),
 'conds': [
   ['نیمِ در حال شارژ بالای ۱۴٫۸ ولت پرید','یعنی سیم همان نیم قطع است؛ بعد از ۱۵۰ میلی‌ثانیه خطای ۶ ست می‌شود.','۱۴٫۸ ولت / ۱۵۰ میلی‌ثانیه (از پنل قابل تنظیم)'],
   ['نیمی زیر ۶ ولت افتاد و ورودی سالم بود','باتری قطع حساب می‌شود؛ بعد از ۱ ثانیه خطای ۶ ست می‌شود.','۶ ولت / ۱ ثانیه'],
   ['باتری برگشت و هر دو نیم بالای ۷ ولت ایستادند','بعد از ۱ ثانیه پایداری خطا پاک می‌شود.','۷ ولت (هیسترزیس ۱ ولت) / ۱ ثانیه'],
   ['اندازه‌گیری نامعتبر یا مود تست دستی','هیچ تصمیمی گرفته نمی‌شود؛ شمارش‌ها فریز یا صفر می‌شوند.','—'],
 ],
 'outs': [
   ['بیت خطای ۶','ست یا پاک‌شدن قطع باتری','شارژر کانال را متوقف/آزاد می‌کند؛ دستگاه به حالت خطا می‌رود؛ چراغ قرمز چشمک و بوق می‌آید'],
   ['پارامترهای آستانه از پنل','آستانهٔ قطع، نبودن، برگشت و زمان‌های پایداری (شناسه‌های ۲۷ تا ۳۴)','از پنل می‌آید؛ بازه‌ها در خود ماژول گیره می‌شوند'],
 ],
}

MODULES['imbalance'] = {
 'file': 'imbalance_process.html',
 'title': 'فرآیند عدم‌توازن باتری — توضیح ساده',
 'sub': 'این ماژول اختلاف ولتاژ دو نیم باتری را زیر نظر می‌گیرد؛ اگر اختلاف زیاد و ماندگار شد «رویداد» ثبت می‌کند و با تکرار، قفل عدم‌توازن می‌زند.',
 'steps': [
   '<b>اختلاف:</b> قدرمطلق اختلاف دو نیم باتری حساب می‌شود؛ فقط وقتی هر دو نیم هم‌حالت‌اند (هر دو در شارژ یا هر دو در استراحت) مقایسه معنا دارد.',
   '<b>سه پنجره:</b> هنگام دشارژ فوری با حد <b class="num">۵۰۰ میلی‌ولت</b>؛ حین شارژ و در استراحت با حد <b class="num">۳۰۰ میلی‌ولت</b> و بعد از زمان انتظار خودش.',
   '<b>ماندگاری:</b> اختلاف باید به اندازهٔ زمان پایداری بالای حد بماند تا «نامزد رویداد» شود؛ پرش‌های کوتاه حساب نیستند.',
   '<b>ثبت رویداد:</b> فقط در پایان یک سیکل کامل (شارژی که به کامل‌شدن رسید) رویداد ثبت می‌شود؛ شارژ نیمه‌کاره نه رویداد است نه پاک‌شدن.',
   '<b>قفل:</b> وقتی رویدادها به حد قفل (پیش‌فرض <b class="num">۱۰ رویداد</b>) رسیدند، قفل می‌خورد: چراغ قرمز چشمک‌زن و بوق دوره‌ای؛ وتوی خروجی و بودجهٔ سیکل شارژ هم از همین‌جا می‌آید.',
   '<b>پاک‌شدن:</b> ۳ سیکل کاملِ بی‌رویداد فقط شمارندهٔ رویداد را صفر می‌کند؛ تعویض واقعی باتری (۳ ثانیه نبودن) همه‌چیز حتی قفل را پاک می‌کند.',
 ],
 'example': [
   'نیم بالا ۱۲٫۹ ولت و نیم پایین ۱۲٫۴ ولت است؛ اختلاف ۵۰۰ میلی‌ولت.',
   'دستگاه روی باتری دارد کار می‌کند (دشارژ)؛ حد دشارژ ۵۰۰ است و اختلاف بالای حد می‌ماند.',
   'اگر این سیکل تا کامل‌شدن شارژ رفته باشد، یک رویداد ثبت می‌شود؛ با رسیدن رویدادها به ۱۰، قفل عدم‌توازن می‌خورد.',
 ],
 'flow': [
   N('sec','navy','بخش ۱: مقایسهٔ دو نیم باتری'),
   N('pill','navy','شروع ارزیابی عدم‌توازن (هر پاس)', ref='func__Imbalance_Evaluate'),
   N('dia','yellow','ورودی‌ها معتبرند؟', side=SIDE('gray',['همهٔ تایمرها فریز می‌شوند و جلو نمی‌روند','قضاوت تازه‌ای نمی‌شود'], 'r', 'نه'), elab='بله'),
   N('box','blue','اختلاف دو نیم باتری حساب می‌شود (قدرمطلق)', [], ref='func__Imbalance_Evaluate: اختلاف'),
   N('dia','yellow','هر دو نیم هم‌حالت‌اند؟', side=SIDE('gray',['دو نیم در دو حالت متفاوت‌اند و مقایسه نمی‌شوند','اختلافِ ساختهٔ خود شارژر خطا حساب نمی‌شود'], 'r', 'نه'), elab='بله'),
   N('box','blue','پنجرهٔ فعال مشخص می‌شود', ['دشارژ: فوری، حد ۵۰۰ میلی‌ولت','حین شارژ: بعد از زمان انتظار، حد ۳۰۰ میلی‌ولت','استراحت: بعد از زمان انتظار، حد ۳۰۰'], ref='Imbalance_Evaluate: پنجره‌ها'),
   N('sec','navy','بخش ۲: پایداری و رویداد سیکل'),
   N('dia','yellow','اختلاف بالای حد پنجره ماند و زمان پایداری پر شد؟', side=SIDE('yellow',['نامزد رویداد برای همین سیکل','پرش کوتاه حساب نمی‌شود'], 'r', 'بله'), elab='نه'),
   N('dia','yellow','سیکل با شارژ کامل تمام شد؟', side=SIDE('gray',['نامزد این سیکل دور ریخته می‌شود و رویدادی ثبت نمی‌شود','شارژ نیمه‌کاره نه رویداد است نه پاک‌شدن'], 'r', 'نه'), elab='بله'),
   N('box','blue','ثبت یک رویداد برای این سیکل', ['شمارندهٔ رویداد یکی بالا می‌رود'], ref='Imbalance_Evaluate: ثبت رویداد', note=SIDE('gray',['رویداد و قفل در NVM می‌مانند','پاک‌شدن کامل: نبود باتری ۳ ثانیه'],'l')),
   N('sec','navy','بخش ۳: قفل و پاک‌شدن'),
   N('dia','yellow','رویدادها به حد قفل رسیدند؟ (پیش‌فرض ۱۰)', side=SIDE('red',['قفل عدم‌توازن','قرمز چشمک‌زن + بوق دوره‌ای؛ وتوی خروجی و بودجهٔ سیکل شارژ'], 'r', 'بله'), elab='نه'),
   N('box','green','پاک‌شدن‌ها', ['۳ سیکل کاملِ بی‌رویداد: فقط شمارندهٔ رویداد صفر','تعویض واقعی باتری (۳ ثانیه نبودن): همه‌چیز صفر'], ref='func__Imbalance_Evaluate: پاک‌شدن'),
 ],
 'tree': T('ارزیابی عدم‌توازن — هر پاس',
   T('ورودی نامعتبر؟ → فریز'),
   T('اختلاف قدرمطلق دو نیم'),
   T('هم‌حالت بودن دو نیم'),
   T('پنجره‌ها: دشارژ / شارژ / استراحت'),
   T('ماندگاری بالای حد → نامزد رویداد'),
   T('پایان سیکل کامل؟ → ثبت رویداد'),
   T('رسیدن به حد قفل → قفل + وتو + بودجه'),
   T('پاک‌شدن: ۳ سیکل پاک / تعویض باتری')),
 'conds': [
   ['اختلاف دو نیم در دشارژ از ۵۰۰ میلی‌ولت رد شد','اگر ماندگار شود نامزد رویداد همان سیکل می‌شود.','حد دشارژ ۵۰۰ میلی‌ولت (شناسهٔ ۱۰۹)'],
   ['اختلاف در شارژ یا استراحت از ۳۰۰ میلی‌ولت رد شد','بعد از زمان انتظار هر پنجره و با ماندگاری، نامزد رویداد می‌شود.','حد ۳۰۰ میلی‌ولت (۱۰۸)؛ انتظارها ۱۱۰ و ۱۱۱؛ پایداری ۱۱۲'],
   ['رویدادها به حد قفل رسیدند','قفل عدم‌توازن: قرمز چشمک‌زن و بوق دوره‌ای؛ خروجی دستگاه می‌تواند وتو شود و شارژ فقط تا بودجهٔ سیکل قفل اجازه دارد.','پیش‌فرض ۱۰ رویداد (۱۱۴)؛ وتو (۱۱۷)؛ بودجه (۱۱۸)'],
   ['۳ سیکل کامل بدون رویداد پشت سر هم آمد','فقط شمارندهٔ رویداد صفر می‌شود؛ قفل سر جایش می‌ماند.','شناسهٔ ۱۳۶، پیش‌فرض ۳ سیکل'],
   ['باتری ۳ ثانیه نبود (تعویض واقعی)','شمارنده، قفل و بودجه همه صفر می‌شوند.','۳ ثانیه نبودن باتری'],
 ],
 'outs': [
   ['قفل و وضعیت عدم‌توازن','چراغ قرمز چشمک‌زن و الگوی بوق دوره‌ای (شناسه‌های ۱۱۵، ۱۱۶، ۱۲۳، ۱۲۴، ۱۳۲، ۱۳۳)','ماژول نمایش از همین خروجی چهره می‌سازد'],
   ['وتوی خروجی','باتری قفل‌شده خروجی دستگاه را ندهد (شناسهٔ ۱۱۷)','به تعویض خروجی می‌رود'],
   ['اجازهٔ شارژ با بودجه','تا پرشدن بودجهٔ سیکل‌های بعد از قفل، شارژ آزاد است؛ بعدش کانال‌ها خاموش می‌مانند (۱۱۸)','به شارژر می‌رود'],
   ['حافظهٔ ماندگار','رویدادها، قفل و سیکل‌های قفل در NVM می‌مانند','با خاموش و روشن‌شدن پاک نمی‌شوند'],
 ],
}


MODULES['changeover'] = {
 'file': 'changeover_process.html',
 'title': 'فرآیند تعویض خروجی — توضیح ساده',
 'sub': 'این ماژول تصمیم می‌گیرد خروجی دستگاه از باتری بیاید یا باتری برای محافظت کنار گذاشته شود؛ و تنها جایی است که خط محافظ باتری را تکان می‌دهد.',
 'steps': [
   '<b>اول اندازه‌گیری:</b> اگر عکس اندازه‌گیری نامعتبر باشد، هیچ تصمیمی گرفته نمی‌شود و حالت فعلی می‌ماند.',
   '<b>خطا:</b> هر بیت خطایی که ست باشد، حالت کلی «خطا» می‌شود و هیچ پینی تکان نمی‌خورد.',
   '<b>وتوها:</b> باتریِ قفل‌شدهٔ عدم‌توازن (با تیک پنل) یا باتریِ «خراب» قفل‌شده هرگز روی خروجی نمی‌رود؛ مثل قطع بحرانی رفتار می‌شود.',
   '<b>قطع نرم:</b> ولتاژ کل باتری زیر <b class="num">۲۱ ولت</b> قفل باتری کم را می‌بندد؛ فقط با رسیدن به <b class="num">۲۱٫۲ ولت</b> باز می‌شود (هیسترزیس).',
   '<b>قطع:</b> اگر شرط قطع (قفل باتری کم یا زیر <b class="num">۲۰٫۸ ولت</b>)، <b class="num">۳ ثانیه</b> پیوسته بماند، خط محافظ فعال و خروجی از باتری جدا می‌شود.',
   '<b>وصل دوباره:</b> فقط وقتی ورودی حاضر است، ولتاژ به ۲۱٫۲ رسیده و ورودی <b class="num">۳۰ ثانیه</b> پایدار مانده و بعد هم ۳ ثانیهٔ تأیید گذشت؛ عمداً بدون برق ورودی وصل نمی‌شود تا باتری ضعیف زیر بار برنگردد.',
 ],
 'example': [
   'دستگاه روی باتری کار می‌کند و ولتاژ کل به ۲۰٫۹ ولت می‌رسد؛ قفل باتری کم بسته می‌شود.',
   '۳ ثانیه در همین وضعیت می‌ماند؛ خط محافظ فعال می‌شود و خروجی از باتری جدا می‌شود.',
   'برق ورودی می‌آید و باتری شارژ می‌شود؛ بعد از پایداری ۳۰ ثانیه‌ای ورودی و رسیدن به ۲۱٫۲ ولت و ۳ ثانیه تأیید، خروجی دوباره وصل می‌شود.',
 ],
 'flow': [
   N('sec','navy','بخش ۱: گیت‌های ایمنی'),
   N('pill','navy','شروع ارزیابی تعویض خروجی (هر پاس)', ref='func__Changeover_Evaluate'),
   N('dia','yellow','Snapshot معتبر است؟', side=SIDE('gray',['حالت فعلی و خط محافظ دست نمی‌خورد','زمانِ این پاس برای هیچ تایمری حساب نمی‌شود'], 'r', 'نه'), elab='بله'),
   N('dia','yellow','بیت خطایی ست است؟', side=SIDE('red',['حالت کلی «خطا»','هیچ پینی تکان نمی‌خورد'], 'r', 'بله'), elab='نه'),
   N('dia','yellow','وتوی خروجی فعال است؟ (عدم‌توازن قفل + تیک، یا باتری خراب قفل)', ref='func__Changeover_Evaluate: وتوها', side=SIDE('red',['خط محافظ فعال و حالت «امن»','این باتری هرگز به خروجی وصل نمی‌شود'], 'r', 'بله'), elab='نه'),
   N('sec','navy','بخش ۲: قفل باتری کم و قطع خروجی'),
   N('box','blue','به‌روزرسانی قفل باتری کم با هیسترزیس', ['زیر ۲۱ ولت: قفل بسته','بالای ۲۱٫۲ ولت: قفل باز','بین این دو: همان قبلی می‌ماند'], ref='func__Changeover_Evaluate: قفل باتری کم'),
   N('dia','yellow','شرط قطع هست؟ (قفل باتری کم یا زیر ۲۰٫۸ ولت)', elab='بله'),
   N('dia','yellow','شرط قطع ۳ ثانیه پیوسته ماند؟', ref='func__Changeover_Evaluate: CutTimer', side=SIDE('red',['خط محافظ فعال؛ خروجی از باتری جدا','حالت «امن» تا وصل دوباره'], 'r', 'بله'), elab='نه'),
   N('sec','navy','بخش ۳: پایداری ورودی و وصل دوباره'),
   N('box','blue','ردیابی پایداری ورودی', ['تا وقتی ورودی حاضر است، پایداری شمرده می‌شود؛ با هر قطع‌شدن، شمارش از صفر شروع می‌شود'], ref='func__Changeover_Evaluate: پایداری ورودی'),
   N('dia','yellow','ورودی برگشت؟ ولتاژ بالای ۲۱٫۲ + ورودی ۳۰ ثانیه پایدار + ۳ ثانیه تأیید', ref='func__Changeover_Evaluate: ReconnectTimer', side=SIDE('green',['خط محافظ آزاد و خروجی وصل','بدون برق ورودی، خروجی عمداً وصل نمی‌شود'], 'r', 'بله'), elab='نه'),
   N('box','green','حالت عادی از روی حضور ورودی', ['ورودی هست: حالت «ورودی»','ورودی نیست: حالت «باتری»؛ اگر محافظ فعال بود «امن» می‌ماند'], note=SIDE('gray',['پین خروجی فقط در لحظهٔ قطع یا وصل حرکت می‌کند','بقیهٔ پاس‌ها خروجی ثابت می‌ماند'],'l'), ref='func__Changeover_Evaluate: حالت خروجی'),
 ],
 'tree': T('ارزیابی تعویض خروجی — هر پاس',
   T('عکس نامعتبر؟ → حفظ حالت'),
   T('خطا؟ → حالت خطا، بدون حرکت پین'),
   T('وتوها: عدم‌توازن / باتری خراب → محافظ + امن'),
   T('قفل باتری کم: ۲۱ بستن / ۲۱٫۲ بازکردن'),
   T('شرط قطع ۳ ثانیه → محافظ فعال (امن)'),
   T('پایداری ورودی ۳۰ ثانیه'),
   T('وصل دوباره: ورودی + ۲۱٫۲ + ۳ ثانیه → آزاد'),
   T('عادی: ورودی/باتری از روی حضور ورودی')),
 'conds': [
   ['ولتاژ کل باتری زیر ۲۱ ولت رفت','قفل باتری کم بسته می‌شود و شمارش قطع ۳ ثانیه‌ای شروع می‌شود.','۲۱ ولت؛ بازشدن در ۲۱٫۲ ولت'],
   ['ولتاژ زیر ۲۰٫۸ ولت رفت (بحرانی)','بدون صبر برای قفل هم شرط قطع حساب می‌شود؛ بعد از ۳ ثانیه خروجی جدا می‌شود.','۲۰٫۸ ولت / ۳ ثانیه'],
   ['باتری عدم‌توازن یا «خراب» قفل شد','با تیک وتو در پنل، باتری هرگز روی خروجی نمی‌رود و خط محافظ فعال می‌ماند.','شناسهٔ وتوی عدم‌توازن ۱۱۷؛ باتری خراب ۱۲۷'],
   ['برق ورودی برگشت و باتری شارژ شد','فقط با ۳۰ ثانیه پایداری ورودی و ۲۱٫۲ ولت و ۳ ثانیه تأیید، خروجی دوباره وصل می‌شود.','۳۰ ثانیه + ۳ ثانیه'],
 ],
 'outs': [
   ['خط محافظ باتری (پین منطقی BSP)','فعال‌کردن در قطع و وتوها؛ آزادکردن فقط در وصل دوباره','تنها فرمان سخت‌افزاری این ماژول'],
   ['حالت کلی دستگاه','خطا / امن / ورودی / باتری','شارژر و نمایش از همین حالت پیروی می‌کنند'],
 ],
}

MODULES['ui'] = {
 'file': 'ui_process.html',
 'title': 'فرآیند چراغ‌ها و بوق — توضیح ساده',
 'sub': 'این ماژول از روی وضعیت بقیهٔ ماژول‌ها یک «چهره» برای سه چراغ (سبز، زرد، قرمز) و بوق انتخاب می‌کند؛ همیشه بالاترین اولویت برنده است.',
 'steps': [
   '<b>اولویت خطاها:</b> اول خطای فنی (هر سه چراغ با هم چشمک)، بعد اضافه‌ولتاژ ورودی، بعد قطع باتری (خطای ۶)، بعد باتری خراب، بعد عدم‌توازن قفل‌شده.',
   '<b>خطای ۶:</b> چراغ قرمز ۱ ثانیه‌ای با ۵۰٪ روشن و ۳ بوق پشت سر هم.',
   '<b>باتری خراب:</b> قرمز ثابت؛ <b>عدم‌توازن:</b> قرمز چشمک‌زن با بوق دوره‌ای جدا تا دو حکم قاطی نشوند.',
   '<b>حالت عادی با ورودی:</b> شارژ کامل = سبز ثابت و زرد خاموش؛ در حال شارژ = زرد چشمک‌زن؛ استراحت حرارتی = زرد چشمک ۱ ثانیه‌ای با سبز ثابت؛ ورودی سالم بدون شارژ = سبز ثابت.',
   '<b>روی باتری (بدون ورودی):</b> درصد باتری از روی ولتاژ خوانده می‌شود؛ سبز با ریتم درصد چشمک می‌زند و در درصدهای پایین بوق هشدار می‌آید (۴۰٪، ۲۰٪، ۱۰٪ و بحرانی).',
   '<b>درصد دو نقشه دارد:</b> map شارژ و map دشارژ جدا هستند تا حد «کامل» با حد مصرف قاطی نشود.',
 ],
 'example': [
   'برق ورودی وصل است و شارژر در حال شارژ است: زرد چشمک می‌زند و سبز ثابت است.',
   'شارژ کامل می‌شود: چشمک زرد کاملاً می‌ایستد و فقط سبز ثابت می‌ماند.',
   'وسط شارژ سیم باتری کشیده می‌شود: چهره فوری عوض می‌شود؛ قرمز چشمک‌زن و ۳ بوق (خطای ۶).',
 ],
 'flow': [
   N('sec','navy','بخش ۱: اولویت خطاها — اولین برنده نمایش داده می‌شود'),
   N('pill','navy','شروع پاس نمایش (هر پاس)', ref='func__Ui_Tick'),
   N('dia','yellow','Snapshot معتبر است؟', side=SIDE('gray',['فازهای چشمک ریست می‌شوند','درصد پایدار قبلی نگه داشته می‌شود'], 'r', 'نه'), elab='بله'),
   N('box','blue','وضعیت ورودی با هیسترزیس خوانده می‌شود', ['حضور ورودی و اضافه‌ولتاژ ورودی (بالای ۲۸ ولت)'], ref='func__Ui_UpdateInputState'),
   N('dia','yellow','خطای فنی (۷) ست است؟', ref='ScenarioTechnicalFault_Tick', side=SIDE('red',['هر سه چراغ هم‌فاز چشمک می‌زنند + بوق','بالاترین اولویت؛ فقط با ریست پاک می‌شود'], 'r', 'بله'), elab='نه', note=SIDE('red',['بالاترین اولویت نمایش است','حتی از اضافه‌ولتاژ هم جلوتر است'],'l')),
   N('dia','yellow','اضافه‌ولتاژ ورودی هست؟', side=SIDE('red',['چهرهٔ اضافه‌ولتاژ (چشمک + بوق دوره‌ای)'], 'r', 'بله'), elab='نه'),
   N('dia','yellow','خطای ۶ (قطع باتری) ست است؟', side=SIDE('red',['قرمز چشمک ۱ ثانیه‌ای ۵۰٪ + ۳ بوق'], 'r', 'بله'), elab='نه'),
   N('dia','yellow','باتری «خراب» قفل شده؟', side=SIDE('red',['قرمز ثابت تا تعویض باتری'], 'r', 'بله'), elab='نه'),
   N('dia','yellow','عدم‌توازن قفل شده؟', side=SIDE('red',['فقط قرمز چشمک‌زن + بوق دوره‌ای جدا'], 'r', 'بله'), elab='نه'),
   N('sec','navy','بخش ۲: نمایش عادی — وقتی خطایی نیست'),
   N('box','blue','درصد باتری از map شارژ', ['شارژ کامل یعنی شارژر اعلام کند، یا درصد به ۱۰۰ برسد (لچ ۱۰۰/۹۵)'], ref='UpdateChargingFullHysteresis'),
   N('dia','yellow','ورودی حاضر است؟', elab='بله'),
   N('dia','yellow','شارژ کامل است؟', side=SIDE('green',['سبز ثابت و زرد کاملاً خاموش'], 'r', 'بله'), elab='نه', ref='func__Ui_UpdateChargingFullHysteresis'),
   N('dia','yellow','استراحت حرارتی فعال است؟', side=SIDE('yellow',['زرد چشمک ۱ ثانیه‌ای ۵۰٪ با سبز ثابت'], 'r', 'بله'), elab='نه'),
   N('dia','yellow','کانالی در حال شارژ است؟', side=SIDE('blue',['چهرهٔ شارژ؛ زرد چشمک‌زن با حداقل روشنایی','اگر نه: سبز ثابتِ «ورودی سالم»'], 'r', 'بله'), elab='نه'),
   N('box','blue','چهرهٔ کار روی باتری (بدون ورودی)', ['سبز با ریتم درصد باتری چشمک می‌زند','بوق هشدار در ۴۰٪، ۲۰٪، ۱۰٪ و بحرانی'], ref='ScenarioBatteryRun_Tick'),
 ],
 'tree': T('نمایش چراغ و بوق — هر پاس',
   T('عکس نامعتبر؟ → ریست فازها'),
   T('وضعیت ورودی با هیسترزیس'),
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
   ['خطای فنی (۷) ست شد','هر سه چراغ با یک فاز چشمک می‌زنند و بوق می‌آید؛ بالاتر از همهٔ چهره‌ها.','دوره ۱ ثانیه، ۵۰٪'],
   ['سیم باتری کشیده شد (خطای ۶)','قرمز چشمک‌زن (۱ ثانیه، ۵۰٪) و ۳ بوق پشت سر هم.','۳ بوق، دورهٔ تکرار ۳ ثانیه'],
   ['شارژ کامل شد','سبز ثابت می‌ماند و چشمک زرد کاملاً خاموش می‌شود؛ تا وقتی کانالی در حال شارژ است، چهرهٔ شارژ می‌ماند.','کامل از شارژر یا لچ ۱۰۰/۹۵٪'],
   ['دستگاه روی باتری است و درصد پایین آمد','در ۴۰٪، ۲۰٪ و ۱۰٪ بوق‌های هشدار پله‌ای و در حد بحرانی بوق بحرانی می‌آید.','آستانه‌های درصد از پنل (map دشارژ)'],
 ],
 'outs': [
   ['سه چراغ سبز، زرد، قرمز (BSP)','چهرهٔ انتخاب‌شده با ریتم و فاز خودش','در هر پاس تازه می‌شود'],
   ['بوق (BSP)','الگوهای بوق هر چهره: ۳ بوق خطای ۶، بوق دوره‌ای عدم‌توازن، هشدارهای درصدی','همراه چهرهٔ فعال'],
   ['خودآزمایی روشن‌شدن','هر سه چراغ در شروع برای اطمینان از سالم‌بودنشان روشن می‌شوند','۵۰۰ میلی‌ثانیه در راه‌اندازی'],
 ],
}

MODULES['protection'] = {
 'file': 'protection_process.html',
 'title': 'فرآیند ناظر حفاظت — توضیح ساده',
 'sub': 'ناظر سبک برنامه: تنها کارش این است که تا وقتی اندازه‌گیری معتبر نیست، خطای «مبدل» (بیت ۰) را نگه دارد. این ماژول فعلاً در ساخت اصلی خاموش است.',
 'steps': [
   'اگر عکس اندازه‌گیری نباشد یا نامعتبر باشد، بیت خطای مبدل <b>ست</b> می‌شود.',
   'با اولین عکس معتبر، همان بیت <b>پاک</b> می‌شود.',
   'این خطا عمداً <b>قفل نمی‌شود</b>: لحظه‌ای است؛ چون پاس‌های اول روشن‌شدن همیشه قبل از آماده‌شدن اندازه‌گیری اجرا می‌شوند و خطای قفل‌شده دستگاه را برای همیشه در حالت امن نگه می‌داشت.',
   'نظارت اضافه‌جریان و باتری ضعیف این‌جا نیست؛ در شارژر و ماژول خطا انجام می‌شود.',
   'ماژول با پیش‌فرض خاموش کامپایل می‌شود (فقط با روشن‌کردن آگاهانه در پیکربندی فعال می‌شود).',
 ],
 'flow': [
   N('sec','navy','بخش ۱: بررسی ناظر — فعلاً در بیلد خاموش است'),
   N('pill','navy','شروع پاس ناظر', ref='func__Protection_Run'),
   N('dia','yellow','Snapshot معتبر است؟', side=SIDE('green',['پاک‌کردن بیت خطای مبدل (بیت ۰)'], 'r', 'بله'), elab='نه'),
   N('box','red','ست‌کردن FAULT_ADC', ['لحظه‌ای است و قفل نمی‌شود','با اولین عکس معتبر خودش پاک می‌شود'], ref='Fault_Set(FAULT_ADC)', note=SIDE('gray',['تنها ماژول خاموش بیلد است','MODULE_PROTECTION=0'],'l')),
 ],
 'tree': T('ناظر حفاظت',
   T('عکس نامعتبر؟ → ست بیت مبدل'),
   T('عکس معتبر؟ → پاک بیت مبدل'),
   T('قفل ندارد؛ ماژول پیش‌فرض خاموش است')),
 'conds': [
   ['اندازه‌گیری نامعتبر شد','بیت خطای مبدل ست می‌شود؛ دستگاه از این بیت مثل بقیهٔ خطاها به حالت خطا می‌رود.','بیت ۰'],
   ['اندازه‌گیری معتبر شد','بیت پاک می‌شود و کار عادی ادامه دارد.','بدون قفل'],
 ],
 'outs': [
   ['بیت خطای مبدل','ست/پاک لحظه‌ای','به ماژول خطا می‌رود و از آن‌جا حالت کلی و شارژر پیروی می‌کنند'],
 ],
}


MODULES['callut'] = {
 'file': 'callut_process.html',
 'title': 'فرآیند جدول کالیبراسیون — توضیح ساده',
 'sub': 'این ماژول جدول‌های تبدیل را نگه می‌دارد: با چند نقطهٔ اندازه‌گیری‌شده روی میز، هر شمارش مبدل به میلی‌ولت یا میلی‌آمپر تبدیل می‌شود.',
 'steps': [
   'برای هر کانال یک جدول از نقطه‌ها هست: شمارش مبدل ← مقدار واقعی (میلی‌ولت یا میلی‌آمپر).',
   'بین دو نقطه، مقدار با <b>خط راست بین همان دو نقطه</b> حساب می‌شود (درون‌یابی خطی)؛ بیرون از جدول، مقدار لبهٔ جدول می‌ماند.',
   'جدول‌ها از پنل قابل خواندن و نوشتن‌اند؛ قبل از قبول‌شدن، جدول تازه کامل بررسی می‌شود (ترتیب نقطه‌ها، تعداد و بازه‌ها).',
   'اگر جدول خراب یا ناقص باشد، جدول پیش‌فرض کارخانه به‌جای آن می‌نشیند تا اندازه‌گیری هیچ‌وقت بی‌جدول نماند.',
   'همین جدول‌ها هستند که اختلاف واقعی قطعات هر برد را جبران می‌کنند؛ برای همین هر برد جدول خودش را دارد.',
 ],
 'example': [
   'روی میز با منبع دقیق، در شمارش ۱۰۰۰ مقدار ۶ ولت و در شمارش ۲۰۰۰ مقدار ۱۲ ولت خوانده شده است.',
   'حالا شمارش ۱۵۰۰ می‌آید؛ جدول دقیقاً وسط دو نقطه را می‌دهد: ۹ ولت.',
   'اگر شمارش ۲۵۰۰ بیاید (بالاتر از آخرین نقطه)، همان مقدار آخرین نقطه برگردانده می‌شود.',
 ],
 'flow': [
   N('sec','navy','بخش ۱: تبدیل با جدول فعلی'),
   N('pill','navy','درخواست تبدیل یک شمارش'),
   N('box','blue','جدول همان کانال انتخاب می‌شود', ['جدول ولتاژ یا جریان، مخصوص همان کانال'], ref='func__CalLut_Active'),
   N('dia','yellow','شمارش داخل بازهٔ جدول است؟', side=SIDE('gray',['مقدار نزدیک‌ترین لبهٔ جدول','بالاتر از جدول یعنی همان نقطهٔ آخر'], 'r', 'نه'), elab='بله'),
   N('box','blue','دو نقطهٔ دورِ شمارش پیدا می‌شود', [], ref='func__CalLut_Points'),
   N('box','green','درون‌یابی خطی بین دو نقطه', ['خروجی: میلی‌ولت یا میلی‌آمپر واقعی'], ref='func__CalLut_Counts + BatteryMa', note=SIDE('gray',['جدول در RAM است و از NVM می‌آید','از پنل هم قابل نوشتن است'],'l')),
   N('sec','navy','بخش ۲: نوشتن جدول تازه از پنل'),
   N('dia','yellow','جدول تازه کامل و درست است؟ (ترتیب، تعداد، بازه)', side=SIDE('red',['قبول نمی‌شود و جدول قبلی می‌ماند'], 'r', 'نه'), elab='بله'),
   N('box','green','نشستن جدول تازه و ذخیره در حافظهٔ ماندگار', ['اگر جدول ذخیره‌شده خراب بود، جدول پیش‌فرض کارخانه می‌آید'], ref='func__CalLut_Commit'),
 ],
 'tree': T('جدول کالیبراسیون',
   T('تبدیل: جست‌وجوی دو نقطه + درون‌یابی خطی'),
   T('بیرون از بازه → مقدار لبه'),
   T('نوشتن از پنل → بررسی کامل → قبول یا رد'),
   T('جدول خراب → پیش‌فرض کارخانه')),
 'conds': [
   ['شمارش بیرون از جدول بود','مقدار نزدیک‌ترین لبهٔ جدول برمی‌گردد؛ هیچ‌وقت عدد بی‌ربط ساخته نمی‌شود.','—'],
   ['جدول تازه از پنل ناقص یا به‌هم‌ریخته بود','قبول نمی‌شود و جدول قبلی سر جایش می‌ماند.','بررسی ترتیب و تعداد نقطه‌ها'],
   ['جدول ذخیره‌شده خراب بود','جدول پیش‌فرض کارخانه فعال می‌شود.','—'],
 ],
 'outs': [
   ['مقدار واقعی (میلی‌ولت/میلی‌آمپر)','تبدیل هر شمارش برای ماژول اندازه‌گیری','در هر پاس اندازه‌گیری'],
   ['جدول‌ها در حافظهٔ ماندگار','خواندن و نوشتن از پنل با بررسی کامل','ذخیره با رکورد معتبر و نسخه'],
 ],
}

MODULES['esplink'] = {
 'file': 'esplink_process.html',
 'title': 'فرآیند لینک پنل (ESP) — توضیح ساده',
 'sub': 'این ماژول زبان صحبت میکرو با پنل ESP است: فریم‌های نسخه‌دار با کنترل خطای قوی، و ۱۴۳ شناسهٔ پارامتر که پنل می‌خواند و می‌نویسد.',
 'steps': [
   '<b>قاب فریم:</b> هر پیام با دو بایت شروع، نسخه، نوع، طول و در پایان CRC-16 می‌آید؛ فریم خراب یا بی‌نسخه دور ریخته می‌شود.',
   '<b>خواندن:</b> پنل مقدار شناسه‌ها را می‌خواهد؛ چون همه در یک فریم جا نمی‌شوند، پاسخ در چند تکه فرستاده می‌شود و پنل خودش مقایسه می‌کند.',
   '<b>نوشتن:</b> هر نوشتن اول به ماژول صاحب آن شناسه می‌رود؛ همان ماژول بازه را بررسی می‌کند و اگر بیرون از بازه باشد قبول نمی‌کند (پنل نمی‌تواند عدد خطرناک بنویسد).',
   '<b>سد دوم:</b> برای شارژ سبز (شناسه‌های درصد شارژ) یک سد سخت جدا هم هست تا ترکیب خطرناک ذخیره نشود.',
   '<b>فرمان‌ها:</b> مود تست دستی شارژر، خاموش/روشن هر کانال شارژ و فرمان‌های ذخیره از همین راه می‌آیند؛ شارژر در تسک خودش آن‌ها را اجرا می‌کند، نه وسط لینک.',
   '<b>ذخیره:</b> تنظیمات ماندگار در رکورد نسخه‌دار NVM می‌نشینند و با خاموش و روشن‌شدن می‌مانند.',
 ],
 'example': [
   'از پنل سقف شارژ کامل را از ۱۴٫۴ به ۱۴٫۲ ولت تغییر می‌دهی.',
   'فریم با CRC درست می‌رسد؛ شناسه به ماژول شارژر می‌رود؛ شارژر بازه را چک می‌کند (بین ۱۱ تا ۱۴٫۶) و قبول می‌کند.',
   'از آن لحظه شارژر با سقف تازه کار می‌کند و مقدار تازه در حافظهٔ ماندگار هم می‌نشیند.',
 ],
 'flow': [
   N('sec','navy','بخش ۱: دریافت و اعتبار فریم'),
   N('pill','navy','رسیدن بایت‌ها از UART', ref='EspLink_Run'),
   N('dia','yellow','فریم کامل و CRC درست است؟', ref='func__EspLink_Run: بررسی CRC-16', side=SIDE('red',['این فریم دور ریخته می‌شود و جوابی ندارد','نویز خط هیچ اثری روی برنامه ندارد'], 'r', 'نه'), elab='بله'),
   N('dia','yellow','نسخه و نوع فریم شناخته‌شده است؟', side=SIDE('red',['این فریم نادیده گرفته می‌شود'], 'r', 'نه'), elab='بله'),
   N('dia','yellow','درخواست خواندن است یا نوشتن؟', elab='خواندن'),
   N('sec','navy','بخش ۲: خواندن و نوشتن'),
   N('box','blue','خواندن: جمع‌کردن مقدار شناسه‌ها', ['پاسخ در چند فریم، تکه‌تکه فرستاده می‌شود','پنل مقدارها را خودش مقایسه می‌کند'], ref='GetParam از ماژول صاحب'),
   N('box','blue','نوشتن: فرستادن به ماژول صاحب شناسه', ['هر ماژول مقدار را در بازهٔ خودش گیره می‌کند'], ref='SetParam در ماژول صاحب'),
   N('dia','yellow','مقدار داخل بازهٔ مجاز بود؟', side=SIDE('red',['قبول نمی‌شود و مقدار قبلی می‌ماند'], 'r', 'نه'), elab='بله'),
   N('sec','navy','بخش ۳: اعمال و پاسخ'),
   N('box','green','اعمال مقدار + ذخیرهٔ ماندگار (اگر لازم است)', ['فرمان‌هایی مثل مود دستی به شارژر سپرده می‌شوند تا در تسک خودش اجرا کند'], note=SIDE('gray',['تله‌متری دوره‌ای جدا فرستاده می‌شود','ربطی به پاسخ همین فریم ندارد'],'l'), ref='SetParam + ذخیرهٔ NVM'),
 ],
 'tree': T('لینک پنل — هر پاس ارتباط',
   T('مونتاژ فریم و بررسی CRC-16'),
   T('نسخه/نوع ناشناخته → رد'),
   T('خواندن: پاسخ تکه‌ای ۱۴۳ شناسه'),
   T('نوشتن: اعتبارسنجی در ماژول صاحب',
     T('بازه بد → رد نوشتن'),
     T('سد سخت شارژ سبز')),
   T('فرمان‌ها: مود دستی، کانال‌ها، ذخیره'),
   T('ذخیرهٔ NVM نسخه‌دار')),
 'conds': [
   ['فریم خراب یا CRC نادرست رسید','فریم کامل دور ریخته می‌شود؛ هیچ مقداری عوض نمی‌شود.','CRC-16'],
   ['پنل عدد بیرون از بازه نوشت','ماژول صاحب شناسه قبول نمی‌کند و مقدار قبلی می‌ماند.','بازهٔ هر شناسه در ماژول خودش'],
   ['ترکیب درصد شارژ سبز خطرناک شد','سد سخت در لحظهٔ ذخیره جلوی ذخیره‌شدن ترکیب بد را می‌گیرد.','سد سخت جدا از بازهٔ تکی'],
   ['لینک وسط مود دستی قطع شد','شارژر بعد از ۳ ثانیه سکوت خودش از مود دستی بیرون می‌آید.','۳ ثانیه dead-man'],
 ],
 'outs': [
   ['مقادیر زنده به پنل','وضعیت‌ها، اندازه‌گیری‌ها، وضعیت شارژ و خطاها','در پاسخ به خواندن پنل'],
   ['مقادیر و فرمان‌ها به ماژول‌ها','تنظیم پارامتر، مود دستی، خاموش/روشن کانال‌ها','بعد از قبول‌شدن بازه در ماژول صاحب'],
   ['رکورد NVM','ذخیرهٔ ماندگار تنظیمات با نسخه و CRC','با فرمان ذخیره یا تغییر ماندگار'],
 ],
}

MODULES['mcupowerpath'] = {
 'file': 'mcupowerpath_process.html',
 'title': 'فرآیند مسیر تغذیهٔ میکرو — توضیح ساده',
 'sub': 'این ماژول تصمیم می‌گیرد خودِ میکرو از باتری تغذیه شود یا از برق ورودی؛ تا وقتی برق هست باتری بی‌جهت برای مغز دستگاه خالی نشود.',
 'steps': [
   '<b>شروع:</b> میکرو از باتری راه می‌افتد؛ باتری وصل است و مدار آماده است.',
   '<b>رفتن روی ورودی:</b> اگر ولتاژ ورودی دست‌کم <b class="num">۲۲ ولت</b> شد و <b class="num">۵ ثانیه</b> پیوسته ماند، مسیر باتری میکرو قطع و میکرو از ورودی تغذیه می‌شود.',
   '<b>نوار مرده:</b> بین <b class="num">۲۱٫۵ تا ۲۲ ولت</b> هیچ تغییری نمی‌شود و تایمر هم لغو می‌شود تا مسیر چپ و راست نپرد.',
   '<b>برگشت به باتری:</b> اگر ورودی زیر <b class="num">۲۱٫۵ ولت</b> افتاد، برگشت به باتری فوری است.',
   '<b>قطع ناگهانی ورودی:</b> لبهٔ افتادن سیگنال حضور ورودی همان لحظه در وقفه مسیر باتری را برمی‌گرداند؛ منتظر پاس بعدی نمی‌ماند.',
   'اگر اندازه‌گیری نامعتبر باشد، مسیر فعلی حفظ و تایمر لغو می‌شود.',
 ],
 'example': [
   'دستگاه روی باتری روشن است؛ برق ورودی وصل می‌شود و روی ۲۴ ولت می‌ایستد.',
   '۵ ثانیه می‌گذرد و ورودی پایدار می‌ماند؛ مسیر باتری میکرو قطع می‌شود و میکرو از ورودی کار می‌کند.',
   'برق می‌رود؛ همان لحظه با لبهٔ سیگنال، میکرو دوباره از باتری تغذیه می‌شود و دستگاه بی‌وقفه ادامه می‌دهد.',
 ],
 'flow': [
   N('sec','navy','بخش ۱: تشخیص و سوئیچ به ورودی'),
   N('pill','navy','شروع: میکرو از باتری تغذیه می‌شود', ref='func__McuPowerPath_Run'),
   N('dia','yellow','لبهٔ افتادن حضور ورودی آمد؟ (وقفه)', ref='McuPowerPath_OnInputIrq', side=SIDE('red',['برگشت فوری به باتری، همان لحظه در وقفه','منتظر پاس بعدی نمی‌ماند'], 'r', 'بله'), elab='نه', note=SIDE('red',['برگشت با وقفه فوری است','حتی وسط پاس کنترل هم صبر نمی‌کند'],'l')),
   N('dia','yellow','Snapshot معتبر است؟', side=SIDE('gray',['مسیر فعلی حفظ و تایمر لغو می‌شود'], 'r', 'نه'), elab='بله'),
   N('dia','yellow','ولتاژ ورودی دست‌کم ۲۲ ولت است؟', elab='بله'),
   N('dia','yellow','۵ ثانیه پیوسته بالای ۲۲ ماند؟', ref='McuPowerPath_Run: تایمر احراز', side=SIDE('gray',['هنوز روی باتری می‌مانیم','افتادن زیر ۲۲ تایمر را از اول می‌کند'], 'r', 'نه'), elab='بله'),
   N('box','blue','قطع مسیر باتری میکرو', ['میکرو از برق ورودی تغذیه می‌شود','باتری دیگر برای تغذیهٔ میکرو خالی نمی‌شود'], ref='func__McuPowerPath_Init: GPIO تغذیه'),
   N('sec','navy','بخش ۲: ماندن و برگشت به باتری'),
   N('dia','yellow','ورودی زیر ۲۱٫۵ ولت افتاد؟', side=SIDE('green',['برگشت فوری به باتری'], 'r', 'بله'), elab='نه'),
   N('box','green','ماندن روی ورودی', ['در نوار ۲۱٫۵ تا ۲۲ ولت هیچ تغییری داده نمی‌شود'], ref='McuPowerPath_Run'),
 ],
 'tree': T('مسیر تغذیهٔ میکرو — هر پاس',
   T('وقفه: لبهٔ افتادن ورودی → باتری فوری'),
   T('عکس نامعتبر؟ → حفظ مسیر، لغو تایمر'),
   T('بالای ۲۲ ولت × ۵ ثانیه → روی ورودی'),
   T('نوار مرده ۲۱٫۵ تا ۲۲ → بدون تغییر'),
   T('زیر ۲۱٫۵ ولت → برگشت به باتری')),
 'conds': [
   ['ورودی ۲۲ ولت شد ولی قبل از ۵ ثانیه افتاد','تایمر لغو می‌شود و میکرو روی باتری می‌ماند.','۵ ثانیه پایداری'],
   ['ورودی بین ۲۱٫۵ و ۲۲ ولت ماند','هیچ سوئیچی نمی‌شود؛ وضعیت قبلی می‌ماند تا مسیر نپرد.','نوار مرده ۵۰۰ میلی‌ولت'],
   ['برق ورودی ناگهان قطع شد','برگشت به باتری در همان وقفه انجام می‌شود.','بدون انتظار پاس بعدی'],
 ],
 'outs': [
   ['پین مسیر تغذیه (BSP)','قطع مسیر باتری وقتی روی ورودی هستیم؛ وصل باتری در برگشت','تنها فرمان سخت‌افزاری این ماژول'],
   ['وضعیت مسیر فعلی','روی ورودی هستیم یا روی باتری','برای پنل و دیاگ خوانده می‌شود'],
 ],
}

MODULES['jitter'] = {
 'file': 'jitter_process.html',
 'title': 'فرآیند جیتر (تریپ سخت‌افزاری) — توضیح ساده',
 'sub': 'جیتر نگهبان سخت‌افزاری اضافه‌جریان است: کمپراتور روی برد در چند میکروثانیه تریپ می‌دهد و این ماژول آن را برای هر کانال قفل می‌کند تا شارژر ببیند.',
 'steps': [
   'کمپراتور سخت‌افزاری هر کانال جدا دیده‌بانی می‌کند؛ وقتی جریان از حد سخت‌افزار رد شود، وقفه می‌آید.',
   'این ماژول برای همان کانال یک پرچم «تریپ» قفل می‌کند؛ پرچم تا وقتی شارژر آن را پاک نکند می‌ماند.',
   'شارژر پرچم را می‌خواند: کانال را همان لحظه متوقف می‌کند، بعد از ۳ ثانیه یکی‌یکی احیا می‌کند و در احیا پرچم را پاک می‌کند.',
   'شمارش تریپ و تریپ سوم و قطع نهایی کار شارژر است، نه این ماژول؛ این‌جا فقط دیدن و قفل‌کردن رویداد است.',
 ],
 'example': [
   'جریان کانال ۱ ناگهان از حد کمپراتور رد می‌شود؛ وقفه می‌آید و پرچم trip کانال ۱ قفل می‌شود.',
   'شارژر در پاس بعدی پرچم را می‌بیند، کانال ۱ را متوقف می‌کند و کانال ۲ به کارش ادامه می‌دهد.',
   'بعد از ۳ ثانیه نوبت احیای کانال ۱ می‌رسد؛ شارژر پرچم را پاک و با duty کمتر شروع می‌کند.',
 ],
 'flow': [
   N('sec','navy','بخش ۱: مسیر trip — از کمپراتور تا شارژر'),
   N('pill','navy','کمپراتور سخت‌افزاری هر کانال آماده است', ref='func__Jitter_Run + وقفهٔ BSP'),
   N('dia','yellow','وقفهٔ تریپ کانالی آمد؟', elab='بله'),
   N('box','red','قفل پرچم trip همان کانال', ['پرچم تا پاک‌شدن توسط شارژر می‌ماند','کانال دیگر دست نمی‌خورد'], ref='Jitter_Run', note=SIDE('gray',['وقفه فقط پرچم می‌گذارد','کار اصلی در پاس کنترل شارژر است'],'l')),
   N('box','blue','شارژر پرچم را می‌خواند', ['کانال فوری می‌ایستد، تریپ شمرده می‌شود و احیا یکی‌یکی انجام می‌شود'], ref='func__Jitter_Run: خواندن پرچم'),
   N('box','green','پاک‌شدن پرچم هنگام احیا', ['شروع دوباره با duty کمتر از قبل تریپ'], ref='func__Jitter_Run: پاک‌کردن'),
 ],
 'tree': T('جیتر — دیده‌بان سخت‌افزاری',
   T('وقفهٔ کمپراتور کانال ۱ → پرچم ۱'),
   T('وقفهٔ کمپراتور کانال ۲ → پرچم ۲'),
   T('خواندن پرچم توسط شارژر'),
   T('پاک‌کردن پرچم هنگام احیای شارژر')),
 'conds': [
   ['جریان از حد کمپراتور رد شد','پرچم همان کانال قفل می‌شود؛ شارژر کانال را متوقف می‌کند.','حد سخت‌افزاری روی برد'],
   ['تریپ وسط احیا دوباره آمد','پرچم دوباره قفل می‌شود و شمارش تریپ شارژر بالا می‌رود؛ سومی قطع نهایی است.','قفل ۳ ثانیه و احیای یکی‌یکی در شارژر'],
 ],
 'outs': [
   ['پرچم trip هر کانال','قفل‌شده تا پاک‌شدن','شارژر می‌خواند و پاک می‌کند؛ خطای فنی هم از همین پرچم استفاده می‌کند'],
 ],
}

MODULES['diag'] = {
 'file': 'diag_process.html',
 'title': 'فرآیند ثبت سابقه (دیاگ) — توضیح ساده',
 'sub': 'این ماژول دفتر ثبت دستگاه است: خطاها، خاموش و روشن‌شدن‌ها، زمان شارژ هر کانال و کمینه/بیشینه‌ها را در حافظهٔ ماندگار فلش نگه می‌دارد.',
 'steps': [
   '<b>ثبت خطا:</b> هر خطایی که ست می‌شود با زمان و عکس اندازه‌گیری همان لحظه ثبت می‌شود؛ دفتر خطا ۱۰ جای آخر را نگه می‌دارد و قدیمی‌ترین می‌رود.',
   '<b>رویدادهای برق:</b> خاموش و روشن‌شدن‌ها جدا ثبت می‌شوند (۲۰ رویداد آخر).',
   '<b>زمان شارژ:</b> ثانیه‌های شارژ هر کانال جمع می‌شود و کامل‌شدن شارژ هر کانال هم شمرده می‌شود.',
   '<b>کمینه/بیشینه:</b> کمترین و بیشترین ولتاژها و جریان‌های دیده‌شده نگه داشته می‌شود.',
   '<b>ذخیرهٔ امن:</b> رکورد در دو صفحهٔ فلش با شماره و نسخه نوشته می‌شود تا اگر وسط نوشتن برق رفت، نسخهٔ قبلی سالم بماند؛ ساعت برد هم با زمان پنل میزان می‌شود.',
 ],
 'example': [
   'خطای ۶ وسط شارژ می‌آید؛ همان لحظه با زمان و ولتاژهای آن لحظه در دفتر ثبت می‌شود.',
   'دستگاه خاموش و روشن می‌شود؛ رویداد برق هم ثبت می‌شود و دفتر بعد از روشن‌شدن سر جایش هست.',
   'از پنل سابقه خوانده می‌شود و معلوم است خطا کی و در چه ولتاژی آمده است.',
 ],
 'flow': [
   N('sec','navy','بخش ۱: رویدادها و آمار'),
   N('pill','navy','دفتر ثبت آماده می‌شود (رکورد سالم از فلش خوانده می‌شود)', ref='func__Diag_Tick'),
   N('dia','yellow','خطایی ست شد؟', ref='Diag_RecordFault', side=SIDE('red',['ثبت خطا با زمان و Snapshot همان لحظه','دفتر ۱۰ خطای آخر؛ قدیمی‌ترین حذف می‌شود'], 'r', 'بله'), elab='نه'),
   N('dia','yellow','خاموش یا روشن‌شدن بود؟', side=SIDE('blue',['ثبت رویداد برق','۲۰ رویداد آخر نگه داشته می‌شود'], 'r', 'بله'), elab='نه'),
   N('box','blue','جمع‌کردن زمان شارژ و کامل‌شدن هر کانال', [], ref='func__Diag_Tick: آمار کانال‌ها'),
   N('box','blue','به‌روزرسانی کمینه و بیشینه‌ها', ['ولتاژها و جریان‌های دیده‌شده'], ref='func__Diag_Tick: min/max'),
   N('sec','navy','بخش ۲: ذخیره در فلش'),
   N('dia','yellow','زمان ذخیره رسید؟ یا فرمان ذخیره آمد؟', ref='Diag_SaveNow', side=SIDE('green',['نوشتن رکورد در صفحهٔ دیگر فلش با شماره و نسخه','وسط نوشتن برق برود، نسخهٔ قبلی سالم می‌ماند'], 'r', 'بله'), elab='نه'),
   N('box','green','رکورد آمادهٔ خواندن از پنل است', ['ساعت برد با زمان پنل هم‌زمان می‌شود'], note=SIDE('gray',['دو صفحهٔ فلش یکی‌درمیان نوشته می‌شوند','تا سایش فلش بین دو صفحه پخش شود'],'l'), ref='func__Diag_Tick: رکورد پنل'),
 ],
 'tree': T('ثبت سابقه (دیاگ)',
   T('رویدادها',
     T('خطا → ثبت با زمان و عکس (۱۰ آخر)'),
     T('برق → خاموش/روشن (۲۰ آخر)'),
     T('کامل‌شدن شارژ هر کانال')),
   T('جمع زمان شارژ کانال‌ها'),
   T('کمینه/بیشینهٔ ولتاژ و جریان'),
   T('ذخیرهٔ دوصفحه‌ای فلش با نسخه'),
   T('ساعت برد از پنل')),
 'conds': [
   ['دفتر خطا پر شد','قدیمی‌ترین خطا می‌رود و ۱۰ خطای آخر همیشه هست.','۱۰ خطا / ۲۰ رویداد برق'],
   ['وسط ذخیره برق رفت','رکورد قبلی سالم می‌ماند چون نوشتن روی صفحهٔ دیگر است و شماره/نسخه کنترل می‌شود.','دو صفحهٔ فلش + شمارهٔ ترتیبی'],
   ['رکورد فلش خراب بود','رکورد پذیرفته نمی‌شود و از نسخهٔ سالم یا پیش‌فرض شروع می‌شود.','عدد شناسایی + نسخه'],
 ],
 'outs': [
   ['رکورد سابقه به پنل','خطاها، رویدادها، زمان‌های شارژ و کمینه/بیشینه‌ها','پنل می‌خواند'],
   ['ذخیره در فلش','رکورد ماندگار دستگاه','با فرمان ذخیره و در رویدادهای مهم'],
 ],
}

MODULES['rtos'] = {
 'file': 'rtos_process.html',
 'title': 'فرآیند تسک‌ها (RTOS) — توضیح ساده',
 'sub': 'این بخش اسکلت زمانی برنامه است: پنج تسک با تخصیص ایستا (بدون حافظهٔ پویا) و یک ترتیب ثابت اجرا؛ watchdog هم زنده‌بودن تسک‌ها را چک می‌کند.',
 'steps': [
   '<b>تسک اندازه‌گیری:</b> هر ۱ میلی‌ثانیه یک پاس اندازه‌گیری و اعلام زنده‌بودن به watchdog.',
   '<b>تسک کنترل:</b> مغز تصمیم دستگاه با ترتیب ثابت: اول خطاها، بعد خطای فنی شارژر، مسیر تغذیه، عدم‌توازن، تعویض خروجی و در پایان خود شارژر؛ بعد هم اعلام به watchdog.',
   '<b>تسک ارتباط:</b> لینک پنل ESP را می‌چرخاند.',
   '<b>تسک نمایش:</b> چراغ‌ها و بوق را تازه می‌کند.',
   '<b>تسک حفاظت:</b> ناظر سبک (بیت مبدل) را اجرا می‌کند؛ در ساخت اصلی خاموش است.',
   '<b>قانون کلی:</b> هر تسک اول عکس اندازه‌گیری را صفر و تازه می‌خواند تا هیچ‌وقت با دادهٔ کهنه یا نیمه‌کاره کار نکند؛ هیچ تسکی خواب مستقیم یا حافظهٔ پویا ندارد.',
 ],
 'flow': [
   N('sec','navy','بخش ۱: راه‌اندازی'),
   N('pill','navy','روشن‌شدن: ساخت ایستای پنج تسک', ref='osKernelStart در rtos_app'),
   N('sec','navy','بخش ۲: پنج تسک هم‌زمان'),
   N('box','blue','تسک اندازه‌گیری — هر ۱ میلی‌ثانیه', ['خواندن مبدل، فیلتر و انتشار عکس','اعلام زنده‌بودن به watchdog'], ref='func__TaskMeasurement'),
   N('box','blue','تسک کنترل — ترتیب ثابت تصمیم', ['خطاها → خطای فنی → تغذیهٔ میکرو → عدم‌توازن','→ تعویض خروجی → شارژر → watchdog'], ref='func__TaskControl', note=SIDE('gray',['ترتیب تصمیم در هر پاس ثابت است','تخصیص حافظهٔ پویا نداریم؛ همه‌چیز استاتیک است'],'l')),
   N('box','blue','تسک ارتباط — لینک پنل ESP', [], ref='func__TaskComm'),
   N('box','blue','تسک نمایش — چراغ‌ها و بوق', [], ref='func__TaskUi'),
   N('box','gray','تسک حفاظت — ناظر بیت مبدل', ['در ساخت اصلی خاموش است'], ref='func__TaskProtection'),
   N('sec','navy','بخش ۳: watchdog'),
   N('dia','yellow','تسکی سر وقت اعلام زنده‌بودن نکرد؟', side=SIDE('red',['watchdog سخت‌افزاری دستگاه را ریست می‌کند','یعنی برنامه جایی گیر کرده است'], 'r', 'بله'), elab='نه'),
   N('box','green','چرخه ادامه دارد', ['همه‌چیز ایستا است؛ حافظهٔ پویا و خواب مستقیم نداریم'], ref='osDelayUntil'),
 ],
 'tree': T('تسک‌ها (RTOS)',
   T('اندازه‌گیری: هر ۱ms + watchdog'),
   T('کنترل: ترتیب ثابت ماژول‌ها',
     T('خطاها و خطای فنی'),
     T('تغذیهٔ میکرو و عدم‌توازن'),
     T('تعویض خروجی و شارژر')),
   T('ارتباط: لینک ESP'),
   T('نمایش: چراغ و بوق'),
   T('حفاظت: ناظر (خاموش در ساخت اصلی)'),
   T('watchdog: گیرکردن = ریست')),
 'conds': [
   ['تسکی گیر کرد و اعلام زنده‌بودن نکرد','watchdog دستگاه را ریست می‌کند تا از حالت گیرکرده بیرون بیاید.','پنجرهٔ watchdog'],
   ['عکس اندازه‌گیری کهنه بود','هر تسک قبل از کار عکس را صفر و تازه می‌خواند؛ با دادهٔ نامعتبر تصمیم نمی‌گیرد.','—'],
 ],
 'outs': [
   ['ترتیب و زمان‌بندی اجرا','هر ماژول در تسک و نوبت خودش اجرا می‌شود','مغز دستگاه (کنترل) همیشه با تازه‌ترین خطاها و اندازه‌گیری کار می‌کند'],
   ['watchdog','ریست خودکار هنگام گیرکردن','اعلام زنده‌بودن هر تسک در هر چرخه'],
 ],
}


MODULES['system'] = {
 'file': 'system_process.html',
 'title': 'فرآیند کل دستگاه — توضیح ساده',
 'sub': 'نمای کلی همهٔ برنامه: از روشن‌شدن تا چرخهٔ همیشگی تصمیم؛ هر ماژول صفحهٔ خودش را هم دارد و این‌جا فقط کنار هم دیده می‌شوند.',
 'steps': [
   '<b>روشن‌شدن:</b> اول سخت‌افزار (مبدل، PWM، پایه‌ها و وقفه‌ها) و بعد ماژول‌ها راه می‌افتند و تنظیمات ماندگار از حافظه خوانده می‌شود؛ چراغ‌ها خودآزمایی می‌شوند.',
   '<b>عکس مشترک:</b> تسک اندازه‌گیری هر ۱ میلی‌ثانیه یک عکس تازه و معتبر از ولتاژها و جریان‌ها منتشر می‌کند؛ همه با همان عکس کار می‌کنند.',
   '<b>ترتیب تصمیم در تسک کنترل ثابت است:</b> خطاها ← خطای فنی شارژر ← مسیر تغذیهٔ میکرو ← عدم‌توازن ← تعویض خروجی ← شارژر. یعنی اول ایمنی، بعد خروجی، بعد شارژ.',
   '<b>عمل:</b> شارژر duty کانال‌ها و رله را می‌راند، تعویض خروجی خط محافظ باتری را، و مسیر تغذیه تعیین می‌کند میکرو از کجا تغذیه شود.',
   '<b>نمایش و ارتباط موازی‌اند:</b> چراغ و بوق از روی وضعیت‌ها چهره می‌سازند و لینک ESP همان وضعیت‌ها را به پنل می‌دهد و تنظیمات را برمی‌گرداند.',
   '<b>نگهبان‌ها:</b> جیتر سخت‌افزاری اضافه‌جریان را در لحظه می‌گیرد و watchdog اگر تسکی گیر کند دستگاه را ریست می‌کند.',
 ],
 'example': [
   'برق ورودی وصل می‌شود: اندازه‌گیری آن را می‌بیند؛ مسیر تغذیه بعد از ۵ ثانیه میکرو را روی ورودی می‌برد.',
   'شارژر هر دو باتری را شارژ می‌کند؛ چراغ زرد چشمک می‌زند و پنل همان وضعیت را نشان می‌دهد.',
   'یک باتری کامل می‌شود و دیگری نه: عدم‌توازن زیر نظر است؛ اگر اختلاف ماندگار از حد رد شود رویداد می‌خورد و دستگاه با چراغ و خروجی واکنش نشان می‌دهد.',
 ],
 'flow': [
   N('sec','navy','بخش ۱: راه‌اندازی دستگاه'),
   N('pill','navy','روشن‌شدن دستگاه', ref='osKernelStart: روشن‌شدن و تسک‌ها'),
   N('box','blue','راه‌اندازی سخت‌افزار از راه BSP', ['مبدل و DMA، PWM، پایه‌ها، وقفهٔ جیتر','خواندن تنظیمات ماندگار از حافظه'], ref='func__BspGpio_Init: راه‌اندازی پایه‌ها'),
   N('box','blue','خودآزمایی چراغ‌ها و ساخت تسک‌ها', ['پنج تسک ایستا؛ بدون حافظهٔ پویا'], ref='osKernelStart: ساخت تسک‌ها'),
   N('sec','navy','بخش ۲: چرخهٔ همیشگی — هر پاس کنترل'),
   N('box','blue','۱) اندازه‌گیری: Snapshot تازه و معتبر', ['هر ۱ میلی‌ثانیه؛ همه از همین Snapshot را می‌خوانند']),
   N('box','blue','۲) Fault و خطای فنی شارژر', ['اول از همه قطع باتری (۶) و قفل فنی (۷) بررسی می‌شود'], ref='func__TaskControl: Fault و فنی'),
   N('box','blue','۳) مسیر تغذیهٔ میکرو و عدم‌توازن', ['تغذیهٔ میکرو و اختلاف دو نیم باتری بررسی می‌شود'], ref='func__McuPowerPath_Run'),
   N('box','blue','۴) تعویض خروجی', ['حالت کلی دستگاه و خط محافظ باتری'], ref='func__TaskControl: Changeover'),
   N('box','blue','۵) Charger: فازها و فرمان duty', ['PWM کانال‌ها و رلهٔ قطع نهایی'], ref='func__TaskControl: Charger_Evaluate', note=SIDE('gray',['ترتیب پاس‌ها همیشه همین است','بعد از پاس آخر، از اول شروع می‌شود'],'l')),
   N('box','green','هم‌زمان: نمایش چراغ/بوق و لینک پنل', ['چراغ و بوق از روی وضعیت‌ها؛ پنل از همین‌جا می‌خواند و تنظیم می‌کند'], ref='func__Ui_Tick + func__EspLink_Run'),
   N('dia','yellow','نگهبان‌ها سالم‌اند؟ (جیتر و watchdog)', side=SIDE('red',['تریپ جیتر: توقف فوری کانال در شارژر','گیرکردن تسک: ریست با watchdog'], 'r', 'نه'), elab='بله'),
   N('pill','navy','برگشت به اول چرخه'),
 ],
 'tree': T('کل برنامهٔ دستگاه',
   T('راه‌اندازی',
     T('BSP: مبدل، PWM، پایه، وقفه'),
     T('خواندن تنظیمات ماندگار (NVM)'),
     T('خودآزمایی چراغ‌ها')),
   T('تسک اندازه‌گیری: Snapshot هر ۱ms'),
   T('تسک کنترل (ترتیب ثابت)',
     T('خطاها (۶) و خطای فنی (۷)'),
     T('مسیر تغذیهٔ میکرو'),
     T('عدم‌توازن دو نیم'),
     T('تعویض خروجی و حالت کلی'),
     T('شارژر: فازها، PID، رله')),
   T('تسک نمایش: چراغ و بوق'),
   T('تسک ارتباط: لینک ESP و پنل'),
   T('نگهبان‌ها: جیتر و watchdog')),
 'conds': [
   ['برق ورودی آمد','مسیر تغذیه بعد از ۵ ثانیه میکرو را روی ورودی می‌برد و شارژر با دیدن ورودی سالم شارژ را شروع می‌کند.','۲۲ ولت / ۵ ثانیه'],
   ['خطای بزرگی آمد (۶ یا ۷)','اول از همه در ترتیب کنترل دیده می‌شود: شارژ متوقف، حالت کلی خطا و چهرهٔ خطا روی چراغ‌ها.','ترتیب ثابت کنترل'],
   ['باتری ضعیف شد','تعویض خروجی بعد از ۳ ثانیه پایداری خروجی را از باتری جدا می‌کند تا باتری آسیب نبیند.','۲۱ ولت / ۳ ثانیه'],
   ['تسکی گیر کرد','watchdog دستگاه را ریست می‌کند و برنامه از راه‌اندازی سالم شروع می‌کند.','پنجرهٔ watchdog'],
 ],
 'outs': [
   ['به سخت‌افزار','PWM دو کانال شارژ، رلهٔ قطع نهایی، خط محافظ باتری، مسیر تغذیهٔ میکرو، چراغ‌ها و بوق','فقط از راه BSP؛ منطق ماژول‌ها پایه نمی‌شناسد'],
   ['به پنل ESP','همهٔ وضعیت‌ها و اندازه‌گیری‌ها برای نمایش؛ تنظیمات و فرمان‌ها برعکس','لینک فریم نسخه‌دار با CRC-16'],
   ['به حافظهٔ ماندگار','تنظیمات، سابقهٔ خطاها و شمارنده‌ها','رکوردهای نسخه‌دار؛ با خاموشی نمی‌پرد'],
 ],
}

DIRS = {'charging': 'Charger', 'measurement': 'Measurement', 'fault': 'Fault',
        'imbalance': 'Imbalance', 'changeover': 'Changeover', 'ui': 'Ui',
        'protection': 'Protection', 'callut': 'CalLut', 'esplink': 'EspLink',
        'mcupowerpath': 'McuPowerPath', 'jitter': 'Jitter', 'diag': 'Diag',
        'rtos': 'Rtos'}
NAVLABELS = [('system', 'کل سیستم'), ('charging', 'شارژر'), ('measurement', 'اندازه‌گیری'),
             ('fault', 'خطاها'), ('imbalance', 'عدم‌توازن'), ('changeover', 'تعویض خروجی'),
             ('ui', 'چراغ و بوق'), ('protection', 'ناظر حفاظت'), ('callut', 'کالیبراسیون'),
             ('esplink', 'لینک پنل'), ('mcupowerpath', 'تغذیهٔ میکرو'), ('jitter', 'جیتر'),
             ('diag', 'ثبت سابقه'), ('rtos', 'تسک‌ها')]
ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

def target_path(key):
    if key == 'system':
        return os.path.join(ROOT, 'Documentation', 'system_process.html')
    return os.path.join(ROOT, 'Firmware', 'Modules', DIRS[key], MODULES[key]['file'])

GH_BLOB = 'https://github.com/sargoliwork101-ui/ChangeOver/blob/arena/859ce091-changeover/'
GH_TREE = 'https://github.com/sargoliwork101-ui/ChangeOver/tree/arena/859ce091-changeover/'
TOKEN_RE = re.compile(r'[A-Za-z][A-Za-z0-9_]{3,}')

IDENT_RE = re.compile(r'([A-Za-z_][A-Za-z0-9_]*)\s*\(')

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
            base_rank = (6 if tester else 0) + (0 if f.endswith('.c') else 2)
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
                if st.startswith(('*', '//', '/*')):
                    if st.startswith('/*') and '*/' not in st[2:]:
                        in_comment = True
                    continue
                if '/*' in line:
                    line = line.split('/*', 1)[0]
                    in_comment = True
                proto = line.rstrip().endswith(';')
                for ident in set(IDENT_RE.findall(line)):
                    offer(ident, base_rank + (1 if proto else 0), rel, ln)
                for tok in set(TOKEN_RE.findall(line)):
                    offer(tok, base_rank + 4, rel, ln)
    return cand

def resolve_ref(ref, idx, fallback):
    toks = [t for t in TOKEN_RE.findall(ref or '')
            if len(t) >= 4 and ('_' in t or any(ch.isupper() for ch in t[1:]))]
    toks.sort(key=lambda t: (0 if t.startswith('func__') else 1, -len(t)))

    def url(v):
        return GH_BLOB + v[1] + '#L' + str(v[2])

    for t in toks:  # exact, definition tier
        if t in idx and idx[t][0] <= 8:
            return url(idx[t])
    for t in toks:  # suffix, definition tier (e.g. RegulateChannel -> func__Charger_RegulateChannel)
        best = None
        for ident, v in idx.items():
            if ident.endswith('_' + t) and v[0] <= 8:
                if best is None or v[0] < best[0] or (v[0] == best[0] and len(ident) < len(best[1])):
                    best = (v[0], ident, v)
        if best:
            return url(best[2])
    for t in toks:  # exact, any mention (enums, macros)
        if t in idx:
            return url(idx[t])
    return fallback

def main():
    sys_idx = None
    for key, m in MODULES.items():
        path = target_path(key)
        nav = []
        for k2, label in NAVLABELS:
            if k2 != key:
                nav.append({'t': label,
                            'href': os.path.relpath(target_path(k2), os.path.dirname(path)).replace(os.sep, '/')})
        sim_path = os.path.join(ROOT, 'Firmware', 'Modules', 'Ui', 'alarm_simulator.html')
        nav.append({'t': 'شبیه‌ساز آلارم‌ها',
                    'href': os.path.relpath(sim_path, os.path.dirname(path)).replace(os.sep, '/')})
        if sys_idx is None:
            sys_idx = build_index(os.path.join(ROOT, 'Firmware'))
        if key == 'system':
            idx = sys_idx
            fallback = GH_TREE + 'Firmware'
        else:
            idx = build_index(os.path.join(ROOT, 'Firmware', 'Modules', DIRS[key]))
            for k2, v2 in sys_idx.items():
                if k2 not in idx or v2[0] + 5 < idx[k2][0]:
                    idx[k2] = (v2[0] + 5, v2[1], v2[2])
            fallback = GH_TREE + 'Firmware/Modules/' + DIRS[key]
        GO = {
         'system': {'۱) اندازه‌گیری': 'measurement', '۲) Fault': 'fault', '۳) مسیر تغذیه': 'mcupowerpath',
                    '۴) تعویض خروجی': 'changeover', '۵) Charger': 'charging', 'هم‌زمان: نمایش': 'ui',
                    'نگهبان‌ها سالم‌اند': 'jitter'},
         'charging': {'اطلاع به چراغ و بوق': 'ui'},
         'measurement': {'تبدیل با جدول کالیبراسیون': 'callut', 'انتشار Snapshot': 'system'},
         'fault': {'خروجی: بیت خطای ۶': 'charging'},
         'changeover': {'وتوی خروجی فعال است': 'imbalance'},
         'ui': {'خطای فنی (۷) ست است؟': 'sim', 'اضافه‌ولتاژ ورودی هست؟': 'sim', 'خطای ۶ (قطع باتری) ست است؟': 'sim',
                'باتری «خراب» قفل شده؟': 'sim', 'عدم‌توازن قفل شده؟': 'sim', 'چهرهٔ کار روی باتری': 'sim'},
         'esplink': {'نوشتن: فرستادن به ماژول صاحب شناسه': 'system'},
         'diag': {'رکورد آمادهٔ خواندن از پنل است': 'esplink'},
         'jitter': {'شارژر پرچم را می‌خواند': 'charging'},
         'protection': {'ست‌کردن FAULT_ADC': 'fault'},
         'rtos': {'تسک اندازه‌گیری': 'measurement', 'تسک کنترل': 'system', 'تسک ارتباط': 'esplink',
                  'تسک نمایش': 'ui', 'تسک حفاظت': 'protection'},
         'callut': {'نشستن جدول تازه': 'esplink'},
         'imbalance': {'پاک‌شدن‌ها': 'ui'},
        }
        AFTER = {
         ('callut', 'شمارش داخل بازهٔ جدول است؟', 'side'): 'همان مقدارِ لبه برمی‌گردد و تبدیل تمام است',
         ('callut', 'درون‌یابی خطی بین دو نقطه', 'note'): 'خط اصلی، پایین همین باکس',
         ('callut', 'جدول تازه کامل و درست است؟', 'side'): 'جدول قبلی کار می‌کند؛ این درخواست تمام است',
         ('changeover', 'Snapshot معتبر است؟', 'side'): 'همین پاس تمام؛ پاس بعد دوباره از اول',
         ('changeover', 'بیت خطایی ست است؟', 'side'): 'فقط قفل باتری کم به‌روز می‌شود؛ قطع‌وصل نداریم',
         ('changeover', 'وتوی خروجی فعال است؟', 'side'): 'فقط قفل باتری کم به‌روز می‌شود؛ قطع‌وصل نداریم',
         ('changeover', 'شرط قطع ۳ ثانیه پیوسته ماند؟', 'side'): 'وصل دوباره فقط با برگشت ورودی و پایداری آن',
         ('changeover', 'ورودی برگشت؟', 'side'): 'حالت عادی، پایین همین جریان',
         ('changeover', 'حالت عادی از روی حضور ورودی', 'note'): 'پاس بعد از اول همین فلوچارت',
         ('charging', 'ذخیرهٔ حافظه (NVM) در حال انجام است؟', 'side'): 'پایان این پاس؛ پاس بعد از اول',
         ('charging', 'قفل خطای فنی (خطای ۷) از قبل ست شده؟', 'side'): 'پایان این پاس؛ قفل تا خاموش و روشن‌شدن می‌ماند',
         ('charging', 'مود تست دستی پنل فعال است؟', 'side'): 'با ۳ ثانیه سکوت لینک خارج می‌شود و شارژ عادی برمی‌گردد',
         ('charging', 'سیم باتری قطع است؟ (خطای ۶)', 'side'): 'کانال‌های دیگر ادامه دارند؛ این کانال متوقف است',
         ('charging', 'اندازه‌گیری سالم و حالت کلی عادی است؟', 'side'): 'پایان این پاس؛ پاس بعد از اول',
         ('charging', 'ولتاژ ورودی بین ۲۲ و ۲۸ ولت است؟', 'side'): 'پایان این پاس؛ پاس بعد دوباره از اول',
         ('charging', 'ولتاژ ورودی بین ۲۲ و ۲۸ ولت است؟', 'note'): 'خط اصلی؛ این بررسی همیشه موازیِ شارژ است',
         ('charging', 'کانال از پنل خاموش شده؟', 'side'): 'کانال بعدی، پایین همین بخش',
         ('charging', 'قفل باتری خراب یا بودجهٔ عدم‌توازن تمام شده؟', 'side'): 'کانال بعدی، پایین همین بخش',
         ('charging', 'ولتاژ باتری بین ۲ و ۱۵ ولت است؟', 'side'): 'کانال بعدی، پایین همین بخش',
         ('charging', 'جریان از ۹۵۰ میلی‌آمپر بالاتر رفته؟', 'side'): 'کانال بعدی؛ شروع بعدی این کانال با شروع نرم است',
         ('charging', 'اتصال باتری ۱۵ ثانیه پایدار بوده؟', 'side'): 'فازهای شارژ پایین، بدون این کانال',
         ('charging', 'شارژ سریع (BULK)', 'side'): 'خط اصلی، سؤال «به ۱۴٫۳ رسید؟»',
         ('charging', 'شارژ سریع (BULK)', 'note'): 'خط اصلی؛ تریپ در همان کانال شمرده می‌شود',
         ('charging', 'جذب (ABSORB)', 'note'): 'خط اصلی، سؤال «کامل شد؟»',
         ('diag', 'خطایی ست شد؟', 'side'): 'خط اصلی، سؤال بعدی',
         ('diag', 'خاموش یا روشن‌شدن بود؟', 'side'): 'خط اصلی، جمع‌کردن آمار',
         ('diag', 'زمان ذخیره رسید؟ یا فرمان ذخیره آمد؟', 'side'): 'خط اصلی، رکورد آمادهٔ خواندن',
         ('diag', 'رکورد آمادهٔ خواندن از پنل است', 'note'): 'خط اصلی؛ پنل هر وقت خواست رکورد را می‌خواند',
         ('esplink', 'فریم کامل و CRC درست است؟', 'side'): 'منتظر فریم بعدی؛ پاسخی فرستاده نمی‌شود',
         ('esplink', 'نسخه و نوع فریم شناخته‌شده است؟', 'side'): 'منتظر فریم بعدی',
         ('esplink', 'مقدار داخل بازهٔ مجاز بود؟', 'side'): 'پاسخ خطا به پنل؛ مقدار قبلی می‌ماند',
         ('esplink', 'اعمال مقدار + ذخیرهٔ ماندگار (اگر لازم است)', 'note'): 'خط اصلی؛ تله‌متری سر وقت خودش فرستاده می‌شود',
         ('fault', 'مود تست دستی پنل فعال است؟', 'side'): 'پایان این پاس؛ شمارش‌ها فریز می‌مانند',
         ('fault', 'Snapshot معتبر است؟', 'side'): 'پایان این پاس؛ پاس بعد از اول',
         ('fault', 'قانون ۱: نیمِ در حال شارژ بالای ۱۴٫۸ ولت رفت و ۱۵۰ms ماند؟', 'side'): 'خروجی خطای ۶، پایین همین جریان',
         ('fault', 'قانون ۲: ورودی سالم است و یک نیم زیر ۶ ولت افتاد و ۱ ثانیه ماند؟', 'side'): 'خروجی خطای ۶، پایین همین جریان',
         ('fault', 'بازیابی: هر دو نیم بالای ۷ ولت و بدون شارژ، ۱ ثانیه ماندند؟', 'side'): 'خروجی خطای ۶، پایین همین جریان',
         ('fault', 'خروجی: بیت خطای ۶', 'note'): 'شارژر همین بیت را در پاس خودش می‌خواند',
         ('imbalance', 'ورودی‌ها معتبرند؟', 'side'): 'پایان این پاس؛ پاس بعد از اول',
         ('imbalance', 'هر دو نیم هم‌حالت‌اند؟', 'side'): 'نامزد این پاس خالی است؛ پاس بعد دوباره',
         ('imbalance', 'اختلاف بالای حد پنجره ماند و زمان پایداری پر شد؟', 'side'): 'سؤال پایان سیکل، پایین همین جریان',
         ('imbalance', 'سیکل با شارژ کامل تمام شد؟', 'side'): 'سؤال قفل، پایین؛ بدون رویداد تازه',
         ('imbalance', 'ثبت یک رویداد برای این سیکل', 'note'): 'خط اصلی، سؤال قفل',
         ('imbalance', 'رویدادها به حد قفل رسیدند؟ (پیش‌فرض ۱۰)', 'side'): 'قفل به تعویض خروجی و چراغ‌وبوق می‌رود',
         ('jitter', 'قفل پرچم trip همان کانال', 'note'): 'خط اصلی؛ شارژر پرچم را در پاس خودش می‌خواند',
         ('mcupowerpath', 'لبهٔ افتادن حضور ورودی آمد؟ (وقفه)', 'side'): 'خط اصلی؛ تغذیه همین حالا از باتری است',
         ('mcupowerpath', 'لبهٔ افتادن حضور ورودی آمد؟ (وقفه)', 'note'): 'خط اصلی؛ وقفه هر لحظه می‌تواند بیاید',
         ('mcupowerpath', 'Snapshot معتبر است؟', 'side'): 'سؤال‌های پایین این پاس رد می‌شوند',
         ('mcupowerpath', '۵ ثانیه پیوسته بالای ۲۲ ماند؟', 'side'): 'سؤال برگشت، پایین؛ هنوز از باتری هستیم',
         ('mcupowerpath', 'ورودی زیر ۲۱٫۵ ولت افتاد؟', 'side'): 'پایان این پاس؛ پاس بعد از اول',
         ('measurement', 'فریم کامل و پایدار از مبدل رسید؟', 'side'): 'بقیهٔ این پاس رد می‌شود؛ پاس بعد (۱ms) از اول',
         ('measurement', 'فریم کامل و پایدار از مبدل رسید؟', 'note'): 'خط اصلی؛ DMA همیشه در پس‌زمینه کار می‌کند',
         ('measurement', 'تنظیم فیلتر از پنل عوض شده؟', 'side'): 'خط اصلی، تمیزکردن جریان و ولتاژ',
         ('protection', 'Snapshot معتبر است؟', 'side'): 'پایان این پاس؛ بیت پاک است',
         ('protection', 'ست‌کردن FAULT_ADC', 'note'): 'خط اصلی؛ این ماژول فعلاً کاری نمی‌کند',
         ('rtos', 'تسک کنترل — ترتیب ثابت تصمیم', 'note'): 'خط اصلی؛ ترتیب هر پاس همین است',
         ('rtos', 'تسکی سر وقت اعلام زنده‌بودن نکرد؟', 'side'): 'دستگاه از اول روشن می‌شود',
         ('ui', 'Snapshot معتبر است؟', 'side'): 'نمایش این پاس همین‌جا می‌ایستد',
         ('ui', 'خطای فنی (۷) ست است؟', 'side'): 'تمام؛ بقیهٔ سؤال‌ها این پاس پرسیده نمی‌شوند',
         ('ui', 'خطای فنی (۷) ست است؟', 'note'): 'خط اصلی؛ این فقط توضیح اولویت است',
         ('ui', 'اضافه‌ولتاژ ورودی هست؟', 'side'): 'تمام؛ بقیهٔ سؤال‌ها این پاس پرسیده نمی‌شوند',
         ('ui', 'خطای ۶ (قطع باتری) ست است؟', 'side'): 'تمام؛ بقیهٔ سؤال‌ها این پاس پرسیده نمی‌شوند',
         ('ui', 'باتری «خراب» قفل شده؟', 'side'): 'تمام؛ بقیهٔ سؤال‌ها این پاس پرسیده نمی‌شوند',
         ('ui', 'عدم‌توازن قفل شده؟', 'side'): 'تمام؛ بقیهٔ سؤال‌ها این پاس پرسیده نمی‌شوند',
         ('ui', 'شارژ کامل است؟', 'side'): 'تمام؛ چهرهٔ نمایش نهایی همین است',
         ('ui', 'استراحت حرارتی فعال است؟', 'side'): 'تمام؛ چهرهٔ نمایش نهایی همین است',
         ('ui', 'کانالی در حال شارژ است؟', 'side'): 'تمام؛ چهرهٔ نمایش نهایی همین است',
         ('system', '۵) Charger: فازها و فرمان duty', 'note'): 'خط اصلی؛ بعد از پاس آخر، چرخه از اول',
         ('system', 'نگهبان‌ها سالم‌اند؟ (جیتر و watchdog)', 'side'): 'ریست دستگاه یا توقف همان کانال؛ چرخه از اول',
        }
        for n in m['flow']:
            if n.get('ref'):
                n['rurl'] = resolve_ref(n['ref'], idx, fallback)
            RET_END = {
             ('charging', 'ذخیرهٔ حافظه (NVM) در حال انجام است؟'), ('charging', 'قفل خطای فنی (خطای ۷) از قبل ست شده؟'),
             ('charging', 'مود تست دستی پنل فعال است؟'), ('charging', 'اندازه‌گیری سالم و حالت کلی عادی است؟'),
             ('charging', 'ولتاژ ورودی بین ۲۲ و ۲۸ ولت است؟'),
             ('fault', 'مود تست دستی پنل فعال است؟'), ('fault', 'Snapshot معتبر است؟'),
             ('imbalance', 'ورودی‌ها معتبرند؟'), ('imbalance', 'هر دو نیم هم‌حالت‌اند؟'),
             ('changeover', 'Snapshot معتبر است؟'),
             ('esplink', 'فریم کامل و CRC درست است؟'), ('esplink', 'نسخه و نوع فریم شناخته‌شده است؟'),
             ('esplink', 'مقدار داخل بازهٔ مجاز بود؟'),
             ('measurement', 'فریم کامل و پایدار از مبدل رسید؟'),
             ('callut', 'شمارش داخل بازهٔ جدول است؟'), ('callut', 'جدول تازه کامل و درست است؟'),
             ('mcupowerpath', 'لبهٔ افتادن حضور ورودی آمد؟ (وقفه)'), ('mcupowerpath', 'Snapshot معتبر است؟'),
             ('system', 'نگهبان‌ها سالم‌اند؟ (جیتر و watchdog)'),
            }
            for kind in ('side', 'note'):
                if n.get(kind):
                    aft = None
                    for (k2, t2, kind2), v2 in AFTER.items():
                        if k2 == key and kind2 == kind and (n.get('t') or '').startswith(t2):
                            aft = v2
                            break
                    if aft:
                        n[kind]['after'] = aft
                    if kind == 'side' and n[kind].get('lab') in ('بله', 'نه'):
                        is_end = any(k2 == key and (n.get('t') or '').startswith(t2) for (k2, t2) in RET_END)
                        n[kind]['ret'] = 'END' if is_end else 'NEXT'
                        n[kind].pop('after', None)
            for sub, tgt in GO.get(key, {}).items():
                if sub in (n.get('t') or ''):
                    if tgt == 'sim':
                        tp = os.path.join(ROOT, 'Firmware', 'Modules', 'Ui', 'alarm_simulator.html')
                    else:
                        tp = target_path(tgt)
                    n['go'] = os.path.relpath(tp, os.path.dirname(path)).replace(os.sep, '/')
                    break
        mm = dict(m); mm['nav'] = nav
        html = TEMPLATE.replace('__TITLE__', m['title'])
        html = html.replace('/*__DATA__*/', json.dumps(mm, ensure_ascii=False))
        with open(path, 'w', encoding='utf-8') as f:
            f.write(html)
        print('wrote', path)

if __name__ == '__main__':
    main()
