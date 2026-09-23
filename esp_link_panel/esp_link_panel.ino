/**
 * @file    esp_link_panel.ino
 * @brief   [EN] ESP-side ESP-Link bridge: exchanges binary frames with the STM32
 *               over UART (921600 8N1, ESP_AGENT_SPEC.md v1.2 incl. section 5.3 formulas) and exposes a small
 *               dark RTL web panel (Vazirmatn) with three tabs: status, calibration, manual test;
 *               the section 5.3 conversion formulas are shown with live values, plus calibration
 *               helpers (zero from raw, gain from an ammeter, voltage offset from a multimeter),
 *               a live 36 s filter chart and a JIT re-arm button in manual mode.
 *          [FA] پل ESP-Link سمت ESP: تبادل فریم باینری با STM32 روی UART
 *               (921600 8N1، مطابق ESP_AGENT_SPEC.md نسخه ۱.۲) و یک پنل وب دارک ساده
 *               راست‌به‌چپ با فونت وزیرمتن و سه تب: وضعیت، کالیبراسیون، تست دستی؛
 *               فرمول‌های تبدیل بخش 5.3 با مقادیر زنده، دستیار کالیبراسیون (صفر از raw، گین از
 *               آمپرمتر، آفست ولتاژ از مولتی‌متر)، نمودار زندهٔ ۳۶ ثانیه‌ای فیلتر و دکمهٔ مسلح‌سازی
 *               مجدد JIT در مود دستی.
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
.as{display:flex;flex-wrap:wrap;align-items:center;gap:6px 10px;padding:9px 0;border-top:1px solid var(--ln)}.as .nm{flex:1 1 180px}.as .lv{font-weight:600;margin-left:4px}
.sb2{background:#243052;color:#cfe0ff}.rm{background:#b8323f;width:100%;margin-top:8px}
canvas{width:100%;height:140px;display:block;background:#0c1018;border-radius:10px;margin-top:10px;direction:ltr}
.lg{display:flex;gap:14px;flex-wrap:wrap;font-size:12px;color:var(--mu);margin-top:6px}.lg i{display:inline-block;width:12px;height:3px;border-radius:2px;margin-left:5px;vertical-align:middle}
.ti2{display:flex;justify-content:space-between;align-items:center}
body.dn section:not(#t2){opacity:.45;filter:grayscale(1)}
@media(max-width:640px){.vs{grid-template-columns:repeat(3,1fr)}.ch{grid-template-columns:1fr}.ms{grid-template-columns:repeat(3,1fr)}}
</style></head><body>
<header><h1>پنل ChangeOver</h1><div class="lk" id="lk"><span id="lt">در حال اتصال…</span><i></i></div></header>
<nav><button class="a" data-t="0">وضعیت</button><button data-t="1">کالیبراسیون</button><button class="m" data-t="2">تست دستی</button></nav>
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
/* ---------- دستیار کالیبراسیون (روی فرمول‌های بخش 5.3) ---------- */
const VR=[['ولتاژ ورودی',14,4],['پک ۲۴V',15,5],['نود ۱۲V',16,6]];
const aIn=(id,ph,fn,bt)=>`<input type="number" step="any" id="${id}" placeholder="${ph}"><button class="sb sb2" onclick="${fn}">${bt}</button>`;
const g0=$('fb0').parentNode,g1=$('fb1').parentNode,g2=$('fb2').parentNode;
g0.insertAdjacentHTML('beforeend',[1,2].map(n=>`<div class="as"><div class="nm">کانال ${n} <span class="lb">raw → فیلترشده</span> <b class="n lv" id="lc${n}">—</b><div class="lb">صفر: کانال را بی‌جریان کنید؛ میانگین ۱۰ نمونهٔ اخیر raw آفست می‌شود. گین: جریان واقعی آمپرمتر (mA).</div></div>
<button class="sb sb2" onclick="zero(${n})">صفر = raw فعلی</button><div class="ct">${aIn('ga'+n,'mA','gain('+n+')','گین')}</div></div>`).join(''));
g1.insertAdjacentHTML('beforeend',VR.map((v,k)=>`<div class="as"><div class="nm">${v[0]} <b class="n lv" id="vl${k}">—</b><div class="lb">عدد مولتی‌متر (V) را وارد کنید؛ آفست جدید = آفست + (واقعی − نمایش)</div></div><div class="ct">${aIn('vm'+k,'V','vcal('+k+')','اعمال')}</div></div>`).join(''));
g2.firstElementChild.outerHTML='<div class="ti ti2"><span>فیلتر جریان</span><span class="sg" id="cs"><button data-c="0" class="on" style="width:auto;padding:3px 10px">کانال ۱</button><button data-c="1" style="width:auto;padding:3px 10px">کانال ۲</button></span></div>';
g2.insertAdjacentHTML('beforeend','<canvas id="cv"></canvas><div class="lg"><span><i style="background:#5b6784"></i>بدون فیلتر · نوسان <b class="n" id="pu">—</b> mA</span><span><i style="background:#4f8cff"></i>فیلترشده · نوسان <b class="n" id="pf">—</b> mA</span><span>۳۶ ثانیهٔ اخیر</span></div>');
let CS=0,LS=-1;const HN=120,H=[{u:[],f:[],r:[]},{u:[],f:[],r:[]}];
document.querySelectorAll('#cs button').forEach(b=>b.onclick=()=>{CS=+b.dataset.c;document.querySelectorAll('#cs button').forEach(x=>x.classList.toggle('on',x===b));chart();});
function zero(n){const r=H[n-1].r.slice(-10);if(r.length<3)return alert('دادهٔ کافی نیست؛ چند ثانیه صبر کنید.');
 const avg=Math.round(r.reduce((a,b)=>a+b,0)/r.length),du=D.t[(n-1)*7+5];
 if(!confirm((du>0?'هشدار: duty کانال '+n+' صفر نیست و جریان جاری است!\n':'')+'آفست صفر کانال '+n+': '+D.p[n-1]+' ← '+avg+' (میانگین '+r.length+' نمونهٔ raw)؟'))return;send(n-1,Math.min(255,Math.max(0,avg)));}
