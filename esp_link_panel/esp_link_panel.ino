/**
 * @file    esp_link_panel.ino
 * @brief   [EN] ESP-side ESP-Link bridge: exchanges binary frames with the STM32 over UART
 *               (921600 8N1, ESP_AGENT_SPEC.md) and serves a dark RTL web panel (Vazirmatn).
 *               v1.14 (user order 2026-09-25): the charge tab gains a live SVG STAGE GRAPH
 *               (threshold bands, BULK->ABSORB->FLOAT curve with the reentry cycle, current
 *               annotations, live battery markers and charger states, dashed preview of typed
 *               values) and the persistence texts - every applied parameter is stored on the
 *               STM32's own flash ~1.5 s after the change and survives power loss there
 *               (wire protocol unchanged; see ESP_AGENT_SPEC.md 5.8).
 *               v1.15 (user order 2026-09-26): ALARMS inside the settings
 *               tab (ids 27..34 fault supervision + 35..37 down-only
 *               charger safety ceilings), with grouped cards, live
 *               threshold bars and a TLM-derived grouped status card
 *               (v1.15b: build-once skeleton, per-bit fault explanations)
 *               plus JSON settings backup (import/export);
 *               PARAMS_BULK grows to 191 payload bytes (38 params), both
 *               boards must flash together.
 *               v1.16 (user order 2026-09-26): UI cadence (ids 38..76 -
 *               LED periods/duties, beep periods/durations/counts/gaps,
 *               beep bands, blink periods, thresholds, panel-session buzzer
 *               mute, RAM-only since v1.16b) with virtual board LEDs (real blinking), a buzzer
 *               icon with a mute cross, per-bit fault LEDs and editable
 *               fields; the wire length field grows to u16 (frame = AA 55
 *               type len_lo len_hi payload xor) and PARAMS_BULK to 416
 *               payload bytes (83 params, v1.17) - both boards must flash
 *               together (a v1.15 parser reads len_hi as payload).
 *               Tabs (v1.7 simplification; filter avg window 1..300 since v1.9):
 *               1) Panel: shared voltages (with DMM offset helpers), the current filter (median 1..15,
 *                  average 1..300) and one column per charger: live status, the measurement chain with
 *                  live formulas, the live filter chart and a charger cut button.
 *               2) Bench capture (spec 5.6): SOLO1/SOLO2/BOTH, a user-typed duty list, advance ONLY on
 *                  the user's submit. v1.7 latch fix (user order 2026-09-25): the statistics window is
 *                  reset when the DMM form OPENS and is read the moment the user PRESSES submit, so the
 *                  CSV row matches the typed meters instead of a stale earlier window; sample_ms in the
 *                  CSV is the actual window duration. One 128-column row per step in LittleFS
 *                  /benchlog.csv, GET /benchlog to download. v1.8 (user order 2026-09-25,
 *                  same day): the bench tab also carries a compact PERMANENT manual-duty card
 *                  (manual-mode toggle + per-channel duty + both-to-zero) so current tests can
 *                  run without the removed engineer tab; the section 5.2 safety contract (10 s
 *                  dead-man, channel ceilings, JIT re-arm) is unchanged. v1.9 (user order 2026-09-25,
 *                  same day): DMM currents accept NEGATIVE values (battery discharge path), the
 *                  moving-average ceiling is 300 samples (visible smoothing at the 10 Hz TLM), the
 *                  filter card shows the live effective span, and the wizard's default duty list is
 *                  denser (2% steps) for the next, denser LUT run. v1.10 (same day): the manual-duty
 *                  card moved into the PANEL tab; per-channel duty-ceiling inputs (0..500 permille);
 *                  chart sample count user-settable (default 100); the wizard settle step REMOVED
 *                  (capture is user-latched, the wait added nothing); the pack-24V divider corrected
 *                  (user factor 0.09827 = 6.8k/69.2k; scale 69200/6800) and voltage offsets now
 *                  +/-5000 mV; DMM form hints compacted.
 *               The v1.6 extras (CAL card, manual tests B/C/D, correction/analysis tab, engineer mode)
 *               were removed except the manual-duty card above; nothing was ever calibrated
 *               automatically and the wire protocol (SET_PARAM / GET_PARAMS / TLM) is unchanged.
 *          [FA] پل ESP-Link سمت ESP: تبادل فریم باینری با STM32 روی UART (921600 8N1، مطابق
 *               ESP_AGENT_SPEC.md نسخهٔ ۱.۴) و یک پنل وب دارک راست‌به‌چپ با فونت وزیرمتن و «دو» تب
 *               (کارت duty دستی از ۱.۱۰ در تب پنل؛ سقف duty، نمونه‌های نمودار، حذف صبر و مقسم پک هم در ۱.۱۰):
 *               ۱) پنل: ولتاژهای مشترک (با کالیبراسیون آفست از مولتی‌متر)، فیلتر جریان (median ۱..۱۵،
 *                  میانگین ۱..۳۰۰) و یک ستون برای هر شارژر: وضعیت زنده، زنجیرهٔ اندازه‌گیری با فرمول
 *                  زنده، نمودار فیلتر و دکمهٔ قطع شارژر.
 *               ۲) داده‌برداری بنچ (بخش 5.6): SOLO1/SOLO2/BOTH با فهرست duty دلخواه و جلو رفتن فقط
 *                  با دکمهٔ کاربر. اصلاح ۱.۷ (دستور کاربر ۲۰۲۶-۰۹-۲۵): پنجرهٔ آمار با باز شدن فرم
 *                  مولتی‌متر صفر می‌شود و «همان لحظهٔ زدن ثبت» خوانده می‌شود تا ردیف CSV با عددهای
 *                  واردشده هم‌لحظه باشد، نه یک پنجرهٔ کهنه؛ ستون sample_ms مدت واقعی همان پنجره است.
 *                  یک ردیف ۸۹ستونی برای هر مرحله در LittleFS به نام /benchlog.csv، دانلود با GET /benchlog.
 *               اضافات نسخهٔ ۱.۶ (کارت CAL، تست‌های دستی B/C/D، تب اصلاح/تحلیل، حالت مهندس) حذف شدند؛
 *               هیچ کالیبراسیونی خودکار اعمال نمی‌شود و پروتکل سیمی (SET/GET/TLM) دست‌نخورده و دوطرفه است.
 *
 * @note    [EN] Wiring: STM32 PA9 (TX) -> ESP RX, STM32 PA10 (RX) <- ESP TX, common GND.
 *               STM32 PA8 drives ESP CH_PD/EN; this sketch never touches that line.
 *               Wi-Fi AP "ChangeOver-ESP", password "123456789", panel at http://192.168.4.1
 *               No cloud: only MCU <-> ESP data exchange (flash is used only for the bench log file).
 *               Manual test mode (ID 19): a GET_PARAMS keepalive is sent every 1 s while manual
 *               mode is active (TLM flag b5 or param 19), also with a background tab (spec 5.2).
 *               Safety: if no browser has polled /t for 10 s (or none since the ESP booted), the ESP
 *               sends ID 19 = 0 every 1 s until the STM32 reports manual off, so a closed panel never
 *               leaves a channel on an unregulated duty; the STM32 3 s dead-man still covers an ESP
 *               hang or a broken link.
 *          [FA] سیم‌بندی: PA9 به RX ماژول، PA10 به TX ماژول، زمین مشترک.
 *               پایه CH_PD/EN را STM32 (PA8) کنترل می‌کند؛ این برنامه به آن دست نمی‌زند.
 *               وای‌فای "ChangeOver-ESP" با رمز "123456789"، پنل در http://192.168.4.1
 *               بدون اینترنت: فقط تبادل داده بین MCU و ESP (فلش فقط برای فایل ثبت بنچ استفاده می‌شود).
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

#include <LittleFS.h>
#include <stdint.h>
#include <stdbool.h>

/* Modules (single translation unit, included in this order): */
#include "plink_config.h"
#include "plink_params.h"
#include "plink_state.h"
#include "plink_panel.h"
#include "plink_font.h"
#include "plink_link.h"
#include "plink_http.h"
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

    /* [EN] Mount the file system for the bench log; format once if it has never been used.
       [FA] سوار کردن فایل‌سیستم برای فایل ثبت بنچ؛ اگر هرگز استفاده نشده یک‌بار فرمت می‌شود. */
