/**
 * @file    esp_link_panel.ino
 * @brief   [EN] ESP-side ESP-Link bridge: exchanges binary frames with the STM32
 *               over UART (921600 8N1, ESP_AGENT_SPEC.md v1.2 incl. section 5.3 formulas) and exposes a small
 *               dark RTL web panel (Vazirmatn) with three tabs: status, settings, manual test;
 *               the section 5.3 conversion formulas are shown with live values.
 *          [FA] پل ESP-Link سمت ESP: تبادل فریم باینری با STM32 روی UART
 *               (921600 8N1، مطابق ESP_AGENT_SPEC.md نسخه ۱.۲) و یک پنل وب دارک ساده
 *               راست‌به‌چپ با فونت وزیرمتن و سه تب: وضعیت، تنظیمات، تست دستی؛
 *               فرمول‌های تبدیل بخش 5.3 با مقادیر زنده نمایش داده می‌شوند.
 *
 * @note    [EN] Wiring: STM32 PA9 (TX) -> ESP RX, STM32 PA10 (RX) <- ESP TX, common GND.
 *               STM32 PA8 drives ESP CH_PD/EN; this sketch never touches that line.
 *               Wi-Fi AP "ChangeOver-ESP", password "123456789", panel at http://192.168.4.1
 *               No flash storage, no cloud: only MCU <-> ESP data exchange.
 *               Manual test mode (ID 19): the GET_PARAMS keepalive runs only while a browser
 *               is actually polling, so a closed/crashed panel lets the STM32 3 s dead-man trip.
 *          [FA] سیم‌بندی: PA9 به RX ماژول، PA10 به TX ماژول، زمین مشترک.
 *               پایه CH_PD/EN را STM32 (PA8) کنترل می‌کند؛ این برنامه به آن دست نمی‌زند.
 *               وای‌فای "ChangeOver-ESP" با رمز "123456789"، پنل در http://192.168.4.1
 *               بدون حافظه فلش و بدون اینترنت: فقط تبادل داده بین MCU و ESP.
 *               مود تست دستی (شناسه ۱۹): keepalive فقط وقتی مرورگری واقعاً در حال خواندن است ارسال
 *               می‌شود تا با بسته‌شدن پنل، dead-man سه‌ثانیه‌ای STM32 عمل کند.
 */

/* ==================== Board Includes ==================== */
#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
  typedef ESP8266WebServer esp_web_server_t;
#elif defined(ESP32)
  #include <WiFi.h>
  #include <WebServer.h>
  typedef WebServer esp_web_server_t;
#else
  #error "Select an ESP8266 or ESP32 board / برد ESP8266 یا ESP32 را انتخاب کنید"
#endif

#include <stdint.h>
#include <stdbool.h>

/* ==================== Link Constants ==================== */
#define ESP_LINK_BAUD_RATE          921600u
#define ESP_LINK_RX_BUFFER_SIZE     1024u
#define ESP_LINK_SOF_BYTE0          0xAAu
#define ESP_LINK_SOF_BYTE1          0x55u
#define ESP_LINK_HEADER_SIZE        4u
#define ESP_LINK_MAX_PAYLOAD        112u
#define ESP_LINK_TLM_SIZE           84u
#define ESP_LINK_TLM_FIELD_OFFSET   4u
#define ESP_LINK_TLM_FIELD_COUNT    20u
#define ESP_LINK_PARAM_ITEM_SIZE    5u
#define ESP_LINK_TIMEOUT_MS         1000u
#define ESP_LINK_TX_INTERVAL_MS     120u
#define ESP_LINK_KEEPALIVE_MS       1000u
#define ESP_LINK_BROWSER_FRESH_MS   1500u

/* ==================== Message Types ==================== */
#define ESP_MSG_SET_PARAM           0x01u
#define ESP_MSG_GET_PARAMS          0x02u
#define ESP_MSG_TLM_LIVE            0x10u
#define ESP_MSG_PARAM_REPORT        0x11u
#define ESP_MSG_PARAMS_BULK         0x12u

/* ==================== Parameter Constants ==================== */
#define ESP_PARAM_COUNT             20u
#define ESP_PARAM_CHG1_ENABLE       11u
#define ESP_PARAM_CHG2_ENABLE       12u
#define ESP_PARAM_MEDIAN_SIZE       7u
#define ESP_PARAM_MANUAL_TEST_MODE  19u
#define ESP_TLM_FLAG_MANUAL_MODE    0x20u

/* ==================== Wi-Fi / HTTP Constants ==================== */
#define ESP_WIFI_AP_SSID            "ChangeOver-ESP"
#define ESP_WIFI_AP_PASS            "123456789"
#define ESP_HTTP_PORT               80
#define ESP_JSON_BUFFER_SIZE        700u
#define ESP_HTTP_FONT_CACHE         "public, max-age=31536000"

/* ==================== Parser States ==================== */
typedef enum
{
    ESP_RX_WAIT_SOF0 = 0,
    ESP_RX_WAIT_SOF1,
    ESP_RX_WAIT_TYPE,
    ESP_RX_WAIT_LEN,
    ESP_RX_WAIT_PAYLOAD,
    ESP_RX_WAIT_XOR
} esp_rx_state_t;

/* ==================== Parameter Ranges (STM32 clamps too) ==================== */
/* [EN] ID: 0..1 offset, 2..3 gain, 4..6 mV offset (signed), 7 median 1/3/5, 8 avg window, 9..10 eff,
        11..12 charger enable, 13..14 duty ceiling, 15/17 fixed-duty on, 16/18 fixed/manual duty,
        19 manual test mode (v1.2).
   [FA] شناسه: ۰..۱ آفست، ۲..۳ گین، ۴..۶ آفست mV علامت‌دار، ۷ مدین ۱/۳/۵، ۸ پنجره میانگین، ۹..۱۰ بازدهی،
        ۱۱..۱۲ قطع/وصل شارژر، ۱۳..۱۴ سقف duty، ۱۵/۱۷ مود duty فیکس، ۱۶/۱۸ duty فیکس/دستی،
        ۱۹ مود تست دستی (نسخه ۱.۲). */
static const int32_t INT32_T__G__ParamMin[ESP_PARAM_COUNT] = {   0,   0,  100,  100, -2000, -2000, -2000, 1,  1, 100, 100, 0, 0,   0,   0, 0,   0, 0,   0, 0 };
static const int32_t INT32_T__G__ParamMax[ESP_PARAM_COUNT] = { 255, 255, 3000, 3000,  2000,  2000,  2000, 5, 10, 999, 999, 1, 1, 500, 500, 1, 500, 1, 500, 1 };

/* ==================== RX State ==================== */
static esp_rx_state_t ESP_RX_STATE_T__G__RxState = ESP_RX_WAIT_SOF0;
static uint8_t  UINT8_T__G__RxType = 0u;
static uint8_t  UINT8_T__G__RxLen = 0u;
static uint8_t  UINT8_T__G__RxIndex = 0u;
static uint8_t  UINT8_T__G__RxXor = 0u;
static uint8_t  UINT8_T__G__RxPayload[ESP_LINK_MAX_PAYLOAD];

/* ==================== Live Data ==================== */
static uint32_t UINT32_T__G__TlmField[ESP_LINK_TLM_FIELD_COUNT];
static uint16_t UINT16_T__G__TlmSeq = 0u;
static uint8_t  UINT8_T__G__TlmFlags = 0u;
static uint32_t UINT32_T__G__TlmFrameCount = 0u;
static uint32_t UINT32_T__G__LastTlmMs = 0u;
static bool     BOOL__G__TlmSeen = false;

static uint32_t UINT32_T__G__ParamApplied[ESP_PARAM_COUNT];
static bool     BOOL__G__ParamKnown[ESP_PARAM_COUNT];

