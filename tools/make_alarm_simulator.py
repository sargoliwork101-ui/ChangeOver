# -*- coding: utf-8 -*-
"""Generate Firmware/Modules/Ui/alarm_simulator.html — interactive alarm simulator.

Every pattern comes from the real firmware defaults (ui_led.h, imbalance.h,
charger.h). Clicking an alarm replays its LED/buzzer pattern.
"""
import os

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
GH = 'https://github.com/sargoliwork101-ui/ChangeOver/blob/arena/bb1aee1b-changeover/Firmware/Modules/Ui/ui_led.c'


def line_of(token):
    path = os.path.join(ROOT, 'Firmware', 'Modules', 'Ui', 'ui_led.c')
    with open(path, encoding='utf-8', errors='ignore') as f:
        for i, line in enumerate(f, 1):
            if token in line and not line.strip().startswith(('*', '//')):
                return i
    return 1


SCEN = [
    # id, title, group, when, led text, beep text, fix, func token, kind, params
    ('selftest', 'خودآزمایی روشن‌شدن', 'عادی', 'هر بار که دستگاه روشن می‌شود، نیم ثانیه.',
     'هر سه چراغ (سبز، زرد، قرمز) ۵۰۰ میلی‌ثانیه روشن و بعد خاموش می‌شوند (در شبیه‌ساز هر ۶ ثانیه تکرار می‌شود تا دیده شود).',
     'بدون بوق.', 'خودکار تمام می‌شود.', 'func__Ui_Init', 'selftest', {'page': 'selftest'}),
    ('tech7', 'خطای فنی شارژر (خطای ۷)', 'خطاها', 'شارژر خرابی فنی را قفل کرده است (مثل duty بدون جریان، یا رلهٔ باز با تریپ JIT).',
     'چراغ قرمز چشمک می‌زند: دورهٔ ۱ ثانیه، ۵۰٪ روشن.',
     'هر ۳ ثانیه، ۳ بوق کوتاه (۲۰۰ میلی‌ثانیه، فاصلهٔ ۱۰۰).',
     'فقط با خاموش و روشن‌کردن برد و رفع عیب سخت‌افزاری پاک می‌شود.',
     'func__Ui_ScenarioTechnicalFault_Tick', 'blink', {'g': [1000, 50], 'y': [1000, 50], 'r': [1000, 50],
                                                       'beep': [3000, [[0, 200], [300, 200], [600, 200]]],
                                                       'page': 'fault_7_technical'}),
    ('ov', 'اضافه‌ولتاژ ورودی', 'خطاها', 'ولتاژ ورودی از ۲۸ ولت بالاتر رفته است.',
     'چراغ قرمز چشمک می‌زند: دورهٔ ۱ ثانیه، ۵۰٪ روشن.',
     'هر ۱۰ ثانیه، یک بوق ۱ ثانیه‌ای.',
     'با پایین‌آمدن ورودی زیر ۲۷ ولت خودش پاک می‌شود.',
     'func__Ui_ScenarioInputOverVoltage_Tick', 'blink', {'g': 'on', 'r': [1000, 50],
                                                        'beep': [10000, [[0, 1000]]],
                                                        'page': 'input_overvoltage'}),
    ('fault6', 'خطای ۶: قطع باتری', 'خطاها', 'سیم یک نیم باتری قطع شده یا باتری برداشته شده است.',
     'چراغ قرمز چشمک می‌زند: دورهٔ ۱ ثانیه، ۵۰٪ روشن.',
     'هر ۳ ثانیه، ۳ بوق کوتاه.',
     'با وصل‌کردن باتری و پایداری ولتاژ، خودش پاک می‌شود.',
     'func__Ui_ScenarioBatLost_Tick', 'blink', {'g': 'on', 'r': [1000, 50],
                                               'beep': [3000, [[0, 233], [333, 233], [666, 233]]],
                                               'page': 'fault_6_battery_lost'}),
    ('dead', 'باتری خراب (قفل)', 'خطاها', 'باتری ۲۴ ساعت شارژ نشده و خراب حساب شده است (سناریوی ۶ شارژر).',
     'چراغ قرمز ثابت روشن می‌ماند.',
     'یک بوق هنگام قفل‌شدن؛ بوق دوره‌ای از پنل قابل تنظیم است.',
     'فقط با تعویض باتری (نبود باتری ۳ ثانیه) پاک می‌شود.',
     'func__Ui_ScenarioDeadBattery_Tick', 'blink', {'r': 'on', 'beep': None, 'page': 'dead_battery'}),
    ('imbal', 'عدم‌توازن قفل‌شده', 'خطاها', 'اختلاف دو نیم باتری در ۱۰ سیکل از حد گذشت و قفل شد.',
     'چراغ قرمز چشمک می‌زند: دورهٔ ۱ ثانیه، ۵۰٪ روشن. سبز و زرد خاموش.',
     'پیش‌فرض: هر ۱ ساعت یک بوق ۲۰۰ میلی‌ثانیه (در شبیه‌ساز سریع‌تر پخش می‌شود).',
     'طبق قانون عدم‌توازن پاک می‌شود (نبود باتری یا پایان بودجه/چرخهٔ پاک).',
     'func__Ui_ScenarioImbalance_Tick', 'blink', {'r': [1000, 50], 'beep': [10000, [[0, 200]]],
                                                 'page': 'imbalance_latched'}),
    ('full', 'شارژ کامل', 'عادی', 'همهٔ کانال‌های فعال به نگه‌داری (FLOAT) رسیده‌اند.',
     'چراغ سبز ثابت، زرد خاموش.', 'بدون بوق.', '—',
     'func__Ui_ScenarioInputOk', 'blink', {'g': 'on', 'page': 'charge_full'}),
    ('charging', 'در حال شارژ', 'عادی', 'ورودی حاضر است و شارژ ادامه دارد. درصد را با لغزنده عوض کن.',
     'چراغ زرد چشمک می‌زند: هرچه شارژ کمتر، زرد بیشتر روشن می‌ماند (زمان روشن = درصد باقی‌مانده از دورهٔ ۱ ثانیه).',
     'بدون بوق.', '—',
     'func__Ui_ScenarioCharging_Tick', 'charge', {'page': 'charging'}),
    ('thermal', 'استراحت حرارتی شارژر', 'عادی', 'شارژر بعد از ۱ ساعت شارژ سنگین، ۱۰ دقیقه استراحت می‌کند.',
     'چراغ زرد چشمک ۱ ثانیه ۵۰٪ + چراغ سبز ثابت.',
     'بدون بوق.', 'بعد از ۱۰ دقیقه خودش به شارژ برمی‌گردد.',
     'func__Ui_ScenarioThermalRest_Tick', 'blink', {'g': 'on', 'y': [1000, 50], 'page': 'thermal_rest'}),
    ('batrun', 'کار روی باتری (بدون ورودی)', 'عادی', 'ورودی نیست و دستگاه از باتری کار می‌کند. درصد باتری را با لغزنده عوض کن.',
     'چراغ سبز چشمک می‌زند: زمان روشن = درصد باتری از دورهٔ ۱ ثانیه.',
     'زیر ۴۰٪ بوق هشدار شروع می‌شود: ۴۰٪ تک‌بوق، ۲۰٪ دوبوق، ۱۰٪ سه‌بوق، ۱٪ بحرانی (در شبیه‌ساز سریع‌تر پخش می‌شود).',
     'با برگشت ورودی تمام می‌شود.',
     'func__Ui_ScenarioBatteryRun_Tick', 'batrun', {'page': 'battery_run'}),
]

