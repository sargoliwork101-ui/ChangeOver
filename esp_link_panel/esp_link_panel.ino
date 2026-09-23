/**
 * @file    esp_link_panel.ino
 * @brief   [EN] ESP-side ESP-Link bridge: exchanges binary frames with the STM32
 *               over UART (921600 8N1, ESP_AGENT_SPEC.md v1.2) and exposes a small
 *               dark RTL web panel (Vazirmatn) with three tabs: status, settings, manual test.
 *          [FA] پل ESP-Link سمت ESP: تبادل فریم باینری با STM32 روی UART
 *               (921600 8N1، مطابق ESP_AGENT_SPEC.md نسخه ۱.۲) و یک پنل وب دارک ساده
 *               راست‌به‌چپ با فونت وزیرمتن و سه تب: وضعیت، تنظیمات، تست دستی.
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
const P={0:['آفست صفر کانال ۱','count',0,255,'n','مقداری که در جریان صفر خوانده می‌شود و قبل از تبدیل از counts کم می‌شود'],
1:['آفست صفر کانال ۲','count',0,255,'n','مانند کانال ۱ برای زنجیرهٔ دوم'],
2:['گین کانال ۱','‰',100,3000,'n','مقیاس نهایی تبدیل به mA؛ مقدار بنچ ۱۰۸۵'],
3:['گین کانال ۲','‰',100,3000,'n','مقیاس نهایی تبدیل به mA کانال ۲'],
4:['آفست ولتاژ ورودی','mV',-2000,2000,'n','بعد از تبدیل مقسم به ولتاژ ورودی ۲۴V جمع می‌شود (علامت‌دار)'],
5:['آفست ولتاژ پک ۲۴V','mV',-2000,2000,'n','کالیبراسیون ولتاژ پک ۲۴V (علامت‌دار)'],
6:['آفست ولتاژ ۱۲V','mV',-2000,2000,'n','کالیبراسیون ولتاژ نود میانی ۱۲V (علامت‌دار)'],
7:['پنجرهٔ مدین','',1,5,'m','۱ = خاموش، ۳ = پیش‌فرض، ۵ پالس‌های دوتایی را هم حذف می‌کند'],
8:['پنجرهٔ میانگین','نمونه',1,10,'n','میانگین متحرک روی آخرین N نمونهٔ جریان؛ ۱ = خاموش'],
9:['بازدهی کانال ۱','‰',100,999,'n','فقط روی تخمین جریان اثر دارد، نه شارژ واقعی'],
10:['بازدهی کانال ۲','‰',100,999,'n','۲۴۲ خطای over-read سنس کانال ۲ را جبران می‌کند؛ به ~۷۰۰ تغییرش ندهید'],
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
<table>${[['ADC خام','count',0],['ولتاژ شنت','µV',1],['جریان بدون فیلتر','mA',2],['جریان فیلترشده','mA',3]].map(r=>`<tr><td>${r[0]}</td><td class="n"><b id="c${n}${r[2]}">—</b> <span class="lb">${r[1]}</span></td></tr>`).join('')}</table>
<button class="bt" id="tg${n}">—</button></div>`).join('');
[1,2].forEach(n=>$('tg'+n).onclick=()=>{const c=D&&D.p[10+n];if(c!==0&&!confirm('PWM شارژر '+n+' فوراً قطع شود؟'))return;send(10+n,c===0?1:0);});
function ctl(id){const p=P[id];
 if(p[4]=='b')return `<button class="sw${id>14?' w':''}" id="b${id}" onclick="flip(${id})">—</button>`;
 if(p[4]=='m')return `<div class="sg" id="g${id}">${[1,3,5].map(v=>`<button data-v="${v}" onclick="send(${id},${v})">${v}</button>`).join('')}</div>`;
 return `<input type="number" id="i${id}" min="${p[2]}" max="${p[3]}" placeholder="${p[2]}…${p[3]}" onkeydown="if(event.key=='Enter')num(${id})"><button class="sb" onclick="num(${id})">ثبت</button>`;}
$('t1').innerHTML=G.map(g=>`<div class="cd"><div class="ti">${g[0]}</div>${g[1].map(id=>`<div class="rw"><div>${P[id][0]} <span class="lb">${P[id][1]}</span><span class="ap n" id="a${id}">—</span></div><div class="ct">${ctl(id)}</div><div class="h" onclick="this.classList.toggle('o')">${P[id][5]}</div></div>`).join('')}</div>`).join('');
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
  [0,1,2,3].forEach(k=>$('c'+n+k).textContent=t[b+k]);
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
"d09GMgABAAAAAIz0ABMAAAABH8wAAIyGACEAxQAAAAAAAAAAAAAAAAAAAAAAAAAAGoIuG7cYHJN6P0hWQVKEOQZgP1NUQVRUJx4Ag0YvRBEICoKGMIHfHDCB"
"xT4BNgIkA40EC4ZEAAQgBYRSByBbpgdRwrZpXAy1Ewhcqfra8BNuDLe7FSluOkE6EiFsHBDsQUsm+v/PCWqM4UN1D9p0bkImuFwiut1otLom7xwaNuNT6qmx"
"qkWvsCa1QMwr+7HfYEc/p1NQi4heoX7r8monAAlkBCA/P88w/J8lJ8avZwUT/A8b9YhbaDjlhBYyWqeTb/ZhXyEDZxT3nhvmcWqSHelowz1ngDs5IvGiT+rs"
"e2JbliGyTEkWED70R+VV7XUXZ/cQpieCCrFiWvx5fm5/7n2xSMaI6NFl0CHIECOKETl6pPxZCQy7PkYVRiNGbgjm1oE2nSLRI3LUWBYbsbFiwQq2sVEjc4TU"
"QEAUWsSBMgEV+9v41/f//dYP33/1w48miOz++a/u6dm9T0IEOhIjwDGoDztiRJ+BKlbByuyu6uX73K8EGmWxSCQ4jEFZwmNRcHcKoU/xzKCb/yVQHlDhm/h+"
"Pt1qMg13j55MNw02LGL+xKnxaQhpCIQQkgy6+X3OnuuZ9bVOlQJNIdBQCDSEQJUTt2256U/zn0x8+HfT/0MpFbtb1zHRzq6pfL73+37Vuu6K+ev1zSu3ZYwC"
"TZGCJyGkIUoIkZMQAhQrrU+wAHD4n7P+MQYRBUQ8FAFROCIeAQFRJMYWxzhmk0zZPq/0mjJ7y/cr9b+1W/uW2u6UmtndTI85cQFcpmmb+ZsqBf3tb7zerwAt"
"UG///3eu3m39vqSFVdUnFaAUK0yWU9giSe9PeuaPnvlbsTH3CLLiPPC/X+3Nqr6/bohoWhKZQyKlO/MPhEqIYs3F//en1usihJAtwMGYOMQpdcbba85utla/"
"2r7eZyuI12vPtuLJJBmPx8YFMBbyv1TNdh+W8C0keYZ7kXAk7eEM6EiMi+ZCp7IiPiSDi2UCL0IQdSFCvEBKTpCcU3vdueiudnUh8c4hhtZ1KjoXbe+iavz/"
"29SvfU8j2XLywfroZImcRafoZC8AVLvtbtHM3KeZNyixI0V27Chk+CDbAcefZkZgSZYhiR30B6BqaeIsaP1J+UDVAlAJXH7gapE7oG7LnK1+UW5TMvf9Vi12"
"gG2xRNY5Hoz82ovWrdoE6ADr/7/NsvRfgUdtVBu/ucyvjGXKx2NKHcROomZVo6s10nFJbaiunt5T3UNfDT4lmRoMBAsITzJ9dU/vKc2SljXLQboZEEXrKNrN"
"c07T9WVvo3gJNHpTM4oUI6TMzN7fu4qlHuVE6rPDnNdTVUHHcQxBEKygOs538753GGvVUNjcNu9d+3U9ivIKiEiIqGTuAyrAXNJdwIve7GQgCCJz9kOYKcQM"
"sZVQkSMJRxWSsjSRnIUp1EHO0iYryWW9NXKd3rhPn1Oq9Msd9sh9jsXHnbTlKQ/V41USnqaEQS9JvvVWfvF1+JffkL+oxv6FAyicarUdzqZIX6HSX312qb2g"
"aLTVkG1255Wu8igBmLtGXrhOgwGb4M8KX/zu2l/KIA1ke6y0kTCKkzTL5Yular3RbPeHo8l0Nl+uN4yut4+vf1BoBLZs12O8c/T0TWABoGVZQh30je0D/7Nm"
"HTsO4s5ZeP0igF9w/dQFAL/02M3LAG4vAI4NDsCQqMnDc3HSVNXZ6XO+u3EVBF8y7vplEHnH48KUL2cRwC67cKmB1e8DR2C0aZIs7C9HcHDpW7Ki43vc96Pf"
"//kDz/4zUZSoY7/pf0hTTPwXYKNYGsWCNYREY2LjEdCgSYsOfYZMWLFhx4ETF648ePHhJ0CIXAVkVFrJrbZRvz2xAowJY+cASJK9AMOUVHY76aL7YW2CeBmy"
"5Fthp21ucrcHvehzP4x4AiZykoY/FdM6ZVMzHaOcdbN3tsxeEEsIRmNh42lMDFhHZQ84ri/5ZuJQCdUb0u0/Sj16qaLNdjsheY6IFcNniKHbDw0dWkaIX04j"
"cQxDMUT4Ogx5jwqIaocBaeN5P71MBI00luMorcUP4EIsaOQkjbdShrbUojJU5dWYh1q0oevUyiE6NORoe6ToxG4EhmSFQ4z2ZxHV4Etcgj3FnuMZM/4/TDDC"
"WY2eXYmo8FHvls9loDyGwGuo/AaFJBlFV6zwdqb+I4+44ITS+jwOEyVUAM+kHNN6Ryban2WBkXhI8ZwkcqAghuaUqdcmbGENwSoFVvaqWBxfEOLsjR6XyEcc"
"Sccbhh0Zqto3i/dQVJ4PaQ2Vq/oZYA1tNjVNGq+kAhmYRzyqq6TdhP4jRTnwDu/wrgZ0fTqCBzEvIn60oWiHgyBlR8WtubIGpGfshbiMDKSWkqaMMnmEtn8J"
"kbKBlxLYC7y339yCGVPzZNb2y4KQgZfdFApttKZEn1ZR0Ch9ojivFateeQmrJKGkPNmcWcAVVyRdFauybEnUS75BxP/rwHfaL34/tq5lFNVLqLQgExZ1s+9F"
"+wxc2gEUrLmzwfXNRx1C5DsGFqvi9ocyU4Dar/c36l7qkTFKpa8s5NzdWh4ZpUbRypJ9jsX7oC0t6KWEKJ+4QLt3Tpgr2iheGOEbX5ltmn+W4ZjxOM1wqbTX"
"zqzgqtFVykik6GsiYxgLXkFPU7rI2YCeZWX9M1bogl/ze2zsdonXlps1peU/J/jV105CerOJHfcu4bmNL3KGQw4mTSu+1ckkC32GNTBap+NwVuJd/5xxJc6S"
"GZzm04LBPs8xNPr4MWd2XoAidgYFGvRWNFY4N3nbWDU/7FkhUmmpgl+GM+/egJWRWYVSaViTtMia0K4iVmGQ6Ua/KhnkGvY0F75Z66aVEQTTAcm1wJnh28ae"
"1Yk0pgXB2I1VknrYPxPdvkL+0rXty5CK2fi7fKag3jB21H9csCt0GYgFJlnEGHVaZHGOsQYSzqrhV9otLOwo6UEzSDFrNpQvCec2SDQ6hnw8ek1K1WrW9pr2"
"NKPNDjvtArODoPMLMTD2TzJEr/iYEBYGNpIWTKf0GNEnYEDMCmKNwYYxF0IS+lxhHgx40eLDhB+eANqCUKK5SeIghaMMB+VxtMh+MrRStFoszXS0rchiuJQ4"
"mjF64UTHxTTGL5zoeqALk0AoGAsNYyAIYEIErdG31nAIdoDZIzgRch6R2aZZBqG5YTiWm17EvMGQDyO+jPnh8x9dF5rWMuyXxYsvldcCyiIO8sHVPBYWcyUb"
"tiONXcxiR0MXFvkSnpUg0EfgULvWpEecy00494+sAwEwXJYcIUykagjtlLuxzAGbVYDKGtIhmAiJinjeECzYaBYox52IkRKtxgKA4wujt8NmjUlXFjvvgquu"
"ue6Gex6cyX+Yv2FjuCKLlVtikyVWU6AKtXm/tlNO+9+Ay6647UvESJKrQLVaK6xWKDHzhDeeXnF/mRuZ8VnfD+GdZ364S+64676HvoaFNBnm2S4bXrQzFCFm"
"C5c2FsppGh1EfsNYFZkiZY81Vv+yBLzeXzSwR4ofYxFzTEF4W4ysTVtqtyeYgqvYNYA9jEcUvMvGuQ0X/bWLlbMQplmCWZyGkAir1mfvrMKtjxWibj2XZdIW"
"5bNY4JIDPh2CG8VDEBFg8Gjd4d0+UvT7H3SDKBaRqQ+2B/saW4f2p6qTEnhIBh7ro75tR90T9UBo7ycYlylzgV9P7PO0i+R2vUIeT5NpDB6Swrsj28L6lQrQ"
"9cpbJNN2akNOjzsVQQ7VJPUWsWw9ht10C79+R0jpvV84WyCrvb7jCVgiIVMXAuiRCCHBkJ6jaQ4Z0j5Q96ad0hhLGunZpGk8znjdIsfB62j+sO1YMNauPr8+"
"rn1KkDG7Sz77cNZSSBPv1JGY4q8who/Cve+HRoTGXW7HaxMYTYhoq4+iql66EHbO8/pZlBHaH73rMLDnkwDyWwDMxb5vZnUXUnRsPm7PZ/ErMhz30O+jNkrv"
"x9Rf/5iHmxFYRpR6UncSf59K7SHZXf1ESpar1DKbbXaN29xd80Z61cve9bkf/DEYi8b7dLtV3yRs8MMY0TyeoeXiadM69eu3+GUsd6VbNtOzabZv1xyc8yuh"
"OvMH9nIJ7eP8Nxzr2Npt38ZfYddt7+7cwzuwt/fpvttvAXJZUhzfhCYl8NBTGGUq0piaKKJML+TJXJfPMzz/r0xKV3XUJEhY8ZJRrpS2yt3Zjq7pth78pU/3"
"Zgf77YBe+fwv6WCHP8aJTnetV38dt+m238E7f/dv6MVk6L6iX5kv8ovLHp+nYx0KhyDiQK/x7XZbzsGuJmeVO6It3r9hT2wA9NyC9vtrzIpGJBCiVTSuO/kG"
"kZKMSAMbYPwWAEuPHMumA5+62bs3YmksZmJ5zu2nWlaVMy8177XIWb/zA6hnM9N/PQ27J7kNPzO/T/cebxWcb9u2HfVxxYcjCOfsOoePphZtqytX5eW+akh/"
"/Un2jpyxUVV/r2SB3DDTFI3lcIV1jCSTv4LIpEbSgnVJ31dxYX5T5HMVnYlDcZg3yke+eNVWJ1GR4fkTiBOppPPK61iEcDXiaphN5O1fUz+ZL4+dVWeNflec"
"XSXAP6aHMvb2eDrjxf6ULFOl3Ojbn1Nzip3DCHdFaXNNq/kgyLK60ZTxXxPijDI62MBF5dEjMI3p0eFnQchnCXuy7+U1phWi7zfTe1vMGCnEzH1q5ShNyuHO"
"fQ3wFVkfy1DGX8H+M56T2Z+WRccqZ9nYm2P3xukVbOU4+x4BvsK78al3tjeXxnGHQaHbeQh/pvmTvnpwfphzxnG+WhRHWr3G6oyBB3g30ejhnEVVm7GP+dPm"
"uWLEPgXYpXqaGnb25iSBnWbHlJ3GMVoBOIjbh3uMAo1olEwYWVXoFu8W4iiiKr0mrFcalRr5U+UM1sPJm8F9Q7paZWPGrPYCitadg6ivFd7n7n9WUjtnHDYK"
"V5p5Y7fn1+bEkMbw/nSNpMOcNydX0KrJWdccRSVCvaE5saLoVRqBMbMkR3XeGieZryDXooiB4e9nBTvSPGhQl3bMH9hKmSObLeZH1XRgL1AUE1WtZh8BdNOq"
"qaOnLfA90WZ1gF/lr8FiM0HxIKpGSoB8bTUu74QdWwDoUZtRdGvO6Fm5eLB/+3hs2EK4HWCkVkmX/ocvdd/cttH/BlBgul53ufwbROalE302o70/Q3OC3WjZ"
"OTGYqOrJBFVZ1Ld1FL4oLXqeZUi+1iuommis7+/l7XGcp3peWPbdfIwvZFCtAHq3OkejmPRhf/TfNA69yrmGQ601lu2MjDeewzvMIv32w+ZIy5TzyE9BOKpN"
"mC2MDdgpjy6GtbRGvHxa1zao5GO9NcHaecwHe0DtXuWUz/U88oTKF9ZsyFde6iHoLc/3KL7Us8pcydmaueEZvfvjRJxZeIRmSz5rj+HuJmZP98YeewqTL59A"
"4ZRQUJDEeTAaaQefGpp/0y+C4mvDYzJ/Xo6TeTtvXzHXsr/wgWlSP/eluKL4ooSiRKLE1mJUHJOO9Af+lLb382SKu0BE1RV+i5MCEWiKCQXY3BRHOINocCCb"
"hfIH8nA645kfQc+2ybEUoozLzpVJFFI+hU7Q8qkd8jRZKdDotzcZPH5rQ8b7dxHGaYWgUSpQ6kB7oVJWDHoLS1Ul4NifjPdqYPxP9XXB9E+9R2ApJQKwdsfP"
"n/3SlNLyEnBTqRIqwVt/kFwE7n3YUwYequrdT8rYuo+1zJd5qi0RqsELrTYmFrzSNU/+X/k0mRAt2LT35zK5WFN3nAeBZlaKXADiA8gjlC49Q1McnMn1uZDz"
"NSNwEacpJYjBA9MFN7wSZiTAYBT682gP7NXb8tPLJAyXumU3X2c1xuHmagZres6XcOSrOvNL/5Xyqvj51MVYkmDY5VUsHIj+P6VNjwEjpqzZsufImYQbT958"
"+QsUKk8hpTId6q2zxQ77Qvnzso9WKgMYvNyhidxLgoTsR4EZC1XI/WvVqdewM5XtorSSyiHxS1Tbk148HsFvJ2JJY2bcEQYb4ELTCO7uPRETCNuF/dpVkBoH"
"2YyEqJJoksZRXKr2VzrkLsG4GzJFrgbFyvKRypUiFeNzJSxuIxWyBBz5Xw+hoNJ3wgkyjKS6go4xek80xjVGgVI6XRptsM1GuxyIVSBcmdjdzducKAPO1qHE"
"zaAAkMNl5+DbBlRZaAykph70N/SM0TEDB5c0R64SWuU6NVhvq532x8pBU9mY9CLW35wscmptmqyxyW4HozsUwx5iB+vsxgmMNNPNMNMs0RKkmmPuuTGcwb7f"
"JUe+QiXKVKqyzHKrrLPRcquUq2OAQQYbZpTRYORZol5w8xbwCIQ4ydJlmi/PIsW4ehWWqlFnpXobbLHSGo0MNNQIE+jYR4HtoaS/a0Ciprvb0A4Bo/i0zgG8"
"7QHaF8PECl3a/u2oi1wLxeD4OUMRiqDM0ZrD8vxshaFHjDdFuCix4iVJOWxm6OojxARhIsSIkyiZ/eGMaSLOkkzb7T4CjIOijDhbkgiZpda08HClqCcjjAwy"
"oS9r19lGZ4xx6iPl4ha0eiOFihQh0GUl0/fzUmiVeutssMMqm2yxlQtvqRQ1o3CuV07BeTMzzIcEBmL93cpRuIijWOCWYZX6GM4Qk8Y2xg7S0O/JROgw0N84"
"k80WiTX4q+vbVM6q4SubUDT++ibp2FMLqDBNzJCEEFtmw7QDbTrOPkL6JPxEaFH8Xj+RnZREpELiZ7KTE7CnLaS/WcXJrcOpg9Lr1M4XN6Fbp3vkIoIJgyHs"
"9BEZiIcEza6GOKGpld2upQUSoCeFCMQD9SZhvo5DBpMDLqopnpcBKBGJaMRGLMRFfCTSQWddQTzZIDVEiEAUYiAm4ognIaFOuoD+qgN8PZlJzZXAeD7k3N/5"
"FWD2Y8+n/3guTlI5oVCUFQ0afZozXgnZKG0QaFgnlNtOJf8LwyQ59TKqZs2qo8YRu/5WGEv0hzPM+idPnT1/8TogiEA5f4V/xD8jBZ9qnt8GgV+psPl84ZQC"
"UDjGuTAyDLNSLWvlMqAd+3T6ih0lv6lne1zr9wbLjTpzVkCY67ATVg0qONxcg82LE5eJaqNKcTlDm5+s55xe3dEdsdgqTGM02xF/d6PIn5OSx1LZutLLWd5y"
"79ByidxoR5yZu5B3tl9nqqv1ujGZ1zxuRUgdSjZpTsfthlF96BRPkp0pSa+xk0nvIe5iigt/d0jsUW5p80xiizrrdblbL2N1JI9AjZ+16aJOTlXqwSYHUF2q"
"6vJI5H1+iSOy5xnep/CNVLwBWrjmMv/9ZBSYOmnxOfyAhCiBT8gLyAYHB/WrXv3F5mrT0dMOnM6NI5A11QEoIHbrOEA2XGqxH1OUjrpwgA7TViBwOQBHXxNK"
"mAl/7Rx4D1fMeCGQ4Huc5BT9V2cM4BoZvrp3ufrbszc3/VitJPZgxyUBNCjU+M3uIBMazneeccZOS/ggx1HqM6wY9pydYv7w5LaFrJCH76QATNBLCPeI7QCW"
"Fb1kxJbZtGfi0gbndwEXetCDH/YIxzjF1V/7ddzm67vz9/Orrv8nptamEnXHjIH0uIVc+GUfst6nTw4c/zaY/5wz75QZ06aMGTWiT6PyvdI9zh5tj7gXB+Dr"
"3R7HML6PJwALHubct/JP/t+nwKQaaUaEmNJCYbRxKWqM6okVyO3RbzqZRatlapVsRrGxqh0X9RL7HX0CIhqMXQtefp5wHBIK7KMaiRDpo5eCxkZhjgwagBsy"
"AhJmVKoMkInx39AMoxIhS5iZCTTUYoCemQuwAxZ0uTRSnFtPOMXyTewnzBJLDMYIY0wqqJ6LJpJTtI+YMOaJAobmdKG8JjwaNCkXLSlZqFPBADlyBF94bu7a"
"JFAIUiUewJG2gcFHshILVDofk3xcVkNHjGTqzolB7RGSkSgTTfSbFgBHYmzCijFVL1CFzHhDAkUQ4GDUQKO+I5SfWBKc0KkmrMa5EalTRENkAHEF3KZYqlXl"
"ky5bEcNmDgt6NJURkDCZukNrvLHUOUbms5k/MnEjEGhFQUoMwYWRJOpkYp0yVTwsCuU0SeNmI4coXEztQuktALtEISwkgJhNBKskIc9LiYcIEQty6Yul7pKj"
"jFYLVE2aUMyyZBFcRpld4Vl2LxSuJ8wWRCWzrlIY4cKhh+O0mP1Puy2BRUdgmhHNCHGGXHJpSgAki7VbjboD8CPiRBhOYU5KxoPCkiKxVZxQBEpWi+kI8FCy"
"HbhbNI5DJATM95A4K1phi6eRzO2k+WSIIurlEJbL1XwxB7Y8c4uFAkUIOKhwBZjm5bXQOtsXpKWDvDkABb+9KSWJlNHqGckd01XFKWhzzEgOpii9WUV4Msjy"
"fY0gBYmD8VsCSh74JNTwKXhoY+gNJ/RECBB69nKSV2B22iYXlCbgqSh9V8apPa1l/935rHVLT3VDZ1rK7FtmUBu0OC9jKSBWNwW1c2IWQ4U5BdVnDbfGX/hO"
"jghWV/2Bzr1+jyEbiTt1pQfoYC1QVBdYprxkA8OJtMewgYe8ifos5qIPh46fsJI06izq+ryJh3fKyTTrjGIBxKR/+LihTFOC2TbDQTLn/MzNct35os3QWy7d"
"TJuv+2gOU/RRYQLTsBH2TWniNPzTkBDJ0nxfYiCcwIdQBAdskxIoWcxs8ghRC8wmO8VmZ/6si2mWB/UFYTrqKUyTapDT4bAMjO/gwNnScJFq1DQ4wYkVlD5B"
"GG8UaDCOfikvbCgOvrx39DgYW8M5T4IHZrU76Q9GDaDIihrmL8rYMQQgLC5JVyoIiktyqxRwCj8WpHnKxRTiLvGDqCV54pxMGPY4IlJFyxnTzskNbWtlAkau"
"10NXnFl6VBk8LYJf5AOt3Mb6S461plkYP1PVHmlLYZTUlTwviCQwqahVevbZgE5N+MmZhR+tTppSUp3AtC+JY6TPMqVYPCnhopzDh/kYJKtWinh4mskXv0ds"
"StwKjSQexJ9YwnvJd+sFhDC1F1qDviHi7FTuzWhl94q5BQVKfNyAF9hAF0BZkim1y6RjeKydZMeaekWfRfmQl0WXRZ2YTcAbEu+5luCG7a3yvAH21RzWYAKz"
"qJeto4EKsoB5a4bA3i99EmgTC+F5SlEFYNoQ1BFiV7QUtqAWnd1BG/QUSnmRiyc9zR0sSa+4UrUuvyNVrR0xeS2XIYcH/kIFhcXFSRUNWLx/AGf1ozWLeivH"
"VX098NbFjmG8Xumz0T8gVBrdquQnRUgyHpmcFIx7zMoIUYz5KxvjpM/vyPUF0sExffJmXOujTs/TRkPgnxaKbz66qjyLYHni+1q6ezcW5+ejBS5lN32uc8uz"
"AAAP8Q/t3ULqLsVFTR5j7V483XoNhqgNLd7eofgA6YYJqXOsVEEjKC9iPv1SsUuXCzzXLTvzopAjr5Z9dIm3eejBk7RbdZj9+1MnMyk8okbhT+WNz1UMydY5"
"7JTtVHyoSglV7eTxj8Uqve5O3BHogA9KQdjs3cZaWnywzr2Wn+RHhZUQwSROE3dE4D4uDAFeAqR3kU2YTVyDmxEpzBme8t0IDMcEL9edO8hMqpoxZY+jz5SS"
"vj3Nkhs77t7N3LRQ62nI5CF9Y+U1htGrG7xZw6KXdopg1NEiKi6XfsaXHdru7po3WDuJnVeWhZ4JGkqjktQneSC12sGU8tOsnjbkc1GVi8SPwzuF+UWIkky8"
"LUVQZ8q05OpG6UkOJxPT6Im7xQKC6WAly8Alh3NjKXZhEGDTRuUSE8hOhEUxp3nFnHgfS5WyLJPb/aFrrLDiO74LzKsoai20jQ7Zpb7WLmCqvx0gVcZMRVU0"
"mSGW6wtULgwGyIfHea6/ixmPq1pUWT3jKUzXWEZZXOQqCUySrXp02MFz1AzJIZNm0Pa9JaT1MeZCLzh+PC+XNLCeY6jUouUWOwLBBVByXBqK8thHX4PD7JpW"
"MjjzqkHFTjGMJwgyrL/qXWikeDPB3HiUk3UBmLhHz0/DvHoeYNJ6oiN54KRXaZjVgYaEyhS9qDkmRriqqmDM0UJUWistUuC1PvAYd5CQMZO7i1DF6PoOfKex"
"I/H65CSAZVoPRBslnKCNUGVRyu7YB/okysQI13+nnClDKHh1Y1H3PASHb3y5GzVhErcPN6Mr4MrCTJsfhtb7SHdClVKY/H4n3ZTAuABDjJqPhg5HJMlqkDNa"
"fSgxOclcYsqXOshOW27jF7LHLn5oUGughI/BEmfFOygAoWsYbkRKqfPzttT/DAZXRiinbiuqmWBEMWIwM8Id0jroer8Uq2G4jezHuZcF4F3wQJ9dSOfcogfL"
"YRTMb5hQEWw40S2WzWAKifeYjW6Jwpv7IkAB9mA2Ugt8d8vU9KNlBDi76wfRgtK8wnGoH4XIe2T4TwkBAnyA29QsDfhKuKfQA6cfuO6VlWawVRLaD71cdIAN"
"sI9fZRnQPEEmsXytnCu5mqFTE20ZKPLUd+0kEbLPI7npwJR3NT8DNu37ARL0ZKuSgVbQg9Q9CfudoBnwAqCvKWEQKz8gGKdTho5B+fe7KbWyFrB8KcUKip6r"
"9ooxM2R8WTPoqrJMCkHGKQTl7JrkKSz60AUQ57E2KLfyqKpBvUc6bsoYiZheRQSfMcb4AGkRACMva/c3lg0uCzlh22fyy/2xNHvqajnsTvryS/XRnrkkPzPG"
"9aOjcvAJaPOvk5DDXdzpMSob9q3Fa3VjFsNG7D60KHa04cH3eVQmtl/jM9A4BegX5IAWgkO4qPyYyRfsht245n491wPhMmUA1FqZS/x168bP6JXo6Hdxq0Pi"
"rc5Qo/REyUZqtmYpzPTEphJDyLJkKnWfJKZWeGjbjPSRVUlVMtLzMet1TxH1cPGf9iS++jQdwpQABLFphtx+xbE4/JVHJiK5A8vnM6MPCnq1KuSq/8uzrPSh"
"67UcpoaRhMvys+K3y5Cf6dpF46aFW9LDTZtmk+a+0vK8vlOWKFNZ1Iqp6uZ2QydomJqCtT2ollbOJA/TPd4nT6dCE2bnQt0RGCMxNxaMb8KeuOhpk43qpNgq"
"fffdgipHzSaKJXoGj71Lj4waylVtXYq9eRCNMe1l/qygrsWy0CD1A/4GsTlZPhOMZ4kg8HonQaP2A7S/QDtyDumleklaxO+di5JUMU+qV8lS47akK5ttlfpD"
"dcJY8VuVqVj8L041SZkDHQVUq0CrZhs9Lm5pZtbcn8COvBsLnrTgDqWZU4gadoB3rTrXMjG6PTra5OOBOGBQctTc/WdiSUJ2K/HNe3gkeUM7v+nS3WoFAZhN"
"D5DbnxIoQJzJkvfChnI0U7XGemp0qw2/t4tw4aFyNh750NfWQb1y7Jn9RKL+y0vKyiY87aybMxhFvVoLjX8UKBOTtsQgMroFNNSudEMuJaowtAM5cP/ZkbSs"
"YYBhb6a3rELF4YcqYzxiPpmcldmJs+9+us1FqMitXlO3oYodEArQ7cxUWza4Y943Cf8MVs5+JfQ10bThSqNWhElyKFDopU/EOFbeSJKbbv1fic8t6nWCltel"
"2shQqxM0+7lv3rO1/EO3Ex76KcOkS+LMCbYqwKHdPgoWOwIUKCjP5uYr9PvmrjBNuSyKOmf2ChZVuZpbRSBbmyEwaQ/E7KSso9I9JnAcfxc6eO4gBWjj8puV"
"sAb/2G+tPaJX0vc+YfOpjYbLuKvfro7bhF5bG87epsu82agoGdJLruFRzIvro7inXeIUNL68Uc3Exu9cG+yIsavE+F7xRLfJG9jBZK0OcXPH9H/pZ60viiSG"
"ni6//diRoikax9g3nqnz8qMhzdIqQUxchFjPsBgCMoFTDL7QRbFM/wqAoa8rGUszL6CRcgvaImTnalF5kL1CxUtamRfIKvi6y3d/VfV13I4Bni0CGZdHh5QD"
"3Ih8cvPnh+GzNdvU3hkjNL55MBfseH5+Ltpq03SSIEt+R7MS9SEwhhC+CSWZhWlNIwabnXTQY74WAAd/JRXzcgxPITydtET0WazRY/ccHkg5gklnwsIgQeNN"
"bxOF146GTCoU30/THJdJ6Rw9Kjq4+5BlNzr0dEdUWcWKxp3ygHJmEsCew7kz3q7QX1MufVtzqt1msSCX490GGzLuqqHzm/9XMh3RmbHK0ZUbn99iVsjM2QxI"
"oVBjw7j080lQdQPPBM2390acchSZozz3SKzgol0fqplOMr1aHUWJ8Sa4Iw+94xFn2wurko9sHslrn4rqKCoXRVk9UEOmPwg1WnNDUT3tKuwVElayXG4oruwf"
"lcrw2hP3o1NysXl4QsKEAiCB3iNd27d15ivxt0Lu8+L6p29wEtGAoniu57tjtE0uhBnqiU2ASOQaDlHVZ43r+Gk+e+JHfHltQB3FYvnNDRwyE+9dLceP8i8p"
"0yIgSj8OlMiUSgtjDFh0C01PT77ZERcs8R+Dd9hWtHqeTqho2wUSvu6Feq17Ib5S2uSUfLGqkANU/j554ho1Mi0aN7SCqN9xR5kW5mYeC8LhgqlctJE5IilQ"
"iharOpTZEOfYCBCgUms93FfcMIO/p+zPMJEoiG8Q5FbApqnBw8XkmwixbGwIeN/0med03DSwevJM9/6zQz6vDAudhw3usy+sp/U+w4aHa8bzRhCacMl/aM1k"
"kp2A5ldzLjU13/qtdNvUS0XHR3PPkh1rI+xf/5xf4rdtMWm9/llicaWQiKzqQoobb+HKFTGkgglGsYF3PaO694GsYqdmauB6CPsL0LdJYzFBqRutKa3qVvR7"
"g32O1wBl+vSLX1vnWOyBU2ylNemD88pJTNdYbxUttbwmq8jJYqKqQ3pwUEQ8zn1n/wcgUZRl0eOjqGXSWXX0yk9fbdeN3nraOYch6AxY0X40vIo75qGtFjDz"
"FYqql6A2VMezbKRm1+MWvloEobK7V/ldDR99t+zs6f8W6iDvDL5jgOxfPD//pKb+kW3/VZexMdNy01pj/rfHh/opzxZ6mmoGSfhB4K2p3rFSq9ffEY+NftZe"
"/etC0aL+18cVx4+9LlKfs9Lt2GnrmvplfRysqba4PHujS6gYaagD8TEWk9GTd5ulP5woWJQ9e2PRAHYae8V/4WTc/bz7J2NDFjwSTYAyXQOUCbw8CNoZTSu8"
"nI0aeAPDNMfWe457LvzZIf9tKWSx9/f/2rX2GgzbBhCRPzW1Z8qy3Klka3xZ2VSe+Q+N8s9p8PvRGLO8/+vg680737Rnjw1+M35sHLxxecftXH/6Hc69E2nO"
"plVwFODx5qAHnQlTuuB68muny3q7dvo5P4/ljnJeXW0aaX+99LlSIRss3oEB13kOJ4mL65/unX7RN/1yGoiRvHwfuQo6PIzNoLN0HMRefV9rsbzoDnZ23XH2"
"eOIjzUcdCfvmLi1+UVb/pkPjNWsysclAVnesNuY8n9MuFb/YaagtbgjAxAShaZMgvIC60y8lnU/unXje0v6HKeVs5R9PSmeGb3KV87byC6tZAl1KYWk/D3Wu"
"PreJsTnBldVqEspfeAyAEUTmlPOUj+ZN4im+ujwIsOwZPzkeaHGV7qAF6LSpOaTlrMUPU8m15QNXDwyU37pT1N5+l11+sf9a93BDBbEKhmoEzUnQHahKNL3G"
"UFfs4BXjFTvSGnYxS6VQxwWNKVzI4fvd/j9/ys/ya5PnrTJj6BMecDPneYKHtBzJ9Tlh8Xcxft8UsKhYCo+n6c3ytGWkn8vRiw+/r52w/HBmOxH6Fv+zIPTo"
"1NSHU5biZl0C+JiMRp0prQFfbLro82DW5+bspaCe5bHQTdO8xbmqlKtU1BvU+lUlBVjfNKy5a/vdhf1rfs6GjqOBhinPv47tRb4yKA3bx8DoZxU8IihN2jnW"
"dZOoff2rJ9yH127Pg/c3ZBcy+xGyirTxRMCMdsycsNPXtUtU3XzSwNXZnr/fECxxASS0StlxPeqBUHjmdK9F75VIAEphMGwH3VAqt78/r4DzEzaBYKFj3hwf"
"fIda1X9XrPpi++bV00sIY4MoKWcaq6vLPuVpjbNM1vDn0I933rziYfaQFQsMUGD9AUyV6vv+jYXB4TCCg6cDhJhdHpAZhbB7Jzyfx3JMKp5MzytnRo2ncj0t"
"PCB0Bsp+BOVQsCIGzoGfL3+2DBLW7i98NADdt7S+enBpBPqo8NEw9MDS6vqBpQHobiL3DOfF2Zq6yvUa/osz3LOClxs1lfVrNeyXIHJ7MdxU98+TyonRbzqa"
"/jGFmRr//bpydOKrjvp/zZ+RZaTNQW6xcJBL2WxFtlIvHOMKjy8/AMegbVNtmh4MrIFOgLX0UDX5fu0j22P+iab8jabe8ltPigBnF3Gc2KUvYzH1zV2er7br"
"7ve/xpXMKdSSmddyQSpsURPYVKnwf/oo51E/dN/ShuBbb+x3Fb4wsSoLwE5FL9cncoWNIH6snQp9uQmToMGv8NcVF6aKBhKvrT8Y2V189ojp8dxjYcX1PhF1"
"brRM6sW4ZTGZK+3ComoI9aPGxSP1+SkmRzD0BiiqsgbMOcPKg6d7TuciWxtZ+W7ybouxcKYqIYWVyq3Rdvz4audGxP7rwOXfw8DfQY1M5Y2QukL6w+5rtDMa"
"jX7js+KRua9qK98ou0pH1RuwJcVtGTAZMgPNGSFU+s9F368WzWsq6s4+4U2IrydPq7hzJVcJqXIRqtEfLe5JxhaDuNjw2CisLlXp0eh3mV3cIeAq+m/Qmntf"
"48tmBDsFDWVtddV8TFQqsjFF7tnkt8HndPMFpYZbxLb8lchuFq2zcAcaRaPFydzSczShqTgQ6bR498IemxZPA+uvDIYtA2b8wTgImjdd/Wjd4Ec/KuuGXyi7"
"3y0+uvJRWc7QKEYkVrb8+FAGEp0NO9attRv3Cupb7hRUbrRaX7DORZbWJNDrhquaQETx8xtHgTKZHewITMuuibt/a9v77ou1X27fuHZqCdk4jJJqrhVXrYFl"
"9z0f3L9itmkacfnVpD98P+GTkSPzJhBr7rxH/pr3sKHP0dng9ERGPdrnPWwANhccnf7oTeh20gX1WDNNtsKvkG93u7qYqE+e8J2eXNPeZ+p2GDE5DP0h3tPj"
"eDOI1g0GXOxaKpuifrO/MVp/2NlgrZWEBTc4zhmQ8SLrHNGWlNdgG2iwQ+gzHOvtEaPRv4FQbOJErXW+fPvyzYfPilLEJ8TWzyIQnjd4b/hufNqdbcR/GrYT"
"svPJCL43G9jeMlU/0j/62EQ44H/N//IrCHD/q2mL9zb+13j8P7TqQ1ed+59X4rflBzCrpqYeTaXNdaTcCqwcSNn0r6JPz02DIHujkWVMcKqLaXWpqo2Zc6jA"
"GaONQGLajuKXlWnQUgiUFLVNlh27ym2pOE+RzFT/keRYHWF/7XOB3G+vRdPFE6eLxKRieTGzCUMs9ifnyzLEksLe9PzSOSZrgLV24PznQG829KOJZ8hcYhqb"
"z8gv0FGQvGECM8IJ+lnCrLrl2FvqMtbkLK1UMcHI6c5Bx5bms0/7P8QkZPMcca6YjPJ6NJ8xTazrw79WvTDM/J2d3TnWiac+opYOvifTbFcVknu6q8G3dhu3"
"l2slRa3MzMO7hnBMYAIGBYfBiJEp+fypro37KUxZAS9BnE0ml/dl8/g9WEIFOi6oAEaoDf9AUX7dFasoLG29XC6517FUSXwPnTNhtUxAP6CBhU/deZfent4P"
"urfzwL9bFSB8tBxs9Uk7+8Vh11BLPO/bwCYZkMG+v8eXyo/3wEv36C3YZv9QbXj4Jli+rwl7CCJnHxoD45OWB8W8vFYUCRHNjI8jMhXcGQsZbX6Qx8vvxGFh"
"AnpMLIlbDhpPvKEZVRA9ElV6MM94iBNWGmWMY920rSnc8jvvDpSy3/ewIVu8cWOhsqd51+5EXNhQXNg+rCNbfcSdVqrMuLBXdfiP4sKju0/f87i7pjn0C3OH"
"EDb77qvSVfnnw24EaVCR9yI4/4qebJy08j14u571qUWs4bIF5/HpGdhPGibHt4WAhIbkxhPL3DjZmKhTl5alRQ0lSbesmu3ONvy/s6dYWZOpZxS9zYHAcNVC"
"6k3AfA3sByra7t2VVHqwy3TR7xfr27su1Iq/sZsqtDsh/sZc29l5qUvwx/Y+F2XiuRqFDC2BYKiRkoWf7FDYRj2NLaqRpWyAqiXB+6NMaSbgzDQBenfkmQ5s"
"2jOCNRWBBbs1d0XUU2u8Pr2mqKHkrgxmnug+/LsouKwjeKILZpbdKSlq1Oj7Btgha/GdGtYlgFBvB2s6g9K3nSyz7G6JsEGtMZxlUadFd2sKdvcdcA4uqwgq"
"uFRzR0ydOssyqNXCxpI7sizzRBcw3XxqVIEDWyrjFUB3AmgrNl4DdOc1oxoc2AKBLXBVIoZLNO3lVcxdEWlnHwxLfx20H5T++r7iVMNlidK0V2XaC4cpSmH4"
"skZtwjo3nBe/3lgGBq6kkgjJySQS28iFWk6//YOEj0fY7LfoCCn27wiWKz/JCNzV7wEqegfQYMtLe5d5+R9Xqps6N2sl39ieKLSdEn9zobar4Uqf/Pft7S6K"
"tI06maiwgoRpcML/ZHaUoKkSCFpcr0g7B+yDJ2NHYw+iBVYjVoVp6Ym8Gkrxkbpgs6povqJGtn6N19J5VSG5Z9xqb1itYihT46UoWDyvllgS2Bh6rpQzU9mk"
"Nb3LNjDno9qklNX6HWZMKQ1W5BnLqk3JloEQF23sSOwhJuS6BaG/7kHemEiaY5+16X8whq+3//FhSfXxm92FL4w5oWMq2z2YuI6SPB6jiYM93xwuISz1i8RF"
"HZQ4uCo/nnKtEYR87i7Xb93jtbS8y6veatOVn7sr6Wh/V1J1bltFQ9XWoXKYehSilsaAV9Rm5ubWZJZyx/6ST1++Jqipuy4oWZY3lK5f49TWXeOEkltNuAxV"
"CQyHU6anq0iE9BpniBnWGzeAfpmhVA/vEioqrxBUw0q+on+XqtdfoZb0bzPT0nLlMRisLCY1N43dKCrByKKgzOOUfHGHiagpWyeKOvKLRf0mvEazjpf0b5Uk"
"JFA4kZkcFZdOU/O5mYUR8RQwfUu9VFZfv1KmFi9VNDQuVhzPTyBBM7Pw0IQEfUawTCK0c4gzoa6uPq7mKLhE1aR6Shf9e3LyrzHRsOR4LfoWZIdeHKM3gnYI"
"nPlzNGw04UjTGHwMNJa3vq6WjCWL2bODfB0Gq/etnr20veAPO/DtoVEkwrOgSFr0taYP35eDXl5oRh/0AXTbuP3ejb24rgCsh9/xa9X+mEqpXPkJpy+774wM"
"DC2LfmSY7Kmcq9vrka0Oo14YW2Yb12HMMVEg7nf12LsKqA+3Ne6aAGWfHcQ+ZfuM2/dv7MXy81j4Hn+3yh/7Y1bRzqEg7IPvt5Ijeir0IdT265XSRyM9okdv"
"L53Mn4zRi4ijh8Qm+5r++or8agpsWFyIGGxkcjncWOBXNG6/X+jufLopqXu7ISgPdKBbVYpPRo5VvvheviZ/I4kNTe2Cx5+BiOTjfMaigo051p7LzS0KoIAU"
"n6FABG7plFWQv12JP1a/zhWcqVYWnN2S9svNuIrBQ4oN6/zGvCKdrkMhaWvRM5ih4Lmw1i9m4PMWjutKCN4luqG/U4/J0jfhRfiWhBzxHsxxOAECxWXSMjJJ"
"JFxyDUBpPDN6l4Te+Gls9dG1kyG/vAaUaZs3E0TBWEdRaZTOf2Aa8vyWY/xvgD7YDs96EESbNm5YlAUedR6YCZC/nfHCsG0ABxyaG/aLq/fWdn51h6h56jLl"
"ApLOfxmpKyvOTs9/kDAHXTrar33zzoSOlNGAAXoMil+CoA1n4VqOH+vL/ADqMl/8x251q8SIlrdnXLaqvFnN0LzbV/z7FgPdrpKEti0wxQhqkXPFVao8eZVd"
"30jPKsVJqWuR4tQVfkPDNb7y1Fala0I6tyQNj1OmpXETXFPSC0tScNiS8gtdMI0Te/afFAjuaOLq9Q/rjMS6qRey06N3iLI6fEFBbSs+2zqJOIpJiCTHItp5"
"/ZrJNGpeGJKPb7qH0gVhyJmIJDYx7ad6NFh3eBBojIKjQScsgkDC46rXnK8kCvTlifyP3tDvddYlCmqAswQAGuwDGjDv7xAtLYZLnt6saFFMkBirfbd2XmfG"
"SnjRaJQ6lSqObAZuP11snx0DWiIodAdq4xtVkpRoiIroTUxSgkOB0J2LI73TfR7wQxIwbPHKMQOGpFskPoqqS0yqi4qoTUqsBc4VANBgP9DA46fXbv044GB7"
"89KXFOGeYWjTHt7c30ugJSnwV0pw+sSG2834IzXaeP8H614NNrr4I3rgnCMU7kBNXr8wMVAUPL1WMWi8tf72u/veYKaIi6PR9JLUbLF38yXvmiCLiqK7u43d"
"upM0xumu13feZKeIJKGYDE5YFtafA+IZ0G3oosOf/v8g0Rj2+WRnHbEimMHwQgT667rSDMLYjMp6HmRxZ20LUI+2gFiYk7MNPXvkFwX9OC2ZcJp5vUAxFrIr"
"/ryNZG7g4nOr8jPHBbmpk7qigkxlDIMRp4Dm58BmtC3HyYUDF/rzPqvsET95s3K47qywdDH+XRVC4/2Uen984Tf+Lifw+ZXay/WKfy4xb87cOkaup+FO6gtR"
"M40CFr0ihkgJKoyUWnRqd67zKgfuNaofH1us+fre/Ibhz/V1f10Z+qELdnh2CT7k9V+QwVHhCxyOOTachoplkYact/oMx3i4YmSSBgmL1YhRRKwmHSUIihcW"
"0KuGqIrGRSVtQ6rmXdgubRvcqS942Ipshj7tPfdKst8J3Ku0FdXEpyrox3IolDQYpiAzHUVDRmUfQRz6wSctV5yMYGjh0AoyCV3fTGRxWoiofnG8LH5GUTqb"
"orEDOg6bpgGHW6aDjPvL9hC22ffY48sduh0AW+g2Vfn7x41j458MsNWUzmTlHx83GI2f9AfLre1uCsT5TmWxpEMZuirc5MjzHaUbVDopcBrZAtTW9ru6jkX7"
"14FjguFCLqq6Fw+9Cey+GhjYOYsXTD+bA8a/uLhMFnyQ0iTQ1tv+aprUWSgH+w7vAOrRylMnl2lBvFen+2Kl7FO7MXW1/Jvtrbsu3jr9XKNCXHuPEwqjr8SL"
"DXa554E3+RJFOhGkyQ2qLk07936jOwp1+rSyha3ZlrVYm+2xfT9ocT2kZOlxllgqAfYPE0g5mI42WiG3lVbackg0dEeKcTjtm7buKVJ0AFnHJSWh4o7QokmQ"
"ADoaYqKydISjsyYyvZxIZw+obqNhnSfINjpGJ0PIGLKSTtUW6/uJfnClA2UxnOXQbUgqLCUFCoNEr70WTQMSKRarKhpQChS4oZnkbYJbpufX0uDsj3HX0PKA"
"nzY8dGRERx0tV5IZ8uppiI5K+ajMziSUDkxx0DLS5dQHSEZSvjH/efF0RuAumAHUYbu7K4B6lOq07jIv+eNqdUfnVq0s8tyUzZT0m53a7rarfZKnmvTzDcpp"
"egquufaCczGOLo15GTWWpG8C4/EdkGeqJ4gzYZ8R6RA+UlpSIc3v4oLifRZq5cltpvbKvbdVLOyopuXvIquGa+23PlQ3YR1wkIBOI5GqcPD4929Rwm4g+AZM"
"E6uawJjlwFk4MLANPZ+7D2wtbPbM1aJgVQfIGc2oD4eby51TQEysX2gAUp4id650XyNxKwtyi/rO51Y3X2ALjJxthK6ilMSNDseEhlbLo8SurT6nc3Kq83jF"
"Axep9f23xOKTrJ09BEq+NjCVKAlJRAH4OJM0aBN7I2je3dkIggMNsqBfSw8ElWd93TgD9l6oI7rAm0e0r3Y6gb3QhcXZWeAM3WoC7r9uCKfs7//PGq52Ou6W"
"5GETZaORjcutZGaOCPJSxsu5LFIrnlG1Dy0AjsP6rfMM+bELPXkfV3VLPn+jbLjiLF97MvWWJG8iI8T40s4AnB8MO0/Z161myFUxw7lLwwPjAlxxZpISJS4z"
"6JDaFAwnJV5Uza4Yowqb5pQUU7Gad84sazPs1vDv1DDas54NbvwihiPxfq4tVkVtKfBylo5JZ2Rl4WnpmWg8MgLlP+Q1HQ7Nl2dgG+uMKhJa34xmcTuJ+F5+"
"sjTllFoznwZMS4J+i5oO3h8F0gL+kZjwacvQSRKtoHmGKK8cxdK1fu2Z28iQ4jwoNp+dh073kQbJfWonn/dl9V3M25ZxsVnRbsk3QFsa2jqu+Njg6huHG+n0"
"gwKhfbi+W/u3E5PdaH5uodcp/XObSE0ivF4J3LXxrYJ6WKYaw0jWqFJzyRXpcBVxW6XafEvR3nqrsMTcmRxXK1csIyunQ1XTPZvZ0pLw5uzOqst3ha28hcQG"
"CWZUsE2Ea2ozmY3H2jrZBF13CheUFpQPGoza8sYhla1Zp7zcuDb48791a+Wvx8niE7oJIWsQdq2Jy5mXl9GHF7CN6YC3oqauSkLJUfBUcFFoKj5+hyPt36JV"
"mT9fPIWkt57N7ZmKlE2dNNPxHVSlyniVUs0cCy2nI2ro24lQqjwSq64vYhX3s/Xg0FsMHDY9JvJvlC8+NmIKSShqSqS2nNSUJsLy9Qn5qiCyStQAySiAla5V"
"tRA6Y5jFtKkoSjocjc2IzWIk4IH9nITAX2MjJEbjdtsnjnkCfmewMlXRMqYXNOKz2vK2axRv319w/M7TG325uDM1K3hq2wIIA2NHdet3F/CwbJYdLjguNleY"
"RKJUZ6C0tG1N+fZtVWv7hQLOGBcGqWaXw6BqIkg3GY1FLacnNVJaazpVJRe2VlUV7UgkSzeEvdvvbY3gEO2aNjQO4wpWljfmKv1sERnxeUdi/k0joAmp01hM"
"eRua2z5WX0ItbsqjrA0C+3NexT4HcIv5C7Yfuu7xKdFigPbldtPxEzcufrizfUmQImeHxlV0sdInQGfHiBH062bHIxKkTalGNh6ZR8nMQGdklmDY1LrAMC5H"
"JxfUj0b7Eyyzw/MQcGIGghiupIUshohTexlCBiWVCCHVMJkJyoCviH3wVGMiQbjKkI/bDX2FvWzg4vJZI/Q/gUEYKwkGP86idb7QulpVvm8lJwWHkqOzdUv3"
"KNquo5337wn8nz/FQYJlQPGXQZxISLE8k7BKnMF3xMZVbFRitN8a8Sc/4mVmuFeET2fRjlwO8+67dInzRW170fVzdT+nD/RYx0ljIhckkB8iNaefkNXHdXz2"
"8mrfjyWOmFUbELkjVm0Ds95k662yLz1naJbrKBmjtYO7XQ3Z5Kkh6uaxnLYseE7eciSI9tgNfPXaHO9FXuDrAcL5ZH9+4A6zOvf1a/WnAkDhKpBQhUmmBmLL"
"Kfxy0Dkn7bKS4DLqAF+Jqndg2Yjjkj25T5OZBPVeqt29BedUqIejgqCRc1R5Xrlpl1aQXrisbyvFWJNZuXhiU0ZcV7JDrnmzp7OCZBGfyafK9ltOfXiPHqyz"
"BBu+80yBCf/vgwH1co8eO1bitnP8BTn7ZIlCuHy2qCz3NGZ1cO2z/LGgDJJzNvFrRLRXy9Fo8zNRBb2p9736T6JK2uebS2uTAGyN+kpoBmSopGfL9lMzctuJ"
"8uc/YMo0wZKJVwFV+5dkKfiJsKyVqOhpENY/M3jqKHnERmyztBpKihf2lnGSW6MJkdlDrIzlDIrMLFOtihNdufFskz7j2Mms7aJu8Y21mk9Tq3XfxoiiohaY"
"AR9HlWpfz9KPNPHJY0uKw8mXHY44AGy+7vny/4MU8wlmVdMkFWOs0d+qKXn+XdnOmWdNVbbAXxCeAcci3Knuqiis36tQQQIRHQmB5mXFKmkIln2xPOaU9Ztq"
"+r6ZvezVPdn7iy3lSO2hCS9WcrgfO6g0piooTdpOy+5l1fAvzSvk5OoUjhIzDral04sopSUCJjQ+mpMnfDNdxIfz8zjl2po3dK1vr6yVLVM5wxIcrLOy69Rg"
"EU3WCdxMp+VRr7NP9HJ41DpCbm3kMH5ya/x8sNCBbSdpm/miZNwHOv0g1Hurz5f5hkSYgYvyLsXS4DHYyPh838a9OasF1kD148QwekM1c5xBCq8oRhQuQVkQ"
"ZlZ3FY0ZRXNsTRkbLfNKKJ4laNsz1oRZXsIMlCQQfyouzx+Cz29DnQ+VN30yIObA5h7eQyn98voK+AkXPXIjGJFu/iQIPnsFmw24SY6+zEhqhFtAQUoOcReX"
"bfFnqDPYfEzC+8ueTaVkt2MyvKs+fwIB4ojTtvRDlMh0P0UOkoIXHUUX+FA5wYUlk8mFPqNs+4Jhccbqgo1wXXk4pGtCl/VmQ8AeFEinf1laUx93t8rfMtv/"
"tC3tYG4k1EWWAI+N6vKS7+Hu0Z3YD5bn+Ajux4B3grEe3ugwtS+Z6CtxwZ6Jw6cPlxUiAyA1yb6Q6iTgeT42i2wFtz7QcWhPY9H+bduLVuKLngvu2I1x+3ib"
"0VMM1xVvAXvFi+J6nugy7Q32rYbajTxe1WN0k4xIquyL4oNMd7cO9w5iK7/0C4M1LbG60/rRx8CUaR9iV9hawSIyUnZR3AAj+4tJjG5VP/I41A4YOTnjWe5d"
"seH+ajwDloJPgvBday3RVXzbyNVQsc2Hey1GLLf3XLQgrlri913ce8PynAXScjj0KfNEL704p57ArInoR+VCdFx0ry+QnDjyCilLO3ITBWDFj7zmctiznZmO"
"Cw6kMxG2BJeJFBKN0XR0+ADy2eBH7gOZOFV+lLPw7wnfXt8WVD33zCW5J+QyGLOdkrqXE5ZfLvoi/MQi/KLN4jeWdDovmjPA0Lib3QV84T6u/XegYdmIPPti"
"Fdpp3J5yJsIG7zaUXYDWik6agTMZupcZU6HUNPDNfP5ZXCJ5hoBQHMW7mV1guRGQ3Cy774BqHzQtNlQX+nHBxqBURqvBM/QRx7A9y71nMrkOmZWs5XfyV1rp"
"c/nzLbYgGhl1REkgZSRiUqM5nirGJxx7YMLXXfXDX5+qMOss2lPfEXe3ocjU2g72hJm4ZNxdmX1y+hnJHj9tTeKmozvaH37BYnDh8QV+Pft4wEb0ZUm4jaB2"
"1KaoBeKfbVn5lm0cNj7CLe8lKFCtBRArSnLTIIieaDTrAfdqnnJSs1NhURQy2l+BJ2XW/RItZfxQYD/Uy0nP7/aoyDN86D6USTDheS48syP3glwldF3013Dj"
"zCOaiAespV5hkcrfP8T0LvesQEV2IBMPhjJTocxwSIyATg6ap1MSedFRKUx6iuOjtvHRJ9dPvb/brtdrmOg55ddiHcwzC9ySvqT1pbL8OIlZqQ1F8gYYGlEH"
"EyhS6zITfHkprPov6RZUkLYfPHXiFEWiZHkXPmHZL3Fqzer27SNabgK3G1GMNBKv8HtWepaXJ4wtF5bf7QE8Bz5G0blNudRJpCXUu9fcG9ZlfdKX6HWtnvZr"
"tXHtP73/Axj7v8eox2vJcIO6sUc7IkPG6P5bv3j050hMWOAR1J9rZdw6nqQoXZyIxOKnvZv/Ucwp5AW5BFRU7/3mvMsYqt23XBrzc3Rjf2wBOh/QyW1tZDpd"
"2ZJLP44aaWmH3LTt/wAA4hmNaYzQxORJnDpFUD5flmcWP5QeIfrlUB5MJQveRO6hPFn9Itmidl+X1fBhCp7wNbEC0emGcHNdXWh1R2eqXFCRgchkXTEWWImK"
"jtpR5+xQHlfs7K4+6fninuiIz/UX5XNOQbcDgzlpQh1vMUQx71ydeo14IhY5yYDkEX52rPFiF8xW149hWWxpGbClMJBv9/+uVihUo99SvlYRE/JSA96JEdhL"
"ah0l0YLagISMAqLkEBpV4BvZ1r+01FuaBIruvT1xYIesaHuP/1biVfbKyEJLPNM3CwU+IeZyrthSt46nY2Z23Z9/KSdGnjyIDkaG/WsfY1l4oNGXHB7olwpT"
"lqARKbSQ3KSpZF0Yrt5cLbiiSIsUs2idUJAZY153ZIhfzDp/6Wtoa6ntNfxf8Jq8jAE6ZwCXTTPVXBrTwUaXtRfe0kTNuUM0Zu7YbI+iXc/oB2J6GG+b+5Dm"
"w/5Xc93DWGTuTd+XfWnzS8qGThwqRsFw39ruIV6NLdp44rhd4z9zbVicJurfsKl9GKW091H+mqFipkYGvqueCR+XzN/JPE420yQGDyxffkhjI1x9Dpm+1Zgt"
"f5ACCTqK0FRlVF3tV1iNqsTVH5yweF6AnJat0DKKPp0zwVR5gU0DjCAeqzzK9MEw9FJZlaC5ELGZVUVhG0a6/hu4PQWrdvx1c86Zi3LPwcS9FXnz/mH10EfN"
"IXRWdhbuu8e1jFbOSmKmfwUDXTHZwNOVki04CJHZk1EdQ2zYyi/kEq4nNkerx3mDVkOWlobBiUjN6dXetU4XImGeLzOVyeykHbLOPEBPiQi5FeScF0e1fj9W"
"vgcV4yD54c+yf/OgYpQFCAjLgW2FOBKkDQg7tD/XuW3+qNkvj5VrlZcrPuGnag+7yW/QZLSQOq+N9zPaM8btKHRdZ4bTbxnwPHlUGqkAAQ8LxxYge5H8APcF"
"P69Uy/2h5ojkn3zgB4qyO5c782zKYWmkKHMEiuXxUUMIthhasmn2vX7s47dClxF4Tzk2BRN783S5OYClWSOCix/vvPV4oHxJ5Z0x5inywGfxC406Poa5u9WE"
"t4wE/EDkeyi/Nfca2Y4VzFY3EWCQ4U28QszfvAxVZAmkeRQYs5vTF7YwbsthK62I5RN3wxxg0UDr2Ct48gOrrFcEluOwPn/lQafxW/zyWT6W7/avyPYuKBl7"
"I0VoYJ0ega/xngV4LLloeBzziNuAkCVPYbSBw2f2zng24SxxFt9VsYYKryS5iAM6E8fS0R71qe+IdnA3wsoBNSwC+bOaFfNiPwj+S1WKsSnkK89FdaaZk3lt"
"jujk/TBfbcAsWDM5nUybDquOl1rf3vRSWaOVC0frCc9wGEJRECXcHB4BD0dqoxSPXY5jTQO/uNKBvZc5GMdDueQk3puP2zh06lLg15tlr2ybhwsD/x/LcE4t"
"B568K79ISQnDeRB7iDJvsy9bJNjHs/tiwYmyl9Q9an8BLC5uFu/OFK52MJ8M3K2AVbIv58wVEiOepwjN5ZoLOwbJ0bhAXC0ZrPOWVYI+rZAZKYnONV5K163i"
"aVF18/EN3mafDOpebCAtSANgM4+PmUZyyU4fB9Hpygyjk4QZta1HPBWVjivvAc0TMVH8vXDb7xacfPfTYjT8it5QsGlXNYBNJb/UIq4cNcx7MiZ8Oj2M4RCV"
"6Gju0X3MvhkUbHKsZMSUJkRNJVk2aTOjzL8UNv85H/LUjGRon6+k9ixaXisCiXAyVEwBHKF5e8W8f2a1gSK8bF42JQ+25R1WzPvd52zBNuEf5VXboxdtce7/"
"iCYwyQQAMmJammFUnPuCrXKytZmjK8cPFc4Mv28jO6e5E8DwDV4o8LngnTdwFc/r4iTGCAo5jSxKZDHGjN10F6eb0ztv+eXEk/Lz8oRYfgTVLSwvuzdq1p1E"
"nnXvjWJmh7mFk1CcHF4OE/huxptLpHcdHgZ+lVwTZF4QHXM4YEPIhGvwsOxCeXxGcks0IRI+zMpYyaDINsWqM4ok79IYASgZG71ow6+rCUZrsYk8TeWZFUqA"
"F1A4NLrPx/rDZZYBJ8zT6DWXVUh2kCj1XqzsuHkyvu4AysFnYCkKB14QQhByAbC58da4/8Fhw442/2IeThM4smkZ+pYC6KPzdbzN3vD9EnkVv5kDdB+bbqLb"
"+hAP+5cIAfneZt9qepn6jeX/B6nmE8zKuW3w2c/pnOvnHtw7uHxZktNRBaw2cf7aXDiDYc4tkATAEWAgCiEPIPia/Ul5sMAy3NpyGvxBIbDwqtT8vZ60s+ZJ"
"RsdzC5PbY/cSpMVf3Yrqsv1KKuIzvMxekfiEeBYoN2BLWqU4LIHgg/eYeO+9T8ZQGQ0uiBP1UnFGpYp1bk6uKqwl5lWjOzHVbCIixByJEwRQEDNvakjlwbU6"
"/JDmEjGJXPf/XQsQwhrsZEF1AszL7BmBiYpn2cCcuAUjg3myd18X33vwRrt1Ps5OtMg84mwpdpsEh89tyPydHJAHqApebiw1NIYMjQwhkrPe+DYVGUF5vYsd"
"vsGe5FYWInOD+fWs6tDig+e/htGPCDOZvL7JPBb/JFUwIoV4slNyHphF++C5ZmHJdzG5fv83/Y+7oLYenVvH5nWeNRweP5cHnD91USvfXJlVp9RUudm75h2p"
"hhNEAYij5iPy6czimvid2hG2NEP7AWs2r2EId47Dh/VXUZlFnZqcneBPFI534x7GZeIw+bThrgKGM89jLkaPRgKH8ckW57KJe4Vzd2d7+067CVqHPhSPet9L"
"2bvFXPNDl7Iuc3VaeJVRgfO9LxkTnv8Jp6wk9sJ92p6HmSNspg948mocnYc5dERMeJn4IW70hFlGWFcm2o97C6ehQRrndWTP6Jk/GRr3USVFz/I47eMpncbP"
"/c9Tm3pRD8deLX/uC18oz8U7Ayd95Ec4HzkGszyBwc7J0jCOYe86loO4a2ERzL33V88x94DIi0jbxl8972EDPyWYlck0FyJRqU8tCDEHEgvyQs40BNN60YH3"
"Fxxbhi67M6C1NuehTk5G55O17q1QMH3EQajXu5kcznVVHnaG2t5vtd+sc5qDwp7U2R6t23Ka41IJUIc9Ct87FY7xPW5rQPjJ8oeul10fCVyURx/+OaJh/X3C"
"LcVRvgu4NN3x/lunDQK//FZSSMiRJ+BdEVb3QeuMgAM8KfGI8NDeSYchLOR6K8L9KwpIizEanYwQoV/kjQuazkjtRgV9/Mk4CNlB/OzFdI/b1Yaeu9Uajxef"
"fZjhdKe8x3CnXO20/Z3XXMYtfR4rvzov89ac12zWW3pOPruKk/4WGNVMdb7eevC7NuPID20dB68732g/8H3biPFZW9uBrcfRvTmXR/li4Qifcbk3upexOyYQ"
"ikcE9F3gt3FibU3y5GJpY/3lUumTtbX9X/2+K7tUIvliu+KrhYpZnbpqduGrBf2yWqdbBv9d6lmdmW1tOzW72nN6Zq6t7eTcFHxBhC6Ew9GFogURhovIRkUZ"
"I2A5Qy5AH4aWuTrwBJkARrUVwJi22AdAaWsZAIySAlCkGQnQGabSKOxOXrEwZ4/N/LVZFVf8As/NdSex7R65Npu6KcsjzPXdtduDuXVm4+pwZ/x7bPv23b1/"
"pTqGxn4DudlSsFUtfuMPS5EN9q+avevSLcMjoIRl+zyHUnh2zj5+dj4y330eoBXicRyp6gdpn07zvMxYW36xFN5Pbi6/XIqch6KduzrehnPX7UTnXO6mXTMZ"
"4t9KYWncgeYnpEQNC3+GlaFxaDxYkkGXjqWIPPlvOX93LFx7qtWURtTBfz/zXyUL7n2dPfCe+hFbogZ/FvzZqmfRzp+veq4WLzxCrAwmOcaIjjgjXdcGD1Yb"
"r2QIv5/x6rCKefg1RyTHbySF/Fua1ffFlDh+zyglL9WOE2VzzCzOW2ItjKynqYbdFlPkzSyc7ZeUKrJSd8fUKw0KY4/Xuem66ZNh4M86YG6I6pnfM5Jr73vV"
"YpjnxW3Cs2f23zQyWY/h1ijZBf13Rv0UnCoMGEXeKP5eNQ0UXONpzcscvntC2WGUG6Zmbf0xeXmDGUHSoGSF+oX2cWpD2idOzcf5kwAqc7K0JT1y0+24l4Dm"
"DJBWxFBuZqHH5NDV8D1m5CnMH/p8S/9co+bneGLbIMK07yFvCgu3WEwLSPTYakca/5BGWm7rG/Pj7OMW1emQeA3Fo0Kzte9JqbetIBDQJjaBVp4Y//2C+dde"
"Wgr4CeR+P1jzFS789EZqwAzEa/6bfD7w+SczcOz/3PTxlbXHDPi0VAwA56RhVnYv5UMti95kZoHA3Qr4X+VAYPrP0yLHN3pTe5i9mEmcXrzSOAfbT2e+2F1Z"
"6ojKwWntXsYsljPMSYYB28DO2i0euDD1pPmxAfNgPDotDbOte+gcarl2g2UF2qn/Ym3s/judB/DVMoM5FD0vGqT/f7oOTYBjgZKTc0kAE3vjHFkO+/8k/j8x"
"7hBRFngnlm1mG57hw/FkZB9I//+vr2H7Id9RngMQ93as55cD3XIIyAf6GrdaoEVItk99gEgkqE2paqUusPma9Qgr5X3eZE9OQfzFyy3FvS39uj7R4gAVNv+6"
"hAvt7FJQAJAP6uufADsqWBS4v1kf3gN+GgrwS0jxK+wvPVBvetTDLVSnf7DhOj5yeRRCYlDn2XibntlYOsHeqMEat+v28Ap6qnMBtkcTTRB8FdI9oFrmwqPi"
"WrGXcELnoHU4WYfc4gWAHV89CL4t6dmnnrjcOgWCGgj28D1u0cezOpEuPv9a4KjX3A0f/u+RktMe+reqhwf09nPCHNDt+5dbSzoIakxA0E0xCDHSed71SR9N"
"BFN1UQsB9EPkt4DuITqaT26vfk9/tZ849XdLWP6w3852ECm1YwP6OwnxnSuwpf4gRMR144CZPK6D7VVfrfvsy3mtc3Ohvurgfp6sRIEPvdZUQu6dcXJo5Z2l"
"riG/SfcvhE+/lFBffPa3y13a+G8H3pbZlx2k+G8ZEtMwyu7F61qjRuYkCIB8JF4lO82TndR+BuB/f46d8YwGDIWBabIttUmVR72sGjtN+hCGMZypnIFZO3vn"
"3NyeF/NrBUT45qxwW7Ztz+y1/RfboFOUwRzO42IqsBVtaWN7eqavumr356SxkbpiKHVo3NDcoVr5cfkt2ek51xsSLxouU+9XX1LfUT9Xf+u97Hnms0aavY94"
"n/X+0XfNV6xGXk4+ZXT6qNJ/ZEDP3DTWqj+qP6OPDmrZjezp2QuyV4/njs8fXz6+Mzg+RPGTqLsncieqzLfMvUPvDF3p7vUia5k+i76IvnqyLHx0+Nzw7aCH"
"OcxcxjxzasGULHpP9IeR985bcC7LXTW9fHp3fHDe3na84rMj9ujMT8z8+szvz5xMvpP8Y8PZjc38ePb47L81vttoj/Ic4iLu+Nxn7bvtJ+0X7Pfsb0afGX1l"
"U74gFcqFu+c/lb49/UD6cUc8A/zswqfdr8eu7UnUYv7HF3ezz2d/3Pz05r29rCALLeH+pc/m385/kv/W+aTzRedbzlPOy/ss0RePLZ9Y/u3lP1n+wZYnt/KS"
"Jm2s3LPyN+XHyy+U3yz/tY2c44px8RdX/6X6fvX/9Zv/1Ha+EM8cz0rPBs9pz0ueP3n+6xXixfRSenV53fIG3gHe4d5K73LvCz4HfJJ9dD6rPps+j3ye+Dzz"
"3etr5evo6+l7xDfMN8Y3xbfUd8Z3ydfk+63vCz8/vxC/aD+EH8GP7lfgx/cr9lP5VfpN+d3ye+7v7J/mL/E/5i/i5/3740D/2T2/Eo763T2fH/hJY78df2L8"
"hX2J1cxq3pCMZeMn9+4Mfjac/i/7rpyS4cq+H+zax/e/sP/N/ZdPXTstVNvVO/f//v6/H/7J8G8PfHgwveGZ9x34qQOfGfn6yP2HUpu5TSH8U3Di4MnRH4z+"
"8tDDh5469MnZg3Mlq2b9waHPjcXncPZ36J7Qw6GFoUOh74V+HPok9FlYQBglrC2sL2w0bCrsebh7OCOcEy4OV4aXh9eFt4YbwkfCJ8Ovh/8Y4RkRFUGI6I24"
"FPEiMiiSFymN1ERWRy5FvoyKjFJEjUe9FfVDtFO0V/TR6MjoxGh+9HD0bPSV6Peif4LshUAgSRABpAOyAtmAgUYEmMAYABDYip59YnvD4x984Ftv/f4b9NQJ"
"+Mwu+EzXjr//mD1Ci/lHzp7+Cj/w/L6PHxFPF7cVsQ2n6V57mnp9uPa3SBe0TryudbAanZ4Zb2S4koSmFlhJlj9/7w/Hjk1R52+iVffhe57vp77+4KuFrKBI"
"LFxqcQFckz35e2Ie6kU5TqoCP2XZDYQOoXP7zB8hCThS7MxXb1r41SRbFlL5Pk5sO2KNfNWpoDQmf2oV17bRm8Sav2ydSVoamDn8mSFq/EjjgVq3g8XGluj/"
"Y/2yQyCn00ccCSHIChs3TaepfIBdnduTBii8VjrkqnaFYbDkr1jrwRi+SdiNnvWQWE4DNYe/WbWhA9PtN6PQYmMNegfvk/FGZI474pgqRVMrTK7FF7hrC3nz"
"elIKMCEPQpXqtoFDUlKRrRzVyUMDyir88iuUYa9thqiicBCFPIAImQeyIFqY+2esAkIyJEnpkzQqy87cqcdyiqEV4KrAnf0bQkJlHI06IixBaehfey1XspYO"
"hy1F/uA/BKcg403IVJo6gTmdEKcr+GsOHdojuaxUBWDvzf8fp5/nOwDKtmtZhmldAVfUgzc5XRDIZKTa93bYkTHuG/rG6qJ63Bn+8AvlAhD0e+iwbQvioy4d"
"+SCdjgXo2uxdecJpkjRB+/xtNi22/eYI6ZPDN7CJ3jf+uRopRbfaCY8Q8/3vP/hlwSjB6vnZmARhJLVmpumJ+tI0i6p9wawlRQ1kd2nn3qSBIBwFyEm8ExCg"
"4TSFsdgY0G58+z10qdE41T/88Xmo+ENStjmDsM0Thgg/JWP7zUMq2bg6T/83/rk6n269eVDOBNcO2Bn/smBWw9CgVUknkkzyXK+iHe62UO37CGqQ8TkLlRWR"
"6oTdCwsN/jRs/SqXYT+Q0lACkGYJTWeR6RnholVqxh1vS755Rj5gRtCD+kCtTDxSILHIxRo+Il72UBA3ydiMwC4aesB79qw1BMkCq1fp92I2buDAauOoUaXK"
"ymitWwX7rLJP8nNYO8f71+/Yjv/9Hd4fP5nrnOjNtu2f+wM+AeLQt4Dh15nv06Dhogtt1zSSFiI8cNh3WT/TqsCRsiheOZbhDpZ9gAkhoD4gha87c0Vb5qGY"
"vAjzCseQS1O7kqchKcsXBnF68H0ZEXJnG1nh0bplLa85VNrDZyBn0Z/yrScCzW/GH70pWWwCjVt32e3fdH8Zufd3q3dsKNjr6ytX+LwRymLHTTMhMJj6MAbl"
"2V87ni/17IMykOuy8KrttHFw7/13uvnGEELe+bPunnwg2BtFvX4nST4r4neqmNjb5HTZs02aHCXMWNgtJqDj/+t7cSeIeGilq7bDkedB3foj5t25n7rocIvb"
"Efkti9D0/3oH73hhYW9eZ4/jxzELkxKTMIaZ34f7yllZbzrtklbx+WCRhO3gVPj4ONvkDdcLFA/OTEMb605aeop0C5bh+9PzSe2wLWfz4X/CrpNe7opyW3Gw"
"zs91f1p0uPnZQP5fDD/KPa/cBy7p3qEkOl3rbn5xrvh/irPveDAL9jbQJyqQzKnLx1nU9E1YUir+d94iV2cTKyKJfr38x0DWEBQAQv/hlDtt92CGfpFRHDqe"
"NqeV0qcMMEA9Q9wRLS7zikAbKAycLzTTNKyk4DFfNCa84tgY1UWlMTgrG7uRCSuUNGoEeiYZMwlWIhjNnFcRDEnGsTpKGsIqMq1N6dZMd+DMbcSB3ZPalsEj"
"Jcz1USA6Zl9MEgFJTFOpBqpe/bxpWbYV4qNNlAuJfeQMSIZs2FFYCEzLw0qFqaw27pZ/WupnrfLAYD0v64wE3sWqvd5KJOrOwq9oCsk2LlybFrjEg7/u90z8"
"NEuEa6XYuFFgITTA1Vhvb3U4RhLxdvFU4CerrqMpulZr+vdqMmhqZSRoMH9d2bYl01tPe3rcLXGOQlf0C4gqfnXV6I9TycYqje1Vpt1SKzUFAf2P2JaxaxuJ"
"MNrFcQ6LVQNDQYUL74YuGVOZYSoTqd5h9pLzdVO0xJrD+37BKDLZ+90FkqE6U9vztQSqYhUG74Uu1FRiTEViaihmL6DvmlIma/aO+XvTu17SL3u6esVV1jkb"
"t5yASFO0m86i1Rqm3V/dTjL32gJCZMk5RW+eCd1mWdhZdjYqnSq6HIlsxyf7cao33dQ1Ki3LBzQo3XTKfpigN10naPX6cpDIknMM35xdudmysDN1NxpkRDn9"
"D92Nz2RkJZxOduXK8klQhjvHl1EDR/ogOrsoq1a7/WiIXA/C4xGkg+JmPWAdWBKInLFXjtHK9sebengZuLRVTNkczSo1hUXGzLS/joywFnI7oaECWlehAeIZ"
"BoomOHkZSg+pGp4OJ/Ktg7s7whuik1nL+s5irhcdhdTUhFs2FE3IU3h6vukXI1KEFXo3DGzaxsOYy3OCYk7DlLFMrAeMSlhqplKrWWWQ1YRRsOQBzWnHPHbb"
"2PWd7//17163f20bn6tzjGvfu+bq5eSXRYyHiXvGQhqVTJ4WxXNNwJ2Z5B+XCwjqrtkCISititverDXa5PKaJTIT0QVMODCpoHJmiuWIp8sUNdP1eAA47e5z"
"IVfYVr41WDOfEvZI1Mlb798x/6WbJNk4Ek9ghOJufRF8vDj9f3AZbubY2b/B996f1ij3Tu0c9s3hsivAWZ+tfHWtNfPzc/mHZWDAXen7PVYj93TFr9feM3t8"
"R06UBZ8Ukdh05kdPxe/DB452wMCzO/6vL9f8UXnZqX/u6uc7jroD5MfUke4dvFJ+J/vD20uPpppqHJw4G79kdmADg6vv3nxd7n3NyX3h+q8mwOm94+oQPxEh"
"hrC/bLswJFan7xg+sHB39qvqzrfnyKW8A1pwAE88F4SAevSNf3/Q2tr4v4Ob3p5ZBTnX9ijFo2vyq2aetvDItRv2u1QZbR7GD8Heo6c6Avic9ddr+vkTK4VT"
"xtXGTVlQDftT6oQOWvo28BB4hMjxKAANsH5j/+6Rn6LxDzdsiZs+BWr4Dstv6Z2Nn4bjPxwh134OiuEHGeeyN7dOyjAmhsOX8Jxakd08cEVF+cpDR0+fjtQv"
"QVe5Pvb/8tP3X0zNxTCKRGGCPLhg8OJLawjRUFGW69CV04Ic+MRAaPlm8jGksF7trKXj4/8wwRJCGlzYDKAtGO3+P11Tls22T+M4eiJJFE1G4UVzFaJD/PDU"
"jCZLH9G0KIrXtE2ipGCndkTgWDQ4ArC58PF2Tv5ZnoVJCXlZnLiszYsMWuMNXhA5ikkXHCDXiOY+M5K8Wo27KKmlkTBPl+U0tkrUeYNx+YsaLUqqlKybXmxp"
"WEe/LHVLY1K32/YhSrHorbphu+n6bpkCuMUVQ1vjaQ97jlSqVsI4dqKQiCI0iZNSZTovH6a5giyVRUuR1UwqFlYtnPDWBCd289w1Qik6S2bEthBwqk3gfbt6"
"a9Su78o5zEh7HE3h1eiWPgb4/K5duhW33V09u3C8i8eOBe1B2bB3ZeAdJB0/0bGBXs3D6lm2aBuoOdrA2hVhzST1073FiOW9uQsLnZGtmue1cTKgtyCwQ7yf"
"fB+ZLSqtfT+dikVSWaZoyaCt/rAgqVDkVhOI+5ePwDfrHRUmeyhVchyFkqwM7UCQpZJpD2tMEPjVYP9uO+761Hl2OM6MtHIJO0PUS7Xe60Zlzo+q8TDPImOl"
"uVGlxdDvH8SYYTYLApfF6MdHFEkyhrSMmxPnKaK0NvbgGpt+9BunVCX0+KKusGmJWNqM9xzQPGFYQbKwdvlFnRCwFzGJLQskFj39SVtOWj6shehK8fYWRjOC"
"mbR7nZMjeLInTkwiesZVDQIWoW1iVTc51YT3PrqAzuTZusjIU+0nDzXl93eOFlZLhtcNw65nlKChfpHWBlU7sxbNKUUbbLL/2wxYCGtQwzRN6VBUVOmMYAgn"
"tUW7kXks3nAsfKMfrz4ize5OPUz/wx5zgHbmgqnG39agkonQxi9y6z6fiJGErEcn4lwi/pOdm6wK8nbyFuj6X8//sybCGoRsUWOTUeM0FT0vwaOx6MB1jEk1"
"QgChJG0vVXQjm7jtOI4I31xi4h/TnBl6bk5L9ArWBV5Md+GKpt2Q2mLo51vdTVTAMabl8q5YwSJDujS3sA51QeOjw5vLf6/3h4feAFiexP+y2/LBQz/cmRGl"
"jvlxiBz9sY+XIaFJPteTSyM5Z8wv3etjDXttIzBlFsvJ60yp/hKBioQ0RgaZiGE0FHDfrHNjsG4k/tpW+E/kbGT5PATfz67Zsd+eou17a94j1hPgdWYFuM3d"
"cCoAeDPUjZZVjkjNcPpRkJiePYt7XB6vL86EYDBUe8cTe5d/IKiSBI2FkNBaDWJ0hhQesuEidz1OuH75ZXoJVRpefAf4B3KOqcAI0YuEUZk9aXcHZpIAoWoQ"
"/iAS9s4nH9OJQ6/vtyo8L8uFwrc9EdiPJ0jNGo29Whg+vlIYghMGQftww6+13FAVsDjG8BkfF6GS2PEAic0dkUshpMuHESUV1bVMhU42RsVfETnMW5BBSyty"
"BMXCYNOV5Vq7PyLfVkRA1fRyfYHih5s+mvrI6NhzCUgYkKhKUEowKyZ7SSaUNJQN7Gr7/KdgBNX68+Hx0LyeVNOUrBgs5qkd1ARFWVgZFH18u3uRow+2Q89R"
"Hai07thUal0RPhPt4dFIwxRpPErVrNGwBhvf9aI1a2TK0egH6NG/3ktdS6LxEKGxsPFN90Ek2+R9o7uIWCvT+Qpiw9xRAircUJBwuwyxuZc6yPhyD6jFXYpc"
"u8bywqNzJjQcKzI2gg1oveMb5oUDObHvtbdBhbsnSQkN1xiOdSyZxkNRT+FD50EEn8rQyQISUbSn7bA9nLMCsmVN3Kl7mJ6+dcGUjo5YVUCP40UH8OCGzRjn"
"4XqvPp2XbgjxdBABY0w4K6Dg3odijdgeI6TS8RigDfR7GShs74eFJrEmWi6VTwAVEk2Fk8qiBEkZeikB6ekS3KqlrBUAif78sClhHcf3hyMd4PC1k+TwCRAS"
"WuoQJvZ3D49e78xFm/auh3EOXf6ad2jRe+ngKYAgNf9GtfO1Yd0av7itE+9RsCayBna0C8OpkwJAaDKOCJef/4IMXDfiqqxapv/Qir7/Fg14ta1bE4FUn4At"
"GGwQOH5dRDBmF+NYgJa1nSL/NvVJpMLCnKWu89eXLlLDQTs0QzWrQS+tqaurrPTzywa7cBgzmQAGk3S4Eov7z0qsLC23zOccvXbfGKqS7Q5PZFQ/s0iNAlNw"
"BA5DN6yCZuo4VsdkwjeOVKv9/J7Lv/yxmXvA2Wv30qer3v0SDLyaLGQd8WTIqBsBp6GOzaenY2fOxro6JSHnhlaGu6vJ+KF/FSIohiPjMqlZdAaDTP7tt2wY"
"BgURGQnKZE21zXiEZ7S2333Dzs7d38X+kP28rsbi148PLvx/KTDR8dU3p3qG3/3QR9hiOQd15LFyGDzoDuK6NU0TW0w0aEXg+fmHEjUOiAMctzfpgCBq4Mrh"
"NxvzE+bIQdxtyQdWsYDrK/nbUg7DROe68oMfjV6Qcjns+7V3Uw5rQKjGqMIg5FreJLK2kA7Ql+KaEVLlfOODib70oikrQJaBPK3yJIVEbw2pwdpRJjAzb3Eu"
"rOA3voFFbFVmV0BWRGYl5xw+PSJJpTtgeKP8qNq96MxQFrNZeAP9/T2V5B0ins9W8WGjsDtheCDn9a7oSGpaKDWJiDAdcD8KuMToIYf92YW9bjcIMnj+FdKO"
"4y7uHHzqiULnySiQsPPOqZdtk9etvsF3wfcvlP5Pqtn9UH7n6iF79wrwGrDOu+L//992Bk/5oE/4ohc/94y4HF9YgGKTcuiHmjMHJOmMDChlfZ/vbrt3a0bg"
"9TZg27nnW3yMA+fZlaF04cjtXb6ivJ6KxyIsOYeo3A4gBL2o6pUumyFJnFJ0RRQeZkW4L1sk6NJSlIIgu7Fi9S7BmGVikGh5dvEhdSwJPgrlT1Qm06wbwOSM"
"ro0pZyh7BVGdUWQdu8VkswfkXx+TNpRzcr+maRWMcsXVCyRnoFdPKZ1opk1nKJTCUMGOHzsKSQDGLERtt01xaiKLkKWV8rG22owycnOM4ZZrt+z2Kx0Xpf2e"
"Pq3WEuk5Xo2Qq+XIa3uVZDMSCZbdbvQ5qsERx5lr82plXHKQ/9ZmzMZ/Qn9aHcrztniyC8+y7Jd/60HSe0/XADG/fabL/vXWH/Z/4y19I0EjL9HeWb0PadVm"
"reYEFY80k8yXJ7uDGdC0Y5d1YwSdziYTfUezyDyXI6RZeeRgmnLu6ed2LKy+CS6KowqWH21+zL95AAzNyx4G2MPBsTOY9waQdD5JDhV0jnS39O5ZI+y8SRBF"
"cbRQhFCWB8vhXawqlLMHwFbbtvbsuAJc+3QriGXh9OZvIXud+TH/m6MX50ofru4eg3J6M/CBRCgAfflFzW+TnlhY6BIvDxzQranSO7arF+DO4zqKYZzd+Zh7"
"8Mc/+Tcsgrsjb5n1iXA3dh3bgBWEZv80pK83wzMm1CSRIkbFJ57zZEzQ2izBDoB9BV6DAITmgA7COj98p9AUFDI00XeQ9VuXL7gW2ADSROn3ayf+lfpq09Kh"
"hSyrdaJjVVpSsphtaX4EL3DZIK1pCVid2M1VRBmQuANlKXqXumWmIyhGgd+UJCVaar7TrmE35ai04ukBHTrQDUVpGdb5b3ZcnigTsU9wNvr6VtmUb0fRUCKB"
"0XoTH1LMTillKjDihAsoYz8AdUtbzNhzbY3ZfCMtq3hwWJ91tUKmt/bjTEwokQFXSRRknIC7ueI+7aUslo+oIJ8K67D/bYNPOPKu++qoFNEEjsfjIa8bSZGa"
"KLmhKqVoWsUQOaIu5LFjrnxaRAo28eg05Fe0bY72LUGFwi6FJRGVQoaiKase1ATQ8cjlJ3qRABCV+lXOsUgwRsVuya35C9FyTbryT9zKyXSXTdXhFHtGGWWi"
"M3tJafkWbtYuzJEAA9eCNtVvrG1N53xsRFvvpXX1Fv1+HjMWWlGJW2zEE5llV3YOaUpcdpIqzaLmGh7XmOygjPkck864q5B1ODUPIRLt/7cl9TqapTlF27xQ"
"pMilMmApcSVjDjZv2fe36zXgw+YPfBsQv5r8EfH7fO/C/y2Bp71oe4ybtC1quxPY/md8KGyQiJAJFsVHTQVmMjr3fXSP2HBWNsEwBgqAEID+zygjUsXgsFxA"
"YKtKV2YzckVLcZyP9LX4I64tHwe6cAN7/wOqoNjVkrh08CIG4a3YVui6jA4O5sOPmapyxWPMg/S17oq+R4NoGePo9oH+zTQKLTBJJsh0ougfbT75791Zq3JF"
"g+1hrzcMq9vRsvZlNy9cKueN4fFOp5mRsvFvc2Ah3LWHjYIQnqmfEk3hYBuN/5q3jj8SD9whTcyRBZAKtuLnLPtlYRGk0i6nFOR+2V8+XTIpjIepYAHWcqUa"
"YHpy7JWoan1vEkJFVYAoLR71FAF7UAwJcF74ghfLgMTIAooKF4IpofqEyHPdsUnIXQhoGLBiIBcCoNfkXxXeMS6lnbKvPCsXuVuX7MLNYeu21uwziind6CLT"
"ONlTVmnJxwQht31hjwaXkJMa22z8P1d9fQqhBlks2h1aAj28Fs7tCDk2pyka1zto5B42kvdAZB8mjATq+cBGtn/wo7J019AX3x98/BTAdrsszVwEg3snwdyo"
"rEvhIzteOoUe03Pw/sK5QJIakiQI/T41ZM3p/n5gOY45xakXSRyP8lXWvKWgExPyYsMUCOgNeIuWcRL03VTmtUqBk4J6hFaisJbrYEdGT6nUNCxuj/bTrR/a"
"h3OT7Z1PRsf19zsJTa5mAuf8fvcyvpqlExGlCux+Ep+emjq6iKm1mYjO7zrgymLlBJHoD8Uxld1xEkWr7bXproSdh2Iri8kUrWRwlG+F2XP3+SxmOTIVS9Ma"
"Wbc+4nUbvbyM1RipUH6HtQnoDsqKCYlHQciuwZKJVJHFunfcdIQwDHbSNL0rrxSEmj28xyUUW9qBxAD7+un+SpxOzQWHy+cmADcFoH6PNuEb6863AQ0OAeEB"
"aifXBO/7n/1Bt5v28IgaEEJvU7cNe/2GE/TXXNMkerEXYxfAN3cG4dM3fJNnOOkLrGlCYCECkIRb2x6bPhcjsqgqIVrKLZWOBGS5gjxJU0n01Z4W0ZLEVCZz"
"uSvcKKOlGXCD5rgX0TlOhmYeOq3gPP8LPW0k2k0kzVRJpgttlP/dwPYdsZZfcOgayQqJ5PCg2HD6bUrPK4deOw4hkHTAE2w/ePxICIOjJnWldnnkphn3pfpt"
"u/za5csyRDqO3PPQ6+CeP3yCwek5X7xA1TPAxZnUI0zGj63XRnF5q2KzZs66TM8kwdDe/yFdXGyRoeGE1OpW7HkhZp2xHK1LM31BiNUNW9ZvsdA2kf2otzT3"
"RS2LJ0kqDjvnEXKnP39iAddU6A1Rw7VXFJOO0h4QuF7lIQ+e9xgy5oh4YUXDOZjmCkpWVSv2/QgTZOyZq2tqXyvC5gzu27+Xsuv6L9Lo3KhoVPo5jcD+51Yt"
"krUrfFxTdfTR7Gm9GgPBdp0iN+1mXZNpPHrolLUTFaUyYBlXBoFxihp0OiPGzkWMh32GsQupk/9Lv19yuBxSua7/XJraPyLDAEKlwuVViU1NrACLYsVT+E+8"
"z4D/wL5NW3gTOm4qo7YbmUCPTmgFxtLRKPqtxGsub2Ot6eomWZE+eZGxwx71mV4MsxeXbsl3Ts9CoYmE+76aFOgWq5rDWMVJXRjPUGIJ/foQSMBm/zAfAUgb"
"QsJUq3aPWFs89JL43jvSVCnsDaM4RrJ0KyIIIOqM/NoLFvWLJrjBgEZpE2W+qgdUK8sFQbKdjo6t85GJ635eWKPcc3oYrq43F43TZAn1K9xC0pimDLMM7GYH"
"/2kp9MhY3egqAVhy2zNHQ7B70WJAldYElbLUhkM7I4MFDUQslR3Zq/015uTJIqFpAUT45kIdBtotf0gPAbtK1zSMri/8v/Y1Mb6g7TUvM4AVIx+9THfokVnw"
"wueYslBdxy89dQMBC+FelK/+56waHxgeceRwGstKKNRBsU4F7wBpRNMHixAcDLRhf0aFmyh0OVFAqI5tqSpkCK/TMV1TEoyIFLQbqrti8tplebGjRqFBUMwl"
"km0RB+HHhbIpD5Bs1iWybuqYpQDnlJ7QCyR6Ll3nsA5dSaoqYaYNqSxG0WQSEap0lPGoGtXivK7ndSQK0iouiuf/aMwBC2sJf0YWUn5veN5WmVD6ZbFO1RLC"
"h/T49TiPW1NdrDMFHa3OSb1YPq/WYX6eMNm4+0FCMXioTX04EDx+1KUuIendVr//fBrn9S5RZ0XygfnffZhrncKkY1Ei7j6oy5ItYeiF17JJGqX8Jx4ghfSQ"
"GsEZOR/0Gojrpxynu4FuDbYjxyymI1LlXHFRBVdJxkb7Xk8xWSUysQcAnaGUsnHJ1tLVqiCvr7rPhZTekEOb3sd4iIo8YDT0+zumuaI1TXgbqsTGsb3GidO/"
"D5KIq1k8G7k6fCGUTIaTgr+y7+szFirqwWoIcEBGsWxZhANKnRj27lPgVyMdO1Q5+BlCFG7RkLrKR/olGn/c4oRYwJs8dB+tx8vsjqyUq/Gk7DmnQGQxbTnH"
"cad5hUk1G1f1zBpIKKuwvcrm74RAFr3iYtAUbeYu3+GeuHi8SwoFpfYzMS+wNMSV0ncbiOUIg3u9JiALLB4vVM3q2EwxnWwAoC+OKUqsWi2E1CKXgTRficTw"
"zbTtnnn5W6fAjkwt9zAAVgObJKFQoHRD7VFBbqeZPMtyvKwpkKGuWBCnP+ePAG99J9uOPbRiyCmLMFku8xJ1LA8ZtvsIjnsgjBQwxcQjwLz2ggACwVty1VfC"
"GziAFGNvfUEy/fV6KLENtht4ophPnCeETWD+7LjtVmWBzES03TMUgTvj+iXbLmg10omcmWpfq7DXj8dduCNnb0YVrKl1q0uKMlqJ8x9vP7AiuDXH608lR81q"
"Ilb23YOA7yARG4FXkygWyZWYi+QAkyO7J2k8l1mISuUb1P4KSfPoEA7+6sVdvN3epA5jPBsev/Ho1vkVN3TdYMoJxonECQQX8xl1Uw7i9jtsqYXVWK7/o8cT"
"SfSJ4EdPmPgwFN2jNJ7biPEEEnfCwI0Shc/zEoBBNKTln5FgmROuq8ZxeBEaMa6daZcK3rvwXWHQGa1ejAne+pu1ouVAyibNbtfEmuou6DTDLdVb83rHugYx"
"SkFvI6S8YN7nMsVCSzUT0SNG0l/qzQN5BxbD3uqACHUhBbz8CRPKJjDtW8OhiSm72V2OTB4kzeVuvpeaGkicMLDTgg53/QvmKg8lYKzQNxQ+gcJINlyt+ssS"
"FkF/nc47R/EHPz446okVKBMg9zY7zEtns4UxRJxKDEG/jRqzKLeROpZkK3G362qCWG50B4MTWWPSPBmS6SCT8Vut5wE3bm/N63clPH4HgkrugWh41GiJISq5"
"3gyfzXBeU+2uNZDi2XAim4CapZnKE1Krluqlbls5GFQFFZRcl2Zm54iqHWJNQ3cG4Vg6Pdao+hphoH03ekej+Yrvue1JXRORxhDR0EnLa3FiqsbsMptcKH8c"
"3u5PGSxzQXDr6kjtV/4VOkrsGvS6kTHRN/2mXKsF44mMncE1x0PTgKtx1Gpvhvq81tUIQY72sEVxxM4n6uUFAY2ljlsB2ZxqVZJpiAbvUKHM9gKNsWZECahO"
"hCaAAGRhCpr0tufzGTNbzUpp4VfcdZsyXPHoUD5eWQuYEX9H+9HUHl6aNpSxnIMFGRI+EGrlc673eN2ZGTf4TJ4vOUgg96hOLGvmcPlegNjm6D7+BGIAxVRP"
"Bz26G9ryjlde9g9FF84d3bO7np2Z0Q1tDZDTgqoNsvTjRDKC8nZ8qcuc0iVKAAGbTEjWsVGY3qY/XPeYMqUmovBxpzRY3Wz9Kbh/bii7kBFCuhNHefiESmU5"
"qiiMJV6RqLc6Nd0yYOXqWDELNAQdtTvmKkqy72MBt8VxqmOrYgDWiNdd3gZXz5iMCCV/yunxE6Sq4JEnzMiteGMaOMyC1XfTRO55+eFEfTWWbT4UASQ728Wl"
"jx1o3M0pGwqgCgj690tmwpGBqBp/esom9udQCczVpibGpTFxeDnU5ZLAfj0nqjsASKev065WkoWqkKlADWqTz2mGz4LrtoWdXrvlFrdv09ofTjIloLlRl0OE"
"XZ3Fh6hWRc4yAUZny5jCE8MdAQ+cXQrpxHIXhx3hUVbsxrINBO3WOdUYq3AiXzdNuPfe9HbBaEa9MGBVbTRWUyZfZCosH7Wjfw7FcFLul1g2qTqLKGuWafA5"
"rDEGobqMT9wmdTF0MIe6kUn/CV8IIlMt3SMoSZLjK9DYbvWtGY5USQAZRan9G2jyv3H/0dNefh1HVhhzXbLELPvU13Fq35s+Hl0dmIEmEnZO+Hg/HsycrbQz"
"WwS0U4fma5yK8ZOve+MxbK5Zw59WQRX+CX+lXqgfhDewmHIZjK/sEPSx4kEo5SyvKOYM/WZDjmN+KmtVz5L3svj3j6q/IKt7BIdmYHlVfnfhOWgWz1hRV/F0"
"kM6wLBdMiz0mP+luXu73B4N+L4SAgZRSGyYCtqpY1AR8iNcINxwXbIKoPRzwen44hbdCi0sc4qnF0nvniIxthJHA5GIbYY/tnQG7w88/HxoFQTHri7UgHqMT"
"ntNr1mCkDkAwhSVjov1XU99p6kAVdfcYM7A52uxirEQb/X6IHbgwBd2xNmB5yMgmQhE5PoY+zh7duj2fkAqShNsev/lReD3qtjFPcbOZawQzfEl1c59mWWf1"
"/f6Scy+CAkYGvUaNivYSwTSidgSAethatqOj3YYuxqs7FqxGLeAPUNSoGQlVKWAYETdnDVjeVTcbWdR1lj1n07RahPJ90io79+oFpcWoxfOjqqUASjLSb2Wb"
"w3MsCu0ARf7Cd1vpjY70ImiCVEJSzQKEigBY0xve4XmepYqxuFNl+bbt9x+1FTaqkbQjrLXhRit8MfXUtyi6FwMQBZ5nimReFn19QXl5aFB2pfaSkds1icMn"
"lAAPAxjO5k0rhJVVzgeASwjRIt/YX1d0TXaH5fbUDlBWfm8lDstqKbUbidF+ahCK/v25Sja5nZjSSihuWTY+N8FYixd7qX00Qpy6HiNqHkqsH3F+33kVb+hq"
"bj+E4ST63GCr4uqS1R1ndP+wdCSaWId+Y7/YYOkaT3xrGFbRwSvNroLmuGanYbcHAkHUm3FnRM0Kn8fP+tv51CiC6JirYSkqJgBswz3esN16251pmW82vP7A"
"RnFxrWvwHWbeMeaPEmoapwROpvvd3I+rukkVFMZ+GeDH8uVjSbl7z5mePvAi7ZtFwOCMdv6go7h/Re2XGOSs3sWXkb8a9riWQaUs4qdSEMZI2RRtXhnu20qA"
"4sMubaWLSygYbfh8WNSjNeR6TH+7Hq2Wx+JHJEEwyuRW7AQQWilm0V+vqBY1UP8BiuIy4Im7A1ztSDvKWhusu0EYPs9Mv4qa/TnFILCkKy8BzOlHrQ2awMhk"
"cU7ClSBqQKdI/jCzl5rH0V/v87TXloTrhF9o09pb9VEO4oV4bqIXymsrrJGFu/V4XU3lwGHiOaflj0pPl0TmRyCbWjt+F4LisRenPfwxqV18OcPGQNqsKdTW"
"ze+6cLEMJnQ6nZCk24mhQ6cW5nxpoVZFvaPNb5jyoZsI2W/g7ILloboM7NEbD+SvMYQK7+KkFoW9ad/OWQIuDG20V0mkJ714aT4lqgQ0KeswkHd3moNm6PpY"
"EA5XL+/0D8gVwKbNWlFH1GhhCN3EsEUshyNAACOTExo0GZpW7l3O3Uq46Hx5SUGEfVcGqq4V5VI9br5GxwylNKOeqEjOsqeXLWe4UVkZZfVy/02T6R5QdDNL"
"FVV35L0TyZyPJpxHqPjisa9c0eAiL4BLaFxyuJG2NszqZT8RxHxeUkS3TluKom75JuMBakpuhL1D1r+WgMLgsi2HoP82t1NgBViLE3t2Jb0yzqCK+Yge/eT/"
"v5a0ZkxOu2s9FFMEjUutqpSPnmJmSK51nTeYFqXbKJdl0uPuGsLYQt1oDwS5CIA/oK5XZB6mVauP2msel3P3W1ZNmPrXlFt1ovERa9+7Y1n3Hhy57Kpv6R0R"
"riO1qCnvMJUZ3ULmDinwpO5xfpmNQ26Uv3AzIjBm5g+0GRXnZjDlRm2NZQ1zXIZsOsWFhkUMIOM7XnzolO3mpHBLD0mimM9TVMfaNIp40+CyIzN3YnyyyxMZ"
"QvqiwPTYf5LcduvonUeXwjVdTPZp0uRdDO5gV89Jic4hd4LjD5C581nmJ9iggCmMCRQHQPC1sZPVCv5U8cSRcB/TWtdvOsDgSWP0QW4n0wZHrvkg+OBLgeY2"
"D5DP2yW3RWjaE0Z+A9qNdjLRDO5JGpEax/muKTKI3DDlm2Q7nTYAsw6uT2kaKSYxUIPNyrzWxmEN9DwbTNpSdsXuq1W3eGSAUey23HLInimbOZ+X1r3QtwdL"
"cZ9NulmnvStblckSLrkqUCFWwyI5t8TC0ZlJLmF3upUaXswwIQa01dzwPqtyNOg0a82DbonSu+PkNDwi3r6Oq88tl+ATwAy7xgNQ0Nyge+TsAwhkcYBYt7bl"
"FzQzBifErExoTeug1lwbCpjXyz5GiiVmWy5uix7MjEcDgkfVH6Ku209CZ6xFvt5PYWCIDhhAL9Q7xeyX0oPOugvW4oenu8jW24ENeB7oYTrQD5eYzzQmJ0ir"
"CdNq47Fw1i//RXIaKcw6FTXlyYDpfqAx9H+rir43lAm/bEWn89TX9QeZrxZhMLRxNy+VRp8QUa19EdlgIKNIVITpdTlv8lF88Dh3AZoxApXUfsERmkhoQw6v"
"IPbVJ2th/Dc4qjMMrsy1WOAltVLQdcANAUkQZK2tayYqy75dWtPLqqKo9TDyzmtDAhF7Sa/F4pennKB8Q5qSZ/VwrSj6LS7koQJaWkHsZ8ndlhOwQJJMpeFd"
"PK0KWn8aQ38Kppg6HQ7fR3XHH3SQ3Wz69l7dsKAVS8aAVXd91z1CxWaCYFsx74lJd+m62wkbJWHH9eOChRpjJt+nNQS7wt0mp3RlO6KlnQCXAOjOuQIGBOS4"
"C9vs0L5xKh+tshMXwAPwYVAUanRK8yUjhjH81YK7DNIYyI2zxR6CRhuoLf1ks9Z2rJ1NqQwcHwMs4m2gdrSGGvuzYWhXldMFxl56bU02H4MIAWzAdBdgPG52"
"YNX+HT7w8lVBhJoSqQjYp4NL80GAOXEMp+cH0yFp/5PIomsnTlq59W36QMUQQwcP8vZ4nZH7HlOeTfVmP4HRp6r23wCgqL7PcVLePGoV1w+pAZ0g9D8pTFV5"
"0TKPxvr0jOysg9LTQ7qAxnhfVUT1cRPumsdVuHbYBjFg2sO+u66fFxKT2JvPHnF8ISu9IWiUZUDRfCKsOOTx6h53/UdPuSOkETbUxNl+TdeVofwu3+e/V56M"
"FOHGngztvKrhNO5H6v62S3EStpKRlA0nsmMiuRh6NeHGle82CTo8mIW6heSzojkZJJ0nLFvD9rn9AI298N61H8B62LbU30FO38wMKNXufzdKGa1Iuk7OC1Yb"
"vlO62Al4/vlLuCU6nc3qkCtBJOzqVh+PixfJJSvZiZjbT+A4WYoeF8phN1xKt+7E7mTYRlhNvak1s07yBfUT+ZNHpFbyYQpPzH4nHh1gtFDOLkJRbbF9uSIg"
"PU7TQK5d6cbtS4QmVNeZx5GsdM/dl8kOFs7ccnmwDje9BE2GVTHc6t5L5omWtw+21z+miRXYqjyiTz722pox0D9BVytRn1ol5YUUkesrmKpiQO21bDcaAcHx"
"UxOrkewXDQyQ853+BieMNGVh46/qN8gzK5hPk2RNM7h8AlwVRvo6GuL4JTKOUqrRtWvgNk6iWla3MdzJhDJRkjFyoMFGb3X1po7pgBNmpI8hdBw4syXZZpZy"
"Z9RvlFckIG1br732QtxdFXBwWE1mSNgkU4MyMdnKEXyXqOraBLcjZg8jKMiwuFX6n1KdFE0pw7Msn0L/11dN4y32gbQSkcgh7Fal7ahb5oO+JYoZ9Pw2qQFe"
"k9gmnvosNXkPwVkUesUTkVivRcFDEr5d8+VTzCxCfszAvhFV9WvjoxakcXA/3CIhFys5GbJUQpkb7kklSt7rVl5MBU/NJ5xS4Bji4gLBuow5HK+txYBHNBIH"
"ervwm43UhCfBLWEeK2b8ttuCdRmMhHYFYtcLPNrBtJ23wu41IxBwcHisa/Ehw5BIDZd9iNBLOCkCXswi7cdACJSLCNcv5xRQ45tMPtx9TG5pMvLrwWAsNMLq"
"JqNamPizoQBaVkanLV34DwmxVZI+Wryu5HYyzN+usAZliqt2oZRKD3W6rRO3LUkriXxd5uLjndBFYljX1gCXT7GMlEQFhgC8ybRq5trRxGDTCkxNqxMaRC7K"
"J56qn3a8Z20XkA7SOeMqrFpYGsl2p2kZZms4gdRWlZYJoamPESbHvlzvy1S71CG8aOUm9W96lhwRiLlTLQWLn+TSJ7UIANIKf9t0dqn8rEmK9fFIjZWywmT6"
"UbOdJbCAunNGFKvORgrwf5dRuS5Sa3PJvQXUlzs6pYiYf+aEK1uLBDU19Memj3HOP4ol33I/S1qL+igt1+wp5T0nROCz9NYWddR8q9NfdSjj+w5Q0u5Omd5k"
"3iJIDIP2xKGtyE/VXCDtwImnFB3laT41ZupAS9fN1bggsW1ET+0jMSKZW7WuXJrmW1uz9qksmatNa5MDY38w0d48cSF8eRaMGmAan6s7SvSvMKZB6tw4eiPF"
"6ou+p/x6CfbuOnPxBcFH7g1Ql2duqkKjkWGRv09vw3X92Pe8dggNViM12A39fagbBw65fGTN661KqiQjSqm/g9O2HWR7UdFLCa2O5iOtoJoZ04rxQr/qVFw8"
"PzmalSZ6O7xSz6HAkbCWKrGbphVrZaD09iaWmnmotqI4BACi6qWGn6i/5klQE3cCR3+wabmHn9K8IlCGOrYLwMUU623ME/ayTc7XOS2zCkqlk9Lt8lbzg64A"
"yPW0gnhDXYWaegk2XFPK6tUy5LNQKWQxIbFoRh4mkIvGgrMY47Xp7J/ORGIHUjiM/GCpP/ecYIwHsnZO5SW0H7Dvsl0K3aOkm4ZKZukpvxXgWINnlSne7C/X"
"K4YpERSuRIB42FeDdi6qNzO7dMid7uAE4aVgHREatwm46NUYFLdxktn5G8yjP0f/Ny3VxWvV4vwiNxZZ4lqx90wJZIH+DDcEMG4XpZCWhU8FmJfq6dUCWkNu"
"m8phk3eFspTQqWrqExuGAXkES+u00UZTNFVCy2YTpbTY4i9+cXIDFJBjBexVOBEfZoA3dz4UiAJk5PXWu1QtFz7ZEJoNJzrC6Yhr4XpllVythLrDScNlHsAm"
"Pocm5Ly/efJkFAkDNbNl2oUW3T9dhUaQaYhGAhNciZoLQduz9BJUINGvLzhagmsfhllrBm1VRvXrATzlZqVat2USBp8rNVV+8yxaw4MgXlrqH77H79q063P/"
"p3dY8bFe2BFopRmj/MI42W0RNwv9zDqkqXpCZLlEdOL7s5EYb78ChwPGPttzXSi0ul2xVRXYZEowYEJ5s6MVHpnRq6C32fqzjLiJ0VYIS2NnIaoOlWWUQ1qw"
"qBpEVjL91O70ef4CSQtGXLjR2hbFb4Fqp6exZg0TQw8mmDc1AND+wIMkgBTjEmUAGZvBuBX8n8eWWlOGjx7XNtQMROnoTCgUHkDmNA4WrdK+be9tHQBRNSNB"
"606/4txOOawwdKqG07sUYXhOEOQuwK0O+izbgKj3Kts5PABgevL62ZMnL149re8jTj7a67BIYbLd2ULICe5tNDHtVpztSGkonx2tXFJk+nIwT4YTATqzBc8x"
"p7eiw+KZ44j5va6d/VttbOnfd3V3/9h26l8qcBF20kP0rhOL/jVy9mDyli3oz+a/X1fH3hh6fXWucS94A7jfaRUCP5vFL6c4urmV9i8f1rl54fXrfqHy2uja"
"Nx4dyEGsCFw8mMCR0Mril/GzPg99LKElWLXUinCLPBOvfgDBEDIh07EGwhT26brWdzydRSmn6q/9W4bWYP/cyo9Lm64dJNe9C2D4YcY49riYjAJNEkRrJKwa"
"7oMm1LJGpqxZ7QAd/4sACeNdGyEEhe7QNR6miRiPXfl5Bhoqex86grX7z+K/4Q+SY37RlLCCsJqiq4LAytbbTJ95hW7Ui4qSbe9nr4On2aV1oMK2bKxDiypK"
"NG1q//EH1JiJDzpza+bSWdsE/FYVwZUu2tXdh1IsDPvvfpreXd5FCd6W64JnCbOdFLHuHB8+PRujOHx+ZO2WUylipxQ9mCSQGFAw79TBFZKbNQV8FAMqpOVY"
"hI9hhbiI9NT5Z1OjSv6bG7hValVEezMqjK4AHWDAqX9pbj8ybzr9/0ximUbAD/DOje1XAIBvfjpUHITImo99QwuAhAEAAgz8n6n+afClv0Oa5ECAhelbIc+s"
"nMKeglG1I+5l6OofNlciPd2TBVWLoMNaVtwpeRFtcKg3Mb+UNc5nZ6OvVQr7poc9/5TpxJ54TUdNIrHdxbHvg83dku2VSk/Fz7BdnekPiwmLpT6fxp4RnCJ1"
"6czLjImPw2m6jPl1ifxu3UdurcU0mVUvPksU3XWUxPInol7DrUKw6zHIaynlPKz/tkYlxXn2IHBywbXvBb9iEFJ3EJQD/DpHeh1lVLEoAQD+H4rqIRNyn/py"
"Cp/Izbfm1Ig1zcfdMoB1DS1tFt4zhAlVT28bTVi8QpW7CAklcfrR/f5YJSE3I2mKFoyqhbxqoFRjnElufWZNviS+FikvIWMPVDtjZzEyhdBbhRipAzlAYP2i"
"E9MDq3piFf5eNTGzPKcUiMseWR43oGWViNSr7hJkwxVe315AmyWg6hEUFJOsJrUgVNYEDO0A8hiiPt487uJDHT57d2kwat4LtVXNipqMoZYjypdE1Q4lJSTt"
"4FmLufWc+TUgm55uN1jVQycWh68W8mZ3HqRc+zb9Eyba+b10UMxbiCg8vSUnr71BdA5QHUVI00xdmZEX69hVA+R8iX9FE1X2hOVxAxWk2nDXtBqiswQwq2aN"
"rm9EHDMYkI+Rl9OwQsqrfOxmEY/gZtpyhFvNlNYRpiugWkZX64ZMR7qpahMQxsfPb/dg3vryXXROaro73NBZsK9pD5Bek5cf61p/DsLc3xP9h4n6RUwvrd7N"
"fKO6DiYXbYdkOn0eLgCb4DgfHEkKRRXDoqN2te/JxwIhMZMIS9rH5DYBWD2dWQVHYp93ud3nZk0HMpoJWkkXyRJOtCXRfosZeRrIk/SSxqerR1or65Xmn6H+"
"nPyQF9ax3fU9u2u1o+mZ2stEpKb2gpl+7agtxqMFvg/HS1pIq9fXCTovXf/Mz5vP2Xz35Jvto7Y27eRLS7Qoq6WhmamHuozsXljUnLZa57FUNdJmEKX9mFz0"
"U50KxuYey40l8K4v41/wXvUmWlbkHaSSk+4Pr7sxb/2cuNOpLy183maZL9V/tDxd4wMr81tF3XMhN3TyYRFNOF0zzx+30gS6n2S+p0IR3pAtggH/BdKJgs50"
"DI/JrWXtsb6RJ2IyFD1pkl4k6qKoyElK8Zi+v6rLIPCQ5XSSDCtrlWgOSDVF0aZ3ehyMl++16p/Fnzjm+fBfF3hP00D1PnxdVa9AYYpgSg0j2xeYTXvAof2B"
"fXsTbYSZW9YZ8yWJHcB5ZY8096yZJOiZ6HWxjpZCB56jtCjfLLQGARULAPeBvIyI9JYxvqNlgp+LZZJ5HCxTDCNc0XYOfS8EUIYUygi4UfwsijNCNJnyMgOE"
"Yfz0TGDGJsKpqFUrI1NMSsefGwlXnpVKMSBO0wjoHWmZEgJRpaISmlhF3tdCUhyqvMRSKltpObRl80Spm6VQLlyMJ0N4vrRINKm4S0DMIqBNZRTErESiyuzn"
"nyf5QmE6FH2mX6ycMqn4VTqTlN35M30h/+PzRKUnQmzcX4uNJTKq5qOlnio/0FPkxF+E15Ad08ojdl6RoERNWq5EzoKSQ3AqSjNYNu8xR4db8hMB"
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