/* ==================== TX Queue (coalesced, rate limited) ==================== */
static uint32_t UINT32_T__G__TxParamValue[ESP_PARAM_COUNT];
static bool     BOOL__G__TxParamPending[ESP_PARAM_COUNT];
static bool     BOOL__G__ParamUserSet[ESP_PARAM_COUNT];
static bool     BOOL__G__TxGetPending = false;
static uint32_t UINT32_T__G__LastTxMs = 0u;
static uint32_t UINT32_T__G__LastKeepaliveMs = 0u;
static uint32_t UINT32_T__G__LastBrowserPollMs = 0u;
static bool     BOOL__G__BrowserSeen = false;

/* [EN] Send priority: charger cut, manual mode, manual duties, then the rest.
   [FA] اولویت ارسال: قطع شارژر، مود دستی، duty دستی، سپس بقیه. */
static const uint8_t UINT8_T__G__TxOrder[ESP_PARAM_COUNT] = { 11, 12, 19, 16, 18, 15, 17, 13, 14, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };

/* ==================== HTTP ==================== */
static esp_web_server_t ESP_WEB_SERVER_T__G__Server(ESP_HTTP_PORT);
static char CHAR__G__JsonBuffer[ESP_JSON_BUFFER_SIZE];

/* ==================== Web Panel (PROGMEM) ==================== */
static const char ESP_PANEL_HTML[] PROGMEM = R"HTML(<!doctype html><html lang="fa" dir="rtl"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>ChangeOver</title><link rel="stylesheet" href="/f.css"><style>
:root{--bg:#0b0e14;--cd:#121722;--ln:#1e2533;--tx:#e7eaf0;--mu:#8089a0;--ac:#4f8cff;--ok:#2ecc8f;--wa:#f5b942;--er:#ff5c6c}
*{box-sizing:border-box;margin:0}
body{background:var(--bg);color:var(--tx);font:14px/1.6 Vazirmatn,Tahoma,sans-serif;max-width:980px;margin:auto;padding:0 12px 24px}
button,input{font:inherit;color:inherit}
.n{direction:ltr;unicode-bidi:isolate;font-variant-numeric:tabular-nums}
header{display:flex;align-items:center;justify-content:space-between;padding:14px 2px 10px}
h1{font-size:17px;font-weight:700}
.lk{display:flex;align-items:center;gap:7px;font-size:12px;color:var(--mu)}
.lk i{width:8px;height:8px;border-radius:50%;background:var(--er)}.lk.on i{background:var(--ok);box-shadow:0 0 6px var(--ok)}
nav{display:flex;gap:4px;background:var(--cd);border:1px solid var(--ln);border-radius:12px;padding:4px;position:sticky;top:6px;z-index:5}
nav button{flex:1;border:0;background:none;border-radius:9px;padding:8px;cursor:pointer;color:var(--mu);font-weight:500}
nav button.a{background:#1f2738;color:var(--tx)}nav button.m.a{background:#3a2a10;color:var(--wa)}
section{display:none;margin-top:12px}section.a{display:block}
.cd{background:var(--cd);border:1px solid var(--ln);border-radius:14px;padding:14px;margin-bottom:12px}
.ti{font-size:13px;font-weight:700;color:var(--mu);margin-bottom:10px}
.vs{display:grid;grid-template-columns:repeat(5,1fr);gap:8px}
.vs div{text-align:center}.vs small,.lb{color:var(--mu);font-size:12px}.vs b{display:block;font-size:20px;font-weight:600}
.fl{display:flex;flex-wrap:wrap;gap:6px;margin-top:12px;padding-top:12px;border-top:1px solid var(--ln)}
.tg{font-size:12px;padding:2px 9px;border-radius:7px;background:#1b2231;color:var(--mu)}
.tg.g{background:#10301f;color:var(--ok)}.tg.r{background:#3a1820;color:var(--er)}.tg.y{background:#382b12;color:var(--wa)}
.ch{display:grid;grid-template-columns:1fr 1fr;gap:12px}
.hd{display:flex;justify-content:space-between;align-items:center;margin-bottom:8px}.hd b{font-size:15px}
.big{display:flex;justify-content:space-between;align-items:baseline;margin:6px 0}.big b{font-size:26px;font-weight:700}
.bar{height:6px;background:#0c1018;border-radius:6px;overflow:hidden;position:relative;margin:4px 0 12px}
.bar i{position:absolute;inset:0 0 0 auto;width:0;background:var(--ac);transition:width .3s}
.bar u{position:absolute;top:0;bottom:0;width:2px;background:var(--wa)}
table{width:100%;border-collapse:collapse;font-size:13px}td{padding:5px 2px;border-top:1px solid var(--ln)}td:last-child{text-align:left}
.bt{width:100%;border:0;border-radius:10px;padding:10px;margin-top:12px;font-weight:700;cursor:pointer;color:#fff}
.cut{background:var(--er)}.run{background:var(--ok)}
.rw{display:grid;grid-template-columns:1fr auto;gap:2px 12px;align-items:center;padding:9px 0;border-top:1px solid var(--ln)}.rw:first-of-type{border-top:0}
.rw .h{grid-column:1/-1;color:var(--mu);font-size:12px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;cursor:pointer}.rw .h.o{white-space:normal}
.ap{font-size:12px;color:var(--ac);margin-right:6px}
.ct{display:flex;align-items:center;gap:6px}
input[type=number]{width:84px;background:#0c1018;border:1px solid var(--ln);border-radius:8px;padding:5px 7px;direction:ltr}
.sb{border:0;border-radius:8px;padding:5px 12px;background:var(--ac);color:#fff;cursor:pointer}
.sw{border:0;border-radius:8px;padding:5px 0;width:64px;background:#252e42;color:var(--mu);cursor:pointer}.sw.on{background:var(--ok);color:#06140d}.sw.w.on{background:var(--wa)}
.sg{display:flex;background:#0c1018;border-radius:8px;padding:2px}.sg button{border:0;background:none;width:34px;padding:3px;border-radius:6px;cursor:pointer;color:var(--mu)}.sg button.on{background:var(--ac);color:#fff}
.wn{background:#3a1820;color:#ffb3bb;border-radius:12px;padding:10px 12px;margin-top:12px;font-size:13px}
.wn b{color:var(--er)}.gb{display:none}.gb.v{display:block}
.mx{display:flex;justify-content:space-between;align-items:center;gap:12px}
input[type=range]{width:100%;accent-color:var(--wa);margin:10px 0 2px;direction:ltr}
.ms{display:grid;grid-template-columns:repeat(5,1fr);gap:6px;margin-top:10px;text-align:center}.ms div{background:#0c1018;border-radius:8px;padding:6px 2px}.ms b{display:block}
.off{background:#5b1c26;font-size:16px}
.fx{direction:ltr;text-align:right;unicode-bidi:isolate;font-size:11px;color:#6f7a93;font-variant-numeric:tabular-nums;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.fb{background:#0c1018;border-radius:10px;padding:8px 10px;margin-bottom:6px}.fb .fx{font-size:12px;color:#9aa6c0;line-height:1.9;white-space:normal}.fb .lb{font-size:11px}
body.dn section:not(#t2){opacity:.45;filter:grayscale(1)}
@media(max-width:640px){.vs{grid-template-columns:repeat(3,1fr)}.ch{grid-template-columns:1fr}.ms{grid-template-columns:repeat(3,1fr)}}
</style></head><body>
<header><h1>پنل ChangeOver</h1><div class="lk" id="lk"><span id="lt">در حال اتصال…</span><i></i></div></header>
<nav><button class="a" data-t="0">وضعیت</button><button data-t="1">تنظیمات</button><button class="m" data-t="2">تست دستی</button></nav>
<div class="wn gb" id="mb"><b>مود تست دستی فعال است</b> — شارژر خودکار و محافظت‌های باتری متوقف‌اند. <span id="ka"></span></div>

<section class="a" id="t0">
<div class="cd"><div class="vs" id="vs"></div><div class="fl" id="fl"></div></div>
<div class="ch" id="ch"></div>
</section>

<section id="t1"></section>

<section id="t2">
<div class="wn">در این مود ماشین حالت شارژر، رمپ، پنجرهٔ اعتبار باتری، BAT_LOST و توقف ۱۴٫۴V همه خاموش‌اند. فقط حضور ورودی ۲۴V، قطع سخت‌افزاری JIT، قطع ۱۵٫۰V هر کانال، سقف duty و قطع کانال فعال می‌مانند.</div>
<div class="cd" style="margin-top:12px"><div class="mx"><div><b>مود تست دستی</b> <span class="ap n" id="a19"></span><div class="lb" id="mh">duty هر کانال مستقیم از شناسه‌های ۱۶ و ۱۸ اعمال می‌شود.</div></div><button class="sw w" id="s19">—</button></div></div>
<div class="ch" id="mc"></div>
<button class="bt off" id="ao">خاموش کردن هر دو کانال (duty = 0)</button>
</section>

<script>
const $=i=>document.getElementById(i),E=(h)=>{const d=document.createElement('div');d.innerHTML=h;return d.firstElementChild;};
const ST=['خاموش','Bulk','Absorb','Float','راه‌اندازی','انتظار JIT','انتظار ورودی','خطای نهایی','باتری قطع','دستی'];
const SC=['','g','g','g','y','r','y','r','r','y'];
/* شناسه: [عنوان, واحد, کمینه, بیشینه, نوع(n عدد، b کلید، m مدین), توضیح] */
const P={0:['آفست صفر کانال ۱','count',0,255,'n','شمارش ADC در جریان صفر؛ داخل فرمول mA از raw کم می‌شود'],
1:['آفست صفر کانال ۲','count',0,255,'n','مانند کانال ۱ برای زنجیرهٔ دوم'],
2:['گین کانال ۱','‰',100,3000,'n','مقیاس نهایی تبدیل به mA؛ مقدار بنچ ۱۰۸۵ (پیش‌فرض ≈ ×۰٫۹۵۲۳ به‌ازای هر count)'],
3:['گین کانال ۲','‰',100,3000,'n','مقیاس نهایی تبدیل به mA کانال ۲'],
4:['آفست ولتاژ ورودی','mV',-2000,2000,'n','بعد از تبدیل مقسم 69.2k/6.8k جمع می‌شود (علامت‌دار)'],
5:['آفست ولتاژ پک ۲۴V','mV',-2000,2000,'n','کالیبراسیون ولتاژ پک ۲۴V (علامت‌دار)'],
6:['آفست ولتاژ ۱۲V','mV',-2000,2000,'n','مقسم 34.2k/6.8k؛ روی باتری پایین و بالا (V24 − V12) هر دو اثر دارد'],
7:['پنجرهٔ مدین','',1,5,'m','مرحلهٔ اول فیلتر؛ ۱ = خاموش، ۳ = پیش‌فرض، ۵ پالس‌های دوتایی را هم حذف می‌کند'],
8:['پنجرهٔ میانگین','نمونه',1,10,'n','مرحلهٔ دوم فیلتر: میانگین آخرین W خروجی مدین؛ ۱ = خاموش، ۱۰ = پیش‌فرض'],
9:['بازدهی کانال ۱','‰',100,999,'n','فقط داخل فرمول تخمین (کانال ۱: Vbat = باتری بالا)؛ روی شارژ واقعی اثر ندارد'],
10:['بازدهی کانال ۲','‰',100,999,'n','کانال ۲: Vbat = باتری پایین؛ ۲۴۲ خطای over-read سنس کانال ۲ را جبران می‌کند، به ~۷۰۰ تغییرش ندهید'],
13:['سقف duty کانال ۱','‰',0,500,'n','رمپ، تنظیم و مود فیکس همه به این سقف محدودند'],
14:['سقف duty کانال ۲','‰',0,500,'n','سقف PWM شارژر ۲'],
15:['نگه‌داشت duty فیکس ۱','',0,1,'b','PWM روی مقدار شناسهٔ ۱۶ می‌ماند؛ توقف در ولتاژ ابزورب همچنان فعال است'],
16:['مقدار duty فیکس ۱','‰',0,500,'n','در مود تست دستی همین مقدار duty کانال ۱ است'],
17:['نگه‌داشت duty فیکس ۲','',0,1,'b','PWM روی مقدار شناسهٔ ۱۸ می‌ماند'],
18:['مقدار duty فیکس ۲','‰',0,500,'n','در مود تست دستی همین مقدار duty کانال ۲ است']};
const G=[['کالیبراسیون جریان',[0,1,2,3]],['کالیبراسیون ولتاژ',[4,5,6]],['فیلتر جریان',[7,8]],['تخمین جریان باتری',[9,10]],['محدودیت duty',[13,14]],['نگه‌داشت duty فیکس (با محافظت خودکار)',[15,16,17,18]]];
const V=[['ورودی',14],['پک ۲۴V',15],['نود ۱۲V',16],['باتری بالا',18],['باتری پایین',17]];
let D=null,tab=0;
const v2=mv=>(mv/1000).toFixed(2),pc=pm=>(pm/10).toFixed(1)+'%';
function send(id,v){fetch('/s?id='+id+'&v='+v,{method:'POST'});const a=$('a'+id);if(a)a.textContent='…';}
function num(id){const e=$('i'+id),p=P[id],v=Math.round(+e.value);if(e.value===''||isNaN(v))return;send(id,Math.min(p[3],Math.max(p[2],v)));e.value='';e.blur();}
function flip(id){const c=D&&D.p[id],nv=c===1?0:1;if(nv&&(id==15||id==17)&&!confirm('حلقهٔ تنظیم این کانال خاموش و PWM روی مقدار فیکس نگه داشته می‌شود. ادامه؟'))return;send(id,nv);}

/* ---------- ساخت صفحه ---------- */
$('vs').innerHTML=V.map((v,i)=>`<div><small>${v[0]}</small><b class="n" id="v${i}">—</b></div>`).join('');
$('ch').innerHTML=[1,2].map(n=>`<div class="cd"><div class="hd"><b>شارژر ${n} <span class="lb">· باتری ${n==1?'بالا':'پایین'}</span></b><span class="tg" id="st${n}">—</span></div>
<div class="big"><span class="lb">جریان تخمینی باتری</span><b class="n" id="ie${n}">—</b></div>
<div class="big"><span class="lb">duty</span><span class="n" id="du${n}">—</span></div><div class="bar"><i id="db${n}"></i><u id="cl${n}"></u></div>
<table>${[['ADC خام','count',0],['ولتاژ شنت','µV',1],['جریان بدون فیلتر','mA',2],['جریان فیلترشده','mA',3],['تخمین باتری','mA',4]].map(r=>`<tr><td>${r[0]}<div class="fx" id="f${n}${r[2]}"></div></td><td class="n"><b id="c${n}${r[2]}">—</b> <span class="lb">${r[1]}</span></td></tr>`).join('')}</table>
<button class="bt" id="tg${n}">—</button></div>`).join('');
[1,2].forEach(n=>$('tg'+n).onclick=()=>{const c=D&&D.p[10+n];if(c!==0&&!confirm('PWM شارژر '+n+' فوراً قطع شود؟'))return;send(10+n,c===0?1:0);});
function ctl(id){const p=P[id];
 if(p[4]=='b')return `<button class="sw${id>14?' w':''}" id="b${id}" onclick="flip(${id})">—</button>`;
 if(p[4]=='m')return `<div class="sg" id="g${id}">${[1,3,5].map(v=>`<button data-v="${v}" onclick="send(${id},${v})">${v}</button>`).join('')}</div>`;
 return `<input type="number" id="i${id}" min="${p[2]}" max="${p[3]}" placeholder="${p[2]}…${p[3]}" onkeydown="if(event.key=='Enter')num(${id})"><button class="sb" onclick="num(${id})">ثبت</button>`;}
$('t1').innerHTML=G.map((g,gi)=>`<div class="cd"><div class="ti">${g[0]}</div>${gi<4?`<div class="fb" id="fb${gi}"></div>`:''}${g[1].map(id=>`<div class="rw"><div>${P[id][0]} <span class="lb">${P[id][1]}</span><span class="ap n" id="a${id}">—</span></div><div class="ct">${ctl(id)}</div><div class="h" onclick="this.classList.toggle('o')">${P[id][5]}</div></div>`).join('')}</div>`).join('');
$('mc').innerHTML=[1,2].map(n=>{const id=14+2*n;return `<div class="cd"><div class="hd"><b>کانال ${n} <span class="lb">· باتری ${n==1?'بالا':'پایین'}</span></b><span class="tg" id="ms${n}">—</span></div>
<div class="mx"><span class="lb">duty فرمان <span class="ap n" id="a${id}m">—</span></span><div class="ct"><input type="number" id="m${n}" min="0" max="500"><button class="sb" id="mk${n}">اعمال</button></div></div>
<input type="range" id="r${n}" min="0" max="500" step="1" value="0">
<div class="ms"><div><span class="lb">duty</span><b class="n" id="md${n}">—</b></div><div><span class="lb">جریان اولیه</span><b class="n" id="mi${n}">—</b></div><div><span class="lb">تخمین باتری</span><b class="n" id="me${n}">—</b></div><div><span class="lb">ولتاژ کانال</span><b class="n" id="mv${n}">—</b></div><div><span class="lb">سقف</span><b class="n" id="mc${n}">—</b></div></div></div>`;}).join('');
[1,2].forEach(n=>{const id=14+2*n,r=$('r'+n),m=$('m'+n);
 r.oninput=()=>m.value=r.value;r.onchange=()=>send(id,r.value);
 $('mk'+n).onclick=()=>{const v=Math.round(+m.value);if(m.value===''||isNaN(v))return;send(id,Math.max(0,Math.min(+r.max,v)));m.blur();};
 m.onkeydown=e=>{if(e.key=='Enter')$('mk'+n).click();};});
$('s19').onclick=()=>{if(!D)return;const on=(D.fl&32)||D.p[19]===1;
 if(!on&&!confirm('شارژر خودکار و همهٔ محافظت‌های باتری متوقف می‌شوند و duty را خودتان تعیین می‌کنید. ادامه؟'))return;send(19,on?0:1);};
$('ao').onclick=()=>{send(16,0);send(18,0);};
document.querySelectorAll('nav button').forEach(b=>b.onclick=()=>{tab=+b.dataset.t;document.querySelectorAll('nav button,section').forEach(x=>x.classList.remove('a'));b.classList.add('a');$('t'+tab).classList.add('a');});

/* ---------- به‌روزرسانی ---------- */
/* فرمول‌های بخش 5.3 سند با مقادیر زنده */
const K_UV=3300/4095*11/10*1000/101,K_MA=K_UV/10,K24=3300/4095*76000/6800,K12=3300/4095*41000/6800;
const f1=x=>x.toFixed(1),V_=mv=>(mv/1000).toFixed(2)+'V',nz=v=>v==null?'?':v;
function formulas(t,p){
 [1,2].forEach(n=>{const b=n==1?0:7,raw=t[b],off=p[n-1],g=p[n+1],eta=p[8+n],vb=n==1?t[18]:t[17],vin=t[14],fl=t[b+3];
  $('f'+n+'0').textContent='12-bit ADC · Vref 3300 mV';
  $('f'+n+'1').textContent=`${raw} × 8.7756 ≈ ${Math.round(raw*K_UV)}`;
  $('f'+n+'2').textContent=off==null||g==null?'':`(${raw} − ${off}) × 0.8776 × ${g}/1000 ≈ ${f1(Math.max(raw-off,0)*K_MA*g/1000)}`;
  $('f'+n+'3').textContent=`average[${nz(p[8])}]( median[${nz(p[7])}]( mA ) )`;
  $('f'+n+'4').textContent=eta==null?'':(fl==0||vin==0)?'I = 0 → 0':`${fl} × ${V_(vin)} × ${eta}‰ / ${V_(Math.max(vb,1000))} ≈ ${Math.round(fl*vin*eta/Math.max(vb,1000)/1000)}`;});
 const L=(a,b)=>`<div class="fx">${a}</div>`+(b?`<div class="lb">${b}</div>`:'');
 $('fb0').innerHTML=[1,2].map(n=>{const raw=t[n==1?0:7],off=p[n-1],g=p[n+1];return L(`Ch${n}: mA = (${raw} − ${nz(off)}) × 0.8776 × ${nz(g)}/1000 ≈ ${off==null||g==null?'?':f1(Math.max(raw-off,0)*K_MA*g/1000)}`);}).join('')+
  L('shunt µV = raw × 3300/4095 × 11/10 × 1000/101 = raw × 8.7756','ثابت‌ها: ADC دوازده‌بیتی، ۳۳۰۰mV، R41/R42 = 1k/10k، LM358 × 101، شنت 10 mOhm');
 const vc=(nm,mv,k,off)=>{const o=off==null?0:off,c=Math.round((mv-o)/k);return L(`${nm} = ${c} × ${k.toFixed(3)} ${o<0?'−':'+'} ${Math.abs(o)} ≈ ${V_(mv)}`);};
 $('fb1').innerHTML=vc('Vin',t[14],K24,p[4])+vc('V24',t[15],K24,p[5])+vc('V12',t[16],K12,p[6])+L(`Vhigh = V24 − V12 = ${V_(t[18])}   ·   Vlow = V12 = ${V_(t[17])}`);
 $('fb2').innerHTML=L(`I_filtered = average[W=${nz(p[8])}]( median[N=${nz(p[7])}]( mA_unfiltered ) )`)+[1,2].map(n=>{const b=n==1?0:7;return L(`Ch${n}: ${t[b+2]} mA → ${t[b+3]} mA`);}).join('');
 $('fb3').innerHTML=L('Iest = I_filtered × Vin × eff / max(Vbat, 1 V)')+[1,2].map(n=>{const b=n==1?0:7,vb=n==1?t[18]:t[17],eta=p[8+n];return L(`Ch${n}: ${t[b+3]} × ${V_(t[14])} × ${nz(eta)}‰ / ${V_(Math.max(vb,1000))} ≈ ${eta==null?'?':(t[b+3]==0||t[14]==0?0:Math.round(t[b+3]*t[14]*eta/Math.max(vb,1000)/1000))} mA`);}).join('');}
function draw(d){D=d;const t=d.t,p=d.p,on=d.on==1,man=(d.fl&32)!=0;
 document.body.classList.toggle('dn',!on);$('lk').classList.toggle('on',on);
 $('lt').innerHTML=on?`آنلاین · <span class="n">seq ${d.seq}</span>`:(d.n?'لینک قطع است':'در انتظار STM32…');
 V.forEach((v,i)=>$('v'+i).textContent=v2(t[v[1]]));
 const F=[['snapshot',d.fl&1],['ورودی ۲۴V',d.fl&2],['اندازه‌گیری معتبر',d.fl&4]];
 $('fl').innerHTML=F.map(f=>`<span class="tg ${f[1]?'g':'r'}">${f[0]}</span>`).join('')+(t[19]&64?'<span class="tg r">خطا: باتری قطع</span>':'')+
  (t[19]&~64?`<span class="tg r n">fault 0x${t[19].toString(16)}</span>`:'')+(man?'<span class="tg y">مود دستی</span>':'');
 [1,2].forEach(n=>{const b=n==1?0:7,s=t[b+6],en=p[10+n],ce=p[12+n],fx=p[13+2*n],cv=t[n==1?18:17];
  const st=$('st'+n);st.textContent=(ST[s]||'#'+s)+(fx===1&&!man?' · فیکس':'');st.className='tg '+(SC[s]||'');
  $('ie'+n).innerHTML=t[b+4]+' <span class="lb">mA</span>';$('du'+n).textContent=pc(t[b+5]);
  $('db'+n).style.width=Math.min(100,t[b+5]/10)+'%';$('cl'+n).style.left=(100-Math.min(100,(ce==null?1000:ce)/10))+'%';
  [0,1,2,3,4].forEach(k=>$('c'+n+k).textContent=t[b+k]);
  const g=$('tg'+n);g.textContent=en===0?'وصل مجدد شارژر '+n:'قطع شارژر '+n;g.className='bt '+(en===0?'run':'cut');
  /* تب تست دستی */
  const lim=Math.min(500,ce==null?500:ce),r=$('r'+n),m=$('m'+n),dv=p[14+2*n];r.max=lim;m.max=lim;
  if(dv!=null&&document.activeElement!==r&&document.activeElement!==m&&!(d.q&(1<<(14+2*n)))){r.value=dv;}
  let tag=ST[s]||'#'+s,cl=SC[s]||'';if(cv>=15000&&man){tag='قطع ۱۵V';cl='r';}if(en===0){tag='کانال قطع';cl='r';}
  $('ms'+n).textContent=tag;$('ms'+n).className='tg '+cl;
  $('md'+n).textContent=pc(t[b+5]);$('mi'+n).textContent=t[b+3]+' mA';$('me'+n).textContent=t[b+4]+' mA';
  $('mv'+n).textContent=v2(cv)+' V';$('mc'+n).textContent=pc(lim);
  const a=$('a'+(14+2*n)+'m');if(!(d.q&(1<<(14+2*n))))a.textContent=dv==null?'—':dv+'‰';});
 Object.keys(P).forEach(id=>{if(d.q&(1<<id))return;const v=p[id],a=$('a'+id);a.textContent=v==null?'—':v;
  const b=$('b'+id);if(b){b.textContent=v===1?'روشن':v===0?'خاموش':'—';b.classList.toggle('on',v===1);}
  const g=$('g'+id);if(g)g.querySelectorAll('button').forEach(x=>x.classList.toggle('on',+x.dataset.v===v));});
 /* شناسهٔ ۱۹ فقط اگر فرمور STM32 گزارشش کرده باشد */
 const sup=p[19]!=null,s19=$('s19');s19.disabled=!sup||!on;
 s19.textContent=!sup?'پشتیبانی نمی‌شود':man?'روشن':'خاموش';s19.classList.toggle('on',man);
 $('a19').textContent=d.q&(1<<19)?'…':'';
 $('mh').textContent=sup?'duty هر کانال مستقیم از شناسه‌های ۱۶ و ۱۸ اعمال می‌شود.':'فرمور فعلی STM32 شناسهٔ ۱۹ را گزارش نکرده است (پروتکل v1.2 لازم است).';
 formulas(t,p);
 $('mb').classList.toggle('v',man);$('ka').innerHTML=man?(d.ka<1500?`پایش لینک فعال · <span class="n">keepalive ${d.ka} ms</span>`:'<b>keepalive متوقف است</b>'):'';}
async function poll(){try{const r=await fetch('/t',{cache:'no-store'});draw(await r.json());}catch(e){document.body.classList.add('dn');$('lk').classList.remove('on');$('lt').textContent='ESP در دسترس نیست';}
 setTimeout(poll,300);}
poll();
</script></body></html>)HTML";

/* ==================== Vazirmatn Font (PROGMEM) ==================== */
/* [EN] Vazirmatn v33.0.3 (SIL OFL 1.1, github.com/rastikerdar/vazirmatn), variable wght 400..700,
        subset to the glyphs used by the panel; embedded because the AP has no internet.
   [FA] فونت وزیرمتن نسخه 33.0.3 (مجوز SIL OFL 1.1)، وزن متغیر ۴۰۰ تا ۷۰۰، فقط حروف مورد استفادهٔ پنل؛
        داخل برنامه جاسازی شده چون اکسس‌پوینت اینترنت ندارد. */
static const char ESP_PANEL_FONT_CSS[] PROGMEM =
"@font-face{font-family:Vazirmatn;font-weight:400 700;font-display:swap;src:url(data:font/woff2;base64,"
"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"
"Rw3Z0xtwJQA="
") format('woff2')}";

/* ==================== Byte Helpers ==================== */

/**
 * @brief  [EN] Read a little-endian u32 from a byte buffer.
 *         [FA] خواندن عدد u32 اندیان‌کوچک از بافر بایتی.
 * @param  uint8_t__ptr_buffer [EN] Source buffer, at least offset+4 bytes / [FA] بافر مبدا، حداقل offset+4 بایت
 * @param  uint8_t__offset     [EN] Byte offset, 0..92 / [FA] آفست بایتی، ۰ تا ۹۲
 * @return [EN] Decoded value / [FA] مقدار رمزگشایی‌شده
 */
static uint32_t func__Esp_ReadU32(const uint8_t *uint8_t__ptr_buffer, uint8_t uint8_t__offset)
{
    uint32_t uint32_t__byte0 = (uint32_t)uint8_t__ptr_buffer[uint8_t__offset];
    uint32_t uint32_t__byte1 = (uint32_t)uint8_t__ptr_buffer[uint8_t__offset + 1u] << 8;
    uint32_t uint32_t__byte2 = (uint32_t)uint8_t__ptr_buffer[uint8_t__offset + 2u] << 16;
    uint32_t uint32_t__byte3 = (uint32_t)uint8_t__ptr_buffer[uint8_t__offset + 3u] << 24;
    uint32_t uint32_t__lowHalf = uint32_t__byte0 | uint32_t__byte1;
    uint32_t uint32_t__highHalf = uint32_t__byte2 | uint32_t__byte3;
    return uint32_t__lowHalf | uint32_t__highHalf;
}

/* ==================== Frame Transmit ==================== */

/**
 * @brief  [EN] Build and write one frame: AA 55 type len payload xor.
 *         [FA] ساخت و ارسال یک فریم: AA 55 type len payload xor.
 * @param  uint8_t__type        [EN] Message type (0x01 / 0x02) / [FA] نوع پیام
 * @param  uint8_t__ptr_payload [EN] Payload bytes, may be NULL when len = 0 / [FA] بایت‌های payload؛ برای طول صفر می‌تواند NULL باشد
 * @param  uint8_t__len         [EN] Payload length, 0..96 bytes / [FA] طول payload، ۰ تا ۹۶ بایت
 * @return [EN] None / [FA] ندارد
 */
static void func__Esp_WriteFrame(uint8_t uint8_t__type, const uint8_t *uint8_t__ptr_payload, uint8_t uint8_t__len)
{
    uint8_t UINT8_T__A__Frame[ESP_LINK_HEADER_SIZE + ESP_LINK_MAX_PAYLOAD + 1u];
    uint8_t uint8_t__xor = (uint8_t)(uint8_t__type ^ uint8_t__len);
    uint8_t uint8_t__index;

    if (uint8_t__len > ESP_LINK_MAX_PAYLOAD)
    {
        return;
    }

    UINT8_T__A__Frame[0] = ESP_LINK_SOF_BYTE0;
    UINT8_T__A__Frame[1] = ESP_LINK_SOF_BYTE1;
    UINT8_T__A__Frame[2] = uint8_t__type;
    UINT8_T__A__Frame[3] = uint8_t__len;

    for (uint8_t__index = 0u; uint8_t__index < uint8_t__len; uint8_t__index++)
    {
        uint8_t uint8_t__byte = uint8_t__ptr_payload[uint8_t__index];
        UINT8_T__A__Frame[ESP_LINK_HEADER_SIZE + uint8_t__index] = uint8_t__byte;
        uint8_t__xor = (uint8_t)(uint8_t__xor ^ uint8_t__byte);
    }

    uint8_t uint8_t__xorPosition = (uint8_t)(ESP_LINK_HEADER_SIZE + uint8_t__len);
    UINT8_T__A__Frame[uint8_t__xorPosition] = uint8_t__xor;
    uint8_t uint8_t__frameSize = (uint8_t)(uint8_t__xorPosition + 1u);

    (void)Serial.write(UINT8_T__A__Frame, uint8_t__frameSize);
    UINT32_T__G__LastTxMs = (uint32_t)millis();
}

/**
 * @brief  [EN] Send SET_PARAM [id:u8][value:u32 LE].
 *         [FA] ارسال SET_PARAM با قالب [id:u8][value:u32 LE].
 * @param  uint8_t__id     [EN] Parameter ID, 0..13 / [FA] شناسه پارامتر، ۰ تا ۱۳
 * @param  uint32_t__value [EN] Raw wire value (signed IDs as two's complement) / [FA] مقدار خام (شناسه‌های علامت‌دار به صورت مکمل دو)
 * @return [EN] None / [FA] ندارد
 */
static void func__Esp_SendSetParam(uint8_t uint8_t__id, uint32_t uint32_t__value)
{
    uint8_t UINT8_T__A__Payload[ESP_LINK_PARAM_ITEM_SIZE];
    UINT8_T__A__Payload[0] = uint8_t__id;
    UINT8_T__A__Payload[1] = (uint8_t)(uint32_t__value & 0xFFu);
    UINT8_T__A__Payload[2] = (uint8_t)((uint32_t__value >> 8) & 0xFFu);
    UINT8_T__A__Payload[3] = (uint8_t)((uint32_t__value >> 16) & 0xFFu);
    UINT8_T__A__Payload[4] = (uint8_t)((uint32_t__value >> 24) & 0xFFu);
    func__Esp_WriteFrame(ESP_MSG_SET_PARAM, UINT8_T__A__Payload, ESP_LINK_PARAM_ITEM_SIZE);
}

/**
 * @brief  [EN] Send at most one queued command per ESP_LINK_TX_INTERVAL_MS
 *              (priority order UINT8_T__G__TxOrder), plus the manual-mode keepalive. Never blocks.
 *         [FA] ارسال حداکثر یک فرمان صف‌شده در هر ESP_LINK_TX_INTERVAL_MS
 *              (به ترتیب اولویت UINT8_T__G__TxOrder) و keepalive مود دستی. هیچ‌وقت مسدود نمی‌کند.
 * @return [EN] None / [FA] ندارد
 */
static void func__Esp_PumpTx(void)
{
    uint32_t uint32_t__nowMs = (uint32_t)millis();
    uint32_t uint32_t__elapsedMs = uint32_t__nowMs - UINT32_T__G__LastTxMs;
    uint8_t uint8_t__step;

    if (uint32_t__elapsedMs < ESP_LINK_TX_INTERVAL_MS)
    {
        return;
    }

    for (uint8_t__step = 0u; uint8_t__step < ESP_PARAM_COUNT; uint8_t__step++)
    {
        uint8_t uint8_t__id = UINT8_T__G__TxOrder[uint8_t__step];
        if (BOOL__G__TxParamPending[uint8_t__id])
        {
            BOOL__G__TxParamPending[uint8_t__id] = false;
            func__Esp_SendSetParam(uint8_t__id, UINT32_T__G__TxParamValue[uint8_t__id]);
            return;
        }
    }

    /* [EN] Manual-mode keepalive: only while a browser is polling (dead-man stays meaningful).
       [FA] keepalive مود دستی: فقط وقتی مرورگر در حال خواندن است (dead-man معنادار می‌ماند). */
    bool bool__flagManual = ((UINT8_T__G__TlmFlags & ESP_TLM_FLAG_MANUAL_MODE) != 0u);
    bool bool__paramManual = BOOL__G__ParamKnown[ESP_PARAM_MANUAL_TEST_MODE] &&
                             (UINT32_T__G__ParamApplied[ESP_PARAM_MANUAL_TEST_MODE] != 0u);
    bool bool__manualActive = bool__flagManual || bool__paramManual;
    uint32_t uint32_t__browserAgeMs = uint32_t__nowMs - UINT32_T__G__LastBrowserPollMs;
    bool bool__browserFresh = BOOL__G__BrowserSeen && (uint32_t__browserAgeMs <= ESP_LINK_BROWSER_FRESH_MS);
    uint32_t uint32_t__keepaliveAgeMs = uint32_t__nowMs - UINT32_T__G__LastKeepaliveMs;
    if (bool__manualActive && bool__browserFresh && (uint32_t__keepaliveAgeMs >= ESP_LINK_KEEPALIVE_MS))
    {
        BOOL__G__TxGetPending = true;
    }

    if (BOOL__G__TxGetPending)
    {
        BOOL__G__TxGetPending = false;
        func__Esp_WriteFrame(ESP_MSG_GET_PARAMS, NULL, 0u);
        UINT32_T__G__LastKeepaliveMs = uint32_t__nowMs;
    }
}

/* ==================== Frame Receive ==================== */

/**
 * @brief  [EN] Store one parameter value reported by the STM32 (applied value).
 *         [FA] ذخیره مقدار اعمال‌شده یک پارامتر که STM32 گزارش داده است.
 * @param  uint8_t__ptr_item [EN] 5-byte item [id][value LE] / [FA] آیتم ۵ بایتی [id][value LE]
 * @return [EN] None / [FA] ندارد
 */
static void func__Esp_StoreParamItem(const uint8_t *uint8_t__ptr_item)
{
    uint8_t uint8_t__id = uint8_t__ptr_item[0];
    if (uint8_t__id < ESP_PARAM_COUNT)
    {
        UINT32_T__G__ParamApplied[uint8_t__id] = func__Esp_ReadU32(uint8_t__ptr_item, 1u);
        BOOL__G__ParamKnown[uint8_t__id] = true;
    }
}

/**
 * @brief  [EN] Dispatch one checksum-valid frame from the STM32.
 *         [FA] پردازش یک فریم معتبر (checksum درست) دریافتی از STM32.
 * @return [EN] None / [FA] ندارد
 */
static void func__Esp_HandleFrame(void)
{
    const uint8_t *uint8_t__ptr_payload = UINT8_T__G__RxPayload;
    uint8_t uint8_t__index;

    if ((UINT8_T__G__RxType == ESP_MSG_TLM_LIVE) && (UINT8_T__G__RxLen == ESP_LINK_TLM_SIZE))
    {
        uint16_t uint16_t__seqLow = (uint16_t)uint8_t__ptr_payload[0];
        uint16_t uint16_t__seqHigh = (uint16_t)((uint16_t)uint8_t__ptr_payload[1] << 8);
        uint16_t uint16_t__seq = (uint16_t)(uint16_t__seqLow | uint16_t__seqHigh);

        /* [EN] First frame or STM32 restart (seq jumps back): refresh the parameter table.
           [FA] اولین فریم یا ری‌استارت STM32 (عقب‌گرد seq): جدول پارامترها دوباره خوانده شود. */
        bool bool__seqRestart = BOOL__G__TlmSeen && (uint16_t__seq < UINT16_T__G__TlmSeq) && (UINT16_T__G__TlmSeq < 0xFF00u);
        if ((!BOOL__G__TlmSeen) || bool__seqRestart)
        {
            BOOL__G__TxGetPending = true;
        }

        /* [EN] STM32 params are RAM-only: re-send values the user set in this session.
                Manual test mode (ID 19) is never re-enabled automatically.
           [FA] پارامترهای STM32 فقط در RAM هستند: مقادیری که کاربر در این نشست داده دوباره ارسال شوند.
                مود تست دستی (شناسه ۱۹) هیچ‌وقت خودکار روشن نمی‌شود. */
        if (bool__seqRestart)
        {
            for (uint8_t__index = 0u; uint8_t__index < ESP_PARAM_COUNT; uint8_t__index++)
            {
                bool bool__isManualSwitch = (uint8_t__index == ESP_PARAM_MANUAL_TEST_MODE);
                if (BOOL__G__ParamUserSet[uint8_t__index] && (!bool__isManualSwitch))
                {
                    BOOL__G__TxParamPending[uint8_t__index] = true;
                }
            }
        }

        for (uint8_t__index = 0u; uint8_t__index < ESP_LINK_TLM_FIELD_COUNT; uint8_t__index++)
        {
            uint8_t uint8_t__fieldBytes = (uint8_t)(uint8_t__index * 4u);
            uint8_t uint8_t__offset = (uint8_t)(ESP_LINK_TLM_FIELD_OFFSET + uint8_t__fieldBytes);
            UINT32_T__G__TlmField[uint8_t__index] = func__Esp_ReadU32(uint8_t__ptr_payload, uint8_t__offset);
        }

        UINT16_T__G__TlmSeq = uint16_t__seq;
        UINT8_T__G__TlmFlags = uint8_t__ptr_payload[2];
        UINT32_T__G__LastTlmMs = (uint32_t)millis();
        UINT32_T__G__TlmFrameCount++;
        BOOL__G__TlmSeen = true;
    }
    else if ((UINT8_T__G__RxType == ESP_MSG_PARAM_REPORT) && (UINT8_T__G__RxLen == ESP_LINK_PARAM_ITEM_SIZE))
    {
        func__Esp_StoreParamItem(uint8_t__ptr_payload);
    }
    else if ((UINT8_T__G__RxType == ESP_MSG_PARAMS_BULK) && (UINT8_T__G__RxLen >= 1u))
    {
        uint8_t uint8_t__count = uint8_t__ptr_payload[0];
        for (uint8_t__index = 0u; uint8_t__index < uint8_t__count; uint8_t__index++)
        {
            uint8_t uint8_t__itemBytes = (uint8_t)(uint8_t__index * ESP_LINK_PARAM_ITEM_SIZE);
            uint8_t uint8_t__offset = (uint8_t)(1u + uint8_t__itemBytes);
            uint16_t uint16_t__itemEnd = (uint16_t)((uint16_t)uint8_t__offset + ESP_LINK_PARAM_ITEM_SIZE);
            if (uint16_t__itemEnd > UINT8_T__G__RxLen)
            {
                break;
            }
            func__Esp_StoreParamItem(&uint8_t__ptr_payload[uint8_t__offset]);
        }
    }
    else
    {
        /* [EN] Unknown type or wrong length: drop / [FA] نوع ناشناخته یا طول نادرست: دور ریخته می‌شود */
    }
}

/**
 * @brief  [EN] Feed one received byte to the frame parser; resyncs on AA 55.
 *         [FA] دادن یک بایت دریافتی به پارسر فریم؛ با AA 55 همگام‌سازی مجدد می‌کند.
 * @param  uint8_t__byte [EN] Received byte, 0..255 / [FA] بایت دریافتی، ۰ تا ۲۵۵
 * @return [EN] None / [FA] ندارد
 */
static void func__Esp_ParseByte(uint8_t uint8_t__byte)
{
    switch (ESP_RX_STATE_T__G__RxState)
    {
        case ESP_RX_WAIT_SOF0:
            if (uint8_t__byte == ESP_LINK_SOF_BYTE0)
            {
                ESP_RX_STATE_T__G__RxState = ESP_RX_WAIT_SOF1;
            }
            break;

        case ESP_RX_WAIT_SOF1:
            if (uint8_t__byte == ESP_LINK_SOF_BYTE1)
            {
                ESP_RX_STATE_T__G__RxState = ESP_RX_WAIT_TYPE;
            }
            else if (uint8_t__byte != ESP_LINK_SOF_BYTE0)
            {
                ESP_RX_STATE_T__G__RxState = ESP_RX_WAIT_SOF0;
            }
            else
            {
                /* [EN] AA AA: stay waiting for 55 / [FA] AA AA: منتظر 55 بمان */
            }
            break;

        case ESP_RX_WAIT_TYPE:
            UINT8_T__G__RxType = uint8_t__byte;
            UINT8_T__G__RxXor = uint8_t__byte;
            ESP_RX_STATE_T__G__RxState = ESP_RX_WAIT_LEN;
            break;

        case ESP_RX_WAIT_LEN:
            if (uint8_t__byte > ESP_LINK_MAX_PAYLOAD)
            {
                ESP_RX_STATE_T__G__RxState = ESP_RX_WAIT_SOF0;
            }
            else
            {
                UINT8_T__G__RxLen = uint8_t__byte;
                UINT8_T__G__RxIndex = 0u;
                UINT8_T__G__RxXor = (uint8_t)(UINT8_T__G__RxXor ^ uint8_t__byte);
                ESP_RX_STATE_T__G__RxState = (uint8_t__byte == 0u) ? ESP_RX_WAIT_XOR : ESP_RX_WAIT_PAYLOAD;
            }
            break;

        case ESP_RX_WAIT_PAYLOAD:
            UINT8_T__G__RxPayload[UINT8_T__G__RxIndex] = uint8_t__byte;
            UINT8_T__G__RxIndex++;
            UINT8_T__G__RxXor = (uint8_t)(UINT8_T__G__RxXor ^ uint8_t__byte);
            if (UINT8_T__G__RxIndex >= UINT8_T__G__RxLen)
            {
                ESP_RX_STATE_T__G__RxState = ESP_RX_WAIT_XOR;
            }
            break;

        case ESP_RX_WAIT_XOR:
            if (uint8_t__byte == UINT8_T__G__RxXor)
            {
                func__Esp_HandleFrame();
            }
            ESP_RX_STATE_T__G__RxState = ESP_RX_WAIT_SOF0;
            break;

        default:
            ESP_RX_STATE_T__G__RxState = ESP_RX_WAIT_SOF0;
            break;
    }
}

/* ==================== HTTP Handlers ==================== */

/**
 * @brief  [EN] GET / : serve the web panel from flash.
 *         [FA] مسیر GET / : ارسال پنل وب از حافظه فلش.
 * @return [EN] None / [FA] ندارد
 */
static void func__Esp_HttpRoot(void)
{
    ESP_WEB_SERVER_T__G__Server.send_P(200, "text/html", ESP_PANEL_HTML);
}

/**
 * @brief  [EN] GET /f.css : Vazirmatn @font-face (cached one year by the browser).
 *         [FA] مسیر GET /f.css : فونت وزیرمتن (مرورگر یک سال کش می‌کند).
 * @return [EN] None / [FA] ندارد
 */
static void func__Esp_HttpFont(void)
{
    ESP_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", ESP_HTTP_FONT_CACHE);
    ESP_WEB_SERVER_T__G__Server.send_P(200, "text/css", ESP_PANEL_FONT_CSS);
}

/**
 * @brief  [EN] GET /t : compact JSON snapshot {on,age,seq,fl,n,q,ka,t[20],p[20]}.
 *              t = TLM u32 fields in spec order (offset 4..80); p = applied params or null.
 *         [FA] مسیر GET /t : خلاصه JSON فشرده {on,age,seq,fl,n,q,ka,t[20],p[20]}.
 *              t فیلدهای u32 تله‌متری به ترتیب سند (آفست ۴ تا ۸۰)؛ p مقدار اعمال‌شده یا null.
 * @return [EN] None / [FA] ندارد
 */
static void func__Esp_HttpTelemetry(void)
{
    uint32_t uint32_t__nowMs = (uint32_t)millis();
    uint32_t uint32_t__ageMs = uint32_t__nowMs - UINT32_T__G__LastTlmMs;
    bool bool__online = BOOL__G__TlmSeen && (uint32_t__ageMs <= ESP_LINK_TIMEOUT_MS);
    uint32_t uint32_t__pendingMask = 0u;
    uint32_t uint32_t__keepaliveAgeMs = uint32_t__nowMs - UINT32_T__G__LastKeepaliveMs;
    uint8_t uint8_t__index;
    size_t size_t__used;

    UINT32_T__G__LastBrowserPollMs = uint32_t__nowMs;
    BOOL__G__BrowserSeen = true;

    for (uint8_t__index = 0u; uint8_t__index < ESP_PARAM_COUNT; uint8_t__index++)
    {
        if (BOOL__G__TxParamPending[uint8_t__index])
        {
            uint32_t__pendingMask |= (1UL << uint8_t__index);
        }
    }

    size_t__used = (size_t)snprintf(CHAR__G__JsonBuffer, ESP_JSON_BUFFER_SIZE,
        "{\"on\":%u,\"age\":%lu,\"seq\":%u,\"fl\":%u,\"n\":%lu,\"q\":%lu,\"ka\":%lu,\"t\":[",
        bool__online ? 1u : 0u, (unsigned long)uint32_t__ageMs, (unsigned int)UINT16_T__G__TlmSeq,
        (unsigned int)UINT8_T__G__TlmFlags, (unsigned long)UINT32_T__G__TlmFrameCount,
        (unsigned long)uint32_t__pendingMask, (unsigned long)uint32_t__keepaliveAgeMs);

    for (uint8_t__index = 0u; uint8_t__index < ESP_LINK_TLM_FIELD_COUNT; uint8_t__index++)
    {
        const char *char__ptr_sep = (uint8_t__index == 0u) ? "" : ",";
        size_t__used += (size_t)snprintf(&CHAR__G__JsonBuffer[size_t__used], ESP_JSON_BUFFER_SIZE - size_t__used,
            "%s%lu", char__ptr_sep, (unsigned long)UINT32_T__G__TlmField[uint8_t__index]);
    }

    size_t__used += (size_t)snprintf(&CHAR__G__JsonBuffer[size_t__used], ESP_JSON_BUFFER_SIZE - size_t__used, "],\"p\":[");

    for (uint8_t__index = 0u; uint8_t__index < ESP_PARAM_COUNT; uint8_t__index++)
    {
        const char *char__ptr_sep = (uint8_t__index == 0u) ? "" : ",";
        if (BOOL__G__ParamKnown[uint8_t__index])
        {
            /* [EN] Signed IDs 4..6 are two's complement on the wire / [FA] شناسه‌های ۴ تا ۶ مکمل دو هستند */
            int32_t int32_t__value = (int32_t)UINT32_T__G__ParamApplied[uint8_t__index];
            size_t__used += (size_t)snprintf(&CHAR__G__JsonBuffer[size_t__used], ESP_JSON_BUFFER_SIZE - size_t__used,
                "%s%ld", char__ptr_sep, (long)int32_t__value);
        }
        else
        {
            size_t__used += (size_t)snprintf(&CHAR__G__JsonBuffer[size_t__used], ESP_JSON_BUFFER_SIZE - size_t__used,
                "%snull", char__ptr_sep);
        }
    }

    (void)snprintf(&CHAR__G__JsonBuffer[size_t__used], ESP_JSON_BUFFER_SIZE - size_t__used, "]}");
    ESP_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    ESP_WEB_SERVER_T__G__Server.send(200, "application/json", CHAR__G__JsonBuffer);
}

/**
 * @brief  [EN] POST /s?id=&v= : clamp and queue one SET_PARAM (latest value wins).
 *         [FA] مسیر POST /s?id=&v= : محدودسازی و صف کردن یک SET_PARAM (آخرین مقدار معتبر است).
 * @return [EN] None / [FA] ندارد
 */
static void func__Esp_HttpSetParam(void)
{
    if ((!ESP_WEB_SERVER_T__G__Server.hasArg("id")) || (!ESP_WEB_SERVER_T__G__Server.hasArg("v")))
    {
        ESP_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0}");
        return;
    }

    int32_t int32_t__id = (int32_t)ESP_WEB_SERVER_T__G__Server.arg("id").toInt();
    int32_t int32_t__value = (int32_t)ESP_WEB_SERVER_T__G__Server.arg("v").toInt();

    if ((int32_t__id < 0) || (int32_t__id >= (int32_t)ESP_PARAM_COUNT))
    {
        ESP_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0}");
        return;
    }

    uint8_t uint8_t__id = (uint8_t)int32_t__id;
    int32_t int32_t__min = INT32_T__G__ParamMin[uint8_t__id];
    int32_t int32_t__max = INT32_T__G__ParamMax[uint8_t__id];
    int32_t int32_t__clamped = (int32_t__value < int32_t__min) ? int32_t__min : int32_t__value;
    int32_t__clamped = (int32_t__clamped > int32_t__max) ? int32_t__max : int32_t__clamped;

    /* [EN] Median size is 1/3/5: round an even value DOWN like the STM32 does.
       [FA] اندازهٔ مدین ۱/۳/۵ است: مقدار زوج مثل STM32 به پایین گرد می‌شود. */
    if ((uint8_t__id == ESP_PARAM_MEDIAN_SIZE) && ((int32_t__clamped % 2) == 0))
    {
        int32_t__clamped = int32_t__clamped - 1;
    }

    UINT32_T__G__TxParamValue[uint8_t__id] = (uint32_t)int32_t__clamped;
    BOOL__G__TxParamPending[uint8_t__id] = true;
    BOOL__G__ParamUserSet[uint8_t__id] = true;
    ESP_WEB_SERVER_T__G__Server.send(200, "application/json", "{\"ok\":1}");
}

/* ==================== Arduino Entry Points ==================== */

/**
 * @brief  [EN] Start UART link, Wi-Fi AP and HTTP server. CH_PD is left untouched.
 *         [FA] راه‌اندازی لینک UART، اکسس‌پوینت وای‌فای و وب‌سرور. پایه CH_PD دست‌نخورده می‌ماند.
 * @return [EN] None / [FA] ندارد
 */
void setup(void)
{
    Serial.setRxBufferSize(ESP_LINK_RX_BUFFER_SIZE);
    Serial.begin(ESP_LINK_BAUD_RATE, SERIAL_8N1);

    WiFi.mode(WIFI_AP);
#if defined(ESP8266)
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
#else
    WiFi.setSleep(false);
#endif
    (void)WiFi.softAP(ESP_WIFI_AP_SSID, ESP_WIFI_AP_PASS);

    ESP_WEB_SERVER_T__G__Server.on("/", HTTP_GET, func__Esp_HttpRoot);
    ESP_WEB_SERVER_T__G__Server.on("/f.css", HTTP_GET, func__Esp_HttpFont);
    ESP_WEB_SERVER_T__G__Server.on("/t", HTTP_GET, func__Esp_HttpTelemetry);
    ESP_WEB_SERVER_T__G__Server.on("/s", HTTP_POST, func__Esp_HttpSetParam);
    ESP_WEB_SERVER_T__G__Server.begin();
}

/**
 * @brief  [EN] Non-blocking loop: drain UART, send queued command, serve HTTP.
 *         [FA] حلقه غیرمسدودکننده: خالی کردن UART، ارسال فرمان صف‌شده، پاسخ به HTTP.
 * @return [EN] None / [FA] ندارد
 */
void loop(void)
{
    while (Serial.available() > 0)
    {
        int32_t int32_t__byte = (int32_t)Serial.read();
        if (int32_t__byte >= 0)
        {
            func__Esp_ParseByte((uint8_t)int32_t__byte);
        }
    }

    func__Esp_PumpTx();
    ESP_WEB_SERVER_T__G__Server.handleClient();
}
