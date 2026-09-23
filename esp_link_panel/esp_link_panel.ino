/**
 * @file    esp_link_panel.ino
 * @brief   [EN] ESP-side ESP-Link bridge: exchanges binary frames with the STM32
 *               over UART (115200 8N1, ESP_AGENT_SPEC.md v1) and exposes a small
 *               dark web panel for live telemetry and the 14 runtime parameters.
 *          [FA] پل ESP-Link سمت ESP: تبادل فریم باینری با STM32 روی UART
 *               (115200 8N1، مطابق ESP_AGENT_SPEC.md نسخه ۱) و یک پنل وب دارک ساده
 *               برای تله‌متری زنده و ۱۴ پارامتر زمان اجرا.
 *
 * @note    [EN] Wiring: STM32 PA9 (TX) -> ESP RX, STM32 PA10 (RX) <- ESP TX, common GND.
 *               STM32 PA8 drives ESP CH_PD/EN; this sketch never touches that line.
 *               Wi-Fi AP "ChangeOver-ESP", password "123456789", panel at http://192.168.4.1
 *               No flash storage, no cloud: only MCU <-> ESP data exchange.
 *          [FA] سیم‌بندی: PA9 به RX ماژول، PA10 به TX ماژول، زمین مشترک.
 *               پایه CH_PD/EN را STM32 (PA8) کنترل می‌کند؛ این برنامه به آن دست نمی‌زند.
 *               وای‌فای "ChangeOver-ESP" با رمز "123456789"، پنل در http://192.168.4.1
 *               بدون حافظه فلش و بدون اینترنت: فقط تبادل داده بین MCU و ESP.
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
#define ESP_LINK_BAUD_RATE          115200u
#define ESP_LINK_RX_BUFFER_SIZE     512u
#define ESP_LINK_SOF_BYTE0          0xAAu
#define ESP_LINK_SOF_BYTE1          0x55u
#define ESP_LINK_HEADER_SIZE        4u
#define ESP_LINK_MAX_PAYLOAD        96u
#define ESP_LINK_TLM_SIZE           84u
#define ESP_LINK_TLM_FIELD_OFFSET   4u
#define ESP_LINK_TLM_FIELD_COUNT    20u
#define ESP_LINK_PARAM_ITEM_SIZE    5u
#define ESP_LINK_TIMEOUT_MS         1000u
#define ESP_LINK_TX_INTERVAL_MS     120u

/* ==================== Message Types ==================== */
#define ESP_MSG_SET_PARAM           0x01u
#define ESP_MSG_GET_PARAMS          0x02u
#define ESP_MSG_TLM_LIVE            0x10u
#define ESP_MSG_PARAM_REPORT        0x11u
#define ESP_MSG_PARAMS_BULK         0x12u

/* ==================== Parameter Constants ==================== */
#define ESP_PARAM_COUNT             14u
#define ESP_PARAM_CHG1_ENABLE       12u
#define ESP_PARAM_CHG2_ENABLE       13u
#define ESP_PARAM_SIGNED_FIRST      4u
#define ESP_PARAM_SIGNED_LAST       6u

/* ==================== Wi-Fi / HTTP Constants ==================== */
#define ESP_WIFI_AP_SSID            "ChangeOver-ESP"
#define ESP_WIFI_AP_PASS            "123456789"
#define ESP_HTTP_PORT               80
#define ESP_JSON_BUFFER_SIZE        640u

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
static const int32_t INT32_T__G__ParamMin[ESP_PARAM_COUNT] = {   0,    0,  100,  100, -2000, -2000, -2000, 0, 0,  1, 100, 100, 0, 0 };
static const int32_t INT32_T__G__ParamMax[ESP_PARAM_COUNT] = { 255,  255, 3000, 3000,  2000,  2000,  2000, 1, 1, 10, 999, 999, 1, 1 };

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

/* ==================== HTTP ==================== */
static esp_web_server_t ESP_WEB_SERVER_T__G__Server(ESP_HTTP_PORT);
static char CHAR__G__JsonBuffer[ESP_JSON_BUFFER_SIZE];