HTML = """<!DOCTYPE html>
<html lang="fa" dir="rtl">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>شبیه‌ساز آلارم‌ها و چراغ‌ها</title>
<style>
@import url('https://fonts.googleapis.com/css2?family=Vazirmatn:wght@400;700&display=swap');
body{font-family:Vazirmatn,Tahoma,sans-serif;background:#12151B;color:#E6EAF2;margin:0;padding:24px 14px 48px;line-height:1.9}
.wrap{max-width:none;margin:0}
h1{font-size:24px;margin:0 0 4px}
h2{font-size:20px;border-bottom:3px solid #35507A;padding-bottom:4px;margin:30px 0 8px}
a{color:#7FB0E6}
.small{font-size:13.5px;color:#9AA7BC}
.card{background:#1B2029;border:1.5px solid #35507A;border-radius:12px;padding:14px 20px;margin:12px 0}
.grid{display:grid;grid-template-columns:300px 1fr;gap:14px}
@media(max-width:760px){.grid{grid-template-columns:1fr}}
button.alarm{display:block;width:100%;text-align:right;background:#232B3A;color:#E6EAF2;border:1.5px solid #35507A;border-radius:10px;padding:10px 14px;margin:0 0 8px;font-family:inherit;font-size:15.5px;cursor:pointer}
button.alarm:hover{border-color:#7FB0E6}
button.alarm.on{background:#24466B;border-color:#7FB0E6}
.grp{font-size:13px;color:#9AA7BC;margin:10px 0 4px}
.panel{background:#0D1017;border:2px solid #35507A;border-radius:16px;padding:22px;text-align:center}
.leds{display:flex;justify-content:center;gap:34px;margin:10px 0 4px}
.led{width:64px;height:64px;border-radius:50%;background:#22262E;border:2px solid #3A4356;transition:background .05s}
.led small{display:block;margin-top:70px;font-size:13px;color:#9AA7BC}
.led.g.on{background:#2FBF71;box-shadow:0 0 34px #2FBF71}
.led.y.on{background:#FFC61A;box-shadow:0 0 34px #FFC61A}
.led.r.on{background:#F03A47;box-shadow:0 0 34px #F03A47}
#buzz{display:inline-block;margin-top:16px;padding:2px 14px;border-radius:20px;border:1.5px solid #3A4356;color:#9AA7BC;font-size:14px}
#buzz.on{background:#7B2D8B;border-color:#C77DFF;color:#fff}
.kv b{color:#7FB0E6}
input[type=range]{width:100%}
.small{font-size:13.5px;color:#9AA7BC}
/*__MENUCSS__*/
</style>
</head>
<body>
<div class="wrap">
<nav class="topnav" id="menu" aria-label="منوی صفحه‌های دستگاه"></nav>
<h1>شبیه‌ساز آلارم‌ها و چراغ‌ها</h1>
<p>روی هر آلارم یا حالت بزن تا چراغ‌ها و بوق، دقیقاً با همان الگوی واقعی دستگاه (مقادیر پیش‌فرض کد) پخش شوند. برای شنیدن بوق، صدای دستگاهت را باز کن.</p>
<div class="grid">
<div class="card" id="list"></div>
<div>
<div class="panel">
<div class="leds">
<div><div class="led g" id="ledG"></div><small>سبز</small></div>
<div><div class="led y" id="ledY"></div><small>زرد</small></div>
<div><div class="led r" id="ledR"></div><small>قرمز</small></div>
</div>
<div id="buzz">بوق: ساکت</div>
<div id="sliderRow" style="display:none;margin-top:18px">
<label id="sliderLab">درصد</label>
<input type="range" id="pct" min="0" max="100" value="60">
</div>
</div>
<div class="card">
<div class="kv"><b>الان:</b> <span id="dTitle">یک مورد را از فهرست انتخاب کن</span></div>
<div class="kv"><b>کِی پیش می‌آید:</b> <span id="dWhen">—</span></div>
<div class="kv"><b>چراغ:</b> <span id="dLed">—</span></div>
<div class="kv"><b>بوق:</b> <span id="dBeep">—</span></div>
<div class="kv"><b>رفع:</b> <span id="dFix">—</span></div>
<div class="kv"><b>کد:</b> <a id="dCode" href="#" target="_blank">—</a></div>
<div class="kv"><b>توضیح کامل:</b> <a id="dPage" href="#" style="display:none">—</a></div>
<p class="small">الگوها از مقادیر پیش‌فرض کد خوانده شده‌اند (ui_led.h و سربرگ ماژول‌ها)؛ از پنل می‌شود بیشترشان را تنظیم کرد. دوره‌های خیلی بلند بوق در این شبیه‌ساز سریع‌تر پخش می‌شوند تا شنیده شوند.</p>
<p><label><input type="checkbox" id="snd" checked> صدای بوق روشن باشد</label></p>
</div>
</div>
</div>
</div>
<script>
var SCEN = __SCEN__;
/*__MENUJS__*/
var MENU = /*__MENU__*/;
var cur = null, actx = null, prevNow = 0;
var ledG = document.getElementById('ledG'), ledY = document.getElementById('ledY'),
    ledR = document.getElementById('ledR'), buzz = document.getElementById('buzz');
function audio(){
  if (!actx){ try { actx = new (window.AudioContext || window.webkitAudioContext)(); } catch(e){} }
  if (actx && actx.state === 'suspended') actx.resume();
}
function beep(dur){
  buzz.classList.add('on'); buzz.textContent = 'بوق: در حال پخش';
  setTimeout(function(){ buzz.classList.remove('on'); buzz.textContent = 'بوق: ساکت'; }, Math.min(dur, 400));
  if (!document.getElementById('snd').checked || !actx) return;
  var o = actx.createOscillator(), g = actx.createGain();
  o.type = 'square'; o.frequency.value = 2400;
  g.gain.setValueAtTime(0.0001, actx.currentTime);
  g.gain.exponentialRampToValueAtTime(0.12, actx.currentTime + 0.01);
  g.gain.exponentialRampToValueAtTime(0.0001, actx.currentTime + dur / 1000);
  o.connect(g); g.connect(actx.destination);
  o.start(); o.stop(actx.currentTime + dur / 1000 + 0.05);
}
function ledState(spec, now){
  if (!spec) return false;
  if (spec === 'on') return true;
  var ph = now % spec[0];
  return ph < spec[0] * spec[1] / 100;
}
function frame(){
  requestAnimationFrame(frame);
  if (!cur) return;
  var now = Date.now(), pct = +document.getElementById('pct').value;
  var g = false, y = false, r = false, beeps = null;
  if (cur.kind === 'selftest'){
    /* [FA] همان ترتیب واقعی: قرمز، زرد، سبز — هر کدام ۵۰۰ میلی‌ثانیه */
    var ph6 = now % 6000;
    r = ph6 < 500; y = ph6 >= 500 && ph6 < 1000; g = ph6 >= 1000 && ph6 < 1500;
  } else if (cur.kind === 'charge'){
    /* [FA] روشن‌بودن زرد = مانده تا پر شدن، با کف ۲٪ و حداقل ۱۵۰ میلی‌ثانیه */
    var rem = Math.max(100 - pct, 2);
    var on = Math.max(rem * 10, 150);
    y = (now % 1000) < on;
    g = true;
  } else if (cur.kind === 'batrun'){
    g = pct >= 1 && (now % 1000) < pct * 10;
    if (pct >= 40) beeps = null;
    else if (pct >= 20) beeps = [6000, [[0, 1000]]];
    else if (pct >= 10) beeps = [6000, [[0, 1000], [1100, 1000]]];
    else if (pct >= 1) beeps = [6000, [[0, 2000], [2100, 2000], [4200, 2000]]];
    else beeps = [10000, [[0, 5000]]];
  } else {
    g = ledState(cur.p.g, now); y = ledState(cur.p.y, now); r = ledState(cur.p.r, now);
    beeps = cur.p.beep || null;
  }
  ledG.classList.toggle('on', !!g); ledY.classList.toggle('on', !!y); ledR.classList.toggle('on', !!r);
  if (beeps && prevNow){
    var per = beeps[0], cyc = Math.floor(now / per);
    beeps[1].forEach(function(pu){
      var at = cyc * per + pu[0];
      if (prevNow < at && now >= at) beep(pu[1]);
    });
  }
  prevNow = now;
}
function pick(id){
  audio();
  cur = SCEN.filter(function(x){ return x.id === id; })[0];
  prevNow = 0;
  document.querySelectorAll('button.alarm').forEach(function(b){ b.classList.toggle('on', b.dataset.id === id); });
  document.getElementById('dTitle').textContent = cur.title;
  document.getElementById('dWhen').textContent = cur.when;
  document.getElementById('dLed').textContent = cur.led;
  document.getElementById('dBeep').textContent = cur.beep;
  document.getElementById('dFix').textContent = cur.fix;
  var dc = document.getElementById('dCode');
  dc.textContent = cur.func; dc.href = cur.url;
  var dp = document.getElementById('dPage');
  if (dp){
    if (cur.p && cur.p.page){
      dp.href = '../../../Documentation/alarms/' + cur.p.page + '.html';
      dp.textContent = 'فلوچارت کامل این مورد';
      dp.style.display = 'inline';
    } else {
      dp.style.display = 'none';
    }
  }
  var sr = document.getElementById('sliderRow');
  if (cur.kind === 'charge'){ sr.style.display = 'block'; document.getElementById('sliderLab').textContent = 'درصد شارژ باتری:'; }
  else if (cur.kind === 'batrun'){ sr.style.display = 'block'; document.getElementById('sliderLab').textContent = 'درصد باقی‌ماندهٔ باتری:'; }
  else sr.style.display = 'none';
  if (cur.kind === 'dead') setTimeout(function(){ beep(300); }, 60);
}
var list = document.getElementById('list'), lastGrp = '';
SCEN.forEach(function(x){
  if (x.grp !== lastGrp){ lastGrp = x.grp; var d = document.createElement('div'); d.className = 'grp'; d.textContent = lastGrp === 'خطاها' ? 'آلارم‌ها و خطاها' : 'حالت‌های عادی'; list.appendChild(d); }
  var b = document.createElement('button');
  b.className = 'alarm'; b.dataset.id = x.id; b.textContent = x.title;
  b.addEventListener('click', function(){ pick(x.id); });
  list.appendChild(b);
});
renderMenu(MENU, 'menu');
requestAnimationFrame(frame);
</script>
</body>
</html>
"""


def main():
    scen = []
    for (sid, title, grp, when, led, beep, fix, func, kind, params) in SCEN:
        scen.append({'id': sid, 'title': title, 'grp': grp, 'when': when, 'led': led,
                     'beep': beep, 'fix': fix, 'func': func, 'kind': kind, 'p': params,
                     'url': GH + '#L' + str(line_of(func))})
    import json
    import make_process_pages as mpp
    out_dir = os.path.dirname(os.path.join(ROOT, 'Firmware', 'Modules', 'Ui', 'alarm_simulator.html'))
    menu = mpp.build_menu(out_dir, 'sim')
    html = HTML.replace('__SCEN__', json.dumps(scen, ensure_ascii=False))
    html = html.replace('/*__MENUCSS__*/', mpp.MENU_CSS).replace('/*__MENUJS__*/', mpp.MENU_JS)
    html = html.replace('/*__MENU__*/', json.dumps(menu, ensure_ascii=False))
    out = os.path.join(ROOT, 'Firmware', 'Modules', 'Ui', 'alarm_simulator.html')
    with open(out, 'w', encoding='utf-8') as f:
        f.write(html)
    print('wrote', out)


if __name__ == '__main__':
    main()