function gain(n){const m=+$('ga'+n).value,cur=D&&D.t[(n-1)*7+3],g=D&&D.p[n+1];if(!(m>0))return;if(!(cur>0)||g==null)return alert('جریان فیلترشدهٔ کانال باید بیشتر از صفر باشد.');
 const ng=Math.min(3000,Math.max(100,Math.round(g*m/cur)));if(confirm('گین کانال '+n+': '+g+' ← '+ng+' ‰\n(نمایش '+cur+' mA، آمپرمتر '+m+' mA)')){send(n+1,ng);$('ga'+n).value='';}}
function vcal(k){const R=VR[k],m=Math.round(+$('vm'+k).value*1000),shown=D&&D.t[R[1]],off=D&&D.p[R[2]];if(!(m>0)||off==null)return;
 const no=Math.min(2000,Math.max(-2000,off+m-shown));if(confirm(R[0]+': آفست '+off+' ← '+no+' mV\n(نمایش '+v2(shown)+' V، مولتی‌متر '+v2(m)+' V)')){send(R[2],no);$('vm'+k).value='';}}
/* نمودار زندهٔ فیلتر */
function chart(){const c=$('cv');if(tab!=1)return;const w=c.clientWidth,h=c.clientHeight,dp=devicePixelRatio||1;if(!w)return;
 if(c.width!=Math.round(w*dp)){c.width=Math.round(w*dp);c.height=Math.round(h*dp);}
 const x=c.getContext('2d');x.setTransform(dp,0,0,dp,0,0);x.clearRect(0,0,w,h);const s=H[CS];if(s.u.length<2)return;
 let lo=Math.min(...s.u,...s.f),hi=Math.max(...s.u,...s.f);if(hi-lo<10){const m=(hi+lo)/2;lo=m-5;hi=m+5;}const pd=(hi-lo)*.12,a=lo-pd,z=hi+pd;
 const X=i=>w-8-(s.u.length-1-i)*(w-16)/(HN-1),Y=v=>h-8-(v-a)/(z-a)*(h-16);
 const ln=(A,col,lw)=>{x.beginPath();A.forEach((v,i)=>i?x.lineTo(X(i),Y(v)):x.moveTo(X(i),Y(v)));x.strokeStyle=col;x.lineWidth=lw;x.stroke();};
 x.fillStyle='#6f7a93';x.font='11px Vazirmatn,sans-serif';x.fillText(Math.round(hi)+' mA',8,16);x.fillText(Math.round(lo)+' mA',8,h-10);
 ln(s.u,'#5b6784',1);ln(s.f,'#4f8cff',2);const pp=A=>{const B=A.slice(-50);return Math.max(...B)-Math.min(...B);};$('pu').textContent=pp(s.u);$('pf').textContent=pp(s.f);}