#if defined(ESP8266)
    BOOL__G__FsOk = LittleFS.begin();
#else
    BOOL__G__FsOk = LittleFS.begin(true);
#endif

    ESP_WEB_SERVER_T__G__Server.on("/", HTTP_GET, func__Esp_HttpRoot);
    ESP_WEB_SERVER_T__G__Server.on("/f.css", HTTP_GET, func__Esp_HttpFont);
    ESP_WEB_SERVER_T__G__Server.on("/t", HTTP_GET, func__Esp_HttpTelemetry);
    ESP_WEB_SERVER_T__G__Server.on("/s", HTTP_POST, func__Esp_HttpSetParam);
    ESP_WEB_SERVER_T__G__Server.on("/m", HTTP_POST, func__Esp_HttpStatReset);
    ESP_WEB_SERVER_T__G__Server.on("/m", HTTP_GET, func__Esp_HttpStatRead);
    /* [EN] v1.66: the direct LUT push lives on its own routes, exactly as it
       lives in its own flash block on the board.
       [FA] v1.66: ارسال مستقیم جدول مسیرهای خودش را دارد، همان‌طور که روی برد
       بلوک فلش خودش را دارد. */
    ESP_WEB_SERVER_T__G__Server.on("/lut", HTTP_POST, func__Esp_HttpLutPush);
    ESP_WEB_SERVER_T__G__Server.on("/lut", HTTP_GET, func__Esp_HttpLutStatus);
    ESP_WEB_SERVER_T__G__Server.on("/lut/reset", HTTP_POST, func__Esp_HttpLutReset);
    ESP_WEB_SERVER_T__G__Server.on("/benchlog", HTTP_GET, func__Esp_HttpBenchLogGet);
    ESP_WEB_SERVER_T__G__Server.on("/benchlog/add", HTTP_POST, func__Esp_HttpBenchLogAdd);
    ESP_WEB_SERVER_T__G__Server.on("/benchlog/clear", HTTP_POST, func__Esp_HttpBenchLogClear);
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
