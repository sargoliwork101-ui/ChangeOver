/**
 * @file    esp_link_panel.ino
 * @brief   [EN] ESP-side ESP-Link bridge: exchanges binary frames with the STM32
 *               over UART (921600 8N1, ESP_AGENT_SPEC.md v1.3 incl. 5.3 formulas and 5.4 CAL_REFERENCE) and exposes a small
 *               dark RTL web panel (Vazirmatn) with three tabs: status, calibration, manual test;
 *               the section 5.3 conversion formulas are shown with live values, plus calibration
 *               helpers (zero from raw, voltage offset from a multimeter, and the v1.3 CAL_REFERENCE card:
 *               the typed DMM mA goes to the STM32, which computes gain or ETA from its live snapshot),
 *               a live 36 s filter chart, a per-channel re-apply (JIT re-arm) button in manual mode, and a
 *               bench tab (spec 5.5) that runs tests A-D itself, reads every TLM frame through the /m
 *               statistics window and prints copyable result blocks (never auto-applies a calibration).
 *               Engineer mode switch (default off, remembered per device): off = status (read-only) and
 *               the CAL card only; on = manual/bench tabs, the parameter table, helpers and formulas.
 *          [FA] پل ESP-Link سمت ESP: تبادل فریم باینری با STM32 روی UART
 *               (921600 8N1، مطابق ESP_AGENT_SPEC.md نسخه ۱.۳) و یک پنل وب دارک ساده
 *               راست‌به‌چپ با فونت وزیرمتن و سه تب: وضعیت، کالیبراسیون، تست دستی؛
 *               فرمول‌های تبدیل بخش 5.3 با مقادیر زنده، دستیار کالیبراسیون (صفر از raw، آفست ولتاژ از
 *               مولتی‌متر، و کارت CAL_REFERENCE نسخه ۱.۳: عدد مولتی‌متر به STM32 می‌رود و خودش گین یا
 *               ضریب تبدیل را از داده‌های زنده حساب می‌کند)، نمودار زندهٔ ۳۶ ثانیه‌ای فیلتر و دکمهٔ «اعمال مجدد
 *               duty» (مسلح‌سازی بعد از JIT) برای هر کانال در مود دستی، و تب «تست بنچ» (بخش 5.5) که تست‌های
 *               A تا D را خودش اجرا می‌کند، تک‌تک فریم‌های TLM را از پنجرهٔ آمار /m می‌خواند و بلوک متن
 *               کپی‌شدنی می‌دهد (هیچ کالیبراسیونی را خودکار اعمال نمی‌کند).
 *               کلید «حالت مهندس» (پیش‌فرض خاموش، روی هر دستگاه به یاد می‌ماند): خاموش = وضعیت (فقط خواندنی) و
 *               فقط کارت CAL؛ روشن = تب‌های تست دستی و بنچ، جدول پارامترها، دستیارها و فرمول‌ها.
 *
 * @note    [EN] Wiring: STM32 PA9 (TX) -> ESP RX, STM32 PA10 (RX) <- ESP TX, common GND.
 *               STM32 PA8 drives ESP CH_PD/EN; this sketch never touches that line.
 *               Wi-Fi AP "ChangeOver-ESP", password "123456789", panel at http://192.168.4.1
 *               No flash storage, no cloud: only MCU <-> ESP data exchange.
 *               Manual test mode (ID 19): a GET_PARAMS keepalive is sent every 1 s while manual
 *               mode is active (TLM flag b5 or param 19), also with a background tab (spec 5.2).
 *               Safety: if no browser has polled /t for 10 s (or none since the ESP booted), the ESP
 *               sends ID 19 = 0 every 1 s until the STM32 reports manual off, so a closed panel never
 *               leaves a channel on an unregulated duty; the STM32 3 s dead-man still covers an ESP
 *               hang or a broken link.
 *          [FA] سیم‌بندی: PA9 به RX ماژول، PA10 به TX ماژول، زمین مشترک.
 *               پایه CH_PD/EN را STM32 (PA8) کنترل می‌کند؛ این برنامه به آن دست نمی‌زند.
 *               وای‌فای "ChangeOver-ESP" با رمز "123456789"، پنل در http://192.168.4.1
 *               بدون حافظه فلش و بدون اینترنت: فقط تبادل داده بین MCU و ESP.
 *               مود تست دستی (شناسه ۱۹): تا وقتی مود دستی فعال است (پرچم b5 یا پارامتر ۱۹) هر ۱ ثانیه
 *               یک GET_PARAMS فرستاده می‌شود، حتی با تب پس‌زمینه (بخش 5.2 سند). ایمنی: اگر ۱۰ ثانیه
 *               هیچ مرورگری /t را نخواند (یا از بوت ESP هنوز نخوانده باشد)، ESP هر ۱ ثانیه شناسهٔ ۱۹ = 0 را
 *               می‌فرستد تا STM32 خاموشی مود دستی را گزارش کند؛ پس پنل بسته هرگز کانال را روی duty بدون
 *               تنظیم رها نمی‌کند. هنگ ESP یا قطع لینک را dead-man سه‌ثانیه‌ای STM32 پوشش می‌دهد.
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
#define ESP_LINK_BROWSER_LOST_MS    10000u
#define ESP_LINK_PARAM_REFRESH_MS   30000u

/* ==================== Message Types ==================== */
#define ESP_MSG_SET_PARAM           0x01u
#define ESP_MSG_GET_PARAMS          0x02u
#define ESP_MSG_CAL_REFERENCE       0x03u
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
#define ESP_PARAM_CUR1_GAIN         2u
#define ESP_PARAM_CHG_ETA1          9u

/* ==================== CAL_REFERENCE (v1.3, spec 5.4) ==================== */
/* [EN] Targets 0/1 = GAIN ch1/ch2, 2/3 = ETA ch1/ch2. The STM32 replies with PARAM_REPORT
        (gain path: gain id, then eta id = 0) or stays silent on rejection.
   [FA] هدف ۰/۱ = گین کانال ۱/۲، ۲/۳ = η کانال ۱/۲. STM32 با PARAM_REPORT پاسخ می‌دهد
        (مسیر گین: id گین، بعد id η = ۰) یا در صورت رد هیچ پاسخی نمی‌دهد. */
#define ESP_CAL_TARGET_COUNT        4u
#define ESP_CAL_MIN_REF_MA          50
#define ESP_CAL_MAX_REF_MA          5000
#define ESP_CAL_REPLY_TIMEOUT_MS    1500u
#define ESP_CAL_PAYLOAD_SIZE        5u

/* ==================== Bench Statistics Window (spec 5.5) ==================== */
/* [EN] The browser polls /t only every 300 ms, so the bench tests read min/max/sum of EVERY TLM
        frame (10 Hz) from this window: POST /m restarts it, GET /m reads it. Fields (t[] index):
        raw1 0, unfiltered1 2, filtered1 3, iest1 4, duty1 5, same for ch2 at 7..12, Vin 14,
        Vlow 17, Vhigh 18. The count stops at 60000 frames (100 min) so the u32 sums never wrap.
   [FA] مرورگر فقط هر ۳۰۰ms /t را می‌خواند؛ پس تست‌های بنچ کمینه/بیشینه/مجموعِ تک‌تک فریم‌های TLM
        (۱۰ هرتز) را از این پنجره می‌گیرند: POST /m آن را از نو شروع و GET /m آن را می‌خواند. فیلدها
        (اندیس t[]): raw1 0، بدون فیلتر ۱ 2، فیلترشده ۱ 3، iest1 4، duty1 5، همین‌ها برای کانال ۲ در
        7..12، Vin 14، Vlow 17، Vhigh 18. شمارش در ۶۰۰۰۰ فریم (۱۰۰ دقیقه) می‌ایستد تا مجموع u32 سرریز نشود. */
#define ESP_STAT_FIELD_COUNT        13u
#define ESP_STAT_MAX_FRAMES         60000u

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

/* ==================== CAL_REFERENCE States ==================== */
typedef enum
{
    ESP_CAL_IDLE = 0,      /* [EN] Nothing run yet / [FA] هنوز اجرا نشده */
    ESP_CAL_WAITING,       /* [EN] Queued or sent, reply awaited / [FA] صف یا ارسال‌شده، منتظر پاسخ */
    ESP_CAL_APPLIED,       /* [EN] PARAM_REPORT received / [FA] PARAM_REPORT رسید */
    ESP_CAL_REJECTED       /* [EN] No reply within the timeout / [FA] در مهلت پاسخی نیامد */
} esp_cal_state_t;

/* ==================== Parameter Ranges (STM32 clamps too) ==================== */
/* [EN] ID: 0..1 offset, 2..3 gain, 4..6 mV offset (signed), 7 median 1/3/5, 8 avg window, 9..10 ETA conversion (v1.3, 0 = identity),
        11..12 charger enable, 13..14 duty ceiling, 15/17 fixed-duty on, 16/18 fixed/manual duty,
        19 manual test mode (v1.2).
   [FA] شناسه: ۰..۱ آفست، ۲..۳ گین، ۴..۶ آفست mV علامت‌دار، ۷ مدین ۱/۳/۵، ۸ پنجره میانگین، ۹..۱۰ ضریب تبدیل η (v1.3، صفر = همانی)،
        ۱۱..۱۲ قطع/وصل شارژر، ۱۳..۱۴ سقف duty، ۱۵/۱۷ مود duty فیکس، ۱۶/۱۸ duty فیکس/دستی،
        ۱۹ مود تست دستی (نسخه ۱.۲). */
static const int32_t INT32_T__G__ParamMin[ESP_PARAM_COUNT] = {   0,   0,  100,  100, -2000, -2000, -2000, 1,  1,   0,   0, 0, 0,   0,   0, 0,   0, 0,   0, 0 };
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
static uint32_t UINT32_T__G__LastParamRefreshMs = 0u;

/* [EN] One CAL_REFERENCE at a time: target, typed mA, state, send time, applied value, run counter.
   [FA] هر بار فقط یک CAL_REFERENCE: هدف، عدد mA، وضعیت، زمان ارسال، مقدار اعمال‌شده، شمارندهٔ اجرا. */
static uint8_t         UINT8_T__G__CalTarget = 0u;
static uint32_t        UINT32_T__G__CalRefMa = 0u;
static esp_cal_state_t ESP_CAL_STATE_T__G__CalState = ESP_CAL_IDLE;
static bool            BOOL__G__CalTxPending = false;
static uint32_t        UINT32_T__G__CalSentMs = 0u;
static uint32_t        UINT32_T__G__CalValue = 0u;
static uint32_t        UINT32_T__G__CalRun = 0u;

/* [EN] Bench statistics window (see ESP_STAT_FIELD_COUNT) / [FA] پنجرهٔ آمار بنچ */
static const uint8_t UINT8_T__G__StatField[ESP_STAT_FIELD_COUNT] = { 0, 2, 3, 4, 5, 7, 9, 10, 11, 12, 14, 17, 18 };
static uint32_t UINT32_T__G__StatSum[ESP_STAT_FIELD_COUNT];
static uint32_t UINT32_T__G__StatMin[ESP_STAT_FIELD_COUNT];
static uint32_t UINT32_T__G__StatMax[ESP_STAT_FIELD_COUNT];
static uint32_t UINT32_T__G__StatCount = 0u;

/* [EN] Send priority: charger cut, manual mode, manual duties, then the rest.
   [FA] اولویت ارسال: قطع شارژر، مود دستی، duty دستی، سپس بقیه. */
static const uint8_t UINT8_T__G__TxOrder[ESP_PARAM_COUNT] = { 11, 12, 19, 16, 18, 15, 17, 13, 14, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };

/* ==================== HTTP ==================== */
static esp_web_server_t ESP_WEB_SERVER_T__G__Server(ESP_HTTP_PORT);
static char CHAR__G__JsonBuffer[ESP_JSON_BUFFER_SIZE];