$('mc').innerHTML=[1,2].map(n=>{const id=14+2*n;return `<div class="cd"><div class="hd"><b>کانال ${n} <span class="lb">· باتری ${n==1?'بالا':'پایین'}</span></b><span class="tg" id="ms${n}">—</span></div>
<div class="mx"><span class="lb">duty فرمان <span class="ap n" id="a${id}m">—</span></span><div class="ct"><input type="number" id="m${n}" min="0" max="500"><button class="sb" id="mk${n}">اعمال</button></div></div>
<input type="range" id="r${n}" min="0" max="500" step="1" value="0">
<div class="ms"><div><span class="lb">duty</span><b class="n" id="md${n}">—</b></div><div><span class="lb">جریان اولیه</span><b class="n" id="mi${n}">—</b></div><div><span class="lb">تخمین باتری</span><b class="n" id="me${n}">—</b></div><div><span class="lb">ولتاژ کانال</span><b class="n" id="mv${n}">—</b></div><div><span class="lb">سقف</span><b class="n" id="mc${n}">—</b></div></div><button class="bt rm gb" id="rm${n}">مسلح‌سازی مجدد (ارسال دوبارهٔ duty)</button></div>`;}).join('');
[1,2].forEach(n=>{const id=14+2*n,r=$('r'+n),m=$('m'+n);
 r.oninput=()=>m.value=r.value;r.onchange=()=>send(id,r.value);
 $('mk'+n).onclick=()=>{const v=Math.round(+m.value);if(m.value===''||isNaN(v))return;send(id,Math.max(0,Math.min(+r.max,v)));m.blur();};
 m.onkeydown=e=>{if(e.key=='Enter')$('mk'+n).click();};});
$('s19').onclick=()=>{if(!D)return;const on=(D.fl&32)||D.p[19]===1;
 if(!on&&!confirm('شارژر خودکار و همهٔ محافظت‌های باتری متوقف می‌شوند و duty را خودتان تعیین می‌کنید. ادامه؟'))return;send(19,on?0:1);};
$('ao').onclick=()=>{send(16,0);send(18,0);};
/* مسلح‌سازی مجدد بعد از تریپ JIT: همان duty اعمال‌شده دوباره فرستاده می‌شود (بخش 5.2) */
[1,2].forEach(n=>$('rm'+n).onclick=()=>{const id=14+2*n,v=D&&D.p[id];if(v==null)return;
 if(!confirm('کانال '+n+' با duty '+v+'‰ دوباره مسلح شود؟\nسومین تریپ JIT = خطای نهایی (فقط با ری‌استارت برد پاک می‌شود).'))return;send(id,v);});
