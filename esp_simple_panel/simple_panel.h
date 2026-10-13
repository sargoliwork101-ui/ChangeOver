/* simple_panel.h - صفحه وب پنل ساده کاربر
   فقط فارسی - طراحی موبایل‌طور، باتری، بدون جریان */

static const char SIMPLE_PANEL_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="fa" dir="rtl">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
<title>ChangeOver - پنل کاربر</title>
<style>
:root{
 --bg:#0f172a;--cd:#1e293b;--in:#0b1222;--ln:#334155;--tx:#e2e8f0;--mu:#94a3b8;
 --ok:#22c55e;--wa:#f59e0b;--er:#ef4444;--ac:#38bdf8;
 --okbg:rgba(34,197,94,.14);--wabg:rgba(245,158,11,.14);--erbg:rgba(239,68,68,.14);
 --radius:16px;
}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg);color:var(--tx);font:14px/1.8 Vazirmatn,system-ui,Tahoma,sans-serif;direction:rtl}
header{position:sticky;top:0;z-index:10;background:rgba(15,23,42,.92);backdrop-filter:blur(8px);border-bottom:1px solid var(--ln);padding:12px 16px;display:flex;justify-content:space-between;align-items:center}
header .brand{font-weight:800;font-size:16px}
header .link{font-size:11px;color:var(--mu);display:flex;gap:8px;align-items:center}
header .dot{width:8px;height:8px;border-radius:50%;background:var(--mu);display:inline-block}
header .dot.on{background:var(--ok);box-shadow:0 0 8px var(--ok)}
header .dot.off{background:var(--er)}
main{max-width:520px;margin:0 auto;padding:14px;display:grid;gap:14px}
.card{background:var(--cd);border:1px solid var(--ln);border-radius:var(--radius);padding:14px 16px;box-shadow:0 6px 20px rgba(0,0,0,.25)}
.card-title{font-weight:700;font-size:13px;color:var(--mu);margin-bottom:10px;letter-spacing:.2px}
.status-card{display:flex;gap:14px;align-items:center;padding:18px 16px}
.status-card .icon{width:54px;height:54px;border-radius:14px;display:grid;place-items:center;font-size:26px;flex:none}
.status-card.ok .icon{background:var(--okbg);border:1px solid rgba(34,197,94,.4)}
.status-card.charge .icon{background:var(--wabg);border:1px solid rgba(245,158,11,.4)}
.status-card.battery .icon{background:rgba(56,189,248,.14);border:1px solid rgba(56,189,248,.4)}
.status-card.fault .icon{background:var(--erbg);border:1px solid rgba(239,68,68,.4)}
.status-card .title{font-weight:800;font-size:16px}
.status-card .subtitle{font-size:12.5px;color:var(--mu);margin-top:2px;line-height:1.6}
.battery-wrap{display:flex;align-items:center;gap:14px;margin:6px 0 10px}
.battery{width:120px;height:58px;border:3px solid var(--ln);border-radius:12px;position:relative;padding:4px;background:var(--in);flex:none}
.battery:after{content:'';position:absolute;right:-10px;top:50%;transform:translateY(-50%);width:6px;height:18px;background:var(--ln);border-radius:0 4px 4px 0}
.battery-level{height:100%;border-radius:6px;transition:width .6s ease,background .6s ease;background:var(--ok)}
.battery-level.low{background:var(--er)}
.battery-level.mid{background:var(--wa)}
.battery-pct{font-size:28px;font-weight:800;letter-spacing:.5px}
.kv{display:flex;justify-content:space-between;align-items:center;padding:7px 0;border-top:1px dashed rgba(51,65,85,.6);font-size:13px}
.kv:first-of-type{border-top:0}
.kv span{color:var(--mu)}
.kv b{font-weight:700;color:var(--tx);font-variant-numeric:tabular-nums;direction:ltr}
.grid2{display:grid;grid-template-columns:1fr;gap:14px}
@media(min-width:520px){.grid2{grid-template-columns:1fr 1fr}}
.half-grid{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.half{background:var(--in);border:1px solid var(--ln);border-radius:12px;padding:10px;text-align:center}
.half-name{font-size:11px;color:var(--mu);margin-bottom:6px}
.mini-battery{width:100%;height:18px;border:2px solid var(--ln);border-radius:6px;padding:2px;background:var(--bg);margin:0 auto 6px;position:relative}
.mini-level{height:100%;border-radius:3px;transition:width .6s ease;background:var(--ok)}
.mini-level.low{background:var(--er)} .mini-level.mid{background:var(--wa)}
.half-pct{font-weight:800;font-size:16px}
.half-v{font-size:11px;color:var(--mu);direction:ltr;font-variant-numeric:tabular-nums}
.imb{margin-top:8px;font-size:11px;color:var(--wa);background:rgba(245,158,11,.08);border:1px solid rgba(245,158,11,.25);border-radius:8px;padding:6px 8px;display:none}
.imb.on{display:block}
.leds{display:flex;gap:12px;flex-wrap:wrap}
.led{display:flex;align-items:center;gap:6px;font-size:12px;color:var(--mu);background:var(--in);border:1px solid var(--ln);border-radius:999px;padding:5px 10px}
.dot-led{width:10px;height:10px;border-radius:50%;background:#334155;display:inline-block;box-shadow:inset 0 0 4px rgba(0,0,0,.6)}
.dot-led.on-green{background:var(--ok);box-shadow:0 0 10px var(--ok)}
.dot-led.on-yellow{background:var(--wa);box-shadow:0 0 10px var(--wa)}
.dot-led.on-red{background:var(--er);box-shadow:0 0 10px var(--er)}
.dot-led.on-buzzer{background:var(--ac);box-shadow:0 0 10px var(--ac)}
.charger-states{margin-top:10px;font-size:11px;color:var(--mu);display:flex;gap:8px;flex-wrap:wrap}
.charger-states span{background:var(--in);border:1px solid var(--ln);border-radius:8px;padding:3px 8px}
.fault-card{border-right:4px solid var(--ln)}
.fault-card.warn{border-right-color:var(--wa);background:rgba(245,158,11,.06)}
.fault-card.err{border-right-color:var(--er);background:rgba(239,68,68,.06)}
.fault-card.ok{border-right-color:var(--ok)}
#faultList{font-size:13px;line-height:1.9}
#faultList .ok{color:var(--ok)} #faultList .warn{color:var(--wa)} #faultList .err{color:var(--er)}
.footer{display:flex;justify-content:space-between;gap:10px;font-size:10.5px;color:var(--mu);padding:2px 4px}
</style>
</head>
<body>
<header>
 <div class="brand">🔋 ChangeOver - پنل کاربر</div>
 <div class="link"><span id="hdrDot" class="dot"></span><span id="hdrText">قطع</span></div>
</header>
<main>
 <div id="statusCard" class="card status-card battery">
  <div class="icon" id="statusIcon">🔋</div>
  <div>
   <div class="title" id="statusTitle">در حال اتصال به برد...</div>
   <div class="subtitle" id="statusSub">لطفا صبر کنید</div>
  </div>
 </div>

 <div class="card">
  <div class="card-title">باتری اصلی - مثل موبایل</div>
  <div class="battery-wrap">
   <div class="battery"><div class="battery-level" id="batLevel" style="width:0%"></div></div>
   <div class="battery-pct" id="batPct">--%</div>
  </div>
  <div class="kv"><span>ولتاژ پک</span><b id="vPack">-- V</b></div>
  <div class="kv"><span>ولتاژ ورودی</span><b id="vInput">-- V</b></div>
  <div class="kv"><span>وضعیت برق شهر</span><b id="inputState">--</b></div>
 </div>

 <div class="grid2">
  <div class="card">
   <div class="card-title">نیم‌باتری‌ها</div>
   <div class="half-grid">
    <div class="half">
     <div class="half-name">باتری بالا</div>
     <div class="mini-battery"><div class="mini-level" id="batHighLevel" style="width:0%"></div></div>
     <div class="half-pct" id="batHighPct">--%</div>
     <div class="half-v" id="vHigh">-- V</div>
    </div>
    <div class="half">
     <div class="half-name">باتری پایین</div>
     <div class="mini-battery"><div class="mini-level" id="batLowLevel" style="width:0%"></div></div>
     <div class="half-pct" id="batLowPct">--%</div>
     <div class="half-v" id="vLow">-- V</div>
    </div>
   </div>
   <div class="imb" id="imbBox"></div>
  </div>

  <div class="card">
   <div class="card-title">چراغ‌های روی برد</div>
   <div class="leds">
    <div class="led"><span id="dotGreen" class="dot-led"></span>سبز</div>
    <div class="led"><span id="dotYellow" class="dot-led"></span>زرد</div>
    <div class="led"><span id="dotRed" class="dot-led"></span>قرمز</div>
   </div>
   <div class="charger-states">
    <span id="ch1State">کانال۱: --</span>
    <span id="ch2State">کانال۲: --</span>
   </div>
   <div style="margin-top:8px;font-size:11px;color:var(--mu)">چشمک‌ها شبیه‌سازی ساده از روی درصد و حالت شارژ است</div>
  </div>
 </div>

 <div id="faultCard" class="card fault-card">
  <div class="card-title">پیغام سیستم</div>
  <div id="faultList">در انتظار داده...</div>
 </div>

 <div class="footer">
  <span id="linkState">لینک: --</span>
  <span id="seqInfo">--</span>
 </div>
</main>

<script>
const T_VIN=14, T_V24=15, T_VLOW=17, T_VHIGH=18, T_FAULT=19, T_STATE1=6, T_STATE2=13, T_IMB=25;
const FAULT_ADC=1, FAULT_BAT_LOST=1<<6, FAULT_TECH=1<<7;

function vToPctPack(v){
 let min=21000, max=29000;
 if(v<=min) return 0;
 if(v>=max) return 100;
 return Math.floor(((v-min)*100)/(max-min));
}
function vToPctHalf(v){
 let min=10500, max=14500;
 if(v<=min) return 0;
 if(v>=max) return 100;
 return Math.floor(((v-min)*100)/(max-min));
}
function fmtV(mv){
 if(mv==null || mv<=0) return '-- V';
 return (mv/1000).toFixed(2)+' V';
}
function stateText(s){
 const map={0:'خاموش',1:'شارژ اصلی',2:'جذب',3:'شناور - پر',4:'راه‌اندازی',5:'انتظار جیتر',6:'انتظار ورودی',7:'خطای نهایی',8:'باتری قطع',9:'دستی'};
 return map[s]||('حالت '+s);
}
let lastData=null;
let blinkPhase=0;
setInterval(()=>{ blinkPhase=(blinkPhase+1)%20; renderLeds(); }, 100);

function renderLeds(){
 if(!lastData || !lastData.on) return;
 let pct=lastData.pctPack;
 let inputPresent=lastData.inputPresent;
 let fault=lastData.faultMask;
 let charging = (lastData.state1==1||lastData.state1==2||lastData.state2==1||lastData.state2==2);
 let isFault = (fault & (FAULT_ADC|FAULT_BAT_LOST|FAULT_TECH))!==0;

 let g=document.getElementById('dotGreen');
 let y=document.getElementById('dotYellow');
 let r=document.getElementById('dotRed');
 g.className='dot-led'; y.className='dot-led'; r.className='dot-led';

 if(isFault){
   r.classList.add('on-red');
 } else if(!inputPresent){
   // روی باتری: سبز چشمک بر اساس درصد
   let on = (pct>=50) ? (blinkPhase < (pct/5)) : (blinkPhase < 4);
   // ساده: درصد بالا روشن بیشتر
   if(blinkPhase < Math.max(2, Math.floor(pct/10)+1)) g.classList.add('on-green');
 } else {
   // برق وصل
   g.classList.add('on-green');
   if(charging){
     if(blinkPhase < 10) y.classList.add('on-yellow');
   } else {
     // پر
   }
 }
}

function render(data){
 lastData=data;
 let vPack=data.t[T_V24]||0;
 let vIn=data.t[T_VIN]||0;
 let vLow=data.t[T_VLOW]||0;
 let vHigh=data.t[T_VHIGH]||0;
 let fault=data.t[T_FAULT]||0;
 let s1=data.t[T_STATE1]||0;
 let s2=data.t[T_STATE2]||0;
 let imb=data.t[T_IMB]||0;

 let pctPack=vToPctPack(vPack);
 let pctLow=vToPctHalf(vLow);
 let pctHigh=vToPctHalf(vHigh);

 data.pctPack=pctPack;
 data.faultMask=fault;
 data.state1=s1; data.state2=s2;

 // باتری اصلی
 let batLevel=document.getElementById('batLevel');
 batLevel.style.width=pctPack+'%';
 batLevel.className='battery-level'+(pctPack<20?' low':pctPack<50?' mid':'');
 document.getElementById('batPct').textContent=pctPack+'%';
 document.getElementById('vPack').textContent=fmtV(vPack);
 document.getElementById('vInput').textContent=fmtV(vIn);
 document.getElementById('inputState').textContent=data.inputPresent?'وصل':'قطع';

 // نیم باتری
 document.getElementById('batHighLevel').style.width=pctHigh+'%';
 document.getElementById('batHighLevel').className='mini-level'+(pctHigh<20?' low':pctHigh<50?' mid':'');
 document.getElementById('batLowLevel').style.width=pctLow+'%';
 document.getElementById('batLowLevel').className='mini-level'+(pctLow<20?' low':pctLow<50?' mid':'');
 document.getElementById('batHighPct').textContent=pctHigh+'%';
 document.getElementById('batLowPct').textContent=pctLow+'%';
 document.getElementById('vHigh').textContent=fmtV(vHigh);
 document.getElementById('vLow').textContent=fmtV(vLow);

 let imbBox=document.getElementById('imbBox');
 if(imb>300){
   imbBox.className='imb on';
   imbBox.textContent='اختلاف دو باتری: '+(imb)+' میلی‌ولت - نیاز به بررسی';
 } else { imbBox.className='imb'; imbBox.textContent=''; }

 // شارژر
 document.getElementById('ch1State').textContent='کانال۱: '+stateText(s1);
 document.getElementById('ch2State').textContent='کانال۲: '+stateText(s2);

 // وضعیت کلی
 let statusCard=document.getElementById('statusCard');
 let statusTitle=document.getElementById('statusTitle');
 let statusSub=document.getElementById('statusSub');
 let statusIcon=document.getElementById('statusIcon');
 let isFault = (fault & (FAULT_ADC|FAULT_BAT_LOST|FAULT_TECH))!==0;

 if(!data.on){
   statusCard.className='card status-card fault';
   statusTitle.textContent='ارتباط با برد قطع است';
   statusSub.textContent='کابل یا برد را بررسی کنید';
   statusIcon.textContent='⛔';
 } else if(fault & FAULT_BAT_LOST){
   statusCard.className='card status-card fault';
   statusTitle.textContent='باتری قطع شده';
   statusSub.textContent='سیم باتری را چک کنید';
   statusIcon.textContent='🔌';
 } else if(fault & FAULT_TECH){
   statusCard.className='card status-card fault';
   statusTitle.textContent='خطای فنی برد';
   statusSub.textContent='برد نیاز به بررسی دارد';
   statusIcon.textContent='⚠️';
 } else if(fault & FAULT_ADC){
   statusCard.className='card status-card fault';
   statusTitle.textContent='خطای اندازه‌گیری';
   statusSub.textContent='سنسورها را بررسی کنید';
   statusIcon.textContent='⚠️';
 } else if(!data.inputPresent){
   statusCard.className='card status-card battery';
   statusTitle.textContent='روی باتری - برق شهر قطع است';
   statusSub.textContent='شارژ باقی‌مانده: '+pctPack+'%';
   statusIcon.textContent='🔋';
 } else if(s1==3 && s2==3){
   statusCard.className='card status-card ok';
   statusTitle.textContent='باتری پر - برق شهر وصل';
   statusSub.textContent='آماده به کار';
   statusIcon.textContent='✅';
 } else if(s1==1||s1==2||s2==1||s2==2){
   statusCard.className='card status-card charge';
   statusTitle.textContent='در حال شارژ';
   statusSub.textContent='باتری '+pctPack+'% - برق شهر وصل';
   statusIcon.textContent='⚡';
 } else {
   statusCard.className='card status-card ok';
   statusTitle.textContent='برق شهر وصل';
   statusSub.textContent='باتری '+pctPack+'%';
   statusIcon.textContent='🔌';
 }

 // پیغام‌ها
 let faultCard=document.getElementById('faultCard');
 let faultList=document.getElementById('faultList');
 let msgs=[];
 if(!data.on){ msgs.push('<span class=err>⛔ ارتباط با برد قطع است - داده قدیمی است</span>'); faultCard.className='card fault-card err'; }
 else if(fault==0){ msgs.push('<span class=ok>✅ سیستم سالم است</span>'); faultCard.className='card fault-card ok'; }
 else {
   if(fault & FAULT_ADC) msgs.push('<span class=err>• خطای اندازه‌گیری - برد نمی‌تواند ولتاژ را بخواند</span>');
   if(fault & FAULT_BAT_LOST) msgs.push('<span class=err>• باتری قطع شده - سیم باتری را وصل کنید</span>');
   if(fault & FAULT_TECH) msgs.push('<span class=err>• خطای فنی - شارژر متوقف شده</span>');
   if(imb>300) msgs.push('<span class=warn>• اختلاف دو باتری زیاد است ('+imb+'mV) - باتری‌ها نامتعادل</span>');
   if(pctPack<=5) msgs.push('<span class=err>• باتری خیلی کم - به زودی خاموش می‌شود</span>');
   else if(pctPack<=20) msgs.push('<span class=warn>• شارژ باتری کم است</span>');
   faultCard.className=(fault & (FAULT_ADC|FAULT_BAT_LOST|FAULT_TECH))?'card fault-card err':'card fault-card warn';
 }
 if(msgs.length==0) msgs.push('<span class=ok>✅ همه چیز عادی است</span>');
 faultList.innerHTML=msgs.join('<br>');

 // لینک
 document.getElementById('linkState').textContent='لینک: '+(data.on?'وصل':'قطع')+' | خطای CRC: '+data.ce;
 document.getElementById('seqInfo').textContent='فریم: '+data.n+' | سن: '+data.age+'ms';
 let hdrDot=document.getElementById('hdrDot');
 hdrDot.className='dot '+(data.on?'on':'off');
 document.getElementById('hdrText').textContent=data.on?'متصل':'قطع';
}

async function poll(){
 try{
  let r=await fetch('/t',{cache:'no-store'});
  let j=await r.json();
  render(j);
 }catch(e){
  // نگهدار
 }
}
poll();
setInterval(poll,1000);
</script>
</body>
</html>
)rawliteral";

static_assert(sizeof(SIMPLE_PANEL_HTML) <= SIMPLE_PANEL_HTML_MAX_BYTES, "ساده پنل از سقف بیشتر شد");