/* ==================== Web Panel (PROGMEM) ==================== */
static const char ESP_PANEL_HTML[] PROGMEM = R"HTML(<!doctype html><html lang="fa" dir="rtl"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>ChangeOver</title><link rel="stylesheet" href="/f.css?v=2"><style>
:root{--bg:#0b0e14;--cd:#121722;--ln:#1e2533;--tx:#e7eaf0;--mu:#8089a0;--ac:#4f8cff;--ok:#2ecc8f;--wa:#f5b942;--er:#ff5c6c}
*{box-sizing:border-box;margin:0}
body{background:var(--bg);color:var(--tx);font:14px/1.6 Vazirmatn,Tahoma,sans-serif;max-width:1440px;margin:auto;padding:0 12px 24px}
button,input{font:inherit;color:inherit}
.n{direction:ltr;unicode-bidi:isolate;font-variant-numeric:tabular-nums}
header{display:flex;align-items:center;justify-content:space-between;padding:14px 2px 10px}
h1{font-size:17px;font-weight:700}
.lk{display:flex;align-items:center;gap:7px;font-size:12px;color:var(--mu)}
.lk i{width:8px;height:8px;border-radius:50%;background:var(--er)}.lk.on i{background:var(--ok);box-shadow:0 0 6px var(--ok)}
nav{display:flex;gap:4px;background:var(--cd);border:1px solid var(--ln);border-radius:12px;padding:4px;position:sticky;top:6px;z-index:5}
nav button{flex:1;border:0;background:none;border-radius:9px;padding:8px;cursor:pointer;color:var(--mu);font-weight:500}
nav button.a{background:#1f2738;color:var(--tx)}nav button.m.a{background:#3a2a10;color:var(--wa)}
section{margin-top:12px}
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
input[type=number]{width:92px;background:#0c1018;border:1px solid var(--ln);border-radius:8px;padding:5px 7px;direction:ltr}
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
.sb2{background:#243052;color:#cfe0ff}.rm{background:#243052;color:#cfe0ff;width:100%;margin-top:8px}.rm.j{background:#b8323f;color:#fff}.rm:disabled{opacity:.4;cursor:default}
canvas{width:100%;height:140px;display:block;background:#0c1018;border-radius:10px;margin-top:10px;direction:ltr}
.lg{display:flex;gap:14px;flex-wrap:wrap;font-size:12px;color:var(--mu);margin-top:6px}.lg i{display:inline-block;width:12px;height:3px;border-radius:2px;margin-left:5px;vertical-align:middle}
.ti2{display:flex;justify-content:space-between;align-items:center}
.ca{border-top:1px solid var(--ln);margin-top:4px;padding-top:12px}.ca .ti{margin-bottom:4px;color:var(--tx)}
.cg{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:10px}
.cb{border:0;border-radius:10px;padding:8px 10px;background:#243052;color:#cfe0ff;cursor:pointer;display:flex;justify-content:space-between;align-items:center}.cb b{font-weight:500;color:#8fb3ff}
.cb.lo{background:#1c2130;color:var(--mu)}.cb:disabled{opacity:.45;cursor:default}
.cm{margin-top:10px;min-height:1.6em}.cm.g{color:var(--ok)}.cm.r{color:var(--er)}.wr{color:var(--wa)}
body.dn #sh,body.dn #ch{opacity:.45;filter:grayscale(1)}
.bsb{position:sticky;top:6px;z-index:4;border-color:var(--wa)}.stp{background:var(--er)}
.bctl{display:flex;flex-wrap:wrap;gap:10px;align-items:center;margin-top:10px}.bctl .sg button{width:auto;padding:3px 10px}.bctl input{width:120px}input.dl{background:#0c1018;border:1px solid var(--ln);border-radius:8px;padding:5px 7px;direction:ltr}
.bqr2{display:flex;gap:8px;margin-top:10px}body.br .brun{opacity:.4;pointer-events:none}
.bq{background:#0c1018;border:1px solid var(--wa);border-radius:10px;padding:10px;margin-top:10px}.bqr{display:flex;flex-wrap:wrap;gap:10px;align-items:flex-end;margin-top:8px}.bqr label{display:flex;flex-direction:column;gap:3px}
.tw{overflow:auto;max-height:280px;margin:6px 0 10px}.bt2 th{position:sticky;top:0;background:var(--cd)}body:not(.br) .stp{display:none}.bt2{font-size:12px;direction:ltr;white-space:nowrap}.bt2 th{color:var(--mu);font-weight:500;text-align:left;padding:4px 6px}.bt2 td{padding:4px 6px;text-align:left!important}
.bsum{font-size:12px;direction:ltr;text-align:left;line-height:1.9;margin-bottom:8px}.okc{color:var(--ok)}.erc{color:var(--er)}
.bxw textarea{width:100%;height:150px;background:#0c1018;color:#9aa6c0;border:1px solid var(--ln);border-radius:10px;padding:8px;font:11px/1.5 monospace;direction:ltr;margin-top:6px}
/* حالت مهندس: پیش‌فرض خاموش؛ ابزارها فقط پنهان‌اند */
.hr{display:flex;align-items:center;gap:12px}
.eg{display:flex;align-items:center;gap:6px;font-size:12px;color:var(--mu);border:1px solid var(--ln);background:none;border-radius:8px;padding:3px 8px;cursor:pointer}
.eg i{width:26px;height:14px;border-radius:7px;background:#252e42;position:relative}.eg i:after{content:'';position:absolute;top:2px;right:2px;width:10px;height:10px;border-radius:50%;background:var(--mu);transition:right .2s}
body.eng .eg{color:var(--wa);border-color:#5a4418}body.eng .eg i{background:var(--wa)}body.eng .eg i:after{right:14px;background:#fff}
body:not(.eng) .en{display:none}
.cc .ca{border-top:0;margin-top:0;padding-top:0}.stp2{background:var(--er);white-space:nowrap}
/* صفحهٔ واحد لپ‌تاپ: نوار مشترک بالا، دو ستون شارژر، تست بنچ پایین */
.vt{background:#0c1018;border-radius:12px;padding:10px 8px;display:flex;flex-direction:column;align-items:center;gap:4px}.vt .fx{text-align:center;max-width:100%}
.vc{justify-content:center}.vc input[type=number]{width:96px}
.fl{margin-top:0;padding-top:0;border-top:0}#sh .hd{flex-wrap:wrap;gap:8px}
.sec{display:flex;justify-content:space-between;align-items:center;font-size:13px;font-weight:700;color:var(--mu);margin:14px 0 6px;padding-top:12px;border-top:1px solid var(--ln)}
.frr{display:grid;grid-template-columns:1fr 1fr;gap:0 28px}.frr .rw:first-of-type{border-top:1px solid var(--ln)}.fxw{margin-top:6px;font-size:12px}
.kc{font-size:11px;margin-top:6px}.cr{margin-top:10px;flex-wrap:wrap}.cr .cb{flex:1 1 120px}.cr input[type=number]{width:120px}
.off2{background:#5b1c26;white-space:nowrap}.mbar{border-color:#5a4418}
.mn.dis{opacity:.4}.mn.dis input,.mn.dis button{pointer-events:none}
.h2{font-size:15px;margin:22px 2px 4px}#t3>.bk>.ti{cursor:pointer;margin-bottom:0}#t3>.bk>.ti:before{content:'+ ';color:var(--ac)}#t3>.bk.op>.ti:before{content:'- '}
#t3>.bk.op>.ti{margin-bottom:10px}#t3>.bk:not(.op)>:not(.ti){display:none}
@media(max-width:1000px){.ch,.frr{grid-template-columns:1fr}.vs{grid-template-columns:repeat(3,1fr)}}
@media(max-width:640px){.cb{font-size:12px;padding:8px 7px}.cb span{white-space:nowrap}.vs{grid-template-columns:repeat(3,1fr)}.ch{grid-template-columns:1fr}.ms{grid-template-columns:repeat(3,1fr)}}
</style></head><body>
<header><h1>پنل ChangeOver</h1><div class="hr"><button class="eg" id="eg" title="نمایش ابزارهای مهندسی: مود دستی، سقف و duty فیکس، قطع کانال، تست‌های بنچ"><i></i>حالت مهندس</button><div class="lk" id="lk"><span id="lt">در حال اتصال…</span><i></i></div></div></header>
<div class="wn gb" id="mb"><div class="mx"><div><b>مود تست دستی فعال است</b> — شارژر خودکار و محافظت‌های باتری متوقف‌اند. <span id="ka"></span></div><button class="sb stp2" id="mx">خروج از مود دستی</button></div></div>
<main id="pg">
<div class="cd" id="sh"><div class="hd"><b>ولتاژها <span class="lb">· عدد مولتی‌متر (V) را کنار هر ولتاژ وارد کنید تا آفست آن کالیبره شود</span></b><div class="fl" id="fl"></div></div><div class="vs" id="vs"></div>
<div class="sec">فیلتر جریان <span class="lb">(مشترک هر دو کانال)</span></div><div class="frr" id="fg"></div><div class="fx fxw" id="ff"></div></div>
<div class="cd en mbar"><div class="mx"><div><b>مود تست دستی</b> <span class="ap n" id="a19"></span><div class="lb" id="mh">duty هر کانال مستقیم از شناسه‌های ۱۶ و ۱۸ اعمال می‌شود.</div></div><div class="ct"><button class="sb off2" id="ao">خاموش کردن هر دو کانال (duty = 0)</button><button class="sw w" id="s19">—</button></div></div>
<div class="lb" style="margin-top:6px">در این مود ماشین حالت شارژر، رمپ، پنجرهٔ اعتبار باتری، BAT_LOST و توقف ۱۴٫۴V همه خاموش‌اند. فقط حضور ورودی ۲۴V، قطع سخت‌افزاری JIT، قطع ۱۵٫۰V هر کانال، سقف duty و قطع کانال فعال می‌مانند.</div></div>
<div class="ch" id="ch"></div>
<section id="t3" class="en"></section>
</main>

<script>
const $=i=>document.getElementById(i);
const ST=['خاموش','Bulk','Absorb','Float','راه‌اندازی','انتظار JIT','انتظار ورودی','خطای نهایی','باتری قطع','دستی'];
const SC=['','g','g','g','y','r','y','r','r','y'];
/* ثابت‌های بخش 5.3 سند */
const K_UV=3300/4095*11/10*1000/101,K_MA=K_UV/10,K24=3300/4095*76000/6800,K12=3300/4095*41000/6800;
/* شناسه: [عنوان, واحد, کمینه, بیشینه, نوع(n عدد، b کلید، m مدین), توضیح] */
const P={0:['آفست صفر','count',0,255,'n','شمارش ADC در جریان صفر؛ داخل فرمول mA از raw کم می‌شود. کانال را بی‌جریان کنید و دکمهٔ صفر = raw فعلی را بزنید'],
1:['آفست صفر','count',0,255,'n','شمارش ADC در جریان صفر؛ داخل فرمول mA از raw کم می‌شود. کانال را بی‌جریان کنید و دکمهٔ صفر = raw فعلی را بزنید'],
2:['گین','‰',100,3000,'n','mA ≈ (raw − آفست) × 0.8776 × گین/۱۰۰۰؛ مقدار بنچ ۱۰۴۶. با دکمهٔ گین پایین از عدد مولتی‌متر کالیبره می‌شود'],
3:['گین','‰',100,3000,'n','mA ≈ (raw − آفست) × 0.8776 × گین/۱۰۰۰؛ مقدار بنچ ۱۳۰۳. با دکمهٔ گین پایین از عدد مولتی‌متر کالیبره می‌شود'],
4:['آفست ولتاژ ورودی','mV',-2000,2000,'n','بعد از تبدیل مقسم 69.2k/6.8k جمع می‌شود (علامت‌دار)'],
5:['آفست ولتاژ پک ۲۴V','mV',-2000,2000,'n','کالیبراسیون ولتاژ پک ۲۴V (علامت‌دار)'],
6:['آفست ولتاژ ۱۲V','mV',-2000,2000,'n','مقسم 34.2k/6.8k؛ روی باتری پایین و بالا (V24 − V12) هر دو اثر دارد'],
7:['پنجرهٔ مدین','',1,5,'m','مرحلهٔ اول فیلتر؛ ۱ = خاموش، ۳ = پیش‌فرض، ۵ پالس‌های دوتایی را هم حذف می‌کند'],
8:['پنجرهٔ میانگین','نمونه',1,10,'n','مرحلهٔ دوم فیلتر: میانگین آخرین W خروجی مدین؛ ۱ = خاموش، ۱۰ = پیش‌فرض'],
9:['ضریب تبدیل','‰',0,999,'n','صفر = همانی (عدد فیلترشده خودش جریان باتری است)؛ غیرصفر: iest = I × Vin × eta / (1000 × Vbat) با ولتاژهای زنده (Vbat = باتری بالا). با دکمهٔ ضریب تبدیل پایین تنظیم کنید، نه دستی'],
10:['ضریب تبدیل','‰',0,999,'n','صفر = همانی؛ غیرصفر: iest = I × Vin × eta / (1000 × Vbat) با ولتاژهای زنده (Vbat = باتری پایین). با دکمهٔ ضریب تبدیل پایین تنظیم کنید، نه دستی'],
13:['سقف duty','‰',0,500,'n','رمپ، تنظیم و مود فیکس همه به این سقف محدودند'],
14:['سقف duty','‰',0,500,'n','رمپ، تنظیم و مود فیکس همه به این سقف محدودند'],
15:['نگه‌داشت duty فیکس','',0,1,'b','PWM روی مقدار duty فیکس می‌ماند؛ توقف در ولتاژ ابزورب همچنان فعال است'],
16:['مقدار duty فیکس','‰',0,500,'n','در مود تست دستی همین مقدار duty کانال ۱ است'],
17:['نگه‌داشت duty فیکس','',0,1,'b','PWM روی مقدار duty فیکس می‌ماند؛ توقف در ولتاژ ابزورب همچنان فعال است'],
18:['مقدار duty فیکس','‰',0,500,'n','در مود تست دستی همین مقدار duty کانال ۲ است']};
/* ولتاژها: [عنوان, اندیس t, شناسهٔ آفست, ضریب مقسم] */
const V=[['ورودی',14,4,K24],['پک ۲۴V',15,5,K24],['نود ۱۲V',16,6,K12],['باتری بالا',18],['باتری پایین',17]];
let D=null;
const v2=mv=>(mv/1000).toFixed(2),pc=pm=>(pm/10).toFixed(1)+'%';
function send(id,v){const a=$('a'+id);if(a)a.textContent='…';fetch('/s?id='+id+'&v='+v,{method:'POST'}).then(r=>{if(!r.ok)throw 0;}).catch(()=>{if(a)a.textContent='خطا';});}
function num(id){const e=$('i'+id),p=P[id],v=Math.round(+e.value);if(e.value===''||isNaN(v))return;send(id,Math.min(p[3],Math.max(p[2],v)));e.value='';e.blur();}
function flip(id){const c=D&&D.p[id],nv=c===1?0:1;if(nv&&(id==15||id==17)&&!confirm('حلقهٔ تنظیم این کانال خاموش و PWM روی مقدار فیکس نگه داشته می‌شود. ادامه؟'))return;send(id,nv);}
function ctl(id){const p=P[id];
 if(p[4]=='b')return `<button class="sw w" id="b${id}" onclick="flip(${id})">—</button>`;
 if(p[4]=='m')return `<div class="sg" id="g${id}">${[1,3,5].map(v=>`<button data-v="${v}" onclick="send(${id},${v})">${v}</button>`).join('')}</div>`;
 return `<input type="number" id="i${id}" min="${p[2]}" max="${p[3]}" placeholder="${p[2]<0?'±'+p[3]:p[2]+'…'+p[3]}" onkeydown="if(event.key=='Enter')num(${id})"><button class="sb" onclick="num(${id})">ثبت</button>`;}
const row=(id,x)=>`<div class="rw"><div>${P[id][0]} <span class="lb">${P[id][1]}</span><span class="ap n" id="a${id}">—</span></div><div class="ct">${x||''}${ctl(id)}</div><div class="h" onclick="this.classList.toggle('o')">${P[id][5]}</div></div>`;

/* ---------- ساخت صفحه: ولتاژها + فیلتر (مشترک) ---------- */
$('vs').innerHTML=V.map((v,i)=>`<div class="vt"><small>${v[0]}</small><b class="n" id="v${i}">—</b><div class="fx" id="fv${i}"></div>${i<3?`
<div class="ct vc"><input type="number" step="any" id="vm${i}" placeholder="مولتی‌متر V" onkeydown="if(event.key=='Enter')vcal(${i})"><button class="sb sb2" onclick="vcal(${i})">اعمال</button></div>
<div class="lb">آفست <span class="ap n" id="a${v[2]}">—</span> mV</div><div class="ct vc en"><input type="number" id="i${v[2]}" min="-2000" max="2000" placeholder="±2000 mV" onkeydown="if(event.key=='Enter')num(${v[2]})"><button class="sb" onclick="num(${v[2]})">ثبت</button></div>`:''}</div>`).join('');
$('fg').innerHTML=row(7)+row(8);
/* ---------- دو ستون جدا: شارژر ۱ و شارژر ۲ ---------- */
$('ch').innerHTML=[1,2].map(n=>`<div class="cd"><div class="hd"><b>شارژر ${n} <span class="lb">· باتری ${n==1?'بالا':'پایین'}</span></b><span class="tg" id="st${n}">—</span></div>
<div class="big"><span class="lb">جریان تخمینی باتری (iest)</span><b class="n" id="ie${n}">—</b></div>
<div class="big"><span class="lb">duty <span id="dc${n}"></span></span><span class="n" id="du${n}">—</span></div><div class="bar"><i id="db${n}"></i><u id="cl${n}"></u></div>
<div class="sec">زنجیرهٔ اندازه‌گیری و محاسبه</div>
<table>${[['ADC خام','count',0],['ولتاژ شنت','µV',1],['جریان بدون فیلتر','mA',2],['جریان فیلترشده','mA',3],['تخمین باتری (iest)','mA',4]].map(r=>`<tr><td>${r[0]}<div class="fx" id="f${n}${r[2]}"></div></td><td class="n"><b id="c${n}${r[2]}">—</b> <span class="lb">${r[1]}</span></td></tr>`).join('')}</table>
<div class="lb kc">ثابت‌ها: ADC دوازده‌بیتی، ۳۳۰۰mV، R41/R42 = 1k/10k، LM358 × 101، شنت 10 mOhm</div>
<canvas id="cv${n}"></canvas><div class="lg"><span><i style="background:#5b6784"></i>بدون فیلتر · نوسان <b class="n" id="pu${n}">—</b> mA</span><span><i style="background:#4f8cff"></i>فیلترشده · نوسان <b class="n" id="pf${n}">—</b> mA</span><span>۳۶ ثانیهٔ اخیر</span></div>
<div class="sec">کالیبراسیون جریان</div>
${row(n-1)}<div class="as"><span class="lb">raw → فیلترشده <b class="n lv" id="lc${n}">—</b></span><button class="sb sb2" onclick="zero(${n})">صفر = raw فعلی</button></div>
${row(n+1)}${row(8+n)}
<div class="ca"><div class="ti">کالیبراسیون با مولتی‌متر (CAL)</div><div class="lb">جریان کانال بالای ۵۰ mA باشد (مثلاً در حالت مهندس: مود دستی با duty حدود ۱۵۰‰). ضریب تبدیل: مولتی‌متر سری با باتری همین کانال. گین (اختیاری و اول): بعدش ضریب تبدیل صفر می‌شود و باید دوباره زده شود. STM32 خودش ضریب را از دادهٔ زنده حساب می‌کند.</div>
<div class="ct cr"><input type="number" id="cr${n}" min="50" max="5000" placeholder="مولتی‌متر mA"><button class="cb" id="cb${n-1}" onclick="cal(${n-1})"><span>گین</span><b class="n" id="kv${n-1}">—</b></button><button class="cb" id="cb${n+1}" onclick="cal(${n+1})"><span>ضریب تبدیل</span><b class="n" id="kv${n+1}">—</b></button></div>
<div class="lb" id="cq${n}"></div><div class="cm lb" id="cm${n}"></div></div>
<div class="en"><div class="sec">محدودیت و duty فیکس</div>${row(12+n)}${row(13+2*n)}${row(14+2*n)}</div>
<div class="en mn" id="mn${n}"><div class="sec">تست دستی <span class="tg" id="ms${n}">—</span></div>
<div class="mx"><span class="lb">duty فرمان <span class="ap n" id="a${14+2*n}m">—</span></span><div class="ct"><input type="number" id="m${n}" min="0" max="500"><button class="sb" id="mk${n}">اعمال</button></div></div>
<input type="range" id="r${n}" min="0" max="500" step="1" value="0"><button class="bt rm" id="rm${n}">اعمال مجدد duty</button></div>
<button class="bt en" id="tg${n}">—</button></div>`).join('');
[1,2].forEach(n=>$('tg'+n).onclick=()=>{const c=D&&D.p[10+n];if(c!==0&&!confirm('PWM شارژر '+n+' فوراً قطع شود؟'))return;send(10+n,c===0?1:0);});
/* ---------- دستیار کالیبراسیون (روی فرمول‌های بخش 5.3) ---------- */
let LS=-1;const HN=120,H=[{u:[],f:[],r:[]},{u:[],f:[],r:[]}];
function zero(n){const r=H[n-1].r.slice(-10);if(r.length<3)return alert('دادهٔ کافی نیست؛ چند ثانیه صبر کنید.');
 const avg=Math.round(r.reduce((a,b)=>a+b,0)/r.length),du=D.t[(n-1)*7+5];
 if(!confirm((du>0?'هشدار: duty کانال '+n+' صفر نیست و جریان جاری است!\n':'')+'آفست صفر کانال '+n+': '+nz(D.p[n-1])+' ← '+avg+' (میانگین '+r.length+' نمونهٔ raw)؟'))return;send(n-1,Math.min(255,Math.max(0,avg)));}
function vcal(k){const R=V[k],m=Math.round(+$('vm'+k).value*1000),shown=D&&D.t[R[1]],off=D&&D.p[R[2]];if(!(m>0))return alert('عدد مولتی‌متر را به ولت وارد کنید (مثلاً 13.05).');if(off==null)return;
 const no=Math.min(2000,Math.max(-2000,off+m-shown));if(confirm(R[0]+': آفست '+off+' ← '+no+' mV\n(نمایش '+v2(shown)+' V، مولتی‌متر '+v2(m)+' V)')){send(R[2],no);$('vm'+k).value='';}}
/* CAL_REFERENCE (بخش 5.4): فقط عدد مولتی‌متر و هدف فرستاده می‌شود؛ محاسبه با STM32 است. رد شدن = هیچ پاسخی */
const CN=['گین کانال ۱','گین کانال ۲','ضریب تبدیل کانال ۱','ضریب تبدیل کانال ۲'];let CR=-1;
function cal(k){const ch=(k&1)+1,e=$('cr'+ch),r=Math.round(+e.value),n='۱۲'[k&1];if(e.value===''||!(r>=50&&r<=5000))return alert('عدد مولتی‌متر را بین 50 و 5000 mA وارد کنید.');
 if(k<2&&!confirm(CN[k]+' از '+r+' mA کالیبره شود؟\nضریب تبدیل کانال '+n+' صفر می‌شود و باید بعدش دوباره آن را کالیبره کنید.'))return;
 const m=$('cm'+ch);m.className='cm lb';m.textContent='در حال ارسال '+CN[k]+'…';
 fetch('/c?t='+k+'&r='+r,{method:'POST'}).then(x=>{if(x.status==409)throw 'کالیبراسیون قبلی هنوز منتظر پاسخ است.';if(x.status==503)throw 'لینک STM32 قطع است.';if(!x.ok)throw 'ورودی نامعتبر.';e.value='';}).catch(t=>{m.className='cm r';m.textContent=typeof t=='string'?t:'ESP در دسترس نیست.';});}
function calView(d){const c=d.c,t=d.t,p=d.p,on=d.on==1;if(!c)return;const w=c[1]==1;
 [0,1,2,3].forEach(k=>{const b=$('cb'+k),n=k&1,lo=t[n*7+3]<50;b.disabled=!on||w;b.classList.toggle('lo',lo);$('kv'+k).textContent=nz(p[k<2?2+n:9+n])+'‰';});
 [1,2].forEach(n=>{const i=t[(n-1)*7+3];$('cq'+n).innerHTML=`جریان فیلترشدهٔ فعلی <b class="n ${i<50?'wr':''}">${i} mA</b>`+(i<50?' — زیر ۵۰ mA رد می‌شود':'');});
 if(w)$('cm'+((c[2]&1)+1)).textContent='منتظر پاسخ STM32 ('+CN[c[2]]+')…';
 if(CR<0)CR=c[0];else if(c[0]!==CR&&c[1]>=2){const k=c[2],n='۱۲'[k&1],m=$('cm'+((k&1)+1));
  if(c[1]==2){m.className='cm g';m.textContent=CN[k]+' = '+c[3]+'‰ اعمال شد'+(k<2?' — ضریب تبدیل کانال '+n+' صفر شد؛ حالا ضریب تبدیل را کالیبره کنید.':'.');}
  else{m.className='cm r';m.textContent='رد شد: جریان/شرایط ناکافی ('+CN[k]+'). جریان کانال باید بالای ۵۰ mA باشد'+(k>=2?' و ورودی حداقل 10V و باتری حداقل 5V':'')+'.';}
  CR=c[0];}}
/* نمودار زندهٔ فیلتر هر کانال */
function chart(){[0,1].forEach(ci=>{const c=$('cv'+(ci+1)),w=c.clientWidth,h=c.clientHeight,dp=devicePixelRatio||1;if(!w)return;
 if(c.width!=Math.round(w*dp)){c.width=Math.round(w*dp);c.height=Math.round(h*dp);}
 const x=c.getContext('2d');x.setTransform(dp,0,0,dp,0,0);x.clearRect(0,0,w,h);const s=H[ci];if(s.u.length<2)return;
 let lo=Math.min(...s.u,...s.f),hi=Math.max(...s.u,...s.f);if(hi-lo<10){const m=(hi+lo)/2;lo=m-5;hi=m+5;}const pd=(hi-lo)*.12,a=lo-pd,z=hi+pd;
 const X=i=>w-8-(s.u.length-1-i)*(w-16)/(HN-1),Y=v=>h-8-(v-a)/(z-a)*(h-16);
 const ln=(A,col,lw)=>{x.beginPath();A.forEach((v,i)=>i?x.lineTo(X(i),Y(v)):x.moveTo(X(i),Y(v)));x.strokeStyle=col;x.lineWidth=lw;x.stroke();};
 x.fillStyle='#6f7a93';x.font='11px Vazirmatn,sans-serif';x.fillText(Math.round(hi)+' mA',8,16);x.fillText(Math.round(lo)+' mA',8,h-10);
 ln(s.u,'#5b6784',1);ln(s.f,'#4f8cff',2);const pp=A=>{const B=A.slice(-50);return Math.max(...B)-Math.min(...B);};$('pu'+(ci+1)).textContent=pp(s.u);$('pf'+(ci+1)).textContent=pp(s.f);});}
[1,2].forEach(n=>{const id=14+2*n,r=$('r'+n),m=$('m'+n);
 r.oninput=()=>m.value=r.value;r.onchange=()=>send(id,r.value);
 $('mk'+n).onclick=()=>{const v=Math.round(+m.value);if(m.value===''||isNaN(v))return;send(id,Math.max(0,Math.min(+r.max,v)));m.blur();};
 m.onkeydown=e=>{if(e.key=='Enter')$('mk'+n).click();};});
$('s19').onclick=()=>{if(!D)return;const on=(D.fl&32)||D.p[19]===1;
 if(!on&&!confirm('شارژر خودکار و همهٔ محافظت‌های باتری متوقف می‌شوند و duty را خودتان تعیین می‌کنید. ادامه؟'))return;send(19,on?0:1);};
$('ao').onclick=()=>{send(16,0);send(18,0);[1,2].forEach(n=>{$('r'+n).value=0;$('m'+n).value='';});};
/* اعمال مجدد: مقدار فعلی پارامتر ۱۶/۱۸ دوباره فرستاده می‌شود؛ بعد از تریپ JIT (وضعیت ۵) همین کار کانال را مسلح می‌کند */
[1,2].forEach(n=>$('rm'+n).onclick=()=>{const id=14+2*n,v=D&&D.p[id];if(v==null)return;
 if(D.t[(n-1)*7+6]===5&&!confirm('کانال '+n+' با duty '+v+'‰ دوباره مسلح شود؟\nسومین تریپ JIT = خطای نهایی (فقط با ری‌استارت برد پاک می‌شود).'))return;send(id,v);});
/* کلید حالت مهندس: روی همین دستگاه به یاد می‌ماند؛ حین تست بنچ خاموش نمی‌شود */
function eng(on){document.body.classList.toggle('eng',on);try{localStorage.setItem('eng',on?'1':'0');}catch(e){}chart();}
$('eg').onclick=()=>{const on=!document.body.classList.contains('eng');if(!on&&BT.run)return alert('تست بنچ در حال اجراست؛ اول آن را متوقف کنید.');eng(on);};
$('mx').onclick=()=>send(19,0);

/* ---------- به‌روزرسانی: فرمول‌های بخش 5.3 با مقادیر زنده ---------- */
const f1=x=>x.toFixed(1),V_=mv=>(mv/1000).toFixed(2)+'V',nz=v=>v==null?'?':v;
/* iest مثل STM32 (charger.c): زیر Vin 10V یا Vbat 5V برگشت به همانی */
const ie=(fl,vin,eta,vb)=>vin<10000||vb<5000?fl+' (همانی: ولتاژ زیر حد)':Math.floor(Math.floor(fl*eta/1000)*vin/vb);
function formulas(t,p){
 [1,2].forEach(n=>{const b=n==1?0:7,raw=t[b],off=p[n-1],g=p[n+1],eta=p[8+n],vb=n==1?t[18]:t[17],vin=t[14],fl=t[b+3];
  $('f'+n+'0').textContent='12-bit ADC · Vref 3300 mV';
  $('f'+n+'1').textContent=`${raw} × 3300/4095 × 11/10 × 1000/101 = ${raw} × 8.7756 ≈ ${Math.round(raw*K_UV)}`;
  $('f'+n+'2').textContent=off==null||g==null?'':`(${raw} − ${off}) × 0.8776 × ${g}/1000 ≈ ${f1(Math.max(raw-off,0)*K_MA*g/1000)}`;
  $('f'+n+'3').textContent=`average[W=${nz(p[8])}]( median[N=${nz(p[7])}]( ${t[b+2]} mA ) ) = ${fl}`;
  $('f'+n+'4').textContent=eta==null?'':eta==0?`eta = 0 → Iest = I = ${fl}`:`${fl} × ${V_(vin)} × ${eta}‰ / ${V_(vb)} ≈ ${ie(fl,vin,eta,vb)}`;});
 V.forEach((v,i)=>{const e=$('fv'+i);if(i<3){const o=p[v[2]]==null?0:p[v[2]],c=Math.round((t[v[1]]-o)/v[3]);e.textContent=`${c} × ${v[3].toFixed(3)} ${o<0?'−':'+'} ${Math.abs(o)}`;}
  else e.textContent=i==3?'V24 − V12':'= V12';});
 $('ff').textContent=`I_filtered = average[W=${nz(p[8])}]( median[N=${nz(p[7])}]( mA_unfiltered ) )`;}
function hist(d){const t=d.t;if(d.on==1&&d.seq!==LS){LS=d.seq;[0,1].forEach(c=>{const b=c*7,s=H[c];s.u.push(t[b+2]);s.f.push(t[b+3]);s.r.push(t[b]);if(s.u.length>HN){s.u.shift();s.f.shift();s.r.shift();}});}}
function draw(d){D=d;const t=d.t,p=d.p,on=d.on==1,man=(d.fl&32)!=0;
 document.body.classList.toggle('dn',!on);$('lk').classList.toggle('on',on);
 $('lt').innerHTML=on?`آنلاین · <span class="n">seq ${d.seq}</span>`:(d.n?'لینک قطع است':'در انتظار STM32…');
 hist(d);
 V.forEach((v,i)=>$('v'+i).textContent=v2(t[v[1]]));
 const F=[['snapshot',d.fl&1],['ورودی ۲۴V',d.fl&2],['اندازه‌گیری معتبر',d.fl&4]];
 $('fl').innerHTML=F.map(f=>`<span class="tg ${f[1]?'g':'r'}">${f[0]}</span>`).join('')+(t[19]&64?'<span class="tg r">خطا: باتری قطع</span>':'')+
  (t[19]&~64?`<span class="tg r n">fault 0x${t[19].toString(16)}</span>`:'')+(man?'<span class="tg y">مود دستی</span>':'');
 [1,2].forEach(n=>{const b=n==1?0:7,s=t[b+6],en=p[10+n],ce=p[12+n],fx=p[13+2*n],cv=t[n==1?18:17];
  const st=$('st'+n);st.textContent=(ST[s]||'#'+s)+(fx===1&&!man?' · فیکس':'');st.className='tg '+(SC[s]||'');
  $('ie'+n).innerHTML=t[b+4]+' <span class="lb">mA</span>';$('du'+n).textContent=pc(t[b+5]);$('dc'+n).textContent=ce==null?'':'· سقف '+pc(ce);
  $('db'+n).style.width=Math.min(100,t[b+5]/10)+'%';$('cl'+n).style.left=(100-Math.min(100,(ce==null?1000:ce)/10))+'%';
  [0,1,2,3,4].forEach(k=>$('c'+n+k).textContent=t[b+k]);$('lc'+n).textContent=t[b]+' → '+t[b+3]+' mA';
  const g=$('tg'+n);g.textContent=en===0?'وصل مجدد شارژر '+n:'قطع شارژر '+n;g.className='bt en '+(en===0?'run':'cut');
  /* بخش تست دستی همین ستون */
  const lim=Math.min(500,ce==null?500:ce),r=$('r'+n),m=$('m'+n),dv=p[14+2*n];r.max=lim;m.max=lim;$('mn'+n).classList.toggle('dis',!man);
  if(dv!=null&&document.activeElement!==r&&document.activeElement!==m&&!(d.q&(1<<(14+2*n)))){r.value=dv;}
  let tag=man?(ST[s]||'#'+s):'مود دستی خاموش',cl=man?(SC[s]||''):'';if(cv>=15000&&man){tag='قطع ۱۵V';cl='r';}const jit=man&&s===5&&en!==0;if(jit){tag='تریپ JIT — مسلح‌سازی لازم است';cl='r';}if(s===7){tag='خطای نهایی — فقط ری‌استارت برد';cl='r';}if(en===0){tag='کانال قطع';cl='r';}
  const rb=$('rm'+n);rb.classList.toggle('j',jit);rb.disabled=!man||dv==null;rb.innerHTML=(jit?'مسلح‌سازی · ':'')+'اعمال مجدد duty'+(dv==null?'':' <span class="n">'+dv+'‰</span>');
  $('ms'+n).textContent=tag;$('ms'+n).className='tg '+cl;
  const a=$('a'+(14+2*n)+'m');if(!(d.q&(1<<(14+2*n))))a.textContent=dv==null?'—':dv+'‰';});
 Object.keys(P).forEach(id=>{if(d.q&(1<<id))return;const v=p[id],a=$('a'+id);a.textContent=v==null?'—':v;
  const b=$('b'+id);if(b){b.textContent=v===1?'روشن':v===0?'خاموش':'—';b.classList.toggle('on',v===1);}
  const g=$('g'+id);if(g)g.querySelectorAll('button').forEach(x=>x.classList.toggle('on',+x.dataset.v===v));});
 /* شناسهٔ ۱۹ فقط اگر فرمور STM32 گزارشش کرده باشد */
 const sup=p[19]!=null,s19=$('s19');s19.disabled=!sup||!on;
 s19.textContent=!sup?'پشتیبانی نمی‌شود':man?'روشن':'خاموش';s19.classList.toggle('on',man);
 $('a19').textContent=d.q&(1<<19)?'…':'';
 $('mh').textContent=sup?'duty هر کانال در ستون همان کانال (بخش تست دستی) تعیین می‌شود.':'فرمور فعلی STM32 شناسهٔ ۱۹ را گزارش نکرده است (پروتکل v1.2 لازم است).';
 formulas(t,p);calView(d);chart();
 $('mb').classList.toggle('v',man);$('ka').innerHTML=man?(d.ka<1500?`پایش لینک فعال · <span class="n">keepalive ${d.ka} ms</span>`:'<b>keepalive متوقف است</b>'):'';}
async function poll(){const c=new AbortController(),k=setTimeout(()=>c.abort(),2000);try{const r=await fetch('/t',{cache:'no-store',signal:c.signal});const d=await r.json();clearTimeout(k);if(document.hidden){D=d;hist(d);}else draw(d);}catch(e){clearTimeout(k);document.body.classList.add('dn');$('lk').classList.remove('on');$('lt').textContent='ESP در دسترس نیست';}
 setTimeout(poll,300);}
/* ---------- تست بنچ (بخش 5.5): پنل خودش تست را اجرا می‌کند، از TLM آمار می‌گیرد و بلوک متن کپی‌شدنی می‌سازد؛ هیچ کالیبراسیونی خودکار اعمال نمی‌شود ---------- */
const FN=['r1','u1','f1','e1','d1','r2','u2','f2','e2','d2','vin','vlo','vhi'];
let BT={run:false};
const sl=ms=>new Promise(r=>setTimeout(r,ms)),fa=n=>'۱۲'[n-1];
const bst=m=>{$('bs').textContent=m;},f0=x=>Math.round(x),f1b=x=>x.toFixed(1),vv=mv=>(mv/1000).toFixed(2);
async function req(u,m){const r=await fetch(u,{method:m||'GET',cache:'no-store'});if(!r.ok)throw 'پاسخ ESP: '+r.status;return r.json();}
/* ایمنی حین تست: لینک، خروج ناخواستهٔ مود دستی، JIT، خطای نهایی، قطع ۱۵V */
function chk(){if(BT.abort)throw 'توقف توسط کاربر';const d=D;if(!d||d.on!=1)throw 'لینک STM32 قطع شد';
 BT.ch.forEach(n=>{const s=d.t[(n-1)*7+6];if(s==7)throw 'خطای نهایی کانال '+fa(n);
  if(BT.man){if(s==5)throw 'تریپ JIT کانال '+fa(n);if(d.t[n==1?18:17]>=15000)throw 'قطع ۱۵V کانال '+fa(n);}});
 if(BT.man&&!(d.fl&32))throw 'مود دستی قطع شد (ددمن یا محافظ پنل)';}
async function wt(ms){const e=Date.now()+ms;while(Date.now()<e){chk();await sl(Math.min(100,Math.max(1,e-Date.now())));}chk();}
async function until(f,ms,err){const e=Date.now()+ms;while(!f()){if(BT.abort)throw 'توقف توسط کاربر';if(Date.now()>e)throw err;await sl(100);}}
/* نوشتن پارامتر و صبر تا گزارش همان مقدار از STM32 */
async function setv(id,v){for(let k=0;k<3;k++){await req('/s?id='+id+'&v='+v,'POST');const e=Date.now()+2500;while(Date.now()<e){await sl(150);if(D&&D.p[id]===v)return;}}throw 'برد مقدار شناسهٔ '+id+' = '+v+' را گزارش نکرد';}
/* پنجرهٔ آمار ESP روی تک‌تک فریم‌های TLM */
async function stat(ms){await req('/m','POST');await wt(ms);const j=await req('/m');if(!j.n)throw 'در این بازه TLM نرسید';
 const o={n:j.n};FN.forEach((k,i)=>o[k]={a:j.s[i]/j.n,lo:j.lo[i],hi:j.hi[i]});return o;}
const vb=(m,n)=>m[n==1?'vhi':'vlo'].a,cf=(m,n,k)=>m[k+n];
/* حالت تک‌کانال: همسایه قطع (11/12=0)، هر دو duty صفر، بعد مود دستی */
async function solo(n,man){const nb=3-n;bst('آماده‌سازی: duty صفر، قطع کانال '+fa(nb));BT.man=false;BT.ch=[];
 await setv(16,0);await setv(18,0);await setv(10+nb,0);await setv(10+n,1);
 if(man){await setv(19,1);await until(()=>D.fl&32,3000,'مود دستی روشن نشد');BT.man=true;}}
async function restore(o){bst('بازگردانی تنظیمات قبل از تست…');const L=[[16,0],[18,0],[19,o[19]],[11,o[11]],[12,o[12]]];if(o[19]===0)L.push([16,o[16]],[18,o[18]]);
 for(const [i,v] of L){try{await setv(i,v);}catch(e){}}}
function dls(id){const a=$(id).value.split(/[,، ]+/).filter(x=>x!=='').map(Number);if(!a.length||a.some(x=>!(x>=0.5&&x<=50)))throw 'فهرست duty نامعتبر است (درصد، ۰٫۵ تا ۵۰، با کاما جدا)';return a.map(x=>Math.round(x*10));}
const chs=k=>{const v=+$('bc'+k).dataset.v;return v==3?[1,2]:[v];};
function ceil(n,pm){const c=D.p[12+n];if(c!=null&&pm>c)throw 'duty '+pm/10+'% بیشتر از سقف کانال '+fa(n)+' ('+c/10+'%) است؛ اول سقف را بالا ببرید';}
const ts=()=>{const d=new Date(),z=x=>String(x).padStart(2,'0');return d.getFullYear()+'-'+z(d.getMonth()+1)+'-'+z(d.getDate())+' '+z(d.getHours())+':'+z(d.getMinutes())+':'+z(d.getSeconds());};
const pline=()=>{const p=D.p;return `params: off=${p[0]}/${p[1]} gain=${p[2]}/${p[3]} median=${p[7]} avg=${p[8]} eta=${p[9]}/${p[10]} ceil=${p[13]}/${p[14]} Voff=${p[4]}/${p[5]}/${p[6]}`;};
/* خروجی: جدول روی صفحه + بلوک متن */
function out(k,O){const tb=O.T.map(t=>`<div class="lb">${t.cap}</div><div class="tw"><table class="bt2"><tr>${t.h.map(x=>`<th>${x}</th>`).join('')}</tr>${t.r.map(r=>`<tr>${r.map(x=>`<td>${x}</td>`).join('')}</tr>`).join('')}</table></div>`).join('');
 $('br'+k).innerHTML=tb+(O.sum.length?`<div class="bsum">${O.sum.map(x=>`<div>${x}</div>`).join('')}</div>`:'');
 const tx=[O.head,pline()];O.T.forEach(t=>{tx.push('# '+t.cap,t.h.join(' | '));t.r.forEach(r=>tx.push(r.join(' | ')));});O.sum.forEach(x=>tx.push(x.replace(/<[^>]+>/g,'')));if(O.err)tx.push('ABORTED: '+O.err);
 const a=$('bx'+k);a.value=tx.join('\n');a.parentNode.classList.add('v');}
function copy(k){const a=$('bx'+k);a.focus();a.select();let ok=false;try{ok=document.execCommand('copy');}catch(e){}
 if(!ok&&navigator.clipboard)navigator.clipboard.writeText(a.value).then(()=>{},()=>{});$('bk'+k).textContent=ok?'کپی شد':'انتخاب شد — کپی کنید';setTimeout(()=>$('bk'+k).textContent='کپی',2000);}
/* ورود عدد مولتی‌متر: نقطه نگه داشته می‌شود تا کاربر دکمهٔ ثبت و بعدی را بزند */
async function dmm(k,txt,req_){const q=$('bq'+k);q.innerHTML=`<div class="lb">${txt}</div><div class="bqr"><label>مولتی‌متر ورودی ۲۴V <span class="lb">mA${req_?' (اجباری)':''}</span><input type="number" id="dq1"></label><label>مولتی‌متر باتری <span class="lb">mA (اختیاری)</span><input type="number" id="dq2"></label><button class="sb" id="dqk">ثبت و بعدی</button></div>`;
 q.classList.add('v');BT.go=false;$('dqk').onclick=()=>{const a=$('dq1').value;if(req_&&!(+a>0))return alert('عدد مولتی‌متر ورودی ۲۴V اجباری است.');BT.go=true;};$('dq1').focus();
 try{while(!BT.go){chk();await sl(100);}return [$('dq1').value===''?null:+$('dq1').value,$('dq2').value===''?null:+$('dq2').value];}finally{q.classList.remove('v');q.innerHTML='';}}
async function run(k,name,fn){if(BT.run)return;if(!D||D.on!=1)return alert('لینک STM32 برقرار نیست.');$('br'+k).closest('.cd').classList.add('op');
 const o={};[11,12,16,18,19].forEach(i=>o[i]=D.p[i]);if(Object.values(o).some(v=>v==null))return alert('پارامترها هنوز از STM32 خوانده نشده‌اند.');
 if(D.p[15]===1||D.p[17]===1)return alert('مود duty فیکس (۱۵/۱۷) روشن است؛ اول خاموشش کنید.');
 const O={head:`[ChangeOver bench] ${name} | ${ts()}`,T:[],sum:[],err:null};
 BT={run:true,abort:false,man:false,ch:[],go:false};document.body.classList.add('br');$('bs').closest('.cd').classList.add('v');
 try{await fn(O);}catch(e){O.err=typeof e=='string'?e:'خطای داخلی: '+e;}
 BT.man=false;BT.ch=[];out(k,O);await restore(o);BT.run=false;document.body.classList.remove('br');bst(O.err?'متوقف شد: '+O.err:'تمام شد — بلوک متن پایین کارت آمادهٔ کپی است');}
/* تست A: خطی‌سازی جریان */
async function tA(O){const L=dls('bd1'),C=chs(1);C.forEach(n=>ceil(n,Math.max(...L)));
 for(const n of C){await solo(n,1);const T={cap:'Test A - linearity CH'+n,h:['duty%','raw avg (min..max)','panel mA','DMM_in','ratio panel/DMM_in','DMM_bat','Vin V','Vbat V'],r:[]};O.T.push(T);
  for(const pm of L){bst(`کانال ${fa(n)} · duty ${pm/10}% · صبر ۳ث`);await setv(14+2*n,pm);BT.ch=[n];await wt(3000);bst(`کانال ${fa(n)} · duty ${pm/10}% · نمونه‌برداری ۳ث`);const m=await stat(3000);
   const r=cf(m,n,'r'),ma=cf(m,n,'f').a;bst(`کانال ${fa(n)} · duty ${pm/10}% · منتظر عدد مولتی‌متر`);
   const [di,db]=await dmm(1,`کانال ${fa(n)} · duty <b class="n">${pm/10}%</b> · raw <b class="n">${f1b(r.a)}</b> · پنل <b class="n">${f0(ma)} mA</b> — عدد مولتی‌متر را وارد کنید (duty نگه داشته شده است)`,1);
   T.r.push([pm/10,`${f1b(r.a)} (${r.lo}..${r.hi})`,f0(ma),di,(ma/di).toFixed(3),db==null?'-':db,vv(m.vin.a),vv(vb(m,n))]);out(1,O);}
  await setv(14+2*n,0);BT.ch=[];}}
/* تست B: صفر و کراس‌تاک (هر دو کانال وصل، کانال پارک duty صفر) */
async function tB(O){const L=dls('bd2');[1,2].forEach(n=>ceil(n,Math.max(...L)));BT.man=false;BT.ch=[];
 bst('آماده‌سازی: هر دو duty صفر، هر دو کانال وصل');await setv(16,0);await setv(18,0);await setv(11,1);await setv(12,1);await setv(19,1);await until(()=>D.fl&32,3000,'مود دستی روشن نشد');BT.man=true;
 await wt(1000);bst('صفر واقعی: هر دو کانال خاموش، ۵ث');const z=await stat(5000);
 O.T.push({cap:'Test B - zero (both duties 0, 5 s)',h:['ch','raw avg (min..max)','offset param','mA filtered avg'],r:[1,2].map(n=>{const r=cf(z,n,'r');return ['CH'+n,`${f1b(r.a)} (${r.lo}..${r.hi})`,D.p[n-1],f0(cf(z,n,'f').a)];})});
 const T={cap:'Test B - cross-talk (neighbor running, parked duty 0)',h:['driver','duty%','driver mA','parked','parked raw avg (min..max)','delta raw vs zero','parked mA'],r:[]};O.T.push(T);out(2,O);
 for(const n of [1,2]){const pk=3-n;for(const pm of L){bst(`کانال ${fa(n)} روی ${pm/10}% · کانال ${fa(pk)} پارک · ۳ث صبر + ۳ث نمونه`);await setv(14+2*n,pm);BT.ch=[n];await wt(3000);const m=await stat(3000);
   const r=cf(m,pk,'r');T.r.push(['CH'+n,pm/10,f0(cf(m,n,'f').a),'CH'+pk,`${f1b(r.a)} (${r.lo}..${r.hi})`,f1b(r.a-cf(z,pk,'r').a),f0(cf(m,pk,'f').a)]);out(2,O);}
  await setv(14+2*n,0);BT.ch=[];await wt(1500);}}
/* رگرسیون خطی: شیب بر دقیقه و R2 */
function lr(y){const n=y.length,xs=y.map((_,i)=>i),mx=(n-1)/2,my=y.reduce((a,b)=>a+b,0)/n;let sxy=0,sxx=0,syy=0;xs.forEach((x,i)=>{sxy+=(x-mx)*(y[i]-my);sxx+=(x-mx)**2;syy+=(y[i]-my)**2;});const b=sxx?sxy/sxx:0;return {m:b*60,r2:syy?sxy*sxy/(sxx*syy):0};}
/* تست C: پایداری/دریفت */
async function tC(O){const pm=dls('bd3')[0],S=Math.max(10,Math.min(600,Math.round(+$('bt3').value)||60)),C=chs(3);C.forEach(n=>ceil(n,pm));
 for(const n of C){await solo(n,1);await setv(14+2*n,pm);BT.ch=[n];bst(`کانال ${fa(n)} · duty ${pm/10}% · صبر ۳ث`);await wt(3000);const T={cap:`Test C - stability CH${n} duty ${pm/10}% ${S} s (1 s averages, after 3 s settle)`,h:['t s','raw avg','mA filtered avg','mA min..max'],r:[]};O.T.push(T);const y=[],rw=[];
  for(let i=1;i<=S;i++){bst(`کانال ${fa(n)} · پایداری ${i}/${S} ث`);const m=await stat(1000),f=cf(m,n,'f');y.push(f.a);rw.push(cf(m,n,'r').a);T.r.push([i,f1b(rw[i-1]),f1b(f.a),f.lo+'..'+f.hi]);if(i%5==0)out(3,O);}
  const g=lr(y);O.sum.push(`CH${n}: mA first ${f1b(y[0])} · last ${f1b(y[S-1])} · min ${f1b(Math.min(...y))} · max ${f1b(Math.max(...y))} · raw first ${f1b(rw[0])} last ${f1b(rw[S-1])} · trend ${g.m>=0?'+':''}${g.m.toFixed(2)} mA/min (R2 ${g.r2.toFixed(2)})`);out(3,O);
  await setv(14+2*n,0);BT.ch=[];}}
/* تست D: عملکرد شارژر خودکار روی iest (بند 630..650) */
async function tD(O){const S=60,C=chs(4);
 for(const n of C){await solo(n,0);bst('خروج از مود دستی');await setv(19,0);BT.ch=[n];const T={cap:`Test D - regulation CH${n} (auto charger, neighbor cut, 1 s averages)`,h:['t s','iest avg (min..max)','duty%','Vbat V','Vin V','state'],r:[]};O.T.push(T);const R=[];
  for(let i=1;i<=S;i++){bst(`کانال ${fa(n)} · عملکرد ${i}/${S} ث`);const m=await stat(1000),e=cf(m,n,'e');R.push(e);T.r.push([i,`${f0(e.a)} (${e.lo}..${e.hi})`,(cf(m,n,'d').a/10).toFixed(1),vv(vb(m,n)),vv(m.vin.a),ST[D.t[(n-1)*7+6]]||D.t[(n-1)*7+6]]);if(i%5==0)out(4,O);}
  const st=R.slice(-10).reduce((a,b)=>a+b.a,0)/10;let tset=null;for(let i=0;i<S;i++){if(R.slice(i).every(e=>Math.abs(e.a-st)<=10)){tset=i+1;break;}}
  const w=R.slice(-20),pp=Math.max(...w.map(e=>e.hi))-Math.min(...w.map(e=>e.lo)),ok=st>=630&&st<=650;
  O.sum.push(`CH${n}: settled iest ${f0(st)} mA (last 10 s) · settling ${tset==null?'not settled':tset+' s'} (within +-10 mA) · p2p ${pp} mA (last 20 s, 10 Hz) · band 630-650: <b class="${ok?'okc':'erc'}">${ok?'PASS':'FAIL'}</b>`);out(4,O);BT.ch=[];}}
/* تست D اختیاری: پیمایش duty با ستون مولتی‌متر = منحنی عملکرد مبدل */
async function tS(O){const L=dls('bd4'),C=chs(4);C.forEach(n=>ceil(n,Math.max(...L)));
 for(const n of C){await solo(n,1);const T={cap:'Test D sweep - converter curve CH'+n+' (3 s settle + 10 s window per step)',h:['duty%','mA filtered avg (min..max)','iest avg','DMM_in','ratio mA/DMM_in','DMM_bat','Vin V','Vbat V'],r:[]};O.T.push(T);
  for(const pm of L){await setv(14+2*n,pm);BT.ch=[n];bst(`کانال ${fa(n)} · پیمایش duty ${pm/10}% · ۳ث صبر + ۱۰ث نمونه`);await wt(3000);const m=await stat(10000),f=cf(m,n,'f');
   const [di,db]=await dmm(5,`کانال ${fa(n)} · duty <b class="n">${pm/10}%</b> · پنل <b class="n">${f0(f.a)} mA</b> — عدد مولتی‌متر (اختیاری)`,0);
   T.r.push([pm/10,`${f0(f.a)} (${f.lo}..${f.hi})`,f0(cf(m,n,'e').a),di==null?'-':di,di?(f.a/di).toFixed(3):'-',db==null?'-':db,vv(m.vin.a),vv(vb(m,n))]);out(5,O);}
  await setv(14+2*n,0);BT.ch=[];}}
const BK=[null,
 ['A · خطی‌سازی جریان','کانال همسایه قطع می‌شود. مولتی‌متر را سری با ورودی ۲۴V کانال تست ببندید (مولتی‌متر دوم سری با باتری اختیاری است). هر نقطه: ۳ث صبر، ۳ث نمونه‌برداری، بعد duty نگه داشته می‌شود تا عدد را ثبت کنید.','5,10,15,20',1],
 ['B · صفر و کراس‌تاک','هر دو کانال وصل‌اند. اول هر دو duty صفر (۵ث، صفر واقعی)؛ بعد کانال ۱ روی هر duty و کانال ۲ پارک، و برعکس. بدون ورود عدد.','10,15,20',0],
 ['C · پایداری و دریفت','کانال همسایه قطع. یک duty ثابت، لاگ هر ۱ث؛ شیب خط صاف رو به بالا یعنی دریفت حرارتی، نه خمیدگی.','15',1],
 ['D · عملکرد شارژر خودکار','کانال همسایه قطع، خروج از مود دستی، ۶۰ث لاگ iest / duty / Vbat؛ عدد نشست، زمان نشست، نوسان و پاس/رد باند 630 تا 650 mA روی iest. پیمایش اختیاری: هر duty با ۳ث صبر و ۱۰ث نمونه، با ستون مولتی‌متر (منحنی عملکرد مبدل).','5,10,15,20',1]];
$('t3').innerHTML=`<div class="cd bsb gb"><div class="mx"><span id="bs">—</span><button class="sb stp" onclick="BT.abort=true">توقف تست</button></div></div>
<div class="wn">پنل هیچ ضریبی را از این تست‌ها خودکار اعمال نمی‌کند؛ بلوک متن هر تست را کپی کنید و بفرستید. keepalive مود دستی در تمام تست‌ها فعال است. در پایان یا توقف، duty صفر و پارامترهای ۱۱، ۱۲ و ۱۹ به مقدار قبل از تست برمی‌گردند.</div>`+
BK.map((b,k)=>!b?'':`<div class="cd" style="margin-top:12px"><div class="ti">${b[0]}</div><div class="lb">${b[1]}</div>
<div class="bctl">${b[3]?`<span class="sg" id="bc${k}" data-v="1">${[[1,'کانال ۱'],[2,'کانال ۲'],[3,'هر دو پشت‌سرهم']].map(c=>`<button data-v="${c[0]}" class="${c[0]==1?'on':''}">${c[1]}</button>`).join('')}</span>`:''}
<label class="lb">duty % <input type="text" id="bd${k}" value="${b[2]}" class="dl"></label>${k==3?'<label class="lb">مدت ث <input type="number" id="bt3" value="60" min="10" max="600"></label>':''}</div>
<div class="bqr2"><button class="sb brun" onclick="${['','bA()','bB()','bC()','bD()'][k]}">شروع تست</button>${k==4?'<button class="sb sb2 brun" onclick="bS()">پیمایش duty</button>':''}</div>
${(k==4?[4,5]:[k]).map(i=>`<div class="bq gb" id="bq${i}"></div><div id="br${i}"></div><div class="gb bxw"><textarea id="bx${i}" readonly></textarea><button class="sb sb2" id="bk${i}" onclick="copy(${i})">کپی</button></div>`).join('')}</div>`).join('');
document.querySelectorAll('#t3 .sg').forEach(g=>g.querySelectorAll('button').forEach(b=>b.onclick=()=>{g.dataset.v=b.dataset.v;g.querySelectorAll('button').forEach(x=>x.classList.toggle('on',x===b));}));
const cfm=m=>confirm(m+'\nدر پایان یا با دکمهٔ توقف تست تنظیمات قبلی برمی‌گردد. ادامه؟');
function bA(){if(cfm('تست خطی‌سازی: مود دستی روشن و کانال همسایه قطع می‌شود.'))run(1,'Test A - current linearity',tA);}
function bB(){if(cfm('تست صفر و کراس‌تاک: مود دستی روشن، هر کانال به نوبت روی duty فهرست.'))run(2,'Test B - zero + cross-talk',tB);}
function bC(){if(cfm('تست پایداری: مود دستی روشن و کانال همسایه قطع می‌شود.'))run(3,'Test C - stability / drift',tC);}
function bD(){if(cfm('تست عملکرد: کانال همسایه قطع و مود دستی خاموش می‌شود؛ شارژر خودکار کانال تست کار می‌کند.'))run(4,'Test D - regulation (band 630-650 on iest)',tD);}
function bS(){if(cfm('پیمایش duty: مود دستی روشن و کانال همسایه قطع می‌شود.'))run(5,'Test D sweep - converter curve',tS);}
/* کارت‌های تست بنچ جمع‌شده؛ با کلیک روی عنوان باز می‌شوند */
$('t3').insertAdjacentHTML('afterbegin','<h2 class="h2">تست‌های بنچ</h2>');
document.querySelectorAll('#t3>.cd:not(.bsb)').forEach(c=>{c.classList.add('bk');c.querySelector('.ti').onclick=()=>c.classList.toggle('op');});
try{eng(localStorage.getItem('eng')==='1');}catch(e){eng(false);}
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
"d09GMgABAAAAAIxQABIAAAABICgAAIvoACEAxQAAAAAAAAAAAAAAAAAAAAAAAAAAGoIuG7cYHJN6BmA/U1RBVFQnHgCDdi9EEQgKgokwgeEpMIHHFAE2AiQD"
"jRoLhlAABCAFhFIHIFuWB1GDN0/RgtoJqE+p2W1btlGIx8FCzOyEG0MPGwcAt/hFZ///OUmHDCV0S4BWqnN3N5oIEbIgMGuuOmhVquos415iK5nnpcB9sg9m"
"NF6bcdBITpDJ9BEEQR+4UzXxSNkMl0wSSDhb8F22iD2t74Z01DJi/IVLtq0Lvjnt2rOtauCBE3/aqywwIqwht0t6NLrk6FM3HqZhkfTI0Yei//tpDdV82xor"
"OhfdSUfIRoFe/e9OVl5OvMsqMHZ5jKh1fRXhX7B7+nOTzMxS7KiO1ZYAa9Kt8I03PD+33ns/Fsn4G6PGoEdt0MLAARORKIERpaSVaGM0YgEyRaw41NnFIXYj"
"FiKnyIEI4w/B3DowCqmWiBAYtSpYsBEbKxYs2cZGjZExSmIgIEgL4kCZDKO/td//97/1//3SDz/a59VZfUsWWJYsY4AHCJ1yd69trDiH8Pp90x0gdtROe0Qd"
"8bzPstcr6WnWyMamPtyE9MbTGuClEhBAWPCfhapW0DK7q2YPfvdLAuOw6O8RBslJJEIhHMYikeIs8ptBN/vPM5Fe61Qp0BQCDYUUQgg5KpzZE5Ft+dv4JxO/"
"/p3ars37LDs5RHxgSOqwkuo6Ts8By/sjT//i6d8BlYCOUBA44OHfTf8PUNZSm3dit7vm8vnex69q5alft8279raMUaCUghcSCCGEhEMIkRMhEIL+2Fz/n5MQ"
"QhTBgo7j9Duv1DKvtL7afMIvdbFuu/8Xq7brdW6pTvM6jiKCgCFk+J9T/rGKiCIiHtiCcEQ8IgKiSIwaY43NbDruGPtvvLFi+tb4e/vj6841OnbaneHBOmNf"
"yqtJRHpo0mGEjs0SUXkQBIWJKeTz8P/7fTujft8MZpaGRGaRKNn2/w8IlRDFmotTQbc5+wflUHIOWoTYkQb/FVnAFmFAEbXQh8+xZ+3dttuWviD8okEEMRhM"
"STQnLoDLNGszsTcVCvMS7yS3hNXCEz5ReY4HJGsvGgDMBCQHmIJVraKdav9/0/WVvqsnNSX4hvp8A92zu83sYQ+shTNBAqwxJppNd4Ok6j6Vnl49lVSSgKYo"
"tICm+YOge1vAd8CaKknQkqDN76Y9Y4zL1qjpNTTj6HF+jXGR8aFzkTeZcdmGfTaaINwk9D7PN9wktZmxabBXpZrfaQQGR9BxL69z78W99Cfp9HX5568/LyTd"
"LkBQWoKAbwkqLJfieQlRvgGDagElEKQCyUs5NMCTbwBKrqUuwZnn/HmGlF6+10t+/L+GZ/Ml85SrP1eYhCIluCJByj63E61aOZz9bZ5sDSGY4DNC6IQQeiNy"
"LQz51pahMxf0Y7OVTcDKafk51P7rPe3t1SFBkSVBxLr1wYRcJtnZAk6PWg7Q9469rl4iurtXmq807ZEWJARJRDIhSNjy/54QGCLz/SWEHMSWOAo3UQRvRSSs"
"GU3GinHUwxSdVlpjnT32OZpjjrUcd8Ip51xwI42aKje1zFaH8flgtPku/vOfHnqvAWi8eQMkmcrFSy/eqoUYarDzssC1qkq9TW6bIr/DXUBxAQVe46Uq0UVy"
"oB2wfSmVw/Uba6E0UFaxa45bZ0uA4wVRUlTdMKGFnGbL84OwG/XipeXRKsIOC3W9gPF4cv4WsAKQj0zLiB6fuyHw+2Ly1DRI+GrmjUsA/t2NuRUA/3nq1hUA"
"txsAucsH4OAQu3MpKEui6NLk6Jc3r4GQn4puXAFRrzTBTP2p3Augv6xcjmO1M6AQGEkrKgv2yyc4cLEw57bj27nvxLzT+S5T+PHurshjuy0aKILJnwTYEDiI"
"YoUTCA4JEzY8BJgwJWbGghUbjpwpuPGg5MWXv0BB1DQKMEipdFhjt3+ccK62gmCCYFsTkGS7AdRQz+Ck6+56XU76Gi1Nhlwr7bTGfZ70vHdttWOosRv38Z/8"
"qZzWWTwrp3bqZ9ecngNzGogDCIKEBRuePteSUxaugHtv1asmAYVAg1HbbLe7aYcjjjq+yKKLw/kVszX/Q0lpiwan1mESj4tIzrAxDjpnIwE1YPewqjDpvRdf"
"IAJ7DclL/GeUKKEVDU+U2pS0EBlT5C8og0rQQ339HXGuEPZm/N5hAoZhC4Vxe6tVRIK6o3EVx69XCudjSECVi/qEdnU9jo+ArkwZxjfIP0lQFJyqWuNiooA0"
"wVVgB5wU8o4mQQcNGoA229GCo+X2Vuvsl8WDS5RJltiIQrKqRK2ACDQIWkRIuE7BUidG08pGlyAeTBUmkIdzji9mFGQQRUWnxEm8JlsBaSTZtPzweGlABpQh"
"h6iP/DZC20UQW5BAAkkD6l88jC+KP5EgEnDHcWAKWcd5p6dvAjrSRSBlOVAlPp/avHwB/dcTVMhAh8wH0nsrbMnNb1XYB65QLgq1BtISc1LeSSyLNydUSI1B"
"QlgGapRo4hwrOWvgox4zCsDY3VEr3h/6cNrj163kc4G+yOp6FNfKaLxiE3CeObmQSRVTdkLxM3cevLp53UOG8ErRWiXMA6Cezs6t9/kJd650XPwy5avKuPB6"
"lrecSr9ggxZsp+K/YK0t5A858fzCUcpbcrIsNRetXCSiD3u/xZT9qhx9fhRM7dpjQkzaRGNFdGJ1z+GlHIdlw6koi8/XyzAtnMickO/nJz+ck1/+v4tz7u/F"
"c+zu/lkWvhPsmz7oFOLvyTnRvwWWLt77S2qX5SYkeLp7CxRzm8cY//V2Jh/LRn7Tv+Y/hPvkQpMNQjjS39KPjhI9evaWlTDCdFFNDnskOqZI4tKBenKBNCzG"
"yf5kvMpYjmXFuVUbcTkJrfYdR16tmMhBLqZqiPkn+ZPJoc6w11y7MHbPVI8ItpCy6TXOBu86R04n0pS1Af5BnBQ/vsMz8U9dGT8aWEu94K429N0ec4jfETnu"
"PVSDIl9WY4BurgFgFYo5r1ZwdoTntNApe4wFOwtzLMviOHGWalWi/w0VKw9dEa5azZaqVE1/nv2sTda5xXoQChB4bwHBPjhWSMeHCcLCgA1HDGHWzFmzIMAS"
"xRHECQNnMkpCKha8IHxZ8icWyEYQHmoSoQhiuRnOQSpHGfaTz9Fse1mIZCmSSiyqmdEvkb1w2UE3rU0FR3WFvLYUHNUPSCFUIAKEBQnBgCGAEMIQHyDTwlYT"
"5GAoUBGuKoaHKuRpiow2MaWWDok3JsduE3OHEoBqJVC1NkiVCVL5BJtSB5qIJcM6tdIQaSqtgFo5DkaguipTrZhjurrRhE3ZlEnZjol6JPTE4bETVcAxVcAV"
"iftDudSGcj8/m4EAHC4O3EGY4KwAIbnpbS20yhprgeEEzgUIERxCCbxhsMGGsYKg0b8VI9k6rAAEfMLp7DCFc6O3p9z3wFPPPPfCOx/u4onP3E1PS5VZpto2"
"l7oaDwZTwvlKu6nZLbc99sRrPSZKsUShWqvUWadORdi5vFzLIw10sDd33355A0B4NHnokTfeeq9Fr86mWijTbknzrpVZ0ooSbUkidKRpdB7yLZkiNISqfKbG"
"eoScqNKwhmr99mBqSGC+FkJbhp3kygJy4ArGOeSQXeOgf/vnxn20a9+wTYZwyRJbiMwKHIxtXnm8Ad4ziELkTzvbMpGI9knNcNkmWwbdE+otEBFAgFezfDtC"
"zDh6PgjEhxGFLMDziF4VIYNposm4f6je76F8bUW8M/eDlmsfHpWIMe5VaY8NB9Hp2EorpickgzBx0ZHKc5zHSq3p2DuXkEvkk3G+0c0K34pqi220sPpDiJde"
"YXvXmoTWO5W3ObJ7De5IuiXZWEgKiQtp/LJwKMmoI7eQHaQm6QEOznBJIhzqwy8klizK8C2BR4FdPWBshmdcP3LGbTbqNsQIM50n8AounEdjoUs6FBJ8BxkE"
"ClR/3JBgMjkXWvvsA0EihPn2QxDLHykYxy/T+0nqM0he8E0HAXZ2HUCpXzHg3N0zM7mXwQUbs6ttWTNBexk0amEjlNbXijBqR8CuLMHBnUfaJeKnzTIbS7UX"
"DVJrrmzFltvsenfY4Mna1d3L3velrXb4ZxASjdWZelH1qtQMfmgjmC+mc7lo/LRO5QZt9NKWveItn+nZN0d305yf244Ie/ppWy4mOUe+rrhOq9/2XfspsLv2"
"0B7fi9u0z7dl27c7AM+HU5HFJYEJDzXFkacyjVmZqtTnEOr1PFZb0zU9UCYhVZyqP6qmeJVWtipudc/O1nZHG3r+d57GPu3ndh8QI0KFiDF4aAjQ0srfeJ3P"
"OY2bKAV0ha5qaaSzgjdsdvHrXGlnO8F0rkv3eevA+Lw0Xl42u3z2/g07ZQNAxV8x4L42m6tNEJgvbXx30g1mqlRLAjaAOr9iwM0367OvpvVfO6DX4hBotkDz"
"c3Z/1uHuOumk5WGLO1duWQCxdUG6pY2wk+RmIO+C7Efq9mqX0/l7a7a2gT3UcTDMMxc7dx2SmLWtPWxbvuzXamZ2H9XelQq07RbGs3ABXmmBVzWOgxFeBoos"
"sL+BeHKjZM6u6PZVHJjel4p5iFygC3GbpvJpLl86nOBQFs1Ob4VqrQr7ySvDCKGEJjSbzcby9q+pfNW8JIpwNAeq9FDz1gPirOxcyeEm84SdM2OzwnLVgcqT"
"Ni4nmSmBguNa1Jwi7IGQf6HHk6aM3xpw0ksrYQ9Nul4+GCIRefmIY4VVzBx22eHHfwwrZCZo6ve5UKBmqybt87BFSdWz9fsNEIVUXlkhAkbF/26yMeTHJard"
"+pk39sbIqyabQLY2+lMGcJvWwcWdnc69cd8qoKTdDUb/KZsM1+nccckex2mdp0tdsTJlgap3of0g4dtppptP3C/o3+k9FLnPAT5ejldCRmUMBzKsTWN3GvrI"
"KoAe1NVsFwOMZ8qYyJJGu0ob7TaUiL7luqhs2exqkl7r53MsQvQ+vA6kPXxZY8esZA5BSocJR9ol4EgdsCt037TLlLxkpM06v/dk6+CBGpPmleLh0rDqHZ6W"
"utll1UK4BXqDVfcwtdzhYaDhIovqgisdbbwLvpOqaprdcxxhK20PRXFAw2UDdVw7Zb/l/OmWnts7FNdEN1+57wBWrVta20ZH+kDxyTbgb9jx27ue0HqcirXi"
"At09iEv1sHUAQL6YjVT1n40gG0k5n2vcwftAsVZ5H0Ck3aqyA3BzuO2Q+zDdtHTa/7xb+hFmeenEgilQYdLCcoI5UL7NRgWTWn5/WD27uG/rbvg9Y9/jzEPR"
"1tmRVZECbSaTJT3oNi0um4iqh08Z1JnLIKoBVPo016nRTOqY1JEzni5b8d3EIXYGWrTZkHhDK+Q54NKdvZBR2ipK7NWfgXBZFkijhTkMbVYRKdSQBXvkpA3k"
"qibWtHZ2R5BuWphWywKiRYMabZz1zUcabXrTnvxPy8jqkJ2f7xC0edYxu2rqN00LJ1BpUtfiyUALQblE+gW7Ee4EFFe5PaLVZ4i4fRFQHnEBCsdugyBRbeIT"
"ndNtexeYH1S+E/pLcjSXttP2hvlE+qseCBbxy16Ca4JvQmhCZIJ6zoeDv0InQY5Pyl8iE9wDRNhoCBKcwgVAYXYJ2LyYL0PxxOj6k3kSRj8Iwqv5cdBui3/l"
"uerrdqS+90jzx8CRo6ek4bBYhetjvTc2LN7uy3juxdXyEzR8UF7Ak2vB0kK5pBRUFJYpFWD9X4p3PKj9H/WlQf3Pu6fAvo/LA4de6fFLnx9bVqEA1+VKvhw0"
"/ylyKXD/V64cPFW20q/LmdItGtZtvmoUfBXo0Gji4kGXtvPEByrKJHzIs2r3L2NSShKvlAcC6Tb2cgGIAeAHDSlzMxE6ZVILKqR8yUhUREkVQlqWHzQrrMZ/"
"aWtqn6vQn0cympPb0sZlYlabuOXPr8xaZMnN1Xyu/FDH98Sf6mN+LJ7AZ/ovpeKp2w2BHG5hwQHRP5ySMGfJmpwTF67ceVLx5ifAIMFChCnEoqDRbZ19Djnl"
"Qu34JVmvSlXN8miHFXTODaW5ioJ8R6DK7lynXoPGS6mF9XbY6bgrZm0uHmFXy/cS4vYytnxg5vrhYscPvX3mQ2xA2A7cv72EKgnhWYhGkSBFFItbqO43OVTO"
"7fe2oiVqo1JZHqfKQpSU3ScBNYVTxBq8Gb8VQsI6OecCCyJr3GKm7LsnQoZLHgalCr02OMDgH2dcql1AuGQqHmzVwcLJ1dnjTxwsRqrgMOskDcBVZXI3yugR"
"5A/8bjE0EfJ/Q3PIVUarUo/19jvstIu1M2QWu19WBosrySKj1qnKHg3OulzbxlrYAQ6xntN4QTVYjTDhIsRLkmaBjPtivHv9Ppc8BZYqVaFKjZXWqLfJxa6w"
"wrVa6qRCVz300hsEfofoFbvyGTxqqDlSzbfIYvmKlChXeX79V1htrfW2uMxVVmqri+4qDaRCoNRen5R/YcCyZXXRkPcBQejTuwfw3AckPfyBUqc2Ct0khZ8F"
"9Tl6imozCl9D+gqW38+R5DoaoNpIcRLNlWLewxXKSlt9DFQrRoI5kqWydzxzEhFPyaad9BoGiELkUIuSentMS1xmZRu6GkTzGbJVYeBIzm5yBZXKoHgSl3Kt"
"EI9FuJtQGg5Ywj1/frjqrbfJRa6ywmo3queBAw2iLJR3q2EslMoy5RQQDMP5+1yLgNIu1BynfAc0yXCFGfT3AIVw36i3EyFlo4P+BjtELM7nrWdkTXjGrU41"
"GVX91iZRc1maHa2mG4UVzkIXvQzBS8fbg5AFlSBaHTL07MQuUXIVAo5dYgX/QnzuJeC6j6Z9yGua4Kibl17HV8URXcoFExe0FTXkR+ZAE007s61wZKagyptL"
"QFMcFmRcEvsEUfwIZ7SVEPjYl+PxvXBIQiZkQz7kQaEiJYqVagGER1thkIAMyIIcyIUC4edZSjPNwWOcAxCApHn1AFVanri/orgdme99DvkAqLI1uEx+DzOT"
"/jkOyLO6FCPcRS7aneL3e/11ACWSCcZzYPLPi7dWXbYJag/utweYCLj2h0MohV3bLRlKvzkPOo8u25dxAB+w5Ge7XY2GIFPzoQ8YAD/zR0Wn1QLgfWTPpzmz"
"bDjhlDgz45zomNTpMwELARoy6mnndb7gBFGsm105KrWKb/yH/Et3mOv/b6oo+X1n02CVlaI3Vv2JJ1KJyh3/zq8szp+cPT4xPtRac2OK+1Xg7UdOqf+QlfiH"
"001NPQVpzKvd6Sf+jwK3rXZVOkYas2psFG1SKr1xR2ora5xzwkRSq3bL1CHFvAVG0jkh5lT1NX2SHFRY1808cTzxBLnIcDFmmWUJ73BcBIyYZMaLDOAzi8sB"
"JEjUKaUCv5IMYzWoPP2eAuZoFkB2TYD3Och0peZ/Sof7t02KAfw1s/APRQABAS5hqxIubOPIJDYRxElGn2984ZxFOJKg42diNadGW1wuhed+BvmLwayfowyi"
"yxrMAbzxHpLjOJYLMhld2OonJX1ZC94AqgSwkmGRU7zJ9FTxH8YAgQJUhBMQ1x+lRVf/M0u8y0DHL8+zdyicmxmKeuFRzBj98xCvxbv9eauwR+QtiuM64/7k"
"We8Ow2IFTV4kl4iicCrqDN8bpkpvAIvPxbwxsecomu9MqMKfWhB4LSxYE/AGLsFNLnUK5SROomYjg0ShqQvZe0F0MAxgYKOjYxJhvTgmx0mIhwjQ8bNk1Wiu"
"ZJTSasHQmsQUOUQGIh5mtr0hYbxQuJ4oids6RCW9rlIQgnScZ1ktZvfznCWw6Ahq06KYwGUI1rXRB5HbtFuNugVkI+RcEEwxNBGPpoUbjBHXjWIKwRSvFtMR"
"YjOmudIfdhRxcyDAZ9zoHf3bcIWN0EYyNwvmkyGKSAsQlMvVvJihFl+1xUKBQvjsV7gCTHN5LZRK9wVpKT/XB6DgtjelOJYyWr1NYUd3veIUdCV6JAddlN6s"
"IjwZpHlfAYSvOvpvGIrnyNgY+3BKH9r1/AE3jIohivm9THIPLJnXMeDHRFl5g/w06OHUntbS/+58VqqlpqqhUhPpfUsPaoMW5zKWAiTUFOBM9GLoYX3B60vM"
"tMZTv4lDgmFUw1eBME2dgbhBLNymthvMgw7cCpUh7y3dIpoCH6jFkWhjAAkPb4X8J1RVOVIY1YFoc3S1moTwLpNCFO4fMZnTViWBc07OJbZmDPA6ftl+2ZUz"
"J7q2G+vJACsedO8VpSiLq+j5KLAQb8+iknOszaOKwo5F2xCthlArSiJZEKpppAOd0Go/Z2Dk8Gezi8XI2FfsyHQzQ1nWC3GnXcjj5Bof9JpYGEWdRhdMvbqU"
"VoG0R3MWdmKr0eG31Y8Z5zd4E+Mf6cTmVhvHG6hYz578m+M55Nj63lWvxNoIQewoVMXZhp2Eqtk5ANzGl8msyHhCMsKT/GDqaZHaRcpLnD0m0BXHnTDu1j0a"
"NtpCYXp9nrHd+WZcE4YaJjommuOiQc3VTFjSPkruhLtjUxmKs76zzWQsRWktndRvHEiZzG6b3Tl8GXeznPHNKcom0/zHB4BlhbYg5aEQ+LGfMF1zUCIG+jA0"
"9zq5NdhxbO3oNFFBXC+81iwaTJljaAneSXCfM8Urv4O9VeDVlPXuZA4vcIX2TwoMyzqJzYqIozP8OPOgHHKUPvS9ss9RzEwWFZmRL/hV1AfznfMLRqBRHFop"
"RQUyqKXhUIchKvRaIGzaVgFF1TLnMNpTgh2hbEBdZOAsK9Ei6fC4HNpfhKIkQZcnPytcqiqfVtYcy3aVxetvtGQVMXENZJdtrC0J22oeccTwCM0fJmc+D9of"
"4aEBvJrVSFCyafc0sUekifE8ziEyhmrSW5Gtmslg2RBjWil+qYnL5oc1q5pULk2MpWHCs17p8YPscITi81b5XugcXg3TvVPPZ5n+QwKUN8dr/Iz3PtCzLS1H"
"ZACUhqv6VDcX4YtfkUaeVscFPvVe+prkFBSv8v1SmT6DiqU+wPSuPMNmHUe5ixzRPU+LETYTmdHfdy0u25RWP4ndslrpM71lUexXPSVZLvKD0S0wVdHl44mv"
"eCoy0elvnXlMS/+ogYHX1UKpVqLz+OLnTGOXDCCuRiRugoL7qtxohkMsXEke+05pjLL+FWCZ7oKmCHI2qy8SqGNYYV7mujL0N5UI8SLF5gBbDcDUYVy8GKW+"
"ifDOYcy5FgbFucIlsLhm9pIRyovNqnmMaaxMfMztBw+k3ibe5ICanOC1QFjCWEjW4MqLPMco7toabB8tHxt8R7d/flct6JmA64kKGMlwkizm4iEsAtzJj6Ra"
"V+axZYNRQ2q6sLCP1xyqK0B2dDO1Z2yC0Y5KMyHz2RVsUZY1d61cNIQ8RAV53A+5r6nkQcKEXDabrjQ03YKMCy3zYHHJixxdLUC+eOjGM1EDWqwN+7+PrbmT"
"4joTaa6vsX4U0ncfAtdEj4quWJCT67yVDSvSEw75rigOzy//R7hD72hgMkPZOuS1I2TRTlGaG7rO8xSjNVlMj5pzGXafqDKcD8CDBHq9W1KrJ9fbWjxizKjd"
"LoSdjqItJ1VOc4mG+F10oVE3xrK5hM5Cbk5QMseYh/h213JrL9pb8DYid/3A4kM1C/3jsUlmQqyBYsoc2RqvLoTmY3pUInOxYxUb8MHJarJRnW4lbYzGBsui"
"x890QrjkDCh70AzZemBISix1tNlPyzXkiTtwtX9GGeOq+xT10nkw3qJW0PiBldkYRcbMV9iAH85/tZqhZ8ITZtK4nz4Upy5+NMaWs1g0Htr5LDy7a9l28NGx"
"S9kerHGG0jdvpJ9TlJRokHWL8RQSihU4rfOc0R9bTE+Dd5wJlAlPs45X2FLVNWdji8ZDLQM8Um7V2t16BJwjvgUpXW2xGi51+KUfWW/tts2G5rqFuTjFcmDF"
"REGUr6co7bqnsctGuM1Y5dQpygePcS9MiQy/fJA3foSir8yDIsXRegImFXTlXL9cOxCSkS80tD6dU15vGSEK86BB3IHPvfKtw+TkUgKe3mw8qr2E9CAeWrGA"
"Pz76h9oABu/gCj3HiwAZnXOIwO7jFzyWLAbjiqmxeIfEM3WERUEN8mhESkZufL6a5zrN2DMKU3msiEzL+koCO6/iIfMM1/A5G5CL5gMIjLXQsfPYCQNYOrpo"
"LcbiE4mIX5aENNkvmKDLoedfZY3rN1Mehw509pTsUloOVARtP/Zhg68YqDXIl+jEBqVk4sG0yFAllE5iUiTsz+xsX1x/OYgMTJ/wFydFKwjjrQMgmswaEAYf"
"NWHAz5deO/PAqTfU28fWOpI/V+XiBW+/rV85PFLT/FXZJtfM8BZZ9l+gYk/Nb8V/xCEXPOtt9AIdi4pY/2lMw5lJxxAY0Ves++7OYWsbwZbcReuACAnyY2z+"
"6DJapi5JL4kAkrh0VDQO3vGvx5z3nNmA48+xPV0hSo+R1ulLriFuW7KIAt8K6NwoQUFGg1mUpsx6ifcOBj64cadHqiI3+570tdCHY/+UT6b1Xj5AmcYZ0+gc"
"e/EltGrgHV/GoBLea/6A/eEn7dfZWYX99ndsDrH6RniAWwaKksz7u55X0Xuu7wAvpW7dDQvb7qypJRwce9Dc4WOaNOURKa63iX7sQRvk9o9VOb28+LYe+Zbe"
"oFb3kYWLbinvkQBYLikP3Z10FyFij0P6qy7H+Tl5GZu5MllMuuFv6fO3uG3MxxPeoztPn0BxjX/4bvIBRHjC1tncFGoIvF9BzCXlfTJMwIlpNGSCRG2Xebji"
"5Jg0rwbpXr8E5cUpiHr1cYfJhuKz23bKQ2F8ylDu5dpE4/4luBcliThLrOAXK1tr9orRXRacJa/oQokdVE9eeJVWL/7HjbrK8w5d6ZiXXDk83n7LgwikkGvd"
"wjvdXLFU/Vq+56Mju4tsfK9+rT5lIKyk91cO/05Qg1tlqnbXzmnGsvTq76PFy+zACwel1h7lCYjBl4ORenCWz+87JmE4eHR1PNFUjcy5xRwo8+q01uRNjV5J"
"Yreka0FlDQPW2T6oa66Go/sZUzd+1y7LEVZZGMi8TlWrknvEVanHTGeyp9IdFz6oLHGD19gyb+klaNd+TAPkSjozdQqvWppRqKKDawotMN6WbNYBu3VBTEDh"
"LNTkw5eDS7072OmT4/9r+eaYO5y047enqXNk9Jc18sB7PjurPnkrMbDN+WbtEQ5PtFMDgd22UbPSCvBgTbWjslT6sBdvCqtoBdfELXPlTlqTfaCymniliqNg"
"8V2N3kK11QbvscEv+Hls3ZULqOP1bT5NAAZd42G70gfNBt6Ju+y9tMGgF/oO24ENWzhoZ8dysNmW32RcjmXyKqwVcSrJC4fY4e+366DkxXpE1716y7QVOiY5"
"fizWWhlVwoYnOWuevLRV2m8HsYYAJDPybvV+2BUrHUlSYjcTqPxye8ZArCVNqT0I83xJFVQHO8SRl9snqPJ3Ijr6zRoG8fIPWlyzNRviV+bAmkpo5U5Yu+Kz"
"/IMrin7Nldp+XN/n9OFV7j+MCz2sj3gvViT2vvQK9kd8f90ureeGB0NPnziCNry/tAQ77ZkHCKaq6no2oPtgwwfj9+hZdEVZIcSRFgIO/q9xAYiP/GwOxc+h"
"ooTkPGAZNN9RHP78ulOLJQcv5QQrlFgZw+wBWXvLQKhI6bUP8ytOp8iZFWZA9JBuICgUnrLlDqZWlCKZVNRx+W0AoN0PFL91dLX+Cre815miDxWDgXpw+3PQ"
"XzxosOe9HEHRwVwxMRWGcOjB3SbFRXERq7IftKAPV/4x5lF7YSQ4Sx65CZccQFZop3082XR4u0eSogFmwtTFmuRQE7uigROTYHPqoY2723YtV3WHoR5u8ic5"
"Tg/WS/I/GSd+0b7TvHzD9BjzKSt8ztE4+95o2n1rA/KTr81K0xhMjGCCYYYTp9t1cs+oV+5vTcLdueaTDD0mF3KLfT+fvGqgzbYGgHSkXbAUU+lYSLKkM6Qz"
"QnztzhjsVXWh+DiFupmLBOanfOeq8bqYLBkTQTkOYy+JhVNWBC8wEZeZnQ28tWUVV2uPxXOwB2u7eDDFr1skT/Yn/K3+hH6DCqjGUiVcLmI543F77jw90pdL"
"H7ibbD64J/kx8A7TiyCanCnGW4tQrMTxNcvsELkIQ1EbECOW9StHb0CrGP470WoBBwcwz3kYsqHQn2uiULE+wbG3ArZMDR4uJp/kWOvGhoB3TZ94TidMA8en"
"37o//PbSkVeGGudhg/vcC5tp3ZFhw+N14zkjCCNf9Du2bjJJTkAKa1gXm5pv/Va2NvVS1vHB/LcpjvpI+9c/tVXYWKwmbTY+SS6t4hOR1V1IYeMtXIUsLqdo"
"glZq4FzPrOl9JKncqZ0auB7K/Awc3qe2miDVjdaWVXfL+r0B4XgNEKaPP/tr+20Gc+AkU26T8+ycfBLTNdZbTUmrqM0ucbKaqO4Q7x8UEI+z39n7HkhyzLbq"
"OSLT06mMOmrVx68shtFbz8fnMQStASvYiw6vZo95aGp49EKZrPolWBWm5Vg3kmH1uMVnSyCs6e5V266GD76pP7P636Ih9p3Bdwyxe5fOLTytrX9i23/VZWzM"
"tNK03lj49fFj/aRvF3uaagdz8IMgQF2zc0g1ceMd4djoJ+01vy6KlnS/ftF+fOh1gersIe1ZO83cpn5JHwtr0pdWwDa7+LKRhjrgE2c1GTN5t1n8/YmiJcm3"
"b+gGsNPYK36LswkPCx7OxocueiSbAGG6BggT+LIfjDGaTnHyNmvhDTTTPFPnOe65+GdH72/L0KXe3/9rd9ltMFgMQMudmto1ZV1hetZ4Ynn5VHr+oZ7+cxoE"
"/Wh0rB/+OvSF5p2vxmBjg1+ND42DNpd33M72Z9xh3TuR7mxaA5cBGm8OftSZNKUNqc99bbW8t2unn/XzGGuU9erq4Ej762U/yGWSwdIdKJAsVG6bhIT+6d7p"
"F33TL6cBhazG7yFXQa2vsRnoF42DuAfvuixVlNzBzm04zh1PfqL+oCNpz/zFpc/K6990aLxmkxvbZMhVdaw15v0wr1kufbHToC9tCMDEBaMpkyCihLzTL845"
"l9I78UNL+x8m2pmqP562zAzfZMsXbKWn1rJ52tTisn4O6mx9fhNta4It0auTKl54DIDIXVlTzlNH1F8Fx+DqQqC27hmfHQ+yuos7OAbM2uo4I3bW4A9R1bWV"
"fVf3DVTcuiNqb7/LrLjQf617uKGSWA1FNYL1yZAdiFIwvU5TVe7gZeOVO+JaZilDKauLFzaueDGP63v7//7V1/pLk+etcmPYUw4wM+XzBQ9t8c8/csLq73Lr"
"d00BS7LliESKzuquWkf5ugReePxdz4T1j2e1E6lr8TsDwq5OTb0/ZS1s0tuBQJPRqDVjfcAXGi4ceTR35ObcxeD5ukNhW6YFq7PVqVfJqDe84M8pqYB707Du"
"rul35/ev+zobOgKDDFOefw1tI18Zpg2WITDsaSWHCMZTd4a6bhI1rz/73Obxtfcr4f0NsGJ6P0JSmT6eDFIJ3Z7knb6uy0Tlzaf9XB3x/P2GVLnzIHFEwenr"
"0Y/4/I1neC1mt6gYDBk2GCzBN+Ryyx+fUIB6yiQQrLSpN8cH3yFX998VKj+z3Lz6mp4wNogSs6ax2jrYSU8bejtkDX8K+XDnzSseZg9JKc8AAdz3oMq0Pb93"
"Y6FwOJTg4OkQS4RVBGRFI+zeiSjkMByPlk5mFFTQo8fT2J5WHrFUGsp+BOVQdEoIqKBPVz5ZAYlze4tfDkD2LG+s7V8egTwpfjIM2be8trFveQByOZl9mvXi"
"TG1d1UYt98Vp9hney83aqvr1WuZLMGSzFG6q++dpx8ToVx1N/5jgpsZ/v+wYnXjWUf+v+Sdka87WILuUP8gmbbUiW8nnh9j84yuPwBVI21SbugcDbaASoC09"
"ZHWhS/vh7bHwVF3xRlNvxa2nIpCzRRwndunKGXRdc5fnK4vhYf9rbNG8TCWaeS0fDIIuqYOaqmR+z58UP+mH7FnelPrazb1q8QuTBtZMHrq55OXGZl7QEcSP"
"egXk5Ra08/0+gy93nW8SDORe23g0sn7J2SOmL+a/4Fde7xOQ50fLxV60W1aT+eIuLKqWUD9qXPLvy59g8njH3gDFnTZg/7zh1KPn51fzka2NjEI3abfVWARd"
"mZTKSGPXajp+fLXzbuTe60D570GQ7qBCpnFGcrpC+8MfqjUzarVu85PSkfln+qo3yq9SUfUGrKK0LRMqQWaiWSOEKr/5mIc1ggV1Zd2Zp5wJ4fWUaSV7XnGV"
"kCYVoBr90MKeFGwp8I6PiI/GatPkHo2+l5ilHTy2rP8Gpbn3Na5khrdT1FDeVlfDxUSnIRtTpZ5NvptcVjeXV2a4RWwrPBXVzaB0Fu9AoimUBIlbRp46LA0H"
"3D0WLr+oQ9PCaeD0zGDYNmDGH42D0AXT1Q/mBz/4UW4YfiHvftf54MoH5XnHRjECoWnxjw7lwM/ZsGPTqt+8V1TfcqeoarPV5rxNvrasNolaN1zdBLSNP9y4"
"CgiT2cGOkGrdNXH3b81Y30Oh5nPLjWu39LnGYZRYfa20eh3M+8jz0cP7/lumEZdfTZ7DD5M+GvFfMIG4AOdd0kbvYUOfo7PB6amEHNjnPWwAzucdnf7oJXc7"
"aYN7bOgGW34r8u1uVxcT+elTW6en11z3mLodRkwOx/4Qnu9xvBlM6QZNLnYtVU3Rv9nfGK0/6Gyw0RSHhzQ4zhuQiQKbvMJtv7MG2yCDHWJWpmO9PWI05jcQ"
"CE2s6Fe3z9++9/iTp6LU2FPS0scQEJ4zeG/6bH7cDTPiPw7fCd35aATfCwP8W6aaJ7onH/ryB/yu+d3+2ga8fla2dG/zf7XH/6NrR6jKs/9zFL7bvmDozNTU"
"k6n0+Y7UW0FVA6lbftXU6flpEGpvNDKMSU51ca0u1fq4eYdKnDHGCFQmSzS3vFyNFsdCcqItuZKhq+yWynMk0UzNH0cdayLtr31qJ7XZbdV04cRqiTCnVFpK"
"b8IQS/1yCyWZQlFxb0Zh2TydMcBY33fuU2A+F/bBxLfIfGI6k0srLNKSkJxhAj3SCfJJ0pyqZegtVTljco5SJpug5XXnoePLCpmrfo8xSTCOI84Vk1lRj+bS"
"pol1ffjXahaH6b8zmZ1jnXjyE3LZ4AOJ2lJdnNvTXQO+2G3eXtGLSlrpWQcvGyIwQUkYFBwKJUalFnKnujYfptIlRZwkISw3t6IPxuH2YAmV6ITgIihBH/Ge"
"rOK6K1ZWXNZ6qUJ0r2O5ivgAnTdxaIWAfkQBxa+D8y2tEBjJiuH68jQtFA+rRXwHzrfUpsHwtJDTL5kRJCvYIkTsRzS70oIhvS7QyHGw52d3ONDb0/te98t9"
"wMCtdjQQWIGu+ahd+xqy66/FzfttYFUI8BCf3xPL/B/tgaX2yVugwfWxyvD4TeDwrjr8MRiiOTAGSqgrg0JOQSsqBxFDT0wg0mXsGStJ0sIgh1PYicNCedS4"
"+Bx2BSi/9oZTdFHMSHTZ/oLaAyxFWbQxgXHTtjZ72/ecOxjb9PsupnKbM24slvc0X7Y7kRB+LCF8D1bEVPm7U8rkmed3K9l/lAsCLz9/kHR3XX3gF+YOIXzu"
"/quymcJz4TeC1aioe5GsfwWfb84e8tl/u57xsVW84ZIVq2V1BvqTej9L1kJAQkLzE4nlbiwYJvrkxRVxSYPi6K1DzXZnGv7f2VMqr83S0UreZsVCcTV88k3A"
"fA2cBUSM3f1TadQQl+mS3y/0tXed1wu/spsqtjsh/Mqs7+y82MX7w2JxkSefrZVJ0KJYDDlKtPiTHSqqUUdhCmolqZtg+ZyQvdEm7QEougmQl0de5cC5PTNE"
"XRlUdLn2roB8cp3Tp1OXNCjuSqDmie6Dv4tCyjtCJrqgZskdRUmjWtfX1AbqC+/UMi4CiLoVou4M1t9yss2Suwp+g0ptOMMgTwvu1hZd3rPPOaS8MrjoYu0d"
"IXnqDMOgUvEbFXck2eaJLiDfem5UAsa20ngFkJ0ArCw3XgNk5zWjCjC2QUgLXJmMYRNNuznT568INHOPhsW/DkYMin99d/hkwyWR3LRbadgN18jKoPjyRk3S"
"BjuCk7jRWA7WPkjLIaSk5ORwILdWy/Zv/cD3wxEm8y0qQoz9O5Lhyj1qBD6qB4CI2QEkOPXF3mVB+seVrqbOLb3oK9sTxbZTwq/O67sarvRJf7dsusjSN+sk"
"guLKHEyDE/4nc5QITRbFooX1svSzwDVkMn40fj96xKGRQ8XBGcmcWlKpf12IWVmyUFkr2bjGaem8KhPdM263N6xV0+RpiWIUNJGjJyqCGsPOlrFmqpo0pvtM"
"A30huk1MWqvfoceVUaAlnvEMfSpMApxdNPEj8QfoyvtVhP4yFX1xinMa+6RN970xYqP9j/eba47f7C5+YcwLG1Pa7sJ4dygKOLQmFvZcc4SIsNwvEJZ0kBLg"
"ysJE0rVGoPmpu023fY/T0nKfU7Pdpq04e1fU0X5fVH3WoqSg9HWoPLoOhdBTaPBKfVZ+fm1W3OnYX1LlyjVebd11nmJF2lC2cY2lr7vGgi23m3CZSgUUh5Nn"
"ZChzCBkJMmOZ2y/eIOjzTLlq+DKhsuoKQTks58r6L5N1uitkRb+Fnp6eL43DYCVxafnpzEaBAiOJhtCPkwqFHSaiunyDKOgoLBX0m/Bq9QZe1L+tSEoisaKy"
"WEo2laLisrOKIxNJoP6Zarm8vv5UuUq4XNnQuFR5vDApB5KVjYckJdnloFlESOcx1oSqpua4iiXjLaonVVPamN9TUn6Ni4GmyHeir0EW7MIYtRFsBCEzf47C"
"R5P8m8bgY2DtEsubbU3bZzV3ZpC/C4PtZ9bOXLQs+kH3fX1gFInwLCoRl3yp7sP3EcnLF5nWB3kEsRgtD25s47oCsB6+x6/V+GGqxFL5R6w+WN9pvzqEj/xI"
"6dplJ1ctG1GtDqNeGFv6ErbDmGMyT9ib7bF7DRDvW8x1TYI0vTJMixafMVoe3tjGtuV94HP8frUf9o9YI88dC8Y++m41N7KnUhdKbr9eJX4y0iN48nbdZOFk"
"nE5AHD0gNNjX9tdXFtaQoMPCYsRgI53NYscDm5Jx+718qfNq09FuS0NwAVgGb1XLPhoZqnrxXe+69I2jTEhaFzzxdKxAOs6lLcmYmKH2fHZ+SQAJBB45FoTA"
"LZ88FOxnp5BjdRts3ukaedGZbXG/1IyrHDwgO2pTWF5QotV2yERtLToaPQz84Ot94wY+bWG5ngrFu8Q09HfqMNm6JrwA35KUJ9yF2QwnxEJwWZTMrJwcXEot"
"GOLwrdFbEXbjp7HZwPXZ0F9eA4TJwlcIosHG2pKyaK3fwHTsD7eiEn8D9J4FPuZBKGXauGlV7hDoPDATIH0784XBYgDn3Job9gpn7tYvf3ZnV+1XlykX4H/u"
"8yhteSlMP/GB7zxkObBf8+YdcUfqaMAANQ7FVSAow9m4luNDfVnvQVwWSv+43NUqMqKl7ZmXDlUtVF2aL/eV/r5NITdU5aADi7SUQOpk33cVy2evMusb6WNK"
"Nit2LZGdvMJtaLjGlZ/crnJNymAr0vE4eXo6O8k1NaNYkYrDKuKf60IorPgz/6TG4gKT166/bzAS66ZeSFZH7xAldfiiIn0rHmZzlDiKSYrKjUe0c/rVk+nk"
"gnAkF990D6UNxuRmIY4yiek/1VsN1h0cBCaj4HLwCatg4PtF9WvOV5J5uopk7gdvePY6a5N5tYASAZACCyDB/oc7RGurYcXzm+0tsokc2lrfrZ3X6fEiTgwa"
"pUojC6OagfdPF8bmxoBYAIHsQA77RCuOJhuiI3uTj8oBKwiyc2Gkd7rPA35ABKrsXzlmQpFUq+Qn0XXJR+uiI/VHk/WAqgQgBTuABEk/vfbeH/scbG9e/JzE"
"3zUMadrF2fX3MhCLKncAbp7YdLuZ6F+rSfR7tOHVcFibvrjqAJXH5+9ATF6/0K3B8Mjn19oHjbc23r5veYOeKiyNQVMVaTChd/NF79pgq8qRdy83dmtnKbTV"
"rtd33mSmCkRhmExWeDbWjwV8aBALZMnhT79/kFZj2B8mO+uIlSE0mhciyE/blW7gx2dW1XNil3bWtwHxhO9Sgw7aScYdsnfavf2g3X5bsXLQ7qsRoPh5eRbI"
"Gf9fRu2P0qoJp5nXi2RjoZeFn7blmBvY+PzqwqxxXn7apLakKEseR6MlyCCFedAZTcvx3OKB8/0Fn1T1CJ++2TFcd4ZftpR4X4lQez8nPxxf/I275QTeXdFf"
"qpf9c3H/zZlbQ7n1FNysrhg108hjUCvjiKTg4iixVadm5zqnauBeo+qLoaXaL+9VNQx/qqv768rP309AD84tw495/RdscJT5AJMhx4ZViFASZch7q88wxMGV"
"Io+qkdB4tRBFxKozULzgRH4RtfoYWda4JKdsilWc85aytsGd+qLHrchmyPPes69EO07gfpWtYHZimow6lEcipUMxRVkZKAoyGuaPOPD9kfR8YQqCpoFDKnNz"
"0PXNRAarhYjqFyZKEmdkZXOpajvg5rBlGnC4ZdrP0L9jD36bfY89vsKh2wFk2btNVf3+4cDY+EcDsd6U22TVHx/2G40f9UPd7U03GeJcp7xU1CGH2TI3KfJc"
"R1lGyrYFHiPbgNi23Nd2LNm/Dtx/xXgRl5TdSwfeBIpnAwM7Z/C86W/ngewXF5fJovdSm3iadP2vpm0pvhQQB3cA8eTUcyeXaV4W2K37QpXkY7sxVbP0K8u1"
"XRdeJ+Nso0zYeZcTCqOrwscaajs9BwJyL5LEE8Hq/OCasvSz7za6o1Crq6ZFLWmlttVqWl3r2WDRjndprvi4RhptAchhQk4epqONUsxupcSpvBwKukMdFqs9"
"m+qeyokJoPoJR4+iEvwpMTmxAVQ0xpJNo0cKnDPlUnliVI6g5jYKVjmRG6ACujA2F5Mrp5JtzOZh8uJ4wrWLM7u6G57HpkFTUyHQ2Jj19b7TAEd2SbM4GnHh"
"HPVC05TrQlp2F+opcO1nw7WUAmRThgceFdFRR8kfRJeCegqio8oMHK5yFOVBjQ4aKl4w8UKOMafQWPj7sa01gsF5BcZhY9jxB0sPfFqZY4gx4MObAcRBu7un"
"APEkzWnDZUH0x9Wujs5tvSRDvdThKfFXO/rutqt9on+tGeca5G06Eq6582JMKY4qjvsMalRkbAHZ+A7I89MRhFnQT4jUWC5SrKgUF3axQYnFSjV21kLXXLn3"
"tomBHVW3/F0S3HCtPUCSuk3rgIkILLcWKIsHjz//JsXvBoKvwHhnZRPIbQAUf2DAAjmXvwesFjZ75mtQ0Op9uSHNqPeHmyucU4Eq3jcsAClNlTpXua/nsKuK"
"8kv6zuXXNJ9n8owsC0JbWZbDjonAhIXVSKOFrq1HVvPyago4pQMXyPX9t4TCWcbOLkJCoSYojSgKTUaBcKFJHLyFvRG84O5sBIODDJLgX8suBFdkf1ko6t0v"
"kr826Ka/5tXOOHDluzBYO4usY7eagM+vm/ZT9s//zxq2ZjrhluhxE2mzkYnLr6JnjfAKUscr2IycVjyteg96BBAN67bP0aRD53sKPqzuFn36Rutw5RmuZjbt"
"lqhgIjPU+NLOADwfDTtP2detZUqVccP5y8MD4zxcadZROUpYbtAiNakYVmqioIZZOUbmN83LSaZSFeesWdJmuFzLvVNLa8/+dnDzFyGwTfR1bTlUsiQVXsHQ"
"0qm07Gw8JSMLjUdGovyOeU1HQAqlmdjGOqMyB61rRjPYnUR8LzdFnHpSpV5IB4Y5wb9FT4fsjQbBAf+IfLmUFchkDqWoeYYorRrFUjW+7VkWZGhpAQRbyCxA"
"ZxwRB0uP6Cd/6Mvuu1BgkbCx2TFuKTdATRraJqF0aHDtjYMT9IwdgyB9uL5bey3JKW4UX7ew60r+c5tIdTK8Xg6kmsRWXj00S4WhpaiVafm5lRlwJdGiVG69"
"JWtvvVWsMHemJOilshVk1XSYcrpnCyZWRDTDOqsv3eW3chaTG0SYUZ6FCFfrs+iNQ22dTIK2O5UNxpdUDBqMmorGY0pbs1Z+qXF98Od/6+akrydIEpO6CaHr"
"sUy9ic1akJZThxexjRkg73htXbWIlCfjKOGCsDR84g5L3L9NqTZ/unQSSW09k98zFSWZmjVT8R1kudJ4lVRDHwuroCJqqZZkCFkahVXVlzBK+5k6wHqLhsNm"
"xEX9jUrDx0dOIQklTcnklll1WTK0UJdUqAzOVQoaYjOLoGXr1S2Ezjh6KWUqmpQBR2Mz47NpSXggnBcd/a+xI+KMRkvbR44FPG5niDxN1jKm4zXis9sKLLWy"
"tx9WH7/z/N0z+bjTtafw5LZFEA5qa2tavzmPh8IYdjinhPh8/tEcUk0mSkOxqCsst5Wt7eeLWGNsaGwNswIKURGB2mQ0lrSsTqrFlNYMslLKb62uLtkRiZZv"
"8HstD7ZHcIh2dRsah3EFB+Y35st9bREhiQX+cf+mE9CEtGkspqINzW4fq1eQS5sKSOuDwPWsF83PArzj/oLuQDY8PiZaDVA+tzQdP3Hjwvs7lou8VCkzLKGy"
"i5ExAfS1I0ZwRApLRCSJm9KMTDyygJSVic7MUmCY5Lo0sSFHJxe2H4nIJGuYawECTsxEECPklNClUGFaL41PI6URY3Nq6fQkecAzYh88zZhM4K/RpON2x54J"
"3jEQu3zSCPmP5xXOOAqFH2dQOl9oXA9VPdkWHg0Jy42Bad3caMplRzvv3/9v8GdPKpi3AgjufhDvyCdZn05aI87gO+ITKjerMJqvjfjZDzhZme6VEdPZFP9L"
"4d59Fy+yPtO3l1w/W/dzxkCPTYI4LmpRFPt9lHr1aa7quJbLXFnr+1ERhVk7DFBn41WHodwtpu4Q7PwPNPVKHSlzVD94uasBljt1jLw1lNeWDc8rWIkCnh6X"
"g169pvVe4gS9HsBfSPHjBu3Qa/Jfv9Z3MgBkz4DETkwKOQhbQeJWgOVT0y/JCS6jDvBT0fUOjMNC7xRP9vMUOkG1myy4t+icBvFwlBHUUpaywCs//eIppBcu"
"++sqIdZkli+d2JIQN+TM0GvezOnsYEnkJ9Kp8r3WU+/f2ztYZw0Wezh+wIA+/UBApdSjx47hZ3FOPC9lzipk/JUzJeX5q5i1wfVPCseCM3OcYZK/QwS7NTlq"
"TWEWqqg37aFX/yxK0b7QXKY/CjQnVQczMwATxfwWy3Mz0uKk9Oc+YGwZb9nEqYSo/BTZMm4yNPtUdMw0CN+YGTwZmDtyWMhbXgvLSeT3lrNSWmMIUbBjjMyV"
"TJLELFGuCZNd2YlMky5zaDbbUtItvLFe+3FajfbrOEF09CI94MPoMs3r2bqRJm7u2LLsYMolB38HEMXV/rDy/yDJfIJe3TRJxhh782/VKH74pnXn9LdN1bYg"
"nReRCcci3Mnuymis76swXhIRHRULKciOl1MQDPtSadxJmzdV1D0zu5kNu2BkKZJqNQcmvBgpEb7M4LK46uB0cTsF1suo5V5ckElza1JZcsw4aNBRS0hlCh4d"
"khjDKuC/mSHgwrkFrApN7Rva1rdPrZevkFnDIhy0s6rr5GAJRdIJvE2r0ujXmSd6WRxyHSFfHzWMn9wePxfCd2DaiZbMfKYYPwKZfhTmvd3nQ39DxM/ERXuX"
"YSnwOGxUYqFP4+68hiIbYPpz4hV6sYY+TsuJqCxFFC9DGLH07O5qCj2a4tiaOjZa7pVUOkfQtGeu87O9+JkoURD+ZEKBXyy+sA11Lkw67YUAmwf7D3IWWfb0"
"hRXwky545EfSotz8cmLxsFNYGMilOvrQo8iRbgFFqXnEyziY1Z9hMeDE+xy8n+TbqVRYOybTu/rTp0AtjFy1pbJI7hm+sjwkCS8IRBcdIbNCihWTKRQ/rax9"
"RrM6fej8Yf4R+cHQrglt9psNAbtQQE39vKy2PuFutZ81TL5qS2Hmu0NcJEnw+OguL+kuNqbduhc0FB/hPYwDt0OwHt7ocJVPrsTPxkW7Jg6uHizPRgbE1qb4"
"xNYcBRbn4rNzD8G5+zoO7Gos2WuxvXBIeM5z0R27OW6feHj0JM31lDePecqL5HqO6DLtDYi1MLuRL9Z0GO0kLYos+ax0P13q1uHeQdq/LX6/0Sp1sdpV3egX"
"QE63D7WLWpJYJirWnHRDVPIvJjHaNd3IF2F24IiTM57h3hUf4afC06Cp+KOxXFe9NXoG1zZqLUx4+P3dViPWll0XrIgN1njiwu4b1metkGg47Dn9RC+1NK+e"
"QK+N7Eflx2rZ6F4fUHzD/xWyKd3/JgpoGp94zecx5zqzHBcdck5H2hLEE6k5FFpT4PA+ZNvgB+4DWThlYbQz/++JtF6fFlQ9+/RFqWfsJZC7SUrbzVIUVgg+"
"izixBL9gs/SVNTWZE8MaoKndze48Ln8PW/gNWDNvRAq7UI12GrcnnY48jDc7BitCawSzZuCZC9lNV1XK1Q1cM5d7BpecO0NAyALxbmYXaH5kbH623TfAcAaS"
"Hh+mDfuwaHNQLKHU4mm6yCFsz0rv6Sy2Q1YVY+WdwlMW+vH86TaTF4OM9pcTcjKTMWkxLE8l7SOWPTCg66664S9PVpq1Vu1p7wi721C5ZP3s+pCZuGi8fGru"
"6eq38ub9tC1lZ6Bn2d//gsbhIhKLfHv2cABP8EERcZg3Z/RwyaJYP5h1VbNtAjYx0q3gJRhpWg8gViry02MRUWg9GwH/0fmIk4ecikuikTF+MnxOVr+/Qcto"
"3xfZH+tlZRR2e1QWGN53P5ZFMN/zXXx6R+oVe5XQdcFPzU4wj6gjHzGWe/kl+v6+IaZ3pecURGAHiqwHw+hpEHpEbByPmhu8QCUlc2KiU+nUVMcndvEjT76v"
"am+3Xa/XMNFzyrfFJoRj5rkd/ZzSl8bwZSVnpzWUSBugaEQdlCdLq8tK8uGkMuo/p1qRQfBe8IUTqyQKNYyvw0cM+2WW3qxqt/hr2EktvYhspJF4hdtzqmfF"
"fcjYsjWF3R7AYuBmHZ0WpVvNIq0h3r3m3vAum1kfote9edq3NcAO/vR9H5Cy8x1GNa7PhRtUjT2aEQkyTvvfxp3An6Mw4UH+qD/XW9l1HFFJhjAZicVPezf/"
"o5WnklfsRWAgIveX5/6+Y/o9K2VxP8c0xuI6YPkbam5bWy6ValqspH4MGynpp8r0k/8DAPChNabTwpJTJnGqVF7FQnmBWfhY7E/0zSM9mkrhvYncRfq49lmK"
"lZ7oOjR8kBRN+JJYieh0Q7i5ri22uqOzlC6oqCBkirYUCziCkYF25F12KPMrdnZXn57+7J6j/5HrL9rmnYJvB4Ww0vlazlKobMG5Ju0a8UQ8cpIWW0D42bHW"
"i1k0V1M/hmUwxeWAT0pBvt3/u2pYphz9mvSlkphUkBbwThzPXqR3FMXw9AFJmUVE0QH0kCKfqLb+5eXesqNg5IdvSwZ2cmVtD7hvJV9lnhpZbEmk+2SjwGti"
"PuuKLfn08QzMzGX3Hz7vJUbN7kc7IcP/tY+zLmY0+uRGBPmmQeUKNCKVEpp/dCpFG46rN9fwrsjSo4QMSicEhMaZNxxpwhdzzp/7GNpa9L2G/4tek5bTwPKJ"
"QLxlJpvL4oxvXevL3u3OYqZnHqoxc8fWsXW065jZgKp3Y4u5j5i+2/9qNrwbC8y95evkfjz8S4ozZ9pZi4z4bWu7i+np1GKMJ23YNfETjR2T6DnX3TFT6WCd"
"Bes3+O8YMmZqZOCbmpmIcdHCnazjuWaKyOCB5UoPqHn8Bv+xe7sRJn0neSL0MEJdHVLT7FtcQ6pja390r/kJAYYf2w4rJ+kyWBN0pRc4YYnhJWLlgfQjGJpO"
"LKnmNRcjHFk4Gts/Uv1v4P1GrCr01z1Zpy9IPY8kfhjy5RvAeqADzaFUBiwb980tbVmd5gRx0xMdeGdZibK8k6wUejACsydtzynEh3f6Wi7Fq8nhwBpbvuCh"
"Y9bqHINoIq6YV9I3rdPvsomVPvQ0Or2TcsAmax81MDL0VrBzQQLZ5t14AqIUF1sY8S3sNw8yRl6EiGU4MA8hbIM1AeEH9uZTbQuBZt8CRv6hgjThCV9le/hN"
"boM6syWn89p4P609c9yOlKztzHT6LRNeII1OzylCwMMjsEXIXiQ3wH3R1yvNem+YOTLlpyPwfSVhnSudBYcrNOk50eZIFMPjg4ZQbClEsWX2uT704VuwFQTe"
"U4pNxcTfXB3+35E1wrvw4c5bkxLNi/U9E2eeyh34sfbFBt0Yzdx9xQxsG4ltINrzrvza3OsXeapgSl0z4UcY0bx3i/u7Gkcr6mRi+mgwZjenz+xgw9bDhzQS"
"q9j9ZpiH7PtpPfVKjuooZsWJUNwGbM5dyYfauNUvv1TT+s3+FZu/CRSn3kiSGMWWR/yr/K6C1dtyyfBF3BN2A0KSMoXRBA2fPtb1TNIZ4hy+a9c6Kf5Wkosw"
"oDN5LAPtUZ/2jmAHdyO8AiTCI0v+jOaUeakfDP5L2YI5XJwvPxvdmW5O4bQ5olP2QmWagDmwY0xGLmU6vCZRbHN7y0tpgx67GFhP+BaHIZQEkyLMEZHwCKQm"
"WvaFy3GsaeAXVyoQeplDcByUS17yvbG4zSFTF4O+3Cp/Zds8XBzkw5Y3nlYBj9ztvEROCsd5EHuIEm+zD1PA28MRfLboRNqds/Lo/XjQhIQRvDem+LSD3ZHA"
"9y9iKPbkbXyripbIkYXls83FHYO5MbggnD4XHKnW8pklNRIuq4b4OOM30o1DiZTouoXEBm/zkUzybqwDJVgNNKOO7z09xwWWsQGi05XRRSXOmqoCPbGTg+zY"
"+d2geSIumrsbzv9m0clnL0Wl5lb2hoETiuoBbFruSw3iSqBhzJOaYLU8agOhSrnJ/L0fMftkksAUOrUsgTwaWlSJOx/dyiz3K4OOfe6PfW5GCgsuNJDnl6ys"
"lwA/OAsuYxQ9UrPllHnvqGo/JbBS/LyS/AMrN54y7+09HwgJ8P9RPmMzZskWJ/1HIKbnEgAYRmxphpJx7ou28rrWZpa2An+seGb43cOSY+o7ATSfkMWiI+e9"
"Cwau4jldrOQ4XjGrkUGKKsWYsVvuwgxzRuct37zEnMKCAj6WG0l2Cy+A9UbPuefkzrn3RtNh4W4ROShWHiePDmRbiWaF+K7D46BnKbXB5kXBkMO+w4RQuBoP"
"hRVLEzNTWmIIUfBhRuapTJJkS6g8LTvqXRbHA+N4MUuHq6o2QSljh6NWObyqwgjwIhKLQj3yoe5gOQo4YZ5Gr7usxcKCBWn34iXHzZOJdftQJkcW70BiwYtC"
"CRJOAt6Nt4b87xw2PDD7i/lVjCAE46fzryWQF5if4232xt8nkVZzm1lg6summ+i2PsTj/mVCQKG32aeGWq56Y+X/QbL5BL1qXBvd9cd13vWzj+5dXLkkyuuo"
"BpwtnJ8mH06jmfOLRAFwBFgbjZAGEHzMfjkF0KBy3PpKDPydQmDh4bTCDzxrZ8MpHh3PL45sn7oXLyb+ua2kDuarqEzM9DJ7ReGTEhlgmhdT1CrGYQmEI3iP"
"iQcPfjTCJBQ4L0HQS8YZ5UrG2XmpslhPLKhBd2JqmEREqDkKxwsgIWbeVOdUhFzQIw5pVgjJK6X/+U6A4NdiJ4tqkqBeZs9ITHQi4zDUlF00Mlgg+ZbsqvIS"
"b7Rb5xewZKtFRJarxW6L2bEz6y//dg7i3yDLOPnx5LC4XEhUKDE3+42v6chI0utdzIhN5iS7qhiZH8KtZ9SEle4/9yWU6s/PonP6JgsY3Fkyb0Qc68lMzXtk"
"FuyBp5n5im/i8n3/b/sdd0FtPzm7gS3oPGM4OH62AFAfu6jkb56aU6XWVrvZuxb418AJggBEoNlfOp1VWpu4ox9hijM17zHmChqO4c6yuND+ajK9pFOdtxPy"
"kczxbsLjhCwcppAy3FVEc+Z4zMfp0EhgMjS56ly26N7t2MuR3sXTbrzWY+8LR70fo3xQq/nmxy7lXeaamPBJo4PGej9tDGjhJ9x0FbEXfqTth3Bz5OHpfZ6c"
"WkfnYRYVERdRLnyMGz1hlhA25Mn24978aUiw2nkDeVeMjJ0cHfcBJS+7wkO0H0uZNX7qd47c1It6PPZq5ac+xGJFPt4ZmOqiPsAdkWIwKxMY7LwkHeMYft+x"
"AnhfC4+k7364dpa+C6DulLje5+rzHz7wU5JZnkJxIRLlurSiUHMQsagg9HRDCJ9POfBk0bHl2CV3GkR/+BzEycnoPKt3b4WA+ksOfJ3OzeRwtqvqoDPE9mGr"
"/Vad0zwE+rTONrBu22met0qCOOySye5UOib2uK0D4c+633e97PqA5yIPfPznSET/4+57skCuCzg/wfHhW695BX3+dVwxIU+ahHdFHHoIqicG7OOUEv35B3ZP"
"OhzDxl5vRbg/I4HgOKPRyRjL9426cV7dGaXZrKSOPx0HmrPET14oPW7XGHru1qg9XnzyQuV0p6LHcKdC5WT5xms+85augFFYU5B1a95rLvstHauQWc3KeAtE"
"DZGdr7fu/6bNOPJ9W8f+68432vd91zZi/Latbd/2FzG9eZdGuUL+CJd2qTeml3Z5jMcXjvCol0HQ5on1ddHTC2WN9ZfKxE/X1/c++/1y00WF6DNL5bPFyjmt"
"qnpu8dmibkWl1a6ALfd61mbmWttOzq31rM7Mt7XNzk/BFwXoYjgcXSxYFGDYCBiKDVYl+f5MyCBk/OQLBqsv+P60kZ+n0D7ABlcPIyABwxmgzgBQC+hxUjsF"
"LSfJMlhhQxZCTmAdBSihsrTcFkJapfyZuxfou6Eep+s1uDIDKXwT9Cer0TTdZgqtpJfcIXRmZJUCr4HeRrN7+qIx9j+tU/d9i6bOOEhtLD+7aUt75ED/V5Gf"
"aauVPhgZEd0+rdCu7c45Q+9OR/b9+wSltb6IOzZYUu2zPHRfzVw+fdiu3dnNoY/bIy/HIgk+bZrh4PO2Ruds8GVbZtJJ/7ZHtHGbrC5UM2oYGQj05Lvvu/fJ"
"cNvc62iHO/v/NDtM+r7am1bSPjT7B/6gA+32fPYI+aBf5OM8MmHgdxX4socvtZF+bflqFXzTgbpBXLyRE5MWDCW3mLAd2melon4FYJlPd/BDV5agQ2RUP9Mb"
"/1U11vG3DNOrMqwoME9XOr+q3NYAdJ2aDPedT8vb7TXvPVlBlOsmul5uUOe03sUbez9dVdR/DrAToD52bxmutX6NacwwrVWHtnvmLA5NZdyDlg5UyP8T+6aT"
"DB4ujnqN+ntpnjio46muygneW4J5A3BEamp935FVBaEchCVJZqe+pcFR8UEFxzX1F9XjITRBtcV9g1Pa6l7U1lRQD2GdwWmtGxfWtaDPoeU5oX55ri2NxzP0"
"vzOxNSgRkq9Q94wot0CDXUWPrVRRaGc+0srsTqHfo19M9JBN8BlqPcnSj9yt+d7mCQE/SV0Ddz6wtv/F/Cld9s+iFtj/ywB2g99W8dK3XS+2wbBfPhuq8N3L"
"ICmTQb/HH0/QxDtgOQB92zBbe5olVyn2vc6kAEHPaiB+kQNC8n8cMdmdy+/rQaacM0GlAv+unfP5Y97uPBmLs5qKOrfKSQbfg1Pd0+SeK2GV7XQBvGJWozjf"
"ODP1pL+BPTAdZJdPa8M09BT6qxSvHmWegiQffO4MNjudDkTxbMMO1MGHBAU3z9etvnQ6UWdy7inANfvkI8h8mDc5+R8TPqSIwKq+tU9qwzXUx0nPHhm/+Tev"
"CWwaqS0vdUmWdgR8d6AHOIZX9BvzTnRDc2a6kn1YEsGzRUOXqQREqZqLWSXNON1TuQHOjZMTe3e77nS9SOc+Inz+dQ7np3kJMBLQiQ6yF+C9RpiRPR80kB9A"
"DIOR8BYK4B2cbV/RAA6K1RkMF1/ZCB2vEN8qSRpibAXf8yMbjx5yQCN8zcXs0QMOds8BvF399FWiKGoueFrmzpXY092YB3QWGurTuuWGzgF4/+pAiXqoZX+2"
"4eS0MCFoIOSi3dnr4SWTIS7dX5y48Zp76fn/ewrIm8caa7hlgM0/HtgB1mB0wZ2oAUFTBIR0ykHpiH2lHWrDCSHSUmqvADJ4dyJLmIbs4PZqj4zW6sXEv10G"
"0hejVvKFstrqm8hvLLlNOsJS/HkaTKZFwf2ibpZb7tRndHe+acwUtFp69lO3MAE8yFZnGQTfacI7b3ixxG59SF3ObibMk5z/enKWcHw0xTr9KECJj3VIasPU"
"9zT+5yYZmlkgAP3Q6mNc0kUdB/irwxIUShg16dTSYLMss0mDl70vjaJHPTFDG9ZUzcDsnNNza57Pp/m7Aixi85a/LVuzN/fRDsQlkSnJYC7mfRER0ult6doe"
"7M1+bU8qAQ8m0BFSQQ/vcgoJSTfrLdrQBaUEhWRFxDtOx1TEAg86R/ObkySnLsfyyyTqQL2gOitZ2Sr6qJ9rrqXWuMmv+7SXrfQVfaaFPdivt7+l2TXqtKY7"
"G8McxVRM01zYg2vv0j5n+Vuzk3tm3RuftFtPudy9tnB/4d0d663ePtqWrY+Y2c7n7rwUb4h3xkfjK9u+su3H24kGLK4U50lVsjsxNjNNid3Y/We7/y3dnJ5M"
"O3Y8auEtqiWX6qWoNCq9Zc9f7rmUncm+77zXOvKS3uv2/n3etOvGbpt70b5X7fv/4uruK1O5jst3+Wfvf/P+v97/n40LDaIR7REqAhJCYVmYUg7lUy/9lxbI"
"QzjR7SoxFP+stbRTbbm1O5FaukiR9KZe1ad6dIzLp7zsX/J3hm5sj8KA7FDuyrO4hHt5mVOMvqvUV750OsY528nvwf13co9t73R7kZKkqqqG2nPkL96ekyf2"
"ydPKxqukZUigi8qgVWHaK6/KhVkzZ2ZkmrJtnJUaeUMSv0fac7rPrFwZMttm33yVKrVS63RGA1pdLnp+4HNWw1q0qz5mg2q/etryjW21Nm6Llreeb1rI2nKF"
"D7vb11zeO2vjLdvaXctvZ5d37Ry0Ldu1O/bzj+A0nz8fMvbWnx3cEIcyrkUr70fjxCYpO/JsCllPCdz4oQ4W+BBGCnmwqEHH4oLzdJ55G+7QPXm12mperO4K"
"VKzwKr4J9/8GX/I5rO+pzFHPhqfXWt5a19rUutGylEzlJ/laPlKOKN/zNnuNnuRvVb03e1yahqCxABMZQOFCha8udb/Ujf7s668eSlx+w5b/eZn8+2In/9xF"
"ZXxyz3998YqrGd26Hn2wI1+t7ZMwyzjt18epro9Cf4vZYYpxvC51cJM1Z9YbOVZTQLUhRVl+/7OfHz/+CXX5DkL9DD7wfrb7g/+9v5KHIs9ApVdXQK8i63ti"
"GezDWJZvAl0YkzsIA/xcNKD/CEqhSam7vnTXwa9uMnXBad7BqeMEYiGdX69nLFL/qZpr7aA3iVZnWT1TtAxwKw4ThGmf9MejtV4XlYPp1FPcJY2I4JnQvLMH"
"hSPDT+LyeUqW8QOTw44GBl9XDNL4tXrT5Gmq1FZvDN8kHGZffVrWM8CsOKaWxxd67LKiUOVgKvUsOyJFdGyJmnf291R0YvhxFLbEPtndPyXsyBz9ZQ0xVea3"
"zTiLSR0yxapTQSqIVfjhTyjV6e3gVeYX/bAXED6lQA5EF7M714wSQLnE85lTOC4IU4Vbz2U0UyMwrMrczpEjn9ATlrYOhzQQTeNjnxW1WniGsSq3+F+QHiP9"
"HVBtmWyEljTCrCbvb5i68AkCivqNAM5geZcnXy4PADRx2mmaa5wD97ixOPMVnkwr9fpEs85eI+UU+zYpaxpy4/WvKxNCKOEZtv0JCPej+rFBJuOER4u3ZRYz"
"HQkswD/+kq0a33lzkuyQ8W8Qh7ycfp8r0cae6CA8gkW969WlH2YpBVguyzlSVOIl0yqFJvorikVMGglmK1LVQO6Qnjo4aiPoL0KQ6uAgIACTTEaJxC6gHf72"
"M/Bae/WKa/TCKah6mRRsHYHxs8IU/xsyd968TCVXH/vkyfT73DH8xJuXytnek3c/m/4wS62GlcF2JZN2wqNK12vg4102qn0+iKm0vs4obQjUKOI5t9KumoA6"
"PE6lGEFaiTsIinOBpvPBhXGRmlGR41Y3Vm5dEy4xISQ7y4NqOrF/IBxwnTPJCeOiL21kdWrPiHpV13nf+QtGjHoGsNsiGGB83sXJQsPwcbHJxWgjmyXnunhU"
"6S5c48R1/fAOvPsv+P70pSPGZLi+sP99f8NPEGP9EGAv2O/ToOHurfbnOkEb3Kz8sP+6fKVVniVVI/liqsAuzvoB7UOCSAApAtxdlhx1UylVQWFlx4LXVu1K"
"MQ9pc/7JKJ2fBU9nPpSPmROS0bXt5Q1TtU9gnbTW/Jrvfe5kN/2Bq22zT2BFU8fz9PTynveq+3XHuKzwkcF3UrtfbGnt/7q6VgPWEQuM2CQ1h2/t3uDjtktg"
"FbXuqrc8aw6efteTu8qOY4zSi/IVYfANY/cz48uP03D4zpvOv0FcAXujtqwPGYxitiTSga9Ztfn6yB+IG6cnGm1e7XzqfUD4DkxfNU602isoRQ961ar668N/"
"JK6f6G9mePNnfTAZpAEp5BLbs64LSsdVy1rY+51VEvV6Y/f5LWzLXnW9QPbF8RlkY9yJryNP92BP+N8Wzm7Ks7DP8M2jiPuI69xxds7aWoPM0N/LDWZ22JPf"
"ffYv88ih58BPkwdSBGPV02O5Zek9taY/cCoNOgvQBZZCANVbtIXFrI4Fy0rYu6esshZHq/Kwu/X6QqG8KggE8IN3ac/lbC+uGn9vEMNE3U6VNmNKAEuEMoU5"
"07E2qAJNQLaz/iWayMpGmqQDuqYu0iwdq8kiyUrkNJ4XZDkLI1FS6wT8tMriFiU6Ry7MTJ2XRRkn+n1qQHldyuBC3zxRnDjvMozxJorHZo/UMNY9QayxMKAU"
"IgEpT2PeBKJbfb9iXbCV4t0m6qXE1eBZgPm0a4wggmGsGreUyIzZq8kEFsQBxqgaw62saghh0Sbm+ztHYGguwY+oCAs2NtzIS2ziwjef3tLjCvHUMlNj/mqy"
"oZXCIdbkWn+tN8UOfPT22CHTgvtg5dwFU/eWqRxZNa2AFX07um8sGxYz3rvtGF+ksKhuQck3vzIiSZyudKLR0FajsEXVVAEsqHuJPYNPLqV8aLSyGTxBAinc"
"SsXab8PXtEKnuZ5wWBlwZ6P5cUVc4/Lo2b9nNJkM426A+cJLO+AagkRuqaL3u/CVKsq0pUMeq5o7a+izirTO5f45+3Tm0Mf2xkr1HquprsneCEMwN7g5Jdnt"
"JjqDtaMkbz7ZDeKZn5qiN88232fbWFu9nS5lURFisf3Y5JRP30k32v9vYquHIKd/c0r2iwj60yMEaz09u4FnfmoM31y8fbdtY4HJdpImehr9DzuMTefqOV86"
"Yu/rYXcEY90Hx9cxldDGrERE0FW70/lqmOz3wvMJKEZpqx6QTVjmCWnEX0zRxsfLTV9aBy7FK0WmSDOiqYkoWFZ2r/YcZAwk6g1NFeQNLTSknmmiaIIrq0LF"
"oZTfOx1JVbrDg+GiNTXokm1/ZrU4jHZBLTXR1E1O44sUKZaa/jQhVVjJH0aAQ/FJzLMVFop6ITIWz62LIb0Shm5UajVbBzksNA0VD2jNTsxVd8x9t/nUX3z3"
"tke37+GY2ys//94tj28JnmYqC809S4UalayUa+y1JuDxbJp2nwcSzm5WIgRhtIQ9aWav7LCuZQuCmOcAGgdOcUjPFkgo9nKRpmbHXi8Ad2+SsK4mvyck9nDD"
"bI7/BH6iztbnD1h+6IoX1INkCidkD4ur4MHphV3gF/QXqc+b+OmjIcRsy6N7JjSD690AKHFY/+iRbucrdxe/LAAThuePF1iNIycro7c+c9f5YwWmM3HQDPya"
"Vr94K/lY1LndGDQ9e4Cur678u/7jKeN9j/x24DJNsb8VLk0eyKvN91Ii4vuZl3MtLT8/cf7+NbNz2xg8fPf31cLwz6bYDzzx/gz4dmxI5SN3ACHwE+Vfy/1h"
"dfPWhnM7acWPipS3l15dFVfAG2gfd/6Wuf965uW/v+pqxt7z/ytvn3sNCjvO9s/RvOZYVfXp3q/cuu2+S9WR083kCOiETlKwo23tr7cMis89WLqiPmt1GlR9"
"/qj4TwHt3w+C+yACgMb3AWAg4msf3Xf2J3b6j9v+hsV9CtR4HzPP8b3bP8Hpy5Mo6nMgxYe0jqVvTp20aqLR6EfZotRQNg8lKcpHXjp25kzMegRR5elh4OsD"
"n737yVICp0gMYqMMbuEE37kQxBvGCYIFc97fCJcfavBT7nXJPhLst7oHMvA5hkNDMOOAlI4kgiVOufuf7IJS3VJ9HMfRNzaJmkItLXq5ggyI3zC1vani/eik"
"xnG3dCyiomFkHkCWQZANAuwG87Enw/+sL0KkYUXgZjxbyxw9xlCDhhxLlTJVJyisRusujhSvVmNv85KW5Hq2VuoDi4S51sXI4m3lLku0KKlrebGqYe9+qY21"
"OVpUVOc0xVj21jwx3nT7pEwBvObJoaNxeQ97hcyoDIZ2nbQ34rAgTWZJvlH4/B3NVgW+Lrp0WMilExHJIAxnTzBSR9PbhNN0nsxxPQlgnEF0hUQuZJosCDxv"
"MZ4RstZ+q5dZvm54PrIY2aIn+tQaqfZaxrIW4x+22EW61MfQ389ApkkvTMcQK6ZogWBaT5CrDvX4oNfd5+bY5fgdt0ewXG6lvgOubOwQjwe95r4yO3Vi57Dr"
"Yfx491alIbUYDjqJkQ2tMjVwXqAirUAtEivs90TAJahxu5cKaN+EN1ve/cmWdVOeJnv5JyBQwFqDrRC6Qv2132XSiVg6X6oZylHVy1VeQhxbSAU9Da+Di+6A"
"kM5rfCSWpTCUF5AD8FK5bDm9mgBw+G7Ef92F3aHuc8JxYUXTy5gv+inLw08HOndel9RLZaYkHGiubKhx1PMhPyyVdvM8myPo5yOKJEuqwlSzjl2kqC77Wp8t"
"6/S5H5zYVJCx1w2FySiJofWovYjqwxIDeYNkm1zKoBScdewYlfMkEf1fRz111O3AWiFDkU6u4DgH9bSiNY+P4KV+51BHeY1qqiYCKdjXCW9YNsNCPvbtYvuz"
"jCUv6Pi1J09Fq78/2EJYLZteLwx7nlmGpvpBVnZDctDb0TqlaIxJTmPnAPIdcEIR1uUsE6riNaiK3FjiU+PYMipfJBG504/XrpBn9oMdxf/w5+KBRNtStLmT"
"Egk1SDh0ke13+li0UsOL8eEkm0r+pRc7rZz9pceoQ/mWp3m9X1nqU5nGJIPEs36+KCUJxzlxgxDagiQQUyCdjYphRsHu2U6WsPUNOvozz+q+FO1dEaOC3QKe"
"5tZYXdMOZYmTqPPWUAuVcEJYtXrINQheKUORvbje1Vm9v7pz4O9dXr7sFwDbL5F/ORzpwmWXD2py1Fn4uB88tn+EW4VUH1ZST9ASaVLWT5/1M6rT24aoM9gh"
"PsxW6nceqIAZB2ZuSwoLEwiz/kUnJ5BlMv7GnMmHrYPQUwr+OD12Ipc6RUcPDng/cU0CvO6sAvd8UTiVEFgR+mXHqUckNt18CVREmV3Pet2Xff5kyetDCr+2"
"442L5F+A8iRB4+Fg+JrfwOkcCZ9y2VX26SDlbvh6YQ0T2168CQbAokhpCMF+snCHzFxKP5ONuIOrGYRfiEV8y5VNKIxhlvi8u8JxslwqfdobQy48lRS0WMGr"
"heHXo9VxOGEgdAY3O7UVN1UFpOYIvZKCq1BJ7HqBKO2juOzkuWSpmKX4mtTKlciNhdbRKp4jZQM4aGsSSzAiNGY1ZbnWGayQ7SjiYUb+rK1E2avdD9+eH5Wj"
"r4MIPyRgHEsowFJKhkIQxY/nek61R+mfNmKY7CqG5yk1s2qG0S2T6Wt5cjIs0JEh2iRDr24PpWz0bLv4AjWAentzY619L3eXsNRdDmtYQp4ksoIWy7dh47M+"
"zKxZU47APJdPMr5rXVvMk1hCYBCqY7lfDOY7tr5yEY8tPS/JoA2TIgRocUIDtZ0qyLG99DCXF7aA2tDfOrUn8Szx/bM4L2GwVBjI7NDKoGOa5RcqAnFHB64/"
"XQtKYf42lyOatpwnsZwn6MDmF4PZ0UI+zQYHkHQxYTvyy7ZDdmvogubFfPEQxW04OvqQRik/nnMCD3dv+VgrqwwbHXmIYSjK7yRAmyFuCeC290fOpHZn8emM"
"m0N5E/V8Ty3Hu1x9MlgrQjYtq4DyPaTFKo0ZRWIc/ScB0jMkcLyQ0kIhKKHeD5clZJHCxZFAfWBvT+kNzwWa2OD8FyNjPbK7bNy2ab91Lsb1d1+mrKS8H116"
"BYEjn7yRHFxtFLPGpT5E3COhGOLZg7kSwXDqiAQQCv/BzSxm/UAC9c1CS1Ztyz/R0Pd/RQOOs3WYIjT2foXGcAQ+e0g+YEysiBc2c+c65M/bIW9DyPilpP/z"
"Bq+4poOlWI8VwfwHiw8fP75794S+Ci7iIknlQBN1PKhZ+EiY5SD889fO2LeaHqX+oNY5HiWk/uhl10ywBZdwEXXYhvXsdvAx5/ie+ROvndCfij+oQjM85ZPv"
"/NwywQCl+56n5uoak4UR/6sXq6GOyaVmoGd1f11dDUH2bAkh0SxO+z7/EY4QDEvCppOzqTRabu5vv8FAFajCoqLAZJ52cxeqOYNFF5s/+cXWQrfCMWR9JFkA"
"iULF1hP3dtn7h2vHmvbkuLpEUl5L3Q9PmWO4YU3LIjUuEkad57gRjwI9DYhh1oXmOh/ECuaWbGjbXpywnhrEvbZ0YEsl3Ngqnki5H8E7ay8On5+89DhVf3jV"
"t7NAMFCGOdPIlD7Pu0jVTmVl7AZsJ0B0nu74Ymoks27JCpBlIJ3XMyEdjN8bUjPqGMO4Lo1KM8O55h9NEatypw5yImLWC+4Nd1fIUtkShtKzdUfSUMptUDXu"
"8KJN1Pf9hBcOCLjUzmOjdklaoPt3x1efOWcSq2LMoiLCWLL5fQBXyDgrINNk0OZ6wAIs/gON6zZTu5d+6wW/7jfjQET9b0jX7XlLDpe5LZD9m5r/pDvj/7f9"
"y6cr/sGD4BUQQ4b///8P7shTXgRMP/Xq773Croe9Zcd1Ggq1N1+7RBSvCYBSkj5KvfYXezVpO10L/Mj2H7LPgpswWjOWqjJPL98Q2khHHIc091iq74dgUMxJ"
"SmPM5EgyS4mKyMGnaQjP5EoBW1uLUwjkrl117izOLp5FQISlcvVpfTAL7vvxa8p7xEUMgcEor7WiqGpnC2GD1mERv0fhMIsw4ycyqnZvHtQ0rYKsV1yjhAom"
"6npKiTg9K6owFMNQwW4eOwZKAdoqrdpMpWxmIejTPy3lXnX12heHT4Ya35tEcZJQZs1RJuDJM5KZSgtwqodCu0C/PqClMQ370HDUxoD3NVjipLR9WWrM5ynq"
"+61JH89+f7iD9uNZW2d4DRZF4c/+DCERfvH7AKBT0HoW8X+997LrE1/pGnwFnfnGO4XHtEazVas1g7CvNLNZqS+8YAZM1Q5ju9CC9GtHM8wv+eAy64KkTria"
"GRmdfPuNA6dO/RLsYfYcks7ux+UHC4Ah6yEvAuxX0VSP9U0YSHlZ3UxV9Q1qs//uVT0cPwpHFMXSsIaQIIzp4UWWlPTcEDicTXNqng+gqNkcSKE8tPlvCF+w"
"Py5/K//S7vIfC4fHYAmJB4EAfQbRhy96Zc/7aaUyorJlQRPFtFT41lH1Fjp4/ncflvT0U1/zcM7+GtiWgtz9bS10PoEPR3k914INhMYbBVTvz/BOC2qiQFEz"
"7If7PJWAco8z2DWgp/a28SD4uo3ewHV+/nW1IwjMpRlmWz4wvvKKewVeBC1B2UT/91cz/3bK21q2AW1k2+3nxk6jLSZL7U3wIyh1/xijrSXg1MRNpiEoA5aW"
"Oajskk3ZXvwHgDj7/TvMIlXgewa9u+J+l7pnZiIoR3PxhkdpztAr3Q4T0zZKhOhUfnc7HYrNYXzhwX+9674cD+JG+Ob2UOzXLfmkHw+nUjitdMixjpKKYrYO"
"I4q/hZUcCyTa0zDu1JMLpd2HsWPOm0XWkqsa0iH1xckE1NAe20gVEG6PPTrnOeajDGaGXzB6dK8V+8E2bcJZdDu7iMokmshmk8mwz9OYJlVQQlvXy960iimw"
"RJcvYtd55Sak4hBXFkDfYD2rNlAFFYo0KaJQnqmRqnFmZA/WeLBZ5J87SIRDYeF2o3sc20iYGxLdi34xWq+J9/6TNGR4vEynu3iUuSbuoAdjUx2sH4877VtL"
"KFRCrV4HG4cX9makOXhAG98Ku+px9Pvlp6NymyvYqDKjqtCSPbeMBGU2t5DFTrlIS1gsUJBcldjLRCY3GYG4/poliEj7/DEjKRYywwUdjabKHkpmUmlQVWwt"
"eRZ19g6cc6dVsCaTAx81ce8v+iIY8PsfWeCHAt+2zNbCzkGZth/uJp71EahNaUQXiOOeVVToReXCXu9eru12d0AXCjVKCAYRDVqLjCaa0ohhS59EF+uCUTki"
"8irk8/gjtqeeA1LoQ2tekKQECSkeHf4QR+hebByb1hh9M5te0zVhy7P+F7Uv6i3jIHLoTYxiAzd6WGqXGgimsy+phZVGZ/fGeFQWSq9osDPq90dh9cPUtQ+7"
"vIhW5Ruj83HcLYi5KLYEkD91QINglGVc4XSRQ0IoSi2rArFk6AH1xL5gBcSrqWL6sq/zKVC90iKlINXR/eW7P00G5zC9UUWmVKpATE+OvTJFUnqTgA1dB4KY"
"4JooAGaRQAK9EnvNi2VAEgShbI2rG2nYnLH4oY0RSUgBBUyA/I79mYQiYXzGfqbgGxARvuWRslhZYo9vdkpH+63ubjvXRF0x+stmkXBEV/PKPCQIodfhDyLn"
"Hz3Xxh7txtnmNzOIhOC6k9cS6Ok17x9ATLROMjQKEGTZ+7FhfxwzYUaYUd5XQcx87/D5qnhx/W8XOVt+C4Dl2MmUFJgVb6Mksi1pKLCGLlcpNoddUuy7J/yA"
"KDZEkecHAyo2imsGjxw2TdO3NOsSSaJKpcnlYwVASKirGhZPQFguG0z8bCPYr6j11wWNGrHoY43AXOCV8HnRuqaw5kXfiUjWfvGopuIgQiQl/8umZu/zW644"
"028Aj1ZsJ7JfuJ1vxqeN36cBTRZyoQvXA55/s4U8HdkQVZhHw+TC7k+OreKS2YjY1KMCv1OuNQL2C3K7VGaeq2u2Ohs9Xgu7X0pEV6MzvZo9sHQKazMepdMB"
"w5LpIvWateV8VFZs+c1lqDHSofobxsajTTyl+MQVEErtYdlCqsBgt+54yIdxHA3iPu6WJfJ8zRk90ymaI84hZQ4yw0yv9IWSrnDRb9QAR0Ko/x7vwO/ZHpQB"
"E0d4gNqtbcHvlTveeC7S7h88kg1f3tR9o/6g0QwGG55tE7Paj7EYLtZj8Ce3P6xFTvlDLYwLoABBsXt322sLFxJEHpMnOEM3y+UfDlmvI0/UUhLTXnuF8kod"
"ULnc9S5/OO+vnQN9FJeELF1kBaRLkaPSTGf/QkyrackYUbNUkhtCkdV/Zs/xXW0nkYC1Da2TaDQ0ygymf0lyRTn92okOBEQVuAOeen3ew4FIwJ82rmx8qkcB"
"p/+2X76POjqNQfjHBU+/ji54+g3mJ7ft9VuUlSMua2t7X9KcXKd9KCnerb+uZs36zAzK0CzEw6Ik16VwNCGtuh17XojSoGsuHtPESBBionle7beoaJvA3K+F"
"67as5UjQxWfR4D5C7fB3TyjhWAvZJbWbzqpm5YM4Cowwtxqu97D1AUPybiLKRgWsk75xFWWrqh37foQC0k7j8QVipB1hklm+83sw0zdu8TvW5YZGZR4cQ67v"
"FQyadupV2Fa0Kztyt/VrNIYsc72yWR1WXZMyJKkZdTwqKeuAoZsy8JujzGSFVgThBYzyh6R5CLLokyavSCyuIBYs46ZWkF+SEwAh/lgs1RKZdKAGMA16nqDz"
"jf++B28DYmOFt8rCda7scTiBdItgt2cu7SbJb81xq8k5qGuGukMOig8/JVwxoCEzq2FupfJVfO6MhcvHSk/KSd1tYKhqrQIherApimRzFKehq4dAAq4bnOF5"
"gIz1UGjTqs2Dl978biXJmQeqqVrYH0VxjGTxuE8QQFBNXvWBlPHQBHdIGlw2ElZH1QHM5M7Ke8n9TIkYy7Ymnvd5tcVSXrOP7BpGKyXfJotoUOKWkmYhl0rJ"
"bDM3w1RVt8iYaNEOAezRbe8SDqOhVI0D1mU/HGl52yWnsCGDUxiENfYI7ZwabLC5TJYCnShirpuyViyB7wgpw0O8XgbGBF2dQ+u/ZrEHOl7rbowwHau/+Jge"
"sINF8ORzFMVCDOsbbn8SBOl8K8mP/+cE0zZGZ3sqGKvLSpCvg1yd6n0DEJJo+jAZgkWyhxNFDRyNwg/HCsHmnKR5JalCtg8MtZUEISIanFXVXbW5Y11OjSSI"
"6uxU2UqS7FIA4YNEkVN2D4nJNYl0M/0vSQGRerRllEj0Q9y3Ma6fsyWdCaW8qhROclPo1BLiTx+3B17Thm13jbqTSEHWAk1++b8mGDTuQcZdE2A64Cu6bKtM"
"KP2yZjFiA2HD/PzXIJAaph1s0KV+UFgS+/BKRbJgqhRK+aRnGBNfuKuH+uXQxomDbkUpXhmyB4PvZ7JlxS4YNCTPL5c+iNJ1pzOO44mHGfw6b0uYRul2JJGx"
"6cDJKDagdvUSuCZUNnw6XP+W8/TTUYxZHTlo0yN5SqM0gyq4Hubs84wvJ6gWiFziCQga163h82acEVirgagB1UdpLqXVIxFmd2CUD7mGBY1Rz7tSMjUjiHgQ"
"PUfq8UWG8bNDgx1412yfjd0c/iC8uRnZhK5KfyyLVirq4nwYsMAKgG5RANoX9NZo/iIlLqfqg3Fvhi6AM5fOXU9n9Za4iMYvW54QBwTgzs+m7bTOzLlQr8aT"
"utcxWUQag/SQ4U7rZrNqLiKztD1IKLVgZ40sX4BgQTTnUtSVHSYvb3ozl6b7bCCjcO0ZV4EMLbtR8t0GIlnCtL1+C5Al4qYGmpY024yUSWkADDsHFMU1awpC"
"qsRmIaufi5XsP2d0ylOKx9tgzmPVceYQJZjrIvKlEmWYuq2CwnszRYZhOVlTIM2aQkmY/lzfAQGGbk7TaVwx5XSNcqlatSvYrXnIdNwrsOzf+pABdO2+Avhf"
"32I5U4CyYu5aACQZJXzx3Duj83fpocQOmHfzAVsJ3McKmsD2tLjjVmWezK3+fs9UePaauyHfK6MIJKXdRnWgVZinm3EP6uziraSBId1EjzbKcCMuPqgvsIdm"
"rekNpkpTzWFhafLuKOLHSORG4NVEigcqBaJYgINOYg5EhQWya1GhfBiRXwUz3bePNv3xWSofH19FC4sthedvPr69fNANXTeYcmZ+EcgAp1xSom/ITtz5DaNl"
"4HyiuPbL27+dsNwKnj9j44tQcHcplOtHQRxIhtVUmSlckRMBDKJGbf8aj+pUzkA1jsPbSI0wCafND3i/RR9cDMBs90ue4LV9xiIGAGmH3tjrWSM1nuCo45/R"
"tW2vf65vEasW9LeFxRXr2R7TbLT0SSR0wsr6S/31SLoHq2F/bUSkOp8Gbnph5MoemFbLRBGktHNjbzmyOJC0Nnv109TSQGKiEqAF3YolTs0scZUH5mJzTOES"
"SIGLoBYq2DeCEAy2aD8ymj984WgykMnRuofcPbqoGxZz5cAIfh0IheR5FgV5ORlJu5J8K+71XI0X9EZvOByl1UXTaOxCUSFrv/fei7AiZn8jyI9VTN+FYJlP"
"gWi0azBNEJRRp5N9JmvbbdXpVQ2keA6cqBagxnBGZ0NSbr5Wn5cVVg6GVV4FZXeHYOaWiJYTYshcSiRaFQ9Yp+lrhIyIl4NJb6Hme25n0jZkpNFUJDh9L/A5"
"NtVTjs4ky1X3zy2Ybyxti6WtLjAo0f0PhIMcGvR6kTwxV/XplKHAeKJivlzRFppGXEFPb/ZnaMhbPY0Q3kRcJJR4PPgUWXyJR2Ol61ZATuqlmofHhWT0QDhU"
"yX6g0bWGIAK1GSEIIAA5KICC0gYhXzFz9aKYEYHQ3XIpww0v7/OPje2ATsxvKUk0tYUb0o42VrEnkz+gggh+9HlWtr1ej48bXFbWz3QOR7bVZixpFUj1DEBS"
"xcqz+SfIARTSYd8pG27oyHPJ2fRPJytLk6f2s/KJyb3Q0QBZNCKFDpfZIzZjWNmBb/ToW3p4CSBgkqk6GThIYaDwR1sBo7bSQpQvO9OdaMf/00r5s4XqQlow"
"4zDa5WIGZdxZUlEYizyJ6La7NV0dMHJ1rFmlPHgc3rZdRUlOvSok6tZpqmMmC4GvxlseH5dqFyxaGKP5Ym/Ar5HqkkdO9CC39cY06ixlOYR5IUcycnWsOdHd"
"qaRSAYIAwzXnXzpCdrOgCZSANigyfpnocDKAoN39w1Ik4TynKmgyLgYR0uaMwdLQj4sC5+dFTtoPQKqR9l6rkix72chMkOgsjrshq77A2HJY2O132u6p+n2q"
"B6NJoQw0N4p2BncuZfBYMlvICRbAiCWdinuG1w6zofM3whqJ4kqvKzlMzh0l3fxOp31DUludU7pjWBY8eKKyzputqB8GYqqtjPWcxUl0neEG4WJzOJElBZfI"
"MEldXUd1QD0PHRZThyNUl/HFsMyZcQ9jMU/jSODktTCGUL5/j6B4XihbROrOFb8yoxVV5EFWRzq5AxSCw88fO+Mr9wld8y3XUMZQr5LMO7ntzy0cBlbvyYEC"
"fA+u8fGBeDjrrSXdJQnk3S60fsZK1F69G+7kUGfbHr1UB8uxGRvEePM8PBxz8ZYeH0IjHD01vBSKhZonCQXTeNVS47gqpeKqZ8sHk+RnE+5flK9+JYv0nu1V"
"uf1EtrNZ1bCjLOEZIJMjQajqtWRfmInuXh4MhsNBP4SAhkynNjA8ZmrKNB6PUabzspPQxgval0M+7+dHs10/xSo2RKLBL0/MErlxKRIMzSXvEO9fNAOKowJu"
"hndADLfQJtWCuOVueEMx1Y5VByCYwpo8Mf5rRzaVDVAPdY+QDdha2eljrEXbw6FwPx7MQXdsjFgZ0qoLTthwGysfp1PBFuJMeJCktuNxr78MZ0S9DgY04+1S"
"I5jRCuqbAtKrDN5C8C6l+SxwdCI1qZmRK9qPiJIurYpBcRO8TSfa7XdMOV4TluXUDrDzlMvVA0ZnPUSagc9cMqG+L+KtIurtKnpB++VmKfjHiFNvPksnMD53"
"2M1zobytAEoZYX4YF+AScbDXQ5GrsJmSubYVvQFaIJ2qJ7IBmIsIAr/RG20GQeDocizM270zd/Dtl31PdJqROGe87dG2L16GneJuiuHFAESB51kCKZtJ7soR"
"7+6rXE2xs6TnTk1k8YUeIgqFEGu+1gnYWOM8DVxDKC8ajp92lF2LmdM+h7OEILLz3khpMm9LWbKSCCKZTBky/lxiMmdBWMrOUHat5pDN4Tg2UqfcgGABIGqn"
"iM2+LHEONn/mvIo3cjV3EMJwAjy77ZY8W7N744LhryqD0EAMzOo/rzJWqMYRP9wYNVHbK9mhgta4ZaNuVoaAF4xWnHFRq8IV8R12nUsn4CHcW2ElkhIAjtGu"
"boNi67odn7I6VsMb9Go4D7d6Jpde+htstMuMKiCKQznD79XFuGoXqYLC2NcBflyp3xbovWcuDMzFN2jfkgBNcr16oB8kA1HJJdKI4g5xN5G/JgbcKKByDlWl"
"lBGc06oletUkeOhqAYrPeDSlHq6hYGU75H2iuMYgzYqm24wSakXs4BFJECVxToq5PQiuSDn09xW1Qm306wGGZQXAYWkRbnbFueBtD7e8SIgXBbNTRa3BOkUm"
"sGYoZwH69l1Lg0nObEnOF0WsBvQqNCTyt0st4ujv+wrtv0vKbQjAHtuTdj4qQJRNFmcYsLq9yjpFuO+mL7OiCiDsFz6j+r1VlkWQVw0+FtuYvgdB+VJZ3sOn"
"XY2RShurvehKzZBSbnz/CVcbHWTksypJeiQYjOj0yml/Bpot1N9t+mnhHvufKQJ9EDbvsJ+eBLa4Ou7F/8swFyaVk1YU9qdDt2TzuNCM1c4aScysFy+th0Ot"
"uRaRDdoxq1rGBmwaBgbAq3+uPXINyS3AZKy2ZCAqyY6jozHqUpbFESAGRYU1zWP62ayOfOx2pwqMfFVbWUmAA1cGqqFJcrket35GwwWRYbUzFbG57Bm63Rxt"
"J6kNsFz54wyZ2QIkl1muqEZTPthKSvNgRvKJSq4e3+aO00FeAadobDKmM5YGma61J5CrVHhRdu9hS1HU827RHqAK9c6wf9r5145DOSBVR0EweBhqrzJQmpux"
"+cwrjfkESbgf89imj/+QtKwurnE3Biil8E5PxKaUato68GmRtcKZw2lVPBnFk84W9jbCyLW62RnysgSAP0TtSfQlWjKmo1HTYwvNo2RbAdP/+uSiNIXGE+L7"
"dx3n/sOJv1z9LWUgjANSJU35TanRMGxkzb0BNKd3SPe4esjzpHzlTkJgyiouykHSuH4Gc1Z1NIYxrXEdMpnMplENB8jyM192/IrtqFq9p49EQSgWKWYQXLAx"
"5Y11Z3aZ15PikuMykSMUXxOYHeTb2B1/8u7jy+KwISSnesTk6xgOZ9eWlMToMneC03/L4o3Pl78GdhzR0h89CVEk/GxM228IKAUlHgU6Wvc2gaK9C4MtRD5Y"
"F/cX51fq74wWvtTozKsA66p9VlcFwjPQ/jpUR0sz1gS94zJAmt7VeTMsA7RBwm8Ql/UXiKoHn08gzIbGPk5YnEjqilDq4xUpIO5qqdnyV6peoXqVErYtWA6Z"
"a3WL53JHwwt9Z7gE+QKql/30W7mWTFZIpakCFWIcCuJ6xXWaBr2oS5iv66qVV7M8Cv8CrV74ojU1GnZbtdbKk55MTDo4941EHIyBmc9rmuELwDKdNgdASXOD"
"3tnFhxAsgACxRTpjstQn6uyJxZjj29M26mSqkMfAXRzdkmiITZ23dWUPZueV3sB7NYWY+/VnYXNsJKHXRQHlAwaU4lt9fqSkyG88ll1zETp8tCl+TSdwAMcB"
"I4wF2pi4io8yFqaSbAqty9CUGPGrfhmZJpY0hstZcm1VtQsY8oM3afFv/RXRV2/xUJ67ucPB4leKJBA0VDfOKON+MCh0n9ZQ8ATmEPJdprz2loi/rz2czZsg"
"kuooh9IuIWSKKBfQbnv+MkaHnpowwJetpsnqbOQSJ6oVyTAAWwBEnpe177rmoMowX8WaoauKotbDyLspj/NEpNBSGPzxtNvTD6WueHafk61qxnGayxUJLa0i"
"9nNHyhQMGCCKlpLy9lRVjvLnKfTnYKw1pbbvo/r/pws1clot30F1U9KkssnZddd33R/RU3JBsFcrB8WUi6bpxaJT4+f5r0HVQFbDA++qluRU2JNw21D2oiqN"
"I1wDoLfOL55jUCI79moqiTkORWvshnvB38J9geX0Dll5/ukiP3/aRy98bKDBn06pz+sWJK1AvcofNtUdxX5fiKcs7OP/P++3ArWVdZdcz1bxe7KkWlqQs7vO"
"kvr7uQjvtaDa/v0yMrewavvBIFqNQ4KBZkbWY1RAe+6C3U+Uqj2F8+uriv3RPU++G7u1PeuUtuazR6mhjNjw054c9v382mML/6mizgIZEPmoDv5rAAToS6ZJ"
"fWfSqW6tZkZacLDn2UBibyHXLgOxkoFVnMUoP13NFmnw7pBiWA6ZIKAypII559sF/Jt1n+/jG62SEOIOdjIi3A6fUyMQmroMKCarMOqUh6pbHwaOHfXESNVv"
"pNqFETIMrZn9ThjyN8fzSOMP21mW4ZbZbDyKLPuAmnPWoiQMuLGbhrJKBVwWejV+10Xvd5gFvegI1AeM5jQpHgbJF4mao91AVIChwS7Ar3gnYnw2OfMOscpu"
"eqZHJfs3jjHO6qKe+SJvd+BvtJVNgBMEpG8FcbyaFj6dqxLmSpencekSuexku5HgQAZ4J1vA8Sw57IVp9MJBNNwIO2jEdDt1OXOWJ9Mf5E+zT8qa++opGAWS"
"oWaBMkkeHvqjWqK9SAKkx2oaCLjSizt3wshWNrD+zU60x+1rsoubxSh5qjy9EIVLkB7YFPO67FiznusHZ2Bn6/thcgW2WyfKI4+uaoNE/YHrK91oVFZ+kCaK"
"pVDXdRNqP8u3oQsg8O++uGbAfzUKyCHqIf9HXdH4JaXCaBqryZkFbCQ9QksrMK0G08lAubLBHJd2gpRLyJs8Aypa0UEicGp2WoLurnCN1dB00KB5mnKL66IN"
"ahfcZEJtCkJ+QZfKsr1s4c5o2KlnIzD8dq9zxVPReEbAzWGcwojRgick42JVV3cIPaLreQmdJipNROjBAoqs1v8TKg2xtDgxK3JpRLeqlvkrZjKaIGB9Nkxw"
"StyLOuq26Y9DyqIft0kZcDqMFvnkSz37TIKLKHSID2JEihmEEyTW7JturhZE+P53lZBm0JZuG1xpQ/o3136LRBW4iEtGXI+vbvf9meSY842si7dArJlu1bqG"
"gqFQMs9OTsZ6ltPyMKLTM+xFuY1/o6RmvofgljEglXHwjTeinAw/j15TUZm7hHHNQNJ7K9Pxr+EJGM5SHYYNTVMkMal2IEI/ypIc4Fj2s54CEsPJkUy7ttaX"
"tiytRsIs/l3o4nQQSIPhWGqAy5oMLqCCKzsvKA331IQsBdwfCa5b8SVavKUVdnOlj8KgEYeRllMqp7MD3V5UcccWtbLAqemHz8fCQ3KodjTAFtM8V4WjaokA"
"nNU/WWGq0cRicQ2mi9aWBpGLiolZ6NPpAe1WkAEy0kiVaOsgrWyn27JNqz2aQLCutS0IrXJ40BhvuKO9qiIaEN42pM5cNzySJXpC4VZbwQ6eFKskvQoATyfe"
"vJqrVL8MUOjpRE+Vc0L39p+xx2sKllBvnTGECQfBgfx3HRMskfGuW3KPER0/zBERicA5gztvBoxhYfTntUQL7t/Fim93PTKPgyGDVdvOlPK8FCJxOfa6T3t6"
"ud0drPUo45/yUEvHPFxyc58gOQw6k+m9fdDXO7D/2XpN0GplWaId0ww5s5fVcuakJkjsTC87s3uECD5dekS5c1wJlo2rVJ4sYlqeY3e7TLuzM7Piu+LJom5K"
"n3shHiZpHyU5CzYvh/hZEY1pHkiad8uwf3Hh0ktmnxmWMXd8XKpFsWRagr+kf8N1/dj3HLaBhmE35h3KRIre6/uHMK+RbE1XWx1kJLWSC5TTH6vjuLB3CoX9"
"yJYtZAmy0sJkOKvHQ3nGqe4vNcby/AyjvSxaEnASMN4GQfh1ra292ktSGCe6FGIzSkMAMMy4weWniOc7MWr43qBpTGqaaeJTyK7zmGau0wTgs0j1DgZYunaH"
"8xynGTKgePK3yea+LtlRXzikWYPyiFbUYFS3DBuuJeaMlg75KlQKwYU64PRAYhSkVWCMYjGyPR35E9lI7kKK+JEkWBqsBw4zxiM1IxNZCbYDWedZTT02QNo9"
"HLS0U7YKzz9AKm/pcn7E6VZMSySob0AQ8X+HBp1M4Irxc9Jl0vlscHAD2EaEPqgmqIChLRhTYyroTTc7iHoH/ivL0Z+nRsW+Lkzy4PMEvxcPQdQbMQEpq67E"
"j0ri+weyFOcxNNALFrs2eqgs9vRdNSQFBy1N/ca2UUAeoOretrLdlS0V5VX3MkYJ8dWvr3NzQljaUkQj+hSUgMcK4GwFQB4vQEZef6tP9Xrp4SXYWW1GZzkZ"
"8RzcbqyRrWqoN5p0PNb5dcJ9YMEt/Z3Z2SSRBCYSlW6tdtnhJVWoBzkPwYKFBe71O2tBx7ONMpgn0f8vWFoCTz7sp41GmSVpHbhRrDD1VrXuyCT0PBt1dX5r"
"ER3Ge0G8tDQ483jfdGnfff/Xb9jxuYGIpbwuhcK0T8Y0Zg17hLeWgzS6nBF53XukhXP5WKLsuGerBrRzt+e6kG/30rBd5Zlkpo4B7fP4nlGZLiMWFv0d/89m"
"n50R6QSsjXsDaxpUXn0pUFlC1UmMaPlVuzvguFskDdUoYXReC8cVUO32lTecDVhsAYOBrgEBkT0PkgAKDCmUCSRfF4z96P88rtKeMjw5bYypWQhi0ZofZEqg"
"tHU/NS1De37l11YKYvmFRLBucxB246Ccdhg2q2azf6ePZ4sQCnbAZgZ9lRVMyV1EE/eGIQDT2RuLs7OXrs2bZ4ibdwcxAwkj4/EaTEXInWeTSkttIiDVgNlL"
"A5U1ksCbwICCHkQQTxR+H2KVLjDMLUwjwRe6vvjRb2Tl3w9lnnzBUepfEnBhbkEleL5+zcp/TV6/NPnaGvhn/d9/bd/1i/GfF5b+ngG/ANKNqxQoKGBUjoRs"
"jubjG+u8uvfnt72q8bMdG994FSADC2MEawwQ4LuS7S+Hr6ZwEWIFLcGqrVb44/CDdPUPCIyA7VOcYBB6wDFuG3n4+RwKPGV9/QMZ1b1H16/+SOx80kPD3gUw"
"PqwZRh9ZCrOUp6mETZpvme4XFZhRs6asWeNcPsoEgYhyxzZCMAaMx5+YIuvJD3fd+/M5cLju/dEZbFx8EX/7P8QZC19UDBkIC2lcgJARjB8o3vbBIdT3ipL7"
"vrdXg/swo/2oJ3rGAh/bT2kKMC4cB4l64RM6ORrzNsjkc7GE8Dkj1Zw93zTcL0muOuH/aOfzh+vZi8WvBUv4vUJpPyks7p5MXF0UuTZxc3Jj7+3Xw0Y/cRhJ"
"IZUB596VwxqUt2ryeMIDyiecyBK/BsuVEhGeOR+WkJv08RZjTvo3RHTciIkMiABm4gsLTj9c+LZA/cBKfgDYXy8zGsvX8P3N7+z6//pDU3Zl/hEvNABSEAAQ"
"4P/QJamd/+ZG6R9+LlbMHyPauRUUbirWWQq1k9HmH/YniZ71Y0b08BaaSbmdilDm86xdR1nkC24vnIIuaxTu1p60vpjtwan6Rm1Gkzz3cF8C7F8HjiaVniT0"
"yQn76FflQMqtw4xCPiLUaSn6vk9ufTgusgsoy0Xy12YHOVqG8WpmXrwk0XTnMknxQ5vX8I4zilVRkblUcJD91LdOKc4Xc0IOMV6Lg6CkEJq7CMINftxCl0Ci"
"wkABQPxNcR5TmmaKeg99f4Q39nyF3Lgm1vR6Nh3oqmmZyXhflJ3L1dM7wwivLgx9Ag3tVNtQ99yxQkVOR7GuNmCdReSlgTKNThY5a3N1vyQhSyyJkBEPuN90"
"GYwcGnpTiJFW0E2E5Pe85ngczipnAn49MdFfmh0ZwtizYHMa0KhIJu+fe4RW4AX+H3MBsxN15sELBJueSVqULEsphjlHRXVSWW3XEz5YzeK6mBB12Q1WtWdl"
"BZuDR2wMTzfzEfRLorPDwghJm5Y2wM4PlGVAOr86T9gmoIfZ5qsF/Zno8yyZM877mlGWPibNqLocQBs8vRkKL73kzRvE9AArqhhxm1kdW/JqA8W4dG6/JDie"
"RMee8M2ZTyWijXtNyzH0EYDJ2vPy9UPEsZ0A6TryehrWyHNSiN07SIDBVvpykJ1mFrUgXFZCdwldUwdu7vjcbUrh14f32znspnIbR3/QEk2vuQyYu+cRBbv7"
"0jX28Knwg39fs99fk1+U42V4u4lPKtchUmV2UMLeBXoAtANyfxfW7ARKEqfKosbnO/KZY2xOkgSL5kNyhgdWi31R0qmxV5qjjJc03doxlOpETSiJGGWWle1T"
"HqhPLo2yi4ZL1w5RL6mH9WeK/SAsNfMzNJHvOLnEzsyfzmlGkTfNJyZ1paMuB48G+M4cr3FBnfpOJmncuv7+uG/e89N+K+typa2Z9nx5KhMd3jRYT+hZXUDW"
"FoeYmrubXYOtoGAMSiS+TekNOyFkNp4XfgG+3gL8Any7hNTrol4hmbx2v+U5gd2sUdcdvbZ88RVncSdvceVpXNBBFrP2Xu2NzR573oayQmMfip4TXYHmMrdc"
"wuA/oyWEAv6JgoOEfa7g28Kvu8JGPx+UY6Xs0beixUruKol6K1LlG/lxXQDcP5Jtdy7hWSVEbwApxk6okcf95dFiPov+d9DvxF70v3HfcSpI7y3n4dQDRCIq"
"jNW47HyG7YEBt/mD69xEgpnxqWvTfknSAjAl9hRs2lMXZxEVvjkEaBe7iBXaV3lBJrdnVpAdMGCwAOAgsHVARA51IPhc7sAIcrcDx66YHQRW5SrIKhh5OwhA"
"mSqFDgi4Kq8g+FKQlFBFBwMIq/bFZwKm9hFBSaVGOYlSYlp+vKl48btFygOE+TQ8um2+nALPRJmJxOQoSXeST3DCVIQjpqyERqULk6OlSg6E0t0Sku7CCiVi"
"FFBqPtZkPIS2dmSESQhMlFe1p8utFq5BUPszpSrIi0h01ZOqw0cw+WrBtxrRpVS3fz8pNOVKKDtemeVyAzpCdP1ofQbJIY0CQtdFDkKQ8hUU8g5QTMEpyQu+"
"ZHYxp/77VEsBAA=="
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
    /* [EN] Every valid frame feeds the STM32 dead-man, so it also counts as the keepalive.
       [FA] هر فریم معتبر ددمن STM32 را تغذیه می‌کند، پس keepalive هم حساب می‌شود. */
    UINT32_T__G__LastKeepaliveMs = UINT32_T__G__LastTxMs;
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
 * @brief  [EN] Send CAL_REFERENCE [target:u8][ref_mA:u32 LE] (type 0x03, spec 5.4) once and
 *              start the reply timer. Never repeated: a lost or rejected frame shows as "rejected".
 *         [FA] ارسال یک‌بارهٔ CAL_REFERENCE با قالب [target:u8][ref_mA:u32 LE] (نوع 0x03، بخش 5.4)
 *              و شروع زمان‌سنج پاسخ. تکرار نمی‌شود: فریم گم‌شده یا ردشده «رد شد» نمایش داده می‌شود.
 * @return [EN] None / [FA] ندارد
 */
static void func__Esp_SendCalReference(void)
{
    uint8_t UINT8_T__A__Payload[ESP_CAL_PAYLOAD_SIZE];
    UINT8_T__A__Payload[0] = UINT8_T__G__CalTarget;
    UINT8_T__A__Payload[1] = (uint8_t)(UINT32_T__G__CalRefMa & 0xFFu);
    UINT8_T__A__Payload[2] = (uint8_t)((UINT32_T__G__CalRefMa >> 8) & 0xFFu);
    UINT8_T__A__Payload[3] = (uint8_t)((UINT32_T__G__CalRefMa >> 16) & 0xFFu);
    UINT8_T__A__Payload[4] = (uint8_t)((UINT32_T__G__CalRefMa >> 24) & 0xFFu);
    func__Esp_WriteFrame(ESP_MSG_CAL_REFERENCE, UINT8_T__A__Payload, ESP_CAL_PAYLOAD_SIZE);
    UINT32_T__G__CalSentMs = UINT32_T__G__LastTxMs;
}

/**
 * @brief  [EN] Parameter ID the STM32 reports first for a CAL target (2/3 gain, 9/10 ETA).
 *         [FA] شناسهٔ پارامتری که STM32 اول برای هر هدف CAL گزارش می‌کند (۲/۳ گین، ۹/۱۰ η).
 * @param  uint8_t__target [EN] CAL target, 0..3 / [FA] هدف CAL، ۰ تا ۳
 * @return [EN] Parameter ID / [FA] شناسهٔ پارامتر
 */
static uint8_t func__Esp_CalReplyId(uint8_t uint8_t__target)
{
    uint8_t uint8_t__channel = (uint8_t)(uint8_t__target & 1u);
    uint8_t uint8_t__base = (uint8_t__target < 2u) ? ESP_PARAM_CUR1_GAIN : ESP_PARAM_CHG_ETA1;
    return (uint8_t)(uint8_t__base + uint8_t__channel);
}

/**
 * @brief  [EN] Send at most one queued command per ESP_LINK_TX_INTERVAL_MS
 *              (priority order UINT8_T__G__TxOrder), plus the manual-mode keepalive or, with no
 *              browser, the manual-exit request (ID 19 = 0). Never blocks.
 *         [FA] ارسال حداکثر یک فرمان صف‌شده در هر ESP_LINK_TX_INTERVAL_MS
 *              (به ترتیب اولویت UINT8_T__G__TxOrder) و keepalive مود دستی یا، بدون مرورگر، درخواست
 *              خروج از مود دستی (ID 19 = 0). هیچ‌وقت مسدود نمی‌کند.
 * @return [EN] None / [FA] ندارد
 */
static void func__Esp_PumpTx(void)
{
    uint32_t uint32_t__nowMs = (uint32_t)millis();
    uint32_t uint32_t__elapsedMs = uint32_t__nowMs - UINT32_T__G__LastTxMs;
    uint8_t uint8_t__step;

    /* [EN] CAL_REFERENCE rejection = silence: no PARAM_REPORT within ESP_CAL_REPLY_TIMEOUT_MS.
       [FA] رد CAL_REFERENCE یعنی سکوت: در ESP_CAL_REPLY_TIMEOUT_MS هیچ PARAM_REPORT نیامد. */
    bool bool__calSent = (ESP_CAL_STATE_T__G__CalState == ESP_CAL_WAITING) && (!BOOL__G__CalTxPending);
    if (bool__calSent && ((uint32_t__nowMs - UINT32_T__G__CalSentMs) >= ESP_CAL_REPLY_TIMEOUT_MS))
    {
        ESP_CAL_STATE_T__G__CalState = ESP_CAL_REJECTED;
    }

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

    /* [EN] CAL goes after queued SETs, so a gain the user typed just before is applied first.
       [FA] CAL بعد از SETهای صف‌شده می‌رود تا گینی که کاربر همین الان زده اول اعمال شود. */
    if (BOOL__G__CalTxPending)
    {
        BOOL__G__CalTxPending = false;
        func__Esp_SendCalReference();
        return;
    }

    /* [EN] Manual-mode keepalive: every ESP_LINK_KEEPALIVE_MS while manual is active (b5 or param 19),
            whether or not a browser is polling (spec 5.2: from every tab and in the background).
       [FA] keepalive مود دستی: هر ESP_LINK_KEEPALIVE_MS تا وقتی مود دستی فعال است (b5 یا پارامتر ۱۹)،
            چه مرورگری poll کند چه نه (بخش 5.2 سند: از هر تب و در پس‌زمینه). */
    bool bool__flagManual = ((UINT8_T__G__TlmFlags & ESP_TLM_FLAG_MANUAL_MODE) != 0u);
    bool bool__paramManual = BOOL__G__ParamKnown[ESP_PARAM_MANUAL_TEST_MODE] &&
                             (UINT32_T__G__ParamApplied[ESP_PARAM_MANUAL_TEST_MODE] != 0u);
    bool bool__manualActive = bool__flagManual || bool__paramManual;

    /* [EN] Closed-panel guard: no /t poll for ESP_LINK_BROWSER_LOST_MS -> the keepalive is replaced by
            SET ID 19 = 0, repeated every ESP_LINK_KEEPALIVE_MS until the STM32 reports manual off
            (a lost frame is simply retried). A throttled background tab still polls about once per
            second and is not affected.
       [FA] محافظ پنل بسته: اگر ESP_LINK_BROWSER_LOST_MS هیچ /t خوانده نشود، به‌جای keepalive فرمان
            ID 19 = 0 هر ESP_LINK_KEEPALIVE_MS تکرار می‌شود تا STM32 خاموشی مود دستی را گزارش کند
            (فریم گم‌شده دوباره فرستاده می‌شود). تب پس‌زمینه با throttle هنوز حدود هر ثانیه می‌خواند. */
    /* [EN] No browser since boot counts as lost: an ESP reset while the STM32 is in manual mode
            must not keep the mode alive for 10 s without anyone watching.
       [FA] نبودن مرورگر از بوت هم «ازدست‌رفته» حساب می‌شود: ری‌استارت ESP وسط مود دستی نباید
            مود را ۱۰ ثانیه بدون ناظر زنده نگه دارد. */
    uint32_t uint32_t__browserAgeMs = uint32_t__nowMs - UINT32_T__G__LastBrowserPollMs;
    bool bool__browserLost = (!BOOL__G__BrowserSeen) || (uint32_t__browserAgeMs >= ESP_LINK_BROWSER_LOST_MS);
    uint32_t uint32_t__keepaliveAgeMs = uint32_t__nowMs - UINT32_T__G__LastKeepaliveMs;
    bool bool__keepaliveDue = bool__manualActive && (uint32_t__keepaliveAgeMs >= ESP_LINK_KEEPALIVE_MS);

    if (bool__keepaliveDue && bool__browserLost)
    {
        func__Esp_SendSetParam(ESP_PARAM_MANUAL_TEST_MODE, 0u);
        return;
    }

    /* [EN] Periodic parameter refresh: the seq-restart detector is blind while the
            sequence counter sits in its wrap window (0xFF00..0xFFFF), so a board reboot
            inside that ~26 s window (once per ~109 min) would leave the applied-value
            table stale forever. One GET_PARAMS every ESP_LINK_PARAM_REFRESH_MS keeps the
            display truthful; it does NOT re-apply user parameters (that only happens on
            a detected restart, so a JIT-parked channel can never be re-armed by this).
       [FA] نوسازی دوره‌ای پارامترها: تشخیص‌گر ری‌استارت در پنجرهٔ wrap شمارندهٔ seq
            (0xFF00..0xFFFF) کور است؛ ریبوت برد در همین پنجرهٔ ~۲۶ ثانیه‌ای (یک‌بار در
            هر ~۱۰۹ دقیقه) جدول مقادیر اعمال‌شده را برای همیشه کهنه می‌گذارد. یک
            GET_PARAMS هر ESP_LINK_PARAM_REFRESH_MS نمایش را راست‌نگه می‌دارد؛
            پارامترهای کاربر را دوباره اعمال نمی‌کند (فقط بعد از ری‌استارتِ
            تشخیص‌شده؛ پس هیچ‌وقت کانال پارک‌شدهٔ JIT را مسلح نمی‌کند). */
    if ((uint32_t__nowMs - UINT32_T__G__LastParamRefreshMs) >= ESP_LINK_PARAM_REFRESH_MS)
    {
        UINT32_T__G__LastParamRefreshMs = uint32_t__nowMs;
        BOOL__G__TxGetPending = true;
    }

    if (bool__keepaliveDue)
    {
        BOOL__G__TxGetPending = true;
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
 * @brief  [EN] Match a PARAM_REPORT against the CAL run in flight. The expected ID marks the run
 *              applied and joins the session cache (re-sent after an STM32 reset, spec 5.4); a
 *              gain calibration also caches ETA = 0, because the firmware resets that channel's ETA.
 *         [FA] تطبیق PARAM_REPORT با CAL در جریان. شناسهٔ مورد انتظار اجرا را «اعمال شد» می‌کند و
 *              در کش نشست می‌رود (بعد از ری‌استارت STM32 دوباره فرستاده می‌شود، بخش 5.4)؛ کالیبراسیون
 *              گین η صفر را هم کش می‌کند، چون فریم‌ور η همان کانال را صفر می‌کند.
 * @param  uint8_t__id [EN] Reported parameter ID / [FA] شناسهٔ پارامتر گزارش‌شده
 * @return [EN] None / [FA] ندارد
 */
static void func__Esp_CalOnReport(uint8_t uint8_t__id)
{
    bool bool__calSent = (ESP_CAL_STATE_T__G__CalState == ESP_CAL_WAITING) && (!BOOL__G__CalTxPending);
    if ((!bool__calSent) || (uint8_t__id != func__Esp_CalReplyId(UINT8_T__G__CalTarget)))
    {
        return;
    }

    UINT32_T__G__CalValue = UINT32_T__G__ParamApplied[uint8_t__id];
    UINT32_T__G__TxParamValue[uint8_t__id] = UINT32_T__G__CalValue;
    BOOL__G__ParamUserSet[uint8_t__id] = true;

    if (UINT8_T__G__CalTarget < 2u)
    {
        uint8_t uint8_t__etaId = func__Esp_CalReplyId((uint8_t)(UINT8_T__G__CalTarget + 2u));
        UINT32_T__G__TxParamValue[uint8_t__etaId] = 0u;
        BOOL__G__ParamUserSet[uint8_t__etaId] = true;
    }

    ESP_CAL_STATE_T__G__CalState = ESP_CAL_APPLIED;
}

/**
 * @brief  [EN] Add the just-stored TLM frame to the bench statistics window.
 *         [FA] افزودن فریم TLM تازه ذخیره‌شده به پنجرهٔ آمار بنچ.
 * @return [EN] None / [FA] ندارد
 */
static void func__Esp_StatAccumulate(void)
{
    uint8_t uint8_t__index;

    if (UINT32_T__G__StatCount >= ESP_STAT_MAX_FRAMES)
    {
        return;
    }

    for (uint8_t__index = 0u; uint8_t__index < ESP_STAT_FIELD_COUNT; uint8_t__index++)
    {
        uint32_t uint32_t__value = UINT32_T__G__TlmField[UINT8_T__G__StatField[uint8_t__index]];
        bool bool__first = (UINT32_T__G__StatCount == 0u);
        UINT32_T__G__StatSum[uint8_t__index] += uint32_t__value;
        if (bool__first || (uint32_t__value < UINT32_T__G__StatMin[uint8_t__index]))
        {
            UINT32_T__G__StatMin[uint8_t__index] = uint32_t__value;
        }
        if (bool__first || (uint32_t__value > UINT32_T__G__StatMax[uint8_t__index]))
        {
            UINT32_T__G__StatMax[uint8_t__index] = uint32_t__value;
        }
    }

    UINT32_T__G__StatCount++;
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
        func__Esp_StatAccumulate();
    }
    else if ((UINT8_T__G__RxType == ESP_MSG_PARAM_REPORT) && (UINT8_T__G__RxLen == ESP_LINK_PARAM_ITEM_SIZE))
    {
        func__Esp_StoreParamItem(uint8_t__ptr_payload);
        func__Esp_CalOnReport(uint8_t__ptr_payload[0]);
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
 * @brief  [EN] Strict decimal parse of an HTTP argument: optional '-', 1..7 digits, nothing else.
 *              (String::toInt() returns 0 for garbage, which would silently target ID 0 / value 0.)
 *         [FA] تبدیل سخت‌گیرانهٔ آرگومان HTTP به عدد: '-' اختیاری و ۱ تا ۷ رقم، بدون هیچ چیز دیگر.
 *              (toInt() برای ورودی خراب صفر می‌دهد و بی‌صدا شناسه/مقدار صفر را هدف می‌گیرد.)
 * @param  char__ptr_text      [EN] NUL-terminated text / [FA] متن پایان‌یافته با NUL
 * @param  int32_t__ptr_value  [EN] Output, -9999999..9999999 / [FA] خروجی، -۹۹۹۹۹۹۹ تا ۹۹۹۹۹۹۹
 * @return [EN] true when the text is a valid integer / [FA] true اگر متن عدد صحیح معتبر باشد
 */
static bool func__Esp_ParseInt(const char *char__ptr_text, int32_t *int32_t__ptr_value)
{
    bool bool__negative = (char__ptr_text[0] == '-');
    uint8_t uint8_t__index = bool__negative ? 1u : 0u;
    uint8_t uint8_t__digits = 0u;
    int32_t int32_t__value = 0;

    while (char__ptr_text[uint8_t__index] != '\0')
    {
        char char__digit = char__ptr_text[uint8_t__index];
        if ((char__digit < '0') || (char__digit > '9') || (uint8_t__digits >= 7u))
        {
            return false;
        }
        int32_t int32_t__digitValue = (int32_t)(char__digit - '0');
        int32_t int32_t__shifted = int32_t__value * 10;
        int32_t__value = int32_t__shifted + int32_t__digitValue;
        uint8_t__digits++;
        uint8_t__index++;
    }

    if (uint8_t__digits == 0u)
    {
        return false;
    }

    *int32_t__ptr_value = bool__negative ? -int32_t__value : int32_t__value;
    return true;
}

/**
 * @brief  [EN] GET / : serve the web panel from flash (never cached, so panel
 *              updates reach every browser immediately).
 *         [FA] مسیر GET / : ارسال پنل وب از حافظه فلش (بدون کش تا هر آپدیت پنل
 *              بلافاصله به همهٔ مرورگرها برسد).
 * @return [EN] None / [FA] ندارد
 */
static void func__Esp_HttpRoot(void)
{
    ESP_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
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
 * @brief  [EN] GET /t : compact JSON snapshot {on,age,seq,fl,n,q,ka,t[20],p[20],c[4]}.
 *              t = TLM u32 fields in spec order (offset 4..80); p = applied params or null;
 *              c = CAL_REFERENCE [run, state 0..3, target, applied value].
 *         [FA] مسیر GET /t : خلاصه JSON فشرده {on,age,seq,fl,n,q,ka,t[20],p[20],c[4]}.
 *              t فیلدهای u32 تله‌متری به ترتیب سند (آفست ۴ تا ۸۰)؛ p مقدار اعمال‌شده یا null؛
 *              c = CAL_REFERENCE [شمارهٔ اجرا، وضعیت ۰..۳، هدف، مقدار اعمال‌شده].
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

    size_t__used += (size_t)snprintf(&CHAR__G__JsonBuffer[size_t__used], ESP_JSON_BUFFER_SIZE - size_t__used,
        "],\"c\":[%lu,%u,%u,%lu]}", (unsigned long)UINT32_T__G__CalRun, (unsigned int)ESP_CAL_STATE_T__G__CalState,
        (unsigned int)UINT8_T__G__CalTarget, (unsigned long)UINT32_T__G__CalValue);
    (void)size_t__used;
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

    int32_t int32_t__id = -1;
    int32_t int32_t__value = 0;
    bool bool__idOk = func__Esp_ParseInt(ESP_WEB_SERVER_T__G__Server.arg("id").c_str(), &int32_t__id);
    bool bool__valueOk = func__Esp_ParseInt(ESP_WEB_SERVER_T__G__Server.arg("v").c_str(), &int32_t__value);

    if ((!bool__idOk) || (!bool__valueOk) || (int32_t__id < 0) || (int32_t__id >= (int32_t)ESP_PARAM_COUNT))
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

/**
 * @brief  [EN] POST /c?t=&r= : queue one CAL_REFERENCE (target 0..3, typed DMM reading 50..5000 mA).
 *              400 = bad arguments, 409 = a run is still waiting, 503 = no live link to the STM32.
 *         [FA] مسیر POST /c?t=&r= : صف کردن یک CAL_REFERENCE (هدف ۰..۳، عدد مولتی‌متر ۵۰..۵۰۰۰ mA).
 *              400 = آرگومان نادرست، 409 = اجرای قبلی هنوز منتظر است، 503 = لینک زنده با STM32 نیست.
 * @return [EN] None / [FA] ندارد
 */
static void func__Esp_HttpCalReference(void)
{
    int32_t int32_t__target = -1;
    int32_t int32_t__refMa = 0;
    bool bool__targetOk = func__Esp_ParseInt(ESP_WEB_SERVER_T__G__Server.arg("t").c_str(), &int32_t__target);
    bool bool__refOk = func__Esp_ParseInt(ESP_WEB_SERVER_T__G__Server.arg("r").c_str(), &int32_t__refMa);
    bool bool__targetRange = (int32_t__target >= 0) && (int32_t__target < (int32_t)ESP_CAL_TARGET_COUNT);
    bool bool__refRange = (int32_t__refMa >= ESP_CAL_MIN_REF_MA) && (int32_t__refMa <= ESP_CAL_MAX_REF_MA);

    if ((!bool__targetOk) || (!bool__refOk) || (!bool__targetRange) || (!bool__refRange))
    {
        ESP_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0}");
        return;
    }

    if (ESP_CAL_STATE_T__G__CalState == ESP_CAL_WAITING)
    {
        ESP_WEB_SERVER_T__G__Server.send(409, "application/json", "{\"ok\":0}");
        return;
    }

    uint32_t uint32_t__ageMs = (uint32_t)millis() - UINT32_T__G__LastTlmMs;
    if ((!BOOL__G__TlmSeen) || (uint32_t__ageMs > ESP_LINK_TIMEOUT_MS))
    {
        ESP_WEB_SERVER_T__G__Server.send(503, "application/json", "{\"ok\":0}");
        return;
    }

    UINT8_T__G__CalTarget = (uint8_t)int32_t__target;
    UINT32_T__G__CalRefMa = (uint32_t)int32_t__refMa;
    UINT32_T__G__CalValue = 0u;
    UINT32_T__G__CalRun++;
    ESP_CAL_STATE_T__G__CalState = ESP_CAL_WAITING;
    BOOL__G__CalTxPending = true;
    ESP_WEB_SERVER_T__G__Server.send(200, "application/json", "{\"ok\":1}");
}

/**
 * @brief  [EN] POST /m : restart the bench statistics window (spec 5.5).
 *         [FA] مسیر POST /m : شروع دوبارهٔ پنجرهٔ آمار بنچ (بخش 5.5).
 * @return [EN] None / [FA] ندارد
 */
static void func__Esp_HttpStatReset(void)
{
    uint8_t uint8_t__index;

    for (uint8_t__index = 0u; uint8_t__index < ESP_STAT_FIELD_COUNT; uint8_t__index++)
    {
        UINT32_T__G__StatSum[uint8_t__index] = 0u;
        UINT32_T__G__StatMin[uint8_t__index] = 0u;
        UINT32_T__G__StatMax[uint8_t__index] = 0u;
    }

    UINT32_T__G__StatCount = 0u;
    ESP_WEB_SERVER_T__G__Server.send(200, "application/json", "{\"ok\":1}");
}

/**
 * @brief  [EN] GET /m : bench window as {n, s[13] sums, lo[13], hi[13]} (browser divides s by n).
 *         [FA] مسیر GET /m : پنجرهٔ بنچ به شکل {n، s[13] مجموع، lo[13]، hi[13]} (مرورگر s را بر n تقسیم می‌کند).
 * @return [EN] None / [FA] ندارد
 */
static void func__Esp_HttpStatRead(void)
{
    const uint32_t *UINT32_T__A__Table[3] = { UINT32_T__G__StatSum, UINT32_T__G__StatMin, UINT32_T__G__StatMax };
    const char *CHAR__A__Key[3] = { "s", "lo", "hi" };
    uint8_t uint8_t__table;
    uint8_t uint8_t__index;
    size_t size_t__used;

    size_t__used = (size_t)snprintf(CHAR__G__JsonBuffer, ESP_JSON_BUFFER_SIZE, "{\"n\":%lu",
                                    (unsigned long)UINT32_T__G__StatCount);

    for (uint8_t__table = 0u; uint8_t__table < 3u; uint8_t__table++)
    {
        size_t__used += (size_t)snprintf(&CHAR__G__JsonBuffer[size_t__used], ESP_JSON_BUFFER_SIZE - size_t__used,
                                         ",\"%s\":[", CHAR__A__Key[uint8_t__table]);
        for (uint8_t__index = 0u; uint8_t__index < ESP_STAT_FIELD_COUNT; uint8_t__index++)
        {
            const char *char__ptr_sep = (uint8_t__index == 0u) ? "" : ",";
            size_t__used += (size_t)snprintf(&CHAR__G__JsonBuffer[size_t__used], ESP_JSON_BUFFER_SIZE - size_t__used,
                                             "%s%lu", char__ptr_sep,
                                             (unsigned long)UINT32_T__A__Table[uint8_t__table][uint8_t__index]);
        }
        size_t__used += (size_t)snprintf(&CHAR__G__JsonBuffer[size_t__used], ESP_JSON_BUFFER_SIZE - size_t__used, "]");
    }

    (void)snprintf(&CHAR__G__JsonBuffer[size_t__used], ESP_JSON_BUFFER_SIZE - size_t__used, "}");
    ESP_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    ESP_WEB_SERVER_T__G__Server.send(200, "application/json", CHAR__G__JsonBuffer);
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
    ESP_WEB_SERVER_T__G__Server.on("/c", HTTP_POST, func__Esp_HttpCalReference);
    ESP_WEB_SERVER_T__G__Server.on("/m", HTTP_POST, func__Esp_HttpStatReset);
    ESP_WEB_SERVER_T__G__Server.on("/m", HTTP_GET, func__Esp_HttpStatRead);
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