document.querySelectorAll('nav button').forEach(b=>b.onclick=()=>{tab=+b.dataset.t;document.querySelectorAll('nav button,section').forEach(x=>x.classList.remove('a'));b.classList.add('a');$('t'+tab).classList.add('a');chart();});

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
 if(on&&d.seq!==LS){LS=d.seq;[0,1].forEach(c=>{const b=c*7,s=H[c];s.u.push(t[b+2]);s.f.push(t[b+3]);s.r.push(t[b]);if(s.u.length>HN){s.u.shift();s.f.shift();s.r.shift();}});}
 V.forEach((v,i)=>$('v'+i).textContent=v2(t[v[1]]));
 [1,2].forEach(n=>{const b=(n-1)*7;$('lc'+n).textContent=t[b]+' → '+t[b+3]+' mA';});VR.forEach((v,k)=>$('vl'+k).textContent=v2(t[v[1]])+' V');
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
  let tag=ST[s]||'#'+s,cl=SC[s]||'';if(cv>=15000&&man){tag='قطع ۱۵V';cl='r';}const jit=man&&s===5&&en!==0;if(jit){tag='تریپ JIT — مسلح‌سازی لازم است';cl='r';}if(s===7){tag='خطای نهایی — فقط ری‌استارت برد';cl='r';}if(en===0){tag='کانال قطع';cl='r';}
  $('rm'+n).classList.toggle('v',jit);
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
 formulas(t,p);chart();
 $('mb').classList.toggle('v',man);$('ka').innerHTML=man?(d.ka<1500?`پایش لینک فعال · <span class="n">keepalive ${d.ka} ms</span>`:'<b>keepalive متوقف است</b>'):'';}
async function poll(){try{const r=await fetch('/t',{cache:'no-store'});draw(await r.json());}catch(e){document.body.classList.add('dn');$('lk').classList.remove('on');$('lt').textContent='ESP در دسترس نیست';}
 setTimeout(poll,300);}
poll();
</script></body></html>)HTML";

/* ==================== Vazirmatn Font (PROGMEM) ==================== */
/* [EN] Vazirmatn v33.0.3 (SIL OFL 1.1, github.com/rastikerdar/vazirmatn), variable wght 400..700,
        subset to the glyphs used by the panel, plus added arrows U+2190/U+2192 drawn on the minus
        stroke (modified font, OFL allows it); embedded because the AP has no internet.
   [FA] فونت وزیرمتن نسخه 33.0.3 (مجوز SIL OFL 1.1)، وزن متغیر ۴۰۰ تا ۷۰۰، فقط حروف مورد استفادهٔ پنل؛
        به‌علاوهٔ فلش‌های ← و → که روی ضخامت علامت منها رسم و اضافه شده‌اند (مجاز در OFL)؛
        داخل برنامه جاسازی شده چون اکسس‌پوینت اینترنت ندارد. */
static const char ESP_PANEL_FONT_CSS[] PROGMEM =
"@font-face{font-family:Vazirmatn;font-weight:400 700;font-display:swap;src:url(data:font/woff2;base64,"
"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"
"3cY22dHhAAAAAA=="
") format('woff2')}";

/* ==================== Byte Helpers ==================== */

/**
 * @brief  [EN] Read a little-endian u32 from a byte buffer.
 *         [FA] خواندن عدد u32 اندیان‌کوچک از بافر بایتی.
 * @param  uint8_t__ptr_buffer [EN] Source buffer, at least offset+4 bytes / [FA] بافر مبدا، حداقل offset+4 بایت
 * @param  uint8_t__offset     [EN] Byte offset, 0..108 (payload max 112) / [FA] آفست بایتی، ۰ تا ۱۰۸ (حداکثر payload ۱۱۲)
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
 * @param  uint8_t__type        [EN] Message type (0x01 SET_PARAM / 0x02 GET_PARAMS) / [FA] نوع پیام (0x01 یا 0x02)
 * @param  uint8_t__ptr_payload [EN] Payload bytes, may be NULL when len = 0 / [FA] بایت‌های payload؛ برای طول صفر می‌تواند NULL باشد
 * @param  uint8_t__len         [EN] Payload length, 0..112 bytes / [FA] طول payload، ۰ تا ۱۱۲ بایت
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
 * @param  uint8_t__id     [EN] Parameter ID, 0..19 / [FA] شناسه پارامتر، ۰ تا ۱۹
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