/* ==================== Web Panel (PROGMEM) ==================== */
static const char ESP_PANEL_HTML[] PROGMEM = R"HTML(<!doctype html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>ChangeOver</title><style>
:root{--bg:#0b0e14;--card:#131824;--line:#1f2636;--tx:#e6e9f0;--mu:#7d8699;--ac:#4f8cff;--ok:#2ecc8f;--wa:#f5b942;--er:#ff5c6c}
*{box-sizing:border-box;margin:0}body{background:var(--bg);color:var(--tx);font:14px/1.4 system-ui,-apple-system,Segoe UI,Roboto,sans-serif;padding:16px;max-width:1100px;margin:auto}
header{display:flex;align-items:center;gap:12px;margin-bottom:16px}h1{font-size:18px;font-weight:600;flex:1}
.pill{display:flex;align-items:center;gap:8px;background:var(--card);border:1px solid var(--line);border-radius:99px;padding:6px 12px;font-size:12px;color:var(--mu)}
.dot{width:8px;height:8px;border-radius:50%;background:var(--er)}.on .dot{background:var(--ok);box-shadow:0 0 8px var(--ok)}
.grid{display:grid;gap:12px}.g5{grid-template-columns:repeat(auto-fit,minmax(150px,1fr))}.g2{grid-template-columns:repeat(auto-fit,minmax(320px,1fr))}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:14px}
.lbl{color:var(--mu);font-size:12px}.val{font-size:24px;font-weight:600;font-variant-numeric:tabular-nums}.val small{font-size:12px;color:var(--mu);margin-left:4px}
.hd{display:flex;justify-content:space-between;align-items:center;margin-bottom:10px}h2{font-size:15px;font-weight:600}
.tag{font-size:11px;padding:3px 8px;border-radius:6px;background:#1c2334;color:var(--mu)}.tag.g{color:var(--ok);background:#12301f}.tag.r{color:var(--er);background:#3a1820}.tag.y{color:var(--wa);background:#382b12}
.bar{height:6px;background:#0d111a;border-radius:9px;overflow:hidden;margin:10px 0 4px}.bar i{display:block;height:100%;width:0;background:var(--ac);transition:width .3s}
.chain{display:grid;grid-template-columns:repeat(4,1fr);gap:6px;margin:12px 0}.chain div{background:#0f131d;border-radius:8px;padding:8px;text-align:center}
.chain b{display:block;font-variant-numeric:tabular-nums}.chain small{color:var(--mu);font-size:10px}
button{font:inherit;border:0;border-radius:9px;padding:9px 14px;cursor:pointer;color:#fff;background:var(--ac)}button:active{transform:scale(.97)}
.tg{width:100%;font-weight:600}.tg.cut{background:var(--er)}.tg.run{background:var(--ok)}
table{width:100%;border-collapse:collapse}td{padding:8px 6px;border-top:1px solid var(--line)}td:first-child{font-family:ui-monospace,monospace;font-size:12px}
input{width:90px;background:#0d111a;border:1px solid var(--line);color:var(--tx);border-radius:8px;padding:7px;font:inherit}
.ap{font-variant-numeric:tabular-nums;font-weight:600}.sec{margin:18px 0 8px;color:var(--mu);font-size:12px;text-transform:uppercase;letter-spacing:.08em}
.warn{display:none;background:#3a1820;color:var(--er);border-radius:10px;padding:10px;margin-bottom:12px}
body.off main{opacity:.4;filter:grayscale(1)}
</style></head><body><header><h1>ChangeOver · ESP Link</h1><div class="pill" id="lk"><span class="dot"></span><span id="lt">Connecting…</span></div></header>
<div class="warn" id="bl">Battery lost fault active (fault bit 6)</div><main>
<div class="grid g5" id="vg"></div><div class="sec">Chargers</div><div class="grid g2" id="cg"></div>
<div class="sec">Parameters</div><div class="card"><table id="pt"></table></div></main>
<script>
const $=i=>document.getElementById(i);
const ST=['OFF','BULK','ABSORB','FLOAT','BRINGUP','JIT WAIT','INPUT WAIT','FINAL FAULT','BAT LOST'];
const P=[['CUR1_OFFSET_COUNTS','cnt',0,255],['CUR2_OFFSET_COUNTS','cnt',0,255],['CUR1_GAIN_PERMILLE','‰',100,3000],['CUR2_GAIN_PERMILLE','‰',100,3000],
['VIN_OFFSET_MV','mV',-2000,2000],['V24_OFFSET_MV','mV',-2000,2000],['V12_OFFSET_MV','mV',-2000,2000],['FILTER_MEDIAN3','',0,1],['FILTER_AVERAGE','',0,1],
['FILTER_WINDOW','smp',1,10],['CHG_EFF_UP_PERMILLE','‰',100,999],['CHG_EFF_DN_PERMILLE','‰',100,999],['CHG1_ENABLE','',0,1],['CHG2_ENABLE','',0,1]];
const V=[['Input 24V',14],['Pack 24V',15],['Mid node 12V',16],['Upper batt',18],['Lower batt',17]];
let D=null;
$('vg').innerHTML=V.map((v,i)=>`<div class="card"><div class="lbl">${v[0]}</div><div class="val"><span id="v${i}">—</span><small>V</small></div></div>`).join('');
$('cg').innerHTML=[1,2].map(n=>`<div class="card"><div class="hd"><h2>Charger ${n} <span class="lbl">${n==1?'upper':'lower'}</span></h2><span class="tag" id="st${n}">—</span></div>
<div class="val"><span id="ie${n}">—</span><small>mA est. battery</small></div><div class="bar"><i id="db${n}"></i></div><div class="lbl">Duty <b id="du${n}">—</b></div>
<div class="chain">${['Raw|cnt','Shunt|µV','Unfilt|mA','Filt|mA'].map((s,k)=>`<div><small>${s.split('|')[0]}</small><b id="c${n}${k}">—</b><small>${s.split('|')[1]}</small></div>`).join('')}</div>
<button class="tg" id="tg${n}" onclick="tog(${n})">—</button></div>`).join('');
$('pt').innerHTML=P.map((p,i)=>`<tr><td>${i} · ${p[0]}</td><td class="lbl">${p[2]}…${p[3]} ${p[1]}</td><td class="ap" id="a${i}">—</td><td>${p[3]==1&&p[2]==0
?`<button id="b${i}" onclick="flip(${i})">Toggle</button>`:`<input type="number" id="i${i}" min="${p[2]}" max="${p[3]}" onkeydown="if(event.key=='Enter')setp(${i})"> <button onclick="setp(${i})">Set</button>`}</td></tr>`).join('');
function send(id,v){fetch('/s?id='+id+'&v='+v,{method:'POST'});$('a'+id).textContent='…';}
function setp(i){const e=$('i'+i),v=Math.round(+e.value);if(e.value===''||isNaN(v))return;send(i,Math.min(P[i][3],Math.max(P[i][2],v)));e.value='';e.blur();}
function flip(i){const c=D&&D.p[i];send(i,c===1?0:1);}
function tog(n){const c=D&&D.p[11+n];send(11+n,c===0?1:0);}
function f2(mv){return (mv/1000).toFixed(2);}
function draw(d){D=d;const t=d.t,on=d.on==1;document.body.classList.toggle('off',!on);$('lk').classList.toggle('on',on);
$('lt').textContent=on?`Online · seq ${d.seq} · ${d.age} ms`:(d.n?'Link down · last data greyed':'Waiting for STM32…');
V.forEach((v,i)=>$('v'+i).textContent=f2(t[v[1]]));$('bl').style.display=(t[19]&64)?'block':'none';
[1,2].forEach(n=>{const b=n==1?0:7,s=t[b+6],en=d.p[11+n];const st=$('st'+n);st.textContent=ST[s]||('#'+s);
st.className='tag '+(s>=7?'r':s==0?'':s>=4?'y':'g');$('ie'+n).textContent=t[b+4];$('du'+n).textContent=(t[b+5]/10).toFixed(1)+' %';
$('db'+n).style.width=Math.min(100,t[b+5]/10)+'%';[0,1,2,3].forEach(k=>$('c'+n+k).textContent=t[b+k]);
const g=$('tg'+n);g.textContent=en===0?'Reconnect charger '+n:'Cut charger '+n;g.className='tg '+(en===0?'run':'cut');});
P.forEach((p,i)=>{if(d.q&(1<<i))return;const a=$('a'+i),v=d.p[i];a.textContent=v===null?'—':v;const b=$('b'+i);if(b)b.textContent=v===1?'ON':v===0?'OFF':'Toggle';});}
async function poll(){try{const r=await fetch('/t',{cache:'no-store'});draw(await r.json());}catch(e){document.body.classList.add('off');$('lk').classList.remove('on');$('lt').textContent='ESP not reachable';}
setTimeout(poll,300);}
poll();
</script></body></html>)HTML";

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
 *              (charger cut/reconnect first). Never blocks.
 *         [FA] ارسال حداکثر یک فرمان صف‌شده در هر ESP_LINK_TX_INTERVAL_MS
 *              (اول قطع/وصل شارژر). هیچ‌وقت مسدود نمی‌کند.
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

    /* [EN] Order: 12, 13, then 0..11 / [FA] ترتیب: ۱۲، ۱۳، سپس ۰ تا ۱۱ */
    for (uint8_t__step = 0u; uint8_t__step < ESP_PARAM_COUNT; uint8_t__step++)
    {
        uint8_t uint8_t__shifted = (uint8_t)(uint8_t__step + ESP_PARAM_CHG1_ENABLE);
        uint8_t uint8_t__id = (uint8_t)(uint8_t__shifted % ESP_PARAM_COUNT);
        if (BOOL__G__TxParamPending[uint8_t__id])
        {
            BOOL__G__TxParamPending[uint8_t__id] = false;
            func__Esp_SendSetParam(uint8_t__id, UINT32_T__G__TxParamValue[uint8_t__id]);
            return;
        }
    }

    if (BOOL__G__TxGetPending)
    {
        BOOL__G__TxGetPending = false;
        func__Esp_WriteFrame(ESP_MSG_GET_PARAMS, NULL, 0u);
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
           [FA] پارامترهای STM32 فقط در RAM هستند: مقادیری که کاربر در این نشست داده دوباره ارسال شوند. */
        if (bool__seqRestart)
        {
            for (uint8_t__index = 0u; uint8_t__index < ESP_PARAM_COUNT; uint8_t__index++)
            {
                if (BOOL__G__ParamUserSet[uint8_t__index])
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
 * @brief  [EN] GET /t : compact JSON snapshot {on,age,seq,fl,n,q,t[20],p[14]}.
 *              t = TLM u32 fields in spec order (offset 4..80); p = applied params or null.
 *         [FA] مسیر GET /t : خلاصه JSON فشرده {on,age,seq,fl,n,q,t[20],p[14]}.
 *              t فیلدهای u32 تله‌متری به ترتیب سند (آفست ۴ تا ۸۰)؛ p مقدار اعمال‌شده یا null.
 * @return [EN] None / [FA] ندارد
 */
static void func__Esp_HttpTelemetry(void)
{
    uint32_t uint32_t__nowMs = (uint32_t)millis();
    uint32_t uint32_t__ageMs = uint32_t__nowMs - UINT32_T__G__LastTlmMs;
    bool bool__online = BOOL__G__TlmSeen && (uint32_t__ageMs <= ESP_LINK_TIMEOUT_MS);
    uint32_t uint32_t__pendingMask = 0u;
    uint8_t uint8_t__index;
    size_t size_t__used;

    for (uint8_t__index = 0u; uint8_t__index < ESP_PARAM_COUNT; uint8_t__index++)
    {
        if (BOOL__G__TxParamPending[uint8_t__index])
        {
            uint32_t__pendingMask |= (1UL << uint8_t__index);
        }
    }

    size_t__used = (size_t)snprintf(CHAR__G__JsonBuffer, ESP_JSON_BUFFER_SIZE,
        "{\"on\":%u,\"age\":%lu,\"seq\":%u,\"fl\":%u,\"n\":%lu,\"q\":%lu,\"t\":[",
        bool__online ? 1u : 0u, (unsigned long)uint32_t__ageMs, (unsigned int)UINT16_T__G__TlmSeq,
        (unsigned int)UINT8_T__G__TlmFlags, (unsigned long)UINT32_T__G__TlmFrameCount,
        (unsigned long)uint32_t__pendingMask);

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
